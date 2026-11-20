#include "common.h"
#include "shared.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/xgl_2.h"

#include "vu0_macro.h"
#include "main/get.h"
#include "main/undulate.h"
#include "main/init_uwamono_sys.h"

#include "main/outer_product.h"
/* Number of MapUnit[] slots (map_2.h's MapUnit table). */
#define MAP_UNIT_COUNT 64


/* MapUnitRecord.flags */
enum {
    MAP_UNIT_NO_DRAW = 0x4,             /* the model is not drawn */
    MAP_UNIT_ACTIVE = 0x10000,          /* the slot is live: updated and collidable */
    MAP_UNIT_LANDED = 0x80000,          /* a falling unit has come to rest (CheckUwamonoHeight) */
    MAP_UNIT_BROKEN = 0x20000,          /* a breakable that has been destroyed */
    MAP_UNIT_SELECTABLE = 0x40000,      /* currently a valid shoot/aim target */
    MAP_UNIT_HIDDEN = 0x100000,         /* excluded from collision (the unit it stands in for is inactive) */
    MAP_UNIT_SAVE_SYMBOL = 0x20000000,  /* save point symbol */
    MAP_UNIT_SHOP_SYMBOL = 0x40000000   /* shop symbol */
};

/* Collision shape of a unit (state byte, crossType, KowareKind shape). */
enum {
    UWAMONO_SHAPE_CIRCLE = 1,
    UWAMONO_SHAPE_BOX = 2
};

/* Record mode: which collision test a record gets (HitCheckMapUnitAt and CrossPointUwamonoAt dispatch on it). */
enum {
    UWAMONO_MODE_BOX = 1,
    UWAMONO_MODE_CIRCLE = 2
};

/* Records of uwamonoRecords the scans visit. */
#define UWAMONO_RECORD_COUNT 0x100
/* Move and turn steps one record holds (the moveX/turnX tables). */
#define UWAMONO_STEP_COUNT 0x20


/* Actors collide against a unit at 1.5 times their radius. */
#define UWAMONO_RADIUS_SCALE 1.5f

typedef struct UwamonoMapSlot UwamonoMapSlot;

typedef struct UwamonoBoxBasis {
    float x;
    unsigned char unmodeled_04[4];
    float z;
} UwamonoBoxBasis;

struct UwamonoMapSlot {
    unsigned int flags;
    void (*update)(void *);
    void (*draw)(void *);
    unsigned char unmodeled_0c[4];
    AlignedVector4 position;
    float rotationX, rotationY, rotationZ;
    unsigned char unmodeled_2c[0x14];
    Matrix4 localMatrix;
    union {
        struct UwamonoBoxBasis *basis;
        Matrix4 *matrixView;
    };
    unsigned char unmodeled_84[0x1e];
    unsigned char actionSub;
    unsigned char linkedUnitIndex;
    short serial;
    unsigned char unmodeled_a6[0x0a];
    AlignedVector4 dimensions;
    unsigned char unmodeled_c0[0x28];
    int effectCf[3];
    unsigned char unmodeled_f4[0xac];
    struct UwamonoMapCollisionState {
        short kindIndex;
        unsigned char unmodeled_02[2];
        signed char signal;
        unsigned char unmodeled_05[0x28];
        signed char crossType;
        signed char hideOverride;
        unsigned char unmodeled_2f[0x131];
    } state;
};

extern UwamonoMapSlot MapUnit[];
extern const float mapUnitPosHitHeightScale;
extern const float mapUnitPosHitBelowMargin;
/* Box corner test of an actor against a unit: the box test decides, and a 2 result also needs the corner test. */ int HitCheckBoxCorner(struct XenoActor *actor, UwamonoRecord *unit);
unsigned int HitCheckMapUnitPosAt(struct XenoActor *query, UwamonoRecord *record);
int HitCheckCorner(struct XenoActor *query, UwamonoRecord *record);
void CheckNearPoint(const Vector4 *origin, const Vector4 *candA,
                    const Vector4 *candB, Vector4 *out);



/* Appends the move step (x, y) to the record's move table; returns 0 when the table is full. */
int uwamonoAddMoveStep(int index, float x, float y)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    int count = record->moveStepCount;

    if (count >= UWAMONO_STEP_COUNT) {
        return 0;
    }
    record->moveX[count] = x;
    record->moveStepCount = record->moveStepCount + 1;
    record->moveY[count] = y;
    return 1;
}

/* Appends the turn step (x, y) to the record's turn table; returns 0 when the table is full. */
int uwamonoAddRotateStep(int index, float x, float y)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    int count = record->turnStepCount;

    if (count >= UWAMONO_STEP_COUNT) {
        return 0;
    }
    record->turnX[count] = x;
    record->turnStepCount = record->turnStepCount + 1;
    record->turnY[count] = y;
    return 1;
}

/* Zeroes the first word of one entry of the move reset table (setStepMove calls this). */
void uwamonoClearMoveResetWord(int index) {
    uwamonoRecords[index].moveStepCount = 0;
}

/* Same as uwamonoClearMoveResetWord for the rotate reset table (setStepRotate calls this). */
void uwamonoClearRotateResetWord(int index) {
    uwamonoRecords[index].turnStepCount = 0;
}

int HitCheckMapUnitAt(struct XenoActor *query, UwamonoRecord *record);

