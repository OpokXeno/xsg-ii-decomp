#ifndef INCLUDE_MAIN_TIWATER_H
#define INCLUDE_MAIN_TIWATER_H

/* Functions of src/main/TiWater.c that other files call. */

#include "shared.h"

struct PxyHeader;

void tiWaterSceneLoop(void);
void tiWaterInitStudio(void);
void tiWaterObjectUpdate(struct PxyHeader *header, int stepMode);
f32 tiWaterVectorAngle(f32 x, f32 y);

#endif /* INCLUDE_MAIN_TIWATER_H */
