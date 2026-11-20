#include "common.h"
#include "main/cfe_list.h"
#include "main/cfe_track.h"
#include "main/cfe_node_pool.h"


/* Links `node` between `prev` and `next`; either neighbour may be null. */
void cfeListLink(CfeTrackNode *prev, CfeTrackNode *node, CfeTrackNode *next) {
    if (prev != 0) {
        prev->next = node;
    }
    node->prev = prev;
    node->next = next;
    if (next != 0) {
        next->prev = node;
    }
}

/* Unlinks `node` from between `prev` and `next`; either neighbour may be null. */
void cfeListUnlink(CfeTrackNode *prev, CfeTrackNode *node, CfeTrackNode *next) {
    if (prev != 0) {
        prev->next = next;
    }
    node->prev = 0;
    node->next = 0;
    if (next != 0) {
        next->prev = prev;
    }
}

/* Finds the node with key `key` in the sorted list at *list, or allocates one (cfeNodeAlloc) and
 * links it in key order; a found node has its reset flags cleared. */
CfeTrackNode *cfeListFindOrAdd(CfeTrackNode **list, int key)
{
    CfeTrackNode **link;
    CfeTrackNode *prev;
    CfeTrackNode *node;
    CfeTrackNode *first;

    if (*list != 0) {
        prev = 0;
        link = list;
        do {
            if (prev == 0 && key < (*link)->startTime) {
                node = cfeNodeAlloc();
                if (node == 0) {
                    return 0;
                }
                node->startTime = key;
                cfeListLink(prev, node, *link);
                *list = node;
                return node;
            }
            if ((*link)->startTime == key) {
                (*link)->flags &= ~CFE_TRACK_RESET_FIRST;
                (*link)->flags &= ~CFE_TRACK_RESET_SECOND;
                return *link;
            }
            if ((*link)->next != 0 && (*link)->startTime < key && key < (*link)->next->startTime) {
                node = cfeNodeAlloc();
                if (node == 0) {
                    return 0;
                }
                node->startTime = key;
                cfeListLink(*link, node, (*link)->next);
                return node;
            }
            prev = *link;
            link = &prev->next;
        } while (*link != 0);
        node = cfeNodeAlloc();
        if (node == 0) {
            return 0;
        }
        node->startTime = key;
        cfeListLink(prev, node, 0);
        return node;
    }
    first = cfeNodeAlloc();
    *list = first;
    if (first != 0) {
        first->startTime = key;
    }
    return first;
}

/* Sets the 8.8 fixed-point value of the key's node and clears CFE_TRACK_LINEAR, then updates the list. */
CfeTrackNode *cfeListSetValue(CfeTrackNode **list, int key, float value)
{
    CfeTrackNode *node = cfeListFindOrAdd(list, key);
    int whole;

    node->flags &= ~CFE_TRACK_LINEAR;
    whole = (int) value;
    node->value = CFE_FIXED_FROM_WHOLE(whole, value);
    cfeTrackUpdateDirections(*list);
    return node;
}

/* Same as cfeListSetValue but sets CFE_TRACK_LINEAR. */
CfeTrackNode *cfeListSetValueLinear(CfeTrackNode **list, int key, float value)
{
    CfeTrackNode *node = cfeListFindOrAdd(list, key);
    int whole = (int) value;

    node->flags |= CFE_TRACK_LINEAR;
    node->value = CFE_FIXED_FROM_WHOLE(whole, value);
    cfeTrackUpdateDirections(*list);
    return node;
}

/* Like cfeListSetValue for an angle key: also unwraps the angles along the list. */
CfeTrackNode *cfeListSetAngle(CfeTrackNode **list, int key, float value)
{
    CfeTrackNode *node = cfeListFindOrAdd(list, key);
    int whole;

    node->flags &= ~CFE_TRACK_LINEAR;
    whole = (int) value;
    node->value = CFE_FIXED_FROM_WHOLE(whole, value);
    cfeChainUnwrapAngles(*list);
    cfeTrackUpdateDirections(*list);
    return node;
}

/* Same as cfeListSetAngle but sets CFE_TRACK_LINEAR. */
CfeTrackNode *cfeListSetAngleLinear(CfeTrackNode **list, int key, float value)
{
    CfeTrackNode *node = cfeListFindOrAdd(list, key);
    int whole = (int) value;

    node->flags |= CFE_TRACK_LINEAR;
    node->value = CFE_FIXED_FROM_WHOLE(whole, value);
    cfeChainUnwrapAngles(*list);
    cfeTrackUpdateDirections(*list);
    return node;
}

/* Copies the flags and the value of `source` to the node of `key` (found or added), then updates the list. */
void cfeListCopyKey(CfeTrackNode **list, CfeTrackNode *source, int key)
{
    CfeTrackNode *node = cfeListFindOrAdd(list, key);

    node->flags = source->flags;
    node->value = source->value;
    cfeTrackUpdateDirections(*list);
}

/* Removes the first node whose key is `key` from the list starting at `head`, frees it (cfeNodeRelease) and
   returns the list head (the next node when the head itself was the match). */
CfeTrackNode *cfeListRemoveKey(CfeTrackNode *head, int key)
{
    CfeTrackNode *prev = 0;
    CfeTrackNode *node = head;
    CfeTrackNode *next;

    if (head == 0) {
        return 0;
    }
    if (head->startTime == key) {
        next = head->next;
        cfeListUnlink(0, head, next);
        cfeNodeRelease(head);
        return next;
    }
    while (node != 0) {
        next = node->next;
        if (node->startTime == key) {
            cfeListUnlink(prev, node, next);
            cfeNodeRelease(node);
            return head;
        }
        prev = node;
        node = next;
    }
    return head;
}


/* Walks the list from `head` along `next`, freeing each node and clearing its links. */
void cfeListFreeAll(CfeTrackNode *head) {
    CfeTrackNode *node;
    CfeTrackNode *next;

    node = head;
    if (node != 0) {
        do {
            next = node->next;
            cfeNodeRelease(node);
            node->prev = 0;
            node->next = 0;
            node = next;
        } while (node != 0);
    }
}
