#ifndef INCLUDE_MAIN_TIMRPACKAGE_H
#define INCLUDE_MAIN_TIMRPACKAGE_H

/* The motion package (TiMRPackage.c) and the data its solvers read: the hierarchy, the animation data and the node table. */

#include "shared.h"

typedef struct TiMRState TiMRState;

typedef struct TiMRPackage TiMRPackage;

typedef struct OffsetTable OffsetTable;

typedef struct TiOffsetEntry TiOffsetEntry;

typedef struct TiPamData TiPamData;

typedef struct TiHrcData TiHrcData;

typedef struct TiHrcDynamics TiHrcDynamics;

typedef struct TiSolverState TiSolverState;

typedef struct TiNodeTable TiNodeTable;

/*
 * One state block, reached at +0x10 (state A) and +0x60 (state B) of TiMRPackage.
 * Its first word is a flag word: the advance functions test bits 0x2, 0x8 and
 * 0x20 of it and the table selectors set bit 0x40. `speed` (+4) is the rate the
 * advance functions multiply by the global step; `position` (+8) is the value
 * they accumulate and compare with the range. The block size of 0x50 follows
 * from the second block's offset. `blendFrames` (+0xC) is the blend length in frames: it is stored by
 * tiMRStateSetBlendFrames, passed by XenoActorSetBlendFrames (and its B variant) to the channel blocks of XenoActor at
 * +0x650 and +0x6A0, and read by the blend code of TiMtnBlend.c.
 * State A also holds the aim target of tiMotionAimTarget (flag 0x10 applies its angle): the world
 * position to aim at, the previous frame's aim angles (aimPrevious) and the current rotations.
 */
struct TiMRState {
    int flags;      /* +0x0: flag word (tiWordSetBits/tiWordClearBits) */
    float speed;
    float position;
    float blendFrames;
    int rangeMin;   /* +0x10: the range of the advance (tiMRStateSetRange) */
    int rangeMax;   /* +0x14 */
    unsigned char unmodeled_18[0x8];
    Vector4 aimVector;   /* +0x20 */
    float aimPrevious[4];  /* +0x30: the previous frame's aim angles */
    float aimRotateX;    /* +0x40 */
    float aimAngle;      /* +0x44 */
    float aimRotateZ;    /* +0x48 */
    unsigned char unmodeled_4C[0x4];
};

/* One solver body (the pDynMat entry of a dynamics node, 0x40 bytes): the point, its velocity and
 * the node. The vectors are DoublewordVector4 unions: the pair solvers copy and add them through the union, so
 * their stores carry alias set 0 (check.py). */
struct TiSolverState {
    unsigned char previous_position[0x10];
    DoublewordVector4 position;
    DoublewordVector4 velocity;
    TiHrcDynamics *node;
    float gust;               /* +0x34: wind gust factor, redrawn by the wind passes */
    unsigned char unmodeled_38[0x8];
};

typedef struct TiSolverState TiDynamicsOut;

/*
 * The package. The first word, the target node of the aim at +0x04, the two state blocks, the scale
 * vector, the entry and table pointers of each state and the pointer fields the per-frame update checks
 * are evidenced. The memset in the initializer covers the whole 0x100 bytes.
 */
