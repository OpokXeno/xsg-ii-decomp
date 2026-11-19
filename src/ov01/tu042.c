/*
 * OV01 original TU 42: 0x00a9b280..0x00a9bc08, the bat.dat table getters
 */
#include "common.h"
#include "shared.h"
#include "main/sef.h"
#include "main/tu124.h"
#include "ov01/tu001.h"
#include "ov01/tu008.h"
#include "ov01/tu017.h"
#include "ov01/tu033.h"
#include "ov01/tu035.h"
#include "ov01/tu036.h"
#include "ov01/tu023.h"
#include "ov01/tu040.h"
#include "ov01/tu042.h"
#include "ov01/tu044.h"
#include "ov01/tu046.h"

/* Model key of each unit index (dataModelKeyGet). */
static int dataModelKeyTbl[26] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00010008,
    0x0001002B, 0x0001002A, 0x00000000, 0x00010032, 0x00010032, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00014007, 0x00014006, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000
};

int dataModelKeyGet(int index) {
    return dataModelKeyTbl[index];
}

int func_00A9B298(int value) {
    return value;
}

/* Returns its argument unchanged. Caller: dataVoiceSetLoad. */
int func_00A9B2A0(int value) {
    return value;
}

/* bat.dat as dataBatDatLoad (tu008) reads it. The header word at byte BAT_DAT_PARTY_UNITS holds the offset of the
 * party unit table (units 1..15), the one at BAT_DAT_ENEMY_UNITS that of the enemy unit table (units 16..0x8C);
 * both have BAT_DAT_UNIT_SIZE-byte records whose first halfword is the offset of the unit's name from the table. */

#define BAT_DAT_PARTY_UNITS 0x8
#define BAT_DAT_ENEMY_UNITS 0xC
#define BAT_DAT_ENEMY_FIRST 16
#define BAT_DAT_UNIT_LIMIT 0x8D
#define BAT_DAT_UNIT_SIZE 0x5C

/* The name batUnitNameGet gives unit 0 ("NULL"). */
extern char *batUnitNullName;

/* Unit record `index` of bat.dat: party units 1..15, enemy units 16..0x8C; for any other index the party record
 * of the battle state's slotIndex (0 when that is not above 1). `word` first holds the byte offset of the header
 * word, then the table offset read from it: with a second local the original's single register is lost. */
void *batUnitRecordGet(int index)
{
    unsigned char *table;
    int word;
    int record;

    if (index < BAT_DAT_ENEMY_FIRST) {
        record = index - 1;
        word = BAT_DAT_PARTY_UNITS;
    } else if (index >= BAT_DAT_UNIT_LIMIT) {
        word = BAT_DAT_PARTY_UNITS;
        record = battleStateGet()->slotIndex - 1;
        if (record <= 0) {
            return 0;
        }
    } else {
        record = index - BAT_DAT_ENEMY_FIRST;
        word = BAT_DAT_ENEMY_UNITS;
    }
    word = *(int *)((unsigned char *)&batDat + word);
    table = (unsigned char *)&batDat + word;
    return table + record * BAT_DAT_UNIT_SIZE;
}

/* Name of unit `index` (numbered as for batUnitRecordGet): batUnitNullName for 0, 0 past the tables. `word` is
 * reused as in batUnitRecordGet. */
char *batUnitNameGet(int index)
{
    unsigned char *table;
    int word;
    int record;

    if (index == 0) {
        return batUnitNullName;
    }
    if (index < BAT_DAT_ENEMY_FIRST) {
        record = index - 1;
        word = BAT_DAT_PARTY_UNITS;
    } else if (index >= BAT_DAT_UNIT_LIMIT) {
        return 0;
    } else {
        record = index - BAT_DAT_ENEMY_FIRST;
        word = BAT_DAT_ENEMY_UNITS;
    }
    word = *(int *)((unsigned char *)&batDat + word);
    table = (unsigned char *)&batDat + word;
    return (char *)table + *(short *)(table + record * BAT_DAT_UNIT_SIZE);
}

/*
 * The tables of bat.dat: dataBatDatLoad (tu008) reads the file to batDat (0x0061D040). Its header holds, from
 * +0x10, 17 words, each the offset of one table from the start of the file; a getter takes the address of its
 * word, subtracts the word's offset in the header and adds the word. With one header struct,
 * (unsigned char *)&batDat + batDat.offsets[n], the getters are two instructions shorter, so each keeps the
 * address of its own word.
 */
