/*
 * The BATTLE CONFIG debug menu: a main page (THINK, MONSTER SET, the three
 * party rows, invincible (MUTEKI) and ICHIGEKI rows, LEARN ALL, CF FLAG, GATTAI, MEMBER), an equip page
 * for the selected unit, and a script page.
 */
#include "common.h"
#include "main/xgl_render.h"
#include "main/xgl_thread.h"
#include "main/xgl_font.h"
#include "main/xgl_cd.h"
#include "main/pt_member.h"
#include "main/menu_scene.h"
#include "shared.h"
#include "ov01/calc.h"
#include "ov01/debug_entry.h"
#include "ov01/choice_menu.h"
#include "main/pad.h"
#include "ov01/data_box.h"
#include "ov01/data_file_load.h"
#include "ov01/scene.h"
#include "main/xgl_studio.h"
#include "main/res_get_path.h"
#include "main/save_party_init.h"

/* Debug menu layout: the menu starts at (CONFIG_MENU_X, CONFIG_MENU_Y), rows are CONFIG_ROW_HEIGHT lines apart,
 * and one value column is CONFIG_GLYPH_WIDTH pixels wide. */
#define CONFIG_MENU_X 12
#define CONFIG_MENU_Y 12
#define CONFIG_ROW_HEIGHT 10
#define CONFIG_GLYPH_WIDTH 8
#define CONFIG_PARTY_COUNT 3
#define CONFIG_NO_MAX 999
#define CONFIG_GATTAI_BASE 0x16000
#define SKILL_CYCLE_LEN 5
#define CONFIG_PARTY_CHARA_MAX 0xF
#define CONFIG_PARTY_POS_MAX 0x1F
#define CONFIG_CF_FLAG_MAX 0xFFFF
#define CONFIG_GATTAI_MAX 0x51
#define CONFIG_MEMBER_MAX 0x3FF
#define CONFIG_UNIT_INDEX_MAX 15
#define CONFIG_SKILL_BYTE_MAX 64
#define CONFIG_ACC_MAX 40
#define CONFIG_LEARN_BYTE_MAX 255
#define CONFIG_CLOTH_MIN 0x10001
#define CONFIG_CLOTH_MAX 0x10028
#define CONFIG_SUBPILOT_MAX 0xA
#define CONFIG_LEVEL_MAX 99
#define CONFIG_SCRIPT_NO_MAX 9999

/* Pages of the debug menu, the values of configPageState. */
enum ConfigPage {
    CONFIG_PAGE_MAIN = 0,
    CONFIG_PAGE_EQUIP = 1
};

/* Callees defined outside this allocation. Results are declared only where a caller reads them. */
int valLR(int *, int, int, int);
int configPlayer(int slot);
int configPlPos(int slot);
extern int configMonsSetNo;
extern int configCFFlagValue;
extern int configCpUDispFlag;
extern int configPlMutekiFlag;
extern int configEnMutekiFlag;
extern int configPlIchigekiFlag;
extern int configStockMugenFlag;
extern int configLearnAllFlag;

/* Mute and ICHIGEKI rows of the main page: constant 0 in this build. */
int configPlMutekiGet(void) {
    return 0;
}

int configEnMutekiGet(void) {
    return 0;
}

int configPlIchigekiGet(void) {
    return 0;
}

int configStockMugenGet(void) {
    return 0;
}

void configDummy(void) {

}

void configDummy_00AA9368(void) {

}

void configDummy_00AA9370(void) {

}

void configDummy_00AA9378(void) {

}

extern int configThinkNo;
static enum ConfigPage configPageState;
static int configMainColumn;
static int configMainRow;
static int configEquipColumn;
static int configEquipRow;
static int configScriptNumber;

/* Clears the page state and the words after it, then sets the debug defaults of the main page. */
void configInit(void)
{
    configMainRow = 0;
    configEquipRow = 0;
    configPageState = CONFIG_PAGE_MAIN;
    configMainColumn = 0;
    configEquipColumn = 0;
    configThinkNo = thinkNoGet();
    configMonsSetNo = cfMonsSetNoGet();
    configCFFlagValue = cfEventGet();
    configCpUDispFlag = 0;
    configScriptNumber = 0;
    ptReset();
}

