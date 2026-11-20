#ifndef INCLUDE_MAIN_RUNTIME_PT_PILOT_H
#define INCLUDE_MAIN_RUNTIME_PT_PILOT_H

#include "ov01/sb_vm.h"

/* Script natives of src/main/runtime_pt_pilot.c (the pilot, leader and tent natives of the menu module). */

int ptSetPilot(SbThread *context);
int ptGetLeader(SbThread *context);
int umnReturn(SbThread *context);
int ptSetArray(SbThread *context);
int ptTent(SbThread *context);

#endif /* INCLUDE_MAIN_RUNTIME_PT_PILOT_H */
