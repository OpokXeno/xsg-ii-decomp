#include "common.h"
#include "shared.h"
#include "ov01/obj_slot.h"
#include "ov01/unit_cmd.h"
#include "ov01/choice_menu.h"
#include "ov01/gr_gp_init.h"
#include "ov01/menu.h"
#include "ov01/menu_wep_sp_panel.h"
#include "ov01/menu_panel.h"
#include "ov01/menu_tex_state.h"
#include "ov01/calc.h"
#include "main/xgl_flags.h"
#include "ov01/menu_combo_panel.h"

#define SPRITE_Z 16777212.0f

/* Ticks a timed panel lasts after menuComboPanelExtraNew; menuComboPanelExtraStep counts them down. */
#define MENU_TIMED_PANEL_TICKS 20
/* objHatoVec speed of a row panel, and of a stat panel of kind 0 (menuStatPanelStep). */
#define MENU_PANEL_SLIDE_SPEED 2
#define MENU_STAT_BASE_SLIDE_SPEED 1
/* menuStatPanelNew: row and aux x of the kind 0 panel, aux x of the other stat panels and of a row panel. */
#define MENU_STAT_BASE_ROW 82
#define MENU_STAT_BASE_AUX_X 84
#define MENU_PANEL_AUX_X 16
/* Ticks a stat panel waits at state 0 before it frees its row (menuStatPanelStep). */
#define MENU_STAT_PANEL_TICKS 15
/* The stat panel kind that prints a number (menuStatPanelDraw) and that number's digit count. */
#define MENU_STAT_KIND_VALUE 7
#define MENU_STAT_VALUE_DIGITS 2
/* Key cells (menuKeyCellDraw): face cells 60 x 52 in 4 columns, icon cells 24 x 28 in 3 columns, icon row at v 84. */
#define MENU_KEY_FACE_W 60
#define MENU_KEY_FACE_H 52
#define MENU_KEY_FACE_COLS 4
#define MENU_KEY_ICON_W 24
#define MENU_KEY_ICON_H 28
#define MENU_KEY_ICON_COLS 3
#define MENU_KEY_ICON_V 84
/* keyGideDisp: an 18 x 18 texel cell from the atlas origin (0x48, 0x54). */
#define MENU_ICON_SIZE 18
#define MENU_ICON_ATLAS_U 0x48
#define MENU_ICON_ATLAS_V 0x54
/* menuStatusIconsDraw tests the ten low bits of the record mask against menuStatusIconUVs. */
#define MENU_STATUS_ICON_COUNT 10
/* menuStatusIconsDraw: bit 0x20 of the record flags picks the lit sprite of the status cell. */
#define MENU_STATUS_FLAG_ALT 0x20
/* menuStatusIconsDraw / menuStatusIconPut: a status cell is 36 x 17 texels; its sprites sit at these texel positions. */
#define MENU_STATUS_CELL_W 36
#define MENU_STATUS_CELL_H 17
#define MENU_STATUS_VARIANT0_U 632
#define MENU_STATUS_VARIANT1_U 668
#define MENU_STATUS_VARIANT_V 172
#define MENU_STATUS_FLAG_ALT_U 704
#define MENU_STATUS_FLAG_U 668
#define MENU_STATUS_FLAG_V 155
/* menuPanelDraw: the box of a panel is 128 x 20 and its bar starts 10 below the origin. */
#define MENU_PANEL_BOX_W 128
#define MENU_PANEL_BOX_H 20
#define MENU_PANEL_BAR_Y 10
#define MENU_PANEL_BAR_H 10
/* menuPanelShow: a panel slides in from this x. */
#define MENU_PANEL_HIDDEN_X -152.0f
/* menuStatPanelDraw: the bar of a stat panel and the value icon drawn at kind 7. */
#define MENU_STAT_BAR_Y 10
#define MENU_STAT_BAR_H 10
#define MENU_STAT_BAR_SRC_Y 74
#define MENU_STAT_VALUE_ICON_W 16
#define MENU_STAT_VALUE_ICON_H 17
#define MENU_STAT_VALUE_ADVANCE 18
/* menuNumDrawTarget: bit 4 of a target's flags marks it as flagged (its number uses the flagged colours). */
#define MENU_TARGET_FLAG_FLAGGED 4

