/* The bat.dat table getters. */
#include "common.h"
#include "shared.h"
#include "main/sef.h"
#include "main/choice_menu.h"
#include "ov01/ov01_task.h"
#include "ov01/effect_pos.h"
#include "ov01/data_file_load.h"
#include "ov01/unit_tbl.h"
#include "ov01/cmd_exec.h"
#include "ov01/tgt_select.h"
#include "ov01/choice_menu.h"
#include "ov01/calc.h"
#include "ov01/scene_script.h"
#include "ov01/data_record_get.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"

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

int dataMotNameIndexGet(int value) {
    return value;
}

/* Returns its argument unchanged. Caller: dataVoiceSetLoad. */
int dataVoiceNameIndexGet(int value) {
    return value;
}

/* bat.dat as dataBatDatLoad (data_file_load.c) reads it. The header word at byte BAT_DAT_PARTY_UNITS holds the offset
 * of the party unit table (units 1..15), the one at BAT_DAT_ENEMY_UNITS that of the enemy unit table (units 16..0x8C);
 * both have BAT_DAT_UNIT_SIZE-byte records whose first halfword is the offset of the unit's name from the table. */

#define BAT_DAT_PARTY_UNITS 0x8
#define BAT_DAT_ENEMY_UNITS 0xC
#define BAT_DAT_ENEMY_FIRST 16
#define BAT_DAT_UNIT_LIMIT 0x8D
#define BAT_DAT_UNIT_SIZE 0x5C

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
        record = manWorkGet()->slotIndex - 1;
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
 * The tables of bat.dat: dataBatDatLoad (data_file_load.c) reads the file to batDat (0x0061D040). Its header holds,
 * from +0x10, 17 words, each the offset of one table from the start of the file; a getter takes the address of its
 * word, subtracts the word's offset in the header and adds the word. With one header struct, (unsigned char *)&batDat +
 * batDat.offsets[n], the getters are two instructions shorter, so each keeps the address of its own word.
 */
extern int tecTableOffset;
extern int effectIdListTableOffset;
extern int slotPairTableOffset;
extern int battleLookupTableOffset;
extern int statusTableOffset;
extern int itemTableOffset;
extern int hitActionTableOffset;
extern int batSecretKeyTableOffset;
extern int batKeyItemTableOffset;
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
 * the callers read halfword entries of the returned row (tableKeyEntryGet in choice_menu.c). */
TableKeyRow *tableKeyRowGet(int key)
{
    unsigned char *table = (unsigned char *)&tableKeyRowsOffset - 20 + tableKeyRowsOffset;

    return (TableKeyRow *)(table + (key - 1) * 20);
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

EtherRecord *etherRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x6E)) {
        return 0;
    }
    table = (unsigned char *)&battleLookupTableOffset - 32 + battleLookupTableOffset;
    return (EtherRecord *) (table + (index - 1) * 12);
}

/* Records of itemTableOffset (entries 1..0x28). */

EsAccessoryRecord *accessoryRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x29)) {
        return 0;
    }
    table = (unsigned char *)&itemTableOffset - 40 + itemTableOffset;
    return (EsAccessoryRecord *) (table + (index - 1) * 16);
}

/* Skill or equipment effect records of statusTableOffset (entries 1..0x68). */

SkillRecord *skillRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x69)) {
        return 0;
    }
    table = (unsigned char *)&statusTableOffset - 36 + statusTableOffset;
    return (SkillRecord *) (table + (index - 1) * 12);
}

/* Records of hitActionTableOffset (entries 1..0x28). */

UsableItemRecord *usableItemRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x29)) {
        return 0;
    }
    table = (unsigned char *)&hitActionTableOffset - 44 + hitActionTableOffset;
    return (UsableItemRecord *) (table + (index - 1) * 16);
}

/* Records of batSecretKeyTableOffset: the file holds 31 records; the getter checks index < 0x64 as the original does. */

