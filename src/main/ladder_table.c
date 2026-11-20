#include "common.h"
#include "shared.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/ladder_table.h"
#include "main/uwamono_record.h"

/* Clears every ladder slot to its unused state and selects slot 0. */
void ladderInitTable(void)
{
    int slotIndex;
    int rungIndex;
    LadderSlot *slot;

    uwamonoLadderState.state = 0;
    for (slotIndex = 0; slotIndex < LADDER_SLOT_COUNT; slotIndex++) {
        slot = &ladderSlots[slotIndex];
        slot->id = -1;
        slot->slotIndex = -1;
        slot->position.x = slot->position.y = slot->position.z = 0.0f;
        slot->position.w = 1.0f;
        slot->height = slot->angle = 0.0f;
        slot->rungCount = 0;
        slot->footBlocked = 0;
        for (rungIndex = 0; rungIndex < LADDER_MAX_RUNGS; rungIndex++) {
            slot->rungs[rungIndex] = 0.0f;
            slot->rungExits[rungIndex] = 0;
        }
    }
}

/* Returns the index of the first slot whose id is id, or -1 when no slot holds it. */
int ladderFindSlot(short id)
{
    int slotIndex;
    LadderSlot *slot;

    slot = ladderSlots;
    for (slotIndex = 0; slotIndex < LADDER_SLOT_COUNT; slotIndex++) {
        if (slot->id == id) {
            return slotIndex;
        }
        slot += 1;
    }
    return -1;
}

/* Returns how many slots, counted from slot 0, are in use before the first unused one. */
int ladderCountUsedPrefix(void)
{
    int usedCount;
    LadderSlot *slot;

    usedCount = 0;
    if (ladderSlots[0].id != -1) {
        slot = ladderSlots;
        for (;;) {
            usedCount += 1;
            slot += 1;
            if (usedCount < LADDER_SLOT_COUNT && slot->id != -1) {
                continue;
            }
            break;
        }
    }
    return usedCount;
}


/* Fills the slot of the ladder at the first unused position from the entry key and the foot data. */
int ladderSetLadder(SbThread *thread)
{
    LadderSlot *slot;
    float *angleHeight;
    char *slotBytes;
    int poseOffset;
    int usedCount;
    int ladderId;
    int entryKey;
    float height;

    if (VM_PLUGIN_CNT_GET(thread) != 3) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    usedCount = ladderCountUsedPrefix();
    ladderId = vmArgGet(thread, 1);
    entryKey = vmArgGet(thread, 2);
    height = (float) vmArgGet(thread, 3) / 100.0f;
    if (entryKey == -1) {
        ladderSlots[usedCount].id = 0;
        return 1;
    }
    /* The angle/height pair is addressed as a byte offset into the table: a field address gets folded differently and changes the code. */
    slotBytes = (char *)ladderSlots;
    slot = &ladderSlots[usedCount];
    poseOffset = usedCount * sizeof(LadderSlot) + 32;
    angleHeight = (float *)(slotBytes + poseOffset);
    slot->slotIndex = usedCount;
    slot->id = ladderId;
    angleHeight[1] = height;
    slot->key = entryKey;
    *angleHeight = uwamonoGetEntryPose(entryKey, &ladderSlots[usedCount].position.x);
    return 1;
}

/* Adds a rung to the ladder of the entry key: its height and the exits it allows. */
int ladderSetEntrance(SbThread *thread)
{
    int slotIndex;
    float rungHeight;
    int leftExit;
    int rightExit;
    int rungExitBits;
    int rungIndex;

    if (VM_PLUGIN_CNT_GET(thread) != 4) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    slotIndex = ladderFindSlot(vmArgGet(thread, 1));
    rungHeight = (float) vmArgGet(thread, 2) / 100.0f;
    leftExit = (vmArgGet(thread, 3) == 1) ? RUNG_EXIT_LEFT : 0;
    rightExit = (vmArgGet(thread, 4) == 1) ? RUNG_EXIT_RIGHT : 0;
    rungExitBits = leftExit | rightExit;
    rungIndex = ladderSlots[slotIndex].rungCount;
    ladderSlots[slotIndex].rungs[rungIndex] = rungHeight;
    ladderSlots[slotIndex].rungCount += 1;
    ladderSlots[slotIndex].rungExits[rungIndex] = rungExitBits;
    return 1;
}
