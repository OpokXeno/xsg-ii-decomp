#include "common.h"
#include "shared.h"
#include "main/game_camera.h"
#include "main/xenoActor.h"
#include "main/xeno_actor_set.h"
#include "ov01/obj_slot.h"
#include "ov01/unit_cmd.h"
#include "ov01/sb_natives_unit.h"
#include "ov01/data_file_load.h"
#include "ov01/calc.h"
#include "ov01/cmd_exec.h"
#include "ov01/choice_menu.h"
#include "ov01/data_record_get.h"
#include "ov01/think_dispatch.h"
#include "ov01/snd_play.h"
#include "ov01/handle_state.h"
#include "ov01/unit_tbl.h"
#include "ov01/handle_table.h"
#include "main/sef.h"
#include "main/xgl_2.h"

#include "ov01/effect_pos.h"
#include "ov01/menu.h"
#include "ov01/data_obj_table.h"
#include "main/xgl_task.h"
#include "ov01/unit_side_table.h"
#include "main/elevator.h"

struct ObjectTask {
    XglTaskPrefix task; /* the scheduler header xglTaskEntryNext maintains. */
    void *work; /* textEntryRemove passes this slot to objWorkFree. */
};

extern void *memset(void *destination, int value, unsigned int count);

/*
 * Partial view of the unit record beyond the published ObjectTask prefix
 * (task+work, ending at +0x14, include/shared.h): unitSetInterpTime and
 * unitMotGet both follow a pointer immediately after that prefix, and
 * unitWpnMotSet reads a 4-entry pointer array starting eight bytes further.
 * Both point at actors (struct XenoActor): unitWpnMotSet forwards the entry it reads to
 * ACT_animGetData/ACT_setMotion. No other byte of this wider record is
 * claimed, and the relationship between this pointer and ObjectTask.work is
 * not evidenced here.
 */
typedef struct UnitRecord UnitRecord;

struct UnitRecord {
    ObjectTask task;                   /* +0x00..+0x13: the ObjectTask
                                         * prefix, opaque here */
    struct XenoActor *motionActor;                /* +0x14 */
    unsigned char unmodeled_018[0x1c - 0x18]; /* +0x18..+0x1b */
    struct XenoActor *wpnActor[4];                /* +0x1c */
    struct XenoActor *subActor;                   /* +0x2c: unitRemoveActor disposes it
                                         * first when it is set */
};

/* The same eight unit pointers have full-record and scheduler-prefix views. */
typedef union UnitTableStorage {
    UnitRecord *records[8];
    ObjectTask *tasks[8];
} UnitTableStorage;

/* The unit table (players from slot 0, enemies from slot 3) and the live counts; unit_tbl.c reads them too. */
UnitTableStorage unitTbl;
int unitPlNum;
int unitEnNum;

void unitInit(void)
{
    memset(&unitTbl, 0, sizeof(unitTbl));
    unitPlNum = 0;
    unitEnNum = 0;
}

BattleUnit *unitCreateSide(int kind, int side);

/*
 * Player unit creation: unitCreateSide with side 0, then the unit goes into
 * player slot 0 of the table (unitTblSet(0, unit)). Returns the unit or 0.
 */
BattleUnit *unitPlCreate(int id)
{
    BattleUnit *unit;

    unit = unitCreateSide(id, 0);
    if (unit != 0) {
        unitTblSet(0, unit);
    }
    return unit;
}

/*
 * Enemy unit creation: unitCreateSide with side 1, a 0x60 flag set in the
 * unit's word at +0xC, then unitTblSet(1, unit). Script-facing unitCreate
 * (ov01/sb_natives_unit.c) reaches it. Returns the unit or 0.
 */
BattleUnit *unitEnCreate(int id)
{
    BattleUnit *unit;

    unit = unitCreateSide(id, 1);
    unit->flags |= 0x60;
    if (unit != 0) {
        unitTblSet(1, unit);
    }
    return unit;
}

/*
 * Unit creation core for one side (0 player, 1 enemy): clears the word at
 * +0xC, runs two init helpers on +0x10 and +0x150, stores the fixed callbacks
 * unitUpdate and unitDrawNone at +4 and +8, and creates the XenoActor root
 * kept at +0x1D0. Returns the unit or 0.
 */
extern void unitDrawNone(BattleUnit *object);

/*
 * unitCreateSide copies the runtime vectors into the actor's fog blocks (XenoActor.color and colorBlock) as two
 * doublewords through unions, an alias-0 access that keeps the stores in the original order. Making the shared fields
 * unions would change main's code (setFog and the map object code write them as floats), so this one site reaches
 * them through a view that starts like the actor.
 */
typedef struct UnitActorFogView {
    unsigned char unmodeled_000[0x490];
    union {
        DoublewordVector4 runtimeVecA;      /* 0x490 */
        float fogDist[4];
    };
    union {
        DoublewordVector4 runtimeVecB;      /* 0x4A0 */
        float fogColor[3];
    };
} UnitActorFogView;

BattleUnit *unitCreateSide(int kind, int side)
{
    BattleUnit *unit;
    struct XenoActor *actor;
    struct XenoActor *root;
    StatBlock *found;
    int notFound;
    StatBlock *battleEntry;
    DoublewordVector4 *vecA;
    DoublewordVector4 *vecB;
    int i;
    unsigned short slotFlag;
    unit = unitSideClaim();
    if (unit == 0) {
        return 0;
    }
    unit->flags = 0;
    objCmdClear(&unit->commandQueue);
    objStdInit(&unit->pose);
    unit->update = unitUpdate;
    unit->draw = unitDrawNone;
    found = battleEntryFindByKey(kind);
    notFound = found == 0;
    battleEntry = battleEntryAcquire(unit, kind);
    unit->stats = battleEntry;
    if (battleEntry == 0) {
        return 0;
    }
    if (notFound == 1) {
        battleEntryActivate(unit, kind);
    }
    root = XenoActorAlloc();
    unit->unit = root;
    actor = root;
    if (kind < 141 || kind > 143) {
        actor->drawFlags = 0x10001;
    }
    actor->alpha = 0.0f;
    if (kind != 125) {
        actor->shadowType = 2;
        actor->drawParam = 0.5f;
    }
    XenoActorSetChannelFlag80(actor);
    actor->light = *lightBlockGet();
    runtimeFogPtrGet(&vecA, &vecB);
    ((UnitActorFogView *) root)->runtimeVecA = *vecA;
    ((UnitActorFogView *) root)->runtimeVecB = *vecB;
    for (i = 0; i < 3; i++) {
        if (unit->stats->pairFirst[i] != 0) {
            actor = XenoActorAlloc();
            slotFlag = unit->stats->pairSecond[i];
            unit->children[i] = actor;
            actor->drawFlags = 0x10001;
            actor->alpha = 0.0f;
            if ((slotFlag & 0x4000) != 0) {
                actor->drawFlags = 0x10005;
            }
            actor->light = *lightBlockGet();
            do {
                ((UnitActorFogView *) actor)->runtimeVecA = *vecA;
                ((UnitActorFogView *) actor)->runtimeVecB = *vecB;
            } while (0);
        } else {
            unit->children[i] = 0;
        }
    }
    return unit;
}

/*
 * Removes a unit: clears its reset slots, releases its actor and child actors, then
 * clears its scheduler entry and the state's status word when that is 16 or more.
 */
void unitRemove(BattleUnit *unit)
{
    struct XenoActor *child;
    int i;

    for (i = 3; i >= 0; i--) {
        unit->stats->activeMask[i] = 0;
    }
    sefReleaseUnitRecords(unit->unit, 0);
    XenoActorRelease(unit->unit);
    for (i = 0; i < 3; i++) {
        child = unit->children[i];
        if (child != 0) {
            XenoActorReleaseChild(unit->unit, child);
            XenoActorRelease(child);
        }
    }
    unitSlotRelease(unit);
    unitTblRemove(unit);
    if (unit->stats->key >= 16) {
        battleEntryRelease(unit);
    }
}

/*
 * The data slot (struct DataSlot, dataUnitFileLoad) as unitBindActors reads it: from +0x14 the buffer words of its
 * file records, 12 bytes apart (XenoActor.rsrc is int, so the buffers are read as words, like AttachRecord).
 * Entries 0 to 3 give the unit actor's resources and look link, entry 16 its rsrc[XAct_Rsrc_XAP], and entries
 * 4 + 4i to 7 + 4i give child i's; entry 18 + 2i gives child i's rsrc[XAct_Rsrc_XAP]. The word at +0xC is the
 * slot's handle.
 */
struct SlotFileWords {
    unsigned char unmodeled_00[0xC];
    unsigned short handle; /* +0xC */
    unsigned char unmodeled_0E[0x14 - 0xE];
    struct AttachEntry entries[24]; /* +0x14 */
};

/*
 * Sets up the unit's actor from the spawn record, then its child actors that are present.
 * Each child is attached at a slot or to a joint by its child kind, then the unit's state
 * takes the slot's handle halfword.
 */
