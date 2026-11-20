#include "common.h"
#include "shared.h"
#include "main/assert_fail.h"
#include "main/xgl_vector_ops.h"
#include "main/xgl_cur_mul.h"
#include "main/xgl_cur_matrix.h"
#include "main/TiMRPackage.h"
#include "main/TiXacAnimCurve.h"
#include "main/xgl_2.h"
#include "main/TiMotionSolver.h"
#include "main/TiDynamicsColl.h"
#include "main/TiMtnBlend.h"
#include "main/TiWindBase.h"
#include "main/ti_dynamics_node.h"
#include "main/xgl_vu1_regs.h"
/* TiMotionTarget.flags: the target applies its angle. */
#define TI_TARGET_FLAG_ANGLE 0x10
/* TiHrc* node flags: undo the parent's scale. */
#define TI_NODE_FLAG_INV_PARENT_SCALE 2
/* TiMRPackage.flags: some node has a non-unit scale. */
#define TI_MR_STATE_FLAG_SCALED 0x20
/* Every hierarchy starts with two locators, then the joints. */
#define TI_LOCATOR_COUNT 2
/* TiHrcSkip.limitFlags: a minimum or maximum clamp on each rotation value (X, Y, Z = values[3..5]). */
#define TI_SKIP_LIMIT_ROT_X_MIN 0x1
#define TI_SKIP_LIMIT_ROT_X_MAX 0x2
#define TI_SKIP_LIMIT_ROT_Y_MIN 0x4
#define TI_SKIP_LIMIT_ROT_Y_MAX 0x8
#define TI_SKIP_LIMIT_ROT_Z_MIN 0x10
#define TI_SKIP_LIMIT_ROT_Z_MAX 0x20

extern const unsigned char tiMotionAssertPPlaceMat[];
extern const unsigned char tiMotionSourceFile[];


/* Multiplies `destination` by a rotation of
 * -angle / 2 when the target's angle flag is set and the range is collapsed (range[1] <= range[0]). */
void tiMotionApplyTargetAngle(Matrix4 destination, Matrix4 *placeMat, TiMRState *target, const float *range)
{
    Matrix4 delta;

    if (placeMat == 0) {
        assertFailed(tiMotionAssertPPlaceMat, tiMotionSourceFile, 14);
    }
    if ((target->flags & TI_TARGET_FLAG_ANGLE) == 0 || range[1] > range[0]) {
        return;
    }
    xglCurSetRotateX(-target->aimAngle * 0.5f);
    xglCurMatrixGet(delta);
    xglMatrixMul(destination, destination, delta);
}


void tiMotionAimTarget(Matrix4 destination, Matrix4 *placeMat, TiMRState *target, const float *range)
{
    Matrix placeInverse;
    Matrix placeMatrix;
    Vector4 local;
    float angle;
    float value;

    if (placeMat == 0) {
        assertFailed(tiMotionAssertPPlaceMat, tiMotionSourceFile, 36);
    }
    if ((target->flags & TI_TARGET_FLAG_ANGLE) != 0) {
        xglMatrixMul(placeMatrix.rows, *placeMat, destination);
        xglMatrixInverse(&placeInverse, &placeMatrix);
        xglVectorMulMat(&local, placeInverse.rows, &target->aimVector);
        if (range[1] > range[0]) {
            float twoPi;
            twoPi = 6.2831855f;
            target->aimAngle = 1.5707964f - xglAtan2Angle(local.x, local.z) + twoPi;
            target->aimRotateX = 6.2831855f - xglAtan2Angle(local.z, local.y);
            target->aimRotateZ = 0.0f;
            while (3.1415927f < target->aimAngle) {
                target->aimAngle = target->aimAngle - 6.2831855f;
            }
            if (3.1415927f < target->aimRotateX) {
                angle = target->aimRotateX;
                do {
                    value = angle - 6.2831855f;
                    angle = value;
                } while (3.1415927f < value);
                target->aimRotateX = value;
            }
            if (target->aimAngle < -0.9f) {
                target->aimAngle = -0.9f;
            }
            if (0.9f < target->aimAngle) {
                target->aimAngle = 0.9f;
            }
            if (target->aimRotateX < -0.4f) {
                target->aimRotateX = -0.4f;
            }
            if (0.4f < target->aimRotateX) {
                target->aimRotateX = 0.4f;
            }
            target->aimAngle = (target->aimAngle - target->aimPrevious[1]) * 0.15f + target->aimPrevious[1];
            value = (target->aimRotateX - target->aimPrevious[0]) * 0.15f + target->aimPrevious[0];
            target->aimRotateX = value;
        } else {
            target->aimRotateZ = xglAtan2Angle(local.x, local.y);
            target->aimAngle = -xglAtan2Angle(local.x, local.z) + 6.2831855f;
            target->aimRotateX = 0.0f;
            while (3.1415927f < target->aimRotateZ) {
                target->aimRotateZ = target->aimRotateZ - 6.2831855f;
            }
            while (3.1415927f < target->aimAngle) {
                target->aimAngle = target->aimAngle - 6.2831855f;
            }
            if (target->aimRotateZ < -0.4f) {
                target->aimRotateZ = -0.4f;
            }
            if (0.4f < target->aimRotateZ) {
                target->aimRotateZ = 0.4f;
            }
            if (target->aimAngle < -0.9f) {
                target->aimAngle = -0.9f;
            }
            if (0.9f < target->aimAngle) {
                target->aimAngle = 0.9f;
            }
            target->aimAngle = (target->aimAngle - target->aimPrevious[1]) * 0.15f + target->aimPrevious[1];
            target->aimRotateZ = (target->aimRotateZ - target->aimPrevious[2]) * 0.15f + target->aimPrevious[2];
        }
    } else {
        value = target->aimRotateX + target->aimAngle + target->aimRotateZ;
        if (value == 0.0f) {
            return;
        }
        target->aimRotateX = target->aimRotateX * 0.8f;
        target->aimAngle = target->aimAngle * 0.8f;
        target->aimRotateZ = target->aimRotateZ * 0.8f;
    }
    xglCurSetRotateY(target->aimAngle);
    if (range[1] > range[0]) {
        xglCurMulRotateX(target->aimRotateX);
    } else {
        xglCurMulRotateZ(target->aimRotateZ);
    }
    xglCurMatrixGet(placeMatrix.rows);
    xglMatrixMul(destination, destination, placeMatrix.rows);
}








