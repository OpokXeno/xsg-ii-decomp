#include "common.h"
#include "shared.h"
#include "gs.h"
#include "ov01/tu048.h"

typedef struct XglPacket XglPacket;

/* EE scratchpad RAM. */
#define SCRATCHPAD_BASE 0x70000000

extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddCode(XglPacket *packet, unsigned int code);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);
extern XglPacket *xglPacketGetCurrent(void);

/*
 * The VIF1 direct-mode packet builder object. xglPacketInit constructs two
 * of them back to back inside asPacketSource, 0x28 bytes apart (main
 * 0x0022c458-0x0022c4b4 writes both entries' +0x20 and +0x24 at that
 * stride), so sizeof(XglPacket) is 0x28. Only the members the functions of
 * this TU touch are named; everything the sceVif1Pk* library alone reads or
 * writes stays unmodeled.
 */
struct XglPacket {
    /* xglFontReloadTexture decrements this forward cursor by four words. */
    u32 *write_cursor;
    /*
     * +0x04: the address xglPacketSend hands to xglDmaDirectSrcChain as the
     * DMA source-chain start whenever a packet is pending (main
     * 0x0022c520-0x0022c548). No function of this TU writes it -- neither
     * xglPacketInit nor xglPacketMove touches +0x04 -- so it is set by the
     * sceVif1Pk* library: sceVif1PkInit (main 0x0020ac68) stores its base
     * argument at +0x00 and +0x04, and xglPacketInit passes it each entry's
     * buffer base (0x00c00000 and 0x00e00000).
     */
    u32 start;
    /*
 * +0x08 and +0x0c: xglPrimAddGifTagDirect (main 0x00222c00 lw v0,8(s0);
 * main 0x00222c0c lw v0,0xc(s0)) tests both for non-zero to decide
 * whether a direct-mode block the sceVif1Pk* library opened is still
 * open, so it appends to that block instead of resetting/opening/
 * closing a new one (compare SGsSendPacket, src/main/sv_get_image_
 * item.c, which always resets/opens/closes without this check).
 * Neither word is written by any function of this TU, so both are set
 * by the sceVif1Pk* library.
 */
    u32 directCode;
    u32 *directState;
    unsigned char unmodeled_10[0x10];
    /*
     * +0x20: the packet's fixed top-of-buffer address. xglPacketInit sets it
     * once per entry after both sceVif1PkInit calls (main 0x0022c49c for the
     * first entry, value 0x00e00000; main 0x0022c4a0 for the second, value
     * 0x01000000): the end of the buffer whose base went to sceVif1PkInit.
     */
    u32 limit;
    /*
     * +0x24: the write cursor. xglPacketInit seeds it equal to `limit` (main
     * 0x0022c48c/0x0022c490) and xglPacketMove reloads `limit` into it at
     * the start of every frame (main 0x0022c4e4/0x0022c4f0), so writers that
     * fill the packet backward from the top always restart from the same
     * address. main/tu107's nmlPacketSetAttributeData and
     * nmlPacketSetAttributeAlloc16N read it, decrement it by the size being
     * requested and store it back, then use the new value as the memcpy
     * destination or return it as the allocation (main 0x00238e28-0x00238e3c
     * and 0x00238fe0-0x00238ff4),
     * which is why the member is byte-addressed instead of scaled.
     */
    u8 *cursor;
};

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
typedef struct GrScreenVertex {
    int x;      /* +0x00 */
    int y;      /* +0x04 */
    int z;      /* +0x08 */
    int flags;  /* +0x0c */
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

typedef struct GrVramCopyParams {
    unsigned short source_base;
    unsigned short source_width;
    unsigned short destination_base;
    unsigned short destination_width;
    unsigned short source_x;
    unsigned short source_y;
    unsigned short destination_x;
    unsigned short destination_y;
    unsigned short width;
    unsigned short height;
} GrVramCopyParams;

extern void grVramCopy(GrVramCopyParams *params);
typedef struct GrRenderState {
    unsigned char unmodeled_00[0x20];
    unsigned short current_buffer;
    unsigned short drawing_buffer;
} GrRenderState;

extern GrRenderState sRender;
/* Framebuffer base addresses; grFBAdrGet indexes this table. */
int fb[4] = { 112, 224, 336, 0xFFFFF };

/* VIF FLUSH code (command 0x11 in bits 24..30, no immediate). */
#define VIF_CODE_FLUSH 0x11000000

extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data,
                                    int count);
