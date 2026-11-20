#include "common.h"
#include "shared.h"
#include "main/TiDynamicsColl.h"
#include "main/xgl_2.h"

/* The accumulator takes this share of each push-out move. */
#define TI_DYNAMICS_COLL_ACCUMULATE_FRACTION 0.2f


/* Pushes a position out of a sphere: the offset from the center must lie inside the box of the radius and inside
 * the sphere; the point moves to the sphere surface along the normalized offset, and the 0.2-weighted correction
 * accumulates into the accumulator. */
void tiDynamicsCollPushOutSphere(Vector4 *position, Vector4 *accumulator, const TiCollSphere *sphere) {
    TiVector3 offset;
    TiVector3 original;
    float distance;
    float radius;

    offset.x = position->x - sphere->center.x;
    offset.y = position->y - sphere->center.y;
    offset.z = position->z - sphere->center.z;
    radius = sphere->radius + 0.0f;
    if (sphere->flags & TI_DYNAMICS_COLL_RECORD_DISABLED) {
        return;
    }
    if (!(__builtin_fabsf(offset.x) < radius)) {
        return;
    }
    if (!(__builtin_fabsf(offset.y) < radius)) {
        return;
    }
    if (!(__builtin_fabsf(offset.z) < radius)) {
        return;
    }
    distance = __builtin_sqrtf(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
    if (distance == 0.0f) {
        distance = 1e-6f;
    }
    if (!(distance < radius)) {
        return;
    }
    distance = 1.0f / distance;
    original.x = position->x;
    original.y = position->y;
    original.z = position->z;
    offset.x = offset.x * distance;
    offset.y = offset.y * distance;
    offset.z = offset.z * distance;
    position->x = offset.x * radius + sphere->center.x;
    position->y = offset.y * radius + sphere->center.y;
    position->z = offset.z * radius + sphere->center.z;
    accumulator->x += (position->x - original.x) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
    accumulator->y += (position->y - original.y) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
    accumulator->z += (position->z - original.z) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
}

/* Pushes one position out of the ground plane: transform by matrix_90 then matrix_50; when the point is below
 * y = 0 and the record is not flagged, project it to y = 0 and transform by matrix_10 and the palette matrix,
 * then accumulate the 0.2-weighted correction and store the point back. */
void tiDynamicsCollClampAbovePlane(Vector4 *position, Vector4 *accumulator, TiCollMatrix *record, TiMRPackage *object) {
    Vector4 point;

    point.x = position->x;
    point.y = position->y;
    point.z = position->z;
    point.w = 1.0f;
    xglVectorMulMat(&point, record->matrix_90.rows, &point);
    xglVectorMulMat(&point, record->matrix_50, &point);
    if (record->flags & TI_DYNAMICS_COLL_RECORD_DISABLED) {
        return;
    }
    if (point.y < 0.0f) {
        point.y = 0.0f;
        xglVectorMulMat(&point, record->matrix_10, &point);
        xglVectorMulMat(&point, object->globalMat[record->index], &point);
        accumulator->x += (point.x - position->x) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
        accumulator->y += (point.y - position->y) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
        accumulator->z += (point.z - position->z) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
        position->x = point.x;
        position->y = point.y;
        position->z = point.z;
    }
}

/* Moves the point onto the sphere of the given radius around center when each offset component is below the
 * radius and the length is below it (one-sided tests, as in the original): the offset is normalized and scaled
 * by radius. */
void tiDynamicsCollPushPointOutOfSphere(Vector4 *point, const Vector4 *center, float radius) {
    TiVector3 delta;
    TiVector3 offset;
    float distance;
    int i;

    offset.x = point->x - center->x;
    offset.y = point->y - center->y;
    offset.z = point->z - center->z;
    delta = offset;
    if (!(delta.x < radius)) {
        return;
    }
    if (!(delta.y < radius)) {
        return;
    }
    if (!(delta.z < radius)) {
        return;
    }
    distance = __builtin_sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (!(distance < radius)) {
        return;
    }
    if (distance == 0.0f) {
        distance = 1e-7f;
    }
    distance = 1.0f / distance;
    for (i = 0; i < 3; i++) {
        float *offset_axis = &delta.x + i;
        const float *center_axis = &center->x + i;
        float *point_axis = &point->x + i;

        *offset_axis = *offset_axis * distance;
        *point_axis = *offset_axis * radius + *center_axis;
    }
}

extern Vector4 tiDynamicsCollCapsuleCenter;

void tiDynamicsCollPushOutCapsule(Vector4 *position, Vector4 *accumulator, TiCollCapsule *capsule, TiMRPackage *object) {
    Vector4 point;
    float distance;
    float radius;
    double absHeight;

    point.w = 1.0f;
    point.x = position->x;
    point.y = position->y;
    point.z = position->z;
    xglVectorMulMat(&point, capsule->matrix_a0.rows, &point);
    xglVectorMulMat(&point, capsule->matrix_60, &point);
    if (capsule->flags & TI_DYNAMICS_COLL_RECORD_DISABLED) {
        return;
    }
    tiDynamicsCollCapsuleCenter.y = capsule->height;
    tiDynamicsCollPushPointOutOfSphere(&point, &tiDynamicsCollCapsuleCenter, capsule->radius);
    tiDynamicsCollCapsuleCenter.y = -capsule->height;
    tiDynamicsCollPushPointOutOfSphere(&point, &tiDynamicsCollCapsuleCenter, capsule->radius);
    absHeight = point.y;
    if (absHeight < 0.0) {
        absHeight = 0.0 - absHeight;
    }
    if (!(absHeight < capsule->height + 0.0f)) {
        return;
    }
    distance = __builtin_sqrtf(point.x * point.x + point.z * point.z);
    radius = capsule->radius + 0.0f;
    if (!(distance < radius)) {
        return;
    }
    if (distance == 0.0f) {
        distance = 1e-7f;
    }
    distance = 1.0f / distance;
    point.x = point.x * distance * radius;
    point.z = point.z * distance * radius;
    xglVectorMulMat(&point, capsule->matrix_20, &point);
    xglVectorMulMat(&point, object->globalMat[capsule->index], &point);
    accumulator->x += (point.x - position->x) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
    accumulator->y += (point.y - position->y) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
    accumulator->z += (point.z - position->z) * TI_DYNAMICS_COLL_ACCUMULATE_FRACTION;
    position->x = point.x;
    position->y = point.y;
    position->z = point.z;
}

void tiDynamicsCollPushOutSphereList(TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state) {
    TiCollSphere *records;
    TiCollEntry *entry;
    unsigned int i;

    if (node->sphereCount == 0) {
        return;
    }
    entry = node->entries;
    records = (TiCollSphere *)((unsigned char *)object->hrcData + object->hrcData->sphereOffset);
    for (i = 0; i < node->sphereCount; i++, entry++) {
        tiDynamicsCollPushOutSphere(&state->position.vector, &state->accumulator.vector, &records[entry->recordIndex]);
    }
}

void tiDynamicsCollPushOutMatrixList(TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state) {
    TiCollMatrix *records;
    TiCollEntry *entry;
    unsigned int i;

    if (node->matrixCount == 0) {
        return;
    }
    records = (TiCollMatrix *)((unsigned char *)object->hrcData + object->hrcData->matrixOffset);
    entry = &node->entries[node->sphereCount];
    for (i = 0; i < node->matrixCount; i++, entry++) {
        tiDynamicsCollClampAbovePlane(&state->position.vector, &state->accumulator.vector, &records[entry->recordIndex], object);
    }
}

void tiDynamicsCollPushOutCapsuleList(TiMRPackage *object, TiHrcDynamics *node, TiSolverState *state) {
    TiCollCapsule *records;
    TiHrcData *record_set;
    TiCollEntry *entry;
    unsigned int i;

    if (node->capsuleCount == 0) {
        return;
    }
    record_set = object->hrcData;
    records = (TiCollCapsule *)((unsigned char *)record_set + record_set->capsuleOffset);
    entry = node->entries + node->sphereCount + node->matrixCount;
    for (i = 0; i < node->capsuleCount; i++) {
        tiDynamicsCollPushOutCapsule(&state->position.vector, &state->accumulator.vector, &records[entry->recordIndex], object);
        entry++;
    }
}

void tiDynamicsCollSolveSpherePair(TiSolverState *first, TiSolverState *second, TiMRPackage *object) {
    DoublewordVector4 midpoint;
    DoublewordVector4 correction;
    TiCollSphere *records;
    TiCollEntry *entry;
    unsigned int i;
    int record_index;
    TiHrcData *record_set;

    if (first->node->sphereCount == 0) {
        return;
    }
    entry = first->node->entries;
    record_set = object->hrcData;
    records = (TiCollSphere *)((unsigned char *)record_set + record_set->sphereOffset);
    midpoint.vector.w = 1.0f;
    correction.vector.w = 1.0f;
    midpoint.vector.x = (second->position.vector.x - first->position.vector.x) * 0.5f + first->position.vector.x;
    correction.vector.x = 0.0f;
    midpoint.vector.y = (second->position.vector.y - first->position.vector.y) * 0.5f + first->position.vector.y;
    correction.vector.y = 0.0f;
    midpoint.vector.z = (second->position.vector.z - first->position.vector.z) * 0.5f + first->position.vector.z;
    correction.vector.z = 0.0f;
    for (i = 0; i < first->node->sphereCount; i++) {
        correction.vector.x = 0.0f;
        correction.vector.y = 0.0f;
        correction.vector.z = 0.0f;
        record_index = entry->recordIndex;
        entry++;
        tiDynamicsCollPushOutSphere(&midpoint.vector, &correction.vector, &records[record_index]);
        first->position.vector.x += correction.vector.x;
        first->position.vector.y += correction.vector.y;
        first->position.vector.z += correction.vector.z;
        first->accumulator.vector.x += correction.vector.x;
        first->accumulator.vector.y += correction.vector.y;
        first->accumulator.vector.z += correction.vector.z;
        second->position.vector.x += correction.vector.x;
        second->position.vector.y += correction.vector.y;
        second->position.vector.z += correction.vector.z;
        second->accumulator.vector.x += correction.vector.x;
        second->accumulator.vector.y += correction.vector.y;
        second->accumulator.vector.z += correction.vector.z;
    }
}

void tiDynamicsCollSolveMatrixPair(TiSolverState *first, TiSolverState *second, TiMRPackage *object) {
    DoublewordVector4 midpoint;
    DoublewordVector4 correction;
    TiCollEntry *entry;
    TiCollMatrix *records;
    int processed;
    unsigned int sphereCount;
    TiHrcData *record_set;
    unsigned int record_index;

    if (first->node->matrixCount == 0) {
        return;
    }
    processed = 0;
    record_set = object->hrcData;
    records = (TiCollMatrix *)((unsigned char *)record_set + record_set->matrixOffset);
    sphereCount = first->node->sphereCount;
    entry = &first->node->entries[sphereCount];
    midpoint.vector.w = 1.0f;
    correction.vector.w = 1.0f;
    correction.vector.x = 0.0f;
    correction.vector.y = 0.0f;
    correction.vector.z = 0.0f;
    midpoint.vector.x = (second->position.vector.x - first->position.vector.x) * 0.5f + first->position.vector.x;
    midpoint.vector.y = (second->position.vector.y - first->position.vector.y) * 0.5f + first->position.vector.y;
    midpoint.vector.z = (second->position.vector.z - first->position.vector.z) * 0.5f + first->position.vector.z;
    if (first->node->matrixCount != 0) {
        do {
            processed++;
            correction.vector.x = 0.0f;
            correction.vector.y = 0.0f;
            correction.vector.z = 0.0f;
            record_index = entry->recordIndex;
            entry++;
            tiDynamicsCollClampAbovePlane(&midpoint.vector, &correction.vector, &records[record_index], object);
            first->position.vector.x = first->position.vector.x + correction.vector.x;
            first->position.vector.y = first->position.vector.y + correction.vector.y;
            first->position.vector.z = first->position.vector.z + correction.vector.z;
            first->accumulator.vector.x = first->accumulator.vector.x + correction.vector.x;
            first->accumulator.vector.y = first->accumulator.vector.y + correction.vector.y;
            first->accumulator.vector.z = first->accumulator.vector.z + correction.vector.z;
            second->position.vector.x = second->position.vector.x + correction.vector.x;
            second->position.vector.y = second->position.vector.y + correction.vector.y;
            second->position.vector.z = second->position.vector.z + correction.vector.z;
            second->accumulator.vector.x = second->accumulator.vector.x + correction.vector.x;
            second->accumulator.vector.y = second->accumulator.vector.y + correction.vector.y;
            second->accumulator.vector.z = second->accumulator.vector.z + correction.vector.z;
        } while (processed < first->node->matrixCount);
    }
}

void tiDynamicsCollSolveCapsulePair(TiSolverState *first, TiSolverState *second, TiMRPackage *object) {
    DoublewordVector4 midpoint;
    DoublewordVector4 correction;
    TiCollCapsule *records;
    TiCollEntry *entry;
    unsigned int i;
    TiHrcData *record_set;

    if (first->node->capsuleCount == 0) {
        return;
    }
    entry = &(&first->node->entries[first->node->sphereCount])[first->node->matrixCount];
    record_set = object->hrcData;
    records = (TiCollCapsule *)((unsigned char *)record_set + record_set->capsuleOffset);
    midpoint.vector.w = 1.0f;
    correction.vector.w = 1.0f;
    midpoint.vector.x = (second->position.vector.x - first->position.vector.x) * 0.5f + first->position.vector.x;
    correction.vector.x = 0.0f;
    midpoint.vector.y = (second->position.vector.y - first->position.vector.y) * 0.5f + first->position.vector.y;
    correction.vector.y = 0.0f;
    midpoint.vector.z = (second->position.vector.z - first->position.vector.z) * 0.5f + first->position.vector.z;
    correction.vector.z = 0.0f;
    for (i = 0; i < first->node->capsuleCount; i++) {
        correction.vector.x = 0.0f;
        correction.vector.y = 0.0f;
        correction.vector.z = 0.0f;
        tiDynamicsCollPushOutCapsule(&midpoint.vector, &correction.vector, &records[entry->recordIndex], object);
        entry++;
        first->position.vector.x += correction.vector.x;
        first->position.vector.y += correction.vector.y;
        first->position.vector.z += correction.vector.z;
        first->accumulator.vector.x += correction.vector.x;
        first->accumulator.vector.y += correction.vector.y;
        first->accumulator.vector.z += correction.vector.z;
        second->position.vector.x += correction.vector.x;
        second->position.vector.y += correction.vector.y;
        second->position.vector.z += correction.vector.z;
        second->accumulator.vector.x += correction.vector.x;
        second->accumulator.vector.y += correction.vector.y;
        second->accumulator.vector.z += correction.vector.z;
    }
}

Vector4 tiDynamicsCollCapsuleCenter = { 0.0f, 0.0f, 0.0f, 1.0f };