extern const unsigned char tiMotionAssertPLocalMat[];
extern const unsigned char tiMotionAssertPHrcData[];


extern const unsigned char tiMotionParentScaleAssertText[];
extern const unsigned char tiMotionAssertParentScale[];

/* Scale lookup for node `index` in the pMRPack table at +0xD0; asserts "can not found parent scale". */
float *tiMotionFindParentScale(TiMRPackage *pMRPack, int index)
{
    TiHrcData *hrc;
    TiHrcSkip *skips;
    TiHrcDynamics *dynamics;
    unsigned char *vectors;
    int vectorBytes;
    int dynamicsIndex;
    unsigned int skipEnd;
    int steps;
    unsigned int base;
    unsigned int rangeEnd;

    if (index < 0) {
        return 0;
    }
    if (index < TI_LOCATOR_COUNT) {
        return pMRPack->hrcData->locators[index].scale;
    }
    hrc = pMRPack->hrcData;
    if ((unsigned int) index < (unsigned int) hrc->jointCount) {
        return hrc->joints[index - TI_LOCATOR_COUNT].scale;
    }
    base = hrc->jointCount;
    rangeEnd = base + hrc->joint2Count;
    if (index < rangeEnd) {
        TiHrcJoint *joint2 = (TiHrcJoint *) ((unsigned char *) hrc + hrc->joint2Offset);
        return joint2[index - base].scale;
    }
    base = rangeEnd;
    if (index < base + hrc->joint3Count) {
        TiHrcJoint *joint3 = (TiHrcJoint *) ((unsigned char *) hrc + hrc->joint3Offset);
        return joint3[index - base].scale;
    }
    base += hrc->joint3Count;
    skipEnd = base + hrc->skipCount;
    if (index < skipEnd) {
        skips = (TiHrcSkip *) ((unsigned char *) hrc + hrc->skipOffset);
        return skips[index - base].scale;
    }
    base = skipEnd;
    if (index < base + hrc->dynamicsCount) {
        skipEnd = base + hrc->dynamicsCount;
        dynamicsIndex = index - base;
        dynamics = (TiHrcDynamics *) ((unsigned char *) hrc + hrc->dynamicsOffset);
        if (dynamicsIndex > 0) {
            steps = dynamicsIndex;
            do {
                vectorBytes = (dynamics->sphereCount + dynamics->matrixCount
                               + dynamics->capsuleCount) * 16;
                vectors = (unsigned char *) dynamics->entries;
                dynamics = (TiHrcDynamics *) (vectors + vectorBytes);
            } while (--steps != 0);
        }
        return dynamics->scale;
    }
    if (tiMotionParentScaleAssertText != 0) {
        assertFailed(tiMotionAssertParentScale, tiMotionSourceFile, 165);
    }
    return 0;
}

/* Identity, then offset, rotation Z/Y/X and
 * scale, each applied only when it is not zero (or one for scale); stores the result in `matrix`. Inline: the
 * solver below expands it, and the compiler still emits it, at the end of the file. */
inline void tiMotionTransformToMatrix(Matrix4 matrix, TiMotionTransform *transform)
{
    tiVu0LoadIdentityMatrix();
    if (transform->offset[0] != 0.0f || transform->offset[1] != 0.0f || transform->offset[2] != 0.0f) {
        xglCurMulTranslate(transform->offset);
    }
    if (transform->rotation[2] != 0.0f) {
        xglCurMulRotateZ(transform->rotation[2]);
    }
    if (transform->rotation[1] != 0.0f) {
        xglCurMulRotateY(transform->rotation[1]);
    }
    if (transform->rotation[0] != 0.0f) {
        xglCurMulRotateX(transform->rotation[0]);
    }
    if (transform->scale[0] != 1.0f || transform->scale[1] != 1.0f || transform->scale[2] != 1.0f) {
        xglCurMulScaleXYZ(transform->scale);
    }
    xglCurMatrixGet(matrix);
}

/* Translation values[0..2], applied when any is not zero. */
#define TI_MUL_TRANSLATE(values)                                                          \
    if ((values)[0] != 0.0f || (values)[1] != 0.0f || (values)[2] != 0.0f) {              \
        xglCurMulTranslate(values);                                                       \
    }
/* Undo the parent's scale when the node asks for it (flag 2). */
#define TI_MUL_PARENT_INV_SCALE(pMRPack, node)                                            \
    if ((node)->flags & TI_NODE_FLAG_INV_PARENT_SCALE) {                                                              \
        float *parentScale = tiMotionFindParentScale(pMRPack, (node)->parent);           \
        if (parentScale != 0                                                              \
            && (parentScale[0] != 1.0f || parentScale[1] != 1.0f || parentScale[2] != 1.0f)) { \
            xglCurMulInvScaleXYZ(parentScale);                                            \
        }                                                                                 \
    }
/* Rotation Z, Y then X, each applied when it is not zero. */
#define TI_MUL_ROTATE(rotation)                                                           \
    if ((rotation)[2] != 0.0f) {                                                          \
        xglCurMulRotateZ((rotation)[2]);                                                  \
    }                                                                                     \
    if ((rotation)[1] != 0.0f) {                                                          \
        xglCurMulRotateY((rotation)[1]);                                                  \
    }                                                                                     \
    if ((rotation)[0] != 0.0f) {                                                          \
        xglCurMulRotateX((rotation)[0]);                                                  \
    }
