#ifndef INCLUDE_OV01_TU012_H
#define INCLUDE_OV01_TU012_H

#include "shared.h"
#include "ov01/tu005.h"
#include "ov01/tu042.h"

/* The unit object of OV01 (tu012-tu016 and the callers that reach it): its types and unit command prototypes. */

/* A unit has three child units (and three slot flags, one per child); the command table has 40 handlers. */
#define UNIT_CHILD_COUNT 3
#define UNIT_COMMAND_COUNT 40

/* Command-table indices (unitCommandTable in src/ov01/tu012.c) that the unit move and mode handlers store as step status. */
enum {
    UNIT_CMD_MODE_DISPATCH = 0x0A,
    UNIT_CMD_JUMP_EFFECT = 0x0B,    /* func_00A85EA0: issues UNIT_CMD_MOVE_CHA_JUMP and effect 0xB */
    UNIT_CMD_HOVER_EFFECT = 0x0C,   /* func_00A85F30: issues UNIT_CMD_MOVE_CHA_HOVER and effect 0xC */
    UNIT_CMD_CODE_WAIT = 0x0E,      /* unitCmdCodeWait */
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
    UNIT_CMD_PHASE_2C_WAIT = 0x1F,  /* unitCmdPhase2CWait */
    UNIT_CMD_CLEAR_ACTIVE = 0x20,
    UNIT_CMD_PHASE_2B_WAIT = 0x21,
    UNIT_CMD_DEACTIVATE = 0x22,     /* func_00A86CB0: clears UNIT_FLAG_ACTIVE */
    UNIT_CMD_TURN_AIM = 0x23,       /* unitCmdTurnAim */
    UNIT_CMD_STREAM_PLAY = 0x24,    /* unitCmdStreamPlay */
    UNIT_CMD_FINISH_REFRESH = 0x26, /* unitCmdFinishRefresh */
    UNIT_CMD_MODE_START = 0x27
};

typedef struct Ov01Object Ov01Object;

struct FadeTask;
typedef struct Ov01Object StatOwner;

typedef struct StatBlock StatBlock;

/* The pose block at +0x150 of the object. */
typedef struct Ov01Pose {
    DoublewordVector4 snapshot; /* +0x0: the start of the blend and the step; unitUpdate copies the actor's snapshot here, tu016 moves it as two words */
    unsigned char unmodeled_10[0x14];
    f32 angle; /* +0x24: radians, see unitSetAngleRadians */
    unsigned char unmodeled_28[0x18];
    f32 stepX; /* +0x40 */
    f32 stepY;
    f32 stepZ;
    unsigned char unmodeled_4C[0x4];
    f32 easeX; /* +0x50: zero */
    f32 easeY; /* +0x54: -0.0204375 in modes 3 and 4, else zero */
    f32 easeZ; /* +0x58: zero */
    unsigned char unmodeled_5C[0x4];
    Vector4 point; /* +0x60: the pose goal (unitCmdDstSet), and the destination of xglVectorMulMat in unitFaceTarget */
    f32 heading; /* +0x70: the heading toward the goal, [0, 2pi) */
    f32 headingDelta; /* +0x74: the signed turn to the heading, in (-pi, pi] */
} Ov01Pose;

/* Unit keys (StatBlock.key): 1..10 party members, 11..15 the large units (tu008 gives them DATA_UNIT_HEAP_LARGE),
 * 16..140 the other data ids; keys from UNIT_KEY_COUNT up are gattai codes. A range test subtracts the first key
 * and compares the unsigned difference with the count. */
#define UNIT_KEY_PARTY_FIRST 1
#define UNIT_KEY_PARTY_COUNT 10
#define UNIT_KEY_LARGE_FIRST 11
#define UNIT_KEY_LARGE_COUNT 5
#define UNIT_KEY_OTHER_FIRST 16
#define UNIT_KEY_COUNT 141

/*
 * The 0x108-byte battle block of a unit (its StatOwner points at it through +0x144). The same
 * record fills the entry pools battleEntryPoolA and battleEntryPoolB and the slot table
 * battleSlotTable: battleSlotCommit copies hp and ep of a unit into its slot, battleSlotExport
 * copies a whole slot into a unit. Only the members the functions of this file touch are named.
 */
