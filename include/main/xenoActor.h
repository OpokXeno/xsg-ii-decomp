#ifndef INCLUDE_MAIN_XENOACTOR_H
#define INCLUDE_MAIN_XENOACTOR_H

#include "shared.h"
#include "main/undulate.h"

/* The XenoActor of src/main/xenoActor.c and its helper records; the functions of that file that other files call are
 * declared below the types. */

struct XenoActor;
typedef struct XenoActor XenoActor;

typedef struct XenoMatrix {
    float m[4][4];
} XenoMatrix;

/* Per-joint record of an actor; the translation is accessed through the vector union (the original's float/word access). */
typedef struct XenoJointRecord {
    float row0[4];
    DoublewordVector4 translation;          /* 0x10 */
    float rest[8];
} XenoJointRecord;

/* One motion state of the actor's TiMRPackage channel block (TiMRPackage.c's TiMRState, 0x50 bytes). */
typedef struct XenoMotionState {
    int flags;                              /* +0x00: tiWordSetBits/tiWordClearBits */
    float speed;                            /* +0x04: tiMRStateSetSpeed; its sign picks which bound XenoActorSetPlayRange restarts from (state A) */
    float frame;                            /* +0x08: the value the motion package accumulates; read back after it advances */
    float blendFrames;                      /* +0x0C: blend length in frames (XenoActorSetBlendFrames) */
    int range[2];                           /* +0x10: the play range; word 0 of state A is the source value XenoActorApplySourceValue converts */
    unsigned char unmodeled_18[8];
    Vector4 vectors[3];                     /* +0x20: state A keeps the focus target, its previous copy and the current point here (XenoActorSetFocusTarget) */
} XenoMotionState;

/* The 0x180-byte light set of an actor: the battle actors copy it whole (unitCreateSide in src/ov01/unit_cmd.c), and
 * uwamonoLightBlendStep blends the vectors. */
typedef struct XenoLight {
    float directions[3][4];                 /* +0x00: the three light directions */
    unsigned char unmodeled_30[0x70 - 0x30];
    float colors[3][4];                     /* +0x70: the three light colors */
    unsigned char unmodeled_a0[0xD0 - 0xA0];
    float ambient[4];                       /* +0xD0: the ambient color */
    unsigned char unmodeled_e0[0xF0 - 0xE0];
    float fourthDirection[4];               /* +0xF0: the fourth light direction */
    u64 unmodeled_100[(0x12C - 0x100) / 8]; /* copied as doublewords with the rest of the block */
    unsigned char unmodeled_128[4];
    float intensity;                        /* +0x12C */
    u64 unmodeled_130[(0x180 - 0x130) / 8];
} XenoLight;

/* Indices of XenoActor.rsrc: the model pack (.pxy, xenoActor.c reads it as PxyHeader *), the texture (.txy), the
 * hierarchy (.xhr, XenoActorAllocMatrices builds the matrices from it), the motion tables (.xap: slot n is
 * XAct_Rsrc_XAP + n, slots 0..5 = indices 4..9) and a few entries xenoActor.c reads as pointers. The names of the
 * first seven are those of the original's asserts; no assert names the rest. */
enum XActRsrc {
    XAct_Rsrc_MDL,
    XAct_Rsrc_TEX,
    XAct_Rsrc_XHR,
    XAct_Rsrc_3,                            /* entry passed to tiMRPackageSetAnimData */
    XAct_Rsrc_XAP,                          /* OffsetTable * of slot 0 */
    XAct_Rsrc_5,
    XAct_Rsrc_XAP_Ext2,
    XAct_Rsrc_7,
    XAct_Rsrc_SlotEntry,                    /* slot 4, read as an int entry (the motion set's second entry) */
    XAct_Rsrc_SlotTableB,                   /* slot 5, read as another table */
    XAct_Rsrc_PAM,                          /* non-zero when a PAM pose is active */
    XAct_Rsrc_EntryTableC,                  /* tiMRPackage range entry C */
    XAct_Rsrc_Count
};

