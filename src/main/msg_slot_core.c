#include "common.h"
#include "shared.h"
#include "main/msg_text_draw.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/msg_slot_core.h"
#include "main/msg_slot_center.h"
#include "main/text_tag.h"
#include "main/xgl_font.h"

extern void *memset(void *destination, int value, unsigned int count);
extern char *strchr(const char *string, int c);
int sprintf(char *destination, const char *format, ...);
extern const char textHanTagFormat[];

#define MSG_SLOT_MIN_WIDTH 0x5E     /* msgSlotSetGeometry raises extent_x to this */
#define MSG_SLOT_OPEN_FRAMES 12     /* frames of the opening and closing animation */
#define MSG_SLOT_FADE_FRAMES 13     /* the item fade counter stops here (pending is then set back to 12) */
#define MSG_SLOT_SIZE 0x98          /* bytes cleared by msgSlotInit */
#define MSG_SLOT_INIT_COLOR 0x98808080 /* gray 0x80 with alpha 0x98 */
#define ITEM_TEXT_MARGIN 0x40       /* width added around the acquired-item text */
/* Same value as SCRATCHPAD_BASE in main/talk_script.h; not included because it declares talkDrawFramesWithFade as void while msgSlotDraw returns its result. */
#define SCRATCHPAD_BASE 0x70000000  /* scratchpad RAM, where textLineWidth builds its line */

/* Timecode slot (type 3): stores the two 64-bit frame counts that timecodeSetFramePairFromHms (timecode_hms.c) computes and sets flag 0x80. */
void timecodeSetFramePair(MsgSlot *record, long long first, long long second) {
    if (record->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        record->u.timecode.increment = second;
        record->u.timecode.limit = first;
        record->flags |= TIMECODE_FLAG_ADVANCE;
    }
}


/* Sets the two frame counts of a timecode slot (type 3) from two hours/minutes/seconds triples at 30 frames per second. */
void timecodeSetFramesHms(MsgSlot *slot, int hours1, int minutes1, int seconds1, int hours2, int minutes2, int seconds2) {
    long long first = (long long) hours1 * FRAMES_PER_HOUR + (long long) minutes1 * FRAMES_PER_MINUTE + (long long) seconds1 * FRAMES_PER_SECOND;
    long long second = (long long) hours2 * FRAMES_PER_HOUR + (long long) minutes2 * FRAMES_PER_MINUTE + (long long) seconds2 * FRAMES_PER_SECOND;

    if (slot->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        timecodeSetFramePair(slot, first, second);
    }
}

void timecodeClearAdvance(MsgSlot *object)
{
    if (object->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        object->flags &= ~TIMECODE_FLAG_ADVANCE;
    }
}

/* While active (slot type 3): stores value in the timecode colour (MsgSlotTimecode.idle_color, +0x7C). */
void timecodeSetColor(MsgSlot *object, int value)
{
    if (object->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        object->u.timecode.idle_color = value;
    }
}

/*
 * While active (slot type 3): stores value in highlight_color (+0x80), resets blink_phase (+0x84) and sets
 * flag 0x800, which makes timecodeUpdate run timecodeStepCounter.
 */
void timecodeStartCounter(MsgSlot *record, int value) {
    if (record->slot_type == MSG_SLOT_TYPE_TIMECODE) {
        record->u.timecode.highlight_color = value;
        record->u.timecode.blink_phase = 0;
        record->flags |= TIMECODE_FLAG_BLINK;
    }
}

void msgSlotInitItem(MsgSlot *record)
{
    msgSlotInit(record, 4);
}

/* Forwards its arguments to msgSlotSetGeometry while the slot is of type 4. */
void msgSlotSetGeometryIfItem(MsgSlot *record, int x, int y, int extent_x, int extent_y) {
    if (record->slot_type == MSG_SLOT_TYPE_ITEM) {
        msgSlotSetGeometry(record, x, y, extent_x, extent_y);
    }
}


void msgSlotSetAcquiredItem(MsgSlot *slot, char *name)
{
    msgSlotSetItem(slot, name, 0);
}