struct StatBlock {
    unsigned char unmodeled_00[0x2];        /* +0x00..+0x01 */
    UnalignedS32 maxHp;                     /* +0x02: the HP cap, read unaligned (calcHpAdjust clamps HP to it; battleEntryLoad starts HP at it) */
    short maxEp;                            /* +0x06: EP cap read by calcEpAdjust */
    short physAttack;                       /* +0x08: physical attack, calcAtkPhy */
    short physDefence;                      /* +0x0a: physical defence, calcDefPhy */
    short magAttack;                        /* +0x0c: magic attack, calcAtkMag */
    short magDefence;                       /* +0x0e: magic defence, calcDefMag */
    signed char hitRate;                    /* +0x10: base of calcHitRateGet */
    signed char evadeRate;                  /* +0x11: base of calcEvadeRate */
    union {
        unsigned char counter12;            /* bumped by unitStatsRefresh, capped at 15 */
        signed char valueBias;              /* the same byte as the bias unitValueCalc subtracts from valueLevel */
    };                                      /* +0x12 */
    signed char expLevel;                   /* +0x13: the unit experience table entry (battleSlotsInit); unitExpGain steps it up, unitGrowthStep divides by 100 - expLevel */
    signed char state;                      /* +0x14: at 4 or more the pose turn is damped */
    signed char subState;                   /* +0x15: the move kind tu014's dispatchers switch on; compared with 4 by unitSetPhase for phase 5 */
    signed char chanceLevel;                /* +0x16: the percent actLevelChanceRoll rolls, capped at 100 */
    signed char inflictBase;                /* +0x17: chance base read by calcStatInflictRate; battleSlotExport fills it */
    signed char defaultCode;                /* +0x18: unitStateFlagGet's fallback code */
    unsigned char unmodeled_19[0x1A - 0x19]; /* +0x19 */
    short slidePercent;                     /* +0x1A: signed percent unitDefArea divides by 100 */
    unsigned short flags;                   /* +0x1C: tu013-tu015 read the sequence bits 0x80 and 0x2000; low two bits pick the modeGuard byte, bits 0x10, 0x20, 0x40, 0x800 and 0x4000 are tested */
    short sequenceIndex;                    /* +0x1E: the low half of the effect key */
    signed char liveRows[4];                /* +0x20: nonzero for each live row calcHitRowGate walks */
    signed char slotValue[8];               /* +0x24: per status bit, read by calcAttackDamage's status loop */
    signed char slotRoll[8];                /* +0x2C: percent rolled for each status bit by calcAttackDamage */
    signed char effectCode[4];              /* +0x34: four effect codes, zero when empty (battleSlotSetByte writes them) */
    unsigned char unmodeled_38[0x52 - 0x38]; /* +0x38..+0x51 */
    unsigned short altMask[5];              /* +0x52: calcStatAltSet/Reset use indices 3 and 4 */
    int hp;                                 /* +0x5C: HP (calcHpAdjust); battleSlotCommit copies it into the slot */
    short ep;                               /* +0x60: EP read by calcEpAdjust; battleSlotCommit copies it into the slot */
    union {
        short key;                          /* the unit's key: below 16 a party slot (tgtSideOf's side split), 16 to 140 pool B; 0 marks a free pool entry */
        unsigned short keyUnsigned;         /* the same halfword as the unsigned value the data panels range-test (ov01 tu022) */
    };                                      /* +0x62 */
    short valueLevel;                       /* +0x64: the level unitValueCalc scales */
    short unitValue;                        /* +0x66: unitValueCalc's value, lowered by unitValueDecay */
    short unitValueSaved;                   /* +0x68: the value unitValueReset saves */
    short posIndex;                         /* +0x6A: the effectPosGet row of the unit */
    float facing;                           /* +0x6C: the unit's facing angle, read by unitIsInFacingArc */
    signed char state70;                    /* +0x70: a signed byte, nonzero while calcHitRowGate is in its battle state */
    signed char level;                      /* +0x71: 0..3 (unitLevelAdd), compared with a record's required level */
    unsigned char effectFlag72;             /* +0x72: set to 4 by unitStatsRefresh */
    union {
        unsigned char dirty;                /* cleared by unitValueRefresh */
        signed char keysSpent;              /* the same byte as the counter of keys the data panels spend (ov01 tu022) */
    };                                      /* +0x73 */
    union {
        short pairFirst[3];                 /* +0x74: first halfword of each pair (ShortPair.first), cleared by battleEntryActivate */
        unsigned short pairFirstUnsigned[3]; /* the same halfwords as the unsigned kinds unitBindActors range-tests (ov01 tu011) */
    };
    unsigned short pairSecond[3];           /* +0x7A: the second halfword of each pair battleSlotsInit loads next to pairFirst */
    short rollPercent;                      /* +0x80: percent 0..100 (unitPercentAdd), rolled by calcStatusHitRoll */
    short current;                          /* +0x82: current value, capped at currentMax (unitCurrentAdd) */
    short currentMax;                       /* +0x84 */
    short equipId[4];                       /* +0x86: four equipment ids, zero when empty (battleSlotSetShort writes them) */
    short linkedSlots[2];                   /* +0x8E: slots 11 to 15 only (battleSlotSetShortLate); [0] is the slot battleSlotExport reads, [1] the sub pilot id (0..0xA, configSubPilot) */
    unsigned short activeMask[4];           /* +0x92: one value per row; set bits are active statuses */
    unsigned char turnCount[4][16];         /* +0x9a: per row and status bit, turns left */
    signed char entryKind[8];               /* +0xDA: unitEntryKindAdd's entry table, ANDed with liveRows by calcHitRowGate */
    signed char entryMark[8];               /* +0xE2: written by calcHitRowGate, read by unitEntriesAllMarked */
    int entryCount;                         /* +0xEC: entries held, at most 8 */
    int comboMask;                          /* +0xF0: status mask of the current combo (calcComboUpdate) */
    int comboHits;                          /* +0xF4: hit count of the current combo, capped at 99 */
    int spawnArgC;                          /* +0xF8: the spawn record's spawnArgC (tu038), unitCreate's argument 4 */
    int spawnArgD;                          /* +0xFC: the spawn record's spawnArgD (tu038), unitCreate's argument 5 */
    StatOwner *source;                      /* +0x100: the unit whose effect sum battleResultAdd adds; unitFaceTarget's target argument */
    short sourceAction;                     /* +0x104: the action index of source (attackQueue) */
    short handle;                           /* +0x106: passed to handleFree by unitCmdRelease */
};


