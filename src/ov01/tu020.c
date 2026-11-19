/*
 * OV01 original TU 20: 0x00a88c60..0x00a88cb0 (4 functions)
 */
#include "common.h"
#include "ov01/tu002.h"
#include "main/tu051.h"
#include "ov01/tu010.h"
#include "ov01/tu020.h"

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
 * that monsTblSet selected (its +0x4 word, passed through monsTblValueSet).
 * monsTblValueGet returns it to ov01StepOpenHandles (tu038).
 */
extern int monsTblValue;

/* monsTblValueSet: stores the selected entry value. */
void monsTblValueSet(int value) {
    monsTblValue = value;
}

/* monsTblValueGet: returns the stored entry value. */
int monsTblValueGet(void) {
    return monsTblValue;
}

/* Initial value; the zero word after it is alignment fill up to the next 8-byte object. */
int monsTblValue = 1;
