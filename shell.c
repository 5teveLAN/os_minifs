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

    // add ".." and "." to the new dir 
    Dentry itself = {new_dir_id, "."}; 
    fs_write_dentry(new_dir_id, &itself);
    
    Dentry parent_dentry = {current_dir_id, ".."}; 
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
        else if (strcmp("exit", command) == 0){
            break;
        }
    }
    free(bmap);
    free(fmap);
}
