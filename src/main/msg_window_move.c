#include "common.h"
#include "main/msg_window_run.h"
#include "main/msg_window_move.h"
#include "main/msg_slot_start.h"
#include "main/msg_list_slot.h"
#include "main/timecode_window.h"
/* Grid step and origin used by msgWindowResizeCurrentCells (the 20, 30 and 64 of its
 * argument arithmetic). */
#define DISPATCH_STEP_X 20
#define DISPATCH_STEP_Y 30
#define DISPATCH_ORIGIN 64


void msgWindowMoveCurrent(MsgWindow *window, int x, int y)
{
    int current;
    MsgSlot *slot;

    current = window->current_index;
    if (current < 0) {
        return;
    }
    slot = &window->slots[current];
    if (slot->flags == 0) {
        return;
    }
    switch (slot->slot_type) {
    case MSG_SLOT_TYPE_TEXT:
        msgTextSlotFit(slot, x, y, slot->extent_x, slot->extent_y);
        break;

    case MSG_SLOT_TYPE_WINDOW:
        msgListSlotSetGeometry(slot, x, y, slot->extent_x, slot->extent_y);
        break;

    case MSG_SLOT_TYPE_TIMECODE:
        timecodeSetGeometry(slot, x, y, slot->extent_x, slot->extent_y);
        break;
    }
}

void msgWindowResizeCurrent(MsgWindow *window, int x, int y)
{
    int current;
    MsgSlot *slot;

    current = window->current_index;
    if (current < 0) {
        return;
    }
    slot = &window->slots[current];
    if (slot->flags == 0) {
        return;
    }
    switch (slot->slot_type) {
    case MSG_SLOT_TYPE_TEXT:
        msgTextSlotFit(slot, slot->x, slot->y, x, y);
        break;

    case MSG_SLOT_TYPE_WINDOW:
        msgListSlotSetGeometry(slot, slot->x, slot->y, x, y);
        break;

    case MSG_SLOT_TYPE_TIMECODE:
        timecodeSetGeometry(slot, slot->x, slot->y, x, y);
        break;
    }
}

void msgWindowResizeCurrentCells(MsgWindow *window, int x, int y)
{
    int current;
    MsgSlot *slot;

    current = window->current_index;
    if (current >= 0) {
        slot = &window->slots[current];
        msgTextSlotFit(slot, slot->x, slot->y,
                      x * DISPATCH_STEP_X + DISPATCH_ORIGIN,
                      y * DISPATCH_STEP_Y + DISPATCH_ORIGIN);
    }
}
