/*
 * OV01 original TU 47: 0x00a9ff80..0x00aa1be0 (52 functions)
 */
#include "common.h"
#include "quadword.h"
#include "vu0_macro.h"
#include "main/tu022.h"
#include "main/tu156.h"
#include "main/xgl_2.h"
#include "main/tu063.h"
#include "main/tu077.h"
#include "main/tu079.h"
#include "main/tu051.h"
#include "main/pad.h"
#include "main/sef.h"
#include "main/xgl_font.h"
#include "ov01/tu002.h"
#include "ov01/tu008.h"
#include "ov01/tu009.h"
#include "ov01/tu010.h"
#include "ov01/tu012.h"
#include "ov01/tu017.h"
#include "ov01/tu019.h"
#include "ov01/tu022.h"
#include "ov01/tu023.h"
#include "ov01/tu036.h"
#include "ov01/tu037.h"
#include "ov01/tu038.h"
#include "ov01/tu042.h"
#include "ov01/tu044.h"
#include "ov01/tu046.h"
#include "ov01/tu047.h"
#include "ov01/tu054.h"
#include "ov01/tu066.h"
#include "ov01/tu071.h"


/* Passes the low halfword of vmArgGet(call, 1) to dataVPadSet; without an argument it requests the
 * SB_JUMP_ARGUMENTS jump and returns 0. */
int VPadSet(SbThread *call)
{
    int padBits;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    padBits = vmArgGet(call, 1);
    dataVPadSet(padBits & 0xFFFF);
    return 1;
}

#include "shared.h"

int msgPut(SbThread *call)
{
    int index;
    int slot;
    char *text;
    int posX;
    int posY;
    int size;
    int yIsDefault;
    int sizeIsGiven;

    if (VM_PLUGIN_CNT_GET(call) < 3) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    posX = 0x3C;
    if (vmArgTagIsZero(call, 4) == 0) {
        posX = vmArgGet(call, 4);
    }
    yIsDefault = vmArgTagIsZero(call, 5);
    posY = 0x1AC;
    if (yIsDefault == 0) {
        posY = vmArgGet(call, 5);
    }
    sizeIsGiven = vmArgTagIsZero(call, 6) == 0;
    size = 0x28;
    if (sizeIsGiven) {
        size = vmArgGet(call, 6);
    }
    text = (char *)vmKeyedEntryGet(call, vmArgGet(call, 3));
    index = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    msgPutObject(index, slot, text, posX, posY, size);
    return 1;
}


/* Sets *waiting to 1 when msgIsIdle returns 0; always returns 1. */
int msgPutWait(int unused, int *waiting)
{
    if (msgIsIdle() == 0) {
        *waiting = 1;
    }
    return 1;
}

void sefSetOverrideVectors(void *, void *);
void unitBodySetPosDir(DoublewordVector4 *pos, DoublewordVector4 *dir);
void unitTblBodySetPosDir(DoublewordVector4 *pos, DoublewordVector4 *dir);
/* The runtime position and direction; tu010 copies them whole into every battle actor's runtimeVecA/B. */
static DoublewordVector4 runtimePos;
static DoublewordVector4 runtimeDir;

/* Script values in hundredths. */
#define VM_FIXED_SCALE 100.0f

/* Script command: fills the runtime position and direction vectors from arguments 2 to 8, then copies them to units as flags ask. */
int runtimeSetFog(SbThread *call)
{
    int flags;

    if (VM_PLUGIN_CNT_GET(call) < 8) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    flags = vmArgGet(call, 1);
    runtimePos.vector.x = (float) vmArgGet(call, 2) / VM_FIXED_SCALE;
    runtimePos.vector.y = (float) vmArgGet(call, 3) / VM_FIXED_SCALE;
    runtimePos.vector.z = (float) vmArgGet(call, 4) / VM_FIXED_SCALE;
    runtimePos.vector.w = (float) vmArgGet(call, 5) / VM_FIXED_SCALE;
    runtimeDir.vector.x = (float) vmArgGet(call, 6);
    runtimeDir.vector.y = (float) vmArgGet(call, 7);
    runtimeDir.vector.z = (float) vmArgGet(call, 8);
    sefSetOverrideVectors(&runtimePos, &runtimeDir);
    if (flags & 1) {
        unitBodySetPosDir(&runtimePos, &runtimeDir);
    }
    if (flags & 2) {
        unitTblBodySetPosDir(&runtimePos, &runtimeDir);
    }
    return 1;
}

