#ifndef INCLUDE_MAIN_FONT_OBJ_H
#define INCLUDE_MAIN_FONT_OBJ_H

#include "shared.h"
#include "main/xgl_tick.h"

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

/* The cursor record of a text block or of a cursor buffer entry: the text pointer at +0x10 is
 * stored by fontObjSetText and cursorBufferSetText. */
/* local view: unit_size and text are pointers here (text at 0x10 is mode_flags in the shared CursorBuffer of cursor_buffer.h). */
struct FontBlockCursor {
    int cursor;
    int previous_cursor;
    int capacity;
    unsigned char *unit_size;
    unsigned char *text;
    int status_flags;
    int count;
    int state;
    int aux_value;
    unsigned char mode;
    unsigned char step;
};

/* One 0xD4-byte text block (a slot of a text box stack): the origin track at +0x00 (its target y at
 * +0x0C is the scroll offset textBoxUpdate lowers one line at a time), the cursor record at +0x30 and
 * the sub-object, a FontObject, at +0x58. flags is zeroed by fontObjInitBlock, as are the four corner words. */
typedef struct FontTextBlock {
    struct FontTrack origin;
    unsigned int flags;
    int corners[4];
    struct FontBlockCursor cursor;
    struct FontObject sub_object;
} FontTextBlock;

union FontCursorCounter {
    unsigned char bytes[4];
    unsigned int packed;
};

/* One 0xD8-byte cursor buffer entry: a text block with a counter word at +0x30. */
struct FontCursorEntry {
    struct FontTrack origin;
    unsigned int flags;
    int corners[4];
    union FontCursorCounter counter;
    struct FontBlockCursor record;
    struct FontObject sub_object;
};

/* Functions of src/main/font_obj.c that other files call. fontObjSetPosition, fontObjSetSize, fontObjSetText and
 * fontObjSubStartOpen are declared by text_box.c alone: its wrappers return their v0, which a void prototype refuses. */

void fontTrackSetPoint(struct FontTrack *track, int x, int y);
void fontTrackSetBoth(struct FontTrack *track, int x, int y);
void fontTrackAdvance(struct FontTrack *track);
int fontTrackAtTarget(struct FontTrack *track);
void fontObjAdvanceTracks(struct FontObject *object);
void **fontObjClearRunLists(void);
void fontObjClearSlots(void);
void fontObjInitGlobals(void);
int fontObjLoadTexture(int texture_index);
void fontObjMarkSlotLoaded(unsigned char slot);
int fontObjBuildBaseVifPacket(void);
void fontObjResetState(void);
void fontObjResetAndBuild(void);
void fontObjResetBufferSlots(void);
void fontObjResetColor(void);
void fontObjResetOpacity(void);
void fontObjSetColor(unsigned long long colour);
void fontObjSetColorRgba(int r, int g, int b, int a);
void fontObjSetAlpha(u64 alpha);
u64 fontObjGetColor(void);
void fontObjSetDepth(int depth);
void fontObjSetOpacity(int opacity);
u8 fontObjGetOpacity(void);
u16 fontObjCellOffsetX(int index);
u16 fontObjCellOffsetY(int index);
void fontObjDrawQuad(int x0, int y0, int x1, int y1, int rgba_bits, int z_bits);
void fontObjStartClose(struct FontObject *object);
void fontObjStartOpen(struct FontObject *object);
void fontObjSetDrawBits(struct FontObject *obj, int draw_bits);
struct FontTrack *fontObjGetOriginTrack(struct FontObject *object);
struct FontTrack *fontObjGetSizeTrack(struct FontObject *object);
void fontObjInitBlock(FontTextBlock *block);
int fontObjDrawText(FontTextBlock *block);
void fontObjSubStartClose(FontTextBlock *block);
void fontObjSubAdvanceTracks(FontTextBlock *block);
void cursorBufferInitEntry(struct FontCursorEntry *entry);
void cursorBufferSetText(struct FontCursorEntry *entry, unsigned char *text);
void cursorBufferUpdateEntry(struct FontCursorEntry *entry);

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

void fontObjDraw(struct FontDrawItem *item);

#endif /* INCLUDE_MAIN_FONT_OBJ_H */
