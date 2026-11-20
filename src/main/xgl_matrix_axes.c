#include "common.h"
#include "shared.h"
#include "main/xgl_matrix_axes.h"
#include "main/xgl_2.h"

/* VU0 data-memory quadword address of the VU1 floating-point register window, and its quadword count. */
#define VU1_FLOAT_WINDOW 0x400
#define VU1_FLOAT_REGISTER_QUADS 32

/*
 * Reads the 32 quadwords of the VU1 floating-point registers through the VU1
 * register window of VU0 data memory (quadword address 0x400) with
 * post-incremented VI loads. Every value lands in one scratch quadword and is
 * discarded.
 */
void xglVu1ReadFloatRegisters(void)
{
    AlignedVector4 scratch;
    int i;

    __asm__ __volatile__("ctc2 %0,$vi1" : : "r"(VU1_FLOAT_WINDOW) : "memory");
    for (i = VU1_FLOAT_REGISTER_QUADS - 1; i >= 0; i--) {
        __asm__ __volatile__("vlqi.xyzw $vf2xyzw,($vi1++)" : : : "memory");
        __asm__ __volatile__("sqc2 $vf2,0(%0)" : : "r"(&scratch) : "memory");
    }
}

#include "quadword.h"

/* Transposes a 4x4 matrix. The source is copied first because callers pass the
 * same matrix as both arguments (xglMatrixFromAxes). */
void xglMatrixReverseInPlace(Matrix4 destination, Matrix4 source)
{
    Matrix4 source_copy;

    QuadwordCopyMatrix(source_copy, source);
    destination[0][0] = source_copy[0][0];
    destination[0][1] = source_copy[1][0];
    destination[0][2] = source_copy[2][0];
    destination[0][3] = source_copy[3][0];
    destination[1][0] = source_copy[0][1];
    destination[1][1] = source_copy[1][1];
    destination[1][2] = source_copy[2][1];
    destination[1][3] = source_copy[3][1];
    destination[2][0] = source_copy[0][2];
    destination[2][1] = source_copy[1][2];
    destination[2][2] = source_copy[2][2];
    destination[2][3] = source_copy[3][2];
    destination[3][0] = source_copy[0][3];
    destination[3][1] = source_copy[1][3];
    destination[3][2] = source_copy[2][3];
    destination[3][3] = source_copy[3][3];
}


/* Builds a 4x4 matrix whose rows are the normalized negated axes, with a last row of (0, 0, 0, 1). */
void xglMatrixFromAxes(Matrix4 matrix, const Vector4 *axis0, const Vector4 *axis1, const Vector4 *axis2)
{
    Vector4 row;
    unsigned int i;

    xglVectorScaleXYZ(&row, axis0, -1.0f);
    xglVectorNormal(&row, &row);
    for (i = 0; i < 4; i++) {
        float *dst = matrix[0] + i;
        float *src = &row.x + i;

        *dst = *src;
    }
    xglVectorScaleXYZ(&row, axis1, -1.0f);
    xglVectorNormal(&row, &row);
    {
        float *src = &row.x;

        for (i = 0; i < 4; i++) {
            matrix[1][i] = src[i];
        }
    }
    xglVectorScaleXYZ(&row, axis2, -1.0f);
    xglVectorNormal(&row, &row);
    {
        float *src = &row.x;

        for (i = 0; i < 4; i++) {
            matrix[2][i] = src[i];
        }
    }
    matrix[3][3] = 1.0f;
    matrix[3][0] = matrix[3][1] = matrix[3][2] = 0.0f;

    xglMatrixReverseInPlace(matrix, matrix);
}
