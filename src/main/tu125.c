#include "common.h"

extern unsigned char textMarkerFound;
extern unsigned char *textMarkerPtrs[];

#define PAGE_MARKER_LEAD 0xA1
#define PAGE_MARKER_TRAIL 0xFC
#define PAGE_MARKER_FILLER 7
#define FONT_COLOR_DEFAULT 0xFFFFF0
#define BOX_OPEN_STEPS 4 /* opening box: tick 0..4 scales the size */
#define CHOICE_VALUE_UNIT 0x10 /* ChoiceMenu.flags: values carry a unit mark */
#define CHOICE_ITEM_HIGHLIGHT 2 /* ChoiceMenu.itemFlags: highlighted item */
#define TEXT_ENTRY_POOL_SIZE 4 /* textEntryPool holds four entries */
#define CHOICE_MENU_SLOTS 2 /* choiceMenuSlots holds two menus */
#define TEXT_PAGE_WAIT_TICKS 20 /* a page without markers stays up this many ticks */
#define TEXT_LINE_HEIGHT_DEFAULT 24 /* the line advance at the start of every line */
#define TEXT_LINE_HEIGHT_ICON 27 /* control codes 6..8 raise the line to at least this */
#define TEXT_LINE_HEIGHT_SPRITE 34 /* control codes 9..15 raise the line to at least this */
#define TEXT_FONT_ZOOM 16 /* xglFontZoom while a text string is drawn */
#define CHOICE_ROW_HEIGHT 24 /* one row of a choice menu list, in pixels */
#define ANALYZE_PANEL_WIDTH 256 /* the ANALYZE box at its full size */
#define ANALYZE_PANEL_HEIGHT 192

/* The state byte shared by the text entries, the text block and the choice menus: 0 free, 1 open, 2 opening, 3
 * closing (removed for an entry). */
enum TextState {
    TEXT_STATE_FREE = 0,
    TEXT_STATE_OPEN = 1,
    TEXT_STATE_OPENING = 2,
    TEXT_STATE_CLOSING = 3
};

/* Cuts text at its page-th 0xA1 0xFC page marker: every marker before it becomes two 0x07 bytes and every
 * marker position is recorded for textMarkerRestore. */
void textMarkerScan(unsigned char *text, unsigned char page)
{
    int found;

    if (text == 0 || page == 0) {
        return;
    }
    found = 0;
    while (*text != 0) {
        if (text[0] == PAGE_MARKER_LEAD && text[1] == PAGE_MARKER_TRAIL) {
            found++;
            textMarkerPtrs[textMarkerFound] = text;
            textMarkerFound++;
            if (found == page) {
                *text = 0;
                break;
            }
            text[0] = PAGE_MARKER_FILLER;
            text[1] = PAGE_MARKER_FILLER;
            text += 2;
        } else {
            if ((signed char)text[0] < 0) {
                text++;
            }
            text++;
        }
    }
}

/* Writes the 0xA1 0xFC page marker back at every position textMarkerScan recorded, then clears the count. */
void textMarkerRestore(void) {
    int markerIndex;

    markerIndex = 0;
    if (textMarkerFound != 0) {
        do {
            textMarkerPtrs[markerIndex][0] = PAGE_MARKER_LEAD;
            textMarkerPtrs[markerIndex][1] = PAGE_MARKER_TRAIL;
            markerIndex++;
        } while (markerIndex < textMarkerFound);
    }
    textMarkerFound = 0;
}

/* Counts the page markers before the terminating zero: the walk of textMarkerScan without its writes. */
int textMarkerCount(unsigned char *text)
{
    int markerCount;

    if (text == 0) {
        return 0;
    }
    markerCount = 0;
    while (*text != 0) {
        if (text[0] == PAGE_MARKER_LEAD && text[1] == PAGE_MARKER_TRAIL) {
            markerCount++;
            text += 2;
        } else {
            if ((signed char)text[0] < 0) {
                text++;
            }
            text++;
        }
    }
    return markerCount;
}

extern unsigned short textOptionFlags[];

#include "main/tu097.h"
#include "main/tu101.h"
#include "main/tu110.h"
#include "main/xgl_font.h"

