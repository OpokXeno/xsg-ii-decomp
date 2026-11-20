#include "common.h"
#include "gs.h"
#include "shared.h"
#include "main/gr_screen.h"
#include "main/talk_script.h"
#include "main/gs_make_tex0.h"
#include "main/gs_make_frame.h"
#include "main/talk_window_frame.h"
#include "main/talk_draw_fade.h"
#include "main/xgl_cd.h"
#include "main/msg_slot_core.h"
#include "main/xgl_packet.h"
#include "main/res_get_path.h"

/* (value * 30) - 30; the unit is not read yet (caller interpScrollBy, interp_update.c). */
int interpLineScrollY(int value) {
    return (value * 30) - 30;
}

/* value - 80; the constant's meaning is not read yet (caller msgTextSlotFit, msg_slot_start.c). */
int textSlotWrapWidth(int value) {
    return value - 80;
}

/* Entry that loads the talk texture; delegates to talkLoadOmiyaTexture. */
void talkStartTextureLoad(void) {
    talkLoadOmiyaTexture();
}

/* Closes the loaded talk texture, then clears the talk-active flag talkActive. */
void talkEndTexture(void) {
    talkCloseTexture();
    talkActive = 0;
}


/*
 * Starts loading the XTX texture "omiya\talk01.xtx" into the handle
 * talkTextureHandle, then waits while the CD service reports work pending
 * (xglCdServiceIfPending, xgl_cd.c).
 */
void talkLoadOmiyaTexture(void) {
    GameResLoadName("omiya\\talk01.xtx", &talkTextureHandle, 0, 0);
    while (xglCdServiceIfPending() != 0) {
    }
}


/* Releases the texture handle talkTextureHandle and clears it. */
void talkCloseTexture(void) {
    GameResBlockFreeByBuffer(talkTextureHandle);
    talkTextureHandle = 0;
}

/* Clears the talk-active flag talkActive. */
void talkDeactivate(void) {
    talkActive = 0;
}

extern int talkPendingValue;

/* Stores a pending value in talkPendingValue. */
void talkSetPending(int value) {
    talkPendingValue = value;
}

/* Returns the pending value in talkPendingValue and clears it. */
int talkTakePending(void) {
    int value;

    value = talkPendingValue;
    talkPendingValue = 0;
    return value;
}


/* Draws the talk window frame of a slot for each pending flag (2, 4, 8). */
void talkDrawFramesBasic(MsgSlot *slot) {
    xglPacketTextureTrans(talkTextureHandle);
    if (slot->flags & MSG_SLOT_CLOSING) {
        talkWindowDrawFrame(slot, TALK_FRAME_CLOSING, slot->open_counter * 0.08f);
    }
    if (slot->flags & MSG_SLOT_OPENING) {
        talkWindowDrawFrame(slot, TALK_FRAME_OPENING, slot->open_counter * 0.08f);
    }
    if (slot->flags & MSG_SLOT_OPEN) {
        talkWindowDrawFrame(slot, TALK_FRAME_FULL, 1.0f);
    }
}

/* Draws the talk window frames of a slot for each pending flag (2, 4, 8), then talkDrawListFooterWindow
 * when the item list is bound (list_active); flag 8 ends with talkDrawListCursorBar unless talkDrawListFooterWindow ran and item_count is 0. */
void talkDrawFramesWithCursor(MsgSlot *slot) {
    xglPacketTextureTrans(talkTextureHandle);
    if (slot->flags & MSG_SLOT_CLOSING) {
        talkWindowDrawFrame(slot, TALK_FRAME_CLOSING, slot->open_counter * 0.08f);
        if (slot->u.list.list_active != 0) {
            talkDrawListFooterWindow(slot);
        }
    }
    if (slot->flags & MSG_SLOT_OPENING) {
        talkWindowDrawFrame(slot, TALK_FRAME_OPENING, slot->open_counter * 0.08f);
        if (slot->u.list.list_active != 0) {
            talkDrawListFooterWindow(slot);
        }
    }
    if (slot->flags & MSG_SLOT_OPEN) {
        if (slot->u.list.list_active == 0) {
            talkWindowDrawFrame(slot, TALK_FRAME_FULL, 1.0f);
            talkDrawListCursorBar(slot);
        } else {
            talkWindowDrawFrame(slot, TALK_FRAME_FULL, 1.0f);
            talkDrawListFooterWindow(slot);
            if (slot->u.list.item_count != 0) {
                talkDrawListCursorBar(slot);
            }
        }
    }
}

