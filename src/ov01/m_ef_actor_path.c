#include "common.h"

#include "shared.h"
#include "main/m_math.h"
#include "ov01/m_ef.h"
#include "quadword.h"
#include "ov01/m_gs.h"
#include "vu0_macro.h"
#include "main/m_se_obj.h"

/* Trail matrices are built while the step is below this count (the trail grows over 13 steps). */
#define MEF_PATH_GROW_STEPS 13
/* MEfAdvancePathStep ends the effect once the step count reaches this value. */
#define MEF_PATH_LIFETIME_STEPS 0x11
/* Trail matrices of the path: MEF_PATH_KEY_COUNT (m_ef.h) sampled keys, MEF_PATH_TRAIL_COUNT
 * matrices, and MEF_PATH_HEAD_SLOT is the first matrix of the head (the four slots from it). */
#define MEF_PATH_TRAIL_COUNT 17
#define MEF_PATH_HEAD_SLOT 13
/* Trail draw: strips of the growing trail, its cross-section, the head strips and slots. */
#define MEF_TRAIL_GROW_STRIPS 20
#define MEF_TRAIL_SECTIONS 4
#define MEF_TRAIL_HEAD_STRIPS 4
#define MEF_TRAIL_HEAD_SLOTS 4
#define MEF_TRAIL_SWEEP_ANGLE 0.5235988f
#define MEF_TRAIL_GROW_ANGLE_STEP 0.31415924f
#define MEF_TRAIL_HEAD_ANGLE_STEP 1.5707963f
#define MEF_TRAIL_GROW_FADE_PERCENT 85
#define MEF_TRAIL_HEAD_FADE_PERCENT 70

/* A colour or edge slot of the trail: the header's MEfGsQword with unsigned words, because the
 * fade multiplies words[3] (alpha) by a percent and divides it unsigned. */
typedef union TrailQword {
    Quadword whole;
    unsigned short nloop; /* low halfword of a GIF tag: NLOOP */
    struct {
        u64 tag;
        u64 registers;
    } gif;
    struct {
        u64 data;
        unsigned char address;
    } ad;
    u32 words[4];
} TrailQword;

/*
 * Partial view of the pooled effect object that the path callbacks of this TU
 * receive. The work record starts at +0x20; its unsigned step count at +0x54 of
 * the work selects one of the 64-byte trail matrices that start at +0xC0 (the
 * draw reads that halfword as a signed trail index).
 */
typedef struct MEfPathWork MEfPathWork;

struct MEfPathWork {
    unsigned char unmodeled_00[0x0C];
    struct XenoActor *actor; /* +0x0C of the work */
    u32 coordinate; /* +0x10 of the work */
    unsigned char unmodeled_14[0x20 - 0x14];
    Vector4 translation; /* +0x20 of the work */
    struct XenoActor *endActor; /* +0x30 of the work */
    u32 endCoordinate; /* +0x34 of the work */
    unsigned char unmodeled_38[0x40 - 0x38];
    MGsPacket gsPacket; /* +0x40 of the work: GS packet cleared by MGsGPInit */
    unsigned short stepCount; /* +0x54 of the work: stepped by MEfAdvancePathStep */
    unsigned char unmodeled_56[0x60 - 0x56];
    Matrix4 actorMatrix; /* +0x60 of the work */
    AlignedVector4 actorTranslation; /* +0xA0 of the work */
    AlignedVector4 actorCoord; /* +0xB0 of the work */
    Matrix4 trail[MEF_PATH_TRAIL_COUNT]; /* +0xC0 of the work */
};

typedef struct MEfActorPathObject MEfActorPathObject;

struct MEfActorPathObject {
    unsigned char unmodeled_00[0x08];
    void (*updateTrail)(MEfActorPathObject *object); /* +0x08 */
    unsigned char unmodeled_0C[0x10 - 0x0C];
    void (*drawTrail)(MEfActorPathObject *object); /* +0x10 */
    void (*advanceStep)(MEfActorPathObject *object); /* +0x14 */
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfPathWork work; /* +0x20 */
};

void MEfBuildPathCurve(MEfPathWork *work);
void MEfBuildPathKeyframes(MEfPathWork *work);
void MEfUpdatePathTrail(MEfActorPathObject *object);
void MEfAdvancePathStep(MEfActorPathObject *object);
void MEfDrawPathTrail(MEfActorPathObject *object);

/*
 * Creates the path of an actor: the actor matrix is taken from the actor, the
 * curve's start and end are copied from it, the trail matrices are built, and
 * the callbacks of the object are set to the path's update, draw and advance.
 */