/* Copies pos and dir into the runtime vectors of the first active monster slot's actor and sets its
 * ACTOR_STATE_UNBOUND bit. The reads through the union keep the pos loads after the flag store. */
void unitBodySetPosDir(DoublewordVector4 *pos, DoublewordVector4 *dir)
{
    struct MonsSlot **slots;
    struct XenoActor *actor;

    monsActiveSlotsGet(&slots);
    actor = slots[0]->actor;
    actor->drawFlags |= ACTOR_STATE_UNBOUND;
    actor->runtimeVecA.vector.x = pos->vector.x;
    actor->runtimeVecA.vector.y = pos->vector.y;
    actor->runtimeVecA.vector.z = pos->vector.z;
    actor->runtimeVecA.vector.w = pos->vector.w;
    actor->runtimeVecB.vector.x = dir->vector.x;
    actor->runtimeVecB.vector.y = dir->vector.y;
    actor->runtimeVecB.vector.z = dir->vector.z;
}

int unitTblGet(int side, Ov01Object ***table);

/* Copies pos and dir into the runtime vectors of the actor of every unit that unitTblGet(2) returns. */
void unitTblBodySetPosDir(DoublewordVector4 *pos, DoublewordVector4 *dir)
{
    Ov01Object **units;
    Ov01Object *unit;
    struct XenoActor *actor;
    int count;
    int i;

    count = unitTblGet(2, &units);
    for (i = 0; i < count; i++) {
        unit = units[i];
        if (unit != 0) {
            actor = unit->unit;
            actor->runtimeVecA.vector.x = pos->vector.x;
            actor->runtimeVecA.vector.y = pos->vector.y;
            actor->runtimeVecA.vector.z = pos->vector.z;
            actor->runtimeVecA.vector.w = pos->vector.w;
            actor->runtimeVecB.vector.x = dir->vector.x;
            actor->runtimeVecB.vector.y = dir->vector.y;
            actor->runtimeVecB.vector.z = dir->vector.z;
        }
    }
}

/* Hands out the addresses of the runtime position and direction vectors through its out-pointers. */
void runtimeVecPtrGet(DoublewordVector4 **pos, DoublewordVector4 **dir)
{
    *pos = &runtimePos;
    *dir = &runtimeDir;
}

int setMoveCmd(SbThread *call)
{
    struct BattleState *block;
    int moveCmd;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    block = battleStateGet();
    moveCmd = vmArgGet(call, 1);
    block->moveCmd = moveCmd;
    return 1;
}

void xglLightSetDefault(XglLightSet *lightSet);
void pxySetDefaultLight(XglLightSet *light);

/* Native initLight: makes the light block the default light set of xgl, the pxy models and tu156. The block is
 * the 0x180-byte light set the battle actors copy as LightBlocks (unitCreateSide, unitTblBodySetLight). */
int initLight(void)
{
    XglLightSet *lightSet;

    lightSet = (XglLightSet *)lightBlockGet();
    xglLightSetDefault(lightSet);
    pxySetDefaultLight(lightSet);
    func_00253048(lightSet);
    return 1;
}

static LightBlocks lightBlock;

/* Returns the light block that initLight and runtime.setLight start from. */
LightBlocks *lightBlockGet(void) {
    return &lightBlock;
}

