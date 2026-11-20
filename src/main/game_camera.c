#include "common.h"
#include "main/script_register_natives.h"
#include "main/ladder_table.h"
#include "main/game_pause_disp.h"
#include "main/xgl_camera.h"
#include "main/pad.h"
#include "main/se_bank_load.h"
#include "main/game_data.h"
#include "main/game_map_object.h"
#include "main/game_camera.h"
#include "main/set_motion.h"
#include "main/xenoActor.h"
#include "main/xgl_font.h"
#include "main/xgl_2.h"
#include "main/sef.h"
#include "main/xgl_cd.h"
#include "main/msg_state.h"
#include "main/xgl_studio.h"
#include "main/get.h"
#include "main/undulate.h"
#include "main/elevator.h"
#include "main/uwamono_event_check.h"
#include "main/get_ladder.h"
#include "main/res_get_path.h"

/* Studio camera state of the game camera (xglStudioGetCamera2(0)->state). */
#define CAMERA_STATE_GAME 4
/* Frames the camera update waits at start (scratchpad phase). */
#define CAMERA_INITIAL_WAIT 40
/* Bits of gameWork.camera.flags. */
#define CAMERA_FLAG_DISABLED 0x8            /* the camera is not updated */
#define CAMERA_FLAG_TRACK_PLAYER 0x20       /* refresh the old player position and the track index */
#define CAMERA_FLAG_MODE_CHANGED 0x40       /* a new camera block was selected */
#define CAMERA_FLAG_SNAP 0x80               /* the slots are cleared and the camera jumps */
#define CAMERA_FLAG_VIEW_ONLY 0x200         /* only cameraApplyView runs */
/* Angles, in radians. */
#define CAMERA_PI 3.1415927f
#define CAMERA_TWO_PI 6.2831855f
#define CAMERA_HALF_PI 1.5707964f
/* Defaults of a camera record (cameraRangeSetDefaults). */
#define CAMERA_DEFAULT_PITCH (-0.39269909f)  /* -22.5 degrees */
#define CAMERA_DEFAULT_PERSPECTIVE 0.69813174f /* 40 degrees */
#define CAMERA_DEFAULT_DISTANCE 5.0f
#define CAMERA_CENTER_HEIGHT 1.2f           /* the look-at offset above the anchor */
/* Camera types of a camera record, selected in cameraUpdateFrame. */
#define CAMERA_TYPE_FOLLOW 0                /* cameraUpdateFollow */
#define CAMERA_TYPE_RECORD 1                /* cameraUpdateFromRecord */
/* Probe modes of a camera record (CameraRecord.range.probeMode): zero pulls the camera in along the line of
   sight, anything else searches the orbit pitch. */
#define CAMERA_PROBE_MODE_SLIDE 33          /* the first probe slides (cameraProbeSlide) */
#define CAMERA_PROBE_MODE_RESLIDE 17        /* the found height is probed again with a slide */
/* The orbit pitch search of cameraSearchOrbitPitch. */
#define CAMERA_SEARCH_STEP_FINE 0.052359883f /* 3 degrees */
#define CAMERA_SEARCH_STEP_COARSE 0.785398185f /* 45 degrees */
#define CAMERA_SEARCH_PITCH_DOWN (-1.57079637f)
#define CAMERA_SEARCH_TOLERANCE 0.01f
#define CAMERA_SEARCH_MAX_STEPS 60
/* uwamono records: 256 of them; kind 4 is the player, kind 5 a part attached to it. */
#define UWAMONO_RECORD_COUNT 256
#define UWAMONO_KIND_TALK 1
#define UWAMONO_KIND_PLAYER 4
#define UWAMONO_KIND_PART 5
/* Draw flags of a XenoActor (drawFlags). */
#define UWAMONO_DRAW_HIDDEN 0x4
/* uwamonoChangePlayerModel: the request that takes the key mapped from the current record, and the key flags. */
#define UWAMONO_KEY_FROM_CURRENT (-1000)
#define UWAMONO_KEY_MASK 0x7FFFFFFF
#define UWAMONO_ES_KEY_FIRST 0x10000
#define UWAMONO_ES_KEY_COUNT 0x4000
/* The form value of an ES (the mecha the party rides) in record+0x338. */
#define UWAMONO_FORM_ES 1.0f
/* Joints the player parts attach to: the ES pair, then the walking pair. */
#define UWAMONO_PART_JOINT_ES_0 80
#define UWAMONO_PART_JOINT_ES_1 81
#define UWAMONO_PART_JOINT_WALK_0 10
#define UWAMONO_PART_JOINT_WALK_1 7
/* UwamonoAttachIdTable (uwamonoAttachIdTable): eight entries of {lo, hi, id0, id1}. */
#define UWAMONO_ATTACH_ENTRY_WORDS 4
#define UWAMONO_ATTACH_TABLE_WORDS 32
#define UWAMONO_ATTACH_PARTS 2

/* gameWork.control and gameWork.camera.flags as union members (alias set 0). Lever: the camera flags mask in
   cameraAdvanceTracks and the control reads and the |= 0x400 of the ladder block keep their order only through
   this view; every other access uses the plain members. */
typedef struct {
    unsigned char unmodeled_000[8];
    union { int control; };
    unsigned char unmodeled_00C[0xC4];
    union { unsigned short flags; };
} GameWorkAlias0;

/* One camera record of cameraCfSlots (208 bytes): its flag is cleared and its index set to -1 by GameCameraReset. */
struct CameraRecord {
    unsigned char unmodeled_00;
    unsigned char flag01;               /* +0x01: cleared by GameCameraReset */
    signed char index02;                /* +0x02: -1 after GameCameraReset */
    unsigned char unmodeled_03[0xD];
    struct CameraSlot slot;             /* +0x10: the slot settings of this record */
    struct CameraRange range;           /* +0x20: the range cleared by cameraRangeSetDefaults */
    unsigned char unmodeled_90[0x40];
};

/* File names of the map object resources (matumoto\\*.pxy models, *.txy textures, spline.dat, enemyorder.dat,
   treasurebox.dat, kowaremono.dat): the first argument of each GameResLoadName call in mapObjectResourcesLoad. */

/* Loads every map object resource named in mapObjectResourceNames (models, textures and data files), waiting for each CD read, and keeps
   entries 1 and 2 in uwamonoEventTable and uwamonoKeyRowTable. */
void mapObjectResourcesLoad(void)
{
    int i;

    for (i = 0; i < MAP_RESOURCE_COUNT; i++) {
        GameResLoadName(mapObjectResourceNames[i], &mapObjectResources[i], 0, 0);
        while (xglCdServiceIfPending() != 0) {
        }
    }
    uwamonoEventTable = mapObjectResources[MAP_RESOURCE_TREASURE_BOX];
    uwamonoKeyRowTable = mapObjectResources[MAP_RESOURCE_ENEMY_ORDER];
}

#include "shared.h"

/* 32 camera records (0x1A00 bytes), copied whole. */
typedef struct CameraSlotBlock {
    union {
        DoublewordAlignedVector4 quadwords[0x1A00 / 16];
        struct CameraRecord record[0x1A00 / 0xD0];
    };
} CameraSlotBlock;

/* 0x280 bytes of the camera slot table, copied with a runtime alignment test. */
typedef struct CameraSlotTable {
    int unmodeled_000[0x280 / 4];
} CameraSlotTable;

extern CameraSlotBlock cameraCfSlots;
/* Three words each, cleared by GameCameraReset; their meaning is not established yet. */
typedef union CameraWords3 {
    int word[3];
    Vector4 v;
    int w[4];
} CameraWords3;

static MapCollisionObject cameraPlayerProbe;
static DoublewordAlignedVector4 cameraPlayerPositionOld;
static MapCollisionObject CamLookAt;
static CameraWords3 CamLookAtPos;
static MapCollisionObject cameraPositionProbe;
static CameraWords3 cameraProbePosition;
static CameraSlotBlock cameraCfSlotsSave;
extern CameraSlotTable cameraSlotTable;
static CameraSlotTable cameraSlotTableSave;
static unsigned char cameraTrackIndexSave;

/* Saves the camera slot records, the slot table and the track index to their save blocks. */
void GameCameraStateSave(void)
{
    cameraCfSlotsSave = cameraCfSlots;
    cameraSlotTableSave = cameraSlotTable;
    cameraTrackIndexSave = gameWork.camera.trackIndex;
}

/* A 128-bit GPR value: stored with sq. */
typedef unsigned int XglQuadword __attribute__((mode(TI)));


/* Restores the camera slot records, the slot table and the track index from their save blocks. */
void GameCameraStateRestore(void)
{
    int keptBit;

    cameraCfSlots = cameraCfSlotsSave;
    cameraSlotTable = cameraSlotTableSave;
    cameraSetIndex(cameraTrackIndexSave);
    keptBit = gameWork.control & 0x400;
    gameWork.control &= ~0x400;
    cameraUpdateFrame();
    gameWork.control |= keptBit;
}

/* Gives a camera range its defaults: no anchor, the look-at height offset, the full angle limits, the default pitch and
   distance, and the default perspective. */
void cameraRangeSetDefaults(struct CameraRange *range)
{
    range->anchorIndex = -1;
    range->anchorSubIndex = -1;
    range->rangeMode = 1;
    range->rangeFlag = 1;
    range->offset.x = 0.0f;
    range->offset.y = CAMERA_CENTER_HEIGHT;
    range->offset.z = 0.0f;
    range->angleLimits.x = -CAMERA_HALF_PI;
    range->angleLimits.y = CAMERA_HALF_PI;
    range->angleLimits.z = -CAMERA_TWO_PI;
    range->angleLimits.w = CAMERA_TWO_PI;
    range->angles.x = CAMERA_DEFAULT_PITCH;
    range->angles.y = 0.0f;
    range->angles.w = CAMERA_DEFAULT_DISTANCE;
    range->lockPosition.w = CAMERA_DEFAULT_PERSPECTIVE;
}


/* Puts the studio camera in the game state, parked far away at the default pitch and facing pi. */
void cameraStudioReset(void)
{
    XglCamera *camera;
    float parkX;
    float parkY;
    float parkZ;

    camera = xglStudioGetCamera2(0);
    camera->state = CAMERA_STATE_GAME;
    parkX = 1000.0f;
    camera->travel.position.x = parkX;
    parkY = 1000.0f;
    camera->travel.position.y = parkY;
    parkZ = 1000.0f;
    camera->travel.position.z = parkZ;
    camera->travel.rotation.x = CAMERA_DEFAULT_PITCH;
    camera->travel.rotation.y = CAMERA_PI;
    camera->travel.rotation.z = 0.0f;
}

void *memset(void *dest, int value, unsigned int count);

void cameraInit(void)
{
    float perspective;
    float startY;
    float startZ;

    cameraStudioReset();
    memset(&gameWork.camera.flags, 0, 432);
    gameWork.camera.flags = 0x80;
    cameraRangeSetDefaults(&gameWork.camera.range);
    perspective = CAMERA_DEFAULT_PERSPECTIVE;
    startY = 3.0f;
    startZ = 10.0f;
    gameWork.camera.to.position.w = perspective;
    do {
    } while (0);
    gameWork.camera.range.lockPosition.y = startY;
    gameWork.camera.range.lockPosition.z = startZ;
    gameWork.camera.range.lookAt.z = 0.0f;
    do {
    } while (0);
    gameWork.camera.from.position.w = perspective;
    do {
    } while (0);
    gameWork.camera.range.lockPosition.x = 0.0f;
    do {
    } while (0);
    gameWork.camera.range.lookAt.x = 0.0f;
    gameWork.camera.range.lookAt.y = 0.0f;
    do {
    } while (0);
}

/* One 20-byte entry of cameraSlotTable: two index bytes set to -1 and a word cleared by GameCameraReset. */
struct CameraSlotEntry {
    signed char indexA;                 /* +0x00 */
    signed char indexB;                 /* +0x01 */
    unsigned char unmodeled_02[0xA];
    int word0C;                         /* +0x0C */
    unsigned char unmodeled_10[0x4];
};


void GameCameraReset(void)
{
    struct CameraRecord *record;
    int i;

    cameraInit();
    record = cameraCfSlots.record;
    for (i = 31; i >= 0; i--) {
        memset(record, 0, 208);
        record->flag01 = 0;
        record->index02 = -1;
        cameraRangeSetDefaults(&record->range);
        record++;
    }
    {
        struct CameraSlotEntry *entry = (struct CameraSlotEntry *)&cameraSlotTable;
        signed char none = -1;
        int j;

        for (j = 31; j >= 0; j--) {
            entry->indexA = none;
            entry->indexB = none;
            entry->word0C = 0;
            entry++;
        }
    }
    CamLookAtPos.word[0] = 0;
    CamLookAtPos.word[1] = 0;
    CamLookAtPos.word[2] = 0;
    cameraProbePosition.word[0] = 0;
    cameraProbePosition.word[1] = 0;
    cameraProbePosition.word[2] = 0;
    gameWork.camera.mode = 0;
    gameWork.camera.trackIndex = -1;
    GameCameraChangeID(-1, 0, 0);
}


void GameCameraChangeID(int mode, int arg1, int arg2)
{
    struct MapFixupEntry *probeBlock;

    if (gameWork.camera.mode == mode && CamLookAt.block != 0 && cameraPositionProbe.block != 0) {
        return;
    }
    probeBlock = 0;
    if (mode >= 0) {
        probeBlock = UnduDataGetHeader(512, mode);
        if (probeBlock == 0) {
            probeBlock = UnduDataGetHeader(512, -1);
            mode = -1;
        }
    } else {
        mode = -1;
    }
    gameWork.camera.mode = mode;
    UnduParamInit(&CamLookAt);
    CamLookAt.recordSet = (MapRecordSet *) 0x70000170;
    CamLookAt.block = probeBlock;
    CamLookAt.previousRecord = 0;
    CamLookAt.record = 0;
    CamLookAt.heightTolerance = 1000.0f;
    CamLookAt.flags = 0x802;
    UnduParamInit(&cameraPositionProbe);
    cameraPositionProbe.recordSet = (MapRecordSet *) 0x70000170;
    gameWork.camera.flags |= CAMERA_FLAG_MODE_CHANGED;
    cameraPositionProbe.flags = 0x802;
    cameraPositionProbe.block = probeBlock;
    cameraPositionProbe.heightTolerance = 1000.0f;
    cameraPositionProbe.record = 0;
    cameraPositionProbe.previousRecord = 0;
}

void cameraSetIndex(int index)
{
    struct MapFixupEntry *block;

    if (index < 0) {
        index = gameWork.camera.trackIndex;
    }
    for (;;) {
        gameWork.camera.trackIndex = index;
        block = UnduDataGetHeader(1536, index);
        cameraPlayerProbe.block = block;
        if (index < 0) {
            break;
        }
        if (block != 0) {
            break;
        }
        index = -1;
    }
    gameWork.camera.flags |= CAMERA_FLAG_TRACK_PLAYER;
}


/* Stores the blend and ease modes when they are >= 0, zeroes the slot's progress, then the frames as a float and the fraction (0, or 1.0 when the frames are 0). */
int cameraSlotSetup(struct CameraSlot *slot, int blend_mode, int ease_mode, int frames)
{
    if (blend_mode >= 0) {
        slot->blendMode = blend_mode;
    }
    if (ease_mode >= 0) {
        slot->easeMode = ease_mode;
    }
    slot->progress = 0.0f;
    slot->duration = I2F(frames);
    if (frames == 0) {
        slot->fraction = 1.0f;
    } else {
        slot->fraction = 0.0f;
    }
}

/* Stores the blend and ease modes when they are >= 0, zeroes the slot's progress, then the frames as a float and the fraction (0, or 1.0 when the frames are 0). */ int cameraSlotSetup(struct CameraSlot *slot, int blend_mode, int ease_mode, int frames);