/* Item slot (type 4): saves the current extent, name and count, then stores the new item and widens the slot to fit its text. */
void msgSlotSetItem(MsgSlot *slot, char *name, int count) {
    MsgSlotItem *acquired = &slot->u.item;

    if (slot->slot_type == MSG_SLOT_TYPE_ITEM) {
        acquired->saved_extent_x = slot->extent_x;
        acquired->saved_name = acquired->name;
        acquired->saved_extent_y = slot->extent_y;
        acquired->saved_count = acquired->count;
        acquired->name = name;
        slot->extent_x = acquiredTextWidth(name, count) + ITEM_TEXT_MARGIN;
        acquired->count = count;
    }
}

void msgSlotSetTimer(MsgSlot *object, int value)
{
    if (object->slot_type == MSG_SLOT_TYPE_ITEM) {
        object->u.item.timer = value;
    }
}

void msgSlotStartOpening(MsgSlot *object)
{
    if (object->slot_type == MSG_SLOT_TYPE_ITEM) {
        object->flags = (object->flags & ~MSG_SLOT_CLOSING) | MSG_SLOT_OPENING;
    }
}

void msgSlotResetCounters(MsgSlot *record) {
    u32 flags_without_4;

    if (record->slot_type == MSG_SLOT_TYPE_ITEM) {
        record->u.item.pending = 0;
        record->open_counter = MSG_SLOT_OPEN_FRAMES;
        flags_without_4 = record->flags & ~MSG_SLOT_OPENING;
        record->flags = (flags_without_4 & ~MSG_SLOT_OPEN) | MSG_SLOT_FADE;
    }
}


/* Per-frame tick of an item slot (type 4) while the game is not paused: counts down the display timer and steps the flag-driven counters. */
void msgSlotItemTick(MsgSlot *slot) {
    MsgSlotItem *acquired;
    u32 flags;

    if (slot->slot_type == MSG_SLOT_TYPE_ITEM && !(gameWork.control & CONTROL_PAUSED)) {
        flags = slot->flags;
        if (flags & MSG_SLOT_OPEN) {
            if (--slot->u.item.timer <= 0) {
                msgSlotStartClosing(slot);
                flags = slot->flags;
            }
        }
        if (flags & MSG_SLOT_OPENING) {
            if (++slot->open_counter >= MSG_SLOT_OPEN_FRAMES) {
                slot->flags = (flags & ~MSG_SLOT_OPENING) | MSG_SLOT_OPEN;
                flags = slot->flags;
            }
        }
        if (flags & MSG_SLOT_CLOSING) {
            if (--slot->open_counter <= 0) {
                slot->flags = 0;
                flags = 0;
            }
        }
        if (flags & MSG_SLOT_FADE) {
            acquired = &slot->u.item;
            if (++acquired->pending >= MSG_SLOT_FADE_FRAMES) {
                acquired->pending = MSG_SLOT_FADE_FRAMES - 1;
                slot->flags = (slot->flags & ~MSG_SLOT_FADE) | MSG_SLOT_OPEN;
            }
        }
    }
}

/* main/talk_script.h declares it void, which does not compile here: "void value not ignored" at the use of the result in msgSlotDraw, which returns this call's v0 unchanged. So int is kept. The definition cannot be int instead: talk_script.c then differs in @sizes .text and .rodata (checked 2026-11-20). */
int talkDrawFramesWithFade(MsgSlot *record);

/* Draws the type 4 slot. Returns int in the original, but falls off the end when the state is not 4 (no return value on that path); a return 0 would change the bytes. */
int msgSlotDraw(MsgSlot *record) {
    int result;

    if (record->slot_type == MSG_SLOT_TYPE_ITEM) {
        if (record->flags & MSG_SLOT_OPENING) {
            talkDrawFramesWithFade(record);
        }
        if (record->flags & MSG_SLOT_CLOSING) {
            talkDrawFramesWithFade(record);
        }
        if (record->flags & MSG_SLOT_OPEN) {
            talkDrawFramesWithFade(record);
            drawAcquiredText(record);
        }
        result = record->flags & MSG_SLOT_FADE;
        if (result != 0) {
            result = talkDrawFramesWithFade(record);
        }
        return result;
    }
}

