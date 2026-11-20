#include "common.h"
#include "main/xgl_packet_scratch.h"
#include "main/talk_script.h"
#include "main/gs_make_tex0.h"
#include "main/debt_box.h"
#include "main/effect_request.h"
#include "main/timecode_counter.h"
#include "main/msg_slot_center.h"
#include "main/xgl_font.h"
#include "gs.h"
#include "main/xgl_font_width.h"
#include "main/xgl_packet.h"

/* Text layout constants shared by the sell, list and talk-box drawers. */
#define DEBT_CAPTION_WIDTH 0xA5     /* width of the "remaining debt" caption; the digits start after it */
#define DEBT_DIGIT_PITCH 15         /* px between the amount's text units */
#define LIST_ROW_HEIGHT 0x1E        /* height of one list row */
#define LIST_FIRST_ROW_OFFSET 8     /* first row below the layout's first row y */
#define LIST_TEXT_INSET 0x20        /* x of the row text inside the box */
#define LIST_COLOR_NORMAL 0xC0C0C0
#define LIST_COLOR_SELECTED 0x50509C
#define TEXT_ORDER_BIAS 7           /* added to the box order value for its text draws */
#define FONT_ZOOM_DEFAULT 0x12
#define FONT_ZOOM_BUTTON 0xE
#define FONT_COLOR_HEADING 0x73960A /* colour of the shadowed heading text */
#define FONT_COLOR_ENABLED 0x828282 /* Sell/Close label colour when active */
#define FONT_COLOR_DISABLED 0x4B4B4B
#define FONT_COLOR_ACCENT_SHADOW 0x202050
#define FONT_COLOR_WHITE 0xFFFFFF
#define FONT_Z_FRONT 0x7FFFFFFF     /* largest z: drawn in front */
#define SELL_BUTTON_Y 0x15B         /* y of the Sell and Close labels */
#define SELL_BUTTON_SELL_X 0xB9
#define SELL_BUTTON_CLOSE_X 0x11D
#define SELL_PANEL_DEBT_Y_ITEMS 0x25 /* debt line y (relative to the box) when the item grid is shown */
#define SELL_PANEL_DEBT_Y_EMPTY 0x57 /* debt line y when the list is empty */
#define SELL_EMPTY_NOTICE_X 0x7A
#define SELL_EMPTY_NOTICE_Y 0x3B
#define SELL_ROW_NAME_X 10          /* column offsets of a shop row, relative to its x */
#define SELL_ROW_PRICE_X 0xBE
#define SELL_ROW_COUNT_X 0x122
#define SELL_LIST_SLOTS 40          /* item slots of a sell list */
#define ASCII_ZERO '0'
#define TEXT_LINE_HEIGHT 30         /* height of one text line in textBoxMeasure */
#define TEXT_BOX_PAD_Y 2
#define TEXT_BOX_PAD_X 0x20
#define TEXT_ROWS_MARGIN 0x20       /* inset of the rows from the layout edge */
#define SELL_ROWS_MARGIN 0x48       /* inset of the sell list rows (room for the header) */
#define ACQUIRED_TEXT_INSET 0x20    /* offset of the acquired-item text inside its block */
#define ACQUIRED_COUNT_MIN_SHOWN 2  /* the count suffix is printed from this count on */
#define FRONT_TEXT_Y_OFFSET 8
#define FONT_INT_ROW_COUNT 10       /* decimal strings printed by fontPrintIntRow */
#define FONT_INT_ROW_Y_OFFSET 8
#define SCISSOR_TALK_TOP 0x21       /* talk box scissor: inset from the box top and bottom */
#define SCISSOR_TALK_BOTTOM 0x20
#define SCISSOR_SELL_TOP 0x4B       /* sell box scissor inset */
#define SCISSOR_SELL_BOTTOM 0x1E
#define ENTRY_SEP_FIRST_X 0x22      /* x offsets of the two separator marks of an entry */
#define ENTRY_SEP_SECOND_X 0x52
#define ENTRY_X_INSET 6             /* x/y of the entry digits inside the box (kinds 1 and 2) */
#define ENTRY_Y_INSET 5
#define DIGIT_ROW_Y_INSET 0x20      /* y of the digit row inside the box (kind 0) */
#define FADE_IN_END 8               /* phases 0-7 fade in, 8-11 hold, 12-19 fade out */
#define FADE_HOLD_END 12
#define FADE_OUT_END 20
#define FADE_STEP 0.125f            /* blend fraction per phase step */

/* The scratchpad area where the box drawers build their formatted text. */
#define SCRATCH_TEXT ((char *) SCRATCHPAD_BASE)

int sprintf(char *dst, const char *format, ...);
void *memset(void *s, int c, unsigned int n);
void *memcpy(void *destination, const void *source, unsigned int size);
char *strchr(const char *s, int c);
int listSlotFirstRowY(const MsgSlot *slot);
int pixelsToRows(int value);
void sellBoxScissorCallback(FontDrawCursor *cursor, void *argument);

/* Returns the x that centres the digits of value (15 px each) inside the box. */
int debtBoxCenterX(MsgSlot *box, int value) {
    int text_w = DEBT_CAPTION_WIDTH;

    do {
        text_w += DEBT_DIGIT_PITCH;
        value = value * 0.1;
    } while ((value < 0 ? -value : value) > 0);
    text_w += DEBT_DIGIT_PITCH;
    return box->x + (box->extent_x >> 1) - (text_w >> 1);
}

extern unsigned char fmtThreeStrings[];
extern const unsigned char fontStylePrefix[];
extern unsigned char fontPropTag[];
extern const unsigned char sellRemainingDebtText[];
extern unsigned char sellMoneyFormat[];

