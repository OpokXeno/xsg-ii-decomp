#include "common.h"
#include "main/msg_slot_center.h"
#include "main/msg_window_center.h"
#include "main/msg_slot_core.h"

/* Centres the window: x comes from msgSlotCenterX (0x100 - width / 2), y from msgWindowCenterY. */
void msgWindowCenter(MsgSlot *window)
{
    window->x = msgSlotCenterX(window);
    window->y = msgWindowCenterY(window);
}

/* Vertical centre: 0xD2 - height / 2 from five lines up, 0xE0 - height / 2 below (msgSlotCenterY has 0xE0). */
int msgWindowCenterY(MsgSlot *window)
{
    int half_height;

    half_height = window->extent_y >> 1;
    if (window->u.list.capacity >= 5) {
        return 0xD2 - half_height;
    }
    return 0xE0 - half_height;
}

/* Opening phase: when the window is active, clears the closing bit 0x2 and sets the opening bit 0x4. */
void msgWindowOpen(MsgSlot *window)
{
    if (window->slot_type == MSG_SLOT_TYPE_WINDOW) {
        window->flags = (window->flags & ~MSG_SLOT_CLOSING) | MSG_SLOT_OPENING;
    }
}
