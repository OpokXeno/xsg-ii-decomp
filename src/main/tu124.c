#include "common.h"
#include "main/tu124.h"
#include "shared.h"
#include "main/tu097.h"
#include "main/pad.h"

/* Choice menus and text entries. The counts, row sizes and states below are the values this TU writes and
 * compares; the two record pools are the choice menu slots and the text entry pool. */
#define CHOICE_MENU_SLOTS 2       /* choiceMenuSlots holds two menus, index 0 and 1 */
#define CHOICE_ROW_HEIGHT 24      /* one row of a choice menu list, in pixels */
#define CHOICE_STYLE_RESERVE 11   /* room a style line takes from the menu height */
#define CHOICE_STEPS 4            /* opening and closing steps of a choice menu */
#define CHOICE_FLAG_ACTIVE 1      /* ChoiceMenuRecord.flags bit 0, set by choiceMenuSetActive */
#define CHOICE_FLAG_NO_RESULT 2   /* ChoiceMenuRecord.flags bit 1: choiceMenuGetResult returns 0 while it is set */
#define TEXT_ENTRY_POOL_SIZE 4    /* textEntryPool holds four entries */
#define TEXT_ENTRY_STEPS 4        /* opening steps of a text entry; a removed entry counts down from here */

enum ChoiceMenuKind {
    CHOICE_KIND_FREE = 0,
    CHOICE_KIND_OPEN = 1,
    CHOICE_KIND_OPENING = 2,
    CHOICE_KIND_CLOSING = 3
};

/* The state byte shared by the text entries, the text block and the choice menus: 0 free, 1 open, 2 opening, 3
 * closing (a removed entry is state 3 too, counting its tick down). */
enum TextState {
    TEXT_STATE_FREE = 0,
    TEXT_STATE_OPEN = 1,
    TEXT_STATE_OPENING = 2,
    TEXT_STATE_CLOSING = 3
};

/* The cursor sub-object at +0x40 of a choice menu record: the first word is the selected index (the cursor
 * offset); cursorBufferInit and the cursor moves write the rest, which is not modeled here. */
typedef struct ChoiceMenuCursor {
    unsigned int selected;
    unsigned char unmodeled_04[0x14 - 0x04];
} ChoiceMenuCursor;

/* The cursor functions of tu098 take the CursorBuffer that this sub-object heads (0x14 bytes of it). */
void cursorBufferInit(ChoiceMenuCursor *cursor, int capacity, int unit_size, int mode_flags);
void cursorBufferSetCapacity(ChoiceMenuCursor *cursor, int capacity);
void cursorBufferSetOffset(ChoiceMenuCursor *cursor, int cursor_offset);
void cursorBufferMoveBy(ChoiceMenuCursor *cursor, int delta);
void cursorBufferMoveUnits(ChoiceMenuCursor *cursor, int units);
unsigned int cursorBufferGetIndex(ChoiceMenuCursor *cursor);

/* A choice menu record on choiceMenuSlots (92-byte stride): the layout words at +0x0 to +0xC, the item, value and
 * item flag lists at +0x10 to +0x18, the flags word at +0x1C, the track at +0x24, the cursor at +0x40 and the
 * style index and style word at +0x56 and +0x58. */
typedef struct ChoiceMenuRecord {
    int x;
    int y;
    int width;
    int height;
    unsigned char **items;
    short *values;
    unsigned char *itemFlags;
    unsigned int flags;
    unsigned int unmodeled_20;
    struct FontTrack track;
    ChoiceMenuCursor cursor;
    unsigned char phase;
    unsigned char kind;
    unsigned char style_index;
    unsigned char unmodeled_57;
    unsigned int style_value;
} ChoiceMenuRecord;

extern ChoiceMenuRecord choiceMenuSlots[];

/* Clears both choice menu slots: the layout words, the lists, the flags and the unmodeled words, the track, and the
 * cursor (capacity 0, unit size 1, mode 0); phase and kind are cleared last. */
