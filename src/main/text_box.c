#include "common.h"
#include "main/pad.h"
#include "main/xgl_tick.h"
#include "main/cursor_buffer.h"
#include "main/text_box.h"
#include "main/font_obj.h"
#include "main/game_data.h"
#include "main/xgl_font.h"

/* Text system sizes: eight text boxes, four slots per box stack, four cursor buffers, 0x18 pixels per line. */
#define TEXT_BOX_COUNT 8
#define TEXT_STACK_SLOTS 4
#define CURSOR_BUFFER_POOL_SIZE 4
#define TEXT_LINE_HEIGHT 0x18
/* textBoxUpdateInput turns the blink step back at these two values of its timer. */
#define TEXT_BLINK_LOW 0x21
#define TEXT_BLINK_HIGH 0x50

/* Returns the element count of the list buffer (capacity bytes of unit_size-byte elements, rounded up) when both are set, else 0. */
int menuListPageCount(ListCursor *cursor)
{
    if (cursor->unit_size == 0) {
        return 0;
    }
    if (cursor->capacity == 0) {
        return 0;
    }
    return (unsigned int) (cursor->capacity - 1) / (unsigned int) cursor->unit_size + 1;
}


typedef struct TextBoxStack {
    int visible_lines;
    int shownFlags;
    FontTextBlock entries[TEXT_STACK_SLOTS];
    int top;
    int pendingScroll;
} TextBoxStack;

/* The wrappers textBoxSetTopPoint, textBoxSetTopSize, textBoxSetTopText and textBoxActivateTop return the callee's v0
 * and call it without a tail jump, so the four callees are declared here with a pointer result; the void prototypes
 * of font_obj.c do not compile there ("void value not ignored"), and font_obj.h leaves them out for that reason. */
void *fontObjSetText(FontTextBlock *block, unsigned char *text);
void *fontObjSetPosition(struct FontObject *obj, int x, int y);
void *fontObjSetSize(struct FontObject *obj, int x, int y);
void *fontObjSubStartOpen(FontTextBlock *block);

/*
 * Initialises a TextBoxStack header: visible_lines 3, pending scroll and top -1, and the block and track of each of its four slots.
 */
void textBoxStackInit(TextBoxStack *stack)
{
    int i;

    if (stack != 0) {
        stack->visible_lines = 3;
        stack->top = -1;
        stack->pendingScroll = -1;
        i = 0;
        for (; i < TEXT_STACK_SLOTS; i++) {
            fontObjInitBlock(&stack->entries[i]);
            fontObjSetDrawBits(&stack->entries[i].sub_object, 0xFFFFF0 + i);
        }
    }
}



/*
 * One 0xD8-byte entry of cursorBufferFlags, addressed through its own symbol (the
 * original code does not reach it as cursorBufferPool + 0x1C). Its first word is
 * the flags word that cursorBufferSetFlags and cursorBufferClearFlags update.
 */
typedef struct CursorBufferFlags {
    unsigned int flags;
    unsigned char unmodeled_04[0xD8 - 4];
} CursorBufferFlags;

typedef struct TextBox {
    int selected_line;
    int lineCount;
    char *text;
    TextBoxStack stack;
} TextBox;

extern int cursorBufferCount;
extern int cursorBufferIndex;
/* One 0xD8-byte pool slot seen both as the cursor-buffer record of cursor_buffer.c and as the font cursor entry of font_obj.c. */
typedef union CursorBufferSlot {
    CursorTextRecord record;
    struct FontCursorEntry entry;
} CursorBufferSlot;

static CursorBufferSlot cursorBufferPool[CURSOR_BUFFER_POOL_SIZE];
extern CursorBufferFlags cursorBufferFlags[];
static TextBox textBoxPool[TEXT_BOX_COUNT];

/*
 * Pushes: advances the top index, capped at 3, and passes the new top entry to three hooks.
 * The text argument is passed by the caller and not read here.
 */
