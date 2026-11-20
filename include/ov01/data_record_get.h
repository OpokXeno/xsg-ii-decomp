#ifndef INCLUDE_OV01_DATA_RECORD_GET_H
#define INCLUDE_OV01_DATA_RECORD_GET_H

/* Functions of src/ov01/data_record_get.c that other files call. */

/* Unit record of yamamoto\batdat.us for a unit index, and the unit's name (data_record_get.c has the numbering). */
void *batUnitRecordGet(int index);
char *batUnitNameGet(int index);

/* Unaligned signed word: the record's word sits at a 2-byte offset. */
typedef signed int UnalignedS32 __attribute__((aligned(1)));

/* One 4-byte entry of the unit experience table dataExpTblGet returns. */
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
 * dataPlUnitInit loads into the slot record's growth values, and the targets unitGrowthStep
 * steps them toward.
 */
typedef struct GrowthRecord GrowthRecord;

struct GrowthRecord {
    unsigned short start[8];                /* +0x00: the growth values a slot starts with */
    unsigned short target[8];               /* +0x10: the values the growth values of the slot record move toward */
};

/* Partial view of a 16-byte enemy reward record enemyRewardRecordGet returns: experience, skill points, class points, chances and the two (category, id) drop pairs. */
struct EnemyRewardRecord {
    int exp;                       /* +0x00: experience */
    short skillPoints;             /* +0x04: skill points */
    short classPoints;             /* +0x06: class points */
    unsigned char secondChance;    /* +0x08 */
    unsigned char firstChance;     /* +0x09 */
    unsigned char secondCategory;  /* +0x0A: the data box category of the second pair, zero when it drops nothing */
    unsigned char firstCategory;   /* +0x0B: the data box category of the first pair */
    unsigned char secondId;        /* +0x0C: the data box id of the second pair */
    unsigned char firstId;         /* +0x0D: the data box id of the first pair */
};

/* The records of the bat.dat tables (the getters below, numbered from 1). */
/* The first two halfwords of a bat.dat record that has texts: the offsets of its RECORD_TEXT_NAME and
 * RECORD_TEXT_INFO texts from the start of its table (the *TextGet getters). */
typedef struct RecordTexts {
    short text[2];
} RecordTexts;

/* The 8-byte records dataBoxKeyItemRecordGet returns. */
typedef struct TextRecord8 {
    RecordTexts texts;                      /* +0x00 */
    signed char mapModel[2];                /* +0x04: the map object model offset by form (uwamonoCreateRecord: form 1.0 takes [0]) */
    unsigned char unmodeled_06[0x2];
} TextRecord8;

/* The ether entry etherRecordGet returns. */
typedef struct EtherRecord EtherRecord;

