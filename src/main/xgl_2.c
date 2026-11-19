#include "common.h"
#include "main/nml_model_set.h"
#include "main/sdv_033.h"
#include "shared.h"
#include "main/xgl_2.h"

/* This unit's .sdata (0x69E070-0x69E078) and .sbss (0x69E398), defined before
 * their first use so that every access is $gp-relative. */
int xglLightSetDefaultReady = 0;
int xglMotionCacheValid = 0;
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

void xglFRandSeed(register float seed)
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

void xglMatrixStackUnit(void);
void xglMatrixStackRotX(float angle);
void xglMatrixStackRotY(float angle);
void xglMatrixStackRotZ(float angle);
void xglMatrixStackSave(Matrix4 matrix);
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

#include "quadword.h"

void xglMemCopy64(void *destination, const void *source, int count)
{
    register void *dst = destination;
    register const void *src = source;
    register int remaining = count;

    QuadwordCopyMatrixArray(dst, src, remaining);
}

void xglMemCopy64Backward(void *destination, const void *source, int count)
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

extern void xglVectorLength(float *destination, const Vector4 *vector);
extern void xglMatrixStackSave(float matrix[4][4]);
void xglVectorScale(Vector4 *destination, const Vector4 *source, float scale);

typedef unsigned int XglQuadword __attribute__((mode(TI)));

void xglVectorMulMat(Vector4 *destination, Matrix4 matrix,
                     const Vector4 *vector);
void xglVectorScaleXYZ(Vector4 *destination, const Vector4 *source, float scale);
void xglVectorNormal_001A9200(Vector4 *destination, const Vector4 *source);
void xglMatrixStackLoad(float matrix[4][4]);
void xglVectorMulAdd(Vector4 *destination, const Vector4 *left,
                     const Vector4 *right, const Vector4 *addend);
void xglRandSeedInit(void);
void xglVectorDiv(Vector4 *destination, const Vector4 *numerator,
                  const Vector4 *divisor);
void xglVectorDivXYZ(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor);
void xglVectorMulAddXYZ(Vector4 *destination, const Vector4 *left,
                        const Vector4 *right, const Vector4 *addend);
void xglVectorMulSub(Vector4 *destination, const Vector4 *left,
                     const Vector4 *right, const Vector4 *subtrahend);
void xglVectorMulSubXYZ(Vector4 *destination, const Vector4 *left,
                        const Vector4 *right, const Vector4 *subtrahend);
void xglVectorDivAdd(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor, const Vector4 *addend);
void xglVectorDivAddXYZ(Vector4 *destination, const Vector4 *numerator,
                        const Vector4 *divisor, const Vector4 *addend);
void xglVectorDivSub(Vector4 *destination, const Vector4 *numerator,
                     const Vector4 *divisor, const Vector4 *subtrahend);
void xglVectorDivSubXYZ(Vector4 *destination, const Vector4 *numerator,
                        const Vector4 *divisor, const Vector4 *subtrahend);
extern void xglMatrixMul(Matrix *destination, Matrix *left, Matrix *right);

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

void xglVectorNormal_001A9200(Vector4 *destination, const Vector4 *source)
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

void xglVectorSubXYZ(Vector4 *destination, const Vector4 *left,
                     const Vector4 *right);
void xglVectorOuter(Vector4 *destination, const Vector4 *first, const Vector4 *second);