struct TiMRPackage {
    int solverPasses;
    int targetNode;           /* +0x04: node index that the aim solver targets (0x1D after the initializer) */
    unsigned char unmodeled_08[8];
    TiMRState state_a;
    TiMRState state_b;
    float scale[3];           /* +0xB0: (1, 1, 1) after the initializer; multiplies the translation of the second locator */
    unsigned char unmodeled_bc[4];
    TiOffsetEntry *entryA;    /* +0xC0: entry of state A; the update checks its version float as pAnimData (xac) */
    OffsetTable *entryTableA; /* +0xC4: offset table of state A (tiMRPackageSelectAnimEntry) */
    TiOffsetEntry *entryB;    /* +0xC8: entry of state B; the update checks its version float as pFaceAnimData (fac) */
    OffsetTable *entryTableB; /* +0xCC: offset table of state B (tiMRPackageSelectFaceAnimEntry) */
    TiHrcData *hrcData;       /* +0xD0: pHrcData, version float at +4 */
    TiPamData *pamData;       /* +0xD4: parts animation data, its two shorts at +8 and +A bound state A */
    TiNodeTable *nodeTable;   /* +0xD8: pStatus, the node point table (tiMRPackageSetEntryTableC) */
    Matrix4 *localMat;        /* +0xDC: pLocalMat */
    Matrix4 *globalMat;       /* +0xE0: pGlobalMat */
    Matrix4 *blendMat;        /* +0xE4 */
    Matrix4 *partsMat;        /* +0xE8 */
    TiDynamicsOut *dynMat;    /* +0xEC: pDynMat */
    Matrix4 *placeMat;        /* +0xF0: pPlaceMat */
    unsigned char unmodeled_f4[0xC];
};

/*
 * Counted table of entries: the entry count sits at +8 and a table of byte
 * offsets, relative to the table itself, starts at +0xC.
 */
struct OffsetTable {
    unsigned char unmodeled_00[8];
    unsigned int count;      /* +8 */
    int entryOffsets[1];     /* +0xC: byte offset of entry i from the table */
};

/* Animation data of the first format (version below 1.4): the joint, second joint and dynamics tables. */
typedef struct TiXacTables1 {
    int jointCount;
    int joint2Count;
    int unmodeled_18;
    int dynamicsCount;
    int jointOffset;
    int joint2Offset;
    int unmodeled_28;
    int dynamicsOffset;
} TiXacTables1;

/* Animation data of the second format: it adds the skip, third joint and visibility tables. */
typedef struct TiXacTables2 {
    int jointCount;
    int joint2Count;
    int skipCount;
    int dynamicsCount;
    unsigned int visibilityCount;
    int joint3Count;
    int oldSkipOffset;
    int unmodeled_2C;
    int jointOffset;
    int joint2Offset;
    int skipOffset;
    int dynamicsOffset;
    int visibilityOffset;
    int joint3Offset;
} TiXacTables2;

/*
 * One entry of an offset table, bound to a state as its animation data: the
 * per-frame update checks its version float at +4; its float range is at +8
 * (min) and +C (max). The tables from +0x10 have one of two layouts by version.
 */
struct TiOffsetEntry {
    unsigned char unmodeled_00[4];
    float version;   /* +4 */
    float rangeMin;  /* +8 */
    float rangeMax;  /* +C */
    union {
        TiXacTables1 first;
        TiXacTables2 second;
    } tables;        /* +0x10 */
};

/* The parts animation data at +0xD4 of the package: two shorts bound state A (lower at +8, upper at +A), the joint
 * curves follow from +0xC (tiPartsAnimSamplePose reads them). */
struct TiPamData {
    unsigned char unmodeled_00[8];
    short lowerLimit;  /* +8 */
    short upperLimit;  /* +A */
    unsigned char curves[0]; /* +0xC */
};

/* One transform: parent index, offset, rotation and scale. */
typedef struct TiMotionTransform {
    int parent;
    float offset[3];
    float rotation[3];
    float scale[3];
} TiMotionTransform;

/* Every node record that the transform readers (TiXacAnimCurve.c) evaluate keeps nine stored channel defaults (translation,
 * rotation and scale), used when a channel's flag bit is clear, and a flags word (0x2) that the reader sets from the header. */

/* One locator of the hierarchy (0x40 bytes). */
typedef struct TiHrcLocator {
    int parent;
    float translationDefault[3];
    float rotationDefault[3];
    float scaleDefault[3];
    float scale[3];
    int flags;
    unsigned char unmodeled_38[0x8];
} TiHrcLocator;

