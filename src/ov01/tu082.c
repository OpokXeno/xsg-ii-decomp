/*
 * OV01 original TU 82: the 31-sample path effect, from MEfCreatePathEffect31.
 */

#include "common.h"

#include "shared.h"
#include "main/m_math_195.h"
#include "main/m_math_191.h"
#include "ov01/tu077.h"
#include "main/tu187.h"
#include "main/tu192.h"
#include "main/tu194.h"

#include "quadword.h"

#include "ov01/tu075.h"

#include "vu0_macro.h"

#include "main/tu197.h"
#include "main/tu190.h"
#include "main/m_math_193.h"

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
typedef struct MEfPathBlock {
    unsigned char unmodeled_00[0x0C];
    struct Actor *firstActor;               /* +0x0C */
    unsigned int firstCoordinate;           /* +0x10 */
    unsigned char unmodeled_14[0x1C];
    struct Actor *secondActor;              /* +0x30 */
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
} MEfPathBlock;

typedef struct MEfPathEffect {
    unsigned char unmodeled_00[0x04];
    int paramName;
    unsigned char unmodeled_08[0x04];
    void (*drawCallback)(struct MEfPathEffect *);     /* +0x0C */
    void (*updateCallback)(struct MEfPathEffect *);   /* +0x10 */
    void (*tickCallback)(struct MEfPathEffect *);     /* +0x14 */
    unsigned char unmodeled_18[0x08];
    MEfPathBlock block;
} MEfPathEffect;

/*
 * The path curve (mefPathCurve): ov01RecordInit clears its first 0xB0 bytes.
 * The five trail points are 16-byte vectors from +0x10; the two control points
 * follow at +0xB0 and +0xC0.
 */
typedef struct PathCurve {
    int pointCount;
    unsigned char unmodeled_04[0x0C];
    Vector4 trail[5];
    unsigned char unmodeled_60[0x50];
    Vector4 startControl;
    Vector4 endControl;
} PathCurve;

/* The ST coordinates of the four corners of a textured quad. */
extern Quadword McEfSTQtbl[4];

/* ov01RecordInit: the definition is tu076.c; this file keeps its own view (void * record). */
void ov01RecordInit(void *record, int value);
/* The texture parameters svGetPrmFromName fills (the same layout as in sv_get_image_item.c). */
typedef struct SvTexParam {
    u64 tex0;
    float scaledWidth;
    float scaledHeight;
    const void *primaryData;     /* +0x10: the model pxySubmitModel draws */
    const void *companionData;   /* +0x14: the texture block pxySetTxyBlock binds */
} SvTexParam;
SvTexParam *svGetPrmFromName(const char *resourceName, int type, SvTexParam *parameter);
void pxySetTxyBlock(const void *block);
int pxySubmitModel(const void *header);
void nmlModelSetDrawMatrix(Matrix4 matrix);
void nmlModelSetStencilAlpha(long long value);
void nmlModelSetStencil(int enabled);
extern unsigned char mefThunderModelName[];
extern unsigned char mefThunderTexName[];
extern long long mefSparkStencilAlpha25;

short ov01LastValueGet(void);

extern PathCurve mefPathCurve;
extern Vector4 MMathUnitVector;
/* MMathCalcHermiteKey(Prm): the tu076.c definitions take other argument types, so this file keeps its own view. */
void MMathCalcHermiteKeyPrm(PathCurve *curve, int index);
void MMathCalcHermiteKey(Vector4 *destination, PathCurve *curve, int index, float parameter);
Vector4 *MEfGetActorCoord(Vector4 *destination, struct Actor *actor, unsigned int coordinate);
float MMathMakeRandom2PI(void);
void *memset(void *destination, int value, unsigned int size);
void MEfMakeRotatedPath(MEfPathBlock *effect);
void MEfFillHermiteSamples31(MEfPathBlock *block);
void MEfDrawSparkModels31(MEfPathEffect *object);
void MEfDrawQuadPacket31(MEfPathEffect *object);
void MEfTickLifetime34(MEfPathEffect *object);

