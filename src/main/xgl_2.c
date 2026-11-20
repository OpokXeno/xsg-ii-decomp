#include "common.h"
#include "main/nml_model_set.h"
#include "main/xgl_camera.h"
#include "shared.h"
#include "main/xgl_2.h"

#include "quadword.h"

#include "vu0_macro.h"
#include "main/xgl_dma.h"
#include "main/xgl_studio.h"

static unsigned long long iRandSeed;

unsigned short xglSRand(void)
{
    iRandSeed = iRandSeed * 1103515245ULL + 12345ULL;
    return (iRandSeed >> 16) & 0x7fff;
}

/* Two steps of the iRandSeed generator; returns the first step shifted left 16 plus the second state shifted right 16. */
unsigned int xglLRand(void)
{
    unsigned int first;

    first = (unsigned int)(iRandSeed * 1103515245ULL + 12345ULL);
    iRandSeed = first * 1103515245U + 12345U;
    return (first << 16) + (unsigned int)(iRandSeed >> 16);
}

/* Next VU0 random value as a float: vrnext advances R, and the low 32 bits of vf1 are the result. */
float xglFRand(void)
{
    __asm__ __volatile__("vrnext.x vf1x, R\n\tqmfc2 $2, vf1\n\tmtc1 $2, $f0" : : : "$2", "$f0", "memory");
}

void xglFSrand(register float seed)
{
    __asm__ __volatile__(
        "mfc1 $2, %0\n"
        "qmtc2 $2, vf1\n"
        "vrinit R, vf1x\n"
        :
        : "f"(seed)
        : "$2");
}

int F2I(register f32 value)
{
    register int result;

    __asm__ __volatile__(
        "mtc1 %1,$f0\n\t"
        "cvt.w.s $f0,$f0\n\t"
        "mfc1 %0,$f0"
        : "=r"(result)
        : "r"(value)
        : "$f0");
    return result;
}

f32 I2F(register int value)
{
    return (f32)value;
}

float asinf(float value);
float atan2f(float y, float x);

void xglEulerFromRotation(Vector4 *euler, Vector4 *rotation)
{
    Matrix4 matrix;

    xglMatrixStackUnit();
    xglMatrixStackRotZ(rotation->z);
    xglMatrixStackRotY(rotation->y);
    xglMatrixStackRotX(rotation->x);
    xglMatrixStackSave(matrix);

    if (matrix[2][0] < 0.0001f && -0.0001f < matrix[2][0] &&
        matrix[2][2] < 0.0001f && -0.0001f < matrix[2][2]) {
        *euler = *rotation;
    } else {
        euler->x = -asinf(matrix[2][1]);
        euler->y = atan2f(matrix[2][0], matrix[2][2]);
        euler->z = atan2f(matrix[0][1], matrix[1][1]);
    }
}

float xglAtan2(float y, float x)
{
    float s;
    float r = 0.0028662258f;
    int swap = 0;
    int quadrant = 0;
    int *bits = (int *)&y;

    if (y == 0.0f) {
        if (0.0f <= x) {
            return 0.0f;
        }
        if (*bits & 0x80000000) {
            return -3.1415927f;
        }
        return 3.1415927f;
    }
    if (x == 0.0f) {
        if (y < 0.0f) {
            return -1.5707964f;
        }
        return 1.5707964f;
    }
    if (0.0f <= y) {
        if (0.0f <= x) {
            quadrant = 0;
        } else {
            quadrant = 1;
        }
    } else {
        if (0.0f <= x) {
            quadrant = 2;
        } else {
            quadrant = 3;
        }
    }
    y = __builtin_fabsf(y);
    x = __builtin_fabsf(x);
    if (y > x) {
        y = x / y;
        swap = 1;
    } else {
        y = y / x;
    }

    s = y * y;
    r = r * s;
    r = r - 0.016165737f;
    r = r * s;
    r = r + 0.042909615f;
    r = r * s;
    r = r - 0.075289637f;
    r = r * s;
    r = r + 0.10656264f;
    r = r * s;
    r = r - 0.14208899f;
    r = r * s;
    r = r + 0.19993551f;
    r = r * s;
    r = r - 0.33333147f;
    r = r * s;
    r = r + 1.0f;
    r = r * y;

    if (swap) {
        r = 1.5707964f - r;
    }
    if (quadrant == 1) {
        r = 3.1415927f - r;
    }
    if (quadrant == 2) {
        r = -r;
    }
    if (quadrant == 3) {
        r = r - 3.1415927f;
    }
    return r;
}

void xglMemCopy64(void *destination, const void *source, int count)
{
    register void *dst = destination;
    register const void *src = source;
    register int remaining = count;

    QuadwordCopyMatrixArray(dst, src, remaining);
}

void xglMemCopy64b(void *destination, const void *source, int count)
{
    register void *dst = destination;
    register const void *src = source;
    register int remaining = count;

    __asm__ __volatile__(
        "1:\n"
        "lq $2, 0(%1)\n"
        "sq $2, 0(%0)\n"
        "lq $2, -16(%1)\n"
        "sq $2, -16(%0)\n"
        "lq $2, -32(%1)\n"
        "sq $2, -32(%0)\n"
        "lq $2, -48(%1)\n"
        "sq $2, -48(%0)\n"
        "addi %2, %2, -1\n"
        "addi %0, %0, -64\n"
        "addi %1, %1, -64\n"
        "bne $0, %2, 1b\n"
        : "+r" (dst), "+r" (src), "+r" (remaining)
        :
        : "$2", "memory");
}

void xglMemCopy16(void *destination, const void *source, int count)
{
    register void *dst = destination;
    register const void *src = source;
    register int remaining = count;

    QuadwordCopyArray(dst, src, remaining);
}


typedef unsigned int XglQuadword __attribute__((mode(TI)));


void xglMemCopy16b(u8 *destination, const u8 *source, int count)
{
    register u8 *dst = destination;
    register const u8 *src = source;
    register int n = count;
    __asm__ __volatile__(
        "1:\n\t"
        "lq $2, 0(%1)\n\t"
        "sq $2, 0(%0)\n\t"
        "addi %2, %2, -1\n\t"
        "addi %0, %0, -16\n\t"
        "addi %1, %1, -16\n\t"
        "bne $0, %2, 1b"
        : "+r"(dst), "+r"(src), "+r"(n) : : "$2", "memory");
}

