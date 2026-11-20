#include "common.h"
#include "ov01/mons_slot_run.h"
#include "ov01/unit_side_table.h"
#include "main/xenoActor.h"
#include "ov01/unit_cmd.h"
#include "ov01/mons_slot.h"

/*
 * monsSlotNew zeroes the actor's position through a float array, an alias-0 access that keeps the stores after the
 * slot's pointer store. Making XenoActor.position a union would give main's code alias set 0 too and change
 * uwamonoPlayerUpdate and Get_Off_Ladder, so this one site reaches the position through a view that starts like the
 * actor (struct XenoActor of main/xenoActor.h, +0x10).
 */
typedef struct MonsActorPositionView {
    unsigned char unmodeled_00[0x10];
    Vector4Elements position; /* +0x10 */
} MonsActorPositionView;

void *memset(void *, int, unsigned int);
#include "shared.h"

/* Entries of monsActiveSlots, the table of record pointers that monsSlotInsert fills. */
#define MONS_ACTIVE_SLOT_COUNT 2
/*
 * Two-entry table of MonsSlot records and its count
 *. monsSlotInsert fills a free entry and bumps the
 * count, monsSlotRemove clears one and drops the count.
 */
static struct MonsSlot *monsActiveSlots[MONS_ACTIVE_SLOT_COUNT];
static int monsActiveCount;

struct MonsSlot *monsSlotNew(void);
void monsSlotEmptyUpdate(void);
int monsSlotInsert(struct MonsSlot *record);
int monsSlotRemove(struct MonsSlot *slot);

/*
 * monsSlotReset: empties the two-entry table and
 * sets its count to 0. Called by ov01SceneInit.
 */
void monsSlotReset(void) {
    memset(monsActiveSlots, 0, MONS_ACTIVE_SLOT_COUNT * sizeof(struct MonsSlot *));
    monsActiveCount = 0;
}


/*
 * monsSlotCreate: makes a new MonsSlot with
 * monsSlotNew and inserts it with monsSlotInsert. Returns the record, or 0
 * when monsSlotNew fails. Callers: ov01StreamOpen, ov01StepOpenHandles.
 */
struct MonsSlot *monsSlotCreate(void) {
    struct MonsSlot *slot;

    slot = monsSlotNew();
    if (slot != 0) {
        monsSlotInsert(slot);
    }
    return slot;
}

/*
 * monsSlotNew: allocates a MonsSlot with the empty
 * callback monsSlotEmptyUpdate, creates an actor with XenoActorAlloc and stores it
 * at +0x90, then hides it (bit 0x4 of +0xBC) and puts it at the origin.
 */
struct MonsSlot *monsSlotNew(void) {
    struct XenoActor *actor;
    struct MonsSlot *slot;
    unsigned int stateFlags;

    slot = monsSlotClaim();
    if (slot == 0) {
        return 0;
    }
    slot->callback = monsSlotEmptyUpdate;
    slot->initWord08 = 0;
    slot->initWord0C = 0;
    actor = XenoActorAlloc();
    slot->actor = actor;
    stateFlags = actor->drawFlags;
    /* The float-array view of the position keeps the zeroing stores after the flags load. */
    ((MonsActorPositionView *) actor)->position.component[0] = ((MonsActorPositionView *) actor)->position.component[1] =
        ((MonsActorPositionView *) actor)->position.component[2] = 0.0F;
    actor->drawFlags = stateFlags | ACTOR_STATE_HIDDEN;
    return slot;
}

/*
 * monsSlotRelease: releases the actor (XenoActorRelease),
 * removes the record from the table (monsSlotRemove) and calls monsSlotReleaseRecord
 * first. Called by eventPairEnd.
 */
void monsSlotRelease(struct MonsSlot *slot) {
    monsSlotReleaseRecord();
    XenoActorRelease(slot->actor);
    monsSlotRemove(slot);
}

/*
 * The stream record (struct DataStreamEntry, ov01/data_file_load.h) as monsSlotCopyParams reads it: the buffers of
 * its first map's pxy and txy files as words, since XenoActor.rsrc is int (the same view as AttachRecord).
 */
struct StreamBufferWords {
    unsigned char unmodeled_00[0x10];
    int model; /* +0x10: the pxy buffer, XenoActor.rsrc[XAct_Rsrc_MDL] */
    unsigned char unmodeled_14[0x8];
    int texture; /* +0x1C: the txy buffer, XenoActor.rsrc[XAct_Rsrc_TEX] */
};

/*
 * monsSlotCopyParams: copies two words of the
 * stream record (+0x10 and +0x1C) into the record's actor at +0x5EC and
 * +0x5F0. Returns 1, or 0 when either argument is null. Caller: ov01StepBindAndShow
 * (scene_load.c).
 */
int monsSlotCopyParams(struct MonsSlot *slot, struct DataStreamEntry *stream) {
    struct StreamBufferWords *words = (struct StreamBufferWords *)stream;
    struct XenoActor *actor;

    if (slot == 0 || stream == 0) {
        return 0;
    }
    actor = slot->actor;
    actor->rsrc[XAct_Rsrc_MDL] = words->model;
    actor->rsrc[XAct_Rsrc_TEX] = words->texture;
    return 1;
}

void monsSlotEmptyUpdate(void) {
}

/*
 * monsActiveSlotsGet: hands back the two-entry table
 * and returns its length, 2. Callers: flowSpecProc, unitTransformBegin,
 * scenarioMtd, eventPairBegin, eventPairEnd, unitBodySetFog.
 */
int monsActiveSlotsGet(struct MonsSlot ***table) {
    *table = monsActiveSlots;
    return MONS_ACTIVE_SLOT_COUNT;
}

/*
 * monsSlotInsert: stores the record in the first
 * free entry of the table and increments the count. Returns 1 when stored,
 * 0 when the table is full.
 */
int monsSlotInsert(struct MonsSlot *record) {
    int entry;

    for (entry = 0; entry < MONS_ACTIVE_SLOT_COUNT; entry++) {
        if (monsActiveSlots[entry] == 0) {
            monsActiveSlots[entry] = record;
            monsActiveCount++;
            return 1;
        }
    }
    return 0;
}

/*
 * monsSlotRemove: clears the entry that holds the
 * record and decrements the count. Returns 1 when found, 0 otherwise.
 */
int monsSlotRemove(struct MonsSlot *slot) {
    int entry;

    for (entry = 0; entry < MONS_ACTIVE_SLOT_COUNT; entry++) {
        if (monsActiveSlots[entry] == slot) {
            monsActiveSlots[entry] = 0;
            monsActiveCount--;
            return 1;
        }
    }
    return 0;
}
