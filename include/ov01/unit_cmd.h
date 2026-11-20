#ifndef INCLUDE_OV01_UNIT_CMD_H
#define INCLUDE_OV01_UNIT_CMD_H

#include "shared.h"
#include "ov01/obj_slot.h"
#include "ov01/obj.h"
#include "ov01/data_record_get.h"

/* Functions of src/ov01/unit_cmd.c that other files call. */

/* Bits of XenoActor.drawFlags (+0xBC): unbound (set before unitUnbind, cleared before unitBindSetup), hidden, and
 * the fade flag set while a fade runs. */
#define ACTOR_STATE_UNBOUND 0x1
#define ACTOR_STATE_HIDDEN 0x4
#define ACTOR_STATE_FADING 0x10000
/* Default progress step of a fade (per frame, unit_fade.c and mons_slot_fade.c). */
#define FADE_DEFAULT_STEP 0.1f

typedef struct ObjectTask ObjectTask;
void unitInit(void);

struct BattleUnit;

/* One 12-byte entry of an attach record; only its first word is read here. */
struct AttachEntry {
    int value;
    unsigned char unmodeled_04[0x8];
};

/*
 * The model entry (DataEntry, data_file_load.h) as unitAttachChild reads it: from +0xC the buffer words of its file
 * records, 12 bytes apart. Entries 0 to 3 (the model files) give the new actor's resources; entry index + 4 (the
 * two motions, the callers pass indexes 0 and 1) gives its rsrc[XAct_Rsrc_5].
 */
struct AttachRecord {
    unsigned char unmodeled_00[0xC];
    struct AttachEntry entries[6];
};

struct DataEntry;
struct DataSlot;

/* Binds the unit's actors to the file buffers of its data slot (unitBind, the scene load steps). */
int unitBindActors(struct BattleUnit *unit, struct DataSlot *slot);

/* Attaches a new child actor to the unit from the model entry (read through AttachRecord). */
int unitAttachChild(struct BattleUnit *unit, struct DataEntry *entry, int index);

/* The unit object of OV01 (unit_cmd.c and the callers that reach it): its types and unit command prototypes. */

/* A unit has three child units (and three slot flags, one per child); the command table has 40 handlers. */
#define UNIT_CHILD_COUNT 3
#define UNIT_COMMAND_COUNT 40

/* Command-table indices (unitCommandTable in src/ov01/unit_cmd.c) that the unit move and mode handlers store as step status. */
enum {
    UNIT_CMD_MODE_DISPATCH = 0x0A,
    UNIT_CMD_JUMP_EFFECT = 0x0B,    /* unitCmdEva: issues UNIT_CMD_MOVE_CHA_JUMP and effect 0xB */
    UNIT_CMD_HOVER_EFFECT = 0x0C,   /* unitCmdDef: issues UNIT_CMD_MOVE_CHA_HOVER and effect 0xC */
    UNIT_CMD_CODE_WAIT = 0x0E,      /* unitCmdMot */
    UNIT_CMD_MOVE_CHA = 0x10,       /* unitCmdMoveCha */
    UNIT_CMD_MOVE_POS = 0x11,       /* unitCmdMovePos */
    UNIT_CMD_PHASE_0E_WAIT = 0x12,  /* unitCmdPhase0EWait */
    UNIT_CMD_POSE_ARRIVE = 0x14,    /* unitCmdPoseArrive */
    UNIT_CMD_POSE_ARRIVE_WAIT = 0x15, /* unitCmdPoseArriveWait */
    UNIT_CMD_MOVE_CHA_RUN = 0x16,
    UNIT_CMD_MOVE_POS_JUMP = 0x17,
    UNIT_CMD_MOVE_CHA_JUMP = 0x18,
    UNIT_CMD_MOVE_CHA_HOVER = 0x19,
    UNIT_CMD_MOVE_POS_HOVER = 0x1A,
    UNIT_CMD_MODE_RAMP = 0x1B,
    UNIT_CMD_MOVE_RATIO = 0x1C,
    UNIT_CMD_MOVE_RAMP = 0x1D,      /* unitCmdMoveRamp */
    UNIT_CMD_FINISH = 0x1E,         /* unitCmdFinish */
    UNIT_CMD_PHASE_2C_WAIT = 0x1F,  /* unitCmdBted */
    UNIT_CMD_CLEAR_ACTIVE = 0x20,
    UNIT_CMD_PHASE_2B_WAIT = 0x21,
    UNIT_CMD_DEACTIVATE = 0x22,     /* unitCmdStat: clears UNIT_FLAG_ACTIVE */
    UNIT_CMD_TURN_AIM = 0x23,       /* unitCmdTurn */
    UNIT_CMD_STREAM_PLAY = 0x24,    /* unitCmdStreamPlay */
    UNIT_CMD_FINISH_REFRESH = 0x26, /* unitCmdFinishRefresh */
    UNIT_CMD_MODE_START = 0x27
};

