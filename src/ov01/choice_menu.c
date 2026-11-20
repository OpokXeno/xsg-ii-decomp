#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "main/game_data.h"
#include "main/se_bank_load.h"
#include "main/choice_menu.h"
#include "ov01/obj_slot.h"
#include "ov01/data_file_load.h"
#include "main/game_camera.h"
#include "ov01/unit_cmd.h"
#include "ov01/battle_init.h"
#include "ov01/calc.h"
#include "ov01/cmd_exec.h"
#include "ov01/choice_menu.h"
#include "ov01/tgt_select.h"
#include "ov01/data_record_get.h"
#include "ov01/think_dispatch.h"
#include "ov01/sb_vm.h"
#include "ov01/data_box.h"
#include "ov01/menu_wep_sp_panel.h"
#include "main/xgl_flags.h"
#include "ov01/debug_entry.h"



/*
 * Private battle state, in original BSS address order. Keeping these local
 * definitions together lets the guarded overlay linker map each exact object
 * to its original storage span.
 */

static int thinkNo;
static int gattaiCode;
static int itemMenuChoice;
static int tecMenuChoice;
static int levelMenuChoice;
static int levelMenuIds[7];
static int levelMenuCount;
static int pairMenuChoice;
static int pairMenuIds[17];
static int pairMenuCount;
static int pairedMenuChoice;
static int pairedMenuIds[8];
static int pairedMenuCount;
static int tgtIdMenuChoice;
static int tgtIdMenuIds[8];
static int tgtIdMenuCount;
static unsigned char *choiceMenuItems[128];
static short choiceMenuValues[128];
static short choiceMenuEntryIds[128];
static unsigned char choiceMenuEntryFlags[128];
static TextEntry *choiceMenuTextTask;
static unsigned char *choiceMenuPages[2];
static TextEntry *pTaskEnCur;
static unsigned char *enemyCursorPages[2];
static int enemyCursorRow;
static int enemyCursorBit;
static int enemyCursorTimer;

extern int statBitValues[];
/* The enemy-cursor task: statusLineOpen opens it, enemyCursorClose and enemyCursorTextRemove
 * remove it. X1 names the same global the enemy-cursor task (battle_init.c). */
/* Text task that the choice-menu openers allocate (the textEntryAlloc result) and the close functions remove. */
/* The choice menu's text page list for textEntryAlloc: the text of the entry under the cursor (set by the pickers),
 * then 0. */
/* The text page list of the enemy cursor (and of the paired technique menu): the text (set by pairedMenuPick and
 * statusLineStep), then 0. */

/* The lists the choice-menu openers fill for choice slot 0: the entry texts (0 after the last), the value shown with
 * each entry, its id and its flag (CHOICE_ENTRY_DISABLED greys it out). The pickers index ids and flags with the
 * 1-based cursor. */

#define CHOICE_ENTRY_DISABLED 3
/* Layout of the openers' choice menu (slot 0) and of their text entry below it; each opener has its own style
 * index and unit size. */
#define CHOICE_MENU_X 0x14
#define CHOICE_MENU_Y 0x6C
#define CHOICE_MENU_WIDTH 0x1AC
#define CHOICE_MENU_HEIGHT 0x70
#define CHOICE_TEXT_X 0x14
#define CHOICE_TEXT_Y 0x118
#define CHOICE_TEXT_WIDTH 0x1AC
#define CHOICE_TEXT_HEIGHT 0x28
#define CHOICE_TEXT_STYLE 8

/* Items 1..ITEM_ID_LAST; an item can be picked in battle when its UsableItemRecord flags have ITEM_BATTLE_USE. */
#define ITEM_ID_LAST 40
#define ITEM_BATTLE_USE 0x4000
#define ITEM_MENU_STYLE 2
#define ITEM_MENU_UNIT_SIZE 2

/*
 * Item menu: lists the items the data box holds (dataBoxChkCategory1) that can be used in battle, with their
 * count, greyed out when unit cannot use them on any target (calcItmEnableChk), and opens it.
 */
void choiceMenuOpenItems(BattleUnit *unit)
{
    int id;
    int count;
    int n;

    n = 0;
    for (id = 1; id <= ITEM_ID_LAST; id++) {
        count = dataBoxChkCategory1(id);
        if (count > 0 && (usableItemRecordGet(id)->flags & ITEM_BATTLE_USE)) {
            choiceMenuItems[n] = hitActionTextGet(id, RECORD_TEXT_NAME);
            choiceMenuValues[n] = count;
            choiceMenuEntryIds[n] = id;
            choiceMenuEntryFlags[n] = calcItmEnableChk(unit, id) ? 0 : CHOICE_ENTRY_DISABLED;
            n++;
        }
    }
    choiceMenuItems[n] = 0;
    choiceMenuSetActive(0, 1);
    choiceMenuOpen(0, CHOICE_MENU_X, CHOICE_MENU_Y, CHOICE_MENU_WIDTH, CHOICE_MENU_HEIGHT, ITEM_MENU_STYLE,
                   ITEM_MENU_UNIT_SIZE, 0, choiceMenuItems, choiceMenuValues, choiceMenuEntryFlags);
    choiceMenuPages[1] = 0;
    choiceMenuPages[0] = 0;
    choiceMenuTextTask = textEntryAlloc(CHOICE_TEXT_X, CHOICE_TEXT_Y, CHOICE_TEXT_WIDTH, CHOICE_TEXT_HEIGHT,
                                        CHOICE_TEXT_STYLE, choiceMenuPages);
}


