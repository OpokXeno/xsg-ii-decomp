#include "common.h"
#include "shared.h"
#include "main/m_math.h"
#include "ov01/m_ef.h"
#include "vu0_macro.h"
#include "quadword.h"
#include "ov01/m_gs.h"
#include "main/m_se_obj.h"
#include "main/sm_initilize.h"

/* Samples of one history effect: one history entry, one path sample and one direction each. */
#define MEF_HISTORY_SAMPLES 19
/* Rings that the draw callback MEfDrawHistoryRings sweeps around the path. */
#define MEF_HISTORY_RINGS 8
/* Ring angle step (pi/4), sweep per status step (pi/60), angle between samples (pi/6) and the ribbon half width. */
#define MEF_HISTORY_RING_ANGLE_STEP 0.7853981f
#define MEF_HISTORY_SWEEP_STEP 0.05235988f
#define MEF_HISTORY_SAMPLE_ANGLE 0.5235988f
#define MEF_HISTORY_RIBBON_WIDTH 0.25f
/* MEfAdvanceHistoryStep ends the effect once the incremented step reaches this value. */
#define MEF_HISTORY_LIFETIME_STEPS 0x27
/* Status word of entry 0: active on the steps before MEF_HISTORY_STATUS_END_STEP, START on step 0,
 * FLAG added on step MEF_HISTORY_STATUS_FLAG_STEP. */
#define MEF_HISTORY_STATUS_ACTIVE 1
#define MEF_HISTORY_STATUS_START 0x11
#define MEF_HISTORY_STATUS_FLAG 0x20
#define MEF_HISTORY_STATUS_FLAG_STEP 0x13
#define MEF_HISTORY_STATUS_END_STEP 0x14

/*
 * Partial view of the pooled effect object that the callbacks of this TU
 * receive. Its three per-frame callbacks sit in the prefix (+0x08, +0x10 and
 * +0x14, stored by the creator MEfCreateActorHistory). The work record starts at +0x20.
 * The step count at +0x74 and the 19-entry history at +0xE0 of the object are
 * touched in this TU; history entry 0 is the status word.
 */

/* A quadword of the history ribbon's vertices: the header's MEfGsQword has no xyz or vector member. */
typedef union HistoryQword {
    Quadword whole;
    unsigned short nloop; /* low halfword of a GIF tag: NLOOP and EOP */
    struct {
        u64 tag;
        u64 registers;
    } gif;
    struct {
        u64 data;
        unsigned char address;
    } ad;
    struct {
        int x, y, z, adc;
    } xyz;
    Vector4 vector;
} HistoryQword;

typedef struct MEfHistoryWork MEfHistoryWork;

struct MEfHistoryWork {
    unsigned char unmodeled_00[0x0C];
    struct XenoActor *actor;               /* +0x0C: first actor of MSeGetActorMatrix */
    u32 coordinate;             /* +0x10: its coordinate argument */
    unsigned char unmodeled_14[0x20 - 0x14];
    Vector4 translation;        /* +0x20: translation applied by MMathTranslateMatrix */
    struct XenoActor *endActor;            /* +0x30: actor of MEfGetActorCoord */
    u32 endCoordinate;          /* +0x34: its coordinate argument */
    unsigned char unmodeled_38[0x40 - 0x38];
    MGsPacket gsPacket;         /* +0x40: GS packet cleared by MGsGPInit */
    unsigned short stepCount;   /* +0x54 of the work (+0x74 of the object): frames since creation */
    unsigned char unmodeled_56[0x60 - 0x56];
    Vector4 actorPosition;      /* +0x60: row 3 of the actor matrix (w = 1), copied by the creator */
    Vector4 actorCoord;         /* +0x70: MEfGetActorCoord output */
    Matrix4 actorMatrix;        /* +0x80: MSeGetActorMatrix output */
    short history[MEF_HISTORY_SAMPLES]; /* +0xC0 of the work (+0xE0 of the object): entry 0 is the status word */
    unsigned char unmodeled_E6[0xF0 - 0xE6];
    Vector4 pathSamples[MEF_HISTORY_SAMPLES]; /* +0xF0: the path samples (MEfBuildPathSamples) */
    Vector4 pathDirections[MEF_HISTORY_SAMPLES]; /* +0x220: the 18 directions, then a copy of the last */
};

typedef struct MEfHistoryObject MEfHistoryObject;

