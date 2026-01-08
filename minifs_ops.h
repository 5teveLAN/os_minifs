#ifndef MINIFS_OPS_H
#define MINIFS_OPS_H
#include <stdbool.h>
#include <stdint.h>

extern const char* FILE_NAME;
extern uint8_t *bmap, *fmap;

typedef struct {
    uint32_t block_size;
    uint32_t block_count;
    uint32_t fcb_size;

    uint32_t bmap_start_block;
    uint32_t bmap_end_block;

    uint32_t fmap_start_block;
    uint32_t fmap_end_block;

    uint32_t fcb_start_block;
    uint32_t fcb_end_block;
    uint32_t fcb_count;
} VCB;

typedef struct  {
    uint32_t is_dir; //4 byte
    uint32_t file_size; //4 byte
    uint32_t dbp[4]; //16 byte
    uint8_t unused[8];     
} FCB; 

#pragma pack(push, 1)
typedef struct {
    uint32_t file_id; //4byte
    char file_name[60]; //60byte
} Dentry;
#pragma pack(pop)
void print_dentry(Dentry* dentry);
void fs_delete_dentry(uint32_t dir_id, uint32_t index);

void fs_write_dentry(uint32_t dir_id,Dentry *new_dentry);

uint32_t fmap_new();

uint32_t bmap_new();

Dentry fs_read_dentry(uint32_t dir_id, uint32_t index);

void fs_write_fcb(uint32_t id,FCB *source_fcb);

FCB fs_read_fcb(uint32_t id);

uint32_t fs_read_uint32(uint32_t block, uint32_t offset);

uint8_t fs_read_char(uint32_t block, uint32_t offset);

void fs_write_uint32(uint32_t block, uint32_t offset, uint32_t data);

void fs_write_char(uint32_t block, uint32_t offset, char data);

void vcb_load();

void vcb_save();

void bmap_load();

void bmap_save();

void fmap_load();

void fmap_save();

void debug_dump();

uint32_t find_file(uint32_t dir_id, char* file_name);
#endif
