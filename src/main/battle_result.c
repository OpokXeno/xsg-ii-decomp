#include "common.h"
#include "main/xenoActor.h"
#include "main/battle_result.h"
#include "main/xgl_render.h"
#include "shared.h"
#include "main/font_obj.h"
#include "main/menu_draw.h"
#include "ov01/calc.h"
#include "ov01/data_box.h"
#include "main/game_camera.h"
#include "main/game_data.h"
#include "main/pt_member.h"
#include "ov01/scene.h"
#include "main/fade_ctrl.h"
#include "main/bg_layer.h"
#include "main/se_bank_load.h"
#include "main/res_get_path.h"
#include "main/xgl_cd.h"

/* Battle result screen: scene states (battleResultState), its frame timing and the display limits of the figures. */
enum BattleResultState {
    BATTLE_RESULT_STATE_COUNT_UP = 0,
    BATTLE_RESULT_STATE_TALLY = 1,
    BATTLE_RESULT_STATE_SUMMARY = 2,
    BATTLE_RESULT_STATE_ENTRY_LIST = 4
};
/* Frames of the count-up before the tally starts, and frames per second for the clear time. */
#define BATTLE_RESULT_COUNT_FRAMES 180
#define BATTLE_RESULT_FRAMES_PER_SECOND 30
/* Largest clear time shown (99:59:59 at 30 frames per second). */
#define BATTLE_RESULT_CLEAR_TIME_CAP 0xA4CB7F
/* Largest score (eight digits) and largest five-digit figure of the score parts. */
#define BATTLE_RESULT_SCORE_CAP 0x05F5E0FFULL
#define BATTLE_RESULT_PART_CAP 0x1869FULL
/* Menu text ids: the list heading and the unit that follows each count. */
#define BATTLE_RESULT_TXD_HEADING 0xE5
#define BATTLE_RESULT_TXD_COUNT_UNIT 0xE6

extern int battleResultScoreMax;
/* The rows calcResult (ov01 calc.c) fills, one per party unit. */
extern struct BattleResultRow *battleResultRows;
extern int (*battleResultStats)[4];
extern unsigned char battleResultScoreDashes[];
extern unsigned char battleResultSignedDashes[];
extern const unsigned char battleResultTextSetup[];
extern const unsigned char battleResultClearTimeLabel[];
extern const unsigned char battleResultTimeTemplate[];
extern unsigned char battleResultTitle[];
extern int battleResultState;
extern int battleResultStateTimer;
extern int battleResultMemberCount;
extern int battleResultEntryCount;
extern int battleResultClearFrames;
extern signed char battleResultCdServiceResult;
long long __udivdi3(long long a, long long b);
void battleResultDrawEntries(int x, int y);

