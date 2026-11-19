/*
 * OV01 original TU 38: 0x00a9a4b0..0x00a9af68 (23 functions)
 *
 * Battle step chain (each step sets its successor with ov01StepSetNext; a
 * wait step pushes ov01StepAwaitLoads as the fallback): ov01StepChainStart starts it;
 * ov01StepSceneScript -> ov01StepInit -> ov01StepStartBlockLoad -> ov01StepAwaitBlockLoad ->
 * ov01StepOpenHandles -> ov01StepBindAndShow -> ov01StepLoadPartyIds (party ids) -> ov01StepSpawnPartyMember
 * (spawn party member) -> ov01StepLoadPartyMember -> ov01StepAwaitPartyLoad -> ov01StepNextPartyMember
 * (next member) -> ov01StepStartEnemies -> ov01StepSpawnEnemy (spawn enemy) -> ov01StepLoadEnemy
 * -> ov01StepAwaitEnemyLoad -> ov01StepNextEnemy (next enemy or battle start) -> ov01StepBattle
 * (tu039).
 */
#include "common.h"
#include "main/gifState.h"
#include "main/sef.h"
#include "ov01/tu005.h"
#include "ov01/tu008.h"
#include "ov01/tu009.h"
#include "ov01/tu012.h"
#include "ov01/tu022.h"
#include "ov01/tu036.h"
#include "ov01/tu037.h"
#include "ov01/tu038.h"
#include "ov01/tu039.h"
#include "ov01/tu040.h"
#include "ov01/tu041.h"
#include "ov01/tu044.h"
#include "ov01/tu046.h"
#include "ov01/tu047.h"
#include "ov01/tu049.h"
#include "ov01/tu050.h"
#include "ov01/tu051.h"
#include "ov01/tu052.h"
#include "ov01/tu055.h"
#include "ov01/tu056.h"
#include "ov01/tu058.h"
#include "ov01/tu060.h"
#include "ov01/tu061.h"
#include "ov01/tu062.h"
#include "ov01/tu070.h"
#include "ov01/tu071.h"

/*
 * Spawn record of the party list ov01PartyList and the enemy list ov01EnemyList.
 * The party list holds three records; monsTblGet returns the enemy list.
 * Only these five words are touched by this TU.
 */
typedef struct UnitSpawnRecord {
    int id;
    int spawnArgA;
    int spawnArgB;
    int spawnArgC;
    int spawnArgD;
} UnitSpawnRecord;

/* Handles of the battle setup: the stream buffer and the sound/monster slot handle (ov01StepOpenHandles). */
static int ov01StreamBuffer[1] = { 0 };
static int D_00AB5CD4 = 0;
/* The unit being spawned and its data slot (dataUnitLoad), kept between the spawn, load and await steps. */
static struct DataSlot *ov01CurSlot = 0;
static Ov01Object *ov01CurUnit = 0;
/* Party and enemy spawn cursors and their record lists (ov01PartyListGet, monsTblGet). */
static int ov01PartyIndex = 0;
static int ov01EnemyIndex = 0;
static UnitSpawnRecord *ov01EnemyList = 0;
static UnitSpawnRecord *ov01PartyList = 0;
/* Load thread stack (ov01LoadBlockStart, ov01LoadRequest), load-done flag and owner of the pending load. */
static unsigned char D_00AC41A0[0x1000];
static int ov01LoadDone;
static struct DataSlot *ov01LoadOwner;
void ExitDeleteThread(void);
void dataCdArenaOpen(void);
void ov01StepSetNext(void *item, void (*next_step)(int context));
void ov01LoadBlockStart(void);
void ov01StepLoadPartyMember(int context);
void ov01StepNextEnemy(int context);
void monsSlotCopyParams(int first, int second);
/* Both views stay local: tu038 keeps its record handles as int (D_00AB5CD4), see tu019.h and tu020.h. */
int monsSlotCreate(void);
void monsSlotShow(void *slot);
int monsTblValueGet(void);
void menuStatRecordsCreate(void);
unsigned char *ptGetStatusBlock(void);
UnitSpawnRecord *ov01PartyListGet(void);
Ov01Object *unitPlCreate(int id);
Ov01Object *unitEnCreate(int id);
void unitBindActors(Ov01Object *object, struct DataSlot *slot);
void ov01StepStartBlockLoad(int context);
void ov01StepAwaitBlockLoad(int context);
void ov01StepOpenHandles(int context);
void ov01StepBindAndShow(int context);
void ov01StepLoadPartyIds(int context);
void ov01StepSpawnPartyMember(int context);
void ov01StepNextPartyMember(int context);
void ov01StepStartEnemies(int context);
void ov01StepSpawnEnemy(int context);
void ov01StepLoadEnemy(int context);
void ov01StepAwaitEnemyLoad(int context);
void ov01StepAwaitLoads(int context);

