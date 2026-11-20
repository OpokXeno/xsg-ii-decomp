#ifndef INCLUDE_MAIN_TALK_DRAW_FADE_H
#define INCLUDE_MAIN_TALK_DRAW_FADE_H

/* Functions of src/main/talk_draw_fade.c that other files call. */

struct MsgSlot;

void talkDrawFadeRect(float scale, int x0, int y0, int x1, int y1, unsigned int color);
void talkDrawListCursorBar(struct MsgSlot *slot);
void talkDrawSelectionBox(struct MsgSlot *slot);

#endif /* INCLUDE_MAIN_TALK_DRAW_FADE_H */
