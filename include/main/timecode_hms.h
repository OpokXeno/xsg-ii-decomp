#ifndef INCLUDE_MAIN_TIMECODE_HMS_H
#define INCLUDE_MAIN_TIMECODE_HMS_H

#include "main/timecode_counter.h"

/* Functions of src/main/timecode_hms.c that other files call. */


/* Starts the timeline at hours/minutes/seconds; returns 0 while the slot is in use or cannot start, 1 otherwise. */
int timecodeStartAtHms(MsgSlot *state, int hours, int minutes, int seconds);
/* Sets the frame count from hours/minutes/seconds/frames. */
void timecodeSetHmsf(MsgSlot *state, int hours, int minutes, int seconds, int frames);
/* Forwards its arguments to timecodeSetMode. */
void timecodeSetModeForward(MsgSlot *timecode, int mode);
/* Stores two frame counts computed from hours/minutes/seconds triples. */
void timecodeSetFramePairFromHms(MsgSlot *state, int hours, int minutes, int seconds, int hours60, int minutes60, int seconds60);

#endif /* INCLUDE_MAIN_TIMECODE_HMS_H */
