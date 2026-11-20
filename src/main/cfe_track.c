#include "common.h"
#include "shared.h"
#include "main/cfe_list.h"
#include "main/cfe_track.h"

#include "main/xac_eval.h"
/* Normalizes the 2-D vector (*x, *y) in 8.8 fixed point (a zero length counts as 1e-6). The
 * conversion back takes the whole part of the unscaled component times 1 / length, as the original
 * does. */
void cfeNormalizeFixedVec2(int *x, int *y)
{
    float fx = CFE_FIXED_TO_FLOAT(*x);
    float fy = CFE_FIXED_TO_FLOAT(*y);
    float length = __builtin_sqrtf(fx * fx + fy * fy);

    if (length == 0.0f) {
        length = 1e-06f;
    }
    length = 1.0f / length;
    *x = (int) ((int) fx * length * 256.0f + (int) ((fx * length - (int) fx * length) * 256.0f));
    *y = (int) ((int) fy * length * 256.0f + (int) ((fy * length - (int) fy * length) * 256.0f));
}

/* Normalizes both direction pairs of the key (cfeNormalizeFixedVec2 on each). */
void cfeEntryNormalizeDirs(CfeTrackNode *entry)
{
    cfeNormalizeFixedVec2(&entry->firstDirection[0], &entry->firstDirection[1]);
    cfeNormalizeFixedVec2(&entry->secondDirection[0], &entry->secondDirection[1]);
}

/* Resets a direction pair to (1, 0) when its flag is set: CFE_TRACK_RESET_FIRST, else CFE_TRACK_RESET_SECOND. */
void cfeEntryResetDirs(CfeTrackNode *entry)
{
    if (entry != 0) {
        int flags = entry->flags;

        if (flags & CFE_TRACK_RESET_FIRST) {
            entry->firstDirection[1] = 0;
            entry->firstDirection[0] = 0x100;
            return;
        }
        if (flags & CFE_TRACK_RESET_SECOND) {
            entry->secondDirection[1] = 0;
            entry->secondDirection[0] = 0x100;
        }
    }
}

/* Sets the directions of every key of the track from its neighbours (an integer track, CFE_TRACK_INTEGER, has none):
 * both directions point along the time and value step to the next key (first key), from the previous key
 * (last key) or from the previous to the next key. CFE_TRACK_LINEAR gives a middle key a first direction from the
 * previous key only, and makes it the previous key's second direction (also on the last key). A track of one
 * key gets (1, 0). The reset flags of the three keys are applied after each step. */
void cfeTrackUpdateDirections(CfeTrackNode *track)
{
    CfeTrackNode *prev = 0;
    CfeTrackNode *node = track;
    CfeTrackNode *next;

    if (track != 0 && (track->flags & CFE_TRACK_INTEGER)) {
        return;
    }
    while (node != 0) {
        next = node->next;
        if (prev == 0) {
            if (next == 0) {
                node->secondDirection[0] = 0x100;
                node->secondDirection[1] = 0;
                node->firstDirection[0] = 0x100;
                node->firstDirection[1] = 0;
            } else {
                node->secondDirection[0] = (next->startTime - node->startTime) << 8;
                node->secondDirection[1]
                    = CFE_FLOAT_TO_FIXED(CFE_FIXED_TO_FLOAT(next->value) - CFE_FIXED_TO_FLOAT(node->value));
                node->firstDirection[0] = node->secondDirection[0];
                node->firstDirection[1] = node->secondDirection[1];
                cfeEntryNormalizeDirs(node);
            }
        } else if (next == 0) {
            node->secondDirection[0] = (node->startTime - prev->startTime) << 8;
            node->secondDirection[1]
                = CFE_FLOAT_TO_FIXED(CFE_FIXED_TO_FLOAT(node->value) - CFE_FIXED_TO_FLOAT(prev->value));
            node->firstDirection[0] = node->secondDirection[0];
            node->firstDirection[1] = node->secondDirection[1];
            cfeEntryNormalizeDirs(node);
            if (node->flags & CFE_TRACK_LINEAR) {
                prev->secondDirection[0] = node->firstDirection[0];
                prev->secondDirection[1] = node->firstDirection[1];
            }
        } else {
            node->secondDirection[0] = (next->startTime - prev->startTime) << 8;
            node->secondDirection[1]
                = CFE_FLOAT_TO_FIXED((CFE_FIXED_TO_FLOAT(node->value) - CFE_FIXED_TO_FLOAT(prev->value))
                                     + (CFE_FIXED_TO_FLOAT(next->value) - CFE_FIXED_TO_FLOAT(node->value)));
            node->firstDirection[0] = node->secondDirection[0];
            node->firstDirection[1] = node->secondDirection[1];
            cfeEntryNormalizeDirs(node);
            if (node->flags & CFE_TRACK_LINEAR) {
                node->firstDirection[0] = (node->startTime - prev->startTime) << 8;
                node->firstDirection[1]
                    = CFE_FLOAT_TO_FIXED(CFE_FIXED_TO_FLOAT(node->value) - CFE_FIXED_TO_FLOAT(prev->value));
                cfeEntryNormalizeDirs(node);
                prev->secondDirection[0] = node->firstDirection[0];
                prev->secondDirection[1] = node->firstDirection[1];
            }
        }
        cfeEntryResetDirs(prev);
        cfeEntryResetDirs(node);
        cfeEntryResetDirs(next);
        prev = node;
        node = node->next;
    }
}

