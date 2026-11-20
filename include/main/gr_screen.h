#ifndef INCLUDE_MAIN_GR_SCREEN_H
#define INCLUDE_MAIN_GR_SCREEN_H

#include "main/font_draw_cursor.h"

#include "main/xgl_camera.h"

typedef void (*XglRenderPacketCallback)(XglPacket *packet);

/* One row of asDispReso: the display geometry sceGsSetDefDispEnv and the draw environment take. */
typedef struct XglRenderReso {
    short depth_psm;                   /* +0x00 */
    short frame_psm;                   /* +0x02: sRenderPsm */
    short frame_width;                 /* +0x04 */
    short frame_height;                /* +0x06 */
} XglRenderReso;

/* One row of asVramBase: the GS frame-buffer pages of a resolution. movie_player.c copies decoded movie frames from
 * the work buffer page to the display page. */
typedef struct XglRenderVram {
    unsigned short depth_base;         /* +0x08 */
    unsigned short unmodeled_0a[3];
    u16 display_page;                  /* +0x10: the page shown on screen (sRenderDispfbBase) */
    u16 draw_page;                     /* +0x12: the page the GS draws into (sRenderPrevDispfbBase) */
    unsigned short work_buffer_base;   /* +0x14: the third full-screen work page (sRenderFrameBase) */
    unsigned short unmodeled_16[2];
    unsigned short texture_base;       /* +0x1A: the texture area page (sRenderTextureBase), in 32-word units. The movie
                                          player (xgl_1.c) also uploads movie strips here. */
    unsigned short unmodeled_1c[2];
} XglRenderVram;

/* sRender (0x00645820): the render state of xgl_render.c, which fills it from asDispReso and asVramBase. The map names
 * sRenderPsm (+0x02), sRenderFrameBase (+0x14), sRenderDisplayBase (+0x20). The width and height at +0x04 and +0x06 are
 * the main frame size. Other files still read it through local views of their own (game_movie.c, game_defocus.c, ...). */
typedef struct XglRenderState {
    XglRenderReso reso;                /* +0x00 */
    XglRenderVram vram;                /* +0x08 */
    u16 display_buffer_base;           /* +0x20: sRenderDisplayBase */
    u16 draw_buffer_base;              /* +0x22 */
    /* +0x24: callback slots 0 to 7 of the final packet pass. xglRenderFinalPacket calls every non-null slot with the frame
     * packet. The loop runs over 17 slots: slots 12 to 16 follow this struct as globals in xgl_render.c. recordRenderAll
     * takes slot 6. */
    XglRenderPacketCallback final_packet_callback[8];
    /* +0x44: slot 8, ov01DrawAll while the battle scene runs (set by ov01SceneInit, cleared by ov01SceneEnd). */
    XglRenderPacketCallback battle_draw_callback;
    /* +0x48: slot 9, menuCallbackHook (xglCallbackQueueRun, menu_scene.c). */
    XglRenderPacketCallback menu_callback;
    /* +0x4c: slot 10, gifStateModeHook (one of gifStateEffectDraws, gifState.c). */
    XglRenderPacketCallback gif_mode_callback;
    /* +0x50: slot 11, fadeUpdateHook (fadeUpdate, fade_ctrl.c). It advances the fade colour. */
    XglRenderPacketCallback fade_update_callback;
} XglRenderState;

extern XglRenderState sRender;

/* ScissorSet's rectangle: x0, y0 (top left) and x1, y1 (bottom right) as the font code passes them. */
typedef struct ScissorRect {
    int x0;
    int y0;
    int x1;
    int y1;
} ScissorRect;

/* Functions of src/main/gr_screen.c that other files call. */
void grSetFrameBase(void);
void grSetRenderFrame(void);
void grResetRenderState(void);
void grDrawScreenSprite(int expand, unsigned char alpha);
void grDrawScreenFade(int fade);
void grCopyFrame(unsigned short dstBase, unsigned short srcBase);
void grFillFrame(void);
void grResetScissor(void);
void grSetScissor(int x0, int y0, int x1, int y1);
/* xglFontPrintExtFunc callbacks: queue a SCISSOR_1 write through the font draw cursor. ScissorSet takes a
 * ScissorRect as its argument; ScissorReset ignores it and restores the full frame. */
void ScissorSet(FontDrawCursor *cursor, void *argument);
void ScissorReset(FontDrawCursor *cursor, void *argument);

#endif /* INCLUDE_MAIN_GR_SCREEN_H */
