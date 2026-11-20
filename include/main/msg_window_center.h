#ifndef INCLUDE_MAIN_MSG_WINDOW_CENTER_H
#define INCLUDE_MAIN_MSG_WINDOW_CENTER_H

#include "main/msg_slot_core.h"

/* Functions of src/main/msg_window_center.c that other files call. */

/* Centres a list window on the screen. */
void msgWindowCenter(MsgSlot *window);
int msgWindowCenterY(MsgSlot *window);
/* Opening phase of a list window: clears the closing bit and sets the opening bit. */
void msgWindowOpen(MsgSlot *window);

#endif /* INCLUDE_MAIN_MSG_WINDOW_CENTER_H */
