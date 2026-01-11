#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>//代表只要是 Windows 系統就包含這個頭檔
#else
#include <arpa/inet.h>//代表只要是 Linux 系統就包含這個頭檔
#endif
#include "minifs_ops.h"

#define DBP_COUNT 4
#define DENTRY_SIZE 64
#define DENTRY_COUNT 64

VCB vcb;
const char* FILE_NAME;
uint32_t working_dir_id;
uint8_t *bmap, *fmap;
char current_path[256];

static bool dir_is_empty(uint32_t dir_id){
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(dir_id, index);
        if (dentry.file_id == 0 && dentry.file_name[0] == '\0'){
            continue;
        }
        if (strcmp(dentry.file_name, ".") == 0 || strcmp(dentry.file_name, "..") == 0){
            continue;
        }
        // Debug: show unexpected non-empty entry
        printf("dir %d: non-empty at index %d -> id=%d name='%.*s' bytes=%02x%02x%02x%02x\n",
               dir_id, index, dentry.file_id, 16, dentry.file_name,
               (unsigned char)dentry.file_name[0], (unsigned char)dentry.file_name[1],
               (unsigned char)dentry.file_name[2], (unsigned char)dentry.file_name[3]);
        return false;
    }
    return true;
}

static void rm_recursive(uint32_t dir_id){
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(dir_id, index);
        if (dentry.file_id == 0 && dentry.file_name[0] == '\0'){
            continue;
        }
        if (strcmp(dentry.file_name, ".") == 0 || strcmp(dentry.file_name, "..") == 0){
            continue;
        }
        uint32_t sub_file_id = dentry.file_id;
        FCB sub_fcb = fs_read_fcb(sub_file_id);
        if (sub_fcb.is_dir){
            rm_recursive(sub_file_id);
        }
        // free block
        for (int i = 0; i < DBP_COUNT; ++i){
            bmap[sub_fcb.dbp[i]] = 0;
        }
        // free fmap
        fmap[sub_file_id] = 0;
        // delete dentry
        fs_delete_dentry(dir_id, index);
    }
    bmap_save();
    fmap_save();
}

void rm(uint32_t parent_id, uint32_t file_id, char* file_name, char* options){
    bool allow_dir = false , recursive = false;
    
    // Protection: Cannot delete root directory (ID 0)
    if (file_id == 0) {
        printf("Cannot remove root directory!\n");
        return;
    }
    
    // Protection: Cannot delete . or .. in root directory
    if (parent_id == 0 && (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)) {
        printf("Cannot remove '%s' from root directory!\n", file_name);
        return;
    }
    
    FCB fcb = fs_read_fcb(file_id);
    if (options && strchr(options, 'f'))
        allow_dir = true;
    if (options && strchr(options, 'r'))
        recursive = true;


    if (fcb.is_dir){
        if (!allow_dir){
            printf("%s is a directory. Use rm -f <dir_name> to remove an empty directory.\n", file_name);
            return;
        }
        if (!recursive && !dir_is_empty(file_id)){
            printf("%s is not empty; remove its contents first.\n", file_name);
            return;
        }
        if (recursive){
            rm_recursive(file_id);
        }
    }

    printf("Removing id=%d name=%s\n", file_id, file_name);
    uint32_t index = find_file(parent_id, file_name);
    fs_delete_dentry(parent_id, index);
    // free block
    for (int i = 0; i < DBP_COUNT; ++i){
        bmap[fcb.dbp[i]] = 0;
        bmap_save();
    }
    fmap[file_id] = 0;
    fmap_save();
}
uint32_t ls(uint32_t target_dir_id){

    printf("Contents of directory (ID: %d):\n", target_dir_id);
    printf("%-8s %-60s\n", "ID", "Name");
    printf("--------------------------------------------------------------------\n");
    
    uint32_t count = 0;
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(target_dir_id, index);
        
        // Skip empty entries
        if (dentry.file_id == 0 && dentry.file_name[0] == '\0'){
            continue;
        }
        
        // Print the entry (fs_read_dentry already converts to host byte order)
        printf("%-8d %-60s\n", dentry.file_id, dentry.file_name);
        count++;
    }
    
    printf("--------------------------------------------------------------------\n");
    printf("Total: %d entries\n", count);
    return count;
}

