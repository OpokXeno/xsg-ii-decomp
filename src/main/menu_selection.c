#include "common.h"
#include "main/save_screen.h"
#include "main/menu_selection.h"
#include "main/txd_table_entry.h"
#include "ov01/calc.h"
#include "ov01/data_box.h"
#include "shared.h"
#include "main/menu_scene.h"
#include "main/pt_member.h"
#include "main/xgl_cd.h"

/* Bits of selectionFlagTable: when an entry can be chosen (selectionEntryIsEligible)
   and what makes it useful for a member (selectionEntryIsUseful). */
enum SelectionFlag {
    SELECTION_FLAG_RESTORE_HP = 0x1,      /* useful while hp differs from maxHp */
    SELECTION_FLAG_RESTORE_EP = 0x2,      /* useful while ep differs from maxEp */
    SELECTION_FLAG_WHOLE_PARTY = 0x4,     /* acts on the whole party; always useful */
    SELECTION_FLAG_RESTRICTED = 0x8,      /* eligible only while ptState.nearbySearchType is 7 or 8 */
    SELECTION_FLAG_DISABLED = 0x10,       /* never eligible */
    SELECTION_FLAG_SPLIT_ONLY = 0x20,     /* eligible only in split mode (without it, only outside) */
    SELECTION_FLAG_ANY_MODE = 0x40,       /* eligible in and outside split mode */
    SELECTION_FLAG_CLASS_POINTS = 0x80,   /* useful while classPoints is below SELECTION_POINTS_MAX */
    SELECTION_FLAG_SKILL_POINTS = 0x100   /* useful while skillPoints is below SELECTION_POINTS_MAX */
};

/* The window entries 30..32, listed first in split mode and skipped by the second scan while
   splitMode is set. */
#define SELECTION_SPLIT_ENTRY_FIRST 30
#define SELECTION_SPLIT_ENTRY_END 33
/* The ptState.nearbySearchType values 7 and 8 under which SELECTION_FLAG_RESTRICTED entries are eligible. */
#define SELECTION_RESTRICTED_STATE_FIRST 7
#define SELECTION_RESTRICTED_STATE_COUNT 2
/* The point ceiling (menu_scene.c clamps classPoints to it). */
#define SELECTION_POINTS_MAX 99999
/* Skill and class points every member starts with, and the skill points added per saved maximum. */
#define SELECTION_START_POINTS 600
#define SELECTION_POINTS_PER_MAXIMUM 30
/* cells[group][slot] once its four subs are taken. */
#define SELECTION_SLOT_ALL_SUBS 0xF
/* The pair records of attackRecordTable (two members each) that selectionPairRecordCount scans. */
#define SELECTION_PAIR_RECORD_FIRST 79
#define SELECTION_PAIR_RECORD_COUNT 16

/* Entry values past the selectionRecordIndex range (0x00..0x6F) that selectionCollectEntries
   appends: SELECTION_ENTRY_EXTRA_FLAG stands for skill SELECTION_EXTRA_FLAG, the entries
   from SELECTION_ENTRY_EXTRA_CODES for Shion's ethers SELECTION_EXTRA_CODE_FIRST..END - 1,
   and SELECTION_ENTRY_NONE is held by nobody. */
#define SELECTION_ENTRY_EXTRA_FLAG 0x70
#define SELECTION_ENTRY_EXTRA_CODES 0x71
#define SELECTION_ENTRY_NONE 0x7F
/* The skill 0x3F that ptStateInit sets for members 2 and 5 when oldDataMaximaCopy reports saved maxima. */
#define SELECTION_EXTRA_FLAG 0x3F
#define SELECTION_EXTRA_CODE_FIRST 0x34
#define SELECTION_EXTRA_CODE_END 0x3A
/* Entry value minus Shion's first ether code (0x3D). */
#define SELECTION_EXTRA_CODE_ENTRY_OFFSET (SELECTION_ENTRY_EXTRA_CODES - SELECTION_EXTRA_CODE_FIRST)

/* unsigned: selectionWindowListDropAt reads it with lbu and decrements with addiu 0xFF. */