/* Callees of the debug menu, prototypes from their definitions (src/main/pt_member.c, src/ov01/choice_menu.c,
 * src/ov01/scene.c). */
int configMenuFrame(int param);


/* Runs the debug menu frame until it reports done. Mode 1 copies the three party status bytes first. */
int configMenuRun(int mode)
{
    int slot;
    TgtSlot *party;
    int clearGreen;

    xglRenderClearFrame();
    clearGreen = 64;
    ClearEnv.value.color_a = 128;
    ClearEnv.value.color_g = clearGreen;
    ClearEnv.value.color_b = 0;
    ClearEnv.value.color_r = 0;
    studioSceneFlagSet();
    if (mode == 1) {
        party = ov01PartyListGet();
        slot = 0;
        do {
            party[slot].charaId = ptGetStatusBlock()->memberIds[slot];
            slot++;
        } while (slot < CONFIG_PARTY_COUNT);
    }
    while (configMenuFrame(mode) != 1) {
        if (ov01SoftResetPressed() != 0) {
            return 0;
        }
        xglSleep();
    }
    studioSceneFlagClear();
    xglSleep();
    return 1;
}

int valUD(int *value, int min, int max, int step);

/* Row cursor of a debug menu: counts the handlers, moves the cursor, clears the marker column when the
 * cursor row changes, draws the marker and runs the handler of the current row. The original has no return
 * statement: the caller's menuDone takes whatever the row handler left in the return register. */
int configMenuUpdate(int (**rowHandlers)(void), int baseX, int baseY, int *markerColumn, int *cursorRow)
{
    int handlerCount;
    int oldRow;

    handlerCount = 0;
    while (rowHandlers[handlerCount] != 0) {
        handlerCount++;
    }
    oldRow = *cursorRow;
    valUD(cursorRow, 0, handlerCount - 1, 1);
    if (oldRow != *cursorRow) {
        *markerColumn = 0;
    }
    xglFontDebugPrintf(baseX + *markerColumn * CONFIG_GLYPH_WIDTH, baseY + *cursorRow * CONFIG_ROW_HEIGHT + CONFIG_ROW_HEIGHT, ">");
    rowHandlers[*cursorRow]();
}

void configMainPageDraw(int x, int y);
void configStockMugenApply(void);
void configLearnAllApply(void);
extern int (*configMainRows[])(void);
extern int (*configEquipRows[])(void);
void configEquipPageDraw(int x, int y);

/* Frame of the debug menu: runs the handler of the current page. Cancel on the equip page returns to the main
 * page, cancel on the main page ends the menu. When it ends, applies LEARN ALL and STOCK MUGEN, copies the three
 * party ids to the status blocks and, for param 0, loads the think script. Returns 1 when the menu is done. */
int configMenuFrame(int param)
{
    int menuDone;
    TgtSlot *party;

    menuDone = 0;
    switch (configPageState) {
    case CONFIG_PAGE_MAIN:
        menuDone = configMenuUpdate(configMainRows, CONFIG_MENU_X, CONFIG_MENU_Y, &configMainColumn, &configMainRow);
        configMainPageDraw(CONFIG_MENU_X, CONFIG_MENU_Y);
        break;
    case CONFIG_PAGE_EQUIP:
        menuDone = configMenuUpdate(configEquipRows, CONFIG_MENU_X, CONFIG_MENU_Y, &configEquipColumn, &configEquipRow);
        configEquipPageDraw(CONFIG_MENU_X, CONFIG_MENU_Y);
        break;
    }
    if (configCancelPressed()) {
        switch (configPageState) {
        case CONFIG_PAGE_MAIN:
            menuDone = 1;
            break;
        case CONFIG_PAGE_EQUIP:
            configPageState = CONFIG_PAGE_MAIN;
            break;
        }
    }
    if (menuDone != 0) {
        int slot;

        if (configLearnAllFlag == 1) {
            configLearnAllApply();
        }
        if (configStockMugenGet()) {
            configStockMugenApply();
        }
        {
            party = ov01PartyListGet();
            for (slot = 0; slot < CONFIG_PARTY_COUNT; slot++) {
                ptGetStatusBlock()->memberIds[slot] = party[slot].charaId;
            }
        }
        if (configCpUDispFlag == 0) {
            xglRenderCpUDispOff();
        } else {
            xglRenderCpUDispOn();
        }
        if (param == 0) {
            dataThinkScriptLoad(configThinkNo);
        }
        return 1;
    }
    return 0;
}

