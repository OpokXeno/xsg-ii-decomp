#ifndef INCLUDE_MAIN_TI_DYNAMICS_NODE_H
#define INCLUDE_MAIN_TI_DYNAMICS_NODE_H

/* Functions of src/main/ti_dynamics_node.c that other files call. */

#include "shared.h"
#include "main/TiMRPackage.h"

/* Unit quaternion (x, y, z, w) to a row-major rotation matrix. */
void XrgQuaternionToMatrix(Matrix4 matrix, const Vector4 *quaternion);
/* Quaternion (x, y, z, w) from the rotation part of a row-major 4x4 matrix. */
void XrgMatrixToQuaternion(float quat[4], Matrix4 matrix);
/* Rotation that takes `left` onto `right`; returns 0 when they are opposite. */
int XrgQuaternionFromVectors(Vector4 *destination, const Vector4 *left, const Vector4 *right);

/* Steps one dynamics node and returns the node that follows it. The first argument is not read. */
TiHrcDynamics *tiDynamicsNodeSolveNext(Matrix4 unused_localMat, TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state);

#endif /* INCLUDE_MAIN_TI_DYNAMICS_NODE_H */
