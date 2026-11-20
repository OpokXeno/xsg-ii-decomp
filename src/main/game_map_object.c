#include "common.h"
#include "main/pause_menu_page_uwamono.h"
#include "main/elevator.h"
#include "main/enemy_1.h"
#include "main/face_point.h"
#include "main/game_camera.h"
#include "main/init_uwamono_sys.h"
#include "main/ladder_table.h"
#include "main/nml_model_set.h"
#include "main/xgl_jpeg.h"
#include "main/game_map_object.h"
#include "main/get.h"
#include "shared.h"
#include "main/xgl_camera.h"
#include "main/undulate.h"
#include "main/game_data.h"
#include "main/ytachi_test.h"
#include "main/uwamono_record.h"
#include "main/sef.h"
#include "main/xgl_packet.h"
#include "ov01/calc.h"
#include "main/cur_matrix_asm.h"
#include "main/xgl_2.h"
#include "main/fade_ctrl.h"
#include "main/nml_packet_add.h"
#include "main/pxy_packet_state.h"
#include "main/xgl_studio.h"
#include "main/se_bank_load.h"
#include "main/res_get_path.h"
#include "main/game_defocus.h"
#include "main/uwamono_event_check.h"
#include "main/uwamono_light_blend.h"
#include "main/script_register_natives.h"

/* Argument block of xglJpegDecode. */
typedef struct JpegInfo {
    u8 *source;                 /* +0x00 */
    u8 *output;                 /* +0x04 */
    int dataOffset;             /* +0x08 */
    u8 unmodeled_0C[0x08];
    short height;               /* +0x14: rows, stored to MainActive.height */
    short width;                /* +0x16 */
    u8 unmodeled_18[0x08];
} JpegInfo;

/* The rider markers a map object owns (UwamonoRecord.handles[1] and [2]) as sef.h SefEffectRecord, with the position and
 * scale accessed through the DoublewordVector4 union (alias set 0, which keeps the loads of the record fields behind the
 * stores); only the fields touched here. */
typedef struct MapEffectView {
    DoublewordVector4 position;     /* +0x00 */
    u8 unmodeled_10[0x10];
    DoublewordVector4 scale;        /* +0x20 */
    u8 unmodeled_30[0x14];
    int flags;                  /* +0x44: bit 0x10 follows bit 0x2000 of UwamonoRecord.flags */
    u8 unmodeled_48[0x26];
    short shown;                /* +0x6E: bit 1 of UwamonoRecord.flags, shifted down */
} MapEffectView;

/* Bits of gameWork.control (CONTROL_PAUSED is GAME_PAUSE_MAP_OBJECTS here: game_camera.h cannot be included,
 * its record types clash). */
#define GAME_PAUSE_MAP_OBJECTS 0x01
#define GAME_HIDE_RIDERS_DELAY 0x10 /* when set, one frame passes before GAME_HIDE_RIDERS is cleared */
#define GAME_HIDE_RIDERS 0x20
/* UwamonoRecord.flags bits. */
#define MAP_OBJ_SHOWN 0x02
/* Bit `key` of a map flag set (an array of 32-bit words). */
#define MAP_BIT_CLEAR(set, key) ((set)[(key) >> 5] &= ~(1 << ((key) & 0x1F)))
#define MAP_BIT_SET(set, key) ((set)[(key) >> 5] |= 1 << ((key) & 0x1F))
#define MAP_OBJ_EFFECT_FOLLOW 0x2000 /* mirrored into SefEffectRecord.flags bit SEF_FOLLOW */
#define SEF_FOLLOW 0x10
/* First word of a JPEG stream (FF D8 FF E0) read as a little-endian int, and the size of its decode buffer. */
#define JPEG_MAGIC_WORD ((int)0xE0FFD8FF)
#define JPEG_DECODE_BUFFER_SIZE 0x400000

/* The header block of a map model: two matrices, the second is the model's base transform. */
typedef struct MapModelHeader {
    union Matrix matrices[2];
} MapModelHeader;

typedef struct MapModelData {
    u8 unmodeled_00[0x68];
    int headerOffset;           /* +0x68: byte offset of the MapModelHeader */
} MapModelData;

/* Entry of mapObjectResources holding the model (data, then texture) of an extra-object type: two entries per model, the models start at entry 4. */
#define MAP_MODEL_SLOT(type) (((type) + 2) * 2)

extern float negativeZeroFloat[];

/* A 0x12000-byte block copy of the map image. */
typedef struct MapImageBlock {
    unsigned char bytes[0x12000];
} MapImageBlock;

/* Map resource scale constants read by mapResourceLoadCallback. */
float mapDefaultGravity = 0.02f;
float mapLightType4DefaultValue = 0.15f;

