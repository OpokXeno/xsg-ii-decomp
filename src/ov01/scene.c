#include "common.h"
#include "main/TiWindBase.h"
#include "main/gifState.h"
#include "main/nml_model_set.h"
#include "main/pad.h"
#include "main/face_point.h"
#include "main/sef.h"
#include "main/game_camera.h"
#include "main/xgl_cd_arena.h"
#include "main/gr_screen.h"
#include "main/text_window.h"
#include "main/choice_menu.h"
#include "main/text_marker.h"
#include "main/msg_state.h"
#include "main/sef_light_set.h"
#include "main/xenoActor.h"
#include "main/xgl_light.h"
#include "main/xgl_render.h"
#include "ov01/obj_slot.h"
#include "ov01/data_file_load.h"
#include "ov01/unit_cmd.h"
#include "ov01/unit_tbl.h"
#include "ov01/mons_slot.h"
#include "ov01/battle_init.h"
#include "ov01/calc.h"
#include "ov01/cmd_exec.h"
#include "ov01/tgt_select.h"
#include "ov01/choice_menu.h"
#include "ov01/scene_script.h"
#include "ov01/scene.h"
#include "ov01/data_record_get.h"
#include "ov01/think_dispatch.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives_unit.h"
#include "ov01/menu.h"
#include "ov01/m_gs.h"
#include "ov01/handle_table.h"
#include "ov01/scene_load.h"

/* ov01 0x00AA8EA0, in a part of the overlay that has no C file yet. */
/* main xgl_2 (no header prototype: nml_model_set passes its own light block type). */

/* Cleared when the scene loop starts; no code of the overlay reads it. */
static int sceneLoopState;

/* Frames run by the scene loop (counted by ov01SceneLoop). */
static int ov01SceneFrames;

/* Scene init: runs the reset chain of the battle systems, makes the light block the default light set of xgl and
 * sef_light_set.c, resets the menus and text systems, clears the frame counter ov01SceneFrames and sets the draw
 * hook (sRender.battle_draw_callback) to ov01DrawAll. The light block is the 0x180-byte light set (see initLight,
 * sb_natives_unit.c). */
void ov01SceneInit(void)
{
    ov01ResetAll();
    dataFileInit();
    battleWorkTablesReset();
    ov01SceneInitDummy();
    monsSlotReset();
    unitInit();
    dataVPadReset();
    msgReset();
    cmdSystemInit();
    tgtSelectInit();
    dataStateReset();
    menuPanelRowsReset();
    handleTableInit();
    ov01SceneStateReset();
    cameraInit();
    xglLightSetDefault((XglLightSet *)lightBlockGet());
    sefLightSetsApply((XglLightSet *)lightBlockGet());
    choiceMenuResetAll();
    textEntryPoolReset();
    analyzePanelClear();
    textWindowReset();
    tiWindClearTables();
    ov01SceneFrames = 0;
    sRender.battle_draw_callback = ov01DrawAll;
}


/* Scene entry: culling ignore on, scene init, then the battle step chain. */
void ov01SceneEnter(void) {
    xglCullingIgnore();
    ov01SceneInit();
    ov01StepChainStart();
}

int pauseProc(void);
int ov01SceneIsDone(void);

/*
 * Scene frame loop: each frame runs the update and draw passes (scripts, objects, camera, actors, effects, text
 * and messages) unless the wait hook pauseProc holds it, counts ov01SceneFrames, and ends when the scene is
 * done or the soft-reset combination is pressed (ov01SoftResetPressed; that sets CONTROL_SOFT_RESET and stops the
 * gif effect).
 */
void ov01SceneLoop(void)
{
    sceneLoopState = 0;
    xglRenderClearSkipOn();
    for (;;) {
        if (ov01SoftResetPressed()) {
            gameWork.control |= CONTROL_SOFT_RESET;
            gifStateDisableEffect();
            break;
        }
        pxySetMode4Flag();
        if (gifStateEffectReady() == 1) {
            pxyRequestOverlayPass();
        }
        if (!pauseProc()) {
            sbRunAll();
            ov01UpdateAll();
            cameraApplyView();
            tiWindAdvanceAll();
            XenoActorDoit();
            XenoActorDisp();
            ov01EffectPauseSet(0);
            sefExecEffect();
            textUpdate();
            msgUpdateTimed();
            msgQueueClose();
            if (ov01SceneIsDone() == 1) {
                break;
            }
            ov01SceneFrames++;
        }
        GameSnapShotCheck();
        xglSleep();
    }
    xglRenderClearSkipOff();
}


/* 1 when START is pressed on pad 0 while L1, L2, R1, R2 and SELECT are held (the soft reset). */
int ov01SoftResetPressed(void)
{
    PadRecord *pad = &PadData[0];
    /* 64-bit (codegen): keeps the mask in one register for the and and the compare; an int gives andi. */
    u64 held;

    if (pad->pressed & PAD_START) {
        held = pad->directionHeld;
        if ((held & PAD_SOFT_RESET_HELD) == PAD_SOFT_RESET_HELD) {
            return 1;
        }
    }
    return 0;
}

