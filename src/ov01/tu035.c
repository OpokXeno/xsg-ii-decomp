/*
 * OV01 original TU 35: 0x00a97180..0x00a97d68 (16 functions)
 */
#include "common.h"
#include "main/pad.h"
#include "main/tu048.h"
#include "ov01/tu005.h"
#include "ov01/tu012.h"
#include "ov01/tu017.h"
#include "ov01/tu022.h"
#include "ov01/tu023.h"
#include "ov01/tu033.h"
#include "ov01/tu035.h"
#include "ov01/tu036.h"
#include "ov01/tu042.h"
#include "ov01/tu091.h"
#include "ov01/tu059.h"
#include "ov01/tu060.h"
#include "ov01/tu061.h"
#include "ov01/tu066.h"
#include "ov01/tu070.h"
#include "ov01/tu071.h"

typedef struct TgtSlot TgtSlot;

/*
 * A battle candidate is a unit object (Ov01Object in ov01/tu012.h). Its flags word at +0x0c: 0x08 is set when
 * tgtSelectBegin takes it, 0x10 is the highlight (set by tgtSelectActivate, cleared by tgtSelectUnhighlight
 * below), 0x20 gates the cursor stage, 0x01 is set and 0x1000 cleared when tgtSelectConfirm picks it.
 */

/* Three-entry table set up by tgtSelectInit: index is the slot number, value starts at 180. */
struct TgtSlot {
    int unmodeled_00;
    int index;
    int value;
    int unmodeled_0c[2];
};

extern TgtSlot ov01PartyTable[];
extern Ov01Object *tgtSelectCurrent;
extern int tgtSelectStatus;
extern int tgtSelectMode;
void *memset(void *s, int c, unsigned int n);
void tgtSelectActivate(void);
void tgtSelectBegin(void);
int tgtSelectCursor(int inputHandled);
void tgtSelectExecute(void);
int tgtSelectStatusUpdate(void);
int tgtSelectPadInput(void);

/* Clears the selection state, numbers the three slots and arms the begin stage (flag 1). */
void tgtSelectInit(void)
{
    int i;

    memset(&tgtSelectState, 0, sizeof(tgtSelectState));
    for (i = 0; i < 3; i++) {
        ov01PartyTable[i].index = i;
        ov01PartyTable[i].value = 180;
    }
    tgtSelectState.flags |= 1;
    tgtSelectState.phase = 0;
}

/* Runs the stage handlers whose bits are set in state->flags, then the status update;
 * returns its result (1 when the selection is done). */
int tgtSelectStep(void)
{
    BattleState *state = &tgtSelectState;
    int inputHandled;

    if (state->flags & 1) {
        tgtSelectBegin();
    }
    if (state->flags & 2) {
        tgtSelectActivate();
    }
    inputHandled = tgtSelectPadInput();
    if (state->flags & 4) {
        tgtSelectCursor(inputHandled);
    }
    if (state->flags & 8) {
        tgtSelectExecute();
    }
    return tgtSelectStatusUpdate();
}

/* Side arguments of the unit counts. */
#define TGT_SIDE_PARTY 0
#define TGT_SIDE_ENEMY 1
/* Side argument for both tables. */
#define TGT_SIDE_ALL 2

/* Status codes tgtSelectStatusUpdate leaves in BattleState.status (0 while the battle goes on). */
enum {
    TGT_STATUS_ENEMY_GONE = 1,          /* unitNumFlag4ClearStatZeroGet(enemies) is 0 */
    TGT_STATUS_PARTY_GONE = 2,          /* unitNumFlag4ClearStatZeroGet(party) is 0 */
    TGT_STATUS_PARTY_LEFT = 3,          /* transition 1 */
    TGT_STATUS_ENEMY_LEFT = 4,          /* transition 2 */
    TGT_STATUS_TIME_UP = 5              /* time limit reached */
};


/* bgmPlayTrack track of the victory. */
#define TGT_VICTORY_BGM 6
/* streamPlayByKey key of the victory line and unitScriptEventSend selector of the victory camera. */
#define TGT_VICTORY_STREAM 7
#define TGT_VICTORY_CAMERA 7

int tgtSelectStatusGet(void);
Ov01Object *unitSelectPlTarget(Ov01Object *last);

/* Returns 1 once the battle is over. While a unit is busy it returns 0. An enemy-gone status starts the victory
 * once (BGM, the line of a party unit, the camera on the last party unit, UNIT_CMD_PHASE_2C_WAIT for every
 * party unit that is not down) and then ends on circle or when no party unit is still moving; the other statuses
 * end at once. */
