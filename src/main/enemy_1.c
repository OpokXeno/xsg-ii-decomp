#include "common.h"
#include "main/get_ladder.h"
#include "main/enemy_1.h"
#include "main/set_motion.h"
#include "main/pause_menu_page_uwamono.h"
#include "main/pad.h"
#include "shared.h"
#include "main/get.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/xenoActor.h"
#include "main/xgl_font.h"
#include "main/xgl_2.h"
#include "main/xgl_render.h"
#include "main/undulate.h"
#include "main/init_uwamono_sys.h"
#include "main/enemy_system_init.h"
#include "main/sef.h"

/*
 * Lever views: a field read or written through one of these has alias set 0, which the original
 * scheduling of the functions below shows. They are kept where making the shared field a union
 * would change other files.
 */
typedef union UwamonoVectorView {
    Vector4 vector;
} UwamonoVectorView;

typedef union UwamonoWordView {
    int word;
    float value;
} UwamonoWordView;

typedef union UwamonoFlagsView {
    int flags;
    long long flags64;
} UwamonoFlagsView;

/*
 * uwamonoMoveAlongHeading reads and writes the position of its record with alias set 0, which fixes the
 * order of its loads and stores. Making UwamonoRecord.position a union would reorder the stores of
 * HitCheckMapUnitWithNyuru, so the function keeps this view of the record.
 */
typedef union UwamonoPosition {
    AlignedVector4 vector;
    u64 words[2];
} UwamonoPosition;

typedef struct UwamonoMoveRecord {
    unsigned char unmodeled_000[0x10];
    UwamonoPosition position;               /* +0x010 */
    unsigned char unmodeled_020[0x330 - 0x20];
    UwamonoBody body;                       /* +0x330 */
    unsigned char unmodeled_338[0x870 - 0x338];
} UwamonoMoveRecord;


/*
 * Starts a motion on the actor of one slot. A motion below 16 indexes the
 * state's motion table; the value splits into a mode slot (value / 100) and a
 * mode (value % 100). The actor is re-set to the channel value 4.0 and given
 * the mode unless it already holds that slot and mode.
 */
/*
 * Copies the point into the record's position when it lies within 0.1 of the
 * record and the mode is 1. Returns 0 when the point is out of reach, else 1.
 */

/*
 * Sets up the state of one slot. The record key decides whether state flag 1 is set,
 * two random byte sets are drawn from the pause and threat cycles, the
 * probe is reset and moved to the record's probe radius, and the slot enters
 * action 12.
 */
void Enemy_Init(int slotIndex)
{
    UwamonoRecord *record;
    UwamonoState *state;
    struct XenoActor *actor;
    MapCollisionObject *probe;
    s16 i;

    record = &uwamonoRecords[slotIndex];
    actor = uwamonoRecords[slotIndex].body.actor;
    state = uwamonoRecords[slotIndex].body.motions;

    actor->direction.y = actor->rotation.y;
    if ((u32) (record->key - 0x18000) < 0x101F)
    {
        state->flags |= 1;
    }
    else
    {
        state->flags &= ~1;
    }
    i = 0;
    for (; i < 8; i++)
    {
        state->pickRow[1][i] = 0;
        state->pickRow[0][i] = 0;
    }
    state->pickIndex[0] = 0;
    state->pickIndex[1] = 0;
    if (state->pauseCycle == 1)
    {
        state->pauseCycle = 2;
    }
    if (state->threatCycle == 1)
    {
        state->threatCycle = 2;
    }
    if (state->pauseCycle >= 8)
    {
        state->pauseCycle = 7;
    }
    if (state->threatCycle >= 8)
    {
        state->threatCycle = 7;
    }
    if (state->pauseCycle != 0)
    {
        state->pickRow[0][Get_Rnd(1, state->pauseCycle) % 8] = 1;
    }
    if (state->threatCycle != 0)
    {
        state->pickRow[1][Get_Rnd(1, state->threatCycle) % 8] = 1;
    }
    probe = &actor->collision;
    Enemy_ActionReady(slotIndex, 12);
    state->timer = 0;
    state->timerLimit = 0;
    UnduParamInit(probe);
    actor->collision.flags = 2;
    actor->collision.probeEnabled = 2;
    {
        float radius = record->extent.x;

        actor->collision.radius = radius;
    }
    UnduCheck(&actor->position, 0, &actor->collision);
    state->routeIndex = 0;
    state->stunTime = 0;
}

/*
 * Advances the action timer of one slot. When the timer reaches its limit the
 * actor drops flag 0x800000 and Enemy_ActionReady enters action 6 if state flag 1
 * is set, otherwise action 5.
 */
void Enemy_Damage_Explosion(int slotIndex)
{
    UwamonoBody *slot;
    UwamonoState *state;
    struct XenoActor *actor;
    u16 timer;
    int flagBit;

    slot = &uwamonoRecords[slotIndex].body;
    state = slot->motions;
    actor = slot->actor;
    timer = state->timer;
    state->timer = (u16) (timer + 1);
    if ((s16) timer < state->timerLimit)
    {
        return;
    }
    actor->enemyFlags &= 0xFF7FFFFF;
    flagBit = 1;
    if (state->reactionFlags & flagBit)
    {
        Enemy_ActionReady(slotIndex, 6);
        return;
    }
    Enemy_ActionReady(slotIndex, 5);
}

/*
 * Counts down the stun time of one slot. When the count is no longer positive
 * the slot leaves the stun: its state flag 1 and actor flag 0x01000000 are
 * cleared, the effect record is released, and the slot returns to action 2.
 */
