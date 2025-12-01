#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "minifs_ops.h"

int FCB_SIZE = 32;
int block_count;
int block_size;
int bmap_addr;
int fcb_start_addr;
int fcb_max_count;
int fcb_current_number;
char* FILE_NAME;
bool bitmap[100];


int mkImg(int total_bytes){
    FILE *fp = fopen(FILE_NAME, "w");

    if (fp == NULL){
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    //move pointer to last byte
    if (fseek(fp, total_bytes-1, SEEK_SET)!=0){ 
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

    printf("Successfully created %d KB disk image.\n", total_bytes/1024);

    return EXIT_SUCCESS;
}
uint32_t mkVCB_RAM() {
    block_count=100;
    block_size=1024;
    bmap_addr=1;
    fcb_start_addr=2; //2-9, total 8 blocks
    fcb_max_count=8*block_size/FCB_SIZE;
    fcb_current_number=0;
    printf("VCB RAM initialized only.\n");
    return EXIT_SUCCESS;
}

uint32_t mkBitmap_RAM(){
    for (uint32_t i = 0; i < block_count; ++i)
        bitmap[i] = false;

    
    bitmap[0] = true;  // VCB block
    bitmap[1] = true;  // bitmap block

    printf("Bitmap RAM initialized only.\n");
    return EXIT_SUCCESS;
}
uint32_t mkVCB_Disk(){
    FILE *fp = fopen(FILE_NAME, "r+b");
    if (!fp){
        perror("Error opening disk image");
        return EXIT_FAILURE;
    }

    fseek(fp, 0, SEEK_SET); 
    printf("Writing VCB to disk at offset %ld\n", ftell(fp));

    uint32_t data[] = {
        block_count,
        block_size,
        bmap_addr,
        fcb_start_addr,
        fcb_max_count,
        fcb_current_number
    };

    for(int i = 0; i < 6; i++){
        for(int offset = 24; offset >= 0; offset -= 8){
            fputc((data[i] >> offset) & 0xFF, fp);
        }
    }

    long pos = ftell(fp);
    for(long i = pos; i < block_size; ++i)
        fputc('\0', fp);

    fclose(fp);
    printf("VCB written to disk.\n");
    return EXIT_SUCCESS;
}

uint32_t mkBitmap_Disk(){
    FILE *fp = fopen(FILE_NAME, "r+b");
    if (!fp){
        perror("Error opening disk image");
        return EXIT_FAILURE;
    }

    
    mkBitmap_RAM();

    
    fseek(fp, bmap_addr * block_size, SEEK_SET);
    printf("Writing Bitmap to disk at offset %ld\n", ftell(fp));

    for (uint32_t i = 0; i < block_count; i++)
        fputc(bitmap[i] ? 1 : 0, fp);

    long pos = ftell(fp);
    for (long i = pos; i < (bmap_addr + 1) * block_size; i++)
        fputc('\0', fp);

    fclose(fp);
    printf("Bitmap written to disk.\n");
    return EXIT_SUCCESS;
}

uint32_t mkdir(){

    fflush(NULL);   

    FILE *fp = fopen(FILE_NAME, "r+b");
    if (fp == NULL) {
        perror("Error opening disk image in mkdir");
        return EXIT_FAILURE;
    }

    long fcb_base = (long)fcb_start_addr * block_size;
    long fcb0_offset = fcb_base;  

    uint32_t file_size = 0;


    fseek(fp, fcb0_offset + 4, SEEK_SET);
    for (int offset = 24; offset >= 0; offset -= 8) {
        fputc((file_size >> offset) & 0xFF, fp);
    }


    fseek(fp, fcb0_offset + 8, SEEK_SET);
    uint32_t dbp1 = 0;
    for (int offset = 24; offset >= 0; offset -= 8) {
        int c = fgetc(fp);
        if (c == EOF) {
            perror("Error reading dbp1 in mkdir");
            fclose(fp);
            return EXIT_FAILURE;
        }
        dbp1 |= ((uint32_t)c) << offset;
    }

    if (dbp1 == 0) {
        fprintf(stderr, "mkdir: dbp1 of root is 0 (not allocated?)\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    printf("Root directory will use data block %u\n", dbp1);


    uint8_t *buf = calloc(block_size, 1);
    if (!buf) {
        perror("calloc");
        fclose(fp);
        return EXIT_FAILURE;
    }

    long data_offset = (long)dbp1 * block_size;
    fseek(fp, data_offset, SEEK_SET);
    fwrite(buf, 1, block_size, fp);

    free(buf);
    fclose(fp);

    printf("Root directory created (FCB0, block %u cleared).\n", dbp1);
    return EXIT_SUCCESS;
}

void mkfs(int file_size){
    mkImg(file_size);
    mkVCB_RAM();
    mkVCB_Disk();
    mkBitmap_Disk();
    //create FCBs from block2 to block9
    mkFCB();
    mkdir();
}
int main(int argc, char* argv[]){
    if (argc!=3){
        fprintf(stderr, "Usage: %s <FILE_NAME> <FILE_SIZE>.", argv[0]);
        return EXIT_FAILURE;
    }
    FILE_NAME = argv[1]; 
    int file_size = atoi(argv[2]);
    if (file_size<0){
        fprintf(stderr, "file size must be positive.");
        return EXIT_FAILURE;
    }
    mkfs(file_size);
    printf("Block size: %d\n", fsread(0,4));

    

}
