#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "main/pad_raw.h"
#include "ov01/tu004.h"
#include "ov01/tu005.h"
#include "ov01/tu012.h"
#include "ov01/tu022.h"
#include "ov01/tu023.h"
#include "ov01/tu036.h"
#include "ov01/tu042.h"
#include "ov01/tu056.h"
#include "ov01/tu059.h"
#include "ov01/tu060.h"
#include "ov01/tu061.h"

/* Uninitialized storage of this file, in address order (only this file names these labels). */
static int dataKeyRegistry[8];
static int dataKeyCount;
static int menuWepSpPanelCursor;
static int menuWepSpPanelRowCount;
static int menuTargetListCursor;
static int padMode;
static short padData;

#define VIRTUAL_PAD_BIT_COUNT 14

/* Bits of DataObject.flags. */
enum DataFlag {
    DATA_FLAG_MATCHED = 0x1,
    DATA_FLAG_HIDDEN = 0x2
};

extern unsigned short padConvTbl[];


/* Global block dataState: the cursor fields (0xA4-0xAC) and the mode word (0xB4)
 * share it; the block is 0xBC bytes, cleared by dataStateReset. units[] holds the selected entries,
 * entry[] their value words (entry[index] is the cursor's current value). */
struct DataState {
    /* 0x00 */ void *units[8];
    /* 0x20 */ unsigned char unmodeled_20[4];
    /* 0x24 */ int entry[32];
    /* 0xA4 */ int count;
    /* 0xA8 */ int index;
    /* 0xAC */ int prev_index;
    /* 0xB0 */ int entryMode;
    /* 0xB4 */ int mode;
    /* 0xB8 */ int prev_mode;
};

extern struct DataState dataState;
extern struct DataObject *dataObject;
void *dataObjCreate(f32 x, f32 y, f32 z);
void dataObjDrawSprite(struct DataObject *obj, u32 *packet);
void menuTexFlush(void);
void grGpInit(void *packet);
void grPacketSend(void *packet);
int sefActorMulLocalVector(void *actor, int input, void *output, int apply);
void *memset(void *dst, int value, unsigned int size);

/* Virtual pad reset: pad mode and pad data, then the two counter pairs (prototype in tu022.h). */
void dataVPadReset(void) {
    dataVPadModeSet(0);
    dataCountersClear();
    menuWepSpPanelReset();
}

/* Clears the counters; battleSelectStep passes unit, it is not used. */
void func_00A88E98(StatOwner *unit) {
    dataCountersClear();
}

/* Word view of DataObject.flags whose store is alias set 0 (the union spelling of the bit set). */
typedef union DataFlagsView {
    int value;
    unsigned int bits;
} DataFlagsView;

struct DataKeyEntry;
int dataKeyRegister(StatOwner *unit, unsigned short key);
int dataKeyFind(StatOwner *unit, struct DataKeyEntry **found);
int dataKeyValueGet(StatOwner *unit, int key);

int dataKeyInputStep(StatOwner *unit)
{
    struct DataKeyEntry *found;
    unsigned short pressed;
    int key;

    if (unit->stats->keyUnsigned - 1U < 10U) {
        if (unit->stats->keysSpent > 0 &&
            (unit->stats->level == 0 || (unit->flags & 0x800))) {
            dataCountersClear();
            return DATA_KEY_CANCEL;
        }
    } else if (unit->stats->keysSpent > 0) {
        dataCountersClear();
        return DATA_KEY_CANCEL;
    }
    pressed = dataPadRead(1);
    if (pressed == 0) {
        return 0;
    }
    if (pressed == PAD_CROSS) {
        dataCountersClear();
        return DATA_KEY_CANCEL;
    }
    if (dataKeyRegister(unit, pressed) == 0) {
        return DATA_KEY_NONE;
    }
    key = dataKeyFind(unit, &found);
    if (key == DATA_KEY_NONE) {
        if (dataKeyCount > 0) {
            dataKeyCount--;
        }
        return DATA_KEY_NONE;
    }
    if (dataKeyCheck(unit, key, tgtUnitGet()) != 0) {
        if (dataKeyCount > 0) {
            dataKeyCount--;
        }
        return DATA_KEY_NONE;
    }
    if (!(unit->stats->keyUnsigned - 1U < 10U && key - 1U < 2U)) {
        unit->stats->keysSpent++;
    }
    unitLevelAdd(unit, -dataKeyValueGet(unit, key));
    return key;
}

int calcStatGet(StatOwner *unit, int arg1, int arg2);

