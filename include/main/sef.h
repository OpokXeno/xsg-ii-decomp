#ifndef INCLUDE_MAIN_SEF_H
#define INCLUDE_MAIN_SEF_H

#include "shared.h"
#include "main/sdv.h"

/* Functions of src/main/sef.c that other files call. */

#define SEF_TABLE_RECORD_SIZE 0xA70

/* A track key whose time is 0x400 ends the track: no key follows it. */
#define SEF_TRACK_END_KEY 0x400
#define SEF_LIST_END 1024
/* Effect record flags (record +0x44): running is set while the record runs its script; slots loaded is cleared when
 * sefInitEffectTbl fails; pending is cleared by the frame-3 update; the skip bits exclude a record from registering and
 * from stepping. */
#define SEF_RECORD_RUNNING 0x2
#define SEF_RECORD_SKIP_REGISTER 0x400
#define SEF_RECORD_SKIP_STEP 0x400000
#define SEF_RECORD_SLOTS_LOADED 0x2000000
#define SEF_RECORD_PENDING 0x10000000
/* Record flag that sefCountOwnerGroupRecords does not count and sefClearOwnerGroupFlags keeps. */
#define SEF_RECORD_EXCLUDE_FLAG 0x8000000
/* Pending bit of gameWork.control that holds the effect draw passes back (the X1 SEF_DRAW_FLAG_PENDING value). */
#define SEF_DRAW_FLAG_PENDING 0x04000000
/* Canonical spawn id of the first id range (see sefCanonicalEffectId). */
#define SEF_SPAWN_ID_BASE 0x100C8384

/* Modes of scKeyRecordMode read by sefStartEffect and sefCreateEffect2: 0x3E takes the group path, 0x3D sets record flag 0x800000. */
#define SEF_START_MODE_FLAG 0x3D
#define SEF_START_MODE_GROUP 0x3E

/* Scale volume (cells of the scale box) from which a spawn is large: smaller ones get a count of their own. */
#define SEF_SPAWN_LARGE_VOLUME 9
/* Effect slot table: SEF_SPAWN_SLOT_COUNT slots of SEF_SPAWN_SLOT_SIZE bytes each. */
#define SEF_SPAWN_SLOT_COUNT 32
#define SEF_SPAWN_SLOT_SIZE 0x40
/* Spawn ids: the canonical base, and the special id that spawns through the record path (scGetKeyRecordFirst lookup). */
#define SEF_SPAWN_SPECIAL_ID 0x10000035
/* Entries of the handle table sefHandleSlots. */
#define SEF_HANDLE_SLOT_COUNT 48

/* Shape kinds of sefGetOfsRange (the emission shape of a particle). */
enum SefShapeKind {
    SEF_SHAPE_SPHERE_RANDOM = 0,
    SEF_SHAPE_SPHERE = 1,
    SEF_SHAPE_CUBE_CENTER = 2,
    SEF_SHAPE_CUBE_TOP = 3,
    SEF_SHAPE_CUBE_BOTTOM = 4,
    SEF_SHAPE_CIRCLE_TURNED = 5,
    SEF_SHAPE_CIRCLE_RANDOM = 6
};

/* Modes of sefApplyRecordMode. */
enum SefRecordMode {
    SEF_RECORD_MODE_RELEASE = 1,
    SEF_RECORD_MODE_SET_FLAG_80,
    SEF_RECORD_MODE_CLEAR_FLAG_80,
    SEF_RECORD_MODE_GATE_ON,
    SEF_RECORD_MODE_GATE_OFF,
    SEF_RECORD_MODE_FLAGS_880
};

typedef struct SefEffectEntry {
    signed char lineIndex[32];  /* +0x00: 32 line indices, negative means none (sv_get_image_item.c's view) */
    signed char sortKey[32];    /* +0x20: the second byte of each line */
    int state;
    int flags;
    short key;
    unsigned char unmodeled_04a[1];
    signed char speedPercent;   /* the speed percentage of the entry */
} SefEffectEntry;