#define CFE_PI 3.1415927f
#define CFE_TWO_PI 6.2831855f

/* Makes the angles along the track continuous: each key's angle is moved by whole turns until it is within
 * pi of the angle of the key before it. */
void cfeChainUnwrapAngles(CfeTrackNode *node)
{
    CfeTrackNode *next;
    CfeTrackNode *after;
    float previous;
    float current;
    int whole;

    if (node == 0) {
        return;
    }
    next = node->next;
    if (next == 0) {
        return;
    }
    do {
        current = CFE_FIXED_TO_FLOAT(next->value);
        previous = CFE_FIXED_TO_FLOAT(node->value);
        if (previous < current) {
            while (__builtin_fabsf(current - previous) > CFE_PI) {
                current -= CFE_TWO_PI;
            }
        } else {
            while (__builtin_fabsf(current - previous) > CFE_PI) {
                current += CFE_TWO_PI;
            }
        }
        whole = (int) current;
        next->value = CFE_FIXED_FROM_WHOLE(whole, current);
        node = next;
        after = next->next;
    } while ((next = after) != 0);
}

/* The value at `time` between the keys `from` and `to`, interpolated linearly. */
float cfeTrackLerp(CfeTrackNode *from, CfeTrackNode *to, float time)
{
    float t = (time - (float) from->startTime) / (float) (to->startTime - from->startTime);
    float end = CFE_FIXED_TO_FLOAT(to->value);
    float start = CFE_FIXED_TO_FLOAT(from->value);

    return (end - start) * t + start;
}

extern f32 cfeTimeScale;

/* Cubic in the time elapsed since the key (scaled by cfeTimeScale): coefficients[0..3], Horner form. The time
 * comes first: EABI passes it in $f12 either way, and this order gives the argument setup of its caller
 * cfeTrackBezier. */
f32 cfeEntryEvalCubic(f32 time, CfeTrackNode *entry, f32 *coefficients)
{
    f32 elapsed = (time - (f32) entry->startTime) * cfeTimeScale;

    return (elapsed * ((elapsed * ((elapsed * coefficients[0]) + coefficients[1])) + coefficients[2])) + coefficients[3];
}

/* Fits the cubic through the Bezier points (x[i], y[i]) into coefficients[0..3] (xac_eval.c). */
extern f32 cfeBezierTimeScale;

/* The value at `time` on the Bezier curve between the keys `from` and `to`: from leaves along its second
 * direction, to arrives along its first one (both normalized; a zero length counts as 1e-7). */
float cfeTrackBezier(CfeTrackNode *from, CfeTrackNode *to, float time)
{
    f32 x[4];
    f32 y[4];
    f32 coefficients[4];
    f32 arrive[2];
    f32 leave[2];
    float length;

    leave[0] = CFE_FIXED_TO_FLOAT(from->secondDirection[0]);
    leave[1] = CFE_FIXED_TO_FLOAT(from->secondDirection[1]);
    length = __builtin_sqrtf(leave[0] * leave[0] + leave[1] * leave[1]);
    if (length == 0.0f) {
        length = 1e-07f;
    }
    length = 1.0f / length;
    leave[0] *= length;
    leave[1] *= length;
    arrive[0] = CFE_FIXED_TO_FLOAT(to->firstDirection[0]);
    arrive[1] = CFE_FIXED_TO_FLOAT(to->firstDirection[1]);
    length = __builtin_sqrtf(arrive[0] * arrive[0] + arrive[1] * arrive[1]);
    if (length == 0.0f) {
        length = 1e-07f;
    }
    length = 1.0f / length;
    arrive[0] *= length;
    arrive[1] *= length;
    y[0] = CFE_FIXED_TO_FLOAT(from->value);
    x[0] = from->startTime * cfeBezierTimeScale;
    x[1] = x[0] + leave[0];
    y[1] = y[0] + leave[1];
    x[3] = to->startTime * cfeBezierTimeScale;
    x[2] = x[3] - arrive[0];
    y[3] = CFE_FIXED_TO_FLOAT(to->value);
    y[2] = y[3] - arrive[1];
    xacBuildCubicSegment(coefficients, x, y);
    return cfeEntryEvalCubic(time, from, coefficients);
}