void xglMemCopy8(u8 *destination, const u8 *source, int count)
{
    register u8 *dst = destination;
    register const u8 *src = source;
    register int n = count;
    __asm__ __volatile__(
        "1:\n\t"
        "ld $2, 0(%1)\n\t"
        "sd $2, 0(%0)\n\t"
        "addi %2, %2, -1\n\t"
        "addi %0, %0, 8\n\t"
        "addi %1, %1, 8\n\t"
        "bne $0, %2, 1b"
        : "+r"(dst), "+r"(src), "+r"(n) : : "$2", "memory");
}

void xglMemCopy8b(u8 *destination, const u8 *source, int count)
{
    register u8 *dst = destination;
    register const u8 *src = source;
    register int n = count;
    __asm__ __volatile__(
        "1:\n\t"
        "ld $2, 0(%1)\n\t"
        "sd $2, 0(%0)\n\t"
        "addi %2, %2, -1\n\t"
        "addi %0, %0, -8\n\t"
        "addi %1, %1, -8\n\t"
        "bne $0, %2, 1b"
        : "+r"(dst), "+r"(src), "+r"(n) : : "$2", "memory");
}

void xglMemCopy4(u8 *destination, const u8 *source, int count)
{
    register u8 *dst = destination;
    register const u8 *src = source;
    register int n = count;
    __asm__ __volatile__(
        "1:\n\t"
        "lw $2, 0(%1)\n\t"
        "sw $2, 0(%0)\n\t"
        "addi %2, %2, -1\n\t"
        "addi %0, %0, 4\n\t"
        "addi %1, %1, 4\n\t"
        "bne $0, %2, 1b"
        : "+r"(dst), "+r"(src), "+r"(n) : : "$2", "memory");
}

void xglMemCopy4b(u8 *destination, const u8 *source, int count)
{
    register u8 *dst = destination;
    register const u8 *src = source;
    register int n = count;
    __asm__ __volatile__(
        "1:\n\t"
        "lw $2, 0(%1)\n\t"
        "sw $2, 0(%0)\n\t"
        "addi %2, %2, -1\n\t"
        "addi %0, %0, -4\n\t"
        "addi %1, %1, -4\n\t"
        "bne $0, %2, 1b"
        : "+r"(dst), "+r"(src), "+r"(n) : : "$2", "memory");
}

void xglMemCopy2(u8 *destination, const u8 *source, int count)
{
    register u8 *dst = destination;
    register const u8 *src = source;
    register int n = count;
    __asm__ __volatile__(
        "1:\n\t"
        "lh $2, 0(%1)\n\t"
        "sh $2, 0(%0)\n\t"
        "addi %2, %2, -1\n\t"
        "addi %0, %0, 2\n\t"
        "addi %1, %1, 2\n\t"
        "bne $0, %2, 1b"
        : "+r"(dst), "+r"(src), "+r"(n) : : "$2", "memory");
}

void xglMemCopy2b(u8 *destination, const u8 *source, int count)
{
    register u8 *dst = destination;
    register const u8 *src = source;
    register int n = count;
    __asm__ __volatile__(
        "1:\n\t"
        "lh $2, 0(%1)\n\t"
        "sh $2, 0(%0)\n\t"
        "addi %2, %2, -1\n\t"
        "addi %0, %0, -2\n\t"
        "addi %1, %1, -2\n\t"
        "bne $0, %2, 1b"
        : "+r"(dst), "+r"(src), "+r"(n) : : "$2", "memory");
}

extern char D_20[];

float xglSin(register float angle)
{
    __asm__ __volatile__(
        "mfc1 $3, %1\n\t"
        "ctc2.i %0, $vi27\n\t"
        "vnop\n\t"
        "qmtc2 $3, $vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i $3, $vf1\n\t"
        "mtc1 $3, $f0"
        : : "r"((u32)&D_20 >> 3), "f"(angle) : "$3", "memory");
}

extern char D_E8[];

float xglCos(register float angle)
{
    __asm__ __volatile__(
        "mfc1 $3, %1\n\t"
        "ctc2.i %0, $vi27\n\t"
        "vnop\n\t"
        "qmtc2 $3, $vf4\n\t"
        "vcallmsr $vi27\n\t"
        "qmfc2.i $3, $vf1\n\t"
        "mtc1 $3, $f0"
        : : "r"((u32)&D_E8 >> 3), "f"(angle) : "$3", "memory");
}

void xglVectorDiv(Vector4 *destination, const Vector4 *numerator,
                  const Vector4 *divisor)
{
    destination->x = numerator->x / divisor->x;
    destination->y = numerator->y / divisor->y;
    destination->z = numerator->z / divisor->z;
    destination->w = numerator->w / divisor->w;
}

void xglVectorDivXYZ(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor)
{
    destination->x = numerator->x / divisor->x;
    destination->y = numerator->y / divisor->y;
    destination->z = numerator->z / divisor->z;
    destination->w = numerator->w;
}

void xglVectorMulAdd(Vector4 *destination, const Vector4 *left,
                     const Vector4 *right, const Vector4 *addend)
{
    destination->x = left->x * right->x + addend->x;
    destination->y = left->y * right->y + addend->y;
    destination->z = left->z * right->z + addend->z;
    destination->w = left->w * right->w + addend->w;
}

void xglVectorMulAddXYZ(Vector4 *destination, const Vector4 *left,
                        const Vector4 *right, const Vector4 *addend)
{
    destination->x = left->x * right->x + addend->x;
    destination->y = left->y * right->y + addend->y;
    destination->z = left->z * right->z + addend->z;
    destination->w = left->w;
}

void xglVectorMulSub(Vector4 *destination, const Vector4 *left,
                     const Vector4 *right, const Vector4 *subtrahend)
{
    destination->x = left->x * right->x - subtrahend->x;
    destination->y = left->y * right->y - subtrahend->y;
    destination->z = left->z * right->z - subtrahend->z;
    destination->w = left->w * right->w - subtrahend->w;
}

void xglVectorMulSubXYZ(Vector4 *destination, const Vector4 *left,
                        const Vector4 *right, const Vector4 *subtrahend)
{
    destination->x = left->x * right->x - subtrahend->x;
    destination->y = left->y * right->y - subtrahend->y;
    destination->z = left->z * right->z - subtrahend->z;
    destination->w = left->w;
}

void xglVectorDivAdd(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor, const Vector4 *addend)
{
    destination->x = numerator->x / divisor->x + addend->x;
    destination->y = numerator->y / divisor->y + addend->y;
    destination->z = numerator->z / divisor->z + addend->z;
    destination->w = numerator->w / divisor->w + addend->w;
}