struct EtherRecord {
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
    signed char code;                       /* +0x01: MMathActorsInRangeByKind's code; must be 0 for calcStatusHitRoll */
    short scale;                            /* +0x02: percent scale battleEffectApplies passes to MMathActorsInRangeByKind */
    signed char evadeThreshold;             /* +0x04: a first roll below this goes to the second roll */
    signed char guardThreshold;             /* +0x05: a second roll below this returns 0 */
    signed char type;                       /* +0x06: compared with 2 by calcHitDmg */
    union {
        signed char kind;                   /* kind byte, mapped by calcHit and calcHitItem */
        unsigned char effectFlags;          /* the same byte as flags 0x1 to 0x40, read by calcUseItem */
        signed char slot;                   /* the slot cmdTechniqueAttackQueue (cmd_exec.c) accepts below 8 */
    } kindBits;                             /* +0x07 */
    unsigned short hitFlags;                /* +0x08: bits 0x4, 0x8, 0x10 and 0x20 are tested; 0x100 sets object
                                             * flag 0x2000 (unitCmdSetPhase) */
    signed char epCost;                     /* +0x0A: the EP cost calcTakeEpGet scales */
    signed char waitWeight;                 /* +0x0B: the wait weight, unitRecordStateGet's result */
    /* +0x0C: the attribute bits, tested against 0x100 (Down), 0x200 (Air), 0x500 and 0x600 by calcHitStateStep; the low five bits are
     * calcComboUpdate's mask */
    unsigned short attributeBits;
    unsigned char power;                    /* +0x0E */
    signed char modeGuard[3];               /* +0x0F..+0x11: bit 0 of modeGuard[mode] blocks the hit */
    /* +0x12..+0x15: the zones the technique strikes, one byte per row slot (calcHitRowGate), the unit-state codes battleEffectApplies
     * tests; bit 0x10 of the first clears object flag 0x2000 (unitCmdSetPhase) */
    signed char strikeZones[4];
    signed char inflictRate;                /* +0x16: chance the value of calcStatResist is taken from */
    signed char row;                        /* +0x17 */
    unsigned short bits;                    /* +0x18: one bit per status of the row */
    short effectRecord;                     /* +0x1A: the effect record number attackEffectsFinished (cmd_exec.c) passes to
                                             * unitEffectKeyFree */
    /* +0x1C: the technique class; zero lets calcHitRowGate walk its zones, unitSlotsClear tests 5, unitAttachRequest 1 */
    signed char techClass;
    signed char phase;                      /* +0x1D: the phase unitCmdSetPhase (unit_cmd.c) sets */
    short motionIndex;                      /* +0x1E: the motion table index; nonzero selects unitSetTimedPhase */
};

/* TecRecord.flags target bit 8 (all targets): such a technique leaves the partner object where it is
 * (eventPairBegin, eventPairEnd) and gives no act stock (counterProc). */
#define TEC_TARGET_ALL 8
/* TecRecord.flags bit 0x20 (also tested with calcStatGet(unit, 4, 0x2000)): cmdExecute does not send the unit to
 * its target, eventUnitCommand reports the technique with its flagged script event. */
#define TEC_FLAG_SCRIPT_FLAGGED 0x20

/* TecRecord.techClass values with an evidenced role (cmd_exec.c). */
#define TEC_GATE_NONE 0             /* calcHitRowGate walks the row gates; the hit adds percent and boost progress */
#define TEC_GATE_PAIR 1             /* pair technique: the partner (unitPairPartnerFind) acts with the unit */
#define TEC_GATE_SLOT_MARK 2        /* cmdExecute marks the pair record's slot (calcEtherDisableSet) */
#define TEC_GATE_COMBINED_ETHER 3   /* cmdExecute runs the combined ether of two units (cmdCombinedEtherRun) */
#define TEC_GATE_SHOW_CHILD 4       /* shows one of the attacker's children during the attack (eventAttackRun) */
#define TEC_GATE_TARGET_MODEL 5     /* loads the model of the target record (attackTargetRecordGet) */
#define TEC_GATE_CURRENT_DRAIN 10   /* takes 100 of the target's current value (attackHitApply) */

/* The record skillRecordGet returns for a status or effect index. */
typedef struct SkillRecord SkillRecord;

struct SkillRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x2];
    /* +0x06: bit 0x2000 marks an effect that lives in the slot table; 0x8000 and 0x4000 are tested by
     * calcCopyParaMake */
    unsigned short slotFlags;
    unsigned short flags;                   /* +0x08: tested with the caller's mask */
    unsigned short code;                    /* +0x0a: low 7 bits are the result */
};

/* The equipment item record accessoryRecordGet returns: its code halfword is at +0xA. */
typedef struct EsAccessoryRecord EsAccessoryRecord;

struct EsAccessoryRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x6];
    short code;                             /* +0xA */
    signed char mapModel[2];                /* +0x0C: the map object model offset by form (uwamonoCreateRecord: form 1.0 takes [0]) */
    unsigned char unmodeled_0E[0x2];
};

typedef struct UsableItemRecord UsableItemRecord;

