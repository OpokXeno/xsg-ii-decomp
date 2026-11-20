#include "common.h"
#include "main/enemy_1.h"
#include "shared.h"
#include "main/elevator.h"
#include "main/gifState.h"
#include "main/runtime_enepc.h"
#include "main/uwamono_record_init.h"
#include "main/talk_natives.h"
#include "main/xenoActor.h"
#include "main/sef.h"
#include "main/game_data.h"
#include "main/ytachi_test.h"
#include "main/uwamono_objx.h"
#include "main/runtime.h"
#include "main/uwamono_record.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "ov01/data_record_get.h"
#include "main/nml_model_set.h"
#include "main/init_uwamono_sys.h"
#include "main/res_get_path.h"
#include "main/srs_file.h"
#include "ov01/data_box.h"
#include "ov01/calc.h"
#include "main/msg_state.h"
#include "main/text_box.h"
#include "main/menu_selection.h"
#include "main/get_ladder.h"
#include "main/game_map_object.h"
#include "main/xgl_flags.h"
#include "main/game_camera.h"
#include "main/fade_ctrl.h"

#include "main/xgl_2.h"
#include "main/sound_driver.h"
#include "main/game_movie.h"
#include "main/se_bank_load.h"
#include "main/vibration.h"

/* A bit of gameWork.control; game_camera.h cannot be included here, its record types clash with this file's. */
#define CONTROL_SCRIPTED 0x8000 /* include/main/game_data.h */

/* uwamonoCreateRecord stores the motion halfword of an actor record with alias set 0 (a one-member union), which orders
 * it behind the position stores before it. UwamonoRecord.motion is a plain member, so this one site reaches it through
 * a view of the record. */
typedef struct ObjxRecordView {
    unsigned char unmodeled_000[0x33C];
    union {
        short value;
    } formSet;                              /* +0x33C */
} ObjxRecordView;

/* Native "setNumber": key, [kind], [entry], [fourth], [fifth]: creates the record of key (an omitted
 * kind releases the record instead). A string entry names a table entry, or an effect for MAP_KIND_EFFECT. */
int runtimeSetNumber(SbThread *thread)
{
    int key;
    int kind;
    int entry;
    int fourth;

    if (VM_PLUGIN_CNT_GET(thread) < 3) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    if (vmArgTagIsZero(thread, 2) != 0) {
        uwamonoReleaseRecord(uwamonoFindIndexByShortKey((short) key));
        return 1;
    }
    kind = vmArgGet(thread, 2);
    if (vmArgTagIsZero(thread, 3) != 0) {
        entry = -1;
    } else if (vmArgGetHigh(thread, 3) == 3) {
        if (kind != MAP_KIND_EFFECT) {
            entry = pxyFindEntryIndex(mapObjectTable, vmKeyedEntryGet(thread, vmArgGet(thread, 3)));
            if (entry == -1) {
                return 0;
            }
        } else {
            entry = srsEffectNameToID(vmKeyedEntryGet(thread, vmArgGet(thread, 3)));
        }
    } else {
        entry = vmArgGet(thread, 3);
    }
    if (vmArgTagIsZero(thread, 4) != 0) {
        fourth = -1;
    } else {
        fourth = vmArgGet(thread, 4);
    }
    return uwamonoCreateRecord(key, kind, entry, fourth, vmArgTagIsZero(thread, 5) != 0 ? -1 : vmArgGet(thread, 5));
}


/* Creates the map object record for key, kind and entry in the first free slot and returns its index, or -1
 * when fourth names a key row that the gate bytes hide. The kind decides how the record is set up. */
