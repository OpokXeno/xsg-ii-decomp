#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/save_party_init.h"
#include "main/pt_member.h"
#include "ov01/calc.h"


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
