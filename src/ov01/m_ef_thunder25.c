/* The path effect functions, from MEfCreateThunder25. */

#include "common.h"

#include "shared.h"
#include "main/m_math.h"
#include "ov01/m_ef.h"
#include "ov01/m_ef_thunder25.h"

#include "quadword.h"

#include "ov01/m_gs.h"

#include "vu0_macro.h"

#include "main/m_se_obj.h"
#include "main/sm_initilize.h"
#include "main/sef.h"

/* GS register addresses and values of the effect packets. */
#define MEF_GS_TEXTURED_STRIP_TAG 0x302A400000008000
#define MEF_GS_TEXTURED_STRIP_REGS 0x512
#define MEF_GS_TEX0_1 0x06
#define MEF_GS_CLAMP_1 0x08
#define MEF_GS_TEX1_1 0x14
/* Samples of the 25-sample path; the tick count at which its effect ends. */
#define MEF_PATH_SAMPLES 25
#define MEF_PATH_LIFETIME_TICKS 0x1C

/* The path block at object+0x20: a transform matrix at +0x80 and the 25
 * Hermite samples at +0xC0 (cleared as 400 bytes by the caller). */
typedef struct MEfThunder25Block {
    unsigned char unmodeled_00[0x0C];
    struct XenoActor *firstActor;        /* +0x0C: MEfGetActorCoord / MSeGetActorMatrix */
    u32 firstCoordinate;         /* +0x10 */
    unsigned char unmodeled_14[0x30 - 0x14];
    struct XenoActor *secondActor;        /* +0x30 */
    u32 secondCoordinate;         /* +0x34 */
    unsigned char unmodeled_38[0x40 - 0x38];
    MGsPacket gsPacket;          /* +0x40: the GS packet MGsGPInit prepares */
    u16 ticks;                   /* +0x54: frames since creation */
    unsigned char unmodeled_56[0x60 - 0x56];
    Vector4 firstPoint;
    Vector4 secondPoint;
    Matrix4 matrix;
    Vector4 samples[25];
} MEfThunder25Block;

/* The pooled path effect: three callbacks in its prefix, the block at +0x20. */
typedef struct MEfThunder25Effect {
    unsigned char unmodeled_00[0x04];
    int paramName;               /* +0x04: the svGetPrmFromName type argument */
    unsigned char unmodeled_08[0x04];
    void (*drawCallback)(struct MEfThunder25Effect *);    /* +0x0C: the spark models */
    void (*updateCallback)(struct MEfThunder25Effect *);  /* +0x10: the GS packet */
    void (*tickCallback)(struct MEfThunder25Effect *);    /* +0x14 */
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfThunder25Block block;          /* +0x20 */
} MEfThunder25Effect;

/* The ST coordinates of the four corners of a textured quad. Declared without const (the definition in m_gs.c is const):
 * a const declaration lets the compiler fold the reads and changes the code. */
extern Quadword McEfSTQtbl[4];

int MEfAppendThunderQuad25(MGsPacket *packet, u64 *texture, Vector4 *point, float size);

extern float mefSampleJitter25[];
extern float mefQuadSize25[];

void *memset(void *destination, int value, unsigned int count);
void MEfBuildThunder25Record(MEfThunder25Block *block);
void MEfFillThunderSamples25(MEfThunder25Block *block);
void MEfDrawThunderSparks25(MEfThunder25Effect *object);
void MEfDrawThunderQuads25(MEfThunder25Effect *object);
void MEfAdvanceThunder25(MEfThunder25Effect *object);

int MEfCreateThunder25(MEfThunder25Effect *object)
{
    MEfThunder25Block *block;

    block = &object->block;
    block->ticks = 0;
    MEfGetActorCoord(&block->firstPoint, block->firstActor, block->firstCoordinate);
    MEfGetActorCoord(&block->secondPoint, block->secondActor, block->secondCoordinate);
    sefActorMulLocalVector(block->secondActor, block->secondCoordinate, &block->secondPoint, 1);
    MSeGetActorMatrix(&block->matrix, block->firstActor, block->firstCoordinate);
    memset(block->samples, 0, sizeof(block->samples));
    MEfBuildThunder25Record(block);
    MEfFillThunderSamples25(block);
    MGsGPInit(&block->gsPacket, 0, 0);
    object->drawCallback = MEfDrawThunderSparks25;
    object->updateCallback = MEfDrawThunderQuads25;
    object->tickCallback = MEfAdvanceThunder25;
    return 1;
}