/* Like talkDrawFramesBasic, then runs talkDrawSelectionBox when flag 0x40 is set after the 0x8 frame. */
void talkDrawFramesWithBox(MsgSlot *slot) {
    xglPacketTextureTrans(talkTextureHandle);
    if (slot->flags & MSG_SLOT_CLOSING) {
        talkWindowDrawFrame(slot, TALK_FRAME_CLOSING, slot->open_counter * 0.08f);
    }
    if (slot->flags & MSG_SLOT_OPENING) {
        talkWindowDrawFrame(slot, TALK_FRAME_OPENING, slot->open_counter * 0.08f);
    }
    if (slot->flags & MSG_SLOT_OPEN) {
        talkWindowDrawFrame(slot, TALK_FRAME_FULL, 1.0f);
        if (slot->flags & TIMECODE_FLAG_SELECT) {
            talkDrawSelectionBox(slot);
        }
    }
}

/* Draws the slot rectangle (origin plus extent) once per pending flag (2, 4, 8). */
void talkDrawFrameRects(MsgSlot *slot) {
    if (slot->flags & MSG_SLOT_CLOSING) {
        talkDrawFadeRect(slot->open_counter * 0.08f, slot->x, slot->y, slot->x + slot->extent_x, slot->y + slot->extent_y, 0);
    }
    if (slot->flags & MSG_SLOT_OPENING) {
        talkDrawFadeRect(slot->open_counter * 0.08f, slot->x, slot->y, slot->x + slot->extent_x, slot->y + slot->extent_y, 0);
    }
    if (slot->flags & MSG_SLOT_OPEN) {
        talkDrawFadeRect(1.0f, slot->x, slot->y, slot->x + slot->extent_x, slot->y + slot->extent_y, 0);
    }
}

/* Draws the talk window frames of a slot; flag 0x400 adds a fade frame driven by the item counter (MsgSlotItem.pending). */
void talkDrawFramesWithFade(MsgSlot *slot) {
    xglPacketTextureTrans(talkTextureHandle);
    if (slot->flags & MSG_SLOT_CLOSING) {
        talkWindowDrawFrame(slot, TALK_FRAME_CLOSING, slot->open_counter * 0.08f);
    }
    if (slot->flags & MSG_SLOT_OPENING) {
        talkWindowDrawFrame(slot, TALK_FRAME_OPENING, slot->open_counter * 0.08f);
    }
    if (slot->flags & MSG_SLOT_OPEN) {
        talkWindowDrawFrame(slot, TALK_FRAME_FULL, 1.0f);
    }
    if (slot->flags & MSG_SLOT_FADE) {
        talkWindowDrawFrame(slot, TALK_FRAME_OPENING, slot->u.item.pending * 0.08f);
        talkWindowDrawFrame(slot, TALK_FRAME_SIZED, 1.0f - slot->u.item.pending * 0.08f);
    }
}

/* Sets the talk-active flag talkActive, which hides the page cursor for one frame. */
void talkSetActive(void) {
    talkActive = 1;
}

/* Flags, screen rectangle, frame counter and RGBA (alpha 0x98) of the fixed window of talkDrawListFooterWindow. It sits
 * below the item list and holds the Sell and Close labels. Active and open, its counter at the open end of the 12-frame animation. */
#define TALK_FIXED_WINDOW_FLAGS (MSG_SLOT_ACTIVE | MSG_SLOT_OPEN)
#define TALK_FIXED_WINDOW_OPEN_FRAMES 0xC
#define TALK_FOOTER_WINDOW_X 0x81
#define TALK_FOOTER_WINDOW_Y 0x13B
#define TALK_FOOTER_WINDOW_W 0xFB
#define TALK_FOOTER_WINDOW_H 0x51
#define TALK_FOOTER_WINDOW_COLOR 0x98808080

/* Draws a fixed window (a stack slot with state 0, so it gets no page cursor) at the slot's depth, once for
 * each pending flag (4, 8, 2) of the slot; talkDrawFramesWithCursor adds it to a list window with a bound item list. */