/* Font colours of the highlight shadow and fill (packed float 1.0 with colour words). */
#define RESULT_SHADOW_COLOUR 0x3F80000080000000LL
#define RESULT_FILL_COLOUR 0x3F80000080407080LL
/* Result row: label, the score and time values, and the adjusted sub-row for entry index. */
void battleResultDrawRow(int x, int y, int index, int tally)
{
    struct BattleResultRow *row;
    PartyMember *record;
    PartyMemberStatus *label;
    PartyMemberLimits *valueRecord;
    StatBlock *stats;
    int wideOffset;
    int subFrameY;
    int cardX;
    int lowerRowY;
    int adjust;
    int scoreRowY;
    int markerY;
    int markerX;
    int markerFillX;
    unsigned long long total;
    unsigned long long rest;

    row = battleResultRows + index;
    record = ptFindRecord(row->key);
    if (record == 0) {
        return;
    }
    label = record->status;
    stats = calcTotalParaMenu(record->id);
    valueRecord = record->limits;
    if (row->key >= 11) {
        x -= 8;
        wideOffset = 17;
    } else {
        wideOffset = 0;
    }
    panelDrawNineSlice(menuPanelFrame, x, y, wideOffset + 374, 106);
    menuDrawSplitFrame(x + 4, y + 5, wideOffset + 366, 96, wideOffset + 176);
    cardX = x + wideOffset;
    menuDrawOutlineFrame(x + 13, y + 28, wideOffset + 173, 66);
    subFrameY = y + 53;
    menuDrawOutlineFrame(cardX + 191, subFrameY, 100, 41);
    adjust = tally != battleResultScoreMax ? row->levelGain : 0;
    panelDrawNineSlice(menuPanelFrame, x + 21, y + 35, wideOffset + 157, 39);
    menuDrawIndexedIcon(x + 27, y + 10, label->memberIndex);
    menuDrawCell(288, x + 68, y + 8, 17);
    textDrawShadow(x + 73, y + 11, label->text, 0xFE00ULL << 46);
    menuDrawLevelShadow(x + wideOffset + 140, y + 7, stats->expLevel - adjust);
    if (row->levelGain != 0) {
        if (adjust == 0) {
            menuDrawCell(396, x + wideOffset + 240, y + 3, 17);
        }
        adjust = 0;
        if (tally != battleResultScoreMax) {
            adjust = row->maxHpGain;
        }
        if (wideOffset != 0) {
            menuDrawHpWideShadow(x + 69, y + 36, stats->hp, stats->maxHp - adjust);
        } else {
            menuDrawHpShadow(x + 69, y + 36, stats->hp, stats->maxHp - adjust);
        }
        adjust = tally != battleResultScoreMax ? row->maxEpGain : 0;
        if (wideOffset != 0) {
            menuDrawEpWideShadow(x + 69, y + 52, stats->ep, stats->maxEp - adjust);
        } else {
            menuDrawEpShadow(x + 69, y + 52, stats->ep, stats->maxEp - adjust);
        }
    } else {
        if (wideOffset != 0) {
            menuDrawHpWideShadow(x + 69, y + 36, stats->hp, stats->maxHp);
        } else {
            menuDrawHpShadow(x + 69, y + 36, stats->hp, stats->maxHp);
        }
        if (wideOffset != 0) {
            menuDrawEpWideShadow(x + 69, y + 52, stats->ep, stats->maxEp);
        } else {
            menuDrawEpShadow(x + 69, y + 52, stats->ep, stats->maxEp);
        }
    }
    fontObjSetColor(RESULT_SHADOW_COLOUR);
    menuDrawStatIcon(5, x + 70, y + 74, 0xFE00ULL << 46);
    fontObjSetColor(0x3F80000080665B6EULL);
    menuDrawStatIcon(5, x + 69, y + 71, 0xFE00ULL << 46);
    fontObjResetColor();
    if (tally == battleResultScoreMax) {
        lowerRowY = y + 75;
        textDrawDecimalAltShadow(x + wideOffset + 117, lowerRowY, valueRecord->next_level_exp, 8, 0xFE00ULL << 46);
    } else {
        lowerRowY = y + 75;
        textDrawDecimalAltShadow(x + wideOffset + 117, lowerRowY, battleResultStats[0][index], 8, 0xFE00ULL << 46);
    }
    fontObjSetColor(RESULT_SHADOW_COLOUR);
    scoreRowY = y + 32;
    markerX = x + wideOffset + 199;
    menuDrawStatIcon(6, markerX, scoreRowY, 0xFE00ULL << 46);
    adjust = y + 57;
    menuDrawStatIcon(2, markerX, adjust, 0xFE00ULL << 46);
    menuDrawStatIcon(3, markerX, lowerRowY, 0xFE00ULL << 46);
    fontObjSetColor(0x3F80000080665B6EULL);
    markerFillX = markerX - 1;
    markerY = y + 29;
    menuDrawStatIcon(6, markerFillX, markerY, 0xFE00ULL << 46);
    fontObjSetColor(0x3F800000807F5747ULL);
    menuDrawStatIcon(2, markerFillX, y + 54, 0xFE00ULL << 46);
    menuDrawStatIcon(3, markerFillX, y + 72, 0xFE00ULL << 46);
    fontObjResetColor();
    total = row->gainFirst;
    total = __udivdi3(total * tally, battleResultScoreMax);
    total += battleResultStats[3][index];
    if (total > BATTLE_RESULT_SCORE_CAP) {
        total = BATTLE_RESULT_SCORE_CAP;
    }
    textDrawDecimalAltShadow(x + wideOffset + 226, scoreRowY, (int)total, 10, 0xFE00ULL << 46);
    if (row->key >= 11) {
        textDrawShadow(cardX + 238, adjust, battleResultScoreDashes, 0xFE00ULL << 46);
        textDrawShadow(cardX + 238, lowerRowY, battleResultScoreDashes, 0xFE00ULL << 46);
        textDrawShadow(cardX + 302, adjust, battleResultSignedDashes, 0xFE00ULL << 46);
        textDrawShadow(cardX + 302, lowerRowY, battleResultSignedDashes, 0xFE00ULL << 46);
    } else {
        unsigned long long part;

        part = row->gainThird;
        part = __udivdi3(part * tally, battleResultScoreMax) + battleResultStats[1][index];
        if (part > BATTLE_RESULT_PART_CAP) {
            part = BATTLE_RESULT_PART_CAP;
        }
        textDrawDecimalShadow(cardX + 238, adjust, (int)part, 5, 0xFE00ULL << 46);
        part = row->gainThird;
        part = part * (battleResultScoreMax - tally);
        part = __udivdi3(part, battleResultScoreMax);
        textDrawSignedDecimalShadow(cardX + 300, adjust, (int)part, 5, 0xFE00ULL << 46);
        part = row->gainSecond;
        part = __udivdi3(part * tally, battleResultScoreMax) + battleResultStats[2][index];
        if (part > BATTLE_RESULT_PART_CAP) {
            part = BATTLE_RESULT_PART_CAP;
        }
        textDrawDecimalShadow(cardX + 238, lowerRowY, (int)part, 5, 0xFE00ULL << 46);
        part = row->gainSecond;
        part = part * (battleResultScoreMax - tally);
        part = __udivdi3(part, battleResultScoreMax);
        textDrawSignedDecimalShadow(cardX + 300, lowerRowY, (int)part, 5, 0xFE00ULL << 46);
    }
    rest = row->gainFirst;
    rest = rest * (battleResultScoreMax - tally);
    rest = __udivdi3(rest, battleResultScoreMax);
    textDrawSignedDecimalShadow(x + wideOffset + 300, scoreRowY, (int)rest, 5, 0xFE00ULL << 46);
    menuDrawShortOutlineFrame(x + wideOffset + 191, markerY, 170, 21);
    menuDrawShortOutlineFrame(x + wideOffset + 294, y + 53, 67, 41);
}

