#include "common.h"
#include "main/pad.h"
#include "main/sound_driver.h"
#include "main/game_camera.h"
#include "main/pt_member.h"
#include "main/txd_table_entry.h"
#include "main/msg_state.h"
#include "main/game_data.h"

#include "shared.h"
#include "main/menu_scene.h"
#include "main/xgl_flags.h"
#include "main/tya_display_setting.h"
#include "ov01/calc.h"
#include "main/xgl_cd.h"
#include "main/msg_balance.h"
#include "main/save_party_init.h"
#include "main/menu_selection.h"

/* Member ids from 11 on are back members: they take the back slots 7-9. */
#define BACK_MEMBER_ID 11
/* memberClothIdTable layout (ptGetClothId): the late-story cloth ids of members 1-7 (indexed by member - 1), then
 * the early ones (the same index + MEMBER_CLOTH_EARLY_SHIFT), then the mid-story id of member 1 followed by the ids of
 * the back members 11-13 (indexed by id + MEMBER_CLOTH_BACK_SHIFT). */
#define MEMBER_CLOTH_EARLY_SHIFT 7
#define MEMBER_CLOTH_MID_FIRST 14
#define MEMBER_CLOTH_BACK_SHIFT 4

/* The member the menu shows: front slot menuSlot, or in split mode back
 * slot 7 + menuSlot for 0-2 and roster entry menuSlot - 3 after them. */
PartyMember *ptGetMenuMember(void)
{
    PartyMember *member;

    if (ptState.splitMode == 1) {
        if (ptState.menuSlot < 3) {
            member = ptState.slots[ptState.menuSlot + PARTY_FRONT_SLOTS];
        } else {
            member = ptState.roster[ptState.menuSlot - 3];
        }
    } else {
        member = ptState.slots[ptState.menuSlot];
    }
    return member;
}

/* The battle state record of the member the menu shows. */
StatBlock *ptGetMenuMemberState(void) {
    return calcTotalParaMenu(ptGetMenuMember()->id);
}

extern int uwamonoCurrentIndex;

/* The index of the slot holding member id, or -1. */
int ptFindSlot(int id)
{
    PartyMember *record;
    int found;
    int i;

    found = -1;
    for (i = 0; i < PARTY_SLOTS; i++) {
        record = ptState.slots[i];
        if (record != 0 && record->id == id) {
            found = i;
            break;
        }
    }
    return found;
}

PartyMember *ptFindRecord(int id) {
    PartyMember *record;
    PartyMember *found;
    int misses;
    PartyMember **slot;
    PartyState *party;

    found = 0;
    misses = 0;
    party = &ptState;
    slot = party->slots;
    for (;;) {
        record = *slot;
        slot++;
        if (record != 0 && record->id == id) {
            found = record;
            break;
        }
        misses++;
        if (misses >= 10) {
            break;
        }
    }
    return found;
}


/* Swaps slots a and b (both 0-9 and occupied). frontIndex follows its member
 * when both are below 3, backIndex when both are 7 or 8; two back slots also
 * swap roster entries a - 7 / b - 7 and a - 4 / b - 4. */
void ptSwapSlots(int a, int b)
{
    PartyMember *member;

    if (a >= PARTY_SLOTS) {
        return;
    }
    if (b >= PARTY_SLOTS) {
        return;
    }
    if (a < 0) {
        return;
    }
    if (b < 0) {
        return;
    }
    if (ptState.slots[a] == 0) {
        return;
    }
    if (ptState.slots[b] == 0) {
        return;
    }
    if (a < 3 && b < 3) {
        if (ptState.frontIndex == a) {
            ptState.frontIndex = b;
        } else if (ptState.frontIndex == b) {
            ptState.frontIndex = a;
        }
    }
    member = ptState.slots[a];
    ptState.slots[a] = ptState.slots[b];
    ptState.slots[b] = member;
    if (a >= PARTY_FRONT_SLOTS && b >= PARTY_FRONT_SLOTS) {
        if (a < 9 && b < 9) {
            if (ptState.backIndex + PARTY_FRONT_SLOTS == a) {
                ptState.backIndex = b - PARTY_FRONT_SLOTS;
            } else if (ptState.backIndex + PARTY_FRONT_SLOTS == b) {
                ptState.backIndex = a - PARTY_FRONT_SLOTS;
            }
        }
        ptSwapRosterEntries(a - 7, b - 7);
        ptSwapRosterEntries(a - 4, b - 4);
    }
}

/* Swaps the second roster's slots a and b (bounds-checked). */
void ptSwapRosterEntries(int a, int b)
{
    PartyMember *member;

    if (a < PARTY_SLOTS) {
        if (b < PARTY_SLOTS) {
            if (a >= 0) {
                if (b >= 0) {
                    member = ptState.roster[a];
                    ptState.roster[a] = ptState.roster[b];
                    ptState.roster[b] = member;
                }
            }
        }
    }
}

extern PartyMember ptMemberRecords[];
/* ptAddMember and ptRefresh address ptProgressRecord by its symbol, as the original does; the save
 * functions go through ptState.progress. */

/* Puts member id (unless a slot already holds it) into the first free slot
 * of its group: ids 11 and up go to the back slots 7-9, the others to the
 * front slots 0-6. Its bit in ptProgressRecord.memberMask is set and the counts kept up. */