/* Prints the "remaining debt" caption, then the amount one text unit at a time, 15 px apart. */
void drawRemainingDebt(int x, int y, int color, int amount) {
    char amount_text[0x20];
    char unit[3];
    int unit_len;
    const char *p;
    int digit_x;

    memset(amount_text, 0, sizeof(amount_text));
    if (sellLabelTextOff == 0) {
        sprintf(SCRATCH_TEXT, fmtThreeStrings, fontStylePrefix, fontPropTag, sellRemainingDebtText);
    }
    xglFontPrint(x, y, color, SCRATCH_TEXT);
    digit_x = x + DEBT_CAPTION_WIDTH;
    memset(unit, 0, sizeof(unit));
    sprintf(amount_text, sellMoneyFormat, amount);
    p = amount_text;
    for (;;) {
        memset(unit, 0, sizeof(unit));
        p = xglFontCopyTextUnit(unit, p, &unit_len);
        if (unit_len <= 0) {
            break;
        }
        xglFontPrint(digit_x, y, color, unit);
        digit_x += DEBT_DIGIT_PITCH;
    }
}

u8 *fontColorTag(int color);
u8 *fontZoomTag(int zoom);
extern const unsigned char fmtSevenStrings[];
extern const unsigned char fontShadowTagA[];
extern const unsigned char fontShadowTagB[];

/* Draws the string text with a colour tag, a zoom tag and two shadow tags. */
void drawShadowedText(int x, int y, int z, unsigned int *text) {
    int text_y;
    char *color_tag;
    char *zoom_tag;

    text_y = y + 10;
    color_tag = fontColorTag(FONT_COLOR_HEADING);
    zoom_tag = fontZoomTag(FONT_ZOOM_DEFAULT);
    sprintf(SCRATCH_TEXT, fmtSevenStrings, fontStylePrefix, color_tag, zoom_tag, fontPropTag, fontShadowTagA, text, fontShadowTagB);
    xglFontPrint(x, text_y, z, SCRATCH_TEXT);
}

extern unsigned char fontXOffsetFmt[];
extern unsigned char fmtDecimal[];

/* Prints fmt through the font printf with ten decimal strings (one per entry of values) as its arguments, 8 px below y. */
void fontPrintIntRow(int x, int y, int color, char *fmt, int *values) {
    char text[FONT_INT_ROW_COUNT][15];
    unsigned long long args[FONT_INT_ROW_COUNT + 1];
    int i;

    memset(args, 0, sizeof(args));
    args[0] = (unsigned int) fmt;
    for (i = 0; i < FONT_INT_ROW_COUNT; i++) {
        sprintf(text[i], fmtDecimal, values[i]);
        args[i + 1] = (unsigned int) text[i];
    }
    xglFontPrintf(x, y + FONT_INT_ROW_Y_OFFSET, color, args);
    xglFontPrint(0, 0, 0, fontXOffsetFmt);
}

/* Counts the lines of the entry text (a line ends at '\n' or at the terminator). */
int textLineCount(MsgSlot *entry) {
    char *p;
    char c;
    int count;

    p = entry->u.text.start;
    count = 0;
    while (*p != 0) {
        count++;
        while ((c = *p) != 0 && c != '\n') {
            p++;
        }
        if (*p == '\n') {
            p++;
        }
    }
    return count;
}

/* Returns the start of line `lines` of text (or its end when the text has fewer lines). */
const char *textSkipLines(const char *text, int lines) {
    int count;

    count = 0;
    while (count < lines && *text != 0) {
        count++;
        while (*text != 0 && *text != '\n') {
            text++;
        }
        if (count >= lines) {
            break;
        }
        if (*text == '\n') {
            text++;
        }
    }
    return text;
}

/* Counts the lines of text that start before end (a line ends at '\n' or at the terminator). */
int textCountLinesBefore(const char *text, const char *end) {
    int count = 0;

    while (*text != 0 && text < end) {
        count++;
        while (*text != 0 && *text != '\n' && text < end) {
            text++;
        }
        if (*text == '\n') {
            text++;
        }
    }
    return count;
}

/* Measures the text from text up to end: *width gets the pixel width of the last line (plus TEXT_BOX_PAD_X), *height the height of all lines (TEXT_LINE_HEIGHT per line + TEXT_BOX_PAD_Y). */
void textBoxMeasure(int *width, int *height, const char *text, int *font, const char *end) {
    char line[0xDC];
    const char *line_start;
    int line_count;

    xglFontWidthBegin(font);
    line_count = 1;
    line_start = text;
    if (*text != 0 && text < end) {
        do {
            if (*text == '\n') {
                line_start = text;
                line_count++;
                xglFontWidthReset();
            }
            if (textIsValueEscape(text++)) {
                xglFontWidthAddValue();
            }
        } while (*text != 0 && text < end);
    }
    *height = line_count * TEXT_LINE_HEIGHT + TEXT_BOX_PAD_Y;
    memset(line, 0, sizeof(line));
    line[0] = 0;
    strcat(line, fontStylePrefix);
    strcat(line, fontPropTag);
    strncat(line, line_start, end - line_start + 1);
    *width = xglFontGetStringWidth(line) + xglFontWidthGetSum() + TEXT_BOX_PAD_X;
}

/* Returns the text after a leading "[...]" tag, or text itself when it does not start with '['. */
const char *textSkipBracketTag(const char *text)
{
    const char *p = text;
    char open = '[';

    if (*p != open) {
        return p;
    }
    {
        char close = ']';

        do {
            p++;
        } while (*p != close);
    }
    return p + 1;
}

/* Returns the text after an opening bracket '[' (the tag body), or 0 when text does not start with one. */
const char *textTagBody(const char *text)
{
    if (*text == '[') {
        return text + 1;
    }
    return 0;
}

const char *appendColoredRun(char *dst, const char *src, int color);
void sellFormatRow(char *dst, int x, char *name, int price, int count, int color);
void textCopyChars(unsigned char *dest, const unsigned char *src, int count);