int Enemy_Stiff(int slotIndex)
{
    UwamonoBody *slot;
    UwamonoState *state;
    struct XenoActor *actor;
    int remaining;

    slot = &uwamonoRecords[slotIndex].body;
    state = slot->motions;
    actor = slot->actor;
    remaining = state->stunTime - 1;
    state->stunTime = (u16) remaining;
    if ((s16) remaining <= 0)
    {
        state->stunTime = 0;
        state->reactionFlags &= 0xFE;
        actor->enemyFlags &= 0xFEFFFFFF;
        sefDeleteEffectCf(state->effect);
        state->effect = 0;
        Homing_Search(slotIndex);
        Enemy_ActionReady(slotIndex, 2);
    }
}

/*
 * Advances the action timer of one slot and turns its actor toward the current
 * record. When the timer reaches its limit the slot enters action 5. The actor's
 * moving angle is then set from its position toward uwamonoRecords[uwamonoCurrentIndex].
 */
void Enemy_Found(int slotIndex)
{
    UwamonoState *state;
    struct XenoActor *actor;
    u16 timer;

    actor = uwamonoRecords[slotIndex].body.actor;
    state = uwamonoRecords[slotIndex].body.motions;
    timer = state->timer;
    state->timer = (u16) (timer + 1);
    if ((s16) timer >= state->timerLimit)
    {
        Enemy_ActionReady(slotIndex, 5);
    }
    actor->direction.y = Get_Angle(&actor->position, &uwamonoRecords[uwamonoCurrentIndex].position);
}

/* Declared without a prototype: Enemy_Chase passes the slot index, the definition takes none. */
float sinf(float x);
float cosf(float x);
extern const char enemyCountDebugText[];
extern const char enemyRandomDebugText[];
extern const float uwamonoPi;
extern const float enemyRouteHalfPi;
extern const char enemySightingDebugFormat[];
extern const char enemyNewTargetDebugFormat[];
extern const char enemyAddDebugText[];
extern const char uwamonoDebugText[];
extern const char goStraightDebugText[];
extern const char enemyDistanceDebugFormat[];

/*
 * Moves one slot toward its focus record along the ring of move points. While the
 * slot has a focus, the current move point is taken from the ring head and the
 * sweep and move tests choose the next point; without a focus the slot follows
 * the nearest route point and turns to face the focus record.
 */
void Enemy_Chase(int slotIndex)
{
    UwamonoRecord *record;
    Vector4 *ringCursor;
    struct XenoActor *actor;
    UwamonoState *state;
    DoublewordAlignedVector4 startPosition;
    Vector4 hit;
    Vector4 crossPoint;
    struct { int r, g, b, a; } color;
    DoublewordAlignedVector4 squarePoint;
    DoublewordAlignedVector4 destination;
    float speed;
    float dist;
    float limit;
    int crossResult;
    int moveResult;
    const char *fmt;

    record = &uwamonoRecords[slotIndex];
    actor = uwamonoRecords[slotIndex].body.actor;
    state = uwamonoRecords[slotIndex].body.motions;

    if (gameWork.fadeTimer > 0)
    {
        Enemy_After_Battle(slotIndex);
        return;
    }
    uwamonoDrawPursuitDebug(slotIndex);

    if (record->parent != -1)
    {
        state->chaseTarget = state->path[state->pathStep];
        destination = state->chaseTarget;
        if (TM_Check_Before_Eyes(slotIndex, state->pursuitSpeed + 0.1f) == 0)
        {
            XenoActorSetBlendFrames(actor, 4.0f);
            XenoActorSetBlendLocal(actor);
            actor->motion[0].speed = 1.0f;
            XenoActorCheckMode(actor, 0);
            return;
        }
    }
    else
    {
        destination = uwamonoRecords[state->focusIndex].position;
    }

    dist = Get_Distance(&actor->position, (&state->chaseTarget));
    if (dist >= 1.0f)
    {
        speed = state->pursuitSpeed * 3.0f;
        if (record->parent != -1)
        {
            speed = speed * 8.0f;
        }
    }
    else
    {
        speed = state->pursuitSpeed;
        if (record->parent != -1)
        {
            speed = speed * 16.0f;
        }
    }

    startPosition = actor->position;
    Check_Undu(slotIndex, &actor->position, &destination, state->id, state->groundId, &hit, 0);
    crossResult = CrossPointUwamono(&actor->position, &destination, actor->radius, &crossPoint);
    if (__builtin_fabsf(hit.x - destination.x) < 0.01f && __builtin_fabsf(hit.z - destination.z) < 0.01f && crossResult == 0)
    {
        state->chaseTarget = destination;
        fmt = enemySightingDebugFormat;
        xglFontDebugPrintf(8, 100, fmt, (double) state->chaseTarget.x, (double) state->chaseTarget.y, (double) state->chaseTarget.z);
    }
    else if (crossResult == 1)
    {
        if (Check_MoveSquare(&destination, &actor->position, &squarePoint, actor->radius, state->id) == 1)
        {
            state->chaseTarget = squarePoint;
            xglFontDebugPrintf(8, 112, uwamonoDebugText);
        }
    }
    else
    {
        if (Check_MoveSquare(&destination, &actor->position, &squarePoint, actor->radius, state->id) == 1)
        {
            state->chaseTarget = squarePoint;
            xglFontDebugPrintf(8, 112, enemyNewTargetDebugFormat, state->focusIndex);
        }
    }

    state->target = state->chaseTarget;
    color.r = 255;
    color.g = 255;
    color.b = 255;
    color.a = 128;
    moveResult = uwamonoMoveAlongHeading(slotIndex, (&state->chaseTarget), speed);
    if (moveResult == 0 && (record->flags & 0x8000) != 0)
    {
        Enemy_ActionReady(slotIndex, 2);
        Homing_Search(slotIndex);
    }

    if (state->alert == 96)
    {
        if (Check_MoveSquare(&crossPoint, &actor->position, &squarePoint, actor->radius, state->id) == 1)
        {
            state->chaseTarget = squarePoint;
            xglFontDebugPrintf(8, 124, uwamonoDebugText);
        }
        else
        {
            state->chaseTarget = crossPoint;
            xglFontDebugPrintf(8, 124, goStraightDebugText);
        }
    }

    if (record->parent == -1)
    {
        if (state->leashRange <= Get_Distance(&actor->position, &uwamonoRecords[state->focusIndex].position)
            || state->areaSize - actor->radius <= Get_Distance(&state->area, &actor->position)
            || state->alert == 88 || state->alert == 64 || uwamonoLadderState.state != 0
            || (Get_Distance(&actor->position, (&state->chaseTarget)) < 0.01f && state->alert != 96 && state->alert != 99))
        {
            uwamonoClampToStopRange(slotIndex);
            state->trailStep = 0;
            vectorCopyPoint(state->trailPoints, &actor->position, ((UwamonoVectorView *) &actor->direction)->vector.y);
            state->trailCount = 1;
            Enemy_ActionReady(slotIndex, 2);
            Homing_Search(slotIndex);
            return;
        }
        if (Get_Distance(&actor->position, (&state->chaseTarget)) < 0.05f)
        {
            if (Check_MoveSquare(&state->probe, &actor->position, &squarePoint, actor->radius, state->id) == 1)
            {
                state->chaseTarget = squarePoint;
                xglFontDebugPrintf(8, 112, enemyAddDebugText);
                uwamonoMoveAlongHeading(slotIndex, (&state->chaseTarget), speed);
            }
            actor->direction.y = Get_Angle(&actor->position, &uwamonoRecords[state->focusIndex].position);
        }
    }
    else
    {
        xglFontDebugPrintf(8, 90, enemyDistanceDebugFormat, (double) Get_Distance(&actor->position, (&state->chaseTarget)));
        limit = uwamonoRecords[record->parent].extent.x * 0.0625f;
        if (limit <= Get_Distance(&actor->position, (&state->chaseTarget)))
        {
            if ((state->pathTail + 1) % UWAMONO_RING_SIZE != state->pathStep)
            {
                state->path[state->pathTail] = uwamonoRecords[record->parent].position;
                state->pathTail = (state->pathTail + 1) % UWAMONO_RING_SIZE;
            }
        }
        while (Get_Distance(&actor->position, ringCursor = &state->chaseTarget) <= speed)
        {
            state->pathStep = (state->pathStep + 1) % UWAMONO_RING_SIZE;
            *ringCursor = state->path[state->pathStep];
            if ((state->pathStep + 1) % UWAMONO_RING_SIZE == state->pathTail)
            {
                break;
            }
        }
    }
}

