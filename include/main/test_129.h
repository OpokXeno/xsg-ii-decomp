#ifndef INCLUDE_MAIN_TEST_129_H
#define INCLUDE_MAIN_TEST_129_H

/* Functions of src/main/test_129.c that other files call. */

#include "main/TiWatSim.h"

/* The data of src/main/test_129.c that TiWater.c reads: the debug panel (mode, selected parameter values, steps and
 * labels; the values are floats the panel edits in place) and the water table registry. */
extern float tiWaterTwoPi;
extern int tiWaterDebugMode;
extern int tiWaterSubCount;
extern TiWaterSub *tiWaterSubList[];
extern const char *tiWaterDebugLabels[];
extern float *tiWaterDebugValues[];
extern float tiWaterDebugSteps[];
extern float tiWaterEffectNum;
extern float tiWaterCx;
extern float tiWaterCy;
extern float tiWaterAlphaType;
extern float tiWaterAlpha;
extern float tiWaterFbp;
extern float tiWaterBlur;
extern float tiWaterDecrease;
extern float tiWaterIntVals[4];

int testLoadFile(int *bufferCursor, const char *name);
/* Converts the two-byte Shift-JIS character at first/second to EUC in place. */
void sjisToEuc(unsigned char *first, unsigned char *second);

#endif /* INCLUDE_MAIN_TEST_129_H */
