#ifndef INCLUDE_MAIN_TU097_H
#define INCLUDE_MAIN_TU097_H

#include "main/tu096.h"

/* FontObject.flags: FONT_OBJ_EASE selects fontTrackEase over fontTrackAdvance; FONT_OBJ_ACTIVE is set while moving. */
#define FONT_OBJ_EASE 0x2
#define FONT_OBJ_ACTIVE 0x10000
/* FontObject.flags bit set while an opening or closing step runs (fontObjUpdate, fontObjGetQuadCorners). */
#define FONT_OBJ_PHASE_RUNNING 0x20000

/* FontObject.move_phase. */
enum FontMovePhase {
    FONT_PHASE_IDLE,
    FONT_PHASE_OPENING,
    FONT_PHASE_CLOSING
};

/* Four tracks at 0x00, then the words and bytes the callers touch. draw_bits is the Q word
 * fontObjDrawQuad receives; move_phase is 0 idle, 1 opening and 2 closing. The active flag
 * 0x10000 in flags is set when an opening starts and cleared when a closing ends or
 * fontObjAdvanceTracks runs. */
struct FontObject {
    struct FontTrack tracks[4];
    int draw_bits;
    int flags;
    unsigned char move_phase;
    unsigned char aux_state;
};

/* FontDrawItem.flags bits that fontObjDraw tests. */
enum FontDrawFlag {
    FONT_DRAW_COLOR_ALPHA = 0x4,   /* alpha = colour alpha * opacity; else the opacity alone */
    FONT_DRAW_USE_SIZE = 0x8,      /* draw size from size_w and size_h instead of the cell */
    FONT_DRAW_OFFSET = 0x20,       /* add offset_x and offset_y to the cell position */
    FONT_DRAW_TRIM_BOTH = 0x100,   /* shrink the far corner by 16 in x and y */
    FONT_DRAW_TRIM_X = 0x800,      /* shrink the far corner by 16 in x */
    FONT_DRAW_TRIM_Y = 0x1000,     /* shrink the far corner by 16 in y */
    FONT_DRAW_FLIP_X = 0x200,      /* swap the left and right screen edges */
    FONT_DRAW_FLIP_Y = 0x400       /* swap the top and bottom screen edges */
};

/* Functions of src/main/tu097.c that other files call. */

void fontTrackSetPoint(struct FontTrack *track, int x, int y);
void fontTrackSetBoth(struct FontTrack *track, int x, int y);
void fontTrackAdvance(struct FontTrack *track);
int fontTrackAtTarget(struct FontTrack *track);
void fontObjAdvanceTracks(struct FontObject *object);
int * fontObjClearRunLists(void);
void fontObjClearSlots(void);
void fontObjResetAndBuild(void);
void fontObjResetBufferSlots(void);
void fontObjResetColor(void);
void fontObjResetOpacity(void);
void fontObjSetColor(unsigned long long colour);
void fontObjSetDepth(int depth);
void fontObjSetOpacity(int opacity);
void fontObjStartClose(struct FontObject *object);
void fontObjStartOpen(struct FontObject *object);

/* The draw request fontObjDraw reads: cell index, screen position, offsets, size and flags. */
struct FontDrawItem {
    int cell_index;
    int x;
    int y;
    int offset_x;
    int offset_y;
    int size_w;
    int size_h;
    int flags;
};

#endif /* INCLUDE_MAIN_TU097_H */
