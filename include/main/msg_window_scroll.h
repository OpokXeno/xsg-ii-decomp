#ifndef INCLUDE_MAIN_MSG_WINDOW_SCROLL_H
#define INCLUDE_MAIN_MSG_WINDOW_SCROLL_H

#include "main/msg_slot_core.h"

/* Functions of src/main/msg_window_scroll.c that other files call. */

void msgWindowStartScan(MsgWindow *window);
void msgWindowSeekToWindowEnd(MsgWindow *window);
int msgWindowGetCurrentItemIndex(MsgWindow *window);
void msgWindowMoveSelection(MsgWindow *window, int value);
int msgWindowTestCurrentFlags(MsgWindow *window, int mask);
int msgWindowCurrentStatus(MsgWindow *window);
void msgEntryClear(MsgSlot *entry);
void msgQueueEntry(MsgSlot *slot);

#endif /* INCLUDE_MAIN_MSG_WINDOW_SCROLL_H */
