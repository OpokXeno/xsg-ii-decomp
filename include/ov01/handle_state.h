#ifndef INCLUDE_OV01_HANDLE_STATE_H
#define INCLUDE_OV01_HANDLE_STATE_H

/* Functions of src/ov01/handle_state.c that other files call. */

/* An SE voice id is the voice tag (the owner's handle) in its top byte, then the code. */
#define SE_VOICE_TAG_SHIFT 0x18

int handleStateGet(unsigned int handle);
int handleStateSet(int handle);
void seVoiceStartTagged(int tag, int id);

#endif /* INCLUDE_OV01_HANDLE_STATE_H */