/* Transform block at +0x70 of the motion hierarchy (the table at actor +0x710). */
typedef struct XenoTransform {
    unsigned char unmodeled_00[4];
    float translation[3];                   /* 0x04 */
    float rotationX;                        /* 0x10 */
    float rotationY;                        /* 0x14 */
    float rotationZ;                        /* 0x18 */
    float scale[3];                         /* 0x1C */
    unsigned char unmodeled_28[0x44 - 0x28];
    float point[3];                         /* 0x44: point the transform is applied to */
} XenoTransform;

/* Motion hierarchy (HRC) of an actor (actor->table). */
typedef struct XenoHrc {
    unsigned char unmodeled_00[0xC];
    unsigned int numTransform;              /* 0x0C */
    int jointCount;                         /* 0x10: the joint counts of the hierarchy (same layout as TiHrcData) */
    int joint2Count;                        /* 0x14 */
    int joint3Count;                        /* 0x18 */
    int skipCount;                          /* 0x1C */
    int dynamicsCount;                      /* 0x20: number of joint records (XenoActor.jointRecords) */
    unsigned int staticCount;               /* 0x24 */
    unsigned char unmodeled_28[0x3C - 0x28];
    unsigned int count;                     /* 0x3C: item count */
    unsigned char unmodeled_40[0x4C - 0x40];
    int dataOffset;                         /* 0x4C: bytes from the table to its items */
    unsigned char unmodeled_50[0x5C - 0x50];
    unsigned int flags;                     /* 0x5C: bit 4 = has a focus joint */
    unsigned char unmodeled_60[0x70 - 0x60];
    XenoTransform transform;                /* 0x70 */
} XenoHrc;

/* The actor of a map object, a battle unit or a cut-scene model: one record of the pool of src/main/xenoActor.c
 * (0x7F0 bytes each). Names of the original's asserts are used where the code shows them. */
