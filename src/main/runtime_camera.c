#include "common.h"
#include "shared.h"
#include "main/game_camera.h"
#include "main/get_ladder.h"
#include "main/runtime_camera.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/game_data.h"

#include "main/xgl_2.h"

/* gameWork.camera.flags bits that the camera commands set. */
enum CameraFlagBits {
    CAMERA_FLAG_POS_RESET = 0x20,
    CAMERA_FLAG_BATTLE = 0x200
};

/* cameraMoveMode bits: a move command selects the reference block or the camera block. */
enum CameraMoveBits {
    CAMERA_MOVE_REF = 1,
    CAMERA_MOVE_CAM = 2
};

/* gameWork.camera.shakeFlags bits: one per shake channel (shakeA, shakeB, shakeC). */
enum CameraShakeChannel {
    CAMERA_SHAKE_CHANNEL_A = 1,
    CAMERA_SHAKE_CHANNEL_B = 2,
    CAMERA_SHAKE_CHANNEL_C = 4
};

/* refAnchorIndex value when the reference is not tied to an anchor. */
enum {
    CAMERA_ANCHOR_NONE = -1
};

extern const f32 cameraCentiScale;

f32 cameraCentiToFloat(int value) {
    return I2F(value) * cameraCentiScale;
}


int cameraFloatToCenti(f32 value) {
    return F2I(value * 100.0f);
}

extern const f32 cameraRadPerCentiDeg;

f32 cameraCentiDegToRad(int value) {
    return I2F(value) * cameraRadPerCentiDeg;
}

extern const f32 cameraRadPerCentiDegDiv;

int cameraRadToCentiDeg(f32 value) {
    return F2I(value / cameraRadPerCentiDegDiv);
}

/* A shake record (cameraShake): a float pair then four halfwords. */
struct CameraShakeRecord {
    float amplitude;
    float step;
    short paramA;
    short paramB;
    short paramC;
    short frames;
};

/* gameWork.camera.range.anchorIndex and anchorSubIndex as union members (alias set 0). Lever: cameraSetRefCamActR stores
   them after the vectors; plain members let the scheduler move the stores. */
typedef struct {
    unsigned char unmodeled_000[0x180];
    union { int anchorIndex; };
    union { short anchorSubIndex; };
} CameraAnchorAlias0;

static u8 cameraRefMoveFlag;
static u8 cameraCamMoveFlag;
static u8 cameraMoveMode;
/* Script argument count of the camera command now running; cameraCheckArgCount compares it with argc. */
static u8 cameraArgCount;
/* The camera state block: every command below reads or writes its fields. */

int cameraCheckArgCount(SbThread *handle) {
    if (VM_PLUGIN_CNT_GET(handle) != cameraArgCount) {
        sbSlotJumpRequest(handle, 2);
        return 0;
    }
    return 1;
}


int cameraReset(void) {
    GameCameraReset();
    return 1;
}

int cameraMode(SbThread *scriptHandle) {
    int modeA;
    int modeB;

    cameraArgCount = 2;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    modeA = vmArgGet(scriptHandle, 1);
    modeB = vmArgGet(scriptHandle, 2);
    gameWork.camera.cameraType = modeA;
    gameWork.camera.cameraSubType = modeB;
    return 1;
}

int cameraAngleMode(void) {
    return 1;
}

int cameraPosReset(SbThread *scriptHandle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    gameWork.camera.flags |= CAMERA_FLAG_POS_RESET;
    return 1;
}

/* One camera record of cameraCfSlots (208 bytes): cameraCfSet and cameraCfChange copy the reference block into it. */
struct CameraRefBlock {
    int words[28];
};

struct CameraStopBlock {
    int words[4];
};

struct CameraSlotRecord {
    unsigned char modeA;               /* +0x00: copy of gameWork.camera.cameraType */
    unsigned char unmodeled_01;
    unsigned char slotIndex;           /* +0x02: copy of gameWork.camera.surfaceGroup */
    unsigned char unmodeled_03[0xD];
    struct CameraStopBlock stopBlock;  /* +0x10: copy of gameWork.camera.slot[0] */
    struct CameraRefBlock refBlock;        /* +0x20: copy of gameWork.camera.range from anchorIndex */
    unsigned char unmodeled_90[0x40];
};

extern struct CameraSlotRecord cameraCfSlots[];

int cameraCfSet(SbThread *scriptHandle) {
    struct CameraSlotRecord *slot;
    int recordIndex;

    cameraArgCount = 1;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    recordIndex = vmArgGet(scriptHandle, 1);
    slot = &cameraCfSlots[recordIndex];
    slot->modeA = gameWork.camera.cameraType;
    slot->slotIndex = gameWork.camera.surfaceGroup;
    *(struct CameraStopBlock *)&slot->stopBlock = *(struct CameraStopBlock *)&gameWork.camera.slot[0];
    *(struct CameraRefBlock *)&slot->refBlock = *(struct CameraRefBlock *)&gameWork.camera.range.anchorIndex;
    return 1;
}

int cameraCfChange(SbThread *scriptHandle) {
    struct CameraSlotRecord *slot;
    int recordIndex;

    cameraArgCount = 1;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    recordIndex = vmArgGet(scriptHandle, 1);
    slot = &cameraCfSlots[recordIndex];
    slot->modeA = gameWork.camera.cameraType;
    slot->slotIndex = gameWork.camera.surfaceGroup;
    *(struct CameraRefBlock *)&slot->refBlock = *(struct CameraRefBlock *)&gameWork.camera.range.anchorIndex;
    return 1;
}

int cameraCfRotRange(SbThread *scriptHandle) {
    f32 radX;
    f32 radY;
    f32 radZ;
    f32 radW;
    int centiDeg;

    cameraArgCount = 4;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    centiDeg = vmArgGet(scriptHandle, 1);
    if (centiDeg == -9000) {
        centiDeg = -8999;
    }
    radX = cameraCentiDegToRad(centiDeg);
    radY = cameraCentiDegToRad(vmArgGet(scriptHandle, 2));
    radZ = cameraCentiDegToRad(vmArgGet(scriptHandle, 3));
    radW = cameraCentiDegToRad(vmArgGet(scriptHandle, 4));
    gameWork.camera.range.angleLimits.x = radX;
    gameWork.camera.range.angleLimits.y = radY;
    gameWork.camera.range.angleLimits.z = radZ;
    gameWork.camera.range.angleLimits.w = radW;
    return 1;
}

