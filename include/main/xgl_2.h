#ifndef INCLUDE_MAIN_XGL_2_H
#define INCLUDE_MAIN_XGL_2_H

#include "shared.h"

/* A vector kept as a plain float[4] (like sceVu0FVECTOR); its w is element 3. Vector4 here changes the
 * address arithmetic of the w stores, so the casts to Vector4 * at the calls stay. */
typedef float XglFVector[4];

/* Light set built by xglLightPackBuildLightSet, cleared with xglZeroQuadwords18 (0x18 quadwords). The labels
 * lightSetLogDump prints are the original's field names: seven sDir (directions and viewDirection), eight sCol
 * (colors and the two ambient colours), three sSha (shadows), three sPntP and three sPntC. */
typedef struct XglLightSet {
    XglFVector directions[6]; /* 0x000: sDir 0-5 */
    Vector4 viewDirection;  /* 0x060: sDir 6 */
    XglFVector colors[6];   /* 0x070: sCol 0-5 */
    Vector4 ambientColor;       /* 0x0D0: sCol 6 */
    Vector4 ambientColorLower;   /* 0x0E0: sCol 7 */
    /* 0x0F0: sSha 0-2. One array (was forward + reserved[2]): tu047 runtimeSetLight indexes it with kind - 5;
     * xgl_2.c builds shadows[0] from light keys 10 and 11. */
    Vector4 shadows[3];
    XglFVector pointPositions[3]; /* 0x120: sPntP */
    XglFVector pointColors[3]; /* 0x150: sPntC */
} XglLightSet;

/* The copies of the light set tu156 keeps are 16-byte aligned (their copies use ld/sd without an
 * alignment test). */
typedef XglLightSet AlignedXglLightSet __attribute__((aligned(16)));

/* A 4x4 matrix as xgl_2.c and nml_model_set.c read it: floats, rows, words or quadwords. */
typedef unsigned int XglMatrixQuadword __attribute__((mode(TI)));

typedef union Matrix {
    float elements[16];
    Matrix4 rows;
    int words[4][4];
    XglMatrixQuadword quadwords[4];
} Matrix;

/* Functions of src/main/xgl_2.c (the whole xgl_2 file) that other files call. */
void xglMatrixInverse(Matrix *destination, Matrix *source);

unsigned short xglSRand(void);
void xglLightPackCacheInvalidate(void);
void xglMatrixStackUnit(void);
void xglVectorScale(Vector4 *destination, const Vector4 *source, float scale);
void xglVectorMulMat(Vector4 *destination, Matrix4 matrix, const Vector4 *vector);
void xglMatrixRotY(Matrix4 destination, Matrix4 source, float angle);
void xglRotTransPersN(Vector4 *destination, Matrix4 matrix, const Vector4 *source,
                      int count, int cameraId);

#endif /* INCLUDE_MAIN_XGL_2_H */