/* runtime.setLight kinds: 0-2 the directional lights (sDir/sCol 0-2), 3 the view light (sDir 6, sCol 6), 4 the
 * lower ambient colour (sCol 7), 5-7 the shadow directions (sSha 0-2). */
#define LIGHT_KIND_VIEW 3
#define LIGHT_KIND_AMBIENT_LOWER 4
#define LIGHT_KIND_SHADOW 5
/* Colour arguments are 0..255 per channel, direction arguments hundredths. */
#define LIGHT_COLOR_SCALE (1.0f / 255.0f)
#define LIGHT_DIRECTION_SCALE 0.01f

void unitTblBodySetLight(LightBlocks *light);

/* One quadword copy. An inline function, not the macro directly: its asm keeps one source line for every caller,
 * so jump2 can cross-jump the three identical bodies of kinds 0-2 after register allocation. */
static inline void lightCopy(Vector4 *destination, AlignedVector4 *source)
{
    QuadwordCopyT0(destination, source);
}

/* Native runtime.setLight(kind, red, green, blue, x, y, z): writes a colour and a direction into light KIND of the
 * light block (grey and +z without arguments) and makes the block the default light set again. */
int runtimeSetLight(SbThread *call)
{
    XglLightSet *lights;
    AlignedVector4 color;
    AlignedVector4 direction;
    AlignedVector4 *dir;
    int kind;
    int index;

    lights = (XglLightSet *)lightBlockGet();
    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    Vu0SetOrigin(&color);
    Vu0SetOrigin(&direction);
    kind = vmArgGet(call, 1);
    color.x = vmArgGet(call, 2) * LIGHT_COLOR_SCALE;
    color.y = vmArgGet(call, 3) * LIGHT_COLOR_SCALE;
    color.z = vmArgGet(call, 4) * LIGHT_COLOR_SCALE;
    direction.x = vmArgGet(call, 5) * LIGHT_DIRECTION_SCALE;
    direction.y = vmArgGet(call, 6) * LIGHT_DIRECTION_SCALE;
    direction.z = vmArgGet(call, 7) * LIGHT_DIRECTION_SCALE;
    /* The copies below go through this pointer: the &direction of Vu0SetOrigin then stays a block-local pseudo
     * in $16 and dir takes $16 by copy preference after call, lights and kind got $17-$19 (original order). */
    dir = &direction;
    if (vmArgTagIsZero(call, 2)) {
        color.x = 0.5f;
        color.y = 0.5f;
        color.z = 0.5f;
    }
    if (vmArgTagIsZero(call, 5)) {
        direction.x = 0.0f;
        direction.y = 0.0f;
        direction.z = 1.0f;
    }
    /* Kinds 0-2 as three identical bodies (merged by jump2): their extra uses raise the priority of dir. */
    switch (kind) {
    case 0:
        lightCopy((Vector4 *)lights->directions[kind], dir);
        lightCopy((Vector4 *)lights->colors[kind], &color);
        break;
    case 1:
        lightCopy((Vector4 *)lights->directions[kind], dir);
        lightCopy((Vector4 *)lights->colors[kind], &color);
        break;
    case 2:
        lightCopy((Vector4 *)lights->directions[kind], dir);
        lightCopy((Vector4 *)lights->colors[kind], &color);
        break;
    case LIGHT_KIND_VIEW:
        lightCopy(&lights->viewDirection, dir);
        lightCopy(&lights->ambientColor, &color);
        break;
    case LIGHT_KIND_AMBIENT_LOWER:
        lightCopy(&lights->ambientColorLower, &color);
        break;
    case LIGHT_KIND_SHADOW:
    case LIGHT_KIND_SHADOW + 1:
    case LIGHT_KIND_SHADOW + 2:
        index = kind - LIGHT_KIND_SHADOW;
        lightCopy(&lights->shadows[index], dir);
        if (vmArgTagIsZero(call, 2) || vmArgTagIsZero(call, 5)) {
            lights->shadows[index].w = 0.0f;
        }
        break;
    }
    pxySetDefaultLight(lights);
    func_00253048(lights);
    unitTblBodySetLight((LightBlocks *)lights);
    return 1;
}

