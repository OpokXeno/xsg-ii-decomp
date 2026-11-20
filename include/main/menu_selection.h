#ifndef INCLUDE_MAIN_MENU_SELECTION_H
#define INCLUDE_MAIN_MENU_SELECTION_H

#include "main/pt_member.h"

/* Functions of src/main/menu_selection.c that other files call. */

/* The command records of the special-attack table (commandRecordTextsBind fills text with
   txdTableEntry): text is the command's name. The attribute bytes are the kind
   icons of an attack command; for a special attack its kind and copilot member mask, or
   the two members who perform it. The detail panels (menuDrawCommandDetail) show the rest as
   text entries: target 0x111 + n, zone 0x116 + n, type 0x125 + n, range 0x12B + n,
   condition 0x13A + n (from 4 on: "%1d" of n - 3), effect 0x140 + n, and one entry
   0x130 + n per bit n of elementMask ("Attribute"). The command panels index the same table by member: the attack
   command panel reads attackRecordTable[(member - 1) * 7 + index] and the detail panel
   attackRecordTable[(member - 7) * 10 + 9 + index] (ten records per member). */
struct LookupRecord {
    char *text;                         /* 0x00: the record's text */
    /* In the special-attack records selectionCopilotRecordCount and selectionCopilotRecordAt scan,
     * attributes[1] is the copilot member mask (bit n for member id n + 1) and attributes[2] the bit
     * of the caller's mask that enables the record; in the pair records of selectionPairRecordCount,
     * attributes[0] and attributes[1] are the two members who perform it. */
    unsigned char attributes[3];
    unsigned char target;               /* 0x07 */
    unsigned char zone;                 /* 0x08 */
    unsigned char type;                 /* 0x09 */
    unsigned char range;                /* 0x0A */
    unsigned char condition;            /* 0x0B */
    unsigned char effect;               /* 0x0C */
    unsigned char unmodeled_0d;
    unsigned short elementMask;         /* 0x0E */
};

/* One entry per (group, slot, sub), indexed by selectionRecordIndex: kind selects the device call (2 or not),
   dispatch_value is its argument, value is the stored result and bit is the flag tested by selectionEntryBitOutsideMask. */
typedef struct SelectionRecord {
    unsigned char kind;
    unsigned char dispatch_value;
    unsigned short value;
    unsigned char bit;
    unsigned char unmodeled_05;
} SelectionRecord;

/* Selection lists and flags kept in the party data files: the window list and the plain item list with their counts. */
extern unsigned char selectionWindowCount;
extern unsigned char selectionPlainCount;
extern unsigned char selectionUsableItemList[256];
extern unsigned char selectionAccessoryList[256];
extern u16 selectionFlagTable[44];
/* Per group (4) and slot (8, the last group has 4): the class points a slot costs to open and gives back. */
extern unsigned short selectionSlotOpenCostTable[28];
extern unsigned short selectionSlotRewardTable[28];
/* The command page cursor capacity, indexed by member id - 4. */
extern unsigned char selectionMemberCapacityTableByKind[16];

extern struct LookupRecord attackRecordTable[88];

void menuSceneResetStub(void);
void commandRecordTextsBind(void);
void ptStateInit(void);
char *selectionRecordTextGet(int record, int value);
char *selectionSubTextGet(int group, int slot, int sub, int value);
unsigned short selectionSubCostGet(int group, int slot, int sub);

void selectionRebuildWindowList(void);
int selectionWindowListDropAt(int index);
void selectionFlagTableInit(void);
int selectionEntryIsEligible(int index);
int selectionWindowEntryTargetsParty(int index);
int selectionEntryIsUseful(int index, PartyMember *holder);
u16 selectionWindowEntryMarkedFlags(int index);
int selectionActiveMemberNeedsHp(void);
int selectionCountedMemberNeedsHpEp(void);
void selectionKeyItemBitsClear(void);
void selectionKeyItemBitsFill(void);
int selectionKeyItemBitTest(int slot);
void selectionRebuildList(void);
int selectionListDropAt(int index);
int selectionMemberCapacity(int id);
int selectionPairRecordCount(int member, unsigned short mask);
int selectionPairRecordAt(int member, int nth, unsigned short mask);
int selectionCopilotRecordCount(int member, unsigned short mask);
int selectionCopilotRecordAt(int member, int nth, unsigned short mask);
int selectionRecordIndex(int group, int slot, int sub);
int selectionSubIsTaken(PartyMember *holder, int group, int slot, int sub);
int selectionHolderHasEntry(PartyMember *holder, int index);
int selectionSlotTakenCount(PartyMember *holder, int group, int slot);
int selectionGroupTakenCount(PartyMember *holder, int group);
int selectionFindIndex(PartyMember *holder, int value);
int selectionCollectEntries(unsigned char *list, PartyMember *holder, int kind, int categoryMask);
int selectionEntryBitOutsideMask(int group, int slot, int sub, int mask);
int selectionGroupIsComplete(PartyMember *holder, int group);
int selectionCanOpenSlot(PartyMember *holder, int group, int slot);
int selectionCanTakeSub(PartyMember *holder, int group, int slot, int sub);
int selectionTakeSub(PartyMember *holder, int group, int slot, int sub);
int selectionApplyEntry(PartyMember *holder, int group, int slot, int sub);
void selectionSlotOpen(PartyMember *holder, int slot, int bit);

#endif /* INCLUDE_MAIN_MENU_SELECTION_H */