int dataKeyCheck(StatOwner *unit, int key, void *tgt)
{
    int keyEntry;
    int budget;

    if (unit->stats->keyUnsigned - 1U < 10U && key - 1U < 2U) {
        int pairEntry;
        int second;

        if (key == 1) {
            pairEntry = tableKeyEntryGet(unit, 3, tgt);
            if (battleEffectApplies(unit, pairEntry, tgt) != 0) {
                return 0;
            }
            pairEntry = tableKeyEntryGet(unit, 4, tgt);
            return battleEffectApplies(unit, pairEntry, tgt) == 0;
        }
        pairEntry = tableKeyEntryGet(unit, 6, tgt);
        if (battleEffectApplies(unit, pairEntry, tgt) != 0) {
            return 0;
        }
        pairEntry = tableKeyEntryGet(unit, 5, tgt);
        return battleEffectApplies(unit, pairEntry, tgt) == 0;
    }
    keyEntry = tableKeyEntryGet(unit, key, tgt);
    if (keyEntry == 0) {
        return 2;
    }
    if (unit->stats->keyUnsigned - 1U < 10U) {
        budget = unit->stats->level;
        if (budget < dataKeyValueGet(unit, key)) {
            return 1;
        }
    }
    if (battleEffectApplies(unit, keyEntry, tgt) == 0) {
        return 1;
    }
    if (calcStatGet(unit, 2, 0x200) != 0 && key - 1U < 10U) {
        return 1;
    }
    return 0;
}

/* One 12-byte entry of the key table at dataKeyTable: a zero used word ends the table. */
struct DataKeyEntry {
    unsigned short *keys;
    int value;
    int key;
};

extern struct DataKeyEntry dataKeyTable[];

int dataKeyValueGet(StatOwner *unit, int key)
{
    int index;
    int keyValue = 0;
    struct DataKeyEntry *keyTable;

    if (key == 0 || key == DATA_KEY_NONE || key == DATA_KEY_CANCEL) {
        return 0;
    }
    if (key < 11 && unit->stats->keyUnsigned - 1U < 10U) {
        keyTable = dataKeyTable;
        for (index = 0; keyTable[index].keys != 0; index++) {
            if (keyTable[index].key != key) {
                continue;
            }
            keyValue = keyTable[index].value;
            break;
        }
    }
    return keyValue;
}

/* Clears the counter pair dataKeyCount / dataKeyRegistry. */
void dataCountersClear(void) {
    dataKeyCount = 0;
    dataKeyRegistry[0] = 0;
}

extern unsigned short D_00AB55F0[];
extern unsigned short D_00AB55F8[];

/* Appends a key to the registry dataKeyRegistry when it is listed in the table chosen by table_select. */
int dataKeyRegister(StatOwner *unit, unsigned short key)
{
    int index;
    unsigned short *keyList;
    int registered;

    keyList =(unit->stats->keyUnsigned - 11U < 5U) ? D_00AB55F8 : D_00AB55F0;
    index = 0;
    while (keyList[index] != 0) {
        if (key == keyList[index]) {
            break;
        }
        index++;
    }
    if (keyList[index] == 0) {
        return 0;
    }
    registered = dataKeyCount;
    if (registered < 8) {
        dataKeyRegistry[registered] = key;
        dataKeyCount = registered + 1;
        return 1;
    }
    return 0;
}

/* One 24-byte row of a slot table: word 0 is a count, words 1 to 5 are slots. */
struct DataSlotRow {
    int count;
    int slot[5];
};

void seVoiceStartDefault(int id);
struct DataSlotRow *dataSlotRowGet(StatOwner *unit, int index);
int dataSlotKeyTest(StatOwner *unit, int key);
extern struct DataKeyEntry D_00AB5810[];

/* Finds the key table entry whose key list equals the registry dataKeyRegistry; returns its key, 21 when none, 0 when the registry is empty. */
int dataKeyFind(StatOwner *unit, struct DataKeyEntry **found)
{
    struct DataKeyEntry *keyTable;
    unsigned short *entryKeys;
    int index;
    unsigned short keyCount;
    int matchedCount;

    if (dataKeyCount == 0) {
        return 0;
    }
    if (unit->stats->keyUnsigned - 11U < 5U) {
        keyTable = D_00AB5810;
    } else {
        keyTable = dataKeyTable;
    }
    for (index = 0; keyTable[index].keys != 0; index++) {
        entryKeys = keyTable[index].keys;
        keyCount = *entryKeys++;
        if (dataKeyCount != keyCount) {
            continue;
        }
        for (matchedCount = 0; matchedCount < keyCount; matchedCount++) {
            if (dataKeyRegistry[matchedCount] != entryKeys[matchedCount]) {
                break;
            }
        }
        if (matchedCount == keyCount) {
            *found = &keyTable[index];
            return (*found)->key;
        }
    }
    return DATA_KEY_NONE;
}

void dataVPadModeSet(int mode)
{
    padMode = mode;
    padData = 0;
}

