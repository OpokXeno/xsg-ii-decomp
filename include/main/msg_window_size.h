#ifndef INCLUDE_MAIN_MSG_WINDOW_SIZE_H
#define INCLUDE_MAIN_MSG_WINDOW_SIZE_H

/* Functions of src/main/msg_window_size.c that other files call. */

#include "main/msg_window_layout.h"

#define MSG_WINDOW_OVERFLOW_LINES 6    /* text line count from which msgWindowOverflows reports an overflow */

int msgWindowOverflows(MsgSlot *window);
void msgWindowSetEmptyLayout(MsgSlot *window);
int msgWindowRowPixels(int rows);

#endif /* INCLUDE_MAIN_MSG_WINDOW_SIZE_H */
