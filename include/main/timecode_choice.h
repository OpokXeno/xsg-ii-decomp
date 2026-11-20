#ifndef INCLUDE_MAIN_TIMECODE_CHOICE_H
#define INCLUDE_MAIN_TIMECODE_CHOICE_H

#include "main/msg_slot_core.h"

/* Functions of src/main/timecode_choice.c that other files call. */

/* Starts a 4-digit number entry in a timecode slot at a frame count (30 per second); returns 0 while the slot is active. */
int timecodeStartAtFrame(MsgSlot *state, int frame);
/* Stores the choice index (forwards to timecodeSetDigits). */
void msgChoiceSetIndex(MsgSlot *slot, int index);
long long msgChoiceGetFrames(MsgSlot *choice);
void msgChoiceStepSelection(MsgSlot *choice, int step);
void msgChoiceStepValue(MsgSlot *choice, int step);

#endif /* INCLUDE_MAIN_TIMECODE_CHOICE_H */
