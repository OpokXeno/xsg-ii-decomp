#include "common.h"
#include "main/pxy_frame_alpha.h"
#include "main/pxy_packet_state.h"
#include "gs.h"

extern s16 cameraScreenWidth;
extern s16 cameraScreenHeight;
extern u16 sRenderDepthBase;
extern s16 sRenderPsm;

/* GS primitive coordinates are 12.4 fixed point around the 2048 origin; Q = 1.0f as raw bits. */
#define GS_ORIGIN 0x800
#define GS_Q_ONE 0x3F800000
/* XYZ2 vertex at (x, y) in pixels. */
#define XY(x, y) GS_SET_XYZ((x) << 4, (y) << 4, 0)
/* Half the screen size. */
#define SCREEN_HALF_WIDTH ((s16) (cameraScreenWidth / 2))
#define SCREEN_HALF_HEIGHT ((s16) (cameraScreenHeight / 2))

void pxyClearFrameAlphaMsb(u32 frameMask, u32 frameBase)
{
    u64 *p;
    u64 *q;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    q = p;
    GS_PUT(p, GS_FBA_1, 0);
    GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0x7FFFFFFF));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    xglDirectQueueCount += ((u32) p - (u32) q) >> 4;
    nmlPacketAddGsFlush();

    nmlPacketGsInit();
    q = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0, GS_Q_ONE));
    GS_PUT(q, GS_PRIM, 6);
    GS_PUT(q, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(q, GS_XYZF2, XY(GS_ORIGIN + SCREEN_HALF_WIDTH, GS_ORIGIN + SCREEN_HALF_HEIGHT));
    xglDirectQueueCount += ((u32) q - (u32) p) >> 4;
    nmlPacketAddGsFlush();

    nmlPacketGsInit();
    q = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, frameMask));
    GS_PUT(q, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
    xglDirectQueueCount += ((u32) q - (u32) p) >> 4;
    nmlPacketAddGsFlush();
}

void pxyFillFrameAlpha80(u32 frameMask, u32 frameBase)
{
    u64 *cursor;
    u64 *start;

    nmlPacketGsInit();
    cursor = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = cursor;
    GS_PUT(cursor, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
    GS_PUT(cursor, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0xFFFFFF));
    GS_PUT(cursor, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    GS_PUT(cursor, GS_RGBAQ, GS_SET_RGBAQ(0, 0, 0, 0x80, GS_Q_ONE));
    GS_PUT(cursor, GS_PRIM, GS_SET_PRIM(6, 0, 0, 0, 0, 0, 0, 0, 0));
    GS_PUT(cursor, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(cursor, GS_XYZF2, XY(GS_ORIGIN + SCREEN_HALF_WIDTH, GS_ORIGIN + SCREEN_HALF_HEIGHT));
    GS_PUT(cursor, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, frameMask));
    GS_PUT(cursor, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
    xglDirectQueueCount += ((u32) cursor - (u32) start) >> 4;
    nmlPacketAddGsFlushWide();
}

void pxyModulateFrameByAlpha(int unusedFrameBase, float unusedScale)
{
    u64 *p;
    u64 *q;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    q = p;
    GS_PUT(p, GS_ALPHA_1, GS_SET_ALPHA(1, 2, 1, 2, 0));
    GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, GS_Q_ONE));
    GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 0, 0, 1, 0, 0, 0, 0));
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN + SCREEN_HALF_WIDTH, GS_ORIGIN + SCREEN_HALF_HEIGHT));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
    xglDirectQueueCount += ((u32) p - (u32) q) >> 4;
    nmlPacketAddGsFlushWide();
}

void pxyClearDepthByFrameAlpha(u32 frameMask, u32 frameBase)
{
    u64 *p;
    u64 *q;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    q = p;
    GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 0));
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0xFFFFFFFF));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 1, 1, 1, 1));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0, 0, 0, 0, GS_Q_ONE));
    xglDirectQueueCount += ((u32) p - (u32) q) >> 4;
    nmlPacketAddGsFlush();

    nmlPacketGsInit();
    q = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_PRIM, 6);
    GS_PUT(q, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(q, GS_XYZF2, XY(GS_ORIGIN + SCREEN_HALF_WIDTH, GS_ORIGIN + SCREEN_HALF_HEIGHT));
    xglDirectQueueCount += ((u32) q - (u32) p) >> 4;
    nmlPacketAddGsFlush();

    nmlPacketGsInit();
    q = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, frameMask));
    GS_PUT(q, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
    xglDirectQueueCount += ((u32) q - (u32) p) >> 4;
    nmlPacketAddGsFlush();
}
