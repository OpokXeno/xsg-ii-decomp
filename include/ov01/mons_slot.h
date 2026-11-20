#ifndef INCLUDE_OV01_MONS_SLOT_H
#define INCLUDE_OV01_MONS_SLOT_H

/* Functions of src/ov01/mons_slot.c that other files call. */

struct MonsSlot;

/* Makes a MonsSlot record (monsSlotNew) and inserts it in the active table; 0 when no record is free. */
struct MonsSlot *monsSlotCreate(void);
/* Hands out the active slot table; returns its length. */
int monsActiveSlotsGet(struct MonsSlot ***table);
struct DataStreamEntry;
/* Copies the pxy and txy buffers of the stream's first map into the slot actor. */
int monsSlotCopyParams(struct MonsSlot *slot, struct DataStreamEntry *stream);
void monsSlotReset(void);
void monsSlotRelease(struct MonsSlot *slot);

#endif /* INCLUDE_OV01_MONS_SLOT_H */
