#include "common.h"
#include "main/xgl_packet_scratch.h"
#include "main/talk_script.h"
#include "main/debt_box.h"
#include "main/talk_window_frame.h"
#include "main/msg_slot_center.h"


#define TALK_CURSOR_Y_OFFSET 15 /* added to the measured text height when the cursor y is placed */

/*
 * Computes the screen rectangle of a talk window frame: sets up the three control points from the
 * window half extents (shape depends on mode), blends them at scale (capped at 1) and stores the
 * rectangle around the window centre, clamped to the screen.
 */
void talkWindowFrameRect(int *left, int *top, int *bottom, int *right, MsgSlot *window, int mode, float scale) {
    int blend_x;
    int blend_y;
    int center_x;
    int center_y;
    int half_w;
    int half_h;
    int size_w;
    int size_h;
    int edge_x;
    int edge_y;

    if (scale >= 1.0f) {
        scale = 1.0f;
    }
    half_w = window->extent_x >> 1;
    half_h = window->extent_y >> 1;
    center_x = window->x + half_w;
    center_y = window->y + half_h;
    if (mode != TALK_FRAME_SIZED) {
        size_w = half_w;
        size_h = half_h;
    } else {
        /* the sized frame of an item slot fading out (talkDrawFramesWithFade): its saved extent */
        size_w = window->u.item.saved_extent_x >> 1;
        size_h = window->u.item.saved_extent_y >> 1;
    }
    switch (mode) {
    case TALK_FRAME_OPENING:
        grControlPointSet(0, 0, 0);
        grControlPointSet(1, size_w, (size_h >> 1) + (size_h >> 3));
        grControlPointSet(2, size_w, size_h);
        break;
    case TALK_FRAME_CLOSING:
    case TALK_FRAME_SIZED:
        grControlPointSet(0, (size_w >> 1) + (size_w >> 2) + (size_w >> 4), 0);
        grControlPointSet(1, 0, 1);
        grControlPointSet(2, size_w, size_h);
        break;
    case TALK_FRAME_FULL: /* listed explicitly; it shares the default shape */
    default:
        grControlPointSet(0, size_w, size_h);
        grControlPointSet(1, size_w, size_h);
        grControlPointSet(2, size_w, size_h);
        break;
    }
    grControlPointBlend(&blend_x, &blend_y, scale);
    edge_x = center_x - blend_x;
    *top = center_y - blend_y;
    if (edge_x < 0) {
        edge_x = 0;
    }
    *left = edge_x;
    *right = center_x + blend_x;
    if (*right > TALK_SCREEN_RIGHT) {
        *right = TALK_SCREEN_RIGHT;
    }
    edge_y = center_y + blend_y;
    *bottom = edge_y;
    if (edge_y > TALK_SCREEN_BOTTOM) {
        *bottom = TALK_SCREEN_BOTTOM;
    }
}

/* Scales the alpha byte of an RGBA color by scale, never raising it above its old value. */
unsigned int talkColorFadeAlpha(unsigned int color, float scale) {
    unsigned int alpha = (color >> 24) & 0xFF;

    alpha *= scale;
    return (xglClampInt(alpha, 0, color >> 24) << 24) | (color & 0xFFFFFF);
}

/* Places the page cursor just after the measured text of the window. */
void talkWindowCursorPos(int *x, int *y, MsgSlot *window) {
    MsgSlotText *text = &window->u.text;
    int width = 0;
    int height = 0;
    const char *end = textSkipLineBreaksBack(text->reveal, text->start);

    textBoxMeasure(&width, &height, text->start, text->args, end);
    *x = window->x + width;
    *y = window->y + height - text->scroll_y + TALK_CURSOR_Y_OFFSET;
}