void mapResourceLoadCallback(int index, void *buffer, unsigned int size, void *value)
{
    JpegDecodeControl jpegInfo;
    MainActive *record = buffer;
    PxyHeader *object;
    PxyHeader *secondHeader;
    u8 *pair;
    u8 *pixels;
    int i;
    int entryIndex;
    int i2;
    int secondOffset;

    int jpegData;

    object = record->object;
    jpegData = record->jpegData;
    mainObject = object;
    mapTextureData = jpegData;

    if (object != 0) {
        xglCullingMapSet(object);
        if (mainObject != 0) {
            PxyHeader *m = mainObject;
            for (entryIndex = 0; entryIndex < m->entryCount; entryIndex++) {
                MAP_BIT_CLEAR(mapMainHideMask.bits, entryIndex);
            }
            secondOffset = m->subPackOffset;
            secondHeader = (PxyHeader *)((u8 *)m + secondOffset);
            mapObjectTable = secondHeader;
            if (secondOffset != 0) {
                i2 = 0;
                if (secondHeader->entryCount > 0) {
                    PxyHeader *headerBase = secondHeader;
                    secondHeader = 0;
                    do {
                        MAP_BIT_CLEAR(mapSecondHideMask.bits, i2);
                        i2++;
                    } while (i2 < headerBase->entryCount);
                }
            }
        }
    }

    if (mapTextureData != 0 && *(int *)mapTextureData == JPEG_MAGIC_WORD) {
        record->jpegStream = (int *)mapTextureData;
        record->pending = GameResBufferAlloc(JPEG_DECODE_BUFFER_SIZE);
        record->offsetX = 0;
        record->offsetY = 0;
        memset(&jpegInfo, 0, sizeof(jpegInfo));
        jpegInfo.source = (u8 *)record->jpegStream;
        jpegInfo.destination = record->pending;
        xglJpegDecode(&jpegInfo);
        pair = jpegInfo.destination;
        /* The decoder stores the image width at +0x14 and the height at +0x16; the record keeps them in the
         * opposite fields. */
        record->height = jpegInfo.imageLimits.width;
        record->width = jpegInfo.imageLimits.height;
        pixels = jpegInfo.source + ((jpegInfo.consumedSize + 0x3F) & ~0x3F);
        for (i = 0; i < jpegInfo.imageLimits.width * jpegInfo.imageLimits.height; i++) {
            pair[3] = (u8)(*pixels >> 1);
            pixels++;
            pair += 4;
        }
        mapTextureData = 0;
    }

    mapLoadBlock(record->block);
    uwamonoEntryCountPtr = 0;
    uwamonoEntryCountPtr = UnduDataGetHeader(0x300, -1);
    if (uwamonoEntryCountPtr != 0) {
        unsigned short entryCount = uwamonoEntryCountPtr->recordCount;
        uwamonoEntries = (UwamonoEntry *) ((int) uwamonoEntryCountPtr + 16);
        i2 = 0;
        if (entryCount != 0) {
            do {
            } while (++i2 < (int)uwamonoEntryCountPtr->recordCount);
        }
    }

    gameWork.control |= GAME_HIDE_RIDERS_DELAY | GAME_HIDE_RIDERS;
    if (gameWork.encounterReaction == -1) {
        float zeroF = 0.0f;
        gameWork.fogDist.w = zeroF;
        gameWork.fogDist.z = zeroF;
        gameWork.fogDist.y = zeroF;
        gameWork.fogDist.x = zeroF;
    }
    gameWork.gravity = mapDefaultGravity;
    gameWork.bounceDivisor = 1.5f;
    if (gameWork.encounterReaction == -1) {
        ladderInitTable();
    }
    sefKillEffect(-1);

    {
    u8 *imageHead = record->imageHead;
    if (imageHead != 0) {
        u8 *copyDst = gameWork.resBuffer;
        u8 *copySrc = imageHead + (((unsigned int)imageHead & 0xFF) >> 8);
        *(MapImageBlock *)copyDst = *(MapImageBlock *)copySrc;
        gameWork.imageEnd = (struct SbImageHeader *)(gameWork.resBuffer + record->imageOffset);
    }
    }

    {
        float zero = negativeZeroFloat[0];
        float one = 1.0f;
        float half = 0.5f;
        float negOne = -1.0f;
        for (i2 = 0; i2 < 16; i2++) {
            gameWork.lights.direction[i2][0][0] = zero;
            gameWork.lights.direction[i2][0][1] = negOne;
            gameWork.lights.direction[i2][0][2] = zero;
            gameWork.lights.direction[i2][0][3] = one;
            gameWork.lights.direction[i2][1][0] = one;
            gameWork.lights.direction[i2][1][1] = one;
            gameWork.lights.direction[i2][1][2] = one;
            gameWork.lights.direction[i2][1][3] = one;
            gameWork.lights.direction[i2][2][0] = one;
            gameWork.lights.direction[i2][2][1] = one;
            gameWork.lights.direction[i2][2][2] = one;
            gameWork.lights.direction[i2][2][3] = one;
            gameWork.lights.color[i2][0][0] = half;
            gameWork.lights.color[i2][0][1] = half;
            gameWork.lights.color[i2][0][2] = half;
            gameWork.lights.color[i2][0][3] = one;
            gameWork.lights.color[i2][1][0] = half;
            gameWork.lights.color[i2][1][1] = half;
            gameWork.lights.color[i2][1][2] = half;
            gameWork.lights.color[i2][1][3] = one;
            gameWork.lights.color[i2][2][0] = half;
            gameWork.lights.color[i2][2][1] = half;
            gameWork.lights.color[i2][2][2] = half;
            gameWork.lights.color[i2][2][3] = one;
            gameWork.lights.ambient[i2][0] = half;
            gameWork.lights.ambient[i2][1] = half;
            gameWork.lights.ambient[i2][2] = half;
            gameWork.lights.ambient[i2][3] = one;
            gameWork.lights.type4Direction[i2][0] = 0;
            gameWork.lights.type4Direction[i2][1] = one;
            gameWork.lights.type4Direction[i2][2] = 0;
            gameWork.lights.type4Direction[i2][3] = 0;
            gameWork.lights.type4Value[i2] = mapLightType4DefaultValue;
        }
    }

    for (i2 = 0; i2 < MAP_RECORD_COUNT; i2++) {
        uwamonoRecords[i2].lightArea = -1;
    }
}


