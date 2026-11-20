#include "common.h"
#include "shared.h"
#include "quadword.h"
#include "main/vu0_vector.h"

/* A variadic no-op like LOG (sef_print.c): it spills its arguments and returns. */
void LOG_00251F78(const void *data, const char *format, ...)
{
}

/* A variadic no-op like LOG (sef_print.c): it spills its arguments and returns. */
void LOG_00251FC0(const void *data, const char *format, ...)
{
}

/* Copies the vector at source to destination and sets its w to the float argument. */
void vu0CopyVectorSetW(Vector4 *destination, Vector4 *source, float w)
{
    QuadwordCopyT0(destination, source);
    destination->w = w;
}

/* Normalizes vf20 in place (xyz divided by its length) and sets w to 1. */
void vu0NormalizeVf20(void) {
    __asm__ __volatile__(
        "vmul.xyz vf1, vf20, vf20\n"
        "vaddy.x vf1, vf1, vf1y\n"
        "vaddz.x vf1, vf1, vf1z\n"
        "vrsqrt Q, vf0w, vf1x\n"
        "vmove.w vf20, vf0\n"
        "vwaitq\n"
        "vmulq.xyz vf20, vf20, Q\n" : : : "memory");
}

void vu0NormalizeVector(Vector4 *vector) {
    __asm__ __volatile__("lqc2 vf20, 0(%0)" : : "r"(vector) : "memory");
    vu0NormalizeVf20();
}

void vu0CurMatrixMul(Matrix4 matrix) {
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("lqc2 vf2, 16(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("lqc2 vf3, 32(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("lqc2 vf4, 48(%0)" : : "r"(matrix) : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf26, vf1x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf27, vf1y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf28, vf1z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf1, vf29, vf1w" : : : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf26, vf2x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf27, vf2y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf28, vf2z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf2, vf29, vf2w" : : : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf26, vf3x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf27, vf3y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf28, vf3z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf3, vf29, vf3w" : : : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf26, vf4x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf27, vf4y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf28, vf4z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf29, vf29, vf4w" : : : "memory");
    __asm__ __volatile__("vmove.xyzw vf26, vf1" : : : "memory");
    __asm__ __volatile__("vmove.xyzw vf27, vf2" : : : "memory");
    __asm__ __volatile__("vmove.xyzw vf28, vf3" : : : "memory");
}