int unitBindActors(BattleUnit *unit, struct DataSlot *slot)
{
    struct SlotFileWords *spawn = (struct SlotFileWords *)slot;
    struct XenoActor *actor;
    struct XenoActor *child;
    int clear_mask;
    int i;

    if (unit == 0 || slot == 0) {
        return 0;
    }
    actor = unit->unit;
    actor->rsrc[0] = spawn->entries[0].value;
    actor->rsrc[1] = spawn->entries[1].value;
    actor->rsrc[2] = spawn->entries[2].value;
    actor->rsrc[XAct_Rsrc_EntryTableC] = spawn->entries[3].value;
    actor->rsrc[XAct_Rsrc_XAP] = spawn->entries[16].value;
    actor->rsrc[XAct_Rsrc_SlotTableB] = dataFaceHandleGet();
    XenoActorAllocMatrices(actor);
    XenoActorSetSolverPasses(actor, 20);
    XenoActorSetBlendFrames(actor, 0.0f);
    XenoActorSetMode(actor, 0);
    for (i = 0; i < 3; i++) {
        child = unit->children[i];
        clear_mask = ~0x4000;
        if (child != 0) {
            child->rsrc[0] = spawn->entries[4 + 4 * i].value;
            child->rsrc[1] = spawn->entries[5 + 4 * i].value;
            child->rsrc[2] = spawn->entries[6 + 4 * i].value;
            child->rsrc[XAct_Rsrc_EntryTableC] = spawn->entries[7 + 4 * i].value;
            child->rsrc[XAct_Rsrc_XAP] = spawn->entries[18 + 2 * i].value;
            XenoActorAllocMatrices(child);
            if (child->rsrc[XAct_Rsrc_XAP] != 0) {
                XenoActorSetBlendFrames(child, 0.0f);
                XenoActorSetMode(child, 0);
            }
            if ((unsigned int)(unit->stats->pairFirstUnsigned[i] - 1) < 2) {
                XenoActorAttachAtSlot(unit->unit, child, 27);
            } else {
                XenoActorAttachChildToJoint(unit->unit, child, (short)(unit->stats->pairSecond[i] & clear_mask));
            }
        }
    }
    unitEffectStart(unit, unit->stats->sequenceIndex, 8);
    statEffOff(unit);
    unit->stats->handle = spawn->handle;
    return 1;
}

/*
 * Attaches a new child actor to the unit's actor from the attach record's entries. The
 * record's entry index + 4 is written to every actor of the unit and to the new one.
 */
int unitAttachChild(BattleUnit *unit, struct DataEntry *entry, int index)
{
    /* The file buffers are read as words (XenoActor.rsrc is int), through the AttachRecord view. */
    struct AttachRecord *attach = (struct AttachRecord *)entry;
    struct XenoActor *child;
    int i;

    if (unit == 0 || attach == 0) {
        return 0;
    }
    child = unit->unit;
    child->rsrc[XAct_Rsrc_5] = attach->entries[index + 4].value;
    for (i = 0; i < 3; i++) {
        child = unit->children[i];
        if (child != 0) {
            child->rsrc[XAct_Rsrc_5] = attach->entries[index + 4].value;
        }
    }
    if (attach->entries[0].value != 0) {
        child = unit->children[2] = XenoActorAlloc();
        child->rsrc[0] = attach->entries[0].value;
        child->rsrc[1] = attach->entries[1].value;
        child->rsrc[2] = attach->entries[2].value;
        child->rsrc[XAct_Rsrc_EntryTableC] = attach->entries[3].value;
        child->rsrc[XAct_Rsrc_XAP] = 0;
        child->rsrc[XAct_Rsrc_5] = attach->entries[index + 4].value;
        XenoActorAllocMatrices(child);
        XenoActorAttachChildToJoint(unit->unit, child, (short)(unit->stats->pairSecond[2] & 0xBFFF));
    }
    return 1;
}

/* keyHigh of the high modes (mode 16 and up): unitActionKey uses 320, unitMoveKey uses 330. */
#define ACTION_KEY_BASE 320
#define MOVE_KEY_BASE 330

void unitRunCommands(BattleUnit *object);

/* Per-frame step: clears flag 0x20000, runs the command queue, copies the unit's snapshot into the pose, then calls
 * statEffProc. */
void unitUpdate(BattleUnit *object) {
    object->flags &= ~0x00020000u;
    unitRunCommands(object);
    object->pose.position = object->unit->position;
    statEffProc(object);
}

void unitDrawNone(BattleUnit *object) {

}

void unitClearBindFlags(BattleUnit *object);

void unitUnbind(BattleUnit *object) {
    unitClearBindFlags(object);
    statEffOn(object);
}

/* Clears bits 0x10004 of the unit and of each child whose slot flag 0x4000 is clear, and bit 2 of the object. */
void unitClearBindFlags(BattleUnit *object) {
    struct XenoActor *child;
    int i;

    object->unit->drawFlags &= ~0x00010004u;
    object->flags &= ~0x00000002u;
    for (i = 0; i < UNIT_CHILD_COUNT; i++) {
        child = object->children[i];
        if (child != 0 && !(object->stats->pairSecond[i] & 0x4000)) {
            child->drawFlags &= ~0x00010004u;
        }
    }
}

void unitSetBindFlags(BattleUnit *object);

void unitBindSetup(BattleUnit *object) {
    unitSetBindFlags(object);
    statEffOff(object);
}

/* Sets bit 4 of the unit and of each child, and bit 2 of the object: the mirror of unitClearBindFlags. */
void unitSetBindFlags(BattleUnit *object) {
    struct XenoActor *child;
    int i;

    object->unit->drawFlags |= 4;
    object->flags |= 2;
    for (i = 0; i < UNIT_CHILD_COUNT; i++) {
        child = object->children[i];
        if (child != 0) {
            child->drawFlags |= 4;
        }
    }
}

/* Returns the unit's count plus one; unitSetPhase compares that with 2 and 0xB. */
int unitCurrentPhase(BattleUnit *object) {
    return object->unit->mode + 1;
}

void unitSetPhase(BattleUnit *object, int phase) {
    struct XenoActor *unit;
    f32 channelValue;
    int i;

    if (phase == 0) {
        return;
    }
    unit = object->unit;
    if (unit->slot == 0 && unit->mode == phase - 1) {
        if ((unsigned int) (phase - 28) >= 4 && phase != 25 && phase != 4 && (unsigned int) (phase - 49) >= 3 && phase != 36 && phase != 39) {
            return;
        }
    }
    if (phase == 1 || phase == 2 || (phase == 5 && object->stats->subState != 4)) {
        unit->flags &= ~2;
    } else {
        unit->flags |= 2;
    }
    if ((unit->flags & 0x200) == 0 && unit->slot == 0 && ((phase == 2 && unitCurrentPhase(object) != 11) || unitCurrentPhase(object) == 2)) {
        channelValue = 8.0f;
    } else {
        channelValue = 0.0f;
    }
    XenoActorSetBlendFrames(unit, channelValue);
    XenoActorSetBlendLocal(unit);
    XenoActorSetMode(unit, phase - 1);
    /* The loop walks the children through the same variable, which then holds each child in turn. */
    for (i = 0; i < UNIT_CHILD_COUNT; i++) {
        unit = object->children[i];
        if (unit != 0 && unit->rsrc[XAct_Rsrc_XAP] != 0) {
            unit->flags |= 2;
            XenoActorSetMode(unit, phase - 1);
        }
    }
}

/* Does nothing for phase 0; else sets status bit 2 on the unit and calls the timing helpers on it and on children with 0x600 set. */

void unitSetTimedPhase(BattleUnit *object, int phase) {
    struct XenoActor *unit;
    int i;

    if (phase == 0) {
        return;
    }
    unit = object->unit;
    unit->flags |= 2;
    XenoActorSetBlendLocal(unit);
    XenoActorSetModeInSlot(unit, 1, phase - 1);
    for (i = 0; i < UNIT_CHILD_COUNT; i++) {
        unit = object->children[i];
        if ((unit != 0) && (unit->rsrc[XAct_Rsrc_5] != 0)) {
            if (xenoActorSlotRangeValid(unit, 1, phase + i) != 0) {
                unit->flags |= 2;
                XenoActorSetModeInSlot(unit, 1, phase + i);
            }
        }
    }
}

/* For motion modes 1 to 10: the unit's timing call with 7.0f, then XenoActorSetSelector with the selector. */
void unitSetSelector(BattleUnit *object, int selector) {
    struct XenoActor *unit;

    if ((unsigned int) (object->stats->keyUnsigned - 1) < 10) {
        unit = object->unit;
        if (!(unit->flags & 0x200)) {
            XenoActorSetBlendFramesB(unit, 7.0f);
            XenoActorSetBlendLocalB(unit);
        }
        XenoActorSetSelector(unit, selector);
    }
}

/* Picks phase 2 with selector 6 when partyUnitHpLow holds and the mode is 1 to 10, else phase 1 with selector 0. */
void unitRefreshPhase(BattleUnit *object) {
    int phase;
    int selector;

    if ((partyUnitHpLow(object) != 0) && ((unsigned int) (object->stats->keyUnsigned - 1) < 10)) {
        phase = 2;
        selector = 6;
    } else {
        phase = 1;
        selector = 0;
    }
    unitSetPhase(object, phase);
    unitSetSelector(object, selector);
}

/* Calls XenoActorSetVector on the unit with x and y scale 1.0 and the third component scaleZ. */
void unitSetVectorZ(BattleUnit *object, float scaleZ) {
    XenoActorSetVector(object->unit, 1.0f, 1.0f, scaleZ);
}

/* Returns XenoActorIsDone's result: the state handlers test it as a done flag. */
int unitIsDone(BattleUnit *object) {
    return XenoActorIsDone(object->unit);
}

/* The key is keyHigh << 12 | keyLow; the unit is not read. */
int unitEffectKeyFree(BattleUnit *unit, int keyHigh, int keyLow) {
    return sefIsFinishEffect((keyHigh << 0xC) | keyLow) != 0;
}

/* Loads the snapshot vector from the table entry for id (three ints divided by 100) and records id in the motion. */
void unitLoadSnapshot(BattleUnit *object, int id) {
    int *entry;
    DoublewordVector4 *snapshot;

    entry = effectPosGet(id);
    if (entry != 0) {
        /* The pose position is stored through its union view (alias set 0), which keeps the three stores in order. */
        snapshot = (DoublewordVector4 *) &object->pose.position;
        snapshot->vector.x = (f32) entry[0] / 100.0f;
        snapshot->vector.y = (f32) entry[1] / 100.0f;
        snapshot->vector.z = (f32) entry[2] / 100.0f;
        objPosePublish(object);
        object->stats->posIndex = id;
    }
}

/* Degrees to radians (pi / 180), then unitSetAngleRadians. */
void unitSetAngleDegrees(BattleUnit *object, int degrees) {
    unitSetAngleRadians(object, (f32) degrees * 0.017453292f);
}