int uwamonoCreateRecord(int key, int kind, int entry, int fourth, int fifth)
{
    UwamonoRecord *record;
    struct XenoActor *actor;
    UwamonoState *state;
    UwamonoPose *table;
    UsableItemRecord *hitAction;
    EsAccessoryRecord *item;
    TextRecord8 *record8;
    int *res = 0;
    int *resources;
    int index;
    int row = -1;
    int set;
    int arg;
    int i;
    UwamonoKeyRow *keyRows;
    Vector4 position;
    Vector4 rotation;

    if (kind == MAP_KIND_ACTOR && fourth != -1) {
        for (i = 0; i < 512; i++) {
            keyRows = uwamonoKeyRowTable;
            if (keyRows[i].key == fourth) {
                row = i;
                break;
            }
        }
        if (i == 512) {
            return -1;
        }
        set = gameWork.control & 0x800;
        if (gameWork.rowFlags[set != 0][row] == 1) {
            if (set == 0) {
                if (uwamonoKeyRowTable[row].gateIfClear == 0) {
                    return -1;
                }
            } else {
                if (uwamonoKeyRowTable[row].gateIfSet == 0) {
                    return -1;
                }
            }
        }
        entry = uwamonoKeyRowTable[row].value;
    }

    index = uwamonoCountLeadingInUse();
    record = &uwamonoRecords[index];
    record->id = (short) key;
    record->kind = kind;
    record->key = entry;

    switch (kind) {
    case MAP_KIND_ACTOR:
    case MAP_KIND_ACTOR_STATIC:
    case MAP_KIND_PART:
        GameResLoadID(entry, (void **) &res, 0, 0);
        resources = res;
        actor = XenoActorAlloc();
        record->body.actor = actor;
        actor->rsrc[XAct_Rsrc_MDL] = resources[0];
        actor->rsrc[XAct_Rsrc_TEX] = resources[1];
        actor->rsrc[XAct_Rsrc_XHR] = resources[2];
        actor->rsrc[XAct_Rsrc_XAP] = resources[4];
        actor->rsrc[XAct_Rsrc_EntryTableC] = resources[3];
        while (GameResLoadID(0x30041, (void **) &res, 0, 0) == -1) {
        }
        actor->rsrc[XAct_Rsrc_SlotTableB] = (int) res;
        XenoActorAllocMatrices(actor);
        XenoActorSetBlendFrames(actor, 0.0f);
        XenoActorSetMode(actor, 0);
        record->positionCoords[0] = actor->position.x;
        record->positionCoords[1] = actor->position.y;
        record->positionCoords[2] = actor->position.z;
        record->positionCoords[3] = actor->position.w;
        ((ObjxRecordView *) record)->formSet.value = 1;
        actor->kind = entry;
        actor->drawFlags |= 8;
        actor->moved = index;
        if (kind != MAP_KIND_PART) {
            record->body.motions = &uwamonoStates[uwamonoStateCountLeading()];
        }
        if (kind == MAP_KIND_ACTOR_STATIC) {
            record->body.motions->owner = index;
        }
        if (entry <= 0x6FFFF) {
            record->extent.x = actor->radius = 0.6f;
            record->extent.y = actor->height = 1.8f;
            record->searchMode = 0;
        } else {
            record->extent.x = actor->radius = 0.0f;
            record->searchMode = 2;
        }
        if ((unsigned int) (entry - 0x10000) < 0x4000) {
            record->form = 1.0f;
        } else {
            record->form = 5.0f;
        }
        if (kind == MAP_KIND_ACTOR) {
            state = record->body.motions;
            state->moveSpeed = 0.02f;
            state->pursuitSpeed = 0.04f;
            state->fanWidth = 1.0471976f;
            state->fanLength = 5.0f;
            state->owner = index;
            state->noticeRange = 0;
            state->threatTime = state->pauseTime = 20;
            state->threatCycle = state->pauseCycle = 4;
            state->area.x = -2.5f;
            state->area.z = 10.0f;
            state->areaSize = 3.0f;
            state->mode = 0;
            state->area.y = 0;
            state->row = row;
            state->flags = 0;
            if (row != -1) {
                keyRows = uwamonoKeyRowTable;
                for (i = 0; i < 8; i++) {
                    state->setNumbers[i] = keyRows[row].bytes[i];
                }
            }
            for (i = 15; i >= 0; i--) {
                state->walk[i] = i;
            }
        }
        record->flags &= ~0x60000;
        record->effectId = 0x10000035;
        XenoActorPushMatrix(actor);
        break;

    case MAP_KIND_MODEL_REQUEST:
        record->kind = MAP_KIND_MODEL;
        record->type = record->key;
        if (record->key == 12) {
            if (gameWork.eventFlags[fourth] == 1) {
                record->kind = MAP_KIND_FREE;
                record->type = -1;
                return index;
            }
            switch (uwamonoEventTable[fourth].type) {
            case 1:
                hitAction = usableItemRecordGet(uwamonoEventTable[fourth].id);
                arg = (uwamonoRecords[uwamonoCurrentIndex].form == 1.0f ? hitAction->mapModel[0] : hitAction->mapModel[1]) + 12;
                break;
            case 2:
                item = accessoryRecordGet(uwamonoEventTable[fourth].id);
                arg = (uwamonoRecords[uwamonoCurrentIndex].form == 1.0f ? item->mapModel[0] : item->mapModel[1]) + 12;
                break;
            case 3:
                dataBoxSecretKeyRecordGet(uwamonoEventTable[fourth].id);
                arg = 18;
                break;
            case 4:
                record8 = dataBoxKeyItemRecordGet(uwamonoEventTable[fourth].id);
                arg = (uwamonoRecords[uwamonoCurrentIndex].form == 1.0f ? record8->mapModel[0] : record8->mapModel[1]) + 12;
                break;
            default:
                arg = 12;
                break;
            }
            if (fifth != -1) {
                arg = fifth + 12;
            }
            record->eventRow = (short) fourth;
            record->type = arg;
        }
        record->key = -1;
        record->positionCoords[0] = record->positionCoords[1] = record->positionCoords[2] = 0.0f;
        record->positionCoords[3] = 1.0f;
        record->rotationCoords[0] = record->rotationCoords[1] = record->rotationCoords[2] = 0.0f;
        record->rotationCoords[3] = 1.0f;
        uwamonoInitRecord((short) index);
        record->transform = &record->savedMatrix;
        break;

    case MAP_KIND_MODEL:
        if (entry == -1) {
            rotation.x = rotation.y = rotation.z = position.x = position.y = position.z = 0.0f;
        } else {
            uwamonoObjectEntryPositionGet(&position, entry);
            uwamonoObjectEntryAnglesGet(&rotation, entry);
        }
        record->positionCoords[0] = position.x;
        record->positionCoords[1] = position.y;
        record->positionCoords[2] = position.z;
        record->positionCoords[3] = position.w;
        record->rotationCoords[0] = rotation.x;
        record->rotationCoords[1] = rotation.y;
        record->rotationCoords[2] = rotation.z;
        record->rotationCoords[3] = rotation.w;
        uwamonoInitRecord((short) index);
        if (entry != -1) {
            table = (UwamonoPose *) ((unsigned char *) mapObjectTable + mapObjectTable->entryMatrixOffset) + entry;
            record->savedMatrix.copy = table->copy;
            record->transform = table;
            record->pose.copy = table->copy;
        } else {
            record->transform = &record->savedMatrix;
        }
        break;

    case MAP_KIND_EFFECT:
        rotation.x = rotation.y = rotation.z = position.x = position.y = position.z = 0.0f;
        record->positionCoords[0] = position.x;
        record->positionCoords[1] = position.y;
        record->positionCoords[2] = position.z;
        record->positionCoords[3] = position.w;
        record->rotationCoords[0] = rotation.x;
        record->rotationCoords[1] = rotation.y;
        record->rotationCoords[2] = rotation.z;
        record->rotationCoords[3] = rotation.w;
        record->flags = 2;
        record->searchMode = 0;
        sefStartEffectKeyDefault(0, entry);
        record->handles[0] = sefCreateEffectCf(entry, &record->position, 0);
        break;

    case MAP_KIND_FLAG:
        record->positionCoords[0] = record->positionCoords[1] = record->positionCoords[2] = 0.0f;
        record->positionCoords[3] = 1.0f;
        break;
    }

    record->state = 0;
    return index;
}


/* Native "setLocater": id, x1, y1, z1, x2, y2, z2 (percent), type, angle1, angle2 (degrees): creates
 * two mover records (MAP_KIND_MOVER) at the two points, linked to each other, with ids id and id + 1. When
 * arguments 3, 4, 6 and 7 are omitted the points are the pose table rows of keys x1 and x2. */
