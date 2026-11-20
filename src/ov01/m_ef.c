#include "common.h"
#include "shared.h"
#include "main/m_math.h"
#include "ov01/m_ef.h"
#include "quadword.h"
#include "ov01/m_gs.h"
#include "vu0_macro.h"
#include "main/xgl_2.h"
#include "main/xgl_studio.h"
#include "main/m_se_obj.h"
#include "main/sm_initilize.h"
#include "main/sef.h"

/* Actor origin, raised origin, or an accessory coordinate selector. */
typedef enum MEfActorPoint {
    MEF_POINT_ACTOR = -1,
    MEF_POINT_ACTOR_RAISED = -2,
    MEF_POINT_ACCESSORY_COUNT = 0x28
} MEfActorPoint;







typedef struct MEfPhaseBlock {
    float cornerAngles[4];  /* four float corner bearings, read as float[4] by MEfMake4Vertex2 */
} MEfPhaseBlock;

typedef struct MEfCornerTable {
    MEfPhaseBlock phases;                    /* +0x00 */
    unsigned char unmodeled_10[0x08];
    unsigned long long corner[2];            /* +0x18: the corner words MEfMakeTranslationMatrix loads */
} MEfCornerTable;

extern const MEfCornerTable MEfVertex2CornerTable;



/* GIF tag fields: EOP is bit 15 of the low word, PRE, PRIM and NREG start at bits 46, 47 and 60. */
#define MEF_GIF_TAG_PRE_SHIFT 46
#define MEF_GIF_TAG_PRIM_SHIFT 47
#define MEF_GIF_TAG_NREG_SHIFT 60

/* Height of the raised origin (MEF_POINT_ACTOR_RAISED) above the actor. */
#define MEF_ACTOR_RAISED_HEIGHT 1.5f

/* sqrt(2), the diagonal factor of a square's half side. */
#define MEF_SQRT2 1.41421356f

/* Frames a wait step runs before MSeObjRequestFree (MEfWaitStep). */
#define MEF_WAIT_FRAMES 31

/* Steps of a phase-flag sweep (MEfSetPhaseFlags): from step MEF_PHASE_STEPS on, the flags stay clear. */
#define MEF_PHASE_STEPS 30

/* Points on the path curve (mefPathCurve) and its key walk size in bytes (two walks of 25 Vector4). */
#define MEF_CURVE_POINTS 4
#define MEF_KEY_WALK_BYTES 0x320

/* Bits of MEfStepRecord.flags_history[0]: the phase is active, starting or ending. */
typedef enum MEfPhaseFlag {
    MEF_PHASE_ACTIVE = 0x01,
    MEF_PHASE_START = 0x10,
    MEF_PHASE_END = 0x20
} MEfPhaseFlag;


/* Writes a GIF tag for regCount registers and returns its first data entry. */
static inline MEfGsQword *OpenTag(MGsPacket *packet, int primEnable, int primitive, int regCount, u64 registers)
{
    MEfGsQword *entry = packet->current;

    entry->gif.tag = MEF_GIF_TAG_EOP | ((u64) primEnable << MEF_GIF_TAG_PRE_SHIFT) | ((u64) primitive << MEF_GIF_TAG_PRIM_SHIFT) | ((u64) regCount << MEF_GIF_TAG_NREG_SHIFT);
    entry->gif.registers = registers;
    packet->regCount = regCount;
    return entry + 1;
}

/* Writes one A+D register setting. */
static inline void SetRegister(MEfGsQword *entry, unsigned char address, u64 data)
{
    entry->ad.data = data;
    entry->ad.address = address;
}

/* Stores NLOOP into the open GIF tag and moves past its data; an empty tag stays open. */
static inline void CloseTag(MGsPacket *packet, int nloop)
{
    if (nloop > 0) {
        int entryCount = packet->regCount * nloop + 1;

        packet->current->nloop |= nloop;
        packet->current += entryCount;
        packet->count += entryCount;
    }
}

/* Colour templates (two RGBAQ quadwords each) and the per-strand edge vectors read by MEfDrawStepTrail. */
extern const MEfGsColor mefStepHeadColors[2];
extern const MEfGsColor mefStepTailColors[2];
extern const DoublewordAlignedVector4 mefStepBodyInnerEdgeVectors[4];
extern const DoublewordAlignedVector4 mefStepBodyOuterEdgeVectors[4];
extern const DoublewordAlignedVector4 mefStepEndInnerEdgeVectors[4];
extern const DoublewordAlignedVector4 mefStepEndOuterEdgeVectors[4];

/*
 * The object the step functions below receive. Offsets follow their loads and
 * stores: the phase-flags, update and advance callbacks at +0x08, +0x10 and
 * +0x14, and the step record at +0x20.
 */
typedef struct MEfStepRecord {
    unsigned char unmodeled_00[0x04];
    struct XenoActor *actor;                            /* +0x04 */
    u32 coordinate;                          /* +0x08 */
    unsigned char unmodeled_0c[0x20 - 0x0c];
    Vector4 vector;                          /* +0x20 */
    unsigned char unmodeled_30[0x40 - 0x30];
    MGsPacket packet;                        /* +0x40 (object +0x60): MGsGPInit / MGsGPTerm */
    short step;                              /* +0x54 (object +0x74) */
    unsigned char unmodeled_56[0x60 - 0x56];
    Vector4 origin;                          /* +0x60: matrix row 3 with w = 1 */
    union {
        int counter;                         /* +0x70 (object +0x90): the wait step's frame count */
        Matrix4 matrix;                      /* +0x70: the step's matrix, MEfStartFromActor/MEfSampleHermitePath */
    };
    float angle;                             /* +0xb0 */
    unsigned short flags_history[25];        /* +0xb4 (object +0xd4): [0] is the flags halfword, [1..24] its history */
    unsigned char unmodeled_e6[0xf0 - 0xe6];
    union {
        Vector4 path[4][25];
        RgVector path_rows[4][25];
    };                     /* +0xf0: rows 0 and 1 the two key walks, rows 2 and 3 the sampled curve */
} MEfStepRecord;

typedef struct MEfStepObject {
    unsigned char unmodeled_00[0x08];
    void (*flags_callback)();
    unsigned char unmodeled_0c[0x04];
    void (*update_callback)();
    void (*advance_callback)();
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfStepRecord record;                    /* +0x20 */
} MEfStepObject;

/*
 * Record of the path step (the same object +0x20 slot as MEfStepRecord).
 * The path step keeps a matrix at +0x70 where the wait step keeps its
 * counter, so it has its own record type.
 */
typedef struct MEfPathRecord {
    unsigned char unmodeled_00[0x04];
    struct XenoActor *actor;                            /* +0x04 */
    u32 coordinate;                          /* +0x08 */
    unsigned char unmodeled_0c[0x20 - 0x0c];
    Vector4 offset;                          /* +0x20: MMathApplyMatrix input */
    unsigned char unmodeled_30[0x40 - 0x30];
    MGsPacket packet;                        /* +0x40: MGsGPInit / MGsGPTerm */
    short step;                              /* +0x54 */
    unsigned char unmodeled_56[0x60 - 0x56];
    Vector4 origin;                          /* +0x60: matrix row 3 with w = 1 */
    Matrix4 matrix;                          /* +0x70 */
    float angle;                             /* +0xb0 */
    unsigned short history[19];              /* +0xb4 */
    unsigned char unmodeled_da[0xe0 - 0xda];
    Vector4 track[4][19];                    /* +0xe0: rows 0-1 sampled points, rows 2-3 their angles */
} MEfPathRecord;

