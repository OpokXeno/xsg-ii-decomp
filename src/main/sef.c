#include "common.h"
#include "shared.h"
#include "main/sef.h"
#include "main/sv_get_image_item.h"
#include "main/vu0_vector.h"
#include "main/sc_get.h"
#include "main/sdv.h"
#include "main/srs.h"
#include "main/pxy_packet_state.h"
#include "main/sv_scene_flags.h"
#include "main/m_math.h"
#include "quadword.h"
#include "vu0_macro.h"
#include "main/xgl_cd.h"
#include "main/game_data.h"
#include "main/ytachi_test.h"
#include "main/sm_initilize.h"
#include "main/TiDynamicsColl.h"
#include "main/TiPartsAnim.h"
#include "main/uwamono_record.h"
#include "main/xgl_studio.h"
#include "main/xgl_camera.h"
#include "main/m_se_obj.h"
#include "main/xenoActor.h"
#include "main/sef_print.h"
#include "main/m_math_geom.h"
#include "main/vu0_matrix.h"
#include "main/vu0_rotate.h"
#include "main/xgl_2.h"
#include "ov01/unit_tbl.h"
#define SEF_EFFECT_LINE_COUNT 32

/*
 * One of a scheduler's 32 effect-table records (sefInitEffectData): the
 * first 0x20 bytes are line-data handles, filled with 0xff by memset (a
 * negative handle is a free slot; svDrawSchedulerEffect draws the others);
 * owner, effect_no and category are set from sefInitEffectData's
 * parameters, flags and frame are reset to 0.
 */
typedef struct EffectData {
    signed char lineHandles[SEF_EFFECT_LINE_COUNT];
    int owner;
    short category;
    short effect_no;
    short flags;
    short active;
    short frame;
} EffectData;

/* 128 records of the evidenced 0x820-byte line-data stride. */
extern unsigned char _lineData[0x41000];

/* Particle allocator (_ptAlloc): 1024 particle records of
 * SEF_PT_ALLOC_RECORD_SIZE bytes, then a 1024-entry ring of free handles. */
#define SEF_PT_ALLOC_COUNT 0x400
/* Particles per line-data record. */
#define SEF_LINE_PARTICLE_COUNT 0x100
#define SEF_QUARTER_PI 0.7853982f
#define SEF_IMAGE_PACKET_SCALE 0.005f
#define SEF_LINE_DATA_RECORD_SIZE 0x820
#define SEF_LINE_DATA_COUNT 0x80
#define SEF_LOCAL_DATA_POINT_HANDLES 0x6A0
#define SEF_LINE_DATA_STRIDE 0x8B0
/* _ptAlloc's per-record byte size, the same 640 sefCalcLocalMatrix already
 * multiplies _nowParentLocal by. */
#define SEF_PT_ALLOC_RECORD_SIZE 640

typedef struct SefLineDataEntry {
    unsigned char unmodeled_000[2];
    unsigned short inUse;
    unsigned char unmodeled_004[SEF_LINE_DATA_RECORD_SIZE - 4];
} SefLineDataEntry;

typedef struct SefLineDataTable {
    unsigned char unmodeled_000[0x810];
    SefLineDataEntry entries[SEF_LINE_DATA_COUNT];
} SefLineDataTable;

typedef struct SefLocalData {
    Matrix4 matrix;                                  /* +0x000 */
    unsigned char unmodeled_040[0x80 - 0x40];
    Vector4 direction;                               /* +0x080 */
    unsigned char unmodeled_090[0xa0 - 0x90];
    Vector4 noise;                                   /* +0x0a0: noise offset scaled by the position keys' header */
    Vector4 position;                                /* +0x0b0 */
    Vector4 drift;                                   /* +0x0c0: added to the attached points' positions */
    unsigned char unmodeled_0d0[0xd8 - 0xd0];
    float scaleTime;                                 /* +0x0d8: the header value of the size keys */
    unsigned char unmodeled_0dc[0xe0 - 0xdc];
    Vector4 positionValue;                           /* +0x0e0: constant value of the position keys */
    unsigned char unmodeled_0f0[0xf8 - 0xf0];
    short directionMode;                             /* +0x0f8 */
    unsigned char unmodeled_0fa[0x100 - 0xfa];
    Vector4 velocityValue;                           /* +0x100: constant value of keys[2] */
    unsigned char unmodeled_110[0x118 - 0x110];
    int constantKeys;                                /* +0x118: one bit per constant local key table */
    unsigned char unmodeled_11c[0x140 - 0x11c];
    Vector4 rotationValue;                           /* +0x140: constant value of keys[1] */
    unsigned char unmodeled_150[0x160 - 0x150];
    Vector4 scaleValue;                              /* +0x160: constant value of keys[3] */
    unsigned char unmodeled_170[0x180 - 0x170];
    struct SefTrackKey16 *keys[7];                   /* +0x180 */
    struct SefTrackKey16 *images;                    /* +0x19c: image list, its header vector precedes it */
    DoublewordVector4 imageOffset;                   /* +0x1a0 */
    unsigned char unmodeled_1b0[0x1cc - 0x1b0];
    unsigned short imageFlags;                       /* +0x1cc: flags of the first image packet */
    short imageKind;                                 /* +0x1ce */
    unsigned char unmodeled_1d0[0x670 - 0x1d0];
    struct SefTrackKey16 *scaleKeys;                 /* +0x670 */
    struct SefTrackKey16 *angleKeys;             /* +0x674: its header shorts sit before the first key */
    struct SefTrackKey16 *colorKeys;                 /* +0x678 */
    struct SefTrackKey *alphaKeys;                    /* +0x67c */
    void *spinKeys;                                  /* +0x680 */
    void *partKeys;                                  /* +0x684 */
    struct SefTrackKey *sizeKeys;                   /* +0x688 */
    short pointLife;                                  /* +0x68c: point life in frames */
    short speedVariation;                             /* +0x68e: speed variation of the points */
    float gravity;                                   /* +0x690 */
    float height;                                    /* +0x694 */
    float startFrame;                                /* +0x698 */
    int frame;                                       /* +0x69c */
    short pointHandles[SEF_LINE_PARTICLE_COUNT];     /* +0x6a0 */
    short emitFrame;                                     /* +0x8a0: frame of the last emission */
    short cursor;                                    /* +0x8a2 */
    short in_use;                                    /* +0x8a4 */
    short pointCount;                                      /* +0x8a6: live point count, returned by sefExecLineLocalData */
    short collision;                                 /* +0x8a8 */
    short count;                                     /* +0x8aa */
    unsigned char link;                              /* +0x8ac */
    unsigned char mode;                              /* +0x8ad */
    unsigned char unmodeled_8ae[SEF_LINE_DATA_STRIDE - 0x8ae];
} SefLocalData;

typedef struct SefPtAllocatorState {
    unsigned char records[SEF_PT_ALLOC_COUNT * SEF_PT_ALLOC_RECORD_SIZE];
    short freeSlots[SEF_PT_ALLOC_COUNT];
    unsigned int head;
    unsigned int tail;
    unsigned char unmodeled_a0808[8];
} SefPtAllocatorState;

void sefFreeLineData(int line_index);
void sefFreeEffectData(EffectData *effect, unsigned int index);
/* Frees particle handle `index` of the record when it is in range and * returns -1; out of range, returns the handle's byte offset in the record. */
int sefFreeLocalData(SefLocalData *data, unsigned int index);
/* The original local pointer stores the KSEG0 alias of the named table
 * sefAtanTbl inside _imageMapper at +0x22440 (main symbol map: size 0x1004). */
extern float *atanTbl_0;
void sefDestroyLocalData(SefLocalData *data);

/* One 8-byte key of a keyframe track: frame and next are the key index
 * the cursor steps to; value is the sample the cursor takes from it. */
typedef struct SefKeyframe {
    short frame;                                     /* +0x00 */
    short next;                                      /* +0x02 */
    unsigned short value;                            /* +0x04 */
    unsigned char unmodeled_006[2];
} SefKeyframe;

/* A track's cursor: the current key index, the sampled value and the
 * playback frame (float). */
typedef struct SefKeyCursor {
    short index;                                     /* +0x00 */
    unsigned short value;                            /* +0x02 */
    float frame;                                     /* +0x04 */
} SefKeyCursor;

/* File-level prototypes of functions defined later in this file or in another TU. */
void sefFlagSet(int mask);
void sefSetupEffect(void);
void sefDeleteEffectWait(int key);

/* A scratchpad parameter word, read as a float or as an int flag. */
typedef union SefScratchWord {
    float f;
    int i;
} SefScratchWord;

/* The 2D draw flag: the grand total word of effect entry 57 (sefSumEntryCounts), read by sefDrawEffect2D before its camera call. */
extern int sefEntry57Total[];

int sefGetEntry57Total(void) {
    return sefEntry57Total[0];
}

/* The 3D draw flag: the screen sprite count word of effect entry 57; sefDrawEffect3D draws only while it is non-zero. */
extern int sefEntry57ScreenSprites[];

int sefGetEntry57ScreenSprites(void) {
    return sefEntry57ScreenSprites[0];
}

void sefSumEntryCounts(void)
{
    unsigned int *counts;
    unsigned int *base;
    unsigned int *worldSprites;
    unsigned int *screenSprites;
    unsigned int *modelPoints;
    unsigned int *total;
    int entry;

    counts = (unsigned int *)sefEntryCounts;
    counts[285] = 0;
    counts[286] = 0;
    counts[287] = 0;
    counts[288] = 0;
    base = counts;
    counts = counts + 285;
    total = base + 3;
    screenSprites = base + 1;
    modelPoints = base + 2;
    worldSprites = base;
    entry = 0;
    do {
        total[entry * 5] = worldSprites[entry * 5] + screenSprites[entry * 5] + modelPoints[entry * 5];
        base[285] += worldSprites[entry * 5];
        base[286] += screenSprites[entry * 5];
        base[287] += modelPoints[entry * 5];
        base[288] += total[entry * 5];
        entry++;
    } while (entry < 57);
}

float srsAtan2(float x, float y)
{
    int y_is_positive = y >= 0.0f;
    int x_is_positive = x >= 0.0f;
    float result = 0.0f;
    float angle;

    if (y == 0.0f) {
        if (x != 0.0f) {
            if (x_is_positive) {
                result = 1.5707964f;
            } else {
                result = -1.5707964f;
            }
        }
    } else if (x == 0.0f) {
        if (!y_is_positive) {
            result = 3.1415927f;
        }
    } else {
        if (!y_is_positive) {
            y = -y;
        }
        if (!x_is_positive) {
            x = -x;
        }

        if (x <= y) {
            angle = atanTbl_0[(int)(x * 1024.0f / y)];
            if (y_is_positive) {
                if (!x_is_positive) {
                    angle = -angle;
                }
            } else if (x_is_positive) {
                angle = 3.1415927f - angle;
            } else {
                angle += 3.1415927f;
            }
        } else {
            angle = atanTbl_0[(int)(y * 1024.0f / x)];
            if (y_is_positive) {
                if (x_is_positive) {
                    angle = 1.5707964f - angle;
                } else {
                    angle = -(1.5707964f - angle);
                }
            } else if (x_is_positive) {
                angle += 1.5707964f;
            } else {
                angle = 4.712389f - angle;
            }
        }
        result = angle;
    }
    return result;
}

/* vector takes the fraction of vector + addend, all four lanes:
 * sum - (float) (int) sum. */
void sefAddVectorFraction(Vector4 *vector, void *addend)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "lqc2 vf2, 0(%1)\n"
        "vadd.xyzw vf1, vf1, vf2\n"
        "vftoi0.xyzw vf3, vf1\n"
        "vitof0.xyzw vf3, vf3\n"
        "vsub.xyzw vf3, vf1, vf3\n"
        "sqc2 vf3, 0(%0)\n"
        :
        : "r"(vector), "r"(addend)
        : "memory");
}

/* One 0x2C0-byte entry of the table at sefPtAllocatorState. head and tail are the free-slot ring indices of
 * sefPtFreeSlots and are used only through entry 0 (the ring control block); the getters return the entry's data,
 * which starts after a 16-byte prefix. */
typedef struct SefSlot {
    int head;
    int tail;
    unsigned char unmodeled_08[0x10 - 0x08];
    unsigned char data[0x2C0 - 0x10];
} SefSlot;

static unsigned char _ptAlloc[0xBD110];
SefSlot *sefPtAllocatorState = (SefSlot *) _ptAlloc;
extern int sefPtAllocCount;
static short sefPtFreeSlots[1104];
/* The request buffer: 0x340 bytes (SefRequestBuffer view); +0x100 (0x8046F0) and sefRequestParentMatrixPointer (+0x300) are
 * labels inside it. */
static unsigned char sefRequestBuffer[0x340];

void sevInitPtAllocator(void)
{
    short *slot;
    int i;

    sefMemZero(sefPtAllocatorState, 0xBD110);
    sefMemZero(sefRequestBuffer, 832);
    slot = sefPtFreeSlots;
    i = 1099;
    slot += i;
    for (; i >= 0; i--) {
        *slot = i;
        slot--;
    }
}

/* Takes the next free slot index from the sefPtFreeSlots ring (head side); returns -1 when the ring is empty. */
int sevAllocPtAllocator(void)
{
    SefSlot *ring = sefPtAllocatorState;
    int head = ring->head;
    int next = head + 1;
    int handle;

    if (!(next < sefPtAllocCount)) {
        next = 0;
    }
    if (next == ring->tail) {
        return -1;
    }
    handle = sefPtFreeSlots[head];
    ring->head = next;
    return handle;
}

void sevFreePtAllocator(int handle)
{
    SefSlot *ring = sefPtAllocatorState;
    int tail = ring->tail;
    int next;

    if (handle < 0) {
        return;
    }
    if (!(handle < sefPtAllocCount)) {
        return;
    }
    if (ring->head == tail) {
        return;
    }
    next = tail + 1;
    sefPtFreeSlots[tail] = handle;
    ring->tail = next;
    if (!(next < sefPtAllocCount)) {
        ring->tail = 0;
    }
}

/* One 8-byte record of a keyed track: the key is compared with the integer
 * time, 0x400 ends the track, link is the index of the next key (-1 when the
 * track stops there), value is the sample the key holds. */
typedef struct SefTrackKey {
    short key;
    unsigned short link;
    float value;
} SefTrackKey;

/* A scheduled-effect slot as sefInitEffectData re-initializes it: the first 0x20
 * bytes are line handles, the next 0x20 are one tag byte per handle (written
 * by sefAllocEffectData), the rest of the fields are only the ones the reset
 * writes. */
typedef struct SefEffectSlot {
    unsigned char lineHandles[0x20];
    unsigned char lineTags[0x40 - 0x20];
    int id;
    unsigned int flags;
    short kind;
    unsigned char lineCount;
} SefEffectSlot;

/* A track cursor: the key index it is on, the running time and the
 * interpolated samples the track readers store. */
typedef struct SefTrackCursor {
    short index;
    short sampled_s16;
    float time;
    float sampled;
} SefTrackCursor;

typedef struct SefTrackKey16 {
    float value[3];                                  /* +0x00 */
    short key;
    signed char link;
    unsigned char unmodeled_00f[1];
} SefTrackKey16;

extern f32 sefTrackTimeStep;

/* Advances a track cursor by one time step, and returns the key it now sits on. */
SefTrackKey *sefAdvanceTrackCursor(SefTrackKey *keys, SefTrackCursor *cursor)
{
    f32 time;
    int frame;
    SefTrackKey *key;
    short next_index;

    time = cursor->time + sefTrackTimeStep;
    key = &keys[cursor->index];
    frame = (int)time;
    cursor->time = time;
    if (frame < SEF_TRACK_END_KEY && key->key != SEF_TRACK_END_KEY && frame >= (key + 1)->key) {
        key++;
        next_index = (short)key->link;
        if (next_index >= 0) {
            cursor->index = key->link;
            key = &keys[next_index];
            cursor->time = (f32)key->key;
        } else {
            cursor->index = (s16)((u16)cursor->index + 1);
        }
    }
    return key;
}

/* The wide-track form of sefAdvanceTrackCursor: advances a cursor over 16-byte records
 * and returns the record it sits on. */
SefTrackKey16 *sefAdvanceVectorTrackCursor(SefTrackKey16 *keys, SefTrackCursor *cursor)
{
    f32 time;
    int frame;
    SefTrackKey16 *key;
    signed char next_index;

    time = cursor->time + sefTrackTimeStep;
    key = &keys[cursor->index];
    frame = (int)time;
    cursor->time = time;
    if (frame < SEF_TRACK_END_KEY && key->key != SEF_TRACK_END_KEY && frame >= (key + 1)->key) {
        key++;
        next_index = key->link;
        if (next_index >= 0) {
            cursor->index = next_index;
            key = &keys[next_index];
            cursor->time = (f32)key->key;
        } else {
            cursor->index = (s16)((u16)cursor->index + 1);
        }
    }
    return key;
}

void sefLerpPositionXyz(void *unused, Vector4 *position, const Vector4 *target)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "lqc2 vf2, 0(%1)\n"
        "mfc1 $8, $f6\n"
        "vmove.w vf2w, vf0w\n"
        "qmtc2 $8, vf3\n"
        "vsub.xyz vf2, vf2, vf1\n"
        "vmulx.xyz vf2, vf2, vf3x\n"
        "vadd.xyz vf2, vf2, vf1\n"
        "sqc2 vf2, 0(%0)\n"
        :
        : "r"(position), "r"(target)
        : "$8", "$f6", "memory");
}

/* A vector track's cursor: the track cursor, then the sampled vector the
 * track writes at +0x10. */
typedef struct SefVectorTrackCursor {
    SefTrackCursor track;                            /* +0x00 */
    unsigned char unmodeled_00c[0x10 - 0x0c];
    Vector4 value;                                   /* +0x10 */
} SefVectorTrackCursor;

/* Samples a vector track at the cursor's time into cursor->value. Returns 0 when
 * the cursor sits on the last key, 1 otherwise. */
int sefLerpVector(SefTrackKey16 *keys, SefVectorTrackCursor *cursor)
{
    Vector4 *value;
    SefTrackKey16 *key;
    int start;
    int span;
    float fraction;

    key = &keys[cursor->track.index];
    if ((key + 1)->key == SEF_TRACK_END_KEY) {
        return 0;
    }
    value = &cursor->value;
    key = sefAdvanceVectorTrackCursor(keys, &cursor->track);
    start = key->key;
    span = (key + 1)->key - start;
    if (span != 0) {
        fraction = (cursor->track.time - (float)start) / (float)span;
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n"
            "lqc2 vf2, 16(%0)\n"
            "mfc1 $9, %2\n"
            "vmove.w vf2, vf0\n"
            "qmtc2 $9, vf3\n"
            "vsub.xyz vf2, vf2, vf1\n"
            "vmulx.xyz vf2, vf2, vf3x\n"
            "vadd.xyz vf2, vf2, vf1\n"
            "sqc2 vf2, 0(%1)\n"
            :
            : "r"(key), "r"(value), "f"(fraction)
            : "$9", "memory");
    }
    return 1;
}

/* A colour track's cursor: the track cursor, then the four integer colour
 * lanes sefSampleColorTrack writes at +0x10. */
typedef struct SefColorTrackCursor {
    SefTrackCursor track;                            /* +0x00 */
    unsigned char unmodeled_00c[0x10 - 0x0c];
    int color[4];                                    /* +0x10 */
} SefColorTrackCursor;

/* Samples a colour track at the cursor's time: the two keys the cursor spans
 * are interpolated per lane, scaled by the track's colour, clamped to 255 and
 * stored as integers in the cursor's colour lanes. */
void sefSampleColorTrack(SefTrackKey16 *keys, SefColorTrackCursor *cursor, const Vector4 *scale)
{
    SefTrackKey16 *key;
    int *lanes;
    short start;
    short end;
    float fraction;

    key = &keys[cursor->track.index];
    if ((key + 1)->key != SEF_TRACK_END_KEY) {
        lanes = cursor->color;
        key = sefAdvanceVectorTrackCursor(keys, &cursor->track);
        start = key->key;
        end = (key + 1)->key;
        if (start != end) {
            fraction = (cursor->track.time - (float)start) / (float)(end - start);
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%0)\n\t"
                "lqc2 $vf2, 16(%0)\n\t"
                "lqc2 $vf5, 0(%2)\n\t"
                "mfc1 $9, %3\n\t"
                "mfc1 $8, %4\n\t"
                "vsub.w $vf2, $vf2, $vf2\n\t"
                "qmtc2 $9, $vf3\n\t"
                "qmtc2 $8, $vf4\n\t"
                "vsub.xyz $vf2, $vf2, $vf1\n\t"
                "vmulx.xyz $vf2, $vf2, $vf3x\n\t"
                "vadd.xyz $vf2, $vf2, $vf1\n\t"
                "vmul.xyzw $vf1, $vf5, $vf2\n\t"
                "vminix.xyz $vf2, $vf1, $vf4x\n\t"
                "vftoi0.xyzw $vf2, $vf2\n\t"
                "sqc2 $vf2, 0(%1)"
                :
                : "r"(key), "r"(lanes), "r"(scale), "f"(fraction), "f"(255.0f)
                : "$8", "$9", "memory");
        }
    }
}

/* Samples a track at the cursor's time into cursor->sampled. Past the last
 * key the sample holds that key's value; otherwise the cursor advances and
 * the sample is interpolated between the two keys it now spans. */
void sefSampleTrack(SefTrackKey *keys, SefTrackCursor *cursor)
{
    SefTrackKey *key;
    f32 sample;

    key = &keys[cursor->index];
    if ((key + 1)->key == SEF_TRACK_END_KEY) {
        sample = key->value;
    } else {
        key = sefAdvanceTrackCursor(keys, cursor);
        if (key->key == (key + 1)->key) {
            return;
        }
        sample = ((key + 1)->value - key->value)
               * ((cursor->time - (f32)key->key) / (f32)((key + 1)->key - key->key))
               + key->value;
    }
    cursor->sampled = sample;
}

/* The truncated-sample reader: like sefSampleTrack but the interpolated value
 * goes to cursor->sampled_s16, and nothing is written while either key of the
 * current span is the 0x400 end marker. */
void sefSampleTrackTruncated(SefTrackKey *keys, SefTrackCursor *cursor)
{
    SefTrackKey *key;
    f32 sample;

    key = &keys[cursor->index];
    if (key->key == SEF_TRACK_END_KEY || (key + 1)->key == SEF_TRACK_END_KEY) {
        return;
    }
    key = sefAdvanceTrackCursor(keys, cursor);
    if (key->key == (key + 1)->key) {
        return;
    }
    sample = ((key + 1)->value - key->value)
           * ((cursor->time - (f32)key->key) / (f32)((key + 1)->key - key->key))
           + key->value;
    cursor->sampled_s16 = (short)sample;
}

void sefProgressInt(SefKeyframe *keys, SefKeyCursor *cursor) {
    cursor->value = ((SefKeyframe *)sefAdvanceTrackCursor((SefTrackKey *)keys, (SefTrackCursor *)cursor))->value;
}


/* Builds in destination the rotation that turns axis onto the normalized direction (MMathMatrixBetweenVectors);
 * returns 0 without touching destination for a zero direction. */
int sefGetDirMatrix(Matrix4 *destination, const Vector4 *direction, const Vector4 *axis)
{
    Vector4 normalized;
    int is_zero;

    is_zero = ((*(int *)&direction->z | *(u64 *)direction) == 0);
    if (is_zero) {
        return 0;
    }
    MMathNormalizeVector2(&normalized, direction);
    MMathMatrixBetweenVectors(destination, axis, &normalized);
    return 1;
}

void sefGetVecMatrix(void *state, Vector4 *minuend, Vector4 *subtrahend, const Vector4 *axis) {
    Vector4 delta;

    Vu0SefSubVectorXYZ(&delta, minuend, subtrahend);
    sefGetDirMatrix(state, &delta, axis);
}

/* result = the normalized (first - second) when flags has 0x400, else the
 * normalized (second - first); xyz only. The normalizing subroutine works on
 * vf20 in place. */
void sefNormalizeDifference(Vector4 *result, const Vector4 *first, const Vector4 *second, int flags) {
    __asm__ __volatile__(
        "lqc2 vf20, 0(%0)\n"
        "lqc2 vf1, 0(%1)\n"
        :
        : "r"(first), "r"(second)
        : "memory");
    if ((flags & 0x400) != 0) {
        __asm__ __volatile__("vsub.xyz vf20, vf20, vf1" : : : "memory");
    } else {
        __asm__ __volatile__("vsub.xyz vf20, vf1, vf20" : : : "memory");
    }
    vu0NormalizeVf20();
    __asm__ __volatile__("sqc2 vf20, 0(%0)" : : "r"(result) : "memory");
}

typedef union SefWord {
    float f;
    int i;
} SefWord;

typedef struct SefTransformState {
    Matrix4 matrix;                                  /* +0x000 */
    union {
        Matrix4 local_matrix;                        /* +0x040: MMathRotateMatrixYXZ fills all four rows */
        struct {
            Vector4 local_rows[3];                   /* +0x040 */
            Vector4 velocity;                        /* +0x070: row 3 of local_matrix */
        };
    };
    Vector4 rotated_rows[3];                         /* +0x080 */
    Vector4 origin;                                  /* +0x0b0: the start position at creation */
    Matrix4 cached_matrix;                           /* +0x0c0 */
    Matrix4 part_matrix;                             /* +0x100 */
    Vector4 position;                            /* +0x140 */
    Vector4 targetPosition;                      /* +0x150 */
    Vector4 scale;                                   /* +0x160 */
    Vector4 pointVelocity;                             /* +0x170 */
    Vector4 lastDisplacement;                             /* +0x180 */
    unsigned char unmodeled_190[0x1a0 - 0x190];
    Vector4 sizeScale;                               /* +0x1a0 */
    Vector4 colorScale;                              /* +0x1b0 */
    Vector4 colorBase;                               /* +0x1c0 */
    unsigned char unmodeled_1d0[0x1e8 - 0x1d0];
    struct SefMotionState *motion;                   /* +0x1e8 */
    unsigned char unmodeled_1ec[0x208 - 0x1ec];
    float spin;                                      /* +0x208 */
    float speed;                                     /* +0x20c */
    Vector4 scaleKeyValue;                           /* +0x210 */
    unsigned char unmodeled_220[0x228 - 0x220];
    float angle_base;                                /* +0x228 */
    unsigned char unmodeled_22c[0x230 - 0x22c];
    Vector4 angles;                                  /* +0x230 */
    unsigned char unmodeled_240[0x24c - 0x240];
    SefWord frame;                                   /* +0x24c */
    Vector4 direction;                               /* +0x250: the direction vector */
    unsigned char unmodeled_260[0x268 - 0x260];
    short keyFrame;                                  /* +0x268 */
    short kind;                                      /* +0x26a */
    float time;                                      /* +0x26c */
    int color[4];                                    /* +0x270 */
    unsigned char unmodeled_280[0x288 - 0x280];
    int flags;                                       /* +0x288 */
    SefWord textureIndex;                            /* +0x28c */
    Vector4 point;                                   /* +0x290 */
    unsigned char unmodeled_2a0[0x2b4 - 0x2a0];
    float spinSpeed;                                 /* +0x2b4 */
    unsigned char unmodeled_2b8[0x2ba - 0x2b8];
    short textureFrame;                              /* +0x2ba */
} SefTransformState;

extern Matrix4 *sefRequestParentMatrixPointer[];
const Vector4 sefDefaultOffset = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f };
extern SefTransformState *sefLinkedPoint;
void sefGetKindMatrix(Matrix4 *dest, int unused, int matrixIndex, int kind);

/* The request block sefRequestBuffer as the transform reads it: kind1Key is its first word; the kind matrices sit at
 * +0x80 (kind 64), +0xc0 (kind 256), +0x100 (kind 128) and +0x240 (kind 161). */
typedef struct SefRequestMatrices {
    int kind1Key;                                    /* +0x000 */
    unsigned char unmodeled_004[0x80 - 0x04];
    Matrix4 kind64Matrix;                            /* +0x080 */
    Matrix4 kind256Matrix;                           /* +0x0c0 */
    Matrix4 kind128Matrix;                           /* +0x100 */
    unsigned char unmodeled_140[0x240 - 0x140];
    Matrix4 kind161Matrix;                           /* +0x240 */
} SefRequestMatrices;

/* The motion curve the slot transform follows: a non-zero hasCurve selects the
 * Hermite key path. */
typedef struct SefMotionCurve {
    unsigned char unmodeled_00[0x16];
    short hasCurve;                                  /* +0x16 */
    MMathHermiteKey *keys;                           /* +0x18 */
} SefMotionCurve;

typedef struct SefPointMotion {
    int fixedGravity;                                /* +0x00 */
    float gravity;                                   /* +0x04 */
    float bounce;                                    /* +0x08 */
    float floor;                                     /* +0x0c */
    float floorBase;                                 /* +0x10 */
    short sameDirection;                             /* +0x14 */
    unsigned short keyParameter;                     /* +0x16 */
    struct SefTrackKey16 *keys;                      /* +0x18 */
    unsigned char unmodeled_1C[0xA0 - 0x1C];
} SefPointMotion;

SefPointMotion sefPointMotion = { 0 };
const Vector4 sefDirectionAxes[2] = { { .x = 0.0f, .y = 0.0f, .z = 1.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = -1.0f, .w = 1.0f } };

/* Clears a vector to the origin (0, 0, 0, 1). */
static inline void sefClearVector(Vector4 *vector)
{
    __asm__ __volatile__("sqc2 $vf0, 0(%0)" : : "r"(vector) : "memory");
}

/* Vector stores whose outputs are the four components of the destination. */
#define SefSubVectorXYZ4(destination, minuend, subtrahend) \
    __asm__ __volatile__("lqc2 vf1, 0(%4)\n" "lqc2 vf2, 0(%5)\n" "vsub.xyz vf1, vf1, vf2\n" "sqc2 vf1, 0(%6)\n" \
        : "=m"((destination)->x), "=m"((destination)->y), "=m"((destination)->z), "=m"((destination)->w) \
        : "r"(minuend), "r"(subtrahend), "r"(destination) : "memory")
#define SefSetOrigin4(destination) \
    __asm__ __volatile__("sqc2 vf0, 0(%4)\n" \
        : "=m"((destination)->x), "=m"((destination)->y), "=m"((destination)->z), "=m"((destination)->w) \
        : "r"(destination) : "memory")
#define SefStoreVf20(destination) \
    __asm__ __volatile__("sqc2 $vf20, 0(%4)" \
        : "=m"((destination)->x), "=m"((destination)->y), "=m"((destination)->z), "=m"((destination)->w) \
        : "r"(destination) : "memory")
#define SefCopyVector4(destination, source) \
    __asm__ __volatile__("lq $8, 0(%5)\n" "sq $8, 0(%4)\n" \
        : "=m"((destination)->x), "=m"((destination)->y), "=m"((destination)->z), "=m"((destination)->w) \
        : "r"(destination), "r"(source) : "$8", "memory")

/* Computes this frame's displacement of a slot into displacement and
 * state->lastDisplacement. With a motion curve the Hermite keys give the point and
 * the step, and the result is relative to position (returns 0); otherwise
 * the position, blended toward the unit z axis, is moved through the matrix the
 * mode selects and scaled by the step (returns 1). */
int sefGetSpeed(Vector4 *displacement, SefTransformState *state, int mode, float blend)
{
    Matrix4 *matrix = 0;
    int refreshed = 0;
    unsigned char *scratchpad = (unsigned char *)0x70000000;
    SefMotionCurve *curve = (SefMotionCurve *) &sefPointMotion;
    SefRequestMatrices *request = (SefRequestMatrices *)sefRequestBuffer;
    Vector4 *position = &state->direction;
    float step;

    step = (state->spinSpeed + state->spin) * ((SefScratchWord *)(scratchpad + 0xE4))->f;
    if (curve->hasCurve != 0) {
        Vector4 *point;
        float length;

        if (!(state->flags & 2)) {
            state->flags |= 2;
            vu0SetRotationFromDirection(state->cached_matrix, &state->targetPosition, &state->origin);
        }
        length = vu0CopyMatrixWithTranslationDist((Vector4 *)0x70000000, state->cached_matrix, &state->origin, &state->targetPosition);
        MMathEvalHermiteKeys((Vector4 *)0x70000040, curve->keys, state->time);
        __asm__ __volatile__(
            "lqc2 $vf26, 0(%0)\n\t"
            "lqc2 $vf27, 16(%0)\n\t"
            "lqc2 $vf28, 32(%0)\n\t"
            "lqc2 $vf29, 48(%0)"
            :
            : "r"(scratchpad)
            : "memory");
        vu0RotateCurrentYXZ(&state->velocity);
        __asm__ __volatile__(
            "mfc1 $8, %0\n\t"
            "qmtc2.ni $8, $vf1\n\t"
            "vmulx.xyzw $vf28, $vf28, $vf1x"
            :
            : "f"(length)
            : "$8", "memory");
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%0)\n\t"
            "vmulax.xyzw ACC, $vf26, $vf1x\n\t"
            "vmadday.xyzw ACC, $vf27, $vf1y\n\t"
            "vmaddaz.xyzw ACC, $vf28, $vf1z\n\t"
            "vmaddw.xyzw $vf20, $vf29, $vf1w"
            :
            : "r"((Vector4 *)0x70000040));
        point = (Vector4 *)0x70000040;
        __asm__ __volatile__("sqc2 $vf20, 0(%0)" : : "r"(point) : "memory");
        SefSubVectorXYZ4(displacement, point, &state->position);
        SefCopyVector4(&state->lastDisplacement, displacement);
        QuadwordCopyT0(&state->position, point);
        return 0;
    }
    if (step == 0.0f) {
        SefSetOrigin4(displacement);
        return 1;
    }
    if (blend != 0.0f && mode != 0) {
        __asm__ __volatile__(
            "mfc1 $8, %2\n\t"
            "vmr32.xyzw $vf13, $vf0\n\t"
            "lqc2 $vf11, 0(%1)\n\t"
            "lqc2 $vf12, 0(%1)\n\t"
            "qmtc2.ni $8, $vf4\n\t"
            "vsub.xyz $vf11, $vf13, $vf11\n\t"
            "vmulax.xyz ACC, $vf11, $vf4x\n\t"
            "vmaddw.xyz $vf11, $vf12, $vf0w\n\t"
            "sqc2 $vf11, 0(%0)"
            :
            : "r"((Vector4 *)0x70000090), "r"(position), "f"(blend)
            : "$8", "memory");
    } else {
        QuadwordCopyT0((Vector4 *)0x70000090, position);
    }
    if (blend != 0.0f || !(state->flags & 2)) {
        state->flags |= 2;
        refreshed = 1;
        if (mode >= 0x20) {
            int flags = ((SefScratchWord *)0x70000138)->i;

            if (flags != 0) {
                sefNormalizeDifference((Vector4 *)0x70000090, &state->targetPosition, &state->position, flags);
            } else {
                matrix = &state->cached_matrix;
                if (vu0SetRotationFromDirection(*matrix, &state->targetPosition, &state->position) == 0) {
                    matrix = 0;
                    sefClearVector((Vector4 *)0x70000090);
                }
            }
        } else if (mode == 0x10) {
            SefTransformState *linkedPoint = sefLinkedPoint;

            if (linkedPoint != 0) {
                int flags = ((SefScratchWord *)0x70000138)->i;

                if (flags != 0) {
                    sefNormalizeDifference((Vector4 *)0x70000090, &linkedPoint->lastDisplacement, &sefDefaultOffset, flags);
                } else {
                    matrix = &state->cached_matrix;
                    if ((short)sefGetDirMatrix(matrix, &linkedPoint->lastDisplacement, sefDirectionAxes) != 0) {
                        matrix = 0;
                        sefClearVector((Vector4 *)0x70000090);
                    }
                }
            }
        } else if (mode == 0xC) {
            if (request->kind1Key != 0) {
                matrix = &request->kind64Matrix;
            }
        }
    } else {
        if (((SefScratchWord *)0x70000138)->i != 0) {
            MMathNormalizeVector2((Vector4 *)0x70000090, &state->lastDisplacement);
        } else if (mode >= 9) {
            matrix = &state->cached_matrix;
        }
    }
    __asm__ __volatile__("lqc2 $vf20, 0(%0)" : : "r"((Vector4 *)0x70000090) : "memory");
    if (matrix != 0) {
        __asm__ __volatile__(
            "lqc2 $vf26, 0(%0)\n\t"
            "lqc2 $vf27, 16(%0)\n\t"
            "lqc2 $vf28, 32(%0)\n\t"
            "lqc2 $vf29, 48(%0)\n\t"
            "vmulax.xyzw ACC, $vf26, $vf20x\n\t"
            "vmadday.xyzw ACC, $vf27, $vf20y\n\t"
            "vmaddaz.xyzw ACC, $vf28, $vf20z\n\t"
            "vmaddw.xyzw $vf20, $vf29, $vf0w"
            :
            : "r"(matrix)
            : "memory");
    }
    if (step < 0.0f && !refreshed) {
        __asm__ __volatile__(
            "mfc1 $8, %0\n\t"
            "qmtc2.ni $8, $vf1\n\t"
            "vmulx.xyz $vf20, $vf20, $vf1x"
            :
            : "f"(-step)
            : "$8", "memory");
    } else {
        __asm__ __volatile__(
            "mfc1 $8, %0\n\t"
            "qmtc2.ni $8, $vf1\n\t"
            "vmulx.xyz $vf20, $vf20, $vf1x"
            :
            : "f"(step)
            : "$8", "memory");
    }
    SefStoreVf20(&state->lastDisplacement);
    SefStoreVf20(displacement);
    return 1;
}

/* Advances a slot's motion by one frame: the step from sefGetSpeed is added
 * to position (the position), gravity is taken off pointVelocity.y (the
 * velocity) once per whole time unit, and on reaching the floor the velocity
 * is scaled by the bounce factor and made to point up. */
void sefMoveParticle(SefTransformState *state, int mode, int life, float blend)
{
    Vector4 *displacement;

    state->time += ((SefScratchWord *)0x700000E4)->f;
    if (!(state->flags & 1)) {
        displacement = (Vector4 *)0x70000050;
        if (sefGetSpeed(displacement, state, mode, blend) == 0) {
            return;
        }
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n"
            "lqc2 vf2, 0(%1)\n"
            "vadd.xyz vf1, vf1, vf2\n"
            "sqc2 vf1, 0(%0)\n"
            :
            : "r"(&state->position), "r"(displacement), "m"(*displacement), "m"(displacement->w)
            : "memory");
    } else {
        Vu0SetOrigin((Vector4 *)0x70000050);
    }
    if (((SefScratchWord *)0x700000F4)->i != 0) {
        state->position.y -= ((SefScratchWord *)0x700000E8)->f;
    } else {
        int frame = (int)state->time;

        if (frame != state->keyFrame) {
            state->pointVelocity.y -= ((SefScratchWord *)0x700000E8)->f;
        }
        state->keyFrame = frame;
    }
    __asm__ __volatile__(
        "lqc2 vf1, 0(%1)\n"
        "lqc2 vf2, 0(%2)\n"
        "vadd.xyz vf1, vf1, vf2\n"
        "sqc2 vf1, 0(%1)\n"
        : "+m"(state->position)
        : "r"(&state->position), "r"(&state->pointVelocity), "m"(state->pointVelocity)
        : "memory");
    if (((SefScratchWord *)0x700000EC)->f != 0.0f && state->position.y <= ((SefScratchWord *)0x700000F0)->f) {
        if (!(state->flags & 1)) {
            Vu0SefAddVectorXYZ(&state->pointVelocity, (Vector4 *)0x70000050, &state->pointVelocity);
        }
        state->flags |= 1;
        state->position.y = ((SefScratchWord *)0x700000F0)->f;
        Vu0SefScaleVectorXYZNI(&state->pointVelocity, &state->pointVelocity, ((SefScratchWord *)0x700000EC)->f);
        if (state->pointVelocity.y < 0.0f) {
            state->pointVelocity.y = -state->pointVelocity.y;
        }
    }
}

