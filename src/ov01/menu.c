#include "common.h"
#include "shared.h"
#include "gs.h"
#include "ov01/unit_cmd.h"
#include "ov01/unit_tbl.h"
#include "ov01/choice_menu.h"
#include "ov01/scene.h"
#include "ov01/gr_gp_init.h"
#include "ov01/gr_matrix.h"
#include "ov01/entry_first_init.h"
#include "ov01/screen_fill.h"
#include "ov01/menu.h"
#include "ov01/menu_panel.h"
#include "ov01/menu_tex_state.h"
#include "ov01/obj_slot.h"
#include "ov01/calc.h"
#include "ov01/battle_init.h"
#include "main/sef.h"

/* Menu panel rows: MENU_PANEL_ROW_COUNT slots, a slot holds the row's y
 * (MENU_PANEL_ROW_Y0 + MENU_PANEL_ROW_STEP * index) while the row is taken, 0
 * when free. menuPanelRowAlloc takes one, menuPanelRowFree frees it. */
#define MENU_PANEL_ROW_Y0 100
#define MENU_PANEL_ROW_STEP 20

void *memset(void *, int, unsigned int);

void menuPanelRowsReset(void)
{
    memset(menuPanelRows, 0, sizeof(menuPanelRows));
    screenFadeTask = 0;
    screenFillTask = 0;
}

/* Takes the first free row and returns its y, or 0 when all rows are taken. */
int menuPanelRowAlloc(void)
{
    int i;
    int y;

    for (i = 0, y = MENU_PANEL_ROW_Y0; i < MENU_PANEL_ROW_COUNT; i++) {
        if (menuPanelRows[i] == 0) {
            menuPanelRows[i] = y;
            return y;
        }
        y += MENU_PANEL_ROW_STEP;
    }
    return 0;
}

/* Frees the row taken at y; returns 1, or 0 when no row has that y. */
int menuPanelRowFree(int y)
{
    int i;

    for (i = 0; i < MENU_PANEL_ROW_COUNT; i++) {
        if (menuPanelRows[i] == y) {
            menuPanelRows[i] = 0;
            return 1;
        }
    }
    return 0;
}

/* A menu object from objSlotAlloc: +0x04 a state word, +0x08 the draw callback
 * (the draw slot of the other objects, see arrowCreate). */
struct MenuTexFlushTask {
    char unmodeled_000[4];
    int state;                  /* +0x04 */
    void (*draw)(void);         /* +0x08 */
};

void menuStatPreObjDraw(void);
static struct MenuTexFlushTask *menuTexFlushTask;

/* Starts the texture-flush task: the object's draw is menuStatPreObjDraw, and the
 * object is kept in menuTexFlushTask (nothing else in the overlay reads it). */
void menuStatPreObjCreate(void)
{
    struct MenuTexFlushTask *task;

    task = objSlotAlloc();
    if (task != 0) {
        menuTexFlushTask = task;
        task->draw = menuStatPreObjDraw;
        task->state = 0;
    }
}

/* Draw callback of the texture-flush task: flushes the menu texture. */
void menuStatPreObjDraw(void)
{
    menuTexFlush();
}

/* The stat records menuStatRecordsCreate made, one per slot of the unit table (0 for an empty slot). */
MenuObject *menuStatRecords[4];

void menuStatObj(MenuObject *record);
void menuStatObjDraw(MenuObject *record);

/* Origin of the first stat record and the distance between two records. */
#define MENU_STAT_X 16
#define MENU_STAT_STEP_X 167
#define MENU_STAT_Y 12.0f

/*
 * Creates one stat record (an objSlotAlloc menu object stepped by menuStatObj and drawn by
 * menuStatObjDraw) per slot of the player unit table, side by side from MENU_STAT_X; an empty slot gets 0.
 * Stops when no object is free.
 */
void menuStatRecordsCreate(void)
{
    BattleUnit **units;
    MenuObject *record;
    int count;
    int i;
    int x;

    x = MENU_STAT_X;
    count = unitTblGet(UNIT_SIDE_PLAYER, &units);
    for (i = 0; i < count; i++) {
        if (units[i] == 0) {
            menuStatRecords[i] = 0;
            continue;
        }
        record = objSlotAlloc();
        if (record == 0) {
            return;
        }
        record->update = menuStatObj;
        record->draw = menuStatObjDraw;
        menuStatRecords[i] = record;
        objStdInit(&record->slide.block);
        record->slide.pos.x = x;
        x += MENU_STAT_STEP_X;
        record->slide.pos.y = MENU_STAT_Y;
        record->slide.pos.z = 0.0f;
        record->kind.stat.unit = units[i];
        record->moving = 0;
        record->kind.stat.shown[0] = 0.0f;
        record->kind.stat.shown[1] = 0.0f;
        record->kind.stat.shown[2] = 0.0f;
    }
}

/* Step of a stat record: eases three displayed floats (+0x98, +0x9C, +0xA0) toward
 * the unit's stat fields (at +0x94 -> +0x144), snapping within 0.5 and clamped. */
/* Shown values closer than this to their unit value snap to it. */
#define MENU_STAT_SNAP 0.5f
/* Fraction of the remaining distance a shown value moves per step: the HP, EP and current gauges, and the faster
 * level pips. */
#define MENU_STAT_EASE_RATE 0.125f
#define MENU_STAT_LEVEL_EASE_RATE 0.25f
/* Highest level a gauge shows: the cap unitLevelAdd clamps StatBlock.level to (UNIT_LEVEL_MAX, calc.c). */
#define MENU_STAT_LEVEL_MAX 3.0f

/* The negation of the distance as its own inline call (codegen: a plain -delta_ in the ternary does not give
 * the original's compare chain). */
static inline float menuStatNeg(float v)
{
    return -v;
}

/* Moves a shown gauge value a RATE fraction towards its unit value, or snaps it there when it is close. A plain
 * block, not do/while(0): the loop note of a do/while is a scheduling barrier and keeps the shown[0] load behind
 * the stats load. */
#define MENU_STAT_EASE(shown, value, rate)                                     \
    {                                                                          \
        float current_ = (shown);                                              \
        float delta_ = (value) - current_;                                     \
        if ((delta_ < 0.0f ? menuStatNeg(delta_) : delta_) < MENU_STAT_SNAP) { \
            (shown) = (value);                                                 \
        } else {                                                               \
            (shown) = current_ + ((value) - current_) * (rate);                \
        }                                                                      \
    }

