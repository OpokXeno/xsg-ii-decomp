#include "common.h"
#include "main/enemy_1.h"
#include "main/set_motion.h"
#include "main/pad.h"
#include "shared.h"
#include "main/se_bank_load.h"
#include "main/game_camera.h"
#include "main/get_ladder.h"
#include "main/xenoActor.h"
#include "main/xgl_font.h"
#include "main/game_data.h"
#include "ov01/sb_vm.h"
#include "main/get.h"
#include "main/uwamono_event_check.h"
#include "main/runtime_uwamono_door.h"
#include "ov01/sb_natives.h"
#include "main/uwamono_record.h"

/* Ladder climb state. */
static short ladderRungIndex;           /* the rung the climb started from */
static float ladderClimbFrom;           /* height at the start of the current rung step */
static float ladderClimbTo;             /* height at the end of the current rung step */
static float ladderFootHeight;
static short ladderClimbFrame;          /* frames elapsed in the current rung step */
static short uwamonoLadderSlot;
static signed char ladderExitSide;      /* 1 when leaving sideways from a rung, 0 at the foot */
static short ladderFrame;               /* cleared when the exit starts */
static short ladderExitFrames;          /* frames of the exit motion (0x20 foot, 0x2A side) */
static float ladderExitOffset;          /* height offset applied during the exit (-0.13 near a rung top) */

/* JP: ハシゴ
 * EN: Ladder */
const char ladderModeText[16] = "\x0d" "\x02" "\xa5" "\xcf" "\xa5" "\xb7" "\xa5" "\xb4" "";
/* JP: 高さ合格%3d
 * EN: Height OK %3d */
const char ladderHeightPassFormat[16] = "\xb9" "\xe2" "\xa4" "\xb5" "\xb9" "\xe7" "\xb3" "\xca" "%3d";
/* JP: 左昇降可能%3d
 * EN: Left side climbable %3d */
const char ladderLeftClimbableFormat[16] = "\xba" "\xb8" "\xbe" "\xba" "\xb9" "\xdf" "\xb2" "\xc4" "\xc7" "\xbd" "%3d";
/* JP: 右昇降可能%3d
 * EN: Right side climbable %3d */
const char ladderRightClimbableFormat[16] = "\xb1" "\xa6" "\xbe" "\xba" "\xb9" "\xdf" "\xb2" "\xc4" "\xc7" "\xbd" "%3d";


/*
 * Tests angle against four .rodata bounds loaded in address order (0x69AF84,
 * 0x69AF88, 0x69AF8C, 0x69AF90). Each test passes an adjacent pair of
 * bounds. Returns the index of the first test that holds, or -1 when none.
 * The four bounds are -3pi/4, 3pi/4, pi/4 and -pi/4, so the result is one of
 * four 90-degree compass sectors (printed as "Compass = %1d" by On_Ladder).
 */
int uwamonoCompassSector(f32 angle)
{
    f32 sectorEdge0;
    f32 sectorEdge1;
    f32 sectorEdge2;
    f32 sectorEdge3;

    sectorEdge0 = -2.3561945f;
    sectorEdge1 = 2.3561945f;
    if (Check_Angle(angle, sectorEdge1, sectorEdge0) != 0) {
        return 0;
    }
    sectorEdge2 = 0.785398185f;
    if (Check_Angle(angle, sectorEdge2, sectorEdge1) != 0) {
        return 1;
    }
    sectorEdge3 = -0.785398185f;
    if (Check_Angle(angle, sectorEdge3, sectorEdge2) != 0) {
        return 2;
    }
    return (Check_Angle(angle, sectorEdge0, sectorEdge3) != 0) ? 3 : -1;
}

#define LADDER_PI 3.1415927f
#define LADDER_HALF_PI 1.5707964f
/* LadderStateWork.state */
enum {
    LADDER_STATE_NONE = 0,
    LADDER_STATE_ON = 4,
    LADDER_STATE_CLIMB_UP = 5,
    LADDER_STATE_CLIMB_DOWN = 6,
    LADDER_STATE_EXIT = 7,
    LADDER_STATE_TOP = 8
};
/* Fly_to_Ladder counts LADDER_APPROACH_FRAMES frames before it turns to the ladder; the foot exit
 * (LADDER_FOOT_EXIT_FRAMES) is set by Ready_Ladder. */