/* Draws the box's text one line per row, the selected line in the highlight colour. */
void msgWindowDrawLineList(MsgSlot *box) {
    char line[0x70];
    int index;
    int row_y;
    int line_y;
    int selected;
    const char *text;

    index = 0;
    text = box->u.list.text;
    row_y = listSlotFirstRowY(box);
    if (text == 0) {
        return;
    }
    xglFontPrintExtFunc(box->z + 1, talkBoxScissorCallback, box);
    selected = pixelsToRows(box->u.list.sweep_value);
    xglFontPrint(0, 0, 0, fontStylePrefix);
    xglFontPrint(0, 0, 0, fontPropTag);
    if (*text != 0) {
        line_y = row_y + LIST_FIRST_ROW_OFFSET;
        do {
            if (index != selected) {
                text = appendColoredRun(line, text, LIST_COLOR_NORMAL);
            } else {
                text = appendColoredRun(line, text, LIST_COLOR_SELECTED);
            }
            xglFontPrint(box->x + LIST_TEXT_INSET, line_y, box->z + TEXT_ORDER_BIAS, line);
            line_y += LIST_ROW_HEIGHT;
            index++;
            if (*text == 0) {
                break;
            }
            text = textSkipLineBreak(text);
        } while (*text != 0);
    }
    xglFontPrintExtFunc(0, talkBoxScissorCallback, (void *) 1);
    xglFontFlush();
}

void drawSellCloseButtons(MsgSlot *widget);
void drawSellItemList(MsgSlot *list);
void sellDrawEmptyNotice(MsgSlot *pos);

/* Draws one sell slot: the action labels, the remaining debt, then the item grid when flag is set or the empty notice. */
void drawSellPanel(MsgSlot *widget) {
    MsgSlotList *state;
    int *value_ref;
    int debt_x;

    state = &widget->u.list;
    value_ref = state->debt;
    xglFontPrintExtFunc(0, sellBoxScissorCallback, (void *) 1);
    if (state->item_count != 0) {
        drawSellCloseButtons(widget);
        if (value_ref != 0) {
            debt_x = debtBoxCenterX(widget, *value_ref);
            drawRemainingDebt(debt_x, widget->y + SELL_PANEL_DEBT_Y_ITEMS, widget->z + TEXT_ORDER_BIAS, *value_ref);
        }
        xglFontFlush();
        drawSellItemList(widget);
        return;
    }
    drawSellCloseButtons(widget);
    if (value_ref != 0) {
        debt_x = debtBoxCenterX(widget, *value_ref);
        drawRemainingDebt(debt_x, widget->y + SELL_PANEL_DEBT_Y_EMPTY, widget->z + TEXT_ORDER_BIAS, *value_ref);
    }
    sellDrawEmptyNotice(widget);
    xglFontFlush();
}

/* Scales an integer by a float constant and rounds to nearest. */
int pixelsToRows(int value) {
    return (float) value * 0.0333f + 0.5;
}

int sellItemFirstRowY(const MsgSlot *slot);

/* Draws the shop rows (name, price, count) of the sell list, highlighting the row under the cursor. */
void drawSellItemList(MsgSlot *list) {
    MsgSlotList *state = &list->u.list;
    int row_y = sellItemFirstRowY(list);
    char **names = state->list_source;
    int *prices = state->item_costs;
    int *counts = state->counts;
    char row_text[0x70];
    int selected;
    int i;

    if (names != 0 && prices != 0 && counts != 0) {
        xglFontPrintExtFunc(list->z + 1, sellBoxScissorCallback, list);
        selected = sellCursorFilledRank(state->sweep_value, counts);
        xglFontPrint(0, 0, 0, fontStylePrefix);
        row_y += LIST_FIRST_ROW_OFFSET;
        xglFontPrint(0, 0, 0, fontPropTag);
        for (i = 0; i < SELL_LIST_SLOTS; i++) {
            if (names[i] != 0 && counts[i] != 0) {
                sellFormatRow(row_text, list->x + LIST_TEXT_INSET, names[i], prices[i], counts[i], i != selected ? LIST_COLOR_NORMAL : LIST_COLOR_SELECTED);
                xglFontPrint(0, row_y, list->z + TEXT_ORDER_BIAS, row_text);
                row_y += LIST_ROW_HEIGHT;
            }
        }
        xglFontFlush();
        xglFontPrintExtFunc(0, sellBoxScissorCallback, (void *) 1);
    }
}

/* Returns how many of the SELL_LIST_SLOTS slots are walked before the rounded scaled value runs out of filled slots. */
int sellCursorFilledRank(int value, int *slots) {
    int *end = slots + SELL_LIST_SLOTS;
    int remaining = (float) value * 0.0333f + 0.5;
    int count;

    for (count = 0; slots < end; count++, slots++) {
        if (*slots != 0 && --remaining < 0) {
            break;
        }
    }
    return count;
}

extern unsigned char fmtString[];
extern unsigned char sellActionSell[];
extern unsigned char sellActionClose[];

/* Draws the Sell and Close action labels at y SELL_BUTTON_Y; the Sell colour is bright when flag is set. */
void drawSellCloseButtons(MsgSlot *widget) {
    MsgSlotList *state;
    int line_y;
    char *zoom_tag;
    char *color_tag;
    char *text_end;

    state = &widget->u.list;
    line_y = widget->z + TEXT_ORDER_BIAS;
    sprintf(SCRATCH_TEXT, fmtString, fontStylePrefix);
    zoom_tag = fontZoomTag(FONT_ZOOM_BUTTON);
    strcat(SCRATCH_TEXT, zoom_tag);
    strcat(SCRATCH_TEXT, fontPropTag);
    text_end = strchr(SCRATCH_TEXT, 0);
    if (state->item_count != 0) {
        color_tag = fontColorTag(FONT_COLOR_ENABLED);
        strcat(SCRATCH_TEXT, color_tag);
    } else {
        color_tag = fontColorTag(FONT_COLOR_DISABLED);
        strcat(SCRATCH_TEXT, color_tag);
    }
    if (sellLabelTextOff == 0) {
        strcat(SCRATCH_TEXT, sellActionSell);
    }
    xglFontPrint(SELL_BUTTON_SELL_X, SELL_BUTTON_Y, line_y, SCRATCH_TEXT);
    *text_end = '\0';
    color_tag = fontColorTag(FONT_COLOR_ENABLED);
    strcat(SCRATCH_TEXT, color_tag);
    if (sellLabelTextOff == 0) {
        strcat(SCRATCH_TEXT, sellActionClose);
    }
    xglFontPrint(SELL_BUTTON_CLOSE_X, SELL_BUTTON_Y, line_y, SCRATCH_TEXT);
}

