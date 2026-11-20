#ifndef INCLUDE_MAIN_FONT_DRAW_CURSOR_H
#define INCLUDE_MAIN_FONT_DRAW_CURSOR_H

#include "shared.h"

struct XglPacket;

/* Packet that FONT_OP_EFFECT_PACKET builds in the cursor's work area: an A+D GIF tag and one
 * register write. */
struct FontEffectScratchPacket {
    u64 vif_tag;
    u64 vif_count;
    u64 effect_state;
    u64 payload_count;
};

/* The font renderer's draw cursor, built by xglFontFlush at the scratchpad base. A queued draw
 * callback (xglFontPrintExtFunc) receives it and adds packets through it. */
typedef struct FontDrawCursor {
    struct XglPacket *packet;
    short origin_x;
    unsigned char unmodeled_06[2];
    unsigned short x;
    unsigned short y;
    int depth;
    unsigned short offset_x;
    unsigned short offset_y;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char unmodeled_17;
    unsigned char flags;
    unsigned char effect;
    unsigned char attributes;
    unsigned char draw_state_high;
    unsigned char unmodeled_1c[4];
    signed char effect_x;
    signed char effect_y;
    unsigned char unmodeled_22[2];
    unsigned char effect_red;
    unsigned char effect_green;
    unsigned char effect_blue;
    unsigned char unmodeled_27;
    /* The GS ALPHA_1 value: the A/B/C/D selectors and FIX. */
    unsigned char alpha_abcd;
    unsigned char alpha_fix;
    unsigned char alpha;
    unsigned char unmodeled_2b;
    unsigned short scale_x;
    unsigned short scale_y;
    union {
        struct FontEffectScratchPacket effect_packet;
        u64 qwords[(0x4000 - 0x30) / sizeof(u64)];
    } work;
} FontDrawCursor;

/* A queued draw callback: called with the draw cursor and the argument
 * registered with it. */
typedef void (*FontExtFunc)(FontDrawCursor *cursor, void *argument);

/* Queues the callback with the given argument at draw order ot (an argument of 1 resets, for the scissor
 * callbacks); xglFontFlush calls it with the draw cursor when it reaches the record. */
void xglFontPrintExtFunc(int ot, FontExtFunc function, void *argument);

#endif /* INCLUDE_MAIN_FONT_DRAW_CURSOR_H */
