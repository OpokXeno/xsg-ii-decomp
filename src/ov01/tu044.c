/*
 * OV01 original TU 44: 0x00a9c928..0x00a9ee28 (91 functions).
 *
 * The SB script VM of overlay 01: thread records, operand stacks, image
 * headers ("SB  " / "SBL " magic), a 46-entry opcode table and a native
 * name table. A thread is one 0x40-byte SbThread record of sbThreadTable.
 */
#include "common.h"
#include "unaligned.h"
#include "shared.h"
#include "ov01/tu044.h"
#include "ov01/tu046.h"

static SbThread sbThreadTable[SB_THREAD_COUNT];

SbThread *sbThreadTableGet(void) {
    return sbThreadTable;
}

static int sbFlags;
/* The per-thread areas: operand stack, slot table and message queue of thread index. */
static int sbStacks[SB_THREAD_COUNT][SB_STACK_WORDS];
static SbSlot sbSlotTables[SB_THREAD_COUNT][SB_SLOT_COUNT];
static SbQueue sbChains[SB_THREAD_COUNT];
void *memset(void *destination, int value, unsigned int count);

/* Frees every thread record and clears the per-thread areas and the native counts. */
void sbInit(void)
{
    int index;

    sbFlags = 0;
    for (index = SB_THREAD_COUNT - 1; index >= 0; index--) {
        sbThreadTable[index].image = 0;
    }
    memset(sbStacks, 0, sizeof(sbStacks));
    memset(sbSlotTables, 0, sizeof(sbSlotTables));
    memset(sbChains, 0, sizeof(sbChains));
    sbResetNativeCounts();
}

/* A native method scripts call by owner and method index (sbOpCallNative); a
 * table of them ends with a null name. status 1 from func keeps the frame waiting. */
typedef struct SbNativeMethod {
    const char *name;
    int (*func)(SbThread *thread, int *status);
} SbNativeMethod;

/* A named table of native methods, registered by sbNativeOwnerAdd. */
typedef struct SbNativeOwner {
    const char *name;
    SbNativeMethod *methods;
    int count;
} SbNativeOwner;

#define SB_NATIVE_OWNER_MAX 0x600
#define SB_NATIVE_METHOD_MAX 0x800

static SbNativeOwner sbNativeOwners[SB_NATIVE_OWNER_MAX];
static int sbNativeOwnerCount;
static SbNativeMethod sbNativeMethods[SB_NATIVE_METHOD_MAX];
static int sbNativeMethodCount;

void sbResetNativeCounts(void) {
    sbNativeMethodCount = 0;
    sbNativeOwnerCount = 0;
}



extern int (*sbOpHandlers[])(SbThread *, unsigned int);
void sbRun(SbThread *thread);
u16 sbCodeU16(SbThread *thread, int index);
int sbCodeS32(SbThread *thread, int value);
void sbLocalSet(SbThread *thread, int index, int value);
void sbStackPush(SbThread *thread, int value);
int sbStackPop(SbThread *thread);
int sbOpLength(unsigned short opcode, int count);

void sbRunAll(void)
{
    int index;

    for (index = 0; index < SB_THREAD_COUNT; index++) {
        if (sbThreadTable[index].image != 0 && !(sbThreadTable[index].status & SB_THREAD_SUSPENDED)) {
            sbRun(&sbThreadTable[index]);
        }
    }
}





int sbSlotJump(SbThread *thread);
int sbStep(SbThread *thread);

void sbRun(SbThread *thread)
{
    unsigned int available;

    available = (unsigned int) (thread->image->global_offset - thread->image->code_offset) >> 1;
    do {
        if ((int) available < thread->pc) {
            sbFatal(thread);
        }
        sbSlotJump(thread);
    } while (sbStep(thread) == 0);
}

int sbIsSbImage(int *header);
int sbThreadStart(struct SbImageHeader *key, int entry);

/* Starts a thread at the image's entry; stops for good on an image without the SB magic. The call sits in a
 * do/while(0) (as from a macro): the original calls sbThreadStart with jal, and gcc only builds a sibcall for a
 * call outside every loop statement, so each plain form gives a tail call. */
void sbStartImage(struct SbImageHeader *image)
{
    if (sbIsSbImage((int *)image) == 0) {
        for (;;) {
        }
    }
    do {
        sbThreadStart(image, image->entry);
    } while (0);
}

SbSlot *sbSlotTableGet(int index) {
    return sbSlotTables[index];
}

/* The thread's slot with this key, or 0. */
SbSlot *sbSlotFind(SbThread *thread, int key)
{
    SbSlot *slot = thread->slots;
    int i;

    for (i = 0; i < SB_SLOT_COUNT; i++, slot++) {
        if (slot->key == key) {
            return slot;
        }
    }
    return 0;
}

/* Copies entry into the slot with its key, else into the first free slot;
 * returns 0 when the table is full. */
int sbSlotStore(SbThread *thread, SbSlot *entry)
{
    SbSlot *slot = sbSlotFind(thread, entry->key);
    SbSlot *slots;
    int i;

    if (slot == 0) {
        slots = thread->slots;
        for (i = 0; i < SB_SLOT_COUNT; i++) {
            if (slots[i].key == 0) {
                slot = &slots[i];
                break;
            }
        }
    }
    if (slot != 0) {
        *slot = *entry;
        return 1;
    }
    return 0;
}

