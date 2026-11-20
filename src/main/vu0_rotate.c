#include "common.h"
#include "shared.h"
#include "vu0_macro.h"
#include "main/sef.h"
#include "main/vu0_matrix.h"
#include "main/vu0_rotate.h"
#include "main/m_math.h"

/* Multiplies the current matrix (vf26-vf29) by a rotation about X by angle (nothing for 0); the VU0
 * micro routines at 0xE8 and 0x20 return the cosine and the sine of vf4x in vf1x. */
void vu0RotateCurrentX(float angle)
{
    if (angle != 0.0f) {
        __asm__ __volatile__(
            "mfc1 $8, %0\n\t"
            "qmtc2.ni $8, vf4\n\t"
            "vcallms 0xE8\n\t"
            "vmove.x vf5, vf1\n\t"
            "qmtc2.ni $8, vf4\n\t"
            "vcallms 0x20\n\t"
            "vmove.x vf6, vf1\n\t"
            "vmulax.xyzw ACC, vf27, vf5x\n\t"
            "vmaddx.xyzw vf1, vf28, vf6x\n\t"
            "vmulax.xyzw ACC, vf28, vf5x\n\t"
            "vmsubx.xyzw vf28, vf27, vf6x\n\t"
            "vmove.xyzw vf27, vf1"
            : : "f"(angle) : "$8", "memory");
    }
}

/* Multiplies the current matrix (vf26-vf29) by a rotation about Y by angle (nothing for 0); the VU0
 * micro routines at 0xE8 and 0x20 return the cosine and the sine of vf4x in vf1x. */
void vu0RotateCurrentY(float angle)
{
    if (angle != 0.0f) {
        __asm__ __volatile__(
            "mfc1 $8, %0\n\t"
            "qmtc2.ni $8, vf4\n\t"
            "vcallms 0xE8\n\t"
            "vmove.x vf5, vf1\n\t"
            "qmtc2.ni $8, vf4\n\t"
            "vcallms 0x20\n\t"
            "vmove.x vf6, vf1\n\t"
            "vmulax.xyzw ACC, vf26, vf5x\n\t"
            "vmsubx.xyzw vf1, vf28, vf6x\n\t"
            "vmulax.xyzw ACC, vf28, vf5x\n\t"
            "vmaddx.xyzw vf28, vf26, vf6x\n\t"
            "vmove.xyzw vf26, vf1"
            : : "f"(angle) : "$8", "memory");
    }
}

/* Multiplies the current matrix (vf26-vf29) by a rotation about Z by angle (nothing for 0); the VU0
 * micro routines at 0xE8 and 0x20 return the cosine and the sine of vf4x in vf1x. */
void vu0RotateCurrentZ(float angle)
{
    if (angle != 0.0f) {
        __asm__ __volatile__(
            "mfc1 $8, %0\n\t"
            "qmtc2.ni $8, vf4\n\t"
            "vcallms 0xE8\n\t"
            "vmove.x vf5, vf1\n\t"
            "qmtc2.ni $8, vf4\n\t"
            "vcallms 0x20\n\t"
            "vmove.x vf6, vf1\n\t"
            "vmulax.xyzw ACC, vf26, vf5x\n\t"
            "vmaddx.xyzw vf1, vf27, vf6x\n\t"
            "vmulax.xyzw ACC, vf27, vf5x\n\t"
            "vmsubx.xyzw vf27, vf26, vf6x\n\t"
            "vmove.xyzw vf26, vf1"
            : : "f"(angle) : "$8", "memory");
    }
}


/* Euler rotation of the current matrix: X, then Y, then Z. */
void vu0RotateCurrentXYZ(Vector4 *angles)
{
    vu0RotateCurrentX(angles->x);
    vu0RotateCurrentY(angles->y);
    vu0RotateCurrentZ(angles->z);
}

/* Same three rotations in the order Y, X, Z. */
void vu0RotateCurrentYXZ(Vector4 *angles)
{
    vu0RotateCurrentY(angles->y);
    vu0RotateCurrentX(angles->x);
    vu0RotateCurrentZ(angles->z);
}

/* Same three rotations in the order Z, Y, X. */
void vu0RotateCurrentZYX(Vector4 *angles)
{
    vu0RotateCurrentZ(angles->z);
    vu0RotateCurrentY(angles->y);
    vu0RotateCurrentX(angles->x);
}


/* Sets the current matrix to a Y rotation, then rotates it by X and Z. */
void vu0SetCurrentRotationYXZ(Vector4 *angles)
{
    vu0SetCurrentRotationY(angles->y);
    vu0RotateCurrentX(angles->x);
    vu0RotateCurrentZ(angles->z);
}

