#include "common.h"

#include "shared.h"
#include "ov01/m_ef.h"
#include "main/m_math.h"

#include "quadword.h"

#include "ov01/m_gs.h"

#include "main/m_se_obj.h"

#include "vu0_macro.h"

/* The path curve samples: ring[0..28] hold them, ring[29..57] the directions between them. */
#define MEF_PATH_SAMPLES 29
/* The smoke: nine trails of five points each, newest trail first. */
#define MEF_SMOKE_TRAILS 9
#define MEF_SMOKE_POINTS 5
/* A trail frees its effect at this tick count. */
#define MEF_TRAIL_LIFETIME_TICKS 0x26


/*
 * The trail block at object+0x20: the actor pair and their coordinates at
 * +0x0C..+0x34, the GS packet at +0x40, a 16-bit tick count at +0x54, two
 * 16-byte samples at +0x60 and +0x70, and a ring of samples from +0x80 indexed
 * by the tick count.
 */
typedef struct TrailBlock {
    unsigned char unmodeled_00[0x04];
    struct XenoActor *firstActor;               /* +0x04 */
    unsigned int firstCoordinate;           /* +0x08 */
    unsigned char unmodeled_0C[0x30 - 0x0C];
    struct XenoActor *secondActor;              /* +0x30 */
    unsigned int secondCoordinate;          /* +0x34 */
    unsigned char unmodeled_38[0x08];
    MGsPacket packet;                       /* +0x40 */
    unsigned short ticks;                   /* +0x54 */
    unsigned char unmodeled_56[0x60 - 0x56];
    AlignedVector4 sample;                  /* +0x60 */
    AlignedVector4 prevSample;             /* +0x70 */
    AlignedVector4 ring[58];                /* +0x80 */
    short smokeActive[MEF_SMOKE_TRAILS];                   /* +0x420: one flag per smoke trail */
    unsigned char unmodeled_432[0x440 - 0x432];
    AlignedVector4 smokePoints[MEF_SMOKE_TRAILS][MEF_SMOKE_POINTS];       /* +0x440: trail 0 is the newest */
    float smokeAngles[MEF_SMOKE_TRAILS][MEF_SMOKE_POINTS];                /* +0x710 */
} TrailBlock;

typedef struct TrailHost {
    unsigned char unmodeled_00[0x4];
    int resourceType;
    void (*step)(struct TrailHost *);
    void (*drawModel)(struct TrailHost *);  /* +0x0C */
    void (*draw)(struct TrailHost *);
    void (*advance)(struct TrailHost *);
    unsigned char unmodeled_18[0x8];
    TrailBlock block;
} TrailHost;


/* The ST coordinates of the four corners of a textured quad. Declared without const (the definition in m_gs.c is const):
 * a const declaration lets the compiler fold the reads and changes the code. */
extern Quadword McEfSTQtbl[4];


/* The scale of the trail model: a quarter across, half along. */
static const Vector4 mefTrailModelScale = { 0.25f, 0.25f, 0.5f, 1.0f };

void *memset(void *destination, int value, unsigned int size);
void MEfBuildTepodonArcKeys(TrailBlock *work);
void MEfFillTepodonSamples(TrailBlock *block);
void updateSmoke(TrailHost *object);
void MEfDrawTepodonModel(TrailHost *object);
void MEfDrawRotatedQuads(TrailHost *object);
void MEfAdvanceTepodon(TrailHost *host);


/*
 * Creates the Tepodon trail (fire type 5): the curve runs from the first actor to the
 * second one, whose end is pushed sideways by a random angle and a signed
 * random distance; the trail rings and the smoke are cleared, the path is
 * sampled, and both samples start at the first actor.
 */