void xglPlaneParameter(Vector4 *plane, const Vector4 *a, const Vector4 *b,
                       const Vector4 *c)
{
    Vector4 edge1;
    Vector4 edge2;

    xglVectorSubXYZ(&edge1, b, a);
    xglVectorSubXYZ(&edge2, c, b);
    xglVectorOuter(plane, &edge1, &edge2);
    xglVectorNormal_001A9200(plane, plane);
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

void xglMatrixStackPush(void);
u8 *xglStudioGetCamera2(void *studio);
void xglMatrixStackMul(Matrix4 matrix);
void xglMatrixStackPop(u32 value);
void xglMatrixStackRTPS(Vector4 *destination, Vector4 *point, Vector4 *screenScale, Vector4 *screenOffset);

float xglRotTransPers(Vector4 *destination, float matrix[4][4], Vector4 *point, void *cameraId)
{
    u8 *camera;

    xglMatrixStackPush();
    camera = xglStudioGetCamera2(cameraId);
    xglMatrixStackLoad((float (*)[4])(camera + 1136));
    if (matrix != 0) {
        xglMatrixStackMul(matrix);
    }
    xglMatrixStackRTPS(destination, point, (Vector4 *)(camera + 128), (Vector4 *)(camera + 112));
    xglMatrixStackPop(1);
    return *(&destination->w);
}

/* The studio camera fields this function reads: the screen offset and scale and the view matrix. */

void xglRotTransPersN(Vector4 *destination, Matrix4 matrix, const Vector4 *source,
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
void xglLightSetDefault(XglLightSet *lightSet);
extern Matrix4 xglIdentityMatrix;
extern Matrix4 D_00645330;

void xglMatrixUnit(Matrix4 destination)
{
    xglMatrixCopy(destination, xglIdentityMatrix);
}

void xglMatrixUnit4s(Matrix4 destination)
{
    xglMatrixCopy(destination, D_00645330);
}

void xglMatrixMul(Matrix *destination, Matrix *left, Matrix *right)
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

float xglSin(float angle);
float xglCos(float angle);

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

typedef struct Matrix4Block {
    Matrix4 m;
} Matrix4Block;

void xglMatrixScale(Matrix4Block *destination, const Matrix4Block *source, const Vector4 *scale)
{
    int column;

    for (column = 0; column < 4; column++) {
        destination->m[0][column] = source->m[0][column] * scale->x;
        destination->m[1][column] = source->m[1][column] * scale->y;
        destination->m[2][column] = source->m[2][column] * scale->z;
        destination->m[3][column] = source->m[3][column] * scale->w;
    }
}

void xglMatrixTrans(Matrix4Block *destination, const Matrix4Block *source, const Vector4 *translation)
{
    int column;

    for (column = 0; column < 4; column++) {
        destination->m[0][column] = source->m[0][column];
        destination->m[1][column] = source->m[1][column];
        destination->m[2][column] = source->m[2][column];
        destination->m[3][column] = source->m[0][column] * translation->x + source->m[1][column] * translation->y + source->m[2][column] * translation->z + source->m[3][column] * translation->w;
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

    xglMatrixMul(destination, source, &rotation);
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
    xglMatrixMul(destination, source, &projection);
}

#include "vu0_macro.h"

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

void xglMatrixStackRTPS(Vector4 *destination, Vector4 *point, Vector4 *screenScale, Vector4 *screenOffset)
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

void xglMatrix2Quaternion_001AAD20(Matrix *quaternion, Matrix *source)
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

void xglFRandSeed(register float seed);
extern void xglDmaDirectSrcChain(u32 channel, u32 address);
extern char D_00672A30[];

void xglGeometryInit(void)
{
    xglFRandSeed(0.1f);
    iRandSeed = 0x12345678ULL;
    xglDmaDirectSrcChain(0, (u32)&D_00672A30);
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

void xglStudioLightReset(void *light)
{
    xglLightSetDefault(light);
}

void *memcpy(void *destination, const void *source, unsigned int size);
void xglZeroQuadwords18(AlignedVector4 *dst);

/* A loaded light pack file: it starts with the "lxy" tag and is registered in xglLightPackTable. */
typedef struct XglLightPack {
    char tag[4];            /* 0x00: "lxy" */
    u8 unk04[0xC];
    char name[0x20];        /* 0x10 */
    int entryCount;         /* 0x30 */
    u8 unk34[8];
    int keyOffset;          /* 0x3C */
    u8 unk40[0xB0];
    int entryOffsets[1];    /* 0xF0 */
} XglLightPack;

/* One slot of the 16-entry light pack table at 0x0095A410 in this TU's .bss (no symbol). */
typedef struct XglLightPackSlot {
    XglLightPack *pack;      /* 0x00 */
    int refs;               /* 0x04 */
    u32 unk08;
    u32 unk0C;
} XglLightPackSlot;

static XglLightPackSlot xglLightPackTable[16];

static XglLightSet xglLightSetDefaultCache;

void xglLightSetDefault(XglLightSet *lightSet)
{
    int i;

    if (xglLightSetDefaultReady == 0) {
        xglZeroQuadwords18(lightSet);
        lightSet->colors[0][0] = lightSet->colors[0][1] = lightSet->colors[0][2] = 0.8f;
        lightSet->colors[1][0] = lightSet->colors[1][1] = lightSet->colors[1][2] = 0.5f;
        lightSet->colors[2][0] = lightSet->colors[2][1] = lightSet->colors[2][2] = 0.5f;
        lightSet->ambientColor.x = lightSet->ambientColor.y = lightSet->ambientColor.z = 0.25f;
        lightSet->ambientColorLower.x = lightSet->ambientColorLower.y = lightSet->ambientColorLower.z = 0.25f;
        lightSet->directions[3][0] = lightSet->directions[4][0] = lightSet->directions[5][0] = 1.5707964f;
        lightSet->viewDirection.x = 1.5707964f;
        lightSet->directions[0][0] = -0.26195645f;
        lightSet->directions[0][1] = 0.53120846f;
        lightSet->directions[0][3] = 1.0f;
        lightSet->directions[1][0] = 2.3561945f;
        lightSet->directions[1][1] = 1.2733574f;
        lightSet->directions[1][3] = 1.0f;
        lightSet->directions[2][0] = 0.34906587f;
        lightSet->directions[2][1] = 3.7866313f;
        lightSet->directions[2][3] = 1.0f;
        for (i = 0; i < 7; i++) {
            xglDirectionFromAngles((Vector4 *)lightSet + i, (Vector4 *)lightSet + i);
        }
        lightSet->pointPositions[0][0] = lightSet->pointPositions[0][1] = lightSet->pointPositions[0][2]
            = lightSet->pointPositions[0][3] = 0.0f;
        lightSet->pointPositions[1][0] = lightSet->pointPositions[1][1] = lightSet->pointPositions[1][2]
            = lightSet->pointPositions[1][3] = 0.0f;
        lightSet->pointPositions[2][0] = lightSet->pointPositions[2][1] = lightSet->pointPositions[2][2]
            = lightSet->pointPositions[2][3] = 0.0f;
        lightSet->pointColors[0][0] = lightSet->pointColors[0][1] = lightSet->pointColors[0][2]
            = lightSet->pointColors[0][3] = 0.0f;
        lightSet->pointColors[1][0] = lightSet->pointColors[1][1] = lightSet->pointColors[1][2]
            = lightSet->pointColors[1][3] = 0.0f;
        lightSet->pointColors[2][0] = lightSet->pointColors[2][1] = lightSet->pointColors[2][2]
            = lightSet->pointColors[2][3] = 0.0f;
        {
            register Vector4 *src = (Vector4 *)lightSet;
            register void *saved = &xglLightSetDefaultCache;
            register Vector4 *copy = src;
            memcpy(saved, copy, sizeof(XglLightSet));
        }
        xglLightSetDefaultReady = 1;
    } else {
        {
            register XglLightSet *dst = lightSet;
            register XglLightSet *cache = &xglLightSetDefaultCache;
            memcpy(dst, cache, sizeof(XglLightSet));
        }
    }
    xglMotionCacheValid = 0;
}

void xglLightPackTableInit(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        xglLightPackTable[i].pack = 0;
        xglLightPackTable[i].refs = 0;
    }
}

int xglLightPackRegister(XglLightPack *pack)
{
    int i;
    Vector4 unused;   /* frame-sizing local, playbook frame */

    if (pack != 0 && pack->tag[0] == 'l' && pack->tag[1] == 'x' && pack->tag[2] == 'y') {
        for (i = 0; i < 16; i++) {
            if (xglLightPackTable[i].pack == pack) {
                xglLightPackTable[i].refs = xglLightPackTable[i].refs + 1;
                return 2;
            }
        }
        {
            for (i = 0; i < 16; i++) {
                if (xglLightPackTable[i].pack == 0) {
                    xglLightPackTable[i].pack = pack;
                    xglLightPackTable[i].refs = 1;
                    return 1;
                }
            }
        }
    }
    return 0;
}

int xglLightPackRelease(XglLightPack *pack)
{
    int i;
    Vector4 unused;

    if (pack != 0 && pack->tag[0] == 'l' && pack->tag[1] == 'x' && pack->tag[2] == 'y') {
        for (i = 0; i < 16; i++) {
            if (xglLightPackTable[i].pack == pack) {
                xglLightPackTable[i].refs = xglLightPackTable[i].refs - 1;
                if (xglLightPackTable[i].refs <= 0) {
                    xglLightPackTable[i].pack = 0;
                    xglLightPackTable[i].refs = 0;
                    return 1;
                }
                return 0;
            }
        }
    }
    return 0;
}

typedef struct XglLightCurve {
    int count;
    int minimum;
    int maximum;
    int unknown12;
    int fixed;
} XglLightCurve;

float pxyInterpolateKeys(int, int *, float, float);

float xglLightPackSampleCurve(int **cursor, float time)
{
    float result = 0.0f;
    XglLightCurve *record;

    record = (XglLightCurve *)*cursor;
    record->fixed = ((int)time << 8) + (int)((time - (float)(int)time) * 256.0f);
    if (record->fixed > (record->maximum << 8)) {
        record->fixed = record->maximum << 8;
    }
    if (record->fixed < (record->minimum << 8)) {
        record->fixed = record->minimum << 8;
    }
    result = pxyInterpolateKeys(record->count, (int *)record + 8,
                           (float)(record->fixed / 256) +
                               (float)(record->fixed % 256) / 256.0f,
                           0.2f);
    *cursor = (int *)record + record->count * 4 + 8;
    return result;
}

void xglVectorZero(Vector4 *destination);
void xglVectorCopy(AlignedVector4 *dst, AlignedVector4 *src);
void xglVectorSubXYZ_001AF3F0(AlignedVector4 *dst, AlignedVector4 *minuend, AlignedVector4 *subtrahend);
void xglVectorSub(AlignedVector4 *dst, AlignedVector4 *minuend, AlignedVector4 *subtrahend);
void xglVectorInner(float *destination, const Vector4 *left, const Vector4 *right);
void xglMatrixStackMulVector(Vector4 *destination, const Vector4 *source);
float acosf(float);

/* One light of a light pack (0x50 bytes per element). */
typedef struct XglLightKey {
    Vector4 position;       /* 0x00 */
    Vector4 rotation;       /* 0x10: x, y, z angles */
    Vector4 extent;         /* 0x20: radii / slopes / box half sizes by type */
    Vector4 color;          /* 0x30 */
    float coneAngle;        /* 0x40 */
    float penumbra;         /* 0x44 */
    float intensity;        /* 0x48 */
    float reserved4C;
} XglLightKey;

typedef struct XglLightEntry {
    int unk00;
    int type;               /* 0x04 */
    u8 unk08[8];
    u8 channelTypes[20];    /* 0x10 */
    u8 unk24[12];
    int stream[1];          /* 0x30 */
} XglLightEntry;

void xglLightPackEvaluate(XglLightPack *pack, float time)
{
    int i;
    int *entryOffsets;
    XglLightKey *keys;
    XglLightEntry *entry;
    int *cursor;
    float *values;
    int channel;
    int valueIndex;

    entryOffsets = pack->entryOffsets;
    keys = (XglLightKey *)((u8 *)pack + pack->keyOffset);
    for (i = 0; i < pack->entryCount; i++) {
        entry = (XglLightEntry *)((u8 *)pack + entryOffsets[i]);
        cursor = entry->stream;
        values = (float *)&keys[i];
        valueIndex = 0;
        for (channel = 0; channel < 20; channel++) {
            switch (entry->channelTypes[channel]) {
            case 2:
                values[valueIndex] = *(float *)cursor++;
                break;
            case 3:
                values[valueIndex] = 0.0f;
                break;
            case 4:
                values[valueIndex] = 1.0f;
                break;
            case 1:
                {
                    while (((int)cursor & 0xf) != 0) {
                        cursor++;
                    }
                }
                values[valueIndex] = xglLightPackSampleCurve(&cursor, time);
                break;
            }
            valueIndex++;
        }
    }
}

typedef struct XglLightSlotDir {
    float x;
    float y;
    float z;
    int weakest;
} XglLightSlotDir;

typedef struct XglLightSlots {
    Vector4 colors[6];
    XglLightSlotDir directions[6];
} XglLightSlots;

void xglLightSlotsInsert(XglLightSlots *lightSlots, Vector4 *color, Vector4 *lightDir)
{
    float strength;
    int i;
    float weakestStrength;
    Vector4 *slot;

    strength = color->x;
    if (strength < color->y) {
        strength = color->y;
    }
    if (strength < color->z) {
        strength = color->z;
    }
    if ((&lightSlots->colors[0])[(&lightSlots->directions[0])->weakest].w < strength) {
        weakestStrength = 0;
        slot = &(&lightSlots->colors[0])[(&lightSlots->directions[0])->weakest];
        xglVectorCopy(slot, color);
        slot->w = strength;
        slot[6].x = lightDir->x;
        slot[6].y = lightDir->y;
        slot[6].z = lightDir->z;
        if (slot[6].x == 0.0f && slot[6].y == 0.0f && slot[6].z == 0.0f) {
            slot[6].z = 1.0f;
        }
        xglVectorNormal_001A9200(&slot[6], &slot[6]);
        weakestStrength = (&lightSlots->colors[0])[0].w;
        (&lightSlots->directions[0])->weakest = 0;
        for (i = 1; i < 6; i++) {
            if ((&lightSlots->colors[0])[i].w < weakestStrength) {
                weakestStrength = (&lightSlots->colors[0])[i].w;
                (&lightSlots->directions[0])->weakest = i;
            }
        }
    }
}

int xglLightPointColor(Vector4 *color, XglLightKey *key, Vector4 *receiver)
{
    Vector4 toLight;
    float distance;

    xglVectorZero(color);
    xglVectorSubXYZ_001AF3F0(&toLight, &key->position, receiver);
    distance = toLight.x * toLight.x + toLight.y * toLight.y + toLight.z * toLight.z;
    if (key->extent.y * key->extent.y <= distance) {
        return 0;
    }
    distance = sqrtf(distance);
    xglVectorCopy(color, &key->color);
    if (distance > key->extent.x) {
        distance = (key->extent.y - distance) / (key->extent.y - key->extent.x);
        xglVectorScale(color, color, distance);
    }
    return 1;
}

int xglLightSpotColor(Vector4 *color, Vector4 *lightDirection, XglLightKey *key, Vector4 *receiver)
{
    DoublewordVector4 coneAxis = { { 0.0f, 0.0f, -1.0f, 1.0f } };
    float angle;
    float halfCone;

    halfCone = key->coneAngle * 0.5f;
    xglVectorSub(lightDirection, receiver, &key->position);
    xglVectorNormal_001A9200(lightDirection, lightDirection);
    xglMatrixStackUnit();
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackMulVector(&coneAxis.vector, &coneAxis.vector);
    xglVectorInner(&angle, &coneAxis.vector, lightDirection);
    angle = acosf(angle);
    if (angle > halfCone) {
        return 0;
    }
    xglVectorCopy(color, &key->color);
    if (halfCone - __builtin_fabsf(key->penumbra) < angle) {
        halfCone = (halfCone - angle) / __builtin_fabsf(key->penumbra);
        xglVectorScale(color, color, halfCone);
    }
    xglVectorScale(lightDirection, lightDirection, -1.0f);
    return 1;
}

int xglLightBoxColor(Vector4 *color, Vector4 *lightDirection, XglLightKey *key, Vector4 *receiver)
{
    DoublewordAlignedVector4 boxAxis = {0};
    float edgeDistance;

    boxAxis.z = 1.0f;
    edgeDistance = 1000000.0f;
    *lightDirection = *receiver;
    xglMatrixStackUnit();
    xglMatrixStackTrans(key);
    if (key->rotation.z != 0.0f) {
        xglMatrixStackRotZ(key->rotation.z);
    }
    if (key->rotation.y != 0.0f) {
        xglMatrixStackRotY(key->rotation.y);
    }
    if (key->rotation.x != 0.0f) {
        xglMatrixStackRotX(key->rotation.x);
    }
    xglMatrixStackMulVector(&boxAxis, &boxAxis);
    xglMatrixStackScale(&key->extent);
    xglMatrixStackInverse();
    lightDirection->w = 1.0f;
    xglMatrixStackMulVector(lightDirection, lightDirection);
    if (lightDirection->x < -1.0f || lightDirection->x > 1.0f || lightDirection->y < -1.0f || lightDirection->y > 1.0f || lightDirection->z > 0.0f) {
        return 0;
    }
    xglVectorCopy(color, &key->color);
    if (__builtin_fabsf((-1.0f - lightDirection->x) * key->extent.x) < edgeDistance) {
        edgeDistance = __builtin_fabsf((-1.0f - lightDirection->x) * key->extent.x);
    }
    if (__builtin_fabsf((1.0f - lightDirection->x) * key->extent.x) < edgeDistance) {
        edgeDistance = __builtin_fabsf((1.0f - lightDirection->x) * key->extent.x);
    }
    if (__builtin_fabsf((-1.0f - lightDirection->y) * key->extent.y) < edgeDistance) {
        edgeDistance = __builtin_fabsf((-1.0f - lightDirection->y) * key->extent.y);
    }
    if (__builtin_fabsf((1.0f - lightDirection->y) * key->extent.y) < edgeDistance) {
        edgeDistance = __builtin_fabsf((1.0f - lightDirection->y) * key->extent.y);
    }
    if (edgeDistance < 1.0f) {
        xglVectorScale(color, color, edgeDistance);
    }
    *lightDirection = boxAxis;
    return 1;
}

void xglVectorAddXYZ(AlignedVector4 *dst, AlignedVector4 *first, AlignedVector4 *second);
void func_001BB3FC(void *cache);
void xglLightSlotsInsert(XglLightSlots *work, Vector4 *vector, Vector4 *direction);
int xglLightPointColor(Vector4 *result, XglLightKey *key, Vector4 *origin);
int xglLightSpotColor(Vector4 *result, Vector4 *direction, XglLightKey *key, Vector4 *origin);
int xglLightBoxColor(Vector4 *result, Vector4 *direction, XglLightKey *key, Vector4 *origin);
int xglLightConeColor(Vector4 *result, Vector4 *direction, XglLightKey *key, Vector4 *origin);

/* Loads the key's Z, Y, X rotation into the matrix stack (identity first, zero angles skipped).
 * A plain block, not do/while (0): the loop's labels would renumber the output. */
#define LOAD_KEY_ROTATION(key) \
    { \
        xglMatrixStackUnit(); \
        if ((key)->rotation.z != 0.0f) { \
            xglMatrixStackRotZ((key)->rotation.z); \
        } \
        if ((key)->rotation.y != 0.0f) { \
            xglMatrixStackRotY((key)->rotation.y); \
        } \
        if ((key)->rotation.x != 0.0f) { \
            xglMatrixStackRotX((key)->rotation.x); \
        } \
    }
#define XGL_MOTION_TYPE_COUNT 19

extern const u64 D_0068E570[2];
extern u64 D_0068E578;
float xglPointLength(Vector4 *from, Vector4 *to);

int xglLightConeColor(Vector4 *color, Vector4 *lightDirection, XglLightKey *key, Vector4 *receiver)
{
    float keyMatrix[4][4];
    Vector4 coneScale;
    float innerSlope;
    float outerSlope;
    float fade;
    float coneRatio;
    DoublewordVector4 edgePoint;
    DoublewordVector4 spareVector0;

    innerSlope = key->extent.x / key->extent.z;
    outerSlope = key->extent.y / key->extent.z;
    fade = 1.0f;
    coneRatio = 1.0f;
    coneScale.x = key->extent.y;
    coneScale.y = key->extent.z;
    coneScale.z = key->extent.y;
    coneScale.w = 1.0f;

    *lightDirection = *receiver;
    xglMatrixStackUnit();
    xglMatrixStackTrans(key);
    if (key->rotation.z != 0.0f) {
        xglMatrixStackRotZ(key->rotation.z);
    }
    if (key->rotation.y != 0.0f) {
        xglMatrixStackRotY(key->rotation.y);
    }
    if (key->rotation.x != 0.0f) {
        xglMatrixStackRotX(key->rotation.x);
    }
    xglMatrixStackSave(keyMatrix);
    xglMatrixStackScale(&coneScale);
    xglMatrixStackInverse();
    lightDirection->w = 1.0f;
    xglMatrixStackMulVector(lightDirection, lightDirection);

    if (lightDirection->y > 1.0f || lightDirection->y < -1.0f) {
        edgePoint.words[0] = D_0068E570[0];
        edgePoint.words[1] = D_0068E578;
        if (lightDirection->y < -1.0f) {
            edgePoint.vector.y = -1.0f;
        }
        coneRatio = xglPointLength(lightDirection, &edgePoint.vector) * outerSlope;
        if (coneRatio >= outerSlope) {
            return 0;
        }
        if (coneRatio > innerSlope) {
            fade = 1.0f - (coneRatio - innerSlope) / (outerSlope - innerSlope);
        }
        xglVectorSubXYZ_001AF3F0(lightDirection, lightDirection, &edgePoint.vector);
        xglMatrixStackLoad(keyMatrix);
        lightDirection->w = 0.0f;
        xglMatrixStackMulVector(lightDirection, lightDirection);
        xglVectorNormal_001A9200(lightDirection, lightDirection);
        xglVectorScale(lightDirection, lightDirection, -1.0f);
        xglVectorCopy(color, &key->color);
        xglVectorScale(color, color, fade);
        return 1;
    } else {
        if (lightDirection->x > -1.0f && lightDirection->x < 1.0f && lightDirection->z > -1.0f && lightDirection->z < 1.0f) {
            coneRatio = sqrtf(lightDirection->x * lightDirection->x + lightDirection->z * lightDirection->z) * outerSlope;
            if (coneRatio >= outerSlope) {
                return 0;
            }
            if (coneRatio > innerSlope) {
                fade = 1.0f - (coneRatio - innerSlope) / (outerSlope - innerSlope);
            }
            lightDirection->y = 0.0f;
            xglMatrixStackLoad(keyMatrix);
            lightDirection->w = 0.0f;
            xglMatrixStackMulVector(lightDirection, lightDirection);
            xglVectorNormal_001A9200(lightDirection, lightDirection);
            xglVectorScale(lightDirection, lightDirection, -1.0f);
            xglVectorCopy(color, &key->color);
            xglVectorScale(color, color, fade);
            return 1;
        }
    }
    return 0;
}

static Vector4 xglMotionCache[1];

void *xglLightPackCacheGet(void)
{
    if (xglMotionCacheValid != 0) {
        return xglMotionCache;
    }
    return 0;
}

void xglLightPackCacheInvalidate(void)
{
    xglMotionCacheValid = 0;
}

void xglLightPackBuildLightSet(XglLightPack *pack, XglLightSet *lightSet, Vector4 *receiver)
{
    int i;
    int *entryOffsets;
    Vector4 lightSlots[12];

    entryOffsets = pack->entryOffsets;
    memset(lightSlots, 0, sizeof(lightSlots));
    xglZeroQuadwords18(lightSet);
    for (i = 0; i < pack->entryCount; i++) {
        XglLightEntry *entry = (XglLightEntry *)((u8 *)pack + entryOffsets[i]);

        if (entry->type < XGL_MOTION_TYPE_COUNT) {
            XglLightKey *key = &((XglLightKey *)((u8 *)pack + pack->keyOffset))[i];
            DoublewordAlignedVector4 axisDir = { 0.0f, 0.0f, -1.0f, 1.0f };
            Vector4 pointColor;
            Vector4 coneColor;
            Vector4 coneDirection;
            Vector4 spotColor;
            Vector4 spotDirection;
            Vector4 boxColor;
            Vector4 boxDirection;
            Vector4 scaledColor;

            switch (entry->type) {
            case 9:
                xglVectorScale(xglMotionCache, &key->color, key->intensity);
                func_001BB3FC(xglMotionCache);
                xglMotionCacheValid = 1;
                break;
            case 1:
                xglVectorScale(&lightSet->ambientColor, &key->color, key->intensity);
                xglVectorScale(&lightSet->ambientColorLower, &key->color, key->intensity);
                break;
            case 2:
                xglVectorScale(&lightSet->ambientColor, &key->color, key->intensity);
                xglVectorScale(&lightSet->ambientColorLower, &key->extent, key->intensity);
                LOAD_KEY_ROTATION(key)
                                xglMatrixStackMulVector(&axisDir, &axisDir);
                xglVectorCopy(&lightSet->viewDirection, &axisDir);
                break;
            case 3:
                if (!xglLightPointColor(&pointColor, key, receiver)) {
                    break;
                }
                xglVectorScale(&pointColor, &pointColor, key->intensity);
                xglVectorAddXYZ(&lightSet->ambientColor, &lightSet->ambientColor, &pointColor);
                xglVectorAddXYZ(&lightSet->ambientColorLower, &lightSet->ambientColorLower, &pointColor);
                break;
            case 7:
                if (!xglLightConeColor(&coneColor, &coneDirection, key, receiver)) {
                    break;
                }
                xglVectorScale(&coneColor, &coneColor, key->intensity);
                xglVectorAddXYZ(&lightSet->ambientColor, &lightSet->ambientColor, &coneColor);
                xglVectorAddXYZ(&lightSet->ambientColorLower, &lightSet->ambientColorLower, &coneColor);
                break;
            case 6:
                if (!xglLightSpotColor(&spotColor, &spotDirection, key, receiver)) {
                    break;
                }
                xglVectorScale(&spotColor, &spotColor, key->intensity);
                xglVectorAddXYZ(&lightSet->ambientColor, &lightSet->ambientColor, &spotColor);
                xglVectorAddXYZ(&lightSet->ambientColorLower, &lightSet->ambientColorLower, &spotColor);
                break;
            case 4:
                if (!xglLightBoxColor(&boxColor, &boxDirection, key, receiver)) {
                    break;
                }
                xglVectorScale(&boxColor, &boxColor, key->intensity);
                xglVectorAddXYZ(&lightSet->ambientColor, &lightSet->ambientColor, &boxColor);
                xglVectorAddXYZ(&lightSet->ambientColorLower, &lightSet->ambientColorLower, &boxColor);
                break;
            case 8:
                axisDir.z = 1.0f;
                LOAD_KEY_ROTATION(key)
                                xglMatrixStackMulVector(&axisDir, &axisDir);
                xglVectorScale(&scaledColor, &key->color, key->intensity);
                xglLightSlotsInsert(lightSlots, &scaledColor, &axisDir);
                break;
            case 10:
                axisDir.z = 1.0f;
                LOAD_KEY_ROTATION(key)
                                xglMatrixStackMulVector(&lightSet->forward, &axisDir);
                lightSet->forward.w = 1.0f;
                break;
            case 11:
                xglVectorSubXYZ_001AF3F0(&lightSet->forward, &key->position, receiver);
                xglVectorNormal_001A9200(&lightSet->forward, &lightSet->forward);
                lightSet->forward.w = 1.0f;
                break;
            }
        } else {
            break;
        }
    }
    {
        int pointCount;

        pointCount = 0;
        for (i = 0; i < pack->entryCount; i++) {
            XglLightEntry *entry = (XglLightEntry *)((u8 *)pack + entryOffsets[i]);

            if (entry->type >= XGL_MOTION_TYPE_COUNT) {
                XglLightKey *key = &((XglLightKey *)((u8 *)pack + pack->keyOffset))[i];
                Vector4 pointColor;
                Vector4 pointDirection;
                Vector4 coneColor;
                Vector4 coneDirection;
                Vector4 boxColor;
                Vector4 boxDirection;
                Vector4 spotColor;
                Vector4 spotDirection;

                switch (entry->type) {
                case 19:
                    if (pointCount < 3) {
                        xglVectorCopy((Vector4 *)lightSet->pointPositions[pointCount], &key->position);
                        xglVectorScale((Vector4 *)lightSet->pointColors[pointCount], &key->color,
                            key->intensity * 128.0f);
                        lightSet->pointPositions[pointCount][3] = 1.0f;
                        pointCount++;
                    }
                    break;
                case 20:
                case 35:
                    if (!xglLightPointColor(&pointColor, key, receiver)) {
                        break;
                    }
                    xglVectorSubXYZ_001AF3F0(&pointDirection, &key->position, receiver);
                    xglVectorScale(&pointColor, &pointColor, key->intensity);
                    xglLightSlotsInsert(lightSlots, &pointColor, &pointDirection);
                    break;
                case 24:
                    if (!xglLightConeColor(&coneColor, &coneDirection, key, receiver)) {
                        break;
                    }
                    xglVectorScale(&coneColor, &coneColor, key->intensity);
                    xglLightSlotsInsert(lightSlots, &coneColor, &coneDirection);
                    break;
                case 21:
                    if (!xglLightBoxColor(&boxColor, &boxDirection, key, receiver)) {
                        break;
                    }
                    xglVectorScale(&boxColor, &boxColor, key->intensity);
                    xglLightSlotsInsert(lightSlots, &boxColor, &boxDirection);
                    break;
                case 23:
                    if (!xglLightSpotColor(&spotColor, &spotDirection, key, receiver)) {
                        break;
                    }
                    xglVectorScale(&spotColor, &spotColor, key->intensity);
                    xglLightSlotsInsert(lightSlots, &spotColor, &spotDirection);
                    break;
                }
            }
        }
    }
    {
        int slot;

        for (slot = 0; slot < 6; slot++) {
            xglVectorCopy((Vector4 *)lightSet->colors[slot], &lightSlots[slot]);
            xglVectorCopy((Vector4 *)lightSet->directions[slot], &lightSlots[slot + 6]);
            lightSet->colors[slot][3] = 0;
        }
    }
}

int strcmp(const char *s1, const char *s2);

void xglLightPackBuildByName(XglLightSet *light, const char *packName, Vector4 *receiver)
{
    int i;

    xglMotionCacheValid = 0;
    for (i = 0; i < 16; i++) {
        if (xglLightPackTable[i].pack != 0) {
            if (strcmp(xglLightPackTable[i].pack->name, packName) == 0) {
                xglLightPackBuildLightSet(xglLightPackTable[i].pack, light, receiver);
                return;
            }
        }
    }
}

void xglLightPackTableClear(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        xglLightPackTable[i].pack = 0;
        xglLightPackTable[i].refs = 0;
    }
}

void xglLightPackEvaluate(XglLightPack *pack, float time);

void xglLightPackEvaluateAll(float time)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (xglLightPackTable[i].pack != 0) {
            xglLightPackEvaluate(xglLightPackTable[i].pack, time);
        }
    }
}

