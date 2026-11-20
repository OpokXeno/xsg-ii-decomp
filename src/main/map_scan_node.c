#include "common.h"
#include "shared.h"
#include "main/undulate.h"

/* Scan-tree constants: box axes, tree depth and capacity, node alignment, record state byte. */
#define MAP_AXIS_X 0
#define MAP_AXIS_Z 2
#define MAP_SCAN_MAX_DEPTH 8
#define MAP_SCAN_MAX_RECORDS 3073
#define MAP_SCAN_SLOT_COUNT 4
/* The scan pool: 1600 node-sized cells (0x6400 bytes) that the trees of all slots are carved from in order. */
#define MAP_SCAN_POOL_CELLS 1600
#define MAP_SCAN_ALIGN 16
#define MAP_SCAN_ALIGN_MASK 0xF
#define MAP_SCAN_TOLERANCE_RATIO 0.01f
#define MAP_RECORD_KIND_NO_PROBE 4
#define MAP_BLOCK_TYPE_RECORD_SETS 3
/* Record state byte: 0xFF once placed in a node, low 6 bits hold depth + 1, top 2 bits hold the side flags. */
#define MAP_RECORD_PLACED 0xFF
#define MAP_DEPTH_MASK 0x3F
#define MAP_CLASS_MASK 0xC0
#define MAP_CLASS_BACK 0x40
#define MAP_CLASS_FRONT 0x80
#define MAP_CLASS_STRADDLE 0xC0

/* One map vertex viewed as coord[4] for the split axis; the same 16 bytes as Vector4. */
typedef struct MapVertex {
    float coord[4];
} MapVertex;

/* Scan box corner, copied by value with doubleword moves. */
typedef Vector4Doublewords MapBoxCorner;

/* Scan-pool and record-state storage of this TU (uninitialized, static: .bss). The pool ends at 0x990FF0. The
 * 16 bytes up to 0x991000, where the Sony libraries' .bss starts, are taken as that object's alignment fill:
 * zero .bss cannot tell it from a 1601st cell, and 1600 is the round capacity. */
static DoublewordVector4 mapBoundsMin;
static DoublewordVector4 mapBoundsMax;
static unsigned char mapRecordStates[MAP_SCAN_MAX_RECORDS - 1];
static MapScanNode mapScanPool[MAP_SCAN_POOL_CELLS];
extern int mapScanIndexCount;