int runtimeSetLocater(SbThread *thread)
{
    int index[2];
    int entry[2];
    UwamonoPathPoint position[2];
    int i;

    if (VM_PLUGIN_CNT_GET(thread) != 10) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    if (vmArgTagIsZero(thread, 3) != 0 && vmArgTagIsZero(thread, 4) != 0 && vmArgTagIsZero(thread, 6) != 0 && vmArgTagIsZero(thread, 7) != 0) {
        entry[0] = uwamonoFindEntry(vmArgGet(thread, 2));
        entry[1] = uwamonoFindEntry(vmArgGet(thread, 5));
        for (i = 0; i < 2; i++) {
            position[i].coords[0] = uwamonoEntries[entry[i]].x;
            position[i].coords[1] = uwamonoEntries[entry[i]].y;
            position[i].coords[2] = uwamonoEntries[entry[i]].z;
            position[i].coords[3] = 1.0f;
        }
    } else {
        for (i = 0; i < 2; i++) {
            position[i].coords[0] = (float) vmArgGet(thread, i * 3 + 2) / 100.0f;
            position[i].coords[1] = (float) vmArgGet(thread, i * 3 + 3) / 100.0f;
            position[i].coords[2] = (float) vmArgGet(thread, i * 3 + 4) / 100.0f;
            position[i].coords[3] = 1.0f;
        }
    }
    for (i = 0; i < 2; i++) {
        index[i] = uwamonoCountLeadingInUse();
        uwamonoRecords[index[i]].kind = MAP_KIND_MOVER;
        uwamonoRecords[index[i]].positionCoords[0] = position[i].coords[0];
        uwamonoRecords[index[i]].positionCoords[1] = position[i].coords[1];
        uwamonoRecords[index[i]].positionCoords[2] = position[i].coords[2];
        uwamonoRecords[index[i]].positionCoords[3] = position[i].coords[3];
        uwamonoRecords[index[i]].turnFromCoords[0] = (float) vmArgGet(thread, 9) / 100.0f / 180.0f * 3.1415927f;
        uwamonoRecords[index[i]].turnToCoords[0] = (float) vmArgGet(thread, 10) / 100.0f / 180.0f * 3.1415927f;
        uwamonoRecords[index[i]].extent.y = 1.0f;
    }
    uwamonoRecords[index[0]].partnerLink = index[1];
    uwamonoRecords[index[1]].partnerLink = index[0];
    uwamonoRecords[index[0]].id = vmArgGet(thread, 1);
    uwamonoRecords[index[1]].id = vmArgGet(thread, 1) + 1;
    uwamonoRecords[index[0]].searchShape = vmArgGet(thread, 8);
    uwamonoRecords[index[1]].searchShape = 10;
    return 1;
}

int runtimeSetLastDan(void)
{
    gifStateStartWhiteShake(uwamonoSlotTable[uwamonoCurrentIndex].body.actor);
    return 1;
}

/* Native "setPlayer": scripted, frameScale, radius, five movement parameters, climbFrames, rate,
 * scale, [child]. An omitted value keeps the current one; scripted 1 sets CONTROL_SCRIPTED and wakes
 * the current record's actor, another value clears it and stops the actor; a child key attaches that
 * record's actor to joint 0x4D, a negative key releases it. */
int runtimeSetPlayer(SbThread *thread)
{
    struct XenoActor *actor = uwamonoRecords[uwamonoCurrentIndex].body.actor;
    int childKey;

    if (VM_PLUGIN_CNT_GET(thread) < 5) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    if (vmArgTagIsZero(thread, 1) == 0) {
        if (vmArgGet(thread, 1) == 1) {
            gameWork.control |= CONTROL_SCRIPTED;
            XenoActorSetBlendFrames(actor, 4.0f);
            XenoActorSetBlendLocal(actor);
            XenoActorSetMode(actor, 0);
        } else {
            gameWork.control &= ~CONTROL_SCRIPTED;
            uwamonoRecords[uwamonoCurrentIndex].body.actor->collision.flags |= 0x1000;
            if (actor->mode < 3 || actor->mode > 5) {
                XenoActorSetBlendLocal(actor);
                uwamonoSetMotion(uwamonoCurrentIndex, 0, 1.0f);
            }
        }
    }
    gameWork.frameScale = vmArgTagIsZero(thread, 2) != 0 ? gameWork.frameScale : (float) vmArgGet(thread, 2) / 100.0f / 100000.0f;
    uwamonoRecords[uwamonoCurrentIndex].extent.x = uwamonoRecords[uwamonoCurrentIndex].body.actor->radius = uwamonoRecords[uwamonoCurrentIndex].body.actor->collision.radius
        = vmArgTagIsZero(thread, 3) != 0 ? uwamonoRecords[uwamonoCurrentIndex].extent.x : (float) vmArgGet(thread, 3) / 100.0f;
    gameWork.playerParams[0][0] = vmArgTagIsZero(thread, 4) != 0 ? gameWork.playerParams[0][0] : (float) vmArgGet(thread, 4) / 100.0f;
    gameWork.playerParams[1][0] = vmArgTagIsZero(thread, 5) != 0 ? gameWork.playerParams[1][0] : (float) vmArgGet(thread, 5) / 100.0f;
    gameWork.playerParams[2][0] = vmArgTagIsZero(thread, 6) != 0 ? gameWork.playerParams[2][0] : (float) vmArgGet(thread, 6) / 100.0f;
    gameWork.playerParams[3][0] = vmArgTagIsZero(thread, 7) != 0 ? gameWork.playerParams[3][0] : (float) vmArgGet(thread, 7) / 100.0f;
    gameWork.playerParams[4][0] = vmArgTagIsZero(thread, 8) != 0 ? gameWork.playerParams[4][0] : (float) vmArgGet(thread, 8) / 100.0f;
    gameWork.climbFrames = vmArgTagIsZero(thread, 9) != 0 ? gameWork.climbFrames : vmArgGet(thread, 9);
    gameWork.gravity = vmArgTagIsZero(thread, 10) != 0 ? gameWork.gravity : (float) vmArgGet(thread, 10) / 10000.0f;
    gameWork.bounceDivisor = vmArgTagIsZero(thread, 11) != 0 ? gameWork.bounceDivisor : (float) vmArgGet(thread, 11) / 100.0f;
    if (vmArgTagIsZero(thread, 12) == 0) {
        childKey = vmArgGet(thread, 12);
        if (childKey >= 0) {
            XenoActorAttachChildToJoint(uwamonoRecords[uwamonoCurrentIndex].body.actor, uwamonoRecords[uwamonoFindIndexByShortKey((short) childKey)].body.actor, 0x4D);
        } else {
            XenoActorReleaseChild(uwamonoRecords[uwamonoCurrentIndex].body.actor, uwamonoRecords[uwamonoFindIndexByShortKey((short) -childKey)].body.actor);
        }
    }
    return 1;
}


/* Debug pad state defined in src/main/uwamono_event_check.c. */
extern int padDebugSector;
extern int padDebugCount;

/* Natives used by runtimeItem (defined elsewhere) and the library compare helpers it calls. */
extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *dest, const char *src);
/* Text default and digit buffer: this file's own data at 0x0069D220 and the rodata run at 0x00689700. */
extern char runtimeItemEmptyText[];
extern char itemPickupMessageBuffer[];

/* runtimeItem writes its digits into this buffer, so the original type is a writable string literal; as a named const array it stays in .rodata and `char *digits` keeps one qualifier warning (a plain char array moves to .data). */
/* EUC-JP text "00" (two digits written in place) then the rest of the 32-byte buffer, zero-filled. */
/* JP: を00個手に入れた！
 * EN: (after the item name) x00 obtained! */
const char itemObtainedCountText[32] = "\xa4\xf2\x30\x30\xb8\xc4\xbc\xea\xa4\xcb\xc6\xfe\xa4\xec\xa4\xbf\xa1\xaa";

