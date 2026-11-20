#include "common.h"
#include "main/assert_fail.h"
#include "shared.h"
#include "main/pad.h"
#include "main/test_128.h"
#include "main/vu0_matrix.h"

/* Heap count of the heap windows (the assert text names it HEAP_MAX). */
#define HEAP_MAX 6
/* Size of one heap arena (the 2048 bytes InitRgHeap is given). */
#define HEAP_ARENA_SIZE 0x800
/* Smallest block a heap keeps: the 0x40-byte RgHeapBlock header plus one byte. */
#define HEAP_MIN_BLOCK_SIZE 65
/* Length of the block magic stamp (RgHeapBlock.magic holds it plus a terminator). */
#define HEAP_MAGIC_SIZE 15
/* Heap map: 256 pixels wide, painted over the arena in colour HEAP_MAP_ARENA_COLOR and the blocks in HEAP_MAP_BLOCK_COLOR. */
#define HEAP_MAP_WIDTH 256
#define HEAP_MAP_ARENA_COLOR 0x80000080
#define HEAP_MAP_BLOCK_COLOR 0x80008080
/* Window placement: the per-heap window (heapWindowCreate), the allocation position window (heapWindowOpen)
 * and the overview of all heaps (heapOverviewWindowOpen). */
#define HEAP_WINDOW_X 100
#define HEAP_WINDOW_Y 80
#define HEAP_WINDOW_WIDTH 256
#define HEAP_WINDOW_HEIGHT 300
#define HEAP_ALLOC_WINDOW_X 10
#define HEAP_ALLOC_WINDOW_Y 120
#define HEAP_ALLOC_WINDOW_WIDTH 480
#define HEAP_ALLOC_WINDOW_HEIGHT 200
#define HEAP_OVERVIEW_WINDOW_X 100
#define HEAP_OVERVIEW_WINDOW_Y 100
#define HEAP_OVERVIEW_WINDOW_WIDTH 200
#define HEAP_OVERVIEW_WINDOW_HEIGHT 140
/* The map stride clamp range of heapWindowMapStrideAdjust (1.0 is 0x10000). */
#define TEXT_SCALE_MIN 1
#define TEXT_SCALE_MAX 0x10000

/*
 * RgHeapBlock: the fixed 0x40-byte header RgHeapAlloc places immediately
 * before the block it returns (RgHeapDumpBlock reads it at pPtr - 0x40).
 * pre/next link the block into whichever circular list currently owns it
 * (RgHeap's free chain from _SetHeapHead, or its allocated chain from
 * _Link/_InsertNext), evidenced by "pre %p next %p\n". size is the block's
 * byte size and mark is set/cleared by _MarkAlloc/_UnmarkAlloc/_IsAllocated,
 * evidenced by "size %p mark %d\n" (size itself is dumped through %p in that
 * string). module and line record the RgHeapAlloc call site (module is
 * strncpy'd from its source_file argument and terminated at module[31]),
 * evidenced by "module '%s' line %d\n". The dumps print module + 1, and
 * RgHeapFree reads module[0] as the owning heap id before it looks the heap
 * up. magic holds the 15-character
 * s_szMagicString stamp _SetMagicString/_IsCollectMagicString read and write
 * at block+0x30. Every byte from 0x00 to 0x3F is accounted for by these six
 * members (4+4+4+2+2+32+16 == 0x40).
 */
struct RgHeapBlock {
    struct RgHeapBlock *pre;
    struct RgHeapBlock *next;
    u32 size;
    u16 mark;
    u16 line;
    char module[32];
    char magic[16];
};

/*
 * The heap head: top and size are printed as "heap = %p top = %p size = %d",
 * +0x08 is the top of the free chain that the dump prints as "FREE MAP top",
 * +0x0C heads the allocated chain. +0x10 (mapStride) is stored as 0x100 by
 * InitRgHeap; heapWindowPaintMap uses it both as the map row length and as the
 * bytes per map cell.
 */
struct RgHeap {
    void *top;
    int size;
    struct RgHeapBlock *freeHead;
    struct RgHeapBlock *allocHead;
    int mapStride;
};

int printf(const char *format, ...);
char *strcpy(char *destination, const char *source);
void testWindowSetColumnTable(int, void *);
void RgHeapDump_sub(struct RgHeap *pHeap, const char *pComment, const char *pSourceFile, int line);
extern const char heapAssertHeaderFormat[];
extern const char heapAssertExpressionFormat[];
extern const char heapAssertHeapInfoFormat[];
extern const char heapAssertDumpComment[];
extern const char heapSourcePath[];
extern const char heapAssertHeapNilFormat[];

/*
 * Heap assertion handler: prints the failed expression with its call site,
 * dumps the heap when one is given, then spins forever. The second argument
 * is the tag the callers pass ("make block", "init-2"); this build never
 * prints it.
 */
void _Error(const char *pExpression, const void *unused, struct RgHeap *pHeap,
                   const char *pSourceFile, int line)
{
    printf(heapAssertHeaderFormat);
    printf(heapAssertExpressionFormat, pExpression, pSourceFile, line);
    if (pHeap != 0) {
        printf(heapAssertHeapInfoFormat, pHeap, pHeap->top, pHeap->size);
        RgHeapDump_sub(pHeap, heapAssertDumpComment, heapSourcePath, 0xA1);
    } else {
        printf(heapAssertHeapNilFormat);
    }
    for (;;) {
    }
}