int battleEndCheck(void)
{
    Ov01Command command;
    UnitScriptEvent focus;
    Ov01Object **units;
    Ov01Object *speaker;
    int count;
    int i;

    if (unitNumBusyGet(TGT_SIDE_ALL) > 0) {
        return 0;
    }
    switch (tgtSelectStatusGet()) {
    case TGT_STATUS_ENEMY_GONE:
        if (cfEventGet() & CF_EVENT_SKIP_VICTORY) {
            return 1;
        }
        if (tgtSelectState.victoryStarted == 0) {
            bgmPlayTrack(TGT_VICTORY_BGM);
            speaker = unitSelectPlTarget(tgtSelectState.lastPartyUnit);
            if (speaker != 0) {
                streamPlayByKey(speaker, TGT_VICTORY_STREAM);
            }
            focus.unit = tgtSelectState.lastPartyUnit;
            if (focus.unit == 0 || (focus.unit->flags & UNIT_FLAG_DOWN)) {
                focus.unit = speaker;
            }
            unitScriptEventSend(TGT_VICTORY_CAMERA, &focus);
            count = unitTblGet(TGT_SIDE_PARTY, &units);
            for (i = 0; i < count; i++) {
                if (units[i] != 0 && !(units[i]->flags & UNIT_FLAG_DOWN)) {
                    command.command = UNIT_CMD_PHASE_2C_WAIT;
                    unitQueueAppend(units[i], &command);
                }
            }
            tgtSelectState.victoryStarted = 1;
            decoFlagsSetAll();
            return 0;
        }
        if (!(dataPadRead(1) & PAD_CIRCLE)) {
            if (unitNumFlag1Flag4ClearGet(TGT_SIDE_PARTY) != 0) {
                return 0;
            }
        }
        /* fall through: the battle is over */
    case TGT_STATUS_PARTY_GONE:
        return 1;
    case TGT_STATUS_PARTY_LEFT:
        return 1;
    case TGT_STATUS_ENEMY_LEFT:
        return 1;
    case TGT_STATUS_TIME_UP:
        return 1;
    }
    return 0;
}

/* Candidate flag of the highlighted candidate (tgtSelectActivate sets it). */
#define TGT_OBJECT_HIGHLIGHT 0x10
/* Candidate flags tgtSelectBegin sets and clears when it takes a candidate. */
#define TGT_OBJECT_TAKEN 0x8
#define TGT_OBJECT_CLEARED_ON_TAKE 0x200
/* Stage bits of BattleState.flags. */
#define TGT_STAGE_BEGIN 0x1
#define TGT_STAGE_ACTIVATE 0x2
#define TGT_STAGE_EXECUTE 0x8
/* Party units have keys below this (StatBlock.key). */
#define TGT_PARTY_KEY_LIMIT 16

void objPathReset(void);
void objPathStep(void);
void objPathSendLast(void);
void func_00A90368(Ov01Object *object);

/* Stage 1: takes the next candidate (actStockPick), arms the activate stage and moves
 * the path objects for the stock level. */
void tgtSelectBegin(void)
{
    Ov01Object *object;
    int level;

    object = actStockPick();
    tgtSelectState.current = object;
    if (object != 0) {
        object->flags = (object->flags & ~TGT_OBJECT_CLEARED_ON_TAKE) | TGT_OBJECT_TAKEN;
        tgtSelectState.flags = (tgtSelectState.flags & ~TGT_STAGE_BEGIN) | TGT_STAGE_ACTIVATE;
        switch (actStockGet()->pickSource) {
        case 1:
            objPathStep();
            break;
        case 0:
            objPathReset();
            break;
        default:
            objPathSendLast();
            break;
        }
        func_00A90368(tgtSelectState.current);
    }
}

/* Stage 2: once unitQueueIsEmpty reports the current candidate ready, highlights it and moves to the cursor stage (flag 4). */
void tgtSelectActivate(void)
{
    BattleState *state = &tgtSelectState;
    UnitScriptEvent event;

    if (unitQueueIsEmpty(state->current) != 0) {
        event.unit = state->current;
        event.unit->flags |= 0x10;
        state->active = 1;
        state->mode = 0;
        state->flags = (state->flags & ~2) | 4;
        unitScriptEventSend(4, &event);
    }
}