int runtimeItem(SbThread *thread)
{
    int kind;
    int target;
    int count;
    int flag;
    int steps;
    char *text;
    char *digits;
    char *dest;
    char buf[256];

    steps = 0;
    digits = itemObtainedCountText;
    text = runtimeItemEmptyText;
    if (VM_PLUGIN_CNT_GET(thread) < 3) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    kind = vmArgGet(thread, 1);
    target = vmArgGet(thread, 2);
    count = vmArgTagIsZero(thread, 3) == 0 ? vmArgGet(thread, 3) : 0;
    flag = vmArgTagIsZero(thread, 4) == 0 ? (signed char) vmArgGet(thread, 4) : 0;
    switch (kind) {
    case 1:
        text = hitActionTextGet(target, 0);
        if (count > 0) {
            for (steps = 0; steps < count; steps++) {
                dataBoxIncCategory1(target);
            }
        } else {
            for (steps = 0; steps < __builtin_fabs(count); steps++) {
                dataBoxDecCategory1(target);
            }
        }
        steps = dataBoxChkCategory1(target);
        break;
    case 2:
        text = itemTextGet(target, 0);
        if (count > 0) {
            for (steps = 0; steps < count; steps++) {
                dataBoxIncCategory2(target);
            }
        } else {
            for (steps = 0; steps < __builtin_fabs(count); steps++) {
                dataBoxDecCategory2(target);
            }
        }
        steps = dataBoxChkCategory2(target);
        break;
    case 3:
        text = dataBoxCategory4TextGet(target, 0);
        if (count > 0) {
            for (steps = 0; steps < count; steps++) {
                dataBoxIncCategory4(target);
            }
        } else {
            for (steps = 0; steps < __builtin_fabs(count); steps++) {
                dataBoxDecCategory4(target);
            }
        }
        steps = dataBoxChkCategory4(target);
        break;
    case 4:
        text = dataBoxCategory3TextGet(target, 0);
        if (count > 0) {
            for (steps = 0; steps < count; steps++) {
                dataBoxIncCategory3(target);
            }
        } else {
            for (steps = 0; steps < __builtin_fabs(count); steps++) {
                dataBoxDecCategory3(target);
            }
        }
        selectionKeyItemBitsFill();
        steps = dataBoxChkCategory3(target);
        break;
    case 5:
        slotPairSet(target);
        break;
    case 6:
        dataSkillLearnSet(3, target);
        break;
    case 7:
        battleSlotMarkSet(2, target);
        break;
    case 8:
        dataEtherLearnSet(target, 63);
        break;
    }
    if (flag == 1) {
        dest = itemPickupMessageBuffer;
        digits[2] = count / 10 + '0';
        digits[3] = count % 10 + '0';
        strcpy(buf, text);
        strcat(buf, digits);
        strcpy(dest, buf);
        cursorBufferDeactivateCurrent();
        cursorBufferPush(dest);
    }
    return steps;
}

/* Native "TimeAttack": argument 1 is the kind, argument 2 the value (-1 when omitted).  Kind 0 opens
   the message panel and stores the kind and the value; kind 1 clears the panel and stores the kind
   with value 0; kind 2 stores the kind; kind 3 stores 0 as the kind; kind 4 returns -1 when the stored
   kind is 1, else the stored value.  Kinds 0 to 3 return the value, -1 or the stored value as the
   case reads; any other kind returns -1. */
int runtimeTimeAttack(SbThread *thread)
{
    int kind;
    int value;
    int result;

    result = -1;
    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    kind = vmArgGet(thread, 1);
    if (vmArgTagIsZero(thread, 2) == 0) {
        value = vmArgGet(thread, 2);
    } else {
        value = -1;
    }
    switch (kind) {
    case 0:
        msgPanelOpen();
        gameWork.panelMode = kind;
        gameWork.panelValue = value;
        result = value;
        break;
    case 1:
        result = -1;
        msgPanelClear();
        gameWork.panelMode = kind;
        gameWork.panelValue = 0;
        break;
    case 2:
        gameWork.panelMode = kind;
        result = gameWork.panelValue;
        break;
    case 3:
        gameWork.panelMode = 0;
        result = gameWork.panelValue;
        break;
    case 4:
        if (gameWork.panelMode == 1) {
            result = -1;
        } else {
            result = gameWork.panelValue;
        }
        break;
    }
    return result;
}

/* Native "GetAnalogR": argument 1 selects the mode.  Mode 0 clears padDebugSector and
   padDebugCount and returns 0; mode 4 returns padDebugCount; any other mode returns -1. */
int runtimeGetAnalogR(SbThread *thread)
{
    int result;

    result = -1;
    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    switch (vmArgGet(thread, 1)) {
    case 0:
        padDebugSector = 0;
        padDebugCount = 0;
        result = 0;
        break;
    case 4:
        result = padDebugCount;
        break;
    }
    return result;
}

char runtimeItemEmptyText[8] = { 0 };
char runtimeNativeNameBgm[8] = "bgm";
char runtimeNativeNameNextBgm[8] = "nextBgm";
char runtimeNativeNameBgmFade[8] = "bgmFade";
char runtimeNativeNameBgmse[8] = "bgmse";
char runtimeNativeNameSeVol[8] = "seVol";
char runtimeNativeNameSePos[8] = "sePos";
char runtimeNativeNameSe[8] = "se";
char runtimeNativeNameAdxplay[8] = "adxplay";
char runtimeNativeNameAdxload[8] = "adxload";
char runtimeNativeNameAdx[8] = "adx";
char runtimeNativeNameMpeg2[8] = "mpeg2";
char runtimeNativeNameSystem[8] = "System";
char runtimeNativeNameSetItai[8] = "setItai";
char runtimeNativeNameSetFog[8] = "setFog";
char runtimeNativeNameGetFlag[8] = "getFlag";
char runtimeNativeNameSetFlag[8] = "setFlag";
char runtimeNativeNameFade[8] = "Fade";
char runtimeNativeNameJumpCF[8] = "jumpCF";
char runtimeNativeNameMessage[8] = "message";
char runtimeNativeNameItem[8] = "Item";
void uwamonoSetRoboMode(signed char id);

extern int uwamonoEventIds[];

/* A bit of XenoActor.drawFlags (defined in src/main/xenoActor.c, not in a header). */
#define XENO_DRAW_COLOR 0x1 /* apply the color block: set by setFog after it writes fogDist/fogColor */

#define NULL ((void *) 0)


/* Native "progEffect": argument 2 is the frame count.  Without argument 1 the first
   argument of sefProgressEffect is 0; with it, argument 2 is also read as a key
   and the effect handle (uwamonoRecords[...].handles[0]) of the record found by uwamonoFindIndexByShortKey supplies it.
   Original behaviour, kept as coded: the key is argument 2 (the frame count), not argument 1. */
