#include "common.h"
#include "main/xgl_font.h"
#include "ov01/unit_cmd.h"
#include "ov01/calc.h"
#include "ov01/obj_slot.h"
#include "ov01/menu_panel.h"
#include "ov01/menu_tex_state.h"
#include "ov01/choice_menu.h"
#include "ov01/scene.h"
#include "ov01/gr_gp_init.h"
#include "ov01/menu_wep_sp_panel.h"
#include "ov01/battle_init.h"

#define MENU_WEPSP_ROW_STRIDE 21        /* height of one wep-special row, also the footer step */
#define MENU_TARGET_ROW_STRIDE 30       /* height of one target list row, also the cursor step */
#define MENU_PULSE_PHASE_HALF 0x80      /* the pulse folds back at half of its 0..0xFF phase */
#define MENU_PULSE_ALPHA_MIN 0x2A       /* lowest fixed alpha of the pulse */
#define MENU_PULSE_ALPHA_BLEND 0x68     /* low word of the ALPHA_1 value set by menuPulseAlphaSet */
#define MENU_STATUS_TEXT_COLOUR 0x00FFFFFC
#define MENU_HP_DISPLAY_MAX 99999       /* above this the HP is drawn as five sprites, not digits */
#define MENU_TGT_STATUS_ALPHA 0x4000000068ULL /* ALPHA_1 value of the target status panel */
#define MENU_GS_ALPHA_1 0x42                  /* GS register ALPHA_1 (include/gs.h has it, this file does not include it) */

/* dataTid2WepSp: searches the 0xC-byte wepSpTbl entries for the technique id in $4. */
typedef struct WepSpEntry {
    int tid;
    int srcX; /* +0x04: the source x of the icon, the srcX argument of menuStatSpriteRGB */
    int srcY; /* +0x08: the source y of the icon, the srcY argument of menuStatSpriteRGB */
} WepSpEntry;

extern WepSpEntry wepSpTbl[];


void menuWepSpPanelStep(MenuState *panel);
void menuWepSpPanelDraw(MenuState *panel);

extern const ColourPair menuWepSpRowColors;

void menuRollPercentDraw(BattleUnit *unit);

/*
 * menuTargetListNew: the same constructor as
 * menuWepSpPanelNew, with step menuTargetListStep and draw menuTargetListDraw, kept in
 * menuTargetListPanel.
 */
void menuTargetListStep(MenuState *panel);
void menuTargetListDraw(MenuState *panel);


/*
 * menuTgtStatusNew: constructor of the target status
 * panel kept in menuTgtStatusPanel: empty step menuTgtStatusStep, draw menuTgtStatusDraw (this file),
 * origin (348, 314), state 0 and flag 0.
 */
void menuTgtStatusDraw(MenuState *panel);
void menuTgtStatusStep();

WepSpEntry *dataTid2WepSp(int tid)
{
    int index;

    for (index = 0; wepSpTbl[index].tid != 0; index++) {
        if (wepSpTbl[index].tid == tid) {
            return &wepSpTbl[index];
        }
    }
    return 0;
}

/*
 * menuWepSpPanelNew: allocates a menu panel with
 * objSlotAlloc, sets its step menuWepSpPanelStep and its draw menuWepSpPanelDraw,
 * origin (8, 312), state 0 and flag 0, and keeps it in menuWepSpPanel. Called from
 * the battle setup ov01StepNextEnemy.
 */
void menuWepSpPanelNew(void)
{
    MenuState *panel;

    panel = objSlotAlloc();
    if (panel != 0) {
        panel->update = menuWepSpPanelStep;
        panel->draw = menuWepSpPanelDraw;
        menuWepSpPanel = panel;
        objStdInit(&panel->slide);
        panel->slide.position.x = 8.0f;
        panel->slide.position.y = 312.0f;
        do {
        } while (0);
        panel->flag = 0;
        panel->slide.position.z = 0;
        do {
        } while (0);
        panel->state = MENU_PANEL_STATE_IDLE;
    }
}

/*
 * menuWepSpPanelStep: the panel's step. It consumes a
 * show (1) or hide (2) request left in state by menuWepSpPanelShow/Hide and
 * sets state back to 0.
 */
void menuWepSpPanelStep(MenuState *panel)
{
    switch (panel->state) {
    case MENU_PANEL_STATE_IDLE:
        break;
    case MENU_PANEL_STATE_SHOW_REQUEST:
        panel->state = MENU_PANEL_STATE_IDLE;
        return;
    case MENU_PANEL_STATE_HIDE_REQUEST:
        panel->state = MENU_PANEL_STATE_IDLE;
        break;
    }
}

