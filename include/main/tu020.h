#ifndef INCLUDE_MAIN_TU020_H
#define INCLUDE_MAIN_TU020_H

/* Functions of src/main/tu020.c that other files call. */

/* Sound levels run from 0 to SOUND_LEVEL_MAX. */
#define SOUND_LEVEL_MAX 0x7F

/* A volume ramp: the current level moves toward the target by step on each
 * call to seVolumeRampStep. */
typedef struct VolumeRamp {
    signed char step;             /* +0x00: added to current on each call; cleared when the ramp reaches target */
    unsigned char set_level;      /* +0x01: a sound slot's level as soundSlotSetLevel last set it */
    unsigned char target;         /* +0x02: level the ramp moves toward */
    unsigned char current;        /* +0x03: level sent to the sound driver */
} VolumeRamp;

int seVolumeCurve(int level);
int seVolumeRampSet(VolumeRamp *ramp, int level, int step);
int seVolumeRampStep(VolumeRamp *ramp);

#endif /* INCLUDE_MAIN_TU020_H */
