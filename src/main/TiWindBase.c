#include "common.h"
#include "main/TiWindBase.h"
#include "main/assert_fail.h"
#include "main/TiDynamicsColl.h"

/* Moves the body toward the rest length of its link to the world matrix `other` (its translation row). */
void tiDynamicsSolveLink(TiSolverState *node, Matrix4 other, const float *restLength)
{
    TiVector3 dir;
    float length;
    float correction;

    dir = (TiVector3) { other[3][0] - node->position.vector.x, other[3][1] - node->position.vector.y, other[3][2] - node->position.vector.z };
    length = __builtin_sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    correction = *restLength - length;
    node->accumulator.vector.x -= dir.x * correction;
    node->accumulator.vector.y -= dir.y * correction;
    node->accumulator.vector.z -= dir.z * correction;
    node->position.vector.z -= dir.z * correction;
    node->position.vector.x -= dir.x * correction;
    node->position.vector.y -= dir.y * correction;
}

#include "shared.h"
#define DIR_WIND_MAX 4U
#define POINT_WIND_MAX 4U
#define WIND_FLAG_ENABLED 0x1
#define WIND_FLAG_GLOBAL 0x2
/* Gust redraw: xglSRand() % WIND_GUST_STEPS, gust = step * 0.01 + 0.5. */
#define WIND_GUST_STEPS 70U

/*
 * Directional wind table entry, 0x60 bytes.
 * flags: 0x1 enabled, 0x2 global (no falloff with distance).
 * 0x10: unit direction (w = 1 at 0x1C). 0x20: velocity, direction * phase * 0.1.
 * 0x30: position (w = 1 at 0x3C). 0x40: phase, oscillates between upper and lower.
 * phaseStep: +1 or -1, the direction of the phase; speed scales each step.
 * 0x4C: upper bound, 0x50: lower bound, 0x54: radius of the falloff.
 */
typedef struct DirWind {
    int flags;
    unsigned char unmodeled_04[0xC];
    /* The vectors stay float[4]: Vector4 members change the code. */
    union {
        Vector4 vector;
        float direction[4];
    };
    float velocity[4];
    float position[4];
    float phase;
    float phaseStep;
    float speed;
    float upper;
    float lower;
    float radius;
    unsigned char unmodeled_58[0x8];
} DirWind;

/* tiWindDirValueTable is the DirWind table 0x40 bytes in; the speed setter reaches its fields through it. */
typedef struct DirWindValue {
    unsigned char unmodeled_00[4];
    float phaseStep;
    float speed;
    unsigned char unmodeled_0C[0x54];
} DirWindValue;

/*
 * Point wind table entry, 0x40 bytes.
 * flags as DirWind. 0x10: position (w = 1 at 0x1C). 0x24: scalar wave, phase * 0.1.
 * 0x28: phase. phaseStep: +1 or -1. speedRange[0]: speed, [1]: upper bound, [2]: lower bound.
 * 0x3C: radius of the falloff.
 */
typedef struct PointWind {
    int flags;
    unsigned char unmodeled_04[0xC];
    float position[4];
    unsigned char unmodeled_20[0x4];
    float wave;
    float phase;
    float phaseStep;
    float speedRange[3];
    float radius;
} PointWind;

/* tiWindPointValueTable is the PointWind table 0x30 bytes in; the range setter reaches speedRange[1] and [2] through it. */
typedef struct PointWindValues {
    float speedRange[3];
    unsigned char unmodeled_0C[0x34];
} PointWindValues;

static DirWind tiWindDirTable[DIR_WIND_MAX];
extern DirWindValue tiWindDirValueTable[DIR_WIND_MAX];
static PointWind tiWindPointTable[POINT_WIND_MAX];
extern PointWindValues tiWindPointValueTable[POINT_WIND_MAX];
extern const char tiWindDirIndexAssertText[];
extern const char tiWindBaseSourceFile[];
extern const char tiWindPointIndexAssertText[];

