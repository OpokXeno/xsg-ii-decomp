#include "common.h"
#include "debug_break.h"
#include "main/elevator.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/ytachi_test.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/undulate.h"
#include "main/res_get_path.h"
#include "main/xgl_2.h"

#define CAMERA_FLAG_TRACK_PLAYER 0x20 /* src/main/game_camera.c */

/* One elevator: position moves from start to target over duration frames. */
struct ElevatorRecord {
    float position[3];
    unsigned char unmodeled_0C[0x10 - 0x0C];
    float start[3];                         /* +0x10 */
    unsigned char unmodeled_1C[0x20 - 0x1C];
    float target[3];                        /* +0x20 */
    unsigned char unmodeled_2C[0x30 - 0x2C];
    float reference[3];                     /* +0x30 */
    unsigned char axis_moves[3];            /* +0x3C, set when elvMove gave the axis */
    unsigned char unmodeled_3F[0x44 - 0x3F];
    float elapsed;                          /* +0x44 */
    float duration;                         /* +0x48 */
    float progress;                         /* +0x4C */
};

extern char elvModuleName[];
extern SbNativeMethod elvNativeTable[];
extern unsigned char elvActiveMask;
extern unsigned char elvNoOutMask;
extern int elvKeepOffFlags[];
extern int printf(const char *format, ...);
extern const char elvAssertFormat[];
/* the assert format: expression, file, line */
extern const char elvNoRangeAssertText[];
/* "(no > 0) && (no < 9)" */
extern const char elvSourceFile[];
/* "../elevator.euc.c" */
extern const char elvKeepOffNoRangeAssertText[];

/* "(no > 0) && (no < UNDUFLAGKEEPOFF_MAX)" */
#define UNDUFLAGKEEPOFF_MAX 32
/* The original assert: prints the expression, file and line, then traps.
 * A bare if, not do { } while (0): its loop notes change the schedule. */
#define ELV_ASSERT(condition, expression, line)                     \
    if (!(condition)) {                                             \
        printf(elvAssertFormat, expression, elvSourceFile, line);           \
        DEBUG_BREAK();                                              \
    }

/* Returns the unit voice cue id 0x50; not an elevator value. */
int unitPlayVoiceCueId(void) {
    return 0x50;
}

/* Returns the unit voice cue id 0x73; not an elevator value. */
int unitActionVoiceCueId(void) {
    return 0x73;
}

/* Sets CAMERA_FLAG_TRACK_PLAYER in the camera flags word when the current actor
 * reports status 0x2nn with nn equal to this elevator's index (zero based), that is
 * while the player stands on this elevator. */
void elvSetStatusFlag(int elevator_index)
{
    int status;

    status = uwamonoRecords[uwamonoCurrentIndex].body.actor->collision.kind;
    if ((status & ~0xFF) == 0x200 && (status & 0xF) == elevator_index) {
        gameWork.camera.flags |= CAMERA_FLAG_TRACK_PLAYER;
    }
}

/* Per-frame motion of one elevator: interpolates the record position toward
 * its target by elapsed/duration (VU0 lerp), and calls elvSetStatusFlag at the
 * end. Called by DispMapObject with the object that holds the elevator index. */
/* The part of an elevator holder this function reads and writes back. */
/* The elevator records, 636 bytes: seven 80-byte records, then the first 76 bytes of
 * record 8. Its progress word would be ladderRungHeights[0] of get_ladder.c (0x6A1B2C), so the
 * storage cannot be eight whole records; the code indexes it with 80-byte records. */
static struct ElevatorRecord elvRecords[8];

void elvUpdateMotion(UwamonoRecord *holder)
{
    struct ElevatorRecord *record;
    int progress_bits;
    float value;

    if (gameWork.control & 1) {
        return;
    }
    record = &elvRecords[holder->elevatorMotion - 1];
    if (record->axis_moves[0] == 0) {
        value = holder->position.x;
        record->position[0] = value;
        record->target[0] = value;
        record->start[0] = value;
    }
    if (record->axis_moves[1] == 0) {
        value = holder->position.y;
        record->position[1] = value;
        record->target[1] = value;
        record->start[1] = value;
    }
    if (record->axis_moves[2] == 0) {
        value = holder->position.z;
        record->position[2] = value;
        record->target[2] = value;
        record->start[2] = value;
    }
    record->reference[0] = record->position[0];
    record->reference[1] = record->position[1];
    record->reference[2] = record->position[2];
    if (record->elapsed < record->duration) {
        record->elapsed = record->elapsed + 1.0f;
        if (0.0f < record->duration) {
            record->progress = I2F((int)record->elapsed) / I2F((int)record->duration);
        } else {
            record->progress = 1.0f;
        }
        __asm__ __volatile__(
            "mfc1 %0, %1\n\t"
            "lqc2 $vf2, 0(%3)\n\t"
            "lqc2 $vf1, 0(%2)\n\t"
            "qmtc2 %0, $vf3\n\t"
            "vsub.xyz $vf2xyz, $vf2xyz, $vf1xyz\n\t"
            "vmulx.xyz $vf2xyz, $vf2xyz, $vf3x\n\t"
            "vadd.xyz $vf3xyz, $vf2xyz, $vf1xyz\n\t"
            "sqc2 $vf3, 0(%4)"
            : "=&r"(progress_bits)
            : "f"(record->progress), "r"(record->start), "r"(record->target), "r"(record->position)
            : "memory");
    }
    if (record->duration <= record->elapsed) {
        if (record->duration != 0.0f) {
            elvSetStatusFlag(holder->elevatorMotion - 1);
        }
        record->elapsed = 0.0f;
        record->duration = 0.0f;
        record->progress = 1.0f;
        record->start[0] = record->position[0];
        record->start[1] = record->position[1];
        record->start[2] = record->position[2];
    }
    holder->position.x = record->position[0];
    holder->position.y = record->position[1];
    holder->position.z = record->position[2];
}