/* A view of the pick block of UwamonoState, which starts at +0x02. */
typedef struct UwamonoPickBlock {
    unsigned char unmodeled_00[0x60];
    signed char pickRow[2][8];   /* +0x62 in the state */
    s16 pickIndex[2];            /* +0x72 in the state */
} UwamonoPickBlock;

void Enemy_Route(int slotIndex)
{
    UwamonoBody *slot;
    UwamonoState *state;
    struct XenoActor *actor;
    UwamonoPickBlock *pickView;
    s16 pickKind;
    int pickCount;
    s16 pickRowIndex;
    int offset;
    float angle;
    float dist;

    slot = &uwamonoRecords[slotIndex].body;
    state = slot->motions;
    actor = slot->actor;
    pickCount = 0;
    state->timer++;
    if ((s16) state->timer == 1)
    {
        if (!(actor->enemyFlags & 0x80000000))
        {
            for (pickKind = 0; pickKind < 2; pickKind++)
            {
                if (pickKind == 0 && state->pauseCycle == 0)
                {
                    continue;
                }
                if (pickKind == 1 && state->threatCycle == 0)
                {
                    continue;
                }
                switch (pickKind)
                {
                case UWAMONO_MODE_INIT:
                    pickCount = state->pauseCycle;
                    break;
                case UWAMONO_MODE_PAUSE:
                    pickCount = state->threatCycle;
                    break;
                }
                offset = pickKind * 2 + 112;
                pickView = (UwamonoPickBlock *) ((u8 *) state + 2);
                pickView->pickIndex[pickKind] = (s16) ((pickView->pickIndex[pickKind] + 1) % pickCount);
                if (pickView->pickIndex[pickKind] == 0)
                {
                    if (pickCount >= 8)
                    {
                        pickCount = 8;
                    }
                    for (pickRowIndex = 0; pickRowIndex < pickCount; pickRowIndex++)
                    {
                        pickView->pickRow[pickKind][pickRowIndex] = 0;
                    }
                    pickView->pickRow[pickKind][Get_Rnd(1, pickCount - 1)] = 1;
                }
                if (pickView->pickRow[pickKind][pickView->pickIndex[pickKind]] == 1)
                {
                    switch (pickKind)
                    {
                    case UWAMONO_MODE_INIT:
                        Enemy_ActionReady(slotIndex, 1);
                        break;
                    case UWAMONO_MODE_PAUSE:
                        Enemy_ActionReady(slotIndex, 2);
                        break;
                    }
                    pickView->pickRow[0][pickView->pickIndex[pickKind]] = pickView->pickRow[1][pickView->pickIndex[pickKind]] = 0;
                    return;
                }
            }
        }

        xglFontDebugPrintf(8, 128, enemyCountDebugText);
        if (state->routeIndex == 0)
        {
            Set_Spline_By_Route(slotIndex, 0);
        }
        if (state->mode == UWAMONO_MODE_PAUSE && !(actor->enemyFlags & 0x80000000))
        {
            xglFontDebugPrintf(8, 136, enemyRandomDebugText);
            offset = Get_Rnd(0, 179);
            angle = Get_Decimal_Surplus_for_Radius((actor->direction.y - enemyRouteHalfPi) + 2.0f * (((float) offset / 360.0f) * uwamonoPi) + uwamonoPi);
            state->target.x = sinf(angle) * (state->areaSize - actor->radius) + state->area.x;
            state->target.z = cosf(angle) * (state->areaSize - actor->radius) + state->area.z;
            angle = Get_Angle(&actor->position, &state->target);
            dist = Get_Distance(&actor->position, &state->target);
            if (state->wanderStep < dist)
            {
                dist = state->wanderStep;
            }
            state->target.x = actor->position.x + sinf(angle) * dist;
            state->target.z = actor->position.z + cosf(angle) * dist;
            actor->direction.y = angle;
        }
        else
        {
            if (state->trailCount != 0)
            {
                state->routeIndex = (s16) ((state->routeIndex + 1) % state->trailCount);
                if (actor->enemyFlags < 0 && state->routeIndex == 0)
                {
                    actor->enemyFlags &= 0x7FFFFFFF;
                    Enemy_ActionReady(slotIndex, 1);
                }
            }
            state->target = state->trailPoints[state->routeIndex];
        }
    }

    if (uwamonoMoveAlongHeading(slotIndex, &state->target, state->moveSpeed) == 0)
    {
        if (state->mode == UWAMONO_MODE_SCRIPT)
        {
            uwamonoSetMotion(slotIndex, 0, 1.0f);
        }
        else
        {
            state->timer = 0;
        }
    }
    if (uwamonoCopyPointIfNear(slotIndex, &state->target, 0) == 1)
    {
        state->timer = 0;
    }
}

