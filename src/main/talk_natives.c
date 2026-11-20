#include "common.h"
#include "main/talk_natives.h"
#include "main/msg_state.h"
#include "main/game_data.h"

#include "shared.h"
#include "main/game_camera.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/cursor_buffer.h"
#include "main/text_box.h"
#include "main/pad.h"
#include "main/quest_flag_set.h"
#include "main/msg_window_bind.h"
#include "main/ysegf.h"
#include "main/yg2path.h"
#include "main/se_bank_load.h"
#include "ov01/data_box.h"
#include "main/xenoActor.h"
#include "main/xgl_vu1_regs.h"
#include "main/ytachi_test.h"


int talkCheck(void) {
    int previous;

    previous = talkResult;
    talkResult = -1;
    if (msgWindowBusy() != 0) {
        return -1;
    }
    return (gameWork.control & CONTROL_MAP_ATTACK) ? -1 : previous;
}

int talkReset(void) {
    talkResult = -1;
    return 1;
}

/* .sdata of this file (0x69D770-0x69D778): the talk state of the active map object. */
struct UwamonoRecord *mapActiveObject = 0;

int talkResult = -1;

int strcmp(const char *left, const char *right);

/* vmArgGetHigh's type of a string argument. */
#define SCRIPT_ARG_STRING 3

/* "NULL" is the entry that ends a script string list. */

/* Script native talk.smsg (table 0x3ec878): begins a message. Argument 1 is one
 * string, or a list of strings ended by "NULL", for the window text; arguments 2
 * and up fill the message slots 1 and up. Clears the open slot's flag word and
 * updates the current map object. */
int talkSmsg(SbThread *thread)
{
    int index;
    int *list;
    int arg;
    void *object;

    /* The argument type is compared as a byte: without the cast gcc drops the andi 0xFF. */
    if ((unsigned char)vmArgGetHigh(thread, 1) != SCRIPT_ARG_STRING) {
        list = vmEntryGet(thread, vmArgGet(thread, 1));
        msgBegin();
        for (index = 0; strcmp("NULL", vmKeyedEntryGet(thread, list[index])) != 0; index++) {
            msgWindowSetScriptText(vmKeyedEntryGet(thread, list[index]));
        }
    } else {
        msgBegin();
        msgWindowSetScriptText(vmKeyedEntryGet(thread, vmArgGet(thread, 1)));
    }
    for (arg = 2; arg <= VM_PLUGIN_CNT_GET(thread); arg++) {
        msgWindowSetSlot(arg - 1, vmArgGet(thread, arg));
    }
    msgSlotSet(msgSlotCurrent(), 0);
    object = uwamonoCurrentRecord();
    mapObjectBeginMessage(object);
    mapObjectFaceActive(object);
    return 1;
}

/* Store script argument 1 as the flag word of the open message slot (none open:
 * nothing stored). No call or native table entry references it. */
int talkSetSlotFlags(SbThread *thread)
{
    int flags = vmArgGet(thread, 1);

    if (msgSlotCurrent() < 0) {
        return 1;
    }
    msgSlotSet(msgSlotCurrent(), flags);
    return 1;
}

/* Script native talk.msgDtl (table 0x3ec880): talk.smsg with a leading argument.
 * Argument 1 is the flag word stored for the open slot, argument 2 the string or
 * "NULL"-ended string list for the window text, arguments 3 and up fill the
 * message slots 1 and up. */
int talkMsgDtl(SbThread *thread)
{
    int index;
    int *list;
    int arg;
    int flags;
    void *object;

    /* The argument type is compared as a byte, as in smsg. */
    if ((unsigned char)vmArgGetHigh(thread, 2) != SCRIPT_ARG_STRING) {
        list = vmEntryGet(thread, vmArgGet(thread, 2));
        msgBegin();
        for (index = 0; strcmp("NULL", vmKeyedEntryGet(thread, list[index])) != 0; index++) {
            msgWindowSetScriptText(vmKeyedEntryGet(thread, list[index]));
        }
    } else {
        msgBegin();
        msgWindowSetScriptText(vmKeyedEntryGet(thread, vmArgGet(thread, 2)));
    }
    for (arg = 3; arg <= VM_PLUGIN_CNT_GET(thread); arg++) {
        msgWindowSetSlot(arg - 2, vmArgGet(thread, arg));
    }
    flags = vmArgGet(thread, 1);
    msgSlotSet(msgSlotCurrent(), flags);
    object = uwamonoCurrentRecord();
    mapObjectBeginMessage(object);
    mapObjectFaceActive(object);
    return 1;
}

/* Names of script natives (.sdata, address order). The native tables below point at them;
 * no code of this file does. */
char talkNativeNameWait[8] = "wait";
char talkNativeNameResume[8] = "resume";
char talkNativeNameMsgDtl[8] = "msgDtl";
char talkNativeNameSmsg[8] = "smsg";
char talkNativeNameReset[8] = "reset";
char talkNativeNameCheck[8] = "check";
char segNativeNameAddrOn[8] = "addrOn";