int textRunLength(const char *text);

/* Appends the colour tag for `color` and the run of `src` up to its break to dst; returns the rest of src. */
const char *appendColoredRun(char *dst, const char *src, int color) {
    int run_len;
    char *end;
    u8 *color_tag;

    run_len = textRunLength(src);
    color_tag = fontColorTag(color);
    sprintf(dst, fmtString, color_tag);
    end = strchr(dst, '\0');
    memcpy(end, src, run_len);
    end[run_len] = '\0';
    return src + run_len;
}

/* Writes the style prefix, the $prop1 tag and the colour tag for `color` to dest, then the run of `text` up to its break. */
const char *appendStyledRun(char *dest, const char *text, int color) {
    int run_len;
    char *color_tag;
    char *end;

    run_len = textRunLength(text);
    color_tag = fontColorTag(color);
    sprintf(dest, fmtThreeStrings, fontStylePrefix, fontPropTag, color_tag);
    end = strchr(dest, 0);
    memcpy(end, text, run_len);
    end[run_len] = '\0';
    return text + run_len;
}

extern unsigned char sellCurrencySuffix[];
void appendXAdvanceTag(char *dst, int advance_x);
void appendNumberText(char *dst, int number);

/* Builds one shop row in dst: colour tag, name at x+10, price at x+0xBE, currency suffix, count at x+0x122. */
void sellFormatRow(char *dst, int x, char *name, int price, int count, int color) {
    int name_x = x + SELL_ROW_NAME_X;
    int price_x = x + SELL_ROW_PRICE_X;
    int count_x = x + SELL_ROW_COUNT_X;

    dst[0] = '\0';
    sprintf(dst, fmtString, fontColorTag(color));
    appendXAdvanceTag(dst, name_x);
    strcat(dst, name);
    appendXAdvanceTag(dst, price_x);
    appendNumberText(dst, price);
    strcat(dst, (const char *) sellCurrencySuffix);
    appendXAdvanceTag(dst, count_x);
    appendNumberText(dst, count);
}

const char *textFindRunEnd(const char *text);

int textRunLength(const char *text)
{
    return textFindRunEnd(text) - text;
}

/* Returns the end of the run that starts at text: its first line break, or the NUL that ends it. */
const char *textFindRunEnd(const char *text)
{
    const char *end;

    end = text;
    while (!textIsLineBreak(end)) {
        if (*end == 0) {
            break;
        }
        end++;
    }
    return end;
}

extern const unsigned char sellNoItemsText[];

/* Draws the empty-list notice "No items to sell." at the position in pos (only when flag is zero). */
void sellDrawEmptyNotice(MsgSlot *pos) {
    u8 *color_tag;

    color_tag = fontColorTag(LIST_COLOR_SELECTED);
    sprintf(SCRATCH_TEXT, fmtThreeStrings, fontStylePrefix, fontPropTag, color_tag);
    if (sellLabelTextOff == 0) {
        strcat(SCRATCH_TEXT, (const char *) sellNoItemsText);
    }
    xglFontPrint(pos->x + SELL_EMPTY_NOTICE_X, pos->y + SELL_EMPTY_NOTICE_Y, pos->z, SCRATCH_TEXT);
}

/* Returns the width of the widest line of the entry's text (0 when it has no text). */
int textMaxLineWidth(MsgSlot *entry) {
    char line[0x70];
    const char *text;
    const char *p;
    int max_width;
    int width;

    max_width = 0;
    text = entry->u.list.text;
    if (text != 0) {
        p = text;
        while (*p != 0) {
            p = appendStyledRun(line, p, FONT_COLOR_WHITE);
            width = xglFontGetStringWidth(line);
            if (width > max_width) {
                max_width = width;
            }
            if (*p == 0) {
                break;
            }
            p = textSkipLineBreak(p);
        }
        return max_width;
    }
    return 0;
}

/* Counts the lines of a text: runs of characters separated by line breaks. */
int textCountLinesOf(const char *text) {
    const char *p;
    int count;

    p = text;
    count = 0;
    while (*p != 0) {
        count++;
        p = textFindRunEnd(p);
        if (*p == 0) {
            break;
        }
        p = textSkipLineBreak(p);
    }
    return count;
}

int sellCountFilledSlots(int *words) {
    int *end;
    int nonzero;
    int count;

    nonzero = 0;
    end = words + (SELL_LIST_SLOTS - 1);
    count = 0;
    if (words != 0) {
        if (end >= words) {
            do {
                if (*words != 0) {
                    nonzero = nonzero + 1;
                }
                words++;
            } while (end >= words);
        }
        count = nonzero;
    }
    return count;
}

int sellCountFilled(int *words, int count) {
    int *end;
    int nonzero;

    end = words + count;
    nonzero = 0;
    if (words < end) {
        do {
            if (*words != 0) {
                nonzero = nonzero + 1;
            }
            words++;
        } while (words < end);
    }
    return nonzero;
}

void talkBoxDrawDigitRow(MsgSlot *box, int x, int y);
void drawEntryDigits(MsgSlot *entry, int x, int y);

/* Draws the talk box contents for its mode (0: talkBoxDrawDigitRow, 1-2: digits), then switches the font to the scissor callback. */
void talkBoxDrawContents(MsgSlot *box) {
    MsgSlotTimecode *layout = &box->u.timecode;
    int x = talkBoxContentX(box);
    int y = talkBoxContentY(box);

    switch (layout->mode) {
    case TIMECODE_MODE_NUMBER:
        talkBoxDrawDigitRow(box, x, y);
        break;
    case TIMECODE_MODE_HMS:
    case TIMECODE_MODE_MSF:
        drawEntryDigits(box, x, y);
        break;
    }
    xglFontFlush();
    xglFontPrintExtFunc(box->z + 1, talkBoxScissorCallback, (void *) 1);
}

void talkBoxDrawDigit(int x, int y, int color, int digit, int variant);
void talkBoxDrawRollingDigit(int x, int y, int color, int slot, MsgSlot *menu);

#define TALK_SCROLL_DIVISOR 20.0f

