/*
 * OV01 original TU 42: 0x00a9b280..0x00a9c610 (49 functions)
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

/* Message object made by msgPutObject: an ov01 task record (ov01TaskClaim) whose update is msgObjRelease. */
struct MsgObject {
    int flags;                                  /* +0x00: the task record's flags */
    void (*update)(struct MsgObject *object);   /* +0x04: msgObjRelease */
    void (*draw)(struct MsgObject *object);     /* +0x08: none */
    int ref_count;                              /* +0x0C: frames the message stays (0: until its voice is done) */
    int mode;                                   /* +0x10: MSG_MODE_VOICE when ref_count was 0 */
    TextEntry *handle;                          /* +0x14: the text entry (textEntryAlloc) */
};

/* MsgObject.mode of a message that stays until its voice is done. */
#define MSG_MODE_VOICE 1
/* MsgObject.mode of a message that stays ref_count frames. */
#define MSG_MODE_TIMED 0

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

/* Message globals: msgPages is the text page list {text, 0} that msgPutObject passes to textEntryAlloc; msgBusy
 * is the busy flag (1 while a message is outstanding). */
static unsigned char *msgPages[2];
static int msgBusy;

/* Clears the message request block and the busy flag. */
void msgReset(void) {
    msgPages[0] = 0;
    msgBusy = 0;
}

/* A script message word (SbMessage.args) carries two values: the second one above this shift. */
#define SCRIPT_ARG_HIGH_SHIFT 16

/*
 * Sends event as a kind message to the unit's script (the battle block's spawnArgC): the unit index, and for
 * UNIT_SCRIPT_KIND_PAIR the second unit's index in the high half and words[0] | words[1] << 16.
 */
void unitScriptPairEventSend(int kind, UnitScriptPairEvent *event)
{
    SbMessage message;

    message.kind = kind;
    message.args[0] = unitTblIndexOf(event->unit);
    if (kind == UNIT_SCRIPT_KIND_PAIR) {
        message.args[0] |= unitTblIndexOf(event->other) << SCRIPT_ARG_HIGH_SHIFT;
        /* In two steps: the one-expression form loads the words and allocates the registers differently. */
        message.args[1] = event->words[0];
        message.args[1] |= event->words[1] << SCRIPT_ARG_HIGH_SHIFT;
    }
    if (event->unit->stats->spawnArgC != 0) {
        sbThreadEnqueue(ov01ScriptKeyGet(), event->unit->stats->spawnArgC, &message, 0);
    }
}

/*
 * Sends event (the unit and three words) as a kind message to the unit's script (the battle block's spawnArgD),
 * or, without a unit or script, to the camera setting's script. The message's first word is the unit index with
 * words[0] in the high half, its second words[1] | words[2] << 16.
 */
void unitScriptEventSend(int kind, UnitScriptEvent *event)
{
    SbMessage message;
    int entry;
    int script;

    message.kind = kind;
    message.args[0] = unitTblIndexOf(event->unit) | (event->words[0] << 16);
    message.args[1] = event->words[1] | (event->words[2] << 16);
    entry = event->unit != 0 && (script = event->unit->stats->spawnArgD) != 0 ? script : cameraSettingGet();
    if (entry != 0) {
        sbThreadEnqueue(ov01ScriptKeyGet(), entry, &message, 0);
    }
}

/* The wait flag thinkDispatch stores for CMD_KIND_CODE_WAIT: unitCmdCodeWait then also waits for unitIsDone. */
#define THINK_CODE_WAIT_UNTIL_DONE 1

/* thinkDispatch: queues the command `command` (CMD_KIND_*) for the unit; natives thinkSet and thinkSet2. A technique
 * takes entry `arg` of the unit's key row (entry 0 when that one is 0); a lookup command is queued only when
 * calcEntryUsable allows battle lookup record `arg`, else a CMD_KIND_NONE record. */