extern const u64 D_0068E610[10];
void xglMatrixStackScale(const Vector4 *vector);
void xglMatrixStackTrans(const Vector4 *vector);

void xglLightPackDebugBox(XglLightKey *key)
{
    int scaled[3];
    DoublewordVector4 point0;
    DoublewordVector4 point1;
    DoublewordVector4 point2;
    DoublewordVector4 point3;
    DoublewordVector4 point4;
    DoublewordVector4 spareVector0;
    DoublewordVector4 spareVector1;
    DoublewordVector4 spareVector2;

    point0.words[0] = D_0068E610[0];
    point0.words[1] = D_0068E610[1];
    point1.words[0] = D_0068E610[2];
    point1.words[1] = D_0068E610[3];
    point2.words[0] = D_0068E610[4];
    point2.words[1] = D_0068E610[5];
    point3.words[0] = D_0068E610[6];
    point3.words[1] = D_0068E610[7];
    point4.words[0] = D_0068E610[8];
    point4.words[1] = D_0068E610[9];
    xglMatrixStackUnit();
    xglMatrixStackTrans(key);
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackScale(&key->extent);
    xglMatrixStackMulVector(&point0.vector, &point0.vector);
    xglMatrixStackMulVector(&point1.vector, &point1.vector);
    xglMatrixStackMulVector(&point2.vector, &point2.vector);
    xglMatrixStackMulVector(&point3.vector, &point3.vector);
    xglMatrixStackMulVector(&point4.vector, &point4.vector);
    /* scaled[] is stored and never read in the original. */
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
}

