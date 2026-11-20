#include "common.h"
#include "shared.h"
#include "main/xgl_packet.h"
#include "main/pxy_packet_state.h"

extern u16 sRenderFrameBase;

/* grSetFrameBase: FRAME_1 (GS register 0x4C) with the frame base in sRenderFrameBase and FBW 8 (0x80000), a
   512-pixel-wide frame. */
void grSetFrameBase(void) {
    nmlPacketGsInit();
    packet_gs_entry64(0x4C, sRenderFrameBase | 0x80000);
    xglDirectQueueSubmitRef();
}

#include "gs.h"
#include "main/gr_screen.h"

/* grSetRenderFrame: FRAME_1 built from sRender: base at +0x20, FBW = width (+0x04) / 64, PSM at +0x02. */
void grSetRenderFrame(void) {
    nmlPacketGsInit();
    packet_gs_entry64(0x4C, GS_SET_FRAME(sRender.display_buffer_base, (s16) (sRender.reso.frame_width / 64), sRender.reso.frame_psm, 0));
    xglDirectQueueSubmitRef();
}


void grResetRenderState(void) {
    xglPacketInitVuState();
    nmlPacketAddScreenClear(sRenderFrameBase, 0xF);
}

/* The render frame is 512x448 pixels. BITBLTBUF widths and FBW count 64-pixel pages: 512 pixels is 8 pages. */
#define GS_FRAME_WIDTH 512
#define GS_FRAME_HEIGHT 448
#define GS_FRAME_WIDTH_PAGES 8

/* grDrawScreenSprite(expand, alpha): sets TEST_1, TEX0_1, CLAMP_1, TEX1_1, ALPHA_1 (alpha in its FIX byte),
   RGBAQ 0x80808080, PRIM 0x156, texture coordinates 1..511 by 1..447 texels and one 512x448 textured sprite. The
   corners move out by `expand` in 1/16 pixel. */
void grDrawScreenSprite(int expand, unsigned char alpha) {
    u64 alphaFix;

    alphaFix = (u64) alpha << 32;
    nmlPacketGsInit();
    packet_gs_entry64(GS_TEST_1, GS_TEST_ZTE_ALWAYS);
    packet_gs_entry64(GS_TEX0_1, ((u64) 0xC800 << 19) | (u64) (0x24020000 | (sRenderFrameBase << 5)));
    packet_gs_entry64(GS_CLAMP_1, 5);
    packet_gs_entry64(GS_TEX1_1, 96);
    packet_gs_entry64(GS_ALPHA_1, alphaFix | 104);
    packet_gs_entry64(GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x3F800000));
    packet_gs_entry64(GS_PRIM, GS_PRIM_TEXTURED_SPRITE);
    packet_gs_entry64(GS_UV, 0x100010);
    packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(28688 - expand, 29200 - expand, 0));
    packet_gs_entry64(GS_UV, 0x1BF01FF0);
    packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(0x8FF0 + expand, 0x8DF0 + expand, 0));
    xglDirectQueueSubmitRef();
}

#include "main/xgl_camera.h"

/* grDrawScreenFade(fade): fade in -0x80..0x80. The sprite grows by 2^24 / (0x800 - fade) - 0x2000 and its alpha
   is (0x80 - |fade|)^2 >> 7, so it is unexpanded and opaque at 0 and vanishes at +-0x80. */
void grDrawScreenFade(int fade) {
    int mag;
    int expand;

    if (fade < -128) {
        fade = -128;
    }
    if (fade > 128) {
        fade = 128;
    }
    mag = fade < 0 ? -fade : fade;
    expand = 0x1000000 / (2048 - fade) - 0x2000;
    /* the same variable carries the clamped fade and then the alpha base */
    fade = (128 - mag) & 0xff;
    grDrawScreenSprite(expand, fade * fade >> 7);
}

/* grCopyFrame(dstBase, srcBase): local-to-local transfer of 512x448 from the source frame base to the
   destination frame base (BITBLTBUF, TRXPOS 0, TRXREG, TRXDIR 2). */