enum {
    LADDER_APPROACH_FRAMES = 7,
    LADDER_FOOT_EXIT_FRAMES = 0x20
};

/* Rung heights: element 63 is the rung the climb starts on; read at the current rung index. */
static float ladderStepHeights[128];
/* Where the exit moves the player to. */
static AlignedVector4 ladderExitPoint;

float sinf(float angle);
float cosf(float angle);

void Fly_to_Ladder(void)
{
    UwamonoRecord *object = &uwamonoRecords[uwamonoCurrentIndex];
    struct XenoActor *actor = object->body.actor;
    short stepIndex;
    int nextFrame;
    short state = uwamonoLadderState.state;
    short mirror = state;
    short exitSide;

    if (state == 2) {
        int nextFrame;

        xglFontDebugPrintf(8, 100, ladderModeText);
        nextFrame = (unsigned short)ladderFrame + 1;
        ladderFrame = nextFrame;
        if ((short)nextFrame < LADDER_APPROACH_FRAMES) {
            return;
        }
        ladderFrame = 0;
        actor->direction.y = ladderSlots[uwamonoLadderSlot].angle;
        exitSide = ladderExitSide;
        if (exitSide != 0) {
            float angle;

            int motion = 19;
            motion = (exitSide ^ state) ? motion : 18;
            uwamonoSetMotion(uwamonoCurrentIndex, motion, 1.0f);
            angle = ladderSlots[uwamonoLadderSlot].angle;
            actor->direction.y = angle;
            actor->rotation.y = angle;
            XenoActorSetSolverPasses(actor, 15);
            cameraApplyAllSlots(0);
        } else {
            uwamonoSetMotion(uwamonoCurrentIndex, 13, 1.0f);
            XenoActorSetSolverPasses(actor, 15);
        }
        uwamonoLadderState.state = 3;
    }

    Get_MiddlePoint(&object->moveFrom, &object->moveTo, ladderFrame, ladderExitFrames, &actor->position);
    nextFrame = (unsigned short)ladderFrame + 1;
    if (ladderFrame >= ladderExitFrames) {
        DoublewordAlignedVector4 landingPoint;

        landingPoint = actor->position;
        ladderFrame = -1;
        uwamonoLadderState.state = LADDER_STATE_ON;
        unduGetSlotHeight(uwamonoCurrentIndex, &landingPoint, 256, -1, 0);
        ladderRungIndex = 63;
        if (ladderExitSide == 0) {
            actor->position.y = ladderFootHeight;
            if (mirror != state) {
                actor->position.x = ladderFootHeight;
            }
        }
        ladderStepHeights[63] = actor->position.y;
        if (ladderExitSide == 0) {
            cameraApplyAllSlots(0);
        }
        uwamonoSetMotion(uwamonoCurrentIndex, 20, 1.0f);
        if (ladderExitSide == 0) {
            XenoActorSetSolverPasses(actor, 20);
        }
        XenoActorApplySourceValue(actor);
        actor->flags |= 1;
        for (stepIndex = 1; stepIndex < 63; stepIndex++) {
            float center = actor->position.y;

            ladderStepHeights[63 + stepIndex] = center + (float)stepIndex * 0.800000012f;
            ladderStepHeights[63 - stepIndex] = center - (float)stepIndex * 0.800000012f;
        }
    } else {
        ladderFrame = nextFrame;
    }
    XenoActorWrapRotation(actor);
}


/* The actor seen through its collision.flags halfword (+0x748, XenoActor.collision): hitFlags is read and written as a
 * bit-field of this view, whose position member is 16-aligned like XenoActor's. The bit-field store keeps alias set 0, so it
 * stays behind the loads before it; a plain collision.flags store is scheduled ahead of the moveTo.y load. */