int cameraCfCamId(SbThread *handle) {
    cameraArgCount = 1;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    GameCameraChangeID(vmArgGet(handle, 1), 0, 0);
    return 1;
}

int cameraCfLock(SbThread *scriptHandle) {
    int lockFlags;

    cameraArgCount = 1;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    lockFlags = vmArgGet(scriptHandle, 1);
    gameWork.camera.range.probeMode = lockFlags & 0x21;
    gameWork.camera.range.lockX = lockFlags & 0x2;
    gameWork.camera.range.lockY = lockFlags & 0x4;
    gameWork.camera.range.lockZ = lockFlags & 0x8;
    return 1;
}

int cameraCfRangeMode(SbThread *handle) {
    cameraArgCount = 1;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    gameWork.camera.range.rangeMode = vmArgGet(handle, 1);
    return 1;
}

int cameraCfSelectChg(SbThread *handle) {
    cameraArgCount = 1;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    cameraSetIndex(vmArgGet(handle, 1));
    return 1;
}


int cameraCfHokan(SbThread *scriptHandle)
{
    int key;
    int subKey;
    struct CameraKeyEntry *entry;

    cameraArgCount = 2;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    key = vmArgGet(scriptHandle, 1);
    subKey = vmArgGet(scriptHandle, 2);
    entry = cameraSlotFind(-1, -1);
    if (entry == 0) {
        sbSlotJumpRequest(scriptHandle, 2);
        return 0;
    }
    entry->key = key;
    entry->sub = subKey;
    entry->anchorReady = gameWork.camera.range.rangeMode;
    entry->slot = gameWork.camera.slot[0];
    return 1;
}

int cameraCfBattleCam(SbThread *scriptHandle) {
    cameraArgCount = 1;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    gameWork.camera.flags &= 0xFDFF;
    if (vmArgGet(scriptHandle, 1) != 0) {
        gameWork.camera.flags |= CAMERA_FLAG_BATTLE;
    }
    gameWork.camera.slot[0].duration = 0;
    gameWork.camera.slot[1].duration = 0;
    gameWork.camera.slot[2].duration = 0;
    gameWork.camera.slot[3].duration = 0;
    gameWork.camera.flags = (gameWork.camera.flags & 0xFFEF) | 0x80;
    return 1;
}

int cameraCfNorth(SbThread *handle) {
    int looked_up;
    int out_of_range;

    cameraArgCount = 1;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    looked_up = vmArgGet(handle, 1);
    out_of_range = (u32)looked_up + 0x8CA0U > 0x11940U;
    gameWork.camera.range.rangeFlag = out_of_range;
    gameWork.camera.range.northAngle = cameraCentiDegToRad(looked_up);
    return 1;
}

int cameraSetRefPos(SbThread *scriptHandle) {
    f32 posX;
    f32 posY;
    f32 posZ;

    cameraArgCount = 3;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    posX = cameraCentiToFloat(vmArgGet(scriptHandle, 1));
    posY = cameraCentiToFloat(vmArgGet(scriptHandle, 2));
    posZ = cameraCentiToFloat(vmArgGet(scriptHandle, 3));
    if (cameraRefMoveFlag == 0) {
        gameWork.camera.range.lookAt.x = posX;
        gameWork.camera.range.lookAt.y = posY;
        gameWork.camera.range.lookAt.z = posZ;
        gameWork.camera.range.anchorIndex = CAMERA_ANCHOR_NONE;
        gameWork.camera.range.anchorSubIndex = 0;
        gameWork.camera.range.offset.x = 0;
        gameWork.camera.range.offset.y = 0;
        gameWork.camera.range.offset.z = 0;
    } else {
        gameWork.camera.to.lookAt.x = posX;
        gameWork.camera.to.lookAt.y = posY;
        gameWork.camera.to.lookAt.z = posZ;
    }
    cameraRefMoveFlag = 0;
    return 1;
}

int cameraSetRefAct(SbThread *scriptHandle) {
    Vector4 position;
    Vector4 offset;
    int anchorIndex;
    int anchorSubIndex;
    int centiX;
    int centiY;
    int centiZ;

    cameraArgCount = 5;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    anchorIndex = vmArgGet(scriptHandle, 1);
    anchorSubIndex = vmArgGet(scriptHandle, 2);
    centiX = vmArgGet(scriptHandle, 3);
    offset.x = cameraCentiToFloat(centiX);
    centiY = vmArgGet(scriptHandle, 4);
    offset.y = cameraCentiToFloat(centiY);
    centiZ = vmArgGet(scriptHandle, 5);
    offset.z = cameraCentiToFloat(centiZ);
    offset.w = 0.0f;
    cameraApplyAnchorOffset(&position, anchorIndex, anchorSubIndex, &offset);
    if (cameraRefMoveFlag == 0) {
        gameWork.camera.range.lookAt.x = position.x;
        gameWork.camera.range.lookAt.y = position.y;
        gameWork.camera.range.lookAt.z = position.z;
        gameWork.camera.range.offset.x = offset.x;
        gameWork.camera.range.offset.y = offset.y;
        gameWork.camera.range.offset.z = offset.z;
        gameWork.camera.range.anchorIndex = anchorIndex;
        gameWork.camera.range.anchorSubIndex = anchorSubIndex;
        gameWork.camera.range.offset.w = offset.w;
    } else {
        gameWork.camera.to.lookAt.x = position.x;
        gameWork.camera.to.lookAt.y = position.y;
        gameWork.camera.to.lookAt.z = position.z;
    }
    cameraRefMoveFlag = 0;
    return 1;
}


