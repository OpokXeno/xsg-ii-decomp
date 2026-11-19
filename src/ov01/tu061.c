/*
 * OV01 original TU 61: 0x00aa6bf8..0x00aa70e8 (6 functions)
 */
#include "common.h"
#include "ov01/tu061.h"
#include "ov01/tu012.h"
#include "ov01/tu048.h"
#include "ov01/tu062.h"

/* Values of TimedSequence.stage. */
enum TimedSeqStage {
    TIMED_SEQ_STAGE_IDLE = 0,
    TIMED_SEQ_STAGE_SLIDE = 1 /* the panel slides in toward its origin */
};

/* Tick count at which a timed sequence ends and frees its row. */
#define MENU_TIMED_SEQ_TICKS 0x10

/* Combo hit count from which a unit takes the timed sequence (below it only the tick is armed). */
#define MENU_TIMED_SEQ_MIN_HITS 2

/* The origin block at +0x10 of a timed sequence: the callees take its address; this body reads only y, the row. */
typedef struct SequenceOrigin {
    f32 x; /* +0x10: the panel x, 16 for a panel from menuTimedSeqShow */
    f32 y;
} SequenceOrigin;

typedef struct TimedSequence {
    unsigned char unmodeled_000[4];
    void (*step)(struct TimedSequence *); /* +0x04: menuTimedSeqStep, the step of the sequence */
    void (*draw)(struct TimedSequence *); /* +0x08: menuTimedPanelDraw */
    unsigned char unmodeled_00C[4];
    SequenceOrigin origin;      /* +0x10: x and y of the panel; y is the row given by menuPanelRowAlloc */
    unsigned char unmodeled_018[0x58];
    f32 slideX;                 /* +0x70: the x the panel slides to, 16 for a panel from menuTimedSeqStart */
    f32 slideY;                 /* +0x74: the row the panel slides on */
    unsigned char unmodeled_078[0x18];
    enum TimedSeqStage stage;   /* +0x90: TIMED_SEQ_STAGE_SLIDE while the panel slides in, else IDLE */
    Ov01Object *owner;          /* +0x94: the unit the sequence shows, zero when the sequence is free */
    int slot;                   /* +0x98: index of the set bit of the unit's word at +0xF0 (menuTimedPanelDraw picks its title with it) */
    int value;                  /* +0x9C: the number menuTimedPanelDraw prints (the unit's word at +0xF4) */
    s16 tickEnabled;            /* +0xA0: set by menuTimedSeqArmTick, lets the ticks count */
    u16 tickCount;              /* +0xA2: counts ticks; 16 ends the sequence */
} TimedSequence;

/* A one-int view of a sequence field. Storing through it is an alias-0 store: gcc keeps the unit->stats reload after it,
 * as the original does (menuTimedSeqShow, the value store). */
typedef union TimedWordView {
    int word;
} TimedWordView;

void *objSlotAlloc(void);
int objStdMove(void *);
int slideSetStep(void *, int);
int menuPanelRowFree(int);
int menuPanelRowAlloc(void);
TimedSequence *menuTimedPanelNew(int value);
void menuTimedSeqArmTick(void);
void menuTimedSeqStep(TimedSequence *sequence);
void menuTimedPanelDraw(TimedSequence *sequence);
extern TimedSequence *menuTimedSeqPanel;

/* One title of the timed panel: the sprite's u, v, width and height, 16 bytes per slot. */
typedef struct MenuTitleEntry {
    int u;      /* +0x0 */
    int v;      /* +0x4 */
    int w;      /* +0x8 */
    int h;      /* +0xC */
} MenuTitleEntry;

extern MenuTitleEntry menuTimedTitleTable[];
void menuTexFlush(void);
void menuPanelTexState(GrPacket *packet);