void selectionRebuildWindowList(void)
{
    int candidate;
    int count;

    count = 0;
    if (ptState.splitMode != 0)
    {
        candidate = SELECTION_SPLIT_ENTRY_FIRST;
        do
        {
            if (dataBoxChkCategory1(candidate) > 0)
            {
                selectionUsableItemList[count] = candidate;
                count += 1;
            }
            candidate += 1;
        }
        while (candidate < SELECTION_SPLIT_ENTRY_END);
    }
    candidate = 0;
    do
    {
        if ((ptState.splitMode == 0 || (unsigned int) (candidate - SELECTION_SPLIT_ENTRY_FIRST)
             >= SELECTION_SPLIT_ENTRY_END - SELECTION_SPLIT_ENTRY_FIRST) && dataBoxChkCategory1(candidate) > 0)
        {
            selectionUsableItemList[count] = candidate;
            count += 1;
        }
        candidate += 1;
    }
    while (candidate < 255);
    selectionWindowCount = count;
}

/* The dataBoxChkCategory1 count of window entry index; an entry without one is dropped from the list (returns 0). */
int selectionWindowListDropAt(int index)
{
    int i;

    if (dataBoxChkCategory1(selectionUsableItemList[index]) <= 0)
    {
        for (i = index + 1; i < selectionWindowCount; i++)
        {
            selectionUsableItemList[i - 1] = selectionUsableItemList[i];
        }
        selectionWindowCount--;
        return 0;
    }
    return dataBoxChkCategory1(selectionUsableItemList[index]);
}

void selectionFlagTableInit(void)
{
    int i;
    u16 *flags;

    for (i = 41; i >= 0; i--)
    {
        selectionFlagTable[i] = 0;
    }
    flags = selectionFlagTable;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_RESTORE_HP;
    *flags++ = SELECTION_FLAG_RESTORE_HP;
    *flags++ = SELECTION_FLAG_RESTORE_HP;
    *flags++ = SELECTION_FLAG_RESTORE_HP;
    *flags++ = SELECTION_FLAG_RESTORE_EP;
    *flags++ = SELECTION_FLAG_RESTORE_EP;
    *flags++ = SELECTION_FLAG_RESTORE_EP;
    *flags++ = SELECTION_FLAG_RESTORE_HP | SELECTION_FLAG_RESTORE_EP;
    *flags++ = SELECTION_FLAG_RESTORE_HP | SELECTION_FLAG_RESTORE_EP;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_ANY_MODE;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_RESTORE_HP | SELECTION_FLAG_RESTORE_EP | SELECTION_FLAG_WHOLE_PARTY;
    *flags++ = SELECTION_FLAG_SKILL_POINTS;
    *flags++ = SELECTION_FLAG_SKILL_POINTS;
    *flags++ = SELECTION_FLAG_SKILL_POINTS;
    *flags++ = SELECTION_FLAG_SKILL_POINTS;
    *flags++ = SELECTION_FLAG_SKILL_POINTS;
    *flags++ = SELECTION_FLAG_CLASS_POINTS;
    *flags++ = SELECTION_FLAG_CLASS_POINTS;
    *flags++ = SELECTION_FLAG_CLASS_POINTS;
    *flags++ = SELECTION_FLAG_RESTORE_HP | SELECTION_FLAG_RESTORE_EP | SELECTION_FLAG_SPLIT_ONLY;
    *flags++ = SELECTION_FLAG_RESTORE_HP | SELECTION_FLAG_RESTORE_EP | SELECTION_FLAG_SPLIT_ONLY;
    *flags++ = SELECTION_FLAG_RESTORE_HP | SELECTION_FLAG_RESTORE_EP | SELECTION_FLAG_WHOLE_PARTY
               | SELECTION_FLAG_SPLIT_ONLY;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_DISABLED;
    *flags++ = SELECTION_FLAG_WHOLE_PARTY;
    *flags++ = SELECTION_FLAG_WHOLE_PARTY;
    *flags++ = SELECTION_FLAG_WHOLE_PARTY;
    *flags = SELECTION_FLAG_WHOLE_PARTY;
}