/* One joint of the hierarchy (0x50 bytes). The default translation is also the range the aim solver reads. */
typedef struct TiHrcJoint {
    int parent;
    float translationDefault[3];
    float orient[3];
    float rotationDefault[3];
    float scaleDefault[3];
    float scale[3];
    float rotationScale;
    int flags;
    unsigned char unmodeled_48[0x8];
} TiHrcJoint;

/* One collision entry of a dynamics node (0x10 bytes): the index of a record of the hierarchy's sphere, matrix or
 * capsule table (see TiDynamicsColl.h). */
typedef struct TiCollEntry {
    int recordIndex;
    unsigned char unmodeled_04[0xC];
} TiCollEntry;

/* One dynamics node: 0xB0 bytes followed by its collision entries (sphereCount spheres, then matrixCount matrix
 * records, then capsuleCount capsules). The solver reads the first doubleword (parent and solverFlags) whole. */
struct TiHrcDynamics {
    union {
        unsigned long long header;
        struct {
            int parent;
            unsigned int solverFlags; /* +0x04: bit 0x2 and 0x4 zero the local z and y of the position (tiDynamicsNodeSolvePalette) */
        };
    };
    float boneLength;         /* +0x08: distance kept from the parent */
    float gravity;            /* +0x0C: subtracted from the velocity y each step */
    float damping;            /* +0x10: the velocity is scaled by it after each step */
    float pullScale;          /* +0x14: how strongly the point is pulled to its target */
    float windMultiplier;     /* +0x18: multiplies the wind push (tiWindDirApply) */
    unsigned int sphereCount; /* +0x1C */
    unsigned int matrixCount;
    unsigned int capsuleCount;
    int flags;
    float weight;
    Vector4 translationDefault;
    float orient[3];
    unsigned char unmodeled_4C[0x4];
    float rotationDefault[3];
    unsigned char unmodeled_5C[0x4];
    float scaleDefault[3];
    unsigned char unmodeled_6C[0x4];
    float scale[3];
    unsigned char unmodeled_7C[0x34];
    TiCollEntry entries[0];
};

/* One static node (0x40 bytes): a fixed transform. */
typedef struct TiHrcStatic {
    TiMotionTransform transform;
    unsigned char unmodeled_28[0x10];
    int id;                     /* +0x38: the joint id that XenoActorAttachChildToJoint searches for */
    unsigned char unmodeled_3C[0x4];
} TiHrcStatic;

/* One skip (face) node of the table at hrc->skipOffset (0x70 bytes): a rotation evaluated against per-axis limits. */
typedef struct TiHrcSkip {
    int parent;
    unsigned int limitFlags;
    int flags;
    float translationDefault[3];
    float orient[3];
    float rotationDefault[3];
    float scaleDefault[3];
    float scale[3];
    unsigned char unmodeled_48[0x4];
    float limit[6];
    unsigned char unmodeled_64[0xC];
} TiHrcSkip;

/*
 * The hierarchy data at +0xD0 of the package: the node counts and table offsets, the collision record
 * tables (spheres, matrix records and capsules, see TiDynamicsColl.h) and the nodes themselves.
 */
struct TiHrcData {
    unsigned char unmodeled_00[0x4];
    float version;          /* +4 */
    unsigned char unmodeled_08[0x4];
    unsigned int transformCount; /* +0xC: transforms of the hierarchy; the actor allocates one 0x40-byte matrix each */
    int jointCount;
    int joint2Count;
    int joint3Count;
    int skipCount;
    int dynamicsCount;
    int staticCount;
    int joint2Offset;
    int joint3Offset;
    int skipOffset;
    int dynamicsOffset;
    int staticOffset;
    unsigned int sphereCount;   /* +0x3C */
    unsigned int matrixCount;
    unsigned int capsuleCount;
    int linkCellCount;          /* +0x48: cells of the link table (tiDynamicsSolveCellPairs) */
    int sphereOffset;           /* +0x4C: offsets of the collision records from the data */
    int matrixOffset;
    int capsuleOffset;
    int linkCellOffset;         /* +0x58: offset of the link table from the data */
    unsigned char unmodeled_5C[0x14];
    TiHrcLocator locators[2];
    TiHrcJoint joints[1];
};

