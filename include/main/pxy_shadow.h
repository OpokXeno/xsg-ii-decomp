#ifndef INCLUDE_MAIN_PXY_SHADOW_H
#define INCLUDE_MAIN_PXY_SHADOW_H

#include "shared.h"
#include "main/xgl_2.h"
#include "main/xgl_light.h"

/* The 0x180-byte light block of a shadow projector, cleared for each model: the XglLightSet layout with the first
 * direction as a Vector4 (the matrix multiply and the w store take a vector) and the first colour as three floats. */
typedef struct PxyShadowLight {
    Vector4 direction; /* +0x00: directions[0] */
    u8 unmodeled_10[0x70 - 0x10];
    float color[3]; /* +0x70: colors[0] */
    u8 unmodeled_7C[0x180 - 0x7C];
} PxyShadowLight;

/* The shadow projector pxyCurrentShadow points to (0x36 quadwords). */
typedef struct PxyShadow {
    Matrix light; /* +0x000: light space to world */
    Matrix textureInverse; /* +0x040 */
    Matrix view; /* +0x080: world to light space */
    Matrix project; /* +0x0c0: world to shadow texture */
    PxyShadowLight lighting; /* +0x100 */
    int x; /* +0x280: texture rectangle */
    int y; /* +0x284 */
    int width; /* +0x288 */
    int height; /* +0x28c */
    s16 repeatU; /* +0x290: REGION_REPEAT instead of REGION_CLAMP */
    s16 repeatV; /* +0x292 */
    s16 psm; /* +0x294 */
    s16 frameBase; /* +0x296 */
    int clutOffset; /* +0x298 */
    u8 unk29C[4];
    float scaleU; /* +0x2a0 */
    float scaleV; /* +0x2a4 */
    u8 unk2A8[8];
    u64 tex0; /* +0x2b0 */
    u64 clamp; /* +0x2b8 */
    Vector4 uv; /* +0x2c0: offset u, scale u, offset v, scale v */
    Vector4 unk2D0; /* +0x2d0 */
    Vector4 direction; /* +0x2e0 */
    Vector4 center; /* +0x2f0 */
    Vector4 min; /* +0x300 */
    Vector4 max; /* +0x310 */
    Vector4 bounds; /* +0x320: centre and radius */
    int unk330; /* +0x330 */
    u8 unk334[4];
    u64 alphaData; /* +0x338: GS ALPHA_1 data (0x42 or 0x89) */
} PxyShadow;

#endif /* INCLUDE_MAIN_PXY_SHADOW_H */
