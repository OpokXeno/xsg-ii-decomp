/* The 31-sample path effect, from MEfCreateThunder31. */

#include "common.h"

#include "shared.h"
#include "main/m_math.h"
#include "ov01/m_ef.h"
#include "ov01/m_ef_thunder25.h"
#include "ov01/m_ef_path.h"

#include "quadword.h"

#include "ov01/m_gs.h"

#include "vu0_macro.h"

#include "main/m_se_obj.h"
#include "main/sm_initilize.h"

/* GS register addresses and values of the effect packets. */
#define MEF_GS_TEXTURED_STRIP_TAG 0x302A400000008000
#define MEF_GS_TEXTURED_STRIP_REGS 0x512
#define MEF_GS_TEX0_1 0x06
#define MEF_GS_CLAMP_1 0x08
#define MEF_GS_TEX1_1 0x14
/* Samples of the 31-sample path; the tick count at which its effect ends. */
#define MEF_PATH_SAMPLES 31
#define MEF_PATH_LIFETIME_TICKS 0x22

/*
 * The path block at object+0x20: a 16-bit tick count at +0x54. The tick
 * callback at object+0x14 advances it and ends the effect at its threshold.
 * The effect keeps its actor points, the path end, its matrix and its 31
 * samples here.
 */
typedef struct MEfThunder31Block {
    unsigned char unmodeled_00[0x0C];
    struct XenoActor *firstActor;               /* +0x0C */
    unsigned int firstCoordinate;           /* +0x10 */
    unsigned char unmodeled_14[0x1C];
    struct XenoActor *secondActor;              /* +0x30 */
    unsigned int secondCoordinate;          /* +0x34 */
    unsigned char unmodeled_38[0x08];
    MGsPacket gsPacket;                     /* +0x40: the GS packet of MGsGPInit */
    unsigned short ticks;                   /* +0x54 */
    unsigned char unmodeled_56[0x0A];
    Vector4 firstPoint;                     /* +0x60 */
    Vector4 secondPoint;                    /* +0x70 */
    Vector4 pathEnd;                        /* +0x80: the last path point */
    Matrix4 matrix;                         /* +0x90 */
    AlignedVector4 samples[31];             /* +0xD0 */
} MEfThunder31Block;

typedef struct MEfThunder31Effect {
    unsigned char unmodeled_00[0x04];
    int paramName;
    unsigned char unmodeled_08[0x04];
    void (*drawCallback)(struct MEfThunder31Effect *);     /* +0x0C */
    void (*updateCallback)(struct MEfThunder31Effect *);   /* +0x10 */
    void (*tickCallback)(struct MEfThunder31Effect *);     /* +0x14 */
    unsigned char unmodeled_18[0x08];
    MEfThunder31Block block;
} MEfThunder31Effect;

/* The ST coordinates of the four corners of a textured quad. Declared without const (the definition in m_gs.c is const):
 * a const declaration lets the compiler fold the reads and changes the code. */
extern Quadword McEfSTQtbl[4];

extern unsigned char mefThunderModelName[];
extern unsigned char mefThunderTexName[];


void *memset(void *destination, int value, unsigned int size);
void MEfMakeRotatedPath(MEfThunder31Block *effect);
void MEfFillThunderSamples31(MEfThunder31Block *block);
void MEfDrawThunderSparks31(MEfThunder31Effect *object);
void MEfDrawThunderQuads31(MEfThunder31Effect *object);
void MEfAdvanceThunder31(MEfThunder31Effect *object);

/*
 * MEfCreateThunder31: start the 31-sample path effect in the object's block.
 * The two points and the matrix come from the actors, the samples are filled
 * from the path curve, and the three callbacks are installed on the object.
 */
int MEfCreateThunder31(MEfThunder31Effect *object)
{
    MEfThunder31Block *block;

    block = &object->block;
    block->ticks = 0;
    MEfGetActorCoord(&block->firstPoint, block->firstActor, block->firstCoordinate);
    MEfGetActorCoord(&block->secondPoint, block->secondActor, block->secondCoordinate);
    MSeGetActorMatrix(&block->matrix, block->firstActor, block->firstCoordinate);
    memset(block->samples, 0, sizeof(block->samples));
    MEfMakeRotatedPath(block);
    MEfFillThunderSamples31(block);
    MGsGPInit(&block->gsPacket, 0, 0);
    object->drawCallback = MEfDrawThunderSparks31;
    object->updateCallback = MEfDrawThunderQuads31;
    object->tickCallback = MEfAdvanceThunder31;
    return 1;
}

