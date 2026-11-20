#ifndef INCLUDE_MAIN_XGL_2_H
#define INCLUDE_MAIN_XGL_2_H

#include "shared.h"
#include "main/cur_matrix_asm.h"

/* Functions of src/main/xgl_2.c that other files call. */

/* A 4x4 matrix as xgl_2.c and nml_model_set.c read it: floats, rows, words or quadwords. */
typedef unsigned int XglMatrixQuadword __attribute__((mode(TI)));

typedef union Matrix {
    float elements[16];
    Matrix4 rows;
    int words[4][4];
    XglMatrixQuadword quadwords[4];
    Vector4 vectors[4];
} Matrix;

/* Functions of src/main/xgl_2.c (the whole xgl_2 file) that other files call. */

/* Random numbers and number conversions. */
unsigned short xglSRand(void);
float xglFRand(void);
int F2I(float value);
float I2F(int value);

/* Trigonometry. */
float xglSin(float angle);
float xglCos(float angle);
float xglAtan2(float y, float x);
void xglEulerFromRotation(Vector4 *euler, Vector4 *rotation);
void xglGeometryInit(void);

/* Memory copies. */
void xglMemCopy64(void *destination, const void *source, int count);
void xglMemCopy16(void *destination, const void *source, int count);

/* Vector helpers. */
void xglRandSeedInit(void);
void xglFSrand(float seed);
void xglVectorDiv(Vector4 *destination, const Vector4 *numerator, const Vector4 *divisor);
void xglVectorDivXYZ(Vector4 *destination, const Vector4 *numerator, const Vector4 *divisor);
void xglVectorMulAddXYZ(Vector4 *destination, const Vector4 *left, const Vector4 *right, const Vector4 *addend);
void xglVectorMulSub(Vector4 *destination, const Vector4 *left, const Vector4 *right, const Vector4 *subtrahend);
void xglVectorMulSubXYZ(Vector4 *destination, const Vector4 *left, const Vector4 *right, const Vector4 *subtrahend);
void xglVectorDivAdd(Vector4 *destination, const Vector4 *numerator, const Vector4 *divisor, const Vector4 *addend);
void xglVectorDivAddXYZ(Vector4 *destination, const Vector4 *numerator, const Vector4 *divisor, const Vector4 *addend);
void xglVectorDivSub(Vector4 *destination, const Vector4 *numerator, const Vector4 *divisor, const Vector4 *subtrahend);
void xglVectorDivSubXYZ(Vector4 *destination, const Vector4 *numerator, const Vector4 *divisor, const Vector4 *subtrahend);
void xglVectorSubXYZ(Vector4 *destination, const Vector4 *left, const Vector4 *right);
void xglVectorMulAdd(Vector4 *destination, const Vector4 *left, const Vector4 *right, const Vector4 *addend);
void xglVectorScale(Vector4 *destination, const Vector4 *source, float scale);
void xglVectorScaleXYZ(Vector4 *destination, const Vector4 *source, float scale);
void xglVectorScaleAddXYZ(Vector4 *destination, const Vector4 *source, float scale, const Vector4 *addend);
void xglVectorInner(float *destination, const Vector4 *left, const Vector4 *right);
void xglVectorOuter(Vector4 *destination, const Vector4 *first, const Vector4 *second);
void xglVectorLength(float *destination, const Vector4 *vector);
float xglPointLength(Vector4 *from, Vector4 *to);
float xglPointLengthXZ(Vector4 *from, Vector4 *to);
void xglVectorNormal(Vector4 *destination, const Vector4 *source);
void xglPlaneParameter(Vector4 *plane, const Vector4 *a, const Vector4 *b, const Vector4 *c);
void xglVectorMulMat(Vector4 *destination, Matrix4 matrix, const Vector4 *vector);
void xglRotTransPersN(ScreenVector4 *destination, Matrix4 matrix, const Vector4 *source, int count, int cameraId);

/* Matrix helpers. */
extern Matrix4 xglIdentityMatrix;
extern Matrix4 xglUnit4sMatrix;
void xglMatrixUnit(Matrix4 destination);
void xglMatrixMul(Matrix4 destination, Matrix4 left, Matrix4 right);
void xglMatrixInverse(Matrix *destination, Matrix *source);
void xglMatrixScale(Matrix *destination, Matrix *source, const Vector4 *scale);
void xglMatrixTrans(Matrix *destination, Matrix *source, const Vector4 *translation);
void xglMatrixRotX(Matrix4 destination, Matrix4 source, float angle);
void xglMatrixRotY(Matrix4 destination, Matrix4 source, float angle);
void xglMatrixRotZ(Matrix4 destination, Matrix4 source, float angle);

/* The VU0 matrix stack. */
void xglMatrixStackUnit(void);
void xglMatrixStackMul(Matrix4 matrix);
void xglMatrixStackInverse(void);
void xglMatrixStackScale(const Vector4 *vector);
void xglMatrixStackTrans(const Vector4 *vector);
void xglMatrixStackRotX(float angle);
void xglMatrixStackRotY(float angle);
void xglMatrixStackRotZ(float angle);
void xglMatrixStackFrustum(float left, float right, float bottom, float top, float nearPlane, float farPlane);
void xglMatrixStackSave(Matrix4 matrix);
void xglMatrixStackLoad(Matrix4 matrix);
void xglMatrixStackPush(void);
void xglMatrixStackPop(u32 value);
void xglMatrixStackMulVector(Vector4 *destination, const Vector4 *source);
void xglMatrixStackRTPS(ScreenVector4 *destination, const Vector4 *point, Vector4 *screenScale, Vector4 *screenOffset);

#endif /* INCLUDE_MAIN_XGL_2_H */
