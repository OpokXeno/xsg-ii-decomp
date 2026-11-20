#include "common.h"
#include "main/ladder_table.h"
#include "main/enemy_1.h"
#include "main/set_motion.h"
#include "main/game_camera.h"
#include "main/battle_result.h"
#include "main/save_screen.h"
#include "main/xgl_render.h"
#include "shared.h"
#include "debug_break.h"
#include "main/pad.h"
#include "main/xgl_camera.h"
#include "main/game_data.h"
#include "main/game_map_object.h"
#include "main/pt_member.h"
#include "main/msg_state.h"
#include "main/ysegf.h"
#include "main/sef.h"
#include "main/xenoActor.h"
#include "main/ytachi_test.h"
#include "main/undulate.h"
#include "ov01/sb_vm.h"
#include "ov01/scene.h"
#include "main/vibration.h"
#include "main/xgl_thread.h"
#include "main/control_entry.h"
#include "main/xgl_font.h"
#include "main/font_obj.h"
#include "main/gifState.h"
#include "main/fade_ctrl.h"
#include "main/game_disk_change.h"
#include "main/game_over_059.h"
#include "ov01/tgt_select.h"
#include "ov01/choice_menu.h"
#include "main/game_pause_disp.h"
#include "main/runtime_menu.h"
#include "main/menu_scene.h"
#include "main/TiGlobalColl.h"
#include "main/uwamono_record.h"
#include "main/uwamono_objx.h"
#include "main/pxy_profile.h"
#include "main/pxy_packet_state.h"
#include "main/nml_model_set.h"
#include "main/xgl_studio.h"
#include "main/get.h"
#include "main/face_point.h"
#include "main/enemy_system_init.h"
#include "main/uwamono_event_check.h"
#include "main/se_bank_load.h"
#include "main/xgl_cd_arena.h"
#include "main/sound_driver.h"
#include "main/xgl_cd.h"
#include "main/res_get_path.h"


extern unsigned char mapSceneScriptBuffer[];
extern unsigned char elvActiveMask;
extern int elvKeepOffFlags[32];


/* Game_Data_Pop restores the two color blocks of an actor with stores of alias set 0, which keep the loads of the saved
 * record fields behind them. struct XenoActor declares them as plain members, so this one site reaches them through a
 * view of the actor with one-member unions. */
typedef struct ActorColorView {
    unsigned char unmodeled_000[0x490];
    union {
        DoublewordAlignedVector4 color;     /* +0x490 */
    };
    union {
        DoublewordAlignedVector4 colorBlock; /* +0x4A0 */
    };
} ActorColorView;

/* The field loop writes the player's actor vectors and draw flags with stores of alias set 0. struct XenoActor keeps the
 * vectors as plain members, which would schedule these sites differently, so they reach the actor through this view. */
typedef struct FieldActorView {
    unsigned char unmodeled_000[0x10];
    union {
        AlignedVector4 position;            /* +0x010 */
    };
    unsigned char unmodeled_020[0x30];
    union {
        AlignedVector4 rotation;            /* +0x050: y is the facing angle */
    };
    union {
        AlignedVector4 direction;           /* +0x060: y is the moving angle */
    };
    unsigned char unmodeled_070[0x4C];
    int drawFlags;                          /* +0x0BC: 0x4 hides the actor */
} FieldActorView;

#define CURRENT_ACTOR ((FieldActorView *) uwamonoRecords[uwamonoCurrentIndex].body.actor)

/* Saves the scene before the actor pool is rebuilt: the camera, the map flags, the light sets, the elevator
 * masks and each record's actor state; then rebuilds the pool and reloads the current record. */
