#ifndef INCLUDE_MAIN_YG2PATH_H
#define INCLUDE_MAIN_YG2PATH_H

#include "shared.h"

/* Functions of src/main/yg2path.c that other files call. */

/* Origin of the 2D sprites on screen (read by render_sprite.c). */
extern int renderOriginX;
extern int renderOriginY;

void yg2PathClearProgress(void);
void yg2PathRun(void);
int yg2PathGetCitizenFlag(u32 index);
/* Finds the line starting with pattern, then copies its quoted text to out; returns the position after the text. */
char *yg2PathReadTaggedText(char *text, const char *pattern, char *out);

#endif /* INCLUDE_MAIN_YG2PATH_H */