/* Runs the collision solvers over every link of each cell of the link table. */
void tiDynamicsSolveCellPairs(TiMRPackage *world)
{
    TiSolverState *nodes;
    int leftStart;
    int rightStart;
    TiLinkRecord *cell;
    TiLinkRecord *pair;
    int nodeBase;
    u32 pairIndex;
    u32 cellIndex;

    nodes = world->dynMat;
    leftStart = world->hrcData->jointCount + world->hrcData->skipCount;
    rightStart = world->hrcData->joint3Count + world->hrcData->joint2Count;
    nodeBase = leftStart + rightStart;
    cell = (TiLinkRecord *)((char *)world->hrcData + world->hrcData->linkCellOffset);
    cellIndex = 0;
    if (world->hrcData->linkCellCount != 0) {
        do {
            pair = cell + 1;
            pairIndex = 0;
            if (cell->cell.linkCount != 0) {
                do {
                    pairIndex++;
                    tiDynamicsSolveLink(&nodes[cell->cell.firstNode - nodeBase], world->globalMat[pair->link.node], &pair->link.restLength);
                    tiDynamicsCollSolveSpherePair(&nodes[cell->cell.firstNode - nodeBase], &nodes[pair->link.node - nodeBase], world);
                    tiDynamicsCollSolveMatrixPair(&nodes[cell->cell.firstNode - nodeBase], &nodes[pair->link.node - nodeBase], world);
                    tiDynamicsCollSolveCapsulePair(&nodes[cell->cell.firstNode - nodeBase], &nodes[pair->link.node - nodeBase], world);
                    pair++;
                } while (pairIndex < cell->cell.linkCount);
            }
            cell = pair;
        } while (++cellIndex < world->hrcData->linkCellCount);
    }
}

/* Clears every directional and point wind entry. */
void tiWindClearTables(void)
{
    u32 index;

    for (index = 0; index < DIR_WIND_MAX; index++) {
        memset(&tiWindDirTable[index], 0, sizeof(DirWind));
    }
    for (index = 0; index < POINT_WIND_MAX; index++) {
        memset(&tiWindPointTable[index], 0, sizeof(PointWind));
    }
}

/* Enables or disables directional wind `index`. */
void tiWindDirSetEnabled(u32 index, int enabled)
{
    if (index >= DIR_WIND_MAX) {
        assertFailed(tiWindDirIndexAssertText, tiWindBaseSourceFile, 29);
    }
    if (enabled != 0) {
        tiWindDirTable[index].flags |= WIND_FLAG_ENABLED;
        return;
    }
    tiWindDirTable[index].flags &= ~WIND_FLAG_ENABLED;
}

/* Sets the direction of directional wind `index`: normalized, with the velocity reset to rest. */
void tiWindDirSetDirection(u32 index, float x, float y, float z)
{
    if (index >= DIR_WIND_MAX) {
        assertFailed(tiWindDirIndexAssertText, tiWindBaseSourceFile, 42);
    }
    tiWindDirTable[index].direction[0] = x;
    tiWindDirTable[index].direction[1] = y;
    tiWindDirTable[index].direction[2] = z;
    tiWindDirTable[index].direction[3] = 1.0f;
    xglVectorNormal(&tiWindDirTable[index].vector, &tiWindDirTable[index].vector);
    tiWindDirTable[index].velocity[0] = 0.0f;
    tiWindDirTable[index].velocity[1] = 0.0f;
    tiWindDirTable[index].velocity[2] = 0.0f;
    tiWindDirTable[index].velocity[3] = 1.0f;
}

/* Sets the position of directional wind `index`; the fourth component is the homogeneous 1. */
void tiWindDirSetPosition(u32 index, const float *position)
{
    if (index >= DIR_WIND_MAX) {
        assertFailed(tiWindDirIndexAssertText, tiWindBaseSourceFile, 60);
    }
    tiWindDirTable[index].position[0] = position[0];
    tiWindDirTable[index].position[1] = position[1];
    tiWindDirTable[index].position[2] = position[2];
    tiWindDirTable[index].position[3] = 1.0f;
}

