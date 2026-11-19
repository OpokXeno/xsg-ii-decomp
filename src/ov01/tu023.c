/*
 * OV01 battle file: 0x00a8a8b8..0x00a92150, the former TUs tu023..tu032 (one original
 * file, TU merge 2026-11-17; see JOINED_CUTS in tools/tu/map_infer.py).
 */
#include "common.h"
#include "main/tu110.h"
#include "shared.h"
#include "ov01/tu023.h"
#include "ov01/tu042.h"
#include "ov01/tu005.h"
#include "ov01/tu036.h"
#include "ov01/tu043.h"

/*
 * Key limits of the battle entry pools. A key below BATTLE_SLOT_KEY_LIMIT names a party slot (slots 1 to 15,
 * battleSlotGet takes the slot index); keys from BATTLE_POOL_A_KEY_MIN take a pool-A entry, the keys in between
 * a pool-B entry (battleEntryAcquire).
 */
#define BATTLE_SLOT_KEY_LIMIT 16
#define BATTLE_POOL_A_KEY_MIN 141
#define BATTLE_ENTRY_POOL_A_COUNT 8
#define BATTLE_ENTRY_POOL_B_COUNT 5

/* Bit counts and caps of the slot records, the boost gauge, the unit flags and the action stock and step counter. */
#define SLOT_CODE_COUNT 59
#define SLOT_PAIR_COUNT 16
#define SLOT_MARK_COUNT 21
#define SLOT_TOTAL_CAP 99999999
#define SLOT_CAPPED_CAP 99999
#define STAT_CAP_PHYS 999
#define PERCENT_MAX 100
#define UNIT_LEVEL_MAX 3
#define BOOST_PROGRESS_PER_STOCK 50
#define STAT_FLAG_PERCENT_GATE 0x800
#define ACT_STEP_COUNT 4
#define ACT_STEP_HOLD 3


/*
 * Slot records: 0x40-byte records in slotRecordTable. Record k (k = slot index 0..15)
 * starts at D_61C3C0 + k * 0x40, so slotRecordTable is record 1. slotRecordGet takes
 * the slot index from the owner's key (+0x62 of its block). The bit numbers passed in
 * are 1-based: code bits 1..59, flag bits 1..104, pair bits 1..16, mark bits 1..21.
 * The four test bodies read the bit arrays through their signed views.
 */
typedef struct SlotRecord SlotRecord;

