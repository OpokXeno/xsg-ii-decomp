/*
 * OV01 original TU 37: 0x00a99ec0..0x00a9a4b0 (11 functions)
 */
#include "common.h"
#include "main/TiWindBase.h"
#include "main/gifState.h"
#include "main/nml_model_set.h"
#include "main/pad.h"
#include "main/face_point.h"
#include "main/sef.h"
#include "main/tu051.h"
#include "main/tu092.h"
#include "main/tu093.h"
#include "main/tu123.h"
#include "main/tu124.h"
#include "main/tu125.h"
#include "main/tu148.h"
#include "main/tu156.h"
#include "main/xenoActor.h"
#include "main/xgl_2.h"
#include "main/xgl_render.h"
#include "ov01/tu005.h"
#include "ov01/tu008.h"
#include "ov01/tu010.h"
#include "ov01/tu017.h"
#include "ov01/tu019.h"
#include "ov01/tu022.h"
#include "ov01/tu023.h"
#include "ov01/tu033.h"
#include "ov01/tu035.h"
#include "ov01/tu036.h"
#include "ov01/tu040.h"
#include "ov01/tu037.h"
#include "ov01/tu042.h"
#include "ov01/tu091.h"
#include "ov01/tu044.h"
#include "ov01/tu047.h"
#include "ov01/tu049.h"
#include "ov01/tu075.h"

/* ov01 0x00AA8EA0, in a part of the overlay that has no C file yet. */
void handleTableInit(void);
/* main xgl_2 (no header prototype: nml_model_set passes its own light block type). */
void xglLightSetDefault(XglLightSet *lightSet);

/* Cleared when the scene loop starts; no code of the overlay reads it. */
static int D_00AC4190;

/* Frames run by the scene loop (counted by ov01SceneLoop). */
static int ov01SceneFrames;

/* Scene init: runs the reset chain of the battle systems, makes the light block the default light set of xgl and
 * tu156, resets the menus and text systems, clears the frame counter ov01SceneFrames and sets the draw hook
 * (sRender.fade_callback) to ov01DrawAll. The light block is the 0x180-byte light set (see initLight, tu047). */
void ov01SceneInit(void)
{
    ov01ResetAll();
    dataFileInit();
    battleWorkTablesReset();
    func_00A9B0F8();
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
    func_00253048((XglLightSet *)lightBlockGet());
    choiceMenuResetAll();
    textEntryPoolReset();
    textBlockClear();
    textWindowReset();
    tiWindClearTables();
    ov01SceneFrames = 0;
    sRender.fade_callback = ov01DrawAll;
}

void ov01StepChainStart(void);

/* Scene entry: culling ignore on, scene init, then the battle step chain. */
void ov01SceneEnter(void) {
    xglCullingIgnore();
    ov01SceneInit();
    ov01StepChainStart();
}

int ov01SoftResetPressed(void);
int func_00A9A138(void);
int ov01SceneIsDone(void);

/*
 * Scene frame loop: each frame runs the update and draw passes (scripts, objects, camera, actors, effects, text
 * and messages) unless the wait hook func_00A9A138 holds it, counts ov01SceneFrames, and ends when the scene is
 * done or the soft-reset combination is pressed (ov01SoftResetPressed; that sets CONTROL_SOFT_RESET and stops the
 * gif effect).
 */
void ov01SceneLoop(void)
{
    D_00AC4190 = 0;
    xglRenderClearSkipOn();
    for (;;) {
        if (ov01SoftResetPressed()) {
            gameWork.control |= CONTROL_SOFT_RESET;
            gifStateDisableEffect();
            break;
        }
        pxySetMode4Flag();
        if (gifStateEffectReady() == 1) {
            func_001BA68C();
        }
        if (!func_00A9A138()) {
            sbRunAll();
            ov01UpdateAll();
            cameraApplyView();
            tiWindAdvanceAll();
            XenoActorDoit();
            XenoActorDisp();
            ov01LastValueSet(0);
            sefExecEffect();
            textUpdate();
            msgUpdateTimed();
            msgClose();
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

extern PadRecord PadData[];

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
int func_00A9A138(void) {
    return 0;
}

/* Scene end: release the scene, reset the tables, clear the draw hook (sRender.fade_callback). */
void ov01SceneEnd(void) {
    ov01SceneRelease();
    ov01ResetAll();
    sRender.fade_callback = 0;
    xglCdArenaClose();
}

/* Scene release: culling ignore off, counters cleared, records of the scene key
 * released (sbThreadsRelease), actors released, tables cleared. */
void partySnapshotBuild(void);

void ov01SceneRelease(void) {
    xglCullingIgnoreOff();
    dataCountersClear();
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

StatOwner *unitSelectPlTarget(StatOwner *keep)
{
    StatOwner **table;
    StatOwner *picked[UNIT_SELECT_SLOT_COUNT];
    int count;
    int i;
    int pickedCount;
    int idx;

    if (keep != 0) {
        if (unitCanAct(keep) != 0) {
            if (keep->stats->key < UNIT_SELECT_PL_INDEX_LIMIT) {
                return keep;
            }
        }
    }

    if (unitNumFlag4ClearStatZeroGet(0) == 0) {
        return 0;
    }

    i = 0;
    count = unitTblGet(0, &table);
    pickedCount = 0;
    if (count > 0) {
        for (; i < count; i++) {
            if (table[i] != 0 && unitCanAct(table[i]) != 0
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
 * Battle start bonuses of the cfEvent: CF_EVENT_BOOST_BONUS adds 1..2 stocks to the boost gauge of side 0,
 * CF_EVENT_PARTY_BONUS raises the party units (units with key 11..15 get 100 or 200 current, the others 1..2
 * levels). With any bonus every unit's unitValue is cleared and the bonus effect starts.
 */
void battleStartBonusApply(void)
{
    struct EffectRequest request;
    Ov01Object **units;
    Ov01Object **list; /* the second loop walks a copy of units; on units itself it reloads units every pass */
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
                    (*list)->stats->unitValue = 0;
                }
                list++;
            } while (--i != 0);
        }
        memset(&request, 0, sizeof(request));
        request.key = (BONUS_EFFECT_GROUP << 12) | record;
        sefStartEffectDefault(&request);
    }
}

/* Scene done flag, set by ov01StepFinish and read by the scene steps of tu038 and tu039. */
int ov01SceneDone;