/* Returns the index of the first active kind-2 record that the query hits, or -1. */
int HitCheckMapUnit(struct XenoActor *query) {
    int index;

    for (index = 0; index < UWAMONO_RECORD_COUNT; index++) {
        UwamonoRecord *record = &uwamonoRecords[index];

        if (record->kind == 2 && (record->flags & 2) && record->searchMode != 0
            && HitCheckMapUnitAt(query, record) != 0) {
            return index;
        }
    }
    return -1;
}

int HitCheckNyuru(struct XenoActor *record, UwamonoRecord *other);
int NyuruMatrix(struct XenoActor *record, UwamonoRecord *other, int skipSnap);

/*
 * A saved pose copy. Its stores go through a union member (alias set 0), so
 * they stay ahead of the hit-list loads in the loop, as in the original.
 */
typedef union UwamonoPoseSave {
    AlignedVector4 vector;
} UwamonoPoseSave;

/*
 * Collects the kind-2 records that the actor hits (index list ended by -1), then
 * pushes the actor out of each of them, restoring its pose between the pushes.
 */
int HitCheckMapUnitWithNyuru(struct XenoActor *actor)
{
    int hits[256];
    int hitCount;
    int index;
    int slot;
    UwamonoPoseSave savedPosition;
    UwamonoPoseSave savedAnchor;

    for (index = 0; index < 256; index++) {
        hits[index] = -1;
    }
    hitCount = 0;
    for (index = 0; index < UWAMONO_RECORD_COUNT; index++) {
        UwamonoRecord *other = &uwamonoRecords[index];

        if (other->kind == 2 && !(other->flags & 0x02000000) && other->searchMode != 0
            && HitCheckMapUnitAt(actor, other) != 0) {
            hits[hitCount++] = index;
        }
    }
    if (hitCount == 0) {
        return 0;
    }
    if (hitCount == 1) {
        return HitCheckNyuru(actor, &uwamonoRecords[hits[0]]);
    }
    for (slot = 0; hits[slot] != -1; slot++) {
        UwamonoRecord *other;

        savedPosition.vector = actor->position;
        savedAnchor.vector = actor->velocity;
        other = &uwamonoRecords[hits[slot]];
        switch (other->searchMode) {
        case UWAMONO_MODE_BOX:
            NyuruMatrix(actor, other, 1);
            break;
        case 2:
            NyuruCircle(actor, other);
            break;
        }
        if (HitCheckMapUnit(actor) == -1) {
            return 1;
        }
        actor->position = savedPosition.vector;
        actor->velocity = savedAnchor.vector;
    }
    actor->position.x -= actor->velocity.x;
    actor->position.z -= actor->velocity.z;
    actor->velocity.x = 0.0f;
    actor->velocity.z = 0.0f;
    return 0;
}

/*
 * HitCheckMapUnitPos (main:0x002c02f0): a thin tail call into
 * HitCheckMapUnitPosSize with its distance threshold fixed at 0.0f (a
 * true `j`, no `jal`, so this function's own return value is whatever
 * HitCheckMapUnitPosSize returns).
 */
int HitCheckMapUnitPos(const Vector4 *position) {
    return HitCheckMapUnitPosSize(position, 0.0f);
}

float atan2f(float y, float x);


int HitCheckMapUnitPosSize(const Vector4 *position, float radius)
{
    struct XenoActor query;
    int index;

    query.position = *position;
    query.radius = radius;
    for (index = 0; index < UWAMONO_RECORD_COUNT; index++) {
        UwamonoRecord *record = &uwamonoRecords[index];

        if (record->kind < 2 || record->kind == 7 || (record->flags & 0x2000000) || record->searchMode == 0) {
            continue;
        }
        if (HitCheckMapUnitPosAt(&query, record)) {
            return index;
        }
    }
    return -1;
}

int HitCheckBox(struct XenoActor *query, UwamonoRecord *record);
extern const float uwamonoMapUnitTopScale;

int HitCheckMapUnitAt(struct XenoActor *query, UwamonoRecord *record)
{
    if (record->position.y + record->extent.y * uwamonoMapUnitTopScale < query->position.y) {
        return 0;
    }
    if (query->position.y + query->height < record->position.y) {
        return 0;
    }
    switch (record->searchMode) {
    case UWAMONO_MODE_BOX:
        return HitCheckBox(query, record);
    case 2:
        return Get_Distance(&query->position, &record->position) < query->radius + record->extent.x;
    default:
        return 0;
    }
}

unsigned int HitCheckMapUnitPosAt(struct XenoActor *query, UwamonoRecord *record)
{
    if (record->position.y + record->extent.y * 0.9f < query->position.y) {
        return 0;
    }
    if (query->position.y < record->position.y - 1.8f) {
        return 0;
    }
    if (record->searchMode != UWAMONO_MODE_BOX) {
        if (record->searchMode == UWAMONO_MODE_CIRCLE) {
            if (!(CheckDist2D(&query->position, &record->position) < query->radius + record->extent.x)) {
                return 0;
            }
            return 1;
        }
        return 0;
    }
    return HitCheckBoxCorner(query, record);
}

int uwamonoPointInBox(const Vector4 *from, const Vector4 *position, const Vector4 *extent, float radius, float angle)
{
    Vector4 delta;
    Vector4 rotated;

    xglMatrixStackUnit();
    xglMatrixStackRotY(-angle);
    delta.x = from->x - position->x;
    delta.y = 0.0f;
    delta.z = from->z - position->z;
    delta.w = 1.0f;
    xglMatrixStackMulVector(&rotated, &delta);
    {
        int inside = 1;

        if (__builtin_fabsf(rotated.x) > extent->x + radius || __builtin_fabsf(rotated.z) > extent->z + radius) {
            inside = 0;
        }
        return inside;
    }
}

