#include "common.h"
#include "main/enemy_1.h"
#include "shared.h"
#include "main/cfePlugIn.h"
#include "main/elevator.h"
#include "main/gifState.h"
#include "main/runtime_enepc.h"
#include "main/runtime_uwamono_door.h"
#include "main/get_ladder.h"
#include "main/runtime_camera.h"
#include "main/runtime_pt_init.h"
#include "main/runtime_menu.h"
#include "main/script_register_natives.h"
#include "main/xenoActor.h"
#include "main/sef.h"
#include "main/undulate.h"
#include "main/game_data.h"
#include "main/ytachi_test.h"
#include "main/uwamono_objx.h"
#include "main/uwamono_record.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "ov01/data_record_get.h"
#include "ov01/sb_natives_unit.h"
#include "main/pxy_packet_state.h"
#include "main/nml_model_set.h"
#include "main/get.h"
#include "main/init_uwamono_sys.h"
#include "main/res_get_path.h"
#include "main/srs_file.h"

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


/* Index of the first used record (kind != MAP_KIND_FREE) whose id is key; -1 when there is none. */
int uwamonoFindIndexByShortKey(short key)
{
    UwamonoRecord *record = uwamonoRecords;
    int index;

    for (index = 0; index < MAP_RECORD_COUNT; index++, record++) {
        if (record->kind != MAP_KIND_FREE && record->id == key) {
            return index;
        }
    }
    return -1;
}

/* A second copy of uwamonoFindIndexByShortKey. */
int uwamonoFindIndexByKey(short key)
{
    UwamonoRecord *record = uwamonoRecords;
    int index;

    for (index = 0; index < MAP_RECORD_COUNT; index++, record++) {
        if (record->kind != MAP_KIND_FREE && record->id == key) {
            return index;
        }
    }
    return -1;
}

void uwamonoObjectEntryPositionGet(Vector4 *destination, int index)
{
    UwamonoPose *entry = (UwamonoPose *)((unsigned char *)mapObjectTable + mapObjectTable->entryMatrixOffset) + index;

    destination->x = entry->matrix[3][0];
    destination->y = entry->matrix[3][1];
    destination->z = entry->matrix[3][2];
    destination->w = 1.0f;
}

float asinf(float value);
double sqrt(double value);
float atan2f(float y, float x);

/* Euler angles of the rotation rows of table entry index: x from the pitch row, y from the
 * heading of the third row, z from the roll (reflected across pi when the up row points down). */
void uwamonoObjectEntryAnglesGet(Vector4 *angles, int index)
{
    UwamonoPose *entry = (UwamonoPose *)((unsigned char *)mapObjectTable + mapObjectTable->entryMatrixOffset) + index;
    float roll;

    if (entry->matrix[1][1] >= 0.0f) {
        double sine = entry->matrix[0][1];

        roll = asinf(sine / sqrt(1.0f - entry->matrix[2][1] * entry->matrix[2][1]));
    } else {
        double sine = entry->matrix[0][1];

        roll = 3.1415927f - asinf(sine / sqrt(1.0f - entry->matrix[2][1] * entry->matrix[2][1]));
    }
    angles->x = asinf(entry->matrix[2][1]);
    angles->y = atan2f(entry->matrix[2][0], entry->matrix[2][2]);
    angles->z = roll;
    angles->w = 1.0f;
}


/* Registers each script native module in turn: objx, enepc, runtime, ladder, camera, math, menu, uwamono, cfe, talk. */
void scriptRegisterModules(void)
{
    objxRegisterNatives();
    enepcRegisterNatives();
    runtimeRegisterNatives();
    ladderRegisterNatives();
    cameraRegisterNatives();
    mathRegisterNatives();
    menuRegisterNatives();
    uwamonoRegisterNatives();
    cfeRegisterPlugIn();
    elvReset();
    scriptRegisterNatives();
    runtimeInitState();
}

/* Native "getParam": [key], param. With a key: the position (0..2, hundredths) or the rotation
 * (3..5, hundredths of a degree) of that record, for MAP_KIND_ACTOR, MODEL and ACTOR_STATIC.
 * Without a key: 6 the current actor's kind, 7 the state argument (-2 when the game was loaded
 * from a save), 8 bit 11 of the control word, 9 the area group of the current record's key.
 * -1 when there is no such value. */
