#include "common.h"
#include "shared.h"
#include "main/sef.h"
#include "main/choice_menu.h"
#include "ov01/ov01_task.h"
#include "ov01/data_file_load.h"
#include "ov01/unit_tbl.h"
#include "ov01/cmd_exec.h"
#include "ov01/tgt_select.h"
#include "ov01/choice_menu.h"
#include "ov01/calc.h"
#include "ov01/scene_script.h"
#include "ov01/data_record_get.h"
#include "ov01/think_dispatch.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"

/* Message object made by msgPutObject: an ov01 task record (ov01TaskClaim) whose update is msgObjRelease. */
struct MsgObject {
    int flags;                                  /* +0x00: the task record's flags */
    void (*update)(struct MsgObject *object);   /* +0x04: msgObjRelease */
    void (*draw)(struct MsgObject *object);     /* +0x08: none */
    int ref_count;                              /* +0x0C: frames the message stays (0: until its voice is done) */
    int mode;                                   /* +0x10: MSG_MODE_VOICE when ref_count was 0 */
    TextEntry *handle;                          /* +0x14: the text entry (textEntryAlloc) */
};

/* MsgObject.mode of a message that stays until its voice is done. */
#define MSG_MODE_VOICE 1
/* MsgObject.mode of a message that stays ref_count frames. */
#define MSG_MODE_TIMED 0

/* Message globals: msgPages is the text page list {text, 0} that msgPutObject passes to textEntryAlloc; msgBusy
 * is the busy flag (1 while a message is outstanding). */
static unsigned char *msgPages[2];
static int msgBusy;

/* Clears the message request block and the busy flag. */
void msgReset(void) {
    msgPages[0] = 0;
    msgBusy = 0;
}

/* A script message word (SbMessage.args) carries two values: the second one above this shift. */
#define SCRIPT_ARG_HIGH_SHIFT 16

/*
 * Sends event as a kind message to the unit's script (the battle block's spawnArgC): the unit index, and for
 * UNIT_SCRIPT_KIND_PAIR the second unit's index in the high half and words[0] | words[1] << 16.
 */
void unitScriptPairEventSend(int kind, UnitScriptPairEvent *event)
{
    SbMessage message;

    message.kind = kind;
    message.args[0] = unitNoGet(event->unit);
    if (kind == UNIT_SCRIPT_KIND_PAIR) {
        message.args[0] |= unitNoGet(event->other) << SCRIPT_ARG_HIGH_SHIFT;
        /* In two steps: the one-expression form loads the words and allocates the registers differently. */
        message.args[1] = event->words[0];
        message.args[1] |= event->words[1] << SCRIPT_ARG_HIGH_SHIFT;
    }
    if (event->unit->stats->spawnArgC != 0) {
        sbThreadEnqueue(ov01ScriptKeyGet(), event->unit->stats->spawnArgC, &message, 0);
    }
}

/*
 * Sends event (the unit and three words) as a kind message to the unit's script (the battle block's spawnArgD),
 * or, without a unit or script, to the camera setting's script. The message's first word is the unit index with
 * words[0] in the high half, its second words[1] | words[2] << 16.
 */
void unitScriptEventSend(int kind, UnitScriptEvent *event)
{
    SbMessage message;
    int entry;
    int script;

    message.kind = kind;
    message.args[0] = unitNoGet(event->unit) | (event->words[0] << 16);
    message.args[1] = event->words[1] | (event->words[2] << 16);
    entry = event->unit != 0 && (script = event->unit->stats->spawnArgD) != 0 ? script : cameraSettingGet();
    if (entry != 0) {
        sbThreadEnqueue(ov01ScriptKeyGet(), entry, &message, 0);
    }
}

/* The wait flag thinkDispatch stores for CMD_KIND_CODE_WAIT: unitCmdMot then also waits for unitIsDone. */
#define THINK_CODE_WAIT_UNTIL_DONE 1

/* thinkDispatch: queues the command `command` (CMD_KIND_*) for the unit; natives thinkSet and thinkSet2. A technique
 * takes entry `arg` of the unit's key row (entry 0 when that one is 0); a lookup command is queued only when
 * calcEthEnableChk allows battle lookup record `arg`, else a CMD_KIND_NONE record. */