/* Sets the speed of directional wind `index`. A new speed resets the phase step to +1. */
void tiWindDirSetSpeed(u32 index, float speed)
{
    DirWindValue *entry;

    if (index >= DIR_WIND_MAX) {
        assertFailed(tiWindDirIndexAssertText, tiWindBaseSourceFile, 73);
    }
    entry = &tiWindDirValueTable[index];
    if (entry->speed != speed) {
        entry->speed = speed;
        entry->phaseStep = 1.0f;
    }
}

/* Sets the upper and lower phase bounds of directional wind `index`. */
void tiWindDirSetRange(u32 index, float upper, float lower)
{
    DirWind *entry;

    if (index >= DIR_WIND_MAX) {
        assertFailed(tiWindDirIndexAssertText, tiWindBaseSourceFile, 87);
    }
    entry = &tiWindDirTable[index];
    entry->upper = upper;
    tiWindDirTable[index].lower = lower;
}

/* Sets the falloff radius of directional wind `index`; a non-zero `global` sets the global flag (no falloff with distance). */
void tiWindDirSetRadius(u32 index, float radius, int global)
{
    if (index >= DIR_WIND_MAX) {
        assertFailed(tiWindDirIndexAssertText, tiWindBaseSourceFile, 98);
    }
    tiWindDirTable[index].radius = radius;
    if (global != 0) {
        tiWindDirTable[index].flags |= WIND_FLAG_GLOBAL;
    } else {
        tiWindDirTable[index].flags &= ~WIND_FLAG_GLOBAL;
    }
}

/* Enables or disables point wind `index`. */
void tiWindPointSetEnabled(u32 index, int enabled)
{
    if (index >= POINT_WIND_MAX) {
        assertFailed(tiWindPointIndexAssertText, tiWindBaseSourceFile, 112);
    }
    if (enabled != 0) {
        tiWindPointTable[index].flags |= WIND_FLAG_ENABLED;
        return;
    }
    tiWindPointTable[index].flags &= ~WIND_FLAG_ENABLED;
}

/* Sets the position of point wind `index`; the fourth component is the homogeneous 1. */
void tiWindPointSetPosition(u32 index, const float *position)
{
    if (index >= POINT_WIND_MAX) {
        assertFailed(tiWindPointIndexAssertText, tiWindBaseSourceFile, 125);
    }
    tiWindPointTable[index].position[0] = position[0];
    tiWindPointTable[index].position[1] = position[1];
    tiWindPointTable[index].position[2] = position[2];
    tiWindPointTable[index].position[3] = 1.0f;
}

/* Sets the speed of point wind `index`, value[0] of its speed and range triple. */
void tiWindPointSetSpeed(u32 index, float speed)
{
    if (index >= POINT_WIND_MAX) {
        assertFailed(tiWindPointIndexAssertText, tiWindBaseSourceFile, 138);
    }
    if (tiWindPointTable[index].speedRange[0] != speed) {
        tiWindPointTable[index].speedRange[0] = speed;
        tiWindPointTable[index].phaseStep = 1.0f;
    }
}

/* Sets the upper and lower phase bounds of point wind `index`. */
void tiWindPointSetRange(u32 index, float upper, float lower)
{
    PointWindValues *entry;

    if (index >= POINT_WIND_MAX) {
        assertFailed(tiWindPointIndexAssertText, tiWindBaseSourceFile, 152);
    }
    entry = &tiWindPointValueTable[index];
    entry->speedRange[1] = upper;
    entry->speedRange[2] = lower;
}

/* Sets the falloff radius of point wind `index`; a non-zero `global` sets the global flag (no falloff with distance). */
void tiWindPointSetRadius(u32 index, float radius, int global)
{
    if (index >= POINT_WIND_MAX) {
        assertFailed(tiWindPointIndexAssertText, tiWindBaseSourceFile, 163);
    }
    tiWindPointTable[index].radius = radius;
    if (global != 0) {
        tiWindPointTable[index].flags |= WIND_FLAG_GLOBAL;
    } else {
        tiWindPointTable[index].flags &= ~WIND_FLAG_GLOBAL;
    }
}

