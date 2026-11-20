#include "common.h"
#include "shared.h"
#include "main/xgl_packet_scratch.h"
#include "main/xgl_packet.h"

static XglPacket *xglScratchPacket;
/* The batch: A+D pairs, the first one carries the GIF tag. */
static GsAdPair *xglScratchQuads;
static int xglScratchQuadCount;

/* Starts a scratchpad quadword batch: the current packet goes to
 * xglScratchPacket, the batch base is the EE scratchpad and the count is reset to zero. */
void xglPacketScratchBegin(void)
{
    xglScratchPacket = xglPacketGetCurrent();
    xglScratchQuads = (GsAdPair *)SCRATCHPAD_BASE;
    xglScratchQuadCount = 0;
}

/* Appends one 128-bit quadword (high, low) to the batch. */
void xglPacketScratchAddQuad(long long high, long long low)
{
    int newCount;
    GsAdPair *slot;

    newCount = xglScratchQuadCount + 1;
    /* The offset is added to the base as an integer: that operand order gives the original register use. */
    slot = (GsAdPair *)(xglScratchQuadCount * sizeof(GsAdPair) + (int)xglScratchQuads);
    slot->reg = high;
    slot->data = low;
    xglScratchQuadCount = newCount;
}

/* Adds count - 1 to the first doubleword of the batch, the tag written at its head. */
void xglPacketScratchPatchTag(void)
{
    xglScratchQuads->data += xglScratchQuadCount - 1;
}

/* Appends the batch to the current packet as a VIF1 direct HL packet. */
void xglPacketScratchAddDirect(void)
{
    sceVif1PkCnt(xglScratchPacket, 0);
    sceVif1PkOpenDirectHLCode(xglScratchPacket, 0);
    sceVif1PkAddDirectDataN(xglScratchPacket, xglScratchQuads, xglScratchQuadCount);
    sceVif1PkCloseDirectHLCode(xglScratchPacket);
}

/* Caps value at upper, then raises it to lower. */
int xglClampInt(int value, int lower, int upper)
{
    int clamped = value;

    if (upper < value) {
        clamped = upper;
    }
    return (clamped < lower) ? lower : clamped;
}

/* Wraps value once into lower..upper: one period (upper - lower + 1) down when above upper, one up when below lower. */
int xglWrapIntRange(int value, int lower, int upper)
{
    int period = upper - lower + 1;

    if (upper < value) {
        value -= period;
    }
    if (value < lower) {
        value += period;
    }
    return value;
}
