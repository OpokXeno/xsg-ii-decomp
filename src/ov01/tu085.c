/*
 * OV01 TU 85: the wave effect (MEfCreateWaveEffect and its host callbacks).
 */

#include "common.h"
#include "shared.h"
#include "main/m_math_191.h"
#include "main/tu192.h"
#include "main/tu194.h"
#include "vu0_macro.h"
#include "main/tu197.h"
#include "main/tu187.h"
#include "quadword.h"
#include "ov01/tu075.h"
#include "main/tu190.h"
#include "main/m_math_193.h"
#include "main/tu196.h"

/* Two RGBAQ/XYZ2 pairs per loop: a register list of its own (the value equals the header's MEF_GS_STRIP_REGS). */
#define MEF_GS_RGBAQ_XYZ2_PAIR_REGS 0x5151
/* Samples per wave: the rows of the sine, blend, trail and history tables. */
#define MEF_WAVE_SAMPLE_COUNT 26
/* Tick count at which MEfAdvanceHistoryTrail ends the wave (it adds 2 per frame). */
#define MEF_WAVE_LIFETIME_TICKS 0x2F
/* Passes of the two draw loops of MEfDrawWaveEffect, and rings per pass. */
#define MEF_WAVE_PASS_COUNT 10
#define MEF_WAVE_RING_COUNT 4
/* Bits 4 and 5 of a history entry: when either is set the wave uses the first colour. */
#define MEF_WAVE_HISTORY_FLAG_MASK 0x30

/* A packed GS vertex: the projected point, or its words (w carries ADC). */
typedef union MEfGsVertex {
    AlignedVector4 vector;
    Quadword whole;
    int words[4];
} MEfGsVertex;

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

extern const MEfGsQword mefWaveColors[5];      /* colour quadwords */
extern const AlignedVector4 mefWaveTopOffsets[MEF_WAVE_RING_COUNT];  /* per-ring offset added to the top vertex (point0) */
extern const AlignedVector4 mefWaveBottomOffsets[MEF_WAVE_RING_COUNT];  /* per-ring offset added to the bottom vertex (point1) */

#define QuadwordCopyT0Dst(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%3)\n" \
        "sq $8, 0(%2)\n" \
        : "=m" (*(destination)), "=m" ((destination)->w) \
        : "r" (destination), "r" (source) \
        : "$8", "memory")

#define QuadwordCopyT0Dst3(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%4)\n" \
        "sq $8, 0(%3)\n" \
        : "=m" (*(destination)), "=m" ((destination)->w), "=m" ((destination)->w) \
        : "r" (destination), "r" (source) \
        : "$8", "memory")

/* Copies a point into a matrix row and sets its w to 1 (lqc2, vmove, sqc2; as tu078). */
#define Vu0MMathCopyPointDst(destination, source) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%1)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, %0\n" \
        : "=m" (*(Vector4 *) (destination)) \
        : "r" (source) \
        : "memory")

/*
 * The wave block at object+0x20. The code at +0x54 selects the status word
 * (flags, +0x270) and the mode (+0x272) that the tick callback writes. The
 * same bytes are the tick count (+0x54) and the 26-entry history (+0x270)
 * that MEfAdvanceHistoryTrail, the advance callback of this object, shifts.
 */
typedef struct WaveBlock {
    unsigned char unmodeled_00[0x04];
    int actorId;                      /* +0x04: first argument of the start coordinate lookup */
    int actorIndex;                   /* +0x08: second argument of the start coordinate lookup */
    unsigned char unmodeled_0c[0x20 - 0x0c];
    Vector4 startOffset;              /* +0x20: added to the start vector */
    int endActorId;                   /* +0x30 */
    int endActorIndex;                /* +0x34 */
    unsigned char unmodeled_38[0x40 - 0x38];
    MGsPacket gsState;                /* +0x40: passed to MGsGPInit */
    union {
        short modeCode;                 /* +0x54: selects the mode below */
        u16 ticks;                    /* +0x54: tick count of MEfAdvanceHistoryTrail */
    };
    unsigned char unmodeled_56[0x60 - 0x56];
    Vector4 startVector;              /* +0x60: first endpoint of the blend */
    Vector4 endVector;                /* +0x70: second endpoint of the blend */
    float sampleScale;                /* +0x80: multiplies the sine samples */
    unsigned char unmodeled_84[0x90 - 0x84];
    Matrix4 matrix;                   /* +0x90: rotation matrix */
    Vector4 blendedSamples[MEF_WAVE_SAMPLE_COUNT];       /* +0xd0: one blended vector per sample */
    union {
        struct {
            unsigned short flags;     /* +0x270 */
            short mode;               /* +0x272 */
        };
        u16 history[MEF_WAVE_SAMPLE_COUNT];             /* +0x270: shifted by MEfAdvanceHistoryTrail */
    };
    unsigned char unmodeled_2a4[0x2b0 - 0x2a4];
    Vector4 sineSamples[MEF_WAVE_SAMPLE_COUNT];          /* +0x2b0: one vector per sample */
    Vector4 trailPoints[MEF_WAVE_SAMPLE_COUNT];          /* +0x450 */
    Vector4 trailSpeeds[MEF_WAVE_SAMPLE_COUNT];          /* +0x5f0 */
} WaveBlock;

