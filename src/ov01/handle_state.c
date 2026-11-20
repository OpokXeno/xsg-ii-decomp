#include "common.h"
#include "ov01/handle_table.h"
#include "ov01/handle_state.h"
#include "main/se_bank_load.h"

/*
 * Partial layout of one HandleSlot record (handle_table.c's table): the state word is its
 * first word (HandleSlot.inUse, typed int here); the other 8 bytes are not used by these functions.
 */
struct HandleSlotState {
    int state;
    unsigned char unmodeled_04[8];
};

/* Handle state words, indexed by handle - HANDLE_BASE. */
extern struct HandleSlotState handleSlotStateTable[];

/* Sets the state word of handle to 1 and returns the byte offset of its record. */
int handleStateSet(int handle) {
    int handleIndex;
    int slotOffset;

    handleIndex = handle - HANDLE_BASE;
    slotOffset = handleIndex * HANDLE_RECORD_SIZE;
    handleSlotStateTable[handleIndex].state = 1;
    return slotOffset;
}

/* Returns the state word of handle. */
int handleStateGet(unsigned int handle) {
    return handleSlotStateTable[handle - HANDLE_BASE].state;
}


/* Starts the SE voice `code` tagged with voiceTag (the owner's handle, in the top byte). */
void seVoiceStartTagged(int voiceTag, int code) {
    seVoiceStartDefault((voiceTag << SE_VOICE_TAG_SHIFT) + code);
}
