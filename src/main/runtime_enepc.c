#include "common.h"
#include "main/enemy_1.h"
#include "main/set_motion.h"
#include "shared.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/runtime.h"
#include "main/runtime_enepc.h"
#include "main/xenoActor.h"
#include "main/game_camera.h"
#include "main/vibration.h"
#include "main/uwamono_objx.h"
#include "main/ytachi_test.h"
#include "main/res_get_path.h"
#include "main/get.h"
#include "main/get_ladder.h"
#include "main/init_uwamono_sys.h"
#include "main/sef.h"

#define NULL ((void *) 0)


/* A path point of a script array: integer coordinates in script units (x 100). */
typedef struct EnepcPathPoint {
    int x;
    int y;
    int z;
} EnepcPathPoint;

/* A route node of a script array: coordinates as above and the node id. */
typedef struct EnepcRouteNode {
    int x;
    int y;
    int z;
    short node;
    unsigned char unmodeled_0e[2];
} EnepcRouteNode;

/* One preset of enepcParamSets, handed to enepcStoreParams in its argument order.
   movementFloats holds the wander step, leash range, move speed and pursuit speed (paramA0..3),
   rangeFloats the fan length and notice range (paramB0..1), and stateShorts the wander time,
   pick count A, wait time and pick count B (shortParam0..3), as enepcStoreParams stores them. */
typedef struct EnepcParamSet {
    float height;                           /* +0x00 */
    float extent;                           /* +0x04 */
    float movementFloats[4];                   /* +0x08 */
    float angleDegrees;                     /* +0x18 */
    float rangeFloats[2];                 /* +0x1C */
    unsigned char unmodeled_24[4];
    short stateShorts[4];                   /* +0x28 */
} EnepcParamSet;

extern struct EnepcParamSet *enepcParamSets;

void enepcStoreParams(int index, float height, float extent,
                      float paramA0, float paramA1, float paramA2, float paramA3,
                      float angleDegrees, float paramB0, float paramB1,
                      short shortParam0, short shortParam1, short shortParam2, short shortParam3,
                      float modeValue3, short motion, float shadowHeightA, float shadowHeightB,
                      int initState);

/* Native "setParams", 7 arguments: argument 1 is the record key, 3 a preset of
   enepcParamSets (absent: none), 4 and 5 a path point array and its count, 6 and 7 a
   route node array and its count.  Argument 2 is read but not used.  A path in route
   mode 9 is mirrored back onto itself (mode 8). */
