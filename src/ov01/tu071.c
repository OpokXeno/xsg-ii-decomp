/*
 * OV01 original TU 71: 0x00aa90c8..0x00aa9340 (7 functions)
 */
#include "common.h"
#include "main/tu022.h"
#include "main/tu063.h"
#include "ov01/tu071.h"

INCLUDE_ASM("asm/nonmatchings/ov01/tu071", seObjectVoiceStart);

#include "shared.h"

void seObjectVoiceStart(void *, int, int);

/* Starts the object's voice for `code` at the default SE volume (volume -1). */
void seObjectVoiceStartDefault(void *object, int code)
{
    seObjectVoiceStart(object, code, -1);
}

void adxSetSlotFile(int slot, int handle);

/* Sets the file of stream slot `slot` to `handle`. */
void streamSlotSetFile(int slot, int handle)
{
    adxSetSlotFile(slot, handle);
}


/* Stops a stream slot (bgmStop passes 0). The original forwards its argument to soundSlotStop. */
void streamSlotStop(int index)
{
    soundSlotStop(index);
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu071", bgmPlayTrack);

INCLUDE_ASM("asm/nonmatchings/ov01/tu071", func_00AA91B8);

/* A table key: the record at owner->key whose table_id selects a table or path. */
struct TableKey {
    char unmodeled_000[0x62];
    s16 table_id;
};

struct TableKeyOwner {
    char unmodeled_000[0x144];
    struct TableKey *key;
};

int func_00A835D0(s16);

/* Plays the stream that the owner's table key maps to, on slot 1 at the stream level. */
void streamPlayByKey(struct TableKeyOwner *owner)
{
    int handle;

    handle = func_00A835D0(owner->key->table_id);
    if (handle != 0) {
        soundSlotReopen(1);
        func_001EDC60(1);
        streamSlotSetFile(1, handle);
    }
}
