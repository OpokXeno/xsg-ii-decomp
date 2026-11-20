#ifndef INCLUDE_MAIN_TIMECODE_WINDOW_H
#define INCLUDE_MAIN_TIMECODE_WINDOW_H

#include "shared.h"
#include "main/msg_slot_core.h"

/* Functions of src/main/timecode_window.c that other files call. */

#define TIMECODE_OPEN_FRAMES 12     /* frames of the opening and closing animation */
#define TIMECODE_ANGLE_END 180      /* the cursor and roll angles run 0..180 degrees */
#define TIMECODE_COUNTER_WRAP 20
#define TIMECODE_COLOR_INIT 0xA8A8A8
/* Modes of the stepper (same numbering as the TextEntry kinds of main/msg_text_draw.c). */
/* Field codes of MsgSlotTimecode.selected: see TIMECODE_PHASE_* in msg_slot_core.h. */
#define TIMECODE_INDEX_MAX 9        /* digits runs 1..9 */
#define TIMECODE_FIELD_PIXELS 75    /* pixels per field step in modes 1 and 2 */
#define TIMECODE_WIDTH_BASE 0x54 /* mode 0 box width: base plus TIMECODE_PHASE_PIXELS per digit after the first */
#define TIMECODE_WIDTH_MIN 0xAA
#define TIMECODE_WIDTH_TIME 150  /* box width of modes 1 and 2 */

/* Each function takes a timecode slot (MsgSlot of msg_slot_core.h, state 3; its body is MsgSlotTimecode). */
void timecodeInit(MsgSlot *slot);
void timecodeSetGeometry(MsgSlot *slot, int first_value, int second_value, int third_value, int fourth_value);
void timecodeStartOpening(MsgSlot *slot);
int timecodeUpdate(MsgSlot *slot);     /* int only to keep the last call from becoming a tail jump; the result is never set */
void timecodeAdvance(MsgSlot *slot);
void timecodeUpdateSelection(MsgSlot *slot);
int timecodeStepRollAngle(MsgSlot *slot);
int timecodeStepCursorAngle(MsgSlot *slot);
void timecodeValidateSelection(MsgSlot *slot);
void timecodeStepCounter(MsgSlot *slot);
int timecodeCursorX(MsgSlot *slot);
void timecodeCursorEnds(MsgSlot *slot, int *low, int *high);   /* pixel range of the cursor between the previous and selected digit */
int timecodeSlotDraw(MsgSlot *slot);      /* flag-driven draw dispatch; the result is never used */
void timecodeBeginClose(MsgSlot *slot);
void timecodeSetEditable(MsgSlot *slot, int value);
void timecodeSetMode(MsgSlot *slot, int mode);
void timecodeSetDigits(MsgSlot *slot, int index);
void timecodeFitBoxWidth(MsgSlot *slot);
void timecodeCenterSlot(MsgSlot *slot);
void timecodeEnableSelection(MsgSlot *slot);

#endif /* INCLUDE_MAIN_TIMECODE_WINDOW_H */