/* Update of a stat record: unless it is sliding, eases the shown HP, EP and third gauge towards the unit's values
 * and caps them at their maxima (the third gauge at MENU_STAT_LEVEL_MAX for party keys, which show the level). */
void menuStatObj(MenuObject *record)
{
    StatBlock *stats;
    float *hpShown;
    float *epShown;
    float *thirdShown;

    if (record->moving != 0) {
        return;
    }
    hpShown = &record->kind.stat.shown[0];
    thirdShown = &record->kind.stat.shown[2];
    epShown = &record->kind.stat.shown[1];
    stats = record->kind.stat.unit->stats;
    MENU_STAT_EASE(*hpShown, stats->hp, MENU_STAT_EASE_RATE);
    if (stats->maxHp < *hpShown) {
        *hpShown = stats->maxHp;
    }
    MENU_STAT_EASE(*epShown, stats->ep, MENU_STAT_EASE_RATE);
    if (stats->maxEp < *epShown) {
        *epShown = stats->maxEp;
    }
    if ((unsigned int)(stats->keyUnsigned - UNIT_KEY_PARTY_FIRST) < UNIT_KEY_PARTY_COUNT) {
        MENU_STAT_EASE(*thirdShown, stats->level, MENU_STAT_LEVEL_EASE_RATE);
        if (*thirdShown > MENU_STAT_LEVEL_MAX) {
            *thirdShown = MENU_STAT_LEVEL_MAX;
        }
    } else {
        MENU_STAT_EASE(*thirdShown, stats->current, MENU_STAT_EASE_RATE);
        if (*thirdShown > stats->currentMax) {
            *thirdShown = stats->currentMax;
        }
    }
}

void menuPlStatPut(MenuObject *record, GrPacket *packet);

/* Draw callback of a stat record (MenuObject.draw): menuPlStatPut reads its origin (slide) and its unit. */
void menuStatObjDraw(MenuObject *record)
{
    GrPacket packet;

    grGpInit(&packet);
    menuPlStatPut(record, &packet);
    grPacketSend(&packet);
}

/* Pulse of the frame: the doubled frame counter as an 8-bit phase, mirrored in its upper half (a triangle wave),
 * added to the lowest fixed alpha. */
#define MENU_STAT_PULSE_PHASE_MASK 0xFF
#define MENU_STAT_PULSE_HALF 0x80
#define MENU_STAT_PULSE_ALPHA_MIN 10
/* ALPHA_1 blends of the two pulse passes, the fixed xglFontCmdAlpha (FIX) in the high word: (Cs - 0) * FIX + Cs, then
 * (Cs - 0) * FIX + Cd. */
#define MENU_STAT_PULSE_BLEND_ADD_SRC GS_SET_ALPHA(0, 2, 2, 0, 0)
#define MENU_STAT_PULSE_BLEND_ADD_DST GS_SET_ALPHA(0, 2, 2, 1, 0)
/* Full widths of the HP and EP bars and of the current gauge: the fills are scaled from shown / max to them. */
#define MENU_STAT_BAR_WIDTH 0x58
#define MENU_STAT_CURRENT_BAR_WIDTH 0x4A

/*
 * Draws one stat record at its origin (slide position): a pulsing frame while its unit is the one under the battle
 * cursor, the unit's face (menuKeyDraw), the HP and EP gauges (shown[0], shown[1] against maxHp and maxEp), then
 * for a party key the level pips (shown[2], up to MENU_STAT_LEVEL_MAX) or else the current gauge and number (shown[2]
 * against currentMax), and the HP and EP numbers.
 */
