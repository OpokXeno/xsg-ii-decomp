#include "common.h"
#include "shared.h"
#include "gs.h"
#include "ov01/tu012.h"
#include "ov01/tu017.h"
#include "ov01/tu036.h"
#include "ov01/tu037.h"
#include "ov01/tu048.h"
#include "ov01/tu049.h"
#include "ov01/tu051.h"
#include "ov01/tu062.h"
#include "ov01/tu063.h"
#include "ov01/tu048.h"

/* GIF packet words are written as doubleword descriptors by the openers and
 * as 32-bit lanes by the vertex writers. Both are the same packet storage. */
typedef union GrGifWord {
    unsigned long long bits;
    unsigned int lanes[2];
} GrGifWord;

typedef struct GrGifOpenTag {
    GrGifWord control;
    GrGifWord register_id;
} GrGifOpenTag;

static inline void grGifOpenRegister(GrPacket *packet, unsigned long long register_id)
{
    GrGifOpenTag *tags = packet->data;
    tags[packet->count].register_id.bits = register_id;
}

void GS_REG_SET(GrPacket *packet, long long register_id, long long control)
{
    GrGifOpenTag *tags;
    int *count;
    int tag_index;

    grGifOpenRegister(packet, register_id);
    count = &packet->count;
    tags = packet->data;
    tag_index = *count;
    tags[tag_index].control.bits = control;
    *count = tag_index + 1;
}

/* Menu panel rows: MENU_PANEL_ROW_COUNT slots, a slot holds the row's y
 * (MENU_PANEL_ROW_Y0 + MENU_PANEL_ROW_STEP * index) while the row is taken, 0
 * when free. menuPanelRowAlloc takes one, menuPanelRowFree frees it. */
#define MENU_PANEL_ROW_COUNT 10
#define MENU_PANEL_ROW_Y0 100
#define MENU_PANEL_ROW_STEP 20

extern int menuPanelRows[MENU_PANEL_ROW_COUNT];
extern int screenFadeTask;
extern int screenFillTask;
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

void *objSlotAlloc(void);
void menuTexFlush(void);
void menuTexFlushDraw(void);
static struct MenuTexFlushTask *menuTexFlushTask;

/* Starts the texture-flush task: the object's draw is menuTexFlushDraw, and the
 * object is kept in menuTexFlushTask (nothing else in the overlay reads it). */
void menuTexFlushTaskStart(void)
{
    struct MenuTexFlushTask *task;

    task = objSlotAlloc();
    if (task != 0) {
        menuTexFlushTask = task;
        task->draw = menuTexFlushDraw;
        task->state = 0;
    }
}

/* Draw callback of the texture-flush task: flushes the menu texture. */
void menuTexFlushDraw(void)
{
    menuTexFlush();
}

/* The stat records menuStatRecordsCreate made, one per slot of the unit table (0 for an empty slot). */
MenuObject *menuStatRecords[4];

void menuStatGaugeStep(MenuObject *record);
void menuStatRecordSubmit(MenuObject *record);

/* Origin of the first stat record and the distance between two records. */
#define MENU_STAT_X 16
#define MENU_STAT_STEP_X 167
#define MENU_STAT_Y 12.0f

/*
 * Creates one stat record (an objSlotAlloc menu object stepped by menuStatGaugeStep and drawn by
 * menuStatRecordSubmit) per slot of the player unit table, side by side from MENU_STAT_X; an empty slot gets 0.
 * Stops when no object is free.
 */