void thinkDispatch(BattleUnit *unit, int command, int arg, BattleUnit *target)
{
    CmdListBody body;
    short code;

    switch (command) {
    case CMD_KIND_TECHNIQUE:
        body.command = command;
        /* `*&` (codegen): key shares an anonymous union in StatBlock; the plain access gets alias set 0, waits for the
         * body.command store and swaps the unit and command registers. */
        code = tableKeyRowGet(*&unit->stats->key)->entries[arg];
        if (code == 0) {
            code = tableKeyRowGet(unit->stats->key)->entries[0];
        }
        body.code = code;
        body.arg.target = (int)target;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_LOOKUP:
        code = etherRecordGet(arg)->tecIndex;
        if (calcEthEnableChk(unit, arg) != 0) {
            body.command = command;
            body.code = code;
            body.arg.target = (int)target;
            body.pairIndex = arg;
        } else {
            body.command = CMD_KIND_NONE;
        }
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_SNAPSHOT:
        body.command = command;
        body.code = arg;
        cmdListSet(unit, &body);
        break;
    /* Cases 0, 2 and 7 as three bodies (codegen): jump2 merges them again; one shared case body gives other code
     * (22 instructions off). */
    case CMD_KIND_NONE:
        body.command = command;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_WAIT:
        body.command = command;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_LEAVE:
        body.command = command;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_CODE_WAIT:
        body.command = command;
        body.code = arg;
        body.arg.target = THINK_CODE_WAIT_UNTIL_DONE;
        cmdListSet(unit, &body);
        break;
    case CMD_KIND_REMOVE:
        body.command = command;
        cmdListSet(unit, &body);
        break;
    }
}

/* thinkSendEnd: queues the think-end record for the unit (cmdListSet), then ends the
 * target selection mode (tgtSelectModeEnd); used by thinkSet and thinkSetEnd. */
void thinkSendEnd(BattleUnit *unit)
{
    CmdListBody command;

    command.command = CMD_KIND_THINK_END;
    cmdListSet(unit, &command);
    tgtSelectModeEnd();
}

/*
 * Native atkTbl: fills table with the attack candidates of unit: for each of the first `rows` techniques of its
 * key row (tableKeyRowGet) and each unit of the side the technique targets that is not flagged 4 and that the
 * technique can affect (battleEffectApplies), one row {technique slot, unit index}. Returns the row count, which
 * is also stored in table->count.
 */
int atkTblBuild(BattleUnit *unit, AtkTbl *table, int rows)
{
    BattleUnit **units;
    int *word;
    TableKeyRow *keys;
    int row;
    int tec;
    int count;
    int i;
    int n;

    word = &table->count;
    n = 1;
    for (row = 0; row < rows; row++) {
        keys = tableKeyRowGet(unit->stats->key);
        tec = keys->entries[row];
        if (tec == 0) {
            continue;
        }
        count = unitTblGet(calcTrg2PE(unit, tecRecordGet(tec)->flags), &units);
        for (i = 0; i < count; i++) {
            if (units[i] == 0) {
                continue;
            }
            if (units[i]->flags & UNIT_FLAG_DOWN) {
                continue;
            }
            if (!battleEffectApplies(unit, tec, units[i])) {
                continue;
            }
            word[n++] = row;
            word[n++] = unitNoGet(units[i]);
        }
    }
    return table->count = (n - 1) / 2;
}

/* Field `field` of row `index` of the attack table (native atkTblGet). */
int atkTblFieldGet(int field, AtkTbl *table, int index)
{
    /* index[table->rows] rather than table->rows[index]: the plain form swaps the operands of the addu that forms
     * the row address (0x00A9C084 and 0x00A9C090); only the reversed form keeps the original order. */
    switch (field) {
    case 1:
        return index[table->rows].key;
    case 2:
        return index[table->rows].value;
    }
    return table->count;
}

