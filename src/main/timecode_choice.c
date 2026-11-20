#include "common.h"
#include "shared.h"
#include "main/timecode_choice.h"
#include "main/timecode_window.h"
#include "main/timecode_frames.h"
#include "main/msg_slot_center.h"

/* Starts a 4-digit number entry in a timecode slot at a frame count (30 per second); returns 0 while the slot is active. */
int timecodeStartAtFrame(MsgSlot *state, int frame) {
    if (state->flags & MSG_SLOT_ACTIVE) {
        return 0;
    }
    timecodeInit(state);
    timecodeSetGeometry(state, 0xB8, 0xB6, 0x90, 0x54);
    msgSlotSetZ(state, MSG_SLOT_FRONT_Z);
    timecodeSetFrames(state, frame);
    timecodeStartOpening(state);
    timecodeSetMode(state, 0);
    timecodeSetDigits(state, 4);
    timecodeSetEditable(state, 2);
    return 1;
}

void msgChoiceSetIndex(MsgSlot *slot, int index) {
    timecodeSetDigits(slot, index);
}

/* Forwarders of msg_state.c's msgChoice (step is +1 or -1; see timecode_frames.c for the callees). */
void msgChoiceStepSelection(MsgSlot *choice, int step) {
    timecodeStepPhase(choice, step);
}

void msgChoiceStepValue(MsgSlot *choice, int step) {
    timecodeStart(choice, step);
}

long long msgChoiceGetFrames(MsgSlot *choice) {
    return timecodeGetFrames(choice);
}