/* Text window: the box at the bottom of the screen and its single line of text. */
typedef struct TextWindow {
    struct FontTrack track; /* +0x00: x and y are track.current_pair.x and [1], eased by fontTrackEase */
    int width;             /* +0x1C */
    int fade;              /* +0x20 frames of the fade-in, opaque from 8 on */
    unsigned char *text;
} TextWindow;

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

/* One of the two choice menus. */
typedef struct ChoiceMenu {
    int x;
    int y;
    int width;
    int height;
    unsigned char **items;
    short *values;         /* +0x14 number printed after each item, NULL for none */
    unsigned char *itemFlags; /* +0x18 bit 1 highlights the item */
    unsigned int flags;    /* +0x1C bit 4: values carry a unit mark */
    int unmodeled_20[2];
    int rowOffset;         /* +0x28 scroll offset of the first row */
    int unmodeled_2c[5];
    unsigned int selected;
    int unmodeled_44;
    unsigned int count;
    unsigned int columns;
    int unmodeled_50;
    unsigned char tick;
    unsigned char state;
    unsigned char unmodeled_56[2];
    char *title;
} ChoiceMenu;

/* The ANALYZE panel with the stats of the analysed enemy. */
typedef struct AnalyzePanel {
    int x;
    int y;
    unsigned char tick;
    unsigned char state;
    unsigned char unmodeled_0a[2];
    int hp;                /* +0x0C negative when unknown */
    int maxHp;
    int unmodeled_14[2];
    int exp;
    int skillPoints;
    int classPoints;
    int rareItem;          /* +0x28 0 none, negative unknown */
    int item;
    signed char attackType;
    unsigned char attributes;
    signed char rareItemKind;
    signed char itemKind;
} AnalyzePanel;

extern TextWindow textWindow;
extern TextEntry textEntryPool[TEXT_ENTRY_POOL_SIZE];
extern ChoiceMenu choiceMenuSlots[CHOICE_MENU_SLOTS];
extern AnalyzePanel textBlock;

/* Advances one text entry: state 2 ticks in, state 1 pages through its text, state 3 ticks out. */
void textEntryUpdate(TextEntry *entry)
{
    unsigned char state;
    int markerCount;

    if (entry == 0) {
        return;
    }
    state = entry->state;
    switch (state) {
    case TEXT_STATE_OPENING:
        entry->tick++;
        if (entry->tick >= BOX_OPEN_STEPS) {
            entry->tick = 0;
            entry->state = TEXT_STATE_OPEN;
        }
        break;
    case TEXT_STATE_CLOSING:
        entry->tick--;
        if (entry->tick == 0) {
            entry->state = TEXT_STATE_FREE;
        }
        break;
    case TEXT_STATE_OPEN:
        if (entry->pages == 0) {
            break;
        }
        if (entry->count != 0) {
            markerCount = textMarkerCount(entry->pages[entry->page]);
            if (markerCount < entry->count) {
                if (entry->pages[entry->page + 1] != 0) {
                    entry->count = state;
                    entry->page++;
                } else {
                    entry->count = markerCount;
                    entry->atLastPage = state;
                }
            }
        } else {
            entry->tick++;
            if (entry->tick >= TEXT_PAGE_WAIT_TICKS) {
                entry->page++;
                if (entry->pages[entry->page] == 0) {
                    entry->page = 0;
                }
                entry->tick = 0;
            }
        }
        break;
    }
}

extern unsigned char textLineHeight;
void menuTexFlush(void);
void grGpInit(void *packet);
void grPutSprOpen(void *packet);
void grPutSprClose(void);
void grPacketSend(void *packet);
void func_00AA7750(void *packet, int x, int y, short value, int flag);
void menuIconPut(void *packet, int x, int y, int col, int row);
void menuStatSprite(void *packet, int x, int y, int width, int height, int u, int v);
void func_00AA8530(void *packet);
void menuStatusIconsTexState(void *packet);

/* A pair of words per control code, read by the draw calls of codes 9..15 and 16..29. */
typedef struct TextDrawPair {
    int first;
    int second;
} TextDrawPair;

extern TextDrawPair textCodeUv9[];
extern TextDrawPair textCodeUv16[];