int HitCheckBox(struct XenoActor *query, UwamonoRecord *record)
{
    DoublewordAlignedVector4 extent;
    float (*basis)[4] = record->matrix;
    Vector4 anchor;
    float deltaX;
    float deltaZ;
    float value;
    float zRadius;
    float major;
    float axisX;
    float axisZ;
    float limit;
    float minor;
    float scaled;

    extent = record->extent;
    axisZ = basis[0][2];
    axisX = basis[0][0];
    deltaX = query->position.x - basis[3][0];
    deltaZ = query->position.z - basis[3][2];
    value = deltaX * axisX + deltaZ * axisZ;
    anchor.x = basis[3][0];
    anchor.y = basis[3][1];
    anchor.z = basis[3][2];
    anchor.w = 1.0f;
    if (extent.x + query->radius < __builtin_fabsf(value)) {
        return 0;
    }
    zRadius = extent.z + query->radius;
    major = query->position.x - (value * axisX + anchor.x);
    minor = query->position.z - (value * axisZ + anchor.z);
    value = major * major + minor * minor;
    limit = zRadius * zRadius;
    if (!(value < limit)) {
        return 0;
    }
    value = __builtin_sqrtf(query->velocity.x * query->velocity.x + query->velocity.z * query->velocity.z)
            * __builtin_sqrtf(deltaX * deltaX + deltaZ * deltaZ);
    major = extent.x;
    minor = extent.z;
    if (extent.x < extent.z) {
        major = extent.z;
        minor = extent.x;
    }
    scaled = (minor / major) * 0.3f;
    limit = 1.0f - scaled;
    if (value > 1e-5f) {
        value = (query->velocity.x * deltaX + query->velocity.z * deltaZ) / value;
    }
    if (!(limit < value)) {
        return 1;
    }
    return 0;
}

/* Box corner test of an actor against a unit: the box test decides, and a 2 result also needs the corner test. */
int HitCheckBoxCorner(struct XenoActor *actor, UwamonoRecord *unit)
{
    int hit = uwamonoPointInBox(&actor->position, &unit->position, &unit->extent, actor->radius, unit->rotation.y);

    if (hit == 0) {
        return 0;
    }
    if (hit == 2) {
        return HitCheckCorner(actor, unit) != 0;
    }
    return 1;
}

float sqrtf(float x);

int HitCheckCorner(struct XenoActor *query, UwamonoRecord *record)
{
    Vector4 corner[4];
    DoublewordAlignedVector4 position;
    DoublewordAlignedVector4 extent;
    float (*transform)[4];

    transform = (*(UwamonoPose **) &record->transform)->matrix;
    position = record->position;
    extent = record->extent;

    corner[0].x = position.x + (extent.x * transform[0][0] + extent.z * transform[2][0]);
    corner[0].y = position.y;
    corner[0].z = position.z + (extent.x * transform[0][2] + extent.z * transform[2][2]);
    if (CheckDist2D(&query->position, &corner[0]) < query->radius) {
        return 1;
    }
    corner[1].x = position.x - (extent.z * transform[0][2] + extent.x * transform[2][2]);
    corner[1].y = position.y;
    corner[1].z = position.z + (extent.z * transform[0][0] + extent.x * transform[2][0]);
    if (CheckDist2D(&query->position, &corner[1]) < query->radius) {
        return 1;
    }
    corner[2].x = position.x - (extent.x * transform[0][0] + extent.z * transform[2][0]);
    corner[2].y = position.y;
    corner[2].z = position.z - (extent.x * transform[0][2] + extent.z * transform[2][2]);
    if (CheckDist2D(&query->position, &corner[2]) < query->radius) {
        return 1;
    }
    corner[3].x = position.x + (extent.z * transform[0][2] + extent.x * transform[2][2]);
    corner[3].y = position.y;
    corner[3].z = position.z - (extent.z * transform[0][0] + extent.x * transform[2][0]);
    if (CheckDist2D(&query->position, &corner[3]) < query->radius) {
        return 1;
    }
    return 0;
}

int NyuruCircle(struct XenoActor *record, UwamonoRecord *other);

int HitCheckNyuru(struct XenoActor *actor, UwamonoRecord *other)
{
    DoublewordAlignedVector4 savedPosition;
    DoublewordAlignedVector4 savedAnchor;
    int hit;

    savedPosition = actor->position;
    savedAnchor = actor->velocity;
    hit = -1;
    switch (other->searchMode) {
    case UWAMONO_MODE_CIRCLE:
        if (NyuruCircle(actor, other) == 0) {
            return 0;
        }
        hit = HitCheckMapUnit(actor);
        break;
    case UWAMONO_MODE_BOX:
        if (NyuruMatrix(actor, other, 0) == 0) {
            return 0;
        }
        hit = HitCheckMapUnit(actor);
        break;
    }
    if (hit != -1) {
    actor->position = savedPosition;
    actor->velocity = savedAnchor;
    actor->position.x = actor->position.x - actor->velocity.x;
    actor->velocity.x = 0.0f;
    actor->position.z = actor->position.z - actor->velocity.z;
    actor->velocity.z = 0.0f;
        return 0;
    }
    return 1;
}

