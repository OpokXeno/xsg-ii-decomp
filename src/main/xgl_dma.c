#include "common.h"
#include "shared.h"
#include "main/xgl_dma.h"
#include "main/pad.h"

extern u8 *buffer;

/* Layout of the DMAC_CTRL register. */
typedef struct XglDmacControl {
    u32 dma_enable : 1;
    u32 cycle_steal_release : 1;
    u32 drain_mode : 2;
    u32 stall_source : 2;
    u32 stall_drain : 2;
    u32 release_cycle : 3;
    u32 reserved : 21;
} XglDmacControl;

/* EE DMAC global registers and the SPR-from channel registers.  DMAC_CTRL
 * is not volatile: its drain_mode updates are whole-word read-modify-writes. */
#define DMAC_CTRL      ((XglDmacControl *)0x1000E000)

#define DMAC_STAT      ((volatile u32 *)0x1000E010)
#define DMAC_RBSR      ((volatile u32 *)0x1000E040)
#define DMAC_RBOR      ((volatile u32 *)0x1000E050)
#define D8_CHCR        ((volatile u32 *)0x1000D000)
#define D8_MADR        ((volatile u32 *)0x1000D010)
#define D8_QWC         ((volatile u32 *)0x1000D020)
#define D8_SADR        ((volatile u32 *)0x1000D080)

/* DMA tag ID (bits 28..30 of the first tag word); the same names as main/xgl_dma.h. */
#define DMA_TAG_ID_CNT 0x10000000U

#define DMA_TAG_ID_REF 0x30000000U
#define DMA_TAG_ID_CALL 0x50000000U
#define DMA_TAG_ID_END 0x70000000U

/* Dn_CHCR bits. */
#define DMA_CHCR_DIR 0x1

#define DMA_CHCR_MOD_CHAIN 0x4
#define DMA_CHCR_TTE 0x40
#define DMA_CHCR_STR 0x100
#define DMA_CHANNEL_COUNT 10

/* xglDmaDirect* drive only the first three channels: VIF0, VIF1 and GIF. */
#define XGL_DMA_DIRECT_CHANNELS 3

/* DMAC_CTRL.MFD: which channel drains the MFIFO ring. */
#define DMAC_CTRL_MFD_NONE 0

#define DMAC_CTRL_MFD_GIF 3

/* MFIFO ring sizes and offsets are in quadwords of 16 bytes. */
#define MFIFO_QUADWORD 16

/* The DMAC takes scratchpad addresses (0x7000xxxx) as an offset with the SPR flag in bit 31;
 * the same names as main/xgl_dma.h. */
#define DMA_SPR_ADDRESS_HIGH 0x7000U

#define DMA_SPR_OFFSET_MASK 0x3fffU
#define DMA_SPR_FLAG 0x80000000U

/* D8_SADR is quadword aligned within the 16 KiB scratchpad. */
#define DMA_SADR_MASK 0x3ff0U

/*
 * The DMA channel table and the MFIFO state are provided by the scaffold data
 * for this TU.  Only the register slots touched by these routines are named.
 */
typedef struct XglDmaChannel {
    /* EE DMAC MMIO registers: CHCR +0x00, MADR +0x10, QWC +0x20, TADR +0x30.
     * The hardware updates CHCR while the polling loops wait for completion. */
    volatile u32 control;
    u32 reserved_04[3];
    volatile u32 memory_address;
    u32 reserved_14[3];
    volatile u32 quadword_count;
    u32 reserved_24[3];
    volatile u32 tag_address;
} XglDmaChannel;

typedef struct XglDmaBuffer {
    u32 *start;
    u32 *current;
} XglDmaBuffer;

/* The first word is the channel pointer; the second word's role is unknown. */
struct MfifoDrainStorage {
    XglDmaChannel *channel;
    u32 unmodeled_04;
};

extern void FlushCache(int mode);
extern int sceGsSyncPath(int mode, int timeout);


extern XglDmaChannel *tbl[12];
static struct MfifoDrainStorage mfifo_drain;

/* Declared here, not at the top: moving it up changes the original code. */
#define mfifo_drain mfifo_drain.channel

void xglDmaDirectNormal(u32 channel, u32 address, u32 count)
{
    XglDmaChannel *dma;

    if (channel >= XGL_DMA_DIRECT_CHANNELS)
        return;
    dma = tbl[channel];
    while (dma->control & DMA_CHCR_STR)
        ;
    dma->quadword_count = count;
    if ((address >> 16) != DMA_SPR_ADDRESS_HIGH)
        dma->memory_address = address;
    else
        dma->memory_address = (address & DMA_SPR_OFFSET_MASK) + DMA_SPR_FLAG;
    *DMAC_STAT = 1U << channel;
    dma->control = DMA_CHCR_STR | DMA_CHCR_TTE | DMA_CHCR_DIR;
}

