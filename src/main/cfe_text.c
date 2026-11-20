#include "common.h"
#include "main/cfe_text.h"

char *cfeTextNextLine(char *text, int lines);

/* Walks the text line by line from `start` and returns the first line the pattern matches (cfeTextCompareLine is 0
 * on a match). */
char *cfeTextFindLine(char *start, const char *pattern)
{
    char *position;

    position = start;
    while (cfeTextCompareLine(pattern, position) != 0) {
        position = cfeTextNextLine(position, 1);
    }
    return position;
}

/* Moves past the next newline, then past spaces and tabs; returns the new cursor. */
char *cfeTextNextLine(char *text, int lines)
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
int cfeTextCompareLine(const char *pattern, const char *text)
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
