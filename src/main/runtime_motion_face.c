#include "common.h"
#include "main/enemy_1.h"
#include "shared.h"
#include "main/xenoActor.h"
#include "main/ytachi_test.h"
#include "main/runtime_motion_face.h"
#include "main/set_motion.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/uwamono_objx.h"
#include "main/res_get_path.h"
#include "main/get.h"

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
        XenoActorSetChannelValueB(actor, 4.0f);
        XenoActorSetSelector(actor, selector);
    }
    return 1;
}

/*
 * Native "Extra". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 2 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; when found and the slot has an actor, passes
 * vmArgGet's second argument to XenoActorSetFirstWord for the actor.
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
     * the XenoActorSetFirstWord call below (two jal vmArgGet with $5 = 2). */
    vmArgGet(call, 2);
    if (lookupIndex == -1) {
        return 0;
    }
    if (uwamonoSlotTable[lookupIndex].body.actor == 0) {
        return 0;
    }
    XenoActorSetFirstWord(uwamonoSlotTable[lookupIndex].body.actor, vmArgGet(call, 2));
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
 * slot's state groups[], stores -1 when vmArgTagIsZero's argument i+2 is
 * nonzero, otherwise stores vmArgGet's argument i+1. Returns 1.
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
            slot->body.motions->groups[i] = -1;
        } else {
            slot->body.motions->groups[i] = (short) vmArgGet(call, i + 1);
        }
    }
    return 1;
}

/*
 * Native "setBattle". Returns 0 after sbSlotJumpRequest when VM_PLUGIN_CNT_GET does not
 * report 2 or more. Otherwise looks up vmArgGet's first argument with
 * uwamonoFindIndexByShortKey; if found, stores vmArgGet's second argument
 * as the state's battle value. Returns 1.
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
    uwamonoStateSlots[lookupIndex].motions->battle = vmArgGet(call, 2);
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
 * home, action 1 and argument 3 / 100 as its speed (1000.0f when omitted),
 * then calls Enemy_Init. Returns 1.
 */
int enepcAlgorithm(SbThread *call)
{
    int lookupIndex;
    int mode;
    float speed;

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
        speed = (float) vmArgGet(call, 3) / 100.0f;
    } else {
        speed = 1000.0f;
    }
    uwamonoRecords[lookupIndex].body.motions->mode = mode;
    uwamonoRecords[lookupIndex].body.motions->area.x = uwamonoRecords[lookupIndex].positionCoords[0];
    uwamonoRecords[lookupIndex].body.motions->area.y = uwamonoRecords[lookupIndex].positionCoords[1];
    uwamonoRecords[lookupIndex].body.motions->area.z = uwamonoRecords[lookupIndex].positionCoords[2];
    uwamonoRecords[lookupIndex].body.motions->areaSize = speed;
    uwamonoRecords[lookupIndex].body.motions->action = 1;
    Enemy_Init(lookupIndex);
    return 1;
}

extern struct EnepcParamSet *enepcParamSets;

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