int runtimeProgEffect(SbThread *call)
{
    int frames;
    int lookupKey;
    int lookupIndex;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    frames = vmArgGet(call, 2);
    if (vmArgTagIsZero(call, 1) != 0) {
        sefProgressEffect(0, (float) frames);
    } else {
        lookupKey = vmArgGet(call, 2);
        lookupIndex = uwamonoFindIndexByShortKey((short) lookupKey);
        sefProgressEffect(uwamonoRecords[lookupIndex].handles[0], (float) frames);
    }
    return 1;
}

int changeRobo(SbThread *call)
{
    int id;

    if (VM_PLUGIN_CNT_GET(call) != 1) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    id = vmArgGet(call, 1);
    uwamonoSetRoboMode(id);
    return 1;
}

/* Called by changeRobo with the robo id.  Works on uwamonoRecords[uwamonoCurrentIndex]:
   returns early when the record's form already matches the id (1.0 for 0, 5.0 for 1);
   otherwise it starts a player model change (event 1) with timer 30, with no state
   (eventState/eventStateArg -1), sets CONTROL_SCRIPTED and calls fadeStartOut(0). */
void uwamonoSetRoboMode(signed char id)
{
    UwamonoRecord *record = &uwamonoRecords[uwamonoCurrentIndex];

    if (record->form == 1.0f) {
        if (id == 0) {
            return;
        }
    }
    if (record->form == 5.0f) {
        if (id == 1) {
            return;
        }
    }
    record->eventState = -1;
    record->eventTimer = 30;
    gameWork.control |= CONTROL_SCRIPTED;
    record->event = 1;
    record->eventStateArg = -1;
    fadeStartOut(0);
}

/* Native "setElevator": argument 1 is a record key, arguments 2 and 3 are the
   elevator motion and its argument stored in that record. */
int runtimeSetElevator(SbThread *call)
{
    int key;
    int index;
    int elevatorMotion;
    int elevatorArg;

    if (VM_PLUGIN_CNT_GET(call) != 3) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    key = vmArgGet(call, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    elevatorMotion = vmArgGet(call, 2);
    elevatorArg = vmArgGet(call, 3);
    uwamonoRecords[index].elevatorMotion = elevatorMotion;
    uwamonoRecords[index].elevatorArg = elevatorArg;
    return 1;
}

/* Native "message": argument 1 is an index into uwamonoEventIds and argument 2 the int stored there. */
int runtimeMessage(SbThread *call)
{
    int index;
    int value;

    if (VM_PLUGIN_CNT_GET(call) != 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    index = vmArgGet(call, 1);
    value = vmArgGet(call, 2);
    uwamonoEventIds[index] = value;
    return 1;
}

/* Native "mapChange": argument 1 is the id that mapResourceSwitch makes the current one, with mode 1. */
int runtimeMapChange(SbThread *call)
{
    int id;

    if (VM_PLUGIN_CNT_GET(call) != 1) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    id = vmArgGet(call, 1);
    mapResourceSwitch(id, 1);
    return 1;
}

/* Native "Fade": arguments 1 to 4 are red, green, blue and alpha, argument 5 the frame count,
   in the order fadeStartColorAlpha takes them. */
int runtimeFade(SbThread *call)
{
    int frames;
    int red;
    int green;
    int blue;
    int alpha;

    if (VM_PLUGIN_CNT_GET(call) != 5) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    red = vmArgGet(call, 1);
    green = vmArgGet(call, 2);
    blue = vmArgGet(call, 3);
    alpha = vmArgGet(call, 4);
    frames = vmArgGet(call, 5);
    fadeStartColorAlpha(frames, (float) red, (float) green, (float) blue, (float) alpha);
    return 1;
}

/* Native "jumpCF": arguments 1 and 2 are the state and its argument (default -1) for a
   deferred state change, argument 3 a mode (default 1) and argument 4 the timer
   (default 30), passed on to uwamonoSetTransition.  Does nothing while a state change
   is already pending on the current record. */
int runtimeJumpCf(SbThread *call)
{
    UwamonoRecord *record = &uwamonoRecords[uwamonoCurrentIndex];
    int state;
    int stateArg;
    signed char mode;

    if (VM_PLUGIN_CNT_GET(call) < 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    if (record->eventTimer != -1 && record->event == 0) {
        return 1;
    }
    state = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) == 0) {
        stateArg = vmArgGet(call, 2);
    } else {
        stateArg = -1;
    }
    if (vmArgTagIsZero(call, 3) == 0) {
        mode = (signed char) vmArgGet(call, 3);
    } else {
        mode = 1;
    }
    uwamonoSetTransition(mode, state, stateArg,
        vmArgTagIsZero(call, 4) != 0 ? 30 : vmArgGet(call, 4));
    return 1;
}

/* Called by runtimeJumpCf (jumpCF) with a mode (default 1), a state and its argument
   and a timer (default 30) from the script.  Queues a deferred state change (event 0)
   on the current uwamono record: eventState, eventStateArg and eventTimer take the
   values.  Sets gameWork.control bit 0x10000 for mode 1; otherwise clears it and,
   while uwamonoLadderState.state is 0, starts motion 0 on the current record.  When
   gameWork.control bit 0x100000 is clear and timer is nonzero, fades out over timer
   frames.  When timer is zero the state change runs at once: the event becomes -1
   (none) and gameWork.nextMap and gameWork.entryKey take the record's eventState and eventStateArg. */
void uwamonoSetTransition(signed char mode, int state, int stateArg, int timer)
{
    UwamonoRecord *record = &uwamonoRecords[uwamonoCurrentIndex];

    gameWork.control |= 0x20000000 | CONTROL_SCRIPTED;
    record->eventState = state;
    record->eventStateArg = stateArg;
    record->eventTimer = timer;
    record->event = 0;

    if (mode == 1) {
        gameWork.control |= 0x10000;
    } else {
        gameWork.control &= ~0x10000;
        if (uwamonoLadderState.state == 0) {
            uwamonoSetMotion(uwamonoCurrentIndex, 0, 1.0f);
        }
    }

    if (!(gameWork.control & 0x100000)) {
        if (timer != 0) {
            fadeStartOut(timer);
        }
    }
    if (timer == 0) {
        record->event = -1;
        gameWork.nextMap = record->eventState;
        gameWork.entryKey = record->eventStateArg;
    }
}

/* Native "setFlag": argument 1 is the bit offset, argument 2 the bit count
   (1 when absent) and argument 3 the value, as the xglFlagsSet parameters. */
int runtimeSetFlag(SbThread *call)
{
    int bitCount;
    int bitOffset;
    int flagValue;

    if (VM_PLUGIN_CNT_GET(call) != 3) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    bitCount = 1;
    bitOffset = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) == 0) {
        bitCount = vmArgGet(call, 2);
    }
    flagValue = vmArgGet(call, 3);
    xglFlagsSet(bitOffset, bitCount, flagValue);
    return 1;
}