/* One 16-byte entry: a (category, id) pair, its repeat count and the highlight flag. */
struct BattleResultEntry {
    int category;
    int id;
    int count;
    int highlight;
};

/* Entry records built by battleResultBuildEntries and their count. */
extern struct BattleResultEntry *battleResultEntries;

/* Pixels between entry rows in the list. */
#define RESULT_ROW_HEIGHT 0x14

extern const unsigned char battleResultHeadingStyleText[];
extern const char battleResultCountFormat[];
int sprintf(unsigned char *dst, const char *format, int count, void *unit);

/* Draws the entry list: each 16-byte record dispatches by type to a draw helper. */
void battleResultDrawEntries(int x, int y) {
    unsigned char countText[16];
    int i;
    char *itemText;
    int highlighted;

    i = 0;
    highlighted = 0;
    panelDrawNineSlice(menuPanelFrame, x, y, 0xF6, battleResultEntryCount * RESULT_ROW_HEIGHT + 0x36);
    menuDrawShortOutlineFrame(x + 4, y + 5, 0xEE, 0x1B);
    menuDrawShortOutlineFrame(x + 4, y + 0x22, 0xEE, battleResultEntryCount * RESULT_ROW_HEIGHT + 0xF);
    textDrawSegment(x + 0x40, y + 8, menuMainTxdEntry(BATTLE_RESULT_TXD_HEADING), 0);
    textDrawSegment(x + 0x40, y + 8, battleResultHeadingStyleText, 0);

    for (; i < battleResultEntryCount; i++) {
        itemText = 0;
        switch (battleResultEntries[i].category) {
        case 1:
            itemText = hitActionTextGet(battleResultEntries[i].id, 0);
            break;
        case 2:
            itemText = itemTextGet(battleResultEntries[i].id, 0);
            break;
        case 3:
            itemText = dataBoxCategory3TextGet(battleResultEntries[i].id, 0);
            break;
        case 4:
            itemText = dataBoxCategory4TextGet(battleResultEntries[i].id, 0);
            break;
        }
        if (itemText != 0) {
            textDrawSegment(x + 0x14, y + i * RESULT_ROW_HEIGHT + 0x2A, itemText, 0);
        }
        sprintf(countText, battleResultCountFormat, battleResultEntries[i].count, menuMainTxdEntry(BATTLE_RESULT_TXD_COUNT_UNIT));
        textDrawSegment(x + 0xB6, y + i * RESULT_ROW_HEIGHT + 0x2A, countText, 0);
        if (battleResultEntries[i].highlight != 0) {
            highlighted = 1;
            fontObjSetColor(RESULT_SHADOW_COLOUR);
            menuDrawCell(0xC8, x + 0xE7, y + i * RESULT_ROW_HEIGHT + 0x2D, 4);
            fontObjSetColor(RESULT_FILL_COLOUR);
            menuDrawCell(0xC8, x + 0xE6, y + i * RESULT_ROW_HEIGHT + 0x2A, 4);
            fontObjResetColor();
        }
    }
    if (highlighted != 0) {
        x -= 0x20;
        panelDrawNineSlice(menuPanelFrame, x, y + battleResultEntryCount * RESULT_ROW_HEIGHT + 0x40, 0x136, 0x25);
        menuDrawShortOutlineFrame(x + 4, y + battleResultEntryCount * RESULT_ROW_HEIGHT + 0x45, 0x12E, 0x1B);
        fontObjSetColor(RESULT_SHADOW_COLOUR);
        menuDrawCell(0xC8, x + 0xD, y + battleResultEntryCount * RESULT_ROW_HEIGHT + 0x4B, 4);
        fontObjSetColor(RESULT_FILL_COLOUR);
        menuDrawCell(0xC8, x + 0xC, y + battleResultEntryCount * RESULT_ROW_HEIGHT + 0x48, 4);
        fontObjResetColor();
        textDrawSegment(x + 0x18, y + battleResultEntryCount * RESULT_ROW_HEIGHT + 0x48, menuMainTxdEntry(0xE7), 0);
    }
}