void Game_Data_Push(void)
{
    int i;
    int set;
    int column;
    int row;
    int bit;
    int mask;
    UwamonoRecord *record;
    XenoActor *actor;
    MapBits *savedBits;

    GameCameraStateSave();
    savedBits = gameWork.savedMapBits;
    savedBits[0] = mapMainHideMask;
    savedBits[1] = mapSecondHideMask;
    for (set = 0; set < 16; set++) {
        for (column = 0; column < 4; column++) {
            for (row = 0; row < 3; row++) {
                gameWork.savedLights.direction[set][row][column] = gameWork.lights.direction[set][row][column];
                gameWork.savedLights.color[set][row][column] = gameWork.lights.color[set][row][column];
            }
            gameWork.savedLights.ambient[set][column] = gameWork.lights.ambient[set][column];
            gameWork.savedLights.type4Direction[set][column] = gameWork.lights.type4Direction[set][column];
        }
        gameWork.savedLights.type4Value[set] = gameWork.lights.type4Value[set];
    }
    gameWork.elevatorMask = elvActiveMask;
    mask = 0;
    for (bit = 0; bit < 32; bit++) {
        if (elvKeepOffFlags[bit]) {
            mask |= 1 << bit;
        }
    }
    gameWork.keepOffMask = mask;
    for (i = 0; i < 256; i++) {
        record = &uwamonoRecords[i];
        actor = record->body.actor;
        if (actor) {
            record->savedProbe = actor->collision;
            record->savedLight = actor->light;
            record->savedColor = actor->color;
            record->savedColorBlock = actor->colorBlock;
            record->savedFog[0] = actor->fogA;
            record->savedFog[1] = actor->fogB;
            record->savedFog[2] = actor->fogC;
            record->savedFog[3] = actor->fogD;
            record->savedDrawMode = actor->drawMode;
            record->savedDrawFlags = actor->drawFlags;
            record->savedDrawParam = actor->drawParam;
            record->savedModeValue4 = actor->modeValue4;
            record->savedFlags = actor->flags;
            record->savedFrame = actor->frame;
            record->savedMode = actor->mode;
            record->savedSlot = actor->slot;
            record->savedRequestedMode = actor->requested_mode;
            record->savedMotionSpeed = actor->motion[0].speed;
            record->savedMotionRange[0] = actor->motion[0].range[0];
            record->savedMotionRange[1] = actor->motion[0].range[1];
        }
    }
    XenoActorPoolInitDefault();
    uwamonoLoadCurrent();
    sbThreadsSuspend((struct SbImageHeader *)mapSceneScriptBuffer);
    sefPushEffect();
    sefDestroyEffectCf();
    fontObjClearSlots();
}

void uwamonoLoadCurrent(void)
{
    gameWork.gravitySaved = gameWork.gravity;
    gameWork.bounceDivisorSaved = gameWork.bounceDivisor;
    gameWork.frameScaleSaved = gameWork.frameScale;
    gameWork.playerParams[0][1] = gameWork.playerParams[0][0];
    gameWork.playerParams[1][1] = gameWork.playerParams[1][0];
    gameWork.playerParams[2][1] = gameWork.playerParams[2][0];
    gameWork.playerParams[3][1] = gameWork.playerParams[3][0];
    gameWork.playerParams[4][1] = gameWork.playerParams[4][0];
    gameWork.climbFramesSaved = gameWork.climbFrames;
    gameWork.formScale = uwamonoRecords[uwamonoCurrentIndex].scale.x;
    gameWork.formSize = uwamonoRecords[uwamonoCurrentIndex].extent.x;
}
/* The resource block an actor's resource id loads (cfePlugIn.c's CfeResourceBlock). */
typedef struct FieldResourceBlock {
    int pack;
    int effect;
    int hierarchy;
    int lookLink;
    int slotTable;
} FieldResourceBlock;



/* Puts back what Game_Data_Push saved after a battle: the map flags, the light sets, the elevator masks and the
 * player rates; then gives every record that had an actor a new one, reloaded from its resources and restored
 * from the record's saved state, and places the save-point models again. */
