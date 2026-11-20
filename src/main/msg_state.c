#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "main/script_register_natives.h"
#include "main/msg_balance.h"
#include "main/msg_state.h"
#include "main/effect_request.h"
#include "main/msg_window_text.h"
#include "main/msg_window_slot.h"
#include "main/msg_window_close.h"
#include "main/timecode_choice.h"
#include "main/timecode_forward.h"
#include "main/msg_window_open.h"
#include "main/msg_window_scroll.h"
#include "main/msg_window_advance.h"
#include "main/timecode_hms.h"

/* Message-system state. msgWindow is the window object, msgChoice the choice
 * object and msgPanel the panel object; the init, close and show functions
 * below call the same object methods on each of them. */
extern int msgSlotIndex;   /* current slot index into msgSlotFlags; -1 when none */
extern int msgChoiceTimer;   /* choice timer: set to 0x5A, ticked, cleared at zero */
static MsgSlot msgChoice;
static MsgSlot msgPanel;
/* Global (msg_window_bind.c's purchase window uses it), allocated in this file's .bss after the statics. */
MsgWindow msgWindow;
int msgSlotFlags[5]; /* five per-slot flag words */
void *memset(void *, int, unsigned int);

/* Reset the slot flags, the window, the choice and the slot index. */
void msgStateReset(void) {
    memset(msgSlotFlags, 0, 0x14);
    msgWindowReset(&msgWindow);
    msgWindowCloseAll(&msgWindow);
    msgWindowSetStartValue(1);
    msgEntryClear(&msgChoice);
    msgSlotIndex = -1;
    msgChoiceTimer = 0;
    talkStateReset();
}

/* Same reset as msgStateReset, and the panel object is initialised too. */
void msgResetAll(void) {
    memset(msgSlotFlags, 0, 0x14);
    msgWindowReset(&msgWindow);
    msgWindowCloseAll(&msgWindow);
    msgWindowSetStartValue(1);
    msgEntryClear(&msgChoice);
    msgEntryClear(&msgPanel);
    msgSlotIndex = -1;
    msgChoiceTimer = 0;
    talkStateReset();
}


void msgGameFlagClearUnlessKept(void) {
    if (msgSlotTestCurrent(MSG_SLOT_KEEP_GAME_FLAG) == 0) {
        msgGameFlagClear();
    }
}

int msgSlotTest(int index, int mask);

/* Test the flag bits of the current slot. */
int msgSlotTestCurrent(int mask) {
    return msgSlotTest(msgSlotCurrent(), mask);
}

/* Clear every slot flag word. */
void msgSlotClearAll(void) {
    int count;

    for (count = 4; count >= 0; count--) {
        msgSlotFlags[count] = 0;
    }
}

/* Store a flag word for one slot. */
void msgSlotSet(int index, int value) {
    msgSlotFlags[index] = value;
}

/* Return the bits of slot `index` that are set in `mask`. */
int msgSlotTest(int index, int mask) {
    return msgSlotFlags[index] & mask;
}


/* Nonzero while the window reports busy (MSG_WINDOW_BUSY). */
int msgWindowBusy(void) {
    return msgWindowTestCurrentFlags(&msgWindow, MSG_WINDOW_BUSY);
}


int msgWindowHasMore(void) {
    return msgWindowPollMoreText(&msgWindow);
}

/* The current slot index (-1 when no slot is open). */
int msgSlotCurrent(void) {
    return msgSlotIndex;
}


/* One step of the open message (nothing while the window status is 2): CIRCLE finishes the window's text,
 * shows the next page or closes the slot; CROSS closes a slot that allows it while the window is not
 * locked. */
