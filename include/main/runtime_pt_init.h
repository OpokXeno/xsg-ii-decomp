#ifndef INCLUDE_MAIN_RUNTIME_PT_INIT_H
#define INCLUDE_MAIN_RUNTIME_PT_INIT_H

#include "ov01/sb_vm.h"

/* Functions of src/main/runtime_pt_init.c that other files call. */

void mathRegisterNatives(void);

int ptInit(SbThread *context);
int ptClear(SbThread *context);

#endif /* INCLUDE_MAIN_RUNTIME_PT_INIT_H */