int cameraSetRefActR(SbThread *act)
{
    Vector4 transformed;
    Vector4 input;
    int anchor_index;
    int anchor_sub_index;
    int raw_x;
    int raw_y;
    int raw_z;
    int ok;

    cameraArgCount = 5;
    ok = cameraCheckArgCount(act);
    if (ok != 0)
    {
        anchor_index = vmArgGet(act, 1);
        anchor_sub_index = vmArgGet(act, 2);
        raw_x = vmArgGet(act, 3);
        input.x = cameraCentiToFloat(raw_x);
        raw_y = vmArgGet(act, 4);
        input.y = cameraCentiToFloat(raw_y);
        raw_z = vmArgGet(act, 5);
        input.z = cameraCentiToFloat(raw_z);
        input.w = 1.0f;
        cameraApplyAnchorOffset(&transformed, anchor_index, anchor_sub_index, &input);
        if (cameraRefMoveFlag == 0)
        {
            float transformed_x = transformed.x;
            float transformed_y = transformed.y;
            float transformed_z = transformed.z;
            float input_x = input.x;
            float input_y = input.y;
            float input_z = input.z;
            float input_w = input.w;

            gameWork.camera.range.anchorSubIndex = anchor_sub_index;
            gameWork.camera.range.offset.w = input_w;
            gameWork.camera.range.lookAt.x = transformed_x;
            gameWork.camera.range.lookAt.y = transformed_y;
            gameWork.camera.range.lookAt.z = transformed_z;
            gameWork.camera.range.offset.x = input_x;
            gameWork.camera.range.offset.y = input_y;
            gameWork.camera.range.offset.z = input_z;
            gameWork.camera.range.anchorIndex = anchor_index;
        }
        else
        {
            gameWork.camera.to.lookAt.x = transformed.x;
            gameWork.camera.to.lookAt.y = transformed.y;
            gameWork.camera.to.lookAt.z = transformed.z;
        }
        cameraRefMoveFlag = 0;
        ok = 1;
    }
    return ok;
}

extern Vector4 cameraRefAngOffset;

int cameraSetRefAng(SbThread *scriptHandle) {
    Matrix4 matrix;
    Vector4 offset;
    f32 rotX;
    f32 rotY;

    cameraArgCount = 2;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    rotX = cameraCentiDegToRad(vmArgGet(scriptHandle, 1));
    rotY = cameraCentiDegToRad(vmArgGet(scriptHandle, 2));
    offset.x = gameWork.camera.range.cameraPosition.x;
    offset.y = gameWork.camera.range.cameraPosition.y;
    offset.z = gameWork.camera.range.cameraPosition.z;
    offset.w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&offset);
    xglMatrixStackRotY(rotY);
    xglMatrixStackRotX(rotX);
    xglMatrixStackTrans(&cameraRefAngOffset);
    xglMatrixStackSave(matrix);
    if (cameraRefMoveFlag == 0) {
        gameWork.camera.range.lookAt.x = matrix[3][0];
        gameWork.camera.range.lookAt.y = matrix[3][1];
        gameWork.camera.range.lookAt.z = matrix[3][2];
        gameWork.camera.range.anchorIndex = CAMERA_ANCHOR_NONE;
        gameWork.camera.range.anchorSubIndex = 0;
        gameWork.camera.range.offset.x = 0;
        gameWork.camera.range.offset.y = 0;
        gameWork.camera.range.offset.z = 0;
    } else {
        gameWork.camera.to.lookAt.x = matrix[3][0];
        gameWork.camera.to.lookAt.y = matrix[3][1];
        gameWork.camera.to.lookAt.z = matrix[3][2];
    }
    cameraRefMoveFlag = 0;
    return 1;
}

int cameraRefLock(SbThread *scriptHandle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    gameWork.camera.slot[0].duration = 0;
    gameWork.camera.range.anchorSubIndex = 0;
    gameWork.camera.range.anchorIndex = CAMERA_ANCHOR_NONE;
    return 1;
}

int cameraSetRefCamActR(SbThread *scriptHandle) {
    Vector4 position;
    Vector4 orbitPos;
    Vector4 offset;
    Vector4 orbitAngles;
    struct XenoActor *anchorRecord;
    int anchorIndex;
    int anchorSubIndex;

    cameraArgCount = 8;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    anchorIndex = vmArgGet(scriptHandle, 1);
    anchorSubIndex = vmArgGet(scriptHandle, 2);
    offset.x = cameraCentiToFloat(vmArgGet(scriptHandle, 3));
    offset.y = cameraCentiToFloat(vmArgGet(scriptHandle, 4));
    offset.z = cameraCentiToFloat(vmArgGet(scriptHandle, 5));
    offset.w = 1.0f;
    orbitAngles.x = cameraCentiDegToRad(vmArgGet(scriptHandle, 6));
    orbitAngles.y = cameraCentiDegToRad(vmArgGet(scriptHandle, 7));
    orbitAngles.z = 0.0f;
    orbitAngles.w = cameraCentiToFloat(vmArgGet(scriptHandle, 8));
    anchorRecord = cameraApplyAnchorOffset(&position, anchorIndex, anchorSubIndex, &offset);
    orbitAngles.x = orbitAngles.x + anchorRecord->rotation.x;
    orbitAngles.y = orbitAngles.y + anchorRecord->rotation.y;
    GetCameraPos(&orbitPos, &position, &orbitAngles);
    if (cameraRefMoveFlag == 0) {
        gameWork.camera.range.offset.w = 1.0f;
        gameWork.camera.range.lookAt.x = position.x;
        gameWork.camera.range.lookAt.y = position.y;
        gameWork.camera.range.lookAt.z = position.z;
        ((CameraAnchorAlias0 *)&gameWork)->anchorIndex = CAMERA_ANCHOR_NONE;
        gameWork.camera.range.offset.x = offset.x;
        gameWork.camera.range.offset.y = offset.y;
        gameWork.camera.range.offset.z = offset.z;
        ((CameraAnchorAlias0 *)&gameWork)->anchorSubIndex = 0;
    } else {
        gameWork.camera.to.lookAt.x = position.x;
        gameWork.camera.to.lookAt.y = position.y;
        gameWork.camera.to.lookAt.z = position.z;
    }
    cameraRefMoveFlag = 0;
    if (cameraCamMoveFlag == 0) {
        gameWork.camera.range.cameraPosition.x = orbitPos.x;
        gameWork.camera.range.cameraPosition.y = orbitPos.y;
        gameWork.camera.range.cameraPosition.z = orbitPos.z;
        gameWork.camera.range.angles.x = orbitAngles.x;
        gameWork.camera.range.angles.y = orbitAngles.y;
        gameWork.camera.range.angles.w = orbitAngles.w;
    } else {
        gameWork.camera.to.position.x = orbitPos.x;
        gameWork.camera.to.position.y = orbitPos.y;
        gameWork.camera.to.position.z = orbitPos.z;
        gameWork.camera.to.angles.x = orbitAngles.x;
        gameWork.camera.to.angles.y = orbitAngles.y;
        gameWork.camera.to.angles.w = orbitAngles.w;
    }
    cameraCamMoveFlag = 0;
    return 1;
}


