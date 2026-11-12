#include "common.h"

/* Volume curve: one point per 4 levels; levels between points are
 * interpolated, levels from 0x7F up give 0. */
#define SE_VOLUME_LEVELS 0x7f

extern short seVolumeCurveTable[];

int seVolumeCurve(int level)
{
    int fraction = level & 3;
    int value;

    if (level < SE_VOLUME_LEVELS && fraction != 0) {
        int point = seVolumeCurveTable[level >> 2];
        int next = seVolumeCurveTable[(level + 4) >> 2];

        value = point + (((next - point) * fraction) >> 2);
    } else {
        value = seVolumeCurveTable[level >> 2];
        if (level >= SE_VOLUME_LEVELS) {
            value = 0;
        }
    }
    return value;
}

INCLUDE_ASM("asm/main/nonmatchings/tu020", seVolumeRampSet);

/* A volume ramp: the current level moves toward the target by step on each
 * call to seVolumeRampStep. Levels are 0..0x7F. */
typedef struct VolumeRamp {
    signed char step;             /* +0x00: added to current on each call; cleared when the ramp reaches target */
    unsigned char unmodeled_01;
    unsigned char target;         /* +0x02: level the ramp moves toward */
    unsigned char current;        /* +0x03: level sent to the sound driver */
} VolumeRamp;

int seVolumeRampStep(VolumeRamp *ramp)
{
    int step;
    int next_value;

    step = ramp->step;
    if (step != 0) {
        next_value = ramp->current;
        next_value += step;
        if ((step < 0 && next_value < ramp->target) || (step > 0 && next_value > ramp->target)
            || next_value < 0 || next_value >= 0x80) {
            next_value = ramp->target;
            ramp->step = 0;
        }
        ramp->current = next_value;
        return next_value;
    }
    return -1;
}
