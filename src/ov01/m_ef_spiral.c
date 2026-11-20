#include "common.h"

#include "shared.h"
#include "ov01/m_ef.h"
#include "ov01/m_ef_missile_trail.h"
#include "main/m_math.h"

#include "quadword.h"

#include "ov01/m_gs.h"

#include "vu0_macro.h"

#include "main/m_se_obj.h"
#include "main/sm_initilize.h"

/* Counter effect constants. The GS register values and names (MEF_GS_*) are in ov01/m_gs.h. */
#define MEF_COUNTER_KEYS 25                  /* keys of the path, one flag word each */
#define MEF_COUNTER_SEGMENT_KEYS 8           /* keys per Hermite segment */
#define MEF_COUNTER_ACTIVE_STEPS 10          /* steps 0..9 of the window hold a live key */
#define MEF_COUNTER_LIFETIME 0x23            /* step at which the object is freed */
#define MEF_COUNTER_STRIPS 8                 /* triangle strips per draw */
#define MEF_COUNTER_STRIP_ANGLE 0.7853981f   /* pi / 4 between strips */
#define MEF_COUNTER_KEY_ANGLE 0.5235988f     /* pi / 6 between keys of a strip */
#define MEF_COUNTER_STEP_SPIN -0.052359879f  /* pi / 60 per step */
#define MEF_COUNTER_WIDTH 0.13f              /* half width of a strip at a live key */
#define MEF_COUNTER_KEY_ACTIVE 0x01U         /* key drawn (set for steps 0..9) */
#define MEF_COUNTER_KEY_HEAD 0x10U           /* key of step 0: its strip has no width */
#define MEF_COUNTER_KEY_TAIL 0x20            /* key of step 9: its strip has no width */



/* The block at +0x20 of the object these functions update. Only the fields
 * they access are modelled. key_flags[0] holds the flags of the newest
 * key (MEF_COUNTER_KEY_*); the functions shift the window by one entry per call. */
typedef struct MEfCounterBlock {
    unsigned char unmodeled_00[0x0C];
    struct XenoActor *startActor;                          /* +0x0C: actor and coordinate of coord_a and matrix */
    u32 startCoordinate;                     /* +0x10 */
    unsigned char unmodeled_14[0x30 - 0x14];
    struct XenoActor *endActor;                            /* +0x30: actor and coordinate of coord_b */
    u32 endCoordinate;                       /* +0x34 */
    unsigned char unmodeled_38[0x40 - 0x38];
    MGsPacket gs;
    unsigned short step;
    unsigned char unmodeled_56[0x0A];
    Vector4 coord_a;
    Vector4 coord_b;
    Matrix4 matrix;
    short key_flags[MEF_COUNTER_KEYS];
    unsigned char unmodeled_f2[0x0E];
    Vector4 keys[MEF_COUNTER_KEYS];
    Vector4 angles[MEF_COUNTER_KEYS];
} MEfCounterBlock;

/* The object of MEfCreateSpiral: its three callbacks at +0x08 (step), +0x10 (draw) and +0x14 (advance). */
typedef struct MEfCounterObject {
    unsigned char unmodeled_00[0x08];
    void (*stepFunc)(struct MEfCounterObject *);
    unsigned char unmodeled_0C[0x04];
    void (*drawFunc)(struct MEfCounterObject *);
    void (*advanceFunc)(struct MEfCounterObject *);
    unsigned char unmodeled_18[0x08];
    MEfCounterBlock block;
} MEfCounterObject;

double fptodp(float value);
int dpcmp(double first, double second);
void *memset(void *dst, int value, unsigned int size);
void MEfBuildSpiralCurve(MEfCounterBlock *block);
void MEfFillSpiralKeys(MEfCounterBlock *block);
void MEfSetSpiralPhaseFlags(MEfCounterObject *object);
void MEfDrawSpiralStrips(MEfCounterObject *object);
void MEfAdvanceSpiral(MEfCounterObject *object);

/* Create callback: reads the actor placements, clears the key tables, builds the path and keys, and installs the three callbacks. */
int MEfCreateSpiral(MEfCounterObject *object)
{
    MEfCounterBlock *block;

    block = &object->block;
    block->step = 0;
    MEfGetActorCoord(&block->coord_a, block->startActor, block->startCoordinate);
    MEfGetActorCoord(&block->coord_b, block->endActor, block->endCoordinate);
    MSeGetActorMatrix(&block->matrix, block->startActor, block->startCoordinate);
    memset(block->key_flags, 0, sizeof(block->key_flags));
    memset(block->keys, 0, sizeof(block->keys));
    memset(block->angles, 0, sizeof(block->angles));
    MEfBuildSpiralCurve(block);
    MEfFillSpiralKeys(block);
    MGsGPInit(&block->gs, 0, 0);
    object->stepFunc = MEfSetSpiralPhaseFlags;
    object->drawFunc = MEfDrawSpiralStrips;
    object->advanceFunc = MEfAdvanceSpiral;
    return 1;
}