struct XenoActor {
    unsigned short attr;                    /* 0x000: bit 2 = has global matrices; 0 marks a free pool record */
    unsigned char unmodeled_002[0x10 - 0x2];
    AlignedVector4 position;                /* 0x010 */
    AlignedVector4 previousPosition;        /* 0x020: copy of the row at 0x10 (x and z before this frame's move: tiWaterUpdateSource) */
    float angle[4];                         /* 0x030: copy of rotation, its three angles wrapped into [0, 2*pi) */
    AlignedVector4 velocity;                /* 0x040: this frame's movement */
    AlignedVector4 rotation;                /* 0x050: y is the facing angle */
    AlignedVector4 direction;               /* 0x060: y is the moving angle */
    AlignedVector4 vector70;                /* 0x070 */
    AlignedVector4 vector80;                /* 0x080 */
    AlignedVector4 scale;                   /* 0x090 */
    unsigned char unmodeled_0a0[0x10];
    float radius;                           /* 0x0B0 */
    float height;                           /* 0x0B4 */
    int enemyFlags;                         /* 0x0B8: the enemy mode bits of ACT_updateEnemy: 0x80000000 follows a route, 0x400000 noticed the player, 0x800000 frozen, 0x1000000 in the cooldown mode */
    int drawFlags;                          /* 0x0BC: 0x4 hides the actor, 0x08000000 draws it translucent */
    float alpha;                            /* 0x0C0: a fade writes its progress, 0 pauses a battle actor */
    int shadowType;                         /* 0x0C4: shadow kind that ACT_DrawShadow (act_3.c) switches on */
    unsigned char unmodeled_0c8[4];
    float drawParam;                        /* 0x0CC: shadow alpha (act_3.c shadow_alpha); uwamono_light_blend.c also writes it as a blend value */
    int renderFlags;                        /* 0x0D0 */
    float renderValue;                      /* 0x0D4 */
    float renderValue2;                     /* 0x0D8 */
    unsigned char lightSource[0x100 - 0xDC]; /* 0x0DC */
    XenoMatrix localMatrix;                 /* 0x100: the matrix unitFaceTarget applies with xglVectorMulMat */
    float focusX;                           /* 0x140: focus offset, damped by cfeDampDirOffset */
    float focusY;                           /* 0x144 */
    float modeValue2;                       /* 0x148 */
    float modeValue3;                       /* 0x14C */
    float sweepPhase;                       /* 0x150: clip-plane sweep phase, advanced by XenoActorUpdateClipSweep and wrapped at 1 */
    float sweepSpeed;                       /* 0x154: sweep speed, its sign is the direction (init 1.0) */
    float clipPlaneValue;                   /* 0x158 */
    int clipPlaneCount;                     /* 0x15C: plane count of pxySetClipPlaneMasked; XenoActorUpdateClipSweep runs while it is set */
    int moveState;                          /* 0x160: clip-plane shape selector of XenoActorUpdateClipSweep (0..7) */
    float clipPlaneValue2;                  /* 0x164 */
    float clipPlaneValue3;                  /* 0x168 */
    float heightOffset;                     /* 0x16C */
    float shadowHeightA;                    /* 0x170: shadow height range, given to pxySetShadowHeightRange */
    float shadowHeightB;                    /* 0x174 */
    float shadowHeightC;                    /* 0x178 */
    float shadowHeightD;                    /* 0x17C */
    float modeValue4;                       /* 0x180 */
    float modeValue5;                       /* 0x184: reset to 0 by XenoActorInit with the other mode values */
    float alphaScale;                       /* 0x188 */
    float fadeRadius;                       /* 0x18C: fade distance override (0 = use the pack radius) */
    XenoLight light;                        /* 0x190 */
    XenoLight sourceLight;                  /* 0x310 */
    DoublewordAlignedVector4 fogDist;       /* 0x490: fog distances (nmlModelSetFogDist); a copy of runtimeFogPtrGet's output, the setFog native writes its args 2-5 / 100 */
    DoublewordAlignedVector4 fogColor;      /* 0x4A0: fog color (nmlModelSetFogCol); setFog args 6-8 */
    DoublewordAlignedVector4 sourceBlock;   /* 0x4B0 */
    u32 visibility[8];                      /* 0x4C0: one bit per geometry of the pack; a cut-scene actor's flag records set them */
    u32 subVisibility[8];                   /* 0x4E0: one bit per geometry of the sub pack */
    u32 geometryMask[8];                    /* 0x500: one bit per shadow-casting geometry */
    unsigned char block520[0x20];           /* 0x520 */
    Vector4 clipPlaneA;                     /* 0x540: first clip plane (pxySetClipPlaneWithAngle) */
    unsigned char clipPlaneA2[0x10];           /* 0x550 */
    Vector4 clipPlaneB;                     /* 0x560: second clip plane */
    unsigned char clipPlaneB2[0x10];           /* 0x570 */
    unsigned char clipPlaneC[0x10];         /* 0x580: rotation angle of the clip planes */
    unsigned char colorIndex[16];           /* 0x590: 0xFF marks a free entry */
    void *overrideMatrix;                   /* 0x5A0 */
    struct XenoActor *parent;               /* 0x5A4: the actor this one is attached to */
    int slotIndex;                          /* 0x5A8 */
    struct XenoActor *children[8];          /* 0x5AC */
    u32 flags;                              /* 0x5CC: see XENO_FLAG_* in xenoActor.c */
    float frame;                            /* 0x5D0: play position (XenoActorUpdateLinkMatrix passes it as the scale); the scene frame tiWaterSceneLoop counts to 120 */
    int mode;                               /* 0x5D4: the motion XenoActorCheckMode compares against */
    int slot;                               /* 0x5D8: mode slot of the current mode (motion number / 100); nonzero keeps unitSetPhase from its early return */
    int requested_mode;                     /* 0x5DC: XenoActorRequestMode stores its argument here */
    float cachedValue;                      /* 0x5E0 */
    int selector;                           /* 0x5E4: the last XenoActorSetSelector value */
    unsigned char unmodeled_5e8[0x5EC - 0x5E8];
    /* The actor's resources, rsrc[XAct_Rsrc_*] in the original's asserts. They hold pointers but are int: with a pointer
     * element type the loads/stores around them (cfeLoadResourceObject's reload of resource->pack after the stores,
     * oLibLoadObject's lw/sw interleave, the stores in uwamono_objx/yg2path/title/unit_cmd) schedule differently, since
     * gcc 2.96 gives int and pointer accesses different alias sets. */
    int rsrc[XAct_Rsrc_Count];              /* 0x5EC */
    unsigned char unmodeled_61c[4];
    int link;                               /* 0x620 */
    unsigned char unmodeled_624[4];
    void *pLocalMat;                        /* 0x628: heap-5 buffers (names from the original asserts), one 0x40-byte matrix per hierarchy transform */
    void *pGlobalMat;                       /* 0x62C */
    void *pBlendMat;                        /* 0x630 */
    void *pPartsMat;                        /* 0x634 */
    void *pDynamics;                        /* 0x638 */
    unsigned char unmodeled_63c[4];
    /* 0x640..0x740: the original's `motionPack` (assert texts "pAct->motionPack.pHrcData / .pGlobalMat / .pPamData"). */
    int motionPack[4];                      /* 0x640: its address is passed to callees (the 0x60-byte channel of XenoActorSetSelector); element 1 is stored by XenoActorSetInitCode */
    XenoMotionState motion[2];              /* 0x650, 0x6A0: the TiMRPackage states A and B */
    unsigned char unmodeled_6f0[0x700 - 0x6F0];
    struct TiOffsetEntry *motionKeys[4];    /* 0x700: one per mode slot; XenoActorSetSelector reads slot 2 */
    XenoHrc *pHrcData;                      /* 0x710: motionPack.pHrcData */
    void *pPamData;                         /* 0x714: motionPack.pPamData */
    unsigned char unmodeled_718[4];
    XenoMatrix *matrices;                   /* 0x71C */
    XenoMatrix *pMotionGlobalMat;           /* 0x720: motionPack.pGlobalMat (the asserts of XenoActorDrawMotion) */
    unsigned char unmodeled_724[0x72C - 0x724];
    XenoJointRecord *jointRecords;          /* 0x72C: one 64-byte record per hierarchy joint (XenoHrc.dynamicsCount of them) */
    XenoMatrix *channelMatrix;              /* 0x730: set to localMatrix when the channel is updated */
    unsigned char unmodeled_734[0x740 - 0x734];
    /* The actor's collision object, the request block of UnduCheck (UnduParamInit resets it). Its members are the actor's own state:
     * flags are the hit flags (+0x748), bits the ground flags (0x10 while on a slope), probeEnabled is set to 1 when a player model
     * is loaded, heightTolerance is the step height, block the map header UnduDataGetHeader returned for the ground, kind the
     * ground status (0x1xxxx slope, 0x1xx ladder, 0x2xx elevator) and radius the form's size (set together with `radius`). */
    MapCollisionObject collision;           /* 0x740 */
    DoublewordVector4 previous;             /* 0x780: the position before this frame */
    unsigned char unmodeled_790[4];
    int moved;                              /* 0x794: the index of the actor's uwamonoRecords entry */
    /* A one-member union: the original reads this field with alias set 0, after the vector copies before it. */
    union {
        int kind;                           /* 0x798: 0x10000..0x13FFF for an ES (the mecha the party rides) */
    };
    int state;                              /* 0x79C: written by XenoActorSetState; XenoActorDrawMotion reads it and clears it */
    short itemCursorA;                      /* 0x7A0: item index, -1 = none (sibling of activeItem); only initialised here */
    short itemCursorB;                      /* 0x7A2: item index, -1 = none; only initialised here */
    short activeItem;                       /* 0x7A4: item index XenoActorRefreshItems compares against its loop counter */
    unsigned char unmodeled_7a6[0x7F0 - 0x7A6]; /* pool record size is 0x7F0 */
};

