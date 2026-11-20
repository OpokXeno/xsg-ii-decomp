#include "common.h"
#include "main/TiDrawCollision.h"
#include "main/TiDrawJoint.h"
#include "main/TiMRPackage.h"
#include "main/TiMtnBlend.h"
#include "main/TiMotionSolver.h"
#include "main/TiPartsAnim.h"
#include "main/TiDynamicsColl.h"
#include "main/TiXacAnimCurve.h"
#include "main/nml_model_set.h"
#include "main/test_128.h"
#include "main/assert_fail.h"
#include "main/xgl_cur_matrix.h"
#include "main/xgl_cur_mul.h"
#include "main/xenoActor.h"
#include "main/act_3.h"
#include "main/xgl_2.h"
#include "main/xgl_studio.h"
#include "quadword.h"
#include "vu0_macro.h"

#include "shared.h"
#include "main/xgl_camera.h"
#include "main/pxy_packet_state.h"
#include "main/xgl_light.h"

/* The PxyHeader of a motion pack is declared in main/pxy_packet_state.h. */
#include "main/xgl_vector_ops.h"

typedef struct PxyRange {
    unsigned char unmodeled_00[0x18];
    int count;                              /* 0x18: the same word as PxyPartRange.count */
} PxyRange;

typedef struct PxyGeometry {
    unsigned char unmodeled_00[0x20];
    u32 flags;                              /* 0x20: bit 0x100000 = casts a shadow */
    unsigned char unmodeled_24[0x70 - 0x24];
} PxyGeometry;

/* XenoHrc (the motion hierarchy, actor->pHrcData) is declared in main/xenoActor.h. */

#define XenoActorPack(a) ((PxyHeader *)(a)->rsrc[XAct_Rsrc_MDL])
#define XenoActorResource(a) ((TiHrcData *)(a)->rsrc[XAct_Rsrc_XHR])
#define XenoActorSlotTable(a, slot) ((OffsetTable *)(a)->rsrc[XAct_Rsrc_XAP + (slot)])
#define XenoActorTexture(a) ((TxyBlock *)(a)->rsrc[XAct_Rsrc_TEX])

/* XenoActorSetModeInSlot reads the slot table through a pointer-typed access: the original's load of it does not wait
 * behind the stores to the actor's int fields before it, which an int rsrc[] element would force. This one site reads
 * the tables (rsrc[XAct_Rsrc_XAP + slot]) through a view of the actor. */
typedef struct XenoActorSlotView {
    unsigned char unmodeled_000[0x5EC + XAct_Rsrc_XAP * 4];
    OffsetTable *tables[4];        /* 0x5FC: slots 0..3 */
} XenoActorSlotView;

/* Bits of XenoActor.flags. */
#define XENO_FLAG_FREEZE_A 0x1           /* XenoActorUpdateTree does not advance motion A */
#define XENO_FLAG_UNWRAP_A 0x2           /* motion A plays once: it does not loop at the end of its range */
#define XENO_FLAG_FREEZE_B 0x4           /* XenoActorUpdateTree does not advance motion B */
#define XENO_FLAG_UNWRAP_B 0x8           /* motion B plays once: it does not loop at the end of its range */
#define XENO_FLAG_BLEND_SWAPPED 0x10     /* the channel blend was swapped in; cleared when the mode is re-applied */
#define XENO_FLAG_RANGE_SET 0x40         /* a play range was set (XenoActorSetPlayRange) */
#define XENO_FLAG_CHANNEL_BIT20 0x80     /* either channel flag word has bit 0x20 set */
#define XENO_FLAG_DRAW_POINTS 0x100      /* XenoActorSetFocusTarget draws the focus joint points */
#define XENO_FLAG_RESET_CHANNEL 0x200    /* XenoActorUpdateTree restarts the channel */
#define XENO_FLAG_SHIFT_JOINTS 0x400     /* set by XenoActorRequestJointShift: XenoActorUpdateTree shifts the joint rows */
#define XENO_FLAG_SKIP_DELTA 0x800       /* set by XenoActorWrapRotation: next update only snapshots the joint rows */

/* colorIndex value meaning "no color"; XenoActorInit fills the 0x10-byte block at colorIndex with it. */
#define XENO_COLOR_INDEX_NONE 0xFF

int XenoActorPoolCount = 48;
extern XenoActor *XenoActorPool;

/* Size in bytes of one pool record. */
#define XENO_ACTOR_POOL_STRIDE 0x7F0

/* XenoActorPoolGet: pool record at index (records are 0x7F0 bytes apart), or 0 outside XenoActorPoolCount. */
XenoActor *XenoActorPoolGet(int index)
{
    if (index < 0 || index >= XenoActorPoolCount) {
        return 0;
    }
    return (XenoActor *)((unsigned char *)XenoActorPool + index * XENO_ACTOR_POOL_STRIDE);
}

static u8 xenoActorPoolBuffer[97728];
/* The actor matrix heap of XenoActorInitHeap (0x38000 bytes from xenoActorHeapBuffer) runs on into
   xenoActorHeapTailBuffer: the two arrays are one block in the original layout. */
static u8 xenoActorHeapBuffer[54096];
static u8 xenoActorHeapTailBuffer[175360];
XenoActor *XenoActorPool = (XenoActor *)xenoActorPoolBuffer;
int *XenoActorActiveTable = (int *)((u8 *)xenoActorPoolBuffer + 97536);

/* XenoActorPoolInit: sets the pool base and count, builds the active table after the pool and inits every record. */
unsigned char *XenoActorPoolInit(void *data, int size) {
    unsigned char *end;
    int i;
    XenoActor *record;
    int *slot;

    XenoActorPoolCount = size;
    end = data;
    XenoActorPool = (XenoActor *)end;
    end += size * XENO_ACTOR_POOL_STRIDE;
    XenoActorActiveTable = (int *)end;
    end += size * 4;
    record = data;
    for (i = 0; i < XenoActorPoolCount; i++) {
        XenoActorInit(record++);
    }
    if (XenoActorPoolCount > 0) { slot = XenoActorActiveTable; i = XenoActorPoolCount; do { *slot++ = 0; } while (--i); }
    XenoActorInitHeap((int)xenoActorHeapBuffer, 0x38000);
    XenoActorClearHoldFlag();
    return end;
}

/* XenoActorPoolInitDefault: initializes the pool from the built-in buffer xenoActorPoolBuffer (0x30 records). */
void XenoActorPoolInitDefault(void) {
    XenoActorPoolInit(xenoActorPoolBuffer, 0x30);
}

const char xenoActorAssertFile[16] = "xenoActor.euc.c";
extern unsigned char xenoActorAssertExpr[];
int XenoActorHoldFlag = 0;
unsigned char xenoActorAssertExpr[8] = "pAct";

/* XenoActorInitHeap: initializes RG heap 5, the heap the actor matrices are allocated from, over the given memory block. */
void XenoActorInitHeap(int memory, int size) {
    InitRgHeap(InstanceOfRgHeapById(5), (void *)memory, size); /* memory is an int address in the original's prototype */
}

void *memset(void *dst, int value, unsigned int size);

/* Not in xenoActor.h: other files declare these two with different types. */

/* XenoActorInit: resets one record to its default state (slotIndex and mode -1, default channel values). */
void XenoActorInit(XenoActor *actor) {
    TiMRPackage *handle = XenoActorMotionPack(actor);

    if (actor == 0) {
        return;
    }
    memset(actor, 0, sizeof(XenoActor));
    actor->itemCursorA = -1;
    actor->activeItem = -1;
    actor->itemCursorB = -1;
    actor->drawParam = 0.2f;
    actor->shadowType = 0;
    actor->radius = 0.3f;
    actor->requested_mode = -1;
    actor->slotIndex = -1;
    actor->shadowHeightC = 0.5f;
    actor->shadowHeightD = -1.0f;
    actor->position.w = 1.0f;
    actor->vector70.w = 1.0f;
    actor->alpha = 1.0f;
    actor->scale.w = 1.0f;
    actor->scale.z = 1.0f;
    actor->scale.y = 1.0f;
    actor->scale.x = 1.0f;
    actor->renderValue2 = 1.0f;
    actor->sweepSpeed = 1.0f;
    actor->clipPlaneValue2 = 1.0f;
    actor->clipPlaneValue3 = 1.0f;
    actor->modeValue4 = 1.0f;
    actor->modeValue3 = 0.0f;
    actor->clipPlaneValue = 0.0f;
    actor->sweepPhase = 0.0f;
    actor->clipPlaneCount = 0;
    actor->shadowHeightA = 0.0f;
    actor->shadowHeightB = 0.0f;
    actor->modeValue2 = 0.0f;
    actor->modeValue5 = 0.0f;
    actor->fadeRadius = 0.0f;
    memset(actor->colorIndex, XENO_COLOR_INDEX_NONE, 0x10);
    tiMRPackageInit(handle);
    tiMRPackageSetAnimData(handle, 0);
    memset(actor->geometryMask, 0xFF, sizeof(actor->geometryMask));
    xglLightSetDefault((XglLightSet *) &actor->light);
    UnduParamInit(&actor->collision);
}

/* XenoActorAlloc: takes the first free record from the pool, initializes it and adds it to the active table. */
XenoActor *XenoActorAlloc(void) {
    unsigned int i;
    int slot;
    XenoActor *pool;
    XenoActor *actor = 0;

    for (i = 0; i < XenoActorPoolCount; i++) {
        if (XenoActorPool[i].attr == 0) {
            XenoActorInit(&XenoActorPool[i]);
            pool = XenoActorPool;
            /* Never taken (actor is still 0 here); the original keeps this check, which
             * splits the pool reload from the attr store (gcse then shares the reload).
             * Real conditions tried in its place all change the code: "pool[i].attr != 0" (69 rows),
             * "pool == 0" (62 rows), "i >= XenoActorPoolCount" (92 rows); each is folded or reordered differently. */
            if (actor != 0) {
                return actor;
            }
            pool[i].attr = 1;
            for (slot = 0; slot < XenoActorPoolCount; slot++) {
                if (XenoActorActiveTable[slot] == 0) {
                    XenoActorActiveTable[slot] = (int)&pool[i];
                    break;
                }
            }
            XenoActorPool[i].flags |= XENO_FLAG_RESET_CHANNEL;
            return &XenoActorPool[i];
        }
    }
    return 0;
}