/*
 * The EE kernel's ThreadParam: ov01LoadBlockStart and ov01LoadRequest fill one on the stack for CreateThread
 * (syscall 0x20, main 0x00120320) and start the thread with StartThread (syscall 0x22). The entry ends with
 * ExitDeleteThread (syscall 0x24).
 */
typedef struct ThreadParam {
    int status;
    void (*entry)(void);      /* +0x04 */
    void *stack;              /* +0x08 */
    int stackSize;            /* +0x0C */
    void *gpReg;              /* +0x10 */
    int initPriority;         /* +0x14 */
    int currentPriority;      /* +0x18 */
    unsigned int attr;        /* +0x1C */
    unsigned int option;      /* +0x20 */
    int waitType;             /* +0x24 */
    int waitId;               /* +0x28 */
    int wakeupCount;          /* +0x2C */
} ThreadParam;

/* Stack size and priority of the load threads; their stack is D_00AC41A0, their gp the main gp value D_6A2DF0. */
#define OV01_LOAD_THREAD_STACK_SIZE 0x1000
#define OV01_LOAD_THREAD_PRIORITY 8

extern unsigned char D_6A2DF0[];
int CreateThread(ThreadParam *param);
int StartThread(int thread, void *arg);
void ov01LoadComplete(void);
void ov01LoadCompleteForOwner(void);
int handleStateGet(unsigned int handle);
extern int ov01SceneDone;
struct SaveStack *ov01StackNew(void);
void runtimeInit(void);
void sbStartImage(struct SbImageHeader *image);
void ov01StepSceneScript(int context);
void ov01StepInit(int context);

/* Load thread entry: releases the battle sound effects (dataBattleSeRelease), marks the load done (ov01LoadDone)
 * and ends the thread. */
void ov01LoadComplete(void) {
    dataBattleSeRelease();
    ov01LoadDone = 1;
    ExitDeleteThread();
}

/* Starts a load thread running ov01LoadComplete. */
void ov01LoadBlockStart(void)
{
    ThreadParam param;

    ov01LoadDone = 0;
    param.entry = ov01LoadComplete;
    param.stack = D_00AC41A0;
    param.gpReg = D_6A2DF0;
    param.stackSize = OV01_LOAD_THREAD_STACK_SIZE;
    param.initPriority = OV01_LOAD_THREAD_PRIORITY;
    StartThread(CreateThread(&param), 0);
}

/* Load thread entry of ov01LoadRequest: passes the owner (ov01LoadOwner) to dataSlotUnload, marks the load
 * done and ends the thread. */
void ov01LoadCompleteForOwner(void) {
    dataSlotUnload(ov01LoadOwner);
    ov01LoadDone = 1;
    ExitDeleteThread();
}

/* Load request for an owner object: when handleStateGet reports the data present
 * the load is marked done at once; otherwise the owner is kept in ov01LoadOwner and
 * a load thread runs ov01LoadCompleteForOwner. */
void ov01LoadRequest(struct DataSlot *owner)
{
    ThreadParam param;

    if (handleStateGet(owner->handle) != 0) {
        ov01LoadDone = 1;
        return;
    }
    ov01LoadOwner = owner;
    ov01LoadDone = 0;
    param.entry = ov01LoadCompleteForOwner;
    param.stack = D_00AC41A0;
    param.gpReg = D_6A2DF0;
    param.stackSize = OV01_LOAD_THREAD_STACK_SIZE;
    param.initPriority = OV01_LOAD_THREAD_PRIORITY;
    StartThread(CreateThread(&param), 0);
}

/* Load-done flag (ov01LoadDone), polled by the wait steps. */
int ov01LoadIsDone(void) {
    return ov01LoadDone;
}

/* Step-chain start: a new save stack (ov01StackNew), the first step ov01StepSceneScript,
 * and the scene done flag ov01SceneDone cleared. */
void ov01StepChainStart(void)
{
    ov01StepSetNext(ov01StackNew(), ov01StepSceneScript);
    ov01SceneDone = 0;
}

/* Wait-and-pop step: once no CD read is pending and no load wait is pending, pops
 * the saved continuation (ov01StackPop, ov01StackPop). */
void ov01StepAwaitLoads(int context) {
    if ((ov01CdPendingReads() == 0) && (loadWaitPending() == 0)) {
        ov01StackPop(context);
    }
}

/* Scene data step: prepares the scene records (the failure paths of the original
 * spin forever), then continues to ov01StepInit. */
void ov01StepSceneScript(int context)
{
    struct SbImageHeader *key;

    runtimeInit();
    key = ov01ScriptKeyGet();
    if (sbLibraryLink(key, ov01ScriptImageGet()) == 0) {
        for (;;) {
        }
    }
    sbStartImage(ov01ScriptKeyGet());
    ov01StepSetNext(context, ov01StepInit);
}

