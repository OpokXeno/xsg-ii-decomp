/*
 * OV01 original TU 47: 0x00a9ff80..0x00aa1be0 (52 functions)
 */
#include "common.h"
#include "main/tu022.h"
#include "main/tu063.h"
#include "main/tu077.h"
#include "main/tu079.h"
#include "main/tu173.h"
#include "main/xgl_font.h"
#include "ov01/tu009.h"
#include "ov01/tu022.h"
#include "ov01/tu032.h"
#include "ov01/tu037.h"
#include "ov01/tu038.h"
#include "ov01/tu042.h"
#include "ov01/tu044.h"
#include "ov01/tu054.h"
#include "ov01/tu066.h"
#include "ov01/tu071.h"

int VM_PLUGIN_CNT_GET();
int vmArgGet(int, int);

/* Passes the low halfword of vmArgGet(call, 1) to dataVPadSet; with no plugin loaded it calls sbChainSetValue(call, 2) and returns 0. */
int VPadSet(int call)
{
    int padBits;

    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    padBits = vmArgGet(call, 1);
    dataVPadSet(padBits & 0xFFFF);
    return 1;
}

#include "shared.h"

int msgPutObject(int, int, int, int, int, int);
int vmArgTagIsZero(int, int);
int vmKeyedEntryGet(int, int);

