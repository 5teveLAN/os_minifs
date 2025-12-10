#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "minifs_ops.h"

const int FCB_SIZE;

int block_count;
int block_size;
int bmap_addr;
int fcb_start_addr;
int fcb_max_count;
int fcb_current_number;
char* FILE_NAME;
bool bitmap[100];
uint32_t touch(char* file_name){

    // 檢查目錄中是否已存在文件
    uint32_t existing_inode = filefind(file_name);
    if (existing_inode){
        printf("File '%s' already exists (inode: %u)\n", file_name, existing_inode);

        return EXIT_FAILURE;
    }

    uint32_t inode_id = mkFCB();
    uint32_t offset = (inode_id-1) * 64;

    fswrite(10, offset, inode_id);
    for (int i = 0; file_name[i] != '\0' && i < 63; ++i) {
        fswritec(10, offset + 4 + i, file_name[i]);
    }
    
    printf("File '%s' created (inode: %u)\n", file_name, inode_id);
    return inode_id;
}


int main(int argc, char* argv[]){
    char input_line[256];
    char *command;
    char *argument;
    if (argc!=2){
        fprintf(stderr, "Usage: %s <FILE_NAME>.", argv[0]);
        return EXIT_FAILURE;
    }
    FILE_NAME = argv[1]; 
    loadVCB();
    loadBitmap();


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
        command = strtok(input_line, " ");
        // continue split previous string
        argument = strtok(NULL, " "); 

        if (strcmp("touch", command) == 0){
            if (argument== NULL){
                printf("usage: touch <file_name>\n");
                continue;
            } 
            touch(argument);
        }
        else if (strcmp("exit", command) == 0){
            break;
        }
        else if (strcmp("exit", command) == 0){
            break;
        }
    }
}
