#ifndef INCLUDE_MAIN_TALK_SPEAKER_NAME_H
#define INCLUDE_MAIN_TALK_SPEAKER_NAME_H

#include "main/timecode_counter.h"

/* Functions of src/main/talk_speaker_name.c that other files call. */

void talkStashPrefix(MsgSlot *box);
void talkRestorePrefix(MsgSlot *box);
void talkCopySpeakerName(unsigned char *buffer, MsgSlot *box);

#endif /* INCLUDE_MAIN_TALK_SPEAKER_NAME_H */
