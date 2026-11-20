#include "common.h"
#include "ov01/ov01_slot.h"
#include "ov01/data_obj_table.h"

/*
 * Two data object slots of 0x1e0 bytes at dataObjTable (struct DataObject, data_obj_table.h), the table that
 * objEnCurCreate (objEnCurCreate) allocates through dataObjAlloc. The slot flags
 * word gates the two callbacks: update (objCur, +0x4) runs unless flags
 * & 0x2, draw (objCurDraw, +0x8) unless flags & 0x4. Each callback is called
 * with the slot it belongs to.
 */

/* Entries in the two-entry dataObjTable. */
#define DATA_OBJ_SLOT_COUNT 2

static struct DataObject dataObjTable[DATA_OBJ_SLOT_COUNT];

/* Releases the unit side slot of a unit (unitRemove calls it; unit side slots). */
void unitSlotRelease(struct BattleUnit *unit) {
    ov01SlotRelease(unit);
}

/* Clears both data object slots (ov01ClearTables calls it). */
void dataObjResetAll(void) {
    int i;

    for (i = 0; i < DATA_OBJ_SLOT_COUNT; i++) {
        ov01SlotClear(&dataObjTable[i]);
    }
}

/* Update sweep over the data object slots (called by the update sweep ov01UpdateAll). */
void dataObjUpdateAll(void) {
    struct DataObject *slot = dataObjTable;
    int i;

    for (i = 0; i < DATA_OBJ_SLOT_COUNT; i++) {
        if (slot->update != 0 && !(slot->slotFlags & OV01_SLOT_SKIP_UPDATE)) {
            slot->update(slot);
        }
        slot++;
    }
}

/* Draw sweep over the data object slots (called from the draw frame ov01DrawAll). */
void dataObjDrawAll(void) {
    struct DataObject *slot = dataObjTable;
    int i;

    for (i = 0; i < DATA_OBJ_SLOT_COUNT; i++) {
        if (slot->draw != 0 && !(slot->slotFlags & OV01_SLOT_SKIP_DRAW)) {
            slot->draw(slot);
        }
        slot++;
    }
}

/* Claims the first free data object slot: returns it, or 0 when both slots are claimed (objEnCurCreate uses it). */
struct DataObject *dataObjAlloc(void) {
    int i;

    for (i = 0; i < DATA_OBJ_SLOT_COUNT; i++) {
        if (ov01SlotClaim(&dataObjTable[i].slotFlags)) {
            return &dataObjTable[i];
        }
    }
    return 0;
}