/* Frees the slot with this key; returns 0 when there is none. */
int sbSlotRemove(SbThread *thread, int key)
{
    SbSlot *slot = sbSlotFind(thread, key);

    if (slot == 0) {
        return 0;
    }
    slot->key = 0;
    return 1;
}

/* Requests a jump to slot key at the current position (sbSlotJump runs it); natives
 * request SB_JUMP_ARGUMENTS when they get too few arguments. */
void sbSlotJumpRequest(SbThread *thread, int key) {
    thread->jumpKey = key;
    thread->jumpPosition = thread->pc;
}

void func_00A9CD30(SbThread *thread);

/* Runs the thread's pending slot jump (jumpKey at jumpPosition): an unknown key is
 * fatal; a position inside the slot's range jumps to its target. */
int sbSlotJump(SbThread *thread)
{
    int key;
    SbSlot *slot;
    int position;

    key = thread->jumpKey;
    if (key == 0) {
        return 0;
    }
    slot = sbSlotFind(thread, key);
    if (slot == 0) {
        thread->pc = thread->jumpPosition;
        if (key < 3) {
            func_00A9CD30(thread);
        }
        sbFatal(thread);
        return 1;
    }
    position = thread->jumpPosition;
    thread->jumpKey = 0;
    thread->jumpPosition = 0;
    if (position >= slot->low) {
        if (slot->high >= position) {
            thread->pc = slot->target;
            return 1;
        }
    }
    return 0;
}





void func_00A9CD30(SbThread *thread)
{
    if (thread->jumpKey == 1) {
        return;
    }
    if (thread->jumpKey == 2) {
        sbCodeS32(thread, thread->jumpPosition + 1);
    }
}


/* Fatal VM error (stack overflow in sbRun): reports the thread, then stops for good. */
void sbFatal(SbThread *thread)
{
    func_00A9F2E0(thread, thread->pc);
    for (;;) {
    }
}

/* Registers the null-terminated method table methods under name: appends the methods to
 * sbNativeMethods and an owner record pointing at them. 0 when either table is full, else 1.
 * The counts are compared unsigned (sltiu); index carries the owner count into the last store. */
int sbNativeOwnerAdd(const char *name, SbNativeMethod *methods)
{
    int count;
    int index;

    if (!((unsigned int) sbNativeOwnerCount < SB_NATIVE_OWNER_MAX)) {
        return 0;
    }
    sbNativeOwners[sbNativeOwnerCount].name = name;
    sbNativeOwners[sbNativeOwnerCount].methods = &sbNativeMethods[sbNativeMethodCount];
    for (count = 0; methods[count].name != 0; count++) {
        if (!((unsigned int) sbNativeMethodCount < SB_NATIVE_METHOD_MAX)) {
            return 0;
        }
        sbNativeMethods[sbNativeMethodCount++] = methods[count];
    }
    index = sbNativeOwnerCount;
    sbNativeOwnerCount = index + 1;
    sbNativeOwners[index].count = count;
    return 1;
}

int sbIsSbImage(int *header) {
    if (header == 0) {
        return 0;
    }
    return *header == 0x20204253;
}

int sbIsSblImage(int *header) {
    if (header == 0) {
        return 0;
    }
    return *header == 0x204C4253;
}

int sbLinkImage(struct SbImageHeader *image, struct VmKeyTable *library);

/*
 * Links the library image (an SBL image) into image (sbLinkImage) and makes it the image's key table. Returns 0
 * when linking fails; hangs on a library that is not an SBL image.
 */
int sbLibraryLink(struct SbImageHeader *image, struct VmKeyTable *library)
{
    if (sbLinkImage(image, library) == 0) {
        return 0;
    }
    if (!sbIsSblImage((int *)library)) {
        for (;;) {
        }
    }
    image->keys = library;
    return 1;
}

/* Opcode whose operand is an import index until sbLinkImage replaces it with the native id. */
#define SB_OP_CALL_NATIVE 0x15
/* sbFlags bit: link an image even when natives are missing (their call opcodes are blanked out). */
#define SB_FLAG_ALLOW_MISSING_NATIVES 1
/* Native id stored for an import sbFindNative does not know (its own "not found" result). */
#define SB_NATIVE_MISSING (-1)

int sbFindNative(const char *ownerName, const char *methodName);
int sbFlagsGet(void);

/*
 * Resolves the image's imports (owner name offset in the high half of each word, method name offset in the low
 * half, both into the name strings after the words) to native ids, then rewrites the import index of every
 * SB_OP_CALL_NATIVE in the code to the native id. Returns 0 when a native is missing and that is not allowed.
 */
/* An import word holds the offsets (from the name block) of its owner name in the high half and of its method
 * name in the low half. */
#define SB_IMPORT_OWNER_SHIFT 16
#define SB_IMPORT_METHOD_MASK 0xFFFF

