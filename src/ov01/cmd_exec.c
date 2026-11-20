#include "common.h"
#include "main/text_marker.h"
#include "main/sef.h"
#include "main/xenoActor.h"
#include "ov01/mons_slot_run.h"
#include "ov01/obj_slot.h"
#include "ov01/data_file_load.h"
#include "main/game_camera.h"
#include "ov01/data_unit_org_get.h"
#include "ov01/unit_cmd.h"
#include "ov01/mons_slot.h"
#include "ov01/unit_tbl.h"
#include "ov01/unit_fade.h"
#include "ov01/mons_slot_show.h"
#include "ov01/calc.h"
#include "ov01/cmd_exec.h"
#include "ov01/data_record_get.h"
#include "ov01/think_dispatch.h"
#include "ov01/data_box.h"
#include "ov01/menu.h"
#include "ov01/choice_menu.h"
#include "ov01/scene_load.h"
#include "ov01/menu_unit_panel_open.h"
#include "ov01/menu_icon_list.h"
#include "ov01/menu_combo_panel.h"
#include "ov01/menu_panel.h"
#include "ov01/deco_flags.h"
#include "ov01/screen_flash.h"
#include "ov01/screen_fill.h"
#include "ov01/handle_state.h"
#include "ov01/snd_play.h"

extern char cmdList[];   /* cmdList: 0x20-byte entries, unit then command body */
/* The command fifo ring (struct FifoRing, ov01/obj_slot.h). */
extern struct FifoRing cmdFifo;   /* the fifo over the cmdList entries */

/* Command list entries (0x20 bytes each); the fifo ring runs over them. */
#define CMD_LIST_COUNT 0x20
#define CMD_LIST_ENTRY_SIZE 0x20

/* Records of eventQueueTable (eventQueueReserve stops at this count). */
#define EVENT_QUEUE_COUNT 0x40

extern EventQueueEntry eventQueueTable[EVENT_QUEUE_COUNT];
extern int eventQueueCount;
extern int eventQueueCursor;
/* Cleared with the event queue: eventRetryCount counts the runs of a record whose
 * handler is not done yet, eventStepStarted is a handler's own flag (unitBindRequest
 * sets it once the bind load runs); both are cleared when a record is done. */
extern int eventRetryCount;
extern int eventStepStarted;

void *memset(void *destination, int value, unsigned int count);

/* cmdSystemInit: clears the command list and fifo, the event table and its counters. The four counters are
 * cleared in one chained assignment: as four statements gcc loads their addresses in the other order. */
void cmdSystemInit(void)
{
    memset(cmdList, 0, CMD_LIST_COUNT * CMD_LIST_ENTRY_SIZE);
    fifoInit(&cmdFifo, CMD_LIST_COUNT);
    memset(eventQueueTable, 0, sizeof(eventQueueTable));
    eventStepStarted = eventRetryCount = eventQueueCursor = eventQueueCount = 0;
}

/* cmdListPtrGet: the command fifo and the command list, through two out pointers. */
void cmdListPtrGet(void **fifo_out, void **list_out) {
    *fifo_out = &cmdFifo;
    *list_out = cmdList;
}

/* BattleUnit.flags bit of a held (paused) unit. */

/* One 0x20-byte cmdList entry: the unit, then the command body at +0x08. The
 * unit is kept as a word, as XS1's CmdListEntry (battle_init.c) keeps it. */
typedef struct CmdListEntry {
    int unit;                         /* +0x00 */
    unsigned char unmodeled_04[0x04];
    CmdListBody body;                 /* +0x08 */
} CmdListEntry;

/* cmdListSet: queues one unit's command record at the fifo's write position. */
void cmdListSet(BattleUnit *unit, CmdListBody *command)
{
    void *fifo;
    void *list;
    CmdListEntry *entry;

    cmdListPtrGet(&fifo, &list);
    /* XS1 cmdListSet's form: the byte offset plus the list address as integers.
     * With pointer arithmetic (list + index) the unit store is scheduled after
     * the body copy. */
    entry = (CmdListEntry *)((char *)(fifoEdGet(fifo) * sizeof(CmdListEntry)) + (int)list);
    entry->unit = (int)unit;
    entry->body = *command;
    fifoPush(fifo);
}

int cmdExecute(BattleUnit *unit, CmdListBody *command);

/* battleExecNorm: clears the event queue counters, then runs the head command of the list unless its unit is
 * down and pops it; returns 0 when the list is empty, else 1. */
int battleExecNorm(void)
{
    void *fifo;
    void *list;
    CmdListEntry *entry;
    BattleUnit *unit;
    CmdListBody *body;

    /* One chained assignment: four separate stores give the same stores with the lui order reversed. */
    eventStepStarted = eventRetryCount = eventQueueCursor = eventQueueCount = 0;
    cmdListPtrGet(&fifo, &list);
    if (fifoNumGet(fifo) == 0) {
        return 0;
    }
    /* cmdListSet's integer form; indexing the list gives list + offset instead of offset + list. */
    entry = (CmdListEntry *)((char *)(fifoStGet(fifo) * sizeof(CmdListEntry)) + (int)list);
    unit = (BattleUnit *)entry->unit;
    body = &entry->body;
    if (!(unit->flags & UNIT_FLAG_DOWN)) {
        cmdExecute(unit, body);
    }
    fifoPop(fifo);
    return 1;
}

/* cmdExecute: runs one command body, a switch on its kind, queueing events on the way. */
/* The 0x20-byte command record cmdExecute builds on its frame: queued as a CmdListBody (eventQueueAddUnitCommand) and
 * run as the unit's Ov01Command. UNIT_CMD_TURN_AIM carries its aim point at +0x8 and, at +0x14
 * (Ov01Command.poseIndex, read by unitCmdTurn), whether the unit turns to that point. */
typedef union UnitCommandRecord {
    CmdListBody body;
    Ov01Command full;
    struct {
        int command;
        int state;
        float point[3];
        int toPoint;
        int pad[2];
    } aim;
} UnitCommandRecord;

/* The key row entry a unit takes over as its technique when calcStatusHitRoll turns a command on it. */
#define KEY_ROW_SWAP_CODE 2
/* A unit that only poses in place instead of moving to its target: a target in battle state 3, or a target with
 * default code bit 0 against a technique with hit flag 0x200 used by unit key 2 or 4. */
#define TARGET_STATE_STAND_OFF 3
#define TARGET_DEFAULT_CODE_STAND_OFF 1
#define TEC_HIT_STAND_OFF 0x200
#define STAND_OFF_UNIT_KEY_A 4
#define STAND_OFF_UNIT_KEY_B 2
/* Techniques with these codes make the unit wait for code step CODE_WAIT_STEP first (UNIT_CMD_CODE_WAIT). */
#define TEC_CODE_WAIT_FIRST 0x22
#define TEC_CODE_WAIT_COUNT 4
#define CODE_WAIT_STEP 4
/* The move commands (UNIT_CMD_MOVE_CHA .. UNIT_CMD_MOVE_POS_HOVER) a target may still be running. */
#define UNIT_CMD_MOVES_COUNT (UNIT_CMD_MOVE_POS_HOVER - UNIT_CMD_MOVE_CHA + 1)
/* StatBlock.facing is in radians; the spawn request takes the angle in whole degrees. */
#define RADIANS_PER_DEGREE 0.017453292f

void eventQueueAdd(int kind, EventQueueArgs *args);
void eventQueueAddUnitCommand(BattleUnit *object, CmdListBody *args);
void cmdAttackQueueAndSpend(BattleUnit *unit, CmdListBody *command);
void cmdStockRun(BattleUnit *unit, CmdListBody *command, EventQueueArgs *args);
void cmdPairRun(BattleUnit *unit, CmdListBody *command, EventQueueArgs *args);
BattleUnit *cmdFindFirstTarget(BattleUnit *unit, CmdListBody *body);

/*
 * Runs one command body of unit (the head of the command list) and queues the events and unit commands it needs.
 * A technique, lookup or item command finds its first target (calcStatusHitRoll may turn a technique on the
 * attacker: the target then acts with its key row technique), moves or poses the unit, aims a technique at all
 * targets, and runs the action through the pair, the stock or the plain path. The think end brings the unit back,
 * a swap spawns the unit's replacement, a leave command goes to every unit of the side. Returns 0 when a command
 * finds no target, else 1.
 * Forms that keep the original's code: every case ends with break and one return 1 follows the switch (the case
 * tails then cross-jump as in the original), the think end sets code before command, and the swap reads all its
 * values before it stores them.
 */
int cmdExecute(BattleUnit *unit, CmdListBody *body)
{
    UnitCommandRecord command;
    EventQueueArgs args;
    BattleUnit **units;
    BattleUnit *target;
    BattleUnit *other;
    BattleUnit *swap;
    Ov01Command *current;
    StatBlock *stats;
    float *point;
    TableKeyRow *keyRow;
    int flagged;
    int count;
    int found;
    int code;
    int index;
    int angle;
    int i;
    int swapped;

    switch (body->command) {
    case CMD_KIND_TECHNIQUE:
    case CMD_KIND_LOOKUP:
    case CMD_KIND_ITEM:
        args.unit = unit;
        eventQueueAdd(EVENT_KIND_WAIT_UNIT_QUEUE, &args);
        if (body->command == CMD_KIND_TECHNIQUE) {
            body->code = battleEffectResolve(unit, body->code, (BattleUnit *)body->arg.target);
        }
        target = cmdFindFirstTarget(unit, body);
        if (target == 0) {
            return 0;
        }
        if (body->command == CMD_KIND_TECHNIQUE && calcStatusHitRoll(unit, target, body->code) != 0) {
            swap = target;
            target = unit;
            unit = swap;
            body->arg.target = (int)target;
            swapped = 1;
            keyRow = tableKeyRowGet(unit->stats->key);
            body->code = keyRow->entries[KEY_ROW_SWAP_CODE];
            unit->stats->rollPercent = 0;
            menuPanelShow();
            target = cmdFindFirstTarget(unit, body);
            if (target == 0) {
                return 0;
            }
        } else {
            swapped = 0;
        }
        unit->commandTarget = (int)target;
        unit->commandCode = body->code;
        if (body->command != CMD_KIND_ITEM) {
            flagged = tecRecordGet(body->code)->flags & TEC_FLAG_SCRIPT_FLAGGED;
            if (flagged == 0
                && (target->stats->state70 == TARGET_STATE_STAND_OFF
                    || ((target->stats->defaultCode & TARGET_DEFAULT_CODE_STAND_OFF)
                        && (tecRecordGet(body->code)->hitFlags & TEC_HIT_STAND_OFF)
                        && (unit->stats->key == STAND_OFF_UNIT_KEY_A || unit->stats->key == STAND_OFF_UNIT_KEY_B)))) {
                if (!(unit->flags & UNIT_FLAG_POSED_OUT)) {
                    if (unit->flags & UNIT_FLAG_MOVED_OUT) {
                        command.body.command = UNIT_CMD_PHASE_0E_WAIT;
                        eventQueueAddUnitCommand(unit, &command.body);
                    } else {
                        command.body.command = UNIT_CMD_POSE_ARRIVE;
                        eventQueueAddUnitCommand(unit, &command.body);
                    }
                    unit->flags |= UNIT_FLAG_POSED_OUT;
                }
            } else if (flagged == 0 && !(unit->flags & (UNIT_FLAG_MOVED_OUT | UNIT_FLAG_POSED_OUT))) {
                command.body.command = UNIT_CMD_MOVE_CHA;
                eventQueueAddUnitCommand(unit, &command.body);
                unit->flags |= UNIT_FLAG_MOVED_OUT;
            }
            if (body->command == CMD_KIND_TECHNIQUE && flagged != 0
                && tecRecordGet(body->code)->gateFlag != TEC_GATE_PAIR) {
                if (!(tecRecordGet(body->code)->flags & TEC_TARGET_ALL)) {
                    command.aim.command = UNIT_CMD_TURN_AIM;
                    command.aim.toPoint = 0;
                    eventQueueAddUnitCommand(unit, &command.body);
                } else if (unit->stats->key < UNIT_KEY_OTHER_FIRST && tecRecordGet(body->code)->code == 0) {
                    /* Aims at the middle of the targets the technique applies to. */
                    point = command.aim.point;
                    found = 0;
                    point[0] = point[1] = point[2] = 0.0f;
                    count = unitTblGet(calcTrg2PE(unit, tecRecordGet(body->code)->flags), &units);
                    for (i = 0; i < count; i++) {
                        other = units[i];
                        if (other == 0 || (other->flags & UNIT_FLAG_DOWN)) {
                            continue;
                        }
                        if (battleEffectApplies(unit, body->code, other) != 0) {
                            found++;
                            point[0] += units[i]->pose.position.x;
                            point[2] += units[i]->pose.position.z;
                        }
                    }
                    point[0] /= (float)found;
                    point[2] /= (float)found;
                    command.aim.command = UNIT_CMD_TURN_AIM;
                    command.aim.toPoint = 1;
                    eventQueueAddUnitCommand(unit, &command.body);
                }
                if ((unsigned int)(body->code - TEC_CODE_WAIT_FIRST) < TEC_CODE_WAIT_COUNT) {
                    command.full.command = UNIT_CMD_CODE_WAIT;
                    command.full.code = CODE_WAIT_STEP;
                    command.full.arg.wait = 1;
                    eventQueueAddUnitCommand(unit, &command.body);
                }
                current = objCmdPtrGet(&target->commandQueue);
                if (current != 0 && (unsigned int)(current->command - UNIT_CMD_MOVE_CHA) < UNIT_CMD_MOVES_COUNT) {
                    args.unit = target;
                    eventQueueAdd(EVENT_KIND_WAIT_UNIT_QUEUE, &args);
                }
            }
            args.unit = unit;
            eventQueueAdd(EVENT_KIND_WAIT_UNIT_QUEUE, &args);
        }
        args.unit = unit;
        args.tecIndex = body->code;
        args.pairIndex = body->pairIndex;
        eventQueueAdd(EVENT_KIND_ACTION_LOAD, &args);
        eventQueueAdd(EVENT_KIND_LOADS_IDLE, &args);
        eventQueueAdd(EVENT_KIND_ATTACH, &args);
        if (tecRecordGet(body->code)->gateFlag == TEC_GATE_PAIR) {
            cmdPairRun(unit, body, &args);
            actEntryFlagSet(body->pairIndex);
        } else if (tecRecordGet(body->code)->gateFlag == TEC_GATE_STOCK) {
            cmdStockRun(unit, body, &args);
        } else {
            eventQueueAddUnitCommand(unit, body);
            if ((unsigned int)(body->command - CMD_KIND_LOOKUP) <= CMD_KIND_ITEM - CMD_KIND_LOOKUP) {
                eventQueueAdd(EVENT_KIND_WAIT_UNIT_QUEUE, &args);
            }
            cmdAttackQueueAndSpend(unit, body);
            statEffOff(unit);
            if (tecRecordGet(body->code)->gateFlag == TEC_GATE_SLOT_MARK) {
                calcEtherDisableSet(body->pairIndex);
            }
        }
        if (swapped == 1) {
            command.body.command = CMD_KIND_THINK_END;
            eventQueueAddUnitCommand(unit, &command.body);
        }
        break;
    case CMD_KIND_THINK_END:
        if (unit->flags & UNIT_FLAG_POSED_OUT) {
            command.full.code = unit->stats->posIndex;
            command.full.command = UNIT_CMD_POSE_ARRIVE_WAIT;
            eventQueueAddUnitCommand(unit, &command.body);
        } else if (unit->flags & UNIT_FLAG_MOVED_OUT) {
            command.full.code = unit->stats->posIndex;
            command.full.command = UNIT_CMD_MOVE_POS;
            eventQueueAddUnitCommand(unit, &command.body);
        }
        unit->flags &= ~UNIT_FLAGS_ACTION;
        eventQueueAddUnitCommand(unit, body);
        eventQueueAdd(EVENT_KIND_RESOLVE_STATUS, 0);
        eventQueueAdd(EVENT_KIND_REMOVE_DEFEATED, 0);
        eventQueueAdd(EVENT_KIND_STATUS_QUEUE_WAIT, 0);
        break;
    case CMD_KIND_SWAP:
        eventQueueAddUnitCommand(unit, body);
        args.unit = unit;
        eventQueueAdd(EVENT_KIND_WAIT_UNIT_QUEUE, &args);
        code = body->code;
        stats = unit->stats;
        angle = stats->facing / RADIANS_PER_DEGREE;
        index = stats->posIndex;
        args.actions = code;
        args.tecIndex = index;
        args.pairIndex = angle;
        eventQueueAdd(EVENT_KIND_SPAWN, &args);
        eventQueueAdd(EVENT_KIND_LOADS_IDLE, &args);
        eventQueueAdd(EVENT_KIND_BIND, &args);
        command.body.command = UNIT_CMD_STREAM_PLAY;
        eventQueueAddUnitCommand(unit, &command.body);
        break;
    case CMD_KIND_LEAVE:
        count = unitTblGet(unit->stats->key >= UNIT_KEY_OTHER_FIRST, &units);
        for (i = 0; i < count; i++) {
            if (units[i] != 0) {
                eventQueueAddUnitCommand(units[i], body);
            }
        }
        break;
    default:
        eventQueueAddUnitCommand(unit, body);
        break;
    }
    return 1;
}