int objxGetParam(SbThread *thread)
{
    UwamonoRecord *current = &uwamonoRecords[uwamonoCurrentIndex];
    int result;
    int index;
    int key;
    int param;

    result = -1;
    if (VM_PLUGIN_CNT_GET(thread) != 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    if (vmArgTagIsZero(thread, 1) == 0) {
        key = vmArgGet(thread, 1);
        index = uwamonoFindIndexByShortKey((short) key);
    } else {
        index = -1;
    }
    param = vmArgGet(thread, 2);
    if (index == -1) {
        switch (param) {
        case 6:
            result = uwamonoRecords[uwamonoCurrentIndex].body.actor->kind;
            break;
        case 7:
            result = gameWork.loadStage != 0 ? -2 : gameWork.entryKey;
            break;
        case 8:
            result = (gameWork.control & 0x800) >> 11;
            break;
        case 9:
            key = current->key;
            if (key >= 0x10001 && key <= 0x10007) {
                result = 0;
            }
            if (key >= 0x10009 && key <= 0x1000D) {
                result = 1;
            }
            if (key >= 0x1000F && key <= 0x10014) {
                result = 2;
            }
            if (key >= 0x10015 && key <= 0x1001A) {
                result = 3;
            }
            if (key >= 0x1001B && key <= 0x10021) {
                result = 4;
            }
            if (key >= 0x10022 && key <= 0x10024) {
                result = 5;
            }
            if (key >= 0x10025 && key <= 0x1002B) {
                result = 6;
            }
            if (key == 0x14002) {
                result = 7;
            }
            if (key == 0x14003) {
                result = 8;
            }
            if (key == 0x14005) {
                result = 9;
            }
            break;
        }
    } else {
        switch (uwamonoRecords[index].kind) {
        case MAP_KIND_ACTOR:
        case MAP_KIND_MODEL:
        case MAP_KIND_ACTOR_STATIC:
            switch (param) {
            case 0:
                result = (int) (uwamonoRecords[index].positionCoords[0] * 100.0f);
                break;
            case 1:
                result = (int) (uwamonoRecords[index].positionCoords[1] * 100.0f);
                break;
            case 2:
                result = (int) (uwamonoRecords[index].positionCoords[2] * 100.0f);
                break;
            case 3:
                result = (int) (uwamonoRecords[index].rotationCoords[0] / 3.1415927f * 180.0f * 100.0f);
                break;
            case 4:
                result = (int) (uwamonoRecords[index].rotationCoords[1] / 3.1415927f * 180.0f * 100.0f);
                break;
            case 5:
                result = (int) (uwamonoRecords[index].rotationCoords[2] / 3.1415927f * 180.0f * 100.0f);
                break;
            }
            break;
        }
    }
    return result;
}

/* Native "move": key, frames, x, y, z (percent; an omitted component keeps the record's position),
 * [motion], [mode]: starts the record moving to the point. */
int objxMove(SbThread *thread)
{
    int key;
    int index;
    int frames;
    int motion;
    signed char mode;
    Vector4Elements destination;

    if (VM_PLUGIN_CNT_GET(thread) < 5) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    if (index == -1) {
        return 1;
    }
    frames = vmArgTagIsZero(thread, 2) != 0 ? 0 : vmArgGet(thread, 2);
    destination.vector.x = vmArgTagIsZero(thread, 3) != 0 ? uwamonoRecords[index].positionCoords[0] : (float) vmArgGet(thread, 3) / 100.0f;
    destination.vector.y = vmArgTagIsZero(thread, 4) != 0 ? uwamonoRecords[index].positionCoords[1] : (float) vmArgGet(thread, 4) / 100.0f;
    destination.vector.z = vmArgTagIsZero(thread, 5) != 0 ? uwamonoRecords[index].positionCoords[2] : (float) vmArgGet(thread, 5) / 100.0f;
    motion = vmArgTagIsZero(thread, 6) != 0 ? -1 : vmArgGet(thread, 6);
    mode = vmArgTagIsZero(thread, 7) != 0 ? 0 : (signed char) vmArgGet(thread, 7);
    uwamonoStartMove(index, &destination, frames, motion != -1, motion, mode);
    return 1;
}

/* Moves record index to destination over frames (at once when frames is 0, also on the actor of
 * an actor record). With a motion an actor record also turns towards the destination over 15
 * frames, the shorter way round. Flag, part and free records are not moved. */
void uwamonoStartMove(int index, Vector4Elements *destination, int frames, signed char hasMotion, int motion, signed char mode)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    float angle;

    switch (record->kind) {
    case MAP_KIND_ACTOR:
    case MAP_KIND_MODEL:
    case MAP_KIND_ACTOR_STATIC:
    case MAP_KIND_EFFECT:
    case MAP_KIND_MOVER:
        if (frames == 0) {
            if (hasMotion == 1) {
                record->position.y = unduGetSlotHeight(index, &destination->vector, record->body.motions->id, record->body.motions->groundId, 0);
            }
            if (record->kind == MAP_KIND_ACTOR_STATIC || record->kind == MAP_KIND_ACTOR) {
                struct XenoActor *actor = record->body.actor;

                actor->position.x = record->position.x = destination->vector.x;
                actor->position.y = record->position.y = destination->vector.y;
                actor->position.z = record->position.z = destination->vector.z;
                XenoActorPushMatrix(actor);
            } else {
                record->position.x = destination->vector.x;
                record->position.y = destination->vector.y;
                record->position.z = destination->vector.z;
            }
            record->moveCount = 0;
            record->moveStep = -1;
        } else {
            record->moveFrom.x = record->position.x;
            record->moveFrom.y = record->position.y;
            record->moveFrom.z = record->position.z;
            record->moveTo.x = destination->vector.x;
            record->moveTo.y = destination->vector.y;
            record->moveTo.z = destination->vector.z;
            record->moveCount = frames;
            record->moveStep = 0;
            if (hasMotion == 1) {
                record->flags |= 0x8000000;
            } else {
                record->flags &= ~0x8000000;
            }
            if (record->kind == MAP_KIND_ACTOR_STATIC || record->kind == MAP_KIND_ACTOR) {
                if (motion != -1) {
                    XenoActorCheckMode(record->body.actor, motion);
                    record->turnToCoords[0] = record->turnFromCoords[0] = record->rotationCoords[0];
                    record->turnFromCoords[1] = record->rotationCoords[1];
                    record->turnFromCoords[2] = record->rotationCoords[2];
                    angle = Get_Angle(&record->moveFrom, &record->moveTo);
                    record->turnToCoords[1] = angle;
                    if (angle < record->turnFromCoords[1]) {
                        if (record->turnFromCoords[1] - angle >= 3.1415927f) {
                            record->turnToCoords[1] = angle + 6.2831855f;
                        }
                    }
                    if (record->turnFromCoords[1] < record->turnToCoords[1]) {
                        if (record->turnToCoords[1] - record->turnFromCoords[1] >= 3.1415927f) {
                            record->turnToCoords[1] = record->turnToCoords[1] - 6.2831855f;
                        }
                    }
                    record->turnCount = 15;
                    record->turnToCoords[2] = record->turnFromCoords[2];
                    record->turnStep = 0;
                }
                record->moveEndMode = mode == 1 ? 0 : -1;
            }
        }
        break;
    }
}