int cameraSetRefCamAct3R(SbThread *scriptHandle)
{
    Vector4 position;
    Vector4 orbitPos;
    Vector4 anchorPos;
    Vector4 offset;
    Vector4 orbitAngles;
    Matrix4 matrix;
    Vector4 betweenAngles;
    int startIndex;
    int startSubIndex;
    int endIndex;
    int endSubIndex;
    float blend;

    cameraArgCount = 11;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    startIndex = vmArgGet(scriptHandle, 1);
    startSubIndex = vmArgGet(scriptHandle, 2);
    endIndex = vmArgGet(scriptHandle, 3);
    endSubIndex = vmArgGet(scriptHandle, 4);
    blend = cameraCentiToFloat(vmArgGet(scriptHandle, 5));
    offset.x = cameraCentiToFloat(vmArgGet(scriptHandle, 6));
    offset.y = cameraCentiToFloat(vmArgGet(scriptHandle, 7));
    offset.z = cameraCentiToFloat(vmArgGet(scriptHandle, 8));
    offset.w = 1.0f;
    orbitAngles.x = cameraCentiDegToRad(vmArgGet(scriptHandle, 9));
    orbitAngles.y = cameraCentiDegToRad(vmArgGet(scriptHandle, 10));
    orbitAngles.z = 0.0f;
    orbitAngles.w = cameraCentiToFloat(vmArgGet(scriptHandle, 11));
    cameraAnchorGet(&orbitPos, startIndex, startSubIndex);
    cameraAnchorGet(&anchorPos, endIndex, endSubIndex);
    position.x = (anchorPos.x - orbitPos.x) * blend + orbitPos.x;
    position.y = (anchorPos.y - orbitPos.y) * blend + orbitPos.y;
    position.z = (anchorPos.z - orbitPos.z) * blend + orbitPos.z;
    cameraAnglesBetween(&betweenAngles, &orbitPos, &anchorPos);
    xglMatrixStackUnit();
    xglMatrixStackRotY(betweenAngles.y);
    xglMatrixStackRotX(betweenAngles.x);
    xglMatrixStackTrans(&offset);
    xglMatrixStackSave(matrix);
    position.x = position.x + matrix[3][0];
    position.y = position.y + matrix[3][1];
    position.z = position.z + matrix[3][2];
    orbitAngles.x = orbitAngles.x + betweenAngles.x;
    orbitAngles.y = orbitAngles.y + betweenAngles.y;
    GetCameraPos(&orbitPos, &position, &orbitAngles);
    if (cameraRefMoveFlag == 0) {
        gameWork.camera.range.offset.w = 1.0f;
        gameWork.camera.range.lookAt.x = position.x;
        gameWork.camera.range.lookAt.y = position.y;
        gameWork.camera.range.lookAt.z = position.z;
        gameWork.camera.range.offset.x = 0.0f;
        gameWork.camera.range.offset.y = 0.0f;
        gameWork.camera.range.offset.z = 0.0f;
        gameWork.camera.range.anchorIndex = CAMERA_ANCHOR_NONE;
        gameWork.camera.range.anchorSubIndex = 0;
    } else {
        gameWork.camera.to.lookAt.x = position.x;
        gameWork.camera.to.lookAt.y = position.y;
        gameWork.camera.to.lookAt.z = position.z;
    }
    cameraRefMoveFlag = 0;
    if (cameraCamMoveFlag == 0) {
        gameWork.camera.range.cameraPosition.x = orbitPos.x;
        gameWork.camera.range.cameraPosition.y = orbitPos.y;
        gameWork.camera.range.cameraPosition.z = orbitPos.z;
        gameWork.camera.range.angles.x = orbitAngles.x;
        gameWork.camera.range.angles.y = orbitAngles.y;
        gameWork.camera.range.angles.w = orbitAngles.w;
    } else {
        gameWork.camera.to.position.x = orbitPos.x;
        gameWork.camera.to.position.y = orbitPos.y;
        gameWork.camera.to.position.z = orbitPos.z;
        gameWork.camera.to.angles.x = orbitAngles.x;
        gameWork.camera.to.angles.y = orbitAngles.y;
        gameWork.camera.to.angles.w = orbitAngles.w;
    }
    cameraCamMoveFlag = 0;
    return 1;
}

int cameraSetCamPos(SbThread *scriptHandle)
{
    float posX;
    float posY;
    float posZ;

    cameraArgCount = 3;
    if (cameraCheckArgCount(scriptHandle) == 0)
    {
        return 0;
    }
    posX = cameraCentiToFloat(vmArgGet(scriptHandle, 1));
    posY = cameraCentiToFloat(vmArgGet(scriptHandle, 2));
    posZ = cameraCentiToFloat(vmArgGet(scriptHandle, 3));
    if (cameraCamMoveFlag == 0)
    {
        gameWork.camera.range.cameraPosition.x = posX;
        gameWork.camera.range.cameraPosition.y = posY;
        gameWork.camera.range.cameraPosition.z = posZ;
    }
    else
    {
        gameWork.camera.to.position.x = posX;
        gameWork.camera.to.position.y = posY;
        gameWork.camera.to.position.z = posZ;
    }
    cameraCamMoveFlag = 0;
    return 1;
}

int cameraSetCamAng(SbThread *act)
{
    Vector4 input;
    int raw_x;
    int raw_y;
    int raw_w;
    int ok;

    cameraArgCount = 3;
    ok = cameraCheckArgCount(act);
    if (ok != 0)
    {
        raw_x = vmArgGet(act, 1);
        input.x = cameraCentiDegToRad(raw_x);
        raw_y = vmArgGet(act, 2);
        input.y = cameraCentiDegToRad(raw_y);
        input.z = 0.0f;
        raw_w = vmArgGet(act, 3);
        input.w = cameraCentiToFloat(raw_w);
        if (cameraCamMoveFlag == 0)
        {
            gameWork.camera.range.angles.w = input.w;
            gameWork.camera.range.angles.x = input.x;
            gameWork.camera.range.angles.y = input.y;
            GetCameraPos(&gameWork.camera.range.cameraPosition, &gameWork.camera.range.lookAt, &input);
        }
        else
        {
            gameWork.camera.to.angles.w = input.w;
            gameWork.camera.to.angles.x = input.x;
            gameWork.camera.to.angles.y = input.y;
            GetCameraPos(&gameWork.camera.to.position, &gameWork.camera.to.lookAt, &input);
        }
        cameraCamMoveFlag = 0;
        ok = 1;
    }
    return ok;
}

