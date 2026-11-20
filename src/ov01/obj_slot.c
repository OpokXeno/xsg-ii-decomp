#include "common.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "ov01/ov01_slot.h"
#include "ov01/ov01_task.h"
#include "ov01/mons_slot_run.h"
#include "ov01/unit_side_table.h"
#include "ov01/data_obj_table.h"
#include "ov01/obj_slot.h"
#include "ov01/obj.h"
#include "ov01/gr_gp_init.h"
#include "ov01/data_unit_org_get.h"

#define OBJ_SLOT_COUNT 64         /* slots in objSlotTable */
#define GS_ZBUF_1 0x4E            /* GS register ZBUF_1 (include/gs.h) */

struct ObjSlot;

/* One 0xB0-byte slot of the 64-slot table objSlotTable.
 * flags bit 0x1: in use (the claim in objSlotAlloc sets it, ov01SlotClear clears the word);
 * bit 0x2 skips the update pass (objSlotRunUpdate), bit 0x4 skips the draw pass (objSlotRunDraw). */
struct ObjSlot {
    int flags;
    void (*update)(struct ObjSlot *);  /* +0x04: first callback, run by the update pass */
    void (*draw)(struct ObjSlot *);    /* +0x08: second callback, run by the draw pass */
    unsigned char unmodeled_0c[0xA4];
};

/* Releases the data object (dataObjRelease): its only caller is objCurRemove (battle_init.c), which clears dataObject
 * after the call. Byte-identical to objSlotRelease (same body, same callee); the caller tells them apart. */
void dataObjRelease(void *object) {
    ov01SlotRelease(object);
}

static struct ObjSlot objSlotTable[OBJ_SLOT_COUNT];

/* objSlotClearAll: clears all OBJ_SLOT_COUNT slots. */
void objSlotClearAll(void)
{
    int i;

    for (i = 0; i < OBJ_SLOT_COUNT; i++) {
        ov01SlotClear(&objSlotTable[i]);
    }
}

/* objSlotRunUpdate: calls the update callback of each slot that has one and is not suspended (flags bit 0x2). */
void objSlotRunUpdate(void)
{
    int i;
    struct ObjSlot *slot;

    for (i = 0; i < OBJ_SLOT_COUNT; i++) {
        slot = &objSlotTable[i];
        if (slot->update != 0 && !(slot->flags & OV01_SLOT_SKIP_UPDATE)) {
            slot->update(slot);
        }
    }
}

/* objSlotRunDraw: calls the draw callback of each slot that has one and is not skipped (flags bit 0x4). */
void objSlotRunDraw(void)
{
    int i;
    struct ObjSlot *slot;

    for (i = 0; i < OBJ_SLOT_COUNT; i++) {
        slot = &objSlotTable[i];
        if (slot->draw != 0 && !(slot->flags & OV01_SLOT_SKIP_DRAW)) {
            slot->draw(slot);
        }
    }
}

/* objSlotAlloc: returns the first slot that the claim (ov01SlotClaim) takes, or 0. */
void *objSlotAlloc(void)
{
    int i;

    for (i = 0; i < OBJ_SLOT_COUNT; i++) {
        if (ov01SlotClaim(&objSlotTable[i].flags)) {
            return &objSlotTable[i];
        }
    }
    return 0;
}

/* Byte-identical to dataObjRelease; objSlotRelease is the slot's release, used by the menu panel steps. It is int because
 * menuComboPanelExtraStep returns the v0 this tail-call stub leaves untouched. */
int objSlotRelease(void *slot) {
    ov01SlotRelease(slot);
}


/* ov01ClearTables: clears the other ov01 tables, then the object table. monsSlotClearAll is defined in
 * mons_slot_run.c. */
void ov01ClearTables(void) {
    ov01TaskClearAll();
    monsSlotClearAll();
    unitSideClearAll();
    dataObjResetAll();
    objSlotClearAll();
}

/* ov01ResetAll: clears the two-slot callback table of ov01_slot.c, then every table (ov01ClearTables). */
void ov01ResetAll(void) {
    ov01CallbackSlotsClear();
    ov01ClearTables();
}

