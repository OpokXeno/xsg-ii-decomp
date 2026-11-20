#ifndef INCLUDE_MAIN_TEXT_TAG_H
#define INCLUDE_MAIN_TEXT_TAG_H

/* Functions of src/main/text_tag.c that other files call. */

struct MsgSlot;

#define TEXT_TAG_COUNT 20      /* entries of the text tag table */

void msgWindowDrawListIcons(struct MsgSlot *window);
void msgWindowLoadBotanXtx(void);
void msgWindowUnloadBotanXtx(void);
void textTagTableClear(void);
int msgWindowStepDirection(int step);
void textTagRemoveKey(const char *key);
/* Name of tag glyph index (1..5) at the start of text: nonzero when it matches. */
int textStartsWithGlyphName(const char *text, int index);
/* text advanced past the name of tag glyph index; other indices leave it unchanged. */
const char *textSkipGlyphName(const char *text, int index);
void textTagAdd(const char *key, const char *position, int x, int y, int kind);
/* Draws the glyphs of the text tags active at text (the revealed end of a text slot), yOffset pixels up. */
void textTagDrawActive(const char *text, int yOffset, struct MsgSlot *slot);

#endif /* INCLUDE_MAIN_TEXT_TAG_H */