void choiceMenuResetAll(void)
{
    int index;

    for (index = 0; index < CHOICE_MENU_SLOTS; index++) {
        choiceMenuSlots[index].height = 0;
        choiceMenuSlots[index].width = 0;
        choiceMenuSlots[index].y = 0;
        choiceMenuSlots[index].x = 0;
        choiceMenuSlots[index].style_index = 0;
        choiceMenuSlots[index].items = 0;
        choiceMenuSlots[index].values = 0;
        choiceMenuSlots[index].itemFlags = 0;
        choiceMenuSlots[index].unmodeled_20 = 0;
        choiceMenuSlots[index].flags = 0;
        fontTrackSetBoth(&choiceMenuSlots[index].track, 0, 0);
        cursorBufferInit(&choiceMenuSlots[index].cursor, 0, 1, 0);
        choiceMenuSlots[index].phase = 0;
        choiceMenuSlots[index].kind = CHOICE_KIND_FREE;
    }
}

extern int textStyleTable[];
int choiceMenuIsOpen(int index);

/* Opens choice menu slot index: the layout words x, y, width and height, the flags word (bit 0 set, bit 1 clear),
 * style_index with its style word textStyleTable[style_index] (0 for style 0), and the item, value and item flag
 * lists. The cursor is initialised with unit_size, capacity = the item count and offset 0; the slot starts in kind 2
 * (opening). Returns 1 when the slot was opened, 0 when index is not 0 or 1 or the slot is already open. */
int choiceMenuOpen(unsigned int index, int x, int y, int width, int height, int style_index, unsigned char unit_size,
                   unsigned int flags, unsigned char **items, short *values, unsigned char *itemFlags)
{
    int count;

    if (index >= CHOICE_MENU_SLOTS) {
        return 0;
    }
    if (!choiceMenuIsOpen(index)) {
        choiceMenuSlots[index].x = x;
        choiceMenuSlots[index].y = y;
        choiceMenuSlots[index].width = width;
        choiceMenuSlots[index].height = height;
        choiceMenuSlots[index].style_index = style_index;
        choiceMenuSlots[index].items = items;
        choiceMenuSlots[index].values = values;
        choiceMenuSlots[index].itemFlags = itemFlags;
        choiceMenuSlots[index].flags = (flags & ~CHOICE_FLAG_NO_RESULT) | CHOICE_FLAG_ACTIVE;
        if (style_index) {
            choiceMenuSlots[index].style_value = textStyleTable[style_index];
        } else {
            choiceMenuSlots[index].style_value = 0;
        }
        choiceMenuSlots[index].unmodeled_20 = 0;
        choiceMenuSlots[index].phase = 0;
        fontTrackSetBoth(&choiceMenuSlots[index].track, 0, 0);
        cursorBufferInit(&choiceMenuSlots[index].cursor, 0, unit_size, 0);
        if (items) {
            count = 0;
            while (items[count] != 0) {
                count++;
            }
            cursorBufferSetCapacity(&choiceMenuSlots[index].cursor, count);
        } else {
            cursorBufferSetCapacity(&choiceMenuSlots[index].cursor, 0);
        }
        cursorBufferSetOffset(&choiceMenuSlots[index].cursor, 0);
        choiceMenuSlots[index].kind = CHOICE_KIND_OPENING;
        return 1;
    }
    return 0;
}

/* The request block choiceMenuOpenFromRequest reads: the caller's open parameters at fixed offsets. */
typedef struct ChoiceMenuRequest {
    int x;
    int y;
    int width;
    int height;
    int style_index;
    unsigned char unit_size;
    unsigned char unmodeled_15[3];
    unsigned int flags;
    unsigned char **items;
    short *values;
    unsigned char *itemFlags;
    int cursor_offset;
} ChoiceMenuRequest;

extern PadRecord PadData;

/* Opens a choice menu from a request block. The records are written as in choiceMenuOpen (the item stores
 * in request order), then the track scroll: the target row moves up while the cursor row does not fit under
 * the height (11 rows short when a style is set), then back while the cursor row is above the top. */
