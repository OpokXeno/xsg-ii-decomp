#include "common.h"
#include "unaligned.h"
#include "main/sef.h"
#include "ov01/mons_slot_show.h"
#include "ov01/battle_init.h"
#include "ov01/unit_tbl.h"
#include "ov01/calc.h"
#include "ov01/effect_pos.h"
#include "ov01/data_record_get.h"
#include "ov01/think_dispatch.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "ov01/handle_state.h"

/*
 * A native receives the running thread. Its operand stack holds 4-byte words: sp words are in use,
 * and the word just below the top is the current frame's argument count. Each argument owns an
 * 8-byte slot that ends below that count word, its low word first.
 */

/* The argument count of the current call: the word below the stack top. */
int VM_PLUGIN_CNT_GET(SbThread *thread)
{
    return thread->stack[thread->sp - 1];
}

#include "shared.h"
#include "ov01/choice_menu.h"



/*
 * Argument slots step by 8 bytes below the count word. The tag of argument k
 * is the first byte of its high word, which lies VM_ARG_TAG_BASE bytes below
 * the stack top for the newest argument.
 */
enum {
    VM_ARG_SLOT_BYTES = 8,
    VM_ARG_TAG_BASE = -8
};


/* Returns the low word of argument argIndex (1-based) of the current call. */
int vmArgGet(SbThread *thread, int argIndex)
{
    int *frameTop;
    int argCount;

    frameTop = thread->stack + thread->sp;
    argCount = frameTop[-1];
    if (argCount < argIndex || argIndex == 0) {
        sbFatal(thread);
    }
    return frameTop[-3 - 2 * (argCount - argIndex)];
}

/* Returns the high word of argument argIndex (1-based) of the current call. */
int vmArgGetHigh(SbThread *thread, int argIndex)
{
    int *frameTop;
    int argCount;

    frameTop = thread->stack + thread->sp;
    argCount = frameTop[-1];
    if (argCount < argIndex || argIndex == 0) {
        sbFatal(thread);
    }
    return frameTop[-2 - 2 * (argCount - argIndex)];
}

/*
 * Reports whether argument argIndex has a zero tag byte: the first byte of
 * its slot's high word. An index past the frame's argument count returns 1.
 */
int vmArgTagIsZero(SbThread *thread, int argIndex)
{
    int *frameTop;
    int argCount;

    frameTop = thread->stack + thread->sp;
    if (argIndex == 0) {
        sbFatal(thread);
    }
    argCount = frameTop[-1];
    if (!(argCount >= argIndex)) {
        return 1;
    }
    return ((unsigned char *) frameTop)[VM_ARG_TAG_BASE - (argCount - argIndex) * VM_ARG_SLOT_BYTES] == 0;
}

/* Script values tagged VM_KEY_TAG_ENTRY in bits 20..23 are keys of the keyed table. */
#define VM_KEY_TAG_MASK 0xF00000
#define VM_KEY_TAG_ENTRY 0x200000

/* Address of the keyed-table entry a tagged key names, or 0 for an untagged
 * value or a key past the table. */
char *vmKeyedEntryGet(SbThread *thread, int key)
{
    /* The keyed table is the SBL library image: entry counts its entries, code_offset locates the index array of
     * offsets and global_offset the data they point into (both offsets from the image start). */
    struct SbImageHeader *table = thread->image->keys;
    int index;

    if ((key & VM_KEY_TAG_MASK) != VM_KEY_TAG_ENTRY) {
        return 0;
    }
    index = key & ~VM_KEY_TAG_MASK;
    if (!(index < table->entry)) {
        return 0;
    }
    return (char *)table + table->global_offset + ((int *)((char *)table + table->code_offset))[index];
}

/* Address of global index of the thread's image (the table sbGlobalGet reads); what lies there depends on the
 * caller (a message for sbOpSend, an attack table for atkTbl). */
void *vmEntryGet(SbThread *thread, int index)
{
    return (int *)((char *)thread->image + thread->image->global_offset) + index;
}

/*
 * Entry `index` holds word offsets from the entry array; returns the entry
 * that word `subIndex` of it points to. The address stays an int: an int * return type (with or without the cast)
 * changes the size of .text.
 */
int vmSubEntryGet(SbThread *thread, int index, int subIndex)
{
    int *base = (int *)((char *)thread->image + thread->image->global_offset);
    int *entry = base + index;

    return (int) (base + entry[subIndex]);
}

/* Empty in this build: sbFatal passes the thread and its pc, as for a debug report. */
void sbFatalReport(SbThread *thread, int pc)
{
}

/*
 * Copies the 32-bit word at source to destination; either may sit at a halfword position of the code stream
 * (UnalignedWordCopy, see unaligned.h).
 */
void wordCopyUnaligned(void *destination, void *source)
{
    UnalignedWordCopy(destination, source);
}