/* Scale values[6..8]: a non-unit scale marks the package (0x20) and is applied; the node keeps it. */
#define TI_MUL_SCALE(pMRPack, values, node)                                               \
    if ((values)[6] != 1.0f || (values)[7] != 1.0f || (values)[8] != 1.0f) {              \
        tiWordSetBits(&(pMRPack)->state_a.flags, TI_MR_STATE_FLAG_SCALED);                                           \
        xglCurMulScaleXYZ(&(values)[6]);                                                  \
    }                                                                                     \
    (node)->scale[0] = (values)[6];                                                       \
    (node)->scale[1] = (values)[7];                                                       \
    (node)->scale[2] = (values)[8];

/* Evaluates one locator into `matrix`. */
static inline void tiMotionSolveLocator(TiMRPackage *pMRPack, Matrix4 matrix, TiHrcLocator *locator,
                                        unsigned char **ppCursor)
{
    float values[9];

    xacEvalTransLocator(values, locator, ppCursor, pMRPack->state_a.position);
    tiVu0LoadIdentityMatrix();
    TI_MUL_TRANSLATE(values);
    TI_MUL_PARENT_INV_SCALE(pMRPack, locator);
    TI_MUL_ROTATE(&values[3]);
    TI_MUL_SCALE(pMRPack, values, locator);
    xglCurMatrixGet(matrix);
}

/* Evaluates one joint into `matrix`; the joint's rotation scale scales the animated rotation. */
static inline void tiMotionSolveJoint(TiMRPackage *pMRPack, Matrix4 matrix, TiHrcJoint *joint,
                                      unsigned char **ppCursor)
{
    float values[9];
    unsigned int k;

    xacEvalTransJoint(values, joint, ppCursor, pMRPack->state_a.position);
    if (joint->rotationScale != 1.0f) {
        for (k = 3; k < 6; k++) {
            values[k] *= joint->rotationScale;
        }
    }
    tiVu0LoadIdentityMatrix();
    TI_MUL_TRANSLATE(values);
    TI_MUL_PARENT_INV_SCALE(pMRPack, joint);
    TI_MUL_ROTATE(joint->orient);
    TI_MUL_ROTATE(&values[3]);
    TI_MUL_SCALE(pMRPack, values, joint);
    xglCurMatrixGet(matrix);
}

/* Evaluates one dynamics node into `matrix`. */
static inline void tiMotionSolveDynamics(TiMRPackage *pMRPack, Matrix4 matrix, TiHrcDynamics *dynamics,
                                         unsigned char **ppCursor)
{
    float values[9];

    xacEvalTransDynamics(values, dynamics, ppCursor, pMRPack->state_a.position);
    tiVu0LoadIdentityMatrix();
    TI_MUL_TRANSLATE(values);
    TI_MUL_PARENT_INV_SCALE(pMRPack, dynamics);
    TI_MUL_ROTATE(dynamics->orient);
    TI_MUL_ROTATE(&values[3]);
    TI_MUL_SCALE(pMRPack, values, dynamics);
    xglCurMatrixGet(matrix);
}

/* Same as tiMotionSolveDynamics for animation data older than version 1.3. */
static inline void tiMotionSolveDynamicsOld(TiMRPackage *pMRPack, Matrix4 matrix, TiHrcDynamics *dynamics,
                                            unsigned char **ppCursor)
{
    float values[9];

    xacEvalTransDynamicsNoWeight(values, dynamics, ppCursor, pMRPack->state_a.position);
    tiVu0LoadIdentityMatrix();
    TI_MUL_TRANSLATE(values);
    TI_MUL_PARENT_INV_SCALE(pMRPack, dynamics);
    TI_MUL_ROTATE(dynamics->orient);
    TI_MUL_ROTATE(&values[3]);
    TI_MUL_SCALE(pMRPack, values, dynamics);
    xglCurMatrixGet(matrix);
}

/* For animation data older than version 1.4 (tiMRPackageUpdate picks it). Evaluates the animation of every node of the
 * hierarchy and stores its local matrix in pMRPack->localMat, in node order: the two locators,
 * the joints, two further joint tables, the dynamics nodes and the static nodes. */
