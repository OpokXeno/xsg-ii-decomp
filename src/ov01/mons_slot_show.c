#include "common.h"
#include "ov01/mons_slot_run.h"
#include "main/game_camera.h"
#include "ov01/unit_cmd.h"
#include "ov01/mons_slot_show.h"

/*
 * monsSlotShow: clears the hidden bit of the record's actor, which makes it visible.
 */
void monsSlotShow(MonsSlot *slot) {
    slot->actor->drawFlags &= ~ACTOR_STATE_HIDDEN;
}

/*
 * monsSlotHide: sets the hidden bit of the record's actor.
 */
void monsSlotHide(MonsSlot *slot) {
    slot->actor->drawFlags |= ACTOR_STATE_HIDDEN;
}

/*
 * monsTblValue: the value of the monster table entry
 * that monsTblSet selected (its +0x4 word, passed through monsTblMapNoSet).
 * monsTblMapNoGet returns it to ov01StepOpenHandles (scene_load.c).
 */
extern int monsTblValue;

/* monsTblMapNoSet: stores the selected entry value. */
void monsTblMapNoSet(int value) {
    monsTblValue = value;
}

/* monsTblMapNoGet: returns the stored entry value. */
int monsTblMapNoGet(void) {
    return monsTblValue;
}

/* Initial value; the zero word after it is alignment fill up to the next 8-byte object. */
int monsTblValue = 1;