void thinkDispatch(Ov01Object *unit, int command, int arg, Ov01Object *target)
{
    /* Block-scope redeclaration (codegen): calls through the declaration of tableKeyRowGet, which is defined earlier
     * in this file, are marked as not throwing; calls through this one are not, so the delay-slot filler keeps $a0
     * live past the second call and annuls the case 1 branch slot (bnel) like the original. */
    extern void *tableKeyRowGet(int key);
    CmdListBody body;
    short code;

    switch (command) {
    case CMD_KIND_TECHNIQUE:
        body.command = command;
        /* `*&` (codegen): key shares an anonymous union in StatBlock; the plain access gets alias set 0, waits for the
         * body.command store and swaps the unit and command registers. */
        code = ((TableKeyRow *)tableKeyRowGet(*&unit->stats->key))->entries[arg];
        if (code == 0) {
            code = ((TableKeyRow *)tableKeyRowGet(unit->stats->key))->entries[0];
        }
        body.code = code;
        body.arg.target = (int)target;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_LOOKUP:
        code = battleLookupGet(arg)->tecIndex;
        if (calcEntryUsable(unit, arg) != 0) {
            body.command = command;
            body.code = code;
            body.arg.target = (int)target;
            body.pairIndex = arg;
        } else {
            body.command = CMD_KIND_NONE;
        }
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_SNAPSHOT:
        body.command = command;
        body.code = arg;
        cmdListSet(unit, &body);
        break;
    /* Cases 0, 2 and 7 as three bodies (codegen): jump2 merges them again; one shared case body gives other code
     * (22 instructions off). */
    case CMD_KIND_NONE:
        body.command = command;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_WAIT:
        body.command = command;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_LEAVE:
        body.command = command;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_CODE_WAIT:
        body.command = command;
        body.code = arg;
        body.arg.target = THINK_CODE_WAIT_UNTIL_DONE;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_REMOVE:
        body.command = command;
        cmdListSet(unit, &body);
        break;
    }
}

/* thinkSendEnd: queues the think-end record for the unit (cmdListSet), then ends the
 * target selection mode (tgtSelectModeEnd); used by thinkSet and thinkSetEnd. */
void thinkSendEnd(Ov01Object *unit)
{
    CmdListBody command;

    command.command = CMD_KIND_THINK_END;
    cmdListSet(unit, &command);
    tgtSelectModeEnd();
}

/*
 * Native atkTbl: fills table with the attack candidates of unit: for each of the first `rows` techniques of its
 * key row (tableKeyRowGet) and each unit of the side the technique targets that is not flagged 4 and that the
 * technique can affect (battleEffectApplies), one row {technique slot, unit index}. Returns the row count, which
 * is also stored in table->count.
 */
int atkTblBuild(Ov01Object *unit, AtkTbl *table, int rows)
{
    Ov01Object **units;
    int *word;
    unsigned char *keys;
    /* Byte offset of the slot in the key row: with keys[row] the address add takes the other operand order. */
    int offset;
    int row;
    int tec;
    int count;
    int i;
    int n;

    word = &table->count;
    n = 1;
    for (row = 0; row < rows; row++) {
        keys = tableKeyRowGet(unit->stats->key);
        offset = row * sizeof(short);
        tec = *(short *)(keys + offset);
        if (tec == 0) {
            continue;
        }
        count = unitTblGet(tgtSideOf(unit, tecRecordGet(tec)->flags), &units);
        for (i = 0; i < count; i++) {
            if (units[i] == 0) {
                continue;
            }
            if (units[i]->flags & UNIT_FLAG_DOWN) {
                continue;
            }
            if (!battleEffectApplies(unit, tec, units[i])) {
                continue;
            }
            word[n++] = row;
            word[n++] = unitTblIndexOf(units[i]);
        }
    }
    return table->count = (n - 1) / 2;
}

/* Field `field` of row `index` of the attack table (native atkTblGet). */
int atkTblFieldGet(int field, AtkTbl *table, int index)
{
    /* index[table->rows] rather than table->rows[index]: the plain form swaps the operands of the addu that forms
     * the row address (0x00A9C084 and 0x00A9C090); only the reversed form keeps the original order. */
    switch (field) {
    case 1:
        return index[table->rows].key;
    case 2:
        return index[table->rows].value;
    }
    return table->count;
}