int uwamonoPushOutOfCircle(Vector4 *from, Vector4 *to, const Vector4 *other, float radius)
{
    Vector4 delta;
    float length;
    float angle;
    float sine;
    float cosine;
    float z;

    delta.x = from->x - other->x;
    delta.y = 0.0f;
    delta.z = from->z - other->z;
    xglVectorLength(&length, &delta);
    if (radius <= length) {
        return 1;
    }
    length = radius + 0.01f;
    angle = atan2f(delta.x, delta.z);
    sine = xglSin(angle);
    delta.x = other->x + length * sine;
    cosine = xglCos(angle);
    z = other->z + length * cosine;
    from->x = from->x - to->x;
    delta.z = z;
    from->z = from->z - to->z;
    to->x = delta.x - from->x;
    to->z = z - from->z;
    return 0;
}


/*
 * Circle-shaped record against another record: returns 1 when the other record's
 * circle does not block it; otherwise it lands the record on the ground height
 * UnduCheck reports and returns 1, or 0 when there is no ground.
 */
int NyuruCircle(struct XenoActor *actor, UwamonoRecord *other) {
    float height;

    if (uwamonoPushOutOfCircle(&actor->position, &actor->velocity, &other->position,
                      actor->radius + other->extent.x) != 0) {
        return 1;
    }
    actor->collision.flags |= 0x1000;
    height = UnduCheck(&actor->position, &actor->velocity, &actor->collision);
    if (height == -1000.0f) {
        return 0;
    }
    actor->position.y = height;
    return 1;
}

extern const float nyuruSnapEpsilonX;
extern const float nyuruSnapEpsilonZ;

int NyuruMatrix(struct XenoActor *actor, UwamonoRecord *other, int skipSnap)
{
    DoublewordVector4 extent;
    Vector4 dir;
    UwamonoTransform matrixCopy;
    float (*transform)[4];
    float length;
    float dx;
    float dz;
    float dot;
    float projection;
    float height;

    transform = other->transform->matrix;
    matrixCopy = other->transform->copy;
    extent.words[0] = ((u64 *)&other->extent)[0];
    extent.words[1] = ((u64 *)&other->extent)[1];
    actor->position.x -= actor->velocity.x;
    actor->position.z -= actor->velocity.z;
    dx = actor->position.x - other->position.x;
    dz = actor->position.z - other->position.z;
    dot = dx * transform[0][0] + dz * transform[0][2];
    length = __builtin_sqrtf(actor->velocity.x * actor->velocity.x + actor->velocity.z * actor->velocity.z);
    if (length == 0.0f) {
        return 0;
    }
    dir.x = actor->velocity.x / length;
    dir.y = 0.0f;
    dir.z = actor->velocity.z / length;
    if (__builtin_fabsf(dot) < extent.vector.x + actor->radius) {
        projection = dir.x * transform[0][0] + dir.z * transform[0][2];
        actor->velocity.x = projection * transform[0][0] * length;
        actor->velocity.z = projection * transform[0][2] * length;
        if (__builtin_fabsf(actor->velocity.x) < nyuruSnapEpsilonX) {
            if (__builtin_fabsf(actor->velocity.z) < nyuruSnapEpsilonX && !skipSnap) {
                if (extent.vector.x < dot) {
                    actor->velocity.x = transform[0][0] * length * 0.5f;
                    actor->velocity.z = transform[0][2] * length * 0.5f;
                } else if (dot < -extent.vector.x) {
                    actor->velocity.x = -transform[0][0] * length * 0.5f;
                    actor->velocity.z = -transform[0][2] * length * 0.5f;
                }
            }
        }
    } else {
        projection = dir.x * transform[2][0] + dir.z * transform[2][2];
        actor->velocity.x = projection * transform[2][0] * length;
        actor->velocity.z = projection * transform[2][2] * length;
        if (__builtin_fabsf(actor->velocity.x) < nyuruSnapEpsilonZ) {
            if (__builtin_fabsf(actor->velocity.z) < nyuruSnapEpsilonZ && !skipSnap) {
                dot = dx * transform[2][0] + dz * transform[2][2];
                if (extent.vector.z < dot) {
                    actor->velocity.x = transform[2][0] * length * 0.5f;
                    actor->velocity.z = transform[2][2] * length * 0.5f;
                } else if (dot < -extent.vector.z) {
                    actor->velocity.x = -transform[2][0] * length * 0.5f;
                    actor->velocity.z = -transform[2][2] * length * 0.5f;
                }
            }
        }
    }
    actor->collision.flags |= 0x1000;
    height = UnduCheck(&actor->position, &actor->velocity, &actor->collision);
    if (height == -1000.0f) {
        actor->position.x -= actor->velocity.x;
        actor->position.z -= actor->velocity.z;
        return 0;
    } else {
        actor->position.y = height;
        return 1;
    }
}

int UwamonoAreaCheck(const Vector4 *from, UwamonoRecord *record, float radius);
int CrossPointUwamonoAt(const Vector4 *first, const Vector4 *second, UwamonoRecord *record, AlignedVector4 *third);

