#ifndef INCLUDE_OV01_TU033_H
#define INCLUDE_OV01_TU033_H

/* Functions of src/ov01/tu033.c that other files call. */

struct Ov01Object;

/* A unit command: six words, copied whole into the command list (cmdListSet) and into kind-1 events
 * (eventQueueAddKind1). command is its index; the pair command of cmdPairRun names its pair record in pairIndex. */
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
    unsigned char unmodeled_14[0x4];
} CmdListBody;

/* Command kinds (CmdListBody.command): each is the unitCommandTable handler (tu012) the unit runs once cmdExecute
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
    CMD_KIND_CODE_WAIT = 14,            /* unitCmdCodeWait: sets phase `code`, then waits for unitIsDone when the
                                         * wait flag (arg.target) is set */
    CMD_KIND_REMOVE = 37                /* unitCmdRemove: unitRemove */
};

void cmdListPtrGet(void **fifo_out, void **list_out);
void cmdListSet(void *unit, CmdListBody *command);

/* The unit effects the object starts or stops by effect key (keyHigh, keyLow); the object is the unit of ov01/tu012.h. */
void unitEffectStart(struct Ov01Object *object, int keyHigh, int keyLow);
void unitEffectStartMode(struct Ov01Object *object, int keyHigh, int keyLow);
void func_00A955C8(struct Ov01Object *object);
void func_00A95608(struct Ov01Object *object);
void unitStatusAuraUpdate(struct Ov01Object *object);
void cmdSystemInit(void);

#endif /* INCLUDE_OV01_TU033_H */