int tgtSelectAuto(Ov01Object *object);
int tgtSelectManual(Ov01Object *object);

/* Candidate flag that lets the cursor stage confirm on its own (tgtSelectAuto). */
#define TGT_OBJECT_AUTO 0x20
/* Pad bits that flip the side toggle (L2 and L1). */
#define TGT_SIDE_TOGGLE_PAD 0x5

/* Stage 4: an automatic candidate confirms once tgtSelectAuto agrees; otherwise, unless the input was already
 * handled, L1/L2 flip the side toggle and tgtSelectManual decides. Declared int although it returns no value:
 * the original does not tail-call tgtSelectConfirm, which gcc only does for a void function here. */
int tgtSelectCursor(int inputHandled)
{
    BattleState *state = &tgtSelectState;

    if (state->current->flags & TGT_OBJECT_AUTO) {
        if (tgtSelectAuto(state->current) != 0) {
            tgtSelectConfirm(state->current);
        }
    } else if (inputHandled != 1) {
        if (dataPadRead(1) & TGT_SIDE_TOGGLE_PAD) {
            state->sideToggle = (state->sideToggle + 1) & 1;
        }
        if (tgtSelectManual(state->current) != 0) {
            tgtSelectConfirm(state->current);
        }
    }
}

/*
 * Confirms object: it becomes lastObject and the execute stage (flag 8) is armed. If the current
 * candidate has lost its highlight, the selection is dropped and the cursor stage (flag 4) cleared.
 */
void tgtSelectConfirm(Ov01Object *object)
{
    BattleState *state = &tgtSelectState;
    int flags;

    object->flags = (object->flags | 1) & ~0x1000;
    state->lastObject = object;
    flags = state->flags | 8;
    state->flags = flags;
    if (!(state->current->flags & 0x10)) {
        state->active = 0;
        state->current = 0;
        state->selectionKind = 0;
        state->selection = 0;
        state->flags = flags & ~4;
    }
}

/* Candidate flag set while the picked unit's turn is over (tgtSelectConfirm clears it). */
#define TGT_OBJECT_TURN_DONE 0x1000
/* Phases of the execute stage (BattleState.phase). */
enum {
    TGT_PHASE_RUN = 0,                  /* run the head command of the list */
    TGT_PHASE_EVENTS = 1,               /* drain the event queue */
    TGT_PHASE_TURN_END = 2              /* wait for the units, then end the turn */
};

void cmdListRunHead(void);
int eventQueueRun(void);
void tgtSelectTurnEnd(Ov01Object *unit);
void menuPanelHide(void);
void unitValueRefresh(Ov01Object *object);
void actStepCounterAdvance(void);
int dataEntryBlockReset(void);

/* Stage 8: runs the picked command, drains its events and, once the unit's turn is
 * done and no unit is busy, ends the turn and arms the begin stage again. */
void tgtSelectExecute(void)
{
    BattleState *state = &tgtSelectState;
    UnitScriptEvent event;

    switch (state->phase) {
    case TGT_PHASE_RUN:
        cmdListRunHead();
        state->phase = TGT_PHASE_EVENTS;
        /* fall through */
    case TGT_PHASE_EVENTS:
        if (eventQueueRun() == 0) {
            return;
        }
        if (tgtSelectState.lastObject->flags & TGT_OBJECT_TURN_DONE) {
            tgtSelectState.phase = TGT_PHASE_TURN_END;
            return;
        }
        tgtSelectState.phase = TGT_PHASE_RUN;
        return;
    case TGT_PHASE_TURN_END:
        if (unitNumFlag1Flag4ClearGet(TGT_SIDE_ALL) != 0) {
            return;
        }
        tgtSelectTurnEnd(state->lastObject);
        menuPanelHide();
        unitValueRefresh(state->lastObject);
        event.unit = state->current;
        unitScriptEventSend(5, &event);
        actStepCounterAdvance();
        dataEntryBlockReset();
        if (state->lastObject->stats->key < TGT_PARTY_KEY_LIMIT) {
            state->lastPartyUnit = state->lastObject;
        }
        state->flags = (state->flags & ~TGT_STAGE_EXECUTE) | TGT_STAGE_BEGIN;
        state->lastObject = 0;
        state->phase = TGT_PHASE_RUN;
        state->object = 0;
        state->commandKind = 0;
        state->hitDamage = 0;
        return;
    }
}



/* Sets the battle status (TGT_STATUS_*) unless the execute stage runs; 1 when the
 * battle is over. */