/* Stores the angle in radians, wrapped to [0, 2 pi), in the pose and in the motion. */
void unitSetAngleRadians(BattleUnit *object, f32 angle) {
    SlideBlock *pose;

    pose = &object->pose;
    pose->angle = angle;
    if (angle < 0.0f) {
        pose->angle = angle + 6.2831855f;
    }
    objPosePublish(object);
    object->stats->facing = pose->angle;
}

/* The motion angle in degrees (radians times 180 / pi), truncated; 0 for a null object. */
int unitGetAngleDegrees(BattleUnit *object) {
    if (object == 0) {
        return 0;
    }
    return (int) (object->stats->facing * 57.295776f);
}

/* Zeroes the snapshot's first three words, then calls objPosePublish. */
void unitClearSnapshot(BattleUnit *object) {
    SlideBlock *pose = &object->pose;

    pose->position.x = 0.0f;
    pose->position.y = 0.0f;
    pose->position.z = 0.0f;
    objPosePublish(object);
}

/* Speed factor by motion state: 2.0 for state 3, 3.0 for state 4, else 1.0. */
f32 unitStateScale(BattleUnit *object) {
    if (object->stats->state == 3) {
        return 2.0f;
    }
    if (object->stats->state == 4) {
        return 3.0f;
    }
    return 1.0f;
}

/* Union view: the float is stored through the value member (alias 0) so the matching form keeps its store order. */
typedef union Ov01FloatAlias {
    f32 value;
    u32 bits;
} Ov01FloatAlias;

/*
 * With a target: heading = atan2 of the pose-minus-target x and z offsets,
 * plus pi when facesTarget is zero (the unit turns its back on the target),
 * wrapped to [0, 2 pi); stored in the pose, the unit's +0x54 and the motion.
 * Then a point from the unit's matrix is written to pose+0x60
 * (xglVectorMulMat on a vector built from unitStateScale).
 */
float atan2f(float, float);

void unitFaceTarget(BattleUnit *object, int facesTarget, BattleUnit *target) {
    Vector4 *targetPosition;
    SlideBlock *pose;
    f32 heading;
    Vector4 offset;

    pose = &object->pose;
    targetPosition = &target->pose.position;
    if (target != 0) {
        Vector4 *position;
        StatBlock *motion;

        position = &pose->position;
        heading = atan2f(position->x - targetPosition->x, position->z - targetPosition->z);
        if (facesTarget == 0) {
            heading += 3.14159265f;
        }
        if (heading > 6.28318548f) {
            heading -= 6.28318548f;
        }
        if (heading < 0.0f) {
            heading += 6.28318548f;
        }
        ((Ov01FloatAlias *) &pose->angle)->value = heading;
        ((Ov01FloatAlias *) &object->unit->rotation.y)->value = heading;
        motion = object->stats;
        motion->facing = heading;
        XenoActorPushMatrix(object->unit);
    }
    offset.x = 0.0f;
    offset.y = -3.0f;
    offset.z = unitStateScale(object);
    offset.w = 1.0f;
    if (facesTarget == 0) {
        offset.z = -offset.z;
    }
    xglVectorMulMat(&pose->goal, object->unit->localMatrix.m, &offset);
}

/*
 * Writes the effect key pair (keyHigh, keyLow) of the action named by the
 * motion: keyHigh is the sequence index, or 320 for the high-mode sequences;
 * keyLow picks the variant from the sequence flags. Its mirror is unitMoveKey
 * (the move key, base 330, variants 1 to 3 and default 11).
 */
void unitActionKey(BattleUnit *object, int *keyHigh, int *keyLow) {
    StatBlock *motion;
    s16 mode;

    motion = object->stats;
    mode = motion->key;
    if (mode >= 16) {
        *keyHigh = motion->sequenceIndex;
        if ((unsigned int) (motion->sequenceIndex - 701) < 33) {
            *keyLow = 10;
            return;
        }
        switch (motion->flags & 3) {
        case 2:
            *keyLow = 6;
            break;
        case 1:
            *keyLow = 5;
            break;
        default:
            *keyLow = 4;
            break;
        }
        *keyHigh = ACTION_KEY_BASE;
    } else {
        *keyHigh = motion->sequenceIndex;
        *keyLow = 10;
    }
}

/*
 * The move key pair (keyHigh, keyLow), the mirror of unitActionKey: base 330
 * and variants 1 to 3, default 11 (the move dispatchers start it).
 */
void unitMoveKey(BattleUnit *object, int *keyHigh, int *keyLow) {
    StatBlock *motion;
    s16 mode;

    motion = object->stats;
    mode = motion->key;
    if (mode >= 16) {
        *keyHigh = motion->sequenceIndex;
        if ((unsigned int) (motion->sequenceIndex - 701) < 33) {
            *keyLow = 11;
            return;
        }
        switch (motion->flags & 3) {
        case 2:
            *keyLow = 3;
            break;
        case 1:
            *keyLow = 2;
            break;
        default:
            *keyLow = 1;
            break;
        }
        *keyHigh = MOVE_KEY_BASE;
    } else {
        *keyHigh = motion->sequenceIndex;
        *keyLow = 11;
    }
}

/* Appends the command record to the object's command queue. */
void unitCmdSet(BattleUnit *object, Ov01Command *command) {
    dataLookupAppend(&object->commandQueue, command);
}

/* Empties the object's command queue. */
void unitCmdClear(BattleUnit *object) {
    objCmdClear(&object->commandQueue);
}

/* Nonzero when the object's command queue has no pending entry. */
int unitQueueIsEmpty(BattleUnit *object) {
    return objCmdPtrGet(&object->commandQueue) == 0;
}

/*
 * Runs the object's command queue. Each entry starts with a type below 0x28
 * that indexes the handler table unitCommandTable; a handler returning nonzero
 * retires the entry and resets the wait counter at +0x138, a zero return
 * keeps it and counts.
 */
extern int (*unitCommandTable[UNIT_COMMAND_COUNT])(BattleUnit *, Ov01Command *);

void unitRunCommands(BattleUnit *object) {
    Ov01Command *command;

    command = objCmdPtrGet(&object->commandQueue);
    while (command != 0 && command->command < UNIT_COMMAND_COUNT) {
        if (command->command != 0 && object->commandQueue.flag == 0) {
            object->flags |= 1;
        }
        if (unitCommandTable[command->command](object, command) == 0) {
            object->commandQueue.flag++;
            break;
        }
        object->commandQueue.flag = 0;
        objCmdNext(&object->commandQueue);
        command = objCmdPtrGet(&object->commandQueue);
    }
}

/* Handler table unitCommandTable entry 0: no operation, always done. */
int unitCmdNoop(BattleUnit *unusedObject, Ov01Command *unusedStep) {
    return 1;
}

int unitCmdSetPhase(BattleUnit *object, Ov01Command *step) {
    TecRecord *record;

    record = tecRecordGet(step->code);
    switch (step->state) {
    case 0:
        if (record->motionIndex == 0) {
            unitSetPhase(object, record->phase);
        } else {
            unitSetTimedPhase(object, record->phase);
        }
        if (record->hitFlags & 0x100) {
            object->flags |= 0x2000;
        }
        if (record->strikeZones[0] & 0x10) {
            object->flags &= ~0x2000;
        }
        step->state++;
        break;
    case 1:
        if (unitIsDone(object) != 0) {
            return 1;
        }
        break;
    default:
        return 0;
    }
    return 0;
}

/* Stat-effect step: state 0 starts the object; later states wait for unitIsDone, then apply the hp and ep effects and the level or current change, and set flag 0x4000. */
int unitCmdStatEffect(BattleUnit *object, Ov01Command *step) {
    int effect;
    int amount;
    int hpKind; /* -50 or -100: the kind argument of objDmgNumCreate for the hit-point effect */
    int level;
    int magnitude;
    unsigned int value;

    hpKind = -50;
    if (step->state == 0) {
        unitEffectStart(object, 0x140, 9);
        seVoiceStartTagged(2, 3);
        if (object->stats->key >= 0x10) {
            object->flags |= 0x4000;
            return 1;
        }
        unitSetPhase(object, 0x1A);
        step->state++;
    } else if (unitIsDone(object) != 0) {
        effect = calcSkill(object, 0x200, 0x8000);
        if (effect > 0) {
            int epDelta;

            hpKind = -100;
            epDelta = object->stats->maxEp;
            epDelta = epDelta * effect;
            epDelta = epDelta / 100;
            calcEp(object, epDelta);
            magnitude = epDelta;
            if (epDelta < 0) {
                magnitude = -epDelta;
            }
            objDmgNumCreate(object, magnitude, 4, -50);
        }
        effect = calcSkill(object, 0x200, 0x800);
        if (effect > 0) {
            int hpDelta;

            hpDelta = object->stats->maxHp;
            hpDelta = hpDelta * effect;
            hpDelta = hpDelta / 100;
            calcHp(object, hpDelta);
            magnitude = hpDelta;
            if (hpDelta < 0) {
                magnitude = -hpDelta;
            }
            objDmgNumCreate(object, magnitude, 2, hpKind);
        }
        value = object->stats->keyUnsigned;
        if (value - 1 < 10) {
            level = 1;
            effect = calcSkill(object, 0x200, 0x2000);
            if (effect > 0) {
                if (ov01RollPercent(50) != 0) {
                    level = effect;
                }
            }
            unitLevelAdd(object, level);
        } else if (((value - 11) & 0xFFFF) < 5) {
            amount = 100;
            effect = calcSkill(object, 0x200, 0x1000);
            if (effect > 0) {
                amount = amount + effect;
            }
            unitCurrentAdd(object, amount);
            if (calcSkillEquChk(object, 0x51) != 0) {
                calcStatResetCha(object);
            }
        }
        value = object->flags;
        object->flags = value | 0x4000;
        return 1;
    }
    return 0;
}