BattleUnit *unitPairPartnerFind(BattleUnit *unit, int value);

/* Unit command indices (CmdListBody.command) the two command handlers below queue. */
#define CMD_EVENT_MOTION_3 3
#define CMD_EVENT_MOTION_9 9

/* A pair command: queues stream open, loads idle, stream bind and pair begin (args target
 * set to the partner unitPairPartnerFind finds for the command's pair record), the command
 * for the unit and the partner, motion 9 for the partner, the follow-up and the pair end. */
void cmdPairRun(BattleUnit *unit, CmdListBody *command, EventQueueArgs *args)
{
    BattleUnit *partner;
    CmdListBody motion;

    partner = unitPairPartnerFind(unit, command->pairIndex);
    eventQueueAdd(EVENT_KIND_STREAM_OPEN, args);
    eventQueueAdd(EVENT_KIND_LOADS_IDLE, args);
    eventQueueAdd(EVENT_KIND_STREAM_BIND, args);
    args->target = partner;
    eventQueueAdd(EVENT_KIND_PAIR_BEGIN, args);
    eventQueueAddUnitCommand(unit, command);
    eventQueueAddUnitCommand(partner, command);
    motion.command = CMD_EVENT_MOTION_9;
    eventQueueAddUnitCommand(partner, &motion);
    cmdAttackQueueAndSpend(unit, command);
    eventQueueAdd(EVENT_KIND_PAIR_END, args);
}


/* A stock command: the stocked unit (args target) gets the stock begin and motion 3,
 * pays the EP calcTakeEpGet gives for the args tec, then the command runs for the unit
 * (wait for its queue, then the follow-up) and the stocked unit ends with motion 9 and
 * the boost clear. */
void cmdStockRun(BattleUnit *unit, CmdListBody *command, EventQueueArgs *args)
{
    BattleUnit *stocked;
    CmdListBody motion;

    stocked = actStockGet()->boost[1];
    args->target = stocked;
    eventQueueAdd(EVENT_KIND_STOCK_BEGIN, args);
    motion.command = CMD_EVENT_MOTION_3;
    eventQueueAddUnitCommand(stocked, &motion);
    calcEp(stocked, -calcTakeEpGet(stocked, args->tecIndex));
    eventQueueAddUnitCommand(unit, command);
    eventQueueAdd(EVENT_KIND_WAIT_UNIT_QUEUE, args);
    cmdAttackQueueAndSpend(unit, command);
    motion.command = CMD_EVENT_MOTION_9;
    eventQueueAddUnitCommand(stocked, &motion);
    eventQueueAdd(EVENT_KIND_STOCK_BOOST_CLEAR, args);
}


/*
 * cmdFindFirstTarget (proposed cmdFindFirstTarget): builds the task record of a command
 * body and returns the first target tgtFindFirst finds for it. Only cmdExecute calls it.
 */
BattleUnit *cmdFindFirstTarget(BattleUnit *unit, CmdListBody *body)
{
    struct AttackCommand record;
    int id;
    int kind;

    record.attacker = unit;
    record.actionIdx = body->code;
    id = body->pairIndex;
    kind = body->attackKind;
    record.id = id;
    record.kind = kind;
    return tgtFindFirst(&record, (BattleUnit *)body->arg.target);
}

int cmdTechniqueAttackQueue(struct AttackCommand *owner, struct BattleUnit *value);
void cmdItemAttackQueue(struct AttackCommand *owner, struct BattleUnit *value);

/* Boost progress a technique command of gateFlag TEC_GATE_NONE, 6, 7 or 8 gives its unit: CMD_BOOST_PROGRESS, on
 * act step CMD_BOOST_ACT_STEP CMD_BOOST_PROGRESS_STEP, or CMD_BOOST_PROGRESS_ROLLED when the CMD_BOOST_ROLL_PERCENT
 * roll fails. */
#define CMD_BOOST_PROGRESS 10
#define CMD_BOOST_ACT_STEP 1
#define CMD_BOOST_PROGRESS_STEP 50
#define CMD_BOOST_PROGRESS_ROLLED 30
#define CMD_BOOST_ROLL_PERCENT 10
/* The current value a large unit (UNIT_KEY_LARGE_*) gets back for a technique command of gateFlag 6..8. */
#define CMD_LARGE_CURRENT_BONUS 25

/* Runs a command of unit against its target (commandTarget): an item through cmdItemAttackQueue, anything else through
 * cmdTechniqueAttackQueue. Then the boost progress of a technique, the EP cost, the level record's cost for TEC_GATE_SHOW_CHILD,
 * and the large unit's current bonus. */
void cmdAttackQueueAndSpend(BattleUnit *unit, CmdListBody *command)
{
    struct AttackCommand owner;
    TecRecord *tec;
    int boost;
    int target;

    target = unit->commandTarget;
    owner.attacker = unit;
    owner.actionIdx = command->code;
    owner.id = command->pairIndex;
    if (command->command == CMD_KIND_ITEM) {
        owner.kind = command->command;
        cmdItemAttackQueue(&owner, (BattleUnit *)target);
    } else {
        owner.kind = command->command;
        cmdTechniqueAttackQueue(&owner, (BattleUnit *)target);
    }
    tec = tecRecordGet(owner.actionIdx);
    if (command->command == CMD_KIND_TECHNIQUE) {
        if (tec->gateFlag == TEC_GATE_NONE || tec->gateFlag == 6 || tec->gateFlag == 7 || tec->gateFlag == 8) {
            boost = CMD_BOOST_PROGRESS;
            if (actStepCounterValue() == CMD_BOOST_ACT_STEP) {
                boost = CMD_BOOST_PROGRESS_STEP;
                if (ov01RollPercent(CMD_BOOST_ROLL_PERCENT) == 0) {
                    boost = CMD_BOOST_PROGRESS_ROLLED;
                }
            }
            calcBp(unit, boost);
        }
    }
    calcEp(unit, -calcTakeEpGet(unit, owner.actionIdx));
    if (tec->gateFlag == TEC_GATE_SHOW_CHILD) {
        unitCurrentAdd(unit, -calcLevelRecordGet(owner.id)->requiredLevel);
    }
    if (unit->stats->key >= UNIT_KEY_LARGE_FIRST && unit->stats->key < UNIT_KEY_LARGE_FIRST + UNIT_KEY_LARGE_COUNT) {
        if (command->command == CMD_KIND_TECHNIQUE) {
            if (tec->gateFlag >= 6 && tec->gateFlag <= 8) {
                unitCurrentAdd(unit, CMD_LARGE_CURRENT_BONUS);
            }
        }
    }
}


int unitJobTypeAtk(struct AttackCommand *command, struct BattleUnit *target, TecRecord *tec);

int cmdTechniqueAttackQueue(struct AttackCommand *owner, struct BattleUnit *value) {
    TecRecord *entry;
    int slot;
    int result;

    entry = tecRecordGet(owner->actionIdx);
    slot = entry->kindBits.slot;
    result = slot < 8;
    if (result != 0 && slot >= 0) {
        result = unitJobTypeAtk(owner, value, entry);
    }
    return result;
}

void cmdItemAttackQueue(struct AttackCommand *owner, struct BattleUnit *value) {
    unitJobTypeAtk(owner, value, tecRecordGet(owner->actionIdx));
}

/* HitRecord.hitMode: the hit kind in the low byte, the flags byte above it. */
#define HIT_MODE_KIND_MASK 0xFF
#define HIT_MODE_FLAGS_SHIFT 8
#define HIT_MODE_FLAGS_MASK 0xFF
/* The hit kinds, as calcHpDmg applies them: no hit (no damage, nothing inflicted), HP and EP loss and gain, two
 * kinds that change neither (calcStatAtk skips HIT_KIND_NO_INFLICT), HP and EP taken over by the attacker
 * (calcDmg: a share of the target's maximum up to its current value, or its maximum but 1), and the kind
 * unitJobTypeAtk gives the hit of a technique without a hit kind. */
#define HIT_KIND_NONE 0
#define HIT_KIND_PLAIN 1
#define HIT_KIND_HP_GAIN 2
#define HIT_KIND_EP_LOSS 3
#define HIT_KIND_EP_GAIN 4
#define HIT_KIND_STATUS 5
#define HIT_KIND_NO_INFLICT 6
#define HIT_KIND_HP_DRAIN_SHARE 7
#define HIT_KIND_HP_DRAIN_MAX 8
#define HIT_KIND_EP_DRAIN_SHARE 9
#define HIT_KIND_EP_DRAIN_MAX 10
#define HIT_KIND_TEC_NONE 0xB
/* The flags byte of HitRecord.hitMode (bits 8 to 15): bit 8 taken, bit 9 blocked. */
#define HIT_FLAG_TAKEN 0x1
#define HIT_FLAG_BLOCKED 0x2

int actionTransformSlotGet(int id);
void eventQueueAddAttackAction(struct AttackCommand *command);
void eventQueueAddAttackHit(struct HitRecord *args);
extern int attackStartedState;

/* Queues an attack: a kind-5 record per target (tgtFindNext walks them) with its hit record and the unit command
 * the target answers with, the attack itself as a kind-4 record (twice with the partner for a pair technique), and
 * a kind-3 record with both counts. Returns 0 for an unknown hit kind. */
int unitJobTypeAtk(struct AttackCommand *command, struct BattleUnit *target, TecRecord *tec)
{
    struct HitRecord hit;
    EventQueueArgs counts;
    struct AttackCommand pair;
    int kind;
    int flags;
    int hits;

    hits = 0;
    do {
        if (tec->kindBits.kind != 0) {
            hit.target = target;
            calcHitDmg(command, &hit);
            kind = hit.hitMode.word & 0xFF;
            flags = (hit.hitMode.word >> 8) & 0xFF;
            manWorkGet()->hitDamage = hit.damage;
            manWorkGet()->hitMode = hit.hitMode.word;
            switch (kind) {
            case 0:
                hit.reaction = UNIT_CMD_JUMP_EFFECT;
                break;
            case 1:
            case 3:
            case 7:
            case 8:
            case 9:
            case 10:
                if (flags & 2) {
                    hit.reaction = UNIT_CMD_HOVER_EFFECT;
                } else {
                    hit.reaction = UNIT_CMD_MODE_DISPATCH;
                }
                break;
            case 2:
            case 4:
                if (flags & 8) {
                    hit.reaction = UNIT_CMD_PHASE_2B_WAIT;
                } else {
                    hit.reaction = UNIT_CMD_CLEAR_ACTIVE;
                }
                break;
            case 5:
            case 6:
                hit.reaction = UNIT_CMD_DEACTIVATE;
                break;
            default:
                return 0;
            }
            target->stats->source = command->attacker;
            target->stats->sourceAction = command->actionIdx;
        } else {
            memset(&hit, 0, sizeof(hit));
            hit.target = target;
            hit.reaction = 0;
            hit.hitMode.word = (hit.hitMode.word & 0xFF00) | 0xB;
        }
        eventQueueAddAttackHit(&hit);
        hits++;
    } while ((target = tgtFindNext()) != 0);
    eventQueueAddAttackAction(command);
    kind = 1;
    command->attacker->stats->valueLevel += unitRecordStateGet(command->attacker, command->actionIdx);
    if (tecRecordGet(command->actionIdx)->gateFlag == 1) {
        kind = 2;
        pair = *command;
        pair.attacker = unitPairPartnerFind(command->attacker, command->id);
        eventQueueAddAttackAction(&pair);
    }
    if (actionTransformSlotGet(command->actionIdx) != 0) {
        attackStartedState = 1;
    } else {
        attackStartedState = 0;
    }
    counts.actions = kind;
    counts.hits = hits;
    eventQueueAdd(EVENT_KIND_ATTACK, &counts);
    return 1;
}

/* The partner of unit in pair record value: the first unit of side 0 that is
 * not held and has the other key of the pair; 0 when there is none. */
BattleUnit *unitPairPartnerFind(BattleUnit *unit, int value)
{
    PairLink *pair;
    BattleUnit **units;
    BattleUnit *other;
    int key;
    int count;
    int i;

    pair = pairLinkGet(value);
    if (pair->slotFirst == unit->stats->key) {
        key = pair->slotSecond;
    } else {
        key = pair->slotFirst;
    }
    count = unitTblGet(0, &units);
    for (i = 0; i < count; i++) {
        other = units[i];
        if (other != 0 && !(other->flags & UNIT_FLAG_DOWN)) {
            if (other->stats->key == key) {
                return other;
            }
        }
    }
    return 0;
}

/* For the command kinds CMD_KIND_TECHNIQUE, CMD_KIND_LOOKUP and CMD_KIND_ITEM: whether the sef entry of the
 * attacker's actor has flag 1; 0 otherwise. */
int attackSefFlag1Check(BattleUnit *attacker, int kind)
{
    if (kind != CMD_KIND_TECHNIQUE && kind != CMD_KIND_LOOKUP && kind != CMD_KIND_ITEM) {
        return 0;
    }
    return sefIsHitActor(attacker->unit) != 0;
}

/* Queues the reaction command of hit for its target (the mode, effect and activity commands; others are
 * ignored). The mode dispatch takes the hit's words at +0x0C and +0x10 as its code and wait, and the face
 * target. When the running command and the new one are both mode or effect commands, the running one is dropped
 * first. */
void scenarioBatExecDst(BattleUnit **faceTarget, struct HitRecord *hit)
{
    Ov01Command command;
    Ov01Command *current;

    switch (hit->reaction) {
    case UNIT_CMD_MODE_DISPATCH:
        command.command = UNIT_CMD_MODE_DISPATCH;
        command.code = hit->followResult;
        command.arg.wait = hit->backAttack;
        if (faceTarget != 0) {
            command.faceTarget = *faceTarget;
        } else {
            command.faceTarget = 0;
        }
        break;
    case UNIT_CMD_HOVER_EFFECT:
        command.command = UNIT_CMD_HOVER_EFFECT;
        break;
    case UNIT_CMD_JUMP_EFFECT:
        command.command = UNIT_CMD_JUMP_EFFECT;
        break;
    case UNIT_CMD_CLEAR_ACTIVE:
        command.command = UNIT_CMD_CLEAR_ACTIVE;
        break;
    case UNIT_CMD_PHASE_2B_WAIT:
        command.command = UNIT_CMD_PHASE_2B_WAIT;
        break;
    case UNIT_CMD_DEACTIVATE:
        command.command = UNIT_CMD_DEACTIVATE;
        break;
    default:
        return;
    }
    current = objCmdPtrGet(&hit->target->commandQueue);
    if (current != 0
        && (current->command == UNIT_CMD_MODE_DISPATCH || current->command == UNIT_CMD_MODE_START
            || current->command == UNIT_CMD_HOVER_EFFECT || current->command == UNIT_CMD_MOVE_RATIO)) {
        if (hit->reaction == UNIT_CMD_MODE_DISPATCH || hit->reaction == UNIT_CMD_MODE_START
            || hit->reaction == UNIT_CMD_HOVER_EFFECT) {
            objCmdNext(&hit->target->commandQueue);
        }
    }
    unitCmdSet(hit->target, &command);
}

