#ifndef INCLUDE_MAIN_XRG_RAND_INT_H
#define INCLUDE_MAIN_XRG_RAND_INT_H

#include "shared.h"

/* Functions of src/main/xrg_rand_int.c that other files call. */

float XrgInnerVector(Vector4 *first, Vector4 *second);
void XrgOuterVector(Vector4 *destination, Vector4 *first, Vector4 *second);
void XrgQuaternionSlerp(Vector4 *result, Vector4 *from, Vector4 *to, f32 t);

#endif /* INCLUDE_MAIN_XRG_RAND_INT_H */