/* Native "rotate": key, frames, x, y, z (degrees; an omitted component keeps the record's rotation):
 * turns the record to the angles over frames (at once when frames is 0, also on the actor of an
 * actor record). Flag, part, mover and free records are not turned. */
int objxRotate(SbThread *thread)
{
    int key;
    int index;
    int frames;
    float angle[3];
    float current[3];

    if (VM_PLUGIN_CNT_GET(thread) != 5) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    frames = vmArgTagIsZero(thread, 2) != 0 ? 0 : vmArgGet(thread, 2);
    angle[0] = vmArgTagIsZero(thread, 3) != 0 ? uwamonoRecords[index].rotationCoords[0] : (float) vmArgGet(thread, 3) / 100.0f / 180.0f * 3.1415927f;
    angle[1] = vmArgTagIsZero(thread, 4) != 0 ? uwamonoRecords[index].rotationCoords[1] : (float) vmArgGet(thread, 4) / 100.0f / 180.0f * 3.1415927f;
    angle[2] = vmArgTagIsZero(thread, 5) != 0 ? uwamonoRecords[index].rotationCoords[2] : (float) vmArgGet(thread, 5) / 100.0f / 180.0f * 3.1415927f;
    current[0] = uwamonoRecords[index].rotationCoords[0];
    current[1] = uwamonoRecords[index].rotationCoords[1];
    current[2] = uwamonoRecords[index].rotationCoords[2];
    switch (uwamonoRecords[index].kind) {
    case MAP_KIND_ACTOR:
    case MAP_KIND_MODEL:
    case MAP_KIND_ACTOR_STATIC:
    case MAP_KIND_EFFECT:
        if (frames == 0) {
            if (uwamonoRecords[index].kind == MAP_KIND_ACTOR_STATIC || uwamonoRecords[index].kind == MAP_KIND_ACTOR) {
                struct XenoActor *actor = uwamonoRecords[index].body.actor;

                actor->rotation.x = actor->direction.x = uwamonoRecords[index].rotationCoords[0] = angle[0];
                actor->rotation.y = actor->direction.y = uwamonoRecords[index].rotationCoords[1] = angle[1];
                actor->rotation.z = actor->direction.z = uwamonoRecords[index].rotationCoords[2] = angle[2];
            } else {
                uwamonoRecords[index].rotationCoords[0] = angle[0];
                uwamonoRecords[index].rotationCoords[1] = angle[1];
                uwamonoRecords[index].rotationCoords[2] = angle[2];
            }
            uwamonoRecords[index].turnStep = -1;
            uwamonoRecords[index].turnCount = 0;
        } else {
            uwamonoRecords[index].turnFromCoords[0] = current[0];
            uwamonoRecords[index].turnFromCoords[1] = current[1];
            uwamonoRecords[index].turnFromCoords[2] = current[2];
            uwamonoRecords[index].turnToCoords[0] = angle[0];
            uwamonoRecords[index].turnToCoords[1] = angle[1];
            uwamonoRecords[index].turnToCoords[2] = angle[2];
            uwamonoRecords[index].turnCount = frames;
            uwamonoRecords[index].turnStep = 0;
        }
        break;
    }
    return 1;
}

