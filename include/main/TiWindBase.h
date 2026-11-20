#ifndef INCLUDE_MAIN_TIWINDBASE_H
#define INCLUDE_MAIN_TIWINDBASE_H

#include "shared.h"
#include "main/TiMRPackage.h"

/* Functions of src/main/TiWindBase.c that other files call. */

/* A record of the link table: a cell header (first node index, link count) is followed by its links (rest length, node
 * index of the other end). */
typedef union TiLinkRecord {
    struct {
        int firstNode;
        int linkCount;
    } cell;
    struct {
        float restLength;
        int node;
    } link;
    unsigned char unmodeled_00[0x10];
} TiLinkRecord;

void tiWindApplyAll(TiHrcDynamics *node, TiSolverState *state);
void tiDynamicsSolveCellPairs(TiMRPackage *world);
void tiDynamicsSolveLink(TiSolverState *node, Matrix4 other, const float *restLength);
void tiWindDirSetEnabled(u32 index, int enabled);
void tiWindDirSetDirection(u32 index, float x, float y, float z);
void tiWindDirSetPosition(u32 index, const float *position);
void tiWindDirSetSpeed(u32 index, float speed);
void tiWindDirSetRange(u32 index, float upper, float lower);
void tiWindDirSetRadius(u32 index, float radius, int global);
void tiWindPointSetEnabled(u32 index, int enabled);
void tiWindPointSetPosition(u32 index, const float *position);
void tiWindPointSetSpeed(u32 index, float speed);
void tiWindPointSetRange(u32 index, float upper, float lower);
void tiWindPointSetRadius(u32 index, float radius, int global);
void tiWindClearTables(void);
void tiWindAdvanceAll(void);
void tiWindDisableAll(void);

#endif /* INCLUDE_MAIN_TIWINDBASE_H */
