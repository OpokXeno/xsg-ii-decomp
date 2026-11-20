#ifndef INCLUDE_MAIN_MSG_WINDOW_TEXT_H
#define INCLUDE_MAIN_MSG_WINDOW_TEXT_H

#include "main/msg_slot_core.h"

/* Functions of src/main/msg_window_text.c that other files call. */

/* Per-slot stored_value of the slots msgWindowOpenNextTextSlot and msgWindowOpenNextSlot open: MSG_SLOT_VALUE_BASE + MSG_SLOT_VALUE_STEP * slot index (meaning unconfirmed). */
#define MSG_SLOT_VALUE_BASE 0x0FFFFFCD
#define MSG_SLOT_VALUE_STEP 10

void msgWindowSetStartValue(int start_value);
int msgWindowOpenNextTextSlot(MsgWindow *window);
void msgWindowFeedCurrentText(MsgWindow *window, const char *text);

#endif /* INCLUDE_MAIN_MSG_WINDOW_TEXT_H */