int selectionEntryIsEligible(int index)
{
    int flags;

    flags = selectionFlagTable[selectionUsableItemList[index]];
    if (flags & SELECTION_FLAG_DISABLED)
    {
        return 0;
    }
    if (flags & SELECTION_FLAG_RESTRICTED)
    {
        if ((unsigned int) (ptState.nearbySearchType - SELECTION_RESTRICTED_STATE_FIRST) >= SELECTION_RESTRICTED_STATE_COUNT)
        {
            return 0;
        }
    }
    if (!(flags & SELECTION_FLAG_ANY_MODE))
    {
        if (ptState.splitMode != 0)
        {
            if (!(flags & SELECTION_FLAG_SPLIT_ONLY))
            {
                return 0;
            }
        }
        else if (flags & SELECTION_FLAG_SPLIT_ONLY)
        {
            return 0;
        }
    }
    return 1;
}

int selectionWindowEntryTargetsParty(int index)
{
    if ((selectionFlagTable[selectionUsableItemList[index]] & SELECTION_FLAG_WHOLE_PARTY) != 0) {
        return 1;
    }
    return 0;
}

/* Whether window entry index is eligible and useful for holder's character (enum SelectionFlag);
   an entry with no bit besides the two split-mode ones is useful. */
int selectionEntryIsUseful(int index, PartyMember *holder)
{
    u16 flags;
    StatBlock *record;
    int useful;
    /* Holds the 1 every test below stores; with a literal 1 the register allocation differs. */
    int usable;

    flags = selectionFlagTable[selectionUsableItemList[index]];
    if (selectionEntryIsEligible(index) == 0)
    {
        return 0;
    }
    record = calcTotalParaMenu(holder->id);
    flags &= ~(SELECTION_FLAG_SPLIT_ONLY | SELECTION_FLAG_ANY_MODE);
    usable = 1;
    useful = flags == 0;
    if (flags & SELECTION_FLAG_SKILL_POINTS)
    {
        if (holder->limits->skillPoints < SELECTION_POINTS_MAX)
        {
            useful = usable;
        }
    }
    if (flags & SELECTION_FLAG_CLASS_POINTS)
    {
        if (holder->limits->classPoints < SELECTION_POINTS_MAX)
        {
            useful = usable;
        }
    }
    if (flags & SELECTION_FLAG_RESTORE_HP)
    {
        if (record->maxHp != record->hp)
        {
            useful = usable;
        }
    }
    if (flags & SELECTION_FLAG_RESTORE_EP)
    {
        if (record->maxEp != record->ep)
        {
            useful = usable;
        }
    }
    if (flags & SELECTION_FLAG_WHOLE_PARTY)
    {
        useful = usable;
    }
    return useful;
}

u16 selectionWindowEntryMarkedFlags(int index)
{
    u16 flags;

    flags = selectionFlagTable[selectionUsableItemList[index]];
    return (flags & (SELECTION_FLAG_CLASS_POINTS | SELECTION_FLAG_SKILL_POINTS)) ? flags : 0;
}

/* Whether a member of the active slots (back 7..9 in split mode, front 0..6 otherwise) is not at full HP. */
int selectionActiveMemberNeedsHp(void)
{
    int start;
    int count;
    int i;
    PartyMember *member;
    StatBlock *record;

    if (ptState.splitMode != 0)
    {
        start = 7;
        count = 3;
    }
    else
    {
        start = 0;
        count = 7;
    }
    for (i = 0; i < count; i++)
    {
        member = ptState.slots[start + i];
        if (member != 0)
        {
            record = calcTotalParaMenu(member->id);
            if (record->hp != record->maxHp)
            {
                return 1;
            }
        }
    }
    return 0;
}

/* Whether one of the counted members (counts[2] back ones while splitMode is set, counts[1] front
   ones otherwise) has HP or EP below its maximum. */
int selectionCountedMemberNeedsHpEp(void)
{
    int start;
    int count;
    int i;
    StatBlock *record;

    if (ptState.splitMode != 0)
    {
        count = ptState.counts[2];
        start = 7;
    }
    else
    {
        count = ptState.counts[1];
        start = 0;
    }
    for (i = 0; i < count; i++)
    {
        record = calcTotalParaMenu(ptState.slots[start + i]->id);
        if (record->hp != record->maxHp || record->ep != record->maxEp)
        {
            return 1;
        }
    }
    return 0;
}