extern int configUnitIndex;

/* Main page of the debug menu: the rows of the page, one every 10 lines below y. */
void configMainPageDraw(int x, int y)
{
    TgtSlot *partyRecords;

    partyRecords = ov01PartyListGet();
    xglFontDebugPrintf(x, y, "\x0b--BATTLE CONFIG MAIN--");
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0c\x80\0\0 BATTLE START");
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b THINK= %3d", configThinkNo);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b MONSTER SET= %3d", configMonsSetNo);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b PLAYER1= %s", batUnitNameGet(partyRecords[0].charaId));
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b PLAYER2= %s", batUnitNameGet(partyRecords[1].charaId));
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b PLAYER3= %s", batUnitNameGet(partyRecords[2].charaId));
    xglFontDebugPrintf(x, y += CONFIG_ROW_HEIGHT, "\x0b PLAYER1 POS= %2d", partyRecords[0].position);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b PLAYER2 POS= %2d", partyRecords[1].position);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b PLAYER3 POS= %2d", partyRecords[2].position);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b PL MUTEKI= %1d", configPlMutekiGet());
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b EN MUTEKI= %1d", configEnMutekiGet());
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b PL ICHIGEKI= %1d", configPlIchigekiGet());
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b STOCK MUGEN= %1d", configStockMugenGet());
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b LEARN ALL= %1d", configLearnAllFlag);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0c\x80\0\0 TENT");
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b CF FLAG= %4x", configCFFlagValue);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b CPU DISP= %1d", configCpUDispFlag);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b GATTAI= %2d", gattaiCodeGet() - CONFIG_GATTAI_BASE);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b MEMBER= %4x", ptGetStatusBlock()->presentMask);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b EQUIP= %s", batUnitNameGet(configUnitIndex));
}


/* Cloth name of a unit: clothIds[unit - 1] resolved to a name, "NULL" when missing. */
void *unitClothNameGet(int unit)
{
    PartyStatusBlock *status;
    void *clothName;

    if (unit < 0xB) {
        status = ptGetStatusBlock();
        clothName = xglCdFindFileName(status->clothIds[unit - 1]);
        if (clothName != 0) {
            return clothName;
        }
    }
    return "NULL";
}

int unitLearnMaskGet(int unit);

/* Equip page: one text row per field of the selected unit, ten pixels apart. */
void configEquipPageDraw(int x, int y)
{
    StatBlock *unit;
    unsigned int learnMask;
    int subPilotId;

    unit = battleSlotGet(configUnitIndex);
    subPilotId = unit->linkedSlots[1];
    learnMask = unitLearnMaskGet(configUnitIndex);
    xglFontDebugPrintf(x, y, "\x0b--BATTLE CONFIG EQUIP--");
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b EQUIP SKILL= %2d  %2d  %2d  %2d", unit->effectCode[0], unit->effectCode[1], unit->effectCode[2], unit->effectCode[3]);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b LEARN SKILL= %2x  %2x  %2x  %2x", learnMask >> 24, (learnMask >> 16) & 0xff, (learnMask & 0xff00) >> 8, learnMask & 0xff);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b ACC= %2d  %2d  %2d  %2d", unit->equipId[0], unit->equipId[1], unit->equipId[2], unit->equipId[3]);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b CLOTH= %s", unitClothNameGet(configUnitIndex));
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b SUB PILOT= %s", batUnitNameGet(subPilotId));
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b LV UP = %2d", unit->expLevel);
    y += CONFIG_ROW_HEIGHT;
    xglFontDebugPrintf(x, y, "\x0b EQUIP= %s", batUnitNameGet(configUnitIndex));
}

