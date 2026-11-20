#include "common.h"
#include "main/enemy_1.h"
#include "main/set_motion.h"
#include "shared.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/runtime_enepc.h"
#include "main/runtime_prog_effect.h"
#include "main/xenoActor.h"
#include "main/game_camera.h"
#include "main/vibration.h"
#include "main/uwamono_objx.h"
#include "main/runtime_item.h"
#include "main/runtime_se.h"
#include "main/runtime_adx.h"

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
   floatParams, floatParams48 and shortParams (and enepcStoreParams' paramA0..3,
   paramB0..1, shortParam0..3) are named by position on purpose: only their setters
   here and their reset (tya_draw_gauge.c) are recovered, not the code that reads them,
   so their roles are unknown. */
typedef struct EnepcParamSet {
    float height;                           /* +0x00 */
    float extent;                           /* +0x04 */
    float floatParams[4];                   /* +0x08 */
    float angleDegrees;                     /* +0x18 */
    float floatParams48[2];                 /* +0x1C */
    unsigned char unmodeled_24[4];
    short shortParams[4];                   /* +0x28 */
} EnepcParamSet;

extern struct EnepcParamSet *enepcParamSets;

void enepcStoreParams(int index, float height, float extent,
                      float paramA0, float paramA1, float paramA2, float paramA3,
                      float angleDegrees, float paramB0, float paramB1,
                      short shortParam0, short shortParam1, short shortParam2, short shortParam3,
                      float modeValue3, short motion, float fogA, float fogB,
                      int initState);

/* Native "Vibration", at least 4 arguments.  When vmArgTagIsZero reports argument 1 as
   absent (nonzero), the fade is stopped (Vibration_Stop); otherwise argument 1 goes to
   Vibration_Set_Weak and arguments 2 to 4 start a Vibration_Set_Strong. */
int runtimeVibration(SbThread *call)
{
    int value;
    int targetValue;
    int currentValue;
    int framesTotal;

    if (VM_PLUGIN_CNT_GET(call) < 4) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    if (vmArgTagIsZero(call, 1) != 0) {
        Vibration_Stop();
        return 1;
    }
    value = vmArgGet(call, 1);
    targetValue = vmArgGet(call, 2);
    currentValue = vmArgGet(call, 3);
    framesTotal = vmArgGet(call, 4);
    Vibration_Set_Weak(value);
    Vibration_Set_Strong(targetValue, currentValue, framesTotal);
    return 1;
}

extern SbNativeMethod runtimeNativeTable[];
extern char runtimeModuleName[];

/* Registers the "runtime" script module: runtimeModuleName holds its name string and
   runtimeNativeTable its table of name/function pointer pairs (setNumber, setLocater,
   setElevator, ...).  Called from scriptRegisterModules. */
