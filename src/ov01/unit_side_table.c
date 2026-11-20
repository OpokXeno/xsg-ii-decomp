#include "common.h"
#include "ov01/ov01_slot.h"
#include "ov01/unit_side_table.h"
#include "ov01/unit_cmd.h"

/* Entries in the nine-entry unitSideTable. */
#define UNIT_SIDE_COUNT 9

/*
 * One 0x1F0-byte slot of the nine-entry table unitSideTable: a unit object (BattleUnit, 0x1F0 bytes). Bit 0x1 of the slot flags is the claim bit (unitSideClaim, called by unitCreateSide); unitCreateSide
 * stores unitUpdate and the draw callback in it, and unitRemove releases the slot through
 * unitSlotRelease (data_obj_table.c).
 * Update runs unless flags & 2 (update sweep unitSideRunUpdate), draw runs unless flags & 4 (draw sweep unitSideRunDraw).
 */
typedef struct UnitSideSlot {
    BattleUnit object;
} UnitSideSlot;

static UnitSideSlot unitSideTable[UNIT_SIDE_COUNT];

/*
 * Releases one MonsSlot record (monsSlotRecordRelease): the only caller is monsSlotRelease (mons_slot.c,
 * monsSlotRelease), which calls it before it releases the actor. Same body as the other release wrappers.
 */
void monsSlotRecordRelease(void)
{
    ov01SlotRelease();
}

/* Clears all nine unit slots (unitSideClearAll), called by ov01ClearTables. */
void unitSideClearAll(void)
{
    int i;

    for (i = 0; i < UNIT_SIDE_COUNT; i++) {
        ov01SlotClear(&unitSideTable[i].object);
    }
}

/* Update sweep over the nine slots (unitSideRunUpdate): runs each slot's update unless flags & 2. */
void unitSideRunUpdate(void)
{
    void (*update)(BattleUnit *);
    UnitSideSlot *slot;
    int i;

    slot = unitSideTable;
    for (i = 0; i < UNIT_SIDE_COUNT; i++) {
        update = slot->object.update;
        if (update != 0 && !(slot->object.slotFlags & OV01_SLOT_SKIP_UPDATE)) {
            update(&slot->object);
        }
        slot++;
    }
}

/* Draw sweep over the nine slots (unitSideRunDraw): runs each slot's draw unless flags & 4. */
void unitSideRunDraw(void)
{
    void (*draw)(BattleUnit *);
    UnitSideSlot *slot;
    int i;

    slot = unitSideTable;
    for (i = 0; i < UNIT_SIDE_COUNT; i++) {
        draw = slot->object.draw;
        if (draw != 0 && !(slot->object.slotFlags & OV01_SLOT_SKIP_DRAW)) {
            draw(&slot->object);
        }
        slot++;
    }
}

/* Claims the first free unit slot: returns it, or 0 when all nine slots are claimed (unitCreateSide uses it). */
BattleUnit *unitSideClaim(void)
{
    int i;

    for (i = 0; i < UNIT_SIDE_COUNT; i++) {
        if (ov01SlotClaim(&unitSideTable[i].object.slotFlags)) {
            return &unitSideTable[i].object;
        }
    }
    return 0;
}