typedef struct ActorLandingView {
    int unmodeled_000[4];
    AlignedVector4 position;                /* 0x010 */
    int unmodeled_020[(0x748 - 0x20) / 4];
    unsigned short hitFlags : 16;           /* 0x748 */
} ActorLandingView;

void Get_Off_Ladder(void)
{
    UwamonoRecord *object = &uwamonoRecords[uwamonoCurrentIndex];
    struct XenoActor *actor = object->body.actor;
    ActorLandingView *landing = (ActorLandingView *) actor;
    short *exitFrame = &ladderFrame;

    ladderFrame = ladderFrame + 1;
    Get_MiddlePoint_Parabora(&object->moveFrom, &object->moveTo, ladderFrame, ladderExitFrames,
                             ladderExitOffset, &actor->position);
    if (ladderFrame >= ladderExitFrames) {
        unsigned short oldHitFlags = landing->hitFlags;

        actor->position = ladderExitPoint;
        *exitFrame = -1;
        uwamonoLadderState.state = 0;
        actor->position.y = object->moveTo.y;
        landing->hitFlags = 0x1000 | oldHitFlags;
        XenoActorSetBlendFrames(actor, 4.0f);
        uwamonoSetMotion(uwamonoCurrentIndex, 0, 1.0f);
        XenoActorShiftJoints(actor, -1, 1, 0, 1);
        if (ladderExitSide != 0) {
            cameraApplyAllSlots(0);
        }
        gameWork.inputDelay = 2;
    }
    XenoActorWrapRotation(actor);
}


/*
 * Tries to start leaving the ladder slot at index from the player's position: from the foot when the
 * player is within 0.7 of it facing the ladder, or sideways from a rung when the player faces the
 * 60 to 120 degree sector on the matching side. Returns 1 and starts the exit when one applies.
 */