/* Copies rows 0..2 of src into dest and sets row 3 of dest to the unit w row. */
#define sefCopyRowsUnitW(dest, src) \
    do { \
        Matrix4 *d_ = (dest); \
        Matrix4 *s_ = (src); \
        __asm__ __volatile__( \
            "lqc2 $vf26, 0(%0)\n\t" \
            "lqc2 $vf27, 16(%0)\n\t" \
            "lqc2 $vf28, 32(%0)" \
            : \
            : "r"(s_), "m"((*s_)[0][0]) \
            : "memory"); \
        __asm__ __volatile__( \
            "sqc2 $vf26, 0(%0)\n\t" \
            "sqc2 $vf27, 16(%0)\n\t" \
            "sqc2 $vf28, 32(%0)" \
            : \
            : "r"(d_) \
            : "memory"); \
        __asm__ __volatile__("sqc2 $vf0, 48(%0)" : : "r"(d_) : "memory"); \
    } while (0)

/* Copies rows 0..2 of src into dest; row 3 of dest is left alone. */
static inline void sefCopyRows3(Matrix4 *dest, const Vector4 *src)
{
    __asm__ __volatile__(
        "lqc2 $vf26, 0(%1)\n\t"
        "lqc2 $vf27, 16(%1)\n\t"
        "lqc2 $vf28, 32(%1)\n\t"
        "sqc2 $vf26, 0(%0)\n\t"
        "sqc2 $vf27, 16(%0)\n\t"
        "sqc2 $vf28, 32(%0)"
        :
        : "r"(dest), "r"(src)
        : "memory");
}

/* Loads the rotated rows (state->rotated_rows) into vf26-vf28. */
static inline void sefLoadRotatedRows(SefTransformState *state)
{
    __asm__ __volatile__(
        "lqc2 $vf26, 0(%0)\n\t"
        "lqc2 $vf27, 16(%0)\n\t"
        "lqc2 $vf28, 32(%0)"
        :
        : "r"(state->rotated_rows)
        : "memory");
}

/* Stores vf26-vf28 and the unit w row (vf0) as state->matrix. */
static inline void sefStoreTransform(SefTransformState *state)
{
    __asm__ __volatile__(
        "sqc2 $vf26, 0(%0)\n\t"
        "sqc2 $vf27, 16(%0)\n\t"
        "sqc2 $vf28, 32(%0)\n\t"
        "sqc2 $vf0, 48(%0)"
        :
        : "r"(state->matrix)
        : "memory");
}

/* Multiplies the current matrix by the rotated rows into the part matrix, then stores the result as state->matrix. */
static inline void sefMultiplyTransform(SefTransformState *state)
{
    sefLoadRotatedRows(state);
    __asm__ __volatile__("vmove.xyzw $vf29, $vf0" : : : "memory");
    vu0CurMatrixMulRight((Vector4 *) &state->part_matrix);
    sefStoreTransform(state);
}

void sefCalcLocalMatrix(SefTransformState *state, int kind, int mode, int flags)
{
    int special;

    special = (mode == 0x4000 || mode == 0x2000);
    if (special || (flags & 2) || (state->flags & 4) == 0) {
        SefRequestMatrices *request = (SefRequestMatrices *) sefRequestBuffer;

        state->flags |= 4;
        sefCopyRows3(&state->part_matrix, vu0IdentityMatrix);
        if (mode == 0x2000) {
            Vector4 *scratch = (Vector4 *) 0x70000050;

            __asm__ __volatile__("lq $8, 0(%2)\nsq $8, 0(%1)\n" : "=m"(*scratch) : "r"(scratch), "r"(&state->pointVelocity), "m"(state->pointVelocity) : "$8", "memory");
            if (!(state->flags & 1)) {
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%1)\n"
                    "lqc2 vf2, 0(%0)\n"
                    "vadd.xyz vf1, vf1, vf2\n"
                    "sqc2 vf1, 0(%1)\n"
                    :
                    : "r"(&state->lastDisplacement), "r"(scratch)
                    : "memory");
            }
            sefGetDirMatrix(&state->part_matrix, scratch, sefDirectionAxes);
        } else if (mode == 0x3000) {
            state->flags |= 0x8000;
            sefGetKindMatrix(&state->part_matrix, request->kind1Key, kind & 31, 1);
            __asm__ __volatile__("sqc2 $vf0, 0(%0)" : : "r"((state->part_matrix)[3]) : "memory");
        } else if (kind == 0x10) {
            if (sefLinkedPoint != 0) {
                if (flags & 1) {
                    QuadwordCopyMatrixT0(&state->part_matrix, &state->cached_matrix);
                } else {
                    sefGetDirMatrix(&state->part_matrix, &sefLinkedPoint->lastDisplacement, sefDirectionAxes);
                }
            }
        } else if (kind == 0x40 && mode == kind) {
            sefCopyRowsUnitW(&state->part_matrix, &request->kind64Matrix);
        } else if (kind == 256 && mode == kind) {
            sefCopyRowsUnitW(&state->part_matrix, &request->kind256Matrix);
        } else if (kind == 128 && mode == kind) {
            sefCopyRowsUnitW(&state->part_matrix, &request->kind128Matrix);
        } else if (kind == 161 && mode == kind) {
            sefCopyRowsUnitW(&state->part_matrix, &request->kind161Matrix);
        } else if (kind > 31) {
            Vector4 *aux = &state->position;

            if (special == 0) {
                aux = (Vector4 *) 0x700000A0;
            }
            if (mode == 0x10) {
                SefTransformState *linkedPoint = sefLinkedPoint;
                if (linkedPoint != 0) {
                    aux = &linkedPoint->position;
                }
            }
            if (kind == 0x21) {
                vu0SetRotationFromDirection(state->part_matrix, &state->targetPosition, aux);
            } else {
                vu0SetRotationFromDirection(state->part_matrix, (Vector4 *) 0x700000B0, aux);
            }
        } else {
            sefLoadRotatedRows(state);
            sefStoreTransform(state);
            return;
        }
    } else if (kind <= 12) {
        sefLoadRotatedRows(state);
        sefStoreTransform(state);
        return;
    }

    sefMultiplyTransform(state);
}

/* Sets the transform's current rotation and stores it into the state. Mode 0
 * rotates about Z by the base angle plus the angles' z and stores the whole
 * current matrix into state->matrix. Otherwise the flag word's 0x8 bit selects
 * the source: the local rows (multiplied by the global matrix when it is set),
 * or the global matrix's four rows; the rotation about the angles follows, or
 * the set-rotation routine when no global matrix is set. The three rows
 * result is stored into state->rotated_rows. */
void sefSetTransformRotation(SefTransformState *state, int mode)
{
    Vector4 *angles = &state->angles;

    if (mode != 0) {
        if ((state->flags & 0x8) != 0) {
            __asm__ __volatile__(
                "lqc2 $vf26, 0(%0)\n\t"
                "lqc2 $vf27, 16(%0)\n\t"
                "lqc2 $vf28, 32(%0)\n\t"
                "vmove.xyzw $vf29, $vf0"
                :
                : "r"(state->local_rows)
                : "memory");
            if (sefRequestParentMatrixPointer[0] != 0) {
                vu0CurMatrixMul(*sefRequestParentMatrixPointer[0]);
            }
            vu0RotateCurrentYXZ(angles);
        } else if (sefRequestParentMatrixPointer[0] != 0) {
            __asm__ __volatile__(
                "lqc2 $vf26, 0(%0)\n\t"
                "lqc2 $vf27, 16(%0)\n\t"
                "lqc2 $vf28, 32(%0)\n\t"
                "lqc2 $vf29, 48(%0)"
                :
                : "r"(sefRequestParentMatrixPointer[0])
                : "memory");
            vu0RotateCurrentYXZ(angles);
        } else {
            vu0SetCurrentRotationYXZ(angles);
        }
        __asm__ __volatile__(
            "sqc2 $vf26, 0(%0)\n\t"
            "sqc2 $vf27, 16(%0)\n\t"
            "sqc2 $vf28, 32(%0)"
            :
            : "r"(state->rotated_rows)
            : "memory");
    } else {
        vu0SetCurrentRotationZ(state->angle_base + angles->z);
        __asm__ __volatile__(
            "sqc2 $vf26, 0(%0)\n\t"
            "sqc2 $vf27, 16(%0)\n\t"
            "sqc2 $vf28, 32(%0)\n\t"
            "sqc2 $vf29, 48(%0)"
            :
            : "r"(state->matrix)
            : "memory");
    }
}

/* One 12-byte key of a curve: key is its time (0x400 ends the curve), link
 * the index the cursor jumps to (negative values mean the cursor steps to the
 * next key, or holds). */
typedef struct SefCurveKey {
    short key;                                       /* +0x00 */
    signed char link;                                /* +0x02 */
    unsigned char unmodeled_003[9];
} SefCurveKey;

/* One 32-byte curve segment; value is the sample the segment holds at its key. */
typedef struct SefCurveSegment {
    unsigned char unmodeled_000[24];
    float value;                                     /* +0x18 */
    unsigned char unmodeled_01c[4];
} SefCurveSegment;

/* The segment cursor of a curve track: the key index and the segment entries
 * it points at in each table. */
typedef struct SefCurveCursor {
    int index;                                       /* +0x00 */
    unsigned char unmodeled_004[4];
    SefCurveSegment *segment_a;                      /* +0x08 */
    SefCurveSegment *segment_b;                      /* +0x0c */
} SefCurveCursor;

/* The curve track: its cursor at +0x1e0 and its playback time at +0x248. */
typedef struct SefCurveTrack {
    unsigned char unmodeled_000[0x1e0];
    SefCurveCursor cursor;                           /* +0x1e0 */
    unsigned char unmodeled_1f0[0x248 - 0x1f0];
    union {
        float value;                                 /* +0x248: the playback time */
        int bits;
    } time;
} SefCurveTrack;

/* The curve state: the key table at +0x19c and the two 30-entry segment tables
 * (the second starts at +0x570). */
typedef struct SefCurveState {
    unsigned char unmodeled_000[0x19c];
    SefCurveKey *keys;                               /* +0x19c */
    unsigned char unmodeled_1a0[0x1b0 - 0x1a0];
    SefCurveSegment segments_a[30];                  /* +0x1b0 */
    SefCurveSegment segments_b[30];                  /* +0x570 */
} SefCurveState;

extern int sefRequestFlags[];

/* Steps a curve track by one tick: with advance < 0 the cursor is reset to the first segments and the time to 0.
 * Otherwise, when the scan flag 8 is set, the cursor moves to the first key with a negative link from the current one;
 * else, once the whole time reaches the current segment's value, the key's link decides: a link >= 0 jumps there
 * (rebasing the time, result 0x1000), -1 steps to the next key (result 1), -2 holds, any other link holds unless the
 * curve ends (result 0). The time then advances by delta. Returns the result. */
int sefStepCurveTrack(SefCurveState *state, SefCurveTrack *track, int advance, float delta)
{
    SefCurveCursor *cursor = &track->cursor;
    float time = track->time.value;
    int frame = (int)time;
    unsigned int curveFlags = sefRequestFlags[0];
    SefCurveKey *keys = state->keys;
    int key = cursor->index;
    SefCurveKey *current = keys + key;
    int result = 0;
    SefCurveSegment *segmentsA = state->segments_a;
    SefCurveSegment *segmentsB = state->segments_b;
    unsigned short scanKeyTime;
    short currentKeyTime;
    int link;
    int next_key;

    if (advance < 0) {
        cursor->segment_a = segmentsA;
        cursor->segment_b = segmentsB;
        cursor->index = 0;
        track->time.value = 0.0f;
        return 0;
    }
    currentKeyTime = current->key;
    scanKeyTime = current->key;
    if (currentKeyTime == SEF_TRACK_END_KEY) {
        return 0;
    }
    if ((curveFlags & 8) != 0) {
        for (;; key++, current = keys + key, scanKeyTime = current->key) {
            int scan_link = current->link;

            if ((short)scanKeyTime == SEF_TRACK_END_KEY) {
                return 0;
            }
            if (scan_link >= 0) {
                if (current[1].key == SEF_TRACK_END_KEY) {
                    continue;
                }
                return 0;
            }
            next_key = key + 1;
            break;
        }
        result = 1;
        cursor->index = next_key;
        cursor->segment_a = &segmentsA[next_key];
        cursor->segment_b = &segmentsB[next_key];
    } else if (cursor->segment_a->value <= (float)frame) {
        link = current->link;
        if (link >= 0) {
            cursor->index = link;
            result = 0x1000;
            track->time.value = segmentsA[link].value - segmentsA[0].value;
            key = cursor->index;
        } else if (link == -1) {
            if (current[1].key != SEF_TRACK_END_KEY) {
                cursor->index = key + 1;
                result = 1;
                key = cursor->index;
            }
        } else if (link != -2) {
            if (current[1].key == SEF_TRACK_END_KEY) {
                return 0;
            }
        }
        cursor->segment_a = &segmentsA[key];
        cursor->segment_b = &segmentsB[key];
    }
    track->time.value = track->time.value + delta;
    return result;
}


/* Segment-sphere test: segment is end - start, offset is start - anchor, and
 * the discriminant of the quadratic in the segment parameter decides whether
 * the segment reaches the sphere of the given radius. */
int sefSegmentHitsSphere(void *anchor, void *start, void *end, f32 radius)
{
    Vector4 offset;
    Vector4 segment;
    Vector4 *segment_ptr;
    f32 segment_sq;
    f32 segment_dot_offset;

    segment_ptr = &segment;
    Vu0SefSubVectorXYZ(segment_ptr, end, start);
    Vu0SefSubVectorXYZ(&offset, start, anchor);
    segment_sq = MMathVectorDotProduct(segment_ptr, segment_ptr);
    segment_dot_offset = MMathVectorDotProduct(segment_ptr, &offset);
    if (((segment_dot_offset * segment_dot_offset)
         - (segment_sq * 4.0f * (MMathVectorDotProduct(&offset, &offset) - (radius * radius)))) >= 0.0f) {
        return 1;
    }
    return 0;
}

typedef unsigned int Quadword __attribute__((mode(TI)));

/* The seven key tables of a line at +0x670 (the scale, angle, colour, alpha, effect-rate, part and size tracks). */
typedef struct SefLineKeySet {
    SefTrackKey16 *scaleKeys;                        /* +0x00 */
    SefTrackKey16 *angleKeys;                        /* +0x04: its header shorts sit before the first key */
    SefTrackKey16 *colorKeys;                        /* +0x08 */
    SefTrackKey16 *alphaKeys;                          /* +0x0c */
    SefTrackKey16 *spinKeys;                           /* +0x10 */
    SefTrackKey16 *partKeys;                         /* +0x14 */
    SefTrackKey16 *sizeKeys;                           /* +0x18 */
} SefLineKeySet;

/* The scratchpad block the line step fills once per line (0x70000060..0x7000013f): the point work vectors, the
 * line's key tables, the time step and the motion parameters sefMoveParticle reads, and the line's switches. */
typedef struct SefLineScratch {
    unsigned char unmodeled_000[0x60];
    Vector4 position;                                /* +0x060: the point's base position */
    Vector4 target;                                  /* +0x070: the part-transformed target */
    Vector4 placement;                               /* +0x080 */
    unsigned char unmodeled_090[0xa0 - 0x90];
    Vector4 placementPoint;                          /* +0x0a0 */
    Vector4 anglePoint;                              /* +0x0b0 */
    SefTrackKey16 *scaleKeys;                        /* +0x0c0 */
    SefTrackKey16 *angleKeys;                        /* +0x0c4 */
    SefTrackKey16 *sizeKeys;                           /* +0x0c8 */
    SefTrackKey16 *alphaKeys;                          /* +0x0cc */
    SefTrackKey16 *colorKeys;                        /* +0x0d0 */
    SefTrackKey16 *directionKeys;                    /* +0x0d4 */
    DoublewordVector4 *imageOffset;                  /* +0x0d8 */
    Vector4 *drift;                                  /* +0x0dc */
    float timeScale;                                 /* +0x0e0 */
    float step;                                      /* +0x0e4 */
    float gravity;                                   /* +0x0e8 */
    float bounce;                                    /* +0x0ec */
    float floor;                                     /* +0x0f0 */
    int fixedGravity;                                /* +0x0f4 */
    int entryId;                                     /* +0x0f8 */
    int angleKind;                                   /* +0x0fc */
    int placementKind;                               /* +0x100 */
    int angleParameter;                              /* +0x104 */
    int directionMode;                               /* +0x108 */
    int sameKind;                                    /* +0x10c */
    int attachedKind;                                /* +0x110 */
    short attached;                                  /* +0x114 */
    short partPlaced;                                /* +0x116 */
    short rotate;                                    /* +0x118 */
    short offsetImage;                               /* +0x11a */
    int constAngle;                                  /* +0x11c */
    int constDirection;                              /* +0x120 */
    int constScale;                                  /* +0x124 */
    int constColor;                                  /* +0x128 */
    int constSize;                                   /* +0x12c */
    short placed;                                    /* +0x130 */
    int partMode;                                    /* +0x134 */
    int billboard;                                   /* +0x138 */
    int moving;                                      /* +0x13c */
} SefLineScratch;

/* vsub.xyz of two vectors into destination; both of its words are outputs. */
#define SefSubVectorXYZ(destination, minuend, subtrahend) \
    __asm__ __volatile__("lqc2 vf1, 0(%4)\n" \
                         "lqc2 vf2, 0(%5)\n" \
                         "vsub.xyz vf1, vf1, vf2\n" \
                         "sqc2 vf1, 0(%6)\n" \
                         : "=m"((destination)->x), "=m"((destination)->y), "=m"((destination)->z), "=m"((destination)->w) \
                         : "r"(minuend), "r"(subtrahend), "r"(destination) \
                         : "memory")
#define SEF_LINE_SCRATCH ((SefLineScratch *) 0x70000000)

typedef union SefKeyBits {
    int bits;
} SefKeyBits;

/* A point's integer colour, copied whole from its colour cursor. */
typedef union SefPointColor {
    int lane[4];
    Quadword quad;
} SefPointColor;

/* One point of a line (the data of a sefPtAllocatorState entry) as the line step reads it. */
typedef struct SefPointState {
    unsigned char unmodeled_000[0x140];
    Vector4 position;                                /* +0x140 */
    unsigned char unmodeled_150[0x160 - 0x150];
    Vector4 scale;                                   /* +0x160 */
    unsigned char unmodeled_170[0x190 - 0x170];
    Vector4 offset;                                  /* +0x190: position relative to the line point */
    Vector4 baseScale;                               /* +0x1a0 */
    Vector4 colorScale;                              /* +0x1b0 */
    unsigned char unmodeled_1c0[0x1d0 - 0x1c0];
    SefPointColor color;                             /* +0x1d0 */
    unsigned char unmodeled_1e0[0x1f0 - 0x1e0];
    Vector4 drift;                                   /* +0x1f0 */
    SefTrackCursor scaleTrack;                       /* +0x200 */
    float speed;                                     /* +0x20c */
    Vector4 scaleValue;                              /* +0x210 */
    SefTrackCursor angleTrack;                       /* +0x220 */
    unsigned char unmodeled_22c[0x240 - 0x22c];
    short directionIndex;                            /* +0x240 */
    short unmodeled_242;
    float directionTime;                             /* +0x244 */
    float age;                                       /* +0x248 */
    float life;                                      /* +0x24c */
    Vector4 direction;                               /* +0x250 */
    short colorIndex;                                /* +0x260 */
    short unmodeled_262;
    float colorTime;                                 /* +0x264 */
    short frame;                                     /* +0x268 */
    short link;                                      /* +0x26a */
    float time;                                      /* +0x26c */
    SefPointColor colorValue;                        /* +0x270 */
    SefTrackCursor partTrack;                        /* +0x280 */
    unsigned char unmodeled_28c[0x2a0 - 0x28c];
    SefTrackCursor spinTrack;                        /* +0x2a0 */
    SefTrackCursor sizeTrack;                        /* +0x2ac */
    SefTrackCursor alphaTrack;                       /* +0x2b8 */
} SefPointState;

int sefLineFlag4000Count = 0;
int sefVectorMode = 0;
int sefCameraOwnerRecord = 0;
int sefEffectRecordCount = 64;
int sefLineDataCount = 192;
int sefPtAllocCount = 1100;
int sefEffectListSavedFlag = 0;
int sefParentLine = -1;
s16 sefInitialized = 0;
int sefGateOwnerZeroFlag = 0;
s16 _hitFlag = 0;
s16 _hitSignal = 0;
s16 _seSignal = 0;
s16 _sefBattleMode = 0;
float sefTimeScale = 1.0f;
float sefPointTimeScale = 1.0f;
f32 sefTrackTimeStep = 1.0f;
float sefStepScale = 1.0f;
float *atanTbl_0 = (float *) 0x20470d80;
int sefEffectRecordHead = 0;
extern int sefParentLine;
extern int sefLinkedPointHandle;

float sefIdleStartFrame = -0.100000001f;
extern SefEffectRecord *sefCurrentRecord;
struct SefPartPlacement;
void sefTransformPartPoint(SefTransformState *part, Vector4 *out, int mode, const Vector4 *in);
float sefPlacePart(struct SefPartPlacement *instance, Vector4 *out, int mode);
void sefAccumulateEntryInfluence(Vector4 *position, int *offset, float radius);

/* Returns the point of handle, or 0 for a free (negative) handle. */
static inline SefPointState *sefPointState(int handle)
{
    if (handle < 0) {
        return 0;
    }
    return (SefPointState *) sefPtAllocatorState[handle].data;
}

/* Steps the 256 points of the line one frame: fills the line's scratchpad block, then for every live point advances
 * its life, samples its tracks, moves it and frees it once its life runs out. Adds the stepped points to the entry's
 * counters and returns their number. */
typedef union SefModeWord { short value; unsigned short bits; } SefModeWord;

int sefExecLineLocalData(SefLocalData *data, int frame)
{
    SefLineScratch *scratch;
    SefLineKeySet *keys;
    SefTrackKey16 *angleKeys;
    SefPointState *point;
    SefPointState *linked;
    float spin;
    int pointFlags;
    int count;
    int i;
    int link;
    int handle;
    DoublewordVector4 *imageOffset;
    unsigned char *requestBytes;
    int useLower;
    int countWeight;
    int directionBits;

    scratch = SEF_LINE_SCRATCH;
    keys = (SefLineKeySet *) &data->scaleKeys;
    count = 0;
    imageOffset = &data->imageOffset;
    scratch->scaleKeys = keys->scaleKeys;
    scratch->entryId = sefCurrentRecord->entryId;
    angleKeys = keys->angleKeys;
    scratch->drift = &data->drift;
    scratch->angleKeys = angleKeys;
    scratch->timeScale = sefPointTimeScale;
    scratch->imageOffset = imageOffset;
    scratch->sizeKeys = keys->sizeKeys;
    scratch->alphaKeys = keys->alphaKeys;
    scratch->colorKeys = keys->colorKeys;
    scratch->directionKeys = data->keys[4];
    scratch->angleKind = angleKeys != 0 ? ((short *) angleKeys)[-2] : 0;
    scratch->placementKind = scratch->angleKeys != 0 ? ((short *) scratch->angleKeys)[-9] : 0;
    scratch->angleParameter = scratch->angleKeys != 0 ? ((short *) scratch->angleKeys)[-1] : 0;
    directionBits = ((SefModeWord *) &data->directionMode)->bits;
    scratch->attachedKind = (unsigned short) ((short *) data->keys[0])[-2] - 16U < 2;
    scratch->sameKind = directionBits == scratch->angleKind;
    scratch->directionMode = directionBits;
    scratch->attached = data->in_use & 1;
    scratch->partPlaced = (data->in_use & 0x1000) && keys->partKeys != 0;
    scratch->rotate = data->imageKind != 4 || scratch->angleParameter != 0;
    scratch->offsetImage = (((long *) imageOffset)[1] | ((long *) imageOffset)[0]) != 0;
    scratch->constAngle = ((SefKeyBits *) &data->constantKeys)->bits & 1;
    scratch->constDirection = ((SefKeyBits *) &data->constantKeys)->bits & 2;
    scratch->constScale = ((SefKeyBits *) &data->constantKeys)->bits & 0x10;
    scratch->constColor = ((SefKeyBits *) &data->constantKeys)->bits & 8;
    scratch->constSize = ((SefKeyBits *) &data->constantKeys)->bits & 0x20;
    scratch->gravity = sefPointMotion.gravity;
    scratch->fixedGravity = sefPointMotion.fixedGravity;
    scratch->bounce = sefPointMotion.bounce;
    scratch->floor = sefPointMotion.floor;
    scratch->moving = data->in_use & 8;
    scratch->placed = data->imageFlags & 0x10;
    scratch->partMode = -1;
    if (keys->partKeys != 0) {
        scratch->partMode = ((signed char *) keys->partKeys)[15];
    }
    scratch->billboard = data->in_use & 0xC00;
    if (scratch->placementKind >= 32) {
        sefGetPoint(&scratch->placementPoint, 0, scratch->placementKind);
    }
    if (scratch->angleKind >= 32) {
        sefGetPoint(&scratch->anglePoint, 0, scratch->angleKind);
    }
    for (i = 0; i < SEF_LINE_PARTICLE_COUNT; i++) {
        handle = data->pointHandles[i];
        if (handle < 0) {
            continue;
        }
        point = (SefPointState *) sefPtAllocatorState[handle].data;
        if (point == 0) {
            continue;
        }
        sefTrackTimeStep = scratch->step = scratch->timeScale * point->speed;
        if (point->life < 2048.0f) {
            point->life -= scratch->step;
        }
        if (point->life >= 0.0f) {
            spin = 0.0f;
            pointFlags = scratch->sameKind;
            if (scratch->partPlaced) {
                sefTransformPartPoint((SefTransformState *) point, &scratch->position, scratch->partMode, 0);
            } else {
                QuadwordCopyT0(&scratch->position, &point->position);
            }
            if (!scratch->constColor) {
                sefSampleColorTrack(scratch->colorKeys, (SefColorTrackCursor *) &point->colorIndex,
                                    &point->colorScale);
            }
            if (scratch->alphaKeys != 0) {
                sefSampleTrackTruncated((SefTrackKey *) scratch->alphaKeys, &point->alphaTrack);
            }
            if (!scratch->constSize) {
                sefSampleTrack((SefTrackKey *) scratch->sizeKeys, &point->sizeTrack);
            }
            if (keys->partKeys != 0) {
                sefLerpVector(keys->partKeys, (SefVectorTrackCursor *) &point->partTrack);
            }
            if (!scratch->constAngle &&
                sefLerpVector(scratch->angleKeys, (SefVectorTrackCursor *) &point->angleTrack)) {
                sefSetTransformRotation((SefTransformState *) point, scratch->rotate);
            }
            if (!scratch->constDirection &&
                sefLerpVector(scratch->directionKeys, (SefVectorTrackCursor *) &point->directionIndex)) {
                MMathNormalizeVector2(&point->direction, &point->direction);
            }
            link = point->link;
            sefLinkedPointHandle = link;
            if (sefParentLine >= 0 && link >= 0) {
                linked = (SefPointState *) sefPtAllocatorState[link].data;
            } else {
                linked = 0;
            }
            sefLinkedPoint = (SefTransformState *) linked;
            if (linked != 0 && linked->life == 0.0f) {
                sefLinkedPointHandle = -1;
                point->link = -1;
                sefLinkedPoint = 0;
            }
            if (!scratch->constScale &&
                sefLerpVector(scratch->scaleKeys, (SefVectorTrackCursor *) &point->scaleTrack)) {
                Vu0SefMulVectorXYZ(&point->scale, &point->scaleValue, &point->baseScale);
            }
            point->color.quad = point->colorValue.quad;
            point->color.lane[3] = point->alphaTrack.sampled_s16;
            if (scratch->offsetImage) {
                sefAddVectorFraction(&point->drift, scratch->imageOffset);
            }
            if (scratch->placed) {
                sefAccumulateEntryInfluence(&scratch->placement, point->color.lane,
                              point->scaleValue.x * sefPlacePart((struct SefPartPlacement *) point, &scratch->placement,
                                                                  scratch->partMode));
            }
            if (scratch->moving) {
                sefStepCurveTrack((SefCurveState *) data, (SefCurveTrack *) point, 0, scratch->step);
            } else {
                point->age += scratch->step;
            }
            if (keys->spinKeys != 0) {
                sefSampleTrack((SefTrackKey *) keys->spinKeys, &point->spinTrack);
                spin = point->spinTrack.sampled;
                if (spin != 0.0f) {
                    pointFlags |= 2;
                }
            }
            if (scratch->attached) {
                if (scratch->attachedKind && sefLinkedPoint != 0) {
                    if (((SefTransformState *) point)->flags & 0x10) {
                        Vu0SefAddVectorXYZ(&point->position, &point->position, &((SefPointState *) sefLinkedPoint)->offset);
                    } else {
                        ((SefTransformState *) point)->flags |= 0x10;
                    }
                } else if (((SefTransformState *) point)->flags & 0x10) {
                    Vu0SefAddVectorXYZ(&point->position, &point->position, scratch->drift);
                } else {
                    ((SefTransformState *) point)->flags |= 0x10;
                }
            }
            sefMoveParticle((SefTransformState *) point, scratch->directionMode, (int) point->life, spin);
            if (scratch->rotate) {
                if (scratch->attached) {
                    ((SefTransformState *) point)->flags &= ~4;
                }
                sefCalcLocalMatrix((SefTransformState *) point, scratch->angleKind, scratch->placementKind,
                              pointFlags);
            }
            if (scratch->partPlaced) {
                sefTransformPartPoint((SefTransformState *) point, &scratch->target, scratch->partMode, 0);
                SefSubVectorXYZ(&point->offset, &scratch->target, &scratch->position);
            } else {
                SefSubVectorXYZ(&point->offset, &point->position, &scratch->position);
            }
            count++;
        } else {
            point->life = 0.0f;
            sefFreeLocalData(data, i);
        }
    }
    if (data->imageKind == 4) {
        useLower = ((short *) data->keys[0])[-2] == 8 || scratch->directionMode == 8;
        countWeight = scratch->angleParameter != 0 ? 17 : 14;
        if (data->imageFlags & 0x4000) {
            countWeight *= 2;
        }
        countWeight *= count;
        if (useLower) {
            sefEntryCounts[scratch->entryId].screenSprites += countWeight;
        } else {
            sefEntryCounts[scratch->entryId].worldSprites += countWeight;
        }
    } else {
        sefEntryCounts[scratch->entryId].modelPoints += count;
    }
    requestBytes = sefRequestBuffer;
    if ((*(long *) (requestBytes + 0x308) & 0x880) == 0x80) {
        return 1;
    }
    if ((*(int *) (requestBytes + 0x308) & 0x800) && count <= 0) {
        data->startFrame = sefIdleStartFrame;
    }
    return count;
}

/* Frees particle handle `index` of the record when it is in range and
 * returns -1; out of range, returns the handle's byte offset in the record. */
int sefFreeLocalData(SefLocalData *data, unsigned int index) {
    int in_range = index < SEF_LINE_PARTICLE_COUNT;
    int offset = (int)(index * sizeof(short)) + SEF_LOCAL_DATA_POINT_HANDLES;

    if (in_range != 0) {
        short *handle = (short *)((unsigned char *)data + offset);

        sevFreePtAllocator(*handle);
        offset = -1;
        *handle = -1;
    }
    return offset;
}

/* Claims the next free particle handle of the record from its cursor: the scan
 * stops at the first free handle before the cursor wraps to its start, and the
 * cursor moves past the claimed index. Returns the index, or -1 when none is free. */
int sefAllocLocalData(SefLocalData *data)
{
    int index;
    int end;
    int handle;

    index = data->cursor;
    end = (index - 1) & (SEF_LINE_PARTICLE_COUNT - 1);
    for (; index != end; index = (index + 1) & (SEF_LINE_PARTICLE_COUNT - 1)) {
        if (data->pointHandles[index] < 0) {
            break;
        }
    }
    if (index == end) {
        return -1;
    }
    handle = sevAllocPtAllocator();
    if (handle < 0) {
        return -1;
    }
    data->pointHandles[index] = handle;
    data->cursor = (index + 1) & (SEF_LINE_PARTICLE_COUNT - 1);
    return index;
}

void sefDestroyLocalData(SefLocalData *data)
{
    int i;

    for (i = 0; i < SEF_LINE_PARTICLE_COUNT; i++) {
        if (data->pointHandles[i] >= 0) {
            sefFreeLocalData(data, i);
        }
    }
}

/* The line script header: the two table indices scGetImmAdrImmIdx2 reads, then the scalars. */
typedef struct SefLineScript {
    unsigned short tables[2];                        /* +0x00 */
    float gravity;                                   /* +0x04 */
    float height;                                    /* +0x08 */
    short collision;                                 /* +0x0c */
    short noAttach;                                  /* +0x0e */
} SefLineScript;

static int sefCurrentLoadTag;
float sefStartFrameEpsilon = 9.99999975e-06f;
/* The z rotations for the two axis-aligned directions sefDiagonalPositiveDirection and sefDiagonalNegativeDirection. */
extern int sefVectorMode;
Vector4 sefDiagonalPositiveDirection = { .x = 1.0f, .y = 1.0f, .z = 0.0f, .w = 0.0f };
Vector4 sefDiagonalNegativeDirection = { .x = 1.0f, .y = -1.0f, .z = 0.0f, .w = 0.0f };
struct SefAnimObject;
int sefAnalyzeAnim(struct SefAnimObject *object);

/* Copies the xyz of a constant key and sets w to 1. */
static inline void sefCopyKeyVector(Vector4 *destination, SefTrackKey16 *key)
{
    __asm__ __volatile__(
        "lqc2 vf2, 0(%1)\n"
        "vmove.w vf2, vf0\n"
        "sqc2 vf2, 0(%0)\n"
        :
        : "r" (destination), "r" (key)
        : "memory");
}

/* Nonzero test of all four lanes, as two doublewords. */
static inline int sefIsVectorNonZero(DoublewordVector4 *vector)
{
    return (vector->words[1] | vector->words[0]) != 0;
}

void sefInitLineData(SefLocalData *line, int startTime, int frame, SefLineScript *script,
                   int link, int count, int flags)
{
    SefTrackKey16 *keys;
    unsigned short *lineTable;
    SefTrackKey16 **keyTable;
    Vector4 *origin;
    Vector4 direction;
    float gravity;
    float height;
    short noAttach;
    short collision;
    int hasDirection;
    unsigned short imageFlags;
    int positionMode;

    keyTable = (SefTrackKey16 **)scGetImmAdrImmIdx2(sefCurrentLoadTag, script->tables, 0);
    lineTable = (unsigned short *)scGetImmAdrImmIdx2(sefCurrentLoadTag, script->tables, 1);
    gravity = script->gravity;
    height = script->height;
    collision = script->collision;
    noAttach = script->noAttach;
    sefMemZero(line, SEF_LINE_DATA_STRIDE);
    sefMemFillWords(line->pointHandles, -1, sizeof(line->pointHandles));
    line->cursor = 0;
    line->link = link;
    line->pointCount = 0;
    line->frame = 0;
    line->mode = count == 0;
    if (frame == 0) {
        line->mode |= 2;
    }
    line->gravity = gravity;
    line->collision = collision;
    line->height = height;
    line->emitFrame = -0x800;
    line->in_use = line->in_use | flags | 0x20;
    line->count = 0;
    if (frame != 0) {
        line->startFrame = (float)(frame - startTime) - sefStartFrameEpsilon;
    }
    if (noAttach != 0) {
        line->in_use |= 0x100;
    }
    line->keys[0] = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *)keyTable, 0);
    line->keys[1] = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *)keyTable, 1);
    line->keys[2] = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *)keyTable, 2);
    line->keys[3] = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *)keyTable, 3);
    line->keys[4] = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *)keyTable, 4);
    line->keys[5] = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *)keyTable, 5);
    line->keys[6] = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *)keyTable, 6);
    sefNoiseNextVector(&line->noise);
    Vu0SefMulVectorXYZ(&line->noise, &line->noise, (Vector4 *)&line->keys[0][-1]);
    if (line->keys[1] != 0 && line->keys[1][1].key == SEF_TRACK_END_KEY) {
        sefCopyKeyVector(&line->rotationValue, line->keys[1]);
    }
    if (line->keys[3] != 0 && line->keys[3][1].key == SEF_TRACK_END_KEY) {
        sefCopyKeyVector(&line->scaleValue, line->keys[3]);
    }
    if (line->keys[0] != 0 && line->keys[0][1].key == SEF_TRACK_END_KEY) {
        sefCopyKeyVector(&line->positionValue, line->keys[0]);
    }
    if (line->keys[2] != 0 && line->keys[2][1].key == SEF_TRACK_END_KEY) {
        sefCopyKeyVector(&line->velocityValue, line->keys[2]);
    }
    if ((short)lineTable[0] == 0x800) {
        sefCurrentRecord->flags |= 0x4000000;
    }
    line->pointLife = lineTable[0];
    line->speedVariation = lineTable[2];
    line->scaleKeys = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, lineTable, 4);
    line->angleKeys = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, lineTable, 5);
    line->colorKeys = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, lineTable, 6);
    line->alphaKeys = (SefTrackKey *)scGetImmAdrImmIdx2(sefCurrentLoadTag, lineTable, 7);
    line->partKeys = (void *)scGetImmAdrImmIdx2(sefCurrentLoadTag, lineTable, 8);
    line->spinKeys = (void *)scGetImmAdrImmIdx2(sefCurrentLoadTag, lineTable, 9);
    line->sizeKeys = (SefTrackKey *)scGetImmAdrImmIdx2(sefCurrentLoadTag, lineTable, 11);
    line->images = (SefTrackKey16 *)scGetImmAdrImmIdx2(sefCurrentLoadTag, lineTable, 10);
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (&line->matrix), "r" (MMathUnitMatrix)
        : "memory");
    if (line->angleKeys != 0) {
        origin = (Vector4 *)&line->angleKeys[-2];
        hasDirection = (*(int *)&origin->z | *(u64 *)origin) != 0;
        if (hasDirection) {
            if (MMathIsVectorEqual(origin, &sefDiagonalPositiveDirection)) {
                line->in_use |= 0x40;
                MMathRotateMatrixZ(&line->matrix, 0, SEF_QUARTER_PI);
            } else if (MMathIsVectorEqual(origin, &sefDiagonalNegativeDirection)) {
                line->in_use |= 0x40;
                MMathRotateMatrixZ(&line->matrix, 0, -SEF_QUARTER_PI);
            } else if ((short)sefGetDirMatrix(&line->matrix, (Vector4 *)&line->angleKeys[-2],
                                                  sefDirectionAxes) != 0) {
                line->in_use |= 0x40;
            }
        }
        if (line->angleKeys[1].key == SEF_TRACK_END_KEY) {
            line->constantKeys |= 1;
        }
    }
    if (line->sizeKeys != 0) {
        if (line->sizeKeys[1].key == SEF_TRACK_END_KEY) {
            line->constantKeys |= 0x20;
        }
        line->scaleTime = line->sizeKeys[-1].value;
    }
    if (line->alphaKeys != 0 && line->alphaKeys[1].key == SEF_TRACK_END_KEY) {
        line->constantKeys |= 0x40;
    }
    if (line->scaleKeys != 0 && line->scaleKeys[1].key == SEF_TRACK_END_KEY) {
        line->constantKeys |= 0x10;
    }
    if (line->colorKeys != 0 && line->colorKeys[1].key == SEF_TRACK_END_KEY) {
        line->constantKeys |= 8;
    }
    keys = line->keys[4];
    if (keys != 0) {
        line->directionMode = (unsigned short)keys[-1].key;
        if (keys[1].key == SEF_TRACK_END_KEY) {
            sefCopyKeyVector(&direction, keys);
            if (*(u64 *)&direction == 0) {
                if (direction.z == 1.0f) {
                    line->in_use |= 0x400;
                } else if (direction.z == -1.0f) {
                    line->in_use |= 0x800;
                }
            }
            MMathNormalizeVector2(&line->direction, &direction);
            line->constantKeys |= 2;
        }
    }
    if (line->images != 0) {
        line->imageOffset.vector.x = ((Vector4 *)line->images)[-1].x;
        line->imageOffset.vector.y = ((Vector4 *)line->images)[-1].y;
        line->imageOffset.vector.z = ((Vector4 *)line->images)[-1].z;
        line->imageOffset.vector.w = ((Vector4 *)line->images)[-1].w;
        if (sefIsVectorNonZero(&line->imageOffset)) {
            line->in_use |= 0x200;
        }
    }
    if (sefAnalyzeAnim((struct SefAnimObject *) line) >= 2) {
        line->in_use |= 8;
    }
    imageFlags = line->imageFlags;
    if (imageFlags & 0x20) {
        line->in_use |= 0x4000;
    }
    if (imageFlags & 0x80) {
        line->in_use |= 0x80;
    }
    positionMode = (unsigned short)line->keys[0][-1].key;
    if (positionMode != 16 && positionMode != 17) {
        sefVectorMode = 0;
        sefGetPosition(&line->position, 0, (short)positionMode);
    }
}