void msgUpdate(void)
{
    if (msgSlotCurrent() < 0) {
        return;
    }
    if (msgWindowCurrentStatus(&msgWindow) == 2) {
        return;
    }
    if ((PadData[0].pressed & PAD_CIRCLE) && msgSlotTestCurrent(MSG_SLOT_NO_CIRCLE) == 0) {
        if (msgWindowTestCurrentFlags(&msgWindow, MSG_WINDOW_TYPING) == 1) {
            msgWindowSeekToWindowEnd(&msgWindow);
        } else if (msgWindowHasMore()) {
            if (msgWindowBusy() == 0) {
                msgWindowAdvanceCurrent(&msgWindow, 1);
                msgWindowStartScan(&msgWindow);
            }
        } else if (msgWindowInUse() == 1 && msgWindowBusy() == 0) {
            msgGameFlagClearUnlessKept();
            msgWindowCloseCurrent(&msgWindow);
            msgSlotIndex--;
        }
    }
    if (msgSlotTestCurrent(MSG_SLOT_CROSS_CLOSES) && msgWindowTestCurrentFlags(&msgWindow, MSG_WINDOW_LOCKED) == 0
        && (PadData[0].pressed & PAD_CROSS)) {
        msgGameFlagClearUnlessKept();
        msgWindowCloseCurrent(&msgWindow);
        msgSlotIndex--;
    }
}


/* Runs msgUpdate unless the choice is in state 3, then counts the choice timer down and closes the choice
 * when it runs out. */
void msgUpdateTimed(void)
{
    if (msgSlotStatus(&msgChoice) != 3) {
        msgUpdate();
    }
    if (msgChoiceTimer > 0) {
        if (--msgChoiceTimer == 0) {
            msgChoiceCloseByState(&msgChoice);
        }
    }
}


/* Close the window, the choice and the panel. */
void msgClose(void) {
    if ((msgWindowCurrentStatus(&msgWindow) != 2) && (msgSlotTestCurrent(2) != 0)) {
        msgWindowActivateTalk();
    }
    msgWindowRequestFlush(&msgWindow);
    msgQueueEntry(&msgChoice);
    msgQueueEntry(&msgPanel);
}


/* Begin a message: reset the window and the choice, open slot 0. */
void msgBegin(void) {
    msgWindowCloseAll(&msgWindow);
    msgChoiceCloseByState(&msgChoice);
    msgSlotIndex = -1;
    msgWindowOpenNextTextSlot(&msgWindow);
    msgSlotIndex += 1;
    msgGameFlagSet();
}

void msgWindowCloseForTalk(void) {
    msgWindowCloseAll(&msgWindow);
    msgSlotIndex = -1;
}

/* Clear the choice and stop its timer. */
void msgChoiceClear(void) {
    msgChoiceCloseByState(&msgChoice);
    msgChoiceTimer = 0;
}


/* Set slot `slot` (1 to 10) of the current message record to `value`. */
void msgWindowSetSlot(int slot, int value) {
    msgWindowSetCurrentSlot(&msgWindow, slot, value);
}


/* Set the text of the current message record. */
void msgWindowSetScriptText(const char *text) {
    msgWindowFeedCurrentText(&msgWindow, text);
}


/* Open the next window record with the text pointer `text` and raise the game flag. */
void msgWindowOpenText(char *text) {
    msgWindowOpenNextSlot(&msgWindow);
    msgWindowSetCurrentText(&msgWindow, text);
    msgGameFlagSet();
}


u32 msgWindowInUse(void) {
    if (msgWindowBusy() != 0) {
        return 1U;
    }
    return (u32) ~msgWindowGetFirstWord(&msgWindow) >> 0x1F;
}

void msgWindowOpenNext(void) {
    msgWindowOpenNextSlot(&msgWindow);
}

/* Set the window's value. */
void msgSetCurrentText(char *text) {
    msgWindowSetCurrentText(&msgWindow, text);
}


/* Poll the window: when it has finished and is ready, return the selected
 * index; CIRCLE (0x20) resets the window, UP and DOWN move the
 * selection by -1 and +1. Returns -1 otherwise. */