void _Link(struct RgHeapBlock *pBlock, struct RgHeapBlock *pPre,
                  struct RgHeapBlock *pNext)
{
    pBlock->pre = pPre;
    pBlock->next = pNext;
}

void _InsertPre(struct RgHeapBlock *pBlock, struct RgHeapBlock *pNew)
{
    _Link(pNew, pBlock->pre, pBlock);
    pBlock->pre->next = pNew;
    pBlock->pre = pNew;
}

void _InsertNext(struct RgHeapBlock *pBlock, struct RgHeapBlock *pNew)
{
    _Link(pNew, pBlock, pBlock->next);
    pBlock->next->pre = pNew;
    pBlock->next = pNew;
}

void _Unlink(struct RgHeapBlock *pBlock)
{
    pBlock->next->pre = pBlock->pre;
    pBlock->pre->next = pBlock->next;
}

void _MarkAlloc(struct RgHeapBlock *pBlock)
{
    pBlock->mark = 1;
}

void _UnmarkAlloc(struct RgHeapBlock *pBlock)
{
    pBlock->mark = 0;
}

int _IsAllocated(struct RgHeapBlock *pBlock)
{
    return pBlock->mark != 0;
}

extern unsigned char *heapMagicStringPtr;

/* Stamps the 15-byte magic (read through the heapMagicStringPtr pointer) into the block header. */
void _SetMagicString(struct RgHeapBlock *pBlock)
{
    char *pMagic = pBlock->magic;
    int index = 0;

    do {
        *pMagic = heapMagicStringPtr[index];
        index += 1;
        pMagic += 1;
    } while (index < HEAP_MAGIC_SIZE);
}

/* Returns 1 when the 15 magic bytes at block+0x30 equal the stamp heapMagicStringPtr points to, else 0. */
int _IsCollectMagicString(struct RgHeapBlock *pBlock)
{
    int index = 0;

    do {
        if ((unsigned char)pBlock->magic[index] != (signed char)heapMagicStringPtr[index]) {
            return 0;
        }
        index += 1;
    } while (index < HEAP_MAGIC_SIZE);
    return 1;
}

extern const char heapAssertHeadSizeBelowBlockSize[];
extern const char heapMakeBlockComment[];

struct RgHeapBlock *_SetHeapHead(struct RgHeapBlock *pBlock, u32 size)
{
    if (size < HEAP_MIN_BLOCK_SIZE) {
        _Error(heapAssertHeadSizeBelowBlockSize, heapMakeBlockComment, 0, heapSourcePath, 269);
    }
    _Link(pBlock, pBlock, pBlock);
    pBlock->size = size;
    return pBlock;
}

/*
 * Walks the circular free list once and folds every block that ends exactly
 * where its successor starts into that block.
 */
void _MergeBlocks(struct RgHeapBlock *pHead)
{
    struct RgHeapBlock *pBlock = pHead;
    struct RgHeapBlock *pNext;

    if (pHead == 0) {
        return;
    }
    do {
        pNext = pBlock->next;
        while ((u8 *)pBlock + pBlock->size == (u8 *)pNext) {
            pBlock->next = pNext->next;
            pBlock->size += pNext->size;
            pNext->next->pre = pBlock;
            pNext = pBlock->next;
        }
        pBlock = pNext;
    } while (pBlock != pHead);
}

/* InstanceOfRgHeapById: the heap heapId (0x20 bytes apart); its 0x800-byte arena is set up by the first call, tracked by a halfword flag per id. */
static unsigned char heapInstances[HEAP_MAX][0x20];
static unsigned char heapArenas[HEAP_MAX][HEAP_ARENA_SIZE];
/* Not heap data: sefAppliedLightSet is the applied effect light set used by sef_light_set.c and sv_get_image_item.c;
   the original layout places it among this file's heap arrays. */
static unsigned char sefAppliedLightSet[384];
extern short heapInitFlags[8];

struct RgHeap *InstanceOfRgHeapById(u32 heapId)
{
    if (heapInitFlags[heapId] == 0) {
        heapInitFlags[heapId] = 1;
        InitRgHeap((struct RgHeap *)heapInstances[heapId], heapArenas[heapId], HEAP_ARENA_SIZE);
    }
    return (struct RgHeap *)heapInstances[heapId];
}

void ClearRgHeap(struct RgHeap *pHeap)
{
    pHeap->freeHead = _SetHeapHead(pHeap->top, pHeap->size);
}

extern const char heapAssertBufSizeAligned[];
extern char heapInitStageComment[];
void *memset(void *destination, int value, unsigned int count);

void InitRgHeap(struct RgHeap *pHeap, void *pMemory, int size)
{
    struct RgHeapBlock *pFreeBlock;

    if (size & 0xF) {
        _Error(heapAssertBufSizeAligned, heapInitStageComment, 0, heapSourcePath, 321);
    }
    memset(pMemory, 0, size);
    pHeap->size = size;
    pHeap->top = pMemory;
    pFreeBlock = _SetHeapHead(pMemory, size);
    pHeap->allocHead = 0;
    pHeap->freeHead = pFreeBlock;
    pHeap->mapStride = 0x100;
}

