#ifndef INCLUDE_MAIN_XGL_CUR_MUL_H
#define INCLUDE_MAIN_XGL_CUR_MUL_H

#include "shared.h"

/* Functions of src/main/xgl_cur_mul.c that other files call. */

void xglCurMulRotateX(float angle);
void xglCurMulRotateY(float angle);
void xglCurMulRotateZ(float angle);
void xglCurMulTranslate(float *translation);
void xglCurMulScale(f32 scale);
void xglCurMulScaleXYZ(const f32 scale[3]);
void xglCurMulInvScaleXYZ(f32 scale[3]);

#endif /* INCLUDE_MAIN_XGL_CUR_MUL_H */