typedef struct BattleUnit BattleUnit;

struct FadeTask;

typedef struct StatBlock StatBlock;

/* Unit keys (StatBlock.key): 1..10 party members, 11..15 the large units (DATA_UNIT_HEAP_LARGE from data_file_load.c),
 * 16..140 the other data ids; keys from UNIT_KEY_COUNT up are gattai codes. A range test subtracts the first key
 * and compares the unsigned difference with the count. */
#define UNIT_KEY_PARTY_FIRST 1
#define UNIT_KEY_PARTY_COUNT 10
#define UNIT_KEY_LARGE_FIRST 11
#define UNIT_KEY_LARGE_COUNT 5
#define UNIT_KEY_OTHER_FIRST 16
#define UNIT_KEY_COUNT 141

/*
 * The 0x108-byte battle block of a unit (its BattleUnit points at it through +0x144). The same
 * record fills the entry pools battleEntryPoolA and battleEntryPoolB and the slot table
 * battleSlotTable: calcPara2OrgSub copies hp and ep of a unit into its slot, calcCopyParaMake
 * copies a whole slot into a unit. Only the members the functions of this file touch are named.
 */
struct StatBlock {
    unsigned char unmodeled_00[0x2];        /* +0x00..+0x01 */
    UnalignedS32 maxHp;                     /* +0x02: the HP cap, read unaligned (calcHp clamps HP to it; battleEntryLoad starts HP at it) */
    short maxEp;                            /* +0x06: EP cap read by calcEp */
    short physAttack;                       /* +0x08: physical attack, calcAtkPhy */
    short physDefence;                      /* +0x0a: physical defence, calcDefPhy */
    short magAttack;                        /* +0x0c: magic attack, calcAtkMag */
    short magDefence;                       /* +0x0e: magic defence, calcDefMag */
    signed char hitRate;                    /* +0x10: base of calcHitParaGet */
    signed char evadeRate;                  /* +0x11: base of calcEvadeRate */
    union {
        unsigned char counter12;            /* bumped by calcSkillInit, capped at 15 */
        signed char valueBias;              /* the same byte as the bias calcWct subtracts from valueLevel */
    };                                      /* +0x12 */
    signed char expLevel;                   /* +0x13: the unit experience table entry (dataPlUnitInit); calcLvUp steps it up, unitGrowthStep divides by 100 - expLevel */
    signed char state;                      /* +0x14: at 4 or more the pose turn is damped */
    signed char subState;                   /* +0x15: the move kind the move dispatchers switch on; compared with 4 by unitSetPhase for phase 5 */
    signed char chanceLevel;                /* +0x16: the percent actLevelChanceRoll rolls, capped at 100 */
    signed char inflictBase;                /* +0x17: chance base read by calcStatInflictRate; calcCopyParaMake fills it */
    signed char defaultCode;                /* +0x18: unitStateFlagGet's fallback code */
    unsigned char unmodeled_19[0x1A - 0x19]; /* +0x19 */
    short slidePercent;                     /* +0x1A: signed percent unitDefArea divides by 100 */
    unsigned short flags;                   /* +0x1C: the command handlers read the sequence bits 0x80 and 0x2000; low two bits pick the modeGuard byte, bits 0x10, 0x20, 0x40, 0x800 and 0x4000 are tested */
    short sequenceIndex;                    /* +0x1E: the low half of the effect key */
    signed char liveRows[4];                /* +0x20: nonzero for each live row calcHitRowGate walks */
    signed char slotValue[8];               /* +0x24: per status bit, read by calcDmg's status loop */
    signed char slotRoll[8];                /* +0x2C: percent rolled for each status bit by calcDmg */
    signed char effectCode[4];              /* +0x34: four effect codes, zero when empty (calcSkillEquipOrg writes them) */
    unsigned char unmodeled_38[0x52 - 0x38]; /* +0x38..+0x51 */
    unsigned short altMask[5];              /* +0x52: calcStatProtectSet/Reset use indices 3 and 4 */
    int hp;                                 /* +0x5C: HP (calcHp); calcPara2OrgSub copies it into the slot */
    short ep;                               /* +0x60: EP read by calcEp; calcPara2OrgSub copies it into the slot */
    union {
        short key;                          /* the unit's key: below 16 a party slot (calcTrg2PE's side split), 16 to 140 pool B; 0 marks a free pool entry */
        unsigned short keyUnsigned;         /* the same halfword as the unsigned value the data panels range-test (battle_init.c) */
    };                                      /* +0x62 */
    short valueLevel;                       /* +0x64: the level calcWct scales */
    short unitValue;                        /* +0x66: calcWct's value, lowered by calcWctDec */
    short unitValueSaved;                   /* +0x68: the value unitValueReset saves */
    short posIndex;                         /* +0x6A: the effectPosGet row of the unit */
    float facing;                           /* +0x6C: the unit's facing angle, read by unitIsInFacingArc */
    signed char state70;                    /* +0x70: a signed byte, nonzero while calcHitRowGate is in its battle state */
    signed char level;                      /* +0x71: 0..3 (unitLevelAdd), compared with a record's required level */
    unsigned char effectFlag72;             /* +0x72: set to 4 by calcSkillInit */
    union {
        unsigned char dirty;                /* cleared by calcTurnEnd */
        signed char keysSpent;              /* the same byte as the counter of keys the data panels spend (battle_init.c) */
    };                                      /* +0x73 */
    union {
        short pairFirst[3];                 /* +0x74: first halfword of each pair (ShortPair.first), cleared by battleEntryActivate */
        unsigned short pairFirstUnsigned[3]; /* the same halfwords as the unsigned kinds unitBindActors range-tests */
    };
    unsigned short pairSecond[3];           /* +0x7A: the second halfword of each pair dataPlUnitInit loads next to pairFirst */
    short rollPercent;                      /* +0x80: percent 0..100 (unitPercentAdd), rolled by calcStatusHitRoll */
    short current;                          /* +0x82: current value, capped at currentMax (unitCurrentAdd) */
    short currentMax;                       /* +0x84 */
    short equipId[4];                       /* +0x86: four equipment ids, zero when empty (calcAccEquipOrg writes them) */
    short linkedSlots[2];                   /* +0x8E: slots 11 to 15 only (battleSlotLinkedSlotSet); [0] is the slot calcCopyParaMake reads, [1] the sub pilot id (0..0xA, configSubPilot) */
    unsigned short activeMask[4];           /* +0x92: one value per row; set bits are active statuses */
    unsigned char turnCount[4][16];         /* +0x9a: per row and status bit, turns left */
    signed char entryKind[8];               /* +0xDA: unitEntryKindAdd's entry table, ANDed with liveRows by calcHitRowGate */
    signed char entryMark[8];               /* +0xE2: written by calcHitRowGate, read by unitEntriesAllMarked */
    int entryCount;                         /* +0xEC: entries held, at most 8 */
    int comboMask;                          /* +0xF0: status mask of the current combo (calcComboUpdate) */
    int comboHits;                          /* +0xF4: hit count of the current combo, capped at 99 */
    int spawnArgC;                          /* +0xF8: the spawn record's spawnArgC (scene_load.c), unitCreate arg 4 */
    int spawnArgD;                          /* +0xFC: the spawn record's spawnArgD (scene_load.c), unitCreate arg 5 */
    BattleUnit *source;                      /* +0x100: the unit whose effect sum monsGainSet adds; unitFaceTarget's target argument */
    short sourceAction;                     /* +0x104: the action index of source (unitJobTypeAtk) */
    short handle;                           /* +0x106: passed to handleFree by unitCmdRelease */
};