/* Draws the digit row of a talk box; the animated or highlighted cell is drawn through the accent path. */
void talkBoxDrawDigitRow(MsgSlot *box, int x, int y) {
    MsgSlotTimecode *state = &box->u.timecode;
    int highlight = (float) state->cursor_x / TALK_SCROLL_DIVISOR + 0.5;
    int pos;
    int cell;
    int digit;

    xglFontPrintExtFunc(box->z, talkBoxScissorCallback, box);
    pos = state->digits;
    for (cell = 0; cell < state->digits; cell++) {
        digit = decimalDigitAt(state->elapsed_frames, pos);
        if (box->flags & TIMECODE_FLAG_RAMP) {
            if (state->selected == pos) {
                talkBoxDrawRollingDigit(x, y, box->z + TEXT_ORDER_BIAS, digit, box);
            } else {
                talkBoxDrawDigit(x, y, box->z + TEXT_ORDER_BIAS, digit, 0);
            }
        } else if (highlight == cell && (box->flags & TIMECODE_FLAG_PHASE)) {
            talkBoxDrawDigit(x, y, box->z + TEXT_ORDER_BIAS, digit, 1);
        } else {
            talkBoxDrawDigit(x, y, box->z + TEXT_ORDER_BIAS, digit, 0);
        }
        x += TIMECODE_PHASE_PIXELS;
        pos--;
    }
    xglFontPrintExtFunc(box->z, talkBoxScissorCallback, (void *) 1);
}

const unsigned char fontXOffsetTag[16] = "$xx0,20;";
const unsigned char fmtFourStrings[16] = "%s%s%s%s";
const unsigned char sellAcquiredText[16] = "Acquired ";
const unsigned char fontGreyTag[16] = "$col808080;";
const unsigned char fontHanCountFmt[16] = "$han1;%d";

void talkBoxPrintDigitPlain(int x, int y, int color, const char *text);
void talkBoxPrintDigitAccent(int x, int y, int color, const char *text);

/* Draws one decimal digit; variant selects the accent colour (talkBoxPrintDigitAccent) over the plain draw (talkBoxPrintDigitPlain). */
void talkBoxDrawDigit(int x, int y, int color, int digit, int variant) {
    char digit_str[2];
    char text_buf[16];

    digit_str[0] = (char) (digit + ASCII_ZERO);
    digit_str[1] = '\0';
    textFormatHanTag(&text_buf[0], &digit_str[0]);
    if (variant != 0) {
        talkBoxPrintDigitAccent(x, y, color, &text_buf[0]);
        return;
    }
    talkBoxPrintDigitPlain(x, y, color, &text_buf[0]);
}

extern const char *entryAccentGlyph;
float cosf(float x);

#define SLOT_SWING_STEP 20          /* px between two slots (TIMECODE_PHASE_PIXELS) */
#define SLOT_SWING_WRAP 200         /* 10 slots; the swing wraps by this */

/* Draws the accent label at a y that follows a cosine swing between two slots (20 px each, wrapping by 200). */
void talkBoxDrawRollingDigit(int x, int y, int color, int slot, MsgSlot *menu) {
    MsgSlotTimecode *anim = &menu->u.timecode;
    int from = slot * SLOT_SWING_STEP;
    int to = anim->digit * SLOT_SWING_STEP;
    float amplitude;
    int mid;

    if (anim->span > 0 && from < to) {
        from += SLOT_SWING_WRAP;
    } else if (anim->span < 0 && to < from) {
        to += SLOT_SWING_WRAP;
    }
    mid = (from + to) >> 1;
    amplitude = (float) ((from - to) >> 1);
    talkBoxPrintDigitAccent(x, y - mid + (int) (amplitude * cosf((float) menu->u.timecode.roll_angle * 0.01745328f)), color, entryAccentGlyph);
}

void formatEntryValue(char *buffer, MsgSlot *entry);
void drawEntryCells(int x, int y, int color, int *buffer, int tag_color);
int entryFadeColor(MsgSlot *entry);

#define ENTRY_DIGIT_ORDER 0x0FFFFFFF /* draw order of the digit cells */

/* Draws the entry's formatted value as digit cells at (x, y). */
void drawEntryDigits(MsgSlot *entry, int x, int y) {
    int digits[4];
    int digit_count;

    formatEntryValue((char *) &digits[0], entry);
    digit_count = entryFadeColor(entry);
    xglFontPrintExtFunc(entry->z + 1, talkBoxScissorCallback, (void *) 1);
    drawEntryCells(x, y, ENTRY_DIGIT_ORDER, &digits[0], digit_count);
}

void formatEntryDigits(char *buffer, MsgSlot *entry);
/* Font and format strings of the sell and talk boxes. */
const unsigned char fontStylePrefix[40] = "\x0b" "\x19" "\x02" "\x0d" "\x02" "$xx0,30;$rubyz23,21;$rubyy-2;";
const unsigned char sellRemainingDebtText[24] = "Remaining Debt: ";
const unsigned char fmtSevenStrings[16] = "%s%s%s%s%s%s%s";
const unsigned char fontShadowTagA[24] = "$shadow+2,+1,000000;";
const unsigned char fontShadowTagB[24] = "$shadow+1,+1,000000;";
const unsigned char sellNoItemsText[72] = "No items to sell.\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "$han1;0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n";
const unsigned char fmtSixDigits[16] = "%d%d%d%d%d%d";

int formatEntryHms(char *buffer, MsgSlot *entry);
int formatEntryMsf(char *buffer, MsgSlot *entry);

/* Formats the entry value into buffer according to the entry kind (0, 1 or 2). */
void formatEntryValue(char *buffer, MsgSlot *entry) {
    switch (entry->u.timecode.mode) {
    case TIMECODE_MODE_NUMBER:
        formatEntryDigits(buffer, entry);
        return;
    case TIMECODE_MODE_HMS:
        formatEntryHms(buffer, entry);
        return;
    case TIMECODE_MODE_MSF:
        formatEntryMsf(buffer, entry);
        break;
    }
}

