#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

extern char* FILE_NAME;
uint32_t fsread(uint32_t block, uint32_t offset){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t block_size = 0;
    uint32_t data = 0x00;
    if (block!=0)
        block_size = fsread(0, 4); 

    fseek(fp, block*block_size+offset, SEEK_SET);
    for (int boffset = 24; boffset >= 0; boffset-=8){
        data |= (uint32_t)fgetc(fp)<<boffset;//BUG IS HERE
    }
    fclose(fp);
    return data;
}
uint32_t fswrite(uint32_t block, uint32_t offset, uint32_t data){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t block_size = 0;
    if (block!=0)
        block_size = fsread(0, 4); 

    fseek(fp, block*block_size+offset, SEEK_SET);
    for (int boffset = 24; boffset >= 0; boffset-=8){
        fputc((data>>boffset) & 0xFF, fp);
    }
    fclose(fp);
}

uint32_t fswritec(uint32_t block, uint32_t offset, char data){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t block_size = 0;
    if (block!=0)
        block_size = fsread(0, 4); 

    fseek(fp, block*block_size+offset, SEEK_SET);
    fputc(data, fp);
    fclose(fp);
}