/* Result screen: RESULT and ClearTime labels, clear time split into minutes and seconds. */
void battleResultDrawScreen(void)
{
    int clearFrames;
    int clearSeconds;
    int minutes;
    int state;
    int rows;
    int emptyRows;
    int rowX;
    int rowY;
    int i;
    int tally;

    fontObjResetAndBuild();
    textDrawSegment(0, 0, battleResultTextSetup, 0);
    panelDrawNineSlice(menuPanelFrame, 4, 8, 84, 37);
    menuDrawShortOutlineFrame(8, 13, 76, 27);
    panelDrawNineSlice(menuPanelFrame, 155, 20, 201, 37);
    menuDrawShortOutlineFrame(159, 25, 193, 27);
    fontObjSetColor(0x3F80000080808080ULL);
    textDrawShadow(18, 19, battleResultTitle, 0xFE00ULL << 46);
    textDrawShadow(167, 31, battleResultClearTimeLabel, 0xFE00ULL << 46);
    textDrawShadow(247, 31, battleResultTimeTemplate, 0xFE00ULL << 46);
    clearFrames = battleResultClearFrames;
    if (clearFrames > BATTLE_RESULT_CLEAR_TIME_CAP) {
        clearFrames = BATTLE_RESULT_CLEAR_TIME_CAP;
    }
    clearSeconds = clearFrames / BATTLE_RESULT_FRAMES_PER_SECOND;
    textDrawDecimalShadowZero(325, 31, clearSeconds % 60, 2, 0);
    minutes = clearSeconds / 60;
    textDrawDecimalShadowZero(293, 31, minutes % 60, 2, 0);
    textDrawDecimalShadow(261, 31, minutes / 60, 2, 0);
    fontObjResetColor();

    state = battleResultState;
    if (state >= BATTLE_RESULT_STATE_ENTRY_LIST) {
        if (battleResultCdServiceResult == 0) {
            menuDrawKindIcon(414, 410, 3);
            textDrawSegment(434, 410, menuMainTxdEntry(232), 0);
        }
        battleResultDrawEntries(133, 140);
        return;
    }

    rows = 3;
    if (battleResultMemberCount < 4) {
        rows = battleResultMemberCount;
    }
    tally = state != 0 ? battleResultStateTimer : 0;
    if (state == BATTLE_RESULT_STATE_SUMMARY) {
        if (battleResultEntryCount == 0) {
            if (battleResultCdServiceResult == 0) {
                menuDrawKindIcon(414, 410, 3);
                textDrawSegment(434, 410, menuMainTxdEntry(232), 0);
            }
        } else {
            menuDrawKindIcon(414, 410, 3);
            textDrawSegment(434, 410, menuMainTxdEntry(233), 0);
        }
    }
    i = 0;
    if (rows > 0) {
        emptyRows = 3 - rows;
        rowX = 11 * emptyRows + 47;
        rowY = 48 * emptyRows + 64;
        for (; i < rows; i++) {
            battleResultDrawRow(rowX, rowY, i, tally);
            rowX += 22;
            rowY += 111;
        }
    }
}