void MEfBuildThunder25Record(MEfThunder25Block *block)
{
    MEfPathCurve *curve;
    AlignedVector4 offset;
    AlignedVector4 angles;
    AlignedVector4 endAngles;
    Matrix4 rotation;

    curve = &mefPathCurve.curve;
    MEfHermitePathInit(curve, 4);
    __asm__ __volatile__(
        "lq $8, 0(%2)\n"
        "sq $8, 0(%1)"
        : "=m"(curve->points[0])
        : "r"(&curve->points[0]), "r"(&block->firstPoint)
        : "$8");
    __asm__ __volatile__(
        "lq $8, 0(%3)\n"
        "sq $8, 0(%2)"
        : "=m"(curve->points[3]), "=m"(curve->points[3].w)
        : "r"(&curve->points[3]), "r"(&block->secondPoint)
        : "$8");
    Vu0SetOrigin(&offset);
    offset.z = MMathCalcDist(&block->firstPoint, &block->secondPoint) * 0.333333f;
    MMathApplyMatrix(&curve->points[1], &block->matrix, &offset);
    MMathCalcAngle(&angles, &curve->points[0], &curve->points[1]);
    MMathCalcAngle(&endAngles, &curve->points[1], &curve->points[3]);
    MMathVectorInterpolation(&angles, &angles, &endAngles, 0.5f);
    MMathRotateMatrixYX(&rotation, 0, &angles);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        : "=m"(rotation[3])
        : "r"(&rotation), "r"(&curve->points[1]), "m"(curve->points[1])
        : "memory");
    Vu0SetOrigin(&offset);
    offset.z = MMathCalcDist(&curve->points[1], &curve->points[3]) * 0.5f;
    MMathApplyMatrix(&curve->points[2], &rotation, &offset);
}

