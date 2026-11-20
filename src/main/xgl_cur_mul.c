#include "common.h"
#include "shared.h"
#include "main/xgl_cur_mul.h"
#include "main/xgl_cur_matrix.h"

f32 cosf(f32);
f32 sinf(f32);
extern Matrix4 xglMulRotMatrixX;
extern Matrix4 xglMulRotMatrixY;
extern Matrix4 xglMulRotMatrixZ;
extern Matrix4 xglMulTranslateMatrix;
extern Matrix4 xglMulScaleMatrix;
extern Matrix4 xglMulScaleXYZMatrix;

/* Rotation about X: cosine on the [1][1] and [2][2] diagonal, sine at [1][2], -sine at [2][1]. */
void xglCurMulRotateX(f32 angle)
{
    f32 cosine;
    f32 sine;

    cosine = cosf(angle);
    sine = sinf(angle);
    xglMulRotMatrixX[2][2] = cosine;
    xglMulRotMatrixX[1][1] = cosine;
    xglMulRotMatrixX[1][2] = sine;
    xglMulRotMatrixX[2][1] = -sine;
    xglCurMatrixMul(xglMulRotMatrixX);
}

/* Rotation about Y: cosine on the [0][0] and [2][2] diagonal, sine at [2][0], -sine at [0][2]. */
void xglCurMulRotateY(f32 angle)
{
    f32 cosine;
    f32 sine;

    cosine = cosf(angle);
    sine = sinf(angle);
    xglMulRotMatrixY[2][2] = cosine;
    xglMulRotMatrixY[0][0] = cosine;
    xglMulRotMatrixY[2][0] = sine;
    xglMulRotMatrixY[0][2] = -sine;
    xglCurMatrixMul(xglMulRotMatrixY);
}

/* Rotation about Z: cosine on the [0][0] and [1][1] diagonal, sine at [0][1], -sine at [1][0]. */
void xglCurMulRotateZ(f32 angle)
{
    f32 cosine;
    f32 sine;

    cosine = cosf(angle);
    sine = sinf(angle);
    xglMulRotMatrixZ[1][1] = cosine;
    xglMulRotMatrixZ[0][0] = cosine;
    xglMulRotMatrixZ[0][1] = sine;
    xglMulRotMatrixZ[1][0] = -sine;
    xglCurMatrixMul(xglMulRotMatrixZ);
}

/* Current matrix = translation matrix * current matrix; translation fills row 3 of xglMulTranslateMatrix. */
void xglCurMulTranslate(float *translation)
{
    xglMulTranslateMatrix[3][0] = translation[0];
    xglMulTranslateMatrix[3][1] = translation[1];
    xglMulTranslateMatrix[3][2] = translation[2];
    xglCurMatrixMul(xglMulTranslateMatrix);
}

/* Current matrix = uniform scale matrix (xglMulScaleMatrix) * current matrix. */
void xglCurMulScale(f32 scale)
{
    xglMulScaleMatrix[0][0] = scale;
    xglMulScaleMatrix[1][1] = scale;
    xglMulScaleMatrix[2][2] = scale;
    xglCurMatrixMul(xglMulScaleMatrix);
}

/* Current matrix = per-axis scale matrix (xglMulScaleXYZMatrix) * current matrix. */
void xglCurMulScaleXYZ(const f32 scale[3])
{
    xglMulScaleXYZMatrix[0][0] = scale[0];
    xglMulScaleXYZMatrix[1][1] = scale[1];
    xglMulScaleXYZMatrix[2][2] = scale[2];
    xglCurMatrixMul(xglMulScaleXYZMatrix);
}

extern Matrix4 xglMulInvScaleMatrix;

/* Writes 1/x, 1/y, 1/z on the diagonal of xglMulInvScaleMatrix for each non-zero component (zero components are skipped), then multiplies the current matrix by it. */
void xglCurMulInvScaleXYZ(f32 scale[3]) {
    if (scale[0] != 0.0f) {
        xglMulInvScaleMatrix[0][0] = 1.0f / scale[0];
    }
    if (scale[1] != 0.0f) {
        xglMulInvScaleMatrix[1][1] = 1.0f / scale[1];
    }
    if (scale[2] != 0.0f) {
        xglMulInvScaleMatrix[2][2] = 1.0f / scale[2];
    }
    xglCurMatrixMul(xglMulInvScaleMatrix);
}

/* Data of this TU: seven 4x4 matrices, each the identity at its original address, in address order. */
Matrix4 xglMulRotMatrixX = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
Matrix4 xglMulRotMatrixY = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
Matrix4 xglMulRotMatrixZ = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
Matrix4 xglMulTranslateMatrix = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
Matrix4 xglMulScaleMatrix = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
Matrix4 xglMulScaleXYZMatrix = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
Matrix4 xglMulInvScaleMatrix = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
