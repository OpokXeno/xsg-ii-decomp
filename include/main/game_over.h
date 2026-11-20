#ifndef INCLUDE_MAIN_GAME_OVER_H
#define INCLUDE_MAIN_GAME_OVER_H

#include "shared.h"

/* Quadword (128-bit) type of the DMA packet unions. */
typedef unsigned int XglQuadword __attribute__((mode(TI)));
/* View of sRender (RenderFrameState of main/gr_screen.h) that game_over_095.c still uses for the depth buffer base at
 * +0x08, which RenderFrameState does not model yet; the other game over files read RenderFrameState. */

typedef union GameOverDmaPacket4 {
    u64 words[4];
    XglQuadword quadwords[2];
} GameOverDmaPacket4;

typedef union GameOverDmaPacket6 {
    u64 words[6];
    XglQuadword quadwords[3];
} GameOverDmaPacket6;

/* An image of the synopsis screen and its size in pixels and in quadwords. */
typedef struct GameSynopsisPanel {
    void *image;
    int width;
    int height;
    int qwc;
} GameSynopsisPanel;

/* Functions of src/main/game_over.c that other files call. */
void GameCopyFrame(void);
/* Presents the 512x448 image at framebuffer (nothing for 0) through the VIF1 packet. */
void DrawImage(void *framebuffer);
/* Draws one synopsis image at (x, y) with the given alpha (nothing for a missing image or alpha 0). */
void GameSynopsisDrawPanel(GameSynopsisPanel *panel, int x, int y, int alpha);
/* The game over / ending screen. */
void GameOver(void);
/* The intermission screen of the given chapter. */
void Intermission(int chapter);

#endif /* INCLUDE_MAIN_GAME_OVER_H */