int unitCmdAction(BattleUnit *object, Ov01Command *command) {
    StatBlock *motion;
    s16 mode;
    int keyHigh;
    int keyLow;

    switch (command->state) {
    case 0:
        unitSetPhase(object, 20);
        unitActionKey(object, &keyHigh, &keyLow);
        unitEffectStart(object, keyHigh, keyLow);
        unitSetSelector(object, 9);
        seObjectVoiceStart(object, 8, unitActionVoiceCueId());
        command->state++;
        break;
    case 1:
        motion = object->stats;
        mode = motion->key;
        if (mode < 16) {
            if (unitIsDone(object) != 0) {
                unitSetSelector(object, 0);
                unitRefreshPhase(object);
                return 1;
            }
        } else {
            if (unitIsDone(object) != 0) {
                unitSetSelector(object, 0);
                unitRefreshPhase(object);
                return 1;
            }
        }
    }
    return 0;
}

int unitCmdPlayVoice(BattleUnit *object, Ov01Command *step) {
    tecRecordGet(step->code);
    switch (step->state) {
    case 0:
        unitSetPhase(object, 23);
        seObjectVoiceStart(object, 9, unitPlayVoiceCueId());
        streamPlayByKey(object, (step->arg.owner == object) ? 1 : 2);
        step->state++;
        break;
    case 1:
        if (unitIsDone(object) != 0) {
            unitRefreshPhase(object);
            return 1;
        }
        break;
    }
    return 0;
}

/* Command step: state 0 starts the fade-out and moves to state 1; state 1 releases the unit once no fade runs. */
int unitCmdRelease(BattleUnit *object, Ov01Command *command) {
    switch (command->state) {
    case 0:
        unitEffectStartMode(object, 320, 8);
        unitFadeOut(object);
        command->state++;
        break;
    case 1:
        if (object->fade != 0) {
            break;
        }
        dataSlotFlagClear(object->stats->key);
        sefReleaseID(object->stats->sequenceIndex);
        handleFree(object->stats->handle);
        dataSlotVoiceReset(object->stats->key);
        unitRemove(object);
        return 1;
    }
    return 0;
}

/* Command step: state 0 fades out, state 1 loads the snapshot once no fade runs, state 2 fades in, state 3 completes once no fade runs. */
int unitCmdLoadSnapshot(BattleUnit *object, Ov01Command *step) {
    switch (step->state) {
    case 0:
        unitFadeOut(object);
        step->state++;
        break;
    case 1:
        if (object->fade == 0) {
            unitLoadSnapshot(object, step->code);
            step->state++;
        }
        break;
    case 2:
        unitFadeIn(object);
        step->state++;
        break;
    case 3:
        if (object->fade == 0) {
            return 1;
        }
        break;
    }
    return 0;
}

/* Command step for the second command: state 0 starts the fade-out; state 1 sets the battle transition and clears the object's flag bit 2. */
int unitCmdSetBattleTransition(BattleUnit *object, Ov01Command *command) {
    int state;

    state = command->state;
    switch (state) {
    case 0:
        unitFadeOut(object);
        command->state++;
        break;
    case 1:
        if (object->fade == 0) {
            if (object->stats->key < 16) {
                manWorkGet()->transition = 1;
            } else {
                manWorkGet()->transition = 2;
            }
            object->flags &= ~1;
            return 1;
        }
        break;
    }
    return 0;
}

/* Handler table unitCommandTable entry 8: calls unitRefreshPhase, then done. */
int unitCmdRefreshPhase(BattleUnit *object, Ov01Command *unusedStep) {
    unitRefreshPhase(object);
    return 1;
}

/* Handler table unitCommandTable entry 9: sets flag 0x1000, clears flag 1, calls statEffOn (the unbind side) when flag 4 is clear, then refreshes the phase unless the phase kind is 2 or 3. */
int unitCmdUnbind(BattleUnit *object, Ov01Command *unusedStep) {
    object->flags = (object->flags | 0x1000) & ~1;
    if ((object->flags & 4) == 0) {
        statEffOn(object);
    }
    if ((unsigned int) ((unsigned char) object->stats->breakState - 2) < 2) {
        return 1;
    }
    return unitCmdRefreshPhase(object, unusedStep);
}

/* Handlers of entries 10 to 39 are defined below in this file. */
int unitCmdDmg(BattleUnit *object, Ov01Command *step);
int unitCmdEva(BattleUnit *object, Ov01Command *step);
int unitCmdDef(BattleUnit *object, Ov01Command *step);
int unitCmdDead(BattleUnit *object, Ov01Command *step);
int unitCmdMot(BattleUnit *object, Ov01Command *step);
int unitCmdTgtSetDummy(BattleUnit *object, Ov01Command *step);
int unitCmdMoveCha(BattleUnit *object, Ov01Command *step);
int unitCmdMovePos(BattleUnit *object, Ov01Command *step);
int unitCmdPhase0EWait(BattleUnit *object, Ov01Command *step);
int unitCmdPhase12Wait(BattleUnit *object, Ov01Command *step);
int unitCmdPoseArrive(BattleUnit *object, Ov01Command *step);
int unitCmdPoseArriveWait(BattleUnit *object, Ov01Command *step);
int unitCmdMoveChaRun(BattleUnit *object, Ov01Command *step);
int unitCmdMovePosJump(BattleUnit *object, Ov01Command *step);
int unitCmdMoveChaJump(BattleUnit *object, Ov01Command *step);
int unitCmdMoveChaHover(BattleUnit *object, Ov01Command *step);
int unitCmdMovePosHover(BattleUnit *object, Ov01Command *step);
int unitCmdModeRamp(BattleUnit *object, Ov01Command *step);
int unitCmdMoveRatio(BattleUnit *object, Ov01Command *step);
int unitCmdMoveRamp(BattleUnit *object, Ov01Command *step);
int unitCmdFinish(BattleUnit *object, Ov01Command *step);
int unitCmdBted(BattleUnit *object, Ov01Command *step);
int unitCmdRec(BattleUnit *object, Ov01Command *step);
int unitCmdWake(BattleUnit *object, Ov01Command *step);
int unitCmdModeStart(BattleUnit *object, Ov01Command *step);

/* unitCommandTable: the 40 handlers the command queue dispatches through, in index order. */
int (*unitCommandTable[UNIT_COMMAND_COUNT])(BattleUnit *, Ov01Command *) = {
    unitCmdNoop, unitCmdSetPhase, unitCmdStatEffect, unitCmdAction,
    unitCmdPlayVoice, unitCmdRelease, unitCmdLoadSnapshot, unitCmdSetBattleTransition,
    unitCmdRefreshPhase, unitCmdUnbind, unitCmdDmg, unitCmdEva,
    unitCmdDef, unitCmdDead, unitCmdMot, unitCmdTgtSetDummy,
    unitCmdMoveCha, unitCmdMovePos, unitCmdPhase0EWait, unitCmdPhase12Wait,
    unitCmdPoseArrive, unitCmdPoseArriveWait, unitCmdMoveChaRun, unitCmdMovePosJump,
    unitCmdMoveChaJump, unitCmdMoveChaHover, unitCmdMovePosHover, unitCmdModeRamp,
    unitCmdMoveRatio, unitCmdMoveRamp, unitCmdFinish, unitCmdBted,
    unitCmdRec, unitCmdWake, unitCmdStat, unitCmdTurn,
    unitCmdStreamPlay, unitCmdRemove, unitCmdFinishRefresh, unitCmdModeStart
};

/* Eight pose frames of the ramp that command 0x1B runs (the command handlers call it the eight-frame ramp). */
#define UNIT_POSE_RAMP_FRAMES 8

int unitCmdCodeGet(int code, int wait);
int unitCmdModeGet(int mode);

/*
 * Command 0x1B: sets the mode from the step code, then ramps pose->position.y over
 * UNIT_POSE_RAMP_FRAMES frames (unitPoseRamp and objPosePublish each frame). The
 * last step clears flags bit 0 and calls unitEffectStart(object, 310, 21).
 */
int unitCmdModeRamp(BattleUnit *object, Ov01Command *step)
{
    SlideBlock *pose = &object->pose;

    switch (step->state) {
    case 0:
        unitSetPhase(object, unitCmdCodeGet(step->code, step->arg.wait));
        step->poseIndex = 0;
        object->stats->breakState = unitCmdModeGet(step->code);
        step->state = step->state + 1;
        break;
    case 1:
        if (step->poseIndex < UNIT_POSE_RAMP_FRAMES) {
            pose->position.y = unitPoseRamp(step->poseIndex, 0);
            objPosePublish(object);
            step->poseIndex = step->poseIndex + 1;
        } else {
            step->state = 2;
        }
        break;
    default:
        object->flags &= ~1;
        unitEffectStart(object, 310, 21);
        return 1;
    }
    return 0;
}

/*
 * Command 0x27: starts the motion for the step code (unitSetPhase with
 * unitCmdCodeGet) and plays its voice. Code 3 also faces the target and copies
 * three pose floats. It waits for unitIsDone (code 3 leaves refresh to the
 * caller); other codes run unitRefreshPhase when done.
 */
int unitCmdModeStart(BattleUnit *object, Ov01Command *step)
{
    SlideBlock *pose = &object->pose;

    if (step->state == 0) {
        object->stats->breakState = unitCmdModeGet(step->code);
        unitSetPhase(object, unitCmdCodeGet(step->code, step->arg.wait));
        if (step->code == 3) {
            seObjectVoiceStartDefault(object, 14);
        } else if (step->code == 1) {
            seObjectVoiceStartDefault(object, 16);
        } else {
            seObjectVoiceStartDefault(object, 15);
        }
        unitSetSelector(object, 6);
        object->flags &= ~1;
        if (step->code == 3) {
            float saved1;
            float saved0;
            float saved2;
            unitFaceTarget(object, step->arg.wait, step->faceTarget);
            saved1 = pose->position.y;
            saved0 = pose->goal.x;
            saved2 = pose->goal.z;
            pose->goal.y = saved1;
            pose->position.x = saved0;
            pose->position.z = saved2;
            objPosePublish(object);
        }
        step->state = step->state + 1;
    } else if (step->code == 3) {
        if (unitIsDone(object) != 0) {
            return 1;
        }
    } else if (unitIsDone(object) != 0) {
        unitRefreshPhase(object);
        return 1;
    }
    return 0;
}

