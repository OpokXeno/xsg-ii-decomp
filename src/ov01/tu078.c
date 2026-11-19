/*
 * OV01 original TU 78 (re-cut 2026-11-18: effect/menu files start at their Create)
 */

#include "common.h"
#include "main/tu197.h"
#include "shared.h"
#include "main/m_math_195.h"
#include "main/m_math_191.h"
#include "main/tu194.h"
#include "quadword.h"
#include "vu0_macro.h"
#include "main/nml_model_set.h"
#include "gs.h"
#include "ov01/tu075.h"
#include "main/tu190.h"
#include "main/m_math_193.h"
#include "main/tu196.h"

typedef int s32;

typedef long long s64;

/* GS values of the sway only: TEST_1 with alpha test on (ATST GREATER against 0),
 * z test GEQUAL; the strip register list RGBAQ, XYZ2, XYZ2. */
#define MEF_SWAY_TEST_1 0x5000D
#define MEF_SWAY_STRIP_REGS 0x551
#define MEF_SWAY_ALPHA_KEEP_PERCENT 65
/* Arms of one sway object and sample points per arm. */
#define MEF_SWAY_ARMS 5
#define MEF_SWAY_SAMPLES 7

/* Arm angles and steps of the sway. */
#define MEF_SWAY_START_ANGLE 0.69813168f /* 40 degrees, radians: random tilt at creation */
#define MEF_DEG_TO_RAD 0.0174532924f
#define MEF_SWAY_FLOOR_HEIGHT 0.02
#define MEF_SWAY_HISTORY_STEP 16 /* the sample history shifts from here on */
#define MEF_SWAY_REACH_STEP 15 /* the arms switch from spread to reach; placed here */
#define MEF_SWAY_TWIST_GROW_STEPS 6 /* the twist of the spread grows for these steps */
#define MEF_SWAY_FADE_STEP 76 /* the scale fades from here on */
#define MEF_SWAY_END_STEP 0x53 /* the object is freed here */

/*
 * The block at +0x20 of the sway object: actor pair, translation, packet, step counter,
 * matrix and position, five spline matrices and points, a 5x7 sample array, arm and
 * jitter arrays and five vectors.
 */
typedef struct MEfSwayBlock {
    unsigned char unmodeled_00[0x04];
    int actor;                               /* +0x04 */
    int bone;                                /* +0x08 */
    unsigned char unmodeled_0c[0x20 - 0x0c];
    Vector4 translation;                     /* +0x20 */
    unsigned char unmodeled_30[0x40 - 0x30];
    MGsPacket packet;                        /* +0x40 */
    unsigned short step;                     /* +0x54 */
    unsigned char unmodeled_56[0x60 - 0x56];
    Matrix4 matrix;                          /* +0x60 */
    Vector4 position;                        /* +0xa0 */
    Matrix4 spline[MEF_SWAY_ARMS];                       /* +0xb0 */
    Vector4 points[MEF_SWAY_ARMS];                       /* +0x1f0 */
    Vector4 samples[MEF_SWAY_ARMS][MEF_SWAY_SAMPLES];                   /* +0x240 */
    float scale_a;                           /* +0x470 */
    unsigned char unmodeled_474[0x480 - 0x474];
    union {
        float wave[10];                      /* +0x480 */
        union {
            struct {
                float push;                      /* +0x480: z push of every arm until step 15 */
                float twist[MEF_SWAY_ARMS];                  /* +0x484: z turn of each arm per step */
            } spread;
            struct {
                float length[MEF_SWAY_ARMS];                 /* +0x480: arm length from step 15 */
                float speed[MEF_SWAY_ARMS];                  /* +0x494: its growth per step */
            } reach;
        } arms;                                  /* +0x480: the two phases share the floats */
    };
    unsigned char unmodeled_4a8[0x4b0 - 0x4a8];
    union {
        Vector4 vecs[MEF_SWAY_ARMS];                     /* +0x4b0: the arms' x/y turns */
        float vecAxes[MEF_SWAY_ARMS][4];                 /* +0x4b0: the same turns by axis */
    };
    float jitter[MEF_SWAY_ARMS];                         /* +0x500 */
} MEfSwayBlock;