void textBoxPush(TextBoxStack *stack, char *text)
{
    int oldTop;

    if (stack != 0) {
        oldTop = stack->top;
        if (oldTop < TEXT_STACK_SLOTS - 1) {
            stack->top = oldTop + 1;
            fontObjSetText(&stack->entries[stack->top], text);
            fontObjSubAdvanceTracks(&stack->entries[stack->top]);
            /* the entry's first 0x1C bytes are read as a track: fontTrackSetBoth writes its words */
            fontTrackSetBoth(&stack->entries[stack->top].origin, 0, 0);
        }
    }
}

/*
 * Sets the top entry's track to the point (x, y) through fontObjSetPosition, a plain wrapper of fontTrackSetBoth
 * (main/font_obj.c), while the stack is not empty. textBoxShowPending passes (30, 200).
 */
void *textBoxSetTopPoint(TextBoxStack *stack, int x, int y)
{
    void *result;

    result = stack;
    if (stack != 0) {
        if (stack->top >= 0) {
            result = fontObjSetPosition(&stack->entries[stack->top].sub_object, x, y);
        }
    }
    return result;
}

/* Passes the size (width, height) to fontObjSetSize for the top entry's track while the stack is not empty. */
void *textBoxSetTopSize(TextBoxStack *stack, int width, int height)
{
    void *result;

    result = stack;
    if (stack != 0) {
        if (stack->top >= 0) {
            result = fontObjSetSize(&stack->entries[stack->top].sub_object, width, height);
        }
    }
    return result;
}

#include "shared.h"

/*
 * Sets the text of the top entry through fontObjSetText (main/font_obj.c), which stores its second argument at
 * entry +0x40 and clears the entry's text state, while the stack is not empty. No caller in this TU.
 */
void *textBoxSetTopText(TextBoxStack *stack, unsigned char *text) {
    void *result;

    result = stack;
    if (stack != 0) {
        if (stack->top >= 0) {
            result = fontObjSetText(&stack->entries[stack->top], text);
        }
    }
    return result;
}

/*
 * Activates the top entry: fontObjSubStartOpen calls fontObjStartOpen on its sub-object, which sets the active bit
 * that cursorBufferActivate also sets, while the stack is not empty.
 */
void *textBoxActivateTop(TextBoxStack *stack) {
    void *result;

    result = stack;
    if (stack != 0) {
        if (stack->top >= 0) {
            result = fontObjSubStartOpen(&stack->entries[stack->top]);
        }
    }
    return result;
}

/* Pops the top entry: the top index drops by one while the stack is not empty. */
void textBoxPop(TextBoxStack *stack) {
    if (stack != 0) {
        if (stack->top >= 0) {
            stack->top = stack->top - 1;
        }
    }
}

extern const char textBoxMarkText[];

/*
 * Prints the top entry of an unflagged stack at its track position, one 0x18 step per visible line above it.
 */
void textBoxDrawMark(TextBoxStack *stack)
{
    if (!(stack->shownFlags & 2)) {
        xglFontPrint(stack->entries[stack->top].sub_object.tracks[0].current_pair.x + 0x1C8,
                     stack->entries[stack->top].sub_object.tracks[0].current_pair.y + stack->visible_lines * TEXT_LINE_HEIGHT - 8,
                     stack->entries[stack->top].sub_object.draw_bits + 1,
                     textBoxMarkText);
    }
}

/*
 * Sets the stack's pending scroll count. textBoxUpdate lowers the entries' first words by one 0x18 step per
 * count and counts this word down to zero; textBoxReset and textBoxShowPending both store 0.
 */
void textBoxSetPendingScroll(TextBoxStack *stack, int pendingScroll) {
    stack->pendingScroll = pendingScroll;
}

/* Passes the top entry to fontObjSubStartClose (the close transition of its track). */
void textBoxCloseTop(TextBoxStack *stack) {
    fontObjSubStartClose(&stack->entries[stack->top]);
}

