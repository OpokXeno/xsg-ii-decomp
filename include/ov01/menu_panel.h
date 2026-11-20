#ifndef INCLUDE_OV01_MENU_PANEL_H
#define INCLUDE_OV01_MENU_PANEL_H

#include "ov01/gr_gp_init.h"

struct BattleUnit;

/* One entry of menuStyleTable (menuStyleEntry): ten digit widths, then the ten texel x of the digits. */
typedef struct MenuDigitStyle {
    int width[10];
    int srcX[10];
} MenuDigitStyle;

MenuDigitStyle *menuStyleEntry(int index);

/* Two colours copied as one 8-byte block (the boost palette of menu.c, the row colours of menu_wep_sp_panel.c). */
typedef struct ColourPair {
    MenuRGBA colours[2];
} ColourPair;

/* Functions of src/ov01/menu_panel.c that other files call. */
void menuPanelCreate(void);
void menuPanelShow(void);
/* Allocates a panel with the countdown step of a timed panel (MENU_TIMED_PANEL_TICKS ticks) and the draw of
 * menuComboPanelDraw; returns 0 when no slot is free. */
struct MenuState *menuComboPanelExtraNew(void);
/* Opens a stat panel of kind. */
void menuStatPanelNew(int kind);

/* menuStatPanelNew kinds attackHitsShow opens for a hit (rows of menuStatRects). */
enum {
    MENU_STAT_PANEL_NO_HIT = 1,         /* hit kind 0 */
    MENU_STAT_PANEL_BLOCKED = 2,        /* hitMode bit 9 */
    MENU_STAT_PANEL_TAKEN = 3,          /* hitMode bit 8 */
    MENU_STAT_PANEL_TAKEN_BONUS = 4,    /* hitMode bit 8 after calcCrit's bonus roll */
    MENU_STAT_PANEL_BACK_ATTACK = 6     /* HitRecord.backAttack */
};
/* Opens the stat panel of a step state (1 to 3, ActStepCounter.stepState). */
void menuStatPanelOpen(int stepState);

/* Draws the face of the unit with key `key`; style picks the smaller of the two frames. */
void menuKeyDraw(GrPacket *packet, int x, int y, int key, int style);
/* Draws one key cell: style 0 uses 60 x 52 face cells, style 1 24 x 28 icon cells; width is the sprite width. */
void menuKeyCellDraw(GrPacket *packet, int x, int y, int key, int style, int width);
/* A sprite of width x height at (x, y) from the texel rectangle at (srcX, srcY) of the same size. */
void menuStatSprite(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY);
/* A sprite of width x height at (x, y) from the texel rectangle (u0, v0)-(u1, v1), which the
 * gauges keep at full size while the width shrinks. */
void menuStatSprite2(GrPacket *packet, int x, int y, int width, int height, int u0, int v0, int u1, int v1);
void menuStatSpriteRGB(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY, const MenuRGBA *rgba);
void menuStatSprite2RGB(GrPacket *packet, int x, int y, int width, int height, int u0, int v0, int u1, int v1, const MenuColor *color);
void menuNumDraw(GrPacket *packet, int x, int y, int value, int digits, int style);
void menuNumDrawTarget(GrPacket *packet, int x, int y, int value, int digits, int style, struct BattleUnit *target);
void menuNumPut(GrPacket *packet, int x, int y, int value, int digits, int style, int colorMode);
void menuNumDrawTargetEp(GrPacket *packet, int x, int y, int value, int digits, int style, struct BattleUnit *target);
/* One 18 x 18 icon from the icon atlas, cell (col, row). */
void keyGideDisp(GrPacket *packet, int x, int y, unsigned int col, unsigned int row);
void menuStatusIconPut(GrPacket *packet, int x, int y);
/* The status icons of a tec record (tecRecordGet). */
void menuStatusIconsDraw(GrPacket *packet, int x, int y, int record);
/* Hides the menu panel and frees its row. */
void menuPanelHide(void);

#endif /* INCLUDE_OV01_MENU_PANEL_H */
