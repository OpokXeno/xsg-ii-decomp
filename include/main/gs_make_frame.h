#ifndef INCLUDE_MAIN_GS_MAKE_FRAME_H
#define INCLUDE_MAIN_GS_MAKE_FRAME_H

#include "shared.h"

/* Functions of src/main/gs_make_frame.c that other files call. */

u64 gsMakeFrame(int buffer, int flag);
int gsMakeAlpha(int blend);
void xglVectorNormalInPlace(Vector4 *v);

#endif /* INCLUDE_MAIN_GS_MAKE_FRAME_H */