/* Init step: enables the GIF resCmdEffect (gifStateEnableEffect), runs four init calls and
 * continues to ov01StepStartBlockLoad with ov01StepAwaitLoads as the fallback. */
void ov01StepInit(int context) {
    gifStateEnableEffect();
    dataFaceFileLoad();
    dataStatFileLoad();
    dataCdArenaOpen();
    dataBattleSeLoad();
    ov01StepSetNext(context, ov01StepStartBlockLoad);
    ov01StackPush(context, ov01StepAwaitLoads);
}

/* Starts the block load (ov01LoadBlockStart) and continues to ov01StepAwaitBlockLoad. */
void ov01StepStartBlockLoad(int context) {
    ov01LoadBlockStart();
    ov01StepSetNext(context, ov01StepAwaitBlockLoad);
}

/* Waits for the block load, then continues to ov01StepOpenHandles. */
void ov01StepAwaitBlockLoad(int context) {
    if (ov01LoadIsDone() != 0) {
        ov01StepSetNext(context, ov01StepOpenHandles);
    }
}

/* Opens two handles (D_00AB5CD4, ov01StreamBuffer) and continues to ov01StepBindAndShow. */
void ov01StepOpenHandles(int context) {
    UnitScriptEvent event;

    menuTexFlushTaskStart();
    seVoiceStartTagged(2, 4);
    D_00AB5CD4 = monsSlotCreate();
    ov01StreamBuffer[0] = dataStreamOpen(monsTblValueGet(), 0);
    event.unit = 0;
    unitScriptEventSend(6, &event);
    ov01StepSetNext(context, ov01StepBindAndShow);
    ov01StackPush(context, ov01StepAwaitLoads);
}

/* Binds the two handles, then continues to ov01StepLoadPartyIds. */
void ov01StepBindAndShow(int context) {
    monsSlotCopyParams(D_00AB5CD4, ov01StreamBuffer[0]);
    monsSlotShow(D_00AB5CD4);
    nmlModelEnableSubWindow0();
    ov01StepSetNext(context, ov01StepLoadPartyIds);
}

/* Reads the three party ids (bytes of ptGetStatusBlock) into ov01PartyList, runs the
 * party setup (posDataGet, effectPosTableSet) and continues to ov01StepSpawnPartyMember. */
void ov01StepLoadPartyIds(int context) {
    int source_index;
    int entry_index;
    unsigned char *src;
    unsigned char byte;
    struct EffectPosRecord *positions;

    source_index = 0;
    entry_index = 0;
    ov01PartyIndex = 0;
    ov01PartyList = ov01PartyListGet();
    do {
        src = ptGetStatusBlock() + source_index;
        source_index += 1;
        byte = *src;
        ov01PartyList[entry_index].id = byte;
        entry_index += 1;
    } while (source_index < 3);
    positions = posDataGet();
    effectPosTableSet(positions);
    ov01StepSetNext(context, ov01StepSpawnPartyMember);
}

/* Spawns party member ov01PartyIndex with unitPlCreate (unitPlCreate) and sets its
 * fields; a zero id skips to ov01StepNextPartyMember. Continues to ov01StepLoadPartyMember. */
void ov01StepSpawnPartyMember(int context) {
    int id;
    Ov01Object *object;

    id = ov01PartyList[ov01PartyIndex].id;
    if (id == 0) {
        ov01StepSetNext(context, ov01StepNextPartyMember);
        return;
    }
    object = unitPlCreate(id);
    ov01CurUnit = object;
    unitLoadSnapshot(object, ov01PartyList[ov01PartyIndex].spawnArgA);
    unitSetAngleDegrees(ov01CurUnit, ov01PartyList[ov01PartyIndex].spawnArgB);
    ov01CurUnit->stats->spawnArgC = 0;
    ov01CurUnit->stats->spawnArgD = 0;
    ov01CurSlot = dataUnitLoad(id);
    unitEffectSlotAcquire(id);
    ov01StepSetNext(context, ov01StepLoadPartyMember);
    ov01StackPush(context, ov01StepAwaitLoads);
}

/* Binds the spawned party member and starts its load (ov01LoadRequest); continues
 * to ov01StepAwaitPartyLoad. */
void ov01StepAwaitPartyLoad(int context);

void ov01StepLoadPartyMember(int context)
{
    Ov01Command command;

    unitBindActors(ov01CurUnit, ov01CurSlot);
    unitFadeIn(ov01CurUnit);
    command.command = UNIT_CMD_FINISH;
    unitQueueAppend(ov01CurUnit, &command);
    ov01LoadRequest(ov01CurSlot);
    ov01StepSetNext(context, ov01StepAwaitPartyLoad);
}