/* Draws one inline control code of a text line at (x, y) and returns how far the line advances. */
int textDrawControlCode(int x, int y, int code)
{
    unsigned char packet[16];
    int advance;

    advance = 0;
    menuTexFlush();
    grGpInit(packet);
    switch (code) {
    case 1:
        advance = 20;
        menuIconPut(packet, x + 1, y + 2, 1, 0);
        break;
    case 2:
        advance = 20;
        menuIconPut(packet, x + 1, y + 2, 2, 0);
        break;
    case 3:
        advance = 20;
        menuIconPut(packet, x + 1, y + 2, 0, 0);
        break;
    case 4:
        advance = 20;
        menuIconPut(packet, x + 1, y + 2, 3, 0);
        break;
    case 5:
        advance = 29;
        func_00AA8530(packet);
        grPutSprOpen(packet);
        menuStatSprite(packet, x + 1, y + 2, 27, 17, 464, 234);
        grPutSprClose();
        break;
    case 6:
        advance = 26;
        func_00AA7750(packet, x + 1, y - 2, 52, 1);
        if (textLineHeight < TEXT_LINE_HEIGHT_ICON) {
            textLineHeight = TEXT_LINE_HEIGHT_ICON;
        }
        break;
    case 7:
        advance = 26;
        func_00AA7750(packet, x + 1, y - 2, 53, 1);
        if (textLineHeight < TEXT_LINE_HEIGHT_ICON) {
            textLineHeight = TEXT_LINE_HEIGHT_ICON;
        }
        break;
    case 8:
        advance = 26;
        func_00AA7750(packet, x + 1, y - 2, 16, 1);
        if (textLineHeight < TEXT_LINE_HEIGHT_ICON) {
            textLineHeight = TEXT_LINE_HEIGHT_ICON;
        }
        break;
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        advance = 26;
        func_00AA8530(packet);
        grPutSprOpen(packet);
        menuStatSprite(packet, x + 1, y - 9, 24, 42, textCodeUv9[code - 9].first, textCodeUv9[code - 9].second);
        grPutSprClose();
        if (textLineHeight < TEXT_LINE_HEIGHT_SPRITE) {
            textLineHeight = TEXT_LINE_HEIGHT_SPRITE;
        }
        break;
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
        advance = 38;
        menuStatusIconsTexState(packet);
        grPutSprOpen(packet);
        menuStatSprite(packet, x + 1, y + 1, 36, 17, textCodeUv16[code - 16].first, textCodeUv16[code - 16].second);
        grPutSprClose();
        break;
    }
    grPacketSend(packet);
    return advance;
}

/* Scans one line of text for its inline size codes: '&' + two digits 534..536 raises the line to 3 and 537..543
 * raises it to 10; the widest code adds to textLineHeight. Two-byte characters are skipped whole. The digits are
 * read as raw bytes, so a code number NN appears as 528 + NN in the comparisons below. */
void textAddNextLineSize(unsigned char *text)
{
    unsigned char extraHeight;
    unsigned char *cursor;
    int sizeCode;

    cursor = text;
    extraHeight = 0;
    while (*cursor != 0) {
        if ((signed char)*cursor < 0) {
            cursor++;
        } else if (*cursor == '\n') {
            break;
        } else if (*cursor == '&' && cursor[1] != 0 && cursor[2] != 0) {
            sizeCode = cursor[1] * 10 + cursor[2];
            if ((unsigned int)(sizeCode - 529) < 29) {
                if ((unsigned int)(sizeCode - 534) < 3) {
                    if (extraHeight < 3) {
                        extraHeight = 3;
                    }
                } else if ((unsigned int)(sizeCode - 537) < 7) {
                    if (extraHeight < 10) {
                        extraHeight = 10;
                    }
                }
                cursor += 2;
            }
        }
        cursor++;
    }
    textLineHeight += extraHeight;
}

extern int xglFontZoom;
int xglFontGetMarkupWidth(const unsigned char *text);
void textDrawSegment(int x, int y, void *text, int style);

/* Draws a text: one segment per line or per &NN control code. The control
 * code value is kept in next across the two draw calls. */
