/*
 * OV01 original TU 71: 0x00aa90c8..0x00aa9340 (7 functions)
 */
#include "common.h"
#include "main/tu022.h"
#include "main/tu063.h"
#include "ov01/tu008.h"
#include "ov01/tu012.h"
#include "ov01/tu070.h"
#include "ov01/tu071.h"
#include "main/tu021.h"
#include "ov01/tu012.h"
#include "ov01/tu042.h"

extern unsigned char seDefaultVolume[];

#include "shared.h"

/* An SE voice id is the owner's handle in the top byte, then the code (as in tu070). */
/* Pan passed to seVoiceStart: 0x40, the middle of the pan range. */
#define SE_PAN_CENTER 0x40
/* Background music tracks run from 1 to BGM_TRACK_COUNT. */
#define BGM_TRACK_COUNT 6

/* Starts the object's voice for `code`; a negative volume selects the default SE volume. */
void seObjectVoiceStart(Ov01Object *object, int code, int volume)
{
    int voice;

    voice = (object->stats->handle << SE_VOICE_TAG_SHIFT) + code;
    if (volume < 0) {
        volume = seDefaultVolume[0];
    }
    seVoiceStart(voice, volume, SE_PAN_CENTER);
}

/* Starts the object's voice for `code` at the default SE volume (volume -1). */
void seObjectVoiceStartDefault(Ov01Object *object, int code)
{
    seObjectVoiceStart(object, code, -1);
}

void adxSetSlotFile(int slot, const char *file);

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

void soundSlotStop(int index);
void func_001EDC20(int index);
extern char *bgmTrackHandles[BGM_TRACK_COUNT];

/* Plays background music track `track` (1 to BGM_TRACK_COUNT) on stream slot 0, from its handle in bgmTrackHandles. */
void bgmPlayTrack(int track)
{
    if ((unsigned int) (track - 1) < BGM_TRACK_COUNT) {
        soundSlotStop(0);
        func_001EDC20(0);
        streamSlotSetFile(0, bgmTrackHandles[track - 1]);
    }
}

/* The record that tecRecordGet returns for a code; only the state byte is read. */
void func_001EDC40(int index);
/* tu008.c defines it on its own view of the unit (struct DataBattleOwner). */
char *dataBattleCodePathGet(Ov01Object *object, int code);

/* Plays the sound for `code`: the key path (state 0, 6 to 8, 10) looks up the object's table key row and
   plays the voice at `code - keyRowValue + 17`; the other states open the battle stream. */
void unitPlayActionSound(Ov01Object *object, int code)
{
    TecRecord *record;
    int keyRowValue;
    char *handle;
    int i;

    record = tecRecordGet(code);
    switch (record->gateFlag) {
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
                    keyRowValue = *(short *) ((char *) tableKeyRowGet(object->stats->key) + (i << 1));
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
            func_001EDC40(1);
            streamSlotSetFile(1, handle);
        }
        handle = 0;
        break;
    }
}

/* Plays the battle action stream of the unit's key in mode (dataBattleActionPathGet) on slot 1. */
void streamPlayByKey(Ov01Object *unit, int mode)
{
    char *path;

    path = dataBattleActionPathGet(unit->stats->key, mode);
    if (path != 0) {
        soundSlotReopen(1);
        func_001EDC60(1);
        streamSlotSetFile(1, path);
    }
}

/* Music track names of tu056's rodata, indexed by track (track 0 is unused). */
extern char D_00ABAD08[];
extern char D_00ABAD18[];
extern char D_00ABAD28[];
extern char D_00ABAD30[];
extern char D_00ABAD38[];
extern char D_00ABAD48[];

char *bgmTrackHandles[BGM_TRACK_COUNT] = {
    D_00ABAD48,
    D_00ABAD38,
    D_00ABAD30,
    D_00ABAD28,
    D_00ABAD18,
    D_00ABAD08,
};