void dataVPadSet(int virtualPad)
{
    unsigned short result = 0;
    int shift = 0;
    const unsigned short *entry = padConvTbl;
    virtualPad &= 0xFFFF;
    /* Original entry at 0x00A1D2A0 enters the body before testing the
     * loop count; keep the table pointer advance and 16-bit mask. */
    do {
        if ((virtualPad >> shift) & 1) {
            result |= *entry;
        }
        shift++;
        entry++;
    } while (shift < VIRTUAL_PAD_BIT_COUNT);
    padData = result;
}

unsigned short dataPadRead(int port)
{
    unsigned short value;

    value = 0;
    if (padMode == 0) {
        switch (port) {
        case 0:
            value = dataPadRawWords[0].port[0];
            break;
        case 1:
            value = dataPadRawWords[0].port[1];
            break;
        case 2:
            value = dataPadRawWords[0].port[2];
            break;
        }
    } else {
        value = padData;
    }
    return value;
}


/* Clears the pair menuWepSpPanelCursor / menuWepSpPanelRowCount. Their meaning is still open. */
void menuWepSpPanelReset(void) {
    menuWepSpPanelCursor = 0;
    menuWepSpPanelRowCount = 0;
}

extern struct DataSlotRow dataSlotRowsA[];
extern struct DataSlotRow dataSlotRowsB[];

/* Row of table A when the unit's table_select is 1 to 10, else of table B. */
struct DataSlotRow *dataSlotRowGet(StatOwner *unit, int index)
{
    if (unit->stats->keyUnsigned - 1U < 10U) {
        return &dataSlotRowsA[index];
    }
    return &dataSlotRowsB[index];
}

/* Pad-driven counter update of the slot rows; returns a slot key, 21 or 0. */
int menuWepSpPanelUpdate(StatOwner *unit)
{
    int columnPad;
    int rowPad;
    struct DataSlotRow *slotRow;
    int slotKey;

    columnPad = dataPadRead(1);
    rowPad = dataPadRead(2);
    if ((columnPad & PAD_RIGHT) != 0) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
        if (menuWepSpPanelCursor <= 0) {
            menuWepSpPanelCursor += 1;
        } else {
            menuWepSpPanelCursor = 0;
        }
        menuWepSpPanelRowCount = 0;
    } else if (columnPad & PAD_LEFT) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
        if (menuWepSpPanelCursor > 0) {
            menuWepSpPanelCursor -= 1;
        } else {
            menuWepSpPanelCursor = 1;
        }
        menuWepSpPanelRowCount = 0;
    }
    slotRow = dataSlotRowGet(unit, menuWepSpPanelCursor);
    if (rowPad & PAD_UP) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
        if (menuWepSpPanelRowCount > 0) {
            menuWepSpPanelRowCount -= 1;
        } else {
            menuWepSpPanelRowCount = slotRow->count - 1;
        }
    } else if (rowPad & PAD_DOWN) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
        if (menuWepSpPanelRowCount < slotRow->count - 1) {
            menuWepSpPanelRowCount += 1;
        } else {
            menuWepSpPanelRowCount = 0;
        }
    }
    if (!(columnPad & PAD_CIRCLE)) {
        return 0;
    }
    slotKey = slotRow->slot[menuWepSpPanelRowCount];
    if (dataSlotKeyTest(unit, slotKey) == 0) {
        seVoiceStartDefault(SE_REFUSE);
        return DATA_KEY_NONE;
    }
    seVoiceStartDefault(SE_CONFIRM);
    return slotKey;
}

int dataBoxChkCategory1(int index);
int effectPosCountFree(StatOwner *unit);
int battleMaskWordsUnmatched(StatOwner *unit, void *out);
int calcCfEventKeyTest(StatOwner *unit);


/* Key test of the slot menu: keys 11 to 19 each pick one check; key 11 always passes. */
int dataSlotKeyTest(StatOwner *unit, int key)
{
    int scratch[28];
    int count;
    int i;

    switch (key) {
    case 12:
        count = 0;
        for (i = 1; i < 60; i++) {
            if (slotCodeTest(unit, i) != 0) {
                count++;
            }
        }
        return count > 0;
    case 13:
        count = 0;
        for (i = 1; i < 41; i++) {
            if (dataBoxChkCategory1(i) > 0) {
                HitAction *box = hitActionGet(i);
                if (box->flags & 0x4000) {
                    count++;
                }
            }
        }
        return count > 0;
    case 16:
        if (battleMaskWordsUnmatched(unit, 0) > 0) {
            return 1;
        }
        break;
    case 17:
        if (calcKeyBandRecords(unit, scratch) > 0) {
            return 1;
        }
        break;
    case 14:
        if (calcKeyPairRecords(unit, scratch + 8) > 0) {
            return 1;
        }
        break;
    case 19:
        if (slotFlagCollect(unit, scratch) > 0) {
            return 1;
        }
        break;
    case 11:
        return 1;
    case 18:
        if (calcCfEventKeyTest(unit) != 0) {
            return 1;
        }
        break;
    case 15:
        if (effectPosCountFree(unit) > 0) {
            if (battleStateGet()->moveCmd == 0) {
                return 1;
            }
        }
        break;
    default:
        break;
    }
    return 0;
}