/* Makes id the current resource. With mode 1, a reload of the current id releases it first and then
 * uwamonoResetRecords(-1, 1) and sefResetHandleSlots() run. id is loaded (retried until GameResLoadID stops
 * returning -1) and a different previous id is released. */
void mapResourceSwitch(int id, signed char mode)
{
    int prev_id;

    prev_id = gameWork.resourceId;
    if (mode == 1) {
        if (prev_id == id) {
            GameResBlockFreeByOwner(id);
        }
        uwamonoResetRecords(-1, 1);
        sefResetHandleSlots();
    }
    gameWork.resourceId = id;
    while (GameResLoadID(id, (void **)&gameWork.active, 0, mapResourceLoadCallback) == -1) {
    }
    if (id != prev_id) {
        GameResBlockFreeByOwner(prev_id);
    }
}


void GameDefocusReset(void)
{
    GameDefocusSet(0, 0, 0);
}


/* Advances the move, turn, scale and fade steps of one map object by a frame.
 * Returns 1 when any of the four steps ran. */
int mapObjectAdvanceTweens(UwamonoRecord *record)
{
    int advanced = 0;

    if (gameWork.control & GAME_PAUSE_MAP_OBJECTS) {
        return 0;
    }
    if (record->moveStep != -1) {
        if (record->moveStepCount == 0) {
            Get_MiddlePoint(&record->moveFrom, &record->moveTo, record->moveStep,
                            record->moveCount, &record->position);
        } else {
            Get_MiddlePoint_Curve(&record->moveFrom, &record->moveTo, record->moveStep,
                          record->moveCount, record->moveX, record->moveY,
                          record->moveStepCount, &record->position);
        }
        record->moveStep = record->moveStep + 1;
        if (record->moveCount < record->moveStep) {
            short endMode = record->moveEndMode;

            record->moveStep = -1;
            record->moveCount = 0;
            if (endMode != -1) {
                XenoActor *actor = record->body.actor;

                if (actor != 0) {
                    XenoActorCheckMode(actor, endMode);
                }
            }
        }
        advanced = 1;
    }
    if (record->turnStep != -1) {
        if (record->turnStepCount == 0) {
            Get_MiddlePoint(&record->turnFrom, &record->turnTo, record->turnStep,
                            record->turnCount, &record->rotation);
        } else {
            Get_MiddlePoint_Curve(&record->turnFrom, &record->turnTo, record->turnStep,
                          record->turnCount, record->turnX, record->turnY,
                          record->turnStepCount, &record->rotation);
        }
        record->turnStep = record->turnStep + 1;
        if (record->turnCount < record->turnStep) {
            XenoActor *actor = record->body.actor;

            record->turnStep = -1;
            record->turnCount = 0;
            if (actor != 0) {
                actor->direction.y = record->rotation.y;
            }
        }
        advanced = 1;
    }
    if (record->scaleStep != -1) {
        record->scale.x = record->scaleFrom.x
            + ((record->scaleTo.x - record->scaleFrom.x) * (float) record->scaleStep) / (float) record->scaleCount;
        record->scale.y = record->scaleFrom.y
            + ((record->scaleTo.y - record->scaleFrom.y) * (float) record->scaleStep) / (float) record->scaleCount;
        record->scale.z = record->scaleFrom.z
            + ((record->scaleTo.z - record->scaleFrom.z) * (float) record->scaleStep) / (float) record->scaleCount;
        record->scaleStep = record->scaleStep + 1;
        if (record->scaleCount < record->scaleStep) {
            record->scaleStep = -1;
            record->scaleCount = 0;
        }
        advanced = 1;
    }
    if (record->transparencyStep != -1) {
        record->transparency = record->transparencyFrom
            + ((record->transparencyTo - record->transparencyFrom) * (float) record->transparencyStep) / (float) record->transparencyCount;
        record->transparencyStep = record->transparencyStep + 1;
        if (record->transparencyCount < record->transparencyStep) {
            record->transparencyStep = -1;
            record->transparencyCount = 0;
        }
        advanced = 1;
    }
    return advanced;
}

