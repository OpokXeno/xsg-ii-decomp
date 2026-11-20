#include "common.h"
#include "main/save_screen.h"
#include "shared.h"
#include "main/pad.h"
#include "main/save_party_init.h"
#include "main/sound_driver.h"
#include "main/game_camera.h"
#include "main/txd_table_entry.h"
#include "main/msg_state.h"
#include "main/xgl_cd.h"
#include "main/xgl_flags.h"
#include "main/game_data.h"
#include "main/pt_member.h"
#include "main/runtime_prog_effect.h"
#include "main/game_over_065.h"
#include "main/menu_selection.h"



void saveDataSetDefaults(void) {
    SaveData.soundMode = 1;
    SaveData.caption_all = 2;
    SaveData.cursor_memory = 0;
    SaveData.cursor_memory_alt = 0;
    SaveData.actuatorMode = 0;
    SaveData.dolby_digital = 0;
    tyaDisplaySetting(TYA_DISPLAY_RESET);
}

/* signed char: ptReset tests it with lb. */
extern signed char menuLoadPending;

/* Sets the load-pending flag that ptReset reads. */
void menuRequestLoad(void) {
    menuLoadPending = 1;
}

extern char menuMainTxdPath[];
extern char menuTutorialTxdPath[];
extern int menuMainTxdBuffer[];
extern int menuTutorialTxdBuffer[];

/* Reads yosikawa\MainMenu.txd and yosikawa\Tutorial.txd from the disc into their buffers, then polls
 * xglCdServiceIfPending until it returns 0. */
void menuLoadTxds(void) {
    xglCdReadFile(menuMainTxdPath, menuMainTxdBuffer, 1, CD_NOTIFY_CURRENT);
    xglCdReadFile(menuTutorialTxdPath, menuTutorialTxdBuffer, 1, CD_NOTIFY_CURRENT);
    do {
    } while (xglCdServiceIfPending() != 0);
}

static PartyMemberStatus ptMemberStatuses[10];
extern PartyMemberLimits slotRecordTable[];

static PartyMember ptMemberRecords[10];


/* gameWork.control bit set while ptState.progress is attached to a uwamono (uwamono_objx.c's script
 * native reads it back as a 0/1 value). */
#define CONTROL_PARTY_ATTACHED 0x800

extern int partyAttachMapIdTable[8];

/* Resets the party. On a pending load it first calls selectionFlagTableInit and
 * configScreenStateReset, writes the default settings (saveDataSetDefaults), applies them to the
 * pads, soundStereoSet and the display offset as padApplySaveSettings does, and reads the menu
 * text tables (menuLoadTxds). Then it rebuilds the ten member records, clears the party state,
 * stamps the play time, adds the default members, makes slot 0 current and clears the
 * selection block and the pending flag. */
