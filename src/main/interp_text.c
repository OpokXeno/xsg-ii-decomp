#include "common.h"
#include "main/interp_text.h"

/* Sets text argument index (1 to 10) of a state 1 (text) slot. */
void interpSetTextArg(MsgSlot *slot, int index, int value) {
    if (slot->state == MSG_SLOT_STATE_TEXT && (unsigned int) (index - 1) < 10) {
        slot->u.text.args[index - 1] = value;
    }
}

/* Sets the speaker name of a state 1 (text) slot. */
void interpSetSpeakerName(MsgSlot *slot, const char *name) {
    if (slot->state == MSG_SLOT_STATE_TEXT) {
        slot->u.text.speaker_name = name;
    }
}

/* Marks a state 1 (text) slot as opening: clears the closing bit, sets the opening bit. */
void interpSetOpening(MsgSlot *slot) {
    if (slot->state == MSG_SLOT_STATE_TEXT) {
        slot->flags = (slot->flags & ~MSG_SLOT_CLOSING) | MSG_SLOT_OPENING;
    }
}