void xglLightPackDebugAmbient(void *state)
{
}

void *memset(void *destination, int value, unsigned int count);
float sinf(float angle);
float cosf(float angle);
float func_002A1D08(float value);
/* gp-relative float constants of this TU (.sdata). */

void xglLightPackDebugSpot(XglLightKey *key)
{
    int scaled[3];
    Vector4 baseRing[8];
    Vector4 apex;
    float height;
    int i;
    float radius;
    register unsigned int size;
    register Vector4 *entry;
    register Vector4 *entry2;
    register int offset;
    register int zoff8;
    register int zoff12;
    register unsigned int tb;
    register int index;
    register unsigned int tby;

    memset(&apex, 0, (size = 16, size));
    apex.w = 1.0f;
    height = key->extent.z * 0.44f + 0.867f;
    radius = func_002A1D08(key->coneAngle * 0.5f) * height;
    apex.z = -height * 1.2f;
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
    for (i = 0; i < 8; i++) {
        index = i << 4;
        tb = (unsigned int)baseRing;
        entry = (Vector4 *)(tb + index);
        entry->x = radius * cosf(2.0f * ((float)i / 8.0f * 3.141592f));
        offset = i * 16 + 4;
        tby = (unsigned int)baseRing;
        entry2 = (Vector4 *)(tby + offset);
        entry2->x = radius * sinf(2.0f * ((float)i / 8.0f * 3.141592f));
        zoff8 = i * 16 + 8;
        *(float *)(zoff8 + (unsigned int)baseRing) = -height;
        zoff12 = i * 16 + 12;
        *(float *)(zoff12 + (unsigned int)baseRing) = 1.0f;
    }
    xglMatrixStackUnit();
    xglMatrixStackTrans(key);
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackMulVector(&apex, &apex);
    for (i = 0; i < 8; i++) {
        xglMatrixStackMulVector(&baseRing[i], &baseRing[i]);
    }
    {
        for (i = 0; i < 8; i++) {
        }
    }
    {
        for (i = 0; i < 8; i++) {
        }
    }
}

