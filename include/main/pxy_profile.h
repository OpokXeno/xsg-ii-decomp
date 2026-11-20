#ifndef INCLUDE_MAIN_PXY_PROFILE_H
#define INCLUDE_MAIN_PXY_PROFILE_H

#include "shared.h"
#include "main/pxy_packet_state.h"

/* Functions of src/main/pxy_profile.c that other files call: the per-frame timer entries and the VU0 matrix helper
 * that shares the file. */

float pxyKeyInterpolate(Vector4Elements *keyA, Vector4Elements *keyB, float time, float blend);
void _CurMatrixMul33normXYZW(float *dst, const float *src);
void pxyProfileClearEntries(void);
void pxyProfileBegin(const char *name);
float pxyProfileEnd(const char *name);
float pxyProfileEndMs(const char *name);
float pxyProfileGetMs(const char *name);
float pxyProfileGetMsUnchecked(const char *name);
void pxyProfileDrawEntries(void);

#endif /* INCLUDE_MAIN_PXY_PROFILE_H */