/* Copies elevator position_out[] and, when asked, the offset of the position
 * from the reference point to delta_out[]. Either pointer may be null. */
void elvGetPosition(int elevator_index, Vector4 *position_out, Vector4 *delta_out)
{
    struct ElevatorRecord *record;

    record = &elvRecords[elevator_index];
    if (position_out != 0) {
        position_out->x = record->position[0];
        position_out->y = record->position[1];
        position_out->z = record->position[2];
    }
    if (delta_out != 0) {
        delta_out->x = record->position[0] - record->reference[0];
        delta_out->y = record->position[1] - record->reference[1];
        delta_out->z = record->position[2] - record->reference[2];
    }
}

/* Converts a script value to a float with the scale elvValueScale. */
float elvValueToFloat(int value)
{
    return I2F(value) * 0.01f;
}

/* Script native id.elvActive: sets or clears bit (no - 1) of elvActiveMask
 * from the second argument. Asserts 0 < no < 9. The variable no is reused
 * for the bit value: one register serves both roles in the original. */
int elvSetActive(SbThread *thread)
{
    int no;
    int active;
    int mask;

    no = vmArgGet(thread, 1);
    active = vmArgGet(thread, 2);
    ELV_ASSERT((no > 0) && (no < 9), elvNoRangeAssertText, 161);
    no = 1 << (no - 1);
    mask = elvActiveMask & ~no;
    if (active) {
        mask |= no;
    }
    elvActiveMask = mask;
    return 1;
}

/* Script native id.elvNoOut: the same bit operation on elvNoOutMask. */
int elvSetNoOut(SbThread *thread)
{
    int no;
    int no_out;
    int mask;

    no = vmArgGet(thread, 1);
    no_out = vmArgGet(thread, 2);
    ELV_ASSERT((no > 0) && (no < 9), elvNoRangeAssertText, 186);
    no = 1 << (no - 1);
    mask = elvNoOutMask & ~no;
    if (no_out) {
        mask |= no;
    }
    elvNoOutMask = mask;
    return 1;
}

/* Script native id.elvMove: elevator no moves to the given x, y, z (an axis
 * left out is marked off in axis_moves and gets 0) over the given number of frames,
 * or jumps there at once when the frame count is not positive. */
int elvMove(SbThread *thread)
{
    struct ElevatorRecord *record;
    int no;
    int frames;
    float x;
    float y;
    float z;

    no = vmArgGet(thread, 1);
    ELV_ASSERT((no > 0) && (no < 9), elvNoRangeAssertText, 214);
    record = &elvRecords[no - 1];
    if (vmArgTagIsZero(thread, 2) != 0) {
        x = 0.0f;
        record->axis_moves[0] = 0;
    } else {
        record->axis_moves[0] = 1;
        x = elvValueToFloat(vmArgGet(thread, 2));
    }
    if (vmArgTagIsZero(thread, 3) != 0) {
        y = 0.0f;
        record->axis_moves[1] = 0;
    } else {
        record->axis_moves[1] = 1;
        y = elvValueToFloat(vmArgGet(thread, 3));
    }
    if (vmArgTagIsZero(thread, 4) != 0) {
        z = 0.0f;
        record->axis_moves[2] = 0;
    } else {
        record->axis_moves[2] = 1;
        z = elvValueToFloat(vmArgGet(thread, 4));
    }
    frames = vmArgGet(thread, 5);
    if (frames > 0) {
        record->target[0] = x;
        record->target[1] = y;
        record->target[2] = z;
        record->elapsed = 0.0f;
        record->duration = frames;
        record->progress = 0.0f;
    } else {
        record->position[0] = x;
        record->position[1] = y;
        record->position[2] = z;
        record->duration = 0.0f;
        elvSetStatusFlag(no - 1);
    }
    return 1;
}

