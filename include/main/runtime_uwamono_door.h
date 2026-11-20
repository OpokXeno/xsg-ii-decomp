#ifndef INCLUDE_MAIN_RUNTIME_UWAMONO_DOOR_H
#define INCLUDE_MAIN_RUNTIME_UWAMONO_DOOR_H

#include "main/runtime_enepc.h"

/* Functions of src/main/runtime_uwamono_door.c that other files call. */

int setCollisionScale(struct SbThread *thread);
int uwamonoSendSignal(struct SbThread *thread);
int uwamonoSetCollisionSize(struct SbThread *thread);
int uwamonoSetDoorAll(struct SbThread *thread);
int uwamonoSetKowaremonoAll(struct SbThread *thread);

void uwamonoRegisterNatives(void);

#endif /* INCLUDE_MAIN_RUNTIME_UWAMONO_DOOR_H */
