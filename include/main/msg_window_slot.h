#ifndef INCLUDE_MAIN_MSG_WINDOW_SLOT_H
#define INCLUDE_MAIN_MSG_WINDOW_SLOT_H

#include "main/msg_slot_core.h"

/* Functions of src/main/msg_window_slot.c that other files call. */

void msgWindowActivateTalk(void);
int msgWindowSetCurrentSlot(MsgWindow *window, int first, int second);

#endif /* INCLUDE_MAIN_MSG_WINDOW_SLOT_H */
