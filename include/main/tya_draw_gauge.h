#ifndef INCLUDE_MAIN_TYA_DRAW_GAUGE_H
#define INCLUDE_MAIN_TYA_DRAW_GAUGE_H

/* Gauge geometry (in pixels) and the filled / unfilled colours. */
typedef struct TyaGaugeArgs {
    short x;
    short y;
    unsigned int vertex_attribute;
    short width;
    short height;
    unsigned short current;
    unsigned short maximum;
    unsigned char filled_color[4];
    unsigned char unfilled_color[4];
} TyaGaugeArgs;

/* Draws a horizontal gauge filled to current / maximum of its width. */
void tyaDrawGauge(TyaGaugeArgs *gauge);

#endif /* INCLUDE_MAIN_TYA_DRAW_GAUGE_H */