void *memcpy(void *, const void *, unsigned int);

void MEfWaitPhaseFlagsDummy(void);
void MEfWaitUpdate(void);
void MEfWaitStep(MEfStepObject *object);

void *memset(void *s, int c, unsigned int n);
void MEfStartFromActor(MEfStepRecord *record);
void MEfSampleHermitePath(MEfStepRecord *record);
void MEfSetPhaseFlags(MEfStepObject *object);
void MEfAdvanceStep(MEfStepObject *object);
void MEfDrawStepTrail(MEfStepObject *object);


void MEfStartPathFromActor(MEfPathRecord *record);
void MEfSamplePathTrack(MEfPathRecord *record);

/* Vu0Sin/Vu0Cos with the COP1 hazard nop written into the block (the original schedules nothing into that slot). */
#define MEfPathSin(result, angle) \
    __asm__ __volatile__( \
        "mfc1 $8, %1\n" \
        "qmtc2 $8, vf4\n" \
        "vcallms 0x20\n" \
        "qmfc2.i $8, vf1\n" \
        "mtc1 $8, %0\n" \
        "nop\n" \
        : "=f" (result) \
        : "f" (angle) \
        : "$8")

#define MEfPathCos(result, angle) \
    __asm__ __volatile__( \
        "mfc1 $8, %1\n" \
        "qmtc2 $8, vf4\n" \
        "vcallms 0xe8\n" \
        "qmfc2.i $8, vf1\n" \
        "mtc1 $8, %0\n" \
        "nop\n" \
        : "=f" (result) \
        : "f" (angle) \
        : "$8")

#define QuadwordCopyT0Dst(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%6)\n" \
        "sq $8, 0(%5)\n" \
        : "=m" (((float *) (destination))[0]), "=m" (((float *) (destination))[1]), "=m" (((float *) (destination))[2]), "=m" (((float *) (destination))[3]), "=m" (((float *) (destination))[3]) \
        : "r" (destination), "r" (source) \
        : "$8", "memory")

Matrix4 *MSeGetActorMatrix(Matrix4 *destination, struct XenoActor *actor, u32 coordinate)
{
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (destination), "r" (MMathUnitMatrix)
        : "memory");
    if (actor == 0) {
        MOutputDebugStringWarn("MSeGetActorMatrix: actor is NULL");
        return destination;
    }
    switch (coordinate) {
    case MEF_POINT_ACTOR:
        MMathRotateMatrixY(destination, destination, actor->rotation.y);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%1)\n"
            "vmove.w $vf1w, $vf0w\n"
            "sqc2 $vf1, 48(%0)\n"
            :
            : "r" (destination), "r" (&actor->position)
            : "memory");
        return destination;
    case MEF_POINT_ACTOR_RAISED:
        MMathRotateMatrixY(destination, destination, actor->rotation.y);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%1)\n"
            "vmove.w $vf1w, $vf0w\n"
            "sqc2 $vf1, 48(%0)\n"
            :
            : "r" (destination), "r" (&actor->position)
            : "memory");
        (*destination)[3][1] = MEF_ACTOR_RAISED_HEIGHT;
        break;
    default:
        if (sefActorMulLocalMatrix(actor, coordinate, destination, 1) == 0) {
            MMathRotateMatrixY(destination, destination, actor->rotation.y);
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%1)\n"
                "vmove.w $vf1w, $vf0w\n"
                "sqc2 $vf1, 48(%0)\n"
                :
                : "r" (destination), "r" (&actor->position)
                : "memory");
            MOutputDebugStringWarn("MSeGetActorMatrix: Invalid point (%d)", coordinate);
        }
        break;
    }
    return destination;
}

/*
 * MEfGetActorCoord: the translation row of MSeGetActorMatrix, with w = 1.
 */
Vector4 *MEfGetActorCoord(Vector4 *destination, struct XenoActor *actor, u32 coordinate)
{
    Matrix4 actorMatrix;

    MSeGetActorMatrix(&actorMatrix, actor, coordinate);
    __asm__ __volatile__(
        "lqc2 $vf1,48(%1)\n\t"
        "vmove.w $vf1w,$vf0w\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        :
        : "r"(destination), "r"(actorMatrix)
        : "memory"
    );
    return destination;
}

Vector4 *MEfMakeRandomVector(Vector4 *destination)
{
    destination->x = MMathMakeRandomPI();
    destination->y = MMathMakeRandomPI();
    destination->z = MMathMakeRandomPI();
    destination->w = 1.0f;
    return destination;
}

Vector4 *MEfScatterXY(Vector4 *destination, Vector4 *target, Vector4 *source, float spread)
{
    Vector4 angles;
    Matrix4 rotation;
    Vector4 cosSin[2];  /* [0].x = cosine, [1].x = sine: the VU0 0xe8 and 0x20 microprogram results */
    float randomAngle;

    MMathCalcAngle(&angles, source, target);
    MMathRotateMatrixYX(&rotation, 0, &angles);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 48(%0)\n"
        :
        : "r" (&rotation), "r" (source)
        : "memory");
    randomAngle = MMathMakeRandom2PI();
    spread *= MMathMakeRandom();
    __asm__ __volatile__(
        "sqc2 $vf0, 0(%0)\n"
        :
        : "r" (&angles)
        : "memory");
    __asm__ __volatile__(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vcallms 0xe8\n"
        "qmfc2.i $8, $vf1\n"
        "mtc1 $8, %0\n"
        "nop\n"
        : "=f" (cosSin[0].x)
        : "f" (randomAngle)
        : "$8");
    angles.x = spread * cosSin[0].x;
    __asm__ __volatile__(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vcallms 0x20\n"
        "qmfc2.i $8, $vf1\n"
        "mtc1 $8, %0\n"
        "nop\n"
        : "=f" (cosSin[1].x)
        : "f" (randomAngle)
        : "$8");
    angles.y = spread * cosSin[1].x;
    MMathApplyMatrix(destination, &rotation, &angles);
    return destination;
}

