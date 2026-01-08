#include "minifs_ops.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32 
#include <winsock2.h>//代表只要是 Windows 系統就包含這個頭檔
#else
#include <arpa/inet.h>//代表只要是 Linux 系統就包含這個頭檔
#endif
#define DBP_COUNT 4
#define DENTRY_SIZE 64
#define DENTRY_COUNT 64

extern const char* FILE_NAME;
extern uint8_t *bmap, *fmap;
extern VCB vcb;

// We want to write the dentry to its directory!
// its directory = 

uint32_t find_file(uint32_t dir_id, char* file_name){
    for (uint32_t index = 1; index < DENTRY_COUNT; ++index){
        Dentry query_dir = fs_read_dentry(dir_id, index);
        if (strcmp(query_dir.file_name, file_name)==0) //no difference
            return index;
            // impossible to be '0'(root dir)
    }
    return 0;
}
void fs_write_dentry(uint32_t dir_id,Dentry *new_dentry){
    uint32_t dir_dbp0 = fs_read_fcb(dir_id).dbp[0];
    Dentry temp_dentry = *new_dentry; //copy struct
    temp_dentry.file_id = htonl(new_dentry->file_id);

    // for 0 ~ DENTRY_COUNT
    for (uint32_t index = 0; index < DENTRY_COUNT; ++index){
        Dentry query_dir = fs_read_dentry(dir_id, index);
        // empty space found

        if (query_dir.file_id == 0 && query_dir.file_name[0] == '\0'){
            FILE *fp = fopen(FILE_NAME, "r+b");
            uint32_t offset = dir_dbp0*vcb.block_size + index*DENTRY_SIZE;
            fseek(fp, offset, SEEK_SET);
            fwrite(&temp_dentry, sizeof(Dentry), 1, fp);
            fclose(fp);
            printf("fount empty space for dentry! index: %d\n",index);
            return;
        }


    }
    // no empty space found
    printf("The directory is full!\n");
    return;

}

void print_dentry(Dentry* dentry){
    printf("file_id   = %d \n", dentry->file_id);
    printf("file_name = %s \n", dentry->file_name);

} 

void fs_delete_dentry(uint32_t dir_id, uint32_t index){
    FILE *fp = fopen(FILE_NAME, "r+b");
    Dentry target_dentry = fs_read_dentry(dir_id, index);
    FCB dir_fcb = fs_read_fcb(dir_id);
    uint32_t dir_dbp0 = dir_fcb.dbp[0];
    uint32_t offset = dir_dbp0*vcb.block_size + index*DENTRY_SIZE;
    Dentry temp_dentry;

    memset(&temp_dentry, 0, sizeof(Dentry));
    fseek(fp, offset, SEEK_SET);
    fwrite(&temp_dentry, sizeof(Dentry), 1, fp);
    fclose(fp);
}

Dentry fs_read_dentry(uint32_t dir_id, uint32_t index){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t dir_dbp0 = fs_read_fcb(dir_id).dbp[0];
    Dentry dentry;
    fseek(fp, dir_dbp0*vcb.block_size  + index*DENTRY_SIZE, SEEK_SET);
    fread(&dentry, sizeof(Dentry), 1, fp);
    fclose(fp);
    dentry.file_id = ntohl(dentry.file_id);  // 大端to host
    return dentry;
}
//get unused fcb id
uint32_t fmap_new(){
    uint32_t id = 0;
    for (int i = 1; i < vcb.fcb_count; ++i)
        if (fmap[i]==0){
            id = i;
            break;
        }
    
    if (!id){
        return 0;
    }
    else {
        fmap[id]=1;
        return id;
    }
}

//get unused block id
uint32_t bmap_new(){
    uint32_t block = 0;
    for (uint32_t i = vcb.fcb_end_block + 1; i < vcb.block_count; ++i)
        if (bmap[i]==0){
            block = i;
            break;
        }
    
    if (!block){
        return 0;
    }
    else {
        bmap[block]=1;
        return block;
    }
}
// write fcb data in big-endian 
void fs_write_fcb(uint32_t id,FCB *source_fcb){
    FILE *fp = fopen(FILE_NAME, "r+b");
    // turn struct to big-endian
    FCB temp_fcb;
    memset(&temp_fcb, 0, sizeof(FCB));
                                // source_fcb is a pointer
    temp_fcb.is_dir    = htonl(source_fcb->is_dir);
    temp_fcb.file_size = htonl(source_fcb->file_size);  // 轉成大端

    for (int i = 0; i < DBP_COUNT; ++i){
        temp_fcb.dbp[i]      = htonl(source_fcb->dbp[i]);       // 轉成大端
    }
    fseek(fp, (vcb.fcb_start_block*vcb.block_size) + (id*vcb.fcb_size)
          , SEEK_SET);
    fwrite(&temp_fcb, sizeof(FCB), 1, fp);
    fclose(fp);

}
// read fcb data in big-endian 
FCB fs_read_fcb(uint32_t id){
    FCB fcb;
    FILE *fp = fopen(FILE_NAME, "r+b");
    fseek(fp, (vcb.fcb_start_block*vcb.block_size) + (id*vcb.fcb_size)
          , SEEK_SET);
    fread(&fcb, sizeof(FCB), 1, fp);
    fclose(fp);
                     // fcb is a FCB struct
    fcb.is_dir = ntohl(fcb.is_dir);
    fcb.file_size = ntohl(fcb.file_size);  // 大端to host

    for (int i = 0; i < DBP_COUNT; ++i){
        fcb.dbp[i]      = ntohl(fcb.dbp[i]);       
    }
    

    return fcb;
}

