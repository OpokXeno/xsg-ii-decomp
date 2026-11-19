#ifndef INCLUDE_OV01_TU042_H
#define INCLUDE_OV01_TU042_H

/* Functions of src/ov01/tu042.c that other files call. */

/* The message task record msgPutObject returns (defined in tu042.c). */
struct MsgObject;

/* Unit record of bat.dat for a unit index, and the unit's name (tu042.c has the numbering). */
void *batUnitRecordGet(int index);
char *batUnitNameGet(int index);

/* Shows text at (x, y) in a width x height box for frames frames (0: until its voice is done). */
struct MsgObject *msgPutObject(int x, int y, char *text, int frames, int width, int height);

/* Effect request block sent to the main-side effect queue (sefStartEffectDefault /
 * sefStartEffectWithValue); the senders clear it first. */
struct EffectRequest {
    struct XenoActor *head;            /* +0x00: the actor of the owner unit (Ov01Object.unit) */
    struct XenoActor *partner;         /* +0x04: the other unit's actor of a pair technique (attackEffectsStart) */
    struct XenoActor *target;          /* +0x08: the same actor again (tu033 senders); the hit target's actor */
    struct XenoActor *parts[4];        /* +0x0C: the child actors of the unit (two each for a pair technique) */
    int key;                           /* +0x1C: (high << 12) | low, see createEffect */
    unsigned char unmodeled_20[0x24 - 0x20];
    short hitMode;                     /* +0x24: the hit record's hitMode (attackEffectsStart) */
    unsigned short hitCode;            /* +0x26: the actionBits byte and the target's row bits (attackEffectsStart) */
    unsigned char unmodeled_28[0x2C - 0x28];
    short mode;                        /* +0x2C: 2 or 3 (unitEffectStartMode); the attacker's state
                                        * (attackEffectsStart) */
    short targetState;                 /* +0x2E: the target's state (attackEffectsStart) */
    unsigned char unmodeled_30[0x38 - 0x30];
};

/* EffectRequest.key (and the keys sefApplyRecordMode and sefLoadEffect take): the effect group above this shift, the
 * record of the group below it. */
#define EFFECT_KEY_GROUP_SHIFT 12

/* Unaligned signed word: the record's word sits at a 2-byte offset. */
typedef signed int UnalignedS32 __attribute__((aligned(1)));

/* One 4-byte entry of the unit experience table unitExpTblGet returns. */
typedef struct UnitExpEntry UnitExpEntry;

struct UnitExpEntry {
    UnalignedS32 exp;
};

/* One pair of halfwords of a row slotPairRowGet returns. */
typedef struct ShortPair ShortPair;

struct ShortPair {
    unsigned short first;
    unsigned short second;
};

/*
 * The 0x20-byte growth record growthRecordGet returns for a party slot key: the start values
 * battleSlotsInit loads into the slot record's growth values, and the targets unitGrowthStep
 * steps them toward.
 */
typedef struct GrowthRecord GrowthRecord;

struct GrowthRecord {
    unsigned short start[8];                /* +0x00: the growth values a slot starts with */
    unsigned short target[8];               /* +0x10: the values the growth values of the slot record move toward */
};

/* Partial view of a 16-byte table key record tableKeyRecordGet returns: points, rates, chances and the two (category, id) drop pairs. */
struct TableKeyRecord {
    int points;                    /* +0x00 */
    short stepRate;                /* +0x04 */
    short flat;                    /* +0x06 */
    unsigned char secondChance;    /* +0x08 */
    unsigned char firstChance;     /* +0x09 */
    unsigned char secondCategory;  /* +0x0A: the data box category of the second pair, zero when it drops nothing */
    unsigned char firstCategory;   /* +0x0B: the data box category of the first pair */
    unsigned char secondId;        /* +0x0C: the data box id of the second pair */
    unsigned char firstId;         /* +0x0D: the data box id of the first pair */
};

/* The script attack table the natives atkTbl and atkTblGet (tu046) take from the image's globals: a row count,
 * then rows of two words. */
typedef struct AtkTblRow {
    int key;   /* +0x00: atkTblGet field 1 */
    int value; /* +0x04: atkTblGet field 2 */
} AtkTblRow;

