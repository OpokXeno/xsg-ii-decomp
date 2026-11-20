#include "common.h"
#include "ov01/obj_slot.h"
#include "ov01/menu_combo_panel.h"
#include "ov01/menu_wep_sp_panel.h"
#include "ov01/menu.h"
#include "ov01/unit_cmd.h"
#include "ov01/gr_gp_init.h"
#include "ov01/menu_panel.h"
#include "ov01/menu_tex_state.h"

/* Values of MenuState.state for the timed sequence. */
enum TimedSeqStage {
    TIMED_SEQ_STAGE_IDLE = 0,
    TIMED_SEQ_STAGE_SLIDE = 1 /* the panel slides in toward its origin */
};

/* Tick count at which a timed sequence ends and frees its row. */
#define MENU_TIMED_SEQ_TICKS 0x10

/* Combo hit count from which a unit takes the timed sequence (below it only the tick is armed). */
#define MENU_TIMED_SEQ_MIN_HITS 2

/* A one-int view of a sequence field. Storing through it is an alias-0 store: gcc keeps the unit->stats reload after it,
 * as the original does (menuComboPanelShow, the value store). */
typedef union TimedWordView {
    int word;
} TimedWordView;

void menuComboPanelStep(MenuState *sequence);

/* One title of the timed panel: the sprite's u, v, width and height, 16 bytes per slot. */
typedef struct MenuTitleEntry {
    int u;      /* +0x0 */
    int v;      /* +0x4 */
    int w;      /* +0x8 */
    int h;      /* +0xC */
} MenuTitleEntry;

extern MenuTitleEntry menuTimedTitleTable[];

/*
 * menuComboPanelNew: constructor of the timed sequence
 * panel. It allocates the panel with objSlotAlloc, sets its step menuComboPanelStep
 * and its draw menuComboPanelDraw, keeps it in menuTimedSeqPanel and
 * zeroes its state, owner, slot, value and tick fields. Called from the battle
 * setup ov01StepNextEnemy.
 */
void menuComboPanelNew(void)
{
    MenuState *sequence;

    sequence = objSlotAlloc();
    if (sequence != 0) {
        sequence->update = menuComboPanelStep;
        sequence->draw = menuComboPanelDraw;
        menuTimedSeqPanel = sequence;
        objStdInit(&sequence->slide);
        sequence->state = 0;
        sequence->owner = 0;
        sequence->wait = 0;
        sequence->value = 0;
        ((TimedWordView *) &sequence->tickEnabled)->word = 0;
    }
}

/*
 * menuComboPanelStep: the step of the timed sequence.
 * While an owner is set, stage 1 slides the origin with objHatoVec until
 * objStdMove reports arrival. Otherwise, once the tick is enabled, each call
 * counts one tick; the sixteenth tick ends the sequence and frees its row.
 */