/*
 * menuTimedSeqNew: constructor of the timed sequence
 * panel. It allocates the panel with objSlotAlloc, sets its step menuTimedSeqStep
 * and its draw menuTimedPanelDraw, keeps it in menuTimedSeqPanel and
 * zeroes its state, owner, slot, value and tick fields. Called from the battle
 * setup ov01StepNextEnemy.
 */
void menuTimedSeqNew(void)
{
    TimedSequence *sequence;

    sequence = objSlotAlloc();
    if (sequence != 0) {
        sequence->step = menuTimedSeqStep;
        sequence->draw = menuTimedPanelDraw;
        menuTimedSeqPanel = sequence;
        ov01MatrixBlockInit(&sequence->origin);
        sequence->stage = 0;
        sequence->owner = 0;
        sequence->slot = 0;
        sequence->value = 0;
        ((TimedWordView *) &sequence->tickEnabled)->word = 0;
    }
}

/*
 * menuTimedSeqStep: the step of the timed sequence.
 * While an owner is set, stage 1 slides the origin with slideSetStep until
 * objStdMove reports arrival. Otherwise, once the tick is enabled, each call
 * counts one tick; the sixteenth tick ends the sequence and frees its row.
 */
void menuTimedSeqStep(TimedSequence *sequence)
{
    int stage;
    void *slideOrigin;

    if (sequence->owner == 0) {
        return;
    }
    stage = sequence->stage;
    if (stage != 0) {
        if (stage == TIMED_SEQ_STAGE_SLIDE) {
            slideOrigin = &sequence->origin;
            slideSetStep(slideOrigin, 2);
            if (objStdMove(slideOrigin) != 0) {
                sequence->stage = TIMED_SEQ_STAGE_IDLE;
            }
            return;
        }
    }
    if (sequence->tickEnabled == 1) {
        s16 ticks;

        ticks = sequence->tickCount + 1;
        sequence->tickCount = sequence->tickCount + 1;
        if (ticks >= MENU_TIMED_SEQ_TICKS) {
            sequence->owner = 0;
            sequence->stage = 0;
            sequence->slot = 0;
            sequence->value = 0;
            menuPanelRowFree((int)sequence->origin.y);
        }
    }
}

/*
 * menuTimedPanelDraw: draws a timed panel while it has
 * an owner: the title row from the table menuTimedTitleTable at the slot (+0x98), then
 * the number at +0x9C with menuNumDrawMode. The same draw is the one of the
 * menuTimedPanelNew objects of ov01/tu062.
 */
void menuTimedPanelDraw(TimedSequence *sequence)
{
    GrPacket packet;
    int x;
    int y;
    int slot;
    int width;
    int titleWidth;

    if (sequence->owner != 0) {
        x = (int)sequence->origin.x;
        y = (int)sequence->origin.y;
        menuTexFlush();
        grGpInit(&packet);
        menuPanelTexState(&packet);
        grPutSprOpen(&packet);
        slot = sequence->slot;
        width = 37;
        menuStatSprite(&packet, 0, y + 10, x + (menuTimedTitleTable[slot].w + width) + 10, 10, 431 - (x + width + menuTimedTitleTable[slot].w), 74);
        menuStatSprite(&packet, x, y, width, 18, 468, 107);
        menuStatSprite(&packet, x + width, y, menuTimedTitleTable[slot].w, menuTimedTitleTable[slot].h, menuTimedTitleTable[slot].u, menuTimedTitleTable[slot].v);
        titleWidth = menuTimedTitleTable[slot].w;
        width = titleWidth + 57;
        menuStatSprite(&packet, x + width, y, 16, 17, 479, 0);
        grPutSprClose();
        width = titleWidth + 75;
        menuNumDrawMode(&packet, x + width, y, sequence->value, 2, 3, 0);
        grPacketSend(&packet);
    }
}