const char xenoActorAssertExprDynamics[24] = "pAct->pDynamics != NIL";
const char xenoActorAssertExprLocalMat[24] = "pAct->pLocalMat != NIL";
const char xenoActorAssertExprGlobalMat[24] = "pAct->pGlobalMat != NIL";
const char xenoActorAssertExprBlendMat[24] = "pAct->pBlendMat != NIL";
const char xenoActorAssertExprPartsMat[16] = "pAct->pPartsMat";

/* XenoActorAllocMatrices: frees and reallocates the matrix buffers of a record from its resource, then rebuilds the motion channel at 0x640. */
int XenoActorAllocMatrices(XenoActor *actor) {
    TiHrcData *resource;
    TiMRPackage *package;
    TiOffsetEntry *entryA, *entryB;
    TiNodeTable *entryTableC;
    OffsetTable *tableA, *tableB;
    TiPamData *clampRange;
    PxyHeader *pack;

    if (actor->pLocalMat != 0) {
        RgHeapFree(actor->pLocalMat, xenoActorAssertFile, 0xB5);
        actor->pLocalMat = 0;
    }
    if (actor->pGlobalMat != 0) {
        RgHeapFree(actor->pGlobalMat, xenoActorAssertFile, 0xB6);
        actor->pGlobalMat = 0;
    }
    if (actor->pBlendMat != 0) {
        RgHeapFree(actor->pBlendMat, xenoActorAssertFile, 0xB7);
        actor->pBlendMat = 0;
    }
    if (actor->pDynamics != 0) {
        RgHeapFree(actor->pDynamics, xenoActorAssertFile, 0xB8);
        actor->pDynamics = 0;
    }
    resource = XenoActorResource(actor);
    if (resource != 0) {
        entryA = (TiOffsetEntry *)actor->rsrc[XAct_Rsrc_3];
        tableA = XenoActorSlotTable(actor, 0);
        entryTableC = (TiNodeTable *)actor->rsrc[XAct_Rsrc_EntryTableC];
        entryB = (TiOffsetEntry *)actor->rsrc[XAct_Rsrc_SlotEntry];
        tableB = XenoActorSlotTable(actor, 5);
        actor->pLocalMat = RgHeapAllocById(5, resource->transformCount << 6, xenoActorAssertFile, 0xC7);
        actor->pGlobalMat = RgHeapAllocById(5, resource->transformCount << 6, xenoActorAssertFile, 0xC8);
        actor->pBlendMat = RgHeapAllocById(5, resource->transformCount << 6, xenoActorAssertFile, 0xC9);
        if (resource->dynamicsCount != 0) {
            actor->pDynamics = RgHeapAllocById(5, resource->dynamicsCount << 6, xenoActorAssertFile, 0xCD);
            if (actor->pDynamics == 0) {
                assertFailed(xenoActorAssertExprDynamics, xenoActorAssertFile, 0xCE);
            }
        }
        if (actor->pLocalMat == 0) {
            assertFailed(xenoActorAssertExprLocalMat, xenoActorAssertFile, 0xD3);
        }
        if (actor->pGlobalMat == 0) {
            assertFailed(xenoActorAssertExprGlobalMat, xenoActorAssertFile, 0xD4);
        }
        if (actor->pBlendMat == 0) {
            assertFailed(xenoActorAssertExprBlendMat, xenoActorAssertFile, 0xD5);
        }
        package = XenoActorMotionPack(actor);
        tiMRPackageInit(package);
        tiMRPackageSetLocalMat(package, actor->pLocalMat);
        tiMRPackageSetGlobalMat(package, actor->pGlobalMat);
        tiMRPackageSetBlendMat(package, actor->pBlendMat);
        tiMRPackageSetHrcData(package, resource);
        tiMRPackageSetAnimData(package, entryA);
        tiMRPackageSetAnimTable(package, tableA);
        tiMRPackageSetFaceAnimData(package, entryB);
        tiMRPackageSetFaceAnimTable(package, tableB);
        tiMRPackageSetEntryTableC(package, entryTableC);
        tiMRPackageFillNodes(package, actor->pDynamics);
        XenoActorSetMode(actor, 0);
        XenoActorSetSelector(actor, 0);
    }
    clampRange = (TiPamData *)actor->rsrc[XAct_Rsrc_PAM];
    if (clampRange != 0) {
        pack = XenoActorPack(actor);
        if (pack != 0) {
            if (actor->pPartsMat != 0) {
                RgHeapFree(actor->pPartsMat, xenoActorAssertFile, 0xED);
            }
            actor->pPartsMat = RgHeapAllocById(5, pack->geometryCount << 6, xenoActorAssertFile, 0xEE);
            if (actor->pPartsMat == 0) {
                assertFailed(xenoActorAssertExprPartsMat, xenoActorAssertFile, 0xF0);
            }
            package = XenoActorMotionPack(actor);
            tiMRPackageInit(package);
            tiMRPackageSetPartsMat(package, actor->pPartsMat);
            tiMRPackageSetClampRange(package, clampRange);
        }
    }
    return 1;
}

/* XenoActorAttachChild: links a live child under a parent's child slots (0x5AC) and sets the child parent (0x5A4). */
int XenoActorAttachChild(XenoActor *parent, XenoActor *child) {
    int i;

    if (parent != 0 && child != 0 && parent->attr != 0 && child->attr != 0 && child->parent == 0) {
        for (i = 0; i < 8; i++) {
            if (parent->children[i] == 0) {
                parent->children[i] = child;
                child->parent = parent;
                return 1;
            }
        }
    }
    return 0;
}

int XenoActorAppendChildOnJoint(XenoActor *parent, XenoActor *child, int joint) {
    int i;
    if (parent != 0 && child != 0 && parent->attr != 0 && child->attr != 0 && child->parent == 0) {
        for (i = 0; i < 8; i++) {
            if (parent->children[i] == 0) {
                if (joint >= 0) {
                    child->overrideMatrix = parent->pMotionGlobalMat + joint;
                } else {
                    child->overrideMatrix = 0;
                }
                parent->children[i] = child;
                child->parent = parent;
                return 1;
            }
        }
    }
    return 0;
}

/* XenoActorAttachChildToJoint: finds the joint with the given id in the resource joint table and attaches the child there. */
int XenoActorAttachChildToJoint(XenoActor *actor, XenoActor *child, int jointId) {
    TiHrcData *resource = XenoActorResource(actor);
    TiHrcStatic *joints;
    int found;
    u32 i;
    if (resource == 0) {
        return 0;
    }
    if (resource->version < 2.4f) {
        if (jointId >= actor->pHrcData->staticCount) {
            return 0;
        }
        return XenoActorAppendChildOnJoint(actor, child, actor->pHrcData->jointCount + actor->pHrcData->joint2Count + actor->pHrcData->skipCount + actor->pHrcData->dynamicsCount + jointId);
    }
    joints = (TiHrcStatic *)((unsigned char *)resource + resource->staticOffset);
    found = -1;
    for (i = 0; i < resource->staticCount; i++) {
        if (joints[i].id == jointId) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        return 0;
    }
    return XenoActorAppendChildOnJoint(actor, child, actor->pHrcData->jointCount + actor->pHrcData->joint2Count + actor->pHrcData->skipCount + actor->pHrcData->dynamicsCount + found);
}

/* XenoActorAttachAtSlot: stores the slot index, attaches the child to parent and returns 1; 0 on bad arguments. */
int XenoActorAttachAtSlot(XenoActor *parent, struct XenoActor *actor, int slotIndex) {
    if (parent != 0 && actor != 0 && slotIndex >= 0) {
        actor->slotIndex = slotIndex;
        XenoActorAttachChild(parent, actor);
        return 1;
    }
    return 0;
}

int XenoActorReleaseChild(XenoActor *parent, XenoActor *child) {
    int i;

    if (parent != 0 && child != 0 && parent->attr != 0 && child->attr != 0 && child->parent != 0) {
        for (i = 0; i < 8; i++) {
            if (parent->children[i] == child) {
                parent->children[i] = 0;
                child->slotIndex = -1;
                child->parent = 0;
                child->overrideMatrix = 0;
                return 1;
            }
        }
    }
    return 0;
}

/* XenoActorSetFlagAll: ORs a flag mask into the drawFlags of every live record.
   The pool count is read again after each change (the reload is in the original code). */
void XenoActorSetFlagAll(int flag) {
    unsigned int i;
    unsigned int count = XenoActorPoolCount;

    for (i = 0; i < count; i++) {
        if (XenoActorPool[i].attr != 0) {
            XenoActorPool[i].drawFlags |= flag;
            count = XenoActorPoolCount;
        }
    }
}

/* Sets the blend length, in frames, of motion state A (XenoMotionState.blendFrames). */
void XenoActorSetBlendFrames(struct XenoActor *actor, float value) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x19F);
    }
    tiMRStateSetBlendFrames(XenoActorMotionState(actor, 0), value);
}

/* Sets the blend length, in frames, of motion state B. */
void XenoActorSetBlendFramesB(struct XenoActor *actor, float value) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x1A9);
    }
    tiMRStateSetBlendFrames(XenoActorMotionState(actor, 1), value);
}

/* Starts a blend of motion A (flag 2) that is local (flag 4 set). */
void XenoActorSetBlendLocal(struct XenoActor *actor) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x1B5);
    }
    tiWordSetBits(&actor->motion[0].flags, 4);
    tiWordSetBits(&actor->motion[0].flags, 2);
}

/* Starts a blend of motion A (flag 2) that is global (flag 4 cleared). */
void XenoActorSetBlendGlobal(struct XenoActor *actor) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x1C2);
    }
    tiWordClearBits(&actor->motion[0].flags, 4);
    tiWordSetBits(&actor->motion[0].flags, 2);
}

