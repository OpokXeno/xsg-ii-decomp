#include "common.h"
#include "shared.h"
#include "vu0_macro.h"
#include "quadword.h"
#include "gs.h"
#include "main/sdv_033.h"
#include "main/sm_initilize.h"
#include "main/sv_get_image_item.h"
#include "main/tu185.h"
#include "main/xgl_2.h"
#include "main/m_math_193.h"

extern void *memset(void *destination, int value, unsigned int size);

/* A 128-bit quadword (one sq), as sef_169.c spells it. */
typedef unsigned int Quadword __attribute__((mode(TI)));

/* One matrix row as the code writes it: single floats, a doubleword half (sd) or the whole quadword (sq). */
typedef union SefRotationRow {
    f32 element[4];
    u64 halves[2];
    Quadword quad;
} SefRotationRow;

/*
 * The 0xA0-byte particle of sefEntryPool has one layout per effect kind; the words every kind uses keep one
 * name in all three. sefPoolAlloc returns a particle's pool index, 0 when its search finds no free entry.
 */

/* Kinds 0 to 7: spawned by func_0026CF90 (0 to 2), func_0026D2B8 (3 and 4) and sefSpawnKinds5To7 (5 to 7); kinds 0 to 2
 * drift in sefUpdateBurstParticles, kinds 5 to 7 grow and fade in sefUpdateFadeOutParticles. */
typedef struct SefFadeParticle {
    SefRotationRow rotation[3]; /* +0x00: built by sefRotationZRows */
    AlignedVector4 position;    /* +0x30 */
    int red;                    /* +0x40 */
    int green;                  /* +0x44 */
    int blue;                   /* +0x48 */
    int alpha;                  /* +0x4C */
    AlignedVector4 scale;       /* +0x50: xyz the axis scales, w the particle size */
    AlignedVector4 velocity;    /* +0x60: kinds 0 to 2: the step per frame for the first 20 frames */
    f32 life;                   /* +0x70: the particle dies once age reaches it; nonzero while the entry is in use */
    f32 age;                    /* +0x74: frames lived */
    f32 verticalSpeed;          /* +0x78: added to position.y each frame by the kind 3 and 4 update; zeroed on spawn */
    f32 axisOffset;             /* +0x7C: distance along rotation[0] that sefDrawEmitter moves the drawn particle */
    AlignedVector4 target;      /* +0x80: kinds 0 to 2: the step velocity turns towards after frame 20 */
    AlignedVector4 growth;      /* +0x90: per-axis factors around 1.0, w = 1.0 */
} SefFadeParticle;

/* Kinds 8 to 19 and 22 up: they fly along velocity at speed. */
typedef struct SefDriftParticle {
    SefRotationRow rotation[3]; /* +0x00: built by sefRotationZRows or sefRandomRotateMatrix */
    AlignedVector4 position;    /* +0x30 */
    int red;                    /* +0x40 */
    int green;                  /* +0x44 */
    int blue;                   /* +0x48 */
    int alpha;                  /* +0x4C */
    AlignedVector4 scale;       /* +0x50: xyz the axis scales, w the particle size */
    AlignedVector4 velocity;    /* +0x60: unit direction of flight (or already scaled by speed, sefSpawnKinds14To16) */
    f32 life;                   /* +0x70 */
    f32 age;                    /* +0x74 */
    f32 verticalSpeed;          /* +0x78: added to position.y each frame, lowered by a constant (gravity) per frame */
    f32 axisOffset;             /* +0x7C: zeroed on spawn */
    f32 speed;                  /* +0x80: sefSpawnKinds14To16 scales the unit velocity by it */
    int landed;                 /* +0x84: set once the particle reaches the floor (sefFloorHeight), when it bounces */
    unsigned char unmodeled_88[0x90 - 0x88];
    AlignedVector4 growth;      /* +0x90 */
} SefDriftParticle;

/* Kinds 20 and 21: they turn about Z in place (sefUpdateKind20Particles, sefUpdateSpinParticles). */
typedef struct SefSpinParticle {
    SefRotationRow rotation[3]; /* +0x00: rebuilt from angle by sefRotationZRows */
    AlignedVector4 position;    /* +0x30 */
    int red;                    /* +0x40 */
    int green;                  /* +0x44 */
    int blue;                   /* +0x48 */
    int alpha;                  /* +0x4C */
    AlignedVector4 scale;       /* +0x50 */
    unsigned char unmodeled_60[0x70 - 0x60];
    f32 life;                   /* +0x70 */
    f32 age;                    /* +0x74 */
    f32 verticalSpeed;          /* +0x78: zeroed on spawn */
    /* +0x7C: the f32 axisOffset of the other kinds (sefDrawEmitter reads it for every kind), cleared on spawn. Typed
     * int: as f32, sefSpawnKind21 stores the 0.0f from $f20 (swc1) where the original stores $zero (sw). */
    int axisOffset;
    f32 angle;                  /* +0x80: degrees, 0 on spawn */
    unsigned char unmodeled_84[0x90 - 0x84];
    AlignedVector4 growth;      /* +0x90 */
} SefSpinParticle;

/* Any kind as sefPoolAlloc sees it: it tests the life word's bits (lw, beqz), zero (0.0f) for a free entry. */
typedef struct SefPoolRecord {
    u8 unmodeled_00[0x70];
    int life;                   /* +0x70 */
    u8 unmodeled_74[0xA0 - 0x74];
} SefPoolRecord;

/* One entry of sefEntryPool. The kind 8-21 spawners and the group updates reach the particle through the
 * union (a union access may alias anything, so gcc keeps the particle stores and the age reloads in the
 * original's order); sefSpawnKinds5To7 and sefUpdateSpinParticles take a pointer to their kind's member. */
typedef union SefPoolEntry {
    SefFadeParticle fade;
    SefDriftParticle drift;
    SefSpinParticle spin;
    SefPoolRecord record;
    Matrix4 matrix;             /* every kind: rotation rows 0 to 2 and the position as row 3 (sefDrawEmitter) */
} SefPoolEntry;

/* The particle pool (0x500B0 bytes, cleared by sefResetPool). Entry 0 is never handed out: sefPoolAlloc returns
 * 0 for no free entry. The SEF_POOL_RING_SIZE entries after it form a ring that the search for a free one walks. */
#define SEF_POOL_RING_SIZE 2048

typedef struct SefPool {
    SefPoolEntry entries[SEF_POOL_RING_SIZE + 1];
    int cursor;                 /* +0x500A0: where the next search starts */
} SefPool;

static Matrix4 sefViewCameraMatrix;
static SefPool sefEntryPool;

void sefResetPool(void) {
    memset(&sefEntryPool, 0, sizeof(SefPool));
}

extern unsigned char sefSmallHeap[];
void smInitilizeSmall(void *cell, unsigned int size);

void sefResetBuffers(void) {
    smInitilizeSmall(sefSmallHeap, 0x20000);
    sefResetPool();
}

/* Set when sefPoolAlloc finds no free entry; sefRunEmitters clears it every frame. */
s16 sefPoolFull = 0;

/* Returns the sefEntryPool index of a free entry (ring entry r is entries[r + 1]), searching the ring from the
 * cursor; the cursor moves to the ring entry after the one handed out. Returns 0 and sets sefPoolFull when no
 * free entry is found: the search stops at the ring entry just before the start, so it looks at 2047 of the 2048.
 * The free test reads life through SefPoolRecord: the plain `entry->fade.life == 0.0f` compiles to a float
 * compare (mtc1, lwc1, c.eq.s), the original tests the word (lw, beqz). The cursor is stored through poolCursor:
 * with `pool->cursor = ...` gcc folds the 0x500A0 offset into the store instead of keeping its address in a
 * register as the original does. */
int sefPoolAlloc(void) {
    SefPool *pool;
    SefPoolEntry *ring;
    SefPoolEntry *entry;
    int *poolCursor;
    int start;
    int last;
    int index;

    pool = &sefEntryPool;
    ring = &pool->entries[1];
    start = pool->cursor;
    poolCursor = &pool->cursor;
    last = (start - 1) & (SEF_POOL_RING_SIZE - 1);
    if (sefPoolFull != 0) {
        return 0;
    }
    for (index = start; index != last; index &= SEF_POOL_RING_SIZE - 1) {
        entry = &ring[index++];
        if (entry->record.life == 0) {
            *poolCursor = index & (SEF_POOL_RING_SIZE - 1);
            return index;
        }
    }
    sefPoolFull = 1;
    return 0;
}

/* VIF UNPACK code: CMD 0x6C (V4-32, USN set), NUM = count in bits 16-23, the low word 0x8000 fixed. */
#define VIF_UNPACK_V4_32_USN 0x6C008000U

/* The first data quadword of a block. The same 16 bytes are written at two widths: sefUnpackBlockInit clears the
 * whole quadword (sq) and then the count in its first word (sw), sefPacketUnpackRef writes the count (sw).
 * Flat fields (entryCount plus three zeroed words) compile to four sw instead of the original's por/sq. */
typedef union SefUnpackData {
    Quadword quad;
    unsigned int entryCount;  /* the particles in the block */
} SefUnpackData;

/* The head of a quadword block that sefDrawEmitter builds and sends by reference: sefUnpackBlockInit zeroes the
 * first VIF codes and writes a one-quadword UNPACK, which sefPacketUnpackRef widens to the whole block. */
typedef struct SefUnpackBlock {
    unsigned int vifNop[3];   /* +0x00: zero VIF codes (NOP) */
    unsigned int unpackCode;  /* +0x0C: VIF UNPACK V4-32 of the block's data */
    SefUnpackData data;       /* +0x10 */
} SefUnpackBlock;

/* Starts a block: zeroes its VIF NOPs, writes a one-quadword UNPACK and clears the first data quadword and its
 * count. */
void sefUnpackBlockInit(SefUnpackBlock *block) {
    block->vifNop[0] = 0;
    block->vifNop[1] = 0;
    block->vifNop[2] = 0;
    block->unpackCode = VIF_UNPACK_V4_32_USN | (1 << 16);
    block->data.quad = 0;
    block->data.entryCount = 0;
}

typedef struct XglPacket XglPacket;

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern XglPacket *xglPacketGetCurrent(void);
void sceVif1PkAddCode(XglPacket *packet, unsigned int tag);

/* VIF codes (command in bits 24..30): FLUSH, MSCNT, and MSCAL whose immediate is the micro program address / 8. */
#define VIF_CODE_FLUSH 0x11000000
#define VIF_CODE_MSCAL 0x14000000
#define VIF_CODE_MSCNT 0x17000000

void sefPacketFlush(void) {
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, VIF_CODE_FLUSH);
}

void sefPacketMscnt(void) {
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, VIF_CODE_MSCNT);
}

extern void sceVif1PkRef(XglPacket *packet, const void *environment, int count, int mode, int offset, int flags);

/* Keeps the physical address of an EE pointer: DMA references take physical addresses (sdv_181.c's name). */
#define SDV_PHYSICAL_ADDRESS_MASK 0x0FFFFFFF

/* blockSize (always 0x28 at every caller) is read nowhere here. */
void sefPacketUnpackRef(SefUnpackBlock *block, int blockSize, int count, unsigned int entryCount) {
    const void *environment;
    XglPacket *currentPacket;

    environment = (const void *) ((unsigned int) block & SDV_PHYSICAL_ADDRESS_MASK);
    currentPacket = xglPacketGetCurrent();
    sefPacketMscnt();
    block->data.entryCount = entryCount;
    block->unpackCode = VIF_UNPACK_V4_32_USN | ((unsigned int) (count - 1) << 16);
    sceVif1PkRef(currentPacket, environment, count, 0, 0, 0);
}

/* Opens a VIF UNPACK of the following data: VU address, UNPACK command, cycle and write lengths (tu045.c calls it
   too). */
void func_0012B6E0(XglPacket *packet, int vuAddress, int unpackCommand, int cycleLength, int writeLength);
void func_0012B770(XglPacket *packet);
void func_0012BCE8(XglPacket *packet, const void *data, int count);
/* The corners of a unit quad, (x, y, 0, 1) for x, y = -1, 1, as vectors and as the eight doublewords
 * sefBuildCameraPacket copies. */
typedef union SefQuadCorners {
    DoublewordAlignedVector4 corner[4];
    u64 words[8];
} SefQuadCorners;

/* The VU packet at sefVuPacket: sefBuildVuPacket sends 0x11 quadwords from +0x00, 0xD from +0x40
 * or 3 from +0xE0. */
