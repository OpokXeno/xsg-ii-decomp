#include "common.h"
#include "main/TiMtnBlend.h"
#include "main/TiMRPackage.h"
#include "main/ti_dynamics_node.h"
#include "main/assert_fail.h"
#include "shared.h"

#include "main/xrg_rand_int.h"
/* TiMRState.flags of a state block: 0x2 the blend window runs, 0x4 selects the local slot (clear: global), 0x40 the blend request. */
#define TI_MTN_BLEND_RUNNING 0x2
#define TI_MTN_BLEND_LOCAL 0x4
#define TI_MTN_BLEND_REQUEST 0x40

extern const char tiMtnBlendPackAssertExpr[];
extern const char tiMtnBlendSourceFile[];
extern const char tiMtnBlendPackGlobalMatAssertExpr[];
extern const char tiMtnBlendPackBlendMatAssertExpr[];
extern const char tiMtnBlendPackLocalMatAssertExpr[];

/* Swaps the global and blend matrix arrays when flag 0x4 is clear. */
void tiMtnBlendSwapGlobal(TiMRPackage *ctx, int *flags) {
    Matrix4 *swap;

    if (!(*flags & TI_MTN_BLEND_LOCAL)) {
        if (ctx == 0) {
            assertFailed(tiMtnBlendPackAssertExpr, tiMtnBlendSourceFile, 14);
        }
        if (ctx->globalMat == 0) {
            assertFailed(tiMtnBlendPackGlobalMatAssertExpr, tiMtnBlendSourceFile, 15);
        }
        if (ctx->blendMat == 0) {
            assertFailed(tiMtnBlendPackBlendMatAssertExpr, tiMtnBlendSourceFile, 16);
        }
        swap = ctx->globalMat;
        ctx->globalMat = ctx->blendMat;
        ctx->blendMat = swap;
    }
}

/* Swaps the local and blend matrix arrays when flag 0x4 is set. */
void tiMtnBlendSwapLocal(TiMRPackage *ctx, int *flags) {
    Matrix4 *swap;

    if (*flags & TI_MTN_BLEND_LOCAL) {
        if (ctx == 0) {
            assertFailed(tiMtnBlendPackAssertExpr, tiMtnBlendSourceFile, 34);
        }
        if (ctx->localMat == 0) {
            assertFailed(tiMtnBlendPackLocalMatAssertExpr, tiMtnBlendSourceFile, 35);
        }
        if (ctx->blendMat == 0) {
            assertFailed(tiMtnBlendPackBlendMatAssertExpr, tiMtnBlendSourceFile, 36);
        }
        swap = ctx->localMat;
        ctx->localMat = ctx->blendMat;
        ctx->blendMat = swap;
    }
}

/* Blends two 4x4 matrices: dst = a * (1 - t) + b * t. The elements are addressed by byte offset: the typed [row][col] form changed the code. */
void tiMtnBlendLerpMatrix(Matrix4 dst, Matrix4 a, Matrix4 b, float t) {
    int byteOffset;
    int row;
    int col;
    float *dstElem;
    float *aElem;
    float *bElem;

    for (row = 0; row < 4; row++) {
        for (col = 0; col < 4; col++) {
            byteOffset = row * 16 + col * 4;
            dstElem = (float *)((char *)dst + byteOffset);
            aElem = (float *)((char *)a + byteOffset);
            bElem = (float *)((char *)b + byteOffset);
            *dstElem = *aElem * (1.0f - t) + *bElem * t;
        }
    }
}

extern const char tiMtnBlendDestAssertExpr[];
extern const char tiMtnBlendMat1AssertExpr[];
extern const char tiMtnBlendMat2AssertExpr[];

