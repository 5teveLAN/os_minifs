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
    uint32_t inode_id = mkFCB();
    char c = file_name[0];
    int offset = 0;
    for (;fsread(10,offset)!=0;offset+=64);
    fswrite(10,offset,inode_id);
    for (int i = 0; c!='\0'; ++i){
        c = file_name[i];
        fswritec(10,offset+4+i,c);
    }
}


int main(int argc, char* argv[]){
    char input_line[256];
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
        if (input_line[0]=='\0')
            continue;
    }
}