int CrossPointUwamono(const Vector4 *origin, const Vector4 *target, float radius, AlignedVector4 *out)
{
    int hitIndex[256];
    AlignedVector4 startLeft;
    AlignedVector4 endLeft;
    AlignedVector4 startRight;
    AlignedVector4 endRight;
    Vector4 dir;
    AlignedVector4 crossPoint;
    UwamonoRecord *record;
    float length;
    float limit;
    float lenSq;
    float dot;
    float sideX;
    float sideZ;
    int count;
    int i;
    int leftSide;
    int rightSide;
    int off;

    dir.x = origin->x - target->x;
    dir.z = origin->z - target->z;
    dir.y = 0.0f;
    dir.w = 1.0f;
    limit = dir.x * dir.x + dir.y + dir.z * dir.z;
    limit = __builtin_sqrtf(limit) + radius;

    for (i = 0; i < 256; i++) {
        hitIndex[i] = -1;
    }

    count = 0;
    for (i = 0; i < UWAMONO_RECORD_COUNT; i++) {
        record = &uwamonoRecords[i];
        if (record->kind != 2 || (record->flags & 0x2) == 0) {
            continue;
        }
        if (__builtin_fabsf(origin->y - record->position.y) > 0.3f) {
            continue;
        }
        if (record->searchMode != 0 && UwamonoAreaCheck(origin, record, limit)) {
            hitIndex[count++] = i;
        }
    }

    out->x = 65535.0f;
    out->y = -1000.0f;
    out->z = 65535.0f;
    if (count == 0) {
        *out = *target;
        return 0;
    }

    xglVectorNormal(&dir, &dir);

    startLeft = *origin;
    endLeft = *target;
    startRight = *origin;
    endRight = *target;
    startLeft.w = 1.0f;
    endLeft.w = 1.0f;
    startRight.w = 1.0f;
    endRight.w = 1.0f;
    sideX = radius * dir.z;
    sideZ = radius * dir.x;
    startLeft.x = startLeft.x + sideX;
    startLeft.z = startLeft.z - sideZ;
    endLeft.x = endLeft.x + sideX;
    endLeft.z = endLeft.z - sideZ;
    startRight.x = startRight.x - sideX;
    startRight.z = startRight.z + sideZ;
    endRight.x = endRight.x - sideX;
    endRight.z = endRight.z + sideZ;

    for (i = 0; i < 256; i++) {
        UwamonoRecord *hitRecord;

        if (hitIndex[i] == -1) {
            break;
        }
        hitRecord = &uwamonoRecords[hitIndex[i]];
        CrossPointUwamonoAt(&startLeft, &endLeft, hitRecord, &crossPoint);
        if (crossPoint.y != -1000.0f) {
            CheckNearPoint(origin, &crossPoint, out, out);
        }
        CrossPointUwamonoAt(&startRight, &endRight, hitRecord, &crossPoint);
        if (crossPoint.y != -1000.0f) {
            CheckNearPoint(origin, &crossPoint, out, out);
        }
        crossPoint.x = origin->x - hitRecord->position.x;
        crossPoint.z = origin->z - hitRecord->position.z;
        if (dir.x * crossPoint.x + dir.z * crossPoint.z > 0.0f) {
            leftSide = CheckPointLine(&startLeft, &endLeft, &hitRecord->position);
            rightSide = CheckPointLine(&startRight, &endRight, &hitRecord->position);
            if (leftSide != rightSide) {
                CrossPointUwamonoAt(origin, &hitRecord->position, hitRecord, &crossPoint);
                CheckNearPoint(origin, &crossPoint, out, out);
            }
        }
    }

    if (out->y == -1000.0f) {
        *out = *target;
        return 0;
    }

    endLeft.x = origin->x - out->x;
    endLeft.y = origin->y;
    endLeft.z = origin->z - out->z;
    limit = dir.x * endLeft.x + dir.z * endLeft.z;
    limit = limit - radius;
    if (limit < 0.0f) {
        *out = *origin;
        return 1;
    }
    out->x = origin->x - limit * dir.x;
    out->y = origin->y;
    out->z = origin->z - limit * dir.z;
    return 1;
}

int CalcNearCrossPointCircle(const Vector4 *origin, const Vector4 *target, UwamonoRecord *record, AlignedVector4 *out);
int CalcNearCrossPointBox(const Vector4 *origin, const Vector4 *target, UwamonoRecord *record, AlignedVector4 *out);

/* Near-crossing calculation for one record, chosen by its mode: 1 = box, 2 = circle; any other mode returns 2. */
int CrossPointUwamonoAt(const Vector4 *first, const Vector4 *second, UwamonoRecord *record, AlignedVector4 *third) {
    if (record->searchMode != UWAMONO_MODE_BOX) {
        if (record->searchMode == UWAMONO_MODE_CIRCLE) {
            return CalcNearCrossPointCircle(first, second, record, third);
        }
        return 2;
    }
    return CalcNearCrossPointBox(first, second, record, third);
}

/* Declared without a prototype: CrossCheckMapUnitAt passes three arguments (a tail jump), the definition takes four. */

int CalcNearCrossPointCircle(const Vector4 *origin, const Vector4 *target, UwamonoRecord *record, AlignedVector4 *out)
{
    AlignedVector4 dir;
    AlignedVector4 normal;
    float length;

    if (CrossCheckMapUnitCircle(origin, target, record, out) != 0) {
        dir = *origin;
        dir.y = record->position.y;
        Vu0SubVectorXYZ(&dir, &record->position, origin);
        xglVectorLength(&length, &dir);
        xglVectorNormal(&normal, &dir);
        length = length - record->extent.x;
        if (length < 0.0f) {
            *out = *origin;
            return;
        }
        out->x = origin->x + length * normal.x;
        out->y = origin->y;
        out->z = origin->z + length * normal.z;
    } else {
        out->y = -1000.0f;
    }
}