int MEfCreateActorPath(MEfActorPathObject *object)
{
    MEfPathWork *work;
    Matrix4 *matrix;
    int index;

    work = &object->work;
    matrix = &work->actorMatrix;
    work->stepCount = 0;
    MSeGetActorMatrix(matrix, work->actor, work->coordinate);
    MMathTranslateMatrix(matrix, matrix, &work->translation);
    __asm__ __volatile__(
        "lqc2 vf1, 48(%1)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 0(%0)\n"
        :
        : "r"(&work->actorTranslation), "r"(matrix)
        : "memory");
    MEfGetActorCoord(&work->actorCoord, work->endActor, work->endCoordinate);
    MEfBuildPathCurve(work);
    MEfBuildPathKeyframes(work);
    for (index = MEF_PATH_HEAD_SLOT; index < MEF_PATH_TRAIL_COUNT; index++) {
        QuadwordCopyMatrixT0(work->trail[index], work->trail[0]);
    }
    MGsGPInit(&work->gsPacket, 0, 0);
    object->updateTrail = MEfUpdatePathTrail;
    object->drawTrail = MEfDrawPathTrail;
    object->advanceStep = MEfAdvancePathStep;
    return 1;
}

/*
 * Fills the path curve from the actor: its start and end are the actor translation
 * and the actor coordinate, its middle is the actor matrix applied to an offset along z,
 * and its end offset is the rotated direction from middle to end.
 */
void MEfBuildPathCurve(MEfPathWork *work)
{
    MEfPathCurve *curve;
    Vector4 offset;
    Vector4 angles;
    Vector4 targetAngles;
    Matrix4 rotation;

    curve = &mefPathCurve.curve;
    MEfHermitePathInit(curve, 4);
    __asm__ __volatile__("lq $8, 0(%2)\n"
        "sq $8, 0(%1)"
        : "=m"(curve->points[0])
        : "r"(&curve->points[0]), "r"(&work->actorTranslation)
        : "$8");
    __asm__ __volatile__("lq $8, 0(%3)\n"
        "sq $8, 0(%2)"
        : "=m"(curve->points[3]), "=m"(curve->points[3].w)
        : "r"(&curve->points[3]), "r"(&work->actorCoord)
        : "$8");
    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");
    offset.z = MMathCalcDist(&work->actorTranslation, &work->actorCoord) * 0.33333298f;
    MMathApplyMatrix(&curve->points[1], &work->actorMatrix, &offset);
    MMathCalcAngle(&angles, &curve->points[0], &curve->points[1]);
    MMathCalcAngle(&targetAngles, &curve->points[1], &curve->points[3]);
    MMathVectorInterpolation(&angles, &angles, &targetAngles, 0.5f);
    MMathRotateMatrixYX(&rotation, 0, &angles);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        : "=m"(rotation[3])
        : "r"(&rotation), "r"(&curve->points[1]), "m"(curve->points[1])
        : "memory");
    __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");
    offset.z = MMathCalcDist(&curve->points[1], &curve->points[3]) * 0.5f;
    MMathApplyMatrix(&curve->points[2], &rotation, &offset);
}

/*
 * Samples the path curve into its 13 keys: each key is a Hermite point of the
 * curve's segment for key/4, at a fraction of key%4 quarters (the last key uses
 * segment 2 at fraction 1). Then trail matrices 0..11 are built from consecutive
 * key pairs, and trail matrix 12 is a copy of matrix 11 whose position row is
 * key 12.
 */
void MEfBuildPathKeyframes(MEfPathWork *work)
{
    MEfKeyedPathCurve *curve;
    int keyIndex;
    Matrix4 *matrix;

    curve = &mefPathCurve.keyed;
    for (keyIndex = 0; keyIndex < MEF_PATH_KEY_COUNT; keyIndex++) {
        int segment;
        int fraction;

        segment = keyIndex / 4;
        fraction = keyIndex % 4;
        if (segment == 3) {
            segment = 2;
            fraction = 4;
        }
        MMathCalcHermiteKeyPrm(&curve->head, segment);
        MMathCalcHermiteKey(&curve->keyPosition[keyIndex], &curve->head, segment, fraction * 0.25f);
    }
    matrix = work->trail;
    for (keyIndex = 0; keyIndex < 12; keyIndex++) {
        MMathCalcLookAtMatrix(matrix + keyIndex, &curve->keyPosition[keyIndex], &curve->keyPosition[keyIndex + 1], &curve->keyPosition[keyIndex], 0.0f);
    }
    QuadwordCopyMatrixT0(work->trail[12], work->trail[11]);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "vmove.w vf1, vf0\n"
        :
        : "r"(&curve->keyPosition[12])
        : "memory");
    __asm__ __volatile__(
        "sqc2 vf1, 48(%0)\n"
        :
        : "r"(&work->trail[12])
        : "memory");
}