#define CAMERA_SLOT_MASK(slot) (1 << (slot))

/* Applies the slots in slot_mask (zero: every slot whose duration flag is set) and copies each slot's staged
   vectors to its applied fields; sets flag 0x10 when frames is nonzero. */
void cameraApplySlots(int slot_mask, int blend_mode, int ease_mode, int frames)
{
    if (slot_mask == 0) {
        if (gameWork.camera.slot[0].duration != 0.0f) {
            slot_mask = CAMERA_SLOT_MASK(0);
        }
        if (gameWork.camera.slot[1].duration != 0.0f) {
            slot_mask |= CAMERA_SLOT_MASK(1);
        }
        if (gameWork.camera.slot[2].duration != 0.0f) {
            slot_mask |= CAMERA_SLOT_MASK(2);
        }
        if (gameWork.camera.slot[3].duration != 0.0f) {
            slot_mask |= CAMERA_SLOT_MASK(3);
        }
        if (slot_mask == 0) {
            return;
        }
    }
    if (slot_mask & CAMERA_SLOT_MASK(0)) {
        cameraSlotSetup(&gameWork.camera.slot[0], blend_mode, ease_mode, frames);
        gameWork.camera.from.lookAt.x = gameWork.camera.range.lookAt.x;
        gameWork.camera.from.lookAt.y = gameWork.camera.range.lookAt.y;
        gameWork.camera.from.lookAt.z = gameWork.camera.range.lookAt.z;
    }
    if (slot_mask & CAMERA_SLOT_MASK(1)) {
        cameraSlotSetup(&gameWork.camera.slot[1], blend_mode, ease_mode, frames);
        gameWork.camera.from.position.x = gameWork.camera.range.lockPosition.x;
        gameWork.camera.from.position.y = gameWork.camera.range.lockPosition.y;
        gameWork.camera.from.position.z = gameWork.camera.range.lockPosition.z;
        gameWork.camera.from.angles.x = gameWork.camera.range.angles.x;
        gameWork.camera.from.angles.y = gameWork.camera.range.angles.y;
        gameWork.camera.from.angles.w = gameWork.camera.range.angles.w;
        gameWork.camera.fromOffset.x = gameWork.camera.range.offset.x;
        gameWork.camera.fromOffset.y = gameWork.camera.range.offset.y;
        gameWork.camera.fromOffset.z = gameWork.camera.range.offset.z;
    }
    if (slot_mask & CAMERA_SLOT_MASK(2)) {
        cameraSlotSetup(&gameWork.camera.slot[2], blend_mode, ease_mode, frames);
        gameWork.camera.from.position.w = gameWork.camera.range.lockPosition.w;
    }
    if (slot_mask & CAMERA_SLOT_MASK(3)) {
        cameraSlotSetup(&gameWork.camera.slot[3], blend_mode, ease_mode, frames);
        gameWork.camera.from.lookAt.w = gameWork.camera.range.lookAt.w;
    }
    if (frames != 0) {
        gameWork.camera.flags |= CAMERA_FLAG_SLOTS_APPLIED;
    }
}

/* index below 4096 selects uwamonoRecords[index].actor; a larger value is the anchor pointer itself. */ struct XenoActor *cameraAnchorGet(Vector4 *out, int index, int subIndex);

/* cameraUpdateFrame reads the actor pointer of the record body with alias set 0, which fixes the order of its stores. */
typedef union CameraAnchorHead {
    struct XenoActor *actor;
    void *body;
} CameraAnchorHead;

struct XenoActor *cameraAnchorGet(Vector4 *out, int index, int subIndex)
{
    struct XenoActor *anchor;

    if (index < 4096) {
        if (index < 0) {
            out->x = 0.0f;
            out->y = 0.0f;
            out->z = 0.0f;
            return 0;
        }
        anchor = uwamonoRecords[index].body.actor;
    } else {
        anchor = (struct XenoActor *)index;
    }
    out->x = anchor->position.x;
    out->y = anchor->position.y;
    out->z = anchor->position.z;
    if (subIndex >= 0) {
        XenoActorMulVector(anchor, subIndex, out, 1);
    }
    return anchor;
}


/* Moves position by offset. With a valid anchor and a unit offset.w, the offset is turned by the anchor's
   yaw and pitch first; otherwise it is added as it is. Returns the anchor. */
struct XenoActor *cameraApplyAnchorOffset(Vector4 *position, int anchorIndex, int anchorSubIndex, Vector4 *offset)
{
    Matrix4 matrix;
    struct XenoActor *node;

    node = cameraAnchorGet(position, anchorIndex, anchorSubIndex);
    if (node != 0 && offset != 0 && offset->w == 1.0f) {
        xglMatrixStackUnit();
        xglMatrixStackRotY(node->rotation.y);
        xglMatrixStackRotX(node->rotation.x);
        xglMatrixStackTrans(offset);
        xglMatrixStackSave(matrix);
        position->x += matrix[3][0];
        position->y += matrix[3][1];
        position->z += matrix[3][2];
    } else if (offset != 0) {
        position->x += offset->x;
        position->y += offset->y;
        position->z += offset->z;
    }
    return node;
}

/* Applies all four slots with the (0xF, 0, 3) index pair, then flags the state. */
void cameraApplyAllSlots(int unusedArg)
{
    cameraApplySlots(0xF, 0, 3, 0xF);
    gameWork.camera.allSlotsFlagB = 1;
    gameWork.camera.flags |= CAMERA_FLAG_TRACK_PLAYER;
    gameWork.camera.allSlotsFlagA = 1;
}

/* A key or sub-key of -1 matches nothing: both become -1 before the scan of 32 entries. */
/* A key or sub-key of -1 matches nothing: both become -1 before the scan of 32 entries. */
struct CameraKeyEntry *cameraSlotFind(int key, int subKey)
{
    struct CameraKeyEntry *entry;
    int index;

    entry = (struct CameraKeyEntry *)&cameraSlotTable;
    if (key == -1 ? subKey != -1 : subKey == -1) {
        key = -1;
        subKey = -1;
    }
    for (index = 0; index < 32; index++) {
        if (entry->key == key) {
            if (entry->sub == subKey) {
                return entry;
            }
        }
        entry++;
    }
    return 0;
}


/* The scratchpad vectors GetCameraPos works in: the matrix stack saves its result to +0x50 and the point is read from its row 3 (+0x80). */
typedef struct CameraOrbitScratch {
    unsigned char unmodeled_00[0x50];
    Matrix4 matrix;                     /* +0x50: the matrix stack's saved result; its row 3 is the point */
    Vector4 shift;                      /* +0x90: the orbit distance on z */
    unsigned char unmodeled_a0[0x30];
    Vector4 origin;                     /* +0xD0 */
} CameraOrbitScratch;

void GetCameraPos(Vector4 *position, Vector4 *offset, Vector4 *angles)
{
    CameraOrbitScratch *scratch;
    float one;
    float angleZ;

    one = 1.0f;
    scratch = (CameraOrbitScratch *)0x70000000;
    *(int *)&scratch->shift.x = 0;
    *(int *)&scratch->shift.y = 0;
    do {
        angleZ = angles->w;
    } while (0);
    scratch->shift.w = one;
    scratch->shift.z = angleZ;
    scratch->origin.x = offset->x;
    scratch->origin.y = offset->y;
    scratch->origin.z = offset->z;
    scratch->origin.w = one;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&scratch->origin);
    xglMatrixStackRotY(angles->y);
    xglMatrixStackRotX(angles->x);
    xglMatrixStackTrans(&scratch->shift);
    xglMatrixStackSave(scratch->matrix);
    position->x = scratch->matrix[3][0];
    position->y = scratch->matrix[3][1];
    position->z = scratch->matrix[3][2];
}


/* Writes the angles from `to` to `from`: x the pitch, y the yaw (plus pi), w the length of the difference. */
void cameraAnglesBetween(Vector4 *angles, const Vector4 *from, const Vector4 *to)
{
    float dx = from->x - to->x;
    float dy = from->y - to->y;
    float dz = from->z - to->z;

    angles->y = xglAtan2(dx, dz) + CAMERA_PI;
    dx = dx * dx;
    dz = dz * dz;
    angles->x = xglAtan2(dy, __builtin_sqrtf(dx + dz));
    angles->w = __builtin_sqrtf(dx + dy * dy + dz);
}

#include "quadword.h"
#include "vu0_macro.h"
#include "main/vibration.h"
#include "main/uwamono_record.h"
#include "main/uwamono_objx.h"
#include "main/init_uwamono_sys.h"
#include "main/script_register_natives.h"
#include "main/ytachi_test.h"

void cameraRangeSetDefaults(struct CameraRange *range);
extern float cameraClearanceUpperLimit;
extern float cameraClearanceLowerLimit;

/* Scratchpad block the move probe reads and writes: the probe's start point and its moving sample. */
typedef struct MoveProbeScratch {
    unsigned char unmodeled_000[0x150];
    Vector4 from;                       /* +0x150: the probe start (position plus delta) */
    Vector4 sample;                     /* +0x160: the moving sample passed to UnduCheck */
} MoveProbeScratch;

float cameraProbeSlideEpsilon = 0.0099999997764825821f;

/* Takes the probe height unless UnduCheck reported no hit. */
static inline void setProbeHeight(Vector4 *position, float hit)
{
    if (hit != -1000.0f) {
        position->y = hit;
    }
}

float cameraProbeSlide(Vector4 *position, const Vector4 *delta, void *result);

/* Moves position by delta with UnduCheck; when the probe stopped short horizontally it keeps probing with halved steps until
   the step is below 0.01 in x and z. Returns the last probe height (-1000 for no floor). */
float cameraProbeSlide(Vector4 *position, const Vector4 *delta, void *result)
{
    MoveProbeScratch *scratch;
    float hit;
    float dx;

    ((MoveProbeScratch *)0x70000000)->from.x = position->x + delta->x;
    ((MoveProbeScratch *)0x70000000)->from.y = position->y + delta->y;
    ((MoveProbeScratch *)0x70000000)->from.z = position->z + delta->z;
    hit = UnduCheck(position, delta, result);
    scratch = (MoveProbeScratch *)0x70000000;
    if (hit != -1000.0f) {
        position->y = hit;
    }
    dx = scratch->from.x - position->x;
    if (cameraProbeSlideEpsilon < __builtin_fabsf(dx) || cameraProbeSlideEpsilon < __builtin_fabsf(scratch->from.z - position->z)) {
        scratch->sample.x = dx * 0.5f;
        scratch->sample.y = (scratch->from.y - position->y) * 0.5f;
        scratch->sample.z = (scratch->from.z - position->z) * 0.5f;
        for (;;) {
            scratch->from.x = position->x;
            scratch->from.z = position->z;
            hit = UnduCheck(position, &scratch->sample, result);
            setProbeHeight(position, hit);
            if (!(__builtin_fabsf(position->x - scratch->from.x) < __builtin_fabsf(scratch->sample.x))
                && !(__builtin_fabsf(position->z - scratch->from.z) < __builtin_fabsf(scratch->sample.z))) {
                continue;
            }
            scratch->sample.x *= 0.5f;
            scratch->sample.y *= 0.5f;
            scratch->sample.z *= 0.5f;
            if (0.01f > __builtin_fabsf(scratch->sample.x) && 0.01f > __builtin_fabsf(scratch->sample.z)) {
                break;
            }
        }
    }
    return hit;
}

/* Scratchpad block of cameraPullInAlongSight: lookAt is the look-at point, goal the wanted camera position and
   sight the vector from the look-at point to it. */
typedef struct SlopeProbeScratch {
    unsigned char unmodeled_000[0x30];
    Vector4 lookAt;
    unsigned char unmodeled_040[0x50];
    Vector4 probePosition;
    Vector4 probeDelta;
    Vector4 sight;
    Vector4 goal;
} SlopeProbeScratch;

static inline float slopeAlong(float base, float slope, float t)
{
    return base + slope * t;
}

float cameraClearanceUpperLimit = 0.05000000074505806f;
float cameraClearanceLowerLimit = -0.05000000074505806f;

/* Pulls the camera in along the line of sight: probes from the last camera position toward the wanted one, and while the floor
   height at the end is outside the allowed clearance moves the goal along the look-at line down to that height (up to 16 times). */
void cameraPullInAlongSight(void)
{
    SlopeProbeScratch *scratch = (SlopeProbeScratch *)0x70000000;
    MapRecord *probeCleared;
    float lowerLimit;
    float upperLimit;
    float noHit;
    float clearance;
    float along;
    int i;

    Vu0SubVectorXYZ(&scratch->sight, &scratch->goal, &scratch->lookAt);
    probeCleared = cameraPositionProbe.record;
    if (probeCleared == 0) {
        QuadwordCopy(&scratch->probePosition, &cameraProbePosition);
        mapBindAndSnapObject(&scratch->probePosition, &cameraPositionProbe);
        UnduCheck(&scratch->probePosition, 0, &cameraPositionProbe);
        probeCleared = cameraPositionProbe.record;
    }
    noHit = -1000.0f;
    upperLimit = cameraClearanceUpperLimit;
    lowerLimit = cameraClearanceLowerLimit;
    for (i = 0; i < 16; i++) {
        float floorHeight;
        float goalX;
        float goalY;
        float goalZ;
        float riseToFloor;
        QuadwordCopy(&scratch->probePosition, &cameraProbePosition);
        Vu0SubVectorXYZ(&scratch->probeDelta, &scratch->goal, &cameraProbePosition);
        cameraPositionProbe.record = probeCleared;
        floorHeight = cameraProbeSlide(&scratch->probePosition, &scratch->probeDelta, &cameraPositionProbe);
        if (floorHeight == noHit) {
            if (probeCleared == 0) {
                QuadwordCopy(&cameraProbePosition, &scratch->goal);
            }
            break;
        }
        clearance = floorHeight - scratch->goal.y;
        if (clearance < upperLimit && lowerLimit < clearance) {
            QuadwordCopy(&cameraProbePosition, &scratch->probePosition);
            break;
        }
        riseToFloor = floorHeight - scratch->lookAt.y;
        along = riseToFloor / scratch->sight.y;
        goalX = slopeAlong(scratch->lookAt.x, scratch->sight.x, along);
        goalY = slopeAlong(scratch->lookAt.y, scratch->sight.y, along);
        goalZ = slopeAlong(scratch->lookAt.z, scratch->sight.z, along);
        scratch->goal.x = goalX;
        scratch->goal.y = goalY;
        scratch->goal.z = goalZ;
    }
    QuadwordCopy(&scratch->goal, &cameraProbePosition);
}


static inline void copyVector(AlignedVector4 *destination, AlignedVector4 *source)
{
    QuadwordCopy(destination, source);
}

/* The scratchpad block cameraSearchOrbitPitch works in (0x70000000). */
typedef struct CameraSearchScratch {
    struct CameraRecord *record;        /* +0x00: the active camera record */
    unsigned char unmodeled_04[0xC];
    struct XenoActor *actor;            /* +0x10: the actor the camera follows */
    unsigned char unmodeled_14[0x1C];
    AlignedVector4 lookAt;              /* +0x30 */
    unsigned char unmodeled_40[0x50];
    AlignedVector4 delta;               /* +0x90 */
    AlignedVector4 probe;               /* +0xA0 */
    AlignedVector4 orbit;               /* +0xB0: pitch, yaw, -, distance */
    AlignedVector4 position;            /* +0xC0 */
} CameraSearchScratch;

/* Searches the orbit pitch (up to CAMERA_SEARCH_MAX_STEPS steps, halving and reversing the step when the floor height crosses the
   camera height) for a camera position that stands on the floor; the result is left in cameraProbePosition. */
