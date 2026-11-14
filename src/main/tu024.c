#include "common.h"
#include "shared.h"
#include "main/tu024.h"

int AddIntcHandler(int cause, int (*handler)(int cause), int next);
int AddDmacHandler(int channel, int (*handler)(int channel), int next);
/* Declared with a second argument: the original clears $a1 before both syscalls, which the
 * one-argument SDK prototype would not do. */
void EnableIntc(unsigned int cause, int value);
void DisableIntc(unsigned int cause, int value);

/* Interrupt and DMA event registry. Two tables of 16-byte records share one layout:
 * {handle, count, id, handler}. The handle comes back from AddIntcHandler or
 * AddDmacHandler (kernel services 0x10 and 0x12); the count is bumped by the
 * DMAC channel handlers below; the id is the INTC cause or the DMAC channel. */

/* DMAC channel event handlers, one per channel registered in xglDmaRecords: VIF0, VIF1, GIF,
 * from IPU, to IPU, from SPR, to SPR (channels 0, 1, 2, 3, 4, 8, 9). The bodies are identical:
 * each bumps the count of its channel's record and returns 0. */
int xglDmaHandlerVif0(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglDmaHandlerVif1(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglDmaHandlerGif(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglDmaHandlerFromIpu(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglDmaHandlerToIpu(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglDmaHandlerFromSpr(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglDmaHandlerToSpr(int channel)
{
    int *count = &xglDmaRecords[xglDmaSlotMap[channel]].count;

    (*count)++;
    ExitHandler();
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

/* Enables INTC cause `cause` through kernel service 0x14 (EnableIntc) when the cause has a
 * handler. Returns 1 when it did. */
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

/* Disables INTC cause `cause` through kernel service 0x15 (DisableIntc) when registered. */
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
    int valid_channel = channel < XGL_DMAC_CHANNELS;
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
    int valid_channel = channel < XGL_DMAC_CHANNELS;
    int disabled = 0;
    volatile int *slot = &xglDmaSlotMap[channel];

    if (valid_channel && (*slot >= 0)) {
        DisableDmac(channel);
        disabled = 1;
    }
    return disabled;
}

/* Registers the INTC handlers (AddIntcHandler, 0x10) and the DMAC handlers (AddDmacHandler,
 * 0x12) from their tables, fills in the slot maps and clears the timer accumulator. Called by
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
