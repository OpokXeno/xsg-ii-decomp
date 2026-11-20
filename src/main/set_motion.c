#include "common.h"
#include "main/get_ladder.h"
#include "main/enemy_1.h"
#include "main/pad.h"
#include "shared.h"
#include "main/get.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/xenoActor.h"
#include "main/set_motion.h"
#include "main/ytachi_test.h"
#include "main/xgl_render.h"
#include "main/enemy_system_init.h"
#include "main/sef.h"

/* The object is a 12-byte array (ytachi_test.c), but only its first byte is read: declared as a one-byte scalar the access is
 * $gp-relative, as in the original (a sized or unsized array declaration here changes the code). */
extern unsigned char uwamonoTriggerTypeFlag;



extern const float actorOverlapMargin;




void Enemy_After_Battle(int unusedKey)
{
    int index;
    unsigned char kind;
    UwamonoState *state;
    Vector4 *route;
    struct XenoActor *actor;

    for (index = 0; index < 256; index++) {
        kind = uwamonoRecords[index].kind;
        if (kind != 1) {
            continue;
        }
        state = uwamonoRecords[index].body.motions;
        route = state->trailPoints;
        actor = uwamonoRecords[index].body.actor;
        if (state->action == 6) {
            state->stunTime = kind;
        }
        if (state->action != 5 && state->action != 9) {
            continue;
        }
        state->trailStep = 0;
        vectorCopyPoint(route, &actor->position, actor->direction.y);
        Enemy_ActionReady(index, 2);
        Homing_Search(index);
    }
}

int Check_Actor(const Vector4 *from, int index)
{
    short i;

    for (i = 0; i < MAP_RECORD_COUNT; i++) {
        UwamonoRecord *record = &uwamonoRecords[i];
        UwamonoState *state = record->body.motions;
        float distance = Get_Distance(from, &record->position);

        if (record->kind == 1) {
            if (!((uwamonoRecords[index].body.motions->flags & 1) && i == uwamonoCurrentIndex)) {
                if (!((state->flags & 1) && index == uwamonoCurrentIndex)) {
                    if (record->extent.x != 0.0f && !(record->flags & 2) && index != i
                        && !(__builtin_fabsf(record->position.y - from->y) >= 1.0f)
                        && distance <= (uwamonoRecords[index].extent.x + record->extent.x) + actorOverlapMargin) {
                        break;
                    }
                }
            }
        }
    }
    return i == MAP_RECORD_COUNT ? -1 : i;
}

int Check_EnemyFound(void)
{
    short i;

    for (i = 0; i < MAP_RECORD_COUNT; i++) {
        UwamonoState *state = uwamonoRecords[i].body.motions;

        if (uwamonoRecords[i].kind == 1 && (state->flags & 1) && (unsigned) ((unsigned char) state->action - 4) < 2) {
            return 1;
        }
    }
    if (gameWork.releaseHandle != 0) {
        sefDeleteEffectCf(gameWork.releaseHandle);
        gameWork.releaseHandle = 0;
    }
    return 0;
}

void Enemy_FindByEar(int viewerIndex, const Vector4 *viewpoint)
{
    int i;
    UwamonoRecord *record = uwamonoRecords;
    UwamonoState *state;
    struct XenoActor *actor;
    Vector4 hit;

    for (i = 0; i < MAP_RECORD_COUNT; i++, record++) {
        if (viewerIndex == i) {
            continue;
        }
        if (record->kind != 1) {
            continue;
        }
        state = record->body.motions;
        actor = record->body.actor;
        if (state->noticeRange == 0.0f || (record->flags & 0x800000) || state->noticeCooldown > 0) {
            continue;
        }
        if (Check_Undu(i, &actor->position, viewpoint, state->id, state->groundId, &hit, 0)) {
            if (Get_Distance3D(viewpoint, &actor->position) < state->noticeRange) {
                Check_Discovery(i);
            }
        }
    }
}

void Check_Discovery(int index)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    UwamonoState *state;
    struct XenoActor *actor;
    int focus;

    state = record->body.motions;
    if (record->body.actor->kind == 0) {
        return;
    }
    actor = record->body.actor;
    if (state->flags < 0 && state->unmodeled_0078 < 0) {
        return;
    }
    if (record->flags & 0x2000000) {
        return;
    }
    if (gameWork.fadeTimer != 0 || uwamonoLadderState.state != 0 || gameWork.loadStage != 0) {
        return;
    }
    if ((gameWork.control & 0x20) && gameWork.entryKey != -1) {
        return;
    }
    if ((unsigned) ((unsigned char) state->action - 4) < 2) {
        return;
    }
    if (state->action == 9 || state->action == 6 || state->action == 0
        || state->action == 10) {
        return;
    }
    actor->enemyFlags |= 0x400000;
    if (pxyTickScaleDouble != 0 && (PadData[0].held & 0x200)) {
        return;
    }
    if (state->mode == UWAMONO_MODE_STIFF || state->mode == UWAMONO_MODE_SPLINE_START) {
        return;
    }
    Enemy_ActionReady(index, 4);
    state->focusIndex = (s16) uwamonoCurrentIndex;
    focus = uwamonoCurrentIndex;
    state->chaseTarget = uwamonoRecords[focus].position;
}