/* One entry of the indexed node table (0x1C bytes): its matrix index and transform. */
typedef struct TiNodeSlot {
    int matrixIndex;
    float offset[3];
    float rotation[3];
} TiNodeSlot;

/* One entry of the keyed node table (0x20 bytes): the id it is searched by, then its matrix index and transform. */
typedef struct TiNodeKey {
    int id;
    int matrixIndex;
    float offset[3];
    float rotation[3];
} TiNodeKey;

/* The node table at +0xD8 of the package: the file version at +4 (below 1.1f the slots are indexed, else the keys are
 * searched by id), the entry count at +8 and the entries from +0x10. */
struct TiNodeTable {
    unsigned char unmodeled_00[4];
    float version;
    unsigned int count;
    unsigned char unmodeled_0C[4];
    TiNodeSlot slots[0];
};

void tiWordClearBits(int *word, int bits);
void tiWordSetBits(int *word, int bits);
void tiWordZero(int *word);

void tiMRPackageInit(TiMRPackage *pMRPackage);
void tiMRPackageSetLocalMat(TiMRPackage *pMRPackage, Matrix4 *value);
void tiMRPackageSetGlobalMat(TiMRPackage *pMRPackage, Matrix4 *value);
void tiMRPackageSetBlendMat(TiMRPackage *pMRPackage, Matrix4 *value);
void tiMRPackageSetPartsMat(TiMRPackage *pMRPackage, Matrix4 *value);
void tiMRStateSetRange(TiMRState *pState, int rangeMin, int rangeMax);
void tiMRPackageSetAnimData(TiMRPackage *pMRPackage, TiOffsetEntry *entry);
void tiMRPackageSetAnimTable(TiMRPackage *pMRPackage, OffsetTable *table);
void tiMRPackageSetFaceAnimData(TiMRPackage *pMRPackage, TiOffsetEntry *entry);
int tiMRPackageIsAnimEntryValid(TiMRPackage *pMRPackage, int unused, unsigned int index);
void tiMRPackageSetFaceAnimTable(TiMRPackage *pMRPackage, OffsetTable *table);
void tiMRPackageSetHrcData(TiMRPackage *pMRPackage, TiHrcData *value);
void tiMRPackageSetClampRange(TiMRPackage *pMRPackage, TiPamData *pam);
void tiMRPackageSetEntryTableC(TiMRPackage *pMRPackage, TiNodeTable *value);
void tiMRStateSetSpeed(TiMRState *pState, float value);
void tiMRStateSetPosition(TiMRState *pState, float value);
void tiMRStateSetBlendFrames(TiMRState *pState, float frames);
void tiMRPackageSetSolverPasses(TiMRPackage *pMRPackage, int value);
void tiMRPackageSetVectorUniform(TiMRPackage *pMRPackage, float value);
void tiMRPackageSetVector(TiMRPackage *pMRPackage, float x, float y, float z);
void tiMRPackageAdvanceAnim(TiMRPackage *pMRPackage);
void tiMRPackageAdvanceAnimUnwrapped(TiMRPackage *pMRPackage);
void tiMRPackageAdvanceFaceAnim(TiMRPackage *pMRPackage);
void tiMRPackageAdvanceFaceAnimUnwrapped(TiMRPackage *pMRPackage);
void tiMRPackageUpdate(TiMRPackage *pMRPackage);
void tiMRPackageSelectAnimEntry(TiMRPackage *pMRPackage, unsigned int index);
void tiMRPackageSetAnimTableAndEntry(TiMRPackage *pMRPackage, OffsetTable *table, unsigned int index);
void tiMRPackageSelectFaceAnimEntry(TiMRPackage *pMRPackage, unsigned int index);

#endif /* INCLUDE_MAIN_TIMRPACKAGE_H */