void msgSlotStartClosing(MsgSlot *record) {
    int clear_mask;

    clear_mask = ~MSG_SLOT_OPENING;
    if (record->slot_type == MSG_SLOT_TYPE_ITEM) {
        record->flags = ((record->flags & clear_mask) & ~MSG_SLOT_OPEN) | MSG_SLOT_CLOSING;
    }
}

static char *textOutputCursor;
static char textOutputBuffer[6000];
/* Resets the text output: the cursor goes to the start of the output buffer, which becomes an empty string. */
void textOutputReset(void) {
    textOutputCursor = textOutputBuffer;
    memset(textOutputBuffer, 0, 2);
    textTagTableClear();
}

char *textOutputAdvance(void)
{
    char *next;

    next = textOutputCursor + 1;
    textOutputCursor = next;
    *next = '\0';
    return textOutputCursor;
}

void textCopyCounted(u8 *dst, u8 *src, int count);
const char *textAppendStringEscape(char **cursor, const char **text);
int textRemainingWidth(const char *end, int value);
int textWordLength(const char *text);
void textFormatInsertGlyphTag(char **cursor, const char **glyph_text, int mode, MsgSlot *object);
int textIsDecimalEscape(const char *text);

/*
 * Copies the glyph text into the output buffer at the output cursor, wrapped to the object's width limit:
 * glyph tags are expanded, and a line break or a space that would overflow the line becomes the separator.
 */
void textFormatWrapped(MsgSlot *object, const char *text) {
    int width;
    char *cursor;
    int fitted;
    int done;
    int limit;
    int remaining;

    cursor = strchr(object->u.text.start, 0);
    limit = object->u.text.width_limit;
    width = textRemainingWidth(cursor - 1, limit);
    for (; *text != 0;) {
refit: /* a glyph tag 1..5 was expanded: re-measure from the new position.
          The goto is kept: compiled alternatives with an "expanded" flag all differ from the original
          (flag + break + continue of the outer for: 201 rows; flag + break inside do { } while (expanded)
          around the measure and copy loop: 229 rows). Re-measuring in place (fitted = xglFontFitWords(); done = -1; continue) duplicates the call in all five arms.
          The goto resumes at the measure without the *text test. */
        fitted = xglFontFitWords(text, &width);
        for (done = 0; done < fitted; done++) {
            if (textIsDecimalEscape(text)) {
                textAppendStringEscape(&cursor, &text);
                continue;
            }
            if (textStartsWithGlyphName(text, 1)) {
                textFormatInsertGlyphTag(&cursor, &text, 1, object);
                width = textRemainingWidth(cursor - 1, limit);
                goto refit;
            }
            if (textStartsWithGlyphName(text, 2)) {
                textFormatInsertGlyphTag(&cursor, &text, 2, object);
                width = textRemainingWidth(cursor - 1, limit);
                goto refit;
            }
            if (textStartsWithGlyphName(text, 3)) {
                textFormatInsertGlyphTag(&cursor, &text, 3, object);
                width = textRemainingWidth(cursor - 1, limit);
                goto refit;
            }
            if (textStartsWithGlyphName(text, 4)) {
                textFormatInsertGlyphTag(&cursor, &text, 4, object);
                width = textRemainingWidth(cursor - 1, limit);
                goto refit;
            }
            if (textStartsWithGlyphName(text, 5)) {
                textFormatInsertGlyphTag(&cursor, &text, 5, object);
                width = textRemainingWidth(cursor - 1, limit);
                goto refit;
            }
            if (textIsLineBreak(text)) {
                textCopyCounted((u8 *) cursor, (u8 *) "\n", 1);
                cursor++;
                text = textSkipLineBreak(text);
                break;
            }
            if (textIsSpace(text)) {
                remaining = fitted - done;
                if (textWordLength(text + 1) >= remaining) {
                    textCopyCounted((u8 *) cursor, (u8 *) "\n", 1);
                    cursor++;
                    text++;
                    break;
                }
            }
            textCopyCounted((u8 *) cursor, (u8 *) text, 1);
            cursor++;
            text++;
        }
        if (*text == 0) {
            break;
        }
        if (done == fitted) {
            textCopyCounted((u8 *) cursor, (u8 *) "\n", 1);
            cursor++;
            if (textIsLineBreak(text)) {
                text = textSkipLineBreak(text);
            }
        }
        if (textIsSpace(text)) {
            text++;
        }
        width = limit;
        if (*text == 0) {
            break;
        }
    }
    *cursor = 0;
    textOutputCursor = cursor;
}

