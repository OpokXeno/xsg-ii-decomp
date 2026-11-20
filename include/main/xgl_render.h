#ifndef INCLUDE_MAIN_XGL_RENDER_H
#define INCLUDE_MAIN_XGL_RENDER_H

#include "main/gs_display.h"
#include "main/xgl_camera.h"
#include "shared.h"

/*
 * GS clear-environment block (ClearEnv, 0x006457C0, size 0x60). xglRenderClearDepth
 * and xglRenderClearFrame each write only the 8-byte tag at +0x10 (a
 * GS-register-shaped value selecting what a queued clear targets);
 * xglRenderClearColor splits a packed 0xAABBGGRR colour into the four
 * one-byte-per-word fields at +0x20..+0x2c. Bytes no claimed function
 * touches stay unmodeled.
 */
typedef struct GsClearEnv {
    u32 gif_tag_low;
    u32 gif_tag_high;
    u32 gif_registers_low;
    u32 gif_registers_high;
    u64 target;
    u64 target_address;
    u32 color_r;
    u32 color_g;
    u32 color_b;
    u32 color_a;
    u32 left;
    u32 top;
    unsigned char unmodeled_38[0x08];
    u32 right;
    u32 bottom;
    unsigned char unmodeled_48[0x08];
    u64 restore_target;
    u64 restore_target_address;
} GsClearEnv;

typedef union GsClearPacket {
    GsClearEnv value;
    AlignedVector4 quadwords[6];
} GsClearPacket;

extern GsClearPacket ClearEnv;

/*
 * GS display-environment block (0x004A9000, size 0x50). xglRenderDispOff/On
 * only touch the 8-byte flag word at +0x00 (bit 0x2 gates display output).
 */
typedef struct GsDispEnv {
    union {
        u64 flags;
        GsPmode pmode;
    };
    unsigned char unmodeled_08[8];
    GsDispfbRegister dispfb1;          /* +0x10 */
    GsDisplayRegister display1;        /* +0x18 */
    unsigned char unmodeled_20[8];
    GsDispfbRegister dispfb2;          /* +0x28 */
    GsDisplayRegister display2;        /* +0x30 */
    u64 extbuf;                        /* +0x38 */
    GsExtdataRegister extdata;         /* +0x40 */
    u64 extwrite;                      /* +0x48 */
} GsDispEnv;

extern GsDispEnv DispEnv;

/* Small-data scalars of xgl_render.c (.sdata): the pxy tick scale (1 doubles the tick), the VSync interval, the render loop count and
 * the scan line interpolation mode. */
extern signed char pxyTickScaleDouble;
extern signed char xglRenderVSyncInterval;
extern int LoopCount;
extern int ScanLineInterpolate;
/* Glyph digit strokes of the pxy debug draw (.data after the hooks). */
extern unsigned char pxyDigitStrokeTable[252];

/* Hook words of the data that follows sRender in xgl_render.c (arrays of one: a scalar would move into .sdata): the pxy hooks run per entry group and per frame copy, the
 * message frame hooks per frame, the effect hook by the frame effect and the pause hook while the pause screen draws. */
extern void (*pxyEntryGroupHooks[])(int);
extern void (*pxyFrameCopyHooks[])(XglPacket *);
extern void (*msgFrameHooks[])(void);
extern void (*gifStateEffectHook[])(XglPacket *);
extern void (*pauseDrawHook[])(XglPacket *);

/* Functions of src/main/xgl_render.c that other files call. */

void xglRenderInit(void);
void xglRenderAndMovieInit(void);
void xglRenderClearDepth(void);
void xglRenderClearFrame(void);
void xglRenderClearColor(u32 color);
void xglRenderFrame(void);
void xglRenderClearSkipOn(void);
void xglRenderClearSkipOff(void);
void xglRenderDispOn(void);
void xglRenderCpUDispOn(void);
void xglRenderCpUDispOff(void);
void xglRenderCopyDisp2Draw(void);
void xglRenderCopyDisp2Flip(void);
void xglRenderDrawFlipPk(XglPacket *packet);
void xglRenderGlobalFadeInit(void);
int xglRenderGlobalFadeGet(void);
void xglRenderGlobalFadeSet(float r, float g, float b, float a);
void xglRenderGlobalFadeSetPacked(int value);

#endif /* INCLUDE_MAIN_XGL_RENDER_H */