/* Parameter field of the unit (UNIT_PARA_*); 0 for no unit or an unknown field (native unitParaGet). */
int unitParaFieldGet(struct Ov01Object *unit, int field)
{
    StatBlock *stats;

    if (unit == 0) {
        return 0;
    }
    stats = unit->stats;
    switch (field) {
    case UNIT_PARA_MAX_HP:
        return stats->maxHp;
    case UNIT_PARA_MAX_EP:
        return stats->maxEp;
    case UNIT_PARA_PHYS_ATTACK:
        return stats->physAttack;
    case UNIT_PARA_PHYS_DEFENCE:
        return stats->physDefence;
    case UNIT_PARA_MAG_ATTACK:
        return stats->magAttack;
    case UNIT_PARA_MAG_DEFENCE:
        return stats->magDefence;
    case UNIT_PARA_HIT_RATE:
        return stats->hitRate;
    case UNIT_PARA_EVADE_RATE:
        return stats->evadeRate;
    case UNIT_PARA_VALUE_BIAS:
        return stats->valueBias;
    case UNIT_PARA_HP:
        return stats->hp;
    case UNIT_PARA_EP:
        return stats->ep;
    case UNIT_PARA_KEY:
        return stats->key;
    case UNIT_PARA_STATE70:
        return stats->state70;
    case UNIT_PARA_LEVEL:
        return stats->level;
    case UNIT_PARA_EXP_LEVEL:
        return stats->expLevel;
    case UNIT_PARA_FLAGS:
        return stats->flags;
    case UNIT_PARA_POS_INDEX:
        return stats->posIndex;
    case UNIT_PARA_LIVE_ROW ... UNIT_PARA_SLOT_VALUE - 1:
        return stats->liveRows[field - UNIT_PARA_LIVE_ROW];
    case UNIT_PARA_SLOT_VALUE ... UNIT_PARA_SLOT_ROLL - 1:
        return stats->slotValue[field - UNIT_PARA_SLOT_VALUE];
    case UNIT_PARA_SLOT_ROLL ... UNIT_PARA_CURRENT - 1:
        return stats->slotRoll[field - UNIT_PARA_SLOT_ROLL];
    case UNIT_PARA_CURRENT:
        return stats->current;
    }
    return 0;
}

/* Sets parameter field of the unit (UNIT_PARA_*, all but UNIT_PARA_STATE70) to value (native unitParaSet). */
void unitParaFieldSet(struct Ov01Object *unit, int field, int value)
{
    StatBlock *stats;

    if (unit == 0) {
        return;
    }
    stats = unit->stats;
    switch (field) {
    case UNIT_PARA_MAX_HP:
        stats->maxHp = value;
        break;
    case UNIT_PARA_MAX_EP:
        stats->maxEp = value;
        break;
    case UNIT_PARA_PHYS_ATTACK:
        stats->physAttack = value;
        break;
    case UNIT_PARA_PHYS_DEFENCE:
        stats->physDefence = value;
        break;
    case UNIT_PARA_MAG_ATTACK:
        stats->magAttack = value;
        break;
    case UNIT_PARA_MAG_DEFENCE:
        stats->magDefence = value;
        break;
    case UNIT_PARA_HIT_RATE:
        stats->hitRate = value;
        break;
    case UNIT_PARA_EVADE_RATE:
        stats->evadeRate = value;
        break;
    case UNIT_PARA_VALUE_BIAS:
        stats->valueBias = value;
        break;
    case UNIT_PARA_HP:
        stats->hp = value;
        break;
    case UNIT_PARA_EP:
        stats->ep = value;
        break;
    case UNIT_PARA_KEY:
        stats->key = value;
        break;
    case UNIT_PARA_LEVEL:
        stats->level = value;
        break;
    case UNIT_PARA_EXP_LEVEL:
        stats->expLevel = value;
        break;
    case UNIT_PARA_FLAGS:
        stats->flags = value;
        break;
    case UNIT_PARA_POS_INDEX:
        stats->posIndex = value;
        break;
    case UNIT_PARA_LIVE_ROW ... UNIT_PARA_SLOT_VALUE - 1:
        stats->liveRows[field - UNIT_PARA_LIVE_ROW] = value;
        break;
    case UNIT_PARA_SLOT_VALUE ... UNIT_PARA_SLOT_ROLL - 1:
        stats->slotValue[field - UNIT_PARA_SLOT_VALUE] = value;
        break;
    case UNIT_PARA_SLOT_ROLL ... UNIT_PARA_CURRENT - 1:
        stats->slotRoll[field - UNIT_PARA_SLOT_ROLL] = value;
        break;
    case UNIT_PARA_CURRENT:
        stats->current = value;
        break;
    }
}