static unsigned char sefLineDataPool[0x68400];
SefLocalData *sefLineDataRecords = (SefLocalData *) sefLineDataPool;
extern int sefLineDataCount;

int sefAllocLineData(void)
{
    SefLocalData *line;
    int index;

    index = 0;
    if (index < sefLineDataCount) {
        line = sefLineDataRecords;
        do {
            if ((unsigned short)line->in_use == 0) {
                line->in_use = 32;
                return index;
            }
            line++;
        } while (++index < sefLineDataCount);
    }
    return -1;
}

void sefFreeLineData(int line_index) {
    if (line_index >= 0) {
        if (line_index < sefLineDataCount) {
            SefLocalData *line = 0;

            if (line_index >= 0) {
                line = sefLineDataRecords + line_index;
            }
            sefDestroyLocalData(line);
            line->in_use = 0;
        }
    }
}

#include "gs.h"
#define SEF_IMAGE_PACKET_COUNT 30
#define SEF_ALT_PACKET_COUNT 8
/* GS pixel formats of an indexed texture: 8-bit (PSMT8) and 4-bit (PSMT4). */
#define SEF_GS_PSMT8 0x13
#define SEF_GS_PSMT4 0x14

/* One 12-byte entry of an object's animation list (SefAnalyzeRecord): time is the
 * key, 1024 ends the list; the flags byte is read from the list head only. */
typedef struct SefAnalyzeRecord {
    short time;                                      /* +0x00 */
    unsigned char unmodeled_002[1];
    unsigned char flags;                             /* +0x03 */
    short imageId;                                   /* +0x04 */
    short altImageId;                                /* +0x06 */
    short imageId2;                                  /* +0x08 */
    short altImageId2;                               /* +0x0a */
} SefAnalyzeRecord;

/* One 32-byte image packet: the first table has time, flags and resourceKind,
 * the second table only the image pair and the size. */
typedef struct SefImagePacket {
    float scaledWidth;                               /* +0x00 */
    float scaledHeight;                              /* +0x04 */
    const void *image;                               /* +0x08 */
    const void *palette;                             /* +0x0c */
    unsigned long long tex0;                         /* +0x10 */
    float time;                                      /* +0x18 */
    unsigned short flags;                            /* +0x1c */
    unsigned short resourceKind;                     /* +0x1e */
} SefImagePacket;

/* The object: its animation list at +0x19c, the 30-packet table at +0x1b0 and
 * the 8-packet alternate table after it at +0x570. */
typedef struct SefAnimObject {
    unsigned char unmodeled_000[0x19c];
    SefAnalyzeRecord *records;                       /* +0x19c */
    unsigned char unmodeled_1a0[0x1b0 - 0x1a0];
    SefImagePacket packets[SEF_IMAGE_PACKET_COUNT];  /* +0x1b0 */
    SefImagePacket altPackets[SEF_ALT_PACKET_COUNT]; /* +0x570 */
} SefAnimObject;

void *memset(void *destination, int value, unsigned int size);

int sefAnalyzeAnim(SefAnimObject *object)
{
    int packetCount;
    SefAnalyzeRecord *records;
    SefImagePacket *packet;
    SefImagePacket *altPacket;
    short *altPaletteId;
    int psm;

    records = object->records;
    packetCount = 0;
    if (records->time != SEF_LIST_END) {
    altPacket = object->altPackets;
    packet = object->packets;
    do {
        int flags;
        short mapperIndex;
        float animScale;
        SvTypeListEntry *image;
        SvTypeListEntry *palette;

        flags = records->flags;
        mapperIndex = sefCurrentRecord->entryId;
        if ((flags & 0x10) != 0) {
            if (records[packetCount].imageId == 0 || records[packetCount].altImageId == 0) {
                sefMemClear(packet, 0x20);
                packet->resourceKind = 4;
                flags |= 0x2000;
                packet->flags = flags;
                break;
            }
        }
        if ((records[packetCount].imageId & 0x100) != 0) {
            flags |= 0x2000;
            image = svGetImageListItem(0, records[packetCount].imageId);
            palette = svGetImageListItem(0, records[packetCount].altImageId);
        } else {
            image = svGetImageListItem(mapperIndex, records[packetCount].imageId);
            palette = svGetImageListItem(mapperIndex, records[packetCount].altImageId);
        }
        if (image != 0 && palette != 0) {
            if (packetCount < SEF_IMAGE_PACKET_COUNT) {
                packet->flags = flags;
                animScale = SEF_IMAGE_PACKET_SCALE;
                packet->time = (float)records[packetCount].time;
                packet->scaledWidth = (float)image->width * animScale;
                packet->scaledHeight = (float)image->height * animScale;
                packet->resourceKind = image->resourceKind;
                psm = image->bitsPerPixel == 8 ? SEF_GS_PSMT8 : SEF_GS_PSMT4;
                packet->image = image->data;
                packet->palette = palette->data;
                packet->tex0 = GS_SET_TEX0(image->basePointer, image->bufferWidthPixels >> 6,
                    psm, image->textureWidthExponent,
                    image->textureHeightExponent, 1, 0, palette->basePointer, 0, 0, 0, 1);
                if (records[packetCount].imageId2 != 0 && *(altPaletteId = &records[packetCount].altImageId2) != 0) {
                    image = svGetImageListItem(mapperIndex, records[packetCount].imageId2);
                    palette = svGetImageListItem(mapperIndex, *altPaletteId);
                    if (image != 0 && palette != 0) {
                        if (packetCount < SEF_ALT_PACKET_COUNT) {
                            packet->flags |= 0x4000;
                            packet[SEF_IMAGE_PACKET_COUNT].image = image->data;
                            packet[SEF_IMAGE_PACKET_COUNT].scaledWidth = (float)image->width * animScale;
                            packet[SEF_IMAGE_PACKET_COUNT].scaledHeight = (float)image->height * animScale;
                            packet[SEF_IMAGE_PACKET_COUNT].resourceKind = image->resourceKind;
                            packet[SEF_IMAGE_PACKET_COUNT].palette = palette->data;
                            psm = image->bitsPerPixel == 8 ? SEF_GS_PSMT8 : SEF_GS_PSMT4;
                            packet[SEF_IMAGE_PACKET_COUNT].tex0 = GS_SET_TEX0(image->basePointer,
                                image->bufferWidthPixels >> 6, psm, image->textureWidthExponent,
                                image->textureHeightExponent, 1, 0, palette->basePointer, 0, 0, 0, 1);
                        }
                    } else {
                        memset(altPacket, 0, 0x20);
                    }
                }
            }
        } else {
            memset(packet, 0, 0x20);
        }
        altPacket++;
        packet++;
        packetCount++;
    } while (records[packetCount].time != SEF_LIST_END);
    }
    return packetCount;
}

/* One 16-byte key record of a line: read as a vector, or as three floats and two shorts. A key pointer points one
 * record past its header record (the header is read at index -1). */
typedef union SefKeyRecord {
    Vector4 vector;
    struct {
        float x;
        float y;
        float z;
        short kind;
        short mode;
    } header;
} SefKeyRecord;

/* The transform state of an effect: the 4x4 matrix the Z path writes, the
 * 3x4 rows the rotation paths read and write, the base angle and the angles
 * vector the rotation uses, and the flag word whose 0x8 bit selects the
 * stored-rows path. */
/* A particle word that is written as a float or an int. */

/* The motion record a particle follows; state 4 keeps the rotation fixed. */
typedef struct SefMotionState {
    unsigned char unmodeled_00[0x1e];
    short state;
} SefMotionState;

static Vector4 sefLinkedPositionVector;
f32 sefFrameJitterScale = 0.00999999978f;
f32 sefParticleDegToRad = 0.0174532924f;
struct SefLaneObject;
void sefGetStartPos(Vector4 *pos, struct SefLaneObject *object);
void sefGetTargetPos(Vector4 *pos, struct SefLaneObject *object);

/* Copies a key vector with w = 1. */
#define SefCopyKeyPoint(destination, source) \
    __asm__ __volatile__( \
        "lqc2 vf2, 0(%1)\n" \
        "vmove.w vf2, vf0\n" \
        "sqc2 vf2, 0(%0)\n" \
        : \
        : "r" (destination), "r" (source) \
        : "memory")
/* Sets all four lanes to 1. */
#define SefSetOnes(destination) \
    __asm__ __volatile__( \
        "vmaxw.xyzw vf1, vf0, vf0w\n" \
        "sqc2 vf1, 0(%0)\n" \
        : \
        : "r" (destination) \
        : "memory")

/* Non-zero when any of x, y, z of the vector is set (the w word is not read). */
static inline int sefVectorIsSet(Vector4 *vector)
{
    return (((int *) vector)[2] | *(long long *) vector) != 0;
}

/* The line data as sefCreateParticle reads it to spawn a particle: its key tables are union records. */
typedef struct SefLineSpawnData {
    unsigned char *position_keys;                    /* +0x000 */
    short *spread_rotation_keys;                     /* +0x004 */
    unsigned char *size_keys;                        /* +0x008 */
    unsigned char *signal_keys;                      /* +0x00c */
    unsigned char unmodeled_010[0x14 - 0x10];
    unsigned char *rotation_keys;                    /* +0x014 */
    unsigned char unmodeled_018[0x20 - 0x18];
    unsigned short initial_frame;                    /* +0x020 */
    short frame_count;                               /* +0x022 */
    unsigned char unmodeled_024[0x80 - 0x24];
    Vector4 tint;                                    /* +0x080: copied to the particle when flags bit 0x2 is set */
    unsigned char unmodeled_090[0xd8 - 0x90];
    float spinRange;                                 /* +0x0d8: scales the particle's random spin */
    unsigned char unmodeled_0dc[0x118 - 0xdc];
    int flags;                                       /* +0x118 */
    unsigned char unmodeled_11c[0x190 - 0x11c];
    union SefKeyRecord *velocityKey;                 /* +0x190 */
    unsigned char unmodeled_194[0x1e0 - 0x194];
    short *color_keys;                               /* +0x1e0 */
    short *uniform_scale_keys;                       /* +0x1e4 */
    unsigned char unmodeled_1e8[0x670 - 0x1e8];
    union SefKeyRecord *keys[6];                     /* +0x670: scale, rotation, color, texture, -, point */
    union SefKeyRecord *spinKey;                     /* +0x688 */
    short pointLife;                                 /* +0x68c */
    short speedVariation;                               /* +0x68e */
    unsigned char unmodeled_690[SEF_LOCAL_DATA_POINT_HANDLES - 0x690];
    short pointHandles[SEF_LINE_PARTICLE_COUNT];                         /* +0x6a0 */
    unsigned char unmodeled_8a0[0x8a2 - 0x8a0];
    short cursor;                                      /* +0x8a2 */
    short in_use;                                      /* +0x8a4 */
    unsigned char unmodeled_8a6[SEF_LINE_DATA_STRIDE - 0x8a6];
} SefLineSpawnData;

/* Creates one particle of the line: claims a particle handle, clears the particle, sets its start and target
 * positions and runs the first animation step, then applies the line's random rotation, spin, velocity, tint,
 * point, angle, texture, size and color keys. Returns the particle index, or -1 when none is free. */
int sefCreateParticle(SefLineSpawnData *data, int kind)
{
    Vector4 rotation;
    Vector4 sizeNoise;
    Vector4 colorNoise;
    SefKeyRecord **keys;
    SefTransformState *particle;
    SefKeyRecord *key;
    Vector4 *scaleTarget;
    Vector4 *noise;
    Vector4 *origin;
    int index;
    int result;
    int updateMatrix;
    short rotationKeyMode;
    f32 colorValue;

    keys = data->keys;
    index = sefAllocLocalData((SefLocalData *) data);
    result = -1;
    if (index >= 0) {
        particle = (SefTransformState *) sefPtAllocatorState[data->pointHandles[index]].data;
        if (particle != 0) {
            sefMemZero(particle, 0x2C0);
            particle->kind = kind;
            sefGetStartPos(&particle->position, (struct SefLaneObject *) data);
            rotationKeyMode = 0;
            sefGetTargetPos(&particle->targetPosition, (struct SefLaneObject *) data);
            QuadwordCopyT0(&particle->colorBase, &sefLinkedPositionVector);
            origin = &particle->origin;
            __asm__ __volatile__("lq $8, 0(%2)\nsq $8, 0(%1)\n" : "=m"(*origin) : "r"(origin), "r"(&particle->position) : "$8", "memory");
            particle->speed = 1.0f;
            particle->frame.f = data->pointLife;
            if (data->speedVariation != 0) {
                particle->speed += sefNoiseNext() * (data->speedVariation * sefFrameJitterScale);
            }
            sefStepCurveTrack((SefCurveState *) data, (SefCurveTrack *) particle, -1, 1.0f);
            if (keys[1] != 0) {
                rotationKeyMode = keys[1][-1].header.mode;
            }
            updateMatrix = 0;
            if (particle->motion->state != 4 || rotationKeyMode != 0) {
                updateMatrix = 1;
            }
            if (data->keys[1] != 0) {
                sefNoiseNextVector(&rotation);
                Vu0SefMulVectorXYZ(&rotation, &rotation, &data->keys[1][-1].vector);
                particle->angle_base = rotation.z;
                if (updateMatrix != 0 && sefVectorIsSet(&rotation)) {
                    MMathRotateMatrixYXZ(&particle->local_matrix, 0, &rotation);
                    particle->flags |= 8;
                }
            }
            if (data->spinKey != 0) {
                particle->spinSpeed = data->spinKey->vector.y;
                if (data->spinRange != 0.0f) {
                    particle->spin = sefRandf() * data->spinRange;
                }
            }
            particle->part_matrix[3][3] = 1.0f;
            particle->cached_matrix[3][3] = 1.0f;
            if (data->velocityKey != 0) {
                if (data->velocityKey[-1].header.mode != 0) {
                    sefNoiseNextVector(&particle->velocity);
                    __asm__ __volatile__("lqc2 vf20, 0(%0)" : : "r"(&data->velocityKey[-1]));
                    __asm__ __volatile__("lqc2 vf1, 0(%0)\n"
                                         "vmul.xyz vf20, vf20, vf1"
                                         :
                                         : "r"(&particle->velocity));
                    __asm__ __volatile__("mfc1 $8, %0\n"
                                         "qmtc2.ni $8, vf1\n"
                                         "vmulx.xyz vf20, vf20, vf1x"
                                         :
                                         : "f"(sefParticleDegToRad)
                                         : "$8");
                    __asm__ __volatile__("sqc2 vf20, 0(%0)" : : "r"(&particle->velocity));
                }
                if (data->flags & 2) {
                    QuadwordCopyT0(&particle->direction, &data->tint);
                }
            } else {
                particle->direction.w = 1.0f;
            }
            if (keys[5] != 0 && keys[5][1].header.kind == SEF_TRACK_END_KEY) {
                SefCopyKeyPoint(&particle->point, keys[5]);
            }
            if (keys[1] != 0 && (data->flags & 1)) {
                SefCopyKeyPoint(&particle->angles, keys[1]);
                sefSetTransformRotation(particle, updateMatrix);
                if (updateMatrix != 0) {
                    __asm__ __volatile__("lqc2 vf26, 0(%1)\n"
                                         "lqc2 vf27, 16(%1)\n"
                                         "lqc2 vf28, 32(%1)\n"
                                         "sqc2 vf26, 0(%0)\n"
                                         "sqc2 vf27, 16(%0)\n"
                                         "sqc2 vf28, 32(%0)\n"
                                         "sqc2 vf0, 48(%0)"
                                         :
                                         : "r"(particle->matrix), "r"(particle->rotated_rows)
                                         : "memory");
                }
            }
            if (keys[3] != 0) {
                particle->textureIndex.i = data->keys[3][-1].header.kind;
                if (data->flags & 0x40) {
                    particle->textureFrame = keys[3]->vector.y;
                }
            }
            particle->scale.w = 1.0f;
            scaleTarget = &particle->sizeScale;
            SefSetOnes(scaleTarget);
            key = keys[0];
            if (key != 0) {
                if (key[-1].header.y < 0.0f) {
                    if (key[-1].header.x != 0.0f) {
                        scaleTarget->x = particle->sizeScale.y = particle->sizeScale.z = 1.0f - sefNoiseNext() * key[-1].header.x;
                    }
                } else {
                    noise = &sizeNoise;
                    sefNoiseNextVector(noise);
                    Vu0SefMulVectorXYZ(noise, noise, &key[-1].vector);
                    Vu0SefSubVectorXYZ(scaleTarget, scaleTarget, noise);
                }
                Vu0SefScaleVectorXYZNI(scaleTarget, scaleTarget, key[-1].vector.w);
                if (data->flags & 0x10) {
                    union SefKeyRecord *source = keys[0];
                    Vector4 *scale;

                    SefCopyKeyPoint(&particle->scaleKeyValue, source);
                    scale = &particle->scale;
                    __asm__ __volatile__("lqc2 vf1, 0(%2)\n"
                                         "lqc2 vf2, 0(%3)\n"
                                         "vmul.xyz vf1, vf1, vf2\n"
                                         "sqc2 vf1, 0(%1)\n"
                                         : "=m"(*scale)
                                         : "r"(scale), "r"(&particle->scaleKeyValue), "r"(scaleTarget)
                                         : "memory");
                }
            } else {
                particle->scaleKeyValue.w = 1.0f;
            }
            scaleTarget = &particle->colorScale;
            SefSetOnes(scaleTarget);
            key = data->keys[2];
            if (key == 0) {
                return index;
            }
            if (key[-1].header.y < 0.0f) {
                if (key[-1].header.x != 0.0f) {
                    colorValue = 1.0f - sefNoiseNext() * key[-1].header.x;
                    particle->colorScale.z = colorValue;
                    particle->colorScale.y = colorValue;
                    scaleTarget->x = colorValue;
                }
            } else {
                noise = &colorNoise;
                sefNoiseNextVector(noise);
                Vu0SefMulVectorXYZ(noise, noise, &key[-1].vector);
                Vu0SefSubVectorXYZ(scaleTarget, scaleTarget, noise);
            }
            if (data->flags & 8) {
                __asm__ __volatile__("mfc1 $8, %3\n"
                                     "lqc2 vf2, 0(%1)\n"
                                     "lqc2 vf1, 0(%2)\n"
                                     "qmtc2.ni $8, vf3\n"
                                     "vmove.w vf2, vf0\n"
                                     "vmul.xyzw vf1, vf1, vf2\n"
                                     "vminix.xyz vf2, vf1, vf3x\n"
                                     "vftoi0.xyzw vf2, vf2\n"
                                     "sqc2 vf2, 0(%0)"
                                     :
                                     : "r"(particle->color), "r"(keys[2]), "r"(scaleTarget), "f"(255.0f), "m"(particle->color[0])
                                     : "$8", "memory");
            }
        }
        result = index;
    }
    return result;
}

typedef struct SefWordView {
    int value;
} SefWordView;

/* A float global written as a one-member object. */
typedef struct SefFloatView {
    float value;
} SefFloatView;

typedef struct SefLineKeys {
    struct SefTrackKey16 *position;
    struct SefTrackKey16 *unused;
    struct SefTrackKey16 *velocity;
    struct SefTrackKey16 *more[4];
} SefLineKeys;

extern short sefLinkedPartIndex;
/* The camera pair the effect update keeps: the active camera's transform of this frame and of the previous one, the
 * motion of its translation row between them and the camera's inverse transform. */
typedef struct SefCameraState {
    Matrix4 current;      /* +0x00 */
    Matrix4 previous;     /* +0x40 */
    Vector4 motion;       /* +0x80: current[3] - previous[3] (xyz) */
    Matrix4 *inverse;     /* +0x90 */
    int valid;            /* +0x94: previous holds a transform */
    unsigned char unmodeled_98[0xA0 - 0x98];
} SefCameraState;

extern SefCameraState sefCameraState;
unsigned short sefLinkedIdTable61[32] = { 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 0, 0, 0, 0 };
unsigned short sefLinkedIdTable62[24] = { 70, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, 0, 0, 0, 0, 0, 0, 0 };

/* result = first + second in x, y and z (w keeps first's). */
static inline void sefAddVectorXYZ(Vector4 *result, Vector4 *first, Vector4 *second)
{
    __asm__ __volatile__("lqc2 vf1, 0(%3)\nlqc2 vf2, 0(%4)\nvadd.xyz vf1, vf1, vf2\nsqc2 vf1, 0(%2)\n" : "=m"(*result), "=m"(result->w) : "r"(result), "r"(first), "r"(second), "m"(*second) : "memory");
}

/* *destination = *source through the 128-bit $8. */
static inline void sefCopyVector(Vector4 *destination, Vector4 *source)
{
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "+m"(*destination), "=m"(destination->w) : "r"(destination), "r"(source) : "$8");
}

/* The line data as sefExecLineGlobalData reads it to emit points. */
typedef struct SefLineEmitData {
    unsigned char *position_keys;                    /* +0x000 */
    short *spread_rotation_keys;                     /* +0x004 */
    unsigned char *size_keys;                        /* +0x008 */
    unsigned char *signal_keys;                      /* +0x00c */
    unsigned char unmodeled_010[0x14 - 0x10];
    unsigned char *rotation_keys;                    /* +0x014 */
    unsigned char unmodeled_018[0x20 - 0x18];
    unsigned short initial_frame;                    /* +0x020 */
    short frame_count;                               /* +0x022 */
    unsigned char unmodeled_024[0x40 - 0x24];
    Vector4 emitPosition;                            /* +0x040: where the next particle starts */
    unsigned char unmodeled_050[0x60 - 0x50];
    Vector4 emitVelocity;                            /* +0x060 */
    unsigned char unmodeled_070[0xa0 - 0x70];
    Vector4 noise;                                   /* +0x0a0 */
    unsigned char unmodeled_0b0[0xc0 - 0xb0];
    Vector4 emitOffset;                              /* +0x0c0 */
    unsigned char unmodeled_0d0[0xe0 - 0xd0];
    Vector4 positionValue;                           /* +0x0e0 */
    unsigned char unmodeled_0f0[0x100 - 0xf0];
    Vector4 velocityValue;                           /* +0x100 */
    unsigned char unmodeled_110[0x172 - 0x110];
    short pointsPerEmit;                             /* +0x172 */
    unsigned char unmodeled_174[0x17a - 0x174];
    short emitInterval;                              /* +0x17a */
    unsigned char unmodeled_17c[0x180 - 0x17c];
    SefLineKeys keys;                                /* +0x180 */
    unsigned char unmodeled_19c[0x1e0 - 0x19c];
    short *color_keys;                               /* +0x1e0 */
    short *uniform_scale_keys;                       /* +0x1e4 */
    unsigned char unmodeled_1e8[0x684 - 0x1e8];
    struct SefTrackKey16 *partKeys;                  /* +0x684 */
    unsigned char unmodeled_688[SEF_LOCAL_DATA_POINT_HANDLES - 0x688];
    short pointHandles[SEF_LINE_PARTICLE_COUNT];                         /* +0x6a0 */
    short emitFrame;                                 /* +0x8a0: frame of the last emission */
    short cursor;                                      /* +0x8a2 */
    short in_use;                                      /* +0x8a4 */
    unsigned char unmodeled_8a6[SEF_LINE_DATA_STRIDE - 0x8a6];
} SefLineEmitData;

int sefExecLineGlobalData(SefLineEmitData *data, int frame)
{
    SefLineKeys *keys;
    short *handles;
    int tableIndex;
    unsigned short *idTable;
    SefTrackKey16 *positionKey;
    int fromPoint;
    int fromTarget;
    int emitInterval;
    int pointsPerEmit;
    int framesSinceEmit;
    short pointHandle;
    int linkedId;
    int pointIndex;
    int remaining;
    unsigned short positionKind;
    unsigned short pointKind;
    unsigned short tableSource;
    int result;

    keys = &data->keys;
    positionKind = keys->position[-1].key;
    fromPoint = (unsigned int) (positionKind - 16) < 2;
    fromTarget = (unsigned int) ((unsigned short) keys->velocity[-1].key - 16) < 2;
    emitInterval = data->emitInterval;
    result = 1;
    if (positionKind & 0x40) {
        sefCurrentRecord->flags |= 0x10;
    }
    sefLinkedPoint = 0;
    sefLinkedPartIndex = -1;
    __asm__ __volatile__("sqc2 vf0, 0(%1)" : "=m"(data->emitOffset) : "r"(&data->emitOffset));
    if (!fromPoint) {
        sefGetPosition(&sefLinkedPositionVector, 0, keys->position[-1].key);
        sefGetPosition(&data->emitPosition, &data->positionValue, keys->position[-1].key);
        sefAddVectorXYZ(&data->emitPosition, &data->emitPosition, &data->noise);
    }
    if (!fromTarget) {
        sefGetPosition(&data->emitVelocity, &data->velocityValue, keys->velocity[-1].key);
    }
    positionKey = keys->position;
    if (positionKey[-1].key == 1) {
        __asm__ __volatile__("lq $8, 0(%2)\nsq $8, 0(%1)\n" : "=m"(data->emitOffset) : "r"(&data->emitOffset), "r"(&sefCameraState.motion) : "$8");
    } else if (positionKey[-1].key == 2 || positionKey[-1].key == 7) {
        __asm__ __volatile__("sqc2 vf0, 0(%1)" : "=m"(data->emitOffset) : "r"(&data->emitOffset));
    }
    if (emitInterval <= 0 || (sefRequestFlags[0] & 0x80)) {
        return result;
    }
    framesSinceEmit = frame - data->emitFrame;
    if (framesSinceEmit == 0) {
        return result;
    }
    if (framesSinceEmit >= emitInterval) {
        data->emitFrame = frame;
        pointsPerEmit = data->pointsPerEmit;
        if (((SefWordView *) &sefParentLine)->value >= 0) {
            SefLineEmitData *line = (SefLineEmitData *) &sefLineDataRecords[((SefWordView *) &sefParentLine)->value];

            if (line == 0) {
                return result;
            }
            for (pointIndex = 0; pointIndex < SEF_LINE_PARTICLE_COUNT; pointIndex++) {
                pointHandle = line->pointHandles[pointIndex];
                if (pointHandle >= 0) {
                    sefLinkedPointHandle = pointHandle;
                    sefLinkedPoint = ((SefWordView *) &sefParentLine)->value >= 0 ? (SefTransformState *) sefPtAllocatorState[pointHandle].data : 0;
                    if (keys->position[-1].key == 0x11) {
                        sefLinkedPartIndex = -1;
                        if (line->partKeys != 0) {
                            sefLinkedPartIndex = (signed char) line->partKeys->unmodeled_00f[0];
                        }
                    }
                    if (fromPoint) {
                        pointKind = keys->position[-1].key;
                        if ((unsigned int) (pointKind - 16) < 2) {
                            if (sefLinkedPoint != 0) {
                                sefCopyVector(&sefLinkedPositionVector, &sefLinkedPoint->colorBase);
                            } else {
                                sefGetPosition(&sefLinkedPositionVector, 0, line->keys.position[-1].key);
                            }
                        } else {
                            sefGetPosition(&sefLinkedPositionVector, 0, (short) pointKind);
                        }
                        sefGetPosition(&data->emitPosition, &data->positionValue, keys->position[-1].key);
                        sefAddVectorXYZ(&data->emitPosition, &data->emitPosition, &data->noise);
                    }
                    if (fromTarget) {
                        sefGetPosition(&data->emitVelocity, &data->velocityValue, keys->velocity[-1].key);
                    }
                    if (pointsPerEmit > 0) {
                        remaining = pointsPerEmit;
                        do {
                            sefCreateParticle((SefLineSpawnData *) data, pointHandle);
                        } while (--remaining != 0);
                    }
                }
            }
        } else {
            tableSource = positionKey[-1].key;
            if ((unsigned int) (tableSource - 0x61) < 2) {
                idTable = (short) tableSource == 0x61 ? sefLinkedIdTable61 : sefLinkedIdTable62;
                for (tableIndex = 0; idTable[tableIndex] != 0; tableIndex++) {
                    linkedId = idTable[tableIndex];
                    if (sefGetLinkedFlag(linkedId - 0x41)) {
                        sefGetPosition(&sefLinkedPositionVector, 0, linkedId);
                        sefGetPosition(&data->emitPosition, &data->positionValue, linkedId);
                        sefAddVectorXYZ(&data->emitPosition, &data->emitPosition, &data->noise);
                        if (pointsPerEmit > 0) {
                            pointIndex = pointsPerEmit;
                            do {
                                sefCreateParticle((SefLineSpawnData *) data, -1);
                            } while (--pointIndex != 0);
                        }
                    }
                }
            } else if (pointsPerEmit > 0) {
                pointIndex = pointsPerEmit;
                do {
                    sefCreateParticle((SefLineSpawnData *) data, -1);
                } while (--pointIndex != 0);
            }
        }
    }
    return result;
}

#define SefCopyVector(destination, source) \
    __asm__ __volatile__("lq $8, 0(%4)X\n" \
                         "sq $8, 0(%2)\n" \
                         : "=m"(*(destination)), "=m"((destination)->w) \
                         : "r"(destination), "r"(source) \
                         : "$8", "memory")

extern int sefLineFlag4000Count;
float sefPointMotionScale = 0.00999999978f;


/* The request block fields the line step reads and writes. */
typedef struct SefLineRequest {
    unsigned char unmodeled_000[0x300];
    SefLocalData *owner;                             /* +0x300: the line the request follows, or 0 */
    unsigned char unmodeled_304[0x308 - 0x304];
    int flags;                                       /* +0x308 */
} SefLineRequest;

static inline SefLineRequest *sefLineRequest(void)
{
    return (SefLineRequest *) sefRequestBuffer;
}

/* The line data as sefExecLineData steps it: the frame word at +0x69C is a float there. */
typedef struct SefLineStepData {
    Matrix4 matrix;                                  /* +0x000 */
    unsigned char unmodeled_040[0x80 - 0x40];
    Vector4 direction;                               /* +0x080 */
    unsigned char unmodeled_090[0xa0 - 0x90];
    Vector4 noise;                                   /* +0x0a0: noise offset scaled by the position keys' header */
    Vector4 position;                                /* +0x0b0 */
    Vector4 velocity;                                /* +0x0c0 */
    unsigned char unmodeled_0d0[0xd8 - 0xd0];
    float scaleTime;                                 /* +0x0d8: the header value of the size keys */
    unsigned char unmodeled_0dc[0xe0 - 0xdc];
    Vector4 positionValue;                           /* +0x0e0: constant value of the position keys */
    unsigned char unmodeled_0f0[0xf8 - 0xf0];
    short directionMode;                             /* +0x0f8 */
    unsigned char unmodeled_0fa[0x100 - 0xfa];
    Vector4 velocityValue;                           /* +0x100: constant value of keys[2] */
    unsigned char unmodeled_110[0x118 - 0x110];
    int constantKeys;                                /* +0x118: one bit per constant local key table */
    unsigned char unmodeled_11c[0x140 - 0x11c];
    Vector4 rotationValue;                           /* +0x140: constant value of keys[1] */
    unsigned char unmodeled_150[0x160 - 0x150];
    Vector4 scaleValue;                              /* +0x160: constant value of keys[3] */
    unsigned char unmodeled_170[0x180 - 0x170];
    struct SefTrackKey16 *keys[7];                          /* +0x180 */
    struct SefTrackKey16 *images;                           /* +0x19c: image list, its header vector precedes it */
    DoublewordVector4 imageOffset;                           /* +0x1a0 */
    unsigned char unmodeled_1b0[0x1cc - 0x1b0];
    unsigned short imageFlags;                       /* +0x1cc: flags of the first image packet */
    short imageKind;                                 /* +0x1ce */
    unsigned char unmodeled_1d0[0x670 - 0x1d0];
    struct SefTrackKey16 *scaleKeys;                        /* +0x670 */
    struct SefTrackKey16 *angleKeys;                    /* +0x674: its header shorts sit before the first key */
    struct SefTrackKey16 *colorKeys;                        /* +0x678 */
    struct SefTrackKey *alphaKeys;                           /* +0x67c */
    void *spinKeys;                                  /* +0x680 */
    void *partKeys;                                  /* +0x684 */
    struct SefTrackKey *sizeKeys;                          /* +0x688 */
    short pointLife;                                  /* +0x68c: point life in frames */
    short speedVariation;                             /* +0x68e: speed variation of the points */
    float gravity;                                   /* +0x690 */
    float height;                                    /* +0x694 */
    float startFrame;                                /* +0x698 */
    float frame;                                     /* +0x69c */
    short pointHandles[SEF_LINE_PARTICLE_COUNT];     /* +0x6a0 */
    short emitFrame;                                     /* +0x8a0: frame of the last emission */
    short cursor;                                    /* +0x8a2 */
    short in_use;                                    /* +0x8a4 */
    short pointCount;                                      /* +0x8a6: live point count, returned by sefExecLineLocalData */
    short collision;                                 /* +0x8a8 */
    short count;                                     /* +0x8aa */
    unsigned char link;                              /* +0x8ac */
    unsigned char mode;                              /* +0x8ad */
    unsigned char unmodeled_8ae[SEF_LINE_DATA_STRIDE - 0x8ae];
} SefLineStepData;

/* Steps the line one frame: counts its start delay down (returns 0 while a waiting line has nothing to do), fills
 * the motion block sefPointMotion for its points, samples its six key tracks, spawns or resets through sefExecLineGlobalData,
 * keeps the velocity of a line placed on a point, applies the item transform to the floor, steps the points with
 * sefExecLineLocalData and advances the frame clock, wrapping it at 1016. Returns 1. */
int sefExecLineData(SefLineStepData *data, int slotFrame)
{
    int started;
    int frame;
    int positionKind;
    int value;              /* the direction kind, later the transform's scaled flag: one variable in the original */
    unsigned char mode;
    int velocityMode;
    short *angleKey;
    int frameMillis;
    int loopOffset;
    unsigned short parameter;
    AlignedVector4 savedPosition;

    started = 0;
    mode = data->mode;
    frame = (int) data->frame;
    sefVectorMode = 0;
    sefTrackTimeStep = sefPointTimeScale;
    if (!(mode & 2) || (sefLineRequest()->flags & 0x800)) {
        float previousStart = data->startFrame;
        float nextStart = previousStart - sefPointTimeScale;

        if (previousStart > 0.0f && nextStart <= 0.0f) {
            started = 1;
        }
        data->startFrame = nextStart;
        if (nextStart < 0.0f && frame != 0) {
            if ((unsigned char) (mode & 1) || (unsigned short) data->pointCount == 0) {
                data->startFrame = 0.0f;
                return 0;
            }
        }
    }
    if (data->startFrame < 0.0f) {
        data->startFrame = -1.0f;
    }
    sefPointMotion.fixedGravity = (unsigned short) data->in_use & 0x100;
    sefPointMotion.gravity = data->gravity * sefPointTimeScale * sefPointMotionScale;
    sefPointMotion.bounce = data->collision * sefPointMotionScale;
    sefPointMotion.keys = data->keys[4];
    sefPointMotion.floor = data->height + sefPointMotion.floorBase;
    sefPointMotion.keyParameter = sefPointMotion.keys != 0 ? ((unsigned short *) sefPointMotion.keys)[-1] : 0;
    sefLineRequest()->owner = (data->in_use & 0x40) ? (SefLocalData *) data : 0;
    if (data->in_use & 0x4000) {
        sefLineFlag4000Count++;
    }
    sefLerpVector(data->keys[0], (SefVectorTrackCursor *) ((unsigned char *) data + 0xD0));
    sefLerpVector(data->keys[2], (SefVectorTrackCursor *) ((unsigned char *) data + 0xF0));
    sefLerpVector(data->keys[1], (SefVectorTrackCursor *) ((unsigned char *) data + 0x130));
    sefLerpVector(data->keys[3], (SefVectorTrackCursor *) ((unsigned char *) data + 0x150));
    sefProgressInt((SefKeyframe *) data->keys[5], (SefKeyCursor *) ((unsigned char *) data + 0x170));
    sefProgressInt((SefKeyframe *) data->keys[6], (SefKeyCursor *) ((unsigned char *) data + 0x178));
    angleKey = (short *) data->angleKeys;
    sefPointMotion.sameDirection = 0;
    value = angleKey[-2];
    if (value == 161 && angleKey[-9] == 161) {
        sefPointMotion.sameDirection = 1;
    }
    if (data->startFrame >= 0.0f || started) {
        sefExecLineGlobalData((SefLineEmitData *) data, frame);
    } else {
        velocityMode = ((short **) data->keys)[0][-2];
        if (velocityMode == 1) {
            __asm__ __volatile__("lq $8, 0(%2)\n"
                                 "sq $8, 0(%1)\n"
                                 : "=m"(data->velocity)
                                 : "r"(&data->velocity), "r"(&sefCameraState.motion)
                                 : "$8", "memory");
        } else if (velocityMode == 2 || velocityMode == 7) {
            Vu0SetOrigin(&data->velocity);
        }
    }
    positionKind = ((short *) data->keys[0])[-2];
    if (positionKind >= 32) {
        QuadwordCopyT0(&savedPosition, &data->position);
        sefGetPosition(&data->position, 0, positionKind);
        Vu0SefSubVectorXYZ(&data->velocity, &data->position, &savedPosition);
    }
    sefGlobalTransform.scaled = 0;
    if (data->in_use & 0x2000) {
        svInitItemDrawKind(&sefGlobalTransform, (struct SvDrawItem *) data, sefCurrentRecord->owner, (int) sefCurrentRecord->object,
                           (int) sefCurrentRecord->ownerHigh);
        value = sefGlobalTransform.scaled;
        if (value != 0) {
            sefPointMotion.floor += sefGlobalTransform.position.y - sefGlobalTransform.position.y / sefGlobalTransform.scale;
        }
    }
    data->pointCount = sefExecLineLocalData((SefLocalData *) data, frame);
    data->frame += sefPointTimeScale;
    frameMillis = (int) (data->frame * 1000.0f);
    if (frameMillis % 10 == 9) {
        frameMillis++;
    }
    data->frame = frameMillis * 0.001;
    if ((int) data->frame >= 1016) {
        if (data->mode & 2) {
            /* +0x17A: the second word of the keyframe cursor sampled from keys[6] above. */
            data->frame = (int) data->frame - ((short *) data)[0x17A / 2] * 2;
            loopOffset = frame - data->emitFrame;
            if (data->frame <= 0.0f) {
                data->frame = 1.0f;
            }
            data->emitFrame = (int) data->frame - loopOffset - 1;
        } else {
            data->frame = 1008.0f;
        }
    }
    return 1;
}