/*
 * Ticks the stack: walks the entries up to the top, runs the scroll step of the top entry for the pending
 * count, then starts or ticks the top entry according to its flags.
 */
void textBoxUpdate(TextBoxStack *stack)
{
    int i;

    if (stack == 0) {
        return;
    }
    for (i = 0; i <= stack->top; i++) {
        if ((unsigned int)stack->entries[i].cursor.aux_value >= (unsigned int)(stack->visible_lines - 1)) {
            if (i == stack->top && stack->pendingScroll == -1) {
                stack->entries[i].origin.target_pair.y = -(TEXT_LINE_HEIGHT * (stack->entries[i].cursor.aux_value - stack->visible_lines) + TEXT_LINE_HEIGHT);
            }
        }
        fontObjDrawText(&stack->entries[i]);
    }
    if (stack->top < 0) {
        return;
    }
    if (stack->pendingScroll > 0) {
        do {
            stack->entries[stack->top].origin.target_pair.y -= TEXT_LINE_HEIGHT;
            stack->pendingScroll--;
        } while (stack->pendingScroll > 0);
    }
    if ((stack->entries[stack->top].cursor.status_flags & CURSOR_FLAG_ACTIVE) != 0) {
        if ((PadData[0].pressed & 0x20) != 0 && (stack->shownFlags & 1) == 0) {
            fontObjSubStartClose(&stack->entries[stack->top]);
        }
        if ((stack->entries[stack->top].sub_object.flags & FONT_OBJ_ACTIVE) == 0) {
            textBoxPop(stack);
        } else if (stack->entries[stack->top].sub_object.move_phase != FONT_PHASE_CLOSING) {
            textBoxDrawMark(stack);
        }
    } else if ((stack->entries[stack->top].cursor.status_flags & CURSOR_FLAG_TICK_PENDING) != 0) {
        textBoxDrawMark(stack);
        if ((PadData[0].pressed & 0x20) != 0 || (stack->shownFlags & 1) != 0) {
            cursorBufferTick(&stack->entries[stack->top].cursor);
        }
    } else if ((PadData[0].pressed & 0x20) != 0 && stack->entries[stack->top].sub_object.move_phase == FONT_PHASE_IDLE) {
        stack->entries[stack->top].cursor.step = 100;
    }
}

/*
 * Updates every text box and draws the highlight bar of each box with a non-empty stack.
 */
void textBoxUpdateAll(void);
/*
 * Moves the selected line of each active box with the pad and updates the highlight blink counter.
 */
void textBoxUpdateInput(void);

void textSystemUpdate(void)
{
    int i;

    for (i = 0; i < CURSOR_BUFFER_POOL_SIZE; i++) {
        cursorBufferUpdateEntry(&cursorBufferPool[i].entry);
    }
    if (cursorBufferCount > 0) {
        cursorBufferHandlePad(&cursorBufferPool[cursorBufferIndex].record);
        if (!(cursorBufferPool[cursorBufferIndex].record.state & CURSOR_FLAG_ACTIVE)) {
            cursorBufferIndex = CURSOR_BUFFER_POOL_SIZE - 1;
            cursorBufferCount = cursorBufferCount - 1;
            if (!(cursorBufferPool[CURSOR_BUFFER_POOL_SIZE - 1].record.state & CURSOR_FLAG_ACTIVE)) {
                while (--cursorBufferIndex > 0) {
                    if (cursorBufferPool[cursorBufferIndex].record.state & CURSOR_FLAG_ACTIVE) {
                        break;
                    }
                }
            }
        }
        if (cursorBufferCount == 0) {
            gameWork.control &= 0xFFFF7FFF;
        }
    }
    textBoxUpdateAll();
    textBoxUpdateInput();
}


extern int cursorBufferState;