/*
 * Steps one slot along its route. On timer tick 1 the slot takes the route point
 * before, at or after its current index, whichever of the three is farthest from the
 * player; it reaches that point when the timer is 1. Otherwise it keeps
 * moving toward its target and steps back when the move is complete.
 */
void Enemy_Escape(int slotIndex)
{
    UwamonoState *state;
    Vector4 neighbours[3];
    float neighbourDistance[3];
    float best;
    int bestIndex;
    s16 i;
    s16 routeLength;
    s16 currentIndex;

    best = 0.0f;
    bestIndex = -1;
    state = uwamonoRecords[slotIndex].body.motions;
    state->timer++;
    if ((s16) state->timer == 1)
    {
        routeLength = state->trailCount;
        currentIndex = state->routeIndex;
        neighbours[0] = state->trailPoints[(currentIndex + routeLength - 1) % routeLength];
        neighbours[1] = state->trailPoints[currentIndex];
        neighbours[2] = state->trailPoints[(currentIndex + 1) % routeLength];
        for (i = 0; i < 3; i++)
        {
            neighbourDistance[i] = Get_Distance(&((struct XenoActor *) uwamonoRecords[uwamonoCurrentIndex].body.actor)->position, &neighbours[i]);
        }
        for (i = 0; i < 3; i++)
        {
            if (best <= neighbourDistance[i])
            {
                best = neighbourDistance[i];
                bestIndex = i;
            }
        }
        state->routePrev = state->routeIndex;
        state->routeIndex = (state->routeIndex + state->trailCount + bestIndex - 1) % state->trailCount;
        state->target = state->trailPoints[state->routeIndex];
        if (bestIndex == 1)
        {
            state->timer = 0;
            return;
        }
    }
    if (uwamonoMoveAlongHeading(slotIndex, &state->target, state->moveSpeed) == 0)
    {
        uwamonoSetMotion(slotIndex, 0, 1.0f);
        state->timer = 1;
        state->routeIndex = state->routePrev;
        state->target = state->trailPoints[state->routeIndex];
    }
    if (uwamonoCopyPointIfNear(slotIndex, &state->target, 1) == 1)
    {
        state->timer = 0;
    }
}

extern s16 enemyRouteChaseDelayCount;

void Enemy_Route_Chase(int slotIndex)
{
    UwamonoBody *slot = &uwamonoRecords[slotIndex].body;
    UwamonoState *state = slot->motions;
    struct XenoActor *actor = slot->actor;
    Vector4 splinePoint;
    float probeDistance[3];
    int splineSteps;
    int motion;
    s16 nearest;
    s16 j;

    if (state->trailCount > 0)
    {
        for (j = 0; j < 3; j++)
        {
            s16 loopCount = state->trailCount;
            splineSteps = loopCount * UWAMONO_SPLINE_STEPS;
            GetBSplineLoop((s16) ((state->trailStep + splineSteps + j - 1) % splineSteps), loopCount, UWAMONO_SPLINE_STEPS, state->trailPoints, &splinePoint);
            probeDistance[j] = Get_Distance(&state->probe, &splinePoint);
        }
        if (probeDistance[0] < probeDistance[1])
        {
            nearest = probeDistance[0] < probeDistance[2] ? 0 : 2;
        }
        else
        {
            nearest = probeDistance[1] < probeDistance[2] ? 1 : 2;
        }
        state->trailStep = (s16) ((state->trailStep + state->trailCount * UWAMONO_SPLINE_STEPS + nearest - 1) % (state->trailCount * UWAMONO_SPLINE_STEPS));
        GetBSplineLoop(state->trailStep, state->trailCount, UWAMONO_SPLINE_STEPS, state->trailPoints, &splinePoint);
        if (nearest == 1)
        {
            actor->direction.y = Get_Angle(&actor->position, &state->probe);
            enemyRouteChaseDelayCount = enemyRouteChaseDelayCount - 1;
        }
        else
        {
            enemyRouteChaseDelayCount = 16;
            actor->direction.y = Get_Angle(&actor->position, &splinePoint);
        }
        if (!(0.012f <= Get_Distance(&actor->position, &splinePoint)) && enemyRouteChaseDelayCount <= 0)
        {
            motion = 1;
        }
        else
        {
            motion = 2;
        }
        uwamonoSetMotion(slotIndex, motion, 1.0f);
        actor->position.x = splinePoint.x;
        actor->position.z = splinePoint.z;
    }
}


