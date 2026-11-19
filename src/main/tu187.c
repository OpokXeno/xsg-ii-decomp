#include "common.h"
#include "shared.h"
#include "main/tu187.h"

typedef char *va_list;

#define va_start(ap, last) ((ap) = (va_list)__builtin_next_arg(last) - (8 - __builtin_args_info(2)) * 8)

int sprintf(char *buffer, const char *format, ...);
/* Prints format with the variadic arguments args (libc, next to vsprintf; errorFatal in tu026.c calls it too). */
void func_002AF028(const char *format, va_list args);
static char MOutputDebugBuffer[256];

/* Debug message, info colour ("MDMSG:" in cyan): formats message into MOutputDebugBuffer, then prints
 * that with the variadic arguments. */
void MOutputDebugString(char *message, ...)
{
    va_list ap;
    va_start(ap, message);

    sprintf(MOutputDebugBuffer, "\x1b[1;36mMDMSG:\x1b[m %s\n", message);
    func_002AF028(MOutputDebugBuffer, ap);
}

/* Debug message, warning colour ("MDMSG:" in magenta): formats message into MOutputDebugBuffer, then prints
 * that with the variadic arguments. */
void MOutputDebugStringWarn(char *message, ...)
{
    va_list ap;
    va_start(ap, message);

    sprintf(MOutputDebugBuffer, "\x1b[1;35mMDMSG:\x1b[m %s\n", message);
    func_002AF028(MOutputDebugBuffer, ap);
}

/*
 * Mersenne Twister (MT19937): seeding, then the tempered 32-bit output of genrand_int32.
 * Parameters: state words, twist offset, word halves, tempering masks.
 */
#define MT_STATE_WORDS 624
#define MT_TWIST_OFFSET 397
#define MT_UPPER_MASK 0x80000000
#define MT_LOWER_MASK 0x7FFFFFFF
#define MT_TEMPERING_MASK_B 0x9D2C5680
#define MT_TEMPERING_MASK_C 0xEFC60000

extern int MMathRandomIndex;
extern int MMathRandomLastSeed;
static u32 MMathRandomState[MT_STATE_WORDS];

/* init_genrand: a zero seed means the previous seed plus 0x1571. */
void MMathRandomSeed(int seed)
{
    if (seed == 0)
    {
        seed = MMathRandomLastSeed + 0x1571;
    }
    MMathRandomLastSeed = seed;
    MMathRandomState[0] = seed;
    for (MMathRandomIndex = 1; MMathRandomIndex < MT_STATE_WORDS; MMathRandomIndex++)
    {
        MMathRandomState[MMathRandomIndex] =
            1812433253UL * (MMathRandomState[MMathRandomIndex - 1] ^ (MMathRandomState[MMathRandomIndex - 1] >> 30))
            + MMathRandomIndex;
    }
}

extern u32 MMathRandomMag01[2];

/* genrand_int32: regenerates the 624 words when they are used up, then tempers the next one. */
u32 MMathRandomNext(void)
{
    u32 y;
    int kk;

    if (MMathRandomIndex >= MT_STATE_WORDS)
    {
        if (MMathRandomIndex == MT_STATE_WORDS + 1)
        {
            MMathRandomSeed(0);
        }
        for (kk = 0; kk < MT_STATE_WORDS - MT_TWIST_OFFSET; kk++)
        {
            y = (MMathRandomState[kk] & MT_UPPER_MASK) | (MMathRandomState[kk + 1] & MT_LOWER_MASK);
            MMathRandomState[kk] = MMathRandomState[kk + MT_TWIST_OFFSET] ^ (y >> 1) ^ MMathRandomMag01[y & 1];
        }
        for (; kk < MT_STATE_WORDS - 1; kk++)
        {
            y = (MMathRandomState[kk] & MT_UPPER_MASK) | (MMathRandomState[kk + 1] & MT_LOWER_MASK);
            MMathRandomState[kk] =
                MMathRandomState[kk + (MT_TWIST_OFFSET - MT_STATE_WORDS)] ^ (y >> 1) ^ MMathRandomMag01[y & 1];
        }
        y = (MMathRandomState[MT_STATE_WORDS - 1] & MT_UPPER_MASK) | (MMathRandomState[0] & MT_LOWER_MASK);
        MMathRandomState[MT_STATE_WORDS - 1] =
            MMathRandomState[MT_TWIST_OFFSET - 1] ^ (y >> 1) ^ MMathRandomMag01[y & 1];
        MMathRandomIndex = 0;
    }
    y = MMathRandomState[MMathRandomIndex++];
    y ^= y >> 11;
    y ^= (y << 7) & MT_TEMPERING_MASK_B;
    y ^= (y << 15) & MT_TEMPERING_MASK_C;
    y ^= y >> 18;
    return y;
}

/* A random integer in [0, 2^31). */
u32 MMathMakeRandomInt(void)
{
    return MMathRandomNext() >> 1;
}

/* A random float in [0, 1): the 32-bit output scaled by 2^-32. */
f32 MMathMakeRandom(void)
{
    u32 bits;

    bits = MMathRandomNext();
    return (f32) bits * 2.3283064e-10f;
}

/* A random float in [-1, 1): the 32-bit output scaled by 2^-31, minus 1. */
f32 MMathMakeRandomSigned(void)
{
    u32 bits;

    bits = MMathRandomNext();
    return ((f32) bits * 4.656613e-10f) - 1.0f;
}

/* A random angle in [0, 2*pi): the 32-bit output scaled by 2*pi / 2^32. */
f32 MMathMakeRandom2PI(void)
{
    u32 bits;

    bits = MMathRandomNext();
    return (f32) bits * 1.462918e-09f;
}

/* A random angle in [-pi, pi). */
f32 MMathMakeRandomPI(void)
{
    u32 bits;

    bits = MMathRandomNext();
    return ((f32) bits * 1.462918e-09f) - 3.1415927f;
}

float fmodf(float, float);

/* Shortest signed difference toAngle - fromAngle, wrapped into [-pi, pi]. */
f32 MMathCalcRotNear(f32 fromAngle, f32 toAngle)
{
    f32 twoPi;
    f32 result;

    twoPi = 6.2831850f;
    result = fmodf(toAngle - fromAngle, twoPi);
    if (3.14159274f < __builtin_fabsf(result))
    {
        if (result < 0.0f)
        {
            result = result + twoPi;
        }
        else
        {
            result = result - twoPi;
        }
    }
    return result;
}


/* Difference toAngle - fromAngle wrapped into the other half-turn (|difference| >= pi). */
f32 MMathCalcRotFar(f32 fromAngle, f32 toAngle)
{
    f32 twoPi;
    f32 result;

    twoPi = 6.2831850f;
    result = fmodf(toAngle - fromAngle, twoPi);
    if (__builtin_fabsf(result) < 3.14159274f)
    {
        if (result < 0.0f)
        {
            result = result + twoPi;
        }
        else
        {
            result = result - twoPi;
        }
    }
    return result;
}

/* Unreferenced in this TU: two words, 0x319 and 0. */
const int D_0068C728[] = { 0x319, 0 };

/* Small data of the Mersenne Twister: the index starts past the end of the state (MT_STATE_WORDS + 1). */
int MMathRandomIndex = 625;
int MMathRandomLastSeed = 0;
u32 MMathRandomMag01[2] = { 0x00000000, 0x9908B0DF };