int tecEffectGroupGet(BattleUnit *unit, TecRecord *tec);

int attackEffectsFinished(struct AttackCommand *owner) {
    TecRecord *entry;
    int status;

    entry = tecRecordGet(owner->actionIdx);
    status = tecEffectGroupGet(owner->attacker, entry);
    return unitEffectKeyFree(owner->attacker, status, entry->extent) != 0;
}

/* hitPopNumCreate: forwards a hit's target, damage and hit mode to objDmgNumCreate (fourth argument 0). */
void hitPopNumCreate(struct AttackCommand *command, struct HitRecord *hit) {
    objDmgNumCreate(hit->target, hit->damage, hit->hitMode.word, 0);
}

/* eventQueueNext: the record at the read cursor, or 0 when the queue is drained. */
void *eventQueueNext(void) {
    if (eventQueueCursor < eventQueueCount) {
        return eventQueueTable + eventQueueCursor;
    }
    return 0;
}

/* eventQueueReserve: the next free record (the appenders fill it), or 0 when the table is full. */
void *eventQueueReserve(void) {
    if (eventQueueCount < EVENT_QUEUE_COUNT) {
        return eventQueueTable + eventQueueCount;
    }
    return 0;
}

#include "shared.h"
#include "ov01/event_stock.h"




extern BattleUnit *ov01LastUnit;
extern struct DataEntry *ov01ModelEntry;

/*
 * eventQueueFindAttackAction: the ordinal'th kind-4 record counting back from the last
 * one, or 0 when the table holds fewer.
 */
EventQueueEntry *eventQueueFindAttackAction(int ordinal)
{
    int remaining;
    int matches;
    EventQueueEntry *entry;

    remaining = eventQueueCursor - 1;
    matches = 0;
    if (remaining >= 0) {
        entry = eventQueueTable + remaining;
        for (;;) {
            if (entry->kind == 4) {
                if (matches == ordinal) {
                    return entry;
                }
                matches += 1;
            }
            remaining -= 1;
            entry -= 1;
            if (remaining < 0) {
                break;
            }
        }
    }
    return 0;
}

/*
 * eventQueueFindAttackHit: the ordinal'th kind-5 record counting back from the last
 * one, or 0 when the table holds fewer.
 */
EventQueueEntry *eventQueueFindAttackHit(int ordinal)
{
    int remaining;
    int matches;
    EventQueueEntry *entry;

    remaining = eventQueueCursor - 1;
    matches = 0;
    if (remaining >= 0) {
        entry = eventQueueTable + remaining;
        for (;;) {
            if (entry->kind == 5) {
                if (matches == ordinal) {
                    return entry;
                }
                matches += 1;
            }
            remaining -= 1;
            entry -= 1;
            if (remaining < 0) {
                break;
            }
        }
    }
    return 0;
}

/* eventQueueAddUnitCommand: appends a kind-1 record holding a pointer and an 0x18-byte payload. */
void eventQueueAddUnitCommand(BattleUnit *object, CmdListBody *args)
{
    EventQueueEntry *entry = eventQueueReserve();

    entry->kind = 1;
    entry->data.kind1.object = object;
    entry->data.kind1.args = *args;
    eventQueueCount++;
}

/* eventQueueAddAttackAction: appends a kind-4 record holding the attack command. */
void eventQueueAddAttackAction(struct AttackCommand *command)
{
    EventQueueEntry *entry = eventQueueReserve();

    entry->kind = 4;
    entry->data.attack = *command;
    eventQueueCount++;
}

/* eventQueueAddAttackHit: appends a kind-5 record with a 0x2C-byte payload. */
void eventQueueAddAttackHit(struct HitRecord *args)
{
    EventQueueEntry *entry = eventQueueReserve();

    entry->kind = 5;
    entry->data.kind5 = *args;
    eventQueueCount++;
}

/* eventQueueAdd: appends a record of the given kind, copying 0x10 bytes when a source is given. */
void eventQueueAdd(int kind, EventQueueArgs *args)
{
    EventQueueEntry *entry = eventQueueReserve();

    entry->kind = kind;
    if (args != 0) {
        entry->data.args = *args;
    }
    eventQueueCount++;
}

/* Event kinds with a handler in eventQueueHandlers. */
#define EVENT_KIND_COUNT 0x15

/* Handlers by record kind: nonzero when the record is done. */
/* The table has three more (empty) slots than kinds. */
#define EVENT_HANDLER_SLOTS 24
extern int (*eventQueueHandlers[EVENT_HANDLER_SLOTS])();

/* eventQueueRun: calls the handler of each queued record's kind. A record whose
 * handler is not done yet stops the drain (eventRetryCount counts the retries); a done
 * record clears both handler counters and is consumed. 0 also for an unknown kind. */
int eventQueueRun(void)
{
    EventQueueEntry *entry;

    for (entry = eventQueueNext(); entry != 0; entry = eventQueueNext()) {
        if (!(entry->kind < EVENT_KIND_COUNT)) {
            return 0;
        }
        if (eventQueueHandlers[entry->kind](entry) == 0) {
            eventRetryCount++;
            return 0;
        }
        eventRetryCount = 0;
        eventStepStarted = 0;
        eventQueueCursor++;
    }
    return 1;
}

int eventNoneRun(void) {
    return 1;
}

/* unitScriptEventSend kinds of eventUnitCommand: a technique of gateFlag TEC_GATE_NONE or 6..8 (the flagged kind
 * for a technique with TEC_FLAG_SCRIPT_FLAGGED), and the unit commands it reports. */
#define TECHNIQUE_SCRIPT_EVENT 1
#define TECHNIQUE_FLAGGED_SCRIPT_EVENT 2
#define MOVE_CHA_SCRIPT_EVENT 3
#define PHASE_0E_WAIT_SCRIPT_EVENT 11
#define POSE_ARRIVE_WAIT_SCRIPT_EVENT 13

/*
 * Kind-1 event handler: appends the queued command to the unit's command queue, keeps the unit and command kind in
 * BattleState for the kinds CMD_KIND_TECHNIQUE..CMD_KIND_ITEM and CMD_KIND_SNAPSHOT, and reports a technique of
 * gateFlag TEC_GATE_NONE or 6..8 and the unit commands UNIT_CMD_MOVE_CHA, UNIT_CMD_PHASE_0E_WAIT and
 * UNIT_CMD_POSE_ARRIVE_WAIT to the unit's script with the index of the command's target. Returns 1.
 */
int eventUnitCommand(EventQueueEntry *entry)
{
    UnitScriptEvent event;
    BattleUnit *unit;
    BattleUnit *target;
    CmdListBody *command;
    TecRecord *tec;
    int kind;

    command = &entry->data.kind1.args;
    unit = entry->data.kind1.object;
    unitCmdSet(unit, (Ov01Command *)command);
    if (command->command > CMD_KIND_NONE
        && (command->command <= CMD_KIND_ITEM || command->command == CMD_KIND_SNAPSHOT)) {
        manWorkGet()->object = unit;
        manWorkGet()->commandKind = command->command;
    }
    event.unit = unit;
    target = (BattleUnit *)unit->commandTarget;
    switch (command->command) {
    case CMD_KIND_TECHNIQUE:
        tec = tecRecordGet(command->code);
        if (tec->gateFlag == TEC_GATE_NONE || tec->gateFlag == 6 || tec->gateFlag == 7 || tec->gateFlag == 8) {
            kind = (tec->flags & TEC_FLAG_SCRIPT_FLAGGED) ? TECHNIQUE_FLAGGED_SCRIPT_EVENT : TECHNIQUE_SCRIPT_EVENT;
            event.words[0] = unitNoGet(target);
            event.words[1] = command->code;
            unitScriptEventSend(kind, &event);
        }
        break;
    case UNIT_CMD_MOVE_CHA:
        event.words[0] = unitNoGet(target);
        unitScriptEventSend(MOVE_CHA_SCRIPT_EVENT, &event);
        break;
    case UNIT_CMD_PHASE_0E_WAIT:
        event.words[0] = unitNoGet(target);
        unitScriptEventSend(PHASE_0E_WAIT_SCRIPT_EVENT, &event);
        break;
    case UNIT_CMD_POSE_ARRIVE_WAIT:
        event.words[0] = unitNoGet(target);
        unitScriptEventSend(POSE_ARRIVE_WAIT_SCRIPT_EVENT, &event);
        break;
    }
    return 1;
}


/*
 * eventWaitUnitQueue (proposed eventWaitUnitQueue): the kind-2 handler of the event table
 * eventQueueHandlers. The handler's result is unitQueueIsEmpty's result, which the drain
 * reads (nonzero: the queue is empty and the event is done). The return type stays
 * void: the value the drain reads is the one left by the call.
 */
void eventWaitUnitQueue(EventQueueEntry *entry) {
    unitQueueIsEmpty(entry->data.args.unit);
}

/* The script message of a hit (unitScriptPairEventSend, words[1]): the hit's flags byte, and this bit for a hit of kind
 * HIT_KIND_NONE. */
#define SCRIPT_HIT_NONE 0x40
/* The child of the attacker that a TEC_GATE_SHOW_CHILD technique shows for the attack (unitSlotResume) and hides
 * again at the end (unitSlotPause). */
#define TEC_GATE_CHILD 2

extern int attackStep;
/* The action and hit counts of the last attack event that finished. */
extern int attackLastActions;
extern int attackLastHits;
void attackEffectsStart(EventQueueEntry *event);
void scenarioMtd(struct AttackCommand *command);
int attackHitApply(struct AttackCommand *command, struct HitRecord *hit);
void unitsMarkUninvolved(int actions, int hits);
void unitsUninvolvedClear(void);
void unitSlotResume(BattleUnit *object, int index);
void unitSlotPause(BattleUnit *object, int index);
int attackHitsShow(EventQueueEntry *entry, struct AttackCommand *command);
int flowSpecProcInit(EventQueueEntry *entry, struct AttackCommand *command);
int flowSpecProc(EventQueueEntry *entry, struct AttackCommand *command);

/*
 * The EVENT_KIND_ATTACK handler (eventQueueHandlers). The first call (attackStartedState clear; unitJobTypeAtk sets it for a
 * transform) starts the effects, the units' stats and the action stream and marks the uninvolved units, and returns
 * 0. The later calls run each action: actions with hitFlags bit 0 through flowSpecProcInit once and
 * flowSpecProc until done, the others show their hits (attackHitsShow) and free their effect key. Once all are
 * done the hit targets get their flags cleared and their script message, and 1 is returned.
 */
int eventAttackRun(EventQueueEntry *entry)
{
    UnitScriptPairEvent message;
    struct AttackCommand *command;
    struct HitRecord *hit;
    int actions;
    int hits;
    int done;
    int i;
    int mode;
    int flag;

    done = 0;
    actions = entry->data.args.actions;
    hits = entry->data.args.hits;
    if (attackStartedState == 0) {
        attackEffectsStart(entry);
        for (i = 0; i < actions; i++) {
            statEffOff(eventQueueFindAttackAction(i)->data.attack.attacker);
        }
        command = &eventQueueFindAttackAction(0)->data.attack;
        unitPlayActionSound(command->attacker, command->actionIdx);
        unitsMarkUninvolved(actions, hits);
        if (tecRecordGet(command->actionIdx)->gateFlag == TEC_GATE_SHOW_CHILD) {
            unitSlotResume(command->attacker, TEC_GATE_CHILD);
        }
        attackStartedState = 1;
        return 0;
    }
    for (i = 0; i < actions; i++) {
        command = &eventQueueFindAttackAction(i)->data.attack;
        scenarioMtd(command);
        if (tecRecordGet(command->actionIdx)->hitFlags & 1) {
            if (eventStepStarted == 0) {
                if (flowSpecProcInit(entry, command) != 0) {
                    eventStepStarted = 1;
                    attackStep = 0;
                }
            } else {
                done += flowSpecProc(entry, command);
            }
        } else {
            attackHitsShow(entry, command);
            done += attackEffectsFinished(command);
        }
    }
    if (done < actions) {
        return 0;
    }
    for (i = 0; i < actions; i++) {
        command = &eventQueueFindAttackAction(i)->data.attack;
        unitSlotsClear(command->attacker, command->actionIdx);
    }
    command = &eventQueueFindAttackAction(0)->data.attack;
    for (i = 0; i < hits; i++) {
        hit = &eventQueueFindAttackHit(i)->data.kind5;
        unitHitFlagsClear(hit->target);
        if (hit->target->flags & UNIT_FLAG_SCRIPT_EVENTS) {
            message.unit = hit->target;
            command = &eventQueueFindAttackAction(0)->data.attack;
            message.other = command->attacker;
            message.words[0] = command->actionIdx;
            mode = hit->hitMode.word;
            flag = (mode & HIT_MODE_KIND_MASK) != HIT_KIND_NONE ? 0 : SCRIPT_HIT_NONE;
            message.words[1] = ((mode >> HIT_MODE_FLAGS_SHIFT) & HIT_MODE_FLAGS_MASK) | flag;
            unitScriptPairEventSend(UNIT_SCRIPT_KIND_PAIR, &message);
        }
        if (tecRecordGet(command->actionIdx)->gateFlag == TEC_GATE_SHOW_CHILD) {
            unitSlotPause(command->attacker, TEC_GATE_CHILD);
        }
        attackHitApply(command, hit);
    }
    attackLastActions = actions;
    attackLastHits = hits;
    unitsUninvolvedClear();
    return 1;
}

/*
 * Holder of the table pointer unitTblGet returns. Codegen: a plain `BattleUnit **` local whose address is passed
 * gets a stack slot of alias set 0, so the unit and actor flag stores in the unit loops kill its load for gcse load
 * PRE (one more reload per loop than the original); the slot of a struct member keeps the original's register.
 */
struct UnitTableRef {
    BattleUnit **units;
};

/* XenoActor.drawFlags bit unitsMarkUninvolved sets on the actors that take no part in the queued action and clears
 * on the others (unitsUninvolvedClear clears it on all). */
#define ACTOR_DRAW_UNINVOLVED 0x4000000

/* Unit keys that always count as involved. */
#define UNIT_KEY_ALWAYS_INVOLVED_A 0x7D
#define UNIT_KEY_ALWAYS_INVOLVED_B 0x1F
#define UNIT_KEY_ALWAYS_INVOLVED_C 0x7C

/* Marks every unit that is neither the attacker of one of the last `actions` kind-4 records nor the target of one
 * of the last `hits` kind-5 records; units with keys 0x7D, 0x1F and 0x7C always count as involved. */
void unitsMarkUninvolved(int actions, int hits)
{
    struct UnitTableRef table;
    int count;
    int i;
    int j;
    int involved;

    count = unitTblGet(UNIT_SIDE_ALL, &table.units);
    for (i = 0; i < count; i++) {
        if (table.units[i] == 0) {
            continue;
        }
        involved = 0;
        for (j = 0; j < actions; j++) {
            if (eventQueueFindAttackAction(j)->data.attack.attacker == table.units[i]) {
                involved = 1;
                break;
            }
        }
        for (j = 0; j < hits; j++) {
            if (eventQueueFindAttackHit(j)->data.kind5.target == table.units[i]) {
                involved = 1;
                break;
            }
        }
        if (table.units[i]->stats->key == UNIT_KEY_ALWAYS_INVOLVED_A
            || table.units[i]->stats->key == UNIT_KEY_ALWAYS_INVOLVED_B
            || table.units[i]->stats->key == UNIT_KEY_ALWAYS_INVOLVED_C) {
            involved = 1;
        }
        if (involved == 0) {
            table.units[i]->unit->drawFlags |= ACTOR_DRAW_UNINVOLVED;
        } else {
            table.units[i]->unit->drawFlags &= ~ACTOR_DRAW_UNINVOLVED;
        }
    }
}

void unitsUninvolvedClear(void) {
    BattleUnit **actors;
    BattleUnit **cursor;
    BattleUnit *actor;
    struct XenoActor *unit;
    int count;
    int remaining;

    count = unitTblGet(2, &actors);
    if (count > 0) {
        cursor = actors;
        remaining = count;
        do {
            actor = *cursor;
            cursor += 1;
            if (actor != 0) {
                unit = actor->unit;
                unit->drawFlags &= 0xFBFFFFFF;
            }
            remaining -= 1;
        } while (remaining != 0);
    }
}