void tiMotionSolveLocalMatricesOld(TiMRPackage *pMRPack)
{
    unsigned char *cursor = 0;
    unsigned char **ppCursor;
    TiHrcData *hrc;
    TiHrcLocator *locators;
    TiHrcJoint *joints;
    TiHrcJoint *joint2;
    TiHrcJoint *joint3;
    TiHrcDynamics *dynamics;
    TiHrcStatic *statics;
    TiOffsetEntry *anim;
    unsigned int nodeIndex;
    unsigned int i;
    int vectorBytes;
    unsigned char *vectors; /* original temporary: removing it changes the address add */

    hrc = pMRPack->hrcData;
    locators = hrc->locators;
    joints = hrc->joints;
    joint2 = (TiHrcJoint *) ((unsigned char *) hrc + hrc->joint2Offset);
    joint3 = (TiHrcJoint *) ((unsigned char *) hrc + hrc->joint3Offset);
    dynamics = (TiHrcDynamics *) ((unsigned char *) hrc + hrc->dynamicsOffset);
    statics = (TiHrcStatic *) ((unsigned char *) hrc + hrc->staticOffset);
    anim = pMRPack->entryA;
    if (pMRPack->localMat == 0) {
        assertFailed(tiMotionAssertPLocalMat, tiMotionSourceFile, 0x1EE);
    }
    if (pMRPack->hrcData == 0) {
        assertFailed(tiMotionAssertPHrcData, tiMotionSourceFile, 0x1EF);
    }

    ppCursor = &cursor;
    nodeIndex = 0;
    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.first.jointOffset;
    } else {
        cursor = 0;
    }
    for (i = 0; i < TI_LOCATOR_COUNT; i++, nodeIndex++) {
        tiMotionSolveLocator(pMRPack, pMRPack->localMat[nodeIndex], &locators[i], ppCursor);
    }
    pMRPack->localMat[1][3][0] *= pMRPack->scale[0];
    pMRPack->localMat[1][3][1] *= pMRPack->scale[1];
    pMRPack->localMat[1][3][2] *= pMRPack->scale[2];

    for (i = 0; i < pMRPack->hrcData->jointCount - TI_LOCATOR_COUNT; i++, nodeIndex++) {
        if (cursor != 0 && i >= anim->tables.first.jointCount - TI_LOCATOR_COUNT) {
            cursor = 0;
        }
        tiMotionSolveJoint(pMRPack, pMRPack->localMat[nodeIndex], &joints[i], ppCursor);
    }

    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.first.joint2Offset;
    } else {
        cursor = 0;
    }
    for (i = 0; i < pMRPack->hrcData->joint2Count; i++, nodeIndex++) {
        if (cursor != 0 && i >= anim->tables.first.joint2Count) {
            cursor = 0;
        }
        tiMotionSolveJoint(pMRPack, pMRPack->localMat[nodeIndex], &joint2[i], ppCursor);
    }

    *ppCursor = 0;
    for (i = 0; i < pMRPack->hrcData->joint3Count; i++, nodeIndex++) {
        tiMotionSolveJoint(pMRPack, pMRPack->localMat[nodeIndex], &joint3[i], ppCursor);
    }

    nodeIndex += pMRPack->hrcData->skipCount;
    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.first.dynamicsOffset;
    } else {
        cursor = 0;
    }
    for (i = 0; i < pMRPack->hrcData->dynamicsCount; i++) {
        vectorBytes = (dynamics->sphereCount + dynamics->matrixCount + dynamics->capsuleCount) * 16;
        if (cursor != 0 && i >= anim->tables.first.dynamicsCount) {
            cursor = 0;
        }
        if (anim != 0) {
            if (anim->version >= 1.3f) {
                tiMotionSolveDynamics(pMRPack, pMRPack->localMat[nodeIndex], dynamics, ppCursor);
            } else {
                dynamics->weight = 0.5f;
                tiMotionSolveDynamicsOld(pMRPack, pMRPack->localMat[nodeIndex], dynamics, ppCursor);
            }
        } else {
            tiMotionSolveDynamics(pMRPack, pMRPack->localMat[nodeIndex], dynamics, ppCursor);
        }
        nodeIndex++;
        vectors = (unsigned char *) dynamics->entries;
        dynamics = (TiHrcDynamics *) (vectors + vectorBytes);
    }

    for (i = 0; i < pMRPack->hrcData->staticCount; i++, nodeIndex++) {
        tiMotionTransformToMatrix(pMRPack->localMat[nodeIndex], &statics[i].transform);
    }
}


/* Same walk as tiMotionSolveLocalMatricesOld for animation data of the second format: the third joint table is
 * animated too, every dynamics node uses the current solver, and the static nodes are solved only
 * when the hierarchy has a static table. */
void tiMotionSolveLocalMatrices(TiMRPackage *pMRPack)
{
    unsigned char *cursor = 0;
    unsigned char **ppCursor;
    TiHrcData *hrc;
    TiHrcLocator *locators;
    TiHrcJoint *joints;
    TiHrcJoint *joint2;
    TiHrcJoint *joint3;
    TiHrcDynamics *dynamics;
    TiHrcStatic *statics;
    TiOffsetEntry *anim;
    unsigned int nodeIndex;
    unsigned int i;
    int vectorBytes;
    unsigned char *vectors; /* original temporary: removing it changes the address add */

    hrc = pMRPack->hrcData;
    locators = hrc->locators;
    joints = hrc->joints;
    joint2 = (TiHrcJoint *) ((unsigned char *) hrc + hrc->joint2Offset);
    joint3 = (TiHrcJoint *) ((unsigned char *) hrc + hrc->joint3Offset);
    dynamics = (TiHrcDynamics *) ((unsigned char *) hrc + hrc->dynamicsOffset);
    statics = (TiHrcStatic *) ((unsigned char *) hrc + hrc->staticOffset);
    anim = pMRPack->entryA;
    if (pMRPack->localMat == 0) {
        assertFailed(tiMotionAssertPLocalMat, tiMotionSourceFile, 0x244);
    }
    if (pMRPack->hrcData == 0) {
        assertFailed(tiMotionAssertPHrcData, tiMotionSourceFile, 0x245);
    }

    ppCursor = &cursor;
    nodeIndex = 0;
    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.second.jointOffset;
    } else {
        cursor = 0;
    }
    for (i = 0; i < TI_LOCATOR_COUNT; i++, nodeIndex++) {
        tiMotionSolveLocator(pMRPack, pMRPack->localMat[nodeIndex], &locators[i], ppCursor);
    }
    pMRPack->localMat[1][3][0] *= pMRPack->scale[0];
    pMRPack->localMat[1][3][1] *= pMRPack->scale[1];
    pMRPack->localMat[1][3][2] *= pMRPack->scale[2];

    for (i = 0; i < pMRPack->hrcData->jointCount - TI_LOCATOR_COUNT; i++, nodeIndex++) {
        if (cursor != 0 && i >= anim->tables.second.jointCount - TI_LOCATOR_COUNT) {
            cursor = 0;
        }
        tiMotionSolveJoint(pMRPack, pMRPack->localMat[nodeIndex], &joints[i], ppCursor);
    }

    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.second.joint2Offset;
    } else {
        cursor = 0;
    }
    for (i = 0; i < pMRPack->hrcData->joint2Count; i++, nodeIndex++) {
        if (cursor != 0 && i >= anim->tables.second.joint2Count) {
            cursor = 0;
        }
        tiMotionSolveJoint(pMRPack, pMRPack->localMat[nodeIndex], &joint2[i], ppCursor);
    }

    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.second.joint3Offset;
    } else {
        cursor = 0;
    }
    for (i = 0; i < pMRPack->hrcData->joint3Count; i++, nodeIndex++) {
        if (cursor != 0 && i >= anim->tables.second.joint3Count) {
            cursor = 0;
        }
        tiMotionSolveJoint(pMRPack, pMRPack->localMat[nodeIndex], &joint3[i], ppCursor);
    }

    nodeIndex += pMRPack->hrcData->skipCount;
    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.second.dynamicsOffset;
    } else {
        cursor = 0;
    }
    for (i = 0; i < pMRPack->hrcData->dynamicsCount; i++) {
        vectorBytes = (dynamics->sphereCount + dynamics->matrixCount + dynamics->capsuleCount) * 16;
        if (cursor != 0 && i >= anim->tables.second.dynamicsCount) {
            cursor = 0;
        }
        tiMotionSolveDynamics(pMRPack, pMRPack->localMat[nodeIndex], dynamics, ppCursor);
        nodeIndex++;
        vectors = (unsigned char *) dynamics->entries;
        dynamics = (TiHrcDynamics *) (vectors + vectorBytes);
    }

    if (pMRPack->hrcData->staticOffset != 0) {
        for (i = 0; i < pMRPack->hrcData->staticCount; i++, nodeIndex++) {
            tiMotionTransformToMatrix(pMRPack->localMat[nodeIndex], &statics[i].transform);
        }
    }
}


