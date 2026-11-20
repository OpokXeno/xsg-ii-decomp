#include "common.h"
#include "main/timecode_counter.h"
#include "main/msg_slot_center.h"

/* Indices into one control point of grControlPoints. A struct {float x, y;} point type changes grControlPointSet's code (6 rows),
 * so the table stays an array of float pairs. */
#define POINT_X 0
#define POINT_Y 1
#define CONTROL_POINT_COUNT 3   /* control points blended by grControlPointBlend */
#define SCREEN_CENTER_X 0x100   /* half of the 512-pixel display width */
#define SCREEN_CENTER_Y 0xE0    /* half of the 448-line display height */

/* The three control points (x, y) that grControlPointBlend blends (set by grControlPointSet). */
extern float grControlPoints[][2];

/* Sets control point `index` to the screen position (x, y). */
void grControlPointSet(int index, int x, int y) {
    grControlPoints[index][POINT_X] = x;
    grControlPoints[index][POINT_Y] = y;
}

/*
 * Position at `fraction` (clamped to 1) along the three control points: weights are
 * (1 - f)^3, 1 - w0 - w2 and f^3. The result is the truncated screen position.
 */
void grControlPointBlend(int *out_x, int *out_y, float fraction) {
    float weights[CONTROL_POINT_COUNT];
    float complement;
    float sum_x;
    float sum_y;
    int i;

    if (fraction > 1.0f) {
        fraction = 1.0f;
    }
    complement = 1.0f - fraction;
    weights[2] = fraction * fraction * fraction;
    weights[0] = complement * complement * complement;
    weights[1] = (1.0f - weights[0]) - weights[2];

    sum_x = 0.0f;
    sum_y = 0.0f;
    for (i = 0; i < CONTROL_POINT_COUNT; i++) {
        sum_x += grControlPoints[i][POINT_X] * weights[i];
        sum_y += grControlPoints[i][POINT_Y] * weights[i];
    }
    *out_x = (int) sum_x;
    *out_y = (int) sum_y;
}

/* X origin that centres the record on a 512-pixel-wide screen. */
int msgSlotCenterX(MsgSlot *record) {
    return SCREEN_CENTER_X - (record->extent_x >> 1);
}

/* Y origin that centres the record on a 448-line screen. */
int msgSlotCenterY(MsgSlot *record) {
    return SCREEN_CENTER_Y - (record->extent_y >> 1);
}

/* True when text starts with a value conversion, %d or %s. */
int textIsValueEscape(const char *text) {
    int conversion;

    if (text[0] != '%') {
        return 0;
    }
    conversion = text[1];
    if (conversion == 'd') {
        return 1;
    }
    return conversion == 's';
}

/* True when the character at text is a space. */
int textIsSpace(const char *text) {
    return *text == ' ';
}

/* Stores value in the slot's z (its depth). */
void msgSlotSetZ(MsgSlot *record, int value) {
    record->z = value;
}

/* True when text starts with a line break: a newline or the two characters backslash, n. */
int textIsLineBreak(const char *text) {
    if (text[0] == '\n') {
        return 1;
    }
    if (text[0] == '\\' && text[1] == 'n') {
        return 1;
    }
    return 0;
}

/* Skips one line break at text (a newline, or the two characters backslash, n); returns text unchanged when there is none. */
const char *textSkipLineBreak(const char *text) {
    const char *p = text;
    if (text[0] == '\n') {
        p = text + 1;
    } else if (text[0] == '\\' && text[1] == 'n') {
        p = text + 2;
    }
    return p;
}

/* Initial control point values, as the original data holds them. */
float grControlPoints[4][2] = {
    { 0.0f, 0.0f },
    { 1.0f, 0.0f },
    { 1.0f, 1.0f },
    { 0.0f, 0.0f },
};
