#include "common.h"
#include "shared.h"
#include "main/msg_state.h"
#include "main/cursor_buffer.h"
#include "main/text_box.h"
#include "main/talk_window_ctrl.h"
#include "ov01/sb_vm.h"

/* Script native talk.winClose (table 0x3ec888): closes the message window. */
int talkWinClose(void) {
    msgWindowCloseForTalk();
    return 1;
}

/* Script native talkResume (table 0x3ec890). */
int talkResume(void) {
    CursorTextRecord *record = cursorBufferGetCurrent();

    if (record != 0) {
        cursorBufferTick(&record->second_step);
    }
    return 1;
}

/* Script native objx.enableFlags (table 0x3ec898): the body only returns 1. */
int talkEnableFlags(void) {
    return 1;
}

/* Script native objx.disableFlags (table 0x3ec8a0): the body only returns 1. */
int talkDisableFlags(void) {
    return 1;
}

/* Script native talk.isDispMsg (table 0x3ec8a8): returns whether the message window is in use (a sibling call, so
 * msgWindowInUse's result is the native's). */
int talkIsDispMsg(void) {
    return msgWindowInUse();
}

/* Script native talk.wait (table 0x3ec8b0): *out is set to 1 when msgWindowInUse returns non-zero. */
int talkWait(SbThread *unused_thread, int *out) {
    if (msgWindowInUse() != 0) {
        *out = 1;
    }
    return 1;
}