/* The track's value at `time`: 0 without keys, the first key's value before it, the last key's value
 * after the end, and in between the two surrounding keys interpolated (linearly with CFE_TRACK_LINEAR,
 * else by cfeTrackBezier). */
float cfeTrackSample(CfeTrackNode *track, int time)
{
    CfeTrackNode *node = track;
    CfeTrackNode *next;

    if (track == 0) {
        return 0.0f;
    }
    if (time < track->startTime) {
        return CFE_FIXED_TO_FLOAT(track->value);
    }
    while (node->next != 0) {
        next = node->next;
        if (time >= node->startTime && time < next->startTime) {
            if (node->flags & CFE_TRACK_LINEAR) {
                return cfeTrackLerp(node, next, time);
            }
            return cfeTrackBezier(node, next, time);
        }
        node = next;
    }
    return CFE_FIXED_TO_FLOAT(node->value);
}

/* The value of the key in effect at `time` (the last key that starts at or before it, the first key before
 * the start): as a whole number for integer tracks, else from 8.8 fixed point. */
float cfeTrackSampleStep(CfeTrackNode *track, int time)
{
    CfeTrackNode *node = track;
    CfeTrackNode *next;

    if (track->flags & CFE_TRACK_INTEGER) {
        if (time < track->startTime) {
            return (float) track->value;
        }
        while (node->next != 0) {
            next = node->next;
            if (!(time < node->startTime) && time < next->startTime) {
                return (float) node->value;
            }
            node = next;
        }
        return (float) node->value;
    }
    if (time < track->startTime) {
        return CFE_FIXED_TO_FLOAT(track->value);
    }
    while (node->next != 0) {
        next = node->next;
        if (!(time < node->startTime) && time < next->startTime) {
            return CFE_FIXED_TO_FLOAT(node->value);
        }
        node = next;
    }
    return CFE_FIXED_TO_FLOAT(node->value);
}

/* 1 when a key of the track has startTime `time`. */
int cfeTrackHasTime(CfeTrackNode *node, int time)
{
    for (; node != 0; node = node->next) {
        if (node->startTime == time) {
            return 1;
        }
    }
    return 0;
}

/* 1 when any of the `count` tracks has a key at `time`. */
int cfeTrackAnyHasTime(CfeTrackNode **tracks, int count, int time)
{
    int i;

    for (i = 0; i < count; i++) {
        if (cfeTrackHasTime(tracks[i], time)) {
            return 1;
        }
    }
    return 0;
}

/* The key of the track with startTime `time`, or null. */
CfeTrackNode *cfeTrackFindTime(CfeTrackNode *node, int time)
{
    CfeTrackNode *found;
    CfeTrackNode *next;

    found = 0;
    if (node == 0) {
        return 0;
    }
    while (node != 0) {
        next = node->next;
        if (node->startTime == time) {
            found = node;
            break;
        }
        node = next;
    }
    return found;
}

/* The first key when the track starts after `time`, the key exactly at `time`, else the first key
 * after `time`; null when the track ends before it. */
CfeTrackNode *cfeTrackFindAtOrAfter(CfeTrackNode *node, int time)
{
    CfeTrackNode *found;
    CfeTrackNode *next;

    found = 0;
    if (node == 0) {
        return 0;
    }
    if (time < node->startTime) {
        found = node;
    }
    while (node != 0 && found == 0) {
        next = node->next;
        if (node->startTime == time) {
            found = node;
            break;
        }
        if (next != 0 && !(time < node->startTime) && time < next->startTime) {
            found = next;
            break;
        }
        node = next;
    }
    return found;
}

/* The key next to `time` in `direction` (-1 = earlier, any other value = later): before the start the first
 * key; at a key the one after it (later) or before it (earlier); between two keys the later or the earlier
 * one; after the end the last key; null for an empty track. */
CfeTrackNode *cfeTrackFindNeighbor(CfeTrackNode *node, int time, int direction)
{
    CfeTrackNode *found;
    CfeTrackNode *next;

    found = 0;
    if (node == 0) {
        return 0;
    }
    if (direction != -1) {
        direction = 1;
    }
    if (time < node->startTime) {
        found = node;
    }
    while (node != 0 && found == 0) {
        next = node->next;
        if (node->startTime == time) {
            if (direction == 1) {
                found = next;
            } else {
                found = node->prev;
            }
            break;
        }
        if (next != 0) {
            if (!(time < node->startTime) && time < next->startTime) {
                if (direction == 1) {
                    found = next;
                } else {
                    found = node;
                }
                break;
            }
        } else if (node->startTime < time) {
            found = node;
            break;
        }
        node = next;
    }
    return found;
}