/* Starts a blend of motion B (flag 2) that is local (flag 4 set). */
void XenoActorSetBlendLocalB(struct XenoActor *actor) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x1CD);
    }
    tiWordSetBits(&actor->motion[1].flags, 4);
    tiWordSetBits(&actor->motion[1].flags, 2);
}

/* Starts a blend of motion B (flag 2) that is global (flag 4 cleared). */
void XenoActorSetBlendGlobalB(struct XenoActor *actor) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x1D8);
    }
    tiWordClearBits(&actor->motion[1].flags, 4);
    tiWordSetBits(&actor->motion[1].flags, 2);
}

void XenoActorResetChannelFlags(struct XenoActor *actor) {
    tiWordClearBits(&actor->motion[0].flags, 2);
    tiWordClearBits(&actor->motion[0].flags, 0x40);
    tiWordClearBits(&actor->motion[1].flags, 2);
    tiWordClearBits(&actor->motion[1].flags, 0x40);
}

void XenoActorClearChannel(XenoActor *actor) {
    tiMRPackageSetAnimTable(XenoActorMotionPack(actor), 0);
    tiMRPackageSetAnimData(XenoActorMotionPack(actor), 0);
}


/* XenoActorSetMode: sets the current mode (slot 0), clears the request and restarts channel 0x640 at the start (or end) of the mode's range. */
void XenoActorSetMode(XenoActor *actor, int mode) {
    TiOffsetEntry *entry;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x1F6);
    }
    actor->requested_mode = -1;
    actor->mode = mode;
    actor->slot = 0;
    tiMRPackageSetAnimTableAndEntry(XenoActorMotionPack(actor), XenoActorSlotTable(actor, 0), mode);
    actor->flags &= ~XENO_FLAG_RANGE_SET;
    tiWordClearBits(&actor->motion[0].flags, 8);
    tiWordClearBits(&actor->motion[1].flags, 8);
    entry = actor->motionKeys[0];
    if (entry != 0) {
        if (actor->motion[0].speed < 0.0f) {
            actor->frame = entry->rangeMax;
        } else {
            actor->frame = entry->rangeMin;
        }
        tiMRStateSetPosition(XenoActorMotionState(actor, 0), actor->frame);
        if (!(actor->flags & XENO_FLAG_BLEND_SWAPPED) && (actor->motion[0].flags & 2)) {
            tiMtnBlendSwapLocal(XenoActorMotionPack(actor), &actor->motion[0].flags);
            tiMtnBlendSwapGlobal(XenoActorMotionPack(actor), &actor->motion[0].flags);
            actor->flags |= XENO_FLAG_BLEND_SWAPPED;
        }
    }
}

void XenoActorCheckMode(XenoActor *actor, int expected_mode) {
    if (actor->mode != expected_mode) {
        XenoActorSetMode(actor, expected_mode);
    }
}

/* XenoActorSetPlayRange: sets the playback range and clamps the playback value into it. */
/* XenoActorSetPlayRange: marks the range as set (flag 0x40); when the play position lies outside [start, end] restarts it from start, or from end when the motion runs backwards; then forwards the range to the state. */
void XenoActorSetPlayRange(XenoActor *actor, int start, int end) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x22F);
    }
    actor->flags |= XENO_FLAG_RANGE_SET;
    if (actor->frame < (float)start || (float)end < actor->frame) {
        if (actor->motion[0].speed < 0.0f) {
            actor->frame = (float)end;
        } else {
            actor->frame = (float)start;
        }
    }
    tiMRStateSetRange(XenoActorMotionState(actor, 0), start, end);
}


void XenoActorApplySourceValue(XenoActor *actor) {
    actor->frame = (f32) actor->motion[0].range[0];
    XenoActorConvertSourceRecord(&actor->motion[0]);
}

/* XenoActorSetModeInSlot: like XenoActorSetMode, for one of the four mode slots (asserts slot < 4). */
void XenoActorSetModeInSlot(XenoActor *actor, u32 slot, int mode) {
    TiOffsetEntry *entry;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x24D);
    }
    if (slot < 4) {
        actor->requested_mode = -1;
        actor->slot = slot;
        actor->mode = mode;
        tiMRPackageSetAnimTableAndEntry(XenoActorMotionPack(actor), ((XenoActorSlotView *) actor)->tables[slot], mode);
        actor->flags &= ~XENO_FLAG_RANGE_SET;
        tiWordClearBits(&actor->motion[0].flags, 8);
        tiWordClearBits(&actor->motion[1].flags, 8);
        entry = actor->motionKeys[0];
        if (entry != 0) {
            if (actor->motion[0].speed < 0.0f) {
                actor->frame = entry->rangeMax;
            } else {
                actor->frame = entry->rangeMin;
            }
            tiMRStateSetPosition(XenoActorMotionState(actor, 0), actor->frame);
            if (!(actor->flags & XENO_FLAG_BLEND_SWAPPED) && (actor->motion[0].flags & 2)) {
                tiMtnBlendSwapLocal(XenoActorMotionPack(actor), &actor->motion[0].flags);
                tiMtnBlendSwapGlobal(XenoActorMotionPack(actor), &actor->motion[0].flags);
                actor->flags |= XENO_FLAG_BLEND_SWAPPED;
            }
        }
    }
}

/*
 * Partial view of the xenoActor record: only the offsets this TU's claimed
 * functions touch are named (XenoActorSetSelector at +0x5CC..+0x7A4 and
 * XenoActorRefreshItems at +0x710 and +0x7A4). Gaps are unmodeled spans.
 */

typedef struct XenoActorItem {
    unsigned char unmodeled_00[0x04];
    f32 weight;                     /* +0x04 */
    unsigned char unmodeled_08[0x20 - 0x08];
    Vector4 payload;                /* +0x20: the position tiDrawCollisionDrawObjectRing draws the ring at */
} XenoActorItem;


/* XenoActorSetSelector: stores the selector, reads the channel value and copies the entry value into cachedValue. */
void XenoActorSetSelector(XenoActor *actor, int selector)
{
    TiOffsetEntry *entry;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x27D);
    }
    actor->selector = selector;
    tiMRPackageSelectFaceAnimEntry(XenoActorMotionPack(actor), selector);
    entry = actor->motionKeys[2];
    if (entry != 0) {
        f32 entryValue = entry->rangeMin;

        actor->cachedValue = entryValue;
        tiMRStateSetPosition(XenoActorMotionState(actor, 1), entryValue);
        if (!(actor->flags & XENO_FLAG_BLEND_SWAPPED) && (actor->motion[1].flags & 2)) {
            tiMtnBlendSwapLocal(XenoActorMotionPack(actor), &actor->motion[1].flags);
            tiMtnBlendSwapGlobal(XenoActorMotionPack(actor), &actor->motion[1].flags);
            actor->flags |= XENO_FLAG_BLEND_SWAPPED;
        }
    }
}

void XenoActorRequestMode(struct XenoActor *actor, int mode) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x29A);
    }
    actor->requested_mode = mode;
}

int XenoActorIsDone(struct XenoActor *actor) {
    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x2A5);
    }
    if ((actor->motion[0].flags & 8) != 0) {
        return 1;
    }
    return 0;
}

/* XenoActorUpdateLinkMatrix: when the actor has a link, rebuilds its position/rotation/scale block from the link's joint row. */
void XenoActorUpdateLinkMatrix(struct XenoActor *actor) {
    float block[9];
    u8 *linkRow;
    int link;

    link = actor->link;
    if (link != 0) {
        linkRow = (u8 *)(link + 0x50);
        xacEvalTransform(block, &linkRow, actor->frame);
        actor->position.x = block[0];
        actor->position.y = block[1];
        actor->position.z = block[2];
        actor->rotation.x = block[3];
        actor->rotation.y = block[4];
        actor->rotation.z = block[5];
        actor->scale.x = block[6];
        actor->scale.y = block[7];
        actor->scale.z = block[8];
    }
}

/* XenoActorApplyRequestedMode: applies a pending mode request, then updates channel 0x640. */
void XenoActorApplyRequestedMode(XenoActor *actor) {
    PxyHeader *pack;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x2CF);
    }
    if (actor->requested_mode >= 0 && XenoActorIsDone(actor)) {
        XenoActorSetBlendLocal(actor);
        XenoActorSetMode(actor, actor->requested_mode);
        actor->requested_mode = -1;
    }
    if (actor->pHrcData != 0) {
        actor->flags &= ~XENO_FLAG_CHANNEL_BIT20;
        actor->channelMatrix = &actor->localMatrix;
        tiMRPackageUpdate(XenoActorMotionPack(actor));
        if ((actor->motion[0].flags & 0x20) || (actor->motion[1].flags & 0x20)) {
            actor->flags |= XENO_FLAG_CHANNEL_BIT20;
        }
    }
    if (actor->pPamData != 0 && (pack = XenoActorPack(actor)) != 0) {
        if (actor->pPartsMat == 0) {
            assertFailed(xenoActorAssertExprPartsMat, xenoActorAssertFile, 0x2F4);
        }
        tiPartsAnimSamplePose(actor->pPartsMat, XenoActorMotionPack(actor), pack->geometryCount);
    }
    actor->flags &= ~XENO_FLAG_BLEND_SWAPPED;
    tiWordClearBits(&actor->motion[0].flags, 0x10);
}

/* xenoActorSlotRangeValid: 1 when the lower bound of entry idx of the slot's range table does not exceed its upper bound. */
int xenoActorSlotRangeValid(XenoActor *actor, int slot, unsigned int idx) {
    OffsetTable *table = XenoActorSlotTable(actor, slot);
    TiOffsetEntry *range;

    if (table == 0) {
        return 0;
    }
    if (idx >= table->count) {
        return 0;
    }
    /* offset read as *(base + idx): the indexed form swaps the addu operands */
    range = (TiOffsetEntry *)((unsigned char *)table + *(table->entryOffsets + idx));
    if (range->rangeMin <= range->rangeMax) {
        return 1;
    }
    return 0;
}

/* XenoActorSetHoldFlag: sets the global update hold XenoActorHoldFlag; XenoActorDoit skips every tree update while it is set. */
void XenoActorSetHoldFlag(void) {
    XenoActorHoldFlag = 1;
}