void menuPlStatPut(MenuObject *record, GrPacket *packet)
{
    BattleUnit *unit;
    unsigned long long alpha;
    int phase;
    int x;
    int y;
    int value;

    x = record->slide.pos.x;
    y = record->slide.pos.y;
    unit = record->kind.stat.unit;
    if (unit == manWorkGet()->current) {
        menuHighlightTexState(packet);
        phase = (ov01SceneFrameCount() * 2) & MENU_STAT_PULSE_PHASE_MASK;
        if (phase >= MENU_STAT_PULSE_HALF) {
            phase = MENU_STAT_PULSE_PHASE_MASK - phase;
        }
        /* GS ALPHA_1 with a fixed xglFontCmdAlpha (high word) that rises and falls with the frame counter, as in
         * menuPulseAlphaSet (menu_wep_sp_panel.c) but from MENU_STAT_PULSE_ALPHA_MIN: a 0x91 x 0xA sprite at y + 0x38,
         * then the frame around the record */
        alpha = (unsigned long long)(phase + MENU_STAT_PULSE_ALPHA_MIN) << 32;
        grGsRegSet(packet, GS_ALPHA_1, alpha | MENU_STAT_PULSE_BLEND_ADD_SRC);
        grOpenSpr(packet);
        menuStatSprite(packet, x, y + 0x38, 0x91, 0xA, 0, 0x41);
        grCloseSpr();
        grGsRegSet(packet, GS_ALPHA_1, alpha | MENU_STAT_PULSE_BLEND_ADD_DST);
        grOpenSpr(packet);
        menuStatSprite2(packet, x - 4, y - 4, 0x9E, 0x3B, 0xC8, 0x78, 0xFF, 0xBA);
        grCloseSpr();
    }
    menuKeyDraw(packet, x, y, unit->stats->key, 0);
    menuGaugeTexState(packet);
    /* HP and EP gauges: each a bar track (MENU_STAT_BAR_WIDTH wide) and its fill scaled to the shown value. */
    grOpenSpr(packet);
    menuStatSprite(packet, x + 0x3A, y + 0xA, MENU_STAT_BAR_WIDTH, 8, 0x48, 0x8A);
    value = record->kind.stat.shown[0];
    menuStatSprite2(packet, x + 0x3A, y + 8, value * MENU_STAT_BAR_WIDTH / unit->stats->maxHp, 9, 0x48, 0x78, 0xA0,
                    0x80);
    menuStatSprite(packet, x + 0x3A, y + 0x20, MENU_STAT_BAR_WIDTH, 8, 0x48, 0x8A);
    value = record->kind.stat.shown[1];
    if (unit->stats->maxEp <= 0) {
        value = 0;
    } else {
        value = value * MENU_STAT_BAR_WIDTH / unit->stats->maxEp;
    }
    menuStatSprite2(packet, x + 0x3A, y + 0x1E, value, 9, 0x48, 0x81, 0xA0, 0x89);
    grCloseSpr();
    if ((unsigned int)(unit->stats->keyUnsigned - UNIT_KEY_PARTY_FIRST) < UNIT_KEY_PARTY_COUNT) {
        /* Level pips: one pip per shown level (texture row 0x81), then the three pip cells (row 0x78). */
        grOpenSpr(packet);
        value = record->kind.stat.shown[2];
        if (value > 0) {
            menuStatSprite(packet, x + 0x48, y + 0x2A, 0x1A, 9, 0xAE, 0x81);
        }
        if (value >= 2) {
            menuStatSprite(packet, x + 0x60, y + 0x2A, 0x1A, 9, 0xAE, 0x81);
        }
        if (value >= 3) {
            menuStatSprite(packet, x + 0x78, y + 0x2A, 0x1A, 9, 0xAE, 0x81);
        }
        menuStatSprite(packet, x + 0x48, y + 0x2A, 0x1A, 9, 0xAE, 0x78);
        menuStatSprite(packet, x + 0x60, y + 0x2A, 0x1A, 9, 0xAE, 0x78);
        menuStatSprite(packet, x + 0x78, y + 0x2A, 0x1A, 9, 0xAE, 0x78);
        grCloseSpr();
    } else {
        /* Current gauge: its track (MENU_STAT_CURRENT_BAR_WIDTH wide) and fill, then the shown value. */
        grOpenSpr(packet);
        value = record->kind.stat.shown[2];
        if (unit->stats->currentMax <= 0) {
            value = 0;
        } else {
            value = value * MENU_STAT_CURRENT_BAR_WIDTH / unit->stats->currentMax;
        }
        menuStatSprite(packet, x + 0x48, y + 0x30, MENU_STAT_CURRENT_BAR_WIDTH, 5, 0x48, 0x98);
        menuStatSprite(packet, x + 0x48, y + 0x2F, value, 5, 0x48, 0x93);
        grCloseSpr();
        menuNumDraw(packet, x + 0x74, y + 0x26, record->kind.stat.shown[2], 3, 2);
    }
    menuNumTexState(packet);
    /* Icons at the HP and EP rows and at the third gauge (a different one for party keys), then the HP number (4
     * digits for party keys, 5 otherwise) and the EP number. */
    grOpenSpr(packet);
    menuStatSprite(packet, x + 0x3C, y - 6, 0x12, 0x10, 0x6B, 0);
    menuStatSprite(packet, x + 0x3C, y + 0x10, 0x12, 0x10, 0x7D, 0);
    if ((unsigned int)(unit->stats->keyUnsigned - UNIT_KEY_PARTY_FIRST) < UNIT_KEY_PARTY_COUNT) {
        menuStatSprite(packet, x + 0x38, y + 0x28, 0x10, 0xE, 0x64, 0x32);
    } else {
        menuStatSprite(packet, x + 0x38, y + 0x28, 0x10, 0xE, 0x54, 0x32);
    }
    grCloseSpr();
    value = record->kind.stat.shown[0];
    if ((unsigned int)(unit->stats->keyUnsigned - UNIT_KEY_PARTY_FIRST) < UNIT_KEY_PARTY_COUNT) {
        menuNumDrawTarget(packet, x + 0x64, y - 8, value, 4, 0, unit);
    } else {
        menuNumDrawTarget(packet, x + 0x58, y - 8, value, 5, 0, unit);
    }
    menuNumDrawTargetEp(packet, x + 0x7B, y + 0xE, record->kind.stat.shown[1], 2, 0, unit);
}

/* Screen points of the five path objects (x, y). */
typedef struct ObjPathPoint {
    int x;
    int y;
} ObjPathPoint;

/* The last path object is the extra one menuStockBoost sends to point 0. */
#define OBJ_PATH_MOVING 4

ObjPathPoint stockPosTbl[OBJ_PATH_COUNT] = {
    { 376, 390 }, { 404, 390 }, { 432, 390 }, { 460, 390 }, { 288, 390 },
};
PathObject *pStockObj[OBJ_PATH_COUNT];

/* ActStock.pickSource value that shows the moving path object instead of point 0. */
#define PATH_STOCK_SOURCE_MOVING 2


void menuStockObj(PathObject *object);
void menuStockObjDraw(PathObject *object);

/* Allocates the five path objects on their points: slot and point index i, fade
 * staggered by -12, stopped; the last one starts hidden. */
void menuStockObjCreate(void)
{
    PathObject *object;
    int i;

    for (i = 0; i < OBJ_PATH_COUNT; i++) {
        object = objSlotAlloc();
        if (object == 0) {
            return;
        }
        object->update = menuStockObj;
        object->draw = menuStockObjDraw;
        pStockObj[i] = object;
        objStdInit(&object->slide.block);
        object->slide.pos.x = stockPosTbl[i].x;
        object->slide.pos.y = stockPosTbl[i].y;
        object->slide.pos.z = 0;
        object->kind.path.slot = i;
        object->kind.path.index = i;
        object->kind.path.fade = i * -12;
        object->moving = 0;
        if (i >= OBJ_PATH_MOVING) {
            object->kind.path.shown = 0;
        }
    }
}

/* Per-frame update (callback +4): while the mover is active it steps toward its target,
 * clears state when objStdMove reports arrival, and advances the counter to 0x18. */
void menuStockObj(PathObject *object) {
    int state;
    SlideBlock *target;
    int counter;

    state = object->moving;
    if (state != 0) {
        if (state >= 0) {
            if (state < 3) {
                target = &object->slide.block;
                objHatoVec(target, 2);
                if (objStdMove(target) != 0) {
                    object->moving = 0;
                }
            }
        }
    }
    counter = 0x18;
    if (object->kind.path.fade < 0x18) {
        counter = object->kind.path.fade + 4;
    }
    object->kind.path.fade = counter;
}

/* Draw callback (+8) of a path object: draws the stock count of the unit on the
 * object's point while the object is shown, its point is within the stock and
 * the unit is active; the targeted unit is drawn 4 pixels higher. */