/* Colour sets of menuNumColors chosen by menuNumPut's colorMode: the plain set, the set of a
 * target whose hp is low (partyUnitHpLow), and the set of a target with flag bit 4. */
enum {
    MENU_NUM_COLOR_NORMAL = 0,
    MENU_NUM_COLOR_HP_LOW = 1,
    MENU_NUM_COLOR_FLAGGED = 2
};

static MenuState *menuPanelCurrent;

typedef int s32;

s32 menuComboPanelExtraStep(MenuState *menu);

/* Allocates a timed panel with the countdown step menuComboPanelExtraStep and the draw of menuComboPanelDraw */
MenuState *menuComboPanelExtraNew(void)
{
  MenuState *menu;

  menu = objSlotAlloc();
  if (menu == 0) {
    return menu;
  }
  menu->updateStatus = menuComboPanelExtraStep;
  menu->draw = menuComboPanelDraw;
  objStdInit(&menu->slide);
  menu->state = 0;
  menu->flag = 0;
  menu->wait = 0;
  menu->value = 0;
  menu->countdown = MENU_TIMED_PANEL_TICKS;
  return menu;
}


/* Update step of the timed panel: state 1 slides
 * the origin with objHatoVec until objStdMove reports arrival; state 0
 * counts countdown down and, at zero, frees the slot row and calls objSlotRelease. */
s32 menuComboPanelExtraStep(MenuState *menu)
{
  int state;
  int status;
  SlideBlock *origin;

  state = menu->state;
  status = 1;
  if (state != 0) {
    if (state == 1) {
      origin = &menu->slide;
      objHatoVec(origin, MENU_PANEL_SLIDE_SPEED);
      status = objStdMove(origin);
      if (status != 0) {
        menu->state = 0;
      }
    }
  } else {
    status = menu->countdown - 1;
    menu->countdown = status;
    if (status <= 0) {
      menuPanelRowFree((s32) menu->slide.position.y);
      status = objSlotRelease(menu);
    }
  }
  return status;
}

void menuPanelStep(MenuState *menu);
void menuPanelDraw(MenuState *menu);

/* Creates the menu panel and makes it current */
void menuPanelCreate(void)
{
  MenuState *menu;

  menu = objSlotAlloc();
  if (menu != 0) {
    menu->update = menuPanelStep;
    menu->draw = menuPanelDraw;
    menuPanelCurrent = menu;
    objStdInit(&menu->slide);
    menu->state = 0;
    menu->flag = 0;
  }
}

/* Update step of the menu panel: slides in while state is 1. */
void menuPanelStep(MenuState *menu)
{
  int state;
  SlideBlock *origin;

  state = menu->state;
  if (state != 0) {
    if (state == 1) {
      /* separate address keeps menu->origin in its own register across both calls */
      origin = &menu->slide;
      objHatoVec(origin, MENU_PANEL_SLIDE_SPEED);
      if (objStdMove(origin) != 0) {
        menu->state = 0;
      }
    }
  }
}


/* Draws the panel while flag is set: a bar from the left
 * edge at y + 10 and a 128 x 20 box at the origin, both inside one grGpInit packet. */
void menuPanelDraw(MenuState *menu)
{
  GrPacket packet;
  int x;
  int y;

  if (menu->flag != 0) {
    x = (int) menu->slide.position.x;
    y = (int) menu->slide.position.y;
    menuTexFlush();
    grGpInit(&packet);
    menuPanelTexState(&packet);
    grOpenSpr(&packet);
    menuStatSprite(&packet, 0, y + MENU_PANEL_BAR_Y, x + MENU_PANEL_BOX_W + MENU_PANEL_BAR_Y, MENU_PANEL_BAR_H, 0x12F - x, 0x4A);
    menuStatSprite(&packet, x, y, MENU_PANEL_BOX_W, MENU_PANEL_BOX_H, 0x180, 0x36);
    grCloseSpr();
    grPacketSend(&packet);
  }
}


/* Shows the menu panel: slides in from x = -152 on a row
 * taken from menuPanelRowAlloc. */