/* Tail call to the result screen (battleResultDrawScreen). */
void battleResultDrawCallback(void) {
    battleResultDrawScreen();
}

/* Builds the 16-byte entry records at battleResultEntries from the table at a0 (count at +0x4C). */
void battleResultBuildEntries(struct BattleResult *result) {
    int entryCount = 0;
    int i;
    int j;

    for (i = 0; i < result->count; i++) {
        int found = 0;

        for (j = 0; j < entryCount; j++) {
            if (battleResultEntries[j].category == result->categories[i] && battleResultEntries[j].id == result->ids[i]) {
                battleResultEntries[j].count++;
                found = 1;
                break;
            }
        }
        if (!found) {
            battleResultEntries[entryCount].category = result->categories[i];
            battleResultEntries[entryCount].id = result->ids[i];
            battleResultEntries[entryCount].count = 1;
            entryCount++;
        }
    }
    battleResultEntryCount = entryCount;
    for (i = 0; i < battleResultEntryCount; i++) {
        if (battleResultEntries[i].count + dataBoxChk(battleResultEntries[i].category, battleResultEntries[i].id) >= 100) {
            battleResultEntries[i].highlight = 1;
        }
    }
}

#include "main/pad.h"
#include "main/sound_driver.h"
#include "main/xgl_cd_arena.h"
#include "main/xgl_tick.h"
#include "main/xgl_thread.h"
#include "main/xgl_pad.h"

/* Views of the structures the scene loop reads; each keeps only the members it uses. */
void *memset(void *dest, int value, unsigned int count);

