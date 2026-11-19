/*
 * OV01 TU 86: the spray effect (MEfCreateSpray and its callbacks).
 */

#include "common.h"
#include "shared.h"
#include "main/m_math_191.h"
#include "main/tu192.h"
#include "vu0_macro.h"
#include "main/tu197.h"
#include "main/tu187.h"
#include "quadword.h"
#include "ov01/tu075.h"
#include "main/tu196.h"

/* Spray points (one streak each) in the spray block. */
#define MEF_SPRAY_POINT_COUNT 20
/* Points per streak: a line strip of four vertices. */
#define MEF_SPRAY_STRIP_POINTS 4
/* Tick at which the origins switch from steps to scaled steps, and the tick at which the anchors start moving. */
#define MEF_SPRAY_STEP_TICKS 30
#define MEF_SPRAY_ANCHOR_TICKS 91
/* Tick count at which MEfTickLifetime121 ends the spray (one tick per frame). */
#define MEF_SPRAY_LIFETIME_TICKS 0x79

/* A spray slot with vector lanes: MEfGsQword (tu075.h) has no vector member, so the spray's point and vertex arrays use this view. */
typedef union SprayQword {
    Quadword whole;
    Vector4 vector;
    int words[4];
    unsigned short nloop;
    struct {
        u64 tag;
        u64 registers;
    } gif;
    struct {
        u64 data;
        unsigned char address;
    } ad;
} SprayQword;

typedef struct SprayBlock SprayBlock;

typedef struct SprayHost SprayHost;

/*
 * The spray block at object+0x20: four arrays of MEF_SPRAY_POINT_COUNT vectors, one entry per
 * spray point, filled in the same loop. The GS direct-transfer packet at
 * +0x40 (five words: data, capacity, current, count, regCount) is passed to
 * MGsGPInit; the 16-bit tick count at +0x54 is advanced by the tick callback,
 * which ends the effect at its threshold.
 */
struct SprayBlock {
    unsigned char unmodeled_00[0x40];
    MGsPacket gs;                       /* +0x40: GS packet of the draw callback */
    u16 ticks;                          /* +0x54 */
    unsigned char unmodeled_56[0x60 - 0x56];
    AlignedVector4 origins[MEF_SPRAY_POINT_COUNT];         /* +0x60: spray point positions */
    AlignedVector4 anchors[MEF_SPRAY_POINT_COUNT];         /* +0x1a0 */
    AlignedVector4 steps[MEF_SPRAY_POINT_COUNT];           /* +0x2e0 */
    AlignedVector4 scaledSteps[MEF_SPRAY_POINT_COUNT];     /* +0x420 */
};

struct SprayHost {
    unsigned char unmodeled_00[0x08];
    void (*stepPoints)(SprayHost *); /* +0x08 */
    unsigned char unmodeled_0c[0x10 - 0x0c];
    void (*draw)(SprayHost *);       /* +0x10 */
    void (*tickLifetime)(SprayHost *); /* +0x14 */
    unsigned char unmodeled_18[0x20 - 0x18];
    SprayBlock block;                /* +0x20 */
};

extern const SprayQword mefSprayColors[2];     /* the two spray colours */
extern const int mefSprayAlphas[4][4];          /* alpha of the four points, per ring */

int MMathRandomNext(void);

float MMathMakeRandom2PI(void);
float MMathMakeRandomSigned(void);
void MEfStepSprayPoints(SprayHost *host);
void MEfDrawSpray(SprayHost *host);
void MEfTickLifetime121(SprayHost *host);

short ov01LastValueGet(void);

