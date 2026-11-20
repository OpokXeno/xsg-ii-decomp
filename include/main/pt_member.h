#ifndef INCLUDE_MAIN_PT_MEMBER_H
#define INCLUDE_MAIN_PT_MEMBER_H

#include "shared.h"
#include "main/xgl_mc_request.h"
#include "ov01/data_record_get.h"
#include "ov01/unit_cmd.h"

/* Functions of src/main/pt_member.c that other files call. */

/* OV01 calcTotalParaMenu (src/ov01/calc.c): the battle state block (StatBlock, 0x108 bytes) of the unit of key.
 * The menus read its hp, ep, maxima, level and stats (saveWriteImage saves the leader's level, menuDrawCharStats
 * copies the whole block). */
StatBlock *calcTotalParaMenu(int key);

void *menuMainTxdEntry(int index);
void *menuTutorialTxdEntry(int index);
void ptRefreshCurrent(void);
void ptRefresh(void);
void ptLoadPartyModels(void);
void ptApplyCurrentModel(void);
void ptResetSlots(void);
void ptEnterSplit(void);
void ptLeaveSplit(void);
void ptSwapSlots(int a, int b);
void ptSwapRosterEntries(int a, int b);
void ptRemoveMember(int id);

/* The status record behind a member's status pointer (ptMemberStatuses): two texts of
 * MainMenu.txd and the member index, which saveWriteImage saves + 1 per slot and the menus draw
 * as the member's icon. */
typedef struct PartyMemberStatus {
    char *text;                 /* +0x00: the txd entry of the member index */
    char *textAlt;              /* +0x04: the txd entry of the member index + 17 */
    unsigned char memberIndex;  /* +0x08: ptReset stores the member index */
    unsigned char unmodeled_09[3];
} PartyMemberStatus;

/* The block behind a member's limits pointer (one 0x40-byte record of slotRecordTable): experience and
 * the two point pools. */
typedef struct PartyMemberLimits {
    int total_exp;              /* +0x00: drawn beside "Total Exp" (menuDrawCharStats) */
    int next_level_exp;         /* +0x04: drawn beside "Next Lv" */
    int skillPoints;            /* +0x08: drawn beside "S.Pt"; taking a sub costs them (selectionTakeSub) */
    int classPoints;            /* +0x0C: drawn beside "C.Pt"; opening a slot costs them (selectionSlotOpen) */
    unsigned char unmodeled_10[0x40 - 0x10];
} PartyMemberLimits;

/* A member record (one of the ten ptMemberRecords): word 0 is the member id; the others point at its
 * status, its points and its taken bits (ptMemberTakenTable). */
typedef struct PartyMember {
    int id;                     /* +0x00: character id, looked up with calcTotalParaMenu */
    PartyMemberStatus *status;  /* +0x04 */
    PartyMemberLimits *limits;  /* +0x08 */
    PartyTakenBits *buffer;     /* +0x0C */
} PartyMember;

/* The record ptState.progress points at (ptProgressRecord, 0x50 bytes). memberMask has one bit per
 * member who joined the party (ptAddMember sets bit id - 1 for ids below 11, id - 4 for the others).
 * xgl_cd_arena.c adds the elapsed time to playTime. mapId is the map the party was attached to a uwamono on,
 * -1 while detached. */
typedef struct PartyProgress {
    unsigned short memberMask;          /* +0x00 */
    unsigned int playTime;              /* +0x04: xglClockDayTime2UInt reading at reset */
    int mapId;                          /* +0x08 */
    unsigned char unmodeled_0C[4];
    PartyValue64 position[2];           /* +0x10: the actor position (x, y, z, w) copied on attach, as two 64-bit values saveWriteImage saves */
    unsigned char savedSplitMode;       /* +0x20: PartyState.splitMode when it was attached */
    unsigned char mapFlag;              /* +0x21: set to 1 by saveWriteImage when gameWork's map id is the game end map */
    PartyEligibility eligibility;       /* +0x22 */
    unsigned char unmodeled_42[0x50 - 0x42];
} PartyProgress;

