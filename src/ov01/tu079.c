/*
 * OV01 original TU 79 (re-cut 2026-11-18: effect/menu files start at their Create)
 */

#include "common.h"

#include "main/nml_model_set.h"

#include "quadword.h"

#include "ov01/tu075.h"

#include "gs.h"

#include "shared.h"
#include "main/m_math_195.h"
#include "ov01/tu077.h"
#include "main/tu194.h"

#include "vu0_macro.h"

#include "main/tu197.h"
#include "main/m_math_193.h"
#include "main/tu196.h"

typedef int s32;

typedef long long s64;

/* Five trails (blocks), seven edge points each. */
#define MEF_TRAIL_BLOCKS 5
#define MEF_TRAIL_POINTS 7

/* Values of this file's own GS constants; the shared names are in ov01/tu075.h. */
#define MEF_GS_STRIP3_REGS 0x551 /* RGBAQ, XYZ2, XYZ2 */

/* Stencil B trail timing and motion. */
#define MEF_STENCIL_B_FADE_TICK 61
#define MEF_STENCIL_B_LIFETIME 0x44
#define MEF_STENCIL_B_DRIFT_HEIGHT 0.02
#define MEF_TRAIL_ALPHA_PERCENT 65



/* Stencil B data: edge offsets and the drift of the arms, the model name table, the alpha value. */
const Vector4 mefStencilEdgeOffsets[2] = {
    { -0.018f, 0.0f, 0.0f, 1.0f },
    { 0.018f, 0.0f, 0.0f, 1.0f },
};

const unsigned char mefStencilModelNameB[32] = "missile.pxy\0\0\0\0\0\x30\0\0\0\x50\0\0\0\x60\0\0\0\xa0\0\0\0";

const Vector4 mefStencilDrift = { 0.0f, 0.0f, 0.8f, 1.0f };

/* The sub-record of an effect object at +0x20: the stencil trail state (translation,
 * base and block matrices, scales, edge points and transparency). Only the fields
 * this file reads or writes are modelled. */

typedef struct MEfEffectSub {
    unsigned char unmodeled_00[4];
    struct Actor *actor;           /* 0x04 */
    u32 coordinate;                /* 0x08 */
    unsigned char unmodeled_0c[0x20 - 0x0c];
    Vector4 translation;           /* 0x20: read by MMathTranslateMatrix in MEfCreateStencilB */
    unsigned char unmodeled_30[0x40 - 0x30];
    MGsPacket packet;              /* 0x40 */
    unsigned short ticks;          /* 0x54 */
    unsigned char unmodeled_56[0x60 - 0x56];
    Matrix4 baseMatrix;            /* 0x60: the actor matrix the blocks are built from */
    Matrix4 matrices[MEF_TRAIL_BLOCKS]; /* 0xA0: one per block, the stencil copy source */
    f32 scales[MEF_TRAIL_BLOCKS];  /* 0x1E0 */
    unsigned char unmodeled_1f4[0x200 - 0x1F4];
    Vector4 leftEdge[MEF_TRAIL_BLOCKS * MEF_TRAIL_POINTS]; /* 0x200: 7 trail points per block */
    Vector4 rightEdge[MEF_TRAIL_BLOCKS * MEF_TRAIL_POINTS]; /* 0x430 */
    f32 transparency_b;            /* 0x660 */
} MEfEffectSub;

typedef struct MEfEffectObject {
    unsigned char unmodeled_00[4];
    s32 slot;                      /* 0x04: passed to svGetPrmFromName */
    void (*stepFunc)(struct MEfEffectObject *);    /* 0x08 */
    void (*drawFunc)(struct MEfEffectObject *);    /* 0x0C */
    void (*drawStripsFunc)(struct MEfEffectObject *); /* 0x10 */
    void (*advanceFunc)(struct MEfEffectObject *); /* 0x14 */
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfEffectSub sub;              /* 0x20 */
} MEfEffectObject;