extern const char tiWindWindAssertText[];

/* Pushes the state with a directional wind: a global wind moves it by velocity * time step * gust; otherwise the push
 * falls off with distance over the radius. The gust is redrawn when the phase reaches its upper bound. */
void tiWindDirApply(DirWind *entry, TiHrcDynamics *step, TiSolverState *state)
{
    float delta[3];
    float dist;
    float falloff;
    int gustRoll;

    if (entry == 0) {
        assertFailed(tiWindWindAssertText, tiWindBaseSourceFile, 177);
    }
    if (entry->phase == entry->upper) {
        gustRoll = xglSRand() % WIND_GUST_STEPS;
        state->gust = (float)gustRoll * 0.01f + 0.5f;
    }
    if (entry->flags & WIND_FLAG_GLOBAL) {
        state->position.vector.x += entry->velocity[0] * step->timeStep * state->gust;
        state->position.vector.y += entry->velocity[1] * step->timeStep * state->gust;
        state->position.vector.z += entry->velocity[2] * step->timeStep * state->gust;
        return;
    }
    delta[0] = state->position.vector.x - entry->position[0];
    delta[1] = state->position.vector.y - entry->position[1];
    delta[2] = state->position.vector.z - entry->position[2];
    dist = __builtin_sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (entry->radius == 0.0f || !(dist <= entry->radius)) {
        return;
    }
    falloff = dist / entry->radius;
    state->position.vector.x += entry->velocity[0] * step->timeStep * falloff * state->gust;
    state->position.vector.y += entry->velocity[1] * step->timeStep * falloff * state->gust;
    state->position.vector.z += entry->velocity[2] * step->timeStep * falloff * state->gust;
}

/* Pushes the state with a point wind: along the unit direction from the wind's position to the state, scaled by the
 * wave strength; a global wind has no falloff with distance. The gust is redrawn when the phase reaches its upper bound. */
void tiWindPointApply(PointWind *entry, TiHrcDynamics *step, TiSolverState *state)
{
    float delta[4];
    float dist;
    float scale;
    int gustRoll;

    if (entry == 0) {
        assertFailed(tiWindWindAssertText, tiWindBaseSourceFile, 224);
    }
    delta[0] = state->position.vector.x - entry->position[0];
    delta[1] = state->position.vector.y - entry->position[1];
    delta[2] = state->position.vector.z - entry->position[2];
    delta[3] = 1.0f;
    if (entry->phase == entry->speedRange[1]) {
        gustRoll = xglSRand() % WIND_GUST_STEPS;
        state->gust = (float)gustRoll * 0.01f + 0.5f;
    }
    dist = __builtin_sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (dist == 0.0f) {
        dist = 1e-7f;
    }
    scale = 1.0f / dist;
    delta[0] *= scale;
    delta[1] *= scale;
    delta[2] *= scale;
    if (entry->flags & WIND_FLAG_GLOBAL) {
        state->position.vector.x += delta[0] * entry->wave * step->timeStep * state->gust;
        state->position.vector.y += delta[1] * entry->wave * step->timeStep * state->gust;
        state->position.vector.z += delta[2] * entry->wave * step->timeStep * state->gust;
        return;
    }
    if (entry->radius == 0.0f || !(dist <= entry->radius)) {
        return;
    }
    scale = dist / entry->radius;
    state->position.vector.x += delta[0] * entry->wave * step->timeStep * scale * state->gust;
    state->position.vector.y += delta[1] * entry->wave * step->timeStep * scale * state->gust;
    state->position.vector.z += delta[2] * entry->wave * step->timeStep * scale * state->gust;
}


/* Applies every enabled wind to the solver state; the parameters are the dynamics node and the solver state. */
void tiWindApplyAll(TiHrcDynamics *first_input, TiSolverState *second_input)
{
    u32 index;

    for (index = 0; index < DIR_WIND_MAX; index++) {
        if (tiWindDirTable[index].flags & WIND_FLAG_ENABLED) {
            tiWindDirApply(&tiWindDirTable[index], first_input, second_input);
        }
    }
    for (index = 0; index < POINT_WIND_MAX; index++) {
        if (tiWindPointTable[index].flags & WIND_FLAG_ENABLED) {
            tiWindPointApply(&tiWindPointTable[index], first_input, second_input);
        }
    }
}

