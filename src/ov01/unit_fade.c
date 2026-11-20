#include "common.h"
#include "ov01/ov01_task.h"
#include "ov01/unit_cmd.h"
#include "main/game_camera.h"
#include "ov01/unit_fade.h"
#include "ov01/cmd_exec.h"

/* A unit has three slot actors (+0x1DC) and a part flag word per slot (+0x7A). */
#define FADE_SLOT_COUNT 3
/* slotPartFlags bit: the slot is skipped by every fade step. */
#define FADE_SLOT_SKIP 0x4000

void unitFadeInStep(FadeTask *task);
void unitFadeOutStep(FadeTask *task);
void unitSlotFadeInStep(FadeTask *task);
void unitSlotFadeOutStep(FadeTask *task);
void unitFadeToStep(FadeTask *task);

/*
 * unitFadeIn: starts a fade-in of a unit. It takes a
 * fade task from ov01TaskClaim at progress 0 with step 0.1, sets the 0x10000
 * flag on the unit's motion actor (+0x1D0) and its slot actors, and records
 * the task at unit+0x1E8. Callers include unitBind.
 */
FadeTask *unitFadeIn(BattleUnit *unit) {
    FadeTask *fade;
    int slotIndex;

    fade = ov01TaskClaim();
    if (fade == 0) {
        return 0;
    }
    fade->payload.unitFade.unit = unit;
    fade->payload.unitFade.step = FADE_DEFAULT_STEP;
    fade->payload.unitFade.progress = 0.0f;
    fade->update = unitFadeInStep;
    fade->draw = 0;
    unit->unit->drawFlags |= ACTOR_STATE_FADING;
    for (slotIndex = 0; slotIndex < FADE_SLOT_COUNT; slotIndex++) {
        if (unit->children[slotIndex] != 0) {
            unit->children[slotIndex]->drawFlags |= ACTOR_STATE_FADING;
        }
    }
    statEffOn(unit);
    unit->fade = fade;
    return fade;
}

/*
 * unitFadeInStep: per-frame step of unitFadeIn's
 * task. It writes the progress as transparency to the motion actor and the
 * slot actors, and at 1.0 clears the 0x10000 flag and ends the task.
 */
void unitFadeInStep(FadeTask *task) {
    BattleUnit *unit;
    struct XenoActor *slot;
    float progress;
    int slotIndex;

    unit = task->payload.unitFade.unit;
    progress = task->payload.unitFade.progress + task->payload.unitFade.step;
    task->payload.unitFade.progress = progress;
    if (progress >= 1.0f) {
        unit->unit->drawFlags &= ~ACTOR_STATE_FADING;
        for (slotIndex = 0; slotIndex < FADE_SLOT_COUNT; slotIndex++) {
            if (unit->children[slotIndex] != 0 && (unit->stats->pairSecond[slotIndex] & FADE_SLOT_SKIP) == 0) {
                unit->children[slotIndex]->drawFlags &= ~ACTOR_STATE_FADING;
            }
        }
        ov01TaskRelease(task);
        unit->fade = 0;
        return;
    }
    unit->unit->alpha = progress;
    for (slotIndex = 0; slotIndex < FADE_SLOT_COUNT; slotIndex++) {
        slot = unit->children[slotIndex];
        if (slot != 0 && (unit->stats->pairSecond[slotIndex] & FADE_SLOT_SKIP) == 0) {
            slot->alpha = task->payload.unitFade.progress;
        }
    }
}

/*
 * unitFadeOut: starts a fade-out of a unit from progress
 * 1.0 with step 0.1 and callback unitFadeOutStep; sets 0x10000 on its actors.
 */
FadeTask *unitFadeOut(BattleUnit *unit) {
    FadeTask *task;
    int slotIndex;

    task = ov01TaskClaim();
    if (task == 0) {
        return 0;
    }
    task->payload.unitFade.progress = 1.0f;
    task->payload.unitFade.step = FADE_DEFAULT_STEP;
    task->payload.unitFade.unit = unit;
    task->update = unitFadeOutStep;
    task->draw = 0;
    unit->unit->drawFlags |= ACTOR_STATE_FADING;
    for (slotIndex = 0; slotIndex < FADE_SLOT_COUNT; slotIndex++) {
        if (unit->children[slotIndex] != 0) {
            unit->children[slotIndex]->drawFlags |= ACTOR_STATE_FADING;
        }
    }
    statEffOff(unit);
    unit->fade = task;
    return task;
}

/*
 * unitFadeOutStep: per-frame step of unitFadeOut's
 * task. At 0.0 it sets transparency 0 on the motion actor and slot actors and
 * ends the task.
 */
void unitFadeOutStep(FadeTask *task) {
    BattleUnit *unit;
    struct XenoActor *slot;
    float progress;
    int slotIndex;

    unit = task->payload.unitFade.unit;
    progress = task->payload.unitFade.progress - task->payload.unitFade.step;
    task->payload.unitFade.progress = progress;
    if (progress <= 0.0f) {
        unit->unit->alpha = 0.0f;
        for (slotIndex = 0; slotIndex < FADE_SLOT_COUNT; slotIndex++) {
            slot = unit->children[slotIndex];
            if (slot != 0) {
                slot->alpha = 0.0f;
            }
        }
        ov01TaskRelease(task);
        unit->fade = 0;
        return;
    }
    unit->unit->alpha = progress;
    for (slotIndex = 0; slotIndex < FADE_SLOT_COUNT; slotIndex++) {
        slot = unit->children[slotIndex];
        if (slot != 0) {
            slot->alpha = task->payload.unitFade.progress;
        }
    }
}