int counterProc(struct AttackCommand *command, struct HitRecord *hit);
/* Defined below in this file. */

/* Row 3 status that brings back a unit the hit takes down with 1 HP (and is used up); the row 1 and row 2
 * status that a HIT_KIND_PLAIN hit clears, with its chance in percent per row. */
#define STAT3_ENDURE 0x400
#define STAT_HIT_CLEARS 0x20
#define HIT_CLEAR_CHANCE_ROW1 50
#define HIT_CLEAR_CHANCE_ROW2 10
/* The current value a TEC_GATE_CURRENT_DRAIN technique takes from the target. */
#define TEC_CURRENT_DRAIN_AMOUNT 100
/* What the hit of a technique with gateFlag TEC_GATE_NONE, 6, 7 or 8 gives: percent to the attacker (more for a
 * taken hit) and boost progress to the target on ACT_STEP_ADD_BOOST. */
#define HIT_PERCENT_TAKEN 20
#define HIT_PERCENT_PLAIN 10
#define HIT_BOOST_PROGRESS 10
/* The actStepCounterValue values at which a technique hit takes the target's act stock and adds boost progress. */
#define ACT_STEP_TAKE_STOCK 5
#define ACT_STEP_ADD_BOOST 7
/* menuStatPanelNew kind shown when a hit takes act stock. */
#define STAT_PANEL_STOCK_LOST 5

/*
 * Applies one hit of an attack to its target and returns 1: the HP damage (calcUseItem for a lookup attack),
 * the endure status, the wake rolls, the inflicted and cured statuses, the technique's drain, the attacker's percent
 * and the target's stock and boost on the counter steps, the act stock roll, and UNIT_CMD_FINISH_REFRESH when the
 * hit changes whether the target is low on HP.
 */
int attackHitApply(struct AttackCommand *command, struct HitRecord *hit)
{
    Ov01Command refresh;
    int stockIndex;
    TecRecord *tec;
    int wasLow;
    int isLow;
    int result;
    int removed;
    int bit;
    int i;

    wasLow = partyUnitHpLow(hit->target);
    if (command->kind == CMD_KIND_ITEM) {
        result = calcUseItem(command->attacker, hit->target, command->id);
    } else {
        result = calcHpDmg(command->attacker, hit->target, hit->hitMode.word, hit->damage);
    }
    if (result == 0 && calcStatGet(hit->target, STAT_ROW_3, STAT3_ENDURE) != 0) {
        calcHp(hit->target, 1);
        result = 1;
        calcStatReset(hit->target, STAT_ROW_3, STAT3_ENDURE);
    }
    if (hit->hitMode.kind == HIT_KIND_PLAIN) {
        if (calcStatGet(hit->target, STAT_ROW_1, STAT_HIT_CLEARS) != 0 && ov01RollPercent(HIT_CLEAR_CHANCE_ROW1) != 0) {
            calcStatReset(hit->target, STAT_ROW_1, STAT_HIT_CLEARS);
        }
        if (calcStatGet(hit->target, STAT_ROW_2, STAT_HIT_CLEARS) != 0 && ov01RollPercent(HIT_CLEAR_CHANCE_ROW2) != 0) {
            calcStatReset(hit->target, STAT_ROW_2, STAT_HIT_CLEARS);
        }
    }
    if (hit->inflictRow != 0) {
        for (bit = 1, i = 15; i >= 0; i--, bit <<= 1) {
            if (hit->inflictMask & bit) {
                calcStatSet(hit->target, hit->inflictRow, bit);
            }
        }
    }
    if (hit->cureRow != 0) {
        for (bit = 1, i = 15; i >= 0; i--, bit <<= 1) {
            if (hit->cureMask & bit) {
                calcStatReset(hit->target, hit->cureRow, bit);
            }
        }
    }
    tec = tecRecordGet(command->actionIdx);
    if (tec->gateFlag == TEC_GATE_CURRENT_DRAIN) {
        unitCurrentAdd(hit->target, -TEC_CURRENT_DRAIN_AMOUNT);
    }
    if (command->kind == CMD_KIND_TECHNIQUE
        && (tec->gateFlag == TEC_GATE_NONE || tec->gateFlag == 6 || tec->gateFlag == 7 || tec->gateFlag == 8)) {
        unitPercentAdd(command->attacker, ((hit->hitMode.word >> HIT_MODE_FLAGS_SHIFT) & HIT_FLAG_TAKEN)
                                              ? HIT_PERCENT_TAKEN : HIT_PERCENT_PLAIN);
        if (actStepCounterValue() == ACT_STEP_TAKE_STOCK && (hit->hitMode.word & HIT_MODE_KIND_MASK) == HIT_KIND_PLAIN
            && command->attacker != hit->target) {
            removed = 0;
            while (actStockRemoveStock(hit->target, &stockIndex) != 0) {
                if (stockIndex > 0) {
                    menuStockDead(stockIndex);
                }
                removed = 1;
            }
            if (removed) {
                calcTurnEnd(hit->target);
                menuStatPanelNew(STAT_PANEL_STOCK_LOST);
            }
        }
    }
    if ((hit->hitMode.word >> HIT_MODE_FLAGS_SHIFT) & HIT_FLAG_TAKEN) {
        unitPercentAdd(hit->target, -(hit->target->stats->rollPercent / 2));
    }
    if (command->kind == CMD_KIND_TECHNIQUE
        && (tec->gateFlag == TEC_GATE_NONE || tec->gateFlag == 6 || tec->gateFlag == 7 || tec->gateFlag == 8)) {
        if (actStepCounterValue() == ACT_STEP_ADD_BOOST) {
            calcBp(hit->target, HIT_BOOST_PROGRESS);
        }
    }
    if (result != 0 && command->kind == CMD_KIND_TECHNIQUE) {
        counterProc(command, hit);
    }
    isLow = partyUnitHpLow(hit->target);
    if (result != 0 && ((wasLow == 0 && isLow != 0) || (wasLow != 0 && isLow == 0))) {
        refresh.command = UNIT_CMD_FINISH_REFRESH;
        unitCmdSet(hit->target, &refresh);
    }
    return 1;
}


/*
 * counterProc (proposed counterProc): rolls the act-stock chance of the unit
 * of an entry, with two effect-code rolls for the low keys, and grants act stock
 * when a roll hits and calcBoostChk accepts the unit.
 */
int counterProc(struct AttackCommand *command, struct HitRecord *hit) {
    BattleUnit *object;
    int chanceHit;
    int flags;
    int requestFlags;

    object = hit->target;
    if (tecRecordGet(command->actionIdx)->flags & TEC_TARGET_ALL) {
        return 0;
    }
    if (object->stats->key < 0x10) {
        chanceHit = actLevelChanceRoll(object, command->actionIdx) != 0;
        if (calcSkillEquChk(object, 0x2B) != 0) {
            flags = hit->hitMode.word;
            if ((flags >> 8) & 1) {
                if (ov01RollPercent(0x19) != 0) {
                    chanceHit = 1;
                }
            }
        }
        if (calcSkillEquChk(object, 0x33) != 0 && (object->flags & 0x4000)) {
            requestFlags = hit->hitMode.word;
            if (((requestFlags >> 8) & 2) != 0 || !(requestFlags & 0xFF)) {
                if (ov01RollPercent(0x32) != 0) {
                    chanceHit = 1;
                }
            }
        }
    } else {
        chanceHit = actLevelChanceRoll(object, command->actionIdx) != 0;
    }
    if (chanceHit == 1 && calcBoostChk(object, 0) != 0) {
        calcBoost(object, 0);
        unitEffectStart(object, 0x140, 7);
        seVoiceStartTagged(2, 1);
    }
    return 1;
}

/* Bit of the actor's sef entry flags (sefGetEntryFlags) that lets attackHitsShow show the hit panels. */
#define SEF_ENTRY_SHOW_HIT 0x4
/* The hit effects attackHitsShow starts on a target: group 0x136, the low key per step state and for a taken hit. */
#define HIT_EFFECT_GROUP 0x136
#define HIT_EFFECT_TAKEN 0x11
#define HIT_EFFECT_STEP1 0x12
#define HIT_EFFECT_STEP2 0x14
#define HIT_EFFECT_STEP3 0x13

/*
 * Shows the hits of a kind-3 attack event (entry) of command: per kind-5 record, once per attack and target, the
 * stat panels of the hit while the target's sef entry allows it; then, for the attack kinds of attackSefFlag1Check, the
 * target's reaction command, its unit panel (the target cursor's unit) and the hit effects. A jump-effect reaction
 * is queued only once per target. Returns 1.
 */
int attackHitsShow(EventQueueEntry *entry, struct AttackCommand *command)
{
    struct HitRecord *hit;
    int hits;
    int i;
    int mode;
    int kind;
    int flags;
    int effect;

    hits = entry->data.args.hits;
    for (i = 0; i < hits; i++) {
        hit = &eventQueueFindAttackHit(i)->data.kind5;
        mode = hit->hitMode.word;
        kind = mode & HIT_MODE_KIND_MASK;
        flags = (mode >> HIT_MODE_FLAGS_SHIFT) & HIT_MODE_FLAGS_MASK;
        if (sefGetEntryFlags(hit->target->unit) & SEF_ENTRY_SHOW_HIT) {
            hitPopNumCreate(command, hit);
            menuComboPanelStart(hit->target);
            if (!(hit->target->flags & UNIT_FLAG_HIT_PANELS)) {
                if (kind == HIT_KIND_NONE) {
                    menuStatPanelNew(MENU_STAT_PANEL_NO_HIT);
                } else if (flags & HIT_FLAG_BLOCKED) {
                    menuStatPanelNew(MENU_STAT_PANEL_BLOCKED);
                }
                if (flags & HIT_FLAG_TAKEN) {
                    if (actStepCounterGet()->bonusFlag == 1) {
                        menuStatPanelNew(MENU_STAT_PANEL_TAKEN_BONUS);
                    } else {
                        menuStatPanelNew(MENU_STAT_PANEL_TAKEN);
                    }
                }
                if (hit->backAttack == 1) {
                    menuStatPanelNew(MENU_STAT_PANEL_BACK_ATTACK);
                }
                hit->target->flags |= UNIT_FLAG_HIT_PANELS;
            }
        }
        if (attackSefFlag1Check(command->attacker, command->kind) != 0) {
            if (hit->reaction == UNIT_CMD_JUMP_EFFECT && (hit->target->flags & UNIT_FLAG_HIT_REACTED)) {
                continue;
            }
            scenarioBatExecDst(&command->attacker, hit);
            /* HIT_KIND_STATUS and HIT_KIND_NO_INFLICT change neither HP nor EP (calcHpDmg): no unit panel or
             * effects. */
            if (kind != HIT_KIND_NONE && kind != HIT_KIND_STATUS && kind != HIT_KIND_NO_INFLICT) {
                if (tgtUnitGet() == hit->target) {
                    menuIconListAppend(hit->target);
                }
                if (!(hit->target->flags & UNIT_FLAG_HIT_REACTED)) {
                    if (tgtUnitGet() == hit->target) {
                        menuUnitPanelOpen(hit->target, hit->stepState);
                        menuComboPanelStart(hit->target);
                        menuStatPanelOpen(actStepCounterGet()->stepState);
                    }
                    switch (hit->stepState) {
                    case 1:
                        effect = HIT_EFFECT_STEP1;
                        break;
                    case 2:
                        effect = HIT_EFFECT_STEP2;
                        break;
                    case 3:
                        effect = HIT_EFFECT_STEP3;
                        break;
                    default:
                        effect = 0;
                        break;
                    }
                    if (effect != 0 && kind == HIT_KIND_PLAIN) {
                        unitEffectStart(hit->target, HIT_EFFECT_GROUP, effect);
                    }
                    if (flags & HIT_FLAG_TAKEN) {
                        unitEffectStart(hit->target, HIT_EFFECT_GROUP, HIT_EFFECT_TAKEN);
                    }
                }
            }
            hit->target->flags |= UNIT_FLAG_HIT_REACTED;
        }
    }
    return 1;
}

/* Special actions (AttackCommand.actionIdx) that flowSpecProcInit resolves and flowSpecProc steps through. */
enum {
    SPECIAL_ACTION_CURE_ALL = 0x38,             /* clears every status row of the target (calcStatResetCha) */
    SPECIAL_ACTION_HALVE_HP = 0x2A4,            /* clears the attacker's status rows 1 and 2 and halves its HP */
    SPECIAL_ACTION_CURE_ALL_2 = 0x2C3,          /* as SPECIAL_ACTION_CURE_ALL */
    SPECIAL_ACTION_ITEM_PICK = 0x2C4,           /* picks a random item (specInoriProc) */
    SPECIAL_ACTION_LEVEL_UP = 0x2C5,            /* the target gains a level */
    SPECIAL_ACTION_BOOST = 0x2C6,               /* the attacker gains a boost stock */
    SPECIAL_ACTION_STATUS_TEXT = 0x2C8,         /* shows the target's status text */
    SPECIAL_ACTION_STEAL = 0x2C9,               /* drops the target's first item half the time */
    SPECIAL_ACTION_TRANSFORM_FIRST = 0x2D5,     /* 0x2D5..0x2D7: the attacker transforms */
    SPECIAL_ACTION_TRANSFORM_LAST = 0x2D7,
    SPECIAL_ACTION_STATUS_STEAL = 0x2F6         /* shows the status text, then drops the target's first item */
};
/* calcStatReset mask of every status of a row. */
#define STAT_ROW_ALL 0xFFFF

void specAnalyzeProc(BattleUnit *unit);
void unitTransformBegin(BattleUnit *unit, int action);

/*
 * Resolves the special action of command (an attack event, entry) on the first hit's target. A command that is no
 * transform and that attackSefFlag1Check refuses returns 0; otherwise 1 (the item pick and the steal resolve in
 * flowSpecProc).
 */
int flowSpecProcInit(EventQueueEntry *entry, struct AttackCommand *command)
{
    struct HitRecord *hit;

    if (actionTransformSlotGet(command->actionIdx) == 0 && attackSefFlag1Check(command->attacker, command->kind) == 0) {
        return 0;
    }
    hit = &eventQueueFindAttackHit(0)->data.kind5;
    switch (command->actionIdx) {
    case SPECIAL_ACTION_HALVE_HP:
        calcStatReset(command->attacker, STAT_ROW_1, STAT_ROW_ALL);
        calcStatReset(command->attacker, STAT_ROW_2, STAT_ROW_ALL);
        calcHp(command->attacker, -(command->attacker->stats->hp / 2));
        break;
    case SPECIAL_ACTION_CURE_ALL:
    case SPECIAL_ACTION_CURE_ALL_2:
        calcStatResetCha(hit->target);
        attackHitsShow(entry, command);
        break;
    case SPECIAL_ACTION_LEVEL_UP:
        unitLevelAdd(hit->target, 1);
        break;
    case SPECIAL_ACTION_BOOST:
        calcBc(command->attacker, 1);
        break;
    case SPECIAL_ACTION_STATUS_TEXT:
        specAnalyzeProc(hit->target);
        break;
    case SPECIAL_ACTION_TRANSFORM_FIRST ... SPECIAL_ACTION_TRANSFORM_LAST:
        unitTransformBegin(command->attacker, command->actionIdx);
        screenFillStart();
        break;
    case SPECIAL_ACTION_STATUS_STEAL:
        specAnalyzeProc(hit->target);
        break;
    case SPECIAL_ACTION_ITEM_PICK:
    case SPECIAL_ACTION_STEAL:
        break;
    }
    return 1;
}