/* Re-initializes a slot: the line handles go to 0xff (free), the id and kind
 * are set from the arguments, bit 31 of flags is cleared and lineCount is zeroed. */
void sefInitEffectData(SefEffectSlot *slot, int id, int kind)
{
    int flags;

    memset(slot, -1, 0x20);
    slot->id = id;
    flags = slot->flags & 0x7FFFFFFF;
    slot->kind = (short)kind;
    slot->flags = flags;
    slot->lineCount = 0;
}

void sefDestroyEffectData(EffectData *effect)
{
    int lineIndex;

    if (*(int *)((unsigned char *)effect + 0x40) != 0) {
        for (lineIndex = 0; lineIndex < 32; lineIndex++) {
            sefFreeEffectData(effect, lineIndex);
        }
    }
    *(int *)((unsigned char *)effect + 0x40) = 0;
}

SefLocalData *sefAllocEffectData(SefEffectSlot *slot, int tag)
{
    int lineSlot;
    int lineHandle;

    lineSlot = (signed char)slot->lineCount;
    if ((unsigned int)lineSlot >= 32) {
        return 0;
    }
    if ((signed char)slot->lineHandles[lineSlot] >= 0) {
        return 0;
    }
    lineHandle = sefAllocLineData();
    if (lineHandle < 0) {
        return 0;
    }
    slot->lineHandles[lineSlot] = lineHandle;
    slot->lineTags[lineSlot] = tag;
    slot->lineCount++;
    return &sefLineDataRecords[lineHandle];
}

void sefFreeEffectData(EffectData *effect, unsigned int index)
{
    if (index < SEF_EFFECT_LINE_COUNT) {
        signed char *handle = &effect->lineHandles[index];

        if (*handle >= 0) {
            sefFreeLineData(*handle);
            *handle = -1;
        }
    }
}

void sefOrEntryFlags(int key, int flags);
void sefClearEntryFlags(int key, int flags);

void sefApplySlotFlags(int wordMask, int flagBits, int setBits)
{
    void (*applyBits)(int, int);
    int *requestWords;

    requestWords = (int *)sefRequestBuffer;
    applyBits = setBits ? sefOrEntryFlags : sefClearEntryFlags;
    if (wordMask == 0) {
        applyBits(requestWords[0], flagBits);
        return;
    }
    if (wordMask & 1) {
        applyBits(requestWords[0], flagBits);
    }
    if (wordMask & 2) {
        applyBits(requestWords[1], flagBits);
    }
    if (wordMask & 4) {
        applyBits(requestWords[2], flagBits);
    }
}

/* One 16-byte entry of a slot's line script (the table at the slot's id): the script index scGetImmAdrImmIdx2 reads,
 * the start time and the sefInitLineData arguments, the tag sefAllocEffectData records and whether the new line inherits
 * its count from the linked line. */
typedef struct SefSlotEntry {
    unsigned short script;
    short startTime;
    short frame;
    short link;
    short count;
    short tag;
    short flags;
    short inherit;
} SefSlotEntry;

extern s16 _hitSignal;

/* Hands a pending request flag of the slot to its target entries: clears it in the slot and sets it in the entries. */
static inline void sefPassSlotFlag(SefEffectSlot *slot, int target, int flag)
{
    if (slot->flags & flag) {
        slot->flags &= ~flag;
        sefApplySlotFlags(target, flag, 1);
    }
}

/* Returns the line record of handle, or 0 for a free (negative) handle. */
static inline SefLocalData *sefLineRecord(int handle)
{
    if (handle < 0) {
        return 0;
    }
    return &sefLineDataRecords[handle];
}

/* Steps the slot to frame: passes its pending flags on, starts the lines of the script entries whose time has come,
 * then updates every line and frees the finished ones (with the lines linked to a line in mode 1). Returns 1 while
 * the slot is waiting or alive, 0 once it is destroyed or empty. */
int sefExecEffectData(SefEffectSlot *slot, int frame, short *hit)
{
    SefSlotEntry *scriptEntry;
    SefLocalData *line;
    SefLocalData *linked;
    SefLineScript *script;
    int targetIndex;
    int linkedRequest;
    int keepSlot;
    int startTime;
    int lineFrame;
    int linkIndex;
    int entryCount;
    int tag;
    int lineFlags;
    int inherit;
    int lineIndex;
    int lineHandle;
    int lineSlot;
    int otherSlot;
    int linkHandle;
    int targetHandle;
    int otherHandle;
    SefLocalData *otherLine;
    int localFrame;
    int aliveLines;
    void **requestWords;
    SefLocalData *linkSource;
    SefSlotEntry *slotEntries;

    /* The gotos below stay (pass 3, eleven structured forms compiled against the original object):
       - break for update/wait (or a static inline helper that returns) makes the loop a normal one with an exit: gcc
         rotates it, putting the loop head at the bottom, and the wait store moves in front of the loop;
       - an inline helper writing aliveLines through a pointer makes it an addressable local, so its stack slot is
         allocated before the localFrame spill (slots 0/4 swap), and a returned value lands in a2 and grows the frame;
       - dropping `goto first`/`again` for a plain for (;;) with continue is the closest form: the code is identical
         except that slot and startTime swap s4/s5. The original's goto again leaves the loop notes and re-enters
         through the preheader, which shifts slot's allocation priority; keeping a label only for again, with
         the loop entered by falling in, changes the delay-slot reload (size differs). */
    slot->flags &= 0x7FFFFFFF;
    if (slot->id == 0) {
        return 0;
    }
    if (frame < slot->kind) {
        goto pending;
    }
    keepSlot = sefRequestBuffer[0x24];
    linkedRequest = ((SefLineRequest *) sefRequestBuffer)->flags & 0x2000000;
    targetIndex = (slot->flags >> 20) & 7;
    sefPassSlotFlag(slot, targetIndex, 0x100);
    sefPassSlotFlag(slot, targetIndex, 0x200);
    sefPassSlotFlag(slot, targetIndex, 0x400);
    sefPassSlotFlag(slot, targetIndex, 0x800);
    sefPassSlotFlag(slot, targetIndex, 0x1000);
    sefPassSlotFlag(slot, targetIndex, 0x2000);
    sefPassSlotFlag(slot, targetIndex, 0x4000);
    sefPassSlotFlag(slot, targetIndex, 0x10000);
    sefPassSlotFlag(slot, targetIndex, 0x20000);
    sefPassSlotFlag(slot, targetIndex, 0x10);
    sefPassSlotFlag(slot, targetIndex, 0x20);
    sefPassSlotFlag(slot, targetIndex, 0x40);
    sefPassSlotFlag(slot, targetIndex, 0x80);
    sefPassSlotFlag(slot, targetIndex, 0x40000);
    sefPassSlotFlag(slot, targetIndex, 0x80000);
    sefPassSlotFlag(slot, targetIndex, 0x800000);
    sefPassSlotFlag(slot, targetIndex, 0x10000000);
    sefPassSlotFlag(slot, targetIndex, 0x20000000);
    sefPassSlotFlag(slot, targetIndex, 0x2);
    sefPassSlotFlag(slot, targetIndex, 0x40000000);
    if (slot->flags & 0x0F000000) {
        sefApplySlotFlags(targetIndex, slot->flags & 0x0F000000, 0);
        sefApplySlotFlags(targetIndex, slot->flags & 0x0F000000, 1);
        slot->flags &= ~0x0F000000;
    }
    if (slot->flags & 0x8000) {
        slot->flags &= ~0x8000;
        requestWords = (void **) sefRequestBuffer;
        sefOrEntryFlags((int) requestWords[2], 8);
        if (linkedRequest != 0) {
            sefCurrentRecord->flags |= 0x40;
            slot->flags &= ~0x8000;
            sefOrEntryFlags((int) requestWords[0], 8);
            sefOrEntryFlags((int) requestWords[1], 8);
        }
    }
    if (slot->flags & 0x1) {
        slot->flags &= ~0x1;
        sefOrEntryFlags((int) ((void **) sefRequestBuffer)[0], 1);
        _hitSignal = 1;
    }
    if (slot->flags & 0x8) {
        slot->flags &= ~0x8;
        if (keepSlot == 0) {
            goto destroy;
        }
    }
    if (slot->flags & 0x4) {
        void **request = (void **) sefRequestBuffer;

        slot->flags &= ~0x4;
        sefOrEntryFlags((int) request[2], 4);
    }
    if (frame < slot->kind) {
        goto pending;
    }
    localFrame = frame - slot->kind;
    if ((unsigned int) slot->id < 3) {
        goto destroy;
    }
    aliveLines = 0;
    goto first;
again:
    linkSource = 0;
first:
    for (;;) {
        slotEntries = (SefSlotEntry *) slot->id;
        scriptEntry = &slotEntries[(signed char) slot->lineCount];
        startTime = scriptEntry->startTime;
        lineFrame = scriptEntry->frame;
        entryCount = scriptEntry->count;
        tag = scriptEntry->tag;
        lineFlags = scriptEntry->flags;
        inherit = scriptEntry->inherit;
        linkIndex = scriptEntry->link;
        if (lineFrame == 0) {
            sefCurrentRecord->flags |= 0x4000000;
        }
        if (startTime >= SEF_TRACK_END_KEY) {
            goto update;
        }
        if (localFrame < startTime) {
            goto wait;
        }
        lineIndex = (signed char) slot->lineCount;
        if (lineIndex >= 32) {
            goto update;
        }
        script = (SefLineScript *) scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *) slotEntries, lineIndex * 8);
        if (script == 0) {
            goto update;
        }
        line = sefAllocEffectData(slot, tag);
        if (line == 0) {
            goto update;
        }
        sefInitLineData(line, startTime, lineFrame, script, linkIndex, entryCount, lineFlags);
        if (inherit != 0) {
            linked = 0;
            linkHandle = (signed char) slot->lineHandles[linkIndex];
            if (linkHandle >= 0) {
                linked = &sefLineDataRecords[linkHandle];
            }
            line->in_use |= 0x2000;
            if (linked != 0) {
                line->count = linked->keys[0][-1].key;
            }
        }
        if (line->keys[0][-1].key != 17) {
            goto again;
        }
        if (linkIndex < 0) {
            goto again;
        }
        linkSource = sefLineRecord((signed char) slot->lineHandles[linkIndex]);
        if (linkSource != 0) {
            linkSource->in_use |= 0x1000;
        }
    }
wait:
    aliveLines = startTime != SEF_TRACK_END_KEY;
update:
    slot->flags |= 0x80000000;
    for (lineSlot = 0; lineSlot < 32; lineSlot++) {
        lineHandle = (signed char) slot->lineHandles[lineSlot];
        if (lineHandle < 0) {
            continue;
        }
        line = &sefLineDataRecords[lineHandle];
        linkIndex = (signed char) line->link;
        if (linkIndex >= 0) {
            linkIndex = (signed char) slot->lineHandles[linkIndex];
        }
        sefParentLine = linkIndex;
        if (sefExecLineData((SefLineStepData *) line, localFrame) != 0) {
            aliveLines++;
            continue;
        }
        sefFreeEffectData((EffectData *) slot, lineSlot);
        if (line->mode & 1) {
            for (otherSlot = 0; otherSlot < 32; otherSlot++) {
                if (otherSlot == lineSlot) {
                    continue;
                }
                otherHandle = (signed char) slot->lineHandles[otherSlot];
                if (otherHandle < 0) {
                    continue;
                }
                otherLine = &sefLineDataRecords[otherHandle];
                if ((signed char) otherLine->link == lineSlot) {
                    sefFreeEffectData((EffectData *) slot, otherSlot);
                }
            }
        }
    }
    if (aliveLines == 0) {
destroy:
        sefDestroyEffectData((EffectData *) slot);
        slot->flags |= 0x80000000;
        return 0;
pending:
        return 1;
    }
    return slot->id != 0;
}

static unsigned char sefLineDataPool[0x68400];

/* Clears the static 0x68400-byte block sefLineDataPool (192 records of the 0x8B0 line-data stride); both start chains run it
   right after sefInitScheduler. */
void sefClearLineDataRecords(void) {
    sefMemClear(sefLineDataPool, 0x68400);
}

extern SefEffectRecord *sefEffectRecords;   /* scaffold .sdata item 0x69DB50; defining it changes sefCreateScheduler2 (blocked-b01.md) */
extern int sefCameraOwnerRecord;

void sefInitScheduler(void)
{
    int recordIndex;
    int slotIndex;

    sefMemZero(sefEffectRecords, 4);
    sefCameraOwnerRecord = 0;
    for (recordIndex = 0; recordIndex < sefEffectRecordCount; recordIndex++) {
        sefEffectRecords[recordIndex].linkValue = -1;
        for (slotIndex = 0; slotIndex < 32; slotIndex++) {
            sefInitEffectData((SefEffectSlot *)&sefEffectRecords[recordIndex].entries[slotIndex], 0, 0);
        }
    }
}

/* The owner whose 0x74 word is non-zero while it has entries in the table. */
struct SefOwner {
    unsigned char unmodeled_000[0x74];
    int tracked;
};


int uwamonoOwnsRecord(void *owner)
{
    UwamonoRecord *record;
    int recordIndex;

    if (owner == 0) {
        return 0;
    }
    record = uwamonoRecords;
    for (recordIndex = 0; recordIndex < 0x100; recordIndex++) {
        if (record->kind == 6 && owner == record->handles[0]) {
            return 1;
        }
        record++;
    }
    return 0;
}

/* Detaches every kind-6 entry registered under owner and releases it by index. */
void uwamonoReleaseOwnedRecords(SefOwner *owner)
{
    UwamonoRecord *record;
    int index;

    if (owner != 0 && owner->tracked != 0) {
        record = uwamonoRecords;
        for (index = 0; index < 0x100; index++) {
            if (record->kind == 6 && record->handles[0] == owner) {
                record->handles[0] = 0;
                uwamonoReleaseRecord(index);
            }
            record++;
        }
    }
}

extern int sefEffectRecordHead;

int sefAllocScheduler(int owner)
{
    int recordCount;
    int lastIndex;
    int index;
    SefEffectRecord *record;

    recordCount = sefEffectRecordCount;
    lastIndex = (sefEffectRecordHead + recordCount - 1) % recordCount;
    for (index = sefEffectRecordHead; index != lastIndex; index++) {
        index = index % sefEffectRecordCount;
        record = &sefEffectRecords[index];
        if (record->inUse == 0) {
            record->linkValue = -1;
            record->inUse = owner;
            {
                int total = sefEffectRecordCount;

                record->coord_a = 0;
                sefEffectRecordHead = (index + 1) % total;
            }
            return index;
        }
    }
    return -1;
}

int sefAllocUnownedScheduler(int owner)
{
    int recordCount;
    int lastIndex;
    int index;
    SefEffectRecord *record;

    recordCount = sefEffectRecordCount;
    lastIndex = (sefEffectRecordHead + recordCount - 1) % recordCount;
    for (index = sefEffectRecordHead; index != lastIndex; index++) {
        index = index % sefEffectRecordCount;
        if (sefEffectRecords[index].inUse == 0 && !uwamonoOwnsRecord(&sefEffectRecords[index])) {
            /* The record's byte offset from the table address. */
            record = (SefEffectRecord *)(index * sizeof(SefEffectRecord) + (int)sefEffectRecords);
            record->linkValue = -1;
            record->inUse = owner;
            {
                int total = sefEffectRecordCount;

                record->coord_a = 0;
                sefEffectRecordHead = (index + 1) % total;
            }
            return index;
        }
    }
    return -1;
}

extern unsigned char sefResourceTable[];

/* A cell record (cellSlot set) is the head of a ResourceSlot of sefResourceTable, and the sdv.c cell functions
 * take it as one. */
static inline struct ResourceSlot *sefCellOf(SefEffectRecord *record)
{
    return (struct ResourceSlot *) record;
}

/* The reverse view: the record that sefCreateKindResourceSlot's slot is the head of. */
static inline SefEffectRecord *sefRecordOfCell(struct ResourceSlot *slot)
{
    return (SefEffectRecord *) slot;
}

void sefFreeSchedulerCf(SefEffectRecord *record)
{
    int effectIndex;

    if (record == 0) {
        return;
    }
    if (record->cellSlot != 0) {
        sefReleaseCellSlot(sefCellOf(record));
        return;
    }
    if ((record < sefEffectRecords) || (sefEffectRecords + sefEffectRecordCount < record)) {
        if (((unsigned char *)record < sefResourceTable) ||
            (sefResourceTable + 0x13800 < (unsigned char *)record)) {
            sefReleaseCellSlot(sefCellOf(record));
        }
        return;
    }
    if (record->inUse == 0) {
        return;
    }
    sdvFreeSpecialSlots(&record->special);
    for (effectIndex = 0; effectIndex < 0x20; effectIndex++) {
        sefDestroyEffectData((EffectData *)&record->entries[effectIndex]);
    }
    sdvDestroyOwnerAlters(record - sefEffectRecords);
    record->inUse = 0;
    record->coord_a = 0;
    record->ownerKey = 0;
}

/* Releases the record at index when it is in use: a record with flag bits 0x102 goes to sefFreeSchedulerCf; otherwise its special
   slots and its 32 effect entries are destroyed, the record is cleared and sdvDestroyOwnerAlters runs. */
void sefFreeScheduler(int index)
{
    int effectIndex;

    if (index < 0) {
        return;
    }
    if (index >= sefEffectRecordCount) {
        return;
    }
    if (sefEffectRecords[index].inUse == 0) {
        return;
    }
    if ((sefEffectRecords[index].flags & 0x102) != 0) {
        sefFreeSchedulerCf(sefEffectRecords + index);
        return;
    }
    sdvFreeSpecialSlots(&sefEffectRecords[index].special);
    for (effectIndex = 0x1f; effectIndex >= 0; effectIndex--) {
        sefDestroyEffectData((void *)&sefEffectRecords[index].entries[31 - effectIndex]);
    }
    sdvDestroyOwnerAlters(index);
    sefEffectRecords[index].inUse = 0;
    sefEffectRecords[index].coord_a = 0;
    sefEffectRecords[index].flags = 0;
    sefEffectRecords[index].ownerKey = 0;
}

void sefDummy_0025DEC0(void) {
}

void sefDummy_0025DEC8(void) {
}

/* One 10-byte entry of the table sefInitEffectTbl reads (stride 10, no padding). key < 0 ends the table; address is
 * the table[1] that scGetImmAdrImmIdx2 reads; flags is copied unaligned into the slot; link < 0 means no link. */
typedef struct SefEffectLoadEntry {
    short key;                 /* +0x00 */
    unsigned short address;    /* +0x02 */
    unsigned char flags[4];    /* +0x04 */
    signed char link;          /* +0x08 */
    unsigned char signal;      /* +0x09 */
} SefEffectLoadEntry;

/* Loads the effect slots of record slotIndex from table: each entry with key >= 0 fills one slot, up to 32; the
 * slots after the last loaded entry get state 0. Returns the number of loaded entries. */
int sefInitEffectTbl(SefEffectLoadEntry *table, int slotIndex, unsigned short loadFlags)
{
    SefEffectEntry *entries;
    SefEffectLoadEntry *entry;
    SefEffectLoadEntry *tableStart;
    SefEffectEntry *slot;
    SefEffectEntry *nextSlot;
    SefEffectLoadEntry *nextEntry;
    int entryCount;
    int slotId;
    int loadedCount;
    int defaultId;
    int *clearState;

    entries = sefEffectRecords[slotIndex].entries;
    entryCount = 0;
    tableStart = table;
    entry = table;
    if (entry->key >= 0) {
        slot = entries;
        defaultId = 1;
        nextSlot = entries;
        nextEntry = table;
    do {
        slotId = scGetImmAdrImmIdx2(sefCurrentLoadTag, (unsigned short *)entry, 1);
        if (slotId == 0) {
            slotId = defaultId;
        }
        sefInitEffectData((SefEffectSlot *)slot, slotId, entry->key);
        __builtin_memcpy(&slot->flags, entry->flags, 4);
        slot->speedPercent = entry->signal;
        if (entry->link >= 0) {
            sefEffectRecords[slotIndex].linkValue = tableStart[entry->link].link;
        }
        nextEntry++;
        nextSlot++;
        entryCount++;
        if (entry->key < 0) {
            break;
        }
        slot = nextSlot;
        entry = nextEntry;
        if (entryCount >= 32) {
            break;
        }
    } while (nextEntry->key >= 0);
    }
    loadedCount = entryCount;
    if (loadedCount < 32) {
        clearState = &entries[loadedCount].state;
        for (; entryCount < 32; entryCount++) {
            entries[entryCount].state = 0;
        }
    }
    return loadedCount;
}

/* The 8-byte kind and scale pair of the request (request +0x1c): the record's ownerKey and coord_b are copied here whole. */
typedef struct SefRequestPair {
    int kind;                       /* +0x00 */
    float scale;                    /* +0x04 */
} SefRequestPair;

/* The 8-byte spawn block of the request (+0x24): the record's spawn flags and loadFlags are copied whole. */
typedef struct SefRequestSpawn {
    unsigned short spawnEnabled;    /* +0x00: zeroed after the copy */
    unsigned short spawnBits;       /* +0x02: zeroed after the copy */
    unsigned char unmodeled_04[2];
    unsigned short loadFlags;       /* +0x06 */
} SefRequestSpawn;

/* The 16-byte kind and spawn block of the request (+0x1c): record +0x74..+0x83 is copied whole. */
typedef struct SefRequestKindSpawn {
    SefRequestPair kindScale;
    SefRequestSpawn spawn;
} SefRequestKindSpawn;

/* The 56-byte request sefSpawnRecordChildren copies out of a record and passes to sefCreateEffect2. */
typedef struct SefRecordRequest {
    int owner;                      /* +0x00: the record owner */
    unsigned char unmodeled_04[0x08 - 0x04];
    int object;                     /* +0x08: the record object, or the owner when that is null */
    int values[3];                  /* +0x0c: the record values */
    int tail;                       /* +0x18: the record requestTail */
    SefRequestKindSpawn block;      /* +0x1c: kindScale and spawn, copied whole */
    unsigned char unmodeled_2c[0x38 - 0x2c];
} SefRecordRequest;

/* The record `index` of sefEffectRecords, addressed as the original does (byte offset plus table base). */
#define SEF_RECORD_AT(index) ((SefEffectRecord *) ((index) * sizeof(SefEffectRecord) + (int) sefEffectRecords))


extern unsigned short sefGroupKey;

/* Binds one script handle: its kind byte, then its script address. */
static inline void sefBindHandle(SdvSequence *handle, int address, int kind)
{
    handle->stride = kind;
    handle->keys = (short *) address;
}

/* Sets the vector to the unit (1, 1, 1, 1). */
static inline void sefSetUnitVector(Vector4 *vector)
{
    __asm__ __volatile__("vmaxw.xyzw vf1, vf0, vf0w\n"
                         "sqc2 vf1, 0(%1)\n"
                         : "+m"(*vector) : "r"(vector), "m"(*vector) : "memory");
}

/* A record seen through a union with its raw words. The original's flag accesses go through such a view, so they
 * carry alias set 0. */
typedef union SefEffectRecordWords {
    SefEffectRecord record;
    int words[sizeof(SefEffectRecord) / sizeof(int)];
} SefEffectRecordWords;

/* The record `index` of the table at `records`. */
static inline SefEffectRecord *sefRecordOf(SefEffectRecord *records, int index)
{
    return (SefEffectRecord *) (index * sizeof(SefEffectRecord) + (int) records);
}

/* Creates the effect record of the script `script` for the request: the record `index` when it is not negative,
 * otherwise a fresh (unowned for owner kind 1) one. Copies the request into it, resets its blocks, binds the six
 * script handles, loads its 32 effect slots and looks up its sub kind and entry id. Returns the record index or -1. */
int sefCreateScheduler2(unsigned short *table, SefRecordRequest *request, const Vector4 *offset, int loadTag, int delayFrames,
                          int reuseIndex, int flag)
{
    SefEffectRecord *record;
    int charNo;
    int effectType;
    unsigned char *savedLoadTag;
    int windHandle;
    int cameraValue;
    int specialHandle;
    int alterHandle;
    int effectSeqHandle;
    int shadowSeqHandle;
    int index;
    int selected;
    const Vector4 *source;
    unsigned char requestMode;
    SefEffectRecord *target;
    SefEffectRecord *created;

    windHandle = scGetImmAdrImmIdx2(loadTag, table, 0);
    cameraValue = scGetImmAdrImmIdx2(loadTag, table, 1);
    specialHandle = scGetImmAdrImmIdx2(loadTag, table, 2);
    alterHandle = scGetImmAdrImmIdx2(loadTag, table, 3);
    effectSeqHandle = scGetImmAdrImmIdx2(loadTag, table, 4);
    shadowSeqHandle = scGetImmAdrImmIdx2(loadTag, table, -1);
    table += 5;
    if (reuseIndex >= 0) {
        SefEffectRecord *reused;

        index = reuseIndex;
        reused = SEF_RECORD_AT(index);
        reused->linkValue = -1;
        reused->inUse = (int) table;
        reused->coord_a = 0.0f;
    } else if (((request->block.kindScale.kind >> 28) & 3) == 1) {
        index = sefAllocUnownedScheduler((int) table);
    } else {
        index = sefAllocScheduler((int) table);
    }
    if (index < 0) {
        return -1;
    }
    savedLoadTag = *(unsigned char **) &sefCurrentLoadTag;
    sefCurrentLoadTag = loadTag;
    *(SefRequestKindSpawn *) &SEF_RECORD_AT(index)->ownerKey = request->block;
    target = SEF_RECORD_AT(index);
    requestMode = ((unsigned char *) request)[0x2C];
    target->owner = request->owner;
    *(int *) &target->object = request->object;
    target->values[0] = request->values[0];
    target->values[1] = request->values[1];
    target->values[2] = request->values[2];
    target->requestTail = request->tail;
    target->ownerHigh = (void *) *(int *) request->unmodeled_04;
    target->requestMode = requestMode;
    record = sefEffectRecords;
    record = sefRecordOf(record, index);
    if (record->coord_b == 0.0f) {
        record->coord_b = 1.0f;
    }
    source = offset;
    if (source == 0) {
        source = &sefDefaultOffset;
    }
    QuadwordCopyT0(record, source);
    QuadwordZero(&record->rotation);
    record->remaining = delayFrames;
    ((SefEffectRecordWords *) record)->record.flags = flag ? SEF_RECORD_SLOTS_LOADED : 0;
    record->fieldA5C = 0.0f;
    record->value = sefGroupKey;
    record->loadTag = loadTag;
    record->cellSlot = 0;
    record->slotIndex = index;
    sefSetUnitVector(&record->vertexScale);
    memset(&record->camera, 0, sizeof(record->camera));
    memset(&SEF_RECORD_AT(index)->special, 0, 0x10);
    memset(&SEF_RECORD_AT(index)->alter, 0, 8);
    memset(&SEF_RECORD_AT(index)->effectSeq, 0, 8);
    memset(&SEF_RECORD_AT(index)->shadowSeq, 0, 8);
    memset(&SEF_RECORD_AT(index)->wind, 0, 8);
    sefBindHandle(&SEF_RECORD_AT(index)->effectSeq, effectSeqHandle, 7);
    sefBindHandle(&SEF_RECORD_AT(index)->shadowSeq, shadowSeqHandle, 7);
    sefBindHandle(&SEF_RECORD_AT(index)->alter, alterHandle, 28);
    sefBindHandle(&SEF_RECORD_AT(index)->special.sequence, specialHandle, 2);
    sefBindHandle(&SEF_RECORD_AT(index)->wind, windHandle, 11);
    selected = sdvSelectCamera(&sefEffectRecords[index].camera, loadTag, (unsigned short *) cameraValue);
    if (loadTag != 0 && cameraValue != 0 && selected == 0) {
        ((SefEffectRecordWords *) SEF_RECORD_AT(index))->record.flags |= SEF_RECORD_PENDING;
    }
    created = SEF_RECORD_AT(index);
    sefSetUnitVector(&created->scale);
    created->gate = 1;
    created->cullRadius = 8.0f;
    if (sefInitEffectTbl((SefEffectLoadEntry *) table, index, request->block.spawn.loadFlags) == 0) {
        ((SefEffectRecordWords *) SEF_RECORD_AT(index))->record.flags &= ~SEF_RECORD_SLOTS_LOADED;
    }
    *(unsigned char **) &sefCurrentLoadTag = savedLoadTag;
    srsAnalyzeEftNo(request->block.kindScale.kind, &charNo, &effectType);
    SEF_RECORD_AT(index)->effectType = effectType;
    SEF_RECORD_AT(index)->entryId = sefSearchMapperIndex(request->block.kindScale.kind);
    return index;
}

int sefCreateScheduler(int first, int second, const Vector4 *offset, int fourth, int fifth) {
    return sefCreateScheduler2((unsigned short *) first, (SefRecordRequest *) second, offset, fourth, 0, fifth, 1);
}

SefEffectRecord *sefCreateEffect2(void *object);

void sefSpawnRecordChildren(SefEffectRecord *record)
{
    SefRecordRequest spawnRequest;
    unsigned int spawnBits;
    unsigned int bit;
    int spawnKind;
    int i;

    if (record->spawnEnabled == 0) {
        return;
    }
    spawnBits = record->spawnBits & 0xFFF;
    spawnKind = record->spawnBits >> 12;
    memset(&spawnRequest, 0, sizeof(spawnRequest));
    do {
        u64 ownerPair = *(u64 *)&record->owner;
        u64 objectPair = *(u64 *)&record->object;
        u64 valuesPair = *(u64 *)&record->values[1];
        int tail;
        do {
            tail = record->requestTail;
        } while (0);
        *(u64 *)&spawnRequest.object = objectPair;
        *(u64 *)&spawnRequest.owner = ownerPair;
        *(u64 *)&spawnRequest.values[1] = valuesPair;
        do {
            spawnRequest.tail = tail;
        } while (0);
    } while (0);
    do {
        spawnRequest.block = *(SefRequestKindSpawn *)&record->ownerKey;
    } while (0);
    spawnRequest.block.spawn.spawnEnabled = 0;
    spawnRequest.block.spawn.spawnBits = 0;
    if (spawnRequest.object == 0) {
        spawnRequest.object = spawnRequest.owner;
    }
    if (spawnBits != 0) {
        bit = 1;
        for (i = 0; i < 8; i++) {
            if (spawnBits & bit) {
                spawnRequest.block.kindScale.kind = 0x136000 | (8 - i);
                sefCreateEffect2(&spawnRequest);
            }
            bit <<= 1;
        }
    }
    if (spawnKind < 3) {
        spawnRequest.block.kindScale.kind = 0x136009 + spawnKind;
        sefCreateEffect2(&spawnRequest);
    }
}

void sefCopyRecordMatrix(Matrix4 *dst, Matrix4 *src)
{
    if (src != 0) {
        QuadwordCopyMatrixT0(dst, src + 4);
    } else {
        __asm__ __volatile__(
            "lqc2 $vf1, 32(%1)\n"
            "lqc2 $vf2, 16(%1)\n"
            "lqc2 $vf3, 0(%1)\n"
            "sqc2 $vf0, 48(%0)\n"
            "sqc2 $vf1, 32(%0)\n"
            "sqc2 $vf2, 16(%0)\n"
            "sqc2 $vf3, 0(%0)\n"
            :
            : "r" (dst), "r" (MMathUnitMatrix)
            : "memory");
    }
}


float sefGetMatrixScale(const Vector4 *vector) {
    return MMathCalcLength(vector);
}

typedef struct SefEntry {
    unsigned char unmodeled_000[0x1000];
    Vector4 slotVectors[64];    /* +0x1000: copied out by sefCopyRecordSlotVector */
    Vector4 slotOffsets[16];    /* +0x1400: position minus slot vector, one per reset index */
    DoublewordVector4 weightedOffsets[16]; /* +0x1500: offset components times the weight, one per reset index */
    signed char slotCounts[16]; /* +0x1600: counted per reset index */
    Vector4 resetVectors[2];    /* +0x1610, +0x1620: VU0 origin written by sefRebuildEntryTable */
    int id;                     /* +0x1630 */
    int flags;                  /* +0x1634: cleared by sefResetEntryFlags */
    float scale;
    signed char slotFlags[0x20]; /* +0x163C: one flag byte per slot, bit 0 = slot set */
    short mode;                 /* +0x165C: value 2 skips the entry; cleared as one halfword by sefRebuildEntryTable */
    short resetHalf;            /* +0x165E: cleared by sefResetEntryFlags */
    short resetTail;            /* +0x1660: cleared by sefRebuildEntryTable */
    short slotFlag;             /* +0x1662: set to 1 while the slots are in use */
    unsigned char unmodeled_1664[0x0C];
} SefEntry;

typedef struct SefRequestBuffer {
    int argument[7];                /* +0x00: the request words p0..p6 */
    int kind;                       /* +0x1C */
    unsigned char unmodeled_020[0x0C];
    short mode;                     /* +0x2C */
    unsigned char unmodeled_02E[0x80 - 0x2E];
    Matrix4 matrix[8];              /* +0x80 */
    unsigned char unmodeled_280[0x30C - 0x280];
    int flagWord;                   /* +0x30C */
    unsigned char unmodeled_310[0x314 - 0x310];
    float fieldA5C;                 /* +0x314: the record's fieldA5C, copied by sefExecScheduler */
    struct SefEntry *found[7];      /* +0x318: found[0] is the lookup of p0 (zero when p7 is set), found[1..6] those of p1..p6 */
} SefRequestBuffer;

/* The record sefLoadRequestBuffer reads its source from: a flag byte, a matrix pointer and an inline matrix. */
typedef struct SefSpawnSource {
    unsigned char unmodeled_000[0x02];
    unsigned char flags;            /* +0x02 */
    unsigned char unmodeled_003[0x264 - 0x03];
    Matrix4 *matrixPointer;         /* +0x264 */
    unsigned char unmodeled_268[0x270 - 0x268];
    Matrix4 matrix;                 /* +0x270 */
} SefSpawnSource;

struct SefEntry *sefFindEntryById(int id);
int sefGetKindSlotVector(Vector4 *destination, int unused, int slot, int kind);
float sefHiddenOriginHeight = -20000.0f;
float sefEntryScaleFactor = 0.100000001f;
extern Vector4 sefEntryScaleOffsetVector;

/* The unit matrix copy of sefCopyRecordMatrix's null branch, written inline: rows 0..2 from MMathUnitMatrix, row 3 from vf0. */
static inline void sefCopyUnitMatrix(Matrix4 *dst)
{
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (dst), "r" (MMathUnitMatrix)
        : "memory");
}

/* The request buffer, reached through its global at each use (the original rebuilds the address of sefRequestBuffer per
 * statement after each call, so no request pointer is held across the calls). */
static inline SefRequestBuffer *sefRequestView(void)
{
    return (SefRequestBuffer *)sefRequestBuffer;
}

void sefLoadRequestBuffer(int p0, int p1, int p2, int p3, int p4, int p5, int p6, int p7)
{
    SefSpawnSource *source;
    Matrix4 *slots;
    Vector4 localB;
    Vector4 localA;

    sefRequestView()->argument[0] = p0;
    sefRequestView()->argument[1] = p1;
    sefRequestView()->argument[2] = p2;
    sefRequestView()->argument[3] = p3;
    sefRequestView()->argument[4] = p4;
    sefRequestView()->argument[5] = p5;
    sefRequestView()->argument[6] = p6;
    sefRequestView()->found[0] = p7 == 0 ? sefFindEntryById(p0) : 0;
    sefRequestView()->found[1] = sefFindEntryById(p1);
    sefRequestView()->found[2] = sefFindEntryById(p2);
    sefRequestView()->found[3] = sefFindEntryById(p3);
    sefRequestView()->found[4] = sefFindEntryById(p4);
    sefRequestView()->found[5] = sefFindEntryById(p5);
    sefRequestView()->found[6] = sefFindEntryById(p6);

    if (p0 != 0) {
        if (p7 != 0) {
            source = (SefSpawnSource *)p0;
            if (source->flags == 0) {
                sefCopyUnitMatrix(sefRequestView()->matrix);
                sefRequestView()->matrix[0][3][1] = sefHiddenOriginHeight;
            } else if (source->matrixPointer != 0) {
                QuadwordCopyMatrixT0(sefRequestView()->matrix, source->matrixPointer);
            } else {
                QuadwordCopyMatrixT0(sefRequestView()->matrix, &source->matrix);
            }
        } else {
            SefRequestBuffer *request = sefRequestView();
            sefCopyRecordMatrix(request->matrix, (Matrix4 *)p0);
            if (request->flagWord != 0 && request->found[0] != 0) {
                if ((request->found[0]->slotFlags[0] & 1) != 0 &&
                    request->matrix[0][0][0] == 1.0f && request->matrix[0][1][1] == 1.0f &&
                    request->matrix[0][2][2] == 1.0f && request->matrix[0][3][0] == 0.0f &&
                    request->matrix[0][3][1] == 0.0f && request->matrix[0][3][2] == 0.0f) {
                    sefGetKindMatrix(request->matrix, p0, 0, 1);
                }
            }
        }
    } else {
        sefCopyUnitMatrix(sefRequestView()->matrix);
    }

    slots = (Matrix4 *)((unsigned char *)sefRequestBuffer + 0x100);
    sefCopyRecordMatrix(slots, (Matrix4 *)p2);
    sefCopyRecordMatrix(slots - 1, (Matrix4 *)p1);
    sefCopyRecordMatrix(slots + 1, (Matrix4 *)p3);
    sefCopyRecordMatrix(slots + 2, (Matrix4 *)p4);
    sefCopyRecordMatrix(slots + 3, (Matrix4 *)p5);
    sefCopyRecordMatrix(slots + 4, (Matrix4 *)p6);

    slots = (Matrix4 *)((unsigned char *)sefRequestBuffer + 0x100);
    if (p0 != 0) {
        sefGetKindSlotVector(&localA, p0, 1, 1);
        sefGetKindSlotVector(&localB, p2, 1, 0);
        localB.y = 0.0f;
        localA.y = 0.0f;
        sefGetVecMatrix(slots + 5, &localA, &localB, sefDirectionAxes);
        QuadwordCopyT0(&(slots + 5)[0][3], &slots[0][3]);
    } else {
        QuadwordCopyMatrixT0(slots + 5, slots);
    }
    sefEntryScaleOffsetVector.z = sefGetEntryScale(p2) * sefEntryScaleFactor;
}

/* The signal scale lives in the unnamed +0x5F4 float of studio camera 0 (XglCamera.unknown_5f4). */
#define SEF_SIGNAL_SCALE (xglStudioGetCamera2(0)->unknown_5f4)

float sefSignalScale = 0.00999999978f;
extern float sefTimeScale;

