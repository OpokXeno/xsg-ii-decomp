#ifndef INCLUDE_MAIN_CURSOR_BUFFER_H
#define INCLUDE_MAIN_CURSOR_BUFFER_H

#include "main/font_obj.h"

/* Cursor buffers of src/main/cursor_buffer.c, used by text_box.c, menu_scene.c and font_obj.c. */

/* CursorTextRecord.status_flags and state bits. CURSOR_FLAG_ACTIVE is the active bit of a cursor record
 * (the text cursor entries of font_obj.c and the text records of text_box.c test the same bit). */
#define CURSOR_FLAG_ACTIVE 0x10000
#define CURSOR_FLAG_TICK_PENDING 0x20000
#define CURSOR_STATE_PAUSE 0x1
#define CURSOR_MODE_WRAP 0x100

/*
 * The cursor of a list or menu: a byte offset into a buffer of capacity bytes with unit_size-byte
 * elements, 0x14 bytes. The menus keep one per list (menu_scene, choice_menu, save_screen, debug_test).
 * mode_flags bit 0x100 (CURSOR_MODE_WRAP) is set by cursorBufferInit; menu_scene also uses bit 0 as the
 * mark of the cursor that is current.
 */
typedef struct ListCursor {
    int cursor;                        /* 0x00: byte offset into the buffer */
    int previous_cursor;               /* 0x04: the cursor before the last move */
    unsigned int capacity;             /* 0x08: size of the buffer in bytes (compared unsigned by menuMapListDraw) */
    int unit_size;                     /* 0x0C: size of one element; 0 means none */
    int mode_flags;                    /* 0x10: CURSOR_MODE_WRAP, bit 0 marks the current cursor */
} ListCursor;

/* The older name of ListCursor, still used by the callers that declare their own prototypes. */
typedef ListCursor CursorBuffer;

/*
 * Fields named from the loads and stores of the functions in cursor_buffer.c.
 * Roles are inferred from access patterns only. cursorBufferHandlePad reads a second
 * step record at offset 0x34: its fields sit at 0x48-0x59, the same layout as
 * 0x14-0x25 here. Offsets 0x26-0x47 are not accessed in cursor_buffer.c.
 */
struct CursorTextRecord {
    ListCursor list;                   /* 0x00-0x13: the list cursor the record starts with (cursorBufferInit) */
    int status_flags;                  /* 0x14: bit 0x10000 cleared by cursorBufferReset, bit 0x20000 by cursorBufferTick */
    int count;                         /* 0x18: advanced by step in cursorBufferTick */
    int state;                         /* 0x1C: bit 0x10000 is the active flag, bit 0x1 pauses the tracks */
    int aux_value;                     /* 0x20: cleared by cursorBufferReset */
    unsigned char mode;                /* 0x24: copied into state by cursorBufferReset and cursorBufferTick */
    unsigned char step;                /* 0x25: added to count by cursorBufferTick */
    unsigned char unmodeled_26[0x0E];  /* 0x26-0x33: not accessed in cursor_buffer.c */
    struct FontBlockCursor second_step; /* 0x34-0x5B: the second record (cursorBufferHandlePad) */
    /*
     * 0x5C: the track object handed to fontObjStartOpen, fontObjStartClose and
     * fontObjAdvanceTracks (main/font_obj.c). Its fields are not modelled here; the records
     * live in a pool whose entries are 0xD8 bytes apart (textSystemInit), which sets the extent.
     * The calls cast it to struct FontObject *.
     */
    unsigned char track_object[0xD8 - 0x5C];
};
typedef struct CursorTextRecord CursorTextRecord;

void cursorBufferInit(ListCursor *buffer, int capacity, int unit_size, int mode_flags);
void cursorBufferHandlePad(CursorTextRecord *record);
void cursorBufferActivate(CursorTextRecord *record);
void cursorBufferStartClose(CursorTextRecord *record);
void cursorBufferDeactivate(CursorTextRecord *record);
void cursorBufferReset(CursorTextRecord *record);
void cursorBufferTick(struct FontBlockCursor *step);
void cursorBufferWrapOrClamp(ListCursor *buffer);
void cursorBufferSetOffset(ListCursor *buffer, int cursor);
void cursorBufferMoveBy(ListCursor *buffer, int delta);
void cursorBufferMoveUnits(ListCursor *buffer, int units);
void cursorBufferSetCapacity(ListCursor *buffer, int capacity);
unsigned int cursorBufferGetIndex(ListCursor *buffer);
int cursorBufferGetAlignedOffset(ListCursor *buffer);

#endif /* INCLUDE_MAIN_CURSOR_BUFFER_H */