/* The steps of a transforming action (actionTransformSlotGet gives its model): attackStep runs through them. */
enum {
    ATTACK_STEP_TRANSFORM,              /* swap the attacker's model to the transform slot */
    ATTACK_STEP_FILL_IN,                /* hold the screen fill for ATTACK_FILL_FRAMES */
    ATTACK_STEP_RESOLVE,                /* resolve the attack (attackHitsShowAndWait), start the screen fill */
    ATTACK_STEP_RECREATE,               /* recreate the unit */
    ATTACK_STEP_RESTORE,                /* bind the battle slot's data again, show the monster slots */
    ATTACK_STEP_RESET,                  /* reset the battle units */
    ATTACK_STEP_FILL_OUT                /* hold the screen fill again, then done */
};
#define ATTACK_FILL_FRAMES 3

/* The percent chance of the target's first drop for the two stealing actions (specNusumuProc): the steal takes it
 * half the time, the status steal always (else the second drop). */
#define STEAL_CHANCE_HALF 50
#define STEAL_CHANCE_SURE 100

extern int attackStep;
extern int attackFillWait;
extern int attackStartedState;
void specNusumuProc(BattleUnit *target, int value);
int attackHitsShowAndWait(EventQueueEntry *entry, struct AttackCommand *owner);
void unitRecreate(BattleUnit *owner);
void battleUnitsReset(void);
void specInoriProc(void);

/*
 * One step of the special actions of a queued attack (attackStep counts their steps); returns 1 when the action
 * is done. Plain actions resolve at once (attackEffectsFinished); the item pick and the steal free their effect key
 * first, the transforms swap the attacker, the status steal waits for the text block. Unknown actions return 0.
 */
int flowSpecProc(EventQueueEntry *entry, struct AttackCommand *command)
{
    struct MonsSlot **slots;
    struct HitRecord *hit;
    TecRecord *tec;
    struct DataSlot *slot;
    int done;

    done = 0;
    hit = &eventQueueFindAttackHit(0)->data.kind5;
    tec = tecRecordGet(command->actionIdx);
    switch (command->actionIdx) {
    case SPECIAL_ACTION_CURE_ALL:
    case SPECIAL_ACTION_HALVE_HP:
    case SPECIAL_ACTION_CURE_ALL_2:
    case SPECIAL_ACTION_LEVEL_UP:
    case SPECIAL_ACTION_BOOST:
        done = attackEffectsFinished(command);
        break;
    case SPECIAL_ACTION_ITEM_PICK:
        if (attackStep == 0) {
            if (unitEffectKeyFree(command->attacker, tecEffectGroupGet(command->attacker, tec), tec->extent) != 0) {
                specInoriProc();
                attackStep++;
            }
        } else if (battleMsgEndChk() != 0) {
            done = 1;
        }
        break;
    case SPECIAL_ACTION_STATUS_TEXT:
        if (analyzePanelIsActive() == 0) {
            done = 1;
        }
        break;
    case SPECIAL_ACTION_STEAL:
        if (attackStep == 0) {
            if (unitEffectKeyFree(command->attacker, tecEffectGroupGet(command->attacker, tec), tec->extent) != 0) {
                specNusumuProc(hit->target, STEAL_CHANCE_HALF);
                attackStep++;
            }
        } else if (battleMsgEndChk() != 0) {
            done = 1;
        }
        break;
    case SPECIAL_ACTION_TRANSFORM_FIRST ... SPECIAL_ACTION_TRANSFORM_LAST:
        switch (attackStep) {
        case ATTACK_STEP_TRANSFORM:
            if (dataCdSync() != 0) {
                break;
            }
            if (sefCheckLoad() != 0) {
                break;
            }
            slot = dataSlotFind(actionTransformSlotGet(command->actionIdx));
            if (slot == 0) {
                return 1;
            }
            unitBindActors(ov01LastUnit, slot);
            unitUnbind(ov01LastUnit);
            attackStartedState = 0;
            command->attacker = ov01LastUnit;
            unitAttachChild(ov01LastUnit, ov01ModelEntry, 0);
            unitSetTimedPhase(ov01LastUnit, 1);
            attackFillWait = 0;
            attackStep++;
            break;
        case ATTACK_STEP_FILL_IN:
            if (attackFillWait >= ATTACK_FILL_FRAMES) {
                screenFillEnd();
                attackStep++;
            }
            attackFillWait++;
            break;
        case ATTACK_STEP_RESOLVE:
            if (attackHitsShowAndWait(entry, command) == 0) {
                break;
            }
            screenFillStart();
            attackStep++;
            break;
        case ATTACK_STEP_RECREATE:
            unitRecreate(ov01LastUnit);
            attackStep++;
            break;
        case ATTACK_STEP_RESTORE:
            if (dataCdSync() != 0) {
                break;
            }
            if (sefCheckLoad() != 0) {
                break;
            }
            slot = dataSlotFind(manWorkGet()->slotIndex);
            if (slot == 0) {
                return 1;
            }
            unitBindActors(ov01LastUnit, slot);
            statEffOn(ov01LastUnit);
            command->attacker = ov01LastUnit;
            monsActiveSlotsGet(&slots);
            monsSlotShow(slots[0]);
            attackStep++;
            break;
        case ATTACK_STEP_RESET:
            battleUnitsReset();
            attackFillWait = 0;
            attackStep++;
            break;
        case ATTACK_STEP_FILL_OUT:
            if (attackFillWait >= ATTACK_FILL_FRAMES) {
                screenFillEnd();
                done = 1;
            }
            attackFillWait++;
            break;
        }
        break;
    case SPECIAL_ACTION_STATUS_STEAL:
        switch (attackStep) {
        case 0:
            if (analyzePanelIsActive() == 0) {
                attackStep++;
            }
            break;
        case 1:
            specNusumuProc(hit->target, STEAL_CHANCE_SURE);
            attackStep++;
            break;
        default:
            done = battleMsgEndChk() != 0;
            break;
        }
        break;
    }
    return done;
}

/* One weighted group of itemPickGroups: a roll below threshold picks one of its count items. */
typedef struct ItemPickGroup {
    int threshold;
    int count;
    int *items;
} ItemPickGroup;

#define ITEM_PICK_GROUPS 4

extern ItemPickGroup itemPickGroups[ITEM_PICK_GROUPS];
/* The 0x80-byte message line specInoriProc builds, and the text "!" appended after the item name. */
extern char itemPickText[];
const char itemPickTextEnd[] = "!";

char *strcpy(char *destination, const char *source);
char *strcat(char *destination, const char *source);

/* specInoriProc: rnd() picks one item from four weighted groups of itemPickGroups;
 * the item is added to the data box and its name is shown. */
void specInoriProc(void)
{
    int roll;
    int item;
    int i;

    roll = rnd(99);
    item = 0;
    for (i = 0; i < ITEM_PICK_GROUPS; i++) {
        if (roll < itemPickGroups[i].threshold) {
            item = itemPickGroups[i].items[rnd(itemPickGroups[i].count - 1)];
            break;
        }
    }
    dataBoxIncCategory1(item);
    strcpy(itemPickText, tableStringGet(1, 1));
    strcat(itemPickText, hitActionTextGet(item, 0));
    strcat(itemPickText, itemPickTextEnd);
    msgPutObject(0x14, 0x120, itemPickText, 0x2D, 0x1AC, 0x28);
}

/* StatBlock.flags bits specAnalyzeProc turns into the text mode (1 or 2). */
#define UNIT_TEXT_MODE_1 0x200
#define UNIT_TEXT_MODE_2 0x400
/* StatBlock.slotValue entries specAnalyzeProc marks (values from 0x15 on). */
#define UNIT_TEXT_SLOT_COUNT 8
#define UNIT_TEXT_SLOT_MARKED 0x15

/* Shows the status text block of unit: HP and EP with their caps, its table key
 * record, the text mode from the stat flags and the marked status slots as bits. */
void specAnalyzeProc(BattleUnit *unit)
{
    AnalyzeStats block;
    StatBlock *stats;
    struct TableKeyRecord *record;
    int mode;
    int bits;
    int i;

    stats = unit->stats;
    record = enemyRewardRecordGet(stats->key);
    block.hp = stats->hp;
    block.maxHp = stats->maxHp;
    block.ep = stats->ep;
    block.maxEp = stats->maxEp;
    block.exp = record->points;
    block.skillPoints = record->stepRate;
    block.classPoints = record->flat;
    block.rareItemKind = record->firstCategory;
    block.rareItem = record->firstId;
    block.itemKind = record->secondCategory;
    block.item = record->secondId;
    mode = 1;
    if (!(stats->flags & UNIT_TEXT_MODE_1)) {
        mode = 0;
        if (stats->flags & UNIT_TEXT_MODE_2) {
            mode = 2;
        }
    }
    block.attackType = mode;
    bits = 0;
    for (i = 0; i < UNIT_TEXT_SLOT_COUNT; i++) {
        if (unit->stats->slotValue[i] >= UNIT_TEXT_SLOT_MARKED) {
            bits |= 1 << i;
        }
    }
    block.attributes = bits;
    analyzePanelStart(0x14, 0x64, &block);
}

/* BattleUnit.flags bits of the data box drops already taken from a unit: the first and the second pair of its
 * TableKeyRecord. */
#define UNIT_FLAG_DROP_FIRST 0x10000
#define UNIT_FLAG_DROP_SECOND 0x8000
/* The data box category whose names itemTextGet gives (the others: hitActionTextGet). */
#define DATA_BOX_CATEGORY_2 2


/*
 * The data box drop of a unit: with chance percent its first pair, else (or when that is empty or taken) its
 * second; a pair is taken once. Composes the drop message ("<name> ..." or the nothing text) in itemPickText and
 * shows it.
 */
void specNusumuProc(BattleUnit *unit, int percent)
{
    struct TableKeyRecord *record;
    char *name;
    int category;
    int id;

    record = enemyRewardRecordGet(unit->stats->key);
    if (ov01RollPercent(percent) != 0 && record->firstId != 0 && !(unit->flags & UNIT_FLAG_DROP_FIRST)) {
        category = record->firstCategory;
        id = record->firstId;
        unit->flags |= UNIT_FLAG_DROP_FIRST;
    } else if (record->secondId != 0 && !(unit->flags & UNIT_FLAG_DROP_SECOND)) {
        category = record->secondCategory;
        id = record->secondId;
        unit->flags |= UNIT_FLAG_DROP_SECOND;
    } else {
        category = -1;
        id = 0;
    }
    if (id != 0) {
        if (category == DATA_BOX_CATEGORY_2) {
            name = itemTextGet(id, 0);
        } else {
            name = hitActionTextGet(id, 0);
        }
        strcpy(itemPickText, tableStringGet(2, 1));
        strcat(itemPickText, name);
        strcat(itemPickText, itemPickTextEnd);
        dataBoxInc(category, id);
    } else {
        strcpy(itemPickText, tableStringGet(3, 1));
    }
    msgPutObject(0x14, 0x120, itemPickText, 0x2D, 0x1AC, 0x28);
}

/* The sef gate the transform closes (owner 0). */
#define TRANSFORM_SEF_GATE 0x37
/* Half a turn in radians (pi): the facing of the transformed unit and of the pair units in eventPairBegin. */
#define ANGLE_HALF_TURN 3.1415927f

/*
 * Starts the transform of unit by action: keeps its key as the battle slot index and its facing as the battle
 * scale, rebinds the side-2 units, removes the unit and creates the transformed one (actionTransformSlotGet) as
 * ov01LastUnit facing pi, loads its data and hides the first monster slot.
 */
void unitTransformBegin(BattleUnit *unit, int action)
{
    struct MonsSlot **slots;
    BattleUnit **units;
    BattleUnit *other;
    int count;
    int i;
    int key;
    int posIndex;
    int model;

    manWorkGet()->slotIndex = unit->stats->key;
    count = unitTblGet(2, &units);
    for (i = 0; i < count; i++) {
        other = units[i];
        if (other != 0) {
            other->unit->drawFlags &= ~ACTOR_STATE_UNBOUND;
            unitBindSetup(other);
        }
    }
    posIndex = unit->stats->posIndex;
    key = unit->stats->key;
    manWorkGet()->scale = unit->stats->facing;
    sefReleaseID(unit->stats->sequenceIndex);
    unitRemove(unit);
    model = actionTransformSlotGet(action);
    ov01LastUnit = unitPlCreate(model);
    unitClearSnapshot(unit);
    unit->stats->posIndex = posIndex;
    unitSetAngleRadians(ov01LastUnit, 3.1415927f);
    dataUnitFileLoad(model);
    dataSlotFlagClear(key);
    monsActiveSlotsGet(&slots);
    monsSlotHide(slots[0]);
    sefSetGateForId(0, TRANSFORM_SEF_GATE);
    decoFlagsSetAll();
}

int attackHitsShowAndWait(EventQueueEntry *entry, struct AttackCommand *owner) {
    attackHitsShow(entry, owner);
    return attackEffectsFinished(owner);
}

/*
 * unitRecreate (proposed unitRecreate): replaces the owner unit by a new one created at
 * the battle index, keeping the owner's level and the battle scale; the data slot of
 * the owner's key is released first.
 */
void unitRecreate(BattleUnit *owner)
{
    f32 scale;
    int level;
    int index;

    level = owner->stats->posIndex;
    scale = manWorkGet()->scale;
    dataSlotFlagClear(owner->stats->key);
    dataSlotRelease(owner->stats->key);
    unitRemove(owner);
    index = manWorkGet()->slotIndex;
    ov01LastUnit = unitPlCreate(index);
    unitLoadSnapshot(owner, level);
    unitSetAngleRadians(ov01LastUnit, scale);
    dataUnitFileLoad(index);
    unitEffectSlotAcquire(index);
}

/*
 * battleUnitsReset (proposed battleUnitsReset): sets bit 1 of every unit's object flags,
 * resets the battle index to 0, calls sefSetGateForId(1, 0x37) and clears the deco flags.
 */
void battleUnitsReset(void)
{
    BattleUnit **items;
    int count;
    int index;
    BattleUnit *owner;

    index = 0;
    count = unitTblGet(2, &items);
    if (count > 0) {
        do {
            owner = items[index];
            index += 1;
            if (owner != 0) {
                owner->unit->drawFlags |= ACTOR_STATE_UNBOUND;
                unitUnbind(owner);
            }
        } while (index < count);
    }
    manWorkGet()->slotIndex = 0;
    sefSetGateForId(1, 0x37);
    decoFlagsClearAll();
}

/* Maps the ids 0x2D5..0x2D7 to 0x8D..0x8F, any other id to 0. */
int actionTransformSlotGet(int id)
{
    int value;

    switch (id) {
    case 0x2D5:
        value = 0x8D;
        break;
    case 0x2D6:
        value = 0x8E;
        break;
    case 0x2D7:
        value = 0x8F;
        break;
    default:
        value = 0;
        break;
    }
    return value;
}

/* Effect key ranges of the unit's own sequence: keys 700..733 move up by 100 for a technique of extent below 8. */
#define TEC_EFFECT_SEQUENCE_FIRST 700
#define TEC_EFFECT_SEQUENCE_COUNT 34
#define TEC_EFFECT_SEQUENCE_SHIFT 100

/* The effect group a technique uses for the unit (the high half of the key unitEffectKeyFree tests), by the
 * technique's gateFlag: fixed groups, the unit's sequence + 10, or the unit's sequence. */
int tecEffectGroupGet(BattleUnit *unit, TecRecord *tec)
{
    int group;

    switch (tec->gateFlag) {
    case 2:
    case 3:
        group = 0;
        break;
    case 9:
        group = 200;
        break;
    case 4:
        group = unit->stats->sequenceIndex + 10;
        break;
    case 1:
        group = 50;
        break;
    case 5:
        group = 14;
        break;
    case 6:
    case 7:
    case 8:
    default:
        group = unit->stats->sequenceIndex;
        if ((unsigned int)(group - TEC_EFFECT_SEQUENCE_FIRST) < TEC_EFFECT_SEQUENCE_COUNT) {
            if (tec->extent < 8) {
                group += TEC_EFFECT_SEQUENCE_SHIFT;
            }
        }
        break;
    }
    return group;
}

/* EffectRequest.hitCode: the low twelve bits carry the low byte of the technique's actionBits (hits of kind
 * HIT_KIND_PLAIN), the top four bits the target's flags & 3 (EFFECT_HIT_ROW_OTHER for the other hits). */