// read 4 byte data in big-endian 
uint32_t fs_read_uint32(uint32_t block, uint32_t offset){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t BLOCK_SIZE = 0;
    uint32_t data = 0x00;
    if (block!=0)
        BLOCK_SIZE = fs_read_uint32(0, 4); 

    fseek(fp, block*BLOCK_SIZE+offset, SEEK_SET);
    for (int boffset = 24; boffset >= 0; boffset-=8){
        data |= (uint32_t)fgetc(fp)<<boffset;//BUG IS HERE
    }
    fclose(fp);
    return data;
}
// read 1 byte data (fgetc)
uint8_t fs_read_char(uint32_t block, uint32_t offset){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t BLOCK_SIZE = 0;
    uint32_t data = 0x00;
    if (block!=0)
        BLOCK_SIZE = fs_read_uint32(0, 4); 

    fseek(fp, block*BLOCK_SIZE+offset, SEEK_SET);
    data = fgetc(fp);
    fclose(fp);
    return data;
}
// write 4 byte data in big-endian 
void fs_write_uint32(uint32_t block, uint32_t offset, uint32_t data){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t BLOCK_SIZE = 0;
    if (block!=0)
        BLOCK_SIZE = fs_read_uint32(0, 4); 

    fseek(fp, block*BLOCK_SIZE+offset, SEEK_SET);
    for (int boffset = 24; boffset >= 0; boffset-=8){
        fputc((data>>boffset) & 0xFF, fp);
    }
    fclose(fp);
}

void fs_write_char(uint32_t block, uint32_t offset, char data){
    FILE *fp = fopen(FILE_NAME, "r+b");
    uint32_t BLOCK_SIZE = 0;
    if (block!=0)
        BLOCK_SIZE = fs_read_uint32(0, 4); 

    fseek(fp, block*BLOCK_SIZE+offset, SEEK_SET);
    fputc(data, fp);
    fclose(fp);
}
void vcb_load(){
    uint32_t offset = 0;
    vcb.block_size       = fs_read_uint32(0, offset); offset += 4;
    vcb.block_count      = fs_read_uint32(0, offset); offset += 4;
    vcb.fcb_size         = fs_read_uint32(0, offset); offset += 4;
    vcb.bmap_start_block = fs_read_uint32(0, offset); offset += 4;
    vcb.bmap_end_block   = fs_read_uint32(0, offset); offset += 4;
    vcb.fmap_start_block = fs_read_uint32(0, offset); offset += 4;
    vcb.fmap_end_block   = fs_read_uint32(0, offset); offset += 4;
    vcb.fcb_start_block   = fs_read_uint32(0, offset); offset += 4;
    vcb.fcb_end_block    = fs_read_uint32(0, offset); offset += 4;
    vcb.fcb_count      = fs_read_uint32(0, offset); offset += 4;
}

void vcb_save(){
    uint32_t offset = 0;
    fs_write_uint32(0, offset,vcb.block_size       ); offset += 4;
    fs_write_uint32(0, offset,vcb.block_count      ); offset += 4;
    fs_write_uint32(0, offset,vcb.fcb_size         ); offset += 4;
    fs_write_uint32(0, offset,vcb.bmap_start_block ); offset += 4;
    fs_write_uint32(0, offset,vcb.bmap_end_block   ); offset += 4;
    fs_write_uint32(0, offset,vcb.fmap_start_block ); offset += 4;
    fs_write_uint32(0, offset,vcb.fmap_end_block   ); offset += 4;
    fs_write_uint32(0, offset,vcb.fcb_start_block  ); offset += 4;
    fs_write_uint32(0, offset,vcb.fcb_end_block    ); offset += 4;
    fs_write_uint32(0, offset,vcb.fcb_count        ); offset += 4;
}

// To use the following functions, you need to load vcb first.
void bmap_load(){
    for (uint32_t i = 0; i < vcb.block_count; ++i){
        bmap[i] = fs_read_char(vcb.bmap_start_block,i);
    }
}

void bmap_save(){
    for (uint32_t i = 0; i < vcb.block_count; ++i){
        fs_write_char(vcb.bmap_start_block,i,bmap[i]);
    }
}

void fmap_load(){
    for (uint32_t i = 0; i < vcb.fcb_count; ++i){
        fmap[i] = fs_read_char(vcb.fmap_start_block, i + 100);
    }
}

void fmap_save(){
    for (uint32_t i = 0; i < vcb.fcb_count; ++i){
        fs_write_char(vcb.fmap_start_block, i + 100, fmap[i]);
    }
}

void debug_dump(){
    //bmap debug
    printf("\nbmap debug\n");
    bmap[10] = 1;
    bmap_save();
    bmap_load();
    for (uint32_t i = 0; i < vcb.block_count; ++i)
        printf("%d",bmap[i]);
    printf("\n%d\n",vcb.block_count);

    //fmap debug
    printf("\nfmap debug\n");
    fmap[10] = 1;
    fmap_save();
    fmap_load();
    for (uint32_t i = 0; i < vcb.fcb_count; ++i)
        printf("%d",fmap[i]);
    printf("\n%d\n",vcb.fcb_count);
    
    //fcb debug
    printf("\nfcb debug\n");
    int id = 10;
    FCB fcb = fs_read_fcb(10);
    printf("fcb%d.is_dir=%d\n", id, fcb.is_dir);
    printf("fcb%d.file_size=%d\n", id, fcb.file_size);
    for (int i = 0; i < DBP_COUNT; ++i){
        printf("fcb%d.dbp[%d]=%d\n", id, i,fcb.dbp[i]);
    }

}