void grCopyFrame(u16 dstBase, u16 srcBase) {
    nmlPacketGsInit();
    packet_gs_entry64(GS_BITBLTBUF, GS_SET_BITBLTBUF(srcBase << 5, GS_FRAME_WIDTH_PAGES, sRender.reso.frame_psm, dstBase << 5, GS_FRAME_WIDTH_PAGES, sRender.reso.frame_psm));
    packet_gs_entry64(GS_TRXPOS, 0);
    packet_gs_entry64(GS_TRXREG, GS_SET_TRXREG(GS_FRAME_WIDTH, GS_FRAME_HEIGHT));
    packet_gs_entry64(GS_TRXDIR, 2);
    packet_gs_entry64(GS_TEXFLUSH, 0);
    xglDirectQueueSubmitRef();
}

/* grFillFrame: sets the frame base, then draws one flat 512x448 blended sprite (RGBAQ 0x80808080, PRIM 0x46)
   over the whole frame, and finally sets the render frame through grSetRenderFrame. */
void grFillFrame(void) {
    grSetFrameBase();
    nmlPacketGsInit();
    packet_gs_entry64(GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    packet_gs_entry64(GS_ALPHA_1, GS_SET_ALPHA(2, 0, 2, 1, 17));
    packet_gs_entry64(GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x3F800000));
    packet_gs_entry64(GS_PRIM, GS_PRIM_SPRITE_ABE);
    packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(0x7000, 0x7200, 0));
    packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(0x9000, 0x8E00, 0));
    packet_gs_entry64(GS_TEXFLUSH, 0);
    xglDirectQueueSubmitRef();
    grSetRenderFrame();
}

/* grResetScissor: SCISSOR_1 = (0, 0) to (511, 447), the whole 512x448 frame. */
void grResetScissor(void) {
    nmlPacketGsInit();
    packet_gs_entry64(GS_SCISSOR_1, GS_SET_SCISSOR(0, GS_FRAME_WIDTH - 1, 0, GS_FRAME_HEIGHT - 1));
    xglDirectQueueSubmitRef();
}

/* grSetScissor(x0, y0, x1, y1): SCISSOR_1 from the two corners, each ordered and clamped to the frame
   (0..511 and 0..447). */
void grSetScissor(int x0, int y0, int x1, int y1) {
    int swapX = x0;
    int swapY;

    if (x1 < x0) {
        x0 = x1;
        x1 = swapX;
    }
    swapY = y0;
    if (y1 < y0) {
        y0 = y1;
        y1 = swapY;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= GS_FRAME_WIDTH) {
        x1 = GS_FRAME_WIDTH - 1;
        swapX = 0;
    }
    if (y1 >= GS_FRAME_HEIGHT) {
        y1 = GS_FRAME_HEIGHT - 1;
        swapY = 0;
    }
    nmlPacketGsInit();
    packet_gs_entry64(GS_SCISSOR_1, GS_SET_SCISSOR(x0, x1, y0, y1));
    xglDirectQueueSubmitRef();
    x0 = 0;
}


/* ScissorSet(packet, rect): a VIF direct-data packet (GIF tag and A+D with SCISSOR_1) from the four ints
   of rect (x0 +0x0, y0 +0x4, x1 +0x8, y1 +0xC), ordered and clamped to sRender's width (+0x04) and height (+0x06);
   sent through sceVif1PkAddDirectDataN. */
void ScissorSet(FontDrawCursor *cursor, void *argument) {
    ScissorRect *rect = argument;
    int x0 = rect->x0;
    int y0 = rect->y0;
    int x1 = rect->x1;
    int y1 = rect->y1;
    u64 *data = cursor->work.qwords;

    if (x1 < x0) {
        x1 = x0;
    }
    if (y1 < y0) {
        y1 = y0;
    }
    if (x0 < 0) {
        x0 = 0;
    } else if (x0 >= sRender.reso.frame_width) {
        x0 = sRender.reso.frame_width;
    }
    if (y0 < 0) {
        y0 = 0;
    } else if (y0 >= sRender.reso.frame_height) {
        y0 = sRender.reso.frame_height;
    }
    if (x1 >= sRender.reso.frame_width) {
        x1 = sRender.reso.frame_width;
    } else if (x1 < 0) {
        x1 = 0;
    }
    if (y1 >= sRender.reso.frame_height) {
        y1 = sRender.reso.frame_height;
    } else if (y1 < 0) {
        y1 = 0;
    }
    data[0] = GIF_TAG_ONE_AD_EOP;
    data[1] = GIF_AD;
    data[2] = GS_SET_SCISSOR(x0, x1 - 1, y0, y1 - 1);
    data[3] = GS_SCISSOR_1;
    sceVif1PkAddDirectDataN(cursor->packet, data, 2);
}