struct XglLightKeyView {
    DoublewordAlignedVector4 position;
    u32 unmodeled_10[4];
    float extentX;
    float extentY;
    u32 unmodeled_28[2];
    float colorX;
    float colorY;
    float colorZ;
    u32 unmodeled_3C[3];
    float intensity;
};

void xglLightPackDebugMarker(struct XglLightKeyView *key)
{
    int spare0;
    int spare1;
    int spare2;
    int x;
    int y;
    int z;
    DoublewordAlignedVector4 copy;

    copy = key->position;
    copy.w = 0.5f;
    x = key->colorX * 128.0f * key->intensity;
    y = key->colorY * 128.0f * key->intensity;
    z = key->colorZ * 128.0f * key->intensity;
}

/* The 0x0068E660 table: four Vector4 (x, y, z, w) copied as eight doublewords. Each word keeps its own
 * scaffold label, so the copies reference the same symbols as the original. */
extern const u64 D_0068E660[8];

void xglLightPackDebugDirectional(XglLightKey *key)
{
    int scaled[3];
    DoublewordVector4 point0;
    DoublewordVector4 point1;
    DoublewordVector4 point2;
    DoublewordVector4 point3;
    DoublewordVector4 spareVector0;
    DoublewordVector4 spareVector1;

    point0.words[0] = D_0068E660[0];
    point0.words[1] = D_0068E660[1];
    point1.words[0] = D_0068E660[2];
    point1.words[1] = D_0068E660[3];
    point2.words[0] = D_0068E660[4];
    point2.words[1] = D_0068E660[5];
    point3.words[0] = D_0068E660[6];
    point3.words[1] = D_0068E660[7];
    /* scaled[] is stored and never read in the original. */
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
    xglMatrixStackUnit();
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackMulVector(&point0.vector, &point0.vector);
    xglMatrixStackMulVector(&point1.vector, &point1.vector);
    xglMatrixStackMulVector(&point2.vector, &point2.vector);
    xglMatrixStackMulVector(&point3.vector, &point3.vector);
}