/* Script native id.slopeSet: gameWork.slopes[no - 1] = {-sin, -cos} of the
 * angle (second argument times elvAngleScale), then the third and fourth
 * arguments as push and speed. */
int elvSlopeSet(SbThread *thread)
{
    GameWorkSlope *slope;
    int no;
    float angle;
    float push;
    float speed;

    no = vmArgGet(thread, 1);
    ELV_ASSERT((no > 0) && (no < 9), elvNoRangeAssertText, 290);
    slope = &gameWork.slopes[no - 1];
    angle = I2F(vmArgGet(thread, 2)) * 0.000174532935f;
    push = elvValueToFloat(vmArgGet(thread, 3));
    speed = elvValueToFloat(vmArgGet(thread, 4));
    slope->x = -xglSin(angle);
    slope->z = -xglCos(angle);
    slope->push = push;
    slope->speed = speed;
    return 1;
}

/* Script native id.keepoff: elvKeepOffFlags[no] = 0 when the second argument
 * is zero, else UnduDataGetHeader(0xB00, no) with the argument's low bit. */
int elvSetKeepOff(SbThread *thread)
{
    int no;
    int keep;

    no = vmArgGet(thread, 1);
    keep = vmArgGet(thread, 2);
    ELV_ASSERT((no > 0) && (no < UNDUFLAGKEEPOFF_MAX), elvKeepOffNoRangeAssertText, 323);
    if (keep == 0) {
        elvKeepOffFlags[no] = 0;
    } else {
        elvKeepOffFlags[no] = (int) UnduDataGetHeader(0xB00, no) | (keep & 1);
    }
    return 1;
}


int preren(SbThread *thread)
{
    int offsetY;
    int offsetX;

    offsetY = vmArgGet(thread, 1);
    offsetX = vmArgGet(thread, 2);
    if (gameWork.active != 0) {
        gameWork.active->offsetY = (short) offsetY;
        gameWork.active->offsetX = (short) offsetX;
    }
    return 1;
}

int preload(SbThread *thread)
{
    ResJumpPoint sample;
    int index;
    int cfId;

    if (vmArgTagIsZero(thread, 1) == 0) {
        index = vmArgGet(thread, 1);
    } else {
        index = -1;
    }
    if (vmArgTagIsZero(thread, 2) == 0) {
        cfId = vmArgGet(thread, 2);
    } else {
        cfId = -1;
    }
    sample.value[0] = elvValueToFloat(vmArgGet(thread, 3));
    sample.value[1] = elvValueToFloat(vmArgGet(thread, 4));
    sample.value[2] = elvValueToFloat(vmArgGet(thread, 5));
    sample.value[3] = elvValueToFloat(vmArgGet(thread, 6));
    resJumpRecordSet(index, cfId, &sample);
    return 1;
}

/* Registers the elevator script module "id" (elvActive, elvMove, elvNoOut, slopeSet,
 * keepoff, preren, preload), then clears the move duration of the eight elevator records. */
void elvReset(void)
{
    struct ElevatorRecord *records;
    float *duration;
    int i;

    sbNativeOwnerAdd(elvModuleName, elvNativeTable);
    records = elvRecords;
    duration = &records[7].duration;
    for (i = 0; i < 8; i++) {
        *duration = 0.0f;
        duration -= sizeof(struct ElevatorRecord) / sizeof(float);
    }
}

/* Data of the original .rodata, .data and .sdata sections, in address order. */
const char elvAssertFormat[96] = "\n\n************************\n\nAssert(%s)\n    file = %s  line = %d\n\n************************\n\n\n";
const char elvNoRangeAssertText[24] = "(no > 0) && (no < 9)";
const char elvSourceFile[24] = "../elevator.euc.c";
const char elvKeepOffNoRangeAssertText[40] = "(no > 0) && (no < UNDUFLAGKEEPOFF_MAX)";
const char elvNativeNameSlopeSet[16] = "slopeSet";
const char elvNativeNameElvNoOut[16] = "elvNoOut";
const char elvNativeNameElvActive[16] = "elvActive";
/* The short names of elvNativeTable (.sdata 0x69D1A0-0x69D1C0, address order). */
const char elevatorNativeNamePreload[8] = "preload";
const char elevatorNativeNamePreren[8] = "preren";
const char elevatorNativeNameKeepOff[8] = "keepoff";
const char elevatorNativeNameElvMove[8] = "elvMove";
SbNativeMethod elvNativeTable[9] = {
    { elvNativeNameElvActive, elvSetActive },
    { elevatorNativeNameElvMove, elvMove },
    { elvNativeNameElvNoOut, elvSetNoOut },
    { elvNativeNameSlopeSet, elvSlopeSet },
    { elevatorNativeNameKeepOff, elvSetKeepOff },
    { elevatorNativeNamePreren, preren },
    { elevatorNativeNamePreload, preload },
    { 0, 0 },
};
char elvModuleName[8] = "id";