/* XenoActorClearHoldFlag: clears the global update hold XenoActorHoldFlag; XenoActorPoolInit clears it when the pool is built. */
void XenoActorClearHoldFlag(void) {
    XenoActorHoldFlag = 0;
}

const char xenoActorAssertExprRelease[16] = "bRelease";
/* XenoActorRelease: frees the matrix buffers, removes the record from the active table and its parent, detaches its children, then resets it. */
void XenoActorRelease(XenoActor *actor) {
    int i, j;
    int found;
    XenoActor *parent;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x331);
    }
    if (actor->pLocalMat != 0) {
        RgHeapFree(actor->pLocalMat, xenoActorAssertFile, 0x333);
        actor->pLocalMat = 0;
    }
    if (actor->pGlobalMat != 0) {
        RgHeapFree(actor->pGlobalMat, xenoActorAssertFile, 0x334);
        actor->pGlobalMat = 0;
    }
    if (actor->pBlendMat != 0) {
        RgHeapFree(actor->pBlendMat, xenoActorAssertFile, 0x335);
        actor->pBlendMat = 0;
    }
    if (actor->pPartsMat != 0) {
        RgHeapFree(actor->pPartsMat, xenoActorAssertFile, 0x336);
        actor->pPartsMat = 0;
    }
    if (actor->pDynamics != 0) {
        RgHeapFree(actor->pDynamics, xenoActorAssertFile, 0x337);
        actor->pDynamics = 0;
    }
    found = 0;
    for (i = 0; i < XenoActorPoolCount; i++) {
        if ((XenoActor *)XenoActorActiveTable[i] == actor) {
            XenoActorActiveTable[i] = 0;
            found = 1;
            break;
        }
    }
    if (!found) {
        assertFailed(xenoActorAssertExprRelease, xenoActorAssertFile, 0x353);
    }
    parent = actor->parent;
    if (parent != 0) {
        for (j = 0; j < 8; j++) {
            if (parent->children[j] == actor) {
                parent->children[j] = 0;
                break;
            }
        }
    }
    for (j = 0; j < 8; j++) {
        if (actor->children[j] != 0) {
            actor->children[j]->parent = 0;
        }
    }
    XenoActorInit(actor);
}


/* XenoActorUpdateClipSweep (XenoActorUpdateClipSweep): advances the clip-plane sweep of an actor (phase at +0x150, direction at +0x154) and
   builds its clip plane pair (shadowA/shadowB) for the current shape state (+0x160). */
void XenoActorUpdateClipSweep(XenoActor *actor) {
    float t;
    float scale;
    float speed;

    if (actor->clipPlaneCount == 0) {
        return;
    }
    speed = 0.0f;
    scale = 1.0f;
    actor->clipPlaneValue2 = scale;
    if (XenoActorPack(actor) != 0 && XenoActorPack(actor)->magic[0] == 'p' && XenoActorPack(actor)->magic[1] == 'x' && XenoActorPack(actor)->magic[2] == 'y') {
        scale = XenoActorPack(actor)->sphere.w;
        if (speed < scale) {
            actor->clipPlaneValue2 = scale;
        }
    }
    speed = actor->sweepSpeed;
    actor->sweepPhase += __builtin_fabsf(speed) * 0.033333335f;
    if (speed < 0.0f) {
        if (actor->sweepPhase >= 1.0f) {
            actor->sweepPhase = 0.0f;
            actor->clipPlaneCount = 0;
        }
    }
    switch (actor->moveState) {
    case 0:
        if (speed < 0.0f) {
            speed = actor->sweepPhase * scale - scale * 0.5f;
        } else {
            speed = (1.0f - actor->sweepPhase) * scale - scale * 0.5f;
        }
        actor->clipPlaneB.y = actor->rotation.y;
        actor->clipPlaneA.z = speed;
        actor->clipPlaneA.w = 1.0f;
        actor->clipPlaneB.x = 0.0f;
        actor->clipPlaneB.z = 0.0f;
        actor->clipPlaneA.x = 0.0f;
        actor->clipPlaneA.y = 0.0f;
        xglMatrixStackUnit();
        xglMatrixStackRotY(actor->rotation.y);
        xglMatrixStackMulVector(&actor->clipPlaneA, &actor->clipPlaneA);
        break;
    case 1:
        if (speed < 0.0f) {
            speed = actor->sweepPhase * scale - scale * 0.5f;
        } else {
            speed = (1.0f - actor->sweepPhase) * scale - scale * 0.5f;
        }
        actor->clipPlaneB.y = actor->rotation.y + 3.1415920f;
        actor->clipPlaneA.z = speed;
        actor->clipPlaneA.w = 1.0f;
        actor->clipPlaneB.x = 0.0f;
        actor->clipPlaneB.z = 0.0f;
        actor->clipPlaneA.x = 0.0f;
        actor->clipPlaneA.y = 0.0f;
        xglMatrixStackUnit();
        xglMatrixStackRotY(actor->rotation.y + 3.1415920f);
        xglMatrixStackMulVector(&actor->clipPlaneA, &actor->clipPlaneA);
        break;
    case 2:
        t = (speed < 0.0f ? actor->sweepPhase : 1.0f - actor->sweepPhase) * scale * 2.0f;
        actor->clipPlaneB.x = -1.5707960f;
        actor->clipPlaneB.y = 0.0f;
        actor->clipPlaneA.y = t;
        actor->clipPlaneA.w = 1.0f;
        actor->clipPlaneB.z = 0.0f;
        actor->clipPlaneA.x = 0.0f;
        actor->clipPlaneA.z = 0.0f;
        break;
    case 3:
        t = (speed < 0.0f ? 1.0f - actor->sweepPhase : actor->sweepPhase) * scale * 2.0f;
        actor->clipPlaneB.x = 1.5707960f;
        actor->clipPlaneB.y = 0.0f;
        actor->clipPlaneA.y = t;
        actor->clipPlaneA.w = 1.0f;
        actor->clipPlaneB.z = 0.0f;
        actor->clipPlaneA.x = 0.0f;
        actor->clipPlaneA.z = 0.0f;
        break;
    case 4:
        t = (speed < 0.0f ? actor->sweepPhase : 1.0f - actor->sweepPhase) * scale * 2.0f;
        actor->clipPlaneB.x = -0.7853980f;
        actor->clipPlaneB.y = -1.5707960f;
        actor->clipPlaneA.y = t;
        actor->clipPlaneA.w = 1.0f;
        actor->clipPlaneB.z = 0.0f;
        actor->clipPlaneA.x = 0.0f;
        actor->clipPlaneA.z = 0.0f;
        break;
    case 5:
        t = (speed < 0.0f ? 1.0f - actor->sweepPhase : actor->sweepPhase) * scale * 2.0f;
        actor->clipPlaneB.x = 0.7853980f;
        actor->clipPlaneB.y = 1.5707960f;
        actor->clipPlaneA.y = t;
        actor->clipPlaneA.w = 1.0f;
        actor->clipPlaneB.z = 0.0f;
        actor->clipPlaneA.x = 0.0f;
        actor->clipPlaneA.z = 0.0f;
        break;
    case 6:
        t = (speed < 0.0f ? actor->sweepPhase : 1.0f - actor->sweepPhase) * scale * 2.0f;
        actor->clipPlaneB.x = -2.3561940f;
        actor->clipPlaneB.y = -1.5707960f;
        actor->clipPlaneA.y = t;
        actor->clipPlaneA.w = 1.0f;
        actor->clipPlaneB.z = 0.0f;
        actor->clipPlaneA.x = 0.0f;
        actor->clipPlaneA.z = 0.0f;
        break;
    case 7:
        t = (speed < 0.0f ? 1.0f - actor->sweepPhase : actor->sweepPhase) * scale * 2.0f;
        actor->clipPlaneB.x = 0.7853980f;
        actor->clipPlaneB.y = -1.5707960f;
        actor->clipPlaneA.y = t;
        actor->clipPlaneA.w = 1.0f;
        actor->clipPlaneB.z = 0.0f;
        actor->clipPlaneA.x = 0.0f;
        actor->clipPlaneA.z = 0.0f;
        break;
    default:
        break;
    }
    actor->clipPlaneB.y += actor->rotation.y;
    Vu0AddVectorXYZ(&actor->clipPlaneA, &actor->clipPlaneA, &actor->vector80);
    Vu0AddVectorXYZ(&actor->clipPlaneA, &actor->clipPlaneA, &actor->localMatrix.m[3]);
}
#define XENO_DRAW_CAMERA_FADE 0x4000000   /* camera-distance fade by alphaScale (root actors; children follow their parent) */

/* XenoActorUpdateCameraFade: fades the actor at +0x188 by its distance to the active camera. */
/* XenoActorUpdateCameraFade: fades the actor at +0x188 by its distance to the active camera. */
void XenoActorUpdateCameraFade(XenoActor *actor) {
    XglCamera *camera;
    PxyHeader *pack;
    DoublewordAlignedVector4 target;
    DoublewordVector4 cameraPosition;
    float radius;
    float distance;
    float fadeEnd;

    camera = xglStudioSelectGetActiveCamera(0);
    actor->alphaScale = 1.0f;
    if ((actor->drawFlags & XENO_DRAW_CAMERA_FADE) && camera != 0 && actor->parent == 0 && (pack = XenoActorPack(actor)) != 0 &&
        pack->magic[0] == 'p' && pack->magic[1] == 'x' && pack->magic[2] == 'y' && (pack->flags & 0x81) == 0) {
        radius = pack->sphere.w;
        target = actor->position;
        cameraPosition = *(DoublewordVector4 *)&camera->travel.position;
        if (actor->fadeRadius != 0.0f) {
            radius = actor->fadeRadius;
        }
        Vu0AddVectorXYZ(&target, &target, &actor->vector80);
        target.w = 1.0f;
        distance = xglPointLength(&cameraPosition.vector, &target);
        if (distance < radius) {
            actor->alphaScale = 0.0f;
        } else {
            fadeEnd = radius * 1.6f;
            if (distance < fadeEnd) {
                actor->alphaScale = 1.0f - (fadeEnd - distance) / (fadeEnd - radius);
            }
        }
        if (actor->alphaScale < 0.0f) {
            actor->alphaScale = 0.0f;
        }
    }
}

