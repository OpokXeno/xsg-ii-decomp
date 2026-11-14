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

/* A vector the original keeps only 8-aligned but copies with ld/sd. */
typedef Vector4 DoublewordAlignedVector4 __attribute__((aligned(8)));

/* Only where the original genuinely accesses the vector through a union. */
typedef union DoublewordVector4 {
    Vector4 vector;
    u64 words[2];
} DoublewordVector4;

/* Row-major 4x4 float matrix. */
typedef float Matrix4[4][4];


/* The end of an INTC/DMAC handler (SDK eekernel.h ExitHandler): sync, then re-enable interrupts. */
#define ExitHandler() __asm__ volatile("sync\n\tei" ::: "memory")

#endif /* INCLUDE_SHARED_H */
