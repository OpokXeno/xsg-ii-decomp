#ifndef INCLUDE_MAIN_GS_DISPLAY_H
#define INCLUDE_MAIN_GS_DISPLAY_H

#include "shared.h"

/* The GS display registers (PMODE, DISPFB, DISPLAY) as the display-environment block DispEnv (xgl_render.h) holds them. */

/* GS PMODE register: display circuit enables and the alpha blend value. */
typedef struct GsPmode {
    unsigned int en1 : 1;
    unsigned int en2 : 1;
    unsigned int crtmd : 3;
    unsigned int mmod : 1;
    unsigned int amod : 1;
    unsigned int slbg : 1;
    unsigned int alp : 8;
    unsigned int unmodeled_16 : 16;
    unsigned int unmodeled_32 : 32;
} GsPmode;

typedef struct GsDispfb {
    unsigned int fbp : 9;
    unsigned int fbw : 6;
    unsigned int psm : 5;
    unsigned int unmodeled_20 : 12;
    unsigned int dbx : 11;
    unsigned int dby : 11;
    unsigned int unmodeled_54 : 10;
} GsDispfb;

typedef struct GsDisplay {
    unsigned int dx : 12;
    unsigned int dy : 11;
    unsigned int magh : 4;
    unsigned int magv : 2;
    unsigned int unmodeled_29 : 3;
    unsigned int dw : 12;
    unsigned int dh : 11;
    unsigned int unmodeled_55 : 9;
} GsDisplay;

typedef union GsDispfbRegister {
    u64 value;
    GsDispfb field;
} GsDispfbRegister;

typedef union GsDisplayRegister {
    u64 value;
    GsDisplay field;
} GsDisplayRegister;

/* GS EXTDATA register: the sampling window of the external video input (EXTBUF is a plain 64-bit word). */
typedef struct GsExtdata {
    unsigned int sx : 12;
    unsigned int sy : 11;
    unsigned int smph : 4;
    unsigned int smpv : 2;
    unsigned int unmodeled_29 : 3;
    unsigned int ww : 12;
    unsigned int wh : 11;
    unsigned int unmodeled_55 : 9;
} GsExtdata;

typedef union GsExtdataRegister {
    u64 value;
    GsExtdata field;
} GsExtdataRegister;

#endif /* INCLUDE_MAIN_GS_DISPLAY_H */
