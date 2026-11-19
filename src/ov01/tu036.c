#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "main/tu048.h"
#include "main/tu021.h"
#include "main/tu124.h"
#include "ov01/tu005.h"
#include "ov01/tu008.h"
#include "main/tu051.h"
#include "ov01/tu010.h"
#include "ov01/tu022.h"
#include "ov01/tu023.h"
#include "ov01/tu033.h"
#include "ov01/tu036.h"
#include "ov01/tu042.h"
#include "ov01/tu091.h"
#include "ov01/tu043.h"
#include "ov01/tu056.h"



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
/* The enemy-cursor task: enemyCursorOpen opens it, enemyCursorClose and objCurRemove
 * remove it. X1 names the same global the enemy-cursor task (battle_init.c). */
/* Text task that the choice-menu openers allocate (the textEntryAlloc result) and the close functions remove. */
/* The choice menu's text page list for textEntryAlloc: the text of the entry under the cursor (set by the pickers),
 * then 0. */
/* The text page list of the enemy cursor (and of the paired technique menu): the text (set by pairedMenuPick and
 * enemyCursorStep), then 0. */

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

/* Items 1..ITEM_ID_LAST; an item can be picked in battle when its HitAction flags have ITEM_BATTLE_USE. */
#define ITEM_ID_LAST 40
#define ITEM_BATTLE_USE 0x4000
#define ITEM_MENU_STYLE 2
#define ITEM_MENU_UNIT_SIZE 2

/*
 * Item menu: lists the items the data box holds (dataBoxChkCategory1) that can be used in battle, with their
 * count, greyed out when unit cannot use them on any target (tgtRecordCanTarget), and opens it.
 */