void xglVectorDivAddXYZ(Vector4 *destination, const Vector4 *numerator,
                        const Vector4 *divisor, const Vector4 *addend)
{
    destination->x = numerator->x / divisor->x + addend->x;
    destination->y = numerator->y / divisor->y + addend->y;
    destination->z = numerator->z / divisor->z + addend->z;
    destination->w = numerator->w;
}

void xglVectorDivSub(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor, const Vector4 *subtrahend)
{
    destination->x = numerator->x / divisor->x - subtrahend->x;
    destination->y = numerator->y / divisor->y - subtrahend->y;
    destination->z = numerator->z / divisor->z - subtrahend->z;
    destination->w = numerator->w / divisor->w - subtrahend->w;
}

void xglVectorDivSubXYZ(Vector4 *destination, const Vector4 *numerator,
                        const Vector4 *divisor, const Vector4 *subtrahend)
{
    destination->x = numerator->x / divisor->x - subtrahend->x;
    destination->y = numerator->y / divisor->y - subtrahend->y;
    destination->z = numerator->z / divisor->z - subtrahend->z;
    destination->w = numerator->w;
}

void xglVectorScale(Vector4 *destination, const Vector4 *source, float scale)
{
    destination->x = source->x * scale;
    destination->y = source->y * scale;
    destination->z = source->z * scale;
    destination->w = source->w * scale;
}

void xglVectorScaleXYZ(Vector4 *destination, const Vector4 *source, float scale)
{
    destination->x = source->x * scale;
    destination->y = source->y * scale;
    destination->z = source->z * scale;
    destination->w = source->w;
}

void xglVectorScaleAdd(Vector4 *destination, const Vector4 *source,
                       float scale, const Vector4 *addend)
{
    destination->x = source->x * scale + addend->x;
    destination->y = source->y * scale + addend->y;
    destination->z = source->z * scale + addend->z;
    destination->w = source->w * scale + addend->w;
}

void xglVectorScaleAddXYZ(Vector4 *destination, const Vector4 *source,
                          float scale, const Vector4 *addend)
{
    destination->x = source->x * scale + addend->x;
    destination->y = source->y * scale + addend->y;
    destination->z = source->z * scale + addend->z;
    destination->w = source->w;
}

void xglVectorScaleSub(Vector4 *destination, const Vector4 *source,
                       float scale, const Vector4 *subtrahend)
{
    destination->x = source->x * scale - subtrahend->x;
    destination->y = source->y * scale - subtrahend->y;
    destination->z = source->z * scale - subtrahend->z;
    destination->w = source->w * scale - subtrahend->w;
}

void xglVectorScaleSubXYZ(Vector4 *destination, const Vector4 *source,
                          float scale, const Vector4 *subtrahend)
{
    destination->x = source->x * scale - subtrahend->x;
    destination->y = source->y * scale - subtrahend->y;
    destination->z = source->z * scale - subtrahend->z;
    destination->w = source->w;
}

void xglVectorInter(Vector4 *destination, const Vector4 *from,
                    const Vector4 *to, float t)
{
    destination->x = (to->x - from->x) * t + from->x;
    destination->y = (to->y - from->y) * t + from->y;
    destination->z = (to->z - from->z) * t + from->z;
    destination->w = (to->w - from->w) * t + from->w;
}

void xglVectorInterXYZ(Vector4 *destination, const Vector4 *from,
                       const Vector4 *to, float t)
{
    destination->x = (to->x - from->x) * t + from->x;
    destination->y = (to->y - from->y) * t + from->y;
    destination->z = (to->z - from->z) * t + from->z;
    destination->w = to->w;
}

void xglVectorInner(float *destination, const Vector4 *left,
                    const Vector4 *right)
{
    *destination = left->x * right->x;
    *destination = *destination + left->y * right->y;
    *destination = *destination + left->z * right->z;
}

float xglVectorInner4(const Vector4 *left, const Vector4 *right)
{
    float dot = 0;

    dot = left->x * right->x + left->y * right->y + left->z * right->z
        + left->w * right->w;
    return dot;
}

void xglVectorOuter(Vector4 *destination, const Vector4 *first, const Vector4 *second)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vopmula.xyz ACC, vf1, vf2\n\t"
        "vopmsub.xyz vf1, vf2, vf1\n\t"
        "sqc2 vf1, 0(%0)"
        : : "r"(destination), "r"(first), "r"(second) : "memory");
}

void xglVectorOuterNormal(Vector4 *destination, const Vector4 *first, const Vector4 *second)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "vopmula.xyz ACC, vf1, vf2\n\t"
        "vopmsub.xyz vf1, vf2, vf1\n\t"
        "vmul.xyz vf2xyz, vf1xyz, vf1xyz\n\t"
        "vaddz.x vf2x, vf2x, vf2z\n\t"
        "vaddy.x vf2x, vf2x, vf2y\n\t"
        "vrsqrt Q, vf0w, vf2x\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf1xyz, vf1xyz, Q\n\t"
        "sqc2 vf1, 0(%0)"
        : : "r"(destination), "r"(first), "r"(second) : "memory");
}

void xglPlaneOuter(Vector4 *destination, const Vector4 *first, const Vector4 *second, const Vector4 *third)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%1)\n\t"
        "lqc2 vf2, 0(%2)\n\t"
        "lqc2 vf3, 0(%3)\n\t"
        "vsub.xyz vf1xyz, vf1xyz, vf2xyz\n\t"
        "vsub.xyz vf3xyz, vf3xyz, vf2xyz\n\t"
        "vopmula.xyz ACC, vf1xyz, vf3xyz\n\t"
        "vopmsub.xyz vf1xyz, vf3xyz, vf1xyz\n\t"
        "sqc2 vf1, 0(%0)"
        : : "r"(destination), "r"(first), "r"(second), "r"(third) : "memory");
}

void xglPlaneNormal(Vector4 *normal, Vector4 *a, Vector4 *b, Vector4 *c)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%1)\n"
        "lqc2 vf2, 0(%2)\n"
        "lqc2 vf3, 0(%3)\n"
        "vsub.xyz vf1, vf1, vf2\n"
        "vsub.xyz vf3, vf3, vf2\n"
        "vopmula.xyz ACC, vf1, vf3\n"
        "vopmsub.xyz vf1, vf3, vf1\n"
        "vmul.xyz vf2, vf1, vf1\n"
        "vaddz.x vf2x, vf2x, vf2z\n"
        "vaddy.x vf2x, vf2x, vf2y\n"
        "vrsqrt Q, vf0w, vf2x\n"
        "vwaitq\n"
        "vmulq.xyz vf1, vf1, Q\n"
        "sqc2 vf1, 0(%0)\n"
        :
        : "r" (normal), "r" (a), "r" (b), "r" (c)
        : "memory");
}