/*
 * menuPulseAlphaSet: sets GS ALPHA_1 (MENU_GS_ALPHA_1)
 * with a fixed alpha that rises and falls with the scene frame counter
 * ov01SceneFrameCount: a triangle wave from 0x2A to 0xA9.
 */
void menuPulseAlphaSet(GrPacket *packet)
{
    int phase;

    phase = (ov01SceneFrameCount() * 2) & 0xFF;
    if (phase >= MENU_PULSE_PHASE_HALF) {
        phase = 0xFF - phase;
    }
    /* the fixed alpha sits in the high word of the ALPHA_1 value. */
    grGsRegSet(packet, MENU_GS_ALPHA_1, ((unsigned long long)(phase + MENU_PULSE_ALPHA_MIN) << 32) | MENU_PULSE_ALPHA_BLEND);
}

/*
 * menuWepSpPanelDraw: draws the panel while its flag
 * is set: a header, then one row per entry of the battle list from
 * dataSlotRowGet, each row's wep-special record from dataTid2WepSp, and a footer
 * at count * 21 below the rows. Calls menuRollPercentDraw for the units with keys 11 to 15.
 */
void menuWepSpPanelDraw(MenuState *panel)
{
    GrPacket packet;
    ColourPair rowColours = menuWepSpRowColors;
    struct BattleState *battleState;
    BattleUnit *currentUnit;
    struct DataSlotRow *slotRow;
    WepSpEntry *wepSpEntry;
    int x;
    int y;
    int rowY;
    int rowIndex;
    int colourIndex;
    int rowCount;
    int shown;
    int rowStride;

    shown = panel->flag;
    if (shown != 0) {
        battleState = manWorkGet();
        x = (int) panel->slide.position.x;
        y = (int) panel->slide.position.y;
        currentUnit = battleState->current;
        grGpInit(&packet);
        menuStepIconTexState(&packet);
        grOpenSpr(&packet);
        menuStatSprite2(&packet, 0, y - 10, 16, 10, 503, 214, 488, 223);
        menuStatSprite(&packet, 82, y - 10, 16, 10, 488, 214);
        grCloseSpr();
        menuRowCursorTexState(&packet);
        slotRow = dataSlotRowGet(currentUnit, menuWepSpPanelCursorGet());
        grOpenSpr(&packet);
        rowIndex = 0;
        if (slotRow->count > 0) {
            rowY = y;
            do {
                wepSpEntry = dataTid2WepSp(slotRow->slot[rowIndex]);
                menuStatSprite(&packet, 0, rowY + 10, 100, 8, 256, 214);
                colourIndex = dataSlotKeyTest(currentUnit, slotRow->slot[rowIndex]) == 0;
                menuStatSpriteRGB(&packet, x, rowY, 82, 17, wepSpEntry->srcX, wepSpEntry->srcY,
                                  &rowColours.colours[colourIndex]);
                rowY += MENU_WEPSP_ROW_STRIDE;
                rowIndex++;
            } while (rowIndex < slotRow->count);
        }
        grCloseSpr();
        menuPulseAlphaSet(&packet);
        grOpenSpr(&packet);
        rowCount = menuWepSpPanelRowCountGet();
        rowStride = MENU_WEPSP_ROW_STRIDE;
        menuStatSprite(&packet, 0, rowCount * rowStride + y, 123, 15, 256, 198);
        grCloseSpr();
        grPacketSend(&packet);
        if (currentUnit->stats->keyUnsigned - 11U < 5U) {
            menuRollPercentDraw(currentUnit);
        }
    }
}

/* menuWepSpPanelShow: requests the wep-special panel to show. */
void menuWepSpPanelShow(void)
{
    menuWepSpPanel->state = MENU_PANEL_STATE_SHOW_REQUEST;
    menuWepSpPanel->flag = 1;
}

/* menuWepSpPanelHide: requests the wep-special panel to hide. */
void menuWepSpPanelHide(void)
{
    menuWepSpPanel->state = MENU_PANEL_STATE_HIDE_REQUEST;
    menuWepSpPanel->flag = 0;
}

void menuTargetListNew(void)
{
    MenuState *panel;

    panel = objSlotAlloc();
    if (panel != 0) {
        panel->update = menuTargetListStep;
        panel->draw = menuTargetListDraw;
        menuTargetListPanel = panel;
        objStdInit(&panel->slide);
        panel->slide.position.x = 8.0f;
        panel->slide.position.y = 312.0f;
        do {
        } while (0);
        panel->flag = 0;
        panel->slide.position.z = 0;
        do {
        } while (0);
        panel->state = MENU_PANEL_STATE_IDLE;
    }
}