int sbLinkImage(struct SbImageHeader *image, struct VmKeyTable *library)
{
    int operand;
    /* Taking the address up front puts operand on the stack before loop 1, so all its accesses share one form
     * and gcse keeps loop 1's value in the register of loop 2's sbOpLength argument ($5). */
    int *operandSlot = &operand;
    int *imports;
    char *names;
    u16 *code;
    u16 *arg;
    int count;
    int index;
    int length;
    u16 opcode;

    if (sbIsSbImage((int *)image) == 0) {
        for (;;) {
        }
    }
    if (image->flags & SB_IMAGE_LINKED) {
        return 1;
    }
    count = image->import_count;
    imports = (int *)((char *)image + image->import_offset);
    image->flags |= SB_IMAGE_LINKED;
    names = (char *)(imports + count);
    for (index = 0; index < count; index++) {
        int word = imports[index];

        operand = sbFindNative(names + (word >> SB_IMPORT_OWNER_SHIFT), names + (word & SB_IMPORT_METHOD_MASK));
        if (operand < 0) {
            /* if/else, not an early return: that form fills the delay slot of the return 0 branch with the -1 */
            if (sbFlagsGet() & SB_FLAG_ALLOW_MISSING_NATIVES) {
                operand = SB_NATIVE_MISSING;
            } else {
                return 0;
            }
        }
        imports[index] = operand;
    }
    code = (u16 *)((char *)image + image->code_offset);
    length = (unsigned int)(image->global_offset - image->code_offset) / sizeof(u16);
    for (index = 0; index < length; index += sbOpLength(opcode, operand)) {
        opcode = code[index];
        arg = &code[index + 1];
        UnalignedWordCopy(operandSlot, arg);
        if (opcode == SB_OP_CALL_NATIVE) {
            UnalignedWordCopy(arg, &imports[operand]);
            if (sbFlagsGet() & SB_FLAG_ALLOW_MISSING_NATIVES) {
                if (imports[operand] == SB_NATIVE_MISSING) {
                    code[index] = 0;
                    arg[0] = 0;
                    code[index + 2] = 0;
                    code[index + 3] = 0;
                }
            }
        }
    }
    return 1;
}

int strcmp(const char *a, const char *b);

/* Native id of a method: owner index in the high half, method index in the low
 * half; -1 when the owner or the method is not registered. */
int sbFindNative(const char *ownerName, const char *methodName)
{
    int owner;
    int method;

    for (owner = 0; owner < sbNativeOwnerCount; owner++) {
        if (strcmp(ownerName, sbNativeOwners[owner].name) == 0) {
            for (method = 0; method < sbNativeOwners[owner].count; method++) {
                if (strcmp(methodName, sbNativeOwners[owner].methods[method].name) == 0) {
                    return (owner << 16) + method;
                }
            }
        }
    }
    return -1;
}


int sbFlagsGet(void) {
    return sbFlags;
}

void sbFlagsSet(int bits) {
    sbFlags |= bits;
}

void sbFlagsClear(int bits) {
    sbFlags &= ~bits;
}

int *sbStackGet(int index);
SbSlot *sbSlotTableGet(int index);
SbQueue *sbChainGet(int index);
void sbChainReset(SbThread *thread);

/* Starts a thread in the first free record: key and entry name it, the pc starts at entry. */
int sbThreadStart(struct SbImageHeader *key, int entry)
{
    SbThread *thread;
    int index;

    thread = sbThreadTable;
    for (index = 0; index < SB_THREAD_COUNT; index++) {
        if (thread->image == 0) {
            memset(thread->wait, 0, sizeof(thread->wait));
            thread->pc = entry;
            thread->frame = 3;
            thread->image = key;
            thread->value = entry;
            thread->sp = 0;
            thread->jumpKey = 0;
            thread->jumpPosition = 0;
            thread->stack = sbStackGet(index);
            thread->stackLimit = SB_STACK_WORDS;
            thread->slots = sbSlotTableGet(index);
            thread->queue = sbChainGet(index);
            memset(thread->slots, 0, sizeof(sbSlotTables[0]));
            sbChainReset(thread);
            thread->status = 0;
            return 1;
        }
        thread++;
    }
    return 0;
}

/* The thread record with this key and value, or 0. */
SbThread *sbFindThread(struct SbImageHeader *key, int value)
{
    int index;

    for (index = 0; index < SB_THREAD_COUNT; index++) {
        if (sbThreadTable[index].image == key && sbThreadTable[index].value == value) {
            return &sbThreadTable[index];
        }
    }
    return 0;
}

/* Frees the thread record with this key and value; 0 when there is none. */
int sbThreadRelease(struct SbImageHeader *key, int value)
{
    SbThread *thread = sbFindThread(key, value);

    if (thread == 0) {
        return 0;
    }
    thread->image = 0;
    return 1;
}

int sbCursorPush(SbThread *thread);
SbMessage *sbChainEntryGet(SbThread *thread, int index);

/* Queues a copy of message on the thread with this key and value; sender, when
 * given, is named in the copy by its value. 0 when there is no such thread or
 * its queue is full. */
int sbThreadEnqueue(struct SbImageHeader *key, int value, SbMessage *message, SbThread *sender)
{
    SbThread *thread;
    SbMessage *entry;
    int index;

    thread = sbFindThread(key, value);
    if (thread == 0) {
        return 0;
    }
    index = sbCursorPush(thread);
    if (index == -1) {
        return 0;
    }
    entry = sbChainEntryGet(thread, index);
    *entry = *message;
    if (sender != 0) {
        entry->sender = sender->value;
    } else {
        entry->sender = -1;
    }
    return 1;
}