void xglVectorLength(float *destination, const Vector4 *vector)
{
    __asm__ __volatile__(
        "lqc2 vf3, 0(%1)\n"
        "vmul.xyz vf3, vf3, vf3\n"
        "vaddy.x vf3, vf3, vf3y\n"
        "vaddz.x vf3, vf3, vf3z\n"
        "vsqrt Q, vf3x\n"
        "vwaitq\n"
        "vaddq.x vf4, vf0, Q\n"
        "qmfc2 $2, vf4\n"
        "sw $2, 0(%0)\n"
        :
        : "r"(destination), "r"(vector)
        : "$2", "memory");
}

float xglPointLength(Vector4 *from, Vector4 *to)
{
    register Vector4 *f = from;
    __asm__ __volatile__(
        "lqc2 vf2, 0(%0)\n\t"
        "lqc2 vf3, 0(%1)\n\t"
        "vsub.xyz vf2xyz, vf2xyz, vf3xyz\n\t"
        "vmul.xyz vf3xyz, vf2xyz, vf2xyz\n\t"
        "vaddy.x vf3x, vf3x, vf3y\n\t"
        "vaddz.x vf3x, vf3x, vf3z\n\t"
        "vsqrt Q, vf3x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf4x, vf0x, Q\n\t"
        "qmfc2 $2, vf4\n\t"
        "mtc1 $2, $f0"
        : "+r"(f) : "r"(to) : "$2", "$f0", "memory");
}

float xglPointLengthXZ(Vector4 *from, Vector4 *to)
{
    register Vector4 *f = from;
    __asm__ __volatile__(
        "lqc2 vf2, 0(%0)\n\t"
        "lqc2 vf3, 0(%1)\n\t"
        "vsub.xyz vf2xyz, vf2xyz, vf3xyz\n\t"
        "vmul.xyz vf3xyz, vf2xyz, vf2xyz\n\t"
        "vaddz.x vf3x, vf3x, vf3z\n\t"
        "vsqrt Q, vf3x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf4x, vf0x, Q\n\t"
        "qmfc2 $2, vf4\n\t"
        "mtc1 $2, $f0"
        : "+r"(f) : "r"(to) : "$2", "$f0", "memory");
}

void xglVectorNormal(Vector4 *destination, const Vector4 *source)
{
    __asm__ __volatile__(
        "lqc2 vf2, 0(%1)\n\t"
        "vmul.xyz vf3xyz, vf2xyz, vf2xyz\n\t"
        "vaddy.x vf3x, vf3x, vf3y\n\t"
        "vaddz.x vf3x, vf3x, vf3z\n\t"
        "vrsqrt Q, vf0w, vf3x\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf2xyz, vf2xyz, Q\n\t"
        "sqc2 vf2, 0(%0)"
        :
        : "r"(destination), "r"(source)
        : "memory"
    );
}

/* Clamps each component of source to [minimum, maximum] into destination. */
void xglVectorClamp(Vector4 *destination, const Vector4 *source,
                    float minimum, float maximum)
{
    destination->x = source->x < minimum ? minimum
                    : source->x > maximum ? maximum : source->x;
    destination->y = source->y < minimum ? minimum
                    : source->y > maximum ? maximum : source->y;
    destination->z = source->z < minimum ? minimum
                    : source->z > maximum ? maximum : source->z;
    destination->w = source->w < minimum ? minimum
                    : source->w > maximum ? maximum : source->w;
}

/* Like xglVectorClamp, but copies the w component of source unchanged. */
void xglVectorClampXYZ(Vector4 *destination, const Vector4 *source,
                       float minimum, float maximum)
{
    destination->x = source->x < minimum ? minimum
                    : source->x > maximum ? maximum : source->x;
    destination->y = source->y < minimum ? minimum
                    : source->y > maximum ? maximum : source->y;
    destination->z = source->z < minimum ? minimum
                    : source->z > maximum ? maximum : source->z;
    destination->w = source->w;
}


void xglPlaneParameter(Vector4 *plane, const Vector4 *a, const Vector4 *b,
                       const Vector4 *c)
{
    Vector4 edge1;
    Vector4 edge2;

    xglVectorSubXYZ(&edge1, b, a);
    xglVectorSubXYZ(&edge2, c, b);
    xglVectorOuter(plane, &edge1, &edge2);
    xglVectorNormal(plane, plane);
    xglVectorInner(&plane->w, plane, a);
    plane->w = -plane->w;
}

void xglVectorMulMat(Vector4 *destination, Matrix4 matrix,
                     const Vector4 *vector)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%2)\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf1x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf1y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf1z\n\t"
        "vmaddw.xyzw vf31xyzw, vf30xyzw, vf1w\n\t"
        "sqc2 vf31, 0(%0)"
        :
        : "r"(destination), "r"(matrix), "r"(vector)
        : "memory"
    );
}


float xglRotTransPers(ScreenVector4 *destination, float matrix[4][4], Vector4 *point, int cameraId)
{
    XglCamera *camera;

    xglMatrixStackPush();
    camera = xglStudioGetCamera2(cameraId);
    xglMatrixStackLoad(camera->volume[0].viewMatrix);
    if (matrix != 0) {
        xglMatrixStackMul(matrix);
    }
    xglMatrixStackRTPS(destination, point, &camera->screen.screenScale, &camera->screen.screenOffset);
    xglMatrixStackPop(1);
    return *(&destination->w);
}

/* The studio camera fields this function reads: the screen offset and scale and the view matrix. */

void xglRotTransPersN(ScreenVector4 *destination, Matrix4 matrix, const Vector4 *source,
                   int count, int cameraId)
{
    XglCamera *camera;

    xglMatrixStackPush();
    camera = xglStudioGetCamera2(cameraId);
    xglMatrixStackLoad(camera->volume[0].viewMatrix);
    if (matrix != 0) {
        xglMatrixStackMul(matrix);
    }
    {
        while (count > 0) {
            xglMatrixStackRTPS(destination, source, &camera->screen.screenScale,
                               &camera->screen.screenOffset);
            source++;
            destination++;
            count--;
        }
    }
    xglMatrixStackPop(1);
}

