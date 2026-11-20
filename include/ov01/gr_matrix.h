#ifndef INCLUDE_OV01_GR_MATRIX_H
#define INCLUDE_OV01_GR_MATRIX_H

#include "shared.h"
#include "ov01/gr_gp_init.h"

/* Functions of src/ov01/gr_matrix.c that other files call. */

/* One object's transform record: four 16-byte vectors (the w words are padding). The translation is position + offset,
 * the angles go to grRotMatrix, and scale is the vector applied last (xglMatrixScale, as in X1). */
typedef struct TransformState {
    Vector4 position;
    Vector4 offset;
    Vector4 angles;
    Vector4 scale;
} TransformState;

Matrix4 *grWorldMatGet(void);
void grCalcMatrix(TransformState *state, Matrix4 *destination);
void grRotMatrix(Matrix4 *destination, Matrix4 *source, const Vector4 *angles);
float grRotTransPers(GrScreenVertex *out, Matrix4 matrix, const Vector4 *point, int *clipFlags);
int grBBCalc(TransformState *state, int *screenX, int *screenY);

#endif /* INCLUDE_OV01_GR_MATRIX_H */