/* ov01UpdateAll: first callbacks of the five tables, then the object table (objSlotRunUpdate). */
void ov01UpdateAll(void) {
    ov01CallbackSlotsRunUpdate();
    ov01TaskRunUpdate();
    monsSlotRunUpdate();
    unitSideRunUpdate();
    dataObjUpdateAll();
    objSlotRunUpdate();
}


/* ov01DrawAll: a GS packet with the ZBUF_1 register (GS_ZBUF_1) set, the second callbacks
 * of the five tables, then the object table's draw pass (objSlotRunDraw). It is the scene's draw hook
 * (sRender.fade_callback, set by ov01SceneInit) and does not use the hook's packet. */
void ov01DrawAll(struct XglPacket *hookPacket)
{
    GrPacket packet;
    unsigned long long zbufWord;

    grGpInit(&packet);
    zbufWord = 0x9880;
    grGsRegSet(&packet, GS_ZBUF_1, zbufWord << 0x11);
    grPacketSend(&packet);
    monsSlotRunDraw();
    unitSideRunDraw();
    dataObjDrawAll();
    objSlotRunDraw();
    ov01TaskRunDraw();
}

/* ov01StackNew: takes a save stack from ov01CallbackSlotClaim (ov01_slot.h) and resets its depth and top. */
struct SaveStack *ov01StackNew(void) {
    struct SaveStack *stack;

    stack = ov01CallbackSlotClaim();
    if (stack != 0) {
        stack->count = 0;
        stack->top = 0;
        stack->unmodeled_08 = 0;
    }
    return stack;
}

/* Sets the successor step of a battle step context (ov01StepSetNext, obj_slot.c): stores nextStep at +0x04.
 * Its callers pass step functions. */
void ov01StepSetNext(struct SaveStack *stack, SaveStackStep nextStep) {
    stack->step = nextStep;
}

/* ov01StackPush: keeps the top in the cell below the new level and makes value the top.
 * The depth stops at SAVE_STACK_LEVELS - 1; a push that reaches the limit leaves the top alone. */
void ov01StackPush(struct SaveStack *stack, SaveStackStep value)
{
    int index;
    int *savedTop;

    index = stack->count;
    stack->count = index + 1;
    savedTop = &stack->cells[index * SAVE_STACK_LEVEL_CELLS - 1];
    *savedTop = stack->top;
    if (index + 1 >= SAVE_STACK_LEVELS) {
        stack->count = SAVE_STACK_LEVELS - 1;
        return;
    }
    stack->step = value;
}

/* ov01StackPop: restores the top kept by the last push. */
void ov01StackPop(struct SaveStack *stack)
{
    int index;
    int *savedTop;

    index = stack->count - 1;
    stack->count = index;
    if (index < 0) {
        stack->count = 0;
        return;
    }
    savedTop = &stack->cells[index * SAVE_STACK_LEVEL_CELLS - 1];
    stack->top = *savedTop;
}

/* ov01StackSetSlot: writes value to slot of the current level; returns the cell's byte offset. */
int ov01StackSetSlot(struct SaveStack *stack, int slot, int value) {
    int cellIndex;
    int *cell;

    cellIndex = stack->count * SAVE_STACK_LEVEL_CELLS + slot;
    cell = &stack->cells[cellIndex];
    *cell = value;
    return cellIndex * 4;
}

/* ov01StackGetSlot: reads slot of the current level. */
int ov01StackGetSlot(struct SaveStack *stack, int slot)
{
    int *cell = &stack->cells[stack->count * SAVE_STACK_LEVEL_CELLS + slot];
    return *cell;
}

/* fifoInit: an empty ring over `capacity` slots (X1 fifoInit in obj.c builds the same ring). */
void fifoInit(struct FifoRing *ring, int capacity) {
    ring->maxIndex = capacity - 1;
    ring->base = 0;
    ring->readIndex = 0;
    ring->writeIndex = 0;
    ring->count = 0;
}

/* fifoPush: reserves the next write slot, or returns -1 when the ring is full. */
int fifoPush(struct FifoRing *ring) {
    int nextWriteIndex;

    if (ring->count >= ring->maxIndex - ring->base + 1) {
        return -1;
    }
    ring->count++;
    nextWriteIndex = ring->writeIndex + 1;
    ring->writeIndex = nextWriteIndex;
    if (ring->maxIndex < nextWriteIndex) {
        ring->writeIndex = ring->base;
    }
    return ring->writeIndex;
}

