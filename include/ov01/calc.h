#ifndef INCLUDE_OV01_CALC_H
#define INCLUDE_OV01_CALC_H

/* Functions of src/ov01/calc.c (the OV01 battle file) that other files call. */

#include "shared.h"
#include "ov01/unit_cmd.h"
#include "ov01/data_record_get.h"

int battleUnitHasEffectCode(int key, int code);
/* Learn flags of a unit (LEARN ALL in the config debug menu, debug_entry.c). */
#define UNIT_LEARN_FLAG_COUNT 0x20

/* BattleUnit and StatBlock (its +0x144 block) are defined in unit_cmd.h. */

/*
 * The boost gauge of a side (boostGaugeParty for the party, boostGaugeRest for the rest; boostGaugeGet
 * picks one for a unit). stock is the number of banked boosts, capped by maxStock (counted up to 5 by
 * calcUseItem, through its unsigned view); progress counts toward the next stock and is held at 50
 * while the gauge is full.
 */
struct BoostGauge {
    union {
        signed char maxStock;
        unsigned char maxStockCount;
    };                                      /* +0x00 */
    signed char stock;                      /* +0x01 */
    signed char progress;                   /* +0x02 */
    unsigned char unmodeled_03;             /* +0x03: pads the gauge to four bytes */
};

/* Slots of the action stock: four stocked units and two boost units. */
#define ACT_STOCK_SLOTS 4
#define ACT_STOCK_BOOST_SLOTS 2

/*
 * The action-stock record (the global actStock, X1 ActStock layout): stock[4] at +0x00,
 * boost[2] at +0x10 (calcBoost fills them), max at +0x18, pickCursor at +0x1C,
 * lastPicked at +0x20, pickSource at +0x24.
 */
struct ActStock {
    BattleUnit *stock[ACT_STOCK_SLOTS];
    BattleUnit *boost[ACT_STOCK_BOOST_SLOTS];
    int max;
    int pickCursor;
    BattleUnit *lastPicked;
    int pickSource;
};

struct ActStock *actStockGet(void);
int battleEffectResolve(struct BattleUnit *command, int index, struct BattleUnit *entity);
int calcStatusHitRoll(struct BattleUnit *source, struct BattleUnit *target, int actionIdx);
void actEntryFlagSet(int index);
void calcEtherDisableSet(int index);

/*
 * Hit record of one target: calcHitRate rolls for it and calcHitDmg fills it; the status
 * steps (calcCrit, calcHitStateStep, calcHitRowGate, calcFollowKind) read it.
 */
struct HitRecord {
    struct BattleUnit *target;                      /* +0x00 */
    int reaction;                           /* +0x04: the UNIT_CMD_* the target answers with (unitJobTypeAtk) */
    int damage;                             /* +0x08: calcDmg result (calcDmgItem for type 2) */
    int followResult;                       /* +0x0c: calcFollowKind result, stored by calcDmg */
    /* +0x10: calcBackAttack's facing test for kind 1; 1 skips calcHitRate and adds 50 to calcCrit's chance */
    int backAttack;
    int stepState;                          /* +0x14: 1 to 3, written by calcHitRowGate and calcHitStateStep */
    union {
        /* calcHit result (calcHitItem for type 2): bit 8 set once taken, bit 9 blocks it */
        int word;
        unsigned char kind;                 /* its low byte: the hit kind calcStatAtk tests */
    } hitMode;                              /* +0x18 */
    int inflictRow;                         /* +0x1c: row calcStatAtk inflicts */
    int inflictMask;                        /* +0x20: bits calcStatAtk inflicts */
    int cureRow;                            /* +0x24: row calcStatClr clears */
    int cureMask;                           /* +0x28: bits calcStatClr clears */
};

/*
 * Command record of one attack (16 bytes); tgtFindFirst reads its attacker and action. The cmd_exec.c command
 * handlers (cmdAttackQueueAndSpend builds it, cmdTechniqueAttackQueue, cmdItemAttackQueue, attackEffectsFinished) take one too.
 */
