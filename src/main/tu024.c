#include "common.h"
#include "shared.h"

/* One record of the INTC or DMAC table: the handler id AddIntcHandler or
 * AddDmacHandler returned, the event count the handler bumps, the INTC
 * cause or DMAC channel (a negative id ends the table) and the handler.
 * xglIntcEventCounts and xglDmaEventCounts label the count of record 0. */
typedef struct XglIntrRecord {
    int handle;
    int count;
    int id;
    int (*handler)(int cause);
} XglIntrRecord;

extern XglIntrRecord xglIntcRecords[];
extern XglIntrRecord xglDmaRecords[];
/* INTC cause -> slot of its record in the INTC table; -1 when none is registered.
 * Both slot maps are volatile: the interrupt handlers read them asynchronously while the
 * main thread (xglIntrInit and the enable/disable calls) writes and tests them. */
extern volatile int xglIntcSlotMap[];
/* DMAC channel -> slot of its record in the DMAC table; -1 when no handler is registered. */
extern volatile int xglDmaSlotMap[];

/* The INTC causes and DMAC channels the slot maps cover. */
#define XGL_INTC_CAUSES 11
#define XGL_DMAC_CHANNELS 16

int AddIntcHandler(int cause, int (*handler)(int cause), int next);
int AddDmacHandler(int channel, int (*handler)(int channel), int next);
void EnableIntc(unsigned int cause, int value);
void DisableIntc(unsigned int cause, int value);

/* Timer accumulator of tu023, cleared by xglIntrInit. */
extern int intrTimerActive;
extern int intrTimerStart;
extern int intrTimerAccum;


/* Interrupt and DMA event registry. Two tables of 16-byte records share one layout:
 * {handle, count, id, handler}. The handle comes back from AddIntcHandler or
 * AddDmacHandler (kernel services 0x10 and 0x12); the count is bumped by the
 * DMAC channel handlers below; the id is the INTC cause or the DMAC channel. */

/* DMAC channel event handler. Seven copies with identical bodies, one per DMAC channel
 * registered in the DMAC table (channels 0, 1, 2, 3, 4, 8 and 9). Each one bumps the
 * count of its channel's record, re-enables interrupts (ei) and returns 0. */
int xglDmaEventHandler(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4A98(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4AD8(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4B18(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4B58(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4B98(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4BD8(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

/* INTC cause -> its event count, or -1 when no handler is registered for the cause. */
int xglIntcGetEventCount(int cause)
{
    volatile int *slot = &xglIntcSlotMap[cause];
    int count = -1;

    if (*slot >= 0) {
        count = xglIntcRecords[*slot].count;
    }
    return count;
}

/* Enables INTC cause `index` through kernel service 0x14 (EnableIntc in the PS2 kernel
 * table; external reference) when the cause has a handler. Returns 1 when it did. */
int xglIntcEnable(unsigned int cause)
{
    int valid_cause = cause < XGL_INTC_CAUSES;
    int enabled = 0;
    volatile int *slot = &xglIntcSlotMap[cause];

    if (valid_cause && (*slot >= 0)) {
        EnableIntc(cause, 0);
        enabled = 1;
    }
    return enabled;
}

/* Disables INTC cause `index` through kernel service 0x15 (DisableIntc) when registered. */
int xglIntcDisable(unsigned int cause)
{
    int valid_cause = cause < XGL_INTC_CAUSES;
    int disabled = 0;
    volatile int *slot = &xglIntcSlotMap[cause];

    if (valid_cause && (*slot >= 0)) {
        DisableIntc(cause, 0);
        disabled = 1;
    }
    return disabled;
}

/* DMAC channel -> its event count, or -1 when no handler is registered for the channel. */
int xglDmaGetEventCount(int channel)
{
    volatile int *slot = &xglDmaSlotMap[channel];
    int count = -1;

    if (*slot >= 0) {
        count = xglDmaRecords[*slot].count;
    }
    return count;
}

int EnableDmac(int channel);
int DisableDmac(int channel);

/* Enables DMAC channel `channel` through kernel service 0x16 (EnableDmac) when a handler
 * is registered for it. Returns 1 when it did. */
int xglDmaEnable(unsigned int channel)
{
    int valid_channel = channel < 0x10;
    int enabled = 0;
    volatile int *slot = &xglDmaSlotMap[channel];

    if (valid_channel && (*slot >= 0)) {
        EnableDmac(channel);
        enabled = 1;
    }
    return enabled;
}

/* Disables DMAC channel `channel` through kernel service 0x17 (DisableDmac) when registered. */
int xglDmaDisable(unsigned int channel)
{
    int valid_channel = channel < 0x10;
    int disabled = 0;
    volatile int *slot = &xglDmaSlotMap[channel];

    if (valid_channel && (*slot >= 0)) {
        DisableDmac(channel);
        disabled = 1;
    }
    return disabled;
}

/* Registers the INTC handlers (AddIntcHandler, 0x10) and the DMAC handlers (AddDmacHandler,
 * 0x12) from their tables, fills in the slot maps and clears three globals. Called by
 * InitializeSystem. */
void xglIntrInit(void)
{
    int i;

    for (i = 0; xglIntcRecords[i].id >= 0; i++) {
        xglIntcSlotMap[xglIntcRecords[i].id] = i;
        xglIntcRecords[i].handle = AddIntcHandler(xglIntcRecords[i].id, xglIntcRecords[i].handler, 0);
    }
    for (i = 0; xglDmaRecords[i].id >= 0; i++) {
        xglDmaSlotMap[xglDmaRecords[i].id] = i;
        xglDmaRecords[i].handle = AddDmacHandler(xglDmaRecords[i].id, xglDmaRecords[i].handler, 0);
    }
    intrTimerActive = 0;
    intrTimerStart = 0;
    intrTimerAccum = 0;
}

/* Default xglHddInstall progress callback: does nothing and returns 0. */
int xglHddDummyCallback(int stage, int offset, void *param)
{
    return 0;
}
