#include "common.h"
#include "shared.h"
#include "main/xgl_cd.h"
#include "ov01/data_unit_org_get.h"
#include "ov01/obj_slot.h"
#include "main/res_get_path.h"

/* The fourth argument is a completion callback: dataFileLoadNB passes dataNBreadCB in $7. */
/* Overlay data this TU reads: the CD pending count, and the heap range (ov01HeapStart, set by ov01HeapInit;
 * ov01HeapNextBlock compares against its end). */
static int ov01CdPendingCount;
/* Overlay heap size in bytes; ov01HeapInit reserves it from the base. */
#define OV01_HEAP_BYTES 0xa00000
/* Bytes of block header before each payload. */
#define OV01_HEAP_HEADER_SIZE 0x40
/* Bit 0 of a block's flags word: set while the block is in use. */
#define OV01_HEAP_FLAG_IN_USE 1

/* Pending CD read count. */
int dataCdSync(void)
{
    return ov01CdPendingCount;
}

/* Clears the pending CD read count; the overlay init calls it. */
void dataCdSyncClear(void)
{
    ov01CdPendingCount = 0;
}

/* Completion callback of dataFileLoadNB: code 4 ends one read and lowers the pending count. */
void dataNBreadCB(int code, int value, int tag)
{
    int count;

    if (code >= 0) {
        if (code == 4) {
            count = ov01CdPendingCount - 1;
            if (count < 0) {
                count = 0;
            }
            ov01CdPendingCount = count;
        }
    }
}

/* Starts one asynchronous CD read (CD_READ_ASYNC, completion callback dataNBreadCB); a successful start raises the pending count. */
u64 dataFileLoadNB(const char *name, void *buffer)
{
    long long result;

    result = xglCdReadFile(name, buffer, CD_READ_ASYNC, dataNBreadCB);
    if (result < 0) {
        return 0;
    }
    ov01CdPendingCount++;
    return 1;
}

/* Synchronous CD read (CD_READ_SYNC): true when xglCdReadFile reports success (>= 0). */
u64 dataFileLoad(const char *name, void *buffer)
{
    return xglCdReadFile(name, buffer, CD_READ_SYNC, CD_NOTIFY_CURRENT) >= 0;
}

/* Reserves the heap from the resource arena. */
void *ov01HeapBaseGet(unsigned int size)
{
    return GameResourceWorkAlloc(size);
}

/* Key lookup through GameResFindID: a result inside the heap range gives 0, any other result is returned. */
u32 ov01LookupNonHeap(int key)
{
    int size[4];
    u32 address;

    address = (u32) GameResFindID(key, size);
    if (address >= ov01HeapStart.first && address <= ov01HeapStart.end) {
        return 0;
    }
    return address;
}

/* Payload pointer to block header: the header sits 0x40 bytes before its payload. */
Ov01HeapBlock *ov01HeapBlockOf(void *payload)
{
    return (Ov01HeapBlock *) ((unsigned char *) payload - OV01_HEAP_HEADER_SIZE);
}

/* Heap init, called by the overlay init dataFileInit: base from ov01HeapBaseGet, 0xA00000 bytes, one free block. */
void ov01HeapInit(void)
{
    unsigned int size;
    Ov01HeapBlock *block;

    size = OV01_HEAP_BYTES;
    block = ov01HeapBaseGet(size);
    ov01HeapStart.end = (unsigned int) block + size;
    ov01HeapStart.first = (unsigned int) block;
    if (block != 0) {
        block->size = size;
        block->flags = 0;
    }
}

/*
 * Next block header after this one. Block sizes are always multiples of 0x40
 * (ov01HeapAlloc rounds them), so the rounding only trims the signed case.
 * NULL past the heap end.
 */
void *ov01HeapNextBlock(Ov01HeapBlock *block)
{
    int payloadSize;
    int roundedSize;
    unsigned char *nextBlock;

    payloadSize = block->size;
    roundedSize = payloadSize;
    if (payloadSize < 0) {
        roundedSize = payloadSize + 63;
    }
    nextBlock = (unsigned char *) block + ((roundedSize >> 6) << 6) + OV01_HEAP_HEADER_SIZE;
    if ((unsigned int) nextBlock >= ov01HeapStart.end) {
        return 0;
    }
    return nextBlock;
}

/* First fit: the first block whose flags AND mask are clear and whose size covers the request; NULL when none does. */
Ov01HeapBlock *ov01HeapFindFree(unsigned int mask, int size)
{
    Ov01HeapBlock *block;

    block = (Ov01HeapBlock *) ov01HeapStart.first;
    while (block != 0) {
        if ((block->flags & mask) == 0) {
            if (block->size >= size) {
                return block;
            }
        }
        block = ov01HeapNextBlock(block);
    }
    return 0;
}

/* Allocates size bytes (rounded up to 0x40) from the heap: splits a larger free block and returns the payload, NULL if nothing fits. */
void *ov01HeapAlloc(int size)
{
    int oldSize;
    Ov01HeapBlock *block;
    Ov01HeapBlock *remainder;

    size = (size + 63) / 64 * 64;
    block = ov01HeapFindFree(OV01_HEAP_FLAG_IN_USE, size);
    if (block == 0) {
        return 0;
    }
    oldSize = block->size;
    block->flags = 3;
    block->size = size;
    if (size + OV01_HEAP_HEADER_SIZE < oldSize) {
        remainder = ov01HeapNextBlock(block);
        remainder->flags = 0;
        remainder->size = oldSize - size - OV01_HEAP_HEADER_SIZE;
    }
    return (unsigned char *) block + OV01_HEAP_HEADER_SIZE;
}

/* Frees a payload: clears its in-use bit, then merges each run of free neighbours across the heap. */
void ov01HeapFree(void *payload)
{
    Ov01HeapBlock *block;
    Ov01HeapBlock *next;
    unsigned int heapStart;

    block = ov01HeapBlockOf(payload);
    heapStart = ov01HeapStart.first;
    if ((unsigned int) block < heapStart || (unsigned int) block >= ov01HeapStart.end) {
        return;
    }
    block->flags = 0;
    block = (Ov01HeapBlock *) heapStart;
    while (block != 0) {
        while ((block->flags & OV01_HEAP_FLAG_IN_USE) == 0) {
            next = ov01HeapNextBlock(block);
            if (next == 0 || (next->flags & OV01_HEAP_FLAG_IN_USE) != 0) {
                break;
            }
            block->size += next->size + OV01_HEAP_HEADER_SIZE;
        }
        block = ov01HeapNextBlock(block);
    }
}

/* Allocates 0x2A800 bytes and frees them again: 1 when that allocation fits in the heap, 0 otherwise. */
int ov01HeapProbeLargeAlloc(void)
{
    void *payload;
    Ov01HeapBlock *block;

    payload = ov01HeapAlloc(0x2a800);
    if (payload == 0) {
        return 0;
    }
    block = ov01HeapBlockOf(payload);
    block->flags = 0;
    return 1;
}

int ov01HeapDummy(void)
{
    return 1;
}
