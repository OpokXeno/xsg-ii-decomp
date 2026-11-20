#ifndef INCLUDE_MAIN_M_MATH_H
#define INCLUDE_MAIN_M_MATH_H

#include "shared.h"

/* Functions of src/main/m_math.c that other files call. */

/* The constants of src/main/sm_initilize.c: the unit vector (0, 0, 0, 1) and the identity matrix. */
extern const Vector4 MMathUnitVector;
extern const Matrix4 MMathUnitMatrix;

void MMathRandomSeed(int seed);
u32 MMathRandomNext(void);
float MMathMakeRandom(void);
float MMathMakeRandomSigned(void);
float MMathMakeRandom2PI(void);
float MMathMakeRandomPI(void);
void MOutputDebugStringWarn(const char *message, ...);

/* The Hermite helpers. */

typedef Vector4 HermiteVector;

float *MMathCalcHermite(float *destination, HermiteVector *tangent_first, HermiteVector *tangent_second,
                         HermiteVector *endpoint_first, HermiteVector *endpoint_second, float parameter);

void MMathCalcHermitePrm(HermiteVector *tangent_start, HermiteVector *tangent_end,
                          const HermiteVector *point_prev, const HermiteVector *point_start,
                          const HermiteVector *point_end, const HermiteVector *point_next);

/* The distance helpers. */

/* Distance from first to second, and the same in the XZ plane. */
float MMathCalcDist(const Vector4 *first, const Vector4 *second);
float MMathCalcDistXZ(const Vector4 *first, const Vector4 *second);
/* Unit vector from source towards target, stored at destination. */
Vector4 *MMathCalcDirVector(Vector4 *destination, const Vector4 *source, const Vector4 *target);

/* The vector helpers. */

/* The length of vector's xyz, and destination = source scaled to unit length. */
float MMathCalcLength(const Vector4 *vector);
Vector4 *MMathNormalizeVector2(Vector4 *destination, const Vector4 *source);
Vector4 *MMathDivVectorS(Vector4 *destination, const Vector4 *source, float scale);
short MMathIsVectorEqual(const Vector4 *first, const Vector4 *second);

/* MMathCalcAngle: pitch to out->x, yaw to out->y; returns out. */
Vector4 *MMathCalcAngle(Vector4 *out, const Vector4 *source, const Vector4 *target);

Vector4 *MMathSubVectorMulS(Vector4 *destination, const Vector4 *first, const Vector4 *second, float scale);

float MMathVectorDotProduct(const Vector4 *first, const Vector4 *second);
Vector4 *MMathVectorCrossProduct(Vector4 *destination, const Vector4 *first, const Vector4 *second);
Vector4 *MMathVectorInterpolation(Vector4 *destination, const Vector4 *first, const Vector4 *second, float parameter);

/* The rotation helpers: each rotates matrix (the unit matrix when matrix is null)
 * about the axes in its name, applied in name order, and stores the result in out, which it returns. The
 * single-axis forms take one angle; the others the x, y and z angles of angles.
 * The source matrix is not declared const: in C a `Matrix4 *` argument does not convert to a pointer to a
 * const array without a diagnostic, and most callers pass a matrix they also write. */
Matrix4 *MMathRotateMatrixX(Matrix4 *out, Matrix4 *matrix, float angle);
Matrix4 *MMathRotateMatrixY(Matrix4 *out, Matrix4 *matrix, float angle);
Matrix4 *MMathRotateMatrixZ(Matrix4 *out, Matrix4 *matrix, float angle);
Matrix4 *MMathRotateMatrixXY(Matrix4 *out, Matrix4 *matrix, const Vector4 *angles);
Matrix4 *MMathRotateMatrixYX(Matrix4 *out, Matrix4 *matrix, const Vector4 *angles);
Matrix4 *MMathRotateMatrixXYZ(Matrix4 *out, Matrix4 *matrix, const Vector4 *angles);
Matrix4 *MMathRotateMatrixYXZ(Matrix4 *out, Matrix4 *matrix, const Vector4 *angles);
Matrix4 *MMathRotateMatrixZXY(Matrix4 *out, Matrix4 *matrix, const Vector4 *angles);
Matrix4 *MMathRotateMatrixZYX(Matrix4 *out, Matrix4 *matrix, const Vector4 *angles);

/* The matrix helpers the effect files call. */

Vector4 *MMathApplyMatrix(Vector4 *out, Matrix4 *matrix, const Vector4 *vector);
/* out = lhs x rhs in the row-vector convention of MMathApplyMatrix; the sources are not declared const for the
 * reason given at the rotation helpers. */
Matrix4 *MMathMulMatrix(Matrix4 *out, Matrix4 *rhs, Matrix4 *lhs);
/* A rotation matrix at position that looks from source towards target, rolled by roll. */
Matrix4 *MMathCalcLookAtMatrix(Matrix4 *out, const Vector4 *source, const Vector4 *target, const Vector4 *position, float roll);
Matrix4 *MMathScaleMatrix(Matrix4 *out, Matrix4 *matrix, const Vector4 *scale);

/* The scale and translation helpers. */

Matrix4 *MMathScaleMatrixRotS(Matrix4 *out, Matrix4 *matrix, float scale);
Matrix4 *MMathTranslateMatrix(Matrix4 *out, Matrix4 *matrix, const Vector4 *translation);

struct XglCamera;

/* Transforms point by matrix, projects it with the camera's screen scale and offset and stores the fixed-point
 * screen position in destination, with the clip mark in w (0x8000 when clipped, else 0), which it returns.
 * The matrix is not declared const for the reason given at the rotation helpers above. */
int MMathRotTransPersClip(Vector4 *destination, const struct XglCamera *camera, Matrix4 *matrix, const Vector4 *point);
/* Shortest-arc rotation quaternion from the unit vector from to the unit vector to; 0 when they are nearly opposite. */
int MMathQuatBetweenVectors(Vector4 *destination, const Vector4 *from, const Vector4 *to);
/* The rotation matrix rows of the quaternion source, stored at destination. */
Vector4 *MMathQuatToMatrix(Vector4 *destination, const Vector4 *source);

#endif /* INCLUDE_MAIN_M_MATH_H */