int choiceMenuOpenFromRequest(unsigned int index, ChoiceMenuRequest *request)
{
    int count;

    if (index >= CHOICE_MENU_SLOTS) {
        return 0;
    }
    if (choiceMenuIsOpen(index)) {
        return 0;
    }
    choiceMenuSlots[index].x = request->x;
    choiceMenuSlots[index].y = request->y;
    choiceMenuSlots[index].width = request->width;
    choiceMenuSlots[index].height = request->height;
    choiceMenuSlots[index].style_index = request->style_index;
    choiceMenuSlots[index].flags = (request->flags & ~CHOICE_FLAG_NO_RESULT) | CHOICE_FLAG_ACTIVE;
    choiceMenuSlots[index].items = request->items;
    choiceMenuSlots[index].values = request->values;
    choiceMenuSlots[index].itemFlags = request->itemFlags;
    if (request->style_index != 0) {
        choiceMenuSlots[index].style_value = textStyleTable[request->style_index];
    } else {
        choiceMenuSlots[index].style_value = 0;
    }
    choiceMenuSlots[index].unmodeled_20 = 0;
    choiceMenuSlots[index].phase = 0;
    fontTrackSetBoth(&choiceMenuSlots[index].track, 0, 0);
    cursorBufferInit(&choiceMenuSlots[index].cursor, 0, request->unit_size, 0);
    if (request->items != 0) {
        count = 0;
        while (request->items[count] != 0) {
            count++;
        }
        cursorBufferSetCapacity(&choiceMenuSlots[index].cursor, count);
    } else {
        cursorBufferSetCapacity(&choiceMenuSlots[index].cursor, 0);
    }
    cursorBufferSetOffset(&choiceMenuSlots[index].cursor, request->cursor_offset - 1);
    choiceMenuSlots[index].kind = CHOICE_KIND_OPENING;
    for (;;) {
        unsigned int room = choiceMenuSlots[index].height;
        ChoiceMenuRecord *record = &choiceMenuSlots[index];
        int *scroll = &record->track.target_pair.y;
        unsigned int rows;

        room -= *scroll;
        rows = cursorBufferGetIndex(&choiceMenuSlots[index].cursor) * CHOICE_ROW_HEIGHT;
        if (choiceMenuSlots[index].style_value != 0) {
            if (room - CHOICE_STYLE_RESERVE >= rows) {
                break;
            }
        } else if (room >= rows) {
            break;
        }
        *scroll -= CHOICE_ROW_HEIGHT;
    }
    for (;;) {
        unsigned int rows;

        rows = cursorBufferGetIndex(&choiceMenuSlots[index].cursor) * CHOICE_ROW_HEIGHT;
        if (rows >= (unsigned int) -choiceMenuSlots[index].track.target_pair.y) {
            break;
        }
        choiceMenuSlots[index].track.target_pair.y += CHOICE_ROW_HEIGHT;
    }
    fontTrackAdvance(&choiceMenuSlots[index].track);
    return 1;
}

/* 92-byte choice menu slot (two slots). flags bit 0 is the active bit set by choiceMenuSetActive; kind is
 * 0 free, 1 open, 2 opening, 3 closing (choiceMenuClose stores 3); phase counts the four opening steps. */
typedef struct ChoiceMenuSlot {
    u32 flags;
    u8 phase;
    u8 kind;
    u8 unmodeled_06[86];
} ChoiceMenuSlot;

extern ChoiceMenuSlot D_006ABDAC[];

/* Sets or clears the active bit (flags bit 0) of choice menu slot index. */
void choiceMenuSetActive(int index, int enable)
{
    if (enable != 0) {
        D_006ABDAC[index].flags |= CHOICE_FLAG_ACTIVE;
        return;
    }
    D_006ABDAC[index].flags &= ~CHOICE_FLAG_ACTIVE;
}

/* Cursor entry of choice menu slot index: 0 while flags bit 0x2 is set, else the selected index plus 1. */
int choiceMenuGetResult(int index)
{
    if (choiceMenuSlots[index].flags & CHOICE_FLAG_NO_RESULT) {
        return 0;
    }
    return choiceMenuSlots[index].cursor.selected + 1;
}

/* A choice menu record seen from its state byte at +0x55 (92-byte stride). */
typedef struct ChoiceMenuStateView {
    unsigned char kind;
    unsigned char unmodeled_01[91];
} ChoiceMenuStateView;

extern ChoiceMenuStateView D_006ABDE5[];

/* Returns 1 while choice menu slot index is open or opening (kind 1 or 2), 0 when it is free or closing. */
int choiceMenuIsOpen(int index)
{
    if (D_006ABDE5[index].kind != CHOICE_KIND_FREE && D_006ABDE5[index].kind != CHOICE_KIND_CLOSING) {
        return 1;
    }
    return 0;
}

