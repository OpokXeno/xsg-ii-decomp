#ifndef INCLUDE_MAIN_VU0_ROTATE_H
#define INCLUDE_MAIN_VU0_ROTATE_H

#include "shared.h"

/* Functions of src/main/vu0_rotate.c that other files call. */

void vu0RotateCurrentX(float angle);
void vu0RotateCurrentY(float angle);
void vu0RotateCurrentZ(float angle);
void vu0RotateCurrentXYZ(Vector4 *angles);
void vu0RotateCurrentYXZ(Vector4 *angles);
void vu0RotateCurrentZYX(Vector4 *angles);
void vu0SetCurrentRotationYXZ(Vector4 *angles);
void vu0TransformPoint(Vector4 *point);
void vu0TranslateCurrent(Vector4 *offset);
int vu0SetRotationFromDirection(Matrix4 matrix, Vector4 *to, Vector4 *from);
float vu0CopyMatrixWithTranslationDist(Vector4 *dst, Matrix4 src, Vector4 *aux, Vector4 *direction);

#endif /* INCLUDE_MAIN_VU0_ROTATE_H */
