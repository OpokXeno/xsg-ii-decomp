/*
 * OV01 original TU 0: 0x00a80000..0x00a801e0 (9 functions)
 */
#include "common.h"
#include "ov01/tu000.h"

/* Entries in the two-entry callback slot table. */
#define OV01_CALLBACK_SLOT_COUNT 2

typedef struct CallbackSlot CallbackSlot;

typedef void (*SlotCallback)(CallbackSlot *slot);

/*
 * One 0x60-byte entry of the two-entry table at ov01CallbackSlotTable. Bit 0x1 of flags
 * is the claim bit (ov01SlotClaim). update runs while flags & 2 is clear and
 * is called from the update sweep ov01CallbackSlotsRunUpdate; draw runs while flags & 4 is
 * clear. ov01CallbackSlotsRunDraw (draw sweep) has no caller inside this overlay.
 */
struct CallbackSlot {
    int flags;
    SlotCallback update;
    SlotCallback draw;
    unsigned char unmodeled_0C[0x54];
};

static CallbackSlot ov01CallbackSlotTable[OV01_CALLBACK_SLOT_COUNT];

void ov01SlotClear(void *slot) {
    CallbackSlot *callback = slot;

    callback->flags = 0;
    callback->update = 0;
    callback->draw = 0;
}

/* Claims a slot whose claim bit is clear: sets flags to 1 and returns 1. */
int ov01SlotClaim(int *flags) {
    if (!(*flags & OV01_SLOT_CLAIM)) {
        *flags = OV01_SLOT_CLAIM;
        return 1;
    }
    return 0;
}

/* Releases one slot: clears the claim bit and both callbacks. Same body as ov01SlotClear. */
void ov01SlotRelease(CallbackSlot *slot) {
    slot->flags = 0;
    slot->update = 0;
    slot->draw = 0;
}

/* Returns nonzero while the claim bit 0x1 of *flags is set. */
int ov01SlotInUse(int *flags) {
    if ((*flags & OV01_SLOT_CLAIM) != 0) {
        return 1;
    }
    return 0;
}

void ov01CallbackSlotsClear(void) {
    int i;

    for (i = 0; i < OV01_CALLBACK_SLOT_COUNT; i++) {
        ov01SlotClear(&ov01CallbackSlotTable[i]);
    }
}

/* Update sweep over the two-entry table: runs each slot's update unless flags & 2. */
void ov01CallbackSlotsRunUpdate(void) {
    SlotCallback update;
    int i;

    for (i = 0; i < OV01_CALLBACK_SLOT_COUNT; i++) {
        update = ov01CallbackSlotTable[i].update;
        if (update != 0 && !(ov01CallbackSlotTable[i].flags & OV01_SLOT_SKIP_UPDATE)) {
            update(&ov01CallbackSlotTable[i]);
        }
    }
}

/* Draw sweep over the two-entry table: runs each slot's draw unless flags & 4. */
void ov01CallbackSlotsRunDraw(void) {
    SlotCallback draw;
    int i;

    for (i = 0; i < OV01_CALLBACK_SLOT_COUNT; i++) {
        draw = ov01CallbackSlotTable[i].draw;
        if (draw != 0 && !(ov01CallbackSlotTable[i].flags & OV01_SLOT_SKIP_DRAW)) {
            draw(&ov01CallbackSlotTable[i]);
        }
    }
}

/* Claims the first free slot of the table: returns it, or 0 when both slots are claimed. */
void *ov01CallbackSlotClaim(void) {
    int i;

    for (i = 0; i < OV01_CALLBACK_SLOT_COUNT; i++) {
        if (ov01SlotClaim(&ov01CallbackSlotTable[i].flags)) {
            return &ov01CallbackSlotTable[i];
        }
    }
    return 0;
}

void ov01CallbackSlotRelease(CallbackSlot *slot) {
    ov01SlotRelease(slot);
}