/* Parameter field of the unit (UNIT_PARA_*); 0 for no unit or an unknown field (native unitParaGet). */
int unitParaFieldGet(struct BattleUnit *unit, int field)
{
    StatBlock *stats;

    if (unit == 0) {
        return 0;
    }
    stats = unit->stats;
    switch (field) {
    case UNIT_PARA_MAX_HP:
        return stats->maxHp;
    case UNIT_PARA_MAX_EP:
        return stats->maxEp;
    case UNIT_PARA_PHYS_ATTACK:
        return stats->physAttack;
    case UNIT_PARA_PHYS_DEFENCE:
        return stats->physDefence;
    case UNIT_PARA_MAG_ATTACK:
        return stats->magAttack;
    case UNIT_PARA_MAG_DEFENCE:
        return stats->magDefence;
    case UNIT_PARA_HIT_RATE:
        return stats->hitRate;
    case UNIT_PARA_EVADE_RATE:
        return stats->evadeRate;
    case UNIT_PARA_VALUE_BIAS:
        return stats->valueBias;
    case UNIT_PARA_HP:
        return stats->hp;
    case UNIT_PARA_EP:
        return stats->ep;
    case UNIT_PARA_KEY:
        return stats->key;
    case UNIT_PARA_STATE70:
        return stats->breakState;
    case UNIT_PARA_LEVEL:
        return stats->stockCount;
    case UNIT_PARA_EXP_LEVEL:
        return stats->expLevel;
    case UNIT_PARA_FLAGS:
        return stats->flags;
    case UNIT_PARA_POS_INDEX:
        return stats->posIndex;
    case UNIT_PARA_LIVE_ROW ... UNIT_PARA_SLOT_VALUE - 1:
        return stats->breakZones[field - UNIT_PARA_LIVE_ROW];
    case UNIT_PARA_SLOT_VALUE ... UNIT_PARA_SLOT_ROLL - 1:
        return stats->attributeReaction[field - UNIT_PARA_SLOT_VALUE];
    case UNIT_PARA_SLOT_ROLL ... UNIT_PARA_CURRENT - 1:
        return stats->attributeReactionRoll[field - UNIT_PARA_SLOT_ROLL];
    case UNIT_PARA_CURRENT:
        return stats->esEnergy;
    }
    return 0;
}

/* Sets parameter field of the unit (UNIT_PARA_*, all but UNIT_PARA_STATE70) to value (native unitParaSet). */
void unitParaFieldSet(struct BattleUnit *unit, int field, int value)
{
    StatBlock *stats;

    if (unit == 0) {
        return;
    }
    stats = unit->stats;
    switch (field) {
    case UNIT_PARA_MAX_HP:
        stats->maxHp = value;
        break;
    case UNIT_PARA_MAX_EP:
        stats->maxEp = value;
        break;
    case UNIT_PARA_PHYS_ATTACK:
        stats->physAttack = value;
        break;
    case UNIT_PARA_PHYS_DEFENCE:
        stats->physDefence = value;
        break;
    case UNIT_PARA_MAG_ATTACK:
        stats->magAttack = value;
        break;
    case UNIT_PARA_MAG_DEFENCE:
        stats->magDefence = value;
        break;
    case UNIT_PARA_HIT_RATE:
        stats->hitRate = value;
        break;
    case UNIT_PARA_EVADE_RATE:
        stats->evadeRate = value;
        break;
    case UNIT_PARA_VALUE_BIAS:
        stats->valueBias = value;
        break;
    case UNIT_PARA_HP:
        stats->hp = value;
        break;
    case UNIT_PARA_EP:
        stats->ep = value;
        break;
    case UNIT_PARA_KEY:
        stats->key = value;
        break;
    case UNIT_PARA_LEVEL:
        stats->stockCount = value;
        break;
    case UNIT_PARA_EXP_LEVEL:
        stats->expLevel = value;
        break;
    case UNIT_PARA_FLAGS:
        stats->flags = value;
        break;
    case UNIT_PARA_POS_INDEX:
        stats->posIndex = value;
        break;
    case UNIT_PARA_LIVE_ROW ... UNIT_PARA_SLOT_VALUE - 1:
        stats->breakZones[field - UNIT_PARA_LIVE_ROW] = value;
        break;
    case UNIT_PARA_SLOT_VALUE ... UNIT_PARA_SLOT_ROLL - 1:
        stats->attributeReaction[field - UNIT_PARA_SLOT_VALUE] = value;
        break;
    case UNIT_PARA_SLOT_ROLL ... UNIT_PARA_CURRENT - 1:
        stats->attributeReactionRoll[field - UNIT_PARA_SLOT_ROLL] = value;
        break;
    case UNIT_PARA_CURRENT:
        stats->esEnergy = value;
        break;
    }
}

