#ifndef INCLUDE_MAIN_UNDULATE_H
#define INCLUDE_MAIN_UNDULATE_H

#include "shared.h"

/*
 * Map collision (src/main/undulate.c, map_scan_node.c): the block, record and scan-tree layouts and the functions
 * the two files share.
 */

/* Height of a probe before any record is hit: the start value of resultHeight and of collisionHeight. */
#define MAP_HEIGHT_NONE (-1000.0f)

/* Block flags: 0x80 fixed up, 3 valid before fixup, 0x83 loaded. Group type 0x0B00 holds the keep-off entries. */
#define MAP_BLOCK_FIXED_UP 0x80
#define MAP_BLOCK_VALID 3
#define MAP_BLOCK_LOADED 0x83
#define MAP_GROUP_KEEP_OFF 0x0B00

/* Number of keep-off flags (elvKeepOffFlags). */
#define MAP_KEEP_OFF_COUNT 32

/* Load-time view of a MapFixupEntry: its three pointer words as plain integers, which UnduInit relocates by the block address. */
typedef struct MapFixupEntryWords {
    unsigned char unmodeled_00[2];
    unsigned short keepId;              /* +0x02 */
    unsigned int pointer04;             /* +0x04 */
    unsigned int pointer08;             /* +0x08 */
    unsigned int pointer0C;             /* +0x0C */
} MapFixupEntryWords;

/* Group of entries: type word at +0x00, entry count at +0x02, relocation flag at +0x03. */
typedef struct MapFixupGroup {
    unsigned short typeWord;            /* +0x00 */
    unsigned char entryCount;           /* +0x02 */
    unsigned char relocated;            /* +0x03 */
    unsigned int firstEntry;            /* +0x04, entry array of block-relative offsets */
} MapFixupGroup;

/* Map block header: flags at +0x00 (0x80 = fixed up, 3 = valid), group count at +0x01, group offsets from +0x04. */
typedef struct MapFixupBlock {
    unsigned char flags;                /* +0x00 */
    unsigned char groupCount;           /* +0x01 */
    unsigned char unmodeled_02[2];
    unsigned int firstGroup;            /* +0x04 */
} MapFixupBlock;

/* Record-set block header read by mapRegisterBlock: type at +0x00, sub-block count at +0x01. */
typedef struct MapBlockHeader {
    unsigned char type;
    unsigned char subBlockCount;
    unsigned char unmodeled_02[2];
} MapBlockHeader;

/* Partial layouts: only the offsets the map files touch are named. */
typedef struct MapRecordKind {
    int kindCode;
    unsigned char unmodeled_04[4];
} MapRecordKind;

/* One collision triangle: three vertices, a plane vector and a kind. */
typedef struct MapRecord {
    unsigned short kind;
    short neighbour[3];                 /* +0x02, -1 marks an open edge */
    unsigned short vertexIndex[3];
    unsigned short planeIndex;
} MapRecord;

/*
 * Triangle record set; also the probe state that the mapProbe* helpers fill. Size 0x1A0: the set mapRegisterBlock
 * builds on its stack in map_scan_node.c.
 */
typedef struct MapRecordSet {
    struct MapCollisionObject *object;  /* +0x00 */
    int recordCount;
    Vector4 *vertices;
    Vector4 *planes;
    MapRecord *records;
    MapRecordKind *kindTable;
    unsigned char unmodeled_18[8];
    Vector4 probeStart;                 /* +0x20 */
    Vector4 origin;                     /* +0x30 */
    Vector4 *motion;                    /* +0x40: displacement the probe moves by, or 0 */
    float heightTolerance;              /* +0x44 */
    float collisionHeight;              /* +0x48: MAP_HEIGHT_NONE until a record is hit */
    MapRecord *collisionRecord;         /* +0x4C */
    unsigned char blockCount;           /* +0x50 */
    unsigned char loopCount;            /* +0x51 */
    unsigned char mask;                 /* +0x52 */
    unsigned char unmodeled_53;
    MapRecord *recordCursor;            /* +0x54 */
    int hitKind;                        /* +0x58 */
    unsigned char unmodeled_5c[4];
    unsigned int keyMask;               /* +0x60 */
    unsigned char unmodeled_64[4];
    unsigned int keyValue;              /* +0x68 */
    unsigned char unmodeled_6c[4];
    float radius;                       /* +0x70 */
    unsigned char unmodeled_74[0x0C];
    float planeA[4];                    /* +0x80 */
    float planeB[4];                    /* +0x90 */
    Vector4 *triangleVertex[3];         /* +0xA0 */
    unsigned char unmodeled_ac[4];
    Vector4 edgeOffset;                 /* +0xB0: written by mapEdgeClosestPoint */
    DoublewordVector4 correction;       /* +0xC0: x and z accumulate the push by mapPushOutOfKeepOff, w is the nearest edge distance */
    unsigned char unmodeled_d0[0x10];
    Vector4 elv;                        /* +0xE0: elevator position of the hit record */
    unsigned char unmodeled_f0[0x30];
    Vector4 savedProbeStart;            /* +0x120 */
    Vector4 savedOrigin;                /* +0x130 */
    float edgeDx;                       /* +0x140 */
    unsigned char unmodeled_144[4];
    float edgeDz;                       /* +0x148 */
    unsigned char unmodeled_14c[4];
    float edgeSx;                       /* +0x150 */
    unsigned char unmodeled_154[4];
    float edgeSz;                       /* +0x158 */
    unsigned char unmodeled_15c[4];
    float deltaX;                       /* +0x160 */
    unsigned char unmodeled_164[4];
    float deltaZ;                       /* +0x168 */
    float hitFlag;                      /* +0x16C */
    unsigned char unmodeled_170[0x30];
} MapRecordSet;

