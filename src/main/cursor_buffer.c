#include "common.h"
#include "main/font_obj.h"
#include "main/cursor_buffer.h"
#include "main/text_box.h"
#include "main/pad.h"
#include "main/se_bank_load.h"


/* Sound id of the cursor move sound (seVoiceStartDefault). */
#define SE_CURSOR_MOVE 0x01000001


/*
 * Pad handling for the object. While the first record is active, circle may show pending text,
 * and then fires the track object unless paused. While the second record is pending a tick and
 * text is not active, circle or pause advances the second record.
 */
void cursorBufferHandlePad(CursorTextRecord *buffer) {
    struct FontBlockCursor *step;
    int next_status;
    int next_state;
    int next_count;

    if (buffer == 0) {
        return;
    }
    if (!(buffer->state & CURSOR_FLAG_ACTIVE)) {
        return;
    }
    if (buffer->second_step.status_flags & CURSOR_FLAG_ACTIVE) {
        if ((PadData[0].pressed & PAD_CIRCLE) && textBoxShowPending()) {
            return;
        }
        if (textBoxAnyActive()) {
            return;
        }
        if ((PadData[0].pressed & PAD_CIRCLE) && !(buffer->state & CURSOR_STATE_PAUSE)) {
            cursorBufferStartClose(buffer);
        }
    } else if (buffer->second_step.status_flags & CURSOR_FLAG_TICK_PENDING) {
        if ((PadData[0].pressed & PAD_CIRCLE) && textBoxShowPending()) {
            return;
        }
        if (textBoxAnyActive()) {
            return;
        }
        if (!(PadData[0].pressed & PAD_CIRCLE)) {
            if (!(buffer->state & CURSOR_STATE_PAUSE)) {
                return;
            }
        }
        step = &buffer->second_step;
        next_status = step->status_flags & ~CURSOR_FLAG_TICK_PENDING;
        next_state = step->mode;
        next_count = step->count + step->step;
        step->status_flags = next_status;
        step->count = next_count;
        step->state = next_state;
    }
}

/* Activates the object: sets the active flag in state and starts the track object. */
void cursorBufferActivate(CursorTextRecord *buffer) {
    if (buffer != 0) {
        buffer->state = buffer->state | CURSOR_FLAG_ACTIVE;
        fontObjStartOpen((struct FontObject *) buffer->track_object);
    }
}

/* Advances the track object unless state bit 0x1 is set. */
void cursorBufferStartClose(CursorTextRecord *buffer) {
    if (buffer != 0) {
        if (!(buffer->state & CURSOR_STATE_PAUSE)) {
            fontObjStartClose((struct FontObject *) buffer->track_object);
        }
    }
}

/* Deactivates the object: clears the active flag in state and stops the track object. */
void cursorBufferDeactivate(CursorTextRecord *buffer) {
    if (buffer != 0) {
        buffer->state = buffer->state & ~CURSOR_FLAG_ACTIVE;
        fontObjAdvanceTracks((struct FontObject *) buffer->track_object);
    }
}

/* Resets the record: status bit 0x10000 cleared, state takes mode, aux_value and count zeroed. */
void cursorBufferReset(CursorTextRecord *buffer) {
    int mode;

    mode = buffer->mode;
    buffer->status_flags &= ~CURSOR_FLAG_ACTIVE;
    buffer->state = mode;
    buffer->aux_value = 0;
    buffer->count = 0;
}

/*
 * One tick: count grows by step, state takes mode, status bit 0x20000 is
 * cleared. cursorBufferHandlePad inlines the same body on the record at 0x34.
 */
void cursorBufferTick(struct FontBlockCursor *buffer) {
    int status;
    int mode;
    int count;

    status = buffer->status_flags & ~CURSOR_FLAG_TICK_PENDING;
    mode = buffer->mode;
    count = buffer->count + buffer->step;
    buffer->state = mode;
    buffer->count = count;
    buffer->status_flags = status;
}

