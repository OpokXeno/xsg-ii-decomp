#include "common.h"
#include "shared.h"
#include "main/record_render.h"
#include "main/effect_request.h"
#include "main/xgl_render.h"
#include "main/msg_window_run.h"
#include "main/talk_script.h"
#include "main/msg_text_draw.h"
#include "main/msg_slot_core.h"
#include "main/text_tag.h"

/* Opens the .sdata of this file (0x69DCA0); read by msg_text_draw.c. */
int sellLabelTextOff = 0;

extern void *memset(void *destination, int value, unsigned int count);

/* Packs an alpha byte over opaque white: 0xAAFFFFFF. */
#define EFFECT_ALPHA_WHITE(alpha) (((alpha) << 24) | 0xFFFFFF)


/* effectRequestToCommand: translates an effect request (kind, with a strength value) into the six-word parameter block and
   command code that recordSetPreset takes. Kinds not listed send command 0 with an unset block. */
void effectRequestToCommand(int bank, unsigned int kind, unsigned int arg, unsigned int value)
{
    int params[12];
    int command = 0;
    int scaled;
    float cells;
    float ratio;

    switch (kind) {
    case 0:
        break;
    case 1:
        params[1] = 0;
        params[0] = EFFECT_ALPHA_WHITE(value);
        command = 10;
        break;
    case 2:
        params[1] = 0;
        params[0] = value << 24;
        command = 10;
        break;
    case 35:
        params[1] = 1;
        params[0] = EFFECT_ALPHA_WHITE(value);
        command = 10;
        break;
    case 7:
        value = value < 0x4D ? value : 0x4C;
        params[0] = 0x80FFFFFF;
        params[1] = value + 0x32;
        params[2] = value + 10;
        command = 8;
        break;
    case 3:
        params[0] = 0x40FFFFFF;
        if (value == 0) {
            value = 1;
        }
        params[1] = value;
        command = 11;
        break;
    case 4:
        params[0] = 0x40FFFFFF;
        if (value == 0) {
            value = 1;
        }
        params[1] = value;
        command = 18;
        break;
    case 5:
        params[0] = EFFECT_ALPHA_WHITE(value);
        command = 12;
        break;
    case 8:
    case 9:
    case 12:
    case 13:
        scaled = (int)((float)value / 127.0f * 247.0f);
        command = 13;
        switch (kind) {
        case 8:
        case 9:
            params[0] = 0xFFFFFF;
            break;
        case 12:
        case 13:
            params[0] = 0;
            break;
        }
        switch (kind) {
        case 8:
        case 12:
            params[1] = scaled;
            params[2] = scaled - 0x32;
            break;
        case 9:
        case 13:
            params[2] = scaled;
            params[1] = scaled - 0x32;
            break;
        }
        if (params[1] < 0) {
            params[1] = 0;
        }
        if (params[1] > 0x7F) {
            params[1] = 0x7F;
        }
        if (params[2] < 0) {
            params[2] = 0;
        }
        if (params[2] > 0x7F) {
            params[2] = 0x7F;
        }
        params[3] = 0;
        break;
    case 10:
    case 11:
    case 14:
    case 15:
        scaled = (int)((float)value / 127.0f * 197.0f);
        command = 13;
        switch (kind) {
        case 10:
        case 11:
            params[0] = 0xFFFFFF;
            break;
        case 14:
        case 15:
            params[0] = 0;
            break;
        }
        switch (kind) {
        case 10:
        case 14:
            params[1] = scaled;
            params[2] = scaled - 0x2D;
            break;
        case 11:
        case 15:
            params[2] = scaled;
            params[1] = scaled - 0x2D;
            break;
        }
        if (params[1] < 0) {
            params[1] = 0;
        }
        if (params[1] > 0x7F) {
            params[1] = 0x7F;
        }
        if (params[2] < 0) {
            params[2] = 0;
        }
        if (params[2] > 0x7F) {
            params[2] = 0x7F;
        }
        params[3] = 1;
        break;
    case 16:
    case 17:
        ratio = (float)value / 127.0f;
        params[0] = 0x7FFFFFFF;
        params[3] = 0xFF4020;
        params[4] = 0;
        params[2] = (int)(360.0f - ratio * 360.0f);
        command = 14;
        if (kind == 16) {
            params[1] = 1;
        } else if (kind == 17) {
            params[1] = 2;
        }
        break;
    case 18:
    case 19:
        ratio = (float)value / 127.0f;
        params[0] = 0x7FFFFFFF;
        params[3] = 0xFF0000;
        params[4] = 0;
        params[2] = (int)(360.0f - ratio * 360.0f);
        command = 15;
        if (kind == 18) {
            params[1] = 1;
        } else if (kind == 19) {
            params[1] = 0;
        }
        break;
    case 6:
        params[1] = value;
        params[0] = 0x7FFFFFFF;
        command = 9;
        if ((int)value <= 0) {
            params[1] = 1;
        }
        break;
    case 20:
        command = 3;
        params[0] = EFFECT_ALPHA_WHITE(value);
        params[1] = 0;
        params[2] = 0x11;
        params[3] = 0x11;
        break;
    case 36:
        command = 1;
        cells = (float)recordGridCellCount();
        ratio = (float)value / 127.0f;
        params[0] = 0x7FFFFFFF;
        params[1] = (int)(cells * ratio);
        break;
    case 21:
        params[0] = EFFECT_ALPHA_WHITE(value);
        params[2] = 10;
        command = 5;
        params[3] = -13;
        params[5] = 7;
        params[1] = 0;
        params[4] = 0;
        break;
    case 22:
        params[0] = EFFECT_ALPHA_WHITE(value);
        params[2] = 0x12;
        command = 5;
        params[3] = -20;
        params[5] = 7;
        params[1] = 0;
        params[4] = 0;
        break;
    case 23:
        params[0] = EFFECT_ALPHA_WHITE(value);
        params[1] = 1;
        command = 5;
        params[2] = 10;
        params[3] = -13;
        params[4] = (value << 24) | 0xFAB4B4;
        params[5] = 7;
        break;
    case 24:
        params[0] = EFFECT_ALPHA_WHITE(value);
        command = 4;
        params[2] = 12;
        params[3] = 7;
        params[1] = 0;
        params[4] = 4;
        params[5] = 0;
        break;
    case 25:
        params[0] = EFFECT_ALPHA_WHITE(value);
        params[1] = 0xAF;
        command = 16;
        params[2] = 22;
        break;
    case 26:
        params[1] = 1;
        params[0] = EFFECT_ALPHA_WHITE(value);
        command = 11;
        break;
    case 27:
        params[1] = 1;
        params[0] = EFFECT_ALPHA_WHITE(value);
        command = 18;
        break;
    case 28:
        params[1] = value;
        command = 20;
        params[0] = 0;
        break;
    case 29:
        params[0] = value;
        command = 22;
        break;
    case 30:
        params[0] = value >> 4;
        command = 19;
        break;
    case 31:
        params[1] = value;
        params[0] = 2;
        command = 20;
        break;
    case 32:
        params[1] = value;
        params[0] = 1;
        command = 20;
        break;
    case 33:
        params[0] = value;
        command = 21;
        break;
    case 34:
        params[0] = value;
        command = 23;
        break;
    }
    recordSetPreset(bank, command, arg, params);
}