/* Scene loop: clears the environment, builds the entries, runs frames until done. */
void battleResultScene(int mode)
{
    BattleUnit **snapshots;
    int slotCount;
    int i;
    BattleUnit *slot;
    PartyMember *member;
    struct BattleResultRow *rec;
    int bonusTotal;
    int scoreTotal;
    int exitRequested;
    PadRecord *pad;
    int stateTimer;

    i = 0;
    exitRequested = 0;
    xglCdArenaOpen(0, 0);
    ClearEnv.value.color_r = 0;
    ClearEnv.value.color_g = 0;
    ClearEnv.value.color_b = 0;
    ClearEnv.value.color_a = 0;
    xglRenderClearFrame();
    xglSleep();
    battleResultClearFrames = ov01SceneFrameCount();

    battleResultEntries = xglCdSectorAlloc(128);
    memset(battleResultEntries, 0, 128);
    battleResultRows = xglCdSectorAlloc(280);
    memset(battleResultRows, 0, 280);
    battleResultStats = xglCdSectorAlloc(64);
    memset(battleResultStats, 0, 64);

    battleResultMemberCount = ptGroupCount();
    slotCount = partySnapshotGet(&snapshots);
    if (slotCount < battleResultMemberCount) {
        battleResultMemberCount = slotCount;
    }

    if (battleResultMemberCount > 0) {
        i = 0;
        do {
            slot = snapshots[i];
            if (slot != 0) {
                member = ptFindRecord(slot->stats->key);
                if (member != 0) {
                    int (*table)[4] = battleResultStats;
                    table[0][i] = member->limits->next_level_exp;
                    table[1][i] = member->limits->classPoints;
                    table[2][i] = member->limits->skillPoints;
                    table[3][i] = member->limits->total_exp;
                }
            }
            i++;
        } while (i < battleResultMemberCount);
    }

    battleResultBuildEntries(monsGainPtrGet());
    calcResult(battleResultRows);

    if (battleResultMemberCount != 0) {
        bonusTotal = 0;
        battleResultScoreMax = 0;
        scoreTotal = 0;
        if (battleResultMemberCount > 0) {
            rec = battleResultRows;
            i = battleResultMemberCount;
            do {
                bonusTotal += rec->levelGain;
                if (battleResultScoreMax < rec->gainFirst) {
                    battleResultScoreMax = rec->gainFirst;
                }
                if (battleResultScoreMax < rec->gainSecond) {
                    battleResultScoreMax = rec->gainSecond;
                }
                if (battleResultScoreMax < rec->gainThird) {
                    battleResultScoreMax = rec->gainThird;
                }
                scoreTotal += rec->gainFirst + rec->gainSecond + rec->gainThird;
                rec++;
                i--;
            } while (i != 0);
        }

        i = 0;
        fontObjLoadTexture(0);
        fontObjLoadTexture(3);
        bgLayerLoadAll();
        xglCdWaitIdle();
        bgLayerCameraSetup();
        GameResourceWorkReload();
        fadeStartIn(16);
        xglPadSetRepeat(0, PAD_CIRCLE, 8, 1);
        pxyTickScaleDoubleOverride(1);
        battleResultState = BATTLE_RESULT_STATE_COUNT_UP;
        battleResultStateTimer = 0;
        if (battleResultScoreMax <= 0) {
            battleResultScoreMax = 1;
        }

        pad = &PadData[0];
        while (1) {
            battleResultCdServiceResult = (signed char) xglCdServiceIfPending();
            if (((pad->directionPressed & PAD_START) != 0
                    && (pad->held & PAD_SOFT_RESET_HELD) == PAD_SOFT_RESET_HELD)
                || (gameWork.control & CONTROL_SOFT_RESET) != 0) {
                gameWork.control |= CONTROL_SOFT_RESET;
                soundSlotsRestart();
                soundSlotReopen(0);
                break;
            }
            if (exitRequested != 0) {
                soundSlotSetVolume(0, 0, 8);
                seVoiceStartDefault(0x01000002);
                break;
            }

            switch (battleResultState) {
            case BATTLE_RESULT_STATE_COUNT_UP:
                stateTimer = battleResultStateTimer + 1;
                battleResultStateTimer = stateTimer;
                if (stateTimer >= BATTLE_RESULT_COUNT_FRAMES || (pad->released & PAD_CIRCLE) != 0
                    || (pad->heldAndPressed & 0x2000200000ULL) == 0x2000000000ULL) {
                    battleResultState = BATTLE_RESULT_STATE_TALLY;
                    battleResultStateTimer = 0;
                    if (scoreTotal != 0) {
                        seVoiceStartDefault(0x0100000A);
                    }
                }
                break;
            case BATTLE_RESULT_STATE_TALLY:
                battleResultStateTimer += 1;
                if ((pad->held & PAD_CIRCLE) != 0) {
                    battleResultStateTimer = battleResultStateTimer + (battleResultScoreMax >> 5) + 1;
                }
                if (battleResultScoreMax < battleResultStateTimer) {
                    battleResultStateTimer = battleResultScoreMax;
                    battleResultState = BATTLE_RESULT_STATE_SUMMARY;
                }
                if (scoreTotal != 0 && (xglTickGet() & 3) == 0) {
                    seVoiceStartDefault(0x0100000A);
                }
                break;
            case BATTLE_RESULT_STATE_SUMMARY:
                if ((pad->directionPressed & PAD_CIRCLE) != 0 || i >= 30) {
                    if (battleResultEntryCount == 0) {
                        exitRequested = 1;
                    } else {
                        battleResultState = BATTLE_RESULT_STATE_ENTRY_LIST;
                        battleResultStateTimer = 0;
                        seVoiceStartDefault(0x01000002);
                    }
                } else {
                    i = (pad->held & PAD_CIRCLE) != 0 ? i + 1 : 0;
                }
                if (bonusTotal != 0) {
                    bonusTotal = 0;
                    seVoiceStartDefault(0x01000009);
                    seVoiceStartDefault(0x01000009);
                    seVoiceStartDefault(0x01000009);
                }
                break;
            case BATTLE_RESULT_STATE_ENTRY_LIST:
                if ((pad->pressed & PAD_CIRCLE) != 0) {
                    exitRequested = 1;
                } else if ((pad->repeated & PAD_CIRCLE) != 0) {
                    battleResultStateTimer = battleResultStateTimer + 1;
                    exitRequested = 0;
                    if (battleResultStateTimer >= 16) {
                        exitRequested = 1;
                    }
                }
                break;
            }

            XenoActorSetFlagAll(2);
            bgLayerClearAllFlags();
            xglCallbackQueueAdd(battleResultDrawCallback);
            XenoActorDoit();
            XenoActorDisp();
            xglTickAdvance();
            xglSleep();
        }

        xglPadSetRepeat(0, PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT | PAD_L1 | PAD_R1, 8, 1);
        if ((gameWork.control & CONTROL_SOFT_RESET) == 0) {
            fadeStartOut(15);
            for (i = 0; i < 16; i++) {
                XenoActorSetFlagAll(2);
                bgLayerClearAllFlags();
                xglCallbackQueueAdd(battleResultDrawCallback);
                XenoActorDoit();
                XenoActorDisp();
                xglTickAdvance();
                xglSleep();
            }
        }
        calcDeadReset();
        bgLayerResetState();
        bgLayerRelease();
        xglCdArenaClose();
        pxyTickScaleDoubleOverride(0);
        if ((gameWork.control & CONTROL_SOFT_RESET) == 0) {
            GameResArenaRelease();
            fadeReset();
            fadeStartIn(30);
        }
    }
}