/* menuTargetListStep: consumes the show or hide request, as menuWepSpPanelStep. */
void menuTargetListStep(MenuState *panel)
{
    switch (panel->state) {
    case MENU_PANEL_STATE_IDLE:
        break;
    case MENU_PANEL_STATE_SHOW_REQUEST:
        panel->state = MENU_PANEL_STATE_IDLE;
        return;
    case MENU_PANEL_STATE_HIDE_REQUEST:
        panel->state = MENU_PANEL_STATE_IDLE;
        break;
    }
}

/*
 * menuTargetListDraw: while the flag is set, draws one
 * row per key of the list from battleMaskWordsUnmatched. Each key is looked up as a
 * battle entry (battleEntryFindByKey, else battleSlotGet) and its hp and ep are printed
 * with menuNumDrawTarget and menuNumDrawTargetEp; the footer sits below the rows at the cursor.
 */
void menuTargetListDraw(MenuState *panel)
{
    GrPacket packet;
    int keys[8];
    int *keyPtr;
    int remaining;
    BattleUnit targetObject;
    BattleUnit *currentUnit;
    StatBlock *targetStats;
    int x;
    int y;
    int rowY;
    int keyCount;
    int i;
    int cursor;
    int hp;
    short ep;

    if (panel->flag != 0) {
        x = (int) panel->slide.position.x;
        y = (int) panel->slide.position.y;
        currentUnit = manWorkGet()->current;
        keyCount = battleMaskWordsUnmatched(currentUnit, keys);
        memset(&targetObject, 0, sizeof(targetObject));
        grGpInit(&packet);
        if (keyCount > 0) {
            remaining = keyCount;
            rowY = y;
            keyPtr = keys;
            do {
                menuKeyDraw(&packet, x, rowY, *keyPtr, 1);
                menuNumTexState(&packet);
                grOpenSpr(&packet);
                menuStatSprite(&packet, x + 32, rowY, 18, 16, 107, 0);
                menuStatSprite(&packet, x + 112, rowY, 18, 16, 125, 0);
                grCloseSpr();
                targetStats = battleEntryFindByKey(*keyPtr);
                if (targetStats != 0) {
                    hp = targetStats->hp;
                    ep = targetStats->ep;
                } else {
                    targetStats = battleSlotGet(*keyPtr);
                    hp = targetStats->hp;
                    ep = targetStats->ep;
                }
                do {
                } while (0);
                targetObject.stats = targetStats;
                keyPtr++;
                remaining--;
                menuNumDrawTarget(&packet, x + 52, rowY, hp,
                                  (currentUnit->stats->keyUnsigned - 11U < 5U) ? 5 : 4, 0, &targetObject);
                menuNumDrawTargetEp(&packet, x + 132, rowY, ep, 2, 0, &targetObject);
                rowY += MENU_TARGET_ROW_STRIDE;
            } while (remaining != 0);
        }
        cursor = menuTargetListCursorGet();
        menuRowCursorTexState(&packet);
        menuPulseAlphaSet(&packet);
        grOpenSpr(&packet);
        menuStatSprite2(&packet, x - 2, y + cursor * MENU_TARGET_ROW_STRIDE - 2, 160, 32, 256, 198, 378, 212);
        grCloseSpr();
        grPacketSend(&packet);
    }
}

/* menuTargetListShow: requests the target list panel to show. */
void menuTargetListShow(void)
{
    menuTargetListPanel->state = MENU_PANEL_STATE_SHOW_REQUEST;
    menuTargetListPanel->flag = 1;
}

/* menuTargetListHide: requests the target list panel to hide. */
void menuTargetListHide(void)
{
    menuTargetListPanel->state = MENU_PANEL_STATE_HIDE_REQUEST;
    menuTargetListPanel->flag = 0;
}

void menuTgtStatusNew(void)
{
    MenuState *panel;

    panel = objSlotAlloc();
    if (panel != 0) {
        panel->update = menuTgtStatusStep;
        panel->draw = menuTgtStatusDraw;
        menuTgtStatusPanel = panel;
        objStdInit(&panel->slide);
        panel->slide.position.x = 376.0f;
        panel->slide.position.y = 314.0f;
        panel->slide.position.z = 0;
        panel->slide.position.x = 348.0f;
        panel->state = MENU_PANEL_STATE_IDLE;
        panel->flag = 0;
    }
}

