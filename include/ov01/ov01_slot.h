#ifndef INCLUDE_OV01_OV01_SLOT_H
#define INCLUDE_OV01_OV01_SLOT_H

/* Functions of src/ov01/ov01_slot.c that other files call. */

/* Bits of a slot record's flags word (CallbackSlot, TaskRecord, MonsSlot, UnitSideSlot, DataObjectSlot, ObjSlot):
 * the claim bit of ov01SlotClaim, the update sweep skips a record with SKIP_UPDATE, the draw sweep with SKIP_DRAW. */
#define OV01_SLOT_CLAIM 0x1
#define OV01_SLOT_SKIP_UPDATE 0x2
#define OV01_SLOT_SKIP_DRAW 0x4

/* Slot helpers: the record is any slot-shaped struct (flags, update and draw at +0, +4, +8). */
int ov01SlotClaim(int *flags);
void ov01SlotClear(void *slot);
/* Unprototyped on purpose: monsSlotRecordRelease (unit_side_table.c) and unitSlotRelease (data_obj_table.c)
 * call it with no argument. */
void ov01SlotRelease();

/* Claims a callback slot (0x60 bytes). Returned as void *: obj_slot.c uses it as its SaveStack, whose top word
 * is at the update callback's offset, so one struct would need a union that no reader asks for. */
void *ov01CallbackSlotClaim(void);
/* Releases a claimed callback slot; ov01StepFinish passes its SaveStack (void *, like ov01CallbackSlotClaim). */
void ov01CallbackSlotRelease(void *slot);
void ov01CallbackSlotsClear(void);
void ov01CallbackSlotsRunUpdate(void);

#endif /* INCLUDE_OV01_OV01_SLOT_H */
