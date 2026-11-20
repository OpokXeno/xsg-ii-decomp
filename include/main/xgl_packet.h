#ifndef INCLUDE_MAIN_XGL_PACKET_H
#define INCLUDE_MAIN_XGL_PACKET_H

#include "main/xgl_camera.h"
#include "shared.h"

/* Functions of src/main/xgl_packet.c that other files call, and the sceVif1Pk* packet builder library the xgl
 * renderer drives them through (the SDK's argument widths: counts are unsigned). */

/* One GS A+D pair of a packet built in the scratchpad: the register data, then the register address. */
typedef struct GsAdPair {
    u64 data;
    u64 reg;
} GsAdPair;

/* One entry of a texture transfer set (X1: xglPacketTextureTrans), also read by xglTexUploadList: per entry a GS load-image header (BITBLTBUF/TRXREG words) plus a reference to the image data. */
typedef struct XglTextureTransEntry {
    u32 size;       /* low 16 bits: width in pixels; high 16 bits: width in 64-pixel blocks when it does not fit */
    u32 height;
    u32 vramOffset; /* added to the texture buffer base << 11, then >> 6 for the GS block address */
    u32 dataSize;   /* +0x0C: quadword count for sceVif1PkRef */
    u32 dataOffset; /* +0x10: from the set base */
} XglTextureTransEntry;

/* Texture transfer set (a script/package block): the entry table sits entryOffset bytes from the set base. */
typedef struct XglTextureTransSet {
    u8 unmodeled_00[8];
    int count;          /* +0x08 */
    u32 entryOffset;    /* +0x0C */
} XglTextureTransSet;

void xglPacketInit(u32 buffer, u32 size, float rate);
void xglPacketMove(void);
void xglPacketSend(void);
XglPacket *xglPacketGetCurrent(void);
void xglPacketInterpolate(void);
void xglPacketTextureTrans(XglTextureTransSet *set);

void sceVif1PkInit(XglPacket *packet, u32 buffer);
void sceVif1PkReset(XglPacket *packet);
u32 *sceVif1PkTerminate(XglPacket *packet);
void sceVif1PkEnd(XglPacket *packet, unsigned int tagFlags);
void sceVif1PkCnt(XglPacket *packet, unsigned int count);
void sceVif1PkAddCode(XglPacket *packet, unsigned int tag);
void sceVif1PkAddDataN(XglPacket *packet, const void *data, unsigned int qwc);
void sceVif1PkRef(XglPacket *packet, const void *data, unsigned int qwc, unsigned int a3, unsigned int a4,
                  unsigned int a5);
void sceVif1PkOpenDirectHLCode(XglPacket *packet, unsigned int count);
void sceVif1PkOpenDirectCode(XglPacket *packet, unsigned int count);
void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, unsigned int qwc);
void sceVif1PkCloseDirectHLCode(XglPacket *packet);
void sceVif1PkCloseDirectCode(XglPacket *packet);
void sceVif1PkOpenUpkCode(XglPacket *packet, unsigned int address, unsigned int format, unsigned int a3,
                          unsigned int a4);
void sceVif1PkCloseUpkCode(XglPacket *packet);
void sceVif1PkAddUpkData128N(XglPacket *packet, const void *data, unsigned int count);
void sceVif1PkCall(XglPacket *packet, unsigned int chain, unsigned int flag);
u32 sceVif1PkSize(XglPacket *packet);
void *sceVif1PkReserve(XglPacket *packet, unsigned int words);

#endif /* INCLUDE_MAIN_XGL_PACKET_H */