/* Bit c of ptState.progress->eligibility (bit c & 7 of byte c >> 3) marks candidate c; selectionKeyItemBitTest tests it. */
void selectionKeyItemBitsClear(void)
{
    int slot;

    slot = 0;
    do {
        ptState.progress->eligibility.bits[slot] = 0;
        slot++;
    } while (slot < 32);
}


void selectionKeyItemBitsFill(void)
{
    int candidate;
    int bit;
    int slot;

    candidate = 1;
    do
    {
        bit = candidate & 7;
        slot = candidate >> 3;
        if (dataBoxChkCategory3(candidate) != 0)
        {
            ptState.progress->eligibility.bits[slot] |= 1 << bit;
        }
        candidate += 1;
    }
    while (candidate < 255);
}

int selectionKeyItemBitTest(int slot)
{
    unsigned int value;

    value = ptState.progress->eligibility.bits[slot >> 3];
    return (value >> (slot & 7)) & 1;
}

/* unsigned: selectionListDropAt reads it with lbu and decrements with addiu 0xFF. */

void selectionRebuildList(void)
{
    int candidate;
    int count;

    candidate = 0;
    count = 0;
    do
    {
        if (dataBoxChkCategory2(candidate) > 0)
        {
            selectionAccessoryList[count] = candidate;
            count += 1;
        }
        candidate += 1;
    }
    while (candidate < 255);
    selectionPlainCount = count;
}


/* Uses up one of plain entry index (dataBoxDecCategory2) and returns what is left; an entry with none left is
   dropped from the list. */
int selectionListDropAt(int index)
{
    int i;

    dataBoxDecCategory2(selectionAccessoryList[index]);
    if (dataBoxChkCategory2(selectionAccessoryList[index]) <= 0)
    {
        for (i = index + 1; i < selectionPlainCount; i++)
        {
            selectionAccessoryList[i - 1] = selectionAccessoryList[i];
        }
        selectionPlainCount--;
        return 0;
    }
    return dataBoxChkCategory2(selectionAccessoryList[index]);
}


extern int menuMainTxdBuffer[];

void commandRecordTextsBind(void)
{
    int index;
    int key;
    struct LookupRecord *record;

    index = 0;
    record = attackRecordTable;
    do
    {
        key = index + 0x50;
        index += 1;
        record->text = txdTableEntry(menuMainTxdBuffer, key);
        record += 1;
    }
    while (index < 99);
}

extern unsigned char selectionMemberCapacityTable[];
/* The selectionMemberCapacityTable value of character id; for id 2 it is 3 higher, and 4 higher while its mark 4 is set. */
int selectionMemberCapacity(int id)
{
    int value;

    value = selectionMemberCapacityTable[id];
    if (id == 2)
    {
        value += 3;
        if (battleSlotMarkTest(id, 4) != 0)
        {
            value += 1;
        }
    }
    return value;
}


/* Counts the pair records of attackRecordTable (SELECTION_PAIR_RECORD_FIRST, 16 of them) that are enabled in
   mask (bit attributes[2]), have member as one of their two members and whose other member ptFindSlot
   finds. */
int selectionPairRecordCount(int member, unsigned short mask)
{
    int count;
    int i;

    count = 0;
    for (i = SELECTION_PAIR_RECORD_FIRST; i < SELECTION_PAIR_RECORD_FIRST + SELECTION_PAIR_RECORD_COUNT; i++)
    {
        if ((mask >> attackRecordTable[i].attributes[2]) & 1)
        {
            if (attackRecordTable[i].attributes[0] == member)
            {
                if (ptFindSlot(attackRecordTable[i].attributes[1]) != -1)
                {
                    count++;
                }
            }
            else if (attackRecordTable[i].attributes[1] == member)
            {
                if (ptFindSlot(attackRecordTable[i].attributes[0]) != -1)
                {
                    count++;
                }
            }
        }
    }
    return count;
}

/* Index of the nth such pair record, or -1 when there is none. The original masks i with 15,
   although i is below 16 there. */