void sefMatchEffectEntries(SefEffectRecord *records)
{
    int i;
    int j;
    int key;
    int matched;
    float scale;

    for (i = 0; i < sefEffectRecordCount; i++) {
        SefEffectRecord *record = &records[i];

        matched = 0;
        if (record->inUse != 0 && record->gate != 0 && !(record->flags & (SEF_RECORD_SKIP_STEP | 0x500 | SEF_RECORD_RUNNING)) && record->remaining <= 0) {
            scale = sefSignalScale;
            key = (int)((record->coord_a + record->coord_b * sefTimeScale) * 1000.0f);
            if (key % 10 == 9) {
                key = key + 1;
            }
            key = key / 1000;
            if (key >= 0x3FF) {
                key = 0x3FC;
            }
            for (j = 0; j < 32; j++) {
                SefEffectEntry *entry = &record->entries[j];

                if (entry->state != 0 && key == entry->key) {
                    matched += entry->flags & 1;
                    if (entry->speedPercent >= 0) {
                        SEF_SIGNAL_SCALE = (float)entry->speedPercent * scale;
                    }
                }
            }
            if (matched != 0) {
                sefSpawnRecordChildren(record);
            }
        }
    }
}

#include "main/sef_light_set.h"
#include "main/srs_file.h"

typedef struct SefRecordOwnerScale {
    unsigned char unmodeled_000[0x14];
    float scale;          /* +0x14 */
} SefRecordOwnerScale;

/* The current-record globals written as one-member objects: sefExecScheduler stores them through these views. */
typedef struct SefRecordView {
    SefEffectRecord *record;
} SefRecordView;

typedef struct SefEntryTable {
    SefEntry entries[24];
    AlignedVector4 origin;      /* +0x21A80: the origin vector stored by sefRebuildEntryTable and sefResetEntryFlags */
    int entryCount;
    unsigned char unmodeled_21A94[0x0C];
} SefEntryTable;

extern SefEntryTable sefEntryTable;
extern short _hitFlag;
extern float sefStepScale;
extern int sefActiveOwnerKey;
void sefRebuildEntryTable(void);
void sefUpdateEntryTransforms(void);
void sefTickGroupTimers(void);
void sefResetEntryFlags(void);
void sefExecLoadQue(void);
void sefUpdateEffectSlots(void);
void sefRefreshEntryRows(void);
void sefGetGroupCenter(float *center, int owner, unsigned short key);

/* destination.xyz = minuend.xyz - subtrahend.xyz and destination.w = minuend.w; the destination vector is the
 * statement's memory operand. */
static inline void sefStoreVectorDifferenceXYZ(Vector4 *destination, const float *minuend, const float *subtrahend)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n"
        "lqc2 vf2, 0(%3)\n"
        "vsub.xyz vf1, vf1, vf2\n"
        "sqc2 vf1, 0(%1)\n"
        : "+m" (*destination)
        : "r" (destination), "r" (minuend), "r" (subtrahend)
        : "memory");
}

/* One effect frame for every record (or only for `only` when it is given): updates the camera pair, then steps each
 * live record's time, request and matrix, runs its 32 effect slots, its camera, sound, wind, special and alter blocks,
 * and restarts or releases the record when no slot is alive. Returns whether a record is still active. */
int sefExecScheduler(SefEffectRecord *only)
{
    short hit;
    XglCamera *camera;
    SefEffectRecord *record;
    int active;
    int i;
    int offset;
    int running;
    int alive;
    int frame;
    int time;
    SefRequestBuffer *request;
    int ownerKey;
    int loadTag;
    int object;
    int k;

    sefLineFlag4000Count = 0;
    sefVectorMode = 0;
    sefRebuildEntryTable();
    sefUpdateEntryTransforms();
    sefTickGroupTimers();
    sdvExecAmbient();
    sefResetEntryFlags();
    sdvClearSpecialWork();
    sefExecLoadQue();
    sefUpdateEffectSlots();
    camera = xglStudioGetActiveCamera();
    active = 0;
    if (sefCameraState.valid != 0) {
        QuadwordCopyMatrixT0(&sefCameraState.previous, &sefCameraState.current);
    } else {
        QuadwordCopyMatrixT0(&sefCameraState.previous, &camera->travel.transform);
        sefCameraState.valid = 1;
    }
    QuadwordCopyMatrixT0(&sefCameraState.current, &camera->travel.transform);
    sefCameraState.current[3][1] = 0.0f;
    sefCameraState.inverse = &camera->travel.inverseTransform;
    sefCameraState.previous[3][1] = 0.0f;
    sefStoreVectorDifferenceXYZ(&sefCameraState.motion, sefCameraState.current[3], sefCameraState.previous[3]);
    sefTimeScale = SEF_SIGNAL_SCALE * sefStepScale;
    if (only == 0) {
        sefMatchEffectEntries(sefEffectRecords);
    }
    for (i = 0; i < sefEffectRecordCount; i++) {
        record = &sefEffectRecords[i];
        alive = 0;
        hit = _hitFlag;
        running = (unsigned short) record->flags & SEF_RECORD_RUNNING;
        if (only != 0 && record != only) {
            continue;
        }
        if (record->inUse != 0 && !(record->flags & SEF_RECORD_SKIP_STEP)) {
            if (record->remaining > 0) {
                active++;
                record->remaining--;
                continue;
            }
            ownerKey = record->ownerKey;
            loadTag = record->loadTag;
            sefPointTimeScale = record->coord_b * sefTimeScale;
            /* The three global stores keep their one-member view casts: plain stores change the code. */
            ((SefWordView *) &sefActiveOwnerKey)->value = ownerKey;
            ((SefWordView *) &sefCurrentLoadTag)->value = loadTag;
            ((SefRecordView *) &sefCurrentRecord)->record = record;
            record->coord_a += sefPointTimeScale;
            if ((int) record->coord_a >= 0x3FF) {
                record->coord_a = 1020.0f;
            }
            time = (int) (record->coord_a * 1000.0f);
            if (time % 10 == 9) {
                time++;
            }
            record->coord_a = time * 0.001;
            frame = time / 1000;
            *(SefRequestKindSpawn *) &((SefRequestBuffer *) sefRequestBuffer)->kind = *(SefRequestKindSpawn *) &record->ownerKey;
            object = (int) record->object;
            request = (SefRequestBuffer *) sefRequestBuffer;
            ((SefLineRequest *) request)->flags = record->flags;
            request->kind = record->ownerKey;
            request->flagWord = running;
            request->mode = (signed char) record->requestMode;
            request->fieldA5C = record->fieldA5C;
            sefLoadRequestBuffer((int) *(SefRecordOwnerScale **) &record->owner, (int) record->ownerHigh, object, record->values[0],
                          record->values[1], record->values[2], record->requestTail, record->flags & SEF_RECORD_SKIP_REGISTER);
            sefPointMotion.floorBase = 0.0f;
            if (((sefActiveOwnerKey >> 28) & 3) == 1 && record->owner != 0) {
                sefPointMotion.floorBase = ((SefRecordOwnerScale *) record->owner)->scale;
            }
            sefGetGroupCenter((float *) &sefEntryTable.origin, sefActiveOwnerKey, record->value);
            ((Vector4 *) record)->w = 1.0f;
            if (sefCalcRotTransSMatrix(&record->rotation, &record->position, &record->scale, 0)) {
                record->flags |= 0x1000000;
            } else {
                record->flags &= ~0x1000000;
            }
            if (!running && (record->flags & SEF_RECORD_PENDING) && frame >= 3) {
                record->flags &= ~SEF_RECORD_PENDING;
                sefOrEntryFlags((int) *(SefEntry **) sefRequestBuffer, 0x1000);
            }
            for (k = 0; k < 32; k++) {
                if (sefExecEffectData((SefEffectSlot *) &sefEffectRecords[i].entries[k], frame, &hit) != 0) {
                    alive++;
                }
            }
            if (!(sefEffectRecords[i].flags & 0x40) &&
                (!(sefEffectRecords[i].flags & 0x200) || sefCameraOwnerRecord == (int) &sefEffectRecords[i])) {
                if (sdvScheduleCamera(&sefEffectRecords[i].camera, frame) != 0) {
                    if (!(sefEffectRecords[i].flags & 0x200)) {
                        sefCameraOwnerRecord = (int) &sefEffectRecords[i];
                        sefEffectRecords[i].flags |= 0x200;
                    }
                }
            }
            if (!running) {
                sdvExecEffectSeq(&sefEffectRecords[i].effectSeq, sefEffectRecords[i].entryId);
                sdvExecShadowSeq(&sefEffectRecords[i].shadowSeq);
                tiWindUpdateKeyframes(&sefEffectRecords[i].wind, record->ownerKey, record->value);
            }
            sdvExecSpecial(&sefEffectRecords[i].special, sefEffectRecords[i].loadTag);
            sdvScheduleAlter(&sefEffectRecords[i].alter, (SdvSefRequest *) &sefEffectRecords[i].owner, sefEffectRecords[i].entryId, i);
            if (!(gameWork.control & SEF_DRAW_FLAG_PENDING) || sefEffectRecordCount == 128) {
                sdvDrawSpecial(&sefEffectRecords[i].special);
            }
            if (alive == 0) {
                if (!running) {
                    tiWindReleaseSlots(record->ownerKey, record->value);
                }
                if (sefEffectRecords[i].linkValue >= 0 || (sefEffectRecords[i].flags & 0x20)) {
                    for (k = 0; k < 32; k++) {
                        sefDestroyEffectData((EffectData *) &sefEffectRecords[i].entries[k]);
                    }
                    sefEffectRecords[i].coord_a = sefEffectRecords[i].linkValue;
                    sefInitEffectTbl((SefEffectLoadEntry *) sefEffectRecords[i].inUse, i, sefEffectRecords[i].loadFlags);
                    i--;
                } else {
                    if (running) {
                        uwamonoReleaseOwnedRecords((SefOwner *) record);
                    }
                    sefFreeScheduler(i);
                }
                continue;
            }
            if (!(sefEffectRecords[i].flags & 0x40)) {
                active++;
            }
        }
        sefEffectRecords[i].flags &= ~0x9;
    }
    sefRefreshEntryRows();
    sefVectorMode = 0;
    return active > 0;
}

/* Returns 1 when the index is outside the table or its record is free, else 0. */
int sefIsDeadSchduler(int index) {
    if ((index < 0) || (index >= sefEffectRecordCount)) {
        return 1;
    }
    return sefEffectRecords[index].inUse == 0;
}

static int sefHandleTable[34];

void sefClearHandleTable(void) {
    memset(sefHandleTable, 0, 0x80);
}

void sefAddHandle(int handle)
{
    int *slot;
    int free;
    int i;
    int value;

    free = -1;
    i = 0;
    slot = sefHandleTable;
    do {
        value = *slot;
        slot++;
        if (value == handle) {
            return;
        }
        if (value == 0) {
            if (free < 0) {
                free = i;
            }
        }
        i++;
    } while (i < 32);
    sefHandleTable[free] = handle;
}

/* Clears every entry of the 32-entry table sefHandleTable that holds handle. */
void sefRemoveHandle(int handle) {
    int i;
    int *entry = sefHandleTable;

    for (i = 0; i < 32; i++) {
        if (*entry == handle) {
            *entry = 0;
        }
        entry++;
    }
}


SefEntryTable sefEntryTable = { 0 };
/* Registers an actor handle in the first free entry: clears the entry's mode and tail, stores the flag and
 * takes the entry scale from half the actor's local height, clamped to [0.25, 4.0]. */
void sefRegisterEntry(int handle, int mode)
{
    int index;
    Vector4 output;

    for (index = 0; index < sefEntryTable.entryCount; index++) {
        if (sefEntryTable.entries[index].id == handle) {
            break;
        }
    }
    if (index != sefEntryTable.entryCount) {
        return;
    }
    if (index >= 24) {
        return;
    }
    sefEntryTable.entries[index].resetTail = 0;
    sefEntryTable.entryCount = index + 1;
    sefEntryTable.entries[index].id = handle;
    sefEntryTable.entries[index].mode = mode;
    if (sefActorMulLocalVector((XenoActor *)handle, 0, &output, 0) != 0) {
        sefEntryTable.entries[index].scale = output.y * 0.5f;
    }
    if (4.0f < sefEntryTable.entries[index].scale) {
        sefEntryTable.entries[index].scale = 4.0f;
    } else if (sefEntryTable.entries[index].scale < 0.25f) {
        sefEntryTable.entries[index].scale = 0.25f;
    }
}

extern s16 _sefBattleMode;

/* Hands the handle of every listed entry to sefRegisterEntry when the gate is set. */
void sefRegisterListEntries(void)
{
    BattleUnit **entries;
    BattleUnit *entry;
    int count;
    int index;
    int handle;

    if (_sefBattleMode != 0) {
        count = unitTblGet(UNIT_SIDE_ALL, &entries);
        index = 0;
        if (count > 0) {
            do {
                entry = entries[index];
                index++;
                if (entry != 0) {
                    handle = (int)entry->unit;
                    if (handle != 0) {
                        sefRegisterEntry(handle, 1);
                    }
                }
            } while (index < count);
        }
    }
}

/* sqc2 of the VU0 origin (0, 0, 0, 1) into one vector; the store is the asm's memory output. */
static inline void sefSetOrigin(AlignedVector4 *vector)
{
    __asm__ __volatile__("sqc2 vf0, 0(%1)\n" : "=m" (*vector) : "r" (vector) : "memory");
}

/* Resets the 24 entries of sefEntryTable (their last two slot vectors take the VU0 origin), clears the table's count, then
 * hands the owner, object, high owner word and values of every live effect record to sefRegisterEntry and registers the list. */
void sefRebuildEntryTable(void)
{
    SefEffectRecord *record;
    int index;

    sefEntryTable.entryCount = 0;
    sefSetOrigin(&sefEntryTable.origin);
    for (index = 0; index < 24; index++) {
        sefEntryTable.entries[index].id = 0;
        sefEntryTable.entries[index].resetTail = 0;
        sefEntryTable.entries[index].mode = 0;
        sefEntryTable.entries[index].resetHalf = 0;
        sefEntryTable.entries[index].flags = 0;
        sefSetOrigin(&sefEntryTable.entries[index].resetVectors[0]);
        sefSetOrigin(&sefEntryTable.entries[index].resetVectors[1]);
    }
    for (index = 0; index < sefEffectRecordCount; index++) {
        record = &sefEffectRecords[index];
        if (record->inUse != 0 && (record->flags & SEF_RECORD_SKIP_REGISTER) == 0) {
            if (record->owner != 0) {
                sefRegisterEntry(record->owner, 1);
            }
            if (record->object != 0) {
                sefRegisterEntry((int)record->object, 0);
            }
            if (record->ownerHigh != 0) {
                sefRegisterEntry((int) record->ownerHigh, 1);
            }
            if (record->values[0] != 0) {
                sefRegisterEntry(record->values[0], 2);
            }
            if (record->values[1] != 0) {
                sefRegisterEntry(record->values[1], 2);
            }
            if (record->values[2] != 0) {
                sefRegisterEntry(record->values[2], 2);
            }
            if (record->requestTail != 0) {
                sefRegisterEntry(record->requestTail, 2);
            }
        }
    }
    sefRegisterListEntries();
}

void sefOrEntryFlags(int key, int flags) {
    SefEntry *entry;
    int i;

    if (key == 0) {
        return;
    }
    i = 0;
    if (sefEntryTable.entryCount <= 0) {
        return;
    }
    if (sefEntryTable.entries[0].id == key) {
        sefEntryTable.entries[0].flags |= flags;
        return;
    }
    /* Goto loop kept: gcc 2.96 leaves a goto-built loop without loop notes (no strength reduction); a do/while or
       for loop here (from 0 or from 1 after the entries[0] test) is strength-reduced / reloads differently and the
       object differs (re-tried in pass 3: the loop bound is reloaded from sefEntryTable every iteration). */
next:
    i++;
check:
    if (i >= sefEntryTable.entryCount) {
        return;
    }
    entry = &sefEntryTable.entries[i];
    if (entry->id != key) {
        goto next;
    }
    entry->flags |= flags;
}

void sefClearEntryFlags(int key, int flags)
{
    SefEntry *entry;
    int i;

    if (key == 0) {
        return;
    }
    entry = &sefEntryTable.entries[0];
    i = 0;
    if (sefEntryTable.entryCount <= 0) {
        return;
    }
    if (entry->id != key) {
        goto loop; /* enters the loop mid-body: no loop notes, as in the original (see sefOrEntryFlags) */
        do {
            entry = 0;
loop:
            i++;
            if (i >= sefEntryTable.entryCount) {
                return;
            }
            entry = &sefEntryTable.entries[i];
        } while (entry->id != key);
    }
    entry->flags &= ~flags;
}

/* Returns the flags of the first entry of the actor, or 8 when none matches (and for no actor). */
int sefGetEntryFlags(struct XenoActor *actor)
{
    SefEntryTable *table;
    SefEntry *entry;
    int i;

    if (actor != 0) {
        table = &sefEntryTable;
        i = 0;
        if (table->entryCount > 0) {
            do {
                entry = &table->entries[i];
                if (entry->id == (int) actor) {
                    return entry->flags;
                }
                i++;
            } while (i < table->entryCount);
        }
    }
    return 8;
}

int sefIsHitActor(struct XenoActor *actor) {
    return sefGetEntryFlags(actor) & 1;
}

int sefEntryHasFlag2(struct XenoActor *actor) {
    return sefGetEntryFlags(actor) & 2;
}

/* Fetches the actor's output for input through its handle channel (tiMRPackageGetNodeMatrix); on success, when apply is set,
   multiplies the output by the actor's local matrix. Same shape as XenoActorMulMatrix without its override matrix. */
int sefActorMulLocalMatrix(XenoActor *actor, int input, void *output, int apply)
{
    int ok;

    ok = tiMRPackageGetNodeMatrix(XenoActorMotionPack(actor), input, output, 1);
    if (ok != 0) {
        if (apply != 0) {
            MMathMulMatrix(output, &actor->localMatrix.m, output);
        }
        return 1;
    }
    return 0;
}

/* Same as sefActorMulLocalMatrix with the second callee (tiMRPackageGetNodePoint) and MMathApplyMatrix: the vector form, as
   XenoActorMulVector in xenoActor.c, here without its override matrix. */
int sefActorMulLocalVector(XenoActor *actor, int input, void *output, int apply)
{
    int ok;

    ok = tiMRPackageGetNodePoint(XenoActorMotionPack(actor), input, output, 1);
    if (ok != 0) {
        if (apply != 0) {
            MMathApplyMatrix(output, &actor->localMatrix.m, output);
        }
        return 1;
    }
    return 0;
}

extern int sefFallbackVectorFlag;
const Vector4 sefFallbackVectorsEven[16] = { { .x = 0.0f, .y = 2.0f, .z = 2.0f, .w = 1.0f }, { .x = 0.40000000596046448f, .y = 0.80000001192092896f, .z = 2.0f, .w = 1.0f }, { .x = -0.20000000298023224f, .y = 1.1000000238418579f, .z = 2.0f, .w = 1.0f }, { .x = 0.40000000596046448f, .y = 0.80000001192092896f, .z = 2.0f, .w = 1.0f }, { .x = 0.20000000298023224f, .y = 1.1000000238418579f, .z = 2.0f, .w = 1.0f }, { .x = -0.10000000149011612f, .y = 0.0f, .z = 2.0f, .w = 1.0f }, { .x = -0.10000000149011612f, .y = 0.60000002384185791f, .z = 2.0f, .w = 1.0f }, { .x = 0.10000000149011612f, .y = 0.0f, .z = 2.0f, .w = 1.0f }, { .x = 0.10000000149011612f, .y = 0.60000002384185791f, .z = 2.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = 2.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = 2.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = 2.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = 2.0f, .w = 1.0f }, { .x = 0.0f, .y = 1.7000000476837158f, .z = 2.0f, .w = 1.0f }, { .x = 0.0f, .y = 1.5499999523162842f, .z = 2.1500000953674316f, .w = 1.0f }, { .x = 0.0f, .y = 1.2999999523162842f, .z = 2.1500000953674316f, .w = 1.0f } };

const Vector4 sefFallbackVectorsOdd[16] = { { .x = 0.0f, .y = 2.0f, .z = -2.0f, .w = 1.0f }, { .x = 0.40000000596046448f, .y = 0.80000001192092896f, .z = -2.0f, .w = 1.0f }, { .x = -0.20000000298023224f, .y = 1.1000000238418579f, .z = -2.0f, .w = 1.0f }, { .x = 0.40000000596046448f, .y = 0.80000001192092896f, .z = -2.0f, .w = 1.0f }, { .x = 0.20000000298023224f, .y = 1.1000000238418579f, .z = -2.0f, .w = 1.0f }, { .x = -0.10000000149011612f, .y = 0.0f, .z = -2.0f, .w = 1.0f }, { .x = -0.10000000149011612f, .y = 0.60000002384185791f, .z = -2.0f, .w = 1.0f }, { .x = 0.10000000149011612f, .y = 0.0f, .z = -2.0f, .w = 1.0f }, { .x = 0.10000000149011612f, .y = 0.60000002384185791f, .z = -2.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = -2.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = -2.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = -2.0f, .w = 1.0f }, { .x = 0.0f, .y = 0.0f, .z = -2.0f, .w = 1.0f }, { .x = 0.0f, .y = 1.7000000476837158f, .z = -2.0f, .w = 1.0f }, { .x = 0.0f, .y = 1.5499999523162842f, .z = -1.8500000238418579f, .w = 1.0f }, { .x = 0.0f, .y = 1.2999999523162842f, .z = -1.8500000238418579f, .w = 1.0f } };

/* Bit test of a vector's xyz as the integer words: z as an int, x and y as one 64-bit word. */
static inline int sefVectorXyzIsZero(const Vector4 *vector)
{
    return (*(int *) &vector->z | *(u64 *) vector) == 0;
}

/* The effect entry as sefUpdateEntryTransforms fills it: the slot vectors are float[4] lanes there. */
typedef struct SefEntryLanes {
    Matrix4 worldMatrices[32];  /* +0x000: sefActorMulLocalMatrix output, apply set */
    Matrix4 localMatrices[32];  /* +0x800: sefActorMulLocalMatrix output, apply clear */
    float slotVectors[64][4];   /* +0x1000: copied out by sefCopyRecordSlotVector; float[4] lanes like sceVu0FVECTOR */
    Vector4 slotOffsets[16];    /* +0x1400: position minus slot vector, one per reset index */
    DoublewordVector4 weightedOffsets[16]; /* +0x1500: offset components times the weight, one per reset index */
    signed char slotCounts[16]; /* +0x1600: counted per reset index */
    unsigned char unmodeled_1610[0x20];
    int id;                     /* +0x1630 */
    int flags;                  /* +0x1634: cleared by sefResetEntryFlags */
    float scale;
    signed char slotFlags[0x20]; /* +0x163C: one flag byte per slot, bit 0 = slot set */
    short mode;                 /* +0x165C: value 2 skips the entry; cleared as one halfword by sefRebuildEntryTable */
    short resetHalf;            /* +0x165E: cleared by sefResetEntryFlags */
    short resetTail;            /* +0x1660: cleared by sefRebuildEntryTable */
    short slotFlag;             /* +0x1662: set to 1 while the slots are in use */
    unsigned char unmodeled_1664[0x0C];
} SefEntryLanes;

/* Copies one quadword through t0; the two outputs tell the compiler the whole destination vector is written. */
static inline void sefCopyEntryVector(Vector4 *destination, const Vector4 *source)
{
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n"
                         : "=m"(*destination), "=m"(destination->w)
                         : "r"(destination), "r"(source)
                         : "$8", "memory");
}

/* Refreshes every entry of sefEntryTable from its actor: 32 matrices and 32 vectors through the actor's
 * channel callees, and a fallback copy of a 16-entry table when the actor or its status block is missing. */
void sefUpdateEntryTransforms(void)
{
    int j;
    int i;
    SefEntryLanes *entry;
    TiMRPackage *channel;
    const Vector4 *fallbackTable;
    Vector4 unit; /* never read: only w is set, as the original keeps it in the frame */

    unit.w = 1.0f;
    for (i = 0; i < sefEntryTable.entryCount; i++) {
        entry = (SefEntryLanes *) (sefEntryTable.entries + i);
        if (entry->id != 0) {
            channel = XenoActorMotionPack((XenoActor *) entry->id);
            memset(entry->slotFlags, 0, 32);
            if (channel->nodeTable != 0 && channel->nodeTable->count != 0) {
                for (j = 0; j < 32; j++) {
                    sefActorMulLocalMatrix((XenoActor *) entry->id, j, entry->worldMatrices[j], 1);
                    sefActorMulLocalMatrix((XenoActor *) entry->id, j, entry->localMatrices[j], 0);
                    if (sefActorMulLocalVector((XenoActor *) entry->id, j, entry->slotVectors[j], 1)) {
                        entry->slotFlags[j] = 1;
                        if (sefVectorXyzIsZero((Vector4 *) entry->slotVectors[j]) != 0) {
                            entry->slotVectors[j][1] = -10000.0f;
                        }
                    }
                    sefActorMulLocalVector((XenoActor *) entry->id, j, entry->slotVectors[32 + j], 0);
                }
            } else {
                if (sefFallbackVectorFlag != 0) {
                    fallbackTable = (i & 1) ? sefFallbackVectorsOdd : sefFallbackVectorsEven;
                    for (j = 0; j < 16; j++) {
                        sefCopyEntryVector((Vector4 *) entry->worldMatrices[j][3], &fallbackTable[j]);
                        sefCopyEntryVector((Vector4 *) entry->slotVectors[j], &fallbackTable[j]);
                    }
                }
            }
        }
    }
}

SefEntry *sefFindEntryById(int id)
{
    SefEntryTable *table;
    SefEntry *entry;
    int i;

    if (id != 0) {
        table = &sefEntryTable;
        for (i = 0; i < table->entryCount; i++) {
            entry = &table->entries[i];
            if (entry->id == id) {
                return entry;
            }
        }
    }
    return 0;
}

/* Kind of the request that scales the entry (sefGetEntryScale answers 4.0 for mode 3, else 1.0). */
#define SEF_REQUEST_KIND_SCALE 0x140008

float sefGetEntryScale(int id) {
    SefEntry *entry = sefFindEntryById(id);
    SefRequestBuffer *request = (SefRequestBuffer *) sefRequestBuffer;

    if (request->kind == SEF_REQUEST_KIND_SCALE) {
        return request->mode == 3 ? 4.0f : 1.0f;
    }
    return entry != 0 ? entry->scale : 1.0f;
}

#define SEF_GROUP_ENTRY_COUNT 8

/*
 * One group timer, 0x20 bytes, eight of them in sefGroupTable. sefRegisterGroupCenter fills it for a tracked group
 * (the centre of the group's objects, the owner id and the key that name the group) with 600 frames on the timer;
 * sefTickGroupTimers counts `remaining` down; sefGetGroupCenter reads the centre back by owner and key.
 */
typedef struct SefGroupEntry {
    float center[4];               /* +0x00: x, y, z, w of the group centre */
    int owner;                     /* +0x10: owner id compared by sefGetGroupCenter and sefRegisterGroupCenter */
    short remaining;               /* +0x14: frames left; zero or less means the entry is free */
    unsigned short key;            /* +0x16: second id of the group, compared as an unsigned short */
    unsigned char unmodeled_18[8]; /* +0x18..0x1F: not read by the code in this unit */
} SefGroupEntry;

SefGroupEntry sefGroupTable[SEF_GROUP_ENTRY_COUNT] = { 0 };
static SefSlotTable sefSlotTable;

/* Starts the pool allocator, the CD reader, the scheduler and the line records, clears the four work tables (sefSlotTable,
 * sefRequestBuffer, sefGroupTable, sefEntryCounts), then runs the init chain of the ambient, resource, handle and
 * effect slot subsystems. */
void sefInitWorkTables(void) {
    sevInitPtAllocator();
    srsInitCdRead();
    sefInitScheduler();
    sefClearLineDataRecords();
    memset(&sefSlotTable, 0, sizeof(sefSlotTable));
    memset(sefRequestBuffer, 0, 0x340);
    memset(sefGroupTable, 0, 0x100);
    memset(sefEntryCounts, 0, 0x488);
    sdvInitAmbient();
    sefResetResourceTable();
    sefClearHandleTable();
    sefResetEffectSlots();
}

/* Common start: the init chain (sefInitWorkTables among it), then sets the initialized flag sefInitialized = 1 (the gate of
 * the draw passes below) and the time scale sefTimeScale = 1.0f. */
extern s16 sefInitialized;
extern int sefGateOwnerZeroFlag;

void sefStartSystem(void) {
    sefResetBuffers();
    sresFreeReloaderMemory();
    sresInitMemoryRes();
    svInitImageMapper();
    sdvResetSpecialWork();
    sefInitWorkTables();
    svSceneFlagsClear(1);
    sefFlagSet(0);
    sefSetLoadQue(0, 0);
    sefTimeScale = 1.0f;
    sefGateOwnerZeroFlag = 0;
    sefInitialized = 1;
}

unsigned char sefDefaultMemoryBlock[0x152800] = { 0 };

/* Start with the built-in block sefDefaultMemoryBlock (0x152800 bytes): record limits 0x40 (sefEffectRecordCount), 0xC0 and 0x44C,
 * step sefStepScale = 1.0f, then the common start. */
void sefInitEffect(void) {
    smInitilize(sefDefaultMemoryBlock, 0x152800);
    sresInitMemoryRes();
    sefEffectRecordCount = 0x40;
    sefLineDataCount = 0xC0;
    sefPtAllocCount = 0x44C;
    sefStepScale = 1.0f;
    sefStartSystem();
    sefNoiseInit();
}


int sefStartWithMemory(unsigned char *block) {
    unsigned char *pool;

    sefEffectRecordCount = 0x40;
    sefLineDataCount = 0xC0;
    sefPtAllocCount = 0x44C;
    _sefBattleMode = 0;
    smInitilize(sefDefaultMemoryBlock, 0x152800);
    sresInitMemoryRes();
    sefStartSystem();
    sefPtAllocatorState = (SefSlot *) block;
    sefMemClear(block, 0xBD120);
    pool = block + 0xBD120;
    sefLineDataRecords = (SefLocalData *) pool;
    sefMemClear(pool, 0x68400);
    pool = block + 0x125520;
    sefEffectRecords = (SefEffectRecord *) pool;
    sefMemClear(pool, 0x29C00);
    return 0x14F1A0;
}


/* Second start variant: sets the mode flag _sefBattleMode = 1, then the reset of sefDestroyEffect and the subsystem
 * starts. The mirror is sefInitEffectCf, which sets _sefBattleMode = 0 and uses sefDestroyEffectCf. */
void sefInitEffectBattle(void) {
    do {
        _sefBattleMode = 1;
        sefTimeScale = 1.0f;
    } while (0);
    sefEffectRecordHead = 0;
    sdvInitAlters();
    sefDestroyEffect();
    sefInitScheduler();
    sefClearLineDataRecords();
    tiWindClearFirstSlot();
    svSceneFlagsClear(1);
    srsLoadEspFile(1, 0);
    sefSetFogOverride(0, 0);
}


void sefInitEffectCf(void) {
    sefTimeScale = 1.0f;
    sefEffectRecordHead = 0;
    _sefBattleMode = 0;
    sefDestroyEffectCf();
    sefInitWorkTables();
    svSceneFlagsSet(1);
    srsLoadEspFile(0, 1);
    xglCdWaitUntilIdle();
    sefSetFogOverride(0, 0);
}

/* Slot table sefSlotTable: the effect number of each of eight wait slots, with its loadPending flag.
 * This claims a slot for the effect number among the first three entries when it is absent, sets its flag, then calls
 * sefSetupEffect. sefSetupEnemy claims among entries 3..7 the same way, for a positive effect number only. */
void sefSetupPlayer(int effectNo) {
    int i;

    for (i = 0; i < 3; i++) {
        if (sefSlotTable.effectNo[i] == effectNo) {
            return;
        }
    }
    for (i = 0; i < 3; i++) {
        if (sefSlotTable.effectNo[i] == 0) {
            sefSlotTable.effectNo[i] = effectNo;
            sefSlotTable.loadPending[i] = 1;
            sefSetupEffect();
            return;
        }
    }
}

void sefSetupEnemy(int key) {
    int i;

    if (key > 0) {
        for (i = 3; i < 8; i++) {
            if (sefSlotTable.effectNo[i] == key) {
                return;
            }
        }
        for (i = 3; i < 8; i++) {
            if (sefSlotTable.effectNo[i] == 0) {
                sefSlotTable.effectNo[i] = key;
                sefSlotTable.loadPending[i] = 1;
                sefSetupEffect();
                return;
            }
        }
    }
}

/* Releases the slot of the key: for the matching entry calls sefDeleteEffectWait (delete the wait effect) and
 * srsReleaseSlotsByIndex, then clears the id and its flag. */
void sefReleaseID(int key) {
    int i;

    for (i = 0; i < 8; i++) {
        if (sefSlotTable.effectNo[i] == key) {
            sefDeleteEffectWait(key);
            srsReleaseSlotsByIndex(key);
            sefSlotTable.effectNo[i] = 0;
            sefSlotTable.loadPending[i] = 0;
            return;
        }
    }
}

void sefSetupEffect(void) {
    srsLoadPendingEffects(&sefSlotTable);
}

/* Reset: sefSetLoadQue(0, 0), then sefKillEffect(-1) frees every record. The memsets clear sefSlotTable, sefRequestBuffer,
 * sefEntryTable (0x21AA0 bytes) and sefEntryCounts; then the VU0 copy of MMathUnitMatrix into sefCameraState.previous runs: rows 0..2 move
 * through vf3, vf2, vf1 and row 3 is stored from vf0. The second pass repeats the copy into sefCameraState.current, and
 * sefCameraState.valid is cleared. */
void sefDestroyEffect(void)
{
    Matrix4 *matrixDst;
    const Matrix4 *matrixSrc;

    sefSetLoadQue(0, 0);
    sefKillEffect(-1);
    sdvInitAmbient();
    sdvClearSpecialWork();
    sresFreeReloaderMemory();
    memset(&sefSlotTable, 0, sizeof(sefSlotTable));
    memset(sefRequestBuffer, 0, 0x340);
    memset(&sefEntryTable, 0, 0x21AA0);
    memset(sefEntryCounts, 0, 0x488);
    sdvDestroyAlters();
    sdvResetSpecialWork();
    tiWindResetSlots();
    matrixDst = &sefCameraState.previous;
    matrixSrc = &MMathUnitMatrix;
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (matrixDst), "r" (matrixSrc)
        : "memory");
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (&sefCameraState.current), "r" (matrixSrc)
        : "memory");
    sefCameraState.valid = 0;
}

/* The same reset as sefDestroyEffect, with sefResetAll where sresFreeReloaderMemory and tiWindResetSlots would run: the VU0 copy of
 * MMathUnitMatrix into sefCameraState.previous and .current is the same two passes. */
void sefDestroyEffectCf(void)
{
    Matrix4 *matrixDst;
    const Matrix4 *matrixSrc;

    sefSetLoadQue(0, 0);
    sefKillEffect(-1);
    sdvInitAmbient();
    sefResetAll();
    sdvClearSpecialWork();
    memset(&sefSlotTable, 0, sizeof(sefSlotTable));
    memset(sefRequestBuffer, 0, 0x340);
    memset(&sefEntryTable, 0, 0x21AA0);
    memset(sefEntryCounts, 0, 0x488);
    sdvDestroyAlters();
    sdvResetSpecialWork();
    matrixDst = &sefCameraState.previous;
    matrixSrc = &MMathUnitMatrix;
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (matrixDst), "r" (matrixSrc)
        : "memory");
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (&sefCameraState.current), "r" (matrixSrc)
        : "memory");
    sefCameraState.valid = 0;
}

void sefLogKeyNoOp(int value, const unsigned char *name) {
}

/* Loads the effect key: when it is positive tail-calls srsLoadEffectSlot with no buffer. The callers pass the effect
 * group (the key's bits 12 and up) as a second argument, which the function never reads. The dossier lists the
 * string "load" for this function; the bytes of this function do not show it. */
extern unsigned char sefLoadLogLabel[];

void sefLoadEffect(int key, int group) {
    sefLogKeyNoOp(key, sefLoadLogLabel);
    if (key > 0) {
        srsLoadEffectSlot(0, key, 0);
    }
}

unsigned char sefLoadLogLabel[8] = "load";
SefEffectRecord *sefCurrentRecord = 0;
int sefActiveOwnerKey = 0;
int sefLinkedPointHandle = 0;
SefTransformState *sefLinkedPoint = 0;

extern s16 _seSignal;

/* One step of the effect update. sefExecEffect calls it when sefFlagTest(1) returns 0, and sefExecEffectSteps calls
 * it per step for id 0. Clears sefEntryCounts first and _hitFlag, _hitSignal and _seSignal last. */
void sefExecStep(void) {
    memset(sefEntryCounts, 0, 0x488);
    sefAdvanceGroupKey();
    sdvExecAlters();
    sefRunEmitters(0);
    sefSumEntryCounts();
    _hitFlag = 0;
    _hitSignal = 0;
    _seSignal = 0;
}

/* The render callback slots of sRender: sefDrawEffect2D at +0x30 and sefDrawEffect3DCallback at +0x40, installed by sefSetRenderHooks. */

typedef struct SefRenderHooks {
    unsigned char unmodeled_00[0x30];
    void (*draw2D)(void);           /* +0x30 */
    unsigned char unmodeled_34[0x0C];
    void (*draw3D)(void);           /* +0x40 */
} SefRenderHooks;

extern SefRenderHooks sRender;
void sefDrawEffect2D(void);
void sefDrawEffect3DCallback(void);

/* Both arms of the test install the same 2D hook; the original keeps the counter and gate reads that decide between them. */
void sefSetRenderHooks(void)
{
    if (sefLineFlag4000Count != 0 || _sefBattleMode != 0) {
        sRender.draw2D = sefDrawEffect2D;
    } else {
        sRender.draw2D = sefDrawEffect2D;
    }
    sRender.draw3D = sefDrawEffect3DCallback;
}


void sefExecEffect(void)
{
    if (_sefBattleMode == 0) {
        if (gameWork.fogDist.x != 0.0f && gameWork.fogDist.y != 0.0f) {
            Vector4 vector;

            vector.x = gameWork.fogDist.x;
            vector.y = gameWork.fogDist.y;
            vector.z = gameWork.fogDist.z;
            vector.w = gameWork.fogDist.w;
            sefSetFogOverride(&vector, &gameWork.fogColor);
        } else {
            sefSetFogOverride(0, 0);
        }
    }
    if (sefFlagTest(1) == 0) {
        sefExecStep();
    }
    sefSetRenderHooks();
}

/* The sef effect record that sefExecEffectSteps reads: only the word at +0x40 is modeled here. */
/* Runs the frame steps of one effect: stores step in sefStepScale and loops with a float time accumulator from 0 up to frames.
   effectRecord 0 runs sefExecStep per step, then sefRunEmitters(0) per frame; other records clear sefEntryCounts
   and run sefExecScheduler per step when stepKind is 0, or sefRunEmitters per frame otherwise. */
void sefExecEffectSteps(SefEffectRecord *effectRecord, float frames, float step)
{
    float elapsed;

    if (step <= 0.0f) {
        return;
    }
    sefStepScale = step;
    if (effectRecord == 0) {
        elapsed = 0.0f;
        while (elapsed < frames) {
            elapsed += step;
            sefExecStep();
        }
        elapsed = 0.0f;
        while (elapsed < frames) {
            elapsed += 1.0f;
            sefRunEmitters(0);
        }
    } else {
        memset(sefEntryCounts, 0, 0x488);
        if (effectRecord->cellSlot == 0) {
            elapsed = 0.0f;
            while (elapsed < frames) {
                elapsed += step;
                sefExecScheduler(effectRecord);
            }
        } else {
            elapsed = 0.0f;
            while (elapsed < frames) {
                elapsed += 1.0f;
                sefRunEmitters(sefCellOf(effectRecord));
            }
        }
    }
    sefStepScale = 1.0f;
}

