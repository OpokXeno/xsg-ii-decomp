#include "common.h"
#include "main/pad.h"
#include "main/menu_draw.h"
#include "main/cursor_buffer.h"
#include "main/text_box.h"
#include "main/menu_scene.h"
#include "main/xgl_cd.h"
#include "ov01/calc.h"
#include "main/menu_selection.h"
#include "main/xgl_tick.h"
#include "main/font_obj.h"
#include "main/pt_member.h"
#include "main/se_bank_load.h"
#include "ov01/data_box.h"
#include "main/xgl_font.h"
#include "ov01/data_record_get.h"
#include "main/xgl_cd_arena.h"
#include "main/save_party_init.h"
#include "main/xgl_2.h"

/* The two-choice (yes/no) prompt at main 0x006a57d8. highlight is the track of the moving highlight
 * band: current_pair.x its x, current_pair.y the highlighted row's y (choiceUpdate keeps it as the
 * target y). state is the selected option (0 or 1, 2 once closed); buffer, firstOptionText and
 * secondOptionText are the prompt text and the two option labels drawn by choiceDraw. */
typedef struct ChoiceState {
    struct FontTrack highlight;             /* 0x00 */
    struct FontTrack fade;                  /* 0x1C: current_pair.x is the opacity choiceDraw applies */
    unsigned char state;                    /* 0x38 */
    unsigned char unmodeled_39[0x3C - 0x39];
    unsigned char *buffer;                  /* 0x3C */
    void *firstOptionText;                  /* 0x40: the word choiceFirstOptionText names */
    void *secondOptionText;                 /* 0x44: the word choiceSecondOptionText names */
} ChoiceState;

/* The countdown object at main 0x006a5820. countdownInit sets count, countdownTick counts it down
 * (a pad button zeroes it), countdownDraw draws text with fade.current_pair.x as its opacity. */
typedef struct CountdownState {
    int count;
    struct FontTrack fade;              /* 0x04 */
    unsigned char *text;                /* 0x20 */
    unsigned char unmodeled_24[0x60 - 0x24]; /* the object spans 0x6a5820..0x6a5880; nothing reads past +0x24 */
} CountdownState;

struct FontDrawItem packetRequest = { 0 };
char textDigits[16] = "0123456789";
int menuCardPanelFrame[10] = { 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3 };
int menuCardSelectedPanelFrame[10] = { 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc };
int menuPanelFrame[10] = { 0x116, 0x117, 0x118, 0x119, 0x11a, 0x11b, 0x11c, 0x11d, 0x11e };
int menuInfoPanelFrame[20] = { 0x105, 0x106, 0x107, 0xff, 0x100, 0x101, 0x102, 0x103, 0x104, 0x0, 0x117, 0x117, 0x117, 0x11a, 0x11a, 0x11a, 0x11d, 0x11d, 0x11d };
int textDrawColor = 0xFFFFF0;
int textCursorY = 0;
signed char textCharSpacing = 4;
unsigned char msgGlyphMeasureBuffer[8] = { 0x19, 0x03 };
/* Style record of msgRangeDrawRevealed: 8 bytes, the layout of msgStyleReset below. */
unsigned char msgRevealFadeStyle[8] = { 0x0f, 0x44, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00 };

static signed char configValueState[8];
static char textDecimalBuffer[16];
static char textSignedDecimalBuffer[16];
static StatBlock charStatsSavedRecord;
static struct FontTrack panelPositionTrack;
static struct FontTrack panelSizeTrack;
static struct FontTrack panelTabTrack;
static struct FontTrack menuCursorPos;
static struct FontTrack menuCursorSize;
static unsigned char msgBuffer[256];
static ChoiceState choiceState;
static CountdownState countdownState;

/* Font draw state word; the draw call in textDrawSegment takes it as its third argument. */

void textColorDefaultSet(void)
{
    textDrawColor = 0xFFFFF0;
}

/* Font control tags textDrawSegment prints before a segment (defined after it): the colour
 * tag whose two wave bytes it rewrites and the alpha tag whose fourth byte it rewrites. */
extern unsigned char textPulseColorTag[8];
extern unsigned char textAlphaTag[8];
/* Closing tags of the colour and alpha tags. */

const unsigned char textColorCloseTag[16] = "$colc0c0c0;";

const unsigned char textAlphaCloseTag[32] = "$alpha0,0,0,0,0,0x80;";

/* One text segment: optional colour and alpha control tags, then the draw call.
 * flags bit 0 pulses the colour, bit 1 halves the alpha, bit 2 scales it by the font opacity. */
void textDrawSegment(int x, int y, const char *text, int flags)
{
    unsigned char alpha;

    if (text == 0) {
        return;
    }
    xglFontPrint(0, 0, textDrawColor, "\031\003");
    if (flags & 1) {
        textPulseColorTag[2] = textPulseColorTag[3] = xglTickWave(32.0f, 24.0f) + 0x68;
        xglFontPrint(0, 0, textDrawColor, textPulseColorTag);
    }
    alpha = (flags & 2) ? 0x40 : 0x80;
    if (flags & 4) {
        alpha = (alpha * fontObjGetOpacity()) >> 7;
    }
    if ((signed char)alpha >= 0) {
        textAlphaTag[3] = alpha;
        xglFontPrint(0, 0, textDrawColor, textAlphaTag);
    }
    xglFontPrint(x, y, textDrawColor, text);
    if (flags & 1) {
        xglFontPrint(0, 0, textDrawColor, textColorCloseTag);
    }
    if ((signed char)alpha >= 0) {
        xglFontPrint(0, 0, textDrawColor, textAlphaCloseTag);
    }
}

unsigned char textPulseColorTag[8] = { 0x0C, 0xC0, 0xC0, 0xC0 };
unsigned char textAlphaTag[8] = { 0x0F, 0x44, 0x00, 0x80 };

/* Multi-line text: a backslash-n escape starts a new line, textCursorY is the y cursor. */

int textDrawLines(int x, int y, unsigned char *text, int flags)
{
    unsigned char *first = text;
    int lines;

    textCursorY = (y >= 0) ? y : textCursorY;
    lines = 0;
    /* "\\n" escapes become a newline and the BEL byte 7; the byte is re-read after the rewrite. */
    while (*text != 0) {
        if (*text == '\\' && text[1] == 'n') {
            text[0] = '\n';
            text[1] = 7;
            lines++;
        } else if (*text == '\n') {
            lines++;
        }
        if ((signed char)*text < 0) {
            text++;
        }
        text++;
    }
    textDrawSegment(x, textCursorY, first, flags);
    textCursorY += lines * 20;
    textCursorY += 30;
    return textCursorY;
}

/* Returns nothing: textDrawDigits and textDrawDigitsWideAlt reload packetRequest.x after
   the call instead of using a result. */

/* Fills the request's cell, position and flags (FONT_DRAW_USE_SIZE and FONT_DRAW_OFFSET
   cleared), then submits it. */
void menuDrawCell(int cell, int x, int y, int flags)
{
    packetRequest.cell_index = cell;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = flags & ~0x28;
    fontObjDraw(&packetRequest);
}

/* Like menuDrawCell with a draw size: FONT_DRAW_USE_SIZE set, FONT_DRAW_OFFSET cleared. */
void menuDrawCellSized(int cell, int x, int y, int width, int height, int flags)
{
    packetRequest.cell_index = cell;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.size_w = width;
    packetRequest.size_h = height;
    packetRequest.flags = (flags | 8) & ~0x20;
    fontObjDraw(&packetRequest);
}


/* Glyph strips of textDrawGlyphString: the request command of the first draw of each class,
 * and the cell (strip) that class's face is drawn from. */
#define GLYPH_CMD_UPPER 0x12
#define GLYPH_CMD_LOWER 0x13
#define GLYPH_CMD_DIGIT 0x14
#define GLYPH_CMD_PERIOD 0x15
#define GLYPH_CMD_PLUS 0x4C
#define GLYPH_CELL_UPPER 0xA0
#define GLYPH_CELL_LOWER 0x16
#define GLYPH_CELL_DIGIT 0x17
#define GLYPH_CELL_PERIOD 0x20
#define GLYPH_CELL_PLUS 0x4B
/* Glyph index inside the strip for symbols. */
#define GLYPH_INDEX_HYPHEN 0x1A
#define GLYPH_INDEX_SLASH 0xA
#define GLYPH_INDEX_COLON 0xB
#define GLYPH_SHADOW_OFFSET 4
#define GLYPH_FLAGS_SHADOW 0x24
#define GLYPH_FLAGS_FACE 0x64

/* Text run: maps each character to a glyph cell (capitals, small letters, digits and a few
   symbols), draws it twice (shadow in `colour` shifted up-left, then the current font colour)
   and advances x by the cell width with per-glyph kerning. Once y is stored, the parameter is
   reused as the glyph index inside the cell row. */
void textDrawGlyphString(int x, int y, const unsigned char *text, unsigned long long colour)
{
    unsigned long long fontColour = fontObjGetColor();
    const unsigned char *p;
    int cell;
    struct FontDrawItem *request;

    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.offset_y = 0;
    /* Original walks a copy of the parameter. */
    p = text;
    while (*p != 0) {
        if ((unsigned char) (*p - 'A') < 26) {
            packetRequest.cell_index = GLYPH_CMD_UPPER;
            cell = GLYPH_CELL_UPPER;
            y = *p - 'A';
        } else if ((unsigned char) (*p - 'a') < 26) {
            packetRequest.cell_index = GLYPH_CMD_LOWER;
            cell = GLYPH_CELL_LOWER;
            y = *p - 'a';
        } else if ((unsigned char) (*p - '0') < 10) {
            packetRequest.cell_index = GLYPH_CMD_DIGIT;
            cell = GLYPH_CELL_DIGIT;
            y = *p - '0';
        } else {
            switch (*p) {
            case ' ':
                packetRequest.x += textCharSpacing;
                p++;
                continue;
            case '-':
                packetRequest.cell_index = GLYPH_CMD_LOWER;
                cell = GLYPH_CELL_LOWER;
                y = GLYPH_INDEX_HYPHEN;
                break;
            case '/':
                packetRequest.cell_index = GLYPH_CMD_DIGIT;
                cell = GLYPH_CELL_DIGIT;
                y = GLYPH_INDEX_SLASH;
                break;
            case ':':
                packetRequest.cell_index = GLYPH_CMD_DIGIT;
                cell = GLYPH_CELL_DIGIT;
                y = GLYPH_INDEX_COLON;
                break;
            case '.':
                packetRequest.cell_index = GLYPH_CMD_PERIOD;
                cell = GLYPH_CELL_PERIOD;
                y = 0;
                packetRequest.x -= 2;
                break;
            case '+':
                packetRequest.cell_index = GLYPH_CMD_PLUS;
                cell = GLYPH_CELL_PLUS;
                y = 0;
                break;
            default:
                p++;
                continue;
            }
        }
        switch (*p) {
        case 'I':
        case 'i':
        case 'l':
        case 'r':
            packetRequest.x -= 1;
            break;
        case 'm':
        case 'w':
            packetRequest.x += 1;
            break;
        }
        /* Original code takes the address twice (here and before the face draw). */
        request = &packetRequest;
        request->offset_x = y * fontObjCellOffsetX(request->cell_index);
        request->x -= GLYPH_SHADOW_OFFSET;
        request->y -= GLYPH_SHADOW_OFFSET;
        fontObjSetColor(colour);
        request->flags = GLYPH_FLAGS_SHADOW;
        fontObjDraw(request);
        request->x += GLYPH_SHADOW_OFFSET;
        request->y += GLYPH_SHADOW_OFFSET;
        if (*p == '.') {
            request->x += 2;
        }
        if (*p == 'Z') {
            request->cell_index = cell + 1;
            request->offset_x = 0;
        } else {
            request->cell_index = cell;
            request->offset_x = y * fontObjCellOffsetX(cell);
        }
        request = &packetRequest;
        fontObjSetColor(fontColour);
        request->flags = GLYPH_FLAGS_FACE;
        fontObjDraw(request);
        request->x += fontObjCellOffsetX(cell) - 1;
        if (cell == GLYPH_CELL_DIGIT || cell == GLYPH_CELL_PLUS) {
            request->x += 3;
        }
        switch (*p) {
        case '.':
            packetRequest.x -= 3;
            break;
        case 'I':
        case 'f':
        case 'i':
        case 'j':
        case 'l':
        case 't':
            packetRequest.x -= 1;
            break;
        case 'm':
        case 'w':
            packetRequest.x += 1;
            break;
        }
        p++;
    }
}

/* Two textDrawGlyphString runs, each with its own up-left layer in `colour`: the first at
   (x + 1, y + 3) with the font colour 0x3F80000080000000 (RGB 0, alpha 0x80) as its face, the
   second at (x, y) with the previous font colour. */
void textDrawShadow(int x, int y, const unsigned char *text, unsigned long long colour)
{
    unsigned long long prevColour;

    prevColour = fontObjGetColor();
    fontObjSetColor(0x3F80000080000000ULL);
    textDrawGlyphString(x + 1, y + 3, text, colour);
    fontObjSetColor(prevColour);
    textDrawGlyphString(x, y, text, colour);
}

/* Text run with the second glyph bank (kinds 0x7A to 0x7F): like textDrawGlyphString, the shadow
   in `colour` 2 pixels up-left, then the face in the current font colour. Small letters, digits
   and most symbols sit `drop` pixels lower. */
void textDrawStringAlt(int x, int y, const unsigned char *text, u64 colour)
{
    unsigned long long fontColour = fontObjGetColor();
    const unsigned char *p;
    int cell;
    int drop;
    struct FontDrawItem *request;

    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.offset_y = 0;
    p = text;
    drop = 0;
    while (*p != 0) {
        if ((unsigned char) (*p - 'A') < 26) {
            packetRequest.cell_index = 0x7B;
            cell = 0x7A;
            y = *p - 'A';
        } else if ((unsigned char) (*p - 'a') < 26) {
            packetRequest.cell_index = 0x7D;
            cell = 0x7C;
            drop = 2;
            y = *p - 'a';
        } else if ((unsigned char) (*p - '0') < 10) {
            packetRequest.cell_index = 0x7F;
            cell = 0x7E;
            drop = 2;
            y = *p - '0';
        } else {
            switch (*p) {
            case ' ':
                packetRequest.x += textCharSpacing;
                p++;
                continue;
            case '-':
                packetRequest.cell_index = 0x7F;
                cell = 0x7E;
                y = 0xD;
                drop = 1;
                break;
            case '/':
                packetRequest.cell_index = 0x7F;
                cell = 0x7E;
                y = GLYPH_INDEX_SLASH;
                drop = 2;
                break;
            case ':':
                packetRequest.cell_index = 0x7F;
                cell = 0x7E;
                y = GLYPH_INDEX_COLON;
                drop = 2;
                break;
            case '.':
                packetRequest.cell_index = 0x7D;
                cell = 0x7C;
                y = 0x1B;
                drop = 2;
                break;
            case '+':
                packetRequest.cell_index = 0x7F;
                cell = 0x7E;
                y = 0xC;
                drop = 2;
                break;
            default:
                p++;
                continue;
            }
        }
        switch (*p) {
        case 'I':
        case 'l':
            packetRequest.x -= 1;
            break;
        case '.':
        case 'i':
            packetRequest.x -= 2;
            break;
        }
        request = &packetRequest;
        request->offset_x = y * fontObjCellOffsetX(request->cell_index);
        request->x -= 2;
        request->y += drop - 2;
        fontObjSetColor(colour);
        request->flags = GLYPH_FLAGS_SHADOW;
        fontObjDraw(request);
        request->x += 2;
        request->y += 2;
        request->cell_index = cell;
        request->offset_x = y * fontObjCellOffsetX(cell);
        fontObjSetColor(fontColour);
        request->flags = GLYPH_FLAGS_FACE;
        fontObjDraw(request);
        request->x += fontObjCellOffsetX(cell);
        if (cell == 0x7E) {
            request->x += 1;
        }
        request->y -= drop;
        drop = 0;
        switch (*p) {
        case '.':
            request->x -= 2;
            break;
        case 'I':
        case 'i':
        case 't':
            packetRequest.x -= 1;
            break;
        }
        p++;
    }
}

/* Shadowed text run over the second glyph bank. */
void textDrawShadowAlt(int x, int y, const unsigned char *text, unsigned long long colour)
{
    unsigned long long prevColour;

    prevColour = fontObjGetColor();
    fontObjSetColor(0x3F80000080000000ULL);
    textDrawStringAlt(x + 1, y + 3, text, colour);
    fontObjSetColor(prevColour);
    textDrawStringAlt(x, y, text, colour);
}

/* Text run whose request flags and glyph kinds depend on windowLayer: flags 0x10034 when it is
   nonzero (menuDrawTitledWindow passes 1; its window pieces all carry the 0x10000 bit), else 0x74. Glyph kinds
   start at 0x2BA with windowLayer, else 0x18: capitals, then +1 small letters, +2 digits (the
   strip starts at '1'), +3 the symbols '-', '/' and '.' ('.' sets flag bit 0x2000 for its two
   draws). Each glyph is drawn in colour 0x3F80000060000000 one pixel right and two down, then in
   the reset colour. */
void textDrawTitle(int windowLayer, int x, int y, const unsigned char *text)
{
    const unsigned char *p = text;
    int command;
    int glyph;

    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 0x74;
    packetRequest.offset_y = 0;
    if (windowLayer != 0) {
        packetRequest.flags = 0x10034;
    }
    while (*p != 0) {
        command = windowLayer ? 0x2BA : 0x18;
        packetRequest.cell_index = command;
        if ((unsigned char) (*p - 'A') < 26) {
            glyph = *p - 'A';
        } else if ((unsigned char) (*p - 'a') < 26) {
            packetRequest.cell_index = command + 1;
            glyph = *p - 'a';
        } else if ((unsigned char) (*p - '0') < 10) {
            packetRequest.cell_index = command + 2;
            glyph = *p - '0' + 9;
            glyph %= 10; /* the strip starts at '1': '0' is its last cell, 9 */
        } else {
            /* default comes first: listed last, the '-' block is cross-jumped into the '/'
               tail, which the original does not do. */
            switch (*p) {
            default:
                p++;
                continue;
            case ' ':
                packetRequest.x += textCharSpacing;
                p++;
                continue;
            case '-':
                packetRequest.cell_index = command + 3;
                glyph = 2;
                break;
            case '/':
                packetRequest.cell_index = command + 3;
                glyph = 1;
                break;
            case '.':
                packetRequest.cell_index = command + 3;
                glyph = 0;
                packetRequest.flags |= 0x2000;
                break;
            }
        }
        switch (*p) {
        case 'I':
        case 'i':
        case 'l':
            packetRequest.x -= 1;
            break;
        case 'm':
            packetRequest.x += 1;
            break;
        }
        packetRequest.offset_x = glyph * fontObjCellOffsetX(packetRequest.cell_index);
        fontObjSetColor(0x3F80000060000000ULL);
        packetRequest.x += 1;
        packetRequest.y += 2;
        fontObjDraw(&packetRequest);
        fontObjResetColor();
        packetRequest.x -= 1;
        packetRequest.y -= 2;
        fontObjDraw(&packetRequest);
        packetRequest.flags &= ~0x2000;
        packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index) - 2;
        if ((unsigned char) (*p - '0') < 10) {
            packetRequest.x += 2;
        }
        if ((unsigned char) (*p - 'A') < 26) {
            packetRequest.x += 1;
        }
        switch (*p) {
        case 'I':
        case 'f':
        case 'i':
        case 'j':
        case 'l':
        case 'r':
        case 't':
            packetRequest.x -= 1;
            /* no break: these letters also take the +1 of case 'm' */
        case 'm':
            packetRequest.x += 1;
            break;
        }
        p++;
    }
}

/* Formats an integer in decimal into a static buffer with width and zero-pad flag; returns the buffer. */
char *textFormatDecimal(int value, int digits, int zeroPad)
{
    int i;
    int done;

    textDecimalBuffer[digits] = 0;
    done = 0;
    for (i = digits - 1; i >= 0; i--) {
        if (done) {
            textDecimalBuffer[i] = zeroPad ? '0' : ' ';
            continue;
        }
        textDecimalBuffer[i] = textDigits[value % 10];
        value /= 10;
        done = value == 0;
    }
    return textDecimalBuffer;
}

/* Decimal number at (x, y): width 1 to 9, 9-pixel character spacing, space padding. */
void textDrawDecimal(int x, int y, int value, int digits, u64 colour)
{
    char *text;

    if (digits > 0) {
        if (digits < 10) {
            textCharSpacing = 9;
            text = textFormatDecimal(value, digits, 0);
            textDrawGlyphString(x, y, text, colour);
            textCharSpacing = 4;
        }
    }
}

/* textDrawDecimalAlt variant: second glyph bank, width up to 10, 7-pixel spacing. */
void textDrawDecimalAlt(int x, int y, int value, int digits, u64 colour)
{
    char *text;

    if (digits > 0) {
        if (digits < 11) {
            textCharSpacing = 7;
            text = textFormatDecimal(value, digits, 0);
            textDrawStringAlt(x, y, text, colour);
            textCharSpacing = 4;
        }
    }
}

/* textDrawDecimal with zero padding. */
void textDrawDecimalZero(int x, int y, int value, int digits, u64 colour)
{
    char *text;

    if (digits > 0) {
        if (digits < 10) {
            textCharSpacing = 9;
            text = textFormatDecimal(value, digits, 1);
            textDrawGlyphString(x, y, text, colour);
            textCharSpacing = 4;
        }
    }
}

/* Decimal number through the shadowed text run. */
void textDrawDecimalShadow(int x, int y, int value, int digits, unsigned long long color)
{
    char *text;

    if (digits > 0) {
        if (digits < 10) {
            textCharSpacing = 9;
            text = textFormatDecimal(value, digits, 0);
            textDrawShadow(x, y, text, color);
            textCharSpacing = 4;
        }
    }
}

/* textDrawDecimalAlt through the shadowed second-bank run. */
void textDrawDecimalAltShadow(int x, int y, int value, int digits, u64 colour)
{
    char *text;

    if (digits > 0) {
        if (digits < 11) {
            textCharSpacing = 7;
            text = textFormatDecimal(value, digits, 0);
            textDrawShadowAlt(x, y, text, colour);
            textCharSpacing = 4;
        }
    }
}

/* textDrawDecimalShadow with zero padding. */
void textDrawDecimalShadowZero(int x, int y, int value, int digits, u64 colour)
{
    char *text;

    if (digits > 0) {
        if (digits < 10) {
            textCharSpacing = 9;
            text = textFormatDecimal(value, digits, 1);
            textDrawShadow(x, y, text, colour);
            textCharSpacing = 4;
        }
    }
}

/* Signed decimal through the shadowed text run: width 1 to 9 plus a '+' or '-' in front of the
   digits, space padding, drawn in a fixed colour. The digits are textDigits[value % 10] of the
   value as passed (no magnitude is taken): only a value >= 0 indexes the "0123456789" table. */
void textDrawSignedDecimalShadow(int x, int y, int value, int digits, u64 colour)
{
    int sign;
    int i;
    int state;

    sign = value;
    if (digits > 0) {
        if (digits < 10) {
            digits++;
            textSignedDecimalBuffer[digits] = 0;
            state = 0;
            for (i = digits - 1; i >= 0; i--) {
                if (state != 0) {
                    if (state == 1) {
                        textSignedDecimalBuffer[i] = sign >= 0 ? '+' : '-';
                        state = 2;
                    } else {
                        textSignedDecimalBuffer[i] = ' ';
                    }
                } else {
                    textSignedDecimalBuffer[i] = textDigits[value % 10];
                    value /= 10;
                    state = value == 0;
                }
            }
            textCharSpacing = 9;
            fontObjSetColor(0x3F800000806C7F4BULL);
            textDrawShadow(x, y, textSignedDecimalBuffer, colour);
            fontObjResetColor();
            textCharSpacing = 4;
        }
    }
}

/* Decimal digits at (x, y), 9-pixel pitch, glyph kind 0x1D: drawn right to left, digits taken
   from the value by repeated division by 10; once the value is used up nothing more is drawn. */
void textDrawDigits(int x, int y, int value, int digits)
{
    int i;
    int digit;

    if (digits > 0) {
        packetRequest.x = x + digits * 9 - 9;
        packetRequest.y = y;
        packetRequest.cell_index = 0x1D;
        packetRequest.flags = 0x60;
        packetRequest.offset_y = 0;
        for (i = 0; i < digits; i++) {
            if (value > 0) {
                digit = value % 10;
                value /= 10;
                packetRequest.offset_x = (digit + 9) % 10 * 9;
                fontObjDraw(&packetRequest);
                packetRequest.x -= 9;
            }
        }
    }
}

/* textDrawDigits without the stop at a used-up value: zeros are drawn up to the full width. */
void textDrawDigitsZero(int x, int y, int value, int digits)
{
    int i;
    int digit;

    if (digits > 0) {
        packetRequest.x = x + digits * 9 - 9;
        packetRequest.y = y;
        packetRequest.cell_index = 0x1D;
        packetRequest.flags = 0x60;
        packetRequest.offset_y = 0;
        for (i = 0; i < digits; i++) {
            digit = value % 10;
            value /= 10;
            packetRequest.offset_x = (digit + 9) % 10 * 9;
            fontObjDraw(&packetRequest);
            packetRequest.x -= 9;
        }
    }
}

/* Two digit runs of the same width joined by one separator glyph (phase 0x63 of kind 0x1D). */
void textDrawDigitsPair(int x, int y, int leftValue, int rightValue, int digits)
{
    textDrawDigits(x, y, leftValue, digits);
    x += digits * 9;
    packetRequest.offset_x = 0x63;
    packetRequest.cell_index = 0x1D;
    packetRequest.flags = 0x60;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.offset_y = 0;
    fontObjDraw(&packetRequest);
    textDrawDigits(x + 9, y, rightValue, digits);
}

/* Digit run in glyph kind 0x27B with a 12-pixel pitch, drawn one pixel higher; zeros up to the
   full width. */
void textDrawDigitsWide(int x, int y, int value, int digits)
{
    int i;
    int digit;

    if (digits > 0) {
        packetRequest.x = x + digits * 12 - 12;
        packetRequest.y = y - 1;
        packetRequest.cell_index = 0x27B;
        packetRequest.flags = 0x21;
        packetRequest.offset_y = 0;
        for (i = 0; i < digits; i++) {
            digit = value % 10;
            value /= 10;
            packetRequest.offset_x = (digit + 9) % 10 * 17;
            fontObjDraw(&packetRequest);
            packetRequest.x -= 12;
        }
    }
}

/* Digit run in glyph kind 0x20C, drawn right to left from x + (count - 1) * 12; each digit's
   phase is ((digit + 9) % 10) * 0x11. */