/* JP: 目撃(%8f,%8f,%8f)
 * EN: Sighted (%8f,%8f,%8f) */
const char enemySightingDebugFormat[24] = "\314\334\267\342(%8f,%8f,%8f)";

/* JP: 新規 %3d
 * EN: New %3d */
const char enemyNewTargetDebugFormat[16] = "\277\267\265\254 %3d";

/* JP: 距離 %8f
 * EN: Distance %8f */
const char enemyDistanceDebugFormat[16] = "\265\367\316\245 %8f";

/* JP: カウント
 * EN: Count */
const char enemyCountDebugText[16] = "\245\253\245\246\245\363\245\310";

/* JP: ランダム
 * EN: Random */
const char enemyRandomDebugText[24] = "\245\351\245\363\245\300\245\340";

void Enemy_Pause(int slotIndex)
{
    UwamonoState *state = uwamonoRecords[slotIndex].body.motions;
    struct XenoActor *actor = uwamonoRecords[slotIndex].body.actor;
    u16 timer;

    if (state->mode == UWAMONO_MODE_PURSUE || state->mode == UWAMONO_MODE_STIFF)
    {
        if (actor->enemyFlags & 0x400000)
        {
            actor->direction.y = Get_Angle(&actor->position, &uwamonoRecords[state->focusIndex].position);
        }
    }
    timer = state->timer;
    state->timer = (u16) (timer + 1);
    if (state->timerLimit - 1 > (s16) timer)
    {
        return;
    }
    switch (state->mode)
    {
    case UWAMONO_MODE_PURSUE:
    case UWAMONO_MODE_STIFF:
        Enemy_ActionReady(slotIndex, 1);
        return;
    case UWAMONO_MODE_PAUSE:
        Enemy_ActionReady(slotIndex, 3);
        state->trailStep = 0;
        state->routeIndex = 0;
        state->timer = 0;
        return;
    case UWAMONO_MODE_SCRIPT:
        Enemy_ActionReady(slotIndex, 3);
        state->trailStep = 0;
        state->timer = 0;
        return;
    case UWAMONO_MODE_START:
        Enemy_ActionReady(slotIndex, 10);
        return;
    case UWAMONO_MODE_SPLINE_START:
        Enemy_ActionReady(slotIndex, 11);
        return;
    default:
    {
        int roll = xglSRand() & 1;

        switch (roll)
        {
        case UWAMONO_MODE_INIT:
            Enemy_ActionReady(slotIndex, 3);
            return;
        case UWAMONO_MODE_PAUSE:
            Enemy_ActionReady(slotIndex, 2);
            return;
        }
        return;
    }
    }
}

/*
 * Ticks the timer of one slot. When the timer reaches its limit the action is
 * set to 3. Kind 8 also clears the step; any other kind clears the pending
 * count, and an actionFlags bit 0x10 advances the step by 2 first.
 */
void Enemy_Threat(int slotIndex)
{
    UwamonoBody *slot = &uwamonoRecords[slotIndex].body;
    UwamonoState *state = slot->motions;
    u16 timer = state->timer;

    state->timer = (u16) (timer + 1);
    if (state->timerLimit - 1 > (s16) timer)
    {
        return;
    }
    if (state->mode == UWAMONO_MODE_SCRIPT)
    {
        Enemy_ActionReady(slotIndex, 3);
        state->trailStep = 0;
        state->timer = 0;
        return;
    }
    Enemy_ActionReady(slotIndex, 3);
    if (state->flags & 0x10)
    {
        do
        {
            state->trailStep += 2;
        } while (0);
        state->timer = 0;
    }
    else
    {
        state->timer = 0;
    }
    (*(u16 *) &state->routeIndex) = 0;
}

/*
 * Ticks the timer of one slot. At its limit the script cursor advances and the
 * action of actionTable for the pending count and the cursor is entered: -1 sets
 * action 3 and runs the expiry tail, any other value sets action 8.
 */
void Enemy_Unique(int slotIndex)
{
    UwamonoBody *slot = &uwamonoRecords[slotIndex].body;
    UwamonoState *state = slot->motions;
    u16 timer = state->timer;
    s16 scriptAction;

    state->timer = (u16) (timer + 1);
    if (state->timerLimit - 1 > (s16) timer)
    {
        return;
    }
    state->motionCol = (u16) (state->motionCol + 1);
    scriptAction = state->actionMotions[state->routeIndex][(s16) state->motionCol];
    if (scriptAction != -1)
    {
        Enemy_ActionReady(slotIndex, 8);
        return;
    }
    Enemy_ActionReady(slotIndex, 3);
    if (state->flags & 0x10)
    {
        do
        {
            state->trailStep += 2;
        } while (0);
        state->timer = 0;
    }
    else
    {
        state->timer = 0;
    }
    state->routeIndex = 0;
}