/*
 * Command 0xA, the mode dispatcher: mode 4 with state 1 enters command 0x1B,
 * mode 4 with state 3 and mode 3 with state 2 finish at once, and anything else
 * runs command 0x27.
 */
int unitCmdDmg(BattleUnit *object, Ov01Command *step)
{
    if (step->code == 4) {
        if (object->stats->breakState == 1) {
            step->command = UNIT_CMD_MODE_RAMP;
            seObjectVoiceStartDefault(object, 13);
            return unitCmdModeRamp(object, step);
        }
        if (object->stats->breakState == 3) {
            object->stats->breakState = unitCmdModeGet(4);
            object->flags &= ~1;
            seObjectVoiceStartDefault(object, 13);
            return 1;
        }
    }
    if (step->code == 3 && object->stats->breakState == 2) {
        object->stats->breakState = unitCmdModeGet(3);
        object->flags &= ~1;
        seObjectVoiceStartDefault(object, 14);
        return 1;
    }
    step->command = UNIT_CMD_MODE_START;
    return unitCmdModeStart(object, step);
}

/*
 * Maps a command mode to the code of its move. Modes 1 and 2 share one entry;
 * a mode outside 0..4 gives 1. wait selects the second code of each pair.
 */
int unitCmdCodeGet(int code, int wait)
{
    int result;

    switch (code) {
    case 0:
        if (wait == 0) {
            result = 28;
            break;
        }
        result = 29;
        break;
    case 1:
    case 2:
        if (wait == 0) {
            result = 30;
            break;
        }
        result = 31;
        break;
    case 3:
        if (wait == 0) {
            result = 36;
            break;
        }
        result = 39;
        break;
    case 4:
        if (wait == 0) {
            result = 32;
            break;
        }
        result = 33;
        break;
    default:
        result = 1;
        break;
    }
    return result;
}

/* Maps the command mode (2, 3, 4) to its mode index; any other value maps to 0. */
int unitCmdModeGet(int mode)
{
    int result;

    switch (mode) {
    case 3:
        result = 2;
        break;
    case 2:
        result = 1;
        break;
    case 4:
        result = 3;
        break;
    default:
        result = 0;
        break;
    }
    return result;
}

/* Command 0xB, one-shot start: sets phase 0x18 (UNIT_CMD_MOVE_CHA_JUMP) and effect 0xB, then waits for the phase to finish. */
int unitCmdEva(BattleUnit *object, Ov01Command *step)
{
    if (step->state == 0) {
        unitSetPhase(object, UNIT_CMD_MOVE_CHA_JUMP);
        seObjectVoiceStartDefault(object, 0xB);
        step->state = step->state + 1;
    } else if (unitIsDone(object) != 0) {
        unitRefreshPhase(object);
        object->flags &= ~1;
        return 1;
    }
    return 0;
}

/* Same shape as unitCmdEva with command 0x19 and effect 0xC. */
int unitCmdDef(BattleUnit *object, Ov01Command *step)
{
    if (step->state == 0) {
        unitSetPhase(object, UNIT_CMD_MOVE_CHA_HOVER);
        seObjectVoiceStartDefault(object, 0xC);
        step->state = step->state + 1;
    } else if (unitIsDone(object) != 0) {
        unitRefreshPhase(object);
        object->flags &= ~1;
        return 1;
    }
    return 0;
}

/*
 * Command 0xD: on the first call resets the stat masks and plays stream 4.
 * Phase 1 waits for the position check and, at mode 16 or more, fades out
 * (unitFadeOut); phase 2 waits for the fade to end, and phase 3 removes
 * the unit with unitRemove.
 */
int unitCmdDead(BattleUnit *object, Ov01Command *step)
{
    StatBlock *motion;
    int keyHigh;
    int keyLow;
    int mask;
    short mode;
    short key;

    switch (step->state) {
    case 0:
        object->flags |= 4;
        if (object->stats->breakState != 2) {
            unitSetPhase(object, 42);
        } else if (unitDeadSpecChk(object) != 0) {
            unitSetPhase(object, 42);
        }
        if (object->stats->breakState == 1) {
            object->stats->breakState = 0;
        }
        calcTurnEnd(object);
        mask = calcStatGet(object, 3, 0xffff);
        calcStatResetCha(object);
        if (mask & 0x400) {
            calcStatSet(object, 3, 1024);
        }
        if (mask & 0x800) {
            calcStatSet(object, 3, 2048);
        }
        if (mask & 0x2000) {
            calcStatSet(object, 3, 8192);
        }
        if (mask & 0x4000) {
            calcStatSet(object, 3, 16384);
        }
        object->stats->hitZoneCount = 0;
        object->stats->comboHits = 0;
        seObjectVoiceStartDefault(object, 5);
        streamPlayByKey(object, 4);
        if ((object->stats->flags & 0x2000) != 0) {
            step->state = step->state + 1;
        } else {
            unitMoveKey(object, &keyHigh, &keyLow);
            unitEffectStart(object, keyHigh, keyLow);
            step->state = step->state + 1;
        }
        return 0;
    case 1:
        mode = object->stats->key;
        unitMoveKey(object, &keyHigh, &keyLow);
        if (mode != 125) {
            /* The key test takes the object as an int (ov01/unit_cmd.h); its first argument is unused. */
            if (unitEffectKeyFree(object, keyHigh, keyLow) == 0) {
                break;
            }
        } else {
            if (object->commandQueue.flag < 241) {
                break;
            }
        }
        motion = object->stats;
        key = motion->key; /* read before the flag store: a union member (alias set 0) would not hoist */
        object->flags &= ~1;
        if (key >= 16) {
            if (motion->flags & 0x80) {
                step->state = step->state + 2;
            } else {
                unitFadeOut(object);
                step->state = step->state + 1;
            }
            break;
        }
        return 1;
    case 2:
        if (object->fade == 0) {
            step->state = 3;
        }
        break;
    case 3:
        if (object->stats->key >= 16) {
            unitRemove(object);
        }
        return 1;
    default:
        break;
    }
    return 0;
}

/*
 * Command 0xE: issues the step code; when step->arg.wait is set it also waits
 * for unitIsDone.
 */
int unitCmdMot(BattleUnit *object, Ov01Command *step)
{
    int code;
    int wait;

    code = step->code;
    wait = step->arg.wait;
    if (step->state == 0) {
        unitSetPhase(object, code);
        step->state = step->state + 1;
    } else {
        if (wait == 0) {
            return 1;
        }
        if (unitIsDone(object) != 0) {
            return 1;
        }
    }
    return 0;
}

/* Always reports done. No caller evidence for a name. */
int unitCmdTgtSetDummy(BattleUnit *unusedObject, Ov01Command *unusedStep)
{
    return 1;
}

/*
 * Character target, run mode (unitCmdRun with mode 1 and flag 0, as the
 * X1 unitCmdRun(object, 1, 0) call); on success finishes the move with
 * unitCmdMoveNext(object, 0), the unitCmdMoveNext(object, 0) call.
 */
int unitCmdMoveChaRun(BattleUnit *object, Ov01Command *step)
{
    if (unitCmdRun(object, 1, 0) != 0) {
        unitCmdMoveNext(object, 0);
        return 1;
    }
    return 0;
}

/*
 * Command 0x17, position target: mode 4 with flag 1 (unitCmdJump), then
 * unitCmdMoveNext(object, 1). Called from unitCmdMovePos.
 */
int unitCmdMovePosJump(BattleUnit *object, Ov01Command *step)
{
    int done;

    done = unitCmdJump(object, 4, 1);
    if (done != 0) {
        unitCmdMoveNext(object, 1);
        done = 1;
    }
    return done;
}

/*
 * Command 0x18, character target: mode 3 with flag 0 (unitCmdJump), then
 * unitCmdMoveNext(object, 0). Called from unitCmdMoveCha.
 */
int unitCmdMoveChaJump(BattleUnit *object, Ov01Command *step)
{
    int done;

    done = unitCmdJump(object, 3, 0);
    if (done != 0) {
        unitCmdMoveNext(object, 0);
        done = 1;
    }
    return done;
}

/*
 * Character target, hover mode (unitCmdHover with mode 5, flag 0, the X1
 * unitCmdHover(object, 5, 0) call); finishes with unitCmdMoveNext(object, 0).
 */
int unitCmdMoveChaHover(BattleUnit *object, Ov01Command *step)
{
    if (unitCmdHover(object, 5, 0) != 0) {
        unitCmdMoveNext(object, 0);
        return 1;
    }
    return 0;
}

/*
 * Position target, hover mode (unitCmdHover with mode 6, flag 1, the X1
 * unitCmdHover(object, 6, 1) call); finishes with unitCmdMoveNext(object, 1).
 */
int unitCmdMovePosHover(BattleUnit *object, Ov01Command *step)
{
    if (unitCmdHover(object, 6, 1) != 0) {
        unitCmdMoveNext(object, 1);
        return 1;
    }
    return 0;
}

/*
 * Character-target dispatcher: unitCmdDstSet(object, 3, 0) (the X1
 * unitCmdDstSet(object, 3, 0) call), then branches on the kind byte at +0x15 of
 * the record at object+0x144 and stores the command code into step->command.
 */
int unitCmdMoveCha(BattleUnit *object, Ov01Command *step)
{
    int result = 1;

    unitCmdDstSet(object, 3, 0);
    switch (object->stats->subState) {
    case 0:
    case 1:
    case 4:
        step->command = UNIT_CMD_MOVE_CHA_RUN;
        result = unitCmdMoveChaRun(object, step);
        break;
    case 2:
        step->command = UNIT_CMD_MOVE_CHA_JUMP;
        result = unitCmdMoveChaJump(object, step);
        break;
    case 3:
        step->command = UNIT_CMD_MOVE_CHA_HOVER;
        result = unitCmdMoveChaHover(object, step);
        break;
    }
    return result;
}

