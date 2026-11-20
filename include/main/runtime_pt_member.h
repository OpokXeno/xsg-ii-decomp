#ifndef INCLUDE_MAIN_RUNTIME_PT_MEMBER_H
#define INCLUDE_MAIN_RUNTIME_PT_MEMBER_H

#include "ov01/sb_vm.h"

/* Script natives of src/main/runtime_pt_member.c (the party member natives of the menu module). */

int ptJoin(SbThread *context);
int ptLeave(SbThread *context);
int ptChangeMode(SbThread *context);
int ptSet(SbThread *context);

#endif /* INCLUDE_MAIN_RUNTIME_PT_MEMBER_H */
