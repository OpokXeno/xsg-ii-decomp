#ifndef INCLUDE_MAIN_TALK_WINDOW_CTRL_H
#define INCLUDE_MAIN_TALK_WINDOW_CTRL_H

struct SbThread;

/* Script natives of src/main/talk_window_ctrl.c (talk module, native table talkNativeTable). */

int talkWinClose(void);
int talkResume(void);
int talkEnableFlags(void);
int talkDisableFlags(void);
int talkIsDispMsg(void);
int talkWait(struct SbThread *unused_thread, int *out);

#endif /* INCLUDE_MAIN_TALK_WINDOW_CTRL_H */
