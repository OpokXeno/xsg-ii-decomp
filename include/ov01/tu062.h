#ifndef INCLUDE_OV01_TU062_H
#define INCLUDE_OV01_TU062_H

#include "ov01/tu048.h"

struct Ov01Object;

/* One RGBA colour of a menu sprite. */
typedef struct MenuRGBA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} MenuRGBA;

/* The sprite colour: menuStatSpriteRGB copies a caller's MenuRGBA into it
 * byte-wise (lwl/lwr, swl/swr), while menuStatSprite/menuStatSprite2 copy the
 * whole word from an aligned local (lw/sw), so the field is word-aligned. */
typedef union MenuColor {
    unsigned int word;
    MenuRGBA rgba;
} MenuColor;

/* Functions of src/ov01/tu062.c that other files call. */
void menuPanelCreate(void);
void menuPanelShow(void);
/* Opens a stat panel of kind. */
void menuStatPanelNew(int kind);

/* menuStatPanelNew kinds attackHitsShow opens for a hit (rows of menuStatRects). */
enum {
    MENU_STAT_PANEL_NO_HIT = 1,         /* hit kind 0 */
    MENU_STAT_PANEL_BLOCKED = 2,        /* hitMode bit 9 */
    MENU_STAT_PANEL_TAKEN = 3,          /* hitMode bit 8 */
    MENU_STAT_PANEL_TAKEN_BONUS = 4,    /* hitMode bit 8 after calcCritDamage's bonus roll */
    MENU_STAT_PANEL_BACK_ATTACK = 6     /* HitRecord.backAttack */
};
/* Opens the stat panel of a step state (1 to 3, ActStepCounter.stepState). */
void menuStatPanelOpen(int stepState);

/* Draws the face of the unit with key `key`; style picks the smaller of the two frames. */
void menuKeyDraw(GrPacket *packet, int x, int y, int key, int style);
/* A sprite of width x height at (x, y) from the texel rectangle at (srcX, srcY) of the same size. */
void menuStatSprite(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY);
/* A sprite of width x height at (x, y) from the texel rectangle (u0, v0)-(u1, v1), which the
 * gauges keep at full size while the width shrinks. */
void menuStatSprite2(GrPacket *packet, int x, int y, int width, int height, int u0, int v0, int u1, int v1);
void menuStatSpriteRGB(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY, const MenuRGBA *rgba);
void menuStatSprite2RGB(GrPacket *packet, int x, int y, int width, int height, int u0, int v0, int u1, int v1, const MenuColor *color);
void menuNumDraw(GrPacket *packet, int x, int y, int value, int digits, int style);
void menuNumDrawTarget(GrPacket *packet, int x, int y, int value, int digits, int style, struct Ov01Object *target);
void menuNumDrawMode(GrPacket *packet, int x, int y, int value, int digits, int style, int colorMode);
void func_00AA7A48(GrPacket *packet, int x, int y, int value, int digits, int style, struct Ov01Object *target);
/* One 18 x 18 icon from the icon atlas, cell (col, row). */
void menuIconPut(GrPacket *packet, int x, int y, unsigned int col, unsigned int row);
void menuStatusIconPut(GrPacket *packet, int x, int y);
/* The status icons of a tec record (tecRecordGet). */
void menuStatusIconsDraw(GrPacket *packet, int x, int y, int record);

#endif /* INCLUDE_OV01_TU062_H */