typedef struct AtkTbl {
    int count;          /* +0x00: atkTblGet field 0 (any other field) */
    AtkTblRow rows[1];  /* +0x04: extent given by count */
} AtkTbl;

struct Ov01Object;

/* The 16-byte record unitScriptEventSend takes: the unit and three words it passes on, with the selector, as a script
 * message (sbThreadEnqueue). */
typedef struct UnitScriptEvent {
    struct Ov01Object *unit;
    int words[3];
} UnitScriptEvent;

void unitScriptEventSend(int kind, UnitScriptEvent *event);

/* The 16-byte record unitScriptPairEventSend takes: the unit and, for UNIT_SCRIPT_KIND_PAIR, a second unit and two
 * words that it passes on, with the kind, as a script message (sbThreadEnqueue). */
typedef struct UnitScriptPairEvent {
    struct Ov01Object *unit;
    struct Ov01Object *other;
    int words[2];
} UnitScriptPairEvent;

/* The unitScriptPairEventSend kind whose message also carries the second unit's index and the two words. */
#define UNIT_SCRIPT_KIND_PAIR 2

void unitScriptPairEventSend(int kind, UnitScriptPairEvent *event);

int atkTblFieldGet(int field, AtkTbl *table, int index);
int atkTblBuild(struct Ov01Object *unit, AtkTbl *table, int rows);
void thinkSendEnd(struct Ov01Object *unit);
void thinkDispatch(struct Ov01Object *unit, int command, int arg, struct Ov01Object *target);

/* The records of the bat.dat tables (the getters below, numbered from 1). */
/* The first two halfwords of a bat.dat record that has texts: the offsets of its RECORD_TEXT_NAME and
 * RECORD_TEXT_INFO texts from the start of its table (the *TextGet getters). */
typedef struct RecordTexts {
    short text[2];
} RecordTexts;

/* The 8-byte records func_00A9B5B8 returns. */
typedef struct TextRecord8 {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x4];
} TextRecord8;

/* The entry battleLookupGet returns. */
typedef struct BattleLookupEntry BattleLookupEntry;

struct BattleLookupEntry {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[2];
    unsigned short flags;                   /* +0x06 */
    short tecIndex;                         /* +0x08: the index tecRecordGet takes */
    unsigned char firstCode;                /* +0x0A */
    unsigned char secondCode;               /* +0x0B */
};

typedef struct TecRecord TecRecord;

/* The 0x20-byte technique record tecRecordGet returns for an action index. */
struct TecRecord {
    /* +0x00: target flags (1, 2, 3 side, 4 self, 8 all, 0x10 flagged units); bit 0x20 is tested with
     * calcStatGet(unit, 4, 0x2000) */
    unsigned char flags;
    signed char code;                       /* +0x01: func_00252CD8's code; must be 0 for calcStatusHitRoll */
    short scale;                            /* +0x02: percent scale battleEffectApplies passes to func_00252CD8 */
    signed char rollThreshold;              /* +0x04: a first roll below this goes to the second roll */
    signed char secondRollThreshold;        /* +0x05: a second roll below this returns 0 */
    signed char type;                       /* +0x06: compared with 2 by calcHitRecordFill */
    union {
        signed char kind;                   /* kind byte, mapped by calcHitMode and calcKindHitMode */
        unsigned char effectFlags;          /* the same byte as flags 0x1 to 0x40, read by calcHitApply */
        signed char slot;                   /* the slot func_00A92D98 (tu033) accepts below 8 */
    } kindBits;                             /* +0x07 */
    unsigned short hitFlags;                /* +0x08: bits 0x4, 0x8, 0x10 and 0x20 are tested; 0x100 sets object
                                             * flag 0x2000 (unitCmdSetPhase) */
    signed char epCost;                     /* +0x0A: the EP cost calcEpCost scales */
    signed char levelDelta;                 /* +0x0B: unitRecordStateGet's result */
    /* +0x0C: tested against 0x100, 0x200, 0x500 and 0x600 by calcHitStateStep; the low five bits are
     * calcComboUpdate's mask */
    unsigned short actionBits;
    unsigned char power;                    /* +0x0E */
    signed char modeGuard[3];               /* +0x0F..+0x11: bit 0 of modeGuard[mode] blocks the hit */
    /* +0x12..+0x15: one gate byte per row slot (calcHitRowGate), the unit-state codes battleEffectApplies
     * tests; bit 0x10 of the first clears object flag 0x2000 (unitCmdSetPhase) */
    signed char rowGate[4];
    signed char inflictRate;                /* +0x16: chance the value of calcStatResist is taken from */
    signed char row;                        /* +0x17 */
    unsigned short bits;                    /* +0x18: one bit per status of the row */
    short extent;                           /* +0x1A: the extent func_00A93288 (tu033) passes to unitEffectKeyFree */
    /* +0x1C: zero lets calcHitRowGate walk its row gates; unitSlotsClear tests 5, unitAttachRequest 1 */
    signed char gateFlag;
    signed char phase;                      /* +0x1D: the phase unitCmdSetPhase (tu012) sets */
    short timer;                            /* +0x1E: nonzero selects unitSetTimedPhase */
};