typedef struct SefVuPacket {
    SefQuadCorners corners;   /* +0x00: copy of sefQuadCorners (sefBuildCameraPacket) */
    Matrix4 modelViewMatrix;  /* +0x40: *sefViewMatrixPtr x the model matrix (sefBuildModelPacket) */
    Matrix4 cameraMatrix;     /* +0x80: copy of sefViewCameraMatrix (sefBuildModelPacket) */
    u64 stripTag;             /* +0xC0: GIF tag: NLOOP 1, EOP, PRIM 0x54 (textured, blended triangle strip), NREG 13 */
    u64 stripRegisters;       /* +0xC8: its registers: A+D, then ST, RGBAQ and XYZ2 for four vertices */
    u64 settingsTag;          /* +0xD0: GIF tag of the three A+D pairs from tex0 on: NLOOP 3, EOP, NREG 1 */
    u64 settingsRegisters;    /* +0xD8: GIF_AD */
    u64 tex0;                 /* +0xE0: A+D TEX0_1 value */
    u64 tex0Register;         /* +0xE8: GS_TEX0_1 */
    u64 alpha;                /* +0xF0: A+D ALPHA_1 value */
    u64 alphaRegister;        /* +0xF8: GS_ALPHA_1 */
    u64 test;                 /* +0x100: A+D TEST_1 value (sefSetupEmitterPacket) */
    u64 testRegister;         /* +0x108: GS_TEST_1 */
} SefVuPacket;

static SefVuPacket sefVuPacket;
extern unsigned char D_1F70[];
extern unsigned char D_1FD0[];

void sefBuildVuPacket(int kind, int alternate) {
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    if (kind == 0) {
        func_0012B6E0(packet, 0x14, 0x6C, 4, 4);
        func_0012BCE8(packet, &sefVuPacket, 0x11);
    } else {
        if (alternate != 0) {
            sceVif1PkAddCode(packet, ((u32) D_1FD0 >> 3) | VIF_CODE_MSCAL);
        } else {
            sceVif1PkAddCode(packet, ((u32) D_1F70 >> 3) | VIF_CODE_MSCAL);
        }
        if (kind == 1) {
            func_0012B6E0(packet, 0x18, 0x6C, 4, 4);
            func_0012BCE8(packet, &sefVuPacket.modelViewMatrix, 0xD);
        } else {
            func_0012B6E0(packet, 0x22, 0x6C, 4, 4);
            func_0012BCE8(packet, &sefVuPacket.tex0, 3);
        }
    }
    func_0012B770(packet);
    sceVif1PkAddCode(packet, VIF_CODE_FLUSH);
}

Matrix4 *MMathMulMatrix(Matrix4 *out, const Matrix4 *rhs, const Matrix4 *lhs);
static const Matrix4 *sefViewMatrixPtr;
static int sefLastDrawnKind;
static f32 sefFloorHeight;
XglCamera *xglStudioGetActiveCamera(void);
extern Matrix4 sefCameraMatrix[1];
/* The unit quad (rodata). Declared without const: as a const object its loads cannot conflict with the packet stores,
 * and gcc then schedules the epilogue's ld $31 after the fourteen stores instead of before them (fd 53/70). */
extern SefQuadCorners sefQuadCorners;

/* Takes the active camera's view matrix (sefViewMatrixPtr) into the VU packet and sefViewCameraMatrix (view x
   sefCameraMatrix),
 * and fills the packet's quad corners, GIF tags and depth test. The corners are copied as doublewords through the
 * union: as one struct assignment the tag stores go out before the epilogue's ld $31 (fd 55/70). */
void sefBuildCameraPacket(void) {
    XglCamera *camera;

    camera = xglStudioGetActiveCamera();
    sefViewMatrixPtr = (const Matrix4 *) &camera->volume[0].viewMatrix;
    QuadwordCopyMatrixT0(&sefVuPacket.modelViewMatrix, sefViewMatrixPtr);
    MMathMulMatrix(&sefViewCameraMatrix, sefViewMatrixPtr, (const Matrix4 *) sefCameraMatrix);
    sefVuPacket.stripTag = 0xD02A400000008001ULL;
    sefVuPacket.stripRegisters = 0x000512512512512EULL;
    sefVuPacket.settingsTag = 0x1000000000008003ULL;
    sefVuPacket.settingsRegisters = GIF_AD;
    sefVuPacket.test = GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 2);
    sefVuPacket.testRegister = GS_TEST_1;
    sefVuPacket.corners.words[0] = sefQuadCorners.words[0];
    sefVuPacket.corners.words[1] = sefQuadCorners.words[1];
    sefVuPacket.corners.words[2] = sefQuadCorners.words[2];
    sefVuPacket.corners.words[3] = sefQuadCorners.words[3];
    sefVuPacket.corners.words[4] = sefQuadCorners.words[4];
    sefVuPacket.corners.words[5] = sefQuadCorners.words[5];
    sefVuPacket.corners.words[6] = sefQuadCorners.words[6];
    sefVuPacket.corners.words[7] = sefQuadCorners.words[7];
}

/* GS ALPHA_1 values: [0] 0x44 normal blend, [1] 0x48 additive (rodata, also read by sdvDrawAlter). */
extern const u64 sefAlphaBlendValues[];

/*
 * Fills the packet's model-view and camera matrices and its TEX0_1/ALPHA_1 A+D pairs for emitter emitterIndex.
 * Additive blending for blend kinds 3-4; 5-7 for even emitter indices; 11-13 for emitters 0 and 3; 14-16 for
 * emitters 0 and 1; 17-19 for emitter 2; 20 for emitter 1. Normal blending otherwise. Each test keeps its own
 * store, as the original's one block per test shows.
 */
void sefBuildModelPacket(u64 tex0, const Matrix4 *matrix, int blendKind, int emitterIndex) {
    MMathMulMatrix(&sefVuPacket.modelViewMatrix, sefViewMatrixPtr, matrix);
    QuadwordCopyMatrixT0(&sefVuPacket.cameraMatrix, &sefViewCameraMatrix);
    sefVuPacket.tex0 = tex0;
    sefVuPacket.tex0Register = GS_TEX0_1;
    if (blendKind >= 3 && blendKind <= 4) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else if (blendKind >= 5 && blendKind <= 7) {
        if (emitterIndex & 1) {
            sefVuPacket.alpha = sefAlphaBlendValues[0];
        } else {
            sefVuPacket.alpha = sefAlphaBlendValues[1];
        }
    } else if (blendKind >= 11 && blendKind <= 13 && (emitterIndex == 0 || emitterIndex == 3)) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else if (blendKind >= 14 && blendKind <= 16 && emitterIndex < 2) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else if (blendKind >= 17 && blendKind <= 19 && emitterIndex == 2) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else if (blendKind == 20 && emitterIndex == 1) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else {
        sefVuPacket.alpha = sefAlphaBlendValues[0];
    }
    sefVuPacket.alphaRegister = GS_ALPHA_1;
}

int sefActiveCount = 0;
float D_0069DC50 = 0.2f;
float D_0069DC54 = 0.1f;

int sefGetActiveCount(void) {
    return sefActiveCount;
}

/* One 0x28-byte emitter of a resource slot: it spawns particles into its ring of pool indices
 * (sefSpawnHandlers per kind) and its group update ages them (sefUpdateHandlers per kind). */
typedef struct SefEmitter {
    u64 tex0;                  /* +0x00: GS TEX0_1 value of the emitter's image (sefSetupEmitterPacket) */
    unsigned short *particles; /* +0x08: ring of sefEntryPool indices, particleCount long; 0 for a free entry */
    f32 elapsed;               /* +0x0C: frames run; truncated to the frame number */
    f32 delay;                 /* +0x10: frames left before the emitter runs, counted down by sefRunEmitters */
    int frame;                 /* +0x14: the last whole frame handled; -1 until the first */
    short cursor;              /* +0x18: where the search for a free ring entry starts */
    short liveCount;           /* +0x1A: activity count of the last update: its live particles (kind 20 always 1) */
    short spawned;             /* +0x1C: set by the emitters that spawn only once */
    short createdFlag;         /* +0x1E: set to 1 when the slot is created; no reader in main, ov01 or ov02 */
    short clearedOnCreate[2];  /* +0x20: zeroed when the slot is created; no reader in main, ov01 or ov02 */
    u8 unmodeled_24[0x28 - 0x24];
} SefEmitter;

/* A texture and its palette (image numbers in the effect's type list) for one emitter. */
typedef struct SefImagePair {
    short image;
    short clut;
} SefImagePair;

/* The part of a resource slot from +0x40 on (offsets below are slot offsets). sefSetSlotFlags walks the
 * table through this part: its loop pointer starts at sefResourceTable + 0x40. */
typedef struct ResourceBody {
    int resetFlag;   /* +0x40: set to 1 by sefReleaseCellSlot after the slot is cleared */
    int flags;       /* +0x44: bit flags (RESOURCE_FLAG_*), set by sefSetSlotFlags and sefObjectSetFlags */
    /* +0x48: nonzero while the slot is in use. sefCreateResourceSlot stores its pairs argument here (the caller's
     * stack array, so a slot created with no pairs stays free); it is only tested against 0, never dereferenced. */
    const SefImagePair *activePairs;
    u8 unmodeled_4c[0x50 - 0x4c];
    /* +0x50: the owning actor or record (tu067.c passes pointers to sefSetRecordOwner); saved with the slot */
    void *owner;
    u8 unmodeled_54[0x58 - 0x54];
    /* +0x58: saved with the slot and written back by sefRestoreEffectRecords; no reader here. A pointer like owner:
     * as an int, sefCollectPersistentSlots's field copies come out in another order. */
    void *savedWord;
    u8 unmodeled_5c[0x6c - 0x5c];
    s16 typeListIndex; /* +0x6C: image type list of the effect (sefFindRecordIndex), passed to svGetImageListItem */
    s16 visible;     /* +0x6E: set to 1 when the slot is set up; zero skips drawing */
    u8 unmodeled_70[0x74 - 0x70];
    int key;         /* +0x74: the owner key that releases or flags the slot */
    u8 unmodeled_78[0x80 - 0x78];
    Matrix4 matrix;  /* +0x80: the slot's model matrix, handed to sefBuildModelPacket */
    SefEmitter emitters[5]; /* +0xC0 */
    /* +0x188: this slot's particle-index rings inside the small-cell heap (particleCount entries per
     * emitter), nonzero while the slot owns cells; sefReleaseCellSlot frees it with smFreeHandle(void *). */
    unsigned short *handle;
    int particleCount; /* +0x18C: entries of each emitter's ring (a power of two) */
    int emitterCount;  /* +0x190: emitters in use: the loop bound of sefRunEmitters and sefDrawResources */
    s16 kind;          /* +0x194: effect kind: selects the handlers in sefSpawnHandlers and sefUpdateHandlers */
    s16 liveCount;     /* +0x196: sum of the emitters' liveCount, recounted by sefRunEmitters */
    s16 clearedOnCreate; /* +0x198: zeroed when the slot is created; no reader in main, ov01 or ov02 */
    u8 unmodeled_19a[0x1a0 - 0x19a];
} ResourceBody;

/* ResourceBody.flags bits read by sefRunEmitters (SUSPENDED is also set and cleared by sefResetSlotRecords). */
#define RESOURCE_FLAG_NO_DEPTH_TEST 0x4    /* sefSetupEmitterPacket draws the slot with the depth test off */
#define RESOURCE_FLAG_PAUSED 0x80          /* the spawn handler is skipped and an empty slot is kept */
#define RESOURCE_FLAG_SUSPENDED 0x400000   /* the slot is not updated at all */
#define RESOURCE_FLAG_PERSISTENT 0x4000000 /* an empty slot is kept */

/* One 0x1A0-byte resource slot of sefResourceTable; only the fields the code touches are named. */
typedef struct ResourceSlot {
    Vector4 position;  /* +0x00: w set to 1.0f per draw by sefDrawResources */
    Vector4 rotation;  /* +0x10 */
    Vector4 scale;     /* +0x20 */
    Vector4 tint;      /* +0x30: colour multiplier of the drawn particles (lqc2 vf15 in sefDrawEmitter);
                        * 1, 1, 1 draws them untinted; the slot is created with it set to ones */
    ResourceBody body;
} ResourceSlot;

/* The slot table: SEF_RESOURCE_SLOT_COUNT slots of 0x1A0 bytes. */
#define SEF_RESOURCE_SLOT_COUNT 192