/* Entry chosen by posDataSet; posDataGet returns it. */
static int posDataEntry;

int posDataSet(SbThread *thread)
{
    int entryIndex;

    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    entryIndex = vmArgGet(thread, 1);
    posDataEntry = vmSubEntryGet(thread, entryIndex, 0);
    return 1;
}

/* Returns the entry posDataSet stored: the effect position table (effectPosTableSet). */
struct EffectPosRecord *posDataGet(void)
{
    return (struct EffectPosRecord *)posDataEntry;
}

/* The monster table entry monsTblSet selected (the enemy list, an array of spawn records); monsTblGet returns it.
 * While monsTblSet works it is its cursor over the candidate list, entries {table, value}. */
static int *monsTblEntry;

/* At most this many candidates in a monster table list (the list ends at an entry whose table word is 0). */
#define MONS_TBL_CANDIDATES_MAX 1000

/*
 * Native monsTblSet(list, choice): counts the candidates of global list `list` (entries {table, value}, ended by a
 * zero table word), takes candidate `choice` (0 when out of range), passes its value to monsTblMapNoSet and selects
 * the first entry of its table. Jumps to SB_JUMP_ARGUMENTS without two arguments or candidates.
 */
int monsTblSet(SbThread *thread)
{
    int count;
    int choice;
    int *entry; /* the chosen entry goes through a local before monsTblEntry, as in the original's register use */

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    for (count = 0; count < MONS_TBL_CANDIDATES_MAX; count++) {
        monsTblEntry = (int *)vmSubEntryGet(thread, vmArgGet(thread, 1), count);
        if (monsTblEntry[0] == 0) {
            break;
        }
    }
    if (count <= 0) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    choice = vmArgGet(thread, 2);
    if (choice >= count) {
        choice = 0;
    }
    entry = (int *)vmSubEntryGet(thread, vmArgGet(thread, 1), choice);
    monsTblEntry = entry;
    monsTblMapNoSet(entry[1]);
    monsTblEntry = (int *)vmSubEntryGet(thread, monsTblEntry[0], 0);
    return 1;
}

/* Returns the monster table entry monsTblSet selected. */
void *monsTblGet(void)
{
    return monsTblEntry;
}

int monsSetNoGet(void)
{
    return cfMonsSetNoGet();
}

/* First argument of the last cameraSet call, stored as given. */
static int cameraSetting;

int cameraSet(SbThread *thread)
{
    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    cameraSetting = vmArgGet(thread, 1);
    return 1;
}

/* Returns the value cameraSet stored. */
int cameraSettingGet(void)
{
    return cameraSetting;
}


int thinkSet(SbThread *thread)
{
    BattleUnit *unitOne;
    BattleUnit *unitFour;
    int argTwo;
    int argThree;

    if (VM_PLUGIN_CNT_GET(thread) < 4) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitOne = unitPtrGet(vmArgGet(thread, 1));
    unitFour = unitPtrGet(vmArgGet(thread, 4));
    argTwo = vmArgGet(thread, 2);
    argThree = vmArgGet(thread, 3);
    thinkDispatch(unitOne, argTwo, argThree, unitFour);
    thinkSendEnd(unitOne);
    return 1;
}

int thinkSet2(SbThread *thread)
{
    BattleUnit *unitOne;
    BattleUnit *unitFour;
    int argTwo;
    int argThree;

    if (VM_PLUGIN_CNT_GET(thread) < 4) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unitOne = unitPtrGet(vmArgGet(thread, 1));
    unitFour = unitPtrGet(vmArgGet(thread, 4));
    argTwo = vmArgGet(thread, 2);
    argThree = vmArgGet(thread, 3);
    thinkDispatch(unitOne, argTwo, argThree, unitFour);
    return 1;
}

int thinkSetEnd(SbThread *thread)
{
    BattleUnit *unit;

    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitPtrGet(vmArgGet(thread, 1));
    thinkSendEnd(unit);
    return 1;
}


int atkTbl(SbThread *thread)
{
    BattleUnit *unit;
    int value;
    int entryIndex;
    AtkTbl *table;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    value = 0xA;
    if (vmArgTagIsZero(thread, 3) == 0) {
        value = vmArgGet(thread, 3);
    }
    unit = unitPtrGet(vmArgGet(thread, 1));
    entryIndex = vmArgGet(thread, 2);
    table = vmEntryGet(thread, entryIndex);
    return atkTblBuild(unit, table, value);
}


int atkTblGet(SbThread *thread)
{
    int argOne;
    int entryIndex;
    AtkTbl *table;
    int argThree;

    if (VM_PLUGIN_CNT_GET(thread) < 3) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    entryIndex = vmArgGet(thread, 2);
    table = vmEntryGet(thread, entryIndex);
    argOne = vmArgGet(thread, 1);
    argThree = vmArgGet(thread, 3);
    return atkTblFieldGet(argOne, table, argThree);
}

