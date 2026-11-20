#ifndef INCLUDE_OV01_GR_GP_INIT_H
#define INCLUDE_OV01_GR_GP_INIT_H

#include "shared.h"

/* Functions of src/ov01/gr_gp_init.c that other files call. */

/* One RGBA colour of a sprite. */
typedef struct MenuRGBA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} MenuRGBA;

/* The sprite colour: menuStatSpriteRGB copies a caller's MenuRGBA into it
 * byte-wise (lwl/lwr, swl/swr), while menuStatSprite/menuStatSprite2 copy the
 * whole word from an aligned local (lw/sw), so the field is word-aligned. */
typedef union MenuColor {
    unsigned int word;
    MenuRGBA rgba;
} MenuColor;

/* grPutSpr's sprite: two corners, one RGBA color and per-corner U/V. */
typedef struct GrSpr {
    Vector4 vertex[2];    /* +0x00 */
    MenuColor col;        /* +0x20 */
    short u[2];           /* +0x24 */
    short v[2];           /* +0x28 */
} GrSpr;

/* grGpInit's argument: the VIF1 DIRECT packet grPacketSend transmits, a data buffer and its length. grGpInit
 * points data at the scratchpad (0x70000000). A local packet must be the whole record, because grGpInit writes
 * both words. */
typedef struct GrPacket {
    void *data;  /* +0x0 */
    int count;   /* +0x4 */
} GrPacket;

/* GIF packet words are written as doubleword descriptors by the openers and
 * as 32-bit lanes by the vertex writers. Both are the same packet storage. */
typedef union GrGifWord {
    unsigned long long bits;
    unsigned int lanes[2];
} GrGifWord;

typedef struct GrGifOpenTag {
    GrGifWord control;
    GrGifWord register_id;
} GrGifOpenTag;

/*
 * grRotTransPers's output vertex: the screen-space X/Y grPutXxx (siblings,
 * not decoded here) feed to the GS in 12.4 fixed point (the *16.0f below),
 * the projected depth as a plain integer, and the out-of-frustum bit
 * (0x8000) grRotTransPers itself copies from *clipFlags.
 */
typedef union GrScreenVertex {
    struct {
        int x;      /* +0x00 */
        int y;      /* +0x04 */
        int z;      /* +0x08 */
        int flags;  /* +0x0c */
    };
    unsigned long long words[2];    /* the union view is what orders grRotTransPers's stores as the original does */
} GrScreenVertex;

/* One GIF packed-mode quadword: an A+D register write, or as the vertex
 * writers fill it ST/Q, RGBAQ or XYZ2 (the vertex grRotTransPers writes). */
typedef struct GrGifSTQ {
    float s; /* +0x00 */
    float t; /* +0x04 */
    float q; /* +0x08 */
} GrGifSTQ;

typedef struct GrGifRGBA {
    int r; /* +0x00 */
    int g; /* +0x04 */
    int b; /* +0x08 */
    int a; /* +0x0c */
} GrGifRGBA;

/* The UV quadword grPutFT4 writes: UV coordinates in 12.4 fixed point,
 * converted from float S/T as unsigned (the c.le.s against 2^31 and the or of
 * 0x80000000). */
typedef struct GrGifUV {
    unsigned int u; /* +0x00 */
    unsigned int v; /* +0x04 */
} GrGifUV;

typedef union GrGifQuad {
    GrGifOpenTag tag;
    GrGifSTQ stq;
    GrGifUV uv;
    GrGifRGBA rgba;
    GrScreenVertex xyz;
} GrGifQuad;

/* The packet's quadword at index, from a fresh read of packet->data. */
#define GR_PACKET_QUAD(packet, index) ((GrGifQuad *)(packet)->data + (index))

static inline void grGifOpenRegister(GrPacket *packet, unsigned long long register_id)
{
    GrGifOpenTag *tags = packet->data;
    tags[packet->count].register_id.bits = register_id;
}

static inline void grGifAppendRegister(GrPacket *packet,
                                       unsigned long long register_id,
                                       unsigned long long control)
{
    GrGifOpenTag *tags;
    int *count;
    int tag_index;

    grGifOpenRegister(packet, register_id);
    count = &packet->count;
    tags = packet->data;
    tag_index = *count;
    tags[tag_index].control.bits = control;
    *count = tag_index + 1;
}

void grGpInit(GrPacket *packet);
void grPacketSend(GrPacket *packet);
void grPutSpr(GrPacket *packet, GrSpr *prim, int flag);
void grOpenSpr(GrPacket *packet);
void grGsRegSet(GrPacket *packet, unsigned long long register_id, unsigned long long value);
void grCloseSpr(void);
void GS_REG_SET(GrPacket *packet, long long register_id, long long control);

#endif /* INCLUDE_OV01_GR_GP_INIT_H */