/* Empty step of the target status panel; it has no body. Its global name menuTgtStatusStep is kept. */
void menuTgtStatusStep(void)
{
}

/* Prototypes of the target status draw and its helpers. */
char *strcat(char *, const char *);
void menuUnitStatusIconsDraw(BattleUnit *, GrPacket *, int, int);

/*
 * The 41-byte status text (strcat and xglFontPrint read it as a string): it is copied
 * as five quadwords and its terminating byte, so both views of its storage are used.
 */
union MenuStatusText {
    unsigned long long words[5];
    unsigned char text[41];
};
extern const union MenuStatusText menuTgtStatusTextTemplate;
extern union MenuStatusText menuTgtStatusTextBuffer;

/*
 * menuTgtStatusDraw: the draw of the target status panel
 * (menuTgtStatusPanel, set up by menuTgtStatusNew). While its flag is set it takes the
 * target from tgtUnitGet and draws the unit's values from its battle block (+0x144):
 * the value at +0x5C in five digits (sprites above 99999), the halfwords at
 * +0x92..+0x98, the flag 0x800 at +0x1C, and the status icons of
 * menuUnitStatusIconsDraw.
 */
void menuTgtStatusDraw(MenuState *panel) {
    /* 16-byte frame slot (two GrPacket words wide, as the int[4] it was); packet[0] is the packet. */
    GrPacket packet[2];
    BattleUnit *unit;
    int x;
    int y;
    char *nameText;
    int i;

    if (panel->flag == 0) {
        return;
    }
    unit = tgtUnitGet();
    if (unit == 0) {
        return;
    }
    if (unit->flags & 4) {
        return;
    }
    x = (int) panel->slide.position.x;
    y = (int) panel->slide.position.y;
    menuTgtStatusTextBuffer.words[0] = menuTgtStatusTextTemplate.words[0];
    menuTgtStatusTextBuffer.words[1] = menuTgtStatusTextTemplate.words[1];
    menuTgtStatusTextBuffer.words[2] = menuTgtStatusTextTemplate.words[2];
    menuTgtStatusTextBuffer.words[3] = menuTgtStatusTextTemplate.words[3];
    menuTgtStatusTextBuffer.words[4] = menuTgtStatusTextTemplate.words[4];
    menuTgtStatusTextBuffer.text[40] = menuTgtStatusTextTemplate.text[40];
    nameText = batUnitNameGet(unit->stats->key);
    strcat(menuTgtStatusTextBuffer.text, nameText);
    xglFontPrint(x, y, MENU_STATUS_TEXT_COLOUR, menuTgtStatusTextBuffer.text);
    grGpInit(packet);
    menuHighlightTexState(packet);
    grGsRegSet(packet, MENU_GS_ALPHA_1, MENU_TGT_STATUS_ALPHA);
    grOpenSpr(packet);
    menuStatSprite2(packet, x - 2, y - 2, 160, 54, 200, 120, 255, 174);
    grCloseSpr();
    y += 14;
    menuUnitSlotTexState(packet);
    grOpenSpr(packet);
    menuStatSprite(packet, x, y, 24, 12, 146, 92);
    grCloseSpr();
    if (unit->stats->hp > MENU_HP_DISPLAY_MAX) {
        grOpenSpr(packet);
        for (i = 0; i < 5; i++) {
            menuStatSprite(packet, x + 54 + i * 10, y - 2, 10, 14, 130, 50);
        }
        grCloseSpr();
    } else {
        menuNumDraw(packet, x + 54, y - 2, unit->stats->hp, 5, 2);
    }
    grOpenSpr(packet);
    menuStatSprite(packet, x, y + 12, 24, 12, 576, 156);
    grCloseSpr();
    grOpenSpr(packet);
    if (unit->stats->activeMask[0] != 0) {
        menuStatSprite(packet, x + 54, y + 12, 13, 12, 576, 169);
    }
    if (unit->stats->activeMask[1] != 0) {
        menuStatSprite(packet, x + 68, y + 12, 13, 12, 591, 169);
    }
    if (unit->stats->activeMask[2] != 0 || unit->stats->activeMask[3] != 0) {
        menuStatSprite(packet, x + 82, y + 12, 13, 12, 606, 169);
    }
    grCloseSpr();
    if (unit->stats->flags & 0x800) {
        grOpenSpr(packet);
        menuStatSprite(packet, x, y + 24, 18, 12, 194, 106);
        grCloseSpr();
        menuNumDraw(packet, x + 54, y + 24, unit->stats->rollPercent, 3, 2);
    } else {
        grOpenSpr(packet);
        menuStatSprite(packet, x, y + 24, 46, 12, 146, 106);
        grCloseSpr();
        if (calcSkillEquChkAll(16) && sdvSpecialBitGet(unit->stats->key)) {
            grOpenSpr(packet);
            menuUnitStatusIconsDraw(unit, packet, x + 54, y + 24);
            grCloseSpr();
        } else if (unit->stats->liveRows[0] != 0) {
            grOpenSpr(packet);
            menuStatSprite(packet, x + 54, y + 24, 10, 14, 130, 50);
            grCloseSpr();
        }
    }
    grPacketSend(packet);
}

