#ifndef INCLUDE_MAIN_NML_PACKET_ADD_H
#define INCLUDE_MAIN_NML_PACKET_ADD_H

#include "shared.h"

/* Functions of src/main/nml_packet_add.c that other files call: the VU0 current-matrix helpers (vf27-vf30);
 * the vector is passed by address whatever its type (a Vector4, a float[4] row or a scratch record). */

void _CurMatrixSet(Matrix4 *matrix);
void _CurApplyMatrix(void *destination, const void *vector);
void _CurApplyMatrix33(void *destination, const void *vector);
void _CurMatrixMul(Matrix4 matrix);

#endif /* INCLUDE_MAIN_NML_PACKET_ADD_H */