int menuWepSpPanelCursorGet(void) {
    return menuWepSpPanelCursor;
}

int menuWepSpPanelRowCountGet(void) {
    return menuWepSpPanelRowCount;
}


void menuTargetListCursorReset(void) {
    menuTargetListCursor = 0;
}

int menuTargetListUpdate(StatOwner *unit)
{
    int targets[8];
    int targetCount;
    int rowPad;

    dataPadRead(1);
    rowPad = dataPadRead(2);
    targetCount = battleMaskWordsUnmatched(unit, targets);
    if (rowPad & PAD_UP) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
        if (menuTargetListCursor > 0) {
            menuTargetListCursor = menuTargetListCursor - 1;
        } else {
            menuTargetListCursor = targetCount - 1;
        }
    } else if (rowPad & PAD_DOWN) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
        if (menuTargetListCursor < targetCount - 1) {
            menuTargetListCursor = menuTargetListCursor + 1;
        } else {
            menuTargetListCursor = 0;
        }
    }
    return targets[menuTargetListCursor];
}

int menuTargetListCursorGet(void) {
    return menuTargetListCursor;
}

/* Clears the DataState block and the object pointer. */
void dataStateReset(void) {
    memset(&dataState, 0, sizeof(dataState));
    dataObject = 0;
}

/* Creates the object at the origin and keeps it in dataObject. */
void dataObjCreateAtOrigin(void) {
    void *result;

    result = dataObjCreate(0.0f, 0.0f, 0.0f);
    dataObject = result;
}

void *dataObjCreate(f32 x, f32 y, f32 z)
{
    struct DataObject *obj;
    struct MatrixBlock *matrix;

    obj = dataObjAlloc();
    if (obj == 0) {
        return 0;
    }
    ((DataFlagsView *)&obj->flags)->bits = 0;
    dataLookupReset(&obj->lookup);
    obj->selectedIndex = 0;
    memset(&obj->id, 0, 16);
    obj->update = dataObjUpdate;
    obj->draw = dataObjDraw;
    matrix = (struct MatrixBlock *)&obj->pose;
    ov01MatrixBlockInit(matrix);
    obj->pose.position.x = x;
    ((struct DataVec3 *)matrix)->y = y;
    ((struct DataVec3 *)matrix)->z = z;
    ((DataFlagsView *)&obj->flags)->bits |= DATA_FLAG_HIDDEN;
    return obj;
}

/* Frees the object and clears its pointer when present. */
void dataObjRemove(void) {
    if (dataObject != 0) {
        dataObjRelease(dataObject);
        dataObject = 0;
    }
}

/* One entry of the object's lookup block. kind 1 keeps an index, kinds 2 and 3 keep the object it names. */
struct DataLookupEntry {
    /* 0x00 */ int kind;
    /* 0x04 */ unsigned char unmodeled_04[4];
    /* 0x08 */ union {
        int index;
        struct DataObject *target;
    } ref;
};

void slideSetStep(struct DataVec3 *slide, int step);
int objStdMove(struct DataVec3 *slide);
void dataObjGetPosition(struct DataObject *obj, struct DataVec3 *out);

void dataObjUpdate(struct DataObject *obj) {
    struct DataLookupEntry *cur;
    struct DataLookup *lookup = &obj->lookup;
    struct DataVec3 *pos;
    int flags;
    struct DataObject *target;

    cur = dataLookupCurrent(lookup);
    if (cur == 0) {
        return;
    }
    pos = &obj->pose.position;
    switch (cur->kind) {
    case 1:
        slideSetStep(pos, 2);
        if (objStdMove(pos) != 0) {
            obj->selectedIndex = cur->ref.index;
            obj->flags &= ~DATA_FLAG_MATCHED;
            dataLookupNext(lookup);
        }
        break;
    case 2:
    case 3:
        flags = obj->flags;
        target = cur->ref.target;
        if ((flags & DATA_FLAG_HIDDEN) == 0 && (target->flags & 0x20000) == 0) {
            dataObjGetPosition(target, &obj->pose.position);
            flags = obj->flags;
        }
        obj->flags = flags & ~DATA_FLAG_MATCHED;
        break;
    case 0:
    default:
        dataLookupNext(&obj->lookup);
        break;
    }
}

