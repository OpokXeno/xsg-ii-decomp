/*
 * OV01 original TU 41: 0x00a9b120..0x00a9b280 (4 functions)
 */
#include "common.h"
#include "main/tu110.h"
#include "ov01/tu012.h"
#include "ov01/tu023.h"
#include "ov01/tu036.h"
#include "ov01/tu041.h"

/* One 16-byte record of the effect position table: x, y, z in 1/100 units (createEffectPos), then a word that is
 * EFFECT_POS_END in the record that ends the table. */
typedef struct EffectPosRecord {
    int position[3]; /* +0x00 */
    int end;         /* +0x0C */
} EffectPosRecord;

#define EFFECT_POS_END -1

/* Base of the effect position table, set by effectPosTableSet. */
static EffectPosRecord *effectPosTable;
/* Number of valid position records (the index of the end record). */
static int effectPosCount;

/* Sets the effect position table: the base goes to effectPosTable, the number of records before the end record to
 * effectPosCount. */
void effectPosTableSet(EffectPosRecord *table)
{
    effectPosTable = table;
    effectPosCount = 0;
    while (table[effectPosCount].end != EFFECT_POS_END) {
        effectPosCount++;
    }
}

/*
 * Returns position record `index` (x, y, z in 1/100 units, read by createEffectPos), or 0 when index is past the
 * last record.
 */
int *effectPosGet(int index) {
    if (!(index < effectPosCount)) {
        return 0;
    }
    return effectPosTable[index].position;
}

/* Number of valid effect position records. */
int effectPosGetCount(void) {
    return effectPosCount;
}

/* Unit effect code that switches a party member to the file of unitEffectFileIds. */
#define UNIT_EFFECT_FILE_SWAP 0x3F

/* Name of the empty unit: the string "NULL" of tu036 (D_00ABA698), read by tu042 as batUnitNullName. */
extern char D_00ABA698[];
char *batUnitNullName = D_00ABA698;

/* Model file ids by unit key: party members with UNIT_EFFECT_FILE_SWAP (key - UNIT_KEY_PARTY_FIRST, 0 for none),
 * the large units (key - UNIT_KEY_LARGE_FIRST) and the other data ids (key - UNIT_KEY_OTHER_FIRST). */
static int unitEffectFileIds[8] = {
    0x00010013, 0x0001000D, 0x00010006, 0x00010028, 0x00010022, 0x0001001E, 0x00010019, 0x00000000
};
static int largeUnitFileIds[6] = {
    0x00014002, 0x00014003, 0x00014005, 0x00000000, 0x00000000, 0x00000000
};
static int otherUnitFileIds[126] = {
    0x0001803D, 0x0001803E, 0x0001803F, 0x00018040, 0x00018041, 0x00018042,
    0x00018043, 0x00018044, 0x00018045, 0x00018046, 0x00018047, 0x00018048,
    0x00018049, 0x0001804A, 0x0001804B, 0x0001804C, 0x0001804D, 0x0001804E,
    0x0001804F, 0x0001800D, 0x0001800E, 0x0001800F, 0x00018010, 0x00018011,
    0x00018012, 0x00018013, 0x00018014, 0x00018015, 0x00018016, 0x00018025,
    0x00018026, 0x0001802F, 0x00018030, 0x00018039, 0x0001803A, 0x0001803B,
    0x00018027, 0x00018004, 0x00018005, 0x00018008, 0x0001800A, 0x0001800B,
    0x0001800C, 0x0001801C, 0x00018022, 0x0001802A, 0x0001802B, 0x0001803C,
    0x00018031, 0x00018052, 0x00018003, 0x00018009, 0x0001801A, 0x0001801B,
    0x0001801D, 0x0001801E, 0x0001801F, 0x00018028, 0x00018029, 0x0001802C,
    0x00018032, 0x00018033, 0x00018034, 0x00018035, 0x00018036, 0x00018001,
    0x00018002, 0x00018006, 0x00018007, 0x00018020, 0x00018021, 0x00018023,
    0x00018024, 0x0001802D, 0x0001802E, 0x00018037, 0x00018038, 0x00018017,
    0x00018018, 0x00018019, 0x00018001, 0x00018002, 0x00018005, 0x00018011,
    0x0001800B, 0x00018020, 0x00018021, 0x00018023, 0x0001800C, 0x00018031,
    0x0001803D, 0x0001803E, 0x0001803F, 0x00018040, 0x00019001, 0x00019002,
    0x00019003, 0x00019004, 0x00019005, 0x00019006, 0x00019007, 0x00019008,
    0x00019009, 0x0001900A, 0x0001900B, 0x0001900C, 0x0001900D, 0x0001900E,
    0x0001900F, 0x00019010, 0x00019011, 0x00019012, 0x00019013, 0x00019014,
    0x00019015, 0x00019016, 0x00019017, 0x00019018, 0x00019019, 0x0001901A,
    0x0001901B, 0x0001901C, 0x0001901D, 0x0001901E, 0x0001901F, 0x00000000
};

/* Model file id of unit `key` (tu008 builds the file name from it): a party member's cloth id from ptStatusBlock
 * (or its unitEffectFileIds entry under UNIT_EFFECT_FILE_SWAP), a table entry for the others, the gattai code past
 * the data ids. */
int unitFileIdGet(int key)
{
    int fileId;
    int effectFileId;

    if (key < UNIT_KEY_LARGE_FIRST) {
        fileId = ptGetStatusBlock()->clothIds[key - UNIT_KEY_PARTY_FIRST];
        if (battleUnitHasEffectCode(key, UNIT_EFFECT_FILE_SWAP)) {
            effectFileId = unitEffectFileIds[key - UNIT_KEY_PARTY_FIRST];
            if (effectFileId != 0) {
                fileId = effectFileId;
            }
        }
    } else if (key < UNIT_KEY_OTHER_FIRST) {
        fileId = largeUnitFileIds[key - UNIT_KEY_LARGE_FIRST];
    } else if (key >= UNIT_KEY_COUNT) {
        fileId = gattaiCodeGet();
    } else {
        fileId = otherUnitFileIds[key - UNIT_KEY_OTHER_FIRST];
    }
    return fileId;
}