void cameraSearchOrbitPitch(void)
{
    CameraSearchScratch *scratch;
    float floorHeight;
    float pitchStep;
    float pitch;
    float error;
    int flip;
    int iteration;

    scratch = (CameraSearchScratch *)0x70000000;
    scratch->orbit.y = scratch->record->range.angles.y;
    scratch->orbit.w = scratch->record->range.angles.w;
    if (cameraPositionProbe.record == 0) {
        *(int *)&scratch->orbit.x = 0;
        GetCameraPos((AlignedVector4 *)&cameraProbePosition, &scratch->lookAt, &scratch->orbit);
        mapBindAndSnapObject((AlignedVector4 *)&cameraProbePosition, &cameraPositionProbe);
        UnduCheck((AlignedVector4 *)&cameraProbePosition, 0, &cameraPositionProbe);
        scratch->orbit.x = scratch->actor->position.x;
        GetCameraPos(&scratch->position, &scratch->lookAt, &scratch->orbit);
        Vu0SubVectorXYZ(&scratch->probe, &scratch->position, (AlignedVector4 *)&cameraProbePosition);
        cameraProbeSlide((AlignedVector4 *)&cameraProbePosition, &scratch->probe, &cameraPositionProbe);
    }

    pitch = scratch->record->range.angles.x;
    scratch->orbit.x = pitch;
    GetCameraPos(&scratch->position, &scratch->lookAt, &scratch->orbit);
    pitchStep = CAMERA_SEARCH_STEP_FINE;
    QuadwordCopy(&scratch->probe, (AlignedVector4 *)&cameraProbePosition);
    Vu0SubVectorXYZ(&scratch->delta, &scratch->position, (AlignedVector4 *)&cameraProbePosition);
    cameraPositionProbe.flags |= 0x1000;
    if (scratch->record->range.probeMode == CAMERA_PROBE_MODE_SLIDE) {
        floorHeight = cameraProbeSlide(&scratch->probe, &scratch->delta, &cameraPositionProbe);
    } else {
        floorHeight = UnduCheck(&scratch->probe, &scratch->delta, &cameraPositionProbe);
    }

    if (floorHeight != -1000.0f) {
        if (scratch->delta.y < floorHeight) {
            pitchStep = -pitchStep;
        }
    } else {
        pitch = CAMERA_SEARCH_PITCH_DOWN;
        scratch->orbit.x = pitch;
        pitchStep = CAMERA_SEARCH_STEP_COARSE;
        QuadwordCopy(&scratch->probe, &scratch->lookAt);
        QuadwordCopy(&scratch->position, &scratch->probe);
        cameraPositionProbe.flags |= 0x1000;
        floorHeight = UnduCheck(&scratch->probe, 0, &cameraPositionProbe);
        if (floorHeight == -1000.0f) {
            pitch = 0.0f;
            scratch->orbit.x = pitch;
            GetCameraPos(&scratch->probe, &scratch->lookAt, &scratch->orbit);
            pitchStep = -CAMERA_SEARCH_STEP_COARSE;
            QuadwordCopy(&scratch->position, &scratch->probe);
            cameraPositionProbe.flags |= 0x1000;
            floorHeight = UnduCheck(&scratch->probe, 0, &cameraPositionProbe);
        }
    }

    if (floorHeight == -1000.0f) {
        copyVector((AlignedVector4 *)&cameraProbePosition, &scratch->actor->velocity);
    } else {
        flip = 1;
        *(int *)&scratch->delta.x = 0;
        *(int *)&scratch->delta.y = 0;
        *(int *)&scratch->delta.z = 0;
        QuadwordCopy((AlignedVector4 *)&cameraProbePosition, &scratch->probe);
        iteration = 0;
        while (iteration < CAMERA_SEARCH_MAX_STEPS) {
            error = floorHeight - scratch->position.y;
            if (error < CAMERA_SEARCH_TOLERANCE && -CAMERA_SEARCH_TOLERANCE < error) {
                xglPointLength(&scratch->probe, &scratch->lookAt);
                if (scratch->record->range.probeMode == CAMERA_PROBE_MODE_RESLIDE) {
                    scratch->delta.x = (scratch->probe.x + scratch->delta.x) - ((AlignedVector4 *)(AlignedVector4 *)&cameraProbePosition)->x;
                    scratch->delta.y = (floorHeight + scratch->delta.y) - ((AlignedVector4 *)(AlignedVector4 *)&cameraProbePosition)->y;
                    scratch->delta.z = (scratch->probe.z + scratch->delta.z) - ((AlignedVector4 *)(AlignedVector4 *)&cameraProbePosition)->z;
                    cameraPositionProbe.flags |= 0x1000;
                    cameraProbeSlide((AlignedVector4 *)&cameraProbePosition, &scratch->delta, &cameraPositionProbe);
                } else {
                    QuadwordCopy((AlignedVector4 *)&cameraProbePosition, &scratch->probe);
                    ((AlignedVector4 *)(AlignedVector4 *)&cameraProbePosition)->y = floorHeight;
                }
                break;
            }
            pitch = pitch + pitchStep;
            scratch->orbit.x = pitch;
            GetCameraPos(&scratch->position, &scratch->lookAt, &scratch->orbit);
            Vu0SubVectorXYZ(&scratch->delta, &scratch->position, &scratch->probe);
            floorHeight = UnduCheck(&scratch->probe, &scratch->delta, &cameraPositionProbe);
            if (floorHeight == -1000.0f) {
                if (flip == 1) {
                    pitchStep = -pitchStep * 0.5f;
                }
                flip = 0;
            } else {
                if (((floorHeight - scratch->position.y) < 0.0f && pitchStep < 0.0f) || (0.0f < (floorHeight - scratch->position.y) && 0.0f < pitchStep)) {
                    flip = 0;
                }
                if (flip == 0) {
                    pitchStep = -pitchStep * 0.5f;
                }
                flip = 1;
            }
            iteration = iteration + 1;
        }
        if (iteration >= CAMERA_SEARCH_MAX_STEPS) {
            copyVector((AlignedVector4 *)&cameraProbePosition, &scratch->position);
        }
    }
    QuadwordCopy(&scratch->position, (AlignedVector4 *)&cameraProbePosition);
}

struct CameraScratch {
    struct CameraRecord *record;        /* +0x00: the camera record pointer */
    unsigned char unmodeled_04[0x04];
    XglCamera *camera;                  /* +0x08 */
    int anchorIndex;                    /* +0x0C */
    unsigned char unmodeled_10[0x08];
    unsigned char moved;                /* +0x18 */
    unsigned char seen;                 /* +0x19 */
    unsigned char unmodeled_1A[0x02];
    float step;                         /* +0x1C */
    unsigned char unmodeled_20[0x10];
    Vector4 lookAt;                   /* +0x30 */
    unsigned char unmodeled_40[0x50];
    Vector4 angles;                   /* +0x90 */
    unsigned char unmodeled_A0[0x20];
    Vector4 position;                   /* +0xC0 */
};

/* The scratchpad block the camera update functions share. The functions below also reach single words of it as
   *(float *)0x700000xx: each such access loads the 0x7000 base again, as the original does, where a CAMERA_PAD field
   shares one base register. */
#define CAMERA_PAD ((struct CameraScratch *)0x70000000)

extern int cameraPositionProbeGround[];
/* Writes the angles from `to` to `from`: x the pitch, y the yaw (plus pi), w the length of the difference. */ void cameraAnglesBetween(Vector4 *angles, const Vector4 *from, const Vector4 *to);
void cameraPullInAlongSight(void);
void cameraSearchOrbitPitch(void);

/* Camera type follow: the look-at point is the anchor plus the offset, the camera position the orbit around it (angles of
   the record), floor-probed when the position probe has a block, with the record's lock axes overriding the result; stores the
   position, look-at and the angles between them. */
void cameraUpdateFollow(void)
{
    struct CameraScratch *scratch;
    struct CameraRecord *record;
    float lookAtX;
    float lookAtY;
    float lookAtZ;

    scratch = (struct CameraScratch *)0x70000000;
    cameraApplyAnchorOffset(&scratch->lookAt, scratch->anchorIndex, -1, &gameWork.camera.range.offset);
    scratch->lookAt.w = 1.0f;
    lookAtX = scratch->lookAt.x;
    gameWork.camera.range.lookAt.x = lookAtX;
    lookAtY = scratch->lookAt.y;
    gameWork.camera.range.lookAt.y = lookAtY;
    lookAtZ = scratch->lookAt.z;
    gameWork.camera.range.lookAt.z = lookAtZ;
    GetCameraPos(&scratch->position, &scratch->lookAt, &scratch->record->range.angles);
    if (cameraPositionProbeGround[0] != 0) {
        if (scratch->record->range.probeMode == 0) {
            cameraPullInAlongSight();
        } else {
            cameraSearchOrbitPitch();
        }
    }
    record = scratch->record;
    if (record->range.lockX != 0) {
        CAMERA_PAD->position.x = record->range.lockPosition.x;
    }
    if (record->range.lockY != 0) {
        CAMERA_PAD->position.y = record->range.lockPosition.y;
    }
    if (record->range.lockZ != 0) {
        CAMERA_PAD->position.z = record->range.lockPosition.z;
    }
    cameraAnglesBetween(&scratch->angles, &scratch->lookAt, &scratch->position);
    gameWork.camera.range.angles.x = CAMERA_PAD->angles.x;
    gameWork.camera.range.angles.y = *(float *)0x70000094;
    gameWork.camera.range.angles.w = *(float *)0x7000009C;
    /* The Vector4Elements union: the original orders these three stores with alias set 0. */
    ((Vector4Elements *)&cameraProbePosition)->vector.x = *(float *)0x700000C0;
    ((Vector4Elements *)&cameraProbePosition)->vector.y = *(float *)0x700000C4;
    gameWork.camera.range.lockPosition.x = *(float *)0x700000C0;
    gameWork.camera.range.lockPosition.y = *(float *)0x700000C4;
    gameWork.camera.range.lockPosition.z = ((Vector4Elements *)&cameraProbePosition)->vector.z = *(float *)0x700000C8;
    gameWork.camera.range.lockPosition.w = scratch->record->range.lockPosition.w;
    gameWork.camera.range.lookAt.w = scratch->record->range.lookAt.w;
}

/* Copies the record's anchor vectors into gameWork when cameraPlayerProbe.block is set, applies the anchor offset when the
   record has an anchor, and writes the angles from the staged vector to gameWork. */
void cameraUpdateFromRecord(void)
{
    struct CameraRecord *record;
    Vector4 anchorPosition;
    float stageX;
    float stageY;
    float stageZ;

    if (cameraPlayerProbe.block != 0) {
        record = CAMERA_PAD->record;
        gameWork.camera.range.lockPosition.x = record->range.lockPosition.x;
        gameWork.camera.range.lockPosition.y = record->range.lockPosition.y;
        gameWork.camera.range.lockPosition.z = record->range.lockPosition.z;
        gameWork.camera.range.lookAt.x = record->range.lookAt.x;
        gameWork.camera.range.lookAt.y = record->range.lookAt.y;
        gameWork.camera.range.lookAt.z = record->range.lookAt.z;
        gameWork.camera.range.lockPosition.w = record->range.lockPosition.w;
        gameWork.camera.range.lookAt.w = record->range.lookAt.w;
    } else {
        record = CAMERA_PAD->record;
    }
    ((Vector4Elements *)&cameraProbePosition)->vector.x = gameWork.camera.range.lockPosition.x;
    ((Vector4Elements *)&cameraProbePosition)->vector.y = gameWork.camera.range.lockPosition.y;
    ((Vector4Elements *)&cameraProbePosition)->vector.z = gameWork.camera.range.lockPosition.z;
    if (record->range.anchorIndex >= 0) {
        cameraApplyAnchorOffset(&anchorPosition, record->range.anchorIndex, record->range.anchorSubIndex, &gameWork.camera.range.offset);
        gameWork.camera.range.lookAt.x = anchorPosition.x;
        gameWork.camera.range.lookAt.y = anchorPosition.y;
        gameWork.camera.range.lookAt.z = anchorPosition.z;
    }
    (CAMERA_PAD->lookAt.x) = gameWork.camera.range.lookAt.x;
    (*(float *)0x70000034) = gameWork.camera.range.lookAt.y;
    (*(float *)0x70000038) = gameWork.camera.range.lookAt.z;
    cameraAnglesBetween(&CAMERA_PAD->angles, &CAMERA_PAD->lookAt, &cameraProbePosition.v);
    gameWork.camera.range.angles.x = *(float *)0x70000090;
    gameWork.camera.range.angles.y = *(float *)0x70000094;
    gameWork.camera.range.angles.w = *(float *)0x7000009C;
}

/* Stages the current camera position and the anchor-adjusted look-at point for cameraApplyView (scratchpad +0x30). */
void cameraStageLookAt(void)
{
    Vector4 anchorPosition;
    int anchorIndex;

    cameraProbePosition.v.x = gameWork.camera.range.lockPosition.x;
    cameraProbePosition.v.y = gameWork.camera.range.lockPosition.y;
    cameraProbePosition.v.z = gameWork.camera.range.lockPosition.z;
    anchorIndex = gameWork.camera.range.anchorIndex;
    if (anchorIndex >= 0) {
        cameraApplyAnchorOffset(&anchorPosition, anchorIndex, gameWork.camera.range.anchorSubIndex, &gameWork.camera.range.offset);
        gameWork.camera.range.lookAt.x = anchorPosition.x;
        gameWork.camera.range.lookAt.y = anchorPosition.y;
        gameWork.camera.range.lookAt.z = anchorPosition.z;
    }
    CAMERA_PAD->lookAt.x = gameWork.camera.range.lookAt.x;
    *(float *)0x70000034 = gameWork.camera.range.lookAt.y;
    *(float *)0x70000038 = gameWork.camera.range.lookAt.z;
}

/* Classifies angle against the arc from limits[0] to limits[1] (both wrapped into [0, 2pi)): -1 inside the arc,
   otherwise the index of the nearer limit (1 before the midpoint of the excluded arc, 0 after it). */
int cameraAngleLimitIndex(float *limits, float angle)
{
    float fromMin;
    float arcSpan;
    float excludedMidpoint;

    fromMin = angle - limits[0];
    arcSpan = limits[1] - limits[0];
    if (CAMERA_TWO_PI < fromMin) {
        do {
            fromMin -= CAMERA_TWO_PI;
        } while (CAMERA_TWO_PI < fromMin);
    }
    if (fromMin < 0.0f) {
        do {
            fromMin += CAMERA_TWO_PI;
        } while (fromMin < 0.0f);
    }
    if (CAMERA_TWO_PI < arcSpan) {
        do {
            arcSpan -= CAMERA_TWO_PI;
        } while (CAMERA_TWO_PI < arcSpan);
    }
    if (arcSpan < 0.0f) {
        do {
            arcSpan += CAMERA_TWO_PI;
        } while (arcSpan < 0.0f);
    }
    if (fromMin < arcSpan) {
        return -1;
    }
    excludedMidpoint = arcSpan * 0.5f + CAMERA_PI;
    if (fromMin < excludedMidpoint) {
        return 1;
    }
    return 0;
}


/* Clamps the view yaw and pitch to the record's angle limits (looking at the nearest limit when outside), then recomputes the
   look-at point from the position, angles and distance; flags in scratchpad +0x18 whether the look-at point moved by more than 0.1. */