extern s64 mefStencilAlphaB;
extern const unsigned char mefStencilModelNameB[];
extern const Vector4 mefStencilEdgeOffsets[];
extern const Vector4 mefStencilDrift;

s16 ov01LastValueGet(void);
double fptodp(float value);
float MMathMakeRandomSigned(void);
int func_002A7510(double a, double b);
void pxySetTxyBlock(s32 value);
void pxySubmitModel(s32 value);
void svGetPrmFromName(void *table, s32 slot, s32 *params);
void nmlModelSetTransparency(f32 transparency);
void MEfUpdateStencilB(MEfEffectObject *obj);
void MEfDrawStencilB(MEfEffectObject *obj);
void MEfDrawTrailStrips(MEfEffectObject *obj);
void MEfAdvanceStencilB(MEfEffectObject *object);

s32 MEfCreateStencilB(MEfEffectObject *obj)
{
    MEfEffectSub *sub;
    Matrix4 actorMatrix;
    Matrix4 scratch;
    Matrix4 *blockMatrix;
    f32 *blockScale;
    int firstPoint;
    int block;
    int point;

    sub = &obj->sub;
    sub->ticks = 0;
    MSeGetActorMatrix(&actorMatrix, sub->actor, sub->coordinate);
    MMathTranslateMatrix(&actorMatrix, &actorMatrix, &sub->translation);
    QuadwordCopyMatrixT0(sub->baseMatrix, &actorMatrix);
    block = 0;
    for (; block < MEF_TRAIL_BLOCKS; block++) {
        /* Each block swings: yaw up to 5 degrees, pitch up to 3 degrees (radians below). */
        blockMatrix = &sub->matrices[block];
        MMathRotateMatrixY(&scratch, &actorMatrix, MMathMakeRandomSigned() * 0.0872664600610733f);
        MMathRotateMatrixX(&scratch, &scratch, MMathMakeRandomSigned() * 0.05235987901687622f);
        QuadwordCopyMatrixT0(blockMatrix, &scratch);
        /* Spin of the block about Z (radians, up to 12 degrees either way), applied by the advance step. */
        blockScale = &sub->scales[block];
        *blockScale = MMathMakeRandomSigned() * 0.20943951606750488f;
        firstPoint = block * MEF_TRAIL_POINTS;
        for (point = 0; point < MEF_TRAIL_POINTS; point++) {
            __asm__ __volatile__(
                "lqc2 vf1, 48(%0)\n"
                "vmove.w vf1w, vf0w\n"
                "sqc2 vf1, 0(%1)\n"
                :
                : "r" (&scratch), "r" (&sub->leftEdge[firstPoint + point])
                : "memory");
            __asm__ __volatile__(
                "lqc2 vf1, 48(%0)\n"
                "vmove.w vf1w, vf0w\n"
                "sqc2 vf1, 0(%1)\n"
                :
                : "r" (&scratch), "r" (&sub->rightEdge[firstPoint + point])
                : "memory");
        }
    }
    sub->transparency_b = 1.0f;
    MGsGPInit(&sub->packet, 0, 0);
    obj->stepFunc = MEfUpdateStencilB;
    obj->drawFunc = MEfDrawStencilB;
    obj->drawStripsFunc = MEfDrawTrailStrips;
    obj->advanceFunc = MEfAdvanceStencilB;
    return 1;
}

/*
 * Step (while ov01LastValueGet() is 0): each trail moves one point along, its
 * newest left and right edge points come from the block matrix, and the
 * transparency fades by 20% per step once the effect is older than
 * MEF_STENCIL_B_FADE_TICK.
 */
