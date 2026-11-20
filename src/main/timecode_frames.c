#include "common.h"
#include "main/debt_box.h"
#include "main/timecode_window.h"
#include "main/timecode_frames.h"
#include "main/timecode_counter.h"
#include "main/ease_step.h"
#include "main/xgl_packet_scratch.h"

/* Time units of the timecode object (the TIMECODE_* slot state, flags and field codes are
 * in main/timecode_counter.h). */
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24            /* the hours field wraps at 24 */
#define TIMECODE_DIGIT_BUFFER 50    /* size of the digit string of timecodeAddToDigit */

/* Prototypes of the callees whose headers do not declare them yet. */
long long timecodeAddToDigit(long long, int, int);
long long timecodeAddToField(long long, int, int);

/* While active: flags &= ~0x40. */
void timecodeDisableSelection(MsgSlot *slot) {
    if (slot->state == MSG_SLOT_STATE_TIMECODE) {
        slot->flags &= ~TIMECODE_FLAG_PHASE;
    }
}

/* While active: elapsed_frames = frames. */
void timecodeSetFrames(MsgSlot *slot, long long frames) {
    if (slot->state == MSG_SLOT_STATE_TIMECODE) {
        slot->u.timecode.elapsed_frames = frames;
    }
}

/* Returns elapsed_frames while active, else 0. */
long long timecodeGetFrames(MsgSlot *slot) {
    if (slot->state != MSG_SLOT_STATE_TIMECODE) {
        return 0;
    }
    return slot->u.timecode.elapsed_frames;
}

/* Splits elapsed_frames into hours, minutes, seconds and frames (30 per second); main 0x00285970. The frame remainder is scaled by HUNDREDTHS_PER_FRAME (hundredths of a second) into *frames. */
void timecodeSplit(MsgSlot *slot, int *hours, int *minutes, int *seconds, int *frames) {
    long long remaining = slot->u.timecode.elapsed_frames;
    int hh, mm, ss, ff;
    if (slot->state == MSG_SLOT_STATE_TIMECODE) {
        hh = remaining / FRAMES_PER_HOUR;
        remaining -= (long long)hh * FRAMES_PER_HOUR;
        mm = remaining / FRAMES_PER_MINUTE;
        remaining -= (long long)mm * FRAMES_PER_MINUTE;
        ss = remaining / FRAMES_PER_SECOND;
        remaining -= (long long)ss * FRAMES_PER_SECOND;
        ff = (long long)((float) remaining * HUNDREDTHS_PER_FRAME);
        *hours = hh; *minutes = mm; *seconds = ss; *frames = ff;
    }
}

/* While active: stores value in selected and previous. */
void timecodeSetPhase(MsgSlot *slot, int value) {
    if (slot->state == MSG_SLOT_STATE_TIMECODE) {
        slot->u.timecode.selected = value;
        slot->u.timecode.previous = value;
    }
}

/* Returns the selected digit or field while active, else 0. */
int timecodeGetPhase(MsgSlot *slot) {
    if (slot->state != MSG_SLOT_STATE_TIMECODE) {
        return 0;
    }
    return slot->u.timecode.selected;
}

/* Moves the selection by delta (wrapped into the range of the mode), remembers the old one in previous, sets flag 0x100 and restarts the cursor ease; main 0x00285B18. */
void timecodeStepPhase(MsgSlot *slot, int delta) {
    MsgSlotTimecode *step = &slot->u.timecode;
    int next;

    if (slot->state == MSG_SLOT_STATE_TIMECODE && !(slot->flags & TIMECODE_FLAG_RAMP)) {
        if (slot->flags & TIMECODE_FLAG_LEVEL) {
            return;
        }
        slot->flags |= TIMECODE_FLAG_LEVEL;
        next = step->selected + delta;
        step->previous = step->selected;
        step->selected = next;
        if (step->mode == TIMECODE_MODE_HMS) {
            step->selected = wrapIntRange(next, TIMECODE_PHASE_SECONDS, TIMECODE_PHASE_HOURS);
        } else if (step->mode == TIMECODE_MODE_MSF) {
            step->selected = wrapIntRange(next, TIMECODE_PHASE_FRAMES, TIMECODE_PHASE_MINUTES);
        } else {
            step->selected = wrapIntRange(next, 1, step->digits);
        }
        step->cursor_angle = 0;
        step->cursor_angle_step = easeAngleStep(step->previous * TIMECODE_PHASE_PIXELS, step->selected * TIMECODE_PHASE_PIXELS);
    }
}

