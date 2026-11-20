#include "common.h"

#include "quadword.h"

#include "ov01/m_gs.h"

#include "shared.h"
#include "ov01/m_ef.h"
#include "main/m_math.h"

#include "vu0_macro.h"

#include "main/m_se_obj.h"

/* The flag window: one status word and one position per history tick, sixteen strips around it. */
#define FLAG_WINDOW_POINTS 25
#define FLAG_WINDOW_STRIPS 16
/* Status bits of a history tick: a live tick, the tick 0 sample (START), the tick 29 sample (END). */
#define FLAG_WINDOW_VALID 0x01U
#define FLAG_WINDOW_START 0x10
#define FLAG_WINDOW_END 0x20
/* The tick a status word is set from is valid below ACTIVE_TICKS, END_TICK marks END, the effect frees at LIFETIME_TICKS. */
#define FLAG_WINDOW_ACTIVE_TICKS 0x1E
#define FLAG_WINDOW_END_TICK 0x1D
#define FLAG_WINDOW_LIFETIME_TICKS 0x37

/* Writes a GIF tag for regCount registers and returns its first data qword. */
static inline MEfGsQword *OpenTag(MGsPacket *packet, int pre, int prim, int regCount, u64 registers)
{
    MEfGsQword *qword = packet->current;

    qword->gif.tag = 0x8000 | ((u64) pre << 46) | ((u64) prim << 47) | ((u64) regCount << 60);
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
 * The flag-window block at object+0x20. The creator reads the actor pair at +0x0C..+0x34 and the two samples
 * at +0x60 and +0x70 and builds the path from them; the draw and advance callbacks read the status words,
 * positions and angles. statusWords[0] holds the status word of the current tick and [1..24] its history.
 */
typedef struct FlagWindowBlock FlagWindowBlock;

struct FlagWindowBlock {
    unsigned char unmodeled_00[0x0C];
    struct XenoActor *firstActor;               /* +0x0C */
    unsigned int firstCoordinate;           /* +0x10 */
    unsigned char unmodeled_14[0x30 - 0x14];
    struct XenoActor *secondActor;              /* +0x30 */
    unsigned int secondCoordinate;          /* +0x34 */
    unsigned char unmodeled_38[0x40 - 0x38];
    MGsPacket packet;                       /* +0x40 */
    s16 ticks;                              /* +0x54 */
    unsigned char unmodeled_56[0x60 - 0x56];
    AlignedVector4 sample;                  /* +0x60 */
    AlignedVector4 prevSample;              /* +0x70 */
    Matrix4 actorMatrix;                    /* +0x80: the first actor's matrix */
    u16 statusWords[FLAG_WINDOW_POINTS];                   /* +0xC0 */
    unsigned char unmodeled_F2[0x100 - 0xF2];
    AlignedVector4 positions[FLAG_WINDOW_POINTS];           /* +0x100 */
    AlignedVector4 angles[FLAG_WINDOW_POINTS];              /* +0x290 */
};

typedef struct FlagWindowHost FlagWindowHost;

struct FlagWindowHost {
    unsigned char unmodeled_00[0x4];
    int resourceType;                       /* +0x04 */
    void (*step)(FlagWindowHost *host);     /* +0x08 */
    unsigned char unmodeled_0C[0x4];
    void (*draw)(FlagWindowHost *host);     /* +0x10 */
    void (*advance)(FlagWindowHost *host);  /* +0x14 */
    unsigned char unmodeled_18[0x8];
    FlagWindowBlock block;                  /* +0x20 */
};


void MEfSetFlagWindowPhaseFlags(FlagWindowHost *object);
void MEfDrawFlagWindow(FlagWindowHost *object);
void MEfAdvanceFlagWindow(FlagWindowHost *object);
void *memset(void *dst, int value, unsigned int count);
void MEfFillFlagWindowSamples(FlagWindowBlock *block);
void MEfMakeFlagWindowPath(FlagWindowBlock *block);

int MEfCreateFlagWindow(FlagWindowHost *object)
{
    FlagWindowBlock *block;

    block = &object->block;
    block->ticks = 0;
    MEfGetActorCoord(&block->sample, block->firstActor, block->firstCoordinate);
    MEfGetActorCoord(&block->prevSample, block->secondActor, block->secondCoordinate);
    MSeGetActorMatrix(&block->actorMatrix, block->firstActor, block->firstCoordinate);
    memset(block->statusWords, 0, sizeof(block->statusWords));
    memset(block->positions, 0, sizeof(block->positions));
    memset(block->angles, 0, sizeof(block->angles));
    MEfMakeFlagWindowPath(block);
    MEfFillFlagWindowSamples(block);
    MGsGPInit(&block->packet, 0, 0);
    object->step = MEfSetFlagWindowPhaseFlags;
    object->draw = MEfDrawFlagWindow;
    object->advance = MEfAdvanceFlagWindow;
    return 1;
}

void MEfMakeFlagWindowPath(FlagWindowBlock *block)
{
    MEfPathCurve *curve;
    AlignedVector4 *key1;
    AlignedVector4 *key2;
    AlignedVector4 *key3;
    AlignedVector4 direction;
    AlignedVector4 previousAngle;
    AlignedVector4 nextAngle;
    Matrix4 frame;
    float segmentLength;
    float turnScale;

    curve = &mefPathCurve.curve;
    key1 = &curve->points[0];
    segmentLength = MMathCalcDist(&block->sample, &block->prevSample) * 0.5f;
    MEfHermitePathInit(curve, 4);
    __asm__ __volatile__(
        "lq $8, 0(%3)\n"
        "sq $8, 0(%2)\n"
        : "=m" (*key1), "=m" (*key1)
        : "r" (key1), "r" (&block->sample)
        : "$8");
    key3 = &curve->points[2];
    __asm__ __volatile__(
        "lq $8, 0(%2)\n"
        "sq $8, 0(%1)\n"
        : "=m" (*key3)
        : "r" (key3), "r" (&block->prevSample)
        : "$8");
    __asm__ __volatile__("sqc2 vf0, 0(%0)\n" : : "r" (&direction) : "memory");
    direction.z = segmentLength;
    key2 = &curve->points[1];
    MMathApplyMatrix(key2, &block->actorMatrix, &direction);
    MMathCalcAngle(&previousAngle, key1, key2);
    MMathCalcAngle(&nextAngle, key2, key3);
    Vu0SefSubVectorXYZ(&direction, &nextAngle, &previousAngle);
    turnScale = MMathMakeRandom() + 1.0f;
    Vu0SefScaleVectorXYZNI(&direction, &direction, turnScale);
    Vu0SefAddVectorXYZ(&direction, &nextAngle, &direction);
    MMathRotateMatrixYX(&frame, 0, &direction);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        : : "r" (key3), "r" (&frame[0]), "m" (*key3) : "memory");
    __asm__ __volatile__("sqc2 vf0, 0(%0)\n" : : "r" (&direction) : "memory");
    direction.z = segmentLength;
    MMathApplyMatrix(&curve->points[3], &frame, &direction);
}