extern void sceVif1PkTerminate(XglPacket *packet);
/* One GS register write of grGsRegListSet's list; register_id -1 ends it. */
typedef struct GrGsRegListEntry {
    int register_id;          /* +0x00 */
    unsigned long long value; /* +0x08 */
} GrGsRegListEntry;

void grGpInit(GrPacket *packet)
{
    packet->count = 0;
    packet->data = (void *)SCRATCHPAD_BASE;
}

/* One record per primitive kind (F2, F4, FT4, FT4STQ, Spr). Each Open function
 * below appends a GIF packed-mode tag to the packet, stores the address of the
 * tag in `control` and clears `count`; the grPutXxx vertex writers add the
 * number of vertices they put to `count`. The matching Close function ORs
 * `count` into the tag's control doubleword, the GIF NLOOP field, after the
 * draw of those vertices. `control` is untyped like GrPacket.data: the Open
 * functions reload packet->data after storing it, which a typed pointer
 * member does not give. */
typedef struct GrPutTag {
    void *control; /* +0x00 */
    int count;     /* +0x04 */
} GrPutTag;

static GrPutTag grPutF2Tag;
static GrPutTag grPutF4Tag;
static GrPutTag grPutFT4Tag;
static GrPutTag grPutFT4STQTag;
static GrPutTag grPutSprTag;

/* Offset to the GS window that the 12.4 screen coordinates are biased by. */
#define GR_GS_WINDOW_OFFSET_X 0x7000
#define GR_GS_WINDOW_OFFSET_Y 0x7200

extern float grRotTransPers(GrScreenVertex *vertex, const Matrix4 matrix,
                            const Vector4 *point, int *clipFlags);

/* grPutF2's line: two end points and one shared RGBA color. */
typedef struct GrF2Vertices {
    Vector4 points[2];      /* +0x00 */
    unsigned char color[4]; /* +0x20: R, G, B, A */
} GrF2Vertices;

/* grPutF4's quad: four corners and one shared RGBA color. */
typedef struct GrF4Vertices {
    Vector4 points[4];      /* +0x00 */
    unsigned char color[4]; /* +0x40: R, G, B, A */
} GrF4Vertices;

/* grPutFT4's and grPutFT4STQ's quad: four corners, one shared RGBA and
 * per-corner S/T. */
typedef struct GrFT4STQVertices {
    Vector4 points[4];      /* +0x00 */
    unsigned char color[4]; /* +0x40: R, G, B, A */
    float s[4];             /* +0x44 */
    float t[4];             /* +0x54 */
} GrFT4STQVertices;

/* grPutSpr's sprite: two corners, one RGBA color and per-corner U/V. */
typedef struct GrSpr {
    Vector4 vertex[2];    /* +0x00 */
    unsigned char col[4]; /* +0x20 */
    short u[2];           /* +0x24 */
    short v[2];           /* +0x28 */
} GrSpr;

/* GS primitive types of the grPutXxx tags (PRIM bits 0..2). */
#define GR_PRIM_LINE 1
#define GR_PRIM_TRIANGLE_STRIP 4
#define GR_PRIM_SPRITE 6

/* GIFtag REGS lists (one nibble per register, first register lowest) of the
 * grPutXxx tags: each vertex is an RGBAQ and an XYZ2, after an UV or ST/Q
 * quadword when textured. */
#define GIF_REGS_RGBAQ_XYZ2 (GS_RGBAQ | (GS_XYZ2 << 4))
#define GIF_REGS_UV_RGBAQ_XYZ2 (GS_UV | (GS_RGBAQ << 4) | (GS_XYZ2 << 8))
#define GIF_REGS_ST_RGBAQ_XYZ2 (GS_ST | (GS_RGBAQ << 4) | (GS_XYZ2 << 8))

/* GIFtag bits 46..63 of the grPutXxx tags: PRE set, the PRIM register value
 * and NREG (FLG 0, packed mode). NLOOP and EOP are or'ed in separately. */
