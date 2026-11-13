#ifndef INCLUDE_MAIN_TU022_H
#define INCLUDE_MAIN_TU022_H

#include "main/tu020.h"

/* Functions of src/main/tu022.c that other files call. */

/* One 16-byte sound slot. Driver calls take driver_id; the create call takes the voice's work
 * buffer and its size. */
typedef struct SoundSlot {
    int driver_id;
    struct {
        unsigned char *buffer;        /* the voice's work buffer, carved from WorkEnd */
        int buffer_size;
    } create;                         /* what the create call takes */
    VolumeRamp ramp;                  /* +0x0C */
} SoundSlot;

/* The sound control block: a 16-byte head with the song number, the levels and the master volume
 * ramp, then the two sound slots (the label soundSlots is slots[0]). */
typedef union SoundControl {
    struct {
        unsigned char initialized;    /* +0x00: nonzero once the driver setup has run */
        unsigned char unmodeled_01[2];
        unsigned char song;           /* +0x03: number of the loaded song (seBgmLoad) */
        unsigned char master_volume;  /* +0x04: scales every level */
        unsigned char master_level;   /* +0x05: default master (music) level */
        unsigned char voice_level;    /* +0x06: default voice level */
        unsigned char slot_level;     /* +0x07: default sound slot level */
        unsigned char unmodeled_08[4];
        VolumeRamp master_ramp;       /* +0x0C */
        SoundSlot slots[2];           /* +0x10 */
    };
    /* The head has a slot's layout too; soundSlotReopen indexes the slots as records[index + 1]. */
    SoundSlot records[3];
} SoundControl;

extern SoundControl soundCtrl;
extern SoundSlot soundSlots[];


int func_001A4010(void);
void func_001A4400(int flag);
void soundApplySettings(void);
void soundSetMuted(int muted);
void soundSlotSetLevel(int index, int value);
int soundSlotSetVolume(int index, int level, int extra);
void soundSlotStop(int index);
int soundSlotsRestart(void);
int soundSlotReopen(int index);
void soundSlotsReopen(void);
void soundInitIfNeeded(void);
void soundDriverInit(void);

#endif /* INCLUDE_MAIN_TU022_H */