/* TecRecord.flags target bit 8 (all targets): such a technique leaves the partner object where it is
 * (eventPairBegin, eventPairEnd) and gives no act stock (unitTryActStock). */
#define TEC_TARGET_ALL 8
/* TecRecord.flags bit 0x20 (also tested with calcStatGet(unit, 4, 0x2000)): cmdExecute does not send the unit to
 * its target, eventUnitCommand reports the technique with its flagged script event. */
#define TEC_FLAG_SCRIPT_FLAGGED 0x20

/* TecRecord.gateFlag values with an evidenced role (tu033). */
#define TEC_GATE_NONE 0             /* calcHitRowGate walks the row gates; the hit adds percent and boost progress */
#define TEC_GATE_PAIR 1             /* pair technique: the partner (unitPairPartnerFind) acts with the unit */
#define TEC_GATE_SLOT_MARK 2        /* cmdExecute marks the pair record's slot (func_00A91B68) */
#define TEC_GATE_STOCK 3            /* cmdExecute runs the command through the action stock (cmdStockRun) */
#define TEC_GATE_SHOW_CHILD 4       /* shows one of the attacker's children during the attack (eventAttackRun) */
#define TEC_GATE_TARGET_MODEL 5     /* loads the model of the target record (tgtIdRecordGet) */
#define TEC_GATE_CURRENT_DRAIN 10   /* takes 100 of the target's current value (attackHitApply) */

/* The record statusRecordGet returns for a status or effect index. */
typedef struct StatusRecord StatusRecord;

struct StatusRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x2];
    /* +0x06: bit 0x2000 marks an effect that lives in the slot table; 0x8000 and 0x4000 are tested by
     * battleSlotExport */
    unsigned short slotFlags;
    unsigned short flags;                   /* +0x08: tested with the caller's mask */
    unsigned short code;                    /* +0x0a: low 7 bits are the result */
};

/* The equipment item record itemRecordGet returns: its code halfword is at +0xA. */
typedef struct ItemRecord ItemRecord;

struct ItemRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x6];
    short code;                             /* +0xA */
    unsigned char unmodeled_0C[0x4];
};

typedef struct HitAction HitAction;

/* Action row hitActionGet returns: the entry index and the amount. */
struct HitAction {
    RecordTexts texts;                      /* +0x00 */
    unsigned short flags;                   /* +0x04: tested by dataSlotKeyTest (tu022) */
    unsigned char unmodeled_06[0x2];        /* +0x06..+0x07 */
    short entry;                            /* +0x08: index tecRecordGet takes */
    unsigned char unmodeled_0a[0x2];        /* +0x0A..+0x0B */
    short amount;                           /* +0x0C: selects the +0x40 action; nonzero picks row 2 */
    unsigned char unmodeled_0e[0x2];        /* +0x0E..+0x0F */
};

/* Link record pairLinkGet returns: the required level and the two slot indices it pairs. */
typedef struct PairLink PairLink;

struct PairLink {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x4];
    short tecIndex;                         /* +0x8: the technique a pair selection uses (battleSelectionGet) */
    short requiredLevel;                    /* +0xA */
    short slotFirst;                        /* +0xC */
    short slotSecond;                       /* +0xE */
};

typedef struct CalcLevelRecord CalcLevelRecord;

