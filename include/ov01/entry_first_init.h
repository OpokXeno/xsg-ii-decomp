#ifndef INCLUDE_OV01_ENTRY_FIRST_INIT_H
#define INCLUDE_OV01_ENTRY_FIRST_INIT_H

/* Functions of src/ov01/entry_first_init.c that other files call. */

/* Address of the running screen fade (a task record claimed from ov01TaskTable), 0 when none. Kept as int: a
 * pointer-typed global changes the store order of fadeIn/fadeOut. */
extern int screenFadeTask;

/* Start a fade: alphaStep is added to (fadeIn) or subtracted from (fadeOut) the alpha each frame once frameDelay frames
 * have passed. */
void fadeIn(int alphaStep, int frameDelay);
void fadeOut(int alphaStep, int frameDelay);
int fadeEndChk(void);

#endif /* INCLUDE_OV01_ENTRY_FIRST_INIT_H */