extern const float enemyChaseContactMargin;
extern const float enemyChaseContactHeightScale;
extern const float enemyEncounterMargin;
extern const float enemyEncounterHeightScale;
extern char enemyMotionSpeedDebugFormat[];
/* * Sets up the state of one slot. The record key decides whether the slot's motion * table keeps flag 1, two random byte sets are drawn from the pick counts, the * probe is reset and moved to the record's probe radius, and the slot enters * action 12. */ void Enemy_Init(int slotIndex);
void Enemy_Pause(int slotIndex);
/* * Ticks the timer of one slot. When the timer reaches its limit the action is * set to 3. Kind 8 also clears the step; any other kind clears the pending * count, and an actionFlags bit 0x10 advances the step by 2 first. */ void Enemy_Threat(int slotIndex);
void Enemy_Route(int slotIndex);
/* * Advances the action timer of one slot and turns its actor toward the current * record. When the timer reaches its limit the slot enters action 5. The actor's * moving angle is then set from its position toward uwamonoRecords[uwamonoCurrentIndex]. */ void Enemy_Found(int slotIndex);
/* * Moves one slot toward its focus record along the ring of move points. While the * slot has a focus, the current move point is taken from the ring head and the * sweep and move tests choose the next point; without a focus the slot follows * the nearest route point and turns to face the focus record. */ void Enemy_Chase(int slotIndex);
/* * Counts down the stun time of one slot. When the count is no longer positive * the slot leaves the stun: its state flag 1 and actor flag 0x01000000 are * cleared, the effect record is released, and the slot returns to action 2. */ int Enemy_Stiff(int slotIndex);
/* * Ticks the timer of one slot. At its limit the script cursor advances and the * action of actionTable for the pending count and the cursor is entered: -1 sets * action 3 and runs the expiry tail, any other value sets action 8. */ void Enemy_Unique(int slotIndex);
/* * Steps one slot along its route. On timer tick 1 the slot takes the route point * before, at or after its current index, whichever of the three is farthest from the * player; it reaches that point when the timer is 1. Otherwise it keeps * moving toward its target and steps back when the move is complete. */ void Enemy_Escape(int slotIndex);
void Enemy_Route_Chase(int slotIndex);

void ACT_updateEnemy(int index)
{
    UwamonoRecord *record;
    UwamonoState *state;
    struct XenoActor *actor;
    struct XenoActor *player;

    record = &uwamonoRecords[index];
    actor = uwamonoRecords[index].body.actor;
    state = uwamonoRecords[index].body.motions;
    player = uwamonoRecords[uwamonoCurrentIndex].body.actor;
    if (gameWork.control & 1)
    {
        return;
    }
    state->alert = 99;
    uwamonoDrawPointPath(index);
    uwamonoDrawStateLinks(index);
    if (state->noticeCooldown > 0)
    {
        state->noticeCooldown--;
    }
    if (uwamonoCurrentIndex == index)
    {
        return;
    }
    if (record->flags & 0x2000000)
    {
        return;
    }
    if (state->targetPriority == 3)
    {
        Actor_LookAt(index);
    }
    if (((UwamonoFlagsView *) &record->flags)->flags64 & 0xC00000)
    {
        actor->collision.block = UnduDataGetHeader(0x100, -1);
        if (record->flags & 0x800000)
        {
            actor->motion[0].speed = 0.0f;
        }
        else
        {
            actor->motion[0].speed = 1.0f;
        }
        if (record->body.motions->mode == UWAMONO_MODE_INIT)
        {
            return;
        }
        if (Get_Distance(&actor->position, &uwamonoRecords[uwamonoCurrentIndex].body.actor->position) <=
                actor->radius + uwamonoRecords[uwamonoCurrentIndex].body.actor->radius + enemyChaseContactMargin &&
            __builtin_fabsf(actor->position.y - uwamonoRecords[uwamonoCurrentIndex].body.actor->position.y) <
                uwamonoRecords[uwamonoCurrentIndex].form * enemyChaseContactHeightScale &&
            gameWork.fadeTimer == 0)
        {
            Check_Encount(index, 0, 0);
        }
        if (record->flags & 0x400)
        {
            actor->rotation.y = record->rotation.y = Get_Cursol_by_Reduce_Speed_Angle_Loop(actor->rotation.y, actor->direction.y, 6.0f);
        }
        return;
    }
    if (record->body.motions->mode == UWAMONO_MODE_INIT)
    {
        return;
    }
    if (state->action != 6 && state->action != 9)
    {
        state->reactionFlags &= 0xFE;
        if (state->effect != 0)
        {
            sefDeleteEffectCf(state->effect);
            state->effect = 0;
        }
    }
    switch (state->action)
    {
    case UWAMONO_MODE_INIT:
        Enemy_Init(index);
    case UWAMONO_MODE_PAUSE:
        Enemy_Pause(index);
        break;
    case UWAMONO_MODE_THREAT:
        Enemy_Threat(index);
        break;
    case UWAMONO_MODE_ROUTE:
        Enemy_Route(index);
        break;
    case UWAMONO_MODE_AIM:
        Enemy_Found(index);
        break;
    case UWAMONO_MODE_PURSUE:
        Enemy_Chase(index);
        break;
    case UWAMONO_MODE_STIFF:
        Enemy_Stiff(index);
        break;
    case UWAMONO_MODE_SCRIPT:
        Enemy_Unique(index);
        break;
    case UWAMONO_MODE_EXPLOSION:
        Enemy_Damage_Explosion(index);
        break;
    case UWAMONO_MODE_ESCAPE:
        Enemy_Escape(index);
        break;
    case UWAMONO_MODE_SPLINE:
        Enemy_Route_Chase(index);
        break;
    }
    DispPillar(&state->area, state->areaSize, 0.0f, 2);
    if (state->fanLength != 0.0f && !(record->flags & 0x2000000) && state->noticeCooldown <= 0 &&
        uwamonoRecordsOverlap(uwamonoCurrentIndex, index) == 1.0f)
    {
        if (Check_InsideFan(index, &record->body.actor->position, actor->rotation.y, state->fanLength,
                            &uwamonoRecords[uwamonoCurrentIndex].body.actor->position, state->fanWidth, actor->radius, state->id) == 1)
        {
            Check_Discovery(index);
        }
        uwamonoDrawFanEdge(&record->position, actor->rotation.y - state->fanWidth * 0.5f, state->fanLength, 1);
        uwamonoDrawFanEdge(&record->position, actor->rotation.y + state->fanWidth * 0.5f, state->fanLength, 1);
    }
    if (pxyTickScaleDouble != 0)
    {
        if ((PadData[0].pressed & 4) || gameWork.fadeTimer != 0)
        {
            if (state->action == 5)
            {
                Enemy_After_Battle(index);
            }
        }
    }
    if (Get_Distance(&actor->position, &player->position) <= actor->radius + player->radius + enemyEncounterMargin &&
        __builtin_fabsf(actor->position.y - uwamonoRecords[uwamonoCurrentIndex].body.actor->position.y) <
            uwamonoRecords[uwamonoCurrentIndex].form * enemyEncounterHeightScale &&
        gameWork.fadeTimer == 0)
    {
        Check_Encount(index, 0, 0);
    }
    actor->rotation.y = record->rotation.y = Get_Cursol_by_Reduce_Speed_Angle_Loop(actor->rotation.y, actor->direction.y, 6.0f);
    xglFontDebugPrintf(8, 160, enemyMotionSpeedDebugFormat, (double) actor->motion[0].speed);
}

