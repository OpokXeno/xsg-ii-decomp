#include "common.h"
#include "main/talk_script.h"
#include "main/msg_text_draw.h"
#include "main/timecode_window.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/msg_slot_core.h"
#include "main/msg_slot_center.h"

#include "shared.h"

/* Initialises the slot (slot type 3), selects digit 1 (selected and previous), places the cursor and sets the idle colour. */
void timecodeInit(MsgSlot *slot)
{
    MsgSlotTimecode *stepper = &slot->u.timecode;

    msgSlotInit(slot, MSG_SLOT_TYPE_TIMECODE);
    stepper->selected = 1;
    stepper->previous = 1;
    stepper->cursor_x = timecodeCursorX(slot);
    stepper->idle_color = TIMECODE_COLOR_INIT;
}

/* Runs msgSlotSetGeometry with the four values while the object is active (slot type 3). */
void timecodeSetGeometry(MsgSlot *slot, int first_value, int second_value, int third_value, int fourth_value)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        msgSlotSetGeometry(slot, first_value, second_value, third_value, fourth_value);
    }
}

/* While active: flags = (flags & ~closing) | opening (TIMECODE_FLAG_*). */
void timecodeStartOpening(MsgSlot *slot)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        slot->flags = (slot->flags & ~(u32) MSG_SLOT_CLOSING) | MSG_SLOT_OPENING;
    }
}


/*
 * Per-frame update, main 0x00285160. Does nothing unless the object is active and gameWork.control has CONTROL_PAUSED clear.
 * The int return type (never set) keeps the final timecodeStepCounter call from becoming a tail jump.
 */
int timecodeUpdate(MsgSlot *slot)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE && !(gameWork.control & CONTROL_PAUSED)) {
        if (slot->flags & MSG_SLOT_OPENING) {
            slot->open_counter += 1;
            if (slot->open_counter >= TIMECODE_OPEN_FRAMES) {
                slot->flags = (slot->flags & ~(u32) MSG_SLOT_OPENING) | MSG_SLOT_OPEN;
                switch (slot->u.timecode.editable) {
                case 1:
                    break;
                case 2:
                    slot->flags |= TIMECODE_FLAG_SELECT;
                    break;
                }
            }
        }
        if (slot->flags & MSG_SLOT_CLOSING) {
            slot->open_counter -= 1;
            if (slot->open_counter <= 0) {
                slot->flags = 0;
            }
        }
        if (slot->flags & TIMECODE_FLAG_ADVANCE) {
            timecodeAdvance(slot);
            if (slot->u.timecode.increment > 0) {
                if (slot->u.timecode.limit < slot->u.timecode.elapsed_frames) {
                    slot->flags &= ~(u32) TIMECODE_FLAG_ADVANCE;
                    slot->u.timecode.elapsed_frames = slot->u.timecode.limit;
                }
            } else if (slot->u.timecode.elapsed_frames < slot->u.timecode.limit) {
                slot->flags &= ~(u32) TIMECODE_FLAG_ADVANCE;
                slot->u.timecode.elapsed_frames = slot->u.timecode.limit;
            }
        }
        if (slot->flags & TIMECODE_FLAG_SELECT) {
            timecodeUpdateSelection(slot);
        }
        if (slot->flags & TIMECODE_FLAG_DIGIT_ROLL) {
            if (timecodeStepRollAngle(slot)) {
                slot->flags &= ~(u32) TIMECODE_FLAG_DIGIT_ROLL;
            }
        }
        if (slot->flags & TIMECODE_FLAG_CURSOR_MOVE) {
            if (timecodeStepCursorAngle(slot)) {
                slot->flags &= ~(u32) TIMECODE_FLAG_CURSOR_MOVE;
            }
        }
        if (slot->flags & TIMECODE_FLAG_BLINK) {
            timecodeStepCounter(slot);
        }
    }
}

/* Unconditional: elapsed_frames += increment. */
void timecodeAdvance(MsgSlot *slot)
{
    slot->u.timecode.elapsed_frames += slot->u.timecode.increment;
}

/* Forwards to timecodeValidateSelection, which keeps the selected digit or field valid for the mode. */
void timecodeUpdateSelection(MsgSlot *slot)
{
    timecodeValidateSelection(slot);
}

/* Advances the roll angle of the rolling digit by roll_angle_step, clamped at 180; returns 1 at the clamp or when the roll flag is clear. */
int timecodeStepRollAngle(MsgSlot *slot)
{
    if (!(slot->flags & TIMECODE_FLAG_DIGIT_ROLL)) {
        return 1;
    }
    slot->u.timecode.roll_angle += slot->u.timecode.roll_angle_step;
    if (slot->u.timecode.roll_angle >= TIMECODE_ANGLE_END + 1) {
        slot->u.timecode.roll_angle = TIMECODE_ANGLE_END;
        return 1;
    }
    return 0;
}