void xglLightPackDebugPoint(struct XglLightKeyView *key)
{
    int spare0;
    int spare1;
    int spare2;
    int x;
    int y;
    int z;
    DoublewordAlignedVector4 copy;

    copy = key->position;
    copy.w = key->extentX;
    x = key->colorX * 128.0f * key->intensity;
    y = key->colorY * 128.0f * key->intensity;
    z = key->colorZ * 128.0f * key->intensity;
    copy.w = key->extentY;
}

extern const u64 D_0068E6A0[4];

void xglLightPackDebugCone(XglLightKey *key)
{
    int scaled[3];
    Vector4 baseRing[8];
    Vector4 ringA[8];
    Vector4 ringB[8];
    Vector4 ringC[8];
    Vector4 ringD[8];
    Vector4 coneScale;
    DoublewordVector4 point0;
    DoublewordVector4 point1;
    int i;
    float radius;
    Vector4 spareVector0;
    register Vector4 *entry;
    register Vector4 *entry2;
    register Vector4 *entry3;
    register Vector4 *entry4;
    register float *zptr;
    register float *zptr2;
    register int index;
    register int index2;
    register unsigned int tb;
    register unsigned int tb2;
    register int zoff8;
    register int zoff82;
    register int off;
    register int off2;
    register int off3;
    register int off4;
    register int off5;
    register int off6;
    register float *zD;
    register float *zC;
    register float *zD2;
    register float *zC2;
    register int off11;
    register int off12;
    register int index3;
    register int index4;
    register int off7;
    register int off8;
    register int off9;
    register int off10;
    register unsigned int tbd;
    register unsigned int tbc;
    register unsigned int tbc2;
    register unsigned int tbd2;
    register unsigned int tbd3;
    register unsigned int tbc3;
    register unsigned int tbd4;
    register unsigned int tbc4;
    register unsigned int tby;
    register unsigned int tby2;
    register unsigned int tby3;
    register unsigned int tby4;
    register unsigned int tby5;
    register unsigned int tby6;
    register unsigned int tby7;
    register unsigned int tby8;

    point0.words[0] = D_0068E6A0[0];
    point0.words[1] = D_0068E6A0[1];
    point1.words[0] = D_0068E6A0[2];
    point1.words[1] = D_0068E6A0[3];
    radius = 1.0f;
    point0.vector.y = (1.0f / key->extent.z) * key->extent.y + 1.0f;
    point1.vector.y = -1.0f - (1.0f / key->extent.z) * key->extent.y;
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
    coneScale.x = key->extent.x;
    coneScale.y = key->extent.z;
    coneScale.z = key->extent.x;
    coneScale.w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(key);
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackScale(&coneScale);
    xglMatrixStackMulVector(&point0.vector, &point0.vector);
    xglMatrixStackMulVector(&point1.vector, &point1.vector);
    for (i = 0; i < 8; i++) {
        index = i << 4;
        tb = (unsigned int)baseRing;
        entry = (Vector4 *)(tb + index);
        entry->x = cosf(2.0f * ((float)i / 8.0f * 3.141592f));
        off = i * 16 + 4;
        tby = (unsigned int)baseRing;
        *(float *)(tby + off) = 1.0f;
        zoff8 = i * 16 + 8;
        tby2 = (unsigned int)baseRing;
        zptr = (float *)(tby2 + zoff8);
        *zptr = sinf(2.0f * ((float)i / 8.0f * 3.141592f));
        off2 = i * 16 + 12;
        tby3 = (unsigned int)baseRing;
        *(float *)(tby3 + off2) = 1.0f;
        xglMatrixStackMulVector(&ringA[i], &baseRing[i]);
        off3 = i * 16 + 4;
        tby4 = (unsigned int)baseRing;
        *(float *)(tby4 + off3) = -1.0f;
        xglMatrixStackMulVector(&ringB[i], &baseRing[i]);
    }
    {
        for (i = 0; i < 8; i++) {
        }
    }
    coneScale.x = key->extent.y;
    coneScale.z = key->extent.y;
    xglMatrixStackUnit();
    xglMatrixStackTrans(key);
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackScale(&coneScale);
    {
        for (i = 0; i < 8; i++) {
        index2 = i << 4;
        tb2 = (unsigned int)baseRing;
        entry3 = (Vector4 *)(tb2 + index2);
        entry3->x = radius * cosf(2.0f * ((float)i / 8.0f * 3.141592f));
        off4 = i * 16 + 4;
        tby5 = (unsigned int)baseRing;
        *(float *)(tby5 + off4) = 1.0f;
        zoff82 = i * 16 + 8;
        tby6 = (unsigned int)baseRing;
        zptr2 = (float *)(tby6 + zoff82);
        *zptr2 = radius * sinf(2.0f * ((float)i / 8.0f * 3.141592f));
        off5 = i * 16 + 12;
        tby7 = (unsigned int)baseRing;
        *(float *)(tby7 + off5) = 1.0f;
        xglMatrixStackMulVector(&ringA[i], &baseRing[i]);
        off6 = i * 16 + 4;
        tby8 = (unsigned int)baseRing;
        *(float *)(tby8 + off6) = -1.0f;
        xglMatrixStackMulVector(&ringB[i], &baseRing[i]);
        index3 = i << 4;
        tbd = (unsigned int)ringD;
        entry2 = (Vector4 *)(tbd + index3);
        index4 = i << 4;
        tbc = (unsigned int)ringC;
        entry4 = (Vector4 *)(tbc + index4);
        entry2->x = entry4->x = cosf(2.0f * ((float)i / 8.0f * 3.141592f)) * 0.707f;
        off7 = i * 16 + 4;
        tbc2 = (unsigned int)ringC;
        *(float *)(tbc2 + off7) = (0.707f / key->extent.z) * key->extent.y + 1.0f;
        off8 = i * 16 + 4;
        tbd2 = (unsigned int)ringD;
        *(float *)(tbd2 + off8) = -1.0f - (0.707f / key->extent.z) * key->extent.y;
        off9 = i * 16 + 8;
        tbd3 = (unsigned int)ringD;
        zD = (float *)(tbd3 + off9);
        off11 = i * 16 + 8;
        tbc3 = (unsigned int)ringC;
        zC = (float *)(tbc3 + off11);
        *zD = *zC = sinf(2.0f * ((float)i / 8.0f * 3.141592f)) * 0.707f;
        off10 = i * 16 + 12;
        tbd4 = (unsigned int)ringD;
        zD2 = (float *)(tbd4 + off10);
        off12 = i * 16 + 12;
        tbc4 = (unsigned int)ringC;
        zC2 = (float *)(tbc4 + off12);
        *zD2 = *zC2 = 1.0f;
        xglMatrixStackMulVector(&ringC[i], &ringC[i]);
        xglMatrixStackMulVector(&ringD[i], &ringD[i]);
        }
    }
    {
        for (i = 0; i < 8; i++) {
        }
    }
}