/* Builds the path curve from the block's two endpoints and the matrix. */
void MEfBuildSpiralCurve(MEfCounterBlock *block)
{
    MEfPathCurve *curve;
    AlignedVector4 handle;
    AlignedVector4 angleA;
    AlignedVector4 angleB;
    Matrix4 rotation;

    curve = &mefPathCurve.curve;
    MEfHermitePathInit(curve, 4);
    __asm__ __volatile__(
        "lq $8, 0(%2)\n"
        "sq $8, 0(%1)"
        : "=m"(curve->points[0])
        : "r"(&curve->points[0]), "r"(&block->coord_a)
        : "$8");
    __asm__ __volatile__(
        "lq $8, 0(%3)\n"
        "sq $8, 0(%2)"
        : "=m"(curve->points[3]), "=m"(curve->points[3].w)
        : "r"(&curve->points[3]), "r"(&block->coord_b)
        : "$8");
    Vu0SetOrigin(&handle);
    handle.z = MMathCalcDist(&block->coord_a, &block->coord_b) * 0.333333f;
    MMathApplyMatrix(&curve->points[1], &block->matrix, &handle);
    MMathCalcAngle(&angleA, &curve->points[0], &curve->points[1]);
    MMathCalcAngle(&angleB, &curve->points[1], &curve->points[3]);
    MMathVectorInterpolation(&angleA, &angleA, &angleB, 0.5f);
    MMathRotateMatrixYX(&rotation, 0, &angleA);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        : "=m"(rotation[3])
        : "r"(&rotation), "r"(&curve->points[1]), "m"(curve->points[1])
        : "memory");
    Vu0SetOrigin(&handle);
    handle.z = MMathCalcDist(&curve->points[1], &curve->points[3]) * 0.5f;
    MMathApplyMatrix(&curve->points[2], &rotation, &handle);
}

/* Samples the path curve into the 25 keys, then the 24 angles between neighbouring keys (the last angle repeats the one before). */
void MEfFillSpiralKeys(MEfCounterBlock *block)
{
    MEfPathCurve *curve;
    Matrix4 matrix;
    AlignedVector4 sample;
    Vector4 *keySlot;
    int keyIndex;
    int segment;
    int fraction;

    curve = &mefPathCurve.curve;
    QuadwordCopyMatrixT0(matrix, block->matrix);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        :
        : "r"(&MMathUnitVector), "r"(matrix)
        : "memory");
    for (keyIndex = 0; keyIndex < MEF_COUNTER_KEYS; keyIndex++) {
        segment = keyIndex / MEF_COUNTER_SEGMENT_KEYS;
        fraction = keyIndex % MEF_COUNTER_SEGMENT_KEYS;
        if (segment == 3) {
            segment = 2;
            fraction = 8;
        }
        MMathCalcHermiteKeyPrm(curve, segment);
        MMathCalcHermiteKey(&sample, curve, segment, (float) fraction * 0.125f);
        keySlot = block->keys + keyIndex;
        __asm__ __volatile__(
            "lq $8, 0(%5)\n"
            "sq $8, 0(%4)\n"
            : "=m"(block->keys[keyIndex].w), "=m"(block->keys[keyIndex].w),
              "=m"(block->keys[keyIndex].w), "=m"(block->keys[keyIndex].w)
            : "r"(keySlot), "r"(&sample)
            : "$8");
    }
    for (keyIndex = 0; keyIndex < MEF_COUNTER_KEYS - 1; keyIndex++) {
        MMathCalcAngle(&block->angles[keyIndex], &block->keys[keyIndex], &block->keys[keyIndex + 1]);
    }
    QuadwordCopyT0(&block->angles[MEF_COUNTER_KEYS - 1], &block->angles[MEF_COUNTER_KEYS - 2]);
}

/* Step callback: sets the flags of the newest key from the step (HEAD at step 0, TAIL at the last active step). */
void MEfSetSpiralPhaseFlags(MEfCounterObject *object)
{
    MEfCounterBlock *block;
    short step;

    block = &object->block;
    if (ov01EffectPauseGet() == 0) {
        step = (short) block->step;
        block->key_flags[0] = 0U;
        if (step < MEF_COUNTER_ACTIVE_STEPS) {
            block->key_flags[0] = MEF_COUNTER_KEY_ACTIVE;
            if (step == 0) {
                block->key_flags[0] = (MEF_COUNTER_KEY_ACTIVE | MEF_COUNTER_KEY_HEAD);
            }
            if (step == MEF_COUNTER_ACTIVE_STEPS - 1) {
                block->key_flags[0] |= MEF_COUNTER_KEY_TAIL;
            }
        }
    }
}

/* Copies a point into a matrix row and sets its w to 1 (lqc2, vmove, sqc2; as m_ef_sway.c). */
#define Vu0MMathCopyPointDst(destination, source) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%1)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, %0\n" \
        : "=m" (*(Vector4 *) (destination)) \
        : "r" (source) \
        : "memory")

/* Writes a GIF tag for regCount registers and returns its first data qword. */
static inline MEfGsQword *OpenTag(MGsPacket *packet, int pre, int prim, int regCount, u64 registers)
{
    MEfGsQword *qword = packet->current;

    qword->gif.tag = MEF_GIF_TAG_EOP | ((u64) pre << 46) | ((u64) prim << 47) | ((u64) regCount << 60);
    qword->gif.registers = registers;
    packet->regCount = regCount;
    return qword + 1;
}