void ptAddMember(int id)
{
    PartyMember *record;
    int split;
    int slot;
    int i;

    if (ptFindRecord(id) != 0) {
        return;
    }
    record = 0;
    for (i = 0; i < PARTY_SLOTS; i++) {
        if (ptMemberRecords[i].id == id) {
            record = &ptMemberRecords[i];
            break;
        }
    }
    split = ptState.splitMode;
    if (split == 1) {
        ptLeaveSplit();
    }
    if (record != 0) {
        slot = -1;
        if (id >= BACK_MEMBER_ID) {
            for (i = PARTY_FRONT_SLOTS; i < PARTY_SLOTS; i++) {
                if (ptState.slots[i] == 0) {
                    ptState.counts[2]++;
                    ptProgressRecord.memberMask |= 1 << (id - 4);
                    slot = i;
                    break;
                }
            }
        } else {
            for (i = 0; i < PARTY_FRONT_SLOTS; i++) {
                if (ptState.slots[i] == 0) {
                    ptState.counts[1]++;
                    ptProgressRecord.memberMask |= 1 << (id - 1);
                    slot = i;
                    break;
                }
            }
        }
        if (slot != -1) {
            ptState.slots[slot] = record;
            ptState.counts[0]++;
        }
        ptRefresh();
    }
    if (split == 1) {
        ptEnterSplit();
    }
}

/* Removes member id from its slot, closing the gap within its group (front
 * slots 0-6 or back slots 7-9) and keeping frontIndex/backIndex on the same
 * members. Split mode is left for the move and restored afterwards. */
void ptRemoveMember(int id)
{
    int split;
    int i;
    int k;

    if (ptFindRecord(id) == 0) {
        return;
    }
    split = ptState.splitMode;
    if (split == 1) {
        ptLeaveSplit();
    }
    for (i = 0; i < PARTY_SLOTS; i++) {
        if (ptState.slots[i] != 0 && ptState.slots[i]->id == id) {
            /* The original compares the slot index unsigned (sltiu). */
            if ((unsigned int)i < PARTY_FRONT_SLOTS) {
                if (ptState.frontIndex == i) {
                    ptState.frontIndex = 0;
                }
                for (k = i; k < 6; k++) {
                    ptState.slots[k] = ptState.slots[k + 1];
                    if (ptState.frontIndex == k + 1) {
                        ptState.frontIndex = k;
                    }
                }
            } else {
                if (ptState.backIndex + PARTY_FRONT_SLOTS == i) {
                    ptState.backIndex = 0;
                }
                for (k = i; k < 9; k++) {
                    ptState.slots[k] = ptState.slots[k + 1];
                    /* backIndex counts from slot 7: slot k + 1 is backIndex k - 6. */
                    if (ptState.backIndex == k - 6) {
                        ptState.backIndex = k - 7;
                    }
                }
            }
            ptState.slots[k] = 0;
            ptState.counts[0]--;
            if (id >= BACK_MEMBER_ID) {
                ptState.counts[2]--;
            } else {
                ptState.counts[1]--;
            }
            ptRefresh();
            break;
        }
    }
    if (split == 1) {
        ptEnterSplit();
    }
}

void ptResetSlots(void)
{
    int i;

    for (i = 9; i >= 0; i--) {
        ptState.slots[i] = 0;
    }
    ptState.counts[2] = 0;
    ptState.counts[1] = 0;
    ptState.counts[0] = 0;
    ptState.frontIndex = 0;
    ptState.backIndex = 0;
    ptRefresh();
}

void ptSetCurrentSlot(int slot)
{
    if (ptState.slots[slot] == 0) {
        return;
    }
    if (slot >= PARTY_FRONT_SLOTS) {
        ptState.backIndex = slot - PARTY_FRONT_SLOTS;
        return;
    }
    ptState.frontIndex = slot;
}

void ptSelectMember(int id) {
    int slot;

    slot = ptFindSlot(id);
    if (slot != -1) {
        ptSetCurrentSlot(slot);
    }
}

int ptGetCurrentSlot(void)
{
    if (ptState.splitMode == 1) {
        return ptState.backIndex + PARTY_FRONT_SLOTS;
    }
    return ptState.frontIndex;
}

int ptGetCurrentId(void)
{
    if (ptState.splitMode == 1) {
        return ptState.slots[ptState.backIndex + PARTY_FRONT_SLOTS]->id;
    }
    return ptState.slots[ptState.frontIndex]->id;
}

/* Index 0-6 of the front slot holding the member that roster entry secondIndex
 * names, or -1 when no front slot holds it. */
int ptFindFrontSlot(int secondIndex) {
    PartyMember **frontSlot;
    PartyMember *secondRecord;
    int frontIndex;
    int found;

    found = -1;
    secondRecord = ptState.roster[secondIndex];
    frontSlot = ptState.slots;
    frontIndex = 0;
    for (;;) {
        if (secondRecord == 0) {
            frontIndex++;
        } else if (*frontSlot == secondRecord) {
            found = frontIndex;
            break;
        } else {
            frontIndex++;
        }
        frontSlot++;
        if (frontIndex >= PARTY_FRONT_SLOTS) {
            break;
        }
    }
    return found;
}