/*
 * MEfMakeRotatedPath: build the four path-curve points of the effect. The
 * curve starts at the first point, ends at a point beyond the second one
 * (1.4 times their distance along the line between them), pushed sideways
 * by a random angle and spread; the middle points sit a third of the way
 * along, oriented by the actor matrix and by the mean of the two angles.
 */
void MEfMakeRotatedPath(MEfThunder31Block *block)
{
    MEfPathCurve *curve;
    Vector4 *first;
    Vector4 *point;
    Vector4 *mid;
    Vector4 *start;
    Vector4 *dest;
    Vector4 offset;
    Vector4 tip;
    Vector4 angles;
    Vector4 endAngles;
    Matrix4 rotation;
    Matrix4 *matrix;
    Vector4 cosine;
    Vector4 sine;
    float value;
    float spread;
    float third;

    curve = &mefPathCurve.curve;
    first = &block->firstPoint;
    MMathCalcAngle(&angles, first, &block->secondPoint);
    matrix = &rotation;
    MMathRotateMatrixYX(matrix, 0, &angles);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%3)\n"
        : "=m"(*matrix), "=m"((*matrix)[3])
        : "r"(first), "r"(matrix), "m"(*first), "m"(first->w)
        : "memory");
    value = MMathCalcDist(first, &block->secondPoint);
    Vu0SetOrigin(&offset);
    offset.z = value * 1.4f;
    point = &tip;
    MMathApplyMatrix(point, matrix, &offset);
    MEfHermitePathInit(curve, 4);
    start = &curve->points[0];
    __asm__ __volatile__("lq $8, 0(%3)\n"
        "sq $8, 0(%2)"
        : "=m"(*start), "=m"(start->w)
        : "r"(start), "r"(first)
        : "$8", "memory");
    Vu0SetOrigin(&offset);
    value = MMathMakeRandom2PI();
    spread = MMathMakeRandom() * 1.5f + 0.4f;
    Vu0Cos(cosine.x, value);
    offset.x = cosine.x * spread;
    Vu0Sin(sine.x, value);
    offset.y = sine.x * spread;
    __asm__ __volatile__(
        "lqc2 vf1, 0(%3)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%4)\n"
        : "=m"(*matrix), "=m"((*matrix)[3]), "=m"((*matrix)[3][3])
        : "r"(point), "r"(matrix), "m"(tip)
        : "memory");
    MMathApplyMatrix(&curve->points[3], matrix, &offset);
    value = MMathCalcDist(&curve->points[0], &curve->points[3]);
    Vu0SetOrigin(&offset);
    third = value * 0.333333f;
    offset.z = third;
    mid = &curve->points[1];
    MMathApplyMatrix(mid, &block->matrix, &offset);
    MMathCalcAngle(&angles, &curve->points[0], mid);
    MMathCalcAngle(&endAngles, mid, &curve->points[3]);
    MMathVectorInterpolation(&angles, &angles, &endAngles, 0.5f);
    MMathRotateMatrixYX(matrix, 0, &angles);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%3)\n"
        : "=m"(*matrix), "=m"((*matrix)[3])
        : "r"(mid), "r"(matrix), "m"(*mid), "m"(mid->w)
        : "memory");
    Vu0SetOrigin(&offset);
    offset.z = third;
    MMathApplyMatrix(&curve->points[2], matrix, &offset);
    dest = &block->pathEnd;
    __asm__ __volatile__("lq $8, 0(%3)\n"
        "sq $8, 0(%2)"
        : "=m"(*dest), "=m"(dest->w)
        : "r"(dest), "r"(&curve->points[3])
        : "$8", "memory");
}

