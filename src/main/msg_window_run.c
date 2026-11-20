#include "common.h"
#include "main/talk_script.h"
#include "main/msg_window_run.h"
#include "main/msg_window_close.h"
#include "main/interp_update.h"
#include "main/text_tag.h"
#include "main/msg_slot_core.h"
#include "main/msg_window_update.h"
#include "main/timecode_window.h"

/* Runs the window's five slots: an active slot goes to msgSlotRun, an inactive current slot re-seeks the last active one. */
void msgWindowRunSlots(MsgWindow *window) {
    MsgSlot *slots = window->slots;
    int index;

    msgWindowCompact(window);
    for (index = 0; index < MSG_SLOT_COUNT; index++) {
        MsgSlot *slot = &slots[index];

        if (!(slot->flags & MSG_SLOT_ACTIVE)) {
            if (index == window->current_index) {
                msgWindowFindLastActive(window);
            }
        } else {
            msgSlotRun(slot);
        }
    }
}

/* Runs one active slot's update and draw pair, chosen by the slot's type. */
void msgSlotRun(MsgSlot *slot) {
    if (!(slot->flags & MSG_SLOT_ACTIVE)) {
        return;
    }
    switch (slot->slot_type) {
    case MSG_SLOT_TYPE_TEXT:
        interpUpdate(slot);
        interpDrawTextSlot(slot);
        break;
    case MSG_SLOT_TYPE_WINDOW:
        msgWindowUpdate(slot);
        msgWindowDraw(slot);
        break;
    case MSG_SLOT_TYPE_TIMECODE:
        timecodeUpdate(slot);
        timecodeSlotDraw(slot);
        break;
    case MSG_SLOT_TYPE_ITEM:
        msgSlotItemTick(slot);
        msgSlotDraw(slot);
        break;
    }
}

void talkEnd(void) {
    msgWindowUnloadBotanXtx();
    talkEndTexture();
}
