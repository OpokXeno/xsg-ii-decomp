#ifndef INCLUDE_MAIN_SV_GET_IMAGE_ITEM_H
#define INCLUDE_MAIN_SV_GET_IMAGE_ITEM_H

#include "common.h"
#include "shared.h"
#include "main/pxy_packet_state.h"

/* One A+D entry: register data, then register address (the GS environment of sdvDrawAlter and the draw packets). */
typedef struct DrawStateRegister {
    u64 data;
    u64 address;
} DrawStateRegister;

/* One 128-bit quadword (sq/lq width). */
typedef unsigned int Quadword __attribute__((mode(TI)));

/* The draw list at svDrawList (static in sv_get_image_item.c, reached by draw_list.c through the linker symbol):
 * entries is the base of the 16-byte slots, flush_code is the word at +0x4 (40 (0x28) at setup; the same code
 * svDrawListFlush appends as its closing record, and drawListSetFlushCode writes it through its own symbol
 * svDrawListFlushCode), end_index is the number of slots queued so far and begin_index the first slot of the
 * record being built. */
typedef struct DrawList {
    Quadword *entries;
    int flush_code;
    int end_index;
    int begin_index;
} DrawList;

/* One entry of the visible list svVisibleSlots: disabled is set for a resource slot, whose recordIndex is then
 * a sefResourceTable index, so svDrawEntriesByKind skips it. */
typedef struct SvDrawSlot {
    short recordIndex;
    short disabled;
    float depth;       /* +0x4: the sort key, the record's clip depth */
} SvDrawSlot;

/* The image mapper: SV_TYPE_LIST_COUNT type lists of SV_ENTRY_COUNT entries. */
#define SV_TYPE_LIST_COUNT 40
#define SV_ENTRY_COUNT 0x100

/*
 * The image-mapper table has a 0x1C-byte header followed by 0x100 entries
 * of 0x24 bytes. These entry fields are evidenced by the lookup routines:
 * the active word is at +0x08, the type byte at +0x0E, and the resource
 * name pointer at +0x20.
 */
typedef struct SvTypeListEntry {
    void *data;
    unsigned char unmodeled_04[4];
    int used;
    short resourceKind;
    union {
        unsigned short typeFlags; /* +0x0e */
        struct { unsigned char type; unsigned char unmodeled_0f; };
    };
    unsigned char unmodeled_10[2];
    unsigned short basePointer;
    unsigned short bitsPerPixel;
    unsigned short bufferWidthPixels;
    unsigned short width;
    unsigned short height;
    unsigned short textureWidthExponent;
    unsigned short textureHeightExponent;
    const char *name;
} SvTypeListEntry;

typedef struct SvTypeList {
    int total;
    int size;
    int capacity;
    unsigned int savedSize;
    int dimensions;                 /* +0x10: width and height halfwords, cleared as one word */
    short entryCount;               /* +0x14 */
    short tag;                      /* +0x16: the fourth svAddImageMapper argument */
    short pendingCount;             /* +0x18: flushed by svImageListDestroy when nonzero */
    unsigned char unmodeled_1A[2];
    SvTypeListEntry entries[SV_ENTRY_COUNT];
} SvTypeList;

/* SvTypeListEntry.resourceKind, equal to the chunk kind it was built from. */
enum {
    SV_RES_CLUT = 3,
    SV_RES_IMAGE = 4,
    SV_RES_SCRIPT = 5,
    SV_RES_MODEL = 6
};

typedef struct SvResourceChunk {
    unsigned int byteSize;
    unsigned char unmodeled_04[4];
    short kind;
    unsigned short typeFlags;
    char payload[0x34];
} SvResourceChunk;

typedef struct SvImageChunk {
    unsigned int byteSize;
    unsigned int dataSize;          /* bytes of pixel data after the header */
    unsigned char unmodeled_08[2];
    unsigned short typeFlags;
    unsigned short width;
    unsigned short height;
    short bitsPerPixel;
    unsigned char unmodeled_12[2];
    char name[0x2C];
} SvImageChunk;