int cameraSetCamAct(SbThread *act)
{
    Vector4 transformed;
    Vector4 input;
    int anchor_index;
    int anchor_sub_index;
    int raw_x;
    int raw_y;
    int raw_z;
    int ok;

    cameraArgCount = 5;
    ok = cameraCheckArgCount(act);
    if (ok != 0)
    {
        anchor_index = vmArgGet(act, 1);
        anchor_sub_index = vmArgGet(act, 2);
        raw_x = vmArgGet(act, 3);
        input.x = cameraCentiToFloat(raw_x);
        raw_y = vmArgGet(act, 4);
        input.y = cameraCentiToFloat(raw_y);
        raw_z = vmArgGet(act, 5);
        input.z = cameraCentiToFloat(raw_z);
        input.w = 0.0f;
        cameraApplyAnchorOffset(&transformed, anchor_index, anchor_sub_index, &input);
        if (cameraCamMoveFlag == 0)
        {
            gameWork.camera.range.cameraPosition.x = transformed.x;
            gameWork.camera.range.cameraPosition.y = transformed.y;
            gameWork.camera.range.cameraPosition.z = transformed.z;
        }
        else
        {
            gameWork.camera.to.position.x = transformed.x;
            gameWork.camera.to.position.y = transformed.y;
            gameWork.camera.to.position.z = transformed.z;
        }
        cameraCamMoveFlag = 0;
        ok = 1;
    }
    return ok;
}

int cameraSetCamActR(SbThread *act)
{
    Vector4 transformed;
    Vector4 input;
    int anchor_index;
    int anchor_sub_index;
    int raw_x;
    int raw_y;
    int raw_z;
    int ok;

    cameraArgCount = 5;
    ok = cameraCheckArgCount(act);
    if (ok != 0)
    {
        anchor_index = vmArgGet(act, 1);
        anchor_sub_index = vmArgGet(act, 2);
        raw_x = vmArgGet(act, 3);
        input.x = cameraCentiToFloat(raw_x);
        raw_y = vmArgGet(act, 4);
        input.y = cameraCentiToFloat(raw_y);
        raw_z = vmArgGet(act, 5);
        input.z = cameraCentiToFloat(raw_z);
        input.w = 1.0f;
        cameraApplyAnchorOffset(&transformed, anchor_index, anchor_sub_index, &input);
        if (cameraCamMoveFlag == 0)
        {
            gameWork.camera.range.cameraPosition.x = transformed.x;
            gameWork.camera.range.cameraPosition.y = transformed.y;
            gameWork.camera.range.cameraPosition.z = transformed.z;
        }
        else
        {
            gameWork.camera.to.position.x = transformed.x;
            gameWork.camera.to.position.y = transformed.y;
            gameWork.camera.to.position.z = transformed.z;
        }
        cameraCamMoveFlag = 0;
        ok = 1;
    }
    return ok;
}

int cameraSetCamAct2(SbThread *scriptHandle)
{
    Vector4 anchorPos;
    Vector4 angles;
    Vector4 orbitPos;
    int anchorIndex;
    int anchorSubIndex;

    cameraArgCount = 5;
    if (cameraCheckArgCount(scriptHandle) == 0)
    {
        return 0;
    }
    anchorIndex = vmArgGet(scriptHandle, 1);
    anchorSubIndex = vmArgGet(scriptHandle, 2);
    angles.x = cameraCentiDegToRad(vmArgGet(scriptHandle, 3));
    angles.y = cameraCentiDegToRad(vmArgGet(scriptHandle, 4));
    angles.z = 0.0f;
    angles.w = cameraCentiToFloat(vmArgGet(scriptHandle, 5));
    cameraAnchorGet(&anchorPos, anchorIndex, anchorSubIndex);
    GetCameraPos(&orbitPos, &anchorPos, &angles);
    if (cameraCamMoveFlag == 0)
    {
        gameWork.camera.range.cameraPosition.x = orbitPos.x;
        gameWork.camera.range.cameraPosition.y = orbitPos.y;
        gameWork.camera.range.cameraPosition.z = orbitPos.z;
        gameWork.camera.range.angles.x = angles.x;
        gameWork.camera.range.angles.y = angles.y;
        gameWork.camera.range.angles.w = angles.w;
    }
    else
    {
        gameWork.camera.to.position.x = orbitPos.x;
        gameWork.camera.to.position.y = orbitPos.y;
        gameWork.camera.to.position.z = orbitPos.z;
        gameWork.camera.to.angles.x = angles.x;
        gameWork.camera.to.angles.y = angles.y;
        gameWork.camera.to.angles.w = angles.w;
    }
    cameraCamMoveFlag = 0;
    return 1;
}

int cameraSetCamAct2R(SbThread *act)
{
    Vector4 scratch;
    Vector4 input;
    Vector4 transformed;
    struct XenoActor *offset_record;
    int anchor_index;
    int anchor_sub_index;
    int raw_x;
    int raw_y;
    int raw_w;
    int ok;

    cameraArgCount = 5;
    ok = cameraCheckArgCount(act);
    if (ok != 0)
    {
        anchor_index = vmArgGet(act, 1);
        anchor_sub_index = vmArgGet(act, 2);
        raw_x = vmArgGet(act, 3);
        input.x = cameraCentiDegToRad(raw_x);
        raw_y = vmArgGet(act, 4);
        input.y = cameraCentiDegToRad(raw_y);
        input.z = 0.0f;
        raw_w = vmArgGet(act, 5);
        input.w = cameraCentiToFloat(raw_w);
        offset_record = cameraAnchorGet(&scratch, anchor_index, anchor_sub_index);
        input.x += offset_record->rotation.x;
        input.y += offset_record->rotation.y;
        GetCameraPos(&transformed, &scratch, &input);
        if (cameraCamMoveFlag == 0)
        {
            float transformed_x = transformed.x;
            float transformed_y = transformed.y;
            float transformed_z = transformed.z;
            float input_x = input.x;
            float input_y = input.y;
            float input_w = input.w;

            gameWork.camera.range.angles.w = input_w;
            gameWork.camera.range.cameraPosition.y = transformed_y;
            gameWork.camera.range.cameraPosition.z = transformed_z;
            gameWork.camera.range.angles.x = input_x;
            gameWork.camera.range.angles.y = input_y;
            gameWork.camera.range.cameraPosition.x = transformed_x;
        }
        else
        {
            float transformed_x = transformed.x;
            float transformed_y = transformed.y;
            float transformed_z = transformed.z;
            float input_x = input.x;
            float input_y = input.y;
            float input_w = input.w;

            gameWork.camera.to.angles.w = input_w;
            gameWork.camera.to.position.y = transformed_y;
            gameWork.camera.to.position.z = transformed_z;
            gameWork.camera.to.angles.x = input_x;
            gameWork.camera.to.angles.y = input_y;
            gameWork.camera.to.position.x = transformed_x;
        }
        cameraCamMoveFlag = 0;
        ok = 1;
    }
    return ok;
}

