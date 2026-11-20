#ifndef INCLUDE_MAIN_XGL_VECTOR_OPS_H
#define INCLUDE_MAIN_XGL_VECTOR_OPS_H

#include "shared.h"
#include "main/xenoActor.h"
#include "main/TiMRPackage.h"

/* Functions of src/main/xgl_vector_ops.c that other files call. */

void tiMRPackageFillNodes(TiMRPackage *pMRPackage, TiDynamicsOut *nodes);
void XenoActorConvertSourceRecord(XenoMotionState *record);
void xglVectorAddXYZ_00292E38(Vector4 *destination, const Vector4 *first, const Vector4 *second);
void tiVu0LoadIdentityMatrix(void);

#endif /* INCLUDE_MAIN_XGL_VECTOR_OPS_H */
