#include "common.h"
#include "main/sm_initilize.h"

#include "shared.h"

/*
 * Memory cells of a pool. A cell is 0x40 bytes: its 16-byte header sits in the
 * first unit and the user data follows it (allocation pointer = cell + 0x40).
 * tag: 0x1234 for a free cell, 0x5678 for an allocated one.
 * unit_count: data units that follow the header of this cell.
 * prev/next: neighbouring cells in address order.
 */
typedef struct SmCell {
  int tag;
  int unit_count;
  struct SmCell *prev;
  struct SmCell *next;
} SmCell;

/* One pool unit: the cell header occupies the first unit of a cell. */
typedef union SmUnit {
  SmCell header;
  unsigned char payload[0x40];
} SmUnit;

/*
 * A pool, as the global object is laid out: first_cell at 0x00, total_units at
 * 0x04. Bytes 0x08..0x17 are allocation counters filled by smInitMemInfo:
 * allocated_units, total_bytes, allocated_bytes, largest_free_block.
 */
typedef struct SmPool {
  SmCell *first_cell;
  int total_units;
  int allocated_units;
  int total_bytes;
  int allocated_bytes;
  int largest_free_block;
} SmPool;

static SmPool smPool;
static SmPool smPoolSmall;

/* Initial counters for a freshly-carved pool: one free block spanning the whole pool. */
void smInitMemInfo(SmPool *pool, SmCell *first_cell, u32 unit_count)
{
  u32 bytes;

  bytes = (unit_count << 6) + 0x40;
  pool->first_cell = first_cell;
  pool->total_units = unit_count;
  pool->allocated_units = 0;
  pool->total_bytes = bytes;
  pool->allocated_bytes = 0;
  pool->largest_free_block = bytes;
}

void smInitilize(void *memory, u32 size)
{
  SmCell *cell;
  u32 unit_count;

  cell = memory;
  unit_count = size >> 6;
  cell->tag = 0x1234;
  cell->unit_count = (int)(unit_count - 1);
  cell->prev = 0;
  cell->next = 0;
  smInitMemInfo(&smPool, cell, unit_count);
}

void smInitilizeSmall(void *memory, u32 size)
{
  SmCell *cell;
  u32 unit_count;

  cell = memory;
  unit_count = size >> 6;
  cell->tag = 0x1234;
  cell->unit_count = (int)(unit_count - 1);
  cell->prev = 0;
  cell->next = 0;
  smInitMemInfo(&smPoolSmall, cell, unit_count);
}

/* Best-fit search over a pool; mode 0 allocates the cell found, nonzero only returns it. */
void *smAllocFromPool(u32 size, SmCell *first_cell, int total_units, int find_only)
{
  SmCell *cell;
  SmCell *best;
  u32 best_units;
  u32 units;
  SmCell *new_cell;
  SmCell *next;
  void *result;

  units = (size + 0x3F) >> 6;
  best = 0;
  best_units = total_units;

  cell = first_cell;
  while (cell != 0)
  {
    if (cell->tag == 0x1234 && cell->unit_count >= units && cell->unit_count < best_units)
    {
      best_units = cell->unit_count;
      best = cell;
      if (best_units == units)
      {
        break;
      }
    }
    cell = cell->next;
  }

  result = best;
  if (find_only == 0)
  {
    result = 0;
    if (best != 0)
    {
      best->tag = 0x5678;
      if (best->unit_count != units)
      {
        new_cell = (SmCell *)((SmUnit *)best + units + 1);
        new_cell->tag = 0x1234;
        new_cell->prev = best;
        next = best->next;
        new_cell->next = next;
        new_cell->unit_count = best->unit_count - units - 1;
        if (next != 0)
        {
          next->prev = new_cell;
        }
        best->unit_count = units;
        best->next = new_cell;
      }
      result = (SmUnit *)best + 1;
    }
  }
  return result;
}

void *smAlloc(int size)
{
  return smAllocFromPool((size + 0x7FF) & ~0x7FF, smPool.first_cell, smPool.total_units, 0);
}

void *smFindFree(int size)
{
  return smAllocFromPool((size + 0x7FF) & ~0x7FF, smPool.first_cell, smPool.total_units, 1);
}

void *smAllocSmall(int size)
{
  return smAllocFromPool((size + 0x3F) & ~0x3F, smPoolSmall.first_cell, smPoolSmall.total_units, 0);
}

/* Absorbs the free cell that follows cell into it. */
static inline void smMergeWithNext(SmCell *cell, SmCell *next)
{
  SmCell *after;

  cell->unit_count += next->unit_count + 1;
  after = next->next;
  cell->next = after;
  if (after != 0)
  {
    after->prev = cell;
  }
}

/* Marks the cell of an allocation free and merges it with free neighbours. */
void smFree(void *allocation)
{
  SmCell *cell;
  SmCell *neighbour;

  if (allocation != 0)
  {
    cell = &((SmUnit *)allocation - 1)->header;
    cell->tag = 0x1234;
    neighbour = cell->next;
    if (neighbour != 0 && neighbour->tag == 0x1234)
    {
      smMergeWithNext(cell, neighbour);
    }
    neighbour = cell->prev;
    if (neighbour != 0 && neighbour->tag == 0x1234)
    {
      smMergeWithNext(neighbour, cell);
    }
  }
}

void smFreeHandle(void *handle)
{
  smFree(handle);
}

/* Walks the cell chain from first_cell, stopping at the first cell that is neither free nor allocated. */
void smWalkCells(SmCell *cell)
{
  int tag;

  if (cell != 0)
  {
    for (;;)
    {
      tag = cell->tag;
      if (tag == 0x1234 || tag == 0x5678)
      {
        cell = cell->next;
        if (cell != 0)
        {
          continue;
        }
      }
      break;
    }
  }
}

void smPrintInfo(void)
{
  smWalkCells(smPool.first_cell);
}

void smPrintInfoSmall(void)
{
  smWalkCells(smPoolSmall.first_cell);
}

/* Identity vector (w = 1) read by MMathCalcAngle and the other math helpers. */
const Vector4 MMathUnitVector = { 0.0f, 0.0f, 0.0f, 1.0f };

/* Identity matrix read by the MMathRotateMatrix* helpers as their default input. */
const Matrix4 MMathUnitMatrix = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

/* Message of MSeObjInit, stored right after the identity matrix in the original. */
const char MSeObjInitAllocMessage[40] = "MSeObjInit: Could not allocate memory";

/* Message of MSeObjProc for an invalid object type. */
const char MSeObjProcInvalidTypeFormat[32] = "MSeObjProc: Invalid type [%d]";
