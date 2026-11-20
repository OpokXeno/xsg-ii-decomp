#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/get.h"
#include "main/runtime_math.h"
#include "shared.h"

/* Script native "atan": two centi-unit arguments, atan2f of them scaled by 10000. */
extern float sinf(float angle);
extern float cosf(float angle);
extern float atan2f(float y, float x);

int mathAtan(SbThread *context)
{
    float centiScale;
    float y;
    int x;

    if (VM_PLUGIN_CNT_GET(context) != 2) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    centiScale = 100.0f;
    y = (float)vmArgGet(context, 1) / centiScale;
    x = vmArgGet(context, 2);
    return (int)(atan2f(y, (float)x / centiScale) * 10000.0f);
}

/* Script native "math.sin": one argument in centi-degrees; returns sin(angle) * 10000 (float literal). */
int mathSin(SbThread *context)
{
    int angle_centideg;

    if (VM_PLUGIN_CNT_GET(context) != 1) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    angle_centideg = vmArgGet(context, 1);
    return (int)(sinf((((float)angle_centideg / 100.0f) / 180.0f) * 3.14159274f) * 10000.0f);
}

/* Script native "math.cos": one argument in centi-degrees; returns cos(angle) * 10000 (float literal). */
int mathCos(SbThread *context)
{
    int angle_centideg;

    if (VM_PLUGIN_CNT_GET(context) != 1) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    angle_centideg = vmArgGet(context, 1);
    return (int)(cosf((((float)angle_centideg / 100.0f) / 180.0f) * 3.14159274f) * 10000.0f);
}

/* Script native "math.random": two arguments, min and max, passed to Get_Rnd. */
int mathRandom(SbThread *context)
{
    int min;
    int max;

    if (VM_PLUGIN_CNT_GET(context) != 2) {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    min = vmArgGet(context, 1);
    max = vmArgGet(context, 2);
    return Get_Rnd(min, max);
}


/* Native table: (name, function) pairs, then a null entry. Passed to sbNativeOwnerAdd by mathRegisterNatives; its names come before "math" in .sdata. */
SbNativeMethod mathNativeTable[5] = {
    { "atan", mathAtan },
    { "sin", mathSin },
    { "cos", mathCos },
    { "random", mathRandom },
    { 0, 0 },
};

/* Registers the math native table mathNativeTable (atan, sin, cos, random) under the module name "math". */
void mathRegisterNatives(void)
{
    sbNativeOwnerAdd("math", mathNativeTable);
}
