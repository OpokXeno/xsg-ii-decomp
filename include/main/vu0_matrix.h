#ifndef INCLUDE_MAIN_VU0_MATRIX_H
#define INCLUDE_MAIN_VU0_MATRIX_H

#include "shared.h"

/* Functions of src/main/vu0_matrix.c that other files call. */

/* The identity matrix the rotation functions start from (test_128.c defines it). */
extern const Vector4 vu0IdentityMatrix[4];

void vu0CurMatrixMulRight(Vector4 *rows);
void vu0CurMatrixMulRows3(Vector4 *rows);
void vu0SetCurrentRotationX(float angle);
void vu0SetCurrentRotationY(float angle);
void vu0SetCurrentRotationZ(float angle);

#endif /* INCLUDE_MAIN_VU0_MATRIX_H */
