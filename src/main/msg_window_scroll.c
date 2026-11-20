#include "common.h"
#include "shared.h"
#include "main/msg_window_run.h"
#include "main/interp_update.h"
#include "main/talk_script.h"
#include "main/talk_scan.h"
#include "main/effect_request.h"
#include "main/msg_window_update.h"
#include "main/timecode_counter.h"

extern void *memset(void *destination, int value, unsigned int count);

void msgEntrySetTalkScanFlag(MsgSlot *entry);
void msgEntryScrollLines(MsgSlot *entry, int value);
void msgEntrySeekToLimit(MsgSlot *entry);

/* Forwards the current slot to msgEntrySetTalkScanFlag (talkSetScanFlag), if there is one. */
void msgWindowStartScan(MsgWindow *window) {
    if (window->current_index >= 0) {
        msgEntrySetTalkScanFlag(&window->slots[window->current_index]);
    }
}

void msgWindowScrollLines(MsgWindow *window, int value) {
    if (window->current_index >= 0) {
        msgEntryScrollLines(&window->slots[window->current_index], value);
    }
}

void msgWindowSeekToLimit(MsgWindow *window) {
    if (window->current_index >= 0) {
        msgEntrySeekToLimit(&window->slots[window->current_index]);
    }
}

void msgWindowSeekToWindowEnd(MsgWindow *window) {
    if (window->current_index >= 0) {
        talkSeekToWindowEnd(&window->slots[window->current_index]);
    }
}

void msgWindowRewindLastTextSlot(MsgWindow *window) {
    int index;

    if (msgWindowFindSlotByState(&index, window, MSG_SLOT_STATE_TEXT) != 0) {
        interpRewind(&window->slots[index]);
    }
}

void msgWindowSetSetting(MsgWindow *window, int value) {
    if (window->current_index >= 0) {
        talkSetSetting(&window->slots[window->current_index], value);
    }
}


int msgWindowGetCurrentItemIndex(MsgWindow *window) {
    int current = window->current_index;
    if (current < 0) {
        return 0;
    }
    return msgWindowGetItemIndex(&window->slots[current]);
}

void msgWindowMoveSelection(MsgWindow *window, int value) {
    if (window->current_index >= 0) {
        msgWindowMoveCursor(&window->slots[window->current_index], value);
    }
}

void msgWindowCloseLastListSlot(MsgWindow *window) {
    int index;

    if (msgWindowFindSlotByState(&index, window, MSG_SLOT_STATE_WINDOW) != 0) {
        msgWindowClose(&window->slots[index]);
    }
}

static int msgEntryTestFlags(MsgWindow *window, int index, int mask);

/* Tests mask against the flags of the current record, 0 without one. The helper is a separate function in the
 * original (msgEntryTestFlags, reached by a tail jump) and repeats the current_index test. */
int msgWindowTestCurrentFlags(MsgWindow *window, int mask) {
    int current = window->current_index;

    if (current < 0) {
        return 0;
    }
    return msgEntryTestFlags(window, current, mask);
}

/* Repeats the current_index test of the caller: the original keeps this as its own function, reached by a tail jump. */
static int msgEntryTestFlags(MsgWindow *window, int index, int mask) {
    if (window->current_index < 0) {
        return 0;
    }
    return (window->slots[index].flags & mask) != 0;
}

/* Returns 1 when none of the window's slots is active, 0 otherwise. */
int msgWindowAllInactive(MsgWindow *window) {
    int index;

    for (index = 0; index < MSG_SLOT_COUNT; index++) {
        if ((window->slots[index].flags & MSG_SLOT_ACTIVE) != 0) {
            return 0;
        }
    }
    return 1;
}

/* Classifies the current slot by its type (and, for a timecode, its mode); 0 when none is active. */
int msgWindowCurrentStatus(MsgWindow *window) {
    MsgSlot *entry;
    int current = window->current_index;
    if (current < 0) { return MSG_STATUS_NONE; }
    entry = &window->slots[current];
    if ((entry->flags & MSG_SLOT_ACTIVE) == 0) { return MSG_STATUS_NONE; }
    switch (entry->state) {
    case MSG_SLOT_STATE_WINDOW: return MSG_STATUS_WINDOW;
    case MSG_SLOT_STATE_TEXT: return MSG_STATUS_TEXT;
    case MSG_SLOT_STATE_TIMECODE:
        if (entry->u.timecode.mode == 0) { return MSG_STATUS_TIMECODE_IDLE; }
        if (entry->u.timecode.mode < 0) { break; } /* negative mode: no status, falls to return 0 */
        if (entry->u.timecode.mode < MSG_TIMECODE_SUB_RUNNING_END) { return MSG_STATUS_TIMECODE_RUNNING; }
        break;
    case MSG_SLOT_STATE_ITEM: return MSG_STATUS_ITEM;
    }
    return MSG_STATUS_NONE;
}


/* Sets flag 0x10 of the slot's talk state (talkSetScanFlag). */
void msgEntrySetTalkScanFlag(MsgSlot *entry) {
    talkSetScanFlag(entry);
}


void msgEntryScrollLines(MsgSlot *entry, int value) {
    talkScrollLines(entry, value);
}


void msgEntrySeekToLimit(MsgSlot *entry) {
    talkSeekToLimit(entry);
}

void msgEntryClear(MsgSlot *entry) {
    memset(entry, 0, sizeof(MsgSlot));
    talkStartScriptLoad();
}

/* Queues a slot (msg_state.c passes the slot pointer); forwards it to msgQueueSlot (effect_request.h). */
void msgQueueEntry(MsgSlot *slot) {
    msgQueueSlot(slot);
}

void msgEntryClearEndScript(MsgSlot *entry) {
    memset(entry, 0, sizeof(MsgSlot));
    talkEndScript();
}
