#include "common.h"
#include "main/msg_list_slot.h"
#include "main/msg_window_layout.h"
#include "main/timecode_counter.h"
#include "main/ease_step.h"


#define BODY_MARGIN 0x20          /* inset above and below the rows */
#define BODY_HEADER_INSET 0x48    /* top inset when a header band is present */
#define BODY_MIN_WIDTH 100
#define BODY_ROW_PITCH 0x1E       /* row height divided into the range by msgListRowsBetween */
#define BODY_PULSE_START 0x10
#define BODY_PULSE_STEP 3

/*
 * Initialises the slot as a list window (state 2): msgSlotInit clears it and stores the state
 * (main/msg_window_move.c calls msgListSlotSetGeometry for state 2), then starts the highlight pulse at its
 * minimum, rising (BODY_PULSE_START, BODY_PULSE_STEP: MSG_PULSE_MIN and MSG_PULSE_STEP of main/msg_window_update.c).
 */
void msgListSlotInit(MsgSlot *slot) {
    msgSlotInit(slot, MSG_SLOT_STATE_WINDOW);
    slot->u.list.pulse_value = BODY_PULSE_START;
    slot->u.list.pulse_step = BODY_PULSE_STEP;
}

/*
 * List window (state 2) update: re-applies the slot geometry, raises the width to at least BODY_MIN_WIDTH, then
 * stores in visible_rows the rows msgListRowsBetween fits below y (inset BODY_MARGIN, or BODY_HEADER_INSET when
 * the item list is bound and adds its header band).
 */
void msgListSlotSetGeometry(MsgSlot *slot, int x, int y, int width, int height) {
    if (slot->state == MSG_SLOT_STATE_WINDOW) {
        msgSlotSetGeometry(slot, x, y, width, height);
        if (slot->extent_x < BODY_MIN_WIDTH) {
            slot->extent_x = BODY_MIN_WIDTH;
        }
        if (slot->u.list.list_active == 0) {
            slot->u.list.visible_rows = msgListRowsBetween(slot->y + BODY_MARGIN, slot->y + slot->extent_y - BODY_MARGIN);
        } else {
            slot->u.list.visible_rows = msgListRowsBetween(slot->y + BODY_HEADER_INSET, slot->y + slot->extent_y - BODY_MARGIN);
        }
    }
}

/* Forwards its two arguments to deltaPerStep with a fixed third argument BODY_ROW_PITCH. */
int msgListRowsBetween(int first, int second) {
    return deltaPerStep(first, second, BODY_ROW_PITCH);
}
