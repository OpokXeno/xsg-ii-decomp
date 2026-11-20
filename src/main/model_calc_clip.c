#include "common.h"

#include "shared.h"
#include "main/model_calc_clip.h"

/*
 * Transforms in by the current matrix (vf27..vf30) and writes it to out with w
 * replaced by the fog value: fog.z + fog.w * (1 / w), clamped to [fog.x, fog.y].
 */
void _CurRotTransPersFog(Vector4 *out, const Vector4 *in, const Vector4 *fog)
{
    __asm__ __volatile__(
        "lqc2 vf31, 0(%1)\n\t"
        "lqc2 vf4, 0(%2)\n\t"
        "vmulax.xyzw ACC, vf27, vf31x\n\t"
        "vmadday.xyzw ACC, vf28, vf31y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf31z\n\t"
        "vmaddw.xyzw vf31, vf30, vf0w\n\t"
        "vdiv Q, vf0w, vf31w\n\t"
        "vmulz.w vf1, vf0, vf4z\n\t"
        "vwaitq\n\t"
        "vmula.w ACC, vf1, vf0\n\t"
        "vmaddq.w vf31, vf4, Q\n\t"
        "vmaxx.w vf31, vf31, vf4x\n\t"
        "vminiy.w vf31, vf31, vf4y\n\t"
        "sqc2 vf31, 0(%0)"
        :
        : "r"(out), "r"(in), "r"(fog)
        : "$2", "memory");
}

int _CurRotTransPersClipFloat(Vector4 *out, const Vector4 *in)
{
    register int clipFlags;

    __asm__ __volatile__(
        "ctc2 $0, $vi18\n\t"
        "lqc2 vf31, 0(%2)\n\t"
        "vmulax.xyzw ACC, vf27, vf31x\n\t"
        "vmadday.xyzw ACC, vf28, vf31y\n\t"
        "vmaddaz.xyzw ACC, vf29, vf31z\n\t"
        "vmaddw.xyzw vf31, vf30, vf0w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vclipw.xyz vf31, vf31w\n\t"
        "vdiv Q, vf0w, vf31w\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf31, vf31, Q\n\t"
        "vmulq.w vf31w, vf0w, Q\n\t"
        "vmulaw.xyz ACC, vf26, vf0w\n\t"
        "vmadd.xyz vf31, vf31, vf25\n\t"
        "sqc2 vf31, 0(%1)\n\t"
        "cfc2 $2, $vi18\n\t"
        "nop\n\t"
        "andi $2, $2, 0x3f\n\t"
        "move %0, $2"
        : "=r"(clipFlags)
        : "r"(out), "r"(in)
        : "$2", "memory");
    return clipFlags;
}

/* _ModelCalcClipInitMat1: loads the clip matrix pair (eight rows at clip) into vf24..vf31 and the matrix into vf20..vf23. */
void _ModelCalcClipInitMat1(Matrix4 clip, Matrix4 matrix)
{
    __asm__ __volatile__(
        "lqc2 vf24, 0(%0)\n\t"
        "lqc2 vf25, 16(%0)\n\t"
        "lqc2 vf26, 32(%0)\n\t"
        "lqc2 vf27, 48(%0)\n\t"
        "lqc2 vf28, 64(%0)\n\t"
        "lqc2 vf29, 80(%0)\n\t"
        "lqc2 vf30, 96(%0)\n\t"
        "lqc2 vf31, 112(%0)\n\t"
        "lqc2 vf20, 0(%1)\n\t"
        "lqc2 vf21, 16(%1)\n\t"
        "lqc2 vf22, 32(%1)\n\t"
        "lqc2 vf23, 48(%1)"
        :
        : "r"(clip), "r"(matrix)
        : "memory");
}

int _ModelCalcClipMat1(const Vector4 *model)
{
    register int clipFlags;

    __asm__ __volatile__(
        "lqc2 vf10, 0(%1)\n\t"
        "vaddw.x vf2x, vf0x, vf10w\n\t"
        "vmulax.xyz ACC, vf20, vf10x\n\t"
        "vmadday.xyz ACC, vf21, vf10y\n\t"
        "vmaddaz.xyz ACC, vf22, vf10z\n\t"
        "vmaddw.xyz vf10, vf23, vf0w\n\t"
        "qmfc2 $2, vf2\n\t"
        "mtc1 $2, $f1\n\t"
        "vmulax.xyzw ACC, vf28, vf10x\n\t"
        "vmadday.xyzw ACC, vf29, vf10y\n\t"
        "vmaddaz.xyzw ACC, vf30, vf10z\n\t"
        "vmaddw.xyzw vf9, vf31, vf0w\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "vmulax.xyzw ACC, vf24, vf10x\n\t"
        "vmadday.xyzw ACC, vf25, vf10y\n\t"
        "vmaddaz.xyzw ACC, vf26, vf10z\n\t"
        "vmaddw.xyzw vf9, vf27, vf0w\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "sub $2, $2, $2\n\t"
        "addi $2, $2, 1\n\t"
        "j 2f\n\t"
        "1:\n\t"
        "sub $2, $2, $2\n\t"
        "2:\n\t"
        "move %0, $2"
        : "=r"(clipFlags)
        : "r"(model)
        : "$2", "$f0", "$f1", "memory");
    return clipFlags;
}

int _ModelCalcClipMat1Xy(const Vector4 *model)
{
    register int clipFlags;

    __asm__ __volatile__(
        "lqc2 vf10, 0(%1)\n\t"
        "vsubw.x vf2x, vf0x, vf10w\n\t"
        "vmulax.xyz ACC, vf20, vf10x\n\t"
        "vmadday.xyz ACC, vf21, vf10y\n\t"
        "vmaddaz.xyz ACC, vf22, vf10z\n\t"
        "vmaddw.xyz vf10, vf23, vf0w\n\t"
        "qmfc2 $2, vf2\n\t"
        "mtc1 $2, $f1\n\t"
        "vmulax.xyzw ACC, vf28, vf10x\n\t"
        "vmadday.xyzw ACC, vf29, vf10y\n\t"
        "vmaddaz.xyzw ACC, vf30, vf10z\n\t"
        "vmaddw.xyzw vf9, vf31, vf0w\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "vmulax.xyzw ACC, vf24, vf10x\n\t"
        "vmadday.xyzw ACC, vf25, vf10y\n\t"
        "vmaddaz.xyzw ACC, vf26, vf10z\n\t"
        "vmaddw.xyzw vf9, vf27, vf0w\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $2, vf9\n\t"
        "mtc1 $2, $f0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "bc1f 1f\n\t"
        "sub $2, $2, $2\n\t"
        "addi $2, $2, 1\n\t"
        "j 2f\n\t"
        "1:\n\t"
        "sub $2, $2, $2\n\t"
        "2:\n\t"
        "move %0, $2"
        : "=r"(clipFlags)
        : "r"(model)
        : "$2", "$f0", "$f1", "memory");
    return clipFlags;
}
