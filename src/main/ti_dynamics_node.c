#include "common.h"
#include "main/TiDynamicsColl.h"
#include "main/ti_dynamics_node.h"
#include "main/TiGlobalColl.h"
#include "main/TiMtnBlend.h"
#include "main/TiWindBase.h"
#include "shared.h"
#include "main/xgl_2.h"

/* TiSolverNode flag bits tested by tiDynamicsNodeSolvePalette: a set bit zeroes that axis of the local position. */
#define TI_NODE_FLAG_ZERO_Y 0x4
#define TI_NODE_FLAG_ZERO_Z 0x2
/* state_a.flags bit tested by tiDynamicsCollSolveNode: clamp the body height. */
#define TI_MR_STATE_FLAG_CLAMP_HEIGHT 0x80

/* Unit quaternion (x, y, z, w) to a row-major rotation matrix; the last column is zero except m[3][3] = 1. */
void XrgQuaternionToMatrix(Matrix4 matrix, const Vector4 *quaternion) {
    matrix[0][0] = 1.0f - 2.0f * (quaternion->y * quaternion->y + quaternion->z * quaternion->z);
    matrix[0][1] = 2.0f * (quaternion->x * quaternion->y + quaternion->w * quaternion->z);
    matrix[0][2] = 2.0f * (quaternion->x * quaternion->z - quaternion->w * quaternion->y);
    matrix[0][3] = 0.0f;
    matrix[1][0] = 2.0f * (quaternion->x * quaternion->y - quaternion->w * quaternion->z);
    matrix[1][1] = 1.0f - 2.0f * (quaternion->x * quaternion->x + quaternion->z * quaternion->z);
    matrix[1][2] = 2.0f * (quaternion->y * quaternion->z + quaternion->w * quaternion->x);
    matrix[1][3] = 0.0f;
    matrix[2][0] = 2.0f * (quaternion->x * quaternion->z + quaternion->w * quaternion->y);
    matrix[2][1] = 2.0f * (quaternion->y * quaternion->z - quaternion->w * quaternion->x);
    matrix[2][2] = 1.0f - 2.0f * (quaternion->x * quaternion->x + quaternion->y * quaternion->y);
    matrix[2][3] = 0.0f;
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = 0.0f;
    matrix[3][3] = 1.0f;
}

/* Quaternion from the rotation part of a row-major 4x4 matrix (Shepperd's method). */
void XrgMatrixToQuaternion(float quat[4], Matrix4 matrix) {
    float trace;
    float s;
    float two_s;
    int i;
    int j;
    int k;
    float *slot;
    int off_i;
    int off_j;
    int off_k;
    int off_larger_diag;
    int off_ji;
    int off_ij;
    int off_ki;
    int off_ik;
    int off_jk;
    int off_kj;
    int quat_off_j;
    int quat_off_k;
    /* Byte-offset float reads kept: the typed matrix[i][j] and quat[j] forms change the code (check.py). */

    trace = matrix[0][0] + matrix[1][1] + matrix[2][2] + matrix[3][3];
    if (trace >= 1.0f) {
        s = __builtin_sqrtf(trace);
        two_s = s + s;
        quat[3] = two_s * 0.25f;
        quat[0] = (matrix[1][2] - matrix[2][1]) / two_s;
        quat[1] = (matrix[2][0] - matrix[0][2]) / two_s;
        quat[2] = (matrix[0][1] - matrix[1][0]) / two_s;
    } else {
        if (matrix[1][1] < matrix[0][0]) {
            i = 0;
        } else {
            i = 1;
        }
        off_larger_diag = 20 * i;
        if (matrix[2][2] > *(float *)((char *)matrix + off_larger_diag)) {
            i = 2;
        }
        j = (i + 1) % 3;
        k = (j + 1) % 3;
        off_i = 20 * i;
        off_j = 20 * j;
        off_k = 20 * k;
        s = __builtin_sqrtf(*(float *)((char *)matrix + off_i) - *(float *)((char *)matrix + off_j) - *(float *)((char *)matrix + off_k) + 1.0f);
        two_s = s + s;
        slot = &quat[i];
        *slot = two_s * 0.25f;
        off_ji = 16 * j + 4 * i;
        off_ij = 16 * i + 4 * j;
        quat_off_j = 4 * j;
        *(float *)((char *)quat + quat_off_j) = (*(float *)((char *)matrix + off_ji) + *(float *)((char *)matrix + off_ij)) / two_s;
        off_ki = 16 * k + 4 * i;
        off_ik = 16 * i + 4 * k;
        quat_off_k = 4 * k;
        *(float *)((char *)quat + quat_off_k) = (*(float *)((char *)matrix + off_ki) + *(float *)((char *)matrix + off_ik)) / two_s;
        off_jk = 16 * j + 4 * k;
        off_kj = 16 * k + 4 * j;
        quat[3] = (*(float *)((char *)matrix + off_jk) - *(float *)((char *)matrix + off_kj)) / two_s;
    }
}