/* Builds the scan subtree for the records of one box: records whose flag is at this depth and overlap the box are classed against the split plane. */
MapScanNode *mapBuildScanNode(MapRecordSet *set, MapScanNode *node, MapBoxCorner lo, MapBoxCorner hi, unsigned char depth)
{
    MapRecord *record = set->records;
    /* Initial value is dead, but its store is part of the original code: keep it. */
    unsigned short *cursor = (unsigned short *)node;
    float tolerance;
    int backCount = 0;
    int recordIndex;
    int corner;
    int frontCount = 0;
    int straddleCount = 0;
    unsigned char classBits;

    node->splitAxis = MAP_AXIS_X;
    node->splitValue = (hi.component[MAP_AXIS_X] + lo.component[MAP_AXIS_X]) * 0.5f;
    node->recordCount = 0;
    node->frontOffset = 0;
    node->backOffset = 0;
    tolerance = __builtin_fabsf((hi.component[MAP_AXIS_X] - lo.component[MAP_AXIS_X]) * MAP_SCAN_TOLERANCE_RATIO);
    if (hi.component[MAP_AXIS_X] - lo.component[MAP_AXIS_X] < hi.component[MAP_AXIS_Z] - lo.component[MAP_AXIS_Z]) {
        node->splitAxis = MAP_AXIS_Z;
        node->splitValue = (hi.component[MAP_AXIS_Z] + lo.component[MAP_AXIS_Z]) * 0.5f;
        tolerance = __builtin_fabsf((hi.component[MAP_AXIS_Z] - lo.component[MAP_AXIS_Z]) * MAP_SCAN_TOLERANCE_RATIO);
    }

    recordIndex = 0;
    for (; set->recordCount > recordIndex; recordIndex++, record++) {
        classBits = 0;
        if (MAP_RECORD_PLACED != mapRecordStates[recordIndex]) {
            Vector4 *vertex[3];
            vertex[0] = set->vertices + record->vertexIndex[0];
            vertex[1] = set->vertices + record->vertexIndex[1];
            vertex[2] = set->vertices + record->vertexIndex[2];
            if (!(vertex[0]->x < lo.component[MAP_AXIS_X] && vertex[1]->x < lo.component[MAP_AXIS_X] && vertex[2]->x < lo.component[MAP_AXIS_X])
                && !(vertex[0]->z < lo.component[MAP_AXIS_Z] && vertex[1]->z < lo.component[MAP_AXIS_Z] && vertex[2]->z < lo.component[MAP_AXIS_Z])
                && !(hi.component[MAP_AXIS_X] < vertex[0]->x && hi.component[MAP_AXIS_X] < vertex[1]->x && hi.component[MAP_AXIS_X] < vertex[2]->x)
                && !(hi.component[MAP_AXIS_Z] < vertex[0]->z && hi.component[MAP_AXIS_Z] < vertex[1]->z && hi.component[MAP_AXIS_Z] < vertex[2]->z)
                && (mapRecordStates[recordIndex] & MAP_DEPTH_MASK) == depth) {
                for (corner = 0; corner < 3; corner++) {
                    /* local view: coord typed float[4] (MapVertex), the shared Vector4 field form changed the code */
                    float value = ((MapVertex *)vertex[corner])->coord[node->splitAxis];
                    if (node->splitValue - tolerance < value) {
                        classBits |= MAP_CLASS_FRONT;
                    }
                    if (node->splitValue + tolerance >= value) {
                        classBits |= MAP_CLASS_BACK;
                    }
                }
                mapRecordStates[recordIndex] = classBits | (depth + 1);
                switch (classBits) {
                case 0:
                    break;
                case MAP_CLASS_FRONT:
                    frontCount++;
                    break;
                case MAP_CLASS_BACK:
                    backCount++;
                    break;
                case MAP_CLASS_STRADDLE:
                    straddleCount++;
                    break;
                }
            }
        }
    }

    cursor = (unsigned short *)(node + 1);
    if (straddleCount != 0 || depth == MAP_SCAN_MAX_DEPTH) {
        for (recordIndex = 0; recordIndex < set->recordCount; recordIndex++) {
            if (mapRecordStates[recordIndex] != MAP_RECORD_PLACED && (mapRecordStates[recordIndex] & MAP_DEPTH_MASK) == depth + 1) {
                unsigned char recordClass = mapRecordStates[recordIndex] & MAP_CLASS_MASK;
                if (depth < MAP_SCAN_MAX_DEPTH ? recordClass == MAP_CLASS_STRADDLE : recordClass != 0) {
                    *cursor = recordIndex;
                    cursor++;
                    node->recordCount = node->recordCount + 1;
                    mapRecordStates[recordIndex] = MAP_RECORD_PLACED;
                }
            }
        }
        {
            int indexTotal = mapScanIndexCount;
            mapScanIndexCount = indexTotal + node->recordCount;
        }
    }
    /* unsigned int alignment arithmetic: an unsigned long form changes the code. */
    if ((unsigned int)cursor & MAP_SCAN_ALIGN_MASK) {
        cursor = (unsigned short *)(((unsigned int)cursor + MAP_SCAN_ALIGN) & ~MAP_SCAN_ALIGN_MASK);
    }
    if (depth < MAP_SCAN_MAX_DEPTH) {
        if (0 != frontCount) {
            float saved;
            node->frontOffset = (unsigned char *)cursor - (unsigned char *)node;
            saved = lo.component[node->splitAxis];
            lo.component[node->splitAxis] = node->splitValue;
            cursor = (unsigned short *)mapBuildScanNode(set, (MapScanNode *)cursor, lo, hi, depth + 1);
            lo.component[node->splitAxis] = saved;
        }
        if (backCount != 0) {
            node->backOffset = (unsigned char *)cursor - (unsigned char *)node;
            hi.component[node->splitAxis] = node->splitValue;
            cursor = (unsigned short *)mapBuildScanNode(set, (MapScanNode *)cursor, lo, hi, depth + 1);
        }
    }
    return (MapScanNode *)cursor;
}