#define EFFECT_HIT_CODE 0x0FFF
#define EFFECT_HIT_ROW 0xF000
#define EFFECT_HIT_ROW_SHIFT 12
#define EFFECT_HIT_ROW_OTHER 3

/* Starts the effects of a kind-3 attack event: per kind-4 record the technique's effect on the attacker (and its
 * pair partner, gateFlag 1), once per kind-5 record (hit) with that hit's target. A pair technique runs only its
 * first record. */
void attackEffectsStart(EventQueueEntry *event)
{
    struct EffectRequest request;
    struct AttackCommand *attack;
    struct HitRecord *hit;
    TecRecord *tec;
    BattleUnit *partner;
    StatBlock *stats;
    int actions;
    int hits;
    int i;
    int j;
    unsigned short hitMode;

    actions = event->data.args.actions;
    hits = event->data.args.hits;
    for (i = 0; i < actions; i++) {
        memset(&request, 0, sizeof(request));
        attack = &eventQueueFindAttackAction(i)->data.attack;
        tec = tecRecordGet(attack->actionIdx);
        request.key = (tecEffectGroupGet(attack->attacker, tec) << EFFECT_KEY_GROUP_SHIFT) | tec->extent;
        request.mode = attack->attacker->stats->state;
        if (tec->gateFlag == TEC_GATE_PAIR) {
            partner = unitPairPartnerFind(attack->attacker, attack->id);
            stats = attack->attacker->stats;
            if (stats->key == pairLinkGet(attack->id)->slotFirst) {
                request.head = attack->attacker->unit;
                request.partner = partner->unit;
                request.parts[0] = attack->attacker->children[0];
                request.parts[1] = attack->attacker->children[1];
                request.parts[2] = partner->children[0];
                request.parts[3] = partner->children[1];
            } else {
                request.head = partner->unit;
                request.partner = attack->attacker->unit;
                request.parts[0] = partner->children[0];
                request.parts[1] = partner->children[1];
                request.parts[2] = attack->attacker->children[0];
                request.parts[3] = attack->attacker->children[1];
            }
        } else {
            request.head = attack->attacker->unit;
            request.parts[0] = attack->attacker->children[0];
            request.parts[1] = attack->attacker->children[1];
            request.parts[2] = attack->attacker->children[2];
        }
        for (j = 0; j < hits;) {
            hit = &eventQueueFindAttackHit(j++)->data.kind5;
            /* hitMode is read before the target stores and stored after the state; other orders schedule
             * the loads and stores differently. */
            hitMode = hit->hitMode.word;
            request.target = hit->target->unit;
            request.targetState = hit->target->stats->state;
            request.hitMode = hitMode;
            if ((unsigned char)hitMode == HIT_KIND_PLAIN) {
                /* Three separate updates of the halfword, as the original stores it three times. */
                request.hitCode &= EFFECT_HIT_ROW;
                request.hitCode = (request.hitCode | (unsigned char)tecRecordGet(attack->actionIdx)->actionBits)
                    & EFFECT_HIT_CODE;
                request.hitCode |= (hit->target->stats->flags & 3) << EFFECT_HIT_ROW_SHIFT;
            } else {
                request.hitCode = (request.hitCode & EFFECT_HIT_CODE) | (EFFECT_HIT_ROW_OTHER << EFFECT_HIT_ROW_SHIFT);
            }
            sefCreateEffect(&request);
        }
        if (tec->gateFlag == TEC_GATE_PAIR) {
            break;
        }
    }
}

/* Starts effect (keyHigh, keyLow) on the unit's actor. */
void unitEffectStart(BattleUnit *object, int keyHigh, int keyLow)
{
    struct EffectRequest request;

    memset(&request, 0, sizeof(request));
    request.key = (keyHigh << 0xC) | keyLow;
    request.head = object->unit;
    request.target = object->unit;
    sefCreateEffect(&request);
}

/* Large units (UNIT_KEY_LARGE_*, the slots with linkedSlots) start their effects in mode 3, the others in mode 2. */

/* Starts effect (keyHigh, keyLow) on the unit's actor, in mode 3 for unit keys 11..15, else 2. */
void unitEffectStartMode(BattleUnit *object, int keyHigh, int keyLow)
{
    struct EffectRequest request;
    int mode;

    memset(&request, 0, sizeof(request));
    mode = ((unsigned int)(object->stats->keyUnsigned - UNIT_KEY_LARGE_FIRST) < UNIT_KEY_LARGE_COUNT) ? 3 : 2;
    request.key = (keyHigh << EFFECT_KEY_GROUP_SHIFT) | keyLow;
    request.head = object->unit;
    request.mode = mode;
    request.target = object->unit;
    sefCreateEffect(&request);
}

/* The effect of the unit's own sequence group that statEffOn, statEffOff and battleExecDead switch
 * (sefApplyRecordMode): record 8 of the group. */
#define SEQUENCE_EFFECT_RECORD 8
#define SEQUENCE_EFFECT_KEY(sequence) (((sequence) << EFFECT_KEY_GROUP_SHIFT) | SEQUENCE_EFFECT_RECORD)

void statEffOn(BattleUnit *object) {
    sefApplyRecordMode(object->unit, (object->stats->sequenceIndex << 0xc) | 8, 3);
    statEffProc(object);
}

void statEffOff(BattleUnit *object)
{
    sefApplyRecordMode(object->unit, (object->stats->sequenceIndex << 0xC) | 8, 2);
    sefApplyRecordMode(object->unit, 0x12C001, 1);
    sefApplyRecordMode(object->unit, 0x12C002, 1);
    sefApplyRecordMode(object->unit, 0x12C003, 1);
}

/* Status aura effects of a unit: effect group 0x12C, records 1..3 (one per state statEffProc tells apart). */
#define AURA_GROUP 0x12C
#define AURA_KEY(record) ((AURA_GROUP << 12) | (record))

/*
 * Shows the status aura that matches the unit's state (stat 2 at level 1, stat 2 at any level, stat 1 at any
 * level), starting it when it does not run yet, and switches the other two auras off (mode 1). Nothing for a unit
 * with flag 2.
 */
void statEffProc(BattleUnit *object)
{
    if (object->flags & UNIT_FLAG_HIDDEN) {
        return;
    }
    if (calcStatGet(object, 2, 1)) {
        if (!sefOwnerPairInUse(object->unit, AURA_KEY(3))) {
            unitEffectStart(object, AURA_GROUP, 3);
        }
        sefApplyRecordMode(object->unit, AURA_KEY(2), 1);
        sefApplyRecordMode(object->unit, AURA_KEY(1), 1);
    } else if (calcStatGet(object, 2, 0xFFFF)) {
        if (!sefOwnerPairInUse(object->unit, AURA_KEY(2))) {
            unitEffectStart(object, AURA_GROUP, 2);
        }
        sefApplyRecordMode(object->unit, AURA_KEY(3), 1);
        sefApplyRecordMode(object->unit, AURA_KEY(1), 1);
    } else if (calcStatGet(object, 1, 0xFFFF)) {
        if (!sefOwnerPairInUse(object->unit, AURA_KEY(1))) {
            unitEffectStart(object, AURA_GROUP, 1);
        }
        sefApplyRecordMode(object->unit, AURA_KEY(3), 1);
        sefApplyRecordMode(object->unit, AURA_KEY(2), 1);
    } else {
        sefApplyRecordMode(object->unit, AURA_KEY(1), 1);
        sefApplyRecordMode(object->unit, AURA_KEY(2), 1);
        sefApplyRecordMode(object->unit, AURA_KEY(3), 1);
    }
}

void unitSlotShowPart(BattleUnit *object);
void unitSlotHidePart(BattleUnit *object);
void unitSlotShowHidePart(BattleUnit *object, int index);
void unitSlotHideShowPart(BattleUnit *object, int index);
struct FadeTask *unitSlotFadeInStart(BattleUnit *object, int index);
struct FadeTask *unitSlotFadeOutStart(BattleUnit *object, int index);

/* Bits of the attacker's sef entry flags (sefGetEntryFlags) that scenarioMtd acts on, named by the call each
 * one triggers. */
#define SEF_ENTRY_DECO_CLEAR 0x2                /* decoFlagsClearAll */
#define SEF_ENTRY_SLOT0_HIDE_SHOW 0x10          /* unitSlotHideShowPart(unit, 0) */
#define SEF_ENTRY_SLOT0_SHOW_HIDE 0x20          /* unitSlotShowHidePart(unit, 0) */
#define SEF_ENTRY_SLOT1_HIDE_SHOW 0x40          /* unitSlotHideShowPart(unit, 1) */
#define SEF_ENTRY_SLOT1_SHOW_HIDE 0x80          /* unitSlotShowHidePart(unit, 1) */
#define SEF_ENTRY_SLOT0_OUT 0x100               /* slot 0 fades out or pauses */
#define SEF_ENTRY_SLOT0_IN 0x200                /* slot 0 fades in or resumes */
#define SEF_ENTRY_SLOT1_OUT 0x400               /* slot 1 fades out or pauses */
#define SEF_ENTRY_SLOT1_IN 0x800                /* slot 1 fades in or resumes */
#define SEF_ENTRY_SCRIPT_EVENT 0x1000           /* unitScriptEventSend(SEF_ENTRY_SCRIPT_EVENT_KIND) */
#define SEF_ENTRY_PLAY_ACTION_SOUND 0x2000             /* unitPlayActionSound with the action index */
#define SEF_ENTRY_SLOT_FADE 0x4000              /* the slot in/out bits fade instead of pause/resume */
#define SEF_ENTRY_SLOT1_PART_HIDE 0x10000       /* unitSlotHidePart */
#define SEF_ENTRY_SLOT1_PART_SHOW 0x20000       /* unitSlotShowPart */
#define SEF_ENTRY_MONS_PARTS_HIDE 0x40000       /* hides the "ck_sc3m" parts of the first monster slot */
#define SEF_ENTRY_SIDE1_UNBIND 0x800000         /* unitUnbind on every side-1 unit */
#define SEF_ENTRY_SELECTOR_MASK 0xF000000       /* unitSetSelector(unit, field - 1) when nonzero */
#define SEF_ENTRY_SELECTOR_SHIFT 24
#define SEF_ENTRY_SIDE1_BIND 0x10000000         /* unitBindSetup on every side-1 unit */
#define SEF_ENTRY_MONS_SLOT1_HIDE 0x20000000    /* monsSlotHide on the second monster slot */
#define SEF_ENTRY_DECO_SET 0x40000000           /* decoFlagsSetAll */

/* The unitScriptEventSend kind SEF_ENTRY_SCRIPT_EVENT sends. */
#define SEF_ENTRY_SCRIPT_EVENT_KIND 12

/*
 * Runs the scene actions of an attack: the flags of the sef entry of the attacker's actor (sefGetEntryFlags) select
 * them bit by bit: show/hide the actor slots and parts, fade or pause the slots (SEF_ENTRY_SLOT_FADE picks
 * fading), hide the two "ck_sc3m" parts of the first monster slot, set the unit's selector
 * (SEF_ENTRY_SELECTOR_MASK), unbind or bind the side-1 units, hide the second monster slot, set or clear the deco
 * flags, send a script event and play the action sound (unitPlayActionSound).
 */
void scenarioMtd(struct AttackCommand *command)
{
    UnitScriptEvent event;
    BattleUnit *unit;
    unsigned int flags;
    unsigned int selector;

    unit = command->attacker;
    flags = sefGetEntryFlags(unit->unit);
    if (flags & SEF_ENTRY_SLOT0_HIDE_SHOW) {
        unitSlotHideShowPart(unit, 0);
    }
    if (flags & SEF_ENTRY_SLOT0_SHOW_HIDE) {
        unitSlotShowHidePart(unit, 0);
    }
    if (flags & SEF_ENTRY_SLOT1_HIDE_SHOW) {
        unitSlotHideShowPart(unit, 1);
    }
    if (flags & SEF_ENTRY_SLOT1_SHOW_HIDE) {
        unitSlotShowHidePart(unit, 1);
    }
    if (flags & SEF_ENTRY_SLOT1_PART_HIDE) {
        unitSlotHidePart(unit);
    }
    if (flags & SEF_ENTRY_SLOT1_PART_SHOW) {
        unitSlotShowPart(unit);
    }
    if (flags & SEF_ENTRY_SLOT_FADE) {
        if (flags & SEF_ENTRY_SLOT0_OUT) {
            unitSlotFadeOutStart(unit, 0);
        }
        if (flags & SEF_ENTRY_SLOT0_IN) {
            unitSlotFadeInStart(unit, 0);
        }
        if (flags & SEF_ENTRY_SLOT1_OUT) {
            unitSlotFadeOutStart(unit, 1);
        }
        if (flags & SEF_ENTRY_SLOT1_IN) {
            unitSlotFadeInStart(unit, 1);
        }
    } else {
        if (flags & SEF_ENTRY_SLOT0_OUT) {
            unitSlotPause(unit, 0);
        }
        if (flags & SEF_ENTRY_SLOT0_IN) {
            unitSlotResume(unit, 0);
        }
        if (flags & SEF_ENTRY_SLOT1_OUT) {
            unitSlotPause(unit, 1);
        }
        if (flags & SEF_ENTRY_SLOT1_IN) {
            unitSlotResume(unit, 1);
        }
    }
    /* Each block keeps its own slots/units local: the original gives all four their own stack word. */
    if (flags & SEF_ENTRY_MONS_PARTS_HIDE) {
        struct MonsSlot **slots;

        monsActiveSlotsGet(&slots);
        XenoActorHidePart(slots[0]->actor, "ck_sc3m01");
        XenoActorHidePart(slots[0]->actor, "ck_sc3m02");
    }
    selector = flags & SEF_ENTRY_SELECTOR_MASK;
    if (selector != 0) {
        unitSetSelector(unit, (selector >> SEF_ENTRY_SELECTOR_SHIFT) - 1);
    }
    if (flags & SEF_ENTRY_SIDE1_UNBIND) {
        BattleUnit **units;
        int count;
        int i;

        count = unitTblGet(1, &units);
        for (i = 0; i < count; i++) {
            if (units[i] != 0) {
                unitUnbind(units[i]);
            }
        }
    }
    if (flags & SEF_ENTRY_SIDE1_BIND) {
        BattleUnit **units;
        int count;
        int i;

        count = unitTblGet(1, &units);
        for (i = 0; i < count; i++) {
            if (units[i] != 0) {
                unitBindSetup(units[i]);
            }
        }
    }
    if (flags & SEF_ENTRY_MONS_SLOT1_HIDE) {
        struct MonsSlot **slots;

        monsActiveSlotsGet(&slots);
        if (slots[1] != 0) {
            monsSlotHide(slots[1]);
        }
    }
    if (flags & SEF_ENTRY_DECO_SET) {
        decoFlagsSetAll();
    }
    if (flags & SEF_ENTRY_DECO_CLEAR) {
        decoFlagsClearAll();
    }
    if (flags & SEF_ENTRY_SCRIPT_EVENT) {
        event.unit = unit;
        unitScriptEventSend(SEF_ENTRY_SCRIPT_EVENT_KIND, &event);
    }
    if (flags & SEF_ENTRY_PLAY_ACTION_SOUND) {
        unitPlayActionSound(unit, command->actionIdx);
    }
}

/* unitSlotResume: clears the pause bit of the slot and sets its alpha to 1.0. */
void unitSlotResume(BattleUnit *object, int index)
{
    struct XenoActor *unit;

    unit = object->children[index];
    if (unit != 0)
    {
        unit->drawFlags &= ~4;
        unit->alpha = 1.0f;
    }
}

/* unitSlotPause: sets the pause bit of the slot and its alpha to 0. */
void unitSlotPause(BattleUnit *object, int index) {
    struct XenoActor *unit;

    unit = object->children[index];
    if (unit != 0) {
        unit->alpha = 0;
        unit->drawFlags |= 4;
    }
}

extern const char *ov01Slot1Part;

/* unitSlotShowPart (proposed unitSlotShowPart): shows part ov01Slot1Part of slot 1 when the slot exists. */
void unitSlotShowPart(BattleUnit *object) {
    struct XenoActor *unit;

    unit = object->children[1];
    if (unit != 0) {
        XenoActorShowPart(unit, ov01Slot1Part);
    }
}


