#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define FCB_SIZE 32

int block_count;
int block_size;
int bmap_addr;
int fcb_start_addr;
int fcb_max_count;
int fcb_current_number;
char* FILE_NAME;
bool bitmap[100];

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

    touch("test");
    touch("test");
    

}