void menuStockObjDraw(PathObject *object)
{
    struct ActStock *stock;
    BattleUnit *unit;
    GrPacket packet;
    int slot;
    int index;
    int left;
    int x;
    int y;

    if (object->kind.path.shown == 0) {
        return;
    }
    stock = actStockGet();
    slot = object->kind.path.slot;
    index = object->kind.path.index;
    left = stock->max - stock->pickCursor + 1;
    if (stock->max == 0) {
        return;
    }
    if (index != OBJ_PATH_MOVING && !(index < left)) {
        return;
    }
    if (index == 0 && stock->pickSource == PATH_STOCK_SOURCE_MOVING) {
        return;
    }
    if (index == OBJ_PATH_MOVING && stock->pickSource != PATH_STOCK_SOURCE_MOVING) {
        return;
    }
    if (object->kind.path.fade <= 0) {
        return;
    }
    if (index == OBJ_PATH_MOVING) {
        unit = stock->lastPicked;
    } else {
        unit = stock->stock[slot];
    }
    if (unit == 0) {
        return;
    }
    if (unit->flags & UNIT_FLAG_DOWN) {
        return;
    }
    if (!(unit->slotFlags & OV01_OBJECT_IN_USE)) {
        return;
    }
    y = object->slide.pos.y;
    x = object->slide.pos.x;
    if (unit == tgtUnitGet()) {
        y -= 4;
    }
    grGpInit(&packet);
    menuKeyCellDraw(&packet, x, y, unit->stats->key, 1, object->kind.path.fade);
    grPacketSend(&packet);
}

/* Puts every path object back on its own point, stopped, its fade restarted (staggered by -12 per object); all
 * but the last are shown. */
void menuStockStart(void)
{
    PathObject *object;
    int i;

    for (i = 0; i < OBJ_PATH_COUNT; i++) {
        object = pStockObj[i];
        object->kind.path.index = i;
        object->kind.path.fade = i * -12;
        object->moving = 0;
        object->slide.pos.x = stockPosTbl[i].x;
        object->slide.pos.y = stockPosTbl[i].y;
        object->slide.pos.z = 0;
        if (i < OBJ_PATH_MOVING) {
            object->kind.path.shown = 1;
        } else {
            object->kind.path.shown = 0;
        }
    }
}

/* Moves each of the first four path objects one point back (to index - 1);
 * one already at point 0 is hidden instead. */
void menuStockNext(void)
{
    PathObject *object;
    int i;
    int index;

    for (i = 0; i < OBJ_PATH_MOVING; i++) {
        object = pStockObj[i];
        index = object->kind.path.index;
        if (index <= 0) {
            object->kind.path.index = 0;
            object->kind.path.shown = 0;
        } else {
            index--;
            object->moving = 1;
            object->kind.path.index = index;
            object->slide.pos.target_x = stockPosTbl[index].x;
            object->slide.pos.target_y = stockPosTbl[index].y;
            object->slide.pos.target_z = 0;
        }
    }
}

/* Sends the last path object from point 4 towards point 0 and shows it. */
void menuStockBoost(void)
{
    PathObject *object = pStockObj[OBJ_PATH_MOVING];

    object->moving = 2;
    object->slide.pos.target_x = stockPosTbl[0].x;
    object->slide.pos.target_y = stockPosTbl[0].y;
    object->slide.pos.target_z = 0;
    object->slide.pos.x = stockPosTbl[OBJ_PATH_MOVING].x;
    object->slide.pos.y = stockPosTbl[OBJ_PATH_MOVING].y;
    object->slide.pos.z = 0;
    object->kind.path.shown = 1;
}

/* After the action of slot is used: when its stock entry is empty, the path
 * objects after it (up to the moving one) each move one point back. */
void menuStockDead(int slot)
{
    int i;
    int index;
    PathObject *object;

    if (actStockGet()->stock[pStockObj[slot]->kind.path.slot] != 0) {
        return;
    }
    for (i = slot + 1; i < OBJ_PATH_MOVING; i++) {
        object = pStockObj[i];
        index = object->kind.path.index;
        if (index > 0) {
            index--;
            object->moving = 1;
            object->kind.path.index = index;
            object->slide.pos.target_x = stockPosTbl[index].x;
            object->slide.pos.target_y = stockPosTbl[index].y;
            object->slide.pos.target_z = 0;
        }
    }
}
void menuBoostObj(MenuCursor *cursor);
void menuBoostObjDraw(MenuCursor *cursor);

/* Creates the menu cursor: update and draw callbacks, a cleared slide block
 * with the cursor parked below the home position, not moving and hidden. */
void menuBoostObjCreate(void)
{
    MenuCursor *cursor;

    cursor = objSlotAlloc();
    if (cursor != 0) {
        cursor->update = menuBoostObj;
        cursor->draw = menuBoostObjDraw;
        menuCursor = cursor;
        objStdInit(&cursor->slide.block);
        cursor->slide.pos.x = MENU_CURSOR_HOME_X;
        cursor->slide.pos.y = MENU_CURSOR_PARKED_Y;
        cursor->slide.pos.z = 0;
        cursor->moving = 0;
        cursor->kind.cursor.visible = 0;
    }
}

/* Cursor update (callback +4 of menuCursor): state 1 steps the cursor position and clears
 * state on arrival. */
void menuBoostObj(MenuCursor *cursor) {
    SlideBlock *pos;

    if (cursor->moving != 0) {
        if (cursor->moving == 1) {
            pos = &cursor->slide.block;
            objHatoVec(pos, 2);
            if (objStdMove(pos) != 0) {
                cursor->moving = 0;
            }
        }
    }
}

void menuBoostObjDrawSel(MenuCursor *);
void menuBoostObjDrawCha(MenuCursor *cursor);

/* Cursor draw (callback +8): parks the cursor at the home position and draws the target
 * status when actStockHasBoost is set, otherwise draws the unit list when substate is 1. */
void menuBoostObjDraw(MenuCursor *cursor) {
    if (actStockHasBoost() != 0) {
        cursor->slide.pos.x = MENU_CURSOR_HOME_X;
        cursor->slide.pos.y = MENU_CURSOR_HOME_Y;
        menuBoostObjDrawCha(cursor);
        return;
    }
    if (cursor->kind.cursor.visible != 1) {
        return;
    }
    menuBoostObjDrawSel(cursor);
}

extern const ColourPair menuBoostPaletteColors;


/* Screen offset of each party slot in the unit list (x, y). */
typedef struct MenuUnitSlotOffset {
    int x;
    int y;
} MenuUnitSlotOffset;

extern MenuUnitSlotOffset menuUnitSlotOffsets[];
/* Frame colour of each party slot (R, G, B, A). */
extern MenuRGBA menuUnitSlotColours[];

/* Unit list at the cursor: the list frame, then per party unit its slot frame
 * and, when the unit is shown and calcBoostChk(unit, 1) allows it, its status. */
