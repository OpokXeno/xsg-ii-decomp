#ifndef INCLUDE_MAIN_TIGLOBALCOLL_H
#define INCLUDE_MAIN_TIGLOBALCOLL_H

/* Functions of src/main/TiGlobalColl.c that other files call. */

#include "main/TiMRPackage.h"

/* Collision set named by the handle: the offsets from the set to the directional and point wind headers (+0x10,
 * +0x20), three entry counts (+0x30, +0x34, +0x38), then the offsets from the set to their entry arrays (+0x3c,
 * +0x40, +0x44). */
typedef struct TiGlobalCollSet {
    unsigned char unmodeled_00[0x10];
    int windDirOffsets[4];      /* +0x10 */
    int windPointOffsets[4];    /* +0x20 */
    unsigned int sphereCount;   /* +0x30 */
    unsigned int planeCount;    /* +0x34 */
    unsigned int cylinderCount; /* +0x38 */
    int sphereEntriesOffset;    /* +0x3c */
    int planeEntriesOffset;     /* +0x40 */
    int cylinderEntriesOffset;  /* +0x44 */
} TiGlobalCollSet;

/* Series coefficients of mapHeadingXZ; the data sits in TiGlobalColl.c. */
extern float mapHeadingCoefficients[8];

void tiGlobalCollSetHandle(TiGlobalCollSet *handle);
void tiGlobalCollClearHandle(void);
void tiGlobalCollSolveBody(TiHrcDynamics *pNode, TiSolverState *context);
void tiGlobalCollClampHeight(TiSolverState *state);
TiGlobalCollSet *tiGlobalCollGetHandle(void);

#endif /* INCLUDE_MAIN_TIGLOBALCOLL_H */
