#ifndef INCLUDE_MAIN_RUNTIME_ENEPC_H
#define INCLUDE_MAIN_RUNTIME_ENEPC_H

#include "ov01/sb_vm.h"

/* Functions of src/main/runtime_enepc.c that other files call: the natives of the "enepc" script module, listed in
 * enepcNativeTable. */

int enepcSetParams(SbThread *thread);
int enepcSetParamsAll(SbThread *thread);
int enepcMotion(SbThread *thread);
int motionFace(SbThread *thread);
int enepcExtra(SbThread *thread);
int enepcExMotion(SbThread *thread);
int enepcLookAt(SbThread *thread);
int enepcEncount(SbThread *thread);
int Resigne(SbThread *thread);
int setGroup(SbThread *thread);
int enepcSetBattle(SbThread *thread);
int enepcSetSe(SbThread *thread);
int enepcDefaultMotion(SbThread *thread);
int enepcSetTogether(SbThread *thread);
int enepcAlgorithm(SbThread *thread);
int enepcReadyEnepcData(SbThread *thread);
int enepcEnableDrawFlags(SbThread *thread);
int disableDrawFlags(SbThread *thread);

void enepcRegisterNatives(void);

#endif /* INCLUDE_MAIN_RUNTIME_ENEPC_H */
