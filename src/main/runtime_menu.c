#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "shared.h"
#include "main/runtime_menu.h"
#include "main/pt_member.h"
#include "main/talk_natives.h"
#include "main/menu_selection.h"
#include "main/runtime_math.h"
#include "main/save_party_init.h"
#include "ov01/calc.h"


int ptInit(SbThread *context)
{
    if (VM_PLUGIN_CNT_GET(context) != 0)
    {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    ptStateInit();
    return 1;
}


/* Script native "ptClear": takes no arguments; calls ptResetSlots and returns 1. */
int ptClear(SbThread *context)
{
    if (VM_PLUGIN_CNT_GET(context) != 0)
    {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    ptResetSlots();
    return 1;
}


/* Script native "ptJoin": one argument; its handle (argument 1) goes to ptAddMember, then the refresh calls. */
int ptJoin(SbThread *context)
{
    int handle;

    if (VM_PLUGIN_CNT_GET(context) != 1) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    handle = vmArgGet(context, 1);
    ptAddMember(handle);
    ptLoadPartyModels();
    ptRefreshCurrent();
    return 1;
}

/* Script native "ptLeave": one argument; its handle (argument 1) goes to ptRemoveMember, then the refresh calls. */
int ptLeave(SbThread *context)
{
    int handle;

    if (VM_PLUGIN_CNT_GET(context) != 1) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    handle = vmArgGet(context, 1);
    ptRemoveMember(handle);
    ptLoadPartyModels();
    ptRefreshCurrent();
    return 1;
}

extern unsigned char playerSlotSkipHeaderFlag;

int ptChangeMode(SbThread *context)
{
    int argCount;
    int requested;
    enum PartySplitState splitMode;

    argCount = VM_PLUGIN_CNT_GET(context);
    if (argCount != 1) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    requested = vmArgGet(context, 1);
    switch (requested) {
    case PARTY_SPLIT_OFF:
        ptLeaveSplit();
        break;
    case PARTY_SPLIT_ON:
        ptEnterSplit();
        break;
    default:
        splitMode = ptState.splitMode;
        switch (splitMode) {
        case PARTY_SPLIT_OFF:
            ptEnterSplit();
            break;
        case PARTY_SPLIT_ON:
            ptLeaveSplit();
            break;
        }
        break;
    }
    playerSlotSkipHeaderFlag = 1;
    ptLoadPartyModels();
    ptRefreshCurrent();
    return 1;
}

int ptSet(SbThread *context)
{
    int memberSlot;

    if (VM_PLUGIN_CNT_GET(context) <= 0 || VM_PLUGIN_CNT_GET(context) >= 8) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    for (memberSlot = 0; memberSlot < 7; memberSlot++) {
        ptRemoveMember(memberSlot + 1);
    }
    memberSlot = 0;
    while (memberSlot < VM_PLUGIN_CNT_GET(context)) {
        ptAddMember(vmArgGet(context, memberSlot + 1));
        memberSlot++;
    }
    ptLoadPartyModels();
    ptRefreshCurrent();
    VM_PLUGIN_CNT_GET(context);
    return 1;
}


int ptSetPilot(SbThread *context)
{
    int pilotId;
    int rosterId;
    int slot;
    int rosterIndex;
    int candidate;

    if (VM_PLUGIN_CNT_GET(context) != 2) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    if (ptState.splitMode != PARTY_SPLIT_ON) {
        return 0;
    }
    pilotId = vmArgGet(context, 1);
    rosterId = vmArgGet(context, 2);
    slot = ptFindSlot(pilotId);
    if (slot == -1) {
        return 0;
    }
    slot -= 4;
    rosterIndex = -1;
    for (candidate = 3; candidate < 10; candidate++) {
        if (ptState.roster[candidate] != 0 && ptState.roster[candidate]->id == rosterId) {
            rosterIndex = candidate;
            break;
        }
    }
    if (rosterIndex == -1) {
        return 0;
    }
    ptSwapRosterEntries(slot, rosterIndex);
    ptRefresh();
    return 1;
}

int ptGetLeader(SbThread *context)
{
    PartyMember *member;
    int id;

    if (VM_PLUGIN_CNT_GET(context) != 1) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    if (vmArgGet(context, 1) != 0) {
        PartyState *backParty = &ptState;
        PartyMember *backMember = backParty->slots[backParty->backIndex + 7];
        id = 0;
        if (backMember != 0) {
            id = backMember->id;
        }
        return id;
    }
    member = ptState.slots[ptState.frontIndex];
    if (member == 0) {
        id = 0;
    } else {
        id = member->id;
    }
    return id;
}

/* Script native "umnReturn": takes no arguments; calls uwamonoDetachCurrent and returns 1. */
int umnReturn(SbThread *context)
{
    if (VM_PLUGIN_CNT_GET(context) != 0) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    uwamonoDetachCurrent();
    return 1;
}


int ptSetArray(SbThread *context)
{
    int memberId;
    int destSlot;
    int srcSlot;

    if (VM_PLUGIN_CNT_GET(context) != 2) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    memberId = vmArgGet(context, 1);
    destSlot = vmArgGet(context, 2);
    srcSlot = ptFindSlot(memberId);
    if (srcSlot == -1) {
        return 0;
    }
    if (srcSlot >= 7) {
        return 0;
    }
    if (destSlot < ptState.counts[0]) {
        if (destSlot >= 0) {
            if (destSlot == srcSlot) {
                return 1;
            }
            ptSwapSlots(destSlot, srcSlot);
            ptRefresh();
            ptLoadPartyModels();
            ptRefreshCurrent();
            return 1;
        }
    }
    return 0;
}

int ptTent(SbThread *context)
{
    if (VM_PLUGIN_CNT_GET(context) != 0) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    battleSlotsRestoreAll();
    return 1;
}

extern SbNativeMethod menuNativeTable[12];
extern unsigned char menuModuleName[];

/* Registers the menu native table menuNativeTable (ptInit ... umnReturn) under the module descriptor menuModuleName ("menu"). */
void menuRegisterNatives(void) {
    sbNativeOwnerAdd(menuModuleName, menuNativeTable);
}

/* Terminated by a zero entry. Passed to sbNativeOwnerAdd by menuRegisterNatives. */

SbNativeMethod menuNativeTable[12] = {
    { "ptInit", ptInit },
    { "ptClear", ptClear },
    { "ptJoin", ptJoin },
    { "ptLeave", ptLeave },
    { "ptChangeMode", ptChangeMode },
    { "ptSet", ptSet },
    { "ptSetPilot", ptSetPilot },
    { "ptGetLeader", ptGetLeader },
    { "ptSetArray", ptSetArray },
    { "ptTent", ptTent },
    { "umnReturn", umnReturn },
    { 0, 0 },
};

unsigned char menuModuleName[8] = "menu";