void mkdir(uint32_t parent_id, char* file_name){
    // check if file name (not include '\0') > 59 
    if (strlen(file_name)>59){
        printf("exceeds the max len of file name: 60!\n");
        return;
    }

    // check fcb remains
    uint32_t new_dir_id = fmap_new();
    fmap_save();
    if (!new_dir_id){
        printf("No more free fcb!\n");
        return;
    }

    // allocate block for directory data
    uint32_t block[4] = {0};
    for (int i = 0; i < 4; ++i){
        block[i] = bmap_new();
    }
    for (int i = 0; i < 4; ++i){
    if (!block[i]){
        printf("No more free blocks!\n");
        fmap[new_dir_id] = 0;
        fmap_save();
        return;
    }
    }

    for (int i = 0; i < 4; ++i){
        bmap[block[i]]  = 1;
    }
    bmap_save();
    
    // zero out the new block for directory entries
    /* BUG CODE
    for (int i = 0; i < 4; ++i){
        for (uint32_t j = 0; j < vcb.block_size; j++){
            fs_write_char(block[i], j, '\0');
        }
    }
    */
     

    // change is_dir in its fcb
    FCB fcb;
    memset(&fcb, 0, sizeof(FCB));
    fcb.is_dir = 1;
    for (int i = 0; i < 4; ++i)
        fcb.dbp[i] = block[i];
    fs_write_fcb(new_dir_id, &fcb);

    // add a dentry on current dir

    Dentry new_dentry; 
    memset(&new_dentry, 0, sizeof(Dentry));
    new_dentry.file_id   = new_dir_id;
    strncpy(new_dentry.file_name, file_name, 60);

    fs_write_dentry(parent_id, &new_dentry);

    // 添加 "." 和 ".." 到新目錄 
    Dentry itself;
    memset(&itself, 0, sizeof(Dentry));
    itself.file_id = new_dir_id;
    strncpy(itself.file_name, ".", 60);
    fs_write_dentry(new_dir_id, &itself);
    
    Dentry parent_dentry;
    memset(&parent_dentry, 0, sizeof(Dentry));
    parent_dentry.file_id = parent_id; // Use parent_id instead of working_dir_id
    strncpy(parent_dentry.file_name, "..", 60);
    fs_write_dentry(new_dir_id, &parent_dentry);
    
    
    return;
}

void touch(uint32_t parent_id, char* file_name){
    // check if file name (not include '\0') > 59 
    if (strlen(file_name)>59){
        printf("exceeds the max len of file name: 60!\n");
        return;
    }

    // check fcb remains
    uint32_t new_file_id = fmap_new();
    fmap_save();
    if (!new_file_id){
        printf("No more free fcb!\n");
        return;
    }
    // allocate block for file data
    uint32_t block[4] = {0};
    for (int i = 0; i < 4; ++i){
        block[i] = bmap_new();
    }
    for (int i = 0; i < 4; ++i){
    if (!block[i]){
        printf("No more free blocks!\n");
        fmap[new_file_id] = 0;
        fmap_save();
        return;
    }
    }
    for (int i = 0; i < 4; ++i){
        bmap[block[i]]  = 1;
    }

    bmap_save();
    // it's a file, not a directory
    FCB fcb = fs_read_fcb(new_file_id);
    fcb.is_dir = 0;
    fcb.file_size = 0;
    for (int i = 0; i < 4; ++i)
        fcb.dbp[i] = block[i];

    fs_write_fcb(new_file_id, &fcb);

    // add a dentry on current dir
    Dentry new_dentry; 
    memset(&new_dentry, 0, sizeof(Dentry));
    new_dentry.file_id   = new_file_id;
    strncpy(new_dentry.file_name, file_name, 60);

    fs_write_dentry(parent_id, &new_dentry);
    
    return;
}