/* Writes the entry value to buffer as digit_count decimal digits, with leading zeros and a terminator. */
void formatEntryDigits(char *buffer, MsgSlot *entry) {
    int digits[10];
    int i;
    int count;
    long long place;

    memset(digits, 0, sizeof(digits));
    count = entry->u.timecode.digits;
    place = 1;
    for (i = count - 1; i >= 0; i--) {
        if (place != 0) {
            digits[i] = (int) (entry->u.timecode.elapsed_frames / place) % 10;
        } else {
            digits[i] = 0;
        }
        place *= 10;
    }
    for (i = 0; i < count; i++) {
        buffer[i] = digits[i] + '0';
    }
    buffer[count] = '\0';
}

extern const unsigned char fmtSixDigits[];
extern unsigned char entryFrameDigitCorrection[];

/* Formats the entry's frame count (30 per second) as the six digits hh:mm:ss. */
int formatEntryHms(char *buffer, MsgSlot *entry) {
    char digit[6];
    int tens_h, tens_m, tens_s;
    int ones_h, ones_m, ones_s;
    float total, hours, minutes, seconds;

    total = entry->u.timecode.elapsed_frames;
    hours = (long long) total / FRAMES_PER_HOUR;
    total -= (long long) hours * FRAMES_PER_HOUR;
    minutes = (long long) total / FRAMES_PER_MINUTE;
    total -= (long long) minutes * FRAMES_PER_MINUTE;
    seconds = (long long) total / FRAMES_PER_SECOND;
    digit[0] = hours * 0.1;
    tens_h = digit[0];
    hours -= tens_h * 10;
    ones_h = hours;
    digit[1] = ones_h;
    digit[2] = minutes * 0.1;
    tens_m = digit[2];
    minutes -= tens_m * 10;
    ones_m = minutes;
    digit[3] = ones_m;
    digit[4] = seconds * 0.1;
    tens_s = digit[4];
    seconds -= tens_s * 10;
    ones_s = seconds;
    digit[5] = ones_s;
    sprintf(buffer, fmtSixDigits, tens_h, (char) ones_h, tens_m, (char) ones_m, tens_s, (char) ones_s);
}

/* Formats the entry's frame count (30 per second) as the six digits mm:ss:ff, ff being hundredths. */
int formatEntryMsf(char *buffer, MsgSlot *entry) {
    char digit[6];
    float total, hours, minutes, seconds, frames;

    total = entry->u.timecode.elapsed_frames;
    hours = (long long) total / FRAMES_PER_HOUR;
    total -= (long long) hours * FRAMES_PER_HOUR;
    minutes = (long long) total / FRAMES_PER_MINUTE;
    total -= (long long) minutes * FRAMES_PER_MINUTE;
    seconds = (long long) total / FRAMES_PER_SECOND;
    frames = (long long) ((total - (long long) seconds * FRAMES_PER_SECOND) * HUNDREDTHS_PER_FRAME);
    digit[0] = minutes * 0.1;
    minutes -= digit[0] * 10;
    digit[1] = minutes;
    digit[2] = seconds * 0.1;
    seconds -= digit[2] * 10;
    digit[3] = seconds;
    digit[4] = frames * 0.1;
    frames -= digit[4] * 10;
    digit[5] = (int) frames - entryFrameDigitCorrection[digit[4]];
    digit[5] = xglClampInt(digit[5], 0, 9);
    sprintf(buffer, fmtSixDigits, digit[0], digit[1], digit[2], digit[3], digit[4], digit[5]);
}

extern unsigned char fontHanTag[];
extern unsigned char entrySeparatorMark[];

#define ENTRY_CELL_SHORT 0x13
#define ENTRY_CELL_LONG 0x1D
#define ENTRY_CHAR_BUFFER ((unsigned char *) SCRATCHPAD_BASE)

/* Draws the six single-character cells of an entry: two separator marks, then each char of buffer at alternating 0x13/0x1D steps. */
void drawEntryCells(int x, int y, int color, int *buffer, int tag_color) {
    const unsigned char *chars = (const unsigned char *) buffer;

    xglFontPrint(0, 0, 0, fontStylePrefix);
    xglFontPrint(0, 0, 0, fontZoomTag(FONT_ZOOM_DEFAULT));
    xglFontPrint(0, 0, 0, fontColorTag(tag_color));
    xglFontPrint(0, 0, 0, fontHanTag);
    xglFontPrint(x + ENTRY_SEP_FIRST_X, y, color, entrySeparatorMark);
    xglFontPrint(x + ENTRY_SEP_SECOND_X, y, color, entrySeparatorMark);
    textCopyChars(ENTRY_CHAR_BUFFER, &chars[0], 1);
    xglFontPrint(x, y, color, ENTRY_CHAR_BUFFER);
    x += ENTRY_CELL_SHORT;
    textCopyChars(ENTRY_CHAR_BUFFER, &chars[1], 1);
    xglFontPrint(x, y, color, ENTRY_CHAR_BUFFER);
    x += ENTRY_CELL_LONG;
    textCopyChars(ENTRY_CHAR_BUFFER, &chars[2], 1);
    xglFontPrint(x, y, color, ENTRY_CHAR_BUFFER);
    x += ENTRY_CELL_SHORT;
    textCopyChars(ENTRY_CHAR_BUFFER, &chars[3], 1);
    xglFontPrint(x, y, color, ENTRY_CHAR_BUFFER);
    x += ENTRY_CELL_LONG;
    textCopyChars(ENTRY_CHAR_BUFFER, &chars[4], 1);
    xglFontPrint(x, y, color, ENTRY_CHAR_BUFFER);
    x += ENTRY_CELL_SHORT;
    textCopyChars(ENTRY_CHAR_BUFFER, &chars[5], 1);
    xglFontPrint(x, y, color, ENTRY_CHAR_BUFFER);
}

