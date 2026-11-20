#include "common.h"
#include "main/talk_scan.h"
#include "main/talk_script.h"
#include "main/msg_slot_core.h"
#include "main/msg_text_draw.h"
#include "main/xgl_font.h"

enum {
    TALK_FRAME_COUNTER_LIMIT = 16,
};

/* Flag bit talkSetScanFlag sets: the cursor of the text slot is still advancing (INTERP_FLAG_SCAN). */

/*
 * The helpers below act on a slot type 1 (text) slot; they do nothing for another slot type. The text lines are
 * reached through the textSkipLines/textCountLinesBefore pair (msg_text_draw.c): one turns a line index into a text
 * position, the other counts the lines up to a position.
 */

/* Advances the slot's frame counter, wrapping at TALK_FRAME_COUNTER_LIMIT. */
void talkAdvanceCounter(MsgSlot *slot) {
    MsgSlotText *text;
    unsigned int counter;

    text = &slot->u.text;
    if (slot->slot_type == MSG_SLOT_TYPE_TEXT) {
        counter = text->frame_counter + 1;
        text->frame_counter = counter;
        if (counter >= TALK_FRAME_COUNTER_LIMIT) {
            text->frame_counter = 0;
        }
    }
}

/* Nonzero while the slot's last shown line is still before the last line of the text. */
int talkHasMoreLines(MsgSlot *slot) {
    if (slot->slot_type != MSG_SLOT_TYPE_TEXT) {
        return 0;
    }
    return (slot->u.text.first_line + slot->u.text.visible_rows - 1) < slot->u.text.line_count;
}

void talkSetScanFlag(MsgSlot *slot) {
    if (slot->slot_type == MSG_SLOT_TYPE_TEXT) {
        slot->flags |= INTERP_FLAG_SCAN;
    }
}

/* Moves the reveal position by `amount` lines: position -> line index, add, back to a position. */
void talkScrollLines(MsgSlot *slot, int amount) {
    MsgSlotText *text;

    text = &slot->u.text;
    if (slot->slot_type == MSG_SLOT_TYPE_TEXT) {
        text->reveal = textSkipLines(text->start, textCountLinesBefore(text->start, text->reveal) + amount);
    }
}

/* Moves the reveal position to the last line of the text. */
void talkSeekToLimit(MsgSlot *slot) {
    MsgSlotText *text;

    text = &slot->u.text;
    if (slot->slot_type == MSG_SLOT_TYPE_TEXT) {
        text->reveal = textSkipLines(text->start, text->line_count);
    }
}

/* Moves the reveal position to the last line of the window. */
void talkSeekToWindowEnd(MsgSlot *slot) {
    MsgSlotText *text;

    text = &slot->u.text;
    if (slot->slot_type == MSG_SLOT_TYPE_TEXT) {
        text->reveal = textSkipLines(text->start, (text->first_line + text->visible_rows) - 1);
    }
}

/* Stores the wrap width limit of the text. */
void talkSetSetting(MsgSlot *slot, int width_limit) {
    if (slot->slot_type == MSG_SLOT_TYPE_TEXT) {
        slot->u.text.width_limit = width_limit;
    }
}


/* Calls talkSetActive, which sets the talk-active flag talkActive. */
void talkActivate(void) {
    talkSetActive();
}

/* Skips two text units of text (xglFontCopyTextUnit into the word buffer) and returns the text after them. */
const char *textSkipTwoUnits(const char *text) {
    const char *result;
    int pass;

    result = text;
    for (pass = 0; pass < 2; pass++) {
        result = xglFontCopyTextUnit(0, result, 0);
    }
    return result;
}