struct AttackCommand {
    struct BattleUnit *attacker;                    /* +0x00 */
    int kind;                               /* +0x04: 4 selects the base chance 200 in calcStatInflictRate */
    int actionIdx;                          /* +0x08: index tecRecordGet takes */
    int id;                                 /* +0x0C: the lookup key (the third argument of calcUseItem) */
};

/* An attack command with the hit record of its current target, as calcUseEtherMenu and calcUseItem build
 * them on their frame. */
struct AttackHit {
    struct AttackCommand command;           /* +0x00 */
    struct HitRecord hit;                   /* +0x10 */
};
BattleUnit *actStockPick(void);
int calcBattleChk(BattleUnit *unit);

/*
 * Slot records: 0x40-byte records in slotRecordTable. Record k (k = slot index 0..15)
 * starts at 0x61C3C0 + k * 0x40, so slotRecordTable is record 1. slotRecordGet takes
 * the slot index from the owner's key (+0x62 of its block). The bit numbers passed in
 * are 1-based: code bits 1..59, flag bits 1..104, pair bits 1..16, mark bits 1..21.
 * The four test bodies read the bit arrays through their signed views.
 */
typedef struct SlotRecord SlotRecord;

struct SlotRecord {
    int gainFirst;                          /* +0x00: capped at 99999999 */
    int expCounter;                         /* +0x04: the experience counter calcLvUp spends; it levels up at 0 */
    int gainSecond;                         /* +0x08: capped at 99999 */
    int gainThird;                          /* +0x0C: capped at 99999 */
    unsigned short growth[8];               /* +0x10: growth values; dataPlUnitInit loads their start, unitGrowthStep steps them toward the GrowthRecord targets */
    union {
        unsigned char codeBits[8];
        signed char codeBitsSigned[8];
    };                                      /* +0x20 */
    unsigned char unmodeled_28[0x2C - 0x28];
    union {
        unsigned char flagBits[13];
        signed char flagBitsSigned[13];
    };                                      /* +0x2C */
    unsigned char unmodeled_39[0x3C - 0x39];
    union {
        unsigned char pairBits[2];
        signed char pairBitsSigned[2];
    };                                      /* +0x3C */
    union {
        unsigned char markBits[3];
        signed char markBitsSigned[3];
    };                                      /* +0x3E */
};

SlotRecord *slotRecordByIndex(int index);
void unitGrowthStep(int key);

/* Party slot and battle entry interface used by the menus and the debug config. */
StatBlock *battleEntryFindByKey(int key);
StatBlock *battleEntryAcquire(BattleUnit *unused, int key);
void battleEntryActivate(BattleUnit *owner, int key);
int battleSlotLinkedSlotSet(int index, int pairIndex, int pairedId);
void battleSlotMarkSet(int key, int mark);
int battleSlotMarkTest(int key, int mark);
void battleSlotsRestoreAll(void);
int calcAccEquipOrg(int index, int slot, int equipId);
int calcSkillEquipOrg(int index, int slot, int effectCode);
int actStockHasBoost(void);
void calcDeadReset(void);
int calcSkillEquChkAll(int effectCode);
StatBlock *calcTotalParaMenu(int key);
int calcTakeEpGetMenu(int key, int lookupKey);
int calcUseEtherMenu(int attackerKey, int targetKey, int lookupKey);
void calcTurnEnd(BattleUnit *obj);
void dataEtherLearnClear(int key, int flag);
void dataEtherLearnSet(int key, int flag);
int dataSkillLearnGet(int key, int code);
int dataSkillLearnSet(int key, int code);
void partySnapshotBuild(void);
int sdvSpecialBitGet(int index);
int slotPairSet(int index);
int unitIsInFacingArc(BattleUnit *origin, BattleUnit *target);