void cursorBufferPush(unsigned char *text)
{
    CursorBufferSlot *buffer;
    int index;

    if (cursorBufferCount < 3) {
        for (index = cursorBufferIndex + 1; index < CURSOR_BUFFER_POOL_SIZE; index++) {
            buffer = &cursorBufferPool[index];
            if ((buffer->record.state & CURSOR_FLAG_ACTIVE) == 0) {
                cursorBufferDeactivate(&buffer->record);
                cursorBufferSetText(&buffer->entry, text);
                /* the record's first 0x1C bytes hold the track words; the record is not typed as a track */
                fontTrackSetBoth(&buffer->entry.origin, 0, 0);
                cursorBufferActivate(&buffer->record);
                cursorBufferIndex = index;
                cursorBufferCount = cursorBufferCount + 1;
                return;
            }
        }
    }
}


/*
 * Resets one TextBox: clears its selected_line word and the head word of its first entry (the text pointer
 * word at +0x08 of the box), then reinitialises the stack that starts at +0x0C.
 */
void textBoxReset(TextBox *box) {
    box->selected_line = 0;
    box->text = 0;
    textBoxStackInit(&box->stack);
    textBoxSetPendingScroll(&box->stack, 0);
}

/*
 * Passes each block of textBoxPool to textBoxReset in turn. The end address
 * is compared as a signed word, as the original code does.
 */
void textBoxResetAll(void)
{
    TextBox *box;
    int boxEnd;

    box = textBoxPool;
    boxEnd = (int) (textBoxPool + TEXT_BOX_COUNT);
    do {
        textBoxReset(box);
        box++;
    } while ((int) box < boxEnd);
}

/* Resets the TextBox at index. */
void textBoxResetAt(int index) {
    textBoxReset(&textBoxPool[index]);
}

/*
 * Stores the text pointer of text box index (the word at +0x08 of its TextBox) while index is one of the eight
 * boxes. Returns 1 when the box was set, else 0.
 */
int textBoxSetText(int index, char *text)
{
    if ((unsigned int) index >= TEXT_BOX_COUNT) {
        return 0;
    }
    textBoxPool[index].text = text;
    return 1;
}

unsigned int strlen(const char *string);

/*
 * Counts the lines of a text: the literal backslash-n pairs in the first length-1 characters, plus one.
 */
int textBoxCountLines(const char *text)
{
    int length;
    int scanEnd;
    int count;
    int i;

    length = strlen(text);
    scanEnd = length - 1;
    count = 0;
    for (i = 0; i < scanEnd; i++) {
        if (text[i] == '\\' && text[i + 1] == 'n') {
            count++;
        }
    }
    return count + 1;
}

/* Height in pixels of a number of text lines: 0x18 per line. */
int textBoxLinesHeight(int lines) {
    return lines * TEXT_LINE_HEIGHT;
}

/*
 * Returns the widest line of a text times 10: the longest run of characters before each literal backslash-n.
 */
int textBoxMaxLineWidth(const char *text, int lineCount)
{
    int length;
    int i;
    int lineWidth;
    int maxWidth;

    length = strlen(text);
    lineWidth = 0;
    i = 0;
    maxWidth = 0;
    while (i < length) {
        if (text[i] == '\\') {
            if (text[i + 1] == 'n') {
                if (maxWidth < lineWidth) {
                    maxWidth = lineWidth;
                }
                lineWidth = 0;
                i++;
            }
        } else {
            lineWidth++;
        }
        i++;
    }
    if (maxWidth < lineWidth) {
        maxWidth = lineWidth;
    }
    return maxWidth * 10;
}

extern int textBoxPending;

/*
 * Queues the box at index as the pending one when its stack is empty (stackTop below 0) and it has text.
 * textBoxPending is stored on that path, keeping its old value when the box has no text.
 */
void textBoxQueue(int index)
{
    if ((unsigned int) index < TEXT_BOX_COUNT) {
        if (textBoxPool[index].stack.top < 0) {
            if (textBoxPool[index].text != 0) {
                textBoxPending = index;
            }
        }
    }
}

