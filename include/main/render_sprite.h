#ifndef INCLUDE_MAIN_RENDER_SPRITE_H
#define INCLUDE_MAIN_RENDER_SPRITE_H

#include "shared.h"

/* Functions of src/main/render_sprite.c that other files call. */

/* Draws a five-pass triangle strip of fillColour from (x0, y0) to (x1, y1), with transparent outer columns at both ends. */
void renderFadeBand(int x0, int y0, int x1, int y1, u32 fillColour);
void renderSprite(int x, int y, int width, int height, int alpha);
void renderSpriteClut(int x, int y, int width, int height, int clutOffset);
void renderSpriteClutAlpha(int x, int y, int width, int height, int clutOffset, int alpha);

#endif /* INCLUDE_MAIN_RENDER_SPRITE_H */
