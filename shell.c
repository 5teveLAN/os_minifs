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
    int offset = 0;
    while (offset < block_size) {
        uint32_t existing_inode = fsread(10, offset);
        
        // if inode == 0 then no file
        if (existing_inode == 0) {
            break;
        }
        
        // 讀取現有文件名
        char existing_name[64] = {0};
        int name_match = 1;
        for (int i = 0; i < 63; i++) {
            existing_name[i] = (char)fsreadc(10, offset + 4 + i);
            if (existing_name[i] == '\0') {
                break;
            }
        }
        
        // 比較文件名
        int i = 0;
        while (file_name[i] != '\0' || existing_name[i] != '\0') {
                // printf("check inode %d:existing:%c, creating:%c\n",existing_inode, existing_name[i], file_name[i]);
            if (file_name[i] != existing_name[i]) {
                name_match = 0;
                break;
            }
            i++;
        }
        
        // If both names ended and matched
        if (name_match) {
            printf("File '%s' already exists (inode: %u)\n", file_name, existing_inode);
            return existing_inode;
        }
        
        offset += 64;
    }

    uint32_t inode_id = mkFCB();

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
    }
}