/* Returns 1 when any of the eight text boxes has a non-empty stack (stackTop not below 0), else 0. */
int textBoxAnyActive(void)
{
    int i;

    for (i = 0; i < TEXT_BOX_COUNT; i++) {
        if (textBoxPool[i].stack.top >= 0) {
            return 1;
        }
    }
    return 0;
}


int textBoxShowPending(void)
{
    int lineCount;
    int width;
    int index;

    if (textBoxPending >= 0) {
        textBoxStackInit(&textBoxPool[textBoxPending].stack);
        textBoxPush(&textBoxPool[textBoxPending].stack, textBoxPool[textBoxPending].text);
        ((void (*)(TextBoxStack *, int, int)) textBoxSetTopPoint)(&textBoxPool[textBoxPending].stack, 30, 200);
        {
            char *text = textBoxPool[textBoxPending].text;
            lineCount = textBoxCountLines(text);
        }
        textBoxPool[textBoxPending].lineCount = lineCount;
        width = textBoxMaxLineWidth(textBoxPool[textBoxPending].text, lineCount);
        index = textBoxPending;
        ((void (*)(TextBoxStack *, int, int)) textBoxSetTopSize)(&textBoxPool[index].stack, width, textBoxLinesHeight(textBoxPool[index].lineCount));
        textBoxActivateTop(&textBoxPool[textBoxPending].stack);
        textBoxSetPendingScroll(&textBoxPool[textBoxPending].stack, 0);
        {
            int shown = textBoxPending;
            textBoxPending = -1;
            textBoxPool[shown].selected_line = 0;
            textBoxPool[shown].stack.shownFlags |= 1;
        }
        return 1;
    }
    return 0;
}

/*
 * Starts closing the window of text box index when it is valid and its stack is not empty, passing the box's stack to
 * textBoxCloseTop. Returns no value of its own.
 */
int textBoxCloseWindow(int index)
{
    if ((unsigned int) index < TEXT_BOX_COUNT) {
        if (textBoxPool[index].stack.top >= 0) {
            textBoxCloseTop(&textBoxPool[index].stack);
        }
    }
}

extern int textHighlightAlpha;

/*
 * Updates every text box and, for each with a non-empty stack, draws the highlight bar behind its selected
 * line: the box's track position and size give the bar, one 0x18-pixel line high, tinted by the input counter.
 */
void textBoxUpdateAll(void)
{
    int i;
    struct FontTrack *position;
    struct FontTrack *size;

    for (i = 0; i < TEXT_BOX_COUNT; i++) {
        textBoxUpdate(&textBoxPool[i].stack);
        if (textBoxPool[i].stack.top >= 0) {
            position = fontObjGetOriginTrack(&textBoxPool[i].stack.entries[0].sub_object);
            size = fontObjGetSizeTrack(&textBoxPool[i].stack.entries[0].sub_object);
            fontObjDrawQuad(position->current_pair.x, position->current_pair.y + textBoxPool[i].selected_line * TEXT_LINE_HEIGHT, position->current_pair.x + size->current_pair.x,
                            position->current_pair.y + textBoxPool[i].selected_line * TEXT_LINE_HEIGHT + TEXT_LINE_HEIGHT, (textHighlightAlpha << 24) | 0x80, 0xFFFFFF);
        }
    }
}

extern int textHighlightAlphaStep;

/*
 * Moves the selected line of each active box with the pad: PAD_UP steps back and wraps to the last line,
 * PAD_DOWN steps forward and wraps to the first. Then it updates the two input counters.
 */