int MEfMake4Vertex(Vector4 *destination, const Vector4 *source, float halfSize)
{
    MEfCameraParams *camera;
    Matrix4 basis;
    Vector4 offset;
    Vector4 *vertex;
    int rejected;
    int i;

    camera = &MEfCamParams;
    rejected = 0;
    __asm__ __volatile__(
        "lq $8, 0(%4)\n"
        "lq $9, 16(%4)\n"
        "lq $10, 32(%4)\n"
        "lq $11, 48(%4)\n"
        "sq $8, %0\n"
        "sq $9, %1\n"
        "sq $10, %2\n"
        "sq $11, %3\n"
        : "=m" (basis[0][0]), "=m" (basis[1][0]), "=m" (basis[2][0]), "=m" (basis[3][0])
        : "r" (&camera->rotationMatrix)
        : "$8", "$9", "$10", "$11", "memory");
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, %0\n"
        : "=m" (basis[3][0])
        : "r" (source)
        : "memory");
    vertex = destination;
    for (i = 0; i < 4; i++) {
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%0)\n"
            :
            : "r" (&offset)
            : "memory");
        offset.x = (i & 2) ? -halfSize : halfSize;
        offset.y = (i & 1) ? -halfSize : halfSize;
        MMathApplyMatrix(&offset, &basis, &offset);
        if (MMathRotTransPersClip(vertex, camera->camera, &camera->viewMatrix, &offset)) {
            rejected++;
        }
        vertex++;
    }
    return rejected;
}

int MEfMake4Vertex2(Vector4 *destination, const Vector4 *source, float halfSize, float angleBase)
{
    MEfCameraParams *camera;
    MEfPhaseBlock phaseCopy;
    Matrix4 basis;
    Vector4 offset;
    Vector4 cosSin[2];  /* [0].x = cosine, [1].x = sine: the VU0 0xe8 and 0x20 microprogram results */
    float cornerAngle;
    int rejected;
    int i;
    Vector4 *vertex;
    Matrix4 *viewMatrix;

    camera = &MEfCamParams;
    rejected = 0;
    phaseCopy = MEfVertex2CornerTable.phases;
    QuadwordCopyMatrixT0(basis, camera->rotationMatrix);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%5)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 48(%4)\n"
        : "=m" (basis[3][0]), "=m" (basis[3][1]), "=m" (basis[3][2]), "=m" (basis[3][3])
        : "r" (basis), "r" (source)
        : "memory");
    vertex = destination;
    viewMatrix = &camera->viewMatrix;
    /* The corners sit on the diagonal: their radius is the half side times sqrt(2). */
    halfSize *= MEF_SQRT2;
    for (i = 0; i <= 3; i++) {
        cornerAngle = angleBase + phaseCopy.cornerAngles[i];
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%0)\n"
            :
            : "r" (&offset)
            : "memory");
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0xe8\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            "nop\n"
            : "=f" (cosSin[0].x)
            : "f" (cornerAngle)
            : "$8");
        offset.x = halfSize * cosSin[0].x;
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0x20\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            "nop\n"
            : "=f" (cosSin[1].x)
            : "f" (cornerAngle)
            : "$8");
        offset.y = halfSize * cosSin[1].x;
        MMathApplyMatrix(&offset, &basis, &offset);
        if (MMathRotTransPersClip(vertex, camera->camera, viewMatrix, &offset)) {
            rejected++;
        }
        vertex++;
    }
    return rejected;
}

int MEfCreateWait(MEfStepObject *object)
{
    object->flags_callback = MEfWaitPhaseFlagsDummy;
    object->update_callback = MEfWaitUpdate;
    object->record.counter = 0;
    object->advance_callback = MEfWaitStep;
    return 1;
}

void MEfWaitPhaseFlagsDummy(void)
{
}

Matrix4 *MEfMakeTranslationMatrix(Matrix4 *destination)
{
    const unsigned long long *cornerWords = MEfVertex2CornerTable.corner;
    DoublewordVector4 translation;

    translation.words[0] = cornerWords[0];
    translation.words[1] = cornerWords[1];
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "lqc2 $vf2, 32(%2)\n"
        "lqc2 $vf3, 16(%2)\n"
        "lqc2 $vf4, 0(%2)\n"
        "sqc2 $vf1, 48(%0)\n"
        "sqc2 $vf2, 32(%0)\n"
        "sqc2 $vf3, 16(%0)\n"
        "sqc2 $vf4, 0(%0)\n"
        :
        : "r" (destination), "r" (&translation), "r" (MMathUnitMatrix)
        : "memory");
    return destination;
}

void MEfWaitUpdate(void)
{
    Matrix4 scratch;
    DoublewordVector4 cornerVector;
    const unsigned long long *followingCornerWords;
    int count;

    followingCornerWords = MEfVertex2CornerTable.corner + 2;
    cornerVector.words[0] = followingCornerWords[0];
    cornerVector.words[1] = followingCornerWords[1];
    for (count = 0x14050; count != 0; count--) {
        MMathQuatToMatrix_00272EC8(scratch, &cornerVector.vector);
    }
}

void MEfWaitStep(MEfStepObject *object)
{
    MEfStepRecord *record;

    record = &object->record;
    record->counter = record->counter + 1;
    if (record->counter >= MEF_WAIT_FRAMES) {
        MSeObjRequestFree(object);
    }
}

int MEfCreateHermitePath(MEfStepObject *object)
{
    MEfStepRecord *record;
    Vector4 *keyWalks;

    record = &object->record;
    keyWalks = object->record.path[0];
    record->step = 0;
    MEfStartFromActor(record);
    memset(record->flags_history, 0, sizeof(record->flags_history));
    memset(keyWalks, 0, MEF_KEY_WALK_BYTES);
    __asm__ __volatile__(
        "lq $8, 0(%4)\n"
        "sq $8, 0(%5)\n"
        : "=m" (keyWalks->x), "=m" (keyWalks->y), "=m" (keyWalks->z), "=m" (keyWalks->w)
        : "r" (&object->record.origin), "r" (keyWalks)
        : "$8", "memory");
    QuadwordCopyT0(object->record.path[1], &object->record.origin);
    MEfSampleHermitePath(record);
    MGsGPInit(&object->record.packet, 0, 0);
    object->flags_callback = MEfSetPhaseFlags;
    object->update_callback = MEfDrawStepTrail;
    object->advance_callback = MEfAdvanceStep;
    return 1;
}

void MEfStartFromActor(MEfStepRecord *record)
{
    /* Corner offset tables, defined (const) at the end of this file. */
    extern float mefStepCornerRadiusTable[4];
    extern float mefStepCornerDepthTable[4];
    MEfPathCurve *curve;
    Matrix4 matrix;
    Vector4 corner;
    Vector4 lanes[2];
    float angle;
    float randomScale;
    float angleStep;
    int i;

    curve = &mefPathCurve.curve;
    MSeGetActorMatrix(&matrix, record->actor, record->coordinate);
    MMathApplyMatrix(&corner, &matrix, &record->vector);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 48(%0)\n"
        :
        : "r" (matrix), "r" (&corner)
        : "memory");
    QuadwordCopyMatrixT0(record->matrix, matrix);
    __asm__ __volatile__(
        "lqc2 $vf1, 48(%2)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 0(%1)\n"
        : "=m" (record->origin)
        : "r" (&record->origin), "r" (matrix)
        : "memory");
    do {
        angle = MMathMakeRandom2PI();
    } while (!(angle < 3.83972430229187f) && !(5.585053443908691f < angle));
    record->angle = angle;
    angleStep = 0.10471976f;
    if ((xglSRand() & 1) == 0) {
        angleStep = -0.10471976f;
    }
    randomScale = MMathMakeRandom() + 0.05f;
    MEfHermitePathInit(curve, MEF_CURVE_POINTS);
    for (i = 0; i < MEF_CURVE_POINTS; i++) {
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%0)\n"
            :
            : "r" (&corner), "r" (&corner)
            : "memory");
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0xe8\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            "nop\n"
            : "=f" (lanes[0].x)
            : "f" (angle)
            : "$8");
        corner.x = lanes[0].x * mefStepCornerRadiusTable[i] * randomScale;
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0x20\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            "nop\n"
            : "=f" (lanes[1].x)
            : "f" (angle)
            : "$8");
        corner.y = lanes[1].x * mefStepCornerRadiusTable[i] * randomScale;
        corner.z = mefStepCornerDepthTable[i];
        angle += angleStep;
        MMathApplyMatrix(&curve->points[i], &matrix, &corner);
    }
}