#define GR_GIF_TAG_PRIM(prim, nreg) \
    ((1ULL << 46) | ((unsigned long long)(prim) << 47) | ((unsigned long long)(nreg) << 60))

/* Starts a primitive group: remembers where its GIF tag goes, then appends
 * the tag (REGS list, then NLOOP 0 | EOP | prim) to the packet. */
static inline void grPutTagOpen(GrPutTag *tag, GrPacket *packet,
                                unsigned long long registers,
                                unsigned long long tag_prim)
{
    int *count;

    tag->count = 0;
    tag->control = GR_PACKET_QUAD(packet, packet->count);
    grGifOpenRegister(packet, registers);
    count = &packet->count;
    GR_PACKET_QUAD(packet, (*count)++)->tag.control.bits = tag->count | GIF_TAG_EOP | tag_prim;
}

/* Ends a primitive group: the vertex count becomes the tag's NLOOP. */
static inline void grPutTagClose(GrPutTag *tag)
{
    GrGifOpenTag *control = tag->control;

    control->control.bits |= tag->count;
}

void grPutF2Open(GrPacket *packet)
{
    grPutTagOpen(&grPutF2Tag, packet, GIF_REGS_RGBAQ_XYZ2,
                 GR_GIF_TAG_PRIM(GS_SET_PRIM(GR_PRIM_LINE, 0, 0, 0, 1, 0, 1, 0, 0), 2));
}

void grPutF2Close(void)
{
    grPutTagClose(&grPutF2Tag);
}

void grPutF4Open(GrPacket *packet)
{
    grPutTagOpen(&grPutF4Tag, packet, GIF_REGS_RGBAQ_XYZ2,
                 GR_GIF_TAG_PRIM(GS_SET_PRIM(GR_PRIM_TRIANGLE_STRIP, 0, 0, 0, 1, 0, 1, 0, 0), 2));
}

void grPutF4Close(void)
{
    grPutTagClose(&grPutF4Tag);
}

void grPutFT4Open(GrPacket *packet)
{
    grPutTagOpen(&grPutFT4Tag, packet, GIF_REGS_UV_RGBAQ_XYZ2,
                 GR_GIF_TAG_PRIM(GS_SET_PRIM(GR_PRIM_TRIANGLE_STRIP, 0, 1, 0, 1, 0, 1, 0, 0), 3));
}

void grPutFT4Close(void)
{
    grPutTagClose(&grPutFT4Tag);
}

void grPutFT4STQOpen(GrPacket *packet)
{
    grPutTagOpen(&grPutFT4STQTag, packet, GIF_REGS_ST_RGBAQ_XYZ2,
                 GR_GIF_TAG_PRIM(GS_SET_PRIM(GR_PRIM_TRIANGLE_STRIP, 0, 1, 0, 1, 0, 0, 0, 0), 3));
}

void grPutFT4STQClose(void)
{
    grPutTagClose(&grPutFT4STQTag);
}

void grPutSprOpen(GrPacket *packet)
{
    grPutTagOpen(&grPutSprTag, packet, GIF_REGS_UV_RGBAQ_XYZ2,
                 GR_GIF_TAG_PRIM(GS_SET_PRIM(GR_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0), 3));
}

void grPutSprClose(void)
{
    grPutTagClose(&grPutSprTag);
}

/* Writes each end point as RGBAQ + XYZ2. Without a matrix the points are
 * already screen coordinates (12.4 fixed point, offset to the GS window). */
