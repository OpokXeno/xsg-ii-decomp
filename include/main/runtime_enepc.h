#ifndef INCLUDE_MAIN_RUNTIME_ENEPC_H
#define INCLUDE_MAIN_RUNTIME_ENEPC_H

#include "ov01/sb_vm.h"

/* Functions of src/main/runtime_enepc.c that other files call. */

void runtimeRegisterNatives(void);

/* Script natives of the "enepc" owner table (runtime_uwamono_door.c). */
int enepcSetParams(SbThread *thread);
int enepcSetParamsAll(SbThread *thread);
int enepcMotion(SbThread *thread);

#endif /* INCLUDE_MAIN_RUNTIME_ENEPC_H */