int Ready_Ladder(short index)
{
    LadderSlot *slot = &ladderSlots[index];
    UwamonoRecord *object = &uwamonoRecords[uwamonoCurrentIndex];
    struct XenoActor *actor = object->body.actor;
    int leaving = 0;
    int rungIndex;
    float footDistance;
    float leftDistance;
    float rightDistance;
    float xPlusSq, xPlusA, xPlusB, zPlusA, zPlusB, xMinusSq, xMinusA, xMinusB, zMinusA, zMinusB;

    /* The foot-exit block sits before the body in the original layout (return 0, then footExit, then body) and the foot test at
       the end of the distance code branches back to it (bc1tl footExit); if/else nesting puts it after the body, and a while
       loop with the exit duplicated into its body changes the code sizes, so the three jumps stay. */
    if (slot->key != -1) {
        goto body;
    }
    return 0;

footExit:
    object->moveFrom = actor->position;
    object->moveTo = slot->position;
    ladderFootHeight = object->moveTo.y + 0.4f;
    actor->direction.y = slot->angle;
    ladderExitFrames = LADDER_FOOT_EXIT_FRAMES;
    ladderExitSide = 0;
    goto finalize;

body:
    do {
    footDistance = Get_Distance(&actor->position, &slot->position);
    xPlusA = actor->position.x - (slot->position.x + sinf(slot->angle + 2.1816616f) * 0.87f);
    xPlusB = actor->position.x - (slot->position.x + sinf(slot->angle + 2.1816616f) * 0.87f);
    xPlusSq = xPlusA * xPlusB;
    zPlusA = actor->position.z - (slot->position.z + cosf(slot->angle + 2.1816616f) * 0.87f);
    zPlusB = actor->position.z - (slot->position.z + cosf(slot->angle + 2.1816616f) * 0.87f);
    leftDistance = __builtin_sqrtf(xPlusSq + zPlusA * zPlusB);
    xMinusA = actor->position.x - (slot->position.x + sinf(slot->angle - 2.1816616f) * 0.87f);
    xMinusB = actor->position.x - (slot->position.x + sinf(slot->angle - 2.1816616f) * 0.87f);
    xMinusSq = xMinusA * xMinusB;
    zMinusA = actor->position.z - (slot->position.z + cosf(slot->angle - 2.1816616f) * 0.87f);
    zMinusB = actor->position.z - (slot->position.z + cosf(slot->angle - 2.1816616f) * 0.87f);
    rightDistance = __builtin_sqrtf(xMinusSq + zMinusA * zMinusB);

    if (footDistance < 0.7f
        && Check_Angle(actor->rotation.y, slot->angle - 0.5235988f, slot->angle + 0.5235988f) == 1
        && slot->position.y - 0.05f <= actor->position.y
        && actor->position.y <= slot->position.y + 0.1f) {
        goto footExit;
    }

    for (rungIndex = 0; rungIndex < slot->rungCount; rungIndex++) {
        float rungY = slot->position.y + slot->rungs[rungIndex];

        if (actor->position.y < rungY - 0.3f || rungY + 0.1f < actor->position.y) {
            continue;
        }
        xglFontDebugPrintf(8, 138, ladderHeightPassFormat, rungIndex);
        if (leftDistance < 0.75f && (slot->rungExits[rungIndex] & RUNG_EXIT_LEFT)) {
            xglFontDebugPrintf(8, 138, ladderLeftClimbableFormat, rungIndex);
            if (Check_Angle(actor->direction.y, slot->angle - 2.0943952f, slot->angle - 1.0471976f) == 1) {
                object->moveTo.x = slot->position.x;
                object->moveTo.y = slot->position.y + slot->rungs[rungIndex];
                object->moveTo.z = slot->position.z;
                object->moveTo.w = 1.0f;
                leaving = 1;
                object->moveFrom = object->moveTo;
                object->moveFrom.y = actor->position.y;
                actor->direction.y = slot->angle - LADDER_HALF_PI;
                ladderExitFrames = 57;
                ladderExitSide = leaving;
                break;
            }
        }
        if (rightDistance < 0.75f && (slot->rungExits[rungIndex] & RUNG_EXIT_RIGHT)) {
            xglFontDebugPrintf(8, 138, ladderRightClimbableFormat, rungIndex);
            if (Check_Angle(actor->direction.y, slot->angle + 1.0471976f, slot->angle + 2.0943952f) == 1) {
                object->moveTo.x = slot->position.x;
                object->moveTo.y = slot->position.y + slot->rungs[rungIndex];
                object->moveTo.z = slot->position.z;
                object->moveTo.w = 1.0f;
                leaving = 1;
                object->moveFrom = object->moveTo;
                object->moveFrom.y = actor->position.y;
                actor->direction.y = slot->angle + LADDER_HALF_PI;
                ladderExitSide = 2;
                ladderExitFrames = 57;
                break;
            }
        }
    }
    if (leaving != 1) {
        return 0;
    }

finalize:
    uwamonoLadderState.state = 2;
    object->turnFrom.y = actor->direction.y;
    object->turnTo.y = slot->angle;
    uwamonoLadderSlot = index;
    ladderFrame = 0;
    XenoActorSetBlendFrames(actor, 4.0f);
    uwamonoSetMotion(uwamonoCurrentIndex, 0, 1.0f);
    Fly_to_Ladder();
    XenoActorWrapRotation(actor);
    return 1;
    } while (0);
}

