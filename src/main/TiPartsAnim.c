#include "common.h"
#include "shared.h"
#include "main/assert_fail.h"
#include "main/xgl_vector_ops.h"
#include "main/xgl_cur_mul.h"
#include "main/xgl_cur_matrix.h"
#include "main/TiXacAnimCurve.h"
#include "main/TiMRPackage.h"
#include "main/TiPartsAnim.h"



/* Samples `count` joints into matrices, reading each joint's curves from the parts animation data of pPack. The nine curve values
 * of a joint are: offset (0-2), rotation X, Y, Z (3-5) and pivot (6-8). */
void tiPartsAnimSamplePose(Matrix4 *matrices, TiMRPackage *pPack, int count)
{
    float values[9];
    float negPivot[3];
    u8 *cursor;
    u8 **ppJointHeader;
    int i;

    if (pPack->pamData == 0) {
        assertFailed("pPack->pPamData", "../TiPartsAnim.euc.c", 58);
    }
    cursor = pPack->pamData->curves;
    for (i = 0; i < count; i++) {
        ppJointHeader = &cursor;
        xacReadJointCurves9(values, ppJointHeader, pPack->state_a.position);
        tiVu0LoadIdentityMatrix();
        if (values[0] != 0.0f || values[1] != 0.0f || values[2] != 0.0f) {
            xglCurMulTranslate(values);
        }
        if (values[6] != 0.0f || values[7] != 0.0f || values[8] != 0.0f) {
            negPivot[0] = -values[6];
            negPivot[1] = -values[7];
            negPivot[2] = -values[8];
            xglCurMulTranslate(&values[6]);
            if (values[5] != 0.0f) {
                xglCurMulRotateZ(values[5]);
            }
            if (values[4] != 0.0f) {
                xglCurMulRotateY(values[4]);
            }
            if (values[3] != 0.0f) {
                xglCurMulRotateX(values[3]);
            }
            xglCurMulTranslate(negPivot);
        } else {
            if (values[5] != 0.0f) {
                xglCurMulRotateZ(values[5]);
            }
            if (values[4] != 0.0f) {
                xglCurMulRotateY(values[4]);
            }
            if (values[3] != 0.0f) {
                xglCurMulRotateX(values[3]);
            }
        }
        xglCurMatrixGet(matrices[i]);
    }
}

/* Samples one joint: reads nine curve values into offset, rotation and pivot, then builds the matrix
 * (translate by offset, pivot around the rotation when the pivot is nonzero, rotations Z, Y, X). */
void tiPartsAnimSampleJoint(Matrix4 matrix, u8 **ppJointHeader, float curveParam)
{
    float values[9];
    float negPivot[3];

    xacReadJointCurves9(values, ppJointHeader, curveParam);
    tiVu0LoadIdentityMatrix();
    if (values[0] != 0.0f || values[1] != 0.0f || values[2] != 0.0f) {
        xglCurMulTranslate(values);
    }
    if (values[6] != 0.0f || values[7] != 0.0f || values[8] != 0.0f) {
        negPivot[0] = -values[6];
        negPivot[1] = -values[7];
        negPivot[2] = -values[8];
        xglCurMulTranslate(&values[6]);
        if (values[5] != 0.0f) {
            xglCurMulRotateZ(values[5]);
        }
        if (values[4] != 0.0f) {
            xglCurMulRotateY(values[4]);
        }
        if (values[3] != 0.0f) {
            xglCurMulRotateX(values[3]);
        }
        xglCurMulTranslate(negPivot);
    } else {
        if (values[5] != 0.0f) {
            xglCurMulRotateZ(values[5]);
        }
        if (values[4] != 0.0f) {
            xglCurMulRotateY(values[4]);
        }
        if (values[3] != 0.0f) {
            xglCurMulRotateX(values[3]);
        }
    }
    xglCurMatrixGet(matrix);
}

/* Writes the matrix of node `nodeKey` into outMatrix: the old format indexes the table directly, the newer one
 * searches it by id. useBaseMatrix selects the start matrix: the handle's matrix of the node (set) or the fixed one.
 * Returns 0 when the node is absent. */
int tiMRPackageGetNodeMatrix(TiMRPackage *handle, int nodeKey, Matrix4 outMatrix, int useBaseMatrix)
{
    TiNodeTable *table;
    TiNodeSlot *slots;
    TiNodeKey *keys;
    TiNodeKey *key;
    unsigned int nodeIndex;

    table = handle->nodeTable;
    if (table == 0) {
        return 0;
    }
    if (table->threshold < 1.1f) {
        slots = table->slots;
        if ((unsigned int) nodeKey >= table->count) {
            return 0;
        }
        nodeIndex = (unsigned int) nodeKey;
        if (useBaseMatrix != 0) {
            xglCurMatrixSet(handle->globalMat[slots[nodeIndex].matrixIndex]);
        } else {
            tiVu0LoadIdentityMatrix();
        }
        if (slots[nodeIndex].rotation[2] != 0.0f) {
            xglCurMulRotateZ(slots[nodeIndex].rotation[2]);
        }
        if (slots[nodeIndex].rotation[1] != 0.0f) {
            xglCurMulRotateY(slots[nodeIndex].rotation[1]);
        }
        if (slots[nodeIndex].rotation[0] != 0.0f) {
            xglCurMulRotateX(slots[nodeIndex].rotation[0]);
        }
        if (slots[nodeIndex].offset[0] != 0.0f || slots[nodeIndex].offset[1] != 0.0f || slots[nodeIndex].offset[2] != 0.0f) {
            xglCurMulTranslate(slots[nodeIndex].offset);
        }
        xglCurMatrixGet(outMatrix);
    } else {
        keys = (TiNodeKey *) table->slots;
        key = 0;
        for (nodeIndex = 0; nodeIndex < handle->nodeTable->count; nodeIndex++) {
            if (keys[nodeIndex].id == nodeKey) {
                key = &keys[nodeIndex];
                break;
            }
        }
        if (key == 0) {
            return 0;
        }
        if (useBaseMatrix != 0) {
            xglCurMatrixSet(handle->globalMat[key->matrixIndex]);
        } else {
            tiVu0LoadIdentityMatrix();
        }
        if (key->rotation[2] != 0.0f) {
            xglCurMulRotateZ(key->rotation[2]);
        }
        if (key->rotation[1] != 0.0f) {
            xglCurMulRotateY(key->rotation[1]);
        }
        if (key->rotation[0] != 0.0f) {
            xglCurMulRotateX(key->rotation[0]);
        }
        if (key->offset[0] != 0.0f || key->offset[1] != 0.0f || key->offset[2] != 0.0f) {
            xglCurMulTranslate(key->offset);
        }
        xglCurMatrixGet(outMatrix);
    }
    return 1;
}
