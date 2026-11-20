#include "common.h"
#include "main/xgl_camera.h"
#include "shared.h"
#include "main/xgl_dma.h"
#include "main/xgl_packet.h"

#include "quadword.h"
#include "vu0_macro.h"

/* EE DMAC channel numbers: the index into the channel table (VIF1 = 1, GIF = 2). */
#define XGL_DMA_CHANNEL_VIF1 1

extern u16 sRenderTextureBase;
extern u64 textureTransBitbltAd[];
extern u64 textureTransRegionAd[];
extern u32 textureTransGifSetupTable[];

extern u16 sRenderDisplayBase;
extern u32 interpolateGifTable[];
extern u32 interpolatePrimTable[];

void FlushCache(int cacheOp);

static XglPacket *pCurrentPacket; /* .sbss 0x69E3A0 */
static XglPacket *pSendPacket; /* .sbss 0x69E3A4 */

void xglPacketTextureTrans(XglTextureTransSet *set) {
    XglTextureTransEntry *entry;
    int i;
    int blockWidth;
    int width;

    entry = (XglTextureTransEntry *)((u8 *)set + set->entryOffset);
    for (i = 0; i < set->count; i++) {
        width = entry->size & 0xFFFF;
        if (entry->size == width) {
            blockWidth = (width + 63) / 64;
        } else {
            blockWidth = entry->size >> 16;
        }
        textureTransBitbltAd[0] = ((u64)(((sRenderTextureBase << 11) + entry->vramOffset) >> 6) << 32) | ((u64)blockWidth << 48);
        textureTransRegionAd[0] = width | ((u64)entry->height << 32);
        sceVif1PkCnt(pCurrentPacket, 0);
        sceVif1PkAddDataN(pCurrentPacket, textureTransGifSetupTable, 28);
        sceVif1PkRef(pCurrentPacket, (u8 *)set + entry->dataOffset, entry->dataSize, 0, 0, 0);
        entry++;
    }
}

/* Direct-mode VIF1 block with a GS tag word; only caller xglRenderMove. X1: xglPacketInterpolate (TEX0_1 from the display buffer when ScanLineInterpolate == 2). */
void xglPacketInterpolate(void) {
    XglPacket *packet = pCurrentPacket;
    u64 *scratch;

    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, interpolateGifTable, 4);
    scratch = (u64 *)SCRATCHPAD_BASE;
    scratch[0] = ((sRenderDisplayBase << 5) | 0x24020000) | ((u64)0xC800 << 19);
    scratch[1] = 6;
    sceVif1PkAddDirectDataN(packet, scratch, 1);
    sceVif1PkAddDirectDataN(packet, interpolatePrimTable, 7);
    sceVif1PkCloseDirectCode(packet);
}

XglPacket *xglPacketGetCurrent(void)
{
    return pCurrentPacket;
}

/* Packet system init: both buffers through sceVif1PkInit, pCurrentPacket = packetSourceFirst, pSendPacket = 0. */
static XglPacket packetSourceFirst;
static XglPacket packetSourceSecond;

void xglPacketInit(u32 buffer, u32 size, float rate)
{
    float ratio = rate;
    int reserve = 0;

    if (ratio < 1.0f) {
        ratio = 1.0f;
    }
    if (ratio > 99.0f) {
        ratio = 99.0f;
    }
    reserve = (size >> 1) * ratio / 100.0f;
    if (reserve & 0xF) {
        reserve = reserve - (reserve & 0xF) + 0x10;
    }
    sceVif1PkInit(&packetSourceFirst, buffer);
    sceVif1PkInit(&packetSourceSecond, (size >> 1) + buffer);
    packetSourceFirst.bufferSize = size >> 1;
    packetSourceSecond.bufferSize = size >> 1;
    packetSourceFirst.buffer = buffer;
    packetSourceSecond.buffer = buffer + (size >> 1);
    packetSourceFirst.limit = (size >> 1) + buffer - reserve;
    packetSourceSecond.limit = (size >> 1) + buffer + (size >> 1) - reserve;
    packetSourceFirst.cursor = (u8 *) packetSourceFirst.limit;
    packetSourceSecond.cursor = (u8 *) packetSourceSecond.limit;
    pCurrentPacket = &packetSourceFirst;
    pSendPacket = 0;
}

/* Ends the current packet, makes it the send packet and switches to the other buffer. */
void xglPacketMove(void) {
    sceVif1PkEnd(pCurrentPacket, 0);
    sceVif1PkTerminate(pCurrentPacket);
    FlushCache(0);
    pCurrentPacket->cursor = (u8 *) pCurrentPacket->limit;
    pSendPacket = pCurrentPacket;
    if (pCurrentPacket == &packetSourceSecond) {
        pCurrentPacket = &packetSourceFirst;
    } else {
        pCurrentPacket = &packetSourceSecond;
    }
    sceVif1PkReset(pCurrentPacket);
}

void xglPacketSend(void)
{
    if (pSendPacket != 0) {
        xglDmaDirectSrcChain(XGL_DMA_CHANNEL_VIF1, pSendPacket->start);
    }
}

unsigned int textureTransGifSetupTable[12] = {
    0, 0, 0, 0x51000006, 32769, 0x50000000, 0xEEEEE, 0,
    0, 0, 63, 0
};

u64 textureTransBitbltAd[4] = { 0x0000000000000000ULL, 0x0000000000000050ULL, 0x0000000000000000ULL, 0x0000000000000051ULL };

u64 textureTransRegionAd[4] = { 0x0000000000000000ULL, 0x0000000000000052ULL, 0x0000000000000000ULL, 0x0000000000000053ULL };

unsigned int interpolateGifTable[20] = {
    32769, 0xB0AB4000, 0x31EEEEEE, 1333, 0, 0, 63, 0,
    0x7FC00A, 1788, 8, 0, 96, 0, 20, 0,
    0, 0, 6, 0
};

unsigned int interpolatePrimTable[28] = {
    0x31001, 0, 71, 0, 100, 64, 66, 0,
    128, 128, 128, 128, 0, 16, 0, 0,
    28664, 29175, 0, 0, 8192, 7184, 0, 0,
    36856, 36343, 0, 0
};