int cameraSetCamAct3(SbThread *scriptHandle)
{
    Vector4 startAnchor;
    Vector4 endAnchor;
    float blend;
    float offsetX;
    float offsetY;
    float offsetZ;
    float posX;
    float posY;
    float posZ;
    int startIndex;
    int startSubIndex;
    int endIndex;
    int endSubIndex;
    int centiOffsetX;
    int centiOffsetY;
    int centiOffsetZ;
    int blendRaw;

    cameraArgCount = 8;
    if (cameraCheckArgCount(scriptHandle) == 0)
    {
        return 0;
    }
    startIndex = vmArgGet(scriptHandle, 1);
    startSubIndex = vmArgGet(scriptHandle, 2);
    endIndex = vmArgGet(scriptHandle, 3);
    endSubIndex = vmArgGet(scriptHandle, 4);
    centiOffsetX = vmArgGet(scriptHandle, 5);
    centiOffsetY = vmArgGet(scriptHandle, 6);
    centiOffsetZ = vmArgGet(scriptHandle, 7);
    blendRaw = vmArgGet(scriptHandle, 8);
    cameraAnchorGet(&startAnchor, startIndex, startSubIndex);
    blend = blendRaw;
    cameraAnchorGet(&endAnchor, endIndex, endSubIndex);
    offsetX = cameraCentiToFloat(centiOffsetX);
    posX = (endAnchor.x - startAnchor.x) * blend + startAnchor.x + offsetX;
    offsetY = cameraCentiToFloat(centiOffsetY);
    posY = (endAnchor.y - startAnchor.y) * blend + startAnchor.y + offsetY;
    offsetZ = cameraCentiToFloat(centiOffsetZ);
    posZ = (endAnchor.z - startAnchor.z) * blend + startAnchor.z + offsetZ;
    if (cameraCamMoveFlag == 0)
    {
        gameWork.camera.range.cameraPosition.x = posX;
        gameWork.camera.range.cameraPosition.y = posY;
        gameWork.camera.range.cameraPosition.z = posZ;
    }
    else
    {
        gameWork.camera.to.position.x = posX;
        gameWork.camera.to.position.y = posY;
        gameWork.camera.to.position.z = posZ;
    }
    cameraCamMoveFlag = 0;
    return 1;
}

int cameraOffsCamPos(SbThread *scriptHandle)
{
    float posX;
    float posY;
    float posZ;

    cameraArgCount = 3;
    if (cameraCheckArgCount(scriptHandle) == 0)
    {
        return 0;
    }
    posX = cameraCentiToFloat(vmArgGet(scriptHandle, 1));
    posY = cameraCentiToFloat(vmArgGet(scriptHandle, 2));
    posZ = cameraCentiToFloat(vmArgGet(scriptHandle, 3));
    posX += gameWork.camera.range.cameraPosition.x;
    posY += gameWork.camera.range.cameraPosition.y;
    posZ += gameWork.camera.range.cameraPosition.z;
    if (cameraCamMoveFlag == 0)
    {
        gameWork.camera.range.cameraPosition.x = posX;
        gameWork.camera.range.cameraPosition.y = posY;
        gameWork.camera.range.cameraPosition.z = posZ;
    }
    else
    {
        gameWork.camera.to.position.x = posX;
        gameWork.camera.to.position.y = posY;
        gameWork.camera.to.position.z = posZ;
    }
    cameraCamMoveFlag = 0;
    return 1;
}

int cameraOffsCamAng(SbThread *scriptHandle)
{
    Vector4 angles;

    cameraArgCount = 3;
    if (cameraCheckArgCount(scriptHandle) == 0)
    {
        return 0;
    }
    angles.x = cameraCentiDegToRad(vmArgGet(scriptHandle, 1));
    angles.y = cameraCentiDegToRad(vmArgGet(scriptHandle, 2));
    angles.z = 0.0f;
    angles.w = cameraCentiToFloat(vmArgGet(scriptHandle, 3));
    angles.x += gameWork.camera.range.angles.x;
    angles.y += gameWork.camera.range.angles.y;
    angles.w += gameWork.camera.range.angles.w;
    if (cameraCamMoveFlag == 0)
    {
        gameWork.camera.range.angles.x = angles.x;
        gameWork.camera.range.angles.y = angles.y;
        gameWork.camera.range.angles.w = angles.w;
        GetCameraPos(&gameWork.camera.range.cameraPosition, &gameWork.camera.range.lookAt, &angles);
    }
    else
    {
        gameWork.camera.to.angles.x = angles.x;
        gameWork.camera.to.angles.y = angles.y;
        gameWork.camera.to.angles.w = angles.w;
        GetCameraPos(&gameWork.camera.to.position, &gameWork.camera.to.lookAt, &angles);
    }
    cameraCamMoveFlag = 0;
    return 1;
}

int cameraCamLock(SbThread *value) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(value) != 0) {
        gameWork.camera.slot[1].duration = 0;
        return 1;
    }
    return 0;
}

int cameraMoveRefMode(SbThread *scriptHandle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    cameraRefMoveFlag = 1;
    cameraMoveMode |= CAMERA_MOVE_REF;
    return 1;
}

int cameraMoveCamMode(SbThread *scriptHandle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    cameraCamMoveFlag = 1;
    cameraMoveMode |= CAMERA_MOVE_CAM;
    return 1;
}

int cameraMove(SbThread *act)
{
    int ok;
    int blend_mode;
    int ease_mode;
    int frames;

    cameraArgCount = 3;
    ok = cameraCheckArgCount(act);
    if (ok != 0)
    {
        blend_mode = vmArgGet(act, 1);
        ease_mode = vmArgGet(act, 2);
        frames = vmArgGet(act, 3);
        if (cameraMoveMode == 0)
        {
            cameraMoveMode = 3;
        }
        cameraApplySlots(cameraMoveMode, blend_mode, ease_mode, frames);
        cameraMoveMode = 0;
        ok = 1;
    }
    return ok;
}