/* A CLUT chunk: the payload's leading halfwords are the size and depth, the name follows at +0x14. */
typedef struct SvClutChunk {
    unsigned char unmodeled_00[10];
    unsigned short typeFlags;
    unsigned short width;
    unsigned short height;
    unsigned short bitsPerPixel;
    unsigned char unmodeled_12[2];
    char name[0x2C];
    unsigned char data[1];
} SvClutChunk;

extern int _draw3D;
/* The _draw3D modes: 2D items, 2D kind-8 sprites and 3D models. */
enum SvRenderMode {
    SV_RENDER_2D = 0,
    SV_RENDER_2D_SPRITE = 1,
    SV_RENDER_3D_MODEL = 2
};

/* A part's transform block, filled by sefInitPartTransform: the 4x4 matrix (rows 0..2 the scaled identity, row 3 the
 * position with w = 1), the position vector, the scale, whether the scale applies, and the kind. The first 0x58 bytes of
 * an SvPart, and the object sefGlobalTransform. */
typedef struct SefPartTransform {
    Matrix4 matrix;   /* +0x00 */
    Vector4 position; /* +0x40 */
    float scale;      /* +0x50 */
    short scaled;     /* +0x54: nonzero when scale applies */
    short kind;       /* +0x56 */
} SefPartTransform;

extern SefPartTransform sefGlobalTransform;

/* Functions of src/main/sv_get_image_item.c that other files call. */
void svLoadDrawViewMatrix(void);
void svBuildDrawStatePacket(void);
void sefTransformClipPoint(Vector4 *out, const Vector4 *in);
int sefClipViewVolumeA(const Vector4 *model, float limit);

/* Returns kind (short in the original, sign-extended before the call). */
int sefInitPartTransform(SefPartTransform *part, int kind, int scaleKey40, int scaleKey80, int scaleKey100);
struct SvDrawItem;
void svInitItemDrawKind(SefPartTransform *part, struct SvDrawItem *item, int scaleKey40, int scaleKey80, int scaleKey100);

int svImageListAlloc(SvTypeList *typeList);

void svAddImageMapper(int typeListId, int entryCount, void *chunk, int tag);
void svDeleteImageMapper(int typeListId);
SvTypeListEntry *svGetScript(int effectId);
SvTypeListEntry *svGetImageListItem(int type, unsigned int flags);

/* Effect slot functions of sv_get_image_item.c, the record table of srs.h (srsRecordTable). */
int srsLoadEffectSlot(void *buffer, int effectId, int arg);
int srsFindFreeEffectSlot(void);
void srsFreeEffectSlot(unsigned int index);
void srsUnloadEffectSlot(unsigned int index);
int srsEvictEffectSlot(void);
int srsEvictEffectSlotsForSize(int size);
int srsIsEffectKeyListed(int key);
void srsReleaseOtherOwnerEffectSlots(void);

void svDrawEffectRecords3D(void);
void svDrawScene2D(void);
void svDrawScene3D(void);
void svForceImageMapper(unsigned int index);

/* The texture parameters svGetPrmFromName fills for a bitmap or model name: the TEX0 value, the scaled size and the two
 * resources the effects draw with. */
typedef struct SvTexParam {
    u64 tex0;                      /* +0x00 */
    float scaledWidth;             /* +0x08 */
    float scaledHeight;            /* +0x0C */
    PxyHeader *primaryData;        /* +0x10: the model pxySubmitModel draws */
    TxyBlock *companionData;       /* +0x14: the texture block nmlModelSetTexture binds */
} SvTexParam;

SvTexParam *svGetPrmFromName(const char *resourceName, int type, SvTexParam *parameter);

#endif /* INCLUDE_MAIN_SV_GET_IMAGE_ITEM_H */