void mapObjectUpdateEffectTransform(UwamonoRecord *record)
{
    SefEffectRecord *effect;

    effect = record->handles[0];
    if (effect == 0) { return; }
    if (record->key != -1) {
        effect->gate = (record->flags >> 1) & 1;
        if (record->flags & MAP_OBJ_EFFECT_FOLLOW) { effect->flags |= SEF_FOLLOW; } else { effect->flags &= ~SEF_FOLLOW; }
    }
    if ((gameWork.control & 1) == 0) {
        if (record->parent == -1 || record->attached != 1) {
            mapObjectAdvanceTweens(record);
        } else {
            record->position.x = uwamonoRecords[record->parent].positionCoords[0] + record->savedPosition.x;
            record->position.y = uwamonoRecords[record->parent].positionCoords[1] + record->savedPosition.y;
            record->position.z = uwamonoRecords[record->parent].positionCoords[2] + record->savedPosition.z;
            record->rotation.x = uwamonoRecords[record->parent].rotationCoords[0] + record->savedRotation.x;
            record->rotation.y = uwamonoRecords[record->parent].rotationCoords[1] + record->savedRotation.y;
            record->rotation.z = uwamonoRecords[record->parent].rotationCoords[2] + record->savedRotation.z;
        }
    }
    effect->position.x = record->position.x;
    effect->position.y = record->position.y;
    effect->position.z = record->position.z;
    effect->position.w = 1.0f;
    effect->rotation.x = record->rotation.x;
    effect->rotation.y = record->rotation.y;
    effect->rotation.z = record->rotation.z;
    effect->rotation.w = 1.0f;
    effect->scale.x = record->scale.x;
    effect->scale.y = record->scale.y;
    effect->scale.z = record->scale.z;
    effect->scale.w = 1.0f;
}

/* The GIF packet of the map image strips; only the words rewritten per strip. */
typedef struct StripPacket {
    u8 unmodeled_00[0x30];
    long long tag;              /* +0x30 */
    u8 unmodeled_38[0x28];
    int topY;                   /* +0x60 */
    u8 unmodeled_64[0x10];
    int topV;                   /* +0x74 */
    int topU;                   /* +0x78 */
    u8 unmodeled_7c[4];
    int bottomY;                /* +0x80 */
    u8 unmodeled_84[0x10];
    int bottomV;                /* +0x94 */
    int bottomU;                /* +0x98 */
} StripPacket;

extern StripPacket mapBackdropStripPacket[];
extern u8 mapBackdropImagePacket[];
extern Vector4 mapBackdropOriginVector;

typedef struct ScanDelta {
    float x;
    float y;
    float z;
    float w;
} ScanDelta;

void mapBackdropDraw(MainActive *active)
{
    u8 *cursor;
    ScanDelta delta;
    XenoActor *target;
    float bestDistance;
    float nearestDz;
    float dx;
    float dz;
    float distanceSq;
    int i;
    int recordIndex;
    int rowFixedBottom;
    int newViewX;
    int col;
    int row;
    int maxCol;
    int maxRow;
    int rowFixedTop;
    u8 *imageAddress;
    int texAddress;
    int state;
    XglPacket *packet;
    XglCamera *camera;
    StripPacket *strip;

    target = uwamonoRecords[uwamonoCurrentIndex].body.actor;
    cursor = (u8 *)&uwamonoRecords[0].position.z;
    nearestDz = 1.0f;
    bestDistance = 1000000.0f;
    /* cursor walks the records at their position.z: -0x16 is kind, -8 position.x, +0x579 searchShape (a typed pointer
     * to the record picks another base register). */
    for (recordIndex = 0; recordIndex < MAP_RECORD_COUNT; recordIndex++, cursor += sizeof(UwamonoRecord)) {
        if (*(cursor - 0x16) == MAP_KIND_MOVER) {
            state = *((signed char *)cursor + 0x579);
            if (state != 0 && state != 10) {
                dx = *(float *)(cursor - 8) - target->position.x;
                delta.x = dx;
                dz = *(float *)cursor - target->position.z;
                delta.z = dz;
                distanceSq = dx * dx + dz * dz;
                delta.w = distanceSq;
                if (distanceSq < bestDistance) {
                    bestDistance = distanceSq;
                    nearestDz = dz;
                }
            }
        }
    }

    camera = xglStudioGetCamera2(0);
    _CurMatrixSet(&camera->volume[0].viewMatrix);
    _CurSetViewScaleTrans(&camera->screen.screenScale, &camera->screen.screenOffset);
    _CurRotTransPersClipScreen(&active->view, &mapBackdropOriginVector);

    active->view.x = 0x800 - (active->view.x >> 4);
    newViewX = 0x800 - (active->view.y >> 4);
    active->view.y = newViewX;
    row = (short)active->height / 2 + active->offsetY + active->view.x - 0x100;
    if (row < 0) {
        row = 0;
    }
    maxRow = (short)active->height - 0x201;
    if (maxRow < row) {
        row = maxRow;
    }
    col = (short)active->width / 2 + active->offsetX + newViewX - 0xE0;
    if (col < 0) {
        col = 0;
    }
    maxCol = (short)active->width - 0x1C0;
    if (maxCol < col) {
        col = maxCol;
    }
    imageAddress = active->pending + (col << 12);
    rowFixedTop = row << 4;
    i = 0;
    pxySetSourceDrawPass();
    packet = xglPacketGetCurrent();

    strip = mapBackdropStripPacket;
    texAddress = 0x71F7;
    rowFixedBottom = rowFixedTop + 0x2000;
    for (; i < 4; i++) {
        sceVif1PkRef(packet, mapBackdropImagePacket, 7, 0, 0, 0);
        sceVif1PkRef(packet, imageAddress, 0x7000, 0, 0x51007000, 0);
        strip->tag = 0x53001;
        strip->topV = texAddress;
        texAddress += 0x700;
        strip->topU = 0;
        strip->topY = rowFixedTop;
        imageAddress += 0x70000;
        strip->bottomV = texAddress;
        strip->bottomU = 0;
        strip->bottomY = rowFixedBottom;
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, strip, 40);
        if (0.0f < nearestDz) {
            strip->tag = 0x5000D;
            strip->topU = 0x20000;
            strip->bottomU = 0x20000;
            sceVif1PkAddDataN(packet, strip, 40);
        }
    }
}