void menuStatRecordsCreate(void)
{
    Ov01Object **units;
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
        record->update = menuStatGaugeStep;
        record->draw = menuStatRecordSubmit;
        menuStatRecords[i] = record;
        ov01MatrixBlockInit(&record->slide.block);
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
/* Highest level a gauge shows: the cap unitLevelAdd clamps StatBlock.level to (UNIT_LEVEL_MAX, tu023.c). */
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
void menuStatGaugeStep(MenuObject *record)
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

void menuStatRecordDraw(MenuObject *record, GrPacket *packet);

/* Draw callback of a stat record (MenuObject.draw): menuStatRecordDraw reads its origin (slide) and its unit. */
void menuStatRecordSubmit(MenuObject *record)
{
    GrPacket packet;

    grGpInit(&packet);
    menuStatRecordDraw(record, &packet);
    grPacketSend(&packet);
}

/* Pulse of the frame: the doubled frame counter as an 8-bit phase, mirrored in its upper half (a triangle wave),
 * added to the lowest fixed alpha. */
#define MENU_STAT_PULSE_PHASE_MASK 0xFF
#define MENU_STAT_PULSE_HALF 0x80
#define MENU_STAT_PULSE_ALPHA_MIN 10
/* ALPHA_1 blends of the two pulse passes, the fixed alpha (FIX) in the high word: (Cs - 0) * FIX + Cs, then
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
void menuStatRecordDraw(MenuObject *record, GrPacket *packet)
{
    Ov01Object *unit;
    unsigned long long alpha;
    int phase;
    int x;
    int y;
    int value;

    x = record->slide.pos.x;
    y = record->slide.pos.y;
    unit = record->kind.stat.unit;
    if (unit == battleStateGet()->current) {
        func_00AA84F0(packet);
        phase = (ov01SceneFrameCount() * 2) & MENU_STAT_PULSE_PHASE_MASK;
        if (phase >= MENU_STAT_PULSE_HALF) {
            phase = MENU_STAT_PULSE_PHASE_MASK - phase;
        }
        /* GS ALPHA_1 with a fixed alpha (high word) that rises and falls with the frame counter, as in
         * menuPulseAlphaSet (tu056) but from MENU_STAT_PULSE_ALPHA_MIN: a 0x91 x 0xA sprite at y + 0x38, then the
         * frame around the record */
        alpha = (unsigned long long)(phase + MENU_STAT_PULSE_ALPHA_MIN) << 32;
        grGsRegSet(packet, GS_ALPHA_1, alpha | MENU_STAT_PULSE_BLEND_ADD_SRC);
        grPutSprOpen(packet);
        menuStatSprite(packet, x, y + 0x38, 0x91, 0xA, 0, 0x41);
        grPutSprClose();
        grGsRegSet(packet, GS_ALPHA_1, alpha | MENU_STAT_PULSE_BLEND_ADD_DST);
        grPutSprOpen(packet);
        menuStatSprite2(packet, x - 4, y - 4, 0x9E, 0x3B, 0xC8, 0x78, 0xFF, 0xBA);
        grPutSprClose();
    }
    menuKeyDraw(packet, x, y, unit->stats->key, 0);
    func_00AA8490(packet);
    /* HP and EP gauges: each a bar track (MENU_STAT_BAR_WIDTH wide) and its fill scaled to the shown value. */
    grPutSprOpen(packet);
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
    grPutSprClose();
    if ((unsigned int)(unit->stats->keyUnsigned - UNIT_KEY_PARTY_FIRST) < UNIT_KEY_PARTY_COUNT) {
        /* Level pips: one pip per shown level (texture row 0x81), then the three pip cells (row 0x78). */
        grPutSprOpen(packet);
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
        grPutSprClose();
    } else {
        /* Current gauge: its track (MENU_STAT_CURRENT_BAR_WIDTH wide) and fill, then the shown value. */
        grPutSprOpen(packet);
        value = record->kind.stat.shown[2];
        if (unit->stats->currentMax <= 0) {
            value = 0;
        } else {
            value = value * MENU_STAT_CURRENT_BAR_WIDTH / unit->stats->currentMax;
        }
        menuStatSprite(packet, x + 0x48, y + 0x30, MENU_STAT_CURRENT_BAR_WIDTH, 5, 0x48, 0x98);
        menuStatSprite(packet, x + 0x48, y + 0x2F, value, 5, 0x48, 0x93);
        grPutSprClose();
        menuNumDraw(packet, x + 0x74, y + 0x26, record->kind.stat.shown[2], 3, 2);
    }
    menuNumTexState(packet);
    /* Icons at the HP and EP rows and at the third gauge (a different one for party keys), then the HP number (4
     * digits for party keys, 5 otherwise) and the EP number. */
    grPutSprOpen(packet);
    menuStatSprite(packet, x + 0x3C, y - 6, 0x12, 0x10, 0x6B, 0);
    menuStatSprite(packet, x + 0x3C, y + 0x10, 0x12, 0x10, 0x7D, 0);
    if ((unsigned int)(unit->stats->keyUnsigned - UNIT_KEY_PARTY_FIRST) < UNIT_KEY_PARTY_COUNT) {
        menuStatSprite(packet, x + 0x38, y + 0x28, 0x10, 0xE, 0x64, 0x32);
    } else {
        menuStatSprite(packet, x + 0x38, y + 0x28, 0x10, 0xE, 0x54, 0x32);
    }
    grPutSprClose();
    value = record->kind.stat.shown[0];
    if ((unsigned int)(unit->stats->keyUnsigned - UNIT_KEY_PARTY_FIRST) < UNIT_KEY_PARTY_COUNT) {
        menuNumDrawTarget(packet, x + 0x64, y - 8, value, 4, 0, unit);
    } else {
        menuNumDrawTarget(packet, x + 0x58, y - 8, value, 5, 0, unit);
    }
    func_00AA7A48(packet, x + 0x7B, y + 0xE, record->kind.stat.shown[1], 2, 0, unit);
}
