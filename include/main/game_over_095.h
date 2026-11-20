#ifndef INCLUDE_MAIN_GAME_OVER_095_H
#define INCLUDE_MAIN_GAME_OVER_095_H

#include "shared.h"

/* Functions of src/main/game_over_095.c that other files call. */

/* Nonzero once the background layer at index has finished (BG_LAYER_DONE). */
int bgLayerReady(int index);
/* Draws the full-screen fade quad; the low byte of fade_value is its alpha. */
void gameOverDrawOverlay(u64 fade_value);

#endif /* INCLUDE_MAIN_GAME_OVER_095_H */