int DispMapObject(void)
{
    Vector4 point;
    MapEffectView *riders[2];
    float matrix[4][4];
    UwamonoRecord *record;
    XenoActor *actor;
    MapModelHeader *header;
    MapModelHeader *source;
    MapModelData *data;
    MainActive *active;
    float size;
    int i;
    int j;
    int k;

    if (gameWork.control & GAME_HIDE_RIDERS) {
        if (gameWork.control & GAME_HIDE_RIDERS_DELAY) {
            gameWork.control &= ~GAME_HIDE_RIDERS_DELAY;
        } else {
            gameWork.control &= ~GAME_HIDE_RIDERS;
        }
    }
    xglMatrixUnit(matrix);
    for (i = 0; i < MAP_RECORD_COUNT; i++) {
        record = &uwamonoRecords[i];
        if (record->kind != MAP_KIND_FREE) {
        if (record->eventTimer != -1) {
            if (record->eventTimer-- <= 0) {
                switch (record->event) {
                case 0:
                    gameWork.nextMap = record->eventState;
                    gameWork.entryKey = record->eventStateArg;
                    break;
                case 1:
                    uwamonoChangePlayerModel(-1000);
                    fadeStartIn(30);
                    break;
                }
            }
        }
        switch (record->kind) {
        case MAP_KIND_ACTOR:
        case MAP_KIND_ACTOR_STATIC:
            actor = record->body.actor;
            Sound_FootStep(i);
            if (record->flags & MAP_OBJ_SHOWN) {
                actor->drawFlags &= ~4;
                actor->drawMode = record->motion;
                uwamonoStartAreaBlend(i);
            } else {
                actor->drawFlags |= 4;
            }
            if (record->kind != MAP_KIND_ACTOR_STATIC) {
                actor->previous.vector.x = actor->position.x;
                actor->previous.vector.y = actor->position.y;
                actor->previous.vector.z = actor->position.z;
                actor->previous.vector.w = actor->position.w;
            }
            if (mapObjectAdvanceTweens(record) == 1) {
                if (record->flags & 0x08000000) {
                    record->position.y = unduGetSlotHeight(i, &record->body.actor->position, record->body.motions->id,
                                                       record->body.motions->groundId, 0);
                }
                actor->velocity.x = record->position.x - actor->position.x;
                /* The original stores all three differences into velocity.x; y and z are left alone. */
                actor->velocity.x = record->position.y - actor->position.y;
                actor->velocity.x = record->position.z - actor->position.z;
                actor->velocity.w = 1.0f;
                actor->position.x = record->position.x;
                actor->position.y = record->position.y;
                actor->position.z = record->position.z;
                actor->position.w = 1.0f;
                actor->rotation.x = record->rotation.x;
                actor->rotation.y = record->rotation.y;
                actor->rotation.z = record->rotation.z;
                actor->rotation.w = 1.0f;
            }
            actor->scale.x = record->scale.x;
            actor->scale.y = record->scale.y;
            actor->scale.z = record->scale.z;
            actor->scale.w = 1.0f;
            if (record->transparency == 0.0f) {
                actor->drawFlags &= ~0x10000;
            } else {
                actor->drawFlags |= 0x10000;
                actor->alpha = (100.0f - record->transparency) / 100.0f;
            }
            if (record->kind != MAP_KIND_ACTOR_STATIC) {
                ACT_updateEnemy(i);
            }
            if ((record->flags & 0x400) && (record->flags & 0x400000)) {
                record->rotation.y = Get_Cursol_by_Reduce_Speed_Angle_Loop(record->rotation.y, actor->direction.y, 4.0f);
                if (!(gameWork.control & 0x8000)) {
                    mapObjectClearTalkFlags(record);
                }
            }
            uwamonoLightBlendStep(i);
            DispPillar(&record->position, record->extent.x, 0.1f, 7);
        }
        }
    }
    if (mapTextureData == 0 || mainObject == 0) {
        active = gameWork.active;
        if (active != 0 && active->pending != 0) {
            for (k = 0; k < MAP_RECORD_COUNT; k++) {
                record = &uwamonoRecords[k];
                if (record->kind == MAP_KIND_FREE) {
                    continue;
                }
                if (record->kind == MAP_KIND_EFFECT) {
                    mapObjectUpdateEffectTransform(record);
                }
                if (record->body.actor != 0) {
                    XenoActorPushMatrix(record->body.actor);
                }
            }
            mapBackdropDraw(active);
        }
        return 0;
    }
    for (j = 0; j < MAP_RECORD_COUNT; j++) {
        record = &uwamonoRecords[j];
        if (record->kind == MAP_KIND_FREE) {
            continue;
        }
        switch (record->kind) {
        case MAP_KIND_MODEL:
            if (record->key != -1) {
                if (record->flags & MAP_OBJ_SHOWN) {
                    MAP_BIT_CLEAR(mapSecondHideMask.bits, record->key);
                } else {
                    MAP_BIT_SET(mapSecondHideMask.bits, record->key);
                }
                if (record->flags & 0x1000) {
                    MAP_BIT_CLEAR(mapSecondPartMask, record->key);
                } else {
                    MAP_BIT_SET(mapSecondPartMask, record->key);
                }
            }
            if (!(gameWork.control & GAME_PAUSE_MAP_OBJECTS)) {
                if (record->parent == -1 || record->attached != 1) {
                    mapObjectAdvanceTweens(record);
                    if (record->tiltStep != -1) {
                        record->tiltStep++;
                        if (record->tiltStep > record->tiltCount) {
                            record->tiltStep = -1;
                        }
                    }
                } else {
                    record->position.x = record->savedPosition.x;
                    record->position.y = record->savedPosition.y;
                    record->position.z = record->savedPosition.z;
                    record->rotation.x = record->savedRotation.x;
                    record->rotation.y = record->savedRotation.y;
                    record->rotation.z = record->savedRotation.z;
                    record->position.w = 1.0f;
                    record->rotation.w = 1.0f;
                }
            }
            if (record->motionKind == 2) {
                if (record->busy == 0 && record->tiltStep == -1) {
                    record->searchMode = 0;
                } else {
                    record->searchMode = 1;
                }
            }
            uwamonoUpdateTransform(record);
            if (record->elevatorMotion != -1) {
                elvUpdateMotion(record);
            }
            if (record->parent == -1 || record->attached != 1) {
                xglMatrixStackUnit();
            } else {
                xglMatrixStackLoad(uwamonoRecords[record->parent].matrix);
            }
            xglMatrixStackTrans(&record->position);
            xglMatrixStackTrans(&record->pivot);
            switch (record->rotationOrder) {
            case 0:
                xglMatrixStackRotX(record->rotation.x);
                xglMatrixStackRotY(record->rotation.y);
                xglMatrixStackRotZ(record->rotation.z);
                break;
            case 1:
                xglMatrixStackRotX(record->rotation.x);
                xglMatrixStackRotZ(record->rotation.z);
                xglMatrixStackRotY(record->rotation.y);
                break;
            case 2:
                xglMatrixStackRotY(record->rotation.y);
                xglMatrixStackRotX(record->rotation.x);
                xglMatrixStackRotZ(record->rotation.z);
                break;
            case 3:
                xglMatrixStackRotY(record->rotation.y);
                xglMatrixStackRotZ(record->rotation.z);
                xglMatrixStackRotX(record->rotation.x);
                break;
            case 4:
                xglMatrixStackRotZ(record->rotation.z);
                xglMatrixStackRotX(record->rotation.x);
                xglMatrixStackRotY(record->rotation.y);
                break;
            case 5:
                xglMatrixStackRotZ(record->rotation.z);
                xglMatrixStackRotY(record->rotation.y);
                xglMatrixStackRotX(record->rotation.x);
                break;
            }
            point.x = -record->pivot.x;
            point.y = -record->pivot.y;
            point.z = -record->pivot.z;
            point.w = 1.0f;
            xglMatrixStackTrans(&point);
            xglMatrixStackScale(&record->scale);
            xglMatrixStackSave(record->transform->matrix);
            xglMatrixStackSave(record->matrix);
            if (record->type != -1) {
                header = nmlPacketSetAttributeAlloc16N(8);
                data = mapObjectResources[MAP_MODEL_SLOT(record->type)];
                source = (MapModelHeader *) ((u8 *) data + data->headerOffset);
                if (record->tiltStep != -1) {
                    Get_MiddlePoint(&record->turnFrom, &record->turnTo, record->tiltStep, record->tiltCount,
                                    &record->tilt);
                    if (record->tiltCount < record->tiltStep) {
                        record->tiltStep = -1;
                    }
                }
                if (record->flags & MAP_OBJ_SHOWN) {
                    *header = *source;
                    xglMatrixStackLoad(header->matrices[1].rows);
                    xglMatrixStackRotX(record->tilt.x);
                    xglMatrixStackSave(header->matrices[1].rows);
                    nmlModelSetDrawMatrix(record->transform->matrix);
                    pxySetPartMatrices(header->matrices);
                    nmlModelSetTexProreal();
                    nmlModelSetTexture(mapObjectResources[MAP_MODEL_SLOT(record->type) + 1]);
                    pxySubmitModel(mapObjectResources[MAP_MODEL_SLOT(record->type)]);
                }
                if (record->type >= 7 && !(gameWork.control & GAME_PAUSE_MAP_OBJECTS)) {
                    record->rotation.y = Get_Decimal_Surplus_for_Radius(record->rotation.y + 0.1f);
                }
                if (record->type >= 7 && record->type <= 9) {
                    if (Get_Distance3D(&uwamonoRecords[uwamonoCurrentIndex].position, &record->position)
                        <= (size = uwamonoRecords[uwamonoCurrentIndex].form) * 0.5f) {
                        if (record->handles[0] == 0) {
                            record->handles[0] = sefCreateEffectCf(size == 1.0f ? 0x100C8032 : 0x100C8064,
                                                                      &record->position, 0);
                            seVoiceStartDefault(0x02000010);
                            battleSlotsRestoreAll();
                        }
                    } else {
                        if (record->handles[0] != 0) {
                            sefDeleteEffectCf(record->handles[0]);
                        }
                        record->handles[0] = 0;
                    }
                }
            }
            point.x = record->position.x + record->pivot.x;
            point.y = record->position.y + record->pivot.y;
            point.z = record->position.z + record->pivot.z;
            point.w = 1.0f;
            uwamonoDrawPointAxes(&point);
            uwamonoDrawPointAxes(&record->transform->row[3]);
            break;
        case MAP_KIND_FLAG:
            if (record->flags & MAP_OBJ_SHOWN) {
                MAP_BIT_CLEAR(mapMainHideMask.bits, record->key);
            } else {
                MAP_BIT_SET(mapMainHideMask.bits, record->key);
            }
            break;
        case MAP_KIND_EFFECT:
            mapObjectUpdateEffectTransform(record);
            break;
        case MAP_KIND_MOVER:
            record->previous.x = record->position.x;
            record->previous.y = record->position.y;
            record->previous.z = record->position.z;
            record->previous.w = record->position.w;
            if (gameWork.control & GAME_PAUSE_MAP_OBJECTS) {
                break;
            }
            if (record->moveStep != -1) {
                if (record->moveStepCount == 0) {
                    Get_MiddlePoint(&record->moveFrom, &record->moveTo, record->moveStep, record->moveCount,
                                    &record->position);
                } else {
                    Get_MiddlePoint_Curve(&record->moveFrom, &record->moveTo, record->moveStep, record->moveCount,
                                  record->moveX, record->moveY, record->moveStepCount,
                                  &record->position);
                }
                record->moveStep++;
                if (record->moveStep > record->moveCount) {
                    record->moveStep = -1;
                }
            }
            if (record->turnStep != -1) {
                if (record->turnStepCount == 0) {
                    Get_MiddlePoint(&record->turnFrom, &record->turnTo, record->turnStep, record->turnCount,
                                    &record->rotation);
                } else {
                    Get_MiddlePoint_Curve(&record->turnFrom, &record->turnTo, record->turnStep, record->turnCount,
                                  record->turnX, record->turnY, record->turnStepCount,
                                  &record->rotation);
                }
                record->turnStep++;
                if (record->turnStep > record->turnCount) {
                    record->turnStep = -1;
                }
            }
            if (record->scaleStep != -1) {
                record->scale.x = record->scaleFrom.x
                                  + (record->scaleTo.x - record->scaleFrom.x) * record->scaleStep / record->scaleCount;
                record->scale.y = record->scaleFrom.y
                                  + (record->scaleTo.y - record->scaleFrom.y) * record->scaleStep / record->scaleCount;
                record->scale.z = record->scaleFrom.z
                                  + (record->scaleTo.z - record->scaleFrom.z) * record->scaleStep / record->scaleCount;
                record->scaleStep++;
                if (record->scaleStep > record->scaleCount) {
                    record->scaleStep = -1;
                }
            }
            break;
        }
        if (record->body.actor != 0) {
            XenoActorPushMatrix(record->body.actor);
        }
    }
    if (!(gameWork.control & GAME_HIDE_RIDERS)) {
        for (j = 0; j < MAP_RECORD_COUNT; j++) {
            record = &uwamonoRecords[j];
            if (record->kind < MAP_KIND_ACTOR || record->kind > MAP_KIND_FLAG) {
                continue;
            }
            uwamonoUpdateInReach(uwamonoRecords[uwamonoCurrentIndex].body.actor, j);
            if (record->state != 0) {
                riders[0] = record->handles[1];
                riders[1] = record->handles[2];
                if (record->flags & 0x80000) {
                    riders[1]->shown = 0;
                    riders[0]->shown = 0;
                } else {
                    if (record->state == 1) {
                        riders[0]->shown = 1;
                        riders[1]->shown = 0;
                        riders[0]->position.vector.x = record->position.x + record->focusOffset.x;
                        riders[0]->position.vector.y = record->position.y + record->focusOffset.y + record->extent.y * 0.5f;
                        riders[0]->position.vector.z = record->position.z + record->focusOffset.z;
                        riders[0]->scale.vector.x = riders[0]->scale.vector.y = riders[0]->scale.vector.z
                            = uwamonoRecords[uwamonoCurrentIndex].form;
                    } else {
                        riders[0]->shown = 0;
                        riders[1]->shown = 1;
                        riders[1]->position.vector.x = record->position.x + record->focusOffset.x;
                        riders[1]->position.vector.y = record->position.y + record->focusOffset.y + record->extent.y * 0.5f;
                        riders[1]->position.vector.z = record->position.z + record->focusOffset.z;
                        riders[1]->scale.vector.x = riders[1]->scale.vector.y = riders[1]->scale.vector.z
                            = uwamonoRecords[uwamonoCurrentIndex].form;
                    }
                    riders[0]->flags |= 4;
                    riders[1]->flags |= 4;
                }
            }
            if (record->motionKind != 0 && record->searchState != 1 && record->busy == 0
                && uwamonoCurrentIndex != -1) {
                if (record->lookLink == -1) {
                    point = record->closedPosition.vector;
                } else {
                    Get_MiddlePoint(&record->closedPosition.vector, &uwamonoRecords[record->lookLink].closedPosition.vector, 1, 2, &point);
                }
                if (uwamonoRecords[uwamonoCurrentIndex].form * 5.0f
                    <= Get_Distance(&uwamonoRecords[uwamonoCurrentIndex].body.actor->position, &point)) {
                    uwamonoSetPairMode(j, 1);
                }
            }
        }
    }
    uwamonoUpdatePoints();
    uwamonoUpdateItemPickups();
    DispCollUwamono();
    return 1;
}