int sbCursorPop(SbThread *thread);
int sbChainPreviousState(SbThread *thread);

/* Dequeue side of sbThreadEnqueue: pops the next cursor index of the thread and takes its chain entry. */
SbMessage *sbThreadDequeue(struct SbImageHeader *key, int value)
{
    SbThread *thread;
    int index;

    thread = sbFindThread(key, value);
    if (thread == 0) {
        return 0;
    }
    if (sbChainPreviousState(thread) == 0) {
        return 0;
    }
    index = sbCursorPop(thread);
    return sbChainEntryGet(thread, index);
}

/* The value of the image's name-table entry called name, or -1. */
int sbNameTableFind(struct SbImageHeader *image, const char *name)
{
    SbNameEntry *entries;
    char *base;
    int count;
    int index;

    index = 0;
    count = image->name_count;
    base = (char *)image + image->name_offset;
    entries = (SbNameEntry *)base;
    for (; index < count; index++) {
        if (strcmp(name, base + entries[index].name_offset) == 0) {
            return entries[index].value;
        }
    }
    return -1;
}

/* The name of the image's name-table entry with this value, or 0. */
char *sbNameTableNameOf(struct SbImageHeader *image, int value)
{
    SbNameEntry *entries;
    char *base;
    int count;
    int index;

    index = 0;
    count = image->name_count;
    base = (char *)image + image->name_offset;
    entries = (SbNameEntry *)base;
    for (; index < count; index++) {
        if (value == entries[index].value) {
            return base + entries[index].name_offset;
        }
    }
    return 0;
}

int sbThreadEnqueue(struct SbImageHeader *key, int value, SbMessage *message, SbThread *sender);

/* Enqueues a thread at the entry the image's name table gives for name; 0 when
 * the name is not there. */
int sbThreadEnqueueByName(struct SbImageHeader *image, const char *name, SbMessage *message)
{
    int entry = sbNameTableFind(image, name);

    if (entry == -1) {
        return 0;
    }
    return sbThreadEnqueue(image, entry, message, 0);
}

void sbThreadsRelease(struct SbImageHeader *key)
{
    int index;

    for (index = 0; index < SB_THREAD_COUNT; index++) {
        if (sbThreadTable[index].image == key) {
            sbThreadRelease(key, sbThreadTable[index].value);
        }
    }
}

/* Sets the suspended bit of the thread with this key and value; 0 when there is none. */
int sbThreadSuspend(struct SbImageHeader *key, int value)
{
    SbThread *thread = sbFindThread(key, value);

    if (thread == 0) {
        return 0;
    }
    thread->status |= SB_THREAD_SUSPENDED;
    return 1;
}

void sbThreadsSuspend(struct SbImageHeader *key)
{
    int index;

    for (index = 0; index < SB_THREAD_COUNT; index++) {
        if (sbThreadTable[index].image == key) {
            sbThreadSuspend(key, sbThreadTable[index].value);
        }
    }
}

/* Clears the suspended bit of the thread with this key and value; 0 when there is none. */
int sbThreadResume(struct SbImageHeader *key, int value)
{
    SbThread *thread = sbFindThread(key, value);

    if (thread == 0) {
        return 0;
    }
    thread->status &= ~SB_THREAD_SUSPENDED;
    return 1;
}

void sbThreadsResume(struct SbImageHeader *key)
{
    int index;

    for (index = 0; index < SB_THREAD_COUNT; index++) {
        if (sbThreadTable[index].image == key) {
            sbThreadResume(key, sbThreadTable[index].value);
        }
    }
}

SbQueue *sbChainGet(int index) {
    return &sbChains[index];
}

void sbChainReset(SbThread *thread) {
    SbQueue *chain;

    chain = thread->queue;
    chain->last = SB_QUEUE_LAST;
    chain->first = 0;
    chain->tail = 0;
    chain->used = (chain->head = 0);
}

int sbCursorPush(SbThread *thread)
{
    SbQueue *cursor;
    int next;

    cursor = thread->queue;
    if (!(cursor->used < (cursor->last - cursor->first) + 1)) {
        return -1;
    }
    cursor->used = cursor->used + 1;
    next = cursor->tail + 1;
    cursor->tail = next;
    if (cursor->last < next) {
        cursor->tail = cursor->first;
    }
    return cursor->tail;
}

/* Takes the next index from the thread's cursor ring; -1 when it is empty. */
int sbCursorPop(SbThread *thread)
{
    SbQueue *cursor;
    int next;

    cursor = thread->queue;
    if (cursor->used <= 0) {
        return -1;
    }
    next = cursor->head + 1;
    cursor->used = cursor->used - 1;
    cursor->head = next;
    if (cursor->last < next) {
        cursor->head = cursor->first;
    }
    return cursor->head;
}

int sbChainPreviousState(SbThread *thread) {
    return thread->queue->used;
}

SbMessage *sbChainEntryGet(SbThread *thread, int index) {
    return &thread->queue->messages[index];
}

int *sbStackGet(int index) {
    return sbStacks[index];
}

u16 sbCodeU16(SbThread *thread, int index) {
    struct SbImageHeader *layout;

    layout = thread->image;
    return ((u16 *) ((unsigned char *) layout + layout->code_offset))[index];
}