/* Script native talk.winClose (table 0x3ec888): closes the message window. */
int talkWinClose(void) {
    msgWindowCloseForTalk();
    return 1;
}

/* Script native talkResume (table 0x3ec890). */
int talkResume(void) {
    CursorTextRecord *record = cursorBufferGetCurrent();

    if (record != 0) {
        cursorBufferTick(&record->second_step);
    }
    return 1;
}

/* Script native talk.enableFlags (table 0x3ec898): the body only returns 1. */
int talkEnableFlags(void) {
    return 1;
}

/* Script native talk.disableFlags (table 0x3ec8a0): the body only returns 1. */
int talkDisableFlags(void) {
    return 1;
}

/* Script native talk.isDispMsg (table 0x3ec8a8): returns whether the message window is in use (a sibling call, so
 * msgWindowInUse's result is the native's). */
int talkIsDispMsg(void) {
    return msgWindowInUse();
}

/* Script native talk.wait (table 0x3ec8b0): *out is set to 1 when msgWindowInUse returns non-zero. */
int talkWait(SbThread *unused_thread, int *out) {
    if (msgWindowInUse() != 0) {
        *out = 1;
    }
    return 1;
}

extern const char segAddrOnFoundText[40];
extern const char segAddrOnDoorText[56];
extern const char segAddrOnDecodedText[32];

/* Message kinds of seg.addrOn, the native's second argument: the address was found, written on a door, or decoded. */
enum SegAddrMessage {
    SEG_ADDR_MSG_FOUND = 1,
    SEG_ADDR_MSG_ON_DOOR = 2,
    SEG_ADDR_MSG_DECODED = 3
};

/* Entries of the department/quest menu: names and costs are read per entry, and the item counts are set per entry. */
#define MENU_DEPT_ENTRIES 40

/* Script native seg.addrOn (table 0x3ec918): segment address number (1-based) and message kind; sets the address flag
   with segSetAddrOn, then shows the text for the kind (kinds 1 to 3). */
int segAddrOnMessage(SbThread *ctx)
{
    int arg_count = VM_PLUGIN_CNT_GET(ctx);
    int messageKind;

    if (arg_count != 2) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    messageKind = vmArgGet(ctx, 2);
    segSetAddrOn(vmArgGet(ctx, 1) - 1, messageKind);
    mapObjectBeginMessage(uwamonoCurrentRecord());
    msgBegin();
    switch (messageKind) {
    case SEG_ADDR_MSG_FOUND:
        seVoiceStartDefault(0x100000a);
        msgWindowSetScriptText(segAddrOnFoundText);
        break;
    case SEG_ADDR_MSG_ON_DOOR:
        msgWindowSetScriptText(segAddrOnDoorText);
        break;
    case SEG_ADDR_MSG_DECODED:
        seVoiceStartDefault(0x200000c);
        msgWindowSetScriptText(segAddrOnDecodedText);
        break;
    }
    msgWindowSetSlot(1, vmArgGet(ctx, 1));
    msgSlotSet(msgSlotCurrent(), 0);
    return 1;
}

/* Script native seg.getAddrOn (table 0x3ec920): argument 1 is the segment address number, 1-based. */
int segAddrOnGetFlag(SbThread *ctx)
{
    if (VM_PLUGIN_CNT_GET(ctx) != 1) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    return segGetAddrOn(vmArgGet(ctx, 1) - 1);
}

/* Script native setQuestFlag (table 0x3ec8f0): flag number (1-based) and value; stored by questFlagSet. */
int setQuestFlag(SbThread *ctx)
{
    int flagNumber;
    int value;

    if (VM_PLUGIN_CNT_GET(ctx) != 2) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    flagNumber = vmArgGet(ctx, 1);
    value = vmArgGet(ctx, 2);
    questFlagSet(flagNumber - 1, value);
    return 1;
}

/* Script native getQuestFlag (table 0x3ec8f8): flag number (1-based); the value comes from questFlagGet. */
int getQuestFlag(SbThread *ctx)
{
    if (VM_PLUGIN_CNT_GET(ctx) != 1) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    return questFlagGet(vmArgGet(ctx, 1) - 1);
}

/* Script native setCitizenFlag (table 0x3ec900): citizen number (1-based) and value; stored by citizenFlagSet. */
int setCitizenFlag(SbThread *ctx)
{
    int citizenNumber;
    int value;

    if (VM_PLUGIN_CNT_GET(ctx) != 2) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    citizenNumber = vmArgGet(ctx, 1);
    value = vmArgGet(ctx, 2);
    citizenFlagSet(citizenNumber - 1, value);
    return 1;
}

/* Script native getCitizenFlag (table 0x3ec908): citizen number (1-based); the value comes from yg2PathGetCitizenFlag. */
int getCitizenFlag(SbThread *ctx)
{
    if (VM_PLUGIN_CNT_GET(ctx) != 1) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    return yg2PathGetCitizenFlag(vmArgGet(ctx, 1) - 1);
}

