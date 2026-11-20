#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "ov01/data_obj_table.h"
#include "ov01/obj_slot.h"
#include "ov01/obj.h"
#include "ov01/gr_matrix.h"
#include "ov01/unit_cmd.h"
#include "ov01/battle_init.h"
#include "ov01/calc.h"
#include "ov01/choice_menu.h"
#include "ov01/data_record_get.h"
#include "ov01/unit_tbl.h"
#include "ov01/gr_gp_init.h"
#include "ov01/data_box.h"
#include "ov01/menu_panel.h"
#include "ov01/menu_tex_state.h"
#include "ov01/scene.h"
#include "ov01/effect_pos.h"
#include "main/se_bank_load.h"
#include "ov01/menu_wep_sp_panel.h"
#include "ov01/menu_unit_panel_open.h"
#include "ov01/menu_icon_list.h"
#include "ov01/menu_combo_panel.h"
#include "main/sef.h"

/* Uninitialized storage of this file, in address order (only this file names these labels). */
static int keyBuff[8];
static int keyBuffIdx;
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


/* Global block curMove: the cursor fields (0xA4-0xAC) and the mode word (0xB4)
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

extern struct DataState curMove;
extern struct DataObject *dataObject;
void *objEnCurCreate(f32 x, f32 y, f32 z);
void objDispCurSubEn(struct DataObject *obj, GrPacket *packet);
void *memset(void *dst, int value, unsigned int size);

/* Virtual pad reset: pad mode and pad data, then the two counter pairs (prototype in battle_init.h). */
void dataVPadReset(void) {
    dataVPadModeSet(0);
    keyBuffClear();
    menuWepSpPanelReset();
}

/* Clears the counters; manInput passes unit, it is not used. */
void keyBuffClearUnit(BattleUnit *unit) {
    keyBuffClear();
}

/* Word view of DataObject.flags whose store is alias set 0 (the union spelling of the bit set). */
typedef union DataFlagsView {
    int value;
    unsigned int bits;
} DataFlagsView;

struct DataKeyEntry;
int keyBuffSet(BattleUnit *unit, unsigned short key);
int keyBuffChk(BattleUnit *unit, struct DataKeyEntry **found);
int keyCmdApGet(BattleUnit *unit, int key);

int keyCmdGet(BattleUnit *unit)
{
    struct DataKeyEntry *found;
    unsigned short pressed;
    int key;

    if (unit->stats->keyUnsigned - 1U < 10U) {
        if (unit->stats->keysSpent > 0 &&
            (unit->stats->stockCount == 0 || (unit->flags & 0x800))) {
            keyBuffClear();
            return DATA_KEY_CANCEL;
        }
    } else if (unit->stats->keysSpent > 0) {
        keyBuffClear();
        return DATA_KEY_CANCEL;
    }
    pressed = dataPadRead(1);
    if (pressed == 0) {
        return 0;
    }
    if (pressed == PAD_CROSS) {
        keyBuffClear();
        return DATA_KEY_CANCEL;
    }
    if (keyBuffSet(unit, pressed) == 0) {
        return DATA_KEY_NONE;
    }
    key = keyBuffChk(unit, &found);
    if (key == DATA_KEY_NONE) {
        if (keyBuffIdx > 0) {
            keyBuffIdx--;
        }
        return DATA_KEY_NONE;
    }
    if (keyCmdChk(unit, key, tgtUnitGet()) != 0) {
        if (keyBuffIdx > 0) {
            keyBuffIdx--;
        }
        return DATA_KEY_NONE;
    }
    if (!(unit->stats->keyUnsigned - 1U < 10U && key - 1U < 2U)) {
        unit->stats->keysSpent++;
    }
    unitLevelAdd(unit, -keyCmdApGet(unit, key));
    return key;
}