/* Transforms a point by the current matrix; the result stays in vf29. */
void vu0TransformPoint(Vector4 *point)
{
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(point) : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf26, vf1x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf27, vf1y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf28, vf1z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf29, vf29, vf1w" : : : "memory");
}

/* Multiplies the current matrix by a translation: identity rows 0-2 and offset as row 3. */
void vu0TranslateCurrent(Vector4 *offset)
{
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(vu0IdentityMatrix) : "memory");
    __asm__ __volatile__("lqc2 vf2, 16(%0)" : : "r"(vu0IdentityMatrix) : "memory");
    __asm__ __volatile__("lqc2 vf3, 32(%0)" : : "r"(vu0IdentityMatrix) : "memory");
    __asm__ __volatile__("lqc2 vf4, 0(%0)" : : "r"(offset) : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf1, vf26x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf2, vf26y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf3, vf26z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf26, vf4, vf26w" : : : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf1, vf27x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf2, vf27y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf3, vf27z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf27, vf4, vf27w" : : : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf1, vf28x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf2, vf28y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf3, vf28z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf28, vf4, vf28w" : : : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf1, vf29x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf2, vf29y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf3, vf29z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf29, vf4, vf29w" : : : "memory");
}

void vu0NoOpUnused(void) {

}


/* Row 3 of the matrix, which holds the direction while the rotation is built: the zero test reads
 * x and y as one doubleword. */
typedef union Vu0Direction {
    Vector4 vector;
    struct {
        unsigned long long xy;
        int z;
    } bits;
} Vu0Direction;

/* Sets matrix to the rotation that turns the Z axis towards `to` - `from` (Y rotation from the
 * heading, then X from the pitch) and returns 1; for equal points it becomes the identity and the
 * result is 0. Rows 2 and 3 hold the angles and the direction until the matrix is stored. */
int vu0SetRotationFromDirection(Matrix4 matrix, Vector4 *to, Vector4 *from)
{
    Vu0Direction *direction = (Vu0Direction *) matrix[3];
    float *angles;
    int result = 0;
    int zero;
    float length;
    float pitch;

    Vu0SubVectorXYZ12(direction, to, from);
    zero = (direction->bits.z | direction->bits.xy) == 0;
    if (zero) {
        Vu0SetCurrentIdentity(vu0IdentityMatrix);
    } else {
        pitch = -direction->vector.y;
        angles = matrix[2];
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n\t"
            "vmul.xz vf1, vf1, vf1\n\t"
            "vaddz.x vf1, vf1, vf1z\n\t"
            "vsqrt Q, vf1x\n\t"
            "sqc2 vf0, 0(%2)\n\t"
            "vwaitq\n\t"
            "vaddq.x vf1, vf0, Q\n\t"
            "qmfc2.ni $8, vf1\n\t"
            "mtc1 $8, %0"
            : "=f"(length) : "r"(direction), "r"(angles) : "$8", "memory");
        angles[0] = srsAtan2(pitch, length);
        angles[1] = srsAtan2(direction->vector.x, direction->vector.z);
        vu0SetCurrentRotationY(angles[1]);
        vu0RotateCurrentX(angles[0]);
        result = 1;
    }
    __asm__ __volatile__(
        "sqc2 vf26, 0(%0)\n\t"
        "sqc2 vf27, 16(%0)\n\t"
        "sqc2 vf28, 32(%0)\n\t"
        "sqc2 vf29, 48(%0)"
        : : "r"(matrix) : "memory");
    return result;
}

/* Copies a matrix: rows 0-2 from src, row 3 from the translation vector in aux. Returns the distance from aux to
 * direction. */
float vu0CopyMatrixWithTranslationDist(Vector4 *dst, Matrix4 src, Vector4 *aux, Vector4 *direction)
{
    float (*source)[4];
    Vector4 *translation = aux;
    Vector4 *destination = dst;
    float distance = MMathCalcDist(translation, direction);
    source = src;
    __asm__ __volatile__("lqc2 vf26, 0(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("lqc2 vf27, 16(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("lqc2 vf28, 32(%0)" : : "r"(source) : "memory");
    __asm__ __volatile__("lqc2 vf29, 0(%0)" : : "r"(translation) : "memory");
    __asm__ __volatile__("sqc2 vf26, 0(%0)" : : "r"(destination) : "memory");
    __asm__ __volatile__("sqc2 vf27, 16(%0)" : : "r"(destination) : "memory");
    __asm__ __volatile__("sqc2 vf28, 32(%0)" : : "r"(destination) : "memory");
    __asm__ __volatile__("sqc2 vf29, 48(%0)" : : "r"(destination) : "memory");
    return distance;
}