extern ChoiceMenuSlot D_006ABDE0[];

/* Closes choice menu slot index: kind 3 (closing), phase 4, active bit cleared. */
void choiceMenuClose(int index)
{
    D_006ABDE0[index].kind = CHOICE_KIND_CLOSING;
    D_006ABDE0[index].phase = CHOICE_STEPS;
    choiceMenuSetActive(index, 0);
}

int choiceMenuShow(unsigned int index)
{
    if (index >= CHOICE_MENU_SLOTS) {
        return 0;
    }
    choiceMenuSlots[index].flags = (choiceMenuSlots[index].flags & ~CHOICE_FLAG_NO_RESULT) | CHOICE_FLAG_ACTIVE;
    choiceMenuSlots[index].kind = CHOICE_KIND_OPENING;
    fontTrackAdvance(&choiceMenuSlots[index].track);
    return 1;
}

/* Moves the cursor of a choice menu record that is active and open or opening (pad UP and DOWN move the cursor by
 * units, LEFT and RIGHT by one), then keeps the track's target y in step with the cursor row: the target moves by
 * 24 while the row is off the window, and back by 24 while the cursor index lies beyond it. */
void choiceMenuUpdate(unsigned int index)
{
    ChoiceMenuRecord *record;
    unsigned int room;
    unsigned int cursorOffset;
    unsigned int cursorIndex;

    if (index >= CHOICE_MENU_SLOTS) {
        return;
    }
    record = &choiceMenuSlots[index];
    fontTrackEase(&record->track);
    switch (record->kind) {
    case CHOICE_KIND_OPENING:
        record->phase += 1;
        if (record->phase >= CHOICE_STEPS) {
            record->phase = 0;
            record->kind = CHOICE_KIND_OPEN;
        }
        break;
    case CHOICE_KIND_CLOSING:
        record->phase -= 1;
        if (record->phase == 0) {
            record->kind = CHOICE_KIND_FREE;
        }
        break;
    }
    if ((record->flags & CHOICE_FLAG_ACTIVE) && (record->kind == CHOICE_KIND_OPEN || record->kind == CHOICE_KIND_OPENING)) {
        if (PadData.directionRepeated & PAD_UP) {
            cursorBufferMoveUnits(&record->cursor, -1);
        }
        if (PadData.directionRepeated & PAD_DOWN) {
            cursorBufferMoveUnits(&record->cursor, 1);
        }
        if (PadData.directionRepeated & PAD_LEFT) {
            cursorBufferMoveBy(&record->cursor, -1);
        }
        if (PadData.directionRepeated & PAD_RIGHT) {
            cursorBufferMoveBy(&record->cursor, 1);
        }
    }
    for (;;) {
        room = record->height - record->track.target_pair.y;
        cursorIndex = cursorBufferGetIndex(&record->cursor);
        cursorOffset = cursorIndex * CHOICE_ROW_HEIGHT;
        if (record->style_value != 0) {
            if (!(room - CHOICE_STYLE_RESERVE < cursorOffset)) {
                break;
            }
        } else if (!(room < cursorOffset)) {
            break;
        }
        record->track.target_pair.y -= CHOICE_ROW_HEIGHT;
    }
    for (;;) {
        cursorIndex = cursorBufferGetIndex(&record->cursor);
        if (!(cursorIndex * CHOICE_ROW_HEIGHT < -record->track.target_pair.y)) {
            break;
        }
        record->track.target_pair.y += CHOICE_ROW_HEIGHT;
    }
}

int textEncodeNewlines(unsigned char *text)
{
    unsigned char *cursor;
    unsigned char ch;
    int chTopByte;

    cursor = text;
    ch = *cursor;
    chTopByte = ch << 24;
    if (ch != 0) {
        do {
            if (chTopByte < 0) {
                /* lead byte of a two-byte character: skip its trail byte */
                cursor += 1;
            } else if (ch == '\\' && cursor[1] == 'n') {
                cursor[0] = '\n';
                cursor[1] = 0x07;
            }
            cursor += 1;
            ch = *cursor;
            chTopByte = ch << 24;
        } while (ch != 0);
    }
    return chTopByte;
}