/* Native "getFlag": argument 1 is the bit offset, argument 2 the bit count
   (1 when absent), as the xglFlagsGet parameters.  Returns the flag value,
   not 1. */
int runtimeGetFlag(SbThread *call)
{
    int bitOffset;
    int bitCount;

    if (VM_PLUGIN_CNT_GET(call) != 2) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    bitOffset = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) != 0) {
        bitCount = 1;
    } else {
        bitCount = vmArgGet(call, 2);
    }
    return xglFlagsGet(bitOffset, bitCount);
}

/* Native "setFog": argument 1 is a uwamono key, -1 or -2 for every slot (-1 also stores
   the fog in gameWork); arguments 2-5 are the fog distances in hundredths (near, far,
   min, max for nmlModelSetFogDist) and 6-8 the fog color, written to each slot's actor,
   whose drawFlags gets XENO_DRAW_COLOR (0x1, xenoActor.c).  A single key sets no
   actor: the range ends at the found index itself (end = first). */
int runtimeSetMapFog(SbThread *call)
{
    int key;
    int first;
    int end;
    int index;
    /* Declared live before the loop and assigned after the slot test: the
       original loads the slot's actor into $2 for the test and copies it to
       $16 for the stores (one variable assigned in the test loads $16 directly). */
    struct XenoActor *actor = NULL;

    if (VM_PLUGIN_CNT_GET(call) != 8) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    key = vmArgGet(call, 1);
    if (key == -1) {
        gameWork.fogDist.x = (float) vmArgGet(call, 2) / 100.0f;
        gameWork.fogDist.y = (float) vmArgGet(call, 3) / 100.0f;
        gameWork.fogDist.z = (float) vmArgGet(call, 4) / 100.0f;
        gameWork.fogDist.w = (float) vmArgGet(call, 5) / 100.0f;
        gameWork.fogColor.x = (float) vmArgGet(call, 6);
        gameWork.fogColor.y = (float) vmArgGet(call, 7);
        gameWork.fogColor.z = (float) vmArgGet(call, 8);
    }
    if (key == -1 || key == -2) {
        first = 0;
        end = MAP_RECORD_COUNT;
    } else {
        /* The original ends the range at the found index itself (end = first, not
           first + 1 as in setItai), so a single key sets no slot's fog; kept as compiled. */
        first = uwamonoFindIndexByShortKey((short) key);
        end = first;
    }
    for (index = first; index < end; index++) {
        if (uwamonoRecords[index].body.actor != NULL) {
            actor = uwamonoRecords[index].body.actor;
            actor->fogDist.x = (float) vmArgGet(call, 2) / 100.0f;
            actor->fogDist.y = (float) vmArgGet(call, 3) / 100.0f;
            actor->fogDist.z = (float) vmArgGet(call, 4) / 100.0f;
            actor->fogDist.w = (float) vmArgGet(call, 5) / 100.0f;
            actor->fogColor.x = (float) vmArgGet(call, 6);
            actor->fogColor.y = (float) vmArgGet(call, 7);
            actor->fogColor.z = (float) vmArgGet(call, 8);
            actor->drawFlags |= XENO_DRAW_COLOR;
        }
    }
    return 1;
}

int setMapColor(SbThread *call)
{
    if (VM_PLUGIN_CNT_GET(call) != 3) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    gameWork.mapColor.x = (float) vmArgGet(call, 1) / 255.0f;
    gameWork.mapColor.y = (float) vmArgGet(call, 2) / 255.0f;
    gameWork.mapColor.z = (float) vmArgGet(call, 3) / 255.0f;
    return 1;
}

/* Native "setItai", up to 10 arguments: argument 2 (default 1) goes to gameWork.shadowCasterMode;
   argument 3 is a uwamono key (every record when absent or negative).  For each live
   record with an actor, arguments 4/5 set/clear drawFlags bits, 6/7 set/clear
   renderFlags bits, 8 is a percentage for modeValue4 and 9 fills the first free (0xFF)
   colorIndex entry.  Argument 10 sets or clears gameWork.control bit 0x200000. */
int runtimeSetItai(SbThread *call)
{
    int first;
    int end;
    int index;
    int entry;

    if (VM_PLUGIN_CNT_GET(call) > 10) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    gameWork.shadowCasterMode = vmArgTagIsZero(call, 2) != 0 ? 1 : vmArgGet(call, 2);
    first = 0;
    end = MAP_RECORD_COUNT;
    if (vmArgTagIsZero(call, 3) == 0 && vmArgGet(call, 3) >= 0) {
        first = uwamonoFindIndexByShortKey((short) vmArgGet(call, 3));
        end = first + 1;
    }
    for (index = first; index < end; index++) {
        if (uwamonoRecords[index].kind != MAP_KIND_FREE && uwamonoRecords[index].body.actor != NULL) {
            if (vmArgTagIsZero(call, 4) == 0) {
                uwamonoRecords[index].body.actor->drawFlags |= vmArgGet(call, 4);
            }
            if (vmArgTagIsZero(call, 5) == 0) {
                uwamonoRecords[index].body.actor->drawFlags &= ~vmArgGet(call, 5);
            }
            if (vmArgTagIsZero(call, 6) == 0) {
                uwamonoRecords[index].body.actor->renderFlags |= vmArgGet(call, 6);
            }
            if (vmArgTagIsZero(call, 7) == 0) {
                uwamonoRecords[index].body.actor->renderFlags &= ~vmArgGet(call, 7);
            }
            if (vmArgTagIsZero(call, 8) == 0) {
                uwamonoRecords[index].body.actor->modeValue4 = (float) vmArgGet(call, 8) / 100.0f;
            }
            if (vmArgTagIsZero(call, 9) == 0) {
                for (entry = 0; entry < 16; entry++) {
                    if (uwamonoRecords[index].body.actor->colorIndex[entry] == 0xFF) {
                        uwamonoRecords[index].body.actor->colorIndex[entry] = vmArgGet(call, 9);
                        break;
                    }
                }
            }
        }
    }
    if (vmArgTagIsZero(call, 10) == 0) {
        if (vmArgGet(call, 10) != 0) {
            gameWork.control |= 0x200000;
        } else {
            gameWork.control &= ~0x200000;
        }
    }
    return 1;
}