/* Draw callback (+0x08) of dataObjCreate: sends a GS packet unless the object is hidden (flag 0x2). */
void dataObjDraw(struct DataObject *obj) {
    u32 packet[4];

    if (!(obj->flags & DATA_FLAG_HIDDEN)) {
        grGpInit(packet);
        menuTexFlush();
        dataObjDrawSprite(obj, packet);
        grPacketSend(packet);
    }
}

/* Reads the object's position from its id. */
void dataObjGetPosition(struct DataObject *obj, struct DataVec3 *out) {
    struct DataVec3 vec;
    f32 x;
    f32 y;
    f32 z;

    sefActorMulLocalVector(obj->id, 2, &vec, 1);
    x = vec.x;
    y = vec.y;
    z = vec.z;
    out->z = z;
    out->y = y;
    out->x = x;
}

int grBBCalc(struct DataVec3 *pos, int *x, int *y);
void grPutSprOpen(void *packet);
void grPutSprClose(void);
void menuStatSprite(void *, int, int, int, int, int, int);
void func_00AA84B0(void *packet);

void dataObjDrawSprite(struct DataObject *obj, u32 *packet)
{
    int x;
    int y;
    int spriteStyle;

    if (grBBCalc(&obj->pose.position, &x, &y)) {
        x -= 23;
        y -= 38;
        spriteStyle = 72;
        if (obj->kind != 0) {
            spriteStyle = (obj->kind == 1) ? 164 : 118;
        }
        func_00AA84B0(packet);
        grPutSprOpen(packet);
        menuStatSprite(packet, x, y, 46, 38, spriteStyle, 186);
        grPutSprClose();
    }
}

void menuTgtStatusShow(void);
void menuUnitPanelBind(void *unit);
void menuIconListShow(void *unit);

/* Shows the target status (clearing the hidden flag first), then binds the unit panel and icon list
 * unless the target byte has bit 0x8 set. */
void dataTargetShow(StatOwner *unit, unsigned char *targetFlags)
{
    dataObject->flags &= ~DATA_FLAG_HIDDEN;
    menuTgtStatusShow();
    if (unit == 0) {
        return;
    }
    if (targetFlags != 0 && (targetFlags[0] & 8)) {
        return;
    }
    menuUnitPanelBind(unit);
    menuTimedSeqShow(unit);
    menuIconListShow(unit);
}

void dataTargetApply(struct DataObject *target, struct DataState *cursor);

void dataTargetHide(void)
{
    StatOwner *unit;

    dataObject->flags |= DATA_FLAG_HIDDEN;
    dataTargetApply(dataObject, &dataState);
    menuTgtStatusHide();
    menuUnitPanelArmTimer();
    menuTimedSeqArmTick();
    unit = tgtUnitGet();
    if ((unit == 0) || (unit->stats->entryCount <= 0)) {
        menuIconListClose();
    }
}

/* Clears the hidden flag 0x2. */
void dataObjShow(void) {
    dataObject->flags &= ~DATA_FLAG_HIDDEN;
}

/* Sets the hidden flag 0x2. */
void dataObjHide(void) {
    dataObject->flags |= DATA_FLAG_HIDDEN;
}

/* Sets flag 0x1 on the target when the lookup for key is nonzero. */
void dataTargetMatchKey(struct DataObject *target, void *key)
{
    dataLookupReset(&target->lookup);
    if (dataLookupAppend(&target->lookup, key)->command != 0) {
        target->flags |= DATA_FLAG_MATCHED;
    }
}

int *effectPosGet(int index);

void dataTargetApply(struct DataObject *target, struct DataState *cursor)
{
    struct DataPose *pose;
    StatOwner *unitEntry;
    int matchKey[8];
    int entryValue;
    int *effectPos;

    pose = &target->pose;
    if (cursor->entryMode == 0) {
        unitEntry = (StatOwner *)cursor->units[cursor->index];
        entryValue = unitEntry->stats->posIndex;
        dataObjGetPosition((struct DataObject *)unitEntry, &pose->anchor.vec);
        pose->position_tail = 0;
    } else {
        entryValue = cursor->entry[cursor->index];
        effectPos = effectPosGet(entryValue);
        pose->anchor.vec.x = (float)effectPos[0] / 100.0f;
        pose->anchor.vec.y = (float)effectPos[1] / 100.0f;
        pose->anchor.vec.z = (float)effectPos[2] / 100.0f;
        pose->position_tail = 0;
    }
    if ((target->flags & DATA_FLAG_HIDDEN) != 0) {
        pose->position.x = pose->anchor.vec.x;
        pose->position.y = pose->anchor.vec.y;
        pose->position.z = pose->anchor.vec.z;
    }
    matchKey[0] = 1;
    matchKey[2] = entryValue;
    dataTargetMatchKey(target, matchKey);
}