int CalcNearCrossPointBox(const Vector4 *origin, const Vector4 *target, UwamonoRecord *record, AlignedVector4 *out)
{
    Vector4 corner[4];
    AlignedVector4 position;
    AlignedVector4 extent;
    AlignedVector4 hit;
    float (*matrix)[4];
    Vector4 *edge1;
    float noCross;

    edge1 = &corner[1];
    noCross = -1000.0f;
    matrix = record->transform->matrix;
    position = record->position;
    extent = record->extent;
    out->y = noCross;
    hit.y = origin->y;

    corner[0].x = position.x + (extent.x * matrix[0][0] + extent.z * matrix[2][0]);
    corner[0].y = position.y;
    corner[0].z = position.z + (extent.x * matrix[0][2] + extent.z * matrix[2][2]);
    corner[1].x = position.x - (extent.z * matrix[0][2] + extent.x * matrix[2][2]);
    corner[1].y = position.y;
    corner[1].z = position.z + (extent.z * matrix[0][0] + extent.x * matrix[2][0]);
    corner[2].x = position.x - (extent.x * matrix[0][0] + extent.z * matrix[2][0]);
    corner[2].y = position.y;
    corner[2].z = position.z - (extent.x * matrix[0][2] + extent.z * matrix[2][2]);
    corner[3].x = position.x + (extent.z * matrix[0][2] + extent.x * matrix[2][2]);
    corner[3].y = position.y;
    corner[3].z = position.z - (extent.z * matrix[0][0] + extent.x * matrix[2][0]);

    if (CheckCrossLine(origin, target, &corner[0], edge1) != 0) {
        CalcCrossPoint(origin, target, &corner[0], edge1, &hit);
        *out = hit;
    }
    if (CheckCrossLine(origin, target, edge1, &corner[2]) != 0) {
        CalcCrossPoint(origin, target, edge1, &corner[2], &hit);
        if (out->y != -1000.0f) {
            CheckNearPoint(origin, &hit, out, out);
        } else {
            *out = hit;
        }
    }
    if (CheckCrossLine(origin, target, &corner[2], &corner[3]) != 0) {
        CalcCrossPoint(origin, target, &corner[2], &corner[3], &hit);
        if (out->y != -1000.0f) {
            CheckNearPoint(origin, &hit, out, out);
        } else {
            *out = hit;
        }
    }
    if (CheckCrossLine(origin, target, &corner[3], &corner[0]) != 0) {
        CalcCrossPoint(origin, target, &corner[3], &corner[0], &hit);
        if (out->y != -1000.0f) {
            CheckNearPoint(origin, &hit, out, out);
        } else {
            *out = hit;
        }
    }
}

void CheckNearPoint(const Vector4 *origin, const Vector4 *candA, const Vector4 *candB, Vector4 *out)
{
    Vector4 delta;
    float lenA;
    float lenB;

    Vu0SubVectorXYZ(&delta, origin, candA);
    xglVectorLength(&lenA, &delta);

    Vu0SubVectorXYZ(&delta, origin, candB);
    xglVectorLength(&lenB, &delta);

    if (lenA < lenB)
        *out = *candA;
    else
        *out = *candB;
}

int AreaCheckBox(const Vector4 *from, UwamonoRecord *record, float radius);
/* Area test of a record against a point: the height band of the record, then its box or circle. */
extern float uwamonoAreaTopScale;
extern float uwamonoAreaBottomMargin;

int UwamonoAreaCheck(const Vector4 *from, UwamonoRecord *record, float radius)
{
    if (record->position.y + record->extent.y * uwamonoAreaTopScale < from->y) {
        return 0;
    }
    if (from->y < record->position.y - uwamonoAreaBottomMargin) {
        return 0;
    }
    if (record->searchMode != UWAMONO_MODE_BOX) {
        if (record->searchMode == UWAMONO_MODE_CIRCLE) {
            if (CheckDist2D(from, &record->position) < radius + record->extent.x) {
                return 1;
            }
        }
    } else {
        if (AreaCheckBox(from, record, radius)) {
            return 1;
        }
    }
    return 0;
}

/* Box area test of a record (the record's extent and anchor height) against a point with a radius. */
int AreaCheckBox(const Vector4 *from, UwamonoRecord *record, float radius) {
    return uwamonoPointInBox(from, &record->position, &record->extent, radius, record->rotation.y) != 0;
}

/* Tests one record against the check point: far in y is a miss; mode 1 is a box, 2 a circle, other modes never hit. */ int CrossCheckMapUnitAt(const AlignedVector4 *checkPosition, const AlignedVector4 *exclude, UwamonoRecord *record);

int CrossCheckMapUnitAim(const AlignedVector4 *checkPosition, UwamonoRecord *exclude)
{
    UwamonoRecord *record;
    int index;

    record = uwamonoRecords;
    for (index = 0; index < UWAMONO_RECORD_COUNT; index++) {
        if (record->kind == 2 && !(record->flags & 0x02000000) && record != exclude && record->searchMode != 0
            && CrossCheckMapUnitAt(checkPosition, &exclude->position, record) != 0) {
            return index;
        }
        record++;
    }
    return -1;
}

int CrossCheckMapUnitBox(const Vector4 *origin, const Vector4 *target, UwamonoRecord *record);

/* Tests one record against the check point: far in y is a miss; mode 1 is a box, 2 a circle, other modes never hit. */
int CrossCheckMapUnitAt(const AlignedVector4 *checkPosition, const AlignedVector4 *exclude, UwamonoRecord *record)
{
    float dy = checkPosition->y - record->position.y;

    if (__builtin_fabsf(dy) > 0.6f) {
        return 0;
    }
    if (record->searchMode != 1) {
        if (record->searchMode != 2) {
            return 0;
        }
        return CrossCheckMapUnitCircle(checkPosition, exclude, record);
    }
    return CrossCheckMapUnitBox(checkPosition, exclude, record);
}