struct MEfHistoryObject {
    unsigned char unmodeled_00[0x08];
    void (*statusCallback)(MEfHistoryObject *object);   /* +0x08: MEfUpdatePathStatus */
    unsigned char unmodeled_0C[0x10 - 0x0C];
    void (*frameCallback)(MEfHistoryObject *object);    /* +0x10: MEfDrawHistoryRings, the draw callback */
    void (*advanceCallback)(MEfHistoryObject *object); /* +0x14: MEfAdvanceHistoryStep */
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfHistoryWork work; /* +0x20 */
};

void *memset(void *destination, int value, unsigned int size);
void MEfAdvanceHistoryStep(MEfHistoryObject *object);
void MEfDrawHistoryRings(MEfHistoryObject *object);
void MEfUpdatePathStatus(MEfHistoryObject *object);
void MEfBuildHistoryCurve(MEfHistoryWork *work);
void MEfBuildPathSamples(MEfHistoryWork *work);



/*
 * Effect creator: clears the work record, builds the actor matrix and its row 3
 * point, the coordinate and the path samples and directions, then installs the
 * three per-frame callbacks. Returns 1.
 */
int MEfCreateActorHistory(MEfHistoryObject *object)
{
    MEfHistoryWork *work;

    work = &object->work;
    work->stepCount = 0;
    MSeGetActorMatrix(&work->actorMatrix, work->actor, work->coordinate);
    MMathTranslateMatrix(&work->actorMatrix, &work->actorMatrix, &work->translation);
    __asm__ __volatile__(
        "lqc2 vf1, 48(%1)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 0(%0)\n"
        :
        : "r" (&work->actorPosition), "r" (&work->actorMatrix)
        : "memory");
    MEfGetActorCoord(&work->actorCoord, work->endActor, work->endCoordinate);
    memset(work->history, 0, sizeof(work->history));
    memset(work->pathSamples, 0, sizeof(work->pathSamples));
    memset(work->pathDirections, 0, sizeof(work->pathDirections));
    MEfBuildHistoryCurve(work);
    MEfBuildPathSamples(work);
    MGsGPInit(&work->gsPacket, 0, 0);
    object->statusCallback = MEfUpdatePathStatus;
    object->frameCallback = MEfDrawHistoryRings;
    object->advanceCallback = MEfAdvanceHistoryStep;
    return 1;
}

/*
 * Fills the path curve of the history effect from the actor position and the actor
 * coordinate, the same way MEfBuildPathCurve does for the path effect: the middle is the
 * actor matrix applied to an offset along z, and the end offset is the rotated direction
 * from middle to end.
 */
void MEfBuildHistoryCurve(MEfHistoryWork *work)
{
    AlignedVector4 translation;
    AlignedVector4 angles;
    AlignedVector4 targetAngles;
    Matrix4 rotation;
    MEfPathCurve *curve;
    Vector4 *startPosition;
    Vector4 *midPosition;
    Vector4 *endPosition;
    float length;

    curve = &mefPathCurve.curve;
    startPosition = &curve->points[0];
    MEfHermitePathInit(curve, 4);
    __asm__ __volatile__(
        "lq $8, 0(%2)\n"
        "sq $8, 0(%1)\n"
        : "=m" (*startPosition)
        : "r" (startPosition), "r" (&work->actorPosition)
        : "$8");
    __asm__ __volatile__(
        "lq $8, 0(%2)\n"
        "sq $8, 0(%1)\n"
        : "=m" (curve->points[3])
        : "r" (&curve->points[3]), "r" (&work->actorCoord), "m" (curve->points[3])
        : "$8");
    Vu0SetOrigin(&translation);
    midPosition = &curve->points[1];
    length = MMathCalcDist(&work->actorPosition, &work->actorCoord);
    translation.z = length * 0.333333f;
    MMathApplyMatrix(midPosition, &work->actorMatrix, &translation);
    MMathCalcAngle(&angles, startPosition, midPosition);
    endPosition = &curve->points[3];
    MMathCalcAngle(&targetAngles, midPosition, endPosition);
    MMathVectorInterpolation(&angles, &angles, &targetAngles, 0.5f);
    MMathRotateMatrixYX(&rotation, 0, &angles);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        :
        : "r" (midPosition), "r" (rotation), "m" (*midPosition)
        : "memory");
    Vu0SetOrigin(&translation);
    length = MMathCalcDist(midPosition, endPosition);
    translation.z = length * 0.5f;
    MMathApplyMatrix(&curve->points[2], &rotation, &translation);
}

