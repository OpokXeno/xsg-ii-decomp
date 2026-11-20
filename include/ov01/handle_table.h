#ifndef INCLUDE_OV01_HANDLE_TABLE_H
#define INCLUDE_OV01_HANDLE_TABLE_H

/* Functions and the handle table of src/ov01/handle_table.c that other files use.
 * A handle is slot index + HANDLE_BASE. The first HANDLE_FIXED_SLOT_COUNT slots get fixed addresses from
 * HANDLE_FIXED_BASE; the others take their memory from the bump cursor, which starts at HANDLE_BUMP_START
 * and may not pass HANDLE_FIXED_BASE. */
#define HANDLE_BASE 3
#define HANDLE_SLOT_COUNT 6
#define HANDLE_FIXED_SLOT_COUNT 3
#define HANDLE_FIXED_BASE 0x100000
#define HANDLE_FIXED_STRIDE 0x50000
#define HANDLE_BUMP_START 0x37800

/* One handle slot (12 bytes, HANDLE_RECORD_SIZE): its in-use flag, its memory address, and a third word that handleFree
 * clears (not named yet). handle_state.c reads the first word of each record as a state word. */
typedef struct HandleSlot {
    unsigned int inUse;
    unsigned int address;
    unsigned int unmodeled_08;
} HandleSlot;

#define HANDLE_RECORD_SIZE 0xC

/* Clears the handle table and resets the bump cursor. */
void handleTableInit(void);
/* Allocates a slot: fromBump 0 takes a fixed-address slot, otherwise the slot gets `size` bytes from the bump cursor. */
int handleAlloc(int fromBump, unsigned int size);
/* Returns the memory address of an in-use handle, 0 when the handle is free. */
int handleAddressGet(int handle);
/* Frees an in-use handle. Returns 1, or 0 when it was free. */
int handleFree(int handle);

#endif /* INCLUDE_OV01_HANDLE_TABLE_H */