/* Native "setLight", 8 arguments: argument 1 selects one of 16 light sets in gameWork,
   argument 2 the light type (0 ambient color, 1-3 a direction and color, 4 a direction
   and a value), arguments 3-5 a color out of 256 and 6-8 a direction out of 100.  A
   lone argument 3 gives a gray light color; type 4 without 6-8 disables the direction
   (w 0) and without 3-5 takes the value 0.2. */
int runtimeSetMapLight(SbThread *call)
{
    Vector4 direction;
    Vector4 color;
    int light;
    int type;

    if (VM_PLUGIN_CNT_GET(call) != 8) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    light = vmArgGet(call, 1);
    type = vmArgGet(call, 2);
    color.x = (float) vmArgGet(call, 3) / 256.0f;
    color.y = (float) vmArgGet(call, 4) / 256.0f;
    color.z = (float) vmArgGet(call, 5) / 256.0f;
    direction.x = (float) vmArgGet(call, 6) / 100.0f;
    direction.y = (float) vmArgGet(call, 7) / 100.0f;
    direction.z = (float) vmArgGet(call, 8) / 100.0f;
    switch (type) {
        case 0:
            gameWork.lights.ambient[light][0] = color.x;
            gameWork.lights.ambient[light][1] = color.y;
            gameWork.lights.ambient[light][2] = color.z;
            break;
        case 1:
        case 2:
        case 3:
            xglVectorNormal(&direction, &direction);
            gameWork.lights.direction[light][type - 1][0] = direction.x;
            gameWork.lights.direction[light][type - 1][1] = direction.y;
            gameWork.lights.direction[light][type - 1][2] = direction.z;
            gameWork.lights.direction[light][type - 1][3] = 1.0f;
            if (vmArgTagIsZero(call, 3) == 0 && vmArgTagIsZero(call, 4) != 0 && vmArgTagIsZero(call, 5) != 0) {
                gameWork.lights.color[light][type - 1][0] = color.x;
                gameWork.lights.color[light][type - 1][1] = color.x;
                gameWork.lights.color[light][type - 1][2] = color.x;
                gameWork.lights.color[light][type - 1][3] = 1.0f;
            } else {
                gameWork.lights.color[light][type - 1][0] = color.x;
                gameWork.lights.color[light][type - 1][1] = color.y;
                gameWork.lights.color[light][type - 1][2] = color.z;
                gameWork.lights.color[light][type - 1][3] = 1.0f;
            }
            break;
        case 4:
            if (vmArgTagIsZero(call, 6) != 0 && vmArgTagIsZero(call, 7) != 0 && vmArgTagIsZero(call, 8) != 0) {
                gameWork.lights.type4Direction[light][3] = 0.0f;
            } else {
                xglVectorNormal(&direction, &direction);
                gameWork.lights.type4Direction[light][0] = direction.x;
                gameWork.lights.type4Direction[light][1] = direction.y;
                gameWork.lights.type4Direction[light][2] = direction.z;
                gameWork.lights.type4Direction[light][3] = 1.0f;
            }
            if (vmArgTagIsZero(call, 3) != 0 && vmArgTagIsZero(call, 4) != 0 && vmArgTagIsZero(call, 5) != 0) {
                gameWork.lights.type4Value[light] = 0.2f;
            } else {
                gameWork.lights.type4Value[light] = color.x;
            }
            break;
    }
    return 1;
}

/* Native "System": argument 1 selects one of several gameWork.control bits to set or
   clear; out of range (or missing) values are ignored.  Always returns 1 once the
   argument count check passes. */
int runtimeSystem(SbThread *call)
{
    int flagId;

    if (VM_PLUGIN_CNT_GET(call) <= 0) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    flagId = vmArgGet(call, 1);
    switch (flagId) {
        case 0:
            gameWork.control |= 0x08000000;
            break;
        case 1:
            gameWork.control |= 0x00080000;
            break;
        case 2:
            gameWork.control &= ~0x00080000;
            break;
        case 3:
            break;
        case 4:
            gameWork.control |= 0x00800000;
            break;
        case 5:
            gameWork.control |= 0x20000000;
            break;
        case 6:
            gameWork.control &= ~0x20000000;
            break;
        case 7:
            gameWork.control |= 0x40000000;
            break;
        case 8:
            gameWork.control &= ~0x40000000;
            break;
        case 9:
            gameWork.control &= ~0x00100000;
            break;
        case 10:
            gameWork.control |= 0x00100000;
            break;
    }
    return 1;
}

const char runtimeNativeNameSetMapColor[16] = "setMapColor";

const char runtimeNativeNameVibration[16] = "Vibration";

const char runtimeNativeNameBgmsePos[16] = "bgmsePos";

const char runtimeNativeNameSetLight[16] = "setLight";

const char runtimeNativeNameMapChange[16] = "mapChange";

const char runtimeNativeNameChangeRobo[16] = "changeRobo";

const char runtimeNativeNameProgEffect[16] = "progEffect";

const char runtimeNativeNameGetAnalogR[16] = "GetAnalogR";

const char runtimeNativeNameTimeAttack[16] = "TimeAttack";

const char runtimeNativeNameSetPlayer[16] = "setPlayer";

const char runtimeNativeNameSetLastDan[16] = "setLastDan";

const char runtimeNativeNameSetElevator[16] = "setElevator";

const char runtimeNativeNameSetLocater[16] = "setLocater";

const char runtimeNativeNameSetNumber[16] = "setNumber";


int mpeg2(SbThread *call) {
    int handle;
    char *result;

    handle = vmArgGet(call, 1);
    result = vmKeyedEntryGet(call, handle);
    GameMpeg2Play(result);
    return 1;
}

int adx(SbThread *call) {
    int handle;
    char *result;

    soundSlotSetVolume(0, -1, 0);
    handle = vmArgGet(call, 1);
    result = vmKeyedEntryGet(call, handle);
    adxSetSlotFile(0, result);
    return 1;
}

int adxLoad(SbThread *call) {
    int handle;
    char *fileName;

    soundSlotSetPause(0, 1);
    soundSlotSetVolume(0, -1, 0);
    handle = vmArgGet(call, 1);
    fileName = vmKeyedEntryGet(call, handle);
    adxSetSlotFile(0, fileName);
    return 1;
}

int adxPlay(void) {
    soundSlotSetPause(0, 0);
    return 1;
}
extern unsigned char seDefaultVolume[];

/* Native "se" (its name pointer is at 0x3e6f00).  Argument 1 is the voice; argument 2 the
   volume, read from seDefaultVolume (the byte seVol writes) when absent; argument 3 the pan, 0x40 when
   absent.  A volume above 0 calls seVoiceStart with the same three values sePos uses;
   otherwise seVoiceStop is called with the voice. */