typedef struct MEfSwayObject {
    unsigned char unmodeled_00[0x04];
    s32 slot;                                /* +0x04: effect slot passed to svGetPrmFromName */
    void (*callbacks[4])(struct MEfSwayObject *); /* +0x08 */
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfSwayBlock block;                      /* +0x20 */
} MEfSwayObject;

/* The two edge points of the sway, then four words (0x30, 0x50, 0x60, 0xa0) of unknown meaning. */
typedef struct MEfSwayEdgeTable {
    Vector4 points[2];
    unsigned int words[4];
} MEfSwayEdgeTable;

extern const MEfSwayEdgeTable mefSwayEdgePoints;

int ov01LastValueGet(void);
extern float MMathMakeRandomSigned(void);
extern int MMathRandomNext(void);
/* MSeGetActorMatrix: the tu077.c definition returns Matrix4 * and takes Matrix4 *, Actor *; this file keeps its void view. */
void MSeGetActorMatrix(Matrix4 *out, int actor, int bone);
void MEfDrawStencilA(MEfSwayObject *object);
void MEfDrawSwayStrands(MEfSwayObject *object);
void MEfAdvanceSway(MEfSwayObject *object);
void MEfUpdateSway(MEfSwayObject *object);

#define Vu0MMathCopyPointAt(destination, base, offset) \
    __asm__ __volatile__( \
        "lqc2 vf1, %2(%1)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, 0(%0)\n" \
        : \
        : "r" (destination), "r" (base), "i" (offset) \
        : "memory")
#define Vu0MMathCopyPointAtW(destination, base, offset) \
    __asm__ __volatile__( \
        "lqc2 vf1, %4(%3)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, 0(%2)\n" \
        : "=m" (*(destination)), "=m" ((destination)->w) \
        : "r" (destination), "r" (base), "i" (offset) \
        : "memory")
#define Vu0MMathCopyPointAtW2(destination, base, offset) \
    __asm__ __volatile__( \
        "lqc2 vf1, %5(%4)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, 0(%3)\n" \
        : "=m" (*(destination)), "=m" ((destination)->w), "=m" ((destination)->w) \
        : "r" (destination), "r" (base), "i" (offset) \
        : "memory")

#define Vu0MMathCopyPointTo(base, offset, source) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%2)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, %1(%0)\n" \
        : \
        : "r" (base), "i" (offset), "r" (source) \
        : "memory")
#define Vu0MMathCopyPointAtV2(destination, base, offset) \
    __asm__ __volatile__( \
        "lqc2 vf1, %5(%4)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, 0(%3)\n" \
        : "=m" (*(destination)), "=m" (*(destination)), "=m" ((destination)->w) \
        : "r" (destination), "r" (base), "i" (offset))
#define QuadwordCopyT0W(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%3)\n" \
        "sq $8, 0(%2)\n" \
        : "=m" (*(destination)), "=m" ((destination)->w) \
        : "r" (destination), "r" (source) \
        : "$8")

