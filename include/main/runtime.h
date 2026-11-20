#ifndef INCLUDE_MAIN_RUNTIME_H
#define INCLUDE_MAIN_RUNTIME_H

#include "shared.h"
#include "ov01/sb_vm.h"

/* Functions of src/main/runtime.c that other files call: the natives of the "runtime" script module, listed in
 * runtimeNativeTable, and the helpers they share with other files. */

int uwamonoCreateRecord(int key, int kind, int entry, int fourth, int fifth);
int runtimeSetNumber(struct SbThread *thread);
int runtimeSetLocater(struct SbThread *thread);
int runtimeSetLastDan(void);
int runtimeSetPlayer(struct SbThread *thread);

int runtimeItem(struct SbThread *thread);
int runtimeTimeAttack(struct SbThread *thread);
int runtimeGetAnalogR(struct SbThread *thread);

/* call is the script call. */

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

/* mpeg2 plays a movie, adx gives slot 0 an ADX file, adxLoad does the same paused and adxPlay unpauses slot 0. */
int mpeg2(struct SbThread *call);
int adx(struct SbThread *call);
int adxLoad(struct SbThread *call);
int adxPlay(void);

/* Sound effects ("se", "sePos", "seVol"), background music sound effects ("bgmse", "bgmsePos") and the BGM control
 * natives. */
int runtimeSe(struct SbThread *call);
int seVol(struct SbThread *call);
int sePos(struct SbThread *call);
int bgmSe(struct SbThread *call);
int bgmSePos(struct SbThread *call);
int bgmFade(struct SbThread *call);
int nextBgm(struct SbThread *call);
int runtimeBgm(struct SbThread *call);

void runtimeRegisterNatives(void);

#endif /* INCLUDE_MAIN_RUNTIME_H */