/* The command queue item every handler of unitCommandTable receives (the queue copies its first 0x18 bytes; callers build it as a 0x20-byte local). */
typedef struct Ov01Command {
    int command; /* +0x0: the table index unitRunCommands dispatches on; the move dispatchers store the command they run */
    int state; /* +0x4: zero until the one-shot start has run */
    int code; /* +0x8: the key, id or motion code the command carries */
    union {
        int wait; /* the wait flag of the mode commands */
        Ov01Object *owner; /* compared with the object by unitCmdPlayVoice */
    } arg; /* +0xC */
    Ov01Object *faceTarget; /* +0x10 */
    int poseIndex; /* +0x14: the index unitPoseRamp takes (also the goal-key flag tu016 reads) */
    unsigned char unmodeled_18[0x8];
} Ov01Command;

/* Ov01Object.slotFlags bit set while the object's slot is in use (ObjSlot.flags in tu005). */
#define OV01_OBJECT_IN_USE 0x1

/* Ov01Object.flags bits: a unit that is down (the draws and the target pickers leave it out) and a unit in the
 * action stock. */
#define UNIT_FLAG_HIDDEN 0x2      /* set by unitSetBindFlags with ACTOR_STATE_HIDDEN on the unit's actors */
#define UNIT_FLAG_DOWN 0x4
#define UNIT_FLAG_STOCKED 0x8
/* One of the enemy-side bits (unitEnCreate sets 0x60): the unit reports its hits and poses to the script
 * (unitScriptPairEventSend). */