void recordClearAllBanks(void) {
    recordClearBanks();
}

/* Thin forwarder to recordPauseUpdates (record_render.c); no caller is recovered, so only the forwarding is
 * evidenced. */
void recordUpdatesHold(void) {
    recordPauseUpdates();
}

/* msgWindowReset: clears the window, sets current_index to -1 and runs three init helpers. */
void msgWindowReset(MsgWindow *window) {
    memset(window, 0, sizeof(MsgWindow));
    window->current_index = -1;
    talkStartTextureLoad();
    msgWindowLoadBotanXtx();
    textOutputReset();
}

/* msgWindowRequestFlush: the window becomes the pending window; msgWindowFlush is armed in slot 0. */
void msgWindowRequestFlush(MsgWindow *window) {
    msgPendingWindow = window;
    msgFrameHooks[0] = msgWindowFlush;
}

/* msgQueueSlot: appends a slot pointer to the queue and arms msgWindowFlush in slot 0. */
void msgQueueSlot(MsgSlot *slot) {
    int index = msgSlotQueueCount;

    msgSlotQueue[index] = slot;
    msgFrameHooks[0] = msgWindowFlush;
    msgSlotQueueCount = index + 1;
}

/* msgWindowFlush: runs the pending window's slots, then the queued slots, then clears the queue. */
void msgWindowFlush(void) {
    int i;

    flushFontBatch();
    if (msgPendingWindow != 0) {
        msgWindowRunSlots(msgPendingWindow);
    }
    for (i = 0; i < msgSlotQueueCount; i++) {
        if (msgSlotQueue[i] != 0) {
            msgSlotRun(msgSlotQueue[i]);
        }
    }
    talkDeactivate();
    msgPendingWindow = 0;
    memset(msgSlotQueue, 0, sizeof(MsgSlot *) * MSG_SLOT_QUEUE_SIZE);
    msgSlotQueueCount = 0;
    msgFrameHooks[0] = 0;
}

/* Pending-window and queue state, defined here so the object holds the original zeroed items. */
MsgSlot *msgSlotQueue[MSG_SLOT_QUEUE_SIZE] = { 0 };
MsgWindow *msgPendingWindow = 0;
int msgSlotQueueCount = 0;