int keyCmdChk(BattleUnit *unit, int key, void *tgt)
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
        budget = unit->stats->stockCount;
        if (budget < keyCmdApGet(unit, key)) {
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

/* One 12-byte entry of the key table at keyCmdTblCha: a zero used word ends the table. */
struct DataKeyEntry {
    unsigned short *keys;
    int value;
    int key;
};

extern struct DataKeyEntry keyCmdTblCha[];

int keyCmdApGet(BattleUnit *unit, int key)
{
    int index;
    int keyValue = 0;
    struct DataKeyEntry *keyTable;

    if (key == 0 || key == DATA_KEY_NONE || key == DATA_KEY_CANCEL) {
        return 0;
    }
    if (key < 11 && unit->stats->keyUnsigned - 1U < 10U) {
        keyTable = keyCmdTblCha;
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

/* Clears the counter pair keyBuffIdx / keyBuff. */
void keyBuffClear(void) {
    keyBuffIdx = 0;
    keyBuff[0] = 0;
}

extern unsigned short keyItemTblCha[];
extern unsigned short keyItemTblAgws[];

/* Appends a key to the registry keyBuff when it is listed in the table chosen by table_select. */
int keyBuffSet(BattleUnit *unit, unsigned short key)
{
    int index;
    unsigned short *keyList;
    int registered;

    keyList =(unit->stats->keyUnsigned - 11U < 5U) ? keyItemTblAgws : keyItemTblCha;
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
    registered = keyBuffIdx;
    if (registered < 8) {
        keyBuff[registered] = key;
        keyBuffIdx = registered + 1;
        return 1;
    }
    return 0;
}

extern struct DataKeyEntry keyCmdTblAgws[];

/* Finds the key table entry whose key list equals the registry keyBuff; returns its key, 21 when none, 0 when the registry is empty. */
int keyBuffChk(BattleUnit *unit, struct DataKeyEntry **found)
{
    struct DataKeyEntry *keyTable;
    unsigned short *entryKeys;
    int index;
    unsigned short keyCount;
    int matchedCount;

    if (keyBuffIdx == 0) {
        return 0;
    }
    if (unit->stats->keyUnsigned - 11U < 5U) {
        keyTable = keyCmdTblAgws;
    } else {
        keyTable = keyCmdTblCha;
    }
    for (index = 0; keyTable[index].keys != 0; index++) {
        entryKeys = keyTable[index].keys;
        keyCount = *entryKeys++;
        if (keyBuffIdx != keyCount) {
            continue;
        }
        for (matchedCount = 0; matchedCount < keyCount; matchedCount++) {
            if (keyBuff[matchedCount] != entryKeys[matchedCount]) {
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
            value = PadData[0].directionHeld;
            break;
        case 1:
            value = PadData[0].directionPressed;
            break;
        case 2:
            value = PadData[0].directionRepeated;
            break;
        }
    } else {
        value = padData;
    }
    return value;
}


/* Clears the command panel cursor (menuWepSpPanelCursor) and its row count (menuWepSpPanelRowCount). */
void menuWepSpPanelReset(void) {
    menuWepSpPanelCursor = 0;
    menuWepSpPanelRowCount = 0;
}

extern struct DataSlotRow dataSlotRowsA[];
extern struct DataSlotRow dataSlotRowsB[];

/* Row of table A when the unit's table_select is 1 to 10, else of table B. */
struct DataSlotRow *dataSlotRowGet(BattleUnit *unit, int index)
{
    if (unit->stats->keyUnsigned - 1U < 10U) {
        return &dataSlotRowsA[index];
    }
    return &dataSlotRowsB[index];
}

/* Pad-driven counter update of the slot rows; returns a slot key, 21 or 0. */
int menuWepSpPanelUpdate(BattleUnit *unit)
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



/* Key test of the slot menu: keys 11 to 19 each pick one check; key 11 always passes. */
int dataSlotKeyTest(BattleUnit *unit, int key)
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
                UsableItemRecord *box = usableItemRecordGet(i);
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
            if (manWorkGet()->moveCmd == 0) {
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

int menuTargetListUpdate(BattleUnit *unit)
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
    memset(&curMove, 0, sizeof(curMove));
    dataObject = 0;
}

/* Creates the object at the origin and keeps it in dataObject. */
void objEnCurCreateAtOrigin(void) {
    void *result;

    result = objEnCurCreate(0.0f, 0.0f, 0.0f);
    dataObject = result;
}

void *objEnCurCreate(f32 x, f32 y, f32 z)
{
    struct DataObject *obj;
    DataObjectPose *pose;

    obj = dataObjAlloc();
    if (obj == 0) {
        return 0;
    }
    ((DataFlagsView *)&obj->flags)->bits = 0;
    objCmdClear(&obj->lookup);
    obj->selectedIndex = 0;
    memset(&obj->id, 0, 16);
    obj->update = objCur;
    obj->draw = objCurDraw;
    pose = &obj->pose;
    objStdInit(&pose->block);
    pose->block.position.x = x;
    pose->block.position.y = y;
    pose->block.position.z = z;
    ((DataFlagsView *)&obj->flags)->bits |= DATA_FLAG_HIDDEN;
    return obj;
}

/* Frees the object and clears its pointer when present. */
void objCurRemove(void) {
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
        BattleUnit *target;
    } ref;
};

void dataObjGetPosition(BattleUnit *obj, Vector4 *out);

void objCur(struct DataObject *obj) {
    struct DataLookupEntry *cur;
    struct DataLookup *lookup = &obj->lookup;
    SlideBlock *pos;
    int flags;
    BattleUnit *target;

    cur = objCmdPtrGet(lookup);
    if (cur == 0) {
        return;
    }
    pos = &obj->pose.block;
    switch (cur->kind) {
    case 1:
        objHatoVec(pos, 2);
        if (objStdMove(pos) != 0) {
            obj->selectedIndex = cur->ref.index;
            obj->flags &= ~DATA_FLAG_MATCHED;
            objCmdNext(lookup);
        }
        break;
    case 2:
    case 3:
        flags = obj->flags;
        target = cur->ref.target;
        if ((flags & DATA_FLAG_HIDDEN) == 0 && (target->flags & 0x20000) == 0) {
            dataObjGetPosition(target, &obj->pose.block.position);
            flags = obj->flags;
        }
        obj->flags = flags & ~DATA_FLAG_MATCHED;
        break;
    case 0:
    default:
        objCmdNext(&obj->lookup);
        break;
    }
}

/* Draw callback (+0x08) of objEnCurCreate: sends a GS packet unless the object is hidden (flag 0x2). */
void objCurDraw(struct DataObject *obj) {
    GrPacket packet;

    if (!(obj->flags & DATA_FLAG_HIDDEN)) {
        grGpInit(&packet);
        menuTexFlush();
        objDispCurSubEn(obj, &packet);
        grPacketSend(&packet);
    }
}

/* Reads the position of the unit's actor. */
void dataObjGetPosition(BattleUnit *obj, Vector4 *out) {
    Vector4 vec;
    f32 x;
    f32 y;
    f32 z;

    sefActorMulLocalVector(obj->unit, 2, &vec, 1);
    x = vec.x;
    y = vec.y;
    z = vec.z;
    out->z = z;
    out->y = y;
    out->x = x;
}

void objDispCurSubEn(struct DataObject *obj, GrPacket *packet)
{
    int x;
    int y;
    int spriteStyle;

    if (grBBCalc(&obj->pose.transform, &x, &y)) {
        x -= 23;
        y -= 38;
        spriteStyle = 72;
        if (obj->kind != 0) {
            spriteStyle = (obj->kind == 1) ? 164 : 118;
        }
        dataObjSpriteTexState(packet);
        grOpenSpr(packet);
        menuStatSprite(packet, x, y, 46, 38, spriteStyle, 186);
        grCloseSpr();
    }
}


/* Shows the target status (clearing the hidden flag first), then binds the unit panel and icon list
 * unless the target byte has bit 0x8 set. */
void dataTargetShow(BattleUnit *unit, unsigned char *targetFlags)
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
    menuComboPanelShow(unit);
    menuIconListShow(unit);
}

void curMove2Pos(struct DataObject *target, struct DataState *cursor);

void dataTargetHide(void)
{
    BattleUnit *unit;

    dataObject->flags |= DATA_FLAG_HIDDEN;
    curMove2Pos(dataObject, &curMove);
    menuTgtStatusHide();
    menuUnitPanelArmTimer();
    menuComboPanelArmTick();
    unit = tgtUnitGet();
    if ((unit == 0) || (unit->stats->hitZoneCount <= 0)) {
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
void curCmdSet(struct DataObject *target, void *key)
{
    objCmdClear(&target->lookup);
    if (dataLookupAppend(&target->lookup, key)->command != 0) {
        target->flags |= DATA_FLAG_MATCHED;
    }
}

void curMove2Pos(struct DataObject *target, struct DataState *cursor)
{
    DataObjectPose *pose;
    BattleUnit *unitEntry;
    int matchKey[8];
    int entryValue;
    int *effectPos;

    pose = &target->pose;
    if (cursor->entryMode == 0) {
        unitEntry = cursor->units[cursor->index];
        entryValue = unitEntry->stats->posIndex;
        dataObjGetPosition(unitEntry, &pose->block.goal);
        pose->transform.offset.y = 0;
    } else {
        entryValue = cursor->entry[cursor->index];
        effectPos = effectPosGet(entryValue);
        pose->block.goal.x = (float)effectPos[0] / 100.0f;
        pose->block.goal.y = (float)effectPos[1] / 100.0f;
        pose->block.goal.z = (float)effectPos[2] / 100.0f;
        pose->transform.offset.y = 0;
    }
    if ((target->flags & DATA_FLAG_HIDDEN) != 0) {
        pose->block.position.x = pose->block.goal.x;
        pose->block.position.y = pose->block.goal.y;
        pose->block.position.z = pose->block.goal.z;
    }
    matchKey[0] = 1;
    matchKey[2] = entryValue;
    curCmdSet(target, matchKey);
}

/* Cursor move: next or prev pad bits step the index, wrapping at count, with
 * sound SE_CURSOR_MOVE; the target is re-applied when its entry changed. Returns the new index. */
int curSel(struct DataObject *target, struct DataState *cursor, int next_mask, int prev_mask)
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
        curMove2Pos(target, cursor);
    }
    return cursor->index;
}

/* Sets the mode word: values below 2 are stored as given, otherwise the
 * flags choose 0 or 1. */
void curSelGrpSet(u32 value, int flags)
{
    struct DataState *state = &curMove;

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

int curSelGrpGet(void) {
    return curMove.mode;
}

/* Pad test for curTgtProcInit: the first argument is the dataObject pointer the caller passes, unused here.
 * Pressing either mask bit flips curMove.mode between 0 and 1 and returns 1. */
int curGrpSel(struct DataObject *obj, struct DataState *state, int next_mask, int toggle_mask)
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

int curEnProcInit(BattleUnit **selected, int side)
{
    struct DataState *state = &curMove;
    void **entries;
    void **unitSlot;
    BattleUnit *entry;
    StatBlock *stats;
    int remaining;
    int unitCount;
    int found;

    state->index = 0;
    state->count = 0;
    /* The slots are walked as void *: the loads then reload after the units store, as in the original. */
    unitCount = unitTblGet(side, (BattleUnit ***)&entries);
    if (unitCount > 0) {
        for (unitSlot = entries, remaining = unitCount; remaining != 0; remaining--, unitSlot++) {
            entry = *(BattleUnit **)unitSlot;
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
            state->entry[state->count] = ((BattleUnit *)*unitSlot)->stats->posIndex;
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

int curEnProc(BattleUnit **out)
{
    struct DataObject *target = dataObject;
    struct DataMatchKey key;
    int changed;
    BattleUnit *selected;
    int matched;
    int kind;

    curMove.entryMode = 0;
    curSel(target, &curMove, PAD_RIGHT, PAD_LEFT);
    changed = curMove.prev_index != curMove.index;
    selected = curMove.units[curMove.index];
    matched = target->flags & DATA_FLAG_MATCHED;
    *out = selected;
    if (!matched) {
        key.kind = DATA_MATCH_VALUE;
        key.payload.pointer = selected;
        curCmdSet(target, &key);
    }
    kind = unitIsInFacingArc(manWorkGet()->current, *out);
    if (kind != 1) {
        kind = 0;
    }
    target->kind = kind;
    return changed;
}

int curPosProcInit(struct BattleUnit *obj)
{
    struct DataObject *target = dataObject;
    struct DataState *state = &curMove;
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
 * whether the index changed. The key block for curCmdSet holds 1 at word 0 and the entry at word 2;
 * the frame reserves 0x20 bytes for it. The unit manInput passes is not used. */
int curPosProc(BattleUnit *unit, int *out)
{
    struct DataObject *target = dataObject;
    int matchKey[8];
    int changed;

    curMove.entryMode = 1;
    curSel(target, &curMove, PAD_RIGHT, PAD_LEFT);
    changed = curMove.prev_index != curMove.index;
    *out = curMove.entry[curMove.index];
    if (!(target->flags & DATA_FLAG_MATCHED)) {
        matchKey[0] = 1;
        matchKey[2] = *out;
        curCmdSet(target, matchKey);
    }
    return changed;
}


/* Collects the units calcTgtUnitTbl returns that the mode selects (mode 0: metric below 16, mode 1:
 * metric 16 or more) into curMove.units and their levels into curMove.entry, then sets
 * curMove.index and *selected. Returns 1 when any unit was collected. */
int curTgtProcInit(BattleUnit **selected, BattleUnit *target, int flags)
{
    BattleUnit *units[12];
    struct DataState *state;
    struct DataObject *object;
    int collected;
    int mode;
    int unit_count;
    int i = 0;
    int result;
    int select;

    state = &curMove;
    object = dataObject;
    state->index = 0;
    if ((flags & 3) == 3) {
        curGrpSel(object, state, PAD_UP, PAD_DOWN);
    }
    unit_count = calcTgtUnitTbl(target, flags, units);
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
        BattleUnit *unit = state->units[i];
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
        select = unitIsInFacingArc(manWorkGet()->current, *selected);
        if (select != 1) {
            select = 0;
        }
        object->kind = select;
    }
    return result;
}

int curTgtProc(BattleUnit **out, int mode)
{
    struct DataObject *target = dataObject;
    struct DataState *state = &curMove;
    struct DataMatchKey key;
    int changed;
    BattleUnit *selectedValue;
    int flagged;
    int i;

    if (!(mode & 8)) {
        state->entryMode = 0;
        curSel(target, state, PAD_RIGHT, PAD_LEFT);
        changed = state->prev_index != state->index;
        selectedValue = state->units[state->index];
        flagged = target->flags & DATA_FLAG_MATCHED;
        *out = selectedValue;
        if (!flagged) {
            key.kind = DATA_MATCH_VALUE;
            key.payload.pointer = selectedValue;
            curCmdSet(target, &key);
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
            curCmdSet(target, &key);
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
    return curMove.units[curMove.index];
}

void dataDummySet(void)
{
}

/* Initialized overlay data of this file (the key lists, the key table, the pad and slot tables, curMove), in address order. */
unsigned short keyItemTblCha[] = { 0x0080, 0x0010, 0x0020, 0x0000 };

unsigned short keyItemTblAgws[] = { 0x0080, 0x0010, 0x0000, 0x0000 };

unsigned short keyComboSquare[] = { 0x0001, 0x0080, 0x0000, 0x0000 };

unsigned short keyComboTriangle[] = { 0x0001, 0x0010, 0x0000, 0x0000 };

unsigned short keyComboSquareSquare[] = { 0x0002, 0x0080, 0x0080, 0x0000 };

unsigned short keyComboSquareTriangle[] = { 0x0002, 0x0080, 0x0010, 0x0000 };

unsigned short keyComboTriangleTriangle[] = { 0x0002, 0x0010, 0x0010, 0x0000 };

unsigned short keyComboTriangleSquare[] = { 0x0002, 0x0010, 0x0080, 0x0000 };

unsigned short keyComboCircle[] = { 0x0001, 0x0020, 0x0000, 0x0000 };

unsigned short keyComboCircleCircle[] = { 0x0002, 0x0020, 0x0020, 0x0000 };

unsigned short keyComboCircleCircleCircle[] = { 0x0003, 0x0020, 0x0020, 0x0020 };

unsigned short keyComboCircleCircleCircleCircle[] = { 0x0004, 0x0020, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short keyComboSquareSquareCircle[] = { 0x0003, 0x0080, 0x0080, 0x0020 };

unsigned short keyComboSquareSquareCircleCircle[] = { 0x0004, 0x0080, 0x0080, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short keyComboSquareSquareCircleCircleCircle[] = { 0x0005, 0x0080, 0x0080, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };

unsigned short keyComboSquareTriangleCircle[] = { 0x0003, 0x0080, 0x0010, 0x0020 };

unsigned short keyComboSquareTriangleCircleCircle[] = { 0x0004, 0x0080, 0x0010, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short keyComboSquareTriangleCircleCircleCircle[] = { 0x0005, 0x0080, 0x0010, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };

unsigned short keyComboTriangleTriangleCircle[] = { 0x0003, 0x0010, 0x0010, 0x0020 };

unsigned short keyComboTriangleTriangleCircleCircle[] = { 0x0004, 0x0010, 0x0010, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short keyComboTriangleTriangleCircleCircleCircle[] = { 0x0005, 0x0010, 0x0010, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };

unsigned short keyComboTriangleSquareCircle[] = { 0x0003, 0x0010, 0x0080, 0x0020 };

unsigned short keyComboTriangleSquareCircleCircle[] = { 0x0004, 0x0010, 0x0080, 0x0020, 0x0020, 0x0000, 0x0000, 0x0000 };

unsigned short keyComboTriangleSquareCircleCircleCircle[] = { 0x0005, 0x0010, 0x0080, 0x0020, 0x0020, 0x0020, 0x0000, 0x0000 };

struct DataKeyEntry keyCmdTblCha[23] = {
    { keyComboTriangleSquareCircleCircleCircle, 1, 10 },
    { keyComboTriangleSquareCircleCircle, 1, 9 },
    { keyComboTriangleSquareCircle, 1, 8 },
    { keyComboTriangleTriangleCircleCircleCircle, 1, 10 },
    { keyComboTriangleTriangleCircleCircle, 1, 9 },
    { keyComboTriangleTriangleCircle, 1, 8 },
    { keyComboSquareTriangleCircleCircleCircle, 1, 10 },
    { keyComboSquareTriangleCircleCircle, 1, 9 },
    { keyComboSquareTriangleCircle, 1, 8 },
    { keyComboSquareSquareCircleCircleCircle, 1, 10 },
    { keyComboSquareSquareCircleCircle, 1, 9 },
    { keyComboSquareSquareCircle, 1, 8 },
    { keyComboCircleCircleCircleCircle, 1, 10 },
    { keyComboCircleCircleCircle, 1, 9 },
    { keyComboCircleCircle, 1, 8 },
    { keyComboSquareSquare, 0, 3 },
    { keyComboSquareTriangle, 0, 4 },
    { keyComboTriangleTriangle, 0, 5 },
    { keyComboTriangleSquare, 0, 6 },
    { keyComboCircle, 0, 7 },
    { keyComboSquare, 0, 1 },
    { keyComboTriangle, 0, 2 },
    { 0, 0, 0 }
};

struct DataKeyEntry keyCmdTblAgws[3] = {
    { keyComboSquare, 0, 1 },
    { keyComboTriangle, 0, 2 },
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

struct DataState curMove = { 0 };

struct DataObject *dataObject = 0;
