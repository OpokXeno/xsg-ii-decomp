#ifndef INCLUDE_OV01_CMD_EXEC_H
#define INCLUDE_OV01_CMD_EXEC_H

/* Functions of src/ov01/cmd_exec.c that other files call. */

#include "ov01/unit_cmd.h"
#include "ov01/calc.h"

struct BattleUnit;

/* A unit command: six words, copied whole into the command list (cmdListSet) and into kind-1 events
 * (eventQueueAddUnitCommand). command is its index; the pair command of cmdPairRun names its pair record in pairIndex. */
typedef struct CmdListBody {
    int command;                        /* +0x00 */
    unsigned char unmodeled_04[0x4];
    int code;                           /* +0x08: the key-row entry, technique or argument thinkDispatch passes */
    /* +0x0C: the target unit of the think commands as a word, like CmdListEntry.unit (think command 14 stores 1
     * here). One int member, not a union of a pointer and an int: a store through a union conflicts with every
     * load, and cmdExecute's original loads unit->stats before its arg.target store reaches memory. */
    struct {
        int target;
    } arg;
    int pairIndex;                      /* +0x10: the pair record unitPairPartnerFind reads; the battle lookup of
                                         * think command 3 */
    int attackKind;                     /* +0x14: copied into AttackCommand.kind by cmdFindFirstTarget */
} CmdListBody;

/* Command kinds (CmdListBody.command): each is the unitCommandTable handler (unit_cmd.c) the unit runs once cmdExecute
 * has queued the command. */
enum {
    CMD_KIND_NONE = 0,                  /* unitCmdNoop */
    CMD_KIND_TECHNIQUE = 1,             /* unitCmdSetPhase: technique `code` on the target */
    CMD_KIND_WAIT = 2,                  /* unitCmdStatEffect: the recovery of a waiting unit */
    CMD_KIND_LOOKUP = 3,                /* unitCmdAction: the technique of battle lookup record pairIndex */
    CMD_KIND_ITEM = 4,                  /* unitCmdPlayVoice: item (hit action record) pairIndex */
    CMD_KIND_SWAP = 5,                  /* unitCmdRelease: the unit leaves and cmdExecute spawns unit `code` in its
                                         * place (unitSpawnFromRequest) */
    CMD_KIND_SNAPSHOT = 6,              /* unitCmdLoadSnapshot: the unit fades over to snapshot `code` */
    CMD_KIND_LEAVE = 7,                 /* unitCmdSetBattleTransition: every unit of the side fades out of the battle */
    CMD_KIND_REFRESH = 8,               /* unitCmdRefreshPhase */
    CMD_KIND_THINK_END = 9,             /* unitCmdUnbind: the end of a unit's think step (thinkSendEnd) */
    CMD_KIND_CODE_WAIT = 14,            /* unitCmdMot: sets phase `code`, then waits for unitIsDone when the
                                         * wait flag (arg.target) is set */
    CMD_KIND_REMOVE = 37                /* unitCmdRemove: unitRemove */
};


/* Payloads the appenders copy whole into a record (four, six and eleven words). */

/* The four-word payload of eventQueueAdd. */
typedef struct EventQueueArgs {
    union {
        int actions;                    /* kind 3: the kind-4 records of the attack */
        BattleUnit *unit;               /* the commanding unit (cmdExecute); kind 19 compares it with its own unit */
        int countdown;                  /* kind 20: the steps eventWaitCountdown has left */
    };                                  /* +0x00 */
    union {
        int tecIndex;                   /* the tec whose EP cost cmdCombinedEtherRun charges */
        int hits;                       /* kind 3: the kind-5 records (targets) of the attack */
    };                                  /* +0x04 */
    int pairIndex;                      /* +0x08: the command's pair record (CmdListBody.pairIndex, pairLinkGet) */
    BattleUnit *target;                 /* +0x0C: the partner / stocked unit of events 0x10 and 0x12 */
} EventQueueArgs;

/* One 0x30-byte record of eventQueueTable (eventQueueTable); kind is its first word,
 * the payload layout depends on the appender. */
typedef struct EventQueueEntry {
    int kind;
    union {
        EventQueueArgs args;          /* eventQueueAdd */
        struct AttackCommand attack;  /* eventQueueAddAttackAction: the attack (unitJobTypeAtk) */
        struct {
            BattleUnit *object;
            CmdListBody args;
        } kind1;                      /* eventQueueAddUnitCommand */
        struct HitRecord kind5;       /* eventQueueAddAttackHit: the hit record of one target */
    } data;
} EventQueueEntry;

/* Record kinds by their handler in eventQueueHandlers (the kinds the code here queues and event_stock handles). */
enum {
    EVENT_KIND_WAIT_UNIT_QUEUE = 2,     /* eventWaitUnitQueue */
    EVENT_KIND_ATTACK = 3,              /* eventAttackRun: shows the attack (unitsMarkUninvolved) */
    EVENT_KIND_ACTION_LOAD = 6,         /* actionResourcesLoad */
    EVENT_KIND_LOADS_IDLE = 7,          /* ov01LoadsIdle */
    EVENT_KIND_ATTACH = 8,              /* unitAttachRequest */
    EVENT_KIND_SPAWN = 9,               /* unitSpawnFromRequest */
    EVENT_KIND_BIND = 10,               /* unitBindRequest */
    EVENT_KIND_REMOVE_DEFEATED = 11,    /* battleExecDead */
    EVENT_KIND_RESOLVE_STATUS = 12,     /* battleResolveStatus */
    EVENT_KIND_STATUS_QUEUE_WAIT = 13,  /* battleStatusQueueWait */
    EVENT_KIND_STREAM_OPEN = 14,        /* ov01StreamOpen */
    EVENT_KIND_STREAM_BIND = 15,        /* ov01StreamBind */
    EVENT_KIND_PAIR_BEGIN = 16,         /* eventPairBegin: unbinds the pair and hides its monster slots */
    EVENT_KIND_PAIR_END = 17,           /* eventPairEnd: shows and releases the monster slots again */
    EVENT_KIND_STOCK_BEGIN = 18,        /* eventCombinedEtherBegin */
    EVENT_KIND_STOCK_BOOST_CLEAR = 19,  /* eventActStockBoostClear */
    EVENT_KIND_WAIT_COUNTDOWN = 20      /* eventWaitCountdown */
};

void cmdListPtrGet(void **fifo_out, void **list_out);
void cmdListSet(struct BattleUnit *unit, CmdListBody *command);

/* The unit effects the object starts or stops by effect key (keyHigh, keyLow); the object is the unit of
 * ov01/unit_cmd.h. */
void unitEffectStart(struct BattleUnit *object, int keyHigh, int keyLow);
void unitEffectStartMode(struct BattleUnit *object, int keyHigh, int keyLow);
void statEffOn(struct BattleUnit *object);
void statEffOff(struct BattleUnit *object);
void statEffProc(struct BattleUnit *object);
void cmdSystemInit(void);
int battleExecNorm(void);
int eventQueueRun(void);

#endif /* INCLUDE_OV01_CMD_EXEC_H */
