#include "common.h"
#include "main/xenoActor.h"
#include "main/xgl_timer0.h"
#include "debug_break.h"
#include "shared.h"
#include "main/game_data.h"
#include "main/game_map_object.h"
#include "main/ytachi_test.h"
#include "ov01/sb_vm.h"
#include "main/uwamono_objx.h"
#include "main/uwamono_record.h"
#include "main/ladder_table.h"
#include "main/init_uwamono_sys.h"
#include "main/xgl_cd.h"
#include "main/res_get_path.h"
#include "main/sef.h"

#include "main/xgl_2.h"
#define NULL ((void *) 0)

extern unsigned char mapSceneScriptBuffer[];      /* the scene file buffer */
extern const char defaultSceneName[];         /* "sc0000.sb": the digits are rewritten */
extern const char mapScenePathFormat[];         /* "cf\\%s" */
extern const char mapSceneLanguagePathFormat[];         /* "cf\\xx\\%sl" */
extern int uwamonoEventIds[];
extern unsigned short *DataSpline;

int sprintf(char *destination, const char *format, ...);


#define CLEAR_RUN_LENGTH 512

/* Empty body; no caller is known. */
void YtachiTest(void)
{
}

/* Loads a uwamono scene: copies the pose of every model record to its
 * transform, registers the script modules, with mode 1 makes id the sef owner,
 * reads cf\scNNNN.sb and the language file cf\xx\scNNNNl (NNNN are the digits
 * of id), makes id the current map, shows the current record and resets the
 * records through uwamonoResetRecords(-1, 1). The scene parse failing is a
 * debugger trap (DEBUG_BREAK, class ee-assert-break). */
void mapSceneScriptLoad(int id, signed char mode)
{
    char path[0x20];
    char languagePath[0x20];
    char *sceneName = defaultSceneName;
    int i;
    int slot;
    UwamonoRecord *record;
    int size;

    for (i = 0; i < MAP_RECORD_COUNT; i++) {
        record = &uwamonoRecords[i];
        if (record->kind == MAP_KIND_MODEL && record->transform != NULL) {
            *record->transform = record->pose;
        }
    }
    sbInit();
    scriptRegisterModules();
    if (mode == 1) {
        sefSetCurrentOwner(id);
        resApplyCfFile(id);
    }
    sceneName[2] = id / 1000 % 10 + '0';
    sceneName[3] = id / 100 % 10 + '0';
    sceneName[4] = id / 10 % 10 + '0';
    sceneName[5] = id % 10 + '0';
    sprintf(path, mapScenePathFormat, sceneName);
    size = xglCdReadFile(path, mapSceneScriptBuffer, 0, CD_NOTIFY_CURRENT);
    sprintf(languagePath, mapSceneLanguagePathFormat, sceneName);
    xglGetLanguageCode(languagePath + 3);
    xglCdReadFile(languagePath, mapSceneScriptBuffer + size, 0, CD_NOTIFY_CURRENT);
    if (!sbLibraryLink((struct SbImageHeader *)mapSceneScriptBuffer, (struct SbImageHeader *)(mapSceneScriptBuffer + size))) {
        DEBUG_BREAK();
    }
    sbStartImage((struct SbImageHeader *)mapSceneScriptBuffer);
    gameWork.mapId = id;
    uwamonoRecords[uwamonoCurrentIndex].flags |= 2;
    gameWork.nextMap = -1;
    for (slot = 0; slot < 8; slot++) {
        uwamonoEventIds[slot] = -1;
    }
    uwamonoResetRecords(-1, 1);
}

/* Clears gameWork.rowFlags (both 512-byte rows). */
void gameRowFlagsClear(void)
{
    int i;

    for (i = 0; i < CLEAR_RUN_LENGTH; i++) {
        gameWork.rowFlags[0][i] = 0;
        gameWork.rowFlags[1][i] = 0;
    }
}

/* Clears gameWork.eventFlags (512 bytes). */
void gameEventFlagsClear(void)
{
    int i;

    for (i = 0; i < CLEAR_RUN_LENGTH; i++) {
        gameWork.eventFlags[i] = 0;
    }
}

/* Resets uwamono records: one record (index >= 0) or all 256 (index -1). Mode 1
 * keeps the MAP_KIND_ACTOR_STATIC and MAP_KIND_PART records and releases each
 * reset record's actor through XenoActorRelease. With index -1 the 32
 * uwamonoStates state blocks are reset too, except the one whose owner is the kept
 * MAP_KIND_ACTOR_STATIC record, then uwamonoResetPoints runs and the enepcLookAtReachTable
 * radii and enepcParamSets are cleared. */