void MEfSampleHermitePath(MEfStepRecord *record)
{
    MEfPathCurve *curve;
    MEfStepRecord *rec;
    Matrix4 matrix;
    Vector4 hermitePoint;
    Vector4 corner;
    Vector4 lanes[2];
    float angle;
    float sineValue;
    float cosineValue;
    float weightSpeed;
    float weight;
    float negWeight;
    int i;
    Vector4 *recordCell;
    int segment;
    int segmentStep;
    int k;

    curve = &mefPathCurve.curve;
    rec = record;
    weight = 0.0f;
    weightSpeed = 0.5f;
    angle = MMathMakeRandom2PI();
    __asm__ __volatile__(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vcallms 0x20\n"
        "qmfc2.i $8, $vf1\n"
        "mtc1 $8, %0\n"
        "nop\n"
        : "=f" (sineValue)
        : "f" (angle)
        : "$8");
    lanes[0].x = sineValue;
    __asm__ __volatile__(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vcallms 0xe8\n"
        "qmfc2.i $8, $vf1\n"
        "mtc1 $8, %0\n"
        "nop\n"
        : "=f" (cosineValue)
        : "f" (angle)
        : "$8");
    lanes[1].x = cosineValue;
    QuadwordCopyMatrixT0(matrix, record->matrix);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 48(%0)\n"
        :
        : "r" (matrix), "r" (&MMathUnitVector)
        : "memory");

    for (i = 0; i < 25; i++) {
        recordCell = (Vector4 *)rec + i;
        segment = i / 8;
        segmentStep = i - segment * 8;
        if (segment == 3) {
            segment = 2;
            segmentStep = 8;
        }
        MMathCalcHermiteKeyPrm(curve, segment);
        MMathCalcHermiteKey(&hermitePoint, curve, segment, (float)segmentStep * 0.125f);
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%4)\n"
            : "=m" (corner.x), "=m" (corner.y), "=m" (corner.z), "=m" (corner.w)
            : "r" (&corner)
            : "memory");
        corner.x = cosineValue * weight;
        corner.y = sineValue * weight;
        MMathApplyMatrix(&corner, &matrix, &corner);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%4)\n"
            "lqc2 $vf2, 0(%5)\n"
            "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n"
            "sqc2 $vf1, 0(%5)\n"
            : "=m" (corner.x), "=m" (corner.y), "=m" (corner.z), "=m" (corner.w)
            : "r" (&hermitePoint), "r" (&corner)
            : "memory");
        __asm__ __volatile__(
            "lq $8, 0(%3)\n"
            "sq $8, 0(%4)\n"
            : "=m" (recordCell[15].y), "=m" (recordCell[15].z), "=m" (recordCell[15].w)
            : "r" (&corner), "r" (&recordCell[15])
            : "$8", "memory");
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%4)\n"
            : "=m" (corner.x), "=m" (corner.y), "=m" (corner.z), "=m" (corner.w)
            : "r" (&corner)
            : "memory");
        negWeight = -weight;
        corner.x = cosineValue * negWeight;
        corner.y = sineValue * negWeight;
        MMathApplyMatrix(&corner, &matrix, &corner);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%4)\n"
            "lqc2 $vf2, 0(%5)\n"
            "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n"
            "sqc2 $vf1, 0(%5)\n"
            : "=m" (corner.x), "=m" (corner.y), "=m" (corner.z), "=m" (corner.w)
            : "r" (&hermitePoint), "r" (&corner)
            : "memory");
        __asm__ __volatile__(
            "lq $8, 0(%4)\n"
            "sq $8, 0(%5)\n"
            : "=m" (recordCell[40].x), "=m" (recordCell[40].y), "=m" (recordCell[40].z), "=m" (recordCell[40].w)
            : "r" (&corner), "r" (&recordCell[40])
            : "$8", "memory");
        if (i >= 11) {
            weightSpeed *= 0.85f;
            weight += weightSpeed;
        }
    }

    for (k = 0; k < 2; k++) {
        for (i = 0; i < 24; i++) {
            MMathCalcAngle(&rec->path[k + 2][i], &rec->path[k][i], &rec->path[k][i + 1]);
            rec->path_rows[k + 2][i][2] = rec->angle;
        }
        QuadwordCopyT0(&rec->path[k + 2][24], &rec->path[k + 2][23]);
    }
}

void MEfSetPhaseFlags(MEfStepObject *object)
{
    MEfStepRecord *record;
    short step;

    record = &object->record;
    if (ov01EffectPauseGet() == 0) {
        step = record->step;
        record->flags_history[0] = 0U;
        if (step < MEF_PHASE_STEPS) {
            record->flags_history[0] = MEF_PHASE_ACTIVE;
            if (step == 0) {
                record->flags_history[0] = MEF_PHASE_START | MEF_PHASE_ACTIVE;
            }
            if (step == MEF_PHASE_STEPS - 1) {
                record->flags_history[0] |= MEF_PHASE_END;
            }
        }
    }
}

/* Copies a vector through $8; the four lanes of the destination are the outputs. */
#define QuadwordCopyT0Lanes(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%5)\n" \
        "sq $8, 0(%4)\n" \
        : "=m" ((destination)->x), "=m" ((destination)->y), "=m" ((destination)->z), "=m" ((destination)->w) \
        : "r" (destination), "r" (source) \
        : "$8")

/* Zeroes a vector from vf0; the four lanes of the destination are the outputs. */
#define Vu0SetOriginLanes(destination) \
    __asm__ __volatile__( \
        "sqc2 vf0, 0(%4)\n" \
        : "=m" ((destination)->x), "=m" ((destination)->y), "=m" ((destination)->z), "=m" ((destination)->w) \
        : "r" (destination))

/* Copies a point into the translation row of a matrix and sets its w to 1. */
#define MEfSetTranslationRow(matrix, source) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%5)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, 48(%4)\n" \
        : "=m" ((matrix)[3][0]), "=m" ((matrix)[3][1]), "=m" ((matrix)[3][2]), "=m" ((matrix)[3][3]) \
        : "r" (matrix), "r" (source))

/*
 * Draws the stepped trail as GS triangle strips. Each visible step gives a pair of
 * vertices: the strand's two edge vectors rotated by the step's angles, moved to the
 * step's point and projected; a clipped vertex restarts the strip (ADC on the next two).
 */