void *memset(void *destination, int value, unsigned int count);

/* Effect request keyed by (high, low), head taken from the owner when one is given; used by native createEffect. */
void effectSendOwned(int high, int low, BattleUnit *owner) {
    struct EffectRequest request;

    memset(&request, 0, sizeof(request));
    if (owner != 0) {
        request.head = owner->unit;
    }
    request.key = (high << 0xC) | low;
    sefCreateEffect(&request);
}


/* Effect request keyed by (high, low) that carries a value; used by native createEffectPos. */
void effectStartAtPosition(int high, int low, Vector4 *position) {
    struct EffectRequest request;

    memset(&request, 0, sizeof(request));
    request.key = (high << 0xC) | low;
    sefStartEffectAtPosition(&request, position);
}

/* The 32-byte technique record of the tecRecordGet table as the native
 * tecParaGet reads it (fields 0 to 3). */

/* Field `field` of technique `index` (native tecParaGet); 0 for technique 0
 * or another field. */
int tecParaFieldGet(int index, int field)
{
    TecRecord *tec;

    if (index == 0) {
        return 0;
    }
    tec = tecRecordGet(index);
    switch (field) {
    case 0:
        return tec->flags;
    case 1:
        return tec->type;
    case 2:
        return tec->kindBits.kind;
    case 3:
        return tec->attributeBits;
    }
    return 0;
}

/* msgPutObject (core of native msgPut): makes a MsgObject from ov01TaskClaim, installs msgObjRelease as its
 * release callback, submits the request block through textEntryAlloc and sets the busy flag. */
void msgObjRelease(struct MsgObject *object);

struct MsgObject *msgPutObject(int x, int y, char *text, int frames, int width, int height)
{
    struct MsgObject *object;

    object = ov01TaskClaim();
    if (object == 0) {
        return 0;
    }
    object->update = msgObjRelease;
    object->ref_count = frames;
    object->draw = 0;
    if (frames == 0) {
        object->mode = MSG_MODE_VOICE;
    } else {
        object->mode = MSG_MODE_TIMED;
    }
    msgPages[0] = (unsigned char *)text; /* text entry pages are unsigned char (choice_menu.c) */
    msgPages[1] = 0;
    object->handle = textEntryAlloc(x, y, width, height, 0, msgPages);
    msgBusy = 1;
    return object;
}

/* Release callback of a MsgObject (stored at +0x4 by msgPutObject). A mode-1 object is freed when
 * textEntryConfirmPoll reports its handle done; otherwise the reference count is dropped. The busy flag follows. */
void msgObjRelease(struct MsgObject *object) {
    TextEntry *handle;
    int remaining;
    int remove;

    remove = 0;
    handle = object->handle;
    if (object->mode == 1) {
        if (textEntryConfirmPoll(handle) != 0) {
            remove = 1;
        }
    } else {
        remaining = object->ref_count - 1;
        object->ref_count = remaining;
        remove = remaining < 1;
    }
    if (remove == 1) {
        textEntryRemove(handle);
        ov01TaskRelease(object);
        msgBusy = 0;
        return;
    }
    msgBusy = 1;
}

/* 1 when no message is outstanding (native msgPutWait). */
int battleMsgEndChk(void) {
    return msgBusy == 0;
}

/* Reads one of the three bytes of Boost gauge `id` (slot order as in boostGaugeSetSlot); 0 for another slot. */
int boostGaugeGetSlot(int id, int slot)
{
    struct BoostGauge *gauge;

    gauge = boostGaugeSelect(id);
    switch (slot) {
    case 0:
        return gauge->stock;
    case 1:
        return gauge->progress;
    case 2:
        return gauge->maxStock;
    }
    return 0;
}

/* Writes one of the three slot bytes of Boost gauge `id`; slot 0 is stock, slot 1 progress, slot 2 maxStock. */
void boostGaugeSetSlot(int id, int slot, int value) {
    struct BoostGauge *gauge;

    gauge = boostGaugeSelect(id);
    switch (slot) {
    case 0:
        gauge->stock = (signed char) value;
        return;
    case 1:
        gauge->progress = (signed char) value;
        break;
    case 2:
        gauge->maxStock = (signed char) value;
        break;
    }
}
