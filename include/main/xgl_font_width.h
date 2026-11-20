#ifndef INCLUDE_MAIN_XGL_FONT_WIDTH_H
#define INCLUDE_MAIN_XGL_FONT_WIDTH_H

/* Functions of src/main/xgl_font_width.c that other files call: the width of a text whose value
 * placeholders are replaced by the numbers of a list (the box measurer textBoxMeasure drives them). */

int xglFontWidthBegin(int *values);
void xglFontWidthReset(void);
void xglFontWidthAddValue(void);
int xglFontWidthGetSum(void);

#endif /* INCLUDE_MAIN_XGL_FONT_WIDTH_H */