extern int tecTableOffset;
extern int effectIdListTableOffset;
extern int slotPairTableOffset;
extern int battleLookupTableOffset;
extern int statusTableOffset;
extern int itemTableOffset;
extern int hitActionTableOffset;
extern int batTable70Offset;
extern int batTable74Offset;
extern int pairLinkTableOffset;
extern int calcLevelTableOffset;
extern int tgtIdTableOffset;
extern int growthTableOffset;
extern int stringTableOffset;

/* Technique records (0x20 bytes); index 0 and below give 0. */
TecRecord *tecRecordGet(int index)
{
    unsigned char *table;

    if (index <= 0) {
        return 0;
    }
    table = (unsigned char *)&tecTableOffset - 16 + tecTableOffset;
    return (TecRecord *) (table + (index - 1) * 32);
}

/* tableKeyRowsOffset: the table of 20-byte key rows; entry numbers start at 1. */
extern int tableKeyRowsOffset;

/* Row of the table for a key: the key is halfword +0x62 of the unit sub record, and
 * the callers read halfword entries of the returned row (tableKeyEntryGet in tu036). */
void *tableKeyRowGet(int key)
{
    unsigned char *table = (unsigned char *)&tableKeyRowsOffset - 20 + tableKeyRowsOffset;

    return table + (key - 1) * 20;
}

/* Records of effectIdListTableOffset (entries numbered from 1). */

short *effectIdListGet(int index)
{
    unsigned char *table;

    if (index <= 0) {
        return 0;
    }
    table = (unsigned char *)&effectIdListTableOffset - 24 + effectIdListTableOffset;
    return (short *) (table + (index - 1) * 6);
}

/* Records of slotPairTableOffset (entries 1..0xF). */

ShortPair *slotPairRowGet(int index)
{
    unsigned char *table;

    if (!(index < 0x10)) {
        return 0;
    }
    table = (unsigned char *)&slotPairTableOffset - 28 + slotPairTableOffset;
    return (ShortPair *) (table + (index - 1) * 12);
}

/* Records of battleLookupTableOffset (entries 1..0x6D). */

BattleLookupEntry *battleLookupGet(int index)
{
    unsigned char *table;

    if (!(index < 0x6E)) {
        return 0;
    }
    table = (unsigned char *)&battleLookupTableOffset - 32 + battleLookupTableOffset;
    return (BattleLookupEntry *) (table + (index - 1) * 12);
}

/* Records of itemTableOffset (entries 1..0x28). */

ItemRecord *itemRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x29)) {
        return 0;
    }
    table = (unsigned char *)&itemTableOffset - 40 + itemTableOffset;
    return (ItemRecord *) (table + (index - 1) * 16);
}

/* Records of statusTableOffset (entries 1..0x68). */

StatusRecord *statusRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x69)) {
        return 0;
    }
    table = (unsigned char *)&statusTableOffset - 36 + statusTableOffset;
    return (StatusRecord *) (table + (index - 1) * 12);
}

/* Records of hitActionTableOffset (entries 1..0x28). */

HitAction *hitActionGet(int index)
{
    unsigned char *table;

    if (!(index < 0x29)) {
        return 0;
    }
    table = (unsigned char *)&hitActionTableOffset - 44 + hitActionTableOffset;
    return (HitAction *) (table + (index - 1) * 16);
}

/* Records of batTable70Offset (entries 1..0x63). */

RecordTexts *func_00A9B580(int index)
{
    unsigned char *table;

    if (!(index < 0x64)) {
        return 0;
    }
    table = (unsigned char *)&batTable70Offset - 48 + batTable70Offset;
    return (RecordTexts *) (table + (index - 1) * 4);
}

/* Records of batTable74Offset (entries 1..0x63). */

TextRecord8 *func_00A9B5B8(int index)
{
    unsigned char *table;

    if (!(index < 0x64)) {
        return 0;
    }
    table = (unsigned char *)&batTable74Offset - 52 + batTable74Offset;
    return (TextRecord8 *) (table + (index - 1) * 8);
}

/* Records of pairLinkTableOffset (entries 1..0x10). */

