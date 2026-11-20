#include "common.h"
#include "main/msg_window_text.h"
#include "main/interp_text.h"
#include "main/interp_update.h"
#include "main/msg_slot_center.h"
#include "main/msg_slot_start.h"
#include "main/msg_slot_core.h"

/* Default reveal delay of new text slots (msgWindowStartValue; msgResetAll in main/msg_state.c sets it to 1). */
extern int msgWindowStartValue;

/* Geometry of a newly opened text slot, passed through msgTextSlotFit to msgSlotSetGeometry (x, y, width, height). */
#define TEXT_SLOT_X (-5)
#define TEXT_SLOT_Y 0x12C
#define TEXT_SLOT_WIDTH 0x208
#define TEXT_SLOT_HEIGHT 0x9A

/* Sets the default reveal delay of new text slots. */
void msgWindowSetStartValue(int start_value) {
    msgWindowStartValue = start_value;
}

/* Opens the next message slot (at most five) as a text slot, sizes it and starts it; returns 1 on success, 0 when full. */
int msgWindowOpenNextTextSlot(MsgWindow *window) {
    int index;
    MsgSlot *slot;

    index = window->current_index + 1;
    if (index >= MSG_SLOT_COUNT) {
        return 0;
    }
    window->current_index = index;
    slot = &window->slots[index];
    msgTextSlotStart(slot);
    msgTextSlotFit(slot, TEXT_SLOT_X, TEXT_SLOT_Y, TEXT_SLOT_WIDTH, TEXT_SLOT_HEIGHT);
    msgSlotSetZ(slot, window->current_index * MSG_SLOT_VALUE_STEP + MSG_SLOT_VALUE_BASE);
    interpSetOpening(slot);
    interpSetPending(slot, msgWindowStartValue);
    return 1;
}

/* Feeds the text input to the current slot of the message window, if there is one (current_index -1 for none). */
void msgWindowFeedCurrentText(MsgWindow *window, const char *text) {
    int current;

    current = window->current_index;
    if (current >= 0) {
        msgEntryFeedText(&window->slots[current], text);
    }
}

int msgWindowStartValue = 1;