/* Native "scale": key, frames, x, y, z (percent; an omitted component keeps the record's scale). */
int objxScale(SbThread *thread)
{
    int key;
    int index;
    int frames;
    Vector4Elements scale;

    if (VM_PLUGIN_CNT_GET(thread) != 5) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    frames = vmArgTagIsZero(thread, 2) != 0 ? 0 : vmArgGet(thread, 2);
    scale.vector.x = vmArgTagIsZero(thread, 3) != 0 ? uwamonoRecords[index].scale.x : (float) vmArgGet(thread, 3) / 100.0f;
    scale.vector.y = vmArgTagIsZero(thread, 4) != 0 ? uwamonoRecords[index].scale.y : (float) vmArgGet(thread, 4) / 100.0f;
    scale.vector.z = vmArgTagIsZero(thread, 5) != 0 ? uwamonoRecords[index].scale.z : (float) vmArgGet(thread, 5) / 100.0f;
    uwamonoStartScale(index, &scale, frames);
    return 1;
}

/* Scales record index to scale over frames (at once when frames is 0, also on the actor of an actor
 * record). Flag, part, mover and free records are not scaled. */
void uwamonoStartScale(int index, Vector4Elements *scale, int frames)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    float current[3];

    current[0] = record->scale.x;
    current[1] = record->scale.y;
    current[2] = record->scale.z;
    switch (record->kind) {
    case MAP_KIND_ACTOR:
    case MAP_KIND_MODEL:
    case MAP_KIND_ACTOR_STATIC:
    case MAP_KIND_EFFECT:
    case MAP_KIND_MODEL_REQUEST:
        if (frames == 0) {
            if (uwamonoRecords[index].kind == MAP_KIND_ACTOR_STATIC || uwamonoRecords[index].kind == MAP_KIND_ACTOR) {
                struct XenoActor *actor = record->body.actor;

                actor->scale.x = record->scale.x = scale->vector.x;
                actor->scale.y = record->scale.y = scale->vector.y;
                actor->scale.z = record->scale.z = scale->vector.z;
                actor->scale.w = record->scale.w = 1.0f;
            } else {
                record->scale.x = scale->vector.x;
                record->scale.y = scale->vector.y;
                record->scale.z = scale->vector.z;
                record->scale.w = 1.0f;
            }
            record->scaleCount = 0;
            record->scaleStep = -1;
        } else {
            record->scaleFrom.x = current[0];
            record->scaleFrom.y = current[1];
            record->scaleFrom.z = current[2];
            record->scaleFrom.w = 1.0f;
            record->scaleTo.x = scale->vector.x;
            record->scaleTo.y = scale->vector.y;
            record->scaleTo.z = scale->vector.z;
            record->scaleCount = frames;
            record->scaleTo.w = 1.0f;
            record->scaleStep = 0;
        }
        break;
    }
}