void textDrawString(int x, int y, unsigned char *text)
{
    unsigned char *cursor;
    unsigned char *nextOrCode;
    unsigned char *lineStart;
    int cursorX;
    int startX;
    int byteValue;
    unsigned char curChar;
    unsigned char curByte;
    int controlCode;

    startX = x;
    xglFontZoom = TEXT_FONT_ZOOM;
    textLineHeight = TEXT_LINE_HEIGHT_DEFAULT;
    cursorX = startX;
    lineStart = text;
    cursor = text;
    while ((curByte = *cursor) != 0) {
        byteValue = curByte;
        if ((signed char)byteValue < 0) {
            nextOrCode = cursor + 2;
        } else if ((curChar = byteValue) == '\n') {
            *cursor = 0;
            textDrawSegment(cursorX, y, lineStart, 4);
            cursorX = startX;
            xglFontGetMarkupWidth(lineStart);
            *cursor = curChar;
            lineStart = cursor + 1;
            nextOrCode = lineStart;
            textAddNextLineSize(lineStart);
            y += textLineHeight;
            textLineHeight = TEXT_LINE_HEIGHT_DEFAULT;
        } else if (curChar == '&' && cursor[1] != 0 && cursor[2] != 0) {
            controlCode = (cursor[1] - '0') * 10 + (cursor[2] - '0');
            nextOrCode = (unsigned char *)controlCode;
            if (controlCode >= 1 && controlCode <= 29) {
                *cursor = 0;
                textDrawSegment(cursorX, y, lineStart, 4);
                cursorX += xglFontGetMarkupWidth(lineStart);
                *cursor = curChar;
                lineStart = cursor + 3;
                cursorX += textDrawControlCode(cursorX, y, (int)nextOrCode);
                nextOrCode = lineStart;
            } else {
                nextOrCode = cursor + 1;
            }
        } else {
            nextOrCode = cursor + 1;
        }
        cursor = nextOrCode;
    }
    curChar = byteValue;
    textDrawSegment(cursorX, y, lineStart, 4);
}

extern char D_0068B588[];
extern char D_0068B5B8[];
extern char D_0068B5F8[];
extern char D_0068B608[];
extern char D_0068B618[];
extern char D_0068B628[];
extern char D_0069D948[];
extern char D_0069D968[];
extern char D_0069D988[];
extern char D_0069D990[];
extern char D_0069D998[];
extern char D_0069D9A0[];
extern char D_0069D9A8[];
extern char D_0069D9B0[];
extern char D_0069D9B8[];
extern char D_0069D9C0[];
extern char D_0069D9C8[];
extern char D_0069D9D0[];
extern char D_0069D9D8[];
extern char D_0069D9E0[];
extern char D_0069D9E8[];
extern char D_0069D9F0[];
extern char D_0069D9F8[];
extern char D_0069DA00[];
extern char D_0069DA08[];
void xglFontFlush(void);
int xglCdArenaGetSize(void);
void fontObjResetState(void);
void fontObjSetAlpha(int alpha);
void menuDrawTitledWindow(int x, int y, int width, int height, char *title);
void grSetScissor(int x0, int y0, int x1, int y1);
void grResetScissor(void);
int textEncodeNewlines(unsigned char *text);
/* sprintf-like: formats one integer value into buffer. */
void sprintf(char *buffer, const char *format, int value);
/* ov01: name of an item by kind (kind 2 and the others). */
char *func_00A9B7D8(int item, int arg);
char *func_00A9B8D8(int item, int arg);