/* Turn, target search and combat helpers other battle files call. */
/* Sums calcSkillSub over the unit's equipment and effect codes (unit, flag mask, code mask). Unprototyped: calcStatInflictRate calls it
 * with a fourth argument that the body never reads, and the argument load is part of the object. */
int calcSkill();
int calcSkillEquChk(BattleUnit *unit, int effectCode);
void calcTurnStart(BattleUnit *obj);
void calcComboReset(BattleUnit *owner);
signed char unitRecordStateGet(BattleUnit *unit, int index);
int unitSidesDiffer(BattleUnit *unitA, BattleUnit *unitB);
void unitEntryTableClear(BattleUnit *owner);
int unitStateAdvance(BattleUnit *owner);
int unitEntriesAllMarked(BattleUnit *owner);
void actStepCounterAdvance(void);
BattleUnit *tgtFindFirst(struct AttackCommand *record, BattleUnit *value);
BattleUnit *tgtFindNext(void);
int effectPosIsFree(BattleUnit *unused_arg0, int index);
int effectPosCountFree(BattleUnit *passthrough);
int battleMaskWordsUnmatched(BattleUnit *entity, int *out);
int calcCfEventKeyTest(BattleUnit *unit);
void dataPlUnitInit(void);

/* Battle unit and action-stock prototypes. */
int dataEtherLearnGet(int key, int flag);
struct StatBlock *battleSlotGet(int index);
int battleEffectApplies(struct BattleUnit *command, int index, struct BattleUnit *entity);
int battleEntryRelease(struct BattleUnit *owner);
int partyUnitHpLow(struct BattleUnit *owner);
int calcTgtUnitTbl(struct BattleUnit *target, int flags, struct BattleUnit **found);
void calcBc(struct BattleUnit *unit, int amount);
struct BoostGauge *boostGaugeSelect(int flag);
int calcStatBit2Idx(int statBit);
void calcBoost(struct BattleUnit *unit, int slot);
int calcBoostChk(struct BattleUnit *obj, int id);
int calcEthEnableChk(struct BattleUnit *entity, int index);
/* Unit HP and status rows: calcHp returns 1 while HP stays above 0, calcStatGet the set bits of mask in
 * row, calcStatReset/calcStatProtectReset clear them (both return 1). */
int calcHp(struct BattleUnit *unit, int delta);
/* EP moves by delta, clamped to [0, max]; returns 1. */
int calcEp(struct BattleUnit *unit, int delta);
/* The status rows (the row argument of calcStatGet, calcStatReset, calcStatProtectReset and calcStatSet). */
#define STAT_ROW_1 1
#define STAT_ROW_2 2
#define STAT_ROW_3 3
int calcStatGet(struct BattleUnit *owner, int row, int mask);
int calcStatReset(struct BattleUnit *owner, int row, int mask);
int calcStatProtectReset(struct BattleUnit *owner, int statIdx, int mask);
int calcStatSet(struct BattleUnit *unit, int row, int statBit);
/* The hit of an attack on target: calcHpDmg with the hit mode and damage, calcUseItem for a lookup key. */
int calcHpDmg(struct BattleUnit *attacker, struct BattleUnit *target, int hitMode, int damage);
int calcUseItem(struct BattleUnit *source, struct BattleUnit *target, int lookupKey);
int unitPercentAdd(struct BattleUnit *unit, int amount);
void calcBp(struct BattleUnit *unit, int amount);
/* Clears every status row of the unit (active masks and turn counts). */
void calcStatResetCha(struct BattleUnit *owner);
void calcHitDmg(struct AttackCommand *action, struct HitRecord *hit);
int calcTakeEpGet(struct BattleUnit *owner, int index);
int slotCodeTest(struct BattleUnit *owner, int bit);
int slotPairTest(int index);
int calcItmEnableChk(struct BattleUnit *target, int id);
int unitLevelAdd(struct BattleUnit *unit, int amount);
int unitCurrentAdd(struct BattleUnit *unit, int amount);
int boostGaugeAddStock(struct BoostGauge *gauge, int amount);
/* The record finders below write the indices they find to found and return their count. */
int calcKeyBandRecords(struct BattleUnit *entity, int *found);
int calcCurrentMeetsRecord(struct BattleUnit *entity, int index);
int calcKeyPairRecords(struct BattleUnit *entity, int *found);
int calcLevelsMeetRecord(struct BattleUnit *entity, int record_index);
int calcPairedRecords(struct BattleUnit *unused, int code, int *found);
int calcPairEntryUsable(struct BattleUnit *entity, int index);
int slotFlagCollect(struct BattleUnit *subject, int *found);
int tgtSideAccepts(struct BattleUnit *obj, int id);
int actLevelChanceRoll(struct BattleUnit *owner, int index);
int actStepCounterValue(void);
void actStockClearBoost1(void);
/* Drops unit from the stock; *stockIndex receives its stock position, or -1. */
int actStockRemoveStock(struct BattleUnit *unit, int *stockIndex);
/* Drops unit from the two boost slots or from the stock; *stockIndex receives its stock position, or -1. */
int actStockRemove(struct BattleUnit *unit, int *stockIndex);
void monsGainSet(struct BattleUnit *unit);
void sdvInitSpecialWork(void);