/* Native "transparent": key, frames, transparency (percent; an omitted transparency keeps the record's,
 * omitted frames means at once). */
int objxTransparent(SbThread *thread)
{
    int key;
    int index;
    int frames;
    float value;

    if (VM_PLUGIN_CNT_GET(thread) != 3) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    if (vmArgTagIsZero(thread, 2) == 0) {
        frames = vmArgGet(thread, 2);
    } else {
        frames = 0;
    }
    if (vmArgTagIsZero(thread, 3) != 0) {
        value = uwamonoRecords[index].transparency;
    } else {
        value = (float) vmArgGet(thread, 3);
    }
    uwamonoStartTransparency(index, frames, value);
    return 1;
}

/* Changes the transparency of record index to value over frames, at once when frames is 0. */
void uwamonoStartTransparency(int index, int frames, float value)
{
    UwamonoRecord *record = &uwamonoRecords[index];

    if (frames == 0) {
        record->transparency = value;
        record->transparencyStep = -1;
        record->transparencyCount = -1;
    } else {
        record->transparencyCount = frames;
        record->transparencyTo = value;
        record->transparencyStep = 0;
        record->transparencyFrom = record->transparency;
    }
}

/* Native "offsetPivot": key, x, y, z (percent): the pivot of the record; an omitted component is kept. */
int objxOffsetPivot(SbThread *thread)
{
    int key;
    int index;
    UwamonoRecord *record;

    if (VM_PLUGIN_CNT_GET(thread) != 4) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    record = &uwamonoRecords[index];
    record->pivot.x = vmArgTagIsZero(thread, 2) != 0 ? record->pivot.x : (float) vmArgGet(thread, 2) / 100.0f;
    record->pivot.y = vmArgTagIsZero(thread, 3) != 0 ? record->pivot.y : (float) vmArgGet(thread, 3) / 100.0f;
    record->pivot.z = vmArgTagIsZero(thread, 4) != 0 ? record->pivot.z : (float) vmArgGet(thread, 4) / 100.0f;
    record->pivot.w = 1.0f;
    return 1;
}

/* Native "enableFlags": key, bits, [target]: sets flag bits on the record; target 99 sets them on every used
 * record, another target on every record of that kind. */
int objxEnableFlags(SbThread *thread)
{
    int key;
    int index;
    int bits;
    int target;
    int i;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    bits = vmArgGet(thread, 2);
    if (vmArgTagIsZero(thread, 3) != 0) {
        target = -1;
    } else {
        target = vmArgGet(thread, 3);
    }
    if (target == -1) {
        uwamonoRecords[index].flags |= bits;
    } else if (target == 99) {
        for (i = 0; i < MAP_RECORD_COUNT; i++) {
            if (uwamonoRecords[i].kind != MAP_KIND_FREE) {
                uwamonoRecords[i].flags |= bits;
            }
        }
    } else {
        for (i = 0; i < MAP_RECORD_COUNT; i++) {
            if (uwamonoRecords[i].kind == target) {
                uwamonoRecords[i].flags |= bits;
            }
        }
    }
    return 1;
}

/* Native "disableFlags": key, bits, [target]: clears flag bits on the record (every record of kind target when
 * target is given); clearing bit 0x20000 also releases the record's rider effects. */