void xglMatrixCopy();

void xglMatrixUnit(Matrix4 destination)
{
    xglMatrixCopy(destination, xglIdentityMatrix);
}

void xglMatrixUnit4s(Matrix4 destination)
{
    xglMatrixCopy(destination, xglUnit4sMatrix);
}

void xglMatrixMul(Matrix4 destination, Matrix4 left, Matrix4 right)
{
    __asm__ __volatile__(
        "lqc2 vf2, 0(%2)\n\t"
        "lqc2 vf3, 16(%2)\n\t"
        "lqc2 vf4, 32(%2)\n\t"
        "lqc2 vf5, 48(%2)\n\t"
        "lqc2 vf27, 0(%1)\n\t"
        "lqc2 vf28, 16(%1)\n\t"
        "lqc2 vf29, 32(%1)\n\t"
        "lqc2 vf30, 48(%1)\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf2x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf2y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf2z\n\t"
        "vmaddw.xyzw vf2xyzw, vf30xyzw, vf2w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf3x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf3y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf3z\n\t"
        "vmaddw.xyzw vf3xyzw, vf30xyzw, vf3w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf4x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf4y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf4z\n\t"
        "vmaddw.xyzw vf4xyzw, vf30xyzw, vf4w\n\t"
        "vmulax.xyzw ACCxyzw, vf27xyzw, vf5x\n\t"
        "vmadday.xyzw ACCxyzw, vf28xyzw, vf5y\n\t"
        "vmaddaz.xyzw ACCxyzw, vf29xyzw, vf5z\n\t"
        "vmaddw.xyzw vf30xyzw, vf30xyzw, vf5w\n\t"
        "sqc2 vf2, 0(%0)\n\t"
        "sqc2 vf3, 16(%0)\n\t"
        "sqc2 vf4, 32(%0)\n\t"
        "sqc2 vf30, 48(%0)"
        :
        : "r"(destination), "r"(left), "r"(right)
        : "memory"
    );
}

/* destination = the transpose of source. */
void xglMatrixReverse(Matrix4 destination, const Matrix *source)
{
    int i;
    int j;
    Matrix4 work;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            work[i][j] = source->rows[j][i];
        }
    }
    xglMatrixCopy(destination, work);
}


void xglMatrixInverse(Matrix *destination, Matrix *source)
{
    float cofactor[16];
    float inverseDeterminant;

    cofactor[0] = source->elements[5] * source->elements[10] - source->elements[6] * source->elements[9];
    cofactor[1] = source->elements[9] * source->elements[2] - source->elements[10] * source->elements[1];
    cofactor[2] = source->elements[1] * source->elements[6] - source->elements[2] * source->elements[5];
    cofactor[3] = 0.0f;
    cofactor[4] = source->elements[6] * source->elements[8] - source->elements[4] * source->elements[10];
    cofactor[5] = source->elements[10] * source->elements[0] - source->elements[8] * source->elements[2];
    cofactor[6] = source->elements[2] * source->elements[4] - source->elements[0] * source->elements[6];
    cofactor[7] = 0.0f;
    cofactor[8] = source->elements[4] * source->elements[9] - source->elements[5] * source->elements[8];
    cofactor[9] = source->elements[8] * source->elements[1] - source->elements[9] * source->elements[0];
    cofactor[10] = source->elements[0] * source->elements[5] - source->elements[1] * source->elements[4];
    cofactor[11] = 0.0f;
    cofactor[12] = cofactor[0] * source->elements[12] + cofactor[4] * source->elements[13]
        + cofactor[8] * source->elements[14];
    cofactor[13] = cofactor[1] * source->elements[12] + cofactor[5] * source->elements[13]
        + cofactor[9] * source->elements[14];
    cofactor[14] = cofactor[2] * source->elements[12] + cofactor[6] * source->elements[13]
        + cofactor[10] * source->elements[14];
    cofactor[15] = 1.0f;
    inverseDeterminant = source->elements[0] * cofactor[0];
    inverseDeterminant += source->elements[1] * cofactor[4];
    inverseDeterminant += source->elements[2] * cofactor[8];
    inverseDeterminant = 1.0f / inverseDeterminant;

    destination->elements[0] = cofactor[0] * inverseDeterminant;
    destination->elements[1] = cofactor[1] * inverseDeterminant;
    destination->elements[2] = cofactor[2] * inverseDeterminant;
    destination->elements[3] = 0.0f;
    destination->elements[4] = cofactor[4] * inverseDeterminant;
    destination->elements[5] = cofactor[5] * inverseDeterminant;
    destination->elements[6] = cofactor[6] * inverseDeterminant;
    destination->elements[7] = 0.0f;
    destination->elements[8] = cofactor[8] * inverseDeterminant;
    destination->elements[9] = cofactor[9] * inverseDeterminant;
    destination->elements[10] = cofactor[10] * inverseDeterminant;
    destination->elements[11] = 0.0f;

    inverseDeterminant = -inverseDeterminant;
    destination->elements[12] = cofactor[12] * inverseDeterminant;
    destination->elements[13] = cofactor[13] * inverseDeterminant;
    destination->elements[14] = cofactor[14] * inverseDeterminant;
    destination->elements[15] = 1.0f;
}

void xglMatrixScale(Matrix *destination, Matrix *source, const Vector4 *scale)
{
    int column;

    for (column = 0; column < 4; column++) {
        destination->rows[0][column] = source->rows[0][column] * scale->x;
        destination->rows[1][column] = source->rows[1][column] * scale->y;
        destination->rows[2][column] = source->rows[2][column] * scale->z;
        destination->rows[3][column] = source->rows[3][column] * scale->w;
    }
}

void xglMatrixTrans(Matrix *destination, Matrix *source, const Vector4 *translation)
{
    int column;

    for (column = 0; column < 4; column++) {
        destination->rows[0][column] = source->rows[0][column];
        destination->rows[1][column] = source->rows[1][column];
        destination->rows[2][column] = source->rows[2][column];
        destination->rows[3][column] = source->rows[0][column] * translation->x + source->rows[1][column] * translation->y + source->rows[2][column] * translation->z + source->rows[3][column] * translation->w;
    }
}

