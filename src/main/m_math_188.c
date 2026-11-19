#include "common.h"
#include "shared.h"
#include "main/m_math_188.h"

/*
 * MMathCalcHermite: four-component cubic Hermite/Catmull-Rom evaluation at
 * parameter t. The VU0 macro-mode block builds the four blend weights from t
 * (packed two-per-register across vf2/vf3's x/y lanes), then accumulates
 * point_start*h1 + point_end*h2 + tangent_start*h3 + tangent_end*h4 into vf1
 * and stores it; vf1.w is forced to VF00's w (1.0) by the trailing vmove.w.
 * The prototype was corrected from the stale `void` shared.h declared for it
 * (no caller anywhere reads the return value, so the change is source-visible
 * only) to the `float *` the original object's `daddu v0,a0,zero` lead-in
 * requires, matching every other pointer-returning function of this TU.
 *
 * The parameter goes to VU0 through the library's fixed scratch GPR: $10
 * here (t2), because $8 and $9 would be the fifth and sixth argument
 * registers of this six-argument function.
 */
float *MMathCalcHermite(float *destination, HermiteVector *tangent_first, HermiteVector *tangent_second,
                         HermiteVector *endpoint_first, HermiteVector *endpoint_second, float parameter)
{
    __asm__ __volatile__(
        "mfc1 $10, %0\n"
        "qmtc2 $10, vf1\n"
        "vaddx.xyz vf2, vf0, vf1x\n"
        "vmulx.xyz vf2, vf2, vf1x\n"
        "vmulx.y vf2, vf2, vf1x\n"
        "vaddw.xyzw vf3, vf0, vf0w\n"
        "vaddw.xyzw vf3, vf3, vf0w\n"
        "vmr32.xyzw vf3, vf3\n"
        "vmul.xyz vf3, vf3, vf2\n"
        "vsubz.y vf5, vf3, vf3z\n"
        "vaddw.y vf5, vf5, vf0w\n"
        "vsuby.z vf6, vf3, vf3y\n"
        "vsubx.y vf7, vf2, vf3x\n"
        "vaddx.y vf7, vf7, vf1x\n"
        "vsubx.y vf8, vf2, vf2x\n"
        "lqc2 vf1, 0(%2)\n"
        "lqc2 vf2, 0(%3)\n"
        "lqc2 vf3, 0(%4)\n"
        "lqc2 vf4, 0(%5)\n"
        "vmulay.xyz ACC, vf1, vf7y\n"
        "vmadday.xyz ACC, vf2, vf8y\n"
        "vmadday.xyz ACC, vf3, vf5y\n"
        "vmaddz.xyz vf1, vf4, vf6z\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 0(%1)\n"
        :
        : "f"(parameter), "r"(destination), "r"(tangent_first),
          "r"(tangent_second), "r"(endpoint_first), "r"(endpoint_second)
        : "$10", "memory");
    return destination;
}

void MMathCalcHermitePrm(HermiteVector *tangent_start, HermiteVector *tangent_end,
                          const HermiteVector *point_prev, const HermiteVector *point_start,
                          const HermiteVector *point_end, const HermiteVector *point_next)
{
    __asm__ __volatile__(
        "lqc2 vf1,0(%2)\n\t"
        "lqc2 vf2,0(%3)\n\t"
        "lqc2 vf3,0(%4)\n\t"
        "lqc2 vf4,0(%5)\n\t"
        "viaddi vi5,vi0,8\n\t"
        "vmfir.x vf5x,vi5\n\t"
        "vitof4.x vf5x,vf5x\n\t"
        "vsub.xyz vf6xyz,vf3xyz,vf1xyz\n\t"
        "vsub.xyz vf7xyz,vf4xyz,vf2xyz\n\t"
        "vmulx.xyz vf6xyz,vf6xyz,vf5x\n\t"
        "vmulx.xyz vf7xyz,vf7xyz,vf5x\n\t"
        "sqc2 vf6,0(%0)\n\t"
        "sqc2 vf7,0(%1)"
        :
        : "r"(tangent_start), "r"(tangent_end), "r"(point_prev), "r"(point_start),
          "r"(point_end), "r"(point_next)
        : "memory"
    );
}

/* Float vector times the degree-to-radian factor (no int conversion; the counterpart of MMathDeg2RadVectorI). */
void *MMathDeg2RadVector(void *destination, const Vector4 *source)
{
    __asm__ __volatile__(
        "li.s $f8, 0.017453292\n"
        "mfc1 $8, $f8\n"
        "qmtc2 $8, vf1\n"
        "lqc2 vf2, 0(%1)\n"
        "vmulx.xyz vf2, vf2, vf1x\n"
        "sqc2 vf2, 0(%0)\n"
        : : "r"(destination), "r"(source) : "$8", "$f8", "memory");
    return destination;
}

/* Int vector converted to float, times the degree-to-radian factor. */
void *MMathDeg2RadVectorI(void *destination, const Vector4 *source)
{
    __asm__ __volatile__(
        "li.s $f8, 0.017453292\n"
        "mfc1 $8, $f8\n"
        "qmtc2 $8, vf1\n"
        "lqc2 vf2, 0(%1)\n"
        "vitof0.xyz vf2, vf2\n"
        "vmulx.xyz vf2, vf2, vf1x\n"
        "sqc2 vf2, 0(%0)\n"
        : : "r"(destination), "r"(source) : "$8", "$f8", "memory");
    return destination;
}

/* Float vector times the radian-to-degree factor (no int conversion; the counterpart of MMathRad2DegVectorI). */
void *MMathRad2DegVector(void *destination, const Vector4 *source)
{
    __asm__ __volatile__(
        "li.s $f8, 57.29578\n"
        "mfc1 $8, $f8\n"
        "qmtc2 $8, vf1\n"
        "lqc2 vf2, 0(%1)\n"
        "vmulx.xyz vf2, vf2, vf1x\n"
        "sqc2 vf2, 0(%0)\n"
        : : "r"(destination), "r"(source) : "$8", "$f8", "memory");
    return destination;
}

/* Float vector times the radian-to-degree factor, converted to int. */
void *MMathRad2DegVectorI(void *destination, const Vector4 *source)
{
    __asm__ __volatile__(
        "li.s $f8, 57.29578\n"
        "mfc1 $8, $f8\n"
        "qmtc2 $8, vf1\n"
        "lqc2 vf2, 0(%1)\n"
        "vmulx.xyz vf2, vf2, vf1x\n"
        "vftoi0.xyz vf2, vf2\n"
        "sqc2 vf2, 0(%0)\n"
        : : "r"(destination), "r"(source) : "$8", "$f8", "memory");
    return destination;
}