const unsigned char battleResultHeadingStyleText[16] = "$han1;$prop1;";
const unsigned char battleResultTextSetup[64] = "$init;$bg2;$shadow+1,+3,000000;$zoom13;$han1;$prop1;$colc0c0c0;";
const unsigned char battleResultClearTimeLabel[16] = "ClearTime";
const unsigned char battleResultTimeTemplate[16] = "/      :      :";
const char explanationLabelText[16] = "EXPLANATION";
int battleResultState = 0;
int battleResultStateTimer = 0;
int battleResultScoreMax = 0;
int battleResultMemberCount = 0;
struct BattleResultRow *battleResultRows = 0;
struct BattleResultEntry *battleResultEntries = 0;
int battleResultEntryCount = 0;
int (*battleResultStats)[4] = 0;
int battleResultClearFrames = 0;
signed char battleResultCdServiceResult = 0;
unsigned char battleResultScoreDashes[8] = "-----";
unsigned char battleResultSignedDashes[8] = "------";
const char battleResultCountFormat[8] = "%d%s";
unsigned char battleResultTitle[8] = "RESULT";
char statusLabelText[8] = "STATUS";
char emptyLabelText[8] = { 0 };
char wEtherLabelText[8] = "W ETHER";
char specialLabelText[8] = "SPECIAL";
char doubleLabelText[8] = "DOUBLE";
