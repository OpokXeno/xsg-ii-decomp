#ifndef INCLUDE_MAIN_TIMECODE_COUNTER_H
#define INCLUDE_MAIN_TIMECODE_COUNTER_H

#include "main/game_data.h"

/* Functions of src/main/timecode_counter.c that other files call. */

/*
 * One 0x98-byte slot of a message window (msgWindowRunSlots runs five of them, msg_window_run.c). Its state
 * at +0x1C selects the handlers msgSlotRun calls: 1 text, 2 list window, 3 timecode, 4 item or choice.
 * msgSlotInit clears it and sets the state. The first 0x28 bytes are common to every state; the
 * rest is the per-state body in the union u. Offsets in the body comments are absolute (slot +).
 */

/* Frame and second counts of the message slots (30 frames per second). */
#define FRAMES_PER_SECOND 30
#define FRAMES_PER_MINUTE (60 * FRAMES_PER_SECOND)
#define FRAMES_PER_HOUR (60 * FRAMES_PER_MINUTE)
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR (60 * SECONDS_PER_MINUTE)

/* Values of MsgSlot.state: the handler family msgSlotRun picks. */
#define MSG_SLOT_STATE_TEXT 1       /* text slot (interp*, interp_update.c) */
#define MSG_SLOT_STATE_WINDOW 2     /* list window slot (msgWindow*, msg_window_update.c); every window
                                    entry point tests it */
#define MSG_SLOT_STATE_TIMECODE 3   /* number entry slot (timecode*, timecode_window.c) */
#define MSG_SLOT_STATE_ITEM 4       /* item or choice slot (msgSlot*, timecode_counter.c) */

/* Bits of MsgSlot.flags. The three phase bits are shared by every state (msgWindowUpdate, timecodeUpdate, the frame drawers). */
#define MSG_SLOT_CLOSING 0x2
#define MSG_SLOT_OPENING 0x4
#define MSG_SLOT_OPEN 0x8
#define INTERP_FLAG_SCAN 0x10       /* text: the cursor is still advancing (interpAdvanceCursor clears it at the end of the range) */
#define INTERP_FLAG_EASE 0x20       /* text: the cosine ease of scroll_y is running (interpStepCosine clears it when done) */
#define TIMECODE_FLAG_PHASE 0x40    /* timecode: the entry is editable: timecodeUpdate keeps the selection valid; the frame drawer also draws the selection box */
#define TIMECODE_FLAG_ADVANCE 0x80  /* timecode: elapsed_frames advances by increment each update */
#define TIMECODE_FLAG_LEVEL 0x100   /* timecode: the cursor is moving (cursor_angle step in progress) */
#define TIMECODE_FLAG_RAMP 0x200    /* timecode: a digit is rolling (roll_angle step in progress) */
#define MSG_SLOT_FADE 0x400        /* item: talkDrawFramesWithFade adds the fade frames driven by the item counter */
#define TIMECODE_FLAG_COUNTER 0x800 /* timecode: blink_phase advances each update; the entry colour fade is on */

/* Values of MsgSlotTimecode.mode and the field codes of selected (modes 1, 2). */
#define TIMECODE_MODE_NUMBER 0
#define TIMECODE_MODE_HMS 1         /* hours, minutes, seconds: field codes 11..13 */
#define TIMECODE_MODE_MSF 2         /* minutes, seconds, frames: field codes 10..12 */
#define TIMECODE_PHASE_FRAMES 10
#define TIMECODE_PHASE_SECONDS 11
#define TIMECODE_PHASE_MINUTES 12
#define TIMECODE_PHASE_HOURS 13
#define TIMECODE_PHASE_PIXELS 20    /* pixels per digit step in mode 0; the digit cell step of the entry box */

/* State 1 (text) body, at +0x28. */
typedef struct MsgSlotText {
    char *start;                      /* +0x28: start of the formatted text, key of the tags textFormatInsertGlyphTag adds */
    const char *reveal;               /* +0x2C: end of the text shown so far; interpAdvanceCursor moves it on */
    int halted;                       /* +0x30: nonzero stops interpAdvanceCursor */
    int pending;                      /* +0x34: frames between two revealed text units (interpSetPending) */
    int wait;                         /* +0x38: frames waited since the last unit */
    int first_line;                   /* +0x3C: first shown line, 1-based; msgTextSlotFit sets it to 1 */
    int scroll_y;                     /* +0x40: scroll position in pixels, eased by interpStepCosine */
    int scroll_to;                    /* +0x44: scroll_y at the end of the ease (interpLineScrollY of first_line) */
    int scroll_from;                  /* +0x48: scroll_y at the start of the ease */
    int angle;                        /* +0x4C: ease phase in degrees, 0..180 */
    int angle_step;                   /* +0x50: degrees per frame */
    int visible_rows;                 /* +0x54: text rows that fit in the slot (msgTextSlotFit) */
    int width_limit;                  /* +0x58: line width the wrapped text must fit */
    int line_count;                   /* +0x5C: lines of the formatted text (msgEntryFeedText) */
    const char *speaker_name;             /* +0x60: tag body of the last input; non-zero draws the name plate */
    int args[10];                     /* +0x64: values of the %d and %s escapes of the text (interpSetTextArg, textBoxMeasure) */
    unsigned int frame_counter;       /* +0x8C: 0..15, advanced by talkAdvanceCounter; bobs the page cursor */
    unsigned char unmodeled_90[0x8];
} MsgSlotText;

