#include "common.h"
#include "main/cfe_node_pool.h"

extern void *memset(void *destination, int value, unsigned int count);

/* cfeNodePool points to a table of CFE_POOL_NODE_COUNT track nodes. cfeNodeAlloc allocates the first free
 * node, cfeNodeRelease frees one and cfeNodePoolCountInUse counts the nodes in use (CFE_TRACK_IN_USE). */
#define CFE_POOL_NODE_COUNT 6144

extern CfeTrackNode *cfeNodePool;

/* Returns the last node of the chain that starts at node, or 0 for a null node. */
CfeTrackNode *cfeListTail(CfeTrackNode *node)
{
    CfeTrackNode *last = 0;
    CfeTrackNode *next;
    CfeTrackNode *current;
    CfeTrackNode *following;

    if (node != 0) {
        next = node->next;
        last = node;
        if (next != 0) {
            do {
                current = next;
                following = current->next;
                next = following;
            } while (following != 0);
            last = current;
        }
    }
    return last;
}

/* Points the pool at a table of CFE_POOL_NODE_COUNT nodes (allocated by cfe_init.c). */
void cfeNodePoolSet(CfeTrackNode *records) {
    cfeNodePool = records;
}

/* Clears every node of the pool, freeing all of them. */
void cfeNodePoolClear(void) {
    memset(cfeNodePool, 0, CFE_POOL_NODE_COUNT * sizeof(CfeTrackNode));
}

/* Allocates the first free pool node: clears it, unlinks it and marks it in use. Returns 0 when all
 * CFE_POOL_NODE_COUNT nodes are in use. */
CfeTrackNode *cfeNodeAlloc(void)
{
    int i;

    for (i = 0; i < CFE_POOL_NODE_COUNT; i++) {
        if ((cfeNodePool[i].flags & CFE_TRACK_IN_USE) == 0) {
            memset(&cfeNodePool[i], 0, sizeof(CfeTrackNode));
            cfeNodePool[i].prev = 0;
            cfeNodePool[i].next = 0;
            cfeNodePool[i].flags |= CFE_TRACK_IN_USE;
            return &cfeNodePool[i];
        }
    }
    return 0;
}

/* Frees a pool node: clears its in-use bit (cfeNodeRelease). */
void cfeNodeRelease(CfeTrackNode *record) {
    record->flags &= ~CFE_TRACK_IN_USE;
}

/* Resets the pool: every node becomes free. */
void cfeNodePoolReset(void) {
    cfeNodePoolClear();
}

/* Number of pool nodes in use (bit 0 set). */
int cfeNodePoolCountInUse(void) {
    CfeTrackNode *record;
    int inUseCount;
    int index;
    int flags;

    inUseCount = 0;
    record = cfeNodePool;
    for (index = 0; index < CFE_POOL_NODE_COUNT; index++) {
        flags = record->flags;
        record++;
        inUseCount += flags & CFE_TRACK_IN_USE;
    }
    return inUseCount;
}

CfeTrackNode *cfeNodePool = 0;
