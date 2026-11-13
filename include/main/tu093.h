#ifndef INCLUDE_MAIN_TU093_H
#define INCLUDE_MAIN_TU093_H

/* sRender (0x00645820) as the main render code reads it (src/main/tu093.c, tu094.c, tu100.c). The map names
 * sRenderPsm (+0x02), sRenderFrameBase (+0x14), sRenderDisplayBase (+0x20). The width and height at +0x04 and
 * +0x06 are the main frame size; game_over_095.c and ov02 read other fields at +0x10..+0x20 (local views). */
typedef struct RenderFrameState {
    unsigned char unmodeled_00[2];
    short frame_psm;                /* +0x02 */
    short frame_width;              /* +0x04 */
    short frame_height;             /* +0x06 */
    unsigned char unmodeled_08[0x0C];
    unsigned short frame_base;      /* +0x14: sRenderFrameBase */
    unsigned char unmodeled_16[4];
    unsigned short frame_base_1;    /* +0x1A: the second page xgl_1_016 uploads movie strips to */
    unsigned char unmodeled_1c[4];
    unsigned short display_base;    /* +0x20: sRenderDisplayBase */
    unsigned short copy_base;       /* +0x22 */
    unsigned char unmodeled_24[0x84 - 0x24];
    unsigned char errorScreenActive; /* +0x84: set while xglHddErrorScreen owns the display */
} RenderFrameState;

extern RenderFrameState sRender;

#endif /* INCLUDE_MAIN_TU093_H */