/* Cursor move: next or prev pad bits step the index, wrapping at count, with
 * sound SE_CURSOR_MOVE; the target is re-applied when its entry changed. Returns the new index. */
int dataCursorMove(struct DataObject *target, struct DataState *cursor, int next_mask, int prev_mask)
{
    int forward_index;
    int backward_index;

    cursor->prev_index = cursor->index;
    if (dataPadRead(1) & next_mask) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
        forward_index = cursor->index + 1;
        cursor->index = forward_index;
        if (forward_index >= cursor->count) {
            cursor->index = 0;
        }
    } else if (dataPadRead(1) & prev_mask) {
        seVoiceStartDefault(SE_CURSOR_MOVE);
        backward_index = cursor->index - 1;
        cursor->index = backward_index;
        if (backward_index < 0) {
            cursor->index = cursor->count - 1;
        }
    }
    if (target->selectedIndex != cursor->entry[cursor->index]) {
        dataTargetApply(target, cursor);
    }
    return cursor->index;
}

/* Sets the mode word: values below 2 are stored as given, otherwise the
 * flags choose 0 or 1. */
void dataStateSetMode(u32 value, int flags)
{
    struct DataState *state = &dataState;

    if (value < 2U) {
        state->mode = value;
        return;
    }
    if (!(flags & 4)) {
        if (flags & 1) {
            state->mode = 0U;
            return;
        }
        state->mode = 1U;
        return;
    }
    state->mode = 0U;
}

int func_00A8A150(void) {
    return dataState.mode;
}

/* Pad test for dataTargetListInit: the first argument is the dataObject pointer the caller passes, unused here.
 * Pressing either mask bit flips dataState.mode between 0 and 1 and returns 1. */
int dataStateModeToggle(struct DataObject *obj, struct DataState *state, int next_mask, int toggle_mask)
{
    int pad = dataPadRead(1);

    if ((pad & next_mask) || (pad & toggle_mask)) {
        if (state->mode == 0) {
            state->mode = 1;
        } else {
            state->mode = 0;
        }
        return 1;
    }
    return 0;
}

int unitTblGet(int side, void ***table);

int dataUnitListInit(void **selected, int side)
{
    struct DataState *state = &dataState;
    void **entries;
    void **unitSlot;
    StatOwner *entry;
    StatBlock *stats;
    int remaining;
    int unitCount;
    int found;

    state->index = 0;
    state->count = 0;
    unitCount = unitTblGet(side, &entries);
    if (unitCount > 0) {
        for (unitSlot = entries, remaining = unitCount; remaining != 0; remaining--, unitSlot++) {
            entry = *(StatOwner **)unitSlot;
            if (entry == 0) {
                continue;
            }
            if ((entry->flags & 4) != 0) {
                continue;
            }
            stats = entry->stats;
            if ((stats->flags & 0x4000) != 0) {
                continue;
            }
            if (*selected == entry) {
                state->index = state->count;
            }
            state->units[state->count] = *unitSlot;
            state->entry[state->count] = ((StatOwner *)*unitSlot)->stats->posIndex;
            state->count++;
        }
    }
    found = 0;
    if (state->count != 0) {
        state->prev_mode = 1;
        *selected = state->units[state->index];
        found = 1;
    }
    return found;
}

int unitIsInFacingArc(void *first, void *second);

enum DataMatchKind {
    DATA_MATCH_VALUE = 2,
    DATA_MATCH_SLOT = 3
};

struct DataMatchKey {
    enum DataMatchKind kind;
    unsigned char unmodeled_04[4];
    /* The matched value: a unit pointer. Written through the union (alias set 0) so the store keeps its place. */
    union {
        void *pointer;
    } payload;
    unsigned char unmodeled_0C[20];
};

int dataUnitCursorStep(void **out)
{
    struct DataObject *target = dataObject;
    struct DataMatchKey key;
    int changed;
    void *selected;
    int matched;
    int kind;

    dataState.entryMode = 0;
    dataCursorMove(target, &dataState, PAD_RIGHT, PAD_LEFT);
    changed = dataState.prev_index != dataState.index;
    selected = dataState.units[dataState.index];
    matched = target->flags & DATA_FLAG_MATCHED;
    *out = selected;
    if (!matched) {
        key.kind = DATA_MATCH_VALUE;
        key.payload.pointer = selected;
        dataTargetMatchKey(target, &key);
    }
    kind = unitIsInFacingArc(battleStateGet()->current, *out);
    if (kind != 1) {
        kind = 0;
    }
    target->kind = kind;
    return changed;
}

int effectPosGetCount(void);
int effectPosIsFree(void *obj, int index);

