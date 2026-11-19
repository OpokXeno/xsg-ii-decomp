/*
 * OV01 original TU 12: 0x00a84830..0x00a85a80 (37 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/xenoActor.h"
#include "ov01/tu005.h"
#include "main/tu051.h"
#include "ov01/tu012.h"
#include "ov01/tu016.h"
#include "ov01/tu033.h"
#include "ov01/tu036.h"
#include "ov01/tu042.h"
#include "ov01/tu071.h"

/* keyHigh of the high modes (mode 16 and up): unitActionKey uses 320, unitMoveKey uses 330. */
#define ACTION_KEY_BASE 320
#define MOVE_KEY_BASE 330

void unitRunCommands(Ov01Object *object);

/* Per-frame step: clears flag 0x20000, runs the command queue, copies the unit's snapshot into the pose, then calls
 * unitStatusAuraUpdate. */
void unitUpdate(Ov01Object *object) {
    object->flags &= ~0x00020000u;
    unitRunCommands(object);
    object->pose.snapshot.vector = object->unit->position;
    unitStatusAuraUpdate(object);
}

void func_00A84880(void) {

}

void unitClearBindFlags(Ov01Object *object);

void unitUnbind(Ov01Object *object) {
    unitClearBindFlags(object);
    func_00A955C8(object);
}