typedef struct WaveHost {
    unsigned char unmodeled_00[0x08];
    void (*setMode)(struct WaveHost *); /* +0x08 */
    unsigned char unmodeled_0c[0x10 - 0x0c];
    void (*draw)(struct WaveHost *);    /* +0x10 */
    void (*advance)(struct WaveHost *); /* +0x14 */
    unsigned char unmodeled_18[0x20 - 0x18];
    WaveBlock block;                  /* +0x20 */
} WaveHost;

Vector4 *MEfGetActorCoord(Vector4 *coord, int actorId, int actorIndex);
void MEfBlendSineSamples(WaveBlock *block);
void MEfBuildSineSamples(WaveBlock *block);
void MEfSetWaveMode(WaveHost *host);
void MEfDrawWaveEffect(WaveHost *host);
void MEfAdvanceHistoryTrail(WaveHost *host);

short ov01LastValueGet(void);

int MEfCreateWaveEffect(WaveHost *host)
{
    WaveBlock *block;
    Vector4 *startPoint;
    Vector4 angles;
    float distance;

    block = &host->block;
    startPoint = &block->startVector;
    block->modeCode = 0;
    MEfGetActorCoord(&block->startVector, block->actorId, block->actorIndex);
    Vu0SefAddVectorXYZ(startPoint, startPoint, &block->startOffset);
    MEfGetActorCoord(&block->endVector, block->endActorId, block->endActorIndex);
    memset(block->blendedSamples, 0, sizeof(block->blendedSamples));
    memset(&block->flags, 0, sizeof(block->history));
    memset(block->sineSamples, 0, sizeof(block->sineSamples));
    memset(block->trailPoints, 0, sizeof(block->trailPoints));
    memset(block->trailSpeeds, 0, sizeof(block->trailSpeeds));
    distance = MMathCalcDist(&block->startVector, &block->endVector);
    block->sampleScale = distance * 0.375f;
    startPoint = &block->startVector;
    MMathCalcAngle(&angles, startPoint, &block->endVector);
    MMathRotateMatrixYX(&block->matrix, 0, &angles);
    MEfBlendSineSamples(block);
    MEfBuildSineSamples(block);
    MGsGPInit(&block->gsState, 0, 0);
    host->setMode = MEfSetWaveMode;
    host->draw = MEfDrawWaveEffect;
    host->advance = MEfAdvanceHistoryTrail;
    return 1;
}

void MEfBlendSineSamples(WaveBlock *block)
{
    int sample;
    float angle;
    float sine;
    Vector4 *blended;
    Vector4 *start;
    Vector4 *end;
    float stored;
    float raised;

    sample = 0;
    start = &block->startVector;
    end = &block->endVector;
    for (; sample < MEF_WAVE_SAMPLE_COUNT; sample++) {
        angle = (float)sample / 25.0f * 3.1415927f + 4.712389f;
        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2 $8, vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, vf1\n\t"
            "mtc1 $8, %0"
            : "=f"(sine) : "f"(angle) : "$8");
        raised = sine + 1.0f;
        blended = &block->blendedSamples[sample];
        /* stored is a dummy memory output: it makes the asm store sine to memory. */
        __asm__("swc1 %1, %0" : "=m"(stored) : "f"(sine));
        MMathVectorInterpolation(blended, start, end, raised * 0.5f);
    }
}

