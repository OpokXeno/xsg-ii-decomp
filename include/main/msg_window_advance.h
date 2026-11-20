#ifndef INCLUDE_MAIN_MSG_WINDOW_ADVANCE_H
#define INCLUDE_MAIN_MSG_WINDOW_ADVANCE_H

#include "main/timecode_counter.h"

/* Functions of src/main/msg_window_advance.c that other files call. */

int msgWindowAdvanceCurrent(MsgWindow *window, int value);
int msgWindowPollMoreText(MsgWindow *window);

#endif /* INCLUDE_MAIN_MSG_WINDOW_ADVANCE_H */