/* Blends two matrices: the rotation by quaternion slerp, the translation by lerp. */
void tiMtnBlendMatrix(Matrix4 *dst, Matrix4 *matA, Matrix4 *matB, float t) {
    float oneMinusT = 1.0f - t;
    Vector4 startQuat;
    Vector4 endQuat;
    Vector4 blendQuat;
    float translation[3];
    float rowLength[3];
    int row;
    float x;
    float y;
    float z;
    float length;

    if (dst == 0) {
        assertFailed(tiMtnBlendDestAssertExpr, tiMtnBlendSourceFile, 71);
    }
    if (matA == 0) {
        assertFailed(tiMtnBlendMat1AssertExpr, tiMtnBlendSourceFile, 72);
    }
    if (matB == 0) {
        assertFailed(tiMtnBlendMat2AssertExpr, tiMtnBlendSourceFile, 73);
    }

    translation[0] = (*matA)[3][0] * oneMinusT + (*matB)[3][0] * t;
    translation[1] = (*matA)[3][1] * oneMinusT + (*matB)[3][1] * t;
    translation[2] = (*matA)[3][2] * oneMinusT + (*matB)[3][2] * t;

    for (row = 0; row < 3; row++) {
        x = (*matA)[row][0] * oneMinusT + (*matB)[row][0] * t;
        y = (*matA)[row][1] * oneMinusT + (*matB)[row][1] * t;
        z = (*matA)[row][2] * oneMinusT + (*matB)[row][2] * t;
        length = __builtin_sqrtf(x * x + y * y + z * z);
        rowLength[row] = length;
        if (length <= 1.0e-6f) {
            return;
        }
    }

    XrgMatrixToQuaternion(&startQuat.x, *matA);
    XrgMatrixToQuaternion(&endQuat.x, *matB);
    XrgQuaternionSlerp(&blendQuat, &startQuat, &endQuat, t);
    XrgQuaternionToMatrix(*dst, &blendQuat);
    (*dst)[3][0] = translation[0];
    (*dst)[3][1] = translation[1];
    (*dst)[3][2] = translation[2];
}