void MEfBuildSineSamples(WaveBlock *block)
{
    int sample;
    float angle;
    float sine;
    float scaledSine;
    Vector4 *sampleVector;
    Vector4 *sampleX;

    sampleVector = block->sineSamples;
    sample = 0;
    sampleX = sampleVector;
    for (; sample < MEF_WAVE_SAMPLE_COUNT; sample++) {
        angle = (float)sample / 25.0f * 3.1415927f;
        __asm__ __volatile__(
            "mfc1 $8, %1\n\t"
            "qmtc2 $8, vf4\n\t"
            "vcallms 0x20\n\t"
            "qmfc2.i $8, vf1\n\t"
            "mtc1 $8, %0"
            : "=f"(sine) : "f"(angle) : "$8");
        scaledSine = sine * block->sampleScale;
        __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(sampleVector), "m"(sine) : "memory");
        sampleX->x = scaledSine + 0.1f;
        sampleVector++;
        sampleX++;
    }
}

void MEfSetWaveMode(WaveHost *host)
{
    WaveBlock *block;
    short modeCode;

    block = &host->block;
    if (ov01LastValueGet() == 0) {
        modeCode = block->modeCode;
        block->flags = 0;
        if (modeCode < 0x15) {
            block->flags = 1;
            block->mode = 1;
            if (modeCode == 0) {
                block->mode = 0x11;
            }
            if (modeCode == 0x14) {
                block->flags |= 0x20;
            }
        }
    }
}

void MEfDrawWaveEffect(WaveHost *host)
{
    WaveBlock *block;
    MGsPacket *packet;
    Matrix4 matrix;
    Vector4 point0;
    Vector4 point1;
    MEfGsVertex top;
    MEfGsVertex bottom;
    MEfGsQword *header;
    MEfGsQword *qword;
    struct XglCamera *camera;
    Matrix4 *view;
    int skip = 0; /* initialised here and again before each strip */
    int loopCount;
    int historyFlags;
    int clipped;
    int sample;
    int pass;
    int ring;
    float angle;
    Quadword color;

    block = &host->block;
    packet = &host->block.gsState;
    camera = MEfCamParams.camera;
    view = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);

    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    qword = header + 1;
    SetRegister(qword, MEF_GS_TEST_1, MEF_GS_TEST_1_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(&block->gsState,3);

    for (pass = 0; pass < MEF_WAVE_PASS_COUNT; pass++) {
        angle = (float) pass * 0.46542114f + -0.52359879f;
        MMathRotateMatrixZ(&matrix, &block->matrix, angle);

        for (sample = 0; sample < MEF_WAVE_SAMPLE_COUNT; sample++) {
            Vu0MMathCopyPointDst(matrix[3], &block->blendedSamples[sample]);
            MMathApplyMatrix(&block->trailPoints[sample], &matrix, &block->sineSamples[sample]);
        }
        for (sample = 0; sample < MEF_WAVE_SAMPLE_COUNT - 1; sample++) {
            MMathCalcAngle(&block->trailSpeeds[sample], &block->trailPoints[sample], &block->trailPoints[sample + 1]);
        }
        QuadwordCopyT0(&block->trailSpeeds[25], &block->trailSpeeds[24]);

        for (ring = 0; ring < MEF_WAVE_RING_COUNT; ring++) {
            qword = OpenTag(&block->gsState,1, MEF_GS_STRIP_PRIM, 4, MEF_GS_RGBAQ_XYZ2_PAIR_REGS);
            loopCount = 0;
            skip = 0;
            for (sample = 0; sample < MEF_WAVE_SAMPLE_COUNT; sample++) {
                if ((short) block->history[sample] == 0) {
                    continue;
                }
                historyFlags = block->history[sample] & MEF_WAVE_HISTORY_FLAG_MASK;
                if (historyFlags != 0) {
                    QuadwordCopyT0Dst(&point0, &block->trailPoints[sample]);
                    QuadwordCopyT0Dst(&point1, &block->trailPoints[sample]);
                } else {
                    MMathRotateMatrixYXZ(&matrix, 0, &block->trailSpeeds[sample]);
                    Vu0MMathCopyPointDst(matrix[3], &block->trailPoints[sample]);
                    MMathApplyMatrix(&point0, &matrix, &mefWaveTopOffsets[ring]);
                    MMathApplyMatrix(&point1, &matrix, &mefWaveBottomOffsets[ring]);
                }

                clipped = MMathRotTransPersClip(&top.vector, camera, view, &point0);
                if (clipped != 0) {
                    skip = 2;
                } else if (skip != 0) {
                    skip--;
                    top.words[3] = MEF_GS_XYZ_ADC;
                }
                clipped = MMathRotTransPersClip(&bottom.vector, camera, view, &point1);
                if (clipped != 0) {
                    skip = 2;
                } else if (skip != 0) {
                    skip--;
                    bottom.words[3] = MEF_GS_XYZ_ADC;
                }

                if (historyFlags != 0) {
                    qword[0].whole = mefWaveColors[0].whole;
                } else {
                    qword[0].whole = mefWaveColors[1].whole;
                }
                qword[1].whole = top.whole;
                if (historyFlags != 0) {
                    color = mefWaveColors[0].whole;
                } else {
                    color = mefWaveColors[2].whole;
                }
                qword[2].whole = color;
                qword[3].whole = bottom.whole;
                loopCount++;
                qword += 4;
            }
            CloseTag(&block->gsState,loopCount);
        }
        if (pass % 2 == 1) {
            MGsGPTerm(&block->gsState);
        }
    }

    for (pass = 0; pass < MEF_WAVE_PASS_COUNT; pass++) {
        angle = (float) pass * 0.46542114f + -0.52359879f;
        MMathRotateMatrixZ(&matrix, &block->matrix, angle);
        for (sample = 0; sample < MEF_WAVE_SAMPLE_COUNT; sample++) {
            Vu0MMathCopyPointDst(matrix[3], &block->blendedSamples[sample]);
            MMathApplyMatrix(&block->trailPoints[sample], &matrix, &block->sineSamples[sample]);
        }

        qword = OpenTag(&block->gsState,1, 0x4A, 2, MEF_GS_RGBAQ_XYZ2_REGS);
        loopCount = 0;
        skip = 0;
        for (sample = 0; sample < MEF_WAVE_SAMPLE_COUNT; sample++) {
            if ((short) block->history[sample] == 0) {
                continue;
            }
            historyFlags = block->history[sample] & MEF_WAVE_HISTORY_FLAG_MASK;
            QuadwordCopyT0Dst3(&point0, &block->trailPoints[sample]);
            clipped = MMathRotTransPersClip(&top.vector, camera, view, &point0);
            loopCount++;
            if (clipped != 0) {
                skip = 1;
            } else if (skip != 0) {
                skip--;
                top.words[3] = MEF_GS_XYZ_ADC;
            }
            if (historyFlags != 0) {
                qword[0].whole = mefWaveColors[3].whole;
            } else {
                qword[0].whole = mefWaveColors[4].whole;
            }
            qword[1].whole = top.whole;
            qword += 2;
        }
        CloseTag(&block->gsState,loopCount);
    }
    MGsGPTerm(&block->gsState);
}