/* Advances the phase of directional wind `entry`: the phase is clamped to the upper bound (0x4C) and to the lower bound (0x50), reversing the step at each. The velocity is direction * phase * 0.1. */
void tiWindDirAdvance(DirWind *entry)
{
    float nextPhase;

    if (entry == 0) {
        assertFailed(tiWindWindAssertText, tiWindBaseSourceFile, 294);
    }
    nextPhase = entry->phase + entry->speed * entry->phaseStep * 0.1f;
    entry->phase = nextPhase;
    if (nextPhase <= entry->upper) {
        entry->phase = entry->upper;
        entry->phaseStep = 1.0f;
    } else if (entry->lower <= nextPhase) {
        entry->phase = entry->lower;
        entry->phaseStep = -1.0f;
    }
    entry->velocity[0] = entry->direction[0] * entry->phase * 0.1f;
    entry->velocity[1] = entry->direction[1] * entry->phase * 0.1f;
    entry->velocity[2] = entry->direction[2] * entry->phase * 0.1f;
}

/* Advances the phase of point wind `entry`: the phase is clamped to the lower bound (speedRange[1]) and to the upper bound (speedRange[2]), reversing the step at each. */
void tiWindPointAdvance(PointWind *entry)
{
    float nextPhase;

    if (entry == 0) {
        assertFailed(tiWindWindAssertText, tiWindBaseSourceFile, 320);
    }
    nextPhase = entry->phase + entry->speedRange[0] * entry->phaseStep * 0.1f;
    entry->phase = nextPhase;
    if (nextPhase <= entry->speedRange[1]) {
        entry->phase = entry->speedRange[1];
        entry->phaseStep = 1.0f;
    } else if (entry->speedRange[2] <= nextPhase) {
        entry->phase = entry->speedRange[2];
        entry->phaseStep = -1.0f;
    }
    entry->wave = entry->phase * 0.1f;
}


/* Advances the phase of every enabled wind. */
void tiWindAdvanceAll(void)
{
    u32 index;

    for (index = 0; index < DIR_WIND_MAX; index++) {
        if (tiWindDirTable[index].flags & WIND_FLAG_ENABLED) {
            tiWindDirAdvance(&tiWindDirTable[index]);
        }
    }
    for (index = 0; index < POINT_WIND_MAX; index++) {
        if (tiWindPointTable[index].flags & WIND_FLAG_ENABLED) {
            tiWindPointAdvance(&tiWindPointTable[index]);
        }
    }
}

/* Disables every wind; the entries keep their data. */
void tiWindDisableAll(void)
{
    u32 index;

    for (index = 0; index < DIR_WIND_MAX; index++) {
        tiWindDirTable[index].flags &= ~WIND_FLAG_ENABLED;
    }
    for (index = 0; index < POINT_WIND_MAX; index++) {
        tiWindPointTable[index].flags &= ~WIND_FLAG_ENABLED;
    }
}

/* Returns directional wind `index`. */
DirWind *tiWindDirGet(u32 index)
{
    if (index >= DIR_WIND_MAX) {
        assertFailed(tiWindDirIndexAssertText, tiWindBaseSourceFile, 375);
    }
    return &tiWindDirTable[index];
}

/* Returns point wind `index`. */
PointWind *tiWindPointGet(u32 index)
{
    if (index >= POINT_WIND_MAX) {
        assertFailed(tiWindPointIndexAssertText, tiWindBaseSourceFile, 385);
    }
    return &tiWindPointTable[index];
}

const char tiWindDirIndexAssertText[24] = "num < DIR_WIND_MAX";
const char tiWindBaseSourceFile[24] = "../TiWindBase.euc.c";
const char tiWindPointIndexAssertText[24] = "num < POINT_WIND_MAX";
const char tiWindWindAssertText[8] = "wind";
