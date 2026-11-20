#ifndef INCLUDE_MAIN_RUNTIME_MATH_H
#define INCLUDE_MAIN_RUNTIME_MATH_H

#include "ov01/sb_vm.h"

/* Functions of src/main/runtime_math.c that other files call: the natives of the "math" script module, listed in
 * mathNativeTable. */

int mathAtan(SbThread *thread);
int mathSin(SbThread *thread);
int mathCos(SbThread *thread);
int mathRandom(SbThread *thread);

void mathRegisterNatives(void);

#endif /* INCLUDE_MAIN_RUNTIME_MATH_H */