typedef struct SefEffectRecord {
    Vector4 position;           /* +0x00: the point sefTransformClipPoint takes */
    Vector4 rotation;           /* +0x10: copied from sefCreateEffectCf2's rotation block, cleared by sefCreateScheduler2 */
    Vector4 scale;              /* +0x20: copied from sefCreateEffectCf2's scale block, (1, 1, 1) at creation */
    Vector4 vertexScale;        /* +0x30: copied by sefSetRecordBlock, (1, 1, 1) at creation */
    int cellSlot;               /* +0x40 */
    int flags;                  /* +0x44: sefSetRecordOwnerMarked sets 0x400 */
    int inUse;                  /* +0x48 */
    int loadTag;                /* +0x4c: copied to sefCurrentLoadTag by sefDestroyRecordEffects */
    int owner;                  /* +0x50 */
    void *ownerHigh;            /* +0x54: the high word of the owner pair, registered with sefRegisterEntry */
    struct SefGroupObject *object; /* +0x58: the object the record belongs to */
    int values[3];              /* +0x5c: set by sefSetRecordValues */
    int requestTail;            /* +0x68: copied to the request by sefSpawnRecordChildren */
    short entryId;              /* +0x6c: the key-table entry index + 9 of the record */
    short gate;                 /* +0x6e */
    float coord_a;              /* +0x70 */
    int ownerKey;               /* +0x74 */
    float coord_b;              /* +0x78 */
    unsigned char spawnEnabled; /* +0x7c: sefSpawnRecordChildren spawns nothing when it is zero */
    unsigned char unmodeled_07d;
    unsigned short spawnBits;   /* +0x7e: bits 0..7 pick kinds 0x136008..0x136001, bits 12..15 pick kind 0x136009 + n */
    unsigned char unmodeled_080[0x82 - 0x80];
    unsigned short loadFlags;   /* +0x82: the third argument of sefInitEffectTbl */
    SefEffectEntry entries[32]; /* +0x84 */
    SdvSequence effectSeq;      /* +0xa04: the effect sequence that sdvExecEffectSeq steps */
    SdvSequence shadowSeq;      /* +0xa0c: the shadow sequence that sdvExecShadowSeq steps */
    SdvSequence alter;          /* +0xa14 */
    SdvCameraSchedule camera;   /* +0xa1c: the camera block (0x28 bytes) that sdvSelectCamera fills */
    SdvSpecial special;         /* +0xa44 */
    SdvSequence wind;           /* +0xa54 */
    float fieldA5C;             /* +0xa5c: zeroed by sefCreateScheduler2, copied to the request +0x314 */
    float cullRadius;            /* +0xa60: set to 8.0 by sefCreateScheduler2 */
    unsigned short value;       /* +0xa64 */
    short remaining;            /* +0xa66 */
    short slotIndex;            /* +0xa68: the second argument of sefInitEffectTbl; negative skips the load */
    signed char linkValue;      /* +0xa6a: the link byte of the entry sefInitEffectTbl linked to (-1 when none) */
    unsigned char effectType;   /* +0xa6b: the effect type srsAnalyzeEftNo returns for the record's effect number */
    unsigned char requestMode;  /* +0xa6c: copied from the request's mode byte (read as a signed char) */
    unsigned char unmodeled_a6d[SEF_TABLE_RECORD_SIZE - 0xa6d];
} SefEffectRecord;

SefEffectRecord *sefCreateEffectCf(int effectId, const Vector4 *position, Vector4 *rotation);
SefEffectRecord *sefSpawnEffectRecordByCode(signed char *text, const Vector4 *position, Vector4 *rotation,
                                            Vector4 *scale);
void sefSetSlotDirectionDefault(Matrix4 *destination, const Vector4 *direction);
void sefSetSlotDirectionBetweenDefault(void *state, Vector4 *minuend, Vector4 *subtrahend);
void sefGetCubePosBtm(Vector4 *pos, const Vector4 *extent);
void sefGetCubePosTop(Vector4 *pos, const Vector4 *extent);

SefEffectRecord *sefGetScheduler(void);
void sefFreeScheduler(int index);

void sefClearLineDataRecords(void);
void sefClearHandleTable(void);
float srsAtan2(float x, float y);

void sefInitEffectBattle(void);
void sefProgressEffect(SefEffectRecord *record, float frames);
void sefExecEffectSteps(SefEffectRecord *record, float frames, float step);

void sefCreateEffect(void *object);
void sefStartEffectAtPosition(void *object, const Vector4 *position);

int sefStartEffectKeyDefault(void *buffer, int effectKey);
void sefLoadEffectCfName(void *buffer, signed char *text);

/* Spawns the effect record for an effect id. Inside an entry (scKeyRecordEntry set) it hands the remapped key to
 * sefCreateKindResourceSlot; otherwise it creates a record, copies the two optional quadword blocks into it and marks it. */
SefEffectRecord *sefCreateEffectCf2(int effectId, const Vector4 *position, Vector4 *rotation, Vector4 *scale,
                                    int slotIndex);

int sefCanonicalEffectId(int effectId);

