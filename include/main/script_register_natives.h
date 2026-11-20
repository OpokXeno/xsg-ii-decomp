#ifndef INCLUDE_MAIN_SCRIPT_REGISTER_NATIVES_H
#define INCLUDE_MAIN_SCRIPT_REGISTER_NATIVES_H

#include "ov01/sb_vm.h"

struct UwamonoRecord;

/* Functions of src/main/script_register_natives.c that other files call. The native owner tables it registers
 * are SbNativeMethod arrays (ov01/sb_vm.h). */

void talkStateReset(void);
void scriptRegisterNatives(void);

/* Script native slotWinGetResult (the "talk" table); the second parameter is the status word. */
int slotWinGetResult(SbThread *thread, int *failure_flag);

/* The talk state of the map object a message belongs to (mapActiveObject, talkResult). */
void mapObjectBeginMessage(struct UwamonoRecord *object);
void mapObjectFaceActive(struct UwamonoRecord *object);
void mapObjectSetActive(struct UwamonoRecord *owner, struct UwamonoRecord *value);
int mapObjectClearTalkFlags(struct UwamonoRecord *object);
void talkSetResult(int value);

#endif /* INCLUDE_MAIN_SCRIPT_REGISTER_NATIVES_H */
