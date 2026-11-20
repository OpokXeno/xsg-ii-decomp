#ifndef INCLUDE_MAIN_XGL_MATRIX_AXES_H
#define INCLUDE_MAIN_XGL_MATRIX_AXES_H

#include "shared.h"

/* Functions of src/main/xgl_matrix_axes.c that other files call. */

void xglMatrixFromAxes(Matrix4 matrix, const Vector4 *axis0, const Vector4 *axis1, const Vector4 *axis2);

#endif /* INCLUDE_MAIN_XGL_MATRIX_AXES_H */
