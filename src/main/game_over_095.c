#include "common.h"
#include "main/xgl_camera.h"
#include "shared.h"
#include "main/game_over_059.h"
#include "main/game_over_095.h"
#include "main/xgl_cd.h"
#include "main/xgl_jpeg.h"
#include "main/bg_layer.h"
#include "gs.h"
#include "main/control_entry.h"
#include "main/xgl_packet.h"
#include "main/gr_screen.h"
#include "main/pxy_packet_state.h"

/* Offset of the scratch area past the destination in gameOverLoadFile. */
#define GAMEOVER_SCRATCH_OFFSET 0xE0000
/* Q = 1.0f as raw bits. */
#define GS_Q_ONE 0x3F800000
/* Added to every sprite vertex (1/16 pixel units). */
#define GS_SPRITE_ORIGIN_X 0x7000
#define GS_SPRITE_ORIGIN_Y 0x71C0


void DrawImage_0020CAB0(u8 *framebuffer);
extern u64 TestEnv_0_003E7E30[12];
extern GameOverDmaPacket4 TransEnv_1_003E7E90;
extern GameOverDmaPacket6 FlushEnv_2_003E7EB0;
extern u8 bgLayerState0[4];

int bgLayerReady(int index)
{
    unsigned int done_flag;

    done_flag = bgLayerState0[index] & BG_LAYER_DONE;
    return done_flag != 0;
}


/* Draws the full-screen fade quad (the 512x448 sprite at the origin); the low byte of fade_value becomes the RGBAQ alpha. */
void gameOverDrawOverlay(u64 fade_value)
{
    nmlPacketGsInit();
    packet_gs_entry64(GS_TEST_1, GS_TEST_ZTE_ALWAYS);
    packet_gs_entry64(GS_ALPHA_1, (128ULL << 32) | 0x44);
    packet_gs_entry64(GS_RGBAQ, ((fade_value << 32) >> 8) | (0xFE00ULL << 46));
    packet_gs_entry64(GS_PRIM, GS_PRIM_SPRITE_ABE);
    packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(0x7000, 0x7200, 0));
    packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(0x9000, 0x8E00, 0));
    xglDirectQueueSubmitRef();
}

void *memset(void *dest, int value, unsigned int count);

/*
 * Reads the file at `path` into the scratch area 0xE0000 bytes past the
 * destination (the buffer, or WorkEnd when none is given), hands the read
 * target and the destination to xglJpegDecode and returns the destination.
 */
u8 *gameOverLoadFile(const char *path, u8 *buffer)
{
    u8 *base;
    u8 *read_target;
    JpegDecodeControl request;

    base = WorkEnd;
    if (buffer != 0) {
        base = buffer;
    }
    read_target = base + GAMEOVER_SCRATCH_OFFSET;
    xglCdReadFile(path, read_target, 0, CD_NOTIFY_DUMMY);
    memset(&request, 0, sizeof(request));
    request.source = read_target;
    request.destination = base;
    xglJpegDecode(&request);
    return base;
}

void DrawImage_0020CAB0(u8 *framebuffer)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    TestEnv_0_003E7E30[4] = ((u64)sRender.display_buffer_base << 0x25) | ((u64)0x8000 << 0x24);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv_0_003E7E30, 0x18);
    sceVif1PkRef(packet, TransEnv_1_003E7E90.words, 2, 0, 0, 0);
    sceVif1PkRef(packet, framebuffer, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, TransEnv_1_003E7E90.words, 2, 0, 0, 0);
    sceVif1PkRef(packet, framebuffer + 0x70000, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, FlushEnv_2_003E7EB0.words, 3, 0, 0, 0);
}


/* Draws a translucent sprite between the corners (x0, y0) and (x1, y1) in pixels, queued with a deferred submit. */
void gameOverDrawSprite(int x0, int y0, int x1, int y1)
{
    nmlPacketGsInit();
    packet_gs_entry64(GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
    packet_gs_entry64(GS_ZBUF_1, GS_SET_ZBUF(sRender.vram.depth_base, 1, 1));
    packet_gs_entry64(GS_RGBAQ, GS_SET_RGBAQ(0, 0x80, 0, 0x80, GS_Q_ONE));
    packet_gs_entry64(GS_ALPHA_1, GS_SET_ALPHA(0, 1, 2, 1, 0x40));
    packet_gs_entry64(GS_PRIM, GS_SET_PRIM(GS_PRIM_SPRITE, 0, 0, 0, 1, 0, 0, 0, 0));
    packet_gs_entry64(GS_XYZF2, GS_SET_XYZ((x0 << 4) + GS_SPRITE_ORIGIN_X, (y0 << 4) + GS_SPRITE_ORIGIN_Y, 0));
    packet_gs_entry64(GS_XYZF2, GS_SET_XYZ((x1 << 4) + GS_SPRITE_ORIGIN_X, (y1 << 4) + GS_SPRITE_ORIGIN_Y, 0));
    xglDirectQueueDefer(0, 4);
}

/* The DMA environment of the overlay's three framebuffer packets, in address order. */
u64 TestEnv_0_003E7E30[12] = { 0x0000000000000000ULL, 0x5100000500000000ULL, 0x1000000000000004ULL, 0x000000000000000eULL, 0x0000000000000000ULL, 0x0000000000000050ULL, 0x0000000000000000ULL, 0x0000000000000051ULL, 0x000001c000000200ULL, 0x0000000000000052ULL, 0x0000000000000000ULL, 0x0000000000000053ULL };
GameOverDmaPacket4 TransEnv_1_003E7E90 = { .words = { 0x0000000000000000ULL, 0x5100000100000000ULL, 0x0800000000007000ULL, 0x0000000000000000ULL } };
GameOverDmaPacket6 FlushEnv_2_003E7EB0 = { .words = { 0x0000000000000000ULL, 0x5100000200000000ULL, 0x1000000000008001ULL, 0x000000000000000eULL, 0x0000000000000000ULL, 0x000000000000003fULL } };