int objxDisableFlags(SbThread *thread)
{
    int key;
    int index;
    int bits;
    int target;
    int first;
    int last;
    int i;
    UwamonoRecord *record;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    bits = vmArgGet(thread, 2);
    target = vmArgTagIsZero(thread, 3) != 0 ? -1 : vmArgGet(thread, 3);
    if (target == -1) {
        first = index;
        last = index + 1;
    } else {
        first = 0;
        last = MAP_RECORD_COUNT;
    }
    for (i = first; i < last; i++) {
        if (uwamonoRecords[i].kind == target || target == -1) {
            uwamonoRecords[i].flags &= ~bits;
            if (bits & 0x20000) {
                record = &uwamonoRecords[i];
                record->state = 0;
                if (record->handles[1] != 0) {
                    sefDeleteEffectCf(record->handles[1]);
                }
                if (record->handles[2] != 0) {
                    sefDeleteEffectCf(record->handles[2]);
                }
                record->handles[1] = record->handles[2] = 0;
            }
        }
    }
    return 1;
}

/* Native "setRotateTurn": key, order: the rotation order of the record. */
int objxSetRotateTurn(SbThread *thread)
{
    int key;
    int index;

    if (VM_PLUGIN_CNT_GET(thread) != 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    uwamonoRecords[index].rotationOrder = vmArgGet(thread, 2);
    return 1;
}

/* Native "setParent": key, [other], [attach]: with attach, the record's effect is owned by other's actor
 * (or by other's record when it has no actor); otherwise other becomes the record's target. */
int objxSetParent(SbThread *thread)
{
    int key;
    int index;
    int other;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    if (index == -1) {
        return 1;
    }
    other = vmArgTagIsZero(thread, 2) != 0 ? -1 : uwamonoFindIndexByShortKey((short) vmArgGet(thread, 2));
    if (vmArgTagIsZero(thread, 3) == 0) {
        if (uwamonoRecords[other].body.actor != 0) {
            sefSetRecordOwner(uwamonoRecords[index].handles[0], uwamonoRecords[other].body.actor);
            XenoActorPushMatrix(uwamonoRecords[other].body.actor);
            uwamonoRecords[other].releaseNextIndex = index;
        } else {
            sefSetRecordOwnerMarked(uwamonoRecords[index].handles[0], &uwamonoRecords[other]);
            uwamonoRecords[other].releaseNextIndex = index;
        }
    } else {
        uwamonoSetTarget(index, other, 1);
    }
    return 1;
}


/* Makes target the parent of record index. When either record is an actor record the record walks
 * to the target: the 128 points of the path are prepared, the state block starts action 5.
 * Otherwise the record keeps its offset from the target; with no target (-1) and flag 1 it takes its
 * position and rotation from its placement matrix instead. Returns 1. */
int uwamonoSetTarget(int index, int target, short flag)
{
    int i;
    float roll;

    if (uwamonoRecords[index].kind == MAP_KIND_ACTOR_STATIC || uwamonoRecords[index].kind == MAP_KIND_ACTOR
        || uwamonoRecords[target].kind == MAP_KIND_ACTOR_STATIC || uwamonoRecords[target].kind == MAP_KIND_ACTOR) {
        uwamonoRecords[index].parent = target;
        for (i = 0; i < 128; i++) {
            Get_MiddlePoint(&uwamonoRecords[index].position, &uwamonoRecords[target].position, (short) i, 128,
                            &uwamonoRecords[index].body.motions->path[i]);
        }
        uwamonoRecords[index].body.motions->mode = 1;
        uwamonoRecords[index].body.motions->pathStep = 0;
        Enemy_ActionReady(index, 5);
        return 1;
    }
    if (target == -1) {
        if (flag == 1) {
            uwamonoRecords[index].positionCoords[0] = uwamonoRecords[index].matrix[3][0];
            uwamonoRecords[index].positionCoords[1] = uwamonoRecords[index].matrix[3][1];
            uwamonoRecords[index].positionCoords[2] = uwamonoRecords[index].matrix[3][2];
            uwamonoRecords[index].positionCoords[3] = 1.0f;
            if (uwamonoRecords[index].matrix[1][1] >= 0.0f) {
                double sine = uwamonoRecords[index].matrix[0][1];

                roll = asinf(sine / sqrt(1.0f - uwamonoRecords[index].matrix[2][1] * uwamonoRecords[index].matrix[2][1]));
            } else {
                double sine = uwamonoRecords[index].matrix[0][1];

                roll = 3.1415927f - asinf(sine / sqrt(1.0f - uwamonoRecords[index].matrix[2][1] * uwamonoRecords[index].matrix[2][1]));
            }
            uwamonoRecords[index].rotationCoords[0] = asinf(uwamonoRecords[index].matrix[2][1]);
            uwamonoRecords[index].rotationCoords[1] = atan2f(uwamonoRecords[index].matrix[2][0], uwamonoRecords[index].matrix[2][2]);
            uwamonoRecords[index].rotationCoords[2] = roll;
            uwamonoRecords[index].rotationCoords[3] = 1.0f;
        }
    } else {
        uwamonoRecords[index].savedPositionCoords[0] = uwamonoRecords[index].positionCoords[0] - uwamonoRecords[target].positionCoords[0];
        uwamonoRecords[index].savedPositionCoords[1] = uwamonoRecords[index].positionCoords[1] - uwamonoRecords[target].positionCoords[1];
        uwamonoRecords[index].savedPositionCoords[2] = uwamonoRecords[index].positionCoords[2] - uwamonoRecords[target].positionCoords[2];
        uwamonoRecords[index].savedRotationCoords[0] = uwamonoRecords[index].rotationCoords[0] - uwamonoRecords[target].rotationCoords[0];
        uwamonoRecords[index].savedRotationCoords[1] = uwamonoRecords[index].rotationCoords[1] - uwamonoRecords[target].rotationCoords[1];
        uwamonoRecords[index].savedRotationCoords[2] = uwamonoRecords[index].rotationCoords[2] - uwamonoRecords[target].rotationCoords[2];
    }
    uwamonoRecords[index].parent = target;
    uwamonoRecords[index].attached = flag;
    return 1;
}

/* Native "setPair": key, otherKey: links the two records to look at each other; when the other record is
 * an effect (MAP_KIND_EFFECT) the effect is owned by this record's actor instead. */
int objxSetPair(SbThread *thread)
{
    int key;
    int index[2];
    UwamonoRecord *record;

    if (VM_PLUGIN_CNT_GET(thread) != 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index[0] = uwamonoFindIndexByShortKey((short) key);
    key = vmArgGet(thread, 2);
    index[1] = uwamonoFindIndexByShortKey((short) key);
    if (uwamonoRecords[index[1]].kind == MAP_KIND_EFFECT) {
        sefSetRecordOwner(uwamonoRecords[index[1]].handles[0], uwamonoRecords[index[0]].body.actor);
    } else {
        record = &uwamonoRecords[index[0]];
        record->lookLink = index[1];
        uwamonoRecords[index[1]].lookLink = index[0];
    }
    return 1;
}

/* Native: key, step (tenths), speed (percent); step -1 clears the move steps instead. */
int setStepMove(SbThread *thread)
{
    int key;
    int index;
    float step;
    float speed;

    if (VM_PLUGIN_CNT_GET(thread) != 3) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    step = (float) vmArgGet(thread, 2) / 10.0f;
    speed = (float) vmArgGet(thread, 3) / 100.0f;
    if (vmArgGet(thread, 2) == -1) {
        uwamonoClearMoveResetWord(index);
    } else {
        uwamonoAddMoveStep(index, step, speed);
    }
    return 1;
}

/* Native: key, step (tenths), speed (percent); step -1 clears the rotate steps instead. */
int setStepRotate(SbThread *thread)
{
    int key;
    int index;
    float step;
    float speed;

    if (VM_PLUGIN_CNT_GET(thread) != 3) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    step = (float) vmArgGet(thread, 2) / 10.0f;
    speed = (float) vmArgGet(thread, 3) / 100.0f;
    if (vmArgGet(thread, 2) == -1) {
        uwamonoClearRotateResetWord(index);
    } else {
        uwamonoAddRotateStep(index, step, speed);
    }
    return 1;
}

/* The original reaches the actor and motions of the record through the uwamonoSlotTable label (uwamonoRecords + 0x330),
 * a different relocation than uwamonoRecords, so these two natives keep it. */

/* Native "changeId": key, groundId: the map block 0x100 entry of the record's ground. */
int objxChangeId(SbThread *thread)
{
    int key;
    int index;
    int groundId;
    MapFixupEntry *ground;
    UwamonoSlot *slot;

    if (VM_PLUGIN_CNT_GET(thread) != 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    groundId = vmArgGet(thread, 2);
    ground = UnduDataGetHeader(0x100, groundId);
    slot = &uwamonoSlotTable[index];
    slot->body.actor->collision.block = ground;
    slot->body.motions->groundId = groundId;
    return 1;
}

/* Native "footId": key, id: the map block 0x700 entry of the record's footstep sound. */
int objxFootId(SbThread *thread)
{
    int key;
    int index;

    if (VM_PLUGIN_CNT_GET(thread) != 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    key = vmArgGet(thread, 1);
    index = uwamonoFindIndexByShortKey((short) key);
    uwamonoRecords[index].body.motions->footId = vmArgGet(thread, 2);
    return 1;
}


/* Native entries of the objx module: (name, function) pairs of 8 bytes. */
extern SbNativeMethod objxNatives[];
/* Module name "objx". */
extern char objxModuleName[];

/* Registers the objx script natives; called by the module registration in scriptRegisterModules. */
void objxRegisterNatives(void)
{
    sbNativeOwnerAdd(objxModuleName, objxNatives);
}

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

/* .data of this file (0x3E6DC0): the objx script natives. */
SbNativeMethod objxNatives[16] = {
    { "getParam", objxGetParam },
    { "move", objxMove },
    { "rotate", objxRotate },
    { "scale", objxScale },
    { "transparent", objxTransparent },
    { "offsetPivot", objxOffsetPivot },
    { "setRotateTurn", objxSetRotateTurn },
    { "setStepMove", setStepMove },
    { "setStepRotate", setStepRotate },
    { "enableFlags", objxEnableFlags },
    { "disableFlags", objxDisableFlags },
    { "setParent", objxSetParent },
    { "setPair", objxSetPair },
    { "changeId", objxChangeId },
    { "footId", objxFootId },
    { 0, 0 },
};


/* Creates the map object record for key, kind and entry in the first free slot and returns its index, or -1
 * when fourth names a key row that the gate bytes hide. The kind decides how the record is set up. */
int uwamonoCreateRecord(int key, int kind, int entry, int fourth, int fifth)
{
    UwamonoRecord *record;
    struct XenoActor *actor;
    UwamonoState *state;
    UwamonoPose *table;
    HitAction *hitAction;
    ItemRecord *item;
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
        XenoActorSetChannelValue(actor, 0.0f);
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
            state->waitTime = state->wanderTime = 20;
            state->pickCountB = state->pickCountA = 4;
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
                    state->groups[i] = keyRows[row].bytes[i];
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
                hitAction = hitActionGet(uwamonoEventTable[fourth].id);
                arg = (uwamonoRecords[uwamonoCurrentIndex].form == 1.0f ? hitAction->mapModel[0] : hitAction->mapModel[1]) + 12;
                break;
            case 2:
                item = itemRecordGet(uwamonoEventTable[fourth].id);
                arg = (uwamonoRecords[uwamonoCurrentIndex].form == 1.0f ? item->mapModel[0] : item->mapModel[1]) + 12;
                break;
            case 3:
                dataBoxCategory4RecordGet(uwamonoEventTable[fourth].id);
                arg = 18;
                break;
            case 4:
                record8 = dataBoxCategory3RecordGet(uwamonoEventTable[fourth].id);
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
    uwamonoRecords[index[0]].lookLink = index[1];
    uwamonoRecords[index[1]].lookLink = index[0];
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
            XenoActorSetChannelValue(actor, 4.0f);
            XenoActorEnableChannel(actor);
            XenoActorSetMode(actor, 0);
        } else {
            gameWork.control &= ~CONTROL_SCRIPTED;
            uwamonoRecords[uwamonoCurrentIndex].body.actor->collision.flags |= 0x1000;
            if (actor->mode < 3 || actor->mode > 5) {
                XenoActorEnableChannel(actor);
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

char objxModuleName[8] = "objx";