void xglDmaDirectSrcChain(u32 channel, u32 address)
{
    XglDmaChannel *dma;

    if (channel >= XGL_DMA_DIRECT_CHANNELS)
        return;
    dma = tbl[channel];
    while (dma->control & DMA_CHCR_STR)
        ;
    dma->quadword_count = 0;
    if ((address >> 16) != DMA_SPR_ADDRESS_HIGH)
        dma->tag_address = address;
    else
        dma->tag_address = (address & DMA_SPR_OFFSET_MASK) + DMA_SPR_FLAG;
    *DMAC_STAT = 1U << channel;
    dma->control = DMA_CHCR_STR | DMA_CHCR_TTE | DMA_CHCR_MOD_CHAIN | DMA_CHCR_DIR;
}

void xglDmaBufferReset(XglDmaBuffer *buffer, u32 *address)
{
    buffer->start = address;
    buffer->current = address;
}

u32 *xglDmaBufferCnt(XglDmaBuffer *buffer, const u32 *source, u32 count)
{
    u32 *current = buffer->current;
    u32 word_index;

    current[0] = DMA_TAG_ID_CNT + count;
    current[1] = 0;
    current[2] = 0;
    current[3] = 0;
    current += 4;
    /* Copies count quadwords (count * 4 words) of the caller's data. */
    for (word_index = 0; word_index < count * 4; word_index++)
        *current++ = *source++;
    buffer->current = current;
    return current;
}

u32 *xglDmaBufferRef(XglDmaBuffer *buffer, u32 address, u32 count)
{
    u32 *current = buffer->current;

    current[0] = DMA_TAG_ID_REF + count;
    current[1] = address;
    current[2] = 0;
    current[3] = 0;
    current += 4;
    buffer->current = current;
    return current;
}

u32 *xglDmaBufferCall(XglDmaBuffer *buffer, u32 address)
{
    u32 *current = buffer->current;

    current[0] = DMA_TAG_ID_CALL;
    current[1] = address;
    current[2] = 0;
    current[3] = 0;
    current += 4;
    buffer->current = current;
    return current;
}

void xglDmaBufferRequest(XglDmaBuffer *buffer, u32 channel)
{
    u32 *current = buffer->current;

    current[0] = DMA_TAG_ID_END;
    current[3] = current[2] = current[1] = 0;
    FlushCache(0);
    xglDmaDirectSrcChain(channel, (u32)buffer->start);
    buffer->current = buffer->start;
}

void xglDmaMFIFOSetup(u32 address, u32 size, int channel)
{
    XglDmaChannel *drain;
    volatile u32 *ring_base_reg;
    volatile u32 *ring_size_reg;
    volatile u32 *spr_madr;

    if (channel >= XGL_DMA_DIRECT_CHANNELS)
        return;
    ring_base_reg = DMAC_RBOR;
    ring_size_reg = DMAC_RBSR;
    DMAC_CTRL->drain_mode = DMAC_CTRL_MFD_GIF;
    *ring_base_reg = address;
    *ring_size_reg = size - MFIFO_QUADWORD;
    drain = tbl[channel];
    mfifo_drain = drain;
    spr_madr = D8_MADR;
    *spr_madr = address;
    drain->tag_address = address;
    drain->quadword_count = 0U;
    drain->control = DMA_CHCR_STR | DMA_CHCR_MOD_CHAIN;
}

u32 xglDmaMFIFOKick(u32 spr_address, u32 count)
{
    u32 ring_size = *DMAC_RBSR + MFIFO_QUADWORD;
    XglDmaChannel **drain_slot = &mfifo_drain;
    XglDmaChannel *drain;
    u32 room;

    while (*D8_CHCR & DMA_CHCR_STR)
        ;
    *D8_SADR = spr_address & DMA_SADR_MASK;
    *D8_QWC = count;
    drain = *drain_slot;
    do {
        room = (drain->tag_address - *D8_MADR + ring_size) & (ring_size - MFIFO_QUADWORD);
        if (room == 0)
            room = ring_size;
    } while (!(count * MFIFO_QUADWORD < room));
    *D8_CHCR = DMA_CHCR_STR;
    return room;
}

void xglDmaMFIFOLeave(void)
{
    XglDmaChannel *drain;

    sceGsSyncPath(0, 0);
    DMAC_CTRL->drain_mode = DMAC_CTRL_MFD_NONE;
    while (*D8_CHCR & DMA_CHCR_STR)
        ;
    drain = mfifo_drain;
    while (drain->control & DMA_CHCR_STR)
        ;
}

/* Ten channel register blocks; the table holds two more zero slots. */
XglDmaChannel *tbl[12] = {
    (XglDmaChannel *)0x10008000, (XglDmaChannel *)0x10009000, (XglDmaChannel *)0x1000A000,
    (XglDmaChannel *)0x1000B000, (XglDmaChannel *)0x1000B400, (XglDmaChannel *)0x1000C000,
    (XglDmaChannel *)0x1000C400, (XglDmaChannel *)0x1000C800, (XglDmaChannel *)0x1000D000,
    (XglDmaChannel *)0x1000D400, 0, 0,
};

/* The two controller port records (0x80 bytes each; xglPadRead in xgl_pad.c fills them). */
PadRecord PadData[2] = { 0 };

/* Returns at once with no effect. Only caller: InitializeSystem. */
void xglDmaInitial(void) {
}
