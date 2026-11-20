#ifndef INCLUDE_MAIN_YSEGF_H
#define INCLUDE_MAIN_YSEGF_H

#include "shared.h"

/* Functions of src/main/ysegf.c that other files call. */

void yg2PathResetAll(void);
/* seg.addrOn flags: one per segment address, index = address number - 1. */
int segGetAddrOn(u32 index);
void segSetAddrOn(u32 index, int value);
void *ysegfAllocFileBuffer(const char *name);
void ysegfReadFile(void **buffer, const char *name, int allocate, int mode);
void ysegfRunMenu(void);

#endif /* INCLUDE_MAIN_YSEGF_H */