/* Solves the skip nodes of the hierarchy: each node of the table at hrc->skipOffset gets the local matrix
 * after the joint tables, with its parent scale, orientation, angle limits and scale. */
void tiMotionSolveSkipNodesOld(TiMRPackage *pMRPack)
{
    TiHrcData *hrc;
    TiOffsetEntry *anim;
    TiHrcSkip *skips;
    TiHrcSkip *node;
    unsigned char *cursor = 0;
    unsigned char **ppCursor;
    unsigned int nodeIndex;
    unsigned int i;
    float values[9];
    Matrix4 *matrix;
    unsigned int matrixOffset;

    hrc = pMRPack->hrcData;
    anim = pMRPack->entryB;
    skips = (TiHrcSkip *) ((unsigned char *) hrc + hrc->skipOffset);
    if (pMRPack->localMat == 0) {
        assertFailed(tiMotionAssertPLocalMat, tiMotionSourceFile, 657);
    }
    if (pMRPack->hrcData == 0) {
        assertFailed(tiMotionAssertPHrcData, tiMotionSourceFile, 658);
    }

    nodeIndex = pMRPack->hrcData->jointCount + pMRPack->hrcData->joint2Count + pMRPack->hrcData->joint3Count;
    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.second.oldSkipOffset;
    } else {
        cursor = 0;
    }
    if (pMRPack->hrcData->skipOffset != 0) {
        {
        for (i = 0; i < pMRPack->hrcData->skipCount; i++, nodeIndex++) {
            ppCursor = &cursor;
            if (cursor != 0 && i >= anim->tables.second.skipCount) {
                cursor = 0;
            }
            /* Address by byte offset (nodeIndex << 6): the indexed form changes the code. */
            matrixOffset = nodeIndex << 6;
            matrix = (Matrix4 *) (matrixOffset + (unsigned int) pMRPack->localMat);
            xacEvalTransFace(values, &skips[i], ppCursor, pMRPack->state_b.position);
            tiVu0LoadIdentityMatrix();
            node = &skips[i];
            if (node->translationDefault[0] != 0.0f || node->translationDefault[1] != 0.0f || node->translationDefault[2] != 0.0f) {
                xglCurMulTranslate(values);
            }
            TI_MUL_PARENT_INV_SCALE(pMRPack, node);
            TI_MUL_ROTATE(node->orient);
            TI_MUL_ROTATE(&values[3]);
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_X_MIN) {
                if (values[3] < node->limit[0]) {
                    values[3] = node->limit[0];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_X_MAX) {
                if (node->limit[1] < values[3]) {
                    values[3] = node->limit[1];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_Y_MIN) {
                if (values[4] < node->limit[2]) {
                    values[4] = node->limit[2];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_Y_MAX) {
                if (node->limit[3] < values[4]) {
                    values[4] = node->limit[3];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_Z_MIN) {
                if (values[5] < node->limit[4]) {
                    values[5] = node->limit[4];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_Z_MAX) {
                if (node->limit[5] < values[5]) {
                    values[5] = node->limit[5];
                }
            }
            if (node->scaleDefault[0] != 1.0f || node->scaleDefault[1] != 1.0f
                || node->scaleDefault[2] != 1.0f) {
                tiWordSetBits(&pMRPack->state_b.flags, TI_MR_STATE_FLAG_SCALED);
                xglCurMulScaleXYZ(&values[6]);
            }
            node->scale[0] = values[6];
            node->scale[1] = values[7];
            node->scale[2] = values[8];
            xglCurMatrixGet(*matrix);
        }
    }
    }
}

void tiMotionSolveSkipNodes(TiMRPackage *pMRPack)
{
    TiHrcData *hrc;
    TiOffsetEntry *anim;
    TiHrcSkip *skips;
    TiHrcSkip *node;
    unsigned char *cursor = 0;
    unsigned char **ppCursor;
    unsigned int nodeIndex;
    unsigned int i;
    float values[9];
    Matrix4 *matrix;
    unsigned int matrixOffset;

    hrc = pMRPack->hrcData;
    anim = pMRPack->entryB;
    skips = (TiHrcSkip *) ((unsigned char *) hrc + hrc->skipOffset);
    if (pMRPack->localMat == 0) {
        assertFailed(tiMotionAssertPLocalMat, tiMotionSourceFile, 686);
    }
    if (pMRPack->hrcData == 0) {
        assertFailed(tiMotionAssertPHrcData, tiMotionSourceFile, 687);
    }

    nodeIndex = pMRPack->hrcData->jointCount + pMRPack->hrcData->joint2Count + pMRPack->hrcData->joint3Count;
    if (anim != 0) {
        cursor = (unsigned char *) anim + anim->tables.second.skipOffset;
    } else {
        cursor = 0;
    }
    if (pMRPack->hrcData->skipOffset != 0) {
        {
        for (i = 0; i < pMRPack->hrcData->skipCount; i++, nodeIndex++) {
            ppCursor = &cursor;
            if (cursor != 0 && i >= anim->tables.second.skipCount) {
                cursor = 0;
            }
            /* Address by byte offset (nodeIndex << 6): the indexed form changes the code. */
            matrixOffset = nodeIndex << 6;
            matrix = (Matrix4 *) (matrixOffset + (unsigned int) pMRPack->localMat);
            xacEvalTransFace(values, &skips[i], ppCursor, pMRPack->state_b.position);
            tiVu0LoadIdentityMatrix();
            node = &skips[i];
            if (node->translationDefault[0] != 0.0f || node->translationDefault[1] != 0.0f || node->translationDefault[2] != 0.0f) {
                xglCurMulTranslate(values);
            }
            TI_MUL_PARENT_INV_SCALE(pMRPack, node);
            TI_MUL_ROTATE(node->orient);
            TI_MUL_ROTATE(&values[3]);
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_X_MIN) {
                if (values[3] < node->limit[0]) {
                    values[3] = node->limit[0];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_X_MAX) {
                if (node->limit[1] < values[3]) {
                    values[3] = node->limit[1];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_Y_MIN) {
                if (values[4] < node->limit[2]) {
                    values[4] = node->limit[2];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_Y_MAX) {
                if (node->limit[3] < values[4]) {
                    values[4] = node->limit[3];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_Z_MIN) {
                if (values[5] < node->limit[4]) {
                    values[5] = node->limit[4];
                }
            }
            if (node->limitFlags & TI_SKIP_LIMIT_ROT_Z_MAX) {
                if (node->limit[5] < values[5]) {
                    values[5] = node->limit[5];
                }
            }
            if (node->scaleDefault[0] != 1.0f || node->scaleDefault[1] != 1.0f
                || node->scaleDefault[2] != 1.0f) {
                tiWordSetBits(&pMRPack->state_b.flags, TI_MR_STATE_FLAG_SCALED);
                xglCurMulScaleXYZ(&values[6]);
            }
            node->scale[0] = values[6];
            node->scale[1] = values[7];
            node->scale[2] = values[8];
            xglCurMatrixGet(*matrix);
        }
    }
    }
}


/* Chains the dynamics records of the hierarchy: each record takes the world matrix of its parent (or the fixed matrix when the parent is negative), multiplies in its local matrix and stores the result in globalMat. A nonzero weight copies the translation of that world matrix into dynMat. */
void tiMotionChainDynamics(TiMRPackage *pMRPack, TiHrcDynamics *dynamics)
{
    TiHrcData *hrc;
    TiHrcDynamics *record;
    unsigned int nodeIndex;
    unsigned int dynamicsIndex;
    int parent;
    int leading;

    hrc = pMRPack->hrcData;
    leading = hrc->jointCount + hrc->skipCount;
    nodeIndex = leading + (hrc->joint3Count + hrc->joint2Count);
    dynamicsIndex = 0;
    if (hrc->dynamicsCount != 0) {
        do {
            record = dynamics;
            parent = record->parent;
            dynamics = tiDynamicsNodeSolveNext(pMRPack->localMat[nodeIndex], pMRPack, record, &pMRPack->dynMat[dynamicsIndex]);
            if (parent >= 0) {
                xglCurMatrixSet(pMRPack->globalMat[parent]);
            } else {
                tiVu0LoadIdentityMatrix();
            }
            xglCurMatrixMul(pMRPack->localMat[nodeIndex]);
            xglCurMatrixGet(pMRPack->globalMat[nodeIndex]);
            if (record->weight != 0.0f) {
                pMRPack->dynMat[dynamicsIndex].position.vector.x = pMRPack->globalMat[nodeIndex][3][0];
                pMRPack->dynMat[dynamicsIndex].position.vector.y = pMRPack->globalMat[nodeIndex][3][1];
                pMRPack->dynMat[dynamicsIndex].position.vector.z = pMRPack->globalMat[nodeIndex][3][2];
            }
            nodeIndex++;
            dynamicsIndex++;
        } while (dynamicsIndex < pMRPack->hrcData->dynamicsCount);
    }
    tiDynamicsSolveCellPairs(pMRPack);
}

extern const unsigned char tiMotionAssertPGlobalMat[];

/* Chains one node: its world matrix is its parent's world matrix (or the fixed matrix for a root) times its local matrix. */
static inline void tiMotionChainNode(TiMRPackage *pMRPack, int parent, unsigned int nodeIndex)
{
    if (parent >= 0) {
        xglCurMatrixSet(pMRPack->globalMat[parent]);
    } else {
        tiVu0LoadIdentityMatrix();
    }
    xglCurMatrixMul(pMRPack->localMat[nodeIndex]);
    xglCurMatrixGet(pMRPack->globalMat[nodeIndex]);
}

/* Chains the world matrices of every node group, then the dynamics, and finishes the collision and blend passes. */
void tiMotionUpdateGlobal(TiMRPackage *pMRPack)
{
    TiHrcData *hrc;
    TiHrcLocator *locators;
    TiHrcJoint *joints;
    TiHrcJoint *joint2;
    TiHrcJoint *joint3;
    TiHrcSkip *skips;
    TiHrcDynamics *dynamics;
    TiHrcStatic *statics;
    unsigned int nodeIndex;
    unsigned int i;

    hrc = pMRPack->hrcData;
    locators = hrc->locators;
    joints = hrc->joints;
    joint2 = (TiHrcJoint *) ((unsigned char *) hrc + hrc->joint2Offset);
    joint3 = (TiHrcJoint *) ((unsigned char *) hrc + hrc->joint3Offset);
    skips = (TiHrcSkip *) ((unsigned char *) hrc + hrc->skipOffset);
    dynamics = (TiHrcDynamics *) ((unsigned char *) hrc + hrc->dynamicsOffset);
    statics = (TiHrcStatic *) ((unsigned char *) hrc + hrc->staticOffset);
    if (pMRPack->globalMat == 0) {
        assertFailed(tiMotionAssertPGlobalMat, tiMotionSourceFile, 0x2F1);
    }
    if (pMRPack->hrcData == 0) {
        assertFailed(tiMotionAssertPHrcData, tiMotionSourceFile, 0x2F2);
    }

    for (i = 0, nodeIndex = 0; i < TI_LOCATOR_COUNT; i++, nodeIndex++) {
        tiMotionChainNode(pMRPack, locators[i].parent, nodeIndex);
    }
    for (i = 0; i < pMRPack->hrcData->jointCount - TI_LOCATOR_COUNT; i++, nodeIndex++) {
        tiMotionChainNode(pMRPack, joints[i].parent, nodeIndex);
        if (nodeIndex == pMRPack->targetNode) {
            /* The target is the package's flags word (+0x10 overlays TiMotionTarget): a cast, no code. */
            tiMotionAimTarget(pMRPack->globalMat[nodeIndex], pMRPack->placeMat, &pMRPack->state_a, joints[i].translationDefault);
        }
    }
    for (i = 0; i < pMRPack->hrcData->joint2Count; i++, nodeIndex++) {
        tiMotionChainNode(pMRPack, joint2[i].parent, nodeIndex);
    }
    for (i = 0; i < pMRPack->hrcData->joint3Count; i++, nodeIndex++) {
        tiMotionChainNode(pMRPack, joint3[i].parent, nodeIndex);
    }
    for (i = 0; i < pMRPack->hrcData->skipCount; i++, nodeIndex++) {
        tiMotionChainNode(pMRPack, skips[i].parent, nodeIndex);
    }
    nodeIndex += pMRPack->hrcData->dynamicsCount;
    for (i = 0; i < pMRPack->hrcData->staticCount; i++, nodeIndex++) {
        tiMotionChainNode(pMRPack, statics[i].transform.parent, nodeIndex);
    }
    tiMtnBlendUpdateGlobal(pMRPack);
    tiDynamicsCollTransformVectors(pMRPack);
    tiDynamicsCollBuildInverseMatrices(pMRPack);
    tiDynamicsCollBuildWideInverseMatrices(pMRPack);
    tiMotionChainDynamics(pMRPack, dynamics);
}

extern const unsigned char tiMotionAssertPMRPack[];
extern const unsigned char tiMotionAssertPVisible[];

/* Sets or clears one visibility bit per curve of the animation's visibility table (+0x40, count +0x20): a set flag from
 * xacFindVisibilityKey clears the bit. pVisible is the actor's byte array of 32-bit visibility words. Animation data of the second format; version 1.4 or later only. */
void tiMotionUpdateVisibility(TiMRPackage *pMRPack, unsigned char *pVisible)
{
    TiOffsetEntry *anim;
    XacCurveKeyTable *table;
    int shown;
    int curveIndex;

    if (pMRPack == 0) {
        assertFailed(tiMotionAssertPMRPack, tiMotionSourceFile, 852);
    }
    if (pVisible == 0) {
        assertFailed(tiMotionAssertPVisible, tiMotionSourceFile, 853);
    }
    anim = pMRPack->entryA;
    if (anim == 0 || anim->version < 1.4f) {
        return;
    }
    curveIndex = 0;
    table = (XacCurveKeyTable *) ((unsigned char *) anim + anim->tables.second.visibilityOffset);
    if (anim->tables.second.visibilityCount != 0) {
        do {
            /* The next table follows the keys of this one. */
            table = (XacCurveKeyTable *) xacFindVisibilityKey(&shown, table, pMRPack->state_a.position);
            if (shown != 0) {
                /* Word access through the byte pointer: an int array index changes the code. */
                *(int *) (pVisible + ((curveIndex >> 5) << 2)) &= ~(1 << (curveIndex & 0x1F));
            } else {
                *(int *) (pVisible + ((curveIndex >> 5) << 2)) |= 1 << (curveIndex & 0x1F);
            }
            curveIndex++;
        /* Reloads the count through pMRPack (anim->tables.second.visibilityCount changes the code). */
        } while (curveIndex < pMRPack->entryA->tables.second.visibilityCount);
    }
}

typedef struct TiMotionOffsetBlock {
    unsigned char unmodeled_00[0x30];
    int joint_header_offset;
} TiMotionOffsetBlock;


/* Thin wrapper: the joint header sits at block + block->joint_header_offset. */
void tiMotionSampleJoint(float *values, TiMotionOffsetBlock *block, float value)
{
    xacEvalJointCurves9(values, (unsigned char *) block + block->joint_header_offset, value);
}

/* Out-of-line copy of tiMotionSolveLocator: evaluates one locator into `matrix`. */
void tiMotionSolveLocatorOutOfLine(Matrix4 matrix, TiMRPackage *pMRPack, TiHrcLocator *locator, unsigned char **ppCursor)
{
    float values[9];

    xacEvalTransLocator(values, locator, ppCursor, pMRPack->state_a.position);
    tiVu0LoadIdentityMatrix();
    TI_MUL_TRANSLATE(values);
    TI_MUL_PARENT_INV_SCALE(pMRPack, locator);
    TI_MUL_ROTATE(&values[3]);
    TI_MUL_SCALE(pMRPack, values, locator);
    xglCurMatrixGet(matrix);
}

/* Out-of-line copy of tiMotionSolveJoint: evaluates one joint into `matrix`. */
void tiMotionSolveJointOutOfLine(Matrix4 matrix, TiMRPackage *pMRPack, TiHrcJoint *joint, unsigned char **ppCursor)
{
    float values[9];
    unsigned int k;

    xacEvalTransJoint(values, joint, ppCursor, pMRPack->state_a.position);
    if (joint->rotationScale != 1.0f) {
        for (k = 3; k < 6; k++) {
            values[k] *= joint->rotationScale;
        }
    }
    tiVu0LoadIdentityMatrix();
    TI_MUL_TRANSLATE(values);
    TI_MUL_PARENT_INV_SCALE(pMRPack, joint);
    TI_MUL_ROTATE(joint->orient);
    TI_MUL_ROTATE(&values[3]);
    TI_MUL_SCALE(pMRPack, values, joint);
    xglCurMatrixGet(matrix);
}

/* Evaluates one skip (face) node into `matrix`: the per-node body of the skip loops above, with the face limits. */
void tiMotionSolveFace(Matrix4 matrix, TiMRPackage *pMRPack, TiHrcSkip *face, unsigned char **ppCursor)
{
    float values[9];

    xacEvalTransFace(values, face, ppCursor, pMRPack->state_b.position);
    tiVu0LoadIdentityMatrix();
    if (face->translationDefault[0] != 0.0f || face->translationDefault[1] != 0.0f || face->translationDefault[2] != 0.0f) {
        xglCurMulTranslate(values);
    }
    TI_MUL_PARENT_INV_SCALE(pMRPack, face);
    TI_MUL_ROTATE(face->orient);
    TI_MUL_ROTATE(&values[3]);
    if ((face->limitFlags & TI_SKIP_LIMIT_ROT_X_MIN) != 0) {
        if (values[3] < face->limit[0]) {
            values[3] = face->limit[0];
        }
    }
    if ((face->limitFlags & TI_SKIP_LIMIT_ROT_X_MAX) != 0) {
        if (face->limit[1] < values[3]) {
            values[3] = face->limit[1];
        }
    }
    if ((face->limitFlags & TI_SKIP_LIMIT_ROT_Y_MIN) != 0) {
        if (values[4] < face->limit[2]) {
            values[4] = face->limit[2];
        }
    }
    if ((face->limitFlags & TI_SKIP_LIMIT_ROT_Y_MAX) != 0) {
        if (face->limit[3] < values[4]) {
            values[4] = face->limit[3];
        }
    }
    if ((face->limitFlags & TI_SKIP_LIMIT_ROT_Z_MIN) != 0) {
        if (values[5] < face->limit[4]) {
            values[5] = face->limit[4];
        }
    }
    if ((face->limitFlags & TI_SKIP_LIMIT_ROT_Z_MAX) != 0) {
        if (face->limit[5] < values[5]) {
            values[5] = face->limit[5];
        }
    }
    if (face->scaleDefault[0] != 1.0f || face->scaleDefault[1] != 1.0f || face->scaleDefault[2] != 1.0f) {
        tiWordSetBits(&pMRPack->state_b.flags, TI_MR_STATE_FLAG_SCALED);
        xglCurMulScaleXYZ(&values[6]);
    }
    face->scale[0] = values[6];
    face->scale[1] = values[7];
    face->scale[2] = values[8];
    xglCurMatrixGet(matrix);
}

void tiMotionSolveDynamicsOutOfLine(Matrix4 matrix, TiMRPackage *pMRPack, TiHrcDynamics *dynamics, unsigned char **ppCursor)
{
    float values[9];

    xacEvalTransDynamics(values, dynamics, ppCursor, pMRPack->state_a.position);
    tiVu0LoadIdentityMatrix();
    TI_MUL_TRANSLATE(values);
    TI_MUL_PARENT_INV_SCALE(pMRPack, dynamics);
    TI_MUL_ROTATE(dynamics->orient);
    TI_MUL_ROTATE(&values[3]);
    TI_MUL_SCALE(pMRPack, values, dynamics);
    xglCurMatrixGet(matrix);
}

void tiMotionSolveDynamicsOldOutOfLine(Matrix4 matrix, TiMRPackage *pMRPack, TiHrcDynamics *dynamics, unsigned char **ppCursor)
{
    float values[9];

    dynamics->weight = 0.5f;
    xacEvalTransDynamicsNoWeight(values, dynamics, ppCursor, pMRPack->state_a.position);
    tiVu0LoadIdentityMatrix();
    TI_MUL_TRANSLATE(values);
    TI_MUL_PARENT_INV_SCALE(pMRPack, dynamics);
    TI_MUL_ROTATE(dynamics->orient);
    TI_MUL_ROTATE(&values[3]);
    TI_MUL_SCALE(pMRPack, values, dynamics);
    xglCurMatrixGet(matrix);
}

/* Assert texts of the solver (the original .rodata strings, in address order). */
const unsigned char tiMotionAssertPPlaceMat[16] = "pPlaceMat";
const unsigned char tiMotionSourceFile[24] = "../TiMotionSolver.euc.c";
const unsigned char tiMotionParentScaleAssertText[32] = "can not found parent scale";
const unsigned char tiMotionAssertParentScale[32] = "!\"can not found parent scale\"";
const unsigned char tiMotionAssertPLocalMat[24] = "pMRPack->pLocalMat";
const unsigned char tiMotionAssertPHrcData[24] = "pMRPack->pHrcData";
const unsigned char tiMotionAssertPGlobalMat[24] = "pMRPack->pGlobalMat";
const unsigned char tiMotionAssertPMRPack[8] = "pMRPack";
const unsigned char tiMotionAssertPVisible[16] = "pVisible";
