#include "common.h"
#include "shared.h"
#include "main/TiMRPackage.h"
#include "main/xgl_vector_ops.h"
#include "main/assert_fail.h"
#include "main/TiMotionSolver.h"
#include "main/xgl_studio.h"

/* Flag bits of TiMRState.flags that the advance and selector functions set. */
#define TI_MR_STATE_FLAG_OUT_OF_RANGE 0x8  /* set while the position is past its range, cleared inside it */
#define TI_MR_STATE_FLAG_ENTRY_BOUND 0x40  /* set when an entry is bound to the state */

extern const char tiMRPackageAssertExpr[];
extern const char tiMRPackageSourceFile[];
extern const char tiMRPackageStateAssertExpr[];
extern const char tiMRPackagePackAssertExpr[];
void *memset(void *destination, int value, unsigned int count);

/* Sets the bits of a flag word. */
void tiWordSetBits(int *word, int bits) {
    *word |= bits;
}

/* Clears the bits of a flag word. */
void tiWordClearBits(int *word, int bits) {
    *word &= ~bits;
}

/* Zeroes one word; the init code passes the flag word of each state. */
void tiWordZero(int *word) {
    *word = 0;
}


/* Package initializer: clears everything, sets speed 1.0 and position 0.0 on both states, vector (1,1,1). */
void tiMRPackageInit(TiMRPackage *pMRPackage) {
    TiMRState *pStateA;
    TiMRState *pStateB;

    pStateA = &pMRPackage->state_a;
    pStateB = &pMRPackage->state_b;
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 38);
    }
    memset(pMRPackage, 0, sizeof(TiMRPackage));
    tiWordZero(&pStateA->flags);
    tiWordZero(&pStateB->flags);
    tiMRStateSetPosition(pStateA, 0.0f);
    tiMRStateSetPosition(pStateB, 0.0f);
    tiMRStateSetBlendFrames(pStateA, 0.0f);
    tiMRStateSetBlendFrames(pStateB, 0.0f);
    tiMRStateSetSpeed(pStateA, 1.0f);
    tiMRStateSetSpeed(pStateB, 1.0f);
    tiMRPackageSetSolverPasses(pMRPackage, 1);
    tiMRPackageSetVector(pMRPackage, 1.0f, 1.0f, 1.0f);
    tiMRPackageSetHrcData(pMRPackage, 0);
    tiMRPackageSetAnimData(pMRPackage, 0);
    tiMRPackageSetLocalMat(pMRPackage, 0);
    tiMRPackageSetGlobalMat(pMRPackage, 0);
    tiMRPackageSetBlendMat(pMRPackage, 0);
    tiMRPackageSetPartsMat(pMRPackage, 0);
    tiMRPackageSetClampRange(pMRPackage, 0);
    tiMRPackageSetEntryTableC(pMRPackage, 0);
    tiMRPackageFillNodes(pMRPackage, 0);
    pMRPackage->targetNode = 0x1D;
}

void tiMRPackageSetLocalMat(TiMRPackage *pMRPackage, Matrix4 *value) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 70);
    }
    pMRPackage->localMat = value;
}

void tiMRPackageSetGlobalMat(TiMRPackage *pMRPackage, Matrix4 *value) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 80);
    }
    pMRPackage->globalMat = value;
}

void tiMRPackageSetBlendMat(TiMRPackage *pMRPackage, Matrix4 *value) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 90);
    }
    pMRPackage->blendMat = value;
}

void tiMRPackageSetPartsMat(TiMRPackage *pMRPackage, Matrix4 *value) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 100);
    }
    pMRPackage->partsMat = value;
}

/* Stores the integer range of a state (min at +0x10, max at +0x14); the advance functions loop over it. */
void tiMRStateSetRange(TiMRState *pState, int rangeMin, int rangeMax) {
    pState->rangeMin = rangeMin;
    pState->rangeMax = rangeMax;
}

/*
 * Binds entry A: a null entry clears +0xC0; an entry whose float range at +8
 * and +C is valid (min <= max) is stored at +0xC0 and its range copied into
 * state A (tiMRStateSetRange). Same validity test as tiMRPackageIsAnimEntryValid.
 */