/* Draws the text window, the text entries, the choice menus and the ANALYZE panel. */
void textDrawAll(void)
{
    char buffer[16];
    int slot;
    int needSeparator;
    TextEntry *entry;
    ChoiceMenu *menu;
    AnalyzePanel *panel;
    int x;
    int y;
    int width;
    int height;
    int top;
    int bottom;
    int i;
    int columnWidth;
    int style;
    int valueX;

    needSeparator = 0;
    xglFontFlush();
    if (xglCdArenaGetSize()) {
        fontObjResetState();
        packetSubmitRequest(0x2BE, 0x200, 0x1C0, 0x10000);
    }
    if (textWindow.fade > 0) {
        fontObjResetState();
        fontObjSetAlpha(0x40);
        if (textWindow.fade < 8) {
            fontObjSetOpacity(textWindow.fade << 4);
        }
        packetSubmitRequestEx(0x2BE, textWindow.track.current_pair.x, textWindow.track.current_pair.y, textWindow.width, 0x20, 0x10104);
        if (textWindow.text) {
            textDrawSegment(0, 0, D_0068B588, 0);
            textDrawString(textWindow.track.current_pair.x + 12, textWindow.track.current_pair.y + 6, textWindow.text);
        }
        fontObjResetColor();
        fontObjResetOpacity();
        xglFontFlush();
    }

    for (slot = 0; slot < TEXT_ENTRY_POOL_SIZE; slot++) {
        if (textEntryPool[slot].state == TEXT_STATE_FREE) {
            continue;
        }
        entry = &textEntryPool[slot];
        if (entry->state == TEXT_STATE_OPEN) {
            x = entry->x;
            y = entry->y;
            width = entry->width;
            height = entry->height;
        } else {
            width = entry->width * entry->tick / BOX_OPEN_STEPS;
            height = entry->height * entry->tick / BOX_OPEN_STEPS;
            x = entry->x + ((entry->width - width) >> 1);
            y = entry->y + ((entry->height - height) >> 1);
            fontObjSetOpacity(entry->tick << 5);
        }
        fontObjResetState();
        menuDrawTitledWindow(x, y, width, height, entry->word_1c.title);
        fontObjResetOpacity();
        grSetScissor(x + 5, entry->word_1c.title == 0 ? y + 5 : y + 16, x + width + 26, y + height + 25);
        if (entry->pages && entry->pages[entry->page]) {
            if (entry->count) {
                textMarkerScan(entry->pages[entry->page], entry->count);
            }
            textDrawSegment(0, 0, D_0068B588, 0);
            textEncodeNewlines(entry->pages[entry->page]);
            textDrawString(entry->x + 8, entry->y + (entry->word_1c.title == 0 ? 8 : 19),
                           entry->pages[entry->page]);
            if (entry->count) {
                textMarkerRestore();
            }
        }
        xglFontFlush();
        grResetScissor();
    }

    for (slot = 0; slot < CHOICE_MENU_SLOTS; slot++) {
        if (choiceMenuSlots[slot].state == TEXT_STATE_FREE) {
            continue;
        }
        menu = &choiceMenuSlots[slot];
        if (menu->state == TEXT_STATE_OPEN) {
            x = menu->x;
            y = menu->y;
            width = menu->width;
            height = menu->height;
        } else {
            width = menu->width * menu->tick / BOX_OPEN_STEPS;
            height = menu->height * menu->tick / BOX_OPEN_STEPS;
            x = menu->x + ((menu->width - width) >> 1);
            y = menu->y + ((menu->height - height) >> 1);
            fontObjSetOpacity(menu->tick << 5);
        }
        top = y + 5;
        fontObjResetState();
        menuDrawTitledWindow(x, y, width, height, menu->title);
        fontObjResetOpacity();
        if (menu->title) {
            top = y + 16;
        }
        grSetScissor(x + 5, top, x + width + 26, y + height + 25);
        if (menu->items) {
            textDrawSegment(0, 0, D_0068B5B8, 0);
            i = 0;
            x = menu->x + 24;
            y = menu->y + menu->rowOffset + (menu->title == 0 ? 6 : 17);
            columnWidth = menu->width / menu->columns;
            if (menu->count != 0) {
                bottom = top + height + 25;
                do {
                    if (top < y + 20 && y < bottom) {
                        /* bit 0: the cursor row; bit 1: a highlighted item */
                        style = i == menu->selected;
                        if (menu->itemFlags) {
                            style = (menu->itemFlags[i] & CHOICE_ITEM_HIGHLIGHT) | (i == menu->selected);
                        }
                        if (menu->items[i]) {
                            textDrawSegment(x, y, menu->items[i], style);
                        }
                        if (menu->values) {
                            valueX = x + columnWidth + ((menu->flags & CHOICE_VALUE_UNIT) == 0 ? -48 : -68);
                            sprintf(buffer, D_0069D988, menu->values[i]);
                            textDrawSegment(0, 0, D_0069D990, 0);
                            textDrawSegment(0, 0, D_0068B5F8, 0);
                            textDrawSegment(valueX, y - 2, buffer, style);
                            textDrawSegment(0, 0, D_0069D998, 0);
                            textDrawSegment(0, 0, D_0068B608, 0);
                            if (menu->flags & CHOICE_VALUE_UNIT) {
                                textDrawSegment(valueX + 28, y, D_0069D9A0, style);
                            }
                        }
                    }
                    if (i % menu->columns == menu->columns - 1) {
                        x = menu->x + 24;
                        y += CHOICE_ROW_HEIGHT;
                    } else {
                        x += columnWidth;
                    }
                    i++;
                } while (i < menu->count);
            }
        }
        xglFontFlush();
        grResetScissor();
    }

    panel = &textBlock;
    if (panel->state == TEXT_STATE_FREE) {
        return;
    }
    if (panel->state == TEXT_STATE_OPEN) {
        x = panel->x;
        width = ANALYZE_PANEL_WIDTH;
        y = panel->y;
        height = ANALYZE_PANEL_HEIGHT;
    } else {
        width = ANALYZE_PANEL_WIDTH * panel->tick / BOX_OPEN_STEPS;
        height = ANALYZE_PANEL_HEIGHT * panel->tick / BOX_OPEN_STEPS;
        x = panel->x + ((ANALYZE_PANEL_WIDTH - width) >> 1);
        y = panel->y + ((ANALYZE_PANEL_HEIGHT - height) >> 1);
        fontObjSetOpacity(panel->tick << 5);
    }
    fontObjResetState();
    menuDrawTitledWindow(x, y, width, height, D_0069D9A8);
    fontObjResetOpacity();
    grSetScissor(x + 5, y + 5, x + width + 26, y + height + 25);
    x = panel->x;
    y = panel->y;
    textDrawSegment(0, 0, D_0068B588, 0);
    textDrawSegment(0, 0, D_0068B618, 0);
    textDrawSegment(x + 16, y + 16, D_0069D9B0, 0);
    textDrawSegment(x + 16, y + 40, D_0069D9B8, 0);
    textDrawSegment(x + 16, y + 64, D_0069D9C0, 0);
    textDrawSegment(x + 16, y + 92, D_0069D9C8, 0);
    textDrawSegment(x + 16, y + 116, D_0069D9D0, 0);
    textDrawSegment(x + 16, y + 140, D_0069D9D8, 0);
    textDrawSegment(x + 16, y + 164, D_0069D9E0, 0);
    textDrawSegment(x + 16, y + 188, D_0069D968, 0);
    textDrawSegment(0, 0, D_0068B628, 0);

    if (panel->hp >= 0) {
        sprintf(buffer, D_0069D9E8, panel->hp);
        textDrawSegment(x + 80, y + 16, buffer, 0);
    } else {
        textDrawSegment(x + 80, y + 16, D_0069D9F0, 0);
    }
    xglFontPrintDirectOT(FONT_COLOR_DEFAULT, D_0069D9F8);
    if (panel->maxHp >= 0) {
        sprintf(buffer, D_0069DA00, panel->maxHp);
        xglFontPrintDirectOT(FONT_COLOR_DEFAULT, buffer);
    } else {
        xglFontPrintDirectOT(FONT_COLOR_DEFAULT, D_0069D9F0);
    }

    if (panel->attackType != 0) {
        textDrawSegment(x + 88, y + 40, menuMainTxdEntry(panel->attackType + 0x151), 0);
    }
    textDrawSegment(x + 88, y + 64, D_0069D948, 0);
    for (i = 0; i < 8; i++) {
        if ((panel->attributes >> i) & 1) {
            if (needSeparator == 1) {
                xglFontPrintDirectOT(FONT_COLOR_DEFAULT, D_0069D9F8);
            }
            needSeparator = 1;
            xglFontPrintDirectOT(FONT_COLOR_DEFAULT, menuMainTxdEntry(i + 0xF0));
        }
    }

    sprintf(buffer, D_0069DA00, panel->exp);
    textDrawSegment(x + 88, y + 92, buffer, 0);
    sprintf(buffer, D_0069DA00, panel->skillPoints);
    textDrawSegment(x + 88, y + 116, buffer, 0);
    sprintf(buffer, D_0069DA00, panel->classPoints);
    textDrawSegment(x + 88, y + 140, buffer, 0);

    if (panel->rareItem > 0) {
        if (panel->rareItemKind == 2) {
            textDrawSegment(x + 88, y + 164, func_00A9B7D8(panel->rareItem, 0), 0);
        } else {
            textDrawSegment(x + 88, y + 164, func_00A9B8D8(panel->rareItem, 0), 0);
        }
    } else if (panel->rareItem < 0) {
        textDrawSegment(x + 88, y + 164, D_0069D9F0, 0);
    } else {
        textDrawSegment(x + 88, y + 164, D_0069DA08, 0);
    }
    if (panel->item > 0) {
        if (panel->itemKind == 2) {
            textDrawSegment(x + 88, y + 188, func_00A9B7D8(panel->item, 0), 0);
        } else {
            textDrawSegment(x + 88, y + 188, func_00A9B8D8(panel->item, 0), 0);
        }
    } else if (panel->item < 0) {
        textDrawSegment(x + 88, y + 188, D_0069D9F0, 0);
    } else {
        textDrawSegment(x + 88, y + 188, D_0069DA08, 0);
    }
    xglFontFlush();
    grResetScissor();
}

