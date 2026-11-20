#ifndef INCLUDE_MAIN_MODEL_CALC_CLIP_H
#define INCLUDE_MAIN_MODEL_CALC_CLIP_H

#include "shared.h"

/* Functions of src/main/model_calc_clip.c that other files call. */

void _CurRotTransPersFog(Vector4 *out, const Vector4 *in, const Vector4 *fog);
int _CurRotTransPersClipFloat(Vector4 *out, const Vector4 *in);
/* clip points at two consecutive matrices (eight rows). */
void _ModelCalcClipInitMat1(Matrix4 clip, Matrix4 matrix);
int _ModelCalcClipMat1(const Vector4 *model);
int _ModelCalcClipMat1Xy(const Vector4 *model);

#endif /* INCLUDE_MAIN_MODEL_CALC_CLIP_H */