/* Tests the node's records, descends the front child on the near side and walks the back-child chain. */
void mapScanNode(MapRecordSet *set, MapScanNode *node)
{
    unsigned short *indices;
    MapRecord *record;
    float distance;
    int kind;
    int i;

    indices = (unsigned short *)(node + 1);
    if (node->recordCount != 0) {
        for (i = 0; i < node->recordCount; i++) {
            record = set->records + indices[i];
            distance = UnduCheckSub(set, record);
            if (distance != MAP_HEIGHT_NONE) {
                kind = record->kind;
                if (set->kindTable != 0) {
                    kind = set->kindTable[kind].kindCode;
                }
                if (kind != MAP_RECORD_KIND_NO_PROBE) {
                    UnduCheckSubHeightCheck(set, record, distance);
                }
            }
        }
    }
    if (node->frontOffset != 0 && node->splitValue <= (&set->origin.x)[node->splitAxis]) {
        mapScanNode(set, (MapScanNode *)((unsigned char *)node + node->frontOffset));
    }
    if (node->backOffset == 0) {
        return;
    }
    if (!((&set->origin.x)[node->splitAxis] < node->splitValue)) {
        return;
    }
    node = (MapScanNode *)((unsigned char *)node + node->backOffset);
    mapScanNode(set, node);
}

extern MapScanNode *mapScanPoolCursor;
extern MapRegisteredNode mapRegisteredSlots[MAP_SCAN_SLOT_COUNT];

/* Bounds the records of one set with VU0 min/max over their vertices, builds the scan tree on that box and registers it in a free slot. */
MapScanNode *mapRegisterNode(MapRecordSet *set)
{
    MapScanNode *root;
    MapRecord *record;
    int recordsLeft;
    int slot;

    record = set->records;
    mapBoundsMin.vector.z = 1000000.0f;
    mapBoundsMin.vector.y = 1000000.0f;
    mapBoundsMax.vector.z = -1000000.0f;
    mapBoundsMax.vector.y = -1000000.0f;
    mapBoundsMin.vector.x = 1000000.0f;
    mapBoundsMax.vector.x = -1000000.0f;
    root = mapScanPoolCursor;
    __asm__ __volatile__(
        "lqc2 vf4, 0(%0)\n\t"
        "lqc2 vf5, 0(%1)\n\t"
        :
        : "r" (&mapBoundsMin), "r" (&mapBoundsMax)
        : "memory");
    if (set->recordCount > 0) {
        recordsLeft = set->recordCount;
        do {
            __asm__ __volatile__(
                "lqc2 vf1, 0(%0)\n\t"
                "lqc2 vf2, 0(%1)\n\t"
                "lqc2 vf3, 0(%2)\n\t"
                "vmini.xyz vf4, vf4, vf1\n\t"
                "vmax.xyz vf5, vf5, vf1\n\t"
                "vmini.xyz vf4, vf4, vf2\n\t"
                "vmax.xyz vf5, vf5, vf2\n\t"
                "vmini.xyz vf4, vf4, vf3\n\t"
                "vmax.xyz vf5, vf5, vf3\n\t"
                :
                : "r" (set->vertices + record->vertexIndex[0]),
                  "r" (set->vertices + record->vertexIndex[1]),
                  "r" (set->vertices + record->vertexIndex[2]));
            record++;
        } while (--recordsLeft != 0);
    }
    __asm__ __volatile__(
        "sqc2 vf4, 0(%0)\n\t"
        "sqc2 vf5, 0(%1)\n\t"
        :
        : "r" (&mapBoundsMin), "r" (&mapBoundsMax)
        : "memory");
    memset(mapRecordStates, 0, set->recordCount);
    mapScanPoolCursor = mapBuildScanNode(set, mapScanPoolCursor, *(MapBoxCorner *) &mapBoundsMin,
                               *(MapBoxCorner *) &mapBoundsMax, 0);
    for (slot = 0; slot < MAP_SCAN_SLOT_COUNT; slot++) {
        if (mapRegisteredSlots[slot].records == 0) {
            mapRegisteredSlots[slot].root = root;
            mapRegisteredSlots[slot].records = set->records;
            break;
        }
    }
    return root;
}