/* State 2 (list window) body, at +0x28. */
typedef struct MsgSlotList {
    int cursor;                       /* +0x28: selected row, also the index into counts */
    char *text;                       /* +0x2C: window text, lines separated by line breaks (msgWindowSetText) */
    int scroll;                       /* +0x30: first visible row, clamped to [0, capacity - visible_rows] */
    int slide_pos;                    /* +0x34: current pixel position of the scroll slide */
    int slide_to;                     /* +0x38: target pixel position, scroll * 30 */
    int slide_from;                   /* +0x3C: position the slide started from */
    int slide_angle;                  /* +0x40: slide phase in degrees */
    int slide_step;                   /* +0x44: degrees per frame */
    int visible_rows;                 /* +0x48: rows shown at once (msgListSlotSetGeometry) */
    int capacity;                     /* +0x4C: line count of the text, or MSG_LIST_CAPACITY once the item list is bound */
    int pulse_value;                  /* +0x50: highlight pulse, bounces between 0x10 and 0x45 (msgWindowPulseStep); the alpha of talkDrawListCursorBar */
    int pulse_step;                   /* +0x54: pulse step, +3 or -3 */
    int sweep_value;                  /* +0x58: cursor bar position, eased by msgWindowSweepStep */
    int sweep_end;                    /* +0x5C: sweep target, reached at 180 degrees */
    int sweep_begin;                  /* +0x60: sweep start, value at 0 degrees */
    int sweep_angle;                  /* +0x64: sweep phase in degrees */
    int sweep_step;                   /* +0x68: degrees per frame */
    int list_active;                  /* +0x6C: set by msgWindowBindItems; the list adds a header band above the rows */
    char **list_source;               /* +0x70: item name table (bind argument); null keeps the cursor on the text lines */
    int *item_costs;                  /* +0x74: cost table of the item list (main/msg_window_bind.c msgItemCost) */
    int *counts;                      /* +0x78: the 40 item counts (main/msg_window_bind.c msgItemCount) */
int *debt;                        /* +0x7C: the remaining debt word (main/msg_window_bind.c
                                      &msgShownBalance), shown by the debt box */
    int item_count;                   /* +0x80: number of nonzero entries in counts */
    unsigned char unmodeled_84[0x14];
} MsgSlotList;

/*
 * State 3 (timecode) body, at +0x28: a number entry box. The value is shown as `digits` digits (mode 0) or as
 * h:m:s / m:s:f fields (modes 1, 2); one digit or field is selected, a step changes it with a rolling animation
 * (roll_angle) and a cursor eases between the previous and the selected digit (cursor_angle, cursor_x).
 */
typedef struct MsgSlotTimecode {
    long long elapsed_frames;         /* +0x28: frames at 30 per second; timecodeAdvance adds increment */
    int mode;                         /* +0x30: 0 number, 1 hours/minutes/seconds, 2 minutes/seconds/frames */
    int editable;                     /* +0x34: set by timecodeSetEditable: 1 shows the entry, 2 also starts the digit selection when the slot opens */
    int selected;                     /* +0x38: the selected digit (1-based from the right, mode 0) or field code (TIMECODE_PHASE_*) */
    int digits;                       /* +0x3C: number of digits, 1 to 9 (timecodeSetDigits); the digit cells of the entry box */
    unsigned char unmodeled_40[8];
    long long limit;                  /* +0x48: elapsed_frames is clamped to it (timecodeSetFramePair) */
    long long increment;              /* +0x50: added to elapsed_frames by timecodeAdvance (timecodeSetFramePair) */
    int phase_start;                  /* +0x58: selected digit or field when the step started */
    int roll_angle;                   /* +0x5C: ease angle of the rolling digit in degrees, clamped at 180 by timecodeStepRollAngle */
    int roll_angle_step;              /* +0x60: added to roll_angle each step */
    int digit;                        /* +0x64: decimalDigitAt of elapsed_frames at the phase, taken by timecodeStart */
    int span;                         /* +0x68: the amount timecodeStart adds */
    int cursor_angle;                 /* +0x6C: ease angle of the cursor in degrees, 0 to 180 */
    int cursor_angle_step;            /* +0x70: added to cursor_angle by timecodeStepCursorAngle */
    int previous;                     /* +0x74: the digit or field selected before the last step; the cursor eases from it */
    int cursor_x;                     /* +0x78: x of the cursor, eased between previous and selected by timecodeCursorX; x bias of the selection box */
    int idle_color;                   /* +0x7C: idle colour of the entry, 0xA8A8A8 after timecodeInit (timecodeSetColor); entryFadeColor returns it while the blink flag is clear */
    int highlight_color;              /* +0x80: highlight colour the entry fades to (timecodeStartCounter) */
    int blink_phase;                  /* +0x84: blink phase, wraps to 0 at 20 in timecodeStepCounter (0-7 fade in, 8-11 hold, 12-19 fade out) */
    unsigned char unmodeled_88[0x10];
} MsgSlotTimecode;

