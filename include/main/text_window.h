#ifndef INCLUDE_MAIN_TEXT_WINDOW_H
#define INCLUDE_MAIN_TEXT_WINDOW_H

#include "main/font_obj.h"

/* Functions of src/main/text_window.c that other files call. */

/* Text window: the box at the bottom of the screen and its single line of text. Its first member is the
 * position track fontTrackSetBoth initialises. */
typedef struct TextWindow {
    struct FontTrack track; /* +0x00: x and y are track.current_pair.x and [1], eased by fontTrackEase */
    int width;              /* +0x1C */
    int fade;               /* +0x20: frames of the fade-in, opaque from 8 on */
    unsigned char *text;
} TextWindow;

void textWindowReset(void);
/* Width in pixels of a markup string ($ codes set the zoom). */
int xglFontGetMarkupWidth(const unsigned char *text);
/* Shows a text box at (x, y) at least minWidth wide. The window keeps the text and textDrawString writes through it. */
void textWindowShow(int x, int y, int minWidth, unsigned char *text, int fadeBase);

#endif /* INCLUDE_MAIN_TEXT_WINDOW_H */