void textDrawDigitsWideAlt(int x, int y, int value, int count) {
    int i;

    if (count > 0) {
        packetRequest.y = y;
        packetRequest.x = x + count * 12 - 12;
        packetRequest.cell_index = 0x20C;
        packetRequest.flags = 0x21;
        packetRequest.offset_y = 0;
        for (i = 0; i < count; i++) {
            int digit = value % 10;

            value /= 10;
            packetRequest.offset_x = ((digit + 9) % 10) * 0x11;
            fontObjDraw(&packetRequest);
            packetRequest.x -= 12;
        }
    }
}

/* Two-layer glyph: a shadow of kind 0x46 (strip index * 15, flags 0x24) in `colour` at (x - 4, y - 2),
   then the face of kind 0x45 (strip index * 7, flags 0x64) at (x, y) in a tint picked from `colour`. */
void menuDrawCompareMark(int x, int y, int index, u64 colour) {
    u64 tint;

    tint = 0x3F80000080808080LL;
    if (colour == 0x3F8000008010104BLL) {
        tint = 0x3F80000080545B80LL;
    } else if (colour == 0x3F80000080641B15LL) {
        tint = 0x3F80000080805148LL;
    }
    packetRequest.x = x - 4;
    packetRequest.y = y - 2;
    packetRequest.cell_index = 0x46;
    packetRequest.flags = GLYPH_FLAGS_SHADOW;
    packetRequest.offset_x = index * 15;
    packetRequest.offset_y = 0;
    fontObjSetColor(colour);
    fontObjDraw(&packetRequest);
    fontObjResetColor();
    packetRequest.offset_x = index * 7;
    packetRequest.cell_index = 0x45;
    packetRequest.flags = GLYPH_FLAGS_FACE;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.offset_y = 0;
    fontObjSetColor(tint);
    fontObjDraw(&packetRequest);
    fontObjResetColor();
}


/* Nine-piece frame from a style of nine sprite kinds in keypad order (kinds[6..8] top row,
   kinds[3..5] middle, kinds[0..2] bottom): corners at their cell size, edges stretched with
   flags 9, the centre (skipped when its kind is negative) with flags 0x109. */
