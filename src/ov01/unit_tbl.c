#include "common.h"
#include "shared.h"
#include "main/game_camera.h"
#include "ov01/unit_cmd.h"
#include "ov01/unit_tbl.h"
#include "ov01/calc.h"

/*
 * The u64 read at object +8 (the word before flags): its high word is BattleUnit.flags, so flag bit 0x1 is bit 32
 * and bit 0x4 is bit 34 of the u64. BattleUnit (unit_cmd.h) has no 8-byte view of that pair, hence the one cast.
 */
#define UNIT_CALLBACK_FLAG1_ONLY 0x100000000ULL
#define UNIT_CALLBACK_FLAG1_FLAG4_MASK 0x500000000ULL

/* Slot layout of unitTbl: players use slots 0 to UNIT_PL_SLOTS - 1, enemies the next UNIT_EN_SLOTS of UNIT_TBL_SLOTS. */
#define UNIT_PL_SLOTS 3
#define UNIT_EN_SLOTS 5
#define UNIT_TBL_SLOTS 8

/* The unit objects of the battle: players first, then enemies. */
extern BattleUnit *unitTbl[UNIT_TBL_SLOTS];
extern int unitPlNum;
extern int unitEnNum;

int unitTblGet(int side, BattleUnit ***table)
{
    if (side == UNIT_SIDE_PLAYER) {
        *table = unitTbl;
        return UNIT_PL_SLOTS;
    }
    if (side == UNIT_SIDE_ENEMY) {
        *table = &unitTbl[UNIT_PL_SLOTS];
        return UNIT_EN_SLOTS;
    }
    *table = unitTbl;
    return UNIT_TBL_SLOTS;
}

/*
 * Stores object in the first empty slot of the side's range: side 0 uses
 * slots 0..2 (player units), side 1 uses slots 3..7 (enemy units). Other
 * sides are rejected. Returns 1 when a slot was filled, 0 otherwise.
 */
int unitTblSet(int side, BattleUnit *object)
{
    int first;
    int end;
    int i;

    if (side == UNIT_SIDE_PLAYER) {
        first = 0;
        end = UNIT_PL_SLOTS;
    } else if (side == UNIT_SIDE_ENEMY) {
        first = UNIT_PL_SLOTS;
        end = UNIT_TBL_SLOTS;
    } else {
        return 0;
    }
    for (i = first; i < end; i++) {
        if (unitTbl[i] == 0) {
            unitTbl[i] = object;
            if (i < UNIT_PL_SLOTS) {
                unitPlNum++;
            } else {
                unitEnNum++;
            }
            return 1;
        }
    }
    return 0;
}

int unitTblRemove(BattleUnit *object)
{
    BattleUnit **units = unitTbl;
    int i;

    for (i = 0; i < UNIT_TBL_SLOTS; i++) {
        if (units[i] == object) {
            units[i] = 0;
            if (i < UNIT_PL_SLOTS) {
                unitPlNum--;
            } else {
                unitEnNum--;
            }
            return 1;
        }
    }
    return 0;
}

int unitNumGet(int side)
{
    int count;

    if (side == UNIT_SIDE_PLAYER) {
        count = unitPlNum;
    } else if (side == UNIT_SIDE_ENEMY) {
        count = unitEnNum;
    } else {
        count = unitPlNum + unitEnNum;
    }
    return count;
}

/* Counts the non-null units of side whose flags word has bit 0x4 clear. */
int unitLiveNumGet(int side)
{
    BattleUnit **records;
    BattleUnit **cursor;
    int count;
    int remaining;
    int flagClearCount;

    count = unitTblGet(side, &records);
    flagClearCount = 0;
    if (count > 0) {
        cursor = records;
        remaining = count;
        do {
            BattleUnit *record = *cursor;

            cursor++;
            if (record != 0 && !(record->flags & 4)) {
                flagClearCount++;
            }
            remaining--;
        } while (remaining != 0);
    }
    return flagClearCount;
}

/* Counts the units of side with bit 0x4 clear that calcStatGet accepts (returns 0) for (2, 1). */
int unitLiveNumGet2(int side)
{
    BattleUnit **records;
    int count;
    int i;
    int acceptedCount;

    acceptedCount = 0;
    i = 0;
    count = unitTblGet(side, &records);
    if (count > 0) {
        do {
            BattleUnit *record = records[i];

            i++;
            if (record != 0 && !(record->flags & 4)) {
                if (calcStatGet(record, 2, 1) == 0) {
                    acceptedCount++;
                }
            }
        } while (i < count);
    }
    return acceptedCount;
}

/* Counts the non-null units of side whose flags word has bit 0x4 set. */
int unitDownNumGet(int side)
{
    BattleUnit **records;
    BattleUnit **cursor;
    int count;
    int remaining;
    int flagSetCount;

    count = unitTblGet(side, &records);
    flagSetCount = 0;
    if (count > 0) {
        cursor = records;
        remaining = count;
        do {
            BattleUnit *record = *cursor;

            cursor++;
            if (record != 0 && (record->flags & 4)) {
                flagSetCount++;
            }
            remaining--;
        } while (remaining != 0);
    }
    return flagSetCount;
}

/*
 * Counts the non-null units of side whose flags word has bit 0x1 set and
 * bit 0x4 clear.
 */
int busyUnitChk(int side)
{
    BattleUnit **records;
    BattleUnit **cursor;
    int count;
    int remaining;
    int flag1ClearCount;

    count = unitTblGet(side, &records);
    flag1ClearCount = 0;
    if (count > 0) {
        cursor = records;
        remaining = count;
        do {
            BattleUnit *record = *cursor;

            cursor++;
            if (record != 0 && (*(u64 *) &record->unmodeled_00[8] & UNIT_CALLBACK_FLAG1_FLAG4_MASK) == UNIT_CALLBACK_FLAG1_ONLY) {
                flag1ClearCount++;
            }
            remaining--;
        } while (remaining != 0);
    }
    return flag1ClearCount;
}

/*
 * Counts the non-null units of side whose command queue is not empty, plus
 * the ones whose move effect has not finished.
 */
int unitNumBusyGet(int side)
{
    BattleUnit **records;
    int keyHigh;
    int keyLow;
    int count;
    int i;
    int busyCount;

    count = unitTblGet(side, &records);
    busyCount = 0;
    for (i = 0; i < count; i++) {
        if (records[i] != 0) {
            unitMoveKey(records[i], &keyHigh, &keyLow);
            if (unitQueueIsEmpty(records[i]) == 0 ||
                unitEffectKeyFree(records[i], keyHigh, keyLow) == 0) {
                busyCount++;
            }
        }
    }
    return busyCount;
}

/* Index of target in the combined unit table (side 2), or -1 when absent. */
int unitNoGet(BattleUnit *target)
{
    BattleUnit **records;
    BattleUnit **cursor;
    int count;
    int i;

    count = unitTblGet(UNIT_SIDE_ALL, &records);
    i = 0;
    if (count > 0) {
        cursor = records;
        do {
            BattleUnit *record = *cursor;

            cursor++;
            if (record != 0 && record == target) {
                return i;
            }
            i++;
        } while (i < count);
    }
    return -1;
}

/* Entry index of the combined unit table (side 2). The check is index > count,
 * so index == count is not rejected by it. */
BattleUnit *unitPtrGet(int index)
{
    BattleUnit **records;
    int count;

    count = unitTblGet(UNIT_SIDE_ALL, &records);
    if (count < index || index < 0) {
        return 0;
    }
    return records[index];
}
