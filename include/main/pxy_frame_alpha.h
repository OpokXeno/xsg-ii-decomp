#ifndef INCLUDE_MAIN_PXY_FRAME_ALPHA_H
#define INCLUDE_MAIN_PXY_FRAME_ALPHA_H

#include "shared.h"

/* Functions of src/main/pxy_frame_alpha.c that other files call. */

void pxyClearFrameAlphaMsb(u32 frameMask, u32 frameBase);
void pxyFillFrameAlpha80(u32 frameMask, u32 frameBase);
void pxyModulateFrameByAlpha(int unusedFrameBase, float unusedScale);
void pxyClearDepthByFrameAlpha(u32 frameMask, u32 frameBase);

#endif /* INCLUDE_MAIN_PXY_FRAME_ALPHA_H */