void uwamonoResetRecords(int index, signed char mode)
{
    int kept;
    int i;
    int j;
    int k;
    int first;
    int last;
    UwamonoRecord *record;
    UwamonoState *state;
    Vector4 *vector;

    /* No record kept yet: -100 rather than -1, which would match every free
     * state block (owner -1) below. */
    kept = -100;
    first = index;
    last = index + 1;
    if (index == -1) {
        first = 0;
        last = MAP_RECORD_COUNT;
    }
    DataSpline = mapObjectResources[MAP_RESOURCE_SPLINE];
    for (i = first; i < last; i++) {
        record = &uwamonoRecords[i];
        if (mode == 1) {
            if (record->kind == MAP_KIND_ACTOR_STATIC) {
                kept = i;
                continue;
            }
            if (record->kind == MAP_KIND_PART) {
                continue;
            }
        }
        record->kind = MAP_KIND_FREE;
        record->type = -1;
        record->parent = -1;
        record->tiltStep = -1;
        record->eventTimer = -1;
        record->transparencyStep = -1;
        record->scaleStep = -1;
        record->turnStep = -1;
        record->moveStep = -1;
        record->motion = 4;
        record->rotationOrder = 5;
        record->moveEndMode = -1;
        record->position.x = record->position.y = record->position.z = 0.0f;
        record->position.w = 1.0f;
        record->rotation.x = record->rotation.y = record->rotation.z = 0.0f;
        record->rotation.w = 1.0f;
        record->scale.x = record->scale.y = record->scale.z = 1.0f;
        record->scale.w = 1.0f;
        record->pivot.x = record->pivot.y = record->pivot.z = record->transparency = 0.0f;
        record->pivot.w = 1.0f;
        record->extent.x = record->extent.y = record->extent.z = 0.0f;
        record->extent.w = 1.0f;
        record->flags = 0xA;
        record->cellMask = 0xFF;
        record->searchMode = 0;
        record->form = 1.0f;
        record->velocity.x = record->velocity.y = record->velocity.z = 0.0f;
        record->velocity.w = 1.0f;
        record->tilt.x = record->tilt.y = record->tilt.z = 0.0f;
        record->tilt.w = 1.0f;
        record->landedFlags = 0;
        record->state = 0;
        record->unmodeled_630 = -1;
        record->transform = NULL;
        record->motionKind = 0;
        record->lookLink = -1;
        record->eventRow = -1;
        if (mode == 1 && record->body.actor != NULL) {
            XenoActorRelease(record->body.actor);
        }
        record->body.actor = NULL;
        record->body.motions = NULL;
        record->elevatorMotion = -1;
        record->elevatorArg = 0;
        record->handles[1] = NULL;
        record->handles[2] = NULL;
        record->handles[0] = NULL;
        record->releaseNextIndex = -1;
        record->effectId = -1;
        record->focusOffset.x = record->focusOffset.y = record->focusOffset.z = 0.0f;
        record->focusOffset.w = 1.0f;
        uwamonoClearMoveResetWord(i);
        uwamonoClearRotateResetWord(i);
        record->linkB = -1;
        record->linkA = -1;
        record->lightArea = -1;
        xglMatrixStackUnit();
        xglMatrixStackSave(record->savedMatrix.matrix);
    }
    if (index != -1) {
        return;
    }
    for (i = 0; i < UWAMONO_STATE_COUNT; i++) {
        state = &uwamonoStates[i];
        if (state->owner == kept) {
            continue;
        }
        state->owner = -1;
        state->index = i;
        state->row = -1;
        state->action = 0;
        state->focusIndex = 0;
        state->target.x = state->target.y = state->target.z = 0.0f;
        state->area.x = state->area.y = state->area.z = state->areaSize = 0.0f;
        state->noticeRange = state->fanLength = state->fanWidth = state->pursuitSpeed =
            state->moveSpeed = state->leashRange = state->wanderStep = 0.0f;
        state->wanderTime = 0;
        state->pickCountA = 0;
        state->waitTime = 0;
        state->pickCountB = 0;
        state->trailStep = 0;
        state->timer = 0;
        state->timerLimit = 0;
        state->alert = 0;
        state->mode = 0;
        state->cooldown = 0;
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 8; k++) {
                state->pickRow[j][k] = 0;
            }
            state->pickIndex[j] = 0;
        }
        state->reactionKind = 0;
        state->reactionFlags = 0;
        state->unmodeled_0078 = 0;
        state->unmodeled_007a = 0;
        state->trailCount = 0;
        for (j = 0; j < 256; j++) {
            state->trail[j][0] = state->trail[j][1] = state->trail[j][2] = 0.0f;
        }
        state->unmodeled_401c = 1;
        for (j = 0; j < 256; j++) {
            state->points[j].coords[0] = state->points[j].coords[1] =
                state->points[j].coords[2] = 0.0f;
            state->points[j].coords[3] = 1.0f;
            for (k = 0; k < 4; k++) {
                state->pointLinks[j][k] = 0;
                state->pointLinkValues[j][k] = 0;
            }
        }
        state->pointCount = 0;
        state->routeNodeCount = 0;
        for (j = 0; j < 64; j++) {
            state->routeNodes[j].coords[0] = state->routeNodes[j].coords[1] =
                state->routeNodes[j].coords[2] = 0.0f;
            state->routeNodes[j].coords[3] = 1.0f;
            state->routeLinkCounts[j] = 0;
            state->routeParents[j] = 0;
            for (k = 0; k < 4; k++) {
                state->routeLinks[j][k] = 0;
            }
        }
        for (j = 0; j < 8; j++) {
            state->groups[j] = -1;
        }
        for (j = 0; j < 4; j++) {
            state->together[j] = -1;
        }
        state->routeIndex = 0;
        state->routePrev = 0;
        state->motionCol = 0;
        /* The zero is a loop-local value: the original keeps it in a register
         * loaded just before the loop and walks the entries forwards. */
        vector = state->path;
        for (j = 0; j < 128; j++, vector++) {
            float zero = 0.0f;

            vector->x = zero;
        }
        state->unmodeled_3fc4[0] = 0;
        state->unmodeled_3fc4[1] = 0;
        state->unmodeled_3fc4[2] = 0;
        state->stepFrame = 0;
        state->soundCue = 0;
        state->unmodeled_3fd8[0] = 0;
        state->unmodeled_3fd8[1] = 0;
        state->probe.x = state->probe.y = state->probe.z = 0.0f;
        state->unmodeled_3ff4 = -1;
        state->homePoint.x = 0.0f;
        state->noticeCooldown = 0;
        state->id = 0;
        state->groundId = -1;
        state->footId = -1;
        state->targetRecord = -1;
        state->flags = 0;
        state->pathStep = -1;
        state->pathTail = 0;
        state->seIds[2] = -1;
        state->seIds[1] = -1;
        state->seIds[0] = -1;
        state->battle = 0;
        state->scanState = 99;
        for (j = 0; j < 16; j++) {
            state->walk[j] = j;
        }
        state->resourceId = -1;
        state->targetPriority = 0;
        state->targetSlot = -1;
        state->targetVector.x = state->targetVector.y = state->targetVector.z = 0.0f;
        state->effect = NULL;
        state->unmodeled_406c = 0;
        state->unmodeled_4070 = 0;
    }
    uwamonoResetPoints();
    for (i = 0; i < 10; i++) {
        enepcLookAtReachTable[i] = 0.0f;
    }
    enepcParamSets = NULL;
}

