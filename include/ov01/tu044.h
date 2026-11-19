#ifndef INCLUDE_OV01_TU044_H
#define INCLUDE_OV01_TU044_H

/*
 * The SB script VM of src/ov01/tu044.c: thread records, their per-thread areas (operand stack, jump slots,
 * message queue) and the image header. The natives of tu046.c and tu047.c receive the running thread.
 */

/* One 0x10-byte message of a thread's queue (sbThreadEnqueue): the caller's
 * words, with sender replaced by the sending thread's value (-1 for none). */
typedef struct SbMessage {
    int kind;                           /* +0x00 */
    int sender;                         /* +0x04 */
    int args[2];                        /* +0x08 */
} SbMessage;

struct VmKeyTable;

/* The SB image header: magic at +0, the native name count at +0xC, the code
 * offset at +0x14, the global table offset at +0x18 and the name table offset
 * at +0x20 (offsets count from the header). */
struct SbImageHeader {
    int magic;                          /* +0x00: "SB  " (sbIsSbImage) or "SBL " (sbIsSblImage) */
    int flags;                          /* +0x04: SB_IMAGE_LINKED once sbLinkImage ran */
    int entry;                          /* +0x08: the code entry sbStartImage starts */
    int name_count;                     /* +0x0C */
    int import_count;                   /* +0x10: native imports sbLinkImage resolves */
    int code_offset;                    /* +0x14 */
    int global_offset;                  /* +0x18 */
    struct VmKeyTable *keys;            /* +0x1C: the keyed table vmKeyedEntryGet (tu046) reads */
    int name_offset;                    /* +0x20 */
    int import_offset;                  /* +0x24: the import words, followed by their name strings */
};

/* SbImageHeader.flags bit sbLinkImage sets after resolving the imports. */
#define SB_IMAGE_LINKED 0x80000000

/* One entry of an image's name table: the name (offset from the header) and its value. */
typedef struct SbNameEntry {
    int name_offset;
    int value;
} SbNameEntry;

/* One 0x10-byte slot of a thread's slot table: key 0 marks a free slot.
 * sbSlotStore copies whole slots; the rest of the slot is not read here. */
typedef struct SbSlot {
    int key;                           /* +0x00 */
    int target;                        /* +0x04: code position sbSlotJump jumps to */
    int low;                           /* +0x08: lowest position the slot covers */
    int high;                          /* +0x0C: highest position the slot covers */
} SbSlot;

/* Thread records of sbThreadTable, and the per-thread areas sbInit clears. */
#define SB_THREAD_COUNT 80

/* Words of a thread's operand stack (0x200 bytes). */
#define SB_STACK_WORDS 0x80

/* Slots in a thread's slot table (sbSlotFind and sbSlotStore scan 8). */
#define SB_SLOT_COUNT 8

/* Messages in a thread's queue (sbChainReset: first 0, last 0xF). */
#define SB_QUEUE_LAST 0xF

/* A thread's message queue (sbChainGet, 0x114 bytes): a ring of message indices from
 * first to last, the read index head, the write index tail and the count used;
 * sbThreadEnqueue writes messages[tail], sbThreadDequeue reads messages[head]. */
typedef struct SbQueue {
    int first;                          /* +0x00 */
    int last;                           /* +0x04 */
    int head;                           /* +0x08: read index (sbCursorPop) */
    int tail;                           /* +0x0C: write index (sbCursorPush) */
    int used;                           /* +0x10 */
    SbMessage messages[SB_QUEUE_LAST + 1]; /* +0x14 */
} SbQueue;

/* sbThreadTable records with this bit set in status are skipped by sbRunAll. */
#define SB_THREAD_SUSPENDED 0x80000000

/*
 * One 0x40-byte record of sbThreadTable: a script thread. image (+0x14) is its key
 * (0 marks a free record; sbFindThread matches it with value), pc counts halfwords
 * from the image's code, the operand stack (+0x1C, stackLimit words) holds sp words
 * and the thread's locals, slots are its jump slots and queue its messages.
 */
typedef struct SbThread {
    int pc;                             /* +0x00 */
    int sp;                             /* +0x04: stack words in use */
    int frame;                          /* +0x08: stack index of the local frame (3 at start; sbOpCall saves it) */
    int jumpKey;                        /* +0x0C: pending slot jump (sbSlotJump) */
    int jumpPosition;                   /* +0x10 */
    struct SbImageHeader *image;        /* +0x14 */
    int value;                          /* +0x18: the entry it started at */
    int *stack;                         /* +0x1C */
    int stackLimit;                     /* +0x20 */
    SbSlot *slots;                      /* +0x24 */
    SbQueue *queue;                     /* +0x28 */
    int status;                         /* +0x2C: SB_THREAD_SUSPENDED; 0 on the first run of a wait */
    int wait[4];                        /* +0x30: wait[0] counts sbOpWaitFrames down; sbThreadStart clears it */
} SbThread;

/* Slot keys of a jump request (func_00A9CD30 handles keys 1 and 2): sbOpBinary requests SB_JUMP_ZERO_DIVIDE for a
 * division or remainder by zero, natives request SB_JUMP_ARGUMENTS when they get too few arguments. */
#define SB_JUMP_ZERO_DIVIDE 1
#define SB_JUMP_ARGUMENTS 2

void sbSlotJumpRequest(SbThread *thread, int key);
void sbFatal(SbThread *thread);
void sbResetNativeCounts(void);
void sbThreadsRelease(struct SbImageHeader *key);
int sbLibraryLink(struct SbImageHeader *image, struct VmKeyTable *library);
void sbRunAll(void);

#endif /* INCLUDE_OV01_TU044_H */
