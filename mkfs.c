#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "minifs_ops.h"
#define DBP_COUNT 4

const char* FILE_NAME;
uint32_t FILE_SIZE;
uint8_t *bmap, *fmap;
extern VCB vcb;

// just write '\0' at total_bytes to the file
int mk_img(){
    FILE *fp = fopen(FILE_NAME, "w");

    if (fp == NULL){
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    //move pointer to last byte
    if (fseek(fp, FILE_SIZE-1, SEEK_SET)!=0){ 
        perror("Error seeking to file end");
        fclose(fp);
        return EXIT_FAILURE;
    }
    //put a char (an ASCII char is 1 byte)
    if (fputc('\0', fp)==EOF){
        perror("Error writing to file end");
        fclose(fp);
        return EXIT_FAILURE;
    }

    if (fclose(fp)!=0){
        perror("Error closing file ");
        return EXIT_FAILURE;
    }

    printf("Successfully created %d KB disk image.\n", FILE_SIZE/1024);

    return EXIT_SUCCESS;
}
// load vcb to memory and write to disk
void mk_vcb() {
    vcb.block_size       = 1024;
    vcb.block_count      = 100;
    vcb.fcb_size         = 32;
    vcb.bmap_start_block = 1;
    vcb.bmap_end_block   = 1;
    vcb.fmap_start_block = 2;
    vcb.fmap_end_block   = 2;
    vcb.fcb_start_block  = 3;
    vcb.fcb_end_block    = 9;
    vcb.fcb_count        = (vcb.fcb_end_block-vcb.fcb_start_block+1)
                           * (vcb.block_size/vcb.fcb_size);
    vcb_save();
}
// allocate memory to bmap and fmap
void mk_bmap(){
    bmap = (uint8_t *)calloc(vcb.block_count, sizeof(uint8_t));
    for (uint32_t i = 0; i < vcb.block_count; ++i)
        bmap[i] = 1;
    bmap_save();
}
void mk_fmap(){
    fmap = (uint8_t *)calloc(vcb.fcb_count, sizeof(uint8_t));
    for (uint32_t i = 1; i < vcb.fcb_count; ++i)
        fmap[i] = 0;
    fmap_save();
}
// create all fcb to from fcb_start_block to fcb_end_block 
void mk_fcb(){
    uint32_t offset;
    FCB fcb;

    for (int i = 0; i < vcb.fcb_count; ++i){
        fcb.is_dir = 0;
        fcb.file_size = 0;
        for (int j = 0; j < DBP_COUNT; ++j)
                        // data_start_block+0/1/2/3  + i *4
            fcb.dbp[j] = (vcb.fcb_end_block+1) + j  + i*DBP_COUNT;
        fs_write_fcb(i, &fcb);
    }

  
}

void mk_root(){
    fmap[0] = 1; //root dir
    FCB root_fcb = fs_read_fcb(0);
    root_fcb.is_dir = 1;
    fs_write_fcb(0, &root_fcb);
    Dentry itself = {0, "."}; //root fcb id = 0! name =  "."
    fs_write_dentry(0, &itself); // equals make a "." directory on root!

}

void mkfs(){
    mk_img();
    mk_vcb();
    mk_bmap();
    mk_fmap();
    mk_fcb();
    mk_root();
    //debug_dump();
}
int main(int argc, char* argv[]){
    if (argc!=3){
        fprintf(stderr, "Usage: %s <FILE_NAME> <FILE_SIZE>.", argv[0]);
        return EXIT_FAILURE;
    }
    FILE_NAME = argv[1]; 
    FILE_SIZE = atoi(argv[2]);
    if (FILE_SIZE<0){
        fprintf(stderr, "file size must be positive.");
        return EXIT_FAILURE;
    }

    mkfs();
    free(bmap);
    free(fmap);
    

}