extern const char heapAllocZeroSizeFormat[];
extern const char heapAssertHeapNotNilParenthesized[];
extern const char heapAllocEntryHeapPtrComment[];
extern const char heapAllocNoHeapFormat[];
extern const char heapNoHeapIdFormat[];
extern const char heapAllocTooBigDataHeapFormat[];
extern const char heapAssertMatchBlockSizeEnough[];
extern const char heapBlockAlignComment[];
extern const char heapAssertAllocAligned[];
extern const char heapNotAlignedBlockComment[];
int sprintf(char *destination, const char *format, ...);
void strncpy(char *pDest, const char *pSrc, u32 count);

void *RgHeapAllocById(u32 heapId, u32 size, const char *pSourceFile, int line)
{
    char buf[32];
    struct RgHeap *pHeap;
    struct RgHeapBlock *pFreeHead;
    struct RgHeapBlock *pBlock;
    struct RgHeapBlock *pFit;
    struct RgHeapBlock *pSplit;
    u32 bestSize;
    u32 blockSize;
    u32 remainder;
    void *pPayload;

    pHeap = InstanceOfRgHeapById(heapId);
    if (size == 0) {
        printf(heapAllocZeroSizeFormat, pSourceFile, line);
    }
    if (pHeap == 0) {
        _Error(heapAssertHeapNotNilParenthesized, heapAllocEntryHeapPtrComment, 0, heapSourcePath, 347);
    }
    pFreeHead = pHeap->freeHead;
    if (pFreeHead == 0) {
        printf(heapAllocNoHeapFormat, pSourceFile, line);
        return 0;
    }
    pFit = 0;
    blockSize = ((size + 15) & -16) + sizeof(struct RgHeapBlock);
    bestSize = 0;
    pBlock = pFreeHead;
    do {
        if (pBlock->size >= blockSize) {
            if (pBlock->size < bestSize || bestSize == 0) {
                bestSize = pBlock->size;
                pFit = pBlock;
            }
        }
        pBlock = pBlock->next;
    } while (pBlock != pFreeHead);
    if (pFit == 0) {
        sprintf(buf, heapNoHeapIdFormat, heapId);
        RgHeapDump_sub(pHeap, buf, heapSourcePath, 375);
        printf(heapAllocTooBigDataHeapFormat, size, pSourceFile, line);
        return 0;
    }
    if (bestSize < blockSize) {
        _Error(heapAssertMatchBlockSizeEnough, heapBlockAlignComment, pHeap, heapSourcePath, 385);
    }
    remainder = bestSize - blockSize;
    if (remainder >= HEAP_MIN_BLOCK_SIZE) {
        pSplit = (struct RgHeapBlock *)((u8 *)pFit + blockSize);
        _SetHeapHead(pSplit, remainder);
    } else {
        blockSize = bestSize;
        pSplit = 0;
    }
    if (pSplit != 0) {
        _InsertNext(pFit, pSplit);
    }
    if (pFit == pFreeHead) {
        pHeap->freeHead = pFit->next;
    }
    _Unlink(pFit);
    _SetHeapHead(pFit, blockSize);
    _MarkAlloc(pFit);
    if (pSourceFile != 0) {
        strncpy(pFit->module + 1, pSourceFile, 30);
        pFit->module[31] = 0;
        pFit->line = line;
    } else {
        pFit->module[1] = 0;
        pFit->line = 0;
    }
    _SetMagicString(pFit);
    pFit->module[0] = heapId;
    if (pHeap->allocHead == 0) {
        pHeap->allocHead = pFit;
        _Link(pFit, pFit, pFit);
    } else {
        _InsertNext(pHeap->allocHead, pFit);
    }
    pPayload = pFit + 1;
    if (((u32)pPayload & 15) != 0) {
        _Error(heapAssertAllocAligned, heapNotAlignedBlockComment, pHeap, heapSourcePath, 429);
    }
    return pPayload;
}

extern const char heapAssertHeapNotNil[];
extern const char heapFreeEntryComment[];
extern const char heapFreeNilAddressFormat[];
extern const char heapErrorComment[];
extern const char heapAlreadyFreeFormat[];
extern const char heapNotAllocatedFreeFormat[];
extern const char heapBlockDestroyedFormat[];
extern const char heapPtrNotInAreaFormat[];
extern const char heapAllocListNodeFreeBlockMismatchFormat[];
int RgHeapIsInSelf(struct RgHeap *pHeap, void *pPtr);