/* State 4 (item or choice) body, at +0x28. */
typedef struct MsgSlotItem {
    int saved_extent_x;               /* +0x28: copy of the slot extent_x taken by msgSlotSetItem; the size of the sized frame */
    int saved_extent_y;               /* +0x2C: copy of extent_y */
    char *saved_name;                 /* +0x30: copy of name */
    int saved_count;                  /* +0x34: copy of count */
    char *name;                       /* +0x38: item name text passed to msgSlotSetAcquiredItem */
    int pending;                      /* +0x3C: fade frame counter of talkDrawFramesWithFade */
    int timer;                        /* +0x40: counted down by the state 4 tick while flag 0x8 is set */
    int count;                        /* +0x44: item count passed to msgSlotSetAcquiredItem */
    unsigned char unmodeled_48[0x50];
} MsgSlotItem;

typedef struct MsgSlot {
    unsigned int flags;               /* +0x00: status bits (MSG_SLOT_ACTIVE and the MsgSlot.flags bits above) */
    int x;                            /* +0x04: slot origin x */
    int y;                            /* +0x08: slot origin y */
    int z;                            /* +0x0C: depth of the frame and text; set by msgSlotSetZ (msg_slot_center.c) */
    int extent_x;                     /* +0x10: width, grown by msgSlotSetAcquiredItem */
    int extent_y;                     /* +0x14: height */
    int open_counter;                  /* +0x18: opening/closing frame counter, 0..12; scales the frame (* 0.08) */
    int state;                        /* +0x1C: selects the body and handlers (1 text .. 4 item) */
    unsigned int frame_color;           /* +0x20: RGBA of the frame (talkWindowDrawFrame) */
    unsigned char unmodeled_24[0x4];
    union {
        MsgSlotText text;             /* state 1 */
        MsgSlotList list;             /* state 2 */
        MsgSlotTimecode timecode;     /* state 3 */
        MsgSlotItem item;             /* state 4 */
    } u;                              /* +0x28 */
} MsgSlot;

typedef char MsgSlotSizeCheck[sizeof(MsgSlot) == 0x98 ? 1 : -1]; /* MsgWindow steps its slots by 0x98 bytes */

#define MSG_SLOT_COUNT 5
#define MSG_SLOT_ACTIVE 0x1
#define MSG_SLOT_FRONT_Z 0x7FFFFF  /* MsgSlot.z of the frontmost slot: the depth msgSlotSetZ stores when a slot is started */
typedef struct MsgWindow {
    int current_index;
    unsigned char unmodeled_04[4];
    MsgSlot slots[MSG_SLOT_COUNT];
    unsigned char unmodeled_300[8];   /* +0x300: the window is 0x308 bytes (msgWindowReset clears it all) */
} MsgWindow;

void timecodeSetColor(MsgSlot *object, int value);
void msgSlotSetGeometry(MsgSlot *object, int first_value, int second_value, int third_value, int fourth_value);
void textFormatHanTag(char *dst, const char *src);
void textOutputReset(void);
void textSetOutputCursor(char *cursor);
void textFormatWrapped(MsgSlot *object, const char *text);
char *textOutputAdvance(void);
const char *textSkipLineBreaksBack(const char *text, const char *start);
int msgWindowFindSlotByState(int *index, MsgWindow *window, int mode);
void numberToDigitsLsbFirst(char *dst, long long value, int length);
void digitsLsbFirstToNumber(long long *out, char *text);
void timecodeSetFramePair(MsgSlot *slot, long long first, long long second);
/* State 4 (item) slots. */
void msgSlotInitItem(MsgSlot *record);
void msgSlotSetItem(MsgSlot *slot, char *name, int count);
void msgSlotSetTimer(MsgSlot *object, int value);
void msgSlotStartOpening(MsgSlot *object);
void msgSlotResetCounters(MsgSlot *record);
void msgSlotSetGeometryIfItem(MsgSlot *record, int x, int y, int extent_x, int extent_y);
void msgSlotItemTick(MsgSlot *slot);
int msgSlotDraw(MsgSlot *slot);
void timecodeClearAdvance(MsgSlot *object);
/* Clears the whole slot and sets its state. */
void msgSlotInit(MsgSlot *slot, int state);
void msgSlotStartClosing(MsgSlot *slot);
void timecodeStartCounter(MsgSlot *slot, int value);

#endif /* INCLUDE_MAIN_TIMECODE_COUNTER_H */