void choiceMenuOpenItems(Ov01Object *unit)
{
    int id;
    int count;
    int n;

    n = 0;
    for (id = 1; id <= ITEM_ID_LAST; id++) {
        count = dataBoxChkCategory1(id);
        if (count > 0 && (hitActionGet(id)->flags & ITEM_BATTLE_USE)) {
            choiceMenuItems[n] = hitActionTextGet(id, RECORD_TEXT_NAME);
            choiceMenuValues[n] = count;
            choiceMenuEntryIds[n] = id;
            choiceMenuEntryFlags[n] = tgtRecordCanTarget(unit, id) ? 0 : CHOICE_ENTRY_DISABLED;
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
    objRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}

int choiceMenuIsOpen(int);
int choiceMenuGetResult(int);

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
 * use them now (calcEntryUsable), and opens it.
 */
void choiceMenuOpenTecs(Ov01Object *unit)
{
    int id;
    int n;

    n = 0;
    for (id = 1; id <= TEC_LOOKUP_LAST; id++) {
        if (slotCodeTest(unit, id)) {
            choiceMenuItems[n] = battleLookupTextGet(id, RECORD_TEXT_NAME);
            choiceMenuValues[n] = calcEpCost(unit, battleLookupGet(id)->tecIndex);
            choiceMenuEntryIds[n] = id;
            choiceMenuEntryFlags[n] = calcEntryUsable(unit, id) ? 0 : CHOICE_ENTRY_DISABLED;
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

void func_00A98180(void) {
    choiceMenuClose(0);
    objRemove(choiceMenuTextTask);
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
void choiceMenuOpenLevels(Ov01Object *unit)
{
    int i;
    int n;

    n = 0;
    levelMenuCount = calcKeyBandRecords(unit, levelMenuIds);
    for (i = 0; i < levelMenuCount; i++) {
        choiceMenuItems[n] = calcLevelTextGet(levelMenuIds[i], RECORD_TEXT_NAME);
        choiceMenuValues[n] = calcLevelRecordGet(levelMenuIds[i])->requiredLevel;
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

void func_00A98408(void) {
    choiceMenuClose(0);
    objRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in levelMenuChoice, 0 when choiceMenuEntryFlags
 * flags it.
 * battleSelectStep passes unit; it is not used. */
int levelMenuPick(Ov01Object *unit) {
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
void choiceMenuOpenPairs(Ov01Object *unit)
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

void func_00A98690(void) {
    choiceMenuClose(0);
    objRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in pairMenuChoice, 0 when choiceMenuEntryFlags
 * flags it.
 * battleSelectStep passes unit; it is not used. */
int pairMenuPick(Ov01Object *unit) {
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
void choiceMenuOpenPaired(Ov01Object *unit)
{
    int i;
    int n;

    n = 0;
    pairedMenuCount = calcPairedRecords(unit, tecMenuChoice, pairedMenuIds);
    for (i = 0; i < pairedMenuCount; i++) {
        choiceMenuItems[n] = battleLookupTextGet(pairedMenuIds[i], RECORD_TEXT_NAME);
        choiceMenuValues[n] = calcEpCost(unit, battleLookupGet(pairedMenuIds[i])->tecIndex);
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
    objRemove(pTaskEnCur);
    pTaskEnCur = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in pairedMenuChoice, 0 when choiceMenuEntryFlags
 * flags it; the text goes to enemyCursorPages instead of choiceMenuPages, the list the other pickers use.
 * battleSelectStep passes unit; it is not used. */
int pairedMenuPick(Ov01Object *unit) {
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
void choiceMenuOpenTgtIds(Ov01Object *unit)
{
    int i;
    int n;

    n = 0;
    tgtIdMenuCount = slotFlagCollect(unit, tgtIdMenuIds);
    for (i = 0; i < tgtIdMenuCount; i++) {
        choiceMenuItems[n] = tgtIdTextGet(tgtIdMenuIds[i], RECORD_TEXT_NAME);
        choiceMenuValues[n] = tgtIdRecordGet(tgtIdMenuIds[i])->minLevel;
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

void func_00A98BB0(void) {
    choiceMenuClose(0);
    objRemove(choiceMenuTextTask);
    choiceMenuTextTask = 0;
}


/* Cursor entry of choice slot 0 (choiceMenuGetResult): caches its id in tgtIdMenuChoice, 0 when choiceMenuEntryFlags
 * flags it.
 * battleSelectStep passes unit; it is not used. */
int tgtIdMenuPick(Ov01Object *unit) {
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


/* Rows of status bits of a unit (StatBlock.activeMask; statBitValueGet indexes them). */
#define STAT_BIT_ROWS 4

/* The value of status bit mask of row: -1 when the bit is not active or has no value. */
int statBitValueGet(Ov01Object *unit, int row, int mask) {
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

/* The status shown by the enemy cursor: row and bit mask (enemyCursorStep walks them with statBitValueGet) and
 * the frames left before the next one. */

/* Opens the enemy cursor's text entry when the unit has any status bit set. */
void enemyCursorOpen(Ov01Object *unit)
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

/* Frames each status stays in the enemy cursor; statBitValueGet's result when the bit is not set. */
#define ENEMY_CURSOR_FRAMES 30
#define STAT_BIT_NONE -1

/*
 * Every ENEMY_CURSOR_FRAMES frames moves the enemy cursor to the next status bit unit has (from the bit after the
 * shown one, over the rows from the shown row on, then the shown row again from bit 1) and shows its text
 * (tableStringGet), or no text when none is set.
 */
void enemyCursorStep(Ov01Object *unit)
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
                value = statBitValueGet(unit, row, bit);
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
                value = statBitValueGet(unit, row, bit);
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

void objCurRemove(void)
{
    if (pTaskEnCur != 0) {
        objRemove(pTaskEnCur);
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
void battleSelectStep(Ov01Object *unit)
{
    UnitScriptEvent event;
    Ov01Object *target;
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
            selection = dataKeyInputStep(unit);
            if (selection != DATA_KEY_NONE && selection != 0) {
                tgtSelectState.menuMode = BATTLE_MENU_KEY_HELD;
                dataTargetHide();
            }
        }
        break;
    case BATTLE_MENU_KEY_HELD:
        selection = dataKeyInputStep(unit);
        if (selection == DATA_KEY_CANCEL) {
            tgtSelectState.menuMode = BATTLE_MENU_KEYS;
            func_00A88E98(unit);
        }
        break;
    case BATTLE_MENU_PANEL_OPEN:
        menuWepSpPanelShow();
        dataTargetHide();
        seVoiceStartDefault(SE_MENU_OPEN);
        event.unit = unit;
        unitScriptEventSend(BATTLE_PANEL_OPEN_EVENT, &event);
        enemyCursorOpen(unit);
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
            objCurRemove();
            break;
        }
        choice = menuWepSpPanelUpdate(unit);
        if (choice != DATA_KEY_NONE && choice != 0) {
            menuWepSpPanelHide();
            event.unit = unit;
            unitScriptEventSend(BATTLE_PANEL_CLOSE_EVENT, &event);
            objCurRemove();
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
            enemyCursorStep(unit);
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
                func_00A98180();
                tec = tecRecordGet(battleLookupGet(tgtSelectState.selectionIndex)->tecIndex);
                dataStateSetMode(-1, tec->flags);
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
            func_00A98180();
            choiceMenuOpenPaired(unit);
            tgtSelectState.menuMode = BATTLE_MENU_PAIRED;
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            func_00A98180();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_TEC_TARGET:
        tec = tecRecordGet(battleLookupGet(tgtSelectState.selectionIndex)->tecIndex);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        dataTargetListInit(&target, unit, tec->flags);
        dataUnitCursorCycle(&target, tec->flags);
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
                tec = tecRecordGet(battleLookupGet(tgtSelectState.selectionIndex)->tecIndex);
                dataStateSetMode(-1, tec->flags);
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
                tec = tecRecordGet(hitActionGet(tgtSelectState.selectionIndex)->entry);
                dataStateSetMode(-1, tec->flags);
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
        tec = tecRecordGet(hitActionGet(tgtSelectState.selectionIndex)->entry);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        dataTargetListInit(&target, unit, tec->flags);
        dataUnitCursorCycle(&target, tec->flags);
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
                func_00A98408();
                tec = tecRecordGet(calcLevelRecordGet(tgtSelectState.selectionIndex)->tecIndex);
                dataStateSetMode(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_LEVEL_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            func_00A98408();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_LEVEL_TARGET:
        tec = tecRecordGet(calcLevelRecordGet(tgtSelectState.selectionIndex)->tecIndex);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        dataTargetListInit(&target, unit, tec->flags);
        dataUnitCursorCycle(&target, tec->flags);
        tgtUnitSet(target);
        dataTargetShow(target, &tec->flags);
        if (pad & PAD_CIRCLE) {
            if (battleEffectApplies(unit, calcLevelRecordGet(tgtSelectState.selectionIndex)->tecIndex, target) != 0) {
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
                func_00A98BB0();
                tec = tecRecordGet(tgtIdRecordGet(tgtSelectState.selectionIndex)->actionIndex);
                dataStateSetMode(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_TGT_ID_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            func_00A98BB0();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_TGT_ID_TARGET:
        tec = tecRecordGet(tgtIdRecordGet(tgtSelectState.selectionIndex)->actionIndex);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        dataTargetListInit(&target, unit, tec->flags);
        dataUnitCursorCycle(&target, tec->flags);
        tgtUnitSet(target);
        dataTargetShow(target, &tec->flags);
        if (pad & PAD_CIRCLE) {
            selection = SELECT_TGT_ID;
            seVoiceStartDefault(SE_CONFIRM);
            unitLevelAdd(unit, -tgtIdRecordGet(tgtSelectState.selectionIndex)->minLevel);
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
                func_00A98690();
                tec = tecRecordGet(pairLinkGet(tgtSelectState.selectionIndex)->tecIndex);
                dataStateSetMode(-1, tec->flags);
                tgtSelectState.menuMode = BATTLE_MENU_PAIR_TARGET;
            } else {
                seVoiceStartDefault(SE_REFUSE);
            }
        } else if (pad & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            func_00A98690();
            tgtSelectState.menuMode = BATTLE_MENU_PANEL_OPEN;
        }
        break;
    case BATTLE_MENU_PAIR_TARGET:
        tec = tecRecordGet(pairLinkGet(tgtSelectState.selectionIndex)->tecIndex);
        tgtSelectState.targetFlags = &tec->flags;
        target = tgtUnitGet();
        dataTargetListInit(&target, unit, tec->flags);
        dataUnitCursorCycle(&target, tec->flags);
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
        dataStateSetMode(0, 0);
        dataEntryListInit(unit);
        dataEntryCursorStep(unit, &tgtSelectState.selectionIndex);
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
int battleSelectionGet(Ov01Object *unit, CmdListBody *command, Ov01Object *target)
{
    int selection;
    short code;
    BattleLookupEntry *lookup;
    HitAction *hitAction;
    CalcLevelRecord *level;
    PairLink *pair;
    TgtIdRecord *tgtId;

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
            lookup = battleLookupGet(tgtSelectState.selectionIndex);
            command->arg.target = (int)target;
            command->code = lookup->tecIndex;
            command->pairIndex = tgtSelectState.selectionIndex;
            break;
        case SELECT_HIT_ACTION:
            command->command = CMD_KIND_ITEM;
            hitAction = hitActionGet(tgtSelectState.selectionIndex);
            command->arg.target = (int)target;
            command->code = hitAction->entry;
            command->pairIndex = tgtSelectState.selectionIndex;
            break;
        case SELECT_LEVEL:
            command->command = CMD_KIND_TECHNIQUE;
            level = calcLevelRecordGet(tgtSelectState.selectionIndex);
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
            tgtId = tgtIdRecordGet(tgtSelectState.selectionIndex);
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

s16 tableKeyEntryGet(StatOwner *owner, int index, void *target) {
    unsigned char *record_table = tableKeyRowGet(owner->stats->key);

    s16 *entry = (s16 *)(record_table + (index - 1) * sizeof(s16));

    return *entry;
}

/* The target slot of the menu: targets[1] while a pair menu mode is open and its flag is set, else targets[0]. */
Ov01Object **tgtUnitAdrGet(void)
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

Ov01Object *tgtUnitGet(void)
{
    return *tgtUnitAdrGet();
}

void tgtUnitSet(Ov01Object *unit)
{
    *tgtUnitAdrGet() = unit;
}

struct BattleState *battleStateGet(void) {
    return &tgtSelectState;
}


int thinkNoGet(void) {
    return thinkNo;
}

void thinkNoSet(int value) {
    thinkNo = value;
}

/*
 * gameWork (main): encounterTarget is the battle's enemy group number (the MONSTER SET debug row of tu072 sets it,
 * monsSetNoGet of tu046 reads it), encounterFlags the cf event bits (XS1 keeps cfEvent in its game loop state;
 * tu039 tests bit 0x20 for the time limit).
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

extern unsigned char ov01PartyTable[];

/* Returns the party table ov01PartyTable (0x14-byte records, one per party slot). */
void *ov01PartyListGet(void) {
    return &ov01PartyTable;
}

/* Gattai code word: GATTAI_CODE_BASE plus the gattai id. gattaiCodeGet reads it back. */

#define GATTAI_CODE_BASE 0x16000
/* Story flag field (8 bits at flag 0x1EE) holding the current gattai id. */
#define GATTAI_ID_FLAG 0x1EE
#define GATTAI_ID_FLAG_BITS 8

int xglFlagsGet(int bit_offset, int bit_count);

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

void battleSlotsInit(void);
void sbInit(void);
void configInit(void);

/* Resets every ov01 table and initializes the subsystems, then defaults the gattai code to 0x16001. */
void ov01InitAll(void) {
    ov01ResetAll();
    sbInit();
    configInit();
    dataBatDatLoad();
    battleSlotsInit();
    dataBoxClearAll();
    sdvInitSpecialWork();
    gattaiCodeSet(0x16001);
}

/* Value of each status bit, four rows of sixteen (statBitValueGet); 0 = no value. */
int statBitValues[64] = {
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0, 0, 0, 15, 0,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 0, 29, 0,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 0, 0,
    44, 45, 46, 47, 48, 0, 0, 0, 49, 50, 51, 52, 53, 54, 0, 0,
};

static const char D_00ABA698[8] = "NULL";