void append_to_file(char* file_name, char* content)
{
    // Find the file
    uint32_t file_id = 0;
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(working_dir_id, index);
        if (strcmp(dentry.file_name, file_name) == 0){
            file_id = dentry.file_id;
            break;
        }
    }
    
    if (file_id == 0){
        printf("File %s not found!\n", file_name);
        return;
    }
    
    // Read FCB
    FCB fcb = fs_read_fcb(file_id);
    
    // Check if it's a directory
    if (fcb.is_dir){
        printf("%s is a directory!\n", file_name);
        return;
    }
    
    // Calculate content length (add newline)
    uint32_t content_len = strlen(content);
    //uint32_t new_size = fcb.file_size + content_len + 1; // +1 for newline
    
    // Write content to file
    uint32_t offset = fcb.file_size;
    for (uint32_t i = 0; i < content_len; i++){
        uint32_t block_index = offset / vcb.block_size;
        uint32_t block_offset = offset % vcb.block_size;
        
        if (block_index >= 4){
            printf("File size exceeds maximum!\n");
            return;
        }
        
        fs_write_char(fcb.dbp[block_index], block_offset, content[i]);
        offset++;
    }
    
    // Add newline
    uint32_t block_index = offset / vcb.block_size;
    uint32_t block_offset = offset % vcb.block_size;
    if (block_index < 4){
        fs_write_char(fcb.dbp[block_index], block_offset, '\n');
        offset++;
    }
    
    // Update FCB
    fcb.file_size = offset;
    fs_write_fcb(file_id, &fcb);
    
    printf("Appended to %s\n", file_name);
}

void overwrite_file(char* file_name, char* content)
{
    // Find the file
    uint32_t file_id = 0;
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(working_dir_id, index);
        if (strcmp(dentry.file_name, file_name) == 0){
            file_id = dentry.file_id;
            break;
        }
    }
    
    if (file_id == 0){
        printf("File %s not found!\n", file_name);
        return;
    }
    
    FCB fcb = fs_read_fcb(file_id);
    if (fcb.is_dir){
        printf("%s is a directory!\n", file_name);
        return;
    }
    
    // Write content to file starting at offset 0 (overwrite/truncate)
    uint32_t content_len = strlen(content);
    uint32_t offset = 0;
    for (uint32_t i = 0; i < content_len; i++){
        uint32_t block_index = offset / vcb.block_size;
        uint32_t block_offset = offset % vcb.block_size;
        
        if (block_index >= 4){
            printf("File size exceeds maximum!\n");
            return;
        }
        
        fs_write_char(fcb.dbp[block_index], block_offset, content[i]);
        offset++;
    }
    
    // Add newline
    uint32_t block_index = offset / vcb.block_size;
    uint32_t block_offset = offset % vcb.block_size;
    if (block_index < 4){
        fs_write_char(fcb.dbp[block_index], block_offset, '\n');
        offset++;
    }
    
    // Update FCB
    fcb.file_size = offset;
    fs_write_fcb(file_id, &fcb);
    
    printf("Wrote to %s\n", file_name);
}
void cat(uint32_t file_id){
    FCB fcb = fs_read_fcb(file_id);
    if (fcb.is_dir){
        printf("Is a directory!\n");
        return;
    }

    if (fcb.file_size == 0){
        // empty file — nothing to print
        return;
    }

    uint32_t offset = 0;
    for (; offset < fcb.file_size; ++offset){
        uint32_t block_index = offset / vcb.block_size;
        uint32_t block_offset = offset % vcb.block_size;
        if (block_index >= 4){
            break;
        }
        uint8_t ch = fs_read_char(fcb.dbp[block_index], block_offset);
        putchar((char)ch);
    }

    // Ensure trailing newline for tidy output
    if (fcb.file_size > 0){
        uint32_t last_off = fcb.file_size - 1;
        uint32_t last_block = last_off / vcb.block_size;
        uint32_t last_block_offset = last_off % vcb.block_size;
        if (fs_read_char(fcb.dbp[last_block], last_block_offset) != '\n')
            putchar('\n');
    }
}

void cd(uint32_t dir_id){
    FCB fcb = fs_read_fcb(dir_id);
    if (!fcb.is_dir){
        printf("Is not a directory!\n");
        return;
    }
    
    // Change to the new directory
    working_dir_id = (uint32_t)dir_id;
    printf("Changed to directory %d\n", working_dir_id);
}