/* The command queue item every handler of unitCommandTable receives (the queue copies its first 0x18 bytes; callers build it as a 0x20-byte local). */
typedef struct Ov01Command {
    int command; /* +0x0: the table index unitRunCommands dispatches on; the move dispatchers store the command they run */
    int state; /* +0x4: zero until the one-shot start has run */
    int code; /* +0x8: the key, id or motion code the command carries */
    union {
        int wait; /* the wait flag of the mode commands */
        BattleUnit *owner; /* compared with the object by unitCmdPlayVoice */
    } arg; /* +0xC */
    BattleUnit *faceTarget; /* +0x10 */
    int poseIndex; /* +0x14: the index unitPoseRamp takes (also the goal-key flag unit_cmd.c reads) */
    unsigned char unmodeled_18[0x8];
} Ov01Command;

/* BattleUnit.slotFlags bit set while the object's slot is in use (ObjSlot.flags in obj_slot.c). */
#define OV01_OBJECT_IN_USE 0x1

/* BattleUnit.flags bits: a unit that is down (the draws and the target pickers leave it out) and a unit in the
 * action stock. */
#define UNIT_FLAG_HIDDEN 0x2      /* set by unitSetBindFlags with ACTOR_STATE_HIDDEN on the unit's actors */
#define UNIT_FLAG_DOWN 0x4
#define UNIT_FLAG_STOCKED 0x8
/* One of the enemy-side bits (unitEnCreate sets 0x60): the unit reports its hits and poses to the script
 * (unitScriptPairEventSend). */