void cameraClampAngles(int afterCommit)
{
    Vector4 back;
    Vector4 eye;
    Matrix4 matrix;
    float yaw;
    float pitch;
    float yawDelta;
    float depth;
    float tanYaw;
    float shift;
    int limitIndex;
    int moved;

    if (afterCommit == 0 || gameWork.camera.range.rangeMode != 0) {
        yaw = gameWork.camera.range.angles.y;
        pitch = gameWork.camera.range.angles.x;
        yawDelta = yaw;
        limitIndex = cameraAngleLimitIndex(&(CAMERA_PAD->record)->range.angleLimits.z, yaw);
        if (limitIndex >= 0) {
            yaw = (&(CAMERA_PAD->record)->range.angleLimits.z)[limitIndex];
            depth = gameWork.camera.range.lookAt.z - gameWork.camera.range.lockPosition.z;
            tanYaw = xglSin(yaw) / xglCos(yaw);
            gameWork.camera.range.lookAt.x = gameWork.camera.range.lockPosition.x + depth * tanYaw;
            cameraAnglesBetween(&CAMERA_PAD->angles, &gameWork.camera.range.lookAt, &gameWork.camera.range.lockPosition);
            gameWork.camera.range.angles.x = CAMERA_PAD->angles.x;
            pitch = gameWork.camera.range.angles.x;
            gameWork.camera.range.angles.y = CAMERA_PAD->angles.y;
            gameWork.camera.range.angles.w = CAMERA_PAD->angles.w;
        }
        yawDelta = yaw - yawDelta;
        while (yawDelta < -CAMERA_PI) {
            yawDelta = yawDelta + CAMERA_TWO_PI;
        }
        while (CAMERA_PI < yawDelta) {
            yawDelta = yawDelta - CAMERA_TWO_PI;
        }
        if (yawDelta < -CAMERA_HALF_PI || CAMERA_HALF_PI < yawDelta) {
            pitch = CAMERA_PI - pitch;
        }
        limitIndex = cameraAngleLimitIndex(&(CAMERA_PAD->record)->range.angleLimits.x, pitch);
        if (limitIndex >= 0) {
            pitch = (&(CAMERA_PAD->record)->range.angleLimits.x)[limitIndex];
        }
        gameWork.camera.range.angles.x = pitch;
        gameWork.camera.range.angles.y = yaw;
        eye.x = gameWork.camera.range.lockPosition.x;
        eye.y = gameWork.camera.range.lockPosition.y;
        back.z = -gameWork.camera.range.angles.w;
        eye.z = gameWork.camera.range.lockPosition.z;
        back.x = 0.0f;
        back.y = 0.0f;
        back.w = 1.0f;
        eye.w = 1.0f;
        xglMatrixStackUnit();
        xglMatrixStackTrans(&eye);
        xglMatrixStackRotY(yaw);
        xglMatrixStackRotX(pitch);
        xglMatrixStackTrans(&back);
        xglMatrixStackSave(matrix);
        moved = 0;
        shift = gameWork.camera.range.lookAt.x - matrix[3][0];
        if (shift < -0.1f || 0.1f < shift) {
            moved = 1;
        }
        shift = gameWork.camera.range.lookAt.y - matrix[3][1];
        if (shift < -0.1f || 0.1f < shift) {
            moved = 1;
        }
        shift = gameWork.camera.range.lookAt.z - matrix[3][2];
        if (shift < -0.1f || 0.1f < shift) {
            moved = 1;
        }
        CAMERA_PAD->moved = moved;
        gameWork.camera.range.lookAt.x = matrix[3][0];
        gameWork.camera.range.lookAt.y = matrix[3][1];
        gameWork.camera.range.lookAt.z = matrix[3][2];
    }
}

/* Once the slots are applied (CAMERA_FLAG_SLOTS_APPLIED), copies the current position, look-at, perspective, roll and angles
   to the "to" targets of the slot transition; while allSlotsFlagB is set it also re-bases the anchor offset on the anchor
   joint and root. Returns 1 when it ran, 0 otherwise. */
int cameraCommitSlots(void)
{
    GameWork *state = &gameWork;
    Vector4 jointPosition;
    Vector4 rootPosition;

    if ((state->camera.flags & CAMERA_FLAG_SLOTS_APPLIED) != 0) {
        float anchorX;
        float anchorY;
        float anchorZ;

        state->camera.to.lookAt.x = state->camera.range.lookAt.x;
        state->camera.to.lookAt.y = state->camera.range.lookAt.y;
        state->camera.to.lookAt.z = state->camera.range.lookAt.z;
        state->camera.to.lookAt.w = state->camera.range.lookAt.w;
        state->camera.to.position.x = state->camera.range.lockPosition.x;
        state->camera.to.position.y = state->camera.range.lockPosition.y;
        state->camera.to.position.z = state->camera.range.lockPosition.z;
        state->camera.to.position.w = state->camera.range.lockPosition.w;
        state->camera.to.angles.x = state->camera.range.angles.x;
        state->camera.to.angles.y = state->camera.range.angles.y;
        state->camera.to.angles.w = state->camera.range.angles.w;
        if (state->camera.allSlotsFlagB != 0) {
            cameraApplyAnchorOffset(&jointPosition, state->camera.range.anchorIndex, state->camera.range.anchorSubIndex, 0);
            cameraApplyAnchorOffset(&rootPosition, state->camera.range.anchorIndex, -1, 0);
            anchorX = (state->camera.anchorBase[0] + rootPosition.x) - jointPosition.x;
            anchorY = (state->camera.anchorBase[1] + rootPosition.y) - jointPosition.y;
            anchorZ = (state->camera.anchorBase[2] + rootPosition.z) - jointPosition.z;
            state->camera.fromOffset.x = anchorX;
            state->camera.range.offset.x = anchorX;
            state->camera.fromOffset.y = anchorY;
            state->camera.range.offset.y = anchorY;
            state->camera.fromOffset.z = anchorZ;
            state->camera.range.offset.z = anchorZ;
            if (state->camera.range.rangeMode != 0) {
                if (CAMERA_PAD->seen != 0 && CAMERA_PAD->moved == 0) {
                    if (state->camera.anchorLocked == 0) {
                        state->camera.slot[0].duration = 0.0f;
                    }
                }
            }
            gameWork.camera.allSlotsFlagB = 0;
        }
        return 1;
    }
    return 0;
}

float cameraEase(u8 mode, float time, float duration)
{
    float scale;
    float half;
    float doubled;
    float rate;

    if (duration == 0.0f) {
        return 1.0f;
    }
    scale = 1.0f / duration;
    switch (mode) {
    case 1:
        doubled = scale + scale;
        half = duration * 0.5f;
        if (time <= half) {
            rate = time * doubled;
            rate = rate * rate * rate * rate * 0.5f;
        } else {
            rate = 1.0f - (time - half) * doubled;
            rate = 1.0f - rate * rate * rate * rate * 0.5f;
        }
        break;
    case 2:
        rate = time * scale;
        rate = rate * rate;
        break;
    case 3:
        rate = 1.0f - time * scale;
        rate = 1.0f - rate * rate;
        break;
    case 0:
    default:
        rate = time * scale;
        break;
    }
    if (rate < 0.0f) {
        rate = 0.0f;
    }
    if (rate > 1.0f) {
        rate = 1.0f;
    }
    return rate;
}

/* Linear interpolation from `from` to `to` by `fraction`. */
float cameraLerpFloat(float from, float to, float fraction)
{
    return ((to - from) * fraction) + from;
}

/* Interpolates an angle from `from` to `to` by `fraction` along the shorter way round the circle. */
float cameraLerpAngle(float from, float to, float fraction)
{
    to = to - from;
    if (to > 3.14159265f) {
        do {
            to -= 6.2831853f;
        } while (to > 3.14159265f);
    }
    if (to < -3.14159265f) {
        do {
            to += 6.2831853f;
        } while (to < -3.14159265f);
    }
    return from + to * fraction;
}

/* VU0 linear interpolation of two vectors into destination, by fraction. */
void cameraLerpVector(Vector4 *destination, const Vector4 *from, const Vector4 *to, float fraction)
{
    __asm__ __volatile__(
        "lqc2 vf4, 0(%0)\n\t"
        "lqc2 vf2, 0(%1)\n\t"
        "lqc2 vf1, 0(%2)\n\t"
        "qmtc2.ni %3, vf3\n\t"
        "vsub.xyz vf4, vf2, vf1\n\t"
        "vmulx.xyz vf2, vf4, vf3x\n\t"
        "vadd.xyz vf4, vf2, vf1\n\t"
        "sqc2 vf4, 0(%0)"
        : : "r"(destination), "r"(to), "r"(from), "r"(fraction) : "memory");
}

/* Advances the wave one frame and returns its offset: rising from -value to value over the first half of the
   period, falling back over the second; zero once no cycles remain. */
float cameraWaveStep(struct CameraWave *wave)
{
    float result = 0.0f;
    float doubled;
    int half;
    unsigned int period;
    unsigned int phase;

    if (wave->remaining == 0) {
        return result;
    }
    phase = wave->phase + 1;
    wave->phase = phase;
    period = wave->period;
    if ((unsigned short)phase >= period) {
        wave->phase = 0;
    }
    if (wave->hold != 0) {
        wave->hold = wave->hold - 1;
    } else {
        wave->remaining = wave->remaining - 1;
        wave->value = wave->value - wave->step;
        if (wave->value < 0.0f) {
            wave->value = 0.0f;
        }
    }
    half = wave->period >> 1;
    doubled = wave->value + wave->value;
    if (wave->phase < half) {
        result = (doubled * I2F(wave->phase)) / I2F(half) - wave->value;
    } else {
        result = wave->value - (doubled * I2F(wave->phase - half)) / I2F(half);
    }
    return result;
}

float cameraEase(u8 mode, float time, float duration);
/* Interpolates an angle from `from` to `to` by `fraction` along the shorter way round the circle. */ float cameraLerpAngle(float from, float to, float fraction);
/* The scratchpad word at 0x70000010 (a union, so its access is alias set 0) holds the pointer to the camera travel block. */
typedef union CameraTravelPad {
    CameraTravel *p;
} CameraTravelPad;
extern float cameraSwaySpringScale;
extern float cameraSwayNoiseScale;

/* Eases the four camera slots by the frame delta, copies finished vectors into the applied set and drives the
   three shake waves through the scratchpad block; the camera move runs last. */
float cameraSwaySpringScale = 0.029999999329447746f;
float cameraSwayNoiseScale = 9.9999997473787516e-05f;

extern const float playerInitialRadius;
extern const float playerInitialHeightReach;
extern const float esFormRadius;
extern const float esFrameScale;
extern const float humanModelScale;
extern const float humanFrameScale;

void cameraAdvanceTracks(float *tracks)
{
    float e;
    float r1;
    float r2;
    float r3;
    float r4;
    float t1;
    float t2;
    float p2;
    float p3;
    float k1;
    float k2;
    float lim2;
    float d2;
    CameraTravelPad *scratch;
    float lim3;
    float d3;
    float cy;
    float cz;

    scratch = (CameraTravelPad *)0x70000010;
    if (gameWork.camera.slot[0].progress < gameWork.camera.slot[0].duration) {
        gameWork.camera.slot[0].progress += CAMERA_PAD->step;
        gameWork.camera.slot[0].fraction = cameraEase((unsigned char)gameWork.camera.slot[0].easeMode, gameWork.camera.slot[0].progress, gameWork.camera.slot[0].duration);
        cameraLerpVector(&gameWork.camera.range.lookAt, &gameWork.camera.from.lookAt, &gameWork.camera.to.lookAt, gameWork.camera.slot[0].fraction);
    }

    if (gameWork.camera.slot[1].progress < gameWork.camera.slot[1].duration) {
        gameWork.camera.slot[1].progress += CAMERA_PAD->step;
        e = cameraEase((unsigned char)gameWork.camera.slot[1].easeMode, gameWork.camera.slot[1].progress, gameWork.camera.slot[1].duration);
        gameWork.camera.slot[1].fraction = e;
        if (gameWork.camera.slot[1].blendMode == 0) {
            cameraLerpVector(&gameWork.camera.range.lockPosition, &gameWork.camera.from.position, &gameWork.camera.to.position, e);
            cameraAnglesBetween(&CAMERA_PAD->angles, &gameWork.camera.range.lookAt, &gameWork.camera.range.lockPosition);
            gameWork.camera.range.angles.x = CAMERA_PAD->angles.x;
            gameWork.camera.range.angles.y = CAMERA_PAD->angles.y;
            gameWork.camera.range.angles.w = CAMERA_PAD->angles.w;
        } else {
            gameWork.camera.range.angles.x = cameraLerpAngle(gameWork.camera.from.angles.x, gameWork.camera.to.angles.x, e);
            gameWork.camera.range.angles.y = cameraLerpAngle(gameWork.camera.from.angles.y, gameWork.camera.to.angles.y, e);
            gameWork.camera.range.angles.w = cameraLerpFloat(gameWork.camera.from.angles.w, gameWork.camera.to.angles.w, e);
            GetCameraPos(&gameWork.camera.range.lockPosition, &gameWork.camera.range.lookAt, &gameWork.camera.range.angles);
        }
        cameraLerpVector(&gameWork.camera.range.offset, &gameWork.camera.fromOffset, (Vector4 *)tracks, gameWork.camera.slot[1].fraction);
    }

    p2 = gameWork.camera.slot[2].progress;
    lim2 = gameWork.camera.slot[2].duration;
    if (p2 < lim2) {
        float next2 = p2 + CAMERA_PAD->step;
        gameWork.camera.slot[2].progress = next2;
        d2 = cameraEase((unsigned char)gameWork.camera.slot[2].easeMode, next2, lim2);
        gameWork.camera.slot[2].fraction = d2;
        gameWork.camera.range.lockPosition.w = cameraLerpFloat(gameWork.camera.from.position.w, gameWork.camera.to.position.w, gameWork.camera.slot[2].fraction);
        p2 = next2;
    }

    p3 = gameWork.camera.slot[3].progress;
    lim3 = gameWork.camera.slot[3].duration;
    if (p3 < lim3) {
        float next3 = p3 + CAMERA_PAD->step;
        gameWork.camera.slot[3].progress = next3;
        d3 = cameraEase((unsigned char)gameWork.camera.slot[3].easeMode, next3, lim3);
        gameWork.camera.slot[3].fraction = d3;
        gameWork.camera.range.lookAt.w = cameraLerpAngle(gameWork.camera.from.lookAt.w, gameWork.camera.to.lookAt.w, gameWork.camera.slot[3].fraction);
        p3 = next3;
    }

    if (gameWork.camera.slot[0].progress >= gameWork.camera.slot[0].duration) {
        gameWork.camera.slot[0].fraction = 1.0f;
        gameWork.camera.from.lookAt.x = gameWork.camera.range.lookAt.x;
        gameWork.camera.from.lookAt.y = gameWork.camera.range.lookAt.y;
        gameWork.camera.from.lookAt.z = gameWork.camera.range.lookAt.z;
        gameWork.camera.slot[0].progress = 0.0f;
        gameWork.camera.slot[0].duration = 0.0f;
    }

    if (gameWork.camera.slot[1].progress >= gameWork.camera.slot[1].duration) {
        gameWork.camera.slot[1].fraction = 1.0f;
        gameWork.camera.from.position.x = gameWork.camera.range.lockPosition.x;
        gameWork.camera.from.position.y = gameWork.camera.range.lockPosition.y;
        gameWork.camera.from.position.z = gameWork.camera.range.lockPosition.z;
        gameWork.camera.from.angles.x = gameWork.camera.range.angles.x;
        gameWork.camera.from.angles.y = gameWork.camera.range.angles.y;
        gameWork.camera.from.angles.w = gameWork.camera.range.angles.w;
        ((GameWorkAlias0 *)&gameWork)->flags = ((GameWorkAlias0 *)&gameWork)->flags & 0xffef;
        gameWork.camera.slot[1].progress = 0.0f;
        gameWork.camera.slot[1].duration = 0.0f;
        gameWork.camera.range.offset.x = tracks[0];
        gameWork.camera.range.offset.y = tracks[1];
        gameWork.camera.range.offset.z = tracks[2];
    }

    if (p2 >= lim2) {
        gameWork.camera.slot[2].fraction = 1.0f;
        gameWork.camera.from.position.w = gameWork.camera.range.lockPosition.w;
        gameWork.camera.slot[2].progress = 0.0f;
        gameWork.camera.slot[2].duration = 0.0f;
    }

    if (p3 >= lim3) {
        gameWork.camera.slot[3].fraction = 1.0f;
        gameWork.camera.from.lookAt.w = gameWork.camera.range.lookAt.w;
        gameWork.camera.slot[3].progress = 0.0f;
        gameWork.camera.slot[3].duration = 0.0f;
    }

    *(int *)0x700000C0 = 0;
    *(int *)0x700000C4 = 0;
    *(int *)0x700000C8 = 0;
    if (gameWork.camera.shakeFlags != 0) {
        CAMERA_PAD->position.x = cameraWaveStep(&gameWork.camera.waves[0]);
        *(float *)0x700000C4 = cameraWaveStep(&gameWork.camera.waves[1]);
        *(float *)0x700000C8 = cameraWaveStep(&gameWork.camera.waves[2]);
        if (gameWork.camera.waves[0].remaining == 0) {
            gameWork.camera.shakeFlags &= 0xFE;
        }
        if (gameWork.camera.waves[1].remaining == 0) {
            gameWork.camera.shakeFlags &= 0xFD;
        }
        if (gameWork.camera.waves[2].remaining == 0) {
            gameWork.camera.shakeFlags &= 0xFB;
        }
        if ((gameWork.camera.shakeFlags & 0x8) != 0) {
            r1 = xglFRand();
            r2 = xglFRand();
            k1 = cameraSwaySpringScale;
            k2 = cameraSwayNoiseScale;
            t1 = -(gameWork.camera.sway[0] + gameWork.camera.sway[2]) * ((r1 - 1.0f) * k1) + (r2 - 1.5f) * k2;
            r3 = xglFRand();
            r4 = xglFRand();
            t2 = -(gameWork.camera.sway[1] + gameWork.camera.sway[3]) * ((r3 - 1.0f) * k1) + (r4 - 1.5f) * k2;
            gameWork.camera.sway[2] += t1;
            gameWork.camera.sway[0] += gameWork.camera.sway[2];
            CAMERA_PAD->position.x += gameWork.camera.sway[0];
            gameWork.camera.sway[3] += t2;
            gameWork.camera.sway[1] += gameWork.camera.sway[3];
            CAMERA_PAD->position.y += gameWork.camera.sway[1];
        }
    }

    scratch->p->position.x = gameWork.camera.range.lockPosition.x;
    cy = gameWork.camera.range.lockPosition.y;
    scratch->p->position.y = cy;
    cz = gameWork.camera.range.lockPosition.z;
    scratch->p->position.z = cz;
    {
        CameraTravel *q = scratch->p;
        q->fov = gameWork.camera.range.lockPosition.w;
        q->chase_position.x = gameWork.camera.range.lookAt.x;
    }
    scratch->p->chase_position.y = gameWork.camera.range.lookAt.y;
    scratch->p->chase_position.z = gameWork.camera.range.lookAt.z;
    scratch->p->rotation.z = gameWork.camera.range.lookAt.w + CAMERA_PAD->position.z;
    cameraAnglesBetween(&CAMERA_PAD->angles, &scratch->p->chase_position, &scratch->p->position);
    scratch->p->rotation.y = *(float *)0x70000094 + *(float *)0x700000C0;
    scratch->p->rotation.x = *(float *)0x70000090 + *(float *)0x700000C4;
    QuadwordCopy(&scratch->p->chase_reference, &scratch->p->chase_position);
    xglCameraMove(*(XglCamera **)0x70000008);
}

