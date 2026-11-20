#ifndef INCLUDE_MAIN_RUNTIME_MENU_H
#define INCLUDE_MAIN_RUNTIME_MENU_H

#include "ov01/sb_vm.h"

/* Functions of src/main/runtime_menu.c that other files call: the natives of the "menu" script module, listed in
 * menuNativeTable. */

int ptInit(SbThread *context);
int ptClear(SbThread *context);

/* The party member natives. */

int ptJoin(SbThread *context);
int ptLeave(SbThread *context);
int ptChangeMode(SbThread *context);
int ptSet(SbThread *context);

/* The pilot, leader and tent natives. */

int ptSetPilot(SbThread *context);
int ptGetLeader(SbThread *context);
int umnReturn(SbThread *context);
int ptSetArray(SbThread *context);
int ptTent(SbThread *context);

void menuRegisterNatives(void);

#endif /* INCLUDE_MAIN_RUNTIME_MENU_H */