#define PARTY_FRONT_SLOTS 7
#define PARTY_BACK_SLOTS 3
#define PARTY_SLOTS (PARTY_FRONT_SLOTS + PARTY_BACK_SLOTS)

/* PartyState.splitMode: PARTY_SPLIT_ON while the split is open. */
enum PartySplitState {
    PARTY_SPLIT_OFF = 0,
    PARTY_SPLIT_ON = 1
};

/* Party state block ptState (0x60 bytes):
 *   member slots 0-9 (slots 0-6 front, 7-9 back) and a second roster of the
 *   same kind (swapped by ptSwapRosterEntries, searched by ptFindRosterEntry);
 *   splitMode is set to 1 by ptEnterSplit and cleared by ptLeaveSplit;
 *   frontIndex is the current front slot, backIndex the current back slot - PARTY_FRONT_SLOTS;
 *   counts are the member counts (counts[1] outside split mode, counts[2] in it). */
typedef struct PartyState {
    PartyProgress *progress;            /* +0x00 */
    PartyMember *slots[PARTY_SLOTS];    /* +0x04 */
    PartyMember *roster[PARTY_SLOTS];   /* +0x2C */
    unsigned char splitMode;            /* +0x54 */
    unsigned char frontIndex;           /* +0x55 */
    unsigned char backIndex;            /* +0x56 */
    unsigned char counts[3];            /* +0x57 */
    /* The member the menu shows (the main menu cursor is stored here): a front slot index, or in split
     * mode an index over back slots 7-9 followed by the second roster. */
    unsigned char menuSlot;             /* +0x5A */
    unsigned char unmodeled_5B;
    int nearbySearchType;                     /* +0x5C: the uwamonoGetNearbySearchType value; restricted selection entries are eligible for 7..8 */
} PartyState;

extern PartyState ptState;
extern PartyProgress ptProgressRecord;
extern PartyTakenBits ptMemberTakenTable[10];

/* ptStatusBlock (0x24 bytes) as ptResetStatusBlock resets it and ptRefresh fills
 * it: the ids of the three members on screen and of the leader; memberMask, a
 * copy of ptProgressRecord.memberMask (one bit per member who joined the party);
 * presentMask, one bit per member of the group on screen (front slots 0-6,
 * or back slots 7-9 in split mode; ov01's debug_entry.c edits it as the MEMBER row);
 * and the ptGetClothId cloth id of each of the seven members (ov01's debug_entry.c
 * edits them as cloth ids). */
typedef struct PartyStatusBlock {
    unsigned char memberIds[3];
    unsigned char leaderId;
    unsigned short memberMask;
    unsigned short presentMask;
    int clothIds[7];
} PartyStatusBlock;

PartyStatusBlock *ptGetStatusBlock(void);

struct UwamonoRecord;

PartyMember *ptGetMenuMember(void);
StatBlock *ptGetMenuMemberState(void);
PartyMember *ptFindRecord(int id);
void ptSetCurrentSlot(int slot);
void ptSelectMember(int id);
int ptFindSlot(int id);
void ptAddMember(int id);
int ptGetCurrentSlot(void);
int ptGetCurrentId(void);
int ptFindFrontSlot(int secondIndex);
int ptFindRosterEntry(int id);
int ptGetClothId(int id);
void ptPlaceMember(int id);
void ptFillCopilots(void);
void ptFillReserve(void);
void ptResetSplitReserve(void);
int ptGetMainPilotId(int id);
int ptGetCopilotId(int id);
int ptAllCopilotsAssigned(void);
int ptGroupCount(void);
void ptResetStatusBlock(void);
int ptFlagCombinationIndex(void);
void saveChecksum(struct SaveData *image, long long *sum);
void saveWriteImage(void);
void saveReadImage(void);
void saveReadGameWork(void);
void padApplySaveSettings(void);
PartyState *ptGetState(void);
PartyMember *ptGetMember(int index);
int talkCheck(void);
int talkReset(void);

/* The active map object (.sdata of pt_member.c), also driven by the talk natives. */
extern struct UwamonoRecord *mapActiveObject;
extern int talkResult;

#endif /* INCLUDE_MAIN_PT_MEMBER_H */
