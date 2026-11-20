#include "common.h"
#include "ov01/data_record_get.h"
#include "main/battle_result.h"
#include "main/text_marker.h"
#include "main/choice_menu.h"
#include "main/gr_screen.h"
#include "ov01/gr_gp_init.h"
#include "ov01/menu_panel.h"
#include "ov01/menu_tex_state.h"
#include "main/xgl_cd_arena.h"

extern unsigned char textMarkerFound;

/* The ANALYZE panel: its position and open state, then the stats of the analysed enemy. */
typedef struct AnalyzePanel {
    int x;
    int y;
    unsigned char tick;
    unsigned char state;
    unsigned char unmodeled_0a[2];
    AnalyzeStats stats;    /* +0x0C */
} AnalyzePanel;

/* The ANALYZE panel block; the 52-byte struct is followed by padding up to the pointer table. */
static AnalyzePanel textBlock;
/* Page-marker positions recorded by textMarkerScan (16 entries). */
static unsigned char *textMarkerPtrs[16];

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


#include "main/pad.h"
#include "main/font_obj.h"
#include "main/menu_draw.h"
#include "main/pt_member.h"
#include "main/text_window.h"
#include "main/xgl_font.h"
#include "main/xgl_tick.h"

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


extern TextWindow textWindow;
extern TextEntry textEntryPool[TEXT_ENTRY_POOL_SIZE];
extern ChoiceMenu choiceMenuSlots[CHOICE_MENU_SLOTS];

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

/* A pair of words per control code, read by the draw calls of codes 9..15 and 16..29. */
typedef struct TextDrawPair {
    int first;
    int second;
} TextDrawPair;

TextDrawPair textCodeUv9[7] = { { .first = 440, .second = 130 }, { .first = 464, .second = 130 }, { .first = 488, .second = 130 }, { .first = 440, .second = 172 }, { .first = 464, .second = 172 }, { .first = 488, .second = 172 }, { .first = 440, .second = 214 } };
TextDrawPair textCodeUv16[15] = { { .first = 632, .second = 172 }, { .first = 668, .second = 172 }, { .first = 668, .second = 155 }, { .first = 704, .second = 155 }, { .first = 632, .second = 104 }, { .first = 668, .second = 104 }, { .first = 704, .second = 104 }, { .first = 632, .second = 121 }, { .first = 668, .second = 121 }, { .first = 704, .second = 121 }, { .first = 632, .second = 138 }, { .first = 668, .second = 138 }, { .first = 704, .second = 138 }, { .first = 632, .second = 155 }, { .first = 0, .second = 0 } };

