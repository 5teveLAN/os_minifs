#ifndef VCB_H
#define VCB_H

#include <stdint.h>

typedef struct {
    uint32_t block_count;
    uint32_t block_size;
    uint32_t fcb_size;

    uint32_t bmap_start_block;
    uint32_t bmap_end_block;

    uint32_t fmap_start_block;
    uint32_t fmap_end_block;

    uint32_t fcb_start_addr;
    uint32_t fcb_end_block;
} VCB;