int dataEntryListInit(void *obj)
{
    struct DataObject *target = dataObject;
    struct DataState *state = &dataState;
    int *countSlot;
    int i;
    int result;

    state->index = 0;
    state->count = 0;
    countSlot = &state->count;
    for (i = 0; i < effectPosGetCount(); i++) {
        if (effectPosIsFree(obj, i)) {
            if (i == target->selectedIndex) {
                state->index = state->count;
            }
            state->entry[(*countSlot)++] = i;
        }
    }
    result = state->count != 0;
    target->kind = 2;
    return result;
}

/* Steps the cursor (next mask 0x2000, prev mask 0x8000), stores the selected entry in *out and returns
 * whether the index changed. The key block for dataTargetMatchKey holds 1 at word 0 and the entry at word 2;
 * the frame reserves 0x20 bytes for it. The unit battleSelectStep passes is not used. */
int dataEntryCursorStep(StatOwner *unit, int *out)
{
    struct DataObject *target = dataObject;
    int matchKey[8];
    int changed;

    dataState.entryMode = 1;
    dataCursorMove(target, &dataState, PAD_RIGHT, PAD_LEFT);
    changed = dataState.prev_index != dataState.index;
    *out = dataState.entry[dataState.index];
    if (!(target->flags & DATA_FLAG_MATCHED)) {
        matchKey[0] = 1;
        matchKey[2] = *out;
        dataTargetMatchKey(target, matchKey);
    }
    return changed;
}


/* Collects the units tgtCollectUnits returns that the mode selects (mode 0: metric below 16, mode 1:
 * metric 16 or more) into dataState.units and their levels into dataState.entry, then sets
 * dataState.index and *selected. Returns 1 when any unit was collected. */
int dataTargetListInit(StatOwner **selected, StatOwner *target, int flags)
{
    StatOwner *units[12];
    struct DataState *state;
    struct DataObject *object;
    int collected;
    int mode;
    int unit_count;
    int i = 0;
    int result;
    int select;

    state = &dataState;
    object = dataObject;
    state->index = 0;
    if ((flags & 3) == 3) {
        dataStateModeToggle(object, state, PAD_UP, PAD_DOWN);
    }
    unit_count = tgtCollectUnits(target, flags, units);
    collected = 0;
    if (unit_count > 0) {
        mode = state->mode;
        for (i = 0; i < unit_count; i++) {
            if ((state->mode == 0 && units[i]->stats->key < 16) ||
                (state->mode == 1 && units[i]->stats->key >= 16)) {
                state->units[collected++] = units[i];
            }
        }
    }
    state->count = collected;
    for (i = 0; i < state->count; i++) {
        StatOwner *unit = state->units[i];
        StatBlock *status = unit->stats;

        state->entry[i] = status->posIndex;
        if (state->prev_mode == state->mode) {
            if (*selected == unit) {
                state->index = i;
            }
        } else if (state->mode == 0 && target == unit) {
            state->index = i;
        }
    }
    result = 0;
    if (state->count != 0) {
        *selected = state->units[state->index];
        result = 1;
    }
    state->prev_mode = state->mode;
    if ((flags & 5) != 0) {
        object->kind = 2;
    } else {
        select = unitIsInFacingArc(battleStateGet()->current, *selected);
        if (select != 1) {
            select = 0;
        }
        object->kind = select;
    }
    return result;
}

int ov01SceneFrameCount(void);

int dataUnitCursorCycle(StatOwner **out, int mode)
{
    struct DataObject *target = dataObject;
    struct DataState *state = &dataState;
    struct DataMatchKey key;
    int changed;
    void *selectedValue;
    int flagged;
    int i;

    if (!(mode & 8)) {
        state->entryMode = 0;
        dataCursorMove(target, state, PAD_RIGHT, PAD_LEFT);
        changed = state->prev_index != state->index;
        selectedValue = state->units[state->index];
        flagged = target->flags & DATA_FLAG_MATCHED;
        *out = selectedValue;
        if (!flagged) {
            key.kind = DATA_MATCH_VALUE;
            key.payload.pointer = selectedValue;
            dataTargetMatchKey(target, &key);
        }
    } else {
        if (!(target->flags & DATA_FLAG_MATCHED)) {
            void **slot;

            for (i = 0; i < state->count; i++) {
                if (state->units[i] == target->id) {
                    if ((ov01SceneFrameCount() & 3) == 0) {
                        i++;
                    }
                    break;
                }
            }
            if (i >= state->count) {
                i = 0;
            }
            slot = &state->units[i];
            key.kind = DATA_MATCH_SLOT;
            key.payload.pointer = *slot;
            dataTargetMatchKey(target, &key);
            target->id = *slot;
        }
        changed = 0;
    }
    return changed;
}