/* Copies the light blocks into the actor of every unit of unitTblGet(2). */
void unitTblBodySetLight(LightBlocks *light)
{
    Ov01Object **units;
    int count;
    int i;

    count = unitTblGet(2, &units);
    for (i = 0; i < count; i++) {
        if (units[i] != 0) {
            units[i]->unit->lightBlocks = *light;
        }
    }
}

/*
 * Native pari(unit, count, state, velocity, value [, angle [, value3]]): sets the move parameters of the unit's
 * actor: moveVelocity, shadowValue (default 1), moveAngle (default 0) and shadowValue3 (default 1) from
 * hundredths, shadowCount and moveState as given. Jumps to SB_JUMP_ARGUMENTS with fewer than four arguments.
 */
int pari(SbThread *thread)
{
    struct XenoActor *actor;
    float velocity;
    float value;
    float angle;
    float value3;

    if (VM_PLUGIN_CNT_GET(thread) < 4) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    actor = unitTblEntryGet(vmArgGet(thread, 1))->unit;
    velocity = vmArgGet(thread, 4) / VM_FIXED_SCALE;
    /* if/else (not a default before a plain if): the scale constant is then shared by the first two
     * divisions only, as in the original. */
    if (vmArgTagIsZero(thread, 5)) {
        value = 1.0f;
    } else {
        value = vmArgGet(thread, 5) / VM_FIXED_SCALE;
    }
    if (vmArgTagIsZero(thread, 6)) {
        angle = 0.0f;
    } else {
        angle = vmArgGet(thread, 6) / VM_FIXED_SCALE;
    }
    if (vmArgTagIsZero(thread, 7)) {
        value3 = 1.0f;
    } else {
        value3 = vmArgGet(thread, 7) / VM_FIXED_SCALE;
    }
    actor->moveAngle = angle;
    actor->shadowValue = value;
    actor->moveVelocity = velocity;
    actor->shadowCount = vmArgGet(thread, 2);
    actor->shadowValue3 = value3;
    actor->moveState = vmArgGet(thread, 3);
    return 1;
}

int partyDataGetSlot(int, int);

int partyDataGet(SbThread *call)
{
    int partyIndex;
    int slot;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    partyIndex = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    return partyDataGetSlot(partyIndex, slot);
}


int partyDataSet(SbThread *call)
{
    int partyIndex;
    int slot;
    int value;

    if (VM_PLUGIN_CNT_GET(call) < 3) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    partyIndex = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    value = vmArgGet(call, 3);
    partyDataSetSlot(partyIndex, slot, value);
    return 1;
}

/* One 0x14-byte entry of the table ov01PartyListGet returns: only +0x04 and +0x08 are written here. */
typedef struct PlPosEntry PlPosEntry;

struct PlPosEntry {
    unsigned char unmodeled_000[4];
    int position;                          /* +0x04: written from script argument 2 by plPosInit */
    int direction;                         /* +0x08: degrees 0..360, written by plDirInit */
    unsigned char unmodeled_00C[0x14 - 0x0C];
};

PlPosEntry *ov01PartyListGet();

int plPosInit(SbThread *call)
{
    int entryIndex;
    PlPosEntry *entries;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    entryIndex = vmArgGet(call, 1);
    entries = ov01PartyListGet();
    entries[entryIndex].position = vmArgGet(call, 2);
    return 1;
}

/* Native plDirInit: sets the direction of party entry argument 1 to argument 2,
 * brought into 0..360 degrees. */
int plDirInit(SbThread *call)
{
    int entryIndex;
    PlPosEntry *entries;
    int direction;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    entryIndex = vmArgGet(call, 1);
    entries = ov01PartyListGet();
    direction = vmArgGet(call, 2);
    while (direction < 0) {
        direction += 360;
    }
    if (direction > 360) {
        direction %= 360;
    }
    entries[entryIndex].direction = direction;
    return 1;
}