int selectionPairRecordAt(int member, int nth, unsigned short mask)
{
    int found;
    int i;

    found = 0;
    for (i = 0; i < SELECTION_PAIR_RECORD_COUNT; i++)
    {
        if ((mask >> attackRecordTable[i + SELECTION_PAIR_RECORD_FIRST].attributes[2]) & 1)
        {
            if (attackRecordTable[i + SELECTION_PAIR_RECORD_FIRST].attributes[0] == member)
            {
                if (ptFindSlot(attackRecordTable[i + SELECTION_PAIR_RECORD_FIRST].attributes[1]) != -1)
                {
                    found++;
                    if (nth + 1 == found)
                    {
                        break;
                    }
                }
            }
            else if (attackRecordTable[i + SELECTION_PAIR_RECORD_FIRST].attributes[1] == member)
            {
                if (ptFindSlot(attackRecordTable[i + SELECTION_PAIR_RECORD_FIRST].attributes[0]) != -1)
                {
                    found++;
                    if (nth + 1 == found)
                    {
                        break;
                    }
                }
            }
        }
    }
    if (i == SELECTION_PAIR_RECORD_COUNT)
    {
        return -1;
    }
    return (i & (SELECTION_PAIR_RECORD_COUNT - 1)) + SELECTION_PAIR_RECORD_FIRST;
}


/* Counts the records among member's seven (from attackRecordTable[member * 10 - 58]) that are enabled in mask
   (bit attributes[2]) and whose copilot member mask includes a member ptFindRecord finds. */
int selectionCopilotRecordCount(int member, unsigned short mask)
{
    int present;
    int count;
    int firstRecord;
    int i;

    present = 0;
    for (i = 0; i < 7; i++)
    {
        if (ptFindRecord(i + 1) != 0)
        {
            present |= 1 << i;
        }
    }
    firstRecord = member * 10 - 58;
    count = 0;
    for (i = 0; i < 7; i++)
    {
        if (((mask >> attackRecordTable[firstRecord + i].attributes[2]) & 1)
            && (attackRecordTable[firstRecord + i].attributes[1] & present))
        {
            count++;
        }
    }
    return count;
}

/* Index of the nth such record, or -1 when there is none. */
int selectionCopilotRecordAt(int member, int nth, unsigned short mask)
{
    int present;
    int found;
    int firstRecord;
    int i;

    present = 0;
    for (i = 0; i < 7; i++)
    {
        if (ptFindRecord(i + 1) != 0)
        {
            present |= 1 << i;
        }
    }
    firstRecord = member * 10 - 58;
    found = 0;
    for (i = 0; i < 7; i++)
    {
        if (((mask >> attackRecordTable[firstRecord + i].attributes[2]) & 1)
            && (attackRecordTable[firstRecord + i].attributes[1] & present))
        {
            found++;
            if (nth + 1 == found)
            {
                break;
            }
        }
    }
    if (i == 7)
    {
        return -1;
    }
    return firstRecord + i;
}

void menuSceneResetStub(void)
{
}

int selectionRecordIndex(int group, int slot, int sub)
{
    return (group << 5) + (slot * 4) + sub;
}

extern SelectionRecord selectionRecordTable[];

/* Returns the text pointer of the OV01 call (menu_draw.c draws it with textDrawSegment). */
char *selectionRecordTextGet(int record, int value)
{
    SelectionRecord *entry;

    entry = &selectionRecordTable[record];
    if (entry->kind < 2)
    {
        return statusTextGet(entry->dispatch_value, value);
    }
    else
    {
        return battleLookupTextGet(entry->dispatch_value, value);
    }
}

char *selectionSubTextGet(int group, int slot, int sub, int value)
{
    return selectionRecordTextGet(selectionRecordIndex(group, slot, sub), value);
}

unsigned short selectionSubCostGet(int group, int slot, int sub)
{
    return selectionRecordTable[selectionRecordIndex(group, slot, sub)].value;
}

/* Group 3 has only four slots: its slots 4..7 count as taken. */
int selectionSubIsTaken(PartyMember *holder, int group, int slot, int sub)
{
    if (group == 3 && slot >= 4)
    {
        return 1;
    }
    return ((unsigned int) holder->buffer->cells[group][slot] >> sub) & 1;
}

/* Whether holder has entry index: the taken bit of a selectionRecordIndex value, or for the extra entries
   selectionCollectEntries appends, the skill 0x3F or Shion's ethers 0x34 to 0x39 that it stands for. */