/* STOCK MUGEN apply: run by configMenuFrame only when the
 * STOCK MUGEN row value is nonzero. Sets the stock of boost gauge record 0 to its maximum stock count. */
void configStockMugenApply(void) {
    struct BoostGauge *stockRecord;

    stockRecord = boostGaugeSelect(0);
    stockRecord->stock = boostGaugeSelect(0)->maxStockCount;
}


/* LEARN ALL apply (run by configMenuFrame when the LEARN ALL flag is set). */
void configLearnAllApply(void)
{
    int slot;
    int code;
    int repeat;

    for (slot = 1; slot < 11; slot++) {
        for (code = 1; code < 60; code++) {
            dataSkillLearnSet(slot, code);
        }
    }
    for (slot = 1; slot < 17; slot++) {
        slotPairSet(slot);
    }
    for (slot = 1; slot < 41; slot++) {
        for (repeat = 98; repeat >= 0; repeat--) {
            dataBoxIncCategory1(slot);
        }
    }
    for (slot = 1; slot < 41; slot++) {
        for (repeat = 98; repeat >= 0; repeat--) {
            dataBoxIncCategory2(slot);
        }
    }
    battleSlotMarkSet(2, 4);
}

/* Bitmask of unit flags 1..32 (the LEARN SKILL row). */
int unitLearnMaskGet(int unit)
{
    int mask;
    int flagBit;
    int flagId;

    if (unit == 0) {
        return 0;
    }
    mask = 0;
    flagBit = 1;
    for (flagId = 1; flagId <= UNIT_LEARN_FLAG_COUNT; flagId++) {
        if (dataEtherLearnGet(unit, flagId) != 0) {
            mask |= flagBit;
        }
        flagBit *= 2;
    }
    return mask;
}

/* Sets flags 1..32 of the unit for the bits of mask and clears the others (the LEARN SKILL row). */
void unitLearnMaskSet(int unit, int mask)
{
    int flagBit;
    int flagId;

    if (unit == 0) {
        return;
    }
    flagBit = 1;
    for (flagId = 1; flagId <= UNIT_LEARN_FLAG_COUNT; flagId++) {
        if ((mask & flagBit) != 0) {
            dataEtherLearnSet(unit, flagId);
        } else {
            dataEtherLearnClear(unit, flagId);
        }
        flagBit *= 2;
    }
}

extern int (*configScriptRows[])(void);
extern char configScriptPrefixName[];
extern const char *configScriptPrefix;
static unsigned char configScriptImage[0xC000];
static char configScriptPath[256];
int sprintf(char *, const char *, ...);
char *strcat(char *, const char *);
/* Local on purpose: the original callers pass one argument, while sbLinkImage (sb_vm.h) takes a second, unread
 * library word that sbLibraryLink passes; the full prototype would make these callers load $5 as well. */
int sbLinkImage(void *image);
void sbStartImage(void *image);

/* Script page: the first two rows of text, then the script file path is built, read into the script image, linked and started. */
int configScriptPage(void)
{
    int result;

    result = configMenuUpdate(configScriptRows, CONFIG_MENU_X, CONFIG_MENU_Y, &configMainColumn, &configMainRow);
    xglFontDebugPrintf(CONFIG_MENU_X, CONFIG_MENU_Y, "\x0b--SCRIPT CONFIG--");
    xglFontDebugPrintf(CONFIG_MENU_X, CONFIG_MENU_Y + CONFIG_ROW_HEIGHT, "\x0b SCRIPT= %4d", configScriptNumber);
    if (configCancelPressed() == 0 && result == 0) {
        return 0;
    }
    sprintf(configScriptPath, "%s%04d", configScriptPrefix, configScriptNumber);
    strcat(configScriptPath, ".sb");
    xglCdReadFile(configScriptPath, configScriptImage, 0, CD_NOTIFY_CURRENT);
    if (sbLinkImage(configScriptImage) == 0) {
        while (1) {
        }
    }
    sbStartImage(configScriptImage);
    return 1;
}


