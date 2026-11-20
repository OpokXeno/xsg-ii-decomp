#include "common.h"
#include "main/xgl_packet_scratch.h"
#include "main/msg_window_run.h"
#include "main/talk_script.h"
#include "main/talk_scan.h"
#include "main/talk_text_box.h"
#include "main/debt_box.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/timecode_counter.h"
#include "main/interp_update.h"
#include "main/ease_step.h"
#include "main/text_tag.h"
#include "main/xgl_font.h"

#include "shared.h"

extern float cosf(float x);


/* Per-frame update of a text slot: ramps the timer, steps the scan and the cosine easing, advances the counter. */
void interpUpdate(MsgSlot *slot)
{
    if (slot->state != MSG_SLOT_STATE_TEXT) {
        return;
    }
    if (gameWork.control & CONTROL_PAUSED) {
        return;
    }
    if (slot->flags & MSG_SLOT_OPENING) {
        slot->open_counter++;
        if (slot->open_counter >= INTERP_OPEN_FRAMES) {
            slot->flags = (slot->flags & ~MSG_SLOT_OPENING) | (MSG_SLOT_OPEN | INTERP_FLAG_SCAN);
        }
    }
    if (slot->flags & MSG_SLOT_CLOSING) {
        slot->open_counter--;
        if (slot->open_counter <= 0) {
            slot->flags = 0;
        }
    }
    if (slot->flags & INTERP_FLAG_SCAN) {
        if (interpAdvanceCursor(slot)) {
            slot->flags &= ~INTERP_FLAG_SCAN;
        }
    }
    if (slot->flags & INTERP_FLAG_EASE) {
        if (interpStepCosine(slot)) {
            slot->flags &= ~INTERP_FLAG_EASE;
        }
    }
    /* Open and no longer scanning (the compiler merges both tests into one doubleword read). */
    if ((slot->flags & MSG_SLOT_OPEN) && !(slot->flags & INTERP_FLAG_SCAN)) {
        talkAdvanceCounter(slot);
    }
}

/*
 * Advances the text cursor of the current line range by one unit once the
 * wait counter reaches the pending delay. Leading '$' control units are
 * consumed first. Returns 1 when the cursor has reached the end of the range
 * (it is then clamped to the end), 0 while text remains or while waiting.
 */
int interpAdvanceCursor(MsgSlot *slot)
{
    MsgSlotText *interp;
    const char *end;

    if (slot->state != MSG_SLOT_STATE_TEXT) {
        return 0;
    }
    interp = &slot->u.text;
    if (interp->halted != 0) {
        return 0;
    }
    end = textSkipLines(interp->start, interp->first_line + interp->visible_rows - 1);
    if (interp->reveal < end) {
        interp->wait++;
        if (interp->wait < interp->pending) {
            return 0;
        }
        interp->wait = 0;
        while (*interp->reveal == '$') {
            interp->reveal = xglFontCopyTextUnit(0, interp->reveal, 0);
        }
        interp->reveal = textSkipTwoUnits(interp->reveal);
        if (interp->reveal >= end - 1) {
            interp->reveal = end;
            return 1;
        }
    } else {
        return 1;
    }
    return 0;
}


/* Type 1 (text) slot handler: refreshes the slot per pending flag, then draws its text box and tags. */
/* Declared int to match msg_window_run.c's call site; the original has no return value path (a void definition
 * changes the bytes). */
int interpDrawTextSlot(MsgSlot *slot)
{
    if (slot->state == MSG_SLOT_STATE_TEXT) {
        if (slot->flags & MSG_SLOT_OPENING) {
            talkDrawFramesBasic(slot);
        }
        if (slot->flags & MSG_SLOT_CLOSING) {
            talkDrawFramesBasic(slot);
        }
        if (slot->flags & MSG_SLOT_OPEN) {
            talkDrawFramesBasic(slot);
            talkDrawTextBox(slot);
            textTagDrawActive(slot->u.text.reveal, slot->u.text.scroll_y, slot);
        }
    }
}

void interpRewind(MsgSlot *slot)
{
    if (slot->state == MSG_SLOT_STATE_TEXT) {
        textSetOutputCursor(slot->u.text.start);
        /* The original clears the two bits with two separate masks (li -5, li -9): the first AND on the flags read
         * as int and the second on the (u32) mask keep the compiler from folding them into one. */
        slot->flags = (((int) slot->flags & ~MSG_SLOT_OPENING) & ~((u32) MSG_SLOT_OPEN)) | MSG_SLOT_CLOSING;
    }
}

void interpSetPending(MsgSlot *slot, int value)
{
    if (slot->state == MSG_SLOT_STATE_TEXT) {
        slot->u.text.pending = value;
    }
}

int interpScrollBy(MsgSlot *slot, int advance)
{
    MsgSlotText *interp;
    int old_value;
    int cursor;
    int next_cursor;
    int base;

    interp = &slot->u.text;
    if (slot->state == MSG_SLOT_STATE_TEXT) {
        slot->flags |= INTERP_FLAG_EASE;
        old_value = interp->scroll_y;
        cursor = interp->first_line + advance;
        interp->scroll_from = old_value;
        interp->first_line = cursor;
        next_cursor = xglClampInt(cursor, 1, interp->line_count - interp->visible_rows + 1);
        interp->first_line = next_cursor;
        base = interpLineScrollY(next_cursor);
        interp->angle = 0;
        interp->scroll_to = base;
        return interp->angle_step = easeAngleStep(interp->scroll_from, base);
    }
}

int interpStepCosine(MsgSlot *slot)
{
    MsgSlotText *interp;
    int angle;
    int base;
    int half_sum;
    int midpoint;
    float span;

    interp = &slot->u.text;
    angle = interp->angle + interp->angle_step;
    interp->angle = angle;
    if (angle >= INTERP_ANGLE_END) {
        interp->scroll_y = interp->scroll_to;
        return 1;
    }
    base = interp->scroll_to;
    half_sum = base + interp->scroll_from;
    midpoint = half_sum >> 1;
    span = (float) (midpoint - base);
    interp->scroll_y = midpoint + (int) (span * cosf((float) angle * 0.0174532793f));
    return 0;
}