void menuBoostObjDrawSel(MenuCursor *cursor)
{
    GrPacket packet;
    BattleUnit **table;
    BattleUnit *unit;
    int count;
    int i;
    int x;
    int y;

    grGpInit(&packet);
    x = (int) cursor->slide.pos.x;
    y = (int) cursor->slide.pos.y;
    menuUnitSlotTexState(&packet);
    grOpenSpr(&packet);
    menuStatSprite(&packet, x - 42, y - 14, 110, 14, 146, 0);
    grCloseSpr();
    count = unitTblGet(0, &table);
    for (i = 0; i < count; i++) {
        menuUnitSlotTexState(&packet);
        grOpenSpr(&packet);
        menuStatSpriteRGB(&packet, x + menuUnitSlotOffsets[i].x - 2, y + menuUnitSlotOffsets[i].y - 2, 28, 42, 227, 77,
                          &menuUnitSlotColours[i]);
        grCloseSpr();
        unit = table[i];
        if (unit == 0 || (unit->flags & UNIT_FLAG_DOWN)) {
            continue;
        }
        if (calcBoostChk(unit, 1) != 0) {
            menuKeyDraw(&packet, x + menuUnitSlotOffsets[i].x, y + menuUnitSlotOffsets[i].y,
                          table[i]->stats->key, 1);
        }
    }
    grPacketSend(&packet);
}

/* Status sprite for the work unit (else the actor unit) at the cursor position. */
void menuBoostObjDrawCha(MenuCursor *cursor) {
    GrPacket packet;
    ColourPair palette[2];
    BattleUnit *actor;
    int index;
    int x;
    int y;

    palette[0] = menuBoostPaletteColors;
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
    menuUnitSlotTexState(&packet);
    grOpenSpr(&packet);
    menuStatSpriteRGB(&packet, x - 42, y + 26, 110, 24, 146, 14, &palette[0].colours[index]);
    grCloseSpr();
    menuKeyDraw(&packet, x,
                  ((void *) actor != tgtUnitGet()) ? y : y - 4,
                  actor->stats->key, 1);
    grPacketSend(&packet);
}

/* The menu cursor object, a common global: deco_flags.c names it too. */
MenuCursor *menuCursor;

/* Screen offset of each of the three party slots. */
MenuUnitSlotOffset menuUnitSlotOffsets[3] = { { -28, 0 }, { 0, 0 }, { 28, 0 } };

/* Frame colour of each party slot; the fourth row is never drawn. */
MenuRGBA menuUnitSlotColours[4] = {
    { 0xff, 0x60, 0xff, 0x80 },
    { 0x60, 0xff, 0x60, 0x80 },
    { 0xff, 0x60, 0x60, 0x80 },
    { 0, 0, 0, 0 }
};

/* Icon lists of the rows in menuIconRowTable below: each one is a list of icon ids ended by 4 (MENU_ICON_END of
 * ov01/menu.c); the trailing words are kept as they are. */
static int menuIconsOneOne[4] = { 1, 1, 4, 0 };
static int menuIconsOneTwo[4] = { 1, 2, 4, 0 };
static int menuIconsTwoTwo[4] = { 2, 2, 4, 0 };
static int menuIconsTwoOne[4] = { 2, 1, 4, 0 };
static int menuIconsAltOneOne[4] = { 129, 1, 4, 0 };
static int menuIconsAltOneTwo[4] = { 129, 2, 4, 0 };
static int menuIconsAltTwoTwo[4] = { 130, 2, 4, 0 };
static int menuIconsAltTwoOne[4] = { 130, 1, 4, 0 };
static int menuIconsZeroPlain[2] = { 0, 4 };
static int menuIconsThree[2] = { 3, 4 };
static int menuIconsZeroInline[2] = { 0, 4 };
static int menuIconsAltZero[4] = { 128, 0, 4, 0 };
static int menuIconsAltAltZero[4] = { 128, 128, 0, 4 };
static int menuIconsOne[2] = { 1, 4 };

/* One row of a command icon table (the same type as ov01/menu.c): its icon list, ended by 4, and its kind;
 * a zero kind ends a table. */
typedef struct MenuIconRow {
    int *icons;
    int kind;
} MenuIconRow;

/* The rows of the command icon tables (menuIconTablesKeyed and menuIconTablesOther of ov01/menu.c) in one array;
 * the first entry has no reader. */
MenuIconRow menuIconRowTable[32] = {
    { (int *) 2, 4 }, { menuIconsOneOne, 3 },
    { menuIconsOneTwo, 4 }, { menuIconsTwoOne, 6 },
    { menuIconsTwoTwo, 5 }, { menuIconsZeroPlain, 7 },
    { menuIconsThree, 22 }, { 0, 0 },
    { menuIconsAltOneOne, 3 }, { menuIconsAltOneTwo, 4 },
    { menuIconsThree, 20 }, { 0, 0 },
    { menuIconsAltTwoOne, 6 }, { menuIconsAltTwoTwo, 5 },
    { menuIconsThree, 20 }, { 0, 0 },
    { menuIconsZeroPlain, 7 }, { menuIconsThree, 20 },
    { 0, 0 }, { menuIconsZeroInline, 8 },
    { menuIconsThree, 20 }, { 0, 0 },
    { menuIconsAltZero, 9 }, { menuIconsThree, 20 },
    { 0, 0 }, { menuIconsAltAltZero, 10 },
    { menuIconsThree, 20 }, { 0, 0 },
    { menuIconsOne, 1 }, { (int *) menuIconRowTable, 2 },
    { menuIconsThree, 22 }, { 0, 0 }
};

/* Constant colours: the two 4-byte colours of menuBoostObjDrawCha. */
const ColourPair menuBoostPaletteColors = { { { 0x80, 0x80, 0x10, 0x80 }, { 0xa0, 0x10, 0x10, 0x80 } } };

/* Show the cursor: enabled, and when actStockHasBoost is clear, ready at the home position. */
void menuBoostDispOn(void)
{
    menuCursor->kind.cursor.visible = 1;
    if (actStockHasBoost() == 0) {
        menuCursor->moving = 1;
        menuCursor->slide.pos.target_x = MENU_CURSOR_HOME_X;
        menuCursor->slide.pos.target_y = MENU_CURSOR_HOME_Y;
    }
}

/* Hide the cursor: disabled and parked at (288, 454). */
void menuBoostDispOff(void)
{
    menuCursor->slide.pos.x = MENU_CURSOR_HOME_X;
    menuCursor->slide.pos.y = MENU_CURSOR_PARKED_Y;
    menuCursor->kind.cursor.visible = 0;
}

/* Position of the icon list. */
#define MENU_ICON_LIST_X 8.0f
#define MENU_ICON_LIST_Y 312.0f

