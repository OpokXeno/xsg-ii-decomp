#ifndef INCLUDE_MAIN_TIPARTSANIM_H
#define INCLUDE_MAIN_TIPARTSANIM_H

/* Functions of src/main/TiPartsAnim.c that other files call. */

#include "shared.h"
#include "main/TiMRPackage.h"

void tiPartsAnimSamplePose(Matrix4 *matrices, TiMRPackage *pPack, int count);
int tiMRPackageGetNodeMatrix(TiMRPackage *handle, int nodeKey, Matrix4 outMatrix, int useBaseMatrix);

#endif /* INCLUDE_MAIN_TIPARTSANIM_H */