void mapObjectsDraw(void)
{
    if (mainObject == 0) {
        return;
    }
    if (gameWork.active != 0 && gameWork.active->pending != 0) {
        return;
    }
    if (gameWork.nextMap != -1) {
        return;
    }
    if (gameWork.fogDist.x != 0.0f && gameWork.fogDist.y != 0.0f) {
        nmlModelSetFogDist(gameWork.fogDist.x, gameWork.fogDist.y, gameWork.fogDist.z, gameWork.fogDist.w);
        nmlModelSetFogCol(&gameWork.fogColor);
    }
    pxySetHideMask(&mapMainHideMask);
    if (mapTextureData != 0) {
        nmlModelSetTexture((TxyBlock *) mapTextureData); /* mapTextureData is an int holding the txy block address */
    }
    pxySetColorScale(&gameWork.mapColor);
    pxySubmitModel(mainObject);
    if (mainObject->subPackOffset != 0 && mapObjectTable != 0) {
        nmlModelSetParent(mainObject);
        pxySetHideMask(&mapSecondHideMask);
        pxySetSecondPartMask(mapSecondPartMask);
        nmlModelSetTexture((TxyBlock *) mapTextureData); /* mapTextureData is an int holding the txy block address */
        pxySetColorScale(&gameWork.mapColor);
        pxySubmitModel(mapObjectTable);
    }
}

