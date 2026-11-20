#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/game_camera.h"
#include "main/pad.h"
#include "main/quest_flag.h"
#include "main/quest_flag_set.h"
#include "main/msg_state.h"
#include "main/msg_window_bind.h"
#include "main/ysegf.h"
#include "main/yg2path.h"
#include "main/se_bank_load.h"
#include "shared.h"
#include "main/game_data.h"
#include "main/script_register_natives.h"
#include "ov01/data_box.h"

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
int menuAddItem(SbThread *ctx, int *error_flag)
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
        *error_flag = 1;
    }
    return 1;
}

/* Kept with a parameter: msgResetBalances (msg_window_bind.c) is defined without one, but the original call passes the argument
 * (menuDeptQuest evaluates vmArgGet(ctx, 3) for it). */

/* Script native menuDeptQuest (table 0x3ec8c8): four arguments; up to 40 string entries are read from the script. */
int menuDeptQuest(SbThread *ctx, int *error_flag)
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
        *error_flag = 1;
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
int menuDeptGetResult(SbThread *unused_ctx, int *error_flag)
{
    int result = msgPurchaseUpdate();

    if (result < 0) {
        *error_flag = 1;
    }
    return result;
}

/* Script native menuGetResult (table 0x3ec8c0): on failure the argument count is reported through arg_count_out. */
int menuGetResult(SbThread *ctx, int *arg_count_out)
{
    int arg_count = VM_PLUGIN_CNT_GET(ctx);
    int result;

    if (arg_count != 1) {
        sbSlotJumpRequest(ctx, 2);
        return 0;
    }
    result = msgPollSelect();
    if (result < 0) {
        *arg_count_out = arg_count;
    }
    return result + 1;
}

/* Script native slotWinOpen (table 0x3ec8d8): two arguments, passed to msgChoiceOpen when the window is ready. */
int slotWinOpen(SbThread *ctx, int *error_flag)
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
        *error_flag = 1;
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
