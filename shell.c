#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "minifs_ops.h"

VCB vcb;
const char* FILE_NAME;
uint32_t current_dir_id;
uint8_t *bmap, *fmap;

void rm(char* file_name){
    // check if file name (not include '\0') > 59 
    if (strlen(file_name)>59){
        printf("exceeds the max len of file name: 60!\n");
        return;
    }

    uint32_t index = find_file(current_dir_id,file_name);
    Dentry dentry = fs_read_dentry(current_dir_id, index);
    fs_delete_dentry(current_dir_id, index);
    fmap[dentry.file_id] = 0;
    fmap_save();

}
uint32_t ls(){
    printf("Contents of directory (ID: %d):\n", current_dir_id);
    printf("%-8s %-60s\n", "ID", "Name");
    printf("--------------------------------------------------------------------\n");
    
    uint32_t count = 0;
    for (uint32_t index = 0; index < 64; ++index){
        Dentry dentry = fs_read_dentry(current_dir_id, index);
        
        // Skip empty entries
        if (dentry.file_id == 0 && dentry.file_name[0] == '\0'){
            continue;
        }
        
        // Convert from network byte order to host byte order
        uint32_t file_id = ntohl(dentry.file_id);
        
        // Print the entry
        printf("%-8d %-60s\n", file_id, dentry.file_name);
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

    // change is_dir in its fcb
    FCB fcb = fs_read_fcb(new_dir_id);
    fcb.is_dir = 1;
    fs_write_fcb(new_dir_id, &fcb);

    printf("Create new fcb: %d\n", new_dir_id);
    printf("dbp[0] at block %d\n", fcb.dbp[0]);

    // add a dentry on current dir

    Dentry new_dentry; 
    new_dentry.file_id   = new_dir_id;
    strncpy(new_dentry.file_name, file_name, 60);

    fs_write_dentry(current_dir_id, &new_dentry);

    // add "." and ".." to the new dir 
    Dentry itself;
    itself.file_id = htonl(new_dir_id);
    strncpy(itself.file_name, ".", 60);
    fs_write_dentry(new_dir_id, &itself);
    
    Dentry parent_dentry;
    parent_dentry.file_id = htonl(current_dir_id);
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

    // it's a file, not a directory
    FCB fcb = fs_read_fcb(new_file_id);
    fcb.is_dir = 0;
    fcb.file_size = 0;
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
    for (uint32_t index = 0; index < 64; ++index){
        Dentry dentry = fs_read_dentry(current_dir_id, index);
        if (strcmp(dentry.file_name, file_name) == 0){
            file_id = ntohl(dentry.file_id);
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

void cd(char* dir_name){
    // 查找目錄（-1表示未找到）
    int32_t dir_id = -1;
    size_t name_len = strlen(dir_name);
    
    for (uint32_t index = 0; index < 64; ++index){
        Dentry dentry = fs_read_dentry(current_dir_id, index);
        
        // Skip empty entries
        if (dentry.file_id == 0 && dentry.file_name[0] == '\0'){
            continue;
        }
        
        // 比較目錄名稱 - 精確匹配長度
        if (memcmp(dentry.file_name, dir_name, name_len) == 0 &&
            (dentry.file_name[name_len] == '\0' || name_len >= 59)){
            dir_id = (int32_t)ntohl(dentry.file_id);
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
    char *argument;
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
        
        command = strtok(input_line, " ");
        // continue split previous string
        argument = strtok(NULL, " "); 

        if (strcmp("mkdir", command) == 0){
            if (argument== NULL){
                printf("usage: mkdir <file_name>\n");
                continue;
            } 
            mkdir(argument);
        }
        else if (strcmp("touch", command) == 0){
            if (argument== NULL){
                printf("usage: touch <file_name>\n");
                continue;
            } 
            touch(argument);
        }
        else if (strcmp("ls", command) == 0){
            ls();
        }
        else if (strcmp("cd", command) == 0){
            if (argument == NULL){
                printf("usage: cd <directory_name>\n");
                continue;
            }
            cd(argument);
        }
        else if (strcmp("rm", command) == 0){
            if (argument == NULL){
                printf("usage: rm <file_name>\n");
                continue;
            }
            rm(argument);
        }
        
        else if (strcmp("exit", command) == 0){
            break;
        }
    }
    free(bmap);
    free(fmap);
}
