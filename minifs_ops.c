#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

extern int block_count;
extern int FCB_SIZE;
extern int block_size;
extern int bmap_addr;
extern int fcb_start_addr;
extern int fcb_max_count;
extern int fcb_current_number;
extern char* FILE_NAME;
extern bool bitmap[100];

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
uint32_t fsreadc(uint32_t block, uint32_t offset){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t block_size = 0;
    uint32_t data = 0x00;
    if (block!=0)
        block_size = fsread(0, 4); 

    fseek(fp, block*block_size+offset, SEEK_SET);
    data = fgetc(fp);
    fclose(fp);
    return data;
}
void fswrite(uint32_t block, uint32_t offset, uint32_t data){
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

void fswritec(uint32_t block, uint32_t offset, char data){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t block_size = 0;
    if (block!=0)
        block_size = fsread(0, 4); 

    fseek(fp, block*block_size+offset, SEEK_SET);
    fputc(data, fp);
    fclose(fp);
}
void loadVCB(){
    block_count = fsread(0,0);
    block_size = fsread(0,4);
    bmap_addr = fsread(0,8);
    fcb_start_addr = fsread(0,12);
    fcb_max_count = fsread(0,16);
    fcb_current_number = fsread(0,20);
}
void loadBitmap(){
    for (uint32_t i = 0; i < block_count; ++i){
        bitmap[i] = fsreadc(1,i);
    }
}
uint32_t mkFCB(){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t fcb_current_number = fsread(0,20);

    // 1. write id_number
    // move to new FCB
    fseek(fp, 2048+fcb_current_number*FCB_SIZE, SEEK_SET);
    printf("Create FCB%d at %ld\n",fcb_current_number, ftell(fp));
    //from MSB to LSB
    for (int offset = 24; offset >= 0; offset-=8){

        fputc((fcb_current_number>>offset) & 0xFF, fp);
    }


    // 2. file_size
    // keep it zero, skip by 4 bytes
    fseek(fp, 4, SEEK_CUR);

    // 3. dbp_1,2,3,4
    // search for free dbp
    uint32_t dbp = 0;
    for (int i = 10; i < block_count; ++i){
        if (!bitmap[i]){
            dbp=i;
            break;
        }
    }

    if (!dbp){
        perror("no free blocks");
        return EXIT_FAILURE;
    }
    // write dbp_1,2,3,4
    for (uint32_t i = 0; i < 4;++i, dbp++){
        printf("Create DBP%d at %ld, it points to block %d\n",i, ftell(fp),dbp);
        //from MSB to LSB
        for (int offset = 24; offset >= 0; offset-=8){
            fputc((dbp>>offset) & 0xFF, fp);
        }
        bitmap[dbp]=true;
    } 
    fswrite(0,20,fcb_current_number+1);
    return fcb_current_number;
  
}

uint32_t filefind(char *file_name){
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
                //printf("check inode %d:existing:%c, creating:%c\n",existing_inode, existing_name[i], file_name[i]);
            if (file_name[i] != existing_name[i]) {
                name_match = 0;
                break;
            }
            i++;
        }
        
        // If both names ended and matched
        if (name_match) {
            return existing_inode;
        }
        
        offset += 64;
    }
    return 0;
}
