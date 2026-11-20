#include "common.h"
#include "shared.h"
#include "main/TiGlobalColl.h"
#include "main/TiXacAnimCurve.h"
#include "main/TiMRPackage.h"
#include "main/assert_fail.h"

#include "main/xgl_2.h"
/* Scale floor of tiGlobalCollClampScale and the share of the scale it takes from the accumulator. */
#define TI_GLOBAL_COLL_MIN_SCALE 0.03f
#define TI_GLOBAL_COLL_SCALE_LOSS_FRACTION 0.2f
/* Level from which tiGlobalCollEvalSubobject takes the second sub-object routine. */
#define TI_GLOBAL_COLL_LEVEL_LIMIT 1.5f

/* Offsets used by tiGlobalCollEvalSubobject; gaps are not modeled. */
typedef struct TiGlobalCollObject {
    unsigned char unmodeled_00[0x04];
    f32 level;               /* 0x04: compared with 1.5f in tiGlobalCollEvalSubobject */
    unsigned char unmodeled_08[0x48];
    unsigned char subobject; /* 0x50: its address is taken in tiGlobalCollEvalSubobject */
} TiGlobalCollObject;

/* Floors the height (y) of the body at TI_GLOBAL_COLL_MIN_SCALE; a body below it loses the old value times
 * TI_GLOBAL_COLL_SCALE_LOSS_FRACTION from its accumulator. */
void tiGlobalCollClampScale(TiSolverState *state) {
    f32 previousScale;

    previousScale = state->position.vector.y;
    if (previousScale < TI_GLOBAL_COLL_MIN_SCALE) {
        state->position.vector.y = TI_GLOBAL_COLL_MIN_SCALE;
        state->accumulator.vector.y -= previousScale * TI_GLOBAL_COLL_SCALE_LOSS_FRACTION;
    }
}

void tiGlobalCollClearHandle(void) {
    tiGlobalCollSetHandle(0);
}

/* The current collision handle: set by tiGlobalCollSetHandle, cleared by tiGlobalCollClearHandle
 * and read by tiGlobalCollGetHandle. Zero means that no collision set is active. */
extern TiGlobalCollSet *tiGlobalCollHandle;

void tiGlobalCollSetHandle(TiGlobalCollSet *handle) {
    tiGlobalCollHandle = handle;
}

TiGlobalCollSet *tiGlobalCollGetHandle(void) {
    return tiGlobalCollHandle;
}

/* 0x20-byte sphere entry: the radius is at +0x0 and the centre at +0x10. */
typedef struct {
    float radius;               /* +0x00 */
    unsigned char unmodeled_04[12];
    Vector4 centre;             /* +0x10 */
} TiGlobalCollSphere;

/* Pushes the body out of each sphere of the handle's list. Inside a sphere the body
 * is moved to the surface along the unit direction from the centre, and its velocity
 * gains the push scaled by the penetration depth. */
void tiGlobalCollPushOutSpheres(TiGlobalCollSet *set, TiHrcDynamics *pNode, TiSolverState *context) {
    TiGlobalCollSphere *sphere;
    TiSolverState *body;
    Vector4 offset;
    float lengthSquared;
    float depth;
    float distance;
    u32 sphereIndex;

    body = context;
    sphere = (TiGlobalCollSphere *) ((unsigned char *) set + set->sphereEntriesOffset);
    sphereIndex = 0;
    if (set->sphereCount != 0) {
        do {
            offset.x = body->position.vector.x - sphere->centre.x;
            offset.y = body->position.vector.y - sphere->centre.y;
            offset.z = body->position.vector.z - sphere->centre.z;
            if (__builtin_fabsf(offset.x) < sphere->radius) {
                if (__builtin_fabsf(offset.y) < sphere->radius) {
                    if (__builtin_fabsf(offset.z) < sphere->radius) {
                        lengthSquared = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
                        distance = __builtin_sqrtf(lengthSquared);
                        if (distance == 0.0f) {
                            distance = 0.000001f;
                        }
                        if (distance < sphere->radius) {
                            depth = sphere->radius - distance;
                            distance = 1.0f / distance;
                            offset.x = offset.x * distance;
                            offset.y = offset.y * distance;
                            offset.z = offset.z * distance;
                            body->accumulator.vector.x = body->accumulator.vector.x + offset.x * sphere->radius * depth;
                            body->accumulator.vector.y = body->accumulator.vector.y + offset.y * sphere->radius * depth;
                            body->accumulator.vector.z = body->accumulator.vector.z + offset.z * sphere->radius * depth;
                            body->position.vector.x = offset.x * sphere->radius + sphere->centre.x;
                            body->position.vector.y = offset.y * sphere->radius + sphere->centre.y;
                            body->position.vector.z = offset.z * sphere->radius + sphere->centre.z;
                        }
                    }
                }
            }
            sphere++;
            sphereIndex++;
        } while (sphereIndex < set->sphereCount);
    }
}

/* 0x80-byte plane entry: toWorld maps a clamped point back out of the plane space,
 * toLocal takes the position into it. */
typedef struct {
    Matrix4 toWorld;            /* +0x00 */
    Matrix4 toLocal;            /* +0x40 */
} TiGlobalCollPlane;


/* Clamps the body above each plane of the handle's set: the position is taken into
 * the plane space with toLocal, and a point below the plane (local y < 0) is moved
 * onto it and mapped back with toWorld. */