int selectionHolderHasEntry(PartyMember *holder, int index)
{
    if (index == SELECTION_ENTRY_NONE)
    {
        return 0;
    }
    if (index == SELECTION_ENTRY_EXTRA_FLAG)
    {
        return dataEtherLearnGet(holder->id, SELECTION_EXTRA_FLAG) != 0;
    }
    if (index > SELECTION_ENTRY_EXTRA_FLAG)
    {
        return dataSkillLearnGet(holder->id, index - SELECTION_EXTRA_CODE_ENTRY_OFFSET) != 0;
    }
    return ((unsigned int) holder->buffer->cells[index >> 5][(index >> 2) & 7] >> (index & 3)) & 1;
}

/* The number of taken subs of (group, slot); the bits are summed pairwise, as in the original. */
int selectionSlotTakenCount(PartyMember *holder, int group, int slot)
{
    int sub0;
    int sub3;
    int sub2;

    sub0 = selectionSubIsTaken(holder, group, slot, 0);
    sub3 = selectionSubIsTaken(holder, group, slot, 3);
    sub2 = selectionSubIsTaken(holder, group, slot, 2);
    sub0 += sub3;
    sub2 += selectionSubIsTaken(holder, group, slot, 1);
    return sub0 + sub2;
}

/* The number of taken subs of group: the counts of its eight slots, summed pairwise as in the original. */
int selectionGroupTakenCount(PartyMember *holder, int group)
{
    int slot0;
    int slot1;
    int slot2;
    int slot3;
    int slot4;
    int slot5;
    int slot6;
    int slot7;

    slot0 = selectionSlotTakenCount(holder, group, 0);
    slot7 = selectionSlotTakenCount(holder, group, 7);
    slot3 = selectionSlotTakenCount(holder, group, 3);
    slot5 = selectionSlotTakenCount(holder, group, 5);
    slot1 = selectionSlotTakenCount(holder, group, 1);
    slot2 = selectionSlotTakenCount(holder, group, 2);
    slot4 = selectionSlotTakenCount(holder, group, 4);
    slot6 = selectionSlotTakenCount(holder, group, 6);
    slot3 += slot5;
    slot4 += slot6;
    slot0 += slot7;
    slot1 += slot2;
    slot0 += slot3;
    slot1 += slot4;
    return slot0 + slot1;
}

int selectionFindIndex(PartyMember *holder, int value)
{
    int found;
    int i;

    found = -1;
    for (i = 0; i < holder->buffer->entry_count; i++)
    {
        if (holder->buffer->entries[i] == value)
        {
            found = i;
            break;
        }
    }
    return found;
}

/* Fills list with the record indices the holder has taken whose record kind is kind (kind 3: every index,
   taken or not), then the extra entries it holds (SELECTION_ENTRY_EXTRA_FLAG for kind 1; 0x71..0x76 for
   kind 2 of the character with id 3), and returns the count. The caller also passes the item category
   mask (menuItemCategoryMask) as a fourth argument, which this function does not read. */
int selectionCollectEntries(unsigned char *list, PartyMember *holder, int kind, int categoryMask)
{
    int group;
    int slot;
    int sub;
    int count;

    count = 0;
    for (group = 0; group < 4; group++)
    {
        for (slot = 0; (group < 3) ? (slot < 8) : (slot < 4); slot++)
        {
            for (sub = 0; sub < 4; sub++)
            {
                if (kind == 3 || selectionSubIsTaken(holder, group, slot, sub) != 0)
                {
                    list[count] = selectionRecordIndex(group, slot, sub);
                    if (kind == 3 || selectionRecordTable[list[count]].kind == kind)
                    {
                        count++;
                    }
                }
            }
        }
    }
    if (dataEtherLearnGet(holder->id, SELECTION_EXTRA_FLAG) != 0 && kind == 1)
    {
        list[count++] = SELECTION_ENTRY_EXTRA_FLAG;
    }
    if (kind == 2 && holder->id == 3)
    {
        /* The group counter is reused for the extra codes: the original keeps them in group's
           register, and a separate variable is allocated another one. */
        for (group = SELECTION_EXTRA_CODE_FIRST; group < SELECTION_EXTRA_CODE_END; group++)
        {
            if (dataSkillLearnGet(holder->id, group) != 0)
            {
                list[count++] = group + SELECTION_EXTRA_CODE_ENTRY_OFFSET;
            }
        }
    }
    return count;
}