/*
 * Advances the cursor ease angle by cursor_angle_step, clamps it at 180 and returns 1 when it
 * went past 180. Also stores the new cursor x (timecodeCursorX) in cursor_x.
 */
int timecodeStepCursorAngle(MsgSlot *slot)
{
    MsgSlotTimecode *stepper = &slot->u.timecode;
    int clamped = 0;

    stepper->cursor_angle += stepper->cursor_angle_step;
    if (stepper->cursor_angle >= TIMECODE_ANGLE_END + 1) {
        clamped = 1;
        stepper->cursor_angle = TIMECODE_ANGLE_END;
    }
    stepper->cursor_x = timecodeCursorX(slot);
    return clamped;
}

/* Keeps the selection valid for the mode and digit count: resets selected and previous to digit 1 (mode 0) or to the leftmost field (modes 1, 2). */
void timecodeValidateSelection(MsgSlot *slot)
{
    int *selected = &slot->u.timecode.selected;
    int *previous = &slot->u.timecode.previous;

    switch (slot->u.timecode.mode) {
    case TIMECODE_MODE_NUMBER:
        if (*selected <= 0 || slot->u.timecode.digits + 1 < *selected) {
            *selected = 1;
            *previous = 1;
        }
        break;
    case TIMECODE_MODE_HMS:
        if ((unsigned)(*selected - TIMECODE_PHASE_SECONDS) < 3) {
            return;
        }
        *selected = TIMECODE_PHASE_HOURS;
        *previous = TIMECODE_PHASE_HOURS;
        break;
    case TIMECODE_MODE_MSF:
        if ((unsigned)(*selected - TIMECODE_PHASE_FRAMES) < 3) {
            return;
        }
        *selected = TIMECODE_PHASE_MINUTES;
        *previous = TIMECODE_PHASE_MINUTES;
        break;
    }
}

/* Advances the blink phase of the entry colour fade, wrapping to 0 at 20. */
void timecodeStepCounter(MsgSlot *slot)
{
    MsgSlotTimecode *stepper = &slot->u.timecode;

    stepper->blink_phase += 1;
    if (stepper->blink_phase >= TIMECODE_COUNTER_WRAP) {
        stepper->blink_phase = 0;
    }
}

/* X of the cursor: eased between the previous and the selected digit, the centre of the range from timecodeCursorEnds offset by the half-width times cos(cursor_angle). */
extern float cosf(float angle);

int timecodeCursorX(MsgSlot *slot)
{
    MsgSlotTimecode *stepper = &slot->u.timecode;
    int low;
    int high;
    int centre;
    float amplitude;
    float angle;

    timecodeCursorEnds(slot, &low, &high);
    angle = stepper->cursor_angle * 0.01745328f;
    amplitude = -((high - low) >> 1);
    centre = (high + low) >> 1;
    return centre + (int)(amplitude * cosf(angle));
}

/*
 * Cursor x range: the pixel positions of the previous (low) and selected (high) digit. Mode 0 scales their distance
 * from the digit count by TIMECODE_PHASE_PIXELS; modes 1 and 2 use the kTimecodeFieldLayout row for the mode, scaled by TIMECODE_FIELD_PIXELS.
 */
/* Field layout of the two time modes (row 0 hours/minutes/seconds, row 1 minutes/seconds/frames): { field count base, first field code }. */
extern int kTimecodeFieldLayout[2][2];

void timecodeCursorEnds(MsgSlot *slot, int *low, int *high)
{
    MsgSlotTimecode *stepper = &slot->u.timecode;

    switch (stepper->mode) {
    case TIMECODE_MODE_NUMBER:
        *low = (stepper->digits - stepper->previous) * TIMECODE_PHASE_PIXELS;
        *high = (stepper->digits - stepper->selected) * TIMECODE_PHASE_PIXELS;
        break;
    case TIMECODE_MODE_HMS:
        *low = (kTimecodeFieldLayout[0][0] - (stepper->previous - kTimecodeFieldLayout[0][1])) * TIMECODE_FIELD_PIXELS;
        *high = (kTimecodeFieldLayout[0][0] - (stepper->selected - kTimecodeFieldLayout[0][1])) * TIMECODE_FIELD_PIXELS;
        break;
    case TIMECODE_MODE_MSF:
        *low = (kTimecodeFieldLayout[1][0] - (stepper->previous - kTimecodeFieldLayout[1][1])) * TIMECODE_FIELD_PIXELS;
        *high = (kTimecodeFieldLayout[1][0] - (stepper->selected - kTimecodeFieldLayout[1][1])) * TIMECODE_FIELD_PIXELS;
        break;
    }
}