int MEfCreateSpray(SprayHost *host)
{
    SprayBlock *block;
    AlignedVector4 origin;
    AlignedVector4 target;
    AlignedVector4 *targetPtr;
    Vector4 *step;
    float angle;
    float radius;
    float unitX;
    float jitter;
    Vector4 stored;                          /* dummy memory outputs: the asm stores into them */
    Vector4 storedAngle;
    int sprayIndex;

    block = &host->block;
    block->ticks = 0;
    targetPtr = &target;
    for (sprayIndex = 0; sprayIndex < MEF_SPRAY_POINT_COUNT; sprayIndex++) {
        angle = MMathMakeRandom2PI();
        radius = MMathMakeRandom() * 13.5f + 1.0f;
        __asm__ __volatile__("sqc2 $vf0, %0\n\t"
                "mfc1 $8, %2\n\t"
                "qmtc2 $8, $vf4\n\t"
                "vcallms 0xe8\n\t"
                "qmfc2.i $8, $vf1\n\t"
                "mtc1 $8, %1\n\t"
                "sll $0, $0, 0"
                : "=m"(origin), "=f"(unitX) : "f"(angle) : "$8", "memory");
        __asm__("swc1 %1, %0" : "=m"(stored.x) : "f"(unitX) : "memory");
        origin.x = unitX * radius;
        origin.y = 30.0f;
        __asm__("mfc1 $8, %1\n\t"
                "qmtc2 $8, $vf4\n\t"
                "vcallms 0x20\n\t"
                "qmfc2.i $8, $vf1\n\t"
                "mtc1 $8, %0\n\t"
                "sll $0, $0, 0"
                : "=f"(angle) : "f"(angle) : "$8", "memory");
        __asm__("swc1 %1, %0" : "=m"(storedAngle.x) : "f"(angle) : "memory");
        origin.z = angle * radius;
        __asm__("sqc2 $vf0, 0(%1)" : "=m"(*targetPtr) : "r"(targetPtr) : "memory");
        jitter = MMathMakeRandomSigned() * 0.5f;
        target.y = 0.0f;
        target.x = origin.x + jitter;
        target.z = origin.z + MMathMakeRandomSigned() * 0.5f;
        QuadwordCopyT0(&block->origins[sprayIndex], &origin);
        QuadwordCopyT0(&block->anchors[sprayIndex], &origin);
        step = &block->steps[sprayIndex];
        MMathSubVectorMulS(step, targetPtr, &origin, 1.0f / 30.0f);
        __asm__("lqc2 $vf1, 0(%0)\n\t"
                "mfc1 $8, %2\n\t"
                "qmtc2 $8, $vf2\n\t"
                "vmulx.xyz $vf1xyz, $vf1xyz, $vf2x\n\t"
                "sqc2 $vf1, 0(%1)"
                : : "r"(step), "r"(&block->scaledSteps[sprayIndex]), "f"(0.333f) : "$8", "memory");
    }
    MGsGPInit(&block->gs, 0, 0);
    host->stepPoints = MEfStepSprayPoints;
    host->draw = MEfDrawSpray;
    host->tickLifetime = MEfTickLifetime121;
    return 1;
}