extern const char uwamonoFallingDebugText[];

/*
 * Steps the record of one slot toward the target along the heading to it, at
 * the given speed. The step is swept against the map; a hit on a map unit or on
 * another record puts the record back where it was. Returns 1 when the record
 * moved freely (the actor then walks or runs by the step length), 0 otherwise.
 */
/* JP: おちてる？
 * EN: Falling? */
const char uwamonoFallingDebugText[16] = "\244\252\244\301\244\306\244\353\241\251";

int uwamonoMoveAlongHeading(int index, Vector4 *target, float speed)
{
    UwamonoMoveRecord *record = (UwamonoMoveRecord *) &uwamonoRecords[index];
    DoublewordAlignedVector4 *position;
    struct { float x, y, z; } delta;
    Vector4 origin;
    DoublewordAlignedVector4 saved = record->position.vector;
    UwamonoState *state = record->body.motions;
    struct XenoActor *actor = record->body.actor;
    UwamonoPosition swept;
    Vector4 step;
    float heading;
    float distance;
    int hit;
    float ground;
    float zero = 0.0f;

    origin.z = zero;
    origin.y = zero;
    origin.x = zero;
    origin.w = 1.0f;
    position = &record->position.vector;
    heading = Get_Angle(position, target);
    delta.x = sinf(heading) * speed;
    delta.y = zero;
    delta.z = cosf(heading) * speed;
    step.x = delta.x;
    step.y = delta.y;
    step.z = delta.z;
    step.w = 1.0f;
    distance = Get_Distance(&origin, &step);
    position->x += step.x;
    record->position.vector.w = 1.0f;
    record->position.vector.y += step.y;
    record->position.vector.z += step.z;
    if (Check_Undu(index, &actor->position, position, state->id,
                       state->groundId, &swept.vector, 0x303) == 0)
    {
        ((UwamonoWordView *) &actor->direction.y)->value = heading;
        record->position = swept;
        actor->position = record->position.vector;
        actor->position.y = record->position.vector.y =
            unduGetSlotHeight(index, position, state->id,
                          state->groundId, 0);
        return 0;
    }
    hit = HitCheckMapUnitPosSize(position, actor->radius);
    if (hit != -1)
    {
        ((UwamonoWordView *) &actor->direction.y)->value = heading;
        record->position.vector = saved;
        actor->position = record->position.vector;
        return 0;
    }
    if (uwamonoFindOverlapRecord(index, position, 1.0f) != hit)
    {
        ((UwamonoWordView *) &actor->direction.y)->value = heading;
        if (state->action != 10)
        {
            record->position.vector = saved;
            actor->position = record->position.vector;
        }
        return 0;
    }
    ((UwamonoWordView *) &actor->direction.y)->value = heading;
    record->position = swept;
    actor->position = record->position.vector;
    ground = unduGetSlotHeight(index, position, state->id,
                           state->groundId, 0);
    if (ground != -1000.0f)
    {
        record->position.vector.y = ground;
        actor->position.y = ground;
    }
    else
    {
        xglFontDebugPrintf(8, 128, uwamonoFallingDebugText);
    }
    XenoActorSetBlendLocal(actor);
    uwamonoSetMotion(index, distance <= 0.135f ? 1 : 2, 1.0f);
    return 1;
}

/*
 * Copies the point into the record's position when it lies within 0.1 of the
 * record and the mode is 1. Returns 0 when the point is out of reach, else 1.
 */
int uwamonoCopyPointIfNear(int index, const Vector4 *point, signed char copyFlag)
{
    UwamonoRecord *record = &uwamonoRecords[index];

    if (!(Get_Distance(&record->position, point) < 0.1f))
    {
        return 0;
    }
    if (copyFlag == 1)
    {
        record->position = *point;
    }
    return 1;
}


/*
 * Probes the point at the given length ahead of the slot's actor. A ready
 * record (kind 1 or 4) other than the slot's own that overlaps it and lies
 * within the two sizes of the point becomes the target (scan state 0).
 * Otherwise the map is probed at the point: scan state 64 when it finds no
 * ground, else 99. Returns 1 only in the last case.
 */
int TM_Check_Before_Eyes(int slotIndex, float length)
{
    UwamonoBody *slot;
    UwamonoRecord *slotRecord;
    struct XenoActor *actor;
    UwamonoState *state;
    Vector4 point;
    short i;
    int offset;

    slotRecord = &uwamonoRecords[slotIndex];
    actor = uwamonoRecords[slotIndex].body.actor;
    state = uwamonoRecords[slotIndex].body.motions;
    Get_Point_By_AngleLength(&actor->position, &point, actor->direction.y, length);
    for (i = 0; i < MAP_RECORD_COUNT; i++)
    {
        if (uwamonoRecords[i].kind == 0 || uwamonoRecords[i].kind == 5)
        {
            continue;
        }
        if (uwamonoRecords[i].kind == 1 || uwamonoRecords[i].kind == 4)
        {
            if (slotIndex != i && uwamonoRecordsOverlap(slotIndex, i) != 0.0f)
            {
                if (Get_Distance(&point, &uwamonoRecords[i].position) <= uwamonoRecords[slotIndex].extent.x + uwamonoRecords[i].extent.x)
                {
                    state->targetRecord = i;
                    state->scanState = UWAMONO_SCAN_RECORD;
                    return 0;
                }
            }
        }
    }
    actor->collision.block = UnduDataGetHeader(state->id, state->groundId);
    actor->collision.radius = slotRecord->extent.x;
    actor->collision.probeEnabled = 2;
    actor->collision.flags = 0x303;
    if (UnduCheck(&point, 0, &actor->collision) == -1000.0f)
    {
        state->scanState = UWAMONO_SCAN_NO_GROUND;
        return 0;
    }
    state->scanState = UWAMONO_SCAN_GROUND;
    return 1;
}

