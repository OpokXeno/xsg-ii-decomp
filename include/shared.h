#ifndef INCLUDE_SHARED_H
#define INCLUDE_SHARED_H

typedef unsigned char u8;

typedef unsigned short u16;

typedef unsigned int u32;

typedef unsigned long long u64;

typedef signed short s16;

typedef float f32;

typedef struct Vector4 {
    float x;
    float y;
    float z;
    float w;
} Vector4;

/* Quadword-aligned like sceVu0FVECTOR: whole-vector copies compile to ld/sd. */
typedef Vector4 AlignedVector4 __attribute__((aligned(16)));

/* A vector viewed as a Vector4 or as its four floats by index. The original reaches such vectors through a union: the
 * stores have alias set 0, so every store reloads the pointers (the levers named where it is used). */
typedef union Vector4Elements {
    Vector4 vector;
    float component[4];
} Vector4Elements;

/* The same view of a quadword-aligned vector. */
typedef union AlignedVector4Elements {
    AlignedVector4 vector;
    float component[4];
} AlignedVector4Elements;

/* Four floats the original indexes one by one and copies by value as two doublewords (ld/sd). */
typedef union Vector4Doublewords {
    float component[4];
    u64 words[2];
} Vector4Doublewords;

/* A vector the original keeps only 8-aligned but copies with ld/sd. */
typedef Vector4 DoublewordAlignedVector4 __attribute__((aligned(8)));

/* Only where the original genuinely accesses the vector through a union. */
typedef union DoublewordVector4 {
    Vector4 vector;
    u64 words[2];
} DoublewordVector4;

/* Row-major 4x4 float matrix. */
typedef float Matrix4[4][4];


/* The EE scratchpad (16 KB), where the GS queue and talk packets are built before they are sent. */
#define SCRATCHPAD_BASE 0x70000000

/* The end of an INTC/DMAC handler (SDK eekernel.h ExitHandler): sync, then re-enable interrupts. */
#define ExitHandler() __asm__ volatile("sync\n\tei" ::: "memory")

#endif /* INCLUDE_SHARED_H */