/* Index of the second roster slot holding id, or -1 when none does. */
int ptFindRosterEntry(int id)
{
    PartyMember *record;
    int found;
    int i;

    found = -1;
    for (i = 0; i < PARTY_SLOTS; i++) {
        record = ptState.roster[i];
        if (record != 0 && record->id == id) {
            found = i;
            break;
        }
    }
    return found;
}

#define STORY_PROGRESS_FLAG 0x191

extern int memberClothProgressTable[];
extern int memberClothIdTable[18];

/* The model (cloth) id of member id: ptApplyCurrentModel passes it to
 * uwamonoChangePlayerModel, ptLoadPartyModels to resPlayerSlotsLoadModels, and
 * ptRefresh keeps it in ptStatusBlock's clothIds. Ids 11 and up (back members) read
 * the last three entries of memberClothIdTable; the others compare the story-progress flag with a per-member
 * threshold, member 1 with an extra threshold/table pair. */
int ptGetClothId(int id)
{
    int progress;

    if (id >= BACK_MEMBER_ID) {
        return memberClothIdTable[id + MEMBER_CLOTH_BACK_SHIFT];
    }
    progress = xglFlagsGet8(STORY_PROGRESS_FLAG);
    id--;
    if (id != 0) {
        if (progress >= memberClothProgressTable[id]) {
            return memberClothIdTable[id];
        }
        return memberClothIdTable[id + MEMBER_CLOTH_EARLY_SHIFT];
    }
    if (progress >= memberClothProgressTable[7]) {
        if (progress >= memberClothProgressTable[0]) {
            return memberClothIdTable[0];
        }
        return memberClothIdTable[MEMBER_CLOTH_MID_FIRST];
    }
    return memberClothIdTable[MEMBER_CLOTH_EARLY_SHIFT];
}


void ptRefreshCurrent(void) {
    int flag;

    flag = 0;
    if (uwamonoCurrentIndex != -1) {
        if (!(uwamonoRecords[uwamonoCurrentIndex].form < 5.0f)) {
            flag = 1;
        }
        flag = flag == ptState.splitMode;
    }
    if (flag != 0) {
        uwamonoLoadCurrent();
    }
    ptApplyCurrentModel();
    if (flag != 0) {
        uwamonoStoreCurrent();
    }
}


/* Passes the current member's ptGetClothId value to uwamonoChangePlayerModel;
 * in split mode (the back member) with bit 31 set. */
void ptApplyCurrentModel(void)
{
    PartyMember *record;
    int request;

    if (ptState.splitMode == 1) {
        record = ptState.slots[ptState.backIndex + PARTY_FRONT_SLOTS];
        if (record != 0) {
            request = ptGetClothId(record->id);
            uwamonoChangePlayerModel(request | 0x80000000);
        }
        return;
    }
    record = ptState.slots[ptState.frontIndex];
    if (record != 0) {
        request = ptGetClothId(record->id);
        uwamonoChangePlayerModel(request);
        return;
    }
}


void ptPlaceMember(int id) {
    int slot;

    /* ids 11 and up go to the back slot 7 in split mode; the others to front slot 0. */
    ptAddMember(id);
    if (id >= BACK_MEMBER_ID) {
        ptEnterSplit();
        slot = ptFindSlot(id);
        ptSwapSlots(PARTY_FRONT_SLOTS, slot);
        ptSetCurrentSlot(PARTY_FRONT_SLOTS);
    } else {
        ptLeaveSplit();
        slot = ptFindSlot(id);
        ptSwapSlots(0, slot);
        ptSetCurrentSlot(0);
    }
    ptLoadPartyModels();
    ptRefreshCurrent();
}

/* Copies each front member (slots 0-6) that the roster does not hold yet
 * into the next free roster entry 3-5 whose back slot (entry + 4) is filled;
 * returns when no such entry is left. */
void ptFillCopilots(void)
{
    int i;
    int entry;

    entry = 3;
    for (i = 0; i < PARTY_FRONT_SLOTS; i++) {
        if (ptState.slots[i] != 0 && ptFindRosterEntry(ptState.slots[i]->id) == -1) {
            while (entry < 6) {
                if (ptState.roster[entry] != 0 || ptState.slots[entry + 4] == 0) {
                    entry++;
                    continue;
                }
                break;
            }
            if (entry < 6) {
                ptState.roster[entry] = ptState.slots[i];
            } else {
                return;
            }
        }
    }
}

/* Copies every front member that the roster does not hold yet into the free
 * roster entries 6-9. */
void ptFillReserve(void)
{
    int i;
    int entry;

    entry = 6;
    for (i = 0; i < PARTY_FRONT_SLOTS; i++) {
        if (ptState.slots[i] != 0 && ptFindRosterEntry(ptState.slots[i]->id) == -1) {
            for (;;) {
                if (entry < 10 && ptState.roster[entry] != 0) {
                    entry++;
                } else {
                    break;
                }
            }
            if (entry >= 10) {
                break;
            }
            ptState.roster[entry] = ptState.slots[i];
        }
    }
}

/* {2, 6, 7}: the front member id paired with each of the back ids 11-13
 * (three bytes of save_party_init.c's .sdata). */
extern unsigned char backMemberCounterpartIdTable[3];

/* Enters split mode when the party has back members. Roster entries 0-2 take
 * the backMemberCounterpartIdTable counterpart of the member in back slots 7-9. Entries 3-9
 * drop a member that left the party or that entries 0-2 now hold; entries
 * 3-5 are also dropped when their back slot (entry + 4) is empty. Then
 * ptFillCopilots and ptFillReserve fill the free entries. */
