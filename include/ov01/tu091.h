#ifndef INCLUDE_OV01_TU091_H
#define INCLUDE_OV01_TU091_H

/* Functions of src/ov01/tu091.c that other files call. */

/* The message task record msgPutObject returns (defined in tu091.c). */
struct MsgObject;

/* Shows text at (x, y) in a width x height box for frames frames (0: until its voice is done). */
struct MsgObject *msgPutObject(int x, int y, char *text, int frames, int width, int height);

/* Effect request block sent to the main-side effect queue (sefStartEffectDefault /
 * sefStartEffectWithValue); the senders clear it first. */
struct EffectRequest {
    struct XenoActor *head;            /* +0x00: the actor of the owner unit (Ov01Object.unit) */
    struct XenoActor *partner;         /* +0x04: the other unit's actor of a pair technique (attackEffectsStart) */
    struct XenoActor *target;          /* +0x08: the same actor again (tu033 senders); the hit target's actor */
    struct XenoActor *parts[4];        /* +0x0C: the child actors of the unit (two each for a pair technique) */
    int key;                           /* +0x1C: (high << 12) | low, see createEffect */
    unsigned char unmodeled_20[0x24 - 0x20];
    short hitMode;                     /* +0x24: the hit record's hitMode (attackEffectsStart) */
    unsigned short hitCode;            /* +0x26: the actionBits byte and the target's row bits (attackEffectsStart) */
    unsigned char unmodeled_28[0x2C - 0x28];
    short mode;                        /* +0x2C: 2 or 3 (unitEffectStartMode); the attacker's state
                                        * (attackEffectsStart) */
    short targetState;                 /* +0x2E: the target's state (attackEffectsStart) */
    unsigned char unmodeled_30[0x38 - 0x30];
};

/* EffectRequest.key (and the keys sefApplyRecordMode and sefLoadEffect take): the effect group above this shift, the
 * record of the group below it. */
#define EFFECT_KEY_GROUP_SHIFT 12

/* The script attack table the natives atkTbl and atkTblGet (tu046) take from the image's globals: a row count,
 * then rows of two words. */
typedef struct AtkTblRow {
    int key;   /* +0x00: atkTblGet field 1 */
    int value; /* +0x04: atkTblGet field 2 */
} AtkTblRow;

typedef struct AtkTbl {
    int count;          /* +0x00: atkTblGet field 0 (any other field) */
    AtkTblRow rows[1];  /* +0x04: extent given by count */
} AtkTbl;

struct Ov01Object;

/* The 16-byte record unitScriptEventSend takes: the unit and three words it passes on, with the selector, as a script
 * message (sbThreadEnqueue). */
typedef struct UnitScriptEvent {
    struct Ov01Object *unit;
    int words[3];
} UnitScriptEvent;

void unitScriptEventSend(int kind, UnitScriptEvent *event);

/* The 16-byte record unitScriptPairEventSend takes: the unit and, for UNIT_SCRIPT_KIND_PAIR, a second unit and two
 * words that it passes on, with the kind, as a script message (sbThreadEnqueue). */
typedef struct UnitScriptPairEvent {
    struct Ov01Object *unit;
    struct Ov01Object *other;
    int words[2];
} UnitScriptPairEvent;

/* The unitScriptPairEventSend kind whose message also carries the second unit's index and the two words. */
#define UNIT_SCRIPT_KIND_PAIR 2

void unitScriptPairEventSend(int kind, UnitScriptPairEvent *event);

int atkTblFieldGet(int field, AtkTbl *table, int index);
int atkTblBuild(struct Ov01Object *unit, AtkTbl *table, int rows);
void thinkSendEnd(struct Ov01Object *unit);
void thinkDispatch(struct Ov01Object *unit, int command, int arg, struct Ov01Object *target);

/* The unit parameters scripts read and write by number (natives unitParaGet and unitParaSet, tu046). 17..36 are
 * the bytes from StatBlock.liveRows on: liveRows (17..20), slotValue (21..28) and slotRoll (29..36). */
enum {
    UNIT_PARA_MAX_HP,
    UNIT_PARA_MAX_EP,
    UNIT_PARA_PHYS_ATTACK,
    UNIT_PARA_PHYS_DEFENCE,
    UNIT_PARA_MAG_ATTACK,
    UNIT_PARA_MAG_DEFENCE,
    UNIT_PARA_HIT_RATE,
    UNIT_PARA_EVADE_RATE,
    UNIT_PARA_VALUE_BIAS,
    UNIT_PARA_HP,
    UNIT_PARA_EP,
    UNIT_PARA_KEY,
    UNIT_PARA_STATE70,                  /* read only */
    UNIT_PARA_LEVEL,
    UNIT_PARA_EXP_LEVEL,
    UNIT_PARA_FLAGS,
    UNIT_PARA_POS_INDEX,
    UNIT_PARA_LIVE_ROW,
    UNIT_PARA_SLOT_VALUE = UNIT_PARA_LIVE_ROW + 4,
    UNIT_PARA_SLOT_ROLL = UNIT_PARA_SLOT_VALUE + 8,
    UNIT_PARA_CURRENT = UNIT_PARA_SLOT_ROLL + 8
};

int unitParaFieldGet(struct Ov01Object *unit, int field);
void unitParaFieldSet(struct Ov01Object *unit, int field, int value);

/* Message and party data prototypes. */
int msgIsIdle(void);
void partyDataSetSlot(int id, int slot, int value);
void msgReset(void);

#endif /* INCLUDE_OV01_TU091_H */
