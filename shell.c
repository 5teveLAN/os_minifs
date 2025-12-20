#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "minifs_ops.h"

int FCB_SIZE;
int BLOCK_COUNT;
int BLOCK_SIZE;
int BMAP_ADDR;
int FCB_START_ADDR;
int FCB_MAX_COUNT;
int FCB_CURRENT_NUMBER;
char* FILE_NAME;
bool bitmap[100];
uint32_t current_dir_id;

uint32_t ls(){

}

uint32_t mkdir(char* file_name){
    FCB fcb;

    Dentry current_dir;
    Dentry parent_dir;

    

    // find the target directory 


    // add a dentry on it
}

uint32_t touch(char* file_name){

    // 檢查目錄中是否已存在文件
    uint32_t existing_inode = filefind(file_name);
    if (existing_inode){
        printf("File '%s' already exists (inode: %u)\n", file_name, existing_inode);

        return EXIT_FAILURE;
    }

    uint32_t inode_id = mkFCB();
    uint32_t offset = (inode_id-1) * 64;

    fs_write_uint32(10, offset, inode_id); //actually is dbp1 of root directory
    for (int i = 0; file_name[i] != '\0' && i < 63; ++i) {
        fs_write_char(10, offset + 4 + i, file_name[i]);
    }
    
    printf("File '%s' created (inode: %u)\n", file_name, inode_id);
    FCB_CURRENT_NUMBER = inode_id;
    fs_write_uint32(0,20,FCB_CURRENT_NUMBER);
}

uint32_t append(char* file_name, char* S){
    uint32_t inode = filefind(file_name);
    if (!inode){
        printf("Could not find the file: %s\n", file_name);
        return EXIT_FAILURE;
    }
    // We assume that dbps are continuous
    //read the block number of dbp1
    uint32_t dbp[4];
    for (int i = 0; i < 4; ++i)
        dbp[i] = fs_read_uint32(2, inode*32+8+i*4);

    printf("inode:%d\n", inode);
    printf("DBP1:%d\n", dbp[1]);
    printf("DBP2:%d\n", dbp[2]);
    printf("DBP3:%d\n", dbp[3]);
    printf("DBP4:%d\n", dbp[4]);

    uint32_t k = 0;
    // the char exceeds block size will not be appended
    for (int i = 0; i < 4; ++i){
        for (int j = 0; j < BLOCK_SIZE; ++j){
            char c = fs_read_char(dbp[i],j);
            if (c=='\0')
                fs_write_char(dbp[i],j, file_name[k++]);
            if (file_name[k]=='\0')
                break;
        }
    }

    return EXIT_SUCCESS;

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
        else if (strcmp("append", command) == 0){
            char* content = strtok(NULL, " "); 
            if (argument== NULL || content==NULL){
                printf("usage: touch <file_name> <string>\n");
                continue;
            } 
            append(argument, content);
        }
        else if (strcmp("fcb_number", command) == 0){
            printf("FCB_CURRENT_NUMBER:%d\n",FCB_CURRENT_NUMBER);
        }
    }
}