Ov01Object *unitEnCreate(int id);

/* Native unitCreate: creates enemy unit argument 1, loads snapshot argument 2,
 * turns it to argument 3 degrees and stores arguments 4 and 5 in its stat block
 * (spawnArgC, spawnArgD); returns the unit, or 0 when it could not be created. */
int unitCreate(SbThread *call)
{
    Ov01Object *unit;

    if (VM_PLUGIN_CNT_GET(call) < 5) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitEnCreate(vmArgGet(call, 1));
    if (unit == 0) {
        return 0;
    }
    unitLoadSnapshot(unit, vmArgGet(call, 2));
    unitSetAngleDegrees(unit, vmArgGet(call, 3));
    unit->stats->spawnArgC = vmArgGet(call, 4);
    unit->stats->spawnArgD = vmArgGet(call, 5);
    /* The script keeps the unit as its address. */
    return (int) unit;
}


int unitLoad(SbThread *call)
{
    int unitIndex;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    dataUnitLoad(unitIndex);
    unitEffectSlotAcquire(unitIndex);
    return 1;
}


/* Sets *waiting to 1 when ov01CdPendingReads or loadWaitPending is nonzero or ov01LoadIsDone is zero; always returns 1. */
int unitLoadWait(int unused, int *waiting)
{
    if ((ov01CdPendingReads() != 0) || (loadWaitPending() != 0) || (ov01LoadIsDone() == 0)) {
        *waiting = 1;
    }
    return 1;
}

void unitBindActors(Ov01Object *unit, struct DataSlot *slot);

/* Native unitBind: binds unit (argument 2) to the data slot of argument 1, fades it
 * in and starts the slot's load; 0 when the slot is not found. */
int unitBind(SbThread *call)
{
    struct DataSlot *slot;
    Ov01Object *unit;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    slot = dataSlotFind(vmArgGet(call, 1));
    if (slot == 0) {
        return 0;
    }
    /* Script argument 2 is the unit's address. */
    unit = (Ov01Object *) vmArgGet(call, 2);
    unitBindActors(unit, slot);
    unitFadeIn(unit);
    func_00A955C8(unit);
    ov01LoadRequest(slot);
    return 1;
}

/* Native unitBind2: unitBind with unitUnbind in place of the fade-in. */
int unitBind2(SbThread *call)
{
    struct DataSlot *slot;
    Ov01Object *unit;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    slot = dataSlotFind(vmArgGet(call, 1));
    if (slot == 0) {
        return 0;
    }
    /* Script argument 2 is the unit's address. */
    unit = (Ov01Object *) vmArgGet(call, 2);
    unitBindActors(unit, slot);
    unitUnbind(unit);
    func_00A955C8(unit);
    ov01LoadRequest(slot);
    return 1;
}

void arrowCreate(int, int, int);

int arrowOn(SbThread *call)
{
    int index;
    int slot;
    int value;

    if (VM_PLUGIN_CNT_GET(call) < 3) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    index = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    value = vmArgGet(call, 3);
    arrowCreate(index, slot, value);
    return 1;
}


int arrowOff(void)
{
    arrowRemove();
    return 1;
}

static int bgmIndex;

int bgmPlay(SbThread *call)
{
    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    bgmIndex = vmArgGet(call, 1);
    return 1;
}

/* Returns the background music index that bgmPlay stored. */
int bgmIndexGet(void) {
    return bgmIndex;
}


int bgmStop(void)
{
    streamSlotStop(0);
    return 1;
}

/* Bit i set for each occupied entry i of the side-2 unit table. */
int unitTblBitGet(void)
{
    Ov01Object **units;
    int count;
    int bits;
    int i;

    count = unitTblGet(2, &units);
    bits = 0;
    for (i = 0; i < count; i++) {
        if (units[i] != 0) {
            bits |= 1 << i;
        }
    }
    return bits;
}


