#ifndef INCLUDE_MAIN_XGL_FONT_H
#define INCLUDE_MAIN_XGL_FONT_H

#include "shared.h"

#include "main/font_draw_cursor.h"
#include "main/xgl_cd.h"

/* Functions of src/main/xgl_font.c that other files call. */

void xglFontFlush(void);
void xglFontInitial(void);
unsigned char xglFontLoad(int font_index, CdCompletionCallback *notify);
int xglFontFitWords(const char *text, int *width);
void xglFontDebugHex(int x, int y, unsigned int value, int digits);
void xglFontDebugPrefixSet(int debug_prefix);
void xglFontReloadTexture(FontDrawCursor *cursor, unsigned int request);

int xglFontGetStringWidth(const char *text);
void xglFontPrint(int x, int y, unsigned int ot, const char *text);
void xglFontPrintDirectOT(int ot, const char *text);
void xglFontPrintf(int x, int y, unsigned int ot, void *arg);
void xglFontPrintDirect(const char *text);
int xglFontGetSPcodeSize(int code, const unsigned char *text);
unsigned short xglFontGetFlags(void);
void xglFontSetFlags(int flags);
void xglFontDebugPrintf(int x, int y, const char *format, ...);
/* Copies one text unit (an inline command, a control code with its arguments, or a
 * one- or two-byte character) to word, or to the word buffer when word is 0; returns
 * the text after it and stores its length in *length when given. */
const char *xglFontCopyTextUnit(char *word, const char *text, int *length);

#endif /* INCLUDE_MAIN_XGL_FONT_H */
