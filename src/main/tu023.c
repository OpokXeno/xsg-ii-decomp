#include "common.h"
#include "shared.h"
#include "main/tu024.h"

/* The INTC handlers registered in xglIntcRecords (main/tu024.h), one per cause: GS, VBlank start,
 * VBlank end, VIF0, VIF1, VU0, VU1, IPU, timer 0, timer 1 (causes 0 and 2..10). Each bumps the
 * count of its cause's record; nine of the ten are the same code, the VIF1 one also runs the timer
 * accumulator. */

/* EE timer 1 count, and VIF1_FBRST, whose STC bit cancels a VIF1 stall. */
#define T1_COUNT ((volatile u32 *) 0x10000800)
#define VIF1_FBRST ((volatile u32 *) 0x10003c10)
#define VIF_FBRST_STC 8

int xglIntcHandlerGs(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglIntcHandlerVblankStart(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglIntcHandlerVblankEnd(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglIntcHandlerVif0(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

/* The VIF1 handler and timer accumulator. It reads the EE timer 1 count (0x10000800). When
 * intrTimerActive is set it adds the ticks since the start count to intrTimerAccum and clears the
 * flag; otherwise it sets the flag and records the start count. It also bumps its count and
 * cancels a VIF1 stall. */
int intrTimerAccumulate(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    if (intrTimerActive) {
        intrTimerAccum += *T1_COUNT - intrTimerStart;
        intrTimerActive = 0;
    } else {
        intrTimerStart = *T1_COUNT;
        intrTimerActive = 1;
    }
    *VIF1_FBRST = VIF_FBRST_STC;
    ExitHandler();
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

int xglIntcHandlerVu0(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglIntcHandlerVu1(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglIntcHandlerIpu(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglIntcHandlerTimer0(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}

int xglIntcHandlerTimer1(int index)
{
    int *count = &xglIntcRecords[xglIntcSlotMap[index]].count;

    (*count)++;
    ExitHandler();
    return 0;
}