void ptEnterSplit(void)
{
    int i;
    int id;
    PartyMember *member;

    if (ptState.splitMode == 1) {
        return;
    }
    if (ptState.counts[2] != 0) {
        for (i = 0; i < 3; i++) {
            member = ptState.slots[i + PARTY_FRONT_SLOTS];
            if (member != 0) {
                id = member->id;
                ptState.roster[i] = ptFindRecord(backMemberCounterpartIdTable[id - BACK_MEMBER_ID]);
            } else {
                ptState.roster[i] = 0;
            }
        }
        for (i = 3; i < 6; i++) {
            if (ptState.roster[i] != 0) {
                if (ptFindRecord(ptState.roster[i]->id) == 0) {
                    ptState.roster[i] = 0;
                } else if (ptFindRosterEntry(ptState.roster[i]->id) < 3) {
                    ptState.roster[i] = 0;
                } else if (ptState.slots[i + 4] == 0) {
                    ptState.roster[i] = 0;
                }
            }
        }
        for (i = 6; i < 10; i++) {
            if (ptState.roster[i] != 0) {
                if (ptFindRecord(ptState.roster[i]->id) == 0) {
                    ptState.roster[i] = 0;
                } else if (ptFindRosterEntry(ptState.roster[i]->id) < 3) {
                    ptState.roster[i] = 0;
                }
            }
        }
        ptFillCopilots();
        ptFillReserve();
        ptState.splitMode = 1;
        ptRefresh();
    }
}

void ptLeaveSplit(void)
{
    if (ptState.splitMode == 0) {
        return;
    }
    if (ptState.counts[1] != 0) {
        ptState.splitMode = 0;
        ptRefresh();
    }
}

void ptResetSplitReserve(void)
{
    int i;
    PartyState *party;
    PartyMember **slots;

    /* Roster entries 3-5. */
    party = &ptState;
    slots = &party->roster[3];
    for (i = 0; i < 3; i++) {
        *slots = 0;
        slots++;
    }
    ptFillReserve();
}

/* The id in roster entry slot - PARTY_FRONT_SLOTS (0-2) for a back member, else 0. */
int ptGetMainPilotId(int id)
{
    int slot;
    PartyMember *member;

    if (id < BACK_MEMBER_ID) {
        return 0;
    }
    slot = ptFindSlot(id);
    if (slot < PARTY_FRONT_SLOTS) {
        return 0;
    }
    member = ptState.roster[slot - PARTY_FRONT_SLOTS];
    if (member != 0) {
        return member->id;
    }
    return 0;
}

/* The id in roster entry slot - 4 (3-5) for a back member, else 0: the
 * member that menu_draw.c's special-attack list treats as its copilot. */
int ptGetCopilotId(int id)
{
    int slot;
    PartyMember *member;

    if (id < BACK_MEMBER_ID) {
        return 0;
    }
    slot = ptFindSlot(id);
    if (slot < PARTY_FRONT_SLOTS) {
        return 0;
    }
    member = ptState.roster[slot - 4];
    if (member != 0) {
        return member->id;
    }
    return 0;
}

/* 1 when every occupied back slot 7-9 has its roster entry 3-5 filled. */
int ptAllCopilotsAssigned(void)
{
    int result;
    int i;

    result = 1;
    for (i = 0; i < 3; i++) {
        if (ptState.slots[i + PARTY_FRONT_SLOTS] == 0) {
            continue;
        }
        if (ptState.roster[i + 3] != 0) {
            continue;
        }
        result = 0;
        break;
    }
    return result;
}

int ptGroupCount(void)
{
    int count;

    if (ptState.splitMode == 1) {
        count = ptState.counts[2];
        if (count >= 2) {
            count = 2;
        }
    } else {
        count = ptState.counts[1];
        if (count >= 3) {
            count = 3;
        }
    }
    return count;
}

void *memset(void *dest, int value, unsigned int size);
static PartyStatusBlock ptStatusBlock;

void ptResetStatusBlock(void)
{
    int *clothIds;
    int i;

    memset(&ptStatusBlock, 0, sizeof(ptStatusBlock));
    clothIds = ptStatusBlock.clothIds;
    for (i = 0; i < 7; i++) {
        clothIds[i] = memberClothIdTable[i];
    }
}

extern PartyMemberStatus ptMemberStatuses[10];

/* Rebuilds ptStatusBlock from the party and pairs the members for the
 * current split mode. */