void xglMatrixRotV(Matrix *destination, Matrix *source, Vector4 *axis,
                   float angle)
{
    Vector4 oneMinusCosAxis;
    Vector4 sinAxis;
    Matrix rotation;

    /* Scaled axis terms: (1 - cos) * axis and sin * axis. */
    xglVectorScaleXYZ(&oneMinusCosAxis, axis, 1.0f - xglCos(angle));
    xglVectorScaleXYZ(&sinAxis, axis, xglSin(angle));

    rotation.elements[0] = oneMinusCosAxis.x * axis->x + xglCos(angle);
    rotation.elements[1] = oneMinusCosAxis.y * axis->x + sinAxis.z;
    rotation.elements[2] = oneMinusCosAxis.z * axis->x - sinAxis.y;
    rotation.elements[3] = 0.0f;
    rotation.elements[4] = oneMinusCosAxis.x * axis->y - sinAxis.z;
    rotation.elements[5] = oneMinusCosAxis.y * axis->y + xglCos(angle);
    rotation.elements[6] = oneMinusCosAxis.z * axis->y + sinAxis.x;
    rotation.elements[7] = 0.0f;
    rotation.elements[8] = oneMinusCosAxis.x * axis->z + sinAxis.y;
    rotation.elements[9] = oneMinusCosAxis.y * axis->z - sinAxis.x;
    rotation.elements[10] = oneMinusCosAxis.z * axis->z + xglCos(angle);
    rotation.elements[11] = 0.0f;
    rotation.elements[12] = 0.0f;
    rotation.elements[13] = 0.0f;
    rotation.elements[14] = 0.0f;
    rotation.elements[15] = 1.0f;

    xglMatrixMul(destination->rows, source->rows, rotation.rows);
}

void xglMatrixRotX(Matrix4 destination, Matrix4 source, float angle)
{
    AlignedVector4 sincos;
    AlignedVector4 *pSincos;

    pSincos = &sincos;
    sincos.y = xglSin(angle);
    sincos.x = xglCos(angle);
    __asm__ __volatile__(
        "lq $2, 0(%1)\n"
        "lqc2 vf28, 16(%1)\n"
        "lqc2 vf29, 32(%1)\n"
        "lqc2 vf1, 0(%2)\n"
        "sq $2, 0(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        "vmulax.xyzw ACC, vf28, vf1x\n"
        "vmaddy.xyzw vf27, vf29, vf1y\n"
        "vmulax.xyzw ACC, vf29, vf1x\n"
        "vmsuby.xyzw vf30, vf28, vf1y\n"
        "sqc2 vf27, 16(%0)\n"
        "sqc2 vf30, 32(%0)\n"
        :
        : "r" (destination), "r" (source), "r" (pSincos)
        : "memory", "$2");
}

void xglMatrixRotY(Matrix4 destination, Matrix4 source, float angle)
{
    AlignedVector4 sincos;
    AlignedVector4 *pSincos;

    pSincos = &sincos;
    sincos.y = xglSin(angle);
    sincos.x = xglCos(angle);
    __asm__ __volatile__(
        "lq $2, 16(%1)\n"
        "lqc2 vf27, 0(%1)\n"
        "lqc2 vf29, 32(%1)\n"
        "lqc2 vf1, 0(%2)\n"
        "sq $2, 16(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        "vmulax.xyzw ACC, vf27, vf1x\n"
        "vmsuby.xyzw vf28, vf29, vf1y\n"
        "vmulax.xyzw ACC, vf29, vf1x\n"
        "vmaddy.xyzw vf30, vf27, vf1y\n"
        "sqc2 vf28, 0(%0)\n"
        "sqc2 vf30, 32(%0)\n"
        :
        : "r" (destination), "r" (source), "r" (pSincos)
        : "memory", "$2");
}

void xglMatrixRotZ(Matrix4 destination, Matrix4 source, float angle)
{
    AlignedVector4 sincos;
    AlignedVector4 *pSincos;

    pSincos = &sincos;
    sincos.y = xglSin(angle);
    sincos.x = xglCos(angle);
    __asm__ __volatile__(
        "lq $2, 32(%1)\n"
        "lqc2 vf27, 0(%1)\n"
        "lqc2 vf28, 16(%1)\n"
        "lqc2 vf1, 0(%2)\n"
        "sq $2, 32(%0)\n"
        "lq $2, 48(%1)\n"
        "sq $2, 48(%0)\n"
        "vmulax.xyzw ACC, vf27, vf1x\n"
        "vmaddy.xyzw vf29, vf28, vf1y\n"
        "vmulax.xyzw ACC, vf28, vf1x\n"
        "vmsuby.xyzw vf30, vf27, vf1y\n"
        "sqc2 vf29, 0(%0)\n"
        "sqc2 vf30, 16(%0)\n"
        :
        : "r" (destination), "r" (source), "r" (pSincos)
        : "memory", "$2");
}

void xglMatrixFrustum(Matrix *destination, Matrix *source,
                      float left, float right, float bottom, float top,
                      float nearPlane, float farPlane)
{
    Matrix projection;

    projection.elements[0] = (2.0f * nearPlane) / (right - left);
    projection.elements[1] = 0.0f;
    projection.elements[2] = 0.0f;
    projection.elements[3] = 0.0f;
    projection.elements[4] = 0.0f;
    projection.elements[5] = (2.0f * nearPlane) / (top - bottom);
    projection.elements[6] = 0.0f;
    projection.elements[7] = 0.0f;
    projection.elements[8] = (right + left) / (right - left);
    projection.elements[9] = (top + bottom) / (top - bottom);
    projection.elements[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    projection.elements[11] = -1.0f;
    projection.elements[12] = 0.0f;
    projection.elements[13] = 0.0f;
    projection.elements[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    projection.elements[15] = 0.0f;
    xglMatrixMul(destination->rows, source->rows, projection.rows);
}

extern char D_108[];

void xglMatrixStackUnit(void)
{
    Vu0CallMicro((u32)&D_108 >> 3);
}

extern char D_130[];

void xglMatrixStackMul(Matrix4 matrix)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "lqc2 vf4, 0(%1)\n\t"
        "lqc2 vf5, 16(%1)\n\t"
        "lqc2 vf6, 32(%1)\n\t"
        "lqc2 vf7, 48(%1)\n\t"
        "vcallmsr $vi27"
        : : "r"((u32)&D_130 >> 3), "r"(matrix) : "memory");
}

extern char D_1B8[];

void xglMatrixStackReverse(void)
{
    Vu0CallMicro((u32)&D_1B8 >> 3);
}

extern char D_218[];

