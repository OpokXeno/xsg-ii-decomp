/*
 * OV01 original TU 3: 0x00a804d8..0x00a80660 (5 functions)
 */
#include "common.h"
#include "ov01/tu000.h"
#include "ov01/tu003.h"

/* Entries in the nine-entry unitSideTable. */
#define UNIT_SIDE_COUNT 9

/*
 * One 0x1F0-byte slot of the nine-entry table unitSideTable (unitSideTable). Bit 0x1 of flags is the claim bit
 * (unitSideClaim, called by unitCreateSide); the claim stores unitUpdate at +0x4 and the draw callback at +0x8,
 * and unitRemove releases the slot through unitRelease (tu004). Update runs unless flags & 2 (update sweep
 * unitRunUpdate), draw runs unless flags & 4 (draw sweep unitRunDraw).
 * Named for its table: tu010 and tu017 define a different struct UnitRecord, and tu033 a UnitSlot.
 */
typedef struct UnitSideSlot {
    int flags;
    void (*update)(struct UnitSideSlot *);
    void (*draw)(struct UnitSideSlot *);
    unsigned char unmodeled_0C[0x1F0 - 0x0C];
} UnitSideSlot;

static UnitSideSlot unitSideTable[UNIT_SIDE_COUNT];

/*
 * Releases one MonsSlot record (monsSlotReleaseRecord): the only caller is monsSlotRelease (tu019, monsSlotRelease),
 * which calls it before it releases the actor. Same body as the other release wrappers.
 */
void monsSlotReleaseRecord(void)
{
    ov01SlotRelease();
}

/* Clears all nine unit slots (unitClearAll), called by ov01ClearTables. */
void unitClearAll(void)
{
    int i;

    for (i = 0; i < UNIT_SIDE_COUNT; i++) {
        ov01SlotClear(&unitSideTable[i]);
    }
}

/* Update sweep over the nine slots (unitRunUpdate): runs each slot's update unless flags & 2. */
void unitRunUpdate(void)
{
    void (*update)(UnitSideSlot *);
    UnitSideSlot *slot;
    int i;

    slot = unitSideTable;
    for (i = 0; i < UNIT_SIDE_COUNT; i++) {
        update = slot->update;
        if (update != 0 && !(slot->flags & OV01_SLOT_SKIP_UPDATE)) {
            update(slot);
        }
        slot++;
    }
}

/* Draw sweep over the nine slots (unitRunDraw): runs each slot's draw unless flags & 4. */
void unitRunDraw(void)
{
    void (*draw)(UnitSideSlot *);
    UnitSideSlot *slot;
    int i;

    slot = unitSideTable;
    for (i = 0; i < UNIT_SIDE_COUNT; i++) {
        draw = slot->draw;
        if (draw != 0 && !(slot->flags & OV01_SLOT_SKIP_DRAW)) {
            draw(slot);
        }
        slot++;
    }
}

/* Claims the first free unit slot: returns it, or 0 when all nine slots are claimed (unitCreateSide uses it). */
UnitSideSlot *unitSideClaim(void)
{
    int i;

    for (i = 0; i < UNIT_SIDE_COUNT; i++) {
        if (ov01SlotClaim(&unitSideTable[i].flags)) {
            return &unitSideTable[i];
        }
    }
    return 0;
}
