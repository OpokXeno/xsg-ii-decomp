#include "common.h"
#include "shared.h"
#include "main/sv_get_image_item.h"

extern DrawList svDrawList;

extern int svDrawListFlushCode;

void drawListSetFlushCode(int value)
{
    svDrawListFlushCode = value;
}

/* Base of the slot table. */
Quadword *drawListGetSlots(void)
{
    return svDrawList.entries;
}

/* The slot at end_index, the first one past the queued slots. */
Quadword *drawListGetNextSlot(void)
{
    return svDrawList.entries + svDrawList.end_index;
}

/* Advances end_index by one slot. */
void drawListCommitSlot(void)
{
    svDrawList.end_index = svDrawList.end_index + 1;
}

/* Advances end_index by increment slots. */
void drawListCommitSlots(int increment)
{
    svDrawList.end_index = svDrawList.end_index + increment;
}

/* Appends one slot passed by value. */
void drawListAppendSlot(Quadword value)
{
    Quadword *slot;

    /* the byte offset is formed first, then the data pointer is added */
    slot = (Quadword *) ((svDrawList.end_index * sizeof(Quadword)) + (int) svDrawList.entries);
    svDrawList.end_index = svDrawList.end_index + 1;
    *slot = value;
}

/* A slot seen as its two doublewords. */
typedef union DrawSlot {
    Quadword quad;
    u64 halves[2];
} DrawSlot;

/* Appends one slot given as its two doublewords, the low one first. */
void drawListAppendSlotHalves(u64 low, u64 high)
{
    DrawSlot *slot;

    /* the slot is written through the DrawSlot union view (the halves are u64 stores); entries is a Quadword * */
    slot = (DrawSlot *) &svDrawList.entries[svDrawList.end_index];
    slot->halves[0] = low;
    slot->halves[1] = high;
    svDrawList.end_index = svDrawList.end_index + 1;
}

/* Appends four consecutive slots copied from src. */
void drawListAppendFourSlots(const Quadword *src)
{
    Quadword *dst;

    /* the byte offset is formed first, then the data pointer is added */
    dst = (Quadword *) ((svDrawList.end_index * sizeof(Quadword)) + (int) svDrawList.entries);
    svDrawList.end_index = svDrawList.end_index + 4;
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

/* Appends one slot copied from src. */
void drawListAppendSlotCopy(const Quadword *src)
{
    svDrawList.entries[svDrawList.end_index++] = *src;
}