void xglMatrixStackInverse(void)
{
    Vu0CallMicro((u32)&D_218 >> 3);
}

extern char D_2E8[];

void xglMatrixStackScale(const Vector4 *vector)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "lqc2 vf4, 0(%1)\n\t"
        "vcallmsr $vi27"
        : : "r"((u32)&D_2E8 >> 3), "r"(vector) : "memory");
}

extern char D_310[];

void xglMatrixStackTrans(const Vector4 *vector)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "lqc2 vf4, 0(%1)\n\t"
        "vcallmsr $vi27"
        : : "r"((u32)&D_310 >> 3), "r"(vector) : "memory");
}

extern char D_338[];

void xglMatrixStackRotV(const Vector4 *vector, float angle)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "lqc2 vf5, 0(%1)\n\t"
        "qmtc2 %2,$vf4\n\t"
        "vcallmsr $vi27"
        : : "r"((u32)&D_338 >> 3), "r"(vector), "r"(angle) : "memory");
}

extern char D_3D0[];

void xglMatrixStackRotX(float angle)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %1,$vf4\n\t"
        "vcallmsr $vi27"
        : : "r"((u32)&D_3D0 >> 3), "r"(angle) : "memory");
}

extern char D_418[];

void xglMatrixStackRotY(float angle)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %1,$vf4\n\t"
        "vcallmsr $vi27"
        : : "r"((u32)&D_418 >> 3), "r"(angle) : "memory");
}

extern char D_460[];

void xglMatrixStackRotZ(float angle)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "qmtc2 %1,$vf4\n\t"
        "vcallmsr $vi27"
        : : "r"((u32)&D_460 >> 3), "r"(angle) : "memory");
}

void xglMatrixStackFrustum(float left, float right, float bottom, float top, float nearPlane, float farPlane)
{
    float *window;

    __asm__ __volatile__(
        "lui $2, %%hi(D_4A8)\n"
        "addiu $2, $2, %%lo(D_4A8)\n"
        "srl $3, $2, 3\n"
        "lui $2, 0x1100\n"
        "ori $2, $2, 0x4800\n"
        "sw $2, %0\n"
        "ctc2.i $3, $vi27\n"
        : "=m" (window)
        :
        : "$2", "$3");
    window[0] = left;
    window[1] = bottom;
    window[2] = nearPlane;
    *(u32 *)&window[3] = 0;
    window[4] = right;
    window[5] = top;
    window[6] = farPlane;
    *(u32 *)&window[7] = 0;
    __asm__ __volatile__("sync\n\tvcallmsr $vi27");
}

void xglMatrixStackSave(float matrix[4][4])
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi0\n\t"
        "sqc2 $vf28,0(%0)\n\t"
        "sqc2 $vf29,16(%0)\n\t"
        "sqc2 $vf30,32(%0)\n\t"
        "sqc2 $vf31,48(%0)"
        :
        : "r"(matrix)
        : "memory"
    );
}

void xglMatrixStackLoad(float matrix[4][4])
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi0\n\t"
        "lqc2 $vf28,0(%0)\n\t"
        "lqc2 $vf29,16(%0)\n\t"
        "lqc2 $vf30,32(%0)\n\t"
        "lqc2 $vf31,48(%0)"
        :
        : "r"(matrix)
        : "memory"
    );
}

extern char D_538[];

void xglMatrixStackPushUnit(void)
{
    Vu0CallMicro((u32)&D_538 >> 3);
}

extern char D_580[];

void xglMatrixStackPush(void)
{
    Vu0CallMicro((u32)&D_580 >> 3);
}

extern char D_5C8[];

void xglMatrixStackPop(u32 value)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "ctc2 %1,$vi1\n\t"
        "vcallmsr $vi27"
        : : "r"((u32)&D_5C8 >> 3), "r"(value) : "memory");
}

extern char D_620[];

void xglMatrixStackMulVector(Vector4 *destination, const Vector4 *source)
{
    __asm__ __volatile__(
        "ctc2.i %0,$vi27\n\t"
        "vnop\n\t"
        "lqc2 vf4, 0(%1)\n\t"
        "vcallmsr $vi27\n\t"
        "cfc2.i $0,$vi0\n\t"
        "sqc2 vf5, 0(%2)"
        : : "r"((u32)&D_620 >> 3), "r"(source), "r"(destination) : "memory");
}

extern char D_648[];

void xglMatrixStackRTPS(ScreenVector4 *destination, const Vector4 *point, Vector4 *screenScale, Vector4 *screenOffset)
{
    __asm__ __volatile__(
        "ctc2.i %0, $vi27\n"
        "lqc2 vf4, 0(%2)\n"
        "lqc2 vf5, 0(%3)\n"
        "lqc2 vf6, 0(%4)\n"
        "vcallmsr $vi27\n"
        "cfc2.i $0, $vi0\n"
        "sqc2 vf7, 0(%1)\n"
        :
        : "r" ((u32)D_648 >> 3), "r" (destination), "r" (point), "r" (screenScale), "r" (screenOffset)
        : "memory");
}

float sqrtf(float x);

void xglMatrix2Quaternion(Matrix *quaternion, Matrix *source)
{
    float trace;

    trace = source->elements[0] + source->elements[5] + source->elements[10];
    if (trace > 0.0f) {
        register Matrix *alias = quaternion;
        register float w;

        alias->elements[3] = sqrtf(trace + 1.0f) * 0.5f;
        w = quaternion->elements[3];
        trace = 0.5f / (w + w);
        quaternion->elements[0] = (source->elements[6] - source->elements[9]) * trace;
        quaternion->elements[1] = (source->elements[8] - source->elements[2]) * trace;
        quaternion->elements[2] = (source->elements[1] - source->elements[4]) * trace;
    } else {
        int nextAxisTable[3] = {1, 2, 0};
        int largestAxis = 0;
        int nextAxis;
        int lastAxis;
        register float *largestComponent;
        register float *base;
        register float w;

        if (source->elements[0] < source->elements[5]) {
            largestAxis = 1;
        }
        if (source->elements[largestAxis * 5] < source->elements[10]) {
            largestAxis = 2;
        }
        nextAxis = nextAxisTable[largestAxis];
        lastAxis = nextAxisTable[nextAxis];
        base = quaternion->elements;
        largestComponent = base + largestAxis;
        *largestComponent = sqrtf(source->elements[largestAxis * 5]
                           - (source->elements[nextAxis * 5] + source->elements[lastAxis * 5]) + 1.0f)
                     * 0.5f;
        w = quaternion->elements[largestAxis];
        trace = 0.5f / (w + w);
        quaternion->elements[3] = (source->elements[nextAxis * 4 + lastAxis] - source->elements[lastAxis * 4 + nextAxis]) * trace;
        quaternion->elements[nextAxis] = (source->elements[largestAxis * 4 + nextAxis] + source->elements[nextAxis * 4 + largestAxis]) * trace;
        quaternion->elements[lastAxis] = (source->elements[largestAxis * 4 + lastAxis] + source->elements[lastAxis * 4 + largestAxis]) * trace;
    }
}

