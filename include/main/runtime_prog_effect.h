#ifndef INCLUDE_MAIN_RUNTIME_PROG_EFFECT_H
#define INCLUDE_MAIN_RUNTIME_PROG_EFFECT_H

#include "ov01/sb_vm.h"

/* Script natives of src/main/runtime_prog_effect.c (the "runtime" owner table in runtime_enepc.c); call is the
 * script call. */

int changeRobo(SbThread *call);
int runtimeFade(SbThread *call);
int runtimeGetFlag(SbThread *call);
int runtimeJumpCf(SbThread *call);
int runtimeMapChange(SbThread *call);
int runtimeMessage(SbThread *call);
int runtimeProgEffect(SbThread *call);
int runtimeSetElevator(SbThread *call);
int runtimeSetFlag(SbThread *call);
int runtimeSetItai(SbThread *call);
int runtimeSetMapFog(SbThread *call);
int runtimeSetMapLight(SbThread *call);
int runtimeSystem(SbThread *call);
int setMapColor(SbThread *call);

void uwamonoSetTransition(signed char mode, int state, int stateArg, int timer);

#endif /* INCLUDE_MAIN_RUNTIME_PROG_EFFECT_H */