/* unitSlotHidePart (proposed unitSlotHidePart): hides part ov01Slot1Part of slot 1 when the slot exists. */
void unitSlotHidePart(BattleUnit *object) {
    struct XenoActor *unit;

    unit = object->children[1];
    if (unit != 0) {
        XenoActorHidePart(unit, ov01Slot1Part);
    }
}

/* The part of the unit's own actor that stands in for a slot actor (unitSlotShowHidePart hides it while the slot
 * is shown, unitSlotHideShowPart shows it while the slot is paused). */
extern const char *ov01UnitSlotPart;

/* Shows slot index of the unit (clears the pause bit, alpha 1) and hides part ov01UnitSlotPart of its actor. */
void unitSlotShowHidePart(BattleUnit *object, int index) {
    struct XenoActor *unit;

    unit = object->children[index];
    if (unit != 0) {
        unit->drawFlags &= ~ACTOR_STATE_HIDDEN;
        unit->alpha = 1.0f;
        XenoActorHidePart(object->unit, ov01UnitSlotPart);
    }
}

/* Pauses slot index of the unit (alpha 0, pause bit) and shows part ov01UnitSlotPart of its actor. */
void unitSlotHideShowPart(BattleUnit *object, int index) {
    struct XenoActor *unit;

    unit = object->children[index];
    if (unit != 0) {
        unit->alpha = 0;
        unit->drawFlags |= ACTOR_STATE_HIDDEN;
        XenoActorShowPart(object->unit, ov01UnitSlotPart);
    }
}

/* Fades slot index of the unit in (unitSlotFadeIn). */
struct FadeTask *unitSlotFadeInStart(BattleUnit *object, int index) {
    return unitSlotFadeIn(object, index);
}

/* Fades slot index of the unit out (unitSlotFadeOut). */
struct FadeTask *unitSlotFadeOutStart(BattleUnit *object, int index) {
    return unitSlotFadeOut(object, index);
}

int eventAttackActionSkip(void) {
    return 1;
}

int eventAttackHitSkip(void) {
    return 1;
}


/*
 * Loads what the action of request needs: the motion entries of its technique (one, or two for a pair technique,
 * the partner's first when the unit is not the pair's first slot), the model of a gateFlag-5 technique's target
 * record (joint in pairSecond[2]) and its effect group. Returns 1.
 */
int actionResourcesLoad(EventQueueEntry *entry)
{
    BattleUnit *unit;
    int value;
    TecRecord *tec;
    TgtIdRecord *record;
    int first;
    int second;
    int group;

    unit = entry->data.args.unit;
    tec = tecRecordGet(entry->data.args.tecIndex);
    value = entry->data.args.pairIndex;
    ov01ModelEntry = 0;
    if (tec->timer != 0) {
        if (tec->gateFlag == TEC_GATE_PAIR) {
            if (pairLinkGet(value)->slotFirst == unit->stats->key) {
                first = 0;
                second = 1;
            } else {
                first = 1;
                second = 0;
            }
            ov01ModelEntry = dataMotEntryLoad(tec->timer, first, 0);
            ov01ModelEntry = dataMotEntryLoad(tec->timer, second, 1);
        } else {
            ov01ModelEntry = dataMotEntryLoad(tec->timer, 0, 0);
        }
    }
    if (tec->gateFlag == TEC_GATE_TARGET_MODEL) {
        record = tgtIdRecordGet(value);
        if (record->modelIndex != 0) {
            unit->stats->pairSecond[2] = record->modelJoint;
            ov01ModelEntry = dataModelEntryLoad(record->modelIndex);
        }
    }
    group = tecEffectGroupGet(unit, tec);
    sefLoadEffect((group << EFFECT_KEY_GROUP_SHIFT) | tec->extent, group);
    return 1;
}