/*
 * Builds the 19 path samples along the curve (Hermite keys at 1/6 steps) and the
 * 18 directions between consecutive samples; the last direction repeats.
 */
void MEfBuildPathSamples(MEfHistoryWork *work)
{
    Matrix4 frame;
    MEfPathCurve *curve = &mefPathCurve.curve;
    Vector4 *store;
    Vector4 *samples;
    int index;

    QuadwordCopyMatrixT0(frame, &work->actorMatrix);
    Vu0MMathSetTranslation(frame, &MMathUnitVector);
    samples = work->pathSamples;
    for (index = 0; index < MEF_HISTORY_SAMPLES; index++) {
        int segment;
        int step;
        AlignedVector4 sample;

        segment = index / 6;
        step = index % 6;
        if (segment == 3) {
            segment = 2;
            step = 6;
        }
        MMathCalcHermiteKeyPrm(curve, segment);
        MMathCalcHermiteKey(&sample, curve, segment, (1.0f / 6.0f) * step);
        store = samples + index;
        __asm__ __volatile__(
            "lq $8, 0(%5)\n"
            "sq $8, 0(%4)\n"
            : "=m" (store->w), "=m" (store->w), "=m" (store->w), "=m" (store->w)
            : "r" (store), "r" (&sample), "r" (store)
            : "$8");
    }
    for (index = 0; index < MEF_HISTORY_SAMPLES - 1; index++) {
        MMathCalcAngle(&work->pathDirections[index], &work->pathSamples[index], &work->pathSamples[index + 1]);
    }
    QuadwordCopyT0(&work->pathDirections[MEF_HISTORY_SAMPLES - 1], &work->pathDirections[MEF_HISTORY_SAMPLES - 2]);
}

/*
 * Status callback. While the effect runs, the status word (history[0]) follows
 * the step count: 1 on steps 0 to 19, 0x11 on step 0 and bit 0x20 added on step
 * 19 (0x13).
 * Step 20 and later clear it.
 */
void MEfUpdatePathStatus(MEfHistoryObject *object) {
    MEfHistoryWork *work;
    short stepCount;

    work = &object->work;
    if (ov01EffectPauseGet() == 0) {
        stepCount = work->stepCount;
        work->history[0] = 0;
        if (stepCount < MEF_HISTORY_STATUS_END_STEP) {
            work->history[0] = MEF_HISTORY_STATUS_ACTIVE;
            if (stepCount == 0) {
                work->history[0] = MEF_HISTORY_STATUS_START;
            }
            if (stepCount == MEF_HISTORY_STATUS_FLAG_STEP) {
                work->history[0] |= MEF_HISTORY_STATUS_FLAG;
            }
        }
    }
}


/* Writes the GIF tag that opens the packet's next primitive and returns the first data quadword after it. */
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

    size = packet->regCount * nloop + 1;
    packet->current->nloop |= nloop;
    packet->current += size;
    packet->count += size;
}

/*
 * Draw callback of the history effect. After the GS setup tag and its TEST, ZBUF and
 * ALPHA registers, it writes MEF_HISTORY_RINGS strips. Each strip runs over the path samples
 * whose history entry is set, two vertices per sample, and the ring angle grows per sample.
 * Clipped vertices get the ADC flag. MGsGPTerm ends the packet.
 */