int XrgQuaternionFromVectors(Vector4 *destination, const Vector4 *left, const Vector4 *right) {
    Vector4 cross;
    float dot;
    float length;
    double abs_length;
    double zero;

    xglVectorOuter(&cross, left, right);
    xglVectorInner(&dot, left, right);
    length = __builtin_sqrtf((dot + 1.0f) * 2.0f);
    zero = 0.0;
    abs_length = length;
    if (abs_length >= zero) {
    } else {
        abs_length = zero - abs_length;
    }
    if (abs_length < 0.01f) {
        cross.y = 1.0f;
        cross.x = 0.0f;
        cross.z = 0.0f;
        destination->y = cross.y;
        destination->x = cross.x;
        destination->z = cross.z;
        destination->w = 0.0f;
        return 0;
    }
    {
        float inverse = 1.0f / length;

        destination->w = 0.5f * length;
        destination->x = cross.x * inverse;
        destination->y = cross.y * inverse;
        destination->z = cross.z * inverse;
    }
    return 1;
}

typedef union { float value; } FloatAlias;

/* Advances one node's point: the previous position is kept in the state's first words, the
 * velocity is damped to a length of 0.2 when it is longer, then scaled by the node weight. */
void tiDynamicsNodeIntegrate(TiHrcDynamics *node, TiSolverState *state) {
    /* Union view kept: a float store through the union changes the code (check.py). */
    FloatAlias *previous = (FloatAlias *)state->previous_position;
    float length;
    float root;
    float delta;

    delta = state->velocity.vector.y - node->gravity;
    state->velocity.vector.y = delta;
    previous[0].value = state->position.vector.x;
    previous[1].value = state->position.vector.y;
    previous[2].value = state->position.vector.z;
    length = state->velocity.vector.x * state->velocity.vector.x;
    length = length + delta * delta;
    length = length + state->velocity.vector.z * state->velocity.vector.z;
    root = __builtin_sqrtf(length);
    if (root >= 0.2f) {
        float inv = 1.0f / root;

        state->velocity.vector.y = (state->velocity.vector.y * inv) * 0.2f;
        state->velocity.vector.x = (state->velocity.vector.x * inv) * 0.2f;
        state->velocity.vector.z = (state->velocity.vector.z * inv) * 0.2f;
    }
    state->position.vector.x = state->position.vector.x + state->velocity.vector.x;
    state->position.vector.y = state->position.vector.y + state->velocity.vector.y;
    state->position.vector.z = state->position.vector.z + state->velocity.vector.z;
    state->velocity.vector.x = state->velocity.vector.x * node->damping;
    state->velocity.vector.y = state->velocity.vector.y * node->damping;
    state->velocity.vector.z = state->velocity.vector.z * node->damping;
}


/* Collision pass for one node: the dynamics push-outs (sphere list, matrix list, capsule list), the
 * global collision body and, when object flag 0x80 is set, the scale clamp. Called twice per step. */
void tiDynamicsCollSolveNode(TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state) {
    tiDynamicsCollPushOutSphereList(object, node, state);
    tiDynamicsCollPushOutPlaneList(object, node, state);
    tiDynamicsCollPushOutCapsuleList(object, node, state);
    tiGlobalCollSolveBody(node, state);
    if (object->state_a.flags & TI_MR_STATE_FLAG_CLAMP_HEIGHT) {
        tiGlobalCollClampHeight(state);
    }
}


/* Pulls the node's point toward its matrix-transformed target, weighted by node->pullScale. */
void tiDynamicsNodePullToTarget(float (*matrix)[4], TiHrcDynamics *node, TiSolverState *state) {
    Vector4 transformed;

    if (node->pullScale != 0.0f) {
        xglVectorMulMat(&transformed, matrix, &node->translationDefault);
        state->velocity.vector.x += (transformed.x - state->position.vector.x) * node->pullScale;
        state->velocity.vector.y += (transformed.y - state->position.vector.y) * node->pullScale;
        state->velocity.vector.z += (transformed.z - state->position.vector.z) * node->pullScale;
    }
}