/* The ground probe block at cameraPlayerProbe: a MapCollisionObject followed by the record index the probe was last mapped to. */
void cameraUpdateFollow(void);
void cameraUpdateFromRecord(void);
int cameraCommitSlots(void);

/* The scratchpad block cameraUpdateFrame works in (0x70000000). */
typedef struct CameraFrameScratch {
    struct CameraRecord *record;        /* +0x00: the active camera record */
    struct CameraSlot *slot;            /* +0x04: the slot settings in use */
    XglCamera *camera;                  /* +0x08 */
    struct XenoActor *anchor;        /* +0x0C: the actor the camera follows */
    void *travel;                       /* +0x10: &camera->travel */
    short phase;                        /* +0x14 */
    unsigned char unmodeled_16[2];
    unsigned char anchorLocked;         /* +0x18 */
    unsigned char anchorMatched;        /* +0x19: the record anchors the current anchor index */
    unsigned char unmodeled_1a[2];
    float height;                       /* +0x1C */
    unsigned char unmodeled_20[0x3C - 0x20];
    float scale;                        /* +0x3C */
    unsigned char unmodeled_40[0xC0 - 0x40];
    Vector4 delta;                      /* +0xC0: the anchor move of this frame */
} CameraFrameScratch;

void cameraUpdateFrame(void)
{
    XglCamera *camera;
    MapCollisionObject *probe;
    struct XenoActor *anchor;
    struct CameraRecord *record;
    struct CameraSlot *slot;
    struct CameraKeyEntry *entry;
    int control;
    int flags;
    int recordIndex;
    int previous;
    unsigned char anchorReady;
    float ret;
    CameraFrameScratch *scratch = (CameraFrameScratch *)0x70000000;

    if (!(gameWork.camera.flags & CAMERA_FLAG_DISABLED)) {
        if (gameWork.camera.flags & CAMERA_FLAG_VIEW_ONLY) {
            cameraApplyView();
            return;
        }
        scratch->phase = CAMERA_INITIAL_WAIT;
        camera = xglStudioGetCamera2(0);
        scratch->camera = camera;
        if (camera->state != CAMERA_STATE_GAME) {
            return;
        }
        control = gameWork.control;
        scratch->travel = &camera->travel;
        scratch->height = camera->unknown_5f4;
        if (control & CONTROL_PAUSED) {
            scratch->height = 0.0f;
        }
        scratch->anchorMatched = 0;
        scratch->scale = 1.0f;
        gameWork.camera.shakeFlags = 0;
        scratch->anchor = ((CameraAnchorHead *) &uwamonoRecords[uwamonoCurrentIndex].body)->actor;
        if (!(control & 0x400)) {
            if (control & CONTROL_TOP_EXIT) {
                cameraStudioReset();
                gameWork.camera.flags |= CAMERA_FLAG_SNAP;
            } else {
                flags = gameWork.camera.flags;
                if (flags & CAMERA_FLAG_SNAP) {
                    gameWork.camera.flags = flags | (CAMERA_FLAG_SNAP | CAMERA_FLAG_MODE_CHANGED | CAMERA_FLAG_TRACK_PLAYER);
                    gameWork.camera.recordIndex = 0;
                    probe = &cameraPlayerProbe;
                    UnduParamInit(probe);
                    probe->recordSet = (MapRecordSet *) 0x70000170;
                    probe->flags = 0x800;
                    cameraSetIndex(-1);
                    gameWork.camera.mode = 0;
                    GameCameraChangeID(-1, 0, 0);
                    flags = gameWork.camera.flags;
                }
                if (flags & CAMERA_FLAG_TRACK_PLAYER) {
                    cameraSetIndex(-1);
                    cameraPlayerPositionOld = (scratch->anchor)->position;
                    gameWork.camera.flags &= 0xFFDF;
                    gameWork.camera.recordIndex = -1;
                    CamLookAt.record = 0;
                    cameraPositionProbe.record = 0;
                    cameraPlayerProbe.record = 0;
                }
                recordIndex = 0;
                {
                    MapCollisionObject *probe = &cameraPlayerProbe;
                    if (probe->block != 0) {
                        anchor = scratch->anchor;
                        Vu0SubVector(&scratch->delta, &anchor->position, &cameraPlayerPositionOld);
                        ret = UnduCheck(&cameraPlayerPositionOld, &scratch->delta, probe);
                        recordIndex = (unsigned char) probe->kind + 1;
                        if (__builtin_fabsf(cameraPlayerPositionOld.y - ret) > 4.0f) {
                            probe->flags |= 0x1000;
                        }
                    }
                }
                record = &cameraCfSlots.record[recordIndex];
                scratch->record = record;
                scratch->slot = &record->slot;
                previous = gameWork.camera.recordIndex;
                if (previous != recordIndex) {
                    gameWork.camera.previousRecordIndex = gameWork.camera.recordIndex;
                    gameWork.camera.recordIndex = recordIndex;
                    entry = cameraSlotFind(previous, (signed char)recordIndex);
                    if (entry != 0) {
                        scratch->slot = &entry->slot;
                        anchorReady = gameWork.camera.range.rangeMode = entry->anchorReady;
                    } else {
                        record = scratch->record;
                        anchorReady = gameWork.camera.range.rangeMode = (unsigned char)record->range.rangeMode;
                    }
                    GameCameraChangeID((signed char)scratch->record->index02, 0, 0);
                    if (gameWork.camera.allSlotsFlagA == 0) {
                        slot = scratch->slot;
                        cameraApplySlots(0xF, slot->blendMode, slot->easeMode, (int)slot->duration);
                        if (gameWork.camera.range.anchorIndex != -1) {
                            record = scratch->record;
                            if (gameWork.camera.range.anchorIndex == record->range.anchorIndex
                                && !(*(long long *)&record->range.lockX & 0xFFFFFF)) {
                                scratch->anchorMatched = 1;
                            }
                        }
                    }
                    record = scratch->record;
                    if (gameWork.camera.cameraType != record->unmodeled_00) {
                        gameWork.camera.previousCameraType = gameWork.camera.cameraType;
                        gameWork.camera.cameraType = record->unmodeled_00;
                        *(XglQuadword *)gameWork.camera.typeState = 0;
                        gameWork.camera.cameraSubType = 0;
                    }
                    gameWork.camera.range = record->range;
                    gameWork.camera.range.rangeMode = anchorReady;
                    gameWork.camera.allSlotsFlagB = 1;
                }
                if (gameWork.camera.flags & CAMERA_FLAG_SNAP) {
                    gameWork.camera.slot[0].duration = 0.0f;
                    gameWork.camera.flags &= 0xFFEF;
                    gameWork.camera.slot[1].duration = 0.0f;
                    gameWork.camera.slot[2].duration = 0.0f;
                    gameWork.camera.slot[3].duration = 0.0f;
                }
                switch (gameWork.camera.cameraType) {
                case CAMERA_TYPE_FOLLOW:
                    cameraUpdateFollow();
                    cameraClampAngles(0);
                    if (cameraCommitSlots() != 0) {
                        cameraUpdateFollow();
                        cameraClampAngles(1);
                    }
                    cameraAdvanceTracks((float *)&scratch->record->range.offset);
                    break;
                case CAMERA_TYPE_RECORD:
                    cameraUpdateFromRecord();
                    cameraClampAngles(0);
                    if (cameraCommitSlots() != 0) {
                        cameraUpdateFromRecord();
                        cameraClampAngles(1);
                    }
                    cameraAdvanceTracks((float *)&scratch->record->range.offset);
                    break;
                }
                cameraApplyAnchorOffset(&gameWork.camera.anchorOffset, gameWork.camera.range.anchorIndex, -1, 0);
                gameWork.camera.anchorBase[0] = gameWork.camera.range.lookAt.x - gameWork.camera.anchorOffset.x;
                gameWork.camera.anchorBase[1] = gameWork.camera.range.lookAt.y - gameWork.camera.anchorOffset.y;
                gameWork.camera.anchorBase[2] = gameWork.camera.range.lookAt.z - gameWork.camera.anchorOffset.z;
                gameWork.camera.anchorLocked = scratch->anchorLocked;
                if (gameWork.camera.range.rangeFlag != 0) {
                    gameWork.camera.range.northAngle = scratch->camera->travel.rotation.y;
                }
                gameWork.camera.allSlotsFlagA = 0;
                gameWork.camera.flags &= 0xFF3F;
            }
        }
    }
}

/* Pending child indices of the robo change (two ints, -1 when none); no symbol recovered yet. */
extern int uwamonoPartIndices[];

/* Key ranges, 68 bytes each: lo at +0x00, hi at +0x04, value at +0x24; no symbol recovered yet. */
typedef struct UwamonoKeyRange {
    int lo;
    int hi;
    unsigned char unmodeled_08[0x20 - 0x08];
    int mapped;                         /* +0x20: the id the block-A search maps a key to */
    int value;
    unsigned char unmodeled_28[0x44 - 0x28];
} UwamonoKeyRange;

/* The key range table, read as words by uwamonoChangePlayerModel (fields in enum UwamonoKeyField). */
extern int uwamonoKeyRangeTable[10 * 17];

void cameraStageLookAt(void);
/* Eases the four camera slots by the frame delta, copies finished vectors into the applied set and drives the three shake waves through the scratchpad block; the camera move runs last. */ void cameraAdvanceTracks(float *tracks);

/* Publishes the studio camera to the scratchpad block, then runs the view update only while the camera is in
   CAMERA_STATE_GAME; the scratchpad camera pointer is read back after the calls because they may have rewritten it. */
void cameraApplyView(void)
{
    XglCamera *camera;
    CameraFrameScratch *scratch = (CameraFrameScratch *)0x70000000;

    scratch->record = 0;
    scratch->phase = CAMERA_INITIAL_WAIT;
    camera = xglStudioGetCamera2(0);
    scratch->camera = camera;
    if (camera->state != CAMERA_STATE_GAME) {
        return;
    }
    scratch->travel = &camera->travel;
    scratch->height = camera->unknown_5f4;
    scratch->scale = 1.0f;
    if ((gameWork.control & CONTROL_PAUSED) != 0) {
        *(int *)&scratch->height = 0;
    }
    cameraStageLookAt();
    cameraAdvanceTracks(&gameWork.camera.range.offset.x);
    xglCameraMove(scratch->camera);
}

/* The attachment table uwamonoAttachIdTable: eight entries of four words {lo, hi, id0, id1}; the entry applies to the model ids lo..hi and names the
   two part models that go with it, ids of -1 are unused. */
extern unsigned char playerSlotSkipHeaderFlag;
extern unsigned int uwamonoAttachIdTable[UWAMONO_ATTACH_TABLE_WORDS];

/* Loads model modelId into path slot `slot`, then the part models the attachment table lists for that model id. */
void resPlayerSlotLoadWithParts(int slot, unsigned int modelId)
{
    unsigned int headerFlag = 0;
    int entry;
    int part;

    if (playerSlotSkipHeaderFlag == 0) {
        headerFlag = 0x80000000;
    }
    playerSlotSkipHeaderFlag = 0;
    resPlayerSlotUpdate(slot);
    resPlayerSlotLoadID(slot, headerFlag | modelId, 0);
    for (entry = 0; entry < UWAMONO_ATTACH_TABLE_WORDS; entry += UWAMONO_ATTACH_ENTRY_WORDS) {
        if (modelId >= uwamonoAttachIdTable[entry] && modelId <= uwamonoAttachIdTable[entry + 1]) {
            for (part = 0; part < UWAMONO_ATTACH_PARTS; part++) {
                if (uwamonoAttachIdTable[entry + 2 + part] != -1) {
                    resPlayerSlotLoadID(slot, headerFlag | uwamonoAttachIdTable[entry + 2 + part], 0);
                }
            }
            return;
        }
    }
}


/* Loads three model ids into the three path slots: an id that resPlayerSlotFindByOwner finds already resident keeps its slot; then
   every free slot takes the first pending id, which resPlayerSlotLoadWithParts loads with its parts. */