void grPutF2(GrPacket *packet, GrF2Vertices *line, const Matrix4 matrix)
{
    int clipFlags = 0;
    Vector4 *point = line->points;
    int *count;
    int i;

    for (i = 0; i < 2; i++, point++) {
        GR_PACKET_QUAD(packet, packet->count)->rgba.r = line->color[0];
        GR_PACKET_QUAD(packet, packet->count)->rgba.g = line->color[1];
        GR_PACKET_QUAD(packet, packet->count)->rgba.b = line->color[2];
        count = &packet->count;
        GR_PACKET_QUAD(packet, (*count)++)->rgba.a = line->color[3];
        if (matrix == 0) {
            GR_PACKET_QUAD(packet, *count)->xyz.x =
                (int)point->x * 16 + GR_GS_WINDOW_OFFSET_X;
            GR_PACKET_QUAD(packet, *count)->xyz.y =
                (int)point->y * 16 + GR_GS_WINDOW_OFFSET_Y;
            GR_PACKET_QUAD(packet, *count)->xyz.z = (int)point->z;
            GR_PACKET_QUAD(packet, (*count)++)->xyz.flags = 0;
        } else {
            grRotTransPers(&GR_PACKET_QUAD(packet, (*count)++)->xyz, matrix,
                           &line->points[i], &clipFlags);
        }
    }
    grPutF2Tag.count += 2;
}

/* Writes a four-corner quad as RGBAQ + XYZ2 per corner, like grPutF2. */
void grPutF4(GrPacket *packet, GrF4Vertices *quad, const Matrix4 matrix)
{
    int clipFlags = 0;
    Vector4 *point = quad->points;
    int *count;
    int i;

    for (i = 0; i < 4; i++, point++) {
        GR_PACKET_QUAD(packet, packet->count)->rgba.r = quad->color[0];
        GR_PACKET_QUAD(packet, packet->count)->rgba.g = quad->color[1];
        GR_PACKET_QUAD(packet, packet->count)->rgba.b = quad->color[2];
        count = &packet->count;
        GR_PACKET_QUAD(packet, (*count)++)->rgba.a = quad->color[3];
        if (matrix == 0) {
            GR_PACKET_QUAD(packet, *count)->xyz.x =
                (int)point->x * 16 + GR_GS_WINDOW_OFFSET_X;
            GR_PACKET_QUAD(packet, *count)->xyz.y =
                (int)point->y * 16 + GR_GS_WINDOW_OFFSET_Y;
            GR_PACKET_QUAD(packet, *count)->xyz.z = (int)point->z;
            GR_PACKET_QUAD(packet, (*count)++)->xyz.flags = 0;
        } else {
            grRotTransPers(&GR_PACKET_QUAD(packet, (*count)++)->xyz, matrix,
                           &quad->points[i], &clipFlags);
        }
    }
    grPutF4Tag.count += 4;
}

/* grPutF4 with a UV quadword per corner: the GrFT4STQVertices S and T
 * scaled to 12.4 fixed point. Two cursors walk the corners: point for the
 * screen-space path, vertex for grRotTransPers. */
void grPutFT4(GrPacket *packet, GrFT4STQVertices *quad, const Matrix4 matrix)
{
    int clipFlags = 0;
    Vector4 *vertex = quad->points;
    Vector4 *point = quad->points;
    int *count;
    int i;

    for (i = 0; i < 4; vertex++, point++, i++) {
        GR_PACKET_QUAD(packet, packet->count)->uv.u = quad->s[i] * 16.0f;
        GR_PACKET_QUAD(packet, packet->count++)->uv.v = quad->t[i] * 16.0f;
        GR_PACKET_QUAD(packet, packet->count)->rgba.r = quad->color[0];
        GR_PACKET_QUAD(packet, packet->count)->rgba.g = quad->color[1];
        GR_PACKET_QUAD(packet, packet->count)->rgba.b = quad->color[2];
        count = &packet->count;
        GR_PACKET_QUAD(packet, (*count)++)->rgba.a = quad->color[3];
        if (matrix == 0) {
            GR_PACKET_QUAD(packet, *count)->xyz.x =
                (int)(point->x * 16.0f + GR_GS_WINDOW_OFFSET_X);
            GR_PACKET_QUAD(packet, *count)->xyz.y =
                (int)(point->y * 16.0f + GR_GS_WINDOW_OFFSET_Y);
            GR_PACKET_QUAD(packet, *count)->xyz.z = (int)point->z;
            GR_PACKET_QUAD(packet, (*count)++)->xyz.flags = 0;
        } else {
            grRotTransPers(&GR_PACKET_QUAD(packet, (*count)++)->xyz, matrix,
                           vertex, &clipFlags);
        }
    }
    grPutFT4Tag.count += 4;
}