int cameraWaitMoveRef(SbThread *scriptHandle, int *waitFlag) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    if (gameWork.camera.slot[0].fraction < 1.0f) {
        *waitFlag = 1;
    }
    return 1;
}

int cameraWaitMoveCam(SbThread *handle, int *flag) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    if (gameWork.camera.slot[1].fraction < 1.0f) {
        *flag = 1;
    }
    return 1;
}

int cameraMoveRefStop(SbThread *handle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    gameWork.camera.slot[0].progress = 0;
    gameWork.camera.slot[0].duration = 0;
    return 1;
}

int cameraMoveCamStop(SbThread *handle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    gameWork.camera.slot[1].progress = 0;
    gameWork.camera.slot[1].duration = 0;
    return 1;
}

int cameraMovePersStop(SbThread *scriptHandle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    gameWork.camera.slot[2].progress = 0;
    gameWork.camera.slot[2].duration = 0;
    return 1;
}

int cameraMoveBankStop(SbThread *scriptHandle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    gameWork.camera.slot[3].progress = 0;
    gameWork.camera.slot[3].duration = 0.0f;
    return 1;
}

int cameraShake(SbThread *scriptHandle)
{
    float amplitude;
    float step;
    int shakeMode;
    int paramA;
    int paramC;
    int frames;

    cameraArgCount = 5;
    if (cameraCheckArgCount(scriptHandle) == 0)
    {
        return 0;
    }
    shakeMode = vmArgGet(scriptHandle, 1);
    amplitude = cameraCentiDegToRad(vmArgGet(scriptHandle, 2));
    paramA = vmArgGet(scriptHandle, 3);
    paramC = vmArgGet(scriptHandle, 4);
    frames = vmArgGet(scriptHandle, 5);
    step = amplitude / I2F(frames);
    gameWork.camera.shakeFlags = shakeMode;
    if (shakeMode & CAMERA_SHAKE_CHANNEL_A)
    {
        gameWork.camera.waves[0].value = amplitude;
        gameWork.camera.waves[0].step = step;
        gameWork.camera.waves[0].period = paramA;
        gameWork.camera.waves[0].phase = 0;
        gameWork.camera.waves[0].hold = paramC;
        gameWork.camera.waves[0].remaining = frames;
    }
    if (shakeMode & CAMERA_SHAKE_CHANNEL_B)
    {
        gameWork.camera.waves[1].value = amplitude;
        gameWork.camera.waves[1].step = step;
        gameWork.camera.waves[1].period = paramA;
        gameWork.camera.waves[1].hold = paramC;
        gameWork.camera.waves[1].remaining = frames;
    }
    if (shakeMode & CAMERA_SHAKE_CHANNEL_C)
    {
        gameWork.camera.waves[2].value = amplitude;
        gameWork.camera.waves[2].step = step;
        gameWork.camera.waves[2].period = paramA;
        gameWork.camera.waves[2].hold = paramC;
        gameWork.camera.waves[2].remaining = frames;
    }
    return 1;
}

int cameraHandShake(SbThread *scriptHandle) {
    cameraArgCount = 0;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    gameWork.camera.sway[2] = 0;
    gameWork.camera.shakeFlags ^= 8;
    gameWork.camera.sway[3] = 0;
    gameWork.camera.sway[0] = 0;
    gameWork.camera.sway[1] = 0;
    return 1;
}

int cameraSetBank(SbThread *handle) {
    cameraArgCount = 1;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    gameWork.camera.range.lookAt.w = cameraCentiDegToRad(vmArgGet(handle, 1));
    return 1;
}

int cameraSetPers(SbThread *handle) {
    cameraArgCount = 1;
    if (cameraCheckArgCount(handle) == 0) {
        return 0;
    }
    gameWork.camera.range.cameraPosition.w = cameraCentiDegToRad(vmArgGet(handle, 1));
    return 1;
}

int cameraMoveBank(SbThread *act)
{
    int angle_raw;
    int bank_byte;
    int bank_raw;
    int ok;

    cameraArgCount = 3;
    ok = cameraCheckArgCount(act);
    if (ok != 0)
    {
        angle_raw = vmArgGet(act, 1);
        bank_byte = vmArgGet(act, 2);
        bank_raw = vmArgGet(act, 3);
        gameWork.camera.to.lookAt.w = cameraCentiDegToRad(angle_raw);
        gameWork.camera.slot[3].easeMode = bank_byte;
        gameWork.camera.slot[3].blendMode = 0;
        gameWork.camera.slot[3].fraction = 0;
        gameWork.camera.slot[3].progress = 0;
        gameWork.camera.slot[3].duration = I2F(bank_raw);
        ok = 1;
    }
    return ok;
}

int cameraMovePers(SbThread *scriptHandle)
{
    int angleCenti;
    int persByte;
    int persRaw;
    int ok;

    cameraArgCount = 3;
    ok = cameraCheckArgCount(scriptHandle);
    if (ok != 0)
    {
        angleCenti = vmArgGet(scriptHandle, 1);
        persByte = vmArgGet(scriptHandle, 2);
        persRaw = vmArgGet(scriptHandle, 3);
        gameWork.camera.to.position.w = cameraCentiDegToRad(angleCenti);
        gameWork.camera.slot[2].easeMode = persByte;
        gameWork.camera.slot[2].blendMode = 0;
        gameWork.camera.slot[2].fraction = 0;
        gameWork.camera.slot[2].progress = 0;
        gameWork.camera.slot[2].duration = I2F(persRaw);
        ok = 1;
    }
    return ok;
}

int cameraGetRefPos(SbThread *scriptHandle) {
    int *outWords;
    float refPosY;
    float refPosZ;

    cameraArgCount = 1;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    outWords = vmEntryGet(scriptHandle, vmArgGet(scriptHandle, 1));
    refPosY = gameWork.camera.range.lookAt.y;
    refPosZ = gameWork.camera.range.lookAt.z;
    outWords[0] = cameraFloatToCenti(gameWork.camera.range.lookAt.x);
    outWords[1] = cameraFloatToCenti(refPosY);
    outWords[2] = cameraFloatToCenti(refPosZ);
    return 1;
}

