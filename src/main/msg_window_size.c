#include "common.h"
#include "main/msg_window_size.h"

/* Overflow test: 1 when the window is active and its text has six lines or more (layout clamps at five). */
int msgWindowOverflows(MsgSlot *window) {
    if (window->state != MSG_SLOT_STATE_WINDOW) {
        return 0;
    }
    if (window->u.list.capacity < MSG_WINDOW_OVERFLOW_LINES) {
        return 0;
    }
    return 1;
}

/* Empty-list layout: when the filled-slot count is zero, the empty list window position and size. */
void msgWindowSetEmptyLayout(MsgSlot *window) {
    if (window->u.list.item_count == 0) {
        window->x = MSG_LIST_X;
        window->y = MSG_LIST_Y_EMPTY;
        window->extent_x = MSG_LIST_WIDTH;
        window->extent_y = MSG_LIST_HEIGHT_EMPTY;
    }
}

/* Row pitch of the message list: rows * MSG_LIST_ROW_PIXELS pixels (the step used by the list layout). */
int msgWindowRowPixels(int rows) {
    return rows * MSG_LIST_ROW_PIXELS;
}