void On_Ladder(void)
{
    int exited;
    LadderSlot *slot = &ladderSlots[uwamonoLadderSlot];
    UwamonoRecord *object = &uwamonoRecords[uwamonoCurrentIndex];
    struct XenoActor *actor = object->body.actor;
    int compass;
    float side;
    float climb;
    float stickX;
    float stickY;
    float x;
    float y;
    int i;
    AlignedVector4 exitPoint;
    AlignedVector4 target;

    exited = 0;
    compass = uwamonoCompassSector(slot->angle);
    side = 0.0f;
    climb = side; /* both start at zero; kept as a chained copy for the original code generation */
    if (!(gameWork.control & CONTROL_SCRIPTED)) {
        stickX = PadData[0].leftX;
        stickY = PadData[0].leftY;
        if (stickX >= 32.0f || stickX <= -32.0f) {
            x = stickX * 0.0004f * 1.5f;
        } else {
            x = 0.0f;
        }
        if (stickY >= 32.0f || stickY <= -32.0f) {
            y = stickY * 0.0004f * 1.5f;
        } else {
            y = 0.0f;
        }
        if (x == 0.0f && y == 0.0f) {
            if (PadData[0].held & PAD_UP) {
                y = -1.5f;
                x = 0.0f;
            }
            if (PadData[0].held & PAD_DOWN) {
                y = 1.5f;
                x = 0.0f;
            }
            if (PadData[0].held & PAD_LEFT) {
                x = -1.5f;
                y = 0.0f;
            }
            if (PadData[0].held & PAD_RIGHT) {
                x = 1.5f;
                y = 0.0f;
            }
        }
        switch (compass) {
        case 0:
            side = x;
            climb = -y;
            break;
        case 1:
            side = y;
            climb = x;
            break;
        case 2:
            side = x;
            climb = y;
            break;
        case 3:
            side = -y;
            climb = -x;
            break;
        }
    }
    xglFontDebugPrintf(4, 0x80, "XYZ = %4f,%4f", side, climb);
    xglFontDebugPrintf(4, 0x8A, "Compass = %1d", compass);
    switch (uwamonoLadderState.state) {
    case LADDER_STATE_CLIMB_UP:
        ladderClimbFrame++;
        if (ladderClimbFrame >= gameWork.climbFrames) {
            uwamonoLadderState.state = LADDER_STATE_ON;
            actor->flags |= 1;
            XenoActorApplySourceValue(actor);
        }
        climb = ladderClimbFrom + (ladderClimbTo - ladderClimbFrom) * ladderClimbFrame / gameWork.climbFrames - actor->position.y;
        target = actor->position;
        target.y += 5.0f;
        Actor_LookAt_Set(uwamonoCurrentIndex, 1, -1, &target);
        break;
    case LADDER_STATE_CLIMB_DOWN:
        ladderClimbFrame++;
        if (ladderClimbFrame >= gameWork.climbFrames) {
            uwamonoLadderState.state = LADDER_STATE_ON;
            actor->flags |= 1;
            XenoActorApplySourceValue(actor);
        }
        climb = ladderClimbFrom + (ladderClimbTo - ladderClimbFrom) * ladderClimbFrame / gameWork.climbFrames - actor->position.y;
        /* The original adjusts target here without setting it first (only the climb-up case copies the position); kept as is. */
        target.y -= 5.0f;
        Actor_LookAt_Set(uwamonoCurrentIndex, 1, -1, &target);
        break;
    case LADDER_STATE_ON:
        ladderClimbFrame = 0;
        if (climb < 0.0f) {
            if (actor->position.y - 0.8f <= slot->position.y && slot->footBlocked == 1) {
                return;
            }
            seVoiceStartDefault(0x200000D);
            ladderClimbFrame = 1;
            ladderClimbFrom = actor->position.y;
            ladderClimbTo = ladderStepHeights[ladderRungIndex-- - 1];
            uwamonoLadderState.state = LADDER_STATE_CLIMB_DOWN;
            uwamonoSetMotion(uwamonoCurrentIndex, 0x14, -2.0f);
            XenoActorSetPlayRange(actor, 0, 0x1D);
            actor->flags &= ~1;
            climb = ladderClimbFrom + (ladderClimbTo - ladderClimbFrom) * ladderClimbFrame / gameWork.climbFrames - actor->position.y;
        }
        if (climb > 0.0f) {
            if (slot->position.y + slot->height <= actor->position.y + 0.8f) {
                uwamonoSendEvent(4, ladderSlots[uwamonoLadderSlot].id, 0);
                if (gameWork.control & CONTROL_MAP_LOADED) {
                    uwamonoLadderState.state = LADDER_STATE_TOP;
                }
                return;
            }
            seVoiceStartDefault(0x200000D);
            ladderClimbFrame = 1;
            ladderClimbFrom = actor->position.y;
            ladderClimbTo = ladderStepHeights[ladderRungIndex++ + 1];
            uwamonoLadderState.state = LADDER_STATE_CLIMB_UP;
            uwamonoSetMotion(uwamonoCurrentIndex, 0x14, 2.0f);
            XenoActorSetPlayRange(actor, 0, 0x1D);
            actor->flags &= ~1;
            climb = ladderClimbFrom + (ladderClimbTo - ladderClimbFrom) * ladderClimbFrame / gameWork.climbFrames - actor->position.y;
        }
        break;
    }
    if (slot->position.y + slot->height < actor->position.y + climb) {
        actor->position.y = slot->position.y + slot->height;
        return;
    }
    if (actor->position.y + climb < slot->position.y + 0.4f && slot->footBlocked == 0) {
        exited = 1;
        uwamonoSendEvent(5, ladderSlots[uwamonoLadderSlot].id, 0);
        actor->position.y = slot->position.y;
        exitPoint = actor->position;
        exitPoint.x = actor->position.x + sinf(slot->angle - LADDER_PI) * 0.8f;
        exitPoint.y = actor->position.y;
        exitPoint.z = actor->position.z + cosf(slot->angle - LADDER_PI) * 0.8f;
        exitPoint.w = 1.0f;
        ladderExitPoint = exitPoint;
        ladderExitFrames = 0x20;
        ladderExitOffset = 0;
        ladderExitSide = 0;
        uwamonoSetMotion(uwamonoCurrentIndex, 0x11, 1.0f);
        actor->flags &= ~1;
        cameraApplyAllSlots(0);
    } else {
        actor->position.y = actor->position.y + climb;
        if (uwamonoLadderState.state == LADDER_STATE_ON) {
            for (i = 0; i < slot->rungCount; i++) {
                float rungY = slot->position.y + slot->rungs[i];

                if (actor->position.y < rungY - 0.3f || rungY + 0.45f < actor->position.y) {
                    continue;
                }
                if (side < 0.0f && (slot->rungExits[i] & RUNG_EXIT_LEFT)) {
                    exitPoint.x = actor->position.x;
                    exitPoint.y = actor->position.y;
                    exitPoint.z = actor->position.z;
                    exitPoint.w = 1.0f;
                    ladderExitPoint.x = actor->position.x + sinf(slot->angle + LADDER_HALF_PI) * 1.2f;
                    ladderExitPoint.y = actor->position.y;
                    ladderExitPoint.z = actor->position.z + cosf(slot->angle + LADDER_HALF_PI) * 1.2f;
                    ladderExitPoint.x = ladderExitPoint.x + sinf(slot->angle - LADDER_PI) * 0.5f;
                    ladderExitPoint.z = ladderExitPoint.z + cosf(slot->angle - LADDER_PI) * 0.5f;
                    ladderExitPoint.w = 1.0f;
                    ladderExitFrames = 0x2A;
                    ladderExitSide = 1;
                    exited = 1;
                    XenoActorSetBlendFrames(actor, 4.0f);
                    if (slot->position.y + slot->rungs[i] + 0.3f <= actor->position.y) {
                        uwamonoSetMotion(uwamonoCurrentIndex, 0x10, 1.0f);
                        ladderExitOffset = -0.13f;
                    } else {
                        uwamonoSetMotion(uwamonoCurrentIndex, 0x10, 1.0f);
                        ladderExitOffset = 0;
                    }
                    actor->flags &= ~1;
                    break;
                }
                if (side > 0.0f && (slot->rungExits[i] & RUNG_EXIT_RIGHT)) {
                    exitPoint.x = actor->position.x;
                    exitPoint.y = actor->position.y;
                    exitPoint.z = actor->position.z;
                    exitPoint.w = 1.0f;
                    ladderExitPoint.x = actor->position.x + sinf(slot->angle - LADDER_HALF_PI) * 1.2f;
                    ladderExitPoint.y = actor->position.y;
                    ladderExitPoint.z = actor->position.z + cosf(slot->angle - LADDER_HALF_PI) * 1.2f;
                    ladderExitPoint.x = ladderExitPoint.x + sinf(slot->angle - LADDER_PI) * 0.5f;
                    ladderExitPoint.z = ladderExitPoint.z + cosf(slot->angle - LADDER_PI) * 0.5f;
                    ladderExitPoint.w = 1.0f;
                    ladderExitFrames = 0x2A;
                    ladderExitSide = 1;
                    exited = 1;
                    XenoActorSetBlendFrames(actor, 4.0f);
                    if (slot->position.y + slot->rungs[i] + 0.3f <= actor->position.y) {
                        uwamonoSetMotion(uwamonoCurrentIndex, 0xF, 1.0f);
                        ladderExitOffset = -0.13f;
                    } else {
                        uwamonoSetMotion(uwamonoCurrentIndex, 0xF, 1.0f);
                        ladderExitOffset = 0;
                    }
                    actor->flags &= ~1;
                    break;
                }
            }
        }
        if (exited == 1) {
            exitPoint.y = unduGetSlotHeight(uwamonoCurrentIndex, &ladderExitPoint, 0x100, -1, 0);
        }
    }
    if (exited == 1) {
        object->moveFrom = actor->position;
        object->moveTo = exitPoint;
        ladderFrame = 0;
        uwamonoLadderState.state = LADDER_STATE_EXIT;
    }
    XenoActorWrapRotation(actor);
}