#define UNIT_FLAG_SCRIPT_EVENTS 0x20
/* Set by attackHitsShow per attack: the unit's reaction to the hit is queued, and its hit panels are shown;
 * func_00A86F20 clears both. */
#define UNIT_FLAG_HIT_REACTED 0x100
#define UNIT_FLAG_HIT_PANELS 0x40000
/* Set by cmdExecute when it sends the unit to its target (UNIT_CMD_MOVE_CHA) or poses it in place
 * (UNIT_CMD_PHASE_0E_WAIT / UNIT_CMD_POSE_ARRIVE); the think end brings it back and clears UNIT_FLAGS_ACTION. */
#define UNIT_FLAG_MOVED_OUT 0x80
#define UNIT_FLAG_POSED_OUT 0x400
/* The flags the think end clears: UNIT_FLAG_MOVED_OUT, UNIT_FLAG_POSED_OUT, 0x800 and the phase flag 0x2000
 * (unitCmdSetPhase). */
#define UNIT_FLAGS_ACTION 0x2C80

struct Ov01Object {
    union {
        unsigned char unmodeled_00[4];  /* tu017 reads +0x8 as a doubleword through this name */
        u32 slotFlags;                  /* the slot word (OV01_OBJECT_IN_USE); objPathDraw draws only units in use */
    };                                  /* +0x0 */
    void (*update)(Ov01Object *object); /* +0x4: unitUpdate (unitCreateSide) */
    void (*draw)(Ov01Object *object); /* +0x8: func_00A84880 */
    u32 flags; /* +0xC: unitEnCreate ORs in 0x60 for an enemy-side unit, unitCreateSide clears it */
    struct DataLookup commandQueue; /* +0x10: the command queue unitRunCommands walks; its flag word at +0x138 is the wait counter */
    int commandTarget; /* +0x13C: the command's target unit, kept as a word like CmdListEntry.unit */
    int commandCode; /* +0x140: the command's code */
    StatBlock *stats; /* +0x144: the battle block (the unit command handlers' motion record is the same block) */
    unsigned char unmodeled_148[0x8];
    Ov01Pose pose; /* +0x150 */
    unsigned char unmodeled_1c8[0x8];
    struct XenoActor *unit; /* +0x1D0: the object's unit actor */
    unsigned char unmodeled_1d4[0x8];
    struct XenoActor *children[UNIT_CHILD_COUNT]; /* +0x1DC: the three child units */
    struct FadeTask *fade; /* +0x1E8: the running fade task, 0 when none runs; a fade keeps unitCmdRelease from releasing the unit */
};

/* Defined in tu018.c: the fade tasks of the unit; unitFadeOut is the sibling with the same unit argument. */
struct FadeTask *unitFadeIn(Ov01Object *object);
struct FadeTask *unitFadeTo(Ov01Object *object, int frames, int targetPercent);
struct FadeTask *unitFadeOut(Ov01Object *object);

/* Unit state and command prototypes. */
void unitUpdate(Ov01Object *object);
void unitUnbind(Ov01Object *object);
int unitCurrentPhase(Ov01Object *object);
void unitSetPhase(Ov01Object *object, int phase);
void unitSetTimedPhase(Ov01Object *object, int phase);
void unitBindSetup(Ov01Object *object);
void unitClearSnapshot(Ov01Object *object);
void unitSetSelector(Ov01Object *object, int selector);
void unitRefreshPhase(Ov01Object *object);
void unitSetVectorZ(Ov01Object *object, float scaleZ);
int unitIsDone(Ov01Object *object);
int unitEffectKeyFree(Ov01Object *unit, int keyHigh, int keyLow);
void unitLoadSnapshot(Ov01Object *object, int id);
void unitSetAngleDegrees(Ov01Object *object, int degrees);
void unitSetAngleRadians(Ov01Object *object, f32 angle);
void unitFaceTarget(Ov01Object *object, int facesTarget, Ov01Object *target);
void unitMoveKey(Ov01Object *object, int *keyHigh, int *keyLow);
void unitQueueAppend(Ov01Object *object, Ov01Command *command);
void unitQueueClear(Ov01Object *object);
int unitQueueIsEmpty(Ov01Object *object);

#endif /* INCLUDE_OV01_TU012_H */