int CrossCheckMapUnitCircle(const Vector4 *origin, const Vector4 *target, UwamonoRecord *record, AlignedVector4 *out)
{
    Vector4 mid;
    Vector4 normal;
    float radius;
    float halfLength;
    float dot;
    float dx;
    float dz;
    float oz;
    float ox;
    float distSq;
    float px;
    float pz;

    normal.x = target->x - origin->x;
    normal.y = 0.0f;
    normal.z = target->z - origin->z;
    dx = normal.x;
    dz = normal.z;
    radius = record->extent.x;
    halfLength = __builtin_sqrtf(dx * dx + dz * dz) * 0.5f;
    xglVectorNormal(&normal, &normal);
    ox = origin->x;
    oz = origin->z;
    mid.x = ox + (target->x - ox) * 0.5f;
    mid.z = oz + (target->z - oz) * 0.5f;
    dx = record->position.x - mid.x;
    dz = record->position.z - mid.z;
    dot = dx * normal.x + dz * normal.z;
    if (halfLength + radius < __builtin_fabsf(dot)) {
        return 0;
    }
    px = dot * normal.x + mid.x;
    pz = dot * normal.z + mid.z;
    dx = record->position.x - px;
    dz = record->position.z - pz;
    distSq = dx * dx + dz * dz;
    if (distSq < radius * radius) {
        return 1;
    }
    return 0;
}

extern float crossCheckBoxMargin;

int CrossCheckMapUnitBox(const Vector4 *origin, const Vector4 *target, UwamonoRecord *record)
{
    Vector4 corner[4];
    AlignedVector4 position;
    DoublewordVector4 extent;
    float (*matrix)[4];

    extent.words[1] = ((const u64 *)&record->extent)[1];
    extent.words[0] = ((const u64 *)&record->extent)[0];
    extent.vector.z = extent.vector.z + crossCheckBoxMargin;
    extent.vector.x = extent.vector.x + crossCheckBoxMargin;
    position = record->position;
    matrix = record->transform->matrix;

    corner[0].x = position.x + (extent.vector.x * matrix[0][0] + extent.vector.z * matrix[2][0]);
    corner[0].y = position.y;
    corner[0].z = position.z + (extent.vector.x * matrix[0][2] + extent.vector.z * matrix[2][2]);
    corner[1].x = position.x - (extent.vector.z * matrix[0][2] + extent.vector.x * matrix[2][2]);
    corner[1].y = position.y;
    corner[1].z = position.z + (extent.vector.z * matrix[0][0] + extent.vector.x * matrix[2][0]);

    if (CheckCrossLine(origin, target, &corner[0], &corner[1]) != 0) {
        return 1;
    }

    corner[2].x = position.x - (extent.vector.x * matrix[0][0] + extent.vector.z * matrix[2][0]);
    corner[2].y = position.y;
    corner[2].z = position.z - (extent.vector.x * matrix[0][2] + extent.vector.z * matrix[2][2]);

    if (CheckCrossLine(origin, target, &corner[1], &corner[2]) != 0) {
        return 1;
    }

    corner[3].x = position.x + (extent.vector.z * matrix[0][2] + extent.vector.x * matrix[2][2]);
    corner[3].y = position.y;
    corner[3].z = position.z - (extent.vector.z * matrix[0][0] + extent.vector.x * matrix[2][0]);

    if (CheckCrossLine(origin, target, &corner[2], &corner[3]) != 0) {
        return 1;
    }

    return CheckCrossLine(origin, target, &corner[3], &corner[0]) != 0;
}

/*
 * The probe block of UnduCheck (CheckUwamonoHeight and AimMapUnitLookCheck pass one):
 * two cleared words, the attribute mask, a flag byte, the range, the ground height and
 * work words. The +0x28 word is set as a float by AimMapUnitLookCheck (zeroed28 is its int view).
 * Same 0x40 bytes as MapCollisionObject (undulate.h), which it views for AimMapUnitLookCheck only: the unions here are
 * scheduling levers (the stores of the mask and of the +0x28 word get alias set 0; with plain MapCollisionObject fields the
 * mask store is scheduled right after its computation and the template copy changes registers), so UnduCheck gets the
 * request through a cast to the type it views.
 */
typedef struct UwamonoProbeRequest {
    int clearedA;                       /* +0x00 */
    int clearedB;                       /* +0x04 */
    union { unsigned short mask; float maskBits; };  /* +0x08 */
    unsigned char unmodeled_0C[3];
    unsigned char flag;                 /* +0x0F */
    float range;                        /* +0x10 */
    unsigned char unmodeled_14[4];
    int ground;                         /* +0x18 */
    unsigned int scratchWork;           /* +0x1C */
    unsigned char unmodeled_20[0x08];
    union { float word; int zeroed28; }; /* +0x28 */
    unsigned char unmodeled_2C[0x14];
} UwamonoProbeRequest;

extern UwamonoProbeRequest uwamonoProbeTemplate; /* game_camera.c defines it as a MapCollisionObject */