void RgHeapFree(void *pPtr, const char *pSourceFile, int line)
{
    struct RgHeapBlock *pBlock;
    struct RgHeap *pHeap;
    struct RgHeapBlock *pAllocHead;
    struct RgHeapBlock *pNext;
    struct RgHeapBlock *pCur;

    pBlock = (struct RgHeapBlock *)pPtr - 1;
    pHeap = InstanceOfRgHeapById(pBlock->module[0]);
    if (pHeap == 0) {
        _Error(heapAssertHeapNotNil, heapFreeEntryComment, 0, heapSourcePath, 443);
    }
    if (pPtr == 0) {
        printf(heapFreeNilAddressFormat, pSourceFile, line);
    }
    if (_IsAllocated(pBlock) == 0) {
        if (_IsCollectMagicString(pBlock) != 0) {
            RgHeapDump_sub(pHeap, heapErrorComment, heapSourcePath, 456);
            printf(heapAlreadyFreeFormat, pPtr, pSourceFile, line, pBlock->module + 1, pBlock->line);
        } else {
            printf(heapNotAllocatedFreeFormat, pPtr, pSourceFile, line);
        }
    }
    if (_IsCollectMagicString(pBlock) == 0) {
        printf(heapBlockDestroyedFormat, pPtr, pSourceFile, line);
    }
    if (RgHeapIsInSelf(pHeap, pBlock) == 0) {
        printf(heapPtrNotInAreaFormat, pSourceFile, pHeap, pSourceFile, line);
    }
    pAllocHead = pHeap->allocHead;
    pNext = pAllocHead->next;
    if (pNext == pAllocHead) {
        if (pBlock != pNext) {
            printf(heapAllocListNodeFreeBlockMismatchFormat);
        }
        pHeap->allocHead = 0;
    } else {
        if (pAllocHead == pBlock) {
            _Unlink(pBlock);
            pHeap->allocHead = pNext;
        } else {
            _Unlink(pBlock);
        }
    }
    _UnmarkAlloc(pBlock);
    if (pHeap->freeHead == 0) {
        pHeap->freeHead = _SetHeapHead(pBlock, pBlock->size);
        return;
    }
    pCur = pHeap->freeHead;
    _Link(pBlock, 0, 0);
    do {
        if (pBlock < pCur) {
            _InsertPre(pCur, pBlock);
            if (pBlock < pHeap->freeHead) {
                pHeap->freeHead = pBlock;
            }
            break;
        }
        pCur = pCur->next;
    } while (pCur != pHeap->freeHead);
    if (pBlock->next == 0) {
        _InsertPre(pHeap->freeHead, pBlock);
    }
    _MergeBlocks(pHeap->freeHead);
}

int RgHeapIsInSelf(struct RgHeap *pHeap, void *pPtr)
{
    if (pPtr < pHeap->top || (char *)pPtr >= (char *)pHeap->top + pHeap->size) {
        return 0;
    }
    return 1;
}

int RgHeapIsInvalidMemory(struct RgHeap *pHeap, void *pPtr)
{
    struct RgHeapBlock *pBlock;

    if (pPtr == 0) {
        return 1;
    }
    pBlock = (struct RgHeapBlock *)pPtr - 1;
    if (_IsAllocated(pBlock) == 0) {
        return 1;
    }
    if (_IsCollectMagicString(pBlock) == 0) {
        return 1;
    }
    return RgHeapIsInSelf(pHeap, pBlock) == 0;
}

extern const char heapDumpSeparatorFormat[];
extern const char heapDumpHeadFormat[];
extern const char heapDumpFreeMapFormat[];
extern const char heapDumpFreeBlockFormat[];
extern const char heapDumpAllocMapFormat[];
extern const char heapDumpAllocBlockFormat[];

void RgHeapDump_sub(struct RgHeap *pHeap, const char *pComment, const char *pSourceFile, int line)
{
    struct RgHeapBlock *pBlock;

    pBlock = pHeap->freeHead;
    printf(heapDumpSeparatorFormat);
    printf(heapDumpHeadFormat, pHeap, pSourceFile, line);
    printf(heapDumpFreeMapFormat, pBlock, pComment);
    do {
        printf(heapDumpFreeBlockFormat, pBlock, pBlock->size, pBlock->size, pBlock->next);
        pBlock = pBlock->next;
    } while (pBlock != pHeap->freeHead);
    pBlock = pHeap->allocHead;
    printf(heapDumpAllocMapFormat, pBlock, pComment);
    if (pBlock != 0) {
        do {
            printf(heapDumpAllocBlockFormat, pBlock, pBlock->size - sizeof(struct RgHeapBlock), pBlock->size, pBlock->next,
                   pBlock->module + 1, pBlock->line);
            pBlock = pBlock->next;
        } while (pBlock != pHeap->allocHead);
    }
}

extern const char heapDumpBlockHeaderFormat[];
extern const char heapDumpBlockLinksFormat[];
extern const char heapDumpBlockSizeMarkFormat[];
extern const char heapDumpBlockModuleFormat[];
extern const char heapDumpBlockNotInHeapFormat[];

void RgHeapDumpBlock(struct RgHeap *pHeap, void *pPtr)
{
    struct RgHeapBlock *pBlock;

    printf(heapDumpBlockHeaderFormat);
    pBlock = (struct RgHeapBlock *) pPtr - 1;
    if (RgHeapIsInSelf(pHeap, pPtr) != 0) {
        printf(heapDumpBlockLinksFormat, pBlock->pre, pBlock->next);
        printf(heapDumpBlockSizeMarkFormat, pBlock->size, pBlock->mark);
        printf(heapDumpBlockModuleFormat, pBlock->module + 1, pBlock->line);
        return;
    }
    printf(heapDumpBlockNotInHeapFormat, pHeap);
}

