#ifndef INCLUDE_MAIN_PXY_DEBUG_DRAW_H
#define INCLUDE_MAIN_PXY_DEBUG_DRAW_H

#include "shared.h"

/* Functions of src/main/pxy_debug_draw.c that other files call: the debug text drawn through the pxy direct queue. */

u8 pxyDebugDrawChar(u8 ch, int x, int y, float scale, u32 color, u32 z, int flush);
int pxyDebugDrawString(const char *str, int x, int y, float scale, u32 color, u32 depth, int flush);
int pxyDebugDrawStringShadow(const char *str, int x, int y, float scale, u32 color, u32 depth, int flush);
void pxyDebugSetTextStyle(int color, float scale, int depth);
void pxyDebugPrintf(int x, int y, const char *format, ...);

#endif /* INCLUDE_MAIN_PXY_DEBUG_DRAW_H */