void Game_Data_Pop(void)
{
    void *data;
    FieldResourceBlock *resource;
    UwamonoState *motions;
    UwamonoRecord *record;
    XenoActor *actor;
    UwamonoPose *entry;
    MapCollisionObject *collision;
    int i;
    int set;
    int j;
    int k;
    int bit;
    int flag;

    data = 0;
    WorkEnd = gameWork.workEnd;
    mapObjectResourcesLoad();
    mapResourceSwitch(gameWork.resourceId, 0);
    mapMainHideMask = gameWork.savedMapBits[0];
    mapSecondHideMask = gameWork.savedMapBits[1];
    for (set = 0; set < 16; set++) {
        for (j = 0; j < 4; j++) {
            for (k = 0; k < 3; k++) {
                gameWork.lights.direction[set][k][j] = gameWork.savedLights.direction[set][k][j];
                gameWork.lights.color[set][k][j] = gameWork.savedLights.color[set][k][j];
            }
            gameWork.lights.ambient[set][j] = gameWork.savedLights.ambient[set][j];
            gameWork.lights.type4Direction[set][j] = gameWork.savedLights.type4Direction[set][j];
        }
        gameWork.lights.type4Value[set] = gameWork.savedLights.type4Value[set];
    }
    elvActiveMask = gameWork.elevatorMask;
    for (bit = 0; bit < 32; bit++) {
        flag = 1 << bit;
        if (!(gameWork.keepOffMask & flag)) {
            elvKeepOffFlags[bit] = 0;
        }
    }
    gameWork.gravity = gameWork.gravitySaved;
    gameWork.bounceDivisor = gameWork.bounceDivisorSaved;
    sefInitEffectCf();
    sefPopEffect();
    XenoActorPoolInitDefault();
    for (i = 0; i < 256; i++) {
        record = &uwamonoRecords[i];
        if (record->body.actor) {
            do {
            } while (GameResLoadID(record->key, &data, 0, 0) == -1);
            resource = data;
            actor = XenoActorAlloc();
            motions = record->body.motions;
            record->body.actor = actor;
            do {
            } while (GameResLoadID(0x30041, &data, 0, 0) == -1);
            actor->rsrc[XAct_Rsrc_MDL] = resource->pack;
            actor->rsrc[XAct_Rsrc_TEX] = resource->effect;
            actor->rsrc[XAct_Rsrc_XHR] = resource->hierarchy;
            actor->rsrc[XAct_Rsrc_XAP] = resource->slotTable;
            actor->rsrc[XAct_Rsrc_EntryTableC] = resource->lookLink;
            actor->rsrc[XAct_Rsrc_SlotTableB] = (int) data;
            if (motions && motions->resourceId != -1) {
                GameResLoadID(motions->resourceId, &data, 0, 0);
                actor->rsrc[XAct_Rsrc_5] = (int) data;
            }
            XenoActorAllocMatrices(actor);
            XenoActorSetChannelValue(actor, 0.0f);
            XenoActorSetMode(actor, 0);
            actor->position = record->position;
            if (uwamonoCurrentIndex == i) {
                actor->direction.y = actor->rotation.y = record->rotation.y;
            } else {
                actor->rotation.y = actor->direction.y = record->rotation.y;
            }
            actor->kind = record->key;
            actor->moved = i;
            actor->collision.radius = actor->radius = record->extent.x;
            actor->height = record->extent.y;
            collision = &actor->collision;
            *collision = record->savedProbe;
            if (motions) {
                actor->collision.block = UnduDataGetHeader(256, motions->groundId);
            }
            for (j = 0; j < 3; j++) {
                actor->light.directions[j][0] = record->followA[j].component[0];
                actor->light.directions[j][1] = record->followA[j].component[1];
                actor->light.directions[j][2] = record->followA[j].component[2];
                actor->light.directions[j][3] = record->followA[j].component[3];
                actor->light.colors[j][0] = record->followB[j].component[0];
                actor->light.colors[j][1] = record->followB[j].component[1];
                actor->light.colors[j][2] = record->followB[j].component[2];
                actor->light.colors[j][3] = record->followB[j].component[3];
            }
            actor->light.ambient[0] = record->followC.component[0];
            actor->light.ambient[1] = record->followC.component[1];
            actor->light.ambient[2] = record->followC.component[2];
            actor->light.ambient[3] = record->followC.component[3];
            actor->light.fourthDirection[0] = record->followD.component[0];
            actor->light.fourthDirection[1] = record->followD.component[1];
            actor->light.fourthDirection[2] = record->followD.component[2];
            actor->light.fourthDirection[3] = record->followD.component[3];
            actor->drawParam = record->blendEnd;
            actor->light = record->savedLight;
            ((ActorColorView *) actor)->color = record->savedColor;
            ((ActorColorView *) actor)->colorBlock = record->savedColorBlock;
            actor->fogA = record->savedFog[0];
            actor->fogB = record->savedFog[1];
            actor->fogC = record->savedFog[2];
            actor->fogD = record->savedFog[3];
            actor->drawMode = record->savedDrawMode;
            actor->drawFlags = record->savedDrawFlags;
            actor->drawParam = record->savedDrawParam;
            actor->modeValue4 = record->savedModeValue4;
            if (record->releaseNextIndex >= 0) {
                sefSetRecordOwner(uwamonoRecords[record->releaseNextIndex].handles[0], record->body.actor);
            }
            XenoActorSetModeInSlot(actor, record->savedSlot, record->savedMode);
            actor->flags = record->savedFlags;
            actor->frame = record->savedFrame;
            actor->slot = record->savedSlot;
            actor->requested_mode = record->savedRequestedMode;
            actor->motion[0].speed = record->savedMotionSpeed;
            actor->motion[0].range[0] = record->savedMotionRange[0];
            actor->motion[0].range[1] = record->savedMotionRange[1];
            actor->flags |= 0x200;
        }
        if (record->kind == 2) {
            if (record->type == -1) {
                if (record->key != -1) {
                    entry = (UwamonoPose *) ((unsigned char *) mapObjectTable + mapObjectTable->entryMatrixOffset) + record->key;
                    record->savedMatrix.copy = entry->copy;
                    record->transform = entry;
                    record->pose.copy = entry->copy;
                } else {
                    record->transform = &record->savedMatrix;
                }
            } else {
                record->transform = &record->savedMatrix;
            }
            if (record->releaseNextIndex >= 0) {
                sefSetRecordOwnerMarked(uwamonoRecords[record->releaseNextIndex].handles[0], record);
            }
        }
    }
    XenoActorSetHoldFlag();
    XenoActorClearHoldFlag();
    uwamonoAttachPlayerParts();
    uwamonoRecords[uwamonoCurrentIndex].body.actor->collision.flags |= 0x1000;
    uwamonoStoreCurrent();
    GameCameraStateRestore();
    gameWork.camera.flags |= 0x20;
    sbThreadsResume((struct SbImageHeader *)mapSceneScriptBuffer);
    sbResetNativeCounts();
    scriptRegisterModules();
}

