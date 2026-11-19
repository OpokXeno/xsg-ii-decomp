/*
 * OV01 original TU 51: 0x00aa3e28..0x00aa4280 (5 functions)
 */
#include "common.h"
#include "ov01/tu005.h"
#include "ov01/tu012.h"
#include "ov01/tu023.h"
#include "ov01/tu036.h"
#include "ov01/tu048.h"
#include "ov01/tu051.h"

int objStdMove(void *);
void slideSetStep(void *, int);
void *objSlotAlloc(void);
void menuCursorUpdate(MenuCursor *cursor);
void menuCursorDraw(MenuCursor *cursor);

/* Creates the menu cursor: update and draw callbacks, a cleared slide block
 * with the cursor parked below the home position, not moving and hidden. */
void menuCursorCreate(void)
{
    MenuCursor *cursor;

    cursor = objSlotAlloc();
    if (cursor != 0) {
        cursor->update = menuCursorUpdate;
        cursor->draw = menuCursorDraw;
        menuCursor = cursor;
        ov01MatrixBlockInit(&cursor->slide.block);
        cursor->slide.pos.x = MENU_CURSOR_HOME_X;
        cursor->slide.pos.y = MENU_CURSOR_PARKED_Y;
        cursor->slide.pos.z = 0;
        cursor->moving = 0;
        cursor->kind.cursor.visible = 0;
    }
}

/* Cursor update (callback +4 of menuCursor): state 1 steps the cursor position and clears
 * state on arrival. */
void menuCursorUpdate(MenuCursor *cursor) {
    void *pos;

    if (cursor->moving != 0) {
        if (cursor->moving == 1) {
            pos = &cursor->slide;
            slideSetStep(pos, 2);
            if (objStdMove(pos) != 0) {
                cursor->moving = 0;
            }
        }
    }
}

int actStockHasBoost(void);
void menuUnitListDraw(MenuCursor *);
void menuCursorTargetDraw(MenuCursor *cursor);

/* Cursor draw (callback +8): parks the cursor at the home position and draws the target
 * status when actStockHasBoost is set, otherwise draws the unit list when substate is 1. */
void menuCursorDraw(MenuCursor *cursor) {
    if (actStockHasBoost() != 0) {
        cursor->slide.pos.x = MENU_CURSOR_HOME_X;
        cursor->slide.pos.y = MENU_CURSOR_HOME_Y;
        menuCursorTargetDraw(cursor);
        return;
    }
    if (cursor->kind.cursor.visible != 1) {
        return;
    }
    menuUnitListDraw(cursor);
}

/* The two 4-byte colours at D_00ABACC8, copied as one 8-byte block. */
typedef struct ColourPair {
    unsigned char bytes[8];
} ColourPair;

extern const ColourPair D_00ABACC8;

extern void func_00AA84D0(GrPacket *packet);
extern void menuKeyDraw(GrPacket *packet, int x, int y, short value, int);
extern void menuStatSpriteRGB(GrPacket *packet, int x, int y, int width, int height,
                              int srcX, int srcY, const void *colour);

/* Screen offset of each party slot in the unit list (x, y). */
typedef struct MenuUnitSlotOffset {
    int x;
    int y;
} MenuUnitSlotOffset;

extern MenuUnitSlotOffset menuUnitSlotOffsets[];
/* Frame colour of each party slot (R, G, B, A). */
extern unsigned char menuUnitSlotColours[][4];

int unitTblGet(int side, Ov01Object ***table);
extern void menuStatSprite(GrPacket *packet, int x, int y, int width, int height,
                           int srcX, int srcY);

/* Unit list at the cursor: the list frame, then per party unit its slot frame
 * and, when the unit is shown and calcBoostOk(unit, 1) allows it, its status. */
void menuUnitListDraw(MenuCursor *cursor)
{
    GrPacket packet;
    Ov01Object **table;
    Ov01Object *unit;
    int count;
    int i;
    int x;
    int y;

    grGpInit(&packet);
    x = (int) cursor->slide.pos.x;
    y = (int) cursor->slide.pos.y;
    func_00AA84D0(&packet);
    grPutSprOpen(&packet);
    menuStatSprite(&packet, x - 42, y - 14, 110, 14, 146, 0);
    grPutSprClose();
    count = unitTblGet(0, &table);
    for (i = 0; i < count; i++) {
        func_00AA84D0(&packet);
        grPutSprOpen(&packet);
        menuStatSpriteRGB(&packet, x + menuUnitSlotOffsets[i].x - 2, y + menuUnitSlotOffsets[i].y - 2, 28, 42, 227, 77,
                          menuUnitSlotColours[i]);
        grPutSprClose();
        unit = table[i];
        if (unit == 0 || (unit->flags & UNIT_FLAG_DOWN)) {
            continue;
        }
        if (calcBoostOk(unit, 1) != 0) {
            menuKeyDraw(&packet, x + menuUnitSlotOffsets[i].x, y + menuUnitSlotOffsets[i].y,
                          table[i]->stats->key, 1);
        }
    }
    grPacketSend(&packet);
}