void MEfStepSprayPoints(SprayHost *host)
{
    SprayBlock *block;
    unsigned short ticks;
    int index;

    block = &host->block;
    if (ov01LastValueGet() != 0) {
        return;
    }
    if ((short) block->ticks < MEF_SPRAY_STEP_TICKS) {
        ticks = block->ticks;
        for (index = 0; index < MEF_SPRAY_POINT_COUNT; index++) {
            Vu0SefAddVectorXYZ(&block->origins[index], &block->origins[index], &block->steps[index]);
        }
    } else {
        ticks = block->ticks;
        for (index = 0; index < MEF_SPRAY_POINT_COUNT; index++) {
            Vu0SefAddVectorXYZ(&block->origins[index], &block->origins[index], &block->scaledSteps[index]);
        }
    }
    if ((short) ticks >= MEF_SPRAY_ANCHOR_TICKS) {
        for (index = 0; index < MEF_SPRAY_POINT_COUNT; index++) {
            Vu0SefAddVectorXYZ(&block->anchors[index], &block->anchors[index], &block->steps[index]);
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

/* Writes a GIF tag for regCount registers and returns its first data qword. */
static inline MEfGsQword *OpenTag(MGsPacket *packet, int pre, int prim, int regCount, u64 registers)
{
    MEfGsQword *qword = packet->current;

    qword->gif.tag = MEF_GIF_TAG_EOP | ((u64) pre << 46) | ((u64) prim << 47) | ((u64) regCount << 60);
    qword->gif.registers = registers;
    packet->regCount = regCount;
    return qword + 1;
}

/*
 * Writes one streak as a line strip of four points: colour and vertex per
 * point. slot is the quadword in front of the four vertices, so the vertices
 * are slot[1..4]. The counter and the write cursor are the caller's.
 */
#define WriteStreak(packet, colours, slot) \
    { \
        qword = OpenTag(packet, 1, 0x4A, 2, MEF_GS_RGBAQ_XYZ2_REGS); \
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) { \
            qword[0].whole = (colours)[pointIndex].whole; \
            qword[1].whole = (slot)[pointIndex + 1].whole; \
            qword += 2; \
        } \
        CloseTag(packet, 4); \
    }

/*
 * Draw callback of the spray: after the GS state, each of the twenty streaks
 * is projected at four points between its origin and its anchor and drawn
 * seven times, side by side, with the alpha of each ring. The packet is
 * flushed after every tenth streak.
 */
void MEfDrawSpray(SprayHost *host)
{
    SprayQword point;                        /* the interpolated point; the strip indexes its vertices from here */
    SprayQword vertex[MEF_SPRAY_STRIP_POINTS]; /* the four projected points, directly after it */
    SprayQword colour[MEF_SPRAY_STRIP_POINTS];
    SprayBlock *block;
    MGsPacket *packet;
    MEfGsQword *header;
    MEfGsQword *qword;
    int tint;                                /* which of the two colours */
    struct XglCamera *camera;
    Matrix4 *view;
    int skip;
    int streak;
    int pointIndex;

    block = &host->block;
    packet = &host->block.gs;
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
    CloseTag(packet, 3);

    for (streak = 0; streak < MEF_SPRAY_POINT_COUNT; streak++) {
        skip = 0;
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) {
            MMathVectorInterpolation(&point.vector, &block->origins[streak], &block->anchors[streak], (float) pointIndex * 0.33333334f);
            if (MMathRotTransPersClip(&vertex[pointIndex].vector, camera, view, &point.vector) != 0) {
                skip = 1;
            } else if (skip != 0) {
                skip--;
                vertex[pointIndex].words[3] = MEF_GS_XYZ_ADC;
            }
        }
        tint = MMathRandomNext() & 1;
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) {
            colour[pointIndex].whole = mefSprayColors[tint].whole;
            colour[pointIndex].words[3] = mefSprayAlphas[0][pointIndex];
        }
        WriteStreak(&block->gs, colour, &point);
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) {
            colour[pointIndex].words[3] = mefSprayAlphas[1][pointIndex];
            vertex[pointIndex].words[0] -= 16;
        }
        WriteStreak(&block->gs, colour, &point);
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) {
            vertex[pointIndex].words[0] += 32;
        }
        WriteStreak(&block->gs, colour, &point);
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) {
            colour[pointIndex].words[3] = mefSprayAlphas[2][pointIndex];
            vertex[pointIndex].words[0] -= 48;
        }
        WriteStreak(&block->gs, colour, &point);
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) {
            vertex[pointIndex].words[0] += 64;
        }
        WriteStreak(&block->gs, colour, &point);
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) {
            colour[pointIndex].words[3] = mefSprayAlphas[3][pointIndex];
            vertex[pointIndex].words[0] -= 80;
        }
        WriteStreak(&block->gs, colour, &point);
        for (pointIndex = 0; pointIndex < MEF_SPRAY_STRIP_POINTS; pointIndex++) {
            vertex[pointIndex].words[0] += 96;
        }
        WriteStreak(&block->gs, colour, &point);
        if (streak % 10 == 9) {
            MGsGPTerm(&block->gs);
        }
    }
}

void MEfTickLifetime121(SprayHost *host)
{
    SprayBlock *block;
    int ticks;

    block = &host->block;
    if (ov01LastValueGet() != 0) {
        return;
    }
    ticks = block->ticks + 1;
    block->ticks = ticks;
    if ((short) ticks >= MEF_SPRAY_LIFETIME_TICKS) {
        MSeObjRequestFree(host);
    }
}

/* The two spray colours (RGBA words of each quadword), then the alpha of the four points per ring. */
const SprayQword mefSprayColors[2] = {
    0x000000ff000000400000006000000070,
    0x000000ff000000500000006800000070,
};

const int mefSprayAlphas[4][4] = {
    { 0xff, 0xaa, 0x55, 0x00 },
    { 0x58, 0x3a, 0x1d, 0x00 },
    { 0x38, 0x25, 0x12, 0x00 },
    { 0x28, 0x1a, 0x0d, 0x00 },
};