extern const char heapDumpHeapIdFormat[];

void RgHeapDumpById(u32 heapId)
{
    char sourceFile[0x80];
    struct RgHeap *pHeap;

    if (heapId < HEAP_MAX) {
        pHeap = InstanceOfRgHeapById(heapId);
        sprintf(sourceFile, heapDumpHeapIdFormat, heapId);
        RgHeapDump_sub(pHeap, sourceFile, heapSourcePath, 611);
    }
}

/*
 * Row callback of the heap window: scales the heap map of the window's heap (RgHeap.mapStride, from
 * testWindowGetUserData), doubled while PAD_TRIANGLE is pressed and halved while PAD_SQUARE is pressed, then
 * keeps it within 1..0x10000.
 */
void *testWindowGetUserData(int window);

void heapWindowMapStrideAdjust(int window)
{
    struct RgHeap *pHeap;
    u16 flags;

    pHeap = testWindowGetUserData(window);
    flags = PadData[0].pressed;
    if ((flags & PAD_TRIANGLE) != 0) {
        pHeap->mapStride = pHeap->mapStride * 2;
    }
    if ((flags & PAD_SQUARE) != 0) {
        pHeap->mapStride = pHeap->mapStride / 2;
    }
    if (pHeap->mapStride <= 0) {
        pHeap->mapStride = TEXT_SCALE_MIN;
    }
    if (pHeap->mapStride > TEXT_SCALE_MAX) {
        pHeap->mapStride = TEXT_SCALE_MAX;
    }
}

struct HeapWindowInfo {
    unsigned char unmodeled_00[0xC];
    int rowCount;
};

void testWindowGetInfo(int window, struct HeapWindowInfo *pInfo);
void testWindowDrawLine(int window, int x0, int y0, int x1, int y1, u32 color);

/*
 * Paints the heap map of a window. The arena is first filled in map rows
 * numbered from 1 with colour 0x80000080; a map row is mapStride cells wide
 * (InitRgHeap stores 0x100 there) and a cell is mapStride bytes, so the
 * partial last row is scaled to its share of the 256-pixel width. Every block
 * of the allocated chain (allocHead) is then painted over it with colour
 * 0x80008080, one run per map row. testWindowGetInfo supplies the row count in
 * info.rowCount.
 */
void heapWindowPaintMap(int window)
{
    struct HeapWindowInfo info;
    struct RgHeap *pHeap;
    struct RgHeapBlock *pBlock;
    int arenaCells;
    int blockRow;
    u32 blockCells;
    int mapRow = 1;
    u32 blockCol;
    u32 runCells;
    u32 byteOffset;
    u32 cellIndex;

    pHeap = testWindowGetUserData(window);
    testWindowGetInfo(window, &info);

    arenaCells = (u32)pHeap->size / pHeap->mapStride;
    if (arenaCells != 0) {
        do {
            if (arenaCells >= pHeap->mapStride) {
                testWindowDrawLine(window, 0, mapRow, HEAP_MAP_WIDTH, mapRow, HEAP_MAP_ARENA_COLOR);
                arenaCells -= pHeap->mapStride;
            } else {
                testWindowDrawLine(window, 0, mapRow, (arenaCells << 8) / pHeap->mapStride, mapRow, HEAP_MAP_ARENA_COLOR);
                arenaCells = 0;
            }
            mapRow += 1;
        } while (mapRow < info.rowCount && arenaCells != 0);
    }

    pBlock = pHeap->allocHead;
    if (pBlock != 0) {
        do {
            byteOffset = (u32)pBlock - (u32)pHeap->top;
            cellIndex = byteOffset / pHeap->mapStride;
            blockCells = pBlock->size / pHeap->mapStride;
            blockRow = cellIndex / pHeap->mapStride;
            blockCol = cellIndex % pHeap->mapStride;

            runCells = pHeap->mapStride - blockCol;
            if (blockCells < runCells) {
                runCells = blockCells;
            }
            testWindowDrawLine(window, blockCol, blockRow, ((blockCol + runCells) << 8) / pHeap->mapStride,
                        blockRow, HEAP_MAP_BLOCK_COLOR);
            blockCells -= runCells;
            blockRow += 1;

            do {
                runCells = pHeap->mapStride;
                if (blockCells < runCells) {
                    runCells = blockCells;
                }
                testWindowDrawLine(window, 0, blockRow, (runCells << 8) / pHeap->mapStride, blockRow, HEAP_MAP_BLOCK_COLOR);
                blockCells -= runCells;
                blockRow += 1;
            } while (blockRow <= info.rowCount && blockCells != 0);
            pBlock = pBlock->next;
        } while (pBlock != pHeap->allocHead);
    }
}

struct HeapWindowRow {
    void *callbacks[2];
    char *pModuleName;
    u16 *pLine;
    unsigned char unmodeled_10[0x24];
    union {
        struct RgHeap *pHeap;
        u32 heapId;
    } heap;
    unsigned char unmodeled_38[0x18];
};

/* A 0xa0-byte template: the HeapWindowRow the code copies, then 0x50 more bytes. */
struct HeapWindowRowTemplate {
    struct HeapWindowRow row;
    unsigned char unmodeled_50[0x50];
};