/*
 * Moves the three oldest trail matrices one slot up, then copies the matrix the
 * step selects into slot 13, unless the last value is set or the step is 13
 * or above.
 */
void MEfUpdatePathTrail(MEfActorPathObject *object)
{
    MEfPathWork *work;
    Matrix4 *slot;
    int count;
    int trailIndex;

    work = &object->work;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    slot = work->trail;
    for (count = 3; count > 0; count--) {
        QuadwordCopyMatrixT0(slot + 16, slot + 15);
        slot--;
    }
    trailIndex = (short) work->stepCount;
    if (trailIndex < MEF_PATH_GROW_STEPS) {
        /* trail[trailIndex] as int address arithmetic: the operand order of its addu is part of the matched code. */
        QuadwordCopyMatrixT0(work->trail[MEF_PATH_HEAD_SLOT], (Matrix4 *) (trailIndex * 64 + (int) work + 0xC0));
    }
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

/* Writes a GIF tag for regCount registers and returns its first data qword. */
static inline MEfGsQword *OpenTag(MGsPacket *packet, int pre, int prim, int regCount, u64 registers)
{
    MEfGsQword *qword = packet->current;

    qword->gif.tag = MEF_GIF_TAG_EOP | ((u64) pre << 46) | ((u64) prim << 47) | ((u64) regCount << 60);
    qword->gif.registers = registers;
    packet->regCount = regCount;
    return qword + 1;
}

/* RGBAQ of the two edges of the growing path. */
static const Quadword mefTrailGrowColors[2] = {
    0x00000080000000600000002000000080,
    0x00000080000000100000000000000020,
};

/* RGBAQ of the two edges of the path head. */
static const Quadword mefTrailHeadColors[2] = {
    0x000000FF000000300000001000000048,
    0x000000FF000000000000000000000010,
};

/* The cross-section of the trail: the offset of each of its four edges from the path. */
static const AlignedVector4 mefTrailSection[MEF_TRAIL_SECTIONS] = {
    { 0.02f, 0.0f, 0.9f, 1.0f },
    { 0.04f, 0.0f, 0.7f, 1.0f },
    { 0.039f, 0.0f, 0.5f, 1.0f },
    { 0.012f, 0.0f, -0.3f, 1.0f },
};

/*
 * Draw callback of the path trail. After the GS state, while the path is still
 * growing (step below 13), twenty strips of four vertex pairs sweep the trail
 * matrix of the step around its z axis and fade the first colour. Then four
 * strips join the last four trail matrices, fading both colours.
 */
void MEfDrawPathTrail(MEfActorPathObject *object)
{
    AlignedVector4 offset;
    Matrix4 rotation;
    Vector4 inner;
    Vector4 outer;
    MEfGsVertex edgeVertex[2];
    TrailQword edgeColour[2];
    MEfPathWork *work;
    MGsPacket *packet;
    MEfGsQword *header;
    MEfGsQword *firstData;
    MEfGsQword *write;  /* the next strip vertex of the open tag */
    struct XglCamera *camera;
    Matrix4 *view;
    short step;
    int adcCountdown;
    int i;
    int slotIndex;
    int sectionIndex;
    float angle;
    float sweep;

    work = &object->work;
    packet = &object->work.gsPacket;
    step = work->stepCount;
    camera = MEfCamParams.camera;
    view = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);

    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    firstData = header + 1;
    SetRegister(firstData, MEF_GS_TEST_1, MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);

    if (step < MEF_PATH_GROW_STEPS) {
        angle = 0.0f;
        for (i = 0; i < MEF_TRAIL_GROW_STRIPS; i++) {
            firstData = OpenTag(&work->gsPacket, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
            adcCountdown = 0;
            MMathRotateMatrixZ(&rotation, &work->trail[step], angle);
            if (i & 1) {
                sweep = MEF_TRAIL_SWEEP_ANGLE;
            } else {
                sweep = -MEF_TRAIL_SWEEP_ANGLE;
            }
            edgeColour[0].whole = mefTrailGrowColors[0];
            edgeColour[1].whole = mefTrailGrowColors[1];
            write = firstData;
            for (sectionIndex = 0; sectionIndex < MEF_TRAIL_SECTIONS; sectionIndex++) {
                __asm__ __volatile__("lq $8, 0(%4)\nsq $8, 0(%3)\n" : "=m"(offset), "=m"(offset.z), "=m"(offset.w) : "r"(&offset), "r"(&mefTrailSection[sectionIndex]) : "$8");
                MMathApplyMatrix(&outer, &rotation, &offset);
                offset.x = 0.0f;
                MMathApplyMatrix(&inner, &rotation, &offset);
                MMathRotateMatrixZ(&rotation, &rotation, sweep);
                if (MMathRotTransPersClip(&edgeVertex[0].vector, camera, view, &inner) != 0) {
                    adcCountdown = 2;
                } else if (adcCountdown != 0) {
                    edgeVertex[0].words[3] = MEF_GS_XYZ_ADC;
                    adcCountdown--;
                }
                if (MMathRotTransPersClip(&edgeVertex[1].vector, camera, view, &outer) != 0) {
                    adcCountdown = 2;
                } else if (adcCountdown != 0) {
                    edgeVertex[1].words[3] = MEF_GS_XYZ_ADC;
                    adcCountdown--;
                }
                write[0].whole = edgeColour[0].whole;
                write[1].whole = edgeVertex[0].whole;
                write[2].whole = edgeColour[1].whole;
                write[3].whole = edgeVertex[1].whole;
                write += 4;
                edgeColour[0].words[3] = edgeColour[0].words[3] * MEF_TRAIL_GROW_FADE_PERCENT / 100;
            }
            CloseTag(&work->gsPacket, 4);
            angle += MEF_TRAIL_GROW_ANGLE_STEP;
        }
    }

    angle = 0.0f;
    for (i = 0; i < MEF_TRAIL_HEAD_STRIPS; i++) {
        firstData = OpenTag(&work->gsPacket, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
        adcCountdown = 0;
        edgeColour[0].whole = mefTrailHeadColors[0];
        edgeColour[1].whole = mefTrailHeadColors[1];
        write = firstData;
        for (slotIndex = 0; slotIndex < MEF_TRAIL_HEAD_SLOTS; slotIndex++) {
            MMathRotateMatrixZ(&rotation, &work->trail[MEF_PATH_HEAD_SLOT + slotIndex], angle);
            Vu0SetOrigin(&offset);
            if (slotIndex < MEF_TRAIL_HEAD_SLOTS - 1) {
                offset.x = -0.003f;
            }
            MMathApplyMatrix(&inner, &rotation, &offset);
            Vu0SetOrigin(&offset);
            if (slotIndex < MEF_TRAIL_HEAD_SLOTS - 1) {
                offset.x = 0.026f;
            }
            MMathApplyMatrix(&outer, &rotation, &offset);
            if (MMathRotTransPersClip(&edgeVertex[0].vector, camera, view, &inner) != 0) {
                adcCountdown = 2;
            } else if (adcCountdown != 0) {
                edgeVertex[0].words[3] = MEF_GS_XYZ_ADC;
                adcCountdown--;
            }
            if (MMathRotTransPersClip(&edgeVertex[1].vector, camera, view, &outer) != 0) {
                adcCountdown = 2;
            } else if (adcCountdown != 0) {
                edgeVertex[1].words[3] = MEF_GS_XYZ_ADC;
                adcCountdown--;
            }
            write[0].whole = edgeColour[0].whole;
            write[1].whole = edgeVertex[0].whole;
            write[2].whole = edgeColour[1].whole;
            write[3].whole = edgeVertex[1].whole;
            write += 4;
            edgeColour[0].words[3] = edgeColour[0].words[3] * MEF_TRAIL_HEAD_FADE_PERCENT / 100;
            edgeColour[1].words[3] = edgeColour[1].words[3] * MEF_TRAIL_HEAD_FADE_PERCENT / 100;
        }
        CloseTag(&work->gsPacket, 4);
        angle += MEF_TRAIL_HEAD_ANGLE_STEP;
    }
    MGsGPTerm(&work->gsPacket);
}

/*
 * Post-process callback. Runs only while ov01EffectPauseGet returns 0; counts one
 * step and ends the effect once the step reaches 17 (0x11).
 */
void MEfAdvancePathStep(MEfActorPathObject *object) {
    MEfPathWork *work;
    int nextStepCount;

    work = &object->work;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    nextStepCount = work->stepCount + 1;
    work->stepCount = nextStepCount;
    if ((short) nextStepCount >= MEF_PATH_LIFETIME_STEPS) {
        MSeObjRequestFree(object);
    }
}
