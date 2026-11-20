#ifndef UNALIGNED_H
#define UNALIGNED_H

/*
 * The original's unaligned word copy (the GAS ulw/usw macro idiom): four
 * bytes moved through the $9 (t1) scratch with lwl/lwr + swl/swr, between two
 * addresses of any alignment. The SB VM of ov01 (sb_vm.c/sb_natives.c) copies the
 * 32-bit operands of its halfword-aligned code stream with it. It is inline
 * asm in the original too, none of which a C copy reproduces:
 *   - the scratch is $9 for every copy, also for two copies in one function;
 *     a C copy of a misaligned word (__builtin_memcpy, a struct of bytes) goes
 *     through movstrsi_internal, whose scratch is the lowest free reload
 *     register ($2);
 *   - the four instructions stay together (wordCopyUnaligned ends `jr $31; nop`),
 *     where gcc splits off the last swr to fill a delay slot;
 *   - memory values are reloaded after the copy ("memory"), as sbLinkImage
 *     reloads the operand it just copied.
 *
 *   UnalignedWordCopy(d, s)  copies the 4 bytes at s to d
 */

#define UnalignedWordCopy(destination, source) \
    __asm__ __volatile__( \
        "ulw $9, %1\n" \
        "usw $9, %0\n" \
        : "=m" (*(int *) (destination)) \
        : "m" (*(const int *) (source)) \
        : "$9", "memory")

#endif