#define UNIT_FLAG_SCRIPT_EVENTS 0x20
/* Set by attackHitsShow per attack: the unit's reaction to the hit is queued, and its hit panels are shown;
 * unitHitFlagsClear clears both. */
#define UNIT_FLAG_HIT_REACTED 0x100
#define UNIT_FLAG_HIT_PANELS 0x40000
/* Set by cmdExecute when it sends the unit to its target (UNIT_CMD_MOVE_CHA) or poses it in place
 * (UNIT_CMD_PHASE_0E_WAIT / UNIT_CMD_POSE_ARRIVE); the think end brings it back and clears UNIT_FLAGS_ACTION. */
#define UNIT_FLAG_MOVED_OUT 0x80
#define UNIT_FLAG_POSED_OUT 0x400
/* The flags the think end clears: UNIT_FLAG_MOVED_OUT, UNIT_FLAG_POSED_OUT, 0x800 and the phase flag 0x2000
 * (unitCmdSetPhase). */
#define UNIT_FLAGS_ACTION 0x2C80

struct BattleUnit {
    union {
        unsigned char unmodeled_00[4];  /* unit_tbl.c reads +0x8 as a doubleword through this name */
        u32 slotFlags;                  /* the slot word (OV01_OBJECT_IN_USE); menuStockObjDraw draws only units in use */
    };                                  /* +0x0 */
    void (*update)(BattleUnit *object); /* +0x4: unitUpdate (unitCreateSide) */
    void (*draw)(BattleUnit *object); /* +0x8: unitDrawNone */
    u32 flags; /* +0xC: unitEnCreate ORs in 0x60 for an enemy-side unit, unitCreateSide clears it */
    struct DataLookup commandQueue; /* +0x10: the command queue unitRunCommands walks; its flag word at +0x138 is the wait counter */
    int commandTarget; /* +0x13C: the command's target unit, kept as a word like CmdListEntry.unit; an BattleUnit * here (or a union view) moves cmdExecute's store and the kind-1 event handler's schedule, so every reader casts the word */
    int commandCode; /* +0x140: the command's code */
    StatBlock *stats; /* +0x144: the battle block (the unit command handlers' motion record is the same block) */
    unsigned char unmodeled_148[0x8];
    SlideBlock pose; /* +0x150..+0x1D0: the unit pose, a slide block (obj.h) */
    struct XenoActor *unit; /* +0x1D0: the object's unit actor */
    unsigned char unmodeled_1d4[0x8];
    struct XenoActor *children[UNIT_CHILD_COUNT]; /* +0x1DC: the three child units */
    struct FadeTask *fade; /* +0x1E8: the running fade task, 0 when none runs; a fade keeps unitCmdRelease from releasing the unit */
};