void menuPanelShow(void)
{
  float origin_y;
  menuPanelCurrent->state = 1;
  menuPanelCurrent->flag = 1;
  origin_y = (float) menuPanelRowAlloc();
  menuPanelCurrent->slide.position.x = MENU_PANEL_HIDDEN_X;
  menuPanelCurrent->slide.goal.x = MENU_PANEL_AUX_X;
  menuPanelCurrent->slide.goal.y = (menuPanelCurrent->slide.position.y = origin_y);
}

/* Hides the menu panel and frees its row */
void menuPanelHide(void) {
    menuPanelCurrent->state = 0;
    menuPanelCurrent->flag = 0;
    menuPanelRowFree((s32) menuPanelCurrent->slide.position.y);
}

/* Allocates a stat panel of `kind`: its origin.x is minus the width word of row kind of
 * menuStatRects; kind 0 keeps y 82, other kinds take a row from menuPanelRowAlloc */
typedef struct MenuStatRect {
    int srcX;                   /* +0x00: texel x of the sprite, the srcX argument of menuStatSprite */
    int srcY;                   /* +0x04: texel y of the sprite, the srcY argument of menuStatSprite */
    int width;                  /* +0x08: negated into origin.x by menuStatPanelNew */
    int height;                 /* +0x0C: the height argument of menuStatSprite */
} MenuStatRect;

extern MenuStatRect menuStatRects[];
void menuStatPanelStep(MenuState *menu);
void menuStatPanelDraw(MenuState *menu);

void menuStatPanelNew(s32 kind)
{
    MenuState *menu;
    s32 auxX;
    s32 row;

    menu = objSlotAlloc();
    if (menu == 0) {
        return;
    }
    menu->update = menuStatPanelStep;
    menu->draw = menuStatPanelDraw;
    objStdInit(&menu->slide);
    row = MENU_STAT_BASE_ROW;
    if (kind == 0) {
        auxX = MENU_STAT_BASE_AUX_X;
    } else {
        row = menuPanelRowAlloc();
        auxX = MENU_PANEL_AUX_X;
    }
    menu->slide.position.y = row;
    menu->slide.position.x = -menuStatRects[kind].width;
    menu->state = 1;
    menu->slide.goal.x = auxX;
    menu->slide.goal.y = row;
    menu->flag = kind;
    menu->wait = MENU_STAT_PANEL_TICKS;
    menu->value = actStepCounterGet()->multiplier;
}

/* Update step of the stat panel: slides in at speed 1
 * for kind 0 and 2 otherwise; counts wait down and frees its row at zero. */
void menuStatPanelStep(MenuState *menu)
{
  MenuState **slot;
  int state;
  int remaining;
  SlideBlock *origin;

  /* The argument's address is taken, as in the original frame: the state
   * step reloads the menu through this slot. */
  slot = &menu;
  state = menu->state;
  if (state != 0) {
    if (state == 1) {
      origin = &(*slot)->slide;
      objHatoVec(origin, ((*slot)->flag == 0) ? MENU_STAT_BASE_SLIDE_SPEED : MENU_PANEL_SLIDE_SPEED);
      if (objStdMove(origin) != 0) {
        (*slot)->state = 0;
      }
    }
  } else {
    remaining = menu->wait - 1;
    menu->wait = remaining;
    if (remaining <= 0) {
      menuPanelRowFree((s32) menu->slide.position.y);
      objSlotRelease(menu);
    }
  }
}

/* Draws the stat panel of kind flag from its rectangle row in menuStatRects; at kind 7 it also prints value */
void menuStatPanelDraw(MenuState *menu)
{
    GrPacket packet;
    unsigned int kind;
    int x;
    int y;
    int width;

    kind = menu->flag;
    x = (int) menu->slide.position.x;
    y = (int) menu->slide.position.y;
    menuTexFlush();
    grGpInit(&packet);
    menuPanelTexState(&packet);
    width = menuStatRects[kind].width;
    grOpenSpr(&packet);
    if (kind != 0) {
        menuStatSprite(&packet, 0, y + MENU_STAT_BAR_Y, x + width + MENU_STAT_BAR_Y, MENU_STAT_BAR_H, 431 - (x + width), MENU_STAT_BAR_SRC_Y);
    }
    grCloseSpr();
    if (kind < 3) {
        menuUnitSlotTexState(&packet);
    }
    grOpenSpr(&packet);
    menuStatSprite(&packet, x, y, width, menuStatRects[kind].height, menuStatRects[kind].srcX, menuStatRects[kind].srcY);
    grCloseSpr();
    if (kind == MENU_STAT_KIND_VALUE) {
        menuPanelTexState(&packet);
        grOpenSpr(&packet);
        menuStatSprite(&packet, x + width, y, MENU_STAT_VALUE_ICON_W, MENU_STAT_VALUE_ICON_H, 479, 0);
        grCloseSpr();
        width += MENU_STAT_VALUE_ADVANCE;
        menuNumPut(&packet, x + width, y, menu->value, MENU_STAT_VALUE_DIGITS, 3, MENU_NUM_COLOR_NORMAL);
    }
    grPacketSend(&packet);
}