/* True when text starts with the conversion %d. */
int textIsDecimalEscape(const char *text) {
    if (text[0] == '%') {
        if (text[1] == 'd') {
            return 1;
        }
    }
    return 0;
}

/* Appends the two-byte tag "%s" at the output cursor; the output cursor and the source text both step two characters. Returns the new text pointer. */
const char *textAppendStringEscape(char **cursor, const char **text) {
    const char *new_text;

    textCopyCounted((u8 *) *cursor, (u8 *) "%s", 2);
    new_text = *text + 2;
    *text = new_text;
    *cursor += 2;
    return new_text;
}

extern const char glyphTagText[];
extern const char glyphTagAlternateText[];

#define GLYPH_TAG_LENGTH 0x10
#define GLYPH_MODE_ALTERNATE 5

/*
 * Inserts a glyph tag at the output cursor: measures the text from the object's start up to the cursor,
 * records a tag of the given mode at that position, writes the 16-byte tag (the alternate form for mode 5)
 * and advances the glyph source past it.
 */
void textFormatInsertGlyphTag(char **cursor, const char **glyph_text, int mode, MsgSlot *object) {
    char **text = &object->u.text.start;
    int size[2];

    textBoxMeasure(&size[0], &size[1], *text, object->u.text.args, *cursor);
    textTagAdd(*text, *cursor, object->x + size[0], object->y + size[1], mode);
    if (mode != GLYPH_MODE_ALTERNATE) {
        textCopyCounted((u8 *) *cursor, (u8 *) glyphTagText, GLYPH_TAG_LENGTH);
        *cursor += GLYPH_TAG_LENGTH;
    } else {
        textCopyCounted((u8 *) *cursor, (u8 *) glyphTagAlternateText, GLYPH_TAG_LENGTH);
        *cursor += GLYPH_TAG_LENGTH;
    }
    *glyph_text = textSkipGlyphName(*glyph_text, mode);
}

int textLineWidth(const char *end);

int textRemainingWidth(const char *end, int value)
{
    return value - textLineWidth(end);
}

const char *textLineStart(const char *text);
extern const unsigned char textPropTag[];

/* Width of the line that ends at end: "$prop1;" and the line text are built in the scratchpad buffer, then measured. */
int textLineWidth(const char *end) {
    char *scratch = (char *) SCRATCHPAD_BASE;
    const char *start;

    start = textLineStart(end);
    *scratch = 0;
    strcat(scratch, (const char *) textPropTag);
    strcat(scratch, "$prop1;");
    strncat(scratch, start, end - start + 1);
    return xglFontGetStringWidth(scratch);
}

/* Start of the line that holds text: scans back to the previous newline or to the string start. */
const char *textLineStart(const char *text) {
    const char *scan;
    int first_char;
    int ch;

    scan = text;
    first_char = *scan;
    if (first_char != 0 && first_char != '\n') {
        for (;;) {
            scan -= 1;
            ch = *scan;
            if (ch == 0 || ch == '\n') {
                break;
            }
        }
    }
    return scan + 1;
}

void textSetOutputCursor(char *cursor) {
    textTagRemoveKey(cursor);
    textOutputCursor = cursor - 1;
}

/* Copies count bytes from src to dst and writes a terminator just past them. */
void textCopyCounted(u8 *dst, u8 *src, int count) {
    if (count > 0) {
        do {
            *dst = *src;
            src += 1;
            count -= 1;
            dst += 1;
        } while (count > 0);
    }
    *dst = 0;
}

int textWordLength(const char *text) {
    const char *p = text;
    while (*p != 0 && *p != ' ' && !textIsLineBreak(p)) {
        p++;
    }
    return p - text;
}