void xglQuaternion2Matrix(Matrix4 destination, const Vector4 *quaternion)
{
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "vaddw.x vf27x, vf0x, vf0w\n\t"
        "vaddw.y vf28y, vf0y, vf0w\n\t"
        "vaddw.z vf29z, vf0z, vf0w\n\t"
        "vadd.xyzw vf14xyzw, vf31xyzw, vf31xyzw\n\t"
        "vsub.w vf27w, vf27w, vf27w\n\t"
        "vsub.w vf28w, vf28w, vf28w\n\t"
        "vsub.w vf29w, vf29w, vf29w\n\t"
        "vmulx.xyzw vf15xyzw, vf31xyzw, vf14x\n\t"
        "vmuly.xyzw vf16xyzw, vf31xyzw, vf14y\n\t"
        "vmulz.xyzw vf17xyzw, vf31xyzw, vf14z\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vaddy.x vf20x, vf15x, vf16y\n\t"
        "vaddz.y vf20y, vf16y, vf17z\n\t"
        "vaddx.z vf20z, vf17z, vf15x\n\t"
        "vnop\n\t"
        "vsubw.z vf27z, vf15z, vf16w\n\t"
        "vaddw.y vf27y, vf15y, vf17w\n\t"
        "vsuby.x vf27x, vf27x, vf20y\n\t"
        "vsubw.x vf28x, vf16x, vf17w\n\t"
        "vaddw.z vf28z, vf16z, vf15w\n\t"
        "vsubz.y vf28y, vf28y, vf20z\n\t"
        "vsubw.y vf29y, vf17y, vf15w\n\t"
        "vaddw.x vf29x, vf17x, vf16w\n\t"
        "vsubx.z vf29z, vf29z, vf20x\n\t"
        "sqc2 vf0, 48(%0)\n\t"
        "sqc2 vf27, 0(%0)\n\t"
        "sqc2 vf28, 16(%0)\n\t"
        "sqc2 vf29, 32(%0)"
        :
        : "r"(destination), "r"(quaternion)
        : "memory"
    );
}

void xglQuaternionInterpolateLinear(Vector4 *destination, const Vector4 *first, const Vector4 *second, float weightFirst, float weightSecond)
{
    __asm__ __volatile__(
        "lwc1 $f3, %3\n"
        "lwc1 $f2, %4\n"
        "mfc1 $6, $f3\n"
        "mfc1 $7, $f2\n"
        "qmtc2 $6, vf10\n"
        "qmtc2 $7, vf11\n"
        "lqc2 vf20, 0(%1)\n"
        "lqc2 vf21, 0(%2)\n"
        "vmul.xyzw vf18, vf20, vf21\n"
        ".set noat\n"
        "addiu $1, $0, 0\n"
        "mtc1 $1, $f0\n"
        ".set at\n"
        "vsubw.x vf1x, vf0x, vf0w\n"
        "vaddy.x vf23x, vf0x, vf18y\n"
        "vaddz.x vf24x, vf0x, vf18z\n"
        "vaddw.x vf25x, vf0x, vf18w\n"
        "vmulaw.x ACCx, vf18x, vf0w\n"
        "vmaddaw.x ACCx, vf23x, vf0w\n"
        "vmaddaw.x ACCx, vf24x, vf0w\n"
        "vmaddw.x vf26x, vf25x, vf0w\n"
        "qmfc2 $2, vf26\n"
        "mtc1 $2, $f1\n"
        "c.lt.s $f0, $f1\n"
        "bc1t 1f\n"
        "vmulx.xyzw vf21, vf21, vf1x\n"
        "1:\n"
        "vmulx.xyzw vf20, vf20, vf10x\n"
        "vmulx.xyzw vf21, vf21, vf11x\n"
        "vadd.xyzw vf22, vf20, vf21\n"
        "vmul.xyzw vf19, vf22, vf22\n"
        "vaddy.x vf23x, vf0x, vf19y\n"
        "vaddz.x vf24x, vf0x, vf19z\n"
        "vaddw.x vf25x, vf0x, vf19w\n"
        "vmulaw.x ACCx, vf19x, vf0w\n"
        "vmaddaw.x ACCx, vf23x, vf0w\n"
        "vmaddaw.x ACCx, vf24x, vf0w\n"
        "vmaddw.x vf26x, vf25x, vf0w\n"
        "vrsqrt Q, vf0w, vf26x\n"
        "vwaitq\n"
        "vmulq.xyzw vf22, vf22, Q\n"
        "sqc2 vf22, 0(%0)\n"
        :
        : "r" (destination), "r" (first), "r" (second), "m" (weightFirst), "m" (weightSecond)
        : "memory", "$2", "$6", "$7", "$f2", "$f3");
}

void xglRandSeedInit(void)
{
    iRandSeed = 0x12345678ULL;
}

extern char xglVu0ResidentMicroprogram[];

void xglGeometryInit(void)
{
    xglFSrand(0.1f);
    iRandSeed = 0x12345678ULL;
    xglDmaDirectSrcChain(0, (u32)&xglVu0ResidentMicroprogram);
}

void xglMatrixCopy()
{
    __asm__ __volatile__(
        "daddu $3,$5,$0\n\t"
        "lq $2,0($3)\n\t"
        "sq $2,0($4)\n\t"
        "lq $2,16($3)\n\t"
        "sq $2,16($4)\n\t"
        "lq $2,32($3)\n\t"
        "sq $2,32($4)\n\t"
        "lq $2,48($3)\n\t"
        "sq $2,48($4)"
        :
        :
        : "$2", "$3", "$4", "$5", "memory");
}

void xglVectorSubXYZ(register Vector4 *destination, register const Vector4 *left,
                     register const Vector4 *right)
{
    Vu0SubVectorXYZ(destination, left, right);
}

Matrix4 xglIdentityMatrix = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };

Matrix4 xglUnit4sMatrix = { { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