/* Action row usableItemRecordGet returns: the entry index and the amount. */
struct UsableItemRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned short flags;                   /* +0x04: tested by dataSlotKeyTest (battle_init.c) */
    unsigned char unmodeled_06[0x2];        /* +0x06..+0x07 */
    short entry;                            /* +0x08: index tecRecordGet takes */
    signed char mapModel[2];                /* +0x0A: the map object model offset by form (uwamonoCreateRecord: form 1.0 takes [0]) */
    short amount;                           /* +0x0C: selects the +0x40 action; nonzero picks row 2 */
    unsigned char unmodeled_0e[0x2];        /* +0x0E..+0x0F */
};

/* Link record pairLinkGet returns: the required level and the two slot indices it pairs. */
typedef struct DoubleTechRecord DoubleTechRecord;

struct DoubleTechRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x4];
    short tecIndex;                         /* +0x8: the technique a pair selection uses (convKey2unit) */
    short requiredLevel;                    /* +0xA */
    short slotFirst;                        /* +0xC */
    short slotSecond;                       /* +0xE */
};

typedef struct EsSpecialRecord EsSpecialRecord;

/* The record attackLevelRecordGet returns; +0xA is its required level, +0xC the entry mask calcEntryMaskTest tests. */
struct EsSpecialRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_004[0x4];
    short tecIndex;                         /* +0x8: the technique a level selection uses (convKey2unit) */
    short requiredLevel;
    short entryMask;
    unsigned char unmodeled_00E[0x2];
};

typedef struct SpecialAttackRecord SpecialAttackRecord;

/* The record attackTargetRecordGet returns for an id: +0x8 is the action index tecRecordGet takes, +0xE the minimum level. */
struct SpecialAttackRecord {
    RecordTexts texts;                      /* +0x00 */
    unsigned char unmodeled_04[0x4];
    short actionIndex;                      /* +0x08: the index tecRecordGet takes */
    short modelIndex;                       /* +0x0A: the model entry actionResourcesLoad loads (0: none) */
    unsigned short modelJoint;              /* +0x0C: the joint that model is attached at (StatBlock.pairSecond[2]) */
    short minLevel;                         /* +0x0E: obj->stats->stockCount below this gives 0 */
};

/* A row of the key table (tableKeyRowGet): 20 bytes of halfword entries. thinkDispatch reads entry `arg` of the
 * unit's row and entry 0 when that one is 0. */
typedef struct TableKeyRow {
    short entries[10];
} TableKeyRow;

/* Table getters: each returns record index of its bat.dat table, 0 for an index outside the table. */
TecRecord *tecRecordGet(int index);
TableKeyRow *tableKeyRowGet(int key);
short *effectIdListGet(int index);
ShortPair *slotPairRowGet(int index);
EtherRecord *etherRecordGet(int index);
EsAccessoryRecord *accessoryRecordGet(int index);
SkillRecord *skillRecordGet(int index);
UsableItemRecord *usableItemRecordGet(int index);
RecordTexts *dataBoxSecretKeyRecordGet(int index);
struct TextRecord8 *dataBoxKeyItemRecordGet(int index);
DoubleTechRecord *pairLinkGet(int index);
EsSpecialRecord *attackLevelRecordGet(int index);
SpecialAttackRecord *attackTargetRecordGet(int index);
UnitExpEntry *dataExpTblGet(void);
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
char *dataBoxCategory4TextGet(int index, int field);
char *dataBoxCategory3TextGet(int index, int field);
char *pairLinkTextGet(int index, int field);
char *calcLevelTextGet(int index, int field);
char *tgtIdTextGet(int index, int field);
char *tableStringGet(int index, int mode);

/* Unit and name table indices of the data loaders: the model key of a unit index, and the motion and voice name
 * indices (both return their argument). */
int dataModelKeyGet(int index);
int dataMotNameIndexGet(int value);
int dataVoiceNameIndexGet(int value);

/* Table row prototype. */
struct EnemyRewardRecord *enemyRewardRecordGet(int key);

#endif /* INCLUDE_OV01_DATA_RECORD_GET_H */
