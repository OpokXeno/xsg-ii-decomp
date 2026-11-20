#ifndef INCLUDE_MAIN_XAC_EVAL_H
#define INCLUDE_MAIN_XAC_EVAL_H

#include "shared.h"
#include "main/TiXacAnimCurve.h"

/* Functions of src/main/xac_eval.c that other files call. */

void xacBuildCubicSegment(f32 *coeffs, const f32 *times, const f32 *values);
f32 xacEvalCubicSegment(f32 value, const TiXacKey *record, const f32 *coeffs);
f32 xacEvalCubicKeys(f32 time, TiXacKey *key, TiXacKey *nextKey);
f32 xacEvalLinearKeys(f32 time, TiXacKey *key, TiXacKey *nextKey);

#endif /* INCLUDE_MAIN_XAC_EVAL_H */