void menuKeyGuideObj(MenuObject *list);
void menuKeyGuideDisp(MenuObject *list);

/* Creates the icon list object: no update step (menuKeyGuideObj is empty), drawn
 * by menuKeyGuideDisp, with a cleared slide block at the list position. */
void menuKeyGuideObjCreate(void)
{
    MenuObject *list;

    list = objSlotAlloc();
    if (list != 0) {
        list->update = menuKeyGuideObj;
        list->draw = menuKeyGuideDisp;
        menuIconListObj = list;
        objStdInit(&list->slide.block);
        list->slide.pos.x = MENU_ICON_LIST_X;
        list->slide.pos.y = MENU_ICON_LIST_Y;
        list->slide.pos.z = 0;
        list->moving = 0;
    }
}

/* Update step of the icon list: nothing to do. */
void menuKeyGuideObj(MenuObject *list)
{
}

#define MENU_ICON_END 4
/* Icon bit that draws the icon from the second atlas row whatever the key state. */
#define MENU_ICON_ALT_ROW 0x80
/* Row kinds drawn without a key check: the status row gets menuStatusIconPut. */
#define MENU_ICON_ROW_PLAIN 0x14
#define MENU_ICON_ROW_STATUS 0x16
/* keyCmdChk results: 0 and 1 draw the icons (1 from the second atlas row), 2 skips the row. */
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
 * (state from keyCmdChk), then its status icons. Nothing while the battle state is not active or the unit has
 * script events. */