int MEfCreateSway(MEfSwayObject *object)
{
    MEfSwayBlock *block;
    Matrix4 baseMatrix;
    Matrix4 armMatrix;
    int i;
    int j;
    float sign;

    block = &object->block;
    block->step = 0;
    MSeGetActorMatrix(&baseMatrix, block->actor, block->bone);
    MMathTranslateMatrix(&baseMatrix, &baseMatrix, &block->translation);
    QuadwordCopyMatrixT0(block->matrix, baseMatrix);
    Vu0MMathCopyPointAt(&block->position, baseMatrix, 48);

    for (i = 0; i < MEF_SWAY_ARMS; i++) {
        MMathRotateMatrixY(&armMatrix, &baseMatrix, MMathMakeRandomSigned() * MEF_SWAY_START_ANGLE);
        MMathRotateMatrixX(&armMatrix, &armMatrix, MMathMakeRandomSigned() * MEF_SWAY_START_ANGLE);
        QuadwordCopyMatrixT0(block->spline[i], armMatrix);
        Vu0MMathCopyPointAtW2(&block->points[i], armMatrix, 48);
        for (j = 0; j < MEF_SWAY_SAMPLES; j++) {
            Vu0MMathCopyPointAtW(&block->samples[i][j], armMatrix, 48);
        }
    }

    block->scale_a = 1.0f;
    block->wave[0] = 0.5f;
    for (i = 0; i < MEF_SWAY_ARMS; i++) {
        sign = -MEF_DEG_TO_RAD;
        if (!(MMathRandomNext() & 1)) {
            sign = MEF_DEG_TO_RAD;
        }
        block->wave[1 + i] = sign;
    }

    MGsGPInit(&block->packet, 0, 0);
    object->callbacks[0] = MEfUpdateSway;
    object->callbacks[1] = MEfDrawStencilA;
    object->callbacks[2] = MEfDrawSwayStrands;
    object->callbacks[3] = MEfAdvanceSway;
    return 1;
}

void MEfUpdateSway(MEfSwayObject *object)
{
    MEfSwayBlock *block;
    Vector4 armTip;
    float jitterAngle;
    float *armSpeed;
    int i;
    int j;

    block = &object->block;
    if (ov01LastValueGet() != 0) {
        return;
    }
    if ((short) block->step >= MEF_SWAY_HISTORY_STEP) {
        for (i = 0; i < MEF_SWAY_ARMS; i++) {
            for (j = MEF_SWAY_SAMPLES - 1; j > 0; j--) {
                QuadwordCopyT0W(&block->samples[i][j], &block->samples[i][j - 1]);
            }
            Vu0MMathCopyPointAtV2(&block->samples[i][0], block->spline[i], 48);
        }
    }
    if ((short) block->step == MEF_SWAY_REACH_STEP) {
        armSpeed = &block->wave[5];
        for (i = 0; i < MEF_SWAY_ARMS; i++) {
            Vu0MMathCopyPointAt(&armTip, &block->spline[i], 48);
            block->wave[i] = MMathCalcDist(&block->position, &armTip);
            armSpeed[i] = 0.1f;
            MMathCalcAngle(&block->vecs[i], &block->position, &armTip);
            jitterAngle = MMathMakeRandomSigned() * 4.0f + 16.0f;
            block->jitter[i] = (MMathRandomNext() & 1) ? jitterAngle * -MEF_DEG_TO_RAD : jitterAngle * MEF_DEG_TO_RAD;
            Vu0MMathCopyPointAt(&armTip, &block->spline[i], 48);
            MMathRotateMatrixYX(&block->spline[i], 0, &block->vecs[i]);
            Vu0MMathCopyPointTo(block->spline[i], 48, &armTip);
            for (j = 0; j < MEF_SWAY_SAMPLES; j++) {
                Vu0MMathCopyPointAtW(&block->samples[i][j], block->spline[i], 48);
            }
        }
    }
    if ((short) block->step >= MEF_SWAY_FADE_STEP) {
        block->scale_a *= 0.8f;
    }
}


void pxySetTxyBlock(s32 value);
void pxySubmitModel(s32 value);
void svGetPrmFromName(void *table, s32 slot, s32 *params);
void nmlModelSetTransparency(f32 transparency);
extern s64 mefStencilAlphaA;
extern const unsigned char mefStencilModelNameA[];

int func_002A7510(double a, double b);
extern const Vector4 mefSwayArmAngles;

/* Stencil A and sway data of the rodata: the model name table, the sway edge table, the arm angles. */
const unsigned char mefStencilModelNameA[16] = "missile.pxy";

const MEfSwayEdgeTable mefSwayEdgePoints = {
    { { -0.018f, 0.0f, 0.0f, 1.0f }, { 0.018f, 0.0f, 0.0f, 1.0f } },
    { 0x30, 0x50, 0x60, 0xa0 },
};