/* One-frame-step form of sefExecEffectSteps (step 1.0f): runs the given number of frames for id. */
void sefProgressEffect(SefEffectRecord *record, float frames) {
    sefExecEffectSteps(record, frames, 1.0f);
}


void sefFinishDrawPass(int value) {
    xglPacketModeReset();
    nmlPacketClrTextureCache();
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    xglPacketInitVuStateArg(value);
}

Matrix4 sefCameraMatrix[1] = { 0 };

/* Draw pass installed at sRender +0x30. Guarded by the initialized flag sefInitialized and by the pending bit
 * 0x04000000 of gameWork.control (the X1 SEF_DRAW_FLAG_PENDING value). Ends the 3D mode with svDrawScene2D (the
 * _draw3D flag of sv_get_image_item_161.c is cleared there). */
void sefDrawEffect2D(void) {
    if (sefInitialized != 0) {
        if (!(gameWork.control & SEF_DRAW_FLAG_PENDING)) {
            if (sefGetEntry57Total() != 0 || sefGetActiveCount() != 0) {
                sefCalcInvView(sefCameraMatrix);
                svDrawScene2D();
            }
            if (_sefBattleMode != 0) {
                MSeObjProc(2);
                MSeObjProc(3);
            }
        }
    }
}

void sefDrawEffect2DAndFinish(int value) {
    sefDrawEffect2D();
    sefFinishDrawPass(value);
}


void sefDrawEffect(void) {
    if (sefInitialized != 0) {
        if (!(gameWork.control & SEF_DRAW_FLAG_PENDING)) {
            sefCalcInvView(sefCameraMatrix);
            svDrawEffectRecords3D();
            if (_sefBattleMode != 0) {
                MSeObjProc(0);
                MSeObjProc(1);
            }
            return;
        }
    } else if (!(gameWork.control & SEF_DRAW_FLAG_PENDING)) {
        return;
    }
    sdvClearSpecialWork();
}


/* Sprite draw pass guarded like sefDrawEffect2D (initialized flag sefInitialized, pending bit 0x04000000). When
 * sefGetEntry57ScreenSprites reports yes, svDrawScene3D draws with _draw3D = 1 (SV_RENDER_2D_SPRITE). */
void sefDrawEffect3D(void)
{
    if (sefInitialized != 0) {
        if ((gameWork.control & SEF_DRAW_FLAG_PENDING) == 0) {
            if (sefGetEntry57ScreenSprites() != 0) {
                svDrawScene3D();
            }
        }
    }
}

void sefDrawEffect3DAndFinish(int value) {
    sefDrawEffect3D();
    sefFinishDrawPass(value);
}

void sefDrawEffect3DCallback(void) {
    sefDrawEffect3D();
}

/* Empty hook: nothing is done here. */
void sefLogRequestNoOp(unsigned char *request) {

}

const unsigned char sefDeleteWaitEffectLabel[16] = "delete wait eft";

/* Deletes the wait effect for the id (string "delete wait eft"): finds the wait-slot record of srsRecordTable whose
 * effect number is key and releases the effect records bound to it with sefFreeScheduler. Called by
 * sefReleaseID. */
void sefDeleteEffectWait(int key)
{
    int keyIndex;
    int i;

    keyIndex = -1;
    for (i = 0; i < 8; i++) {
        if ((srsRecordTable.waitSlots[i].handle != 0) && (srsRecordTable.waitSlots[i].effectNo == key)) {
            keyIndex = i + 1;
            break;
        }
    }

    for (i = 0; i < sefEffectRecordCount; i++) {
        if ((sefEffectRecords[i].inUse != 0) && (sefEffectRecords[i].entryId == keyIndex)) {
            sefLogKeyNoOp(sefEffectRecords[i].ownerKey, sefDeleteWaitEffectLabel);
            sefFreeScheduler(i);
        }
    }
}

extern int scKeyRecordMode;


typedef struct SefRequestBytes {
    unsigned char bytes[0x38];
} SefRequestBytes;

void sefRegisterGroupCenter(int owner, unsigned short key);
/* sc_get.c defines it as SefScriptRecord *scGetKeyRecord(int key, int *offset_out, int index). This file keeps the word
 * form: the record address and the offset address are ints, because sefCreateScheduler (int table address),
 * sefCreateScheduler2 (int loadTag, kept as an int field of the record) and scGetImmAdrImmIdx2 (int base) take the
 * record as a word; the typed prototype needs a cast at every use. */
extern int scGetKeyRecord(int kind, int out, int flag);
extern int scKeyRecordGroupCount;

void sefStartEffect(void *request, const Vector4 *position)
{
    SefEffectRecord *record;
    int innerOut;
    int resultOut;
    int result;
    int index;
    int createdIndex;
    int innerResult;
    int kind;
    int ownerCount;

    if (((SefRequestBuffer *)request)->kind > 0) {
        sefLogRequestNoOp(sefRequestBuffer);
        *(SefRequestBytes *)sefRequestBuffer = *(SefRequestBytes *)request;
        kind = ((SefRequestBuffer *)sefRequestBuffer)->kind;
        result = scGetKeyRecord(kind, (int)&resultOut, 1);
        if (result != 0) {
            if (scKeyRecordMode == SEF_START_MODE_GROUP) {
                ownerCount = sefCountOwnerGroupRecords(kind, sefGroupKey);
                if (ownerCount == 0) {
                    innerResult = scGetKeyRecord(((SefRequestBuffer *)sefRequestBuffer)->kind, (int)&innerOut, 0);
                    if (innerResult) {
                        createdIndex = sefCreateScheduler(innerResult + innerOut * 2, (int)sefRequestBuffer, position, innerResult, -1);
                        if (createdIndex >= 0) {
                            if (sefCountMarkedEntries(&sefEffectRecords[createdIndex]) == 0) {
                                sefEffectRecords[createdIndex].flags = (sefEffectRecords[createdIndex].flags | SEF_RECORD_EXCLUDE_FLAG) & 0xFDFFFFFF;
                            } else {
                                sefEffectRecords[createdIndex].flags = sefEffectRecords[createdIndex].flags | 0xA000000;
                            }
                        }
                    }
                } else {
                    sefClearOwnerGroupFlags(kind, sefGroupKey, SEF_RECORD_SLOTS_LOADED);
                }
                if (resultOut) {
                    sefCreateScheduler2((unsigned short *) (result + resultOut * 2),
                                          (SefRecordRequest *) sefRequestBuffer, position, result, scKeyRecordGroupCount * ownerCount, -1, 1);
                }
                sefRegisterGroupCenter(kind, sefGroupKey);
            } else {
                index = sefCreateScheduler(result + resultOut * 2, (int)sefRequestBuffer, position, result, -1);
                if (index >= 0) {
                    record = &sefEffectRecords[index];
                    if (scKeyRecordMode == SEF_START_MODE_FLAG) {
                        record->flags = record->flags | 0x800000;
                    }
                }
            }
        }
    }
}

/* Starts the effect with value 0. */
void sefCreateEffect(void *object) {
    sefStartEffect(object, 0);
}

/* Starts the effect at the given position. */
void sefStartEffectAtPosition(void *object, const Vector4 *position) {
    sefStartEffect(object, position);
}

/* Variant of sefStartEffect that copies the request the same way and returns
 * a pointer to the record it set up in sefEffectRecords, or NULL. */
SefEffectRecord *sefCreateEffect2(void *object)
{
    SefEffectRecord *record = 0;
    int index = -1;
    unsigned int result;
    int resultOut;
    SefRequestBuffer *request = (SefRequestBuffer *)object;

    if (request->kind > 0) {
        *(SefRequestBytes *)sefRequestBuffer = *(SefRequestBytes *)request;
        sefLogRequestNoOp(sefRequestBuffer);
    }
    if (request->kind <= 0) {
        return 0;
    }
    if ((result = scGetKeyRecordFirst(((SefRequestBuffer *)sefRequestBuffer)->kind, (int)&resultOut)) == 0) {
        return 0;
    }
    if (0 <= (index = sefCreateScheduler(result + resultOut * 2, (int)sefRequestBuffer, 0, result, -1))) {
        record = sefEffectRecords + index;
        record->flags = record->flags | 0x140;
        record->flags = record->flags & 0xEFFFFFFF;
        if (SEF_START_MODE_FLAG == scKeyRecordMode) {
            record->flags = record->flags | 0x800000;
        }
    }
    return record;
}

/* Forwards its record to sefFreeSchedulerCf. */
void sefDeleteEffect2(SefEffectRecord *record) {
    sefFreeSchedulerCf(record);
}

/* Maps the four alias ids of the effect table to their canonical keys; any other id passes through. */
int sefResolveEffectAlias(int effectId) {
    if (effectId == 0x100C801D) {
        effectId = 0x100C8399;
    } else if (effectId == 0x100C801E) {
        effectId = 0x100C839A;
    } else if (effectId == 0x100C8023) {
        effectId = 0x100C83A7;
    } else if (effectId == 0x100C8024) {
        effectId = 0x100C83A8;
    }
    return effectId;
}

/* Maps an effect id to its canonical key: the alias table of sefResolveEffectAlias first, then a band of
 * ids folded onto 16-aligned values plus 1. srsLoadEffectSlot and the spawn functions use the key. */
int sefCanonicalEffectId(int effectId) {
    effectId = sefResolveEffectAlias(effectId);
    if ((unsigned int) (effectId + 0xEFF37C7B) < 0x27U) {
        effectId = SEF_SPAWN_ID_BASE;
    } else if ((unsigned int) (effectId + 0xFFED3FFF) <= 0x283E6U) {
        effectId = (effectId & ~0xF) + 1;
    }
    return effectId;
}


/* Starts the effect key: image keys are loaded as a block and pruned, other keys load one slot;
 * a positive or -2 mode registers the handle. */
int sefStartEffectKey(void *first, int key, int mode) {
    svSceneFlagsSet(1);
    if (key <= 0) {
        return 0;
    }
    if (srsIsEffectImageKey(key) != 0) {
        first = 0;
        key &= ~0x3FF;
        mode = srsLoadEffectBlock(key);
        sefPruneHandleSlots();
    } else {
        mode = srsLoadEffectSlot(first, key, mode);
    }
    if (mode > 0 || mode == -2) {
        sefRegisterHandle(key, first);
        if (first == 0) {
            xglCdWaitUntilIdle();
        }
    }
    return mode;
}

int sefStartEffectKeyDefault(void *first, int second) {
    return sefStartEffectKey(first, second, 0);
}


/* Starts the effect named by the text code: decodes it to a key, loads the esp file when the code begins
 * with 'e', then starts the key through sefStartEffectKey in mode 1. */
void sefLoadEffectCfName(void *first, signed char *text) {
    int effectKey;

    effectKey = srsEffectNameToID(text);
    if ((first != 0) && (effectKey != 0) && (text != 0)) {
        if (*text == 'e') {
            srsLoadEspFile(0, 1);
        }
    }
    sefStartEffectKey(first, effectKey, 1);
}

/* The 56-byte request sefCreateEffectCf2 passes to sefCreateScheduler. */
typedef struct SefSpawnRequest {
    unsigned char unmodeled_00[0x1C];
    int effectId;               /* 0x1C: the canonical effect id */
    float scale;                /* 0x20: 1.0 */
    unsigned char unmodeled_24[0x38 - 0x24];
} SefSpawnRequest;

/* The entry read through scKeyRecordEntry: a halfword index into the table of the status word. */
typedef struct SefEntryHeader {
    unsigned char unmodeled_00[0x2];
    unsigned short entryIndex;  /* 0x2: 0xffff marks no entry */
} SefEntryHeader;

extern SefEntryHeader *scKeyRecordEntry;
/* sc_get.c defines it as SefScriptRecord *scGetKeyRecordFirst(int key, int *offset_out); this file uses the word forms (see scGetKeyRecord). */
int scGetKeyRecordFirst(int first, int second);

/* Copies one quadword block with lq/sq through $8; the source block is an in-out memory operand of the asm. */
static inline void sefCopyBlock(Vector4 *destination, Vector4 *source)
{
    __asm__ __volatile__("lq $8, 0(%2)\n"
                         "sq $8, 0(%1)\n"
                         : "+m"(*source)
                         : "r"(destination), "r"(source)
                         : "$8", "memory");
}

/* Spawns the effect record for an effect id. Inside an entry (scKeyRecordEntry set) it hands the remapped key to
 * sefCreateKindResourceSlot; otherwise it creates a record, copies the two optional quadword blocks into it and marks
   it. */
SefEffectRecord *sefCreateEffectCf2(int effectId, const Vector4 *position, Vector4 *rotation, Vector4 *scale,
                                    int slotIndex)
{
    SefEffectRecord *record = 0;
    SefSpawnRequest request;
    SefEntryHeader *entry;
    char stem[256];
    int slot;
    int lookupBase;
    int canonicalId;
    int recordIndex;
    int key;

    if (effectId <= 0) {
        return 0;
    }
    canonicalId = sefCanonicalEffectId(effectId);
    lookupBase = scGetKeyRecordFirst(canonicalId, (int) &slot);
    if (lookupBase == 0) {
        srsFormatEffectStem(effectId, stem, 1);
        return 0;
    }
    entry = scKeyRecordEntry;
    if (entry != 0) {
        if (entry->entryIndex != 0xFFFF) {
            key = sefResolveEffectAlias(effectId);
            if (key == 0x100C838A) {
                key = 0x100C8390;
            } else if (key == 0x100C838B) {
                key = 0x100C8391;
            } else if (key == 0x100C838C) {
                key = 0x100C8392;
            }
            return sefRecordOfCell(sefCreateKindResourceSlot(key, (SefImageSource *) (lookupBase + entry->entryIndex * 2),
                                                             (key & 0x3FF) - 901, position, rotation, scale, slotIndex));
        }
        return 0;
    }
    memset(&request, 0, sizeof(request));
    request.effectId = canonicalId;
    request.scale = 1.0f;
    recordIndex = sefCreateScheduler(lookupBase + slot * 2, (int) &request, position, lookupBase, slotIndex);
    if (recordIndex >= 0) {
        record = &sefEffectRecords[recordIndex];
        if (rotation != 0) {
            sefCopyBlock(&record->rotation, rotation);
        }
        if (scale != 0) {
            sefCopyBlock(&record->scale, scale);
        }
        if (scKeyRecordMode == 61) {
            record->flags |= 0x800000;
        }
        if (((effectId >> 28) & 3) == 2) {
            record->flags |= 0x10;
        }
        record->flags |= SEF_RECORD_RUNNING;
    }
    return record;
}

void sefSetRecordOwner(SefEffectRecord *record, void *owner) {
    if (record != 0) {
        record->owner = (int) owner;
    }
}

void sefSetRecordOwnerMarked(SefEffectRecord *record, void *owner) {
    if (record != 0) {
        record->owner = (int) owner;
        record->flags |= 0x400;
    }
}

void sefSetRecordValues(SefEffectRecord *record, struct XenoActor *first, struct XenoActor *second,
                        struct XenoActor *third) {
    if (record != 0) {
        record->values[0] = (int) first;
        record->values[1] = (int) second;
        record->values[2] = (int) third;
    }
}

void sefSetRecordBlock(SefEffectRecord *record, const Vector4 *source) {
    if (record != 0) {
        QuadwordCopyT0(&record->vertexScale, source);
    }
}

/* Creates the record for an effect id at a position, with a rotation block. */
SefEffectRecord *sefCreateEffectCf(int effectId, const Vector4 *position, Vector4 *rotation) {
    return sefCreateEffectCf2(effectId, position, rotation, 0, -1);
}

/* Decodes the effect code in text, then creates the record for it. */
SefEffectRecord *sefSpawnEffectRecordByCode(signed char *text, const Vector4 *position, Vector4 *rotation,
                                            Vector4 *scale) {
    int effectId;

    effectId = srsEffectNameToID(text);
    return sefCreateEffectCf2(effectId, position, rotation, scale, -1);
}

/* The request sefSpawnOwnedEffect reads: the owner id and the effect id. */
typedef struct SefOwnedRequest {
    int owner;                  /* 0x0: copied to the record's owner */
    unsigned char unmodeled_04[0x1C - 0x04];
    int effectId;               /* 0x1C: passed to sefCreateEffectCf2 */
} SefOwnedRequest;

/* Spawns the record for the owner's effect id and stores the owner in it. */
SefEffectRecord *sefSpawnOwnedEffect(SefOwnedRequest *request, const Vector4 *position, Vector4 *rotation,
                                     Vector4 *scale) {
    SefEffectRecord *record;

    record = sefCreateEffectCf2(request->effectId, position, rotation, scale, -1);
    if (record != 0) {
        record->owner = request->owner;
    }
    return record;
}

/* Releases one effect record. */
void sefDeleteEffectCf(SefEffectRecord *record) {
    sefFreeSchedulerCf(record);
}


/* Releases the in-use records whose entry id is i + 9, for each key-table entry i (entries 9 to 56) whose key is
 * key, then unloads slot i. */
void sefReleaseEntryRecords(void *handle)
{
    int slotIndex;
    int recordIndex;

    for (slotIndex = 0; slotIndex < 48; slotIndex++) {
        if (srsRecordTable.slots[slotIndex].handle == handle) {
            for (recordIndex = 0; recordIndex < sefEffectRecordCount; recordIndex++) {
                if (sefEffectRecords[recordIndex].inUse != 0 && sefEffectRecords[recordIndex].entryId == slotIndex + 9) {
                    sefFreeScheduler(recordIndex);
                }
            }
            srsUnloadEffectSlot(slotIndex);
        }
    }
}

/* Releases the effect records of the owner (an actor): with flag zero only the records whose key is in the 300..399 kind
 * range (top bits clear) are candidates; a candidate whose key is in the kept-key list survives. */
void sefReleaseUnitRecords(void *owner, int releaseAll)
{
    int keptKeys[1];
    int i;
    unsigned int keptIndex;
    unsigned char hit;
    unsigned int key;
    unsigned int kind;

    keptKeys[0] = 0x140008;
    if (owner == 0) {
        return;
    }
    for (i = 0; i < sefEffectRecordCount; i++) {
        if (sefEffectRecords[i].inUse == 0 || sefEffectRecords[i].owner != (int) owner) {
            continue;
        }
        key = sefEffectRecords[i].ownerKey;
        kind = (key >> 12) & 0x3ff;
        hit = ((key >> 28) & 3) == 0 && kind - 300 < 100;
        if ((releaseAll | hit) == 0) {
            continue;
        }
        hit = 0;
        for (keptIndex = 0; keptIndex < 1; keptIndex++) {
            if (key == keptKeys[keptIndex]) {
                hit = 1;
                break;
            }
        }
        if (!hit) {
            sefFreeScheduler(i);
        }
    }
}

/* Releases every in-use record whose owner is ownerId. */
void sefReleaseRecordsOfOwner(int ownerId)
{
    int i;

    if (ownerId != 0) {
        for (i = 0; i < sefEffectRecordCount; i++) {
            /* The record's byte offset from the table address. */
            SefEffectRecord *record = (SefEffectRecord *)(i * sizeof(SefEffectRecord) + (int)sefEffectRecords);

            if (record->inUse != 0 && record->owner == ownerId) {
                sefDeleteEffectCf(record);
            }
        }
    }
}

/* Releases every in-use record whose query key passes srsIsEffectImageKey. */
void sefReleaseImageRecords(void)
{
    int i;

    for (i = 0; i < sefEffectRecordCount; i++) {
        /* The record's byte offset from the table address. */
        SefEffectRecord *record = (SefEffectRecord *)(i * sizeof(SefEffectRecord) + (int)sefEffectRecords);

        if (record->inUse != 0 && srsIsEffectImageKey(record->ownerKey) != 0) {
            sefDeleteEffectCf(&sefEffectRecords[i]);
        }
    }
}

/* SefEffectRecord.inUse holds the owner word; its owner records it as a load-entry table. */
typedef union {
    int owner;
    SefEffectLoadEntry *table;
} SefRecordOwner;

/* Destroys the 32 effect entries of record and reloads its slots, or resets a cell record; then sets or clears the SEF_RECORD_SKIP_STEP flag. */
void sefDestroyRecordEffects(SefEffectRecord *record, int setFlag)
{
    SefEffectEntry *entry;
    int slotIndex;
    int ownerKey;
    int loadTag;
    int entryIndex;
    SefEffectLoadEntry *table;

    if (record == 0) {
        return;
    }
    if (record->inUse == 0) {
        return;
    }
    if (record->cellSlot != 0) {
        sefResetSlotRecords(sefCellOf(record), setFlag);
        return;
    }
    slotIndex = record->slotIndex;
    if (slotIndex >= 0) {
        entry = record->entries;
        for (entryIndex = 31; entryIndex >= 0; entryIndex--) {
            sefDestroyEffectData((EffectData *)entry);
            entry++;
        }
        ownerKey = record->ownerKey;
        ((SefFloatView *) &sefPointTimeScale)->value = record->coord_b;
        table = ((SefRecordOwner *)&record->inUse)->table;
        loadTag = record->loadTag;
        record->coord_a = 0;
        ((SefWordView *) &sefActiveOwnerKey)->value = ownerKey;
        sefCurrentLoadTag = loadTag;
        sefCurrentRecord = record;
        sefInitEffectTbl(table, slotIndex, record->loadFlags);
    }
    if (setFlag != 0) {
        record->flags |= SEF_RECORD_SKIP_STEP;
    } else {
        record->flags &= ~SEF_RECORD_SKIP_STEP;
    }
}

void sefDestroyRecordEffectsClearFlag(SefEffectRecord *record)
{
    sefDestroyRecordEffects(record, 0);
}

/* Resets one record when record is given (its effect data is destroyed and SEF_RECORD_SKIP_STEP is set). Otherwise
 * releases every effect record, clears the shared tables and the line-data pool. */
void sefResetEffects(SefEffectRecord *record)
{
    int recordIndex;

    if (record != 0) {
        sefDestroyRecordEffects(record, 1);
        record->flags |= SEF_RECORD_SKIP_STEP;
        return;
    }
    sefEffectRecordHead = 0;
    for (recordIndex = 0; recordIndex < sefEffectRecordCount; recordIndex++) {
        sefFreeScheduler(recordIndex);
    }
    sdvDestroyAlters();
    sdvClearSpecialWork();
    sdvResetSpecialWork();
    sevInitPtAllocator();
    sefMemClear(sefLineDataRecords, sefLineDataCount * sizeof(SefLocalData));
}

void sefHitEffect(void)
{
    _hitFlag = 1;
}

SefLinkedContext *sefGetBattleData(void)
{
    return (SefLinkedContext *) sefRequestBuffer;
}

unsigned char *sevGetPtAllocator(int slotIndex)
{
    return sefPtAllocatorState[slotIndex].data;
}

/* Returns the data of slot slotIndex. sevGetPtAllocator has the same body and is left unnamed. */
unsigned char *sevGetPtAllocator2(int slotIndex)
{
    return sefPtAllocatorState[slotIndex].data;
}

/* Returns line-data record entryIndex, or null for a negative index. */
SefLocalData *sefGetLineAdr(int entryIndex)
{
    if (entryIndex < 0) {
        return 0;
    }
    return &sefLineDataRecords[entryIndex];
}

/* Returns the base of the effect record table. */
SefEffectRecord *sefGetScheduler(void)
{
    return sefEffectRecords;
}

/* Returns the record most recently stored by sefDestroyRecordEffects (the current record). */
SefEffectRecord *sefGetNowScheduler(void)
{
    return sefCurrentRecord;
}

/* The first index is only checked for a valid value; the slot is selected by the second. */
unsigned char *sefGetParentLine(int checkedIndex, int slotIndex)
{
    if (checkedIndex >= 0 && slotIndex >= 0) {
        return sefPtAllocatorState[slotIndex].data;
    }
    return 0;
}

/* The do-while keeps each call out of tail position, as the original keeps its frame. */
void sefSetSlotDirectionDefault(Matrix4 *destination, const Vector4 *direction)
{
    do {
        sefGetDirMatrix(destination, direction, sefDirectionAxes);
    } while (0);
}

void sefSetSlotDirectionBetweenDefault(void *state, Vector4 *minuend, Vector4 *subtrahend)
{
    do {
        sefGetVecMatrix(state, minuend, subtrahend, sefDirectionAxes);
    } while (0);
}

typedef union SefQuadBlock {
    Quadword quad;
    unsigned int words[4];
} SefQuadBlock;

/* Stores value into each of the size / 4 words starting at dest. */
void sefMemFillWords(void *dest, int value, int size) {
    int *word;
    int count;

    word = dest;
    count = size >> 2;
    if (count > 0) {
        do {
            count -= 1;
            *word = value;
            word += 1;
        } while (count != 0);
    }
}

void sefMemZero(void *dest, int size)
{
    SefQuadBlock *blocks = dest;
    int block_count = size >> 4;
    int word_count = (size & 0xf) >> 2;
    SefQuadBlock *tailBlock = blocks + block_count;
    int i;

    for (i = 0; i < block_count; i++) {
        blocks[i].quad = 0;
    }
    for (i = 0; i < word_count; i++) {
        tailBlock->words[i] = 0;
    }
}

/* Clears size bytes at dest with memset. */
void sefMemClear(void *dest, unsigned int size) {
    memset(dest, 0, size);
}

/* The LCG of the seed: multiplier 0xF82D and increment 0x303B; the high 15 bits (SEF_RAND_MASK) are the sample. */
#define SEF_RAND_MULTIPLIER 0xF82Du
#define SEF_RAND_INCREMENT 0x303Bu
#define SEF_RAND_MASK 0x7FFF
/* Noise table length (samples wrap at 1024 entries) and the scale that maps the samples to the unit range. */
#define SEF_NOISE_TABLE_LEN 1024u
#define SEF_NOISE_SCALE 32768.0f

/* LCG state of sefRandf, stored at sefRandSeed. */
extern unsigned int sefRandSeed;
/* Set to 1 once sefNoiseInit has filled the noise table. */
extern int sefNoiseReady;

/* 1024 uniform noise samples in [-0.5, 0.5) with a read index. sefNoiseNextVector reads three samples at a time and
 * can read the three floats after the 1024th, which sefNoiseInit also fills (1027 samples in all). */
typedef struct SefNoiseTable {
    f32 samples[1024];
    unsigned char unmodeled_1000[12];
    int nextIndex;
} SefNoiseTable;

static SefNoiseTable sefNoiseTable;

/* Fills the noise table once with uniform samples in [-0.5, 0.5) and resets its read index to zero. */
void sefNoiseInit(void)
{
    f32 sample;
    f32 *cursor;
    int i;

    if (sefNoiseReady == 0) {
        sefNoiseReady = 1;
        MMathRandomSeed(0);
        sefNoiseTable.nextIndex = 0;
        cursor = (f32 *)&sefNoiseTable;
        for (i = 0; i < 1027; i++) {
            sample = MMathMakeRandom() - 0.5f;
            *cursor = sample;
            cursor++;
        }
    }
}

/* Random float in [0, 1): one LCG step, the high 15 bits scaled by 2^-15. */
f32 sefRandf(void)
{
    int bits;

    sefRandSeed = sefRandSeed * SEF_RAND_MULTIPLIER + SEF_RAND_INCREMENT;
    bits = (sefRandSeed >> 16) & SEF_RAND_MASK;
    return (f32)bits * (1.0f / 32768.0f);
}

/* Returns the next noise sample and advances the read index of the noise table. */
f32 sefNoiseNext(void)
{
    int index = sefNoiseTable.nextIndex;

    sefNoiseTable.nextIndex = (index + 1) & 0x3FF;
    return sefNoiseTable.samples[index];
}

/* Random float in [0.75, 1): one LCG step of sefRandSeed, the high 15 bits scaled by 2^-15, times 0.25, plus 0.75. */
f32 sefRandfUpperQuarter(void)
{
    unsigned int seed;

    seed = sefRandSeed * SEF_RAND_MULTIPLIER + SEF_RAND_INCREMENT;
    sefRandSeed = seed;
    return (f32) (int) ((seed >> 16) & SEF_RAND_MASK) * 3.0517578125e-05f * 0.25f + 0.75f;
}

/* Single-precision 2*pi, read from the scaffold literal sefTwoPi of this unit. */
f32 sefTwoPi = 6.28318548f;

/* Random angle in [0, 2*pi): the same LCG step as sefRandf, scaled by 2^-15 and then by 2*pi. */
f32 sefRandAngle(void)
{
    int bits;

    sefRandSeed = sefRandSeed * SEF_RAND_MULTIPLIER + SEF_RAND_INCREMENT;
    bits = (sefRandSeed >> 16) & 0x7FFF;
    return (f32)bits * (1.0f / 32768.0f) * sefTwoPi;
}

unsigned int sefRandVectorSeeds[4] = { 0x5CFEDu, 0x198DBBu, 0xEC96Fu, 1 };
unsigned int sefRandVectorIncrements[4] = { 12347, 12347, 12347, 0 };

/* Random vector: steps the x, y and z seeds of the vector LCG (times 0xF82D, then all four lanes plus the increments in one paddw), keeps the high 15 bits of each lane and converts them to floats in out. */
void sefRandVector(Vector4 *out)
{
    unsigned int *laneSeeds;
    Quadword mask;                  /* the 128-bit literal of the original: 0x7FFF in x, y and z, but only 1 in w */

    laneSeeds = sefRandVectorSeeds;
    mask = 0x100007FFF00007FFF00007FFFULL;
    laneSeeds[0] *= 0xF82D;
    laneSeeds[1] *= 0xF82D;
    laneSeeds[2] *= 0xF82D;
    __asm__ __volatile__(
        "lq $8, 0(%1)\n"
        "lq $9, 0(%2)\n"
        "paddw $8, $8, $9\n"
        "sq $8, 0(%1)\n"
        "psrlw $8, $8, 16\n"
        "pand $8, $8, %3\n"
        "qmtc2 $8, vf1\n"
        "vitof0.xyzw vf1, vf1\n"
        "sqc2 vf1, 0(%0)\n"
        : : "r"(out), "r"(laneSeeds), "r"(sefRandVectorIncrements), "r"(mask) : "$8", "$9", "memory");
}

/* Copies the next three noise samples into pos as x, y, z, sets w to 1 and advances the read index by three. */
void sefNoiseNextVector(Vector4 *pos)
{
    DoublewordVector4 *destination = (DoublewordVector4 *) pos;
    int index = sefNoiseTable.nextIndex;

    destination->vector = *(Vector4 *) &sefNoiseTable.samples[index];
    destination->vector.w = 1.0f;
    sefNoiseTable.nextIndex = (index + 3) & 0x3FF;
}

/* Writes the position transformed by the matrix into dest (the position's translation merged into the matrix). */
void sefMergeMatrixPos(Matrix4 *dest, Matrix4 *matrix, const Vector4 *position)
{
    QuadwordMergeMatrixPosition(dest, matrix, position);
}


/* Builds the effect matrix in out (the scratchpad when out is null): the identity rows of MMathUnitMatrix are copied
 * first, then a nonzero translation becomes row 3, a nonzero rotation is applied, and a scale that is not all ones.
 * Returns how many of the three steps ran. */
int sefCalcRotTransSMatrix(const Vector4 *rotation, const Vector4 *translation, const Vector4 *scale, Matrix4 *out)
{
    Matrix4 *dest;
    const Matrix4 *unitMatrix;
    int stepsRun;
    int hasTranslation;
    int hasRotation;

    dest = out;
    if (dest == 0) {
        dest = (Matrix4 *)0x70000000;
    }
    stepsRun = 0;
    unitMatrix = &MMathUnitMatrix;
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (dest), "r" (unitMatrix)
        : "memory");
    hasTranslation = (*(int *)&translation->z | *(u64 *)translation) != 0;
    if (hasTranslation) {
        QuadwordCopyT0((*dest)[3], translation);
        stepsRun = 1;
    }
    hasRotation = (*(int *)&rotation->z | *(u64 *)rotation) != 0;
    if (hasRotation) {
        MMathRotateMatrixZYX(dest, dest, rotation);
        stepsRun++;
    }
    if (scale != 0 && !(scale->x == 1.0f && scale->y == 1.0f && scale->z == 1.0f)) {
        MMathScaleMatrix(dest, dest, scale);
        stepsRun++;
    }
    return stepsRun;
}

void sefCalcInvView(Matrix4 *dest)
{
    XglCamera *camera = xglStudioGetActiveCamera();

    if (dest == 0) {
        dest = sefCameraMatrix;
    }
    sefMergeMatrixPos(dest, &camera->travel.transform, &sefDefaultOffset);
}