/* Draws one inline control code of a text line at (x, y) and returns how far the line advances. */
int textDrawControlCode(int x, int y, int code)
{
    GrPacket packet;
    int advance;

    advance = 0;
    menuTexFlush();
    grGpInit(&packet);
    switch (code) {
    case 1:
        advance = 20;
        keyGideDisp(&packet, x + 1, y + 2, 1, 0);
        break;
    case 2:
        advance = 20;
        keyGideDisp(&packet, x + 1, y + 2, 2, 0);
        break;
    case 3:
        advance = 20;
        keyGideDisp(&packet, x + 1, y + 2, 0, 0);
        break;
    case 4:
        advance = 20;
        keyGideDisp(&packet, x + 1, y + 2, 3, 0);
        break;
    case 5:
        advance = 29;
        menuStepIconTexState(&packet);
        grOpenSpr(&packet);
        menuStatSprite(&packet, x + 1, y + 2, 27, 17, 464, 234);
        grCloseSpr();
        break;
    case 6:
        advance = 26;
        menuKeyDraw(&packet, x + 1, y - 2, 52, 1);
        if (textLineHeight < TEXT_LINE_HEIGHT_ICON) {
            textLineHeight = TEXT_LINE_HEIGHT_ICON;
        }
        break;
    case 7:
        advance = 26;
        menuKeyDraw(&packet, x + 1, y - 2, 53, 1);
        if (textLineHeight < TEXT_LINE_HEIGHT_ICON) {
            textLineHeight = TEXT_LINE_HEIGHT_ICON;
        }
        break;
    case 8:
        advance = 26;
        menuKeyDraw(&packet, x + 1, y - 2, 16, 1);
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
        menuStepIconTexState(&packet);
        grOpenSpr(&packet);
        menuStatSprite(&packet, x + 1, y - 9, 24, 42, textCodeUv9[code - 9].first, textCodeUv9[code - 9].second);
        grCloseSpr();
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
        menuStatusIconsTexState(&packet);
        grOpenSpr(&packet);
        menuStatSprite(&packet, x + 1, y + 1, 36, 17, textCodeUv16[code - 16].first, textCodeUv16[code - 16].second);
        grCloseSpr();
        break;
    }
    grPacketSend(&packet);
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

const char textBoxInitText[48] = "$init;$bg2;$shadow+1,+3,000000;$han1;$prop1;";
const char choiceMenuInitText[64] = "$init;$bg2;$shadow+1,+3,000000;$han1;$prop1;$xx3,0;$colc0c0c0;";
const char choiceValueZoomText[16] = "$zoom8,16;";
const char choiceItemZoomText[16] = "$zoom16;";
const char analyzeLabelColorText[16] = "$colc0c0c0;";
const char analyzeValueZoomText[24] = "$prop1;$zoom12,16;";
extern char choiceValueFormat[];
extern char fontPropOffText[];
extern char fontPropOnText[];
extern char choiceValueUnitText[];
extern char analyzeTitleText[];
extern char analyzeHpLabelText[];
extern char analyzeWeakAttackLabelText[];
extern char analyzeWeakAttributeLabelText[];
extern char analyzeExpLabelText[];
extern char analyzeSkillPointLabelText[];
extern char analyzeClassPointLabelText[];
extern char analyzeRareItemLabelText[];
extern char analyzeHpFormat[];
extern char analyzeUnknownText[];
extern char analyzeSeparatorText[];
extern char analyzeNumberFormat[];
extern char analyzeNoneText[];
/* sprintf-like: formats one integer value into buffer. */
void sprintf(char *buffer, const char *format, int value);
/* ov01: name of an item by kind (kind 2 and the others). */

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
        menuDrawCell(0x2BE, 0x200, 0x1C0, 0x10000);
    }
    if (textWindow.fade > 0) {
        fontObjResetState();
        fontObjSetAlpha(0x40);
        if (textWindow.fade < 8) {
            fontObjSetOpacity(textWindow.fade << 4);
        }
        menuDrawCellSized(0x2BE, textWindow.track.current_pair.x, textWindow.track.current_pair.y, textWindow.width, 0x20, 0x10104);
        if (textWindow.text) {
            textDrawSegment(0, 0, textBoxInitText, 0);
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
            textDrawSegment(0, 0, textBoxInitText, 0);
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
            textDrawSegment(0, 0, choiceMenuInitText, 0);
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
                            sprintf(buffer, choiceValueFormat, menu->values[i]);
                            textDrawSegment(0, 0, fontPropOffText, 0);
                            textDrawSegment(0, 0, choiceValueZoomText, 0);
                            textDrawSegment(valueX, y - 2, buffer, style);
                            textDrawSegment(0, 0, fontPropOnText, 0);
                            textDrawSegment(0, 0, choiceItemZoomText, 0);
                            if (menu->flags & CHOICE_VALUE_UNIT) {
                                textDrawSegment(valueX + 28, y, choiceValueUnitText, style);
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
    menuDrawTitledWindow(x, y, width, height, analyzeTitleText);
    fontObjResetOpacity();
    grSetScissor(x + 5, y + 5, x + width + 26, y + height + 25);
    x = panel->x;
    y = panel->y;
    textDrawSegment(0, 0, textBoxInitText, 0);
    textDrawSegment(0, 0, analyzeLabelColorText, 0);
    textDrawSegment(x + 16, y + 16, analyzeHpLabelText, 0);
    textDrawSegment(x + 16, y + 40, analyzeWeakAttackLabelText, 0);
    textDrawSegment(x + 16, y + 64, analyzeWeakAttributeLabelText, 0);
    textDrawSegment(x + 16, y + 92, analyzeExpLabelText, 0);
    textDrawSegment(x + 16, y + 116, analyzeSkillPointLabelText, 0);
    textDrawSegment(x + 16, y + 140, analyzeClassPointLabelText, 0);
    textDrawSegment(x + 16, y + 164, analyzeRareItemLabelText, 0);
    textDrawSegment(x + 16, y + 188, itemLabelText, 0);
    textDrawSegment(0, 0, analyzeValueZoomText, 0);

    if (panel->stats.hp >= 0) {
        sprintf(buffer, analyzeHpFormat, panel->stats.hp);
        textDrawSegment(x + 80, y + 16, buffer, 0);
    } else {
        textDrawSegment(x + 80, y + 16, analyzeUnknownText, 0);
    }
    xglFontPrintDirectOT(FONT_COLOR_DEFAULT, analyzeSeparatorText);
    if (panel->stats.maxHp >= 0) {
        sprintf(buffer, analyzeNumberFormat, panel->stats.maxHp);
        xglFontPrintDirectOT(FONT_COLOR_DEFAULT, buffer);
    } else {
        xglFontPrintDirectOT(FONT_COLOR_DEFAULT, analyzeUnknownText);
    }

    if (panel->stats.attackType != 0) {
        textDrawSegment(x + 88, y + 40, menuMainTxdEntry(panel->stats.attackType + 0x151), 0);
    }
    textDrawSegment(x + 88, y + 64, emptyLabelText, 0);
    for (i = 0; i < 8; i++) {
        if ((panel->stats.attributes >> i) & 1) {
            if (needSeparator == 1) {
                xglFontPrintDirectOT(FONT_COLOR_DEFAULT, analyzeSeparatorText);
            }
            needSeparator = 1;
            xglFontPrintDirectOT(FONT_COLOR_DEFAULT, menuMainTxdEntry(i + 0xF0));
        }
    }

    sprintf(buffer, analyzeNumberFormat, panel->stats.exp);
    textDrawSegment(x + 88, y + 92, buffer, 0);
    sprintf(buffer, analyzeNumberFormat, panel->stats.skillPoints);
    textDrawSegment(x + 88, y + 116, buffer, 0);
    sprintf(buffer, analyzeNumberFormat, panel->stats.classPoints);
    textDrawSegment(x + 88, y + 140, buffer, 0);

    if (panel->stats.rareItem > 0) {
        if (panel->stats.rareItemKind == 2) {
            textDrawSegment(x + 88, y + 164, itemTextGet(panel->stats.rareItem, 0), 0);
        } else {
            textDrawSegment(x + 88, y + 164, hitActionTextGet(panel->stats.rareItem, 0), 0);
        }
    } else if (panel->stats.rareItem < 0) {
        textDrawSegment(x + 88, y + 164, analyzeUnknownText, 0);
    } else {
        textDrawSegment(x + 88, y + 164, analyzeNoneText, 0);
    }
    if (panel->stats.item > 0) {
        if (panel->stats.itemKind == 2) {
            textDrawSegment(x + 88, y + 188, itemTextGet(panel->stats.item, 0), 0);
        } else {
            textDrawSegment(x + 88, y + 188, hitActionTextGet(panel->stats.item, 0), 0);
        }
    } else if (panel->stats.item < 0) {
        textDrawSegment(x + 88, y + 188, analyzeUnknownText, 0);
    } else {
        textDrawSegment(x + 88, y + 188, analyzeNoneText, 0);
    }
    xglFontFlush();
    grResetScissor();
}

/* Clears the tick (+8) and the state byte (+9) of the text block. */
void analyzePanelClear(void) {
    textBlock.state = TEXT_STATE_FREE;
    textBlock.tick = 0;
}

/* Starts the text block at (x, y): copies the analysis stats and opens it. */
void analyzePanelStart(int x, int y, const AnalyzeStats *stats)
{
    if (stats == 0) {
        return;
    }
    textBlock.x = x;
    textBlock.y = y;
    __builtin_memcpy(&textBlock.stats, stats, sizeof(AnalyzeStats));
    textBlock.state = TEXT_STATE_OPENING;
    textBlock.tick = 0;
}

/* Nonzero while the ANALYZE block is open or opening: states 0 and 3 are inactive. */
int analyzePanelIsActive(void)
{
    if (textBlock.state == TEXT_STATE_FREE || textBlock.state == TEXT_STATE_CLOSING) {
        return 0;
    }
    return 1;
}

/* Steps textBlock.state: 2 counts tick up to 4 and then becomes 1; 1 waits until PadData[0].pressed bits 0x60 are set,
 * then becomes 3 with tick 4; 3 counts tick down to 0 and then becomes 0. */
void analyzePanelUpdate(void)
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
        if ((PadData[0].pressed & 0x60) != 0) {
            textBlock.state = TEXT_STATE_CLOSING;
            textBlock.tick = BOX_OPEN_STEPS;
        }
        break;
    }
}


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
    analyzePanelUpdate();
    xglCallbackQueueAdd(textDrawAll);
    xglTickIncrement();
}

