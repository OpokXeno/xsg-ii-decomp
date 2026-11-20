#ifndef INCLUDE_MAIN_GS_MAKE_TEX0_H
#define INCLUDE_MAIN_GS_MAKE_TEX0_H

/* Functions of src/main/gs_make_tex0.c that other files call. */

/* Per-channel linear blend of two RGBA colors by fraction t. The operands are 64-bit (callers widen the colors: dsll32/dsrl32 before the call; int parameters compile different bytes in debt_box.c), and the result is a 64-bit value the caller sign-extends from 32 bits (dsll32/dsra32). */
long long xglColorLerp(long long from, long long to, float t);

/* Steps an angle in degrees back into [0, 360] by at most one turn. */
float xglWrapDegrees(float degrees);

/* GS TEX0 of a 512x512 PSMCT32 frame page: buffer 0 the display page, 1 the copy page, 2 the frame page. */
unsigned long long gsMakeTex0(int buffer, int tex_cc);

#endif /* INCLUDE_MAIN_GS_MAKE_TEX0_H */