void resPlayerSlotsLoadModels(int modelId0, int modelId1, int modelId2)
{
    unsigned int pending[3];
    unsigned int routed[3];
    int residentSlot;
    int source;
    int slot;
    int candidate;

    pending[0] = modelId0;
    pending[1] = modelId1;
    pending[2] = modelId2;
    routed[0] = 0;
    routed[1] = 0;
    routed[2] = 0;
    for (source = 0; source < 3; source++) {
        residentSlot = resPlayerSlotFindByOwner(pending[source]);
        if (residentSlot >= 0) {
            routed[residentSlot] = pending[source];
            pending[source] = 0;
        }
    }
    for (slot = 0; slot < 3; slot++) {
        if (routed[slot] == 0) {
            for (candidate = 0; candidate < 3; candidate++) {
                if (pending[candidate] != 0) {
                    resPlayerSlotLoadWithParts(slot, pending[candidate]);
                    routed[slot] = pending[candidate];
                    pending[candidate] = 0;
                    break;
                }
            }
        }
    }
}

/* Attaches the player's part records (uwamonoPartIndices) to the player actor: in the ES form to joints 80 and 81 (the first shown, the
   second hidden), otherwise released and re-attached to joints 10 and 7; then applies the key range value to the actor. */
void uwamonoAttachPlayerParts(void)
{
    UwamonoRecord *record = &uwamonoRecords[uwamonoCurrentIndex];
    float form = record->form;
    UwamonoKeyRange *keyRange;
    int wordsLeft;

    if (form == UWAMONO_FORM_ES) {
        if (uwamonoPartIndices[0] != -1) {
            uwamonoRecords[uwamonoPartIndices[0]].body.actor->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
            uwamonoRecords[uwamonoPartIndices[0]].body.actor->drawMode = 1;
            XenoActorAttachChildToJoint(record->body.actor, uwamonoRecords[uwamonoPartIndices[0]].body.actor, UWAMONO_PART_JOINT_ES_0);
        }
        if (uwamonoPartIndices[1] != -1) {
            uwamonoRecords[uwamonoPartIndices[1]].body.actor->drawFlags |= UWAMONO_DRAW_HIDDEN;
            uwamonoRecords[uwamonoPartIndices[1]].body.actor->drawMode = 1;
            XenoActorAttachChildToJoint(record->body.actor, uwamonoRecords[uwamonoPartIndices[1]].body.actor, UWAMONO_PART_JOINT_ES_1);
        }
    } else {
        if (uwamonoPartIndices[0] != -1) {
            uwamonoRecords[uwamonoPartIndices[0]].body.actor->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
            XenoActorReleaseChild(record->body.actor, uwamonoRecords[uwamonoPartIndices[0]].body.actor);
            XenoActorAttachChildToJoint(record->body.actor, uwamonoRecords[uwamonoPartIndices[0]].body.actor, UWAMONO_PART_JOINT_WALK_0);
        }
        if (uwamonoPartIndices[1] != -1) {
            uwamonoRecords[uwamonoPartIndices[1]].body.actor->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
            XenoActorReleaseChild(record->body.actor, uwamonoRecords[uwamonoPartIndices[1]].body.actor);
            XenoActorAttachChildToJoint(record->body.actor, uwamonoRecords[uwamonoPartIndices[1]].body.actor, UWAMONO_PART_JOINT_WALK_1);
        }
    }
    for (wordsLeft = 153, keyRange = (UwamonoKeyRange *)uwamonoKeyRangeTable; wordsLeft >= 0; wordsLeft -= 17, keyRange++) {
        if (keyRange->lo <= record->key && record->key <= keyRange->hi) {
            XenoActorSetInitCode(record->body.actor, keyRange->value);
        }
    }
}

/* Key ranges: 10 records of 17 words each; no symbol recovered yet. */
enum UwamonoKeyField {
    KEY_LO = 0,
    KEY_HI = 1,
    KEY_COLOR_A = 2,    /* 3 words: copied to uwamonoKeyColorA */
    KEY_COLOR_B = 5,    /* 3 words: copied to uwamonoKeyColorB */
    KEY_MAPPED = 8,     /* the id the block-A search maps a key to */
    KEY_VALUE = 9,
    KEY_LEVEL_B = 10,   /* 3 words: copied to uwamonoSearchEventTable */
    KEY_LEVEL_A = 13,   /* 3 words: copied to uwamonoSearchWaitTable */
    KEY_EFFECT = 16,
    KEY_RANGE_WORDS = 17
};
extern unsigned int uwamonoAttachIdTable[32];
extern int uwamonoPartIndices[];
extern short uwamonoKeyEffectId;
extern short uwamonoSearchWaitTable[3];
extern short uwamonoSearchEventTable[3];
extern unsigned char uwamonoKeyColorA[3];
extern unsigned char uwamonoKeyColorB[3];
int sceCdSync(int mode);

/* Changes the player model (key = request without the high bit; UWAMONO_KEY_FROM_CURRENT takes the key mapped from the current record): loads
   the model, creates or updates the player record (kind 4), re-creates its part records, copies the colors and levels of the key range,
   starts its effect and sets the ES or walking scale values. */
void uwamonoChangePlayerModel(int request)
{
    int key = request & UWAMONO_KEY_MASK;
    int partCount = 0;
    int slot;
    int currentIndex;
    int playerIndex;
    int i;
    int n;
    int j;
    int m;
    int q;
    int recordIndex;
    void *modelParams;
    unsigned short effectId;
    UwamonoRecord *record;

    while (xglCdServiceIfPending() != 0) {
    }
    if (key == UWAMONO_KEY_FROM_CURRENT) {
        int recKey = uwamonoRecords[uwamonoCurrentIndex].key;

        for (i = 0; i < 10 * KEY_RANGE_WORDS; i += KEY_RANGE_WORDS) {
            if (recKey < uwamonoKeyRangeTable[i + KEY_LO] || uwamonoKeyRangeTable[i + KEY_HI] < recKey) {
                continue;
            }
            key = uwamonoKeyRangeTable[i + KEY_MAPPED];
            break;
        }
    }
    uwamonoCurrentIndex = -1;
    uwamonoPartIndices[0] = -1;
    uwamonoPartIndices[1] = -1;
    for (recordIndex = 0; recordIndex < UWAMONO_RECORD_COUNT; recordIndex++) {
        if (uwamonoRecords[recordIndex].kind == UWAMONO_KIND_PLAYER) {
            uwamonoCurrentIndex = recordIndex;
        }
        if (uwamonoRecords[recordIndex].kind == UWAMONO_KIND_PART && partCount < UWAMONO_ATTACH_PARTS) {
            uwamonoPartIndices[partCount] = recordIndex;
            partCount++;
        }
    }
    slot = resPlayerSlotFindByOwner(key);
    if (slot < 0) {
        resPlayerSlotUpdate(0);
        slot = 0;
    }
    currentIndex = uwamonoCurrentIndex;
    if (currentIndex == -1) {
        resPlayerSlotLoadID(slot, key, 0);
        playerIndex = uwamonoCreateRecord(0, UWAMONO_KIND_PLAYER, key, -1, -1);
        uwamonoCurrentIndex = playerIndex;
        record = &uwamonoRecords[playerIndex];
    } else {
        record = &uwamonoRecords[currentIndex];
        record->flags |= 0x2;
        {
            struct XenoActor *actor = record->body.actor;

            if (request == actor->kind) {
                return;
            }
            resPlayerSlotLoadID(slot, key, &modelParams);
            {
                int *values = modelParams;

                actor->rsrc[XAct_Rsrc_MDL] = values[0];
                actor->rsrc[XAct_Rsrc_TEX] = values[1];
                actor->rsrc[XAct_Rsrc_XHR] = values[2];
                actor->rsrc[XAct_Rsrc_XAP] = values[4];
                actor->rsrc[XAct_Rsrc_EntryTableC] = values[3];
            }
            XenoActorAllocMatrices(actor);
            XenoActorSetChannelValue(actor, 4.0f);
            XenoActorSetMode(actor, 0);
            record->body.actor = actor;
            actor->kind = key;
            record->key = key;
        }
    }

    uwamonoLadderState.state = 0;
    record->flags |= 0x2;
    record->body.actor->collision.radius = playerInitialRadius;
    record->motion = 1;
    record->body.actor->radius = playerInitialRadius;
    record->extent.x = playerInitialRadius;
    record->body.actor->collision.probeEnabled = 1;
    if ((unsigned int)(key - UWAMONO_ES_KEY_FIRST) < UWAMONO_ES_KEY_COUNT) {
        record->form = UWAMONO_FORM_ES;
    } else {
        record->form = 5.0f;
    }

    gameWork.control &= ~CONTROL_SCRIPTED;
    gameWork.playerParams[0][0] = 6.0f;
    gameWork.playerParams[1][0] = playerInitialHeightReach;
    gameWork.playerParams[4][0] = 1.0f;
    gameWork.playerParams[3][0] = 1.0f;
    gameWork.playerParams[2][0] = 1.0f;
    gameWork.climbFrames = 15;

    for (i = 0; i < UWAMONO_ATTACH_PARTS; i++) {
        if (uwamonoPartIndices[i] != -1) {
            uwamonoReleaseRecord(uwamonoPartIndices[i]);
            uwamonoPartIndices[i] = -1;
        }
    }

    for (i = 0; i < UWAMONO_ATTACH_TABLE_WORDS; i += UWAMONO_ATTACH_ENTRY_WORDS) {
        if (key < (int)uwamonoAttachIdTable[i] || (int)uwamonoAttachIdTable[i + 1] < key) {
            continue;
        }
        for (j = 0; j < UWAMONO_ATTACH_PARTS; j++) {
            int id = (int)uwamonoAttachIdTable[i + 2 + j];

            if (id != -1) {
                resPlayerSlotLoadID(slot, id, 0);
                uwamonoPartIndices[j] = uwamonoCreateRecord(j + 1, UWAMONO_KIND_PART, id, -1, -1);
            }
        }
        break;
    }

    uwamonoAttachPlayerParts();
    uwamonoKeyColorA[0] = uwamonoKeyColorA[1] = uwamonoKeyColorA[2] = 0xFF;

    for (i = 0; i < 10 * KEY_RANGE_WORDS; i += KEY_RANGE_WORDS) {
        int effectFlags;

        if (uwamonoKeyRangeTable[i + KEY_LO] > key || uwamonoKeyRangeTable[i + KEY_HI] < key) {
            continue;
        }
        uwamonoKeyColorA[0] = uwamonoKeyRangeTable[i + KEY_COLOR_A + 0];
        uwamonoKeyColorA[1] = uwamonoKeyRangeTable[i + KEY_COLOR_A + 1];
        uwamonoKeyColorA[2] = uwamonoKeyRangeTable[i + KEY_COLOR_A + 2];
        uwamonoKeyColorB[0] = uwamonoKeyRangeTable[i + KEY_COLOR_B + 0];
        uwamonoKeyColorB[1] = uwamonoKeyRangeTable[i + KEY_COLOR_B + 1];
        uwamonoKeyColorB[2] = uwamonoKeyRangeTable[i + KEY_COLOR_B + 2];
        uwamonoSearchEventTable[0] = uwamonoKeyRangeTable[i + KEY_LEVEL_B + 0];
        uwamonoSearchEventTable[1] = uwamonoKeyRangeTable[i + KEY_LEVEL_B + 1];
        uwamonoSearchEventTable[2] = uwamonoKeyRangeTable[i + KEY_LEVEL_B + 2];
        uwamonoSearchWaitTable[0] = uwamonoKeyRangeTable[i + KEY_LEVEL_A + 0];
        uwamonoSearchWaitTable[1] = uwamonoKeyRangeTable[i + KEY_LEVEL_A + 1];
        uwamonoSearchWaitTable[2] = uwamonoKeyRangeTable[i + KEY_LEVEL_A + 2];
        XenoActorSetInitCode(record->body.actor, uwamonoKeyRangeTable[i + KEY_VALUE]);
        effectId = uwamonoKeyRangeTable[i + KEY_EFFECT];
        effectFlags = 0x10000001;
        uwamonoKeyEffectId = (short)effectId;
        sefStartEffectKeyDefault(0, (((int)((unsigned int)effectId << 16)) >> 4) | effectFlags);
        sceCdSync(0);
    }

    if (record->form == UWAMONO_FORM_ES) {
        gameWork.frameScale = esFrameScale;
        record->body.actor->collision.radius = esFormRadius;
        record->body.actor->radius = esFormRadius;
        record->extent.x = esFormRadius;
        record->scale.z = 1.0f;
        record->scale.y = 1.0f;
        record->scale.x = 1.0f;
    } else {
        gameWork.frameScale = humanFrameScale;
        record->body.actor->collision.radius = 4.0f;
        record->body.actor->radius = 4.0f;
        record->extent.x = 4.0f;
        record->scale.z = humanModelScale;
        record->scale.y = humanModelScale;
        record->scale.x = humanModelScale;
        gameWork.playerParams[0][0] = 20.0f;
        gameWork.playerParams[1][0] = 1.0f;
    }
    UnlockMapUnit();
    seFootBanksLoad();
}

#define NULL ((void *) 0)

extern int uwamonoSearchEventCount;
extern int uwamonoSearchMode;
extern short uwamonoKeyEffectId;
extern signed char uwamonoMovingFlag;
static float cameraNorthAngle;
static SefEffectRecord *uwamonoSearchEffectHandle;
extern float uwamonoSlopeFactorTable[];
extern unsigned char uwamonoSlopeDirectionTable[];
extern MapCollisionObject uwamonoProbeTemplate;

/* Per jump height step (UwamonoRecord.level): the frames before the landing
 * check (uwamonoSearchWaitCount) and before the target action (uwamonoSearchEventCount). */
short uwamonoSearchWaitTable[3] = { -1, -1, -1 };

short uwamonoSearchEventTable[3] = { -1, -1, -1 };

float atan2f(float y, float x);
float floorf(float value);
/* The first record other than owner (a talk or player record, not blocked) whose radius sphere, scaled by reachScale,
   reaches point (the owner position when null), or -1. */ int uwamonoFindOverlapRecord(int owner, AlignedVector4 *point, float reachScale);
int uwamonoPushOutOfActor(int index, struct XenoActor *actor);
void uwamonoPickTarget(short index);

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define UWAMONO_IS_ES(actor) ((unsigned int) ((actor)->kind - 0x10000) < 0x4000)
/* Stick directions snap to 16 sectors: pi/8 per sector, offset by half a sector. */
#define EIGHTH_TURN 0.39269909f
#define EIGHTH_TURN_HALF 0.19634955f
#define QUARTER_TURN_FLOAT 0.78539819f /* pi/4 as the original float constant */
/* Draw flags of a XenoActor (drawFlags): UWAMONO_DRAW_HIDDEN is defined at the top of the file. */
#define UWAMONO_DRAW_TRANSLUCENT 0x08000000
/* Record keys: the ES range of the party starts at 0x10000; 0x10026 is Jin's ES. */
#define UWAMONO_KEY_JIN_ES 0x10026
#define UWAMONO_KEY_IN(key, first, count) ((unsigned int) ((key) - (first)) < (count))
/* Ground status classes of XenoActor.status. */
#define GROUND_SLOPE 0x10000
#define GROUND_LADDER 0x100
#define GROUND_ELEVATOR 0x200

/* Candidate target points (10 entries; runtime_motion_face.c views the same table as float rows) and their reach. */
extern Vector4 enepcLookAtPointTable[];

/*
 * Picks the target of a map object: the nearest point in reach first, then
 * the script target, then a nearby object that can be targeted.
 */