void menuComboPanelStep(MenuState *sequence)
{
    int stage;
    void *slideOrigin;

    if (sequence->owner == 0) {
        return;
    }
    stage = sequence->state;
    if (stage != 0) {
        if (stage == TIMED_SEQ_STAGE_SLIDE) {
            slideOrigin = &sequence->slide;
            objHatoVec(slideOrigin, 2);
            if (objStdMove(slideOrigin) != 0) {
                sequence->state = 0;
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
            sequence->state = 0;
            sequence->wait = 0;
            sequence->value = 0;
            menuPanelRowFree((int)sequence->slide.position.y);
        }
    }
}

/*
 * menuComboPanelDraw: draws a timed panel while it has
 * an owner: the title row from the table menuTimedTitleTable at the slot (+0x98), then
 * the number at +0x9C with menuNumPut. The same draw is the one of the
 * menuComboPanelExtraNew objects of ov01/menu_panel.c.
 */
void menuComboPanelDraw(MenuState *sequence)
{
    GrPacket packet;
    int x;
    int y;
    int slot;
    int width;
    int titleWidth;

    if (sequence->owner != 0) {
        x = (int)sequence->slide.position.x;
        y = (int)sequence->slide.position.y;
        menuTexFlush();
        grGpInit(&packet);
        menuPanelTexState(&packet);
        grOpenSpr(&packet);
        slot = sequence->wait;
        width = 37;
        menuStatSprite(&packet, 0, y + 10, x + (menuTimedTitleTable[slot].w + width) + 10, 10, 431 - (x + width + menuTimedTitleTable[slot].w), 74);
        menuStatSprite(&packet, x, y, width, 18, 468, 107);
        menuStatSprite(&packet, x + width, y, menuTimedTitleTable[slot].w, menuTimedTitleTable[slot].h, menuTimedTitleTable[slot].u, menuTimedTitleTable[slot].v);
        titleWidth = menuTimedTitleTable[slot].w;
        width = titleWidth + 57;
        menuStatSprite(&packet, x + width, y, 16, 17, 479, 0);
        grCloseSpr();
        width = titleWidth + 75;
        menuNumPut(&packet, x + width, y, sequence->value, 2, 3, 0);
        grPacketSend(&packet);
    }
}

/*
 * menuComboPanelStart (argument unit): for a unit whose
 * word at +0xF4 is below MENU_TIMED_SEQ_MIN_HITS, ends the sequence when the unit owns it (tick armed,
 * tick count 16) and does nothing otherwise. For a unit of 2 and up: when the
 * unit already owns the sequence only the value at +0x9C is set; otherwise the
 * sequence (or, when another unit owns it, a new menuComboPanelExtraNew panel of
 * ov01/menu_panel.c) takes the unit with state 1, the slot of the lowest set bit of the
 * unit's word at +0xF0, the value from +0xF4 and origin (-152, row).
 */
void menuComboPanelStart(BattleUnit *unit)
{
    MenuState *sequence;
    f32 row;
    int slot;
    int mask;

    sequence = menuTimedSeqPanel;
    if (unit->stats->comboHits < MENU_TIMED_SEQ_MIN_HITS) {
        if (sequence->owner == unit) {
            menuComboPanelArmTick();
            sequence->tickCount = MENU_TIMED_SEQ_TICKS;
        }
        return;
    }
    if (sequence->owner == unit) {
        sequence->value = unit->stats->comboHits;
        return;
    }
    if (sequence->owner != 0) {
        sequence = menuComboPanelExtraNew();
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
    sequence->state = TIMED_SEQ_STAGE_SLIDE;
    sequence->wait = slot;
    ((TimedWordView *) &sequence->tickEnabled)->word = 0;
    row = (f32) menuPanelRowAlloc();
    sequence->slide.position.x = -152.0f;
    sequence->slide.goal.x = 16.0f;
    sequence->slide.position.y = row;
    sequence->slide.goal.y = row;
}

/* menuComboPanelArmTick: enables the tick count of the timed sequence. */
void menuComboPanelArmTick(void) {
    menuTimedSeqPanel->tickEnabled = 1;
}

/*
 * menuComboPanelShow (argument unit): for a unit below MENU_TIMED_SEQ_MIN_HITS hits
 * it arms the tick of the sequence when one is owned. Otherwise it frees the old
 * row of an owned sequence, binds the unit with its hit count, the slot of the
 * lowest set bit of its combo mask, a full tick count, stage idle and a new row,
 * origin (16, row).
 */
void menuComboPanelShow(BattleUnit *unit)
{
    MenuState *sequence;
    int mask;
    int slot;

    sequence = menuTimedSeqPanel;
    if (unit->stats->comboHits < MENU_TIMED_SEQ_MIN_HITS) {
        if (sequence->owner != 0) {
            menuComboPanelArmTick();
            sequence->tickCount = MENU_TIMED_SEQ_TICKS;
        }
        return;
    }
    if (sequence->owner != 0) {
        menuPanelRowFree((int) sequence->slide.position.y);
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
    sequence->wait = slot;
    sequence->tickCount = MENU_TIMED_SEQ_TICKS;
    sequence->state = 0;
    sequence->tickEnabled = 0;
    sequence->slide.position.y = (f32) menuPanelRowAlloc();
    sequence->slide.position.x = 16.0f;
}

/* The timed sequence panel, a common global. */
MenuState *menuTimedSeqPanel;

/* Title rows of menuComboPanelDraw: the sprite u, v and the size w, h of each title. */
MenuTitleEntry menuTimedTitleTable[5] = {
    { 370, 86, 57, 18 },
    { 414, 107, 52, 18 },
    { 256, 107, 70, 18 },
    { 329, 107, 44, 18 },
    { 375, 107, 37, 18 }
};