extern ResourceSlot sefResourceTable[SEF_RESOURCE_SLOT_COUNT];

/* Clears the active count (sefActiveCount) and the whole slot table. */
void sefResetResourceTable(void) {
    sefActiveCount = 0;
    memset(sefResourceTable, 0, sizeof(sefResourceTable));
}

void sefReleaseByKey(int key);

void sefResetAll(void) {
    sefReleaseByKey(-1);
    sefResetBuffers();
    sefResetResourceTable();
}

void sefReleaseCellSlot(ResourceSlot *slot);

/* Releases every active slot whose key matches (a negative key matches all), then clears the active count. */
void sefReleaseByKey(int key) {
    int index;

    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        if (sefResourceTable[index].body.activePairs != 0 && sefResourceTable[index].body.handle != 0
            && (key < 0 || key == sefResourceTable[index].body.key)) {
            sefReleaseCellSlot(&sefResourceTable[index]);
        }
    }
    sefActiveCount = 0;
}

/* Sets flagBits in every active slot whose key matches; a negative key matches all. */
void sefSetSlotFlags(int key, int flagBits) {
    ResourceSlot *table;
    int index;
    ResourceBody *body;

    table = sefResourceTable;
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        body = &table[index].body;
        if (body->activePairs != 0 && body->handle != 0 && (key < 0 || key == body->key)) {
            body->flags |= flagBits;
        }
    }
}

/* Clears flagBits in every active slot whose key matches; a negative key matches all. */
void sefClearSlotFlags(int key, int flagBits) {
    ResourceSlot *table;
    int index;
    ResourceBody *body;

    table = sefResourceTable;
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        body = &table[index].body;
        if (body->activePairs != 0 && body->handle != 0 && (key < 0 || key == body->key)) {
            body->flags &= ~flagBits;
        }
    }
}

/* Frees every particle the slot's rings still hold: the pool entry's life goes to 0 and the ring entry with it. */
void sefFreeSlotCells(ResourceSlot *slot) {
    int i;

    for (i = 0; i < slot->body.particleCount * slot->body.emitterCount; i++) {
        if (slot->body.handle[i] != 0) {
            sefEntryPool.entries[slot->body.handle[i]].fade.life = 0.0f;
            slot->body.handle[i] = 0;
        }
    }
}

/* Stores (1, 1, 1, 1) through vector: vf0.w broadcast into all four lanes of vf1. */
static inline void sefSetOnes(AlignedVector4 *vector) {
    __asm__ __volatile__(
        "vmaxw.xyzw vf1, vf0, vf0w\n"
        "sqc2 vf1, 0(%0)\n"
        :
        : "r" (vector)
        : "memory");
}

extern const Matrix4 MMathUnitMatrix;

/* Loads MMathUnitMatrix into destination: rows 0..2 through vf3..vf1, row 3 stored from vf0 (0, 0, 0, 1). */
static inline void sefLoadUnitMatrix(Matrix4 *destination) {
    __asm__ __volatile__(
        "lqc2 vf1, 32(%1)\n"
        "lqc2 vf2, 16(%1)\n"
        "lqc2 vf3, 0(%1)\n"
        "sqc2 vf0, 48(%0)\n"
        "sqc2 vf1, 32(%0)\n"
        "sqc2 vf2, 16(%0)\n"
        "sqc2 vf3, 0(%0)\n"
        :
        : "r" (destination), "r" (&MMathUnitMatrix)
        : "memory");
}

/* Particles in each emitter ring, per effect kind. */
extern short sefRingSizes[];
int sefFindRecordIndex(int key);

void *svGetImageListItem(int type, unsigned int flags);

/* Creates a resource slot for effectId: the first free slot from slotIndex on (0 when negative; a busy
 * slotIndex fails), with emitterCount emitters drawing pairs. Ids 0x100C83A7 and 0x100C83A8 become
 * 0x100C8399 and 0x100C839A at five times the scale. Returns the slot, 0 on failure. firstBlock, the first
 * quadword block sefSpawnEffectRecord passes on, is not read. */
ResourceSlot *sefCreateResourceSlot(int effectId, SefImagePair *pairs, int kind, AlignedVector4 *position,
                                    unsigned char *firstBlock, AlignedVector4 *scale, int emitterCount, int slotIndex,
                                    f32 delayStep) {
    int enlarged;
    int size;
    unsigned short *rings;
    ResourceSlot *slot;
    SefEmitter *emitter;
    SvTypeListEntry *image;
    SvTypeListEntry *clut;
    int typeListIndex;
    int index;
    int emitterIndex;
    int psm;
    const SefImagePair *busy;

    size = sefRingSizes[kind] * emitterCount * 2;
    enlarged = 0;
    rings = smAllocSmall(size);
    if (rings == 0) {
        return 0;
    }
    if (effectId == 0x100C83A7) {
        effectId = 0x100C8399;
        enlarged = 1;
    } else if (effectId == 0x100C83A8) {
        effectId = 0x100C839A;
        enlarged = 1;
    }
    index = 0;
    if (slotIndex >= 0) {
        busy = sefResourceTable[slotIndex].body.activePairs;
        index = slotIndex;
        if (busy != 0) {
            return 0;
        }
    }
    for (; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        slot = &sefResourceTable[index];
        if (slot->body.activePairs == 0) {
            typeListIndex = sefFindRecordIndex(0x100C8384);
            memset(rings, 0, size);
            memset(slot, 0, sizeof(ResourceSlot));
            slot->body.activePairs = pairs;
            slot->body.key = effectId;
            slot->body.particleCount = sefRingSizes[kind];
            slot->body.handle = rings;
            slot->body.kind = kind;
            slot->body.typeListIndex = typeListIndex;
            slot->body.resetFlag = 1;
            slot->body.emitterCount = emitterCount;
            slot->body.clearedOnCreate = 0;
            slot->body.liveCount = 0;
            slot->body.visible = 1;
            sefSetOnes((AlignedVector4 *) &slot->tint);
            Vu0SetOrigin(&slot->rotation);
            if (position != 0) {
                QuadwordCopyT0(&slot->position, position);
            } else {
                Vu0SetOrigin(&slot->position);
            }
            if (scale != 0) {
                QuadwordCopyT0(&slot->scale, scale);
            } else {
                sefSetOnes((AlignedVector4 *) &slot->scale);
            }
            if (enlarged) {
                Vu0SefScaleVectorXYZNI(&slot->scale, &slot->scale, 5.0f);
            }
            emitterIndex = 0;
            if (emitterCount > 0) {
                /* The id is stored in the key above; from here on effectId holds the address of the current
                 * emitter. With a separate `SefEmitter *` cursor (emitter = slot->body.emitters, emitter++) gcc
                 * swaps the slot and cursor registers ($17/$18, fd 193/252 in place). */
                effectId = (int) slot->body.emitters;
                do {
                    emitter = (SefEmitter *) effectId;
                    emitter->frame = -1;
                    emitter->cursor = 0;
                    emitter->spawned = 0;
                    emitter->createdFlag = 1;
                    emitter->clearedOnCreate[0] = 0;
                    emitter->delay = delayStep * emitterIndex;
                    emitter->clearedOnCreate[1] = 0;
                    emitter->elapsed = 0;
                    emitter->particles = rings + slot->body.particleCount * emitterIndex;
                    if (pairs != 0) {
                        image = svGetImageListItem(typeListIndex, pairs[emitterIndex].image);
                        clut = svGetImageListItem(typeListIndex, pairs[emitterIndex].clut);
                        if (image != 0 && clut != 0) {
                            psm = image->bitsPerPixel == 8 ? GS_PSMT8 : GS_PSMT4;
                            emitter->tex0 = GS_SET_TEX0(image->basePointer, image->bufferWidthPixels >> 6, psm,
                                                        image->textureWidthExponent, image->textureHeightExponent, 1, 0,
                                                        clut->basePointer, 0, 0, 0, 1);
                        }
                    }
                    effectId += sizeof(SefEmitter);
                    emitterIndex++;
                } while (emitterIndex < emitterCount);
            }
            sefLoadUnitMatrix(&slot->body.matrix);
            if (slot->body.kind < 5 || (slot->body.kind >= 20 && slot->body.kind <= 21)) {
                slot->body.flags |= RESOURCE_FLAG_PERSISTENT;
            }
            slot->body.flags |= 2;
            return slot;
        }
    }
    if (rings != 0) {
        smFreeHandle(rings);
    }
    return 0;
}

/* Frees the particles of a slot that owns cells and resets its emitters' frame, elapsed time and delay (the one-shot
 * spawned latch is kept), then sets or clears its suspended flag. */
void sefResetSlotRecords(ResourceSlot *slot, int suspend) {
    int index;

    if (slot != 0 && slot->body.handle != 0) {
        sefFreeSlotCells(slot);
        for (index = 0; index < slot->body.emitterCount; index++) {
            slot->body.emitters[index].frame = -1;
            slot->body.emitters[index].elapsed = 0.0f;
            slot->body.emitters[index].delay = 0.0f;
        }
        if (suspend) {
            slot->body.flags |= RESOURCE_FLAG_SUSPENDED;
        } else {
            slot->body.flags &= ~RESOURCE_FLAG_SUSPENDED;
        }
    }
}

void sefObjectSetFlags(ResourceSlot *slot, int mask) {
    if (slot != 0 && slot->body.handle != 0) {
        slot->body.flags |= mask;
    }
}

void sefObjectClearFlags(ResourceSlot *slot, int mask) {
    if (slot != 0 && slot->body.handle != 0) {
        slot->body.flags &= ~mask;
    }
}

void sefReleaseCellSlot(ResourceSlot *slot) {
    if (slot != 0 && slot->body.handle != 0) {
        sefFreeSlotCells(slot);
        smFreeHandle(slot->body.handle);
        memset(slot, 0, sizeof(ResourceSlot));
        slot->body.resetFlag = 1;
    }
}

/* One 12-byte entry of an effect's image list. */
typedef struct SefImageSource {
    unsigned short unmodeled_00[2];
    unsigned short image;   /* +0x04 */
    unsigned short clut;    /* +0x06 */
    unsigned short unmodeled_08[2];
} SefImageSource;

/* Picks the image pairs and emitter count for an effect kind (ids 0x100C83A7 and 0x100C83A8 force kinds 20
 * and 21) and creates its resource slot through sefCreateResourceSlot (firstBlock is only passed on). */
