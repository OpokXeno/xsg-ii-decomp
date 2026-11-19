/*
 * OV01 original TU 69: 0x00aa8ea0..0x00aa9058 (4 functions)
 */
#include "common.h"

#include "ov01/tu069.h"

void *memset(void *dst, int value, unsigned int size);
void func_00138D78(int index);
/* The handle table (tu070.c reads the state word of each record through D_00AE0F20) and its bump cursor. */
static HandleSlot handleSlots[HANDLE_SLOT_COUNT];
static unsigned int handleBumpCursor;

/* Clears the handle table, runs func_00138D78 for slot 0 and for each of the six handles, and resets the bump cursor. */
void handleTableInit(void)
{
    int i;

    memset(handleSlots, 0, sizeof(handleSlots));
    func_00138D78(0);
    for (i = 0; i < HANDLE_SLOT_COUNT; i++) {
        func_00138D78(i + HANDLE_BASE);
    }
    handleBumpCursor = HANDLE_BUMP_START;
}

/* Allocates a slot: fromBump 0 takes a fixed-address slot, otherwise the slot gets `size` bytes from the bump cursor. */
int handleAlloc(int fromBump, unsigned int size)
{
    int i;
    int first;
    int end;
    unsigned int bumpEnd;
    unsigned int bumpStart;
    HandleSlot *slot;
    unsigned int *bumpCursor;

    if (fromBump == 0) {
        first = 0;
        end = HANDLE_FIXED_SLOT_COUNT;
    } else {
        first = HANDLE_FIXED_SLOT_COUNT;
        end = HANDLE_SLOT_COUNT;
    }
    i = first;
    if (i < end) {
        slot = &handleSlots[i];
        for (; i < end; i++, slot++) {
            if (slot->inUse == 0) {
                slot->inUse = 1;
                if (fromBump == 0) {
                    slot->address = HANDLE_FIXED_BASE + (i - first) * HANDLE_FIXED_STRIDE;
                } else {
                    bumpCursor = &handleBumpCursor;
                    bumpStart = *bumpCursor;
                    bumpEnd = bumpStart + size;
                    if (bumpEnd > HANDLE_FIXED_BASE) {
                        return -1;
                    }
                    slot->address = bumpStart;
                    *bumpCursor = bumpEnd;
                }
                return i + HANDLE_BASE;
            }
        }
    }
    return -1;
}

/* Returns the memory address of an in-use handle, 0 when the handle is free. */
int handleAddressGet(int handle)
{
    int index;

    index = handle - HANDLE_BASE;
    if (handleSlots[index].inUse == 0) {
        return 0;
    }
    return handleSlots[index].address;
}

/* Frees an in-use handle: clears its three words and calls func_00138D78. Returns 1, or 0 when it was free. */
int handleFree(int handle)
{
    int index;

    index = handle - HANDLE_BASE;
    if (handleSlots[index].inUse == 0) {
        return 0;
    }
    handleSlots[index].inUse = 0;
    handleSlots[index].address = 0;
    handleSlots[index].unmodeled_08 = 0;
    func_00138D78(handle);
    return 1;
}