void ptRefresh(void)
{
    PartyStatusBlock *block;
    PartyMember *leader;
    int progress;
    int i;

    block = &ptStatusBlock;
    if (ptState.splitMode == 1) {
        leader = ptState.slots[ptState.backIndex + PARTY_FRONT_SLOTS];
        if (leader != 0) {
            block->leaderId = leader->id;
        } else {
            block->leaderId = 0;
        }
        block->memberIds[2] = 0;
        for (i = 0; i < 2; i++) {
            if (ptState.slots[i + PARTY_FRONT_SLOTS] != 0) {
                block->memberIds[i] = ptState.slots[i + PARTY_FRONT_SLOTS]->id;
            } else {
                block->memberIds[i] = 0;
            }
        }
        block->presentMask = 0;
        for (i = 0; i < 3; i++) {
            if (ptState.slots[i + PARTY_FRONT_SLOTS] != 0) {
                block->presentMask |= 1 << (ptState.slots[i + PARTY_FRONT_SLOTS]->id - 4);
            }
        }
        for (i = 0; i < 3; i++) {
            if (ptState.slots[i + PARTY_FRONT_SLOTS] != 0) {
                if (ptState.roster[i] != 0) {
                    battleSlotLinkedSlotSet(ptState.slots[i + PARTY_FRONT_SLOTS]->id, 0, ptState.roster[i]->id);
                } else {
                    battleSlotLinkedSlotSet(ptState.slots[i + PARTY_FRONT_SLOTS]->id, 0, 0);
                }
                if (ptState.roster[i + 3] != 0) {
                    battleSlotLinkedSlotSet(ptState.slots[i + PARTY_FRONT_SLOTS]->id, 1, ptState.roster[i + 3]->id);
                } else {
                    battleSlotLinkedSlotSet(ptState.slots[i + PARTY_FRONT_SLOTS]->id, 1, 0);
                }
            }
        }
    } else {
        leader = ptState.slots[ptState.frontIndex];
        if (leader != 0) {
            block->leaderId = leader->id;
        } else {
            block->leaderId = 0;
        }
        for (i = 0; i < 3; i++) {
            if (ptState.slots[i] != 0) {
                block->memberIds[i] = ptState.slots[i]->id;
            } else {
                block->memberIds[i] = 0;
            }
        }
        block->presentMask = 0;
        for (i = 0; i < PARTY_FRONT_SLOTS; i++) {
            if (ptState.slots[i] != 0) {
                block->presentMask |= 1 << (ptState.slots[i]->id - 1);
            }
        }
    }
    block->memberMask = ptProgressRecord.memberMask;
    for (i = 0; i < 7; i++) {
        block->clothIds[i] = ptGetClothId(i + 1);
    }
    progress = xglFlagsGet8(STORY_PROGRESS_FLAG);
    for (i = 9; i >= 0; i--) {
        ptMemberStatuses[i].memberIndex = i;
    }
    /* From story progress 0x2B entries 2 and 5 take the indices 11 and 12,
     * from 0x57 entry 1 the index 10, after the others. */
    i = 10;
    if (progress >= 0x57) {
        ptMemberStatuses[1].memberIndex = i;
    }
    if (progress >= 0x2B) {
        ptMemberStatuses[2].memberIndex = 11;
        ptMemberStatuses[5].memberIndex = 12;
    }
    if (xglFlagsGet8(0x1EE) > 0) {
        /* The original leaves through dataSkillLearnSet in cases 1 and 2 (a jump)
         * and calls it in case 3. */
        switch (xglFlagsGet2(0x1D6)) {
        case 1:
            dataSkillLearnSet(3, 0x37);
            return;
        case 2:
            dataSkillLearnSet(3, 0x38);
            return;
        case 3:
            dataSkillLearnSet(3, 0x39);
            break;
        }
    }
}

/* One row of the table ptFlagCombinationIndex searches: the four two-bit flag values
 * it is chosen for; a row whose first value is 0 ends the table. */
typedef struct FlagCombination {
    unsigned char values[4];
} FlagCombination;

extern FlagCombination flagCombinationTable[];

/* 1-based index of the last row of flagCombinationTable that matches the flags
 * 0x1DE, 0x1E1, 0x1D6 and 0x1DB, or 0 when none does. */
int ptFlagCombinationIndex(void)
{
    int flags[4];
    int found;
    int i;

    found = 0;
    flags[0] = xglFlagsGet2(0x1DE);
    flags[1] = xglFlagsGet2(0x1E1);
    flags[2] = xglFlagsGet2(0x1D6);
    flags[3] = xglFlagsGet2(0x1DB);
    for (i = 0; flagCombinationTable[i].values[0] != 0; i++) {
        if (flagCombinationTable[i].values[0] == flags[0] && flagCombinationTable[i].values[1] == flags[1]
            && flagCombinationTable[i].values[2] == flags[2] && flagCombinationTable[i].values[3] == flags[3]) {
            found = i + 1;
        }
    }
    return found;
}


/* Loads the models of the three party members on screen: back slots 7-9 in
 * split mode, front slots 0-2 otherwise. */
void ptLoadPartyModels(void)
{
    int modelId0;
    int modelId1;
    int modelId2;

    modelId0 = 0;
    modelId1 = 0;
    modelId2 = 0;
    ptRefresh();
    if (ptState.splitMode == 1) {
        if (ptState.slots[7] != 0) {
            modelId0 = ptGetClothId(ptState.slots[7]->id);
        }
        if (ptState.slots[8] != 0) {
            modelId1 = ptGetClothId(ptState.slots[8]->id);
        }
        if (ptState.slots[9] != 0) {
            modelId2 = ptGetClothId(ptState.slots[9]->id);
        }
    } else {
        if (ptState.slots[0] != 0) {
            modelId0 = ptGetClothId(ptState.slots[0]->id);
        }
        if (ptState.slots[1] != 0) {
            modelId1 = ptGetClothId(ptState.slots[1]->id);
        }
        if (ptState.slots[2] != 0) {
            modelId2 = ptGetClothId(ptState.slots[2]->id);
        }
    }
    resPlayerSlotsLoadModels(modelId0, modelId1, modelId2);
}

