#ifndef INCLUDE_MAIN_MSG_WINDOW_MOVE_H
#define INCLUDE_MAIN_MSG_WINDOW_MOVE_H

/* Functions of src/main/msg_window_move.c. */

#include "main/timecode_counter.h"

void msgWindowMoveCurrent(MsgWindow *window, int x, int y);
void msgWindowResizeCurrent(MsgWindow *window, int x, int y);
void msgWindowResizeCurrentCells(MsgWindow *window, int x, int y);

#endif /* INCLUDE_MAIN_MSG_WINDOW_MOVE_H */
