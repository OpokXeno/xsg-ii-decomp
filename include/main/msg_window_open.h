#ifndef INCLUDE_MAIN_MSG_WINDOW_OPEN_H
#define INCLUDE_MAIN_MSG_WINDOW_OPEN_H

#include "main/timecode_counter.h"

/* Functions of src/main/msg_window_open.c that other files call. */

int msgWindowOpenNextSlot(MsgWindow *window);
void msgWindowSetCurrentText(MsgWindow *window, char *text);
void msgWindowBindCurrentItems(MsgWindow *window, char **source, int *costs, int *counts, int *debt);

#endif /* INCLUDE_MAIN_MSG_WINDOW_OPEN_H */
