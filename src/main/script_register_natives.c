#include "common.h"
#include "ov01/sb_vm.h"
#include "shared.h"
#include "main/pt_member.h"
#include "main/script_register_natives.h"
#include "main/quest_flag.h"
#include "main/msg_state.h"
#include "main/talk_msg.h"
#include "main/talk_window_ctrl.h"
#include "main/xenoActor.h"
#include "main/game_camera.h"
#include "main/game_data.h"
#include "main/xgl_vu1_regs.h"
#include "main/ytachi_test.h"


/* Script native slotWinGetResult (table 0x3ec8e0). */
int slotWinGetResult(SbThread *thread, int *failure_flag)
{
    int status;

    status = msgChoicePoll();
    if (status < 0) {
        *failure_flag = 1;
    }
    return status;
}

extern char talkModuleName[];
extern char g2pathModuleName[];
extern char segModuleName[];
extern SbNativeMethod talkNativeTable[];
extern SbNativeMethod g2pathNativeTable[];
extern SbNativeMethod segNativeTable[];

/*
 * Registers three native groups with the script VM: "talk" (table 0x3ec868),
 * "g2path" (table 0x3ec8f0) and "seg" (table 0x3ec918); the group names sit at
 * talkModuleName, g2pathModuleName and segModuleName.
 */
void scriptRegisterNatives(void)
{
    sbNativeOwnerAdd(talkModuleName, talkNativeTable);
    sbNativeOwnerAdd(g2pathModuleName, g2pathNativeTable);
    sbNativeOwnerAdd(segModuleName, segNativeTable);
}


void mapObjectBeginMessage(UwamonoRecord *object)
{
    if (object != 0) {
        XenoActorEnableChannel(object->body.actor);
        XenoActorSetMode(object->body.actor, 0);
        gameWork.control |= CONTROL_SCRIPTED;
    }
}


void mapObjectSetActive(UwamonoRecord *owner, UwamonoRecord *value)
{
    mapActiveObject = value;
}


void mapObjectFaceActive(UwamonoRecord *object)
{
    if (object != 0 && mapActiveObject != 0) {
        mapActiveObject->flags |= 0x400400;
        if (msgSlotTestCurrent(16) == 0) {
            /* The active object turns to face `object`: heading = offset - atan2 of the pose difference. */
            struct XenoActor *targetPose = object->body.actor;
            struct XenoActor *activePose = mapActiveObject->body.actor;
            mapActiveObject->body.actor->direction.y = 1.5707963705062866f - xglAtan2Angle(targetPose->position.x - activePose->position.x, targetPose->position.z - activePose->position.z);
        }
        if (msgSlotTestCurrent(8) == 0) {
            XenoActorEnableChannel(mapActiveObject->body.actor);
            XenoActorSetMode(mapActiveObject->body.actor, mapActiveObject->body.motions->walk[3]);
        }
        mapActiveObject = 0;
    }
}

int mapObjectClearTalkFlags(UwamonoRecord *object)
{
    int masked;
    int flags;

    masked = object->flags & 0xFFBFFFFF;
    flags = masked & ~0x400;
    object->flags = flags;
    return flags;
}


void talkStateReset(void)
{
    mapActiveObject = 0;
    talkResult = -1;
}

void talkSetResult(int value)
{
    talkResult = value;
}


extern const char talkNativeNameCheck[];
extern const char talkNativeNameReset[];
extern const char talkNativeNameSmsg[];
extern const char talkNativeNameMsgDtl[];
extern const char talkNativeNameWinClose[];
extern const char talkNativeNameResume[];
extern const char talkNativeNameEnableFlags[];
extern const char talkNativeNameDisableFlags[];
extern const char talkNativeNameIsDispMsg[];
extern const char talkNativeNameWait[];
extern const char talkNativeNameMenuAddItem[];
extern const char talkNativeNameMenuGetResult[];
extern const char talkNativeNameMenuDeptQuest[];
extern const char talkNativeNameMenuDeptGetResult[];
extern const char talkNativeNameSlotWinOpen[];
extern const char talkNativeNameSlotWinGetResult[];
extern const char g2pathNativeNameSetQuestFlag[];
extern const char g2pathNativeNameGetQuestFlag[];
extern const char g2pathNativeNameSetCitizenFlag[];
extern const char g2pathNativeNameGetCitizenFlag[];
extern const char segNativeNameAddrOn[];
extern const char segNativeNameGetAddrOn[];

SbNativeMethod talkNativeTable[17] = {
    { talkNativeNameCheck, talkCheck },
    { talkNativeNameReset, talkReset },
    { talkNativeNameSmsg, talkSmsg },
    { talkNativeNameMsgDtl, talkMsgDtl },
    { talkNativeNameWinClose, talkWinClose },
    { talkNativeNameResume, talkResume },
    { talkNativeNameEnableFlags, talkEnableFlags },
    { talkNativeNameDisableFlags, talkDisableFlags },
    { talkNativeNameIsDispMsg, talkIsDispMsg },
    { talkNativeNameWait, talkWait },
    { talkNativeNameMenuAddItem, menuAddItem },
    { talkNativeNameMenuGetResult, menuGetResult },
    { talkNativeNameMenuDeptQuest, menuDeptQuest },
    { talkNativeNameMenuDeptGetResult, menuDeptGetResult },
    { talkNativeNameSlotWinOpen, slotWinOpen },
    { talkNativeNameSlotWinGetResult, slotWinGetResult },
    { 0, 0 }
};

SbNativeMethod g2pathNativeTable[5] = {
    { g2pathNativeNameSetQuestFlag, setQuestFlag },
    { g2pathNativeNameGetQuestFlag, getQuestFlag },
    { g2pathNativeNameSetCitizenFlag, setCitizenFlag },
    { g2pathNativeNameGetCitizenFlag, getCitizenFlag },
    { 0, 0 }
};

SbNativeMethod segNativeTable[3] = {
    { segNativeNameAddrOn, segAddrOnMessage },
    { segNativeNameGetAddrOn, segAddrOnGetFlag },
    { 0, 0 }
};

/* .sdata of this file (0x69D7B8-0x69D7D0): the module names of scriptRegisterNatives. */
char talkModuleName[8] = "talk";

char g2pathModuleName[8] = "g2path";

char segModuleName[8] = "seg";