extern struct HeapWindowRowTemplate heapWindowAllocRowTemplate;
void testWindowAddRow(int window, struct HeapWindowRow *pRow);

/*
 * One window row per allocated block of heap 0. Every row starts as a copy of
 * the heapWindowAllocRowTemplate template; pModuleName points at module[1] (module[0] holds the
 * owning heap id, see RgHeapFree) and pLine at the block's line field.
 */
void heapWindowAddAllocRows(int window)
{
    struct HeapWindowRow row = heapWindowAllocRowTemplate.row;
    struct RgHeap *pHeap = InstanceOfRgHeapById(0);
    struct RgHeapBlock *pBlock = pHeap->allocHead;

    if (pBlock != 0) {
        do {
            row = heapWindowAllocRowTemplate.row;
            row.pModuleName = &pBlock->module[1];
            row.pLine = &pBlock->line;
            testWindowAddRow(window, &row);
            pBlock = pBlock->next;
        } while (pBlock != pHeap->allocHead);
    }
}

void testWindowClearRows(void);

/* Window callback registered by heapWindowOpen: calls testWindowClearRows on the window, then rebuilds its heap
 * contents with heapWindowAddAllocRows, the same call heapWindowOpen makes once when it opens the window. */
void heapWindowRefresh(int window)
{
    testWindowClearRows();
    heapWindowAddAllocRows(window);
}

struct WindowCreateParam {
    int x;
    int y;
    int width;
    int height;
    char name[32];
};

extern char *heapWindowTitles[];
extern const char heapWindowIdAssertText[];
extern struct HeapWindowRowTemplate heapWindowRowTemplate;
extern char heapWindowEmptyModuleName[8];
extern char heapWindowEmptyText[8];
int testWindowFindByName(int parentWindow, const char *pName);
void testWindowShow(int window);
int testWindowCreate(struct WindowCreateParam *pParam, int parentWindow);

/*
 * Heap window for the heap id testWindowGetUserData gives parentWindow (below HEAP_MAX). If
 * testWindowFindByName finds a window for the named slot, it is shown with testWindowShow
 * and nothing else happens. Otherwise a window is created with testWindowCreate at
 * (100, 80), 256 by 300, and the heap's row is passed to testWindowAddRow.
 */
void heapWindowCreate(int parentWindow)
{
    struct WindowCreateParam param;
    struct HeapWindowRow row;
    u32 nId;
    int window;

    /* The window user data of a heap list entry is the heap id itself, stored in the pointer-sized slot. */
    nId = (u32) testWindowGetUserData(parentWindow);
    if (nId >= HEAP_MAX) {
        assertFailed(heapWindowIdAssertText, heapSourcePath, 800);
    }
    window = testWindowFindByName(parentWindow, heapWindowTitles[nId]);
    if (window >= 0) {
        testWindowShow(window);
        return;
    }
    param.x = HEAP_WINDOW_X;
    param.y = HEAP_WINDOW_Y;
    param.width = HEAP_WINDOW_WIDTH;
    param.height = HEAP_WINDOW_HEIGHT;
    strcpy(param.name, heapWindowTitles[nId]);
    window = testWindowCreate(&param, parentWindow);
    row = heapWindowRowTemplate.row;
    row.pModuleName = heapWindowEmptyModuleName;
    row.heap.pHeap = InstanceOfRgHeapById(nId);
    testWindowAddRow(window, &row);
}

extern char heapWindowAllocTitle[];
void testWindowSetRefreshCallback(int window, void (*pCallback)(int));

void heapWindowOpen(int parentWindow)
{
    const char *pName = heapWindowAllocTitle;
    struct WindowCreateParam param;
    int window;

    window = testWindowFindByName(parentWindow, pName);
    if (window >= 0) {
        testWindowShow(window);
    } else {
        param.x = HEAP_ALLOC_WINDOW_X;
        param.y = HEAP_ALLOC_WINDOW_Y;
        param.width = HEAP_ALLOC_WINDOW_WIDTH;
        param.height = HEAP_ALLOC_WINDOW_HEIGHT;
        strcpy(param.name, pName);
        window = testWindowCreate(&param, parentWindow);
        testWindowSetRefreshCallback(window, heapWindowRefresh);
        heapWindowAddAllocRows(window);
    }
}

extern char heapOverviewTitle[8];
extern char heapWindowTitleHeap0[];
extern char heapWindowTitleHeap1[];
extern char heapWindowTitleHeap2[];
extern char heapWindowTitleHeap3[];
extern char heapWindowTitleSystem[];
extern char heapWindowTitleMatrix[];
extern unsigned char heapOverviewWindowColumnTable[];
extern struct HeapWindowRowTemplate heapOverviewRowTemplate;
extern const char heapOverviewWindowCloseFormat[];
void testWindowSetText(const char *pText, int window);

