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

#define DENTRY_COUNT 16  // 1024 bytes / 64 bytes per dentry

VCB vcb;
const char* FILE_NAME;
uint32_t current_dir_id;
uint8_t *bmap, *fmap;

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
        if (sub_fcb.dbp[0]){
            bmap[sub_fcb.dbp[0]] = 0;
        }
        // free fmap
        fmap[sub_file_id] = 0;
        // delete dentry
        fs_delete_dentry(dir_id, index);
    }
    bmap_save();
    fmap_save();
}

void rm(const char* file_name, char* options){
    bool allow_dir = false , recursive = false;
    // check if file name (not include '\0') > 59 
    if (strlen(file_name) > 59){
        printf("exceeds the max len of file name: 60!\n");
        return;
    }

    if (options && strchr(options, 'f'))
        allow_dir = true;
    if (options && strchr(options, 'r'))
        recursive = true;

    uint32_t index = find_file(current_dir_id, (char*)file_name);
    if (index == 0){
        printf("%s not found!\n", file_name);
        return;
    }

    Dentry dentry = fs_read_dentry(current_dir_id, index);
    uint32_t file_id = dentry.file_id;
    FCB fcb = fs_read_fcb(file_id);

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
    fs_delete_dentry(current_dir_id, index);
    // free block
    if (fcb.dbp[0]){
        bmap[fcb.dbp[0]] = 0;
        bmap_save();
    }
    fmap[file_id] = 0;
    fmap_save();
}
uint32_t ls(char* file_name){
    uint32_t target_dir_id;

    if (file_name){
        if (!find_file(current_dir_id , file_name)){
            printf("No such file: %s\n", file_name);
            return 0;
        }
        uint32_t index = find_file(current_dir_id,file_name);
        Dentry dentry = fs_read_dentry(current_dir_id, index);
        uint32_t dir_id = (int32_t)ntohl(dentry.file_id);
        FCB target_fcb = fs_read_fcb(dir_id);
        if (!target_fcb.is_dir){
            printf("%s is not directory!\n", file_name);
            return 0;
        }
        target_dir_id = dir_id;
    }
    else{
        target_dir_id = current_dir_id;
    }

    printf("Contents of directory (ID: %d):\n", target_dir_id);
    printf("%-8s %-60s\n", "ID", "Name");
    printf("--------------------------------------------------------------------\n");
    
    uint32_t count = 0;
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(current_dir_id, index);
        
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

void mkdir(char* file_name){
    // check if file name (not include '\0') > 59 
    if (strlen(file_name)>59){
        printf("exceeds the max len of file name: 60!\n");
        return;
    }
    // check if name exists
    if (find_file(current_dir_id, file_name)){
        printf("file %s already exists!\n", file_name);
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
    uint32_t block = bmap_new();
    bmap_save();
    if (!block){
        printf("No more free blocks!\n");
        fmap[new_dir_id] = 0;
        fmap_save();
        return;
    }

    // zero out the new block for directory entries
    for (uint32_t i = 0; i < vcb.block_size; i++){
        fs_write_char(block, i, '\0');
    }

    // change is_dir in its fcb
    FCB fcb;
    memset(&fcb, 0, sizeof(FCB));
    fcb.is_dir = 1;
    fcb.dbp[0] = block;
    fs_write_fcb(new_dir_id, &fcb);

    printf("Create new fcb: %d\n", new_dir_id);
    printf("dbp[0] at block %d\n", fcb.dbp[0]);

    // add a dentry on current dir

    Dentry new_dentry; 
    new_dentry.file_id   = new_dir_id;
    strncpy(new_dentry.file_name, file_name, 60);

    fs_write_dentry(current_dir_id, &new_dentry);

    // 添加 "." 和 ".." 到新目錄 
    Dentry itself;
    itself.file_id = new_dir_id;
    strncpy(itself.file_name, ".", 60);
    fs_write_dentry(new_dir_id, &itself);
    
    Dentry parent_dentry;
    parent_dentry.file_id = current_dir_id;
    strncpy(parent_dentry.file_name, "..", 60);
    fs_write_dentry(new_dir_id, &parent_dentry);
    
    return;
}

void touch(char* file_name){
    // check if file name (not include '\0') > 59 
    if (strlen(file_name)>59){
        printf("exceeds the max len of file name: 60!\n");
        return;
    }
    // check if name exists
    if (find_file(current_dir_id, file_name)){
        printf("file %s already exists!\n", file_name);
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
    uint32_t block = bmap_new();
    bmap_save();
    if (!block){
        printf("No more free blocks!\n");
        fmap[new_file_id] = 0;
        fmap_save();
        return;
    }

    // it's a file, not a directory
    FCB fcb = fs_read_fcb(new_file_id);
    fcb.is_dir = 0;
    fcb.file_size = 0;
    fcb.dbp[0] = block;
    fs_write_fcb(new_file_id, &fcb);

    printf("Create new fcb: %d\n", new_file_id);

    // add a dentry on current dir
    Dentry new_dentry; 
    new_dentry.file_id   = new_file_id;
    strncpy(new_dentry.file_name, file_name, 60);

    fs_write_dentry(current_dir_id, &new_dentry);
    
    return;
}

void append_to_file(char* file_name, char* content)
{
    // Find the file
    uint32_t file_id = 0;
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(current_dir_id, index);
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
    uint32_t new_size = fcb.file_size + content_len + 1; // +1 for newline
    
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
        Dentry dentry = fs_read_dentry(current_dir_id, index);
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
void cat(char* file_name){
    // Find the file
    uint32_t file_id = 0;
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(current_dir_id, index);
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

void cd(char* dir_name){
    // 查找目錄（-1表示未找到）
    int32_t dir_id = -1;
    size_t name_len = strlen(dir_name);
    
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry dentry = fs_read_dentry(current_dir_id, index);
        
        // Skip empty entries
        if (dentry.file_id == 0 && dentry.file_name[0] == '\0'){
            continue;
        }
        
        // 比較目錄名稱 - 精確匹配長度
        if (memcmp(dentry.file_name, dir_name, name_len) == 0 &&
            (dentry.file_name[name_len] == '\0' || name_len >= 59)){
            dir_id = (int32_t)dentry.file_id;
            break;
        }
    }
    
    if (dir_id < 0){
        printf("%s: No such directory!\n", dir_name);
        return;
    }
    
    // Check if it's a directory
    FCB fcb = fs_read_fcb((uint32_t)dir_id);
    if (!fcb.is_dir){
        printf("%s is not a directory!\n", dir_name);
        return;
    }
    
    // Change to the new directory
    current_dir_id = (uint32_t)dir_id;
    printf("Changed to directory %d\n", current_dir_id);
}

int main(int argc, char* argv[]){
    char input_line[256];
    char *command;
    char *options;
    char *parameter;

    // check input
    if (argc!=2){
        fprintf(stderr, "Usage: %s <FILE_NAME>.", argv[0]);
        return EXIT_FAILURE;
    }

    FILE_NAME = argv[1]; 
    vcb_load();
    bmap = (uint8_t *)calloc(vcb.block_count, sizeof(uint8_t));
    fmap = (uint8_t *)calloc(vcb.fcb_count, sizeof(uint8_t));
    bmap_load();
    fmap_load();


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
        */ 
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
            if (!find_file(current_dir_id, filename)){
                touch(filename);
            }
            
            overwrite_file(filename, content);
            continue;
        }

        /*
        If is a nomral command
        */ 
        command = NULL;
        options = NULL;
        parameter = NULL;

        // 1. Get command
        command = strtok(input_line, " ");
        // 2. Get options array and parameter
        // continue split previous string
        char* next_tok = strtok(NULL, " "); 
        
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
        
       // printf("command: %s\n", command ? command : "Nan");
       // printf("options: %s\n", options ? options : "Nan");
       // printf("parameter: %s\n", parameter ? parameter : "Nan" );
       // continue;

        if (strcmp("mkdir", command) == 0){
            if (parameter== NULL){
                printf("usage: mkdir <file_name>\n");
                continue;
            } 
            mkdir(parameter);
        }

        else if (strcmp("touch", command) == 0){
            if (parameter== NULL){
                printf("usage: touch <file_name>\n");
                continue;
            } 
            touch(parameter);
        }
        
        else if (strcmp("ls", command) == 0){
            ls(parameter);
        }

        else if (strcmp("cat", command) == 0){
            if (parameter == NULL){
                printf("usage: cat <file_name>\n");
                continue;
            }
            cat(parameter);
        }

        else if (strcmp("cd", command) == 0){
            if (parameter == NULL){
                printf("usage: cd <directory_name>\n");
                continue;
            }
            cd(parameter);
        }
        
        else if (strcmp("rm", command) == 0){
            if (parameter == NULL){
                printf("usage: rm <file_name>\n");
                continue;
            }
            rm(parameter, options);
        }
        
        else if (strcmp("exit", command) == 0){
            break;
        }
    }
    free(bmap);
    free(fmap);
}