/*
 * One entry of a block group, which is also a collision group: record count at +0x00, keep id at +0x02, vertex array at +0x04
 * (vertices[0] gives the group's Y range), planes, kinds, records from +0x10. UnduInit relocates the three pointers through
 * MapFixupEntryWords. The tag keeps its fixup-entry name because enemy_1.c and xenoActor.h name it; MapRecordGroup is the same type.
 */
typedef struct MapFixupEntry {
    unsigned short recordCount;         /* +0x00 */
    unsigned short keepId;              /* +0x02 */
    Vector4 *vertices;                  /* +0x04 */
    Vector4 *planes;                    /* +0x08 */
    MapRecordKind *kindTable;           /* +0x0C */
    MapRecord records[1];               /* +0x10, recordCount entries */
} MapFixupEntry;

typedef MapFixupEntry MapRecordGroup;

/*
 * Collision object, which is also the request block of the map probes (UnduParamInit resets it, callers set flags, block and the
 * unit masks, UnduCheck fills in the hit): the current and previous record, flag bits (the attribute mask of a probe), the block
 * walk counts (the cross-check counts of a probe), the hit distance, its group and record set, the hit's kind, and the key filter
 * copied into the set (the excluded and included unit masks of a probe). Size 0x40.
 */
typedef struct MapCollisionObject {
    MapRecord *record;                  /* +0x00: the hit record, 0 for none */
    MapRecord *previousRecord;          /* +0x04 */
    unsigned short flags;               /* +0x08: the attribute mask selecting what a probe hits */
    unsigned short bits;                /* +0x0A */
    unsigned char count;                /* +0x0C: cross-check count along x, 16 by default */
    unsigned char loopCount;            /* +0x0D: cross-check count along z, 16 by default */
    unsigned char missCount;            /* +0x0E */
    unsigned char probeEnabled;         /* +0x0F: 0xFF by default, copied from the actor's probeEnabled */
    float heightTolerance;              /* +0x10 */
    float distance;                     /* +0x14: MAP_HEIGHT_NONE until a hit */
    MapRecordGroup *block;              /* +0x18: the group the probe reads, from UnduDataGetHeader */
    MapRecordSet *recordSet;            /* +0x1C */
    int kind;                           /* +0x20: the hit's kind, the attribute word of a probe */
    unsigned char unmodeled_24[4];
    float radius;                       /* +0x28: 0.5 by default */
    unsigned char unmodeled_2c[4];
    unsigned int keyMask;               /* +0x30 */
    unsigned char unmodeled_34[4];
    unsigned int keyValue;              /* +0x38: 1 by default */
    unsigned char unmodeled_3c[4];
} MapCollisionObject;

/* Scan-tree node header; the record indices follow it. */
typedef struct MapScanNode {
    unsigned short recordCount;
    unsigned char splitAxis;
    unsigned char unmodeled_03[1];
    float splitValue;
    unsigned short frontOffset;
    unsigned short backOffset;
    unsigned char unmodeled_0C[4];
} MapScanNode;

/* Scan-set registration: one slot per record set, holding the records pointer and its scan-tree root. */
typedef struct MapRegisteredNode {
    MapRecord *records;
    MapScanNode *root;
} MapRegisteredNode;

f32 UnduCheckSub(const MapRecordSet *set, const MapRecord *record);
int UnduCheckSubHeightCheck(MapRecordSet *set, MapRecord *record, float height);
int mapScanRegisteredNode(MapRecordSet *set);
MapFixupEntry *UnduDataGetHeader(int groupType, int keepId);
f32 UnduCheck(Vector4 *position, const Vector4 *delta, MapCollisionObject *object);
void UnduParamInit(MapCollisionObject *object);
void mapStoreEdgePlanes(float edgePlanes[4]);
void mapPushOutOfKeepOff(MapRecordSet *probe, Vector4 *point);
MapRecordSet *mapSetupCollisionObject(Vector4 *position, const int *delta, MapCollisionObject *object);
void mapRegisterBlock(MapBlockHeader *block);
void mapLoadBlock(MapFixupBlock *block);
void mapBindAndSnapObject(Vector4 *position, MapCollisionObject *object);
void uwamonoPlayerUpdateDummy(void);

#endif /* INCLUDE_MAIN_UNDULATE_H */
