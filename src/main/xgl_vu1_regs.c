#include "common.h"
#include "main/xgl_vu1_regs.h"

float atanf(float ratio);

/*
 * Angle of the vector (x, y) in radians: atan of y / x, then moved into the
 * half-plane of the vector. A zero x is replaced by a tiny value.
 */
float xglAtan2Angle(float x, float y)
{
    float angle;
    float divisor;

    divisor = x;
    if (divisor == 0.0f) {
        divisor = 0.00001f;
    }
    angle = atanf(y / divisor);
    if (divisor > 0.0f) {
        if (angle < 0.0f) {
            angle += 6.2831855f;
        }
    } else {
        angle += 3.1415927f;
    }
    return angle;
}

#include "shared.h"

/* VU0 data-memory quadword addresses of the VU1 register windows, and the quadwords read from each. */
#define VU1_CONTROL_WINDOW 0x430
#define VU1_INTEGER_WINDOW 0x420
#define VU1_REGISTER_QUADS 16

/*
 * Reads the 16 quadwords of the VU1 control registers through the VU1 register window of
 * VU0 data memory (quadword address 0x430) with post-incremented VI loads.
 * Every value lands in one scratch quadword and is discarded.
 */
void xglVu1ReadControlRegisters(void)
{
    AlignedVector4 scratch;
    int i;

    __asm__ __volatile__("ctc2 %0,$vi1" : : "r"(VU1_CONTROL_WINDOW) : "memory");
    for (i = VU1_REGISTER_QUADS - 1; i >= 0; i--) {
        __asm__ __volatile__("vlqi.xyzw $vf2xyzw,($vi1++)" : : : "memory");
        __asm__ __volatile__("sqc2 $vf2,0(%0)" : : "r"(&scratch) : "memory");
    }
}

/*
 * Reads the 16 quadwords of the VU1 integer registers through the VU1 register window of
 * VU0 data memory (quadword address 0x420) with post-incremented VI loads.
 * Every value lands in one scratch quadword and is discarded.
 */
void xglVu1ReadIntegerRegisters(void)
{
    AlignedVector4 scratch;
    int i;

    __asm__ __volatile__("ctc2 %0,$vi1" : : "r"(VU1_INTEGER_WINDOW) : "memory");
    for (i = VU1_REGISTER_QUADS - 1; i >= 0; i--) {
        __asm__ __volatile__("vlqi.xyzw $vf2xyzw,($vi1++)" : : : "memory");
        __asm__ __volatile__("sqc2 $vf2,0(%0)" : : "r"(&scratch) : "memory");
    }
}