int runtimeSe(SbThread *call) {
    int voice;
    int volume;
    int pan;

    voice = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) != 0) {
        volume = seDefaultVolume[0];
    } else {
        volume = vmArgGet(call, 2);
    }
    if (vmArgTagIsZero(call, 3) == 0) {
        pan = vmArgGet(call, 3);
    } else {
        pan = 0x40;
    }
    if (volume > 0) {
        seVoiceStart(voice, volume, pan);
    } else {
        seVoiceStop(voice);
    }
    return 1;
}

int seVol(SbThread *call) {
    seDefaultVolume[0] = (unsigned char) vmArgGet(call, 1);
    return 1;
}

int sePos(SbThread *call) {
    float position[3];
    int voice;
    unsigned int volumePan;

    voice = vmArgGet(call, 1);
    position[0] = I2F(vmArgGet(call, 2)) / 100.0f;
    position[1] = I2F(vmArgGet(call, 3)) / 100.0f;
    position[2] = I2F(vmArgGet(call, 4)) / 100.0f;
    volumePan = seVoicePanVolumeFromPosition(position);
    if (volumePan != 0) {
        seVoiceStart(voice, volumePan >> 8, volumePan & 0xFF);
    }
    return 1;
}

/* Native "bgmse": argument 1 is the voice; argument 2 the volume, -1 when absent; argument 3
   the pan, 0x40 when absent.  The three values go to seVoiceRecordSet. */
int bgmSe(SbThread *call) {
    int voice;
    int volume;
    int pan;

    voice = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) == 0) {
        volume = vmArgGet(call, 2);
    } else {
        volume = -1;
    }
    if (vmArgTagIsZero(call, 3) == 0) {
        pan = vmArgGet(call, 3);
    } else {
        pan = 0x40;
    }
    seVoiceRecordSet(voice, volume, pan);
    return 1;
}

int bgmSePos(SbThread *call) {
    float position[3];
    int voice;

    voice = vmArgGet(call, 1);
    position[0] = I2F(vmArgGet(call, 2)) / 100.0f;
    position[1] = I2F(vmArgGet(call, 3)) / 100.0f;
    position[2] = I2F(vmArgGet(call, 4)) / 100.0f;
    seVoiceRecordAddAt(voice, position);
    return 1;
}

/* Native "bgmFade": argument 1 is the music volume (below 0: the default); argument 2 the
   fade length seVolumeSet hands to seVolumeRampSet, 0 (at once) when absent. */
int bgmFade(SbThread *call) {
    int volume;
    int fade;

    volume = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) == 0) {
        fade = vmArgGet(call, 2);
    } else {
        fade = 0;
    }
    seVolumeSet(volume, fade);
    return 1;
}

int nextBgm(SbThread *call) {
    bgmNameListIndex = (unsigned char) vmArgGet(call, 1);
    return 1;
}

/* Native "bgm": argument 1 names the track; the name is resolved through vmKeyedEntryGet and
   passed to seBgmNameSet, which starts it (an empty name stops the music). */
int runtimeBgm(SbThread *call) {
    int trackRef;
    char *trackName;

    trackRef = vmArgGet(call, 1);
    trackName = vmKeyedEntryGet(call, trackRef);
    seBgmNameSet(trackName);
    return 1;
}

extern SbNativeMethod runtimeNativeTable[];
extern char runtimeModuleName[];

/* Native "Vibration", at least 4 arguments.  When vmArgTagIsZero reports argument 1 as
   absent (nonzero), the fade is stopped (Vibration_Stop); otherwise argument 1 goes to
   Vibration_Set_Weak and arguments 2 to 4 start a Vibration_Set_Strong. */
int runtimeVibration(SbThread *call)
{
    int value;
    int targetValue;
    int currentValue;
    int framesTotal;

    if (VM_PLUGIN_CNT_GET(call) < 4) {
        sbSlotJumpRequest(call, 2);
        return 0;
    }
    if (vmArgTagIsZero(call, 1) != 0) {
        Vibration_Stop();
        return 1;
    }
    value = vmArgGet(call, 1);
    targetValue = vmArgGet(call, 2);
    currentValue = vmArgGet(call, 3);
    framesTotal = vmArgGet(call, 4);
    Vibration_Set_Weak(value);
    Vibration_Set_Strong(targetValue, currentValue, framesTotal);
    return 1;
}

/* Registers the "runtime" script module: runtimeModuleName holds its name string and
   runtimeNativeTable its table of name/function pointer pairs (setNumber, setLocater,
   setElevator, ...).  Called from scriptRegisterModules. */
void runtimeRegisterNatives(void) {
    sbNativeOwnerAdd(runtimeModuleName, runtimeNativeTable);
}

char runtimeModuleName[8] = "runtime";
SbNativeMethod runtimeNativeTable[35] = {
    { runtimeNativeNameSetNumber, runtimeSetNumber }, { runtimeNativeNameSetLocater, runtimeSetLocater }, { runtimeNativeNameSetElevator, runtimeSetElevator }, { runtimeNativeNameSetLastDan, runtimeSetLastDan },
    { runtimeNativeNameSetPlayer, runtimeSetPlayer }, { runtimeNativeNameItem, runtimeItem }, { runtimeNativeNameTimeAttack, runtimeTimeAttack }, { runtimeNativeNameGetAnalogR, runtimeGetAnalogR },
    { runtimeNativeNameProgEffect, runtimeProgEffect }, { runtimeNativeNameChangeRobo, changeRobo }, { runtimeNativeNameMessage, runtimeMessage }, { runtimeNativeNameJumpCF, runtimeJumpCf },
    { runtimeNativeNameMapChange, runtimeMapChange }, { runtimeNativeNameFade, runtimeFade }, { runtimeNativeNameSetFlag, runtimeSetFlag }, { runtimeNativeNameGetFlag, runtimeGetFlag },
    { runtimeNativeNameSetFog, runtimeSetMapFog }, { runtimeNativeNameSetItai, runtimeSetItai }, { runtimeNativeNameSetLight, runtimeSetMapLight }, { runtimeNativeNameSystem, runtimeSystem },
    { runtimeNativeNameMpeg2, mpeg2 }, { runtimeNativeNameAdx, adx }, { runtimeNativeNameAdxload, adxLoad }, { runtimeNativeNameAdxplay, adxPlay },
    { runtimeNativeNameSe, runtimeSe }, { runtimeNativeNameSePos, sePos }, { runtimeNativeNameSeVol, seVol }, { runtimeNativeNameBgmse, bgmSe },
    { runtimeNativeNameBgmsePos, bgmSePos }, { runtimeNativeNameBgmFade, bgmFade }, { runtimeNativeNameNextBgm, nextBgm }, { runtimeNativeNameBgm, runtimeBgm },
    { runtimeNativeNameVibration, runtimeVibration }, { runtimeNativeNameSetMapColor, setMapColor },
    { 0, 0 }
};