void *memset();

/* Effect request keyed by (high, low), head taken from the owner when one is given; used by native createEffect. */
void effectSendOwned(int high, int low, StatOwner *owner) {
    struct EffectRequest request;

    memset(&request, 0, sizeof(request));
    if (owner != 0) {
        request.head = owner->unit;
    }
    request.key = (high << 0xC) | low;
    sefStartEffectDefault(&request);
}


/* Effect request keyed by (high, low) that carries a value; used by native createEffectPos. */
void effectSendValue(int high, int low, Vector4 *position) {
    struct EffectRequest request;

    memset(&request, 0, sizeof(request));
    request.key = (high << 0xC) | low;
    sefStartEffectWithValue(&request, (int) position);
}

/* The 32-byte technique record of the tecRecordGet table as the native
 * tecParaGet reads it (fields 0 to 3). */

/* Field `field` of technique `index` (native tecParaGet); 0 for technique 0
 * or another field. */
int tecParaFieldGet(int index, int field)
{
    TecRecord *tec;

    if (index == 0) {
        return 0;
    }
    tec = tecRecordGet(index);
    switch (field) {
    case 0:
        return tec->flags;
    case 1:
        return tec->type;
    case 2:
        return tec->kindBits.kind;
    case 3:
        return tec->actionBits;
    }
    return 0;
}

/* msgPutObject (core of native msgPut): makes a MsgObject from ov01TaskClaim, installs msgObjRelease as its
 * release callback, submits the request block through textEntryAlloc and sets the busy flag. */
void msgObjRelease(struct MsgObject *object);

struct MsgObject *msgPutObject(int x, int y, char *text, int frames, int width, int height)
{
    struct MsgObject *object;

    object = ov01TaskClaim();
    if (object == 0) {
        return 0;
    }
    object->update = msgObjRelease;
    object->ref_count = frames;
    object->draw = 0;
    if (frames == 0) {
        object->mode = MSG_MODE_VOICE;
    } else {
        object->mode = MSG_MODE_TIMED;
    }
    msgPages[0] = (unsigned char *)text; /* text entry pages are unsigned char (tu124) */
    msgPages[1] = 0;
    object->handle = textEntryAlloc(x, y, width, height, 0, msgPages);
    msgBusy = 1;
    return object;
}

/* Release callback of a MsgObject (stored at +0x4 by msgPutObject). A mode-1 object is freed when
 * textEntryPlayVoice reports its handle done; otherwise the reference count is dropped. The busy flag follows. */
void msgObjRelease(struct MsgObject *object) {
    TextEntry *handle;
    int remaining;
    int remove;

    remove = 0;
    handle = object->handle;
    if (object->mode == 1) {
        if (textEntryPlayVoice(handle) != 0) {
            remove = 1;
        }
    } else {
        remaining = object->ref_count - 1;
        object->ref_count = remaining;
        remove = remaining < 1;
    }
    if (remove == 1) {
        objRemove(handle);
        ov01TaskRelease(object);
        msgBusy = 0;
        return;
    }
    msgBusy = 1;
}

/* 1 when no message is outstanding (native msgPutWait). */
int msgIsIdle(void) {
    return msgBusy == 0;
}

/* Reads one of the three bytes of gauge `id` (slot order as in partyDataSetSlot); 0 for another slot. */
int partyDataGetSlot(int id, int slot)
{
    struct BoostGauge *gauge;

    gauge = boostGaugeSelect(id);
    switch (slot) {
    case 0:
        return gauge->stock;
    case 1:
        return gauge->progress;
    case 2:
        return gauge->maxStock;
    }
    return 0;
}

/* Writes one of the three slot bytes of record `id`; slot 0 is stock, slot 1 progress, slot 2 maxStock. */
void partyDataSetSlot(int id, int slot, int value) {
    struct BoostGauge *gauge;

    gauge = boostGaugeSelect(id);
    switch (slot) {
    case 0:
        gauge->stock = (signed char) value;
        return;
    case 1:
        gauge->progress = (signed char) value;
        break;
    case 2:
        gauge->maxStock = (signed char) value;
        break;
    }
}