StripPacket mapBackdropStripPacket[1] = {
    {
        .unmodeled_00 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 80, 1, 128, 0, 0, 0, 64, 139, 128, 238, 22, 83, 83, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 63, 0, 0, 0, 0, 0, 0, 0 },
        .tag = 329737LL,
        .unmodeled_38 = { 71, 0, 0, 0, 0, 0, 0, 0, 0, 56, 4, 40, 6, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0, 0, 128, 0, 0, 0, 128, 0, 0, 0, 128, 0, 0, 0 },
        .topY = 0,
        .unmodeled_64 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 248, 111, 0, 0 },
        .topV = 29175,
        .topU = 0,
        .unmodeled_7c = { 0, 0, 0, 0 },
        .bottomY = 8192,
        .unmodeled_84 = { 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 248, 143, 0, 0 },
        .bottomV = 36343,
        .bottomU = 0,
    },
};

u8 mapBackdropImagePacket[112] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 81, 4, 0, 0, 0, 0, 0, 0, 16, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 56, 16, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 112, 0, 0, 0, 82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 83, 0, 0, 0, 0, 0, 0, 0, 0, 112, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0 };

Vector4 mapBackdropOriginVector = { 0.0f, 0.0f, 0.0f, 1.0f };

float negativeZeroFloat[1] = { -0.0f };

int uwamonoSearchWaitCount = -1;

/* The map resource table (one entry per name of mapObjectResourceNames), zero until mapObjectResourcesLoad fills it. */
void *mapObjectResources[MAP_RESOURCE_COUNT] = { 0 };

/* The data that follows the table up to 0x2B7CF0 (.data, address order). The names and types are those of the
 * files that read them; this file reads only mapSecondPartMask. */
MapBits mapMainHideMask = { { 0 } };

MapBits mapSecondHideMask = { { 0 } };

int mapSecondPartMask[10] = { 0 };