void tiMRPackageSetAnimData(TiMRPackage *pMRPackage, TiOffsetEntry *entry) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 117);
    }
    if (entry != 0) {
        if (entry->rangeMin <= entry->rangeMax) {
            pMRPackage->entryA = entry;
            tiMRStateSetRange(&pMRPackage->state_a, (int) entry->rangeMin, (int) entry->rangeMax);
        }
    } else {
        pMRPackage->entryA = 0;
    }
}

void tiMRPackageSetAnimTable(TiMRPackage *pMRPackage, OffsetTable *table) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 137);
    }
    pMRPackage->entryTableA = table;
}

/* Binds entry B the same way as tiMRPackageSetAnimData, into +0xC8 and state B at +0x60. */
void tiMRPackageSetFaceAnimData(TiMRPackage *pMRPackage, TiOffsetEntry *entry) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 147);
    }
    if (entry != 0) {
        if (entry->rangeMin <= entry->rangeMax) {
            pMRPackage->entryB = entry;
            tiMRStateSetRange(&pMRPackage->state_b, (int) entry->rangeMin, (int) entry->rangeMax);
        }
    } else {
        pMRPackage->entryB = 0;
    }
}

/*
 * Returns 1 when entry `index` of offset table A exists and its range is valid
 * (float at +8 <= float at +C); 0 otherwise. The second argument is not read.
 */
int tiMRPackageIsAnimEntryValid(TiMRPackage *pMRPackage, int unused, unsigned int index) {
    OffsetTable *table;
    TiOffsetEntry *entry;

    table = pMRPackage->entryTableA;
    if (table == 0) {
        return 0;
    }
    if (index >= table->count) {
        return 0;
    }
    /* entryOffsets is a byte offset from the table (the form index[table->entryOffsets] is kept: table->entryOffsets[index] changed the code). */
    entry = (TiOffsetEntry *) ((unsigned char *) table + index[table->entryOffsets]);
    if (entry->rangeMin <= entry->rangeMax) {
        return 1;
    }
    return 0;
}

void tiMRPackageSetFaceAnimTable(TiMRPackage *pMRPackage, OffsetTable *table) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 191);
    }
    pMRPackage->entryTableB = table;
}

void tiMRPackageSetHrcData(TiMRPackage *pMRPackage, TiHrcData *value) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 201);
    }
    pMRPackage->hrcData = value;
}

void tiMRPackageSetClampRange(TiMRPackage *pMRPackage, TiPamData *pam) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 211);
    }
    pMRPackage->pamData = pam;
}

void tiMRPackageSetEntryTableC(TiMRPackage *pMRPackage, TiNodeTable *value) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 221);
    }
    pMRPackage->nodeTable = value;
}

/* Stores the rate of a state (speed, +4). */
void tiMRStateSetSpeed(TiMRState *pState, float value) {
    if (pState == 0) {
        assertFailed(tiMRPackageStateAssertExpr, tiMRPackageSourceFile, 231);
    }
    pState->speed = value;
}

/* Stores the position of a state (+8), the value the advance functions accumulate. */
void tiMRStateSetPosition(TiMRState *pState, float value) {
    if (pState == 0) {
        assertFailed(tiMRPackageStateAssertExpr, tiMRPackageSourceFile, 241);
    }
    pState->position = value;
}

void tiMRStateSetBlendFrames(TiMRState *pState, float frames) {
    if (pState == 0) {
        assertFailed(tiMRPackageStateAssertExpr, tiMRPackageSourceFile, 251);
    }
    pState->blendFrames = frames;
}

/* Stores the first word of the package (set to 1 by the initializer and by the per-frame update). */
void tiMRPackageSetSolverPasses(TiMRPackage *pMRPackage, int value) {
    if (pMRPackage == 0) {
        assertFailed(tiMRPackagePackAssertExpr, tiMRPackageSourceFile, 261);
    }
    pMRPackage->solverPasses = value;
}