/* The record calcLevelRecordGet returns; +0xA is its required level, +0xC the entry mask calcEntryMaskTest tests. */
struct CalcLevelRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_004[0x4];
    short tecIndex;                         /* +0x8: the technique a level selection uses (battleSelectionGet) */
    short requiredLevel;
    short entryMask;
    unsigned char unmodeled_00E[0x2];
};

typedef struct TgtIdRecord TgtIdRecord;

/* The record tgtIdRecordGet returns for an id: +0x8 is the action index tecRecordGet takes, +0xE the minimum level. */
struct TgtIdRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x4];
    short actionIndex;                      /* +0x08: the index tecRecordGet takes */
    short modelIndex;                       /* +0x0A: the model entry actionResourcesLoad loads (0: none) */
    unsigned short modelJoint;              /* +0x0C: the joint that model is attached at (StatBlock.pairSecond[2]) */
    short minLevel;                         /* +0x0E: obj->stats->level below this gives 0 */
};

/* The unit parameters scripts read and write by number (natives unitParaGet and unitParaSet, tu046). 17..36 are
 * the bytes from StatBlock.liveRows on: liveRows (17..20), slotValue (21..28) and slotRoll (29..36). */
enum {
    UNIT_PARA_MAX_HP,
    UNIT_PARA_MAX_EP,
    UNIT_PARA_PHYS_ATTACK,
    UNIT_PARA_PHYS_DEFENCE,
    UNIT_PARA_MAG_ATTACK,
    UNIT_PARA_MAG_DEFENCE,
    UNIT_PARA_HIT_RATE,
    UNIT_PARA_EVADE_RATE,
    UNIT_PARA_VALUE_BIAS,
    UNIT_PARA_HP,
    UNIT_PARA_EP,
    UNIT_PARA_KEY,
    UNIT_PARA_STATE70,                  /* read only */
    UNIT_PARA_LEVEL,
    UNIT_PARA_EXP_LEVEL,
    UNIT_PARA_FLAGS,
    UNIT_PARA_POS_INDEX,
    UNIT_PARA_LIVE_ROW,
    UNIT_PARA_SLOT_VALUE = UNIT_PARA_LIVE_ROW + 4,
    UNIT_PARA_SLOT_ROLL = UNIT_PARA_SLOT_VALUE + 8,
    UNIT_PARA_CURRENT = UNIT_PARA_SLOT_ROLL + 8
};

int unitParaFieldGet(struct Ov01Object *unit, int field);
void unitParaFieldSet(struct Ov01Object *unit, int field, int value);

/* Table getters: each returns record index of its bat.dat table, 0 for an index outside the table. */
TecRecord *tecRecordGet(int index);
void *tableKeyRowGet(int key);
short *effectIdListGet(int index);
ShortPair *slotPairRowGet(int index);
BattleLookupEntry *battleLookupGet(int index);
ItemRecord *itemRecordGet(int index);
StatusRecord *statusRecordGet(int index);
HitAction *hitActionGet(int index);
RecordTexts *func_00A9B580(int index);
struct TextRecord8 *func_00A9B5B8(int index);
PairLink *pairLinkGet(int index);
CalcLevelRecord *calcLevelRecordGet(int index);
TgtIdRecord *tgtIdRecordGet(int index);
UnitExpEntry *unitExpTblGet(void);
GrowthRecord *growthRecordGet(int index);
short *stringOffsetsGet(int index);

/* The two texts of a data record (the *TextGet getters below): the record's first two halfwords are their
 * offsets from the table start. */
enum RecordText {
    RECORD_TEXT_NAME = 0,
    RECORD_TEXT_INFO = 1
};

char *battleLookupTextGet(int index, int field);
char *itemTextGet(int index, int field);
char *statusTextGet(int index, int field);
char *hitActionTextGet(int index, int field);
char *func_00A9B950(int index, int field);
char *func_00A9B9C8(int index, int field);
char *pairLinkTextGet(int index, int field);
char *calcLevelTextGet(int index, int field);
char *tgtIdTextGet(int index, int field);
char *tableStringGet(int index, int mode);

/* Message, party data and table row prototypes. */
int msgIsIdle(void);
void partyDataSetSlot(int id, int slot, int value);
struct TableKeyRecord *tableKeyRecordGet(int key);
void msgReset(void);

#endif /* INCLUDE_OV01_TU042_H */