/* Maps menu kinds 1, 2, 3 to rows 8, 10, 9 and creates that stat panel through menuStatPanelNew;
 * other kinds do nothing */
void menuStatPanelOpen(s32 kind)
{
    s32 row;

    switch (kind) {
    case 1:
        row = 8;
        break;
    case 2:
        row = 10;
        break;
    case 3:
        row = 9;
        break;
    default:
        return;
    }
    menuStatPanelNew(row);
}

void menuKeyDraw(GrPacket *packet, int x, int y, int key, int style)
{
    menuKeyCellDraw(packet, x, y, key, style, style == 0 ? MENU_KEY_FACE_W : MENU_KEY_ICON_W);
}

s32 menuKeyFaceDefaultSheetChk(s32 key);

/* Draws a sprite of width centred in a texel cell: style 0 uses 60 x 52 cells and style 1 uses 24 x 28 */
void menuKeyCellDraw(GrPacket *packet, int x, int y, int key, int style, int width)
{
    int u0;
    int v0;
    int texW;
    int texH;
    int cellX;

    if (style == 0) {
        texW = MENU_KEY_FACE_W;
        texH = MENU_KEY_FACE_H;
        u0 = (menuTexCellIndex(key) % MENU_KEY_FACE_COLS) * texW;
        v0 = (menuTexCellIndex(key) / MENU_KEY_FACE_COLS) * texH;
        if (menuKeyFaceDefaultSheetChk(key) != 0) {
            menuTexStateByKey(packet, key);
        } else {
            menuTexStateAltByKey(packet, key);
        }
    } else {
        texW = MENU_KEY_ICON_W;
        texH = MENU_KEY_ICON_H;
        u0 = (menuTexCellIndex(key) % MENU_KEY_ICON_COLS) * texW;
        v0 = (menuTexCellIndex(key) / MENU_KEY_ICON_COLS) * texH + MENU_KEY_ICON_V;
        menuSmallCellTexState(packet);
    }
    cellX = texW / 2 - width / 2;
    grOpenSpr(packet);
    menuStatSprite2(packet, x + cellX, y, width, texH, u0, v0, u0 + texW, v0 + texH);
    grCloseSpr();
}


/* Result for a key: 1 by default, key 2 compares the value
 * from xglFlagsGet with 87, keys 3 and 6 compare it with 43. */
s32 menuKeyFaceDefaultSheetChk(s32 key)
{
    s32 result;
    s32 flagValue;

    result = 1;
    flagValue = xglFlagsGet(401, 8);
    switch (key) {
    case 2:
        if (flagValue >= 87) {
            result = 0;
        }
        break;
    case 3:
        if (flagValue >= 43) {
            result = 0;
        }
        break;
    case 6:
        result = flagValue < 43;
        break;
    }
    return result;
}


/* Prints `value` in `digits` decimal digits at style `style`, mode 0 */
void menuNumDraw(GrPacket *packet, s32 x, s32 y, s32 value, s32 digits, s32 style) {
    menuNumPut(packet, x, y, value, digits, style, MENU_NUM_COLOR_NORMAL);
}


/* Same printer with the glyph mode taken from the target */
void menuNumDrawTarget(GrPacket *packet, s32 x, s32 y, s32 value, s32 digits, s32 style, BattleUnit *target) {
    s32 colorMode;

    colorMode = MENU_NUM_COLOR_FLAGGED;
    if (!(target->flags & MENU_TARGET_FLAG_FLAGGED)) {
        colorMode = partyUnitHpLow(target) != 0;
    }
    menuNumPut(packet, x, y, value, digits, style, colorMode);
}