int MEfCreateTepodon(TrailHost *object)
{
    TrailBlock *block;
    MEfSmokePathCurve *curve;
    AlignedVector4 *startControl;
    AlignedVector4 *endControl;
    AlignedVector4 *sampleDest;
    AlignedVector4 *prevSampleDest;
    AlignedVector4 cosine;
    AlignedVector4 sine;
    float pushAngle;
    float pushDistance;

    curve = &mefPathCurve.smoke;
    block = &object->block;
    startControl = &curve->startControl;
    block->ticks = 0;
    MEfGetActorCoord(startControl, block->firstActor, block->firstCoordinate);
    endControl = &curve->endControl;
    MEfGetActorCoord(endControl, block->secondActor, block->secondCoordinate);
    pushAngle = MMathMakeRandomPI();
    pushDistance = MMathMakeRandomSigned() * 2.0f;
    Vu0Cos(cosine.x, pushAngle);
    endControl->x += cosine.x * pushDistance;
    Vu0Sin(sine.x, pushAngle);
    curve->endAxes[1] += sine.x * pushDistance;
    MSeGetActorMatrix(&curve->actorMatrix, block->firstActor, block->firstCoordinate);
    memset(block->ring, 0, MEF_PATH_SAMPLES * sizeof(AlignedVector4));
    memset(&block->ring[MEF_PATH_SAMPLES], 0, MEF_PATH_SAMPLES * sizeof(AlignedVector4));
    memset(block->smokeActive, 0, sizeof(block->smokeActive));
    memset(block->smokePoints, 0, sizeof(block->smokePoints));
    memset(block->smokeAngles, 0, sizeof(block->smokeAngles));
    MEfBuildTepodonArcKeys(block);
    MEfFillTepodonSamples(block);
    sampleDest = &block->sample;
    __asm__ __volatile__("lq $8, %3\n"
                         "sq $8, 0(%2)"
                         : "=m"(*sampleDest), "=m"(sampleDest->w)
                         : "r"(sampleDest), "m"(*startControl), "m"(startControl->w)
                         : "$8", "memory");
    prevSampleDest = &block->prevSample;
    __asm__ __volatile__("lq $8, %3\n"
                         "sq $8, 0(%2)"
                         : "=m"(*prevSampleDest), "=m"(prevSampleDest->w)
                         : "r"(prevSampleDest), "m"(*startControl), "m"(startControl->w)
                         : "$8", "memory");
    MGsGPInit(&block->packet, 0, 0);
    object->step = updateSmoke;
    object->drawModel = MEfDrawTepodonModel;
    object->draw = MEfDrawRotatedQuads;
    object->advance = MEfAdvanceTepodon;
    return 1;
}

/*
 * Lays out the five keys of the arc between the two control points: the ends
 * are the controls themselves, the three inner keys sit at 3%, 15% and 40% of
 * the way, lifted by 1.6, 3.0 and 2.8.
 */
void MEfBuildTepodonArcKeys(TrailBlock *work)
{
    MEfSmokePathCurve *curve;
    Vector4 *startControl;
    Vector4 *endControl;
    Vector4 *firstKey;
    Vector4 *lastKey;

    curve = &mefPathCurve.smoke;
    MEfHermitePathInit(&curve->head, 5);
    startControl = &curve->startControl;
    firstKey = &curve->head.points[0];
    __asm__ __volatile__("lq $8, %3\n"
                         "sq $8, 0(%2)"
                         : "=m"(*firstKey), "=m"(firstKey->w)
                         : "r"(firstKey), "m"(*startControl), "m"(startControl->w)
                         : "$8", "memory");
    lastKey = &curve->head.points[4];
    endControl = &curve->endControl;
    __asm__ __volatile__("lq $8, %3\n"
                         "sq $8, 0(%2)"
                         : "=m"(*lastKey), "=m"(lastKey->w)
                         : "r"(lastKey), "m"(*endControl), "m"(endControl->w)
                         : "$8", "memory");
    MMathVectorInterpolation(&curve->head.points[1], startControl, endControl, 0.03f);
    curve->head.points[1].y += 1.6f;
    MMathVectorInterpolation(&curve->head.points[2], startControl, endControl, 0.15f);
    curve->head.points[2].y += 3.0f;
    MMathVectorInterpolation(&curve->head.points[3], startControl, endControl, 0.4f);
    curve->head.points[3].y += 2.8f;
}

/*
 * Samples the Tepodon path into 29 points (Hermite keys at 1/7 steps, the
 * last one the end of segment 3) and the 28 directions between neighbours; the
 * last direction repeats. Samples and directions fill the block's ring.
 */
