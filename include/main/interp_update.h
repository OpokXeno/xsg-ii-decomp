#ifndef INCLUDE_MAIN_INTERP_UPDATE_H
#define INCLUDE_MAIN_INTERP_UPDATE_H

#include "shared.h"
#include "main/timecode_counter.h"

/* Functions of src/main/interp_update.c: the state 1 (text) slot (MsgSlot of timecode_counter.h, body MsgSlotText). */

/* Flag bits of MsgSlot.flags of a text slot beyond the phase bits shared with MSG_SLOT_* (timecode_counter.h). */
#define INTERP_OPEN_FRAMES 12   /* frames the opening phase (flag 0x4) lasts before the slot is open */
#define INTERP_ANGLE_END 180    /* degrees at which the cosine ease reaches its end value */

void interpUpdate(MsgSlot *slot);
int interpAdvanceCursor(MsgSlot *slot);
int interpDrawTextSlot(MsgSlot *slot);
void interpRewind(MsgSlot *slot);
void interpSetPending(MsgSlot *slot, int value);
int interpScrollBy(MsgSlot *slot, int advance);
int interpStepCosine(MsgSlot *slot);

#endif /* INCLUDE_MAIN_INTERP_UPDATE_H */