/* Copies the form's saved player parameters back (the reverse of uwamonoLoadCurrent) and gives the
 * current record and its actor the form's scale and size. */
void uwamonoStoreCurrent(void)
{
    uwamonoRecords[uwamonoCurrentIndex].scaleCoords[0] = uwamonoRecords[uwamonoCurrentIndex].scaleCoords[1] = uwamonoRecords[uwamonoCurrentIndex].scaleCoords[2] = gameWork.formScale;
    gameWork.frameScale = gameWork.frameScaleSaved;
    gameWork.playerParams[0][0] = gameWork.playerParams[0][1];
    gameWork.playerParams[1][0] = gameWork.playerParams[1][1];
    gameWork.playerParams[2][0] = gameWork.playerParams[2][1];
    gameWork.playerParams[3][0] = gameWork.playerParams[3][1];
    gameWork.playerParams[4][0] = gameWork.playerParams[4][1];
    gameWork.climbFrames = gameWork.climbFramesSaved;
    uwamonoRecords[uwamonoCurrentIndex].body.actor->collision.radius = uwamonoRecords[uwamonoCurrentIndex].body.actor->radius = uwamonoRecords[uwamonoCurrentIndex].extent.x = gameWork.formSize;
}

#define NULL ((void *) 0)

extern int fadeUpdateHook[];
Vector4Elements restartPositionVector = { 0 };

/* Game calls soundSlotsRestart as a void function: with its int result the code around the call changes. */
typedef void VoidFunction(void);

/* Callees of Game (declared where their owners do not declare them for this TU). */
int printf(const char *format, ...);
void korgEvagSetFxVolume(int a, int b);
void sceGsSyncPath(int a, int b);


