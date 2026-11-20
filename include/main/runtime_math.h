#ifndef INCLUDE_MAIN_RUNTIME_MATH_H
#define INCLUDE_MAIN_RUNTIME_MATH_H

#include "ov01/sb_vm.h"

/* Script natives of src/main/runtime_math.c (the "math" owner table in runtime_pt_init.c). */

int mathAtan(SbThread *thread);
int mathSin(SbThread *thread);
int mathCos(SbThread *thread);
int mathRandom(SbThread *thread);

#endif /* INCLUDE_MAIN_RUNTIME_MATH_H */