/* XenoActor.flags bits: see the XENO_FLAG_* block after the XenoActor definition. */
#define XENO_DRAW_SOURCE_DIRTY 0x1000000


/* XenoActorUpdateTree: per-frame update of the actor and its child records (recursive). */
void XenoActorUpdateTree(XenoActor *actor) {
    XenoHrc *table;
    XenoMatrix inverseMatrix;
    Vector4 delta;
    DoublewordAlignedVector4 scaled;
    Vector4 angles;
    Vector4 jointDelta;
    u32 i;
    int child;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x47C);
    }
    if (actor != 0) {
        actor->sourceBlock = actor->position;
        actor->drawFlags &= ~XENO_DRAW_SOURCE_DIRTY;
        if (XenoActorPack(actor) != 0 && XenoActorPack(actor)->magic[0] == 'p' && XenoActorPack(actor)->magic[1] == 'x' && XenoActorPack(actor)->magic[2] == 'y') {
            Vu0AddVectorXYZ(&actor->sourceBlock, &actor->sourceBlock, &XenoActorPack(actor)->sphere);
        }
        actor->renderValue += actor->renderValue2;
        XenoActorUpdateLinkMatrix(actor);
        if (actor->pLocalMat != 0) {
            XenoActorPushMatrix(actor);
            if (actor->pHrcData->dynamicsCount != 0 && (actor->flags & XENO_FLAG_SKIP_DELTA) == 0) {
                /* XenoMatrix is the actor's own view of the 4x4 matrix that xgl_2.h declares as the Matrix union. */
                xglMatrixInverse((Matrix *)&inverseMatrix, (Matrix *)&actor->localMatrix);
                xglVectorMulMat(&delta, inverseMatrix.m, &actor->previousPosition);
                delta.x *= 0.3f;
                delta.y *= 0.3f;
                delta.z *= 0.3f;
                QuadwordCopy(&angles, &actor->rotation);
                while (angles.x < 0.0f) {
                    angles.x += 6.2831855f;
                }
                while (angles.y < 0.0f) {
                    angles.y += 6.2831855f;
                }
                while (angles.z < 0.0f) {
                    angles.z += 6.2831855f;
                }
                scaled.z = actor->angle[2] - angles.z;
                scaled.x = actor->angle[0] - angles.x;
                scaled.y = actor->angle[1] - angles.y;
                xglCurSetRotateZ(scaled.z);
                xglCurMulRotateY(scaled.y);
                xglCurMulRotateX(scaled.x);
                for (i = 0; i < actor->pHrcData->dynamicsCount; i++) {
                    xglVectorAddXYZ_00292E38((Vector4 *)actor->jointRecords[i].rest, (Vector4 *)actor->jointRecords[i].rest, &delta);
                    xglCurApplyVector(&jointDelta, &actor->jointRecords[i].translation.vector);
                    jointDelta.x = (jointDelta.x - actor->jointRecords[i].translation.vector.x) * 0.6f;
                    jointDelta.y = (jointDelta.y - actor->jointRecords[i].translation.vector.y) * 0.6f;
                    jointDelta.z = (jointDelta.z - actor->jointRecords[i].translation.vector.z) * 0.6f;
                    __asm__ __volatile__(
                        "lqc2 vf20, 0(%0)\n\t"
                        "lqc2 vf21, 0(%1)\n\t"
                        "vadd.xyzw vf20, vf20, vf21\n\t"
                        "sqc2 vf20, 0(%0)"
                        : : "r"(actor->jointRecords[i].rest + 0), "r"(&jointDelta) : "memory");
                }
                QuadwordCopy(&actor->previousPosition, &actor->position);
                QuadwordCopy(actor->angle, &angles);
            } else {
                actor->flags &= ~XENO_FLAG_SKIP_DELTA;
            }
            if (actor->flags & XENO_FLAG_RESET_CHANNEL) {
                XenoActorResetChannelFlags(actor);
                actor->flags &= ~XENO_FLAG_RESET_CHANNEL;
            }
            tiMRStateSetPosition(XenoActorMotionState(actor, 0), actor->frame);
            tiMRStateSetPosition(XenoActorMotionState(actor, 1), actor->cachedValue);
            XenoActorApplyRequestedMode(actor);
            if (actor->flags & XENO_FLAG_SHIFT_JOINTS) {
                XenoActorShiftJoints(actor, 1, 1, 1, 1);
                actor->flags &= ~XENO_FLAG_SHIFT_JOINTS;
            }
            actor->vector80.x = actor->pMotionGlobalMat[1].m[3][0];
            actor->vector80.y = actor->pMotionGlobalMat[1].m[3][1];
            actor->vector80.z = actor->pMotionGlobalMat[1].m[3][2];
            actor->vector80.w = 1.0f;
        } else {
            /* Each arm resets the position with its own constants (kept in FPRs). */
            float zero;
            float one;

            if (actor->rsrc[XAct_Rsrc_PAM] != 0) {
                tiMRStateSetPosition(XenoActorMotionState(actor, 0), actor->frame);
                tiMRStateSetPosition(XenoActorMotionState(actor, 1), actor->cachedValue);
                XenoActorApplyRequestedMode(actor);
                actor->flags &= ~XENO_FLAG_RESET_CHANNEL;
                zero = 0.0f;
                one = 1.0f;
                actor->vector80.z = zero;
                actor->vector80.y = zero;
                actor->vector80.x = zero;
                actor->vector80.w = one;
            } else {
                zero = 0.0f;
                one = 1.0f;
                actor->vector80.z = zero;
                actor->vector80.y = zero;
                actor->vector80.x = zero;
                actor->vector80.w = one;
            }
        }
        actor->sourceBlock = actor->position;
        if ((actor->pHrcData != 0 && (actor->motionKeys[0] != 0 || actor->motionKeys[1] != 0)) || actor->rsrc[XAct_Rsrc_PAM] != 0) {
            actor->sourceBlock = actor->vector80;
            xglVectorScaleXYZ(&scaled, &actor->sourceBlock, 1.0f / actor->scale.x);
            scaled.w = 1.0f;
            xglVectorMulMat(&actor->sourceBlock, actor->localMatrix.m, &scaled);
            QuadwordCopy(&scaled, &actor->sourceBlock);
            /* Written back through the doubleword union view of the block. */
            ((DoublewordVector4 *)&actor->sourceBlock)->vector = scaled;
            if ((actor->flags & XENO_FLAG_FREEZE_A) == 0) {
                if (actor->flags & XENO_FLAG_UNWRAP_A) {
                    tiMRPackageAdvanceAnimUnwrapped(XenoActorMotionPack(actor));
                } else {
                    tiMRPackageAdvanceAnim(XenoActorMotionPack(actor));
                }
                actor->frame = actor->motion[0].frame;
            }
        }
        if (actor->pHrcData != 0 && (actor->motionKeys[2] != 0 || actor->motionKeys[3] != 0) && (actor->flags & XENO_FLAG_FREEZE_B) == 0) {
            if (actor->flags & XENO_FLAG_UNWRAP_B) {
                tiMRPackageAdvanceFaceAnimUnwrapped(XenoActorMotionPack(actor));
            } else {
                tiMRPackageAdvanceFaceAnim(XenoActorMotionPack(actor));
            }
            actor->cachedValue = actor->motion[1].frame;
        }
        XenoActorUpdateClipSweep(actor);
        XenoActorUpdateCameraFade(actor);
    }
    for (child = 0; child < 8; child++) {
        if (actor->children[child] != 0) {
            XenoActorUpdateTree(actor->children[child]);
        }
    }
}

/* RGBA colours of the collision ring of an item, idle and active (tiDrawCollisionDrawObjectRing). */
u32 XenoActorItemRingColorIdle[4] = { 96, 170, 96, 255 };
u32 XenoActorItemRingColorActive[4] = { 204, 255, 170, 255 };

/* XenoActorRefreshItems: draws each table item; the active item uses its own text. */
void XenoActorRefreshItems(XenoActor *actor)
{
    XenoHrc *table;
    XenoActorItem *item;
    u32 itemIndex;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x52E);
    }
    table = actor->pHrcData;
    itemIndex = 0;
    if (table != 0) {
        item = (XenoActorItem *) ((unsigned char *) table + table->dataOffset);
        if (table->count != 0) {
            do {
                f32 weight = item->weight;

                if (actor->activeItem == itemIndex) {
                    tiDrawCollisionDrawObjectRing(&item->payload, XenoActorItemRingColorActive, weight);
                } else {
                    tiDrawCollisionDrawObjectRing(&item->payload, XenoActorItemRingColorIdle, weight);
                }
                itemIndex++;
                item++;
            } while (itemIndex < actor->pHrcData->count);
        }
        actor->activeItem = -1;
    }
}

/* XenoActorMulMatrix: multiplies a matrix by the actor override or local matrix when its channel is active. */
int XenoActorMulMatrix(struct XenoActor *actor, int nodeKey, Matrix4 matrix, int useBaseMatrix) {
    int result;

    result = tiMRPackageGetNodeMatrix(XenoActorMotionPack(actor), nodeKey, matrix, useBaseMatrix);
    if (result != 0) {
        if (actor->overrideMatrix != 0) {
            xglMatrixMul(matrix, matrix, actor->overrideMatrix);
        } else {
            xglMatrixMul(matrix, matrix, actor->localMatrix.m);
        }
        result = 1;
    }
    return result;
}

/* XenoActorMulVector: multiplies a vector by the actor override or local matrix when its channel is active. */
int XenoActorMulVector(struct XenoActor *actor, int nodeKey, Vector4 *vector, int apply) {
    int result;

    result = tiMRPackageGetNodePoint(XenoActorMotionPack(actor), nodeKey, vector, apply);
    if (result != 0) {
        if (actor->overrideMatrix != 0) {
            xglVectorMulMat(vector, actor->overrideMatrix, vector);
        } else {
            xglVectorMulMat(vector, actor->localMatrix.m, vector);
        }
        result = 1;
    }
    return result;
}