/* Starts a timeline: flag 0x200 and roll angle setup, then by mode: then timecodeAddToDigit (mode 0) or timecodeAddToField (mode 1 or 2); main 0x00285BF8. */
void timecodeStart(MsgSlot *slot, int param) {
    MsgSlotTimecode *step = &slot->u.timecode;

    if (slot->state == MSG_SLOT_STATE_TIMECODE && !(slot->flags & TIMECODE_FLAG_RAMP)) {
        if (slot->flags & TIMECODE_FLAG_LEVEL) {
            return;
        }
        slot->flags |= TIMECODE_FLAG_RAMP;
        step->phase_start = step->selected;
        step->digit = decimalDigitAt(step->elapsed_frames, step->selected);
        step->roll_angle = 0;
        step->roll_angle_step = easeAngleStep(0, param * TIMECODE_PHASE_PIXELS);
        step->span = param;
        switch (step->mode) {
        case 0:
            step->elapsed_frames = timecodeAddToDigit(step->elapsed_frames, step->selected, param);
            break;
        case 1:
        case 2:
            step->elapsed_frames = timecodeAddToField(step->elapsed_frames, step->selected, param);
            break;
        }
    }
}

/*
 * Adds amountIn to the decimal digit of value at place pos (1 = ones), wrapping that digit through 0..9; main 0x00285CD0.
 * The wrap bounds are the lower digits of value with the digit set to 9 (maxValue) and to 0 (minValue).
 */
long long timecodeAddToDigit(long long value, int pos, int amountIn) {
    long long amount = amountIn;
    char nines[TIMECODE_DIGIT_BUFFER], digits[TIMECODE_DIGIT_BUFFER], zeros[TIMECODE_DIGIT_BUFFER];
    long long maxValue;
    long long minValue;
    int last = pos - 1;
    int i;

    numberToDigitsLsbFirst(digits, value, last);
    __builtin_memcpy(nines, digits, sizeof(nines));
    nines[last] = '9';
    __builtin_memcpy(zeros, digits, sizeof(zeros));
    zeros[last] = '0';
    digitsLsbFirstToNumber(&maxValue, nines);
    digitsLsbFirstToNumber(&minValue, zeros);
    for (i = 0; i < last; i++) {
        amount *= 10;
    }
    value += amount;
    if (maxValue < value || value < minValue) {
        value -= amount * 10;
    }
    return value;
}

/* Applies amount to one field of a frame count split into hours, minutes, seconds and frames (30 per second),
 * wrapping inside the field; field code 10 = frames, 11 = seconds, 12 = minutes, 13 = hours. Returns the new frame count, or 0 for another code. */
long long timecodeAddToField(long long frames, int code, int amount) {
    int fields[4];
    int *field;
    int limit;
    int total = frames;
    fields[3] = total / FRAMES_PER_HOUR;
    total -= (int)((long long)fields[3] * FRAMES_PER_HOUR);
    fields[2] = total / FRAMES_PER_MINUTE;
    total -= (int)((long long)fields[2] * FRAMES_PER_MINUTE);
    fields[1] = total / FRAMES_PER_SECOND;
    total -= (int)((long long)fields[1] * FRAMES_PER_SECOND);
    fields[0] = total;
    switch (code) {
    case TIMECODE_PHASE_FRAMES: field = &fields[0]; limit = FRAMES_PER_SECOND; break;
    case TIMECODE_PHASE_SECONDS: field = &fields[1]; limit = SECONDS_PER_MINUTE; break;
    case TIMECODE_PHASE_MINUTES: field = &fields[2]; limit = MINUTES_PER_HOUR; break;
    case TIMECODE_PHASE_HOURS: field = &fields[3]; limit = HOURS_PER_DAY; break;
    default: return 0;
    }
    *field += amount;
    if (*field >= limit) {
        *field -= limit;
    }
    if (*field < 0) {
        *field += limit;
    }
    return (long long)fields[3] * FRAMES_PER_HOUR + (long long)fields[2] * FRAMES_PER_MINUTE + (long long)fields[1] * FRAMES_PER_SECOND + fields[0];
}

/* While active: elapsed_frames = hours * 108000 + minutes * 1800 + seconds * 30, then timecodeSetFrames. */
void timecodeSetHms(MsgSlot *slot, int hours, int minutes, int seconds) {
    long long frames = (long long)hours * FRAMES_PER_HOUR + (long long)minutes * FRAMES_PER_MINUTE + (long long)seconds * FRAMES_PER_SECOND;

    if (slot->state == MSG_SLOT_STATE_TIMECODE) {
        timecodeSetFrames(slot, frames);
    }
}