void tiGlobalCollClampAbovePlanes(TiGlobalCollSet *set, TiHrcDynamics *pNode, TiSolverState *context) {
    TiGlobalCollPlane *plane;
    TiSolverState *body;
    Vector4 localPoint;
    u32 planeIndex;

    body = context;
    plane = (TiGlobalCollPlane *) ((unsigned char *) set + set->planeEntriesOffset);
    planeIndex = 0;
    if (set->planeCount != 0) {
        do {
            localPoint.x = body->position.vector.x;
            localPoint.y = body->position.vector.y;
            localPoint.z = body->position.vector.z;
            localPoint.w = 1.0f;
            xglVectorMulMat(&localPoint, plane->toLocal, &localPoint);
            if (localPoint.y < 0.0f) {
                localPoint.y = 0.0f;
                if (localPoint.x == 0.0f && localPoint.z == 0.0f) {
                    localPoint.x = 0.0001f;
                }
                xglVectorMulMat(&localPoint, plane->toWorld, &localPoint);
                body->position.vector.x = localPoint.x;
                body->position.vector.y = localPoint.y;
                body->position.vector.z = localPoint.z;
            }
            plane++;
            planeIndex++;
        } while (planeIndex < set->planeCount);
    }
}

/* 0x90-byte cylinder entry: toWorld (+0x10) maps the pushed point back, toLocal (+0x50)
 * takes the position into the cylinder space. */
typedef struct {
    float radius;               /* +0x00 */
    float halfHeight;           /* +0x04 */
    unsigned char unmodeled_08[8];
    Matrix4 toWorld;            /* +0x10 */
    Matrix4 toLocal;            /* +0x50 */
} TiGlobalCollCylinder;

/* Pushes the body out of each cylinder of the handle's set: the position is taken into
 * the cylinder space with toLocal; inside the radius and half height, its horizontal
 * distance is pulled back to the radius and the point is mapped back with toWorld. */
void tiGlobalCollPushOutCylinders(TiGlobalCollSet *set, TiHrcDynamics *pNode, TiSolverState *context) {
    TiGlobalCollCylinder *cylinder;
    TiSolverState *body;
    Vector4 localPoint;
    float radialDistance;
    u32 cylinderIndex;

    body = context;
    cylinder = (TiGlobalCollCylinder *) ((unsigned char *) set + set->cylinderEntriesOffset);
    cylinderIndex = 0;
    if (set->cylinderCount != 0) {
        do {
            localPoint.x = body->position.vector.x;
            localPoint.y = body->position.vector.y;
            localPoint.z = body->position.vector.z;
            localPoint.w = 1.0f;
            xglVectorMulMat(&localPoint, cylinder->toLocal, &localPoint);
            if (__builtin_fabsf(localPoint.y) < cylinder->halfHeight) {
                radialDistance = localPoint.x;
                radialDistance = radialDistance * localPoint.x;
                radialDistance = radialDistance + localPoint.z * localPoint.z;
                radialDistance = __builtin_sqrtf(radialDistance);
                if (radialDistance < cylinder->radius) {
                    if (radialDistance == 0.0f) {
                        radialDistance = 0.0000001f;
                    }
                    radialDistance = 1.0f / radialDistance;
                    localPoint.x = localPoint.x * radialDistance * cylinder->radius;
                    localPoint.z = localPoint.z * radialDistance * cylinder->radius;
                    xglVectorMulMat(&localPoint, cylinder->toWorld, &localPoint);
                    body->position.vector.x = localPoint.x;
                    body->position.vector.y = localPoint.y;
                    body->position.vector.z = localPoint.z;
                }
            }
            cylinder++;
            cylinderIndex++;
        } while (cylinderIndex < set->cylinderCount);
    }
}

/* Solves one body (context) of the dynamics node pNode against
 * the shapes of the current handle: spheres, planes, then cylinders. */
void tiGlobalCollSolveBody(TiHrcDynamics *pNode, TiSolverState *context) {
    TiGlobalCollSet *handle;

    handle = tiGlobalCollGetHandle();
    if (pNode == 0) {
        assertFailed("pNode", "../TiGlobalColl.euc.c", 138);
    }
    if (handle != 0) {
        tiGlobalCollPushOutSpheres(handle, pNode, context);
        tiGlobalCollClampAbovePlanes(handle, pNode, context);
        tiGlobalCollPushOutCylinders(handle, pNode, context);
    }
}

/* Chooses the sub-object routine by obj->level: below 1.5f or not. */
void tiGlobalCollEvalSubobject(f32 *values, TiGlobalCollObject *obj, f32 amount) {
    u8 *subobjectPtr;

    subobjectPtr = &obj->subobject;
    if (obj->level < TI_GLOBAL_COLL_LEVEL_LIMIT) {
        xacReadJointCurves12Padded(values, &subobjectPtr, amount);
        return;
    }
    xacReadJointCurves15(values, &subobjectPtr, amount);
}

/* Handle slot: 16 bytes in the original, only the first word is read. */
TiGlobalCollSet *tiGlobalCollHandle = 0;

/* Coefficients read by mapHeadingXZ (src/main/undulate.c). */
float mapHeadingCoefficients[8] = {
    0.999999344f, -0.333298564f, 0.199465364f, -0.139085338f, 0.0964200422f, -0.055909887f, 0.0218612291f, -0.00405405788f
};