void MEfFillThunderSamples25(MEfThunder25Block *block)
{
    MEfPathCurve *curve;
    Matrix4 matrix;
    AlignedVector4 sample;
    AlignedVector4 *store;
    int index;
    int segment;
    int step;

    curve = &mefPathCurve.curve;
    QuadwordCopyMatrixT0(&matrix, &block->matrix);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        :
        : "r"(&MMathUnitVector), "r"(&matrix)
        : "memory");
    for (index = 0; index < 25; index++) {
        segment = index / 8;
        step = index % 8;
        if (segment == 3) {
            segment = 2;
            step = 8;
        }
        MMathCalcHermiteKeyPrm(curve, segment);
        MMathCalcHermiteKey(&sample, curve, segment, (float)step * 0.125f);
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
int MEfAppendThunderQuad25(MGsPacket *packet, u64 *texture, Vector4 *point, float size)
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

void MEfDrawThunderSparks25(MEfThunder25Effect *object)
{
    MEfThunder25Block *block;
    Vector4 angles;
    Matrix4 rotation;
    SvTexParam param;
    int i;
    int j;

    block = &object->block;
    if ((short) block->ticks < MEF_PATH_SAMPLES) {
        svGetPrmFromName("de_thun01.pxy", 0, &param);
        for (i = 0; i < 6; i++) {
            MEfMakeRandomVector(&angles);
            MMathRotateMatrixXYZ(&rotation, 0, &angles);
            MMathScaleMatrixRotS(&rotation, &rotation, MMathMakeRandom() * 0.3f + 0.5f);
            __asm__ __volatile__(
                "lqc2 $vf1,0(%1)\n\t"
                :
                : "r"(rotation), "r"((Vector4 *) ((short) block->ticks * 16 + (int) block + 192))
                : "memory"
            );
            __asm__ __volatile__(
                "vmove.w $vf1w,$vf0w\n\t"
                "sqc2 $vf1,48(%0)\n\t"
                :
                : "r"(rotation)
                : "memory"
            );
            nmlModelSetTexture(param.companionData);
            nmlModelSetDrawMatrix(rotation);
            nmlModelSetStencilAlpha(mefSparkStencilAlpha25);
            nmlModelSetStencil(1);
            pxySubmitModel(param.primaryData);
        }
        svGetPrmFromName("tex_thun01.pxy", object->paramName, &param);
        for (j = 0; j < 4; j++) {
            MEfMakeRandomVector(&angles);
            MMathRotateMatrixXYZ(&rotation, 0, &angles);
            MMathScaleMatrixRotS(&rotation, &rotation, MMathMakeRandom() * 0.3f + 0.4f);
            __asm__ __volatile__(
                "lqc2 $vf1,0(%1)\n\t"
                :
                : "r"(rotation), "r"((Vector4 *) ((short) block->ticks * 16 + (int) block + 192))
                : "memory"
            );
            __asm__ __volatile__(
                "vmove.w $vf1w,$vf0w\n\t"
                "sqc2 $vf1,48(%0)\n\t"
                :
                : "r"(rotation)
                : "memory"
            );
            nmlModelSetTexture(param.companionData);
            nmlModelSetDrawMatrix(rotation);
            nmlModelSetStencilAlpha(mefSparkStencilAlpha25);
            nmlModelSetStencil(1);
            pxySubmitModel(param.primaryData);
        }
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

/*
 * Draws the path effect's samples as textured quads: the packet opens with
 * five A+D settings, then each in-range sample of the last four steps adds
 * two jittered quads (MEfAppendThunderQuad25) before the packet is flushed.
 */
void MEfDrawThunderQuads25(MEfThunder25Effect *object)
{
    MEfThunder25Block *block;
    MGsPacket *packet;
    MEfGsQword *setup;
    AlignedVector4 point;
    SvTexParam param;
    Vector4 *sample;
    int layer;
    int sampleIndex;
    int quad;

    block = &object->block;
    packet = &block->gsPacket;
    MGsGPInit(packet, 0, 0);
    setup = packet->current;
    setup[0].gif.tag = MEF_GS_SETUP_TAG;
    setup[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    SetRegister(&setup[1], MEF_GS_TEST_1, MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL);
    SetRegister(&setup[2], MEF_GS_TEX1_1, 96);
    SetRegister(&setup[3], MEF_GS_CLAMP_1, 5);
    SetRegister(&setup[4], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&setup[5], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE_FIX);
    CloseTag(packet, 5);
    svGetPrmFromName("sh_hikari.bmp", object->paramName, &param);

    for (layer = 0; layer < 4; layer++) {
        sampleIndex = (short)block->ticks - layer;
        if ((unsigned int) sampleIndex < MEF_PATH_SAMPLES) {
            /* samples[index] as int address arithmetic: the operand order of its addu is part of the matched code. */
            sample = (Vector4 *)((int)sampleIndex * 16 + (int)block + 192);
            for (quad = 0; quad < 2; quad++) {
                __asm__ __volatile__(
                    "lq $8, 0(%1)\nsq $8, 0(%0)\n"
                    :
                    : "r"(&point), "r"(sample), "m"(*sample)
                    : "$8", "memory");
                point.x += MMathMakeRandomSigned() * mefSampleJitter25[layer];
                point.y += MMathMakeRandomSigned() * mefSampleJitter25[layer];
                point.z += MMathMakeRandomSigned() * mefSampleJitter25[layer];
                MEfAppendThunderQuad25(&block->gsPacket, &param.tex0, &point, mefQuadSize25[layer]);
            }
        }
    }
    MGsGPTerm(&block->gsPacket);
}

void MEfAdvanceThunder25(MEfThunder25Effect *object)
{
    MEfThunder25Block *block;
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

/* Path-effect tables of the 25-sample effect: the spark stencil alpha, the sample jitter and the quad sizes. */
long long mefSparkStencilAlpha25 = 0x48;

float mefSampleJitter25[4] = { 0.05f, 0.04f, 0.03f, 0.02f };

float mefQuadSize25[4] = { 1.0f, 0.55f, 0.35f, 0.2f };
