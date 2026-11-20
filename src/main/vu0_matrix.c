#include "common.h"
#include "shared.h"
#include "main/vu0_matrix.h"
#include "vu0_macro.h"

/* Current matrix = current x M, M given by four rows (vf26-vf29 are the current matrix). */
void vu0CurMatrixMulRight(Vector4 *rows) {
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(rows) : "memory");
    __asm__ __volatile__("lqc2 vf2, 16(%0)" : : "r"(rows) : "memory");
    __asm__ __volatile__("lqc2 vf3, 32(%0)" : : "r"(rows) : "memory");
    __asm__ __volatile__("lqc2 vf4, 48(%0)" : : "r"(rows) : "memory");
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

/* Current matrix = current x [M; (0,0,0,1)] for three argument rows; row 3 is untouched. */
void vu0CurMatrixMulRows3(Vector4 *rows) {
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(rows) : "memory");
    __asm__ __volatile__("lqc2 vf2, 16(%0)" : : "r"(rows) : "memory");
    __asm__ __volatile__("lqc2 vf3, 32(%0)" : : "r"(rows) : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf1, vf26x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf2, vf26y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf3, vf26z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf26, vf0, vf26w" : : : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf1, vf27x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf2, vf27y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf3, vf27z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf27, vf0, vf27w" : : : "memory");
    __asm__ __volatile__("vmulax.xyzw ACC, vf1, vf28x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf2, vf28y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf3, vf28z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf28, vf0, vf28w" : : : "memory");
}

/* Sets the current matrix (vf26-vf29) to a rotation about X by angle; 0 gives the identity. The VU0
 * micro routines at 0xE8 and 0x20 return the cosine and the sine of vf4x in vf1x. */
void vu0SetCurrentRotationX(float angle)
{
    if (angle == 0.0f) {
        Vu0SetCurrentIdentity(vu0IdentityMatrix);
        return;
    }
    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "lqc2 vf26, 0(%0)\n\t"
        "vmove.xyzw vf29, vf0\n\t"
        "vsub.xyzw vf27, vf0, vf0\n\t"
        "vsub.xyzw vf28, vf0, vf0\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0xE8\n\t"
        "vaddx.y vf27, vf0, vf1x\n\t"
        "vaddx.z vf28, vf0, vf1x\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0x20\n\t"
        "vaddx.z vf27, vf0, vf1x\n\t"
        "vsubx.y vf28, vf0, vf1x"
        : : "r"(vu0IdentityMatrix), "f"(angle) : "$8", "memory");
}

/* Sets the current matrix (vf26-vf29) to a rotation about Y by angle; 0 gives the identity. The VU0
 * micro routines at 0xE8 and 0x20 return the cosine and the sine of vf4x in vf1x. */
void vu0SetCurrentRotationY(float angle)
{
    if (angle == 0.0f) {
        Vu0SetCurrentIdentity(vu0IdentityMatrix);
        return;
    }
    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "lqc2 vf27, 16(%0)\n\t"
        "vmove.xyzw vf29, vf0\n\t"
        "vsub.xyzw vf26, vf0, vf0\n\t"
        "vsub.xyzw vf28, vf0, vf0\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0xE8\n\t"
        "vaddx.x vf26, vf0, vf1x\n\t"
        "vaddx.z vf28, vf0, vf1x\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0x20\n\t"
        "vsubx.z vf26, vf0, vf1x\n\t"
        "vaddx.x vf28, vf0, vf1x"
        : : "r"(vu0IdentityMatrix), "f"(angle) : "$8", "memory");
}

/* Sets the current matrix (vf26-vf29) to a rotation about Z by angle; 0 gives the identity. The VU0
 * micro routines at 0xE8 and 0x20 return the cosine and the sine of vf4x in vf1x. */
void vu0SetCurrentRotationZ(float angle)
{
    if (angle == 0.0f) {
        Vu0SetCurrentIdentity(vu0IdentityMatrix);
        return;
    }
    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "lqc2 vf28, 32(%0)\n\t"
        "vmove.xyzw vf29, vf0\n\t"
        "vsub.xyzw vf26, vf0, vf0\n\t"
        "vsub.xyzw vf27, vf0, vf0\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0xE8\n\t"
        "vaddx.x vf26, vf0, vf1x\n\t"
        "vaddx.y vf27, vf0, vf1x\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0x20\n\t"
        "vaddx.y vf26, vf0, vf1x\n\t"
        "vsubx.x vf27, vf0, vf1x"
        : : "r"(vu0IdentityMatrix), "f"(angle) : "$8", "memory");
}