/* VU0 product of a Vector4 and a column-major matrix: destination = matrix * vector. */
void sefApplyMatrixVector(Vector4 *destination, Matrix4 *matrix, Vector4 *vector)
{
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(vector) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("lqc2 vf3, 16(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("lqc2 vf4, 32(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf2, vf1x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf3, vf1y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf4, vf1z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf1, vf0, vf1w" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(destination) : "memory");
}

/* Record id that holds an effect id in srsRecordTable. A type 9 effect is looked up by its id in the 48 slot
 * records (ids 9..56), a type 1 effect by bits 12..21 of its id in the eight wait-slot records (ids 1..8). Returns -1
 * when the id is 0 or no record holds it, and 0 for any other effect type. Both loops take their record count
 * from the class being searched (slotClass). */
int sefSearchMapperIndex(int key)
{
    int charNo;
    int effectType;
    int slotClass;
    int recordIndex;

    if (key == 0) {
        return -1;
    }
    srsAnalyzeEftNo(key, &charNo, &effectType);
    if (effectType == 9) {
        slotClass = 1;
        for (recordIndex = 0; recordIndex < (slotClass ? 48 : 8); recordIndex++) {
            if (srsRecordTable.slots[recordIndex].handle != 0 && srsRecordTable.slots[recordIndex].key == key) {
                return recordIndex + 9;
            }
        }
        return -1;
    }
    if (effectType == 1) {
        slotClass = 0;
        for (recordIndex = 0; recordIndex < (slotClass ? 48 : 8); recordIndex++) {
            if (srsRecordTable.waitSlots[recordIndex].handle != 0
                && ((srsRecordTable.waitSlots[recordIndex].key >> 12) & 0x3FF)
                       == ((key >> 12) & 0x3FF)) {
                return recordIndex + 1;
            }
        }
        return -1;
    }
    return 0;
}

/* Copies one 16-byte vector through t0 with the R5900 quadword load and store. */
static inline void sefCopyQuadword(Vector4 *dest, const void *src)
{
    __asm__ __volatile__(
        "lq $8, 0(%2)\n"
        "sq $8, 0(%1)\n"
        : "+m"(*dest)
        : "r"(dest), "r"(src)
        : "$8", "memory");
}

/* Copies the vector of slot `slot` of the record found by key into *destination when that slot's flag byte is set,
 * and returns bit 0 of the flag byte (0 without a record). */
int sefCopyRecordSlotVector(Vector4 *destination, int key, int slot)
{
    SefEntry *entry = sefFindEntryById(key);

    if (entry == 0) {
        return 0;
    }
    if (entry->slotFlags[slot] != 0) {
        sefCopyQuadword(destination, &entry->slotVectors[slot]);
    }
    return entry->slotFlags[slot] & 1;
}


/* One effect object record, 0x1670 bytes. Its per-index flag bytes start at +0x163C; bit 0 is the in-use bit that
 * the kind and slot helpers return. */
typedef struct SefObject {
    unsigned char unmodeled_000[0x163C];
    unsigned char flags;
} SefObject;

/* Returns bit 0 of flag byte linkIndex of the linked object table (objectOffset), or 0 without a link. */
int sefGetLinkedFlag(int linkIndex)
{
    SefLinkedContext *context = sefGetBattleData();
    int offset = context->objectOffset;

    if (offset == 0) {
        return 0;
    }
    return ((SefObject *) (linkIndex + offset))->flags & 1;
}

/* A kind's table: 64 matrices (64 bytes each), 32 slot vectors at +0x1000, then 64 vectors at +0x1200 and, from +0x163C, one flag byte per index. */
typedef struct SefKindTable {
    Matrix4 matrices[64];                       /* +0x0000 */
    Vector4 slotVectors[32];                    /* +0x1000: copied out by sefGetKindSlotVector */
    Vector4 vectors[64];                        /* +0x1200 */
    unsigned char unmodeled_1600[0x163C - 0x1600];
    signed char vectorFlags[64];                /* +0x163C */
} SefKindTable;

/* One 16-byte vector copy through t0. There is no memory clobber or output operand, so the table pointer loaded before
 * the copy stays valid after it, as in the original; the two "m" inputs only name the destination quadword. */
static inline void sefCopySlotVector(Vector4 *dest, Vector4 *src)
{
    __asm__ __volatile__(
        "lq $8, 0(%1)\n"
        "sq $8, 0(%0)\n"
        :
        : "r"(dest), "r"(src), "m"(*dest), "m"(*dest)
        : "$8");
}

/* Copies slot vector `slot` of the kind's record into *destination, first marking a clear slot flag with 0x80; kind 1
 * then overrides y with the context's kind1VectorY when that is nonzero. Returns bit 0 of the slot flag (0 without a
 * record or for an unlisted kind). The second argument is never read. */
int sefGetKindSlotVector(Vector4 *destination, int unused, int slot, int kind)
{
    SefLinkedContext *context = sefGetBattleData();

    if (kind == 1) {
        if (context->kind1Table != 0) {
            if (context->kind1Table->vectorFlags[slot] == 0) {
                context->kind1Table->vectorFlags[slot] = 0x80;
            }
            sefCopySlotVector(destination, &context->kind1Table->slotVectors[slot]);
            if (context->kind1VectorY != 0.0f) {
                destination->y = context->kind1VectorY;
            }
            return context->kind1Table->vectorFlags[slot] & 1;
        }
    } else if (kind == 0) {
        if (context->kind0Table != 0) {
            if (context->kind0Table->vectorFlags[slot] == 0) {
                context->kind0Table->vectorFlags[slot] = 0x80;
            }
            sefCopySlotVector(destination, &context->kind0Table->slotVectors[slot]);
            return context->kind0Table->vectorFlags[slot] & 1;
        }
    } else if (kind == 5) {
        if (context->kind5Table != 0) {
            if (context->kind5Table->vectorFlags[slot] == 0) {
                context->kind5Table->vectorFlags[slot] = 0x80;
            }
            sefCopySlotVector(destination, &context->kind5Table->slotVectors[slot]);
            return context->kind5Table->vectorFlags[slot] & 1;
        }
    } else if (kind == 2) {
        if (context->kind2Table != 0) {
            if (context->kind2Table->vectorFlags[slot] == 0) {
                context->kind2Table->vectorFlags[slot] = 0x80;
            }
            sefCopySlotVector(destination, &context->kind2Table->slotVectors[slot]);
            return context->kind2Table->vectorFlags[slot] & 1;
        }
    } else if (kind == 3) {
        if (context->kind3Table != 0) {
            if (context->kind3Table->vectorFlags[slot] == 0) {
                context->kind3Table->vectorFlags[slot] = 0x80;
            }
            sefCopySlotVector(destination, &context->kind3Table->slotVectors[slot]);
            return context->kind3Table->vectorFlags[slot] & 1;
        }
    } else if (kind == 4) {
        if (context->kind4Table != 0) {
            if (context->kind4Table->vectorFlags[slot] == 0) {
                context->kind4Table->vectorFlags[slot] = 0x80;
            }
            sefCopySlotVector(destination, &context->kind4Table->slotVectors[slot]);
            return context->kind4Table->vectorFlags[slot] & 1;
        }
    } else if (kind == 6) {
        if (context->kind6Table != 0) {
            if (context->kind6Table->vectorFlags[slot] == 0) {
                context->kind6Table->vectorFlags[slot] = 0x80;
            }
            sefCopySlotVector(destination, &context->kind6Table->slotVectors[slot]);
            return context->kind6Table->vectorFlags[slot] & 1;
        }
    }
    return 0;
}

/* Copies the vector at index of the table selected by kind into *out and returns bit 0 of the table's flag byte for
 * that index. The second argument is passed by the caller and never read. */
int sefGetKindVector(Vector4 *out, int unused, int index, int kind)
{
    SefLinkedContext *context = sefGetBattleData();
    SefKindTable *kindTable;

    if (kind == 1) {
        kindTable = (SefKindTable *) context->objectOffset;
        if (kindTable != 0) {
            sefCopyQuadword(out, &kindTable->vectors[index]);
            return kindTable->vectorFlags[index] & 1;
        }
    } else if (kind == 0) {
        kindTable = context->kind0Table;
        if (kindTable != 0) {
            sefCopyQuadword(out, &kindTable->vectors[index]);
            return kindTable->vectorFlags[index] & 1;
        }
    } else if (kind == 5) {
        kindTable = context->kind5Table;
        if (kindTable != 0) {
            sefCopyQuadword(out, &kindTable->vectors[index]);
            return kindTable->vectorFlags[index] & 1;
        }
    } else if (kind == 2) {
        kindTable = context->kind2Table;
        if (kindTable != 0) {
            sefCopyQuadword(out, &kindTable->vectors[index]);
            return kindTable->vectorFlags[index] & 1;
        }
    } else if (kind == 3) {
        kindTable = context->kind3Table;
        if (kindTable != 0) {
            sefCopyQuadword(out, &kindTable->vectors[index]);
            return kindTable->vectorFlags[index] & 1;
        }
    } else if (kind == 4) {
        kindTable = context->kind4Table;
        if (kindTable != 0) {
            sefCopyQuadword(out, &kindTable->vectors[index]);
            return kindTable->vectorFlags[index] & 1;
        }
    } else if (kind == 6) {
        kindTable = context->kind6Table;
        if (kindTable != 0) {
            sefCopyQuadword(out, &kindTable->vectors[index]);
            return kindTable->vectorFlags[index] & 1;
        }
    }
    return 0;
}

/* One matrix copy through t0-t3 for the arms of sefGetKindMatrix; the helper keeps the copies as one shared block. */
static inline void sefCopyKindMatrix(Matrix4 *dest, Matrix4 *src)
{
    QuadwordCopyMatrixT0(dest, src);
}

/* Copies matrix matrixIndex of the kind's table into dest. Kinds 0, 1 and 5 write nothing when their table is empty;
 * the other kinds, and any unlisted kind, copy the unit matrix. */
void sefGetKindMatrix(Matrix4 *dest, int unused, int matrixIndex, int kind)
{
    SefLinkedContext *context = sefGetBattleData();
    Matrix4 *matrixTable;
    const Matrix4 *unitMatrix;

    if (kind == 1) {
        matrixTable = (Matrix4 *) context->objectOffset;
        if (matrixTable == 0) {
            return;
        }
        sefCopyKindMatrix(dest, &matrixTable[matrixIndex]);
        return;
    }
    if (kind == 0) {
        matrixTable = context->kind0Table->matrices;
        if (matrixTable == 0) {
            return;
        }
        sefCopyKindMatrix(dest, &matrixTable[matrixIndex]);
        return;
    }
    if (kind == 5) {
        matrixTable = context->kind5Table->matrices;
        if (matrixTable == 0) {
            return;
        }
        sefCopyKindMatrix(dest, &matrixTable[matrixIndex]);
        return;
    }
    if (kind == 2) {
        matrixTable = context->kind2Table->matrices;
        if (matrixTable != 0) {
            sefCopyKindMatrix(dest, &matrixTable[matrixIndex]);
            return;
        }
    }
    if (kind == 3) {
        matrixTable = context->kind3Table->matrices;
        if (matrixTable != 0) {
            sefCopyKindMatrix(dest, &matrixTable[matrixIndex]);
            return;
        }
    }
    if (kind == 4) {
        matrixTable = context->kind4Table->matrices;
        if (matrixTable != 0) {
            sefCopyKindMatrix(dest, &matrixTable[matrixIndex]);
            return;
        }
    }
    if (kind == 6) {
        matrixTable = context->kind6Table->matrices;
        if (matrixTable != 0) {
            sefCopyKindMatrix(dest, &matrixTable[matrixIndex]);
            return;
        }
    }

    unitMatrix = &MMathUnitMatrix;
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (dest), "r" (unitMatrix)
        : "memory");
}

/* Matrices of a kind table: the second half starts this many matrices after the first. */
#define SEF_KIND_MATRIX_HALF 32

/* The same copy as sefGetKindMatrix, read from the second half of each kind's table (32 matrices further on). */

void sefGetKindMatrixSecondHalf(Matrix4 *dest, int unused, int matrixIndex, int kind)
{
    SefLinkedContext *context = sefGetBattleData();
    Matrix4 *matrixTable;
    const Matrix4 *unitMatrix;

    if (kind == 1) {
        matrixTable = (Matrix4 *) context->objectOffset;
        if (matrixTable == 0) {
            return;
        }
        sefCopyKindMatrix(dest, &matrixTable[matrixIndex + SEF_KIND_MATRIX_HALF]);
        return;
    }
    if (kind == 0) {
        matrixTable = context->kind0Table->matrices;
        if (matrixTable == 0) {
            return;
        }
        sefCopyKindMatrix(dest, &matrixTable[matrixIndex + SEF_KIND_MATRIX_HALF]);
        return;
    }
    if (kind == 5) {
        matrixTable = context->kind5Table->matrices;
        if (matrixTable == 0) {
            return;
        }
        sefCopyKindMatrix(dest, &matrixTable[matrixIndex + SEF_KIND_MATRIX_HALF]);
        return;
    }
    if (kind == 2) {
        matrixTable = context->kind2Table->matrices;
        if (matrixTable != 0) {
            sefCopyKindMatrix(dest, &matrixTable[matrixIndex + SEF_KIND_MATRIX_HALF]);
            return;
        }
    }
    if (kind == 3) {
        matrixTable = context->kind3Table->matrices;
        if (matrixTable != 0) {
            sefCopyKindMatrix(dest, &matrixTable[matrixIndex + SEF_KIND_MATRIX_HALF]);
            return;
        }
    }
    if (kind == 4) {
        matrixTable = context->kind4Table->matrices;
        if (matrixTable != 0) {
            sefCopyKindMatrix(dest, &matrixTable[matrixIndex + SEF_KIND_MATRIX_HALF]);
            return;
        }
    }
    if (kind == 6) {
        matrixTable = context->kind6Table->matrices;
        if (matrixTable != 0) {
            sefCopyKindMatrix(dest, &matrixTable[matrixIndex + SEF_KIND_MATRIX_HALF]);
            return;
        }
    }

    unitMatrix = &MMathUnitMatrix;
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (dest), "r" (unitMatrix)
        : "memory");
}


void sefTransformPartPoint(SefTransformState *part, Vector4 *out, int mode, const Vector4 *offset)
{
    __asm__ __volatile__(
        "lqc2 vf26, 0(%0)\n"
        "lqc2 vf27, 16(%0)\n"
        "lqc2 vf28, 32(%0)\n"
        "lqc2 vf29, 0(%1)\n"
        :
        : "r"(part), "r"(&part->position)
        : "memory");
    if (mode != 0) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n"
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            :
            : "r"(&part->scale), "m"(part->scale));
        if (mode >= 0) {
            Vector4 *point = &part->point;
            vu0TransformPoint(point);
        }
    } else {
        vu0TransformPoint(&part->point);
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n"
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            :
            : "r"(&part->scale)
            : "memory");
    }
    if (offset != 0) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n"
            "vmulax.xyzw ACC, vf26, vf1x\n"
            "vmadday.xyzw ACC, vf27, vf1y\n"
            "vmaddaz.xyzw ACC, vf28, vf1z\n"
            "vmaddw.xyzw vf29, vf29, vf1w\n"
            :
            : "r"(offset)
            : "memory");
    }
    __asm__ __volatile__(
        "sqc2 vf29, 0(%0)\n"
        :
        : "r"(out)
        : "memory");
}

static Matrix4 sefScratchMatrix;
Vector4 sefEntryScaleOffsetVector = { .x = 0.0f, .y = 0.0f, .z = 0.5f, .w = 1.0f };

/* Resolves the point kind to a position in dest and applies the offset in source: through the matrix block the kind
 * selects when there is one (kind 64 scales the offset by the current record's second block first), otherwise by
 * plain addition. Kind 1 transforms the offset by sefCameraState keeping its y; kinds below 9 copy it. Returns the block. */
Matrix4 *sefGetPoint(Vector4 *dest, const Vector4 *source, int kind)
{
    Vector4 flatOffset;
    Vector4 shiftedOffset;
    SefLinkedContext *context;
    Matrix4 *matrixBlock;
    int vectorMode;
    int useScale;
    int sourceNonzero;
    short hasSource;

    matrixBlock = 0;
    useScale = 0;
    vectorMode = sefVectorMode;
    context = sefGetBattleData();
    if (source != 0) {
        sourceNonzero = (((int *) source)[2] | *(long long *) source) != 0;
        hasSource = sourceNonzero;
    } else {
        hasSource = 0;
    }
    __asm__ __volatile__("sqc2 vf0, 0(%1)" : "=m"(*dest) : "r"(dest) : "memory");
    if (kind == 1) {
        if (source == 0) {
            source = &sefDefaultOffset;
        }
        sefCopyQuadword(&flatOffset, source);
        flatOffset.y = 0.0f;
        sefApplyMatrixVector(dest, &sefCameraState.current, &flatOffset);
        dest->y = source->y;
        Vu0SefAddVectorXYZ(dest, dest, sefCameraState.current[3]);
        return 0;
    }
    if (kind < 9) {
        if (hasSource) {
            sefCopyQuadword(dest, source);
        }
        return 0;
    }
    if (kind == 64) {
        if (context->kind1Key != 0 && vectorMode == 0) {
            matrixBlock = &context->kind64Matrix;
            useScale = 1;
        }
    } else if (kind == 256) {
        if (context->kind5Key != 0) {
            matrixBlock = &context->kind256Matrix;
        }
    } else if (kind == 128) {
        if (context->kind0Key != 0) {
            matrixBlock = sefPointMotion.sameDirection != 0 ? &context->kind128AltMatrix : &context->kind128Matrix;
        }
    } else if (kind & 0x40) {
        if (vectorMode != 0) {
            sefGetKindVector(dest, context->kind1Key, kind - 65, 1);
            /* The original compares kind with zero after this call (here and in the 0x400, 0x1000 and 0x200
             * arms, not in the 0x800 one) and what the test guards is dead on this path: no instruction is
             * left, but kind stays live across the call and the argument order follows from that. The
             * guarded statement itself is not recoverable; clearing the mode is the simplest dead one. */
            if (kind != 0) {
                vectorMode = 0;
            }
        } else {
            sefGetKindSlotVector(dest, context->kind1Key, kind - 65, 1);
            sefMergeMatrixPos(&sefScratchMatrix, &context->kind64Matrix, dest);
            matrixBlock = &sefScratchMatrix;
        }
    } else if (kind & 0x100) {
        sefGetKindSlotVector(dest, context->kind5Key, kind - 257, 5);
        sefMergeMatrixPos(&sefScratchMatrix, &context->kind256Matrix, dest);
        matrixBlock = &sefScratchMatrix;
    } else if (kind == 162) {
        if (context->kind0Key != 0) {
            sefCopyKindMatrix(&sefScratchMatrix,
                              sefPointMotion.sameDirection != 0 ? &context->kind128AltMatrix : &context->kind128Matrix);
            sefScratchMatrix[3][1] = 0.0f;
            matrixBlock = &sefScratchMatrix;
        }
    } else if (kind & 0x80) {
        if (kind == 161) {
            if (source != 0) {
                Vu0SefAddVectorXYZ(&shiftedOffset, source, &sefEntryScaleOffsetVector);
            } else {
                QuadwordCopyT0(&shiftedOffset, &sefEntryScaleOffsetVector);
            }
            sefGetKindSlotVector(dest, context->kind0Key, 31, 0);
            sefMergeMatrixPos(&sefScratchMatrix, &context->kind128AltMatrix, dest);
            MMathApplyMatrix(dest, &sefScratchMatrix, &shiftedOffset);
            return &sefScratchMatrix;
        }
        sefGetKindSlotVector(dest, context->kind0Key, kind - 129, 0);
        if (sefPointMotion.sameDirection != 0) {
            sefMergeMatrixPos(&sefScratchMatrix, &context->kind128AltMatrix, dest);
        } else {
            sefMergeMatrixPos(&sefScratchMatrix, &context->kind128Matrix, dest);
        }
        matrixBlock = &sefScratchMatrix;
    } else if (kind & 0x400) {
        if (vectorMode != 0) {
            sefGetKindVector(dest, context->kind2Key, kind - 1025, 2);
            if (kind != 0) {
                vectorMode = 0;
            }
        } else {
            sefGetKindSlotVector(dest, context->kind2Key, kind - 1025, 2);
            sefMergeMatrixPos(&sefScratchMatrix, &context->kind1024Matrix, dest);
            matrixBlock = &sefScratchMatrix;
        }
    } else if (kind & 0x800) {
        if (vectorMode != 0) {
            sefGetKindVector(dest, context->kind3Key, kind - 2049, 3);
        } else {
            if (context->kind3Key == 0 && context->kind2Key != 0) {
                sefGetKindSlotVector(dest, context->kind2Key, kind - 2049, 2);
            } else {
                sefGetKindSlotVector(dest, context->kind3Key, kind - 2049, 3);
            }
            sefMergeMatrixPos(&sefScratchMatrix, &context->kind2048Matrix, dest);
            matrixBlock = &sefScratchMatrix;
        }
    } else if (kind & 0x1000) {
        if (vectorMode != 0) {
            sefGetKindVector(dest, context->kind4Key, kind - 4097, 4);
            if (kind != 0) {
                vectorMode = 0;
            }
        } else {
            sefGetKindSlotVector(dest, context->kind4Key, kind - 4097, 4);
            sefMergeMatrixPos(&sefScratchMatrix, &context->kind4096Matrix, dest);
            matrixBlock = &sefScratchMatrix;
        }
    } else if (kind & 0x200) {
        if (vectorMode != 0) {
            sefGetKindVector(dest, context->kind6Key, kind - 513, 6);
            if (kind != 0) {
                vectorMode = 0;
            }
        } else {
            sefGetKindSlotVector(dest, context->kind6Key, kind - 513, 6);
            sefMergeMatrixPos(&sefScratchMatrix, &context->kind512Matrix, dest);
            matrixBlock = &sefScratchMatrix;
        }
    } else if (kind == 16) {
        if (sefLinkedPoint != 0) {
            sefCopyQuadword(dest, &sefLinkedPoint->position);
        }
    } else if (kind == 17) {
        if (sefLinkedPoint != 0) {
            sefTransformPartPoint(sefLinkedPoint, dest, sefLinkedPartIndex, source);
            return 0;
        }
    } else if (kind >= 0x2000) {
        return 0;
    } else if (kind == 32) {
        sefCopyQuadword(dest, &sefEntryTable.origin);
    }
    if (matrixBlock != 0) {
        if (hasSource) {
            if (useScale) {
                Vu0SefMulVectorXYZ(dest, &sefCurrentRecord->scale, source);
                MMathApplyMatrix(dest, matrixBlock, dest);
            } else {
                MMathApplyMatrix(dest, matrixBlock, source);
            }
        } else {
            sefCopyQuadword(dest, (*matrixBlock)[3]);
        }
    } else if (hasSource) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n"
            "lqc2 vf2, 0(%2)\n"
            "vadd.xyz vf1, vf1, vf2\n"
            "sqc2 vf1, 0(%1)\n"
            : "+m"(*dest)
            : "r"(dest), "r"(source)
            : "memory");
    }
    return matrixBlock;
}

/* Copies the part matrix selected by kind: kind 64, 128, 162 and 256 copy a fixed matrix of the context, kind 162 also
 * clears dest[3][1]; the kind bits 0x40, 0x80, 0x100, 0x200, 0x400, 0x800 and 0x1000 pass a per-kind key and the
 * matching kind to sefGetKindMatrix. */
void sefGetPartMatrix(Matrix4 *dest, int kind)
{
    SefLinkedContext *context = sefGetBattleData();

    if (kind == 64) {
        sefCopyKindMatrix(dest, &context->kind64Matrix);
        return;
    }
    if (kind & 0x40) {
        sefGetKindMatrix(dest, context->kind1Key, kind - 65, 1);
        return;
    }
    if (kind == 128) {
        sefCopyKindMatrix(dest, &context->kind128Matrix);
        return;
    }
    if (kind == 162) {
        sefCopyKindMatrix(dest, &context->kind128Matrix);
        (*dest)[3][1] = 0.0f;
        return;
    }
    if (kind & 0x80) {
        sefGetKindMatrix(dest, context->kind0Key, kind - 129, 0);
        return;
    }
    if (kind == 256) {
        sefCopyKindMatrix(dest, &context->kind256Matrix);
        return;
    }
    if (kind & 0x100) {
        sefGetKindMatrix(dest, context->kind5Key, kind - 257, 5);
        return;
    }
    if (kind & 0x400) {
        sefGetKindMatrix(dest, context->kind2Key, kind - 1025, 2);
        return;
    }
    if (kind & 0x800) {
        if (context->kind3Key == 0 && context->kind2Key != 0) {
            sefGetKindMatrix(dest, context->kind2Key, kind - 2049, 2);
            return;
        }
        sefGetKindMatrix(dest, context->kind3Key, kind - 2049, 3);
        return;
    }
    if (kind & 0x1000) {
        sefGetKindMatrix(dest, context->kind4Key, kind - 4097, 4);
        return;
    }
    if (kind & 0x200) {
        sefGetKindMatrix(dest, context->kind6Key, kind - 513, 6);
        return;
    }
}

/* Kinds 97 and 98 are passed on as kind 64; a NULL source reads sefDefaultOffset. The w component is set to 1.0f. */
void sefGetPosition(Vector4 *dest, const Vector4 *source, int kind)
{
    if ((unsigned int)(kind - 97) < 2) {
        kind = 64;
    }
    if (source == 0) {
        source = &sefDefaultOffset;
    }
    sefGetPoint(dest, source, kind);
    dest->w = 1.0f;
}

f32 sefCircleAngleScale = 6.28318548f;

/* Random point on the circle of the given radius in the XZ plane, y zero: the seed steps once, the angle is the
 * sampled fraction of a turn. */
void sefGetCirclePos(Vector4 *pos, f32 radius)
{
    f32 angle;

    sefRandSeed = sefRandSeed * SEF_RAND_MULTIPLIER + SEF_RAND_INCREMENT;
    angle = (f32)(int)((sefRandSeed >> 16) & SEF_RAND_MASK) / SEF_NOISE_SCALE * sefCircleAngleScale;
    Vu0SetOrigin(pos);
    pos->x = xglCos(angle) * radius;
    pos->z = xglSin(angle) * radius;
}

/* Direction from three consecutive noise samples (the fourth float is overwritten by w = 1), normalized and scaled by
 * radius. */
void sefGetSpherePos(DoublewordVector4 *pos, f32 radius)
{
    unsigned int index = sefNoiseTable.nextIndex;

    const Vector4 *source = (const Vector4 *)&sefNoiseTable.samples[index];

    pos->vector = *source;
    pos->vector.w = 1.0f;
    sefNoiseTable.nextIndex = (index + 3u) % SEF_NOISE_TABLE_LEN;
    Vu0SefScaleVectorXYZ(&pos->vector, &pos->vector, SEF_NOISE_SCALE);
    MMathNormalizeVector2(&pos->vector, &pos->vector);
    Vu0SefScaleVectorXYZ(&pos->vector, &pos->vector, radius);
}

/* Unit direction from three consecutive noise samples (the fourth float is overwritten by w = 1), scaled to length one
 * by the normalize call. */
void sefGetUnitDirection(DoublewordVector4 *vector)
{
    unsigned int index = sefNoiseTable.nextIndex;
    const Vector4 *source = (const Vector4 *)&sefNoiseTable.samples[index];

    vector->vector = *source;
    vector->vector.w = 1.0f;
    sefNoiseTable.nextIndex = (index + 3u) % SEF_NOISE_TABLE_LEN;
    Vu0SefScaleVectorXYZ(&vector->vector, &vector->vector, SEF_NOISE_SCALE);
    MMathNormalizeVector2(&vector->vector, &vector->vector);
}

/* Random point of the cube [-0.5, 0.5) x [0, 1) x [-0.5, 0.5) scaled by extent: the bottom-aligned cube. */
void sefGetCubePosBtm(Vector4 *pos, const Vector4 *extent) {
    f32 shiftedY;

    sefNoiseNextVector(pos);
    shiftedY = pos->y + 0.5f;
    pos->y = shiftedY;
    Vu0SefMulVectorXYZ(pos, pos, extent);
}

/* The top-aligned cube: sefGetCubePosBtm with y negated. */
void sefGetCubePosTop(Vector4 *pos, const Vector4 *extent)
{
    sefGetCubePosBtm(pos, extent);
    pos->y = -pos->y;
}

/* Random point of the centred cube [-0.5, 0.5)^3 scaled by the extent vector. */
void sefGetCubePos(Vector4 *destination, Vector4 *scale)
{
    sefNoiseNextVector(destination);
    Vu0SefMulVectorXYZ(destination, destination, scale);
}

/* Transforms source into destination by the first matrix whose bit is set in value and whose context key is non-zero;
 * value 1 and value 17 use fixed matrices, otherwise the source is copied. */
void sefApplyPartTransform(Vector4 *destination, Vector4 *source, int partKind)
{
    SefLinkedContext *context = sefGetBattleData();

    if (partKind & 0x80) {
        if (context->kind0Key != 0) {
            if (sefPointMotion.sameDirection != 0) {
                sefApplyMatrixVector(destination, &context->kind128AltMatrix, source);
                return;
            }
            sefApplyMatrixVector(destination, &context->kind128Matrix, source);
            return;
        }
    }
    if (partKind & 0x40) {
        if (context->kind1Key != 0) {
            sefApplyMatrixVector(destination, &context->kind64Matrix, source);
            return;
        }
    }
    if (partKind & 0x100) {
        if (context->kind5Key != 0) {
            sefApplyMatrixVector(destination, &context->kind256Matrix, source);
            return;
        }
    }
    if (partKind & 0x400) {
        if (context->kind2Key != 0) {
            sefApplyMatrixVector(destination, &context->kind1024Matrix, source);
            return;
        }
    }
    if (partKind & 0x800) {
        if (context->kind3Key != 0) {
            sefApplyMatrixVector(destination, &context->kind2048Matrix, source);
            return;
        }
    }
    if (partKind & 0x1000) {
        if (context->kind4Key != 0) {
            sefApplyMatrixVector(destination, &context->kind4096Matrix, source);
            return;
        }
    }
    if (partKind & 0x200) {
        if (context->kind6Key != 0) {
            sefApplyMatrixVector(destination, &context->kind512Matrix, source);
            return;
        }
    }
    if (partKind == 1) {
        sefApplyMatrixVector(destination, &sefCameraState.current, source);
        return;
    }
    if (partKind == 17) {
        if (sefLinkedPoint != 0) {
            sefApplyMatrixVector(destination, &sefLinkedPoint->matrix, source);
            return;
        }
    }
    /* The two read operands of *source are the register-rank lever (style debt): one read, two operand refs. */
    __asm__ __volatile__(
        "lq $8, 0(%2)\n"
        "sq $8, 0(%1)\n"
        : "+m"(*destination)
        : "r"(destination), "r"(source), "m"(*source), "m"(*source)
        : "$8", "memory");
}

f32 sefOffsetRotateXDegToRad = 0.0174532924f;
f32 sefOffsetRotateZDegToRad = 0.0174532924f;

/* A point of the emission shape `kind` with the offsets as its size: sphere (random or fixed radius), the three
 * cubes, a circle turned by the y/z angles, or a random circle with a random height. */
void sefGetOfsRange(Vector4 *pos, Vector4 *offsets, int kind)
{
    Vector4 circlePoint;

    __asm__ __volatile__("sqc2 vf0, 0(%1)" : "+m"(*pos) : "r"(pos) : "memory");
    if (kind == SEF_SHAPE_SPHERE_RANDOM) {
        if (offsets->x != 0.0f) {
            sefGetSpherePos((DoublewordVector4 *) pos, sefRandf() * offsets->x);
        }
    } else if (kind == SEF_SHAPE_SPHERE) {
        if (offsets->x != 0.0f) {
            sefGetSpherePos((DoublewordVector4 *) pos, offsets->x);
        }
    } else if (kind == SEF_SHAPE_CUBE_CENTER) {
        sefGetCubePos(pos, offsets);
    } else if (kind == SEF_SHAPE_CUBE_TOP) {
        sefGetCubePosTop(pos, offsets);
    } else if (kind == SEF_SHAPE_CUBE_BOTTOM) {
        sefGetCubePosBtm(pos, offsets);
    } else if (kind == SEF_SHAPE_CIRCLE_TURNED) {
        sefGetCirclePos(&circlePoint, offsets->x);
        __asm__ __volatile__("lqc2 $vf26, 0(%0)\n\t"
                             "lqc2 $vf27, 16(%0)\n\t"
                             "lqc2 $vf28, 32(%0)\n\t"
                             "vmove.xyzw $vf29, $vf0"
                             :
                             : "r"(vu0IdentityMatrix)
                             : "memory");
        if (offsets->y != 0.0f) {
            vu0RotateCurrentX(offsets->y * sefOffsetRotateXDegToRad);
        }
        if (offsets->z != 0.0f) {
            vu0RotateCurrentZ(offsets->z * sefOffsetRotateZDegToRad);
        }
        __asm__ __volatile__("lqc2 $vf1, 0(%1)\n\t"
                             "vmulax.xyzw ACC, $vf26, $vf1x\n\t"
                             "vmadday.xyzw ACC, $vf27, $vf1y\n\t"
                             "vmaddaz.xyzw ACC, $vf28, $vf1z\n\t"
                             "vmaddw.xyzw $vf20, $vf29, $vf1w\n\t"
                             "sqc2 $vf20, 0(%2)"
                             : "+m"(*pos)
                             : "r"(&circlePoint), "r"(pos)
                             : "memory");
    } else if (kind == 6) {
        sefGetCirclePos(pos, sefRandf() * offsets->x);
        pos->y = sefRandf() * offsets->y;
    }
}

/* One 0x20-byte lane vector: the Vector4 and its unmodeled tail. */
typedef struct SefLaneVector {
    Vector4 vector;
    unsigned char unmodeled_010[0x10];
} SefLaneVector;

/* Shape and part pointers of a lane; each points one record past the short it reads (negative index). */
typedef struct SefShapeSlot {
    short index;
    unsigned char unmodeled_02[0xE];
} SefShapeSlot;

typedef struct SefPartSlot {
    short value;
    unsigned char unmodeled_02[0x2];
} SefPartSlot;

typedef struct SefLaneLinks {
    SefPartSlot *part;
    SefShapeSlot *shape;
} SefLaneLinks;

typedef struct SefLaneObject {
    unsigned char unmodeled_000[0x40];
    SefLaneVector offsets[2];
    unsigned char unmodeled_080[0x140 - 0x80];
    SefLaneVector anchors[2];
    SefLaneLinks links[2];
} SefLaneObject;

void sefGetStartPos(Vector4 *pos, SefLaneObject *object)
{
    short index;

    index = object->links[0].shape[-1].index;
    sefGetOfsRange(pos, &object->anchors[0].vector, index);
    if (sefVectorMode == 0 && index >= SEF_SHAPE_CUBE_CENTER && index < SEF_SHAPE_CIRCLE_RANDOM) {
        sefApplyPartTransform(pos, pos, object->links[0].part[-1].value);
    }
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n"
        "lqc2 vf2, 0(%3)\n"
        "vadd.xyz vf1, vf1, vf2\n"
        "sqc2 vf1, 0(%1)\n"
        : "=m"(*pos)
        : "r"(pos), "r"(&object->offsets[0].vector), "r"(pos)
        : "memory");
}

void sefGetTargetPos(Vector4 *pos, SefLaneObject *object)
{
    short index;

    index = object->links[1].shape[-1].index;
    sefGetOfsRange(pos, &object->anchors[1].vector, index);
    if (sefVectorMode == 0 && index >= SEF_SHAPE_CUBE_CENTER && index < SEF_SHAPE_CIRCLE_RANDOM) {
        sefApplyPartTransform(pos, pos, object->links[1].part[-1].value);
    }
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n"
        "lqc2 vf2, 0(%3)\n"
        "vadd.xyz vf1, vf1, vf2\n"
        "sqc2 vf1, 0(%1)\n"
        : "=m"(*pos)
        : "r"(pos), "r"(&object->offsets[1].vector), "r"(pos)
        : "memory");
}

/* Stores the origin vector at sefEntryTable.origin, then clears the two reset fields of every entry. */
void sefResetEntryFlags(void)
{
    SefEntry *entry;
    int index;

    Vu0SetOrigin(&sefEntryTable.origin);
    for (index = 0; index < sefEntryTable.entryCount; index++) {
        entry = &sefEntryTable.entries[index];
        entry->flags = 0;
        entry->resetHalf = 0;
    }
}

/* A part's placement: the 4x4 matrix at +0x000 (its three columns are the transform's axes), the translation at +0x140,
 * the scale at +0x160 and the point vu0TransformPoint takes at +0x290. */
typedef struct SefPartPlacement {
    Matrix4 matrix;                                  /* +0x000 */
    unsigned char unmodeled_040[0x140 - 0x40];
    Vector4 translation;                             /* +0x140 */
    unsigned char unmodeled_150[0x160 - 0x150];
    Vector4 scale;                                   /* +0x160 */
    unsigned char unmodeled_170[0x290 - 0x170];
    Vector4 point;                                   /* +0x290 */
} SefPartPlacement;

/* Places the part: the translation merged into the matrix (less the global position, and with the global matrix applied,
 * when the global flag is set). A negative mode copies the merged translation row to out, otherwise out receives the
 * merged matrix applied to the point. Returns the global scale on the flagged path, else 1.0f. */
float sefPlacePart(SefPartPlacement *placement, Vector4 *out, int mode)
{
    Matrix4 combined;
    float scale = 1.0f;
    AlignedVector4 localTranslation;

    if (sefGlobalTransform.scaled) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n"
            "lqc2 vf2, 0(%2)\n"
            "vsub.xyz vf1, vf1, vf2\n"
            "sqc2 vf1, 0(%3)\n"
            : "=m"(localTranslation)
            : "r"(&placement->translation), "r"(&sefGlobalTransform.position), "r"(&localTranslation),
              "m"(placement->translation), "m"(sefGlobalTransform.position));
        sefMergeMatrixPos(&combined, &placement->matrix, &localTranslation);
        MMathMulMatrix(&combined, &sefGlobalTransform.matrix, &combined);
        scale = sefGlobalTransform.scale;
    } else {
        sefMergeMatrixPos(&combined, &placement->matrix, &placement->translation);
    }
    if (mode >= 0) {
        Vector4 *point = &placement->point;

        MMathApplyMatrix(out, &combined, point);
        return scale;
    }
    sefCopyQuadword(out, combined[3]);
    return scale;
}

/* The quadword copies of sefRefreshEntryRows: lq/sq through $8 with no "memory" clobber, so the entry's slot count stays a
 * plain load across them. The row-reset copy also names its destination a second time as an input. */
#define QuadwordCopyT0Twice(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%1)\n" \
        "sq $8, 0(%0)\n" \
        : \
        : "r" (destination), "r" (source), "r" (destination) \
        : "$8")
#define QuadwordCopyT0Plain(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%1)\n" \
        "sq $8, 0(%0)\n" \
        : \
        : "r" (destination), "r" (source) \
        : "$8")

/* The object an entry's id holds the address of: its flags word at +0xBC (bit 0x8 marks the entry's slots as in use) and
 * the 16-byte rows at +0x190 that sefRefreshEntryRows fills. */
typedef struct SefEntryObject {
    unsigned char unmodeled_000[0xBC];
    int flags;
    unsigned char unmodeled_0C0[0x190 - 0xC0];
    unsigned char rows[16][16];
} SefEntryObject;


/*
 * Refreshes the 16-byte rows of every live entry's object (entries of mode 2 are skipped) from the block sefLightSetActiveGet
 * returns. With no slots, a set slot flag is cleared, bit 0x8 of the object flags is dropped while sefGateOwnerZeroFlag is clear
 * and rows 3..5 and 10..12 are reloaded. With slots, the flag is set, the same six rows are reloaded and rows 3..5
 * and 10..12 of the remaining slots receive the slot offsets and weighted offsets, active slots first.
 */
/* SefEntryObject.flags bit: the entry's slots are in use (set while the rows hold the slot copies). */
#define SEF_ENTRY_SLOTS_IN_USE 0x8

void sefRefreshEntryRows(void)
{
    SefEntry *entry;
    unsigned char (*rows)[16];
    unsigned char (*row)[16];
    unsigned char (*source)[16];
    short resetCount;
    int i;
    int k;
    int slot;

    if (_sefBattleMode != 0) {
        for (i = 0; i < sefEntryTable.entryCount; i++) {
            entry = &sefEntryTable.entries[i];
            if (entry->id != 0 && entry->mode != 2) {
                rows = ((SefEntryObject *) entry->id)->rows;
                source = sefLightSetActiveGet();
                resetCount = entry->resetHalf;
                if (resetCount == 0) {
                    if (entry->slotFlag != 0) {
                        entry->slotFlag = 0;
                        if (sefGateOwnerZeroFlag == 0) {
                            ((SefEntryObject *) entry->id)->flags &= ~SEF_ENTRY_SLOTS_IN_USE;
                        }
                        row = rows;
                        for (k = 0; k < 3; k++) {
                            /* the int offset form stays: source + k changes the code */
                            unsigned char (*sourceRow)[16] = (unsigned char (*)[16]) (k * 16 + (int) source);
                            QuadwordCopyT0Twice(row[10], sourceRow[10]);
                            QuadwordCopyT0Plain(row[3], sourceRow[3]);
                            row++;
                        }
                    }
                } else {
                    ((SefEntryObject *) entry->id)->flags |= SEF_ENTRY_SLOTS_IN_USE;
                    entry->slotFlag = 1;
                    slot = 3;
                    k = 0;
                    row = rows;
                    while (k < 3) {
                        unsigned char (*sourceRow)[16] = source + k;
                        resetCount = k;
                        QuadwordCopyT0Plain(row[10], sourceRow[10]);
                        QuadwordCopyT0Plain(row[3], sourceRow[3]);
                        row++;
                        k++;
                    }
                    for (k = 0; k < (short) (unsigned short) entry->resetHalf && slot < 6; k++) {
                        if (entry->slotCounts[k] != 0) {
                            QuadwordCopyT0Plain(rows[slot], &entry->slotOffsets[k]);
                            QuadwordCopyT0Plain(rows[slot + 7], &entry->weightedOffsets[k]);
                            slot++;
                        }
                    }
                    if (slot < 6) {
                        for (k = 0; k < (short) (unsigned short) entry->resetHalf && slot < 6; k++) {
                            if (entry->slotCounts[k] == 0) {
                                QuadwordCopyT0Plain(rows[slot], &entry->slotOffsets[k]);
                                QuadwordCopyT0Plain(rows[slot + 7], &entry->weightedOffsets[k]);
                                slot++;
                            }
                        }
                    }
                }
            }
        }
    }
}

/* Offset of the rows block inside a SefEntryObject (the entry id is the object's address). */
#define SEF_ENTRY_ROWS_OFFSET 0x190

void sefAccumulateEntryInfluence(Vector4 *position, int *laneOffsets, float radius)
{
    SefLinkedContext *context;
    SefEntry *entry;
    Vector4 slotVector;
    Vector4 difference;
    float radiusSquared;
    float dot;
    float weight;
    Vector4 *row;
    short resetIndex;
    int rowsBase;
    int index;
    int k;

    radiusSquared = radius * radius;
    context = sefGetBattleData();
    for (index = 0; index < sefEntryTable.entryCount; index++) {
        entry = &sefEntryTable.entries[index];
        if (entry->id == 0 || entry->resetHalf >= 16 || entry->mode == 2) {
            continue;
        }
        if (sefCopyRecordSlotVector(&slotVector, entry->id, 3) == 0) {
            continue;
        }
        /* The second "m"(*position) operand is not read by the asm text: it keeps the
           position parameter in the register its global allocation key earns (style debt). */
        __asm__ __volatile__(
            "lqc2 vf1, %0\n"
            "lqc2 vf2, 0(%1)\n"
            "vsub.xyz vf1, vf1, vf2\n"
            "sqc2 vf1, 0(%2)\n"
            : : "m"(*position), "r"(&slotVector), "r"(&difference), "m"(*position) : "memory");
        dot = MMathVectorDotProduct(&difference, &difference);
        if (dot < radiusSquared) {
            rowsBase = entry->id + SEF_ENTRY_ROWS_OFFSET;
            weight = ((radiusSquared - dot) / radiusSquared) * 0.0078125f; /* 1/128 */
            if (entry->resetHalf == 0) {
                row = (Vector4 *) rowsBase;
                for (k = 2; k >= 0; k--) {
                    __asm__ __volatile__("sqc2 vf0,0(%0)\n" : : "r"(&row[10]) : "memory");
                    __asm__ __volatile__("sqc2 vf0,0(%0)\n" : : "r"(&row[3]) : "memory");
                    row++;
                }
                memset(entry->slotCounts, 0, 16);
            }
            if (context->kind1Key == entry->id || context->kind0Key == entry->id
                || context->kind5Key == entry->id) {
                entry->slotCounts[entry->resetHalf]++;
            }
            resetIndex = entry->resetHalf;
            sefCopyQuadword(&entry->slotOffsets[resetIndex], &difference);
            entry->weightedOffsets[resetIndex].vector.x = (float) laneOffsets[0] * weight;
            entry->weightedOffsets[entry->resetHalf].vector.y = (float) laneOffsets[1] * weight;
            entry->weightedOffsets[entry->resetHalf].vector.z = (float) laneOffsets[2] * weight;
            entry->weightedOffsets[entry->resetHalf].vector.w = 0.0f;
            entry->resetHalf++;
        }
    }
}

/* Counts every group timer down by one frame, stopping at zero. */
void sefTickGroupTimers(void)
{
    SefGroupEntry *entry;
    int index;

    entry = sefGroupTable;
    for (index = SEF_GROUP_ENTRY_COUNT - 1; index >= 0; index--) {
        if (entry->remaining > 0) {
            entry->remaining = entry->remaining - 1;
        }
        entry++;
    }
}

/*
 * Writes the origin vector (0, 0, 0, 1) to center, then copies the centre of the first table entry whose owner and key
 * match. Leaves center as the origin when no entry matches.
 */
