#include "common.h"
#include "main/font_obj.h"
#include "main/text_window.h"
#include "main/xgl_font.h"

/* Font escape byte that opens a two-byte character in the scratch string. */
#define FONT_ESCAPE 0x19
/* Zoom 1.0 in xglFontZoom (16 = 1.0). */
#define FONT_ZOOM_ONE 16
/* Frame width of the text box around its text, in the same units as the width. */
#define TEXT_WINDOW_FRAME_WIDTH 24

/* Scratch string the width routine measures: 0x19 0x03 (the font escape), the character, then the terminator. */
static char xglFontScratchString[8];
/* Scale of the markup width, 16 is 1.0; set by the $zoom and $init codes. */
extern int xglFontZoom;

extern TextWindow textWindow;

/* Width of a markup string. A '$' code ("$init", "$zoom,N;") sets the zoom and the text up to its ';' is skipped;
 * other characters are measured one at a time through the scratch string. */
int xglFontGetMarkupWidth(const unsigned char *text)
{
    int width = 0;
    int in_code = 0;

    while (*text != 0) {
        if (*text >= 0x80) {
            xglFontScratchString[0] = FONT_ESCAPE;
            xglFontScratchString[1] = 3;
            xglFontScratchString[2] = text[0];
            xglFontScratchString[3] = text[1];
            text++;
            xglFontScratchString[4] = 0;
            width += (xglFontGetStringWidth(xglFontScratchString) * xglFontZoom) >> 4;
        } else if (in_code) {
            if (*text == ';') {
                in_code = 0;
            }
        } else if (*text == '$') {
            switch (text[1]) {
            case 'z':
                if (text[2] == 'o' && text[3] == text[2] && text[4] == 'm') {
                    if (text[6] == ',' || text[6] == ';' || text[6] == 0) {
                        xglFontZoom = text[5] - '0';
                    } else {
                        xglFontZoom = (text[5] - '0') * 10 + (text[6] - '0');
                    }
                }
                break;
            case 'i':
                if (text[2] == 'n' && text[3] == text[1] && text[4] == 't') {
                    xglFontZoom = FONT_ZOOM_ONE;
                }
                break;
            }
            in_code = 1;
        } else if (*text >= 0x20) {
            xglFontScratchString[0] = FONT_ESCAPE;
            xglFontScratchString[1] = 3;
            xglFontScratchString[2] = *text;
            xglFontScratchString[3] = 0;
            width += (xglFontGetStringWidth(xglFontScratchString) * xglFontZoom) >> 4;
        }
        text++;
    }
    return width;
}

void textWindowReset(void)
{
    fontTrackSetBoth(&textWindow.track, 0, 0);
    textWindow.width = 0;
    textWindow.fade = 0;
    textWindow.text = 0;
}

/* Shows a text box: sizes it to the markup text (at least minWidth), aims its track at (x, y) and keeps the text and the
 * fade base. Without text the box keeps the bare 24-unit frame. */
void textWindowShow(int x, int y, int minWidth, unsigned char *text, int fadeBase)
{
    if (text != 0) {
        xglFontZoom = FONT_ZOOM_ONE;
        textWindow.width = xglFontGetMarkupWidth(text) + TEXT_WINDOW_FRAME_WIDTH;
    } else {
        textWindow.width = TEXT_WINDOW_FRAME_WIDTH;
    }
    if (textWindow.width < minWidth) {
        textWindow.width = minWidth;
    }
    fontTrackSetBoth(&textWindow.track, -(textWindow.width * 2), y);
    fontTrackSetTarget(&textWindow.track, x, y);
    textWindow.fade = fadeBase + 8;
    textWindow.text = text;
}
