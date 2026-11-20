#include "common.h"
#include "gs.h"
#include "main/xgl_packet_scratch.h"
#include "main/gr_screen.h"
#include "main/talk_script.h"
#include "main/gs_make_tex0.h"
#include "main/gs_make_frame.h"
#include "main/talk_draw_fade.h"
#include "main/talk_text_box.h"
#include "main/msg_slot_core.h"
#include "main/msg_slot_center.h"
#include "main/xgl_packet.h"


/* Screen to GS primitive coordinates: 12.4 fixed point around the 2048 centre. */
#define RECT_GS_X(x) ((int) (((float) (x) + 1792.0f) * 16.0f))
#define RECT_GS_Y(y) ((int) (((float) (y) + 1824.0f) * 16.0f))
#define RECT_GS_Z 0x7FFFFFFF /* nearest depth */
#define RECT_FADE_STEPS 64.0f

/*
 * Draws the screen rectangle (x0, y0)-(x1, y1) as one GS sprite in color. The alpha byte of
 * color is replaced by scale * 64; a scale below 1 shrinks the rectangle toward its centre
 * along the control points (grControlPointSet / grControlPointBlend).
 */
void talkDrawFadeRect(float scale, int x0, int y0, int x1, int y1, unsigned int color) {
    XglPacket *packet = xglPacketGetCurrent();
    GsPair *pairs = (GsPair *) SCRATCHPAD_BASE;
    int pairCount = 0;
    int left;
    int top;
    int right;
    int bottom;
    int blend_x;
    int blend_y;
    int width;
    int height;
    int center_x;
    int center_y;
    int half_w;
    int half_h;

    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = TALK_GIF_TAG_AD_EOP;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ZBUF_1;
    pairs[pairCount].ad.data = GS_SET_ZBUF(0, GS_PSMZ24, 1);
    pairCount++;
    pairs[pairCount].ad.reg = GS_SCISSOR_1;
    pairs[pairCount].ad.data = GS_SET_SCISSOR(0, TALK_SCREEN_RIGHT, 0, TALK_SCREEN_BOTTOM);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_CLAMP_1;
    pairs[pairCount].ad.data = GS_SET_CLAMP(GS_REGION_CLAMP, GS_REGION_CLAMP, 0, TALK_SCREEN_RIGHT + 1, 0, TALK_SCREEN_BOTTOM + 1);
    pairCount++;
    pairs[pairCount].ad.reg = GS_COLCLAMP;
    pairs[pairCount].ad.data = 1;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = color;
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_SPRITE, 1, 0, 0, 1, 0, 0, 0, 0);
    pairs[pairCount].ad.reg = GS_PRIM;
    pairCount++;
    color = ((unsigned int) (scale * RECT_FADE_STEPS) << 24) | (color & 0xFFFFFF);
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = color;
    pairCount++;
    center_x = (x1 + x0) >> 1;
    center_y = (y1 + y0) >> 1;
    width = x1 - x0;
    height = y1 - y0;
    half_w = width >> 1;
    half_h = height >> 1;
    if (scale < 1.0f) {
        grControlPointSet(0, 0, 0);
        grControlPointSet(1, width >> 2, 0);
        grControlPointSet(2, half_w, half_h);
        grControlPointBlend(&blend_x, &blend_y, scale);
        left = center_x - blend_x;
        right = center_x + blend_x;
        top = center_y - blend_y;
        bottom = center_y + blend_y;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(left), RECT_GS_Y(top), RECT_GS_Z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(right), RECT_GS_Y(bottom), RECT_GS_Z);
        pairCount++;
    } else {
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(x0), RECT_GS_Y(y0), RECT_GS_Z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(x1), RECT_GS_Y(y1), RECT_GS_Z);
        pairCount++;
    }
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}


/* Geometry and colors of the slot frame, in pixels from the slot rectangle. */
#define FRAME_SCISSOR_X1 (TALK_SCREEN_RIGHT + 1)
#define FRAME_BASE_Y 0x20            /* top of the bar below the scrolled slot origin */
#define FRAME_BASE_Y_PAGER 0x48      /* the same when the bound item list adds its header band */
#define FRAME_BAR_HEIGHT 0x25
#define FRAME_BAR_TOP_MARGIN 4
#define FRAME_SIDE_LEFT 0x15
#define FRAME_SIDE_RIGHT 0x16
#define FRAME_CLIP_INSET 0x23        /* the panel is clipped this far inside the slot top and bottom */
#define FRAME_STEPS_MIN 3
#define FRAME_STEPS_MAX 6
#define FRAME_ALPHA_MIN 0x10
#define FRAME_ALPHA_MAX 0x45
#define FRAME_RGB 0x804040
/* Texture corners of the panel and box sprites (UV registers hold 12.4 fixed point: 10.0 and 13.0 texels). */
#define SLOT_UV_MIN 0xA0
#define SLOT_UV_MAX 0xD0

