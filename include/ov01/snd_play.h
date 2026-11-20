#ifndef INCLUDE_OV01_SND_PLAY_H
#define INCLUDE_OV01_SND_PLAY_H

/* Functions of src/ov01/snd_play.c that other files call. */

#include "ov01/unit_cmd.h"

void seObjectVoiceStart(BattleUnit *object, int code, int volume);
void seObjectVoiceStartDefault(BattleUnit *object, int code);
void streamSlotSetFile(int slot, const char *file);
void streamSlotStop(int index);
void bgmPlayTrack(int track);
void streamPlayByKey(struct BattleUnit *unit, int mode);
/* Starts the stream of the unit's action (the technique's gateFlag picks it). */
void unitPlayActionSound(struct BattleUnit *unit, int action);

#endif /* INCLUDE_OV01_SND_PLAY_H */