/* Identical to menuNumDraw (kept as a separate function; no caller evidence separates them). target is unused;
 * menuPlStatPut passes the unit as it does to menuNumDrawTarget. */
void menuNumDrawTargetEp(GrPacket *packet, s32 x, s32 y, s32 value, s32 digits, s32 style, BattleUnit *target) {
    menuNumPut(packet, x, y, value, digits, style, MENU_NUM_COLOR_NORMAL);
}

/* Decimal number printer: packet, x, y, value, digits,
 * style and mode, the glyph set of menuNumColors. */
extern MenuDigitStyle menuStyleTable[];
extern MenuRGBA menuNumColors[];

void menuNumPut(GrPacket *packet, int x, int y, int value, int digits, int style, int colorMode)
{
    int baseY;
    int glyphH;
    int glyphSrcY;
    int place;
    int remaining;
    MenuDigitStyle *styleEntry;
    int *digitSrcX;

    baseY = y;
    glyphH = 14;
    glyphSrcY = 50;
    if (style != 2) {
        if (style < 3) {
            glyphH = 27;
            glyphSrcY = 23;
            if (style != 1) {
                glyphSrcY = 0;
                glyphH = 23;
            }
        } else if (style == 3) {
            glyphH = 18;
            glyphSrcY = 18;
        } else {
            glyphH = 23;
            glyphSrcY = 0;
        }
    }

    if (style == 3) {
        menuPanelTexState(packet);
    } else if (style == 1) {
        menuNumStyle1TexState(packet);
    } else {
        menuNumTexState(packet);
    }
    grOpenSpr(packet);

    place = 1;
    for (remaining = 0; remaining < digits - 1; remaining++) {
        place *= 10;
    }
    styleEntry = menuStyleEntry(style);
    if (digits > 0) {
        remaining = digits;
        /* digit d's width is styleEntry->width[d]; its texel x is srcX[d], reached as digitSrcX[d + 8] (the base of the loop is kept) */
        digitSrcX = styleEntry->width + 2;
        do {
            int digit;
            int digitWidth;
            int *widthSlot;

            remaining--;
            digit = value / place;
            value = value % place;
            widthSlot = styleEntry->width + digit;
            digitWidth = *widthSlot;
            place = place / 10;
            menuStatSpriteRGB(packet, x, baseY, digitWidth, glyphH, digitSrcX[digit + 8], glyphSrcY, &menuNumColors[colorMode]);
            x += digitWidth;
        } while (remaining != 0);
    }
    grCloseSpr();
}

/* Entry `index` of the style table menuStyleTable. */
MenuDigitStyle *menuStyleEntry(int index) {
    return &menuStyleTable[index];
}


/* Draws one 18 x 18 icon from the atlas at (0x48, 0x54), cell (col, row). */
void keyGideDisp(GrPacket *packet, int x, int y, unsigned int col, unsigned int row)
{
    int iconSize;
    int u;
    int v;

    menuIconTexState(packet);
    iconSize = MENU_ICON_SIZE;
    col = col * iconSize;
    row = row * iconSize;
    v = row + MENU_ICON_ATLAS_V;
    u = col + MENU_ICON_ATLAS_U;
    grOpenSpr(packet);
    menuStatSprite(packet, x, y, MENU_ICON_SIZE, MENU_ICON_SIZE, u, v);
    grCloseSpr();
}

typedef struct MenuStatusUV {
    int srcX;
    int srcY;
} MenuStatusUV;

extern MenuStatusUV menuStatusIconUVs[];