/* JP: 梯子モード
 * EN: Ladder mode */
const char ladderModeTitleText[16] = "\x0d" "\x02" "\xc4" "\xf4" "\xbb" "\xd2" "\xa5" "\xe2" "\xa1" "\xbc" "\xa5" "\xc9" "";

int Ladder_Main(void)
{
    struct XenoActor *actor = uwamonoRecords[uwamonoCurrentIndex].body.actor;
    int slotIndex;

    if (uwamonoLadderState.state == LADDER_STATE_NONE) {
        for (slotIndex = 0; slotIndex < LADDER_SLOT_COUNT; slotIndex++) {
            if (ladderSlots[slotIndex].id == -1) {
                continue;
            }
            if (Ready_Ladder(slotIndex) != 0) {
                Enemy_After_Battle(-1);
                break;
            }
        }
        if (uwamonoLadderState.state == LADDER_STATE_NONE) {
            return 0;
        }
    }
    xglFontDebugPrintf(8, 12, ladderModeTitleText);
    actor->rotation.y = Get_Cursol_by_Reduce_Speed_Angle_Loop(actor->rotation.y, actor->direction.y, 6.0f);
    switch (uwamonoLadderState.state) {
    case 1:
        break;
    case 2:
    case 3:
        Fly_to_Ladder();
        break;
    case LADDER_STATE_ON:
    case LADDER_STATE_CLIMB_UP:
    case LADDER_STATE_CLIMB_DOWN:
        On_Ladder();
        break;
    case LADDER_STATE_EXIT:
        Get_Off_Ladder();
        break;
    }
    return 1;
}

