#ifndef INCLUDE_OV01_SB_NATIVES_UNIT_H
#define INCLUDE_OV01_SB_NATIVES_UNIT_H

/* Functions of src/ov01/sb_natives_unit.c that other files call. */

struct XenoLight;
union DoublewordVector4;

/* Registers the bat, math and menu native tables after resetting the native counts and the runtime state. */
void runtimeInit(void);
void runtimeInitState(void);
/* Hands out the effect group and record the running script selected. */
void runEffGet(int *group, int *record);
struct XenoLight *lightBlockGet(void);
int bgmIndexGet(void);
/* Hands out the runtime position and direction vectors. */
void runtimeFogPtrGet(union DoublewordVector4 **pos, union DoublewordVector4 **dir);

#endif /* INCLUDE_OV01_SB_NATIVES_UNIT_H */
