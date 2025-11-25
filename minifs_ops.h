#ifndef MINIFS_OPS_H
#define MINIFS_OPS_H
#include <stdbool.h>
#include <stdint.h>


uint32_t fsread(uint32_t block, uint32_t offset);

uint32_t fsreadc(uint32_t block, uint32_t offset);

uint32_t fswrite(uint32_t block, uint32_t offset, uint32_t data);

uint32_t fswritec(uint32_t block, uint32_t offset, char data);

uint32_t mkFCB();

uint32_t loadVCB();

uint32_t loadBitmap();

#endif