/* The 0x0068E6C0 table: three Vector4 copied as six doublewords, one scaffold label per word. */
extern const u64 D_0068E6C0[6];

void xglLightPackDebugAxis(XglLightKey *key)
{
    int scaled[3];
    Vector4 zero;
    DoublewordVector4 point0;
    DoublewordVector4 point1;
    DoublewordVector4 point2;
    DoublewordVector4 spareVector0;
    DoublewordVector4 spareVector1;
    register unsigned int size;

    /* The count is stored inside the call: the original loads it after the buffer address. */
    memset(&zero, 0, (size = 16, size));
    point0.words[0] = D_0068E6C0[0];
    point0.words[1] = D_0068E6C0[1];
    point1.words[0] = D_0068E6C0[2];
    point1.words[1] = D_0068E6C0[3];
    point2.words[0] = D_0068E6C0[4];
    point2.words[1] = D_0068E6C0[5];
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
    xglMatrixStackUnit();
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackMulVector(&point0.vector, &point0.vector);
    xglMatrixStackMulVector(&point1.vector, &point1.vector);
    xglMatrixStackMulVector(&point2.vector, &point2.vector);
    scaled[0] = key->extent.x * 128.0f;
    scaled[1] = key->extent.y * 128.0f;
    scaled[2] = key->extent.z * 128.0f;
    xglVectorSubXYZ_001AF3F0(&point0.vector, &zero, &point0.vector);
}