void ptReset(void) {
    XglClock now;
    int i;
    int j;
    int k;

    if (menuLoadPending) {
        selectionFlagTableInit();
        configScreenStateReset();
        saveDataSetDefaults();
        PadData[0].actuatorMode = PadData[1].actuatorMode = SaveData.actuatorMode;
        soundStereoSet(SaveData.soundMode != 0);
        tyaSetDisplayOffset(SaveData.displayOffsetX, SaveData.displayOffsetY);
        menuLoadTxds();
    }
    ptProgressRecord.mapId = -1;
    ptProgressRecord.memberMask = 0;
    ptProgressRecord.mapFlag = 0;
    for (i = 0; i < 10; i++) {
        ptMemberStatuses[i].text = txdTableEntry(menuMainTxdBuffer, i);
        ptMemberStatuses[i].textAlt = txdTableEntry(menuMainTxdBuffer, i + 17);
        ptMemberStatuses[i].memberIndex = i;
        for (j = 0; j < 4; j++) {
            ptMemberTakenTable[i].header[j] = 0;
            for (k = 7; k >= 0; k--) {
                ptMemberTakenTable[i].cells[j][k] = 0;
            }
            ptMemberTakenTable[i].entries[j] = 0;
        }
        ptMemberTakenTable[i].openGroup = 0;
        ptMemberTakenTable[i].takenCount = 0;
        ptMemberTakenTable[i].entry_count = 0;
        /* Members 0-6 are ids 1-7 on slot records 0-6, members 7-9 ids 11-13 on records 10-12. */
        if (i >= 7 && i < 10) {
            ptMemberRecords[i].id = i + 4;
            ptMemberRecords[i].limits = &slotRecordTable[i + 3];
        } else {
            ptMemberRecords[i].limits = &slotRecordTable[i];
            ptMemberRecords[i].id = i + 1;
        }
        ptMemberRecords[i].status = &ptMemberStatuses[i];
        ptMemberRecords[i].buffer = &ptMemberTakenTable[i];
        ptState.slots[i] = 0;
        ptState.roster[i] = 0;
    }
    ptState.progress = &ptProgressRecord;
    ptState.splitMode = 0;
    ptState.frontIndex = 0;
    ptState.backIndex = 0;
    ptState.counts[0] = 0;
    ptState.counts[1] = 0;
    ptState.counts[2] = 0;
    xglClockRead(&now);
    /* The same record as ptProgressRecord, reloaded through the pointer just stored: written as
     * ptProgressRecord.playTime the store goes to the symbol address and the bytes change. */
    ptState.progress->playTime = xglClockDayTime2UInt(&now);
    ptResetStatusBlock();
    ptAddMember(1);
    ptAddMember(4);
    ptAddMember(5);
    ptAddMember(2);
    ptAddMember(3);
    ptAddMember(6);
    ptAddMember(7);
    ptAddMember(11);
    ptAddMember(12);
    ptAddMember(13);
    ptSetCurrentSlot(0);
    ptRefresh();
    selectionBlockClear();
    menuLoadPending = 0;
}

PartyMember *ptMemberRecordAt(int index) {
    return &ptMemberRecords[index];
}

/* Returns the first of the 10 slots whose id matches, or NULL. */
PartyMember *ptMemberRecordFindById(int id) {
    PartyMember *found = 0;
    int entry_index;

    for (entry_index = 0; entry_index < 10; entry_index++) {
        if (ptMemberRecords[entry_index].id == id) {
            found = &ptMemberRecords[entry_index];
            break;
        }
    }
    return found;
}

/* Attaches ptState.progress to the current uwamono: saves the current map, the actor's position
 * and the split mode, enters or leaves split mode as runSplit asks and starts the transition to
 * the map of kind (partyAttachMapIdTable). */
void uwamonoAttachCurrent(unsigned int kind, int runSplit) {
    PartyProgress *progress = ptState.progress;

    if (progress->mapId == -1 && uwamonoCurrentIndex != -1 && kind < 8) {
        progress->mapId = gameWork.mapId;
        /* the 16-byte position, copied as the two 64-bit values saveWriteImage saves */
        __builtin_memcpy(progress->position, &uwamonoRecords[uwamonoCurrentIndex].body.actor->position, sizeof(progress->position));
        progress->savedSplitMode = ptState.splitMode;
        if (runSplit && ptState.splitMode == 0) {
            ptEnterSplit();
        } else if (!runSplit && ptState.splitMode != 0) {
            ptLeaveSplit();
        }
        if (!runSplit && kind == 7) {
            xglFlagsSet1(0x2F0, 1);
        }
        gameWork.control |= CONTROL_PARTY_ATTACHED;
        uwamonoSetTransition(0, partyAttachMapIdTable[kind], kind == 6 ? 4 : 201, 0);
        msgResetAll();
    }
}

/* Detaches ptState.progress: restores the split mode saved on attach, clears flag 0x2F0 and
 * CONTROL_PARTY_ATTACHED, starts the transition back to the saved map and marks it detached. */
void uwamonoDetachCurrent(void) {
    if (ptState.progress->mapId != -1) {
        if (ptState.progress->savedSplitMode != 0) {
            if (ptState.splitMode == 0) {
                ptEnterSplit();
            }
        } else if (ptState.splitMode != 0) {
            ptLeaveSplit();
        }
        xglFlagsSet1(0x2F0, 0);
        gameWork.control &= ~CONTROL_PARTY_ATTACHED;
        uwamonoSetTransition(0, ptState.progress->mapId, -2, 0);
        msgResetAll();
        ptState.progress->mapId = -1;
    }
}

