#include "common.h"
#include "main/yg2_text.h"

char *yg2TextNextLine(char *text, int lines);

/* Walks the text line by line from `start` and returns the first line the pattern matches (yg2TextCompareLine is 0
 * on a match). */
char *yg2TextFindLine(char *start, const char *pattern)
{
    char *position;

    position = start;
    while (yg2TextCompareLine(pattern, position) != 0) {
        position = yg2TextNextLine(position, 1);
    }
    return position;
}

/* Moves past the next newline, then past spaces and tabs; returns the new cursor. */
char *yg2TextNextLine(char *text, int lines)
{
    while (*text != '\n') {
        text++;
    }
    text++;
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return text;
}

/* Compares the text with the pattern up to the pattern's newline; a '%' in the pattern ends the
 * comparison as a match. 0 on a match, nonzero otherwise. */
int yg2TextCompareLine(const char *pattern, const char *text)
{
    int c;

    for (; *pattern != '\n'; pattern++) {
        c = *pattern;
        if (c != *text++) {
            return *pattern != '%';
        }
    }
    return *pattern != *text;
}