ResourceSlot *sefCreateKindResourceSlot(int effectId, SefImageSource *sources, int kind, AlignedVector4 *position,
                                        unsigned char *firstBlock, AlignedVector4 *scale, int slotIndex) {
    SefImagePair pairs[5];

    if (effectId == 0x100C83A7) {
        kind = 20;
    }
    if (effectId == 0x100C83A8) {
        kind = 21;
    }
    if (kind < 0) {
        return 0;
    }
    if (kind < 3) {
        pairs[0].image = sources[0].image;
        pairs[0].clut = sources[0].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, firstBlock, scale, 1, slotIndex, 0.0f);
    } else if (kind == 3) {
        pairs[0].image = sources[1].image;
        pairs[0].clut = sources[1].clut;
        pairs[1].image = sources[2].image;
        pairs[1].clut = sources[2].clut;
        return sefCreateResourceSlot(effectId, pairs, 3, position, firstBlock, scale, 2, slotIndex, 0.0f);
    } else if (kind == 4) {
        pairs[0].image = sources[1].image;
        pairs[0].clut = sources[1].clut;
        pairs[1].image = sources[2].image;
        pairs[1].clut = sources[2].clut;
        return sefCreateResourceSlot(effectId, pairs, 4, position, firstBlock, scale, 2, slotIndex, 0.0f);
    } else if (kind >= 5 && kind <= 7) {
        pairs[0].image = sources[3].image;
        pairs[0].clut = sources[3].clut;
        pairs[1].image = sources[0].image;
        pairs[1].clut = sources[0].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, firstBlock, scale, 2, slotIndex, 1.0f);
    } else if (kind >= 8 && kind <= 10) {
        pairs[0].image = sources[0].image;
        pairs[0].clut = sources[0].clut;
        pairs[1].image = sources[5].image;
        pairs[1].clut = sources[5].clut;
        pairs[2].image = sources[4].image;
        pairs[2].clut = sources[4].clut;
        pairs[3].image = sources[0].image;
        pairs[3].clut = sources[0].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, firstBlock, scale, 4, slotIndex, 1.0f);
    } else if (kind >= 11 && kind <= 13) {
        pairs[0].image = sources[3].image;
        pairs[0].clut = sources[3].clut;
        pairs[1].image = sources[0].image;
        pairs[1].clut = sources[0].clut;
        pairs[2].image = sources[7].image;
        pairs[2].clut = sources[7].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, firstBlock, scale, 3, slotIndex, 1.0f);
    } else if (kind >= 22 && kind <= 24) {
        pairs[0].image = sources[0].image;
        pairs[0].clut = sources[0].clut;
        pairs[1].image = sources[7].image;
        pairs[1].clut = sources[7].clut;
        pairs[2].image = sources[7].image;
        pairs[2].clut = sources[7].clut;
        pairs[3].image = sources[0].image;
        pairs[3].clut = sources[0].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, firstBlock, scale, 4, slotIndex, 1.0f);
    } else if (kind >= 14 && kind <= 16) {
        pairs[0].image = sources[8].image;
        pairs[0].clut = sources[8].clut;
        pairs[1].image = sources[9].image;
        pairs[1].clut = sources[9].clut;
        pairs[2].image = sources[10].image;
        pairs[2].clut = sources[10].clut;
        pairs[3].image = sources[11].image;
        pairs[3].clut = sources[11].clut;
        pairs[4].image = sources[12].image;
        pairs[4].clut = sources[12].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, firstBlock, scale, 5, slotIndex, 1.0f);
    } else if (kind >= 17 && kind <= 19) {
        pairs[0].image = sources[0].image;
        pairs[0].clut = sources[0].clut;
        pairs[1].image = sources[0].image;
        pairs[1].clut = sources[0].clut;
        pairs[2].image = sources[13].image;
        pairs[2].clut = sources[13].clut;
        pairs[3].image = sources[14].image;
        pairs[3].clut = sources[14].clut;
        pairs[4].image = sources[15].image;
        pairs[4].clut = sources[15].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, firstBlock, scale, 5, slotIndex, 0.0f);
    } else if (kind == 20) {
        pairs[0].image = sources[18].image;
        pairs[0].clut = sources[18].clut;
        pairs[1].image = sources[17].image;
        pairs[1].clut = sources[17].clut;
        pairs[2].image = sources[19].image;
        pairs[2].clut = sources[19].clut;
        return sefCreateResourceSlot(effectId, pairs, 20, position, firstBlock, scale, 3, slotIndex, 0.0f);
    } else if (kind == 21) {
        pairs[0].image = sources[20].image;
        pairs[0].clut = sources[20].clut;
        pairs[1].image = sources[21].image;
        pairs[1].clut = sources[21].clut;
        return sefCreateResourceSlot(effectId, pairs, 21, position, firstBlock, scale, 2, slotIndex, 0.0f);
    } else {
        return 0;
    }
}

f32 sefRandf(void);
void sefGetCirclePos(void *vector, f32 scale);

void sefGetCylinderPos(Vector4 *vector, f32 scale, f32 yScale) {
    sefGetCirclePos(vector, sefRandf() * scale);
    vector->y = sefRandf() * yScale;
}

void sefGetUnitDirection(AlignedVector4 *vector);

/* Writes a random unit direction (sefGetUnitDirection) into vector and scales it: by -scale when scale is negative,
 * else by scale times sefRandf(). */
void sefGetRandomScaledDirection(AlignedVector4 *vector, f32 scale) {
    f32 factor;

    sefGetUnitDirection(vector);
    if (scale < 0.0f) {
        factor = -scale;
    } else {
        factor = sefRandf() * scale;
    }
    Vu0SefScaleVectorXYZNI(vector, vector, factor);
}

/* Writes a rotation about Z by angle into the first three rows of rows: (c, s, 0, 0), (-s, c, 0, 0), (0, 0, 1, 0).
 * VU0 macro mode: the cosine (0xE8) and sine (0x20) micro subroutines take the angle in vf4 and leave the result
 * in the x lane of vf1. Each result lands in the x member of a stack vector, as in the original; the two asm blocks
 * have no "memory" clobber, so the matrix stores reuse the FPR copies. */
void sefRotationZRows(SefRotationRow *rows, f32 angle) {
    Vector4 cosine;
    Vector4 sine;

    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0xE8\n\t"
        "qmfc2.i $8, vf1\n\t"
        "mtc1 $8, %0"
        : "=f"(cosine.x) : "f"(angle) : "$8");
    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0x20\n\t"
        "qmfc2.i $8, vf1\n\t"
        "mtc1 $8, %0"
        : "=f"(sine.x) : "f"(angle) : "$8");
    rows[0].element[0] = cosine.x;
    rows[0].element[1] = sine.x;
    rows[0].halves[1] = 0;
    rows[1].element[0] = -sine.x;
    rows[1].element[1] = cosine.x;
    rows[1].halves[1] = 0;
    rows[2].quad = 0;
    rows[2].element[2] = 1.0f;
}

f32 sefRandAngle(void);

/* Writes a random rotation into the first three rows of rows: angle x 0, y and z random, composed by
 * MMathRotateMatrixYXZ. */
void sefRandomRotateMatrix(SefRotationRow *rows)
{
    Vector4 angles;
    /* The original copies the three rows out as six doublewords (ld/sd), so the matrix is also seen as u64 words:
     * the plain copy rows[row].element[column] = matrix[row][column] compiles to a lwc1/swc1 loop (39 insns for 29). */
    union {
        Matrix4 rows;
        u64 words[8];
    } matrix;

    angles.x = 0.0f;
    angles.y = sefRandAngle();
    angles.z = sefRandAngle();
    MMathRotateMatrixYXZ(&matrix.rows, 0, &angles);
    rows[0].halves[0] = matrix.words[0];
    rows[0].halves[1] = matrix.words[1];
    rows[1].halves[0] = matrix.words[2];
    rows[1].halves[1] = matrix.words[3];
    rows[2].halves[0] = matrix.words[4];
    rows[2].halves[1] = matrix.words[5];
}

/* Fires burst emitter index of slot once per frame: spawns up to count particles that fly from D_005E6EE0
 * towards a random point around D_005E6EF0. */
INCLUDE_ASM("asm/main/nonmatchings/tu185", func_0026CF90);

INCLUDE_ASM("asm/main/nonmatchings/tu185", func_0026D2B8);

/* What one emitter of an effect kind spawns (0x20 bytes). */
typedef struct SefEmitterParams {
    int interval;           /* +0x00: frames between spawns; 0 spawns every frame */
    int count;              /* +0x04: particles per spawn */
    f32 life;               /* +0x08 */
    f32 spread;             /* +0x0C: scale handed to sefGetRandomScaledDirection for the start position */
    f32 size;               /* +0x10 */
    f32 scaleW;             /* +0x14: stored in the particle's scale.w */
    short shade;            /* +0x18: red, green and blue */
    short alpha;            /* +0x1A */
    short lastFrame;        /* +0x1C: no spawning after this frame */
    unsigned char unmodeled_1e[0x20 - 0x1E];
} SefEmitterParams;

/* The parameter rows of kinds 5 to 7 (sefEmitterParamsKinds5To7, 0x5E6F80), addressed from the row of kind 0: the
   original
 * indexes [kind][line] from 0x5E6D00, five rows (0x80 bytes each) before the table, which lies inside srsRecordTable
 * (+0x630). The same access as (sefEmitterParamsKinds5To7 - 5)[kind] or [kind - 5] costs two instructions (fd
   137/135). */
extern SefEmitterParams D_005E6D00[][4];
f32 sefNoiseNext(void);
void *MMathNormalizeVector2(void *destination, const Vector4 *source);

/* Emitters of effect kinds 0 to 7 (parameter rows of four per kind): once per frame, spawns up to count particles into
   free
 * entries of the ring. */
void sefSpawnKinds5To7(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefEmitterParams *params;
    SefFadeParticle *particle;
    int frame;
    int fire;
    int mask;
    int cursor;
    int last;
    int spawned;
    int id;
    int lastFrame;
    f32 growth;

    emitter = &slot->body.emitters[index];
    fire = 0;
    frame = emitter->elapsed;
    params = &D_005E6D00[slot->body.kind][index];
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        fire = 1;
    }
    if (emitter->frame != frame && frame <= lastFrame) {
        emitter->frame = frame;
        if (fire) {
            mask = slot->body.particleCount - 1;
            last = (emitter->cursor - 1) & mask;
            cursor = emitter->cursor;
            spawned = 0;
            while (cursor != last) {
                cursor &= mask;
                if (emitter->particles[cursor] == 0) {
                    id = sefPoolAlloc();
                    if (id == 0) {
                        break;
                    }
                    particle = &sefEntryPool.entries[id].fade;
                    emitter->particles[cursor] = id;
                    spawned++;
                    sefGetRandomScaledDirection(&particle->position, params->spread);
                    sefRotationZRows(particle->rotation, sefRandAngle());
                    particle->scale.x = particle->scale.y = particle->scale.z = params->size;
                    particle->scale.w = params->scaleW;
                    growth = sefNoiseNext() * 0.5f + 1.0f;
                    particle->growth.x = particle->growth.y = particle->growth.z = growth;
                    particle->growth.w = 1.0f;
                    particle->red = particle->green = particle->blue = params->shade;
                    particle->alpha = params->alpha;
                    particle->position.y += 0.25f;
                    particle->life = params->life;
                    particle->position.w = 1.0f;
                    particle->age = 0;
                    particle->verticalSpeed = 0;
                    particle->axisOffset = 0;
                    if (spawned >= params->count) {
                        break;
                    }
                }
                cursor++;
            }
        }
    }
}

/* velocity = target - position (xyz). The "m" operand is an input the instructions never read (they go through
   the register operands); it is there only so that ee-gcc gives the position pointer the registers the original
   uses ($17/$18; without it the parameter row takes them). The three spawners that use it keep the position in a
   local; the plain Vu0SefSubVectorXYZ forms, a const or reordered local, the local also used for the position
   stores, and several other lifetimes compiled to the swapped allocation in sefSpawnKinds8To10, 22Up and
   14To16. */
#define SefVelocityTowards(velocity, target, position) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%1)\n" \
        "lqc2 vf2, 0(%2)\n" \
        "vsub.xyz vf1, vf1, vf2\n" \
        "sqc2 vf1, 0(%0)\n" \
        : \
        : "r" (velocity), "r" (target), "r" (position), "m" (*(position)) \
        : "memory")

extern SefEmitterParams sefEmitterParamsKinds8To10[];

/* Emitters of effect kinds 8 to 10 (parameter rows of four per kind): 0 and 3 spin in place, 1 and 2 fly towards a
   random point
   above the origin; emitter 1 shrinks its particles after the ninth. */