/*
 * The 32-bit code word at halfword `index` of the code stream. It sits at a halfword position, so it is copied
 * bytewise (UnalignedWordCopy, see unaligned.h).
 */
int sbCodeS32(SbThread *thread, int index) {
    struct SbImageHeader *layout;
    int value;

    layout = thread->image;
    UnalignedWordCopy(&value, (u16 *) ((unsigned char *) layout + layout->code_offset) + index);
    return value;
}

int sbGlobalGet(SbThread *thread, int index) {
    struct SbImageHeader *layout;

    layout = thread->image;
    return ((int *) ((unsigned char *) layout + layout->global_offset))[index];
}

void sbGlobalSet(SbThread *thread, int index, int value) {
    struct SbImageHeader *layout;

    layout = thread->image;
    ((int *) ((unsigned char *) layout + layout->global_offset))[index] = value;
}

/* Local read: the locals sit in the operand stack, from word frame on. */
int sbLocalGet(SbThread *thread, int index)
{
    int *locals;

    locals = thread->stack + thread->frame;
    return locals[index];
}

/* Local write: word frame + index of the operand stack. */
void sbLocalSet(SbThread *thread, int index, int value) {
    int *locals;

    locals = thread->stack + thread->frame;
    locals[index] = value;
}

/* Pushes value on the frame's operand stack; a full stack is fatal. */
void sbStackPush(SbThread *thread, int value)
{
    if (thread->sp >= thread->stackLimit) {
        sbFatal(thread);
    }
    thread->stack[thread->sp++] = value;
}

int sbStackPop(SbThread *thread)
{
    int top;

    if (thread->sp <= 0) {
        sbFatal(thread);
    }
    top = thread->sp - 1;
    thread->sp = top;
    return thread->stack[top];
}

/* Opcode with an operand list: one byte opcode word plus `count` words. */
#define SB_OP_SWITCH 0x25

extern signed char sbOpLengths[];

/* Length of an instruction: from sbOpLengths, or 4 * count + 5 for SB_OP_SWITCH. */
int sbOpLength(unsigned short opcode, int count)
{
    if (opcode == SB_OP_SWITCH) {
        return count * 4 + 5;
    }
    return sbOpLengths[opcode];
}

/* One step of the VM: fetch the opcode at pc and run its handler. */
int sbStep(SbThread *thread)
{
    unsigned int opcode;
    int original_total;
    int result;

    opcode = sbCodeU16(thread, thread->pc);
    if (opcode < 0x2E) {
        original_total = thread->pc;
        result = sbOpHandlers[opcode](thread, opcode);
        if (result == 1) {
            thread->status = result;
            thread->pc = original_total;
        } else {
            thread->status = 0;
        }
        if (result == 2) {
            thread->pc = thread->value;
        }
    } else {
        result = 2;
        sbFatal(thread);
    }
    return result;
}

/* Opcode 0x00: no effect; advances past the opcode. */
int sbOpNop(SbThread *thread, int opcode)
{
    thread->pc += sbOpLength(opcode & 0xFFFF, 0);
    return 0;
}

/* Operators of the sbOpBinary operand word. */
#define SB_BIN_ADD 1
#define SB_BIN_SUB 2
#define SB_BIN_MUL 3
#define SB_BIN_DIV 4
#define SB_BIN_MOD 5
#define SB_BIN_OR 6
#define SB_BIN_AND 7
#define SB_BIN_SHIFT_RIGHT 8
#define SB_BIN_SHIFT_LEFT 9
#define SB_BIN_NEG 10
#define SB_BIN_EQ 11
#define SB_BIN_GT 12
#define SB_BIN_LT 13
#define SB_BIN_GE 14
#define SB_BIN_LE 15
#define SB_BIN_NE 16
#define SB_BIN_LOGICAL_OR 17
#define SB_BIN_LOGICAL_AND 18

/* Opcode 0x01: pops the right and the left operand (only the right one for SB_BIN_NEG), applies the operator of
 * the operand word and pushes the result. A division or remainder by zero requests the SB_JUMP_ZERO_DIVIDE jump and
 * pushes 0; an unknown operator is fatal. */