void Check_Encount(int index, signed char takeValue, int reactionValue)
{
    short sectors[2];
    unsigned char reactionTable[3][3] = { { 0, 3, 4 }, { 1, 3, 4 }, { 2, 3, 4 } };
    short candidateCount = 0;
    short i;
    UwamonoBody *slot;
    UwamonoState *state;
    struct XenoActor *actor;
    float angle;
    GameWork *work; /* lever: the pointer keeps the register allocation of the encounterTarget stores */
    int inSector;
    int slotOffset;

    if (index == -1) {
        gameWork.encounterRecord = index;
        gameWork.sceneFlags |= 1;
        gameWork.encounterFlags = reactionValue;
        gameWork.encounterTarget = 0;
        gameWork.encounterReaction = 0;
        return;
    }
    slotOffset = index * sizeof(UwamonoRecord) + 0x330;
    slot = (UwamonoBody *) ((char *) uwamonoRecords + slotOffset);
    state = slot->motions;
    actor = slot->actor;
    if (uwamonoLadderState.state != 0) {
        return;
    }
    if (takeValue == 0) {
        if (!(state->flags & 1)) {
            return;
        }
        if (gameWork.control & 0x8000) {
            return;
        }
        if (gameWork.loadStage != 0) {
            return;
        }
        if ((gameWork.control & 0x20) && gameWork.entryKey != -1) {
            return;
        }
        angle = Get_Angle_Relative(&uwamonoRecords[uwamonoCurrentIndex].position, &actor->position,
                                   uwamonoRecords[uwamonoCurrentIndex].rotation.y);
        for (;;) {
            if (Check_Angle(angle, -1.22173059f, 1.22173059f) == 1) {
                sectors[0] = 0;
                break;
            }
            inSector = Check_Angle(angle, 1.22173059f, 2.61799383f);
            if (inSector == 1) {
                sectors[0] = inSector;
                break;
            }
            inSector = Check_Angle(angle, -2.61799383f, 1.22173059f);
            if (inSector == 1) {
                sectors[0] = inSector;
            } else {
                sectors[0] = 2;
            }
            break;
        }
        angle = Get_Angle_Relative(&actor->position, &uwamonoRecords[uwamonoCurrentIndex].position,
                                   actor->rotation.y);
        for (;;) {
            if (Check_Angle(angle, -1.22173059f, 1.22173059f) != 1) {
                inSector = Check_Angle(angle, 1.22173059f, 2.61799383f);
                if (inSector == 1) {
                    sectors[1] = inSector;
                    break;
                }
                inSector = Check_Angle(angle, -2.61799383f, 1.22173059f);
                if (inSector == 1) {
                    sectors[1] = inSector;
                } else {
                    sectors[1] = 2;
                }
                break;
            }
            sectors[1] = 0;
            break;
        }
        state->reactionKind = reactionTable[sectors[0]][sectors[1]];
        gameWork.encounterReaction = (signed char) reactionTable[sectors[0]][sectors[1]];
    } else {
        gameWork.encounterReaction = reactionValue;
    }
    for (i = 0; i < 8; i++) {
        if ((short) state->setNumbers[i] != -1) {
            candidateCount++;
        }
    }
    work = &gameWork;
    if (candidateCount > 0) {
        work->encounterTarget = state->setNumbers[Get_Rnd(0, candidateCount - 1)];
    } else {
        work->encounterTarget = 0;
    }
    gameWork.encounterRecord = index;
    gameWork.sceneFlags |= 1;
    gameWork.encounterFlags = 0;
    if (uwamonoRecords[uwamonoCurrentIndex].form != 1.0f) {
        gameWork.encounterFlags = 0x800;
    }
    if ((char) state->reactionFlags & 1) {
        if (uwamonoTriggerTypeFlag == 1) {
            gameWork.encounterFlags |= 1;
        } else {
            gameWork.encounterFlags |= 2;
        }
    }
    if (gameWork.panelMode == 0) {
        gameWork.encounterFlags |= 0x20;
    }
    gameWork.encounterFlags |= state->cfEventBits;
    if (state->effect != 0) {
        sefDeleteEffectCf(state->effect);
        state->effect = 0;
    }
    if (gameWork.releaseHandle != 0) {
        sefDeleteEffectCf(gameWork.releaseHandle);
        gameWork.releaseHandle = 0;
    }
}