void sefSpawnKinds8To10(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 *position;
    SefEmitterParams *params;
    AlignedVector4 direction;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int row;
    int lastFrame;

    due = 0;
    emitter = &slot->body.emitters[index];
    frame = emitter->elapsed;
    row = slot->body.kind - 8;
    params = &sefEmitterParamsKinds8To10[index + row * 4];
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        position = &entry->drift.position;
        emitter->particles[cursor] = id;
        sefGetRandomScaledDirection(position, params->spread);
        entry->drift.scale.w = params->scaleW;
        entry->drift.growth.w = 1.0f;
        if (index == 0 || index == 3) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z =
                sefNoiseNext() * 0.5f + 1.0f;
        } else {
            sefRandomRotateMatrix(entry->drift.rotation);
            if (index == 1 && spawned >= 9) {
                Vu0SetOrigin(&entry->drift.scale);
                entry->drift.scale.x = 1.0f;
                entry->drift.scale.y = 1.0f;
                entry->drift.scale.z = 1.0f;
                entry->drift.scale.w = 0.032f;
            } else {
                entry->drift.scale.x = 0.75f;
                entry->drift.scale.y = 3.0f;
                entry->drift.scale.z = 0.75f;
            }
            entry->drift.growth.x = sefNoiseNext() * params->size + 1.0f;
            entry->drift.growth.y = sefNoiseNext() * params->size + 1.0f;
            entry->drift.growth.z = sefNoiseNext() * params->size + 1.0f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            direction.y += 1.5f;
            SefVelocityTowards(&entry->drift.velocity, &direction, position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = sefNoiseNext() * 0.025f + 0.15f;
            entry->drift.landed = 0;
        }
        entry->drift.position.y += 0.5f;
        entry->drift.position.w = 1.0f;
        entry->drift.red = entry->drift.green = entry->drift.blue = params->shade;
        entry->drift.alpha = params->alpha;
        entry->drift.life = params->life;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.axisOffset = 0;
        spawned++;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern SefEmitterParams sefEmitterParamsKinds11To13[];

/* Emitters of effect kinds 11 to 13 (parameter rows of five per kind; sefCreateKindResourceSlot creates three
   emitters):
 * each spawn emits up to count particles while free entries last, until lastFrame. */
void sefSpawnKinds11To13(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    SefEmitterParams *params;
    AlignedVector4 direction;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int row;
    int lastFrame;

    due = 0;
    emitter = &slot->body.emitters[index];
    row = slot->body.kind - 11;
    params = &sefEmitterParamsKinds11To13[index + row * 5];
    frame = emitter->elapsed;
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        emitter->particles[cursor] = id;
        sefGetRandomScaledDirection(&entry->drift.position, params->spread);
        entry->drift.growth.w = 1.0f;
        entry->drift.scale.w = params->scaleW;
        if (index != 2) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z =
                sefNoiseNext() * 0.5f + 1.0f;
        } else {
            sefRandomRotateMatrix(entry->drift.rotation);
            entry->drift.scale.x = 2.0f;
            entry->drift.scale.y = 5.0f;
            entry->drift.scale.z = 2.0f;
            entry->drift.growth.x = sefNoiseNext() * 1.5f + 1.0f;
            entry->drift.growth.y = sefNoiseNext() * 1.5f + 1.0f;
            entry->drift.growth.z = sefNoiseNext() * 1.5f + 1.0f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            direction.y += 1.5f;
            Vu0SefSubVectorXYZ(&entry->drift.velocity, &direction, &entry->drift.position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = sefNoiseNext() * 0.05f + 0.2f;
            entry->drift.landed = 0;
        }
        entry->drift.position.y += 0.25f;
        entry->drift.position.w = 1.0f;
        entry->drift.red = entry->drift.green = entry->drift.blue = params->shade;
        entry->drift.alpha = params->alpha;
        entry->drift.life = params->life;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.axisOffset = 0;
        spawned++;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern SefEmitterParams sefEmitterParamsKinds22Up[];

/* Emitters 1 and 2 of these kinds add xglSRand() & SEF_SHADE_TINT_MASK, 0 to 31, to the shade. */
#define SEF_SHADE_TINT_MASK 0x1F

/* Emitters of effect kinds 22 and up (parameter rows of four per kind): like sefSpawnKinds8To10 with its emitters 1
   and 2 facing a random
   angle, adding a random tint to the shade and emitter 2 shrinking its particles. */
void sefSpawnKinds22Up(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 *position;
    SefEmitterParams *params;
    AlignedVector4 direction;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int row;
    int lastFrame;
    int tint;

    due = 0;
    emitter = &slot->body.emitters[index];
    row = slot->body.kind - 22;
    params = &sefEmitterParamsKinds22Up[index + row * 4];
    frame = emitter->elapsed;
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    tint = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        position = &entry->drift.position;
        emitter->particles[cursor] = id;
        sefGetRandomScaledDirection(position, params->spread);
        entry->drift.scale.w = params->scaleW;
        entry->drift.growth.w = 1.0f;
        if (index == 0 || index == 3) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z =
                sefNoiseNext() * 0.5f + 1.0f;
        } else {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            tint = xglSRand() & SEF_SHADE_TINT_MASK;
            if (index == 2) {
                Vu0SetOrigin(&entry->drift.scale);
                entry->drift.scale.x = 1.0f;
                entry->drift.scale.y = 1.0f;
                entry->drift.scale.z = 1.0f;
                entry->drift.scale.w = 0.025f;
            } else {
                entry->drift.scale.x = 1.5f;
                entry->drift.scale.y = 1.5f;
                entry->drift.scale.z = 1.5f;
            }
            entry->drift.growth.x = sefNoiseNext() * params->size + 1.0f;
            entry->drift.growth.y = sefNoiseNext() * params->size + 1.0f;
            entry->drift.growth.z = sefNoiseNext() * params->size + 1.0f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            direction.y += 1.5f;
            SefVelocityTowards(&entry->drift.velocity, &direction, position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = sefNoiseNext() * 0.05f + 0.21f;
            entry->drift.landed = 0;
        }
        entry->drift.position.y += 0.5f;
        entry->drift.position.w = 1.0f;
        entry->drift.red = entry->drift.green = entry->drift.blue = params->shade + tint;
        entry->drift.alpha = params->alpha;
        entry->drift.life = params->life;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.axisOffset = 0;
        spawned++;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern SefEmitterParams sefEmitterParamsKinds14To16[];

/* Emitters of effect kinds 14 to 16 (parameter rows of five per kind): 0 to 2 fly outwards along their start position,
   3 and 4 fly
   towards a random point. */
void sefSpawnKinds14To16(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 *position;
    SefEmitterParams *params;
    AlignedVector4 direction;
    float spread;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int row;
    int lastFrame;

    due = 0;
    emitter = &slot->body.emitters[index];
    row = slot->body.kind - 14;
    params = &sefEmitterParamsKinds14To16[index + row * 5];
    frame = emitter->elapsed;
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        position = &entry->drift.position;
        emitter->particles[cursor] = id;
        sefGetRandomScaledDirection(position, params->spread);
        entry->drift.scale.w = params->scaleW;
        entry->drift.growth.w = 1.0f;
        entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
        if (index < 3) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z =
                sefNoiseNext() * 0.5f + 1.0f;
            entry->drift.red = 0x50;
            entry->drift.green = 0x50;
            entry->drift.blue = 0x5A;
            if (index != 0) {
                entry->drift.speed = sefNoiseNext() * 0.005f + 0.005f;
            } else {
                entry->drift.speed = 0.001f;
            }
            MMathNormalizeVector2(&entry->drift.velocity, position);
            Vu0SefScaleVectorXYZNI(&entry->drift.velocity, &entry->drift.velocity, entry->drift.speed);
        } else {
            if (index == 3) {
                spread = 1.5f;
            } else {
                spread = 1.0f;
            }
            sefRandomRotateMatrix(entry->drift.rotation);
            entry->drift.growth.x = sefNoiseNext() * spread + 1.0f;
            entry->drift.growth.y = sefNoiseNext() * spread + 1.0f;
            entry->drift.growth.z = sefNoiseNext() * spread + 1.0f;
            entry->drift.red = entry->drift.green = entry->drift.blue = params->shade;
            entry->drift.landed = 0;
            entry->drift.speed = sefNoiseNext() * 0.01f + 0.01f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            SefVelocityTowards(&entry->drift.velocity, &direction, position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
        }
        entry->drift.position.y += 0.25f;
        entry->drift.alpha = params->alpha;
        entry->drift.axisOffset = 0;
        entry->drift.position.w = 1.0f;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.life = params->life;
        spawned++;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern SefEmitterParams sefEmitterParamsKinds17To19[];

/* Emitters of effect kinds 17 to 19 (parameter rows of five per kind, sefEmitterParamsKinds17To19 indexed from kind
   0): 0 and 1 start on a cylinder,
   2 at a random offset, 3 and 4 on a cylinder and fly towards a random point. */
void sefSpawnKinds17To19(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    SefEmitterParams *params;
    AlignedVector4 direction;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int lastFrame;

    due = 0;
    emitter = &slot->body.emitters[index];
    params = &sefEmitterParamsKinds17To19[index + (slot->body.kind - 17) * 5];
    frame = emitter->elapsed;
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        emitter->particles[cursor] = id;
        entry->drift.scale.w = params->scaleW;
        entry->drift.growth.w = 1.0f;
        if (index == 2) {
            sefGetRandomScaledDirection(&entry->drift.position, params->spread);
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z = sefNoiseNext() + 1.0f;
            entry->drift.position.y = 0.2f;
        } else if (index == 1) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            sefGetCylinderPos(&entry->drift.position, 0.2f, 0.0f);
            entry->drift.position.y += 0.5f;
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z = sefNoiseNext() + 1.0f;
            entry->drift.velocity.x = sefNoiseNext() * 4.0f;
            entry->drift.velocity.y = sefRandf() * 2.0f;
            entry->drift.velocity.z = sefRandf() * 4.0f;
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = 0.06f;
        } else if (index == 0) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            sefGetCylinderPos(&entry->drift.position, 0.2f, 0.0f);
            entry->drift.position.y += 0.5f;
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z = sefNoiseNext() + 1.0f;
        } else {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            if (index == 3) {
                entry->drift.growth.x = sefNoiseNext() + 1.0f;
                entry->drift.growth.y = sefNoiseNext() + 1.0f;
                entry->drift.growth.z = sefNoiseNext() + 1.0f;
                entry->drift.red = entry->drift.green = entry->drift.blue = 0x80;
            } else {
                entry->drift.growth.y = entry->drift.growth.z = entry->drift.growth.x = sefNoiseNext() + 1.0f;
                entry->drift.red = entry->drift.green = entry->drift.blue = 0x28;
            }
            sefGetCylinderPos(&entry->drift.position, 0.2f, 0.0f);
            entry->drift.position.y = 0.1f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            direction.y += 1.0f;
            direction.z += 2.0f;
            Vu0SefSubVectorXYZ(&entry->drift.velocity, &direction, &entry->drift.position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = sefRandf() * 0.05f + 0.2f;
            entry->drift.landed = 0;
        }
        entry->drift.position.w = 1.0f;
        if (index < 3) {
            entry->drift.red = entry->drift.green = entry->drift.blue = params->shade;
        }
        entry->drift.alpha = params->alpha;
        if (slot->body.kind == 19) {
            entry->drift.position.z += emitter->elapsed * 0.2f;
        }
        spawned++;
        entry->drift.life = params->life;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.axisOffset = 0;
        if (spawned >= params->count) {
            break;
        }
    }
}

/* Per emitter of effect kind 20: the spawn interval in frames (also the particle life) and the particle size. */
extern short sefKind20Intervals[];
extern float sefKind20Sizes[];

/* Emitters of effect kind 20: one particle every interval frames; emitter 1 starts it orange-red and transparent. */
void sefSpawnKind20(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    int frame;
    int mask;
    int cursor;
    int last;
    int id;
    int due;

    emitter = &slot->body.emitters[index];
    frame = emitter->elapsed;
    due = frame % sefKind20Intervals[index] == 0;
    if (emitter->frame != frame) {
        emitter->frame = frame;
        if (!due) {
            return;
        }
        mask = slot->body.particleCount - 1;
        last = (emitter->cursor - 1) & mask;
        for (cursor = emitter->cursor; cursor != last; cursor++) {
            cursor &= mask;
            if (emitter->particles[cursor] == 0) {
                id = sefPoolAlloc();
                if (id == 0) {
                    break;
                }
                entry = &sefEntryPool.entries[id];
                emitter->particles[cursor] = id;
                sefSetOnes(&entry->spin.scale);
                sefSetOnes(&entry->spin.growth);
                entry->spin.scale.w = sefKind20Sizes[index];
                Vu0SetOrigin(&entry->spin.position);
                sefRotationZRows(entry->spin.rotation, 0.0f);
                if (index == 1) {
                    entry->spin.red = 200;
                    entry->spin.green = 60;
                    entry->spin.blue = 40;
                    entry->spin.alpha = 0;
                } else {
                    entry->spin.alpha = entry->spin.red = entry->spin.green = entry->spin.blue = 0x80;
                }
                entry->spin.life = sefKind20Intervals[index];
                entry->spin.angle = 0.0f;
                entry->spin.age = 0.0f;
                entry->spin.verticalSpeed = 0.0f;
                entry->spin.axisOffset = 0;
                break;
            }
        }
    }
}

/* Emitters of effect kind 21: tries to spawn one particle on the first whole frame handled. The one-shot latch is set
 * before the search, so a failed search or allocation is not retried. */
void sefSpawnKind21(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    int frame;
    int mask;
    int cursor;
    int last;
    int id;

    emitter = &slot->body.emitters[index];
    frame = emitter->elapsed;
    if (emitter->frame == frame) {
        return;
    }
    emitter->frame = frame;
    if (emitter->spawned != 0) {
        return;
    }
    emitter->spawned = 1;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] == 0) {
            id = sefPoolAlloc();
            if (id == 0) {
                break;
            }
            entry = &sefEntryPool.entries[id];
            emitter->particles[cursor] = id;
            sefSetOnes(&entry->spin.scale);
            sefSetOnes(&entry->spin.growth);
            if (index != 0) {
                entry->spin.scale.w = 0.32f;
            } else {
                entry->spin.scale.w = 0.64f;
            }
            Vu0SetOrigin(&entry->spin.position);
            sefRotationZRows(entry->spin.rotation, 0.0f);
            entry->spin.red = 200;
            entry->spin.life = 10.0f;
            entry->spin.axisOffset = 0;
            entry->spin.blue = 0x80;
            entry->spin.green = 0x80;
            entry->spin.angle = 0.0f;
            entry->spin.age = 0.0f;
            entry->spin.verticalSpeed = 0.0f;
            entry->spin.alpha = 0x80;
            break;
        }
    }
}

