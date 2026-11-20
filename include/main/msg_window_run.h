#ifndef INCLUDE_MAIN_MSG_WINDOW_RUN_H
#define INCLUDE_MAIN_MSG_WINDOW_RUN_H

#include "main/timecode_counter.h"

/* Functions of src/main/msg_window_run.c that other files call. */


/* Status codes msgWindowCurrentStatus (msg_window_scroll.c) and msgSlotStatus (timecode_forward.c)
 * report for a slot. */
#define MSG_STATUS_NONE 0
#define MSG_STATUS_TEXT 1
#define MSG_STATUS_WINDOW 2
#define MSG_STATUS_TIMECODE_IDLE 3
#define MSG_STATUS_TIMECODE_RUNNING 4
#define MSG_STATUS_ITEM 5
/* A timecode slot (state 3) reports idle at mode 0 (MsgSlotTimecode.mode) and running for modes 1 up to (not including) this value. */
#define MSG_TIMECODE_SUB_RUNNING_END 3

void msgWindowRunSlots(MsgWindow *window);
void msgSlotRun(MsgSlot *slot);
/* Unloads the botan XTX, then ends the talk script. */
void talkEnd(void);

#endif /* INCLUDE_MAIN_MSG_WINDOW_RUN_H */