void tiDynamicsNodeConstrainNormal(float (*matrix)[4], TiHrcDynamics *node, TiSolverState *state) {
    Vector4 delta;
    Vector4 unit;
    float scaled_x;
    float scaled_y;
    float scaled_z;

    delta.x = state->position.vector.x - matrix[3][0];
    delta.y = state->position.vector.y - matrix[3][1];
    delta.z = state->position.vector.z - matrix[3][2];
    xglVectorNormal(&unit, &delta);

    scaled_x = unit.x * node->boneLength;
    state->position.vector.x = scaled_x;
    scaled_y = unit.y * node->boneLength;
    state->position.vector.y = scaled_y;
    scaled_z = unit.z * node->boneLength;
    state->position.vector.z = scaled_z;

    state->velocity.vector.x += (scaled_x - delta.x) * node->damping;
    state->velocity.vector.y += (scaled_y - delta.y) * node->damping;
    state->velocity.vector.z += (scaled_z - delta.z) * node->damping;

    state->position.vector.x = scaled_x + matrix[3][0];
    state->position.vector.y = scaled_y + matrix[3][1];
    state->position.vector.z = scaled_z + matrix[3][2];
}

/* Normalizes the xyz part of the first three rows of a matrix in place; a zero length is replaced by 1e-7. */
void tiMatrixNormalizeRows(float (*matrix)[4]) {
    int i = 0;
    float *row_z = matrix[0] + 2;
    while (i < 3) {
        float length;

        length = __builtin_sqrtf(row_z[-2] * row_z[-2] + row_z[-1] * row_z[-1] + row_z[0] * row_z[0]);
        if (length == 0.0f) {
            length = 1.0e-7f;
        }
        length = 1.0f / length;
        row_z[-2] = row_z[-2] * length;
        ((FloatAlias *)row_z)[-1].value = ((FloatAlias *)row_z)[-1].value * length;
        ((FloatAlias *)row_z)[0].value = ((FloatAlias *)row_z)[0].value * length;
        row_z = row_z + 4;
        i++;
    }
}

/* Solves one node's palette matrix: the inverse-transformed position is pushed through the matrix
 * when the node's flag bits (flags bits 1 and 2, tested together through the 64-bit header) clear v.y or v.z; the result is
 * blended with the rotation built from the normals. Runs only while blend_weight is below 1. */
void tiDynamicsNodeSolvePalette(Matrix *matrix, TiHrcDynamics *node, TiSolverState *state) {
    Matrix blended;
    Matrix rotation;
    Vector4 local_position;
    Vector4 local_direction;
    Vector4 target_direction;
    Matrix inverse;
    Vector4 align_quat;
    float weight;

    if (1.0f <= node->weight) {
        return;
    }
    xglMatrixInverse(&inverse, matrix);
    xglVectorMulMat(&local_position, inverse.rows, &state->position.vector);
    if (node->header & ((unsigned long long)(TI_NODE_FLAG_ZERO_Y | TI_NODE_FLAG_ZERO_Z) << 32)) {
        if (node->solverFlags & TI_NODE_FLAG_ZERO_Y) {
            local_position.y = 0.0f;
        }
        if (node->solverFlags & TI_NODE_FLAG_ZERO_Z) {
            local_position.z = 0.0f;
        }
        xglVectorMulMat(&state->position.vector, matrix->rows, &local_position);
    }
    xglVectorNormal(&local_direction, &local_position);
    xglVectorNormal(&target_direction, &node->translationDefault);
    XrgQuaternionFromVectors(&align_quat, &target_direction, &local_direction);
    XrgQuaternionToMatrix(rotation.rows, &align_quat);
    if (0.0f != node->weight) {
        xglMatrixMul(blended.rows, matrix->rows, rotation.rows);
        weight = node->weight;
        tiMtnBlendLerpMatrix(matrix->rows, blended.rows, matrix->rows, weight);
        tiMatrixNormalizeRows(matrix->rows);
    } else {
        xglMatrixMul(matrix->rows, matrix->rows, rotation.rows);
    }
}

/* One solver step for a node: integrate, wind, pull toward the target, collide, constrain to the
 * normal distance and collide again. The palette matrix of the node is node->parent (0x40 bytes each). */
void tiDynamicsNodeStep(TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state) {
    tiDynamicsNodeIntegrate(node, state);
    tiWindApplyAll(node, state);
    tiDynamicsNodePullToTarget(object->globalMat[node->parent], node, state);
    tiDynamicsCollSolveNode(object, node, state);
    tiDynamicsNodeConstrainNormal(object->globalMat[node->parent], node, state);
    tiDynamicsCollSolveNode(object, node, state);
}

/* Steps one node, then runs tiDynamicsNodeSolvePalette on the same palette matrix, and returns the node that follows
 * this one: the record is followed by its sphereCount + matrixCount + capsuleCount collision entries. */
TiHrcDynamics *tiDynamicsNodeSolveNext(Matrix4 unused_localMat, TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state)
{
    int entryCount;
    TiCollEntry *end;

    tiDynamicsNodeStep(object, node, state);
    tiDynamicsNodeSolvePalette((Matrix *) &object->globalMat[node->parent], node, state);
    entryCount = node->sphereCount + node->matrixCount;
    end = node->entries;
    entryCount += node->capsuleCount;

    return (TiHrcDynamics *)(end + entryCount);
}
