#include "common.h"
#include "shared.h"
#include "main/sef.h"
#include "main/tu197.h"
#include "ov01/tu075.h"
/* Default packet capacity used when no custom buffer is supplied. */
#define MGS_PACKET_DEFAULT_CAPACITY 0x400

void MMathRandomSeed(int seed);
int sefResetWorkTables(void);

/* ov01 .bss of this file: the tick counter and the reset flag word. */
static int ov01TickCount;
static int D_00AED084;

/* Scene state reset, called by ov01SceneInit:
 * zeroes ov01LastValue and ov01TickCount, then runs the two init calls. */
void ov01SceneStateReset(void) {
    extern s16 ov01LastValue;
    extern int ov01TickCount;
    extern int D_00AED084;

    sefStartWithDefaultMemory();
    sefStartClipTestDisabled();
    ov01LastValue = 0;
    ov01TickCount = 0;
    D_00AED084 = 0;
    MMathRandomSeed(0);
    MSeObjInit();
}

void ov01SceneSystemsShutdown(void) {
    MSeObjShutdown();
    sefResetWorkTables();
}

/* Records value as the last value and counts one tick. */
void ov01LastValueSet(s16 value) {
    extern s16 ov01LastValue;
    extern int ov01TickCount;

    ov01LastValue = value;
    ov01TickCount += 1;
}

/* Last value recorded by ov01LastValueSet. */
s16 ov01LastValueGet(void) {
    extern s16 ov01LastValue;

    return ov01LastValue;
}

/* Ticks since the last reset. */
int ov01TickCountGet(void) {
    extern int ov01TickCount;

    return ov01TickCount;
}

void MGsGPInit(MGsPacket *packet, void *address, int size)
{
    void *base = (address != 0) ? address : (void *)SCRATCHPAD_BASE;

    packet->data = base;
    packet->capacity = (address != 0) ? size : MGS_PACKET_DEFAULT_CAPACITY;
    packet->current = base;
    packet->count = 0;
    packet->regCount = 0;
}

void sceVif1PkAddCode(void *packet, int code);
void sceVif1PkAddDirectDataN(void *packet, void *data, int qwc);
void sceVif1PkCloseDirectHLCode(void *packet);
void sceVif1PkCnt(void *packet, int count);
void sceVif1PkOpenDirectHLCode(void *packet, int count);
void *xglPacketGetCurrent(void);

/* Flushes the packet that MGsGPInit prepares: sends the queued direct data to VIF1 and resets the packet. */
void MGsGPTerm(MGsPacket *packet) {
    void *vifPacket;

    if (packet->count == 0) {
        packet->current = packet->data;
        packet->regCount = 0;
        return;
    }
    vifPacket = xglPacketGetCurrent();
    sceVif1PkCnt(vifPacket, 0);
    sceVif1PkAddCode(vifPacket, 0x11000000);
    sceVif1PkOpenDirectHLCode(vifPacket, 0);
    sceVif1PkAddDirectDataN(vifPacket, packet->data, packet->count);
    sceVif1PkCloseDirectHLCode(vifPacket);
    packet->current = packet->data;
    packet->count = 0;
    packet->regCount = 0;
}

void *memset(void *dest, int value, unsigned int size);

void MSeFireEntryClear(void *entry) {
    memset(entry, 0, 0x40);
}

/* Entry copied into an MSe object by MSeFire (64 bytes, copied as doublewords). */
typedef struct MSeFireEntry {
    int type;                       /* +0x00 */
    long long unmodeled_08[6];      /* +0x08 */
    int param;                      /* +0x38 */
    int unmodeled_3C;               /* +0x3C */
} MSeFireEntry;

/* One object of the MSe pool; slots are 0x800 bytes. */
typedef struct MSeObject {
    unsigned short flags;           /* +0x00 */
    unsigned short unmodeled_02;    /* +0x02 */
    int param;                      /* +0x04 */
    unsigned char unmodeled_08[0x18]; /* +0x08 */
    MSeFireEntry entry;             /* +0x20 */
} MSeObject;

typedef int (*MSeFireCallback)(MSeObject *object);

extern int MSeFireSpawnCount[];
extern MSeFireCallback MSeFireCallbacks[];
extern const char MSeFireBufferFullText[];
extern const char MSeFireCreateFailText[];
MSeObject *MSeObjAlloc(void);

void MSeFire(MSeFireEntry *entry) {
    MSeObject *object;
    MSeFireEntry *objectEntry;
    int remaining;

    remaining = MSeFireSpawnCount[entry->type];
    while (--remaining != -1) {
        object = MSeObjAlloc();
        if (object == 0) {
            MOutputDebugStringWarn(MSeFireBufferFullText);
            break;
        }
        object->param = entry->param;
        objectEntry = &object->entry;
        *objectEntry = *entry;
        if (MSeFireCallbacks[entry->type](object) == 0) {
            MSeObjRequestFree(&object->flags);
            MOutputDebugStringWarn(MSeFireCreateFailText, entry->type);
        }
    }
}

/* Effect constructors that MSeFireCallbacks names (defined in the other ov01 effect files). */
int MEfCreateWait();
int MEfCreateHermitePath();
int MEfCreateCounter();
int MEfCreatePathEffect25();
int MEfCreatePathEffect31();
int MEfCreatePathEffect29();
int MEfCreateFlagWindowEffect30();
int MEfCreateWaveEffect();
int MEfCreateSpray();
int MEfCreatePathStep();
int MEfCreateSway();
int MEfCreateStencilB();
int MEfCreateActorPath();
int MEfCreateActorHistory();

s16 ov01LastValue = 0;

MSeFireCallback MSeFireCallbacks[14] = {
    MEfCreateWait, MEfCreateHermitePath, MEfCreateCounter, MEfCreatePathEffect25, MEfCreatePathEffect31,
    MEfCreatePathEffect29, MEfCreateFlagWindowEffect30, MEfCreateWaveEffect, MEfCreateSpray, MEfCreatePathStep,
    MEfCreateSway, MEfCreateStencilB, MEfCreateActorPath, MEfCreateActorHistory,
};

int MSeFireSpawnCount[16] = {
    1, 8, 1, 1, 1, 1, 1, 1, 1, 8, 1, 1, 1, 1, 0, 0,
};

const char MSeFireBufferFullText[24] = "MSeFire: ObjBuffer full";
const char MSeFireCreateFailText[56] = "MSeFire: Failed to create an effect (ID:%d)";

const Quadword McEfSTQtbl[4] = {
    0x000000003f8000000000000000000000,
    0x000000003f8000003f80000000000000,
    0x000000003f800000000000003f800000,
    0x000000003f8000003f8000003f800000,
};