/* Waits for the party member's load, then continues to ov01StepNextPartyMember. */
void ov01StepAwaitPartyLoad(int context) {
    if (ov01LoadIsDone() != 0) {
        ov01StepSetNext(context, ov01StepNextPartyMember);
    }
}

/* Next party member: ov01PartyIndex counts up; below three it goes back to
 * ov01StepSpawnPartyMember, else menuStatRecordsCreate runs and the enemy step starts (ov01StepStartEnemies). */
void ov01StepNextPartyMember(int context) {
    int count;
    int more;

    count = ov01PartyIndex + 1;
    more = count < 3;
    ov01PartyIndex = count;
    if (more != 0) {
        ov01StepSetNext(context, ov01StepSpawnPartyMember);
        return;
    }
    menuStatRecordsCreate();
    ov01StepSetNext(context, ov01StepStartEnemies);
}

/* Enemy list start: resets ov01EnemyIndex and takes the enemy list from monsTblGet. */
void ov01StepStartEnemies(int context) {
    ov01EnemyIndex = 0;
    ov01EnemyList = monsTblGet();
    ov01StepSetNext(context, ov01StepSpawnEnemy);
}

/* Spawns enemy ov01EnemyIndex with unitEnCreate (unitEnCreate) and sets its fields and
 * sub record; continues to ov01StepLoadEnemy. */
void ov01StepSpawnEnemy(int context) {
    int id;
    Ov01Object *object;

    id = ov01EnemyList[ov01EnemyIndex].id;
    object = unitEnCreate(id);
    ov01CurUnit = object;
    unitLoadSnapshot(object, ov01EnemyList[ov01EnemyIndex].spawnArgA);
    unitSetAngleDegrees(ov01CurUnit, ov01EnemyList[ov01EnemyIndex].spawnArgB);
    ov01CurUnit->stats->spawnArgC = ov01EnemyList[ov01EnemyIndex].spawnArgC;
    ov01CurUnit->stats->spawnArgD = ov01EnemyList[ov01EnemyIndex].spawnArgD;
    ov01CurSlot = dataUnitLoad(id);
    unitEffectSlotAcquire(id);
    ov01StepSetNext(context, ov01StepLoadEnemy);
    ov01StackPush(context, ov01StepAwaitLoads);
}

/* Binds the enemy and starts its load (ov01LoadRequest); continues to ov01StepAwaitEnemyLoad. */
void ov01StepLoadEnemy(int context) {

    unitBindActors(ov01CurUnit, ov01CurSlot);
    unitFadeIn(ov01CurUnit);
    ov01LoadRequest(ov01CurSlot);
    ov01StepSetNext(context, ov01StepAwaitEnemyLoad);
}

/* Waits for the enemy's load, then continues to ov01StepNextEnemy. */
void ov01StepAwaitEnemyLoad(int context) {
    if (ov01LoadIsDone() != 0) {
        ov01StepSetNext(context, ov01StepNextEnemy);
    }
}

/* ov01 0x00AA4670, in a part of the overlay that has no C file yet. */
void menuValuePanelCreate(void);

/* Enemies in an enemy list at most; the list ends early at a record with id 0. */
#define OV01_ENEMY_MAX 5
/* streamPlayByKey key of the line a party member says when the battle starts. */
#define BATTLE_START_STREAM 6

/*
 * Next enemy: spawns the next one of the enemy list (ov01StepSpawnEnemy); after the last it builds the battle
 * menus and panels, starts the battle music and the opening line of a party member (unitSelectPlTarget), probes
 * the heap and continues to ov01StepBattle.
 */
void ov01StepNextEnemy(int context)
{
    Ov01Object *speaker;

    ov01EnemyIndex++;
    if (ov01EnemyIndex < OV01_ENEMY_MAX && ov01EnemyList[ov01EnemyIndex].id != 0) {
        ov01StepSetNext(context, ov01StepSpawnEnemy);
        return;
    }
    objPathInit();
    menuCursorCreate();
    menuIconListCreate();
    menuValuePanelCreate();
    actStepIconCreate();
    menuWepSpPanelNew();
    menuTargetListNew();
    menuUnitPanelNew();
    menuIconListNew();
    menuTimedSeqNew();
    menuPanelCreate();
    menuTgtStatusNew();
    dataObjCreateAtOrigin();
    battleStartBonusApply();
    bgmPlayTrack(bgmIndexGet());
    speaker = unitSelectPlTarget(0);
    if (speaker != 0) {
        streamPlayByKey(speaker, BATTLE_START_STREAM);
    }
    cfEventGet();
    ov01HeapProbeLargeAlloc();
    ov01StepSetNext(context, ov01StepBattle);
}