/*
 * Writes a textured quad with an STQ quadword per corner. The Q of each corner
 * is the reciprocal W that grRotTransPers returns for it.
 */
void grPutFT4STQ(GrPacket *packet, GrFT4STQVertices *quad,
                 const Matrix4 matrix)
{
    int clipFlags = 0;
    int *count;
    int index;
    float q;
    int i;

    for (i = 0; i < 4; i++) {
        q = grRotTransPers(&(GR_PACKET_QUAD(packet, packet->count) + 2)->xyz,
                           matrix, &quad->points[i], &clipFlags);
        GR_PACKET_QUAD(packet, packet->count)->stq.s = quad->s[i] * q;
        GR_PACKET_QUAD(packet, packet->count)->stq.t = quad->t[i] * q;
        count = &packet->count;
        index = *count;
        GR_PACKET_QUAD(packet, index)->stq.q = q;
        *count = index + 1;
        GR_PACKET_QUAD(packet, *count)->rgba.r = quad->color[0];
        GR_PACKET_QUAD(packet, *count)->rgba.g = quad->color[1];
        GR_PACKET_QUAD(packet, *count)->rgba.b = quad->color[2];
        GR_PACKET_QUAD(packet, *count)->rgba.a = quad->color[3];
        *count += 2;
    }
    grPutFT4STQTag.count += 4;
}

/*
 * Writes a sprite as two corners, each an RGBAQ and an XYZ2 quadword. flag 0
 * converts the vertices to 12.4 screen coordinates; otherwise they are written
 * as integers.
 */
void grPutSpr(GrPacket *packet, GrSpr *prim, int flag)
{
    Vector4 *v;
    int *count;
    int *countSlot;
    int **countSlotRef;
    int i;
    int rgbaIndex;
    int xyzIndex;

    count = &packet->count;
    for (i = 0; i < 2; i++) {
        GR_PACKET_QUAD(packet, packet->count)->rgba.r = prim->u[i] * 16;
        countSlot = &packet->count;
        rgbaIndex = *countSlot;
        GR_PACKET_QUAD(packet, rgbaIndex)->rgba.g = prim->v[i] * 16;
        rgbaIndex++;
        *countSlot = rgbaIndex;
        GR_PACKET_QUAD(packet, rgbaIndex)->rgba.r = prim->col[0];
        GR_PACKET_QUAD(packet, *countSlot)->rgba.g = prim->col[1];
        GR_PACKET_QUAD(packet, *countSlot)->rgba.b = prim->col[2];
        xyzIndex = *countSlot;
        GR_PACKET_QUAD(packet, xyzIndex)->rgba.a = prim->col[3];
        xyzIndex++;
        countSlotRef = &countSlot;
        *(*countSlotRef) = xyzIndex;
        v = &prim->vertex[i];
        if (flag == 0) {
            int flagsIndex;

            GR_PACKET_QUAD(packet, xyzIndex)->xyz.x = ((int)v->x * 16) + GR_GS_WINDOW_OFFSET_X;
            GR_PACKET_QUAD(packet, *count)->xyz.y = ((int)v->y * 16) + GR_GS_WINDOW_OFFSET_Y;
            GR_PACKET_QUAD(packet, *count)->xyz.z = (int)v->z;
            flagsIndex = *count;
            GR_PACKET_QUAD(packet, flagsIndex)->xyz.flags = 0;
            flagsIndex++;
            *count = flagsIndex;
        } else {
            int flagsIndex;

            GR_PACKET_QUAD(packet, xyzIndex)->xyz.x = (int)v->x;
            GR_PACKET_QUAD(packet, *count)->xyz.y = (int)v->y;
            GR_PACKET_QUAD(packet, *count)->xyz.z = (int)v->z;
            flagsIndex = *count;
            GR_PACKET_QUAD(packet, flagsIndex)->xyz.flags = 0;
            flagsIndex++;
            *count = flagsIndex;
        }
    }

    grPutSprTag.count += 2;
}

void func_0012B4B8(XglPacket *packet);

