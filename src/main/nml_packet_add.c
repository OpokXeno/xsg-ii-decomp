#include "common.h"
#include "shared.h"
#include "vu0_macro.h"
#include "main/nml_packet_add.h"

void _CurMatrixSet(Matrix4 *matrix)
{
    Vu0SetCurrentMatrix(matrix);
}

void _CurApplyMatrix(void *destination, const void *vector) {
    Vu0ApplyCurrentMatrix(destination, vector);
}

/* Applies the current 3x3 matrix (vf27..vf29) to the point in vector, xyzw lanes, no vf30 term. */
void _CurApplyMatrix33(void *destination, const void *vector) {
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n"
        "vmulax.xyzw ACC, vf27, vf31x\n"
        "vmadday.xyzw ACC, vf28, vf31y\n"
        "vmaddz.xyzw vf31, vf29, vf31z\n"
        "sqc2 vf31, 0(%0)\n"
        :
        : "r"(destination), "r"(vector)
        : "memory");
}

void _CurMatrixMul(Matrix4 matrix)
{
    __asm__ __volatile__(
        "lqc2 vf2, 0(%0)\n\t"
        "lqc2 vf3, 16(%0)\n\t"
        "lqc2 vf4, 32(%0)\n\t"
        "lqc2 vf5, 48(%0)\n\t"
        "vmulax.xyzw ACC, vf27, vf2x\n\t"
        "vmadday.xyzw ACC, vf28, vf2y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf2z\n\t"
        "vmaddw.xyzw vf2, vf30, vf2w\n\t"
        "vmulax.xyzw ACC, vf27, vf3x\n\t"
        "vmadday.xyzw ACC, vf28, vf3y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf3z\n\t"
        "vmaddw.xyzw vf3, vf30, vf3w\n\t"
        "vmulax.xyzw ACC, vf27, vf4x\n\t"
        "vmadday.xyzw ACC, vf28, vf4y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf4z\n\t"
        "vmaddw.xyzw vf4, vf30, vf4w\n\t"
        "vmulax.xyzw ACC, vf27, vf5x\n\t"
        "vmadday.xyzw ACC, vf28, vf5y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf5z\n\t"
        "vmaddw.xyzw vf30, vf30, vf5w\n\t"
        "vmulw.xyzw vf27, vf2, vf0w\n\t"
        "vmulw.xyzw vf28, vf3, vf0w\n\t"
        "vmulw.xyzw vf29, vf4, vf0w"
        :
        : "r"(matrix)
        : "memory"
    );
}