void heapOverviewWindowOpen(int parentWindow)
{
    struct WindowCreateParam param;
    struct HeapWindowRow row;
    int window;

    window = testWindowFindByName(parentWindow, heapOverviewTitle);
    if (window >= 0) {
        testWindowShow(window);
        return;
    }
    param.x = HEAP_OVERVIEW_WINDOW_X;
    param.y = HEAP_OVERVIEW_WINDOW_Y;
    param.width = HEAP_OVERVIEW_WINDOW_WIDTH;
    param.height = HEAP_OVERVIEW_WINDOW_HEIGHT;
    strcpy(param.name, heapOverviewTitle);
    window = testWindowCreate(&param, parentWindow);
    testWindowSetColumnTable(window, heapOverviewWindowColumnTable);
    row = heapOverviewRowTemplate.row;
    row.pModuleName = heapWindowTitleHeap0;
    row.heap.heapId = 0;
    testWindowAddRow(window, &row);
    row = heapOverviewRowTemplate.row;
    row.pModuleName = heapWindowTitleHeap1;
    row.heap.heapId = 1;
    testWindowAddRow(window, &row);
    row = heapOverviewRowTemplate.row;
    row.pModuleName = heapWindowTitleHeap2;
    row.heap.heapId = 2;
    testWindowAddRow(window, &row);
    row = heapOverviewRowTemplate.row;
    row.pModuleName = heapWindowTitleHeap3;
    row.heap.heapId = 3;
    testWindowAddRow(window, &row);
    row = heapOverviewRowTemplate.row;
    row.pModuleName = heapWindowTitleSystem;
    row.heap.heapId = 4;
    testWindowAddRow(window, &row);
    row = heapOverviewRowTemplate.row;
    row.pModuleName = heapWindowTitleMatrix;
    row.heap.heapId = 5;
    testWindowAddRow(window, &row);
    testWindowSetText(heapOverviewWindowCloseFormat, window);
}

extern unsigned char heapOverviewMenuColumnTable[];

void heapOverviewMenuAttach(int value)
{
    testWindowSetColumnTable(value, heapOverviewMenuColumnTable);
}

void testOverlayMenuOpen(int, int, int, int, int);

void testOverlayEntryCall(int value)
{
    testOverlayMenuOpen(value, 0, 0, 0, 0);
}

const char heapAssertHeaderFormat[40] = "-------- Heap Assert %s ---------\n\000\000\000\000\000\000";

const char heapAssertExpressionFormat[24] = "%s (in %s at %d)\n\n\000\000\000\000\000\000";

const char heapAssertHeapInfoFormat[32] = "heap = %p top = %p size = %d\n\n\000\000";

const char heapAssertDumpComment[16] = "heap-dump\000\000\000\000\000\000\000";

const char heapSourcePath[16] = "../test.euc.c\000\000\000";

const char heapAssertHeapNilFormat[40] = "heap = NIL\n\000\000\000\000\000gAMe sHoW 2001.10.15\000\000\000\000";

const char heapAssertHeadSizeBelowBlockSize[24] = "HEAD_SIZE < nBlockSize\000\000";

const char heapMakeBlockComment[40] = "make block\000\000\000\000\000\000(HEAD_SIZE & 0xf) == 0\000\000";

const char heapAssertBufSizeAligned[24] = "(nBufSize & 0xf) == 0\000\000\000";

const char heapAllocZeroSizeFormat[40] = "size <= 0 error allocated at %s,%d\000\000\000\000\000\000";

const char heapAssertHeapNotNilParenthesized[16] = "(pHeap != NIL)\000\000";

const char heapAllocEntryHeapPtrComment[24] = "alloc-entry-heap-ptr\000\000\000\000";

const char heapAllocNoHeapFormat[48] = "there is no heap for allocation at %s,%d\000\000\000\000\000\000\000\000";

const char heapNoHeapIdFormat[16] = "no heap %d\n\000\000\000\000\000";

const char heapAllocTooBigDataHeapFormat[56] = "too big size (%d) of allocation\nat %s,%d\nfrom data heap\000";

const char heapAssertMatchBlockSizeEnough[40] = "(nMatchBlockSiz >= nNeedBlockSiz)\000\000\000\000\000\000\000";

const char heapBlockAlignComment[16] = "block align\000\000\000\000\000";

const char heapAssertAllocAligned[48] = "(((int)(u_char*)pAlloc + HEAD_SIZE) & 0xf) == 0\000";

const char heapNotAlignedBlockComment[24] = "not aligned block\000\000\000\000\000\000\000";

const char heapAssertHeapNotNil[16] = "pHeap != NIL\000\000\000\000";

const char heapFreeEntryComment[16] = "free entry\000\000\000\000\000\000";

const char heapFreeNilAddressFormat[32] = "free NIL address at %s,%d\000\000\000\000\000\000\000";

const char heapErrorComment[16] = "heap error\000\000\000\000\000\000";

const char heapAlreadyFreeFormat[64] = "%p is already free at %s,%d\n(MAYBE allocated at %s,%d !\?)\000\000\000\000\000\000\000";

const char heapNotAllocatedFreeFormat[32] = "not allocated %p free at %s,%d\000\000";

const char heapBlockDestroyedFormat[56] = "heap block is destroyed or not in heap %p free at %s,%d\000";

const char heapPtrNotInAreaFormat[48] = "%s is not in %p HEAP memory area\nfree at %s %d\000\000";