const Vector4 mefSwayArmAngles = { 1.5707964f, 1.5707964f, 1.5707964f, 1.0f };
/* Five passes of the stencil draw (the loop counter runs from 4 down to -1). The
 * parameter record (8 ints) is looked up by slot; params[4] and params[5] feed the
 * two calls after the model setup. */
void MEfDrawStencilA(MEfSwayObject *object)
{
    MEfSwayBlock *block;
    s32 params[8];
    Matrix4 *splineCursor;
    s32 remaining;

    block = &object->block;
    svGetPrmFromName(mefStencilModelNameA, object->slot, params);
    splineCursor = block->spline;
    remaining = 4;
    do {
        remaining -= 1;
        nmlModelSetStencil(1);
        nmlModelSetStencilAlpha(mefStencilAlphaA);
        nmlModelSetTransparency(block->scale_a);
        pxySetTxyBlock(params[5]);
        nmlModelSetDrawMatrix(splineCursor);
        splineCursor++;
        pxySubmitModel(params[4]);
    } while (remaining >= 0);
}

/* Writes a GIF tag for regCount registers and returns its first data qword. */
static inline MEfGsQword *OpenTag(MGsPacket *packet, int pre, int prim, int regCount, u64 registers)
{
    MEfGsQword *qword = packet->current;

    qword->gif.tag = GIF_TAG_EOP | ((u64) pre << 46) | ((u64) prim << 47) | ((u64) regCount << 60);
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
 * Draws the five sway strands: each spline matrix is moved to every one of its
 * seven samples, the two edge points of mefSwayEdgePoints are projected, and the pair is
 * written as one strip step whose alpha keeps 65% of its value per sample. A clipped point
 * suppresses the drawing kick of the next two steps.
 */
void MEfDrawSwayStrands(MEfSwayObject *object)
{
    AlignedVector4 leftEdge;
    AlignedVector4 rightEdge;
    MEfGsVertex topVertex;
    MEfGsVertex bottomVertex;
    MEfGsColor stripColor;
    Matrix4 strandMatrix;
    MEfSwayBlock *block;
    MGsPacket *packet;
    MEfGsQword *setup;
    MEfGsQword *stripQword;
    Vector4 *sample;
    struct XglCamera *camera;
    Matrix4 *cameraView;
    int strand;
    int step;
    int skipSteps;

    block = &object->block;
    packet = &object->block.packet;
    camera = MEfCamParams.camera;
    cameraView = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);
    setup = packet->current;
    setup[0].gif.tag = MEF_GS_SETUP_TAG;
    setup[0].gif.registers = GIF_AD;
    packet->regCount = 1;
    SetRegister(&setup[1], GS_TEST_1, MEF_SWAY_TEST_1);
    SetRegister(&setup[2], GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&setup[3], GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);
    for (strand = 0; strand < MEF_SWAY_ARMS; strand++) {
        stripColor.whole = 0x000000A0000000600000005000000030;
        skipSteps = 0;
        QuadwordCopyMatrixT0(strandMatrix, block->spline[strand]);
        stripQword = OpenTag(&block->packet, 1, MEF_GS_STRIP_PRIM, 3, MEF_SWAY_STRIP_REGS);
        for (step = 0; step < MEF_SWAY_SAMPLES; step++) {
            sample = &block->samples[strand][step];
            __asm__ __volatile__(
                "lqc2 vf1, 0(%0)\n"
                "vmove.w vf1, vf0\n"
                "sqc2 vf1, 48(%1)\n"
                :
                : "r" (sample), "r" (strandMatrix), "r" (sample), "r" (strandMatrix)
                : "memory");
            MMathApplyMatrix(&leftEdge, &strandMatrix, &mefSwayEdgePoints.points[0]);
            MMathApplyMatrix(&rightEdge, &strandMatrix, &mefSwayEdgePoints.points[1]);
            if (MMathRotTransPersClip(&topVertex.vector, camera, cameraView, &leftEdge) != 0) {
                skipSteps = 2;
            } else if (skipSteps != 0) {
                skipSteps--;
                topVertex.words[3] = MEF_GS_XYZ_ADC;
            }
            if (MMathRotTransPersClip(&bottomVertex.vector, camera, cameraView, &rightEdge) != 0) {
                skipSteps = 2;
            } else if (skipSteps != 0) {
                skipSteps--;
                bottomVertex.words[3] = MEF_GS_XYZ_ADC;
            }
            stripQword[0].whole = stripColor.whole;
            stripQword[1].whole = topVertex.whole;
            stripQword[2].whole = bottomVertex.whole;
            stripQword += 3;
            stripColor.rgba[3] = stripColor.rgba[3] * MEF_SWAY_ALPHA_KEEP_PERCENT / 100;
        }
        CloseTag(&block->packet, 7);
    }
    MGsGPTerm(&block->packet);
}

