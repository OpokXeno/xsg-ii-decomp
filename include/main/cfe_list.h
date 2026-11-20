#ifndef INCLUDE_MAIN_CFE_LIST_H
#define INCLUDE_MAIN_CFE_LIST_H

/* The track key node, the key lists of src/main/cfe_list.c and the functions other files call. */

/*
 * A key of a time-keyed track: one 0x30-byte node of the pool in src/main/cfe_node_pool.c (bit 0 of flags marks a
 * node in use there). The tracks are doubly linked through prev and next in increasing startTime; the list
 * helpers here keep that order. value is the 8.8 fixed-point key value; on rotation tracks it is an angle that
 * cfeChainUnwrapAngles makes continuous along the track. Each direction pair is a 2-D unit vector in 8.8 fixed
 * point, normalized by cfeEntryNormalizeDirs and reset to (1, 0) by cfeEntryResetDirs.
 */
typedef struct CfeTrackNode {
    int flags;                       /* 0x00 */
    int startTime;                   /* 0x04 */
    int value;                       /* 0x08: 8.8 fixed point */
    int firstDirection[2];           /* 0x0C */
    int secondDirection[2];          /* 0x14 */
    struct CfeTrackNode *prev;       /* 0x1C */
    struct CfeTrackNode *next;       /* 0x20 */
    unsigned char unmodeled_24[0x0C];
} CfeTrackNode;

/* CfeTrackNode.flags. */
#define CFE_TRACK_IN_USE 0x1        /* the pool node is in use (src/main/cfe_node_pool.c) */
#define CFE_TRACK_LINEAR 0x2        /* interpolate linearly to the next key (cfeTrackLerp), not by cfeTrackBezier */
#define CFE_TRACK_INTEGER 0x4       /* on the first key: whole numbers, sampled stepwise (cfeTrackSampleStep); */
                                    /* such a track has no directions, cfeTrackUpdateDirections skips it */
#define CFE_TRACK_RESET_FIRST 0x8   /* cfeEntryResetDirs resets the first direction pair to (1, 0) */
#define CFE_TRACK_RESET_SECOND 0x10 /* ... and the second one */

/* An 8.8 fixed-point key value as a float. */
#define CFE_FIXED_TO_FLOAT(value) ((float) ((value) / 256) + (float) ((value) % 256) * (1.0f / 256))

/* A float as an 8.8 fixed-point value: the whole part and the 1/256 steps are converted apart. The value is
 * not parenthesized inside the casts, as in the original: for a difference a - b only a is truncated. */
#define CFE_FLOAT_TO_FIXED(value) ((int) (((int) value) * 256.0f + (int) (((value) - ((int) value)) * 256.0f)))

/* The same conversion as the key setters write it: the whole part `whole` (already (int) value) shifted into
 * place, plus the 1/256 steps of the rest. Integer arithmetic where CFE_FLOAT_TO_FIXED adds in float, so the
 * two forms compile differently. */
#define CFE_FIXED_FROM_WHOLE(whole, value) (((whole) << 8) + (int) (((value) - (float) (whole)) * 256.0f))

CfeTrackNode *cfeListSetValue(CfeTrackNode **list, int key, float value);
CfeTrackNode *cfeListSetValueLinear(CfeTrackNode **list, int key, float value);
CfeTrackNode *cfeListSetAngle(CfeTrackNode **list, int key, float value);
CfeTrackNode *cfeListSetAngleLinear(CfeTrackNode **list, int key, float value);

#endif /* INCLUDE_MAIN_CFE_LIST_H */
