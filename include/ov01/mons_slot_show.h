#ifndef INCLUDE_OV01_MONS_SLOT_SHOW_H
#define INCLUDE_OV01_MONS_SLOT_SHOW_H

/* Functions of src/ov01/mons_slot_show.c that other files call. */

struct MonsSlot;

void monsSlotShow(struct MonsSlot *slot);
void monsSlotHide(struct MonsSlot *slot);
int monsTblMapNoGet(void);
void monsTblMapNoSet(int value);

#endif /* INCLUDE_OV01_MONS_SLOT_SHOW_H */
