#ifndef INCLUDE_MAIN_INTERP_TEXT_H
#define INCLUDE_MAIN_INTERP_TEXT_H

#include "main/msg_slot_core.h"

/* Setters of a slot type 1 (text) slot, from src/main/interp_text.c; they do nothing for another slot type. */

/* interpSetTextArg is declared by msg_window_slot.c alone, its only caller, which returns its v0. */
void interpSetSpeakerName(MsgSlot *slot, const char *name);
void interpSetOpening(MsgSlot *slot);

#endif /* INCLUDE_MAIN_INTERP_TEXT_H */