int sbOpBinary(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int right;
    int left;
    int result;
    int operation;

    opcode16 = opcode;
    operation = sbCodeS32(thread, thread->pc + 1);
    if (operation == SB_BIN_NEG) {
        right = sbStackPop(thread);
        left = 0;
    } else {
        right = sbStackPop(thread);
        left = sbStackPop(thread);
    }
    switch (operation) {
    case SB_BIN_ADD:
        result = left + right;
        break;
    case SB_BIN_SUB:
        result = left - right;
        break;
    case SB_BIN_MUL:
        result = left * right;
        break;
    case SB_BIN_DIV:
        if (right == 0) {
            result = 0;
            sbSlotJumpRequest(thread, SB_JUMP_ZERO_DIVIDE);
            break;
        }
        result = left / right;
        break;
    case SB_BIN_MOD:
        if (right == 0) {
            result = 0;
            sbSlotJumpRequest(thread, SB_JUMP_ZERO_DIVIDE);
            break;
        }
        result = left % right;
        break;
    case SB_BIN_OR:
        result = left | right;
        break;
    case SB_BIN_AND:
        result = left & right;
        break;
    case SB_BIN_SHIFT_RIGHT:
        result = left >> right;
        break;
    case SB_BIN_SHIFT_LEFT:
        result = left << right;
        break;
    case SB_BIN_NEG:
        result = -right;
        break;
    case SB_BIN_EQ:
        result = left == right;
        break;
    case SB_BIN_GT:
        result = left > right;
        break;
    case SB_BIN_LT:
        result = left < right;
        break;
    case SB_BIN_GE:
        result = left >= right;
        break;
    case SB_BIN_LE:
        result = left <= right;
        break;
    case SB_BIN_NE:
        result = left != right;
        break;
    case SB_BIN_LOGICAL_OR:
        result = left || right;
        break;
    case SB_BIN_LOGICAL_AND:
        result = left && right;
        break;
    default:
        sbFatal(thread);
        result = 0;
        break;
    }
    sbStackPush(thread, result);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x02: no effect; skips the opcode and its operand word. */
int func_00A9DDE0(SbThread *thread, int opcode)
{
    thread->pc += sbOpLength(opcode & 0xFFFF, 0);
    return 0;
}

int sbOpPushWord(SbThread *thread, int amount)
{
    unsigned short amount16;
    int slot;

    amount16 = (unsigned short) amount;
    slot = sbCodeS32(thread, thread->pc + 1);
    sbStackPush(thread, slot);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

/* Opcode 0x04: pushes the operand word after the opcode, as sbOpPushWord does. */
int func_00A9DE80(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int operand;

    opcode16 = (unsigned short) opcode;
    operand = sbCodeS32(thread, thread->pc + 1);
    sbStackPush(thread, operand);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x05: no effect; skips the opcode and its operand word. */
int func_00A9DEE0(SbThread *thread, int opcode)
{
    thread->pc += sbOpLength(opcode & 0xFFFF, 0);
    return 0;
}

/* Opcode 0x06: pushes the operand word after the opcode, as sbOpPushWord does. */
int func_00A9DF20(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int operand;

    opcode16 = (unsigned short) opcode;
    operand = sbCodeS32(thread, thread->pc + 1);
    sbStackPush(thread, operand);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x07: pushes the operand word after the opcode, as sbOpPushWord does. */
int func_00A9DF80(SbThread *thread, int opcode)
{
    int opcode16;
    int operand;

    opcode16 = opcode & 0xFFFF;
    operand = sbCodeS32(thread, thread->pc + 1);
    sbStackPush(thread, operand);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

int sbGlobalGet(SbThread *thread, int index);

int sbOpPushGlobal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int slot;
    int next_slot;

    amount16 = (unsigned short) amount;
    slot = sbCodeS32(thread, thread->pc + 1);
    next_slot = sbGlobalGet(thread, slot);
    sbStackPush(thread, next_slot);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

int sbLocalGet(SbThread *thread, int index);

int sbOpPushLocal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int slot;
    int next_slot;

    amount16 = (unsigned short) amount;
    slot = sbCodeS32(thread, thread->pc + 1);
    next_slot = sbLocalGet(thread, slot);
    sbStackPush(thread, next_slot);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

void sbGlobalSet(SbThread *thread, int index, int value);

int sbOpStoreGlobal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int first;
    int second;

    amount16 = (unsigned short) amount;
    first = sbStackPop(thread);
    second = sbStackPop(thread);
    sbGlobalSet(thread, first, second);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

int sbOpStoreLocal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int first;
    int second;

    amount16 = (unsigned short) amount;
    first = sbStackPop(thread);
    second = sbStackPop(thread);
    sbLocalSet(thread, first, second);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

/* Stores into a global like the plain store but leaves the value on the stack. */
int sbOpStoreGlobalKeep(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int first;
    int second;

    opcode16 = (unsigned short) opcode;
    first = sbStackPop(thread);
    second = sbStackPop(thread);
    sbGlobalSet(thread, first, second);
    thread->sp++;
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Stores into a local like the plain store but leaves the value on the stack. */
int sbOpStoreLocalKeep(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int first;
    int second;

    opcode16 = (unsigned short) opcode;
    first = sbStackPop(thread);
    second = sbStackPop(thread);
    sbLocalSet(thread, first, second);
    thread->sp++;
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

int sbOpJump(SbThread *thread)
{
    thread->pc = sbCodeS32(thread, thread->pc + 1);
    return 0;
}

int sbOpJumpIfZero(SbThread *thread, unsigned short id)
{
    int next;

    next = sbCodeS32(thread, thread->pc + 1);
    if (sbStackPop(thread) == 0) {
        thread->pc = next;
    } else {
        thread->pc += sbOpLength(id, 0);
    }
    return 0;
}

int sbOpJumpIfNotZero(SbThread *thread, unsigned short id)
{
    int next;

    next = sbCodeS32(thread, thread->pc + 1);
    if (sbStackPop(thread) != 0) {
        thread->pc = next;
    } else {
        thread->pc += sbOpLength(id, 0);
    }
    return 0;
}

/*
 * Opcode 0x11: pushes a call frame header (return slot, saved limit, frame
 * marker) and records the marker in the local at -1.
 */
int sbOpEnterFrame(SbThread *thread, unsigned short opcode)
{
    sbStackPush(thread, 0);
    sbStackPush(thread, thread->frame);
    sbStackPush(thread, 0);
    sbLocalSet(thread, -1, thread->sp);
    thread->pc += sbOpLength(opcode, 0);
    return 0;
}

/* Opcode 0x12: call. Stores the return position in the frame's return slot and jumps to the operand. */
int sbOpCall(SbThread *thread)
{
    thread->frame = sbLocalGet(thread, -1);
    sbLocalSet(thread, -3, thread->pc + 2);
    thread->pc = sbCodeS32(thread, thread->pc + 1);
    return 0;
}

/* Opcode 0x13: return. Restores the stack depth, the saved limit and the pc from the frame header. */
int sbOpReturn(SbThread *thread, unsigned short opcode)
{
    if (thread->sp == 0) {
        sbFatal(thread);
    }
    thread->sp = thread->frame;
    sbStackPop(thread);
    thread->frame = sbStackPop(thread);
    thread->pc = sbStackPop(thread);
    thread->pc += sbOpLength(opcode, 0);
    return 0;
}

/* Opcode 0x14: return with a value. The top of the stack survives the frame restore. */
int sbOpReturnValue(SbThread *thread, unsigned short opcode)
{
    int returnValue;

    returnValue = sbStackPop(thread);
    if (thread->sp == 0) {
        sbFatal(thread);
    }
    thread->sp = thread->frame;
    sbStackPop(thread);
    thread->frame = sbStackPop(thread);
    thread->pc = sbStackPop(thread);
    sbStackPush(thread, returnValue);
    thread->pc += sbOpLength(opcode, 0);
    return 0;
}

/* Native call opcode: the word after the opcode is the native id (sbFindNative).
 * Status 1 from the method repeats the opcode on the next run; otherwise the
 * arguments (a count word over two words per argument) are dropped and the
 * result is pushed. */
int sbOpCallNative(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int id;
    int status;
    int result;
    int words;

    opcode16 = opcode;
    id = sbCodeS32(thread, thread->pc + 1);
    status = 0;
    result = sbNativeOwners[id >> 16].methods[id & 0xFFFF].func(thread, &status);
    if (status == 1) {
        return 1;
    }
    words = thread->stack[thread->sp - 1] * 2 + 1;
    thread->sp -= words;
    sbStackPush(thread, result);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

int sbOpSpawn(SbThread *thread, int amount)
{
    unsigned short amount16;
    int slot;

    amount16 = (unsigned short) amount;
    slot = sbStackPop(thread);
    thread->pc += sbOpLength(amount16, 0);
    return (sbThreadStart(thread->image, slot) != 0) ? 0 : 2;
}

/* Kills the thread whose value is popped; 2 (stop) when the thread killed itself. */
int sbOpKillThread(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int value;
    int result;

    opcode16 = (unsigned short) opcode;
    value = sbStackPop(thread);
    thread->pc += sbOpLength(opcode16, 0);
    result = sbThreadRelease(thread->image, value);
    if (result != 0) {
        result = 2;
        if (thread->value != value) {
            result = 0;
        }
        return result;
    }
    return 2;
}

int sbOpDup(SbThread *thread, int value)
{
    int operand;

    operand = value & 0xFFFF;
    sbStackPush(thread, thread->stack[thread->sp - 1]);
    thread->pc += sbOpLength(operand, 0);
    return 0;
}

int sbOpDrop(SbThread *thread, int value)
{
    thread->sp -= 1;
    thread->pc += sbOpLength(value & 0xFFFF, 0);
    return 0;
}

int sbOpLoadGlobal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int slot;
    int next_slot;

    amount16 = (unsigned short) amount;
    slot = sbStackPop(thread);
    next_slot = sbGlobalGet(thread, slot);
    sbStackPush(thread, next_slot);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

int sbOpLoadLocal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int slot;
    int next_slot;

    amount16 = (unsigned short) amount;
    slot = sbStackPop(thread);
    next_slot = sbLocalGet(thread, slot);
    sbStackPush(thread, next_slot);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

/* Opcode 0x1C: returns 2, which sbStep takes as a rewind of the pc to the thread entry. */
int sbOpEnd(SbThread *thread, int opcode)
{
    thread->pc += sbOpLength(opcode & 0xFFFF, 0);
    return 2;
}

/* Opcode 0x1D: returns 3; sbStep keeps the advanced pc and the run stops for this frame. */
int sbOpYield(SbThread *thread, int opcode)
{
    thread->pc += sbOpLength(opcode & 0xFFFF, 0);
    return 3;
}

int sbOpWaitFrames(SbThread *thread, int amount)
{
    unsigned short amount16;
    int result;

    amount16 = (unsigned short) amount;
    if (thread->status == 0) {
        thread->wait[0] = sbStackPop(thread);
        result = 1;
    } else {
        thread->wait[0] -= 1;
        if (thread->wait[0] <= 0) {
            result = 0;
            thread->pc += sbOpLength(amount16, 0);
        } else {
            result = 1;
        }
    }
    return result;
}

SbMessage *sbThreadDequeue(struct SbImageHeader *key, int value);

/* Send opcode: pops a thread value and an entry index and queues that entry as a
 * message from this thread to the thread with the same key and the popped value. */
int sbOpSend(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int value;
    SbMessage message;

    opcode16 = opcode;
    value = sbStackPop(thread);
    /* The message is the four words from global index of the image (vmEntryGet). */
    message = *(SbMessage *)vmEntryGet(thread, sbStackPop(thread));
    sbThreadEnqueue(thread->image, value, &message, thread);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Receive opcode: pops a thread value and an entry index, takes the next message
 * of the thread with the same key and that value into the entry and pushes 1, or
 * pushes 0 when there is none. */
int sbOpReceive(SbThread *thread, int opcode)
{
    unsigned short opcode16;
    int value;
    SbMessage *out;
    SbMessage *message;
    int found;

    opcode16 = opcode;
    value = sbStackPop(thread);
    out = vmEntryGet(thread, sbStackPop(thread));
    message = sbThreadDequeue(thread->image, value);
    if (message != 0) {
        *out = *message;
        found = 1;
    } else {
        found = 0;
    }
    sbStackPush(thread, found);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

int sbOpIncGlobal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int first;
    int second;

    amount16 = (unsigned short) amount;
    first = sbStackPop(thread);
    second = sbGlobalGet(thread, first);
    sbGlobalSet(thread, first, second + 1);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

int sbOpIncLocal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int first;
    int second;

    amount16 = (unsigned short) amount;
    first = sbStackPop(thread);
    second = sbLocalGet(thread, first);
    sbLocalSet(thread, first, second + 1);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

int sbOpDecGlobal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int first;
    int second;

    amount16 = (unsigned short) amount;
    first = sbStackPop(thread);
    second = sbGlobalGet(thread, first);
    sbGlobalSet(thread, first, second - 1);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

int sbOpDecLocal(SbThread *thread, int amount)
{
    unsigned short amount16;
    int first;
    int second;

    amount16 = (unsigned short) amount;
    first = sbStackPop(thread);
    second = sbLocalGet(thread, first);
    sbLocalSet(thread, first, second - 1);
    thread->pc += sbOpLength(amount16, 0);
    return 0;
}

/* Switch opcode: pops a value and looks it up by binary search in the sorted
 * (value, target) pairs after the opcode (count at +1, default target at +3,
 * pair i at +5 + 4 * i); the pc moves to the matching or the default target. */
int sbOpSwitch(SbThread *thread)
{
    int low;
    int high;
    int mid;
    int key;
    int value;

    low = 0;
    key = sbStackPop(thread);
    high = sbCodeS32(thread, thread->pc + 1);
    while (low < high) {
        mid = (low + high) / 2;
        value = sbCodeS32(thread, thread->pc + mid * 4 + 5);
        if (value < key) {
            low = mid + 1;
        } else if (key < value) {
            high = mid;
        } else {
            thread->pc = sbCodeS32(thread, thread->pc + mid * 4 + 7);
            return 0;
        }
    }
    thread->pc = sbCodeS32(thread, thread->pc + 3);
    return 0;
}

/* Operand length in halfwords of each opcode (sbOpLength); 0 for the variable-length switch. */
signed char sbOpLengths[48] = {
    1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 1,
    1, 1, 3, 3, 3, 1, 3, 1, 1, 3, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
};

/* Handlers 0x26..0x2C are in tu045. */
int sbPushConst0(SbThread *thread, int opcode);
int func_00A9EE78(SbThread *thread, int opcode);
int sbPushConst1(SbThread *thread, int opcode);
int sbPushConst2(SbThread *thread, int opcode);
int sbPushConst3(SbThread *thread, int opcode);
int sbPushConst4(SbThread *thread, int opcode);
int sbPushConst5(SbThread *thread, int opcode);
int sbPushConst6(SbThread *thread, int opcode);

/* Handler of each opcode, indexed by opcode (sbStep). */
int (*sbOpHandlers[46])(SbThread *, unsigned int) = {
    sbOpNop,
    sbOpBinary,
    func_00A9DDE0,
    sbOpPushWord,
    func_00A9DE80,
    func_00A9DEE0,
    func_00A9DF20,
    func_00A9DF80,
    sbOpPushGlobal,
    sbOpPushLocal,
    sbOpStoreGlobal,
    sbOpStoreLocal,
    sbOpStoreGlobalKeep,
    sbOpStoreLocalKeep,
    sbOpJump,
    sbOpJumpIfZero,
    sbOpJumpIfNotZero,
    sbOpEnterFrame,
    sbOpCall,
    sbOpReturn,
    sbOpReturnValue,
    sbOpCallNative,
    sbOpSpawn,
    sbOpKillThread,
    sbOpDup,
    sbOpDrop,
    sbOpLoadGlobal,
    sbOpLoadLocal,
    sbOpEnd,
    sbOpYield,
    sbOpWaitFrames,
    sbOpSend,
    sbOpReceive,
    sbOpIncGlobal,
    sbOpIncLocal,
    sbOpDecGlobal,
    sbOpDecLocal,
    sbOpSwitch,
    sbPushConst0,
    func_00A9EE78,
    sbPushConst1,
    sbPushConst2,
    sbPushConst3,
    sbPushConst4,
    sbPushConst5,
    sbPushConst6,
};