/* Clears every ladder slot to its unused state and selects slot 0. */
void ladderInitTable(void)
{
    int slotIndex;
    int rungIndex;
    LadderSlot *slot;

    uwamonoLadderState.state = 0;
    for (slotIndex = 0; slotIndex < LADDER_SLOT_COUNT; slotIndex++) {
        slot = &ladderSlots[slotIndex];
        slot->id = -1;
        slot->slotIndex = -1;
        slot->position.x = slot->position.y = slot->position.z = 0.0f;
        slot->position.w = 1.0f;
        slot->height = slot->angle = 0.0f;
        slot->rungCount = 0;
        slot->footBlocked = 0;
        for (rungIndex = 0; rungIndex < LADDER_MAX_RUNGS; rungIndex++) {
            slot->rungs[rungIndex] = 0.0f;
            slot->rungExits[rungIndex] = 0;
        }
    }
}

/* Returns the index of the first slot whose id is id, or -1 when no slot holds it. */
int ladderFindSlot(short id)
{
    int slotIndex;
    LadderSlot *slot;

    slot = ladderSlots;
    for (slotIndex = 0; slotIndex < LADDER_SLOT_COUNT; slotIndex++) {
        if (slot->id == id) {
            return slotIndex;
        }
        slot += 1;
    }
    return -1;
}