void MEfFillTepodonSamples(TrailBlock *block)
{
    MEfSmokePathCurve *curve;
    Matrix4 frame;
    AlignedVector4 sample;
    AlignedVector4 *sampleSlot;
    AlignedVector4 *sampleSource;
    AlignedVector4 *finalSlot;
    AlignedVector4 *angleSlot;
    AlignedVector4 *direction;
    int index;
    int segment;
    int step;
    int perSegment;

    curve = &mefPathCurve.smoke;
    QuadwordCopyMatrixT0(&frame, &curve->actorMatrix);
    Vu0MMathSetTranslation(&frame, &MMathUnitVector);
    sampleSource = &sample;
    index = 0;
    perSegment = 7;
    for (; index < MEF_PATH_SAMPLES; index++) {
        segment = index / perSegment;
        step = index % perSegment;
        if (segment == 4) {
            segment = 3;
            step = 7;
        }
        MMathCalcHermiteKeyPrm(&curve->head, segment);
        MMathCalcHermiteKey(&sample, &curve->head, segment, (float)step * (1.0f / 7.0f));
        sampleSlot = block->ring + index;
        __asm__ __volatile__(
            "lq $8, 0(%5)\n"
            "sq $8, 0(%4)\n"
            : "=m"(block->ring[index].w), "=m"(block->ring[index].w),
              "=m"(block->ring[index].w), "=m"(block->ring[index].w)
            : "r"(sampleSlot), "r"(&sample)
            : "$8");
    }
    for (index = 0; index < 28; index++) {
        direction = &sample;
        MMathCalcAngle(direction, &block->ring[index], &block->ring[index + 1]);
        angleSlot = &block->ring[MEF_PATH_SAMPLES + index];
        __asm__ __volatile__("lq $8, 0(%3)\n"
                             "sq $8, 0(%2)"
                             : "=m"(*angleSlot), "=m"(angleSlot->w)
                             : "r"(angleSlot), "r"(direction), "m"(*direction), "m"(direction->w)
                             : "$8", "memory");
    }
    finalSlot = &block->ring[MEF_PATH_SAMPLES + 28];
    __asm__ __volatile__("lq $8, %3\n"
                         "sq $8, 0(%2)"
                         : "=m"(*finalSlot), "=m"(finalSlot->w)
                         : "r"(finalSlot), "m"(*sampleSource), "m"(sampleSource->w)
                         : "$8", "memory");
}

/*
 * Ages the nine smoke trails by one slot and, while the step is below 29,
 * emits a new trail of five points jittered along the step from the previous
 * sample to the current one, each with a random angle.
 */
void updateSmoke(TrailHost *object)
{
    TrailBlock *block;
    AlignedVector4 offset;
    Matrix4 frame;
    int tick;
    int trail;
    int point;

    block = &object->block;
    tick = (short)block->ticks;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    for (trail = 8; trail > 0; trail--) {
        block->smokeActive[trail] = block->smokeActive[trail - 1];
        for (point = 0; point < MEF_SMOKE_POINTS; point++) {
            QuadwordCopyT0(&block->smokePoints[trail][point], &block->smokePoints[trail - 1][point]);
            block->smokeAngles[trail][point] = block->smokeAngles[trail - 1][point];
        }
    }
    block->smokeActive[0] = 0;
    if (tick < MEF_PATH_SAMPLES) {
        block->smokeActive[0] = 1;
        for (point = 0; point < MEF_SMOKE_POINTS; point++) {
            MMathVectorInterpolation(&offset, &block->sample, &block->prevSample, (float)point / (float)MEF_SMOKE_POINTS);
            MMathRotateMatrixYXZ(&frame, 0, &block->ring[MEF_PATH_SAMPLES + tick]);
            __asm__ __volatile__(
                "lqc2 vf1, 0(%3)\n"
                "vmove.w vf1, vf0\n"
                "sqc2 vf1, 48(%4)\n"
                : "=m"(frame), "=m"(frame[3]), "=m"(frame[3][3])
                : "r"(&offset), "r"(frame), "m"(offset));
            __asm__ __volatile__("sqc2 vf0, 0(%0)" : : "r"(&offset) : "memory");
            offset.x = MMathMakeRandom() * 0.06f - 0.03f;
            offset.y = MMathMakeRandom() * 0.06f - 0.03f;
            MMathApplyMatrix(&block->smokePoints[0][point], &frame, &offset);
            block->smokeAngles[0][point] = MMathMakeRandom2PI();
        }
    }
}

