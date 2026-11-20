#ifndef INCLUDE_MAIN_TIDRAWJOINT_H
#define INCLUDE_MAIN_TIDRAWJOINT_H

/* Functions of src/main/TiDrawJoint.c that other files call. */

#include "shared.h"
#include "main/cur_matrix_asm.h"

void tiDrawJointDrawAxes(Matrix4 matrix, float value);
void tiDrawJointScreenPoints(unsigned int count, const Vector4 *source, ScreenVector4 *destination);
void tiDrawJointTransformPoints(unsigned int count, Matrix4 matrix, const Vector4 *source, Vector4 *destination);
void tiDrawJointPackPoints(unsigned int count, const ScreenVector4 *source, const unsigned int *colour, int flag);
struct TiDrawJointState;
void tiDrawJointDrawLists(struct TiDrawJointState *state, float (*pGlobalMat)[4][4], float (*current)[4][4]);

#endif /* INCLUDE_MAIN_TIDRAWJOINT_H */