void frameCntGet(void)
{
    ov01SceneFrameCount();
}


void ETGet(void) {
    actStepCounterValue();
}

int unitIsInFacingArc(void *, void *);

int unitDirChk(SbThread *call)
{
    void *unit;
    void *target;
    int unitIndex;
    int targetIndex;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    targetIndex = vmArgGet(call, 2);
    target = unitTblEntryGet(targetIndex);
    return unitIsInFacingArc(unit, target);
}

static int D_00AE0C2C;
static int D_00AE0C30;

int runEffSet(SbThread *call)
{
    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    D_00AE0C2C = 0x140;
    D_00AE0C30 = vmArgGet(call, 1);
    return 1;
}

/* Reads back the two words runEffSet stores. */
void runEffGet(int *first, int *second)
{
    *first = D_00AE0C2C;
    *second = D_00AE0C30;
}


int decoDisp(SbThread *call)
{
    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    if (vmArgGet(call, 1) == 0) {
        decoFlagsSetAll();
    } else {
        decoFlagsClearAll();
    }
    return 1;
}


int decoDisp2(SbThread *call)
{
    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    if (vmArgGet(call, 1) == 0) {
        decoFlagsSetFirst();
    } else {
        decoFlagsClearFirst();
    }
    return 1;
}

void textWindowShow(int, int, int, int, int);

int namePut(SbThread *call)
{
    int index;
    int slot;
    int pos;
    int key;
    int mode;
    int raw;

    if (VM_PLUGIN_CNT_GET(call) < 5) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    raw = vmArgGet(call, 4);
    key = vmKeyedEntryGet(call, raw);
    index = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    pos = vmArgGet(call, 3);
    mode = vmArgGet(call, 5);
    textWindowShow(index, slot, pos, key, mode);
    return 1;
}

/*
 * The vector at XenoActor +0x90 as objx writes it: one float per component. Stored as
 * actor->vector90.x/y/z the function is two instructions shorter than the original; written
 * through this union view, each store reloads the actor pointer as the original does.
 */
typedef union ActorVectorElements {
    Vector4 vector;
    float element[4];
} ActorVectorElements;

/* Native objx: sets the scale of unit argument 1 to argument 2 percent on all three axes. */
int objx(SbThread *call)
{
    Ov01Object *unit;
    float scale;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitTblEntryGet(vmArgGet(call, 1));
    scale = vmArgGet(call, 2) / 100.0f;
    ((ActorVectorElements *)&unit->unit->vector90)->element[0] = scale;
    ((ActorVectorElements *)&unit->unit->vector90)->element[1] = scale;
    ((ActorVectorElements *)&unit->unit->vector90)->element[2] = scale;
    return 1;
}


int streamPlay(SbThread *call)
{
    int streamId;
    const char *streamHandle;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    streamId = vmArgGet(call, 1);
    streamHandle = (const char *)vmKeyedEntryGet(call, streamId);
    soundSlotReopen(1);
    streamSetLevel(1);
    streamSlotSetFile(1, streamHandle);
    return 1;
}

int unitGetAngleDegrees(Ov01Object *unit);

int unitDirGet(SbThread *call)
{
    int unitIndex;
    Ov01Object *unit;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    return unitGetAngleDegrees(unit);
}

float unitDefArea(Ov01Object *unit);

int unitDefAreaGet(SbThread *call)
{
    int unitIndex;
    Ov01Object *unit;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    return (int) (unitDefArea(unit) * 100.0f);
}


int cfFlagSet(SbThread *call)
{
    int flags;
    int bits;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    flags = cfEventGet();
    bits = vmArgGet(call, 1);
    cfEventSet(flags | bits);
    return 1;
}


/* Script command: code 0x5A starts the BGM fade through func_001EDC20; any other code goes to soundSlotSetVolume with its value. */
int runtimeBgmFade(SbThread *call)
{
    int code;
    int value;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    code = vmArgGet(call, 1);
    value = vmArgGet(call, 2);
    if (code == 0x5A) {
        func_001EDC20(0);
    } else {
        soundSlotSetVolume(0, code, value);
    }
    return 1;
}