/* 1 once no CD read and no load wait is pending. */
int ov01LoadsIdle(void)
{
    if (dataCdSync() == 0) {
        if (sefCheckLoad() == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * unitAttachRequest (proposed unitAttachRequest): the kind-8 event handler. It attaches
 * the requested unit to the model entry in ov01ModelEntry; when the slot's state is 1 it
 * also attaches the handle that unitPairPartnerFind matches for the request's value.
 */
int unitAttachRequest(EventQueueEntry *entry)
{
    BattleUnit *unit;

    if (ov01ModelEntry != 0) {
        unit = entry->data.args.unit;
        unitAttachChild(unit, ov01ModelEntry, 0);
        if (tecRecordGet(entry->data.args.tecIndex)->gateFlag == 1) {
            unitAttachChild(unitPairPartnerFind(unit, entry->data.args.pairIndex), ov01ModelEntry, 1);
        }
    }
    return 1;
}

/*
 * unitSpawnFromRequest (proposed unitSpawnFromRequest): the kind-9 event handler. It creates
 * the requested unit (unitPlCreate for ids outside 0x10..0x8C, else unitEnCreate),
 * loads its snapshot and angle, loads its data and keeps it in ov01LastUnit with flag 8.
 */
int unitSpawnFromRequest(EventQueueEntry *entry)
{
    int id;
    int index;
    int value;

    id = entry->data.args.actions;
    index = entry->data.args.tecIndex;
    value = entry->data.args.pairIndex;
    if ((u32) (id - 0x10) >= 0x7D) {
        ov01LastUnit = unitPlCreate(id);
    } else {
        ov01LastUnit = unitEnCreate(id);
    }
    unitLoadSnapshot(ov01LastUnit, index);
    unitSetAngleDegrees(ov01LastUnit, value);
    dataUnitFileLoad(id);
    unitEffectSlotAcquire(id);
    ov01LastUnit->flags |= 8;
    return 1;
}


/* Bind event handler: binds the data slot of request id to ov01LastUnit, fades
 * it in and starts the load (returns 0); on the next calls returns 1 once the
 * load is done. 0 also when the slot is not found. */
int unitBindRequest(EventQueueEntry *entry)
{
    struct DataSlot *slot;

    if (eventStepStarted == 0) {
        slot = dataSlotFind(entry->data.args.actions);
        if (slot == 0) {
            return 0;
        }
        unitBindActors(ov01LastUnit, slot);
        unitFadeIn(ov01LastUnit);
        statEffOn(ov01LastUnit);
        ov01LoadRequest(slot);
        eventStepStarted = 1;
    } else if (ov01LoadIsDone() != 0) {
        return 1;
    }
    return 0;
}

/* Object flags a unit loses when it leaves the action stock here (UNIT_FLAG_STOCKED and two more). */
#define UNIT_FLAGS_STOCK_CLEAR 0x4208

/* Battle unit keys from this one on are monsters (pool B); below are the party slots. */
#define UNIT_KEY_POOL_B_FIRST 16

/* menuStatPanelNew kind battleExecDead opens for a defeated monster in action step 2. */
#define STAT_PANEL_DEFEAT 7

/* unitScriptEventSend kind battleExecDead sends for a unit that is taken out, with the index of its source. */
#define DEFEAT_SCRIPT_EVENT 8

/*
 * For every unit: a unit that cannot act or has no HP left (and is not the battle state's last object) is taken out
 * of the action stock; then, once its move commands are queued, a monster adds its battle result, arms the menus,
 * releases its sequence and aura effects (and opens stat panel 7 in action step 2 when its key record has a step
 * rate), a party unit releases its effects through statEffOff; both report to the unit's script (event 8) with
 * the index of the unit's source. Returns 1.
 */
int battleExecDead(void)
{
    UnitScriptEvent event;
    struct UnitTableRef table;
    int stockIndex;
    int count;
    int i;

    count = unitTblGet(UNIT_SIDE_ALL, &table.units);
    for (i = 0; i < count; i++) {
        if (table.units[i] == 0) {
            continue;
        }
        if (calcBattleChk(table.units[i]) == 0 || table.units[i]->stats->hp == 0) {
            if (table.units[i] != manWorkGet()->lastObject) {
                while (actStockRemove(table.units[i], &stockIndex) != 0) {
                    if (stockIndex > 0) {
                        menuStockDead(stockIndex);
                    }
                }
                table.units[i]->flags &= ~UNIT_FLAGS_STOCK_CLEAR;
            }
        }
        if (unitCmdDeadSet(table.units[i]) == 0) {
            continue;
        }
        if (table.units[i]->stats->key >= UNIT_KEY_POOL_B_FIRST) {
            monsGainSet(table.units[i]);
            menuUnitPanelArmTimer();
            menuIconListClose();
            menuComboPanelArmTick();
            sefApplyRecordMode(table.units[i]->unit, SEQUENCE_EFFECT_KEY(table.units[i]->stats->sequenceIndex),
                               SEF_RECORD_MODE_RELEASE);
            sefApplyRecordMode(table.units[i]->unit, AURA_KEY(1), SEF_RECORD_MODE_RELEASE);
            sefApplyRecordMode(table.units[i]->unit, AURA_KEY(2), SEF_RECORD_MODE_RELEASE);
            sefApplyRecordMode(table.units[i]->unit, AURA_KEY(3), SEF_RECORD_MODE_RELEASE);
            if (actStepCounterValue() == 2 && enemyRewardRecordGet(table.units[i]->stats->key)->stepRate != 0) {
                menuStatPanelNew(STAT_PANEL_DEFEAT);
            }
        } else {
            statEffOff(table.units[i]);
        }
        event.unit = table.units[i];
        if (table.units[i]->stats->source != 0) {
            event.words[0] = unitNoGet(table.units[i]->stats->source);
        } else {
            event.words[0] = 0;
        }
        unitScriptEventSend(DEFEAT_SCRIPT_EVENT, &event);
    }
    return 1;
}

int stat1Proc(BattleUnit *unit, int commandKind, int hitDamage, int hitMode);

/*
 * battleResolveStatus (proposed battleResolveStatus): the kind-12 event handler. While the
 * battle object has status bits in rows 1-3 it waits for its queue (returns 0); then it
 * resolves the object and each unit with state 0: HP adjust, level add and act stock
 * for bit 0x4000 of row 3, HP adjust for bit 0x400, and both bits are cleared.
 */
int battleResolveStatus(void) {
    BattleUnit **units;
    BattleUnit *object;
    BattleUnit *unit;
    int commandKind;
    int hitDamage;
    int hitMode;
    int count;
    int i;
    int result;

    object = manWorkGet()->object;
    commandKind = manWorkGet()->commandKind;
    hitDamage = manWorkGet()->hitDamage;
    hitMode = manWorkGet()->hitMode;
    if (object != 0 && (calcStatGet(object, 1, 0xFFFF) != 0 || calcStatGet(object, 2, 0xFFFF) != 0 || calcStatGet(object, 3, 0xFFFF) != 0)) {
        result = unitQueueIsEmpty(object);
        if (result == 0) {
            return 0;
        }
        stat1Proc(object, commandKind, hitDamage, hitMode);
    }
    count = unitTblGet(2, &units);
    for (i = 0; i < count; i++) {
        unit = units[i];
        if (unit != 0 && unit->stats->hp == 0) {
            if (calcStatGet(unit, 3, 0x4000) != 0) {
                calcHp(units[i], 1);
                unitLevelAdd(units[i], 3);
                if (calcBoostChk(units[i], 0) != 0) {
                    calcBoost(units[i], 0);
                    unitEffectStart(units[i], 0x140, 7);
                    seVoiceStartTagged(2, 1);
                }
                calcStatReset(units[i], 3, 0x4000);
                calcStatProtectReset(units[i], 3, 0x4000);
            } else if (calcStatGet(units[i], 3, 0x400) != 0) {
                calcHp(units[i], 1);
                calcStatReset(units[i], 3, 0x400);
                calcStatProtectReset(units[i], 3, 0x400);
            }
        }
    }
    return 1;
}

/* The statuses stat1Proc applies at the end of a step (calcStatGet rows 1 to 3). */
#define STAT1_HP_DRAIN 0x2          /* row 1: loses a tenth of maxHp */
#define STAT2_HP_DRAIN 0x2          /* row 2: loses a quarter of maxHp */
#define STAT2_HIT_REPEAT 0x1000     /* row 2: takes the step's plain hit damage again */
#define STAT1_EP_DRAIN 0x100        /* row 1: loses STATUS_EP_DRAIN EP */
#define STAT3_EP_REGEN 0x1000       /* row 3: gains STATUS_EP_REGEN EP */
#define STAT1_CURRENT_DRAIN 0x4000  /* row 1: loses a quarter of currentMax */
#define STAT2_CURRENT_DRAIN 0x4000  /* row 2: loses half of currentMax */
#define STATUS_EP_DRAIN 5
#define STATUS_EP_REGEN 2
/* objDmgNumCreate kinds (as calcHpDmg numbers the hit kinds) and the flag of an amount from a status; the amounts
 * of one step are shown STATUS_AMOUNT_DELAY frames apart. */
#define AMOUNT_HP_LOSS 1
#define AMOUNT_HP_GAIN 2
#define AMOUNT_EP_LOSS 3
#define AMOUNT_EP_GAIN 4
#define AMOUNT_STATUS 0x400
#define STATUS_AMOUNT_DELAY 50

/*
 * Applies the end-of-step statuses of unit: HP from the hit damage (a HIT_KIND_PLAIN hit with STAT2_HIT_REPEAT) and
 * the HP drains, EP from STAT1_EP_DRAIN and STAT3_EP_REGEN, current from the current drains. Each nonzero change is
 * queued as the unit's reaction, applied and shown (objDmgNumCreate). Returns 1 and clears the unit's source when
 * anything changed. commandKind is not used.
 */
int stat1Proc(BattleUnit *unit, int commandKind, int hitDamage, int hitMode)
{
    struct HitRecord hit;
    int hp;
    int ep;
    int current;
    int delay;
    int delta;
    int kind;

    hp = 0;
    current = 0;
    delay = 0;
    if ((hitMode & HIT_MODE_KIND_MASK) == HIT_KIND_PLAIN && calcStatGet(unit, STAT_ROW_2, STAT2_HIT_REPEAT)) {
        hp = hitDamage;
    }
    if (calcStatGet(unit, STAT_ROW_1, STAT1_HP_DRAIN)) {
        hp += unit->stats->maxHp / 10;
    }
    if (calcStatGet(unit, STAT_ROW_2, STAT2_HP_DRAIN)) {
        hp += unit->stats->maxHp / 4;
    }
    ep = 0;
    if (calcStatGet(unit, STAT_ROW_1, STAT1_EP_DRAIN)) {
        ep = STATUS_EP_DRAIN;
    }
    if (calcStatGet(unit, STAT_ROW_3, STAT3_EP_REGEN)) {
        ep -= STATUS_EP_REGEN;
    }
    if (calcStatGet(unit, STAT_ROW_1, STAT1_CURRENT_DRAIN)) {
        current = unit->stats->currentMax / 4;
    }
    if (calcStatGet(unit, STAT_ROW_2, STAT2_CURRENT_DRAIN)) {
        current += unit->stats->currentMax / 2;
    }
    if (hp != 0) {
        if (hp > 0) {
            hit.reaction = UNIT_CMD_MODE_DISPATCH;
        } else {
            hit.reaction = UNIT_CMD_CLEAR_ACTIVE;
        }
        hit.target = unit;
        hit.damage = hp;
        hit.followResult = 0;
        hit.backAttack = 0;
        scenarioBatExecDst(0, &hit);
        /* The negation is written after the queue call (sched1 hoists it above the call, after the
         * argument setup); written before the call it lands between the two argument moves. */
        delta = -hp;
        calcHp(unit, delta);
        kind = hp > 0 ? AMOUNT_HP_LOSS : AMOUNT_HP_GAIN;
        objDmgNumCreate(unit, hp < 0 ? delta : hp, kind | AMOUNT_STATUS, delay);
        delay += STATUS_AMOUNT_DELAY;
    }
    if (ep != 0) {
        if (ep > 0) {
            hit.reaction = UNIT_CMD_MODE_DISPATCH;
        } else {
            hit.reaction = UNIT_CMD_CLEAR_ACTIVE;
        }
        hit.target = unit;
        hit.damage = ep;
        hit.followResult = 0;
        hit.backAttack = 0;
        scenarioBatExecDst(0, &hit);
        delta = -ep;
        calcEp(unit, delta);
        kind = ep > 0 ? AMOUNT_EP_LOSS : AMOUNT_EP_GAIN;
        objDmgNumCreate(unit, ep < 0 ? delta : ep, kind | AMOUNT_STATUS, delay);
        delay += STATUS_AMOUNT_DELAY;
    }
    if (current != 0) {
        if (current > 0) {
            hit.reaction = UNIT_CMD_MODE_DISPATCH;
        } else {
            hit.reaction = UNIT_CMD_CLEAR_ACTIVE;
        }
        hit.target = unit;
        hit.damage = current;
        hit.followResult = 0;
        hit.backAttack = 0;
        scenarioBatExecDst(0, &hit);
        delta = -current;
        unitCurrentAdd(unit, delta);
        kind = current > 0 ? AMOUNT_EP_LOSS : AMOUNT_EP_GAIN;
        objDmgNumCreate(unit, current < 0 ? delta : current, kind | AMOUNT_STATUS, delay);
    }
    if (hp != 0 || ep != 0 || current != 0) {
        unit->stats->source = 0;
        return 1;
    }
    return 0;
}

/* Row-3 statuses that bring a down unit back when the battle step ends (stat2Proc): with its full
 * maxHp, or with half of it. */
#define STAT3_REVIVE_FULL 0x800
#define STAT3_REVIVE_HALF 0x2000

int stat2Proc(void);

/* Waits (returns 0) while a side-2 unit with a revive status (row 3) still has queued commands; then runs
 * stat2Proc and returns 1. */
int battleStatusQueueWait(void)
{
    BattleUnit **units;
    BattleUnit *unit;
    int count;
    int i;

    count = unitTblGet(2, &units);
    for (i = 0; i < count; i++) {
        unit = units[i];
        if (unit != 0) {
            if (calcStatGet(unit, 3, STAT3_REVIVE_FULL) != 0 || calcStatGet(unit, 3, STAT3_REVIVE_HALF) != 0) {
                if (unitQueueIsEmpty(unit) == 0) {
                    return 0;
                }
            }
        }
    }
    stat2Proc();
    return 1;
}

/* Brings back every down side-2 unit with a revive status: clears the status, queues
 * UNIT_CMD_PHASE_2B_WAIT, restores maxHp (or half of it), clears UNIT_FLAG_DOWN and shows the amount
 * (objDmgNumCreate with flags 0x802). Returns 1. */
int stat2Proc(void)
{
    Ov01Command command;
    BattleUnit **units;
    BattleUnit *unit;
    int count;
    int i;
    int amount;
    int shown;

    count = unitTblGet(2, &units);
    for (i = 0; i < count; i++) {
        unit = units[i];
        amount = 0;
        if (unit == 0) {
            continue;
        }
        if (unit->flags & UNIT_FLAG_DOWN) {
            if (calcStatGet(unit, 3, STAT3_REVIVE_FULL) != 0) {
                amount = unit->stats->maxHp;
                calcStatReset(unit, 3, STAT3_REVIVE_FULL);
                calcStatProtectReset(unit, 3, STAT3_REVIVE_FULL);
            } else if (calcStatGet(unit, 3, STAT3_REVIVE_HALF) != 0) {
                amount = unit->stats->maxHp / 2;
                calcStatReset(unit, 3, STAT3_REVIVE_HALF);
                calcStatProtectReset(unit, 3, STAT3_REVIVE_HALF);
            }
        }
        if (amount != 0) {
            command.command = UNIT_CMD_PHASE_2B_WAIT;
            unitCmdSet(unit, &command);
            calcHp(unit, amount);
            unit->flags &= ~UNIT_FLAG_DOWN;
            shown = -amount;
            if (amount >= 0) {
                shown = amount;
            }
            objDmgNumCreate(unit, shown, 0x802, 0);
        }
    }
    return 1;
}

extern struct MonsSlot *ov01StreamSlot;

/*
 * ov01StreamOpen (proposed ov01StreamOpen): the kind-14 event handler. It stores the
 * MonsSlot that monsSlotCreate creates (monsSlotCreate, mons_slot.c) in ov01StreamSlot and opens
 * data stream 0x43.
 */
int ov01StreamOpen(void) {
    ov01StreamSlot = monsSlotCreate();
    dataStreamOpen(0x43, 1);
    return 1;
}

/* Once data stream 0x43 (opened by ov01StreamOpen) is found, hands it to the
 * MonsSlot in ov01StreamSlot (monsSlotCopyParams) and returns 1; 0 until then. */
int ov01StreamBind(void)
{
    struct DataStreamEntry *stream = dataStreamFind(0x43);

    if (stream == 0) {
        return 0;
    }
    monsSlotCopyParams(ov01StreamSlot, stream);
    return 1;
}

/*
 * The pair technique events place three units: the commanding unit (args unit), its partner (args target) and the
 * unit's partner object. Index 0 is the unit, 1 the partner, 2 the partner object: pairEventPos holds the spots
 * eventPairBegin moves them to, pairSavedPos and pairSavedAngle the positions and facing angles it replaced, which
 * eventPairEnd puts back.
 */
extern DoublewordAlignedVector4 pairEventPos[3];
extern DoublewordAlignedVector4 pairSavedPos[3];
extern float pairSavedAngle[3];

/* The unit key whose slot 0 is resumed for the pair techniques PAIR_SLOT_PAIR_A and PAIR_SLOT_PAIR_B (and paused
 * again at the end). */
#define PAIR_SLOT_UNIT_KEY 3
#define PAIR_SLOT_PAIR_A 10
#define PAIR_SLOT_PAIR_B 16
/* The partner object key that stands at a fixed depth instead of -unitDefArea. */
#define PAIR_FIXED_DEPTH_KEY 0x8C
#define PAIR_FIXED_DEPTH -10.0f

/*
 * EVENT_KIND_PAIR_BEGIN handler. Step 0 takes the pair's required level off both units, binds every unit again,
 * hides the first monster slot, sets the default sef gate for the technique and starts the screen flash (returns
 * 0). Step 1 counts ATTACK_FILL_FRAMES calls and, as the original does, falls through into step 2 on every call.
 * Step 2 moves the partner, the unit and (unless the technique has TEC_TARGET_ALL) the unit's partner object to their
 * pairEventPos spots, saving positions and angles, unbinds them, hides the first monster slot, shows the stream slot
 * and returns 1.
 * The positions are copied with __builtin_memcpy: a struct assignment from an array element moves the vector with
 * ldl/ldr, and its stores keep the partner object's stats load from staying after them.
 */
int eventPairBegin(EventQueueEntry *entry)
{
    struct MonsSlot **slots;
    BattleUnit **units;
    BattleUnit *object;
    BattleUnit *other;
    struct XenoActor *actor;
    int tecIndex;
    int pairIndex;
    int count;
    int i;

    tecIndex = entry->data.args.tecIndex;
    pairIndex = entry->data.args.pairIndex;
    switch (eventStepStarted) {
    case 0:
        object = entry->data.args.unit;
        unitLevelAdd(object, -pairLinkGet(pairIndex)->requiredLevel);
        object = entry->data.args.target;
        unitLevelAdd(object, -pairLinkGet(pairIndex)->requiredLevel);
        count = unitTblGet(UNIT_SIDE_ALL, &units);
        for (i = 0; i < count; i++) {
            other = units[i];
            if (other != 0) {
                other->unit->drawFlags &= ~ACTOR_STATE_UNBOUND;
                unitBindSetup(other);
            }
        }
        monsActiveSlotsGet(&slots);
        monsSlotHide(slots[0]);
        sefSetGateDefault(0, tecRecordGet(tecIndex)->extent);
        decoFlagsSetAll();
        screenFlashStart();
        attackFillWait = 0;
        eventStepStarted++;
        break;
    case 1:
        if (attackFillWait >= ATTACK_FILL_FRAMES) {
            eventStepStarted = 2;
        } else {
            attackFillWait++;
        }
        /* fall through */
    case 2:
        object = entry->data.args.target;
        actor = object->unit;
        __builtin_memcpy(&pairSavedPos[1], &actor->position, sizeof(Vector4));
        __builtin_memcpy(&actor->position, &pairEventPos[1], sizeof(Vector4));
        pairSavedAngle[1] = actor->rotation.y;
        actor->rotation.y = ANGLE_HALF_TURN;
        unitUnbind(object);
        if (object->stats->key == PAIR_SLOT_UNIT_KEY
            && (pairIndex == PAIR_SLOT_PAIR_A || pairIndex == PAIR_SLOT_PAIR_B)) {
            unitSlotResume(object, 0);
        }
        object = entry->data.args.unit;
        actor = object->unit;
        __builtin_memcpy(&pairSavedPos[0], &actor->position, sizeof(Vector4));
        __builtin_memcpy(&actor->position, &pairEventPos[0], sizeof(Vector4));
        pairSavedAngle[0] = actor->rotation.y;
        actor->rotation.y = ANGLE_HALF_TURN;
        unitUnbind(object);
        if (object->stats->key == PAIR_SLOT_UNIT_KEY
            && (pairIndex == PAIR_SLOT_PAIR_A || pairIndex == PAIR_SLOT_PAIR_B)) {
            unitSlotResume(object, 0);
        }
        if (!(tecRecordGet(tecIndex)->flags & TEC_TARGET_ALL)) {
            object = (BattleUnit *) object->commandTarget;
            actor = object->unit;
            __builtin_memcpy(&pairSavedPos[2], &actor->position, sizeof(Vector4));
            __builtin_memcpy(&actor->position, &pairEventPos[2], sizeof(Vector4));
            if (object->stats->key == PAIR_FIXED_DEPTH_KEY) {
                actor->position.z = PAIR_FIXED_DEPTH;
            } else {
                actor->position.z = -unitDefArea(object);
            }
            pairSavedAngle[2] = actor->rotation.y;
            actor->rotation.y = 0.0f;
            unitUnbind(object);
        }
        monsActiveSlotsGet(&slots);
        monsSlotHide(slots[0]);
        monsSlotShow(ov01StreamSlot);
        return 1;
    }
    return 0;
}

/* unitScriptEventSend kind of the pair end, with the commanding unit. */
#define PAIR_END_SCRIPT_EVENT 12

/*
 * EVENT_KIND_PAIR_END handler. Step 0 binds the partner and the unit again (pausing slot 0 of the PAIR_SLOT_UNIT_KEY
 * unit for the pair techniques PAIR_SLOT_PAIR_A and PAIR_SLOT_PAIR_B) and the unit's partner object unless the
 * technique has TEC_TARGET_ALL, hides the stream slot and starts the screen flash (returns 0). Step 1 counts
 * ATTACK_FILL_FRAMES calls and falls through into step 2 on every call, as in eventPairBegin. Step 2 puts the saved
 * positions and angles back, unbinds every unit, releases the stream slot, shows the first monster slot, refreshes
 * the partner's value, clears the second stock boost, sets the default sef gate, clears the deco flags, sends the
 * pair end script event for the unit and returns 1.
 */
int eventPairEnd(EventQueueEntry *entry)
{
    UnitScriptEvent event;
    BattleUnit **units;
    struct MonsSlot **slots;
    BattleUnit *object;
    BattleUnit *other;
    struct XenoActor *actor;
    int tecIndex;
    int pairIndex;
    int count;
    int i;

    tecIndex = entry->data.args.tecIndex;
    pairIndex = entry->data.args.pairIndex;
    switch (eventStepStarted) {
    case 0:
        object = entry->data.args.target;
        unitBindSetup(object);
        if (object->stats->key == PAIR_SLOT_UNIT_KEY
            && (pairIndex == PAIR_SLOT_PAIR_A || pairIndex == PAIR_SLOT_PAIR_B)) {
            unitSlotPause(object, 0);
        }
        object = entry->data.args.unit;
        unitBindSetup(object);
        if (object->stats->key == PAIR_SLOT_UNIT_KEY
            && (pairIndex == PAIR_SLOT_PAIR_A || pairIndex == PAIR_SLOT_PAIR_B)) {
            unitSlotPause(object, 0);
        }
        if (!(tecRecordGet(tecIndex)->flags & TEC_TARGET_ALL)) {
            unitBindSetup((BattleUnit *) object->commandTarget);
        }
        monsSlotHide(ov01StreamSlot);
        screenFlashStart();
        attackFillWait = 0;
        eventStepStarted++;
        break;
    case 1:
        if (attackFillWait >= ATTACK_FILL_FRAMES) {
            eventStepStarted = 2;
        } else {
            attackFillWait++;
        }
        /* fall through */
    case 2:
        object = entry->data.args.target;
        actor = object->unit;
        __builtin_memcpy(&actor->position, &pairSavedPos[1], sizeof(Vector4));
        actor->rotation.y = pairSavedAngle[1];
        object = entry->data.args.unit;
        actor = object->unit;
        __builtin_memcpy(&actor->position, &pairSavedPos[0], sizeof(Vector4));
        actor->rotation.y = pairSavedAngle[0];
        if (!(tecRecordGet(tecIndex)->flags & TEC_TARGET_ALL)) {
            object = (BattleUnit *) object->commandTarget;
            actor = object->unit;
            __builtin_memcpy(&actor->position, &pairSavedPos[2], sizeof(Vector4));
            actor->rotation.y = pairSavedAngle[2];
        }
        count = unitTblGet(UNIT_SIDE_ALL, &units);
        for (i = 0; i < count; i++) {
            other = units[i];
            if (other != 0) {
                other->unit->drawFlags |= ACTOR_STATE_UNBOUND;
                unitUnbind(other);
            }
        }
        monsSlotRelease(ov01StreamSlot);
        monsActiveSlotsGet(&slots);
        monsSlotShow(slots[0]);
        calcTurnEnd(entry->data.args.target);
        actStockClearBoost1();
        sefSetGateDefault(1, tecRecordGet(tecIndex)->extent);
        decoFlagsClearAll();
        event.unit = entry->data.args.unit;
        unitScriptEventSend(PAIR_END_SCRIPT_EVENT, &event);
        return 1;
    }
    return 0;
}

/* Data of this TU, in address order. */
extern const char kosmosRightArmPartName[];
extern const char jinScabbardHiltPartName[];

const char *ov01UnitSlotPart = kosmosRightArmPartName;
const char *ov01Slot1Part = jinScabbardHiltPartName;
int itemPickFallbackItems[3] = { 1, 5, 11 };
int itemPickCommonItems[2] = { 12, 13 };
int itemPickUncommonItems[3] = { 21, 2, 6 };
int itemPickRareItems[4] = { 8, 15, 25, 26 };
ItemPickGroup itemPickGroups[ITEM_PICK_GROUPS] = {
    { 3, 4, itemPickRareItems },
    { 12, 3, itemPickUncommonItems },
    { 50, 2, itemPickCommonItems },
    { 100, 3, itemPickFallbackItems },
};
DoublewordAlignedVector4 pairEventPos[3] = {
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};
int (*eventQueueHandlers[EVENT_HANDLER_SLOTS])() = {
    eventNoneRun, eventUnitCommand, (int (*)()) eventWaitUnitQueue, eventAttackRun, eventAttackActionSkip, eventAttackHitSkip,
    actionResourcesLoad, ov01LoadsIdle, unitAttachRequest, unitSpawnFromRequest, unitBindRequest,
    battleExecDead, battleResolveStatus, battleStatusQueueWait, ov01StreamOpen, ov01StreamBind,
    eventPairBegin, eventPairEnd, eventStockBegin, eventActStockBoostClear, eventWaitCountdown,
};
DoublewordAlignedVector4 pairSavedPos[3] = { 0 };
float pairSavedAngle[3] = { 0 };

char cmdList[1024];
struct FifoRing cmdFifo;
EventQueueEntry eventQueueTable[EVENT_QUEUE_COUNT];
int eventQueueCount;
int eventQueueCursor;
int eventRetryCount;
int eventStepStarted;
int attackFillWait;
int attackStartedState;
BattleUnit *ov01LastUnit;
struct MonsSlot *ov01StreamSlot;
struct DataEntry *ov01ModelEntry;
int attackLastActions;
int attackLastHits;
int attackStep;
char itemPickText[128];