void textBoxUpdateInput(void)
{
    int i;
    int blinkTimer;

    for (i = 0; i < TEXT_BOX_COUNT; i++) {
        if (textBoxPool[i].stack.top >= 0) {
            if (textBoxPool[i].stack.entries[0].sub_object.move_phase == FONT_PHASE_IDLE) {
                if (PadData[0].repeated & PAD_UP) {
                    textBoxPool[i].selected_line = textBoxPool[i].selected_line - 1;
                    if (textBoxPool[i].selected_line < 0) {
                        textBoxPool[i].selected_line = textBoxPool[i].lineCount - 1;
                    }
                }
                if (PadData[0].repeated & PAD_DOWN) {
                    textBoxPool[i].selected_line = textBoxPool[i].selected_line + 1;
                    if (textBoxPool[i].selected_line >= textBoxPool[i].lineCount) {
                        textBoxPool[i].selected_line = 0;
                    }
                }
            }
        }
    }
    blinkTimer = textHighlightAlpha + textHighlightAlphaStep;
    textHighlightAlpha = blinkTimer;
    if ((unsigned int) blinkTimer < TEXT_BLINK_LOW) {
        textHighlightAlphaStep = 2;
        return;
    }
    if ((unsigned int) blinkTimer >= TEXT_BLINK_HIGH) {
        textHighlightAlphaStep = -2;
    }
}


int textBoxConfirmSelection(int index)
{
    if (textBoxPool[index].stack.top < 0) {
        return -1;
    }
    if ((PadData[0].pressed & 0x20) != 0) {
        if ((textBoxPool[index].stack.entries[0].cursor.status_flags & CURSOR_FLAG_ACTIVE) != 0) {
            ((void (*)(int)) textBoxCloseWindow)(index);
            if (cursorBufferCount > 0) {
                cursorBufferStartClose(&cursorBufferPool[cursorBufferIndex].record);
            }
            return textBoxPool[index].selected_line;
        }
        return -1;
    }
    return -1;
}

extern TextBox textBoxPoolAlias[];

u32 textBoxHasSelection(int index) {
    u32 inverted;

    inverted = ~textBoxPoolAlias[index].selected_line;
    return inverted >> 0x1F;
}

void textSystemInit(void)
{
    int i;
    int extra;

    extra = 0xFFFFF0;
    for (i = 0; i < CURSOR_BUFFER_POOL_SIZE; i++) {
        cursorBufferInitEntry(&cursorBufferPool[i].entry);
        fontObjSetDrawBits(&cursorBufferPool[i].entry.sub_object, extra);
        extra += 2;
    }
    cursorBufferCount = 0;
    cursorBufferIndex = 0;
    cursorBufferState = 0;
    textBoxResetAll();
}

/* Returns the current cursor buffer while the count is positive, else null. */
CursorTextRecord *cursorBufferGetCurrent(void)
{
    if (cursorBufferCount > 0) {
        return &cursorBufferPool[cursorBufferIndex].record;
    }
    return 0;
}

int cursorBufferHasCurrent(void) {
    return cursorBufferCount > 0;
}

/* Sets the given bits in the current cursor buffer's state while the count is positive. */
void cursorBufferSetFlags(unsigned int bits)
{
    if (cursorBufferCount > 0) {
        cursorBufferFlags[cursorBufferIndex].flags |= bits;
    }
}

/* Clears the given bits in the current cursor buffer's state while the count is positive. */
void cursorBufferClearFlags(unsigned int mask)
{
    if (cursorBufferCount > 0) {
        cursorBufferFlags[cursorBufferIndex].flags &= ~mask;
    }
}

/* Passes the current cursor buffer to cursorBufferStartClose while the count is positive. */
void cursorBufferStartCloseCurrent(void)
{
    if (cursorBufferCount > 0) {
        cursorBufferStartClose(&cursorBufferPool[cursorBufferIndex].record);
    }
}

/* Passes the current cursor buffer to cursorBufferDeactivate while the count is positive. */
void cursorBufferDeactivateCurrent(void)
{
    if (cursorBufferCount > 0) {
        cursorBufferDeactivate(&cursorBufferPool[cursorBufferIndex].record);
    }
}

const char textBoxMarkText[24] = "$mark1f;";

int cursorBufferCount = 0;

int cursorBufferIndex = 0;

int cursorBufferState = 0;

int textBoxPending = -1;

int textHighlightAlpha = 0x50;

int textHighlightAlphaStep = -2;
