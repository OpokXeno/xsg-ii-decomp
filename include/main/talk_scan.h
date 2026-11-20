#ifndef INCLUDE_MAIN_TALK_SCAN_H
#define INCLUDE_MAIN_TALK_SCAN_H

/* Functions of src/main/talk_scan.c that other files call. */

#include "main/timecode_counter.h"

void talkActivate(void);
/* Helpers of a state 1 (text) slot; they do nothing for another state. */
void talkAdvanceCounter(MsgSlot *slot);
int talkHasMoreLines(MsgSlot *slot);
void talkSetScanFlag(MsgSlot *slot);
void talkScrollLines(MsgSlot *slot, int amount);
void talkSeekToLimit(MsgSlot *slot);
void talkSeekToWindowEnd(MsgSlot *slot);
void talkSetSetting(MsgSlot *slot, int width_limit);
const char *textSkipTwoUnits(const char *text);

#endif /* INCLUDE_MAIN_TALK_SCAN_H */