void tiMtnBlendUpdateGlobal(TiMRPackage *ctx) {
    float progress;
    float remaining;

    if (ctx == 0) {
        assertFailed(tiMtnBlendPackAssertExpr, tiMtnBlendSourceFile, 121);
    }
    if (ctx->entryA != 0) {
        unsigned int item;
        int matrixIndex;

        if ((ctx->state_a.flags & TI_MTN_BLEND_REQUEST) == 0) {
            tiWordClearBits(&ctx->state_a.flags, TI_MTN_BLEND_RUNNING);
        } else if ((*(long long *)&ctx->state_a.flags & (TI_MTN_BLEND_RUNNING | TI_MTN_BLEND_LOCAL)) == TI_MTN_BLEND_RUNNING) {
            remaining = ctx->state_a.position - (float)ctx->state_a.rangeMin;
            if (ctx->state_a.value == 0.0f || ctx->state_a.value <= remaining || remaining < 0.0f) {
                tiWordClearBits(&ctx->state_a.flags, TI_MTN_BLEND_RUNNING);
                tiWordClearBits(&ctx->state_a.flags, TI_MTN_BLEND_REQUEST);
                return;
            }
            if (ctx->blendMat == 0) {
                assertFailed(tiMtnBlendPackBlendMatAssertExpr, tiMtnBlendSourceFile, 145);
            }
            if (ctx->globalMat == 0) {
                assertFailed(tiMtnBlendPackGlobalMatAssertExpr, tiMtnBlendSourceFile, 146);
            }
            progress = remaining / ctx->state_a.value;
            matrixIndex = 0;
            if (progress < 0.0f) {
                progress = 0.0f;
            } else if (1.0f < progress) {
                progress = 1.0f;
            }
            for (item = 0; item < ctx->hrcData->jointCount; item++) {
                tiMtnBlendMatrix(ctx->globalMat + matrixIndex, ctx->blendMat + matrixIndex,
                                 ctx->globalMat + matrixIndex, progress);
                matrixIndex++;
            }
            for (item = 0; item < ctx->hrcData->joint2Count; item++) {
                tiMtnBlendMatrix(ctx->globalMat + matrixIndex, ctx->blendMat + matrixIndex,
                                 ctx->globalMat + matrixIndex, progress);
                matrixIndex++;
            }
            for (item = 0; item < ctx->hrcData->joint3Count; item++) {
                tiMtnBlendMatrix(ctx->globalMat + matrixIndex, ctx->blendMat + matrixIndex,
                                 ctx->globalMat + matrixIndex, progress);
                matrixIndex++;
            }
        }
    }
    if (ctx->entryB != 0) {
        unsigned int item;
        int matrixIndex;

        if ((ctx->state_b.flags & TI_MTN_BLEND_REQUEST) == 0) {
            tiWordClearBits(&ctx->state_b.flags, TI_MTN_BLEND_RUNNING);
        } else if ((*(long long *)&ctx->state_b.flags & (TI_MTN_BLEND_RUNNING | TI_MTN_BLEND_LOCAL)) == TI_MTN_BLEND_RUNNING) {
            remaining = ctx->state_b.position - (float)ctx->state_b.rangeMin;
            if (ctx->state_b.value == 0.0f || ctx->state_b.value <= remaining || remaining < 0.0f) {
                tiWordClearBits(&ctx->state_b.flags, TI_MTN_BLEND_RUNNING);
                tiWordClearBits(&ctx->state_b.flags, TI_MTN_BLEND_REQUEST);
                return;
            }
            if (ctx->blendMat == 0) {
                assertFailed(tiMtnBlendPackBlendMatAssertExpr, tiMtnBlendSourceFile, 191);
            }
            if (ctx->globalMat == 0) {
                assertFailed(tiMtnBlendPackGlobalMatAssertExpr, tiMtnBlendSourceFile, 192);
            }
            progress = remaining / ctx->state_b.value;
            matrixIndex = ctx->hrcData->jointCount + ctx->hrcData->joint2Count + ctx->hrcData->joint3Count;
            if (progress < 0.0f) {
                progress = 0.0f;
            } else if (1.0f < progress) {
                progress = 1.0f;
            }
            for (item = 0; item < ctx->hrcData->skipCount; item++) {
                tiMtnBlendMatrix(ctx->globalMat + matrixIndex, ctx->blendMat + matrixIndex,
                                 ctx->globalMat + matrixIndex, progress);
                matrixIndex++;
            }
        }
    }
}