void *dataSelectedGet(unsigned char *target)
{
    if (target[0] & 8) {
        return dataObject->id;
    }
    return dataState.units[dataState.index];
}

void dataDummySet(void)
{
}

/* Initialized overlay data of this file (the key lists, the key table, the pad and slot tables, dataState), in address order. */
unsigned short D_00AB55F0[] = { 0x0080, 0x0010, 0x0020, 0x0000 };

unsigned short D_00AB55F8[] = { 0x0080, 0x0010, 0x0000, 0x0000 };

unsigned short D_00AB5600[] = { 0x0001, 0x0080, 0x0000, 0x0000 };

unsigned short D_00AB5608[] = { 0x0001, 0x0010, 0x0000, 0x0000 };

unsigned short D_00AB5610[] = { 0x0002, 0x0080, 0x0080, 0x0000 };

unsigned short D_00AB5618[] = { 0x0002, 0x0080, 0x0010, 0x0000 };

unsigned short D_00AB5620[] = { 0x0002, 0x0010, 0x0010, 0x0000 };

unsigned short D_00AB5628[] = { 0x0002, 0x0010, 0x0080, 0x0000 };

unsigned short D_00AB5630[] = { 0x0001, 0x0020, 0x0000, 0x0000 };

unsigned short D_00AB5638[] = { 0x0002, 0x0020, 0x0020, 0x0000 };

unsigned short D_00AB5640[] = { 0x0003, 0x0020, 0x0020, 0x0020 };

unsigned short D_00AB5648[] = { 0x0004, 0x0020, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short D_00AB5658[] = { 0x0003, 0x0080, 0x0080, 0x0020 };

unsigned short D_00AB5660[] = { 0x0004, 0x0080, 0x0080, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short D_00AB5670[] = { 0x0005, 0x0080, 0x0080, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };

unsigned short D_00AB5680[] = { 0x0003, 0x0080, 0x0010, 0x0020 };

unsigned short D_00AB5688[] = { 0x0004, 0x0080, 0x0010, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short D_00AB5698[] = { 0x0005, 0x0080, 0x0010, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };

unsigned short D_00AB56A8[] = { 0x0003, 0x0010, 0x0010, 0x0020 };

unsigned short D_00AB56B0[] = { 0x0004, 0x0010, 0x0010, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short D_00AB56C0[] = { 0x0005, 0x0010, 0x0010, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };

unsigned short D_00AB56D0[] = { 0x0003, 0x0010, 0x0080, 0x0020 };

unsigned short D_00AB56D8[] = { 0x0004, 0x0010, 0x0080, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short D_00AB56E8[] = { 0x0005, 0x0010, 0x0080, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };

struct DataKeyEntry dataKeyTable[23] = {
    { D_00AB56E8, 1, 10 },
    { D_00AB56D8, 1, 9 },
    { D_00AB56D0, 1, 8 },
    { D_00AB56C0, 1, 10 },
    { D_00AB56B0, 1, 9 },
    { D_00AB56A8, 1, 8 },
    { D_00AB5698, 1, 10 },
    { D_00AB5688, 1, 9 },
    { D_00AB5680, 1, 8 },
    { D_00AB5670, 1, 10 },
    { D_00AB5660, 1, 9 },
    { D_00AB5658, 1, 8 },
    { D_00AB5648, 1, 10 },
    { D_00AB5640, 1, 9 },
    { D_00AB5638, 1, 8 },
    { D_00AB5610, 0, 3 },
    { D_00AB5618, 0, 4 },
    { D_00AB5620, 0, 5 },
    { D_00AB5628, 0, 6 },
    { D_00AB5630, 0, 7 },
    { D_00AB5600, 0, 1 },
    { D_00AB5608, 0, 2 },
    { 0, 0, 0 }
};

struct DataKeyEntry D_00AB5810[3] = {
    { D_00AB5600, 0, 1 },
    { D_00AB5608, 0, 2 },
    { 0, 0, 0 }
};

unsigned short padConvTbl[] = { 0x1000, 0x4000, 0x8000, 0x2000, 0x0010, 0x0040, 0x0080, 0x0020, 0x0004, 0x0001, 0x0008, 0x0002, 0x0800, 0x0100, 0x0000, 0x0000 };

struct DataSlotRow dataSlotRowsA[2] = {
    { 5, { 0xB, 0xC, 0xD, 0xE, 0xF } },
    { 3, { 0x10, 0x12, 0x13, 0x0, 0x0 } }
};

struct DataSlotRow dataSlotRowsB[2] = {
    { 4, { 0xB, 0xC, 0x11, 0xF, 0x0 } },
    { 2, { 0x10, 0x12, 0x0, 0x0, 0x0 } }
};

struct DataState dataState = { 0 };

struct DataObject *dataObject = 0;