int AimMapUnitLookCheck(UwamonoRecord *record)
{
    struct XenoActor *object;
    UwamonoProbeRequest request;
    DoublewordAlignedVector4 position;
    Vector4 delta;

    object = uwamonoRecords[uwamonoCurrentIndex].body.actor;
    if (CrossCheckMapUnitAim(&object->position, record) != -1) {
        return 0;
    }
    request = uwamonoProbeTemplate;
    position = object->position;
    delta.x = record->position.x - object->position.x;
    delta.y = record->position.y - object->position.y;
    delta.z = record->position.z - object->position.z;
    delta.w = 1.0f;
    request.mask = (request.mask & 0xFFF8) | 0x803;
    request.flag = 0;
    request.word = 0.0f;
    UnduCheck(&position, 0, (MapCollisionObject *) &request);
    if (UnduCheck(&position, &delta, (MapCollisionObject *) &request) == -1000.0f) {
        return 0;
    }
    if (__builtin_fabsf(position.x - record->position.x) < 0.01f
        && __builtin_fabsf(position.z - record->position.z) < 0.01f) {
        return 1;
    }
    return 0;
}

void CheckUwamonoHeight(UwamonoRecord *record);

/* Copies the record position into its transform translation when the record is visible and the global gate is clear. */
void uwamonoUpdateTransform(UwamonoRecord *record)
{
    int flags;
    UwamonoPose *transform;

    flags = record->flags;
    if ((flags & 2) && !(gameWork.control & 1)) {
        if ((flags & 8) && record->parent == -1) {
            CheckUwamonoHeight(record);
        }
        transform = record->transform;
        transform->matrix[3][0] = record->position.x;
        transform->matrix[3][1] = record->position.y;
        transform->matrix[3][2] = record->position.z;
    }
}

int HitCheckUwamono(UwamonoRecord *record);

/* Falls a unit under gravity until it lands on the map or on another unit. */
void CheckUwamonoHeight(UwamonoRecord *record)
{
    MapCollisionObject probe;
    float velocity;
    float groundY;
    float newY;
    int hitIndex;

    UnduParamInit(&probe);
    probe.block = UnduDataGetHeader(256, -1);
    probe.flags = 0;
    groundY = UnduCheck(&record->position, 0, &probe);
    if (groundY == -1000.0f) {
        return;
    }
    if (!(record->flags & 8)) {
        return;
    }
    velocity = record->velocity.y;
    for (;;) {
        if (velocity > -0.6f) {
            velocity = record->velocity.y = velocity - gameWork.gravity;
        }
        newY = record->position.y + velocity;
        record->position.y = newY;
        if (newY < groundY) {
            record->position.y = groundY + 0.001f;
            if ((*(unsigned long long *)&record->flags & 0x10000010ULL) == 0) {
                record->velocity.y = -velocity / gameWork.bounceDivisor;
            } else {
                record->velocity.y = 0.0f;
            }
            if (record->velocity.y < 0.05f) {
                record->landedFlags |= MAP_UNIT_LANDED;
                record->velocity.y = 0.0f;
            }
        } else {
            hitIndex = HitCheckUwamono(record);
            if (hitIndex != -1) {
                record->position.y = uwamonoRecords[hitIndex].position.y
                    + uwamonoRecords[hitIndex].extent.y + 0.001f;
                if ((*(unsigned long long *)&record->flags & 0x10000010ULL) == 0) {
                    record->velocity.y = -record->velocity.y / gameWork.bounceDivisor;
                } else {
                    record->velocity.y = 0.0f;
                }
                if (record->velocity.y < 0.1f) {
                    record->velocity.y = 0.0f;
                }
            }
        }
        if (!(record->flags & 0x10)) {
            return;
        }
        if (record->velocity.y == 0.0f) {
            break;
        }
        velocity = record->velocity.y;
    }
}

int HitCheckUwamonoAt(UwamonoRecord *record, UwamonoRecord *candidate);

int HitCheckUwamono(UwamonoRecord *record)
{
    int index;

    for (index = 0; index < UWAMONO_RECORD_COUNT; index++) {
        UwamonoRecord *candidate = &uwamonoRecords[index];

        if (candidate->kind == 2 && candidate->key != record->key
            && (candidate->flags & 2) && candidate->searchMode != 0
            && HitCheckUwamonoAt(record, candidate) != 0) {
            return index;
        }
    }
    return -1;
}

extern float hitCheckCircleMargin;
int HitCheckBoxUwamono(UwamonoRecord *record, UwamonoRecord *other);

int HitCheckUwamonoAt(UwamonoRecord *record, UwamonoRecord *candidate)
{
    if (record->position.y < candidate->position.y) {
        return 0;
    }
    if (candidate->position.y + candidate->extent.y < record->position.y) {
        return 0;
    }
    switch (candidate->searchMode) {
    case UWAMONO_MODE_CIRCLE:
        if (CheckDist2D(&record->position, &candidate->position) < candidate->extent.x + hitCheckCircleMargin) {
            return 1;
        }
        return 0;
    case UWAMONO_MODE_BOX:
        return HitCheckBoxUwamono(record, candidate);
    default:
        return 0;
    }
}

extern float hitCheckBoxPointRadius;

int HitCheckBoxUwamono(UwamonoRecord *record, UwamonoRecord *other) {
    return uwamonoPointInBox(&record->position, &other->position, &other->extent, hitCheckBoxPointRadius, other->rotation.y) != 0;
}

const float uwamonoMapUnitTopScale = 0.89999997615814209f;
const float nyuruSnapEpsilonX = 0.0010000000474974513f;
const float nyuruSnapEpsilonZ = 0.0010000000474974513f;
float uwamonoAreaTopScale = 0.89999997615814209f;
float uwamonoAreaBottomMargin = 1.7999999523162842f;
float crossCheckBoxMargin = 0.10000000149011612f;
float hitCheckCircleMargin = 0.10000000149011612f;
float hitCheckBoxPointRadius = 0.15000000596046448f;