/*
 * The 0x50-byte battle result block (battleResult): word totals at +0x00, +0x04 and +0x08; up to eight
 * (category, id) pairs of dropped items from +0x0C and +0x2C (dataBoxInc and dataBoxChk take them in
 * that order, battleResultBuildEntries of src/main/battle_result.c switches on the category); the pair count at +0x4C.
 */
struct BattleResult {
    int points;                    /* +0x00: points summed by monsGainSet */
    int stepRateTotal;             /* +0x04: stepRate summed, scaled by the phase-2 multiplier */
    int flatTotal;                 /* +0x08: flat summed by monsGainSet */
    int categories[8];             /* +0x0C: data box category of each pair */
    int ids[8];                    /* +0x2C: data box id of each pair */
    int count;                     /* +0x4C */
};

/*
 * One 28-byte row calcResult fills per party unit of keys below 16: the unit key, the three stat gains
 * calcResultSub adds to the slot record and the levels, hp cap and ep cap the experience granted.
 */
struct BattleResultRow {
    int key;                       /* +0x00 */
    int gainFirst;                 /* +0x04 */
    int gainSecond;                /* +0x08 */
    int gainThird;                 /* +0x0C */
    int levelGain;                 /* +0x10 */
    int maxHpGain;                 /* +0x14 */
    int maxEpGain;                 /* +0x18 */
};

/* Battle result block and party snapshot prototypes. */
struct BattleResult *monsGainPtrGet(void);
int calcResult(struct BattleResultRow *rows);
int partySnapshotGet(BattleUnit ***snapshots);
void battleWorkTablesReset(void);
int calcTrg2PE(struct BattleUnit *entity, int flags);

/*
 * The step counter actStepCounter: step runs 0..3; at step 3 `hold` is a random value of 4 or
 * more. actStepCounterSet draws the multiplier and clears the words from +0xC.
 */
struct ActStepCounter {
    int step;                               /* +0x00 */
    int hold;                               /* +0x04 */
    int multiplier;                         /* +0x08: 2, 4 or 10; monsGainSet's rate at phase 2 */
    union {
        int cleared0C;                      /* +0x0C: cleared by actStepCounterSet */
        int levelGranted;                   /* same word: set once calcDmg granted the attacker a level */
        int statPanelState;                 /* same word: menuUnitPanelOpen creates the stat panel while it is 1, then sets 2 */
    };
    int bonusFlag;                          /* +0x10: set by calcCrit's bonus roll */
    int stepState;                          /* +0x14: 1 to 3, set by calcHitStateStep and calcHitRowGate */
};

struct ActStepCounter *actStepCounterGet(void);

#endif /* INCLUDE_OV01_CALC_H */