void panelDrawNineSlice(const int *kinds, int x, int y, int width, int height) {
    int innerWidth;
    int innerHeight;

    innerWidth = width - fontObjCellOffsetX(kinds[6]) - fontObjCellOffsetX(kinds[8]);
    innerHeight = height - fontObjCellOffsetY(kinds[6]) - fontObjCellOffsetY(kinds[0]);
    packetRequest.cell_index = kinds[6];
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = kinds[7];
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[8];
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x = x;
    packetRequest.y += fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.cell_index = kinds[3];
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.size_h = innerHeight;
    packetRequest.flags = 9;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    if (kinds[4] >= 0) {
        packetRequest.cell_index = kinds[4];
        packetRequest.flags = 0x109;
        packetRequest.size_w = innerWidth;
        if (innerWidth > 0 && innerHeight > 0) {
            fontObjDraw(&packetRequest);
        }
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[5];
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x = x;
    packetRequest.y += innerHeight;
    packetRequest.cell_index = kinds[0];
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = kinds[1];
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[2];
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
}

/* panelDrawNineSlice with flag bit 17 on the corners top-left and bottom-right, the edges and the centre. */
void panelDrawNineSliceDepth(const int *kinds, int x, int y, int width, int height) {
    int innerWidth;
    int innerHeight;

    innerWidth = width - fontObjCellOffsetX(kinds[6]) - fontObjCellOffsetX(kinds[8]);
    innerHeight = height - fontObjCellOffsetY(kinds[6]) - fontObjCellOffsetY(kinds[0]);
    packetRequest.cell_index = kinds[6];
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 0x20001;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = kinds[7];
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 0x20009;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[8];
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x = x;
    packetRequest.y += fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.cell_index = kinds[3];
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.size_h = innerHeight;
    packetRequest.flags = 0x20009;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    if (kinds[4] >= 0) {
        packetRequest.cell_index = kinds[4];
        packetRequest.flags = 0x20109;
        packetRequest.size_w = innerWidth;
        if (innerWidth > 0 && innerHeight > 0) {
            fontObjDraw(&packetRequest);
        }
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[5];
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.flags = 0x20009;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x = x;
    packetRequest.y += innerHeight;
    packetRequest.cell_index = kinds[0];
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = kinds[1];
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 0x20009;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[2];
    packetRequest.flags = 0x20001;
    fontObjDraw(&packetRequest);
}

/* panelDrawNineSlice with the top-left corner drawn with flags 0xA11. */
void panelDrawNineSliceFlipCorner(const int *kinds, int x, int y, int width, int height) {
    int innerWidth;
    int innerHeight;

    innerWidth = width - fontObjCellOffsetX(kinds[6]) - fontObjCellOffsetX(kinds[8]);
    innerHeight = height - fontObjCellOffsetY(kinds[6]) - fontObjCellOffsetY(kinds[0]);
    packetRequest.cell_index = kinds[6];
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 0xA11;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = kinds[7];
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[8];
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x = x;
    packetRequest.y += fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.cell_index = kinds[3];
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.size_h = innerHeight;
    packetRequest.flags = 9;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    if (kinds[4] >= 0) {
        packetRequest.cell_index = kinds[4];
        packetRequest.flags = 0x109;
        packetRequest.size_w = innerWidth;
        if (innerWidth > 0 && innerHeight > 0) {
            fontObjDraw(&packetRequest);
        }
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[5];
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x = x;
    packetRequest.y += innerHeight;
    packetRequest.cell_index = kinds[0];
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = kinds[1];
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[2];
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
}

/* panelDrawNineSlice with flags 0 on the corners, 8 on the edges and 0x108 on the centre. */
void panelDrawNineSliceNormalBlend(const int *kinds, int x, int y, int width, int height) {
    int innerWidth;
    int innerHeight;

    innerWidth = width - fontObjCellOffsetX(kinds[6]) - fontObjCellOffsetX(kinds[8]);
    innerHeight = height - fontObjCellOffsetY(kinds[6]) - fontObjCellOffsetY(kinds[0]);
    packetRequest.cell_index = kinds[6];
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 0;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = kinds[7];
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 8;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[8];
    packetRequest.flags = 0;
    fontObjDraw(&packetRequest);
    packetRequest.x = x;
    packetRequest.y += fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.cell_index = kinds[3];
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.size_h = innerHeight;
    packetRequest.flags = 8;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    if (kinds[4] >= 0) {
        packetRequest.cell_index = kinds[4];
        packetRequest.flags = 0x108;
        packetRequest.size_w = innerWidth;
        if (innerWidth > 0 && innerHeight > 0) {
            fontObjDraw(&packetRequest);
        }
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[5];
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.flags = 8;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x = x;
    packetRequest.y += innerHeight;
    packetRequest.cell_index = kinds[0];
    packetRequest.flags = 0;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = kinds[1];
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 8;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = kinds[2];
    packetRequest.flags = 0;
    fontObjDraw(&packetRequest);
}

/* Record group frame at (x, y) of width by height: corner and edge cells 0x116 to 0x11F with a
   75-wide title tab (cells 0x11C, 0x11D, 0x11F) on the top edge. */
void menuDrawTabFrame(int x, int y, int width, int height)
{
    int innerWidth;
    int innerHeight;
    int sideHeight;
    int topLeftCell = 0x11C;
    int bottomLeftCell = 0x116;

    innerWidth = width - fontObjCellOffsetX(topLeftCell) - fontObjCellOffsetX(0x11E);
    innerHeight = height - fontObjCellOffsetY(topLeftCell) - fontObjCellOffsetY(bottomLeftCell);
    packetRequest.cell_index = topLeftCell;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    sideHeight = innerHeight - 33;
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = 0x11D;
    packetRequest.size_w = 75;
    packetRequest.size_h = fontObjCellOffsetY(0x11D);
    packetRequest.flags = 9;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.cell_index = 0x11F;
    packetRequest.x += 75;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x += 40;
    packetRequest.y += 17;
    packetRequest.cell_index = 0x11D;
    packetRequest.size_w = innerWidth - 115;
    packetRequest.size_h = fontObjCellOffsetY(0x11D);
    packetRequest.flags = 9;
    fontObjDraw(&packetRequest);
    packetRequest.x += packetRequest.size_w;
    packetRequest.cell_index = 0x11E;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x = x;
    packetRequest.y += fontObjCellOffsetY(packetRequest.cell_index) - 17;
    packetRequest.cell_index = 0x119;
    packetRequest.size_w = fontObjCellOffsetX(0x119);
    packetRequest.size_h = innerHeight;
    packetRequest.flags = 9;
    if (sideHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.y += 17;
    packetRequest.cell_index = 0x11A;
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = innerHeight - 17;
    packetRequest.flags = 0x109;
    if (innerWidth > 0 && sideHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = 0x11B;
    packetRequest.size_w = fontObjCellOffsetX(0x11B);
    packetRequest.flags = 9;
    if (sideHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x = x;
    packetRequest.y += sideHeight + 16;
    packetRequest.flags = 1;
    packetRequest.cell_index = bottomLeftCell;
    packetRequest.size_h -= 16;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = 0x117;
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(0x117);
    packetRequest.flags = 9;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = 0x118;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x = x + fontObjCellOffsetX(topLeftCell);
    packetRequest.y = y + fontObjCellOffsetY(topLeftCell);
    packetRequest.cell_index = 0x11A;
    packetRequest.size_w = 75;
    packetRequest.size_h = 33 - fontObjCellOffsetY(0x11D);
    packetRequest.flags = 9;
    fontObjDraw(&packetRequest);
}

/* Fixed-size badge at (x, y): kind 0x136 and the 0x54-wide strip 0x137 (flags 0x819) on the
   left, then from x + 0x64 a small frame of kinds 0x135, 0x11D, 0x11A, 0x117, 0x11E, 0x118 and
   0x11B (flags 1 for 0x135, 0x11E and 0x118, flags 9 for the strips). */
void menuDrawBadge(int x, int y) {
    packetRequest.cell_index = 0x136;
    packetRequest.y = y + 0xA;
    packetRequest.x = x;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x137;
    packetRequest.x = x + 0x10;
    packetRequest.size_w = 0x54;
    packetRequest.size_h = fontObjCellOffsetY(0x137);
    packetRequest.flags = 0x819;
    fontObjDraw(&packetRequest);
    packetRequest.x = x + 0x64;
    packetRequest.cell_index = 0x135;
    packetRequest.y = y;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x += 0x28;
    packetRequest.cell_index = 0x11D;
    packetRequest.size_w = 0x46;
    packetRequest.size_h = fontObjCellOffsetY(0x11D);
    packetRequest.flags = 9;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x11A;
    packetRequest.size_w = 0x6E;
    packetRequest.x = x + 0x64;
    packetRequest.y = y + 0x10;
    packetRequest.size_h = 3;
    fontObjDraw(&packetRequest);
    packetRequest.y = y + 0x13;
    packetRequest.cell_index = 0x117;
    packetRequest.size_h = fontObjCellOffsetY(0x117);
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x11E;
    packetRequest.x = x + 0xD2;
    packetRequest.y = y;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.y = y + 0x13;
    packetRequest.cell_index = 0x118;
    fontObjDraw(&packetRequest);
    packetRequest.y = y + 0x10;
    packetRequest.cell_index = 0x11B;
    packetRequest.size_w = fontObjCellOffsetX(0x11B);
    packetRequest.size_h = 3;
    packetRequest.flags = 9;
    fontObjDraw(&packetRequest);
}

/* Record group of ten submits, sprite kinds 0x100 to 0x10A. */
/* Nine-piece frame with fixed cells (the panelDrawNineSlice layout with its kinds in place):
   corners 258/260/261/266, edges 259/255/257/262, centre 256 with flags 0x109; extra cells
   253 and 254 finish the bottom row. */
void menuDrawListFrame(int x, int y, int width, int height)
{
    int innerWidth;
    int innerHeight;
    int topLeftCell = 258;
    int topRightCell = 260;
    int tabCell;

    innerWidth = width - fontObjCellOffsetX(topLeftCell) - fontObjCellOffsetX(topRightCell);
    innerHeight = height - fontObjCellOffsetY(topLeftCell) - fontObjCellOffsetY(261);
    packetRequest.cell_index = topLeftCell;
    packetRequest.y = y;
    packetRequest.x = x;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = 259;
    packetRequest.size_w = innerWidth;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerWidth > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = topRightCell;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x = x;
    packetRequest.y += fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.cell_index = 255;
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.size_h = innerHeight;
    packetRequest.flags = 9;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = 256;
    packetRequest.size_w = innerWidth;
    packetRequest.flags = 0x109;
    if (innerWidth > 0 && innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x += innerWidth;
    packetRequest.cell_index = 257;
    packetRequest.size_w = fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.x = x;
    packetRequest.y += innerHeight;
    packetRequest.flags = 1;
    packetRequest.cell_index = 261;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.cell_index = 262;
    tabCell = 266;
    packetRequest.size_w = innerWidth - 60 - fontObjCellOffsetX(tabCell);
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 9;
    fontObjDraw(&packetRequest);
    packetRequest.x += packetRequest.size_w;
    packetRequest.cell_index = tabCell;
    packetRequest.size_w = 100;
    packetRequest.size_h = fontObjCellOffsetY(tabCell);
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.x += fontObjCellOffsetX(packetRequest.cell_index);
    packetRequest.size_w = 60;
    packetRequest.cell_index = 253;
    packetRequest.size_h = fontObjCellOffsetY(packetRequest.cell_index);
    packetRequest.flags = 9;
    if (innerHeight > 0) {
        fontObjDraw(&packetRequest);
    }
    packetRequest.cell_index = 254;
    packetRequest.flags = 1;
    packetRequest.x += packetRequest.size_w;
    fontObjDraw(&packetRequest);
}

/* Calls menuDrawOutlineFrame (defined below) and nothing else: the callers' four arguments pass straight through. */
void menuDrawOutlineWindow(int x, int y, int width, int height) {
    menuDrawOutlineFrame(x, y, width, height);
}

/* Titled frame: menuDrawShortOutlineFrame's outline, a title bar (kinds 0x52 and 0x54 with flags 1, the
   0x53 strip with flags 9, then the 0x5A overlay with flags 0x118), then the optional title
   text at (x + 4, y - 1). */
void menuDrawTitledFrame(int x, int y, int width, int height, const char *text) {
    int barWidth;

    menuDrawShortOutlineFrame(x, y, width, height);
    packetRequest.cell_index = 0x52;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 1;
    barWidth = width - 0x10;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x53;
    packetRequest.x += 8;
    packetRequest.size_w = barWidth;
    packetRequest.size_h = 0x10;
    packetRequest.flags = 9;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x54;
    packetRequest.x += barWidth;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x5A;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.size_w = width;
    packetRequest.size_h = 0x10;
    packetRequest.flags = 0x118;
    fontObjDraw(&packetRequest);
    if (text != 0) {
        textDrawTitle(0, x + 4, y - 1, text);
    }
}

/* menuDrawTitledFrame's title bar tinted 0x3F80000080800000 (flags 5 and 0xD), the tint reset before
   the title text at (x + 12, y - 1). */
void menuDrawTitledFrameBlue(int x, int y, int width, int height, const char *text) {
    int barWidth;

    menuDrawShortOutlineFrame(x, y, width, height);
    fontObjSetColor(0x3F80000080800000LL);
    packetRequest.cell_index = 0x52;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 5;
    barWidth = width - 0x10;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x53;
    packetRequest.x += 8;
    packetRequest.size_w = barWidth;
    packetRequest.size_h = 0x10;
    packetRequest.flags = 0xD;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x54;
    packetRequest.x += barWidth;
    packetRequest.flags = 5;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x5A;
    packetRequest.flags = 0x118;
    packetRequest.size_w = width;
    packetRequest.size_h = 0x10;
    packetRequest.x = x;
    packetRequest.y = y;
    fontObjDraw(&packetRequest);
    fontObjResetColor();
    if (text != 0) {
        textDrawTitle(0, x + 0xC, y - 1, text);
    }
}

/* menuDrawTitledFrameBlue with the tint 0x3F80000080000080. */
void menuDrawTitledFrameRed(int x, int y, int width, int height, const char *text) {
    int barWidth;

    menuDrawShortOutlineFrame(x, y, width, height);
    fontObjSetColor(0x3F80000080000080LL);
    packetRequest.cell_index = 0x52;
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.flags = 5;
    barWidth = width - 0x10;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x53;
    packetRequest.x += 8;
    packetRequest.size_w = barWidth;
    packetRequest.size_h = 0x10;
    packetRequest.flags = 0xD;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x54;
    packetRequest.x += barWidth;
    packetRequest.flags = 5;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x5A;
    packetRequest.flags = 0x118;
    packetRequest.size_w = width;
    packetRequest.size_h = 0x10;
    packetRequest.x = x;
    packetRequest.y = y;
    fontObjDraw(&packetRequest);
    fontObjResetColor();
    if (text != 0) {
        textDrawTitle(0, x + 0xC, y - 1, text);
    }
}

/* Fixed layout of twelve pieces placed from (x, y): kinds 0x226 to 0x232 and 0x254; the
   stretched strips are drawn with flags 8 and 9. */
void menuDrawFixedFrame(int x, int y) {
    packetRequest.cell_index = 0x22E;
    packetRequest.y = y + 0x36;
    packetRequest.x = x;
    packetRequest.flags = 0;
    fontObjDraw(&packetRequest);
    packetRequest.flags = 1;
    packetRequest.cell_index = 0x226;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x22F;
    packetRequest.size_w = 0x8E;
    packetRequest.x += 0x10;
    packetRequest.size_h = 0x10;
    packetRequest.flags = 8;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x254;
    packetRequest.x += 0x8E;
    packetRequest.y -= 4;
    packetRequest.flags = 0;
    fontObjDraw(&packetRequest);
    packetRequest.size_w = 0x9D;
    packetRequest.cell_index = 0x22F;
    packetRequest.x += 0x20;
    packetRequest.y -= 0xD;
    packetRequest.size_h = 0x10;
    packetRequest.flags = 8;
    fontObjDraw(&packetRequest);
    packetRequest.flags = 0;
    packetRequest.cell_index = 0x230;
    packetRequest.x += 0x9D;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x227;
    packetRequest.y = y + 0x10;
    packetRequest.size_h = 0x26;
    packetRequest.x = x;
    packetRequest.size_w = 0x10;
    packetRequest.flags = 9;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x231;
    packetRequest.size_h = 0x15;
    packetRequest.x += 0x15B;
    packetRequest.flags = 8;
    packetRequest.size_w = 0x10;
    fontObjDraw(&packetRequest);
    packetRequest.x = x;
    packetRequest.cell_index = 0x228;
    packetRequest.y = y;
    packetRequest.flags = 1;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 0x229;
    packetRequest.size_w = 0x14B;
    packetRequest.x += 0x10;
    packetRequest.size_h = 0x10;
    packetRequest.flags = 9;
    fontObjDraw(&packetRequest);
    packetRequest.flags = 0;
    packetRequest.cell_index = 0x232;
    packetRequest.x += 0x14B;
    fontObjDraw(&packetRequest);
    packetRequest.flags = 1;
    packetRequest.cell_index = 0x22A;
    fontObjDraw(&packetRequest);
}

/* Three-record group (kinds 0x13A to 0x13C) at (x, y, width). */
void menuDrawThreePieceBar(int x, int y, int width)
{
    packetRequest.cell_index = 314;
    packetRequest.flags = 1;
    packetRequest.x = x;
    packetRequest.y = y;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 316;
    packetRequest.x = x + width - 10;
    fontObjDraw(&packetRequest);
    packetRequest.cell_index = 315;
    packetRequest.x = x + 10;
    packetRequest.y = y;
    packetRequest.size_w = width - 20;
    packetRequest.size_h = 21;
    packetRequest.flags |= 8;
    fontObjDraw(&packetRequest);
}

/* Border without fill: edge and corner pieces 0x23-0x2A; the tint is set first. */
void menuDrawBorder(int x, int y, int width, int height, unsigned long long color) {
    int bottomY;
    int innerX;
    int innerWidth;
    int innerHeight;

    bottomY = (y + height) - 0x10;
    innerX = x + 0x10;
    innerWidth = width - 0x20;
    fontObjSetColor(color);
    menuDrawCell(0x23, x, bottomY, 5);
    innerHeight = height - 0x20;
    menuDrawCellSized(0x24, innerX, bottomY, innerWidth, 0x10, 5);
    menuDrawCell(0x25, (x + width) - 0x10, bottomY, 5);
    menuDrawCellSized(0x26, x, y + 0x10, 0x10, innerHeight, 5);
    menuDrawCellSized(0x27, (x + width) - 0x10, y + 0x10, 0x10, innerHeight, 5);
    menuDrawCell(0x28, x, y, 5);
    menuDrawCellSized(0x29, innerX, y, innerWidth, 0x10, 5);
    menuDrawCell(0x2A, (x + width) - 0x10, y, 5);
    fontObjResetColor();
}

/* Panel frame: border and corner pieces 0x2F-0x35 with the fill in mode 0x105; the tint is set first. */
void menuDrawPanelFrame(int x, int y, int width, int height, unsigned long long color) {
    int bottomY;
    int innerY;
    int innerX;
    int innerWidth;
    int innerHeight;

    bottomY = (y + height) - 0x10;
    innerY = y + 0x10;
    innerX = x + 0x10;
    innerWidth = width - 0x20;
    fontObjSetColor(color);
    menuDrawCell(0x2F, x, bottomY, 5);
    innerHeight = height - 0x20;
    menuDrawCellSized(0x30, innerX, bottomY, innerWidth, 0x10, 5);
    menuDrawCell(0x31, (x + width) - 0x10, bottomY, 5);
    menuDrawCellSized(0x32, x, innerY, 0x10, innerHeight, 5);
    menuDrawCellSized(0x33, innerX, innerY, innerWidth, innerHeight, 0x105);
    menuDrawCellSized(0x32, (x + width) - 0x10, innerY, 0x10, innerHeight, 5);
    menuDrawCell(0x34, x, y, 5);
    menuDrawCellSized(0x30, innerX, y, innerWidth, 0x10, 5);
    menuDrawCell(0x35, (x + width) - 0x10, y, 5);
    fontObjResetColor();
}

/* Wide panel frame grown by 14 pixels on every side: 30-pixel pieces 0x38-0x40 with the fill
   in mode 0x105; the tint is set first. */
void menuDrawPanelFrameWide(int x, int y, int width, int height, unsigned long long color) {
    int bottomY;
    int innerY;
    int innerX;
    int innerWidth;
    int innerHeight;

    fontObjSetColor(color);
    y -= 0xE;
    height += 0x1C;
    x -= 0xE;
    width += 0x1C;
    bottomY = (y + height) - 0x1E;
    innerY = y + 0x1E;
    innerX = x + 0x1E;
    innerWidth = width - 0x3C;
    menuDrawCell(0x38, x, bottomY, 5);
    innerHeight = height - 0x3C;
    menuDrawCellSized(0x39, innerX, bottomY, innerWidth, 0x1E, 5);
    menuDrawCell(0x3A, (x + width) - 0x1E, bottomY, 5);
    menuDrawCellSized(0x3B, x, innerY, 0x1E, innerHeight, 5);
    menuDrawCellSized(0x3C, innerX, innerY, innerWidth, innerHeight, 0x105);
    menuDrawCellSized(0x3D, (x + width) - 0x1E, innerY, 0x1E, innerHeight, 5);
    menuDrawCell(0x3E, x, y, 5);
    menuDrawCellSized(0x3F, innerX, y, innerWidth, 0x1E, 5);
    menuDrawCell(0x40, (x + width) - 0x1E, y, 5);
    fontObjResetColor();
}

/* Panel frame: border and corner pieces 0x2F-0x35 with the fill in mode 0x106; the tint is set first. */
/* color is a 64-bit RGBA+Q word, as in menuDrawPanel's 0x3F800000607F7F7F tint. */
void menuDrawPanelFrameAlt(int x, int y, int width, int height, unsigned long long color) {
    int bottomY;
    int innerY;
    int innerX;
    int innerWidth;
    int innerHeight;

    bottomY = (y + height) - 0x10;
    innerY = y + 0x10;
    innerX = x + 0x10;
    innerWidth = width - 0x20;
    fontObjSetColor(color);
    menuDrawCell(0x2F, x, bottomY, 6);
    innerHeight = height - 0x20;
    menuDrawCellSized(0x30, innerX, bottomY, innerWidth, 0x10, 6);
    menuDrawCell(0x31, (x + width) - 0x10, bottomY, 6);
    menuDrawCellSized(0x32, x, innerY, 0x10, innerHeight, 6);
    menuDrawCellSized(0x33, innerX, innerY, innerWidth, innerHeight, 0x106);
    menuDrawCellSized(0x32, (x + width) - 0x10, innerY, 0x10, innerHeight, 6);
    menuDrawCell(0x34, x, y, 6);
    menuDrawCellSized(0x30, innerX, y, innerWidth, 0x10, 6);
    menuDrawCell(0x35, (x + width) - 0x10, y, 6);
    fontObjResetColor();
}

/* Wide panel frame grown by 14 pixels on every side: 30-pixel pieces 0x38-0x40 with the fill
   in mode 0x106; the tint is set first. */
void menuDrawPanelFrameWideAlt(int x, int y, int width, int height, unsigned long long color) {
    int bottomY;
    int innerY;
    int innerX;
    int innerWidth;
    int innerHeight;

    fontObjSetColor(color);
    y -= 0xE;
    height += 0x1C;
    x -= 0xE;
    width += 0x1C;
    bottomY = (y + height) - 0x1E;
    innerY = y + 0x1E;
    innerX = x + 0x1E;
    innerWidth = width - 0x3C;
    menuDrawCell(0x38, x, bottomY, 6);
    innerHeight = height - 0x3C;
    menuDrawCellSized(0x39, innerX, bottomY, innerWidth, 0x1E, 6);
    menuDrawCell(0x3A, (x + width) - 0x1E, bottomY, 6);
    menuDrawCellSized(0x3B, x, innerY, 0x1E, innerHeight, 6);
    menuDrawCellSized(0x3C, innerX, innerY, innerWidth, innerHeight, 0x106);
    menuDrawCellSized(0x3D, (x + width) - 0x1E, innerY, 0x1E, innerHeight, 6);
    menuDrawCell(0x3E, x, y, 6);
    menuDrawCellSized(0x3F, innerX, y, innerWidth, 0x1E, 6);
    menuDrawCell(0x40, (x + width) - 0x1E, y, 6);
    fontObjResetColor();
}

/* Tint words of the panel layers: the frame and the fill under the border. */
#define MENU_PANEL_FRAME_COLOR 0x3F800000507F7F7FLL
#define MENU_PANEL_FILL_COLOR 0x3F800000607F7F7FLL

/* Panel: frame in mode 5, fill in mode 6, then the border tinted `color`. */
void menuDrawPanel(int x, int y, int width, int height, unsigned long long color) {
    menuDrawPanelFrame(x, y, width, height, MENU_PANEL_FRAME_COLOR);
    menuDrawPanelFrameAlt(x, y, width, height, MENU_PANEL_FILL_COLOR);
    menuDrawBorder(x, y, width, height, color);
}

/* menuDrawPanel with the wide frames: menuDrawPanelFrameWide in mode 5, menuDrawPanelFrameWideAlt in mode 6, then the
   border. */
void menuDrawPanelWide(int x, int y, int width, int height, unsigned long long color) {
    menuDrawPanelFrameWide(x, y, width, height, MENU_PANEL_FRAME_COLOR);
    menuDrawPanelFrameWideAlt(x, y, width, height, MENU_PANEL_FILL_COLOR);
    menuDrawBorder(x, y, width, height, color);
}


/* Small window frame (menuDrawTitledWindow's layout): width and height lose 0x22 first; two
   passes of the corner and edge pieces 0xB2-0xB5 (an edge is skipped when its length is
   0), one pass of 0xB6-0xB9, then a band of 5-pixel rows 0xBA-0xBC whose alpha follows a
   half sine over the reduced height + 20. A narrow window (width <= 0x40) draws its 0xBA
   fill at half that alpha, then sets the colour again from the halved alpha doubled (its
   low bit lost) for the 0xBC ends. */
void menuDrawWindowFrame(int x, int y, int width, int height)
{
    int bandHeight;
    int i;
    int alpha;
    int offset;

    width -= 0x22;
    height -= 0x22;
    if (width < 0 || height < 0) {
        return;
    }
    for (i = 0; i < 2; i++) {
        menuDrawCell(0xB4, x - 0x10, y - 0x10, 0);
        menuDrawCell(0xB4, x + width + 0x10, y - 0x10, 0x200);
        menuDrawCell(0xB4, x - 0x10, y + height + 0x10, 0x400);
        menuDrawCell(0xB4, x + width + 0x10, y + height + 0x10, 0x600);
        if (width != 0) {
            menuDrawCellSized(0xB5, x + 0x10, y - 0x10, width, 0x20, 0x800);
            menuDrawCellSized(0xB5, x + 0x10, y + height + 0x10, width, 0x20, 0xC00);
        }
        if (height != 0) {
            menuDrawCellSized(0xB2, x - 0x10, y + 0x10, 0x20, height, 0x1000);
            menuDrawCellSized(0xB2, x + width + 0x10, y + 0x10, 0x20, height, 0x1200);
        }
        if (width != 0 && height != 0) {
            menuDrawCellSized(0xB3, x + 0x10, y + 0x10, width, height, 0x100);
        }
    }

    menuDrawCell(0xB8, x - 0x10, y - 0x10, 1);
    menuDrawCell(0xB8, x + width + 0x10, y - 0x10, 0x201);
    menuDrawCell(0xB8, x - 0x10, y + height + 0x10, 0x401);
    menuDrawCell(0xB8, x + width + 0x10, y + height + 0x10, 0x601);
    if (width != 0) {
        menuDrawCellSized(0xB9, x + 0x10, y - 0x10, width, 0x20, 0x801);
        menuDrawCellSized(0xB9, x + 0x10, y + height + 0x10, width, 0x20, 0xC01);
    }
    if (height != 0) {
        menuDrawCellSized(0xB6, x - 0x10, y + 0x10, 0x20, height, 0x1001);
        menuDrawCellSized(0xB6, x + width + 0x10, y + 0x10, 0x20, height, 0x1201);
    }
    if (width != 0 && height != 0) {
        menuDrawCellSized(0xB7, x + 0x10, y + 0x10, width, height, 0x101);
    }

    bandHeight = height + 20;
    for (i = 0; i < bandHeight; i += 5) {
        alpha = (int)(xglSin((float)i * 3.1415927f / (float)bandHeight) * 128.0f);
        fontObjSetColor(((u64)alpha << 24) | 0x3F80000000808080LL);
        alpha >>= 1;
        offset = 1;
        if (width > 0x40) {
            menuDrawCell(0xBB, x + 0x10, y + i + 6, 0x15);
            menuDrawCell(0xBB, x + width - 0x11, y + i + 6, 0x215);
            offset = 2;
            menuDrawCellSized(0xBA, x + 0x30, y + i + 6, width - 0x40, 5, 0x815);
        } else {
            fontObjSetColor(((u64)alpha << 24) | 0x3F80000000808080LL);
            menuDrawCellSized(0xBA, x + 0x10, y + i + 6, width, 5, 0x815);
            alpha <<= 1;
            fontObjSetColor(((u64)alpha << 24) | 0x3F80000000808080LL);
        }
        menuDrawCell(0xBC, x - 0xF, y + i + 6, 0x15);
        menuDrawCell(0xBC, x + width - offset + 0x11, y + i + 6, 0x215);
    }
    fontObjResetColor();
}

/* Short outline frame: 10-pixel top and bottom bars, pieces 0x69-0x70 with sides 0x4E and 0x58. */
void menuDrawShortOutlineFrame(int x, int y, int width, int height) {
    int rightX;
    int bottomY;
    int innerY;
    int innerHeight;
    int innerX;
    int innerWidth;

    rightX = (x + width) - 0x10;
    innerY = y + 0xA;
    bottomY = (y + height) - 0xA;
    menuDrawCell(0x6D, x, bottomY, 0);
    innerWidth = width - 0x20;
    innerX = x + 0x10;
    innerHeight = height - 0x14;
    menuDrawCellSized(0x6E, innerX, bottomY, innerWidth, 0xA, 0);
    menuDrawCell(0x6F, rightX, bottomY, 0);
    menuDrawCellSized(0x58, rightX, innerY, 0x10, innerHeight, 0);
    menuDrawCell(0x70, rightX, y, 0);
    menuDrawCell(0x69, x, bottomY, 1);
    menuDrawCellSized(0x4E, x, innerY, 0x10, innerHeight, 1);
    menuDrawCell(0x6A, x, y, 1);
    menuDrawCellSized(0x6B, innerX, y, innerWidth, 0xA, 1);
    menuDrawCell(0x6C, rightX, y, 1);
}

void menuDrawOutlineFrame(int x, int y, int width, int height) {
    int rightX;
    int bottomY;
    int innerY;
    int innerHeight;
    int innerX;
    int innerWidth;

    rightX = (x + width) - 0x10;
    bottomY = (y + height) - 0x10;
    innerY = y + 0x10;
    innerHeight = height - 0x20;
    innerX = x + 0x10;
    menuDrawCell(0x63, x, bottomY, 0);
    menuDrawCellSized(0x64, x, innerY, 0x10, innerHeight, 0);
    innerWidth = width - 0x20;
    menuDrawCell(0x65, x, y, 0);
    menuDrawCellSized(0x66, innerX, y, innerWidth, 0x10, 0);
    menuDrawCell(0x67, rightX, y, 0);
    menuDrawCell(0x5D, x, bottomY, 1);
    menuDrawCellSized(0x5E, innerX, bottomY, innerWidth, 0x10, 1);
    menuDrawCell(0x5F, rightX, bottomY, 1);
    menuDrawCellSized(0x60, rightX, innerY, 0x10, innerHeight, 1);
    menuDrawCell(0x61, rightX, y, 1);
}

/* Split frame: outer pieces 0x4D-0x59, the 0x62 mark at split and 0x50 bars on both sides of it. */
void menuDrawSplitFrame(int x, int y, int width, int height, int split) {
    int bottom;
    int top_inner;
    int split_x;
    int right;
    int left_inner;

    bottom = (y + height) - 0x10;
    top_inner = y + 0x11;
    split_x = x + split;
    right = (x + width) - 0x10;
    left_inner = x + 0x10;
    menuDrawCell(0x55, x, bottom, 0);
    menuDrawCellSized(0x56, left_inner, bottom, width - 0x20, 0x10, 0);
    menuDrawCell(0x57, right, bottom, 0);
    menuDrawCellSized(0x58, right, y + 0x21, 0x10, height - 0x31, 0);
    menuDrawCell(0x59, right, top_inner, 0);
    menuDrawCell(0x4D, x, bottom, 1);
    menuDrawCellSized(0x4E, x, y + 0x10, 0x10, height - 0x20, 1);
    menuDrawCell(0x4F, x, y, 1);
    menuDrawCell(0x51, right, top_inner, 1);
    menuDrawCell(0x62, split_x, y, 1);
    menuDrawCellSized(0x50, left_inner, y, split - 0x10, 0x10, 1);
    menuDrawCellSized(0x50, split_x + 0x20, top_inner, (width - split) - 0x30, 0x10, 1);
}

/* Window frame around a width x height area: four groups of corner, edge and fill
   pieces 0x2A1-0x2B2 (an edge is skipped when its length is 0), the title bar
   0x2B6-0x2B8 with the title text when there is one, then a band of 5-pixel rows
   0x2B3-0x2B5 whose alpha follows a half sine over height + 20 (a window no wider than
   0x40 draws its 0x2B3 fill at half that alpha, as in menuDrawWindowFrame). */
void menuDrawTitledWindow(int x, int y, int width, int height, char *title)
{
    int bandHeight;
    int left;
    int top;
    int right;
    int i;
    int alpha;
    int offset;

    if (width < 0 || height < 0) {
        return;
    }
    left = x - 0x10;
    top = y - 0x10;
    right = x + width;

    menuDrawCell(0x2A3, left, top, 0x10000);
    menuDrawCell(0x2A3, x + width + 0x10, top, 0x10200);
    menuDrawCell(0x2A3, left, y + height + 0x10, 0x10400);
    menuDrawCell(0x2A3, x + width + 0x10, y + height + 0x10, 0x10600);
    if (width != 0) {
        menuDrawCellSized(0x2A4, x + 0x10, top, width, 0x20, 0x10800);
        menuDrawCellSized(0x2A4, x + 0x10, y + height + 0x10, width, 0x20, 0x10C00);
    }
    if (height != 0) {
        menuDrawCellSized(0x2A1, left, y + 0x10, 0x20, height, 0x11000);
        menuDrawCellSized(0x2A1, x + width + 0x10, y + 0x10, 0x20, height, 0x11200);
    }
    if (width != 0 && height != 0) {
        menuDrawCellSized(0x2A2, x + 0x10, y + 0x10, width, height, 0x10100);
    }

    menuDrawCell(0x2B1, left, top, 0x10001);
    menuDrawCell(0x2B1, x + width + 0x10, top, 0x10201);
    menuDrawCell(0x2B1, left, y + height + 0x10, 0x10401);
    menuDrawCell(0x2B1, x + width + 0x10, y + height + 0x10, 0x10601);
    if (width != 0) {
        menuDrawCellSized(0x2B2, x + 0x10, top, width, 0x20, 0x10801);
        menuDrawCellSized(0x2B2, x + 0x10, y + height + 0x10, width, 0x20, 0x10C01);
    }
    if (height != 0) {
        menuDrawCellSized(0x2AF, left, y + 0x10, 0x20, height, 0x11001);
        menuDrawCellSized(0x2AF, x + width + 0x10, y + 0x10, 0x20, height, 0x11201);
    }
    if (width != 0 && height != 0) {
        menuDrawCellSized(0x2B0, x + 0x10, y + 0x10, width, height, 0x10101);
    }

    menuDrawCell(0x2A9, x + width + 0x10, top, 0x10000);
    menuDrawCell(0x2A5, left, y + height + 0x10, 0x10000);
    menuDrawCell(0x2A7, x + width + 0x10, y + height + 0x10, 0x10000);
    if (width != 0) {
        menuDrawCellSized(0x2A6, x + 0x10, y + height + 0x10, width, 0x20, 0x10800);
    }
    if (height != 0) {
        menuDrawCellSized(0x2A8, x + width + 0x10, y + 0x10, 0x20, height, 0x11000);
    }

    menuDrawCell(0x2AC, left, top, 0x10001);
    menuDrawCell(0x2AE, x + width + 0x10, top, 0x10001);
    menuDrawCell(0x2AA, left, y + height + 0x10, 0x10001);
    if (width != 0) {
        menuDrawCellSized(0x2AD, x + 0x10, top, width, 0x20, 0x10801);
    }
    if (height != 0) {
        menuDrawCellSized(0x2AB, left, y + 0x10, 0x20, height, 0x11001);
    }

    if (title != 0) {
        menuDrawCell(0x2B6, left, top, 0x10001);
        menuDrawCell(0x2B8, x + width + 0x10, top, 0x10001);
        if (width != 0) {
            menuDrawCellSized(0x2B7, x + 0x10, top, width, 0x20, 0x10801);
        }
        textDrawTitle(1, x + 0x10, y + 2, title);
    }

    bandHeight = height + 20;
    for (i = 0; i < bandHeight; i += 5) {
        alpha = (int)(xglSin((float)i * 3.1415927f / (float)bandHeight) * 128.0f);
        fontObjSetColor(((u64)alpha << 24) | 0x3F80000000808080LL);
        alpha >>= 1;
        offset = 1;
        if (width > 0x40) {
            menuDrawCell(0x2B4, x + 0x10, y + i + 6, 0x10015);
            menuDrawCell(0x2B4, right - 0x11, y + i + 6, 0x10215);
            offset = 2;
            menuDrawCellSized(0x2B3, x + 0x30, y + i + 6, width - 0x40, 5, 0x10815);
        } else {
            fontObjSetColor(((u64)alpha << 24) | 0x3F80000000808080LL);
            menuDrawCellSized(0x2B3, x + 0x10, y + i + 6, width, 5, 0x10815);
            alpha <<= 1;
            fontObjSetColor(((u64)alpha << 24) | 0x3F80000000808080LL);
        }
        menuDrawCell(0x2B5, left, y + i + 6, 0x10015);
        menuDrawCell(0x2B5, right - offset + 0x10, y + i + 6, 0x10215);
    }
    fontObjResetColor();
}

/* Scroll frame for a count-sized panel: the frame pieces shift with offset = (count-4)*16,
   and the body gets one 0xE8 fill strip per entry. */
void menuDrawScrollFrame(int count)
{
    int offset = (count - 4) << 4;
    int negOffset;
    int y;
    int n;

    menuDrawCellSized(0xE5, 0, 0x183 - offset, 0x200, offset + 0x32, 0x101);
    menuDrawCell(0xE9, 0, 0x1BD, 1);
    menuDrawCell(0xE7, 0, 0x155 - offset, 1);
    if (count > 0) {
        negOffset = -offset;
        y = negOffset + 0x17D;
        n = count;
        do {
            menuDrawCell(0xE8, 0, y, 1);
            y += 0x10;
            n--;
        } while (n != 0);
    }
    menuDrawCellSized(0xEA, 0, 0x1B3, 0x200, 0x10, 0x801);
    menuDrawCellSized(0xEA, 0, 0x183 - offset, 0x200, 0x10, 0x801);
}

#include "shared.h"


extern unsigned short menuIconKinds[];

/* Icon at (x, y): the backing piece 0x157, then the kind stored in menuIconKinds[index]. */
void menuDrawIndexedIcon(int x, int y, int index)
{
    unsigned short iconKind = menuIconKinds[index];

    menuDrawCell(0x157, x, y, 0x10);
    menuDrawCell(iconKind, x, y, 0x11);
}

/* Icon at (x, y) (menuDrawIndexedIcon) plus the 0x138 mark when it is the current
   member's: the back slot in split mode, the front slot otherwise. */
void menuDrawIndexedIconMarked(int x, int y, int icon)
{
    menuDrawIndexedIcon(x, y, icon);
    if (ptState.splitMode == PARTY_SPLIT_ON) {
        if (ptState.slots[ptState.backIndex + 7]->status->memberIndex == icon) {
            menuDrawCell(0x138, x - 3, y - 3, 0x10);
        }
    } else {
        if (ptState.slots[ptState.frontIndex]->status->memberIndex == icon) {
            menuDrawCell(0x138, x - 3, y - 3, 0x10);
        }
    }
}

/* Icon at (x, y): the backing piece 0x158, then menuIconKinds[icon] + 1. */
void menuDrawLitIcon(int x, int y, int icon)
{
    int kind = menuIconKinds[icon] + 1;

    menuDrawCell(0x158, x, y, 0x10);
    menuDrawCell(kind, x, y, 0x11);
}

/* Wide icon at (x, y): the backing piece 0x158, then menuIconKinds[icon] + 1, both 0x16 x 0x14. */
void menuDrawLitIconWide(int x, int y, int icon)
{
    int kind = menuIconKinds[icon] + 1;

    menuDrawCellSized(0x158, x, y, 0x16, 0x14, 0x10);
    menuDrawCellSized(kind, x, y, 0x16, 0x14, 1);
}

/* Like menuDrawLitIcon with a fixed colour set for both pieces. */
void menuDrawDimIcon(int x, int y, int icon)
{
    int kind = menuIconKinds[icon] + 1;

    fontObjSetColor(0x3F80000080404040LL);
    menuDrawCell(0x158, x, y, 0x14);
    menuDrawCell(kind, x, y, 0x15);
    fontObjResetColor();
}

/* Lit icon at (x, y) (menuDrawLitIcon) plus the 0x138 mark when it is the current
   member's: the back slot in split mode, the front slot otherwise. */
void menuDrawLitIconMarked(int x, int y, int icon)
{
    menuDrawLitIcon(x, y, icon);
    if (ptState.splitMode == PARTY_SPLIT_ON) {
        if (ptState.slots[ptState.backIndex + 7]->status->memberIndex == icon) {
            menuDrawCell(0x138, x - 3, y - 3, 0x10);
        }
    } else {
        if (ptState.slots[ptState.frontIndex]->status->memberIndex == icon) {
            menuDrawCell(0x138, x - 3, y - 3, 0x10);
        }
    }
}

/* Cursor placement: (0, 0, 0) resets menuCursorPos and menuCursorSize to zero; while their
   targets are still zero both are set straight to (x, y + 10) and (width, 0), a flat box on
   the row's centre line; otherwise, with blink set, the targets become (x, y) and
   (width, 0x14), and without blink nothing moves. */
void menuPlaceCursor(int x, int y, int width, int blink)
{
    int blinkVal = blink ? 0x14 : 0;

    if (x == 0 && y == 0 && width == 0) {
        fontTrackSetBoth(&menuCursorPos, 0, 0);
        fontTrackSetBoth(&menuCursorSize, 0, 0);
        return;
    }
    if (menuCursorPos.target_pair.x == 0 && menuCursorPos.target_pair.y == 0 && menuCursorSize.target_pair.x == 0) {
        fontTrackSetBoth(&menuCursorPos, x, y + 0xA);
        fontTrackSetBoth(&menuCursorSize, width, 0);
        return;
    }
    if (blinkVal != 0) {
        fontTrackSetTarget(&menuCursorPos, x, y - (blinkVal >> 1) + 0xA);
        fontTrackSetTarget(&menuCursorSize, width, blinkVal);
        return;
    }
}

/* Cursor box: position from menuCursorPos, size from menuCursorSize, kind 0x82 with mode 0x105. */
void menuDrawCursor(void)
{
    fontTrackEase(&menuCursorPos);
    fontTrackEase(&menuCursorSize);
    fontTrackEase(&menuCursorPos);
    fontTrackEase(&menuCursorSize);
    fontObjSetColorRgba(0x24, 0, 0x48, 0x80);
    menuDrawCellSized(0x82, menuCursorPos.current_pair.x, menuCursorPos.current_pair.y,
                          menuCursorSize.current_pair.x, menuCursorSize.current_pair.y, 0x105);
    fontObjResetColor();
}

/* Select arrows 0x164 and 0x165 beside a row; their bob offset comes from xglTickGet. */
void menuDrawSelectArrows(int x, int width, int y)
{
    int offset = (xglTickGet() & 0xF) >> 1;
    int row = y + 0x3C;

    menuDrawCell(0x164, x - offset - 8, row, 1);
    menuDrawCell(0x165, x + width + offset - 8, row, 1);
}

/* Four page tabs 0x15D+page; the tab of selected's page (selected / 8) opens into its
   cells instead: 8 per page (4 on the last), 0x12F at selected and 0x15C elsewhere. */
void menuDrawPageTabs(int x, int y, int selected)
{
    int page;
    int cell;
    int i;

    for (page = 0; page < 4; page++) {
        if (page == selected >> 3) {
            cell = page * 8;
            for (i = 0; i < (page < 3 ? 8 : 4); i++) {
                if (cell == selected) {
                    x -= 4;
                    menuDrawCell(0x12F, x, y, 1);
                    x += 0x15;
                } else {
                    menuDrawCell(0x15C, x, y + 0xC, 1);
                    x += 7;
                }
                cell++;
            }
        } else {
            menuDrawCell(0x15D + page, x, y + 4, 1);
            x += 0xE;
        }
    }
}

/* Row of count cells, 0x12F at selected; with more than wideCellLimit cells they shrink to
   0x15C and, from 15 cells on, wrap after cell 13 into a second row. 0x1C cells draw as
   menuDrawPageTabs's page tabs. */
void menuDrawPageCells(int x, int y, int selected, int count, int wideCellLimit)
{
    int wrapX;
    int i;

    if (count == 0x1C) {
        menuDrawPageTabs(x, y, selected);
        return;
    }
    wrapX = -99999;
    if (count >= 15 && wideCellLimit < count) {
        wrapX = x;
        y -= 8;
    }
    for (i = 0; i < count; i++) {
        if (i == selected) {
            x -= 4;
            if (wideCellLimit < count) {
                x--;
            }
            menuDrawCell(0x12F, x, y, 1);
            x += 0x18;
            if (wideCellLimit < count) {
                x -= 3;
            }
        } else if (wideCellLimit < count) {
            menuDrawCell(0x15C, x, y + 0xC, 1);
            x += 7;
        } else {
            menuDrawCell(0x15B, x, y + 0xC, 1);
            x += 0xC;
        }
        if (wrapX != -99999 && i == 13) {
            x = wrapX;
            y += 8;
        }
    }
}

/* List window: a 0xE0 x 0x11F panel; with two or more entries it adds the 0x1C mark and the rows (menuDrawPageCells).
   */
void menuDrawListWindow(int x, int y, int layer, int count)
{
    menuDrawListFrame(x, y, 0xE0, 0x11F);
    if (count >= 2) {
        menuDrawCell(0x1C, x + 0x94, y + 0x10C, 0x40);
        textDrawDigits(x + 0xC0, y + 0x10C, layer + 1, 2);
        menuDrawPageCells(x + 0xC, y + 0x101, layer, count, 8);
    }
}


/* Front-party member icons at x - count*0xC + i*0x18: lit (menuDrawLitIcon) when
   selectionHolderHasEntry approves the entry, dimmed (menuDrawDimIcon) otherwise; a found
   selectionFindIndex match adds the 7 mark. */
void menuDrawFrontMemberIcons(int x, int y, int record)
{
    int offset;
    int memberX;
    int i = 0;
    PartyMember *member;

    if (ptState.counts[1] != 0) {
        offset = 0;
        do {
            member = ptState.slots[i];
            memberX = (x - ptState.counts[1] * 0xC) + offset;
            offset += 0x18;
            if (selectionHolderHasEntry(member, record)) {
                menuDrawLitIcon(memberX, y, member->status->memberIndex);
            } else {
                menuDrawDimIcon(memberX, y, member->status->memberIndex);
            }
            if (selectionFindIndex(member, record) != -1) {
                fontObjSetColor(0x3F8000008072804FLL);
                menuDrawStatIcon(7, memberX + 2, y - 0x12, 0x3F800000802B2900LL);
                fontObjResetColor();
            }
            i++;
        } while (i < ptState.counts[1]);
    }
}

extern const char *menuLevelClassTexts[];

const unsigned char menuArrowZoomText[24] = "$zoom6,13;>$zoom13;";

/* Character info line: label, then (mode>0) the group name and (mode>=2) the slot
   name; group>=3 and slot>=4 with mode 2 draws a single "none" segment instead. */
void menuDrawCharInfo(int x, int y, int group, int slot, int mode)
{
    textDrawSegment(x, y, textColorCloseTag, 0);
    if (group >= 3 && slot >= 4 && mode == 2) {
        textDrawSegment(x + 6, y, "----", 0);
        return;
    }
    textDrawSegment(x, y, menuArrowZoomText, 0);
    if (mode <= 0) {
        return;
    }
    textDrawSegment(x + 6, y, "Level", 0);
    textDrawSegment(x + 0x30, y, menuLevelClassTexts[group], 0);
    textDrawSegment(x + 0x3A, y, menuArrowZoomText, 0);
    if (mode >= 2) {
        textDrawSegment(x + 0x40, y, "Class", 0);
        textDrawSegment(x + 0x66, y, menuLevelClassTexts[4 + slot], 0);
        textDrawSegment(x + 0x76, y, menuArrowZoomText, 0);
    }
}


#define MENU_LIST_COLOR_IDLE 0x3F80000000000000LL
#define MENU_LIST_COLOR_SELECTED 0x3F80000080120B50LL

const unsigned char msgStyleInit[72] = "$init;$bg2;$shadow+1,+3,000000;$zoom13;$han1;$prop1;$xx0,21;$colc0c0c0;";

/* Item window: up to ten rows of selectionUsableItemList from the cursor's first visible
   row, each with its count (dataBoxChkCategory1) and name (hitActionTextGet); ineligible entries
   are drawn dimmed. */
void menuDrawItemList(int x, int y, ListCursor *cursor)
{
    int firstRow = cursorBufferGetAlignedOffset(cursor);
    int page = cursorBufferGetIndex(cursor);
    int i;
    int rowStyle;

    menuDrawListWindow(x, y, page, menuListPageCount(cursor));
    menuDrawTitledFrame(x + 8, y + 0x20, 0xA1, 0xDF, "  List");
    menuDrawTitledFrame(x + 0xAB, y + 0x20, 0x2D, 0xDF, "  Num");
    menuDrawCell(0x12B, x + 0x43, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    for (i = 0; i < 10 && i + firstRow < selectionWindowCount; i++) {
        rowStyle = firstRow + i == cursor->cursor;
        textDrawDecimalShadow(x + 0xB9, y + 0x32 + i * 0x14, dataBoxChkCategory1(selectionUsableItemList[firstRow + i]), 2,
                              rowStyle ? MENU_LIST_COLOR_SELECTED : MENU_LIST_COLOR_IDLE);
        if (!selectionEntryIsEligible(firstRow + i)) {
            rowStyle |= 2;
        }
        textDrawSegment(x + 0x18, y + 0x30 + i * 0x14, hitActionTextGet(selectionUsableItemList[firstRow + i], 0),
                        rowStyle);
    }
    if (cursor->mode_flags & 1) {
        menuPlaceCursor(x + 0xA, y + 0x30 + (cursor->cursor - firstRow) * 0x14, 0x9D, cursor->mode_flags & 1);
        menuDrawCursor();
    }
}


/* Two-column item window: up to 18 rows (9 per column) from the cursor's first visible
   row; candidates whose selectionKeyItemBitTest bit is clear show entry 0x190 instead
   of their name and count. */
void menuDrawItemListDetail(int x, int y, ListCursor *cursor)
{
    int firstRow = cursorBufferGetAlignedOffset(cursor);
    int i;
    int columnX;
    int rowY;
    int count;
    int rowStyle;

    menuDrawListFrame(x, y, 0x1C2, 0x11F);
    menuDrawCell(0x1C, x + 0x176, y + 0x10C, 0x40);
    textDrawDigits(x + 0x1A2, y + 0x10C, cursorBufferGetIndex(cursor) + 1, 2);
    menuDrawPageCells(x + 0xC, y + 0x101, cursorBufferGetIndex(cursor), menuListPageCount(cursor), 0x14);
    menuDrawBorder(x + 8, y + 0x20, 0xD8, 0xDF, 0x3F80000080381A1ALL);
    menuDrawTitledFrame(x + 0xC, y + 0x25, 0xA1, 0xD5, "  List");
    menuDrawTitledFrame(x + 0xAF, y + 0x25, 0x2D, 0xD5, "  Num");
    menuDrawBorder(x + 0xE2, y + 0x20, 0xD8, 0xDF, 0x3F80000080381A1ALL);
    menuDrawTitledFrame(x + 0xE6, y + 0x25, 0xA1, 0xD5, "  List");
    menuDrawTitledFrame(x + 0x189, y + 0x25, 0x2D, 0xD5, "  Num");
    menuDrawCell(0x12B, x + 0xB4, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    for (i = 0; i < 18 && i + firstRow < cursor->capacity; i++) {
        columnX = x + (i / 9) * 0xDA;
        rowY = y + (i % 9) * 0x15 + 0x36;
        count = dataBoxChkCategory3(firstRow + i + 1);
        rowStyle = firstRow + i == cursor->cursor;
        if (selectionKeyItemBitTest(firstRow + i + 1)) {
            textDrawDecimalShadow(columnX + 0xBD, rowY + 2, count, 2,
                                  rowStyle ? MENU_LIST_COLOR_SELECTED : MENU_LIST_COLOR_IDLE);
            textDrawSegment(columnX + 0x12, rowY, dataBoxCategory3TextGet(firstRow + i + 1, 0), rowStyle);
        } else {
            textDrawSegment(columnX + 0x12, rowY, menuMainTxdEntry(0x190), rowStyle);
        }
    }
    if (cursor->mode_flags & 1) {
        menuPlaceCursor(x + ((cursor->cursor - firstRow) / 9) * 0xDA + 0xE,
                      y + ((cursor->cursor - firstRow) % 9) * 0x15 + 0x36, 0x9D, cursor->mode_flags & 1);
        menuDrawCursor();
    }
}

/* 1 when item is one of the member's buffer entries. */
int menuMemberHasEntry(PartyMember *member, unsigned char item)
{
    int found;
    int i;

    found = 0;
    for (i = 0; i < member->buffer->entry_count; i++) {
        if (member->buffer->entries[i] == item) {
            found = 1;
            break;
        }
    }
    return found;
}



/* Plain item list ("  List" / "  Num" columns): up to eight rows of selectionAccessoryList
   from the first visible row with their counts, the page marks when the list has
   more than one page, then the icons of the back members (slots 7-9) other than the
   current character, marked when the selected item is in that member's list. */
void menuDrawPlainItemList(int x, int y, ListCursor *cursor)
{
    int firstRow = cursorBufferGetAlignedOffset(cursor);
    int page = cursorBufferGetIndex(cursor);
    int pageCount = menuListPageCount(cursor);
    int i;
    int rowStyle;
    unsigned char icon;

    menuDrawListFrame(x, y, 0xCF, 0x11F);
    if (pageCount >= 2) {
        menuDrawCell(0x1C, x + 0x83, y + 0x10C, 0x40);
        textDrawDigits(x + 0xAF, y + 0x10C, page + 1, 2);
        menuDrawPageCells(x + 0xC, y + 0x101, page, pageCount, 8);
    }
    menuDrawTitledFrame(x + 8, y + 0x20, 0x90, 0xBE, "  List");
    menuDrawTitledFrame(x + 0x9A, y + 0x20, 0x2D, 0xBE, "  Num");
    menuDrawCell(0x141, x + 0x1E, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    for (i = 0; i < 8 && i + firstRow < selectionPlainCount; i++) {
        rowStyle = (firstRow + i) == cursor->cursor;
        textDrawDecimalShadow(x + 0xA8, y + i * 0x14 + 0x34, dataBoxChkCategory2(selectionAccessoryList[firstRow + i]), 2,
                              rowStyle ? MENU_LIST_COLOR_SELECTED : MENU_LIST_COLOR_IDLE);
        textDrawSegment(x + 0x10, y + i * 0x14 + 0x32, itemTextGet(selectionAccessoryList[firstRow + i], 0), rowStyle);
    }
    if (cursor->mode_flags & 1) {
        menuPlaceCursor(x + 0xA, y + (cursor->cursor - firstRow) * 0x14 + 0x32, 0x8C, 1);
        menuDrawCursor();
    }
    /* x now walks the member icons. */
    x += 0x30;
    for (i = 7; i < 10; i++) {
        if (ptState.slots[i] != 0 && ptGetMenuMember()->id != ptState.slots[i]->id) {
            icon = ptState.slots[i]->status->memberIndex;
            if (cursor->capacity != 0 && menuMemberHasEntry(ptState.slots[i], selectionAccessoryList[cursor->cursor])) {
                menuDrawLitIcon(x, y + 0xE3, icon);
                fontObjSetColor(0x3F80000060708040LL);
                menuDrawCell(0x2E, x + 0x1B, y + 0xE9, 5);
                fontObjResetColor();
            } else {
                menuDrawDimIcon(x, y + 0xE3, icon);
            }
            x += 0x4A;
        }
    }
}

/* Ether list of the selected party member: member tabs (0x166 for the selected one),
   then one row per ether with its EP cost; a row whose cost exceeds the member
   record's ep is drawn dimmed. Only the cost
   lookup tests ptGetMenuMember's member for null; the dim test reads it unchecked. */
void menuDrawMemberItemList(int x, int y, ListCursor *cursor, unsigned char *items)
{
    int firstRow;
    int tabX;
    int tab;
    unsigned int i;
    int count;
    int memberCount;
    int rowStyle;
    unsigned char selected = ptState.menuSlot;

    firstRow = cursorBufferGetAlignedOffset(cursor);
    menuDrawListFrame(x, y, 0xE0, 0xAA);
    menuDrawTitledFrame(x + 8, y + 0x20, 0xA1, 0x6A, "  List");
    menuDrawTitledFrame(x + 0xAB, y + 0x20, 0x2D, 0x6A, "  EP");
    menuDrawCell(0x12A, x + 0x41, y + 4, 1);
    menuDrawCell(0x43, x + 0x98, y + 0x97, 0x40);
    if (ptState.splitMode == PARTY_SPLIT_ON) {
        memberCount = ptState.counts[2];
    } else {
        memberCount = ptState.counts[1];
    }
    if (memberCount >= 2) {
        tabX = x + 0x10;
        for (tab = 0; tab < memberCount; tab++) {
            if (tab == selected) {
                tabX -= 2;
                menuDrawCell(0x166, tabX, y + 0x8A, 1);
                tabX += 0x10;
            } else {
                menuDrawCell(0x167, tabX, y + 0x94, 1);
                tabX += 9;
            }
        }
    }
    textDrawSegment(0, 0, msgStyleInit, 0);
    for (i = 0; i < cursor->capacity; i++) {
        count = 0;
        if (ptGetMenuMember() != 0) {
            count = calcTakeEpGetMenu(ptGetMenuMember()->id, items[i]);
        }
        rowStyle = (firstRow + i) == cursor->cursor;
        textDrawDecimalShadow(x + 0xB9, y + i * 0x14 + 0x32, count, 2,
                              rowStyle ? MENU_LIST_COLOR_SELECTED : MENU_LIST_COLOR_IDLE);
        if (calcTotalParaMenu(ptGetMenuMember()->id)->ep < count) {
            rowStyle |= 2;
        }
        textDrawSegment(x + 0x18, y + i * 0x14 + 0x30, battleLookupTextGet(items[i], 0), rowStyle);
    }
    if (cursor->mode_flags & 1) {
        menuPlaceCursor(x + 0xA, y + (cursor->cursor - firstRow) * 0x14 + 0x30, 0x9D, 1);
        menuDrawCursor();
    }
}

/* The default menu text colour (64-bit RGBA+Q word). */
#define MENU_PARTY_TEXT_COLOR 0x3F8000008054200BLL

/* Draws a stat value, colour-coded against a second value (menuDrawCharStats): a
   marker is drawn beside it when the values differ. */
void menuDrawStatCompare(int x, int y, int value, int compare)
{
    unsigned long long accentColour;
    unsigned long long textColour;

    if (value > compare) {
        accentColour = 0x3F8000008010104BULL;
        textColour = 0x3F80000080545B80ULL;
        menuDrawCompareMark(x + 0x1C, y, 0, accentColour);
    } else if (value < compare) {
        accentColour = 0x3F80000080641B15ULL;
        textColour = 0x3F80000080805148ULL;
        menuDrawCompareMark(x + 0x1C, y, 1, accentColour);
    } else {
        accentColour = MENU_PARTY_TEXT_COLOR;
        textColour = 0x3F80000080808080ULL;
    }

    fontObjSetColor(textColour);
    textDrawDecimal(x, y, value, 3, accentColour);
    fontObjResetColor();
}

extern SelectionRecord selectionRecordTable[];
/* Copy of the member's record taken before the preview equip. */

const unsigned char totalExpText[16] = "Total Exp";

/* Stat comparison panel: saves the member's record, equips item in slot (slot >= 0),
   draws the stat labels and each stat against the saved copy, then the level panel
   (Next Lv, Total Exp). Then, for slot >= 0, the slot is re-equipped: below entry_count
   with its previous entry (members outside 0-10 get the entry byte itself, members 0-10
   that entry's selectionRecordTable dispatch_value, and when that is 0x23 hp and
   ep come back from the saved copy), at or past entry_count with 0. The previous entry
   is read before the slot test, as in the asm: with slot -1 it is the byte in front of
   entries (flags[1]) and is not used. */
void menuDrawCharStats(int x, int y, PartyMember *member, int item, int slot)
{
    StatBlock *record;
    unsigned char previous;
    SelectionRecord *entry;

    record = calcTotalParaMenu(member->id);
    charStatsSavedRecord = *record;
    previous = member->buffer->entries[slot];
    if (slot > -1) {
        if (member->id < 0 || member->id > 10) {
            calcAccEquipOrg(member->id, slot, item);
        } else {
            calcSkillEquipOrg(member->id, slot, item);
        }
    }
    record = calcTotalParaMenu(member->id);
    panelDrawNineSlice(menuPanelFrame, x, y, 0x56, 0x8B);
    if (ptState.splitMode == PARTY_SPLIT_ON) {
        textDrawGlyphString(x + 8, y + 0xA, "Pow", MENU_PARTY_TEXT_COLOR);
    } else {
        textDrawGlyphString(x + 8, y + 0xA, "Str", MENU_PARTY_TEXT_COLOR);
    }
    if (ptState.splitMode == PARTY_SPLIT_ON) {
        textDrawGlyphString(x + 8, y + 0x1C, "Arm", MENU_PARTY_TEXT_COLOR);
    } else {
        textDrawGlyphString(x + 8, y + 0x1C, "Vit", MENU_PARTY_TEXT_COLOR);
    }
    textDrawGlyphString(x + 8, y + 0x2E, "Dex", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 8, y + 0x40, "Eva", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 8, y + 0x52, "Agl", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 8, y + 0x64, "Eatk", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 8, y + 0x76, "Edef", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 0x2A, y + 0xA, ":", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 0x2A, y + 0x1C, ":", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 0x2A, y + 0x2E, ":", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 0x2A, y + 0x40, ":", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 0x2A, y + 0x52, ":", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 0x2A, y + 0x64, ":", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 0x2A, y + 0x76, ":", MENU_PARTY_TEXT_COLOR);
    menuDrawStatCompare(x + 0x35, y + 0xA, record->physAttack, charStatsSavedRecord.physAttack);
    menuDrawStatCompare(x + 0x35, y + 0x1C, record->physDefence, charStatsSavedRecord.physDefence);
    menuDrawStatCompare(x + 0x35, y + 0x2E, record->hitRate, charStatsSavedRecord.hitRate);
    menuDrawStatCompare(x + 0x35, y + 0x40, record->evadeRate, charStatsSavedRecord.evadeRate);
    menuDrawStatCompare(x + 0x35, y + 0x52, record->valueBias, charStatsSavedRecord.valueBias);
    menuDrawStatCompare(x + 0x35, y + 0x64, record->magAttack, charStatsSavedRecord.magAttack);
    menuDrawStatCompare(x + 0x35, y + 0x76, record->magDefence, charStatsSavedRecord.magDefence);
    if (ptState.splitMode == PARTY_SPLIT_ON) {
        x -= 0x11;
        panelDrawNineSlice(menuPanelFrame, x - 0xA3, y + 0x56, 0xAE, 0x35);
    } else {
        panelDrawNineSlice(menuPanelFrame, x - 0xA3, y + 0x56, 0x9D, 0x35);
    }
    textDrawGlyphString(x - 0x9E, y + 0x60, "Next Lv", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x - 0x9E, y + 0x74, totalExpText, MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x - 0x58, y + 0x60, ":", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x - 0x58, y + 0x74, ":", MENU_PARTY_TEXT_COLOR);
    if (ptState.splitMode == PARTY_SPLIT_ON) {
        x += 0x11;
    }
    textDrawDecimal(x - 0x53, y + 0x60, member->limits->next_level_exp, 8, MENU_PARTY_TEXT_COLOR);
    textDrawDecimalAlt(x - 0x51, y + 0x74, member->limits->total_exp, 10, MENU_PARTY_TEXT_COLOR);
    if (slot > -1) {
        if (slot < member->buffer->entry_count) {
            if (member->id < 0 || member->id > 10) {
                calcAccEquipOrg(member->id, slot, previous);
            } else {
                entry = &selectionRecordTable[previous];
                calcSkillEquipOrg(member->id, slot, entry->dispatch_value);
                if (entry->dispatch_value == 0x23) {
                    record = battleSlotGet(member->id);
                    record->hp = charStatsSavedRecord.hp;
                    record->ep = charStatsSavedRecord.ep;
                }
            }
        } else {
            if (member->id < 0 || member->id > 10) {
                calcAccEquipOrg(member->id, slot, 0);
            } else {
                calcSkillEquipOrg(member->id, slot, 0);
            }
        }
    }
}

extern int menuFlipCornerPanelFrame[10];

/* Equip slot list: one row per entry in items, a text label (itemTextGet's text, the one
   menuDrawPlainItemList lists, while the party split is open, otherwise selectionRecordTextGet's
   record text) and the selection cursor box. */
void menuDrawEquipPanel(int x, int y, ListCursor *cursor, unsigned char *items, unsigned char rowCount)
{
    int firstRow;
    int i;
    int rowStyle;

    firstRow = cursorBufferGetAlignedOffset(cursor);
    panelDrawNineSliceFlipCorner(menuFlipCornerPanelFrame, x, y, 0x9D, rowCount * 0x14 + 0x37);
    menuDrawTitledFrame(x + 8, y + 0xF, 0x94, rowCount * 0x14 + 0x19, "  Equip");
    textDrawSegment(0, 0, msgStyleInit, 0);

    for (i = 0; i < cursor->capacity; i++) {
        rowStyle = (firstRow + i) == cursor->cursor;
        if (ptState.splitMode == PARTY_SPLIT_ON) {
            textDrawSegment(x + 0x10, y + 0x20 + i * 0x14, itemTextGet(items[i], 0), rowStyle | 4);
        } else {
            textDrawSegment(x + 0x10, y + 0x20 + i * 0x14, selectionRecordTextGet(items[i], 0), rowStyle | 4);
        }
    }

    if (cursor->mode_flags & 1) {
        menuPlaceCursor(x + 0xA, y + (cursor->cursor - firstRow) * 0x14 + 0x20, 0x90, 1);
        menuDrawCursor();
    }
}



/* A selection entry: the record index (group << 5) + (slot << 2) + sub. Copied by value
   into selectionEntryOutsideMask: the asm keeps the loaded byte in its own register and masks
   it with 0xFF for the range test and the group and slot fields. Compiled with a plain
   `unsigned char id` parameter (items as unsigned char *), gcc drops that andi, tests the
   loaded byte directly and schedules the sub field after the call setup, so the struct stays. */
typedef struct SelectionRecordId {
    unsigned char id;
} SelectionRecordId;


/* Whether the entry is a record entry (below 0x70) whose bit lies outside mask. */
static inline int selectionEntryOutsideMask(SelectionRecordId entry, int mask)
{
    if (entry.id < 0x70) {
        return selectionEntryBitOutsideMask(entry.id >> 5, (entry.id >> 2) & 7, entry.id & 3, mask);
    }
    return 0;
}

const unsigned char menuCategoryTitleText[16] = "  Category";

/* Selection entry list of one category: four rows from the first visible one (an
   entry outside mask shows text 0x190), the kind mark of each entry (0x133, 0x131 or
   0x132), the cursor, and the selected entry's detail (0x7F with an empty list). */
void menuDrawItemCategory(int x, int y, ListCursor *cursor, unsigned char *items, int mask)
{
    SelectionRecordId entry;
    int firstRow = cursorBufferGetAlignedOffset(cursor);
    int page = cursorBufferGetIndex(cursor);
    int i;
    int outside;
    int rowStyle;
    unsigned char kind;

    menuDrawListWindow(x, y, page, menuListPageCount(cursor));
    menuDrawBorder(x + 8, y + 0x1F, 0xD0, 0x2E, 0x3F80000080381A1ALL);
    menuDrawTitledFrame(x + 0xC, y + 0x24, 0xC8, 0x24, menuCategoryTitleText);
    menuDrawTitledFrame(x + 8, y + 0x51, 0xA4, 0x77, "  List");
    menuDrawTitledFrame(x + 0xAD, y + 0x51, 0x2B, 0x77, " Sort");
    menuDrawCell(0x12C, x + 0x35, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    if (cursor->capacity != 0) {
        menuDrawCharInfo(x + 0x18, y + 0x31, items[cursor->cursor] >> 5, (items[cursor->cursor] >> 2) & 7, 2);
    }
    for (i = 0; i < 4 && firstRow + i < cursor->capacity; i++) {
        entry.id = items[firstRow + i];
        outside = selectionEntryOutsideMask(entry, mask);
        rowStyle = (firstRow + i) == cursor->cursor;
        if (selectionHolderHasEntry(ptGetMenuMember(), items[firstRow + i]) == 0) {
            rowStyle |= 2;
        }
        if (outside) {
            textDrawSegment(x + 0x18, y + i * 0x17 + 0x62, menuMainTxdEntry(0x190), rowStyle);
        } else {
            textDrawSegment(x + 0x18, y + i * 0x17 + 0x62, selectionRecordTextGet(items[firstRow + i], 0),
                            rowStyle);
            kind = selectionRecordTable[items[firstRow + i]].kind;
            if (kind == 0 || kind == 1) {
                if (kind == 0) {
                    menuDrawCell(0x133, x + 0xBA, y + i * 0x17 + 0x62, 0);
                } else {
                    menuDrawCell(0x131, x + 0xBA, y + i * 0x17 + 0x62, 0);
                }
            } else {
                menuDrawCell(0x132, x + 0xBA, y + i * 0x17 + 0x62, 0);
            }
        }
    }
    if (cursor->mode_flags & 1) {
        menuPlaceCursor(x + 0xA, y + (cursor->cursor - firstRow) * 0x17 + 0x62, 0xA0, cursor->mode_flags & 1);
        menuDrawCursor();
    }
    if (cursor->capacity != 0) {
        menuDrawFrontMemberIcons(x + 0x70, y + 0xE0, items[cursor->cursor]);
    } else {
        menuDrawFrontMemberIcons(x + 0x70, y + 0xE0, 0x7F);
    }
}

void menuDrawKindIcon(int x, int y, int index)
{
    fontObjSetColor(0x3F80000080000000LL);
    menuDrawCell(0x44, x, y, 4);
    fontObjResetColor();
    menuDrawCell(index + 0x47, x, y, 1);
}

const unsigned char normalAttackTitleText[16] = "  Normal Attack";

const unsigned char menuSingleTitleText[16] = "  Single";

const unsigned char menuCommandTitleText[16] = "  Command";

const unsigned char textZoom12Tag[16] = "$zoom12;";

/* Attack command list of a party member: seven rows, each with the command text and
   its kind icons (kind 3 shows text 0x14A for members from 11 on, kind 4 text 0x14B).
   Members below 11 have seven commands of their own (rows from 7 on use the shared
   records from 0x5F), the others ten. */
void menuDrawAttackCommand(int x, int y, ListCursor *cursor, int member)
{
    int firstRow = cursorBufferGetAlignedOffset(cursor);
    int early = member < 11;
    int i;
    int rowStyle;
    int iconX;
    int index;

    panelDrawNineSliceFlipCorner(menuFlipCornerPanelFrame, x, y, 0xFD, 0xC7);
    if (!early) {
        menuDrawTitledFrame(x + 8, y + 0xF, 0xA0, 0xA9, normalAttackTitleText);
    } else {
        menuDrawTitledFrame(x + 8, y + 0xF, 0xA0, 0xA9, menuSingleTitleText);
    }
    menuDrawTitledFrame(x + 0xAA, y + 0xF, 0x4B, 0xA9, menuCommandTitleText);
    menuDrawCell(0x169, x + 0xB8, y - 0x14, 1);
    menuDrawCell(0x16A, x + 0xD0, y - 0x14, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    textDrawSegment(0, 0, textZoom12Tag, 0);
    for (i = 0; i < 7 && i + firstRow < cursor->capacity; i++) {
        rowStyle = (firstRow + i) == cursor->cursor;
        if (!early) {
            iconX = x + 0xC6;
            index = (member - 11) * 10 + i + 0x31;
        } else {
            iconX = x + 0xB4;
            index = (member - 1) * 7 + i;
            if (firstRow + i >= 7) {
                index = i + 0x5F;
            }
        }
        textDrawSegment(x + 0x10, y + 0x1F + i * 0x15, attackRecordTable[index].text, rowStyle);
        if (attackRecordTable[index].attributes[0] != 0) {
            if (!early && attackRecordTable[index].attributes[0] == 3) {
                textDrawSegment(iconX + 2, y + 0x1F + i * 0x15, menuMainTxdEntry(0x14A), rowStyle);
            } else if (attackRecordTable[index].attributes[0] == 4) {
                textDrawSegment(iconX - 2, y + 0x1F + i * 0x15, menuMainTxdEntry(0x14B), rowStyle);
            } else {
                menuDrawKindIcon(iconX, y + 0x1F + i * 0x15, attackRecordTable[index].attributes[0]);
            }
        }
        if (attackRecordTable[index].attributes[1] != 0) {
            menuDrawKindIcon(iconX + 0x12, y + 0x1F + i * 0x15, attackRecordTable[index].attributes[1]);
        }
        if (attackRecordTable[index].attributes[2] != 0) {
            menuDrawKindIcon(iconX + 0x24, y + 0x1F + i * 0x15, attackRecordTable[index].attributes[2]);
        }
    }
    if (cursor->mode_flags & 1) {
        menuPlaceCursor(x + 0xA, y + (cursor->cursor - firstRow) * 0x15 + 0x1F, 0x9C, cursor->mode_flags & 1);
        menuDrawCursor();
    }
}


const unsigned char specialAttackTitleText[24] = "  Special Attack";

const unsigned char menuDoubleTitleText[16] = "  Double";

const unsigned char menuChargeTitleText[16] = "  Charge";

const unsigned char menuPartnerTitleText[16] = "  Partner";

/* Special attack list of a party member. From member 11 on: the attack's kind text,
   dimmed unless the bit of the member's copilot (ptGetCopilotId: the id in the roster
   entry paired with the member, 0 for none) is set in the attack's member mask, the one
   menuDrawCopilotInfo draws as "Copilot". Below 11: combination attacks, with the icon of
   the partner member. */
void menuDrawSpecialAttack(int x, int y, ListCursor *cursor, int member, unsigned short mode)
{
    int firstRow = cursorBufferGetAlignedOffset(cursor);
    int early = member < 11;
    unsigned int i;
    int rowStyle;
    int index;
    int copilotId;

    panelDrawNineSliceFlipCorner(menuFlipCornerPanelFrame, x, y, 0xFD, 0xC7);
    if (!early) {
        menuDrawTitledFrame(x + 8, y + 0xF, 0xA0, 0xA9, specialAttackTitleText);
    } else {
        menuDrawTitledFrame(x + 8, y + 0xF, 0xA0, 0xA9, menuDoubleTitleText);
    }
    if (!early) {
        menuDrawTitledFrame(x + 0xAA, y + 0xF, 0x4B, 0xA9, menuChargeTitleText);
    } else {
        menuDrawTitledFrame(x + 0xAA, y + 0xF, 0x4B, 0xA9, menuPartnerTitleText);
    }
    menuDrawCell(0x168, x + 0xB8, y - 0x14, 1);
    menuDrawCell(0x16B, x + 0xD0, y - 0x14, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    textDrawSegment(0, 0, textZoom12Tag, 0);
    for (i = 0; i < cursor->capacity; i++) {
        rowStyle = (firstRow + i) == cursor->cursor;
        if (!early) {
            index = selectionCopilotRecordAt(member, i, mode);
            copilotId = ptGetCopilotId(member);
            if (copilotId <= 0 || !((attackRecordTable[index].attributes[1] >> (copilotId - 1)) & 1)) {
                rowStyle |= 2;
            }
            textDrawSegment(x + 0xBE, y + i * 0x15 + 0x1F, menuMainTxdEntry(attackRecordTable[index].attributes[0] + 0x14D),
                            rowStyle);
            textDrawSegment(x + 0x10, y + i * 0x15 + 0x1F, attackRecordTable[index].text, rowStyle);
        } else {
            index = selectionPairRecordAt(member, i, mode);
            if (index != -1) {
                if (attackRecordTable[index].attributes[0] == member) {
                    menuDrawLitIconWide(x + 0xC4, y + i * 0x15 + 0x1F,
                                  ptMemberRecordAt(attackRecordTable[index].attributes[1] - 1)->status->memberIndex);
                } else {
                    menuDrawLitIconWide(x + 0xC4, y + i * 0x15 + 0x1F,
                                  ptMemberRecordAt(attackRecordTable[index].attributes[0] - 1)->status->memberIndex);
                }
                textDrawSegment(x + 0x10, y + i * 0x15 + 0x1F, attackRecordTable[index].text, rowStyle);
            }
        }
    }
    if (cursor->mode_flags & 1) {
        menuPlaceCursor(x + 0xA, y + (cursor->cursor - firstRow) * 0x15 + 0x1F, 0x9C, cursor->mode_flags & 1);
        menuDrawCursor();
    }
}
/* Defined in save_party_init.c as 175 raw bytes (see the comment there); its records are LookupRecords. */
extern struct LookupRecord sharedCommandRecordTable[];

int sprintf(char *buffer, const char *format, ...);

const unsigned char menuDetailsTitleText[16] = "  Details";

const unsigned char textLabelColorTag[16] = "$col708040;";

const unsigned char menuAttributeText[16] = "Attribute";

const unsigned char menuConditionText[16] = "Condition";

/* Command detail panel of an attack command (cursor rows below 7 are member's own
   commands, the others shared ones): "Command" (kind icons, or text 0x14B), "Target",
   "Zone", "Type", "Range", "Attribute" (joined with "/"), "Condition" and "Effect". */
void menuDrawCommandInfo(int x, int y, ListCursor *cursor, int member)
{
    struct LookupRecord *record;
    int i;
    int attributeX;
    int separate;
    char countText[16];

    if (cursor->cursor >= 7) {
        record = &sharedCommandRecordTable[cursor->cursor];
    } else {
        record = &attackRecordTable[(member - 1) * 7 + cursor->cursor];
    }
    panelDrawNineSlice(menuInfoPanelFrame, x, y, 0xE0, 0x11F);
    menuDrawBorder(x + 8, y + 0x1F, 0xD0, 0x2E, 0x3F80000080381A1ALL);
    menuDrawTitledFrame(x + 0xC, y + 0x24, 0xC8, 0x24, "  Name");
    menuDrawTitledFrame(x + 8, y + 0x51, 0xD0, 0xBE, menuDetailsTitleText);
    menuDrawCell(0x128, x + 0x27, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    textDrawSegment(0, 0, "$prop1;", 0);
    textDrawSegment(0, 0, textLabelColorTag, 0);
    textDrawSegment(x + 0x10, y + 0x60, "Command", 0);
    textDrawSegment(x + 0x10, y + 0x74, "Target", 0);
    textDrawSegment(x + 0x10, y + 0x88, "Zone", 0);
    textDrawSegment(x + 0x10, y + 0x9C, "Type", 0);
    textDrawSegment(x + 0x10, y + 0xB0, "Range", 0);
    textDrawSegment(x + 0x10, y + 0xC4, menuAttributeText, 0);
    textDrawSegment(x + 0x10, y + 0xD8, menuConditionText, 0);
    textDrawSegment(x + 0x10, y + 0xEC, "Effect", 0);
    textDrawSegment(0, 0, textColorCloseTag, 0);
    for (i = 0; i < 8; i++) {
        textDrawSegment(x + 0x58, y + i * 0x14 + 0x62, ":", 0);
    }
    textDrawSegment(x + 0x18, y + 0x32, record->text, 0);
    if (record->attributes[0] < 4) {
        if (record->attributes[0] != 0) {
            menuDrawKindIcon(x + 0x62, y + 0x60, record->attributes[0]);
        }
        if (record->attributes[1] != 0) {
            menuDrawKindIcon(x + 0x74, y + 0x60, record->attributes[1]);
        }
        if (record->attributes[2] != 0) {
            menuDrawKindIcon(x + 0x86, y + 0x60, record->attributes[2]);
        }
    } else {
        textDrawSegment(x + 0x62, y + 0x60, menuMainTxdEntry(0x14B), 0);
    }
    if (record->target != 0) {
        textDrawSegment(x + 0x64, y + 0x77, menuMainTxdEntry(record->target + 0x111), 0);
    }
    textDrawSegment(x + 0x64, y + 0x8B, menuMainTxdEntry(record->zone + 0x116), 0);
    if (record->type != 0) {
        textDrawSegment(x + 0x64, y + 0x9F, menuMainTxdEntry(record->type + 0x125), 0);
    }
    if (record->range != 0) {
        textDrawSegment(x + 0x64, y + 0xB3, menuMainTxdEntry(record->range + 0x12B), 0);
    }
    attributeX = 0;
    separate = 0;
    for (i = 0; i < 9; i++) {
        if ((record->elementMask >> i) & 1) {
            if (separate) {
                textDrawSegment(x + attributeX + 0x64, y + 0xC7, "/", 0);
                attributeX += 7;
            }
            textDrawSegment(x + attributeX + 0x64, y + 0xC7, menuMainTxdEntry(i + 0x130), 0);
            attributeX += (xglFontGetTextWidth(menuMainTxdEntry(i + 0x130)) * 13) >> 4;
            separate = 1;
        }
    }
    if (record->condition != 0) {
        if (record->condition >= 4) {
            sprintf(countText, "%1d", record->condition - 3);
            textDrawSegment(x + 0x64, y + 0xDB, menuMainTxdEntry(0x140), 0);
            textDrawSegment(x + 0xA0, y + 0xDB, countText, 0);
        } else {
            textDrawSegment(x + 0x64, y + 0xDB, menuMainTxdEntry(record->condition + 0x13A), 0);
        }
    }
    if (record->effect != 0) {
        textDrawSegment(x + 0x64, y + 0xEF, menuMainTxdEntry(record->effect + 0x140), 0);
    }
}

/* Detail panel of a combination attack: "Partner" (name text 0x10 + partner and
   icon), "Target", "Type", "Attribute" (text 0xEF for types 1 and 2 without
   attributes), "Condition" and "Effect". */
void menuDrawPartnerInfo(int x, int y, ListCursor *cursor, int member, unsigned short mode)
{
    struct LookupRecord *record;
    int partner;
    int i;
    int attributeX;
    int separate;
    char countText[16];

    panelDrawNineSlice(menuInfoPanelFrame, x, y, 0xE0, 0x11F);
    menuDrawBorder(x + 8, y + 0x1F, 0xD0, 0x2E, 0x3F80000080381A1ALL);
    menuDrawTitledFrame(x + 0xC, y + 0x24, 0xC8, 0x24, "  Name");
    menuDrawTitledFrame(x + 8, y + 0x51, 0xD0, 0xBE, menuDetailsTitleText);
    menuDrawCell(0x128, x + 0x27, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    textDrawSegment(0, 0, "$prop1;", 0);
    textDrawSegment(0, 0, textLabelColorTag, 0);
    textDrawSegment(x + 0x10, y + 0x60, "Partner", 0);
    textDrawSegment(x + 0x10, y + 0x74, "Target", 0);
    textDrawSegment(x + 0x10, y + 0x88, "Type", 0);
    textDrawSegment(x + 0x10, y + 0x9C, menuAttributeText, 0);
    textDrawSegment(x + 0x10, y + 0xB0, menuConditionText, 0);
    textDrawSegment(x + 0x10, y + 0xC4, "Effect", 0);
    textDrawSegment(0, 0, textColorCloseTag, 0);
    for (i = 0; i < 6; i++) {
        textDrawSegment(x + 0x58, y + i * 0x14 + 0x62, ":", 0);
    }
    /* i is reused for the record index. */
    i = selectionPairRecordAt(member, cursor->cursor, mode);
    if (i != -1) {
        record = &attackRecordTable[i];
        textDrawSegment(x + 0x18, y + 0x32, record->text, 0);
        if (record->attributes[0] != member) {
            partner = record->attributes[0];
        } else {
            partner = record->attributes[1];
        }
        textDrawSegment(0, 0, "$prop1;", 0);
        textDrawSegment(x + 0x64, y + 0x63, menuMainTxdEntry(partner + 0x10), 0);
        menuDrawLitIcon(x + 0xB4, y + 0x61, ptMemberRecordAt(partner - 1)->status->memberIndex);
        if (record->target != 0) {
            textDrawSegment(x + 0x64, y + 0x77, menuMainTxdEntry(record->target + 0x111), 0);
        }
        if (record->type != 0) {
            textDrawSegment(x + 0x64, y + 0x8B, menuMainTxdEntry(record->type + 0x125), 0);
        }
        attributeX = 0;
        separate = 0;
        if (record->elementMask == 0) {
            if (record->type == 1 || record->type == 2) {
                textDrawSegment(x + 0x64, y + 0x9F, menuMainTxdEntry(0xEF), 0);
            }
        } else {
            for (i = 0; i < 9; i++) {
                if ((record->elementMask >> i) & 1) {
                    if (separate) {
                        textDrawSegment(x + attributeX + 0x64, y + 0x9F, "/", 0);
                        attributeX += 7;
                    }
                    textDrawSegment(x + attributeX + 0x64, y + 0x9F, menuMainTxdEntry(i + 0x130), 0);
                    attributeX += (xglFontGetTextWidth(menuMainTxdEntry(i + 0x130)) * 13) >> 4;
                    separate = 1;
                }
            }
        }
        if (record->condition < 10) {
            sprintf(countText, "%1d", record->condition);
            textDrawSegment(x + 0x64, y + 0xB3, menuMainTxdEntry(0x140), 0);
            textDrawSegment(x + 0xA0, y + 0xB3, countText, 0);
        }
        if (record->effect != 0) {
            textDrawSegment(x + 0x64, y + 0xC7, menuMainTxdEntry(record->effect + 0x140), 0);
        }
    }
}

/* Command detail panel of member's selected command (ten records per member):
   "Command", "Target", "Type", "Range", "Attribute", "Effect" and "Copilot" (always
   "None") rows with their values; the attributes are joined with "/". */
void menuDrawCommandDetail(int x, int y, ListCursor *cursor, int member)
{
    struct LookupRecord *record = &attackRecordTable[(member - 7) * 10 + 9 + cursor->cursor];
    int i;
    int attributeX;
    int separate;

    panelDrawNineSlice(menuInfoPanelFrame, x, y, 0xE0, 0x11F);
    menuDrawBorder(x + 8, y + 0x1F, 0xD0, 0x2E, 0x3F80000080381A1ALL);
    menuDrawTitledFrame(x + 0xC, y + 0x24, 0xC8, 0x24, "  Name");
    menuDrawTitledFrame(x + 8, y + 0x51, 0xD0, 0xBE, menuDetailsTitleText);
    menuDrawCell(0x128, x + 0x27, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    textDrawSegment(0, 0, "$prop1;", 0);
    textDrawSegment(0, 0, textLabelColorTag, 0);
    textDrawSegment(x + 0x10, y + 0x60, "Command", 0);
    textDrawSegment(x + 0x10, y + 0x74, "Target", 0);
    textDrawSegment(x + 0x10, y + 0x88, "Type", 0);
    textDrawSegment(x + 0x10, y + 0x9C, "Range", 0);
    textDrawSegment(x + 0x10, y + 0xB0, menuAttributeText, 0);
    textDrawSegment(x + 0x10, y + 0xC4, "Effect", 0);
    textDrawSegment(x + 0x10, y + 0xD8, "Copilot", 0);
    textDrawSegment(0, 0, textColorCloseTag, 0);
    for (i = 0; i < 7; i++) {
        textDrawSegment(x + 0x58, y + i * 0x14 + 0x62, ":", 0);
    }
    textDrawSegment(x + 0x18, y + 0x32, record->text, 0);
    if (record->attributes[0] != 0) {
        if (record->attributes[0] == 3) {
            textDrawSegment(x + 0x64, y + 0x63, menuMainTxdEntry(0x14A), 0);
        } else {
            menuDrawKindIcon(x + 0x62, y + 0x60, record->attributes[0]);
        }
    }
    if (record->target != 0) {
        textDrawSegment(x + 0x64, y + 0x77, menuMainTxdEntry(record->target + 0x111), 0);
    }
    if (record->type != 0) {
        textDrawSegment(x + 0x64, y + 0x8B, menuMainTxdEntry(record->type + 0x125), 0);
    }
    if (record->range != 0) {
        textDrawSegment(x + 0x64, y + 0x9F, menuMainTxdEntry(record->range + 0x12B), 0);
    }
    attributeX = 0;
    separate = 0;
    for (i = 0; i < 9; i++) {
        if ((record->elementMask >> i) & 1) {
            if (separate) {
                textDrawSegment(x + attributeX + 0x64, y + 0xB3, "/", 0);
                attributeX += 7;
            }
            textDrawSegment(x + attributeX + 0x64, y + 0xB3, menuMainTxdEntry(i + 0x130), 0);
            attributeX += (xglFontGetTextWidth(menuMainTxdEntry(i + 0x130)) * 13) >> 4;
            separate = 1;
        }
    }
    if (record->effect != 0) {
        textDrawSegment(x + 0x64, y + 0xC7, menuMainTxdEntry(record->effect + 0x140), 0);
    }
    textDrawSegment(x + 0x64, y + 0xDB, "None", 0);
}

/* Detail panel of a special attack: "Copilot" (icons of the front members whose bit
   is set in the attack's member mask), "Charge" (kind text 0x14D + n), "Target",
   "Type", "Range", "Attribute" and "Effect"; the effect text is cut at each backslash-n
   escape: every cut piece is drawn on the same first row and the rest 0x14 lower, so
   there are two rows whatever the number of escapes. i is reused as the record index and
   as the effect text index. */
void menuDrawCopilotInfo(int x, int y, ListCursor *cursor, int member, unsigned short mode)
{
    unsigned char line[32];
    int lineY = 0;
    int i;
    struct LookupRecord *record;
    int iconX;
    int attributeX;
    int separate;
    unsigned char *text;

    panelDrawNineSlice(menuInfoPanelFrame, x, y, 0xE0, 0x11F);
    menuDrawBorder(x + 8, y + 0x1F, 0xD0, 0x2E, 0x3F80000080381A1ALL);
    menuDrawTitledFrame(x + 0xC, y + 0x24, 0xC8, 0x24, "  Name");
    menuDrawTitledFrame(x + 8, y + 0x51, 0xD0, 0xBE, menuDetailsTitleText);
    menuDrawCell(0x128, x + 0x27, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    textDrawSegment(0, 0, "$prop1;", 0);
    textDrawSegment(0, 0, textLabelColorTag, 0);
    textDrawSegment(x + 0x10, y + 0x60, "Copilot", 0);
    textDrawSegment(x + 0x10, y + 0x7C, "Charge", 0);
    textDrawSegment(x + 0x10, y + 0x90, "Target", 0);
    textDrawSegment(x + 0x10, y + 0xA4, "Type", 0);
    textDrawSegment(x + 0x10, y + 0xB8, "Range", 0);
    textDrawSegment(x + 0x10, y + 0xCC, menuAttributeText, 0);
    textDrawSegment(x + 0x10, y + 0xE0, "Effect", 0);
    textDrawSegment(0, 0, textColorCloseTag, 0);
    /* Rows after the first ("Copilot") sit 8 lower. */
    for (i = 0; i < 7; i++) {
        textDrawSegment(x + 0x58, y + i * 0x14 + (i <= 0 ? 0x62 : 0x6A), ":", 0);
    }
    i = selectionCopilotRecordAt(member, cursor->cursor, mode);
    if (i != -1) {
        record = &attackRecordTable[i];
        textDrawSegment(x + 0x18, y + 0x32, record->text, 0);
        iconX = x + 0x64;
        for (i = 0; i < 7; i++) {
            if (ptState.slots[i] != 0 && ((record->attributes[1] >> (ptState.slots[i]->id - 1)) & 1)) {
                menuDrawLitIcon(iconX, y + 0x61, ptState.slots[i]->status->memberIndex);
                iconX += 0x18;
            }
        }
        if (record->attributes[0] != 0) {
            textDrawSegment(x + 0x64, y + 0x7F, menuMainTxdEntry(record->attributes[0] + 0x14D), 0);
        }
        if (record->target != 0) {
            textDrawSegment(x + 0x64, y + 0x93, menuMainTxdEntry(record->target + 0x111), 0);
        }
        if (record->type != 0) {
            textDrawSegment(x + 0x64, y + 0xA7, menuMainTxdEntry(record->type + 0x125), 0);
        }
        if (record->range != 0) {
            textDrawSegment(x + 0x64, y + 0xBB, menuMainTxdEntry(record->range + 0x12B), 0);
        }
        attributeX = 0;
        separate = 0;
        for (i = 0; i < 9; i++) {
            if ((record->elementMask >> i) & 1) {
                if (separate) {
                    textDrawSegment(x + attributeX + 0x64, y + 0xCF, "/", 0);
                    attributeX += 7;
                }
                textDrawSegment(x + attributeX + 0x64, y + 0xCF, menuMainTxdEntry(i + 0x130), 0);
                attributeX += (xglFontGetTextWidth(menuMainTxdEntry(i + 0x130)) * 13) >> 4;
                separate = 1;
            }
        }
        if (record->effect != 0) {
            text = menuMainTxdEntry(record->effect + 0x140);
            for (i = 0; text[i] != 0; i++) {
                if (text[i] == '\\' && text[i + 1] == 'n') {
                    line[i] = 0;
                    text += i + 2;
                    i = 0;
                    textDrawSegment(x + 0x64, y + 0xE3, line, 0);
                    lineY = 0x14;
                    line[i] = text[i];
                } else {
                    line[i] = text[i];
                }
            }
            line[i] = 0;
            textDrawSegment(x + 0x64, y + lineY + 0xE3, line, 0);
        }
    }
}

const unsigned char classLevelText[16] = "Class Lv";

const unsigned char learnedSkillsText[16] = "Learned S :";

/* Class panel: class level ("Class Lv", buffer flags[0] + 1) with the class points (C.Pt),
   learned skills (flags[1]) with the skill points (S.Pt). Nothing without a control context. */
void menuDrawClassInfo(int x, int y)
{
    PartyMember *context = ptGetMenuMember();

    if (context != 0) {
        panelDrawNineSlice(menuPanelFrame, x, y, 0xE5, 0x32);
        textDrawGlyphString(x + 0xA, y + 9, classLevelText, MENU_PARTY_TEXT_COLOR);
        textDrawGlyphString(x + 0x53, y + 9, ":", MENU_PARTY_TEXT_COLOR);
        textDrawDecimal(x + 0x60, y + 9, context->buffer->openGroup + 1, 3, MENU_PARTY_TEXT_COLOR);
        textDrawGlyphString(x + 0x86, y + 9, "C.Pt :", MENU_PARTY_TEXT_COLOR);
        textDrawDecimal(x + 0xB1, y + 9, context->limits->classPoints, 5, MENU_PARTY_TEXT_COLOR);
        textDrawGlyphString(x + 0xA, y + 0x1C, learnedSkillsText, MENU_PARTY_TEXT_COLOR);
        textDrawDecimal(x + 0x60, y + 0x1C, context->buffer->takenCount, 3, MENU_PARTY_TEXT_COLOR);
        textDrawGlyphString(x + 0x86, y + 0x1C, "S.Pt :", MENU_PARTY_TEXT_COLOR);
        textDrawDecimal(x + 0xB1, y + 0x1C, context->limits->skillPoints, 5, MENU_PARTY_TEXT_COLOR);
    }
}


#define LICENSE_ROW_PITCH 0x17 /* rows of the group and sub pages */
#define LICENSE_ROW_PITCH_COMPACT 0x12 /* rows of the slot page */
#define LICENSE_MARK_SHADOW 0x3F80000080000000LL
#define LICENSE_MARK_FILL 0x3F80000080407080LL
#define LICENSE_MARK_CMD 0x80 /* request command of one mark */
#define LICENSE_MARK_BITS 4

const unsigned char skillBoardTitleText[16] = "  License";

/* License board page: three tabs (mode 0, 1, 2), each with its own list of rows. */
void menuDrawSkillBoard(int x, int y, ListCursor *groupCursor, ListCursor *slotCursor,
                          ListCursor *subCursor, int mode, int mask)
{
    int firstRow; /* first visible row; scrolls the highlight */
    int unlocked = 0;
    int markCount = 0;
    PartyTakenBits *board = ptGetMenuMember()->buffer;
    ListCursor *cursor;
    int i;
    int rowStyle;
    unsigned long long color;
    int k;
    int tabX;
    int record;
    int kind;

    switch (mode) {
    case 0:
    default:
        menuDrawTitledFrame(x + 8, y + 0x51, 0x9F, 0xAE, "  List");
        menuDrawTitledFrame(x + 0xA9, y + 0x51, 0x44, 0xAE, skillBoardTitleText);
        cursor = groupCursor;
        break;
    case 1:
        menuDrawTitledFrame(x + 8, y + 0x51, 0xA9, 0xAE, "  List");
        menuDrawTitledFrame(x + 0xB3, y + 0x51, 0x3A, 0xAE, "  C.Pt");
        cursor = slotCursor;
        break;
    case 2:
        menuDrawTitledFrame(x + 8, y + 0x51, 0xA9, 0x77, "  List");
        menuDrawTitledFrame(x + 0xB3, y + 0x51, 0x3A, 0x77, "  S.Pt");
        cursor = subCursor;
        break;
    }
    firstRow = cursorBufferGetAlignedOffset(cursor);
    menuDrawListFrame(x, y, 0xF5, 0x11F);
    menuDrawBorder(x + 8, y + 0x1F, 0xE5, 0x2E, 0x3F80000080381A1ALL);
    menuDrawTitledFrame(x + 0xC, y + 0x24, 0xDD, 0x24, "  Route");
    menuDrawCell(0x12E, x + 0x4C, y + 4, 1);
    textDrawSegment(0, 0, msgStyleInit, 0);
    menuDrawCharInfo(x + 0x18, y + 0x31, groupCursor->cursor, slotCursor->cursor, mode);

    for (i = 0; i < cursor->capacity; i++) {
        switch (mode) {
        case 0:
            if (i <= board->openGroup) {
                unlocked = 1;
            } else {
                unlocked = 0;
            }
            break;
        case 1:
            if (selectionCanOpenSlot(ptGetMenuMember(), groupCursor->cursor, i)
                || ((board->header[groupCursor->cursor] >> i) & 1)) {
                unlocked = 1;
            } else {
                unlocked = 0;
            }
            markCount = 0;
            for (k = 0; k < LICENSE_MARK_BITS; k++) {
                markCount += (board->cells[groupCursor->cursor][i] >> k) & 1;
            }
            break;
        case 2:
            if (selectionCanTakeSub(ptGetMenuMember(), groupCursor->cursor, slotCursor->cursor, i)
                || ((board->cells[groupCursor->cursor][slotCursor->cursor] >> i) & 1)) {
                unlocked = 1;
            } else {
                unlocked = 0;
            }
            break;
        }
        rowStyle = i == cursor->cursor;
        color = rowStyle ? MENU_LIST_COLOR_SELECTED : MENU_LIST_COLOR_IDLE;
        if (!unlocked) {
            rowStyle |= 2;
        }
        switch (mode) {
        case 0:
        default:
            textDrawSegment(x + 0x18, y + 0x62 + i * LICENSE_ROW_PITCH, menuMainTxdEntry(i + 0x36), rowStyle);
            if (unlocked) {
                textDrawGlyphString(x + 0xB8, y + 0x66 + i * LICENSE_ROW_PITCH, "PASS", color);
            } else {
                textDrawGlyphString(x + 0xBA, y + 0x66 + i * LICENSE_ROW_PITCH, "----", color);
            }
            break;
        case 1:
            textDrawSegment(x + 0x18, y + 0x62 + i * LICENSE_ROW_PITCH_COMPACT, menuMainTxdEntry(i + 0x3A), rowStyle);
            for (k = 0; k < markCount; k++) {
                fontObjSetColor(LICENSE_MARK_SHADOW);
                menuDrawCell(LICENSE_MARK_CMD, x + 0x71 + k * 0xE, y + 0x67 + i * LICENSE_ROW_PITCH_COMPACT, 0x44);
                fontObjSetColor(LICENSE_MARK_FILL);
                menuDrawCell(LICENSE_MARK_CMD, x + 0x70 + k * 0xE, y + 0x64 + i * LICENSE_ROW_PITCH_COMPACT, 0x44);
                fontObjResetColor();
            }
            if ((board->header[groupCursor->cursor] >> i) & 1) {
                textDrawGlyphString(x + 0xBF, y + 0x66 + i * LICENSE_ROW_PITCH_COMPACT, "----", color);
            } else {
                if (selectionSlotOpenCostTable[groupCursor->cursor * 8 + i] > ptGetMenuMember()->limits->classPoints) {
                    fontObjSetOpacity(0x40);
                } else {
                    fontObjResetOpacity();
                }
                textDrawDecimalShadow(x + 0xBF, y + 0x66 + i * LICENSE_ROW_PITCH_COMPACT, selectionSlotOpenCostTable[groupCursor->cursor * 8 + i], 4, color);
            }
            break;
        case 2:
            {
                int outside = selectionEntryBitOutsideMask(groupCursor->cursor, slotCursor->cursor, i, mask);
                record = selectionRecordIndex(groupCursor->cursor, slotCursor->cursor, i);

                if (outside) {
                    rowStyle |= 2;
                    textDrawSegment(x + 0x10, y + 0x62 + i * LICENSE_ROW_PITCH, menuMainTxdEntry(0x190), rowStyle);
                    textDrawGlyphString(x + 0xBF, y + 0x66 + i * LICENSE_ROW_PITCH, "----", color);
                } else {
                    textDrawSegment(x + 0x10, y + 0x62 + i * LICENSE_ROW_PITCH,
                                    selectionSubTextGet(groupCursor->cursor, slotCursor->cursor, i, 0), rowStyle);
                    kind = selectionRecordTable[record].kind;
                    if (kind == 0 || kind == 1) {
                        if (kind == 0) {
                            menuDrawCell(0x133, x + 0x98, y + 0x62 + i * LICENSE_ROW_PITCH, 0);
                        } else {
                            menuDrawCell(0x131, x + 0x98, y + 0x62 + i * LICENSE_ROW_PITCH, 0);
                        }
                    } else {
                        menuDrawCell(0x132, x + 0x98, y + 0x62 + i * LICENSE_ROW_PITCH, 0);
                    }
                    if ((board->cells[groupCursor->cursor][slotCursor->cursor] >> i) & 1) {
                        fontObjSetColor(LICENSE_MARK_SHADOW);
                        menuDrawCell(LICENSE_MARK_CMD, x + 0xCB, y + 0x69 + i * LICENSE_ROW_PITCH, 0x44);
                        fontObjSetColor(LICENSE_MARK_FILL);
                        menuDrawCell(LICENSE_MARK_CMD, x + 0xCA, y + 0x66 + i * LICENSE_ROW_PITCH, 0x44);
                        fontObjResetColor();
                    } else {
                        if (selectionSubCostGet(groupCursor->cursor, slotCursor->cursor, i)
                            > ptGetMenuMember()->limits->skillPoints) {
                            fontObjSetOpacity(0x40);
                        } else {
                            fontObjResetOpacity();
                        }
                        textDrawDecimalShadow(x + 0xBF, y + 0x66 + i * LICENSE_ROW_PITCH,
                                              selectionSubCostGet(groupCursor->cursor, slotCursor->cursor, i), 4, color);
                    }
                }
            }
            break;
        }
        fontObjResetOpacity();
    }

    if (cursor->mode_flags & 1) {
        switch (mode) {
        case 0:
            menuPlaceCursor(x + 0xA, y + 0x62 + (cursor->cursor - firstRow) * LICENSE_ROW_PITCH, 0x9B,
                            cursor->mode_flags & 1);
            break;
        case 1:
            menuPlaceCursor(x + 0xA, y + 0x62 + (cursor->cursor - firstRow) * LICENSE_ROW_PITCH_COMPACT, 0xA5,
                            cursor->mode_flags & 1);
            break;
        case 2:
            menuPlaceCursor(x + 0xA, y + 0x62 + (cursor->cursor - firstRow) * LICENSE_ROW_PITCH, 0xA5,
                            cursor->mode_flags & 1);
            break;
        }
        menuDrawCursor();
    }

    tabX = x + 0x10;
    for (i = 0; i < 3; i++) {
        if (i == mode) {
            tabX -= 4;
            menuDrawCell(0x130, tabX, y + 0x101, 1);
            tabX += 0x18;
        } else {
            menuDrawCell(0x15A, tabX, y + 0x10B, 1);
            tabX += 0xC;
        }
    }
    switch (mode) {
    case 0:
    default:
        menuDrawCell(0x21, x + 0xB2, y + 0x10C, 0x40);
        break;
    case 1:
        menuDrawCell(0x81, x + 0xB2, y + 0x10D, 0x40);
        break;
    case 2:
        menuDrawCell(0x22, x + 0xB2, y + 0x10C, 0x40);
        {
            int entry = selectionRecordIndex(groupCursor->cursor, slotCursor->cursor, subCursor->cursor);

            menuDrawFrontMemberIcons(x + 0x7A, y + 0xE0, entry);
        }
        break;
    }
}

/* The save settings the option page shows (saveDataSetDefaults sets them: +0x28 = 1, +0x3A = 2, the rest 0). */
/* the shown value of each option row */
extern signed char menuConfigLabelTable[];
/* per option: value label group, 0 when it shows none */

const unsigned char menuConfigTitleText[16] = "  Configuration";

const unsigned char instructionTitleText[16] = "  Instruction";

/* Option page: eight option rows (items holds their ids), the default row (index 8) and the cursor. */
void menuDrawConfig(int x, int y, ListCursor *cursor, unsigned char *items)
{
    int rowY;
    int i;
    int selected;
    int labelX;
    int defaultY;
    int hiddenRow; /* cursor index for which this page draws no cursor */

    panelDrawNineSlice(menuPanelFrame, x - 0x28, y, 0x15E, 0xC4);
    menuDrawTitledFrame(x - 0x24, y + 5, 0x156, 0xBA, menuConfigTitleText);
    panelDrawNineSlice(menuPanelFrame, x - 0x28, y + 0xCE, 0x15E, 0x36);
    menuDrawTitledFrame(x - 0x24, y + 0xD3, 0x156, 0x2C, instructionTitleText);
    configValueState[0] = SaveData.cursor_memory;
    configValueState[1] = SaveData.cursor_memory_alt;
    configValueState[2] = SaveData.soundMode;
    configValueState[3] = SaveData.actuatorMode == 0;
    configValueState[4] = SaveData.caption_all != 0;
    configValueState[7] = SaveData.dolby_digital;
    configValueState[6] = 0;
    configValueState[5] = 0;
    textDrawSegment(0, 0, msgStyleInit, 0);
    rowY = y + 0x16;
    labelX = x - 0x10;
    for (i = 0; i < 8; i++) {
        selected = i == cursor->cursor;
        textDrawSegment(labelX, rowY, menuMainTxdEntry(items[i] + 0x156), selected);
        if (menuConfigLabelTable[items[i]] != 0) {
            int label = menuConfigLabelTable[items[i]] * 2 + 0x161;

            textDrawSegment(x + 0x94, rowY, menuMainTxdEntry(label + configValueState[items[i]]), selected);
        }
        rowY += 0x14;
    }
    defaultY = y + 0xE4;
    selected = cursor->cursor == 8;
    textDrawSegment(labelX, defaultY, menuMainTxdEntry(0x15E), selected);
    hiddenRow = 9;
    if (cursor->cursor != hiddenRow && (cursor->mode_flags & 1)) {
        if (selected) {
            menuPlaceCursor(x - 0x22, defaultY, 0x152, 1);
        } else {
            menuPlaceCursor(x - 0x22, y + 0x16 + cursor->cursor * 0x14, 0x152, 1);
        }
        menuDrawCursor();
    }
}

const unsigned char controllerTitleText[24] = "  Controller";

/* Controller page: the pad picture with its shaded parts and the four button labels. */
void menuDrawControllerConfig(void)
{
    panelDrawNineSlice(menuPanelFrame, 9, 0x5C, 0x1EE, 0x121);
    menuDrawTitledFrame(0xD, 0x61, 0x1E6, 0x117, controllerTitleText);
    menuDrawCell(0x2BF, 0, 0xAC, 0);
    menuDrawCell(0x2C0, 0, 0xAC, 1);
    menuDrawCell(0x2C1, 0xFA, 0x10B, 1);
    fontObjSetColor(0x3F800000802E2E2ELL);
    menuDrawCell(0x2CE, 0, 0x5C, 0x45);
    fontObjSetColor(LICENSE_MARK_SHADOW);
    menuDrawCell(0x2C2, 1, 0x5E, 0x44);
    fontObjSetColor(0x3F80000080787850LL);
    menuDrawCell(0x2C2, 0, 0x5B, 0x44);
    fontObjSetColor(LICENSE_MARK_SHADOW);
    menuDrawCell(0x2C3, 1, 0x5F, 0x44);
    fontObjSetColor(0x3F80000080535353LL);
    menuDrawCell(0x2C3, 0, 0x5C, 0x40);
    fontObjSetColor(0x3F8000008051464ALL);
    menuDrawCell(0x2CA, 0xAB, 0x72, 0x44);
    menuDrawCell(0x2C9, 0xAB, 0x91, 0x44);
    menuDrawCell(0x2CC, 0x12F, 0x72, 0x44);
    menuDrawCell(0x2CB, 0x12F, 0x91, 0x44);
    fontObjSetColor(LICENSE_MARK_SHADOW);
    menuDrawCell(0x83, 0x194, 0x72, 4);
    menuDrawCell(0x83, 0x194, 0x9A, 4);
    menuDrawCell(0x83, 0x194, 0xC2, 4);
    menuDrawCell(0x83, 0x194, 0xEA, 4);
    fontObjResetColor();
    menuDrawCell(0x85, 0x194, 0x72, 1);
    menuDrawCell(0x87, 0x194, 0x9A, 1);
    menuDrawCell(0x84, 0x194, 0xC2, 1);
    menuDrawCell(0x86, 0x194, 0xEA, 1);
}

/* The play time panel: now minus the progress record's play time, clamped by PartyTimeDispChange. */
void menuDrawPlayTime(int x, int y)
{
    XglClock playTime;
    unsigned int now;
    int rowY;

    xglClockRead(&playTime);
    now = xglClockDayTime2UInt(&playTime);
    rowY = y + 0xF;
    xglClockUInt2DayTime(&playTime, now - ptState.progress->playTime);
    PartyTimeDispChange(&playTime);
    menuDrawTitledFrame(x, y, 0x6B, 0x21, "  Time");
    textDrawGlyphString(x + 0x23, rowY, ":", MENU_PARTY_TEXT_COLOR);
    textDrawGlyphString(x + 0x3F, rowY, ":", MENU_PARTY_TEXT_COLOR);
    textDrawDecimal(x + 0x10, rowY, playTime.hour, 2, MENU_PARTY_TEXT_COLOR);
    textDrawDecimalZero(x + 0x2D, rowY, playTime.minute, 2, MENU_PARTY_TEXT_COLOR);
    textDrawDecimalZero(x + 0x49, rowY, playTime.second, 2, MENU_PARTY_TEXT_COLOR);
}

void menuResetItem(MenuItem *item)
{
    if (item != 0) {
        item->member = -1;
        fontTrackSetBoth(&item->position, 0, 0);
        fontTrackSetBoth(&item->highlight, 0, 0);
        item->flags = 0;
    }
}

/* The member entry of a menu item: none for member -1, else
   ptState.roster[member] for a MENU_ITEM_ROSTER item (as in menuDrawItems), ptState.slots[member]
   otherwise. */
PartyMember *menuItemMemberEntry(MenuItem *item)
{
    int index = item->member;

    if (index == -1) {
        return 0;
    }
    if (item->flags & MENU_ITEM_ROSTER) {
        return ptState.roster[index];
    }
    return ptState.slots[index];
}

/* Icon pair at (x, y): piece 0xA2 + kind in `colour`, then piece 0xAA + kind in the font colour
   that was set on entry. Only the low word of that colour is kept (the asm sign-extends it, then
   passes it on zero-extended), so the second piece is drawn with a zero upper half (Q). */
void menuDrawStatIcon(int kind, int x, int y, u64 colour)
{
    int savedColourLow;

    savedColourLow = (int) fontObjGetColor();
    kind += 0xA2;
    fontObjSetColor(colour);
    menuDrawCell(kind, x, y, 4);
    fontObjSetColor((unsigned int) savedColourLow);
    menuDrawCell(kind + 8, x, y, 4);
}

void menuDrawLevel(int x, int y, int level)
{
    fontObjSetColor(0x3F80000080805939LL);
    menuDrawStatIcon(4, x, y + 1, MENU_PARTY_TEXT_COLOR);
    fontObjResetColor();
    textDrawDecimal(x + 0x14, y + 4, level, 2, MENU_PARTY_TEXT_COLOR);
}

/* Text colours of an HP value against its maximum (StatBlock's hp and maxHp): dead, at a
   quarter or less, below the maximum, full. */
void menuHpColours(int hp, int maxHp, u64 *shadowColour, u64 *faceColour)
{
    *faceColour = 0x3F80000080808080LL;
    if (hp <= 0) {
        *shadowColour = MENU_LIST_COLOR_SELECTED;
        *faceColour = 0x3F80000080575480LL;
    } else if (maxHp >= hp * 4) {
        *shadowColour = 0x3F8000008029224ALL;
        *faceColour = 0x3F8000008060FFFFLL;
    } else {
        *shadowColour = MENU_PARTY_TEXT_COLOR;
        if (hp != maxHp) {
            *faceColour = 0x3F80000080787878LL;
        }
    }
}

void menuDrawHp(int x, int y, int hp, int maxHp)
{
    u64 shadowColour;
    u64 faceColour;

    fontObjSetColor(0x3F800000807D763FLL);
    menuDrawStatIcon(0, x, y, 0x3F80000080572000LL);
    fontObjResetColor();
    menuHpColours(hp, maxHp, &shadowColour, &faceColour);
    textDrawGlyphString(x + 0x3C, y + 4, "/", MENU_PARTY_TEXT_COLOR);
    textDrawDecimal(x + 0x45, y + 4, maxHp, 4, MENU_PARTY_TEXT_COLOR);
    fontObjSetColor(faceColour);
    textDrawDecimal(x + 0x18, y + 4, hp, 4, shadowColour);
    fontObjResetColor();
}

void menuDrawEp(int x, int y, int ep, int maxEp)
{
    fontObjSetColor(0x3F800000807D763FLL);
    menuDrawStatIcon(1, x, y, 0x3F80000080572000LL);
    fontObjResetColor();
    textDrawGlyphString(x + 0x3C, y + 4, "/", MENU_PARTY_TEXT_COLOR);
    textDrawDecimal(x + 0x21, y + 4, ep, 3, MENU_PARTY_TEXT_COLOR);
    textDrawDecimal(x + 0x45, y + 4, maxEp, 3, MENU_PARTY_TEXT_COLOR);
}

void menuDrawHpWide(int x, int y, int hp, int maxHp)
{
    u64 shadowColour;
    u64 faceColour;

    fontObjSetColor(0x3F800000807D763FLL);
    menuDrawStatIcon(0, x, y, 0x3F80000080572000LL);
    fontObjResetColor();
    menuHpColours(hp, maxHp, &shadowColour, &faceColour);
    textDrawGlyphString(x + 0x44, y + 4, "/", MENU_PARTY_TEXT_COLOR);
    textDrawDecimal(x + 0x4D, y + 4, maxHp, 5, MENU_PARTY_TEXT_COLOR);
    fontObjSetColor(faceColour);
    textDrawDecimal(x + 0x18, y + 4, hp, 5, shadowColour);
    fontObjResetColor();
}

void menuDrawEpWide(int x, int y, int ep, int maxEp)
{
    fontObjSetColor(0x3F800000807D763FLL);
    menuDrawStatIcon(1, x, y, 0x3F80000080572000LL);
    fontObjResetColor();
    textDrawGlyphString(x + 0x44, y + 4, "/", MENU_PARTY_TEXT_COLOR);
    textDrawDecimal(x + 0x29, y + 4, ep, 3, MENU_PARTY_TEXT_COLOR);
    textDrawDecimal(x + 0x4D, y + 4, maxEp, 3, MENU_PARTY_TEXT_COLOR);
}

void menuDrawLevelShadow(int x, int y, int level)
{
    fontObjSetColor(LICENSE_MARK_SHADOW);
    menuDrawStatIcon(4, x + 1, y + 3, MENU_LIST_COLOR_IDLE);
    fontObjSetColor(0x3F80000080805939LL);
    menuDrawStatIcon(4, x, y, MENU_LIST_COLOR_IDLE);
    fontObjResetColor();
    textDrawDecimalShadow(x + 0x14, y + 4, level, 2, MENU_LIST_COLOR_IDLE);
}

void menuDrawHpShadow(int x, int y, int hp, int maxHp)
{
    u64 shadowColour;
    u64 faceColour;

    fontObjSetColor(LICENSE_MARK_SHADOW);
    menuDrawStatIcon(0, x + 1, y + 3, MENU_LIST_COLOR_IDLE);
    fontObjSetColor(0x3F800000807D763FLL);
    menuDrawStatIcon(0, x, y, MENU_LIST_COLOR_IDLE);
    fontObjResetColor();
    menuHpColours(hp, maxHp, &shadowColour, &faceColour);
    textDrawShadow(x + 0x3C, y + 4, "/", MENU_LIST_COLOR_IDLE);
    textDrawDecimalShadow(x + 0x45, y + 4, maxHp, 4, MENU_LIST_COLOR_IDLE);
    fontObjSetColor(faceColour);
    textDrawDecimalShadow(x + 0x18, y + 4, hp, 4, MENU_LIST_COLOR_IDLE);
    fontObjResetColor();
}

void menuDrawEpShadow(int x, int y, int ep, int maxEp)
{
    fontObjSetColor(LICENSE_MARK_SHADOW);
    menuDrawStatIcon(1, x + 1, y + 3, MENU_LIST_COLOR_IDLE);
    fontObjSetColor(0x3F800000807D763FLL);
    menuDrawStatIcon(1, x, y, MENU_LIST_COLOR_IDLE);
    fontObjResetColor();
    textDrawShadow(x + 0x3C, y + 4, "/", MENU_LIST_COLOR_IDLE);
    textDrawDecimalShadow(x + 0x21, y + 4, ep, 3, MENU_LIST_COLOR_IDLE);
    textDrawDecimalShadow(x + 0x45, y + 4, maxEp, 3, MENU_LIST_COLOR_IDLE);
}

void menuDrawHpWideShadow(int x, int y, int hp, int maxHp)
{
    u64 shadowColour;
    u64 faceColour;

    fontObjSetColor(LICENSE_MARK_SHADOW);
    menuDrawStatIcon(0, x + 1, y + 3, MENU_LIST_COLOR_IDLE);
    fontObjSetColor(0x3F800000807D763FLL);
    menuDrawStatIcon(0, x, y, MENU_LIST_COLOR_IDLE);
    fontObjResetColor();
    menuHpColours(hp, maxHp, &shadowColour, &faceColour);
    textDrawShadow(x + 0x44, y + 4, "/", MENU_LIST_COLOR_IDLE);
    textDrawDecimalShadow(x + 0x4D, y + 4, maxHp, 5, MENU_LIST_COLOR_IDLE);
    fontObjSetColor(faceColour);
    textDrawDecimalShadow(x + 0x18, y + 4, hp, 5, MENU_LIST_COLOR_IDLE);
    fontObjResetColor();
}

void menuDrawEpWideShadow(int x, int y, int ep, int maxEp)
{
    fontObjSetColor(LICENSE_MARK_SHADOW);
    menuDrawStatIcon(1, x + 1, y + 3, MENU_LIST_COLOR_IDLE);
    fontObjSetColor(0x3F800000807D763FLL);
    menuDrawStatIcon(1, x, y, MENU_LIST_COLOR_IDLE);
    fontObjResetColor();
    textDrawShadow(x + 0x44, y + 4, "/", MENU_LIST_COLOR_IDLE);
    textDrawDecimalShadow(x + 0x29, y + 4, ep, 3, MENU_LIST_COLOR_IDLE);
    textDrawDecimalShadow(x + 0x4D, y + 4, maxEp, 3, MENU_LIST_COLOR_IDLE);
}

/* The blinking band (cell 0x134) at (x, y): its offset_x steps through eight 0x10-pixel frames with
   the tick counter. */
void menuDrawAnimatedBand(int x, int y)
{
    packetRequest.x = x;
    packetRequest.y = y;
    packetRequest.cell_index = 0x134;
    packetRequest.offset_x = (xglTickGet() & 7) * 0x10;
    packetRequest.flags = 0x21;
    packetRequest.offset_y = 0;
    fontObjDraw(&packetRequest);
}

/* A framed panel from three tracks: pos gives the top-left corner, size the outer width and
   height, tabs the widths of the top (current_pair.x) and bottom (current_pair.y) tabs. The request commands
   are command + 0..11 for the pieces, 0xC1 for the top tab mark. */
void menuDrawTrackPanel(int command, struct FontTrack *pos, struct FontTrack *size, struct FontTrack *tabs)
{
    int y = pos->current_pair.y;
    int x = pos->current_pair.x;
    int width;

    menuDrawCell(command + 7, x, y, 1);
    x += 0x10;
    width = size->current_pair.x - tabs->current_pair.x - 0x30;
    menuDrawCellSized(command + 8, x, y, width, 0x16, 1);
    x += width;
    menuDrawCell(command + 9, x, y, 1);
    x += 0x10;
    width = tabs->current_pair.x;
    menuDrawCellSized(command + 10, x, y, width, 0x16, 1);
    menuDrawCell(0xC1, x + ((width - fontObjCellOffsetX(0xC1)) >> 1) + 4, y + 4, 1);
    menuDrawCell(command + 11, x + width, y, 1);
    y += 0x16;

    x = pos->current_pair.x;
    width = size->current_pair.y - 0x2C;
    menuDrawCellSized(command + 5, x, y, 0x10, width, 1);
    menuDrawCellSized(command + 6, x + size->current_pair.x - 0x10, y, 0x10, width, 1);
    y += width;

    x = pos->current_pair.x;
    menuDrawCell(command, x, y, 1);
    x += 0x10;
    width = tabs->current_pair.y;
    menuDrawCellSized(command + 1, x, y, width, 0x16, 1);
    x += width;
    menuDrawCell(command + 2, x, y, 1);
    x += 0x10;
    width = size->current_pair.x - tabs->current_pair.y - 0x30;
    menuDrawCellSized(command + 3, x, y, width, 0x16, 1);
    menuDrawCell(command + 4, x + width, y, 1);
}

extern int panelKindSizeTable[2][4][2];

/* size and tab widths of the four panel kinds */
/* Places the shared panel tracks at (x, y) with the sizes of kind (0..3) and draws it with commands 0x94... */
void menuDrawPanelHighlight(int x, int y, int kind)
{
    kind &= 3;
    fontTrackSetBoth(&panelPositionTrack, x, y);
    fontTrackSetBoth(&panelSizeTrack, panelKindSizeTable[0][kind][0], panelKindSizeTable[0][kind][1]);
    fontTrackSetBoth(&panelTabTrack, panelKindSizeTable[1][kind][0], panelKindSizeTable[1][kind][1]);
    menuDrawTrackPanel(0x94, &panelPositionTrack, &panelSizeTrack, &panelTabTrack);
}

#define MENU_PARTY_ICON_SHADOW 0x3F80000080665B6ELL
#define MENU_PARTY_ICON_FILL 0x3F80000080410C3ALL
#define MENU_PARTY_FADE_SHIFT 7 /* highlight.current_pair.y is a fraction of 128 */
#define MENU_PARTY_HIDDEN 9 /* layout value that draws nothing */

/* Record group frame at (x, y) of width by height: corner and edge cells 0x116 to 0x11F with a 75-wide title tab (cells 0x11C, 0x11D, 0x11F) on the top edge. */ void menuDrawTabFrame(int x, int y, int width, int height);

/* Party panel of member for item in one of the layouts 0-8 (MENU_PARTY_HIDDEN draws nothing), at
   (x, y) plus the item's position. */
void menuDrawPartyPanel(MenuItem *item, PartyMember *member, int x, int y, int layout)
{
    StatBlock *record;
    PartyMemberStatus *status;
    PartyMemberExpPoints *limits;

    if (item == 0 || member == 0 || layout == MENU_PARTY_HIDDEN) {
        return;
    }
    x += item->position.current_pair.x;
    y += item->position.current_pair.y;
    status = member->status;
    record = calcTotalParaMenu(member->id);
    limits = member->limits;
    fontObjSetOpacity(item->highlight.current_pair.x);
    switch (layout) {
    case 0:
    case 1:
        if (item->flags & MENU_ITEM_ALT_FRAME) {
            panelDrawNineSlice(menuPanelFrame, x, y, 0xE2, 0x2B);
            menuDrawCell(0x121, x + 0xE, y + 4, 0x11);
            menuDrawLitIconMarked(x + 8, y + 7, status->memberIndex);
            textDrawGlyphString(x + 0x28, y + 7, status->text, MENU_PARTY_TEXT_COLOR);
            menuDrawLevel(x + 0x2F, y + 0x14, record->expLevel);
            menuDrawHp(x + 0x70, y + 4, record->hp, record->maxHp);
            menuDrawEp(x + 0x79, y + 0x14, record->ep, record->maxEp);
            if (item->highlight.current_pair.y != 0) {
                fontObjSetOpacity((item->highlight.current_pair.x * item->highlight.current_pair.y)
                                  >> MENU_PARTY_FADE_SHIFT);
                menuDrawPanelHighlight(x - 0xA, y - 0xE, 1);
            }
        } else {
            panelDrawNineSlice(menuPanelFrame, x, y + 0x19, 0x9D, 0x27);
            menuDrawCell(0x120, x + 0x2F, y + 4, 0x11);
            menuDrawIndexedIconMarked(x + 6, y, status->memberIndex);
            textDrawGlyphString(x + 0x34, y + 7, status->text, MENU_PARTY_TEXT_COLOR);
            menuDrawLevel(x + 0x77, y + 3, record->expLevel);
            menuDrawHp(x + 0x30, y + 0x1A, record->hp, record->maxHp);
            menuDrawEp(x + 0x30, y + 0x2A, record->ep, record->maxEp);
            fontObjSetColor(MENU_PARTY_ICON_SHADOW);
            menuDrawStatIcon(5, x + 0x30, y + 0x3D, MENU_PARTY_ICON_FILL);
            fontObjResetColor();
            textDrawDecimalAlt(x + 0x60, y + 0x41, limits->next_level_exp, 8, MENU_PARTY_TEXT_COLOR);
            if (item->highlight.current_pair.y != 0) {
                fontObjSetOpacity((item->highlight.current_pair.x * item->highlight.current_pair.y)
                                  >> MENU_PARTY_FADE_SHIFT);
                menuDrawPanelHighlight(x - 0xA, y - 0xA, 0);
            }
        }
        break;
    case 4:
        menuDrawBadge(x, y);
        if (item->flags & MENU_ITEM_ALT_FRAME) {
            menuDrawCellSized(0x121, x + 0xE, y + 6, fontObjCellOffsetX(0x121), 8, 0x11);
            menuDrawCellSized(0x115, x + 0x80, y + 1, 0x5A, 0x10, 0x11);
        } else {
            menuDrawCellSized(0x120, x + 0xE, y + 6, fontObjCellOffsetX(0x120), 8, 0x11);
            menuDrawCellSized(0x114, x + 0x80, y + 1, 0x5A, 0x10, 0x11);
        }
        menuDrawLitIcon(x + 8, y + 2, status->memberIndex);
        textDrawGlyphString(x + 0x24, y - 1, status->text, MENU_PARTY_TEXT_COLOR);
        if (item->flags & MENU_ITEM_VALUES) {
            fontObjSetColor(0x3F800000807F5747LL);
            menuDrawStatIcon(2, x + 0x30, y + 0xD, 0x3F8000008052081BLL);
            menuDrawStatIcon(3, x + 0x84, y + 0xD, 0x3F8000008052081BLL);
            fontObjResetColor();
            textDrawDecimal(x + 0x50, y + 0x11, limits->classPoints, 5, MENU_PARTY_TEXT_COLOR);
            textDrawDecimal(x + 0xA4, y + 0x11, limits->skillPoints, 5, MENU_PARTY_TEXT_COLOR);
        } else {
            menuDrawHp(x + 0x28, y + 0xD, record->hp, record->maxHp);
            fontObjSetColor(0x3F800000807D763FLL);
            menuDrawStatIcon(1, x + 0x94, y + 0xD, 0x3F80000080572000LL);
            fontObjResetColor();
            textDrawGlyphString(x + 0xBF, y + 0x11, "/", MENU_PARTY_TEXT_COLOR);
            textDrawDecimal(x + 0xAC, y + 0x11, record->ep, 2, MENU_PARTY_TEXT_COLOR);
            textDrawDecimal(x + 0xC9, y + 0x11, record->maxEp, 2, MENU_PARTY_TEXT_COLOR);
        }
        break;
    case 7:
        if (item->flags & MENU_ITEM_TALL) {
            menuDrawTabFrame(x, y + 0xB, 0xAE, 0x47);
            if (item->flags & MENU_ITEM_ALT_FRAME) {
                menuDrawCell(0x121, x + 0x2F, y + 4, 0x11);
                menuDrawCell(0x115, x + 0x32, y + 0x1B, 0x11);
            } else {
                menuDrawCell(0x120, x + 0x2F, y + 4, 0x11);
                menuDrawCell(0x114, x + 0x32, y + 0x1B, 0x11);
            }
            menuDrawIndexedIcon(x + 6, y, status->memberIndex);
            textDrawGlyphString(x + 0x32, y + 7, status->text, MENU_PARTY_TEXT_COLOR);
            menuDrawLevel(x + 0x88, y + 3, record->expLevel);
            menuDrawHpWide(x + 0x30, y + 0x2B, record->hp, record->maxHp);
            menuDrawEpWide(x + 0x30, y + 0x3B, record->ep, record->maxEp);
            if (item->highlight.current_pair.y != 0) {
                fontObjSetOpacity((item->highlight.current_pair.x * item->highlight.current_pair.y)
                                  >> MENU_PARTY_FADE_SHIFT);
                menuDrawPanelHighlight(x - 9, y - 0xA, 2);
            }
        } else {
            menuDrawThreePieceBar(x, y, 0x5E);
            menuDrawLitIconMarked(x + 8, y - 9, status->memberIndex);
            textDrawStringAlt(x + 0x24, y + 5, status->text, MENU_PARTY_TEXT_COLOR);
            if (item->flags & MENU_ITEM_MARK_A) {
                menuDrawCell(0x13D, x + 0x1C, y - 0xC, 1);
            } else if (item->flags & MENU_ITEM_MARK_B) {
                menuDrawCell(0x13F, x + 0x1C, y - 0xC, 1);
            } else {
                menuDrawCell(0x13E, x + 0x1C, y - 0xC, 1);
            }
            if (item->highlight.current_pair.y != 0) {
                fontObjSetOpacity((item->highlight.current_pair.x * item->highlight.current_pair.y)
                                  >> MENU_PARTY_FADE_SHIFT);
                menuDrawPanelHighlight(x - 8, y - 0x12, 3);
            }
        }
        break;
    case 5:
    case 6:
        if (item->flags & MENU_ITEM_TALL) {
            panelDrawNineSlice(menuPanelFrame, x, y + 0x19, 0xAF, 0x27);
            if (item->flags & MENU_ITEM_ALT_FRAME) {
                menuDrawCell(0x121, x + 0x2F, y + 4, 0x11);
            } else {
                menuDrawCell(0x120, x + 0x2F, y + 4, 0x11);
            }
            menuDrawIndexedIconMarked(x + 6, y, status->memberIndex);
            textDrawGlyphString(x + 0x32, y + 7, status->text, MENU_PARTY_TEXT_COLOR);
            menuDrawLevel(x + 0x88, y + 3, record->expLevel);
            menuDrawHpWide(x + 0x30, y + 0x1A, record->hp, record->maxHp);
            menuDrawEpWide(x + 0x30, y + 0x2A, record->ep, record->maxEp);
            fontObjSetColor(MENU_PARTY_ICON_SHADOW);
            menuDrawStatIcon(5, x + 0x30, y + 0x3D, MENU_PARTY_ICON_FILL);
            fontObjResetColor();
            textDrawDecimalAlt(x + 0x70, y + 0x41, limits->next_level_exp, 8, MENU_PARTY_TEXT_COLOR);
            if (item->highlight.current_pair.y != 0) {
                fontObjSetOpacity((item->highlight.current_pair.x * item->highlight.current_pair.y)
                                  >> MENU_PARTY_FADE_SHIFT);
                menuDrawPanelHighlight(x - 9, y - 0xA, 2);
            }
        } else {
            menuDrawThreePieceBar(x, y, 0x5E);
            menuDrawLitIconMarked(x + 8, y - 9, status->memberIndex);
            textDrawStringAlt(x + 0x24, y + 5, status->text, MENU_PARTY_TEXT_COLOR);
            if (item->flags & MENU_ITEM_MARK_A) {
                menuDrawCell(0x13D, x + 0x1C, y - 0xC, 1);
            } else if (item->flags & MENU_ITEM_MARK_B) {
                menuDrawCell(0x13F, x + 0x1C, y - 0xC, 1);
            } else {
                menuDrawCell(0x13E, x + 0x1C, y - 0xC, 1);
            }
            if (item->highlight.current_pair.y != 0) {
                fontObjSetOpacity((item->highlight.current_pair.x * item->highlight.current_pair.y)
                                  >> MENU_PARTY_FADE_SHIFT);
                menuDrawPanelHighlight(x - 8, y - 0x12, 3);
            }
        }
        break;
    case 8:
        panelDrawNineSlice(menuPanelFrame, x, y + 0x29, 0xAE, 0x27);
        if (item->flags & MENU_ITEM_ALT_FRAME) {
            menuDrawCell(0x121, x + 0x2F, y + 4, 0x11);
            menuDrawCell(0x115, x + 0x38, y + 0x19, 0x11);
        } else {
            menuDrawCell(0x120, x + 0x2F, y + 4, 0x11);
            menuDrawCell(0x114, x + 0x38, y + 0x19, 0x11);
        }
        menuDrawIndexedIcon(x + 6, y, status->memberIndex);
        textDrawGlyphString(x + 0x32, y + 7, status->text, MENU_PARTY_TEXT_COLOR);
        menuDrawLevel(x + 0x88, y + 3, record->expLevel);
        menuDrawHpWide(x + 0x30, y + 0x2A, record->hp, record->maxHp);
        menuDrawEpWide(x + 0x30, y + 0x3A, record->ep, record->maxEp);
        break;
    case 2:
    case 3:
    default:
        menuDrawTabFrame(x, y + 0xB, 0x9D, 0x47);
        if (item->flags & MENU_ITEM_ALT_FRAME) {
            menuDrawCell(0x121, x + 0x2F, y + 4, 0x11);
            menuDrawCell(0x115, x + 0x32, y + 0x1B, 0x11);
        } else {
            menuDrawCell(0x120, x + 0x2F, y + 4, 0x11);
            menuDrawCell(0x114, x + 0x32, y + 0x1B, 0x11);
        }
        menuDrawIndexedIcon(x + 6, y, status->memberIndex);
        textDrawGlyphString(x + 0x34, y + 7, status->text, MENU_PARTY_TEXT_COLOR);
        menuDrawLevel(x + 0x77, y + 3, record->expLevel);
        menuDrawHp(x + 0x30, y + 0x2B, record->hp, record->maxHp);
        menuDrawEp(x + 0x30, y + 0x3B, record->ep, record->maxEp);
        if (item->highlight.current_pair.y != 0) {
            fontObjSetOpacity((item->highlight.current_pair.x * item->highlight.current_pair.y)
                              >> MENU_PARTY_FADE_SHIFT);
            menuDrawPanelHighlight(x - 9, y - 0xA, 2);
        }
        break;
    }
    fontObjResetOpacity();
}

typedef signed char s8;

typedef int s32;


/* Draws an empty party slot: background, the mark icon for item->flags, and (MENU_ITEM_LABEL)
 * the 0x158/0x159 marks and the "Canaan" label; fades the mark the same way menuDrawPartyPanel does. */
void menuDrawEmptyPartySlot(MenuItem *item, int x, int y) {
    int cornerX;
    int cornerY;

    cornerX = x + item->position.current_pair.x;
    cornerY = y + item->position.current_pair.y;
    fontObjSetOpacity(item->highlight.current_pair.x);
    menuDrawThreePieceBar(cornerX, cornerY, 0x5E);
    if (item->flags & MENU_ITEM_MARK_A) {
        menuDrawCell(0x13D, cornerX + 0x1C, cornerY - 0xC, 1);
        if (item->flags & MENU_ITEM_LABEL) {
            menuDrawCell(0x158, cornerX + 8, cornerY - 9, 0x10);
            menuDrawCell(0x159, cornerX + 8, cornerY - 9, 0x11);
            textDrawStringAlt(cornerX + 0x24, cornerY + 5, "Canaan", MENU_PARTY_TEXT_COLOR);
        }
    } else if (item->flags & MENU_ITEM_MARK_B) {
        menuDrawCell(0x13F, cornerX + 0x1C, cornerY - 0xC, 1);
    } else {
        menuDrawCell(0x13E, cornerX + 0x1C, cornerY - 0xC, 1);
    }
    if (item->highlight.current_pair.y != 0) {
        fontObjSetOpacity((item->highlight.current_pair.x * item->highlight.current_pair.y)
                          >> MENU_PARTY_FADE_SHIFT);
        menuDrawPanelHighlight(cornerX - 8, cornerY - 0x12, 3);
    }
    fontObjResetOpacity();
}

/* Resets the menu: full (the 13-item party grid, items 0-2 the slots 7-9, items 3-12 the roster) or the
 * short list of ptState.counts[1] items. */
void menuResetItems(MenuBlock *object, s32 full) {
    s32 index;

    for (index = 0; index < 13; index++) {
        menuResetItem(&object->items[index]);
        if (full != 0) {
            if (index < 3) {
                object->items[index].flags = MENU_ITEM_TALL;
                object->items[index].member = index + 7;
                if (index == 2) {
                    object->items[index].flags |= MENU_ITEM_ALT_FRAME;
                }
            } else {
                object->items[index].member = index - 3;
                object->items[index].flags |= MENU_ITEM_ROSTER;
                if (index >= 9) {
                    object->items[index].flags |= (MENU_ITEM_MARK_B | MENU_ITEM_ALT_FRAME);
                } else {
                    if (index < 6) {
                        object->items[index].flags |= MENU_ITEM_MARK_A;
                    }
                    if (index % 3 == 2) {
                        object->items[index].flags |= MENU_ITEM_ALT_FRAME;
                    }
                }
            }
        } else if (index < 7) {
            object->items[index].member = index;
            if (index >= 3) {
                object->items[index].flags |= MENU_ITEM_ALT_FRAME;
            }
        }
    }
    fontTrackSetBoth(&object->position, 0, 0);
    fontTrackSetBoth(&object->tracks[0], 0, 0);
    fontTrackSetBoth(&object->tracks[3], 0, 0);
    object->layout = 0;
    object->prev_layout = 0;
    object->cursor = 0;
    object->prev_cursor = 0;
    object->reset_word = 0;
    if (full != 0) {
        object->prev_layout = object->layout = 5;
        object->item_count = 13;
        object->flags = 0x10000;
        for (index = 3; index < 13; index++) {
            object->items[index].flags |= MENU_ITEM_DISABLED;
        }
    } else {
        object->item_count = ptState.counts[1];
        object->flags = 0;
    }
}

/* Moves the cursor frame (tracks 0-2) onto item index for the current layout: track 0 to the item's first
 * track target offset by the menu's own target, tracks 1 and 2 to the frame size for that layout. */
void menuPlaceItemCursor(MenuBlock *object, s32 index) {
    if (index < 0) {
        return;
    }
    if (index >= object->item_count) {
        return;
    }
    switch (object->layout) {
    case 0:
    case 1:
        if (object->items[index].flags & MENU_ITEM_ALT_FRAME) {
            fontTrackSetTarget(&object->tracks[0],
                               object->position.target_pair.x + object->items[index].position.target_pair.x - 0xA,
                               object->position.target_pair.y + object->items[index].position.target_pair.y - 0xE);
            fontTrackSetTarget(&object->tracks[1], 0xF6, 0x43);
        } else {
            fontTrackSetTarget(&object->tracks[0],
                               object->position.target_pair.x + object->items[index].position.target_pair.x - 0xA,
                               object->position.target_pair.y + object->items[index].position.target_pair.y - 0xA);
            fontTrackSetTarget(&object->tracks[1], 0xB1, 0x62);
        }
        fontTrackSetTarget(&object->tracks[2], 0x4E, 0x20);
        break;
    case 4:
        fontTrackSetTarget(&object->tracks[0],
                           object->position.target_pair.x + object->items[index].position.target_pair.x - 0x14,
                           object->position.target_pair.y + object->items[index].position.target_pair.y + 9);
        break;
    case 8:
        fontTrackSetTarget(&object->tracks[0],
                           object->position.target_pair.x + object->items[index].position.target_pair.x - 0x14,
                           object->position.target_pair.y + object->items[index].position.target_pair.y + 0x1E);
        break;
    case 5:
    case 6:
        if (object->items[index].flags & MENU_ITEM_TALL) {
            fontTrackSetTarget(&object->tracks[0],
                               object->position.target_pair.x + object->items[index].position.target_pair.x - 9,
                               object->position.target_pair.y + object->items[index].position.target_pair.y - 0xA);
            fontTrackSetTarget(&object->tracks[1], 0xC2, 0x62);
            fontTrackSetTarget(&object->tracks[2], 0x5F, 0x20);
        } else {
            fontTrackSetTarget(&object->tracks[0],
                               object->position.target_pair.x + object->items[index].position.target_pair.x - 8,
                               object->position.target_pair.y + object->items[index].position.target_pair.y - 0x12);
            fontTrackSetTarget(&object->tracks[1], 0x6F, 0x31);
            fontTrackSetTarget(&object->tracks[2], 0x2B, 0x12);
        }
        break;
    default:
        fontTrackSetTarget(&object->tracks[0],
                           object->position.target_pair.x + object->items[index].position.target_pair.x,
                           object->position.target_pair.y + object->items[index].position.target_pair.y);
        fontTrackSetTarget(&object->tracks[1], 0x12, 0xA);
        fontTrackSetTarget(&object->tracks[2], 0x12, 0xA);
        fontTrackSetTarget(&object->tracks[3], 0, 0);
        fontTrackAdvance(&object->tracks[3]);
        break;
    }
}

/* Where an item sits in a layout: menuLayoutSlotTable[layout][slot], 13 slots per layout. mode picks how
 * menuRefreshItems lights the item: 0 off, 1 on, 2 only under the cursor. */
typedef struct MenuLayoutSlot {
    unsigned char mode;
    unsigned char unmodeled_01[3];
    int x;
    int y;
} MenuLayoutSlot;

extern MenuLayoutSlot menuLayoutSlotTable[][13];

/* Lights each item for the current layout by its menuLayoutSlotTable mode: the highlight track's target is x 0x80 when
 * lit, else 0, and y 0x80 when selected (MENU_ITEM_SELECTED). A party item (MENU_ITEM_ROSTER) with a roster entry is
 * lit only under the cursor in layout 7; one without, in rows 3-8 of layouts 5 and 6, is lit when slot
 * 7 + column holds a member. With object flags 0x1 a disabled item's x target is quartered. */
void menuRefreshItems(MenuBlock *object) {
    s32 index;
    s32 lit;
    s32 layout;
    s32 mode;
    s32 status;

    lit = 0;
    for (index = 0; index < object->item_count; index++) {
        layout = object->layout;
        mode = menuLayoutSlotTable[layout][index].mode;
        switch (mode) {
        case 0:
        case 1:
            lit = mode;
            break;
        case 2:
            lit = index == object->cursor;
            break;
        }
        status = object->items[index].member;
        if (status != -1 && (object->items[index].flags & MENU_ITEM_ROSTER)) {
            if (ptState.roster[status] != 0) {
                if (layout == 7) {
                    lit = index == object->cursor;
                }
            } else if ((layout >= 5 && layout < 7) && (index >= 3 && index < 9)) {
                lit = ptState.slots[index % 3 + 7] != 0;
            }
        }
        if (lit) {
            fontTrackSetTarget(&object->items[index].highlight, 0x80,
                               (object->items[index].flags & MENU_ITEM_SELECTED) ? 0x80 : 0);
        } else {
            fontTrackSetTarget(&object->items[index].highlight, 0,
                               (object->items[index].flags & MENU_ITEM_SELECTED) ? 0x80 : 0);
        }
        if ((object->flags & 1) && (object->items[index].flags & MENU_ITEM_DISABLED)) {
            object->items[index].highlight.target_pair.x >>= 2;
        }
    }
}

/* Advances every item's tracks, or only redraws the position track of an item whose highlight track has
 * a nonzero x target; draws every item's highlight track, then sets the glow track's target from flags
 * 0x1, draws the frame tracks, moves the cursor frame onto the cursor item and draws the menu's own track. */
void menuDrawObject(MenuBlock *object) {
    s32 index;

    index = 0;
    menuRefreshItems(object);
    if (object->item_count > 0) {
        do {
            if (object->items[index].highlight.target_pair.x != 0) {
                fontTrackEase(&object->items[index].position);
            } else {
                fontTrackAdvance(&object->items[index].highlight);
                fontTrackAdvance(&object->items[index].position);
            }
            fontTrackEase(&object->items[index].highlight);
            index += 1;
        } while (index < object->item_count);
    }
    if (object->flags & 1) {
        fontTrackSetTarget(&object->tracks[3], 0x80, 0);
    } else {
        fontTrackSetTarget(&object->tracks[3], 0, 0);
    }
    fontTrackEase(&object->tracks[0]);
    fontTrackEase(&object->tracks[3]);
    fontTrackEase(&object->tracks[1]);
    fontTrackEase(&object->tracks[2]);
    menuPlaceItemCursor(object, object->cursor);
    fontTrackEase(&object->position);
}

/* Draws the items: party items from their roster entry (layout 7 draws them as layout 3), empty grid slots
 * of layouts 5 and 6 as empty panels, the rest from their slot. flags 0x2 hides the menu; flags 0x4 draws
 * the blinking band on every lit filled slot (restoring the cursor track afterwards); layouts 4 and 8 draw
 * the band at the cursor while flags 0x1 is set, the others draw the frame 0x88. */
void menuDrawItems(MenuBlock *object) {
    s32 index;
    PartyMember *member;
    struct FontTrack savedTrack;

    if (object->flags & 2) {
        return;
    }
    for (index = 0; index < object->item_count; index++) {
        if (object->items[index].member == -1) {
            continue;
        }
        if (object->items[index].flags & MENU_ITEM_ROSTER) {
            member = ptState.roster[object->items[index].member];
            if (member != 0) {
                if (object->layout == 7) {
                    menuDrawPartyPanel(&object->items[index], member, object->position.current_pair.x,
                                       object->position.current_pair.y, 3);
                } else {
                    menuDrawPartyPanel(&object->items[index], member, object->position.current_pair.x,
                                       object->position.current_pair.y, object->layout);
                }
            } else if (object->layout == 5 || object->layout == 6) {
                if (index >= 3 && index < 9) {
                    if (ptState.slots[index % 3 + 7] != 0) {
                        menuDrawEmptyPartySlot(&object->items[index], object->position.current_pair.x,
                                      object->position.current_pair.y);
                    }
                } else {
                    menuDrawEmptyPartySlot(&object->items[index], object->position.current_pair.x,
                                  object->position.current_pair.y);
                }
            }
        } else {
            menuDrawPartyPanel(&object->items[index],
                               ptState.slots[object->items[index].member],
                               object->position.current_pair.x, object->position.current_pair.y, object->layout);
        }
    }
    if (object->flags & 4) {
        savedTrack = object->tracks[0];
        if (xglTickGet() & 1) {
            fontObjSetOpacity(0x20);
        }
        for (index = 0; index < object->item_count; index++) {
            if (object->items[index].member != -1 && object->items[index].highlight.target_pair.x > 0
                && ptState.slots[object->items[index].member] != 0) {
                menuPlaceItemCursor(object, index);
                fontTrackAdvance(&object->tracks[0]);
                menuDrawAnimatedBand(object->tracks[0].current_pair.x, object->tracks[0].current_pair.y);
            }
        }
        fontObjResetOpacity();
        object->tracks[0] = savedTrack;
    } else if (object->layout == 4 || object->layout == 8) {
        if (object->flags & 1) {
            menuDrawAnimatedBand(object->tracks[0].current_pair.x, object->tracks[0].current_pair.y);
        }
    } else {
        fontObjSetOpacity(object->tracks[3].current_pair.x);
        menuDrawTrackPanel(0x88, &object->tracks[0], &object->tracks[1], &object->tracks[2]);
        fontObjResetOpacity();
    }
}

/* Switches the grid to layout (the grid variant 5-8 of layouts 0, 1, 3 and 4 when flags 0x10000 is set),
 * sends every item's position track toward its slot (layout 2 rotates the slots so the cursor item takes
 * slot 1 and the item before it slot 0), and re-enables the items for the new layout. */
void menuSetMode(MenuBlock *object, s32 layout)
{
    s32 index;
    s32 slot;

    if (object->flags & 0x10000) {
        switch (layout) {
        case 0:
            layout = 5;
            break;
        case 1:
            layout = 6;
            break;
        case 3:
            layout = 7;
            break;
        case 4:
            layout = 8;
            break;
        }
    }
    object->prev_layout = object->layout;
    object->layout = layout;
    for (index = 0; index < object->item_count; index++) {
        slot = index;
        if (object->item_count != 0 && layout == 2) {
            slot = index - object->cursor + 1;
            slot += object->item_count;
            slot %= object->item_count;
        }
        fontTrackSetTarget(&object->items[index].position, menuLayoutSlotTable[layout][slot].x,
                           menuLayoutSlotTable[layout][slot].y);
        if (object->items[index].highlight.target_pair.x == 0) {
            fontTrackAdvance(&object->items[index].position);
        }
    }
    menuRefreshItems(object);
    if ((object->flags & 0x10000) && object->layout != 6 && object->layout != 5) {
        menuEnableItems(object, 0x401);
    } else {
        menuEnableItems(object, 0xF0F);
    }
    menuPlaceItemCursor(object, object->cursor);
    fontTrackAdvance(&object->tracks[0]);
    fontTrackAdvance(&object->tracks[1]);
    fontTrackAdvance(&object->tracks[2]);
}

/* Hides the menu: sets layout mode (as menuSetMode does) and snaps every track to its target. */
void menuHide(MenuBlock *object, s32 mode) {
    s32 index;

    index = 0;
    menuSetMode(object, mode);
    if (object->item_count > 0) {
        do {
            fontTrackAdvance(&object->items[index].position);
            fontTrackAdvance(&object->items[index].highlight);
            index += 1;
        } while (index < object->item_count);
    }
    menuPlaceItemCursor(object, object->cursor);
    fontTrackAdvance(&object->tracks[0]);
    fontTrackAdvance(&object->tracks[1]);
    fontTrackAdvance(&object->tracks[2]);
}

extern unsigned char menuSkipDisabledDepth;

/* Wraps the cursor into 0..item_count-1 and, while it lands on a disabled item (MENU_ITEM_DISABLED), keeps
 * stepping in the direction it last moved. menuSkipDisabledDepth is incremented before each recursive step and
 * cleared on return. */
void menuSkipDisabled(MenuBlock *object) {
    s32 count;
    s32 cursor;

    count = object->item_count;
    if (count <= 0) {
        object->prev_cursor = 0;
        object->cursor = 0;
    } else {
        cursor = object->cursor;
        while (object->cursor < 0) {
            object->cursor += object->item_count;
        }
        object->cursor %= count;
        if (object->items[object->cursor].flags & MENU_ITEM_DISABLED) {
            if (cursor - object->prev_cursor < 0) {
                object->cursor = cursor - 1;
                menuSkipDisabledDepth++;
                menuSkipDisabled(object);
                menuSkipDisabledDepth = 0;
                return;
            }
            object->cursor = cursor + 1;
            menuSkipDisabledDepth++;
            menuSkipDisabled(object);
        }
    }
    menuSkipDisabledDepth = 0;
}


/* Moves the cursor by delta and plays the cursor sound when it actually moved. */
void menuMoveCursor(MenuBlock *object, s32 delta) {
    s32 old_cursor;

    if (object->flags & 4) {
        return;
    }
    old_cursor = object->cursor;
    object->prev_cursor = old_cursor;
    object->cursor = old_cursor + delta;
    menuSkipDisabled(object);
    if (object->prev_cursor != object->cursor) {
        seVoiceStartDefault(0x01000001);
    }
}

/* Sets the cursor to an absolute index. */
void menuSetCursor(MenuBlock *object, s32 value) {
    s32 old_cursor;

    old_cursor = object->cursor;
    object->cursor = value;
    object->prev_cursor = old_cursor;
    menuSkipDisabled(object);
}

/* Moves the cursor to the nearest enabled item in direction (0 up, 1 right, 2 down, 3 left), measured
 * between the items' position tracks; the cursor sound plays when the cursor moved. Vertical moves take the
 * item nearest along the axis within 45 degrees; horizontal moves the one nearest across the axis within
 * about 63 degrees, then the nearest along it. */
void menuNavigate(MenuBlock *object, s32 direction) {
    s32 cursorAdjX;
    s32 cursorAdjY;
    s32 best;
    s32 bestAcross;
    s32 bestAlong;
    s32 index;
    s32 itemAdjX;
    s32 itemAdjY;
    s32 along;
    s32 across;
    s32 old_cursor;

    if ((object->flags & 0x10000) && object->cursor >= 3) {
        cursorAdjX = 0;
        cursorAdjY = 0;
    } else {
        cursorAdjX = 0x28;
        cursorAdjY = 0x14;
    }
    best = -1;
    bestAcross = 9999;
    bestAlong = 9999;
    for (index = 0; index < object->item_count; index++) {
        if (object->cursor == index) {
            continue;
        }
        if (object->items[index].flags & MENU_ITEM_DISABLED) {
            continue;
        }
        if ((object->flags & 0x10000) && index >= 3) {
            itemAdjX = 0;
            itemAdjY = 0;
        } else {
            itemAdjX = 0x1E;
            itemAdjY = 0x1E;
        }
        switch (direction) {
        default:
            continue;
        case 0:
            along = object->items[index].position.current_pair.y + itemAdjY
                    - object->items[object->cursor].position.current_pair.y - cursorAdjY;
            if (along >= 0) {
                continue;
            }
            across = object->items[index].position.current_pair.x + itemAdjX
                     - object->items[object->cursor].position.current_pair.x - cursorAdjX;
            along = (along < 0) ? -along : along;
            across = (across < 0) ? -across : across;
            if (along < across) {
                continue;
            }
            if (along >= bestAlong) {
                continue;
            }
            bestAcross = across;
            bestAlong = along;
            best = index;
            break;
        case 2:
            along = object->items[index].position.current_pair.y + itemAdjY
                    - object->items[object->cursor].position.current_pair.y - cursorAdjY;
            if (along <= 0) {
                continue;
            }
            across = object->items[index].position.current_pair.x + itemAdjX
                     - object->items[object->cursor].position.current_pair.x - cursorAdjX;
            along = (along < 0) ? -along : along;
            across = (across < 0) ? -across : across;
            if (along < across) {
                continue;
            }
            if (along >= bestAlong) {
                continue;
            }
            bestAcross = across;
            bestAlong = along;
            best = index;
            break;
        case 3:
            along = object->items[index].position.current_pair.x + itemAdjX
                    - object->items[object->cursor].position.current_pair.x - cursorAdjX;
            if (along >= 0) {
                continue;
            }
            across = object->items[index].position.current_pair.y + itemAdjY
                     - object->items[object->cursor].position.current_pair.y - cursorAdjY;
            along = (along < 0) ? -along : along;
            across = (across < 0) ? -across : across;
            if (along * 2 < across) {
                continue;
            }
            if (across > bestAcross) {
                continue;
            }
            if (across == bestAcross && along >= bestAlong) {
                continue;
            }
            bestAcross = across;
            bestAlong = along;
            best = index;
            break;
        case 1:
            along = object->items[index].position.current_pair.x + itemAdjX
                    - object->items[object->cursor].position.current_pair.x - cursorAdjX;
            if (along <= 0) {
                continue;
            }
            across = object->items[index].position.current_pair.y + itemAdjY
                     - object->items[object->cursor].position.current_pair.y - cursorAdjY;
            along = (along < 0) ? -along : along;
            across = (across < 0) ? -across : across;
            if (along * 2 < across) {
                continue;
            }
            if (across > bestAcross) {
                continue;
            }
            if (across == bestAcross && along >= bestAlong) {
                continue;
            }
            bestAcross = across;
            bestAlong = along;
            best = index;
            break;
        }
    }
    if (best != -1) {
        old_cursor = object->cursor;
        menuSetCursor(object, best);
        if (old_cursor != object->cursor) {
            seVoiceStartDefault(0x01000001);
        }
    }
}

/* Highlights item selected (MENU_ITEM_SELECTED set, highlight track's y target 0x80) and clears every other item. */
void menuSetSelected(MenuBlock *object, s32 selected) {
    s32 index;

    for (index = 0; index < object->item_count; index++) {
        if (index == selected) {
            object->items[index].highlight.target_pair.y = 0x80;
            object->items[index].flags |= MENU_ITEM_SELECTED;
        } else {
            object->items[index].highlight.target_pair.y = 0;
            object->items[index].flags &= ~MENU_ITEM_SELECTED;
        }
    }
}

/* Sets or clears MENU_ITEM_VALUES on every item. */
void menuSetValuesMode(MenuBlock *object, s32 enable) {
    s32 index;

    for (index = 0; index < object->item_count; index++) {
        if (enable != 0) {
            object->items[index].flags |= MENU_ITEM_VALUES;
        } else {
            object->items[index].flags &= ~MENU_ITEM_VALUES;
        }
    }
}

/* Draws item index highlighted: its position track is moved to (0x11, 0x100) or, from item 6 on, (0x11, 0x121)
 * and its opacity set to 0x80 for the draw, then both are restored. An item whose member (the party
 * roster index) is negative is skipped; an empty entry draws the empty slot. */
void menuDrawItemHighlighted(MenuBlock *object, s32 index) {
    struct FontTrack savedTrack;
    s32 savedOpacity;
    PartyMember *member;

    if (object->items[index].member < 0) {
        return;
    }
    savedTrack = object->items[index].position;
    savedOpacity = object->items[index].highlight.current_pair.x;
    if (index >= 6) {
        fontTrackSetBoth(&object->items[index].position, 0x11, 0x121);
    } else {
        fontTrackSetBoth(&object->items[index].position, 0x11, 0x100);
    }
    object->items[index].highlight.current_pair.x = 0x80;
    member = ptState.roster[object->items[index].member];
    if (member != 0) {
        menuDrawPartyPanel(&object->items[index], member, object->position.current_pair.x,
                           object->position.current_pair.y, object->layout);
    } else {
        menuDrawEmptyPartySlot(&object->items[index], object->position.current_pair.x, object->position.current_pair.y);
    }
    object->items[index].position = savedTrack;
    object->items[index].highlight.current_pair.x = savedOpacity;
}

/* Disables every item (MENU_ITEM_DISABLED), then enables the groups selected by enableMask and moves the
 * cursor off disabled items. Grid (object flags 0x10000): 0x100 items 0-1, 0x200 item 2, 0x1 items 0-2,
 * 0x2/0x4/0x8 items 3-5/6-8/9-12; 0x800 disables items 3-8 whose column has no member in slot 7 + column.
 * List: 0x100 items 0-2, 0x200 the rest. 0x400 disables items whose member (the roster entry for items
 * with MENU_ITEM_ROSTER, else the slot) is empty. */
#define MENU_ENABLE_NEED_BACK_MEMBER 0x800 /* grid items 3-8 need a member in slot 7 + column */

void menuEnableItems(MenuBlock *object, s32 enableMask) {
    s32 index;

    for (index = 0; index < object->item_count; index++) {
        object->items[index].flags |= MENU_ITEM_DISABLED;
        if (object->flags & 0x10000) {
            if (index < 3) {
                if (index < 2) {
                    if (enableMask & 0x100) {
                        object->items[index].flags &= ~MENU_ITEM_DISABLED;
                    }
                } else if (enableMask & 0x200) {
                    object->items[index].flags &= ~MENU_ITEM_DISABLED;
                }
                if (enableMask & 1) {
                    object->items[index].flags &= ~MENU_ITEM_DISABLED;
                }
            } else if (index < 6) {
                if (enableMask & 2) {
                    object->items[index].flags &= ~MENU_ITEM_DISABLED;
                }
            } else if (index < 9) {
                if (enableMask & 4) {
                    object->items[index].flags &= ~MENU_ITEM_DISABLED;
                }
            } else if (enableMask & 8) {
                object->items[index].flags &= ~MENU_ITEM_DISABLED;
            }
            if ((enableMask & MENU_ENABLE_NEED_BACK_MEMBER) && (index >= 3 && index < 9)
                && ptState.slots[index % 3 + 7] == 0) {
                object->items[index].flags |= MENU_ITEM_DISABLED;
            }
        } else if (index < 3) {
            if (enableMask & 0x100) {
                object->items[index].flags &= ~MENU_ITEM_DISABLED;
            }
        } else if (enableMask & 0x200) {
            object->items[index].flags &= ~MENU_ITEM_DISABLED;
        }
        if (enableMask & 0x400) {
            if (object->items[index].flags & MENU_ITEM_ROSTER) {
                if (ptState.roster[object->items[index].member] == 0) {
                    object->items[index].flags |= MENU_ITEM_DISABLED;
                }
            } else if (ptState.slots[object->items[index].member] == 0) {
                object->items[index].flags |= MENU_ITEM_DISABLED;
            }
        }
    }
    menuSkipDisabled(object);
}

/* Empties a message range: no text, nothing counted, nothing revealed. */
void menuResetState(MsgRange *range) {
    if (range != 0) {
        range->text = 0;
        range->glyph_count = 0;
        range->reveal_pos = 0;
        range->drawn = 0;
    }
}


/* Stores text in the range and counts its tokens, the units msgRangeDrawRevealed reveals. Two-byte
 * characters count once; code 7 counts none; a '$' to ';' span counts once (its characters count none, the
 * closing ';' is counted); a "\\n" pair is rewritten in place to a newline (counted) plus code 7; an '&'
 * followed by two digits counts once. */
void msgRangeSetText(MsgRange *range, unsigned char *text)
{
    unsigned char *p;
    int hidden;

    if (range != 0) {
        msgRangeReset(range);
        if (text == 0) {
            range->text = 0;
            range->glyph_count = 0;
            return;
        }
        range->text = text;
        range->glyph_count = 0;
        p = text;
        hidden = 0;
        for (; *p != 0; p++) {
            if ((signed char)*p < 0) {
                p++;
            } else if (*p == 7) {
                continue;
            } else if (*p == '\\') {
                if (p[1] == 'n') {
                    p[1] = 7;
                    p[0] = '\n';
                    p++;
                }
            } else if (hidden && *p == ';') {
                hidden = 0;
            } else if (*p == '$') {
                hidden = 1;
            } else if (*p == '&') {
                if ((unsigned char)(p[1] - '0') < 10 && p[2] >= '0' && p[2] <= '9') {
                    p += 2;
                }
            }
            if (!hidden) {
                range->glyph_count++;
            }
        }
    }
}

extern unsigned char msgStyleReset[];

/* Clears the reveal so the next draw starts again from the lead-in. */
void msgRangeReset(MsgRange *range)
{
    if (range != 0) {
        range->drawn = 0;
        range->reveal_pos = -8;
    }
}

/* Draws the inline icon of message code `code` at (x, y) with the given alpha and returns the width it
 * advances: codes 1-4 are kind icons on a shadow plate (0x14 wide), codes 10-12 the icons 0x131-0x133
 * (0x12 wide); any other code draws nothing and returns 0. */
int msgRangeDrawIcon(int x, int y, int code, int alpha)
{
    int width = 0;
    unsigned int index = code - 1;

    if (index < 4) {
        width = 0x14;
        fontObjSetColor(((u64)alpha << 24) | 0x3F80000000000000LL);
        menuDrawCell(0x44, x + 1, y - 1, 4);
    }
    fontObjSetColor(((u64)alpha << 24) | 0x3F80000000808080LL);
    switch (index) {
    case 0:
        menuDrawCell(0x49, x + 1, y - 1, 5);
        break;
    case 1:
        menuDrawCell(0x48, x + 1, y - 1, 5);
        break;
    case 2:
        menuDrawCell(0x4A, x + 1, y - 1, 5);
        break;
    case 3:
        menuDrawCell(0x47, x + 1, y - 1, 5);
        break;
    case 9:
        menuDrawCell(0x131, x + 1, y + 1, 4);
        width = 0x12;
        break;
    case 10:
        menuDrawCell(0x132, x + 1, y + 1, 4);
        width = 0x12;
        break;
    case 11:
        menuDrawCell(0x133, x + 1, y + 1, 4);
        width = 0x12;
        break;
    }
    fontObjResetColor();
    return width;
}

/* The width probe: font tag 0x19 0x03 followed by the glyph copied into bytes 2..4. */
/* The 0x0F 'D' alpha command; byte 3 is the alpha of the fading token. */

const unsigned char msgRangeStyleTag[16] = "$prop1;$xx3,0;";

/* Draws the first `drawn` tokens of the message at (x, y), fading in the ones past reveal_pos (alpha
 * 0x80 less 0x10 per token through the 0x0F 'D' font command, each drawn one more pixel to the right
 * of its place), then advances drawn and reveal_pos by up to three tokens. A newline returns to x one
 * 0x14 line down; code 7 is skipped; a "$...;" span is drawn as one escape string; "&NN" draws inline
 * icon NN; a glyph advances by 13/16 of its font width plus 1. */
void msgRangeDrawRevealed(MsgRange *range, int x, int y)
{
    unsigned char glyph[3];
    unsigned char *escape;
    unsigned char *p;
    int penX;
    int advance;
    int icon;
    int index;
    int fade;

    if (range == 0 || range->text == 0) {
        return;
    }
    textDrawSegment(0, 0, msgStyleInit, 0);
    penX = x;
    textDrawSegment(0, 0, msgRangeStyleTag, 0);
    p = range->text;
    advance = 0;
    escape = 0;
    icon = 0;
    for (index = 0; index < range->drawn; index++) {
        if ((signed char)*p < 0) {
            glyph[0] = *p++;
            advance = 0x10;
            glyph[1] = *p++;
            glyph[2] = 0;
        } else {
            switch (*p) {
            case '\n':
                glyph[0] = 0;
                advance = -1;
                p++;
                break;
            case 7:
                glyph[0] = 0;
                advance = 0;
                p++;
                index--;
                continue;
            case '$':
                advance = 0;
                escape = p;
                glyph[0] = 0;
                for (; *p != 0; p++) {
                    if ((signed char)*p < 0) {
                        p++;
                    } else if (*p == ';') {
                        p++;
                        glyph[0] = *p;
                        *p = 0;
                        break;
                    }
                }
                break;
            case '&':
                if ((unsigned char)(p[1] - '0') < 10 && p[2] >= '0' && p[2] <= '9') {
                    icon = (unsigned char)((p[1] - '0') * 10 + p[2] - '0');
                    p += 3;
                    break;
                }
                glyph[0] = *p++;
                glyph[1] = 0;
                advance = 0xF;
                break;
            case ' ':
                glyph[0] = *p++;
                glyph[1] = 0;
                advance = 8;
                break;
            default:
                glyph[0] = *p++;
                glyph[1] = 0;
                advance = 0xF;
                break;
            }
        }
        if (range->reveal_pos < index) {
            fade = index - range->reveal_pos;
            msgRevealFadeStyle[3] = 0x80 - fade * 16;
            xglFontPrint(0, 0, 0xFFFFF0, msgRevealFadeStyle);
            if (escape != 0) {
                textDrawSegment(penX + fade, y, escape, 0);
            } else if (icon != 0) {
                advance = msgRangeDrawIcon(penX + fade, y, icon, msgRevealFadeStyle[3]);
            } else {
                textDrawSegment(penX + fade, y, glyph, 0);
                msgGlyphMeasureBuffer[2] = glyph[0];
                msgGlyphMeasureBuffer[3] = glyph[1];
                msgGlyphMeasureBuffer[4] = glyph[2];
                if (advance != -1) {
                    advance = (xglFontGetStringWidth(msgGlyphMeasureBuffer) * 13 >> 4) + 1;
                }
            }
        } else if (escape != 0) {
            textDrawSegment(penX, y, escape, 0);
        } else if (icon != 0) {
            advance = msgRangeDrawIcon(penX, y, icon, 0x80);
        } else {
            textDrawSegment(penX, y, glyph, 0);
            msgGlyphMeasureBuffer[2] = glyph[0];
            msgGlyphMeasureBuffer[3] = glyph[1];
            msgGlyphMeasureBuffer[4] = glyph[2];
            if (advance != -1) {
                advance = (xglFontGetStringWidth(msgGlyphMeasureBuffer) * 13 >> 4) + 1;
            }
        }
        if (escape != 0) {
            escape = 0;
            *p = glyph[0];
        }
        if (icon != 0) {
            icon = 0;
        }
        if (advance == -1) {
            penX = x;
            y += 0x14;
        } else {
            penX += advance;
        }
    }
    for (index = 0; index < 3; index++) {
        if (range->drawn < range->glyph_count) {
            range->drawn++;
        }
        if (range->reveal_pos < range->glyph_count) {
            range->reveal_pos++;
        }
    }
    textDrawSegment(0, 0, msgStyleReset, 0);
}

/* Returns 1 once every glyph is revealed, 0 while some are still hidden. */
int msgRangeIsRevealed(const MsgRange *range)
{
    if (range->reveal_pos < range->glyph_count) {
        return 0;
    }
    return 1;
}

/* Draws the whole message at (x, y) between the style reset strings. */
void msgRangeDraw(MsgRange *range, int x, int y)
{
    if (range->text != 0) {
        textDrawSegment(0, 0, msgStyleInit, 0);
        textDrawSegment(0, 0, msgRangeStyleTag, 0);
        textDrawSegment(x, y, range->text, 0);
        textDrawSegment(0, 0, msgStyleReset, 0);
    }
}

/* Appends text to msgBuffer, keeping two-byte (high-bit) characters whole, turning a "\\n" pair into a
 * newline and stopping once the buffer holds 0xFF bytes. */
void msgBufferAppend(unsigned char *text)
{
    unsigned char *dst;
    int count;
    unsigned char c;

    if (text != 0) {
        dst = msgBuffer;
        count = 0;
        while (*dst != 0) {
            if ((signed char)*dst < 0) {
                dst++;
                count++;
            }
            dst++;
            count++;
        }
        c = *text;
        while (c != 0 && count < 0xFF) {
            *dst = c;
            if ((signed char)*text < 0) {
                text++;
                dst++;
                count++;
                *dst = *text;
            } else if (*text == '\\' && text[1] == 'n') {
                text++;
                *dst = '\n';
            }
            text++;
            count++;
            c = *text;
            dst++;
        }
        *dst = 0;
    }
}

/* Starts the yes/no prompt: the horizontal track jumps straight to initialOption's slot (the
   fixed row y 0xF8), the opacity track fades in from 0 to 0x7F, and the two option labels
   (choiceSetFirstOptionText/choiceSetSecondOptionText use the same words) come from the
   menu string table. */
void choiceInit(unsigned char *text, int initialOption)
{
    fontTrackSetBoth(&choiceState.highlight, initialOption * 0x78 + 0xA0, 0xF8);
    fontTrackSetBoth(&choiceState.fade, 0, 0x7F);
    fontTrackSetTarget(&choiceState.fade, 0x7F, 0);
    choiceState.buffer = msgBuffer;
    choiceState.state = initialOption;
    msgBuffer[0] = 0;
    msgBufferAppend(text);

    choiceState.firstOptionText = menuMainTxdEntry(0x45);
    choiceState.secondOptionText = menuMainTxdEntry(0x46);
}


extern void *choiceFirstOptionText[];

/* Sets the text of the first option of the choice prompt (choiceState +0x40); choiceDraw draws it at x 0xB4. */
void choiceSetFirstOptionText(void *text)
{
    choiceFirstOptionText[0] = text;
}

extern void *choiceSecondOptionText[];

/* Sets the text of the second option of the choice prompt (choiceState +0x44); choiceDraw draws it at x 0x12C. */
void choiceSetSecondOptionText(void *text)
{
    choiceSecondOptionText[0] = text;
}

/* Sets the highlight track's y to an absolute row (no animation): target and current y are the
   row, the scaled y is row * 16. */
void choiceSetRow(int row)
{
    choiceState.highlight.target_pair.y = row;
    choiceState.highlight.current_pair.y = row;
    choiceState.highlight.scaled_pair.y = row * 16;
}

int choiceUpdate(void)
{
    if (choiceState.state < 2) {
        if (PadData[0].directionRepeated & 0xA000) {
            seVoiceStartDefault(0x01000001);
            choiceState.state ^= 1;
            fontTrackSetTarget(&choiceState.highlight, (signed char) choiceState.state * 0x78 + 0xA0,
                               choiceState.highlight.current_pair.y);
        }
        if (PadData[0].directionPressed & 0x40) {
            choiceState.state = 2;
            seVoiceStartDefault(0x01000008);
            return 2;
        }
        if (PadData[0].directionPressed & 0x20) {
            choiceState.state |= 0x10;
            seVoiceStartDefault(0x01000002);
            return 1;
        }
    }
    return 0;
}


/* Draws the yes/no prompt: panel frame, prompt text, the two option labels (glyph kind 5 for
   the currently selected option, 4 for the other) and, while state read as a signed byte is
   not negative, the moving highlight band. fontTrackEase runs twice on each track, as in the asm. */
void choiceDraw(void)
{
    /* The asm addresses the opacity track first and reaches the record through it. */
    struct FontTrack *fade = &choiceState.fade;
    ChoiceState *state = &choiceState;

    fontTrackEase(fade);
    fontTrackEase(fade);
    fontObjSetOpacity(state->fade.current_pair.x);
    menuDrawWindowFrame(0x78, 0xA0, 0x110, 0x78);
    menuDrawShortOutlineFrame(0x7C, 0xA4, 0x108, 0x70);
    textDrawSegment(0, 0, msgStyleInit, 4);
    if (state->buffer != 0) {
        textDrawSegment(0x8C, 0xAC, state->buffer, 4);
    }
    textDrawSegment(0xB4, state->highlight.current_pair.y - 2, state->firstOptionText,
                    (signed char) state->state == 0 ? 5 : 4);
    textDrawSegment(0x12C, state->highlight.current_pair.y - 2, state->secondOptionText,
                    (signed char) state->state == 1 ? 5 : 4);
    xglFontFlush();
    if ((signed char) state->state >= 0) {
        fontTrackEase(&state->highlight);
        fontTrackEase(&state->highlight);
        fontObjResetAndBuild();
        packetRequest.x = state->highlight.current_pair.x;
        packetRequest.cell_index = 0x134;
        packetRequest.y = state->highlight.current_pair.y;
        packetRequest.offset_x = (xglTickGet() & 7) * 0x10;
        packetRequest.flags = 0x21;
        packetRequest.offset_y = 0;
        fontObjDraw(&packetRequest);
    }
    fontObjResetOpacity();
}

/* The dispatch value of the choice prompt's state byte: a negative byte is returned as-is,
   otherwise only its low two bits. */
int choiceGetResult(void)
{
    if ((signed char) choiceState.state < 0) {
        return (signed char) choiceState.state;
    }
    return choiceState.state & 3;
}

/* Sets the countdown's displayed text and starting count. The fade track starts at (0, 0x7F)
   and heads to (0x7F, 0): its x, the opacity countdownDraw applies, fades in from 0 to 0x7F. */
void countdownInit(unsigned char *text, int count)
{
    countdownState.count = count;
    countdownState.text = msgBuffer;
    msgBuffer[0] = 0;
    msgBufferAppend(text);
    fontTrackSetBoth(&countdownState.fade, 0, 0x7F);
    fontTrackSetTarget(&countdownState.fade, 0x7F, 0);
}

int countdownTick(void)
{
    if (countdownState.count > 0) {
        countdownState.count = countdownState.count - 1;
    }
    if (PadData[0].pressed & 0x60) {
        if (countdownState.count >= -1) {
            seVoiceStartDefault(0x01000002);
            countdownState.count = 0;
        }
    }
    return countdownState.count;
}

const unsigned char countdownStyleTag[16] = "$xx0,28;";

void countdownDraw(void)
{
    /* As in choiceDraw, the asm addresses the fade track first and reaches the record through it. */
    struct FontTrack *fade = &countdownState.fade;
    CountdownState *countdown = &countdownState;

    fontTrackEase(fade);
    fontTrackEase(fade);
    fontObjSetOpacity(countdown->fade.current_pair.x);
    menuDrawWindowFrame(0x50, 0xB4, 0x160, 0x50);
    menuDrawShortOutlineFrame(0x54, 0xB8, 0x158, 0x48);
    textDrawSegment(0, 0, msgStyleInit, 0);
    textDrawSegment(0, 0, countdownStyleTag, 0);
    if (countdown->text != 0) {
        textDrawSegment(0x60, 0xC4, countdown->text, 4);
    }
    fontObjResetOpacity();
    xglFontFlush();
    fontObjResetAndBuild();
}

/* Sets the extra advance added to every glyph of text. */
void textSetCharSpacing(signed char spacing)
{
    textCharSpacing = spacing;
}

/* Restores the default glyph spacing of 4. */
void textResetCharSpacing(void)
{
    textCharSpacing = 4;
}

/* Party save storage that save_party_init.c, menu_selection.c and pt_member.c read; this TU's code does not touch
 * it. */
PartyTakenBits ptMemberTakenTable[10];
PartyProgress ptProgressRecord;

/* .data of this file from 0x3EB308 (yg2PathFrameImage and yg2PathFrameBorderImage are read by yg2path.c). */
int yg2PathFrameImage[20] = {
    0x1DA, 0x1DB, 0x1DC, 0x1DD, -1,
    0x1DE, 0x1DF, 0x1E0, 0x1E1, 0x0,
    0x1D2, 0x1D3, 0x1D4, 0x1D5, -1,
    0x1D6, 0x1D7, 0x1D8, 0x1D9, 0x0
};

int yg2PathFrameBorderImage[10] = {
    0x1CA, 0x1CB, 0x1CC, 0x1CD, -1,
    0x1CE, 0x1CF, 0x1D0, 0x1D1, 0x0
};

int menuFlipCornerPanelFrame[10] = {
    0x105, 0x106, 0x107, 0xFF, 0x100,
    0x101, 0x109, 0x108, 0x109, 0x0
};

unsigned short menuIconKinds[16] = {
    327, 196, 194, 335, 329, 198, 333, 339,
    341, 337, 325, 323, 331, 0, 0, 0
};

const char *menuLevelClassTexts[12] = {
    "1",
    "2",
    "3",
    "4",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H"
};

signed char menuConfigLabelTable[8] = { 1, 4, 2, 1, 6, 0, 0, 5 };

MenuLayoutSlot menuLayoutSlotTable[10][13] = {
    {  /* layout 0 */
        { 1, { 0 }, 21, 116 }, { 1, { 0 }, 45, 203 }, { 1, { 0 }, 69, 290 },
        { 1, { 0 }, 238, 125 }, { 1, { 0 }, 252, 174 }, { 1, { 0 }, 266, 223 },
        { 1, { 0 }, 280, 272 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 },
    },
    {  /* layout 1 */
        { 1, { 0 }, 21, 116 }, { 1, { 0 }, 45, 203 }, { 1, { 0 }, 69, 290 },
        { 1, { 0 }, 238, 125 }, { 1, { 0 }, 252, 182 }, { 1, { 0 }, 266, 239 },
        { 1, { 0 }, 280, 296 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 },
    },
    {  /* layout 2 */
        { 0, { 0 }, 27, 105 }, { 0, { 0 }, 70, 197 }, { 0, { 0 }, 27, 289 },
        { 0, { 0 }, -152, 322 }, { 0, { 0 }, -327, 289 }, { 0, { 0 }, -327, 105 },
        { 0, { 0 }, -152, 72 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 },
    },
    {  /* layout 3 */
        { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 },
        { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 },
        { 2, { 0 }, 17, 95 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 },
    },
    {  /* layout 4 */
        { 1, { 0 }, 273, 95 }, { 1, { 0 }, 273, 137 }, { 1, { 0 }, 273, 179 },
        { 1, { 0 }, 273, 227 }, { 1, { 0 }, 273, 267 }, { 1, { 0 }, 273, 307 },
        { 1, { 0 }, 273, 347 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 },
    },
    {  /* layout 5 */
        { 1, { 0 }, 21, 116 }, { 1, { 0 }, 69, 251 }, { 1, { 0 }, 275, 116 },
        { 1, { 0 }, 10, 212 }, { 1, { 0 }, 58, 347 }, { 1, { 0 }, 264, 212 },
        { 1, { 0 }, 112, 212 }, { 1, { 0 }, 160, 347 }, { 1, { 0 }, 366, 212 },
        { 0, { 0 }, 291, 299 }, { 0, { 0 }, 393, 299 }, { 0, { 0 }, 291, 337 },
        { 0, { 0 }, 393, 337 },
    },
    {  /* layout 6 */
        { 1, { 0 }, 21, 116 }, { 1, { 0 }, 69, 251 }, { 1, { 0 }, 275, 116 },
        { 1, { 0 }, 10, 212 }, { 1, { 0 }, 58, 347 }, { 1, { 0 }, 264, 212 },
        { 1, { 0 }, 112, 212 }, { 1, { 0 }, 160, 347 }, { 1, { 0 }, 366, 212 },
        { 1, { 0 }, 291, 299 }, { 1, { 0 }, 393, 299 }, { 1, { 0 }, 291, 337 },
        { 1, { 0 }, 393, 337 },
    },
    {  /* layout 7 */
        { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 },
        { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 },
        { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 },
        { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 }, { 2, { 0 }, 17, 95 },
        { 2, { 0 }, 17, 95 },
    },
    {  /* layout 8 */
        { 1, { 0 }, 301, 103 }, { 1, { 0 }, 301, 195 }, { 1, { 0 }, 301, 295 },
        { 0, { 0 }, 1024, 448 }, { 0, { 0 }, 1024, 448 }, { 0, { 0 }, 1024, 448 },
        { 0, { 0 }, 1024, 448 }, { 0, { 0 }, 1024, 448 }, { 0, { 0 }, 1024, 448 },
        { 0, { 0 }, 1024, 448 }, { 0, { 0 }, 1024, 448 }, { 0, { 0 }, 1024, 448 },
        { 0, { 0 }, 1024, 448 },
    },
    {  /* layout 9 */
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 }, { 0, { 0 }, 0, 0 },
        { 0, { 0 }, 0, 0 },
    },
};

/* The four panel kinds' size pair and tab pair (rows [0] and [1] of menuDrawPanelHighlight). */
int panelKindSizeTable[2][4][2] = {
    { { 177, 98 }, { 246, 67 }, { 194, 98 }, { 111, 49 } },
    { { 78, 32 }, { 78, 32 }, { 95, 32 }, { 43, 18 } }
};

/* .sdata of this file: style reset record used by msgRangeDraw and msgRangeDrawRevealed. */
unsigned char msgStyleReset[8] = { 0x0f, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00 };
