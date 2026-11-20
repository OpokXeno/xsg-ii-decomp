#include "common.h"
#include "main/xgl_packet_scratch.h"
#include "main/talk_script.h"
#include "main/msg_text_draw.h"
#include "main/msg_window_layout.h"
#include "main/msg_window_update.h"
#include "main/msg_window_size.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/msg_slot_core.h"
#include "main/ease_step.h"
#include "main/text_tag.h"

#include "shared.h"

f32 cosf(f32);


#define MSG_OPEN_FRAMES 12
/* Flag bits of MsgSlot.flags beyond the MSG_SLOT_* ones of msg_slot_core.h (closing, opening, open). */
#define MSG_WIN_SLIDE 0x20          /* list slide in progress (msgWindowSlideStep) */
#define MSG_WIN_SWEEP 0x100         /* cursor sweep in progress (msgWindowSweepStep) */
#define MSG_WIN_ANGLE_END 180       /* slide and sweep run their cosine for 0..180 degrees */
#define MSG_PULSE_MIN 0x10          /* the highlight pulse bounces between these values in steps of 3 */
#define MSG_PULSE_MAX 0x45
#define MSG_PULSE_STEP 3

/* Message window update (msgWindowUpdate, called from main/msg_window_run.c). Flag bits of MsgSlot.flags:
 * 0x2 closing, 0x4 opening (12 frames, then 0x48 = open | box), 0x8 pulse (msgWindowPulseStep), 0x20 slide,
 * 0x100 sweep of the cursor. The opening/closing frame counter is MsgSlot.open_counter.
 * The result is never used; int keeps the final call from becoming a tail jump. */
int msgWindowUpdate(MsgSlot *window)
{
    if (window->slot_type == MSG_SLOT_TYPE_WINDOW)
    {
        if (window->u.list.list_active != 0)
        {
            msgWindowCountItems(window);
        }
        if (!(gameWork.control & CONTROL_PAUSED))
        {
            if (window->flags & MSG_SLOT_OPENING)
            {
                window->open_counter++;
                if (window->open_counter >= MSG_OPEN_FRAMES)
                {
                    window->flags = (window->flags & ~MSG_SLOT_OPENING) | MSG_SLOT_OPEN | TIMECODE_FLAG_SELECT;
                }
            }
            if (window->flags & MSG_SLOT_CLOSING)
            {
                window->open_counter--;
                if (window->open_counter <= 0)
                {
                    window->flags = 0;
                }
            }
            if (window->flags & MSG_SLOT_OPEN)
            {
                msgWindowPulseStep(window);
            }
            if (window->flags & MSG_WIN_SLIDE)
            {
                if (msgWindowSlideStep(window))
                {
                    window->flags &= ~MSG_WIN_SLIDE;
                }
            }
            if (window->flags & MSG_WIN_SWEEP)
            {
                if (msgWindowSweepStep(window))
                {
                    window->flags &= ~MSG_WIN_SWEEP;
                }
            }
            if (window->u.list.list_active != 0)
            {
                msgWindowUpdateList(window);
            }
        }
    }
}

/* Pulse of the list highlight: bounces pulse_value by pulse_step between MSG_PULSE_MIN and MSG_PULSE_MAX. */
void msgWindowPulseStep(MsgSlot *window)
{
    int *value = &window->u.list.pulse_value;
    int *stepp = &window->u.list.pulse_step;
    int step = *stepp;

    if (step > 0)
    {
        if (*value >= MSG_PULSE_MAX - MSG_PULSE_STEP + 1)
        {
            *value = MSG_PULSE_MAX;
            *stepp = -MSG_PULSE_STEP;
            step = -MSG_PULSE_STEP;
        }
    }
    else
    {
        if (*value < MSG_PULSE_MIN + MSG_PULSE_STEP)
        {
            *value = MSG_PULSE_MIN;
            *stepp = MSG_PULSE_STEP;
            step = MSG_PULSE_STEP;
        }
    }
    *value = *value + step;
}