/* Clears the tick (+8) and the state byte (+9) of the text block. */
void textBlockClear(void) {
    textBlock.state = TEXT_STATE_FREE;
    textBlock.tick = 0;
}

/* Starts the text block at (x, y): copies the 40 bytes of analysis stats (hp to itemKind) and opens it. */
void textBlockStart(int x, int y, const unsigned char *stats)
{
    if (stats == 0) {
        return;
    }
    textBlock.x = x;
    textBlock.y = y;
    __builtin_memcpy(&textBlock.hp, stats, 40);
    textBlock.state = TEXT_STATE_OPENING;
    textBlock.tick = 0;
}

/* Nonzero while the ANALYZE block is open or opening: states 0 and 3 are inactive. */
int textBlockIsActive(void)
{
    if (textBlock.state == TEXT_STATE_FREE || textBlock.state == TEXT_STATE_CLOSING) {
        return 0;
    }
    return 1;
}

/* Steps textBlock.state: 2 counts tick up to 4 and then becomes 1; 1 waits until textOptionFlags bits 0x60 are set,
 * then becomes 3 with tick 4; 3 counts tick down to 0 and then becomes 0. */
void textBlockUpdate(void)
{
    switch (textBlock.state) {
    case TEXT_STATE_OPENING:
        textBlock.tick++;
        if (textBlock.tick >= BOX_OPEN_STEPS) {
            textBlock.tick = 0;
            textBlock.state = TEXT_STATE_OPEN;
        }
        break;
    case TEXT_STATE_CLOSING:
        textBlock.tick--;
        if (textBlock.tick == 0) {
            textBlock.state = TEXT_STATE_FREE;
        }
        break;
    case TEXT_STATE_OPEN:
        if ((textOptionFlags[0] & 0x60) != 0) {
            textBlock.state = TEXT_STATE_CLOSING;
            textBlock.tick = BOX_OPEN_STEPS;
        }
        break;
    }
}

void choiceMenuUpdate(int index);
void xglTickIncrement(void);

/* Advances the text window, the text entries, the choice menus and the text block, then queues the text draw. */
void textUpdate(void)
{
    int i;

    if (textWindow.fade > 0) {
        fontTrackEase(&textWindow.track);
        textWindow.fade--;
    }
    for (i = 0; i < TEXT_ENTRY_POOL_SIZE; i++) {
        if (textEntryPool[i].state != TEXT_STATE_FREE) {
            textEntryUpdate(&textEntryPool[i]);
        }
    }
    for (i = 0; i < CHOICE_MENU_SLOTS; i++) {
        if (choiceMenuSlots[i].state != TEXT_STATE_FREE) {
            choiceMenuUpdate(i);
        }
    }
    textBlockUpdate();
    xglCallbackQueueAdd((int)textDrawAll);
    xglTickIncrement();
}
