#include "common.h"
#include "main/msg_window_slot.h"
#include "main/msg_slot_core.h"
#include "main/talk_scan.h"
#include "main/interp_text.h"

/* int keeps the result in v0 for msgWindowSetCurrentSlot below; a void prototype does not compile there ("void value not
   ignored"), so interp_text.h does not declare it. The definition cannot be int either: interp_text.c then gains
   an instruction (@sizes .text DIFF, checked 2026-11-20), since the original leaves v0 as a by-product of the test. */
/* Sets text argument `first` (1 to 10) of the current slot to the value `second`; without a current slot (index -1)
 * it returns the byte offset of that index, the product the compiler computes before the test. */
int msgWindowSetCurrentSlot(MsgWindow *window, int first, int second) {
    int current;
    int offset;
    MsgSlot *slot;

    current = window->current_index;
    offset = current * sizeof(MsgSlot);
    slot = &window->slots[current];
    if (current >= 0) {
        return interpSetTextArg(slot, first, second);
    }
    return offset;
}

/* Sets the speaker name of the current text slot, if any. */
void msgWindowSetCurrentFirstSlot(MsgWindow *window, const char *name) {
    int current;
    MsgSlot *slot;

    current = window->current_index;
    slot = &window->slots[current]; /* address formed before the current >= 0 test, as in the original */
    if (current >= 0) {
        interpSetSpeakerName(slot, name);
    }
}

/* Close step of the message window, called from msgQueueClose (main/msg_state.c). */
void msgWindowActivateTalk(void) {
    talkActivate();
}