/* fifoPop: releases the read slot and returns the new read index, or -1 when the ring is empty. */
int fifoPop(struct FifoRing *ring) {
    int nextReadIndex;

    if (ring->count <= 0) {
        return -1;
    }
    nextReadIndex = ring->readIndex + 1;
    ring->count--;
    ring->readIndex = nextReadIndex;
    if (ring->maxIndex < nextReadIndex) {
        ring->readIndex = ring->base;
    }
    return ring->readIndex;
}

/* Reads the write index (+0x0C, the tail) of the command fifo ring (fifoEdGet): cmdListSet indexes cmdList
 * with it before fifoPush. */
int fifoEdGet(struct FifoRing *ring) {
    return ring->writeIndex;
}

/* Reads the read index (+0x08, the head) of the command fifo ring (fifoStGet): battleExecNorm indexes
 * cmdList with it before fifoPop. */
int fifoStGet(struct FifoRing *ring) {
    return ring->readIndex;
}

/* Reads the count (+0x10) of the command fifo ring (fifoNumGet): battleExecNorm returns when it is zero. */
int fifoNumGet(struct FifoRing *ring) {
    return ring->count;
}

/* objCmdClear: empties the block (lastIndex = -1, cursor = 0). */
void objCmdClear(struct DataLookup *lookup)
{
    lookup->lastIndex = -1;
    lookup->cursor = 0;
    lookup->flag = 0;
}

/* objCmdPtrGet: the item at the cursor, or 0 when the cursor is past lastIndex. */
void *objCmdPtrGet(struct DataLookup *lookup)
{
    if (lookup->lastIndex < lookup->cursor) {
        return 0;
    }
    return &lookup->items[lookup->cursor];
}

/* dataLookupAppend: stores the 0x18-byte record at the next index and returns its item.
 * Past DATA_LOOKUP_CAPACITY items it returns 0 but keeps counting lastIndex. */
struct DataLookupItem *dataLookupAppend(struct DataLookup *lookup, void *record)
{
    struct DataLookupItem *source = record;
    struct DataLookupItem *item;

    lookup->lastIndex++;
    if (lookup->lastIndex >= DATA_LOOKUP_CAPACITY) {
        return 0;
    }
    *(lookup->items + lookup->lastIndex) = *source;
    item = lookup->items + lookup->lastIndex;
    item->next.word = 0;
    return lookup->items + lookup->lastIndex;
}

/* objCmdNext: advances the cursor; past lastIndex, resets the block and returns 0. */
void *objCmdNext(struct DataLookup *lookup) {
    int nextIndex;
    int exhausted;

    nextIndex = lookup->cursor + 1;
    exhausted = lookup->lastIndex < nextIndex;
    lookup->cursor = nextIndex;
    if (exhausted) {
        objCmdClear(lookup);
        return 0;
    }
    lookup->flag = 0;
    return &lookup->items[nextIndex];
}

/* dataLookupIsEmpty: 1 when the block's lastIndex word is negative (empty, after a reset); the sign bit is read as an
 * unsigned shift, so the index is viewed unsigned. */
u32 dataLookupIsEmpty(struct DataLookup *lookup) {
    return (u32)lookup->lastIndex >> 31;
}

/* rnd: uniform integer in 0..max. The same name and signature are extern in the other ov01 TUs and in X1. */
int rnd(int max)
{
    return xglSRand() % (max + 1);
}

/* ov01RollPercent: 1 with probability percent/100 (rnd(99) + 1 is 1..100). */
int ov01RollPercent(int percent)
{
    if (percent < rnd(99) + 1) {
        return 0;
    }
    return 1;
}

void *memset(void *s, int c, unsigned int n);

/* objStdInit: zeroes the 0x80-byte slide block, then sets position.w and the four scale values to 1.0; the reason
 * for that pattern is not shown in this TU. */
void objStdInit(SlideBlock *block)
{
    memset(block, 0, sizeof(*block));
    block->position.w = 1.0f;
    block->scale.w = 1.0f;
    block->scale.z = 1.0f;
    block->scale.y = 1.0f;
    block->scale.x = 1.0f;
}

/* Overlay heap range (ov01HeapInit sets it, data_unit_org_get.c reads it). */
struct Ov01HeapRange ov01HeapStart;