/* Sets the package scale vector to one value. */
void tiMRPackageSetVectorUniform(TiMRPackage *pMRPackage, float value) {
    pMRPackage->scale[2] = value;
    pMRPackage->scale[1] = value;
    pMRPackage->scale[0] = value;
}

/* Sets the package scale vector. */
void tiMRPackageSetVector(TiMRPackage *pMRPackage, float x, float y, float z) {
    pMRPackage->scale[0] = x;
    pMRPackage->scale[1] = y;
    pMRPackage->scale[2] = z;
}

#include "main/xgl_camera.h"


/*
 * Advances state A: position += speed times the global step; a position past
 * rangeMax wraps to rangeMin and a position below rangeMin jumps to rangeMax,
 * each setting flag 0x8 and clearing 0x2; the limit shorts of the parts data at +0xD4 then sets or
 * clears 0x8 again. Called from XenoActorUpdateTree.
 */
void tiMRPackageAdvanceAnim(TiMRPackage *pMRPackage) {
    XglCamera *camera;
    TiPamData *limits;

    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 287);
    }
    camera = xglStudioGetCamera2(0);
    pMRPackage->state_a.position = pMRPackage->state_a.position + pMRPackage->state_a.speed * camera->unknown_5f4;
    if (pMRPackage->entryA != 0) {
        if ((float) pMRPackage->state_a.rangeMax <= pMRPackage->state_a.position) {
            pMRPackage->state_a.position = (float) pMRPackage->state_a.rangeMin;
            tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
            tiWordClearBits(&pMRPackage->state_a.flags, 0x2);
        } else if (pMRPackage->state_a.position < (float) pMRPackage->state_a.rangeMin) {
            pMRPackage->state_a.position = (float) pMRPackage->state_a.rangeMax;
            tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
            tiWordClearBits(&pMRPackage->state_a.flags, 0x2);
        } else {
            tiWordClearBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        }
    }
    limits = pMRPackage->pamData;
    if (limits == 0) {
        return;
    }
    if ((float) limits->upperLimit <= pMRPackage->state_a.position) {
        pMRPackage->state_a.position = (float) limits->lowerLimit;
        tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        return;
    }
    if (pMRPackage->state_a.position < (float) limits->lowerLimit) {
        pMRPackage->state_a.position = (float) limits->upperLimit;
        tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        return;
    }
    tiWordClearBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
}

/* Advances state A like tiMRPackageAdvanceAnim; clears flag 0x2 only when it is set. */
void tiMRPackageAdvanceAnimUnwrapped(TiMRPackage *pMRPackage) {
    XglCamera *camera;
    TiPamData *limits;

    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 344);
    }
    camera = xglStudioGetCamera2(0);
    pMRPackage->state_a.position = pMRPackage->state_a.position + pMRPackage->state_a.speed * camera->unknown_5f4;
    if (pMRPackage->entryA != 0) {
        if ((float) pMRPackage->state_a.rangeMax <= pMRPackage->state_a.position
            || pMRPackage->state_a.position < (float) pMRPackage->state_a.rangeMin) {
            tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
            if (pMRPackage->state_a.flags & 0x2) {
                tiWordClearBits(&pMRPackage->state_a.flags, 0x2);
            }
        } else {
            tiWordClearBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        }
    }
    limits = pMRPackage->pamData;
    if (limits == 0) {
        return;
    }
    if ((float) limits->upperLimit <= pMRPackage->state_a.position) {
        tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        return;
    }
    if (pMRPackage->state_a.position < (float) limits->lowerLimit) {
        tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        return;
    }
    tiWordClearBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
}