/*
 * Position-target dispatcher: unitCmdDstSet(object, 4, 1) (the X1
 * unitCmdDstSet(object, 4, 1) call), then the same kind dispatch as
 * unitCmdMoveCha with its own codes; sets the flag bit 0x1000 on exit.
 */
int unitCmdMovePos(BattleUnit *object, Ov01Command *step)
{
    int result;

    unitCmdDstSet(object, 4, 1);
    result = 1;
    switch (object->stats->subState) {
    case 0:
    case 2:
    case 4:
        step->command = UNIT_CMD_MOVE_POS_JUMP;
        result = unitCmdMovePosJump(object, step);
        break;
    case 3:
        step->command = UNIT_CMD_MOVE_POS_HOVER;
        result = unitCmdMovePosHover(object, step);
        break;
    }
    object->flags = (object->flags & ~1) | 0x1000;
    return result;
}

/* Values of StatBlock.breakState that the move steps set: unitCmdMoveRatio sets NORMAL, the pose ramp (unitCmdMoveRamp) sets DOWN. */
enum {
    UNIT_BREAK_STATE_NORMAL = 0,
    UNIT_BREAK_STATE_DOWN = 2
};

/* The pose index viewed as a plain int through a union (alias set 0), so its store keeps its place in the block. */
union UnitPoseIndexWord {
    int value;
};

/* Command 0xE, then waits for unitIsDone to report done. */
int unitCmdPhase0EWait(BattleUnit *object, Ov01Command *step)
{
    switch (step->state) {
    case 0:
        unitSetPhase(object, 0xE);
        step->state++;
        /* fall through */
    case 1:
        if (unitIsDone(object) != 0) {
            step->state++;
        }
        return 0;
    default:
        /* No return here: the original returns the constant 1 that the case-1 compare left in v0. */
        break;
    }
}

/* Command 0x12 on the first call, then advances the step without waiting; later calls wait for unitIsDone. */
int unitCmdPhase12Wait(BattleUnit *object, Ov01Command *step)
{
    switch (step->state) {
    case 0:
        unitSetPhase(object, 0x12);
        step->state++;
        /* fall through */
    case 1:
        step->state++;
        break;
    default:
        if (unitIsDone(object) != 0) {
            return 1;
        }
        break;
    }
    return 0;
}

/*
 * Phase 0 starts the pose: the setup calls, the +0x70 float copied to +0x24,
 * the publish, command 0xF, then the step advances. Later calls report 1 once
 * unitCmdMoveCalc reports arrival.
 */
int unitCmdPoseArrive(BattleUnit *object, Ov01Command *step)
{
    SlideBlock *pose;

    pose = &object->pose;
    if (step->state == 0) {
        unitCmdDstSet(object, 3, 0);
        unitCmdVASet(object, 3, 0);
        objPoseHeadingUpdate(object, 0);
        pose->angle = pose->goalAngle;
        objPosePublish(object);
        unitSetPhase(object, 0xF);
        step->state++;
    } else if (unitCmdMoveCalc(object) != 0) {
        return 1;
    }
    return 0;
}

/*
 * Phase 0 sets the pose up and sends command 0x12, then falls into the arrival
 * test. The test sends 0x13 and advances once unitCmdMoveCalc reports arrival; later
 * phases wait for unitIsDone.
 */
int unitCmdPoseArriveWait(BattleUnit *object, Ov01Command *step)
{
    switch (step->state) {
    case 0:
        unitCmdDstSet(object, 4, 1);
        unitCmdVASet(object, 4, 0);
        unitSetPhase(object, 0x12);
        step->state++;
        /* fall through */
    case 1:
        if (unitCmdMoveCalc(object) != 0) {
            unitSetPhase(object, 0x13);
            step->state++;
        }
        break;
    default:
        if (unitIsDone(object) != 0) {
            return 1;
        }
        break;
    }
    return 0;
}

/*
 * Percentage-driven move start: reads the row at motion->posIndex of the table,
 * stores the two row percentages as fractions in the snapshot x and z, picks command 0x29 or 0x26 from
 * unitCurrentPhase, and waits for the move to finish.
 */
int unitCmdMoveRatio(BattleUnit *object, Ov01Command *step)
{
    DoublewordVector4 *snapshot;
    s16 effectPosIndex;
    f32 primary;

    snapshot = (DoublewordVector4 *) &object->pose.position;
    if (step->state == 0) {
        object->stats->breakState = UNIT_BREAK_STATE_NORMAL;
        effectPosIndex = object->stats->posIndex;
        primary = (f32) *effectPosGet(effectPosIndex);
        snapshot->vector.y = 0.0f;
        snapshot->vector.x = primary / 100.0f;
        snapshot->vector.z = (f32) effectPosGet(effectPosIndex)[2] / 100.0f;
        objPosePublish(object);
        unitSetPhase(object, (unitCurrentPhase(object) ^ 0x27) == 0 ? 0x29 : 0x26);
        object->flags &= ~1;
        step->state += 1;
    } else if (unitIsDone(object) != 0) {
        unitRefreshPhase(object);
        return 1;
    }
    return 0;
}

/*
 * Phase 0 turns the unit toward its target, sets the pose blend and starts command
 * 0x23 or 0x22 with source sound 0x136015. Each call advances the eight-frame ramp
 * while poseIndex is below UNIT_POSE_RAMP_FRAMES. Once unitCmdMoveCalc reports arrival, the step plays
 * command 0x27 or 0x24, the voice, and clears the active flag.
 */
int unitCmdMoveRamp(BattleUnit *object, Ov01Command *step)
{
    SlideBlock *pose;
    UnitScriptPairEvent message;
    int facing;
    int code;
    f32 zero;
    DoublewordVector4 *snapshot;

    pose = &object->pose;
    snapshot = (DoublewordVector4 *) &pose->position;
    if (step->state == 0) {
        object->stats->breakState = UNIT_BREAK_STATE_DOWN;
        facing = 0;
        if (unitCurrentPhase(object) != 0x20) {
            facing = 1;
        }
        unitFaceTarget(object, facing, object->stats->source);
        zero = 0.0f;
        pose->velocity.x = (pose->goal.x - snapshot->vector.x) * 0.125f;
        pose->velocity.z = (pose->goal.z - snapshot->vector.z) * 0.125f;
        pose->acceleration.z = zero;
        pose->acceleration.y = zero;
        pose->acceleration.x = zero;
        pose->velocity.y = zero;
        ((union UnitPoseIndexWord *)&step->poseIndex)->value = 0;
        if (unitCurrentPhase(object) == 0x20) {
            code = 0x22;
        } else {
            code = 0x23;
        }
        unitSetPhase(object, code);
        sefApplyRecordMode(object->unit, (310 << 12) | 21, 1);
        step->state++;
    }
    if (step->poseIndex < UNIT_POSE_RAMP_FRAMES) {
        snapshot->vector.y = unitPoseRamp(step->poseIndex, 1);
        objPosePublish(object);
        step->poseIndex = step->poseIndex + 1;
    }
    if (unitCmdMoveCalc(object) != 0) {
        unitSetPhase(object, (unitCurrentPhase(object) ^ 0x22) == 0 ? 0x24 : 0x27);
        unitEffectStart(object, 310, 20);
        seObjectVoiceStartDefault(object, 14);
        if ((object->flags & UNIT_FLAG_SCRIPT_EVENTS) != 0) {
            memset(&message, 0, sizeof(message));
            message.unit = object;
            unitScriptPairEventSend(UNIT_SCRIPT_KIND_PAIR, &message);
        }
        object->flags &= ~1;
        return 1;
    }
    return 0;
}

/* Finishes at once: unitRefreshPhase, clears the active flag, reports done. */
int unitCmdFinish(BattleUnit *object, Ov01Command *unusedStep)
{
    unitRefreshPhase(object);
    object->flags &= ~1;
    return 1;
}

/* Command 0x2C, then waits for unitIsDone; clears the active flag when done. */
int unitCmdBted(BattleUnit *object, Ov01Command *step)
{
    if (step->state == 0) {
        unitSetPhase(object, 0x2C);
        step->state++;
    } else if (unitIsDone(object) != 0) {
        object->flags &= ~1;
        return 1;
    }
    return 0;
}

/* Clears the active flag and reports done at once. */
int unitCmdRec(BattleUnit *object, Ov01Command *unusedStep)
{
    object->flags &= ~1;
    return 1;
}

/*
 * Command 0x2B, then waits. When motion->breakState is 2 it sets step->command to
 * 0x1C instead and returns without issuing the command.
 */
int unitCmdWake(BattleUnit *object, Ov01Command *step)
{
    if (step->state == 0) {
        object->flags &= ~UNIT_FLAG_DOWN;
        if (object->stats->breakState == UNIT_BREAK_STATE_DOWN) {
            step->command = UNIT_CMD_MOVE_RATIO;
            return 0;
        }
        unitSetPhase(object, 0x2B);
        step->state++;
    } else if (unitIsDone(object) != 0) {
        unitRefreshPhase(object);
        object->flags &= ~1;
        return 1;
    }
    return 0;
}

/* Object flag bit 0x1: unitRunCommands sets it while a command entry runs; the handlers that
 * finish an entry clear it, as unitCmdFinish does. */
#define UNIT_FLAG_ACTIVE 0x1

/* unitCmdTurn snaps the angle to the heading once the object has waited this many runs. */
#define TURN_SNAP_WAITS 5

/* The pose ramp table has eight entries (unitPoseRamp). */
#define POSE_RAMP_COUNT 8

/* The limit block of the actor's model pack (XenoActor.rsrc[XAct_Rsrc_MDL]); only its four extents are read. */
typedef struct Ov01Limit {
    unsigned char unmodeled_00[0x90];
    f32 extentA; /* +0x90 */
    unsigned char unmodeled_94[0x4];
    f32 extentB; /* +0x98 */
    unsigned char unmodeled_9C[0x4];
    f32 extentC; /* +0xA0 */
    unsigned char unmodeled_A4[0x4];
    f32 extentD; /* +0xA8 */
} Ov01Limit;