/* The second argument is the frame count passed to unitFadeTo (ov01/tu018). */
int unitFade(SbThread *call)
{
    Ov01Object *unit;
    int unitIndex;
    int frames;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    frames = vmArgGet(call, 2);
    unitFadeTo(unit, frames, vmArgGet(call, 3));
    return 1;
}

/* Native stockGet: the key of the unit in action stock slot argument 1, or 0 for an empty or invalid slot. */
s16 stockGet(SbThread *call)
{
    s16 key;
    StatOwner *owner;
    int slot;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    key = 0;
    slot = vmArgGet(call, 1);
    if (slot < ACT_STOCK_SLOTS) {
        owner = actStockGet()->stock[slot];
        if (owner != 0) {
            key = owner->stats->key;
        }
    }
    return key;
}

int unitMotWait(SbThread *call, int *waiting)
{
    Ov01Object *unit;
    int unitIndex;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    if (unit->flags & 0x10) {
        *waiting = 1;
    }
    if (unitIsDone(unit) == 0) {
        *waiting = 1;
    }
    return 1;
}

int XenoActorHidePart(struct XenoActor *actor, int part);

int partsHide(SbThread *call)
{
    int unitIndex;
    struct XenoActor *actor;
    int value;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    actor = unitTblEntryGet(unitIndex)->unit;
    value = vmKeyedEntryGet(call, vmArgGet(call, 2));
    XenoActorHidePart(actor, value);
    return 1;
}

int XenoActorShowPart(struct XenoActor *actor, int part);

int partsShow(SbThread *call)
{
    int unitIndex;
    struct XenoActor *actor;
    int value;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    actor = unitTblEntryGet(unitIndex)->unit;
    value = vmKeyedEntryGet(call, vmArgGet(call, 2));
    XenoActorShowPart(actor, value);
    return 1;
}

int put(void) {
    return 1;
}

extern PadRecord PadData;

/* Native sampPadRead: the d-pad bits of pad 0 - pressed (argument 1), repeated
 * (2) or held (any other value). */
int sampPadRead(SbThread *call)
{
    int value;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    switch (vmArgGet(call, 1)) {
    case 1:
        value = PadData.directionPressed;
        break;
    case 2:
        value = PadData.directionRepeated;
        break;
    default:
        value = PadData.directionHeld;
        break;
    }
    return value;
}


int sampFontPut(SbThread *call)
{
    int index;
    int slot;
    int raw;
    int key;

    if (VM_PLUGIN_CNT_GET(call) < 3) {
        sbSlotJumpRequest(call, SB_JUMP_ARGUMENTS);
        return 0;
    }
    index = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    raw = vmArgGet(call, 3);
    key = vmKeyedEntryGet(call, raw);
    xglFontPrint(index, slot, -1, key);
    return 1;
}

/* One native method of an owner table: the script name and its function (a table ends with a null name). */
typedef struct NativeMethod {
    const char *name;
    int (*func)();
} NativeMethod;

/* Natives of other files and of the asm-only runtimeSetLight, listed in the tables below. */
int runtimeSetLight();
int VPadDisable();
int VPadEnable();
int actorGet();
int atkTbl();
int atkTblGet();
int boost();
int cameraSet();
int counterBoost();
int createEffect();
int createEffectPos();
int loadEffect();
int loadWaitEffect();
int monsSetNoGet();
int monsTblSet();
int posDataSet();
int tecParaGet();
int thinkSet();
int thinkSet2();
int thinkSetEnd();
int unitParaGet();
int unitParaSet();
int unitStatGet();
int unitStatSet();

/* Native method tables of the three owners registered by runtimeInit. */
NativeMethod sampMethods[] = {
    { "fontPut", sampFontPut },
    { "padRead", sampPadRead },
    { 0, 0 },
};