/* Defined in unit_fade.c: the fade tasks of the unit; unitFadeOut is the sibling with the same unit argument. */
struct FadeTask *unitFadeIn(BattleUnit *object);
struct FadeTask *unitFadeTo(BattleUnit *object, int frames, int targetPercent);
struct FadeTask *unitFadeOut(BattleUnit *object);

/* Unit creation and removal. */
BattleUnit *unitPlCreate(int id);
BattleUnit *unitEnCreate(int id);
void unitRemove(BattleUnit *unit);

/* Unit state and command prototypes. */
void unitUpdate(BattleUnit *object);
void unitUnbind(BattleUnit *object);
int unitCurrentPhase(BattleUnit *object);
void unitSetPhase(BattleUnit *object, int phase);
void unitSetTimedPhase(BattleUnit *object, int phase);
void unitBindSetup(BattleUnit *object);
void unitClearSnapshot(BattleUnit *object);
void unitSetSelector(BattleUnit *object, int selector);
void unitRefreshPhase(BattleUnit *object);
void unitSetVectorZ(BattleUnit *object, float scaleZ);
int unitIsDone(BattleUnit *object);
int unitEffectKeyFree(BattleUnit *unit, int keyHigh, int keyLow);
void unitLoadSnapshot(BattleUnit *object, int id);
void unitSetAngleDegrees(BattleUnit *object, int degrees);
void unitSetAngleRadians(BattleUnit *object, f32 angle);
int unitGetAngleDegrees(BattleUnit *object);
void unitFaceTarget(BattleUnit *object, int facesTarget, BattleUnit *target);
void unitMoveKey(BattleUnit *object, int *keyHigh, int *keyLow);
void unitCmdSet(BattleUnit *object, Ov01Command *command);
void unitCmdClear(BattleUnit *object);
int unitQueueIsEmpty(BattleUnit *object);

/* The unit pose, goal and move commands that other files call. */

void objPosePublish(BattleUnit *object);
void unitCmdVASet(BattleUnit *object, int mode, int unusedFlag);
int unitCmdMoveCalc(BattleUnit *object);
int objPoseHeadingUpdate(BattleUnit *object, int faceAway);
void unitCmdMoveNext(BattleUnit *object, int action);
float unitPoseRamp(int index, int reverse);
int unitDeadSpecChk(BattleUnit *object);
void unitCmdDstSet(BattleUnit *object, int cmdId, int mode);
void unitSlotsClear(BattleUnit *object, int rowKey);
void unitHitFlagsClear(BattleUnit *object);
int unitCmdRun(BattleUnit *object, int cmdId, int flip);
/* The third argument is unused: unitCmdMoveChaJump and unitCmdMovePosJump pass the move flag as the sibling run and hover calls do. */
int unitCmdJump(BattleUnit *object, int phase, int unusedFlag);
int unitCmdHover(BattleUnit *object, int cmdId, int flip);
float unitDefArea(BattleUnit *object);
int unitCmdDeadSet(BattleUnit *object);

/* Entries 34 to 38 of unitCommandTable. */
int unitCmdStat(BattleUnit *object, Ov01Command *unusedStep);
int unitCmdTurn(BattleUnit *object, Ov01Command *step);
int unitCmdStreamPlay(BattleUnit *object, Ov01Command *unusedStep);
int unitCmdRemove(BattleUnit *object, Ov01Command *unusedStep);
int unitCmdFinishRefresh(BattleUnit *object, Ov01Command *unusedStep);

#endif /* INCLUDE_OV01_UNIT_CMD_H */