void MEfFillThunderSamples31(MEfThunder31Block *block)
{
    MEfPathCurve *curve;
    Matrix4 matrix;
    AlignedVector4 sample;
    AlignedVector4 *store;
    int index;
    int segment;
    int step;
    int perSegment;

    curve = &mefPathCurve.curve;
    QuadwordCopyMatrixT0(&matrix, &block->matrix);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        :
        : "r"(&MMathUnitVector), "r"(&matrix)
        : "memory");
    index = 0;
    perSegment = 10;
    for (; index < MEF_PATH_SAMPLES; index++) {
        segment = index / perSegment;
        step = index % perSegment;
        if (segment == 3) {
            segment = 2;
            step = 10;
        }
        MMathCalcHermiteKeyPrm(curve, segment);
        MMathCalcHermiteKey(&sample, curve, segment, (float)step * 0.1f);
        store = block->samples + index;
        __asm__ __volatile__(
            "lq $8, 0(%5)\n"
            "sq $8, 0(%4)\n"
            : "=m"(block->samples[index].w), "=m"(block->samples[index].w),
              "=m"(block->samples[index].w), "=m"(block->samples[index].w)
            : "r"(store), "r"(&sample)
            : "$8");
    }
}

/*
 * Appends one textured quad around a point to the packet: a randomly turned
 * square of the given size, an A+D tag that selects the texture, then a strip
 * tag with the four corners (ST, a fixed half-alpha grey, XYZ). Returns 0 when
 * the quad is culled.
 */
