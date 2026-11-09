#include "common.h"
#include "shared.h"

/* Interrupt handlers of main/tu023. Each handler takes an index, maps it through xglIntcSlotMap to a
 * 16-byte record in xglIntcEventCounts and adds 1 to the word at its start. Nine of the ten handlers are the
 * same code; the table of handler addresses starts at 0x00644FC4 with 16-byte entries. */

/* EE timer 1 count, and VIF1_FBRST, whose STC bit cancels a VIF1 stall. */
#define T1_COUNT ((volatile u32 *) 0x10000800)
#define VIF1_FBRST ((volatile u32 *) 0x10003c10)
#define VIF_FBRST_STC 8

/* xglIntcEventCounts labels the count word of the first 16-byte record of
 * the INTC table (see tu024.c); the handlers bump the count of their slot. */
typedef struct XglIntrCountSlot {
    int count;
    unsigned char unmodeled_04[12];
} XglIntrCountSlot;

extern int xglIntcSlotMap[];
extern XglIntrCountSlot xglIntcEventCounts[];

/* Timer accumulator: intrTimerActive active flag, intrTimerStart start count, intrTimerAccum ticks total. */
extern int intrTimerActive;
extern int intrTimerStart;
extern int intrTimerAccum;

int func_001A4768(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A47A8(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A47E8(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4828(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

/* Timer accumulator. It reads the EE timer 1 count (0x10000800). When intrTimerActive is set it adds the
 * ticks since the start count to intrTimerAccum and clears the flag; otherwise it sets the flag and
 * records the start count. The same handler also bumps its counter record. */
int intrTimerAccumulate(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    if (intrTimerActive) {
        intrTimerAccum += *T1_COUNT - intrTimerStart;
        intrTimerActive = 0;
    } else {
        intrTimerStart = *T1_COUNT;
        intrTimerActive = 1;
    }
    *VIF1_FBRST = VIF_FBRST_STC;
    /* sync: orders the stores before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

/* Clears the timer accumulator. */
void intrTimerReset(void)
{
    intrTimerActive = 0;
    intrTimerStart = 0;
    intrTimerAccum = 0;
}

/* Returns the accumulated timer ticks. */
int intrTimerGetAccum(void)
{
    return intrTimerAccum;
}

int func_001A4918(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4958(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4998(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A49D8(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}

int func_001A4A18(int index)
{
    volatile int *slot = &xglIntcSlotMap[index];
    XglIntrCountSlot *record = &xglIntcEventCounts[*slot];

    record->count++;
    /* sync: orders the count store before the interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts in the handler context */
    __asm__ volatile("ei");
    return 0;
}