const char mapScenePathFormat[8] = "cf\\%s";
struct EnepcParamSet *enepcParamSets = NULL;
UwamonoKeyRow *uwamonoKeyRowTable = 0;
UwamonoEventRow *uwamonoEventTable = 0;
unsigned char uwamonoTriggerTypeFlag[12] = { 0 };

const char defaultSceneName[16] = "sc0000.sb";
const char mapSceneLanguagePathFormat[16] = "cf\\xx\\%sl";

/* 32 UwamonoState blocks of 0x40C0 bytes (enemy_1.c and set_motion.c read them). */
UwamonoState uwamonoStates[32] = { 0 };

/* Nine Vector4 entries; runtime_motion_face.c and game_camera.c read them (as float[][4] and Vector4[]). */
float enepcLookAtPointTable[9][4] = { 0 };

/* Ten radii of enepcLookAtReachTable are cleared here; the 12th float is padding in this layout. */
float enepcLookAtReachTable[12] = { 0 };

/* .data of this file read by uwamono_record.c (0x33EEC0-0x33FDC0; link order, data map review 2026-11-17). */
/* 16 records of the point table, all zero at startup. */
UwamonoPoint uwamonoPoints[UWAMONO_POINT_COUNT] = { 0 };

/* Ladder table: 0x0D00 bytes of zero at startup (208-byte records). */
LadderSlot ladderSlots[LADDER_SLOT_COUNT] = { 0 };

/* .data of this file read by init_uwamono_sys.c and others (0x33FDC0-0x3E6DC0). */
/*
 * .data, in address order. The record table of the uwamono system: 256 UwamonoRecord entries of 0x870 bytes
 * at 0x33FDC0. Every other label below 0x3C6DC0 is a field offset of record 0 (uwamonoMoveRecords,
 * uwamonoRotateRecords, uwamonoScaleRecords, progEffectTable, ...); the extern declarations above keep
 * the code's stride and resolve through the linker.
 */
UwamonoRecord uwamonoRecords[256] = { 0 };

unsigned char mapSceneScriptBuffer[131072] = { 0 };