/* Writes one A+D register setting. */
static inline void SetRegister(MEfGsQword *qword, unsigned char address, u64 data)
{
    qword->ad.data = data;
    qword->ad.address = address;
}

/* Stores NLOOP into the open GIF tag and moves past its data. */
static inline void CloseTag(MGsPacket *packet, int nloop)
{
    int size;

    if (nloop > 0) {
        size = packet->regCount * nloop + 1;
        packet->current->nloop |= nloop;
        packet->current += size;
        packet->count += size;
    }
}

/*
 * Draw callback: writes the GS state, then eight triangle strips around the
 * path. Each strip takes the 25 keys whose key flags are set and gives every
 * key two vertices, pushed apart along the rotated z axis.
 */
void MEfDrawSpiralStrips(MEfCounterObject *object)
{
    MEfCounterBlock *block;
    MGsPacket *packet;
    Matrix4 matrix;
    Vector4 point;
    MEfGsVertex upper;
    MEfGsVertex lower;
    AlignedVector4 offset;
    Vector4 *rotation;
    MEfGsQword *header;
    MEfGsQword *strip;
    struct XglCamera *camera;
    Matrix4 *view;
    int keyCount;
    int clipRemaining;
    int keyIndex;
    int stripIndex;
    float angle;
    float width;
    /* RGBAQ of the outer and inner edge of each strip. */
    static const Quadword outerColor = 0x000000C0000000800000004000000000;
    static const Quadword innerColor = 0x00000030000000800000000000000000;
    Quadword outer;
    Quadword inner;

    block = &object->block;
    packet = &object->block.gs;
    camera = MEfCamParams.camera;
    view = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);

    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    SetRegister(&header[1], MEF_GS_TEST_1, MEF_GS_TEST_1_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);

    __asm__ __volatile__("sqc2 $vf0, 0(%2)\n" : "=m"(offset), "=m"(offset.w) : "r"(&offset), "r"(&offset));
    offset.x = 0.4f;

    for (stripIndex = 0; stripIndex < MEF_COUNTER_STRIPS; stripIndex++) {
        strip = OpenTag(&block->gs, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
        outer = outerColor;
        inner = innerColor;
        angle = (float) stripIndex * MEF_COUNTER_STRIP_ANGLE + (float) (short) block->step * MEF_COUNTER_STEP_SPIN;
        keyCount = 0;
        clipRemaining = 0;
        for (keyIndex = 0; keyIndex < MEF_COUNTER_KEYS; keyIndex++, angle += MEF_COUNTER_KEY_ANGLE) {
            if (block->key_flags[keyIndex] == 0) {
                continue;
            }
            rotation = &block->angles[keyIndex];
            rotation->z = angle;
            MMathRotateMatrixYXZ(&matrix, 0, rotation);
            Vu0MMathCopyPointDst(matrix[3], &block->keys[keyIndex]);
            if (((unsigned short) block->key_flags[keyIndex] & (MEF_COUNTER_KEY_HEAD | MEF_COUNTER_KEY_TAIL)) != 0 || keyIndex == 0 || keyIndex == MEF_COUNTER_KEYS - 1) {
                width = 0.0f;
            } else {
                width = MEF_COUNTER_WIDTH;
            }
            offset.z = width;
            MMathApplyMatrix(&point, &matrix, &offset);
            if (MMathRotTransPersClip(&upper.vector, camera, view, &point) != 0) {
                clipRemaining = 2;
            } else if (clipRemaining != 0) {
                clipRemaining--;
                upper.words[3] = MEF_GS_XYZ_ADC;
            }
            offset.z = -width;
            MMathApplyMatrix(&point, &matrix, &offset);
            if (MMathRotTransPersClip(&lower.vector, camera, view, &point) != 0) {
                clipRemaining = 2;
            } else if (clipRemaining != 0) {
                clipRemaining--;
                lower.words[3] = MEF_GS_XYZ_ADC;
            }
            strip[0].whole = outer;
            keyCount++;
            strip[1].whole = upper.whole;
            strip[2].whole = inner;
            strip[3].whole = lower.whole;
            strip += 4;
        }
        CloseTag(&block->gs, keyCount);
    }
    MGsGPTerm(&block->gs);
}

/* Advance callback: shifts the key flags by one entry, steps the counter and frees the object at MEF_COUNTER_LIFETIME. */
void MEfAdvanceSpiral(MEfCounterObject *object)
{
    MEfCounterBlock *block;
    int keyIndex;
    int nextStep;

    block = &object->block;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    for (keyIndex = MEF_COUNTER_KEYS - 1; keyIndex > 0; keyIndex--) {
        block->key_flags[keyIndex] = block->key_flags[keyIndex - 1];
    }
    nextStep = block->step + 1;
    block->step = nextStep;
    if ((short) nextStep >= MEF_COUNTER_LIFETIME) {
        MSeObjRequestFree(object);
    }
}