/* Draws the window parts for its phase flags; the list is drawn as text rows (msgWindowDrawLineList or drawSellPanel). */
void msgWindowDraw(MsgSlot *window)
{
    if (window->slot_type == MSG_SLOT_TYPE_WINDOW)
    {
        if (window->flags & MSG_SLOT_OPENING)
        {
            talkDrawFramesWithCursor(window);
        }
        if (window->flags & MSG_SLOT_CLOSING)
        {
            talkDrawFramesWithCursor(window);
        }
        if (window->flags & MSG_SLOT_OPEN)
        {
            /* do/while(0) is codegen-forcing. Compiled alternatives, all rejected (rows differ from the original):
               hoisted talkDrawFramesWithCursor before the if, explicit return after the first arm (Draw 6 rows);
               arms swapped with explicit return (12 rows); plain if/else (Update 29, size shifts later functions);
               hoisted call + if/else (Update 32). Only the break layout of this form reproduces the bytes. */
            do
            {
                if (window->u.list.list_active == 0)
                {
                    talkDrawFramesWithCursor(window);
                    msgWindowDrawLineList(window);
                    break;
                }
                talkDrawFramesWithCursor(window);
                drawSellPanel(window);
                msgWindowDrawListIcons(window);
            } while (0);
        }
    }
}

/* Closing phase: when the window is active, clears the opening, pulse, 0x40 and sweep bits and sets 0x2. */
void msgWindowClose(MsgSlot *window)
{
    if (window->slot_type == MSG_SLOT_TYPE_WINDOW)
    {
        window->flags &= ~MSG_SLOT_OPENING;
        window->flags &= ~MSG_SLOT_OPEN;
        window->flags &= ~TIMECODE_FLAG_SELECT;
        window->flags &= ~MSG_WIN_SWEEP;
        window->flags |= MSG_SLOT_CLOSING;
    }
}

/* Cursor index of an active window, 0 otherwise. */
int msgWindowGetItemIndex(MsgSlot *window)
{
    if (window->slot_type != MSG_SLOT_TYPE_WINDOW)
    {
        return 0;
    }
    return window->u.list.cursor;
}

/* Scrolls the list by delta rows and starts the 30-pixel slide that follows the scroll. */
int msgWindowScrollBy(MsgSlot *window, int delta)
{
    MsgSlotList *list = &window->u.list;

    if (window->slot_type == MSG_SLOT_TYPE_WINDOW)
    {
        window->flags |= MSG_WIN_SLIDE;
        list->slide_from = list->slide_pos;
        list->scroll += delta;
        list->scroll = xglClampInt(list->scroll, 0, list->capacity - list->visible_rows);
        list->slide_to = msgWindowRowPixels(list->scroll);
        list->slide_angle = 0;
        return list->slide_step = easeAngleStep(list->slide_from, list->slide_to);
    }
}

/* Degrees-to-radians factor used by the slide (a separate copy from msgRadiansPerDegree). */

/* Slide step: advances slide_angle by slide_step; slide_pos moves from slide_from to slide_to on a cosine. */
int msgWindowSlideStep(MsgSlot *window)
{
    MsgSlotList *list = &window->u.list;
    f32 amplitude;
    int midpoint;

    list->slide_angle += list->slide_step;
    if (list->slide_angle >= MSG_WIN_ANGLE_END)
    {
        list->slide_pos = list->slide_to;
        return 1;
    }
    midpoint = (list->slide_to + list->slide_from) >> 1;
    amplitude = (f32) (midpoint - list->slide_to);
    list->slide_pos = midpoint + (int) (amplitude * cosf((f32) list->slide_angle * 0.01745328f));
    return 0;
}

/* Sweep step: advances sweep_angle by sweep_step. At 180 degrees the value is sweep_end, then 1 is returned. */
int msgWindowSweepStep(MsgSlot *window)
{
    MsgSlotList *list = &window->u.list;

    list->sweep_angle += list->sweep_step;
    if (list->sweep_angle >= MSG_WIN_ANGLE_END)
    {
        list->sweep_value = list->sweep_end;
        return 1;
    }
    list->sweep_value = msgWindowSweepValue(window);
    return 0;
}