void resolve_path(char* path, uint32_t* parent_id, uint32_t* file_id, char* file_name){
    // Create a copy of the path to avoid modifying the original
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';
    
    char* path_tok[256] = {NULL};
    char* next_tok = strtok(path_copy, "/"); 
    uint32_t temp_parent_id =0, temp_file_id;
    int n_token = 0, vaild_count = 0;
    Dentry dentry;

    // 1. is absolute?
    if (path[0]=='/'){
        temp_file_id = 0;
    }
    else {
        temp_file_id = working_dir_id;
    }

    while (next_tok){
        path_tok[n_token++] = next_tok;
        next_tok = strtok(NULL, "/");
    }

    // 2. look forward
    while(vaild_count < n_token && get_file_dentry(temp_file_id, path_tok[vaild_count], &dentry)){
        temp_parent_id = temp_file_id;
        temp_file_id = dentry.file_id;
        vaild_count++;
    }

    // Three condition:
    // 1. Get parent, Get file (count = n_token)
    //    -> Found complete path, return (parent, file)
    // 2. Get parent, No  file (count = n_token -1)
    //    -> File doesn't exist but parent found, return (parent, INVALID)
    // 3. No  parent, No  file (other)
    //    -> Invalid path, return (INVALID, INVALID)

    if (vaild_count == n_token){
        // Found complete path
        *parent_id = temp_parent_id;
        *file_id = temp_file_id;
        strncpy(file_name , path_tok[n_token-1], 60); //last tok
    }
    else if (vaild_count == n_token -1){
        // File doesn't exist, but parent directory is valid
        *parent_id = temp_file_id; // Use current directory as parent, not temp_parent_id
        *file_id = INVALID_ID;
        strncpy(file_name , path_tok[n_token-1], 60);
    }
    else {
        // Invalid path
        *parent_id =INVALID_ID; 
        *file_id = INVALID_ID;
    }

}

void update_current_path(){
    char path_tok[256][60];
    memset(path_tok, 0, sizeof(path_tok));
    int cnt = 0;
    Dentry parent_dentry;
    uint32_t dir_id = working_dir_id;
    uint32_t parent_dir_id;
    
    // 防止最大深度無限循環
    int max_depth = 100;
    while (max_depth-- > 0){
        if (!get_file_dentry(dir_id,"..", &parent_dentry)) break;
        parent_dir_id = parent_dentry.file_id;
        
        // 如果父級與當前（根案例）相同，則中斷
        if (parent_dir_id == dir_id) break;
        
        find_file_name_by_id(parent_dir_id, dir_id, path_tok[cnt++]);
        dir_id = parent_dir_id;
        
        // Additional safety check
        if (cnt >= 256) break;
    } 
    memset(current_path, 0, sizeof(current_path));

    if (cnt==0){ //in root
        strcat(current_path, "/");
    }
    else {
        for (int i = cnt - 1; i >= 0; i--){
            strcat(current_path, "/");
            strcat(current_path, path_tok[i]);
        }
    }
}

void pwd(){
    update_current_path();
    printf("%s\n", current_path);
}

void stat(uint32_t file_id){
    // 1. 呼叫你現有的函式讀取 FCB
	FCB fcb = fs_read_fcb(file_id);

	printf("======= FCB Structure Info (ID: %u) =======\n", file_id);
	
	// 2. 印出類型 (Directory 或 File)
	printf("Type:       %s\n", fcb.is_dir ? "Directory" : "File");
	
	// 3. 印出檔案大小
	printf("File Size:  %u bytes\n", fcb.file_size);
	
	// 4. 展開印出 Data Block Pointers (DBP)
	printf("Data Blocks:\n");
	for (int i = 0; i < 4; i++) {
		printf("  [DBP %d]:   %u\n", i, fcb.dbp[i]);
	}
	
	printf("===========================================\n");
}