void menuKeyGuideDisp(MenuObject *list)
{
    GrPacket packet;
    BattleUnit *unit;
    BattleUnit *target;
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

    unit = manWorkGet()->current;
    if (manWorkGet()->active != 1) {
        return;
    }
    if (unit->flags & UNIT_FLAG_SCRIPT_EVENTS) {
        return;
    }
    target = tgtUnitGet();
    page = manWorkGet()->selectionKind;
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
                state = keyCmdChk(unit, rows[r].kind, target);
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
                        keyGideDisp(&packet, x, y, icon & ~MENU_ICON_ALT_ROW, cellRow);
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

/* The icon list object, a common global: deco_flags.c names it too. */
MenuObject *menuIconListObj;

MenuIconRow *menuIconTablesKeyed[12] = {
    &menuIconRowTable[1], &menuIconRowTable[8], &menuIconRowTable[12], &menuIconRowTable[16],
    &menuIconRowTable[16], &menuIconRowTable[16], &menuIconRowTable[16], &menuIconRowTable[19],
    &menuIconRowTable[22], &menuIconRowTable[25], 0, 0
};
MenuIconRow *menuIconTablesOther[4] = { &menuIconRowTable[28], 0, 0, 0 };

/* The boost gauge bar is MENU_VALUE_BAR_WIDTH pixels wide; a displayed gauge of MENU_VALUE_GAUGE_FULL fills it. */
#define MENU_VALUE_BAR_WIDTH 0x74
#define MENU_VALUE_GAUGE_FULL 50
/* GS_ALPHA_1, the GS register number of the ALPHA_1 state (include/gs.h names it the same way). */
#define GS_ALPHA_1 0x42

/* Slides each displayed gauge toward its boost gauge while counter is zero: within half a unit it snaps to the target, otherwise it closes an eighth of the distance. */
void menuBoostGaugeStep(struct MenuValuePanel *panel);
/* Draws both boost gauges at the panel position: each as a sprite bar filled to its gauge value, with its stock count beside it; a full gauge flashes with the pulse. */
void menuBoostGaugeDraw(struct MenuValuePanel *panel);

/* Creates the panel in the slot objSlotAlloc returns, with both callbacks, its position and zeroed slide state. */
void menuBoostGaugeCreate(void)
{
    struct MenuValuePanel *panel;

    panel = objSlotAlloc();
    if (panel != 0) {
        panel->update = menuBoostGaugeStep;
        panel->draw = menuBoostGaugeDraw;
        menuValuePanelObj = panel;
        objStdInit(&panel->slide);
        panel->slide.position.x = 376.0f;
        panel->slide.position.y = 404.0f;
        panel->gauge[1] = 0.0f;
        do {
            panel->slide.position.z = 0;
        } while (0);
        do {
            panel->counter = 0;
        } while (0);
        panel->gauge[0] = 0.0f;
    }
}

/* Slides each displayed gauge toward its boost gauge while counter is zero: within half a unit it snaps to the target, otherwise it closes an eighth of the distance. */
void menuBoostGaugeStep(struct MenuValuePanel *panel)
{
    float *displayed;
    float distance;
    unsigned char targetRaw;
    float absDistance;

    if (panel->counter != 0) {
        return;
    }
    {
        struct BoostGauge *boost = boostGaugeSelect(0);

        displayed = &panel->gauge[0];
        distance = boost->progress - *displayed;
        targetRaw = boost->progress;
        if ((distance < 0.0f) ? ((absDistance = -distance) < 0.5f) : (distance < 0.5f)) {
            *displayed = (signed char)targetRaw;
        } else {
            *displayed = *displayed + ((signed char)targetRaw - *displayed) * 0.125f;
        }
    }
    {
        struct BoostGauge *boost = boostGaugeSelect(1);

        displayed = &panel->gauge[1];
        distance = boost->progress - *displayed;
        targetRaw = boost->progress;
        if ((distance < 0.0f) ? ((absDistance = -distance) < 0.5f) : (distance < 0.5f)) {
            *displayed = (signed char)targetRaw;
        } else {
            *displayed = *displayed + ((signed char)targetRaw - *displayed) * 0.125f;
        }
    }
}


/* Draws both boost gauges at the panel position: each as a sprite bar filled to its gauge value, with its stock count beside it; a full gauge flashes with the pulse. */
void menuBoostGaugeDraw(struct MenuValuePanel *panel)
{
    GrPacket packet;
    struct BoostGauge *boost;
    int x;
    int y;
    int flashAlpha;
    int fillWidth0;
    int fillWidth1;
    int shownValue; /* the gauge value, then the bar width the full check compares with, then the stock count */
    int numberX;

    grGpInit(&packet);
    menuGaugeTexState(&packet);
    x = panel->slide.position.x;
    y = panel->slide.position.y;
    /* the flash fades the ALPHA_1 FIX byte (bits 32..39) in a triangle: 8 steps per frame */
    flashAlpha = (ov01SceneFrameCount() * 8) & 0xFF;
    if (flashAlpha >= 0x80) {
        flashAlpha = 0xFF - flashAlpha;
    }
    flashAlpha += 0xA;

    boost = boostGaugeSelect(0);
    shownValue = panel->gauge[0];
    fillWidth0 = shownValue * MENU_VALUE_BAR_WIDTH / MENU_VALUE_GAUGE_FULL;
    grGsRegSet(&packet, GS_ALPHA_1, 0x44);
    grOpenSpr(&packet);
    menuStatSprite(&packet, x, y - 0x18, MENU_VALUE_BAR_WIDTH, 9, 0x48, 0xB0);
    menuStatSprite(&packet, x, y - 0x21, 0x2E, 8, 0x48, 0x9D);
    grCloseSpr();
    {
        shownValue = fillWidth0;
        if (boost->stock >= boost->maxStock && shownValue == MENU_VALUE_BAR_WIDTH) {
            grGsRegSet(&packet, GS_ALPHA_1, ((u64)flashAlpha << 32) | 0x68);
        }
    }
    grOpenSpr(&packet);
    menuStatSprite(&packet, x, y - 0x1A, fillWidth0, 0xA, 0x48, 0xA7);
    grCloseSpr();

    boost = boostGaugeSelect(1);
    shownValue = panel->gauge[1];
    fillWidth1 = shownValue * MENU_VALUE_BAR_WIDTH / MENU_VALUE_GAUGE_FULL;
    grGsRegSet(&packet, GS_ALPHA_1, 0x44);
    grOpenSpr(&packet);
    menuStatSprite2(&packet, x, y + 0x10, MENU_VALUE_BAR_WIDTH, 0xA, 0x48, 0xB9, 0xBB, 0xB0);
    menuStatSprite(&packet, x, y + 0x19, 0x2E, 8, 0x78, 0x9D);
    grCloseSpr();
    {
        shownValue = fillWidth1;
        if (boost->stock >= boost->maxStock) {
            if (shownValue == MENU_VALUE_BAR_WIDTH) {
                grGsRegSet(&packet, GS_ALPHA_1, ((u64)flashAlpha << 32) | 0x68);
            }
        }
    }
    numberX = x + MENU_VALUE_BAR_WIDTH;
    grOpenSpr(&packet);
    menuStatSprite2(&packet, x, y + 0x13, fillWidth1, 8, 0x48, 0xB0, fillWidth1 + 0x48, 0xA7);
    grCloseSpr();
    grGsRegSet(&packet, GS_ALPHA_1, 0);
    boost = boostGaugeSelect(0);
    menuNumDraw(&packet, numberX, y - 0x24, boost->stock, 1, 0);
    boost = boostGaugeSelect(1);
    shownValue = boost->stock;
    menuNumDraw(&packet, numberX, y + 0xC, shownValue, 1, 0);
    grPacketSend(&packet);
}

/* The value panel object, a common global. */
struct MenuValuePanel *menuValuePanelObj;

/* Arrow state cycle length: arrowCycleUpdate wraps the cycle counter at this value. */
#define ARROW_CYCLE_LENGTH 0x11
/* Popup number cells: the row height (the glyph height cap), the growth per update and the frames to live. */
#define MENU_POP_ROW_HEIGHT 27
#define MENU_POP_CELL_GROWTH 4
#define MENU_POP_LIFETIME 25

/* The cycle counter at +0x94 of the object: arrowCycleUpdate advances it by one
 * and resets it to zero once it reaches 0x11. */
struct ArrowObject {
    unsigned char unmodeled_00[0x04];
    void (*update)(struct ArrowObject *object);
    void (*draw)(struct ArrowObject *object);
    unsigned char unmodeled_0c[0x04];
    SlideBlock slide;
    int counter;
    int cycle;
    unsigned int direction;
};

/* The direction word is stored through this union (alias set 0), so the store is
 * not reordered against the float stores of x and y. */
typedef union {
    unsigned int direction;
} ArrowValueWord;
void arrowCycleUpdate(struct ArrowObject *object);
/* Arrow draw (callback +8): the sprite sits at x plus the cycle, and y; the direction flag picks the texel order. */
void arrowDraw(struct ArrowObject *object);
void arrowCreate(int x, int y, int direction) {
    struct ArrowObject *object;
    float floatX;
    float floatY;
    unsigned int directionWord;

    directionWord = direction;

    object = objSlotAlloc();
    if (object != 0) {
        object->update = arrowCycleUpdate;
        object->draw = arrowDraw;
        arrowObj = object;
        objStdInit(&object->slide);
        floatX = x;
        floatY = y;
        object->slide.position.x = floatX;
        ((ArrowValueWord *) &object->direction)->direction = directionWord;
        object->slide.position.y = floatY;
        do {
            object->slide.position.z = 0;
            object->cycle = 0;
            object->counter = 0;
        } while (0);
    }
}

/* Arrow update (callback +4): advances the cycle and wraps it at 0x11. */
void arrowCycleUpdate(struct ArrowObject *object) {
    int next;

    next = object->cycle + 1;
    if (next >= ARROW_CYCLE_LENGTH) {
        next = 0;
    }
    object->cycle = next;
}

/* Arrow draw (callback +8): the sprite sits at x plus the cycle, and y; the direction flag picks the texel order. */
void arrowDraw(struct ArrowObject *object) {
    GrPacket packet;
    int screenX;
    int screenY;
    int srcLeft;
    int srcRight;
    int srcBottom;

    menuTexFlush();
    grGpInit(&packet);
    menuStepIconTexState(&packet);
    screenX = object->slide.position.x + object->cycle;
    screenY = object->slide.position.y;
    if (object->direction == 0) {
        srcLeft = 464;
        srcRight = 488;
        srcBottom = 234;
    } else {
        srcLeft = 487;
        srcRight = 464;
        srcBottom = 234;
    }
    grOpenSpr(&packet);
    menuStatSprite2(&packet, screenX, screenY, 24, 20, srcLeft, 214, srcRight, srcBottom);
    grCloseSpr();
    grPacketSend(&packet);
}


void arrowRemove(void) {
    if (arrowObj != 0) {
        objSlotRelease(arrowObj);
        arrowObj = 0;
    }
}

/* The object objDmgNumCreate makes: the transformed position (slide.block.position) in its matrix
 * block, then a countdown table of two rows of four bytes at +0xa0 that
 * objDmgNum steps on each update. The last cell (+0xa7) is the lifetime. */
struct PopNumObject {
    unsigned char unmodeled_00[0x04];
    void (*update)(struct PopNumObject *object);
    void (*draw)(struct PopNumObject *object);
    unsigned char unmodeled_0c[0x04];
    union {
        SlideBlock block;
        TransformState transform; /* the view grBBCalc projects */
    } slide;
    int counter;
    BattleUnit *owner;
    int amount;
    int kind;
    unsigned char cells[2][4];
};

void objDmgNum(struct PopNumObject *object);
void objDmgNumDraw(struct PopNumObject *object);

void objDmgNumCreate(BattleUnit *owner, int amount, int kind, int scale) {
    struct PopNumObject *object;
    int cellValue;
    int row;
    int column;
    float step;

    object = objSlotAlloc();
    if (object != 0) {
        object->update = objDmgNum;
        object->draw = objDmgNumDraw;
        objStdInit(&object->slide.block);
        sefActorMulLocalVector(owner->unit, 31, &object->slide.block.position, 1);
        step = scale / 100.0f;
        if (owner->stats->state >= 4) {
            step = step * 3.0f;
        }
        object->slide.block.position.y = object->slide.block.position.y + step;
        object->owner = owner;
        object->amount = amount;
        object->kind = kind;
        object->counter = 0;
        cellValue = 0;
        for (row = 4; row < 6; row++) {
            for (column = 0; column < 4 && (row != 5 || column != 3); column++) {
                object->cells[row - 4][column] = cellValue;
                cellValue -= 8;
            }
        }
        object->cells[1][3] = MENU_POP_LIFETIME;
    }
}

void objDmgNum(struct PopNumObject *object) {
    int row;
    int column;
    int rowOffset;
    unsigned char *cell;
    int lifetime;

    row = 4;
    rowOffset = 16;
    while (row < 6) {
        for (column = 0; column < 4 && (row != 5 || column != 3); column++) {
            if ((signed char) object->cells[row - 4][column] < MENU_POP_ROW_HEIGHT) {
                object->cells[row - 4][column] += MENU_POP_CELL_GROWTH;
            } else {
                object->cells[row - 4][column] = MENU_POP_ROW_HEIGHT;
            }
        }
        rowOffset += 4;
        row++;
    }
    lifetime = object->cells[1][3];
    object->cells[1][3] = lifetime - 1;
    if ((signed char) lifetime <= 0) {
        objSlotRelease(object);
    }
}

/* The draw view of the PopNumObject objDmgNumCreate makes, as dmgNumPut reads it: kind is an unsigned byte index
 * and the cells are signed bytes there. It stays a separate view because merging the byte read into PopNumObject
 * (a union on kind, signed cells) changes the code of the update and create bodies. */
struct PopNumDrawObject {
    unsigned char unmodeled_00[0x98];
    int amount;
    unsigned char kind;
    unsigned char unmodeled_9d[0x03];
    signed char cells[2][4];
};

void dmgNumPut(GrPacket *packet, int x, int y, struct PopNumDrawObject *object);

/* Draw callback of the object made by objDmgNumCreate (its +0x08 slot): grBBCalc
 * projects the object's origin (+0x10) to the screen, and when it is on screen
 * dmgNumPut draws the entry there. The object is passed as an int here. */
void objDmgNumDraw(struct PopNumObject *object) {
    GrPacket packet;
    int screenX;
    int screenY;

    if (grBBCalc(&object->slide.transform, &screenX, &screenY) != 0) {
        menuTexFlush();
        grGpInit(&packet);
        menuNumStyle1TexState(&packet);
        grOpenSpr(&packet);
        dmgNumPut(&packet, screenX, screenY, (struct PopNumDrawObject *) object);
        grCloseSpr();
        grPacketSend(&packet);
    }
}

extern MenuColor menuPopNumColors[];
extern int menuPopNumColorIndex[];

/* Draws the object's value (amount) in decimal, most significant place first.
 * Leading places are skipped (at least one digit is drawn). Each digit's glyph
 * advance comes from the style table, its height from the object's cell row,
 * and the colour from kind through menuPopNumColorIndex. */
void dmgNumPut(GrPacket *packet, int x, int y, struct PopNumDrawObject *object) {
    MenuDigitStyle *style;
    MenuColor *colour;
    int colourIndex;
    int remaining;
    int place;
    int divisor;
    int digits;
    int digit;
    int glyphWidth;
    int cellHeight;
    int srcLeft;
    int cellGap;
    int centreOffset;
    int rowHeight;

    rowHeight = MENU_POP_ROW_HEIGHT;
    divisor = 100000;
    colourIndex = menuPopNumColorIndex[object->kind];
    remaining = object->amount;
    digits = 0;
    place = 1;
    do {
        if (remaining >= place) {
            digits++;
        }
        place = place * 10;
    } while (!(divisor < place));
    if (digits == 0) {
        digits = 1;
    }
    x -= digits * 6;
    style = menuStyleEntry(1);
    colour = &menuPopNumColors[colourIndex];
    for (place = 6; place > 0; place--) {
        digit = remaining / divisor;
        remaining = remaining % divisor;
        divisor = divisor / 10;
        if (digits >= place) {
            glyphWidth = style->width[digit];
            if (place >= 5) {
                cellHeight = object->cells[1][(place - 1) % 4];
            } else {
                cellHeight = object->cells[0][place - 1];
            }
            if (cellHeight > 0) {
                srcLeft = style->srcX[digit];
                cellGap = rowHeight - cellHeight;
                centreOffset = rowHeight / 2 - cellHeight / 2;
                menuStatSprite2RGB(packet, x, y + (centreOffset - cellGap), glyphWidth, cellHeight, srcLeft, 23, srcLeft + glyphWidth, 50, colour);
            }
            x += glyphWidth;
        }
    }
}

/* Colours of the pop-up numbers (r, g, b, a), picked through menuPopNumColorIndex. */
MenuColor menuPopNumColors[6] = {
    { .rgba = { 0x80, 0x80, 0x80, 0x80 } },
    { .rgba = { 0x60, 0xff, 0x60, 0x80 } },
    { .rgba = { 0xff, 0x60, 0xff, 0x80 } },
    { .rgba = { 0x60, 0xff, 0xff, 0x80 } },
    { .rgba = { 0xf0, 0xfd, 0x95, 0x80 } },
    { .rgba = { 0, 0, 0, 0 } }
};

/* Colour index of each pop-up kind (kind 0..11) into menuPopNumColors. */
int menuPopNumColorIndex[12] = { 0, 0, 1, 2, 3, 0, 0, 4, 4, 2, 2, 0 };