/* Draws a status icon per set bit of the record's 16-bit mask, from menuStatusIconUVs */
void menuStatusIconsDraw(GrPacket *packet, int x, int y, int index)
{
    struct BattleState *battle;
    TecRecord *record;
    MenuStatusUV *iconUV;
    int bitIndex;

    battle = manWorkGet();
    if (battle->sideToggle != 0) { /* the L1/L2 switch also hides the status icons */
        return;
    }
    record = tecRecordGet(index);
    menuStatusIconsTexState(packet);
    grOpenSpr(packet);
    if (record->type == 0) {
        menuStatSprite(packet, x, y, MENU_STATUS_CELL_W, MENU_STATUS_CELL_H, MENU_STATUS_VARIANT0_U, MENU_STATUS_VARIANT_V);
    } else if (record->type == 1) {
        menuStatSprite(packet, x, y, MENU_STATUS_CELL_W, MENU_STATUS_CELL_H, MENU_STATUS_VARIANT1_U, MENU_STATUS_VARIANT_V);
    }
    x += MENU_STATUS_CELL_W;
    if (record->flags & MENU_STATUS_FLAG_ALT) {
        menuStatSprite(packet, x, y, MENU_STATUS_CELL_W, MENU_STATUS_CELL_H, MENU_STATUS_FLAG_ALT_U, MENU_STATUS_FLAG_V);
    } else {
        menuStatSprite(packet, x, y, MENU_STATUS_CELL_W, MENU_STATUS_CELL_H, MENU_STATUS_FLAG_U, MENU_STATUS_FLAG_V);
    }
    x += MENU_STATUS_CELL_W;
    iconUV = menuStatusIconUVs;
    {
        int one = 1;

        for (bitIndex = 0; bitIndex < MENU_STATUS_ICON_COUNT; bitIndex++) {
            int bit;

            bit = one << bitIndex;
            if (record->actionBits & bit) {
                menuStatSprite(packet, x, y, MENU_STATUS_CELL_W, MENU_STATUS_CELL_H, iconUV->srcX, iconUV->srcY);
                x += MENU_STATUS_CELL_W;
            }
            iconUV++;
        }
    }
    grCloseSpr();
}

void menuStatusIconPut(GrPacket *packet, s32 x, s32 y)
{
  menuStatusIconsTexState(packet);
  grOpenSpr(packet);
  menuStatSprite(packet, x, y, MENU_STATUS_CELL_W, MENU_STATUS_CELL_H, MENU_STATUS_FLAG_ALT_U, MENU_STATUS_VARIANT_V);
  grCloseSpr();
}

extern unsigned long long menuStatSpriteColor;

/* Sprite with the colour word at menuStatSpriteColor */
void menuStatSprite(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY)
{
    GrSpr sprite;

    sprite.vertex[0].x = x;
    sprite.vertex[0].y = y;
    sprite.vertex[0].z = SPRITE_Z;
    sprite.vertex[1].x = x + width;
    sprite.vertex[1].y = y + height;
    sprite.vertex[1].z = SPRITE_Z;
    sprite.u[0] = srcX;
    sprite.u[1] = srcX + width + 1;
    sprite.v[0] = srcY;
    sprite.v[1] = srcY + height + 1;
    sprite.col.word = menuStatSpriteColor;
    grPutSpr(packet, &sprite, 0);
}

extern unsigned long long menuStatSprite2Color;

/* Sprite with independent texel bounds and the colour word at menuStatSprite2Color */
void menuStatSprite2(GrPacket *packet, int x, int y, int width, int height, int u0, int v0, int u1, int v1)
{
    GrSpr sprite;

    sprite.u[0] = u0;
    sprite.vertex[0].x = x;
    sprite.vertex[0].y = y;
    sprite.vertex[0].z = SPRITE_Z;
    sprite.v[0] = v0;
    sprite.vertex[1].x = x + width;
    sprite.vertex[1].y = y + height;
    sprite.vertex[1].z = SPRITE_Z;
    sprite.u[1] = u1 + 1;
    sprite.v[1] = v1 + 1;
    sprite.col.word = menuStatSprite2Color;
    grPutSpr(packet, &sprite, 0);
}

void menuStatSpriteRGB(GrPacket *packet, int x, int y, int width, int height, int srcX, int srcY, const MenuRGBA *rgba) {
    GrSpr sprite;

    sprite.vertex[0].x = x;
    sprite.vertex[0].y = y;
    sprite.vertex[0].z = SPRITE_Z;
    sprite.vertex[1].x = x + width;
    sprite.vertex[1].y = y + height;
    sprite.vertex[1].z = SPRITE_Z;
    sprite.u[0] = srcX;
    sprite.u[1] = srcX + width + 1;
    sprite.v[0] = srcY;
    sprite.v[1] = srcY + height + 1;
    sprite.col.rgba = *rgba;
    grPutSpr(packet, &sprite, 0);
}

