#ifndef INCLUDE_MAIN_TEXT_BOX_H
#define INCLUDE_MAIN_TEXT_BOX_H

#include "main/cursor_buffer.h"

/* Functions of src/main/text_box.c that other files call. */

/* Element count of a list buffer: capacity bytes in unit_size-byte elements, rounded up; 0 when either is 0. */
int menuListPageCount(ListCursor *cursor);
CursorTextRecord *cursorBufferGetCurrent(void);
void cursorBufferPush(unsigned char *text);
void cursorBufferDeactivateCurrent(void);
int textBoxAnyActive(void);
int textBoxShowPending(void);

#endif /* INCLUDE_MAIN_TEXT_BOX_H */
