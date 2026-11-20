#ifndef INCLUDE_MAIN_TIMOTIONSOLVER_H
#define INCLUDE_MAIN_TIMOTIONSOLVER_H

/* Functions of src/main/TiMotionSolver.c that other files call. */

#include "main/TiMRPackage.h"

void tiMotionSolveLocalMatrices(TiMRPackage *pMRPack);
void tiMotionSolveLocalMatricesOld(TiMRPackage *pMRPack);
void tiMotionSolveSkipNodes(TiMRPackage *pMRPack);
void tiMotionSolveSkipNodesOld(TiMRPackage *pMRPack);
void tiMotionUpdateGlobal(TiMRPackage *pMRPack);
void tiMotionUpdateVisibility(TiMRPackage *pMRPack, unsigned char *pVisible);

#endif /* INCLUDE_MAIN_TIMOTIONSOLVER_H */
