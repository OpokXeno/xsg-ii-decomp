#include "common.h"
#include "ov01/ov01_task.h"
#include "main/game_camera.h"
#include "ov01/unit_cmd.h"
#include "shared.h"

typedef struct FadeOwner {
    unsigned char unmodeled_00[0x90];
    struct XenoActor *target;  /* +0x90 */
    FadeTask *fadeActive;   /* +0x94: the running fade task (monsSlotFadeInStart and
                             * monsSlotFadeOutStart store it); cleared to 0 when the
                             * fade ends. */
} FadeOwner;

void monsSlotFadeInStep(FadeTask *task);
void monsSlotFadeOutStep(FadeTask *task);

/*
 * monsSlotFadeInStart: starts a fade-in of the owner's target. It takes a FadeTask
 * at progress 0 with step 0.1, sets the 0x10000 flag on the target, and
 * stores the task in owner+0x94. It returns the task, or 0 when none was free.
 */
FadeTask *monsSlotFadeInStart(FadeOwner *owner) {
    FadeTask *task = ov01TaskClaim();

    if (task != 0) {
        task->payload.monsFade.owner = owner;
        task->payload.monsFade.step = FADE_DEFAULT_STEP;
        task->payload.monsFade.progress = 0.0f;
        task->update = monsSlotFadeInStep;
        task->draw = 0;
        owner->fadeActive = task;
        owner->target->drawFlags |= ACTOR_STATE_FADING;
    }
    return task;
}

/*
 * monsSlotFadeInStep: the per-frame step of the fade-in
 * task. At 1.0 it clears the target's 0x10000 flag, ends the task and clears
 * fadeActive; below 1.0 it writes the progress to the target's alpha.
 */
void monsSlotFadeInStep(FadeTask *task) {
    FadeOwner *owner;
    struct XenoActor *target;
    f32 progress;

    owner = task->payload.monsFade.owner;
    progress = task->payload.monsFade.progress + task->payload.monsFade.step;
    task->payload.monsFade.progress = progress;
    if (progress >= 1.0f) {
        target = owner->target;
        target->drawFlags &= ~ACTOR_STATE_FADING;
        ov01TaskRelease(task);
        owner->fadeActive = 0;
        return;
    }
    owner->target->alpha = progress;
}

/*
 * monsSlotFadeOutStart: starts a fade-out of the owner's
 * target from progress 1.0 with step 0.1, with the callback monsSlotFadeOutStep,
 * and sets the 0x10000 flag on the target.
 */
FadeTask *monsSlotFadeOutStart(FadeOwner *owner) {
    FadeTask *task = ov01TaskClaim();

    if (task != 0) {
        task->payload.monsFade.progress = 1.0f;
        task->payload.monsFade.step = FADE_DEFAULT_STEP;
        task->payload.monsFade.owner = owner;
        task->update = monsSlotFadeOutStep;
        task->draw = 0;
        owner->fadeActive = task;
        owner->target->drawFlags |= ACTOR_STATE_FADING;
    }
    return task;
}

/*
 * monsSlotFadeOutStep: the per-frame step of the fade-out
 * task. At 0.0 it sets the target's alpha to 0, ends the task and
 * clears fadeActive; the 0x10000 flag stays set.
 */
void monsSlotFadeOutStep(FadeTask *task) {
    FadeOwner *owner;
    f32 progress;

    owner = task->payload.monsFade.owner;
    progress = task->payload.monsFade.progress - task->payload.monsFade.step;
    task->payload.monsFade.progress = progress;
    if (progress <= 0.0f) {
        owner->target->alpha = 0.0f;
        ov01TaskRelease(task);
        owner->fadeActive = 0;
        return;
    }
    owner->target->alpha = progress;
}