/* XenoActorPushMatrix: builds the actor transform on the matrix stack (translation, rotations, scale and offset of the actor or its parent), saves it in the local matrix and
   concatenates the override matrix; a slot of 2 or more additionally multiplies by the parent's joint matrices. */
void XenoActorPushMatrix(XenoActor *actor) {
    XenoActor *source;
    XenoMatrix *local = &actor->localMatrix;
    XenoMatrix *matrices;
    PxyHeader *pack;

    source = actor->parent;
    xglMatrixStackUnit();
    if (source == 0) {
        source = actor;
    }
    xglMatrixStackTrans(&source->position);
    xglMatrixStackRotZ(source->rotation.z);
    xglMatrixStackRotY(source->rotation.y);
    xglMatrixStackRotX(source->rotation.x);
    xglMatrixStackScale(&source->scale);
    xglMatrixStackTrans(&source->vector70);
    xglMatrixStackSave(local->m);
    if (actor->overrideMatrix != 0) {
        xglMatrixMul(local->m, local->m, actor->overrideMatrix);
    }
    if (actor->slotIndex >= 2 && actor->parent != 0 && actor->parent->pMotionGlobalMat != 0) {
        pack = XenoActorPack(actor->parent);
        xglMatrixStackLoad(local->m);
        xglMatrixStackMul(actor->parent->pMotionGlobalMat[actor->slotIndex].m);
        xglMatrixStackMul(((XenoMatrix *)((unsigned char *)pack + pack->jointMatrixOffset))[actor->slotIndex - 2].m);
        xglMatrixStackSave(local->m);
    }
}

void XenoActorSetSolverPasses(struct XenoActor *actor, int value) {
    tiMRPackageSetSolverPasses(XenoActorMotionPack(actor), value);
}

void XenoActorSetVectorUniform(XenoActor *actor, float value) {
    tiMRPackageSetVectorUniform(XenoActorMotionPack(actor), value);
}

void XenoActorSetVector(XenoActor *actor, float x, float y, float z) {
    tiMRPackageSetVector(XenoActorMotionPack(actor), x, y, z);
}

const char xenoActorAssertExprMotionPackHrcData[32] = "pAct->motionPack.pHrcData";
const char xenoActorAssertExprMotionPackGlobalMat[32] = "pAct->motionPack.pGlobalMat";
const char xenoActorAssertExprJointMatrixCount[72] = "pAct->motionPack.pHrcData->numTransform - 2 >= pPxyHead->nJointMatrix";
const char xenoActorAssertExprMotionPackPamData[40] = "pAct->motionPack.pPamData";