/* Nonzero while the PAD_CIRCLE bit of PadData[0].pressed is set. */
int configStart(void)
{
    int selectBit;

    selectBit = PadData[0].pressed & PAD_CIRCLE;
    return selectBit != 0;
}

/* THINK row: edits the AI script number (0..999) and passes it on. */
int configThink(void)
{
    valLR(&configThinkNo, 0, CONFIG_NO_MAX, 1);
    thinkNoSet(configThinkNo);
    return 0;
}

/* MONSTER SET row: edits the enemy group number (0..999) and passes it on. */
int configMonsSet(void) {
    valLR(&configMonsSetNo, 0, CONFIG_NO_MAX, 1);
    cfMonsSetNoSet(configMonsSetNo);
    return 0;
}

void configPlayer1(void) {
    configPlayer(0);
}

void configPlayer2(void) {
    configPlayer(1);
}

void configPlayer3(void) {
    configPlayer(2);
}

/* PLAYERn row: cycles the character of party slot `slot` (0..0xF). */
int configPlayer(int slot)
{
    TgtSlot *records;
    int charaId;

    records = ov01PartyListGet();
    charaId = records[slot].charaId;
    valLR(&charaId, 0, CONFIG_PARTY_CHARA_MAX, 1);
    records[slot].charaId = charaId;
    return 0;
}

void configPlPos1(void) {
    configPlPos(0);
}

void configPlPos2(void) {
    configPlPos(1);
}

void configPlPos3(void) {
    configPlPos(2);
}

/* PLAYERn POS row: cycles the position of party slot `slot` (0..0x1F). */
int configPlPos(int slot)
{
    TgtSlot *records;
    int position;

    records = ov01PartyListGet();
    position = records[slot].position;
    valLR(&position, 0, CONFIG_PARTY_POS_MAX, 1);
    records[slot].position = position;
    return 0;
}

/* Handler of the PL MUTEKI row: slot 10 of the main-page table configMainRows.
 * The flag it edits is never read in ov01; the row prints the constant from configPlMutekiGet. */
int configPlMuteki(void) {
    valLR(&configPlMutekiFlag, 0, 1, 1);
    return 0;
}

int configEnMuteki(void) {
    valLR(&configEnMutekiFlag, 0, 1, 1);
    return 0;
}

int configPlIchigeki(void) {
    valLR(&configPlIchigekiFlag, 0, 1, 1);
    return 0;
}

int configStockMugen(void) {
    valLR(&configStockMugenFlag, 0, 1, 1);
    return 0;
}

/* LEARN ALL row: a 0..1 flag; when it is 1 the page runs configLearnAllApply. */
int configLearnAll(void) {
    valLR(&configLearnAllFlag, 0, 1, 1);
    return 0;
}


/* Row 14 of the main page: when the select flag is set, refreshes the battle slots. Returns 0. */
int configBattleSlotsRefresh(void)
{
    if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
        battleSlotsRestoreAll();
    }
    return 0;
}

/* CF FLAG row: a 16-bit value written through cfEventSet. */
int configCFEvent(void) {
    valLR(&configCFFlagValue, 0, CONFIG_CF_FLAG_MAX, 1);
    cfEventSet(configCFFlagValue);
    return 0;
}

/* CP U DISP row: a 0..1 flag that picks the display routine on the main page. */
int configCpUDisp(void) {
    valLR(&configCpUDispFlag, 0, 1, 1);
    return 0;
}