/*
 * unitSlotFadeIn: fades in the actor in slot `index`
 * (unit+0x1DC + 4*index) with the FadeTask callback unitSlotFadeInStep.
 * It clears bit 0x4 and sets 0x10000 on that actor.
 */
FadeTask *unitSlotFadeIn(BattleUnit *unit, int index) {
    struct XenoActor *actor = unit->children[index];
    FadeTask *task;

    if (actor == 0) {
        return 0;
    }
    task = ov01TaskClaim();
    if (task != 0) {
        task->payload.slotFade.actor = actor;
        task->payload.slotFade.step = FADE_DEFAULT_STEP;
        task->payload.slotFade.progress = 0.0f;
        task->update = unitSlotFadeInStep;
        task->draw = 0;
        actor->drawFlags |= ACTOR_STATE_FADING;
        actor->drawFlags &= ~ACTOR_STATE_HIDDEN;
    }
    return task;
}

/*
 * unitSlotFadeInStep: the step of unitSlotFadeIn's
 * task. Progress rises by step; at 1.0 the actor's 0x10000 flag is cleared
 * and the task ends.
 */
void unitSlotFadeInStep(FadeTask *task) {
    struct XenoActor *actor;
    float progress;

    actor = task->payload.slotFade.actor;
    progress = task->payload.slotFade.progress + task->payload.slotFade.step;
    task->payload.slotFade.progress = progress;
    if (progress >= 1.0f) {
        actor->drawFlags &= ~ACTOR_STATE_FADING;
        ov01TaskRelease(task);
        return;
    }
    actor->alpha = progress;
}

/*
 * unitSlotFadeOut: fades out the actor in slot
 * `index` from progress 1.0 with the callback unitSlotFadeOutStep. Sets 0x10000.
 */
FadeTask *unitSlotFadeOut(BattleUnit *unit, int index) {
    struct XenoActor *actor = unit->children[index];
    FadeTask *fade;

    if (actor == 0) {
        return 0;
    }
    fade = ov01TaskClaim();
    if (fade == 0) {
        return 0;
    }
    fade->payload.slotFade.step = FADE_DEFAULT_STEP;
    fade->payload.slotFade.progress = 1.0f;
    fade->payload.slotFade.actor = actor;
    fade->update = unitSlotFadeOutStep;
    actor->drawFlags |= ACTOR_STATE_FADING;
    fade->draw = 0;
    return fade;
}

/*
 * unitSlotFadeOutStep: the step of unitSlotFadeOut's
 * task. Progress falls by step; at 0.0 the actor becomes transparent, bit
 * 0x4 is set (the actor is hidden), and the task ends.
 */
void unitSlotFadeOutStep(FadeTask *task) {
    struct XenoActor *actor;
    float progress;

    actor = task->payload.slotFade.actor;
    progress = task->payload.slotFade.progress - task->payload.slotFade.step;
    task->payload.slotFade.progress = progress;
    if (progress <= 0.0f) {
        actor->alpha = 0.0f;
        actor->drawFlags |= ACTOR_STATE_HIDDEN;
        ov01TaskRelease(task);
        return;
    }
    actor->alpha = progress;
}

/*
 * unitFadeTo: fades a unit to targetPercent over `frames`
 * steps with the callback unitFadeToStep.
 */
FadeTask *unitFadeTo(BattleUnit *unit, int frames, int targetPercent) {
    FadeTask *task;
    int slotIndex;

    task = ov01TaskClaim();
    if (task == 0) {
        return 0;
    }
    task->payload.unitFade.unit = unit;
    task->update = unitFadeToStep;
    task->draw = 0;
    task->payload.unitFade.progress = unit->unit->alpha;
    task->payload.unitFade.target = (float) targetPercent / 100.0f;
    task->payload.unitFade.step = (task->payload.unitFade.target - task->payload.unitFade.progress) / (float) frames;
    unit->unit->drawFlags |= ACTOR_STATE_FADING;
    for (slotIndex = 0; slotIndex < FADE_SLOT_COUNT; slotIndex++) {
        if (unit->children[slotIndex] != 0) {
            unit->children[slotIndex]->drawFlags |= ACTOR_STATE_FADING;
        }
    }
    unit->fade = task;
    return task;
}

/*
 * unitFadeToStep: the per-frame step of
 * unitFadeTo's task. It advances progress by step, writes it to the
 * motion actor and the slot actors that do not carry the 0x4000 part flag,
 * and ends the task once the remaining distance is below |step|.
 */
void unitFadeToStep(FadeTask *task) {
    BattleUnit *unit;
    struct XenoActor *slot;
    float progress;
    float remaining;
    int slotIndex;

    unit = task->payload.unitFade.unit;
    progress = task->payload.unitFade.progress + task->payload.unitFade.step;
    remaining = task->payload.unitFade.target - progress;
    task->payload.unitFade.progress = progress;
    if (remaining < 0.0f) {
        remaining = -remaining;
    }
    if (remaining < (task->payload.unitFade.step < 0.0f ? -task->payload.unitFade.step : task->payload.unitFade.step)) {
        ov01TaskRelease(task);
        unit->fade = 0;
        return;
    }
    unit->unit->alpha = task->payload.unitFade.progress;
    for (slotIndex = 0; slotIndex < FADE_SLOT_COUNT; slotIndex++) {
        slot = unit->children[slotIndex];
        if (slot != 0 && (unit->stats->pairSecond[slotIndex] & FADE_SLOT_SKIP) == 0) {
            slot->alpha = task->payload.unitFade.progress;
        }
    }
}

