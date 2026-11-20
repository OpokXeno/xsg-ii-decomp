#ifndef INCLUDE_MAIN_EFFECT_REQUEST_H
#define INCLUDE_MAIN_EFFECT_REQUEST_H

#include "main/msg_slot_core.h"

/* Functions of src/main/effect_request.c that other files call. */

#define MSG_SLOT_QUEUE_SIZE MSG_SLOT_COUNT /* slots the queue holds, one per window slot */

/* msgFrameHooks (xgl_render.h): slot 0 holds msgWindowFlush while message slots are pending. */
extern MsgWindow *msgPendingWindow;        /* window whose slots msgWindowFlush runs first */
extern MsgSlot *msgSlotQueue[];            /* MSG_SLOT_QUEUE_SIZE slots queued by msgQueueSlot */
extern int msgSlotQueueCount;              /* filled entries of msgSlotQueue */

extern int sellLabelTextOff;               /* nonzero hides the sell list's label texts (msg_text_draw.c) */

void msgWindowReset(MsgWindow *window);
void recordUpdatesHold(void);
void msgWindowRequestFlush(MsgWindow *window);
void msgQueueSlot(MsgSlot *slot);
void msgWindowFlush(void);
/* Sends an effect request to recordSetPreset: kind selects the command, value is its strength, arg is forwarded (callers pass a float converted to unsigned). */
void effectRequestToCommand(int bank, unsigned int kind, unsigned int arg, unsigned int value);

#endif /* INCLUDE_MAIN_EFFECT_REQUEST_H */
