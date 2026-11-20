#ifndef INCLUDE_MAIN_TIDYNAMICSCOLL_H
#define INCLUDE_MAIN_TIDYNAMICSCOLL_H

/* The collision records of the hierarchy data (TiHrcData sphere, matrix and capsule tables) and the functions of
 * src/main/TiDynamicsColl.c and src/main/ti_dynamics_coll_push.c that other files call. */

#include "shared.h"
#include "main/TiMRPackage.h"
#include "main/xgl_2.h"

/* Record flags bit 0: the record takes no push-out. */
#define TI_DYNAMICS_COLL_RECORD_DISABLED 0x1

/* Three-float vector for the solvers' offsets (a Vector4 would change their frames). */
typedef struct TiVector3 {
    float x;
    float y;
    float z;
} TiVector3;

/* 48-byte sphere record. matrixIndex selects the global matrix that transforms src into the centre (the build pass
 * skips an index of -1); radius is at +4 and flags bit 0 disables the push-out. */
typedef struct TiCollSphere {
    int matrixIndex;
    float radius;
    unsigned int flags;
    unsigned char unmodeled_0C[0x4];
    Vector4 src;
    Vector4 center;             /* +0x20 */
} TiCollSphere;

/* 208-byte record of the ground-plane clamp: index selects the palette matrix, flags bit 0 gates the push-out.
 * matrix_90 is built from the global matrix of the record (the inverse, or the identity for an index of -1);
 * a point is taken through matrix_90 then matrix_50, and back out through matrix_10. */
typedef struct TiCollPlane {
    int index;
    unsigned int flags;
    unsigned char unmodeled_08[0x8];
    Matrix4 matrix_10;
    Matrix4 matrix_50;
    Matrix matrix_90;
} TiCollPlane;

/* 224-byte capsule record: index selects a 64-byte palette matrix, flags bit 0 disables the push-out, radius and
 * height size the ring and the two end points. matrix_a0 (built like matrix_90) and matrix_60 take the point
 * into the capsule frame (in that order), matrix_20 maps it back out before the palette matrix. */
typedef struct TiCollCapsule {
    int index;
    unsigned int flags;
    unsigned char unmodeled_08[0x8];
    float radius;
    float height;
    unsigned char unmodeled_18[0x8];
    Matrix4 matrix_20;
    Matrix4 matrix_60;
    Matrix matrix_a0;
} TiCollCapsule;

/* Transforms each sphere record from src into its centre with its global matrix. */
int tiMRPackageGetNodePoint(TiMRPackage *pMRPack, u32 key, Vector4 *dst, int apply);
void tiDynamicsCollTransformVectors(TiMRPackage *pMRPack);
void tiDynamicsCollBuildInverseMatrices(TiMRPackage *pMRPack);
void tiDynamicsCollBuildWideInverseMatrices(TiMRPackage *pMRPack);

void tiDynamicsCollPushOutSphereList(TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state);
void tiDynamicsCollPushOutPlaneList(TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state);
void tiDynamicsCollPushOutCapsuleList(TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state);
void tiDynamicsCollSolveSpherePair(TiSolverState *first, TiSolverState *second, TiMRPackage *object);
void tiDynamicsCollSolvePlanePair(TiSolverState *first, TiSolverState *second, TiMRPackage *object);
void tiDynamicsCollSolveCapsulePair(TiSolverState *first, TiSolverState *second, TiMRPackage *object);

#endif /* INCLUDE_MAIN_TIDYNAMICSCOLL_H */