extern SaveBattleSlots battleSlotTable;
extern SaveSlotRecords slotRecordTable;
extern SaveShortTable dataBoxQuantityTable1;
extern SaveShortTable dataBoxQuantityTable2;
extern SaveShortTable dataBoxQuantityTable3;
extern SaveShortTable dataBoxQuantityTable4;
extern int startStockCount;
extern SaveWords4 brokenEnemyBuf;
extern QuestFlagBlock questFlags;
extern CitizenFlagBlock citizenFlags;
extern SegFlagBlock segAddrOnFlags;

/* The image checksum: every byte of a copy of the image (its checksum field
 * zeroed) weighted by its 1-based position, plus 0x793 per byte. */
void saveChecksum(struct SaveData *image, long long *sum)
{
    struct SaveData copy;
    unsigned char *byte;
    int i;

    copy = *image;
    copy.checksum = 0;
    *sum = 0;
    byte = (unsigned char *)&copy;
    for (i = 0; i < sizeof(struct SaveData); i++) {
        *sum = *sum + *byte++ * (long long)(i + 1) + 0x793;
    }
}


/* Fills the save image: play time, the global blocks, the party and the
 * counters, then the checksum. The blocks are copied through their address
 * (the original addresses them as SaveData+offset constants, not from the
 * SaveData base). */
#define MAP_ID_GAME_END 0x1B91 /* game_over.c's name; entering it sets the progress record's mapFlag */
#define SAVE_VERSION 200

void saveWriteImage(void)
{
    XglClock now;
    long long sum;
    StatBlock *leaderInfo;
    SaveParty *party;
    int i;

    ptRefresh();
    xglClockRead(&now);
    xglClockUInt2DayTime(&SaveData.playTime, xglClockDayTime2UInt(&now) - ptState.progress->playTime);
    *(SaveBattleSlots *)(u8 *)&SaveData.battleSlots = battleSlotTable;
    *(SaveSlotRecords *) (unsigned char *) &SaveData.slotRecords = slotRecordTable;
    *(SaveShortTable *) (unsigned char *) &SaveData.shortTables[0] = dataBoxQuantityTable1;
    *(SaveShortTable *) (unsigned char *) &SaveData.shortTables[1] = dataBoxQuantityTable2;
    *(SaveShortTable *) (unsigned char *) &SaveData.shortTables[2] = dataBoxQuantityTable3;
    *(SaveShortTable *) (unsigned char *) &SaveData.shortTables[3] = dataBoxQuantityTable4;
    *(SaveWords4 *) (unsigned char *) &SaveData.brokenEnemyBuf = brokenEnemyBuf;
    SaveData.startStockCount = startStockCount;
    party = &SaveData.party;
    if (gameWork.mapId == MAP_ID_GAME_END) {
        ptState.progress->mapFlag = 1;
    }
    party->mapFlag = ptState.progress->mapFlag;
    party->memberMask = ptState.progress->memberMask;
    party->splitMode = ptState.splitMode;
    party->frontIndex = ptState.frontIndex;
    party->backIndex = ptState.backIndex;
    party->counts[0] = ptState.counts[0];
    party->counts[1] = ptState.counts[1];
    party->counts[2] = ptState.counts[2];
    party->mapId = ptState.progress->mapId;
    party->position[0] = ptState.progress->position[0];
    party->position[1] = ptState.progress->position[1];
    party->savedSplitMode = ptState.progress->savedSplitMode;
    party->eligibility = ptState.progress->eligibility;
    for (i = 0; i < PARTY_SLOTS; i++) {
        if (ptState.slots[i] != 0) {
            party->slotIds[i] = ptState.slots[i]->id;
        } else {
            party->slotIds[i] = 0;
        }
        if (ptState.roster[i] != 0) {
            party->rosterIds[i] = ptState.roster[i]->id;
        } else {
            party->rosterIds[i] = 0;
        }
        party->takenBits[i] = ptMemberTakenTable[i];
    }
    if (ptState.splitMode == 0) {
        PartyMember *leader = ptState.slots[ptState.frontIndex];

        if (leader != 0) {
            party->leaderId = leader->id;
            leaderInfo = calcTotalParaMenu(leader->id);
            party->leaderLevel = leaderInfo->expLevel;
        } else {
            party->leaderId = 0;
            party->leaderLevel = 0;
        }
        for (i = 0; i < PARTY_FRONT_SLOTS; i++) {
            if (ptState.slots[i] != 0) {
                party->slotValues[i] = ptState.slots[i]->status->memberIndex + 1;
            } else {
                party->slotValues[i] = 0;
            }
        }
    } else {
        PartyMember *leader = ptState.slots[ptState.backIndex + PARTY_FRONT_SLOTS];

        if (leader != 0) {
            party->leaderId = leader->id;
            leaderInfo = calcTotalParaMenu(leader->id);
            party->leaderLevel = leaderInfo->expLevel;
        } else {
            party->leaderId = 0;
            party->leaderLevel = 0;
        }
        for (i = 0; i < PARTY_BACK_SLOTS; i++) {
            if (ptState.slots[i + PARTY_FRONT_SLOTS] != 0) {
                party->slotValues[i] = ptState.slots[i + PARTY_FRONT_SLOTS]->status->memberIndex + 1;
            } else {
                party->slotValues[i] = 0;
            }
        }
        for (; i < PARTY_FRONT_SLOTS; i++) {
            party->slotValues[i] = 0;
        }
    }
    *(GameWork *) (unsigned char *) &SaveData.game = gameWork;
    *(QuestFlagBlock *) (unsigned char *) &SaveData.questFlags = questFlags;
    *(CitizenFlagBlock *) (unsigned char *) &SaveData.citizenFlags = citizenFlags;
    *(SegFlagBlock *) (unsigned char *) &SaveData.segFlags = segAddrOnFlags;
    SaveData.balance = msgBalanceGet();
    SaveData.saveCount++;
    SaveData.version = SAVE_VERSION;
    xglClockRead(&SaveData.saveTime);
    saveChecksum(&SaveData, &sum);
    SaveData.checksum = sum;
}


