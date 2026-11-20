#ifndef INCLUDE_MAIN_CUR_MATRIX_ASM_H
#define INCLUDE_MAIN_CUR_MATRIX_ASM_H

#include "shared.h"

/* One projected point as the rot-trans-pers functions store it: GS fixed-point x, y and z (the vftoi result
   of the VU0 vertex) and the float w that stays in the fourth word (the perspective scale). */
typedef struct ScreenVector4 {
    int x;
    int y;
    int z;
    float w;
} ScreenVector4;

/* Functions of src/main/cur_matrix_asm.c that other files call. */

void _CurSetViewScaleTrans(Vector4 *scale, Vector4 *translation);
void _CurMatrixScale(Vector4 *scale);
int _CurRotTransPersClipScreen(ScreenVector4 *screenOut, const Vector4 *point);
int _CurBoxVisible(Vector4 *boxMin, Vector4 *boxMax);

#endif /* INCLUDE_MAIN_CUR_MATRIX_ASM_H */
