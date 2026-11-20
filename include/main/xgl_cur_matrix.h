#ifndef INCLUDE_MAIN_XGL_CUR_MATRIX_H
#define INCLUDE_MAIN_XGL_CUR_MATRIX_H

#include "shared.h"

/* Functions of src/main/xgl_cur_matrix.c that other files call. */

void xglCurMatrixSet(Matrix4 matrix);
void xglCurMatrixGet(Matrix4 matrix);
void xglCurApplyVector(Vector4 *destination, const Vector4 *vector);
void xglCurSetRotateX(float value);
void xglCurSetRotateY(float angle);
void xglCurSetRotateZ(f32 angle);
void xglCurSetTranslate(const Vector4 *vector);
void xglCurSetScale(f32 scale);
void xglCurSetScaleXYZ(const Vector4 *scale);
void xglCurMatrixMul(Matrix4 matrix);

#endif /* INCLUDE_MAIN_XGL_CUR_MATRIX_H */