int tgtSelectStatusUpdate(void)
{
    BattleState *state = &tgtSelectState;

    state->status = 0;
    if (state->flags & TGT_STAGE_EXECUTE) {
        return 0;
    }
    if ((cfEventGet() & CF_EVENT_TIME_LIMIT) && gameWork.panelValue <= 0) {
        state->status = TGT_STATUS_TIME_UP;
        return 1;
    }
    if (unitNumFlag4ClearStatZeroGet(TGT_SIDE_PARTY) == 0) {
        tgtSelectState.status = TGT_STATUS_PARTY_GONE;
        return 1;
    }
    if (unitNumFlag4ClearStatZeroGet(TGT_SIDE_ENEMY) == 0) {
        tgtSelectState.status = TGT_STATUS_ENEMY_GONE;
        return 1;
    }
    if (tgtSelectState.transition == 1) {
        tgtSelectState.status = TGT_STATUS_PARTY_LEFT;
        return 1;
    }
    if (tgtSelectState.transition == 2) {
        tgtSelectState.status = TGT_STATUS_ENEMY_LEFT;
        return 1;
    }
    return 0;
}

/* Returns the status word at state+0x30 (codes 1 to 5), which battleEndCheck switches on. */
int tgtSelectStatusGet(void)
{
    return tgtSelectStatus;
}

/* StatBlock.state70 values whose units get an end-of-turn command. */
#define TGT_UNIT_STATE_RATIO 2
#define TGT_UNIT_STATE_PANEL 3
/* menuUnitPanelOpen mode of the UNIT_CMD_MOVE_RAMP command TGT_UNIT_STATE_PANEL units get. */
#define TGT_PANEL_MODE 2

int unitSidesDiffer(StatOwner *unitA, StatOwner *unitB);
void battleEntryClearReset(StatOwner *owner);
void battleStateClearWords(StatOwner *owner);
int unitStateAdvance(StatOwner *owner);
int unitEntriesAllMarked(StatOwner *owner);

/* End of a turn of unit: unless the boost unit is on the unit's side, every unit not
 * held is reset and steps its state (units in state 2 or 3 first get their command);
 * then the target's panels follow its state. */
void tgtSelectTurnEnd(Ov01Object *unit)
{
    Ov01Command command;
    Ov01Object **units;
    Ov01Object *boost;
    Ov01Object *target;
    int count;
    int i;
    int state;

    boost = actStockGet()->boost[1];
    if (boost == 0 || unitSidesDiffer(boost, unit) != 0) {
        count = unitTblGet(TGT_SIDE_ALL, &units);
        for (i = 0; i < count; i++) {
            if (units[i] == 0) {
                continue;
            }
            if (units[i]->flags & UNIT_FLAG_DOWN) {
                continue;
            }
            battleEntryClearReset(units[i]);
            battleStateClearWords(units[i]);
            state = units[i]->stats->state70;
            if (state == TGT_UNIT_STATE_RATIO) {
                command.command = UNIT_CMD_MOVE_RATIO;
                unitQueueClear(units[i]);
                unitQueueAppend(units[i], &command);
            } else if (state == TGT_UNIT_STATE_PANEL) {
                command.command = UNIT_CMD_MOVE_RAMP;
                unitQueueClear(units[i]);
                unitQueueAppend(units[i], &command);
                menuUnitPanelOpen(units[i], TGT_PANEL_MODE);
            }
            unitStateAdvance(units[i]);
        }
    }
    target = tgtUnitGet();
    if (target == 0) {
        return;
    }
    state = target->stats->state70;
    if (state == 0) {
        menuUnitPanelArmTimer();
    } else {
        menuIconListClose();
    }
    if (unitEntriesAllMarked(target) == 0) {
        battleEntryClearReset(target);
        menuIconListClose();
    }
    if (target->stats->comboHits < 2) {
        menuTimedSeqArmTick();
    }
}

/* Buttons that open the boost menu while held, and the effect a boost starts. */
#define TGT_BOOST_HELD (PAD_R2 | PAD_R1)
#define TGT_BOOST_EFFECT_HIGH 0x140
#define TGT_BOOST_EFFECT_LOW 7
/* seVoiceStartTagged tag and id of the boost voice. */
#define TGT_BOOST_VOICE_TAG 2
#define TGT_BOOST_VOICE_ID 1

void menuCursorShow(void);
void menuCursorHide(void);

