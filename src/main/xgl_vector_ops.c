#include "common.h"
#include "main/xgl_vector_ops.h"
#include "main/xenoActor.h"
#include "main/TiMRPackage.h"
#include "shared.h"
#include "quadword.h"

/* One dynamics record of the HRC data: 0xB0 bytes, then 16 bytes per vector counted by vectorCount (the sphere,
 * matrix and capsule counts of TiHrcDynamics). */
typedef struct TiHrcDynamicsRecord {
    unsigned char unmodeled_00[0x1C];
    int vectorCount[3];
    unsigned char unmodeled_28[0x80 - 0x28];
    Vector4 rows[3];          /* +0x80..+0xB0: copied into each node */
} TiHrcDynamicsRecord;

/* Builds the node list of the package from its HRC data; nodes is the output buffer, stored at +0xEC (dynMat). */
void tiMRPackageFillNodes(TiMRPackage *pMRPackage, TiDynamicsOut *nodes) {
    TiHrcData *hrc;
    TiHrcDynamicsRecord *dynamics;
    TiDynamicsOut *node;
    unsigned int i;
    Vector4 *vectorBlock;

    pMRPackage->dynMat = nodes;
    if (nodes != 0) {
        hrc = pMRPackage->hrcData;
        i = 0;
        dynamics = (TiHrcDynamicsRecord *) ((unsigned char *) hrc + hrc->dynamicsOffset);
        if (hrc->dynamicsCount != 0) {
            node = nodes;
            do {
                QuadwordCopy(&node->position, &dynamics->rows[1]);
                QuadwordCopy(node->previous_position, &dynamics->rows[0]);
                QuadwordCopy(&node->accumulator, &dynamics->rows[2]);
                i++;
                node->node = (TiHrcDynamics *) dynamics;
                node->gust = 1.0f;
                node++;
                vectorBlock = (Vector4 *) (dynamics + 1);
                dynamics = (TiHrcDynamicsRecord *) (vectorBlock
                    + (dynamics->vectorCount[0] + dynamics->vectorCount[1] + dynamics->vectorCount[2]));
            } while (i < pMRPackage->hrcData->dynamicsCount);
        }
    }
}

#include "vu0_macro.h"

/* Converts the source value (range[0]) of a motion state to the float at +8 of the state. */
void XenoActorConvertSourceRecord(XenoMotionState *record) {
    record->frame = (f32) record->range[0];
}

/* Copies one vector through VU0 register vf2. */
void xglVectorCopy_00292E28(Vector4 *destination, const Vector4 *source) {
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("sqc2 vf2, 0(%0)" : : "r"(destination) : "memory");
}

/* Adds the xyz parts of two vectors; w is taken from the first operand. */
void xglVectorAddXYZ_00292E38(Vector4 *destination, const Vector4 *first, const Vector4 *second) {
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(first) : "memory");
    __asm__ __volatile__("lqc2 vf3, 0(%0)" : : "r"(second) : "memory");
    __asm__ __volatile__("vadd.xyz vf4, vf2, vf3" : : : "memory");
    __asm__ __volatile__("vmove.w vf4, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf4, 0(%0)" : : "r"(destination) : "memory");
}

/* Subtracts the xyz parts of two vectors; w is taken from the minuend. */
void xglVectorSubXYZ_00292E58(Vector4 *destination, const Vector4 *minuend, const Vector4 *subtrahend) {
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(minuend) : "memory");
    __asm__ __volatile__("lqc2 vf3, 0(%0)" : : "r"(subtrahend) : "memory");
    __asm__ __volatile__("vsub.xyz vf4, vf2, vf3" : : : "memory");
    __asm__ __volatile__("vmove.w vf4, vf2" : : : "memory");
    __asm__ __volatile__("sqc2 vf4, 0(%0)" : : "r"(destination) : "memory");
}

/* Component-wise product of two vectors. */
void xglVectorMul_00292E78(Vector4 *destination, const Vector4 *first, const Vector4 *second) {
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(first) : "memory");
    __asm__ __volatile__("lqc2 vf3, 0(%0)" : : "r"(second) : "memory");
    __asm__ __volatile__("vmul.xyzw vf4, vf2, vf3" : : : "memory");
    __asm__ __volatile__("sqc2 vf4, 0(%0)" : : "r"(destination) : "memory");
}

void xglVectorNormal_00292E90(Vector4 *destination, const Vector4 *source) {
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("vmul.xyz vf3, vf2, vf2" : : : "memory");
    __asm__ __volatile__("vaddy.x vf3, vf3, vf3y" : : : "memory");
    __asm__ __volatile__("vaddz.x vf3, vf3, vf3z" : : : "memory");
    __asm__ __volatile__("vrsqrt Q, vf0w, vf3x" : : : "memory");
    __asm__ __volatile__("vwaitq" : : : "memory");
    __asm__ __volatile__("vmulq.xyz vf2, vf2, Q" : : : "memory");
    __asm__ __volatile__("sqc2 vf2, 0(%0)" : : "r"(destination) : "memory");
}

/* Copies four vectors (a 4x4 matrix) through vf2..vf5. */
void xglMatrixCopy_00292EB8(Vector4 *destination, const Vector4 *source) {
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("lqc2 vf3, 16(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("lqc2 vf4, 32(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("lqc2 vf5, 48(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("sqc2 vf2, 0(%0)" : : "r"(destination) : "memory");
    __asm__ __volatile__("sqc2 vf3, 16(%0)" : : "r"(destination) : "memory");
    __asm__ __volatile__("sqc2 vf4, 32(%0)" : : "r"(destination) : "memory");
    __asm__ __volatile__("sqc2 vf5, 48(%0)" : : "r"(destination) : "memory");
}

/* A four-row matrix in data; its rows load into the resident VU0 matrix. */
extern Matrix4 tiVu0IdentityMatrix;

/* Loads the data matrix tiVu0IdentityMatrix into the VU0 current matrix (vf27..vf30). */
void tiVu0LoadIdentityMatrix(void) {
    Vu0SetCurrentMatrix(tiVu0IdentityMatrix);
}

Matrix4 tiVu0IdentityMatrix = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};
