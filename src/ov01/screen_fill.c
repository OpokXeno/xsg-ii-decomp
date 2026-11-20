#include "common.h"
#include "shared.h"
#include "ov01/ov01_task.h"
#include "ov01/screen_flash.h"
#include "ov01/screen_fill.h"
#include "ov01/gr_gp_init.h"

/* The running grey screen fill, 0 when none. */
ScreenTask *screenFillTask;

/* The ten menu panel row words that menuPanelRowsReset (menu.c) clears. */
int menuPanelRows[MENU_PANEL_ROW_COUNT];

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
