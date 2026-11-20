#include "common.h"
#include "main/debt_box.h"
#include "main/msg_window_layout.h"
#include "main/msg_window_center.h"

/* Stores a new text for an active window, its line count, then sizes and centres the window. */
void msgWindowSetText(MsgSlot *window, char *text)
{
    if (window->state == MSG_SLOT_STATE_WINDOW) {
        window->u.list.text = text;
        window->u.list.capacity = textCountLinesOf(text);
        msgWindowLayoutSize(window);
        msgWindowCenter(window);
    }
}

/* Binds the item list of the window (msgWindowBindCurrentItems in
 * main/msg_window_open.c): source, cost table, count table and debt word, the 40-entry capacity, then
 * counts the filled slots and sizes and places the list window. */
void msgWindowBindItems(MsgSlot *window, char **source, int *costs, int *counts, int *debt)
{
    MsgSlotList *list = &window->u.list;

    if (window->state == MSG_SLOT_STATE_WINDOW) {
        window->u.list.list_source = source;
        window->u.list.item_costs = costs;
        window->u.list.counts = counts;
        window->u.list.debt = debt;
        window->u.list.list_active = 1;
        window->u.list.capacity = MSG_LIST_CAPACITY;
        list->item_count = sellCountFilledSlots(list->counts);
        msgWindowSizeList(window);
        msgWindowPlaceList(window);
    }
}

/* Window size: height = min(capacity, 5 rows) * row pitch + frame, width = max(text width + frame, minimum width). */
void msgWindowLayoutSize(MsgSlot *window)
{
    int lines = window->u.list.capacity;
    int width;

    if (lines > MSG_LIST_VISIBLE_ROWS) {
        lines = MSG_LIST_VISIBLE_ROWS;
    }
    window->extent_y = lines * MSG_LIST_ROW_PIXELS + MSG_LIST_FRAME_PIXELS;
    width = textMaxLineWidth(window) + MSG_LIST_FRAME_PIXELS;
    if (width < MSG_LIST_MIN_WIDTH) {
        width = MSG_LIST_MIN_WIDTH;
    }
    window->extent_x = width;
}

/* List height and width: 0xFE or 0xA5 by whether the item count is nonzero, width 0x180. */
void msgWindowSizeList(MsgSlot *window)
{
    int count = sellCountFilledSlots(window->u.list.counts);

    if (count != 0) {
        window->extent_x = MSG_LIST_WIDTH;
        window->extent_y = MSG_LIST_HEIGHT_FILLED;
    } else {
        window->extent_x = MSG_LIST_WIDTH;
        window->extent_y = MSG_LIST_HEIGHT_EMPTY;
    }
}

/* List position: x = 0x40, y = 0x79 or 0x51 by whether the item count is nonzero. */
void msgWindowPlaceList(MsgSlot *window)
{
    int count = sellCountFilledSlots(window->u.list.counts);

    if (count != 0) {
        window->x = MSG_LIST_X;
        window->y = MSG_LIST_Y_FILLED;
    } else {
        window->x = MSG_LIST_X;
        window->y = MSG_LIST_Y_EMPTY;
    }
}