void MEfAdvanceHistoryTrail(WaveHost *host)
{
    WaveBlock *block;
    s16 nextTicks;
    int sample;


    block = &host->block;
    if (ov01LastValueGet() != 0)
    {
        return;
    }
    /* The history shift is written twice, as the original does. */
    for (sample = MEF_WAVE_SAMPLE_COUNT - 1; sample > 0; sample--)
    {
        host->block.history[sample] = host->block.history[sample - 1];
    }
    for (sample = MEF_WAVE_SAMPLE_COUNT - 1; sample > 0; sample--)
    {
        block->history[sample] = block->history[sample - 1];
    }
    nextTicks = block->ticks + 2;
    block->ticks += 2;
    if (nextTicks >= MEF_WAVE_LIFETIME_TICKS)
    {
        MSeObjRequestFree(host);
    }
}

/* Wave tables: the five colour quadwords, then the per-ring offsets of the top and bottom vertices. */
const MEfGsQword mefWaveColors[5] = {
    0x000000ff000000000000000000000000,
    0x000000ff000000180000004200000050,
    0x000000ff000000000000000000000004,
    0x000000ff000000000000000000000000,
    0x000000ff000000280000007000000080,
};

const AlignedVector4 mefWaveTopOffsets[MEF_WAVE_RING_COUNT] = {
    { -0.005f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.005f, 0.0f, 1.0f },
    { 0.005f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.005f, 0.0f, 1.0f },
};

const AlignedVector4 mefWaveBottomOffsets[MEF_WAVE_RING_COUNT] = {
    { 0.3f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.3f, 0.0f, 1.0f },
    { -0.3f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.3f, 0.0f, 1.0f },
};