/* GATTAI row: an id offset by CONFIG_GATTAI_BASE in 1..CONFIG_GATTAI_MAX. */
int configGattai(void)
{
    int gattaiId;

    gattaiId = gattaiCodeGet() - CONFIG_GATTAI_BASE;
    valLR(&gattaiId, 1, CONFIG_GATTAI_MAX, 1);
    gattaiCodeSet(gattaiId + CONFIG_GATTAI_BASE);
    return 0;
}

int configMember(void)
{
    int member;

    member = ptGetStatusBlock()->presentMask;
    valLR(&member, 0, CONFIG_MEMBER_MAX, 1);
    ptGetStatusBlock()->presentMask = member;
    return 0;
}

/* UNIT row of the config page: edits the selected unit index (1..CONFIG_UNIT_INDEX_MAX). While the select flag is
 * set and a unit is selected, switches the page state to the equip page and clears configEquipColumn. Returns 0. */
int configUnitSelect(void)
{
    valLR(&configUnitIndex, 1, CONFIG_UNIT_INDEX_MAX, 1);
    if ((PadData[0].pressed & PAD_CIRCLE) != 0 && configUnitIndex != 0) {
        configPageState = CONFIG_PAGE_EQUIP;
        configEquipColumn = 0;
    }
    return 0;
}

extern int configLearnSkillColumns[SKILL_CYCLE_LEN];
extern int configEquipSkillColumns[SKILL_CYCLE_LEN];

/* EQUIP SKILL row: the skill table entry (configEquipSkillColumns) selects one byte of the unit's StatBlock, edited through the debug value page. */
int equipSkill(void)
{
    int skillIndex;
    StatBlock *unit;
    int byteValue;

    unit = battleSlotGet(configUnitIndex);
    skillIndex = 0;
    while (skillIndex < SKILL_CYCLE_LEN && configEquipColumn != configEquipSkillColumns[skillIndex]) {
        skillIndex++;
    }
    if (PadData[0].pressed & PAD_CIRCLE) {
        if (skillIndex < SKILL_CYCLE_LEN - 1) {
            skillIndex = skillIndex + 1;
        } else {
            skillIndex = 1;
        }
        configEquipColumn = configEquipSkillColumns[skillIndex];
    }
    if (skillIndex) {
        byteValue = unit->effectCode[skillIndex - 1];
        valLR(&byteValue, 0, CONFIG_SKILL_BYTE_MAX, 1);
        unit->effectCode[skillIndex - 1] = byteValue;
    }
    return 0;
}

/* LEARN SKILL row: edits one byte of the unit's learn mask, the byte chosen by the skill table entry. */
int configLearnSkill(void)
{
    int skillIndex;

    battleSlotGet(configUnitIndex);
    skillIndex = 0;
    while (skillIndex < SKILL_CYCLE_LEN && configEquipColumn != configLearnSkillColumns[skillIndex]) {
        skillIndex++;
    }
    if (PadData[0].pressed & PAD_CIRCLE) {
        int nextEntry;

        if (skillIndex < SKILL_CYCLE_LEN - 1) {
            skillIndex = skillIndex + 1;
        } else {
            skillIndex = 1;
        }
        nextEntry = configLearnSkillColumns[skillIndex];
        configEquipColumn = nextEntry;
    }
    if (skillIndex) {
        unsigned int learnMask;
        int byteValue;

        learnMask = unitLearnMaskGet(configUnitIndex);
        byteValue = (learnMask >> ((4 - skillIndex) << 3)) & 0xff;
        valLR(&byteValue, 0, CONFIG_LEARN_BYTE_MAX, 1);
        learnMask = (learnMask & ~(0xffu << ((4 - skillIndex) << 3))) | (byteValue << ((4 - skillIndex) << 3));
        unitLearnMaskSet(configUnitIndex, learnMask);
    }
    return 0;
}

extern int configAccColumns[SKILL_CYCLE_LEN];

