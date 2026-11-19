/*
 * OV01 original TU 2: 0x00a80368..0x00a804d8 (4 functions)
 */
#include "common.h"
#include "ov01/tu000.h"
#include "ov01/tu002.h"

/* Entries in the two-entry monsSlotTable. */
#define MONS_SLOT_COUNT 2

static MonsSlot monsSlotTable[MONS_SLOT_COUNT];

/* Clears both slots of monsSlotTable (ov01ClearTables calls it). */
void monsSlotClearAll(void)
{
    int i;

    for (i = 0; i < MONS_SLOT_COUNT; i++) {
        ov01SlotClear(&monsSlotTable[i]);
    }
}

/* Update sweep over the two slots (monsSlotRunUpdate). */
void monsSlotRunUpdate(void)
{
    int (*update)(MonsSlot *slot);
    MonsSlot *slot;
    int i;

    slot = monsSlotTable;
    for (i = 0; i < MONS_SLOT_COUNT; i++) {
        update = slot->update;
        if (update != 0 && !(slot->flags & OV01_SLOT_SKIP_UPDATE)) {
            update(slot);
        }
        slot++;
    }
}

/* Draw sweep over the two slots (monsSlotRunDraw). */
void monsSlotRunDraw(void)
{
    int (*draw)(MonsSlot *slot);
    MonsSlot *slot;
    int i;

    slot = monsSlotTable;
    for (i = 0; i < MONS_SLOT_COUNT; i++) {
        draw = slot->draw;
        if (draw != 0 && !(slot->flags & OV01_SLOT_SKIP_DRAW)) {
            draw(slot);
        }
        slot++;
    }
}

/* Claims the first free slot of monsSlotTable: returns it, or 0 when both slots are claimed. */
MonsSlot *monsSlotClaim(void)
{
    int i;

    for (i = 0; i < MONS_SLOT_COUNT; i++) {
        if (ov01SlotClaim(&monsSlotTable[i].flags)) {
            return &monsSlotTable[i];
        }
    }
    return 0;
}