/* Clears bits 0x10004 of the unit and of each child whose slot flag 0x4000 is clear, and bit 2 of the object. */
void unitClearBindFlags(Ov01Object *object) {
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

void unitSetBindFlags(Ov01Object *object);

void unitBindSetup(Ov01Object *object) {
    unitSetBindFlags(object);
    func_00A95608(object);
}

/* Sets bit 4 of the unit and of each child, and bit 2 of the object: the mirror of unitClearBindFlags. */
void unitSetBindFlags(Ov01Object *object) {
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
int unitCurrentPhase(Ov01Object *object) {
    return object->unit->mode + 1;
}

/* Phase setter: the callers pass fixed phase numbers, and it updates the unit's status bit 2 and its children. */
void XenoActorSetMode(struct XenoActor *actor, int mode);

void unitSetPhase(Ov01Object *object, int phase) {
    struct XenoActor *unit;
    f32 channelValue;
    int i;

    if (phase == 0) {
        return;
    }
    unit = object->unit;
    if (unit->phaseLock == 0 && unit->mode == phase - 1) {
        if ((unsigned int) (phase - 28) >= 4 && phase != 25 && phase != 4 && (unsigned int) (phase - 49) >= 3 && phase != 36 && phase != 39) {
            return;
        }
    }
    if (phase == 1 || phase == 2 || (phase == 5 && object->stats->subState != 4)) {
        unit->flags &= ~2;
    } else {
        unit->flags |= 2;
    }
    if ((unit->flags & 0x200) == 0 && unit->phaseLock == 0 && ((phase == 2 && unitCurrentPhase(object) != 11) || unitCurrentPhase(object) == 2)) {
        channelValue = 8.0f;
    } else {
        channelValue = 0.0f;
    }
    XenoActorSetChannelValue(unit, channelValue);
    XenoActorEnableChannel(unit);
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
int xenoActorSlotRangeValid(void *, int, int);

void unitSetTimedPhase(Ov01Object *object, int phase) {
    struct XenoActor *unit;
    int i;

    if (phase == 0) {
        return;
    }
    unit = object->unit;
    unit->flags |= 2;
    XenoActorEnableChannel(unit);
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
void unitSetSelector(Ov01Object *object, int selector) {
    struct XenoActor *unit;

    if ((unsigned int) (object->stats->keyUnsigned - 1) < 10) {
        unit = object->unit;
        if (!(unit->flags & 0x200)) {
            XenoActorSetChannelValueB(unit, 7.0f);
            XenoActorEnableChannelB(unit);
        }
        XenoActorSetSelector(unit, selector);
    }
}

/* Called through an unprototyped declaration: ov01/tu023.c defines partyUnitHpLow(StatOwner *), but the original call here passed no unit (tu062 declares it the same way). */
int partyUnitHpLow();

/* Picks phase 2 with selector 6 when partyUnitHpLow holds and the mode is 1 to 10, else phase 1 with selector 0. */
void unitRefreshPhase(Ov01Object *object) {
    int phase;
    int selector;

    if ((partyUnitHpLow() != 0) && ((unsigned int) (object->stats->keyUnsigned - 1) < 10)) {
        phase = 2;
        selector = 6;
    } else {
        phase = 1;
        selector = 0;
    }
    unitSetPhase(object, phase);
    unitSetSelector(object, selector);
}

void XenoActorSetVector(struct XenoActor *actor, float, float, float);

/* Calls XenoActorSetVector on the unit with x and y scale 1.0 and the third component scaleZ. */
void unitSetVectorZ(Ov01Object *object, float scaleZ) {
    XenoActorSetVector(object->unit, 1.0f, 1.0f, scaleZ);
}

/* Returns XenoActorIsDone's result: the state handlers test it as a done flag. */
int unitIsDone(Ov01Object *object) {
    return XenoActorIsDone(object->unit);
}

int sefNoOwnerFlaggedRecords(int key);

/* The key is keyHigh << 12 | keyLow; the unit is not read. */
int unitEffectKeyFree(Ov01Object *unit, int keyHigh, int keyLow) {
    return sefNoOwnerFlaggedRecords((keyHigh << 0xC) | keyLow) != 0;
}

#include "ov01/tu041.h"
#include "ov01/tu054.h"

/* Loads the snapshot vector from the table entry for id (three ints divided by 100) and records id in the motion. */
void unitLoadSnapshot(Ov01Object *object, int id) {
    int *entry;
    Ov01Pose *pose;

    entry = effectPosGet(id);
    if (entry != 0) {
        pose = &object->pose;
        pose->snapshot.vector.x = (f32) entry[0] / 100.0f;
        pose->snapshot.vector.y = (f32) entry[1] / 100.0f;
        pose->snapshot.vector.z = (f32) entry[2] / 100.0f;
        objPosePublish(object);
        object->stats->posIndex = id;
    }
}

void unitSetAngleRadians(Ov01Object *object, f32 angle);

/* Degrees to radians (pi / 180), then unitSetAngleRadians. */
void unitSetAngleDegrees(Ov01Object *object, int degrees) {
    unitSetAngleRadians(object, (f32) degrees * 0.017453292f);
}

/* Stores the angle in radians, wrapped to [0, 2 pi), in the pose and in the motion. */
void unitSetAngleRadians(Ov01Object *object, f32 angle) {
    Ov01Pose *pose;

    pose = &object->pose;
    pose->angle = angle;
    if (angle < 0.0f) {
        pose->angle = angle + 6.2831855f;
    }
    objPosePublish(object);
    object->stats->facing = pose->angle;
}

/* The motion angle in degrees (radians times 180 / pi), truncated; 0 for a null object. */
int unitGetAngleDegrees(Ov01Object *object) {
    if (object == 0) {
        return 0;
    }
    return (int) (object->stats->facing * 57.295776f);
}

/* Zeroes the snapshot's first three words, then calls objPosePublish. */
void unitClearSnapshot(Ov01Object *object) {
    Ov01Pose *pose = &object->pose;

    pose->snapshot.vector.x = 0.0f;
    pose->snapshot.vector.y = 0.0f;
    pose->snapshot.vector.z = 0.0f;
    objPosePublish(object);
}

/* Speed factor by motion state: 2.0 for state 3, 3.0 for state 4, else 1.0. */
f32 unitStateScale(Ov01Object *object) {
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
void xglVectorMulMat(Vector4 *destination, Matrix4 matrix, const Vector4 *vector);
void XenoActorPushMatrix(struct XenoActor *actor);

void unitFaceTarget(Ov01Object *object, int facesTarget, Ov01Object *target) {
    DoublewordVector4 *targetWords;
    Ov01Pose *pose;
    f32 heading;
    Vector4 offset;

    pose = &object->pose;
    targetWords = &target->pose.snapshot;
    if (target != 0) {
        DoublewordVector4 *words;
        StatBlock *motion;

        words = &pose->snapshot;
        heading = atan2f(words->vector.x - targetWords->vector.x, words->vector.z - targetWords->vector.z);
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
    xglVectorMulMat(&pose->point, object->unit->matrix, &offset);
}

/*
 * Writes the effect key pair (keyHigh, keyLow) of the action named by the
 * motion: keyHigh is the sequence index, or 320 for the high-mode sequences;
 * keyLow picks the variant from the sequence flags. Its mirror is unitMoveKey
 * (the move key, base 330, variants 1 to 3 and default 11).
 */
void unitActionKey(Ov01Object *object, int *keyHigh, int *keyLow) {
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
 * and variants 1 to 3, default 11 (the move dispatchers in tu013 start it).
 */
void unitMoveKey(Ov01Object *object, int *keyHigh, int *keyLow) {
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
void unitQueueAppend(Ov01Object *object, Ov01Command *command) {
    dataLookupAppend(&object->commandQueue, command);
}

/* Empties the object's command queue. */
void unitQueueClear(Ov01Object *object) {
    dataLookupReset(&object->commandQueue);
}

/* Nonzero when the object's command queue has no pending entry. */
int unitQueueIsEmpty(Ov01Object *object) {
    return dataLookupCurrent(&object->commandQueue) == 0;
}

/*
 * Runs the object's command queue. Each entry starts with a type below 0x28
 * that indexes the handler table unitCommandTable; a handler returning nonzero
 * retires the entry and resets the wait counter at +0x138, a zero return
 * keeps it and counts.
 */
extern int (*unitCommandTable[UNIT_COMMAND_COUNT])(Ov01Object *, Ov01Command *);

void unitRunCommands(Ov01Object *object) {
    Ov01Command *command;

    command = dataLookupCurrent(&object->commandQueue);
    while (command != 0 && command->command < UNIT_COMMAND_COUNT) {
        if (command->command != 0 && object->commandQueue.flag == 0) {
            object->flags |= 1;
        }
        if (unitCommandTable[command->command](object, command) == 0) {
            object->commandQueue.flag++;
            break;
        }
        object->commandQueue.flag = 0;
        dataLookupNext(&object->commandQueue);
        command = dataLookupCurrent(&object->commandQueue);
    }
}

/* Handler table unitCommandTable entry 0: no operation, always done. */
int unitCmdNoop(void) {
    return 1;
}


int unitCmdSetPhase(Ov01Object *object, Ov01Command *step) {
    TecRecord *record;

    record = tecRecordGet(step->code);
    switch (step->state) {
    case 0:
        if (record->timer == 0) {
            unitSetPhase(object, record->phase);
        } else {
            unitSetTimedPhase(object, record->phase);
        }
        if (record->hitFlags & 0x100) {
            object->flags |= 0x2000;
        }
        if (record->rowGate[0] & 0x10) {
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

int ov01RollPercent(int percent);
int seVoiceStartTagged(int tag, int id);
int calcStatEffectSum(Ov01Object *object, int effectMask, int statMask);
void calcEpAdjust(Ov01Object *object, int amount);
void calcHpAdjust(Ov01Object *object, int amount);
void unitLevelAdd(Ov01Object *object, int level);
void unitCurrentAdd(Ov01Object *object, int amount);
int unitHasEffectCode(Ov01Object *object, int code);
void calcStatResetCha(Ov01Object *object);

/* Stat-effect step: state 0 starts the object; later states wait for unitIsDone, then apply the hp and ep effects and the level or current change, and set flag 0x4000. */
int unitCmdStatEffect(Ov01Object *object, Ov01Command *step) {
    int effect;
    int amount;
    int hpKind; /* -50 or -100: the kind argument of menuPopNumCreate for the hit-point effect */
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
        effect = calcStatEffectSum(object, 0x200, 0x8000);
        if (effect > 0) {
            int epDelta;

            hpKind = -100;
            epDelta = object->stats->maxEp;
            epDelta = epDelta * effect;
            epDelta = epDelta / 100;
            calcEpAdjust(object, epDelta);
            magnitude = epDelta;
            if (epDelta < 0) {
                magnitude = -epDelta;
            }
            menuPopNumCreate(object, magnitude, 4, -50);
        }
        effect = calcStatEffectSum(object, 0x200, 0x800);
        if (effect > 0) {
            int hpDelta;

            hpDelta = object->stats->maxHp;
            hpDelta = hpDelta * effect;
            hpDelta = hpDelta / 100;
            calcHpAdjust(object, hpDelta);
            magnitude = hpDelta;
            if (hpDelta < 0) {
                magnitude = -hpDelta;
            }
            menuPopNumCreate(object, magnitude, 2, hpKind);
        }
        value = object->stats->keyUnsigned;
        if (value - 1 < 10) {
            level = 1;
            effect = calcStatEffectSum(object, 0x200, 0x2000);
            if (effect > 0) {
                if (ov01RollPercent(50) != 0) {
                    level = effect;
                }
            }
            unitLevelAdd(object, level);
        } else if (((value - 11) & 0xFFFF) < 5) {
            amount = 100;
            effect = calcStatEffectSum(object, 0x200, 0x1000);
            if (effect > 0) {
                amount = amount + effect;
            }
            unitCurrentAdd(object, amount);
            if (unitHasEffectCode(object, 0x51) != 0) {
                calcStatResetCha(object);
            }
        }
        value = object->flags;
        object->flags = value | 0x4000;
        return 1;
    }
    return 0;
}

int func_001EDCA8(void);
void seObjectVoiceStart(void *, int, int);

int unitCmdAction(Ov01Object *object, Ov01Command *command) {
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
        seObjectVoiceStart(object, 8, func_001EDCA8());
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

int func_001EDCA0(void);

int unitCmdPlayVoice(Ov01Object *object, Ov01Command *step) {
    tecRecordGet(step->code);
    switch (step->state) {
    case 0:
        unitSetPhase(object, 23);
        seObjectVoiceStart(object, 9, func_001EDCA0());
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

int unitRemove(Ov01Object *object);
void sefReleaseWaitSlot(int key);
int handleFree(s16 handle);
int dataSlotFlagClear(s16 key);
int dataSlotReset(s16 key);

/* Command step: state 0 starts the fade-out and moves to state 1; state 1 releases the unit once no fade runs. */
int unitCmdRelease(Ov01Object *object, Ov01Command *command) {
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
        dataSlotFlagClear(object->stats->keyUnsigned);
        sefReleaseWaitSlot(object->stats->sequenceIndex);
        handleFree(object->stats->handle);
        dataSlotReset(object->stats->keyUnsigned);
        unitRemove(object);
        return 1;
    }
    return 0;
}

/* Command step: state 0 fades out, state 1 loads the snapshot once no fade runs, state 2 fades in, state 3 completes once no fade runs. */
int unitCmdLoadSnapshot(Ov01Object *object, Ov01Command *step) {
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
int unitCmdSetBattleTransition(Ov01Object *object, Ov01Command *command) {
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
                battleStateGet()->transition = 1;
            } else {
                battleStateGet()->transition = 2;
            }
            object->flags &= ~1;
            return 1;
        }
        break;
    }
    return 0;
}

/* Handler table unitCommandTable entry 8: calls unitRefreshPhase, then done. */
int unitCmdRefreshPhase(Ov01Object *object, int unusedArg) {
    unitRefreshPhase(object);
    return 1;
}

/* Handler table unitCommandTable entry 9: sets flag 0x1000, clears flag 1, calls func_00A955C8 (the unbind side) when flag 4 is clear, then refreshes the phase unless the phase kind is 2 or 3. */
int unitCmdUnbind(Ov01Object *object, int unusedArg) {
    object->flags = (object->flags | 0x1000) & ~1;
    if ((object->flags & 4) == 0) {
        func_00A955C8(object);
    }
    if ((unsigned int) ((unsigned char) object->stats->state70 - 2) < 2) {
        return 1;
    }
    return unitCmdRefreshPhase(object, unusedArg);
}

/* Handlers of entries 10 to 39 are defined by the overlay files tu013 to tu016. */
int unitCmdModeDispatch();
int func_00A85EA0();
int func_00A85F30();
int func_00A85FC0();
int unitCmdCodeWait();
int func_00A862D8();
int unitCmdMoveCha();
int unitCmdMovePos();
int unitCmdPhase0EWait();
int unitCmdPhase12Wait();
int unitCmdPoseArrive();
int unitCmdPoseArriveWait();
int unitCmdMoveChaRun();
int unitCmdMovePosJump();
int unitCmdMoveChaJump();
int unitCmdMoveChaHover();
int unitCmdMovePosHover();
int unitCmdModeRamp();
int unitCmdMoveRatio();
int unitCmdMoveRamp();
int unitCmdFinish();
int unitCmdPhase2CWait();
int unitCmdClearActive();
int unitCmdPhase2BWait();
int func_00A86CB0();
int unitCmdTurnAim();
int unitCmdStreamPlay();
int unitCmdRemove();
int unitCmdFinishRefresh();
int unitCmdModeStart();

/* unitCommandTable: the 40 handlers the command queue dispatches through, in index order. */
int (*unitCommandTable[UNIT_COMMAND_COUNT])(Ov01Object *, Ov01Command *) = {
    unitCmdNoop, unitCmdSetPhase, unitCmdStatEffect, unitCmdAction,
    unitCmdPlayVoice, unitCmdRelease, unitCmdLoadSnapshot, unitCmdSetBattleTransition,
    unitCmdRefreshPhase, unitCmdUnbind, unitCmdModeDispatch, func_00A85EA0,
    func_00A85F30, func_00A85FC0, unitCmdCodeWait, func_00A862D8,
    unitCmdMoveCha, unitCmdMovePos, unitCmdPhase0EWait, unitCmdPhase12Wait,
    unitCmdPoseArrive, unitCmdPoseArriveWait, unitCmdMoveChaRun, unitCmdMovePosJump,
    unitCmdMoveChaJump, unitCmdMoveChaHover, unitCmdMovePosHover, unitCmdModeRamp,
    unitCmdMoveRatio, unitCmdMoveRamp, unitCmdFinish, unitCmdPhase2CWait,
    unitCmdClearActive, unitCmdPhase2BWait, func_00A86CB0, unitCmdTurnAim,
    unitCmdStreamPlay, unitCmdRemove, unitCmdFinishRefresh, unitCmdModeStart
};