int selectionEntryBitOutsideMask(int group, int slot, int sub, int mask)
{
    SelectionRecord *table;
    SelectionRecord *record;
    int clear_bits;

    table = selectionRecordTable;
    record = &table[selectionRecordIndex(group, slot, sub)];
    clear_bits = ~mask;
    clear_bits &= 1 << record->bit;
    return clear_bits != 0;
}

/* Whether every sub of group counts as taken: 32 of them, or 16 for group 3, which the 16 that
   selectionSubIsTaken reports for its missing slots 4..7 already reach. */
int selectionGroupIsComplete(PartyMember *holder, int group)
{
    int count;

    count = selectionGroupTakenCount(holder, group);
    if (group == 3)
    {
        if (count >= 16)
        {
            return 1;
        }
    }
    else if (count >= 32)
    {
        return 1;
    }
    return 0;
}


/* Whether group can open slot: not open yet, group open, and its selectionSlotOpenCostTable cost within classPoints. */
int selectionCanOpenSlot(PartyMember *holder, int group, int slot)
{
    if (((holder->buffer->header[group] >> slot) & 1) || holder->buffer->openGroup < group
        || holder->limits->classPoints < selectionSlotOpenCostTable[group * 8 + slot])
    {
        return 0;
    }
    return 1;
}

/* Whether sub of (group, slot) can be taken: not taken yet, slot open, and its record value within skillPoints. */
int selectionCanTakeSub(PartyMember *holder, int group, int slot, int sub)
{
    if (((holder->buffer->cells[group][slot] >> sub) & 1) || !((holder->buffer->header[group] >> slot) & 1)
        || holder->limits->skillPoints < selectionRecordTable[selectionRecordIndex(group, slot, sub)].value)
    {
        return 0;
    }
    return 1;
}

/* Takes sub of (group, slot) and pays its record value in skill points; returns 1 once all four subs
   of the slot are taken, opening the next group when the slot belongs to the highest open one. */
int selectionTakeSub(PartyMember *holder, int group, int slot, int sub)
{
    int index;
    PartyMemberExpPoints *limits;
    int open;

    holder->buffer->cells[group][slot] |= 1 << sub;
    index = selectionRecordIndex(group, slot, sub);
    limits = holder->limits;
    limits->skillPoints -= selectionRecordTable[index].value;
    holder->buffer->takenCount++;
    if (holder->buffer->cells[group][slot] != SELECTION_SLOT_ALL_SUBS)
    {
        return 0;
    }
    open = holder->buffer->openGroup;
    if (group >= open && group < 3)
    {
        holder->buffer->openGroup = open + 1;
    }
    return 1;
}

/* Applies the skill or Shion's ether code of the record of (group, slot, sub) to holder's character
   and takes the sub (selectionTakeSub). */
int selectionApplyEntry(PartyMember *holder, int group, int slot, int sub)
{
    int index;
    SelectionRecord *entry;

    index = selectionRecordIndex(group, slot, sub);
    entry = &selectionRecordTable[index];
    if (entry->kind == 2)
    {
        dataSkillLearnSet(holder->id, entry->dispatch_value);
    }
    else
    {
        dataEtherLearnSet(holder->id, entry->dispatch_value);
    }
    return selectionTakeSub(holder, group, slot, sub);
}

/* Opens slot bit of group slot and pays its selectionSlotOpenCostTable cost in class points. */
void selectionSlotOpen(PartyMember *holder, int slot, int bit)
{
    PartyMemberExpPoints *limits;

    holder->buffer->header[slot] |= 1 << bit;
    limits = holder->limits;
    limits->classPoints -= selectionSlotOpenCostTable[slot * 8 + bit];
}

extern PartyMember ptMemberRecords[];
/* OV01 dataBoxIncCategory3 (config/symbols/ov01.txt). */