void Game(void)
{
    int i;
    int studio;
    int choice;
    int result;
    int found;
    int pressed;
    float pose;
    float zero;
    XglCamera *camera;
    UwamonoRecord *rec;
    XenoActor *actor;
    UwamonoEntry *entry;

    padApplySaveSettings();
    GameSynopsis();
    XenoActorPoolInitDefault();
    uwamonoResetRecords(-1, 0);
    gameWork.control &= ~0x100000;
    gameWork.encounterReaction = gameWork.entryKey = -1;
    gameWork.panelMode = 1;
    gameWork.fadeTimer = 0;
    gameRowFlagsClear();
    gameEventFlagsClear();
    zero = 0.0f;
    restartPositionVector.component[2] = zero;
    restartPositionVector.component[1] = zero;
    restartPositionVector.component[0] = zero;
    GameResourceInit((void *) 0x1000000, 0x1000000);
    seBankLoad(2, "CFS_SE", 0x7D000);
    sefInitEffect();
    sefInitEffectCf();
    sefStartEffectKeyDefault(0, 0x10000033);
    sefStartEffectKeyDefault(0, 0x10000035);
    sefStartEffectKeyDefault(0, 0x10000037);
    sefStartEffectKeyDefault(0, 0x10000021);
    sefStartEffectKeyDefault(0, 0x1000003F);
    sefStartEffectKeyDefault(0, 0x10000040);
    do {
    } while (xglCdServiceIfPending() != 0);
    xglRenderClearSkipOff();
    xglCullingMapInit();
    yg2PathResetAll();
    gifStateDisableEffect();
    ov01InitAll();
    if (gameWork.loadStage != 0) {
        saveReadImage();
        saveReadGameWork();
        gameWork.loadStage = 1;
    }
    gameWork.resourceId = -1;
    gameWork.workEnd = WorkEnd;
    uwamonoCurrentIndex = -1;
    gameWork.mapLoaded = 0;
    mapObjectResourcesLoad();
    tiGlobalCollClearHandle();
    msgResetAll();
    gameWork.resBuffer = GameResBufferAlloc(0x12000);
    resPlayerSlotsInit();
    resResetDescriptorTables();
    if (gameWork.loadStage == 0) {
        ptSwapSlots(7, ptFindSlot(13));
        ptEnterSplit();
    }
    ptLoadPartyModels();
    ptApplyCurrentModel();
    resSelectSlot(1);
    if (GameSynopsisCrossfadeWait() != 0) {
        return;
    }
    fadeReset();

    for (;;) {
        xglRenderClearDepth();
        ClearEnv.value.color_r = 0;
        ClearEnv.value.color_g = 0;
        ClearEnv.value.color_b = 0;
        ClearEnv.value.color_a = 128;
        xglRenderDispOn();
        xglStudioInit();
        for (studio = 0; studio < 4; studio++) {
            xglStudioChange(studio);
            xglStudioMainCameraInit();
            camera = xglStudioGetCamera2(0);
            camera->state = 2;
            xglCameraSetPadPort(camera, 0);
            for (i = 1; i < 8; i++) {
                camera = xglStudioGetCamera2(i);
                xglCameraInit(camera);
                xglCameraMove(camera);
                xglCameraSetPadPort(camera, 0);
                camera->state = 2;
            }
        }
        studioSceneFlagSet();
        xglRenderCpUDispOn();
        xglFontPrint(0, 0, 0, "$init;");
        gameWork.control &= ~0x401;
        gameWork.sceneFlag = 0;
        gameWork.fadeTimer = 0;
        mapSceneScriptLoad(gameWork.nextMap, 1);
        XenoActorPoolInitDefault();
        uwamonoResetRecords(-1, 0);
        if (uwamonoCurrentIndex == -1) {
            printf("player lost\n");
            DEBUG_BREAK();
        } else {
            ptLoadPartyModels();
            ptApplyCurrentModel();
        }
        GameCameraReset();
        UnlockMapUnit();
        Vibration_Stop();
        seSetMuted(1, 0);
        CURRENT_ACTOR->position.x = restartPositionVector.component[0];
        CURRENT_ACTOR->position.y = restartPositionVector.component[1];
        CURRENT_ACTOR->position.z = restartPositionVector.component[2];
        gameWork.encounterReaction = -1;
        CURRENT_ACTOR->drawFlags |= 4;
        gameWork.shadowCasterMode = 1;
        gameWork.mapColor.z = gameWork.mapColor.y = gameWork.mapColor.x = 1.0f;
        gameWork.control &= ~0x80000;
        gameWork.control &= ~0x800000;
        gameWork.control &= ~0x10000;
        gameWork.control &= ~0x20000000;
        gameWork.control &= ~0x200000;
        if (gameWork.loadStage != 0) {
            ptLoadPartyModels();
            ptApplyCurrentModel();
            gameWork.loadStage = 2;
        }
        menuSceneSetupHook();
        fadeStartIn(30);
        if (gameWork.control & 0x100000) {
            fadeClear();
        }
        while (1) {
            if ((PadData[0].heldAndPressed & 0x0800010F) == 0x0800010F) {
                gameWork.control |= 0x10000000;
                break;
            }
            pxySetMode3Flag(1);
            if (!(gameWork.control & 0x800000)) {
                pxySetShadowUploadFlag();
            }
            if (gameWork.control & 0x80000) {
                pxySetTexMapDropAll();
            }
            if (gameWork.control & 0x200000) {
                xglRenderClearSkipOn();
            } else {
                xglRenderClearSkipOff();
            }
            pxyRequestOverlayPass();
            pxySetShadowCasterMode(gameWork.shadowCasterMode);
            if (gameWork.control & 1) {
                pxySetFreezeAnimation();
            }
            if ((gameWork.control & 0x20) && gameWork.entryKey != -1) {
                if (gameWork.entryKey == -2) {
                    uwamonoRecords[uwamonoCurrentIndex].positionCoords[0] = CURRENT_ACTOR->position.x = ptState.progress->position[0].coords[0];
                    uwamonoRecords[uwamonoCurrentIndex].positionCoords[1] = CURRENT_ACTOR->position.y = ptState.progress->position[0].coords[1];
                    uwamonoRecords[uwamonoCurrentIndex].positionCoords[2] = CURRENT_ACTOR->position.z = ptState.progress->position[1].coords[0];
                    CURRENT_ACTOR->position.w = 1.0f;
                    uwamonoRecords[uwamonoCurrentIndex].rotationCoords[1] = CURRENT_ACTOR->rotation.y = CURRENT_ACTOR->direction.y = 0.0f;
                } else {
                    found = uwamonoFindEntry(gameWork.entryKey);
                    if (found != -1) {
                        uwamonoRecords[uwamonoCurrentIndex].positionCoords[0] = CURRENT_ACTOR->position.x = uwamonoEntries[found].x;
                        uwamonoRecords[uwamonoCurrentIndex].positionCoords[1] = CURRENT_ACTOR->position.y = uwamonoEntries[found].y;
                        uwamonoRecords[uwamonoCurrentIndex].positionCoords[2] = CURRENT_ACTOR->position.z = uwamonoEntries[found].z;
                        CURRENT_ACTOR->position.w = 1.0f;
                        pose = uwamonoGetEntryPose(gameWork.entryKey, NULL);
                        uwamonoRecords[uwamonoCurrentIndex].rotationCoords[1] = CURRENT_ACTOR->rotation.y = CURRENT_ACTOR->direction.y = pose;
                        CURRENT_ACTOR->drawFlags &= ~4;
                    } else {
                        CURRENT_ACTOR->position.x = CURRENT_ACTOR->position.y = CURRENT_ACTOR->position.z = 0.0f;
                        CURRENT_ACTOR->position.w = 1.0f;
                        uwamonoRecords[uwamonoCurrentIndex].rotationCoords[1] = CURRENT_ACTOR->rotation.y = CURRENT_ACTOR->direction.y = 0.0f;
                    }
                }
                gameWork.control &= ~0x400;
                uwamonoRecords[uwamonoCurrentIndex].body.actor->collision.flags |= 0x1000;
                gameWork.camera.flags |= 0x80;
            }
            if (gameWork.loadStage != 0) {
                int slot;

                for (slot = 0; slot < 256; slot++) {
                    if (uwamonoRecords[slot].kind == 2 && (unsigned int)(uwamonoRecords[slot].type - 7) < 2) {
                        uwamonoRecords[uwamonoCurrentIndex].positionCoords[0] = CURRENT_ACTOR->position.x = uwamonoRecords[slot].positionCoords[0];
                        uwamonoRecords[uwamonoCurrentIndex].positionCoords[1] = CURRENT_ACTOR->position.y = uwamonoRecords[slot].positionCoords[1];
                        uwamonoRecords[uwamonoCurrentIndex].positionCoords[2] = CURRENT_ACTOR->position.z = uwamonoRecords[slot].positionCoords[2];
                        CURRENT_ACTOR->position.w = 1.0f;
                        uwamonoRecords[uwamonoCurrentIndex].rotationCoords[1] = CURRENT_ACTOR->rotation.y = CURRENT_ACTOR->direction.y = 0.0f;
                        gameWork.loadStage--;
                        gameWork.control &= ~0x400;
                        uwamonoRecords[uwamonoCurrentIndex].body.actor->collision.flags |= 0x1000;
                        CURRENT_ACTOR->drawFlags &= ~4;
                        gameWork.camera.flags |= 0x80;
                        break;
                    }
                }
            }

            if (!(gameWork.control & 1)) {
                sbRunAll();
                if (gameWork.control & 0x10000000) {
                    break;
                }
                if (gameWork.control & 0x08000000) {
                    break;
                }
            }
            if (!(gameWork.control & 0x200)) {
                pxyProfileBegin("DispMapObject");
                DispMapObject();
                pxyProfileEnd("DispMapObject");
                uwamonoPlayerUpdate();
                cameraUpdateFrame();
                mapObjectsDraw();
            }
            if (!(gameWork.control & 1)) {
                if (!(gameWork.control & 0x200)) {
                    pxyProfileBegin("sefExecEffect");
                    sefExecEffect();
                    pxyProfileEnd("sefExecEffect");
                }
                VibrationEngine();
                if (!(gameWork.control & 0x200) && !(uwamonoSearchWaitCount > 0 || (gameWork.control & 0x20000000))) {
                    if (!(gameWork.control & 0x40008400) && uwamonoLadderState.state == 0 && gameWork.encounterReaction == -1) {
                        pressed = PadData[0].pressed;
                        if (pressed & 0x10) {
                            resResetScriptState();
                            GameCameraStateSave();
                            soundSetMuted(1);
                            menuSceneRunLoop();
                            soundSetMuted(0);
                            GameCameraStateRestore();
                        } else if (pressed & 0x20) {
                            choice = uwamonoGetNearbySearchType();
                            if ((unsigned int)(choice - 7) < 2) {
                                resResetScriptState();
                                GameCameraStateSave();
                                soundSetMuted(1);
                                MenuFileMain(0);
                                soundSetMuted(0);
                                GameCameraStateRestore();
                            }
                            if (choice == 9) {
                                menuSceneRunKeyCheck();
                            }
                        }
                    }
                    msgUpdateTimed();
                }
                if (gameWork.control & 0x10000000) {
                    break;
                }
                if (gameWork.panelMode == 0) {
                    gameWork.panelValue--;
                    if (gameWork.panelValue < 0) {
                        gameWork.panelValue = 0;
                        gameWork.panelMode = 1;
                        uwamonoSendEvent(7, 0, 0);
                    }
                } else {
                    msgPanelClear();
                }
                padDebugPrintDirection(12);
            } else {
                sefSetRenderHooks();
                pauseStart();
                xglClockAccumulate();
            }
            if (gameWork.panelMode == 0) {
                msgPanelSetTimecode(0, 0, gameWork.panelValue / 30, gameWork.panelValue % 30);
            } else {
                msgPanelClear();
            }
            msgClose();
            resJumpPointsUpdate();
            if (fadeUpdateHook[0] != 0 || gameWork.encounterReaction != -1 || (gameWork.control & 0x230)
                || gameWork.nextMap != -1 || uwamonoLadderState.state == 2 || uwamonoLadderState.state == 3
                || uwamonoLadderState.state == 7 || gameWork.inputDelay != 0 || xglRenderGlobalFadeGet() < 0) {
                if (gameWork.inputDelay != 0) {
                    gameWork.inputDelay--;
                }
            } else if (PadData[0].pressed & 0x800) {
                gameWork.inputDelay = 16;
                if (gameWork.control & 1) {
                    gameWork.control &= ~1;
                    seSetMuted(1, 0);
                    soundSlotSetPauseIfNotHeld(0, 0);
                    seVoiceStartDefault(0x2000011);
                } else {
                    xglClockSampleDayTime();
                    gameWork.control |= 1;
                    seVoiceStartDefault(0x2000011);
                    seSetMuted(1, 1);
                    soundSlotSetPauseIfNotHeld(0, 1);
                }
            }
            if (gameWork.nextMap != -1) {
                break;
            }
            if (!(gameWork.control & 0x200)) {
                if (gameWork.encounterReaction == -1) {
                    if (!(gameWork.control & 1)) {
                        pxyProfileBegin("XenoActorDoit");
                        XenoActorDoit();
                        pxyProfileEnd("XenoActorDoit");
                    }
                    pxyProfileBegin("XenoActorDisp");
                    XenoActorDisp();
                    pxyProfileEnd("XenoActorDisp");
                }
            }
            if (gameWork.encounterReaction == -1) {
                xglSleep();
                if (gameWork.encounterReaction == -1) {
                    continue;
                }
            }

            xglRenderClearDepth();
            xglSleep();
            resResetScriptState();
            soundRestart();
            korgEvagSetFxVolume(1, 0);
            Game_Data_Push();
            Vibration_Stop();
            gifStateClearScreenAndMessages();
            XenoActorPoolInitDefault();
            fadeClear();
            xglFontFlush();
            sceGsSyncPath(0, 0);
            xglRenderCopyDisp2Draw();
            ov01SceneEnter();
            ov01SceneLoop();
            ov01SceneEnd();
            gifStateDisableEffect();
            if (gameWork.control & 0x10000000) {
                ((VoidFunction *) soundSlotsRestart)();
                break;
            }
            result = battleRetCodeGet();
            if (result == 2) {
                ((VoidFunction *) soundSlotsRestart)();
                gameWork.control |= 0x88000000;
                break;
            }
            if (result != 5) {
                battleResultScene(0);
                fontObjClearSlots();
            }
            if (gameWork.control & 0x10000000) {
                break;
            }
            msgPanelShowDefault();
            ((VoidFunction *) soundSlotsRestart)();
            if (gameWork.mapId < 6300) {
                seStateRestoreField();
            } else {
                seStateClear();
            }
            uwamonoResetPoints();
            Game_Data_Pop();
            UnlockMapUnit();
            msgResetAll();
            gameWork.encounterReaction = -1;
            gameWork.fadeTimer = 90;
            gameWork.encounterFlags = 0;
            switch (battleRetCodeGet()) {
            case 1:
                uwamonoSendEvent(6, 0, uwamonoRecords[gameWork.encounterRecord].id);
                uwamonoReleaseChildren(gameWork.encounterRecord);
                break;
            case 2:
                uwamonoSendEvent(6, 1, uwamonoRecords[gameWork.encounterRecord].id);
                break;
            case 3:
                uwamonoSendEvent(6, 2, uwamonoRecords[gameWork.encounterRecord].id);
                break;
            case 4:
                uwamonoSendEvent(6, 3, uwamonoRecords[gameWork.encounterRecord].id);
                uwamonoReleaseChildren(gameWork.encounterRecord);
                break;
            case 5:
                uwamonoSendEvent(7, -1, -1);
                break;
            }
            gameWork.entryKey = -1;
            gameWork.releaseHandle = NULL;
            Enemy_After_Battle(-1);
            if (gameWork.panelMode == 0) {
                msgPanelOpen();
            }
            actor = uwamonoRecords[uwamonoCurrentIndex].body.actor;
            if (actor != NULL) {
                actor->drawFlags |= 4;
            }
            if (gameWork.releaseHandle != NULL) {
                sefDeleteEffectCf(gameWork.releaseHandle);
                gameWork.releaseHandle = NULL;
            }
        }
        gameWork.loadStage = 0;
        ((VoidFunction *) soundSlotsRestart)();
        resResetScriptState();
        if (gameWork.control & 0x18000000) {
            break;
        }
        sefDestroyEffectCf();
        msgStateReset();
        xglSleep();
        xglSleep();
        fadeClear();
        if (gameWork.nextMap == -1) {
            break;
        }
    }
    fadeReset();
    xglRenderGlobalFadeInit();
    tiGlobalCollClearHandle();
    soundReset();
    soundSlotSetPause(0, 0);
    soundSetDefaults();
    resSelectSlot(0);
    xglRenderCpUDispOff();
    XenoActorPoolInitDefault();
    sefInitEffectCf();
    uwamonoResetRecords(-1, 0);
    xglCullingMapInit();
    xglRenderClearSkipOff();
    xglSleep();
    xglSleep();
    if (gameWork.control & 0x08000000) {
        GameOver();
    }
}

