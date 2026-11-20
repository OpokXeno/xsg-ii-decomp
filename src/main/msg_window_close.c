#include "common.h"
#include "main/msg_window_close.h"
#include "main/msg_slot_core.h"
#include "main/msg_window_run.h"
#include "main/interp_update.h"
#include "main/msg_window_update.h"
#include "main/timecode_window.h"

/* Returns the index of the current slot (current_index). */
int msgWindowGetFirstWord(MsgWindow *window)
{
    return window->current_index;
}

/* Slot flag bits tested by the message window loops: the same bits as the list bits MSG_WIN_SLIDE (slide in progress)
 * and MSG_WIN_SWEEP (cursor sweep in progress) of msg_window_update.c. */
#define MSG_SLOT_SLIDING 0x20
#define MSG_SLOT_SWEEPING 0x100

/* Starts closing the current slot unless it is a window slot that is sliding or sweeping. */
void msgWindowCloseCurrent(MsgWindow *window)
{
    int current;
    MsgSlot *status;

    current = window->current_index;
    if (current < 0) {
        return;
    }
    status = &window->slots[current];
    if (status->slot_type == MSG_SLOT_TYPE_WINDOW) {
        if (status->flags & MSG_SLOT_SLIDING) {
            return;
        }
        if (status->flags & MSG_SLOT_SWEEPING) {
            return;
        }
    }
    msgWindowStartClosingSlot(window, current);
}

/* Sets current_index to the last slot with flag 0x1 set (-1 when none is). */
void msgWindowFindLastActive(MsgWindow *window)
{
    MsgSlot *slots = window->slots;
    int active = window->slots[MSG_SLOT_COUNT - 1].flags & MSG_SLOT_ACTIVE;

    window->current_index = MSG_SLOT_COUNT - 1;
    if (active) {
        return;
    }
    do {
        window->current_index--;
    } while (window->current_index >= 0 && !(slots[window->current_index].flags & MSG_SLOT_ACTIVE));
}
/* Moves active slots down over inactive ones so the active slots are packed at the front. */
void msgWindowCompact(MsgWindow *window)
{
    MsgSlot *slots = window->slots;
    int slot;
    int next;

    for (slot = 0; slot <= window->current_index && slot < MSG_SLOT_COUNT - 1; slot++) {
        if (!(slots[slot].flags & MSG_SLOT_ACTIVE)) {
            for (next = slot + 1; next <= window->current_index && next < MSG_SLOT_COUNT; next++) {
                if (slots[next].flags & MSG_SLOT_ACTIVE) {
                    slots[slot] = slots[next];
                    slots[next].flags = 0;
                    if (window->current_index == next) {
                        window->current_index = slot;
                    }
                    break;
                }
            }
        }
    }
}

/* Runs the close/rewind handler of slot `index` for its state (text, window, timecode, item or choice). */
void msgWindowStartClosingSlot(MsgWindow *window, int index)
{
    MsgSlot *entries = window->slots;
    MsgSlot *status;

    if (index < 0 || entries[index].flags == 0) {
        return;
    }
    status = &entries[index];
    switch (status->slot_type) {
    case MSG_SLOT_TYPE_TEXT: interpRewind(status); break;
    case MSG_SLOT_TYPE_WINDOW: msgWindowClose(status); break;
    case MSG_SLOT_TYPE_TIMECODE: timecodeBeginClose(status); break;
    case MSG_SLOT_TYPE_ITEM: msgSlotStartClosing(status); break;
    }
}

/* Runs all five slots unless the current one is done or paused. */
void msgWindowCloseAll(MsgWindow *window)
{
    int index;
    int flags;

    flags = window->slots[window->current_index].flags;
    if (flags & MSG_SLOT_SLIDING) {
        return;
    }
    if (flags & MSG_SLOT_SWEEPING) {
        return;
    }
    for (index = 0; index < MSG_SLOT_COUNT; index++) {
        msgWindowStartClosingSlot(window, index);
    }
}
