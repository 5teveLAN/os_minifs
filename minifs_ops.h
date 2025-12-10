#ifndef MINIFS_OPS_H
#define MINIFS_OPS_H
#include <stdbool.h>
#include <stdint.h>


uint32_t fsread(uint32_t block, uint32_t offset);

uint32_t fsreadc(uint32_t block, uint32_t offset);

void fswrite(uint32_t block, uint32_t offset, uint32_t data);

void fswritec(uint32_t block, uint32_t offset, char data);

uint32_t mkFCB();

void loadVCB();

void loadBitmap();

uint32_t filefind(char *file_name);
#endif
