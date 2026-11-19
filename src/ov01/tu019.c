/*
 * OV01 original TU 19: 0x00a88a50..0x00a88c60 (9 functions)
 */
#include "common.h"
#include "ov01/tu002.h"
#include "ov01/tu003.h"
#include "main/xenoActor.h"
#include "ov01/tu010.h"
#include "ov01/tu019.h"

/*
 * The actor as monsSlotNew and monsSlotCopyParams touch it (the full record is struct XenoActor of main/tu051.h, which
 * this file does not include): the position zeroed through a float array, an alias-0 access that keeps the stores
 * after the slot's pointer store. A union around XenoActor.position would give main's code alias set 0 too and
 * change uwamonoPlayerUpdate and uwamonoParabolaMove. resource = rsrc[XAct_Rsrc_MDL], rsrc[XAct_Rsrc_TEX].
 */
struct XenoActor {
    unsigned char unmodeled_00[0x10];
    union {
        Vector4 position;
        float coords[4]; /* +0x10 */
    };
    unsigned char unmodeled_20[0x9C];
    unsigned int drawFlags; /* +0xBC: bit 0x4 hides the actor */
    unsigned char unmodeled_c0[0x52C];
    unsigned int resource[2]; /* +0x5EC */
};

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
    actor->coords[0] = actor->coords[1] =
        actor->coords[2] = 0.0F;
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

/* The second argument of monsSlotCopyParams: two words at +0x10 and +0x1C. */
struct MonsParams {
    unsigned char unmodeled_00[0x10];
    unsigned int paramA; /* +0x10 */
    unsigned char unmodeled_14[0x8];
    unsigned int paramB; /* +0x1C */
};

/*
 * monsSlotCopyParams: copies two words of the
 * second argument (+0x10 and +0x1C) into the record's actor at +0x5EC and
 * +0x5F0. Returns 1, or 0 when either argument is null. Caller: ov01StepBindAndShow
 * (tu038).
 */
int monsSlotCopyParams(struct MonsSlot *slot, struct MonsParams *params) {
    struct XenoActor *actor;

    if (slot == 0 || params == 0) {
        return 0;
    }
    actor = slot->actor;
    actor->resource[0] = params->paramA;
    actor->resource[1] = params->paramB;
    return 1;
}

void monsSlotEmptyUpdate(void) {
}

/*
 * monsActiveSlotsGet: hands back the two-entry table
 * and returns its length, 2. Callers: attackSpecialStep, unitTransformBegin,
 * func_00A95810, eventPairBegin, eventPairEnd, unitBodySetPosDir.
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
