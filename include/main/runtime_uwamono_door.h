#ifndef INCLUDE_MAIN_RUNTIME_UWAMONO_DOOR_H
#define INCLUDE_MAIN_RUNTIME_UWAMONO_DOOR_H

struct SbThread;

/* Functions of src/main/runtime_uwamono_door.c that other files call. */

void enepcRegisterNatives(void);
int setCollisionScale(struct SbThread *thread);
int uwamonoSendSignal(struct SbThread *thread);
int uwamonoSetCollisionSize(struct SbThread *thread);
int uwamonoSetDoorAll(struct SbThread *thread);
int uwamonoSetKowaremonoAll(struct SbThread *thread);

#endif /* INCLUDE_MAIN_RUNTIME_UWAMONO_DOOR_H */
