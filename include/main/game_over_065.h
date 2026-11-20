#ifndef INCLUDE_MAIN_GAME_OVER_065_H
#define INCLUDE_MAIN_GAME_OVER_065_H

#include "shared.h"

/* Functions of src/main/game_over_065.c that other files call. */

/* The first argument of tyaDisplaySetting is a mode or the framebuffer of the adjustment screen. */
#define TYA_DISPLAY_APPLY ((u8 *)0) /* DispEnv follows the saved display offset */
#define TYA_DISPLAY_RESET ((u8 *)1) /* clears the saved offset and the DispEnv shift */

/* The display adjustment screen (or, for a mode, applies or resets the saved offset).
 * A framebuffer needs the 0xE0000 bytes past it for the compressed image. */
void tyaDisplaySetting(u8 *framebuffer_or_mode);
/* DispEnv.display1 dx/dy = the base dx + 5 * horizontal, the base dy + vertical. */
void tyaSetDisplayOffset(int horizontal, int vertical);

#endif /* INCLUDE_MAIN_GAME_OVER_065_H */