int msgPut(int call)
{
    int index;
    int slot;
    int key;
    int posX;
    int posY;
    int size;
    int yIsDefault;
    int sizeIsGiven;

    if (VM_PLUGIN_CNT_GET() < 3) {
        sbChainSetValue(call, 2);
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
    key = vmKeyedEntryGet(call, vmArgGet(call, 3));
    index = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    msgPutObject(index, slot, key, posX, posY, size);
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
void unitBodySetPosDir(void *, void *);
void unitTblBodySetPosDir(Vector4 *pos, Vector4 *dir);
extern Vector4 runtimePos;
extern Vector4 runtimeDir;

/* Script command: fills the runtime position and direction vectors from arguments 2 to 8, then copies them to units as flags ask. */
int runtimeSetFog(int call)
{
    int flags;

    if (VM_PLUGIN_CNT_GET() < 8) {
        sbChainSetValue(call, 2);
        return 0;
    }
    flags = vmArgGet(call, 1);
    runtimePos.x = (float) vmArgGet(call, 2) / 100.0f;
    runtimePos.y = (float) vmArgGet(call, 3) / 100.0f;
    runtimePos.z = (float) vmArgGet(call, 4) / 100.0f;
    runtimePos.w = (float) vmArgGet(call, 5) / 100.0f;
    runtimeDir.x = (float) vmArgGet(call, 6);
    runtimeDir.y = (float) vmArgGet(call, 7);
    runtimeDir.z = (float) vmArgGet(call, 8);
    sefSetOverrideVectors(&runtimePos, &runtimeDir);
    if (flags & 1) {
        unitBodySetPosDir(&runtimePos, &runtimeDir);
    }
    if (flags & 2) {
        unitTblBodySetPosDir(&runtimePos, &runtimeDir);
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", unitBodySetPosDir);

/* Unit table entry: only the body pointer at +0x1D0 is used here. */
typedef struct UnitTblEntry UnitTblEntry;

struct UnitTblEntry {
    unsigned char unmodeled_000[0x1D0];
    struct UnitBody *body;                 /* +0x1D0 */
};

int unitTblGet(int side, UnitTblEntry ***table);

/* Partial view of the body: the vector pair at +0x490 and +0x4A0. */
typedef struct UnitBody UnitBody;

struct UnitBody {
    unsigned char unmodeled_000[0x490];
    Vector4 pos;                           /* +0x490..+0x49F */
    float dir[3];                          /* +0x4A0..+0x4AB */
};

/* Copies pos and dir into the body of every unit that unitTblGet(2) returns. */
void unitTblBodySetPosDir(Vector4 *pos, Vector4 *dir)
{
    UnitTblEntry **units;
    UnitTblEntry *unit;
    UnitBody *body;
    int count;
    int i;

    count = unitTblGet(2, &units);
    for (i = 0; i < count; i++) {
        unit = units[i];
        if (unit != 0) {
            body = unit->body;
            body->pos.x = pos->x;
            body->pos.y = pos->y;
            body->pos.z = pos->z;
            body->pos.w = pos->w;
            body->dir[0] = dir->x;
            body->dir[1] = dir->y;
            body->dir[2] = dir->z;
        }
    }
}

/* Hands out the addresses of the runtime position and direction vectors through its out-pointers. */
void runtimeVecPtrGet(void **pos, void **dir)
{
    *pos = &runtimePos;
    *dir = &runtimeDir;
}

/* Only the word setMoveCmd stores is evidenced; the rest of the block is not modeled. */
struct MoveCmdBlock {
    unsigned char unmodeled_00[0x50];
    int move_cmd; /* +0x50 */
};

struct MoveCmdBlock *battleStateGet(void);

int setMoveCmd(int call)
{
    struct MoveCmdBlock *block;
    int moveCmd;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    block = battleStateGet();
    moveCmd = vmArgGet(call, 1);
    block->move_cmd = moveCmd;
    return 1;
}

int xglLightSetDefault(int);
int pxySetDefaultLight(int);
int func_00253048(int);
void *lightBlockGet(void);

int initLight(void)
{
    int lightBlock;

    lightBlock = lightBlockGet();
    xglLightSetDefault(lightBlock);
    pxySetDefaultLight(lightBlock);
    func_00253048(lightBlock);
    return 1;
}

extern unsigned char lightBlock[];

/* Returns the 0x180-byte light block that initLight and runtime.setLight start from. */
void *lightBlockGet(void) {
    return lightBlock;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", runtimeSetLight);

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", unitTblBodySetLight);

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", pari);

int partyDataGetSlot(int, int);

int partyDataGet(int call)
{
    int partyIndex;
    int slot;

    if (VM_PLUGIN_CNT_GET() < 2) {
        sbChainSetValue(call, 2);
        return 0;
    }
    partyIndex = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    return partyDataGetSlot(partyIndex, slot);
}


int partyDataSet(int call)
{
    int partyIndex;
    int slot;
    int value;

    if (VM_PLUGIN_CNT_GET() < 3) {
        sbChainSetValue(call, 2);
        return 0;
    }
    partyIndex = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    value = vmArgGet(call, 3);
    partyDataSetSlot(partyIndex, slot, value);
    return 1;
}

/* One 0x14-byte entry of the table ov01PartyListGet returns: only +0x04 is written here. */
typedef struct PlPosEntry PlPosEntry;

struct PlPosEntry {
    unsigned char unmodeled_000[4];
    int position;                          /* +0x04: written from script argument 2 by plPosInit */
    unsigned char unmodeled_008[0x14 - 0x08];
};

PlPosEntry *ov01PartyListGet();

int plPosInit(int call)
{
    int entryIndex;
    PlPosEntry *entries;

    if (VM_PLUGIN_CNT_GET() < 2) {
        sbChainSetValue(call, 2);
        return 0;
    }
    entryIndex = vmArgGet(call, 1);
    entries = ov01PartyListGet();
    entries[entryIndex].position = vmArgGet(call, 2);
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", plDirInit);

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", unitCreate);

int dataUnitLoad(int);
int func_00A82E18(int);

int unitLoad(int call)
{
    int unitIndex;

    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    dataUnitLoad(unitIndex);
    func_00A82E18(unitIndex);
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

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", unitBind);

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", unitBind2);

void arrowCreate(int, int, int);

int arrowOn(int call)
{
    int index;
    int slot;
    int value;

    if (VM_PLUGIN_CNT_GET() < 3) {
        sbChainSetValue(call, 2);
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

extern int bgmIndex;

int bgmPlay(int call)
{
    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
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

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", unitTblBitGet);


void frameCntGet(void)
{
    ov01SceneFrameCount();
}


void ETGet(void) {
    actStepCounterValue();
}

void *unitTblEntryGet(int);
int func_00A8D9E8(void *, void *);

int unitDirChk(int call)
{
    void *unit;
    void *target;
    int unitIndex;
    int targetIndex;

    if (VM_PLUGIN_CNT_GET() < 2) {
        sbChainSetValue(call, 2);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    targetIndex = vmArgGet(call, 2);
    target = unitTblEntryGet(targetIndex);
    return func_00A8D9E8(unit, target);
}

extern int D_00AE0C2C;
extern int D_00AE0C30;

int runEffSet(int call)
{
    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
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

void decoFlagsSetAll(void);

int decoDisp(int call)
{
    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    if (vmArgGet(call, 1) == 0) {
        decoFlagsSetAll();
    } else {
        decoFlagsClearAll();
    }
    return 1;
}


int decoDisp2(int call)
{
    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
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

int namePut(int call)
{
    int index;
    int slot;
    int pos;
    int key;
    int mode;
    int raw;

    if (VM_PLUGIN_CNT_GET() < 5) {
        sbChainSetValue(call, 2);
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

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", objx);


int streamPlay(int call)
{
    int streamId;
    int streamHandle;

    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    streamId = vmArgGet(call, 1);
    streamHandle = vmKeyedEntryGet(call, streamId);
    soundSlotReopen(1);
    streamSetLevel(1);
    streamSlotSetFile(1, streamHandle);
    return 1;
}

int unitGetAngleDegrees(int);

int unitDirGet(int call)
{
    int unitIndex;
    int unit;

    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    return unitGetAngleDegrees(unit);
}

float func_00A87390(int);

int unitDefAreaGet(int call)
{
    int unitIndex;
    int unit;

    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    return (int) (func_00A87390(unit) * 100.0f);
}

int cfEventGet(void);
int cfEventSet(int);

int cfFlagSet(int call)
{
    int flags;
    int bits;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    flags = cfEventGet();
    bits = vmArgGet(call, 1);
    cfEventSet(flags | bits);
    return 1;
}


/* Script command: code 0x5A starts the BGM fade through func_001EDC20; any other code goes to soundSlotSetVolume with its value. */
int runtimeBgmFade(int call)
{
    int code;
    int value;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbChainSetValue(call, 2);
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

int unitFadeTo(void *, int, int);

/* The second argument is the frame count passed to unitFadeTo (ov01/tu018). */
int unitFade(int call)
{
    void *unit;
    int unitIndex;
    int frames;

    if (VM_PLUGIN_CNT_GET() < 2) {
        sbChainSetValue(call, 2);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    unit = unitTblEntryGet(unitIndex);
    frames = vmArgGet(call, 2);
    unitFadeTo(unit, frames, vmArgGet(call, 3));
    return 1;
}

/* Partial views of a stock slot: the detail pointer at +0x144 and the count at +0x62. */
typedef struct StockDetail StockDetail;

struct StockDetail {
    unsigned char unmodeled_000[0x62];
    s16 stock;                             /* +0x62 */
};

typedef struct StockOwner StockOwner;

struct StockOwner {
    unsigned char unmodeled_000[0x144];
    StockDetail *detail;                   /* +0x144 */
};

StockOwner **actStockGet();

s16 stockGet(int call)
{
    s16 stock;
    StockOwner **owners;
    StockOwner *owner;
    StockOwner **ownerSlot;
    int slot;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbChainSetValue(call, 2);
        return 0;
    }
    stock = 0;
    slot = vmArgGet(call, 1);
    if (slot < 4) {
        owners = actStockGet();
        ownerSlot = owners + slot;
        owner = *ownerSlot;
        if (owner != 0) {
            stock = owner->detail->stock;
        }
    }
    return stock;
}

int unitIsDone(void *);

/* Partial view of the unit record: only the flags word at +0x0C is used here. */
typedef struct UnitMotRecord UnitMotRecord;

struct UnitMotRecord {
    unsigned char unmodeled_000[0xC];
    int flags;                             /* +0x0C; bit 0x10 */
};

int unitMotWait(int call, int *waiting)
{
    UnitMotRecord *unit;
    int unitIndex;

    if (VM_PLUGIN_CNT_GET() <= 0) {
        sbChainSetValue(call, 2);
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

void XenoActorHidePart(int, int);

/* Partial view of the record unitTblEntryGet returns: only +0x1D0 is used here. */
typedef struct PartsUnit PartsUnit;

struct PartsUnit {
    unsigned char unmodeled_000[0x1D0];
    int parts;                             /* +0x1D0 */
};

int partsHide(int call)
{
    int unitIndex;
    int parts;
    int value;

    if (VM_PLUGIN_CNT_GET() < 2) {
        sbChainSetValue(call, 2);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    parts = ((PartsUnit *) unitTblEntryGet(unitIndex))->parts;
    value = vmKeyedEntryGet(call, vmArgGet(call, 2));
    XenoActorHidePart(parts, value);
    return 1;
}

void XenoActorShowPart(int, int);

int partsShow(int call)
{
    int unitIndex;
    int parts;
    int value;

    if (VM_PLUGIN_CNT_GET() < 2) {
        sbChainSetValue(call, 2);
        return 0;
    }
    unitIndex = vmArgGet(call, 1);
    parts = ((PartsUnit *) unitTblEntryGet(unitIndex))->parts;
    value = vmKeyedEntryGet(call, vmArgGet(call, 2));
    XenoActorShowPart(parts, value);
    return 1;
}

int put(void) {
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu047", sampPadRead);


int sampFontPut(int call)
{
    int index;
    int slot;
    int raw;
    int key;

    if (VM_PLUGIN_CNT_GET() < 3) {
        sbChainSetValue(call, 2);
        return 0;
    }
    index = vmArgGet(call, 1);
    slot = vmArgGet(call, 2);
    raw = vmArgGet(call, 3);
    key = vmKeyedEntryGet(call, raw);
    xglFontPrint(index, slot, -1, key);
    return 1;
}

void sbNativeOwnerAdd(char *, char *);
void *memset(void *, int, unsigned int);
extern char D_00AB6078[];
extern char D_00AB6090[];
extern char D_00ABAC90[];
extern char D_00ABAC98[];

/* Registers two command tables with sbNativeOwnerAdd, then clears the state the runtime commands write: the two words of runEffSet, the BGM index, the light block and both vectors. */
void runtimeInitState(void)
{
    sbNativeOwnerAdd(D_00ABAC90, D_00AB6078);
    sbNativeOwnerAdd(D_00ABAC98, D_00AB6090);
    D_00AE0C2C = 0;
    D_00AE0C30 = 0;
    bgmIndex = 0;
    memset(lightBlock, 0, 0x180);
    memset(&runtimePos, 0, sizeof(Vector4));
    memset(&runtimeDir, 0, sizeof(Vector4));
}

void runtimeInitState(void);
extern char D_00AB60A0[];
extern char D_00ABACA0[];

/* Runtime start-up: resets the state, registers a third command table, then runs the two start calls. */
void runtimeInit(void)
{
    sbResetNativeCounts();
    runtimeInitState();
    sbNativeOwnerAdd(&D_00ABACA0, &D_00AB60A0);
    mathInit();
    cameraRegisterNatives();
}