/* actor->drawFlags bits tested by XenoActorDrawMotion; names with a number are set elsewhere and not recovered yet. */
#define XENO_DRAW_COLOR 0x1               /* apply the color block and tint */
#define XENO_DRAW_SKIP_ONCE 0x2           /* cleared on the draw that is skipped */
#define XENO_DRAW_HIDDEN 0x4
#define XENO_DRAW_LIGHT 0x8
#define XENO_DRAW_LIGHT_SOURCE 0x10       /* use sourceLight, refreshed from lightSource */
#define XENO_DRAW_LIGHT_ACTIVE 0x20       /* the light has positive intensity */
#define XENO_DRAW_FOCUS_PENDING 0x40
#define XENO_DRAW_OFFSET_B 0x80
/* XENO_DRAW_FOCUS_LOCKED (0x100) is declared in main/xenoActor.h. */
#define XENO_DRAW_AFTER_POSITION 0x200   /* named by its slot in XenoActorDrawMotion: the render-state call (pxyRequestForcedVisibility) after the model position is set */
#define XENO_DRAW_NO_TREE_UPDATE 0x400    /* XenoActorDoit skips this root actor */
#define XENO_DRAW_ALPHA 0x10000
#define XENO_DRAW_RENDER_2 0x20000        /* adds render flag 2 */
#define XENO_DRAW_SHADOW_PROJECT 0x40000
#define XENO_DRAW_SHADOW_STENCIL 0x80000
#define XENO_DRAW_AFTER_LIGHT 0x100000   /* render-state call (nmlModelSetDropPass) right after the light block */
#define XENO_DRAW_PRE_VISIBILITY 0x200000 /* render-state call (nmlModelSetShadowMapEntry) before the visibility mask is submitted */
#define XENO_DRAW_SOURCE 0x800000       /* the bit TiWater sets on its source actor; pxySetSourceDrawPass runs before the visibility submit */
#define XENO_DRAW_POST_FIRST 0x2000000  /* first render-state call (pxySetShadowFirstPass) of the shadow/focus block */
#define XENO_DRAW_TRANSLUCENT 0x8000000 /* draws the actor translucent (xenoActor.h); pxySetFogWhite in the shadow/focus block */
#define XENO_DRAW_MODEL_HIDDEN 0x10000000 /* set when pxyIsModelHidden() reports the pack's model hidden */
/* Any of these (or a shadow count) enters the shadow/focus block. */
#define XENO_DRAW_POST_MASK 0xA0C01C0
#define XENO_RENDER_FLAG_2 2
/* XenoActorDrawMotion: draws the motion pack (HRC, global matrices and PAM data) of the actor. */
void XenoActorDrawMotion(XenoActor *actor)
{
    PxyHeader *pack;
    XenoMatrix *localMatrix;
    void *visibility;
    PxyHeader *jointPack;
    Matrix *joints;
    XenoMatrix *jointBase;
    PxyHeader *subPack;
    PxyRange *range;
    PxyGeometry *geometry;
    XenoLight *light;
    XenoHrc *hrc;
    XenoMatrix *focus;
    DoublewordAlignedVector4 position;
    int renderFlags;
    u32 flags;
    float scale;
    int joint;
    int part;
    int child;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x5BB);
    }
    /* original keeps this redundant actor != 0 test below; removing it moves code */
    actor->drawFlags &= ~XENO_DRAW_MODEL_HIDDEN;
    if (actor->drawFlags & (XENO_DRAW_SKIP_ONCE | XENO_DRAW_HIDDEN)) {
        actor->drawFlags &= ~XENO_DRAW_SKIP_ONCE;
        return;
    }
    XenoActorPushMatrix(actor);
    if (actor->pMotionGlobalMat != 0 && (actor->attr & 2)) {
        if (actor->pHrcData == 0) {
            assertFailed(xenoActorAssertExprMotionPackHrcData, xenoActorAssertFile, 0x5CE);
        }
        if (actor->pMotionGlobalMat == 0) {
            assertFailed(xenoActorAssertExprMotionPackGlobalMat, xenoActorAssertFile, 0x5CF);
        }
        /* The actor is the joint state, and XenoMatrix is the matrix view the actor stores its global and local matrices as. */
        tiDrawJointDrawLists((struct TiDrawJointState *)actor, (float (*)[4][4])actor->pMotionGlobalMat, (float (*)[4][4])&actor->localMatrix);
        if (actor->attr & 4) {
            XenoActorRefreshItems(actor);
        }
    }
    if (actor != 0 && (pack = XenoActorPack(actor)) != 0 && pack->magic[0] == 'p' && pack->magic[1] == 'x' && pack->magic[2] == 'y') {
        localMatrix = &actor->localMatrix;
        if (actor->state != 0) {
            pxyLatchActorState(actor->state);
            actor->state = 0;
        }
        nmlModelSetDrawMatrix(localMatrix->m);
        nmlModelSetScale(&actor->scale);
        if (actor->colorIndex[0] != XENO_COLOR_INDEX_NONE) {
            pxySetColorIndexSource(actor->colorIndex);
        }
        if (actor->parent != 0) {
            nmlModelSetParent(XenoActorPack(actor->parent));
            if ((actor->parent->drawFlags & XENO_DRAW_CAMERA_FADE) && actor->parent->alphaScale < 1.0f) {
                nmlModelSetStencil(1);
                nmlModelSetZwrite(1);
                nmlModelSetTransparency(actor->parent->alpha * actor->parent->alphaScale);
                if (actor->parent->alpha * actor->parent->alphaScale <= 0.0f) {
                    pxyAbortModel();
                    return;
                }
            }
        } else {
            if (actor->drawFlags & XENO_DRAW_LIGHT) {
                light = &actor->light;
                if (actor->drawFlags & XENO_DRAW_LIGHT_SOURCE) {
                    light = &actor->sourceLight;
                    xglLightPackBuildByName((XglLightSet *) light, (const char *) actor->lightSource, &actor->sourceBlock);
                }
                pxySetLightPtr((XglLightSet *) light);
                if (light->intensity > 0.0f) {
                    actor->drawFlags |= XENO_DRAW_LIGHT_ACTIVE;
                } else {
                    actor->drawFlags &= ~XENO_DRAW_LIGHT_ACTIVE;
                }
                if (actor->drawFlags & XENO_DRAW_LIGHT_ACTIVE) {
                    nmlModelSetHumanModel();
                }
            }
            if (actor->drawFlags & XENO_DRAW_AFTER_LIGHT) {
                nmlModelSetDropPass();
            }
            if ((actor->drawFlags & XENO_DRAW_CAMERA_FADE) && actor->alphaScale < 1.0f) {
                nmlModelSetStencil(1);
                nmlModelSetZwrite(1);
                nmlModelSetTransparency(actor->alpha * actor->alphaScale);
                if (actor->alpha * actor->alphaScale <= 0.0f) {
                    pxyAbortModel();
                    return;
                }
            }
        }
        if (actor->drawFlags & XENO_DRAW_COLOR) {
            nmlModelSetFogCol(&actor->fogColor);
            nmlModelSetFogDist(actor->fogDist.x, actor->fogDist.y, actor->fogDist.z, actor->fogDist.w);
        }
        if (actor->drawFlags & XENO_DRAW_ALPHA) {
            range = (PxyRange *) ((char *) pack + pack->rangeOffset);
            nmlModelSetStencil(1);
            if (range->count != 0) {
                nmlModelSetZwrite(1);
            }
            nmlModelSetTransparency(actor->alpha);
            if (actor->alpha <= 0.0f) {
                pxyAbortModel();
                return;
            }
        }
        if (actor->drawFlags & XENO_DRAW_PRE_VISIBILITY) {
            nmlModelSetShadowMapEntry();
        }
        visibility = actor->visibility;
        if (actor->drawFlags & XENO_DRAW_SOURCE) {
            pxySetSourceDrawPass();
        }
        pxySetHideMask(visibility);
        nmlModelSetSortOffset(actor->modeValue3);
        position = actor->vector80;
        position.y += actor->heightOffset;
        pxySetModelPosition(&position);
        if (actor->flags & XENO_FLAG_CHANNEL_BIT20) {
            nmlModelSetTexMapCircle();
        }
        if (actor->drawFlags & XENO_DRAW_AFTER_POSITION) {
            pxyRequestForcedVisibility();
        }
        switch (actor->shadowType) {
        case 1:
            pxySetShadowMode(1, actor->drawParam, actor->modeValue2, actor->modeValue4);
            if (actor->shadowHeightA != 0.0f || actor->shadowHeightB != 0.0f) {
                pxySetShadowHeightRange(actor->shadowHeightA, actor->shadowHeightB);
            } else {
                scale = 1.0f;
                if (pack->sphere.w >= 3.0f) {
                    scale = 5.0f;
                }
                pxySetShadowHeightRange(actor->shadowHeightC * scale, actor->shadowHeightD * scale);
            }
            break;
        case 9:
            pxySetShadowMode(5, actor->drawParam, actor->modeValue2, actor->modeValue4);
            break;
        case 8:
            pxySetShadowMode(4, actor->drawParam, actor->modeValue2, actor->modeValue4);
            pxySetShadowHeightRange(actor->shadowHeightA, actor->shadowHeightB);
            break;
        case 2:
            pxySetShadowMode(2, actor->drawParam, actor->modeValue2, actor->modeValue4);
            break;
        case 3:
            pxySetShadowMode(3, actor->drawParam, actor->modeValue2, actor->modeValue4);
            break;
        }
        if (actor->pHrcData != 0
            && (actor->motionKeys[0] != 0 || actor->motionKeys[1] != 0 || actor->motionKeys[2] != 0
                || actor->motionKeys[3] != 0)) {
            jointPack = XenoActorPack(actor);
            if (actor->pHrcData->numTransform - 2 < jointPack->nJointMatrix) {
                assertFailed(xenoActorAssertExprJointMatrixCount, xenoActorAssertFile, 0x684);
            }
            if (jointPack->nJointMatrix != 0) {
                joints = nmlPacketSetAttributeAlloc16N(jointPack->nJointMatrix * 4);
                jointBase = (XenoMatrix *) ((char *) jointPack + jointPack->jointMatrixOffset);
                for (joint = 0; joint < jointPack->nJointMatrix; joint++) {
                    xglMatrixMul(joints[joint].rows, actor->pMotionGlobalMat[joint + 2].m, jointBase[joint].m);
                }
                pxySetJointMatrices(joints, (Matrix *) &actor->pMotionGlobalMat[2]);
                pxySetMotionGlobalMatrices((Matrix *) &actor->pMotionGlobalMat[2]);
            }
            if (actor->attr & 4) {
                XenoActorRefreshItems(actor);
            }
            tiMotionUpdateVisibility(XenoActorMotionPack(actor), visibility);
            flags = actor->drawFlags;
            if (!(flags & XENO_DRAW_FOCUS_LOCKED)) {
                hrc = actor->pHrcData;
                if (hrc->skipCount != 0 && (hrc->flags & 4)) {
                    focus = &actor->matrices[hrc->jointCount + hrc->joint2Count + hrc->joint3Count];
                    actor->drawFlags = flags | XENO_DRAW_FOCUS_PENDING;
                    actor->focusX = focus->m[3][2];
                    actor->focusY = focus->m[3][1];
                } else {
                    actor->focusX = 0.0f;
                    actor->focusY = 0.0f;
                }
            }
        } else if (actor->rsrc[XAct_Rsrc_PAM] != 0) {
            if (actor->pPamData == 0) {
                assertFailed(xenoActorAssertExprMotionPackPamData, xenoActorAssertFile, 0x6AE);
            }
            pxySetPartMatrices(actor->pPartsMat);
        }
        if (actor->rsrc[XAct_Rsrc_TEX] != 0) {
            nmlModelSetTexture(XenoActorTexture(actor));
        }
        if ((actor->drawFlags & XENO_DRAW_POST_MASK) || actor->clipPlaneCount != 0) {
            if (actor->drawFlags & XENO_DRAW_POST_FIRST) {
                pxySetShadowFirstPass();
            }
            if (actor->drawFlags & (XENO_DRAW_FOCUS_PENDING | XENO_DRAW_FOCUS_LOCKED)) {
                pxySetFocus(actor->focusX, actor->focusY);
                actor->drawFlags &= ~XENO_DRAW_FOCUS_PENDING;
                actor->drawFlags &= ~XENO_DRAW_FOCUS_LOCKED;
            }
            if (actor->drawFlags & XENO_DRAW_OFFSET_B) {
                pxySetFocus(actor->modeValue2, actor->modeValue3);
            }
            if (actor->drawFlags & XENO_DRAW_SHADOW_PROJECT) {
                nmlModelSetStencil(1);
                nmlModelSetZwrite(1);
                nmlModelSetTransparency(actor->alpha);
                pxySetClipPlaneWithAngle((Vector4Elements *) &actor->clipPlaneA, (Vector4Elements *) &actor->clipPlaneB, (PxyAngle *) actor->clipPlaneC);
            } else if (actor->drawFlags & XENO_DRAW_SHADOW_STENCIL) {
                pxyAddClipPlanePair((PxyRowBits *) &actor->clipPlaneA, (PxyRowBits *) &actor->clipPlaneB);
                pxyAddClipPlanePair((PxyRowBits *) actor->clipPlaneA2, (PxyRowBits *) actor->clipPlaneB2);
                actor->drawFlags &= ~XENO_DRAW_SHADOW_STENCIL;
            }
            if (actor->clipPlaneCount != 0) {
                if (pack->flags & 0x2000) {
                    geometry = (PxyGeometry *) ((char *) pack + pack->geometryOffset);
                    memset(actor->geometryMask, 0, sizeof(actor->geometryMask));
                    for (part = 0; part < pack->geometryCount; part++) {
                        if (geometry->flags & 0x100000) {
                            actor->geometryMask[part >> 5] |= 1 << (part & 0x1F);
                        }
                        geometry++;
                    }
                }
                pxySetClipPlaneMasked((PxyRowBits *) &actor->clipPlaneA, (PxyRowBits *) &actor->clipPlaneB, actor->clipPlaneCount, actor->clipPlaneValue,
                              actor->clipPlaneValue2, actor->clipPlaneValue3, actor->geometryMask);
            }
            if (actor->drawFlags & XENO_DRAW_TRANSLUCENT) {
                pxySetFogWhite();
            }
        }
        renderFlags = actor->renderFlags;
        if (actor->drawFlags & XENO_DRAW_RENDER_2) {
            renderFlags |= XENO_RENDER_FLAG_2;
        }
        subPack = XenoActorPack(actor);
        pxySetAnimTime(actor->renderValue);
        nmlModelSetRenderLevel(renderFlags);
        pxySubmitModel(subPack);
        if (pxyIsModelHidden() != 0) {
            actor->drawFlags |= XENO_DRAW_MODEL_HIDDEN;
        }
        if (subPack->subPackOffset != 0) {
            nmlModelSetParent(subPack);
            nmlModelSetDrawMatrix(localMatrix->m);
            if (actor->drawFlags & XENO_DRAW_COLOR) {
                nmlModelSetFogCol(&actor->fogColor);
                nmlModelSetFogDist(actor->fogDist.x, actor->fogDist.y, actor->fogDist.z, actor->fogDist.w);
            }
            if (actor->drawFlags & XENO_DRAW_LIGHT) {
                pxySetLightPtr((XglLightSet *) &actor->light);
            }
            if (actor->rsrc[XAct_Rsrc_TEX] != 0) {
                nmlModelSetTexture(XenoActorTexture(actor));
            }
            pxySetHideMask(actor->subVisibility);
            pxySetSecondPartMask(actor->block520);
            pxySetAnimTime(actor->renderValue);
            nmlModelSetRenderLevel(renderFlags);
            pxySubmitModel((PxyHeader *) ((char *) subPack + subPack->subPackOffset));
        }
    }
    for (child = 0; child < 8; child++) {
        if (actor->children[child] != 0) {
            XenoActorDrawMotion(actor->children[child]);
        }
    }
}

/* XenoActorReleaseAll: releases every record in the active table. */
void XenoActorReleaseAll(void) {
    int index;
    int activeRecord;

    index = 0;
    if (XenoActorPoolCount > 0) {
        do {
            activeRecord = XenoActorActiveTable[index];
            index += 1;
            if (activeRecord != 0) {
                XenoActorRelease((XenoActor *)activeRecord);
            }
        } while (index < XenoActorPoolCount);
    }
}

void XenoActorDoit(void) {
    int i;

    if (XenoActorHoldFlag == 0) {
        for (i = 0; i < XenoActorPoolCount; ) {
            XenoActor *actor = (XenoActor *)XenoActorActiveTable[i++];
            if (actor != 0 && actor->parent == 0 && (actor->drawFlags & XENO_DRAW_NO_TREE_UPDATE) == 0) {
                XenoActorUpdateTree(actor);
            }
        }
    }
}

/* act_3.c takes the actor as its own ActShadowActor view of XenoActor (a layout view, see there), so this one stays local. */
void ACT_DrawShadow(XenoActor *actor);