void MEfUpdateStencilB(MEfEffectObject *obj)
{
    MEfEffectSub *sub;
    unsigned char *point;
    unsigned char *blockEdges;
    unsigned char *blockStart;
    int firstPoint;
    int block;
    int shift;

    sub = &obj->sub;
    if (ov01LastValueGet() == 0) {
        block = 0;
        blockEdges = (unsigned char *)sub;
        firstPoint = 0;
        for (; block < MEF_TRAIL_BLOCKS; block++) {
            blockStart = (unsigned char *)(firstPoint * 16);
            blockStart += (int)sub;
            point = blockStart + 96; /* the block's last point, 6 * 16 bytes in */
            for (shift = MEF_TRAIL_POINTS - 1; shift > 0; shift--) {
                QuadwordCopyT0(point + 512, point + 496);
                QuadwordCopyT0(point + 1072, point + 1056);
                point -= 16;
            }
            MMathApplyMatrix((Vector4 *)(blockEdges + 512), &sub->matrices[block], &mefStencilEdgeOffsets[0]);
            MMathApplyMatrix((Vector4 *)(blockEdges + 1072), &sub->matrices[block], &mefStencilEdgeOffsets[1]);
            blockEdges += 112; /* MEF_TRAIL_POINTS points of 16 bytes */
            firstPoint += MEF_TRAIL_POINTS;
        }
        if ((short) sub->ticks >= MEF_STENCIL_B_FADE_TICK) {
            sub->transparency_b *= 0.8f;
        }
    }
}

/* Same stencil draw as MEfDrawStencilA, with the second table, value and
 * transparency. One pass per block: remaining runs 3 down to -1, five passes
 * over the block matrices sub->matrices. */
void MEfDrawStencilB(MEfEffectObject *obj)
{
    MEfEffectSub *sub;
    s32 params[8];
    Matrix4 *cursor;
    s32 remaining;

    sub = &obj->sub;
    svGetPrmFromName(mefStencilModelNameB, obj->slot, params);
    cursor = sub->matrices;
    remaining = 4;
    do {
        remaining -= 1;
        nmlModelSetStencil(1);
        nmlModelSetStencilAlpha(mefStencilAlphaB);
        nmlModelSetTransparency(sub->transparency_b);
        pxySetTxyBlock(params[5]);
        nmlModelSetDrawMatrix(cursor);
        cursor++;
        pxySubmitModel(params[4]);
    } while (remaining >= 0);
}

