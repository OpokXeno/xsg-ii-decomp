#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/pt_member.h"


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