void talkDrawListFooterWindow(MsgSlot *slot) {
    MsgSlot window = {TALK_FIXED_WINDOW_FLAGS, TALK_FOOTER_WINDOW_X, TALK_FOOTER_WINDOW_Y, slot->z,
                      TALK_FOOTER_WINDOW_W, TALK_FOOTER_WINDOW_H, TALK_FIXED_WINDOW_OPEN_FRAMES, 0, TALK_FOOTER_WINDOW_COLOR};
    float scale;

    scale = slot->open_counter * 0.08f;
    if (slot->flags & MSG_SLOT_OPENING) {
        talkWindowDrawFrame(&window, TALK_FRAME_OPENING, scale);
    }
    if (slot->flags & MSG_SLOT_OPEN) {
        talkWindowDrawFrame(&window, TALK_FRAME_FULL, 1.0f);
    }
    if (slot->flags & MSG_SLOT_CLOSING) {
        talkWindowDrawFrame(&window, TALK_FRAME_CLOSING, scale);
    }
}

/* Frame geometry (pixels from the window rectangle): the corner pieces are 0x20 square, the edge
 * pieces start TALK_EDGE inside the rectangle (TALK_TOP_EDGE at the top) and the gradient strips are
 * inset TALK_STRIP_INSET from the top and bottom. */
#define TALK_CORNER 0x20
#define TALK_EDGE 4
#define TALK_TOP_EDGE 7
#define TALK_STRIP_INSET 0x14

/* Page cursor: its texture and palette offsets above the font texture base, its size in pixels, and the
 * bob (it rises TALK_CURSOR_BOB pixels in the second half of the 16 frame counter). */
#define TALK_CURSOR_TEXTURE 0x208
#define TALK_CURSOR_CLUT 0x228
#define TALK_CURSOR_WIDTH 0x14
#define TALK_CURSOR_HEIGHT 0xF
#define TALK_CURSOR_BOB_FRAME 8
#define TALK_CURSOR_BOB 4

/* Screen to GS primitive coordinates: 12.4 fixed point around the 2048 centre. */
#define TALK_GS_X(x) ((int) (((float) (x) + 1792.0f) * 16.0f))
#define TALK_GS_Y(y) ((int) (((float) (y) + 1824.0f) * 16.0f))
#define TALK_COLOR_NEUTRAL 0x80808080 /* RGBAQ that leaves the texture colors unchanged */
#define TALK_COLOR_BLACK 0x80000000   /* black with alpha 0 */
#define TALK_PRIM_TEXTURED GS_SET_PRIM(GS_PRIM_SPRITE, 1, 1, 0, 1, 0, 1, 0, 0)    /* textured sprite, alpha blend, UV */
#define TALK_PRIM_MASK_SPRITE GS_SET_PRIM(GS_PRIM_SPRITE, 1, 0, 0, 1, 0, 1, 0, 0) /* untextured sprite, alpha blend */
#define TALK_PRIM_MASK_STRIP GS_SET_PRIM(4, 1, 0, 0, 1, 0, 1, 0, 0)               /* untextured triangle strip, alpha blend */
#define TALK_ABS(value) ((value) < 0 ? -(value) : (value))

/* One piece of the frame: textured corners and edges (kind 0..13), the
 * gradient-masked middle strips (14..18) and blend changes (19..23); kind
 * TALK_PIECE_END closes the list. */
typedef struct TalkPiece {
    int kind;
    int texture_offset;                    /* TBP0 above the font texture base */
    int clut_offset;                       /* CBP above the font texture base */
    int repeat;                            /* region repeat instead of region clamp */
    int u_limit;
    int v_limit;
    int u;
    int v;
} TalkPiece;

enum {
    TALK_PIECE_STRIP_FIRST = 14,
    TALK_PIECE_STRIP_LEFT_INNER = 15,
    TALK_PIECE_MIDDLE = 16,
    TALK_PIECE_STRIP_RIGHT_INNER = 17,
    TALK_PIECE_COLOR = 19,
    TALK_PIECE_BLEND_ONE_A = 20,       /* alpha mode 1 */
    TALK_PIECE_BLEND_ZERO = 21,        /* alpha mode 0 */
    TALK_PIECE_BLEND_ONE_B = 22,       /* alpha mode 1 */
    TALK_PIECE_BLEND_ONE_C = 23,       /* alpha mode 1 */
    TALK_PIECE_END = 24,
    TALK_RECT_COUNT = 19,
};

/* The font texture base, in words, plus a piece offset. */
#define TALK_TEXTURE_WORDS(offset) ((sRender.vram.texture_base << 5) + (offset))
/* Scissor of the whole 512x448 screen. */
#define TALK_SCISSOR_FULL GS_SET_SCISSOR(0, TALK_SCREEN_RIGHT, 0, TALK_SCREEN_BOTTOM)