/* Clears the ten holders' taken bits and gives them SELECTION_START_POINTS skill and class points,
   takes the first two subs of (0, 0) for the member with id 4, resets the selection state, adds
   SELECTION_POINTS_PER_MAXIMUM skill points per saved maximum when oldDataMaximaCopy reports them,
   fills the eligibility bitmap and stamps the play time.
   Two spellings per object are needed: the clear loop stores to ptMemberTakenTable[member] directly while the
   id-4 grant reloads ptMemberRecords[member].buffer for each update, and mapFlag and memberMask are
   stored to ptProgressRecord directly while mapId and playTime go through ptState.progress, as in the original. */
void ptStateInit(void)
{
    SaveTextMaximaBytes maxima;
    XglClock now;
    int member;
    int group;
    int slot;
    SelectionRecord *entry;

    dataPlUnitInit();
    dataBoxClearAll();
    sdvInitSpecialWork();
    member = 0;
    do
    {
        for (group = 0; group < 4; group++)
        {
            ptMemberTakenTable[member].header[group] = 0;
            for (slot = 0; slot < 8; slot++)
            {
                ptMemberTakenTable[member].cells[group][slot] = 0;
            }
            ptMemberTakenTable[member].entries[group] = 0;
        }
        ptMemberTakenTable[member].openGroup = 0;
        ptMemberTakenTable[member].takenCount = 0;
        ptMemberTakenTable[member].entry_count = 0;
        ptMemberRecords[member].limits->skillPoints = SELECTION_START_POINTS;
        ptMemberRecords[member].limits->classPoints = SELECTION_START_POINTS;
        ptState.slots[member] = 0;
        ptState.roster[member] = 0;
        if (ptMemberRecords[member].id == 4)
        {
            ptMemberRecords[member].buffer->header[0] |= 1;
            ptMemberRecords[member].buffer->cells[0][0] |= 3;
            ptMemberRecords[member].buffer->takenCount += 2;
            entry = &selectionRecordTable[selectionRecordIndex(0, 0, 0)];
            if (entry->kind == 2)
            {
                dataSkillLearnSet(4, entry->dispatch_value);
            }
            else
            {
                dataEtherLearnSet(4, entry->dispatch_value);
            }
            entry = &selectionRecordTable[selectionRecordIndex(0, 0, 1)];
            if (entry->kind == 2)
            {
                dataSkillLearnSet(4, entry->dispatch_value);
            }
            else
            {
                dataEtherLearnSet(4, entry->dispatch_value);
            }
        }
        member++;
    }
    while (member < 10);
    ptProgressRecord.mapFlag = 0;
    ptState.splitMode = 0;
    ptState.frontIndex = 0;
    ptState.backIndex = 0;
    ptState.counts[0] = 0;
    ptState.counts[1] = 0;
    ptState.counts[2] = 0;
    ptState.progress->mapId = -1;
    ptProgressRecord.memberMask = 0;
    cursorBufferTablesInit();
    selectionKeyItemBitsClear();
    if (oldDataMaximaCopy(&maxima) & 3)
    {
        ptMemberRecords[0].limits->skillPoints += maxima.bytes[0] * SELECTION_POINTS_PER_MAXIMUM;
        ptMemberRecords[1].limits->skillPoints += maxima.bytes[1] * SELECTION_POINTS_PER_MAXIMUM;
        ptMemberRecords[2].limits->skillPoints += maxima.bytes[2] * SELECTION_POINTS_PER_MAXIMUM;
        ptMemberRecords[4].limits->skillPoints += maxima.bytes[3] * SELECTION_POINTS_PER_MAXIMUM;
        ptMemberRecords[5].limits->skillPoints += maxima.bytes[4] * SELECTION_POINTS_PER_MAXIMUM;
        ptMemberRecords[6].limits->skillPoints += maxima.bytes[5] * SELECTION_POINTS_PER_MAXIMUM;
        dataEtherLearnSet(5, SELECTION_EXTRA_FLAG);
        dataEtherLearnSet(2, SELECTION_EXTRA_FLAG);
        dataBoxIncCategory3(0x45);
    }
    configScreenStateReset();
    selectionKeyItemBitsFill();
    xglClockRead(&now);
    ptState.progress->playTime = xglClockDayTime2UInt(&now);
}