/* Colour of the entry: dimmed idle colour when the 0x800 flag is clear, else a fade of its colour with a half-blend, stepped by phase (0-7 in, 8-11 hold, 12-19 out, then the blend). */
int entryFadeColor(MsgSlot *entry) {
    MsgSlotTimecode *state = &entry->u.timecode;
    unsigned int color = state->highlight_color;
    int plain = state->highlight_color;
    int blended = xglColorLerp(0, color, 0.5f);
    int phase;

    if ((entry->flags & TIMECODE_FLAG_COUNTER) == 0) {
        return state->idle_color;
    }
    phase = state->blink_phase;
    if (phase < FADE_IN_END) {
        return xglColorLerp((unsigned int) blended, color, phase * FADE_STEP);
    }
    if (phase < FADE_HOLD_END) {
        return plain;
    }
    if (phase < FADE_OUT_END) {
        return xglColorLerp(color, (unsigned int) blended, (phase - FADE_HOLD_END) * FADE_STEP);
    }
    return blended;
}

/* X position of the box contents: centered on the width for the default mode, ENTRY_X_INSET for modes 1-2, else 0. */
int talkBoxContentX(MsgSlot *box)
{
    int result = 0;

    switch (box->u.timecode.mode) {
    case TIMECODE_MODE_NUMBER:
        result = box->x + (box->extent_x >> 1) - ((box->u.timecode.digits * TIMECODE_PHASE_PIXELS) >> 1);
        break;
    case TIMECODE_MODE_HMS:
    case TIMECODE_MODE_MSF:
        result = box->x + ENTRY_X_INSET;
        break;
    }
    return result;
}

/* Y position of the box contents for its mode: DIGIT_ROW_Y_INSET below y for the default, ENTRY_Y_INSET for modes 1-2, else 0. */
int talkBoxContentY(MsgSlot *box)
{
    int result = 0;

    switch (box->u.timecode.mode) {
    case TIMECODE_MODE_NUMBER:
        result = box->y + DIGIT_ROW_Y_INSET;
        break;
    case TIMECODE_MODE_HMS:
    case TIMECODE_MODE_MSF:
        result = box->y + ENTRY_Y_INSET;
        break;
    }
    return result;
}

#define FONT_SCISSOR_MAX_Y 0x1C0
#define FONT_SCISSOR_WIDTH 0x200

void talkBoxScissorCallback(FontDrawCursor *cursor, void *argument)
{
    MsgSlot *box = argument;
    /* A+D packet: SCISSOR_1 covers the full width and the rows of the box, clamped to the screen. */
    u64 *gif = cursor->work.qwords;
    unsigned long low;
    unsigned long high;
    int top;
    int bottom;

    if (box == (MsgSlot *) 1) {
        top = 0;
        bottom = FONT_SCISSOR_MAX_Y;
    } else {
        top = box->y + SCISSOR_TALK_TOP;
        bottom = box->y + box->extent_y - SCISSOR_TALK_BOTTOM;
        if (top < 0) top = 0;
        if (bottom > FONT_SCISSOR_MAX_Y) bottom = FONT_SCISSOR_MAX_Y;
    }
    low = ((unsigned long) top << 32) | (FONT_SCISSOR_WIDTH << 16);
    high = (unsigned long) bottom << 48;
    gif[0] = GIF_TAG_ONE_AD_EOP;
    gif[1] = GIF_AD;
    gif[2] = high | low;
    gif[3] = GS_SCISSOR_1;
    sceVif1PkAddDirectDataN(cursor->packet, gif, 2);
}

/* Same as talkBoxScissorCallback with the box body inset (SCISSOR_SELL_TOP, SCISSOR_SELL_BOTTOM). */
void sellBoxScissorCallback(FontDrawCursor *cursor, void *argument)
{
    MsgSlot *box = argument;
    u64 *gif = cursor->work.qwords;
    unsigned long low;
    unsigned long high;
    int top;
    int bottom;

    if (box == (MsgSlot *) 1) {
        top = 0;
        bottom = FONT_SCISSOR_MAX_Y;
    } else {
        top = box->y + SCISSOR_SELL_TOP;
        bottom = box->y + box->extent_y - SCISSOR_SELL_BOTTOM;
        if (top < 0) top = 0;
        if (bottom > FONT_SCISSOR_MAX_Y) bottom = FONT_SCISSOR_MAX_Y;
    }
    low = ((unsigned long) top << 32) | (FONT_SCISSOR_WIDTH << 16);
    high = (unsigned long) bottom << 48;
    gif[0] = GIF_TAG_ONE_AD_EOP;
    gif[1] = GIF_AD;
    gif[2] = high | low;
    gif[3] = GS_SCISSOR_1;
    sceVif1PkAddDirectDataN(cursor->packet, gif, 2);
}

/* Returns the decimal digit of value at 1-based position pos from the right (pos 1 = ones). */
int decimalDigitAt(long value, int pos)
{
    int divisor = 1;

    while (pos > 1) {
        divisor *= 10;
        pos--;
    }
    return value / divisor % 10;
}

int formatAcquiredText(char *buffer, char *name_text, u32 count);
void fontPrintFront(int x, int y, char *text);

/* Draws the acquired-item message for block: a background packet, the text, then the background off. */
void drawAcquiredText(MsgSlot *block) {
    char acquired_text[0x70];

    formatAcquiredText(&acquired_text[0], block->u.item.name, block->u.item.count);
    xglFontPrintExtFunc(block->z, talkBoxScissorCallback, block);
    fontPrintFront(block->x + ACQUIRED_TEXT_INSET, block->y + ACQUIRED_TEXT_INSET, &acquired_text[0]);
    xglFontFlush();
    xglFontPrintExtFunc(0, talkBoxScissorCallback, (void *) 1);
}