/* Sweep value: cosine ease from sweep_begin (0 degrees) to sweep_end (180 degrees). */
int msgWindowSweepValue(MsgSlot *window)
{
    MsgSlotList *list = &window->u.list;
    f32 amplitude;
    int midpoint;

    amplitude = (f32) -((list->sweep_end - list->sweep_begin) >> 1);
    midpoint = (list->sweep_end + list->sweep_begin) >> 1;
    return midpoint + (int) (amplitude * cosf((f32) list->sweep_angle * 0.01745328f));
}

/* Item count: the number of nonzero entries of the 40-entry count table. */
void msgWindowCountItems(MsgSlot *window)
{
    MsgSlotList *list = &window->u.list;

    list->item_count = sellCountFilledSlots(list->counts);
}

/* Per-frame list update: keeps the cursor on a filled item, then clears the sweep and slide bits when the list is empty. */
void msgWindowUpdateList(MsgSlot *window)
{
    msgWindowSkipEmptyItem(window);
    msgWindowSetEmptyLayout(window);
    if (window->u.list.item_count == 0)
    {
        window->flags &= ~MSG_WIN_SWEEP;
        window->flags &= ~MSG_WIN_SLIDE;
    }
}

/* Moves the cursor off an empty item (count zero) to the nearest filled one. */
void msgWindowSkipEmptyItem(MsgSlot *window)
{
    MsgSlotList *list = &window->u.list;

    if (list->list_active == 0 || list->list_source == 0)
    {
        return;
    }
    if (list->counts[list->cursor] == 0)
    {
        msgWindowMoveCursor(window, msgWindowNearestItemOffset(list->counts, list->cursor));
    }
}

/* Offset from index to the nearest filled item. */
int msgWindowNearestItemOffset(int *unused_table, int index)
{
    return msgWindowNearestFilledIndex(unused_table, index) - index;
}

/* Index of the nearest filled entry: searches forward from the cursor first, then backward. */
int msgWindowNearestFilledIndex(int *counts, int index)
{
    int *from = &counts[index];
    int *found = msgWindowFindFilledEntry(from, &counts[MSG_LIST_CAPACITY - 1]);

    if (found)
    {
        return found - counts;
    }
    found = msgWindowFindFilledEntry(from, counts);
    if (found)
    {
        return found - counts;
    }
    return 0;
}

/* Scan of 4-byte entries from a pointer toward a bound: the first nonzero entry's address, or 0. */
int *msgWindowFindFilledEntry(int *from, int *bound)
{
    int *p = from;
    int *entry;

    if (from <= bound)
    {
        do
        {
            entry = p;
            if (*p++ != 0)
            {
                return entry;
            }
        } while (p <= bound);
        return 0;
    }
    if (from >= bound)
    {
        do
        {
            entry = p;
            if (*p-- != 0)
            {
                return entry;
            }
        } while (p >= bound);
    }
    return 0;
}

/* Sets the cursor of an active window. */
void msgWindowSetItemIndex(MsgSlot *window, int index)
{
    if (window->slot_type == MSG_SLOT_TYPE_WINDOW)
    {
        window->u.list.cursor = index;
    }
}

/* First visible row (scroll), 0 when the window is not active. */
int msgWindowGetScroll(MsgSlot *window)
{
    if (window->slot_type != MSG_SLOT_TYPE_WINDOW)
    {
        return 0;
    }
    return window->u.list.scroll;
}

/* Scroll plus visible rows: the first row after the visible ones. */
int msgWindowGetScrollEnd(MsgSlot *window)
{
    if (window->slot_type != MSG_SLOT_TYPE_WINDOW)
    {
        return 0;
    }
    return window->u.list.scroll + window->u.list.visible_rows;
}