void MEfDrawTepodonModel(TrailHost *object)
{
    TrailBlock *block;
    Matrix4 drawMatrix;
    SvTexParam texParam;
    short ticks;

    block = &object->block;
    ticks = block->ticks;
    if (ticks < MEF_PATH_SAMPLES) {
        svGetPrmFromName("tepodon.pxy", object->resourceType, &texParam);
        MMathRotateMatrixYXZ(&drawMatrix, 0, &block->ring[MEF_PATH_SAMPLES + ticks]);
        MMathScaleMatrix(&drawMatrix, &drawMatrix, &mefTrailModelScale);
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmove.w vf1w, vf0w\n\t"
            "sqc2 vf1, 48(%1)"
            : : "r" (&block->ring[ticks]), "r" (drawMatrix) : "memory");
        nmlModelSetTexture(texParam.companionData);
        nmlModelSetDrawMatrix(drawMatrix);
        pxySubmitModel(texParam.primaryData);
    }
}

/* GIF tag of the textured strip: NREG 3 (ST, RGBAQ, XYZ2), PRIM 0x54. */
#define MEF_GS_TEXTURED_STRIP_TAG 0x302A400000008000
#define MEF_GS_TEXTURED_STRIP_REGS 0x512
/* GS A+D register addresses written by the smoke setup (GS_TEST_1 and friends in include/gs.h). */
#define MEF_GS_TEX1_1 0x14
#define MEF_GS_CLAMP_1 0x08
#define MEF_GS_TEX0_1 0x06


/*
 * Builds the GS packet for the smoke: a setup block with the GS state, then one
 * textured strip of four corners for each visible point of each active trail
 * (X1 fnSMP01_DP000).
 */