RecordTexts *dataBoxSecretKeyRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x64)) {
        return 0;
    }
    table = (unsigned char *)&batSecretKeyTableOffset - 48 + batSecretKeyTableOffset;
    return (RecordTexts *) (table + (index - 1) * 4);
}

/* Records of batKeyItemTableOffset: the file holds 107 records; the getter checks index < 0x64 as the original does. */

TextRecord8 *dataBoxKeyItemRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x64)) {
        return 0;
    }
    table = (unsigned char *)&batKeyItemTableOffset - 52 + batKeyItemTableOffset;
    return (TextRecord8 *) (table + (index - 1) * 8);
}

/* Records of pairLinkTableOffset (entries 1..0x10). */

DoubleTechRecord *pairLinkGet(int index)
{
    unsigned char *table;

    if (!(index < 0x11)) {
        return 0;
    }
    table = (unsigned char *)&pairLinkTableOffset - 56 + pairLinkTableOffset;
    return (DoubleTechRecord *) (table + (index - 1) * 16);
}

/* Records of calcLevelTableOffset (entries 1..0x15). */

EsSpecialRecord *attackLevelRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0x16)) {
        return 0;
    }
    table = (unsigned char *)&calcLevelTableOffset - 60 + calcLevelTableOffset;
    return (EsSpecialRecord *) (table + (index - 1) * 16);
}

/* Records of tgtIdTableOffset (entries 1..0xA). */

SpecialAttackRecord *attackTargetRecordGet(int index)
{
    unsigned char *table;

    if (!(index < 0xB)) {
        return 0;
    }
    table = (unsigned char *)&tgtIdTableOffset - 64 + tgtIdTableOffset;
    return (SpecialAttackRecord *) (table + (index - 1) * 16);
}

/* Self-relative table at tableKeyRecordsOffset: 16-byte entries, valid for index 16..143. */
extern int tableKeyRecordsOffset;

struct EnemyRewardRecord *enemyRewardRecordGet(int key)
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

UnitExpEntry *dataExpTblGet(void) {
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

/* Text `field` of record `index` (numbered from 1) of the etherRecordGet table; 0 for another field. */
char *battleLookupTextGet(int index, int field)
{
    EtherRecord *table;

    table = etherRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the accessoryRecordGet table; 0 for another field. */
char *itemTextGet(int index, int field)
{
    EsAccessoryRecord *table;

    table = accessoryRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the skillRecordGet table; 0 for another field. */
char *statusTextGet(int index, int field)
{
    SkillRecord *table;

    table = skillRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the usableItemRecordGet table; 0 for another field. */
char *hitActionTextGet(int index, int field)
{
    UsableItemRecord *table;

    table = usableItemRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the dataBoxSecretKeyRecordGet table; 0 for another field. */
char *dataBoxCategory4TextGet(int index, int field)
{
    RecordTexts *table;

    table = dataBoxSecretKeyRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the dataBoxKeyItemRecordGet table; 0 for another field. */
char *dataBoxCategory3TextGet(int index, int field)
{
    TextRecord8 *table;

    table = dataBoxKeyItemRecordGet(1);
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
    DoubleTechRecord *table;

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

/* Text `field` of record `index` (numbered from 1) of the attackLevelRecordGet table; 0 for another field. */
char *calcLevelTextGet(int index, int field)
{
    EsSpecialRecord *table;

    table = attackLevelRecordGet(1);
    index--;
    switch (field) {
    case RECORD_TEXT_NAME:
        return (char *)table + table[index].texts.text[RECORD_TEXT_NAME];
    case RECORD_TEXT_INFO:
        return (char *)table + table[index].texts.text[RECORD_TEXT_INFO];
    }
    return 0;
}

/* Text `field` of record `index` (numbered from 1) of the attackTargetRecordGet table; 0 for another field. */
char *tgtIdTextGet(int index, int field)
{
    SpecialAttackRecord *table;

    table = attackTargetRecordGet(1);
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