/* Wait hook polled by the frame loop; it never reports a wait. */
int pauseProc(void) {
    return 0;
}

/* Scene end: release the scene, reset the tables, clear the draw hook (sRender.battle_draw_callback). */
void ov01SceneEnd(void) {
    ov01SceneRelease();
    ov01ResetAll();
    sRender.battle_draw_callback = 0;
    xglCdArenaClose();
}

/* Scene release: culling ignore off, counters cleared, records of the scene key
 * released (sbThreadsRelease), actors released, tables cleared. */

void ov01SceneRelease(void) {
    xglCullingIgnoreOff();
    keyBuffClear();
    sbThreadsRelease(ov01ScriptKeyGet());
    XenoActorReleaseAll();
    ov01SceneSystemsShutdown();
    partySnapshotBuild();
    ov01ClearTables();
}

/* Frames run by the scene loop (ov01SceneFrames, counted by ov01SceneLoop). */
int ov01SceneFrameCount(void) {
    return ov01SceneFrames;
}

extern int ov01SceneDone;

/* Scene done flag, set by the last step ov01StepFinish. */
int ov01SceneIsDone(void) {
    return ov01SceneDone;
}

/* Keeps the given unit when it is still valid; otherwise a random valid unit of
 * the player side (unitTblGet side 0) is returned, or 0 when there is none. */
/* Player-side party slots (unitTblGet side 0 returns 3; one spare). */
#define UNIT_SELECT_SLOT_COUNT 4
/* Entry indices below this are player characters. */
#define UNIT_SELECT_PL_INDEX_LIMIT 8

BattleUnit *selectPL(BattleUnit *keep)
{
    BattleUnit **table;
    BattleUnit *picked[UNIT_SELECT_SLOT_COUNT];
    int count;
    int i;
    int pickedCount;
    int idx;

    if (keep != 0) {
        if (calcBattleChk(keep) != 0) {
            if (keep->stats->key < UNIT_SELECT_PL_INDEX_LIMIT) {
                return keep;
            }
        }
    }

    if (unitLiveNumGet2(0) == 0) {
        return 0;
    }

    i = 0;
    count = unitTblGet(0, &table);
    pickedCount = 0;
    if (count > 0) {
        for (; i < count; i++) {
            if (table[i] != 0 && calcBattleChk(table[i]) != 0
                && table[i]->stats->key < UNIT_SELECT_PL_INDEX_LIMIT) {
                picked[pickedCount] = table[i];
                pickedCount++;
            }
        }
    }

    if (pickedCount == 0) {
        return 0;
    }

    idx = rnd(pickedCount - 1);
    return picked[idx];
}

void *memset(void *destination, int value, unsigned int size);

/* Effect group of the battle start bonus effect; its record is 1 (boost), 2 (party) or 3 (both). */
#define BONUS_EFFECT_GROUP 0x154

/*
 * Battle start bonuses of the cfEvent: CF_EVENT_BOOST_BONUS adds 1..3 stocks to the boost gauge of side 0,
 * CF_EVENT_PARTY_BONUS raises the party units (units with key 11..15 get 100 or 200 current, the others 1..3
 * levels). With any bonus every unit's unitValue is cleared and the bonus effect starts.
 */
void battleStartBonusApply(void)
{
    struct EffectRequest request;
    BattleUnit **units;
    BattleUnit **list; /* the second loop walks a copy of units; on units itself it reloads units every pass */
    int count;
    int i;
    int record;

    record = 0;
    if (cfEventGet() & CF_EVENT_BONUSES) {
        count = unitTblGet(0, &units);
        if (cfEventGet() & CF_EVENT_BOOST_BONUS) {
            record = 1;
            boostGaugeAddStock(boostGaugeSelect(0), rnd(2) + 1);
        }
        if (cfEventGet() & CF_EVENT_PARTY_BONUS) {
            for (i = 0; i < count; i++) {
                if (units[i] != 0) {
                    if ((unsigned int)((unsigned short)units[i]->stats->key - 11) < 5) {
                        unitCurrentAdd(units[i], rnd(1) * 100 + 100);
                    } else {
                        unitLevelAdd(units[i], rnd(2) + 1);
                    }
                }
            }
            record = 2;
        }
        if ((cfEventGet() & CF_EVENT_BONUSES) == CF_EVENT_BONUSES) {
            record = 3;
        }
        if (count > 0) {
            list = units;
            i = count;
            do {
                if (*list != 0) {
                    (*list)->stats->waitCounter = 0;
                }
                list++;
            } while (--i != 0);
        }
        memset(&request, 0, sizeof(request));
        request.key = (BONUS_EFFECT_GROUP << 12) | record;
        sefCreateEffect(&request);
    }
}

/* Scene done flag, set by ov01StepFinish and read by the scene steps of scene_load.c and scene_finish.c. */
int ov01SceneDone;