void MEfFillFlagWindowSamples(FlagWindowBlock *block)
{
    Matrix4 frame;
    AlignedVector4 sample;
    MEfPathCurve *curve;
    float parameter;
    int index;
    int segment;
    int step;

    curve = &mefPathCurve.curve;
    QuadwordCopyMatrixT0(frame, &block->actorMatrix);
    Vu0MMathSetTranslation(frame, &MMathUnitVector);
    index = 0;
    do {
        segment = index / 8;
        step = index - segment * 8;
        if (segment == 3) {
            segment = 2;
            step = 8;
        }
        MMathCalcHermiteKeyPrm(curve, segment);
        parameter = step * 0.125f;
        MMathCalcHermiteKey(&sample, curve, segment, parameter);
        __asm__ __volatile__(
            "lq $8, 0(%4)\n"
            "sq $8, 0(%5)\n"
            : "=m" (block->positions[index].w), "=m" (block->positions[index].w), "=m" (block->positions[index].w), "=m" (block->positions[index].w)
            : "r" (&sample), "r" (&block->positions[index])
            : "$8");
        index++;
    } while (index < FLAG_WINDOW_POINTS);
    for (index = 0; index < 24; index++) {
        MMathCalcAngle(&block->angles[index], &block->positions[index], &block->positions[index + 1]);
    }
    QuadwordCopyT0(&block->angles[24], &block->angles[23]);
}