/* destination.xyz = from + (to - from) * t through vf1..vf3 (the w of to is replaced by 1 first). */
#define SefLerpVectorXYZ(destination, from, to, t) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%1)\n" \
        "lqc2 vf2, 0(%2)\n" \
        "mfc1 $8, %3\n" \
        "vmove.w vf2, vf0\n" \
        "qmtc2.ni $8, vf3\n" \
        "vsub.xyz vf2, vf2, vf1\n" \
        "vmulx.xyz vf2, vf2, vf3x\n" \
        "vadd.xyz vf2, vf2, vf1\n" \
        "sqc2 vf2, 0(%0)\n" \
        : \
        : "r" (destination), "r" (from), "r" (to), "f" (t) \
        : "$8", "memory")

/* Group update of effect kinds 0 to 2 (sefUpdateHandlers): ages the live particles and moves each by a step that turns
 * from velocity towards target after frame 20 and lengthens with age; the particles grow (fastest up to frame 10),
 * fade in until frame 10 and out from frame 60. */
void sefUpdateBurstParticles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 step;
    unsigned short id;
    f32 age;
    int i;

    emitter = &slot->body.emitters[index];
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->fade.age < entry->fade.life) {
                entry->fade.age = entry->fade.age + 1.0f;
                emitter->liveCount++;
                if (entry->fade.age > 20.0f) {
                    SefLerpVectorXYZ(&step, &entry->fade.velocity, &entry->fade.target,
                                     (entry->fade.age - 20.0f) * 0.01f);
                } else {
                    QuadwordCopyT0NoClobber(&step, &entry->fade.velocity);
                }
                Vu0SefScaleVectorXYZNI(&step, &step, entry->fade.age * 0.0008f + 0.025f);
                Vu0SefAddVectorXYZ(&entry->fade.position, &entry->fade.position, &step);
                age = entry->fade.age;
                if (age <= 10.0f) {
                    entry->fade.scale.x += 0.05f;
                } else if (age < 60.0f) {
                    entry->fade.scale.x += 0.03f;
                } else {
                    entry->fade.scale.x += 0.017f;
                }
                entry->fade.scale.z = entry->fade.scale.y = entry->fade.scale.x;
                if (age < 10.0f) {
                    entry->fade.alpha += 25;
                } else if (!(age < 60.0f)) {
                    entry->fade.alpha -= 4;
                    if (entry->fade.alpha < 0) {
                        entry->fade.alpha = 0;
                    }
                }
            } else {
                entry->fade.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

INCLUDE_ASM("asm/main/nonmatchings/tu185", func_0026F010);

/* Group update of effect kinds 5-7 (sefUpdateHandlers): ages the live particles,
 * grows them and fades them out; odd emitters also brighten. */
void sefUpdateFadeOutParticles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    unsigned short id;
    int i;

    emitter = &slot->body.emitters[index];
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->fade.age < entry->fade.life) {
                entry->fade.age = entry->fade.age + 1.0f;
                emitter->liveCount++;
                if (index & 1) {
                    if (slot->body.kind == 5) {
                        if (entry->fade.age <= 10.0f) {
                            entry->fade.scale.x += 0.025f;
                        } else {
                            entry->fade.scale.x += 0.02f;
                        }
                    } else if (slot->body.kind == 6) {
                        if (entry->fade.age <= 10.0f) {
                            entry->fade.scale.x += 0.03f;
                        } else {
                            entry->fade.scale.x += 0.02f;
                        }
                    } else if (entry->fade.age <= 10.0f) {
                        entry->fade.scale.x += 0.07f;
                    } else {
                        entry->fade.scale.x += 0.033f;
                    }
                    entry->fade.scale.z = entry->fade.scale.y = entry->fade.scale.x;
                    entry->fade.green = entry->fade.blue = entry->fade.red = entry->fade.red + 1;
                    if (entry->fade.age <= 10.0f) {
                        entry->fade.alpha += 25;
                    } else if (entry->fade.age > 20.0f) {
                        entry->fade.alpha -= 12;
                    }
                    if (entry->fade.alpha < 0) {
                        entry->fade.alpha = 0;
                    }
                } else {
                    if (entry->fade.age <= 5.0f) {
                        entry->fade.scale.x += 0.16f;
                    } else {
                        entry->fade.scale.x += 0.06f;
                    }
                    entry->fade.scale.z = entry->fade.scale.y = entry->fade.scale.x;
                    if (entry->fade.age >= 10.0f) {
                        entry->fade.green -= 2;
                        entry->fade.blue -= 4;
                        entry->fade.alpha -= 6;
                        if (entry->fade.alpha < 0) {
                            entry->fade.alpha = 0;
                        }
                    }
                }
            } else {
                entry->fade.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

INCLUDE_ASM("asm/main/nonmatchings/tu185", func_0026F4C8);

INCLUDE_ASM("asm/main/nonmatchings/tu185", func_0026F7D8);

INCLUDE_ASM("asm/main/nonmatchings/tu185", func_0026FB88);

INCLUDE_ASM("asm/main/nonmatchings/tu185", func_0026FFE0);

/* Group update of effect kind 20 (sefUpdateHandlers): emitter 1 shifts the colour and
 * grows at ages 0 to 30 while alive, emitter 2 turns 6 degrees a frame and pulses its size. */
void sefUpdateKind20Particles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    unsigned short id;
    int i;

    emitter = &slot->body.emitters[index];
    emitter->liveCount = 1;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->spin.age < entry->spin.life) {
                if (index == 1) {
                    if (entry->spin.age <= 30.0f) {
                        entry->spin.green = 60 - (int) (entry->spin.age + entry->spin.age);
                        entry->spin.red = (int) (entry->spin.age * 1.667f) + 200;
                        entry->spin.blue = (int) (entry->spin.age * 1.333f) + 40;
                    }
                    if (entry->spin.age <= 16.0f) {
                        entry->spin.alpha += 8;
                    } else if (entry->spin.age <= 31.0f) {
                        entry->spin.alpha -= 8;
                    }
                    if (entry->spin.age <= 30.0f) {
                        entry->spin.scale.y = entry->spin.scale.z = entry->spin.scale.x =
                            entry->spin.scale.x + 0.0333f;
                    }
                }
                if (index == 2) {
                    entry->spin.angle -= 6.0f;
                    sefRotationZRows(entry->spin.rotation, entry->spin.angle * 0.017453292f);
                    if (entry->spin.age < 18.0f) {
                        entry->spin.scale.x += 0.0222f;
                    } else if (entry->spin.age < 22.0f) {
                        entry->spin.scale.x += 0.025f;
                    } else if (entry->spin.age < 25.0f) {
                        entry->spin.scale.x -= 0.2f;
                    } else if (entry->spin.age < 30.0f) {
                        entry->spin.scale.x += 0.02f;
                    } else if (entry->spin.age < 48.0f) {
                        entry->spin.scale.x += 0.0222f;
                    } else if (entry->spin.age < 52.0f) {
                        entry->spin.scale.x += 0.025f;
                    } else if (entry->spin.age < 55.0f) {
                        entry->spin.scale.x -= 0.2f;
                    } else if (entry->spin.age < 60.0f) {
                        entry->spin.scale.x += 0.02f;
                    }
                    entry->spin.scale.y = entry->spin.scale.x;
                }
                entry->spin.age = entry->spin.age + 1.0f;
            } else {
                entry->spin.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
    if (emitter->elapsed > 59.9f) {
        emitter->elapsed = 0.0f;
    }
}

/* Counts the live particles of the emitter; except for emitter 0 each one also turns 6 degrees (back to 0 once
 * at or past -359) and gets its matrix rebuilt. Then the emitter ages by one frame. */
void sefUpdateSpinParticles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefSpinParticle *particle;
    int i;
    unsigned short id;

    emitter = &slot->body.emitters[index];
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            particle = &sefEntryPool.entries[id].spin;
            if (index != 0) {
                particle->angle -= 6.0f;
                if (particle->angle <= -359.0f) {
                    particle->angle = 0.0f;
                }
                sefRotationZRows(particle->rotation, particle->angle * 0.017453292f);
            }
            emitter->liveCount++;
        }
    }
    emitter->elapsed += 1.0f;
}

/* A per-kind emitter handler, called with the slot and the emitter index. */
typedef void (*SefEmitterFunc)(ResourceSlot *slot, int index);

/* The owner record of sef_164.c; a resource slot is one (its key word at +0x74). */
typedef struct SefOwner SefOwner;

void uwamonoReleaseOwnedRecords(SefOwner *owner);
extern s16 sefClipTestDisabled;
extern SefEmitterFunc sefSpawnHandlers[];
extern SefEmitterFunc sefUpdateHandlers[];

/* Runs the emitters of every active slot (or only of target when it is not null) and recounts sefActiveCount:
 * an emitter waits out its delay first, then runs its kind's spawn and group-update handlers. A slot left
 * without live particles is released unless it is paused or persistent. */
void sefRunEmitters(ResourceSlot *target) {
    ResourceSlot *slot;
    SefEmitter *emitter;
    int index;
    int emitterIndex;

    sefActiveCount = 0;
    sefPoolFull = 0;
    if (sefClipTestDisabled != 0) {
        return;
    }
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        slot = &sefResourceTable[index];
        if (slot->body.activePairs == 0) {
            continue;
        }
        if (target != 0 && target != slot) {
            continue;
        }
        if (slot->body.flags & RESOURCE_FLAG_SUSPENDED) {
            continue;
        }
        sefFloorHeight = (0.0f < slot->position.y) ? -slot->position.y : 0.0f;
        slot->body.liveCount = 0;
        for (emitterIndex = 0; emitterIndex < slot->body.emitterCount; emitterIndex++) {
            emitter = &slot->body.emitters[emitterIndex];
            if (0.0f < emitter->delay) {
                emitter->delay -= 1.0f;
            } else {
                if (!(slot->body.flags & RESOURCE_FLAG_PAUSED)) {
                    sefSpawnHandlers[slot->body.kind](slot, emitterIndex);
                }
                sefUpdateHandlers[slot->body.kind](slot, emitterIndex);
                slot->body.liveCount += emitter->liveCount;
            }
        }
        sefActiveCount += slot->body.liveCount;
        if (slot->body.liveCount == 0
            && !(slot->body.flags & (RESOURCE_FLAG_PERSISTENT | RESOURCE_FLAG_PAUSED))) {
            if (((slot->body.key >> 28) & 3) == 1) {
                uwamonoReleaseOwnedRecords((SefOwner *)slot);
            }
            sefReleaseCellSlot(slot);
        }
    }
}

Matrix4 *MMathScaleMatrix(Matrix4 *out, Matrix4 *matrix, const Vector4 *scale);
/* Nonzero when x, y or z has any bit set: the original tests the raw words (x and y as one doubleword, z as a
 * word), so -0.0f counts as set. */
static inline short sefVectorXYZSet(const Vector4 *vector) {
    return (*(const int *)&vector->z | *(const u64 *)&vector->x) != 0;
}

/* Sets the VU packet up for one emitter of slot: with the slot matrix, or, when the slot has a position, rotation
 * or scale of its own, with that transform applied to it first (and the camera matrix scaled too); then TEST_1,
 * whose depth test is off (ZTST ALWAYS) under RESOURCE_FLAG_NO_DEPTH_TEST or for kinds 20 and 21, else GEQUAL.
 * tex0 and kind, the caller's copies of emitter->tex0 and slot->body.kind, are not read. The (const Matrix4 *)
 * casts only silence gcc's warning for a Matrix4 * passed as const Matrix4 *. */