void MEfDrawHistoryRings(MEfHistoryObject *object)
{
    /* The two ring vertex colours, defined (const) at the end of this file. */
    extern MEfGsQword mefRingColors[2];
    Matrix4 sampleMatrix;
    Vector4 transformed;
    HistoryQword firstVertex;
    HistoryQword secondVertex;
    Vector4 edgeOffset;
    MEfHistoryWork *work;
    MGsPacket *setupPacket;
    MEfGsQword *setupHeader;
    MEfGsQword *stripCursor;
    struct XglCamera *camera;
    Matrix4 *view;
    float angle;
    Vector4 *direction;
    Vector4 *position;
    Vector4 *origin;
    Vector4 *edgePoint;
    float width;
    int adcCountdown;
    int quadCount;
    int ring;
    int sampleIndex;
    MGsPacket *stripPacket;

    camera = MEfCamParams.camera;
    view = &MEfCamParams.viewMatrix;
    setupPacket = &object->work.gsPacket;
    MGsGPInit(setupPacket, 0, 0);
    setupHeader = setupPacket->current;
    setupHeader[0].gif.tag = MEF_GS_SETUP_TAG;
    setupHeader[0].gif.registers = MEF_GS_SETUP_REGS;
    setupPacket->regCount = 1;
    work = &object->work;
    SetRegister(&setupHeader[1], MEF_GS_TEST_1, MEF_GS_TEST_1_ZGEQUAL);
    SetRegister(&setupHeader[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&setupHeader[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(setupPacket, 3);
    origin = &edgeOffset;
    __asm__ __volatile__("sqc2 $vf0,0(%2)\n\t" : "=m"(*origin), "=m"(origin->w) : "r"(origin) : "memory");
    edgeOffset.x = 0.1f;
    for (ring = 0; ring < MEF_HISTORY_RINGS; ring++) {
        quadCount = 0;
        adcCountdown = 0;
        stripPacket = &work->gsPacket;
        stripCursor = OpenTag(&work->gsPacket, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
        angle = ring * MEF_HISTORY_RING_ANGLE_STEP + (short) work->stepCount * MEF_HISTORY_SWEEP_STEP;
        for (sampleIndex = 0; sampleIndex < MEF_HISTORY_SAMPLES;) {
            if (work->history[sampleIndex] != 0) {
                direction = &work->pathDirections[sampleIndex];
                position = &work->pathSamples[sampleIndex];
                direction->z = angle;
                MMathRotateMatrixYXZ(&sampleMatrix, 0, direction);
                __asm__ __volatile__(
                    "lqc2 $vf1,0(%0)\n\t"
                    "vmove.w $vf1w,$vf0w\n\t"
                    "sqc2 $vf1,48(%1)\n\t"
                    : : "r"(position), "r"(&sampleMatrix) : "memory");
                if ((work->history[sampleIndex] & 0x30) != 0 || sampleIndex == 0 || sampleIndex == MEF_HISTORY_SAMPLES - 1) {
                    width = 0.0f;
                } else {
                    width = MEF_HISTORY_RIBBON_WIDTH;
                }
                edgeOffset.z = width;
                edgePoint = &transformed;
                MMathApplyMatrix(edgePoint, &sampleMatrix, origin);
                if (MMathRotTransPersClip(&firstVertex.vector, camera, view, edgePoint) != 0) {
                    adcCountdown = 2;
                } else if (adcCountdown != 0) {
                    adcCountdown--;
                    firstVertex.xyz.adc = MEF_GS_XYZ_ADC;
                }
                edgeOffset.z = -width;
                MMathApplyMatrix(edgePoint, &sampleMatrix, origin);
                if (MMathRotTransPersClip(&secondVertex.vector, camera, view, edgePoint) != 0) {
                    adcCountdown = 2;
                } else if (adcCountdown != 0) {
                    adcCountdown--;
                    secondVertex.xyz.adc = MEF_GS_XYZ_ADC;
                }
                quadCount++;
                stripCursor[0].whole = mefRingColors[0].whole;
                stripCursor[1].whole = firstVertex.whole;
                stripCursor[2].whole = mefRingColors[1].whole;
                stripCursor[3].whole = secondVertex.whole;
                stripCursor += 4;
            }
            sampleIndex++;
            angle += -MEF_HISTORY_SAMPLE_ANGLE;
        }
        if (quadCount > 0) {
            CloseTag(stripPacket, quadCount);
        }
    }
    MGsGPTerm(stripPacket);
}


/*
 * Returns unless ov01EffectPauseGet reports 0. Otherwise shifts history[0..17] up
 * by one entry, advances the step counter, and calls MSeObjRequestFree once the
 * incremented step reaches 0x27.
 */
void MEfAdvanceHistoryStep(MEfHistoryObject *object)
{
    MEfHistoryWork *work;
    short nextStep;
    int index;

    work = &object->work;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    index = MEF_HISTORY_SAMPLES - 1;
    do {
        work->history[index] = work->history[index - 1];
        index -= 1;
    } while (index > 0);
    nextStep = (short) (work->stepCount + 1);
    work->stepCount = work->stepCount + 1;
    if (nextStep >= MEF_HISTORY_LIFETIME_STEPS) {
        MSeObjRequestFree(object);
    }
}

/* RGBAQ colours of the outer and inner ring vertices. */
const MEfGsQword mefRingColors[2] = {
    { 0x00000060000000200000005800000080 },
    { 0x00000020000000000000001000000030 },
};
