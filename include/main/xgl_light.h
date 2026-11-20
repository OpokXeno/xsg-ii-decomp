#ifndef INCLUDE_MAIN_XGL_LIGHT_H
#define INCLUDE_MAIN_XGL_LIGHT_H

#include "shared.h"

/* Functions of src/main/xgl_light.c that other files call. */

/* A vector kept as a plain float[4] (like sceVu0FVECTOR); its w is element 3. Vector4 here changes the
 * address arithmetic of the w stores, so the casts to Vector4 * at the calls stay. */
typedef float XglFVector[4];

/* Light set built by xglLightPackBuildLightSet, cleared with xglLightSetClear (0x18 quadwords). The labels
 * lightSetLogDump prints are the original's field names: seven sDir (directions and viewDirection), eight sCol
 * (colors and the two ambient colours), three sSha (shadows), three sPntP and three sPntC. */
typedef struct XglLightSet {
    XglFVector directions[6]; /* 0x000: sDir 0-5 */
    Vector4 viewDirection;  /* 0x060: sDir 6 */
    XglFVector colors[6];   /* 0x070: sCol 0-5 */
    Vector4 ambientColor;       /* 0x0D0: sCol 6 */
    Vector4 ambientColorLower;   /* 0x0E0: sCol 7 */
    /* 0x0F0: sSha 0-2. One array (was forward + reserved[2]): ov01 sb_natives_unit.c runtimeSetLight
     * indexes it with kind - 5;
     * xglLightPackBuildLightSet builds shadows[0] from light keys 10 and 11. */
    Vector4 shadows[3];
    XglFVector pointPositions[3]; /* 0x120: sPntP */
    XglFVector pointColors[3]; /* 0x150: sPntC */
} XglLightSet;

/* The copies of the light set sef_light_set.c keeps are 16-byte aligned (their copies use ld/sd without an
 * alignment test). */
typedef XglLightSet AlignedXglLightSet __attribute__((aligned(16)));
void xglLightPackCacheInvalidate(void);
void *xglLightPackCacheGet(void);
void xglStudioLightReset(void *light);
void xglLightSetDefault(XglLightSet *lightSet);
void xglLightSetClear(XglLightSet *lightSet);
void xglLightPackTableInit(void);
void xglLightPackTableClear(void);
void xglLightPackEvaluateAll(float time);
void xglLightPackBuildByName(XglLightSet *light, const char *packName, Vector4 *receiver);
void xglLightPackDebugDrawAll(void);

#endif /* INCLUDE_MAIN_XGL_LIGHT_H */
