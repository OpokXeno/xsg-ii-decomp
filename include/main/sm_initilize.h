#ifndef INCLUDE_MAIN_SM_INITILIZE_H
#define INCLUDE_MAIN_SM_INITILIZE_H

#include "shared.h"

/* Functions of src/main/sm_initilize.c that other files call. */

/* An allocation of size bytes (rounded up to 0x800) from the large heap, 0 when it is full. */
void *smAlloc(int size);
/* A small-heap allocation of size bytes (rounded up to 0x40), 0 when the heap is full. */
void *smAllocSmall(int size);
void smFreeHandle(void *handle);
/* Makes memory of size bytes the large pool, one free cell spanning all of it. */
void smInitilize(void *memory, unsigned int size);
/* The same for the small pool. */
void smInitilizeSmall(void *memory, unsigned int size);
/* The free cell smAlloc(size) would take (0 when none fits), without allocating. */
void *smFindFree(int size);
/* Frees an allocation of smAlloc/smAllocSmall (0 is ignored) and merges the free neighbours. */
void smFree(void *allocation);

/* Constants stored after the pool code of src/main/sm_initilize.c, read by the math and effect code. */
/* Identity vector (w = 1), the default input of MMathCalcAngle and the other math helpers. */
extern const Vector4 MMathUnitVector;
/* Identity matrix, the default input of the MMathRotateMatrix* helpers. */
extern const Matrix4 MMathUnitMatrix;
/* MSeObjProc's warning format for an invalid object type (m_se_obj.c passes it to MOutputDebugStringWarn(char *)). */
extern const char MSeObjProcInvalidTypeFormat[];

#endif /* INCLUDE_MAIN_SM_INITILIZE_H */