/* The nearest key after `time` (direction > 0) or before it (otherwise) over `count` tracks, found
 * per track with cfeTrackFindNeighbor; null when no track has one. */
CfeTrackNode *cfeTracksFindNearest(CfeTrackNode **tracks, int count, int time, int direction)
{
    CfeTrackNode *best = 0;
    CfeTrackNode *node;
    int i;

    for (i = 0; i < count; i++) {
        node = cfeTrackFindNeighbor(tracks[i], time, direction);
        if (node == 0) {
            continue;
        }
        if (best == 0) {
            if (direction > 0) {
                if (time < node->startTime) {
                    best = node;
                }
            } else {
                if (node->startTime < time) {
                    best = node;
                }
            }
        } else if (direction > 0) {
            if (time < node->startTime && node->startTime < best->startTime) {
                best = node;
            }
        } else {
            if (node->startTime < time && best->startTime < node->startTime) {
                best = node;
            }
        }
    }
    return best;
}

/* Number of keys from entry to the end of its track. */
int cfeTrackCount(CfeTrackNode *entry)
{
    CfeTrackNode *cursor = entry;
    int count = 0;

    if (cursor != 0) {
        do {
            cursor = cursor->next;
            count += 1;
        } while (cursor != 0);
    }
    return count;
}

/* Number of keys of the track with lower <= startTime <= upper. */
int cfeTrackCountInRange(CfeTrackNode *node, int lower, int upper)
{
    int count = 0;

    for (; node != 0; node = node->next) {
        if (node->startTime >= lower && node->startTime <= upper) {
            count++;
        }
    }
    return count;
}

/* Number of runs of equal values along the track (1 for a track whose keys all share it, 0 for none). */
int cfeTrackCountValueRuns(CfeTrackNode *node)
{
    int count = 0;
    int value;

    if (node != 0) {
        value = node->value;
        count = 1;
        for (; node != 0; node = node->next) {
            if (node->value != value) {
                count++;
                value = node->value;
            }
        }
    }
    return count;
}

/* Number of runs of equal values among the keys with lower <= startTime <= upper (0 for none). */
int cfeTrackCountValueRunsInRange(CfeTrackNode *node, int lower, int upper)
{
    int count;
    int value;

    if (node == 0) {
        return 0;
    }
    while (node->startTime < lower) {
        node = node->next;
        if (node == 0) {
            break;
        }
    }
    if (node == 0) {
        return 0;
    }
    value = node->value;
    count = 1;
    while (node != 0) {
        if (node->value != value) {
            count++;
            value = node->value;
        }
        node = node->next;
        if (node == 0 || node->startTime > upper) {
            break;
        }
    }
    return count;
}

/* Moves every key from the first one at or after `time` by `delta`, then updates the track. */
void cfeTrackShiftTimes(CfeTrackNode *track, int time, int delta)
{
    CfeTrackNode *node = cfeTrackFindAtOrAfter(track, time);
    CfeTrackNode *next;

    while (node != 0) {
        next = node->next;
        node->startTime += delta;
        node = next;
    }
    cfeTrackUpdateDirections(track);
}

/* Moves every key from the first one at or after `time` back by `delta`, at most to one tick after the
 * key before it, then updates the track. A zero delta changes nothing. */
void cfeTrackShiftTimesBack(CfeTrackNode *track, int time, int delta)
{
    CfeTrackNode *node = cfeTrackFindAtOrAfter(track, time);
    CfeTrackNode *next;
    int *start;
    int gap;

    if (node != 0) {
        if (delta == 0) {
            return;
        }
        if (node->prev != 0) {
            /* Read through a pointer: the original loads startTime again here. */
            start = &node->startTime;
            gap = *start - node->prev->startTime - 1;
            if (gap < delta) {
                delta = gap;
            }
        }
        while (node != 0) {
            next = node->next;
            node->startTime -= delta;
            node = next;
        }
    }
    cfeTrackUpdateDirections(track);
}

/* 1 when every key of the track has the same value; 0 for an empty track. */
int cfeTrackAllValuesEqual(CfeTrackNode *node)
{
    int *first;
    int value;

    if (node == 0) {
        return 0;
    }
    /* Read through a pointer: the loop's first compare loads the value again, as in the original. */
    first = &node->value;
    value = *first;
    for (; node != 0; node = node->next) {
        if (node->value != value) {
            return 0;
        }
    }
    return 1;
}

f32 cfeTimeScale = 1.0f;
f32 cfeBezierTimeScale = 1.0f;