/*
 * Draws the cursor bar of a list window slot (state 2) as one direct GS packet: a shadowed bar at the
 * cursor row (the sweep position less the scroll slide), a textured end piece and a blended panel (its
 * width is the slot width in 64 pixel steps, clamped to 3..6, its opacity the clamped highlight pulse).
 */
void talkDrawListCursorBar(MsgSlot *slot) {
    int top;
    int left;
    int bottom;
    int tex_u;
    GsPair *pairs;
    int pairCount;
    XglPacket *packet;
    int right;
    int steps;
    unsigned int color;
    int z;
    int base;
    int clip_left;
    int clip_right;
    int clip_top;
    int clip_bottom;
    int shadow_y1 = 17;  /* bottom edge of the shadow bar */
    int bar_y1 = 13;     /* bottom edge of the bar */

    if (slot->u.list.list_active == 0) {
        base = slot->y - slot->u.list.slide_pos + slot->u.list.sweep_value + FRAME_BASE_Y;
    } else {
        base = slot->y - slot->u.list.slide_pos + slot->u.list.sweep_value + FRAME_BASE_Y_PAGER;
    }
    top = base - FRAME_BAR_TOP_MARGIN;
    bottom = base + FRAME_BAR_HEIGHT;
    left = slot->x - FRAME_SIDE_LEFT;
    right = slot->x + slot->extent_x + FRAME_SIDE_RIGHT;
    z = slot->z;
    steps = xglClampInt((right - left) >> 6, FRAME_STEPS_MIN, FRAME_STEPS_MAX);
    clip_left = 0;
    clip_right = FRAME_SCISSOR_X1;
    clip_top = slot->y + FRAME_CLIP_INSET;
    clip_bottom = slot->y + slot->extent_y - FRAME_CLIP_INSET;
    tex_u = steps + 10;
    color = (xglClampInt(slot->u.list.pulse_value, FRAME_ALPHA_MIN, FRAME_ALPHA_MAX) << 24) | FRAME_RGB;
    packet = xglPacketGetCurrent();
    pairs = (GsPair *) SCRATCHPAD_BASE;
    pairCount = 0;
    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = TALK_GIF_TAG_AD_EOP;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ZBUF_1;
    pairs[pairCount].ad.data = GS_SET_ZBUF(0, GS_PSMZ24, 1);
    pairCount++;
    pairs[pairCount].ad.reg = GS_SCISSOR_1;
    pairs[pairCount].ad.data = GS_SET_SCISSOR(0, TALK_SCREEN_RIGHT, 0, TALK_SCREEN_BOTTOM);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = GS_SET_TEST(1, 6, 0, 0, 0, 0, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_CLAMP_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_COLCLAMP;
    pairs[pairCount].ad.data = 1;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = GS_SET_FRAME(sRender.display_buffer_base, 8, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = color;
    pairCount++;
    pairs[pairCount].ad.reg = GS_PRIM;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_SPRITE, 1, 0, 0, 1, 0, 1, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(2, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = GS_SET_RGBAQ(0, 0, 0, 0x80, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(7), RECT_GS_Y(7), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(steps + 13), RECT_GS_Y(shadow_y1), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = GS_SET_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(11), RECT_GS_Y(11), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(steps + 9), RECT_GS_Y(bar_y1), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(1);
    pairCount++;
    pairs[pairCount].ad.reg = GS_SCISSOR_1;
    pairs[pairCount].ad.data = GS_SET_SCISSOR(clip_left, clip_right, clip_top, clip_bottom);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX0_1;
    pairs[pairCount].ad.data = gsMakeTex0(2, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_PRIM;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_SPRITE, 1, 1, 0, 1, 0, 1, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = color;
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MIN, SLOT_UV_MIN);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(left), RECT_GS_Y(top), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(tex_u << 4, 0xE0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(right), RECT_GS_Y(bottom), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}

#define BOX_Q_ONE 0x3F800000 /* 1.0f, the RGBAQ Q of the untextured sprites */
#define BOX_MARK_Z 100
#define BOX_MARGIN 12
#define BOX_GLOW_OFFSET 3
#define BOX_SMALL_SIZE 0x14   /* width and height of the small box */
#define BOX_LARGE_WIDTH 0x32  /* size of the large box (timecode modes 1 and 2) */
#define BOX_LARGE_HEIGHT 0x19

/*
 * Draws the selection box around a slot's talk text as one direct GS packet: a small marker
 * (a dark square with a grey centre) and the textured box sprite, drawn once at the box and four
 * more times offset diagonally by 3 pixels for a glow. In main it is reached only from timecodeSlotDraw
 * (timecode_window.c, state 3): the timecode modes 1 and 2 (hours/minutes/seconds, minutes/seconds/frames)
 * use the larger box.
 */
void talkDrawSelectionBox(MsgSlot *slot) {
    int z;
    int shadow_min = 9;  /* marker square corners */
    int shadow_max = 14;
    int dot_min = 11;
    int dot_max = 12;
    XglPacket *packet;
    GsPair *pairs;
    int pairCount;
    int left;
    int top;
    int right;
    int bottom;

    left = talkGetBoxLayoutX(slot);
    top = talkGetBoxLayoutY(slot);
    z = slot->z;
    switch (slot->u.timecode.mode) {
    case 0:
    default:
        right = left + BOX_SMALL_SIZE;
        bottom = top + BOX_SMALL_SIZE;
        break;
    case 1:
    case 2:
        right = left + BOX_LARGE_WIDTH;
        bottom = top + BOX_LARGE_HEIGHT;
        break;
    }
    packet = xglPacketGetCurrent();
    left -= BOX_MARGIN;
    top -= BOX_MARGIN;
    right += BOX_MARGIN;
    bottom += BOX_MARGIN;
    pairs = (GsPair *) SCRATCHPAD_BASE;
    pairCount = 0;
    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = TALK_GIF_TAG_AD_EOP;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ZBUF_1;
    pairs[pairCount].ad.data = GS_SET_ZBUF(0, GS_PSMZ24, 1);
    pairCount++;
    pairs[pairCount].ad.reg = GS_SCISSOR_1;
    pairs[pairCount].ad.data = GS_SET_SCISSOR(0, TALK_SCREEN_RIGHT, 0, TALK_SCREEN_BOTTOM);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = GS_SET_TEST(1, 6, 0, 0, 0, 0, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_CLAMP_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(2, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_PRIM;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_SPRITE, 0, 0, 0, 0, 0, 0, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = GS_SET_RGBAQ(0, 0, 0, 0x80, BOX_Q_ONE);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(shadow_min), RECT_GS_Y(shadow_min), BOX_MARK_Z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(shadow_max), RECT_GS_Y(shadow_max), BOX_MARK_Z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_PRIM;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_SPRITE, 0, 0, 0, 0, 0, 0, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, BOX_Q_ONE);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(dot_min), RECT_GS_Y(dot_min), BOX_MARK_Z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(dot_max), RECT_GS_Y(dot_max), BOX_MARK_Z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(1);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX0_1;
    pairs[pairCount].ad.data = gsMakeTex0(2, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_PRIM;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_SPRITE, 1, 1, 0, 1, 0, 1, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = GS_SET_RGBAQ(0x40, 0x40, 0x60, 0x7F, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MIN, SLOT_UV_MIN);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(left), RECT_GS_Y(top), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MAX, SLOT_UV_MAX);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(right), RECT_GS_Y(bottom), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MIN, SLOT_UV_MIN);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(left + BOX_GLOW_OFFSET), RECT_GS_Y(top + BOX_GLOW_OFFSET), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MAX, SLOT_UV_MAX);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(right + BOX_GLOW_OFFSET), RECT_GS_Y(bottom + BOX_GLOW_OFFSET), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MIN, SLOT_UV_MIN);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(left - BOX_GLOW_OFFSET), RECT_GS_Y(top - BOX_GLOW_OFFSET), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MAX, SLOT_UV_MAX);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(right - BOX_GLOW_OFFSET), RECT_GS_Y(bottom - BOX_GLOW_OFFSET), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MIN, SLOT_UV_MIN);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(left - BOX_GLOW_OFFSET), RECT_GS_Y(top + BOX_GLOW_OFFSET), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MAX, SLOT_UV_MAX);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(right - BOX_GLOW_OFFSET), RECT_GS_Y(bottom + BOX_GLOW_OFFSET), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MIN, SLOT_UV_MIN);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(left + BOX_GLOW_OFFSET), RECT_GS_Y(top - BOX_GLOW_OFFSET), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(SLOT_UV_MAX, SLOT_UV_MAX);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(RECT_GS_X(right + BOX_GLOW_OFFSET), RECT_GS_Y(bottom - BOX_GLOW_OFFSET), z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}