int msgPollSelect(void) {
    int selected;
    PadRecord *pad;

    if (msgWindowTestCurrentFlags(&msgWindow, 0x14) == 0) {
        if ((msgWindowTestCurrentFlags(&msgWindow, 8) == 0) && (msgWindowTestCurrentFlags(&msgWindow, 2) != 0)) {
            selected = msgWindowGetCurrentItemIndex(&msgWindow);
            msgGameFlagClearUnlessKept();
            return selected;
        }
        pad = PadData;
        if (pad->pressed & PAD_CIRCLE) {
            msgWindowCloseAll(&msgWindow);
        }
        if (pad->directionRepeated & PAD_UP) {
            msgWindowMoveSelection(&msgWindow, -1);
        }
        if (pad->directionRepeated & PAD_DOWN) {
            msgWindowMoveSelection(&msgWindow, 1);
        }
    }
    return -1;
}


/* Show the choice for the item name; its timer runs from 0x5A. */
void msgChoiceShow(char *name) {
    msgChoiceTimer = 0x5A;
    msgChoiceShowSingle(&msgChoice, name);
    msgSlotPlaceByState(&msgChoice, 0xA, 0x20);
}


/* Show the choice for the item name and count; its timer runs from 0x5A. */
void msgChoiceShowPair(char *name, int count) {
    msgChoiceTimer = 0x5A;
    msgItemSlotStart(&msgChoice, name, count);
    msgSlotPlaceByState(&msgChoice, 0xA, 0x20);
}

/* Start the choice's timeline at `frame`, then the second step with `second`. */
void msgChoiceOpen(int frame, int second) {
    timecodeStartAtFrame(&msgChoice, frame);
    msgChoiceSetIndex(&msgChoice, second);
}


/* Polls the open choice: CIRCLE closes it and returns its frame count; left/right and up/down move the
 * selection. -1 while no choice is open or while one of the choice bits 0x300 (not recovered) is set. */
int msgChoicePoll(void)
{
    int frames;

    if (msgChoiceTestFlags(&msgChoice.flags, MSG_CHOICE_OPEN) == 0) {
        return -1;
    }
    if (msgChoiceTestFlags(&msgChoice.flags, 0x300) != 0) {
        return -1;
    }
    if (PadData[0].pressed & PAD_CIRCLE) {
        frames = msgChoiceGetFrames(&msgChoice);
        msgChoiceCloseByState(&msgChoice);
        msgWindowCloseAll(&msgWindow);
        msgGameFlagClear();
        return frames;
    }
    if (PadData[0].directionRepeated & PAD_LEFT) {
        msgChoiceStepSelection(&msgChoice, 1);
    }
    if (PadData[0].directionRepeated & PAD_RIGHT) {
        msgChoiceStepSelection(&msgChoice, -1);
    }
    if (PadData[0].directionRepeated & PAD_UP) {
        msgChoiceStepValue(&msgChoice, 1);
    }
    if (PadData[0].directionRepeated & PAD_DOWN) {
        msgChoiceStepValue(&msgChoice, -1);
    }
    return -1;
}


/* Open the panel, then show its default entry. */
void msgPanelOpen(void) {
    timecodeStartAtHms(&msgPanel, 0, 0, 0);
    msgPanelShowDefault();
}

/* Set the panel's timecode: hours, minutes, seconds and frames. */
void msgPanelSetTimecode(int hours, int minutes, int seconds, int frames) {
    timecodeSetHmsf(&msgPanel, hours, minutes, seconds, frames);
}


/* Show the panel entry 0x151 in mode 8. */
void msgPanelShowDefault(void) {
    msgPanelShow(0x151, 8);
}

/* Show a panel entry with `first` and `second`. */
void msgPanelShow(int first, int second) {
    msgSlotPlaceByState(&msgPanel, first, second);
}

/* Clear the panel. */
void msgPanelClear(void) {
    msgChoiceCloseByState(&msgPanel);
}

int msgSlotIndex = 0;
int msgChoiceTimer = 0;