/* Script native menuAddItem (table 0x3ec8b8): a string argument is added to the menu. */
int menuAddItem(SbThread *ctx, int *wait_status)
{
    if (VM_PLUGIN_CNT_GET(ctx) != 2) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    if ((vmArgGetHigh(ctx, 2) & 0xff) != 3) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    if (msgWindowBusy() == 0 && msgWindowHasMore() == 0 && (PadData[0].pressed & 0x20) != 0) {
        msgWindowOpenText(vmKeyedEntryGet(ctx, vmArgGet(ctx, 2)));
        gameWork.control |= 0x8000;
    } else {
        *wait_status = 1;
    }
    return 1;
}

/* Kept with a parameter: msgResetBalances (msg_window_bind.c) is defined without one, but the original call passes the argument
 * (menuDeptQuest evaluates vmArgGet(ctx, 3) for it). */

/* Script native menuDeptQuest (table 0x3ec8c8): four arguments; up to 40 string entries are read from the script. */
int menuDeptQuest(SbThread *ctx, int *wait_status)
{
    int *names;
    int *costs;
    int count;
    int i;

    count = 640;
    if (VM_PLUGIN_CNT_GET(ctx) != 4) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    if (msgWindowBusy() != 0 || msgWindowHasMore() != 0) {
        *wait_status = 1;
    } else {
        names = vmEntryGet(ctx, vmArgGet(ctx, 1));
        costs = vmEntryGet(ctx, vmArgGet(ctx, 2));
        msgResetBalances(vmArgGet(ctx, 3));
        for (i = 0; i < count / 16; i++) {
            msgSetItemName(i, vmKeyedEntryGet(ctx, names[i]));
            msgSetItemCost(i, costs[i]);
        }
        if (vmArgGet(ctx, 4) == 0) {
            msgSetMode(0);
            for (i = 0; i < MENU_DEPT_ENTRIES; i++) {
                msgSetItemCount(i, dataBoxChkCategory1(i + 1));
            }
        } else {
            msgSetMode(1);
            for (i = 0; i < MENU_DEPT_ENTRIES; i++) {
                msgSetItemCount(i, dataBoxChkCategory2(i + 1));
            }
        }
        msgWindowBindTables();
        gameWork.control |= 0x8000;
    }
    return 1;
}

/* Script native menuDeptGetResult (table 0x3ec8d0). */
int menuDeptGetResult(SbThread *unused_ctx, int *wait_status)
{
    int result = msgSaleUpdate();

    if (result < 0) {
        *wait_status = 1;
    }
    return result;
}

/* Script native menuGetResult (table 0x3ec8c0): while the selection is still pending (negative poll result), wait_status receives the argument count. */
int menuGetResult(SbThread *ctx, int *wait_status)
{
    int arg_count = VM_PLUGIN_CNT_GET(ctx);
    int result;

    if (arg_count != 1) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    result = msgPollSelect();
    if (result < 0) {
        *wait_status = arg_count;
    }
    return result + 1;
}

/* Script native slotWinOpen (table 0x3ec8d8): two arguments, passed to msgChoiceOpen when the window is ready. */
int slotWinOpen(SbThread *ctx, int *wait_status)
{
    int timelineFrame;
    int choiceIndex;

    if (VM_PLUGIN_CNT_GET(ctx) != 2) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    if (msgWindowBusy() == 0 && msgWindowHasMore() == 0 && (PadData[0].pressed & 0x20) != 0) {
        timelineFrame = vmArgGet(ctx, 1);
        choiceIndex = vmArgGet(ctx, 2);
        msgChoiceOpen(timelineFrame, choiceIndex);
        gameWork.control |= 0x8000;
    } else {
        *wait_status = 1;
    }
    return 1;
}

const char segAddrOnFoundText[40] = "You found Segment Address No. %d!";

const char segAddrOnDoorText[56] = "\"Segment Address No. %d\" is written on the door.";

const char segAddrOnDecodedText[32] = "Segment Address No. %d decoded!";

const char talkNativeNameSlotWinGetResult[24] = "slotWinGetResult";

const char talkNativeNameSlotWinOpen[16] = "slotWinOpen";

const char talkNativeNameMenuDeptGetResult[24] = "menuDeptGetResult";

const char talkNativeNameMenuDeptQuest[16] = "menuDeptQuest";

const char talkNativeNameMenuGetResult[16] = "menuGetResult";

const char talkNativeNameMenuAddItem[16] = "menuAddItem";

const char talkNativeNameIsDispMsg[16] = "isDispMsg";

const char talkNativeNameDisableFlags[16] = "disableFlags";

const char talkNativeNameEnableFlags[16] = "enableFlags";

const char talkNativeNameWinClose[16] = "winClose";

const char g2pathNativeNameGetCitizenFlag[16] = "getCitizenFlag";

const char g2pathNativeNameSetCitizenFlag[16] = "setCitizenFlag";

const char g2pathNativeNameGetQuestFlag[16] = "getQuestFlag";

const char g2pathNativeNameSetQuestFlag[16] = "setQuestFlag";

const char segNativeNameGetAddrOn[16] = "getAddrOn";


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
        XenoActorSetBlendLocal(object->body.actor);
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
            XenoActorSetBlendLocal(mapActiveObject->body.actor);
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
