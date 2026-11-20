#ifndef INCLUDE_MAIN_RUNTIME_MOTION_FACE_H
#define INCLUDE_MAIN_RUNTIME_MOTION_FACE_H

#include "ov01/sb_vm.h"

/* Natives of the enepc module defined in src/main/runtime_motion_face.c (listed in runtime_uwamono_door.c's table). */

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

#endif /* INCLUDE_MAIN_RUNTIME_MOTION_FACE_H */
