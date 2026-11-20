#ifndef INCLUDE_MAIN_XGL_DMA_HANDLER_H
#define INCLUDE_MAIN_XGL_DMA_HANDLER_H

/* Functions of src/main/xgl_dma_handler.c that other files call. */

/* The INTC and DMAC handler registry of src/main/xgl_dma_handler.c; the INTC handlers are in xgl_intc.c. */

/* One record of the INTC or DMAC table: the handler id AddIntcHandler or
 * AddDmacHandler returned, the event count the handler bumps, the INTC
 * cause or DMAC channel (a negative id ends the table) and the handler. */
typedef struct XglIntrRecord {
    int handle;
    int count;
    int id;
    int (*handler)(int cause);
} XglIntrRecord;

/* The INTC causes and DMAC channels the slot maps cover. */
#define XGL_INTC_CAUSES 11
#define XGL_DMAC_CHANNELS 16

extern XglIntrRecord xglIntcRecords[];
extern XglIntrRecord xglDmaRecords[];
/* INTC cause / DMAC channel -> slot of its record in the table; -1 when none is registered.
 * Both slot maps are volatile: the interrupt handlers read them asynchronously while the
 * main thread (xglIntrInit and the enable/disable calls) writes and tests them. */
extern volatile int xglIntcSlotMap[];
extern volatile int xglDmaSlotMap[];

/* Timer accumulator of the VIF1 handler (xgl_intc.c): active flag, start count, ticks total. */
extern int intrTimerActive;
extern int intrTimerStart;
extern int intrTimerAccum;

void xglIntrInit(void);
int xglIntcGetEventCount(int cause);
int xglIntcEnable(unsigned int cause);
int xglIntcDisable(unsigned int cause);
int xglDmaGetEventCount(int channel);
int xglDmaEnable(unsigned int channel);
int xglDmaDisable(unsigned int channel);

#endif /* INCLUDE_MAIN_XGL_DMA_HANDLER_H */