/*
 * Flag-driven dispatch while active: flags 0x4, 0x2 and 0x8 each run talkDrawFramesWithBox (mode 0) or
 * talkDrawFrameRects; flag 0x8 also draws the contents.
 * The result is never used; int keeps the final call from becoming a tail jump.
 */
int timecodeSlotDraw(MsgSlot *slot)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        if (slot->flags & MSG_SLOT_OPENING) {
            if (slot->u.timecode.mode == TIMECODE_MODE_NUMBER) { talkDrawFramesWithBox(slot); } else { talkDrawFrameRects(slot); }
        }
        if (slot->flags & MSG_SLOT_CLOSING) {
            if (slot->u.timecode.mode == TIMECODE_MODE_NUMBER) { talkDrawFramesWithBox(slot); } else { talkDrawFrameRects(slot); }
        }
        if (slot->flags & MSG_SLOT_OPEN) {
            if (slot->u.timecode.mode == TIMECODE_MODE_NUMBER) { talkDrawFramesWithBox(slot); } else { talkDrawFrameRects(slot); }
            timecodeDrawContents(slot);
        }
    }
}

/* While active: clears flag bits 0x4, 0x8, 0x40, 0x80 and 0x800, then sets 0x2. */
void timecodeBeginClose(MsgSlot *slot)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        slot->flags &= ~MSG_SLOT_OPENING;
        slot->flags &= ~MSG_SLOT_OPEN;
        slot->flags &= ~TIMECODE_FLAG_SELECT;
        slot->flags &= ~TIMECODE_FLAG_ADVANCE;
        slot->flags &= ~TIMECODE_FLAG_BLINK;
        slot->flags |= MSG_SLOT_CLOSING;
    }
}

/* While active: stores value in editable. */
void timecodeSetEditable(MsgSlot *slot, int value)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        slot->u.timecode.editable = value;
    }
}

/*
 * While active: sets the mode, then recomputes the box width (timecodeFitBoxWidth), the
 * selection (timecodeValidateSelection) and the cursor x (timecodeCursorX).
 */
void timecodeSetMode(MsgSlot *slot, int mode)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        slot->u.timecode.mode = mode;
        timecodeFitBoxWidth(slot);
        timecodeValidateSelection(slot);
        slot->u.timecode.cursor_x = timecodeCursorX(slot);
    }
}

/* While active and digits in 1 to 9: stores the digit count, recomputes as timecodeSetMode does and stores cursor_x. */
void timecodeSetDigits(MsgSlot *slot, int digits)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE && (unsigned)(digits - 1) < TIMECODE_INDEX_MAX) {
        slot->u.timecode.digits = digits;
        timecodeFitBoxWidth(slot);
        timecodeCenterSlot(slot);
        timecodeValidateSelection(slot);
        slot->u.timecode.cursor_x = timecodeCursorX(slot);
    }
}

/* Box width: TIMECODE_WIDTH_TIME for modes 1 and 2; for mode 0, TIMECODE_WIDTH_BASE + TIMECODE_PHASE_PIXELS per digit after the first, at least TIMECODE_WIDTH_MIN. */
void timecodeFitBoxWidth(MsgSlot *slot)
{
    int digits;

    switch (slot->u.timecode.mode) {
    case TIMECODE_MODE_NUMBER:
        digits = slot->u.timecode.digits - 1;
        slot->extent_x = digits * TIMECODE_PHASE_PIXELS + TIMECODE_WIDTH_BASE;
        if (slot->extent_x < TIMECODE_WIDTH_MIN) {
            slot->extent_x = TIMECODE_WIDTH_MIN;
        }
        break;
    case TIMECODE_MODE_HMS:
    case TIMECODE_MODE_MSF:
        slot->extent_x = TIMECODE_WIDTH_TIME;
        break;
    }
}

/* Stores the results of msgSlotCenterX and msgSlotCenterY. */
void timecodeCenterSlot(MsgSlot *slot)
{
    /* Inlining the arithmetic of the two helpers would drop the calls and change the rows. */
    slot->x = msgSlotCenterX(slot);
    slot->y = msgSlotCenterY(slot);
}

/* While active: flags |= 0x40. The opposite of timecodeDisableSelection in timecode_frames.c. */
void timecodeEnableSelection(MsgSlot *slot)
{
    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        slot->flags |= TIMECODE_FLAG_SELECT;
    }
}

int kTimecodeFieldLayout[2][2] = { { 2, 11 }, { 2, 10 } };