void Set_Spline_By_Route(int index, short step)
{
    UwamonoBody *slot = &uwamonoRecords[index].body;
    UwamonoState *state = slot->motions;
    short i;

    if (state->mode == UWAMONO_MODE_SCRIPT || state->mode == UWAMONO_MODE_START) {
        for (i = 0; i < state->pointCount; i++) {
            state->trail[i][0] = state->points[i].coords[0];
            state->trail[i][1] = state->points[i].coords[1];
            state->trail[i][2] = state->points[i].coords[2];
        }
        state->trailCount = state->pointCount;
        state->trailStep = (s16) ((state->trailCount + step) * 128 + 2);
    }
}

void Homing_Search(int index)
{
    UwamonoBody *slot = &uwamonoRecords[index].body;
    UwamonoState *state = slot->motions;
    struct XenoActor *actor = slot->actor;
    float pointDistance[64];
    short pathOrder[64];
    Vector4 straightHit;
    short j;
    short i;
    short k;
    short current = 0;
    short pathLength;
    int guard;
    float angle;
    float best;

    state->noticeCooldown = 60;
    if (!((s16) state->routeNodeCount > 0)) {
        if (state->mode != 6) {
            state->mode = UWAMONO_MODE_PAUSE;
            actor->enemyFlags &= 0x7FFFFFFF;
        }
        return;
    }
    for (k = 0; k < (s16) state->routeNodeCount; k++) {
        pointDistance[k] = Get_Distance(&actor->position, &state->homeNodes[k].point);
    }
    for (i = 0; i < (s16) (s16) state->routeNodeCount; i++) {
        best = 1000.0f;
        pathLength = -1;
        for (j = 0; j < (s16) state->routeNodeCount; j++) {
            if (pointDistance[j] < best && 0.0f < pointDistance[j]) {
                best = pointDistance[j];
                pathLength = j;
            }
        }
        pathOrder[i] = pathLength;
        pointDistance[pathLength] = 0.0f;
    }
    for (i = 0; i < (s16) state->routeNodeCount; i++) {
        if (Check_Straight_ID(actor->radius, &actor->position, &state->homeNodes[pathOrder[i]].point, 32, &straightHit, 2048) == 32) {
            current = pathOrder[i];
            break;
        }
    }
    pathOrder[0] = current;
    guard = 0;
    pathLength = 1;
    while (current != 0) {
        for (i = 0; i < state->routeLinkCounts[current]; i++) {
            if (state->routeLinks[current][i] < current) {
                current = state->routeLinks[current][i];
            }
        }
        guard++;
        if (current == 0) {
            pathOrder[pathLength++] = 0;
            break;
        }
        if (guard > 0xFFFE) {
            break;
        }
        pathOrder[pathLength++] = current;
    }
    angle = Get_Angle(&actor->position, &state->homeNodes[pathOrder[i]].point);
    actor->direction.y = angle;
    vectorCopyPoint(&state->trailPoints[0], &actor->position, angle);
    state->trailCount = 1;
    state->trailStep = 1;
    for (i = 0; i < pathLength; i++) {
        TM_Script_Spline_Add(index, state->homeNodes[pathOrder[i]].coords[0], state->homeNodes[pathOrder[i]].coords[1], state->homeNodes[pathOrder[i]].coords[2]);
    }
    Get_Angle(&state->trailPoints[state->trailCount - 2], &state->trailPoints[state->trailCount - 1]);
    actor->enemyFlags |= 0x80000000;
    Enemy_ActionReady(index, 3);
    Enemy_ActionReady(index, 2);
}
const float actorOverlapMargin = 0.100000001f;

enum {
    UWAMONO_NO_ROUTE_NODE = -1,         /* a route node with no parent */
    UWAMONO_NO_TARGET = -1,             /* the target slot when no target is held */
    UWAMONO_FLAG_CANDIDATE = 0x1000000  /* record flags word: marks a candidate target */
};

/*
 * Appends a route node at the current node count with the given position. A
 * node whose parent differs from itself records the parent; a parent other
 * than -1 also links the two nodes in each other's edge row.
 */
