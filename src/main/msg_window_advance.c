#include "common.h"

#include "main/interp_update.h"
#include "main/msg_window_run.h"
#include "main/msg_window_update.h"
#include "main/msg_window_size.h"
#include "main/msg_slot_core.h"

/* Runs the current record by its state; text and window slots call a handler with value. */
int msgWindowAdvanceCurrent(MsgWindow *window, int value)
{
    int current;
    int state;
    int result;
    MsgSlot *slot;

    current = window->current_index;
    result = current * 4; /* computed before the current >= 0 test, as in the original */
    if (current >= 0) {
        slot = &window->slots[current];
        state = slot->slot_type;
        result = state < MSG_SLOT_TYPE_TIMECODE;
        if (state != MSG_SLOT_TYPE_WINDOW) {
            if (result != 0) {
                result = 1;
                if (state == MSG_SLOT_TYPE_TEXT) {
                    result = interpScrollBy(slot, value);
                }
            }
        } else {
            result = msgWindowScrollBy(slot, value);
        }
    }
    return result;
}

/* Polls the current record: a text slot asks whether the talk has more lines, a window slot whether the window overflows. */
int msgWindowPollMoreText(MsgWindow *window) {
    int pending = 0;
    int current = window->current_index;
    MsgSlot *slot;
    int state;

    if (current < 0) {
        return 0;
    }
    slot = &window->slots[current];
    state = slot->slot_type;
    if (state != MSG_SLOT_TYPE_WINDOW) {
        if (state < MSG_SLOT_TYPE_TIMECODE) {
            if (state == MSG_SLOT_TYPE_TEXT) {
                pending = talkHasMoreLines(slot);
            }
        }
    } else {
        pending = msgWindowOverflows(slot);
    }
    return pending;
}

/* Stores value in the current slot (its frame_color) and returns the record base. */
void *msgWindowSetFrameColor(MsgWindow *window, int value) {
    int current;
    void *result;

    current = window->current_index;
    result = (void *) (current * 4); /* computed before the current >= 0 test, as in the original */
    if (current >= 0) {
        MsgSlot *slot = &window->slots[current];
        slot->frame_color = value;
        result = (char *) slot - 8; /* the record base, 8 bytes before the slot, as the original returns */
    }
    return result;
}
