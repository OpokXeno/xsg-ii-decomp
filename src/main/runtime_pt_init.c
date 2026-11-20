#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "shared.h"
#include "main/runtime_pt_init.h"
#include "main/pt_member.h"
#include "main/menu_selection.h"
#include "main/runtime_math.h"


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


int ptInit(SbThread *context)
{
    if (VM_PLUGIN_CNT_GET(context) != 0)
    {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    ptStateInit();
    return 1;
}


/* Script native "ptClear": takes no arguments; calls ptResetSlots and returns 1. */
int ptClear(SbThread *context)
{
    if (VM_PLUGIN_CNT_GET(context) != 0)
    {
        sbSlotJumpRequest(context, 2);
        return 0;
    }
    ptResetSlots();
    return 1;
}


