#ifndef INCLUDE_MAIN_TIMTNBLEND_H
#define INCLUDE_MAIN_TIMTNBLEND_H

/* Functions of src/main/TiMtnBlend.c that other files call. */

#include "main/TiMRPackage.h"

void tiMtnBlendSwapGlobal(TiMRPackage *ctx, int *flags);
void tiMtnBlendSwapLocal(TiMRPackage *ctx, int *flags);
void tiMtnBlendLerpMatrix(Matrix4 dst, Matrix4 a, Matrix4 b, float t);
void tiMtnBlendUpdateGlobal(TiMRPackage *ctx);
void tiMtnBlendUpdateLocal(TiMRPackage *ctx);

#endif /* INCLUDE_MAIN_TIMTNBLEND_H */
