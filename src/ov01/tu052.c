/*
 * OV01 original TU 52: 0x00aa4280..0x00aa4670 (5 functions)
 */
#include "common.h"
#include "ov01/tu005.h"
#include "ov01/tu051.h"
#include "ov01/tu052.h"
#include "ov01/tu012.h"
#include "ov01/tu022.h"
#include "ov01/tu023.h"
#include "ov01/tu036.h"
#include "ov01/tu048.h"
#include "ov01/tu062.h"

int actStockHasBoost(void);
void *objSlotAlloc(void);

/* Show the cursor: enabled, and when actStockHasBoost is clear, ready at the home position. */
void menuCursorShow(void)
{
    menuCursor->kind.cursor.visible = 1;
    if (actStockHasBoost() == 0) {
        menuCursor->moving = 1;
        menuCursor->slide.pos.target_x = MENU_CURSOR_HOME_X;
        menuCursor->slide.pos.target_y = MENU_CURSOR_HOME_Y;
    }
}

/* Hide the cursor: disabled and parked at (288, 454). */
void menuCursorHide(void)
{
    menuCursor->slide.pos.x = MENU_CURSOR_HOME_X;
    menuCursor->slide.pos.y = MENU_CURSOR_PARKED_Y;
    menuCursor->kind.cursor.visible = 0;
}

extern MenuObject *menuIconListObj;

/* Position of the icon list. */
#define MENU_ICON_LIST_X 8.0f
#define MENU_ICON_LIST_Y 312.0f

void menuIconListUpdate(MenuObject *list);
void menuIconTableDraw(MenuObject *list);

/* Creates the icon list object: no update step (menuIconListUpdate is empty), drawn
 * by menuIconTableDraw, with a cleared slide block at the list position. */
void menuIconListCreate(void)
{
    MenuObject *list;

    list = objSlotAlloc();
    if (list != 0) {
        list->update = menuIconListUpdate;
        list->draw = menuIconTableDraw;
        menuIconListObj = list;
        ov01MatrixBlockInit(&list->slide.block);
        list->slide.pos.x = MENU_ICON_LIST_X;
        list->slide.pos.y = MENU_ICON_LIST_Y;
        list->slide.pos.z = 0;
        list->moving = 0;
    }
}

/* Update step of the icon list: nothing to do. */
void menuIconListUpdate(MenuObject *list)
{
}

/* One row of a command icon table: its icons (ended by MENU_ICON_END) and its kind, the key dataKeyCheck and
 * tableKeyEntryGet look up. */
typedef struct MenuIconRow {
    int *icons;
    int kind;
} MenuIconRow;

#define MENU_ICON_END 4
/* Icon bit that draws the icon from the second atlas row whatever the key state. */
#define MENU_ICON_ALT_ROW 0x80
/* Row kinds drawn without a key check: the status row gets menuStatusIconPut. */
#define MENU_ICON_ROW_PLAIN 0x14
#define MENU_ICON_ROW_STATUS 0x16
/* dataKeyCheck results: 0 and 1 draw the icons (1 from the second atlas row), 2 skips the row. */
#define MENU_ICON_KEY_HIDDEN 2
#define MENU_ICON_WIDTH 18
#define MENU_ICON_ROW_HEIGHT 20
/* Rows of kinds 8..10 put their status icons right after the icons, the others at MENU_ICON_STATUS_X. */
#define MENU_ICON_ROW_INLINE_FIRST 8
#define MENU_ICON_ROW_INLINE_COUNT 3
#define MENU_ICON_STATUS_X 36.0f

/* The icon tables per selection kind: for units with key 1..10, and for the others. */
extern MenuIconRow *menuIconTablesKeyed[];
extern MenuIconRow *menuIconTablesOther[];

/* Draws the command icon rows of the current unit's selection kind from the list position down: per row its icons
 * (state from dataKeyCheck), then its status icons. Nothing while the battle state is not active or the unit has
 * script events. */
void menuIconTableDraw(MenuObject *list)
{
    GrPacket packet;
    Ov01Object *unit;
    Ov01Object *target;
    MenuIconRow *rows;
    int page;
    int state;
    int x;
    int y;
    int r;
    int i;
    int icon;
    int record;
    int cellRow;

    unit = battleStateGet()->current;
    if (battleStateGet()->active != 1) {
        return;
    }
    if (unit->flags & UNIT_FLAG_SCRIPT_EVENTS) {
        return;
    }
    target = tgtUnitGet();
    page = battleStateGet()->selectionKind;
    grGpInit(&packet);
    if ((unsigned int)(unit->stats->keyUnsigned - UNIT_KEY_PARTY_FIRST) < UNIT_KEY_PARTY_COUNT) {
        rows = menuIconTablesKeyed[page];
    } else {
        rows = menuIconTablesOther[page];
    }
    y = list->slide.pos.y;
    if (rows != 0 && rows[0].kind != 0) {
        r = 0;
        do {
            if (rows[r].kind == MENU_ICON_ROW_PLAIN || rows[r].kind == MENU_ICON_ROW_STATUS) {
                state = 0;
            } else {
                state = dataKeyCheck(unit, rows[r].kind, target);
            }
            x = list->slide.pos.x;
            for (i = 0; rows[r].icons[i] != MENU_ICON_END; i++) {
                if (state < MENU_ICON_KEY_HIDDEN) {
                    if (state >= 0) {
                        icon = rows[r].icons[i];
                        /* Two ifs on a cleared row (codegen: a ternary or `cellRow = state != 0` gives the
                         * movz the other way round). */
                        cellRow = 0;
                        if (state != 0) {
                            cellRow = 1;
                        }
                        if (icon & MENU_ICON_ALT_ROW) {
                            cellRow = 1;
                        }
                        menuIconPut(&packet, x, y, icon & ~MENU_ICON_ALT_ROW, cellRow);
                        x += MENU_ICON_WIDTH;
                    }
                }
            }
            if ((unsigned int)(rows[r].kind - MENU_ICON_ROW_INLINE_FIRST) < MENU_ICON_ROW_INLINE_COUNT) {
                x += MENU_ICON_WIDTH;
            } else {
                x = list->slide.pos.x + MENU_ICON_STATUS_X;
            }
            if (rows[r].kind == MENU_ICON_ROW_STATUS) {
                menuStatusIconPut(&packet, x, y);
            } else if (rows[r].kind != MENU_ICON_ROW_PLAIN && state == 0) {
                record = tableKeyEntryGet(unit, rows[r].kind, target);
                if (record < 0) {
                    record = battleEffectResolve(unit, record, target);
                }
                if (record != 0) {
                    menuStatusIconsDraw(&packet, x, y, record);
                }
            }
            if (state != MENU_ICON_KEY_HIDDEN) {
                y += MENU_ICON_ROW_HEIGHT;
            }
            r++;
        } while (rows[r].kind != 0);
    }
    grPacketSend(&packet);
}

/* The icon list object, a common global: tu066 names it too. */
MenuObject *menuIconListObj;

/* The row lists of menuIconTableDraw inside the row array ov01/tu051 defines as D_00AB63C0. */
extern MenuIconRow D_00AB63C0[];

MenuIconRow *menuIconTablesKeyed[12] = {
    &D_00AB63C0[1], &D_00AB63C0[8], &D_00AB63C0[12], &D_00AB63C0[16],
    &D_00AB63C0[16], &D_00AB63C0[16], &D_00AB63C0[16], &D_00AB63C0[19],
    &D_00AB63C0[22], &D_00AB63C0[25], 0, 0
};
MenuIconRow *menuIconTablesOther[4] = { &D_00AB63C0[28], 0, 0, 0 };