/* Closes choice slot 0 and removes the text task choiceMenuTextTask (the five identical close bodies of this TU). */
void choiceMenuTextClose(void) {
    choiceMenuClose(0);
    textEntryRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in itemMenuChoice, 0 when choiceMenuEntryFlags
 * flags it. */
int itemMenuPick(void) {
    int cursor_index;
    s16 entry_id;
    char *result;

    if (choiceMenuIsOpen(0) == 1) {
        cursor_index = choiceMenuGetResult(0);
        if (cursor_index > 0) {
            entry_id = choiceMenuEntryIds[cursor_index - 1];
            itemMenuChoice = entry_id;
            result = hitActionTextGet(entry_id, 1);
            choiceMenuPages[0] = result;
            if (choiceMenuEntryFlags[cursor_index - 1] != 0) {
                itemMenuChoice = 0;
            }
        }
    }
    return itemMenuChoice;
}

/* Techniques 1..TEC_LOOKUP_LAST of the battle lookup table. */
#define TEC_LOOKUP_LAST 59
#define TEC_MENU_STYLE 1
#define TEC_MENU_UNIT_SIZE 2

/*
 * Technique menu: lists the techniques unit has (slotCodeTest) with their EP cost, greyed out when unit cannot
 * use them now (calcEthEnableChk), and opens it.
 */
void choiceMenuOpenTecs(BattleUnit *unit)
{
    int id;
    int n;

    n = 0;
    for (id = 1; id <= TEC_LOOKUP_LAST; id++) {
        if (slotCodeTest(unit, id)) {
            choiceMenuItems[n] = battleLookupTextGet(id, RECORD_TEXT_NAME);
            choiceMenuValues[n] = calcTakeEpGet(unit, etherRecordGet(id)->tecIndex);
            choiceMenuEntryIds[n] = id;
            choiceMenuEntryFlags[n] = calcEthEnableChk(unit, id) ? 0 : CHOICE_ENTRY_DISABLED;
            n++;
        }
    }
    choiceMenuItems[n] = 0;
    choiceMenuSetActive(0, 1);
    choiceMenuOpen(0, CHOICE_MENU_X, CHOICE_MENU_Y, CHOICE_MENU_WIDTH, CHOICE_MENU_HEIGHT, TEC_MENU_STYLE,
                   TEC_MENU_UNIT_SIZE, 0, choiceMenuItems, choiceMenuValues, choiceMenuEntryFlags);
    choiceMenuPages[1] = 0;
    choiceMenuPages[0] = 0;
    choiceMenuTextTask = textEntryAlloc(CHOICE_TEXT_X, CHOICE_TEXT_Y, CHOICE_TEXT_WIDTH, CHOICE_TEXT_HEIGHT,
                                        CHOICE_TEXT_STYLE, choiceMenuPages);
}

void tecMenuClose(void) {
    choiceMenuClose(0);
    textEntryRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in tecMenuChoice, 0 when choiceMenuEntryFlags
 * flags it. */
int tecMenuPick(void) {
    int cursor_index;
    s16 entry_id;
    char *result;

    if (choiceMenuIsOpen(0) == 1) {
        cursor_index = choiceMenuGetResult(0);
        if (cursor_index > 0) {
            entry_id = choiceMenuEntryIds[cursor_index - 1];
            tecMenuChoice = entry_id;
            result = battleLookupTextGet(entry_id, 1);
            choiceMenuPages[0] = result;
            if (choiceMenuEntryFlags[cursor_index - 1] != 0) {
                tecMenuChoice = 0;
            }
        }
    }
    return tecMenuChoice;
}

/* The level record indices calcKeyBandRecords wrote for the level menu, and their count (each record menu has
 * its own pair). */

#define LEVEL_MENU_STYLE 4
#define LEVEL_MENU_UNIT_SIZE 1

/*
 * Level record menu: lists the level records of unit's key band (calcKeyBandRecords) with their required level,
 * greyed out while unit's current value does not reach it (calcCurrentMeetsRecord), and opens it.
 */
void choiceMenuOpenLevels(BattleUnit *unit)
{
    int i;
    int n;

    n = 0;
    levelMenuCount = calcKeyBandRecords(unit, levelMenuIds);
    for (i = 0; i < levelMenuCount; i++) {
        choiceMenuItems[n] = calcLevelTextGet(levelMenuIds[i], RECORD_TEXT_NAME);
        choiceMenuValues[n] = attackLevelRecordGet(levelMenuIds[i])->requiredLevel;
        choiceMenuEntryIds[n] = levelMenuIds[i];
        choiceMenuEntryFlags[n] = calcCurrentMeetsRecord(unit, levelMenuIds[i]) ? 0 : CHOICE_ENTRY_DISABLED;
        n++;
    }
    choiceMenuItems[n] = 0;
    choiceMenuSetActive(0, 1);
    choiceMenuOpen(0, CHOICE_MENU_X, CHOICE_MENU_Y, CHOICE_MENU_WIDTH, CHOICE_MENU_HEIGHT, LEVEL_MENU_STYLE,
                   LEVEL_MENU_UNIT_SIZE, 0, choiceMenuItems, choiceMenuValues, choiceMenuEntryFlags);
    choiceMenuPages[1] = 0;
    choiceMenuPages[0] = 0;
    choiceMenuTextTask = textEntryAlloc(CHOICE_TEXT_X, CHOICE_TEXT_Y, CHOICE_TEXT_WIDTH, CHOICE_TEXT_HEIGHT,
                                        CHOICE_TEXT_STYLE, choiceMenuPages);
}

void levelMenuClose(void) {
    choiceMenuClose(0);
    textEntryRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in levelMenuChoice, 0 when choiceMenuEntryFlags
 * flags it.
 * manInput passes unit; it is not used. */
int levelMenuPick(BattleUnit *unit) {
    int cursor_index;
    s16 entry_id;
    char *result;

    if (choiceMenuIsOpen(0) == 1) {
        cursor_index = choiceMenuGetResult(0);
        if (cursor_index > 0) {
            entry_id = choiceMenuEntryIds[cursor_index - 1];
            levelMenuChoice = entry_id;
            result = calcLevelTextGet(entry_id, 1);
            choiceMenuPages[0] = result;
            if (choiceMenuEntryFlags[cursor_index - 1] != 0) {
                levelMenuChoice = 0;
            }
        }
    }
    return levelMenuChoice;
}


#define PAIR_MENU_STYLE 3
#define PAIR_MENU_UNIT_SIZE 1

/*
 * Pair link menu: lists the pair links of unit's key (calcKeyPairRecords) with their required level, greyed out
 * while the levels of the pair do not reach it (calcLevelsMeetRecord), and opens it.
 */
void choiceMenuOpenPairs(BattleUnit *unit)
{
    int i;
    int n;

    n = 0;
    pairMenuCount = calcKeyPairRecords(unit, pairMenuIds);
    for (i = 0; i < pairMenuCount; i++) {
        choiceMenuItems[n] = pairLinkTextGet(pairMenuIds[i], RECORD_TEXT_NAME);
        choiceMenuValues[n] = pairLinkGet(pairMenuIds[i])->requiredLevel;
        choiceMenuEntryIds[n] = pairMenuIds[i];
        choiceMenuEntryFlags[n] = calcLevelsMeetRecord(unit, pairMenuIds[i]) ? 0 : CHOICE_ENTRY_DISABLED;
        n++;
    }
    choiceMenuItems[n] = 0;
    choiceMenuSetActive(0, 1);
    choiceMenuOpen(0, CHOICE_MENU_X, CHOICE_MENU_Y, CHOICE_MENU_WIDTH, CHOICE_MENU_HEIGHT, PAIR_MENU_STYLE,
                   PAIR_MENU_UNIT_SIZE, 0, choiceMenuItems, choiceMenuValues, choiceMenuEntryFlags);
    choiceMenuPages[1] = 0;
    choiceMenuPages[0] = 0;
    choiceMenuTextTask = textEntryAlloc(CHOICE_TEXT_X, CHOICE_TEXT_Y, CHOICE_TEXT_WIDTH, CHOICE_TEXT_HEIGHT,
                                        CHOICE_TEXT_STYLE, choiceMenuPages);
}

void pairMenuClose(void) {
    choiceMenuClose(0);
    textEntryRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in pairMenuChoice, 0 when choiceMenuEntryFlags
 * flags it.
 * manInput passes unit; it is not used. */
int pairMenuPick(BattleUnit *unit) {
    int cursor_index;
    s16 entry_id;
    char *result;

    if (choiceMenuIsOpen(0) == 1) {
        cursor_index = choiceMenuGetResult(0);
        if (cursor_index > 0) {
            entry_id = choiceMenuEntryIds[cursor_index - 1];
            pairMenuChoice = entry_id;
            result = pairLinkTextGet(entry_id, 1);
            choiceMenuPages[0] = result;
            if (choiceMenuEntryFlags[cursor_index - 1] != 0) {
                pairMenuChoice = 0;
            }
        }
    }
    return pairMenuChoice;
}


#define PAIRED_MENU_STYLE 1
#define PAIRED_MENU_UNIT_SIZE 1

/*
 * Paired technique menu (its text entry is the enemy cursor's): lists the techniques paired with the one picked
 * last (tecMenuChoice, calcPairedRecords) with their EP cost, greyed out when unit cannot use them now
 * (calcPairEntryUsable), and opens it.
 */
void choiceMenuOpenPaired(BattleUnit *unit)
{
    int i;
    int n;

    n = 0;
    pairedMenuCount = calcPairedRecords(unit, tecMenuChoice, pairedMenuIds);
    for (i = 0; i < pairedMenuCount; i++) {
        choiceMenuItems[n] = battleLookupTextGet(pairedMenuIds[i], RECORD_TEXT_NAME);
        choiceMenuValues[n] = calcTakeEpGet(unit, etherRecordGet(pairedMenuIds[i])->tecIndex);
        choiceMenuEntryIds[n] = pairedMenuIds[i];
        choiceMenuEntryFlags[n] = calcPairEntryUsable(unit, pairedMenuIds[i]) ? 0 : CHOICE_ENTRY_DISABLED;
        n++;
    }
    choiceMenuItems[n] = 0;
    choiceMenuSetActive(0, 1);
    choiceMenuOpen(0, CHOICE_MENU_X, CHOICE_MENU_Y, CHOICE_MENU_WIDTH, CHOICE_MENU_HEIGHT, PAIRED_MENU_STYLE,
                   PAIRED_MENU_UNIT_SIZE, 0, choiceMenuItems, choiceMenuValues, choiceMenuEntryFlags);
    enemyCursorPages[1] = 0;
    enemyCursorPages[0] = 0;
    pTaskEnCur = textEntryAlloc(CHOICE_TEXT_X, CHOICE_TEXT_Y, CHOICE_TEXT_WIDTH, CHOICE_TEXT_HEIGHT,
                                CHOICE_TEXT_STYLE, enemyCursorPages);
}

void enemyCursorClose(void) {
    choiceMenuClose(0);
    textEntryRemove(pTaskEnCur);
    pTaskEnCur = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in pairedMenuChoice, 0 when choiceMenuEntryFlags
 * flags it; the text goes to enemyCursorPages instead of choiceMenuPages, the list the other pickers use.
 * manInput passes unit; it is not used. */
int pairedMenuPick(BattleUnit *unit) {
    int cursor_index;
    s16 entry_id;
    char *result;

    if (choiceMenuIsOpen(0) == 1) {
        cursor_index = choiceMenuGetResult(0);
        if (cursor_index > 0) {
            entry_id = choiceMenuEntryIds[cursor_index - 1];
            pairedMenuChoice = entry_id;
            result = battleLookupTextGet(entry_id, 1);
            enemyCursorPages[0] = result;
            if (choiceMenuEntryFlags[cursor_index - 1] != 0) {
                pairedMenuChoice = 0;
            }
        }
    }
    return pairedMenuChoice;
}


#define TGT_ID_MENU_STYLE 4
#define TGT_ID_MENU_UNIT_SIZE 1

/*
 * Target id menu: lists the ids of unit's set slot flags (slotFlagCollect) with their minimum level, greyed out
 * when unit's side does not accept them (tgtSideAccepts), and opens it.
 */
void choiceMenuOpenTgtIds(BattleUnit *unit)
{
    int i;
    int n;

    n = 0;
    tgtIdMenuCount = slotFlagCollect(unit, tgtIdMenuIds);
    for (i = 0; i < tgtIdMenuCount; i++) {
        choiceMenuItems[n] = tgtIdTextGet(tgtIdMenuIds[i], RECORD_TEXT_NAME);
        choiceMenuValues[n] = attackTargetRecordGet(tgtIdMenuIds[i])->minLevel;
        choiceMenuEntryIds[n] = tgtIdMenuIds[i];
        choiceMenuEntryFlags[n] = tgtSideAccepts(unit, tgtIdMenuIds[i]) ? 0 : CHOICE_ENTRY_DISABLED;
        n++;
    }
    choiceMenuItems[n] = 0;
    choiceMenuSetActive(0, 1);
    choiceMenuOpen(0, CHOICE_MENU_X, CHOICE_MENU_Y, CHOICE_MENU_WIDTH, CHOICE_MENU_HEIGHT, TGT_ID_MENU_STYLE,
                   TGT_ID_MENU_UNIT_SIZE, 0, choiceMenuItems, choiceMenuValues, choiceMenuEntryFlags);
    choiceMenuPages[1] = 0;
    choiceMenuPages[0] = 0;
    choiceMenuTextTask = textEntryAlloc(CHOICE_TEXT_X, CHOICE_TEXT_Y, CHOICE_TEXT_WIDTH, CHOICE_TEXT_HEIGHT,
                                        CHOICE_TEXT_STYLE, choiceMenuPages);
}

void tgtIdMenuClose(void) {
    choiceMenuClose(0);
    textEntryRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in tgtIdMenuChoice, 0 when choiceMenuEntryFlags
 * flags it.
 * manInput passes unit; it is not used. */
int tgtIdMenuPick(BattleUnit *unit) {
    int cursor_index;
    s16 entry_id;
    char *result;

    if (choiceMenuIsOpen(0) == 1) {
        cursor_index = choiceMenuGetResult(0);
        if (cursor_index > 0) {
            entry_id = choiceMenuEntryIds[cursor_index - 1];
            tgtIdMenuChoice = entry_id;
            result = tgtIdTextGet(entry_id, 1);
            choiceMenuPages[0] = result;
            if (choiceMenuEntryFlags[cursor_index - 1] != 0) {
                tgtIdMenuChoice = 0;
            }
        }
    }
    return tgtIdMenuChoice;
}


/* Rows of status bits of a unit (StatBlock.activeMask; statBitTextIdGet indexes them). */
#define STAT_BIT_ROWS 4

/* The value of status bit mask of row: -1 when the bit is not active or has no value. */
int statBitTextIdGet(BattleUnit *unit, int row, int mask) {
    int *values;
    int index;
    int value;

    if ((unit->stats->activeMask[row] & mask) == 0) {
        return -1;
    }
    values = statBitValues;
    index = calcStatBit2Idx(mask);
    value = values[row * 16 + index];
    if (value != 0) {
        return value;
    }
    return -1;
}

/* Enemy cursor text entry: position, size and style passed to textEntryAlloc. */
#define ENEMY_CURSOR_X 10
#define ENEMY_CURSOR_Y 0x58
#define ENEMY_CURSOR_WIDTH 0x90
#define ENEMY_CURSOR_HEIGHT 0x14
#define ENEMY_CURSOR_STYLE 7

/* The status line shown while the command panel is open: the acting unit's status bits (statusLineStep walks them
 * with statBitTextIdGet) and the frames left before the next one. */

/* Opens the status line text entry (pTaskEnCur) when the unit has any status bit set. */
void statusLineOpen(BattleUnit *unit)
{
    u16 *bits = unit->stats->activeMask;
    int row;

    for (row = 0; row < STAT_BIT_ROWS; row++) {
        if (bits[row] != 0) {
            break;
        }
    }
    if (row < STAT_BIT_ROWS) {
        enemyCursorPages[1] = 0;
        enemyCursorPages[0] = 0;
        pTaskEnCur = textEntryAlloc(ENEMY_CURSOR_X, ENEMY_CURSOR_Y, ENEMY_CURSOR_WIDTH, ENEMY_CURSOR_HEIGHT,
                                    ENEMY_CURSOR_STYLE, enemyCursorPages);
        enemyCursorTimer = 0;
        enemyCursorRow = 0;
        enemyCursorBit = 1;
    }
}

/* Frames each status stays in the enemy cursor; statBitTextIdGet's result when the bit is not set. */
#define ENEMY_CURSOR_FRAMES 30
#define STAT_BIT_NONE -1

/*
 * Every ENEMY_CURSOR_FRAMES frames moves the status line to the next status bit unit has (from the bit after the
 * shown one, over the rows from the shown row on, then the shown row again from bit 1) and shows its text
 * (tableStringGet), or no text when none is set.
 */
void statusLineStep(BattleUnit *unit)
{
    int i;
    int row;
    int bit;
    int value;

    if (enemyCursorTimer <= 0) {
        enemyCursorTimer = ENEMY_CURSOR_FRAMES;
        enemyCursorBit <<= 1;
        value = STAT_BIT_NONE;
        for (i = 0; i < STAT_BIT_ROWS; i++) {
            row = (enemyCursorRow + i) % STAT_BIT_ROWS;
            for (bit = enemyCursorBit; bit & 0xFFFF; bit <<= 1) {
                value = statBitTextIdGet(unit, row, bit);
                if (value != STAT_BIT_NONE) {
                    enemyCursorRow = row;
                    enemyCursorBit = bit;
                    break;
                }
            }
            if (value != STAT_BIT_NONE) {
                break;
            }
            enemyCursorBit = 1;
        }
        if (value == STAT_BIT_NONE) {
            row = enemyCursorRow;
            for (bit = enemyCursorBit; bit & 0xFFFF; bit <<= 1) {
                value = statBitTextIdGet(unit, row, bit);
                if (value != STAT_BIT_NONE) {
                    enemyCursorRow = row;
                    enemyCursorBit = bit;
                    break;
                }
            }
        }
        if (value != STAT_BIT_NONE) {
            enemyCursorPages[0] = tableStringGet(value, 1);
        } else {
            enemyCursorPages[0] = 0;
        }
    }
    enemyCursorTimer--;
}

void enemyCursorTextRemove(void)
{
    if (pTaskEnCur != 0) {
        textEntryRemove(pTaskEnCur);
        pTaskEnCur = 0;
    }
}

/* Menu selections of the battle menu (BattleState.selection) and the commands (CmdListBody.command) they give. */
enum {
    SELECT_TARGET_LAST = 10,                /* 1..10: key-row entries (tableKeyEntryGet) */
    SELECT_WAIT = 11,
    SELECT_LOOKUP,
    SELECT_HIT_ACTION,
    SELECT_PAIR,
    SELECT_SNAPSHOT,
    SELECT_SWAP,
    SELECT_LEVEL,
    SELECT_LEAVE,
    SELECT_TGT_ID,
    SELECT_END = 20
};

/* unitScriptEventSend kinds of the command panel opening and closing. */
#define BATTLE_PANEL_OPEN_EVENT 9
#define BATTLE_PANEL_CLOSE_EVENT 10

/*
 * One frame of the battle menu of unit: steps the menu mode (key input, command panel, the record menus and their
 * target cursors) on the pressed pad bits and stores the selection it ends with (0 while the menu runs).
 */
void manInput(BattleUnit *unit)
{
    UnitScriptEvent event;
    BattleUnit *target;
    TecRecord *tec;
    int selection;
    int pad;
    int index;
    int choice;

    selection = 0;
    pad = dataPadRead(1);
    if (tgtSelectState.selection != 0 && tgtSelectState.selection != DATA_KEY_NONE) {
        tgtSelectState.selectionKind = tgtSelectState.selection;
    }
    switch (tgtSelectState.menuMode) {
    case BATTLE_MENU_KEYS:
        if (pad & PAD_CROSS) {
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
            tgtSelectState.active = 2;
            menuWepSpPanelReset();
            dataTargetHide();
        } else {
            selection = keyCmdGet(unit);
            if (selection != DATA_KEY_NONE && selection != 0) {
                tgtSelectState.menuMode = BATTLE_MENU_KEY_HELD;
                dataTargetHide();
            }
        }
        break;
    case BATTLE_MENU_KEY_HELD:
        selection = keyCmdGet(unit);
        if (selection == DATA_KEY_CANCEL) {
            tgtSelectState.menuMode = BATTLE_MENU_KEYS;
            keyBuffClearUnit(unit);
        }
        break;
    case BATTLE_MENU_PANEL_OPEN:
        menuWepSpPanelShow();
        dataTargetHide();
        seVoiceStartDefault(SE_MENU_OPEN);
        event.unit = unit;
        unitScriptEventSend(BATTLE_PANEL_OPEN_EVENT, &event);
        statusLineOpen(unit);
        tgtSelectState.menuMode = BATTLE_MENU_PANEL;
        /* fall through */
    case BATTLE_MENU_PANEL:
        if (pad & PAD_CROSS) {
            tgtSelectState.active = 1;
            tgtSelectState.menuMode = BATTLE_MENU_KEYS;
            tgtSelectState.selectionKind = 0;
            tgtSelectState.selection = 0;           /* stored again at the end; the original has both stores */
            menuWepSpPanelHide();
            seVoiceStartDefault(SE_MENU_CLOSE);
            event.unit = unit;
            unitScriptEventSend(BATTLE_PANEL_CLOSE_EVENT, &event);
            enemyCursorTextRemove();
            break;
        }
        choice = menuWepSpPanelUpdate(unit);
        if (choice != DATA_KEY_NONE && choice != 0) {
            menuWepSpPanelHide();
            event.unit = unit;
            unitScriptEventSend(BATTLE_PANEL_CLOSE_EVENT, &event);
            enemyCursorTextRemove();
            switch (choice) {
            case SELECT_WAIT:
                tgtSelectState.menuMode = BATTLE_MENU_WAIT;
                break;
            case SELECT_LOOKUP:
                choiceMenuOpenTecs(unit);
                tgtSelectState.menuMode = BATTLE_MENU_TECS;
                break;
            case SELECT_HIT_ACTION:
                choiceMenuOpenItems(unit);
                tgtSelectState.menuMode = BATTLE_MENU_ITEMS;
                break;
            case SELECT_SNAPSHOT:
                tgtSelectState.menuMode = BATTLE_MENU_SNAPSHOT;
                break;
            case SELECT_LEVEL:
                choiceMenuOpenLevels(unit);
                tgtSelectState.menuMode = BATTLE_MENU_LEVELS;
                break;
            case SELECT_TGT_ID:
                choiceMenuOpenTgtIds(unit);
                tgtSelectState.menuMode = BATTLE_MENU_TGT_IDS;
                break;
            case SELECT_PAIR:
                choiceMenuOpenPairs(unit);
                tgtSelectState.menuMode = BATTLE_MENU_PAIRS;
                break;
            case SELECT_SWAP:
                menuTargetListCursorReset();
                menuTargetListShow();
                tgtSelectState.menuMode = BATTLE_MENU_SWAP;
                break;
            case SELECT_LEAVE:
                tgtSelectState.menuMode = BATTLE_MENU_LEAVE;
                break;
            default:
                tgtSelectState.menuMode = BATTLE_MENU_END;
                break;
            }
        } else {
            selection = choice;
            statusLineStep(unit);
        }
        break;
    case BATTLE_MENU_END:
        tgtSelectState.menuMode = BATTLE_MENU_KEYS;
        selection = SELECT_END;
        menuWepSpPanelReset();
        dataObjHide();
        break;
    case BATTLE_MENU_WAIT:
        tgtSelectState.menuMode = BATTLE_MENU_END;
        selection = SELECT_WAIT;
        break;
    case BATTLE_MENU_TECS:
        /* The pick stays in a local here (it is also calcPairedRecords' code); the other menus test the stored
         * field, which gives the original register choice. */
        index = tecMenuPick();
        tgtSelectState.selectionIndex = index;
        if (pad & PAD_CIRCLE) {
            if (index != 0) {
                seVoiceStartDefault(SE_CONFIRM);
                tecMenuClose();
                tec = tecRecordGet(etherRecordGet(tgtSelectState.selectionIndex)->tecIndex);
                curSelGrpSet(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_TEC_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_TRIANGLE) {
            if (index == 0 || calcPairedRecords(unit, index, pairedMenuIds) <= 0) {
                seVoiceStartDefault(SE_REFUSE);
                break;
            }
            seVoiceStartDefault(SE_CONFIRM);
            tecMenuClose();
            choiceMenuOpenPaired(unit);
            tgtSelectState.menuMode = BATTLE_MENU_PAIRED;
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            tecMenuClose();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_TEC_TARGET:
        tec = tecRecordGet(etherRecordGet(tgtSelectState.selectionIndex)->tecIndex);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        curTgtProcInit(&target, unit, tec->flags);
        curTgtProc(&target, tec->flags);
        tgtUnitSet(target);
        dataTargetShow(target, &tec->flags);
        if (pad & PAD_CIRCLE) {
            selection = SELECT_LOOKUP;
            seVoiceStartDefault(SE_CONFIRM);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_END;
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            choiceMenuOpenTecs(unit);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_TECS;
        }
        break;
    case BATTLE_MENU_PAIRED:
        tgtSelectState.selectionIndex = pairedMenuPick(unit);
        if (pad & PAD_CIRCLE) {
            if (tgtSelectState.selectionIndex != 0) {
                seVoiceStartDefault(SE_CONFIRM);
                enemyCursorClose();
                tec = tecRecordGet(etherRecordGet(tgtSelectState.selectionIndex)->tecIndex);
                curSelGrpSet(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_TEC_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            enemyCursorClose();
            choiceMenuOpenTecs(unit);
            tgtSelectState.menuMode = BATTLE_MENU_TECS;
        }
        break;
    case BATTLE_MENU_ITEMS:
        tgtSelectState.selectionIndex = itemMenuPick();
        if (pad & PAD_CIRCLE) {
            if (tgtSelectState.selectionIndex != 0) {
                seVoiceStartDefault(SE_CONFIRM);
                choiceMenuTextClose();
                tec = tecRecordGet(usableItemRecordGet(tgtSelectState.selectionIndex)->entry);
                curSelGrpSet(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_ITEM_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            choiceMenuTextClose();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_ITEM_TARGET:
        tec = tecRecordGet(usableItemRecordGet(tgtSelectState.selectionIndex)->entry);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        curTgtProcInit(&target, unit, tec->flags);
        curTgtProc(&target, tec->flags);
        tgtUnitSet(target);
        dataTargetShow(target, &tec->flags);
        if (pad & PAD_CIRCLE) {
            selection = SELECT_HIT_ACTION;
            seVoiceStartDefault(SE_CONFIRM);
            dataBoxDecCategory1(tgtSelectState.selectionIndex);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_END;
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            choiceMenuOpenItems(unit);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_ITEMS;
        }
        break;
    case BATTLE_MENU_LEVELS:
        tgtSelectState.selectionIndex = levelMenuPick(unit);
        if (pad & PAD_CIRCLE) {
            if (tgtSelectState.selectionIndex != 0) {
                seVoiceStartDefault(SE_CONFIRM);
                levelMenuClose();
                tec = tecRecordGet(attackLevelRecordGet(tgtSelectState.selectionIndex)->tecIndex);
                curSelGrpSet(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_LEVEL_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            levelMenuClose();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_LEVEL_TARGET:
        tec = tecRecordGet(attackLevelRecordGet(tgtSelectState.selectionIndex)->tecIndex);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        curTgtProcInit(&target, unit, tec->flags);
        curTgtProc(&target, tec->flags);
        tgtUnitSet(target);
        dataTargetShow(target, &tec->flags);
        if (pad & PAD_CIRCLE) {
            if (battleEffectApplies(unit, attackLevelRecordGet(tgtSelectState.selectionIndex)->tecIndex, target) != 0) {
                selection = SELECT_LEVEL;
                seVoiceStartDefault(SE_CONFIRM);
                dataTargetHide();
                tgtSelectState.menuMode = BATTLE_MENU_END;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            choiceMenuOpenLevels(unit);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_LEVELS;
        }
        break;
    case BATTLE_MENU_TGT_IDS:
        tgtSelectState.selectionIndex = tgtIdMenuPick(unit);
        if (pad & PAD_CIRCLE) {
            if (tgtSelectState.selectionIndex != 0) {
                seVoiceStartDefault(SE_CONFIRM);
                tgtIdMenuClose();
                tec = tecRecordGet(attackTargetRecordGet(tgtSelectState.selectionIndex)->actionIndex);
                curSelGrpSet(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_TGT_ID_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            tgtIdMenuClose();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_TGT_ID_TARGET:
        tec = tecRecordGet(attackTargetRecordGet(tgtSelectState.selectionIndex)->actionIndex);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        curTgtProcInit(&target, unit, tec->flags);
        curTgtProc(&target, tec->flags);
        tgtUnitSet(target);
        dataTargetShow(target, &tec->flags);
        if (pad & PAD_CIRCLE) {
            selection = SELECT_TGT_ID;
            seVoiceStartDefault(SE_CONFIRM);
            unitLevelAdd(unit, -attackTargetRecordGet(tgtSelectState.selectionIndex)->minLevel);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_END;
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            choiceMenuOpenTgtIds(unit);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_TGT_IDS;
        }
        break;
    case BATTLE_MENU_PAIRS:
        tgtSelectState.selectionIndex = pairMenuPick(unit);
        if (pad & PAD_CIRCLE) {
            if (tgtSelectState.selectionIndex != 0) {
                seVoiceStartDefault(SE_CONFIRM);
                pairMenuClose();
                tec = tecRecordGet(pairLinkGet(tgtSelectState.selectionIndex)->tecIndex);
                curSelGrpSet(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_PAIR_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            pairMenuClose();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_PAIR_TARGET:
        tec = tecRecordGet(pairLinkGet(tgtSelectState.selectionIndex)->tecIndex);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        curTgtProcInit(&target, unit, tec->flags);
        curTgtProc(&target, tec->flags);
        tgtUnitSet(target);
        dataTargetShow(target, &tec->flags);
        if (pad & PAD_CIRCLE) {
            selection = SELECT_PAIR;
            seVoiceStartDefault(SE_CONFIRM);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_END;
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            choiceMenuOpenPairs(unit);
            dataTargetHide();
            tgtSelectState.menuMode = BATTLE_MENU_PAIRS;
        }
        break;
    case BATTLE_MENU_IDLE:
        break;
    case BATTLE_MENU_SNAPSHOT:
        curSelGrpSet(0, 0);
        curPosProcInit(unit);
        curPosProc(unit, &tgtSelectState.selectionIndex);
        dataObjShow();
        if (pad & PAD_CIRCLE) {
            selection = SELECT_SNAPSHOT;
            seVoiceStartDefault(SE_CONFIRM);
            tgtSelectState.menuMode = BATTLE_MENU_END;
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_SWAP:
        tgtSelectState.selectionIndex = menuTargetListUpdate(unit);
        if (pad & PAD_CIRCLE) {
            selection = SELECT_SWAP;
            seVoiceStartDefault(SE_CONFIRM);
            menuTargetListHide();
            tgtSelectState.menuMode = BATTLE_MENU_END;
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            menuTargetListHide();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_LEAVE:
        tgtSelectState.menuMode = BATTLE_MENU_END;
        selection = SELECT_LEAVE;
        break;
    }
    tgtSelectState.selection = selection;
}

/* The command of the battle menu's selection for the unit: key-row entries 1..10 (entries 1 and 2 are no command
 * for party keys 1..10), the record-based selections with their technique and record index, and the end of the
 * turn (CMD_KIND_REFRESH for a party unit with selection kind 1 or 2, else CMD_KIND_THINK_END). Returns 1. */
int convKey2unit(BattleUnit *unit, CmdListBody *command, BattleUnit *target)
{
    int selection;
    short code;
    EtherRecord *lookup;
    UsableItemRecord *hitAction;
    EsSpecialRecord *level;
    DoubleTechRecord *pair;
    SpecialAttackRecord *tgtId;

    selection = tgtSelectState.selection;
    if (selection == 0) {
    } else if (selection == SELECT_END) {
        if ((unsigned int)(unit->stats->keyUnsigned - 1) < 10
            && (unsigned int)(tgtSelectState.selectionKind - 1) < 2) {
            command->command = CMD_KIND_REFRESH;
            tgtSelectState.selectionKind = 0;
            tgtSelectState.selection = 0;
        } else {
            command->command = CMD_KIND_THINK_END;
        }
    } else if (selection <= SELECT_TARGET_LAST) {
        if ((unsigned int)(unit->stats->keyUnsigned - 1) < 10 && (unsigned int)(selection - 1) < 2) {
            command->command = CMD_KIND_NONE;
        } else {
            code = tableKeyEntryGet(unit, tgtSelectState.selection, target);
            command->command = CMD_KIND_TECHNIQUE;
            command->code = code;
            command->arg.target = (int)target;
        }
    } else {
        switch (selection) {
        case SELECT_WAIT:
            command->command = CMD_KIND_WAIT;
            break;
        case SELECT_LOOKUP:
            command->command = CMD_KIND_LOOKUP;
            lookup = etherRecordGet(tgtSelectState.selectionIndex);
            command->arg.target = (int)target;
            command->code = lookup->tecIndex;
            command->pairIndex = tgtSelectState.selectionIndex;
            break;
        case SELECT_HIT_ACTION:
            command->command = CMD_KIND_ITEM;
            hitAction = usableItemRecordGet(tgtSelectState.selectionIndex);
            command->arg.target = (int)target;
            command->code = hitAction->entry;
            command->pairIndex = tgtSelectState.selectionIndex;
            break;
        case SELECT_LEVEL:
            command->command = CMD_KIND_TECHNIQUE;
            level = attackLevelRecordGet(tgtSelectState.selectionIndex);
            command->arg.target = (int)target;
            command->code = level->tecIndex;
            command->pairIndex = tgtSelectState.selectionIndex;
            break;
        case SELECT_PAIR:
            command->command = CMD_KIND_TECHNIQUE;
            pair = pairLinkGet(tgtSelectState.selectionIndex);
            command->arg.target = (int)target;
            command->code = pair->tecIndex;
            command->pairIndex = tgtSelectState.selectionIndex;
            break;
        case SELECT_TGT_ID:
            command->command = CMD_KIND_TECHNIQUE;
            tgtId = attackTargetRecordGet(tgtSelectState.selectionIndex);
            command->arg.target = (int)target;
            command->code = tgtId->actionIndex;
            command->pairIndex = tgtSelectState.selectionIndex;
            break;
        case SELECT_SNAPSHOT:
            command->command = CMD_KIND_SNAPSHOT;
            command->code = tgtSelectState.selectionIndex;
            break;
        case SELECT_SWAP:
            command->command = CMD_KIND_SWAP;
            command->code = tgtSelectState.selectionIndex;
            break;
        case SELECT_LEAVE:
            command->command = CMD_KIND_LEAVE;
            break;
        default:
            command->command = CMD_KIND_NONE;
            break;
        }
    }
    return 1;
}

/* Returns the entry of the key row of the unit's table at index (1-based); the target the callers pass is not used. */

s16 tableKeyEntryGet(BattleUnit *owner, int index, void *target) {
    TableKeyRow *row = tableKeyRowGet(owner->stats->key);

    return row->entries[index - 1];
}

/* The target slot of the menu: targets[1] while a pair menu mode is open and its flag is set, else targets[0]. */
BattleUnit **tgtUnitAdrGet(void)
{
    switch (tgtSelectState.menuMode) {
    case BATTLE_MENU_TEC_TARGET:
    case BATTLE_MENU_ITEM_TARGET:
    case BATTLE_MENU_LEVEL_TARGET:
    case BATTLE_MENU_TGT_ID_TARGET:
    case BATTLE_MENU_PAIR_TARGET:
        if (tgtSelectState.targetFlags != 0 && (tgtSelectState.targetFlags[0] & 1)) {
            return &tgtSelectState.targets[1];
        }
        return &tgtSelectState.targets[0];
    }
    return &tgtSelectState.targets[0];
}

BattleUnit *tgtUnitGet(void)
{
    return *tgtUnitAdrGet();
}

void tgtUnitSet(BattleUnit *unit)
{
    *tgtUnitAdrGet() = unit;
}

struct BattleState *manWorkGet(void) {
    return &tgtSelectState;
}


int thinkNoGet(void) {
    return thinkNo;
}

void thinkNoSet(int value) {
    thinkNo = value;
}

/*
 * gameWork (main): encounterTarget is the battle's enemy group number (the MONSTER SET debug row of debug_entry.c sets
 * it, monsSetNoGet of sb_natives.c reads it), encounterFlags the cf event bits (XS1 keeps cfEvent in its game loop
 * state; scene_finish.c tests bit 0x20 for the time limit).
 */
int cfMonsSetNoGet(void) {
    return gameWork.encounterTarget;
}

void cfMonsSetNoSet(int monsSetNo) {
    gameWork.encounterTarget = monsSetNo;
}

int cfEventGet(void) {
    return gameWork.encounterFlags;
}

void cfEventSet(int cfEvent) {
    gameWork.encounterFlags = cfEvent;
}

/* Returns the party table ov01PartyTable (0x14-byte records, one per party slot). */
TgtSlot *ov01PartyListGet(void) {
    return ov01PartyTable;
}

/* Gattai code word: GATTAI_CODE_BASE plus the gattai id. gattaiCodeGet reads it back. */

#define GATTAI_CODE_BASE 0x16000
/* Story flag field (8 bits at flag 0x1EE) holding the current gattai id. */
#define GATTAI_ID_FLAG 0x1EE
#define GATTAI_ID_FLAG_BITS 8


/* The gattai code: from the story flag when a gattai id is set there,
 * else the code gattaiCodeSet stored. */
int gattaiCodeGet(void) {
    int id = xglFlagsGet(GATTAI_ID_FLAG, GATTAI_ID_FLAG_BITS);
    int code;

    if (id == 0) {
        code = gattaiCode;
    } else {
        code = id + GATTAI_CODE_BASE;
    }
    return code;
}

void gattaiCodeSet(int code) {
    gattaiCode = code;
}


/* Resets every ov01 table and initializes the subsystems, then defaults the gattai code to 0x16001. */
void ov01InitAll(void) {
    ov01ResetAll();
    sbInit();
    configInit();
    dataBatDatLoad();
    dataPlUnitInit();
    dataBoxClearAll();
    sdvInitSpecialWork();
    gattaiCodeSet(0x16001);
}

/* Value of each status bit, four rows of sixteen (statBitTextIdGet); 0 = no value. */
int statBitValues[64] = {
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0, 0, 0, 15, 0,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 0, 29, 0,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 0, 0,
    44, 45, 46, 47, 48, 0, 0, 0, 49, 50, 51, 52, 53, 54, 0, 0,
};

static const char nullUnitName[8] = "NULL";
