/*
 * Battle step chain (the continuation set by ov01StepSetNext): ov01StepBattle ->
 * ov01StepStartFadeOut -> ov01StepAwaitObjectsIdle -> ov01StepBeginFinish -> ov01StepFinish.
 */
#include "common.h"
#include "ov01/tgt_select.h"
#include "ov01/choice_menu.h"
#include "ov01/scene.h"
#include "ov01/scene_finish.h"
#include "ov01/menu.h"
#include "ov01/ov01_slot.h"
#include "ov01/entry_first_init.h"
#include "main/game_data.h"
#include "main/msg_state.h"

extern int ov01SceneDone;
void ov01StepStartFadeOut(struct SaveStack *context);

#define FRAMES_PER_SECOND 30

/* First battle step: runs the target selection until it is done, then hides the cursor and moves on to
 * ov01StepStartFadeOut. Under a time limit it shows the remaining time and counts it down by one frame. */
void ov01StepBattle(struct SaveStack *context)
{
    int rate; /* the divisor sits in a register before the test, as in the original */

    if (tgtSelectStep() != 0) {
        menuBoostDispOff();
        ov01StepSetNext(context, ov01StepStartFadeOut);
    }
    rate = FRAMES_PER_SECOND;
    if (cfEventGet() & CF_EVENT_TIME_LIMIT) {
        msgPanelSetTimecode(0, 0, gameWork.panelValue / rate, gameWork.panelValue % rate);
        if (gameWork.panelValue > 0) {
            gameWork.panelValue--;
        }
    }
}

void ov01StepAwaitObjectsIdle(struct SaveStack *context);
void ov01StepBeginFinish(struct SaveStack *context);
void ov01StepFinish(struct SaveStack *context);

/* Battle step: waits until battleAfter reports the battle over, then opens a
 * panel object (fadeOut) and moves on to ov01StepAwaitObjectsIdle. */
void ov01StepStartFadeOut(struct SaveStack *context)
{
    if (battleAfter() == 0) {
        return;
    }
    cfEventGet();
    fadeOut(4, 2);
    ov01StepSetNext(context, ov01StepAwaitObjectsIdle);
}

/* Battle step: waits while the object list of entry_first_init.c is busy (fadeEndChk is
 * 1 when screenFadeTask is empty), then clears the scene (ov01SceneRelease). */
void ov01StepAwaitObjectsIdle(struct SaveStack *context)
{
    if (fadeEndChk() == 0) {
        return;
    }
    ov01SceneRelease();
    ov01StepSetNext(context, ov01StepBeginFinish);
}

void ov01StepBeginFinish(struct SaveStack *context)
{
    ov01StepSetNext(context, ov01StepFinish);
}

/* Last step of the scene: releases its entry and sets ov01SceneDone, the scene
 * done flag that the frame loop (ov01SceneIsDone) polls. */
void ov01StepFinish(struct SaveStack *context)
{
    ov01CallbackSlotRelease(context);
    ov01SceneDone = 1;
}