/* Boost input: while R1 or R2 is held the boost cursor shows and square, triangle or
 * circle boosts party unit 0, 1 or 2 (when calcBoostOk allows it); 1 while the boost
 * menu is open, else the cursor is hidden and 0 returned. */
int tgtSelectPadInput(void)
{
    Ov01Object **units;
    unsigned short pressed;
    int slot;

    pressed = dataPadRead(1);
    if (dataPadRead(0) & TGT_BOOST_HELD) {
        menuCursorShow();
        slot = 0;
        if (!(pressed & PAD_SQUARE)) {
            slot = 1;
            if (!(pressed & PAD_TRIANGLE)) {
                slot = (pressed & PAD_CIRCLE) ? 2 : -1;
            }
        }
        unitTblGet(TGT_SIDE_PARTY, &units);
        if (slot != -1) {
            if (units[slot] != 0 && calcBoostOk(units[slot], 1) != 0) {
                calcActStockSet(units[slot], 1);
                unitEffectStart(units[slot], TGT_BOOST_EFFECT_HIGH, TGT_BOOST_EFFECT_LOW);
                seVoiceStartTagged(TGT_BOOST_VOICE_TAG, TGT_BOOST_VOICE_ID);
            }
        }
        return 1;
    }
    menuCursorHide();
    return 0;
}

void tgtSelectUnhighlight(Ov01Object *unit)
{
    tgtSelectCurrent->flags &= ~TGT_OBJECT_HIGHLIGHT;
    thinkSendEnd(unit);
}

/* StatBlock row-1 status bit that lets an automatic candidate give up its turn. */
#define TGT_STAT1_SKIP 0x400
/* Percent chance of that. */
#define TGT_SKIP_PERCENT 20

void tgtSelectUnhighlight(Ov01Object *unit);

/* Automatic candidate: in mode 0 a unit with the skip status may give up its turn (20 %);
 * otherwise its think request is queued and mode 1 waits; in a later mode the candidate
 * is confirmed (highlight dropped) once no party unit is busy. */
int tgtSelectAuto(Ov01Object *object)
{
    UnitScriptPairEvent request;

    switch (tgtSelectState.mode) {
    case 0:
        if (calcStatGet(object, 1, TGT_STAT1_SKIP) != 0 && ov01RollPercent(TGT_SKIP_PERCENT) != 0) {
            tgtSelectUnhighlight(object);
            return 1;
        }
        request.unit = object;
        request.other = 0;
        unitScriptPairEventSend(1, &request);
        tgtSelectState.mode = 1;
        break;
    case 1:
        break;
    default:
        if (unitNumBusyGet(TGT_SIDE_PARTY) > 0) {
            return 0;
        }
        tgtSelectState.current->flags &= ~TGT_OBJECT_HIGHLIGHT;
        return 1;
    }
    return 0;
}

void tgtSelectModeEnd(void)
{
    tgtSelectMode = 2;
}

/* BattleState.selection value that leaves the menu without a command. */
#define TGT_SELECTION_BACK 0x15

int dataUnitListInit(void **selected, int side);
int dataUnitCursorStep(void **out);

/* Manual candidate: opens the target menu on the first call (the unit list of the
 * candidate's side), runs the battle menu and, once a command was chosen, queues it;
 * the think-end command also drops the highlight. 1 when a command was queued. */
int tgtSelectManual(Ov01Object *object)
{
    CmdListBody command;
    void *target;
    int party;

    party = object->stats->key < TGT_PARTY_KEY_LIMIT;
    target = tgtUnitGet();
    if (tgtSelectState.menuMode == 0) {
        dataStateSetMode(1, 0);
        dataUnitListInit(&target, party);
        dataUnitCursorStep(&target);
        tgtUnitSet(target);
        dataTargetShow(target, 0);
    }
    battleSelectStep(object);
    if (tgtSelectState.selection != 0 && tgtSelectState.selection != TGT_SELECTION_BACK) {
        battleSelectionGet(object, &command, target);
        cmdListSet(object, &command);
        if (command.command == CMD_KIND_THINK_END) {
            tgtSelectState.current->flags &= ~TGT_OBJECT_HIGHLIGHT;
        }
        return 1;
    }
    return 0;
}

TgtSlot ov01PartyTable[4] = {
    { 1, 0, 180 },
    { 4, 1, 180 },
    { 5, 2, 180 },
    { 0 },
};

BattleState tgtSelectState;