void MEfDrawRotatedQuads(TrailHost *object)
{
    /* RGBAQ of each smoke trail, newest first: grey that loses a little alpha at both ends (X1 coltbl). */
    static const MEfGsColor coltbl[MEF_SMOKE_TRAILS] = {
        { { 0x80, 0x80, 0x80, 0x78 } },
        { { 0x80, 0x80, 0x80, 0x80 } },
        { { 0x80, 0x80, 0x80, 0x80 } },
        { { 0x80, 0x80, 0x80, 0x80 } },
        { { 0x80, 0x80, 0x80, 0x80 } },
        { { 0x80, 0x80, 0x80, 0x80 } },
        { { 0x80, 0x80, 0x80, 0x80 } },
        { { 0x80, 0x80, 0x80, 0x78 } },
        { { 0x80, 0x80, 0x80, 0x70 } },
    };
    /* Quad size of each smoke trail: the smoke grows as it ages and shrinks on the last step (X1 sztbl). */
    static const float sztbl[MEF_SMOKE_TRAILS] = { 0.1f, 0.13f, 0.16f, 0.18f, 0.2f, 0.22f, 0.24f, 0.26f, 0.22f };
    TrailBlock *block;
    int trail;
    long stripRegisters;
    int setupPairs;
    Vector4 corners[4];
    SvTexParam texParam;
    MGsPacket *packet;
    MGsPacket *stripPacket;
    MEfGsQword *setup;
    /* The GIF reads these register writes from the packet over DMA. */
    volatile MEfGsQword *adSlot;
    volatile MEfGsQword *tex1Slot;
    volatile MEfGsQword *clampSlot;
    volatile MEfGsQword *zbufSlot;
    volatile MEfGsQword *alphaSlot;
    Quadword rgbaq;
    int point;
    int setupQuadwords;
    int stripRegCount;
    int tex0Register;

    svGetPrmFromName("kemuri01_.bmp", 0, &texParam);
    packet = &object->block.packet;
    MGsGPInit(&object->block.packet, 0, 0);
    setup = packet->current;
    setup[0].gif.tag = MEF_GS_SETUP_TAG;
    setup[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    /* The GS state: test, texture filter, clamp, depth and additive alpha. */
    adSlot = setup + 1;
    tex1Slot = setup + 2;
    clampSlot = setup + 3;
    zbufSlot = setup + 4;
    alphaSlot = setup + 5;
    adSlot->ad.address = MEF_GS_TEST_1;
    adSlot->ad.data = MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL;
    tex1Slot->ad.address = MEF_GS_TEX1_1;
    tex1Slot->ad.data = 0x60;
    clampSlot->ad.address = MEF_GS_CLAMP_1;
    clampSlot->ad.data = 5;
    zbufSlot->ad.address = MEF_GS_ZBUF_1;
    zbufSlot->ad.data = MEF_GS_ZBUF_1_NO_DEPTH_WRITE;
    alphaSlot->ad.address = MEF_GS_ALPHA_1;
    alphaSlot->ad.data = 0x48;
    setupPairs = 5;
    setupQuadwords = packet->regCount * setupPairs + 1;
    packet->current->nloop |= setupPairs;
    packet->current += setupQuadwords;
    packet->count += setupQuadwords;
    /* Emit the textured strip of each active smoke trail. */
    block = &object->block;
    for (trail = 0; trail < MEF_SMOKE_TRAILS; trail++) {
        if (block->smokeActive[trail] != 0) {
            rgbaq = coltbl[trail].whole;
            for (point = 0; point < MEF_SMOKE_POINTS; point++) {
                if (MEfMake4Vertex2(corners, &block->smokePoints[trail][point], sztbl[trail], block->smokeAngles[trail][point]) == 0) {
                    MEfGsQword *tag;
                    MEfGsQword *strip;
                    u64 tex0Value;
                    int tagQuadwords;
                    int stripQuadwords;

                    stripPacket = &block->packet;
                    tex0Register = MEF_GS_TEX0_1;
                    stripRegisters = MEF_GS_TEXTURED_STRIP_REGS;
                    stripRegCount = 3;
                    tag = stripPacket->current;
                    tag[0].gif.tag = MEF_GS_SETUP_TAG;
                    tag[0].gif.registers = MEF_GS_SETUP_REGS;
                    stripPacket->regCount = 1;
                    adSlot = tag + 1;
                    tex0Value = texParam.tex0;
                    adSlot->ad.address = tex0Register;
                    adSlot->ad.data = tex0Value;
                    tagQuadwords = stripPacket->regCount + 1;
                    stripPacket->current->nloop |= 1;
                    stripPacket->current += tagQuadwords;
                    stripPacket->count += tagQuadwords;
                    /* Strip tag, then four ST, colour and vertex quadwords. */
                    strip = stripPacket->current;
                    strip[0].gif.tag = MEF_GS_TEXTURED_STRIP_TAG;
                    strip[0].gif.registers = stripRegisters;
                    stripPacket->regCount = stripRegCount;
                    (++strip)->whole = McEfSTQtbl[0];
                    (++strip)->whole = rgbaq;
                    (++strip)->whole = *(Quadword *) &corners[0];
                    (++strip)->whole = McEfSTQtbl[1];
                    (++strip)->whole = rgbaq;
                    (++strip)->whole = *(Quadword *) &corners[1];
                    (++strip)->whole = McEfSTQtbl[2];
                    (++strip)->whole = rgbaq;
                    (++strip)->whole = *(Quadword *) &corners[2];
                    (++strip)->whole = McEfSTQtbl[3];
                    (++strip)->whole = rgbaq;
                    (++strip)->whole = *(Quadword *) &corners[3];
                    stripQuadwords = stripPacket->regCount * 4 + 1;
                    stripPacket->current->nloop |= 4;
                    stripPacket->current += stripQuadwords;
                    stripPacket->count += stripQuadwords;
                }
            }
        }
    }
    MGsGPTerm(&block->packet);
}

void MEfAdvanceTepodon(TrailHost *host)
{
    TrailBlock *block;
    int ticks;

    block = &host->block;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    ticks = block->ticks + 1;
    block->ticks = ticks;
    if ((short)ticks < MEF_PATH_SAMPLES) {
        __asm__ __volatile__(
            "lq $8, 0(%4)\n"
            "sq $8, 0(%3)\n"
            : "=m" (block->prevSample), "=m" (block->prevSample), "=m" (block->prevSample)
            : "r" (&block->prevSample), "r" (&block->sample)
            : "$8");
        /* ring[ticks] as address arithmetic: the addu operand order is part of the matched code. */
        QuadwordCopyT0(&block->sample, (AlignedVector4 *)((short)ticks * 16 + (int)block) + 8);
    }
    if ((short)ticks >= MEF_TRAIL_LIFETIME_TICKS) {
        MSeObjRequestFree(host);
    }
}
