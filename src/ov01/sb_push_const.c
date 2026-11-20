/*
 * Each function is the handler of one SB bytecode opcode in the dispatch table
 * sbOpHandlers (entries 0x26..0x2D; the dispatcher is sbStep in sb_vm.c).
 * An opcode takes no operand. It pushes a constant on the VM stack
 * (sbStackPush) and moves the stream position past the opcode. The position
 * is the first word of the VM record, which the dispatcher fetches from; the
 * opcode advances it by its length (sbOpLength; one 16-bit unit here).
 */
#include "common.h"
#include "ov01/sb_push_const.h"

/* Opcode 0x26: pushes 0. */
int sbPushConst0(SbThread *thread, int opcode) {
    unsigned short opcode16;

    opcode16 = (unsigned short) opcode;
    sbStackPush(thread, 0);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x27: pushes 0, the same body as opcode 0x26. */
int sbPushConst0Op27(SbThread *thread, int opcode) {
    unsigned short opcode16;

    opcode16 = (unsigned short) opcode;
    sbStackPush(thread, 0);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x28: pushes 1. */
int sbPushConst1(SbThread *thread, int opcode) {
    unsigned short opcode16;

    opcode16 = (unsigned short) opcode;
    sbStackPush(thread, 1);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x29: pushes 2. */
int sbPushConst2(SbThread *thread, int opcode) {
    unsigned short opcode16;

    opcode16 = (unsigned short) opcode;
    sbStackPush(thread, 2);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x2A: pushes 3. */
int sbPushConst3(SbThread *thread, int opcode) {
    unsigned short opcode16;

    opcode16 = (unsigned short) opcode;
    sbStackPush(thread, 3);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x2B: pushes 4. */
int sbPushConst4(SbThread *thread, int opcode) {
    unsigned short opcode16;

    opcode16 = (unsigned short) opcode;
    sbStackPush(thread, 4);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x2C: pushes 5. */
int sbPushConst5(SbThread *thread, int opcode) {
    unsigned short opcode16;

    opcode16 = (unsigned short) opcode;
    sbStackPush(thread, 5);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}

/* Opcode 0x2D: pushes 6. */
int sbPushConst6(SbThread *thread, int opcode) {
    unsigned short opcode16;

    opcode16 = (unsigned short) opcode;
    sbStackPush(thread, 6);
    thread->pc += sbOpLength(opcode16, 0);
    return 0;
}
