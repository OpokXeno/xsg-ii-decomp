#ifndef INCLUDE_MAIN_MSG_TEXT_DRAW_H
#define INCLUDE_MAIN_MSG_TEXT_DRAW_H

/* Functions of src/main/msg_text_draw.c that other files call. */

#include "main/msg_slot_core.h"

#include "main/xgl_font.h"

/*
 * SDK string functions the sell/talk text builders call; no TU of the recovered code defines them.
 * (The libgcc soft-float helpers are emitted by the compiler from plain arithmetic and are not declared.)
 */
char *strcat(char *dst, const char *src);              /* appends src to dst (strcat); callers use the result only in formatAcquiredText */
char *strncat(char *dst, const char *src, int count);   /* appends at most count characters of src (strncat) */

int talkBoxContentX(MsgSlot *box);
int talkBoxContentY(MsgSlot *box);
void drawAcquiredText(MsgSlot *block);
void drawShadowedText(int x, int y, int z, unsigned int *text);
int sellCountFilledSlots(int *words);
void flushFontBatch(void);
int textSlotFirstRowY(const MsgSlot *slot);
void fontPrintIntRow(int x, int y, int color, char *fmt, int *values);
/* xglFontPrintExtFunc callback: sets the SCISSOR_1 rows of the talk box; the box argument 1 means the full screen. */
void talkBoxScissorCallback(FontDrawCursor *cursor, void *argument);

int textLineCount(MsgSlot *entry);
const char *textSkipLines(const char *text, int lines);
int textCountLinesBefore(const char *text, const char *end);
void textBoxMeasure(int *width, int *height, const char *text, int *font, const char *end);
const char *textSkipBracketTag(const char *text);
const char *textTagBody(const char *text);
int textMaxLineWidth(MsgSlot *entry);
int textCountLinesOf(const char *text);
int sellCountFilled(int *words, int count);
int decimalDigitAt(long value, int pos);
void timecodeDrawContents(MsgSlot *box);
void drawSellPanel(MsgSlot *widget);
void msgWindowDrawLineList(MsgSlot *box);
int acquiredTextWidth(char *name_text, int count);

/* Frame remainder (30 per second) to hundredths of a second, as double: the original multiplies by 3.333. */
#define HUNDREDTHS_PER_FRAME 3.333

#endif /* INCLUDE_MAIN_MSG_TEXT_DRAW_H */
