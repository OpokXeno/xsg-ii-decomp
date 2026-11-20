#include "common.h"
#include "shared.h"
#include "main/sef.h"
#include "main/m_se_obj.h"
#include "ov01/m_gs.h"
#include "main/xgl_packet.h"
#include "main/m_math.h"
/* Default packet capacity used when no custom buffer is supplied. */
#define MGS_PACKET_DEFAULT_CAPACITY 0x400


/* ov01 .bss of this file: the tick counter and the reset flag word (ov01SceneResetFlag is written and never read). */
static int ov01TickCount;
static int ov01SceneResetFlag;

/* Scene state reset, called by ov01SceneInit:
 * zeroes ov01LastValue and ov01TickCount, then runs the two init calls. */
void ov01SceneStateReset(void) {
    extern s16 ov01LastValue;
    extern int ov01TickCount;
    extern int ov01SceneResetFlag;

    sefInitEffect();
    sefInitEffectBattle();
    ov01LastValue = 0;
    ov01TickCount = 0;
    ov01SceneResetFlag = 0;
    MMathRandomSeed(0);
    MSeObjInit();
}

void ov01SceneSystemsShutdown(void) {
    MSeObjShutdown();
    sefDestroyEffect();
}

/* Records value as the pause switch of all effect families and counts one tick. */
void ov01EffectPauseSet(s16 value) {
    extern s16 ov01LastValue;
    extern int ov01TickCount;

    ov01LastValue = value;
    ov01TickCount += 1;
}

/* Pause switch recorded by ov01EffectPauseSet. */
s16 ov01EffectPauseGet(void) {
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

void MSeFireEntryClear(MSeFireEntry *entry) {
    memset(entry, 0, 0x40);
}

extern int MSeFireSpawnCount[];
extern MSeFireCallback MSeFireCallbacks[];
extern const char MSeFireBufferFullText[];
extern const char MSeFireCreateFailText[];

void MSeFire(MSeFireEntry *entry) {
    MSeObj *object;
    MSeFireEntry *objectEntry;
    int remaining;

    remaining = MSeFireSpawnCount[entry->type];
    while (--remaining != -1) {
        object = MSeObjAlloc();
        if (object == 0) {
            MOutputDebugStringWarn(MSeFireBufferFullText);
            break;
        }
        object->imageMapper = entry->param;
        objectEntry = &object->entry;
        *objectEntry = *entry;
        if (MSeFireCallbacks[entry->type](object) == 0) {
            MSeObjRequestFree(object);
            MOutputDebugStringWarn(MSeFireCreateFailText, entry->type);
        }
    }
}

/* The pause switch of all effect families (ov01EffectPauseSet stores it). */
s16 ov01LastValue = 0;

MSeFireCallback MSeFireCallbacks[14] = {
    MEfCreateWait, MEfCreateHermitePath, MEfCreateSpiral, MEfCreateThunder25, MEfCreateThunder31,
    MEfCreateTepodon, MEfCreateFlagWindow, MEfCreateWaveEffect, MEfCreateSpray, MEfCreatePathStep,
    MEfCreateSway, MEfCreateMissileTrail, MEfCreateActorPath, MEfCreateActorHistory,
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