void uwamonoPickTarget(short index)
{
    UwamonoState *motions = uwamonoRecords[index].body.motions;
    float nearest = 1000.0f;
    short found = -1;
    int scriptTarget;
    int nearby;
    int i;

    for (i = 0; i < 10; i++) {
        float distance = Get_Distance(&enepcLookAtPointTable[i], &uwamonoRecords[index].position);

        if (enepcLookAtReachTable[i] != 0.0f && distance <= enepcLookAtReachTable[i] && distance < nearest) {
            nearest = distance;
            found = i;
        }
    }
    if (found != -1) {
        Actor_LookAt_Set(index, 2, -1, &enepcLookAtPointTable[found]);
        Actor_LookAt(index);
        return;
    }
    Actor_LookAt_Release(index, 2);
    if (motions->targetPriority == 3) {
        Actor_LookAt(index);
        return;
    }
    if (gameWork.control & CONTROL_SCRIPTED) {
        Actor_LookAt_Release(index, 0);
        return;
    }
    scriptTarget = uwamonoFindSelected();
    if (scriptTarget != -1) {
        Actor_LookAt_Set(index, 0, scriptTarget, NULL);
        Actor_LookAt(index);
        return;
    }
    Actor_LookAt_Release(index, 0);
    nearby = Get_MostNear_Actor(index);
    if (nearby == -1) {
        Actor_LookAt_Release(index, 1);
        return;
    }
    if (uwamonoRecords[nearby].flags & 2) {
        Actor_LookAt_Set(index, 1, nearby, NULL);
        Actor_LookAt(index);
        return;
    }
    /* The original tests the result here: the call is a jal + jr, not a tail call. */
    if (Actor_LookAt_Release(index, 1)) {
        return;
    }
}

/*
 * Per-frame control of the player's map object: rider fade, input, walk
 * motions, talk/search targets, jumps, ground status and the move probe, then
 * copies the result to the record and to the riders.
 */
void uwamonoPlayerUpdate(void)
{
    /* record and the indexed uwamonoRecords[uwamonoCurrentIndex] reads below are the same object; the mix
       is kept for the original code generation. */
    UwamonoRecord *record = &uwamonoRecords[uwamonoCurrentIndex];
    struct XenoActor *actor = uwamonoRecords[uwamonoCurrentIndex].body.actor;
    struct XenoActor *riders[2];
    Vector4 unused; /* Kept: the original builds this (0, 0, 0, 1) vector and never reads it. */
    Vector4 elevator;
    AlignedVector4 old;
    float speed;
    float frameScale;
    int i;
    int riding;
    int moving;
    int walk;
    int target;
    int stick_x;
    int stick_y;
    unsigned int status;

    if (uwamonoPartIndices[0] != -1) {
        riders[0] = uwamonoRecords[uwamonoPartIndices[0]].body.actor;
    } else {
        riders[0] = NULL;
    }
    if (uwamonoPartIndices[1] != -1) {
        riders[1] = uwamonoRecords[uwamonoPartIndices[1]].body.actor;
    } else {
        riders[1] = NULL;
    }
    actor->previous.vector.x = actor->position.x;
    actor->previous.vector.y = actor->position.y;
    actor->previous.vector.z = actor->position.z;
    actor->previous.vector.w = actor->position.w;
    actor->moved = 0;
    for (i = 0; i < 2; i++) {
        if (riders[i] != NULL) {
            riders[i]->moved = 0;
            riders[i]->radius = actor->radius;
            riders[i]->previous.vector.x = actor->position.x;
            riders[i]->previous.vector.y = actor->position.y;
            riders[i]->previous.vector.z = actor->position.z;
            riders[i]->previous.vector.w = actor->position.w;
        }
    }
    if (gameWork.control & CONTROL_PAUSED) {
        return;
    }
    if (xglStudioGetCamera2(0)->state != 4) {
        return;
    }
    if (gameWork.fadeTimer != 0) {
        if (--gameWork.fadeTimer <= 0) {
            record->transparency = 0.0f;
            gameWork.fadeTimer = 0;
            actor->alpha = 1.0f;
            actor->drawFlags &= ~UWAMONO_DRAW_TRANSLUCENT;
            if (riders[0] != NULL) {
                riders[0]->alpha = 1.0f;
                riders[0]->drawFlags &= ~UWAMONO_DRAW_TRANSLUCENT;
            }
            if (riders[1] != NULL) {
                riders[1]->alpha = 1.0f;
                riders[1]->drawFlags &= ~UWAMONO_DRAW_TRANSLUCENT;
            }
        } else if (gameWork.fadeTimer % 2 == 0 || gameWork.fadeTimer >= 23) {
            actor->drawFlags |= UWAMONO_DRAW_TRANSLUCENT;
            actor->alpha = (100.0f - record->transparency) / 100.0f;
            if (riders[0] != NULL) {
                riders[0]->drawFlags |= UWAMONO_DRAW_TRANSLUCENT;
                riders[0]->alpha = (100.0f - record->transparency) / 100.0f;
            }
            if (riders[1] != NULL) {
                riders[1]->drawFlags |= UWAMONO_DRAW_TRANSLUCENT;
                riders[1]->alpha = (100.0f - record->transparency) / 100.0f;
            }
        } else {
            actor->alpha = 1.0f;
            actor->drawFlags &= ~UWAMONO_DRAW_TRANSLUCENT;
            if (riders[0] != NULL) {
                riders[0]->alpha = 1.0f;
                riders[0]->drawFlags &= ~UWAMONO_DRAW_TRANSLUCENT;
            }
            if (riders[1] != NULL) {
                riders[1]->alpha = 1.0f;
                riders[1]->drawFlags &= ~UWAMONO_DRAW_TRANSLUCENT;
            }
        }
    }
    if (Ladder_Main() == 0) {
        speed = 0.0f;
        frameScale = gameWork.frameScale;
        if (uwamonoSearchWaitCount >= 0 && record->key == UWAMONO_KEY_JIN_ES) {
            if (actor->mode == 5 && actor->motion[0].frame >= 18.0f) {
                /* The original calls it twice in a row (and ShowPart below the same). */
                XenoActorHidePart(riders[0], "jin_saya_tuka");
                XenoActorHidePart(riders[0], "jin_saya_tuka");
            }
            if (record->key == UWAMONO_KEY_JIN_ES && actor->mode == 5 && actor->motion[0].frame >= 50.0f) {
                XenoActorShowPart(riders[0], "jin_saya_tuka");
                XenoActorShowPart(riders[0], "jin_saya_tuka");
            }
        }
        if (uwamonoSearchEventCount != -1 && --uwamonoSearchEventCount == -1 && uwamonoSearchMode < 6 && uwamonoSearchMode >= 3) {
            target = uwamonoFindSelected();
            if (target != -1) {
                uwamonoSendEvent(1, uwamonoRecords[target].id, 0);
                if (uwamonoRecords[target].flags & 0x40000) {
                    uwamonoTriggerRecord(target);
                    UnlockMapUnit();
                }
            }
        }
        if (uwamonoSearchWaitCount >= 0) {
            uwamonoSearchWaitCount--;
        }
        /* JP: ウェイト %3d
         * EN: Wait %3d */
        xglFontDebugPrintf(8, 0x40, "\xa5\xa6\xa5\xa7\xa5\xa4\xa5\xc8 %3d", uwamonoSearchWaitCount);
        if (uwamonoSearchWaitCount == -1) {
            uwamonoSearchWaitCount = 0;
            if (uwamonoSearchMode < 6) {
                if (uwamonoSearchMode >= 3) {
                    XenoActorSetChannelValueB(actor, 4.0f);
                    XenoActorSetSelector(actor, 0);
                    gameWork.control &= ~CONTROL_SEARCHING;
                }
            }
            uwamonoSearchMode = -1;
            if (UWAMONO_IS_ES(actor)) {
                /* do/while(0) with break: kept for the original branch layout (same in the blocks below). */
                do {
                    if (UWAMONO_KEY_IN(record->key, 0x10025, 7)) {
                        if (riders[0] != NULL) {
                            riders[0]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                        }
                        if (riders[1] != NULL) {
                            riders[1]->drawFlags |= UWAMONO_DRAW_HIDDEN;
                        }
                        break;
                    }
                    if (UWAMONO_KEY_IN(record->key, 0x10015, 6)) {
                        if (riders[0] != NULL) {
                            riders[0]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                        }
                        if (riders[1] != NULL) {
                            riders[1]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                        }
                        break;
                    }
                    if (riders[0] != NULL) {
                        riders[0]->drawFlags |= UWAMONO_DRAW_HIDDEN;
                    }
                    if (riders[1] != NULL) {
                        riders[1]->drawFlags |= UWAMONO_DRAW_HIDDEN;
                    }
                } while (0);
            }
            if (!(gameWork.control & CONTROL_SCRIPTED) || (gameWork.control & CONTROL_REPLAYED)) {
                float x;
                float y;
                float angle;
                float step;
                int pressure;
                int digital;

                if (gameWork.control & CONTROL_REPLAYED) {
                    stick_x = gameWork.replayStickX;
                    stick_y = gameWork.replayStickY;
                    actor->collision.kind = gameWork.replayStatus;
                } else {
                    /* D-pad pressure and the pad's digital direction (scaled to the same range): the one with
                       the larger magnitude wins, opposite signs cancel. */
                    pressure = PadData[0].horizontalPressure;
                    digital = PadData[0].leftX * 2;
                    if (pressure != 0 && digital != 0 && (pressure ^ digital) < 0) {
                        stick_x = 0;
                    } else if (pressure < 0 || digital < 0) {
                        stick_x = MIN(pressure, digital);
                    } else {
                        stick_x = MAX(pressure, digital);
                    }
                    pressure = PadData[0].verticalPressure;
                    digital = PadData[0].leftY * 2;
                    if (pressure != 0 && digital != 0 && (pressure ^ digital) < 0) {
                        stick_y = 0;
                    } else if (pressure < 0 || digital < 0) {
                        stick_y = MIN(pressure, digital);
                    } else {
                        stick_y = MAX(pressure, digital);
                    }
                    gameWork.replayStickX = stick_x;
                    gameWork.replayStickY = stick_y;
                    gameWork.replayStatus = actor->collision.kind;
                }
                if (stick_x == 0 && stick_y == 0) {
                    cameraNorthAngle = gameWork.camera.range.northAngle;
                }
                x = I2F(stick_x);
                y = I2F(stick_y);
                speed = __builtin_sqrtf(x * x + y * y);
                if (speed > 256.0f) {
                    speed = 256.0f;
                }
                angle = Get_Decimal_Surplus_for_Radius(
                    floorf((atan2f(x, y) + EIGHTH_TURN_HALF) / EIGHTH_TURN) * EIGHTH_TURN + cameraNorthAngle);
                if (speed > 0.0f) {
                    actor->direction.y = angle;
                }
                if ((PadData[0].held & PAD_R2) && speed > 191.0f) {
                    speed = 191.0f;
                }
                step = speed * frameScale;
                actor->velocity.x += xglSin(actor->direction.y) * step;
                actor->velocity.z += xglCos(actor->direction.y) * step;
            }
            if (!(gameWork.control & CONTROL_SCRIPTED)) {
                do {
                    if (speed <= 0.0f) {
                        actor->motion[0].speed = 1.0f;
                        walk = 0;
                        break;
                    }
                    if (speed < 192.0f) {
                        actor->motion[0].speed = speed * 2.6f / 192.0f;
                        walk = 1;
                        break;
                    }
                    walk = 2;
                    actor->motion[0].speed = speed * (1.0f / 256.0f);
                } while (0);
                if (actor->mode != walk && (walk != 0 || actor->mode != 3) && (walk != 0 || actor->mode != 4)
                    && (walk != 0 || actor->mode != 5)) {
                    actor->flags &= ~2;
                    if (uwamonoMovingFlag != 0) {
                        XenoActorEnableChannel(actor);
                    }
                    XenoActorCheckMode(actor, uwamonoRecords[uwamonoCurrentIndex].body.motions->walk[walk]);
                    XenoActorSetChannelValue(actor, 4.0f);
                    /* moving and riding are kept as locals: the original goes through them for its code generation. */
                    moving = 1;
                    riding = UWAMONO_IS_ES(actor);
                    uwamonoMovingFlag = moving;
                    if (riding) {
                        for (i = 0; i < 2; i++) {
                            if (riders[i] != NULL) {
                                XenoActorSetChannelValue(riders[i], 4.0f);
                                XenoActorEnableChannel(riders[i]);
                                XenoActorSetMode(riders[i], walk);
                            }
                        }
                    }
                }
                target = uwamonoFindTalkTarget(actor, 50.0f);
                /* JP: トーク対象 → %8d
                 * EN: Talk target -> %8d */
                xglFontDebugPrintf(8, 0x20, "\xa5\xc8\xa1\xbc\xa5\xaf\xc2\xd0\xbe\xdd \xa2\xaa %8d", target);
                if (target != -1) {
                    if (PadData[0].pressed & PAD_CIRCLE) {
                        UwamonoRecord *object = &uwamonoRecords[target];

                        switch (object->kind) {
                        case 2:
                            if (UWAMONO_IS_ES(actor)) {
                                if (uwamonoRecords[target].searchState == 0) {
                                    uwamonoSetPairMode(target, 0);
                                }
                            } else if (uwamonoRecords[target].searchState == 2) {
                                uwamonoSetPairMode(target, 0);
                            }
                            uwamonoStartPairMove(target, 5, 30);
                            break;
                        case 1: {
                            struct XenoActor *other = uwamonoRecords[target].body.actor;

                            if (UWAMONO_KEY_IN(other->kind, 0x10000, 0x8000) && msgWindowInUse() == 0) {
                                talkSetResult(object->id);
                                mapObjectSetActive(record, object);
                                XenoActorEnableChannel(record->body.actor);
                                XenoActorSetMode(record->body.actor, 0);
                            }
                            break;
                        }
                        }
                    } else if (uwamonoRecords[target].flags & 0x800) {
                        if (msgWindowInUse() == 0) {
                            talkSetResult(uwamonoRecords[target].id);
                        }
                    } else {
                        mapObjectSetActive(record, NULL);
                    }
                } else {
                    mapObjectSetActive(record, NULL);
                }
                if (walk == 2) {
                    Enemy_FindByEar(uwamonoCurrentIndex, &uwamonoRecords[uwamonoCurrentIndex].body.actor->position);
                }
                if (walk != 0) {
                    uwamonoMoveSelection(0);
                }
                if (!(gameWork.control & CONTROL_SCRIPTED) && !(gameWork.control & CONTROL_SEARCHING) && uwamonoSearchEventCount == -1) {
                    if (PadData[0].pressed & PAD_L1) {
                        uwamonoMoveSelection(1);
                    }
                    if (PadData[0].pressed & PAD_R1) {
                        uwamonoMoveSelection(-1);
                    }
                    if (PadData[0].pressed & PAD_SQUARE) {
                        int searched;
                        int jumpMode;

                        searched = uwamonoFindSelected();
                        if (searched != -1 && (uwamonoRecords[searched].flags & 0x20000)) {
                            gameWork.control |= CONTROL_SEARCHING;
                            uwamonoSendEvent(0, uwamonoRecords[searched].id, 0);
                            actor->direction.y = Get_Angle(&actor->position, &uwamonoRecords[searched].position);
                            actor->rotation.y = Get_Cursol_by_Reduce_Speed_Angle_Loop(actor->rotation.y, actor->direction.y, 4.0f);
                            /* The smoothed value above is overwritten at once; the call stays for the original code. */
                            actor->rotation.y = Get_Angle(&actor->position, &uwamonoRecords[searched].position);
                            XenoActorSetChannelValue(actor, 0.0f);
                            uwamonoMovingFlag = 0;
                            XenoActorSetMode(actor, uwamonoRecords[searched].level + 3);
                            actor->requested_mode = 0;
                            uwamonoSearchWaitCount = uwamonoSearchWaitTable[uwamonoRecords[searched].level];
                            uwamonoSearchEventCount = uwamonoSearchEventTable[uwamonoRecords[searched].level];
                            if (UWAMONO_IS_ES(actor)) {
                                for (i = 0; i < 2; i++) {
                                    if (riders[i] != NULL) {
                                        XenoActorSetChannelValue(riders[i], 0.0f);
                                        XenoActorEnableChannel(riders[i]);
                                        XenoActorSetMode(riders[i], uwamonoRecords[searched].level + 3);
                                        riders[i]->flags |= 2;
                                    }
                                }
                                do {
                                    if (UWAMONO_KEY_IN(record->key, 0x10025, 7)) {
                                        if (riders[0] != NULL) {
                                            riders[0]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                                        }
                                        if (riders[1] != NULL) {
                                            riders[1]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                                        }
                                        break;
                                    }
                                    if (UWAMONO_KEY_IN(record->key, 0x10015, 6)) {
                                        if (riders[0] != NULL) {
                                            riders[0]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                                        }
                                        if (riders[1] != NULL) {
                                            riders[1]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                                        }
                                        break;
                                    }
                                    if (riders[0] != NULL) {
                                        riders[0]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                                    }
                                    if (riders[1] != NULL) {
                                        riders[1]->drawFlags |= UWAMONO_DRAW_HIDDEN;
                                    }
                                } while (0);
                                seVoiceStartDefault(uwamonoRecords[searched].level * 3 + Get_Rnd(0, 2) + 0x07000001);
                                seVoiceStartDefault(uwamonoRecords[searched].level + 0x0700000B);
                                uwamonoSearchEffectHandle = sefCreateEffectCf((uwamonoKeyEffectId << 12) | (uwamonoRecords[searched].level + 1) | 0x10000000, 0, 0);
                            } else if (uwamonoRecords[searched].level == 0) {
                                seVoiceStartDefault(0x0700000B);
                                uwamonoSearchEffectHandle = sefCreateEffectCf((uwamonoKeyEffectId << 12) | 0x10000001, 0, 0);
                            } else {
                                seVoiceStartDefault(0x0700000C);
                                uwamonoSearchEffectHandle = sefCreateEffectCf((uwamonoKeyEffectId << 12) | 0x10000002, 0, 0);
                            }
                            sefSetRecordOwner(uwamonoSearchEffectHandle, actor);
                            sefSetRecordValues(uwamonoSearchEffectHandle, riders[0], riders[1], 0);
                            /* jumpMode is kept as a local for the original code generation. */
                            jumpMode = uwamonoRecords[searched].level + 3;
                            actor->motion[0].speed = 1.0f;
                            uwamonoSearchMode = jumpMode;
                            XenoActorSetChannelValueB(actor, 4.0f);
                            XenoActorSetSelector(actor, 5);
                            Vibration_Set_Weak(4);
                        }
                    }
                }
            }
        }
        status = actor->collision.kind;
        if ((status & 0xFFFF0000) == GROUND_SLOPE) {
            float angle;
            float distance;
            unsigned int slope = status & 0xF0F;

            if (!(slope & 0xF00)) {
                float factor;

                if (slope & 8) {
                    GameWorkSlope *entry = &gameWork.slopes[slope & 7];
                    float push = frameScale * entry->push * 256.0f;

                    actor->velocity.x += entry->x * push;
                    actor->velocity.z += entry->z * push;
                    factor = entry->speed;
                } else {
                    factor = uwamonoSlopeFactorTable[slope & 3];
                }
                angle = actor->direction.y;
                distance = frameScale * ((factor - 1.0f) * speed);
            } else {
                angle = I2F(uwamonoSlopeDirectionTable[slope >> 8]) * QUARTER_TURN_FLOAT;
                distance = frameScale * uwamonoSlopeFactorTable[slope & 3] * 256.0f;
            }
            actor->velocity.x += xglSin(angle) * distance;
            actor->velocity.z += xglCos(angle) * distance;
        }
        if ((actor->collision.kind & ~0xFF) == GROUND_ELEVATOR) {
            elvGetPosition(actor->collision.kind & 0xF, 0, &elevator);
            actor->velocity.x += elevator.x;
            actor->velocity.z += elevator.z;
        }
        if (!(gameWork.control & CONTROL_SCRIPTED)) {
            actor->rotation.y = Get_Cursol_by_Reduce_Speed_Angle_Loop(actor->rotation.y, actor->direction.y, 4.0f);
        }
        actor->collision.flags = (actor->collision.flags & ~7) | 0x302;
        if (!(uwamonoRecords[uwamonoCurrentIndex].flags & 0x04000000)) {
            float height;

            old = actor->position;
            height = UnduCheck(&actor->position, &actor->velocity, &actor->collision);
            actor->velocity.x = actor->position.x - old.x;
            actor->velocity.z = actor->position.z - old.z;
            if (height != -1000.0f) {
                if ((actor->collision.bits & 0x10) && actor->velocity.y != 0.0f) {
                    actor->velocity.y = -0.011f;
                }
                if (actor->position.y < height
                    || (actor->velocity.y == 0.0f && actor->position.y - actor->collision.heightTolerance < height)) {
                    actor->position.y = height;
                    actor->velocity.y = 0.0f;
                } else {
                    actor->position.y += actor->velocity.y;
                    actor->velocity.y += -0.011f;
                }
            } else {
                actor->velocity.x = 0.0f;
                actor->velocity.y = 0.0f;
                actor->velocity.z = 0.0f;
            }
        }
        HitCheckMapUnitWithNyuru(actor);
        target = uwamonoFindOverlapRecord(uwamonoCurrentIndex, 0, 1.0f);
        if (target != -1) {
            uwamonoPushOutOfActor(uwamonoCurrentIndex, uwamonoRecords[target].body.actor);
        }
        actor->velocity.x = actor->velocity.z = 0.0f;
        if (!(((GameWorkAlias0 *)&gameWork)->control & CONTROL_TOP_EXIT) && (actor->collision.kind & ~0x1F) == GROUND_LADDER) {
            if (!(gameWork.control & 0x420)) {
                gameWork.control |= 0x400;
                uwamonoSendEvent(3, (actor->collision.kind & 0x1F) + 1, 0);
            }
            ((GameWorkAlias0 *)&gameWork)->control |= 0x400;
        } else {
            gameWork.control &= ~0x400;
        }
    } else {
        if (UWAMONO_IS_ES(actor)) {
            do {
                if (UWAMONO_KEY_IN(record->key, 0x10025, 7)) {
                    if (riders[0] != NULL) {
                        riders[0]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                    }
                    if (riders[1] != NULL) {
                        riders[1]->drawFlags |= UWAMONO_DRAW_HIDDEN;
                    }
                    break;
                }
                if (UWAMONO_KEY_IN(record->key, 0x10015, 6)) {
                    if (riders[0] != NULL) {
                        riders[0]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                    }
                    if (riders[1] != NULL) {
                        riders[1]->drawFlags &= ~UWAMONO_DRAW_HIDDEN;
                    }
                    break;
                }
                if (riders[0] != NULL) {
                    riders[0]->drawFlags |= UWAMONO_DRAW_HIDDEN;
                }
                if (riders[1] != NULL) {
                    riders[1]->drawFlags |= UWAMONO_DRAW_HIDDEN;
                }
            } while (0);
        }
    }
    UnduParamInit(&uwamonoProbeTemplate);
    uwamonoProbeTemplate.block = actor->collision.block;
    UnduCheck(&actor->position, NULL, &uwamonoProbeTemplate);
    record->position = actor->position;
    record->rotation = actor->rotation;
    if (UWAMONO_IS_ES(actor)) {
        for (i = 0; i < 2; i++) {
            if (riders[i] != NULL) {
                riders[i]->position = actor->position;
                riders[i]->rotation = actor->rotation;
                riders[i]->direction = actor->direction;
                riders[i]->scale = actor->scale;
                riders[i]->vector70 = actor->vector70;
                riders[i]->vector80 = actor->vector80;
                riders[i]->velocity = actor->velocity;
                riders[i]->radius = actor->radius;
            }
        }
    }
    unused.x = unused.y = unused.z = 0.0f;
    unused.w = 1.0f;
    uwamonoPickTarget(uwamonoCurrentIndex);
    Check_EnemyFound();
    Check_Locater(uwamonoCurrentIndex);
    uwamonoPlayerUpdateDummy();
}


