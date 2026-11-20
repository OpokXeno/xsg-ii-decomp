#ifndef INCLUDE_MAIN_CFE_NODE_POOL_H
#define INCLUDE_MAIN_CFE_NODE_POOL_H

/* Functions of src/main/cfe_node_pool.c that other files call. */

#include "main/cfe_list.h"

CfeTrackNode *cfeListTail(CfeTrackNode *node);
void cfeNodePoolSet(CfeTrackNode *records);
void cfeNodePoolClear(void);
CfeTrackNode *cfeNodeAlloc(void);
void cfeNodeRelease(CfeTrackNode *record);

#endif /* INCLUDE_MAIN_CFE_NODE_POOL_H */