/* Restores the play time base, the global blocks and the party from the save
 * image: the reverse of saveWriteImage. The blocks are read through a cast of
 * their address: with plain assignments (battleSlotTable = SaveData.battleSlots;
 * ...) gcc computes the later source addresses from one SaveData base register
 * and allocates other registers; through the cast each source is a
 * SaveData+offset constant (lui/addiu), as in the original. */
void saveReadImage(void)
{
    XglClock now;
    unsigned int playTime;
    SaveParty *party;
    int i;

    cursorBufferTablesInit();
    xglClockRead(&now);
    playTime = xglClockDayTime2UInt(&SaveData.playTime);
    ptState.progress->playTime = xglClockDayTime2UInt(&now) - playTime;
    battleSlotTable = *(SaveBattleSlots *) (unsigned char *) &SaveData.battleSlots;
    slotRecordTable = *(SaveSlotRecords *) (unsigned char *) &SaveData.slotRecords;
    dataBoxQuantityTable1 = *(SaveShortTable *) (unsigned char *) &SaveData.shortTables[0];
    dataBoxQuantityTable2 = *(SaveShortTable *) (unsigned char *) &SaveData.shortTables[1];
    dataBoxQuantityTable3 = *(SaveShortTable *) (unsigned char *) &SaveData.shortTables[2];
    dataBoxQuantityTable4 = *(SaveShortTable *) (unsigned char *) &SaveData.shortTables[3];
    brokenEnemyBuf = *(SaveWords4 *) (unsigned char *) &SaveData.brokenEnemyBuf;
    startStockCount = SaveData.startStockCount;
    party = &SaveData.party;
    ptState.progress->mapFlag = party->mapFlag;
    ptState.progress->memberMask = party->memberMask;
    ptState.splitMode = party->splitMode;
    ptState.frontIndex = party->frontIndex;
    ptState.backIndex = party->backIndex;
    ptState.counts[0] = party->counts[0];
    ptState.counts[1] = party->counts[1];
    ptState.counts[2] = party->counts[2];
    ptState.progress->mapId = party->mapId;
    ptState.progress->position[0] = party->position[0];
    ptState.progress->position[1] = party->position[1];
    ptState.progress->savedSplitMode = party->savedSplitMode;
    ptState.progress->eligibility = party->eligibility;
    for (i = 0; i < PARTY_SLOTS; i++) {
        if (party->slotIds[i] != 0) {
            ptState.slots[i] = ptMemberRecordFindById(party->slotIds[i]);
        } else {
            ptState.slots[i] = 0;
        }
        if (party->rosterIds[i] != 0) {
            ptState.roster[i] = ptMemberRecordFindById(party->rosterIds[i]);
        } else {
            ptState.roster[i] = 0;
        }
        ptMemberTakenTable[i] = party->takenBits[i];
    }
    ptRefresh();
}


#define MAP_ID_GAME_END_RESUME 0x9EC /* the map a save made on MAP_ID_GAME_END loads into */

/* Restores gameWork, the quest, citizen and segment flags and the balance
 * from the save image. A save made on MAP_ID_GAME_END resumes on
 * MAP_ID_GAME_END_RESUME with story progress 0x75. CONTROL_SOFT_RESET and
 * the control bits 27, 26 and 20 do not survive a load (runtimeSystem in runtime.c
 * sets 27 and toggles 20). The original copies the flag blocks inline in the
 * order quest, citizen, segment, all addressed from &SaveData.questFlags. A
 * plain memcpy is a call (the build has -fno-builtin), and plain struct
 * assignments (questFlags = SaveData.questFlags; ...) are scheduled by
 * priority whatever their order: the segment block is copied before the
 * citizen block and the control mask moves ahead of the copies. With
 * __builtin_memcpy in the order below the moves come out as in the original.
 * gameWork is read through a cast of its address as in saveReadImage: plain
 * gameWork = SaveData.game; addresses the copy from the SaveData base. */
void saveReadGameWork(void)
{
    int control;
    int balance;

    gameWork = *(GameWork *) (unsigned char *) &SaveData.game;
    gameWork.nextMap = gameWork.mapId;
    if (gameWork.mapId == MAP_ID_GAME_END) {
        gameWork.mapId = gameWork.nextMap = MAP_ID_GAME_END_RESUME;
        xglFlagsSet8(STORY_PROGRESS_FLAG, 0x75);
    }
    control = gameWork.control;
    __builtin_memcpy(&questFlags, &SaveData.questFlags, sizeof(questFlags));
    balance = SaveData.balance;
    __builtin_memcpy(&segAddrOnFlags, &SaveData.segFlags, sizeof(segAddrOnFlags));
    __builtin_memcpy(&citizenFlags, &SaveData.citizenFlags, sizeof(citizenFlags));
    gameWork.control = control & ~(CONTROL_SOFT_RESET | 0x08000000 | 0x04000000 | CONTROL_CFE_NO_FADE);
    msgBalanceSet(balance);
}