struct SlotRecord {
    int gainFirst;                          /* +0x00: capped at 99999999 */
    int expCounter;                         /* +0x04: the experience counter unitExpGain spends; it levels up at 0 */
    int gainSecond;                         /* +0x08: capped at 99999 */
    int gainThird;                          /* +0x0C: capped at 99999 */
    unsigned short growth[8];               /* +0x10: growth values; battleSlotsInit loads their start, unitGrowthStep steps them toward the GrowthRecord targets */
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


/* One 0x1F0-byte unit record, copied as doublewords. */
struct Ov01Snapshot {
    unsigned long long words[62];
};

/*
 * Uninitialized storage of this file, in address order (only this file names these labels).
 * boostGaugeParty is the party gauge (maxStock, stock, progress at +0, +1, +2); the rest gauge is at +4.
 */
static StatOwner battleContextTable[2];
static StatBlock battleStateTable[2];
static StatBlock battleEntryPoolA[BATTLE_ENTRY_POOL_A_COUNT];
static StatBlock battleEntryPoolB[BATTLE_ENTRY_POOL_B_COUNT];
static struct ActStock actStock;
static signed char boostGaugeParty[8];
static StatOwner *tgtTbl[8];
static int tgtIdx;
static int tgtNum;
static struct ActStepCounter actStepCounter;
static struct BattleResult battleResult;
static struct Ov01Snapshot partySnapshots[3];
static StatOwner *partySnapshotPtrs[4];
static unsigned char actEntryFlags[24];
static unsigned char D_00AC2AC8[8];

extern StatBlock battleSlotTable[];
extern unsigned char D_0069DC7C;
/*
 * tgtFindFirst's candidate table: tgtNum entries of tgtTbl are filled in target-search
 * order, tgtIdx walks them out one at a time. The gap between tgtTbl and tgtIdx
 * (0x00a57e68..0x00a57e88, 0x20 bytes) is this array's only size evidence.
 */
extern int battleMaskWordTable[];
extern unsigned char slotRecordTable[];
void *memset(void *destination, int value, unsigned int count);
int rnd(int limit);
float atan2f(float y, float x);
/* Defined in this file (or still asm here). */
void actStockReset(void);
void battleResultReset(void);
void actStepCounterSet(int step);
void battleEntryClearReset(StatOwner *owner);
StatBlock *battleEntryFindByKey(int key);
void battleSlotCommit(StatOwner *owner);
void battleEntryLoad(StatBlock *entry, int key);
/*
 * Binds slot `key` to the battle block of the unit selected by contextIndex
 * (battleAttackResolve passes 1 for its targets, every other caller 0) and returns that unit. The slot is exported
 * into the block, the block becomes the unit's stats, and the slot's hp and ep are then clamped to the block's hp
 * cap and ep cap.
 */
StatOwner *battleContextBind(int key, int contextIndex);
int calcHpDmg(StatOwner *attacker, StatOwner *target, int hitMode, int damage);
int calcEpAdjust(StatOwner *unit, int delta);
int calcHitApply(StatOwner *source, StatOwner *target, int lookupKey);
int unitHasEffectCode(StatOwner *unit, int effectCode);
/* calcHpAdjust: HP at +0x5C moves by delta, clamped to [0, max] (max is the unaligned int at +0x2); returns 1 while HP stays above 0. calcHpDmg calls it. */
int calcHitRateGet(StatOwner *owner);
int calcEvadeRate(StatOwner *owner);
int calcStatResist(StatOwner *owner, int row, int statBit);
int calcAttackStatusMask(StatOwner *owner, int statusMask);
/* calcHitMode: maps the kind byte (+0x7) of a command record to the packed hit mode that calcHpDmg takes; calls calcHitRoll for attack kinds. */
int calcHitMode(struct AttackCommand *action, struct HitRecord *hit);
/* calcAttackDamage: damage of one attack from the attacker's and defender's stats, the command power and the status rows. */
int calcAttackDamage(struct AttackCommand *action, struct HitRecord *hit);
int calcKindHitMode(struct AttackCommand *command, struct HitRecord *hit);
/* calcRestoreValue: the value of one hit on the HP path (mode bits 0x1 and 0x8) or the EP path (bit 0x2): the action amount scales the HP or EP cap, then the stat row 3 gate, the effect sum and the cap decide it. */
int calcRestoreValue(struct AttackCommand *command, struct HitRecord *hit, int mode, int resetGate);
int calcHitRowGate(struct AttackCommand *command, struct HitRecord *hit);
signed char unitStateFlagGet(StatOwner *owner);
void unitEntryKindAdd(StatOwner *owner, int kind);
int calcBackAttack(int *attackerSlot, int *targetSlot);
int unitIsInFacingArc(int originAddress, int targetAddress);
int unitStatsRefresh(StatOwner *unit);
void calcStatTurn(StatOwner *owner);
signed char calcStatDefTurnGet(int row, int statBit);
int calcStatIdx2Bit(int statIdx);
int calcStatInflictRate(struct AttackCommand *command, StatOwner **targetSlot, int row, int statBit);
/* unitApplyStatBonus: a bonus record flagged 0x8000 adds its value, scaled by calcStatEffectSum, to the unit's halfword at +0x8, capped at 999. Other branches not evidenced here. */
void unitApplyStatBonus(StatOwner *unit, int recordIdx);
/* Called with three arguments and, by calcStatInflictRate, with a fourth that the body never reads. */
int calcStatEffectSum();
int calcStatEffectValue(StatOwner *unusedUnit, int recordIdx, int flagMask, int codeMask);
SlotRecord *slotRecordGet(StatOwner *owner);
SlotRecord *slotRecordByIndex(int index);
int slotFlagSet(StatOwner *owner, int bit);
int slotFlagTest(StatOwner *owner, int bit);
int slotFlagClear(StatOwner *owner, int bit);
int slotCodeSet(StatOwner *owner, int bit);
int slotPairTest(int index);
int slotMarkSet(StatOwner *owner, int bit);
int slotMarkTest(StatOwner *owner, int bit);
int slotMarkClear(StatOwner *owner, int bit);
void battleFlushActiveEntries(void);
/*
 * Returns 1 for a unit whose flags (+0x0C) lack 0x4 and for which the status tests of calcStatGet for
 * (group 1, 0x20), (group 2, 0x20) and (group 2, 0x1) are all clear; 0 otherwise, and for a null unit.
 */
int unitCanAct(StatOwner *unit);
int unitValueCalc(StatOwner *obj);
void unitValueReset(StatOwner *obj);
int tgtCmdAccepts(struct AttackCommand *record, StatOwner *value);
struct BoostGauge *boostGaugeGet(StatOwner *unit);
int boostGaugeAddProgress(struct BoostGauge *gauge, int amount);
void sdvSpecialBitSet(int index);
signed char actEntryFlagGet(int index);
signed char func_00A91B90(int index);
/* Refreshes the hp and ep of the slots 1 to 15 from their battle blocks, then commits each. */
void battleSlotsRefresh(void);
/* Defined in other files. */
int unitTblGet(int side, StatOwner ***table);

/* ---- old ov01/tu023 ---- */
/* Resets the battle work tables: both entry pools, the action stock, the boostGaugeParty block, the target list, the result and the flag tables. */
void battleWorkTablesReset(void)
{
    int startStock;
    int maxStock;

    memset(battleEntryPoolA, 0, sizeof(battleEntryPoolA));
    memset(battleEntryPoolB, 0, sizeof(battleEntryPoolB));
    actStockReset();
    memset(boostGaugeParty, 0, sizeof(boostGaugeParty));
    startStock = D_0069DC7C;
    boostGaugeParty[0] = 3;
    boostGaugeParty[1] = startStock;
    boostGaugeParty[4] = 3;
    maxStock = 3;
    if (maxStock <= (signed char) startStock) {
        boostGaugeParty[1] = maxStock;
        boostGaugeParty[2] = 50;
    }
    boostGaugeParty[5] = 0;
    memset(tgtTbl, 0, sizeof(tgtTbl));
    tgtIdx = 0;
    tgtNum = 0;
    actStepCounterSet(rnd(2));
    battleResultReset();
    memset(actEntryFlags, 0, 0x11);
    memset(D_00AC2AC8, 0, 3);
}

void battleEntryClearReset(StatOwner *owner)
{
    StatBlock *block = owner->stats;

    memset(block->entryKind, 0, sizeof(block->entryKind));
    memset(block->entryMark, 0, sizeof(block->entryMark));
    block->entryCount = 0;
}

void battleStateClearWords(StatOwner *owner)
{
    StatBlock *block = owner->stats;

    block->comboMask = 0;
    block->comboHits = 0;
}

/* Copies the battleMaskWordTable word of each bit set in stat4 into words, ends the list with 0 and returns the count. */
int battleMaskWordsStat4(int *words)
{
    unsigned short mask;
    int bit;
    int count;

    mask = ptGetStatusBlock()->memberMask;
    count = 0;
    for (bit = 0; bit < 16; bit++) {
        if ((mask >> bit) & 1) {
            words[count] = battleMaskWordTable[bit];
            count++;
        }
    }
    words[count] = 0;
    return count;
}

/* Copies the battleMaskWordTable word of each bit set in stat6 into words, ends the list with 0 and returns the count. */
int battleMaskWordsStat6(int *words)
{
    unsigned short mask;
    int bit;
    int count;

    mask = ptGetStatusBlock()->presentMask;
    count = 0;
    for (bit = 0; bit < 16; bit++) {
        if ((mask >> bit) & 1) {
            words[count] = battleMaskWordTable[bit];
            count++;
        }
    }
    words[count] = 0;
    return count;
}

/* Returns the first of the eight battleEntryPoolA entries whose key is key, or 0. */
StatBlock *battleEntryFindByKey(int key)
{
    int index;

    for (index = 0; index < BATTLE_ENTRY_POOL_A_COUNT; index++) {
        if (battleEntryPoolA[index].key == key) {
            return &battleEntryPoolA[index];
        }
    }
    return 0;
}

/*
 * Allocates the entry for key. A key below 16 takes its pool-A entry when one exists, else the
 * first free pool-A entry; keys 16 to 140 take a free pool-B entry and the rest a free pool-A
 * entry. A free entry is one whose key is 0. Returns the entry, or 0 when none is free.
 */
StatBlock *battleEntryAcquire(int unused, int key)
{
    StatBlock *entry;
    int index;

    entry = 0;
    if (key < BATTLE_SLOT_KEY_LIMIT) {
        entry = battleEntryFindByKey(key);
        if (entry != 0) {
            return entry;
        }
        for (index = 0; index < BATTLE_ENTRY_POOL_A_COUNT; index++) {
            if (battleEntryPoolA[index].key == 0) {
                battleEntryPoolA[index].key = key;
                entry = &battleEntryPoolA[index];
                break;
            }
        }
    } else if (key >= BATTLE_POOL_A_KEY_MIN) {
        for (index = 0; index < BATTLE_ENTRY_POOL_A_COUNT; index++) {
            if (battleEntryPoolA[index].key == 0) {
                battleEntryPoolA[index].key = key;
                entry = &battleEntryPoolA[index];
                break;
            }
        }
    } else {
        for (index = 0; index < BATTLE_ENTRY_POOL_B_COUNT; index++) {
            if (battleEntryPoolB[index].key == 0) {
                battleEntryPoolB[index].key = key;
                entry = &battleEntryPoolB[index];
                break;
            }
        }
    }
    return entry;
}

int battleStateClearStatus(StatOwner *owner)
{
    StatBlock *block = owner->stats;

    if (block != 0) {
        block->key = 0;
        return 1;
    }
    return 0;
}

/*
 * Exports entry `entry` to `out`: copies the whole entry, stores the byte of the lookup
 * record at +0x17, and for the slots 11 to 15 copies the code and flag marks of the slot
 * into `out` through two owner records.
 */
void battleSlotExport(StatBlock *entry, StatBlock *out)
{
    StatOwner entryOwner;
    StatOwner slotOwner;
    SlotRecord *slotRecord;
    int specialSlotKey;
    signed char *effectCodes;
    int statusKey;
    int markIndex;

    *out = *entry;
    out->inflictBase = ((unsigned char *) batUnitRecordGet(entry->key))[0x17];
    if ((unsigned short) entry->key - 0xB < 5U) {
        specialSlotKey = 0xC;
        slotRecord = slotRecordByIndex(entry->key);
        memset(slotRecord->codeBits, 0, 0xC);
        memset(slotRecord->flagBits, 0, 0x10);
        entryOwner.stats = entry;
        slotOwner.stats = battleSlotGet(entry->linkedSlots[0]);
        if (entry->key == specialSlotKey) {
            for (statusKey = 1; statusKey <= SLOT_CODE_COUNT; statusKey++) {
                if (slotCodeTest(&slotOwner, statusKey) != 0 && (battleLookupGet(statusKey)->flags & 0x4000) != 0) {
                    slotCodeSet(&entryOwner, statusKey);
                }
            }
        }
        for (statusKey = 1; statusKey < 0x41; statusKey++) {
            if (slotFlagTest(&slotOwner, statusKey) != 0 && (statusRecordGet(statusKey)->slotFlags & 0x8000) != 0
                && (statusRecordGet(statusKey)->slotFlags & 0x2000) != 0) {
                slotFlagSet(&entryOwner, statusKey);
            }
        }
        effectCodes = (signed char *) out + 4;
        for (markIndex = 0; markIndex < 4;) {
            int effectOffset = 0x30 + markIndex;

            markIndex++;
            statusKey = ((signed char *) slotOwner.stats)[effectOffset + 4];
            if (statusRecordGet(statusKey)->slotFlags & 0x8000) {
                effectCodes[effectOffset] = statusKey;
            }
        }
    }
}

StatBlock *battleSlotGet(int index)
{
    if (index >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    return &battleSlotTable[index - 1];
}

/* Commits the battleEntryPoolA entry found for each key from 1 to 15 through battleSlotCommit. */
void battleFlushActiveEntries(void)
{
    StatOwner owner;
    StatBlock *entry;
    int key;

    for (key = 1; key < BATTLE_SLOT_KEY_LIMIT; key++) {
        entry = battleEntryFindByKey(key);
        if (entry != 0) {
            owner.stats = entry;
            battleSlotCommit(&owner);
        }
    }
}

void battleSlotCommit(StatOwner *owner)
{
    StatBlock *block = owner->stats;
    StatBlock *slot = battleSlotGet(block->key);

    slot->hp = block->hp;
    slot->ep = block->ep;
}

/* The resident image's gp value; the battle flag copy is the word 0x5174 bytes under it (0x0069DC7C). */
extern int D_6A2DF0[];

/* The first 0x5C bytes of a StatBlock, copied whole from the unit's record. */
typedef struct StatBlockHead StatBlockHead;

struct StatBlockHead {
    unsigned char unmodeled_00[0x2];
    UnalignedS32 maxHp;
    short maxEp;
    unsigned char unmodeled_08[0x5C - 0x8];
};

void battleSlotsInit(void)
{
    int slotIndex;

    memset(slotRecordTable, 0, 0x400);
    D_6A2DF0[-5213] = 0;
    for (slotIndex = 1; slotIndex < BATTLE_SLOT_KEY_LIMIT; slotIndex++) {
        StatBlock *slot = battleSlotGet(slotIndex);
        SlotRecord *record = slotRecordByIndex(slotIndex);
        GrowthRecord *growthRow;
        ShortPair *pairRow;
        unsigned short *secondWord;
        int valueIndex;

        battleEntryLoad(slot, slotIndex);
        record->expCounter = unitExpTblGet()[slot->expLevel].exp;
        growthRow = growthRecordGet(slotIndex);
        for (valueIndex = 0; valueIndex < 8; valueIndex++) {
            record->growth[valueIndex] = growthRow->start[valueIndex];
        }
        pairRow = slotPairRowGet(slotIndex);
        for (valueIndex = 0; valueIndex < 3; valueIndex++) {
            slot->pairFirst[valueIndex] = *(unsigned short *)((char *)pairRow + (valueIndex << 2));
            secondWord = &pairRow->second;
            slot->pairSecond[valueIndex] = *(unsigned short *)((char *)secondWord + (valueIndex << 2));
        }
        switch (slotIndex) {
        case 11:
            battleSlotSetShortLate(11, 0, 2);
            break;
        case 12:
            battleSlotSetShortLate(12, 0, 6);
            break;
        case 13:
            battleSlotSetShortLate(13, 0, 7);
            break;
        }
        if (slotIndex == 2) {
            record->markBits[0] = 7;
        }
    }
    slotPairSet(1);
}

void battleEntryLoad(StatBlock *entry, int key)
{
    memset(entry, 0, sizeof(StatBlock));
    *(StatBlockHead *)entry = *(StatBlockHead *)batUnitRecordGet(key);
    entry->hp = entry->maxHp;
    entry->ep = entry->maxEp;
    entry->key = key;
}

void func_00A8B2C8(void)
{
}

/*
 * Activates the entry of owner for key. A key below 16 exports its entry to the slot
 * battleSlotGet selects; keys 16 to 140 load it; the rest export through the state's
 * slot index, clear three halfwords and record the key. The shared tail sets the
 * entry's unit values and tail-calls unitStatsRefresh.
 */
void battleEntryActivate(StatOwner *owner, int key)
{
    StatBlock *entry;
    struct BattleState *battleState;
    int unitValue;
    int pairIndex;

    entry = owner->stats;
    if (key < BATTLE_SLOT_KEY_LIMIT) {
        battleSlotExport(battleSlotGet(key), entry);
    } else if (key >= BATTLE_POOL_A_KEY_MIN) {
        battleState = battleStateGet();
        battleSlotExport(battleSlotGet(battleState->slotIndex), entry);
        for (pairIndex = 2; pairIndex >= 0; pairIndex--) {
            entry->pairFirst[pairIndex] = 0;
        }
        entry->key = key;
    } else {
        battleEntryLoad(entry, key);
    }
    unitValue = unitValueCalc(owner);
    entry->level = 0;
    entry->unitValue = unitValue;
    entry->unitValueSaved = unitValue;
    entry->effectFlag72 = 3;
    entry->rollPercent = 0;
    entry->current = 0;
    entry->currentMax = 200;
    unitStatsRefresh(owner);
}


/*
 * Binds slot `key` to the battle block of the unit selected by contextIndex
 * (battleAttackResolve passes 1 for its targets, every other caller 0) and returns that unit. The slot is exported into the
 * block, the block becomes the unit's stats, and the slot's hp and ep are then
 * clamped to the block's hp cap and ep cap.
 */
StatOwner *battleContextBind(int key, int contextIndex)
{
    StatOwner *unit;
    StatBlock *block;
    StatBlock *slot;

    battleSlotExport(battleSlotGet(key), &battleStateTable[contextIndex]);
    battleContextTable[contextIndex].stats = &battleStateTable[contextIndex];
    unitStatsRefresh(&battleContextTable[contextIndex]);
    unit = &battleContextTable[contextIndex];
    slot = battleSlotGet(key);
    block = unit->stats;
    if (block->maxHp < slot->hp) {
        slot->hp = block->maxHp;
    }
    if (block->maxEp < slot->ep) {
        slot->ep = block->maxEp;
    }
    return unit;
}

StatBlock *battleContextStateGet(int key)
{
    return battleContextBind(key, 0)->stats;
}

void battleContextApplyLookup(int key, int lookupKey)
{
    StatOwner *unit = battleContextBind(key, 0);
    BattleLookupEntry *entry = battleLookupGet(lookupKey);

    calcEpCost(unit, entry->tecIndex);
}

/*
 * battleAttackResolve: binds the attack's lookup entries to their contexts and runs each
 * target through calcHitRecordFill and calcHpDmg. The targets are the words battleMaskWordsStat6
 * fills when the flags byte has bit 8, else the one `targetKey`. Every target's slot is
 * committed, the lookup value adjusts the owner's EP, and the slot takes the owner's EP.
 */
int battleAttackResolve(int attackerKey, int targetKey, int lookupKey)
{
    int words[12];
    struct AttackHit attack;
    StatOwner *owner;
    BattleLookupEntry *lookup;
    struct TecRecord *technique;
    int count;
    int wordIndex;

    owner = battleContextBind(attackerKey, 0);
    lookup = battleLookupGet(lookupKey);
    attack.command.kind = 3;
    attack.command.actionIdx = lookup->tecIndex;
    attack.command.attacker = owner;
    attack.command.id = lookupKey;
    lookup = battleLookupGet(lookupKey);
    technique = tecRecordGet(lookup->tecIndex);
    if (technique->flags & 8) {
        count = battleMaskWordsStat6(words);
    } else {
        words[0] = targetKey;
        count = 1;
    }
    for (wordIndex = 0; wordIndex < count; wordIndex++) {
        StatOwner *unit = battleContextBind(words[wordIndex], 1);

        attack.hit.target = unit;
        calcHitRecordFill(&attack.command, &attack.hit);
        calcHpDmg(attack.command.attacker, attack.hit.target, attack.hit.hitMode.word, attack.hit.damage);
        battleSlotCommit(unit);
    }
    lookup = battleLookupGet(lookupKey);
    calcEpAdjust(owner, -calcEpCost(owner, lookup->tecIndex));
    battleSlotGet(attackerKey)->ep = owner->stats->ep;
    return 1;
}

/* battleItemUse: the word loop of battleAttackResolve without the attack record. Binds each target word, applies calcHitApply to it and commits the slot. */
int battleItemUse(int targetKey, int lookupKey)
{
    int words[12];
    struct HitAction *lookup;
    struct TecRecord *technique;
    int count;
    int remaining;
    int *wordPtr;

    lookup = hitActionGet(lookupKey);
    technique = tecRecordGet(lookup->entry);
    if (technique->flags & 8) {
        count = battleMaskWordsStat6(words);
    } else {
        words[0] = targetKey;
        count = 1;
    }
    if (count > 0) {
        remaining = count;
        wordPtr = words;
        do {
            StatOwner *unit;

            remaining--;
            unit = battleContextBind(*wordPtr, 0);
            wordPtr++;
            calcHitApply(0, unit, lookupKey);
            battleSlotCommit(unit);
        } while (remaining != 0);
    }
    dataBoxDecCategory1(lookupKey);
    return 1;
}

/* battleSlotFlagSet: sets flag `flag` (1 to 64) of the slot's flag set. unitLearnMaskSet calls it for each set bit of a mask. */
void battleSlotFlagSet(int key, int flag)
{
    StatOwner *unit = battleContextBind(key, 0);

    slotFlagSet(unit, flag);
}

/* battleSlotFlagTest: true when flag `flag` of the slot's flag set is set. unitLearnMaskGet calls it. */
int battleSlotFlagTest(int key, int flag)
{
    StatOwner *unit = battleContextBind(key, 0);

    return slotFlagTest(unit, flag);
}

/* battleSlotFlagClear: clears flag `flag` of the slot's flag set. unitLearnMaskSet calls it for each clear bit of a mask. */
void battleSlotFlagClear(int key, int flag)
{
    StatOwner *unit = battleContextBind(key, 0);

    slotFlagClear(unit, flag);
}

/* battleUnitHasEffectCode: true when the unit in slot `key` has effect code `code` (unitHasEffectCode).
 * unitFileIdGet calls it with code 0x3F. */
int battleUnitHasEffectCode(int key, int code)
{
    StatOwner *unit = battleContextBind(key, 0);

    return unitHasEffectCode(unit, code);
}

/* battleSlotCodeSet: sets effect code `code` (1 to 59) of the slot. configLearnAllApply calls it for keys 1 to 10 and codes 1 to 59. */
void battleSlotCodeSet(int key, int code)
{
    StatOwner *unit = battleContextBind(key, 0);

    slotCodeSet(unit, code);
}

/* battleSlotCodeTest: calls slotCodeTest on the slot and discards the result. */
void battleSlotCodeTest(int key, int code)
{
    StatOwner *unit = battleContextBind(key, 0);

    slotCodeTest(unit, code);
}

/*
 * Set, test and clear of the 21-entry mark set at +0x3E of the slot record (slotMarkSet sets,
 * slotMarkTest tests, slotMarkClear clears). configLearnAllApply tail-calls battleSlotMarkSet with (2, 4).
 * The test wrapper's result is discarded by its C body.
 */
/* battleSlotMarkSet: sets mark `mark` (1 to 21) of the slot's mark set. */
void battleSlotMarkSet(int key, int mark)
{
    slotMarkSet(battleContextBind(key, 0), mark);
}

/* battleSlotMarkTest: reads mark `mark` (1 to 21) of the slot's mark set. */
void battleSlotMarkTest(int key, int mark)
{
    slotMarkTest(battleContextBind(key, 0), mark);
}

/* battleSlotMarkClear: clears mark `mark` (1 to 21) of the slot's mark set. */
void battleSlotMarkClear(int key, int mark)
{
    slotMarkClear(battleContextBind(key, 0), mark);
}

/* Refreshes the hp and ep of the slots 1 to 15 from their battle blocks, then commits each. */
void battleSlotsRefresh(void)
{
    StatOwner *unit;
    StatBlock *block;
    int key;

    for (key = 1; key < BATTLE_SLOT_KEY_LIMIT; key++) {
        unit = battleContextBind(key, 0);
        block = unit->stats;
        block->hp = block->maxHp;
        block->ep = block->maxEp;
        battleSlotCommit(unit);
    }
}

/* ---- old ov01/tu024 ---- */
int battleSlotSetShort(int index, int slot, int equipId)
{
    StatBlock *entry;

    if (index == 0 || index >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    if (slot >= 4) {
        return 0;
    }
    entry = battleSlotGet(index);
    entry->equipId[slot] = equipId;
    return 1;
}

int battleSlotSetByte(int index, int slot, int effectCode)
{
    StatBlock *entry;

    if (index == 0 || index >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    if (slot >= 4) {
        return 0;
    }
    entry = battleSlotGet(index);
    entry->effectCode[slot] = effectCode;
    return 1;
}

int battleSlotSetShortLate(int index, int pairIndex, int pairedId)
{
    StatBlock *entry;

    if (index < 11 || index >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    if (pairIndex >= 2) {
        return 0;
    }
    entry = battleSlotGet(index);
    entry->linkedSlots[pairIndex] = pairedId;
    return 1;
}

/* ---- old ov01/tu025 ---- */
/* unitPercentAdd: adds amount to the percent at body+0x80 (clamped 0..100) only
 * while flag 0x800 of body+0x1C is set; returns 0 and changes nothing otherwise. */
int unitPercentAdd(StatOwner *unit, int amount)
{
    StatBlock *body;
    int percent;

    body = unit->stats;
    if ((body->flags & STAT_FLAG_PERCENT_GATE) == 0) {
        return 0;
    }
    percent = body->rollPercent;
    percent += amount;
    if (percent < 0) {
        percent = 0;
    } else if (percent > PERCENT_MAX) {
        percent = PERCENT_MAX;
    }
    body->rollPercent = percent;
    return 1;
}

/* Adds amount to the stock of the unit's boost gauge (boostGaugeGet picks the gauge).
 * calcActStockSet calls it with -1 when a boost is spent. */
void boostAddStock(StatOwner *unit, int amount) {
    boostGaugeAddStock(boostGaugeGet(unit), amount);
}

/* Stock step: stock (+1) moves by amount, clamped to 0..maxStock (+0). A gain
 * that fills the gauge holds progress (+2) at 50; a non-positive change clears
 * a progress held at 50. */
int boostGaugeAddStock(struct BoostGauge *gauge, int amount)
{
    int stock;

    stock = gauge->stock + amount;
    if (stock < 0) {
        stock = 0;
    } else if (gauge->maxStock < stock) {
        stock = gauge->maxStock;
    }
    gauge->stock = stock;
    if (amount > 0) {
        if ((signed char)stock >= gauge->maxStock) {
            gauge->progress = BOOST_PROGRESS_PER_STOCK;
        }
    } else if (gauge->progress >= BOOST_PROGRESS_PER_STOCK) {
        gauge->progress = 0;
    }
    return 1;
}

/* Adds amount to the progress of the unit's boost gauge. Same unit note as
 * boostAddStock. Callers: func_00A92C18 and attackHitApply (tu033). */
void boostAddProgress(StatOwner *unit, int amount) {
    boostGaugeAddProgress(boostGaugeGet(unit), amount);
}

/* Progress step: progress (+2) moves by amount, floored at 0. Reaching 50
 * banks one stock through boostGaugeAddStock and subtracts 50; a full gauge pins
 * the progress at 50. Returns 1. */
int boostGaugeAddProgress(struct BoostGauge *gauge, int amount) {
    int nextProgress;

    nextProgress = gauge->progress + amount;
    if (nextProgress < 0) {
        nextProgress = 0;
    } else if (nextProgress >= BOOST_PROGRESS_PER_STOCK) {
        boostGaugeAddStock(gauge, 1);
        nextProgress = nextProgress - BOOST_PROGRESS_PER_STOCK;
        if (gauge->stock >= gauge->maxStock) {
            nextProgress = BOOST_PROGRESS_PER_STOCK;
        }
    }
    gauge->progress = (signed char) nextProgress;
    return 1;
}

int unitLevelAdd(StatOwner *unit, int amount)
{
    StatBlock *body;
    int level;

    body = unit->stats;
    level = body->level;
    level += amount;
    if (level < 0) {
        level = 0;
    } else if (level > UNIT_LEVEL_MAX) {
        level = UNIT_LEVEL_MAX;
    }
    body->level = level;
    return 1;
}

/* Current-value step: body+0x82 moves by amount, floored at 0 and capped at
 * the max stored at body+0x84. Returns 1. */
int unitCurrentAdd(StatOwner *unit, int amount)
{
    StatBlock *body;
    int current;

    body = unit->stats;
    current = body->current;
    current += amount;
    if (current < 0) {
        current = 0;
    } else if (body->currentMax < current) {
        current = body->currentMax;
    }
    body->current = current;
    return 1;
}

/* ---- old ov01/tu026 ---- */
extern unsigned char calcStatDefTurnTable[];
/*
 * The penalty table and the entry masks are one object: calcEntryMasks follows calcStatInflictPenaltyTable
 * at 0x00ac5a14, 4 bytes past an 8-aligned array start, so an array of its own cannot sit there.
 */
struct CalcTables {
    unsigned char inflictPenalty[44];       /* rows of 16 status bits, read by calcStatInflictPenaltyGet */
    int entryMasks[11];                     /* entry masks, read by calcEntryMaskTest (index 1 to 10) */
};

extern struct CalcTables calcTables;

/* calcHpAdjust: HP at +0x5C moves by delta, clamped to [0, max] (max is the unaligned int at +0x2); returns 1 while HP stays above 0. calcHpDmg calls it. */
int calcHpAdjust(StatOwner *unit, int delta)
{
    StatBlock *stats;
    int hp;

    stats = unit->stats;
    hp = stats->hp;
    hp += delta;
    if (hp < 0) {
        hp = 0;
    } else if (stats->maxHp < hp) {
        hp = stats->maxHp;
    }
    stats->hp = hp;
    return hp != 0;
}

/* calcEpAdjust: EP halfword at +0x60 moves by delta, clamped to [0, max] (max is the halfword at +0x6); returns 1. */
int calcEpAdjust(StatOwner *unit, int delta)
{
    StatBlock *block;
    int ep;

    block = unit->stats;
    ep = block->ep;
    ep += delta;
    if (ep < 0) {
        ep = 0;
    } else if (block->maxEp < ep) {
        ep = block->maxEp;
    }
    block->ep = ep;
    return 1;
}

int calcHitRateGet(StatOwner *owner)
{
    int hitRate;

    hitRate = owner->stats->hitRate;
    if (calcStatGet(owner, 2, 4) != 0) {
        hitRate -= 40;
    } else if (calcStatGet(owner, 1, 4) != 0) {
        hitRate -= 80;
    }
    if (hitRate < 0) {
        hitRate = 0;
    }
    return hitRate;
}

int calcEvadeRate(StatOwner *owner)
{
    int rate;

    rate = owner->stats->evadeRate;
    if (calcStatGet(owner, 2, 4) != 0) {
        rate -= 40;
    } else if (calcStatGet(owner, 1, 4) != 0) {
        rate -= 80;
    }
    if (unitHasEffectCode(owner, 28) != 0) {
        if ((owner->flags & 0x4000) != 0) {
            rate *= 2;
        }
    }
    if (rate < 0) {
        rate = 0;
    }
    return rate;
}

/* calcAtkPhy: sums the physical attack halfword at +0x8 with its status-row modifiers. */
int calcAtkPhy(StatOwner *unit)
{
    int base;
    int statusMod;
    int buffMod;

    base = unit->stats->physAttack * 4;
    statusMod = 0;
    buffMod = 0;
    if (calcStatGet(unit, 2, 0x8) != 0) {
        statusMod = -base / 2;
    } else if (calcStatGet(unit, 1, 0x8) != 0) {
        statusMod = -base / 4;
    }
    if (calcStatGet(unit, 4, 0x1000) != 0) {
        buffMod = base / 4;
    }
    if (calcStatGet(unit, 4, 0x800) != 0) {
        buffMod = -base / 4;
    }
    return base + statusMod + buffMod;
}

/* calcAtkMag: as calcAtkPhy for the halfword at +0xC (magic attack); used by calcAttackDamage. */
int calcAtkMag(StatOwner *unit)
{
    int base;
    int statusMod;
    int buffMod;

    base = unit->stats->magAttack * 5;
    statusMod = 0;
    buffMod = 0;
    if (calcStatGet(unit, 2, 0x40) != 0) {
        statusMod = -base / 2;
    } else if (calcStatGet(unit, 1, 0x40) != 0) {
        statusMod = -base / 4;
    }
    if (calcStatGet(unit, 4, 0x400) != 0) {
        buffMod = base / 4;
    }
    if (calcStatGet(unit, 4, 0x200) != 0) {
        buffMod = -base / 4;
    }
    return base + statusMod + buffMod;
}

/* calcDefPhy: as calcAtkPhy for the halfword at +0xA (physical defence) on the defender. */
int calcDefPhy(StatOwner *unit)
{
    int base;
    int statusMod;
    int buffMod;

    base = unit->stats->physDefence * 3;
    statusMod = 0;
    buffMod = 0;
    if (calcStatGet(unit, 2, 0x10) != 0) {
        statusMod = -base / 2;
    } else if (calcStatGet(unit, 1, 0x10) != 0) {
        statusMod = -base / 4;
    }
    if (calcStatGet(unit, 4, 0x1000) != 0) {
        buffMod = -base / 4;
    }
    if (calcStatGet(unit, 4, 0x800) != 0) {
        buffMod = base / 4;
    }
    return base + statusMod + buffMod;
}

/* calcDefMag: as calcAtkPhy for the halfword at +0xE (magic defence) on the defender. */
int calcDefMag(StatOwner *unit)
{
    int base;
    int statusMod;
    int buffMod;

    base = unit->stats->magDefence * 4;
    statusMod = 0;
    buffMod = 0;
    if (calcStatGet(unit, 2, 0x80) != 0) {
        statusMod = -base / 2;
    } else if (calcStatGet(unit, 1, 0x80) != 0) {
        statusMod = -base / 4;
    }
    if (calcStatGet(unit, 4, 0x400) != 0) {
        buffMod = -base / 4;
    }
    if (calcStatGet(unit, 4, 0x200) != 0) {
        buffMod = base / 4;
    }
    return base + statusMod + buffMod;
}

unsigned char calcActionPower(StatOwner *unusedAttacker, int index) {
    struct TecRecord *record;

    record = tecRecordGet(index);
    return record->power;
}

/* Signed byte of the status block at +0x28 for rows 1 and 2 and the status bit's index, minus 20 when calcStatGet(row 1, 0x200) is set, plus 10 when calcStatGet(row 3, 0x20) is set. */
int calcStatResist(StatOwner *owner, int row, int statBit)
{
    int resist;
    int statIdx;
    int offset;

    if (row != 1 && row != 2) {
        return 0;
    }
    statIdx = calcStatBit2Idx(statBit);
    offset = row * 16 + statIdx;
    resist = *(signed char *)(offset + (int)owner->stats + 40);
    if (calcStatGet(owner, 1, 512) != 0) {
        resist -= 20;
    }
    if (calcStatGet(owner, 3, 32) != 0) {
        resist += 10;
    }
    return resist;
}

int calcAttackStatusMask(StatOwner *owner, int statusMask)
{
    int statBits[6] = { 1, 2, 4, 8, 16, 0 };
    int i;

    if ((statusMask & 0x1f) != 0) {
        return statusMask;
    }
    for (i = 0; statBits[i] != 0; i++) {
        if (calcStatGet(owner, 3, statBits[i]) != 0) {
            statusMask |= statBits[i];
        }
    }
    return statusMask;
}

int calcEpCost(StatOwner *owner, int index)
{
    int cost;

    cost = tecRecordGet(index)->epCost;
    if (unitHasEffectCode(owner, 41) != 0) {
        cost = cost / 2;
    }
    if (unitHasEffectCode(owner, 49) != 0) {
        cost *= 2;
    }
    if (calcStatGet(owner, 2, 256) != 0) {
        cost *= 2;
    }
    return cost;
}

/* calcHitRecordFill: fills a hit record for one target (its mode is set by calcHitMode). It returns nothing: its
 * callers' code needs a void result (attackQueue keeps the hit word in $2), and the closing tecRecordGet call,
 * whose result is unused, compiles to the same tail call. */
void calcHitRecordFill(struct AttackCommand *action, struct HitRecord *hit)
{
    struct TecRecord *command;
    int packedMode;

    hit->inflictRow = 0;
    hit->inflictMask = 0;
    hit->cureRow = 0;
    hit->cureMask = 0;
    command = tecRecordGet(action->actionIdx);
    if (command->type == 2) {
        hit->backAttack = 0;
        hit->hitMode.word = calcKindHitMode(action, hit);
        command = tecRecordGet(action->actionIdx);
        hit->damage = calcRestoreValue(action, hit, command->kindBits.kind, 0);
    } else {
        command = tecRecordGet(action->actionIdx);
        if (command->kindBits.kind == 1) {
            hit->backAttack = calcBackAttack((int *)action, (int *)hit);
        } else {
            hit->backAttack = 0;
        }
        packedMode = calcHitMode(action, hit);
        hit->hitMode.word = packedMode;
        if ((packedMode & 0xFF) == 0) {
            hit->damage = 0;
        } else {
            hit->damage = calcAttackDamage(action, hit);
        }
    }
    tecRecordGet(action->actionIdx);
}

/* calcHitRoll: rnd(99) rolls checked against the record's bytes at +0x4 and +0x5 and the target's flags; returns 0, 1 or 2. Returns 0 when the unit's state byte at +0x70 is set. */
int calcHitRoll(struct AttackCommand *action, struct HitRecord *hit)
{
    struct TecRecord *command;
    struct TecRecord *record;
    int hitRate;
    int evade;
    int margin;
    int roll;
    int sum;

    if (hit->target->stats->state70 != 0) {
        return 0;
    }
    if (unitCanAct(hit->target) == 0) {
        return 0;
    }
    if (hit->backAttack == 1) {
        return 0;
    }
    command = tecRecordGet(action->actionIdx);
    hitRate = calcHitRateGet(action->attacker);
    evade = calcEvadeRate(hit->target);
    if (calcStatGet(hit->target, 4, 0x2000) != 0 && (command->flags & 0x20) != 0) {
        hitRate -= 0x50;
        if (hitRate < 0) {
            hitRate = 0;
        }
    }
    record = tecRecordGet(action->actionIdx);
    if ((record->hitFlags & 0x10) != 0) {
        return 0;
    }
    margin = evade - hitRate;
    sum = rnd(99) + margin + 1;
    roll = sum < 0 ? 0 : sum;
    if (roll >= command->rollThreshold && (hit->target->stats->flags & 0x40) == 0) {
        record = tecRecordGet(action->actionIdx);
        if ((record->hitFlags & 0x8) == 0) {
            return 2;
        }
    }
    roll = rnd(99) + margin + 1;
    if (roll < 0) {
        roll = 0;
    }
    if (roll < command->secondRollThreshold || (hit->target->stats->flags & 0x20) != 0) {
        return 0;
    }
    return 1;
}

/* calcHitMode: maps the kind byte (+0x7) of a command record to the packed hit mode that calcHpDmg takes; calls calcHitRoll for attack kinds. */
int calcHitMode(struct AttackCommand *action, struct HitRecord *hit)
{
    struct TecRecord *tec;
    int roll;
    int mode;

    tec = tecRecordGet(action->actionIdx);
    switch (tec->kindBits.kind) {
    case 1:
        roll = calcHitRoll(action, hit);
        mode = (tec->hitFlags & 0x20) ? 3 : 1;
        if (roll == 1) {
            mode |= 0x200;
            break;
        }
        if (roll == 2) {
            mode = 0;
        }
        break;
    case 2:
        mode = (tec->hitFlags & 0x20) ? 4 : 2;
        break;
    case 3:
        mode = 2050;
        break;
    case 4:
        mode = 5;
        break;
    case 5:
        mode = 6;
        break;
    case 6:
        roll = calcHitRoll(action, hit);
        mode = (tec->hitFlags & 0x20) ? 9 : 7;
        if (roll == 1) {
            mode |= 0x200;
            break;
        }
        break;
    case 7:
        roll = calcHitRoll(action, hit);
        mode = (tec->hitFlags & 0x20) ? 10 : 8;
        if (roll == 1) {
            mode |= 0x200;
            break;
        }
        break;
    default:
        mode = 1;
        break;
    }
    return mode;
}

int unitNumFlag4SetGet(int value);
int calcFollowKind(int followKind, struct HitRecord *query);
int calcCritDamage(struct AttackCommand *command, struct HitRecord *hit, int damage);
int calcHitInflict(struct AttackCommand *command, struct HitRecord *hit);
int calcHitCure(struct AttackCommand *command, struct HitRecord *hit);
int calcHitStateStep(struct AttackCommand *command, struct HitRecord *hit);
int calcComboUpdate(struct AttackCommand *command, StatOwner **targetSlot);

/* calcAttackDamage: damage of one attack from the attacker's and defender's stats, the command power and the status rows. */
int calcAttackDamage(struct AttackCommand *action, struct HitRecord *hit)
{
    struct TecRecord *command;
    int factor;
    int statBit;
    int attack;
    int defence;
    int value;
    int battleState;
    int amount;
    int statusMask;
    int slotHitCount;
    int slotValue;
    int damage;
    int modeIdx;
    int resultState;

    damage = 0;
    command = tecRecordGet(action->actionIdx);
    switch (command->kindBits.kind) {
    case 1:
        if (command->type >= 2) {
            break;
        }
        if (command->type < 0) {
            break;
        }
        {
            if (command->type == 0) {
                attack = calcAtkPhy(action->attacker);
                defence = calcDefPhy(hit->target);
            } else {
                attack = calcAtkMag(action->attacker);
                defence = calcDefMag(hit->target);
            }
            value = calcActionPower(action->attacker, action->actionIdx);
            amount = (attack * value) / 20 - defence;
            if (amount < 0) {
                amount = 0;
            } else {
                amount += rnd(amount / 10 + 2);
            }
            battleState = hit->target->stats->state70;
            damage = amount;
            switch (battleState) {
            case 1:
                damage += damage / 2;
                break;
            case 2:
            case 3:
                damage = amount * 2;
                break;
            default:
                break;
            }
            if (unitHasEffectCode(action->attacker, 46) != 0) {
                int flag46;

                flag46 = unitNumFlag4SetGet((action->attacker)->stats->key >= BATTLE_SLOT_KEY_LIMIT);
                damage += (amount * flag46) / 4;
            }
            if (unitHasEffectCode(hit->target, 47) != 0) {
                int flag47;

                flag47 = unitNumFlag4SetGet(hit->target->stats->key >= BATTLE_SLOT_KEY_LIMIT);
                damage -= (amount * flag47) / 4;
            }
            amount = damage;
            statusMask = calcAttackStatusMask(action->attacker, command->actionBits);
            if ((statusMask & calcStatGet(hit->target, 4, 255)) != 0) {
                if (calcStatGet(hit->target, 4, 256) != 0) {
                    damage -= damage / 2;
                } else {
                    damage -= damage / 4;
                }
            }
            if (unitHasEffectCode(action->attacker, 49) != 0) {
                damage += amount;
            }
            if (unitHasEffectCode(hit->target, 63) != 0) {
                damage += amount * (rnd(5) + 10) / 100;
            }
            if (command->type == 1 && actStepCounterValue() == 4) {
                damage += amount / 2;
            }
            amount = damage;
            damage = calcCritDamage(action, hit, amount);
            calcComboUpdate(action, &hit->target);

            factor = hit->target->stats->comboHits;
            if (factor >= 2) {
                damage += amount * (factor * 10 - 10) / 100;
            }
            slotHitCount = 0;
            factor = 20;
            for (statBit = 0; statBit < 8; statBit++) {
                if (((statusMask >> statBit) & 1) != 0) {
                    if (ov01RollPercent(hit->target->stats->slotRoll[statBit]) != 0) {
                        slotValue = hit->target->stats->slotValue[statBit];
                        if (slotValue >= 0) {
                            factor = factor + slotValue - 20;
                            slotHitCount++;
                        } else {
                            slotHitCount++;
                            factor = slotValue;
                            break;
                        }
                    }
                }
            }
            if (slotHitCount > 0) {
                if (factor == 0) {
                    hit->hitMode.word |= 0x2000;
                    damage = 0;
                } else if (factor < 0) {
                    hit->hitMode.word = (hit->hitMode.word & 0xff00) | 0x1002;
                    damage = damage * -factor / 20;
                } else {
                    damage = damage * factor / 20;
                }
            }
            modeIdx = hit->target->stats->flags & 3;
            if ((modeIdx == 0 && (command->modeGuard[0] & 0x2) != 0)
                || (modeIdx == 1 && (command->modeGuard[1] & 0x2) != 0)
                || (modeIdx == 2 && (command->modeGuard[2] & 0x2) != 0)) {
                damage /= 2;
            }
            hit->stepState = calcHitStateStep(action, hit);
            if (((hit->hitMode.word >> 8) & 0xff) == 2) {
                factor = 50;
                if (command->type != 0) {
                    factor = 25;
                }
                factor += calcStatEffectSum(hit->target, 2048, 4096);
                damage -= damage * factor / 100;
            }
            if (unitHasEffectCode(hit->target, 40) != 0) {
                damage = damage * 90 / 100;
            }
        }
        break;
    case 2:
    case 3:
        {
            if ((command->hitFlags & 0x400) != 0) {
                value = hit->target->stats->maxHp;
                amount = value * command->power / 20;
            } else {
                amount = calcAtkMag(action->attacker) / 5 * command->power;
            }
            damage = 0;
            if (calcStatGet(hit->target, 4, 256) != 0) {
                damage = amount / 4;
            }
            if (calcStatGet(hit->target, 3, 256) != 0) {
                calcStatReset(hit->target, 3, 256);
                damage += amount;
            }
            factor = calcStatEffectSum(hit->target, 2048, 0x8000);
            if (factor > 0) {
                damage += amount * factor / 100;
            }
            if (command->type == 1 && actStepCounterValue() == 4) {
                damage += amount / 2;
            }
            damage += amount;
        }
        break;
    case 4:
    case 5:
        break;
    case 6:
        if ((hit->hitMode.word & 0xff) == 7) {
            amount = hit->target->stats->maxHp;
            factor = hit->target->stats->hp;
        } else {
            amount = hit->target->stats->maxEp;
            factor = hit->target->stats->ep;
        }
        value = calcActionPower(action->attacker, action->actionIdx);
        damage = amount * value / 20;
        if (factor < damage) {
            damage = factor;
        }
        if (((hit->hitMode.word >> 8) & 0x2) != 0) {
            damage -= damage / 2;
        }
        hit->stepState = calcHitStateStep(action, hit);
        break;
    case 7:
        if ((hit->hitMode.word & 0xff) == 8) {
            damage = hit->target->stats->maxHp - 1;
        } else {
            damage = hit->target->stats->maxEp - 1;
        }
        if (((hit->hitMode.word >> 8) & 0x2) != 0) {
            damage -= damage / 2;
        }
        hit->stepState = calcHitStateStep(action, hit);
        break;
    default:
        break;
    }
    if (damage < 0) {
        damage = 0;
    }
    calcHitInflict(action, hit);
    calcHitCure(action, hit);
    hit->followResult = calcFollowKind((int)action, hit);
    resultState = hit->stepState;
    if (resultState == 1 && hit->target->stats->state70 == 0) {
        if (ov01RollPercent(calcStatEffectSum(action->attacker, 2048, 8192) + 50) != 0
            && actStepCounterGet()->levelGranted == 0) {
            unitLevelAdd(action->attacker, 1);
            actStepCounterGet()->levelGranted = 1;
        }
    }
    return damage;
}

int calcCritDamage(struct AttackCommand *command, struct HitRecord *hit, int damage)
{
    struct TecRecord *tec;
    int chance;
    int factor;
    int hitFlags;
    int kind;
    int modeIdx;

    chance = 10;
    factor = 3;
    hitFlags = hit->hitMode.word;
    if (((hitFlags >> 8) & 2) == 0) {
        kind = hitFlags & 0xff;
        if (kind == 1) {
            if (actStepCounterValue() == 0) {
                chance = 50;
            }
            modeIdx = hit->target->stats->flags & 3;
            tec = tecRecordGet(command->actionIdx);
            if ((modeIdx == 0 && (tec->modeGuard[0] & 4) != 0) ||
                (modeIdx == 1 && (tec->modeGuard[1] & 4) != 0) ||
                (modeIdx == 2 && (tec->modeGuard[2] & 4) != 0)) {
                chance += 50;
            }
            chance += calcStatEffectSum(command->attacker, 2048, 1024);
            if (hit->backAttack == 1) {
                chance += 50;
            }
            if (ov01RollPercent(chance <= PERCENT_MAX ? chance : PERCENT_MAX) != 0) {
                if (actStepCounterValue() == 0) {
                    if (ov01RollPercent(10) != 0) {
                        factor = 4;
                        actStepCounterGet()->bonusFlag = 1;
                    }
                }
                hit->hitMode.word |= 0x100;
                damage = damage * factor / 2;
            }
            return damage;
        }
    }
    return damage;
}

/*
 * calcHitInflict: the hit record's target may block the hit by its modeGuard byte
 * (chosen by the block's flags); otherwise the record takes the action's row and each bit
 * of the action that the roll passes, where the roll is the action's chance less
 * calcStatResist's value. Returns whether a bit was inflicted.
 */
int calcHitInflict(struct AttackCommand *command, struct HitRecord *hit)
{
    struct TecRecord *action;
    StatBlock *stats;
    unsigned int modeIdx;
    int statBit;
    int inflictRate;

    action = tecRecordGet(command->actionIdx);
    if (action->row == 0) {
        return 0;
    }
    if (hit->hitMode.kind == 0 || hit->hitMode.kind == 6) {
        return 0;
    }
    stats = hit->target->stats;
    modeIdx = stats->flags & 3;
    if ((modeIdx == 0 && (action->modeGuard[0] & 1)) || (modeIdx == 1 && (action->modeGuard[1] & 1)) || (modeIdx == 2 && (action->modeGuard[2] & 1))) {
        return 0;
    }
    hit->inflictRow = 0;
    hit->inflictMask = 0;
    statBit = 1;
    do {
        if ((action->bits & statBit) != 0) {
            inflictRate = action->inflictRate - calcStatResist(hit->target, action->row, statBit);
            if (inflictRate > 0 && ov01RollPercent(inflictRate) != 0) {
                hit->inflictRow = action->row;
                hit->inflictMask |= statBit;
            }
        }
        statBit <<= 1;
    } while (statBit != 0);
    return hit->inflictRow != 0;
}

/* calcHitCure: for each status bit of the action's row, the bit is cleared on the target when it is active there and calcStatInflictRate's chance rolls. */
int calcHitCure(struct AttackCommand *command, struct HitRecord *hit)
{
    struct TecRecord *action;
    int curedBits;
    int statBit;
    int row;

    curedBits = 0;
    action = tecRecordGet(command->actionIdx);
    if (action->kindBits.kind == 5) {
        statBit = 1;
        do {
            if (calcStatGet(hit->target, action->row, statBit) != 0) {
                if (ov01RollPercent(calcStatInflictRate(command, &hit->target, action->row, statBit)) != 0) {
                    curedBits |= statBit;
                }
            }
            statBit <<= 1;
        } while (statBit != 0);
        if (curedBits != 0) {
            row = action->row;
            hit->cureMask = curedBits;
            hit->cureRow = row;
        } else {
            hit->cureRow = 0;
            hit->cureMask = 0;
        }
        return 1;
    }
    hit->cureRow = 0;
    hit->cureMask = 0;
    return 0;
}

/* Packed hit mode of the record's kind byte: bit 0 gives 2, else bit 1 gives 4, else bit 2 gives 6, else bit 3 gives 2 with the 0x800 flag, else 1. The hit record is not read. */
int calcKindHitMode(struct AttackCommand *command, struct HitRecord *hit)
{
    struct TecRecord *record;
    signed char kind;
    int mode;

    record = tecRecordGet(command->actionIdx);
    kind = record->kindBits.kind;
    if ((kind & 1) != 0) {
        mode = 2;
    } else {
        mode = 4;
        if ((kind & 2) == 0) {
            mode = 6;
            if ((kind & 4) == 0) {
                if ((kind & 8) != 0) {
                    mode = 2;
                    mode |= 0x800;
                } else {
                    mode = 1;
                }
            }
        }
    }
    return mode;
}

/* calcRestoreValue: the value of one hit on the HP path (mode bits 0x1 and 0x8) or the EP path (bit 0x2): the action amount scales the HP or EP cap, then the stat row 3 gate, the effect sum and the cap decide it. */
int calcRestoreValue(struct AttackCommand *command, struct HitRecord *hit, int mode, int resetGate)
{
    struct HitAction *action;
    StatOwner *target;
    int amount;
    int value;
    int gateBonus;
    int effectBonus;

    value = 0;
    tecRecordGet(command->actionIdx);
    action = hitActionGet(command->id);
    amount = action->amount;
    if (mode & 0x9) {
        target = hit->target;
        value = (target->stats->maxHp * amount) / 4;
        gateBonus = 0;
        if (calcStatGet(target, 3, 256)) {
            gateBonus = value;
            if (resetGate) {
                calcStatReset(hit->target, 3, 256);
            }
        }
        effectBonus = calcStatEffectSum(hit->target, 0x800, 0x8000);
        if (effectBonus > 0) {
            effectBonus = (hit->target->stats->maxHp * effectBonus) / PERCENT_MAX;
        }
        value += gateBonus + effectBonus;
        if (hit->target->stats->maxHp < value) {
            value = hit->target->stats->maxHp;
        }
    } else if (mode & 0x2) {
        target = hit->target;
        value = (target->stats->maxEp * amount) / 4;
        gateBonus = 0;
        if (calcStatGet(target, 3, 256)) {
            gateBonus = value;
            if (resetGate) {
                calcStatReset(hit->target, 3, 256);
            }
        }
        value += gateBonus;
        if (hit->target->stats->maxEp < value) {
            value = hit->target->stats->maxEp;
        }
    }
    return value;
}

int calcHitStateStep(struct AttackCommand *command, struct HitRecord *hit)
{
    struct TecRecord *element;
    unsigned short unitFlags;
    unsigned short actionBits;
    int state;
    int oldState;
    int blocked;
    unsigned char hitKind;

    state = hit->target->stats->state70;
    blocked = calcHitRowGate(command, hit);
    oldState = (signed char) state;
    unitFlags = hit->target->stats->flags;
    element = tecRecordGet(command->actionIdx);
    actionBits = element->actionBits;
    element = tecRecordGet(command->actionIdx);
    if (element->gateFlag == 0) {
        hitKind = hit->hitMode.kind;
        if (hitKind == 1) {
            switch (state) {
            case 0:
                if (blocked) {
                    state = 1;
                    sdvSpecialBitSet(hit->target->stats->key);
                    hit->hitMode.word &= ~0x200;
                }
                if ((actionBits & 0x600) == 0x600) {
                    state = 3;
                    hit->target->stats->state70 = hitKind;
                } else if ((actionBits & 0x500) == 0x500) {
                    state = 2;
                    hit->target->stats->state70 = hitKind;
                }
                break;
            case 1:
                if ((actionBits & 0x100) != 0) {
                    if ((unitFlags & 0x8) == 0) {
                        state = 2;
                    } else {
                        actStepCounterGet()->stepState = 2;
                    }
                } else if ((actionBits & 0x200) != 0) {
                    if ((unitFlags & 0x4) == 0) {
                        state = 3;
                    } else {
                        actStepCounterGet()->stepState = 3;
                    }
                }
                break;
            case 2:
                state = 2;
                break;
            case 3:
                state = 3;
                break;
            default:
                return oldState;
            }
        }
    }
    return state;
}

/*
 * calcHitRowGate: walks up to four row gates of the attack's element; each live gate
 * takes the row status of unitStateFlagGet (the lowest set bit wins) and passes it to
 * unitEntryKindAdd. Then the live rows clear their entryMark when their entry is off,
 * and the result is 0 when any such row is off.
 */
int calcHitRowGate(struct AttackCommand *command, struct HitRecord *hit)
{
    StatBlock *stats;
    struct TecRecord *element;
    int rowsActive;
    int i;
    int gateCode;
    int stateBits;

    stats = hit->target->stats;
    if (stats->state70 != 0) {
        battleEntryClearReset(hit->target);
    }
    if (stats->liveRows[0] == 0) {
        return 0;
    }
    if (stats->state70 != 0) {
        return 0;
    }
    element = tecRecordGet(command->actionIdx);
    if (element->gateFlag == 0 && (stats->flags & 0x10) != 0 && hit->backAttack == 0) {
        actStepCounterGet()->stepState = 1;
        return 0;
    }
    i = 0;
    while (i < 4) {
        element = tecRecordGet(command->actionIdx);
        gateCode = element->rowGate[i];
        i++;
        if (gateCode == 0) {
            break;
        }
        stateBits = gateCode & unitStateFlagGet(hit->target);
        gateCode = stateBits;
        if ((stateBits & 1) != 0) {
            gateCode = 1;
        } else if ((stateBits & 2) != 0) {
            gateCode = 2;
        } else if ((stateBits & 4) != 0) {
            gateCode = 4;
        }
        unitEntryKindAdd(hit->target, gateCode);
    }
    rowsActive = 1;
    for (i = 0; stats->liveRows[i] != 0 && i < 4; i++) {
        if ((stats->liveRows[i] & stats->entryKind[i]) == 0) {
            stats->entryMark[i] = 0;
            rowsActive = 0;
        } else {
            stats->entryMark[i] = 1;
        }
    }
    return rowsActive;
}

/*
 * unitStateFlagGet: the bit flag of the unit's state byte (+0x70 of
 * the block at +0x144): 1 for state 3, 4 for state 2, else the block's default code.
 * Callers combine the result as a bit mask with 1, 2 and 4.
 */
signed char unitStateFlagGet(StatOwner *owner) {
    StatBlock *stats;

    stats = owner->stats;
    if (stats->state70 == 3) {
        return 1;
    }
    if (stats->state70 == 2) {
        return 4;
    }
    return stats->defaultCode;
}

/* unitEntryKindAdd: appends one kind to the unit's entry table while it holds fewer than 8 entries (called by calcHitRowGate). */
void unitEntryKindAdd(StatOwner *owner, int kind)
{
    StatBlock *block;

    block = owner->stats;
    if (block->entryCount < 8) {
        block->entryKind[block->entryCount++] = kind;
    }
}

/* unitEntriesAllMarked: 0 when the entry count is 0; 1 when it is negative; else 0 at the first entry whose mark byte is 0, otherwise 1. */
int unitEntriesAllMarked(StatOwner *owner)
{
    StatBlock *stats;
    int count;
    int idx;

    stats = owner->stats;
    count = stats->entryCount;
    if (count == 0) {
        return 0;
    }
    idx = 0;
    if (count > 0) {
        do {
            if (stats->entryMark[idx] == 0) {
                return 0;
            }
            idx++;
        } while (idx < stats->entryCount);
    }
    return 1;
}

int calcFollowKind(int followKind, struct HitRecord *query)
{
    int phase;
    int takenBit;

    phase = query->stepState;
    followKind = 0;
    if (phase) {
        switch (phase) {
        case 2:
            followKind = 3;
            break;
        case 1:
            followKind = 2;
            break;
        case 3:
            followKind = 4;
            break;
        }
    } else {
        int flags;

        flags = query->hitMode.word;
        if ((flags & 0xff) == 1) {
            takenBit = (flags >> 8) & 1;
            followKind = takenBit != 0;
        }
    }
    return followKind;
}

int calcBackAttack(int *attackerSlot, int *targetSlot) {
    return unitIsInFacingArc(*attackerSlot, *targetSlot);
}

/* unitIsInFacingArc: true when the bearing from the first unit to the second lies within a quarter turn of the second unit's facing (+0x6C of its block). */
int unitIsInFacingArc(int originAddress, int targetAddress)
{
    StatOwner *origin;
    StatOwner *target;
    Vector4 *from;
    Vector4 *to;
    StatBlock *stats;
    float heading;
    float facing;
    float delta;
    float gap;

    origin = (StatOwner *)originAddress;
    target = (StatOwner *)targetAddress;
    if (origin == target) {
        return 0;
    }
    to = &target->pose.snapshot.vector;
    from = &origin->pose.snapshot.vector;
    heading = atan2f(to->x - from->x, to->z - from->z);
    stats = target->stats;
    facing = stats->facing;
    if (heading < 0.0f) {
        heading += 6.2831855f;
    }
    delta = heading - facing;
    if (delta < 0.0f) {
        delta = -delta;
    }
    if (6.2831855f - delta < delta) {
        delta = 6.2831855f - delta;
    }
    gap = delta - 1.5707964f;
    if (gap < 0.0f) {
        gap = -gap;
        if (0.000001f < gap) {
        } else {
            return 0;
        }
    } else {
        if (0.000001f < gap) {
        } else {
            return 0;
        }
    }
    return delta < 1.5707964f;
}

int unitStateAdvance(StatOwner *owner)
{
    StatBlock *stats;
    signed char state;

    stats = owner->stats;
    state = stats->state70;
    switch (state) {
    case 1:
    case 2:
        stats->state70 = 0;
        return 1;
    case 3:
        stats->state70 = 2;
        return 1;
    default:
        return 0;
    }
}

/* calcComboUpdate: hit counter on the target (+0xF4, capped at 99); restarts when the kind mask (+0xF0) does not overlap. */
int calcComboUpdate(struct AttackCommand *command, StatOwner **targetSlot)
{
    struct TecRecord *element;
    StatBlock *stats;
    int statusMask;

    element = tecRecordGet(command->actionIdx);
    statusMask = calcAttackStatusMask(command->attacker, element->actionBits) & 0x1f;
    stats = (*targetSlot)->stats;
    if (stats->comboHits != 0 && (stats->comboMask & statusMask) != 0) {
        if (stats->comboHits < 99) {
            stats->comboHits = stats->comboHits + 1;
        }
    } else {
        stats->comboMask = statusMask;
        stats->comboHits = 1;
    }
    return 0;
}

int calcHpDmg(StatOwner *attacker, StatOwner *target, int hitMode, int damage)
{
    int result;

    switch (hitMode & 0xff) {
    case 1:
        result = calcHpAdjust(target, -damage);
        break;
    case 3:
        result = calcEpAdjust(target, -damage);
        break;
    case 2:
        result = calcHpAdjust(target, damage);
        break;
    case 4:
        result = calcEpAdjust(target, damage);
        break;
    case 7:
    case 8:
        result = calcHpAdjust(target, -damage);
        calcHpAdjust(attacker, damage);
        break;
    case 9:
    case 10:
        result = calcEpAdjust(target, -damage);
        calcEpAdjust(attacker, damage);
        break;
    case 5:
    case 6:
    default:
        result = 1;
        break;
    }
    return result;
}

/* calcHitApply: applies one filled hit record to its target: HP and EP, status set and clear, flags. */
int calcHitApply(StatOwner *source, StatOwner *target, int lookupKey)
{
    struct AttackHit attack;
    struct HitAction *action;
    struct TecRecord *entry;
    struct BoostGauge *gauge;
    int raised;
    short amount;
    int statRow;
    int statBit;
    int flags;

    action = hitActionGet(lookupKey);
    entry = tecRecordGet(action->entry);
    amount = action->amount;
    attack.command.kind = 4;
    attack.hit.target = target;
    attack.command.attacker = source;
    attack.command.actionIdx = action->entry;
    attack.command.id = lookupKey;

    flags = entry->kindBits.effectFlags;
    if (flags & 0x1) {
        calcHpAdjust(target, calcRestoreValue(&attack.command, &attack.hit, 1, 1));
        flags = entry->kindBits.effectFlags;
    }
    if (flags & 0x2) {
        calcEpAdjust(target, calcRestoreValue(&attack.command, &attack.hit, 2, 1));
        flags = entry->kindBits.effectFlags;
    }
    if (flags & 0x4) {
        statRow = 1;
        if (amount != 0) {
            statRow = 2;
        }
        statBit = 1;
        do {
            if (calcStatGet(attack.hit.target, statRow, statBit) != 0 &&
                ov01RollPercent(calcStatInflictRate(&attack.command, &attack.hit.target, statRow, statBit)) != 0) {
                calcStatReset(target, statRow, statBit);
            }
            statBit *= 2;
        } while (statBit != 0);
        flags = entry->kindBits.effectFlags;
    }
    if (flags & 0x8) {
        calcHpAdjust(target, calcRestoreValue(&attack.command, &attack.hit, 1, 1));
        flags = entry->kindBits.effectFlags;
    }
    if (flags & 0x10) {
        slotRecordGet(target)->gainSecond += amount;
        if (slotRecordGet(target)->gainSecond > SLOT_CAPPED_CAP) {
            slotRecordGet(target)->gainSecond = SLOT_CAPPED_CAP;
        }
        flags = entry->kindBits.effectFlags;
    }
    if (flags & 0x20) {
        slotRecordGet(target)->gainThird += amount;
        if (slotRecordGet(target)->gainThird > SLOT_CAPPED_CAP) {
            slotRecordGet(target)->gainThird = SLOT_CAPPED_CAP;
        }
        flags = entry->kindBits.effectFlags;
    }
    if (flags & 0x40) {
        switch (amount) {
        case 0:
            gauge = boostGaugeGet(target);
            raised = gauge->maxStockCount + 1;
            gauge->maxStockCount = (unsigned char)raised;
            if ((signed char)raised >= 6) {
                gauge->maxStockCount = 5;
            }
            if (gauge->progress >= BOOST_PROGRESS_PER_STOCK && gauge->stock < 5) {
                gauge->progress = 0;
            }
            break;
        case 1:
            battleSlotsRefresh();
            break;
        case 2:
            calcHpAdjust(target, -calcRestoreValue(&attack.command, &attack.hit, 0x40, 1));
            break;
        }
    }
    return 1;
}

/* calcStatusHitRoll: 1 when the target can be hit by the status roll: neither unit has flag 0x4, the record at index is neither flagged 0x8 nor in state, the target's status block has flag 0x800 and the target has 0x4000, and ov01RollPercent of the block's percent succeeds. */
int calcStatusHitRoll(StatOwner *source, StatOwner *target, int actionIdx)
{
    if (!(source != 0 && !(source->flags & UNIT_FLAG_DOWN) && target != 0 && !(target->flags & UNIT_FLAG_DOWN)
          && !(tecRecordGet(actionIdx)->flags & 8)
          && tecRecordGet(actionIdx)->code == 0)) {
        return 0;
    }
    if (!(target->stats->flags & STAT_FLAG_PERCENT_GATE)) {
        return 0;
    }
    if (tecRecordGet(actionIdx)->hitFlags & 4) {
        return 0;
    }
    if (!(target->flags & 0x4000)) {
        return 0;
    }
    return ov01RollPercent(target->stats->rollPercent) != 0;
}

int calcStatGet(StatOwner *owner, int row, int mask)
{
    StatBlock *stats;
    int value;

    if (row == 0) {
        return 0;
    }
    stats = owner->stats;
    value = (&stats->activeMask[-1])[row] & mask;
    if (row >= 3 && row <= 4) {
        value |= stats->altMask[row] & mask;
    }
    return value;
}

int calcStatSet(StatOwner *unit, int row, int statBit)
{
    int bitIdx;
    int turns;
    int effectSum;

    if (row == 0) {
        return 0;
    }
    if (row >= 1 && row <= 2) {
        if (row == 1) {
            if ((unit->stats->activeMask[0] & statBit) && !(statBit & 0x600)) {
                if (ov01RollPercent(20) != 0) {
                    calcStatReset(unit, 1, statBit);
                    row = 2;
                }
            } else if ((unit->stats->activeMask[row] & statBit) && !(statBit & 0x600)) {
                return 0;
            }
        } else if (row == 2) {
            if ((unit->stats->activeMask[0] & statBit) && !(statBit & 0x1E00)) {
                calcStatReset(unit, 1, statBit);
            }
        }
    }
    bitIdx = calcStatBit2Idx(statBit);
    turns = calcStatDefTurnGet(row, statBit);
    effectSum = calcStatEffectSum(unit, 0x800, 0x800);
    if (effectSum > 0) {
        turns *= effectSum;
    }
    unit->stats->activeMask[row - 1] |= statBit;
    unit->stats->turnCount[row - 1][bitIdx] = turns;
    switch (row) {
    case 1:
        if (statBit & 0x1) {
            calcStatReset(unit, 3, 0x40);
            calcStatReset(unit, 3, 0x80);
        }
        if (statBit & 0x200) {
            calcStatReset(unit, 3, 0x20);
        }
        break;
    case 3:
        if (statBit & 0x40) {
            calcStatReset(unit, 1, 0x1);
            calcStatReset(unit, 3, 0x80);
        }
        if (statBit & 0x80) {
            calcStatReset(unit, 1, 0x1);
            calcStatReset(unit, 3, 0x40);
        }
        if (statBit & 0x20) {
            calcStatReset(unit, 1, 0x200);
        }
        break;
    case 4:
        if (statBit & 0x800) {
            calcStatReset(unit, 4, 0x1000);
        }
        if (statBit & 0x1000) {
            calcStatReset(unit, 4, 0x800);
        }
        if (statBit & 0x200) {
            calcStatReset(unit, 4, 0x400);
        }
        if (statBit & 0x400) {
            calcStatReset(unit, 4, 0x200);
        }
        break;
    }
    return 1;
}

/* calcStatAltSet: sets mask bits in altMask[statIdx] for statIdx 3 and 4. */
int calcStatAltSet(StatOwner *owner, int statIdx, int mask)
{
    if (statIdx >= 3 && statIdx <= 4) {
        owner->stats->altMask[statIdx] |= mask;
    }
    return 1;
}

int calcStatReset(StatOwner *owner, int row, int mask)
{
    StatBlock *stats;
    int bitIdx;

    if (row == 0) {
        return 0;
    }
    bitIdx = calcStatBit2Idx(mask);
    stats = owner->stats;
    stats->activeMask[row - 1] &= ~mask;
    stats->turnCount[row - 1][bitIdx] = 0;
    return 1;
}

/* calcStatAltReset: clears mask bits in altMask[statIdx] for statIdx 3 and 4. */
int calcStatAltReset(StatOwner *owner, int statIdx, int mask)
{
    if (statIdx >= 3 && statIdx <= 4) {
        owner->stats->altMask[statIdx] &= ~mask;
    }
    return 1;
}

/* calcStatResetCha: clears every row's activeMask and turn counts. */
void calcStatResetCha(StatOwner *owner)
{
    int row;
    int col;

    row = 1;
    do {
        owner->stats->activeMask[row - 1] = 0;
        col = 0;
        do {
            owner->stats->turnCount[row - 1][col] = 0;
            col++;
        } while (col < 16);
        row++;
    } while (row < 5);
}

/* calcStatTurn: each turn, counts down the turn counts of the active statuses and clears those that run out. */
void calcStatTurn(StatOwner *owner)
{
    int row;
    int col;
    StatBlock *stats;
    int nextRow;
    int count;

    stats = owner->stats;
    for (row = 0; row < 4; row++) {
        col = 0;
        nextRow = row + 1;
        do {
            if ((stats->activeMask[row] & calcStatIdx2Bit(col)) != 0) {
                count = (signed char)stats->turnCount[row][col];
                if (count >= 0) {
                    count = count - 1;
                    if (count <= 0) {
                        calcStatReset(owner, nextRow, calcStatIdx2Bit(col));
                        count = 0;
                    }
                    stats->turnCount[row][col] = count;
                }
            }
            col++;
        } while (col < 16);
    }
}

/* calcStatDefTurnGet: default turn count of status statBit in row, from calcStatDefTurnTable (row 1 to 4 only). */
signed char calcStatDefTurnGet(int row, int statBit)
{
    int bitIdx;

    if (row == 0 || row >= 5) {
        return 0;
    }
    bitIdx = calcStatBit2Idx(statBit);
    return (signed char)calcStatDefTurnTable[(row * 16) + bitIdx];
}

/*
 * The inverse of calcStatIdx2Bit below: the index of statBit's lowest set bit
 * among the 16 status bits calcStatSet, calcStatReset and calcStatDefTurnGet
 * index with it, or 16 when statBit holds none of them.
 */
int calcStatBit2Idx(int statBit)
{
    int statIdx;
    int mask;

    mask = 1;
    for (statIdx = 0; statIdx < 16; statIdx++) {
        if (statBit & mask) {
            break;
        }
        mask <<= 1;
    }
    return statIdx;
}

int calcStatIdx2Bit(int statIdx)
{
    return 1 << statIdx;
}

/*
 * calcStatInflictPenaltyGet: signed byte of calcStatInflictPenaltyTable
 * (calcStatInflictPenaltyTable) for rows 1 and 2 and the status bit's index. calcStatInflictRate subtracts
 * it from the chance when calcStatResist returns 0.
 */
signed char calcStatInflictPenaltyGet(int row, int statBit)
{
    int bitIdx;

    if (row == 0 || row >= 3) {
        return 0;
    }
    bitIdx = calcStatBit2Idx(statBit);
    return (signed char)calcTables.inflictPenalty[(row * 16) + bitIdx];
}

int calcStatInflictRate(struct AttackCommand *command, StatOwner **targetSlot, int row, int statBit)
{
    int base;
    int rate;
    int penalty;

    if (command->kind == 4) {
        base = 200;
    } else {
        base = command->attacker->stats->inflictBase;
    }
    rate = base;
    rate += calcStatEffectSum(*targetSlot, 0x800, 0x4000, base);
    if (calcStatResist(*targetSlot, row, statBit) == 0) {
        penalty = calcStatInflictPenaltyGet(row, statBit);
    } else {
        penalty = 0;
    }
    rate -= penalty;
    if (rate < 0) {
        rate = 0;
    }
    if (rate > PERCENT_MAX) {
        return PERCENT_MAX;
    }
    return rate;
}

/* unitStatsRefresh: refresh the unit's bonuses from its equipment, its effect codes and the slot table, then set its fixed stat values. Always returns 1. */
int unitStatsRefresh(StatOwner *unit)
{
    struct ItemRecord *equipItem;
    struct StatusRecord *statusRecord;
    int i;
    int code;

    for (i = 0; i < 4; i++) {
        if (unit->stats->equipId[i] != 0) {
            equipItem = itemRecordGet(unit->stats->equipId[i]);
            code = equipItem->code;
            if (code != 0) {
                unitApplyStatBonus(unit, code);
            }
        }
    }
    for (i = 0; i < 4; i++) {
        code = unit->stats->effectCode[i];
        if (code != 0) {
            unitApplyStatBonus(unit, code);
        }
    }
    for (i = 1; i < 0x80; i++) {
        if (slotFlagTest(unit, i) != 0) {
            statusRecord = statusRecordGet(i);
            if ((statusRecord->slotFlags & 0x2000) != 0) {
                unitApplyStatBonus(unit, i);
            }
        }
    }
    if (unitHasEffectCode(unit, 0xF) != 0) {
        unit->stats->effectFlag72 = 4;
    }
    if (unitHasEffectCode(unit, 0x32) != 0) {
        unit->stats->level = 1;
    }
    if (unitHasEffectCode(unit, 0x3F) != 0) {
        unit->stats->counter12 += 1;
        if ((signed char)unit->stats->counter12 >= 0x10) {
            unit->stats->counter12 = 0xF;
        }
    }
    if (unitHasEffectCode(unit, 0x63) != 0) {
        unit->stats->current = 100;
    }
    if (unitHasEffectCode(unit, 0x64) != 0) {
        unit->stats->currentMax = 300;
    }
    return 1;
}

/* unitApplyStatBonus: a bonus record flagged 0x8000 adds its value, scaled by calcStatEffectSum, to the unit's halfword at +0x8, capped at 999. Other branches not evidenced here. */
void unitApplyStatBonus(StatOwner *unit, int recordIdx)
{
    StatusRecord *record;
    StatBlock *stats;
    int value;
    int scale;
    int statIdx;
    int cellOffset;
    int cellsLeft;
    int rowBase;
    int cellMax;
    unsigned char *statCells;
    unsigned char *statCapCells;
    unsigned char *loopStatCells;
    unsigned char *loopStatCapCells;

    record = statusRecordGet(recordIdx);
    stats = unit->stats;
    if (record->flags & 0x8000) {
        value = record->code & 0x7F;
        scale = calcStatEffectSum(unit, 0x8000, 0x80);
        if (scale > 0) {
            value *= scale;
        }
        if (record->code & 0x8000) {
            stats->physAttack += value;
            if (stats->physAttack >= 1000) {
                stats->physAttack = STAT_CAP_PHYS;
            }
        }
        if (record->code & 0x4000) {
            stats->physDefence += value;
            if (stats->physDefence >= 1000) {
                stats->physDefence = STAT_CAP_PHYS;
            }
        }
        if (record->code & 0x2000) {
            stats->hitRate += value;
            if (stats->hitRate >= 101) {
                stats->hitRate = PERCENT_MAX;
            }
        }
        if (record->code & 0x1000) {
            stats->evadeRate += value;
            if (stats->evadeRate >= 101) {
                stats->evadeRate = PERCENT_MAX;
            }
        }
        if (record->code & 0x800) {
            stats->magAttack += value;
            if (stats->magAttack >= 1000) {
                stats->magAttack = STAT_CAP_PHYS;
            }
        }
        if (record->code & 0x400) {
            stats->magDefence += value;
            if (stats->magDefence >= 1000) {
                stats->magDefence = STAT_CAP_PHYS;
            }
        }
        if (record->code & 0x200) {
            stats->counter12 += value;
            if ((signed char)stats->counter12 >= 16) {
                stats->counter12 = 15;
            }
        }
        if (record->code & 0x100) {
            stats->maxHp += stats->maxHp * value / PERCENT_MAX;
            if ((unsigned short)unit->stats->key - 11 < 5U) {
                if (stats->maxHp > 99999) {
                    stats->maxHp = 99999;
                }
            } else if (stats->maxHp >= 10000) {
                stats->maxHp = 9999;
            }
            stats->maxEp += stats->maxEp * value / PERCENT_MAX;
            if (stats->maxEp >= 100) {
                stats->maxEp = 99;
            }
        }
    }
    if (record->flags & 0x4000) {
        calcStatAltSet(unit, record->flags & 0xF, record->code);
    }
    if (record->flags & 0x2000) {
        scale = ((record->flags & 0xF0) >> 4) * 5;
        value = record->flags & 0xF;
        if (value != 15) {
            statIdx = calcStatBit2Idx(record->code);
            statCells = (unsigned char *)unit->stats + 8;
            cellOffset = value * 16;
            cellOffset += statIdx;
            cellOffset += 32;
            statCells[cellOffset] += scale;
            statCapCells = (unsigned char *)unit->stats + 8;
            if ((signed char)statCapCells[cellOffset] >= 101) {
                statCapCells[cellOffset] = PERCENT_MAX;
            }
        } else {
            value = 1;
            cellMax = PERCENT_MAX;
            rowBase = 16;
            for (; value < 3; value++) {
                cellOffset = rowBase + 32;
                cellsLeft = 15;
                do {
                    loopStatCells = (unsigned char *)unit->stats + 8;
                    loopStatCells[cellOffset] += scale;
                    loopStatCapCells = (unsigned char *)unit->stats + 8;
                    if ((signed char)loopStatCapCells[cellOffset] >= 101) {
                        loopStatCapCells[cellOffset] = cellMax;
                    }
                    cellOffset++;
                } while (--cellsLeft >= 0);
                rowBase += 16;
            }
        }
    }
}

/* calcStatEffectSum: sums calcStatEffectValue over the four equipment ids at +0x86 of the unit block (each item's halfword at +0xA is the code). */
int calcStatEffectSum(StatOwner *unit, int flagMask, int codeMask)
{
    struct ItemRecord *item;
    struct StatusRecord *record;
    int sum;
    int code;
    int i;

    sum = 0;
    for (i = 0; i < 4; i++) {
        if (unit->stats->equipId[i] != 0) {
            item = itemRecordGet(unit->stats->equipId[i]);
            code = item->code;
            if (code != 0) {
                sum += calcStatEffectValue(unit, code, flagMask, codeMask);
            }
        }
    }
    for (i = 0; i < 4; i++) {
        code = unit->stats->effectCode[i];
        if (code != 0) {
            sum += calcStatEffectValue(unit, code, flagMask, codeMask);
        }
    }
    for (i = 1; i < 128; i++) {
        if (slotFlagTest(unit, i) != 0) {
            record = statusRecordGet(i);
            if ((record->slotFlags & 0x2000) != 0) {
                sum += calcStatEffectValue(unit, i, flagMask, codeMask);
            }
        }
    }
    return sum;
}

/* calcStatEffectValue: low 7 bits of a record's code when its flags and code pass the masks, else 0. */
int calcStatEffectValue(StatOwner *unusedUnit, int recordIdx, int flagMask, int codeMask)
{
    StatusRecord *record;
    int code;

    record = statusRecordGet(recordIdx);
    if ((record->flags & flagMask) != 0) {
        code = record->code;
        if ((code & codeMask) != 0) {
            return code & 0x7F;
        }
    }
    return 0;
}

/* unitHasEquipId: true when the value is one of the four equipment ids (halfwords at +0x86). */
int unitHasEquipId(StatOwner *owner, int id)
{
    StatBlock *block;
    int idx;

    block = owner->stats;
    for (idx = 0; idx < 4; idx++) {
        if (id == block->equipId[idx]) {
            return 1;
        }
    }
    return 0;
}

int unitHasEffectCode(StatOwner *unit, int effectCode)
{
    StatusRecord *record;
    ItemRecord *item;
    int i;

    record = statusRecordGet(effectCode);
    if ((record->slotFlags & 0x2000) != 0) {
        if (slotFlagTest(unit, effectCode) != 0) {
            return 1;
        }
    } else {
        for (i = 0; i < 4; i++) {
            if (unit->stats->equipId[i] != 0) {
                item = itemRecordGet(unit->stats->equipId[i]);
                if (effectCode == item->code) {
                    return 1;
                }
            }
        }
        for (i = 0; i < 4; i++) {
            if (effectCode == unit->stats->effectCode[i]) {
                return 1;
            }
        }
    }
    return 0;
}

/* partyHasEffectCode: true when a unit of side 0 (the party, unitTblGet) has effect code effectCode. menuTgtStatusDraw calls it with 0x10. */
int partyHasEffectCode(int effectCode)
{
    StatOwner **entries;
    StatOwner *entry;
    int count;
    int idx;

    idx = 0;
    count = unitTblGet(0, &entries);
    if (count > 0) {
        do {
            entry = entries[idx];
            idx++;
            if (entry != 0 && unitHasEffectCode(entry, effectCode) != 0) {
                return 1;
            }
        } while (idx < count);
    }
    return 0;
}

/* ---- old ov01/tu027 ---- */
/* Record for the owner's slot index, or 0 when the index is 16 or more. */
SlotRecord *slotRecordGet(StatOwner *owner)
{
    int slot;

    slot = owner->stats->key;
    if (slot >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    return (SlotRecord *) (slotRecordTable - 0x40 + (slot << 6));
}

/* Record for the slot index given, or 0 when the index is 16 or more. */
SlotRecord *slotRecordByIndex(int index) {
    if (index >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    return (SlotRecord *) ((index << 6) + (slotRecordTable - 0x40));
}

int slotFlagSet(StatOwner *owner, int bit)
{
    SlotRecord *record;

    if ((unsigned int)(bit - 1) >= 64) {
        return 0;
    }
    bit = bit - 1;
    record = slotRecordGet(owner);
    record->flagBits[bit / 8] |= 1 << (bit % 8);
    return 1;
}

/* Tests flag bit 'bit' (1-based, 1..104) of the owner's slot record; returns 1 when set, 0 when clear, out of range or the slot is 16 or more. */
int slotFlagTest(StatOwner *owner, int bit)
{
    SlotRecord *record;

    if ((unsigned int)(bit - 1) >= 104) {
        return 0;
    }
    if (owner->stats->key >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    record = slotRecordGet(owner);
    bit = bit - 1;
    return ((unsigned int)record->flagBitsSigned[bit / 8] >> (bit % 8)) & 1;
}

/* Clears flag bit 'bit' (1-based, 1..64) in the owner's slot record; returns 1 when the index is in range, 0 otherwise. */
int slotFlagClear(StatOwner *owner, int bit)
{
    SlotRecord *record;

    if ((unsigned int)(bit - 1) >= 64) {
        return 0;
    }
    bit = bit - 1;
    record = slotRecordGet(owner);
    record->flagBits[bit / 8] &= ~(1 << (bit % 8));
    return 1;
}

/* Sets code bit 'bit' (1-based, 1..59) in the owner's slot record; returns 1 when the index is in range, 0 otherwise. */
int slotCodeSet(StatOwner *owner, int bit)
{
    SlotRecord *record;

    if ((unsigned int)(bit - 1) >= SLOT_CODE_COUNT) {
        return 0;
    }
    bit = bit - 1;
    record = slotRecordGet(owner);
    record->codeBits[bit / 8] |= 1 << (bit % 8);
    return 1;
}

/* Tests code bit 'bit' (1-based, 1..59) of the owner's slot record; returns 1 when set, 0 when clear, out of range or the slot is 16 or more. */
int slotCodeTest(StatOwner *owner, int bit)
{
    SlotRecord *record;

    if ((unsigned int)(bit - 1) >= SLOT_CODE_COUNT) {
        return 0;
    }
    if (owner->stats->key >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    record = slotRecordGet(owner);
    bit = bit - 1;
    return ((unsigned int)record->codeBitsSigned[bit / 8] >> (bit % 8)) & 1;
}

/* Sets pair bit 'index' (1-based, 1..16) in both slot records named by the link record for it; returns 1 when in range, 0 otherwise. The parameter becomes the 0-based bit index after the link lookup. */
int slotPairSet(int index)
{
    PairLink *link;
    SlotRecord *firstRecord;
    SlotRecord *secondRecord;
    int mask;

    if ((unsigned int)(index - 1) >= SLOT_PAIR_COUNT) {
        return 0;
    }
    link = pairLinkGet(index);
    index = index - 1;
    mask = 1;
    firstRecord = slotRecordByIndex(link->slotFirst);
    firstRecord->pairBits[index / 8] |= mask << (index % 8);
    secondRecord = slotRecordByIndex(link->slotSecond);
    secondRecord->pairBits[index / 8] |= mask << (index % 8);
    return 1;
}

/* Reads pair bit 'index' (1-based, 1..16) of the slot record linked from the given index; returns 0 when the index is out of range. The parameter becomes the 0-based bit index after the link lookup. */
int slotPairTest(int index)
{
    PairLink *link;
    SlotRecord *record;

    if ((unsigned int)(index - 1) >= SLOT_PAIR_COUNT) {
        return 0;
    }
    link = pairLinkGet(index);
    index = index - 1;
    record = slotRecordByIndex(link->slotFirst);
    return ((unsigned int)record->pairBitsSigned[index / 8] >> (index % 8)) & 1;
}

/* Sets mark bit 'bit' (1-based, 1..21) of the owner's slot record; returns 1 when the index is in range, 0 otherwise. */
int slotMarkSet(StatOwner *owner, int bit)
{
    SlotRecord *record;

    if ((unsigned int)(bit - 1) >= SLOT_MARK_COUNT) {
        return 0;
    }
    bit = bit - 1;
    record = slotRecordGet(owner);
    record->markBits[bit / 8] |= 1 << (bit % 8);
    return 1;
}

/* Tests mark bit 'bit' (1-based, 1..21) of the owner's slot record; returns 1 when set, 0 when clear, out of range or the slot is 16 or more. */
int slotMarkTest(StatOwner *owner, int bit)
{
    SlotRecord *record;

    if ((unsigned int)(bit - 1) >= SLOT_MARK_COUNT) {
        return 0;
    }
    if (owner->stats->key >= BATTLE_SLOT_KEY_LIMIT) {
        return 0;
    }
    record = slotRecordGet(owner);
    bit = bit - 1;
    return ((unsigned int)record->markBitsSigned[bit / 8] >> (bit % 8)) & 1;
}

/* Clears mark bit 'bit' (1-based, 1..21) of the owner's slot record; returns 1 when the index is in range, 0 otherwise. */
int slotMarkClear(StatOwner *owner, int bit)
{
    SlotRecord *record;

    if ((unsigned int)(bit - 1) >= SLOT_MARK_COUNT) {
        return 0;
    }
    bit = bit - 1;
    record = slotRecordGet(owner);
    record->markBits[bit / 8] &= ~(1 << (bit % 8));
    return 1;
}

/* ---- old ov01/tu028 ---- */

/* Clears the battle result block. */
void battleResultReset(void) {
    memset(&battleResult, 0, 0x50);
}

/* Returns the battle result block. */
struct BattleResult *battleResultGet(void) {
    return &battleResult;
}

/*
 * Adds the battle result of one unit: the table key record gives the points and
 * two (category, id) pairs; each pair is taken on a percent roll and stored in
 * the next free battle result slot.
 */
void battleResultAdd(StatOwner *unit) {
    int rate;
    struct TableKeyRecord *keyRecord;
    struct BattleResult *result;
    StatOwner *source;
    int category;
    int id;

    category = 0;
    id = 0;
    source = unit->stats->source;
    result = battleResultGet();
    keyRecord = tableKeyRecordGet(unit->stats->key);
    result->points += keyRecord->points;
    if (actStepCounterValue() == 2) {
        rate = actStepCounterGet()->multiplier;
    } else {
        rate = 1;
    }
    result->stepRateTotal += keyRecord->stepRate * rate;
    result->flatTotal += keyRecord->flat;
    if (source != 0) {
        rate = keyRecord->firstChance;
        rate += calcStatEffectSum(source, 0x400, 0x2000);
        if (rate >= 101) {
            rate = PERCENT_MAX;
        }
    }
    if (ov01RollPercent(rate) != 0 && !(unit->flags & 0x10000)) {
        category = keyRecord->firstCategory;
        id = keyRecord->firstId;
    } else if (ov01RollPercent(keyRecord->secondChance) != 0 && !(unit->flags & 0x8000)) {
        category = keyRecord->secondCategory;
        id = keyRecord->secondId;
    }
    if (category != 0) {
        if (result->count < 8) {
            result->categories[result->count] = category;
            result->ids[result->count] = id;
            result->count += 1;
        }
    }
}

/*
 * Party snapshots: clears partySnapshots (0x5D0 bytes) and partySnapshotPtrs, copies each
 * party unit (unitTblGet side 0) into a 0x1F0-byte slot of partySnapshots and
 * stores that slot's address in partySnapshotPtrs.
 */

void partySnapshotBuild(void) {
    StatOwner **units;
    int count;
    struct Ov01Snapshot *snapshots;
    int i;

    battleFlushActiveEntries();
    D_6A2DF0[-5213] = boostGaugeParty[1];
    snapshots = (struct Ov01Snapshot *)partySnapshots;
    memset(snapshots, 0, sizeof(struct Ov01Snapshot) * 3);
    memset(partySnapshotPtrs, 0, 12);
    count = unitTblGet(0, &units);
    for (i = 0; i < count; i++) {
        if (units[i] != 0) {
            snapshots[i] = *(struct Ov01Snapshot *)units[i];
            ((struct Ov01Snapshot **)partySnapshotPtrs)[i] = &snapshots[i];
        }
    }
}

/* Stores the snapshot pointer table in *snapshots and returns the party size, 3. */
int partySnapshotGet(StatOwner ***snapshots) {
    *snapshots = partySnapshotPtrs;
    return 3;
}

void battleReviveDownedParty(void) {
    StatOwner **snapshots[1];
    int count;
    int i;
    int key;
    StatBlock *slot;

    count = partySnapshotGet(snapshots);
    for (i = 0; i < count; i++) {
        if (snapshots[0][i] == 0) {
            continue;
        }
        key = snapshots[0][i]->stats->key;
        if (key >= BATTLE_SLOT_KEY_LIMIT) {
            continue;
        }
        slot = battleSlotGet(key);
        if ((snapshots[0][i]->flags & UNIT_FLAG_DOWN) != 0 || slot->hp == 0) {
            slot->hp = 1;
        }
    }
}

int unitExpGain(int key, int gain, struct BattleResultRow *row);
void battleAbsentUnitsApply(int lowKeys, int divisor);
void battleGainsAdd(StatOwner *unit, int *gains, int divisor, struct BattleResultRow *outRow);

/*
 * Closes a battle: counts the party units that are still up, hands the battle result pairs to the
 * data box, and fills one 28-byte row per party unit of keys below 16 (key, the three stat gains,
 * the levels and the hp and ep caps the experience granted). Returns the party size.
 */
int battleResultApply(struct BattleResultRow *rows)
{
    struct BattleResult *result;
    StatOwner **snapshots;
    StatOwner **partyList;
    StatBlock *slot;
    int count;
    int alive;
    int levelsBefore;
    int maxHpBefore;
    int maxEpBefore;
    int lowKey;
    int key;
    int i;

    lowKey = 1;
    alive = 0;
    result = battleResultGet();
    count = partySnapshotGet(&snapshots);
    for (i = 0; i < count; i++) {
        if (snapshots[i] != 0 && (snapshots[i]->flags & UNIT_FLAG_DOWN) == 0) {
            alive++;
        }
    }
    for (i = 0; i < result->count; i++) {
        dataBoxInc(result->categories[i], result->ids[i]);
    }
    for (i = 0; i < count; i++) {
        if (snapshots[i] == 0) {
            continue;
        }
        if (snapshots[i]->stats->key >= BATTLE_SLOT_KEY_LIMIT) {
            continue;
        }
        memset(rows, 0, 28);
        partyList = snapshots;
        key = partyList[i]->stats->key;
        rows->key = key;
        if ((partyList[i]->flags & UNIT_FLAG_DOWN) == 0) {
            battleGainsAdd(partyList[i], &result->points, alive, rows);
            slot = battleSlotGet(key);
            levelsBefore = slot->expLevel;
            maxHpBefore = slot->maxHp;
            maxEpBefore = slot->maxEp;
            if (unitExpGain(key, rows->gainFirst, rows) != 0) {
                rows->levelGain = slot->expLevel - levelsBefore;
                rows->maxHpGain = slot->maxHp - maxHpBefore;
                rows->maxEpGain = slot->maxEp - maxEpBefore;
            }
            lowKey = (unsigned) ((unsigned short) snapshots[i]->stats->key - 1) < 10;
        }
        rows++;
    }
    battleAbsentUnitsApply(lowKey, alive);
    return count;
}

/*
 * Pays the battle result to the units that were left out of the party snapshots: for each key
 * of the battle mask list that no snapshot unit carries, the unit is bound, its stat gains
 * and experience are taken from the result and its hp and ep are raised by a quarter of the
 * caps (half with effect code 20).
 */
void battleAbsentUnitsApply(int lowKeys, int divisor)
{
    struct BattleResult *result;
    int keys[12];
    int gains[20];
    struct BattleResultRow row;
    StatOwner **snapshots;
    StatOwner *unit;
    StatBlock *slot;
    int keyCount;
    int partyCount;
    int found;
    int key;
    int hp;
    int ep;
    int i;
    int j;

    result = battleResultGet();
    memset(gains, 0, sizeof(gains));
    keyCount = battleMaskWordsStat4(keys);
    partyCount = partySnapshotGet(&snapshots);
    for (i = 0; i < keyCount; i++) {
        key = keys[i];
        found = 0;
        for (j = 0; j < partyCount; j++) {
            if (snapshots[j] != 0 && key == snapshots[j]->stats->key) {
                found = 1;
                break;
            }
        }
        if (found == 1) {
            continue;
        }
        if (lowKeys != 0 ? key >= 11 : key < 11) {
            continue;
        }
        if (battleEntryFindByKey(key) == 0) {
            gains[0] = result->points / 4 * 3;
        } else {
            gains[0] = result->points;
        }
        unit = battleContextBind(key, 0);
        slot = battleSlotGet(key);
        row.key = key;
        battleGainsAdd(unit, gains, divisor, &row);
        unitExpGain(key, row.gainFirst, &row);
        hp = unit->stats->maxHp / 4;
        ep = unit->stats->maxEp / 4;
        if (unitHasEffectCode(unit, 20) != 0) {
            hp *= 2;
            ep *= 2;
        }
        calcHpAdjust(unit, hp);
        calcEpAdjust(unit, ep);
        slot->hp = unit->stats->hp;
        slot->ep = unit->stats->ep;
    }
}

/*
 * Adds the three stat gains of unit to its slot record. A zero or negative
 * divisor clears out instead. Each gain is raised by its effect sum and the
 * record totals are capped.
 */
void battleGainsAdd(StatOwner *unit, int *gains, int divisor, struct BattleResultRow *outRow) {
    SlotRecord *record;
    int gain;
    int bonus;

    if (divisor <= 0) {
        memset(outRow, 0, sizeof(*outRow));
        return;
    }
    record = slotRecordGet(unit);
    if (record == 0) {
        return;
    }

    gain = gains[0] / divisor;
    bonus = calcStatEffectSum(unit, 0x400, 0x8000);
    if (bonus > 0) {
        gain += (gain * bonus) / PERCENT_MAX;
    }
    record->gainFirst += gain;
    if (record->gainFirst > SLOT_TOTAL_CAP) {
        record->gainFirst = SLOT_TOTAL_CAP;
    }
    outRow->gainFirst = gain;

    gain = gains[1] / divisor;
    bonus = calcStatEffectSum(unit, 0x400, 0x4000);
    if (unitHasEffectCode(unit, 63) != 0) {
        bonus += 10;
    }
    if (bonus > 0) {
        gain += (gain * bonus) / PERCENT_MAX;
    }
    record->gainSecond += gain;
    if (record->gainSecond > SLOT_CAPPED_CAP) {
        record->gainSecond = SLOT_CAPPED_CAP;
    }
    outRow->gainSecond = gain;

    gain = gains[2] / divisor;
    record->gainThird += gain;
    if (record->gainThird > SLOT_CAPPED_CAP) {
        record->gainThird = SLOT_CAPPED_CAP;
    }
    outRow->gainThird = gain;
}

void unitGrowthStep(int key);

/*
 * Adds gain to the experience counter of unit key; each time the counter reaches zero the unit
 * steps up one level (unitGrowthStep raises its stats) until the counter is positive again or the
 * level is 99. Returns 1 when the unit was at a lower level and the gain was spent, 0 when it
 * was already at level 99 or the counter stayed positive.
 */
int unitExpGain(int key, int gain, struct BattleResultRow *row)
{
    SlotRecord *slot;
    StatBlock *stat;
    UnitExpEntry *table;

    slot = slotRecordByIndex(key);
    stat = battleSlotGet(key);
    table = unitExpTblGet();
    if (stat->expLevel >= 99) {
        return 0;
    }
    slot->expCounter -= gain;
    if (slot->expCounter > 0) {
        return 0;
    }
    while (slot->expCounter <= 0 && stat->expLevel < 99) {
        stat->expLevel++;
        slot->expCounter += table[stat->expLevel].exp;
        unitGrowthStep(key);
    }
    return 1;
}

/*
 * Steps the eight growth values of party slot key toward the matching targets of its
 * GrowthRecord. Each step is a share of the gap, scaled to 100 - expLevel, and is
 * added to the stat index i feeds: the physical and magic attack and defence, the two
 * rates, the hp cap and maxEp, each capped.
 */
void unitGrowthStep(int key)
{
    GrowthRecord *target;
    SlotRecord *slot;
    StatBlock *stat;
    int i;

    target = growthRecordGet(key);
    slot = slotRecordByIndex(key);
    stat = battleSlotGet(key);
    for (i = 0; i < 8; i++) {
        int step;
        int goal = target->target[i];
        int current = slot->growth[i];
        int next = (100 * (goal - current) / (100 - stat->expLevel) + 25) / 50 + current;

        slot->growth[i] = next;
        step = next - current;
        switch (i) {
        case 0: {
            int sum = (unsigned short) stat->physAttack + step;
            stat->physAttack = sum;
            if ((short) sum >= 1000) {
                stat->physAttack = STAT_CAP_PHYS;
            }
            break;
        }
        case 1: {
            int sum = (unsigned short) stat->physDefence + step;
            stat->physDefence = sum;
            if ((short) sum >= 1000) {
                stat->physDefence = STAT_CAP_PHYS;
            }
            break;
        }
        case 2: {
            int sum = (unsigned short) stat->magAttack + step;
            stat->magAttack = sum;
            if ((short) sum >= 1000) {
                stat->magAttack = STAT_CAP_PHYS;
            }
            break;
        }
        case 3: {
            int sum = (unsigned short) stat->magDefence + step;
            stat->magDefence = sum;
            if ((short) sum >= 1000) {
                stat->magDefence = STAT_CAP_PHYS;
            }
            break;
        }
        case 4: {
            int sum = (unsigned char) stat->hitRate + step;
            stat->hitRate = sum;
            if ((signed char) sum >= 101) {
                stat->hitRate = PERCENT_MAX;
            }
            break;
        }
        case 5: {
            int sum = (unsigned char) stat->evadeRate + step;
            stat->evadeRate = sum;
            if ((signed char) sum >= 101) {
                stat->evadeRate = PERCENT_MAX;
            }
            break;
        }
        case 6: {
            int sum = stat->maxHp + step;
            stat->maxHp = sum;
            if (stat->maxHp > 999999) {
                stat->maxHp = 999999;
            }
            break;
        }
        case 7: {
            int sum = (unsigned short) stat->maxEp + step;
            stat->maxEp = sum;
            if ((short) sum >= 100) {
                stat->maxEp = 99;
            }
            break;
        }
        }
    }
}

/* ---- old ov01/tu029 ---- */
typedef signed char s8;

typedef int s32;

/*
 * Clears the 0x4000 flag of the unit. A unit whose sub-record state is 2 also
 * queues command 28 on its command queue.
 */
void func_00A90368(StatOwner *obj)
{
    Ov01Command command;

    obj->flags &= ~0x4000;
    if (obj->stats->state70 == 2) {
        command.command = 28;
        unitQueueAppend(obj, &command);
    }
}

/* Clears the dirty mark and the 0x208 flag bits, then recomputes the unit value. */
void unitValueRefresh(StatOwner *obj)
{
    obj->stats->dirty = 0;
    obj->flags &= ~0x208;
    unitValueReset(obj);
    calcStatTurn(obj);
}

/* Resets valueLevel to 0 and stores the calculated value as unitValue and unitValueSaved. */
void unitValueReset(StatOwner *obj)
{
    s32 derived;
    StatBlock *stat;

    derived = unitValueCalc(obj);
    stat = obj->stats;
    stat->valueLevel = 0;
    stat->unitValue = derived;
    stat->unitValueSaved = derived;
}

/*
 * Level-based value: (level - bias) * 9 + 180, then the status tests of
 * calcStatGet for (group 1, mask 1), (group 3, mask 0x40) and (group 3,
 * mask 0x80) add or subtract shares of it.
 */
s32 unitValueCalc(StatOwner *obj)
{
    StatBlock *stat;
    s32 levelBase;
    s32 derived;
    s32 adjust;
    s32 penalty;

    adjust = 0;
    stat = obj->stats;
    levelBase = (stat->valueLevel - stat->valueBias) * 9;
    derived = levelBase + 180;
    if (calcStatGet(obj, 1, 1) != 0) {
        adjust = derived / 2;
    }
    if (calcStatGet(obj, 3, 0x40) != 0) {
        penalty = derived;
        if (derived < 0) {
            penalty = levelBase + 183;
        }
        adjust -= penalty >> 2;
    }
    if (calcStatGet(obj, 3, 0x80) != 0) {
        adjust -= derived / 2;
    }
    return derived + adjust;
}

/*
 * For a unit that can resCmdAct (unitCanAct), lowers unitValue by 1, and by 1
 * more when ov01RollPercent(30) rolls; never below 0. Returns the new value.
 */
s32 unitValueDecay(StatOwner *obj)
{
    s32 value;
    s32 rolled;

    value = obj->stats->unitValue;
    if (unitCanAct(obj) != 0) {
        rolled = ov01RollPercent(30) != 0;
        value = value - rolled - 1;
        if (value < 0) {
            value = 0;
        }
    }
    obj->stats->unitValue = value;
    return value;
}

/*
 * Returns the signed state byte of record index, or 0 when unit has effect code 0x13.
 * The caller (attackQueue) adds the result to the unit's valueLevel (+0x64).
 */
s8 unitRecordStateGet(StatOwner *unit, s32 index)
{
    s8 state;

    state = tecRecordGet(index)->levelDelta;
    if (unitHasEffectCode(unit, 0x13) != 0) {
        state = 0;
    }
    return state;
}

/*
 * Returns 1 when exactly one of the two units has key (+0x62) below 16, the same
 * split tgtSideOf reads; 0 when both or neither are below it. When it returns 1,
 * tgtSelectTurnEnd walks all units (team 2) instead of one target.
 */
s32 unitSidesDiffer(StatOwner *unitA, StatOwner *unitB)
{
    if (unitA->stats->key < BATTLE_SLOT_KEY_LIMIT) {
        if (unitB->stats->key < BATTLE_SLOT_KEY_LIMIT) {
            return 0;
        }
        return 1;
    }
    return unitB->stats->key < BATTLE_SLOT_KEY_LIMIT;
}

/*
 * Returns 1 for a unit whose flags (+0x0C) lack 0x4 and for which the status
 * tests of calcStatGet for (group 1, 0x20), (group 2, 0x20) and (group 2, 0x1)
 * are all clear; 0 otherwise, and for a null unit.
 */
int unitCanAct(StatOwner *unit)
{
    if (unit == 0 || (unit->flags & UNIT_FLAG_DOWN) != 0 || calcStatGet(unit, 1, 0x20) != 0 || calcStatGet(unit, 2, 0x20) != 0 || calcStatGet(unit, 2, 0x1) != 0) {
        return 0;
    }
    return 1;
}

/* ---- old ov01/tu030 ---- */

int *effectPosGet(int index);
int func_00252CD8(int code, struct XenoActor *first, struct XenoActor *second, float scale);

/*
 * Whether the record for index applies to entity: the body's effect row and the record flags allow it, its status
 * has no 0x4000, its scale test passes when it has a code, and each code (up to four, ended by zero) matches the
 * entity's unit state. A negative index is first resolved by battleEffectResolve.
 */
int battleEffectApplies(StatOwner *command, int index, StatOwner *entity)
{
    struct TecRecord *record;
    int code;
    signed char *gateCursor;
    signed char *gate;
    int i;
    struct XenoActor *commandUnit;
    struct XenoActor *entityUnit;

    if (entity == 0) {
        return 0;
    }
    if (index < 0) {
        index = battleEffectResolve(command, index, entity);
    }
    if (index == 0) {
        return 0;
    }
    record = tecRecordGet(index);
    if ((effectPosGet(entity->stats->posIndex)[3] & 2) != 0 && (record->flags & 0x20) == 0) {
        return 0;
    }
    if ((entity->stats->flags & 0x4000) != 0) {
        return 0;
    }
    commandUnit = command->unit;
    entityUnit = entity->unit;
    if (record->code != 0) {
        if (func_00252CD8(record->code, commandUnit, entityUnit, (float)record->scale / 100.0f) == 0) {
            return 0;
        }
    }
    gate = record->rowGate;
    i = 0;
    code = *gate;
    if (code != 0) {
        gateCursor = gate;
        do {
            code &= unitStateFlagGet(entity);
            if (code == 0) {
                return 0;
            }
            i++;
            gateCursor++;
            if (i >= 4) {
                break;
            }
            code = *gateCursor;
        } while (code != 0);
    }
    return 1;
}

/*
 * Returns 0 when the unit's flag 4 is set or its key is 16 or more; otherwise 1 when the unaligned hp cap at
 * +0x2 of its StatBlock, divided by four, reaches the unit's hp at +0x5C, and 0 when it does not. The read
 * at +0x2 is unaligned (lwl/lwr in the original).
 */
int partyUnitHpLow(StatOwner *owner)
{
    StatBlock *stats;

    if ((owner->flags & UNIT_FLAG_DOWN) == 0) {
        stats = owner->stats;
        if (stats->key < BATTLE_SLOT_KEY_LIMIT) {
            if (stats->maxHp / 4 >= stats->hp) {
                return 1;
            }
        }
    }
    return 0;
}

/* Returns 1 or 0 for the key test the flags select, 2 when neither test applies. */
int tgtSideOf(StatOwner *entity, int flags)
{
    if ((flags & 3) == 1 || (flags & 4) != 0) {
        if (entity->stats->key < BATTLE_SLOT_KEY_LIMIT) {
            return 0;
        }
        return 1;
    }
    return (flags & 3) == 2 ? entity->stats->key < BATTLE_SLOT_KEY_LIMIT : 2;
}


/*
 * For a negative index, returns the first id effectIdListGet lists for -index whose record passes the flag tests
 * against the entity's unit state, or 0; a non-negative index is returned unchanged.
 */
int battleEffectResolve(StatOwner *command, int index, StatOwner *entity)
{
    int id;
    int stateFlags;
    int listPos;
    signed char recordFlags;

    if (index < 0) {
        stateFlags = unitStateFlagGet(entity);
        if (entity->stats->state70 != 3 && (command->flags & 0x80) != 0) {
            stateFlags &= ~1;
        }
        listPos = 0;
        do {
            int magnitude;
            short *idList;
            int byteOffset;

            magnitude = index;
            if (index < 0) {
                int negated;

                negated = -index;
                magnitude = negated;
            }
            idList = effectIdListGet(magnitude);
            byteOffset = listPos * 2;
            id = *(short *)((char *)idList + byteOffset);
            if (id != 0) {
                recordFlags = tecRecordGet(id)->rowGate[0];
                if (((recordFlags & 8) == 0 || entity->stats->state70 == 2) &&
                    ((recordFlags & 0x10) == 0 || (command->flags & 0x2000) != 0) && (stateFlags & recordFlags) != 0) {
                    return id;
                }
            }
            listPos++;
        } while (listPos < 3);
        return 0;
    }
    return index;
}

/*
 * Returns 0 when the effect row for index has its flags bit 0 set or when a unit on side 2 has index as its index;
 * 1 otherwise.
 */
int effectPosIsFree(int unused_arg0, int index)
{
    StatOwner **units;
    int count;
    int i;

    if ((effectPosGet(index)[3] & 1) != 0) {
        return 0;
    }
    count = unitTblGet(2, &units);
    for (i = 0; i < count; i++) {
        if (units[i] != 0 && index == units[i]->stats->posIndex) {
            return 0;
        }
    }
    return 1;
}

int effectPosGetCount(void);

/* Counts the effect indices for which effectPosIsFree returns 1; the first argument is passed unchanged to it. */
int effectPosCountFree(int passthrough)
{
    int count;
    int index;

    count = 0;
    for (index = 0; index < effectPosGetCount(); index++) {
        if (effectPosIsFree(passthrough, index)) {
            count++;
        }
    }
    return count;
}

/*
 * Copies into out the battleMaskWordTable words that the status block's word mask selects and that no unit's body
 * key matches, and returns how many; out may be null. A nonzero calcStatGet result returns 0.
 */
int battleMaskWordsUnmatched(StatOwner *entity, int *out)
{
    int words[0x14];
    StatOwner **units;
    PartyStatusBlock *block;
    int count;
    int unitCount;
    int i;
    int bit;
    int wordIndex;
    int unitIndex;
    int matched;

    if (calcStatGet(entity, 2, 0xFFFF) != 0) {
        return 0;
    }
    block = ptGetStatusBlock();
    count = 0;
    for (i = 0; i < 16; i++) {
        bit = 1 << i;
        if ((block->presentMask & bit) != 0) {
            words[count] = battleMaskWordTable[i];
            count++;
        }
    }
    words[count] = 0;
    count = 0;
    unitCount = unitTblGet(0, &units);
    if (words[0] != 0) {
        wordIndex = 0;
        do {
            matched = 0;
            for (unitIndex = 0; unitIndex < unitCount; unitIndex++) {
                if (units[unitIndex] != 0 && words[wordIndex] == units[unitIndex]->stats->key) {
                    matched = 1;
                    break;
                }
            }
            if (matched != 1) {
                if (out != 0) {
                    out[count] = words[wordIndex];
                }
                count++;
            }
            wordIndex++;
        } while (words[wordIndex] != 0);
    }
    return count;
}

/*
 * Collects into found the units the side of target selects whose flag 4 matches flag 0x10 of flags, skipping
 * empty slots and units whose status has 0x4000 set, and returns how many. Flag 4 of flags stores target alone
 * into found instead and returns 1.
 */
int tgtCollectUnits(StatOwner *target, int flags, StatOwner **found)
{
    StatOwner **sideUnits;
    int sideCount;
    int foundCount;
    int unitIndex;

    foundCount = 0;
    if ((flags & 4) != 0) {
        if (found != 0) {
            *found = target;
        }
        foundCount = 1;
    } else {
        sideCount = unitTblGet(tgtSideOf(target, flags), &sideUnits);
        for (unitIndex = 0; unitIndex < sideCount; unitIndex++) {
            StatOwner *unit;

            unit = sideUnits[unitIndex];
            if (unit == 0 || (unit->stats->flags & 0x4000) != 0) {
                continue;
            }
            if ((flags & 0x10) == 0) {
                if ((unit->flags & UNIT_FLAG_DOWN) != 0) {
                    continue;
                }
            } else if ((unit->flags & UNIT_FLAG_DOWN) == 0) {
                continue;
            }
            if (found != 0) {
                found[foundCount] = sideUnits[unitIndex];
            }
            foundCount++;
        }
    }
    return foundCount;
}

/*
 * Whether the entry for index passes: the entity has no stat 0x400 of group 2, tgtCollectUnits finds a unit for the
 * entry's index record's flags, the entity's limit is not below calcEpCost's value, and func_00A91B90 returns 0.
 */
int calcEntryUsable(StatOwner *entity, int index)
{
    BattleLookupEntry *lookupEntry;
    unsigned char tecFlags;
    int tecIndex;

    lookupEntry = battleLookupGet(index);
    tecIndex = lookupEntry->tecIndex;
    tecFlags = tecRecordGet(tecIndex)->flags;
    if (calcStatGet(entity, 2, 0x400) != 0) {
        return 0;
    }
    if (tgtCollectUnits(entity, tecFlags, 0) == 0) {
        return 0;
    }
    if (entity->stats->ep < calcEpCost(entity, tecIndex)) {
        return 0;
    }
    return func_00A91B90(index) == 0;
}

/* Whether target is a valid target of the record id: the record's flags byte selects the units (tgtCollectUnits). */
int tgtRecordCanTarget(StatOwner *target, int id)
{
    struct HitAction *hitAction;
    unsigned char tecFlags;

    hitAction = hitActionGet(id);
    tecFlags = tecRecordGet(hitAction->entry)->flags;
    if (dataBoxChkCategory1(id) <= 0) {
        return 0;
    }
    return tgtCollectUnits(target, tecFlags, 0) != 0;
}


int calcEntryMaskTest(int index, int mask)
{
    if (index == 0 || index >= 11) {
        return 0;
    }
    return (calcTables.entryMasks[index] & mask) != 0;
}

/*
 * Stores into found the effect indices of the band the entity's key selects (11, 12 or 13) whose mask test
 * passes and whose required level the entity's value_84 does not fall below, and returns how many; any other
 * key returns 0.
 */
int calcKeyBandRecords(StatOwner *entity, int *found)
{
    StatBlock *statBlock;
    struct CalcLevelRecord *record;
    unsigned short rawKey;
    unsigned short metricMask;
    short bandKey;
    int firstIndex;
    int lastIndex;
    int count;
    int index;

    statBlock = entity->stats;
    count = 0;
    rawKey = statBlock->key;
    if ((unsigned int)(rawKey - 11) >= 5) {
        return 0;
    }
    bandKey = rawKey;
    switch (bandKey) {
    case 11:
        firstIndex = 1;
        lastIndex = 7;
        break;
    case 12:
        firstIndex = 8;
        lastIndex = 14;
        break;
    case 13:
        firstIndex = 15;
        lastIndex = 21;
        break;
    default:
        return 0;
    }
    for (index = firstIndex; index <= lastIndex; index++) {
        record = calcLevelRecordGet(index);
        metricMask = record->entryMask;
        if (calcEntryMaskTest(statBlock->linkedSlots[1], metricMask) == 0) {
            continue;
        }
        if (statBlock->currentMax < record->requiredLevel) {
            continue;
        }
        found[count] = index;
        count++;
    }
    return count;
}

/* Returns 1 when the entity's current value reaches the required level of the record index selects, 0 otherwise. */
int calcCurrentMeetsRecord(StatOwner *entity, int index)
{
    struct CalcLevelRecord *levelRecord;

    levelRecord = calcLevelRecordGet(index);
    if (entity->stats->current < levelRecord->requiredLevel) {
        return 0;
    }
    return 1;
}

/*
 * Stores into found the effect indices 0..16 whose record pairs the entity's key with the acting unit's key
 * in either order, and returns how many.
 */
int calcKeyPairRecords(StatOwner *entity, int *found)
{
    StatOwner *actingUnit;
    PairLink *pairLink;
    short entityKey;
    short actingKey;
    int count;
    int index;

    count = 0;
    actingUnit = actStockGet()->boost[1];
    if (actingUnit == 0) {
        return 0;
    }
    entityKey = entity->stats->key;
    actingKey = actingUnit->stats->key;
    for (index = 0; index < 17; index++) {
        if (slotPairTest(index) != 0 && actEntryFlagGet(index) == 0) {
            pairLink = pairLinkGet(index);
            if ((pairLink->slotFirst == entityKey && pairLink->slotSecond == actingKey) ||
                (pairLink->slotFirst == actingKey && pairLink->slotSecond == entityKey)) {
                found[count] = index;
                count++;
            }
        }
    }
    return count;
}

/* Whether both the entity and the acting unit reach the level record record_index requires. */
int calcLevelsMeetRecord(StatOwner *entity, int record_index)
{
    StatOwner *actingUnit;
    PairLink *pairLink;

    actingUnit = actStockGet()->boost[1];
    if (actingUnit == 0) {
        return 0;
    }
    pairLink = pairLinkGet(record_index);
    if (entity->stats->level < pairLink->requiredLevel || actingUnit->stats->level < pairLink->requiredLevel) {
        return 0;
    }
    return 1;
}

/*
 * Collects into found the pair records 60..89 that hold code and whose partner code the acting unit satisfies,
 * and returns how many.
 */
int calcPairedRecords(StatOwner *unused, int code, int *found)
{
    StatOwner *actingUnit;
    unsigned char pairFirst;
    unsigned char pairSecond;
    int count;
    int index;

    count = 0;
    actingUnit = actStockGet()->boost[1];
    if (actingUnit == 0) {
        return 0;
    }
    for (index = 60; index < 90; index++) {
        pairFirst = battleLookupGet(index)->firstCode;
        pairSecond = battleLookupGet(index)->secondCode;
        if ((code == pairFirst && slotCodeTest(actingUnit, pairSecond) != 0) ||
            (code == pairSecond && slotCodeTest(actingUnit, pairFirst) != 0)) {
            found[count] = index;
            count++;
        }
    }
    return count;
}

/*
 * Returns 1 when calcEntryUsable passes both for the entity and for the acting unit (the current entity of
 * actStockGet) with index, 0 otherwise.
 */
int calcPairEntryUsable(StatOwner *entity, int index)
{
    StatOwner *actingUnit;

    actingUnit = actStockGet()->boost[1];
    if (actingUnit == 0) {
        return 0;
    }
    if (calcEntryUsable(entity, index) == 0) {
        return 0;
    }
    return calcEntryUsable(actingUnit, index) != 0;
}

int slotFlagCollect(StatOwner *subject, int *found)
{
    int count;
    int index;

    count = 0;
    for (index = 1; index < 11; index++) {
        if (slotMarkTest(subject, index) != 0) {
            found[count] = index;
            count++;
        }
    }
    return count;
}

/* ---- old ov01/tu031 ---- */


int tgtSideAccepts(StatOwner *obj, int id)
{
    StatOwner **sideUnits;
    struct TgtIdRecord *idRecord;
    int actionIndex;
    unsigned char tecFlags;
    int sideCount;
    int hits;
    int unitIndex;

    if (obj->stats->level < tgtIdRecordGet(id)->minLevel) {
        return 0;
    }
    idRecord = tgtIdRecordGet(id);
    unitIndex = 0;
    hits = 0;
    actionIndex = idRecord->actionIndex;
    tecFlags = tecRecordGet(actionIndex)->flags;
    sideCount = unitTblGet(tgtSideOf(obj, tecFlags), &sideUnits);
    for (; unitIndex < sideCount; unitIndex++) {
        if (sideUnits[unitIndex] != 0) {
            if (battleEffectApplies(obj, actionIndex, sideUnits[unitIndex]) != 0) {
                hits++;
            }
        }
    }
    return hits != 0;
}

int cfEventGet(void);

/*
 * Units with key 3, 6 or 12 report 1 unless the event flags of cfEventGet
 * have 0x400 set; every other unit reports 0.
 */
int calcCfEventKeyTest(StatOwner *unit)
{
    int eventFlags;

    if (unit->stats->key == 3 || unit->stats->key == 6 || unit->stats->key == 12) {
        eventFlags = cfEventGet();
        if ((eventFlags & 0x400) == 0) {
            return 1;
        }
    }
    return 0;
}

StatOwner *tgtFindFirst(struct AttackCommand *record, StatOwner *value)
{
    StatOwner *singleTarget[1];
    StatOwner **sideUnits;
    StatOwner *attacker;
    int actionFlags;
    int multiFlag;
    int valueAccepted;
    int sideCount;
    int side;
    int unitIndex;

    attacker = record->attacker;
    tgtIdx = 0;
    tgtNum = 0;
    tgtIdx = 0;
    actionFlags = tecRecordGet(record->actionIdx)->flags;
    multiFlag = actionFlags & 0x8;
    if (multiFlag == 0) {
        valueAccepted = 0;
        if (value != 0 && tgtCmdAccepts(record, value)) {
            if (actionFlags & 4) {
                if (attacker == value) {
                    valueAccepted = 1;
                }
            } else if (actionFlags & 0x10) {
                if (value->flags & UNIT_FLAG_DOWN) {
                    valueAccepted = 1;
                }
            } else {
                if (!(value->flags & UNIT_FLAG_DOWN)) {
                    valueAccepted = 1;
                }
            }
        }
        if (valueAccepted == 1) {
            tgtTbl[0] = value;
            tgtNum = valueAccepted;
            return tgtTbl[tgtIdx++];
        }
    }
    if (actionFlags & 4) {
        singleTarget[0] = attacker;
        sideUnits = singleTarget;
        sideCount = 1;
    } else {
        if ((actionFlags & 3) == 3) {
            side = 2;
        } else if (actionFlags & 1) {
            if (attacker->stats->key >= BATTLE_SLOT_KEY_LIMIT) {
                side = 1;
            } else {
                side = 0;
            }
        } else {
            side = attacker->stats->key < BATTLE_SLOT_KEY_LIMIT;
        }
        sideCount = unitTblGet(side, &sideUnits);
    }
    for (unitIndex = 0; unitIndex < sideCount; unitIndex++) {
        if (sideUnits[unitIndex] == 0 || !tgtCmdAccepts(record, sideUnits[unitIndex])) {
            continue;
        }
        if (actionFlags & 0x10) {
            if (!(sideUnits[unitIndex]->flags & UNIT_FLAG_DOWN)) {
                continue;
            }
        } else if (sideUnits[unitIndex]->flags & UNIT_FLAG_DOWN) {
            continue;
        }
        tgtTbl[tgtNum++] = sideUnits[unitIndex];
    }
    if (tgtNum == 0) {
        return 0;
    }
    if (multiFlag == 0) {
        tgtNum = 1;
    }
    return tgtTbl[tgtIdx++];
}

StatOwner *tgtFindNext(void)
{
    if (tgtIdx >= tgtNum) {
        return 0;
    }
    return tgtTbl[tgtIdx++];
}

/* Whether the candidate unit value is an acceptable target for the command record. */
int tgtCmdAccepts(struct AttackCommand *record, StatOwner *value)
{
    return battleEffectApplies(record->attacker, record->actionIdx, value) != 0;
}

/* Returns the entry at 0x00ac2430 when the unit's key is below 16, else the entry at 0x00ac2434. */
struct BoostGauge *boostGaugeGet(StatOwner *unit)
{
    if (unit->stats->key < BATTLE_SLOT_KEY_LIMIT) {
        return (struct BoostGauge *)boostGaugeParty;
    }
    return (struct BoostGauge *)&boostGaugeParty[4];
}

/* Returns the entry at 0x00ac2434 when flag is nonzero, else the entry at 0x00ac2430. */
struct BoostGauge *boostGaugeSelect(int flag)
{
    if (flag == 0) {
        return (struct BoostGauge *)boostGaugeParty;
    }
    return (struct BoostGauge *)&boostGaugeParty[4];
}

/* Returns 1 when the stock slot at index 4 or index 5 of the actStockGet table is set, else 0. */
int actStockHasBoost(void)
{
    struct ActStock *stock;

    int hasBoost;

    stock = actStockGet();
    if (stock->boost[0] != 0) {
        hasBoost = 1;
    } else {
        stock = actStockGet();
        if (stock->boost[1] == 0) {
            return 0;
        }
        hasBoost = 1;
    }
    return hasBoost;
}

int calcBoostOk(StatOwner *obj, int id)
{
    struct BoostGauge *gauge;
    int cannotAct;

    if (obj->stats->key >= 141) {
        return 0;
    }
    cannotAct = unitCanAct(obj) == 0;
    if (cannotAct) {
        return 0;
    }
    if ((obj->flags & 0x200) != 0 || actStockGet()->boost[id] != 0 || obj->stats->hp == 0 ||
        obj->stats->state70 == 3) {
        return 0;
    }
    gauge = boostGaugeGet(obj);
    if (id == 1) {
        if (obj->stats->state70 == 2) {
            return 0;
        }
        if (calcStatGet(obj, 2, 0x800) != 0) {
            return 0;
        }
        if (gauge->stock <= 0) {
            return 0;
        }
        if (obj->flags & UNIT_FLAG_STOCKED) {
            if (unitHasEffectCode(obj, 44) == 0) {
                return 0;
            }
        }
    }
    return 1;
}

/*
 * Marks the unit with 0x200 and stores it in boost slot `slot` of the action stock;
 * slot 1 first spends a boost (boostAddStock with -1).
 */
void calcActStockSet(StatOwner *unit, int slot)
{
    struct ActStock *stock;

    if (slot == 1) {
        boostAddStock(unit, -1);
    }
    unit->flags |= 0x200;
    stock = actStockGet();
    stock->boost[slot] = unit;
}

/* ---- old ov01/tu032 ---- */
/* Level-scaled chance roll for entry `index`: 0 when the entry has flag 4, else the roll of ov01RollPercent with the owner's level, capped at 100. */
int actLevelChanceRoll(StatOwner *owner, int index)
{
    struct TecRecord *techRecord;
    signed char chanceLevel;
    int rollPercent;

    techRecord = tecRecordGet(index);
    if (techRecord->hitFlags & 4) {
        return 0;
    }
    rollPercent = PERCENT_MAX;
    chanceLevel = owner->stats->chanceLevel;
    if (chanceLevel <= PERCENT_MAX) {
        rollPercent = chanceLevel;
    }
    return ov01RollPercent(rollPercent) != 0;
}

struct ActStepCounter *actStepCounterGet(void)
{
    return &actStepCounter;
}

/* Sets the step counter to `step`; at step 3 it draws a hold of rnd() + 4 into +0x4, then draws a class 2, 4 or 10 by rnd(100) into +0x8 and clears +0xC..+0x14. */
void actStepCounterSet(int step)
{
    struct ActStepCounter *counter;
    int roll;

    counter = actStepCounterGet();
    counter->step = step;
    if (step == ACT_STEP_HOLD) {
        counter->hold = rnd(3) + 4;
    }
    roll = rnd(100);
    if (roll >= 96) {
        counter->multiplier = 10;
    } else if (roll >= 61) {
        counter->multiplier = 4;
    } else {
        counter->multiplier = 2;
    }
    counter->cleared0C = 0;
    counter->bonusFlag = 0;
    counter->stepState = 0;
}

/* The current phase: the step, or the hold when the step is 3. */
int actStepCounterValue(void)
{
    struct ActStepCounter *counter;
    int step;

    counter = actStepCounterGet();
    step = counter->step;
    if (step != ACT_STEP_HOLD) {
        return step;
    }
    return counter->hold;
}

/* Advances the step counter modulo 4 and sets it through actStepCounterSet. */
void actStepCounterAdvance(void)
{
    struct ActStepCounter *counter;
    int nextStep;

    counter = actStepCounterGet();
    nextStep = counter->step + 1;
    counter->step = nextStep;
    if (nextStep >= ACT_STEP_COUNT) {
        counter->step = 0;
    }
    actStepCounterSet(counter->step);
}

extern unsigned char sdvSpecialBuf[];

void sdvInitSpecialWork(void)
{
    memset(&sdvSpecialBuf, 0, 0x10);
}

/* Sets bit (index - 16) of the 16-byte table sdvSpecialBuf; indexes outside 16..143 are ignored. */
void sdvSpecialBitSet(int index)
{
    int bit;

    bit = index - 16;
    if ((unsigned int) bit < 128U) {
        sdvSpecialBuf[bit / 8] |= 1 << (bit % 8);
    }
}

/* Returns bit (index - 16) of sdvSpecialBuf, or 0 outside 16..143. */
int sdvSpecialBitGet(int index)
{
    int bit;
    int byteIndex;

    bit = index - 16;
    if ((unsigned int) bit >= 128U) {
        return 0;
    }
    byteIndex = bit;
    if (bit < 0) {
        byteIndex = index - 9;
    }
    return ((unsigned int) sdvSpecialBuf[byteIndex >> 3] >> (bit - (byteIndex >> 3) * 8)) & 1;
}

/* Marks entry `index` in the per-entry byte table actEntryFlags; the index is the key of tecRecordGet. */
void actEntryFlagSet(int index)
{
    actEntryFlags[index] = 1;
}

/* Reads the per-entry byte table actEntryFlags at `index`. */
signed char actEntryFlagGet(int index)
{
    return actEntryFlags[index];
}

/* Marks slot index - 0x37 (slots 0..2) of the table D_00AC2AC8. */
void func_00A91B68(int index)
{
    unsigned int slot;

    slot = index - 0x37;
    if (slot < 3U) {
        D_00AC2AC8[slot] = 1;
    }
}

/* Reads slot index - 0x37 of D_00AC2AC8; 0 outside slots 0..2. */
signed char func_00A91B90(int index)
{
    unsigned int slot;

    slot = index - 0x37;
    if (!(slot < 3U)) {
        return 0;
    }
    return D_00AC2AC8[slot];
}

void actStockReset(void)
{
    struct ActStock *stock;
    StatOwner **slot;
    StatOwner **end;
    int i;

    stock = actStockGet();
    stock->max = 0;
    stock->pickCursor = 0;
    stock->lastPicked = 0;
    stock->pickSource = 1;
    slot = &stock->stock[ACT_STOCK_SLOTS - 1];
    for (i = 3; i >= 0; i--) {
        *slot = 0;
        slot--;
    }
    end = &stock->boost[0];
    slot = &stock->boost[1];
    i = slot - end;
    do {
        *slot = 0;
        slot--;
        i--;
    } while (i >= 0);
}

/* Returns the action-stock record. */
struct ActStock *actStockGet(void)
{
    return &actStock;
}

/* Half the side-2 units that pass the battle check (flag 4 skipped), rounded up, capped at 4: the action-stock limit. */
int actStockMaxGet(void)
{
    StatOwner **units;
    StatOwner *unit;
    int count;
    int eligible;
    int i;

    count = unitTblGet(2, &units);
    eligible = 0;
    for (i = 0; i < count; i++) {
        unit = units[i];
        if (unit != 0 && (unit->flags & UNIT_FLAG_DOWN) == 0) {
            if (unitCanAct(unit) != 0) {
                eligible++;
            }
        }
    }
    if (eligible > 0) {
        if ((eligible * 5) % 10 >= 5) {
            eligible = eligible / 2 + 1;
        } else {
            eligible = eligible / 2;
        }
    }
    if (eligible >= 5) {
        return ACT_STOCK_SLOTS;
    }
    return eligible;
}

/* Nonzero when the stock is used up: pickCursor at or past max and both boost slots empty. */
int actStockNeedsRefill(void)
{
    struct ActStock *stock;
    StatOwner **boost;
    int max;
    int i;

    stock = actStockGet();
    max = stock->max;
    if (stock->pickCursor < max) {
        return 0;
    }
    boost = &stock->boost[0];
    for (i = 0; i < ACT_STOCK_BOOST_SLOTS; i++) {
        if (*boost != 0) {
            return 0;
        }
        boost++;
    }
    return 1;
}

/* Refills the action stock: limit from actStockMaxGet, cursor and stock cleared, then the side-2 units that pass the checks and lack flag 8 are stocked and flagged 8. */
void actStockMake(void)
{
    struct ActStock *stock;
    StatOwner **units;
    StatOwner *unit;
    StatOwner **slot;
    int count;
    int stocked;
    int i;

    stock = actStockGet();
    stocked = 0;
    stock->max = actStockMaxGet();
    stock->pickCursor = 0;
    count = unitTblGet(2, &units);
    slot = &stock->stock[ACT_STOCK_SLOTS - 1];
    for (i = 3; i >= 0; i--) {
        *slot = 0;
        slot--;
    }
    while (stocked < stock->max) {
        for (i = 0; i < count; i++) {
            if (units[i] != 0 && (units[i]->flags & UNIT_FLAG_DOWN) == 0) {
                if (unitCanAct(units[i]) != 0 && unitValueDecay(units[i]) == 0) {
                    unit = units[i];
                    if ((unit->flags & UNIT_FLAG_STOCKED) == 0) {
                        unit->flags |= UNIT_FLAG_STOCKED;
                        stock->stock[stocked] = unit;
                        stocked++;
                        if (stocked >= stock->max) {
                            break;
                        }
                    }
                }
            }
        }
    }
}

/* Hands out the next acting unit: refills when actStockNeedsRefill says so, takes boost1 and boost2 first, then the stock in order, skips units with flag 4, keeps the pick in lastPicked. */
StatOwner *actStockPick(void)
{
    struct ActStock *stock;
    StatOwner *unit;
    StatOwner **boost;
    int i;
    int cursor;

    stock = actStockGet();
    stock->pickSource = 1;
    do {
        if (actStockNeedsRefill()) {
            actStockMake();
            stock->pickSource = 0;
        }
        unit = 0;
        for (i = 0; i < ACT_STOCK_BOOST_SLOTS; i++) {
            boost = &stock->boost[i];
            if (*boost != 0) {
                unit = *boost;
                *boost = 0;
                if (stock->pickCursor > 0) {
                    stock->stock[stock->pickCursor - 1] = 0;
                }
                stock->pickSource = 2;
                break;
            }
        }
        if (unit == 0) {
            cursor = stock->pickCursor;
            unit = stock->stock[cursor];
            if (cursor > 0) {
                stock->stock[cursor - 1] = 0;
            }
            stock->pickCursor = cursor + 1;
        }
        if (unit == 0 || (unit->flags & UNIT_FLAG_DOWN) != 0) {
            unit = 0;
        }
    } while (unit == 0);
    stock->lastPicked = unit;
    return unit;
}

/* Drops `unit` from the two boost slots or from the stock; *index receives its stock position, or -1. */
int actStockRemove(StatOwner *unit, int *stockIndex)
{
    struct ActStock *stock;
    StatOwner **boost;
    int i;

    stock = actStockGet();
    *stockIndex = -1;
    boost = &stock->boost[0];
    for (i = 0; i < ACT_STOCK_BOOST_SLOTS; i++) {
        if (boost[i] == unit) {
            boost[i] = 0;
            return 1;
        }
    }
    for (i = 0; i < stock->max; i++) {
        if (stock->stock[i] == unit) {
            *stockIndex = i;
            stock->stock[i] = 0;
            return 1;
        }
    }
    return 0;
}

/* Drops `unit` from the stock only; *index receives its stock position, or -1. */
int actStockRemoveStock(StatOwner *unit, int *stockIndex)
{
    struct ActStock *stock;
    int i;

    stock = actStockGet();
    *stockIndex = -1;
    for (i = 0; i < stock->max; i++) {
        if (stock->stock[i] == unit) {
            *stockIndex = i;
            stock->stock[i] = 0;
            return 1;
        }
    }
    return 0;
}

/* Clears the second boost slot (boost1); eventActStockBoostClear calls it. */
void actStockClearBoost1(void)
{
    actStockGet()->boost[1] = 0;
}

/* Names the overlay .data tables reach (the tables are not split per file yet). */
const char D_00ABA420[] = "kosmos_udeR";
const char D_00ABA430[] = "jin_saya_tuka";

/* Initialized overlay data of this file (the mask and turn tables and the entry masks), in address order. */
int battleMaskWordTable[12] = {
    1, 2, 3, 4, 5, 6, 7, 11,
    12, 13, 0, 0
};

unsigned char calcStatDefTurnTable[64] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0xFF, 0x00,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

struct CalcTables calcTables = {
    {
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x00, 0x00,
    0x0F, 0x0A, 0x05, 0x05, 0x05, 0x0A, 0x05, 0x05, 0x0F, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x0A, 0x00,
    0x19, 0x14, 0x0F, 0x0F, 0x0F, 0x14, 0x0F, 0x0F, 0x19, 0x14, 0x14, 0x14
    },
    {
    0x00140014, 0x00008000, 0x00004000, 0x00002000, 0x00001000, 0x00000800,
    0x00000400, 0x00000200, 0x00000000, 0x00000000, 0x00000000
    }
};