char itemLabelText[8] = "ITEM";
char etherLabelText[8] = "ETHER";
int xglFontZoom = TEXT_FONT_ZOOM;
unsigned char textMarkerFound = 0;
/* Two page-marker bytes and a zero, then textLineHeight (the byte at +3, not 8-aligned). */
unsigned char pageMarkerText[3] = { PAGE_MARKER_LEAD, PAGE_MARKER_TRAIL, 0x00 };
unsigned char textLineHeight = TEXT_LINE_HEIGHT_DEFAULT;

/* .sdata strings of textDrawAll (choiceValueFormat..analyzeNoneText); the leading items stay scaffold. */
char choiceValueFormat[8] = "%3d";
char fontPropOffText[8] = "$prop0;";
char fontPropOnText[8] = "$prop1;";
/* JP: ％
 * EN: % (the full-width percent sign) */
char choiceValueUnitText[8] = "\241\363";
char analyzeTitleText[8] = "ANALYZE";
char analyzeHpLabelText[8] = "HP";
char analyzeWeakAttackLabelText[8] = "W.ATK.";
char analyzeWeakAttributeLabelText[8] = "W.ATR.";
char analyzeExpLabelText[8] = "EXP";
char analyzeSkillPointLabelText[8] = "S.Pt";
char analyzeClassPointLabelText[8] = "C.Pt";
char analyzeRareItemLabelText[8] = "RARE";
char analyzeHpFormat[8] = "%6d";
char analyzeUnknownText[8] = "\077\077\077\077";
char analyzeSeparatorText[8] = "/";
char analyzeNumberFormat[8] = "%d";
char analyzeNoneText[8] = "----";