/* ACC row: the skill table entry (configAccColumns) selects one halfword of the unit's StatBlock, edited through the debug value page. */
int configAcc(void)
{
    int skillIndex;
    StatBlock *unit;
    int accValue;

    unit = battleSlotGet(configUnitIndex);
    skillIndex = 0;
    while (skillIndex < SKILL_CYCLE_LEN && configEquipColumn != configAccColumns[skillIndex]) {
        skillIndex++;
    }
    if (PadData[0].pressed & PAD_CIRCLE) {
        if (skillIndex < SKILL_CYCLE_LEN - 1) {
            skillIndex = skillIndex + 1;
        } else {
            skillIndex = 1;
        }
        configEquipColumn = configAccColumns[skillIndex];
    }
    if (skillIndex) {
        accValue = unit->equipId[skillIndex - 1];
        valLR(&accValue, 0, CONFIG_ACC_MAX, 1);
        unit->equipId[skillIndex - 1] = accValue;
    }
    return 0;
}

/* CLOTH row: edits the cloth id in the unit table entry unit + 1,
 * the entry unitClothNameGet resolves to the name printed on the equip page. */
int configCloth(void)
{
    int clothId;

    clothId = ptGetStatusBlock()->clothIds[configUnitIndex - 1];
    valLR(&clothId, CONFIG_CLOTH_MIN, CONFIG_CLOTH_MAX, 1);
    ptGetStatusBlock()->clothIds[configUnitIndex - 1] = clothId;
    return 0;
}

/* The base unit of a slot: batUnitRecordGet returns its StatBlock (the LV UP row reads its expLevel). */

/* SUB PILOT row: edits the sub-pilot id (0..0xA) of the selected unit. */
int configSubPilot(void)
{
    StatBlock *unit;
    int subPilotId;

    unit = battleSlotGet(configUnitIndex);
    subPilotId = unit->linkedSlots[1];
    valLR(&subPilotId, 0, CONFIG_SUBPILOT_MAX, 1);
    unit->linkedSlots[1] = subPilotId;
    return 0;
}

void unitSetLevel(int slotIndex, int level);

/* LV UP row: the minimum comes from the base unit; a change goes through unitSetLevel. */
int configLvUp(void)
{
    StatBlock *unit;
    StatBlock *baseUnit;
    int level;

    unit = battleSlotGet(configUnitIndex);
    baseUnit = batUnitRecordGet(configUnitIndex);
    level = unit->expLevel;
    valLR(&level, baseUnit->expLevel, CONFIG_LEVEL_MAX, 1);
    if (level != unit->expLevel) {
        unitSetLevel(configUnitIndex, level);
    }
    return 0;
}

void unitSetLevel(int slotIndex, int level)
{
    StatBlock *unit;
    StatBlock *baseUnit;
    GrowthRecord *growthRecord;
    SlotRecord *slotRecord;
    UnitExpEntry *expTable;
    int counter;

    unit = battleSlotGet(slotIndex);
    baseUnit = batUnitRecordGet(slotIndex);
    growthRecord = growthRecordGet(slotIndex);
    slotRecord = slotRecordByIndex(slotIndex);
    for (counter = 0; counter < 8; counter++) {
        slotRecord->growth[counter] = growthRecord->start[counter];
    }
    unit->expLevel = baseUnit->expLevel;
    unit->physAttack = baseUnit->physAttack;
    unit->physDefence = baseUnit->physDefence;
    unit->magAttack = baseUnit->magAttack;
    unit->magDefence = baseUnit->magDefence;
    unit->hitRate = baseUnit->hitRate;
    unit->evadeRate = baseUnit->evadeRate;
    unit->maxHp = baseUnit->maxHp;
    unit->maxEp = baseUnit->maxEp;
    counter = baseUnit->expLevel;
    if (counter < level) {
        counter = level - counter;
        do {
            counter--;
            unit->expLevel++;
            unitGrowthStep(slotIndex);
        } while (counter != 0);
    }
    unit->expLevel = level;
    unit->hp = unit->maxHp;
    unit->ep = unit->maxEp;
    expTable = dataExpTblGet();
    slotRecord->expCounter = expTable[unit->expLevel].exp;
}


