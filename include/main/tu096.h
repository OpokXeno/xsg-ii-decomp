#ifndef INCLUDE_MAIN_TU096_H
#define INCLUDE_MAIN_TU096_H

/* One 0x1C-byte font track, the tracks of struct FontObject (main/tu097). current_pair moves toward
 * target_pair under fontTrackEase or fontTrackAdvance; scaled_pair is the current pair in 12.4 fixed
 * point and step_count is the speed while moving, the hold counter at the target. */
struct FontPair {
    int x;
    int y;
};

struct FontTrack {
    struct FontPair current_pair;
    struct FontPair target_pair;
    struct FontPair scaled_pair;
    int step_count;
};
typedef struct FontTrack FontTrack;

/* Functions of src/main/tu096.c that other files call. */

void fontTrackEase(struct FontTrack *track);
void fontTrackSetTarget(struct FontTrack *track, int targetX, int targetY);
int xglTickGet(void);
void xglTickReset(void);

#endif /* INCLUDE_MAIN_TU096_H */