int cameraGetRefAng(SbThread *scriptHandle) {
    int *outWords;
    float angleY;

    cameraArgCount = 1;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    outWords = vmEntryGet(scriptHandle, vmArgGet(scriptHandle, 1));
    angleY = gameWork.camera.range.angles.y;
    outWords[0] = cameraRadToCentiDeg(gameWork.camera.range.angles.x);
    outWords[1] = cameraRadToCentiDeg(angleY);
    return 1;
}

int cameraGetCamPos(SbThread *scriptHandle) {
    int *outWords;
    float camPosY;
    float camPosZ;

    cameraArgCount = 1;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    outWords = vmEntryGet(scriptHandle, vmArgGet(scriptHandle, 1));
    camPosY = gameWork.camera.range.cameraPosition.y;
    camPosZ = gameWork.camera.range.cameraPosition.z;
    outWords[0] = cameraFloatToCenti(gameWork.camera.range.cameraPosition.x);
    outWords[1] = cameraFloatToCenti(camPosY);
    outWords[2] = cameraFloatToCenti(camPosZ);
    return 1;
}

int cameraGetCamAng(SbThread *scriptHandle) {
    int *outWords;
    float camAngleY;
    float camAngleW;

    cameraArgCount = 1;
    if (cameraCheckArgCount(scriptHandle) == 0) {
        return 0;
    }
    outWords = vmEntryGet(scriptHandle, vmArgGet(scriptHandle, 1));
    camAngleY = gameWork.camera.range.angles.y;
    camAngleW = gameWork.camera.range.angles.w;
    outWords[0] = cameraRadToCentiDeg(gameWork.camera.range.angles.x);
    outWords[1] = cameraRadToCentiDeg(camAngleY);
    outWords[2] = cameraFloatToCenti(camAngleW);
    return 1;
}

extern SbNativeMethod cameraNatives[];
extern char cameraModuleName[];

void cameraRegisterNatives(void) {
    sbNativeOwnerAdd(cameraModuleName, cameraNatives);
    cameraRefMoveFlag = 0;
    cameraCamMoveFlag = 0;
    cameraMoveMode = 0;
}
unsigned char cameraNativeSetPersName[8] = "SetPers";
unsigned char cameraNativeSetBankName[8] = "SetBank";
unsigned char cameraNativeShakeName[8] = "Shake";
unsigned char cameraNativeMoveName[8] = "Move";
unsigned char cameraNativeCamLockName[8] = "CamLock";
unsigned char cameraNativeRefLockName[8] = "RefLock";
unsigned char cameraNativeCfNorthName[8] = "cfNorth";
unsigned char cameraNativeCfHokanName[8] = "cfHokan";
unsigned char cameraNativeCfLockName[8] = "cfLock";
unsigned char cameraNativeCfCamIDName[8] = "cfCamID";
unsigned char cameraNativeCfSetName[8] = "cfSet";
unsigned char cameraNativeModeName[8] = "mode";
unsigned char cameraNativeResetName[8] = "reset";
char cameraModuleName[8] = "camera";

const f32 cameraCentiScale = 0.00999999978f;
const f32 cameraRadPerCentiDeg = 0.000174532935f;
const f32 cameraRadPerCentiDegDiv = 0.000174532935f;

/* .data of this file (0x3E7040-0x3E71E8): the camera script natives. */

/* Camera reference position: x = 0, y = 0, z = -10, w = 1. */
Vector4 cameraRefAngOffset = { 0.0f, 0.0f, -10.0f, 1.0f };

/* Native table of the camera module. */
SbNativeMethod cameraNatives[51] = {
    { cameraNativeResetName, cameraReset },
    { cameraNativeModeName, cameraMode },
    { "AngleMode", cameraAngleMode },
    { "PosReset", cameraPosReset },
    { cameraNativeCfSetName, cameraCfSet },
    { "cfChange", cameraCfChange },
    { "cfRotRange", cameraCfRotRange },
    { "cfRangeMode", cameraCfRangeMode },
    { cameraNativeCfCamIDName, cameraCfCamId },
    { cameraNativeCfLockName, cameraCfLock },
    { "cfSelectCHG", cameraCfSelectChg },
    { cameraNativeCfHokanName, cameraCfHokan },
    { "cfBattleCam", cameraCfBattleCam },
    { cameraNativeCfNorthName, cameraCfNorth },
    { "SetRefPos", cameraSetRefPos },
    { "SetRefAct", cameraSetRefAct },
    { "SetRefActR", cameraSetRefActR },
    { "SetRefAng", cameraSetRefAng },
    { cameraNativeRefLockName, cameraRefLock },
    { "SetRefCamActR", cameraSetRefCamActR },
    { "SetRefCamAct3R", cameraSetRefCamAct3R },
    { "SetCamPos", cameraSetCamPos },
    { "SetCamAng", cameraSetCamAng },
    { "SetCamAct", cameraSetCamAct },
    { "SetCamActR", cameraSetCamActR },
    { "SetCamAct2", cameraSetCamAct2 },
    { "SetCamAct2R", cameraSetCamAct2R },
    { "SetCamAct3", cameraSetCamAct3 },
    { "OffsCamPos", cameraOffsCamPos },
    { "OffsCamAng", cameraOffsCamAng },
    { cameraNativeCamLockName, cameraCamLock },
    { "MoveRefMode", cameraMoveRefMode },
    { "MoveCamMode", cameraMoveCamMode },
    { cameraNativeMoveName, cameraMove },
    { "WaitMoveRef", cameraWaitMoveRef },
    { "WaitMoveCam", cameraWaitMoveCam },
    { "MoveRefStop", cameraMoveRefStop },
    { "MoveCamStop", cameraMoveCamStop },
    { "MovePersStop", cameraMovePersStop },
    { "MoveBankStop", cameraMoveBankStop },
    { cameraNativeShakeName, cameraShake },
    { "HandShake", cameraHandShake },
    { cameraNativeSetBankName, cameraSetBank },
    { cameraNativeSetPersName, cameraSetPers },
    { "MoveBank", cameraMoveBank },
    { "MovePers", cameraMovePers },
    { "GetRefPos", cameraGetRefPos },
    { "GetRefAng", cameraGetRefAng },
    { "GetCamPos", cameraGetCamPos },
    { "GetCamAng", cameraGetCamAng },
    { 0, 0 }
};