/* Page cursor texture coordinates (GS UV, 12.4 fixed point) of its two drawn corners, in the first and the second
 * (5-bit wide texture) sprite; v 0x0A50/0x0B50 and u 0x0140/0x0280/0x0000 as in the original packet. */
#define TALK_CURSOR_UV_FIRST_A GS_SET_UV(0x0140, 0x0A50)
#define TALK_CURSOR_UV_FIRST_B GS_SET_UV(0x0280, 0x0B50)
#define TALK_CURSOR_UV_SECOND_A GS_SET_UV(0x0000, 0x0A50)
#define TALK_CURSOR_UV_SECOND_B GS_SET_UV(0x0140, 0x0B50)

/* Screen rectangle of a piece (a mirrored piece has right < left). */
typedef struct TalkRect {
    long top;
    long left;
    long right;
    long bottom;
} TalkRect;

extern TalkPiece talkWindowPieces[];


/*
 * Draws a talk window as one direct GS packet: the frame pieces of talkWindowPieces
 * around the rectangles below, then the page cursor when the window shows one.
 */
void talkWindowDrawFrame(MsgSlot *window, int mode, float scale)
{
    int left = window->x;
    int top = window->y;
    int right = left + window->extent_x;
    int bottom = top + window->extent_y;
    int z = window->z;
    unsigned int color = window->frame_color;
    TalkPiece *piece = talkWindowPieces;
    int middleFits = 1;
    GsPair *pairs;
    XglPacket *packet;
    int pairCount;
    int cursor_x;
    int cursor_y;
    int scissor_left;
    int scissor_top;
    int scissor_bottom;
    int scissor_right;
    /* Screen rectangle of each piece. */
    TalkRect rect[TALK_RECT_COUNT] = {
        { top + TALK_TOP_EDGE, left + TALK_EDGE, left + TALK_CORNER, top + TALK_CORNER },
        { bottom - TALK_EDGE, left + TALK_EDGE, left + TALK_CORNER, bottom - TALK_CORNER },
        { top + TALK_TOP_EDGE, right - TALK_EDGE, right - TALK_CORNER, top + TALK_CORNER },
        { bottom - TALK_EDGE, right - TALK_EDGE, right - TALK_CORNER, bottom - TALK_CORNER },
        { top + TALK_TOP_EDGE, left + TALK_CORNER, right - TALK_CORNER, top + TALK_CORNER },
        { bottom - TALK_EDGE, left + TALK_CORNER, right - TALK_CORNER, bottom - TALK_CORNER },
        { top + TALK_CORNER, left + TALK_EDGE, left + TALK_CORNER, bottom - TALK_CORNER },
        { top + TALK_CORNER, right - TALK_EDGE, right - TALK_CORNER, bottom - TALK_CORNER },
        { top + TALK_CORNER, left + TALK_CORNER, right - TALK_CORNER, bottom - TALK_CORNER },
        { bottom - TALK_CORNER, left + TALK_EDGE, left + TALK_CORNER, bottom - TALK_EDGE },
        { top + TALK_TOP_EDGE, right - TALK_CORNER, right - TALK_EDGE, top + TALK_CORNER },
        { bottom - TALK_CORNER, right - TALK_CORNER, right - TALK_EDGE, bottom - TALK_EDGE },
        { top + TALK_CORNER, right - TALK_CORNER, right - TALK_EDGE, bottom - TALK_CORNER },
        { bottom - TALK_CORNER, left + TALK_CORNER, right - TALK_CORNER, bottom - TALK_EDGE },
        { top + TALK_STRIP_INSET, left + TALK_EDGE, left + TALK_CORNER, bottom - TALK_STRIP_INSET },
        /* The strip ends at left + 0x40 and right - 0x3E (not mirrored). */
        { top + TALK_STRIP_INSET, left + TALK_CORNER, left + 0x40, bottom - TALK_STRIP_INSET },
        { top + TALK_STRIP_INSET, left + 0x40, right - 0x3E, bottom - TALK_STRIP_INSET },
        { top + TALK_STRIP_INSET, right - TALK_CORNER, right - 0x3E, bottom - TALK_STRIP_INSET },
        { top + TALK_STRIP_INSET, right - TALK_EDGE, right - TALK_CORNER, bottom - TALK_STRIP_INSET },
    };

    cursor_x = 0;
    cursor_y = 0;
    /* Too narrow for the middle piece: the pieces beside it meet halfway. */
    if (rect[TALK_PIECE_MIDDLE].left >= rect[TALK_PIECE_MIDDLE].right) {
        middleFits = 0;
        rect[TALK_PIECE_STRIP_LEFT_INNER].right = rect[TALK_PIECE_STRIP_RIGHT_INNER].right = (left + right) >> 1;
    }
        if (window->slot_type == 1 && (window->flags & MSG_SLOT_OPEN) && !(window->flags & INTERP_FLAG_SCAN) && talkActive == 0) {
        talkWindowCursorPos(&cursor_x, &cursor_y, window);
    }
    packet = xglPacketGetCurrent();
    pairs = (GsPair *) SCRATCHPAD_BASE;
    pairCount = 0;
    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = TALK_GIF_TAG_AD_EOP;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_PABE;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_COLCLAMP;
    pairs[pairCount].ad.data = 1;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ZBUF_1;
    pairs[pairCount].ad.data = GS_SET_ZBUF(0, GS_PSMZ24, 1);
    pairCount++;
    talkWindowFrameRect(&scissor_left, &scissor_top, &scissor_bottom, &scissor_right, window, mode, scale);
    color = talkColorFadeAlpha(color, scale);
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_SCISSOR_1;
    pairs[pairCount].ad.data = GS_SET_SCISSOR(scissor_left, scissor_right, scissor_top, scissor_bottom);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = GS_SET_ALPHA(0, 1, 0, 1, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = GS_SET_FRAME(sRender.display_buffer_base, 8, GS_PSMCT32, 0);
    pairCount++;
    pairs[pairCount].ad.data = TALK_PRIM_TEXTURED;
    pairs[pairCount].ad.reg = GS_PRIM;
    pairCount++;

    if (piece->kind != TALK_PIECE_END) {
        do {
            pairs[pairCount].ad.reg = GS_TEXFLUSH;
            pairs[pairCount].ad.data = 0;
            pairCount++;
            if (piece->kind < TALK_PIECE_STRIP_FIRST) {
                int u = piece->u;
                int v = piece->v;
                int r_top;
                int r_right;
                int r_left;
                int r_bottom;
                int dx;
                int dy;
                int width;
                int height;

                r_left = rect[piece->kind].left;
                r_top = rect[piece->kind].top;
                r_right = rect[piece->kind].right;
                r_bottom = rect[piece->kind].bottom;
                dx = r_right - r_left;
                dy = r_bottom - r_top;
                width = TALK_ABS(dx);
                height = TALK_ABS(dy);

                pairs[pairCount].ad.reg = GS_TEX0_1;
                pairs[pairCount].ad.data = GS_SET_TEX0(TALK_TEXTURE_WORDS(piece->texture_offset), 8, GS_PSMT8, 5, 9, 1, 0,
                                                  TALK_TEXTURE_WORDS(piece->clut_offset), 0, 0, 0, 1);
                pairCount++;
                if (piece->repeat != 0) {
                    pairs[pairCount].ad.reg = GS_CLAMP_1;
                    pairs[pairCount].ad.data = GS_SET_CLAMP(GS_REGION_REPEAT, GS_REGION_REPEAT, piece->u_limit, 0, piece->v_limit, 0);
                    pairCount++;
                } else {
                    pairs[pairCount].ad.reg = GS_CLAMP_1;
                    pairs[pairCount].ad.data = GS_SET_CLAMP(GS_REGION_CLAMP, GS_REGION_CLAMP, u, piece->u_limit, v, piece->v_limit);
                    pairCount++;
                }
                pairs[pairCount].ad.data = GS_SET_UV(u * 16, v * 16);
                pairs[pairCount].ad.reg = GS_UV;
                pairCount++;
                pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(r_left), TALK_GS_Y(r_top), z);
                pairs[pairCount].ad.reg = GS_XYZF2;
                pairCount++;
                pairs[pairCount].ad.reg = GS_UV;
                pairs[pairCount].ad.data = GS_SET_UV((u + width) * 16, (v + height) * 16);
                pairCount++;
                pairs[pairCount].ad.reg = GS_XYZF2;
                pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(r_right), TALK_GS_Y(r_bottom), z);
                pairCount++;
            } else if ((unsigned int) (piece->kind - TALK_PIECE_STRIP_FIRST) < 5) {
                if (piece->kind != TALK_PIECE_MIDDLE || middleFits == 1) {
                    int s_left;
                    int s_top;
                    int s_right;
                    int s_bottom;
                    int fade_top;
                    int middle;
                    int eighth;
                    int fade_bottom;
                    int width;
                    int height;

                    s_left = rect[piece->kind].left;
                    if (s_left < 0) {
                        s_left = 0;
                    }
                    s_top = rect[piece->kind].top;
                    s_right = rect[piece->kind].right;
                    s_bottom = rect[piece->kind].bottom;
                    fade_top = (top + bottom) >> 1;
                    middle = fade_top;
                    eighth = (bottom - top) >> 3;
                    fade_top -= eighth;
                    fade_bottom = middle + eighth;
                    pairs[pairCount].ad.data = TALK_SCISSOR_FULL;
                    pairs[pairCount].ad.reg = GS_SCISSOR_1;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_ALPHA_1;
                    pairs[pairCount].ad.data = gsMakeAlpha(0);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_FRAME_1;
                    pairs[pairCount].ad.data = gsMakeFrame(2, 0);
                    pairCount++;
                    /* A black alpha mask into the work frame: opaque at the edges, clear in the middle. */
                    pairs[pairCount].ad.data = TALK_PRIM_MASK_SPRITE;
                    pairs[pairCount].ad.reg = GS_PRIM;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_RGBAQ;
                    pairs[pairCount].ad.data = TALK_COLOR_BLACK;
                    pairCount++;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_left), TALK_GS_Y(s_top), 1);
                    pairs[pairCount].ad.reg = GS_XYZ3;
                    pairCount++;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_right), TALK_GS_Y(s_bottom), 1);
                    pairs[pairCount].ad.reg = GS_XYZF2;
                    pairCount++;
                    pairs[pairCount].ad.data = 0;
                    pairs[pairCount].ad.reg = GS_TEXFLUSH;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_TEX0_1;
                    pairs[pairCount].ad.data = GS_SET_TEX0(TALK_TEXTURE_WORDS(piece->texture_offset), 8, GS_PSMT8, 5, 9, 1, 0,
                                                      TALK_TEXTURE_WORDS(piece->clut_offset), 0, 0, 0, 1);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_CLAMP_1;
                    pairs[pairCount].ad.data = GS_SET_CLAMP(GS_REGION_REPEAT, GS_REGION_REPEAT, piece->u_limit, 0, piece->v_limit, 0);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_PRIM;
                    pairs[pairCount].ad.data = TALK_PRIM_TEXTURED;
                    pairCount++;
                    pairs[pairCount].ad.data = TALK_COLOR_NEUTRAL;
                    pairs[pairCount].ad.reg = GS_RGBAQ;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_UV;
                    pairs[pairCount].ad.data = 0;
                    pairCount++;
                    width = s_right - s_left;
                    height = s_bottom - s_top;
                    width = TALK_ABS(width);
                    height = TALK_ABS(height);
                    pairs[pairCount].ad.reg = GS_XYZ3;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_left), TALK_GS_Y(s_top), z);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_UV;
                    pairs[pairCount].ad.data = GS_SET_UV(width * 16, height * 16);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_XYZF2;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_right), TALK_GS_Y(s_bottom), z);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_TEXFLUSH;
                    pairs[pairCount].ad.data = 0;
                    pairCount++;
                    pairs[pairCount].ad.data = TALK_PRIM_MASK_STRIP;
                    pairs[pairCount].ad.reg = GS_PRIM;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_RGBAQ;
                    pairs[pairCount].ad.data = TALK_COLOR_BLACK;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_XYZ3;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_left), TALK_GS_Y(s_top), z);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_XYZ3;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_right), TALK_GS_Y(s_top), z);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_RGBAQ;
                    pairs[pairCount].ad.data = 0;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_XYZF2;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_left), TALK_GS_Y(fade_top), z);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_XYZF2;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_right), TALK_GS_Y(fade_top), z);
                    pairCount++;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_left), TALK_GS_Y(fade_bottom), z);
                    pairs[pairCount].ad.reg = GS_XYZ3;
                    pairCount++;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_right), TALK_GS_Y(fade_bottom), z);
                    pairs[pairCount].ad.reg = GS_XYZ3;
                    pairCount++;
                    pairs[pairCount].ad.data = TALK_COLOR_BLACK;
                    pairs[pairCount].ad.reg = GS_RGBAQ;
                    pairCount++;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_left), TALK_GS_Y(s_bottom), z);
                    pairs[pairCount].ad.reg = GS_XYZF2;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_XYZF2;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_right), TALK_GS_Y(s_bottom), z);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_TEXFLUSH;
                    pairs[pairCount].ad.data = 0;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_SCISSOR_1;
                    pairs[pairCount].ad.data = GS_SET_SCISSOR(scissor_left, scissor_right, scissor_top, scissor_bottom);
                    pairCount++;
                    /* Then the work frame, masked, through the scissor of the window. */
                    pairs[pairCount].ad.reg = GS_FRAME_1;
                    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_TEX0_1;
                    pairs[pairCount].ad.data = gsMakeTex0(2, 0);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_ALPHA_1;
                    pairs[pairCount].ad.data = gsMakeAlpha(1);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_CLAMP_1;
                    pairs[pairCount].ad.data = 0;
                    pairCount++;
                    pairs[pairCount].ad.data = TALK_PRIM_TEXTURED;
                    pairs[pairCount].ad.reg = GS_PRIM;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_RGBAQ;
                    pairs[pairCount].ad.data = color;
                    pairCount++;
                    pairs[pairCount].ad.data = GS_SET_UV(s_left * 16, s_top * 16);
                    pairs[pairCount].ad.reg = GS_UV;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_XYZ3;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_left), TALK_GS_Y(s_top), z);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_UV;
                    pairs[pairCount].ad.data = GS_SET_UV(s_right * 16, s_bottom * 16);
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_XYZF2;
                    pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(s_right), TALK_GS_Y(s_bottom), z);
                    pairCount++;
                }
            } else {
                switch (piece->kind) {
                case TALK_PIECE_COLOR:
                    pairs[pairCount].ad.reg = GS_RGBAQ;
                    pairs[pairCount].ad.data = color;
                    pairCount++;
                    pairs[pairCount].ad.reg = GS_ALPHA_1;
                    pairs[pairCount++].ad.data = gsMakeAlpha(0);
                    break;
                case TALK_PIECE_BLEND_ZERO:
                    pairs[pairCount].ad.reg = GS_ALPHA_1;
                    pairs[pairCount++].ad.data = gsMakeAlpha(0);
                    break;
                case TALK_PIECE_BLEND_ONE_A:
                case TALK_PIECE_BLEND_ONE_B:
                case TALK_PIECE_BLEND_ONE_C:
                    pairs[pairCount].ad.reg = GS_ALPHA_1;
                    pairs[pairCount++].ad.data = gsMakeAlpha(1);
                    break;
                }
            }
            piece++;
        } while (piece->kind != TALK_PIECE_END);
    }
    /* Same merged flag test as above. */
    if (window->slot_type == 1 && (window->flags & MSG_SLOT_OPEN) && !(window->flags & INTERP_FLAG_SCAN) && talkActive == 0) {
        pairs[pairCount].ad.reg = GS_TEXFLUSH;
        pairs[pairCount].ad.data = 0;
        pairCount++;
        pairs[pairCount].ad.reg = GS_SCISSOR_1;
        pairs[pairCount].ad.data = GS_SET_SCISSOR(0, TALK_SCREEN_RIGHT, window->y + TALK_CORNER + 1, window->y + window->extent_y - TALK_CORNER);
        pairCount++;
        pairs[pairCount].ad.reg = GS_RGBAQ;
        pairs[pairCount].ad.data = TALK_COLOR_NEUTRAL;
        pairCount++;
        pairs[pairCount].ad.reg = GS_TEX0_1;
        pairs[pairCount].ad.data = GS_SET_TEX0(TALK_TEXTURE_WORDS(TALK_CURSOR_TEXTURE), 8, GS_PSMT8, 6, 5, 1, 0,
                                              TALK_TEXTURE_WORDS(TALK_CURSOR_CLUT), 0, 0, 0, 1);
        pairCount++;
        pairs[pairCount].ad.reg = GS_PRIM;
        pairs[pairCount].ad.data = TALK_PRIM_TEXTURED;
        pairCount++;
        pairs[pairCount].ad.data = TALK_CURSOR_UV_FIRST_A;
        pairs[pairCount].ad.reg = GS_UV;
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(cursor_x), TALK_GS_Y(cursor_y), z);
        pairCount++;
        pairs[pairCount].ad.data = TALK_CURSOR_UV_FIRST_B;
        pairs[pairCount].ad.reg = GS_UV;
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(cursor_x + TALK_CURSOR_WIDTH), TALK_GS_Y(cursor_y + TALK_CURSOR_HEIGHT), z);
        pairCount++;
        if (window->u.text.frame_counter >= TALK_CURSOR_BOB_FRAME) {
            cursor_y -= TALK_CURSOR_BOB;
        }
        pairs[pairCount].ad.reg = GS_TEXFLUSH;
        pairs[pairCount].ad.data = 0;
        pairCount++;
        pairs[pairCount].ad.reg = GS_TEX0_1;
        pairs[pairCount].ad.data = GS_SET_TEX0(TALK_TEXTURE_WORDS(TALK_CURSOR_TEXTURE), 8, GS_PSMT8, 5, 5, 1, 0,
                                              TALK_TEXTURE_WORDS(TALK_CURSOR_CLUT), 0, 0, 0, 1);
        pairCount++;
        pairs[pairCount].ad.data = TALK_PRIM_TEXTURED;
        pairs[pairCount].ad.reg = GS_PRIM;
        pairCount++;
        pairs[pairCount].ad.data = TALK_CURSOR_UV_SECOND_A;
        pairs[pairCount].ad.reg = GS_UV;
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(cursor_x), TALK_GS_Y(cursor_y), z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = TALK_CURSOR_UV_SECOND_B;
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(TALK_GS_X(cursor_x + TALK_CURSOR_WIDTH), TALK_GS_Y(cursor_y + TALK_CURSOR_HEIGHT), z);
        pairCount++;
    }
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_SCISSOR_1;
    pairs[pairCount].ad.data = TALK_SCISSOR_FULL;
    pairCount++;
    pairs[pairCount].ad.reg = GS_CLAMP_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}

