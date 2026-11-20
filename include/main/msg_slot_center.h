#ifndef INCLUDE_MAIN_MSG_SLOT_CENTER_H
#define INCLUDE_MAIN_MSG_SLOT_CENTER_H

/* Functions of src/main/msg_slot_center.c that other files call. */

#include "main/msg_slot_core.h"

int msgSlotCenterX(MsgSlot *record);
int msgSlotCenterY(MsgSlot *record);
void msgSlotSetZ(MsgSlot *record, int value);
void grControlPointBlend(int *out_x, int *out_y, float fraction);
void grControlPointSet(int index, int x, int y);
int textIsSpace(const char *text);
int textIsLineBreak(const char *text);
const char *textSkipLineBreak(const char *text);

#endif /* INCLUDE_MAIN_MSG_SLOT_CENTER_H */
