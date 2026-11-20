#include "common.h"
#include "main/xgl_camera.h"
#include "main/xgl_packet.h"
#include "main/gr_screen.h"
#include "shared.h"
#include "main/game_over_059.h"
#include "main/game_over_065.h"
#include "main/game_data.h"


void DrawImage_001EE618(XglQuadword *framebuffer);
extern u64 TestEnv_0_002BD4C0[12];
extern GameOverDmaPacket4 TransEnv_1_002BD520;
extern GameOverDmaPacket6 FlushEnv_2_002BD540;

void DrawImage_001EE618(XglQuadword *framebuffer)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    TestEnv_0_002BD4C0[4] = ((u64)sRender.display_buffer_base << 0x25) | ((u64)0x8000 << 0x24);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv_0_002BD4C0, 0x18);
    sceVif1PkRef(packet, TransEnv_1_002BD520.words, 2, 0, 0, 0);
    sceVif1PkRef(packet, framebuffer, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, TransEnv_1_002BD520.words, 2, 0, 0, 0);
    sceVif1PkRef(packet, framebuffer + 0x7000, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, FlushEnv_2_002BD540.words, 3, 0, 0, 0);
}

#include "main/xgl_thread.h"
#include "main/pad.h"
#include "main/se_bank_load.h"
#include "main/game_camera.h"
#include "main/xgl_mc_request.h"
#include "main/menu_draw.h"
#include "main/xgl_cd.h"
#include "main/xgl_font.h"
#include "main/xgl_jpeg.h"
#include "main/gs_display.h"
/* Sound effects played with seVoiceStartDefault (the ids game_over_059.c and cursor_buffer.c use). */
#define SE_DISPLAY_CURSOR_MOVE 0x01000001
#define SE_DISPLAY_DECIDE 0x01000002
#define SE_DISPLAY_RESET 0x01000002
#define SE_DISPLAY_CANCEL 0x01000008
#define TYA_DISPLAY_OFFSET_LIMIT 20

extern void *memset(void *dst, int value, unsigned int size);

/*
 * Display adjustment screen (tyaDisplaySetting in X1, src/main/tya_display_setting.c,
 * same image path and debug format strings): loads data\yajima\mnt_set.jpg into
 * the framebuffer, moves the picture with the pad and draws DrawImage_001EE618
 * each frame. The offset it applies is SaveData +0x32 (horizontal) and +0x34
 * (vertical), the same fields as X1.
 */
/* DispEnv (xgl_render.h) seen past its 0x50 bytes: +0x50 holds the base dx/dy the offset is measured from
 * (dispEnvTailWork[0] in xgl_render.c; see tyaSetDisplayOffset). The access has to go through DispEnv's own symbol. */
typedef struct TyaDisplayEnv {
    u8 unmodeled_00[0x18];
    GsDisplayRegister display1; /* +0x18 */
    u8 unmodeled_20[0x30];
    GsDisplayRegister base;     /* +0x50 */
} TyaDisplayEnv;

extern TyaDisplayEnv DispEnv;

/*
 * A display offset halfword seen as signed or unsigned. The soft-reset path restores
 * the horizontal offset through it: the union access (alias set 0) is what keeps that
 * store after the DispEnv update in the original schedule.
 */
typedef union TyaOffsetHalfword {
    short value;
    unsigned short bits;
} TyaOffsetHalfword;