int MEfAppendThunderQuad31(MGsPacket *packet, u64 *texture, Vector4 *point, float size)
{
    /* RGBAQ of the quad: grey at half alpha. */
    static const Quadword quadColor = 0x00000040000000800000008000000080;
    Vector4 verts[4];
    Quadword color;
    MEfGsQword *setup;
    MEfGsQword *texSetting;
    MEfGsQword *strip;
    u64 texValue;
    int culled;
    int setupCount;
    int stripCount;

    culled = MEfMake4Vertex2(verts, point, size, MMathMakeRandom2PI());
    color = quadColor;
    if (culled != 0) {
        return 0;
    }
    setup = packet->current;
    setup[0].gif.tag = MEF_GS_SETUP_TAG;
    setup[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    texSetting = setup + 1;
    texValue = *texture;
    texSetting->ad.address = MEF_GS_TEX0_1;
    texSetting->ad.data = texValue;
    setupCount = packet->regCount + 1;
    packet->current->nloop |= 1;
    packet->current = packet->current + setupCount;
    packet->count += setupCount;
    strip = packet->current;
    strip[0].gif.tag = MEF_GS_TEXTURED_STRIP_TAG;
    strip[0].gif.registers = MEF_GS_TEXTURED_STRIP_REGS;
    packet->regCount = 3;
    strip[1].whole = McEfSTQtbl[0];
    strip[2].whole = color;
    strip[3].whole = *(Quadword *)&verts[0];
    strip[4].whole = McEfSTQtbl[1];
    strip[5].whole = color;
    strip[6].whole = *(Quadword *)&verts[1];
    strip[7].whole = McEfSTQtbl[2];
    strip[8].whole = color;
    strip[9].whole = *(Quadword *)&verts[2];
    strip[10].whole = McEfSTQtbl[3];
    strip[11].whole = color;
    strip[12].whole = *(Quadword *)&verts[3];
    stripCount = packet->regCount * 4 + 1;
    packet->current->nloop |= 4;
    packet->current = packet->current + stripCount;
    packet->count += stripCount;
    return 1;
}

extern long long mefSparkStencilAlpha31;
extern float mefSampleJitter31[4];
extern float mefQuadSize31[4];


/*
 * Draw callback of the 31-sample path effect: while the head sample exists,
 * draws six randomly rotated and scaled spark models at it.
 */
void MEfDrawThunderSparks31(MEfThunder31Effect *object)
{
    MEfThunder31Block *block;
    Vector4 angles;
    Matrix4 rotation;
    SvTexParam param;
    int i;

    block = &object->block;
    if ((short) block->ticks < MEF_PATH_SAMPLES) {
        svGetPrmFromName("tex_thun01.pxy", object->paramName, &param);
        for (i = 0; i < 6; i++) {
            MEfMakeRandomVector(&angles);
            MMathRotateMatrixXYZ(&rotation, 0, &angles);
            MMathScaleMatrixRotS(&rotation, &rotation, MMathMakeRandom() * 0.4f + 0.4f);
            __asm__ __volatile__(
                "lqc2 vf1, 0(%0)\n"
                "vmove.w vf1, vf0\n"
                "sqc2 vf1, 48(%1)\n"
                :
                : "r"((AlignedVector4 *) ((short) block->ticks * 16 + (int) block + 0xD0)), "r"(rotation)
                : "memory");
            nmlModelSetTexture(param.companionData);
            nmlModelSetDrawMatrix(rotation);
            nmlModelSetStencilAlpha(mefSparkStencilAlpha31);
            nmlModelSetStencil(1);
            pxySubmitModel(param.primaryData);
        }
    }
}

/*
 * Update callback of the 31-sample path effect: opens the packet with the GS
 * state for additive textured sprites (TEST, TEX1, CLAMP, ZBUF, ALPHA), then
 * for each of the four trail segments still within the samples draws two
 * sprites at the sample, jittered by the segment's spread and sized by its
 * size, then the head sprite at the path end, and closes the packet.
 */
void MEfDrawThunderQuads31(MEfThunder31Effect *object)
{
    Vector4 point;
    SvTexParam param;
    MEfThunder31Block *block;
    MGsPacket *packet;
    MEfGsQword *setup;
    /* The GIF reads these register writes from the packet over DMA. */
    volatile MEfGsQword *testSetting;
    volatile MEfGsQword *filterSetting;
    volatile MEfGsQword *clampSetting;
    volatile MEfGsQword *zbufferSetting;
    volatile MEfGsQword *alphaSetting;
    int registerCount;
    int count;
    int segment;
    int sample;
    int quad;
    AlignedVector4 *source;

    block = &object->block;
    packet = &block->gsPacket;
    MGsGPInit(packet, 0, 0);
    setup = packet->current;
    setup[0].gif.tag = MEF_GS_SETUP_TAG;
    setup[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    testSetting = setup + 1;
    filterSetting = setup + 2;
    clampSetting = setup + 3;
    zbufferSetting = setup + 4;
    alphaSetting = setup + 5;
    testSetting->ad.address = MEF_GS_TEST_1;
    testSetting->ad.data = MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL;
    filterSetting->ad.address = MEF_GS_TEX1_1;
    filterSetting->ad.data = 0x60;
    clampSetting->ad.address = MEF_GS_CLAMP_1;
    clampSetting->ad.data = 5;
    zbufferSetting->ad.address = MEF_GS_ZBUF_1;
    zbufferSetting->ad.data = MEF_GS_ZBUF_1_NO_DEPTH_WRITE;
    alphaSetting->ad.address = MEF_GS_ALPHA_1;
    alphaSetting->ad.data = MEF_GS_ALPHA_1_ADDITIVE_FIX;
    registerCount = 5;
    count = packet->regCount * registerCount + 1;
    packet->current->nloop |= registerCount;
    packet->current = packet->current + count;
    packet->count += count;
    svGetPrmFromName("sh_hikari.bmp", object->paramName, &param);
    for (segment = 0; segment < 4; segment++) {
        sample = (short) block->ticks - segment;
        if ((unsigned int) sample < MEF_PATH_SAMPLES) {
            /* samples[index] as int address arithmetic: the operand order of its addu is part of the matched code. */
            source = (AlignedVector4 *) (sample * 16 + (int) block + 0xD0);
            for (quad = 0; quad < 2; quad++) {
                __asm__ __volatile__("lq $8, 0(%3)\n"
                    "sq $8, 0(%2)"
                    : "=m"(point), "=m"(point.w)
                    : "r"(&point), "r"(source)
                    : "$8", "memory");
                point.x += MMathMakeRandomSigned() * mefSampleJitter31[segment];
                point.y += MMathMakeRandomSigned() * mefSampleJitter31[segment];
                point.z += MMathMakeRandomSigned() * mefSampleJitter31[segment];
                MEfAppendThunderQuad31(&block->gsPacket, &param.tex0, &point, mefQuadSize31[segment]);
            }
        }
    }
    MEfDrawPathEndMarker(&block->gsPacket, &block->pathEnd, 0.2f);
    MGsGPTerm(&block->gsPacket);
}

void MEfAdvanceThunder31(MEfThunder31Effect *object)
{
    MEfThunder31Block *block;
    int ticks;

    block = &object->block;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    ticks = block->ticks + 1;
    block->ticks = ticks;
    if ((short) ticks >= MEF_PATH_LIFETIME_TICKS) {
        MSeObjRequestFree(object);
    }
}

/* Path-effect tables of the 31-sample effect: the spark stencil alpha, the sample jitter and the quad sizes. */
long long mefSparkStencilAlpha31 = 0x48;

float mefSampleJitter31[4] = { 0.05f, 0.04f, 0.03f, 0.02f };

float mefQuadSize31[4] = { 1.0f, 0.55f, 0.35f, 0.2f };
