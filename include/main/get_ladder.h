#ifndef INCLUDE_MAIN_GET_LADDER_H
#define INCLUDE_MAIN_GET_LADDER_H

#include "shared.h"

/* Functions of src/main/get_ladder.c that other files call. */

int Ladder_Main(void);

struct SbThread;

enum {
    LADDER_SLOT_COUNT = 16,
    LADDER_MAX_RUNGS = 32
};

/* Bits of a rung's exits (LadderSlot.rungExits): the rung can be left to the left or to the right. */
enum {
    RUNG_EXIT_LEFT = 1,
    RUNG_EXIT_RIGHT = 2
};

/* One 208-byte record of the 16-slot ladder table ladderSlots; its first halfword (id) is -1 when the slot is unused. */
typedef struct LadderSlot {
    short id;                               /* 0x00 */
    unsigned char unmodeled_02[2];
    int slotIndex;                          /* 0x04: the slot's own number, -1 by ladderInitTable */
    int key;                                /* 0x08: the key passed to uwamonoGetEntryPose, -1 when the slot holds no ladder */
    unsigned char unmodeled_0c[4];
    AlignedVector4 position;                /* 0x10: the foot of the ladder, y is its height; (0, 0, 0, 1) by ladderInitTable */
    float angle;                            /* 0x20: facing angle of the ladder */
    float height;                           /* 0x24 */
    float rungs[LADDER_MAX_RUNGS];          /* 0x28: height of each rung above the foot */
    unsigned char rungExits[LADDER_MAX_RUNGS]; /* 0xA8: RUNG_EXIT_* bits */
    short rungCount;                        /* 0xC8 */
    signed char footBlocked;                /* 0xCA: 1 the foot of the ladder cannot be left */
    unsigned char unmodeled_cb[5];
} LadderSlot;

extern LadderSlot ladderSlots[LADDER_SLOT_COUNT];

/* The 8-byte ladder state block in .sdata: only the state is read or written in this TU.  As a
 * struct (not a plain short) its stores alias the actor loads, which orders the climb blocks. */
typedef struct LadderStateWork {
    short state;                            /* 0x0: 4 on the ladder, 5/6 climbing, 7 leaving, 8 top */
    short unused[3];
} LadderStateWork;

extern LadderStateWork uwamonoLadderState;

/* The ladder table and the natives of the "ladder" script module (they take the running thread). */
void ladderInitTable(void);
int ladderFindSlot(short id);
int ladderCountUsedPrefix(void);
int ladderSetLadder(struct SbThread *thread);
int ladderSetEntrance(struct SbThread *thread);
void ladderRegisterNatives(void);

#endif /* INCLUDE_MAIN_GET_LADDER_H */