/* One text entry: an opening box with a list of pages (the same layout as tu125 reads). */
typedef struct TextEntry {
    int x;
    int y;
    int width;
    int height;
    unsigned char **pages;
    int page;
    unsigned char tick;        /* +0x18 opening step, 0..4 */
    unsigned char state;       /* +0x19 TextState: 0 free, 1 open, 2 opening, 3 closing (removed) */
    unsigned char style_index; /* +0x1A */
    unsigned char unmodeled_1b;
    union {
        unsigned int style_value;  /* +0x1C textStyleTable word of style_index */
        char *title;               /* +0x1C box title (tu125), NULL for none */
    } word_1c;
    unsigned char count;       /* +0x20 voice cues started (tu124) or the page-cut count (tu125) */
    unsigned char atLastPage;  /* +0x21 set when the entry reaches its last page */
    unsigned char unmodeled_22[2];
} TextEntry;

extern TextEntry textEntryPool[];

/* Clears all four entries of the text entry pool. */
void textEntryPoolReset(void)
{
  int index;

  for (index = 0; index < TEXT_ENTRY_POOL_SIZE; index++)
  {
    textEntryPool[index].height = 0;
    textEntryPool[index].width = 0;
    textEntryPool[index].y = 0;
    textEntryPool[index].x = 0;
    textEntryPool[index].style_index = 0;
    textEntryPool[index].pages = 0;
    textEntryPool[index].page = 0;
    textEntryPool[index].tick = 0;
    textEntryPool[index].state = TEXT_STATE_FREE;
  }
}

/* Allocates a text entry: the first free entry (state 0), else the first removed one (state 3). It stores x, y,
 * width, height, pages, style_index and the style word textStyleTable[style_index] (0 for
 * style 0), starts the entry in state 2 and returns it; returns 0 when no entry is free or removed. */
TextEntry *textEntryAlloc(int x, int y, int width, int height, signed char style_index, unsigned char **pages)
{
    TextEntry *entry = 0;
    int index;

    for (index = 0; index < TEXT_ENTRY_POOL_SIZE; index++) {
        if (textEntryPool[index].state == TEXT_STATE_FREE) {
            entry = &textEntryPool[index];
            break;
        }
    }
    if (entry == 0) {
        for (index = 0; index < TEXT_ENTRY_POOL_SIZE; index++) {
            if (textEntryPool[index].state == TEXT_STATE_CLOSING) {
                entry = &textEntryPool[index];
                break;
            }
        }
    }
    if (entry != 0) {
        entry->x = x;
        entry->y = y;
        entry->width = width;
        entry->height = height;
        entry->pages = pages;
        entry->style_index = style_index;
        if (style_index != 0) {
            entry->word_1c.style_value = textStyleTable[style_index];
        } else {
            entry->word_1c.style_value = 0;
        }
        entry->page = 0;
        entry->state = TEXT_STATE_OPENING;
        entry->tick = 0;
        entry->count = 0;
        entry->atLastPage = 0;
    }
    return entry;
}

/* Removes a text entry of textEntryPool: a matching pool entry is set to state 3 (removed) with tick 4. */
void objRemove(TextEntry *entry)
{
    int index;

    for (index = 0; index < TEXT_ENTRY_POOL_SIZE; index++) {
        if (&textEntryPool[index] == entry) {
            textEntryPool[index].state = TEXT_STATE_CLOSING;
            textEntryPool[index].tick = TEXT_ENTRY_STEPS;
            return;
        }
    }
}

void seVoiceStartDefault(int voice);
extern u16 textOptionFlags[];

/* Returns the atLastPage byte of a text entry; in page-advance state 1, with the option bit 0x20 of textOptionFlags
 * set, it starts the default voice cue 0x01000002 once and counts it in count. */
u8 textEntryPlayVoice(TextEntry *entry)
{
    if (entry == 0) {
        return 0;
    }
    if (entry->count == 0) {
        entry->count = 1;
    }
    if (entry->state == TEXT_STATE_OPEN && (textOptionFlags[0] & 0x20)) {
        seVoiceStartDefault(0x01000002);
        entry->count += 1;
    }
    return entry->atLastPage;
}