/*
 * MEfCreatePathEffect31: start the 31-sample path effect in the object's block.
 * The two points and the matrix come from the actors, the samples are filled
 * from the path curve, and the three callbacks are installed on the object.
 */
int MEfCreatePathEffect31(MEfPathEffect *object)
{
    MEfPathBlock *block;

    block = &object->block;
    block->ticks = 0;
    MEfGetActorCoord(&block->firstPoint, block->firstActor, block->firstCoordinate);
    MEfGetActorCoord(&block->secondPoint, block->secondActor, block->secondCoordinate);
    MSeGetActorMatrix(&block->matrix, block->firstActor, block->firstCoordinate);
    memset(block->samples, 0, sizeof(block->samples));
    MEfMakeRotatedPath(block);
    MEfFillHermiteSamples31(block);
    MGsGPInit(&block->gsPacket, 0, 0);
    object->drawCallback = MEfDrawSparkModels31;
    object->updateCallback = MEfDrawQuadPacket31;
    object->tickCallback = MEfTickLifetime34;
    return 1;
}

/*
 * MEfMakeRotatedPath: build the four path-curve points of the effect. The
 * curve starts at the first point, ends at a point beyond the second one
 * (1.4 times their distance along the line between them), pushed sideways
 * by a random angle and spread; the middle points sit a third of the way
 * along, oriented by the actor matrix and by the mean of the two angles.
 */
void MEfMakeRotatedPath(MEfPathBlock *block)
{
    PathCurve *curve;
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

    curve = &mefPathCurve;
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
    ov01RecordInit(curve, 4);
    start = &curve->trail[0];
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
    MMathApplyMatrix(&curve->trail[3], matrix, &offset);
    value = MMathCalcDist(&curve->trail[0], &curve->trail[3]);
    Vu0SetOrigin(&offset);
    third = value * 0.333333f;
    offset.z = third;
    mid = &curve->trail[1];
    MMathApplyMatrix(mid, &block->matrix, &offset);
    MMathCalcAngle(&angles, &curve->trail[0], mid);
    MMathCalcAngle(&endAngles, mid, &curve->trail[3]);
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
    MMathApplyMatrix(&curve->trail[2], matrix, &offset);
    dest = &block->pathEnd;
    __asm__ __volatile__("lq $8, 0(%3)\n"
        "sq $8, 0(%2)"
        : "=m"(*dest), "=m"(dest->w)
        : "r"(dest), "r"(&curve->trail[3])
        : "$8", "memory");
}

void MEfFillHermiteSamples31(MEfPathBlock *block)
{
    PathCurve *curve;
    Matrix4 matrix;
    AlignedVector4 sample;
    AlignedVector4 *store;
    int index;
    int segment;
    int step;
    int perSegment;

    curve = &mefPathCurve;
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
int MEfAppendTexturedQuad31(MGsPacket *packet, u64 *texture, Vector4 *point, float size)
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
float MMathMakeRandomSigned(void);
void MEfDrawPathEndMarker(MGsPacket *packet, Vector4 *point, float size);
extern float mefSampleJitter31[4];
extern float mefQuadSize31[4];


/*
 * Draw callback of the 31-sample path effect: while the head sample exists,
 * draws six randomly rotated and scaled spark models at it.
 */
void MEfDrawSparkModels31(MEfPathEffect *object)
{
    MEfPathBlock *block;
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
            pxySetTxyBlock(param.companionData);
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
void MEfDrawQuadPacket31(MEfPathEffect *object)
{
    Vector4 point;
    SvTexParam param;
    MEfPathBlock *block;
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
                MEfAppendTexturedQuad31(&block->gsPacket, &param.tex0, &point, mefQuadSize31[segment]);
            }
        }
    }
    MEfDrawPathEndMarker(&block->gsPacket, &block->pathEnd, 0.2f);
    MGsGPTerm(&block->gsPacket);
}

void MEfTickLifetime34(MEfPathEffect *object)
{
    MEfPathBlock *block;
    int ticks;

    block = &object->block;
    if (ov01LastValueGet() != 0) {
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