/* Returns how many slots, counted from slot 0, are in use before the first unused one. */
int ladderCountUsedPrefix(void)
{
    int usedCount;
    LadderSlot *slot;

    usedCount = 0;
    if (ladderSlots[0].id != -1) {
        slot = ladderSlots;
        for (;;) {
            usedCount += 1;
            slot += 1;
            if (usedCount < LADDER_SLOT_COUNT && slot->id != -1) {
                continue;
            }
            break;
        }
    }
    return usedCount;
}


/* Fills the slot of the ladder at the first unused position from the entry key and the foot data. */
int ladderSetLadder(SbThread *thread)
{
    LadderSlot *slot;
    float *angleHeight;
    char *slotBytes;
    int poseOffset;
    int usedCount;
    int ladderId;
    int entryKey;
    float height;

    if (VM_PLUGIN_CNT_GET(thread) != 3) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    usedCount = ladderCountUsedPrefix();
    ladderId = vmArgGet(thread, 1);
    entryKey = vmArgGet(thread, 2);
    height = (float) vmArgGet(thread, 3) / 100.0f;
    if (entryKey == -1) {
        ladderSlots[usedCount].id = 0;
        return 1;
    }
    /* The angle/height pair is addressed as a byte offset into the table: a field address gets folded differently and changes the code. */
    slotBytes = (char *)ladderSlots;
    slot = &ladderSlots[usedCount];
    poseOffset = usedCount * sizeof(LadderSlot) + 32;
    angleHeight = (float *)(slotBytes + poseOffset);
    slot->slotIndex = usedCount;
    slot->id = ladderId;
    angleHeight[1] = height;
    slot->key = entryKey;
    *angleHeight = uwamonoGetEntryPose(entryKey, &ladderSlots[usedCount].position.x);
    return 1;
}

/* Adds a rung to the ladder of the entry key: its height and the exits it allows. */
int ladderSetEntrance(SbThread *thread)
{
    int slotIndex;
    float rungHeight;
    int leftExit;
    int rightExit;
    int rungExitBits;
    int rungIndex;

    if (VM_PLUGIN_CNT_GET(thread) != 4) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    slotIndex = ladderFindSlot(vmArgGet(thread, 1));
    rungHeight = (float) vmArgGet(thread, 2) / 100.0f;
    leftExit = (vmArgGet(thread, 3) == 1) ? RUNG_EXIT_LEFT : 0;
    rightExit = (vmArgGet(thread, 4) == 1) ? RUNG_EXIT_RIGHT : 0;
    rungExitBits = leftExit | rightExit;
    rungIndex = ladderSlots[slotIndex].rungCount;
    ladderSlots[slotIndex].rungs[rungIndex] = rungHeight;
    ladderSlots[slotIndex].rungCount += 1;
    ladderSlots[slotIndex].rungExits[rungIndex] = rungExitBits;
    return 1;
}

extern SbNativeMethod ladderNatives[];
extern char ladderModuleName[];

void ladderRegisterNatives(void) {
    sbNativeOwnerAdd(ladderModuleName, ladderNatives);
}

char ladderModuleName[8] = "ladder";
LadderStateWork uwamonoLadderState = { 0 };
/* .data of this file (0x3E7020-0x3E7040): the ladder script natives. */

/* Native table of the ladder module. */
SbNativeMethod ladderNatives[4] = {
    { "setLadder", ladderSetLadder },
    { "setEntrance", ladderSetEntrance },
    { 0, 0 },
    { 0, 0 }
};
