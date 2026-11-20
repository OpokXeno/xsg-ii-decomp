#ifndef INCLUDE_MAIN_MSG_SLOT_START_H
#define INCLUDE_MAIN_MSG_SLOT_START_H

#include "main/msg_slot_core.h"

/* Functions of src/main/msg_slot_start.c that other files call. */

/* Sets the geometry of a state 1 (text) slot and the text rows and wrap width that follow from it. */
void msgTextSlotFit(MsgSlot *slot, int x, int y, int width, int height);
/* Feeds one text input (a text pointer) into a state 1 (text) slot. */
void msgEntryFeedText(MsgSlot *slot, const char *input);
/* Starts a type 1 text slot. */
void msgTextSlotStart(MsgSlot *slot);
/* Starts a choice slot; returns 0 when the slot was left alone, 1 otherwise. */
int msgItemSlotStart(MsgSlot *slot, char *name, int count);
int msgChoiceShowSingle(MsgSlot *first, char *name);
void msgChoiceCloseByState(MsgSlot *slot);
void msgSlotPlaceByState(MsgSlot *slot, int x, int y);
int msgSlotStatus(MsgSlot *slot);
int textRowsBetween(int start, int end);
/* Nonzero when any bit of mask is set in *flags. */
int msgChoiceTestFlags(unsigned int *flags, int mask);

#endif /* INCLUDE_MAIN_MSG_SLOT_START_H */
