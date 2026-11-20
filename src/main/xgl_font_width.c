#include "common.h"
#include "main/xgl_font.h"
#include "main/xgl_font_width.h"

static int xglFontWidthSum;
static int *xglFontWidthValues;
static int xglFontWidthIndex;
extern int xglFontWidthBase;
extern int xglFontWidthReady;
extern const char fontWidthPrefixText[];
extern char xglFontWidthFormat[];
extern char xglFontPropTag[];
extern char xglFontWidthPlaceholder[];
extern char xglFontWidthValueFormat[];
int sprintf(char *buffer, const char *format, ...);
void xglFontWidthMeasureTemplate(void);

/*
 * Starts a width-summing run over the value list `values`: the list is stored, the
 * running sum and index are reset, and the width of the bare template is measured once
 * (xglFontWidthMeasureTemplate). Returns 1 once the template width is known.
 */
int xglFontWidthBegin(int *values)
{
    int ready;

    ready = xglFontWidthReady;
    xglFontWidthValues = values;
    xglFontWidthSum = 0;
    xglFontWidthIndex = 0;
    if (ready == 0)
    {
        xglFontWidthMeasureTemplate();
        ready = 1;
        xglFontWidthReady = 1;
    }
    return ready;
}

/*
 * Measures the template "<ruby prefix>$prop1;%s" with the value placeholder left literal;
 * every later item is measured against this base width.
 */
void xglFontWidthMeasureTemplate(void)
{
    char template_text[80];

    sprintf(template_text, xglFontWidthFormat, fontWidthPrefixText, xglFontPropTag, xglFontWidthPlaceholder);
    xglFontWidthBase = xglFontGetStringWidth(template_text);
}

/*
 * Formats the current list entry as "%d", substitutes it into the template, and adds the
 * width beyond the template base to the running sum; then advances the index.
 */
void xglFontWidthAddValue(void)
{
    char value_text[16];
    char item_text[80];
    int width;

    if (xglFontWidthValues != 0)
    {
        sprintf(value_text, xglFontWidthValueFormat, xglFontWidthValues[xglFontWidthIndex]);
        sprintf(item_text, xglFontWidthFormat, fontWidthPrefixText, xglFontPropTag, value_text);
        width = xglFontGetStringWidth(item_text);
        xglFontWidthSum = xglFontWidthSum + (width - xglFontWidthBase);
        xglFontWidthIndex = xglFontWidthIndex + 1;
    }
}

void xglFontWidthReset(void)
{
    xglFontWidthSum = 0;
}

/* Sum of the widths added since the last reset. */
int xglFontWidthGetSum(void)
{
    return xglFontWidthSum;
}

const char fontWidthPrefixText[40] = "\013\031\002\r\002$xx0,30;$rubyz23,21;$rubyy-2;";

int xglFontWidthBase = 0;

int xglFontWidthReady = 0;

char xglFontWidthFormat[8] = "%s%s%s";

char xglFontPropTag[8] = "$prop1;";

char xglFontWidthPlaceholder[8] = "%s";

char xglFontWidthValueFormat[4] = "%d";