/*
 * menuUnitStatusIconsDraw: draws one icon per byte of
 * the unit body's list at +0x20, up to four bytes, stopping at a zero byte. The
 * byte picks the sprite (1, 2 or 4 give the sprite columns 0xAE, 0xBE or 0xCE),
 * and each icon is 16 units to the right of the last.
 */
void menuUnitStatusIconsDraw(BattleUnit *unit, GrPacket *packet, int x, int y) {
    StatBlock *stats = unit->stats;
    int iconIndex;
    int iconSrcX;

    for (iconIndex = 0; stats->liveRows[iconIndex] != 0 && iconIndex < 4; iconIndex++) {
        switch (stats->liveRows[iconIndex]) {
        case 1:
            iconSrcX = 0xAE;
            break;
        case 2:
            iconSrcX = 0xBE;
            break;
        case 4:
            iconSrcX = 0xCE;
            break;
        default:
            return;
        }
        menuStatSprite(packet, x, y, 16, 12, iconSrcX, 92);
        x += 16;
    }
}

/* menuTgtStatusShow: sets the visible flag (+0x94) of the target status panel. */
void menuTgtStatusShow(void) {
    menuTgtStatusPanel->flag = 1;
}

/* menuTgtStatusHide: clears the visible flag (+0x94) of the target status panel. */
void menuTgtStatusHide(void) {
    menuTgtStatusPanel->flag = 0;
}

/*
 * menuRollPercentDraw: draws a fixed panel with the unit's roll percent as a
 * three-digit number, taken from the battle block at +0x144 of the unit (its halfword
 * at +0x80). Called by menuWepSpPanelDraw for units with keys 11 to 15.
 */
void menuRollPercentDraw(BattleUnit *unit) {
    /* 16-byte frame slot (two GrPacket words wide, as the int[4] it was); packet[0] is the packet. */
    GrPacket packet[2];

    menuTexFlush();
    grGpInit(packet);
    menuUnitSlotTexState(packet);
    grOpenSpr(packet);
    menuStatSprite(packet, 0x178, 0x154, 0x12, 0xC, 0xC2, 0x6A);
    grCloseSpr();
    menuNumDraw(packet, 0x1AE, 0x154, unit->stats->rollPercent, 3, 2);
    grPacketSend(packet);
}

/* The panels of this file, common globals: menuTgtStatusPanel is named by deco_flags.c too. */
MenuState *menuWepSpPanel;
MenuState *menuTargetListPanel;
MenuState *menuTgtStatusPanel;

/* Wep-special table: technique id, source x and y of its icon; a zero id ends it. */
WepSpEntry wepSpTbl[10] = {
    { 11, 256, 130 }, { 12, 256, 147 }, { 13, 256, 164 }, { 14, 256, 181 }, { 15, 338, 181 },
    { 16, 338, 130 }, { 17, 338, 147 }, { 18, 338, 164 }, { 19, 338, 147 }, { 0, 0, 0 }
};

/* The two 4-byte row colours of menuWepSpPanelDraw. */
const ColourPair menuWepSpRowColors = { { { 0x80, 0x80, 0x80, 0x80 }, { 0x20, 0x20, 0x20, 0x80 } } };

/* The status text template of menuTgtStatusDraw: 40 characters, then zero bytes up to 48. */
const union MenuStatusText menuTgtStatusTextTemplate = { .text = "$han1;$prop1;$zoom15,10;$colc0c0c0;$bg3;" };

/* Name strings stored in .rodata, one array each; no reader in this file. */
const char bgmFanfareName[16] = "fanfale01";
const char bgmLastBossName[16] = "bosslast";
const char bgmBoss02Name[8] = "boss02";
const char bgmBoss01Name[8] = "boss01";
const char bgmBattle02Name[16] = "battle02";
const char bgmBattle01Name[16] = "battle01";
const char configScriptPrefixName[8] = "cf\\sc";
