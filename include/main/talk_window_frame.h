#ifndef INCLUDE_MAIN_TALK_WINDOW_FRAME_H
#define INCLUDE_MAIN_TALK_WINDOW_FRAME_H

/* Functions of src/main/talk_window_frame.c that other files call. */

#include "main/talk_script.h"

void talkWindowFrameRect(int *left, int *top, int *bottom, int *right, MsgSlot *window, int mode, float scale);
unsigned int talkColorFadeAlpha(unsigned int color, float scale);
void talkWindowCursorPos(int *x, int *y, MsgSlot *window);

#endif /* INCLUDE_MAIN_TALK_WINDOW_FRAME_H */