int main(int argc, char* argv[]){
    char input_line[256];
    char *command;
    char *options;
    char *parameter;
    char path[256];

    // check input
    if (argc!=2){
        fprintf(stderr, "Usage: %s <FILE_NAME>.\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE_NAME = argv[1]; 
    
    // Check if file exists and can be opened
    FILE *test_fp = fopen(FILE_NAME, "r+b");
    if (!test_fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", FILE_NAME);
        return EXIT_FAILURE;
    }
    fclose(test_fp);
    
    // Load VCB first
    vcb_load();
    
    // Allocate memory for bitmaps with error checking
    bmap = (uint8_t *)calloc(vcb.block_count, sizeof(uint8_t));
    if (!bmap) {
        fprintf(stderr, "Error: Failed to allocate memory for bmap\n");
        return EXIT_FAILURE;
    }
    
    fmap = (uint8_t *)calloc(vcb.fcb_count, sizeof(uint8_t));
    if (!fmap) {
        fprintf(stderr, "Error: Failed to allocate memory for fmap\n");
        free(bmap);
        return EXIT_FAILURE;
    }
    
    // Load bitmaps
    bmap_load();
    fmap_load();
    
    // Initialize working directory to root
    working_dir_id = 0;


    while (1) {
        printf("minishell > ");
        
        // Read a line of input from the user (up to 255 characters)
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            // Handle EOF (e.g., Ctrl+D)
            break;
        }
        input_line[strcspn(input_line, "\n")] = 0;
        if (input_line[0]=='\0')
            continue;
        /*
        If is append command
         
        // Check for append operation (>>)
        char *append_op = strstr(input_line, ">>");
        if (append_op != NULL) //------------------------append----------------------
        {
            // 解析： echo "content" >> 文件名或內容 >> 文件名
            *append_op = '\0';  // Split at >>
            char *content_part = input_line;
            char *filename = append_op + 2;  // Skip >>
            
            // 修剪內容部分的前導/尾隨空格
            while (*content_part == ' ') content_part++;
            char *content_end = content_part + strlen(content_part) - 1;
            while (content_end > content_part && *content_end == ' ') {
                *content_end = '\0';
                content_end--;
            }
            
            // 修剪文件名部分的前導/尾隨空格
            while (*filename == ' ') filename++;
            char *filename_end = filename + strlen(filename) - 1;
            while (filename_end > filename && *filename_end == ' ') {
                *filename_end = '\0';
                filename_end--;
            }
            
            // 檢查是否以 "echo" 開頭
            char *content = content_part;
            if (strncmp(content_part, "echo", 4) == 0) {
                content = content_part + 4;
                // Skip spaces after echo
                while (*content == ' ') content++;
                
                // Remove quotes if present
                if (*content == '"') {
                    content++;
                    char *end_quote = strrchr(content, '"');
                    if (end_quote != NULL) {
                        *end_quote = '\0';
                    }
                }
            }
            
            if (strlen(content) == 0 || strlen(filename) == 0) {
                printf("usage: echo \"<content>\" >> <file_name> or <content> >> <file_name>\n");
                continue;
            }
            
            append_to_file(filename, content);
            continue;
        }

        // Check for overwrite operation (single '>')
        char *redir_op = strstr(input_line, ">");
        if (redir_op != NULL && !(redir_op[1] == '>')) //---------------------overwrite-------------------
        {
            *redir_op = '\0';  // Split at >
            char *content_part = input_line;
            char *filename = redir_op + 1;  // Skip >
            
            // 修剪內容部分的前導/尾隨空格
            while (*content_part == ' ') content_part++;
            char *content_end = content_part + strlen(content_part) - 1;
            while (content_end > content_part && *content_end == ' ') {
                *content_end = '\0';
                content_end--;
            }
            
            // 修剪文件名部分的前導/尾隨空格
            while (*filename == ' ') filename++;
            char *filename_end = filename + strlen(filename) - 1;
            while (filename_end > filename && *filename_end == ' ') {
                *filename_end = '\0';
                filename_end--;
            }
            
            // 檢查是否以 "echo" 開頭
            char *content = content_part;
            if (strncmp(content_part, "echo", 4) == 0) {
                content = content_part + 4;
                // Skip spaces after echo
                while (*content == ' ') content++;
                
                // Remove quotes if present
                if (*content == '"') {
                    content++;
                    char *end_quote = strrchr(content, '"');
                    if (end_quote != NULL) {
                        *end_quote = '\0';
                    }
                }
            }
            
            if (strlen(filename) == 0) {
                printf("usage: echo \"<content>\" > <file_name> or <content> > <file_name>\n");
                continue;
            }
            
            // Create file if not exists
            if (!find_file(parent_id, filename)){
                touch(parent_id, filename);
            }
            
            overwrite_file(filename, content);
            continue;
        }
        */

        /*
        If is a nomral command
        */ 
        command = NULL;
        options = NULL;
        parameter = NULL;
        memset(path, 0, sizeof(path));

        char* next_tok = NULL;
        // 1. Get command
        command = strtok(input_line, " ");
        // 2. Get options array and path
        // continue split previous string
        next_tok = strtok(NULL, " "); 
        

        if (next_tok){
            // if with options
            if (next_tok[0] == '-'){
                options = next_tok;
                parameter = strtok(NULL, " "); 
            }
            // if no options
            else{
                parameter = next_tok;
            }
        }

        uint32_t parent_id, file_id;
        char file_name[60];
        if (parameter)
            strncpy(path, parameter, 256);
        if (path[0] != '\0')
            resolve_path(path, &parent_id, &file_id, file_name);

        /* 
           * illegle path
           pid = INVALID, fid = INVALID

           Three Type of Commands:

           Type 1. The file/dir must exist 
              (ls, cat, cd, rm, stat)
           pid = n, fid = n

           Type 2. Create file/dir on parent path (mkdir, touch)
              (mkdir, touch)
           pid = n, fid = INVALID

           Type 3. No options, No parameters
               (pwd) 

        */

        if (parameter && parent_id == INVALID_ID && file_id == INVALID_ID){
            printf("No such path!\n");
            continue;
        }
        // Type 1
        if (strcmp("ls", command) == 0){
            // 1. Command check
            if (options){
                printf("No options for this command\n");
                continue;
            }
            else if (parameter && file_id==INVALID_ID){
                printf("No such file\n");
                continue;
            }
            else if (!parameter){
                ls(working_dir_id);
                continue;
            }
            ls(file_id);
        }

        else if (strcmp("cat", command) == 0){
            // 1. Command check
            if (parameter== NULL){
                printf("usage: cat <file_name>\n");
                continue;
            } 
            else if (options){
                printf("No options for this command\n");
                continue;
            }
            else if (file_id==INVALID_ID){
                printf("No such file\n");
                continue;
            }
            cat(file_id);
        }

        else if (strcmp("cd", command) == 0){
            // 1. Command check
            if (parameter== NULL){
                printf("usage: cd <file_name>\n");
                continue;
            } 
            else if (options){
                printf("No options for this command\n");
                continue;
            }
            else if (file_id==INVALID_ID){
                printf("No such file\n");
                continue;
            }
            cd(file_id);
        }
        
        else if (strcmp("stat", command) == 0){
            // 1. Command check
            if (parameter== NULL){
                printf("usage: stat <file_name>\n");
                continue;
            } 
            else if (options){
                printf("No options for this command\n");
                continue;
            }
            else if (file_id==INVALID_ID){
                printf("No such file\n");
                continue;
            }
            stat(file_id);
        }     

        else if (strcmp("rm", command) == 0){
            // 1. Command check
            if (parameter== NULL){
                printf("usage: rm <file_name>\n");
                continue;
            } 
            else if (file_id==INVALID_ID){
                printf("No such file\n");
                continue;
            }
            rm(parent_id, file_id, file_name, options);
        }
        
        // Type 2
        else if (strcmp("mkdir", command) == 0){
            // 1. Command check
            if (parameter== NULL){
                printf("usage: mkdir <file_name>\n");
                continue;
            } 
            else if (options){
                printf("No options for this command\n");
                continue;
            }
            else if (file_id!=INVALID_ID){
                printf("File exists!\n");
                continue;
            }
            mkdir(parent_id, file_name);
        }

        else if (strcmp("touch", command) == 0){
            // 1. Command check
            if (parameter== NULL){
                printf("usage: touch <file_name>\n");
                continue;
            } 
            else if (options){
                printf("No options for this command\n");
                continue;
            }
            else if (file_id!=INVALID_ID){
                printf("File exists!\n");
                continue;
            }
            touch(parent_id, file_name);
        }
        // Type 3
        else if (strcmp("exit", command) == 0){
            break;
        }
        else if (strcmp("pwd", command) == 0){
            if (parameter){
                printf("No parameter for this command\n");
                continue;
            } 
            else if (options){
                printf("No options for this command\n");
                continue;
            }
            pwd();
        }
    }
    free(bmap);
    free(fmap);
}
