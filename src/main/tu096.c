#include "common.h"
#include "shared.h"
#include "main/tu096.h"
#include "main/tu097.h"

void func_0020CCC0(int *left, int *right) {
    int leftValue = *left;

    *left = *right;
    *right = leftValue;
}

/* 16-bit tick counter (D_0069D498): zeroed, read and incremented by the functions below. */
extern int xglTickCounter;

/* Zeroes the tick counter; called from the init sequence menuSceneReset (main/tu100). */
void xglTickReset(void) {
    xglTickCounter = 0;
}

/* Returns the tick counter; six callers, among them menuDrawSelectArrows (main/tu101). */
int xglTickGet(void) {
    return xglTickCounter;
}

/* Adds one to the tick counter, wrapping at 16 bits. */
void xglTickIncrement(void) {
    xglTickCounter = (xglTickCounter + 1) & 0xFFFF;
}

float xglSin(float angle);
/* xglPi is the float pi (3.1415927). */
extern float xglPi;

/* sin(tick * pi / divisor) * amplitude, truncated; textDrawSegment (main/tu101) passes 32.0 and 24.0. */
int xglTickWave(float divisor, float amplitude) {
    return (int) (xglSin(((float) xglTickCounter * xglPi) / divisor) * amplitude);
}

/* The speed grows by this much each tick while the track moves, up to the maximum. */
#define FONT_TRACK_SPEED_STEP 24
#define FONT_TRACK_SPEED_MAX 64

/*
 * Moves a track one tick toward its target pair: each axis closes a speed/128
 * part of its distance in 1/16 units, and the speed grows. At the target the
 * speed word counts down and is reset to 1 when it runs out.
 */
void fontTrackEase(struct FontTrack *track) {
    int target_scaled;
    int delta;

    if (fontTrackAtTarget(track) != 0) {
        track->step_count = track->step_count - 1;
        if (track->step_count > 0) {
            return;
        }
        track->step_count = 1;
        return;
    }

    target_scaled = track->target_pair.x << 4;
    delta = target_scaled - track->scaled_pair.x;
    if ((delta >> 4) != 0) {
        track->scaled_pair.x = track->scaled_pair.x + ((delta * track->step_count) >> 7);
        track->current_pair.x = track->scaled_pair.x >> 4;
    } else {
        track->current_pair.x = track->target_pair.x;
        track->scaled_pair.x = target_scaled;
    }

    target_scaled = track->target_pair.y << 4;
    delta = target_scaled - track->scaled_pair.y;
    if ((delta >> 4) != 0) {
        track->scaled_pair.y = track->scaled_pair.y + ((delta * track->step_count) >> 7);
        track->current_pair.y = track->scaled_pair.y >> 4;
    } else {
        track->current_pair.y = track->target_pair.y;
        track->scaled_pair.y = target_scaled;
    }

    track->step_count = track->step_count + FONT_TRACK_SPEED_STEP;
    if (track->step_count > FONT_TRACK_SPEED_MAX) {
        track->step_count = FONT_TRACK_SPEED_MAX;
    }
}

/*
 * Stores the target pair of a track. fontTrackEase moves the current pair
 * toward it and fontTrackAtTarget tests for arrival.
 */
void fontTrackSetTarget(struct FontTrack *track, int targetX, int targetY) {
    track->target_pair.x = targetX;
    track->target_pair.y = targetY;
}