/* The last three words of the jump table jtbl_006851D0 (the switch in Game emits its five entries). */
const int gameJumpTablePadding[3] = { 0, 0, 0 };

/* Path strings of the map resource table (16-byte rows in the original are 24/32-byte slots). */
const char mapItem010TxyName[24] = "matumoto\\Item010.txy";
const char mapItem010PxyName[24] = "matumoto\\Item010.pxy";
const char mapItem009TxyName[24] = "matumoto\\Item009.txy";
const char mapItem009PxyName[24] = "matumoto\\Item009.pxy";
const char mapItem008TxyName[24] = "matumoto\\Item008.txy";
const char mapItem008PxyName[24] = "matumoto\\Item008.pxy";
const char mapItem007TxyName[24] = "matumoto\\Item007.txy";
const char mapItem007PxyName[24] = "matumoto\\Item007.pxy";
const char mapItem006TxyName[24] = "matumoto\\Item006.txy";
const char mapItem006PxyName[24] = "matumoto\\Item006.pxy";
const char mapItem005TxyName[24] = "matumoto\\Item005.txy";
const char mapItem005PxyName[24] = "matumoto\\Item005.pxy";
const char mapItem004TxyName[24] = "matumoto\\Item004.txy";
const char mapItem004PxyName[24] = "matumoto\\Item004.pxy";
const char mapItem003TxyName[24] = "matumoto\\Item003.txy";
const char mapItem003PxyName[24] = "matumoto\\Item003.pxy";
const char mapItem103TxyName[24] = "matumoto\\Item103.txy";
const char mapItem103PxyName[24] = "matumoto\\Item103.pxy";
const char mapItem102TxyName[24] = "matumoto\\Item102.txy";
const char mapItem102PxyName[24] = "matumoto\\Item102.pxy";
const char mapItem101TxyName[24] = "matumoto\\Item101.txy";
const char mapItem101PxyName[24] = "matumoto\\Item101.pxy";
const char mapItem002TxyName[24] = "matumoto\\Item002.txy";
const char mapItem002PxyName[24] = "matumoto\\Item002.pxy";
const char mapItem001TxyName[24] = "matumoto\\Item001.txy";
const char mapItem001PxyName[24] = "matumoto\\Item001.pxy";
const char mapRetP2TxyName[24] = "matumoto\\Ret_P2.txy";
const char mapRetP2PxyName[24] = "matumoto\\Ret_P2.pxy";
const char mapRetPTxyName[24] = "matumoto\\Ret_P.txy";
const char mapRetPPxyName[24] = "matumoto\\Ret_P.pxy";
const char mapEvsPTxyName[24] = "matumoto\\Evs_P.txy";
const char mapEvsPPxyName[24] = "matumoto\\Evs_P.pxy";
const char mapSaveP2TxyName[24] = "matumoto\\Save_P2.txy";
const char mapSaveP2PxyName[24] = "matumoto\\Save_P2.pxy";
const char mapSavePTxyName[24] = "matumoto\\Save_P.txy";
const char mapSavePPxyName[24] = "matumoto\\Save_P.pxy";
const char mapTre101TxyName[24] = "matumoto\\Tre101.txy";
const char mapTre101PxyName[24] = "matumoto\\Tre101.pxy";
const char mapTre002TxyName[24] = "matumoto\\Tre002.txy";
const char mapTre002PxyName[24] = "matumoto\\Tre002.pxy";
const char mapTre001TxyName[24] = "matumoto\\Tre001.txy";
const char mapTre001PxyName[24] = "matumoto\\Tre001.pxy";
const char mapTra004TxyName[24] = "matumoto\\Tra004.txy";
const char mapTra004PxyName[24] = "matumoto\\Tra004.pxy";
const char mapTra003TxyName[24] = "matumoto\\Tra003.txy";
const char mapTra003PxyName[24] = "matumoto\\Tra003.pxy";
const char mapTra002TxyName[24] = "matumoto\\Tra002.txy";
const char mapTra002PxyName[24] = "matumoto\\Tra002.pxy";
const char mapTra001TxyName[24] = "matumoto\\Tra001.txy";
const char mapTra001PxyName[24] = "matumoto\\Tra001.pxy";
const char mapSplineDataName[24] = "matumoto\\spline.dat";
const char mapEnemyOrderDataName[24] = "matumoto\\enemyorder.dat";
const char mapTreasureBoxDataName[32] = "matumoto\\treasurebox.dat";
const char mapKowaremonoDataName[32] = "matumoto\\kowaremono.dat";

/* .sdata cells 0x0069B960..0x0069B974, in address order: other TUs use them; this TU reads none of them. */
PxyHeader *mainObject = 0;
int mapTextureData = 0;
PxyHeader *mapObjectTable = 0;
struct MapFixupEntry *uwamonoEntryCountPtr = 0;
UwamonoEntry *uwamonoEntries = 0;
/* The game work block (0x1B28 bytes): the scene, player, camera and module state the field loop and the
 * other modules share. */
GameWork gameWork = { 0 };