/* Initializes the cursor over a buffer of capacity bytes with unit_size-byte elements; sets mode bit 0x100. */
void cursorBufferInit(ListCursor *buffer, int capacity, int unit_size, int mode_flags) {
    buffer->mode_flags = mode_flags | CURSOR_MODE_WRAP;
    buffer->unit_size = unit_size;
    buffer->cursor = 0;
    buffer->previous_cursor = 0;
    buffer->capacity = capacity;
}

/*
 * Keeps the cursor inside [0, capacity). Without CURSOR_MODE_WRAP, or while a direction is held
 * (auto-repeat, not newly pressed), it clamps to the ends; otherwise it wraps around.
 */
void cursorBufferWrapOrClamp(ListCursor *buffer) {
    unsigned int capacity;

    capacity = buffer->capacity;
    if (capacity == 0) {
        buffer->cursor = 0;
        return;
    }
    if ((buffer->mode_flags & CURSOR_MODE_WRAP) &&
        !(PadData[0].directionRepeated & ~PadData[0].directionPressed & (PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT))) {
        if (buffer->cursor < 0) {
            if (buffer->previous_cursor == 0) {
                buffer->cursor = capacity - 1;
            } else {
                buffer->cursor = 0;
            }
        } else if (buffer->cursor >= capacity) {
            if (buffer->previous_cursor == capacity - 1) {
                buffer->cursor = 0;
            } else {
                buffer->cursor = capacity - 1;
            }
        }
    } else {
        if (buffer->cursor < 0) {
            buffer->cursor = 0;
        } else if (buffer->cursor >= capacity) {
            buffer->cursor = capacity - 1;
        }
    }
}

/* Sets the cursor to an absolute byte offset, then clamps or wraps it. */
void cursorBufferSetOffset(ListCursor *buffer, int cursor) {
    int old_cursor;

    old_cursor = buffer->cursor;
    buffer->cursor = cursor;
    buffer->previous_cursor = old_cursor;
    cursorBufferWrapOrClamp(buffer);
}

/* Moves the cursor by delta bytes, clamps or wraps it, and requests the cursor move sound when it moved. */
void cursorBufferMoveBy(ListCursor *buffer, int delta) {
    int old_cursor;

    old_cursor = buffer->cursor;
    buffer->previous_cursor = old_cursor;
    buffer->cursor = old_cursor + delta;
    cursorBufferWrapOrClamp(buffer);
    if (buffer->previous_cursor != buffer->cursor) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
    }
}

/* Moves the cursor by units whole elements (units * unit_size bytes), then clamps or wraps it. */
void cursorBufferMoveUnits(ListCursor *buffer, int units) {
    int old_cursor;
    int delta;

    old_cursor = buffer->cursor;
    delta = units * buffer->unit_size;
    buffer->previous_cursor = old_cursor;
    buffer->cursor = delta + old_cursor;
    cursorBufferWrapOrClamp(buffer);
    if (buffer->previous_cursor != buffer->cursor) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
    }
}

/* Sets the buffer capacity and pulls the cursor back inside it. */
void cursorBufferSetCapacity(ListCursor *buffer, int capacity) {
    int cursor;
    int last;

    cursor = buffer->cursor;
    last = capacity - 1;
    buffer->capacity = capacity;
    if (cursor >= capacity) {
        buffer->cursor = last;
        cursor = last;
    }
    if (cursor < 0) {
        buffer->cursor = 0;
    }
}

/* Element index under the cursor: cursor / unit_size, or 0 without a unit size. */
unsigned int cursorBufferGetIndex(ListCursor *buffer) {
    unsigned int unit_size;

    unit_size = buffer->unit_size;
    if (!(unit_size != 0)) {
        return 0;
    }
    return (unsigned int) buffer->cursor / unit_size;
}

/* Cursor rounded down to a whole element: index * unit_size, or 0 without a unit size. */
int cursorBufferGetAlignedOffset(ListCursor *buffer) {
    unsigned int unit_size;

    unit_size = buffer->unit_size;
    if (!(unit_size != 0)) {
        return 0;
    }
    return ((unsigned int) buffer->cursor / unit_size) * unit_size;
}
