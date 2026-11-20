#include "common.h"
#include "ov01/deco_flags.h"
#include "ov01/ov01_slot.h"
#include "ov01/menu.h"
#include "ov01/menu_wep_sp_panel.h"
#include "ov01/act_step_icon.h"

/* The flag bit decoFlagsSetAll and decoFlagsClearAll set or clear in the slot flags of an object: OV01_SLOT_SKIP_DRAW. */
#define DECO_FLAG_BIT OV01_SLOT_SKIP_DRAW
/* Entries of menuStatRecords and of pStockObj. */
#define DECO_STAT_RECORD_COUNT 3
#define DECO_PATH_COUNT 5

/*
 * Sets flag bit 0x4 in the first word of every entry: the three of menuStatRecords and the five of
 * pStockObj (null array entries are skipped) and the five single pointers (not checked for null).
 * Declared int with no value returned: battleAfter (tgt_select.c) only matches against a value-returning
 * callee, and this body has no result to return.
 */
int decoFlagsSetAll(void) {
    MenuObject *record;
    int index;

    for (index = 0; index < DECO_STAT_RECORD_COUNT; index++) {
        record = menuStatRecords[index];
        if (record != 0) {
            record->slotFlags |= DECO_FLAG_BIT;
        }
    }
    for (index = 0; index < DECO_PATH_COUNT; index++) {
        record = pStockObj[index];
        if (record != 0) {
            record->slotFlags |= DECO_FLAG_BIT;
        }
    }
    menuCursor->slotFlags |= DECO_FLAG_BIT;
    menuIconListObj->slotFlags |= DECO_FLAG_BIT;
    menuValuePanelObj->slotFlags |= DECO_FLAG_BIT;
    actStepIconObj->slotFlags |= DECO_FLAG_BIT;
    menuTgtStatusPanel->slotFlags |= DECO_FLAG_BIT;
}

/* Clears flag bit 0x4 in every group that decoFlagsSetAll sets. */
void decoFlagsClearAll(void) {
    MenuObject *record;
    int index;

    for (index = 0; index < DECO_STAT_RECORD_COUNT; index++) {
        record = menuStatRecords[index];
        if (record != 0) {
            record->slotFlags &= ~DECO_FLAG_BIT;
        }
    }
    for (index = 0; index < DECO_PATH_COUNT; index++) {
        record = pStockObj[index];
        if (record != 0) {
            record->slotFlags &= ~DECO_FLAG_BIT;
        }
    }
    menuCursor->slotFlags &= ~DECO_FLAG_BIT;
    menuIconListObj->slotFlags &= ~DECO_FLAG_BIT;
    menuValuePanelObj->slotFlags &= ~DECO_FLAG_BIT;
    actStepIconObj->slotFlags &= ~DECO_FLAG_BIT;
    menuTgtStatusPanel->slotFlags &= ~DECO_FLAG_BIT;
}

/* Sets flag bit 0x4 in the first group only (menuStatRecords). */
void decoFlagsSetFirst(void) {
    MenuObject *record;
    int index;

    for (index = 0; index < DECO_STAT_RECORD_COUNT; index++) {
        record = menuStatRecords[index];
        if (record != 0) {
            record->slotFlags |= DECO_FLAG_BIT;
        }
    }
}

/* Clears flag bit 0x4 in the first group only (menuStatRecords). */
void decoFlagsClearFirst(void) {
    MenuObject *record;
    int index;

    for (index = 0; index < DECO_STAT_RECORD_COUNT; index++) {
        record = menuStatRecords[index];
        if (record != 0) {
            record->slotFlags &= ~DECO_FLAG_BIT;
        }
    }
}
