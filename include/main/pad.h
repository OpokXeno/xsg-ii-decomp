#ifndef INCLUDE_MAIN_PAD_H
#define INCLUDE_MAIN_PAD_H

#include "shared.h"

#include "main/pad_buttons.h"

/* A 128-bit store or load of the +0x30 analog group (a C store of a zero is `por $4,$0,$0; sq $4,0x30(pad)`;
 * QuadwordZero would drop the por and change the register allocation of xglPadRead). */
typedef unsigned int PadQuadword __attribute__((mode(TI)));

/* A port record of PadData (0x80 bytes: xglPadRead scales the port index by 128; the object is defined
 * in src/main/xgl_dma.c). xglPadRead's probe state clears each state group with one store of its width:
 * mode/actuatorCount, the button halfwords, the analog group, the pressures, the actuators and the axes. */
typedef struct PadRecord {
    unsigned char packet[0x20];       /* +0x00: raw controller packet */
    union {
        struct {
            unsigned char mode;          /* +0x20 */
            unsigned char actuatorCount; /* +0x21 */
        };
        unsigned short modeState;
    };
    unsigned short modeId;                   /* +0x22 */
    unsigned char unmodeled_24[2];
    unsigned char connectionState;    /* +0x26 */
    unsigned char setupState;         /* +0x27: PAD_SETUP_* of xgl_pad.c */
    union {
        struct {
            unsigned short held;     /* +0x28 */
            unsigned short pressed;  /* +0x2A */
            unsigned short repeated; /* +0x2C */
            unsigned short released; /* +0x2E */
        };
        /* The union gives every access to these halfwords alias set 0 (gcc 2.96); yajima_test.c keeps its
           own union-free view because of that. */
        unsigned long long heldAndPressed; /* +0x28: the button halfwords read as one doubleword (held and pressed
                                              are its low two halfwords) */
    };
    union {
        struct {
            unsigned short directionHeld;     /* +0x30 */
            unsigned short directionPressed;  /* +0x32 */
            unsigned short directionRepeated; /* +0x34 */
            unsigned short directionReleased; /* +0x36 */
            signed char position[4];          /* +0x38 */
            /* +0x3C/+0x3E: d-pad pressure (packet bytes 8-11, right minus left and
               down minus up; xglPadRead). */
            short horizontalPressure;         /* +0x3C */
            short verticalPressure;           /* +0x3E */
        };
        unsigned long long directionState;    /* +0x30: the four direction halfwords as one doubleword */
        PadQuadword analogState;              /* +0x30: the whole analog group */
    };
    union {
        unsigned char pressure[8];     /* +0x40 */
        unsigned long long pressureState;
    };
    signed char repeatDelay;           /* +0x48 */
    signed char repeatInterval;        /* +0x49 */
    unsigned short repeatMask;         /* +0x4A */
    signed char repeatWait;            /* +0x4C */
    signed char repeatCount;           /* +0x4D */
    unsigned char horizontalDeadZone;  /* +0x4E */
    unsigned char verticalDeadZone;    /* +0x4F */
    union {
        struct {
            unsigned char actuator[6];     /* +0x50: small motor on byte 0, large motor on byte 1 */
            unsigned char actuatorMode;    /* +0x56: the actuator setting (SaveData +0x3E); nonzero clears the actuators */
            unsigned char unmodeled_57;
        };
        struct {
            unsigned int actuatorLow;      /* actuator[0..3] */
            unsigned short actuatorHigh;   /* actuator[4..5] */
        };
    };
    unsigned char buttonMap[8];        /* +0x58 */
    unsigned char axisDeadZone[4];     /* +0x60 */
    union {
        signed char axis[4];           /* +0x64 */
        struct {
            signed char rightX;        /* +0x64 */
            signed char rightY;        /* +0x65 */
            signed char leftX;         /* +0x66 */
            signed char leftY;         /* +0x67 */
        };
        unsigned int axisState;
    };
    unsigned char unmodeled_68[0x18];
} PadRecord;

extern PadRecord PadData[2];

#endif /* INCLUDE_MAIN_PAD_H */