/* The first record other than owner (a talk or player record, not blocked) whose radius sphere, scaled by reachScale,
   reaches point (the owner position when null), or -1. */
int uwamonoFindOverlapRecord(int owner, AlignedVector4 *point, float reachScale)
{
    UwamonoRecord *ownerRecord = &uwamonoRecords[owner];
    struct XenoActor *ownerActor = ownerRecord->body.actor;
    AlignedVector4 *from = point ? point : &ownerActor->position;
    UwamonoState *motions = ownerRecord->body.motions;
    short other;

    for (other = 0; other < UWAMONO_RECORD_COUNT; other++) {
        float dx;
        float dz;
        float dist;
        float limit;

        int motionBlocked;

        if (uwamonoRecords[other].kind != UWAMONO_KIND_TALK && uwamonoRecords[other].kind != UWAMONO_KIND_PLAYER) {
            continue;
        }
        motionBlocked = motions->flags & 1;
        if (motionBlocked && uwamonoRecords[other].kind == UWAMONO_KIND_PLAYER) {
            continue;
        }
        if (owner == other || uwamonoRecords[other].body.actor->radius == 0.0f || (uwamonoRecords[other].flags & 0x02000000) != 0) {
            continue;
        }
        if (uwamonoRecordsOverlap(owner, other) == 0.0f) {
            continue;
        }
        dx = from->x - uwamonoRecords[other].body.actor->position.x;
        dz = from->z - uwamonoRecords[other].body.actor->position.z;
        dist = __builtin_sqrtf(dx * dx + dz * dz);
        limit = (ownerActor->radius + uwamonoRecords[other].body.actor->radius) * reachScale;
        if (dist <= limit) {
            return other;
        }
    }
    return -1;
}

/* Pushes the mover out to touch the obstacle's circle (by the sum of the radii, at the angle between them). Returns 1 when the new
   spot is free of other records and has floor, otherwise undoes the move and returns 0. */
int uwamonoPushOutOfActor(int moverIndex, struct XenoActor *obstacle)
{
    struct XenoActor *mover = uwamonoRecords[moverIndex].body.actor;
    AlignedVector4 *moverPosition = &mover->position;
    float angle = Get_Angle(moverPosition, &obstacle->position);
    float reach;
    AlignedVector4 goal;
    float groundHeight;

    moverPosition->x -= mover->velocity.x;
    mover->position.z -= mover->velocity.z;
    reach = mover->radius + obstacle->radius + 0.01f;
    goal.x = obstacle->position.x - reach * xglSin(angle);
    reach = reach * xglCos(angle);
    goal.z = obstacle->position.z - reach;
    mover->velocity.x = goal.x - mover->position.x;
    mover->velocity.z = goal.z - mover->position.z;
    mover->position.x += mover->velocity.x;
    mover->position.z += mover->velocity.z;
    if (uwamonoFindOverlapRecord(moverIndex, 0, 1.0f) != -1) {
        moverPosition->x -= mover->velocity.x;
        mover->position.z -= mover->velocity.z;
        return 0;
    }
    if (HitCheckMapUnit(mover) != -1) {
        moverPosition->x -= mover->velocity.x;
        mover->position.z -= mover->velocity.z;
        return 0;
    }
    groundHeight = UnduCheck(moverPosition, 0, &mover->collision);
    if (groundHeight == -1000.0f) {
        moverPosition->x -= mover->velocity.x;
        mover->position.z -= mover->velocity.z;
        return 0;
    }
    mover->position.y = groundHeight;
    return 1;
}

/* The record at uwamonoCurrentIndex, or null while no record is selected (index below zero). */
void *uwamonoCurrentRecord(void)
{
    if (!(uwamonoCurrentIndex >= 0)) {
        return 0;
    }
    return &uwamonoRecords[uwamonoCurrentIndex];
}

const float playerInitialRadius = 0.639999986f;
const float playerInitialHeightReach = 0.200000003f;
const float esFormRadius = 0.639999986f;
const float esFrameScale = 0.000510000042f;
const float humanModelScale = 0.899999976f;
const float humanFrameScale = 0.00144500006f;

int uwamonoSearchEventCount = -1;
int uwamonoSearchMode = -1;
int uwamonoCurrentIndex = -1;
int uwamonoPartIndices[2] = { -1, -1 };
unsigned char uwamonoKeyUnusedByte = 0xff;   /* no reader in this file */
short uwamonoKeyEffectId = -1;
unsigned char uwamonoKeyColorA[3] = { 0xff, 0xff, 0xff };
unsigned char uwamonoKeyColorB[3] = { 0xff, 0xff, 0xff };
signed char uwamonoMovingFlag = 0;
unsigned char playerSlotSkipHeaderFlag = 0;

/* .data of this file, address order (0x2B7CF0-0x2B9CF8). */
CameraSlotBlock cameraCfSlots = { 0 };

CameraSlotTable cameraSlotTable = { { 0 } };

/* Pairs of 16-bit ids per word: a model id, then the two part models that go with it (see uwamonoAttachIdTable above). */
unsigned int uwamonoAttachIdTable[UWAMONO_ATTACH_TABLE_WORDS] = {
    0x00010001, 0x00010006, 0x00010008, 0xFFFFFFFF,
    0x00010009, 0x0001000D, 0x0001000E, 0xFFFFFFFF,
    0x0001000F, 0x00010014, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00010015, 0x0001001A, 0x00010032, 0x00010033,
    0x0001001B, 0x0001001F, 0x00010020, 0xFFFFFFFF,
    0x00010022, 0x00010024, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00010025, 0x00010029, 0x0001002A, 0x0001002B,
    0x00014005, 0x00014005, 0x00014006, 0x00014007
};

int uwamonoKeyRangeTable[10 * 17] = {
    0x10001, 0x10006, 15, 16, 17, 0, 0, 0,
    0x14005, 29, 9, 14, 49, 10, 15, 50,
    306, 0x10009, 0x1000d, 3, 4, 5, 0, 0,
    0, 0x14005, 29, 9, 14, 49, 10, 15,
    50, 304, 0x1000f, 0x10014, 15, 16, 17, 0,
    0, 0, 0x14005, 29, 9, 14, 49, 10,
    15, 50, 303, 0x10015, 0x1001a, 15, 16, 17,
    0, 0, 0, 0x14005, 29, 9, 14, 49,
    10, 15, 50, 305, 0x1001b, 0x1001f, 15, 16,
    17, 0, 0, 0, 0x14005, 29, 9, 14,
    49, 10, 15, 50, 307, 0x10022, 0x10024, 3,
    4, 5, 0, 0, 0, 0x14005, 29, 9,
    14, 49, 10, 15, 50, 301, 0x10025, 0x10029,
    15, 16, 17, 0, 0, 0, 0x14005, 29,
    9, 14, 49, 10, 15, 50, 302, 0x14002,
    0x14002, 25, 26, 26, 0, 0, 0, 0x10023,
    21, 14, 19, 19, 15, 20, 20, 401,
    0x14003, 0x14003, 25, 26, 26, 0, 0, 0,
    0x10023, 19, 14, 19, 19, 15, 20, 20,
    403, 0x14005, 0x14005, 25, 26, 26, 0, 0,
    0, 0x10023, 34, 14, 19, 19, 15, 20,
    20, 402
};

float uwamonoSlopeFactorTable[4] = { 0.5f, 1.0f, 2.0f, 3.0f };

unsigned char uwamonoSlopeDirectionTable[16] = {
    0, 4, 2, 3, 0, 0, 1, 0, 6, 5, 0, 0, 7, 0, 0, 0
};

MapCollisionObject uwamonoProbeTemplate = { 0 };
