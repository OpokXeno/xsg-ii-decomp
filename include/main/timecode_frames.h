#ifndef INCLUDE_MAIN_TIMECODE_FRAMES_H
#define INCLUDE_MAIN_TIMECODE_FRAMES_H

#include "main/timecode_counter.h"

/* Functions of src/main/timecode_frames.c that other files call. */

/* The frame count of a timecode slot (30 per second), 0 unless the slot is a timecode slot. */
long long timecodeGetFrames(MsgSlot *slot);
/* Moves the selected digit or field of a timecode slot by delta, wrapped into the range of its mode. */
void timecodeStepPhase(MsgSlot *slot, int delta);
/* Starts a timeline on a timecode slot (flag 0x200 and roll angle setup, then by mode). */
void timecodeStart(MsgSlot *slot, int param);
void timecodeSetFrames(MsgSlot *slot, long long frames);
void timecodeSetPhase(MsgSlot *slot, int phase);
/* Splits the frame count of a timecode slot into hours, minutes, seconds and frames. */
void timecodeSplit(MsgSlot *slot, int *hours, int *minutes, int *seconds, int *frames);

#endif /* INCLUDE_MAIN_TIMECODE_FRAMES_H */
