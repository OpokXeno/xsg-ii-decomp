#include "common.h"
#include "main/enemy_1.h"
#include "shared.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/ladder_table.h"
#include "main/game_map_object.h"
#include "main/sef.h"
#include "main/xgl_flags.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/runtime_prog_effect.h"
#include "main/fade_ctrl.h"
#include "main/uwamono_objx.h"

#include "main/xgl_2.h"
void uwamonoSetRoboMode(signed char id);

extern int uwamonoEventIds[];

/* A bit of XenoActor.drawFlags (defined in src/main/xenoActor.c, not in a header). */
#define XENO_DRAW_COLOR 0x1 /* apply the color block: set by setFog after it writes fogDist/fogColor */

#define NULL ((void *) 0)


/* Native "progEffect": argument 2 is the frame count.  Without argument 1 the first
   argument of sefProgressEffect is 0; with it, argument 2 is also read as a key
   and the effect handle (uwamonoRecords[...].handles[0]) of the record found by uwamonoFindIndexByShortKey supplies it. */
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
   (none) and gameWork.nextMap/stateArg take the record's values. */
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
            actor->color.x = (float) vmArgGet(call, 2) / 100.0f;
            actor->color.y = (float) vmArgGet(call, 3) / 100.0f;
            actor->color.z = (float) vmArgGet(call, 4) / 100.0f;
            actor->color.w = (float) vmArgGet(call, 5) / 100.0f;
            actor->colorBlock.x = (float) vmArgGet(call, 6);
            actor->colorBlock.y = (float) vmArgGet(call, 7);
            actor->colorBlock.z = (float) vmArgGet(call, 8);
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

const char enepcNativeNameDisableDrawFlags[24] = "disableDrawFlags";

const char enepcNativeNameEnableDrawFlags[16] = "enableDrawFlags";

const char enepcNativeNameReadyEnepcData[16] = "readyEnepcData";

const char enepcNativeNameAlgorithm[16] = "Algorithm";

const char enepcNativeNameSetTogether[16] = "setTogether";

const char enepcNativeNameDefaultMotion[16] = "defaultMotion";

const char enepcNativeNameSetBattle[16] = "setBattle";

const char enepcNativeNameSetGroup[16] = "setGroup";

const char enepcNativeNameMotionFace[16] = "motionFace";

const char enepcNativeNameExMotion[16] = "ExMotion";

const char enepcNativeNameSetParamsAll[16] = "setParamsAll";

const char enepcNativeNameSetParams[24] = "setParams";