unsigned char backMemberCounterpartIdTable[3] = { 0x02, 0x06, 0x07 };

signed char menuLoadPending = 1;

unsigned char selectionWindowCount = 0;

unsigned char selectionPlainCount = 0;

char menuMainTxdPath[24] = "yosikawa\\MainMenu.txd";

char menuTutorialTxdPath[24] = "yosikawa\\Tutorial.txd";

int partyAttachMapIdTable[8] = {
    1010, 2110, 2210, 3120, 3410, 4310, 4610, 5010
};

struct LookupRecord specialAttackRecordTable[88] = {
    { 0, { 2, 2, 0 }, 1, 1, 1, 1, 0, 0, 0, 0x44 },
    { 0, { 2, 1, 0 }, 1, 2, 1, 1, 0, 0, 0, 0x44 },
    { 0, { 1, 1, 0 }, 1, 3, 1, 1, 0, 0, 0, 0x44 },
    { 0, { 1, 2, 0 }, 1, 4, 1, 1, 0, 0, 0, 0x44 },
    { 0, { 3, 0, 0 }, 1, 10, 1, 1, 0, 2, 0, 0x44 },
    { 0, { 3, 0, 0 }, 1, 9, 1, 1, 2, 0, 0, 0x44 },
    { 0, { 3, 3, 0 }, 1, 12, 1, 1, 2, 0, 0, 0x44 },
    { 0, { 2, 2, 0 }, 1, 1, 1, 1, 0, 0, 0, 0x4 },
    { 0, { 2, 1, 0 }, 1, 2, 1, 1, 0, 0, 0, 0x4 },
    { 0, { 1, 1, 0 }, 1, 3, 1, 1, 0, 0, 0, 0x4 },
    { 0, { 1, 2, 0 }, 1, 4, 1, 1, 0, 0, 0, 0x4 },
    { 0, { 3, 0, 0 }, 1, 10, 1, 1, 0, 1, 0, 0x4 },
    { 0, { 3, 0, 0 }, 1, 7, 1, 1, 0, 0, 0, 0x1 },
    { 0, { 3, 3, 0 }, 1, 8, 1, 1, 3, 0, 0, 0x1 },
    { 0, { 3, 0, 0 }, 1, 11, 2, 2, 0, 0, 0, 0x20 },
    { 0, { 3, 3, 0 }, 1, 11, 2, 2, 0, 0, 0, 0x80 },
    { 0, { 3, 3, 3 }, 1, 11, 2, 2, 0, 0, 0, 0x20 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 2, 2, 0 }, 1, 1, 1, 1, 0, 0, 0, 0x1 },
    { 0, { 2, 1, 0 }, 1, 2, 1, 1, 0, 0, 0, 0x1 },
    { 0, { 1, 1, 0 }, 1, 3, 1, 1, 0, 0, 0, 0x1 },
    { 0, { 1, 2, 0 }, 1, 4, 1, 1, 0, 0, 0, 0x1 },
    { 0, { 3, 0, 0 }, 1, 10, 1, 1, 0, 1, 0, 0x4 },
    { 0, { 3, 0, 0 }, 1, 7, 1, 1, 0, 0, 0, 0x1 },
    { 0, { 3, 3, 0 }, 1, 8, 1, 1, 3, 0, 0, 0x1 },
    { 0, { 2, 2, 0 }, 1, 1, 1, 1, 0, 0, 0, 0xC },
    { 0, { 2, 1, 0 }, 1, 2, 1, 1, 0, 0, 0, 0xC },
    { 0, { 1, 1, 0 }, 1, 3, 1, 1, 0, 0, 0, 0xC },
    { 0, { 1, 2, 0 }, 1, 4, 1, 1, 0, 0, 0, 0xC },
    { 0, { 3, 0, 0 }, 1, 10, 1, 1, 0, 2, 0, 0xC },
    { 0, { 3, 0, 0 }, 1, 9, 1, 1, 2, 0, 0, 0x9 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 3, 0, 0 }, 1, 11, 2, 2, 0, 0, 0, 0x2 },
    { 0, { 3, 3, 0 }, 1, 11, 2, 2, 0, 0, 0, 0x2 },
    { 0, { 3, 3, 3 }, 1, 11, 2, 2, 0, 0, 0, 0x2 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0x0 },
    { 0, { 2, 2, 0 }, 1, 1, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 2, 1, 0 }, 1, 2, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 1, 1, 0 }, 1, 3, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 1, 2, 0 }, 1, 4, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 3, 0, 0 }, 1, 10, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 3, 0, 0 }, 1, 7, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 3, 3, 0 }, 1, 8, 1, 2, 3, 0, 0, 0x2 },
    { 0, { 2, 0, 0 }, 1, 0, 1, 1, 0, 0, 0, 0x4 },
    { 0, { 1, 0, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x80 },
    { 0, { 3, 0, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x80 },
    { 0, { 1, 29, 0 }, 1, 0, 1, 2, 0, 0, 0, 0x4 },
    { 0, { 2, 4, 0 }, 1, 0, 1, 2, 0, 0, 0, 0x24 },
    { 0, { 2, 8, 0 }, 1, 0, 1, 1, 0, 0, 0, 0x1 },
    { 0, { 2, 1, 0 }, 1, 0, 1, 1, 0, 0, 0, 0x44 },
    { 0, { 2, 16, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x8 },
    { 0, { 3, 4, 1 }, 2, 0, 2, 2, 0, 0, 0, 0x80 },
    { 0, { 3, 8, 1 }, 1, 0, 1, 2, 0, 0, 0, 0x41 },
    { 0, { 2, 0, 0 }, 1, 0, 1, 1, 0, 0, 0, 0x4 },
    { 0, { 1, 0, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x2 },
    { 0, { 3, 0, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x2 },
    { 0, { 1, 29, 0 }, 2, 0, 2, 2, 0, 0, 0, 0x2 },
    { 0, { 2, 4, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x2 },
    { 0, { 2, 8, 0 }, 1, 0, 2, 2, 0, 3, 0, 0x42 },
    { 0, { 2, 1, 0 }, 1, 0, 1, 1, 0, 0, 0, 0x11 },
    { 0, { 2, 16, 0 }, 1, 0, 1, 1, 0, 0, 0, 0xC },
    { 0, { 3, 29, 1 }, 2, 0, 2, 2, 0, 0, 0, 0x80 },
    { 0, { 3, 16, 1 }, 1, 0, 2, 2, 0, 0, 0, 0x8 },
    { 0, { 2, 0, 0 }, 1, 0, 1, 1, 0, 0, 0, 0x4 },
    { 0, { 1, 0, 0 }, 1, 0, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 3, 0, 0 }, 1, 0, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 1, 29, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x80 },
    { 0, { 2, 4, 0 }, 1, 0, 1, 2, 0, 0, 0, 0x2 },
    { 0, { 2, 8, 0 }, 1, 0, 1, 1, 0, 0, 0, 0x41 },
    { 0, { 2, 1, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x10 },
    { 0, { 2, 16, 0 }, 1, 0, 2, 2, 0, 0, 0, 0x8 },
    { 0, { 3, 5, 1 }, 1, 0, 2, 2, 0, 0, 0, 0x80 },
    { 0, { 3, 8, 1 }, 2, 0, 2, 2, 0, 0, 0, 0x40 },
    { 0, { 4, 1, 0 }, 1, 11, 1, 2, 2, 0, 0, 0x11 },
    { 0, { 3, 6, 1 }, 2, 0, 3, 2, 1, 4, 0, 0x0 },
    { 0, { 7, 1, 2 }, 2, 11, 2, 2, 2, 0, 0, 0x40 },
    { 0, { 7, 3, 3 }, 2, 11, 1, 2, 2, 0, 0, 0x2 },
    { 0, { 3, 1, 4 }, 1, 11, 2, 2, 2, 0, 0, 0x80 },
    { 0, { 5, 2, 5 }, 1, 10, 1, 1, 2, 0, 0, 0x4 },
    { 0, { 3, 2, 6 }, 1, 10, 1, 1, 2, 5, 0, 0x4 },
    { 0, { 7, 6, 7 }, 1, 11, 2, 2, 2, 0, 0, 0x0 },
    { 0, { 5, 6, 8 }, 1, 11, 1, 2, 3, 0, 0, 0xC },
};

/* The eleven 16-byte command records menu_draw.c reads as struct LookupRecord, kept as 175 raw bytes: the
 * one-byte selectionMemberCapacityTable label sits on the last record's padding byte (0x003EC0D7), so a
 * struct array would overlap it and the data carve could not tile the span. */
unsigned char sharedCommandRecordTable[175] = {
    0, 0, 0, 0, 4, 3, 9, 1,
    10, 1, 1, 2, 0, 0, 1, 0,
    0, 0, 0, 0, 2, 6, 10, 2,
    11, 4, 2, 2, 6, 0, 0, 0,
    0, 0, 0, 0, 6, 1, 11, 2,
    0, 3, 2, 3, 7, 0, 0, 0,
    0, 0, 0, 0, 3, 2, 12, 1,
    11, 2, 2, 3, 0, 0, 128, 0,
    0, 0, 0, 0, 2, 1, 13, 1,
    10, 1, 1, 3, 0, 0, 4, 0,
    0, 0, 0, 0, 5, 7, 14, 2,
    11, 1, 2, 2, 0, 0, 10, 0,
    0, 0, 0, 0, 4, 3, 15, 1,
    11, 2, 2, 3, 0, 0, 64, 0,
    0, 0, 0, 0, 4, 0, 0, 1,
    0, 1, 2, 4, 0, 0, 2, 0,
    0, 0, 0, 0, 4, 0, 0, 2,
    0, 1, 2, 5, 0, 0, 4, 0,
    0, 0, 0, 0, 4, 0, 0, 1,
    0, 2, 2, 4, 0, 0, 128, 0,
    0, 0, 0, 0, 4, 0, 0, 3,
    0, 2, 2, 6, 0, 0, 128
};

unsigned char selectionMemberCapacityTableByKind[16] = { 7, 7, 3, 7, 6, 3, 7, 3, 3, 3, 0, 0, 0, 0, 0, 0 };

unsigned short selectionSlotOpenCostTable[28] = {
    300, 300, 300, 300, 300, 300, 300, 600,
    600, 600, 600, 600, 600, 600, 600, 1200,
    1200, 1200, 1200, 1200, 1200, 1200, 1200, 2400,
    2400, 2400, 2400, 4800
};

unsigned short selectionSlotRewardTable[28] = {
    450, 450, 450, 450, 450, 450, 450, 900,
    900, 900, 900, 900, 900, 900, 900, 1800,
    1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800,
    3600, 3600, 3600, 7200
};

unsigned char selectionMemberCapacityTable[] = { 0 };

unsigned char selectionRecordTable[] = { 2 };

unsigned char selectionRecordDispatchValues[] = { 1 };

unsigned char selectionRecordTailTable[766] = {
    200, 0, 0, 0, 2, 4, 150, 0,
    0, 0, 0, 5, 100, 0, 0, 0,
    0, 7, 100, 0, 0, 0, 2, 48,
    200, 0, 0, 0, 2, 49, 200, 0,
    0, 0, 1, 29, 100, 0, 0, 0,
    1, 30, 150, 0, 0, 0, 2, 42,
    150, 0, 0, 0, 0, 16, 100, 0,
    0, 0, 1, 33, 150, 0, 0, 0,
    1, 34, 150, 0, 0, 0, 2, 9,
    150, 0, 0, 0, 2, 14, 150, 0,
    0, 0, 1, 31, 150, 0, 0, 0,
    1, 32, 150, 0, 0, 0, 2, 32,
    200, 0, 0, 0, 2, 31, 200, 0,
    0, 0, 2, 33, 200, 0, 0, 0,
    2, 34, 200, 0, 0, 0, 2, 10,
    150, 0, 0, 0, 2, 11, 150, 0,
    0, 0, 2, 12, 150, 0, 0, 0,
    2, 13, 150, 0, 0, 0, 2, 15,
    150, 0, 0, 0, 2, 16, 150, 0,
    0, 0, 2, 17, 150, 0, 0, 0,
    2, 18, 150, 0, 0, 0, 2, 43,
    44, 1, 1, 0, 0, 17, 144, 1,
    2, 0, 0, 26, 88, 2, 3, 0,
    1, 45, 88, 2, 4, 0, 2, 24,
    44, 1, 0, 0, 2, 23, 44, 1,
    0, 0, 2, 22, 44, 1, 0, 0,
    2, 25, 44, 1, 0, 0, 2, 29,
    44, 1, 0, 0, 2, 27, 44, 1,
    0, 0, 2, 28, 44, 1, 0, 0,
    2, 30, 44, 1, 0, 0, 2, 7,
    88, 2, 0, 0, 2, 2, 88, 2,
    0, 0, 2, 3, 88, 2, 0, 0,
    2, 8, 144, 1, 0, 0, 2, 39,
    244, 1, 5, 0, 0, 1, 244, 1,
    0, 0, 0, 3, 144, 1, 0, 0,
    1, 38, 88, 2, 6, 0, 0, 8,
    200, 0, 0, 0, 0, 9, 200, 0,
    0, 0, 0, 12, 200, 0, 0, 0,
    0, 13, 200, 0, 0, 0, 2, 50,
    144, 1, 0, 0, 2, 51, 144, 1,
    0, 0, 2, 19, 244, 1, 7, 0,
    2, 35, 144, 1, 0, 0, 1, 56,
    144, 1, 0, 0, 1, 55, 144, 1,
    0, 0, 1, 57, 144, 1, 0, 0,
    1, 58, 144, 1, 0, 0, 2, 40,
    176, 4, 8, 0, 0, 15, 232, 3,
    9, 0, 0, 28, 220, 5, 10, 0,
    1, 53, 176, 4, 11, 0, 2, 5,
    32, 3, 0, 0, 2, 46, 176, 4,
    12, 0, 0, 4, 88, 2, 0, 0,
    1, 35, 220, 5, 13, 0, 1, 59,
    244, 1, 0, 0, 1, 60, 244, 1,
    0, 0, 1, 61, 244, 1, 0, 0,
    1, 62, 244, 1, 0, 0, 2, 6,
    32, 3, 0, 0, 0, 20, 232, 3,
    14, 0, 1, 36, 232, 3, 0, 0,
    1, 48, 176, 4, 15, 0, 0, 11,
    144, 1, 0, 0, 0, 6, 144, 1,
    0, 0, 0, 14, 144, 1, 0, 0,
    0, 10, 144, 1, 0, 0, 2, 41,
    176, 4, 0, 0, 2, 44, 32, 3,
    0, 0, 1, 37, 32, 3, 0, 0,
    1, 50, 232, 3, 16, 0, 2, 38,
    176, 4, 0, 0, 0, 18, 220, 5,
    17, 0, 1, 43, 220, 5, 0, 0,
    1, 39, 8, 7, 18, 0, 2, 21,
    232, 3, 0, 0, 2, 20, 32, 3,
    0, 0, 0, 25, 32, 3, 0, 0,
    1, 49, 8, 7, 19, 0, 0, 23,
    16, 14, 20, 0, 0, 21, 16, 14,
    21, 0, 1, 44, 16, 14, 22, 0,
    1, 41, 16, 14, 23, 0, 2, 36,
    96, 9, 0, 0, 2, 45, 160, 15,
    24, 0, 1, 40, 160, 15, 0, 0,
    1, 46, 128, 12, 0, 0, 2, 37,
    240, 10, 0, 0, 0, 27, 192, 18,
    25, 0, 0, 19, 96, 9, 0, 0,
    1, 47, 128, 12, 0, 0, 1, 52,
    16, 14, 26, 0, 1, 54, 128, 12,
    0, 0, 1, 42, 240, 10, 27, 0,
    0, 2, 96, 9, 0, 0, 2, 47,
    128, 37, 28, 0, 0, 24, 32, 28,
    29, 0, 0, 22, 32, 28, 30, 0,
    1, 51, 64, 31, 31, 0, 1, 63,
    0, 0, 0, 0, 2, 52, 0, 0,
    0, 0, 2, 53, 0, 0, 0, 0,
    2, 54, 0, 0, 0, 0, 2, 55,
    0, 0, 0, 0, 2, 56, 0, 0,
    0, 0, 2, 57, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0
};