/*
 * Late update of the sway: refreshes the five arm points from the spline
 * matrices. Until step 15 the arms are pushed out along z and twisted, the push
 * fading and (for the first steps) the twist growing; from step 15 every arm
 * still above the floor is rebuilt from the sway position, its angles and its
 * growing length. The object is freed at step 0x53.
 */
void MEfAdvanceSway(MEfSwayObject *object)
{
    MEfSwayBlock *block;
    Vector4 translation;
    Matrix4 armMatrix;
    Matrix4 *armSpline;
    Matrix4 *armScratch;
    Vector4 *swayPosition;
    float *armSpeed;
    int i;

    block = &object->block;
    if (ov01LastValueGet() != 0) {
        return;
    }
    for (i = 0; i < MEF_SWAY_ARMS; i++) {
        Vu0MMathCopyPointAtW(&object->block.points[i], object->block.spline[i], 48);
    }
    if ((short) block->step < MEF_SWAY_REACH_STEP) {
        Vu0SetOrigin(&translation);
        translation.z = block->arms.spread.push;
        for (i = 0; i < MEF_SWAY_ARMS; i++) {
            MMathTranslateMatrix(&block->spline[i], &block->spline[i], &translation);
            MMathRotateMatrixZ(&block->spline[i], &block->spline[i], block->arms.spread.twist[i]);
        }
        block->arms.spread.push *= 0.65f;
        if ((short) block->step < MEF_SWAY_TWIST_GROW_STEPS) {
            for (i = 0; i < MEF_SWAY_ARMS; i++) {
                block->arms.spread.twist[i] *= 1.85f;
            }
        }
    }
    if ((short) block->step >= MEF_SWAY_REACH_STEP) {
        for (i = 0, armSpline = block->spline, armScratch = &armMatrix, swayPosition = &block->position,
             armSpeed = block->arms.reach.speed; i < MEF_SWAY_ARMS; i++) {
            if (func_002A7510(block->spline[i][3][1], MEF_SWAY_FLOOR_HEIGHT) > 0) {
                MMathRotateMatrixYX(armScratch, 0, &block->vecs[i]);
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%2)\n"
                    "vmove.w vf1w, vf0w\n"
                    "sqc2 vf1, 48(%3)\n"
                    : "=m" (*armScratch), "=m" ((*armScratch)[3])
                    : "r" (swayPosition), "r" (armScratch), "m" (*swayPosition), "m" (swayPosition->w)
                    : "memory");
                Vu0SetOrigin(&translation);
                translation.z = block->arms.reach.length[i];
                MMathTranslateMatrix(armScratch, armScratch, &translation);
                MMathRotateMatrixXY(armSpline, armScratch, &mefSwayArmAngles);
                block->arms.reach.length[i] += armSpeed[i];
                armSpeed[i] *= 1.05f;
                block->vecAxes[i][1] += block->jitter[i];
                block->jitter[i] *= 0.992f;
            }
            armSpline++;
        }
    }
    block->step++;
    if ((short) block->step >= MEF_SWAY_END_STEP) {
        MSeObjRequestFree(object);
    }
}

s64 mefStencilAlphaA = 0x44;