void MEfSetFlagWindowPhaseFlags(FlagWindowHost *object) {
    FlagWindowBlock *block;
    s16 ticks;

    block = &object->block;
    if (ov01EffectPauseGet() == 0) {
        ticks = block->ticks;
        block->statusWords[0] = 0U;
        if (ticks < FLAG_WINDOW_ACTIVE_TICKS) {
            block->statusWords[0] = FLAG_WINDOW_VALID;
            if (ticks == 0) {
                block->statusWords[0] = 0x11U;
            }
            if (ticks == FLAG_WINDOW_END_TICK) {
                block->statusWords[0] |= FLAG_WINDOW_END;
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

/*
 * Draw callback: writes the GS state, then sixteen line strips around the
 * window. Each strip takes the 25 history points whose status word is set,
 * rotated by the strip angle and pushed out by a cosine of their age.
 */
void MEfDrawFlagWindow(FlagWindowHost *object)
{
    FlagWindowBlock *block;
    MGsPacket *packet;
    Matrix4 pointFrame;
    Vector4 worldPoint;
    MEfGsVertex vertex;
    AlignedVector4 localOffset;
    /* The strip colour: dark red, opaque. */
    static const Quadword stripColor = 0x000000FF000000000000000000000040;
    MEfGsVertex rgbaq;
    Vector4 cosine;
    AlignedVector4 *pointAngle;
    MEfGsQword *header;
    MEfGsQword *qword;
    struct XglCamera *camera;
    Matrix4 *cameraMatrix;
    int statusWord;
    short startFlag;
    short endFlag;
    int pointCount;
    int hiddenRun;
    int clipResult;
    int historyIndex;
    int stripIndex;
    float stripAngle;
    float phase;

    block = &object->block;
    packet = &object->block.packet;
    camera = MEfCamParams.camera;
    cameraMatrix = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);

    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    qword = header + 1;
    SetRegister(qword, MEF_GS_TEST_1, MEF_GS_TEST_1_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);

    __asm__ __volatile__("sqc2 $vf0, 0(%2)\n" : "=m"(localOffset), "=m"(localOffset.w) : "r"(&localOffset), "r"(&localOffset));
    rgbaq.whole = stripColor;

    for (stripIndex = 0; stripIndex < FLAG_WINDOW_STRIPS; stripIndex++) {
        qword = OpenTag(&block->packet, 1, 0x4A, 2, 0x51);
        phase = (float) block->ticks * -0.10471976f;
        stripAngle = (float) stripIndex * 0.39269906f + phase;
        pointCount = 0;
        hiddenRun = 0;
        for (historyIndex = 0; historyIndex < FLAG_WINDOW_POINTS; historyIndex++) {
            statusWord = block->statusWords[historyIndex];
            if ((short) block->statusWords[historyIndex] == 0) {
                continue;
            }
            phase = (float) (historyIndex - block->ticks) * 1.0471976f;
            Vu0Cos(cosine.x, phase);
            localOffset.x = cosine.x * 0.2f + 0.25f;
            if ((startFlag = statusWord & FLAG_WINDOW_START) != 0 || (endFlag = statusWord & FLAG_WINDOW_END) != 0) {
                localOffset.x = 0.0f;
            }
            pointAngle = &block->angles[historyIndex];
            pointAngle->z = stripAngle;
            MMathRotateMatrixYXZ(&pointFrame, 0, pointAngle);
            Vu0MMathCopyPointDst(pointFrame[3], &block->positions[historyIndex]);
            MMathApplyMatrix(&worldPoint, &pointFrame, &localOffset);
            clipResult = MMathRotTransPersClip(&vertex.vector, camera, cameraMatrix, &worldPoint);
            pointCount++;
            if (clipResult != 0) {
                hiddenRun = 1;
            } else if (hiddenRun != 0) {
                hiddenRun--;
                vertex.words[3] = 0x8000;
            }
            qword[0].whole = rgbaq.whole;
            qword[1].whole = vertex.whole;
            qword += 2;
        }
        CloseTag(&block->packet, pointCount);
    }
    MGsGPTerm(&block->packet);
}

void MEfAdvanceFlagWindow(FlagWindowHost *object) {
    FlagWindowBlock *block;
    int historyIndex;
    s16 ticks;

    block = &object->block;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    for (historyIndex = FLAG_WINDOW_POINTS - 1; historyIndex > 0; historyIndex--) {
        block->statusWords[historyIndex] = block->statusWords[historyIndex - 1];
    }
    ticks = block->ticks + 1;
    block->ticks = block->ticks + 1;
    if (ticks >= FLAG_WINDOW_LIFETIME_TICKS) {
        MSeObjRequestFree(object);
    }
}