const char heapAllocListNodeFreeBlockMismatchFormat[48] = "alloc list node 1 && free block is not this\000\000\000\000\000";

const char heapDumpSeparatorFormat[40] = "\n\n---------------------------------\n\000\000\000\000";

const char heapDumpHeadFormat[32] = "HEAD DUMP %p from %s %d\n\000\000\000\000\000\000\000\000";

const char heapDumpFreeMapFormat[40] = "***** FREE MAP top %p (comment %s)\n\000\000\000\000\000";

const char heapDumpFreeBlockFormat[32] = "%p : size = %x(%d) next:%p\n\000\000\000\000\000";

const char heapDumpAllocMapFormat[40] = "***** ALLOC MAP top %p (comment %s)\n\000\000\000\000";

const char heapDumpAllocBlockFormat[48] = "%p : size = %x(%d) next:%p allocated at %s,%d\n\000\000";

const char heapDumpBlockHeaderFormat[24] = "------------ dump %p\n\000\000\000";

const char heapDumpBlockLinksFormat[16] = "pre %p next %p\n\000";

const char heapDumpBlockSizeMarkFormat[24] = "size %d mark %d\n\000\000\000\000\000\000\000\000";

const char heapDumpBlockModuleFormat[24] = "module '%s' line %d\n\000\000\000\000";

const char heapDumpBlockNotInHeapFormat[32] = "  this block is not in heap %p\n\000";

const char heapDumpHeapIdFormat[16] = "heapId %d\000\000\000\000\000\000\000";

const char heapWindowIdAssertText[32] = "nId>=0 && nId < HEAP_MAX\000\000\000\000\000\000\000\000";

const char heapOverviewWindowCloseFormat[32] = "window close not %d\000\000\000\000\000\000\000\000\000\000\000\000\000";

const Vector4 vu0IdentityMatrix[4] = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };

short heapInitFlags[8] = { 0 };

/* JP: メモリ確保位置
 * EN: Memory allocation position */
char heapWindowAllocTitle[16] = "\245\341\245\342\245\352\263\316\312\335\260\314\303\326\000\000";

/* JP: メモリ０配置状況
 * EN: Memory 0 layout */
char heapWindowTitleHeap0[24] = "\245\341\245\342\245\352\243\260\307\333\303\326\276\365\266\267\000\000\000\000\000\000\000\000";

/* JP: メモリ１配置状況
 * EN: Memory 1 layout */
char heapWindowTitleHeap1[24] = "\245\341\245\342\245\352\243\261\307\333\303\326\276\365\266\267\000\000\000\000\000\000\000\000";

/* JP: メモリ２配置状況
 * EN: Memory 2 layout */
char heapWindowTitleHeap2[24] = "\245\341\245\342\245\352\243\262\307\333\303\326\276\365\266\267\000\000\000\000\000\000\000\000";

/* JP: メモリ３配置状況
 * EN: Memory 3 layout */
char heapWindowTitleHeap3[24] = "\245\341\245\342\245\352\243\263\307\333\303\326\276\365\266\267\000\000\000\000\000\000\000\000";

/* JP: システム配置状況
 * EN: System layout */
char heapWindowTitleSystem[24] = "\245\267\245\271\245\306\245\340\307\333\303\326\276\365\266\267\000\000\000\000\000\000\000\000";

/* JP: マトリクス配置状況
 * EN: Matrix layout */
char heapWindowTitleMatrix[24] = "\245\336\245\310\245\352\245\257\245\271\307\333\303\326\276\365\266\267\000\000\000\000\000\000";

char *heapWindowTitles[8] = { heapWindowTitleHeap0, heapWindowTitleHeap1, heapWindowTitleHeap2, heapWindowTitleHeap3, heapWindowTitleSystem, heapWindowTitleMatrix, 0, 0 };

struct HeapWindowRowTemplate heapWindowAllocRowTemplate = { 0, 0, (char *)heapWindowEmptyModuleName, (u16 *)heapWindowEmptyText, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char heapOverviewMenuColumnTable[160] = { 0, 0, 0, 0, 0, 0, 0, 0, 192, 218, 105, 0, 0, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 248, 226, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

unsigned char heapOverviewWindowColumnTable[160] = { 0, 0, 0, 0, 0, 0, 0, 0, 128, 208, 62, 0, 0, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 226, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
struct HeapWindowRowTemplate heapOverviewRowTemplate = { 0, 0, (char *)heapWindowTitleHeap0, 0, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 224, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
struct HeapWindowRowTemplate heapWindowRowTemplate = { (void *)heapWindowMapStrideAdjust, (void *)heapWindowPaintMap, (char *)heapWindowTitleHeap0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

unsigned char *heapMagicStringPtr = (unsigned char *)(heapAssertHeapNilFormat + 0x10);
char heapInitStage1Comment[8] = "init-1";
char heapInitStageComment[8] = "init-2\000\000";

char heapWindowEmptyModuleName[8] = "\000\000\000\000\000\000\000\000";

/* JP: メモリ
 * EN: Memory */
char heapOverviewTitle[8] = "\245\341\245\342\245\352\000\000";

char heapWindowEmptyText[8] = "\000\000\000\000\000\000\000\000";
