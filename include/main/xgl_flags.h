#ifndef INCLUDE_MAIN_XGL_FLAGS_H
#define INCLUDE_MAIN_XGL_FLAGS_H

/* Functions of src/main/xgl_flags.c that other files call: the packed game-flag bit array. */

void xglFlagsInitial(void);

/* Reads or writes bit_count bits of the array starting at bit_offset. */
int xglFlagsGet(int bit_offset, int bit_count);
int xglFlagsSet(int bit_offset, int bit_count, int value);

/* The fixed-width forms: the width in bits is the digit of the name. */
int xglFlagsGet1(int bit_offset);
int xglFlagsGet2(int bit_offset);
int xglFlagsGet4(int bit_offset);
int xglFlagsGet8(int bit_offset);
int xglFlagsGet16(int bit_offset);
int xglFlagsGet32(int bit_offset);
unsigned long long xglFlagsGet64(int bit_offset);
int xglFlagsSet1(int bit_offset, int value);
int xglFlagsSet2(int bit_offset, int value);
int xglFlagsSet4(int bit_offset, int value);
int xglFlagsSet8(int bit_offset, int value);
int xglFlagsSet16(int bit_offset, int value);
int xglFlagsSet32(int bit_offset, int value);
long long xglFlagsSet64(int bit_offset, long long value);

#endif /* INCLUDE_MAIN_XGL_FLAGS_H */