void sefGetGroupCenter(float *center, int owner, unsigned short key)
{
    SefGroupEntry *entry;
    int index;

    Vu0SetOrigin(center);
    entry = sefGroupTable;
    for (index = 0; index < SEF_GROUP_ENTRY_COUNT; index++) {
        if (entry->owner == owner && entry->key == key) {
            QuadwordCopyT0(center, entry->center);
            return;
        }
        entry++;
    }
}

/*
 * Copies a group centre into its timer entry. Both registration paths share one copy block in the original, which
 * gcc only produces when the two copies come from the same asm statement.
 */
static inline void sefGroupStoreCenter(SefGroupEntry *entry, Vector4 *centre)
{
    QuadwordCopyT0(entry->center, centre);
}

/* The object a live effect record belongs to; only its position (+0x10, an xyz vector) is read here. */
typedef struct SefGroupObject {
    unsigned char unmodeled_00[0x10];
    Vector4 position;                              /* +0x10 */
} SefGroupObject;


#define SEF_GROUP_MEMBER_MAX 24
#define SEF_GROUP_TIMER_FRAMES 600

/*
 * Gathers the live effect records owned by owner under key. Each distinct object is counted once and its position is
 * summed into the group centre; the centre is averaged when two or more objects contributed. The centre is written to
 * the table entry for (owner, key) if one exists, otherwise to the first free entry, which gets a 600-frame timer.
 */
void sefRegisterGroupCenter(int owner, unsigned short key)
{
    int index;
    int seenCount;
    int seenIndex;
    SefEffectRecord *records;
    SefEffectRecord *record;
    SefGroupObject *object;
    SefGroupObject *seen[SEF_GROUP_MEMBER_MAX];
    AlignedVector4 sum;
    Vector4 *center;
    SefGroupEntry *entry;

    records = sefGetScheduler();
    center = &sum;
    seenCount = 0;
    Vu0SetOrigin(center);
    for (index = 0; index < sefEffectRecordCount; index++) {
        record = &records[index];
        if (record->inUse == 0 || record->ownerKey != owner) {
            continue;
        }
        object = record->object;
        if (object == 0 || record->value != key) {
            continue;
        }
        if (record->flags & SEF_RECORD_EXCLUDE_FLAG) {
            continue;
        }
        for (seenIndex = 0; seenIndex < seenCount; seenIndex++) {
            if (seen[seenIndex] == record->object) {
                break;
            }
        }
        if (seenIndex != seenCount) {
            continue;
        }
        /* center.xyz += object position; no memory clobber, so the record count load stays hoisted */
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n"
            "lqc2 vf2, 0(%2)\n"
            "vadd.xyz vf1, vf1, vf2\n"
            "sqc2 vf1, 0(%0)\n"
            :
            : "r" (center), "r" (center), "r" (&object->position));
        seen[seenCount] = object;
        seenCount++;
        if (seenCount >= SEF_GROUP_MEMBER_MAX) {
            break;
        }
    }
    if (seenCount >= 2) {
        MMathDivVectorS(center, center, (float)seenCount);
    }

    for (index = 0; index < SEF_GROUP_ENTRY_COUNT; index++) {
        entry = &sefGroupTable[index];
        if (entry->owner == owner && entry->key == key) {
            entry->remaining = SEF_GROUP_TIMER_FRAMES;
            entry->key = key;
            sefGroupStoreCenter(entry, center);
            return;
        }
    }

    for (index = 0; index < SEF_GROUP_ENTRY_COUNT; index++) {
        entry = &sefGroupTable[index];
        if (entry->remaining == 0) {
            entry->remaining = SEF_GROUP_TIMER_FRAMES;
            entry->owner = owner;
            entry->key = key;
            sefGroupStoreCenter(entry, center);
            return;
        }
    }
}


/* One of the SEF_SPAWN_SLOT_COUNT effect slots of sefEffectSlots (SEF_SPAWN_SLOT_SIZE bytes): sefUpdateEffectSlots respawns its effect count times
 * per update around position until age reaches lifetime, then frees the slot. */
typedef struct SefSpawnSlot {
    Vector4 position;                /* +0x00 */
    Vector4 direction;               /* +0x10: the scale block of sefCreateEffectCf2 */
    Vector4 scale;                   /* +0x20: scales the noise offset */
    int effectId;                    /* +0x30: 0 marks a free slot */
    float angle;                     /* +0x34: Y rotation of the offsets */
    short lifetime;                  /* +0x38 */
    short age;                       /* +0x3a */
    short count;                     /* +0x3c */
    short largeVolume;               /* +0x3e: set when the scale volume is 9 or more */
} SefSpawnSlot;

static SefSpawnSlot sefEffectSlots[SEF_SPAWN_SLOT_COUNT];

/* Clears the whole effect slot table sefEffectSlots: 32 records of 0x40 bytes. */
void sefResetEffectSlots(void) {
    sefMemZero(sefEffectSlots, SEF_SPAWN_SLOT_COUNT * SEF_SPAWN_SLOT_SIZE);
}

extern const char sefSclLogLabel[];
extern const char sefSizeLogLabel[];
const char sefSpawnCountLogFormat[32] = "max=%d 2=%d t=%d";

/* Spawns one effect into the first free slot of sefEffectSlots, after the canonical-id and count checks. */
void sefSpawnEffect(int effectId, Vector4 *position, Vector4 *direction, Vector4 *scale, float angle, int count)
{
    SefSpawnSlot *slot = sefEffectSlots;
    int slotIndex;
    int canonicalId;
    int canonicalDiffers;
    int inRange;
    int lookupOut;
    int volume;
    int spawnCount;
    int smallScale;
    /* Block-scope prototype (a lever: the same declaration is in sef.h, but without this one the code changes): the
     * calls below are compiled as calls to a not yet defined function. */
    extern int sefStartEffectKeyDefault(void *buffer, int effectKey);

    if (effectId <= 0) {
        return;
    }
    if (effectId == SEF_SPAWN_SPECIAL_ID) {
        if (scGetKeyRecordFirst(effectId, (int)&lookupOut) == 0) {
            sefStartEffectKeyDefault(0, SEF_SPAWN_SPECIAL_ID);
        }
        sefCreateEffectCf2(SEF_SPAWN_SPECIAL_ID, position, 0, direction, -1);
        return;
    }
    canonicalId = sefCanonicalEffectId(effectId);
    canonicalDiffers = canonicalId != SEF_SPAWN_ID_BASE;
    inRange = (unsigned int)(effectId - SEF_SPAWN_ID_BASE) < 100u;
    if ((unsigned int)(effectId - 0x100C838A) < 3u) {
        if (count >= 17) {
            count >>= 1;
        }
    }
    if (direction != 0) {
        LOG(direction, sefSclLogLabel);
    }
    if (scale != 0) {
        LOG(scale, sefSizeLogLabel);
    }

    for (slotIndex = 0; slotIndex < SEF_SPAWN_SLOT_COUNT; ++slotIndex, ++slot) {
        if (slot->effectId != 0) {
            continue;
        }
        volume = 0;
        if (position != 0) {
            QuadwordCopyT0(&slot->position, position);
        } else {
            Vu0SetOrigin(&slot->position);
        }
        if (direction != 0) {
            QuadwordCopyT0(&slot->direction, direction);
        } else {
            __asm__ __volatile__(
                "vmaxw.xyzw vf1, vf0, vf0w\n"
                "sqc2 vf1, 0(%0)\n"
                :
                : "r" (&slot->direction)
                : "memory");
        }
        slot->direction.w = 1.0f;
        slot->largeVolume = 0;

        if (scale != 0) {
            /* the "m" operand on the source scale is the original's extra use of it (callee-saved rank) */
            __asm__ __volatile__(
                "lq $8, 0(%1)\n"
                "sq $8, 0(%0)\n"
                :
                : "r" (&slot->scale), "r" (scale), "m" (*scale)
                : "$8", "memory");
            volume = (int)(slot->scale.x + 0.9f) * (int)(slot->scale.y + 0.9f) * (int)(slot->scale.z + 0.9f);
            spawnCount = volume;
            if (count > 0) {
                if (count < volume) {
                    if (volume < SEF_SPAWN_LARGE_VOLUME) {
                        spawnCount = volume / count + 2;
                    } else {
                        spawnCount = volume / count;
                    }
                } else {
                    count = 0;
                    if (volume < SEF_SPAWN_LARGE_VOLUME) {
                        spawnCount = volume + 2;
                    }
                }
            }
            if (spawnCount <= 0) {
                spawnCount = 3;
            }
            slot->count = spawnCount;
            smallScale = volume < SEF_SPAWN_LARGE_VOLUME;
        } else {
            __asm__ __volatile__(
                "vmaxw.xyzw vf1, vf0, vf0w\n"
                "sqc2 vf1, 0(%0)\n"
                :
                : "r" (&slot->scale)
                : "memory");
            slot->count = 3;
            smallScale = 1;
        }

        if (!smallScale) {
            float factor;
            int shift;

            factor = 4.0f;
            if (volume < 64) {
                factor = 2.0f;
            }
            shift = volume >= 64 ? 4 : 2;
            if (volume >= 80) {
                shift = 4;
            }

            Vu0SefScaleVectorXYZ(&slot->direction, &slot->direction, factor);
            slot->largeVolume = 1;
            slot->count = slot->count >> shift;
            if (slot->count == 0) {
                slot->count = 1;
            }
            if (count >= 10) {
                count >>= 1;
            }
        } else {
            Vu0SefScaleVectorXYZ(&slot->direction, &slot->direction, 1.2f);
        }

        LOG(&slot->direction, sefSpawnCountLogFormat, slot->count, volume, slot->count * count);

        slot->age = 0;
        slot->lifetime = count;
        slot->angle = angle;
        slot->effectId = effectId;
        slot->scale.w = 1.0f;
        if (canonicalDiffers) {
            short half = (short)slot->count >> 1;

            slot->count = half;
            if (half == 0) {
                slot->count = 1;
            }
        }
        if (inRange) {
            return;
        }
        slot->count = 1;
        if (direction != 0) {
            QuadwordCopyT0(&slot->direction, direction);
        } else {
            __asm__ __volatile__(
                "vmaxw.xyzw vf1, vf0, vf0w\n"
                "sqc2 vf1, 0(%0)\n"
                :
                : "r" (&slot->direction)
                : "memory");
        }
        slot->lifetime = 1;
        slot->largeVolume = 0;
        Vu0SetOrigin(&slot->scale);
        return;
    }
}


/* For each live slot: spawns count records at noise offsets (y lifted by 0.5, scaled, rotated by angle around Y)
 * around position, then ages the slot and frees it once age reaches lifetime. */
void sefUpdateEffectSlots(void)
{
    Vector4 offset;
    Matrix4 rotation;
    SefSpawnSlot *slot;
    int slotIndex;
    int spawnIndex;

    slot = sefEffectSlots;
    for (slotIndex = 0; slotIndex < SEF_SPAWN_SLOT_COUNT; slotIndex++, slot++) {
        if (slot->effectId != 0) {
            MMathRotateMatrixY(&rotation, 0, slot->angle);
            for (spawnIndex = 0; spawnIndex < slot->count; spawnIndex++) {
                sefNoiseNextVector(&offset);
                offset.y += 0.5f;
                /* offset.xyz *= scale.xyz; the "+m" operand on the scale gives that pointer its callee-saved rank */
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%1)\n"
                    "lqc2 vf2, 0(%2)\n"
                    "vmul.xyz vf1, vf1, vf2\n"
                    "sqc2 vf1, 0(%1)\n"
                    : "+m"(slot->scale)
                    : "r"(&offset), "r"(&slot->scale)
                    : "memory");
                MMathApplyMatrix(&offset, &rotation, &offset);
                Vu0SefAddVectorXYZ(&offset, &offset, &slot->position);
                sefCreateEffectCf2(slot->effectId, &offset, 0, &slot->direction, -1);
            }
            slot->age++;
            if (slot->age >= slot->lifetime) {
                slot->effectId = 0;
            }
        }
    }
}

/* Forwards to sefSpawnEffect with the caller's arguments unchanged (tail call in the binary). */
void sefSpawnEffectById(int effectId, Vector4 *position, Vector4 *outVector, Vector4 *scale, float radius, int count) {
    sefSpawnEffect(effectId, position, outVector, scale, radius, count);
}

void sefSpawnEffectByName(signed char *name, Vector4 *position, Vector4 *outVector, Vector4 *scale, float radius, int count) {
    sefSpawnEffectById(srsEffectNameToID(name), position, outVector, scale, radius, count);
}

/* Load wait table loadWaitTable: sixteen ids, sixteen try counters, sixteen slot values and the ring head. */
#define SEF_LOAD_WAIT_RING 16
#define SEF_LOAD_WAIT_MAX_TRIES 300
#define SEF_LOAD_STATUS_BUSY -2
typedef struct LoadWaitTable {
    int id[SEF_LOAD_WAIT_RING];
    unsigned short tries[SEF_LOAD_WAIT_RING];
    short slot[SEF_LOAD_WAIT_RING];
    short head;
} LoadWaitTable;

/* The 144-byte original object at loadWaitTable: the 132-byte LoadWaitTable plus 12 bytes the original reserves after it. */
typedef struct LoadWaitTableBlock {
    LoadWaitTable table;
    unsigned char unmodeled_84[0x90 - 0x84];
} LoadWaitTableBlock;

static LoadWaitTableBlock loadWaitTable;

/* Adds id with its slot value to loadWaitTable; an id of 0 clears the whole table. */
void sefSetLoadQue(int id, int slot)
{
    LoadWaitTable *table;
    int i;
    int ringEnd;

    table = &loadWaitTable.table;
    if (id == 0) {
        memset(table, 0, sizeof(*table));
    } else {
        ringEnd = (table->head - 1) & (SEF_LOAD_WAIT_RING - 1);
        for (i = table->head; i != ringEnd; i = (i + 1) & (SEF_LOAD_WAIT_RING - 1)) {
            if (table->id[i] == id) {
                return;
            }
        }
        for (i = table->head; i != ringEnd; i = (i + 1) & (SEF_LOAD_WAIT_RING - 1)) {
            if (table->id[i] == 0) {
                table->id[i] = id;
                table->slot[i] = slot;
                table->tries[i] = 0;
                return;
            }
        }
    }
}

/* Retries each id of the ring from head with srsLoadEffectSlot; an entry still busy (-2) keeps the poll waiting until 300 tries. */
void sefExecLoadQue(void)
{
    LoadWaitTable *table;
    int i;
    int ringEnd;
    int loadResult;

    table = &loadWaitTable.table;
    ringEnd = (table->head - 1) & (SEF_LOAD_WAIT_RING - 1);
    for (i = table->head; i != ringEnd; i = (i + 1) & (SEF_LOAD_WAIT_RING - 1)) {
        if (table->id[i] != 0) {
            loadResult = srsLoadEffectSlot(0, table->id[i], table->slot[i]);
            if (loadResult == SEF_LOAD_STATUS_BUSY) {
                table->tries[i]++;
                if ((short) table->tries[i] < SEF_LOAD_WAIT_MAX_TRIES) {
                    return;
                }
            }
            table->id[i] = 0;
            table->tries[i] = 0;
            table->slot[i] = 0;
            table->head = (table->head + 1) & (SEF_LOAD_WAIT_RING - 1);
        }
    }
}

/* Returns 1 when any id is set in loadWaitTable, else 0. */
int sefLoadQueBusy(void)
{
    int i;

    for (i = 0; i < SEF_LOAD_WAIT_RING; i++) {
        if (loadWaitTable.table.id[i] != 0) {
            return 1;
        }
    }
    return 0;
}

/* Returns 1 while the load wait table (sefLoadQueBusy) holds an id or the srs read counter is positive. */
int sefCheckLoad(void)
{
    if (sefLoadQueBusy() != 0) {
        return 1;
    }
    return srsLeaveCdRead() > 0;
}

/* Returns once the CD status test xglCdServiceIfPending reports idle. */
void xglCdWaitUntilIdle(void)
{
    while (xglCdServiceIfPending() != 0) {
    }
}

/* A word set when either override vector was supplied; sefGetFogOverrideBlock hands out its address. */
static unsigned char sefOverrideFlag[16];
static AlignedVector4 sefOverrideVectorA;
static AlignedVector4 sefOverrideVectorB;

/*
 * Copies the two override vectors; a null source resets that vector to the
 * origin (0, 0, 0, 1). The flag records whether any override was given.
 */
void sefSetFogOverride(Vector4 *vectorA, Vector4 *vectorB)
{
    if (vectorA != 0) {
        QuadwordCopyT0(&sefOverrideVectorA, vectorA);
    } else {
        Vu0SetOrigin(&sefOverrideVectorA);
    }
    if (vectorB != 0) {
        QuadwordCopyT0(&sefOverrideVectorB, vectorB);
    } else {
        Vu0SetOrigin(&sefOverrideVectorB);
    }
    *(int *) sefOverrideFlag = vectorA != 0 || vectorB != 0;
}

SefFogOverride *sefGetFogOverrideBlock(void) {
    return (SefFogOverride *) sefOverrideFlag;
}

/* sefFlagWord is one flag word; sefExecEffect tests bit 1 of it through sefFlagTest. */
extern int sefFlagWord;

void sefFlagSet(int mask) {
    sefFlagWord |= mask;
}

void sefFlagClear(int mask) {
    sefFlagWord &= ~mask;
}

int sefFlagTest(int mask) {
    return sefFlagWord & mask;
}

void sefFlagToggle(int mask) {
    sefFlagWord ^= mask;
}

/* Clears the skip-step flag of an effect record in use (a cell record through sefObjectClearFlags). */
void sefClearEnabledFlag(SefEffectRecord *record) {
    if (record != 0) {
        if (record->inUse != 0) {
            if (record->cellSlot != 0) {
                sefObjectClearFlags(sefCellOf(record), SEF_RECORD_SKIP_STEP);
            } else {
                record->flags &= ~SEF_RECORD_SKIP_STEP;
            }
        }
    }
}

void sefPauseControl(int selector) {
    if (selector < 0) {
        sefFlagToggle(1);
        return;
    }
    if (selector > 0) {
        sefFlagSet(1);
        return;
    }
    sefFlagClear(1);
}

/*
 * Returns 1 when a record among the first sefEffectRecordCount has its in_use word set
 * and its owner equal to owner, otherwise 0.
 */
int sefOwnerInUse(int effectKey) {
    int recordIndex;
    SefEffectRecord *records;

    records = sefGetScheduler();
    for (recordIndex = 0; recordIndex < sefEffectRecordCount; recordIndex += 1) {
        if (records[recordIndex].inUse != 0 && records[recordIndex].ownerKey == effectKey) {
            return 1;
        }
    }
    return 0;
}

int sefOwnerPairInUse(void *owner, int ownerKey) {
    SefEffectRecord *records;
    int recordIndex;

    records = sefGetScheduler();
    for (recordIndex = 0; recordIndex < sefEffectRecordCount; recordIndex += 1) {
        if (records[recordIndex].inUse != 0 && records[recordIndex].ownerKey == ownerKey && records[recordIndex].owner == (int) owner) {
            return 1;
        }
    }
    return 0;
}

int sefOwnerGroupInUse(int owner, unsigned short value) {
    SefEffectRecord *cursor;
    int recordIndex;

    cursor = sefGetScheduler();
    for (recordIndex = 0; recordIndex < sefEffectRecordCount; recordIndex += 1) {
        if (cursor->inUse != 0 && cursor->ownerKey == owner && cursor->value == value) {
            return 1;
        }
        cursor += 1;
    }
    return 0;
}

int sefCountOwnerGroupRecords(int effectKey, unsigned short value) {
    SefEffectRecord *cursor;
    int recordIndex;
    int count;

    count = 0;
    cursor = sefGetScheduler();
    for (recordIndex = 0; recordIndex < sefEffectRecordCount; recordIndex += 1) {
        if (cursor->inUse != 0 && cursor->ownerKey == effectKey && cursor->value == value) {
            if ((cursor->flags & SEF_RECORD_EXCLUDE_FLAG) == 0) {
                count += 1;
            }
        }
        cursor += 1;
    }
    return count;
}

void sefClearOwnerGroupFlags(int effectKey, unsigned short value, int mask) {
    SefEffectRecord *cursor;
    int slot;
    int scanLimit;
    int recordCount;
    int flags;
    int keepBit;

    cursor = sefGetScheduler();
    scanLimit = sefEffectRecordCount;
    slot = 0;
    if (scanLimit > 0) {
        keepBit = SEF_RECORD_EXCLUDE_FLAG;
        recordCount = scanLimit;
        do {
            if (cursor->inUse != 0 && cursor->ownerKey == effectKey && cursor->value == value) {
                flags = cursor->flags;
                if ((flags & keepBit) == 0) {
                    cursor->flags = flags & ~mask;
                    scanLimit = recordCount;
                }
            }
            slot += 1;
            cursor += 1;
        } while (slot < scanLimit);
    }
}

int sefIsFinishEffect(int effectKey) {
    SefEffectRecord *table;
    SefEffectRecord *cursor;
    int slot;
    int count;
    int flags;

    table = sefGetScheduler();
    count = 0;
    if (effectKey == 0) {
        return 1;
    }
    if (sefEffectRecordCount > 0) {
        cursor = table;
        for (slot = 0; slot < sefEffectRecordCount; slot += 1) {
            if (cursor->inUse != 0) {
                flags = cursor->flags;
                if ((flags & 0x40) == 0) {
                    if ((flags & 0x02000000) != 0) {
                        if (effectKey == 0 || cursor->ownerKey == effectKey) {
                            count += 1;
                        }
                    }
                }
            }
            cursor += 1;
        }
    }
    return count == 0;
}

int sefIsFinishEffect2(int effectKey) {
    SefEffectRecord *table;
    SefEffectRecord *cursor;
    int slot;
    int count;

    table = sefGetScheduler();
    count = 0;
    if (effectKey <= 0) {
        return 1;
    }
    if (sefEffectRecordCount > 0) {
        cursor = table;
        for (slot = 0; slot < sefEffectRecordCount; slot += 1) {
            if (cursor->inUse != 0 && cursor->ownerKey == effectKey) {
                count += 1;
            }
            cursor += 1;
        }
    }
    return count == 0;
}

/*
 * Calls sefFreeScheduler with the index of each record whose in_use word is set
 * and whose owner matches owner (every record when owner is negative), clears
 * sefEffectRecordHead when owner is negative, then calls sefReleaseByKey with owner.
 */
void sefKillEffect(int effectKey) {
    int slot;
    SefEffectRecord *table;
    SefEffectRecord *cursor;

    slot = 0;
    table = sefGetScheduler();
    if (effectKey < 0) {
        sefEffectRecordHead = 0;
    }
    if (sefEffectRecordCount > 0) {
        cursor = table;
        do {
            if (cursor->inUse != 0 && (effectKey < 0 || cursor->ownerKey == effectKey)) {
                sefFreeScheduler(slot);
            }
            slot += 1;
            cursor += 1;
        } while (slot < sefEffectRecordCount);
    }
    sefReleaseByKey(effectKey);
}

int sefCountMarkedEntries(SefEffectRecord *record) {
    int count;
    int i;

    count = 0;
    if (record != 0) {
        for (i = 0; i < 32; i += 1) {
            if (record->entries[i].state != 0) {
                if (record->entries[i].flags & 0x8000) {
                    count += 1;
                }
            }
        }
    }
    return count;
}

/* Applies mode to every record in use whose ownerKey is key and whose owner is owner: releases it, sets and clears
 * flag 0x80, sets and clears the gate, or sets flags 0x880. */
void sefApplyRecordMode(void *owner, int ownerKey, int mode)
{
    SefEffectRecord *table;
    SefEffectRecord *cursor;
    int slot;
    int count;
    /* Block-scope prototype: the call below is compiled as a call to a not yet defined function. */
    extern void sefFreeScheduler(int index);

    table = sefGetScheduler();
    slot = 0;
    if (sefEffectRecordCount > 0) {
        cursor = table;
        count = sefEffectRecordCount;
        do {
            if (cursor->inUse != 0 && cursor->ownerKey == ownerKey && cursor->owner == (int) owner) {
                if (mode == SEF_RECORD_MODE_RELEASE) {
                    sefFreeScheduler(slot);
                    count = sefEffectRecordCount;
                } else if (mode == SEF_RECORD_MODE_SET_FLAG_80) {
                    cursor->flags |= 0x80;
                    count = sefEffectRecordCount;
                } else if (mode == SEF_RECORD_MODE_CLEAR_FLAG_80) {
                    cursor->flags &= ~0x80;
                    count = sefEffectRecordCount;
                } else if (mode == SEF_RECORD_MODE_GATE_ON) {
                    cursor->gate = 1;
                } else if (mode == SEF_RECORD_MODE_GATE_OFF) {
                    cursor->gate = 0;
                } else if (mode == SEF_RECORD_MODE_FLAGS_880) {
                    cursor->flags |= 0x880;
                    count = sefEffectRecordCount;
                }
            }
            cursor += 1;
            slot += 1;
        } while (slot < count);
    }
}

void sefSetOwnerFlags(int effectKey, int flags) {
    SefEffectRecord *records;
    SefEffectRecord *cursor;
    int count;
    int savedCount;
    int i;

    records = sefGetScheduler();
    count = sefEffectRecordCount;
    i = 0;
    if (count > 0) {
        cursor = records;
        savedCount = count;
        do {
            if (cursor->inUse != 0) {
                if (effectKey < 0 || cursor->ownerKey == effectKey) {
                    cursor->flags |= flags;
                    count = savedCount;
                }
            }
            i++;
            cursor++;
        } while (i < count);
    }
    sefSetSlotFlags(effectKey, flags);
}

/* Clears the mask bits of every in-use record whose owner matches (any record when owner is negative), then calls
 * sefClearSlotFlags with the same owner and mask. */
void sefClearOwnerFlags(int effectKey, int mask)
{
    SefEffectRecord *table;
    SefEffectRecord *record;
    int count;
    int i;

    table = sefGetScheduler();
    count = sefEffectRecordCount;
    i = 0;
    if (count > 0) {
        record = table;
        do {
            if (record->inUse != 0 && (effectKey < 0 || record->ownerKey == effectKey)) {
                record->flags &= ~mask;
                count = sefEffectRecordCount;
            }
            i += 1;
            record += 1;
        } while (i < count);
    }
    sefClearSlotFlags(effectKey, mask);
}

typedef struct GateChunk112 {
    unsigned long long dwords[14];
} GateChunk112;

typedef struct GateChunk128 {
    unsigned long long dwords[16];
} GateChunk128;

typedef struct GateSource {
    GateChunk112 front;
    GateChunk128 tail;
} GateSource;

/* The light fields of a unit actor (BattleUnit.unit, a XenoActor) as sefSetGateBlocks writes them: flags is drawFlags
 * (+0xBC), chunk400 and chunk512 are the two halves of its XenoLight (+0x190). */
typedef struct GateBlock {
    unsigned char unmodeled_000[188];
    int flags;
    unsigned char unmodeled_0c0[400 - 192];
    GateChunk112 chunk400;
    GateChunk128 chunk512;
} GateBlock;

/* Bit of GateBlock.flags: set when sefSetGateBlocks gives the block its free source (owner zero). */
#define SEF_GATE_BLOCK_CLOSED 0x8


void sefSetGateBlocks(int owner, int reset) {
    GateSource *ownedSource;
    GateSource *freeSource;
    BattleUnit **table;
    BattleUnit *entry;
    GateBlock *block;
    int count;
    int i;
    int k;

    ownedSource = sefLightSetActiveGet();
    freeSource = sefLightSetDefaultGet();
    count = unitTblGet(UNIT_SIDE_ALL, &table);
    for (i = 0; i < count; i += 1) {
        entry = table[i];
        if (entry != 0) {
            block = (GateBlock *)entry->unit;
            if (owner != 0) {
                block->chunk400 = ownedSource->front;
                block->chunk512 = ownedSource->tail;
                block->flags &= ~SEF_GATE_BLOCK_CLOSED;
            } else {
                block->chunk400 = freeSource->front;
                block->chunk512 = freeSource->tail;
                if (reset != 0) {
                    for (k = 0; k < 3; k += 1) {
                        memset(&block->chunk400.dwords[k * 2], 0, 16);
                    }
                    for (k = 0; k < 3; k += 1) {
                        memset(&block->chunk512.dwords[k * 2], 0, 16);
                    }
                }
                block->flags |= SEF_GATE_BLOCK_CLOSED;
            }
        }
    }
}

/* Key group id (ownerKey >> 12) whose records sefUpdateGroupGate gates, and the record flag it toggles. */
#define SEF_GATE_GROUP_ID 350
#define SEF_RECORD_GATE_FLAG 0x400000

void sefUpdateGroupGate(int owner, int enable, int reset) {
    SefEffectRecord *table;
    SefEffectRecord *cursor;
    int slot;
    int scanLimit;
    int recordCount;
    int groupKind;
    int gateOwned;
    int flagSetBit;
    int flagClearMask;

    table = sefGetScheduler();
    if (enable > 0) {
        sefSetGateBlocks(owner, reset);
        if (sefGateOwnerZeroFlag != 0 && reset != 0) {
            sefGateOwnerZeroFlag += 1;
        }
    }
    scanLimit = sefEffectRecordCount;
    slot = 0;
    if (scanLimit > 0) {
        cursor = table;
        groupKind = SEF_GATE_GROUP_ID;
        flagClearMask = ~SEF_RECORD_GATE_FLAG;
        gateOwned = 1;
        flagSetBit = SEF_RECORD_GATE_FLAG;
        recordCount = scanLimit;
        do {
            if (cursor->inUse != 0) {
                if ((cursor->ownerKey >> 12) == groupKind) {
                    if (owner != 0) {
                        scanLimit = recordCount;
                        cursor->gate = gateOwned;
                        cursor->flags &= flagClearMask;
                    } else {
                        scanLimit = recordCount;
                        cursor->gate = 0;
                        cursor->flags |= flagSetBit;
                    }
                }
            }
            cursor += 1;
            slot += 1;
        } while (slot < scanLimit);
    }
}

short sefGateIdList[] = { 55, 56, 57, 0 };

/*
 * Scans the zero-terminated short table sefGateIdList for id. When id is listed,
 * sefGateOwnerZeroFlag takes whether owner is zero and sefUpdateGroupGate runs for owner.
 */
void sefSetGateForId(int owner, int id) {
    int i;
    int found;

    found = 0;
    if (sefGateIdList[0] != 0) {
        i = 0;
        do {
            if (id == sefGateIdList[i]) {
                found = 1;
            }
            i += 1;
        } while (sefGateIdList[i] != 0);
    }
    if (found != 0) {
        sefGateOwnerZeroFlag = owner == 0;
        sefUpdateGroupGate(owner, id, found);
    }
}

short sefDefaultGateIdList[] = { 2, 12, 13, 0 };

void sefSetGateDefault(int first, int second)
{
    short *cursor;
    short entry;

    sefGateOwnerZeroFlag = first == 0;
    if (sefDefaultGateIdList[0] != 0) {
        cursor = sefDefaultGateIdList;
        do {
            cursor += 1;
            entry = *cursor;
        } while (entry != 0);
    }
    sefUpdateGroupGate(first, second, 1);
}

/* Set by sefSetCurrentOwner after sefKillEffect(-1); this file has no other reader. */
int sefFlagWord = 0;
int sefCurrentOwner = 0;
unsigned int sefRandSeed = 2599403U;
int sefNoiseReady = 0;
extern unsigned char _scheduler[];
SefEffectRecord *sefEffectRecords = (SefEffectRecord *) _scheduler;
short sefLinkedPartIndex = 0;
const char sefSclLogLabel[8] = "scl";
const char sefSizeLogLabel[8] = "size";

/* Changes the current owner: releases every effect record, then stores owner in sefCurrentOwner. */
void sefSetCurrentOwner(int effectKey) {
    sefKillEffect(-1);
    sefCurrentOwner = effectKey;
}

/* Returns the current owner set by sefSetCurrentOwner. */
int sefGetCurrentOwner(void) {
    return sefCurrentOwner;
}


/* One slot of the SEF_HANDLE_SLOT_COUNT-slot handle table sefHandleSlots: the value a handle was
 * registered with, and the handle. This file never reads the last four bytes. */
typedef struct SefHandleSlot {
    void *value;
    int handle;
    u8 unmodeled_08[4];
} SefHandleSlot;

SefHandleSlot sefHandleSlots[48] = { 0 };

/* Clears the 48-slot handle table, calls srsReleaseOtherOwnerEffectSlots, then refills the
 * slots from the records of srsRecordTable that hold a buffer: the eight wait-slot
 * records first, then the 48 effect slots. Each refilled slot takes the record's key, with value 0. */
void sefResetHandleSlots(void) {
    int count;
    int i;

    memset(sefHandleSlots, 0, SEF_HANDLE_SLOT_COUNT * sizeof(SefHandleSlot));
    count = 0;
    srsReleaseOtherOwnerEffectSlots();
    for (i = 0; i < 8; i++) {
        if (srsRecordTable.waitSlots[i].handle != 0) {
            sefHandleSlots[count].value = 0;
            sefHandleSlots[count].handle = srsRecordTable.waitSlots[i].key;
            count++;
        }
    }
    for (i = 0; i < SEF_HANDLE_SLOT_COUNT; i++) {
        if (srsRecordTable.slots[i].handle != 0) {
            sefHandleSlots[count].value = 0;
            sefHandleSlots[count].handle = srsRecordTable.slots[i].key;
            count++;
        }
    }
}

void sefDummy_00265400(void) {

}

/* Registers handle with value in the first free slot, unless the handle is
 * already in the table. */
void sefRegisterHandle(int handle, void *value) {
    int freeSlot;
    int i;

    freeSlot = -1;
    for (i = 0; i < SEF_HANDLE_SLOT_COUNT; i++) {
        if (sefHandleSlots[i].handle == handle) {
            break;
        }
        if (freeSlot < 0 && sefHandleSlots[i].handle == 0) {
            freeSlot = i;
        }
    }
    if (i == SEF_HANDLE_SLOT_COUNT) {
        sefHandleSlots[freeSlot].value = value;
        sefHandleSlots[freeSlot].handle = handle;
    }
}

/* Clears every slot whose handle srsIsEffectImageKey accepts. */
void sefPruneHandleSlots(void) {
    SefHandleSlot *slot;
    int *handle_ptr;
    int handle;
    int i;

    slot = sefHandleSlots;
    handle_ptr = &slot->handle;
    for (i = 0; i < SEF_HANDLE_SLOT_COUNT; i++) {
        handle = handle_ptr[0];
        if (handle != 0 && srsIsEffectImageKey(handle) != 0) {
            handle_ptr[-1] = 0;
            handle_ptr[0] = 0;
        }
        handle_ptr += 3;
    }
}

/* Calls sefStartEffectKeyDefault with the value and the handle of every registered slot. */
void sefDispatchHandleSlots(void) {
    SefHandleSlot *slot;
    int handle;
    int i;

    slot = sefHandleSlots;
    for (i = 0; i < SEF_HANDLE_SLOT_COUNT; i++) {
        handle = slot->handle;
        if (handle != 0) {
            sefStartEffectKeyDefault(slot->value, handle);
        }
        slot++;
    }
}

/* The owner of a record seen as the group object pointer it is: sef.h still declares the field int, and the
 * saved list keeps it next to object with the same type. The integrator retypes SefEffectRecord.owner. */
typedef struct SefEffectRecordOwnerView {
    unsigned char unmodeled_000[0x50];
    struct SefGroupObject *owner;      /* +0x50 */
} SefEffectRecordOwnerView;

static SefSavedEffect sefEffectRecordList[64];
extern int sefEffectListSavedFlag;

/* Key of the record that sefPushEffect never saves. */
#define SEF_SAVE_EXCLUDED_KEY 0x10000021

/* Number of entries of sefEffectRecordList (the saved effect records). */
#define SEF_EFFECT_LIST_COUNT 64

/* Copies one quadword through t0; the two outputs tell the compiler the whole destination vector is written. */
static inline void sefCopyListVector(Vector4 *destination, const void *source)
{
    __asm__ __volatile__("lq $8, 0(%5)\n"
                         "sq $8, 0(%4)\n"
                         : "=m"(destination->x), "=m"(destination->y), "=m"(destination->z), "=m"(destination->w)
                         : "r"(destination), "r"(source)
                         : "$8", "memory");
}

/* Clears the sefEffectRecordList list, sets sefEffectListSavedFlag to 1 and calls sefResetHandleSlots.
 * Copies the in-use records that pass its flag and key tests into the list
 * (0x50-byte entries), registers their handles through sefRegisterHandle, and
 * passes the list and its count to sefCollectPersistentSlots. */
void sefPushEffect(void)
{
    SefEffectRecord *record;
    int count;
    int i;

    count = 0;
    record = sefGetScheduler();
    memset(sefEffectRecordList, 0, SEF_EFFECT_LIST_COUNT * sizeof(SefSavedEffect));
    sefEffectListSavedFlag = 1;
    sefResetHandleSlots();
    for (i = 0; i < sefEffectRecordCount; i++, record++) {
        if (record->inUse != 0 && count < sefEffectRecordCount && record->ownerKey != SEF_SAVE_EXCLUDED_KEY
            && (record->flags & 0x04000000)) {
            sefRegisterHandle(record->ownerKey, 0);
            sefCopyListVector(&sefEffectRecordList[count].position, record);
            sefCopyListVector(&sefEffectRecordList[count].rotation, &record->rotation);
            sefCopyListVector(&sefEffectRecordList[count].scale, &record->scale);
            sefEffectRecordList[count].flags = record->flags;
            sefEffectRecordList[count].owner = ((SefEffectRecordOwnerView *) record)->owner;
            sefEffectRecordList[count].key = record->ownerKey;
            sefEffectRecordList[count].object = record->object;
            sefEffectRecordList[count].gate = record->gate;
            sefEffectRecordList[count].cellSlot = record->cellSlot;
            sefEffectRecordList[count].slotIndex = i;
            count++;
        }
    }
    sefCollectPersistentSlots(sefEffectRecordList, count);
}

/* Releases every record, dispatches the handle slots, then re-creates each
 * entry of the sefEffectRecordList list through sefCreateEffectCf2 and copies its fields back. */
void sefPopEffect(void)
{
    SefEffectRecord *record;
    int i;

    sefKillEffect(-1);
    sefDispatchHandleSlots();
    sefEffectListSavedFlag = 0;
    for (i = 0; i < sefEffectRecordCount; i++) {
        if (sefEffectRecordList[i].key != 0 && &sefEffectRecordList[i].key != 0) {
            record = sefCreateEffectCf2(sefEffectRecordList[i].key, &sefEffectRecordList[i].position,
                                          &sefEffectRecordList[i].rotation, &sefEffectRecordList[i].scale,
                                          sefEffectRecordList[i].slotIndex);
            if (record == 0) {
                continue;
            }
            ((SefEffectRecordOwnerView *) record)->owner = sefEffectRecordList[i].owner;
            record->flags = sefEffectRecordList[i].flags;
            record->object = sefEffectRecordList[i].object;
            record->gate = sefEffectRecordList[i].gate;
        }
    }
}

/* Calls sefGetScheduler and returns its table, then spins for a count that follows sefEffectRecordCount. */
void *sefGetEffectRecordsAfterScan(void) {
    void *records;
    int i;

    records = sefGetScheduler();
    i = sefEffectRecordCount;
    if (i > 0) {
        for (i = i - 1; i != 0; i--) {
        }
    }
    return records;
}

/* .bss of sef.c (pass 2): static storage, address order (see notes2-b01). */
static unsigned char _ptAlloc[0xBD110];
static SefEntryCounts sefEntryCounts[58];

/* .data of sef.c (pass 2): zero-filled original objects, address order. */
SefCameraState sefCameraState = { 0 };