/* XenoActorDisp: draws every root actor of the active table, then the shadows when one of them uses draw mode 6 or 7. */
void XenoActorDisp(void) {
    int i;
    int drawShadows = 0;
    XenoActor *actor;
    pxyAbortModel();
    for (i = 0; i < XenoActorPoolCount; i++) {
        actor = (XenoActor *)XenoActorActiveTable[i];
        if (actor != 0 && actor->parent == 0) {
            XenoActorDrawMotion(actor);
            actor = (XenoActor *)XenoActorActiveTable[i];
            if (actor->shadowType == 6 || actor->shadowType == 7) {
                drawShadows = 1;
            }
        }
    }
    if (drawShadows) {
        ACT_DrawShadowBegin();
        for (i = 0; i < XenoActorPoolCount; i++) {
            actor = (XenoActor *)XenoActorActiveTable[i];
            if (actor != 0) {
                ACT_DrawShadow(actor);
            }
        }
        ACT_DrawShadowEnd();
    }
}

/* XenoActorFindPart: index of the part with the given name (strcmp search). */
int strcmp(const char *, const char *);
int XenoActorFindPart(XenoActor *actor, const char *name) {
    PxyHeader *pack;
    PxyGeometry *geom;
    int i;

    if (actor != 0 && name != 0 && (pack = XenoActorPack(actor)) != 0 && pack->magic[0] == 'p' && pack->magic[1] == 'x' && pack->magic[2] == 'y') {
        geom = (PxyGeometry *)((unsigned char *)pack + pack->geometryOffset);
        for (i = 0; i < pack->geometryCount; i++) {
            if (strcmp((const char *)geom, name) == 0) {
                return i;
            }
            geom++;
        }
    }
    return -1;
}

/* XenoActorHidePart: hides a named part (sets its bit in the visibility word). */
int XenoActorHidePart(XenoActor *actor, const char *partName) {
    PxyHeader *pack;
    PxyHeader *subPack;
    PxyGeometry *geom;
    PxyGeometry *subGeom;
    int i;

    if (actor != 0 && partName != 0 && (pack = XenoActorPack(actor)) != 0 && pack->magic[0] == 'p' && pack->magic[1] == 'x' && pack->magic[2] == 'y') {
        geom = (PxyGeometry *)((unsigned char *)pack + pack->geometryOffset);
        if (pack->subPackOffset != 0) {
            subPack = (PxyHeader *)((unsigned char *)pack + pack->subPackOffset);
            subGeom = (PxyGeometry *)((unsigned char *)subPack + subPack->geometryOffset);
            for (i = 0; i < subPack->geometryCount; i++) {
                if (strcmp((const char *)subGeom, partName) == 0) {
                    actor->subVisibility[i >> 5] |= (1 << (i & 31));
                    return 1;
                }
                subGeom++;
            }
        }
        for (i = 0; i < pack->geometryCount; i++) {
            if (strcmp((const char *)geom, partName) == 0) {
                actor->visibility[i >> 5] |= (1 << (i & 31));
                return 1;
            }
            geom++;
        }
    }
    return 0;
}

/* XenoActorShowPart: shows a named part (clears its bit in the visibility word). */
int XenoActorShowPart(XenoActor *actor, const char *partName) {
    PxyHeader *pack;
    PxyHeader *subPack;
    PxyGeometry *geom;
    PxyGeometry *subGeom;
    int i;

    if (actor != 0 && partName != 0 && (pack = XenoActorPack(actor)) != 0 && pack->magic[0] == 'p' && pack->magic[1] == 'x' && pack->magic[2] == 'y') {
        geom = (PxyGeometry *)((unsigned char *)pack + pack->geometryOffset);
        if (pack->subPackOffset != 0) {
            subPack = (PxyHeader *)((unsigned char *)pack + pack->subPackOffset);
            subGeom = (PxyGeometry *)((unsigned char *)subPack + subPack->geometryOffset);
            for (i = 0; i < subPack->geometryCount; i++) {
                if (strcmp((const char *)subGeom, partName) == 0) {
                    actor->subVisibility[i >> 5] &= ~(1 << (i & 31));
                    return 1;
                }
                subGeom++;
            }
        }
        for (i = 0; i < pack->geometryCount; i++) {
            if (strcmp((const char *)geom, partName) == 0) {
                actor->visibility[i >> 5] &= ~(1 << (i & 31));
                return 1;
            }
            geom++;
        }
    }
    return 0;
}

void XenoActorRequestJointShift(XenoActor *actor) {
    actor->flags |= XENO_FLAG_SHIFT_JOINTS;
}

/* XenoActorShiftJoints: adds or subtracts the actor offset to the translation of every joint. */
void XenoActorShiftJoints(XenoActor *actor, int sign, int x, int y, int z) {
    XenoHrc *table = actor->pHrcData;
    u32 i;

    if (table != 0) {
        if (sign > 0) {
            for (i = 0; i < table->dynamicsCount; i++) {
                if (x) { actor->jointRecords[i].translation.vector.x += actor->vector80.x; }
                if (y) { actor->jointRecords[i].translation.vector.y += actor->vector80.y; }
                if (z) { actor->jointRecords[i].translation.vector.z += actor->vector80.z; }
            }
        } else {
            for (i = 0; i < table->dynamicsCount; i++) {
                if (x) { actor->jointRecords[i].translation.vector.x -= actor->vector80.x; }
                if (y) { actor->jointRecords[i].translation.vector.y -= actor->vector80.y; }
                if (z) { actor->jointRecords[i].translation.vector.z -= actor->vector80.z; }
            }
        }
    }
}

/* XenoActorWrapRotation: copies the position and rotation vectors and wraps the three rotation angles into [0, 2*pi). */
void XenoActorWrapRotation(XenoActor *actor) {
    QuadwordCopy(&actor->previousPosition, &actor->position);
    QuadwordCopy(actor->angle, &actor->rotation);
    while (actor->angle[0] < 0.0f) {
        actor->angle[0] += 6.2831855f;
    }
    while (actor->angle[1] < 0.0f) {
        actor->angle[1] += 6.2831855f;
    }
    while (actor->angle[2] < 0.0f) {
        actor->angle[2] += 6.2831855f;
    }
    actor->flags |= XENO_FLAG_SKIP_DELTA;
}

/* XenoActorApplyRootOffset: moves the actor to its transformed position on the selected axes and shifts every joint record back by the actor position on those axes. */
void XenoActorApplyRootOffset(XenoActor *actor, int moveX, int moveY, int moveZ) {
    XenoHrc *table = actor->pHrcData;
    Vector4 point;
    u32 i;

    XenoActorPushMatrix(actor);
    xglCurMatrixSet(actor->localMatrix.m);
    xglCurApplyVector(&point, &actor->vector80);
    if (moveX) {
        actor->position.x = point.x;
    }
    if (moveY) {
        actor->position.y = point.y;
    }
    if (moveZ) {
        actor->position.z = point.z;
    }
    for (i = 0; i < table->dynamicsCount; i++) {
        if (moveX) {
            actor->jointRecords[i].translation.vector.x -= actor->vector80.x;
        }
        if (moveY) {
            actor->jointRecords[i].translation.vector.y -= actor->vector80.y;
        }
        if (moveZ) {
            actor->jointRecords[i].translation.vector.z -= actor->vector80.z;
        }
    }
}

/* XenoActorSetFocusTarget: sets the actor's focus target point, shifts the previous point and draws the joint points when flag 0x100 is set.
   The two vector copies are the original's quadword-transfer inline asm (QuadwordCopy, include/quadword.h). */
void XenoActorSetFocusTarget(XenoActor *actor, Vector4 *target) {
    f32 matrix[4][4];

    tiWordSetBits(&actor->motion[0].flags, 0x10);
    QuadwordCopy(&actor->motion[0].vectors[0], target);
    QuadwordCopy(&actor->motion[0].vectors[1], &actor->motion[0].vectors[2]);
    if (actor->flags & XENO_FLAG_DRAW_POINTS) {
        xglCurSetTranslate(&actor->motion[0].vectors[0]);
        xglCurMatrixGet(matrix);
        tiDrawJointDrawAxes(matrix, 1.5f);
    }
}

void XenoActorSetInitCode(XenoActor *actor, int value) {
    actor->motionPack[1] = value;
}

/* XenoActorGetSlotRange: reads the two bounds of entry idx of the slot's range table as ints; 0 when the entry is missing or its bounds are inverted. */
int XenoActorGetSlotRange(XenoActor *actor, int slot, unsigned int idx, int *start, int *end) {
    OffsetTable *table;
    TiOffsetEntry *range;

    if (actor == 0) {
        assertFailed(xenoActorAssertExpr, xenoActorAssertFile, 0x865);
    }
    table = XenoActorSlotTable(actor, slot);
    if (table == 0) {
        return 0;
    }
    if (idx < table->count) {
        range = (TiOffsetEntry *)((unsigned char *)table + *(table->entryOffsets + idx));
        if (!(range->rangeMin > range->rangeMax)) {
            *start = (int)range->rangeMin;
            *end = (int)range->rangeMax;
            return 1;
        }
    }
    return 0;
}

/* XenoActorGetPoolCount: returns the pool count XenoActorPoolCount. */
int XenoActorGetPoolCount(void) {
    return XenoActorPoolCount;
}

/* Applies the actor's position through its local matrix to id. id is declared int although the callee takes a Vector4 *: the original passes the raw word (the original bytes treat it as an int argument). */
void XenoActorTransformOffset(struct XenoActor *actor, int id) {
    XenoActorPushMatrix(actor);
    xglCurMatrixSet(actor->localMatrix.m);
    xglCurApplyVector((Vector4 *)id, &actor->vector80);
}

/* Quad index table of the drop-shadow contact circle; act_3.c (DrawDropShadowSub) reads it. Defined here. */
const u8 dropShadowContactQuadIndices[56] = {
    4, 0, 1, 6, 1, 6, 7, 2,
    7, 2, 3, 5, 12, 9, 8, 15,
    8, 15, 14, 11, 14, 11, 10, 13,
    0, 4, 8, 12, 4, 1, 12, 9,
    1, 7, 9, 15, 7, 3, 15, 11,
    3, 5, 11, 13, 5, 2, 13, 10,
    2, 6, 10, 14, 6, 0, 14, 8
};