int enepcSetParams(SbThread *call)
{
    int key;
    int index;
    int paramSet;
    EnepcPathPoint *points;
    int pointCount;
    EnepcRouteNode *nodes;
    int nodeCount;
    UwamonoState *state;
    short shortParam3;
    int i;
    int j;

    if (VM_PLUGIN_CNT_GET(call) < 7) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    key = vmArgGet(call, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    if (index == -1) {
        return 1;
    }
    /* Argument 2 is read and discarded (the original calls vmArgGet(call, 2)
       and never uses the result). */
    vmArgGet(call, 2);
    if (vmArgTagIsZero(call, 3) == 0) {
        paramSet = vmArgGet(call, 3);
    } else {
        paramSet = -1;
    }
    if (vmArgTagIsZero(call, 4) == 0) {
        points = vmEntryGet(call, vmArgGet(call, 4));
    } else {
        points = NULL;
    }
    pointCount = vmArgGet(call, 5);
    if (vmArgTagIsZero(call, 6) == 0) {
        nodes = vmEntryGet(call, vmArgGet(call, 6));
    } else {
        nodes = NULL;
    }
    nodeCount = vmArgGet(call, 7);
    state = uwamonoRecords[index].body.motions;
    if (points != NULL) {
        state->pointCount = pointCount;
        for (i = 0; i < state->pointCount; i++) {
            state->points[i].coords[0] = (float) points[i].x / 100.0f;
            state->points[i].coords[1] = (float) points[i].y / 100.0f;
            state->points[i].coords[2] = (float) points[i].z / 100.0f;
        }
        if (state->mode == 9) {
            for (i = 0; i < state->pointCount - 2; i++) {
                state->points[state->pointCount + i].coords[0] = state->points[state->pointCount - i - 2].coords[0];
                state->points[state->pointCount + i].coords[2] = state->points[state->pointCount - i - 2].coords[2];
            }
            state->pointCount = state->pointCount * 2 - 2;
            /* The original tests mode against 9 a second time here (lbu 0x5F reloaded,
               bne at enepcSetParams +0x280). */
            if (state->mode == 9) {
                state->mode = 8;
            }
        }
        for (j = 0; j < state->pointCount; j++) {
            for (i = 0; i < 4; i++) {
                state->actionMotions[j][i] = -1;
                state->actionLengths[j][i] = 0;
            }
        }
    }
    if (nodes != NULL) {
        state->routeNodeCount = 0;
        for (i = 0; i < nodeCount; i++) {
            Homing_Add(index, nodes[i].node, (float) nodes[i].x / 100.0f,
                                (float) nodes[i].y / 100.0f, (float) nodes[i].z / 100.0f);
        }
    }
    Set_Spline_By_Route(index, 0);
    if (paramSet != -1 && enepcParamSets != NULL) {
        /* The fourth short of the preset is read before the flag is cleared: the
           original loads it ahead of every other argument. */
        shortParam3 = enepcParamSets[paramSet].stateShorts[3];
        state->flags &= ~4;
        enepcStoreParams(index, enepcParamSets[paramSet].height, enepcParamSets[paramSet].extent,
                         enepcParamSets[paramSet].movementFloats[0], enepcParamSets[paramSet].movementFloats[1],
                         enepcParamSets[paramSet].movementFloats[2], enepcParamSets[paramSet].movementFloats[3],
                         enepcParamSets[paramSet].angleDegrees,
                         enepcParamSets[paramSet].rangeFloats[0], enepcParamSets[paramSet].rangeFloats[1],
                         enepcParamSets[paramSet].stateShorts[0], enepcParamSets[paramSet].stateShorts[1],
                         enepcParamSets[paramSet].stateShorts[2], shortParam3,
                         -1000.0f, -1, -1000.0f, -1000.0f, 1);
    }
    return 1;
}

/* uwamono_objx.c's second, byte-identical copy of uwamonoFindIndexByShortKey; the original
   calls this copy here (jal uwamonoFindIndexByKey). */

/* Native "setParamsAll", at least 15 arguments (fewer: error 2, returns 0).
   Argument 1 is the record key; the others are optional: arguments 2 to 10 are
   the floats of enepcStoreParams in script units (x 100; arguments 6 and 7 also
   x 1000), 11 to 14 its shorts, 16, 18 and 19 its actor floats, 17 its motion; -1 or
   -1000.0 when absent (vmArgTagIsZero nonzero).  Argument 15, when not -1, sets bit 0x4 of
   the state flags; argument 20 absent makes enepcStoreParams run Enemy_Init. */
int enepcSetParamsAll(SbThread *call)
{
    int key;
    int index;
    float height;
    float extent;
    float paramA0;
    float paramA1;
    float paramA2;
    float paramA3;
    float angleDegrees;
    float paramB0;
    float paramB1;
    short shortParam0;
    short shortParam1;
    short shortParam2;
    short shortParam3;
    signed char lockFlag;
    float modeValue3;
    short motion;
    float shadowHeightA;
    float shadowHeightB;
    int initState;
    UwamonoRecord *record;
    UwamonoState *state;

    if (VM_PLUGIN_CNT_GET(call) < 15) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    key = vmArgGet(call, 1);
    index = uwamonoFindIndexByKey((short) key);
    if (index == -1 || uwamonoRecords[index].body.actor == NULL) {
        return 1;
    }
    if (vmArgTagIsZero(call, 2) == 0) {
        height = (float) vmArgGet(call, 2) / 100.0f;
    } else {
        height = -1.0f;
    }
    if (vmArgTagIsZero(call, 3) == 0) {
        extent = (float) vmArgGet(call, 3) / 100.0f;
    } else {
        extent = -1.0f;
    }
    if (vmArgTagIsZero(call, 4) == 0) {
        paramA0 = (float) vmArgGet(call, 4) / 100.0f;
    } else {
        paramA0 = -1.0f;
    }
    if (vmArgTagIsZero(call, 5) == 0) {
        paramA1 = (float) vmArgGet(call, 5) / 100.0f;
    } else {
        paramA1 = -1.0f;
    }
    if (vmArgTagIsZero(call, 6) == 0) {
        paramA2 = (float) vmArgGet(call, 6) / 100.0f / 1000.0f;
    } else {
        paramA2 = -1.0f;
    }
    if (vmArgTagIsZero(call, 7) == 0) {
        paramA3 = (float) vmArgGet(call, 7) / 100.0f / 1000.0f;
    } else {
        paramA3 = -1.0f;
    }
    if (vmArgTagIsZero(call, 8) == 0) {
        angleDegrees = (float) vmArgGet(call, 8) / 100.0f;
    } else {
        angleDegrees = -1.0f;
    }
    if (vmArgTagIsZero(call, 9) == 0) {
        paramB0 = (float) vmArgGet(call, 9) / 100.0f;
    } else {
        paramB0 = -1.0f;
    }
    if (vmArgTagIsZero(call, 10) == 0) {
        paramB1 = (float) vmArgGet(call, 10) / 100.0f;
    } else {
        paramB1 = -1.0f;
    }
    if (vmArgTagIsZero(call, 11) == 0) {
        shortParam0 = vmArgGet(call, 11);
    } else {
        shortParam0 = -1;
    }
    if (vmArgTagIsZero(call, 12) == 0) {
        shortParam1 = vmArgGet(call, 12);
    } else {
        shortParam1 = -1;
    }
    if (vmArgTagIsZero(call, 13) == 0) {
        shortParam2 = vmArgGet(call, 13);
    } else {
        shortParam2 = -1;
    }
    if (vmArgTagIsZero(call, 14) == 0) {
        shortParam3 = vmArgGet(call, 14);
    } else {
        shortParam3 = -1;
    }
    if (vmArgTagIsZero(call, 15) == 0) {
        lockFlag = vmArgGet(call, 15);
    } else {
        lockFlag = -1;
    }
    if (vmArgTagIsZero(call, 16) == 0) {
        modeValue3 = (float) vmArgGet(call, 16) / 100.0f;
    } else {
        modeValue3 = -1000.0f;
    }
    if (vmArgTagIsZero(call, 17) == 0) {
        motion = vmArgGet(call, 17);
    } else {
        motion = -1;
    }
    if (vmArgTagIsZero(call, 18) == 0) {
        shadowHeightA = (float) vmArgGet(call, 18) / 100.0f;
    } else {
        shadowHeightA = -1000.0f;
    }
    if (vmArgTagIsZero(call, 19) == 0) {
        shadowHeightB = (float) vmArgGet(call, 19) / 100.0f;
    } else {
        shadowHeightB = -1000.0f;
    }
    initState = vmArgTagIsZero(call, 20) != 0;
    record = &uwamonoRecords[index];
    state = record->body.motions;
    if (lockFlag != -1) {
        state->flags |= 4;
    }
    enepcStoreParams(index, height, extent, paramA0, paramA1, paramA2, paramA3,
                     angleDegrees, paramB0, paramB1,
                     shortParam0, shortParam1, shortParam2, shortParam3,
                     modeValue3, motion, shadowHeightA, shadowHeightB, initState);
    return 1;
}


/* Shared by setParams and setParamsAll.  For uwamonoRecords[index] whose kind is
   MAP_KIND_ACTOR_STATIC or MAP_KIND_ACTOR, every argument that is not its sentinel (-1
   for the shorts and most floats, -1000.0 for the three actor floats) is stored in its
   field; the angle is given in degrees.  Enemy_Init runs when initState is 1. */
void enepcStoreParams(int index, float height, float extent,
                      float paramA0, float paramA1, float paramA2, float paramA3,
                      float angleDegrees, float paramB0, float paramB1,
                      short shortParam0, short shortParam1, short shortParam2, short shortParam3,
                      float modeValue3, short motion, float shadowHeightA, float shadowHeightB,
                      int initState)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    struct XenoActor *actor;
    UwamonoState *state;

    if (record->kind == MAP_KIND_ACTOR_STATIC || record->kind == MAP_KIND_ACTOR) {
        actor = record->body.actor;
        state = record->body.motions;
        if (height != -1.0f) {
            record->extent.y = height;
        }
        if (extent != -1.0f) {
            actor->radius = extent;
            record->extent.x = extent;
        }
        if (paramA0 != -1.0f) {
            state->wanderStep = paramA0;
        }
        if (paramA1 != -1.0f) {
            state->leashRange = paramA1;
        }
        if (paramA2 != -1.0f) {
            state->moveSpeed = paramA2;
        }
        if (paramA3 != -1.0f) {
            state->pursuitSpeed = paramA3;
        }
        if (angleDegrees != -1.0f) {
            state->fanWidth = angleDegrees / 180.0f * 3.1415927f;
        }
        if (paramB0 != -1.0f) {
            state->fanLength = paramB0;
        }
        if (paramB1 != -1.0f) {
            state->noticeRange = paramB1;
        }
        if (shortParam0 != -1) {
            state->pauseTime = shortParam0;
        }
        if (shortParam1 != -1) {
            state->pauseCycle = shortParam1;
        }
        if (shortParam2 != -1) {
            state->threatTime = shortParam2;
        }
        if (shortParam3 != -1) {
            state->threatCycle = shortParam3;
        }
        if (modeValue3 != -1000.0f) {
            actor->modeValue3 = modeValue3;
        }
        if (motion != -1) {
            record->motion = motion;
        }
        if (shadowHeightA != -1000.0f) {
            actor->shadowHeightA = shadowHeightA;
        }
        if (shadowHeightB != -1000.0f) {
            actor->shadowHeightB = shadowHeightB;
        }
        if (initState != 1) {
            return;
        }
        Enemy_Init(index);
    }
}