NativeMethod debMethods[] = {
    { "put", put },
    { 0, 0 },
};

NativeMethod batMethods[] = {
    { "posDataSet", posDataSet },
    { "monsTblSet", monsTblSet },
    { "monsSetNoGet", monsSetNoGet },
    { "thinkSet", thinkSet },
    { "thinkSet2", thinkSet2 },
    { "thinkSetEnd", thinkSetEnd },
    { "atkTbl", atkTbl },
    { "atkTblGet", atkTblGet },
    { "cameraSet", cameraSet },
    { "actorGet", actorGet },
    { "unitParaGet", unitParaGet },
    { "unitParaSet", unitParaSet },
    { "createEffect", createEffect },
    { "createEffectPos", createEffectPos },
    { "loadEffect", loadEffect },
    { "loadWaitEffect", loadWaitEffect },
    { "tecParaGet", tecParaGet },
    { "unitStatGet", unitStatGet },
    { "unitStatSet", unitStatSet },
    { "counterBoost", counterBoost },
    { "boost", boost },
    { "VPadEnable", VPadEnable },
    { "VPadDisable", VPadDisable },
    { "VPadSet", VPadSet },
    { "msgPut", msgPut },
    { "msgPutWait", msgPutWait },
    { "setFog", runtimeSetFog },
    { "setMoveCmd", setMoveCmd },
    { "initLight", initLight },
    { "setLight", runtimeSetLight },
    { "pari", pari },
    { "partyDataGet", partyDataGet },
    { "partyDataSet", partyDataSet },
    { "plPosInit", plPosInit },
    { "plDirInit", plDirInit },
    { "unitCreate", unitCreate },
    { "unitLoad", unitLoad },
    { "unitLoadWait", unitLoadWait },
    { "unitBind", unitBind },
    { "unitBind2", unitBind2 },
    { "arrowOn", arrowOn },
    { "arrowOff", arrowOff },
    { "bgmPlay", bgmPlay },
    { "bgmStop", bgmStop },
    { "unitTblBitGet", unitTblBitGet },
    { "frameCntGet", frameCntGet },
    { "ETGet", ETGet },
    { "unitDirChk", unitDirChk },
    { "runEffSet", runEffSet },
    { "decoDisp", decoDisp },
    { "decoDisp2", decoDisp2 },
    { "namePut", namePut },
    { "scale", objx },
    { "streamPlay", streamPlay },
    { "unitDirGet", unitDirGet },
    { "unitDefAreaGet", unitDefAreaGet },
    { "cfFlagSet", cfFlagSet },
    { "bgmFade", runtimeBgmFade },
    { "unitFade", unitFade },
    { "stockGet", stockGet },
    { "unitMotWait", unitMotWait },
    { "partsHide", partsHide },
    { "partsShow", partsShow },
    { 0, 0 },
};

void sbNativeOwnerAdd(const char *, NativeMethod *);
void *memset(void *, int, unsigned int);

/* Registers two command tables with sbNativeOwnerAdd, then clears the state the runtime commands write: the two words of runEffSet, the BGM index, the light block and both vectors. */
void runtimeInitState(void)
{
    sbNativeOwnerAdd("samp", sampMethods);
    sbNativeOwnerAdd("deb", debMethods);
    D_00AE0C2C = 0;
    D_00AE0C30 = 0;
    bgmIndex = 0;
    memset(&lightBlock, 0, sizeof(LightBlocks));
    memset(&runtimePos, 0, sizeof(DoublewordVector4));
    memset(&runtimeDir, 0, sizeof(DoublewordVector4));
}

void runtimeInitState(void);

/* Runtime start-up: resets the state, registers a third command table, then runs the two start calls. */
void runtimeInit(void)
{
    sbResetNativeCounts();
    runtimeInitState();
    sbNativeOwnerAdd("bat", batMethods);
    mathInit();
    cameraRegisterNatives();
}