/*
 * objPosePublish and objPoseFetch move the actor's position as two 8-aligned words, an alias-0 access. Making
 * XenoActor.position a union (or giving it a u64 view) would give every access of main's code alias set 0 and change
 * the code of uwamonoPlayerUpdate and Get_Off_Ladder, and a union around BattleUnit.unit does the same to the
 * other units, so these two functions reach the position through a view that starts like the actor
 * (struct XenoActor of main/xenoActor.h, +0x10).
 */
typedef struct UnitActorSnapshotView {
    unsigned char unmodeled_00[0x10];
    DoublewordVector4 snapshot; /* +0x10: XenoActor.position */
} UnitActorSnapshotView;

/* The same view of the unit pose (SlideBlock.position, +0x00 of the block). */
typedef struct UnitPoseSnapshotView {
    DoublewordVector4 snapshot;
} UnitPoseSnapshotView;

typedef struct Ov01Lookup Ov01Lookup;

typedef struct Ov01Sequence Ov01Sequence;

/* A table row addressed by a key, its word at +0x08. */
typedef union Ov01LookupWord {
    unsigned short value;
    int index;
    float scale;
} Ov01LookupWord;

struct Ov01Lookup {
    unsigned char unmodeled_00[0x8];
    Ov01LookupWord entryWord;   /* +0x08: read as a row value, an index or a float */
    unsigned char unmodeled_0C[0x10 - 0x0C];
    float goalZ;                /* +0x10: the z goal when the mode is neither 0 nor 1 */
};

struct Ov01Sequence {
    unsigned char unmodeled_00[0x4];
    int step;                   /* +0x04 */
    int loadId;                 /* +0x08: the snapshot id a load passes on */
};

/* Prototypes of this TU's functions and of the callees. */
float unitCmdLenGet(BattleUnit *object);
float unitColiGet(BattleUnit *unusedFirst, BattleUnit *value, int rowKey);

int unitCmdStat(BattleUnit *object, Ov01Command *unusedStep) {
    object->flags &= ~UNIT_FLAG_ACTIVE;
    return 1;
}

/* One step of the pose for the step record. Its first run aims the goal and
 * the heading; every run steps the angle, publishes the pose and returns
 * whether the angle arrived. After five waits the angle snaps to the heading
 * and the call reports done. */
int unitCmdTurn(BattleUnit *object, Ov01Command *step) {
    SlideBlock *pose;
    int arrived;

    pose = &object->pose;
    if (step->state == 0) {
        unitCmdDstSet(object, 3, step->poseIndex == 0 ? 0 : 2);
        objPoseHeadingUpdate(object, 0);
        if (object->stats->state >= 4) {
            pose->angleRate *= 0.25f;
        }
        step->state += 1;
    }
    arrived = slideAngleStep(pose);
    objPosePublish(object);
    if (object->commandQueue.flag >= TURN_SNAP_WAITS) {
        pose->angle = pose->goalAngle;
        return 1;
    }
    return arrived != 0;
}

int unitCmdStreamPlay(BattleUnit *object, Ov01Command *unusedStep) {
    seVoiceStartTagged(2, 2);
    streamPlayByKey(object, 3);
    return 1;
}

int unitCmdRemove(BattleUnit *object, Ov01Command *unusedStep) {
    unitRemove(object);
    return 1;
}

int unitCmdFinishRefresh(BattleUnit *object, Ov01Command *unusedStep) {
    /* mode 2 and mode 3 skip the call */
    if ((unsigned int) ((unsigned char) object->stats->breakState - 2) >= 2U) {
        unitRefreshPhase(object);
    }
    object->flags &= ~UNIT_FLAG_ACTIVE;
    return 1;
}

/* Clears the object's pending work: slot 2 is finished when the key's row byte
 * is 5, the source's busy flag is cleared, and each set slot finishes its work
 * when busy. */
void unitSlotsClear(BattleUnit *object, int rowKey) {
    void *fifo;
    void *list;
    struct XenoActor *source;
    TecRecord *row;
    int slot;

    cmdListPtrGet(&fifo, &list);
    if (fifoNumGet(fifo) == 0) {
        object->flags |= 0x800;
    }
    row = tecRecordGet(rowKey);
    if (row->techClass == 5) {
        source = object->children[2];
        if (source != 0) {
            source->rsrc[XAct_Rsrc_5] = 0;
            XenoActorRelease(source);
            object->children[2] = 0;
        }
    }
    source = object->unit;
    source->rsrc[XAct_Rsrc_5] = 0;
    for (slot = 0; slot < 3; slot++) {
        source = object->children[slot];
        if (source != 0) {
            if (source->rsrc[XAct_Rsrc_5] != 0) {
                XenoActorClearChannel(source);
            }
            source->rsrc[XAct_Rsrc_5] = 0;
        }
    }
}

/* Clears the object's per-attack hit flags. */
void unitHitFlagsClear(BattleUnit *object) {
    object->flags &= ~(UNIT_FLAG_HIT_REACTED | UNIT_FLAG_HIT_PANELS);
}

/* Queues the object's commands for its motion mode: mode 3 queues 29 first,
 * and mode 2 queues 28 as well when unitDeadSpecChk says so; command 13 always
 * follows. The object gets UNIT_FLAG_DOWN and the call returns 1, or it returns 0
 * when it already has that flag or its motion is held. */
int unitCmdDeadSet(BattleUnit *object) {
    Ov01Command command;

    if ((object->flags & UNIT_FLAG_DOWN) != 0) {
        return 0;
    }
    if (object->stats->hp == 0) {
        if (object->stats->breakState == 3) {
            command.command = 29;
            unitCmdSet(object, &command);
        }
        if (object->stats->breakState == 2) {
            if (unitDeadSpecChk(object) != 0) {
                command.command = 28;
                unitCmdSet(object, &command);
            }
        }
        command.command = 13;
        unitCmdSet(object, &command);
        object->flags |= UNIT_FLAG_DOWN;
        return 1;
    }
    return 0;
}

int unitDeadSpecChk(BattleUnit *object) {
    switch (object->stats->key) {
    case 110:
    case 115:
    case 119:
    case 122:
    case 124:
    case 126:
    case 139:
    case 140:
        return 1;
    }
    return 0;
}

/* Sets the pose goal (x, y, z, w) from one of three sources: mode 0 reads the
 * partner's motion, mode 1 reads the lookup entry's index, and any other mode
 * reads the entry's floats. With cmdId set and mode 0 the z goal is limited
 * against the partner's slide limit. */
void unitCmdDstSet(BattleUnit *object, int cmdId, int mode) {
    Ov01Lookup *lookup;
    BattleUnit *partner;
    float *goal;
    int partnerKey;
    int posIndex;
    int *posRow;
    float diff;

    goal = &object->pose.goal.x;
    lookup = objCmdPtrGet(&object->commandQueue);
    if (mode == 0) {
        partner = (BattleUnit *) object->commandTarget;
        partnerKey = object->commandCode;
        posIndex = partner->stats->posIndex;
        posRow = effectPosGet(posIndex);
        goal[0] = (float) posRow[0] / 100;
        /* the original stores the word zero (sw $0) into goal[1] here */
        ((int *) goal)[1] = 0;
        posRow = effectPosGet(posIndex);
        goal[2] = (float) posRow[2] / 100.0f;
        if (cmdId != 0) {
            diff = unitCmdLenGet(object) - unitColiGet(object, partner, partnerKey);
            if (0.0f > diff) {
                diff = 0.0f;
            }
            slideLimitGoalXZ(&object->pose, diff);
        }
    } else if (mode == 1) {
        posIndex = lookup->entryWord.index;
        posRow = effectPosGet(posIndex);
        goal[0] = (float) posRow[0] / 100.0f;
        goal[1] = 0.0f;
        posRow = effectPosGet(posIndex);
        goal[2] = (float) posRow[2] / 100.0f;
    } else {
        goal[0] = lookup->entryWord.scale;
        goal[1] = 0.0f;
        goal[2] = lookup->goalZ;
    }
    goal[3] = 1.0f;
}

void unitCmdVASet(BattleUnit *object, int mode, int unusedFlag) {
    SlideBlock *pose;

    pose = &object->pose;
    unitCmdLenGet(object);
    pose->acceleration.x = 0.0f;
    pose->acceleration.z = 0.0f;
    pose->velocity.x = (pose->goal.x - pose->position.x) / 15.0f;
    pose->velocity.z = (pose->goal.z - pose->position.z) / 15.0f;
    if (mode == 3 || mode == 4) {
        pose->acceleration.y = -0.0204375f;
        pose->velocity.y = 0.15328124f;
    } else {
        pose->acceleration.y = 0.0f;
        pose->velocity.y = (pose->goal.y - pose->position.y) / 15.0f;
    }
}

float unitCmdLenGet(BattleUnit *object) {
    return slideHorizontalDistance(&object->pose);
}

/* Blends the pose snapshot toward its start by numerator (tail call; the
 * second argument passes through unchanged to slideLimitGoalXZ). */
void unitCmdDivSet(BattleUnit *object, f32 numerator) {
    slideLimitGoalXZ(&object->pose, numerator);
}

/* One step of the pose snapshot, then the publish; returns the step's result. */
int unitCmdMoveCalc(BattleUnit *object) {
    int arrived;

    arrived = objStdMove(&object->pose);
    objPosePublish(object);
    return arrived;
}

/* unusedFirst keeps its argument slot; the body never reads it. */
float unitColiGet(BattleUnit *unusedFirst, BattleUnit *value, int rowKey) {
    float base;

    if (value != 0) {
        base = unitDefArea(value);
    } else {
        base = 0.0f;
    }
    return base + ((float) tecRecordGet(rowKey)->scale / 100.0f);
}