void xglLightPackDebugAmbient(void *state);
void xglLightPackDebugMarker(struct XglLightKeyView *key);
void xglLightPackDebugPoint(struct XglLightKeyView *key);

void xglLightPackDebugDrawAll(void)
{
    int i;
    XglLightKey *key;
    int *entryOffsets;
    int j;
    XglLightEntry *entry;

    for (i = 0; i < 16; i++) {
        if (xglLightPackTable[i].pack != 0) {
            key = (XglLightKey *)((u8 *)xglLightPackTable[i].pack + xglLightPackTable[i].pack->keyOffset);
            entryOffsets = xglLightPackTable[i].pack->entryOffsets;
            for (j = 0; j < xglLightPackTable[i].pack->entryCount; j++) {
                entry = (XglLightEntry *)((u8 *)xglLightPackTable[i].pack + entryOffsets[j]);
                switch (entry->type - 1) {
                case 0:
                    xglLightPackDebugAmbient(key);
                    break;
                case 1:
                    xglLightPackDebugAxis(key);
                    break;
                case 7:
                case 9:
                    xglLightPackDebugDirectional(key);
                    break;
                case 3:
                case 4:
                case 20:
                case 21:
                    xglLightPackDebugBox(key);
                    break;
                case 5:
                case 22:
                    xglLightPackDebugSpot(key);
                    break;
                case 23:
                    xglLightPackDebugCone(key);
                    break;
                case 10:
                case 18:
                    xglLightPackDebugMarker(key);
                    break;
                case 2:
                case 19:
                case 34:
                    xglLightPackDebugPoint(key);
                    break;
                }
                key++;
            }
        }
    }
}

void xglVectorCopy(register AlignedVector4 *dst, register AlignedVector4 *src)
{
    QuadwordCopy(dst, src);
}

void xglVectorZero(register AlignedVector4 *v)
{
    QuadwordZero(v);
}

void xglVectorSub(register AlignedVector4 *dst, register AlignedVector4 *minuend, register AlignedVector4 *subtrahend)
{
    Vu0SubVector(dst, minuend, subtrahend);
}

void xglVectorAddXYZ(register AlignedVector4 *dst, register AlignedVector4 *first, register AlignedVector4 *second)
{
    Vu0AddVectorXYZ(dst, first, second);
}

void xglVectorSubXYZ_001AF3F0(register AlignedVector4 *dst, register AlignedVector4 *minuend, register AlignedVector4 *subtrahend)
{
    Vu0SubVectorXYZ(dst, minuend, subtrahend);
}

void xglZeroQuadwords18(register AlignedVector4 *dst)
{
    __asm__ __volatile__(
        "sq $0, 0(%0)\n"
        "sq $0, 16(%0)\n"
        "sq $0, 32(%0)\n"
        "sq $0, 48(%0)\n"
        "sq $0, 64(%0)\n"
        "sq $0, 80(%0)\n"
        "sq $0, 96(%0)\n"
        "sq $0, 112(%0)\n"
        "sq $0, 128(%0)\n"
        "sq $0, 144(%0)\n"
        "sq $0, 160(%0)\n"
        "sq $0, 176(%0)\n"
        "sq $0, 192(%0)\n"
        "sq $0, 208(%0)\n"
        "sq $0, 224(%0)\n"
        "sq $0, 240(%0)\n"
        "sq $0, 256(%0)\n"
        "sq $0, 272(%0)\n"
        :
        : "r" (dst)
        : "memory");
}

Matrix4 xglIdentityMatrix = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };

Matrix4 D_00645330 = { { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };

const u64 D_0068E570[2] = { 0x3F80000000000000ULL, 0x3F80000000000000ULL };

const u64 D_0068E610[10] = {
    0x3F8000003F800000ULL, 0x3F80000000000000ULL, 0xBF8000003F800000ULL, 0x3F80000000000000ULL, 0xBF800000BF800000ULL, 0x3F80000000000000ULL, 0x3F800000BF800000ULL, 0x3F80000000000000ULL,
    0, 0x3F800000BF800000ULL
};

const u64 D_0068E660[8] = {
    0, 0x3F8000003F800000ULL, 0, 0x3F800000BF800000ULL, 0x3DCCCCCDULL, 0x3F800000BE99999AULL, 0xBDCCCCCDULL, 0x3F800000BE99999AULL
};

const u64 D_0068E6A0[4] = {
    0x3F80000000000000ULL, 0x3F80000000000000ULL, 0xBF80000000000000ULL, 0x3F80000000000000ULL
};

const u64 D_0068E6C0[6] = {
    0, 0x3F800000BF800000ULL, 0x3E99999AULL, 0x3F80000000000000ULL, 0xBE99999AULL, 0x3F80000000000000ULL
};