/* menuStatSprite2 with the colour copied from the caller's MenuRGBA */
void menuStatSprite2RGB(GrPacket *packet, int x, int y, int width, int height, int u0, int v0, int u1, int v1, const MenuColor *color)
{
    GrSpr sprite;

    sprite.u[0] = u0;
    sprite.vertex[0].x = x;
    sprite.vertex[0].y = y;
    sprite.vertex[0].z = SPRITE_Z;
    sprite.v[0] = v0;
    sprite.vertex[1].x = x + width;
    sprite.vertex[1].y = y + height;
    sprite.vertex[1].z = SPRITE_Z;
    sprite.u[1] = u1 + 1;
    sprite.v[1] = v1 + 1;
    sprite.col.rgba = color->rgba;
    grPutSpr(packet, &sprite, 0);
}

/* Stat panel rectangles: texel srcX, srcY, width and height of each kind (menuStatRects[kind]). */
MenuStatRect menuStatRects[11] = {
    { 0x92, 0x27, 0x40, 0x11 },
    { 0x92, 0x38, 0x4a, 0x11 },
    { 0x92, 0x49, 0x40, 0x11 },
    { 0x122, 0x36, 0x5d, 0x12 },
    { 0x100, 0x36, 0x7f, 0x12 },
    { 0x100, 0x56, 0x70, 0x12 },
    { 0x1ad, 0x56, 0x50, 0x12 },
    { 0x17f, 0x0, 0x5d, 0x12 },
    { 0x100, 0x0, 0x80, 0x12 },
    { 0x100, 0x12, 0x80, 0x12 },
    { 0x100, 0x24, 0x80, 0x12 },
};

/* The number colour sets: menuNumColors[colorMode] (MENU_NUM_COLOR_*). */
MenuRGBA menuNumColors[4] = {
    { 0xa0, 0xa0, 0xa0, 0x80 },
    { 0xff, 0xff, 0x60, 0x80 },
    { 0xff, 0x60, 0x60, 0x80 },
    { 0x00, 0x00, 0x00, 0x00 },
};

/* Digit styles of menuNumPut: ten digit widths, then the ten texel x of the digits. */
MenuDigitStyle menuStyleTable[4] = {
    {
        { 0xc, 0x8, 0xb, 0xb, 0xb, 0xa, 0xb, 0xa, 0xc, 0xb },
        { 0x0, 0xc, 0x14, 0x1f, 0x2a, 0x35, 0x3f, 0x4a, 0x54, 0x60 },
    },
    {
        { 0xf, 0x9, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf },
        { 0x0, 0xf, 0x18, 0x27, 0x36, 0x45, 0x54, 0x63, 0x72, 0x80 },
    },
    {
        { 0x9, 0x7, 0x9, 0x9, 0x9, 0x8, 0x8, 0x8, 0x8, 0x9 },
        { 0x0, 0x9, 0x10, 0x19, 0x22, 0x2b, 0x33, 0x3b, 0x43, 0x4b },
    },
    {
        { 0xd, 0xa, 0xd, 0xd, 0xd, 0xd, 0xd, 0xd, 0xd, 0xd },
        { 0x180, 0x18d, 0x197, 0x1a4, 0x1b1, 0x1be, 0x1cb, 0x1d8, 0x1e5, 0x1f2 },
    },
};

/* Texel position of each status icon bit (menuStatusIconsDraw). */
MenuStatusUV menuStatusIconUVs[10] = {
    { 0x29c, 0x8a },
    { 0x278, 0x8a },
    { 0x2c0, 0x79 },
    { 0x278, 0x79 },
    { 0x29c, 0x79 },
    { 0x2c0, 0x68 },
    { 0x29c, 0x68 },
    { 0x278, 0x68 },
    { 0x278, 0x9b },
    { 0x2c0, 0x8a },
};

/* Colour word of menuStatSpriteColor. */
unsigned long long menuStatSpriteColor = 0x80808080ULL;

/* Colour word of menuStatSprite2Color. */
unsigned long long menuStatSprite2Color = 0x80808080ULL;


/* Object of the arrow sprite (set and released by menu.c). */
struct ArrowObject *arrowObj;