void grPacketSend(GrPacket *packet)
{
    XglPacket *current_packet;

    if (packet->count == 0) {
        return;
    }
    current_packet = xglPacketGetCurrent();
    sceVif1PkCnt(current_packet, 0);
    sceVif1PkAddCode(current_packet, VIF_CODE_FLUSH);
    sceVif1PkOpenDirectHLCode(current_packet, 0);
    sceVif1PkAddDirectDataN(current_packet, packet->data, packet->count);
    sceVif1PkCloseDirectHLCode(current_packet);
    func_0012B4B8(current_packet);
}

void grGsRegSet(GrPacket *packet, unsigned long long register_id,
                unsigned long long value)
{
    grGifAppendRegister(packet, 14, 0x1000000000008001ULL);
    grGifAppendRegister(packet, register_id, value);
}

void grGsRegListSet(GrPacket *packet, GrGsRegListEntry *entries)
{
    GrGifOpenTag *head;
    GrGifOpenTag *tags;
    int *count;
    int tag_index = packet->count;
    int register_count = 0;

    grGifAppendRegister(packet, 14, 0x1000000000008000ULL);
    while (entries->register_id != -1) {
        grGifOpenRegister(packet, entries->register_id);
        count = &packet->count;
        tags = packet->data;
        tags[(*count)++].control.bits = entries->value;
        entries++;
        register_count++;
    }
    head = packet->data;
    head[tag_index].control.bits |= register_count;
}

int grFBAdrGet(int index)
{
    return fb[index];
}

int grBBIdxGet(void)
{
    int index = 0;

    if (fb[0] != 0xFFFFF) {
        while (fb[index] != 0xFFFFF) {
            if (fb[index] != sRender.drawing_buffer &&
                fb[index] != sRender.current_buffer) {
                return index;
            }
            index++;
        }
    }

    return -1;
}

int grDBIdxGet(int *drawing_index, int *current_index)
{
    int index = 0;

    if (fb[0] != 0xFFFFF) {
        for (; fb[index] != 0xFFFFF; index++) {
            if (sRender.drawing_buffer == fb[index]) {
                *drawing_index = index;
            }
            if (sRender.current_buffer == fb[index]) {
                *current_index = index;
            }
        }
    }

    return 1;
}

/* BITBLTBUF base pointers (64-word units: 0x0E00, 0x1C00, 0x2A00) of the
 * three framebuffers grVramCopyBuffers indexes. */
typedef struct GrFrameBaseTable {
    unsigned short base[3];
} GrFrameBaseTable;

const GrFrameBaseTable grFrameBaseTable = { { 0x0E00, 0x1C00, 0x2A00 } };
const GrVramCopyParams grVramCopyParams = { 0, 8, 0, 8, 0, 0, 0, 0, 512, 448 };
char menuTgtStatusTextBuffer[128];

/* Copies framebuffer source to framebuffer destination (indices into the
 * base table); unlike XS1's grVramCopyFBtoFB the indices are not checked. */
void grVramCopyBuffers(int source, int destination)
{
    GrFrameBaseTable bases = grFrameBaseTable;
    GrVramCopyParams params = grVramCopyParams;

    params.source_base = bases.base[source];
    params.destination_base = bases.base[destination];
    grVramCopy(&params);
}

/* Local-to-local GS transfer: BITBLTBUF (0x50), TRXPOS (0x51), TRXREG
 * (0x52), then TRXDIR (0x53) = 2 starts it. */
void grVramCopy(GrVramCopyParams *params)
{
    GrPacket packet;
    int *count;

    grGpInit(&packet);
    grGifOpenRegister(&packet, 14);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits = 0x1000000000008004ULL;

    grGifOpenRegister(&packet, 0x50);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits =
        params->source_base |
        (unsigned long long)params->source_width << 16 |
        (unsigned long long)params->destination_base << 32 |
        (unsigned long long)params->destination_width << 48;

    grGifOpenRegister(&packet, 0x51);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits =
        params->source_x |
        (unsigned long long)params->source_y << 16 |
        (unsigned long long)params->destination_x << 32 |
        (unsigned long long)params->destination_y << 48;

    grGifOpenRegister(&packet, 0x52);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits =
        params->width | (unsigned long long)params->height << 32;

    grGifOpenRegister(&packet, 0x53);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits = 2;

    grPacketSend(&packet);
}
