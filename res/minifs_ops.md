# Variables

### FILE_NAME, bmap, fmap

```c
extern const char* FILE_NAME;
extern uint8_t *bmap, *fmap;
```
Defination them in the main program.
Const: Data can't be change, but the pointer can.
### vcb

```c
# minifs_ops.c
# include "minifs_olp"
VCB vcb;
```

```c
# minifs_ops.h
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
    
    uint32_t fcb_count;
} VCB;
```

To use it just declare `extern VCB vcb` in the main program.

## Functions

uint32_t fs_write_fcb(uint32_t id,FCB *source_fcb);

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