void tyaDisplaySetting(u8 *framebuffer_or_mode)
{
    short saved_x;
    short saved_y;
    short horizontal;
    short vertical;

    saved_x = SaveData.displayOffsetX;
    saved_y = SaveData.displayOffsetY;

    if (framebuffer_or_mode == TYA_DISPLAY_APPLY) {
        DispEnv.display1.field.dx = DispEnv.base.field.dx + saved_x * 5;
        DispEnv.display1.field.dy = DispEnv.base.field.dy + saved_y;
        return;
    }
    if (framebuffer_or_mode == TYA_DISPLAY_RESET) {
        SaveData.displayOffsetX = 0;
        SaveData.displayOffsetY = 0;
        DispEnv.display1.field.dx = DispEnv.base.field.dx;
        DispEnv.display1.field.dy = DispEnv.base.field.dy;
        return;
    }
    {
        u8 *image_source = framebuffer_or_mode + 0xE0000;
        JpegDecodeControl request;

        do {
        } while (xglCdServiceIfPending() != 0);
        xglCdReadFile("yajima\\mnt_set.jpg", image_source, 0, CD_NOTIFY_DUMMY);
        memset(&request, 0, sizeof(request));
        request.source = image_source;
        request.destination = framebuffer_or_mode;
        xglJpegDecode(&request);
    }

    while (1) {
        if ((PadData[0].heldAndPressed & PAD_SOFT_RESET) == PAD_SOFT_RESET) {
            SaveData.displayOffsetY = saved_y;
            DispEnv.display1.field.dx = DispEnv.base.field.dx + saved_x * 5;
            DispEnv.display1.field.dy = DispEnv.base.field.dy + saved_y;
            ((TyaOffsetHalfword *)&SaveData.displayOffsetX)->value = saved_x;
            gameWork.control |= CONTROL_SOFT_RESET;
            break;
        }

        horizontal = SaveData.displayOffsetX;
        vertical = SaveData.displayOffsetY;

        if (PadData[0].directionRepeated & PAD_RIGHT) {
            horizontal++;
        }
        if (PadData[0].directionRepeated & PAD_LEFT) {
            horizontal--;
        }
        if (PadData[0].directionRepeated & PAD_DOWN) {
            vertical++;
        }
        if (PadData[0].directionRepeated & PAD_UP) {
            vertical--;
        }
        if (PadData[0].pressed & PAD_SQUARE) {
            horizontal = 0;
            vertical = 0;
            seVoiceStartDefault(SE_DISPLAY_RESET);
        }
        if (PadData[0].pressed & PAD_CROSS) {
            horizontal = saved_x;
            vertical = saved_y;
        }
        if (horizontal < -TYA_DISPLAY_OFFSET_LIMIT) {
            horizontal = -TYA_DISPLAY_OFFSET_LIMIT;
        }
        if (vertical < -TYA_DISPLAY_OFFSET_LIMIT) {
            vertical = -TYA_DISPLAY_OFFSET_LIMIT;
        }
        if (horizontal > TYA_DISPLAY_OFFSET_LIMIT) {
            horizontal = TYA_DISPLAY_OFFSET_LIMIT;
        }
        if (vertical > TYA_DISPLAY_OFFSET_LIMIT) {
            vertical = TYA_DISPLAY_OFFSET_LIMIT;
        }
        if (SaveData.displayOffsetX != horizontal || SaveData.displayOffsetY != vertical) {
            if (PadData[0].directionHeld & 0xF000) {
                seVoiceStartDefault(SE_DISPLAY_CURSOR_MOVE);
            }
        }

        SaveData.displayOffsetX = horizontal;
        SaveData.displayOffsetY = vertical;
        DispEnv.display1.field.dx = DispEnv.base.field.dx + horizontal * 5;
        DispEnv.display1.field.dy = DispEnv.base.field.dy + vertical;

        if (PadData[0].pressed & PAD_CIRCLE) {
            seVoiceStartDefault(SE_DISPLAY_DECIDE);
            break;
        }
        if (PadData[0].pressed & PAD_CROSS) {
            seVoiceStartDefault(SE_DISPLAY_CANCEL);
            break;
        }
        xglFontDebugPrintf(8, 8, "%04x %04x\n%04x %04x",
                           DispEnv.display1.field.dx, DispEnv.display1.field.dy,
                           SaveData.displayOffsetX, SaveData.displayOffsetY);
        DrawImage_001EE618((XglQuadword *)framebuffer_or_mode);
        xglSleep();
    }
    xglSleep();
}

/*
 * Applies the display offset to DispEnv: dx = the base dx + 5 * horizontal, dy =
 * the base dy + vertical (the mode-0 arithmetic of X1 tyaDisplaySetting).
 */
void tyaSetDisplayOffset(int horizontal, int vertical)
{
    DispEnv.display1.field.dx = DispEnv.base.field.dx + horizontal * 5;
    DispEnv.display1.field.dy = DispEnv.base.field.dy + vertical;
}

/* The DMA environment of the overlay's three framebuffer packets, in address order (DrawImage_001EE618 reads them). */
u64 TestEnv_0_002BD4C0[12] = { 0x0000000000000000ULL, 0x5100000500000000ULL, 0x1000000000000004ULL, 0x000000000000000eULL, 0x0000000000000000ULL, 0x0000000000000050ULL, 0x0000000000000000ULL, 0x0000000000000051ULL, 0x000001c000000200ULL, 0x0000000000000052ULL, 0x0000000000000000ULL, 0x0000000000000053ULL };
GameOverDmaPacket4 TransEnv_1_002BD520 = { .words = { 0x0000000000000000ULL, 0x5100000100000000ULL, 0x0800000000007000ULL, 0x0000000000000000ULL } };
GameOverDmaPacket6 FlushEnv_2_002BD540 = { .words = { 0x0000000000000000ULL, 0x5100000200000000ULL, 0x1000000000008001ULL, 0x000000000000000eULL, 0x0000000000000000ULL, 0x000000000000003fULL } };