void sefSetupEmitterPacket(u64 tex0, ResourceSlot *slot, short kind, int emitterIndex) {
    Matrix4 matrix;
    SefEmitter *emitter;
    short hasPosition;
    short hasRotation;
    short hasScale;

    emitter = &slot->body.emitters[emitterIndex];
    hasPosition = sefVectorXYZSet(&slot->position);
    hasRotation = sefVectorXYZSet(&slot->rotation);
    hasScale = sefVectorXYZSet(&slot->scale);
    if (hasPosition || hasRotation || hasScale) {
        sefLoadUnitMatrix(&matrix);
        if (hasPosition) {
            QuadwordCopyT0(matrix[3], &slot->position);
        }
        if (hasRotation) {
            MMathRotateMatrixXYZ(&matrix, &matrix, &slot->rotation);
        }
        if (hasScale) {
            MMathScaleMatrix(&matrix, &matrix, &slot->scale);
        }
        MMathMulMatrix(&matrix, (const Matrix4 *) &matrix, (const Matrix4 *) &slot->body.matrix);
        sefBuildModelPacket(emitter->tex0, (const Matrix4 *) &matrix, slot->body.kind, emitterIndex);
        MMathScaleMatrix(&sefVuPacket.cameraMatrix, &sefVuPacket.cameraMatrix, &slot->scale);
    } else {
        sefBuildModelPacket(emitter->tex0, (const Matrix4 *) &slot->body.matrix, slot->body.kind, emitterIndex);
    }
    if (slot->body.flags & RESOURCE_FLAG_NO_DEPTH_TEST) {
        sefVuPacket.test = GS_SET_TEST(1, 6, 0, 0, 0, 0, 0, 1);
    } else if (slot->body.kind == 20 || slot->body.kind == 21) {
        sefVuPacket.test = GS_SET_TEST(1, 6, 0, 0, 0, 0, 0, 1);
    } else {
        sefVuPacket.test = GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 2);
    }
}

u8 *xglPacketReserve(int count);

/* Added to a reserved packet address to write it through the uncached segment. */
#define SEF_UNCACHED_SEGMENT 0x20000000

/* sefDrawEmitter builds each particle in vf1..vf5: matrix rows 0 to 3, then the colour. */

/* vf15 = the slot's tint, kept for SefTintColour. */
#define SefLoadTint(tint) \
    __asm__ __volatile__("lqc2 vf15, 0(%0)\n" : : "r" (tint) : "memory")

/* vf1..vf4 = the four rows of matrix. */
#define SefLoadMatrixRows(matrix) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%0)\n" \
        "lqc2 vf2, 16(%0)\n" \
        "lqc2 vf3, 32(%0)\n" \
        "lqc2 vf4, 48(%0)\n" \
        : \
        : "r" (matrix) \
        : "memory")

/* vf5 = the particle's colour words; rows 0 to 2 scaled by scale.xyz * scale.w * growth.xyz (vf7). */
#define SefScaleParticleRows(particle) \
    __asm__ __volatile__( \
        "lqc2 vf6, 80(%0)\n" \
        "lqc2 vf5, 64(%0)\n" \
        "lqc2 vf8, 144(%0)\n" \
        "vmulw.xyz vf7, vf6, vf6w\n" \
        "vmul.xyz vf7, vf7, vf8\n" \
        "vmulx.xyzw vf1, vf1, vf7x\n" \
        "vmuly.xyzw vf2, vf2, vf7y\n" \
        "vmulz.xyzw vf3, vf3, vf7z\n" \
        : \
        : "r" (particle) \
        : "memory")

/* vf5 = vf5 * vf15 (the integer colour through float and back). */
#define SefTintColour() \
    __asm__ __volatile__( \
        "vitof0.xyzw vf5, vf5\n" \
        "vmul.xyzw vf5, vf5, vf15\n" \
        "vftoi0.xyzw vf5, vf5\n" \
        : : : "memory")

/* Row 3 (the position) += row 0 * offset. */
#define SefOffsetAlongRow0(offset) \
    __asm__ __volatile__( \
        "mfc1 $8, %0\n" \
        "qmtc2.ni $8, vf8\n" \
        "vmulax.xyzw ACC, vf1, vf8x\n" \
        "vmadday.xyzw ACC, vf2, vf0y\n" \
        "vmaddaz.xyzw ACC, vf3, vf0z\n" \
        "vmaddw.xyzw vf4, vf4, vf0w\n" \
        : \
        : "f" (offset) \
        : "$8", "memory")

/* Stores vf1..vf5 to the five quadwords at destination. */
#define SefStoreParticle(destination) \
    __asm__ __volatile__( \
        "sqc2 vf1, 0(%0)\n" \
        "sqc2 vf2, 16(%0)\n" \
        "sqc2 vf3, 32(%0)\n" \
        "sqc2 vf4, 48(%0)\n" \
        "sqc2 vf5, 64(%0)\n" \
        : \
        : "r" (destination) \
        : "memory")

/* Draws the live particles of emitter emitterIndex of slot: per particle five quadwords (its matrix with rows 0 to 2
 * scaled by its size, then its colour, tinted by slot->tint), 24 particles per full block (two header quadwords
 * plus 120 particle quadwords, 0x7A in all). The spinning emitters turn the matrix by the particle age first;
 * axisOffset moves it along row 0. */
void sefDrawEmitter(ResourceSlot *slot, int emitterIndex) {
    Matrix4 matrix;
    Vector4 angles;
    int drawn;
    SefEmitter *emitter;
    SefPoolEntry *entry;
    u8 *reserved;
    Quadword *packet;
    int qwc;
    int batch;
    int i;
    int alternate;
    int spinning;
    int tinted;
    int overhead;
    unsigned short id;

    qwc = 2;
    batch = 0;
    emitter = &slot->body.emitters[emitterIndex];
    drawn = 0;
    alternate = 0;
    if (slot->body.kind >= 8 && slot->body.kind <= 10) {
        alternate = emitterIndex == 1 || emitterIndex == 2;
    }
    spinning = alternate;
    if (slot->body.kind >= 14 && slot->body.kind <= 16 && emitterIndex >= 3) {
        alternate = 1;
        spinning = 1;
    } else if (slot->body.kind >= 11 && slot->body.kind <= 13 && emitterIndex == 2) {
        alternate = 1;
    }
    if (slot->body.visible == 0) {
        return;
    }
    if (emitter->liveCount <= 0) {
        if (emitterIndex == 0) {
            sefSetupEmitterPacket(emitter->tex0, slot, slot->body.kind, 0);
            sefBuildVuPacket(1, alternate);
        }
        return;
    }
    sefSetupEmitterPacket(emitter->tex0, slot, slot->body.kind, emitterIndex);
    if (emitterIndex != 0) {
        sefBuildVuPacket(2, alternate);
    } else {
        sefBuildVuPacket(1, alternate);
    }
    /* Two header quadwords per block of 24 particles, plus 8. */
    overhead = emitter->liveCount / 24 * 2 + 8;
    reserved = xglPacketReserve(emitter->liveCount * 5 + overhead);
    packet = (Quadword *) (reserved + SEF_UNCACHED_SEGMENT);
    if (reserved == 0) {
        return;
    }
    sefUnpackBlockInit((SefUnpackBlock *) packet);
    if (slot->tint.x == 1.0f && slot->tint.y == 1.0f && slot->tint.z == 1.0f) {
        tinted = 0;
    } else {
        tinted = 1;
        SefLoadTint(&slot->tint);
    }
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id == 0) {
            continue;
        }
        entry = &sefEntryPool.entries[id];
        if (!(0.0f < entry->drift.life)) {
            continue;
        }
        if (spinning) {
            angles.x = angles.z = (entry->drift.age + entry->drift.age) * 0.017453292f;
            angles.y = entry->drift.age * 1.5f * 0.017453292f;
            /* Also leaves the turned rows in vf1..vf4 (its last sqc2 store them, m_math_193.c). */
            MMathRotateMatrixYXZ(&matrix, &entry->matrix, &angles);
        } else {
            SefLoadMatrixRows(&entry->matrix);
        }
        SefScaleParticleRows(entry);
        if (tinted) {
            SefTintColour();
        }
        if (entry->drift.axisOffset != 0.0f) {
            SefOffsetAlongRow0(entry->drift.axisOffset);
        }
        SefStoreParticle(&packet[qwc]);
        qwc += 5;
        batch++;
        drawn++;
        if ((unsigned int) qwc >= 0x79) {
            sefPacketUnpackRef((SefUnpackBlock *) packet, 0x28, qwc, batch);
            batch = 0;
            packet += qwc;
            qwc = 2;
            sefUnpackBlockInit((SefUnpackBlock *) packet);
        }
    }
    if (qwc >= 3) {
        sefPacketUnpackRef((SefUnpackBlock *) packet, 0x28, qwc, batch);
    }
    if (drawn != 0) {
        sefPacketMscnt();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/tu185", func_00271078);

INCLUDE_ASM("asm/main/nonmatchings/tu185", func_00271150);

ResourceSlot *sefGetResourceSlot(int index) {
    return &sefResourceTable[index];
}

void svLoadDrawViewMatrix(void);
void svBuildDrawStatePacket(void);

/* Rebuilds the draw-state and VU packets, then draws every emitter of slot
 * slotIndex (one call per emitter, up to emitterCount); nothing when no particle
 * is active or slotIndex is outside the table. The packet is flushed when the
 * slot had an emitter. */
void sefDrawResources(int slotIndex) {
    ResourceSlot *slot;
    int emitterIndex;

    if (sefActiveCount <= 0) {
        return;
    }
    if ((unsigned int)slotIndex >= SEF_RESOURCE_SLOT_COUNT) {
        return;
    }
    sefLastDrawnKind = -1;
    svLoadDrawViewMatrix();
    svBuildDrawStatePacket();
    sefBuildCameraPacket();
    sefBuildVuPacket(0, 0);

    slot = &sefResourceTable[slotIndex];
    emitterIndex = 0;
    if (slot->body.emitterCount > 0) {
        do {
            slot->position.w = 1.0f;
            sefDrawEmitter(slot, emitterIndex);
            emitterIndex += 1;
            sefLastDrawnKind = slot->body.kind;
        } while (emitterIndex < slot->body.emitterCount);
    }
    if (sefLastDrawnKind >= 0) {
        sefPacketFlush();
    }
}

/* The original body is a bare return: no code between entry and return. */
void sefNoOp(void) {
}

/* One entry of sefEffectRecordList (tu176.c, 64 entries): a persistent slot saved by sefCollectPersistentSlots and
 * respawned by sefRestoreEffectRecords, which passes position, rotation, scale, key and slotIndex to
 * sefSpawnEffectRecord and writes owner, savedWord, flags and visible back into the new record. */
typedef struct SefSavedEffect {
    Vector4 position;  /* +0x00 */
    Vector4 rotation;  /* +0x10 */
    Vector4 scale;     /* +0x20 */
    void *owner;       /* +0x30: the slot's owner */
    void *savedWord;   /* +0x34: the slot's savedWord */
    int key;           /* +0x38 */
    int flags;         /* +0x3C */
    s16 visible;       /* +0x40 */
    s16 resetFlagLow;  /* +0x42: not read back */
    s16 slotIndex;     /* +0x44: index in sefResourceTable */
    u8 unmodeled_46[0x50 - 0x46];
} SefSavedEffect;

/* Appends every active persistent slot to the saved list from entry count on, up to 64 entries (sefCollectEffectRecords
 * tail-calls it after collecting the effect records). */
void sefCollectPersistentSlots(SefSavedEffect *list, int count) {
    ResourceSlot *slot;
    int index;

    slot = sefResourceTable;
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++, slot++) {
        if (slot->body.activePairs != 0 && count < 64 && (slot->body.flags & RESOURCE_FLAG_PERSISTENT)) {
            QuadwordCopyT0(&list[count].position, &slot->position);
            QuadwordCopyT0(&list[count].rotation, &slot->rotation);
            QuadwordCopyT0(&list[count].scale, &slot->scale);
            list[count].flags = slot->body.flags;
            list[count].owner = slot->body.owner;
            list[count].key = slot->body.key;
            list[count].savedWord = slot->body.savedWord;
            list[count].visible = slot->body.visible;
            list[count].resetFlagLow = slot->body.resetFlag;
            list[count].slotIndex = index;
            count++;
        }
    }
}

/* GS ALPHA register values of the effect blend modes: (Cs - Cd) * As + Cd, (Cs - 0) * As + Cd, the fixed-alpha
 * (Cs - Cd) * 0x80 + Cd, and (0 - Cs) * As + Cd. */
const u64 sefAlphaBlendValues[4] = { 0x44ULL, 0x48ULL, 0x0000008000000042ULL, 0x6AULL };

const SefQuadCorners sefQuadCorners = {
    { { -1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, 0.0f, 1.0f } }
};

const double D_0068C650[2] = { 0.00125, 0.0 };

