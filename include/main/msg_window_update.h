#ifndef INCLUDE_MAIN_MSG_WINDOW_UPDATE_H
#define INCLUDE_MAIN_MSG_WINDOW_UPDATE_H

/* Functions of src/main/msg_window_update.c that other files call. */

struct MsgSlot;

/* Each function takes a list window slot (MsgSlot of msg_slot_core.h, slot type 2). */
int msgWindowUpdate(struct MsgSlot *window);   /* int only for the original's codegen; the result is never set */
void msgWindowPulseStep(struct MsgSlot *window);
void msgWindowDraw(struct MsgSlot *window);
void msgWindowClose(struct MsgSlot *window);
int msgWindowGetItemIndex(struct MsgSlot *window);
int msgWindowScrollBy(struct MsgSlot *window, int delta);
int msgWindowSlideStep(struct MsgSlot *window);
int msgWindowSweepStep(struct MsgSlot *window);
int msgWindowSweepValue(struct MsgSlot *window);
void msgWindowCountItems(struct MsgSlot *window);
void msgWindowUpdateList(struct MsgSlot *window);
void msgWindowSkipEmptyItem(struct MsgSlot *window);
int msgWindowNearestItemOffset(int *unused_table, int index);
int msgWindowNearestFilledIndex(int *counts, int index);
int *msgWindowFindFilledEntry(int *from, int *bound);
void msgWindowSetItemIndex(struct MsgSlot *window, int index);
int msgWindowGetScroll(struct MsgSlot *window);
int msgWindowGetScrollEnd(struct MsgSlot *window);
int msgWindowGetVisibleRows(struct MsgSlot *window);
int msgWindowMoveCursor(struct MsgSlot *window, int delta);
void msgWindowStepCursor(struct MsgSlot *window, int delta);
void msgWindowStepCursorSkipEmpty(struct MsgSlot *window, int delta);

#endif /* INCLUDE_MAIN_MSG_WINDOW_UPDATE_H */