void runtimeRegisterNatives(void) {
    sbNativeOwnerAdd(runtimeModuleName, runtimeNativeTable);
}

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
                state->pointLinks[j][i] = -1;
                state->pointLinkValues[j][i] = 0;
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
        shortParam3 = enepcParamSets[paramSet].shortParams[3];
        state->flags &= ~4;
        enepcStoreParams(index, enepcParamSets[paramSet].height, enepcParamSets[paramSet].extent,
                         enepcParamSets[paramSet].floatParams[0], enepcParamSets[paramSet].floatParams[1],
                         enepcParamSets[paramSet].floatParams[2], enepcParamSets[paramSet].floatParams[3],
                         enepcParamSets[paramSet].angleDegrees,
                         enepcParamSets[paramSet].floatParams48[0], enepcParamSets[paramSet].floatParams48[1],
                         enepcParamSets[paramSet].shortParams[0], enepcParamSets[paramSet].shortParams[1],
                         enepcParamSets[paramSet].shortParams[2], shortParam3,
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
    float fogA;
    float fogB;
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
        fogA = (float) vmArgGet(call, 18) / 100.0f;
    } else {
        fogA = -1000.0f;
    }
    if (vmArgTagIsZero(call, 19) == 0) {
        fogB = (float) vmArgGet(call, 19) / 100.0f;
    } else {
        fogB = -1000.0f;
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
                     modeValue3, motion, fogA, fogB, initState);
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
                      float modeValue3, short motion, float fogA, float fogB,
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
            state->wanderTime = shortParam0;
        }
        if (shortParam1 != -1) {
            state->pickCountA = shortParam1;
        }
        if (shortParam2 != -1) {
            state->waitTime = shortParam2;
        }
        if (shortParam3 != -1) {
            state->pickCountB = shortParam3;
        }
        if (modeValue3 != -1000.0f) {
            actor->modeValue3 = modeValue3;
        }
        if (motion != -1) {
            record->motion = motion;
        }
        if (fogA != -1000.0f) {
            actor->fogA = fogA;
        }
        if (fogB != -1000.0f) {
            actor->fogB = fogB;
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
        XenoActorSetChannelValue(actor, 4.0f);
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

char runtimeModuleName[8] = "runtime";

/* script native table: (name string pointer, function) pairs; the name pointers are .rodata of other TUs. */
extern const char runtimeNativeNameSetNumber[];
extern const char runtimeNativeNameSetLocater[];
extern const char runtimeNativeNameSetElevator[];
extern const char runtimeNativeNameSetLastDan[];
extern const char runtimeNativeNameSetPlayer[];
extern char runtimeNativeNameItem[];
extern const char runtimeNativeNameTimeAttack[];
extern const char runtimeNativeNameGetAnalogR[];
extern const char runtimeNativeNameProgEffect[];
extern const char runtimeNativeNameChangeRobo[];
extern char runtimeNativeNameMessage[];
extern char runtimeNativeNameJumpCF[];
extern const char runtimeNativeNameMapChange[];
extern char runtimeNativeNameFade[];
extern char runtimeNativeNameSetFlag[];
extern char runtimeNativeNameGetFlag[];
extern char runtimeNativeNameSetFog[];
extern char runtimeNativeNameSetItai[];
extern const char runtimeNativeNameSetLight[];
extern char runtimeNativeNameSystem[];
extern char runtimeNativeNameMpeg2[];
extern char runtimeNativeNameAdx[];
extern char runtimeNativeNameAdxload[];
extern char runtimeNativeNameAdxplay[];
extern char runtimeNativeNameSe[];
extern char runtimeNativeNameSePos[];
extern char runtimeNativeNameSeVol[];
extern char runtimeNativeNameBgmse[];
extern const char runtimeNativeNameBgmsePos[];
extern char runtimeNativeNameBgmFade[];
extern char runtimeNativeNameNextBgm[];
extern char runtimeNativeNameBgm[];
extern const char runtimeNativeNameVibration[];
extern const char runtimeNativeNameSetMapColor[];
SbNativeMethod runtimeNativeTable[35] = {
    { runtimeNativeNameSetNumber, runtimeSetNumber }, { runtimeNativeNameSetLocater, runtimeSetLocater }, { runtimeNativeNameSetElevator, runtimeSetElevator }, { runtimeNativeNameSetLastDan, runtimeSetLastDan },
    { runtimeNativeNameSetPlayer, runtimeSetPlayer }, { runtimeNativeNameItem, runtimeItem }, { runtimeNativeNameTimeAttack, runtimeTimeAttack }, { runtimeNativeNameGetAnalogR, runtimeGetAnalogR },
    { runtimeNativeNameProgEffect, runtimeProgEffect }, { runtimeNativeNameChangeRobo, changeRobo }, { runtimeNativeNameMessage, runtimeMessage }, { runtimeNativeNameJumpCF, runtimeJumpCf },
    { runtimeNativeNameMapChange, runtimeMapChange }, { runtimeNativeNameFade, runtimeFade }, { runtimeNativeNameSetFlag, runtimeSetFlag }, { runtimeNativeNameGetFlag, runtimeGetFlag },
    { runtimeNativeNameSetFog, runtimeSetMapFog }, { runtimeNativeNameSetItai, runtimeSetItai }, { runtimeNativeNameSetLight, runtimeSetMapLight }, { runtimeNativeNameSystem, runtimeSystem },
    { runtimeNativeNameMpeg2, mpeg2 }, { runtimeNativeNameAdx, adx }, { runtimeNativeNameAdxload, adxLoad }, { runtimeNativeNameAdxplay, adxPlay },
    { runtimeNativeNameSe, seSetVolumePan }, { runtimeNativeNameSePos, sePos }, { runtimeNativeNameSeVol, seVol }, { runtimeNativeNameBgmse, bgmSe },
    { runtimeNativeNameBgmsePos, bgmSePos }, { runtimeNativeNameBgmFade, bgmFade }, { runtimeNativeNameNextBgm, nextBgm }, { runtimeNativeNameBgm, bgmSetTrack },
    { runtimeNativeNameVibration, runtimeVibration }, { runtimeNativeNameSetMapColor, setMapColor },
    { 0, 0 }
};
