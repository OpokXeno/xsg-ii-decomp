#ifndef INCLUDE_OV01_OBJ_SLOT_H
#define INCLUDE_OV01_OBJ_SLOT_H

/* Functions of src/ov01/obj_slot.c that other files call, and the lookup block they share. */

#define DATA_LOOKUP_CAPACITY 12 /* items in a DataLookup block */

#define SAVE_STACK_LEVEL_CELLS 5  /* cells per SaveStack level */
#define SAVE_STACK_LEVELS 4       /* levels of a SaveStack (a push to this depth is refused) */

struct SaveStack;
/* A step of the battle step chain: ov01StepSetNext and ov01StackPush take one, and the chain driver calls it with its stack. */
typedef void (*SaveStackStep)(struct SaveStack *context);

/* One 0x18-byte item of a lookup block. dataLookupAppend clears the word at +4 with an int store, which is why it is
 * a union there. */
struct DataLookupItem {
    int command; /* +0x00: the command word of a queued record (unitRunCommands dispatches on it); battle_init.c's match key reads it back */
    union {
        void (*nextStep)(int context);
        int word;
    } next;
    int unmodeled_08[4];
};

/* Lookup block (reached through DataTarget.lookup in battle_init.c, and as the command queue at +0x10 of an BattleUnit in unit_cmd.c):
 * up to DATA_LOOKUP_CAPACITY 0x18-byte items, the read cursor and the index of the last item (-1 when empty). */
struct DataLookup {
    struct DataLookupItem items[DATA_LOOKUP_CAPACITY];
    int cursor;
    int lastIndex;
    int flag;
};

/* Save stack of SAVE_STACK_LEVELS levels. `top` is the live value and `count` the depth. Level L has SAVE_STACK_LEVEL_CELLS
 * cells, cells[5L + slot]; the push from depth c keeps the old top in cells[5c - 1] (unmodeled_0c for c = 0). */
struct SaveStack {
    int unmodeled_00;
    union {
        int top;
        SaveStackStep step;   /* the successor step ov01StepSetNext stores, and ov01StackPush pushes; the same word as top */
    };
    int unmodeled_08;
    int unmodeled_0c;
    int cells[SAVE_STACK_LEVELS * SAVE_STACK_LEVEL_CELLS - 1];
    int count;
};

/* Index ring of the slots base..maxIndex (the same ring as X1's Fifo in obj.c): readIndex and writeIndex
 * are its cursors, count the number of entries. */
struct FifoRing {
    int base;
    int maxIndex;
    int readIndex;
    int writeIndex;
    int count;
};


/* Stores the 0x18-byte record at the next item and returns the item, or 0 when the block is full. */
struct DataLookupItem *dataLookupAppend(struct DataLookup *lookup, void *record);
void objCmdClear(struct DataLookup *lookup);
void *objCmdPtrGet(struct DataLookup *lookup);
void *objCmdNext(struct DataLookup *lookup);
void dataObjRelease(void *object);
/* Claims a free slot of the object table and returns it (0 when all are claimed). Returned as void *: each caller
 * reads the claimed record as its own panel struct (MenuState, IconListPanel, ...). */
void *objSlotAlloc(void);
void ov01ClearTables(void);

/* The overlay heap: the address of its first block and the end address (ov01HeapInit sets both). */
struct Ov01HeapRange {
    unsigned int first;
    unsigned int end;
};
extern struct Ov01HeapRange ov01HeapStart;

void fifoInit(struct FifoRing *ring, int capacity);
int fifoEdGet(struct FifoRing *ring);
int fifoPush(struct FifoRing *ring);
int fifoPop(struct FifoRing *ring);
int fifoStGet(struct FifoRing *ring);
int fifoNumGet(struct FifoRing *ring);
void ov01ResetAll(void);
int ov01RollPercent(int percent);
int rnd(int max);
struct SaveStack *ov01StackNew(void);
void ov01StackPop(struct SaveStack *stack);
/* Keeps the top in the cell below the new level and makes step the top. */
void ov01StackPush(struct SaveStack *stack, SaveStackStep step);
/* Sets the successor step of a battle step context. */
void ov01StepSetNext(struct SaveStack *stack, SaveStackStep nextStep);
struct XglPacket;
void ov01DrawAll(struct XglPacket *hookPacket);
void ov01UpdateAll(void);
/* Releases a slot claimed with objSlotAlloc. */
int objSlotRelease(void *slot);

#endif /* INCLUDE_OV01_OBJ_SLOT_H */
