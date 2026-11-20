#ifndef INCLUDE_OV01_SB_PUSH_CONST_H
#define INCLUDE_OV01_SB_PUSH_CONST_H

#include "ov01/sb_vm.h"

/* Opcode handlers 0x26..0x2D of src/ov01/sb_push_const.c: each pushes a constant on the thread's stack. */
int sbPushConst0(SbThread *thread, int opcode);
int sbPushConst0Op27(SbThread *thread, int opcode);
int sbPushConst1(SbThread *thread, int opcode);
int sbPushConst2(SbThread *thread, int opcode);
int sbPushConst3(SbThread *thread, int opcode);
int sbPushConst4(SbThread *thread, int opcode);
int sbPushConst5(SbThread *thread, int opcode);
int sbPushConst6(SbThread *thread, int opcode);

#endif /* INCLUDE_OV01_SB_PUSH_CONST_H */