PairLink *pairLinkGet(int index)
{
    unsigned char *table;

    if (!(index < 0x11)) {
        return 0;
    }
    table = (unsigned char *)&pairLinkTableOffset - 56 + pairLinkTableOffset;
    return (PairLink *) (table + (index - 1) * 16);
}

/* Records of calcLevelTableOffset (entries 1..0x15). */

CalcLevelRecord *calcLevelRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x16)) {
        return 0;
    }
    table = (unsigned char *)&calcLevelTableOffset - 60 + calcLevelTableOffset;
    return (CalcLevelRecord *) (table + (index - 1) * 16);
}

/* Records of tgtIdTableOffset (entries 1..0xA). */

TgtIdRecord *tgtIdRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0xB)) {
        return 0;
    }
    table = (unsigned char *)&tgtIdTableOffset - 64 + tgtIdTableOffset;
    return (TgtIdRecord *) (table + (index - 1) * 16);
}

/* Self-relative table at tableKeyRecordsOffset: 16-byte entries, valid for index 16..143. */
extern int tableKeyRecordsOffset;

struct TableKeyRecord *tableKeyRecordGet(int key)
{
    u32 offset = key - 16;
    unsigned char *table;
    void *result;

    if (!(offset < 0x80U)) {
        return 0;
    }
    table = (unsigned char *)&tableKeyRecordsOffset - 68;
    result = &table[tableKeyRecordsOffset] + key * 16 - 256;
    return result;
}

/* Base of the self-relative table at unitExpTableOffset. */
extern int unitExpTableOffset;

UnitExpEntry *unitExpTblGet(void) {
    return (UnitExpEntry *)((u8 *)&unitExpTableOffset - 72 + unitExpTableOffset);
}

/* Records of growthTableOffset (entries 1..0xF). */

GrowthRecord *growthRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x10)) {
        return 0;
    }
    table = (unsigned char *)&growthTableOffset - 76 + growthTableOffset;
    return (GrowthRecord *) (table + (index - 1) * 32);
}

/* Records of stringTableOffset (entries numbered from 1). */

short *stringOffsetsGet(int index)
{
    unsigned char *table;

    if (index <= 0) {
        return 0;
    }
    table = (unsigned char *)&stringTableOffset - 80 + stringTableOffset;
    return (short *) (table + (index - 1) * 2);
}

/* The *TextGet getters below decrement index after the table call: decremented first, the copy of index and the
 * decrement merge into one instruction, unlike the original. */

/* Text `field` of record `index` (numbered from 1) of the battleLookupGet table; 0 for another field. */
char *battleLookupTextGet(int index, int field)
{
    BattleLookupEntry *table;

    table = battleLookupGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the itemRecordGet table; 0 for another field. */
char *itemTextGet(int index, int field)
{
    ItemRecord *table;

    table = itemRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the statusRecordGet table; 0 for another field. */
char *statusTextGet(int index, int field)
{
    StatusRecord *table;

    table = statusRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the hitActionGet table; 0 for another field. */
char *hitActionTextGet(int index, int field)
{
    HitAction *table;

    table = hitActionGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the func_00A9B580 table; 0 for another field. */
char *func_00A9B950(int index, int field)
{
    RecordTexts *table;

    table = func_00A9B580(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the func_00A9B5B8 table; 0 for another field. */
char *func_00A9B9C8(int index, int field)
{
    TextRecord8 *table;

    table = func_00A9B5B8(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the pairLinkGet table; 0 for another field. */
char *pairLinkTextGet(int index, int field)
{
    PairLink *table;

    table = pairLinkGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the calcLevelRecordGet table; 0 for another field. */
char *calcLevelTextGet(int index, int field)
{
    CalcLevelRecord *table;

    table = calcLevelRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the tgtIdRecordGet table; 0 for another field. */
char *tgtIdTextGet(int index, int field)
{
    TgtIdRecord *table;

    table = tgtIdRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* The string table: record 1 of the table at stringTableOffset is its offset list; each offset counts from the list. */
char *tableStringGet(int index, int mode)
{
    int base;

    /* The list address is added as an int: with char * arithmetic (base + offset, &base[offset],
     * offsets[index - 1]) the original keeps base in another register. */
    base = (int)stringOffsetsGet(1);
    if (mode == 1) {
        return (char *)(base + *(short *)(((index - 1) << 1) + base));
    }
    return 0;
}