/* Advances state B: position += speed times the global step, with the same wrap at its range. */
void tiMRPackageAdvanceFaceAnim(TiMRPackage *pMRPackage) {
    XglCamera *camera;

    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 398);
    }
    camera = xglStudioGetCamera2(0);
    pMRPackage->state_b.position = pMRPackage->state_b.position + pMRPackage->state_b.speed * camera->unknown_5f4;
    if (pMRPackage->entryB == 0) {
        return;
    }
    if ((float) pMRPackage->state_b.rangeMax <= pMRPackage->state_b.position) {
        pMRPackage->state_b.position = (float) pMRPackage->state_b.rangeMin;
        tiWordSetBits(&pMRPackage->state_b.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        tiWordClearBits(&pMRPackage->state_b.flags, 0x2);
        return;
    }
    if (pMRPackage->state_b.position < (float) pMRPackage->state_b.rangeMin) {
        pMRPackage->state_b.position = (float) pMRPackage->state_b.rangeMax;
        tiWordSetBits(&pMRPackage->state_b.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        tiWordClearBits(&pMRPackage->state_b.flags, 0x2);
        return;
    }
    tiWordClearBits(&pMRPackage->state_b.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
}

/* Advances state B like tiMRPackageAdvanceFaceAnim; clears flag 0x2 only when it is set. */
void tiMRPackageAdvanceFaceAnimUnwrapped(TiMRPackage *pMRPackage) {
    XglCamera *camera;

    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 434);
    }
    camera = xglStudioGetCamera2(0);
    pMRPackage->state_b.position = pMRPackage->state_b.position + pMRPackage->state_b.speed * camera->unknown_5f4;
    if (pMRPackage->entryB == 0) {
        return;
    }
    if ((float) pMRPackage->state_b.rangeMax <= pMRPackage->state_b.position) {
        tiWordSetBits(&pMRPackage->state_b.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        if (pMRPackage->state_b.flags & 0x2) {
            tiWordClearBits(&pMRPackage->state_b.flags, 0x2);
        }
        return;
    }
    if (pMRPackage->state_b.position < (float) pMRPackage->state_b.rangeMin) {
        tiWordSetBits(&pMRPackage->state_b.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
        if (pMRPackage->state_b.flags & 0x2) {
            tiWordClearBits(&pMRPackage->state_b.flags, 0x2);
        }
        return;
    }
    tiWordClearBits(&pMRPackage->state_b.flags, TI_MR_STATE_FLAG_OUT_OF_RANGE);
}

extern const char tiMRPackageGlobalMatAssertExpr[];
extern const char tiMRPackageLocalMatAssertExpr[];
extern const char tiMRPackageHrcVersionAssertExpr[];
extern const char tiMRPackageAnimVersionAssertExpr[];
extern const char tiMRPackageFaceVersionAssertExpr[];

/*
 * Per-frame update of the package. Checks the matrices (pLocalMat +0xDC,
 * pGlobalMat +0xE0), clears flag 0x20 on both states, checks the version
 * floats of pHrcData (+0xD0, xhr), pAnimData (+0xC0, xac) and pFaceAnimData
 * (+0xC8, fac), runs the animation updates and ends by setting solverPasses to 1.
 */
void tiMRPackageUpdate(TiMRPackage *pMRPackage) {
    TiHrcData *block;
    TiOffsetEntry *entry;
    int i;

    if (pMRPackage == 0) {
        assertFailed(tiMRPackageAssertExpr, tiMRPackageSourceFile, 471);
    }
    if (pMRPackage->globalMat == 0) {
        assertFailed(tiMRPackageGlobalMatAssertExpr, tiMRPackageSourceFile, 472);
    }
    if (pMRPackage->localMat == 0) {
        assertFailed(tiMRPackageLocalMatAssertExpr, tiMRPackageSourceFile, 473);
    }
    tiWordClearBits(&pMRPackage->state_a.flags, 0x20);
    tiWordClearBits(&pMRPackage->state_b.flags, 0x20);

    block = pMRPackage->hrcData;
    if (!(2.3f <= block->version)) {
        assertFailed(tiMRPackageHrcVersionAssertExpr, tiMRPackageSourceFile, 479);
    }
    entry = pMRPackage->entryA;
    if (entry != 0) {
        if (!(1.2f <= entry->version)) {
            assertFailed(tiMRPackageAnimVersionAssertExpr, tiMRPackageSourceFile, 480);
        }
    }
    entry = pMRPackage->entryB;
    if (entry != 0) {
        if (!(1.2f <= entry->version)) {
            assertFailed(tiMRPackageFaceVersionAssertExpr, tiMRPackageSourceFile, 481);
        }
    }

    entry = pMRPackage->entryA;
    if (entry != 0) {
        if (!(1.4f <= entry->version)) {
            tiMotionSolveLocalMatricesOld(pMRPackage);
        } else {
            tiMotionSolveLocalMatrices(pMRPackage);
        }
    } else {
        tiMotionSolveLocalMatrices(pMRPackage);
    }

    entry = pMRPackage->entryB;
    if (entry != 0) {
        if (!(1.4f <= entry->version)) {
            tiMotionSolveSkipNodesOld(pMRPackage);
        } else {
            tiMotionSolveSkipNodes(pMRPackage);
        }
    } else {
        tiMotionSolveSkipNodes(pMRPackage);
    }

    for (i = 0; i < pMRPackage->solverPasses; i++) {
        tiMtnBlendUpdateLocal(pMRPackage);
        tiMotionUpdateGlobal(pMRPackage);
    }
    tiMRPackageSetSolverPasses(pMRPackage, 1);
}

/* Binds entry `index` of table A to state A and sets flag 0x40 of state A. */
void tiMRPackageSelectAnimEntry(TiMRPackage *pMRPackage, unsigned int index) {
    OffsetTable *table;
    int entryOffset;

    table = pMRPackage->entryTableA;
    if (table == 0) {
        return;
    }
    if (index < table->count) {
        entryOffset = index[table->entryOffsets];
        tiMRPackageSetAnimData(pMRPackage, (TiOffsetEntry *) ((unsigned char *) table + entryOffset));
        tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_ENTRY_BOUND);
    }
}

/* Stores table A (the argument), then binds entry `index` of it to state A as tiMRPackageSelectAnimEntry. */
void tiMRPackageSetAnimTableAndEntry(TiMRPackage *pMRPackage, OffsetTable *table, unsigned int index) {
    int entryOffset;

    if (table == 0) {
        return;
    }
    if (index < table->count) {
        entryOffset = index[table->entryOffsets];
        tiMRPackageSetAnimTable(pMRPackage, table);
        tiMRPackageSetAnimData(pMRPackage, (TiOffsetEntry *) ((unsigned char *) table + entryOffset));
        tiWordSetBits(&pMRPackage->state_a.flags, TI_MR_STATE_FLAG_ENTRY_BOUND);
    }
}

/* Binds entry `index` of table B to state B and sets flag 0x40 of state B. */
void tiMRPackageSelectFaceAnimEntry(TiMRPackage *pMRPackage, unsigned int index) {
    OffsetTable *table;
    int entryOffset;

    table = pMRPackage->entryTableB;
    if (table == 0) {
        return;
    }
    if (index < table->count) {
        entryOffset = index[table->entryOffsets];
        tiMRPackageSetFaceAnimData(pMRPackage, (TiOffsetEntry *) ((unsigned char *) table + entryOffset));
        tiWordSetBits(&pMRPackage->state_b.flags, TI_MR_STATE_FLAG_ENTRY_BOUND);
    }
}

const char tiMRPackageAssertExpr[16] = "pMRPackage";
const char tiMRPackageSourceFile[24] = "../TiMRPackage.euc.c";
const char tiMRPackageStateAssertExpr[8] = "pState";
const char tiMRPackagePackAssertExpr[8] = "pMRPack";
const char tiMRPackageGlobalMatAssertExpr[24] = "pMRPackage->pGlobalMat";
const char tiMRPackageLocalMatAssertExpr[24] = "pMRPackage->pLocalMat";
const char tiMRPackageHrcVersionAssertExpr[88] = "pMRPackage->pHrcData->fileVersion >= CURRENT_XHR_VERSION && \"illegal xhr version\"";
const char tiMRPackageAnimVersionAssertExpr[88] = "pMRPackage->pAnimData->fileVersion >= CURRENT_XAC_VERSION && \"illegal xac version\"";
const char tiMRPackageFaceVersionAssertExpr[88] = "pMRPackage->pFaceAnimData->fileVersion >= CURRENT_XAC_VERSION && \"illegal fac version\"";