/* Scalar and array data of the original .data and .sdata sections, in address order. */
TalkPiece talkWindowPieces[39] = {
    { 19, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 32, 1, 31, 31, 0, 0 },
    { 1, 0, 32, 1, 31, 31, 0, 0 },
    { 2, 0, 32, 1, 31, 31, 0, 0 },
    { 3, 0, 32, 1, 31, 31, 0, 0 },
    { 6, 8, 32, 1, 31, 31, 0, 0 },
    { 4, 4, 32, 1, 31, 31, 0, 0 },
    { 8, 12, 32, 1, 31, 31, 0, 0 },
    { 7, 8, 32, 1, 31, 31, 0, 0 },
    { 5, 4, 32, 1, 31, 31, 0, 0 },
    { 20, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 16, 40, 1, 31, 31, 0, 0 },
    { 1, 16, 40, 1, 31, 31, 0, 0 },
    { 2, 16, 40, 1, 31, 31, 0, 0 },
    { 3, 16, 40, 1, 31, 31, 0, 0 },
    { 6, 24, 40, 1, 31, 31, 0, 0 },
    { 4, 20, 40, 1, 31, 31, 0, 0 },
    { 8, 28, 40, 1, 31, 31, 0, 0 },
    { 7, 24, 40, 1, 31, 31, 0, 0 },
    { 5, 20, 40, 1, 31, 31, 0, 0 },
    { 21, 0, 0, 0, 0, 0, 0, 0 },
    { 9, 516, 544, 0, 31, 31, 0, 5 },
    { 13, 528, 544, 0, 31, 31, 0, 5 },
    { 11, 532, 544, 0, 31, 31, 6, 5 },
    { 12, 284, 544, 0, 31, 31, 6, 0 },
    { 10, 276, 544, 0, 31, 31, 6, 0 },
    { 22, 0, 0, 0, 0, 0, 0, 0 },
    { 9, 512, 288, 0, 31, 31, 0, 5 },
    { 6, 264, 288, 0, 31, 31, 0, 0 },
    { 0, 256, 288, 0, 31, 31, 0, 0 },
    { 4, 260, 288, 0, 31, 31, 0, 0 },
    { 10, 272, 288, 0, 31, 31, 6, 0 },
    { 23, 0, 0, 0, 0, 0, 0, 0 },
    { 14, 536, 296, 1, 31, 3, 0, 0 },
    { 15, 524, 296, 1, 31, 3, 0, 0 },
    { 16, 520, 296, 1, 31, 3, 0, 0 },
    { 17, 524, 296, 1, 31, 3, 0, 0 },
    { 18, 536, 296, 1, 31, 3, 0, 0 },
    { 24, 0, 0, 0, 0, 0, 0, 0 },
};
int talkPendingValue = 0;
/* Handle of the talk01.xtx window frame texture (despite the name, not a script). */
void *talkTextureHandle = 0;
/* Talk-active flag: while set, the page cursor is hidden for one frame. */
int talkActive = 0;