void Homing_Add(int index, short parent, float x, float y, float z)
{
    UwamonoState *state = uwamonoRecords[index].body.motions;
    short node = state->routeNodeCount;
    state->homeNodes[node].coords[0] = x;
    state->homeNodes[node].coords[1] = y;
    state->homeNodes[node].coords[2] = z;
    if (node != parent) {
        state->routeParents[node] = parent;
        if (parent != UWAMONO_NO_ROUTE_NODE) {
            state->routeLinks[node][state->routeLinkCounts[node]] = parent;
            state->routeLinks[parent][state->routeLinkCounts[parent]] = node;
            state->routeLinkCounts[node]++;
            state->routeLinkCounts[parent]++;
        }
    }
    state->routeNodeCount++;
}

/* the float at gp-31932 (gp = 0x6A2DF0), read as the fan limit */

int Get_MostNear_Actor(short index)
{
    struct XenoActor *actor;
    float distance;
    float nearestDistance;
    int bestIndex;
    short candidate;

    actor = uwamonoRecords[index].body.actor;
    bestIndex = -1;
    nearestDistance = 5.0f;
    for (candidate = 0; candidate < 256; candidate++) {
        if (index == candidate) {
            continue;
        }
        if (uwamonoRecords[candidate].kind != 4 && uwamonoRecords[candidate].kind != 1) {
            continue;
        }
        if ((uwamonoRecords[candidate].flags & UWAMONO_FLAG_CANDIDATE) == 0) {
            continue;
        }
        if (!Check_InsideFan(index, &actor->position, actor->rotation.y, 5.0f, &uwamonoRecords[candidate].body.actor->position, 3.14159274f, actor->radius, -1)) {
            continue;
        }
        distance = Get_Distance3D(&actor->position, &uwamonoRecords[candidate].body.actor->position);
        if (distance < nearestDistance && distance <= 8.0f) {
            nearestDistance = distance;
            bestIndex = candidate;
        }
    }
    return bestIndex;
}


void Actor_LookAt(int index)
{
    UwamonoState *state;
    UwamonoRecord *targetRecord;
    struct XenoActor *actor;
    short targetSlot;
    float x;
    float y;
    float z;

    actor = uwamonoRecords[index].body.actor;
    state = uwamonoRecords[index].body.motions;
    targetSlot = state->targetSlot;
    if (targetSlot == UWAMONO_NO_TARGET) {
        XenoActorSetFocusTarget(actor, &state->targetVector);
        return;
    }
    targetRecord = &uwamonoRecords[targetSlot];
    x = targetRecord->position.x;
    state->targetVector.x = x;
    y = targetRecord->position.y;
    state->targetVector.y = y;
    z = targetRecord->position.z;
    state->targetVector.w = 1.0f;
    state->targetVector.z = z;
    if (targetRecord->kind == 1 || targetRecord->kind == 4) {
        state->targetVector.y = y + targetRecord->extent.y;
    } else {
        state->targetVector.x = x + targetRecord->focusOffset.x;
        state->targetVector.y = y + (targetRecord->focusOffset.y + targetRecord->extent.y * 0.5f);
        state->targetVector.z = z + targetRecord->focusOffset.z;
    }
    XenoActorSetFocusTarget(actor, &state->targetVector);
}

/*
 * Offers a target to the unit at index. The offer is refused (returns 0) when
 * its priority is below the held priority. Otherwise it takes the target slot;
 * a target of -1 also stores the point as the target vector with w 1.
 * Returns 1 when the offer is taken.
 */
int Actor_LookAt_Set(int index, signed char priority, int targetSlot, Vector4 *point)
{
    UwamonoState *state;

    state = uwamonoRecords[index].body.motions;
    if (priority < state->targetPriority) {
        return 0;
    }
    if (targetSlot != UWAMONO_NO_TARGET) {
        state->targetPriority = priority;
        state->targetSlot = targetSlot;
    } else {
        state->targetSlot = targetSlot;
        state->targetVector.x = point->x;
        state->targetVector.y = point->y;
        state->targetVector.z = point->z;
        state->targetPriority = priority;
        state->targetVector.w = 1.0f;
    }
    return 1;
}

/*
 * Clears the held target when the offered threshold is not below the target
 * priority. Returns 1 when the target was cleared, 0 otherwise.
 */
int Actor_LookAt_Release(int index, signed char threshold)
{
    UwamonoState *state;

    state = uwamonoRecords[index].body.motions;
    if (threshold < state->targetPriority) {
        return 0;
    }
    state->targetPriority = 0;
    state->targetSlot = UWAMONO_NO_TARGET;
    return 1;
}

/* Drops the held target: no slot, zero priority. */
void Actor_LookAt_Init(int index)
{
    UwamonoState *state;

    state = uwamonoRecords[index].body.motions;
    state->targetSlot = UWAMONO_NO_TARGET;
    state->targetPriority = 0;
}