/* Status sprite for the work unit (else the actor unit) at the cursor position. */
void menuCursorTargetDraw(MenuCursor *cursor) {
    GrPacket packet;
    ColourPair palette[2];
    Ov01Object *actor;
    int index;
    int x;
    int y;

    palette[0] = D_00ABACC8;
    if (actStockGet()->boost[0] != 0) {
        index = 1;
        actor = actStockGet()->boost[0];
    } else if (actStockGet()->boost[1] != 0) {
        index = 0;
        actor = actStockGet()->boost[1];
    } else {
        return;
    }
    x = (int) cursor->slide.pos.x;
    y = (int) cursor->slide.pos.y;
    grGpInit(&packet);
    func_00AA84D0(&packet);
    grPutSprOpen(&packet);
    menuStatSpriteRGB(&packet, x - 42, y + 26, 110, 24, 146, 14, &palette[0].bytes[index * 4]);
    grPutSprClose();
    menuKeyDraw(&packet, x,
                  ((void *) actor != tgtUnitGet()) ? y : y - 4,
                  actor->stats->key, 1);
    grPacketSend(&packet);
}

/* The menu cursor object, a common global: tu052 and tu066 name it too. */
MenuCursor *menuCursor;

/* Screen offset of each of the three party slots. */
MenuUnitSlotOffset menuUnitSlotOffsets[3] = { { -28, 0 }, { 0, 0 }, { 28, 0 } };

/* Frame colour of each party slot; the fourth row is never drawn. */
unsigned char menuUnitSlotColours[4][4] = {
    { 0xff, 0x60, 0xff, 0x80 },
    { 0x60, 0xff, 0x60, 0x80 },
    { 0xff, 0x60, 0x60, 0x80 },
    { 0, 0, 0, 0 }
};

/* Icon lists of the rows in D_00AB63C0 below: each one is a list of icon ids ended by 4 (MENU_ICON_END of
 * ov01/tu052); the trailing words are kept as they are. */
static int D_00AB6300[4] = { 1, 1, 4, 0 };
static int D_00AB6310[4] = { 1, 2, 4, 0 };
static int D_00AB6320[4] = { 2, 2, 4, 0 };
static int D_00AB6330[4] = { 2, 1, 4, 0 };
static int D_00AB6340[4] = { 129, 1, 4, 0 };
static int D_00AB6350[4] = { 129, 2, 4, 0 };
static int D_00AB6360[4] = { 130, 2, 4, 0 };
static int D_00AB6370[4] = { 130, 1, 4, 0 };
static int D_00AB6380[2] = { 0, 4 };
static int D_00AB6388[2] = { 3, 4 };
static int D_00AB6390[2] = { 0, 4 };
static int D_00AB6398[4] = { 128, 0, 4, 0 };
static int D_00AB63A8[4] = { 128, 128, 0, 4 };
static int D_00AB63B8[2] = { 1, 4 };

/* One row of a command icon table (the same type as ov01/tu052): its icon list, ended by 4, and its kind;
 * a zero kind ends a table. */
typedef struct MenuIconRow {
    int *icons;
    int kind;
} MenuIconRow;

/* The rows of the command icon tables (menuIconTablesKeyed and menuIconTablesOther of ov01/tu052) in one array;
 * the first entry has no reader. */
MenuIconRow D_00AB63C0[32] = {
    { (int *) 2, 4 }, { D_00AB6300, 3 },
    { D_00AB6310, 4 }, { D_00AB6330, 6 },
    { D_00AB6320, 5 }, { D_00AB6380, 7 },
    { D_00AB6388, 22 }, { 0, 0 },
    { D_00AB6340, 3 }, { D_00AB6350, 4 },
    { D_00AB6388, 20 }, { 0, 0 },
    { D_00AB6370, 6 }, { D_00AB6360, 5 },
    { D_00AB6388, 20 }, { 0, 0 },
    { D_00AB6380, 7 }, { D_00AB6388, 20 },
    { 0, 0 }, { D_00AB6390, 8 },
    { D_00AB6388, 20 }, { 0, 0 },
    { D_00AB6398, 9 }, { D_00AB6388, 20 },
    { 0, 0 }, { D_00AB63A8, 10 },
    { D_00AB6388, 20 }, { 0, 0 },
    { D_00AB63B8, 1 }, { (int *) D_00AB63C0, 2 },
    { D_00AB6388, 22 }, { 0, 0 }
};

/* Constant colours: the two 4-byte colours of menuCursorTargetDraw. */
const ColourPair D_00ABACC8 = { { 0x80, 0x80, 0x10, 0x80, 0xa0, 0x10, 0x10, 0x80 } };