/* Half the summed absolute extents of the limit's two axis pairs, the larger
 * of the two; when the motion sets a percent it returns that percent as a
 * fraction instead. */
float unitDefArea(BattleUnit *object) {
    Ov01Limit *limit;
    float result;
    float pairAC;
    float pairBD;
    float magnitude;
    float other;

    if (object->stats->slidePercent == 0) {
        limit = (Ov01Limit *) object->unit->rsrc[XAct_Rsrc_MDL];
        magnitude = limit->extentA;
        if (magnitude < 0.0f) {
            magnitude = -magnitude;
        }
        other = limit->extentC;
        if (other < 0.0f) {
            other = -other;
            pairAC = (magnitude + other) * 0.5f;
        } else {
            pairAC = (magnitude + other) * 0.5f;
        }
        do {
            magnitude = limit->extentB;
        } while (0);
        if (magnitude < 0.0f) {
            magnitude = -magnitude;
        }
        other = limit->extentD;
        if (other < 0.0f) {
            other = -other;
            pairBD = (magnitude + other) * 0.5f;
        } else {
            pairBD = (magnitude + other) * 0.5f;
        }
        if (pairBD < pairAC) {
            return pairAC;
        }
        result = pairBD;
    } else {
        result = (float) object->stats->slidePercent / 100.0f;
    }
    return result;
}

/* Publishes the pose snapshot and angle to the source and to the motion. */
void objPosePublish(BattleUnit *object) {
    struct XenoActor *source;
    long long snapshotLow;
    float angle;

    do {
        snapshotLow = ((UnitPoseSnapshotView *) &object->pose)->snapshot.words[0];
    } while (0);
    source = object->unit;
    ((UnitActorSnapshotView *) source)->snapshot.words[0] = snapshotLow;
    do {
        ((UnitActorSnapshotView *) source)->snapshot.words[1] = ((UnitPoseSnapshotView *) &object->pose)->snapshot.words[1];
    } while (0);
    angle = object->pose.angle;
    source->rotation.y = angle;
    object->stats->facing = angle;
}

/* Loads the pose snapshot from the source and returns its low word. */
long long objPoseFetch(BattleUnit *object) {
    struct XenoActor *source;
    long long snapshotLow;

    source = object->unit;
    XenoActorApplyRootOffset(source, 1, 0, 1);
    snapshotLow = ((UnitActorSnapshotView *) source)->snapshot.words[0];
    ((UnitPoseSnapshotView *) &object->pose)->snapshot.words[0] = snapshotLow;
    ((UnitPoseSnapshotView *) &object->pose)->snapshot.words[1] = ((UnitActorSnapshotView *) source)->snapshot.words[1];
    return snapshotLow;
}

int unitCmdRun(BattleUnit *object, int cmdId, int flip) {
    Ov01Sequence *sequence;
    SlideBlock *pose;
    struct {
        int group;
        int record;
    } effect;

    sequence = objCmdPtrGet(&object->commandQueue);
    pose = &object->pose;
    runEffGet(&effect.group, &effect.record);
    switch (sequence->step) {
    case 0:
        unitSetPhase(object, 4);
        objPoseHeadingUpdate(object, 0);
        pose->angleRate = pose->angleRate / 10.0f;
        slideAngleStep(pose);
        objPosePublish(object);
        sequence->step += 1;
        /* fall through */
    case 1:
        if (unitIsDone(object) != 0) {
            unitSetPhase(object, 5);
            unitCmdVASet(object, cmdId, flip);
            if (effect.group == 0 && effect.record == 0) {
                effect.record = 9;
                effect.group = object->stats->sequenceIndex;
            }
            unitEffectStart(object, effect.group, effect.record);
            seObjectVoiceStartDefault(object, 1);
            sequence->step++;
        } else {
            slideAngleStep(pose);
            objPosePublish(object);
        }
        break;
    case 2:
        if (unitCmdMoveCalc(object) == 0) {
            break;
        }
        if (effect.group == 0 && effect.record == 0) {
            effect.record = 9;
            effect.group = object->stats->sequenceIndex;
        }
        sefApplyRecordMode(object->unit, (effect.group << 12) | effect.record, 6);
        return 1;
    }
    return 0;
}

/* One step of the object's sequence: the step in the looked-up entry selects
 * the branch, and the branch advances the phase. unusedFlag is the move flag unit_cmd.c passes; this body never reads it. */
int unitCmdJump(BattleUnit *object, int phase, int unusedFlag) {
    SlideBlock *pose;
    Ov01Sequence *sequence;
    float zScale;
    int posTarget;

    pose = &object->pose;
    sequence = objCmdPtrGet(&object->commandQueue);
    switch (sequence->step) {
    case 0:
        if (phase == 3) {
            phase = 4;
            posTarget = 0;
        } else {
            phase = 9;
            posTarget = 1;
        }
        objPoseHeadingUpdate(object, posTarget);
        slideAngleStep(pose);
        objPosePublish(object);
        zScale = unitCmdLenGet(object);
        if (object->stats->state >= 4) {
            zScale /= 10.0f;
        } else {
            zScale /= 3.0f;
        }
        unitSetVectorZ(object, zScale);
        pose->angleRate = zScale;
        unitSetPhase(object, phase);
        sequence->step++;
        break;
    case 1:
        if (!unitIsDone(object)) {
            break;
        }
        posTarget = (phase != 3);
        phase = 7;
        if (posTarget) {
            phase = 10;
        }
        seObjectVoiceStartDefault(object, 2);
        unitSetPhase(object, phase);
        sequence->step++;
        break;
    case 2:
        if (!unitIsDone(object)) {
            break;
        }
        if (phase == 3) {
            object->flags |= 0x20000;
            objPoseFetch(object);
            slideSnapPosToGoal(pose);
            objPosePublish(object);
            unitSetVectorZ(object, 1.0f);
            unitSetPhase(object, 6);
            return 1;
        }
        unitSetPhase(object, (phase == 4) ? 11 : 6);
        sequence->step++;
        break;
    case 3:
        if (!unitIsDone(object)) {
            break;
        }
        object->flags |= 0x20000;
        if (phase == 4) {
            objPoseFetch(object);
            slideSnapPosToGoal(pose);
            objPosePublish(object);
            unitSetVectorZ(object, 1.0f);
        }
        return 1;
    default:
        break;
    }
    return 0;
}

int unitCmdHover(BattleUnit *object, int cmdId, int flip) {
    SlideBlock *pose;
    Ov01Sequence *sequence;

    pose = &object->pose;
    sequence = objCmdPtrGet(&object->commandQueue);
    switch (sequence->step) {
    case 0:
        seObjectVoiceStartDefault(object, 1);
        unitFadeOut(object);
        sequence->step++;
        break;
    case 1:
        if (object->fade != 0) {
            break;
        }
        objPoseHeadingUpdate(object, cmdId != 5);
        slideAngleStep(pose);
        objPosePublish(object);
        if (flip == 0) {
            pose->position.x = pose->goal.x;
            pose->position.y = pose->goal.y;
            pose->position.z = pose->goal.z;
            objPosePublish(object);
        } else {
            unitLoadSnapshot(object, sequence->loadId);
        }
        unitRefreshPhase(object);
        sequence->step++;
        break;
    case 2:
        if (cmdId == 5) {
            unitUnbind(object);
            return 1;
        }
        unitFadeIn(object);
        sequence->step++;
        break;
    case 3:
        if (object->fade == 0) {
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

/* Looks the object's key up; with action 1 it copies the entry's value into
 * the motion and runs unitRefreshPhase. */
void unitCmdMoveNext(BattleUnit *object, int action) {
    Ov01Lookup *lookup;

    lookup = objCmdPtrGet(&object->commandQueue);
    if (action == 1) {
        object->stats->posIndex = lookup->entryWord.value;
        unitRefreshPhase(object);
    }
}

/* Heading from the snapshot to the pose's current point (atan2 of the x and
 * z deltas), wrapped, with motion->facing kept when the two points coincide. */
int objPoseHeadingUpdate(BattleUnit *object, int faceAway) {
    SlideBlock *pose;
    float delta;
    float stillMagnitude;
    float wrapMagnitude;
    float finalMagnitude;

    pose = &object->pose;
    if (pose->goal.x == pose->position.x
        && pose->goal.z == pose->position.z) {
        pose->goalAngle = object->stats->facing;
    } else {
        pose->goalAngle = atan2f(pose->goal.x - pose->position.x,
                               pose->goal.z - pose->position.z);
        if (faceAway == 1) {
            pose->goalAngle += 3.1415927f;
        }
    }
    if (pose->goalAngle < 0.0f) {
        pose->goalAngle += 6.2831855f;
    }
    if (pose->goalAngle >= 6.2831855f) {
        pose->goalAngle -= 6.2831855f;
    }
    delta = pose->goalAngle - pose->angle;
    if ((delta < 0.0f ? (stillMagnitude = -delta) : delta) <= 0.000001f) {
        pose->angleRate = 0.0f;
        return 0;
    }
    if ((delta < 0.0f ? (wrapMagnitude = -delta) : delta) > 3.1415927f) {
        if (delta < 0.0f) {
            delta += 6.2831855f;
        } else {
            delta -= 6.2831855f;
        }
    }
    if ((delta < 0.0f ? (finalMagnitude = -delta) : delta) <= 0.000001f) {
        delta = 0.0f;
    }
    pose->angleRate = delta;
    return 1;
}

/* The eight ramp values; reverse reads the table from its far end. */
float unitPoseRamp(int index, int reverse) {
    float rampTable[POSE_RAMP_COUNT] = {0.0f, 1.064f, 1.842f, 2.378f, 2.717f, 2.904f, 2.983f, 3.0f};
    float ramp = 0.0f;

    if (index < POSE_RAMP_COUNT) {
        int slot = index;
        if (reverse != 0) {
            slot = POSE_RAMP_COUNT - 1;
            slot -= index;
        }
        ramp = rampTable[slot];
    }
    return ramp;
}