/* The request block sefGetBattleData returns (sefRequestBuffer): the kind keys, the kind matrices and the kind tables. */
typedef struct SefLinkedContext {
    int kind1Key;                    /* +0x000, the second argument of sefGetKindMatrix for kind 1 */
    int kind5Key;                    /* +0x004 */
    int kind0Key;                    /* +0x008 */
    int kind2Key;                    /* +0x00C */
    int kind3Key;                    /* +0x010 */
    int kind4Key;                    /* +0x014 */
    int kind6Key;                    /* +0x018 */
    int kind;                        /* +0x01C: the request's effect key */
    unsigned char unmodeled_020[0x0C];
    short mode;                      /* +0x02C: the request's mode */
    unsigned char unmodeled_02E[0x80 - 0x2E];
    Matrix4 kind64Matrix;            /* +0x080, copied for kind 64 */
    Matrix4 kind256Matrix;           /* +0x0C0, copied for kind 256 */
    Matrix4 kind128Matrix;           /* +0x100, copied for kinds 128 and 162 */
    Matrix4 kind1024Matrix;          /* +0x140, sefApplyPartTransform for bit 0x400 */
    Matrix4 kind2048Matrix;          /* +0x180, sefApplyPartTransform for bit 0x800 */
    Matrix4 kind4096Matrix;          /* +0x1C0, sefApplyPartTransform for bit 0x1000 */
    Matrix4 kind512Matrix;           /* +0x200, sefApplyPartTransform for bit 0x200 */
    Matrix4 kind128AltMatrix;        /* +0x240, bit 0x80 when sefUseAltMatrix is set */
    unsigned char unmodeled_280[0x94];
    float kind1VectorY;              /* +0x314: overrides y of the kind-1 slot vector when nonzero */
    union {
        int objectOffset;            /* +0x318: read as an offset by sefGetLinkedFlag */
        struct SefKindTable *kind1Table; /* +0x318: the kind-1 table of sefGetKindSlotVector */
    };
    struct SefKindTable *kind5Table;
    struct SefKindTable *kind0Table;
    struct SefKindTable *kind2Table;
    struct SefKindTable *kind3Table;
    struct SefKindTable *kind4Table;
    struct SefKindTable *kind6Table;
} SefLinkedContext;

SefLinkedContext *sefGetBattleData(void);

/* The effect request at the start of sefRequestBuffer as the sdv sequences read it (sdv.c): the actors the
 * select bits of a key row pick. */
typedef struct SefRequest {
    struct XenoActor *source;   /* +0x00 */
    unsigned char unmodeled_04[4];
    struct XenoActor *target;   /* +0x08 */
    struct XenoActor *links[3]; /* +0x0C the actors SDV_KEY_LINK_1..3 select */
    unsigned char unmodeled_18[0x24 - 0x18];
    unsigned char waitHit;      /* +0x24 nonzero: hit-wait rows report their row */
} SefRequest;

void sefMemClear(void *dest, unsigned int size);
void sefMemZero(void *dest, int size);
void sefMemFillWords(void *dest, int value, int size);

void sefNoiseInit(void);

void sefResetEffectSlots(void);

/* sefSlotTable: the effect number of each of the eight wait slots and the flag that asks
 * srsLoadPendingEffects (srs.c) to load its package. */
typedef struct SefSlotTable {
    short effectNo[8];                 /* +0x00 */
    unsigned char unmodeled_10[0x20 - 0x10];
    short loadPending[8];              /* +0x20 */
} SefSlotTable;

int sefCheckLoad(void);
void sefReleaseID(int key);
void sefSetGateForId(int owner, int id);
void sefSetGateDefault(int owner, int id);
void xglCdWaitUntilIdle(void);

int sefFlagTest(int mask);

/* Counts the entries of record that are active and have flag 0x8000. */
int sefCountMarkedEntries(SefEffectRecord *record);

int sefCountOwnerGroupRecords(int owner, unsigned short value);
void sefClearOwnerGroupFlags(int owner, unsigned short value, int mask);
void sefKillEffect(int owner);
int sefOwnerPairInUse(void *owner, int ownerKey);
int sefGetEntryFlags(struct XenoActor *actor);
int sefIsHitActor(struct XenoActor *actor);
int sefEntryHasFlag2(struct XenoActor *actor);
void sefPruneHandleSlots(void);
void sefRegisterHandle(int handle, void *value);

