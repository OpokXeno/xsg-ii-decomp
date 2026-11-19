/*
 * OV01 original TU 15: 0x00a865d8..0x00a86cb0 (10 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov01/tu012.h"
#include "ov01/tu016.h"
#include "ov01/tu033.h"
#include "ov01/tu041.h"
#include "ov01/tu042.h"
#include "ov01/tu091.h"
#include "ov01/tu071.h"

/* Values of Ov01Motion.moveMode: unitCmdMoveRatio sets RATIO, the pose ramp (unitCmdMoveRamp) sets RAMP. */
enum {
    UNIT_PARAM_MODE_RATIO = 0,
    UNIT_PARAM_MODE_RAMP = 2
};

/* Frames of the pose ramp: unitCmdMoveRamp advances poseIndex up to this value. */
#define UNIT_POSE_RAMP_FRAMES 8

/* The pose index viewed as a plain int through a union (alias set 0), so its store keeps its place in the block. */
union UnitPoseIndexWord {
    int value;
};

/* Command 0xE, then waits for unitIsDone to report done. */
int unitCmdPhase0EWait(Ov01Object *object, Ov01Command *step)
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
int unitCmdPhase12Wait(Ov01Object *object, Ov01Command *step)
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
 * objPoseStep reports arrival.
 */
int unitCmdPoseArrive(Ov01Object *object, Ov01Command *step)
{
    Ov01Pose *pose;

    pose = &object->pose;
    if (step->state == 0) {
        unitCmdDstSet(object, 3, 0);
        objPoseSetSteps(object, 3, 0);
        objPoseHeadingUpdate(object, 0);
        pose->angle = pose->heading;
        objPosePublish(object);
        unitSetPhase(object, 0xF);
        step->state++;
    } else if (objPoseStep(object) != 0) {
        return 1;
    }
    return 0;
}

/*
 * Phase 0 sets the pose up and sends command 0x12, then falls into the arrival
 * test. The test sends 0x13 and advances once objPoseStep reports arrival; later
 * phases wait for unitIsDone.
 */
int unitCmdPoseArriveWait(Ov01Object *object, Ov01Command *step)
{
    switch (step->state) {
    case 0:
        unitCmdDstSet(object, 4, 1);
        objPoseSetSteps(object, 4, 0);
        unitSetPhase(object, 0x12);
        step->state++;
        /* fall through */
    case 1:
        if (objPoseStep(object) != 0) {
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
int unitCmdMoveRatio(Ov01Object *object, Ov01Command *step)
{
    Ov01Pose *pose;
    s16 effectPosIndex;
    f32 primary;

    pose = &object->pose;
    if (step->state == 0) {
        object->stats->state70 = UNIT_PARAM_MODE_RATIO;
        effectPosIndex = object->stats->posIndex;
        primary = (f32) *effectPosGet(effectPosIndex);
        pose->snapshot.vector.y = 0.0f;
        pose->snapshot.vector.x = primary / 100.0f;
        pose->snapshot.vector.z = (f32) effectPosGet(effectPosIndex)[2] / 100.0f;
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

void sefApplyRecordMode(void *source, int id, int flag);
void *memset(void *dst, int value, unsigned int size);

/*
 * Phase 0 turns the unit toward its target, sets the pose blend and starts command
 * 0x23 or 0x22 with source sound 0x136015. Each call advances the eight-frame ramp
 * while poseIndex is below UNIT_POSE_RAMP_FRAMES. Once objPoseStep reports arrival, the step plays
 * command 0x27 or 0x24, the voice, and clears the active flag.
 */
int unitCmdMoveRamp(Ov01Object *object, Ov01Command *step)
{
    Ov01Pose *pose;
    UnitScriptPairEvent message;
    int facing;
    int code;
    f32 zero;

    pose = &object->pose;
    if (step->state == 0) {
        object->stats->state70 = UNIT_PARAM_MODE_RAMP;
        facing = 0;
        if (unitCurrentPhase(object) != 0x20) {
            facing = 1;
        }
        unitFaceTarget(object, facing, object->stats->source);
        zero = 0.0f;
        pose->stepX = (pose->point.x - pose->snapshot.vector.x) * 0.125f;
        pose->stepZ = (pose->point.z - pose->snapshot.vector.z) * 0.125f;
        pose->easeZ = zero;
        pose->easeY = zero;
        pose->easeX = zero;
        pose->stepY = zero;
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
        pose->snapshot.vector.y = unitPoseRamp(step->poseIndex, 1);
        objPosePublish(object);
        step->poseIndex = step->poseIndex + 1;
    }
    if (objPoseStep(object) != 0) {
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
int unitCmdFinish(Ov01Object *object)
{
    unitRefreshPhase(object);
    object->flags &= ~1;
    return 1;
}

/* Command 0x2C, then waits for unitIsDone; clears the active flag when done. */
int unitCmdPhase2CWait(Ov01Object *object, Ov01Command *step)
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
int unitCmdClearActive(Ov01Object *object)
{
    object->flags &= ~1;
    return 1;
}

/*
 * Command 0x2B, then waits. When motion->state70 is 2 it sets step->command to
 * 0x1C instead and returns without issuing the command.
 */
int unitCmdPhase2BWait(Ov01Object *object, Ov01Command *step)
{
    if (step->state == 0) {
        object->flags &= ~4;
        if (object->stats->state70 == UNIT_PARAM_MODE_RAMP) {
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