/* Returns the actor address of unit argument 1 as the script value. */
int actorGet(SbThread *thread)
{
    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    return (int)unitPtrGet(vmArgGet(thread, 1))->unit;
}


int unitParaGet(SbThread *thread)
{
    BattleUnit *unit;
    int argTwo;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitPtrGet(vmArgGet(thread, 1));
    argTwo = vmArgGet(thread, 2);
    return unitParaFieldGet(unit, argTwo);
}


int unitParaSet(SbThread *thread)
{
    BattleUnit *unit;
    int argTwo;
    int argThree;

    if (VM_PLUGIN_CNT_GET(thread) < 3) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitPtrGet(vmArgGet(thread, 1));
    argTwo = vmArgGet(thread, 2);
    argThree = vmArgGet(thread, 3);
    unitParaFieldSet(unit, argTwo, argThree);
    return 1;
}


/* The effect key packs the group above bit 12 and the effect index below it. */
int loadEffect(SbThread *thread)
{
    int effectGroup;
    int effectIndex;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    effectGroup = vmArgGet(thread, 1);
    effectIndex = vmArgGet(thread, 2);
    sefLoadEffect((effectGroup << 0xC) | effectIndex, effectGroup);
    return 1;
}


int loadWaitEffect(int unused, int *flag)
{
    if (sefCheckLoad() != 0) {
        *flag = 1;
    }
    return 1;
}


/* Native createEffect: starts effect (argument 1, argument 2), on the actor of
 * the unit of argument 3 when that argument is given. */
int createEffect(SbThread *thread)
{
    BattleUnit *owner;
    int high;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    if (vmArgTagIsZero(thread, 3) != 0) {
        owner = 0;
    } else {
        owner = unitPtrGet(vmArgGet(thread, 3));
    }
    high = vmArgGet(thread, 1);
    effectSendOwned(high, vmArgGet(thread, 2), owner);
    return 1;
}


int createEffectPos(SbThread *thread)
{
    Vector4 position;
    float zRaw;
    int positionKey;
    int argOne;
    int argTwo;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    positionKey = vmArgGet(thread, 3);
    position.x = (float) effectPosGet(positionKey)[0] / 100.0f;
    position.y = (float) effectPosGet(positionKey)[1] / 100.0f;
    zRaw = (float) effectPosGet(positionKey)[2];
    position.w = 1.0f;
    position.z = zRaw / 100.0f;
    argOne = vmArgGet(thread, 1);
    argTwo = vmArgGet(thread, 2);
    effectStartAtPosition(argOne, argTwo, &position);
    return 1;
}


int tecParaGet(SbThread *thread)
{
    int argOne;
    int argTwo;

    if (VM_PLUGIN_CNT_GET(thread) < 2) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    argOne = vmArgGet(thread, 1);
    argTwo = vmArgGet(thread, 2);
    return tecParaFieldGet(argOne, argTwo);
}


int unitStatGet(SbThread *thread)
{
    BattleUnit *unit;
    int argTwo;
    int argThree;

    if (VM_PLUGIN_CNT_GET(thread) < 3) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitPtrGet(vmArgGet(thread, 1));
    argTwo = vmArgGet(thread, 2);
    argThree = vmArgGet(thread, 3);
    return calcStatGet(unit, argTwo, argThree);
}


int unitStatSet(SbThread *thread)
{
    BattleUnit *unit;
    int argTwo;
    int argThree;

    if (VM_PLUGIN_CNT_GET(thread) < 3) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitPtrGet(vmArgGet(thread, 1));
    argTwo = vmArgGet(thread, 2);
    argThree = vmArgGet(thread, 3);
    calcStatSet(unit, argTwo, argThree);
    return 1;
}


int counterBoost(SbThread *thread)
{
    BattleUnit *unit;
    int applied;

    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitPtrGet(vmArgGet(thread, 1));
    applied = calcBoostChk(unit, 0);
    if (applied != 0) {
        calcBoost(unit, 0);
        unitEffectStart(unit, 0x140, 7);
        seVoiceStartTagged(2, 1);
        applied = 1;
    }
    return applied;
}

int boost(SbThread *thread)
{
    BattleUnit *unit;
    int applied;

    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, SB_JUMP_ARGUMENTS);
        return 0;
    }
    unit = unitPtrGet(vmArgGet(thread, 1));
    applied = calcBoostChk(unit, 1);
    if (applied != 0) {
        calcBoost(unit, 1);
        unitEffectStart(unit, 0x140, 7);
        seVoiceStartTagged(2, 1);
        applied = 1;
    }
    return applied;
}


int VPadEnable(void)
{
    dataVPadModeSet(1);
    return 1;
}

int VPadDisable(void)
{
    dataVPadModeSet(0);
    return 1;
}