/* Five words per effect entry, 0x14 bytes each (sefEntryCounts, cleared by the sef init chain). */
typedef struct SefEntryCounts {
    int worldSprites;                                /* +0x00: packet space of the world sprites */
    int screenSprites;                               /* +0x04: packet space of the screen sprites */
    int modelPoints;                                 /* +0x08: point count of the models */
    int total;                                       /* +0x0c: the sum of the three counts (sefSumEntryCounts) */
    int unmodeled_10;
} SefEntryCounts;

extern SefEntryCounts sefEntryCounts[];
/* Address of the counter entry entry_index (sef_entry_counts.c). */
void *sefEntryCountsEntryGet(int entry_index);
void sefExecEffect(void);

struct XenoActor;
struct SefLocalData;
struct SefGroupObject;
/* The owner record of the resource slots (its key word at +0x74); the struct is defined in sef.c. */
typedef struct SefOwner SefOwner;

/* Effect start, load and teardown. */
void sefInitEffect(void);
void sefInitEffectCf(void);
void sefDestroyEffect(void);
void sefDestroyEffectCf(void);
void sefPushEffect(void);
void sefPopEffect(void);
void sefSetRenderHooks(void);
void sefDrawEffect(void);
void sefLoadEffect(int key, int group);
void sefSetupPlayer(int key);
void sefSetupEnemy(int key);
void sefSetLoadQue(int id, int slot);
void sefResetHandleSlots(void);
void sefReleaseImageRecords(void);
int sefExecScheduler(SefEffectRecord *only);

/* Effect records. */
void sefDeleteEffectCf(SefEffectRecord *record);
void sefSetRecordOwner(SefEffectRecord *record, void *owner);
void sefSetRecordOwnerMarked(SefEffectRecord *record, void *owner);
void sefSetRecordValues(SefEffectRecord *record, struct XenoActor *first, struct XenoActor *second,
                        struct XenoActor *third);
void sefDestroyRecordEffectsClearFlag(SefEffectRecord *record);
void sefApplyRecordMode(void *owner, int ownerKey, int mode);
void sefReleaseUnitRecords(void *owner, int releaseAll);
void sefSpawnEffectById(int effectId, Vector4 *position, Vector4 *outVector, Vector4 *scale, float radius, int count);
void uwamonoReleaseOwnedRecords(SefOwner *owner);

/* Owner keys. */
void sefSetCurrentOwner(int owner);
int sefGetCurrentOwner(void);
void sefSetOwnerFlags(int owner, int flags);
void sefClearOwnerFlags(int owner, int mask);
int sefIsFinishEffect(int owner);
int sefIsFinishEffect2(int owner);
int sefSearchMapperIndex(int key);

/* Geometry helpers of the effect scripts. */
void sefApplyPartTransform(Vector4 *destination, Vector4 *source, int partKind);
void sefGetCirclePos(Vector4 *pos, float radius);
int sefGetKindVector(Vector4 *out, int unused, int index, int kind);
void sefGetPosition(Vector4 *dest, const Vector4 *source, int kind);
Matrix4 *sefGetPoint(Vector4 *dest, const Vector4 *source, int kind);
void sefMergeMatrixPos(Matrix4 *dest, Matrix4 *matrix, const Vector4 *position);
void sefCalcInvView(Matrix4 *dest);
int sefCalcRotTransSMatrix(const Vector4 *rotation, const Vector4 *translation, const Vector4 *scale, Matrix4 *out);
void sefLoadRequestBuffer(int p0, int p1, int p2, int p3, int p4, int p5, int p6, int p7);
float sefNoiseNext(void);
float sefRandf(void);
float sefRandAngle(void);
void sefNoiseNextVector(Vector4 *pos);
float sefGetEntryScale(int id);
/* The fog override block sefSetFogOverride fills (the statics sefOverrideFlag and the two override vectors of sef.c,
 * one object in the original): the enabled word, four fog distances at +0x10 and the fog colour at +0x20. */
typedef struct SefFogOverride {
    int enabled;
    unsigned char unmodeled_04[0x10 - 0x04];
    float dist[4];
    Vector4 fogColor;
} SefFogOverride;

SefFogOverride *sefGetFogOverrideBlock(void);
void sefSetFogOverride(Vector4 *vectorA, Vector4 *vectorB);
struct SefLocalData *sefGetLineAdr(int entryIndex);
unsigned char *sevGetPtAllocator2(int slotIndex);
int sefGetLinkedFlag(int linkIndex);

/* Actor access: the local matrix and the point of an actor's joint. */
int sefActorMulLocalMatrix(struct XenoActor *actor, int input, void *output, int apply);
int sefActorMulLocalVector(struct XenoActor *actor, int input, void *output, int apply);

#endif /* INCLUDE_MAIN_SEF_H */