/* SCRIPT NO row: edits the script number (0..9999) and reports bit 0x20 of the option flags. */
int configScriptNo(void)
{
    int flags;

    valLR(&configScriptNumber, 0, CONFIG_SCRIPT_NO_MAX, 1);
    flags = PadData[0].pressed & PAD_CIRCLE;
    return flags != 0;
}

/* Handlers of the row tables return nothing or a "leave the row" flag. */
typedef int (*ConfigRowHandler)(void);

int configThinkNo = 0;
int configMonsSetNo = 0;
int configCFFlagValue = 0;
int configCpUDispFlag = 0;
int configUnitIndex = 1;
int configPlMutekiFlag = 0;
int configEnMutekiFlag = 0;
int configPlIchigekiFlag = 0;
int configStockMugenFlag = 0;
int configLearnAllFlag = 0;
const char *configScriptPrefix = configScriptPrefixName;

/* Main page rows, in screen order; the list ends with a null entry. */
ConfigRowHandler configMainRows[21] = {
    configStart,
    configThink,
    configMonsSet,
    (ConfigRowHandler) configPlayer1,
    (ConfigRowHandler) configPlayer2,
    (ConfigRowHandler) configPlayer3,
    (ConfigRowHandler) configPlPos1,
    (ConfigRowHandler) configPlPos2,
    (ConfigRowHandler) configPlPos3,
    configPlMuteki,
    configEnMuteki,
    configPlIchigeki,
    configStockMugen,
    configLearnAll,
    configBattleSlotsRefresh,
    configCFEvent,
    configCpUDisp,
    configGattai,
    configMember,
    configUnitSelect,
    0
};

ConfigRowHandler configEquipRows[8] = {
    equipSkill,
    configLearnSkill,
    configAcc,
    configCloth,
    configSubPilot,
    configLvUp,
    configUnitSelect,
    0
};

ConfigRowHandler configScriptRows[2] = {
    configScriptNo,
    0
};

int configEquipSkillColumns[SKILL_CYCLE_LEN] = { 0, 13, 16, 20, 23 };
int configLearnSkillColumns[SKILL_CYCLE_LEN] = { 0, 13, 16, 20, 23 };
int configAccColumns[SKILL_CYCLE_LEN] = { 0, 5, 8, 11, 14 };

/* Nonzero while bit 0x40 (PAD_CROSS) of PadData[0].pressed is set: the back button of the debug menus. */
int configCancelPressed(void) {
    return PadData[0].pressed & PAD_CROSS;
}

/* Up/down step of a debug menu value held with D-pad up or down; wraps at min and max. Returns nonzero when it changed. */
int valUD(int *value, int min, int max, int step) {
    int original = *value;
    int current = original;
    int increased = original + step;
    u16 heldButtons = PadData[0].repeated;

    if (heldButtons & PAD_DOWN) {
        *value = increased;
        current = increased;
    } else if (heldButtons & PAD_UP) {
        int decreased = original - step;

        *value = decreased;
        current = decreased;
    }
    if (max < current) {
        *value = min;
        current = min;
    }
    if (current < min) {
        *value = max;
        current = max;
    }
    return current != original;
}

/* Left/right step of a debug menu value (R1/L1 move by ten steps); wraps at min and max. Returns nonzero when it changed. */
int valLR(int *value, int min, int max, int step) {
    int original = *value;
    int current = original;
    u16 heldButtons = PadData[0].repeated;

    if (heldButtons & PAD_R1) {
        int raised = original + 10 * step;

        *value = raised;
        current = raised;
    }
    if (heldButtons & PAD_L1) {
        int lowered = current - 10 * step;

        *value = lowered;
        current = lowered;
    }
    if (heldButtons & PAD_RIGHT) {
        int shifted = current + step;

        *value = shifted;
        current = shifted;
    } else if (heldButtons & PAD_LEFT) {
        int shifted = current - step;

        *value = shifted;
        current = shifted;
    }
    if (max < current) {
        *value = min;
        current = min;
    }
    if (current < min) {
        *value = max;
        current = max;
    }
    return current != original;
}
