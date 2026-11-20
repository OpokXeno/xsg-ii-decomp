#ifndef INCLUDE_MAIN_MSG_WINDOW_CLOSE_H
#define INCLUDE_MAIN_MSG_WINDOW_CLOSE_H

#include "main/timecode_counter.h"

/* Functions of src/main/msg_window_close.c that other files call. */

int msgWindowGetFirstWord(MsgWindow *window);
void msgWindowCloseCurrent(MsgWindow *window);
void msgWindowFindLastActive(MsgWindow *window);
void msgWindowCompact(MsgWindow *window);
void msgWindowStartClosingSlot(MsgWindow *window, int index);
void msgWindowCloseAll(MsgWindow *window);

#endif /* INCLUDE_MAIN_MSG_WINDOW_CLOSE_H */
