#ifndef INCLUDE_OV01_CHOICE_MENU_H
#define INCLUDE_OV01_CHOICE_MENU_H

/* Functions of src/ov01/choice_menu.c that other files call. */

#include "ov01/tgt_select.h"

int cfEventGet(void);

/* Bits of the cfEvent word (cfEventGet). */
#define CF_EVENT_BOOST_BONUS 0x1                /* battleStartBonusApply: 1..2 boost stocks for side 0 */
#define CF_EVENT_PARTY_BONUS 0x2                /* battleStartBonusApply: current or levels for the party */
#define CF_EVENT_BONUSES (CF_EVENT_BOOST_BONUS | CF_EVENT_PARTY_BONUS)
#define CF_EVENT_TIME_LIMIT 0x20                /* the battle has a time limit (gameWork.panelValue) */
#define CF_EVENT_SKIP_VICTORY 0x200             /* the battle ends without the victory sequence */

void cfEventSet(int cfEvent);
int cfMonsSetNoGet(void);
void cfMonsSetNoSet(int monsSetNo);
int gattaiCodeGet(void);
struct BattleUnit;

/*
 * The battle state block at tgtSelectState: the target selection of tgt_select.c and the battle values the other files
 * read through manWorkGet. flags holds the stage bits 1 (begin), 2 (activate), 4 (cursor) and 8 (execute)
 * that tgtSelectStep dispatches on.
 */
typedef struct BattleState BattleState;

struct BattleState {
    int flags;                              /* +0x00: the stage bits */
    struct BattleUnit *current;             /* +0x04: the object under the cursor */
    struct BattleUnit *lastObject;          /* +0x08: the object tgtSelectConfirm picked */
    struct BattleUnit *targets[2];          /* +0x0C: the target slots tgtUnitAdrGet hands out */
    struct BattleUnit *lastPartyUnit;       /* +0x14: the last party unit whose turn ended */
    int menuMode;                           /* +0x18: BATTLE_MENU_*; BATTLE_MENU_KEYS until battleCtrlMan opens the
                                             * target menu; the *_TARGET modes can use targets[1] */
    int phase;                              /* +0x1C */
    int selection;                          /* +0x20: manInput's choice (0 none, TGT_SELECTION_BACK) */
    int selectionKind;                      /* +0x24: set by manInput; convKey2unit takes 1 and 2 */
    int active;                             /* +0x28: manInput sets 2 when the command panel opens, 1 when
                                             * it is closed */
    int selectionIndex;                     /* +0x2C: the record the menu selection picked (convKey2unit) */
    int status;                             /* +0x30: TGT_STATUS_* (tgt_select.c) */
    int victoryStarted;                     /* +0x34: set once battleAfter has started the victory */
    struct BattleUnit *object;              /* +0x38: cleared at the end of a turn */
    int commandKind;                        /* +0x3C: the CMD_KIND_* of the command eventUnitCommand queued for object
                                             * (kinds 1..4 and 6) */
    int hitDamage;                          /* +0x40: the damage of unitJobTypeAtk's last hit (HitRecord.damage) */
    int hitMode;                            /* +0x44: its hit mode word (HitRecord.hitMode); battleResolveStatus passes
                                             * object, commandKind, hitDamage and hitMode on */
    int transition;                         /* +0x48: 1 when the motion mode is below 16 (a party unit left), else 2 */
    int sideToggle;                         /* +0x4C: flipped by L1/L2 in the cursor stage */
    int moveCmd;                            /* +0x50: set by setMoveCmd */
    int mode;                               /* +0x54 */
    int slotIndex;                          /* +0x58: the index battleEntryActivate and unitRecreate use */
    float scale;                            /* +0x5C: the angle unitRecreate restores */
    unsigned char *targetFlags;             /* +0x60: bit 0 of its first byte selects targets[1] */
};

/* Modes of the battle menu (BattleState.menuMode) that manInput steps; the *_TARGET modes pick the target of
 * the chosen record (tgtUnitAdrGet). */
enum BattleMenuMode {
    BATTLE_MENU_KEYS,                       /* key input (keyCmdGet), PAD_CROSS opens the command panel */
    BATTLE_MENU_KEY_HELD,                   /* a key was entered: waits for DATA_KEY_CANCEL */
    BATTLE_MENU_PANEL_OPEN,
    BATTLE_MENU_PANEL,
    BATTLE_MENU_END,                        /* a choice was made: back to BATTLE_MENU_KEYS with SELECT_END */
    BATTLE_MENU_WAIT,
    BATTLE_MENU_TECS,
    BATTLE_MENU_TEC_TARGET,
    BATTLE_MENU_PAIRED,
    BATTLE_MENU_ITEMS,
    BATTLE_MENU_ITEM_TARGET,
    BATTLE_MENU_LEVELS,
    BATTLE_MENU_LEVEL_TARGET,
    BATTLE_MENU_TGT_IDS,
    BATTLE_MENU_TGT_ID_TARGET,
    BATTLE_MENU_PAIRS,
    BATTLE_MENU_PAIR_TARGET,
    BATTLE_MENU_IDLE,                       /* manInput does nothing in this mode */
    BATTLE_MENU_SNAPSHOT,
    BATTLE_MENU_SWAP,
    BATTLE_MENU_LEAVE
};

extern BattleState tgtSelectState;

void manInput(struct BattleUnit *unit);

struct BattleState *manWorkGet(void);
struct CmdListBody;
int convKey2unit(struct BattleUnit *unit, struct CmdListBody *command, struct BattleUnit *target);
struct BattleUnit **tgtUnitAdrGet(void);
struct BattleUnit *tgtUnitGet(void);
void tgtUnitSet(struct BattleUnit *unit);

short tableKeyEntryGet(struct BattleUnit *owner, int index, void *target);
void gattaiCodeSet(int code);
void thinkNoSet(int value);
int thinkNoGet(void);

/* The party table ov01PartyTable (0x14-byte records, one per party slot). */
TgtSlot *ov01PartyListGet(void);
void ov01InitAll(void);

#endif /* INCLUDE_OV01_CHOICE_MENU_H */