/* The scaffold assembly functions the handler tables name. */
void func_0026CF90(ResourceSlot *slot, int index);
void func_0026D2B8(ResourceSlot *slot, int index);
void func_0026F010(ResourceSlot *slot, int index);
void func_0026F4C8(ResourceSlot *slot, int index);
void func_0026F7D8(ResourceSlot *slot, int index);
void func_0026FB88(ResourceSlot *slot, int index);
void func_0026FFE0(ResourceSlot *slot, int index);

/* Particle ring size (entries per emitter) of each effect kind. */
short sefRingSizes[28] = {
    32, 32, 32, 128, 128, 32, 32, 64,
    32, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 128, 16, 16, 64, 64,
    64, 0, 0, 0
};

int D_005E6EA8[14] = { 4, 1, 0, 4, 1, 0, 4, 1, 0, 4, 1, 0, 0, 0 };
Vector4 D_005E6EE0 = { 0.0f, 0.5f, 0.0f, 1.0f };
Vector4 D_005E6EF0 = { 0.0f, 10.0f, 0.0f, 1.0f };
Matrix4 D_005E6F00 = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
int D_005E6F40[4] = { 80, 50, 25, 25 };
Vector4 D_005E6F50 = { 1.0f, 1.0f, 1.0f, 1.0f };
Vector4 D_005E6F60 = { 0.01f, 0.01f, 0.005f, 0.005f };
Vector4 D_005E6F70 = { 3.0f, 3.0f, 3.0f, 2.0f };

SefEmitterParams sefEmitterParamsKinds5To7[12] = {
    { 2, 2, 30.0f, -0.3f, 0.3f, 0.22399999f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.4f, 0.75f, 0.512f, 25, 0, 10 },
    { 2, 2, 30.0f, -0.3f, 0.3f, 0.22399999f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.4f, 0.75f, 0.512f, 25, 0, 10 },
    { 2, 2, 30.0f, -0.3f, 0.3f, 0.24f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.4f, 0.85f, 0.544f, 25, 0, 10 },
    { 2, 2, 30.0f, -0.3f, 0.3f, 0.24f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.4f, 0.85f, 0.544f, 25, 0, 10 },
    { 2, 2, 30.0f, -0.4f, 0.3f, 0.24f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.7f, 0.4f, 0.41599998f, 25, 255, 10 },
    { 2, 2, 30.0f, -0.4f, 0.3f, 0.24f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.7f, 0.4f, 0.41599998f, 25, 255, 16 }
};

SefEmitterParams sefEmitterParamsKinds8To10[12] = {
    { 2, 5, 30.0f, 1.2f, 0.5f, 0.768f, 64, 150, 2 },
    { 100, 8, 30.0f, 0.6f, 1.5f, 0.128f, 64, 150, 2 },
    { 100, 5, 30.0f, 0.6f, 1.0f, 0.128f, 64, 150, 2 },
    { 2, 4, 30.0f, 1.2f, 0.5f, 0.64f, 64, 150, 2 },
    { 2, 5, 30.0f, 1.4f, 0.5f, 0.89599997f, 64, 150, 4 },
    { 2, 10, 30.0f, 0.8f, 1.5f, 0.128f, 64, 150, 2 },
    { 2, 5, 30.0f, 0.8f, 1.0f, 0.128f, 64, 150, 4 },
    { 2, 4, 30.0f, 1.4f, 0.5f, 0.768f, 64, 150, 4 },
    { 2, 5, 30.0f, 1.6f, 0.5f, 0.96f, 64, 150, 4 },
    { 2, 10, 30.0f, 0.8f, 1.5f, 0.128f, 64, 150, 2 },
    { 2, 5, 30.0f, 0.8f, 1.0f, 0.128f, 64, 150, 4 },
    { 2, 4, 30.0f, 1.6f, 0.5f, 0.89599997f, 64, 150, 4 }
};

SefEmitterParams sefEmitterParamsKinds11To13[15] = {
    { 2, 3, 10.0f, 0.6f, 0.5f, 0.22399999f, 128, 192, 2 },
    { 2, 2, 30.0f, 0.8f, 0.6f, 0.576f, 25, 255, 2 },
    { 2, 5, 30.0f, 0.2f, 1.0f, 0.0448f, 40, 128, 2 },
    { 2, 3, 10.0f, 0.6f, 0.5f, 0.22399999f, 128, 192, 2 },
    { 2, 2, 30.0f, 0.8f, 0.6f, 0.576f, 25, 255, 2 },
    { 2, 4, 10.0f, 0.7f, 0.5f, 0.272f, 128, 192, 2 },
    { 2, 2, 30.0f, 1.0f, 0.6f, 0.64f, 25, 255, 2 },
    { 2, 5, 30.0f, 0.3f, 1.0f, 0.048f, 40, 128, 2 },
    { 2, 3, 10.0f, 0.7f, 0.5f, 0.272f, 128, 192, 2 },
    { 2, 3, 30.0f, 1.0f, 0.6f, 0.64f, 25, 255, 2 },
    { 2, 4, 10.0f, 0.85f, 0.5f, 0.32f, 128, 192, 2 },
    { 2, 3, 30.0f, 1.1f, 0.6f, 0.768f, 25, 255, 2 },
    { 2, 6, 30.0f, 0.4f, 1.0f, 0.0512f, 40, 128, 2 },
    { 2, 3, 10.0f, 0.85f, 0.5f, 0.32f, 128, 192, 2 },
    { 2, 3, 30.0f, 1.1f, 0.6f, 0.768f, 25, 255, 2 }
};

SefEmitterParams sefEmitterParamsKinds22Up[12] = {
    { 2, 5, 30.0f, 1.2f, 0.5f, 0.768f, 64, 150, 2 },
    { 2, 5, 30.0f, 0.2f, 1.0f, 0.0448f, 110, 128, 2 },
    { 2, 7, 30.0f, 0.9f, 1.0f, 0.016f, 120, 255, 3 },
    { 2, 4, 30.0f, 1.2f, 0.5f, 0.64f, 64, 150, 2 },
    { 2, 5, 30.0f, 1.4f, 0.5f, 0.89599997f, 64, 150, 4 },
    { 2, 5, 30.0f, 0.3f, 1.0f, 0.048f, 110, 128, 2 },
    { 2, 6, 30.0f, 0.8f, 1.0f, 0.0139999995f, 120, 255, 3 },
    { 2, 4, 30.0f, 1.4f, 0.5f, 0.768f, 64, 150, 4 },
    { 2, 5, 30.0f, 1.6f, 0.5f, 0.96f, 64, 150, 4 },
    { 2, 6, 30.0f, 0.4f, 1.0f, 0.0512f, 110, 128, 2 },
    { 2, 4, 30.0f, 0.6f, 1.0f, 0.012f, 120, 255, 3 },
    { 2, 4, 30.0f, 1.6f, 0.5f, 0.89599997f, 64, 150, 4 }
};

SefEmitterParams sefEmitterParamsKinds14To16[15] = {
    { 2, 4, 30.0f, 0.7f, 0.7f, 0.32f, 80, 128, 4 },
    { 2, 4, 30.0f, 0.6f, 1.0f, 0.012f, 90, 255, 3 },
    { 2, 8, 30.0f, 0.8f, 0.7f, 0.64f, 80, 128, 2 },
    { 2, 4, 30.0f, 0.3f, 2.4f, 0.048f, 128, 200, 2 },
    { 2, 6, 30.0f, 0.3f, 1.4f, 0.032f, 128, 200, 2 },
    { 2, 4, 30.0f, 0.8f, 0.7f, 0.352f, 80, 128, 4 },
    { 2, 6, 30.0f, 0.8f, 1.0f, 0.0139999995f, 90, 255, 3 },
    { 2, 10, 30.0f, 0.9f, 0.7f, 0.768f, 80, 128, 2 },
    { 2, 4, 30.0f, 0.7f, 2.4f, 0.055999998f, 128, 200, 2 },
    { 2, 5, 30.0f, 0.7f, 1.4f, 0.04f, 128, 200, 2 },
    { 2, 4, 30.0f, 0.9f, 0.7f, 0.41599998f, 80, 128, 4 },
    { 2, 7, 30.0f, 0.9f, 1.0f, 0.016f, 90, 255, 3 },
    { 2, 10, 30.0f, 1.0f, 0.7f, 0.89599997f, 80, 128, 2 },
    { 2, 4, 30.0f, 0.9f, 2.4f, 0.064f, 128, 200, 2 },
    { 2, 5, 30.0f, 0.9f, 1.4f, 0.048f, 128, 200, 2 }
};

SefEmitterParams sefEmitterParamsKinds17To19[15] = {
    { 100, 1, 40.0f, 0.2f, 0.5f, 0.32f, 25, 0, 2 },
    { 100, 4, 40.0f, 0.2f, 0.5f, 0.384f, 25, 255, 2 },
    { 100, 1, 2.0f, 0.0f, 1.0f, 0.384f, 128, 128, 2 },
    { 2, 1, 30.0f, 0.2f, 1.0f, 0.064f, 128, 128, 4 },
    { 2, 1, 30.0f, 0.2f, 1.0f, 0.096f, 128, 128, 4 },
    { 100, 1, 40.0f, 0.3f, 0.5f, 0.384f, 25, 0, 2 },
    { 2, 4, 40.0f, 0.2f, 0.5f, 0.384f, 25, 255, 4 },
    { 2, 1, 2.0f, 0.0f, 1.0f, 0.384f, 128, 128, 2 },
    { 2, 1, 30.0f, 0.2f, 1.0f, 0.064f, 128, 128, 4 },
    { 2, 1, 30.0f, 0.2f, 1.0f, 0.096f, 128, 128, 4 },
    { 100, 1, 40.0f, 0.5f, 0.8f, 0.41599998f, 25, 0, 2 },
    { 2, 4, 40.0f, 0.5f, 0.8f, 0.41599998f, 25, 255, 8 },
    { 2, 1, 2.0f, 0.0f, 1.0f, 0.48f, 128, 128, 8 },
    { 2, 2, 30.0f, 0.2f, 1.0f, 0.064f, 128, 128, 8 },
    { 2, 2, 30.0f, 0.2f, 1.0f, 0.096f, 128, 128, 8 }
};

short sefKind20Intervals[] = { 60, 30, 60, 0 };
float sefKind20Sizes[4] = { 0.64f, 0.288f, 0.32f, 0.0f };
float D_005E79B8[4] = { 0.002f, 0.0032f, 0.0005f, 0.0008f };

SefEmitterFunc sefSpawnHandlers[26] = {
    func_0026CF90, func_0026CF90, func_0026CF90, func_0026D2B8, func_0026D2B8,
    sefSpawnKinds5To7, sefSpawnKinds5To7, sefSpawnKinds5To7,
    sefSpawnKinds8To10, sefSpawnKinds8To10, sefSpawnKinds8To10,
    sefSpawnKinds11To13, sefSpawnKinds11To13, sefSpawnKinds11To13,
    sefSpawnKinds14To16, sefSpawnKinds14To16, sefSpawnKinds14To16,
    sefSpawnKinds17To19, sefSpawnKinds17To19, sefSpawnKinds17To19,
    sefSpawnKind20, sefSpawnKind21,
    sefSpawnKinds22Up, sefSpawnKinds22Up, sefSpawnKinds22Up,
    0
};

SefEmitterFunc sefUpdateHandlers[26] = {
    sefUpdateBurstParticles, sefUpdateBurstParticles, sefUpdateBurstParticles, func_0026F010, func_0026F010,
    sefUpdateFadeOutParticles, sefUpdateFadeOutParticles, sefUpdateFadeOutParticles,
    func_0026F4C8, func_0026F4C8, func_0026F4C8,
    func_0026F7D8, func_0026F7D8, func_0026F7D8,
    func_0026FB88, func_0026FB88, func_0026FB88,
    func_0026FFE0, func_0026FFE0, func_0026FFE0,
    sefUpdateKind20Particles, sefUpdateSpinParticles,
    func_0026F4C8, func_0026F4C8, func_0026F4C8,
    0
};

/* Last look-at and position keys applied by sdv_181.c (its SdvCameraKeyLog, 0x28 bytes). */
short sdvCameraKeyLog[20] = { 0 };

/* The small-cell heap handed to smInitilizeSmall (0x20000 bytes), and the resource slot table behind it. */
unsigned char sefSmallHeap[0x20000] = { 0 };
ResourceSlot sefResourceTable[SEF_RESOURCE_SLOT_COUNT] = { 0 };