/* Clears the record, marks it in use (flags 1), sets its state and the init word MSG_SLOT_INIT_COLOR. */
void msgSlotInit(MsgSlot *record, int state) {
    memset(record, 0, MSG_SLOT_SIZE);
    record->flags = 1;
    record->slot_type = state;
    record->frame_color = MSG_SLOT_INIT_COLOR;
}

/*
 * Stores the slot geometry: origin (x, y) and extent. The callers pass the dispatch
 * position first and the body extent last (main/msg_window_move.c msgWindowMoveCurrent); the width is
 * clamped to at least MSG_SLOT_MIN_WIDTH.
 */
void msgSlotSetGeometry(MsgSlot *object, int x, int y, int extent_x, int extent_y)
{
    object->x = x;
    object->y = y;
    object->extent_x = extent_x;
    if (extent_x < MSG_SLOT_MIN_WIDTH) {
        object->extent_x = MSG_SLOT_MIN_WIDTH;
    }
    object->extent_y = extent_y;
}

static inline int lineBreakAt(const char *text) {
    if (text[0] == '\n') {
        return 1;
    }
    if (text[0] == '\\' && text[1] == 'n') {
        return 1;
    }
    return 0;
}
const char *textSkipLineBreaksBack(const char *text, const char *start) {
    while (lineBreakAt(text) && start < text) {
        text--;
    }
    return text;
}

/* Finds the highest active slot of the window whose type is mode; stores its index and returns 1, or returns 0. */
int msgWindowFindSlotByState(int *index, MsgWindow *window, int mode) {
    MsgSlot *slots = window->slots;
    int i;
    for (i = MSG_SLOT_COUNT - 1; i >= 0; i--) {
        if ((slots[i].flags & MSG_SLOT_ACTIVE) && slots[i].slot_type == mode) {
            break;
        }
    }
    if (i < 0) {
        return 0;
    }
    *index = i;
    return 1;
}

/* Formats the "$han1;" markup tag followed by the name into first. */
void textFormatHanTag(char *dst, const char *src)
{
    sprintf(dst, textHanTagFormat, src);
}

/* Writes value as decimal digits, least significant first, padded with zeros to length digits, then a sign byte (1 for negative, 0 otherwise) and a terminating 0. */
void numberToDigitsLsbFirst(char *dst, long long value, int length) {
    long long magnitude;
    int negative;
    long long place = 1;
    long long digit;
    int count = 0;

    /* Only the low 32 bits of the value carry digits; the sign comes from the full 64-bit value. */
    magnitude = (int) value < 0 ? -(int) value : (int) value;
    negative = 0;
    if (value < 0) {
        value = -value; /* dead store: keeps jump from if-converting until after combine, so the sign copy is not merged.
                          Alternatives without it all differ: "negative = value < 0;" (23 rows), "negative = 0; if (value < 0) negative = 1;"
                          (23 rows), and negating before taking the magnitude (40 rows). */
        negative = 1;
    }
    while (magnitude != 0 || count < length) {
        digit = (magnitude / place) % 10;
        dst[count++] = digit + '0';
        magnitude -= digit * place;
        place *= 10;
    }
    dst[count] = negative;
    dst[count + 1] = 0;
}

/* Parses a number stored least significant digit first (the format numberToDigitsLsbFirst writes): digits end at the first
 * character below '0', and a terminator of 1 marks a negative value. */
void digitsLsbFirstToNumber(long long *out, char *text) {
    int count = 0;
    long long place = 1;
    long long value = 0;

    for (; text[count] >= '0'; count++) {
        value += (text[count] - '0') * place;
        place *= 10;
    }
    if (text[count] == 1) {
        value = -value;
    }
    *out = value;
}

const char glyphTagText[24] = "$xx1,18; $xx1,4;";
const char glyphTagAlternateText[24] = "$xx1,28; $xx1,4;";

const unsigned char textPropTag[40] = {
    11, 25, 2, 13, 2, 36, 120, 120,
    48, 44, 51, 48, 59, 36, 114, 117,
    98, 121, 122, 50, 51, 44, 50, 49,
    59, 36, 114, 117, 98, 121, 121, 45,
    50, 59, 0, 0, 0, 0, 0, 0
};

const char textHanTagFormat[16] = "$han1;%s";