void tiMtnBlendUpdateLocal(TiMRPackage *ctx) {
    float progress;
    float remaining;

    if (ctx == 0) {
        assertFailed(tiMtnBlendPackAssertExpr, tiMtnBlendSourceFile, 217);
    }
    if (ctx->entryA != 0) {
        unsigned int item;
        int matrixIndex;

        if ((ctx->state_a.flags & TI_MTN_BLEND_REQUEST) == 0) {
            tiWordClearBits(&ctx->state_a.flags, TI_MTN_BLEND_RUNNING);
        } else if ((*(long long *)&ctx->state_a.flags & (TI_MTN_BLEND_RUNNING | TI_MTN_BLEND_LOCAL)) == (TI_MTN_BLEND_RUNNING | TI_MTN_BLEND_LOCAL)) {
            remaining = ctx->state_a.position - (float)ctx->state_a.rangeMin;
            if (ctx->state_a.value == 0.0f || ctx->state_a.value <= remaining || remaining < 0.0f) {
                tiWordClearBits(&ctx->state_a.flags, TI_MTN_BLEND_RUNNING);
                tiWordClearBits(&ctx->state_a.flags, TI_MTN_BLEND_REQUEST);
                return;
            }
            if (ctx->blendMat == 0) {
                assertFailed(tiMtnBlendPackBlendMatAssertExpr, tiMtnBlendSourceFile, 241);
            }
            if (ctx->localMat == 0) {
                assertFailed(tiMtnBlendPackLocalMatAssertExpr, tiMtnBlendSourceFile, 242);
            }
            progress = remaining / ctx->state_a.value;
            matrixIndex = 0;
            if (progress < 0.0f) {
                progress = 0.0f;
            } else if (1.0f < progress) {
                progress = 1.0f;
            }
            for (item = 0; item < ctx->hrcData->jointCount; item++) {
                tiMtnBlendMatrix(ctx->localMat + matrixIndex, ctx->blendMat + matrixIndex,
                                 ctx->localMat + matrixIndex, progress);
                matrixIndex++;
            }
            for (item = 0; item < ctx->hrcData->joint2Count; item++) {
                tiMtnBlendMatrix(ctx->localMat + matrixIndex, ctx->blendMat + matrixIndex,
                                 ctx->localMat + matrixIndex, progress);
                matrixIndex++;
            }
            for (item = 0; item < ctx->hrcData->joint3Count; item++) {
                tiMtnBlendMatrix(ctx->localMat + matrixIndex, ctx->blendMat + matrixIndex,
                                 ctx->localMat + matrixIndex, progress);
                matrixIndex++;
            }
        }
    }
    if (ctx->entryB != 0) {
        unsigned int item;
        int matrixIndex;

        if ((ctx->state_b.flags & TI_MTN_BLEND_REQUEST) == 0) {
            tiWordClearBits(&ctx->state_b.flags, TI_MTN_BLEND_RUNNING);
        } else if ((*(long long *)&ctx->state_b.flags & (TI_MTN_BLEND_RUNNING | TI_MTN_BLEND_LOCAL)) == (TI_MTN_BLEND_RUNNING | TI_MTN_BLEND_LOCAL)) {
            remaining = ctx->state_b.position - (float)ctx->state_b.rangeMin;
            if (ctx->state_b.value == 0.0f || ctx->state_b.value <= remaining || remaining < 0.0f) {
                tiWordClearBits(&ctx->state_b.flags, TI_MTN_BLEND_RUNNING);
                tiWordClearBits(&ctx->state_b.flags, TI_MTN_BLEND_REQUEST);
                return;
            }
            if (ctx->blendMat == 0) {
                assertFailed(tiMtnBlendPackBlendMatAssertExpr, tiMtnBlendSourceFile, 287);
            }
            if (ctx->localMat == 0) {
                assertFailed(tiMtnBlendPackLocalMatAssertExpr, tiMtnBlendSourceFile, 288);
            }
            progress = remaining / ctx->state_b.value;
            matrixIndex = ctx->hrcData->jointCount + ctx->hrcData->joint2Count + ctx->hrcData->joint3Count;
            if (progress < 0.0f) {
                progress = 0.0f;
            } else if (1.0f < progress) {
                progress = 1.0f;
            }
            for (item = 0; item < ctx->hrcData->skipCount; item++) {
                tiMtnBlendMatrix(ctx->localMat + matrixIndex, ctx->blendMat + matrixIndex,
                                 ctx->localMat + matrixIndex, progress);
                matrixIndex++;
            }
        }
    }
}

/* Assert strings: each one is shared by several routines, so each is one named array. */
const char tiMtnBlendPackAssertExpr[] = "pPack";
const char tiMtnBlendSourceFile[] = "../TiMtnBlend.euc.c";
const char tiMtnBlendPackGlobalMatAssertExpr[] = "pPack->pGlobalMat";
const char tiMtnBlendPackBlendMatAssertExpr[] = "pPack->pBlendMat";
const char tiMtnBlendPackLocalMatAssertExpr[] = "pPack->pLocalMat";
const char tiMtnBlendDestAssertExpr[] = "pDest";
const char tiMtnBlendMat1AssertExpr[] = "pMat1";
const char tiMtnBlendMat2AssertExpr[] = "pMat2";
