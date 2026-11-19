/*
 * OV01 original TU 68: 0x00aa8c10..0x00aa8ea0 (3 functions)
 */
#include "common.h"
#include "shared.h"
#include "ov01/tu001.h"
#include "ov01/tu067.h"
#include "ov01/tu068.h"

/* The running grey screen fill, 0 when none. */
ScreenTask *screenFillTask;

/* The ten menu panel row words that menuPanelRowsReset (tu049.c) clears. */
int menuPanelRows[10];

/* Starts a grey screen fill that lasts until screenFillEnd. */
void screenFillStart(void)
{
    ScreenTask *task = ov01TaskClaim();

    if (task != 0)
    {
        task->update = 0;
        task->draw = screenFillGray;
        screenFillTask = task;
    }
}

/* Ends the grey screen fill started by screenFillStart. */
void screenFillEnd(void)
{
    if (screenFillTask != 0)
    {
        ov01TaskRelease(screenFillTask);
        screenFillTask = 0;
    }
}

typedef struct GrPacket {
    union GrGifQuad *data;
    int count;
} GrPacket;

typedef union GrGifWord {
    unsigned long long bits;
    unsigned int lanes[2];
} GrGifWord;

typedef struct GrGifOpenTag {
    GrGifWord control;
    GrGifWord register_id;
} GrGifOpenTag;

typedef struct GrScreenVertex {
    int x;
    int y;
    int z;
    int flags;
} GrScreenVertex;

typedef struct GrGifRGBA {
    int r;
    int g;
    int b;
    int a;
} GrGifRGBA;

typedef union GrGifQuad {
    GrGifOpenTag tag;
    GrGifRGBA rgba;
    GrScreenVertex xyz;
} GrGifQuad;

#define GR_PACKET_QUAD(packet, index) ((packet)->data + (index))

void grGpInit(GrPacket *packet);
void grPacketSend(GrPacket *packet);

/* Stores register_id as the register list of the open tag at the packet's current quad (count). */
static inline void grGifOpenRegister(GrPacket *packet, unsigned long long register_id)
{
    GrGifQuad *quads = packet->data;
    quads[packet->count].tag.register_id.bits = register_id;
}

/* GIF tag of the fill: EOP set, two register descriptors (NREG 2), two loops (NLOOP 2). */
#define GR_GIF_FILL_TAG 0x2003400000008002ULL
/* Register list of the fill's quads, low nibble first: RGBAQ (1), then XYZ2 (5). */
#define GR_GIF_REGS_RGBAQ_XYZ2 0x51
/* Alpha of the fill colour: 0x80, half of the 0..0xFF range. */
#define GR_FILL_ALPHA 0x80

/* Draws a fill of the given RGB colour over the screen at alpha GR_FILL_ALPHA (grey in screenFillGray). */
void grPacketScreenFill(int r, int g, int b)
{
    GrPacket packet;
    int *countPtr;

    grGpInit(&packet);
    grGifOpenRegister(&packet, GR_GIF_REGS_RGBAQ_XYZ2);
    countPtr = &packet.count;
    GR_PACKET_QUAD(&packet, (*countPtr)++)->tag.control.bits = GR_GIF_FILL_TAG;

    GR_PACKET_QUAD(&packet, packet.count)->rgba.r = r;
    GR_PACKET_QUAD(&packet, packet.count)->rgba.g = g;
    GR_PACKET_QUAD(&packet, packet.count)->rgba.b = b;
    GR_PACKET_QUAD(&packet, (*countPtr)++)->rgba.a = GR_FILL_ALPHA;

    GR_PACKET_QUAD(&packet, packet.count)->xyz.x = 0x7000;
    GR_PACKET_QUAD(&packet, packet.count)->xyz.y = 0x7200;
    GR_PACKET_QUAD(&packet, packet.count)->xyz.z = 0x00fffffd;
    GR_PACKET_QUAD(&packet, (*countPtr)++)->xyz.flags = 0;

    GR_PACKET_QUAD(&packet, packet.count)->rgba.r = r;
    GR_PACKET_QUAD(&packet, packet.count)->rgba.g = g;
    GR_PACKET_QUAD(&packet, packet.count)->rgba.b = b;
    GR_PACKET_QUAD(&packet, (*countPtr)++)->rgba.a = GR_FILL_ALPHA;

    GR_PACKET_QUAD(&packet, packet.count)->xyz.x = 0x9000;
    GR_PACKET_QUAD(&packet, packet.count)->xyz.y = 0x8e00;
    GR_PACKET_QUAD(&packet, packet.count)->xyz.z = 0x00fffffd;
    GR_PACKET_QUAD(&packet, (*countPtr)++)->xyz.flags = 0;

    grPacketSend(&packet);
}