/* Native "motion", at least 6 arguments: argument 1 is the record key, argument 2 the
   motion (group * 100 + number), argument 3 the speed in percent, divided by 100 (absent:
   100.0f is passed as is, not 1.0: the original loads that constant undivided),
   arguments 4 and 5 the play range (-1 or absent: keep the slot's bound).  Argument 6
   present clears the actor's loop bit 0x2, absent sets it.  The motion is only restarted
   when it differs from the actor's current one. */
int enepcMotion(SbThread *call)
{
    int key;
    int index;
    int motion;
    float speed;
    int start;
    int end;
    int clearLoop;
    struct XenoActor *actor;
    int rangeStart;
    int rangeEnd;

    if (VM_PLUGIN_CNT_GET(call) < 6) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    key = vmArgGet(call, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    if (index == -1) {
        return 1;
    }
    motion = vmArgGet(call, 2);
    if (vmArgTagIsZero(call, 3) == 0) {
        speed = (float) vmArgGet(call, 3) / 100.0f;
    } else {
        speed = 100.0f;
    }
    if (vmArgTagIsZero(call, 4) == 0) {
        start = vmArgGet(call, 4);
    } else {
        start = -1;
    }
    if (vmArgTagIsZero(call, 5) == 0) {
        end = vmArgGet(call, 5);
    } else {
        end = -1;
    }
    clearLoop = vmArgTagIsZero(call, 6) == 0;
    if (uwamonoRecords[index].kind != MAP_KIND_ACTOR_STATIC && uwamonoRecords[index].kind != MAP_KIND_ACTOR) {
        return 1;
    }
    actor = uwamonoRecords[index].body.actor;
    if (actor->slot != motion / 100 || actor->mode != motion % 100) {
        uwamonoSetMotion(index, motion, speed);
        XenoActorSetBlendFrames(actor, 4.0f);
        XenoActorGetSlotRange(actor, 0, motion, &rangeStart, &rangeEnd);
        if (start == -1 && end != -1) {
            XenoActorSetPlayRange(actor, 0, end);
        }
        if (start != -1 && end == -1) {
            XenoActorSetPlayRange(actor, start, rangeEnd);
            actor->frame = start;
        }
        if (start != -1 && end != -1) {
            XenoActorSetPlayRange(actor, start, end);
            actor->frame = start;
        }
    }
    if (clearLoop == 1) {
        actor->flags &= ~2;
    } else {
        actor->flags |= 2;
    }
    return 1;
}

/* The natives take the running script thread: VM_PLUGIN_CNT_GET reads its argument count (sb_natives.h). With the
 * call declared, enepcLookAt schedules `daddu $18,$4` into the jal delay slot with $4 still live; declared without
 * it, the copy of call moves ahead of the array init. */

/* Resigne reads the moving angle of the actor through a union (alias set 0), which keeps the load behind the
 * short store before it. Making XenoActor.direction a union would change the code of the other files that
 * use it, so this one site reads it through a one-member view. */
typedef union ActorDirectionView {
    Vector4 vector;
} ActorDirectionView;

/* enepcExMotion stores the loaded resource into XenoActor.rsrc[XAct_Rsrc_5] as a pointer: the store does not order
 * against the int store that follows it, as it would through the shared int array. This one site writes it through a
 * view of the actor. */
typedef struct ActorResourceView {
    unsigned char unmodeled_000[0x5EC + XAct_Rsrc_5 * 4];
    void *resource;                         /* 0x600: rsrc[XAct_Rsrc_5] */
} ActorResourceView;


/*
 * Native "motionFace". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 3 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found and the record is of kind
 * MAP_KIND_ACTOR or MAP_KIND_ACTOR_STATIC and its actor's selector differs from argument 2, sets the actor's channel
 * value to 4.0f and its selector to argument 2. Argument 3 is read but
 * unused. Returns 1.
 */
int motionFace(SbThread *call)
{
    int lookupIndex;
    int selector;
    struct XenoActor *actor;

    if (VM_PLUGIN_CNT_GET(call) < 3) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupIndex = uwamonoFindIndexByShortKey((short) vmArgGet(call, 1));
    if (lookupIndex == -1) {
        return 1;
    }
    selector = vmArgGet(call, 2);
    /* Argument 3 is read and discarded: the original calls vmArgGet(call, 3)
     * and never uses the result. */
    vmArgGet(call, 3);
    if (uwamonoRecords[lookupIndex].kind != MAP_KIND_ACTOR
        && uwamonoRecords[lookupIndex].kind != MAP_KIND_ACTOR_STATIC) {
        return 1;
    }
    actor = uwamonoRecords[lookupIndex].body.actor;
    if (actor->selector != selector) {
        XenoActorSetBlendFramesB(actor, 4.0f);
        XenoActorSetSelector(actor, selector);
    }
    return 1;
}

/*
 * Native "Extra". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 2 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; when found and the slot has an actor, passes
 * vmArgGet's second argument to XenoActorSetSolverPasses for the actor.
 * Returns 1 on success, 0 otherwise.
 */
int enepcExtra(SbThread *call)
{
    int lookupKey;
    int lookupIndex;
    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupKey = vmArgGet(call, 1);
    lookupIndex = uwamonoFindIndexByShortKey((short) lookupKey);
    /* The original reads argument 2 here, discards it, and reads it again for
     * the XenoActorSetSolverPasses call below (two jal vmArgGet with $5 = 2). */
    vmArgGet(call, 2);
    if (lookupIndex == -1) {
        return 0;
    }
    if (uwamonoSlotTable[lookupIndex].body.actor == 0) {
        return 0;
    }
    XenoActorSetSolverPasses(uwamonoSlotTable[lookupIndex].body.actor, vmArgGet(call, 2));
    return 1;
}

/*
 * Native "ExMotion". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 2. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found, loads the resource for vmArgGet's
 * second argument, stores the resource id to the slot's state and the loaded
 * resource to the slot's actor. Returns 1.
 */
int enepcExMotion(SbThread *call)
{
    void *resource;
    int lookupKey;
    int lookupIndex;
    int resourceId;
    UwamonoSlot *slot;

    resource = 0;
    if (VM_PLUGIN_CNT_GET(call) != 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupKey = vmArgGet(call, 1);
    lookupIndex = uwamonoFindIndexByShortKey((short) lookupKey);
    if (lookupIndex == -1) {
        return 1;
    }
    resourceId = vmArgGet(call, 2);
    GameResLoadID(resourceId, &resource, 0, 0);
    slot = &uwamonoSlotTable[lookupIndex];
    ((ActorResourceView *) slot->body.actor)->resource = resource;
    slot->body.motions->resourceId = resourceId;
    return 1;
}

/* Candidate target points and their reach (10 entries; src/main/game_camera.c reads
 * them as Vector4 enepcLookAtPointTable[]; the float[][4] view gives this function's
 * point-index addressing, the Vector4 one swaps the addu operands). The reach is declared in ytachi_test.h. */
extern float enepcLookAtPointTable[][4];


/*
 * Native "lookat". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 4 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey. With argument 5 present, when one of the 10
 * candidate target points has no reach yet, sets point number argument 5 to
 * arguments 2 to 4 / 100 with reach argument 6 / 100. Without it: arguments
 * 2 to 4 all omitted clear the record's target; arguments 3 and 4 omitted
 * offer the record found for argument 2 as the target, otherwise the point
 * arguments 2 to 4 / 100. Only records of kind MAP_KIND_ACTOR or
 * MAP_KIND_ACTOR_STATIC take an offer (0 is returned for the others).
 * Returns 1.
 */
int enepcLookAt(SbThread *call)
{
    int indices[2] = { -1, -1 };
    Vector4 point;
    int pointIndex;
    float reach;
    int i;

    if (VM_PLUGIN_CNT_GET(call) < 4) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    indices[0] = uwamonoFindIndexByShortKey((short) vmArgGet(call, 1));
    if (indices[0] == -1) {
        return 1;
    }
    if (vmArgTagIsZero(call, 5) == 0) {
        pointIndex = vmArgGet(call, 5);
        reach = (float) vmArgGet(call, 6) / 100.0f;
        for (i = 0; i < 10; i++) {
            if (enepcLookAtReachTable[i] == 0.0f) {
                enepcLookAtPointTable[pointIndex][0] = (float) vmArgGet(call, 2) / 100.0f;
                enepcLookAtPointTable[pointIndex][1] = (float) vmArgGet(call, 3) / 100.0f;
                enepcLookAtPointTable[pointIndex][2] = (float) vmArgGet(call, 4) / 100.0f;
                enepcLookAtReachTable[pointIndex] = reach;
                break;
            }
        }
        return 1;
    }
    if (vmArgTagIsZero(call, 2) != 0 && vmArgTagIsZero(call, 3) != 0 && vmArgTagIsZero(call, 4) != 0) {
        Actor_LookAt_Release(indices[0], 3);
        return 1;
    }
    if (vmArgTagIsZero(call, 3) != 0 && vmArgTagIsZero(call, 4) != 0) {
        indices[1] = uwamonoFindIndexByShortKey((short) vmArgGet(call, 2));
    } else {
        indices[1] = -1;
        point.x = (float) vmArgGet(call, 2) / 100.0f;
        point.y = (float) vmArgGet(call, 3) / 100.0f;
        point.z = (float) vmArgGet(call, 4) / 100.0f;
    }
    if (uwamonoRecords[indices[0]].kind != MAP_KIND_ACTOR
        && uwamonoRecords[indices[0]].kind != MAP_KIND_ACTOR_STATIC) {
        return 0;
    }
    if (indices[1] == -1) {
        Actor_LookAt_Set(indices[0], 3, -1, &point);
    } else {
        Actor_LookAt_Set(indices[0], 3, indices[1], 0);
    }
    return 1;
}


/*
 * Native "Encount". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 2. Otherwise passes the record index of argument 1 (from
 * uwamonoFindIndexByShortKey), 1 and argument 2 to Check_Encount and
 * returns 1.
 */
int enepcEncount(SbThread *call)
{
    int key;
    int index;

    if (VM_PLUGIN_CNT_GET(call) != 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    key = vmArgGet(call, 1);
    index = uwamonoFindIndexByShortKey(key);
    Check_Encount(index, 1, vmArgGet(call, 2));
    return 1;
}

/* src/main/get.c defines it with two parameters; this caller passes the
 * actor's moving angle in $f12 as a third. */

/*
 * Native "Resigne". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 1. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found and the slot's state is in action 5,
 * clears the trail step, copies the actor position to the first trail point
 * and enters action 2 with a rebuilt trail. Returns 1.
 */
int Resigne(SbThread *call)
{
    int lookupIndex;
    UwamonoSlot *slot;
    UwamonoState *state;
    struct XenoActor *actor;

    if (VM_PLUGIN_CNT_GET(call) != 1) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupIndex = uwamonoFindIndexByShortKey((short) vmArgGet(call, 1));
    if (lookupIndex == -1) {
        return 1;
    }
    slot = &uwamonoSlotTable[lookupIndex];
    state = slot->body.motions;
    actor = slot->body.actor;
    if (state->action == 5) {
        state->trailStep = 0;
        vectorCopyPoint(&state->trailPoints[0], &actor->position, ((ActorDirectionView *) &actor->direction)->vector.y);
        Enemy_ActionReady(lookupIndex, 2);
        Homing_Search(lookupIndex);
    }
    return 1;
}

/*
 * Native "setGroup". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 2 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found, for each of the 8 entries of the
 * slot's state setNumbers[], stores -1 when vmArgTagIsZero's argument i+2 is
 * nonzero, otherwise stores vmArgGet's argument i+1. Returns 1.
 * Original behaviour, kept as coded: the tag test reads argument i+2 but the value is argument i+1, so
 * group entry 0 takes the lookup key (argument 1).
 */
int setGroup(SbThread *call)
{
    int lookupKey;
    int lookupIndex;
    int i;
    UwamonoSlot *slot;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupKey = vmArgGet(call, 1);
    lookupIndex = uwamonoFindIndexByShortKey((short) lookupKey);
    if (lookupIndex == -1) {
        return 1;
    }
    slot = &uwamonoSlotTable[lookupIndex];
    for (i = 0; i < 8; i++) {
        if (vmArgTagIsZero(call, i + 2) != 0) {
            slot->body.motions->setNumbers[i] = -1;
        } else {
            slot->body.motions->setNumbers[i] = (short) vmArgGet(call, i + 1);
        }
    }
    return 1;
}

/*
 * Native "setBattle". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 2 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found, stores vmArgGet's second argument
 * as the state's cfEventBits value. Returns 1.
 */
int enepcSetBattle(SbThread *call)
{
    int lookupKey;
    int lookupIndex;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupKey = vmArgGet(call, 1);
    lookupIndex = uwamonoFindIndexByShortKey((short) lookupKey);
    if (lookupIndex == -1) {
        return 1;
    }
    uwamonoStateSlots[lookupIndex].motions->cfEventBits = vmArgGet(call, 2);
    return 1;
}

/*
 * Native "setSE". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 4 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found, stores vmArgGet's arguments 2
 * through 4 into the slot's state seIds[0..2]. Returns 1.
 */
int enepcSetSe(SbThread *call)
{
    int lookupKey;
    int lookupIndex;
    int i;
    UwamonoSlot *slot;

    if (VM_PLUGIN_CNT_GET(call) < 4) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupKey = vmArgGet(call, 1);
    lookupIndex = uwamonoFindIndexByShortKey((short) lookupKey);
    if (lookupIndex == -1) {
        return 1;
    }
    slot = &uwamonoSlotTable[lookupIndex];
    for (i = 0; i < 3; i++) {
        slot->body.motions->seIds[i] = vmArgGet(call, i + 2);
    }
    return 1;
}

/*
 * Native "defaultMotion". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does
 * not report 3 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found, stores vmArgGet's third argument
 * (the motion) into the state's walk[] at the slot given by the second
 * argument. Returns 1.
 */
int enepcDefaultMotion(SbThread *call)
{
    int lookupKey;
    int lookupIndex;
    int slot;
    int motion;

    if (VM_PLUGIN_CNT_GET(call) < 3) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupKey = vmArgGet(call, 1);
    lookupIndex = uwamonoFindIndexByShortKey((short) lookupKey);
    if (lookupIndex == -1) {
        return 1;
    }
    slot = vmArgGet(call, 2);
    motion = vmArgGet(call, 3);
    uwamonoStateSlots[lookupIndex].motions->walk[slot] = motion;
    return 1;
}

/*
 * Native "setTogether". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does
 * not report 2 or more. Otherwise resolves script arguments 1 through 4 with
 * uwamonoFindIndexByShortKey (-1 when omitted) and stores the four indices
 * into the together[] of every record found. Returns 1.
 */
int enepcSetTogether(SbThread *call)
{
    int members[4];
    int i;
    int j;
    UwamonoState *state;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    members[0] = vmArgTagIsZero(call, 1) != 0 ? -1 : uwamonoFindIndexByShortKey((short) vmArgGet(call, 1));
    members[1] = vmArgTagIsZero(call, 2) != 0 ? -1 : uwamonoFindIndexByShortKey((short) vmArgGet(call, 2));
    members[2] = vmArgTagIsZero(call, 3) != 0 ? -1 : uwamonoFindIndexByShortKey((short) vmArgGet(call, 3));
    members[3] = vmArgTagIsZero(call, 4) != 0 ? -1 : uwamonoFindIndexByShortKey((short) vmArgGet(call, 4));
    for (i = 0; i < 4; i++) {
        if (members[i] != -1) {
            state = uwamonoRecords[members[i]].body.motions;
            for (j = 0; j < 4; j++) {
                state->together[j] = members[j];
            }
        }
    }
    return 1;
}


/*
 * Native "Algorithm". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 3. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found, stores argument 2 as the state's mode, the record position as its
 * home, action 1 and argument 3 / 100 as its area size (1000.0f when omitted),
 * then calls Enemy_Init. Returns 1.
 */
int enepcAlgorithm(SbThread *call)
{
    int lookupIndex;
    int mode;
    float areaSize;

    if (VM_PLUGIN_CNT_GET(call) != 3) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupIndex = uwamonoFindIndexByShortKey((short) vmArgGet(call, 1));
    if (lookupIndex == -1) {
        return 1;
    }
    mode = vmArgGet(call, 2);
    if (vmArgTagIsZero(call, 3) == 0) {
        areaSize = (float) vmArgGet(call, 3) / 100.0f;
    } else {
        areaSize = 1000.0f;
    }
    uwamonoRecords[lookupIndex].body.motions->mode = mode;
    uwamonoRecords[lookupIndex].body.motions->area.x = uwamonoRecords[lookupIndex].positionCoords[0];
    uwamonoRecords[lookupIndex].body.motions->area.y = uwamonoRecords[lookupIndex].positionCoords[1];
    uwamonoRecords[lookupIndex].body.motions->area.z = uwamonoRecords[lookupIndex].positionCoords[2];
    uwamonoRecords[lookupIndex].body.motions->areaSize = areaSize;
    uwamonoRecords[lookupIndex].body.motions->action = 1;
    Enemy_Init(lookupIndex);
    return 1;
}

/*
 * Native "readyEnepcData". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does
 * not report 1. Otherwise loads the resource for argument 1 (the preset
 * table that setParams indexes), stores it to enepcParamSets and returns 1.
 */
int enepcReadyEnepcData(SbThread *call)
{
    void *resource;
    int resourceId;

    resource = 0;
    if (VM_PLUGIN_CNT_GET(call) != 1) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    resourceId = vmArgGet(call, 1);
    /* Retries until the resource has loaded. */
    while (GameResLoadID(resourceId, &resource, 0, 0) == -1) {
    }
    enepcParamSets = resource;
    return 1;
}

/*
 * Native "enableDrawFlags". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET
 * does not report 2 or more. Otherwise looks up vmArgGet's first argument
 * with uwamonoFindIndexByShortKey; if found, ORs argument 2 into the draw
 * flags of the actors of kind MAP_KIND_ACTOR_STATIC or MAP_KIND_ACTOR: of
 * every record whose kind equals argument 3,
 * or of the looked-up record alone when argument 3 is omitted. Returns 1.
 */
int enepcEnableDrawFlags(SbThread *call)
{
    int lookupIndex;
    int flags;
    int kind;
    int start;
    int end;
    int i;
    struct XenoActor *actor;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    lookupIndex = uwamonoFindIndexByShortKey((short) vmArgGet(call, 1));
    if (lookupIndex == -1) {
        return 1;
    }
    flags = vmArgGet(call, 2);
    kind = vmArgTagIsZero(call, 3) != 0 ? -1 : vmArgGet(call, 3);
    if (kind == -1) {
        start = lookupIndex;
        end = start + 1;
        /* Cannot be taken (lookupIndex was checked above), but the original
         * compares start with -1 again here (bne at enepcEnableDrawFlags +0xA0). */
        if (start == -1) {
            return 1;
        }
    } else {
        start = 0;
        end = MAP_RECORD_COUNT;
    }
    for (i = start; i < end; i++) {
        if (uwamonoRecords[i].kind == kind || kind == -1) {
            if (uwamonoRecords[i].kind == MAP_KIND_ACTOR_STATIC || uwamonoRecords[i].kind == MAP_KIND_ACTOR) {
                actor = uwamonoRecords[i].body.actor;
                if (actor != 0) {
                    actor->drawFlags |= flags;
                }
            }
        }
    }
    return 1;
}

const char enepcNativeNameSetSE[8] = "setSE";

const char enepcNativeNameResigne[8] = "Resigne";

const char enepcNativeNameEncount[8] = "Encount";

const char enepcNativeNameLookat[8] = "lookat";

const char enepcNativeNameMotion[8] = "motion";

const char enepcNativeNameExtra[8] = "Extra";
/* Upper bound of the uwamonoSlotTable walk in disableDrawFlags. */
#define UWAMONO_SLOT_COUNT 0x100

extern SbNativeMethod enepcNativeTable[];
extern char enepcModuleName[];

/* Native table of the enepc module: name and function pairs, then a null entry. The natives are defined in this file. */

const char enepcNativeNameDisableDrawFlags[24] = "disableDrawFlags";

const char enepcNativeNameEnableDrawFlags[16] = "enableDrawFlags";

const char enepcNativeNameReadyEnepcData[16] = "readyEnepcData";

const char enepcNativeNameAlgorithm[16] = "Algorithm";

const char enepcNativeNameSetTogether[16] = "setTogether";

const char enepcNativeNameDefaultMotion[16] = "defaultMotion";

const char enepcNativeNameSetBattle[16] = "setBattle";

const char enepcNativeNameSetGroup[16] = "setGroup";

const char enepcNativeNameMotionFace[16] = "motionFace";

const char enepcNativeNameExMotion[16] = "ExMotion";

const char enepcNativeNameSetParamsAll[16] = "setParamsAll";

const char enepcNativeNameSetParams[24] = "setParams";


int disableDrawFlags(SbThread *thread)
{
    int shortKey;
    int flagMask;
    int slotIndex;
    int kindFilter;
    int firstSlot;
    int endSlot;
    int slot;
    struct XenoActor *actor;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    shortKey = vmArgGet(thread, 1);
    slotIndex = uwamonoFindIndexByShortKey(shortKey);
    if (slotIndex == -1) {
        return 1;
    }
    flagMask = vmArgGet(thread, 2);
    if (vmArgTagIsZero(thread, 3) != 0) {
        kindFilter = -1;
    } else {
        kindFilter = vmArgGet(thread, 3);
    }
    if (kindFilter == -1) {
        firstSlot = slotIndex;
        endSlot = slotIndex + 1;
        if (slotIndex == kindFilter) {
            return 1;
        }
    } else {
        firstSlot = 0;
        endSlot = UWAMONO_SLOT_COUNT;
    }
    for (slot = firstSlot; slot < endSlot; slot++) {
        if (uwamonoRecords[slot].kind != kindFilter && kindFilter != -1) {
            continue;
        }
        if (uwamonoRecords[slot].kind != 4 && uwamonoRecords[slot].kind != 1) {
            continue;
        }
        actor = uwamonoRecords[slot].body.actor;
        if (actor != 0) {
            actor->drawFlags &= ~flagMask;
        }
    }
    return 1;
}

/* Registers the enepc native table enepcNativeTable with the module name "enepc". */
void enepcRegisterNatives(void) {
    sbNativeOwnerAdd(enepcModuleName, enepcNativeTable);
}

SbNativeMethod enepcNativeTable[19] = {
    { enepcNativeNameSetParams, enepcSetParams },
    { enepcNativeNameSetParamsAll, enepcSetParamsAll },
    { enepcNativeNameExtra, enepcExtra },
    { enepcNativeNameMotion, enepcMotion },
    { enepcNativeNameExMotion, enepcExMotion },
    { enepcNativeNameMotionFace, motionFace },
    { enepcNativeNameLookat, enepcLookAt },
    { enepcNativeNameEncount, enepcEncount },
    { enepcNativeNameResigne, Resigne },
    { enepcNativeNameSetGroup, setGroup },
    { enepcNativeNameSetBattle, enepcSetBattle },
    { enepcNativeNameSetSE, enepcSetSe },
    { enepcNativeNameDefaultMotion, enepcDefaultMotion },
    { enepcNativeNameSetTogether, enepcSetTogether },
    { enepcNativeNameAlgorithm, enepcAlgorithm },
    { enepcNativeNameReadyEnepcData, enepcReadyEnepcData },
    { enepcNativeNameEnableDrawFlags, enepcEnableDrawFlags },
    { enepcNativeNameDisableDrawFlags, disableDrawFlags },
    { 0, 0 }
};

/* Module name of the enepc natives. */
char enepcModuleName[8] = "enepc";