void MEfDrawStepTrail(MEfStepObject *object)
{
    AlignedVector4 edge[2];
    AlignedVector4 corner;
    Matrix4 matrix;
    MEfGsVertex top;
    MEfGsVertex bottom;
    MEfGsColor colour[2];
    MEfStepRecord *record;
    struct XglCamera *camera;
    Matrix4 *viewMatrix;
    MEfGsQword *cursor;
    MEfGsQword *stepQuads;
    int stripLength;
    int strand;
    int row;
    MGsPacket *packet;
    MEfGsQword *header;
    int cullLeft;
    int step;
    int sample;
    int fade;

    packet = &object->record.packet;
    camera = MEfCamParams.camera;
    viewMatrix = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);
    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    cursor = header + 1;
    SetRegister(cursor, MEF_GS_TEST_1, MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);
    record = &object->record;

    /* The head of the trail: one strip per strand, fading with the step. */
    for (strand = 0; strand < 4; strand++) {
        stripLength = 0;
        cullLeft = 0;
        cursor = OpenTag(&record->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
        for (step = 0; step < 12; step++) {
            if ((short) record->flags_history[step] == 0) {
                continue;
            }
            fade = 11 - step;
            colour[0].whole = mefStepHeadColors[0].whole;
            colour[1].whole = mefStepHeadColors[1].whole;
            colour[0].rgba[1] += fade * 8;
            colour[1].rgba[1] += fade;
            colour[0].rgba[2] -= fade * 8;
            colour[1].rgba[2] -= fade * 8;
            if ((record->flags_history[step] & (MEF_PHASE_START | MEF_PHASE_END)) != 0) {
                colour[0].rgba[3] = colour[1].rgba[3] = 0;
                Vu0SetOriginLanes(&edge[0]);
                Vu0SetOriginLanes(&edge[1]);
            } else if (step == 11) {
                colour[0].rgba[3] = colour[1].rgba[3] = 0;
                QuadwordCopyT0Lanes(&edge[0], &mefStepEndInnerEdgeVectors[strand]);
                QuadwordCopyT0Lanes(&edge[1], &mefStepEndOuterEdgeVectors[strand]);
            } else {
                QuadwordCopyT0Lanes(&edge[0], &mefStepBodyInnerEdgeVectors[strand]);
                QuadwordCopyT0Lanes(&edge[1], &mefStepBodyOuterEdgeVectors[strand]);
            }
            MMathRotateMatrixYXZ(&matrix, 0, &record->path[2][step]);
            MEfSetTranslationRow(matrix, &record->path[0][step]);
            MMathApplyMatrix(&corner, &matrix, &edge[0]);
            if (MMathRotTransPersClip(&top.vector, camera, viewMatrix, &corner) != 0) {
                cullLeft = 2;
            } else if (cullLeft != 0) {
                cullLeft--;
                top.words[3] = MEF_GS_XYZ_ADC;
            }
            MMathApplyMatrix(&corner, &matrix, &edge[1]);
            if (MMathRotTransPersClip(&bottom.vector, camera, viewMatrix, &corner) != 0) {
                cullLeft = 2;
            } else if (cullLeft != 0) {
                cullLeft--;
                bottom.words[3] = MEF_GS_XYZ_ADC;
            }
            cursor[stripLength * 4 + 0].whole = colour[0].whole;
            cursor[stripLength * 4 + 1].whole = top.whole;
            cursor[stripLength * 4 + 2].whole = colour[1].whole;
            cursor[stripLength * 4 + 3].whole = bottom.whole;
            stripLength++;
        }
        CloseTag(&record->packet, stripLength);
    }

    /* The tail: both rows of the path, with four samples between the points of steps 22 and 23. */
    for (row = 0; row < 2; row++) {
        for (strand = 0; strand < 4; strand++) {
            cursor = OpenTag(&record->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
            cullLeft = 0;
            stripLength = 0;
            for (step = 10; step < 25; step++) {
                if ((short) record->flags_history[step] == 0) {
                    continue;
                }
                colour[1].whole = mefStepTailColors[1].whole;
                colour[0].whole = mefStepTailColors[0].whole;
                colour[1].rgba[1] -= step - 10;
                if ((record->flags_history[step] & (MEF_PHASE_START | MEF_PHASE_END)) != 0) {
                    colour[0].rgba[3] = colour[1].rgba[3] = 0;
                    Vu0SetOriginLanes(&edge[0]);
                    Vu0SetOriginLanes(&edge[1]);
                } else if (step == 10) {
                    colour[0].rgba[3] = colour[1].rgba[3] = 0;
                } else {
                    QuadwordCopyT0Lanes(&edge[0], &mefStepEndInnerEdgeVectors[strand]);
                    QuadwordCopyT0Lanes(&edge[1], &mefStepEndOuterEdgeVectors[strand]);
                }
                MMathRotateMatrixYXZ(&matrix, 0, &record->path[2 + row][step]);
                /* The samples of one step are written from its first pair while the cursor advances. */
                stepQuads = cursor;
                for (sample = 0; sample < 4 && (sample == 0 || (step == 22 || step == 23)); sample++) {
                    MMathVectorInterpolation(&corner, &record->path[row][step], &record->path[row][step + 1], (float) sample * 0.25f);
                    MEfSetTranslationRow(matrix, &corner);
                    MMathApplyMatrix(&corner, &matrix, &edge[0]);
                    if (MMathRotTransPersClip(&top.vector, camera, viewMatrix, &corner) != 0) {
                        cullLeft = 2;
                    } else if (cullLeft != 0) {
                        cullLeft--;
                        top.words[3] = MEF_GS_XYZ_ADC;
                    }
                    MMathApplyMatrix(&corner, &matrix, &edge[1]);
                    if (MMathRotTransPersClip(&bottom.vector, camera, viewMatrix, &corner) != 0) {
                        cullLeft = 2;
                    } else if (cullLeft != 0) {
                        cullLeft--;
                        bottom.words[3] = MEF_GS_XYZ_ADC;
                    }
                    stepQuads[sample * 4 + 0].whole = colour[0].whole;
                    stepQuads[sample * 4 + 1].whole = top.whole;
                    stepQuads[sample * 4 + 2].whole = colour[1].whole;
                    stepQuads[sample * 4 + 3].whole = bottom.whole;
                    cursor += 4;
                    stripLength++;
                }
            }
            CloseTag(&record->packet, stripLength);
        }
    }
    MGsGPTerm(&record->packet);
}

void MEfAdvanceStep(MEfStepObject *object)
{
    MEfStepRecord *record;
    unsigned short *shiftSlot;
    int shiftsLeft;
    int next_step;

    record = &object->record;
    if (ov01EffectPauseGet() != 0) {
        return;
    }
    shiftsLeft = 24;
    shiftSlot = &record->flags_history[24];
    do {
        shiftsLeft -= 1;
        *shiftSlot = shiftSlot[-1];
        shiftSlot -= 1;
    } while (shiftsLeft > 0);
    next_step = (unsigned short) record->step + 1;
    record->step = next_step;
    if ((short) next_step >= 0x37) {
        MSeObjRequestFree(object);
    }
}

/* The path step object: the same callback slots as MEfStepObject, with the path record at +0x20. */
typedef struct MEfPathObject {
    unsigned char unmodeled_00[0x08];
    void (*flags_callback)();
    unsigned char unmodeled_0c[0x04];
    void (*update_callback)();
    void (*advance_callback)();
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfPathRecord record;                    /* +0x20 */
} MEfPathObject;

struct MEfShortStepObject;
void MEfSetPathPhaseFlags(MEfStepObject *object);
void MEfDrawPathStepTrail(MEfPathObject *object);
void MEfAdvanceStepShort(struct MEfShortStepObject *record);

/*
 * Creator of the path step: clears the step and the history, takes the actor
 * frame, clears both sampled rows and starts each at the frame origin, samples
 * the path, then installs the flags, draw and advance callbacks. Returns 1.
 */
int MEfCreatePathStep(MEfPathObject *object)
{
    MEfPathRecord *record;
    Vector4 *trackRows;

    record = &object->record;
    trackRows = object->record.track[0];
    record->step = 0;
    MEfStartPathFromActor(record);
    memset(record->history, 0, sizeof(record->history));
    memset(trackRows, 0, sizeof(record->track[0]) * 2);
    __asm__ __volatile__(
        "lq $8, 0(%4)\n"
        "sq $8, 0(%5)\n"
        : "=m" (trackRows->x), "=m" (trackRows->y), "=m" (trackRows->z), "=m" (trackRows->w)
        : "r" (&object->record.origin), "r" (trackRows)
        : "$8", "memory");
    QuadwordCopyT0(object->record.track[1], &object->record.origin);
    MEfSamplePathTrack(record);
    MGsGPInit(&record->packet, 0, 0);
    object->flags_callback = MEfSetPathPhaseFlags;
    object->update_callback = MEfDrawPathStepTrail;
    object->advance_callback = MEfAdvanceStepShort;
    return 1;
}

/* Path step set-up: samples the actor's matrix, moves the offset point into it, and fills the four curve points on a circle around that point. */
void MEfStartPathFromActor(MEfPathRecord *record)
{
    /* Ring offset tables, defined (const) at the end of this file. */
    extern float mefPathCornerRadiusTable[4];
    extern float mefPathCornerDepthTable[4];
    unsigned short randomBits;
    int i;
    Matrix4 matrix;
    Vector4 point;
    Vector4 cosine;
    Vector4 sine;
    float angleStep;
    Vector4 *from;
    Vector4 *to;
    float angle;
    float scale;
    Vector4 *out;
    MEfPathCurve *curve = &mefPathCurve.curve;

    MSeGetActorMatrix(&matrix, record->actor, record->coordinate);
    MMathApplyMatrix(&point, &matrix, &record->offset);
    __asm__("lqc2 $vf1,0(%1)\n\t"
            "vmove.w $vf1w,$vf0w\n\t"
            "sqc2 $vf1,%0"
            : "=m"(matrix[3])
            : "r"(&point)
            : "memory");
    QuadwordCopyMatrixT0(record->matrix, matrix);
    __asm__("lqc2 $vf1,%3\n\t"
            "vmove.w $vf1w,$vf0w\n\t"
            "sqc2 $vf1,0(%2)"
            : "=m"(record->origin), "=m"(record->origin.w)
            : "r"(&record->origin), "m"(matrix[3][0])
            : "memory");
    angle = MMathMakeRandom2PI();
    angleStep = 0.10471976f;
    record->angle = angle;
    randomBits = xglSRand();
    if ((randomBits & 1) == 0) {
        angleStep = -0.10471976f;
    }
    scale = MMathMakeRandom() + 0.05f;
    MEfHermitePathInit(curve, MEF_CURVE_POINTS);
    for (i = 0; i < MEF_CURVE_POINTS; i++) {
        out = &curve->points[i];
        __asm__ __volatile__("sqc2 vf0, 0(%4)\n"
                             : "=m"(point), "=m"(point.w), "=m"(*out), "=m"(out->w)
                             : "r"(&point)
                             : "memory");
        MEfPathCos(cosine.x, angle);
        point.x = cosine.x * mefPathCornerRadiusTable[i] * scale;
        MEfPathSin(sine.x, angle);
        point.y = sine.x * mefPathCornerRadiusTable[i] * scale;
        point.z = mefPathCornerDepthTable[i];
        MMathApplyMatrix(out, &matrix, &point);
        angle = angle + angleStep;
    }
}

void MEfSamplePathTrack(MEfPathRecord *record)
{
    Matrix4 matrix;
    Vector4 point;
    Vector4 entry;
    Vector4 sine;
    Vector4 cosine;
    float angle;
    float sineValue;
    float cosineValue;
    float distance;
    float spacing;
    Vector4 *from;
    Vector4 *to;
    float sixth;
    float damp;
    int perSegment;
    int i;
    MEfPathCurve *curve;
    int segment;
    int sampleInSegment;
    int row;
    MEfPathRecord *rec;

    rec = record;
    curve = &mefPathCurve.curve;
    angle = MMathMakeRandom2PI();
    distance = 0.0f;
    spacing = 2.0f;
    Vu0Sin(sineValue, angle);
    sine.x = sineValue;
    Vu0Cos(cosineValue, angle);
    cosine.x = cosineValue;
    QuadwordCopyMatrixT0(matrix, rec->matrix);
    __asm__ __volatile__("lui $2,%%hi(MMathUnitVector)\n\t"
            "addiu $2,$2,%%lo(MMathUnitVector)\n\t"
            "lqc2 $vf1,0($2)\n\t"
            "vmove.w $vf1w,$vf0w\n\t"
            "sqc2 $vf1,%0"
            : "=m"(matrix[3])
            :
            : "$2", "memory");
    sixth = 1.0f / 6.0f;
    damp = 0.85f;
    for (i = 0, perSegment = 6; i < 19; i++) {
        segment = i / perSegment;
        sampleInSegment = i % perSegment;
        if (segment == 3) {
            segment = 2;
            sampleInSegment = 6;
        }
        MMathCalcHermiteKeyPrm(curve, segment);
        MMathCalcHermiteKey(&point, curve, segment, (float) sampleInSegment * sixth);
        __asm__ __volatile__("sqc2 $vf0,0(%4)" : "=m"(entry.x), "=m"(entry.y), "=m"(entry.z), "=m"(entry.w) : "r"(&entry) : "memory");
        entry.x = cosineValue * distance;
        entry.y = sineValue * distance;
        MMathApplyMatrix(&entry, &matrix, &entry);
        __asm__ __volatile__("lqc2 $vf1,0(%4)\n\t"
                "lqc2 $vf2,0(%5)\n\t"
                "vadd.xyz $vf1xyz,$vf1xyz,$vf2xyz\n\t"
                "sqc2 $vf1,0(%5)"
                : "=m"(entry.x), "=m"(entry.y), "=m"(entry.z), "=m"(entry.w) : "r"(&point), "r"(&entry) : "memory");
        QuadwordCopyT0Dst(&rec->track[0][i], &entry);
        __asm__ __volatile__("sqc2 $vf0,0(%4)" : "=m"(entry.x), "=m"(entry.y), "=m"(entry.z), "=m"(entry.w) : "r"(&entry) : "memory");
        entry.x = cosineValue * -distance;
        entry.y = sineValue * -distance;
        MMathApplyMatrix(&entry, &matrix, &entry);
        __asm__ __volatile__("lqc2 $vf1,0(%4)\n\t"
                "lqc2 $vf2,0(%5)\n\t"
                "vadd.xyz $vf1xyz,$vf1xyz,$vf2xyz\n\t"
                "sqc2 $vf1,0(%5)"
                : "=m"(entry.x), "=m"(entry.y), "=m"(entry.z), "=m"(entry.w) : "r"(&point), "r"(&entry) : "memory");
        QuadwordCopyT0Dst(&rec->track[1][i], &entry);
        if (i >= 9) {
            spacing = spacing * damp;
            distance = distance + spacing;
        }
    }
    for (row = 0; row < 2; row++) {
        for (i = 0; i < 18; i++) {
            MMathCalcAngle(&rec->track[row + 2][i], &rec->track[row][i], &rec->track[row][i + 1]);
            /* the z lane as a float array element: a .z member store schedules differently */
            ((float *)&rec->track[row + 2][i])[2] = rec->angle;
        }
        QuadwordCopyT0(&rec->track[row + 2][18], &rec->track[row + 2][17]);
    }
}

void MEfSetPathPhaseFlags(MEfStepObject *object)
{
    MEfStepRecord *record;
    short step;

    record = &object->record;
    if (ov01EffectPauseGet() == 0) {
        step = record->step;
        record->flags_history[0] = 0U;
        if (step < MEF_PHASE_STEPS) {
            record->flags_history[0] = MEF_PHASE_ACTIVE;
            if (step == 0) {
                record->flags_history[0] = MEF_PHASE_START | MEF_PHASE_ACTIVE;
            }
            if (step == MEF_PHASE_STEPS - 1) {
                record->flags_history[0] |= MEF_PHASE_END;
            }
        }
    }
}

/* Colour templates and per-strand edge vectors of the path trail (the layout of MEfDrawStepTrail's tables). */
extern const MEfGsColor mefPathHeadColors[2];
extern const MEfGsColor mefPathTailColors[2];
extern const DoublewordAlignedVector4 mefPathBodyInnerEdgeVectors[4];
extern const DoublewordAlignedVector4 mefPathBodyOuterEdgeVectors[4];
extern const DoublewordAlignedVector4 mefPathEndInnerEdgeVectors[4];
extern const DoublewordAlignedVector4 mefPathEndOuterEdgeVectors[4];

/* Draws the path trail the way MEfDrawStepTrail draws the stepped one, over its 19-step record. */
void MEfDrawPathStepTrail(MEfPathObject *object)
{
    AlignedVector4 edge[2];
    AlignedVector4 corner;
    Matrix4 matrix;
    MEfGsVertex top;
    MEfGsVertex bottom;
    MEfGsColor colour[2];
    MEfPathRecord *record;
    struct XglCamera *camera;
    Matrix4 *viewMatrix;
    MEfGsQword *cursor;
    MEfGsQword *stepQuads;
    int stripLength;
    int strand;
    int row;
    MGsPacket *packet;
    MEfGsQword *header;
    int cullLeft;
    int step;
    int sample;
    int fade;

    packet = &object->record.packet;
    camera = MEfCamParams.camera;
    viewMatrix = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);
    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    cursor = header + 1;
    SetRegister(cursor, MEF_GS_TEST_1, MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);
    record = &object->record;

    /* The head of the trail: one strip per strand, fading with the step. */
    for (strand = 0; strand < 4; strand++) {
        stripLength = 0;
        cullLeft = 0;
        cursor = OpenTag(&record->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
        for (step = 0; step < 10; step++) {
            if ((short) record->history[step] == 0) {
                continue;
            }
            fade = 9 - step;
            colour[0].whole = mefPathHeadColors[0].whole;
            colour[1].whole = mefPathHeadColors[1].whole;
            colour[0].rgba[1] += fade * 8;
            colour[1].rgba[1] += fade;
            colour[0].rgba[2] -= fade * 8;
            colour[1].rgba[2] -= fade * 8;
            if ((record->history[step] & (MEF_PHASE_START | MEF_PHASE_END)) != 0) {
                colour[0].rgba[3] = colour[1].rgba[3] = 0;
                Vu0SetOriginLanes(&edge[0]);
                Vu0SetOriginLanes(&edge[1]);
            } else if (step == 9) {
                colour[0].rgba[3] = colour[1].rgba[3] = 0;
                QuadwordCopyT0Lanes(&edge[0], &mefPathEndInnerEdgeVectors[strand]);
                QuadwordCopyT0Lanes(&edge[1], &mefPathEndOuterEdgeVectors[strand]);
            } else {
                QuadwordCopyT0Lanes(&edge[0], &mefPathBodyInnerEdgeVectors[strand]);
                QuadwordCopyT0Lanes(&edge[1], &mefPathBodyOuterEdgeVectors[strand]);
            }
            MMathRotateMatrixYXZ(&matrix, 0, &record->track[2][step]);
            MEfSetTranslationRow(matrix, &record->track[0][step]);
            MMathApplyMatrix(&corner, &matrix, &edge[0]);
            if (MMathRotTransPersClip(&top.vector, camera, viewMatrix, &corner) != 0) {
                cullLeft = 2;
            } else if (cullLeft != 0) {
                cullLeft--;
                top.words[3] = MEF_GS_XYZ_ADC;
            }
            MMathApplyMatrix(&corner, &matrix, &edge[1]);
            if (MMathRotTransPersClip(&bottom.vector, camera, viewMatrix, &corner) != 0) {
                cullLeft = 2;
            } else if (cullLeft != 0) {
                cullLeft--;
                bottom.words[3] = MEF_GS_XYZ_ADC;
            }
            cursor[stripLength * 4 + 0].whole = colour[0].whole;
            cursor[stripLength * 4 + 1].whole = top.whole;
            cursor[stripLength * 4 + 2].whole = colour[1].whole;
            cursor[stripLength * 4 + 3].whole = bottom.whole;
            stripLength++;
        }
        CloseTag(&record->packet, stripLength);
    }

    /* The tail: both rows of the path, with four samples between the points of steps 16 and 17. */
    for (row = 0; row < 2; row++) {
        for (strand = 0; strand < 4; strand++) {
            cursor = OpenTag(&record->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
            cullLeft = 0;
            stripLength = 0;
            for (step = 8; step < 19; step++) {
                if ((short) record->history[step] == 0) {
                    continue;
                }
                colour[1].whole = mefPathTailColors[1].whole;
                colour[0].whole = mefPathTailColors[0].whole;
                colour[1].rgba[1] -= step - 8;
                if ((record->history[step] & (MEF_PHASE_START | MEF_PHASE_END)) != 0) {
                    colour[0].rgba[3] = colour[1].rgba[3] = 0;
                    Vu0SetOriginLanes(&edge[0]);
                    Vu0SetOriginLanes(&edge[1]);
                } else if (step == 8) {
                    colour[0].rgba[3] = colour[1].rgba[3] = 0;
                } else {
                    QuadwordCopyT0Lanes(&edge[0], &mefPathEndInnerEdgeVectors[strand]);
                    QuadwordCopyT0Lanes(&edge[1], &mefPathEndOuterEdgeVectors[strand]);
                }
                MMathRotateMatrixYXZ(&matrix, 0, &record->track[2 + row][step]);
                /* The samples of one step are written from its first pair while the cursor advances. */
                stepQuads = cursor;
                for (sample = 0; sample < 4 && (sample == 0 || (step == 16 || step == 17)); sample++) {
                    MMathVectorInterpolation(&corner, &record->track[row][step], &record->track[row][step + 1], (float) sample * 0.25f);
                    MEfSetTranslationRow(matrix, &corner);
                    MMathApplyMatrix(&corner, &matrix, &edge[0]);
                    if (MMathRotTransPersClip(&top.vector, camera, viewMatrix, &corner) != 0) {
                        cullLeft = 2;
                    } else if (cullLeft != 0) {
                        cullLeft--;
                        top.words[3] = MEF_GS_XYZ_ADC;
                    }
                    MMathApplyMatrix(&corner, &matrix, &edge[1]);
                    if (MMathRotTransPersClip(&bottom.vector, camera, viewMatrix, &corner) != 0) {
                        cullLeft = 2;
                    } else if (cullLeft != 0) {
                        cullLeft--;
                        bottom.words[3] = MEF_GS_XYZ_ADC;
                    }
                    stepQuads[sample * 4 + 0].whole = colour[0].whole;
                    stepQuads[sample * 4 + 1].whole = top.whole;
                    stepQuads[sample * 4 + 2].whole = colour[1].whole;
                    stepQuads[sample * 4 + 3].whole = bottom.whole;
                    cursor += 4;
                    stripLength++;
                }
            }
            CloseTag(&record->packet, stripLength);
        }
    }
    MGsGPTerm(&record->packet);
}


/*
 * The block at +0x20 of the record MEfAdvanceStepShort updates; its step counter sits
 * at +0x54 of the block (+0x74 of the record).
 */
typedef struct MEfShortStepBlock {
    unsigned char unmodeled_00[0x54];
    unsigned short step;                               /* +0x54 */
    unsigned char unmodeled_56[0xb4 - 0x56];
} MEfShortStepBlock;

/*
 * The record MEfAdvanceStepShort updates: a 19-entry history at +0xd4 that the
 * function shifts one entry toward its end.
 */
typedef struct MEfShortStepObject {
    unsigned char unmodeled_00[0x20];
    MEfShortStepBlock block;                                /* +0x20 */
    unsigned short history[19];                        /* +0xd4 */
} MEfShortStepObject;


/*
 * Returns unless ov01EffectPauseGet reports 0. Otherwise shifts history[0..17] up
 * by one entry, advances the step counter, and calls MSeObjRequestFree once the
 * incremented step reaches 0x31.
 */
void MEfAdvanceStepShort(MEfShortStepObject *record)
{
  int shiftIndex;
  short next_step;
  MEfShortStepBlock *block;

  block = &record->block;
  if (ov01EffectPauseGet() != 0) {
    return;
  }
  shiftIndex = 18;
  do {
    record->history[shiftIndex] = record->history[shiftIndex - 1];
    shiftIndex -= 1;
  } while (shiftIndex > 0);
  next_step = (short) (block->step + 1);
  block->step = (unsigned short) (block->step + 1);
  if (next_step >= 0x31) {
    MSeObjRequestFree(record);
  }
}

/* Read-only tables of the step and path trails (original .rodata), in address order. */
/* Corner table of the vertex-2 corners: four corner bearings, then the two corner words. */
const MEfCornerTable MEfVertex2CornerTable = {
    { { 0.0f, 1.5707964f, 4.712389f, 3.1415927f } },
    { 0 },
    { 0x400000003f800000ULL, 0x3f80000040400000ULL },
};

const float mefWaitQuaternion[4] = { -4.0f, 3.0f, -2.0f, 1.0f };
const float mefStepCornerRadiusTable[4] = { 0.0f, 5.0f, 7.0f, 8.0f };
const float mefStepCornerDepthTable[4] = { 0.0f, 1.6f, 8.0f, 20.0f };
const MEfGsColor mefStepHeadColors[2] = { { rgba: { 0x10, 0x30, 0xa0, 0xff } }, { rgba: { 0x00, 0x10, 0x80, 0xff } } };
const DoublewordAlignedVector4 mefStepBodyInnerEdgeVectors[4] = {
    { -0.005f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.005f, 0.0f, 1.0f },
    { 0.005f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.005f, 0.0f, 1.0f }
};
const DoublewordAlignedVector4 mefStepBodyOuterEdgeVectors[4] = {
    { 0.06f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.06f, 0.0f, 1.0f },
    { -0.06f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.06f, 0.0f, 1.0f }
};
const MEfGsColor mefStepTailColors[2] = { { rgba: { 0x10, 0x30, 0xa0, 0xff } }, { rgba: { 0x00, 0x10, 0x80, 0xff } } };
const DoublewordAlignedVector4 mefStepEndInnerEdgeVectors[4] = {
    { -0.003f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.003f, 0.0f, 1.0f },
    { 0.003f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.003f, 0.0f, 1.0f }
};
const DoublewordAlignedVector4 mefStepEndOuterEdgeVectors[4] = {
    { 0.03f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.03f, 0.0f, 1.0f },
    { -0.03f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.03f, 0.0f, 1.0f }
};
const float mefPathCornerRadiusTable[4] = { 0.0f, 30.0f, 42.0f, 48.0f };
const float mefPathCornerDepthTable[4] = { 0.0f, 30.4f, 152.0f, 380.0f };
const MEfGsColor mefPathHeadColors[2] = { { rgba: { 0x10, 0x30, 0xa0, 0xff } }, { rgba: { 0x00, 0x10, 0x80, 0xff } } };
const DoublewordAlignedVector4 mefPathBodyInnerEdgeVectors[4] = {
    { -0.03f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.03f, 0.0f, 1.0f },
    { 0.03f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.03f, 0.0f, 1.0f }
};
const DoublewordAlignedVector4 mefPathBodyOuterEdgeVectors[4] = {
    { 0.32f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.32f, 0.0f, 1.0f },
    { -0.32f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.32f, 0.0f, 1.0f }
};
const MEfGsColor mefPathTailColors[2] = { { rgba: { 0x10, 0x30, 0xa0, 0xff } }, { rgba: { 0x00, 0x10, 0x80, 0xff } } };
const DoublewordAlignedVector4 mefPathEndInnerEdgeVectors[4] = {
    { -0.015f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.015f, 0.0f, 1.0f },
    { 0.015f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.015f, 0.0f, 1.0f }
};
const DoublewordAlignedVector4 mefPathEndOuterEdgeVectors[4] = {
    { 0.17f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.17f, 0.0f, 1.0f },
    { -0.17f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.17f, 0.0f, 1.0f }
};

/* mefPathCurve: 8192 bytes of zero storage, shared with the other path TUs. */
MEfPathCurveObject mefPathCurve = { 0 };