/*
 * Starts a motion on the actor of one slot. A motion below 16 indexes the
 * state's motion table; the value splits into a mode slot (value / 100) and a
 * mode (value % 100). The actor is re-set to the channel value 4.0 and given
 * the mode unless it already holds that slot and mode.
 */
void uwamonoSetMotion(int slotIndex, int motion, float speed)
{
    UwamonoBody *slot = &uwamonoRecords[slotIndex].body;
    struct XenoActor *actor = slot->actor;
    UwamonoState *state = slot->motions;

    if (motion < 16)
    {
        motion = state->walk[motion];
    }
    actor->motion[0].speed = speed;
    if (actor->slot == motion / 100 && actor->mode == motion % 100)
    {
        return;
    }
    XenoActorSetBlendFrames(actor, 4.0f);
    XenoActorSetModeInSlot(actor, motion / 100, motion % 100);
}

static inline UwamonoRecord *uwamonoRecordAt(int index)
{
    return &uwamonoRecords[index];
}

void Enemy_ActionReady(int slot_index, signed char action)
{
    UwamonoRecord *record = uwamonoRecordAt(slot_index);
    UwamonoState *state = uwamonoRecords[slot_index].body.motions;
    struct XenoActor *actor = uwamonoRecords[slot_index].body.actor;

    if (record->kind != 1)
    {
        return;
    }
    state->action = action;
    state->timer = 0;
    XenoActorSetBlendFrames(actor, 4.0f);
    XenoActorSetBlendLocal(actor);
    switch (action)
    {
    case UWAMONO_MODE_ROUTE:
        uwamonoSetMotion(slot_index, 1, 1.0f);
        break;
    case UWAMONO_MODE_PAUSE:
        state->timerLimit = state->pauseTime;
        uwamonoSetMotion(slot_index, 0, 1.0f);
        break;
    case UWAMONO_MODE_START:
        if ((s16) state->pauseTime > 0)
        {
            state->timerLimit = state->pauseTime;
            uwamonoSetMotion(slot_index, 0, 1.0f);
            state->action = 1;
        }
        break;
    case UWAMONO_MODE_THREAT:
        if ((s16) state->threatTime > 0)
        {
            state->timerLimit = state->threatTime;
            uwamonoSetMotion(slot_index, 4, 1.0f);
        }
        break;
    case UWAMONO_MODE_AIM:
        state->timerLimit = 14;
        uwamonoSetMotion(slot_index, 3, 1.0f);
        if (state->flags & 1)
        {
            if (gameWork.releaseHandle == 0)
            {
                gameWork.releaseHandle = sefCreateEffectCf(0x10000021, 0, 0);
            }
        }
        if (state->seIds[0] != -1)
        {
            seVoiceStartDefault(0x03000000 + state->seIds[Get_Rnd(0, 2)]);
        }
        break;
    case UWAMONO_MODE_PURSUE:
        state->timerLimit = 30;
        state->unmodeled_007a = 0;
        actor->enemyFlags = (actor->enemyFlags | 0x200000) & 0x7FFFFFFF;
        uwamonoSetMotion(slot_index, 2, 1.0f);
        break;
    case UWAMONO_MODE_SCRIPT:
    {
        state->timerLimit = state->actionLengths[state->routeIndex][state->motionCol];
        uwamonoSetMotion(slot_index, state->actionMotions[state->routeIndex][state->motionCol], 1.0f);
        break;
    }
    case UWAMONO_MODE_EXPLOSION:
    {
        SefEffectRecord *effect;
        float formScale;

        state->reactionFlags |= 1;
        state->stunTime += 300;
        state->timerLimit = 30;
        actor->enemyFlags &= 0xFEFFFFFF;
        if (state->effect != 0)
        {
            sefDeleteEffectCf(state->effect);
        }
        effect = sefCreateEffectCf(0x10000037, 0, 0);
        sefSetRecordOwner(effect, actor);
        state->effect = effect;
        formScale = uwamonoRecords[uwamonoCurrentIndex].form;
        effect->scale.z = formScale;
        effect->scale.y = formScale;
        effect->scale.x = formScale;
        actor->motion[0].speed = 0.0f;
        XenoActorResetChannelFlags(actor);
        break;
    }
    case UWAMONO_MODE_STIFF:
        actor->motion[0].speed = 0.0f;
        break;
    default:
        break;
    }
}

/* JP: 上物
 * EN: Field object */
const char uwamonoDebugText[8] = "\276\345\312\252";

/* JP: 直進
 * EN: Go straight */
const char goStraightDebugText[8] = "\304\276\277\312";

/* JP: 追加
 * EN: Add */
const char enemyAddDebugText[6] = "\304\311\262\303";

s16 enemyRouteChaseDelayCount = 0;

char enemyMotionSpeedDebugFormat[4] = "%8f";

const float uwamonoPi = 3.14159274f;
const float enemyRouteHalfPi = 1.57079637f;
const float enemyChaseContactMargin = 0.300000012f;
const float enemyChaseContactHeightScale = 1.70000005f;
const float enemyEncounterMargin = 0.100000001f;
const float enemyEncounterHeightScale = 1.70000005f;