extern const unsigned char sellAcquiredText[];
extern const unsigned char fontGreyTag[];
extern const unsigned char fontHanCountFmt[];
extern unsigned char acquiredCountLead[];
extern unsigned char acquiredTextEnd[];
typedef struct StylePrefixCopy {
    char text[0x23];
} StylePrefixCopy;
/* Builds the acquired-item message in buffer: style prefix, name, an optional count suffix when count >= 2. */
int formatAcquiredText(char *buffer, char *name_text, u32 count) {
    char count_text[0x10];
    u8 *color_tag;

    /* Inlined strcpy(buffer, "...") of the 0x23-byte style-prefix literal; becomes a strcpy once the string is C-owned. */
    *(StylePrefixCopy *) buffer = *(StylePrefixCopy *) fontStylePrefix;
    strcat(buffer, (const char *) fontPropTag);
    strcat(buffer, (const char *) sellAcquiredText);
    color_tag = fontColorTag(LIST_COLOR_SELECTED);
    strcat(buffer, (const char *) color_tag);
    strcat(buffer, name_text);
    strcat(buffer, (const char *) fontGreyTag);
    if (count >= ACQUIRED_COUNT_MIN_SHOWN) {
        sprintf(count_text, fontHanCountFmt, count);
        strcat(buffer, (const char *) acquiredCountLead);
        strcat(buffer, count_text);
    }
    return (int) strcat(buffer, (const char *) acquiredTextEnd);
}

/* Pixel width of the acquired-item message for (name_text, count). */
int acquiredTextWidth(char *name_text, int count)
{
    char acquired_text[0x70];

    formatAcquiredText(acquired_text, name_text, count);
    return xglFontGetStringWidth(acquired_text);
}

/* Copies `count` bytes from src to dest and NUL-terminates dest. */
void textCopyChars(unsigned char *dest, const unsigned char *src, int count)
{
    unsigned char *out;
    const unsigned char *in;

    out = dest;
    in = src;
    if (count > 0) {
        do {
            *out = *in;
            in++;
            count--;
            out++;
        } while (count > 0);
    }
    *out = 0;
}

/* Flushes the font batch. */
void flushFontBatch(void)
{
    xglFontFlush();
}

/* Rows of text below the slot origin: y minus the scroll position (u.text.scroll_y of a text slot, u.list.slide_pos of a list
 * slot), inset by the margin. */
int textSlotFirstRowY(const MsgSlot *slot)
{
    return slot->y - slot->u.text.scroll_y + TEXT_ROWS_MARGIN;
}

extern unsigned char fmtXAdvance[];

/* Appends an x-advance tag "$ax<advance_x>;" to dst. */
void appendXAdvanceTag(char *dst, int advance_x) {
    u8 tag[0x20];

    memset(&tag[0], 0, sizeof(tag));
    sprintf((char *) &tag[0], fmtXAdvance, advance_x);
    strcat(dst, (const char *) &tag[0]);
}

/* Appends the decimal `number` to dst. */
void appendNumberText(char *dst, int number) {
    u8 number_text[0x20];

    memset(&number_text[0], 0, sizeof(number_text));
    sprintf((char *) &number_text[0], fmtDecimal, number);
    strcat(dst, (const char *) &number_text[0]);
}

int listSlotFirstRowY(const MsgSlot *slot)
{
    return slot->y - slot->u.list.slide_pos + TEXT_ROWS_MARGIN;
}

int sellItemFirstRowY(const MsgSlot *slot)
{
    return slot->y - slot->u.list.slide_pos + SELL_ROWS_MARGIN;
}

extern const unsigned char fontXOffsetTag[];

void talkBoxPrintDigitPlain(int x, int y, int color, const char *text) {
    sprintf(SCRATCH_TEXT, fmtThreeStrings, fontStylePrefix, fontXOffsetTag, text);
    xglFontPrint(x, y, color, SCRATCH_TEXT);
}

extern const unsigned char fmtFourStrings[];

void talkBoxPrintDigitAccent(int x, int y, int color, const char *text) {
    u8 *color_tag;

    color_tag = fontColorTag(FONT_COLOR_ACCENT_SHADOW);
    sprintf(SCRATCH_TEXT, fmtFourStrings, fontStylePrefix, fontXOffsetTag, color_tag, text);
    xglFontPrint(x, y - 1, color, SCRATCH_TEXT);
}

/* Draws text at (x, y + 8) with the largest z, in front of the rest. */
void fontPrintFront(int x, int y, char *text)
{
    xglFontPrint(x, y + FRONT_TEXT_Y_OFFSET, FONT_Z_FRONT, text);
}

const unsigned char fmtColorTag[16] = "$col%06x;";
const unsigned char fmtZoomTag[16] = "$zoom%d,%d;";

static unsigned char fontColorTagBuffer[24];

/* Returns the colour tag "$col%06x;" for `color`, formatted into a static buffer. */
u8 *fontColorTag(int color) {
    sprintf((char *) fontColorTagBuffer, fmtColorTag, color);
    return fontColorTagBuffer;
}

static unsigned char fontZoomTagBuffer[24];

/* Returns the zoom tag "$zoom%d,%d;" for `zoom`, formatted into a static buffer. */
u8 *fontZoomTag(int zoom) {
    sprintf((char *) fontZoomTagBuffer, fmtZoomTag, zoom, zoom);
    return fontZoomTagBuffer;
}

/* Digit-correction table of formatEntryMsf (.data), and the Sell label text offset (.sdata). */
unsigned char entryFrameDigitCorrection[16] = {
    0, 0, 1, 2, 0, 2, 1, 2,
    1, 0, 0, 0, 0, 0, 0, 0
};

/* .sdata of this file (0x69DCD0-0x69DD48): its format strings and tags (data map review 2026-11-17). */
unsigned char fmtThreeStrings[8] = "%s%s%s";

unsigned char fontPropTag[8] = "$prop1;";

unsigned char sellMoneyFormat[8] = "%dG";

unsigned char fmtDecimal[8] = "%d";

unsigned char fontXOffsetFmt[8] = "$xx1,4;";

unsigned char fmtString[8] = "%s";

unsigned char sellActionSell[8] = ":Sell";

unsigned char sellActionClose[8] = ":Close";

unsigned char sellCurrencySuffix[8] = "G";

unsigned char fmtXAdvance[8] = "$ax%d;";

const char *entryAccentGlyph = (const char *) sellNoItemsText + 0x18;

unsigned char fontHanTag[8] = "$han1;";

unsigned char entrySeparatorMark[8] = ":";

/* JP:  × 
 * EN:  x  (the count prefix) */
unsigned char acquiredCountLead[8] = " \xa1" "\xdf" " ";

unsigned char acquiredTextEnd[8] = ".";