/* Visible rows of the list. */
int msgWindowGetVisibleRows(MsgSlot *window)
{
    if (window->slot_type != MSG_SLOT_TYPE_WINDOW)
    {
        return 0;
    }
    return window->u.list.visible_rows;
}

/* Moves the cursor by delta: the text-line path when list_active is 0, the skip-empty path otherwise. */
/* The result is never used; int keeps the final call from becoming a tail jump. */
int msgWindowMoveCursor(MsgSlot *window, int delta)
{
    MsgSlotList *list = &window->u.list;

    if (window->slot_type == MSG_SLOT_TYPE_WINDOW)
    {
        if (list->list_active == 0)
        {
            msgWindowStepCursor(window, delta);
        }
        else
        {
            msgWindowStepCursorSkipEmpty(window, delta);
        }
    }
}

/* Text-line path: moves the cursor by delta over the lines of the text, wrapping, and starts the sweep. */
void msgWindowStepCursor(MsgSlot *window, int delta)
{
    MsgSlotList *list = &window->u.list;
    int bottom;
    int overshoot;
    char *text;

    if (delta == 0 || (window->flags & MSG_WIN_SWEEP))
    {
        return;
    }
    text = list->text;
    if (text == 0)
    {
        return;
    }
    window->flags |= MSG_WIN_SWEEP;
    list->sweep_begin = list->sweep_value;
    list->cursor += delta;
    list->cursor = xglWrapIntRange(list->cursor, 0, textCountLinesOf(text) - 1);
    list->sweep_angle = 0;
    list->sweep_end = list->cursor * MSG_LIST_ROW_PIXELS;
    list->sweep_step = easeAngleStep(list->sweep_value, list->sweep_end);
    bottom = list->scroll + list->visible_rows - 1;
    if (list->visible_rows >= MSG_LIST_VISIBLE_ROWS)
    {
        overshoot = list->scroll - list->cursor;
        if (overshoot > 0)
        {
            msgWindowScrollBy(window, -overshoot);
        }
        overshoot = list->cursor - bottom;
        if (overshoot > 0)
        {
            msgWindowScrollBy(window, overshoot);
        }
    }
}

/* Skip-empty path: moves the cursor by delta over filled items only, wrapping, and starts the sweep. */
void msgWindowStepCursorSkipEmpty(MsgSlot *window, int delta)
{
    MsgSlotList *list = &window->u.list;
    int old_cursor;
    int rank;
    int bottom;
    int overshoot;

    if (delta == 0 || (window->flags & MSG_WIN_SWEEP))
    {
        return;
    }
    if (list->list_source == 0)
    {
        return;
    }
    list->item_count = sellCountFilledSlots(list->counts);
    if (list->item_count == 0)
    {
        return;
    }
    window->flags |= MSG_WIN_SWEEP;
    old_cursor = list->cursor;
    list->sweep_begin = list->sweep_value;
    list->cursor = old_cursor + delta;
    for (;;)
    {
        list->cursor = xglWrapIntRange(list->cursor, 0, MSG_LIST_CAPACITY - 1);
        if (list->counts[list->cursor] != 0)
        {
            break;
        }
        list->cursor += msgWindowStepDirection(delta);
        if (old_cursor == list->cursor)
        {
            window->flags &= ~MSG_WIN_SWEEP;
            window->flags &= ~MSG_WIN_SLIDE;
            return;
        }
    }
    rank = sellCountFilled(list->counts, list->cursor);
    list->sweep_angle = 0;
    list->sweep_end = rank * MSG_LIST_ROW_PIXELS;
    list->sweep_step = easeAngleStep(list->sweep_value, list->sweep_end);
    bottom = list->scroll + list->visible_rows - 1;
    if (list->visible_rows >= MSG_LIST_VISIBLE_ROWS)
    {
        overshoot = list->scroll - rank;
        if (overshoot > 0)
        {
            msgWindowScrollBy(window, -overshoot);
        }
        overshoot = rank - bottom;
        if (overshoot > 0)
        {
            msgWindowScrollBy(window, overshoot);
        }
    }
}