/* Scans the registered tree of this record set, or returns 0 when the set is too large or unregistered. */
int mapScanRegisteredNode(MapRecordSet *set)
{
    int slot;

    mapScanIndexCount = 0;
    if (set->recordCount >= MAP_SCAN_MAX_RECORDS) {
        return 0;
    }
    for (slot = 0; slot < MAP_SCAN_SLOT_COUNT; slot++) {
        if (mapRegisteredSlots[slot].records == set->records) {
            mapScanNode(set, mapRegisteredSlots[slot].root);
            return 1;
        }
    }
    return 0;
}

typedef struct MapBlockSub {
    unsigned char unmodeled_00[2];
    unsigned char entryCount;
    unsigned char flags;
} MapBlockSub;

/* Entry header of one registered record set; its record array follows it. */
typedef struct MapBlockEntry {
    unsigned short recordCount;
    unsigned char unmodeled_02[2];
    unsigned int vertexOffset;
    unsigned char unmodeled_08[8];
} MapBlockEntry;

/* Clears the registry, starts the scan pool and registers every entry of the block's sub-blocks. */
void mapRegisterBlock(MapBlockHeader *block)
{
    MapRecordSet set;
    MapBlockSub *sub;
    MapBlockEntry *entry;
    MapRegisteredNode *slot;
    unsigned int *entryOffset;
    int subBlockIndex;
    int entryIndex;
    int clearIndex;

    slot = mapRegisteredSlots;
    for (clearIndex = MAP_SCAN_SLOT_COUNT - 1; clearIndex >= 0; clearIndex--) {
        slot->records = 0;
        slot->root = 0;
        slot++;
    }
    mapScanPoolCursor = mapScanPool;
    if (block == 0 || block->type != MAP_BLOCK_TYPE_RECORD_SETS || block->subBlockCount == 0) {
        return;
    }
    subBlockIndex = 0;
    do {
        /* Block offsets as unsigned int: a (unsigned char *) byte-pointer form changes the code. */
        sub = (MapBlockSub *)(((unsigned int *)(block + 1))[subBlockIndex] + (unsigned int)block);
        if (sub->flags == 0) {
            entryIndex = 0;
            if (sub->entryCount != 0) {
                entryOffset = (unsigned int *)(sub + 1);
                do {
                    entry = (MapBlockEntry *)(entryOffset[entryIndex] + (unsigned int)block);
                    set.recordCount = entry->recordCount;
                    set.vertices = (Vector4 *)(entry->vertexOffset + (unsigned int)block);
                    set.records = (MapRecord *)(entry + 1);
                    mapRegisterNode(&set);
                    entryIndex++;
                } while (entryIndex < sub->entryCount);
            }
        }
        subBlockIndex++;
    } while (subBlockIndex < block->subBlockCount);
}

/* Registry slots, keep-off flags and the scan pool and small globals of this TU (.data and .sdata). */
MapRegisteredNode mapRegisteredSlots[MAP_SCAN_SLOT_COUNT] = { 0 };
int elvKeepOffFlags[32] = {
    0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
    0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
    0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
    0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U
};
int mapScanIndexCount = 0;
MapScanNode *mapScanPoolCursor = mapScanPool;
unsigned char elvActiveMask = 0;
unsigned char elvNoOutMask[7] = { 0 };
