#include "common.h"
#include "main/sound_driver.h"
#include "main/yajima_test.h"
#include "ov01/data_file_load.h"
#include "ov01/unit_cmd.h"
#include "ov01/handle_state.h"
#include "ov01/snd_play.h"
#include "main/se_bank_load.h"
#include "ov01/data_record_get.h"

extern unsigned char seDefaultVolume[];

#include "shared.h"

/* An SE voice id is the owner's handle in the top byte, then the code (as in handle_state.c). */
/* Pan passed to seVoiceStart: 0x40, the middle of the pan range. */
#define SE_PAN_CENTER 0x40
/* Background music tracks run from 1 to BGM_TRACK_COUNT. */
#define BGM_TRACK_COUNT 6

/* Starts the object's voice for `code`; a negative volume selects the default SE volume. */
void seObjectVoiceStart(BattleUnit *object, int code, int volume)
{
    int voice;

    voice = (object->stats->handle << SE_VOICE_TAG_SHIFT) + code;
    if (volume < 0) {
        volume = seDefaultVolume[0];
    }
    seVoiceStart(voice, volume, SE_PAN_CENTER);
}

/* Starts the object's voice for `code` at the default SE volume (volume -1). */
void seObjectVoiceStartDefault(BattleUnit *object, int code)
{
    seObjectVoiceStart(object, code, -1);
}


/* Sets the file of stream slot `slot` to `handle`. */
void streamSlotSetFile(int slot, const char *file)
{
    adxSetSlotFile(slot, file);
}

/* Stops a stream slot (bgmStop passes 0). The original forwards its argument to soundSlotStop. */
void streamSlotStop(int index)
{
    soundSlotStop(index);
}

extern char *bgmTrackHandles[BGM_TRACK_COUNT];

/* Plays background music track `track` (1 to BGM_TRACK_COUNT) on stream slot 0, from its handle in bgmTrackHandles. */
void bgmPlayTrack(int track)
{
    if ((unsigned int) (track - 1) < BGM_TRACK_COUNT) {
        soundSlotStop(0);
        bgmSlotLevelSet(0);
        streamSlotSetFile(0, bgmTrackHandles[track - 1]);
    }
}


/* Plays the sound for `code`: the key path (state 0, 6 to 8, 10) looks up the object's table key row and
   plays the voice at `code - keyRowValue + 17`; the other states open the battle stream. */
void unitPlayActionSound(BattleUnit *object, int code)
{
    TecRecord *record;
    int keyRowValue;
    char *handle;
    int i;

    record = tecRecordGet(code);
    switch (record->techClass) {
    case 0:
    case 6:
    case 7:
    case 8:
    case 10:
        keyRowValue = 0;
        if (object->stats->key == 3) {
            keyRowValue = 11;
        } else {
            if (object->stats->key == 6) {
                keyRowValue = 27;
            } else {
                for (i = 0; i < 10; i++) {
                    keyRowValue = tableKeyRowGet(object->stats->key)->entries[i];
                    if (keyRowValue != 0) {
                        break;
                    }
                }
            }
        }
        if (keyRowValue != 0) {
            seObjectVoiceStartDefault(object, code - keyRowValue + 17);
        }
        keyRowValue = 0;
        break;
    default:
        handle = dataBattleCodePathGet(object, code);
        if (handle != 0) {
            soundSlotReopen(1);
            actionSoundSlotLevelSet(1);
            streamSlotSetFile(1, handle);
        }
        handle = 0;
        break;
    }
}

/* Plays the battle action stream of the unit's key in mode (dataBattleActionPathGet) on slot 1. */
void streamPlayByKey(BattleUnit *unit, int mode)
{
    char *path;

    path = dataBattleActionPathGet(unit->stats->key, mode);
    if (path != 0) {
        soundSlotReopen(1);
        streamKeySlotLevelSet(1);
        streamSlotSetFile(1, path);
    }
}

/* Music track names of menu_wep_sp_panel.c's rodata, indexed by track (track 0 is unused). */
extern char bgmFanfareName[];
extern char bgmLastBossName[];
extern char bgmBoss02Name[];
extern char bgmBoss01Name[];
extern char bgmBattle02Name[];
extern char bgmBattle01Name[];

char *bgmTrackHandles[BGM_TRACK_COUNT] = {
    bgmBattle01Name,
    bgmBattle02Name,
    bgmBoss01Name,
    bgmBoss02Name,
    bgmLastBossName,
    bgmFanfareName,
};
