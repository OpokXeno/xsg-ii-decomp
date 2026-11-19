#include "common.h"
#include "shared.h"
#include "main/tu051.h"
#include "main/xenoActor.h"
#include "ov01/tu005.h"
#include "ov01/tu010.h"
#include "ov01/tu012.h"
#include "ov01/tu047.h"

typedef struct XglTaskPrefix XglTaskPrefix;

typedef struct XglTaskScheduler XglTaskScheduler;

/*
 * Only the link/callback prefix is evidenced by this allocation.  The
 * complete task records are allocated with a 0x80-byte stride, and callers
 * write task-specific payload beyond this 0x10-byte prefix.  This declaration
 * must not be read as the complete task layout or allocation size.
 */
struct XglTaskPrefix {
    XglTaskScheduler *scheduler;
    XglTaskPrefix *next;
    XglTaskPrefix *previous;
    int (*callback)(XglTaskPrefix *task);
};

struct XglTaskScheduler {
    XglTaskPrefix *free_tasks;
    XglTaskPrefix *active_head;
    XglTaskPrefix *active_tail;
    XglTaskPrefix *next_to_visit;
};

struct ObjectTask {
    XglTaskPrefix task; /* the scheduler header xglTaskEntryNext maintains. */
    void *work; /* objRemove passes this slot to objWorkFree. */
};

extern void *memset(void *destination, int value, unsigned int count);
extern XglTaskPrefix *xglTaskEntryNext(XglTaskScheduler *scheduler,
                                int (*callback)(XglTaskPrefix *task),
                                XglTaskPrefix *entry);

/*
 * Partial view of the unit record beyond the published ObjectTask prefix
 * (task+work, ending at +0x14, include/shared.h): unitSetInterpTime and
 * unitMotGet both follow a pointer immediately after that prefix, and
 * unitWpnMotSet reads a 4-entry pointer array starting eight bytes further.
 * Both point at actors (struct XenoActor): unitWpnMotSet forwards the entry it reads to
 * ACT_animGetData/ACT_setMotion. No other byte of this wider record is
 * claimed, and the relationship between this pointer and ObjectTask.work is
 * not evidenced here.
 */
typedef struct UnitRecord UnitRecord;

struct UnitRecord {
    ObjectTask task;                   /* +0x00..+0x13: the ObjectTask
                                         * prefix, opaque here */
    struct XenoActor *motionActor;                /* +0x14 */
    unsigned char unmodeled_018[0x1c - 0x18]; /* +0x18..+0x1b */
    struct XenoActor *wpnActor[4];                /* +0x1c */
    struct XenoActor *subActor;                   /* +0x2c: unitRemoveActor disposes it
                                         * first when it is set */
};

/* The same eight unit pointers have full-record and scheduler-prefix views. */
typedef union UnitTableStorage {
    UnitRecord *records[8];
    ObjectTask *tasks[8];
} UnitTableStorage;

/* The unit table (players from slot 0, enemies from slot 3) and the live counts; tu017.c reads them too. */
UnitTableStorage unitTbl;
int unitPlNum;
int unitEnNum;

void unitInit(void)
{
    memset(&unitTbl, 0, sizeof(unitTbl));
    unitPlNum = 0;
    unitEnNum = 0;
}

Ov01Object *unitCreateSide(int kind, int side);
extern void unitTblSet(int side, Ov01Object *unit);

/*
 * Player unit creation: unitCreateSide with side 0, then the unit goes into
 * player slot 0 of the table (unitTblSet(0, unit)). Returns the unit or 0.
 */
Ov01Object *unitPlCreate(int id)
{
    Ov01Object *unit;

    unit = unitCreateSide(id, 0);
    if (unit != 0) {
        unitTblSet(0, unit);
    }
    return unit;
}

/*
 * Enemy unit creation: unitCreateSide with side 1, a 0x60 flag set in the
 * unit's word at +0xC, then unitTblSet(1, unit). Script-facing unitCreate
 * (ov01/tu047) reaches it. Returns the unit or 0.
 */
Ov01Object *unitEnCreate(int id)
{
    Ov01Object *unit;

    unit = unitCreateSide(id, 1);
    unit->flags |= 0x60;
    if (unit != 0) {
        unitTblSet(1, unit);
    }
    return unit;
}

/*
 * Unit creation core for one side (0 player, 1 enemy): clears the word at
 * +0xC, runs two init helpers on +0x10 and +0x150, stores the fixed callbacks
 * unitUpdate and func_00A84880 at +4 and +8, and creates the XenoActor root
 * kept at +0x1D0. Returns the unit or 0.
 */
extern Ov01Object *unitSideClaim(int kind, int side);
extern u32 battleEntryFindByKey(int kind);
extern StatBlock *battleEntryAcquire(Ov01Object *unit, int kind);
extern void battleEntryActivate(Ov01Object *unit, int kind);
extern void func_00A84880(Ov01Object *object);
extern void XenoActorSetChannelFlag80(struct XenoActor *actor);


Ov01Object *unitCreateSide(int kind, int side)
{
    Ov01Object *unit;
    struct XenoActor *actor;
    struct XenoActor *root;
    u32 found;
    int notFound;
    StatBlock *battleEntry;
    DoublewordVector4 *vecA;
    DoublewordVector4 *vecB;
    int i;
    unsigned short slotFlag;
    unit = unitSideClaim(kind, side);
    if (unit == 0) {
        return 0;
    }
    unit->flags = 0;
    dataLookupReset(&unit->commandQueue);
    /* The pose block is the matrix block (ov01/tu022 passes it the same way). */
    ov01MatrixBlockInit((struct MatrixBlock *) &unit->pose);
    unit->update = unitUpdate;
    unit->draw = func_00A84880;
    found = battleEntryFindByKey(kind);
    notFound = found < 1;
    battleEntry = battleEntryAcquire(unit, kind);
    unit->stats = battleEntry;
    if (battleEntry == 0) {
        return 0;
    }
    if (notFound == 1) {
        battleEntryActivate(unit, kind);
    }
    root = XenoActorAlloc();
    unit->unit = root;
    actor = root;
    if (kind < 141 || kind > 143) {
        actor->drawFlags = 0x10001;
    }
    actor->alpha = 0.0f;
    if (kind != 125) {
        actor->drawMode = 2;
        actor->drawParam = 0.5f;
    }
    XenoActorSetChannelFlag80(actor);
    actor->lightBlocks = *lightBlockGet();
    runtimeVecPtrGet(&vecA, &vecB);
    root->runtimeVecA = *vecA;
    root->runtimeVecB = *vecB;
    for (i = 0; i < 3; i++) {
        if (unit->stats->pairFirst[i] != 0) {
            actor = XenoActorAlloc();
            slotFlag = unit->stats->pairSecond[i];
            unit->children[i] = actor;
            actor->drawFlags = 0x10001;
            actor->alpha = 0.0f;
            if ((slotFlag & 0x4000) != 0) {
                actor->drawFlags = 0x10005;
            }
            actor->lightBlocks = *lightBlockGet();
            do {
                actor->runtimeVecA = *vecA;
                actor->runtimeVecB = *vecB;
            } while (0);
        } else {
            unit->children[i] = 0;
        }
    }
    return unit;
}
