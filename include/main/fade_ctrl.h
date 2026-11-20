#ifndef INCLUDE_MAIN_FADE_CTRL_H
#define INCLUDE_MAIN_FADE_CTRL_H

/* Functions of src/main/fade_ctrl.c that other files call. */

void fadeReset(void);
void fadeClear(void);
void fadeUpdate(void);
void fadeSetColor(float red, float green, float blue);
void fadeStartIn(int frames);
void fadeStartOut(int frames);
void fadeStartColor(int frames, float red, float green, float blue);
void fadeStartColorAlpha(int frames, float red, float green, float blue, float alpha);

#endif /* INCLUDE_MAIN_FADE_CTRL_H */
