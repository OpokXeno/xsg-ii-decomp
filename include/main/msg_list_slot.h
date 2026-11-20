#ifndef INCLUDE_MAIN_MSG_LIST_SLOT_H
#define INCLUDE_MAIN_MSG_LIST_SLOT_H

/* Functions of src/main/msg_list_slot.c: the state 2 (list window) setup of a message slot
 * (MsgSlot, timecode_counter.h). */

struct MsgSlot;

/* Initialises the slot as a list window (state 2). */
void msgListSlotInit(struct MsgSlot *slot);
/* Applies the geometry to a list window slot (state 2) and recounts its visible rows. */
void msgListSlotSetGeometry(struct MsgSlot *slot, int x, int y, int width, int height);
/* Forwards to deltaPerStep with the fixed step 0x1E (the list row pitch). */
int msgListRowsBetween(int first, int second);

#endif /* INCLUDE_MAIN_MSG_LIST_SLOT_H */