/* Bit 0x100 of XenoActor.drawFlags: the focus pair is locked (set while cfeDampDirOffset's damped offsets are nonzero). */
#define XENO_DRAW_FOCUS_LOCKED 0x100

void XenoActorInit(struct XenoActor *actor);
void XenoActorInitHeap(int memory, int size);
struct XenoActor *XenoActorPoolGet(int index);
int XenoActorGetPoolCount(void);
int XenoActorAllocMatrices(struct XenoActor *actor);
void XenoActorClearHoldFlag(void);
int XenoActorAttachChild(struct XenoActor *parent, struct XenoActor *child);
int XenoActorAppendChildOnJoint(struct XenoActor *parent, struct XenoActor *child, int joint);
int XenoActorAttachChildToJoint(struct XenoActor *actor, struct XenoActor *child, int jointId);
int XenoActorAttachAtSlot(struct XenoActor *parent, struct XenoActor *actor, int slotIndex);
int XenoActorReleaseChild(struct XenoActor *parent, struct XenoActor *child);
void XenoActorSetFlagAll(int flag);
void XenoActorSetBlendGlobal(struct XenoActor *actor);
void XenoActorSetBlendGlobalB(struct XenoActor *actor);
void XenoActorResetChannelFlags(struct XenoActor *actor);
void XenoActorClearChannel(struct XenoActor *actor);
void XenoActorSetMode(struct XenoActor *actor, int mode);
void XenoActorCheckMode(struct XenoActor *actor, int expected_mode);
void XenoActorRequestMode(struct XenoActor *actor, int mode);
int XenoActorMulMatrix(struct XenoActor *actor, int nodeKey, Matrix4 matrix, int useBaseMatrix);
int XenoActorMulVector(struct XenoActor *actor, int nodeKey, Vector4 *vector, int apply);
void XenoActorSetVector(struct XenoActor *actor, float x, float y, float z);
void XenoActorSetFocusTarget(struct XenoActor *actor, Vector4 *target);
void XenoActorSetInitCode(struct XenoActor *actor, int value);
void XenoActorWrapRotation(struct XenoActor *actor);
int XenoActorGetSlotRange(struct XenoActor *actor, int slot, unsigned int idx, int *start, int *end);
void XenoActorUpdateTree(struct XenoActor *actor);
void XenoActorPushMatrix(struct XenoActor *actor);
void XenoActorShiftJoints(struct XenoActor *actor, int sign, int x, int y, int z);
struct XenoActor *XenoActorAlloc(void);
void XenoActorRelease(struct XenoActor *actor);
void XenoActorSetBlendLocal(struct XenoActor *actor);
void XenoActorSetBlendLocalB(struct XenoActor *actor);
void XenoActorPoolInitDefault(void);
void XenoActorReleaseAll(void);
void XenoActorSetBlendFrames(struct XenoActor *actor, float value);
void XenoActorSetBlendFramesB(struct XenoActor *actor, float value);
int XenoActorIsDone(struct XenoActor *actor);
int XenoActorHidePart(struct XenoActor *actor, const char *partName);
int XenoActorShowPart(struct XenoActor *actor, const char *partName);
void XenoActorApplySourceValue(struct XenoActor *actor);
void XenoActorSetPlayRange(struct XenoActor *actor, int start, int end);
void XenoActorSetSolverPasses(struct XenoActor *actor, int value);
void XenoActorSetModeInSlot(struct XenoActor *actor, unsigned int slot, int mode);
void XenoActorSetSelector(struct XenoActor *actor, int selector);
void XenoActorSetHoldFlag(void);
/* Moves the actor to its transformed position on the selected axes and shifts the joint records back by it. */
void XenoActorApplyRootOffset(struct XenoActor *actor, int moveX, int moveY, int moveZ);
/* 1 when the lower bound of entry idx of the slot's range table does not exceed its upper bound. */
int xenoActorSlotRangeValid(struct XenoActor *actor, int slot, unsigned int idx);
void XenoActorDoit(void);
void XenoActorDisp(void);

/* The package block at 0x640 is typed int motionPack[4] and XenoMotionState motion[2] above; these are its TiMRPackage
 * and TiMRState views (main/TiMRPackage.h), for the calls into TiMRPackage.c. */
#define XenoActorMotionPack(a) ((TiMRPackage *)(a)->motionPack)
#define XenoActorMotionState(a, i) ((TiMRState *)&(a)->motion[i])

/* XenoActor.kind (+0x798) as the resource loaders cfeLoadResourceObject and oLibLoadObject write it: a pointer-typed
 * store through a view that only has this member, so the loads after it move above it as in the original. Through
 * actor->kind, a one-member union read with alias set 0, the stores of the resource words are scheduled before it. */
typedef struct XenoActorKindView {
    unsigned char unmodeled_000[0x798];
    void *kind;
} XenoActorKindView;

#endif /* INCLUDE_MAIN_XENOACTOR_H */