/* Writes a GIF tag for regCount registers; *data receives its first data qword. */
static inline void OpenTag(MGsPacket *packet, MEfGsQword **data, int pre, int prim, int regCount, u64 registers)
{
    MEfGsQword *qword = packet->current;

    *data = qword + 1;
    qword->gif.tag = GIF_TAG_EOP | ((u64) pre << 46) | ((u64) prim << 47) | ((u64) regCount << 60);
    qword->gif.registers = registers;
    packet->regCount = regCount;
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

/* A packed GS vertex: the projected point, or its words (w carries ADC). */
typedef union MEfGsVertex {
    AlignedVector4 vector;
    Quadword whole;
    int words[4];
} MEfGsVertex;

/* A packed RGBAQ colour quadword. */
typedef union MEfGsColor {
    Quadword whole;
    unsigned int rgba[4];
} MEfGsColor;


/*
 * Draws the five trails as strips: each step projects the two edge points
 * MEfUpdateStencilB keeps at sub+0x200 and sub+0x430 (7 per trail), and scales the
 * colour alpha to 65% of its value per step. A clipped point suppresses the
 * drawing kick of the next two steps (the sibling of MEfDrawSwayStrands in tu078).
 */
void MEfDrawTrailStrips(MEfEffectObject *obj)
{
    AlignedVector4 leftPoint;
    AlignedVector4 rightPoint;
    MEfGsVertex topVertex;
    MEfGsVertex bottomVertex;
    MEfGsColor stripColor;
    Matrix4 strandMatrix;
    MEfEffectSub *sub;
    MGsPacket *packet;
    MEfGsQword *setup;
    MEfGsQword *write;
    struct XglCamera *camera;
    Matrix4 *viewMatrix;
    int strand;
    int step;
    int clipSkip;

    sub = &obj->sub;
    packet = &obj->sub.packet;
    camera = MEfCamParams.camera;
    viewMatrix = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);
    setup = packet->current;
    setup[0].gif.tag = MEF_GS_SETUP_TAG;
    setup[0].gif.registers = GIF_AD;
    packet->regCount = 1;
    SetRegister(&setup[1], GS_TEST_1, MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL);
    SetRegister(&setup[2], GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&setup[3], GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);
    for (strand = 0; strand < MEF_TRAIL_BLOCKS; strand++) {
        /* RGBAQ words 0x30, 0x50, 0x60 and 0xA0 (alpha is the last). */
        stripColor.whole = 0x000000A0000000600000005000000030;
        clipSkip = 0;
        QuadwordCopyMatrixT0(strandMatrix, sub->matrices[strand]);
        OpenTag(&sub->packet, &write, 1, MEF_GS_STRIP_PRIM, 3, MEF_GS_STRIP3_REGS);
        for (step = 0; step < MEF_TRAIL_POINTS; step++) {
            __asm__ __volatile__(
                "lq $8, 0(%3)\n"
                "sq $8, 0(%2)\n"
                : "=m" (leftPoint.w), "=m" (leftPoint.w)
                : "r" (&leftPoint), "r" (&sub->leftEdge[strand * MEF_TRAIL_POINTS + step]), "r" (&leftPoint)
                : "$8");
            __asm__ __volatile__(
                "lq $8, 0(%3)\n"
                "sq $8, 0(%2)\n"
                : "=m" (rightPoint.w), "=m" (rightPoint.w)
                : "r" (&rightPoint), "r" (&sub->rightEdge[strand * MEF_TRAIL_POINTS + step]), "r" (&rightPoint)
                : "$8");
            if (MMathRotTransPersClip(&topVertex.vector, camera, viewMatrix, &leftPoint) != 0) {
                clipSkip = 2;
            } else if (clipSkip != 0) {
                clipSkip--;
                topVertex.words[3] = MEF_GS_XYZ_ADC;
            }
            if (MMathRotTransPersClip(&bottomVertex.vector, camera, viewMatrix, &rightPoint) != 0) {
                clipSkip = 2;
            } else if (clipSkip != 0) {
                clipSkip--;
                bottomVertex.words[3] = MEF_GS_XYZ_ADC;
            }
            write[0].whole = stripColor.whole;
            write[1].whole = topVertex.whole;
            write[2].whole = bottomVertex.whole;
            write += 3;
            stripColor.rgba[3] = stripColor.rgba[3] * MEF_TRAIL_ALPHA_PERCENT / 100;
        }
        CloseTag(&sub->packet, MEF_TRAIL_POINTS);
    }
    MGsGPTerm(&sub->packet);
}

/*
 * Advance (while ov01LastValueGet() is 0): a block whose y translation (matrix
 * row 3, element 1) is above MEF_STENCIL_B_DRIFT_HEIGHT drifts by mefStencilDrift and
 * spins about Z by its scale. The effect frees itself after
 * MEF_STENCIL_B_LIFETIME steps.
 */
void MEfAdvanceStencilB(MEfEffectObject *object)
{
    MEfEffectSub *sub;
    int index;
    int nextStep;

    sub = &object->sub;
    if (ov01LastValueGet() != 0) {
        return;
    }
    for (index = 0; index < MEF_TRAIL_BLOCKS; index++) {
        if (func_002A7510(fptodp(sub->matrices[index][3][1]), MEF_STENCIL_B_DRIFT_HEIGHT) > 0) {
            MMathTranslateMatrix(&sub->matrices[index], &sub->matrices[index], &mefStencilDrift);
            MMathRotateMatrixZ(&sub->matrices[index], &sub->matrices[index], sub->scales[index]);
        }
    }
    nextStep = sub->ticks + 1;
    sub->ticks = nextStep;
    if ((short) nextStep >= MEF_STENCIL_B_LIFETIME) {
        MSeObjRequestFree(object);
    }
}

s64 mefStencilAlphaB = 0x44;