/*
 * menuTimedSeqStart (argument unit): for a unit whose
 * word at +0xF4 is below MENU_TIMED_SEQ_MIN_HITS, ends the sequence when the unit owns it (tick armed,
 * tick count 16) and does nothing otherwise. For a unit of 2 and up: when the
 * unit already owns the sequence only the value at +0x9C is set; otherwise the
 * sequence (or, when another unit owns it, a new menuTimedPanelNew panel of
 * ov01/tu062) takes the unit with state 1, the slot of the lowest set bit of the
 * unit's word at +0xF0, the value from +0xF4 and origin (-152, row).
 */
void menuTimedSeqStart(Ov01Object *unit)
{
    TimedSequence *sequence;
    f32 row;
    int slot;
    int mask;

    sequence = menuTimedSeqPanel;
    if (unit->stats->comboHits < MENU_TIMED_SEQ_MIN_HITS) {
        if (sequence->owner == unit) {
            menuTimedSeqArmTick();
            sequence->tickCount = MENU_TIMED_SEQ_TICKS;
        }
        return;
    }
    if (sequence->owner == unit) {
        sequence->value = unit->stats->comboHits;
        return;
    }
    if (sequence->owner != 0) {
        sequence = menuTimedPanelNew(unit->stats->comboHits);
        if (sequence == 0) {
            return;
        }
    }
    ((TimedWordView *) &sequence->value)->word = unit->stats->comboHits;
    mask = unit->stats->comboMask;
    for (slot = 0; slot < 5; slot++) {
        if (mask & 1) {
            break;
        }
        mask >>= 1;
    }
    sequence->owner = unit;
    sequence->stage = TIMED_SEQ_STAGE_SLIDE;
    sequence->slot = slot;
    ((TimedWordView *) &sequence->tickEnabled)->word = 0;
    row = (f32) menuPanelRowAlloc();
    sequence->origin.x = -152.0f;
    sequence->slideX = 16.0f;
    sequence->origin.y = row;
    sequence->slideY = row;
}

/* menuTimedSeqArmTick: enables the tick count of the timed sequence. */
void menuTimedSeqArmTick(void) {
    menuTimedSeqPanel->tickEnabled = 1;
}

/*
 * menuTimedSeqShow (argument unit): for a unit below MENU_TIMED_SEQ_MIN_HITS hits
 * it arms the tick of the sequence when one is owned. Otherwise it frees the old
 * row of an owned sequence, binds the unit with its hit count, the slot of the
 * lowest set bit of its combo mask, a full tick count, stage idle and a new row,
 * origin (16, row).
 */
void menuTimedSeqShow(Ov01Object *unit)
{
    TimedSequence *sequence;
    int mask;
    int slot;

    sequence = menuTimedSeqPanel;
    if (unit->stats->comboHits < MENU_TIMED_SEQ_MIN_HITS) {
        if (sequence->owner != 0) {
            menuTimedSeqArmTick();
            sequence->tickCount = MENU_TIMED_SEQ_TICKS;
        }
        return;
    }
    if (sequence->owner != 0) {
        menuPanelRowFree((int) sequence->origin.y);
    }
    ((TimedWordView *) &sequence->value)->word = unit->stats->comboHits;
    mask = unit->stats->comboMask;
    for (slot = 0; slot < 5; slot++) {
        if (mask & 1) {
            break;
        }
        mask >>= 1;
    }
    sequence->owner = unit;
    sequence->slot = slot;
    sequence->tickCount = MENU_TIMED_SEQ_TICKS;
    sequence->stage = 0;
    sequence->tickEnabled = 0;
    sequence->origin.y = (f32) menuPanelRowAlloc();
    sequence->origin.x = 16.0f;
}

/* The timed sequence panel, a common global. */
TimedSequence *menuTimedSeqPanel;

/* Title rows of menuTimedPanelDraw: the sprite u, v and the size w, h of each title. */
MenuTitleEntry menuTimedTitleTable[5] = {
    { 370, 86, 57, 18 },
    { 414, 107, 52, 18 },
    { 256, 107, 70, 18 },
    { 329, 107, 44, 18 },
    { 375, 107, 37, 18 }
};