void padApplySaveSettings(void)
{
    PadData[0].actuatorMode = PadData[1].actuatorMode = SaveData.actuatorMode;
    soundStereoSet(SaveData.soundMode != 0);
    tyaSetDisplayOffset(SaveData.displayOffsetX, SaveData.displayOffsetY);
}

static int menuMainTxdBuffer[16384 / sizeof(int)];

void *menuMainTxdEntry(int index) {
    return txdTableEntry(menuMainTxdBuffer, index);
}

static int menuTutorialTxdBuffer[8192 / sizeof(int)];

void *menuTutorialTxdEntry(int index) {
    return txdTableEntry(menuTutorialTxdBuffer, index);
}

PartyState *ptGetState(void) {
    return &ptState;
}

PartyMember *ptGetMember(int index) {
    return ptState.slots[index];
}

PartyStatusBlock *ptGetStatusBlock(void) {
    return &ptStatusBlock;
}

/* Data of main/pt_member.c, in address order. */
int memberClothIdTable[18] = {
    65552, 65546, 65538, 65574, 65571, 65564, 65558,
    65553, 65547, 65541, 65575, 65571, 65565, 65560,
    65554, 81922, 81923, 81925
};

int memberClothProgressTable[8] = {
    31, 87, 43, 22, 0, 43, 31, 22
};

FlagCombination flagCombinationTable[82] = {
    { 1, 1, 1, 1 },
    { 2, 2, 2, 2 },
    { 3, 3, 3, 3 },
    { 1, 1, 1, 2 },
    { 1, 1, 1, 3 },
    { 1, 1, 2, 1 },
    { 1, 1, 2, 2 },
    { 1, 1, 2, 3 },
    { 1, 1, 3, 1 },
    { 1, 1, 3, 2 },
    { 1, 1, 3, 3 },
    { 1, 2, 1, 1 },
    { 1, 2, 1, 2 },
    { 1, 2, 1, 3 },
    { 1, 2, 2, 1 },
    { 1, 2, 2, 2 },
    { 1, 2, 2, 3 },
    { 1, 2, 3, 1 },
    { 1, 2, 3, 2 },
    { 1, 2, 3, 3 },
    { 1, 3, 1, 1 },
    { 1, 3, 1, 2 },
    { 1, 3, 1, 3 },
    { 1, 3, 2, 1 },
    { 1, 3, 2, 2 },
    { 1, 3, 2, 3 },
    { 1, 3, 3, 1 },
    { 1, 3, 3, 2 },
    { 1, 3, 3, 3 },
    { 2, 1, 1, 1 },
    { 2, 1, 1, 2 },
    { 2, 1, 1, 3 },
    { 2, 1, 2, 1 },
    { 2, 1, 2, 2 },
    { 2, 1, 2, 3 },
    { 2, 1, 3, 1 },
    { 2, 1, 3, 2 },
    { 2, 1, 3, 3 },
    { 2, 2, 1, 1 },
    { 2, 2, 1, 2 },
    { 2, 2, 1, 3 },
    { 2, 2, 2, 1 },
    { 2, 2, 2, 3 },
    { 2, 2, 3, 1 },
    { 2, 2, 3, 2 },
    { 2, 2, 3, 3 },
    { 2, 3, 1, 1 },
    { 2, 3, 1, 2 },
    { 2, 3, 1, 3 },
    { 2, 3, 2, 1 },
    { 2, 3, 2, 2 },
    { 2, 3, 2, 3 },
    { 2, 3, 3, 1 },
    { 2, 3, 3, 2 },
    { 2, 3, 3, 3 },
    { 3, 1, 1, 1 },
    { 3, 1, 1, 2 },
    { 3, 1, 1, 3 },
    { 3, 1, 2, 1 },
    { 3, 1, 2, 2 },
    { 3, 1, 2, 3 },
    { 3, 1, 3, 1 },
    { 3, 1, 3, 2 },
    { 3, 1, 3, 3 },
    { 3, 2, 1, 1 },
    { 3, 2, 1, 2 },
    { 3, 2, 1, 3 },
    { 3, 2, 2, 1 },
    { 3, 2, 2, 2 },
    { 3, 2, 2, 3 },
    { 3, 2, 3, 1 },
    { 3, 2, 3, 2 },
    { 3, 2, 3, 3 },
    { 3, 3, 1, 1 },
    { 3, 3, 1, 2 },
    { 3, 3, 1, 3 },
    { 3, 3, 2, 1 },
    { 3, 3, 2, 2 },
    { 3, 3, 2, 3 },
    { 3, 3, 3, 1 },
    { 3, 3, 3, 2 },
    { 0, 0, 0, 0 }
};

/* The party state (0x60 bytes), declared in pt_member.h. */
PartyState ptState = { 0 };

unsigned char selectionUsableItemList[256] = { 0 };

unsigned char selectionAccessoryList[256] = { 0 };

/* .bss of this file after its own (0x6ABBC0), read by menu_selection.c. */
u16 selectionFlagTable[44];
