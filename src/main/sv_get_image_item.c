#include "common.h"
#include "shared.h"
#include "main/sv_get_image_item.h"
#include "main/srs.h"
#include "main/sdv.h"
#include "main/m_math.h"
#include "main/sef.h"
#include "main/res_get_path.h"
#include "main/sm_initilize.h"
#include "vu0_macro.h"
#include "gs.h"
#include "main/xgl_packet.h"
#include "main/nml_model_set.h"
#include "main/pxy_packet_state.h"
#include "main/xgl_studio.h"
#include "main/vu0_matrix.h"
#include "main/vu0_rotate.h"
#include "main/vu0_vector.h"
#include "main/xgl_2.h"

/* Small data before the draw state pointer: five 8-byte tag strings, the sort count, the draw mode and the fog value. */
/* Scaffold .bss object of this TU: 384 bytes at svEffectLightSet (the scaffold split it at 0x6AF890, a 16-byte aligned vector at +0xD0). */
static u8 svEffectLightSet[384];
void *svActiveLightSet = svEffectLightSet;
char lightLogDirectionLabel[8] = "sDir";
char lightLogColorLabel[8] = "sCol";
char lightLogShadowLabel[8] = "sSha";
char lightLogPointPositionLabel[8] = "sPntP";
char lightLogPointColorLabel[8] = "sPntC";
int svSortCount = 0;
int _draw3D = 0;
int svFogValue = -1;

/* Scaffold .sbss items of this TU only (no other TU reads them), in address order from svDrawTestWord. */
static u64 svDrawTestWord;
static int svZTestEnable;
static const Vector4 *svVertexScale;
static const Vector4 *svRecordScale;
static int svRequestedMapperIndex;
static int svActiveMapperIndex;
static void *svRecordSourcePointer;
static void *svRecordTargetPointer;
static int svItemModeFlags;
static Vector4 *svRecordSourceRowsPointer;
static int svDrawMode;
static int svPolygonAxesMode;
static Matrix4 *svRecordMatrix;
static int svPivotAddress;
static Matrix4 *svViewMatrix;
static int svItemDrawFlags;


typedef struct XglPacket XglPacket;

typedef struct DrawListEntry {
    unsigned int flush_word;        /* +0x00: VIF FLUSH, 0x11000000 */
    unsigned int nop_word;          /* +0x04 */
    unsigned int stcycl_word;       /* +0x08: STCYCL 4,4, 0x01000404 */
    unsigned int unpack_word;       /* +0x0C: UNPACK header, code | 0x6C010000 */
    union {
        Quadword payload;           /* +0x10: cleared as one quadword */
        struct {
            unsigned int count_word; /* +0x10 */
            int operand_a;
            int operand_b;
            unsigned int tail_word; /* +0x1C */
        };
    };
} DrawListEntry;

extern void *memset(void *destination, int value, unsigned int count);
/*
 * Appends one draw record at the end cursor: a VIF FLUSH, STCYCL 4,4, an
 * UNPACK to VU address `vuAddress` and the operands. The record starts at the
 * old end, which becomes begin_index, and end_index moves on by 2.
 */
void svDrawListAppend(int code, int operand_a, int operand_b);
/* Emits the VIF packet for `count` records at `entry`: sceVif1PkRef calls and UNPACK headers, split at 0x100 records. */
void svDrawListEmit(XglPacket *packet, DrawListEntry *entry, int pending_count);
extern int svSortCount;
/* Byte offsets of the two draw list buffers and the flush record code (svDrawList.flush_code). */
#define SV_DRAW_LIST_FIRST_OFFSET 0xE00
#define SV_DRAW_LIST_SECOND_OFFSET 0x200
#define SV_DRAW_LIST_FLUSH_CODE 0x28
/* Queued records that trigger svDrawListFlush from svDrawListTryFlush. */
#define SV_DRAW_LIST_FLUSH_THRESHOLD 0x1AE
/* Depth-sort buckets (svSortBuckets) and the queued item limit (svSortItems). */
#define SV_SORT_BUCKET_COUNT 512
#define SV_SORT_ITEM_LIMIT 1200
/* One depth-sort item (svSortItems, 12 bytes): the next item index in its bucket, -1 at the end, then the id, flags and depth. */
typedef struct SortItem {
    short next;
    short id;
    short flags;
    float depth;
} SortItem;

/* One depth bucket (svSortBuckets, 12 bytes): the head item index, -1 when empty, and the tail word that the first insert sets. */
typedef struct SortBucket {
    short head;
    unsigned char unmodeled_02[4];
    short tail;
    unsigned char unmodeled_08[4];
} SortBucket;

/* The view block svDrawSendViewSetup uploads to VU1: a unit rotation with the origin, the screen scale and offset (z scaled by 1/16, w cleared) and the view matrix. */
typedef struct SvViewUpload {
    Matrix4 unit;
    Vector4 scale;
    Vector4 offset;
    Matrix4 view;
} SvViewUpload;

/* .bss of this TU in address order (data recovery pass 4): the sort chain, the matrix and vector scratch, the view upload,
 * the draw list header and the draw-list storage that follows it. */
static SortItem svSortItems[SV_SORT_ITEM_LIMIT];
static SortBucket svSortBuckets[SV_SORT_BUCKET_COUNT];
static Matrix4 svEffectCameraMatrices[4];
static Vector4 svEffectScaleVector;
static u8 svDrawStatePacketBuffer[256];
static SvViewUpload svViewUpload;
static DrawList svDrawList;
static u8 _scheduler[0x29C00];
extern u32 svDrawModeTagTable[];
extern int svStaticRefQuadwordCount[];
extern unsigned char svStaticRefData[];

/*
 * Clears the depth-sort state: the item count, the 0x200 bucket heads
 * (svSortBuckets, 12 bytes each) and the item table (svSortItems, 12 bytes
 * each). Every slot is set to -1, the empty marker.
 */
void svSortReset(void)
{
    svSortCount = 0;
    memset(svSortBuckets, -1, 0x1800);
    memset(svSortItems, -1, 0x3840);
}

/*
 * Inserts an item (id, flags, depth) into the depth-sort buckets. The bucket
 * is depth * 20, clamped to 0x1FF; inside a bucket the items stay in
 * ascending depth order. Refused once 0x4B0 items are queued.
 */
void svSortAdd(int id, int flags, float depth)
{
    int itemIndex = svSortCount;
    int bucketIndex;
    SortBucket *bucket;
    SortItem *item;
    SortItem *nextItem;
    SortItem *prev;
    int nextIndex;

    if (depth < 0.0f) {
        return;
    }
    bucketIndex = (int)(depth * 20.0f);
    if (bucketIndex >= SV_SORT_BUCKET_COUNT) {
        bucketIndex = SV_SORT_BUCKET_COUNT - 1;
    }
    if (itemIndex >= SV_SORT_ITEM_LIMIT) {
        return;
    }
    bucket = &svSortBuckets[bucketIndex];
    item = &svSortItems[itemIndex];
    /* The bucket head sits where an item keeps its next index, so the walk starts on the bucket. */
    prev = (SortItem *)bucket;
    if (bucket->head < 0) {
        bucket->head = itemIndex;
        bucket->tail = itemIndex;
        item->depth = depth;
        item->flags = flags;
        item->id = id;
    } else {
        item->id = id;
        item->flags = flags;
        item->depth = depth;
        for (;;) {
            if (prev->next < 0) {
                break;
            }
            nextIndex = prev->next;
            nextItem = &svSortItems[nextIndex];
            if (nextItem->depth < depth) {
                prev->next = itemIndex;
                item->next = nextIndex;
                break;
            }
            prev = nextItem;
        }
        if (prev->next < 0) {
            prev->next = itemIndex;
        }
    }
    svSortCount++;
}

/* Eight doublewords of GS state, one row of the draw state tables. */
typedef struct DrawStateWords {
    u64 words[8];
} DrawStateWords;
/* Read-only scaffold items of this TU from svAlphaTestTable up to svEffectProjectionMatrix: GS alpha and state tables, the identity-like matrix and the vector. */
const u64 svAlphaTestTable[4] = { 0x44ULL, 0x48ULL, 0x8000000042ULL, 0x6AULL };
const u64 svPairAlphaTable[4] = { 0x54ULL, 0x58ULL, 0x8000000052ULL, 0x6AULL };
const Matrix4 svUnitQuadCornerMatrix = {
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 1.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 1.0f, 0.0f },
    { 1.0f, 1.0f, 1.0f, 0.0f },
};
const DrawStateWords svDefaultDrawStateTable = { { 0x3F800000BF800000ULL, 0x3F80000000000000ULL, 0x3F8000003F800000ULL, 0x3F80000000000000ULL, 0xBF800000BF800000ULL, 0x3F80000000000000ULL, 0xBF8000003F800000ULL, 0x3F80000000000000ULL } };
const DrawStateWords svMode7DrawStateTable = { { 0xBF800000ULL, 0x3F8000003F800000ULL, 0x3F800000ULL, 0x3F8000003F800000ULL, 0xBF800000ULL, 0x3F800000BF800000ULL, 0x3F800000ULL, 0x3F800000BF800000ULL } };
const DrawStateWords svMode9DrawStateTable = { { 0x3F80000000000000ULL, 0x3F800000BF800000ULL, 0x3F80000000000000ULL, 0x3F8000003F800000ULL, 0xBF80000000000000ULL, 0x3F800000BF800000ULL, 0xBF80000000000000ULL, 0x3F8000003F800000ULL } };
const DrawStateWords svSpriteDrawStateTable = { { 0x42C80000C2C80000ULL, 0x3F80000000000000ULL, 0x42C8000042C80000ULL, 0x3F80000000000000ULL, 0xC2C80000C2C80000ULL, 0x3F80000000000000ULL, 0xC2C8000042C80000ULL, 0x3F80000000000000ULL } };
const Vector4 svDefaultVertexScale = { 0.0f, 0.0f, 0.0f, 1.0f };
const u64 svDrawRefUvWords[2] = { 0xC02A400000008000ULL, 0x525252521EEEULL };


/* The GS state block at the pointer in svDrawStatePacket: two matrices, one row of the draw state tables, a GIF tag with its
 * A+D register list and three A+D entries. */
typedef struct DrawStatePacket {
    Matrix4 matrixA;                        /* +0x00: the view product written by the matrix pipeline */
    Matrix4 baseMatrix;                     /* +0x40: the base matrix svDrawListAppendRecordA loads (the item matrix) */
    union {
        DrawStateWords stateWords;          /* +0x80: copied from the state table of the item's mode */
        DoublewordVector4 textureState[4];  /* the same row as four A+D entries */
        Vector4 corners[4];                 /* the same quadwords as the quad sefClipPolygon transforms */
    };
    u64 gif_tag;
    u64 gif_registers;
    DrawStateRegister clamp;
    DrawStateRegister zbuf;
    DrawStateRegister alpha;
} DrawStatePacket;

/* The two buffer addresses sefEntryCountsEntryGet hands back: the first one is used when the flag is set, the second one otherwise. */
typedef struct DrawBufferPair {
    u32 first;
    u32 second;
} DrawBufferPair;

extern DrawStatePacket *svDrawStatePacket;
extern const u64 svAlphaTestTable[];

/* Starts the draw list (svDrawList) in one of two buffers, chosen by the flag, with both cursors at zero. */
u8 *svDrawListBegin(int useFirst)
{
    DrawBufferPair *buffers;
    u8 *bufferBase;
    void *packetStart;

    buffers = sefEntryCountsEntryGet(57);
    if (useFirst != 0) {
        bufferBase = (u8 *) (buffers->first + SV_DRAW_LIST_FIRST_OFFSET);
    } else {
        if (buffers->second == 0) {
            return 0;
        }
        bufferBase = (u8 *) (buffers->second + SV_DRAW_LIST_SECOND_OFFSET);
    }
    svDrawList.entries = 0;
    /* the definition names this argument the quadword count; the caller passes the buffer address */
    packetStart = nmlPacketSetAttributeAlloc16N((int) bufferBase);
    if (packetStart == 0) {
        return 0;
    }
    svDrawList.entries = (Quadword *) (0x20000000 + (u32) packetStart);
    svDrawList.flush_code = SV_DRAW_LIST_FLUSH_CODE;
    svDrawList.end_index = 0;
    svDrawList.begin_index = 0;
    return bufferBase;
}

/*
 * Appends one draw record at the end cursor: a VIF FLUSH, STCYCL 4,4, an
 * UNPACK to VU address `vuAddress` and the operands. The record starts at the
 * old end, which becomes begin_index, and end_index moves on by 2.
 */
void svDrawListAppend(int vuAddress, int operand_a, int operand_b)
{
    DrawListEntry *record;

    svDrawList.begin_index = svDrawList.end_index;
    record = (DrawListEntry *) (svDrawList.entries + svDrawList.begin_index);
    record->payload = 0;
    record->flush_word = 0x11000000;
    record->nop_word = 0;
    record->stcycl_word = 0x01000404;
    record->unpack_word = vuAddress | 0x6C010000;
    record->count_word = 0;
    record->operand_a = operand_a;
    record->operand_b = operand_b;
    svDrawList.end_index += 2;
}

#include "quadword.h"

void sceVif1PkAddUpkData128(XglPacket *packet, Quadword word);
extern u8 D_80[];
extern u8 D_110[];
extern u32 svDrawRecordDivisors[];

/* Emits the VIF packet for `count` records at `entry`: sceVif1PkRef calls and UNPACK headers, split at 0x100 records. */
void svDrawListEmit(XglPacket *packet, DrawListEntry *records, int pending_count)
{
    DrawListEntry *header;
    int drawMode;
    int divisor;
    int countMinusOne;
    u32 sourceAddress;
    Quadword carriedQuad;

    drawMode = svDrawMode;
    sourceAddress = (u32) records & 0xfffffff;
    divisor = svDrawRecordDivisors[drawMode * 2];
    header = (DrawListEntry *) (svDrawList.entries + svDrawList.begin_index);
    countMinusOne = pending_count - 1;
    if (drawMode == 13) {
        header->count_word = (pending_count - 8) / divisor;
        header->operand_a = divisor;
    } else if (drawMode == 14) {
        header->count_word = ((pending_count - 14) / divisor) >> 1;
        header->operand_a = divisor;
    } else {
        header->count_word = countMinusOne / divisor;
        header->operand_a = divisor;
    }
    if (countMinusOne < 256) {
        header->unpack_word = (countMinusOne << 16) | 0x6C000028;
        sceVif1PkRef(packet, (const void *) sourceAddress, pending_count, 0, 0, 0);
        return;
    }
    if (countMinusOne == 256) {
        header->unpack_word = 0x6C000028;
        sceVif1PkRef(packet, (const void *) sourceAddress, pending_count, 0, 0, 0);
        return;
    }
    header->unpack_word = 0x6C000028;
    /* Record 257 (+0x1010) stays a byte offset: header[257] access changed the code. */
    carriedQuad = *(Quadword *) ((u8 *) header + 4112);
    sceVif1PkRef(packet, (const void *) sourceAddress, 257, 0, 0, 0);
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenUpkCode(packet, 296, 108, 4, 4);
    sceVif1PkAddUpkData128(packet, carriedQuad);
    sceVif1PkCloseUpkCode(packet);
    if (countMinusOne == 257) {
        return;
    }
    *(Quadword *) ((u8 *) header + 4112) = 0;
    *(int *) ((u8 *) header + 4124) = ((countMinusOne - 257) << 16) | 0x6C000129;
    sceVif1PkRef(packet, (const void *) ((u8 *) header + 4112), countMinusOne - 256, 0, 0, 0);
}

/*
 * Flushes the queued draw records. Runs only with at least three queued,
 * _draw3D != 2 and svDrawMode > 0: it emits the records, FLUSHes, calls the
 * draw microprogram of the current mode (MSCAL), then appends record 0x28.
 */
void svDrawListFlush(int pending_count)
{
    XglPacket *packet;
    u32 tag;
    DrawListEntry *entry;

    if (pending_count >= 3 && _draw3D != SV_RENDER_3D_MODEL && svDrawMode > 0) {
        packet = xglPacketGetCurrent();
        svDrawListEmit(packet,
                      (DrawListEntry *) (svDrawList.entries + svDrawList.begin_index),
                      pending_count);
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddCode(packet, 0x11000000);
        tag = svDrawModeTagTable[svDrawMode * 2];
        sceVif1PkAddCode(packet, (tag >> 3) | 0x14000000);
        sceVif1PkAddCode(packet, 0x11000000);
        entry = (DrawListEntry *) (svDrawList.entries + svDrawList.begin_index);
        svDrawListAppend(0x28, entry->operand_a, entry->operand_b);
    }
}

void svDrawListTryFlush(void)
{
    int pending_count;

    pending_count = svDrawList.end_index - svDrawList.begin_index;
    if (pending_count >= SV_DRAW_LIST_FLUSH_THRESHOLD) {
        svDrawListFlush(pending_count);
    }
}

void svDrawSendStaticRef(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceVif1PkRef(packet, svStaticRefData, svStaticRefQuadwordCount[0], 0, 0, 0);
}

#include "main/xgl_camera.h"

extern Vector4 svDefaultScreenParams[];
extern const Matrix4 svUnitQuadCornerMatrix;
extern SvViewUpload svViewUpload;
extern char D_50[];
/* The static VU1 view setup packet, nine quadwords. */
extern unsigned int svViewSetupPacket[];

/* One quadword through t0; an inline helper, so the copies the two arms below end with stay one asm statement. */
static inline void svViewCopyQuadword(Vector4 *destination, Vector4 *source)
{
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*destination), "+m"(*source) : "r"(destination), "r"(source) : "$8", "memory");
}

/*
 * Sends the view setup to VU1: the static reference svViewSetupPacket, then the view block (unit rotation, the
 * screen scale and offset of the active camera or, in 3D mode 1, the defaults, and svUnitQuadCornerMatrix) as ten
 * quadwords, then FLUSH and MSCAL of the microprogram at D_50.
 */
void svDrawSendViewSetup(void)
{
    XglCamera *camera;
    XglPacket *packet;

    camera = xglStudioGetActiveCamera();
    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceVif1PkRef(packet, svViewSetupPacket, 9, 0, 0, 0);
    __asm__ __volatile__(
        "lqc2 vf1, 32(%1)\n"
        "lqc2 vf2, 16(%1)\n"
        "lqc2 vf3, 0(%1)\n"
        "sqc2 vf0, 48(%0)\n"
        "sqc2 vf1, 32(%0)\n"
        "sqc2 vf2, 16(%0)\n"
        "sqc2 vf3, 0(%0)\n"
        : : "r"(svViewUpload.unit), "r"(MMathUnitMatrix) : "memory");
    QuadwordCopyMatrixT0(svViewUpload.view, svUnitQuadCornerMatrix);
    if (_draw3D == 1) {
        svViewCopyQuadword(&svViewUpload.scale, &svDefaultScreenParams[3]);
        svViewCopyQuadword(&svViewUpload.offset, &svDefaultScreenParams[2]);
    } else {
        svViewCopyQuadword(&svViewUpload.scale, &camera->screen.screenScale);
        svViewCopyQuadword(&svViewUpload.offset, &camera->screen.screenOffset);
    }
    svViewUpload.scale.z = svViewUpload.scale.z * 0.0625f;
    svViewUpload.scale.w = 0;
    svViewUpload.offset.z = svViewUpload.offset.z * 0.0625f;
    svViewUpload.offset.w = 0;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenUpkCode(packet, 10, 108, 4, 4);
    sceVif1PkAddUpkData128N(packet, &svViewUpload, 10);
    sceVif1PkCloseUpkCode(packet);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceVif1PkAddCode(packet, ((u32) D_50 >> 3) | 0x14000000);
}

void svDrawSetState(short repeatWrap, int alphaIndex)
{
    DrawStatePacket *packet;
    DrawStatePacket *state;

    packet = svDrawStatePacket;
    packet->gif_tag = 0x1000000000008003ULL;
    packet->gif_registers = 0x0E;
    if (repeatWrap != 0) {
        packet->clamp.data = 0;
    } else {
        packet->clamp.data = 5;
    }
    state = svDrawStatePacket;
    state->clamp.address = 0x08;
    state->zbuf.data = 0x9880ULL << 17;
    state->zbuf.address = 0x4E;
    state->alpha.data = svAlphaTestTable[alphaIndex];
    state->alpha.address = 0x42;
}

void svDrawSubmitMode(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    if (_draw3D == SV_RENDER_3D_MODEL) {
        return;
    }
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceVif1PkOpenUpkCode(packet, 20, 108, 4, 4);
    sceVif1PkAddUpkData128N(packet, svDrawStatePacket, 16);
    sceVif1PkCloseUpkCode(packet);
    sceVif1PkAddCode(packet, 0x11000000);
    if (svDrawMode < 5 || svDrawMode == 11 || svDrawMode == 12) {
        sceVif1PkAddCode(packet, ((u32) D_80 >> 3) | 0x14000000);
    } else {
        sceVif1PkAddCode(packet, ((u32) D_110 >> 3) | 0x14000000);
    }
}

/* VRAM size svAddImage reserves for a square 4-bit image of at most 16 pixels. */
#define SV_IMAGE_SQUARE_4BIT_VRAM 0x100
/* Mapper list sizes: list 0 is 0xE0000 bytes, the others 0xF0000; every list has capacity 0x10000. */
#define SV_LIST0_SIZE 0xE0000
#define SV_LIST_SIZE 0xF0000
#define SV_LIST_CAPACITY 0x10000
/* Bytes svAddClut advances the list by: a 256-colour or a 16-colour CLUT block. */
#define SV_CLUT_8BIT_BYTES 256
#define SV_CLUT_4BIT_BYTES 128

/*
 * One 0x24-byte entry of the working image list svImageListCreate clears
 * and svGetImageItem indexes; only its address is ever taken in this
 * allocation, so its fields stay unmodeled.
 */
typedef struct SvImageItem {
    unsigned char unmodeled_00[0x24];
} SvImageItem;

/*
 * The single working image list: a 0x1C-byte header (untouched by this
 * allocation) followed by 0x100 SvImageItem entries (0x1C + 0x100*0x24 =
 * 0x241C, svImageListCreate's own clear size).
 */
typedef struct SvImageList {
    unsigned char unmodeled_header[0x1C];
    SvImageItem items[SV_ENTRY_COUNT];
} SvImageList;

typedef struct SvReferenceImageSlot {
    unsigned int words[2];
} SvReferenceImageSlot;

/* _imageMapper runs from 0x3ED680 to sefGlobalTransform (0x80A40 bytes): 56 whole type lists and 0x2420 bytes more.
 * svInitImageMapper clears and sets up 57 lists (57 * 0x241C bytes, 4 more than the object), so how the
 * last list and the tail divide is not settled. */
typedef struct SvImageMapperStorage {
    SvTypeList typeLists[56];
    unsigned char unmodeled_tail[0x2420];
} SvImageMapperStorage;

SvTypeList *svGetTypeList(int type);
typedef struct SvImageListStorage {
    unsigned char header[0x1C];
    SvImageItem items[SV_ENTRY_COUNT];
    unsigned char gap[0x28];
    float table[1024];
    unsigned char tail[12];
} SvImageListStorage;
static SvImageListStorage _imageList;
extern SvImageMapperStorage _imageMapper;

void *svGetImageItem(int index) {
    return &((SvImageList *) &_imageList)->items[index];
}

void svImageListCreate(void) {
    memset(&_imageList, 0, sizeof(SvImageList));
}

/* Releases every entry of a type list (the used flags), then resets its counters, tag and dimensions. */
void svImageListDestroy(SvTypeList *typeList) {
    int index;

    for (index = 0; index < SV_ENTRY_COUNT; index++) {
        if (typeList->entries[index].used != 0) {
            typeList->entries[index].used = 0;
        }
    }
    typeList->savedSize = typeList->size;
    typeList->total = 0;
    typeList->entryCount = 0;
    typeList->tag = 0;
    typeList->dimensions = 0;
    typeList->pendingCount = 0;
}

int svImageListAlloc(SvTypeList *typeList)
{
    int index;

    for (index = 0; index < SV_ENTRY_COUNT; index++) {
        if (typeList->entries[index].used == 0) {
            return index;
        }
    }
    return -1;
}

SvTypeList * svGetTypeList(int type) {
    return &_imageMapper.typeLists[type];
}

SvTypeListEntry *svGetImageListItemSub(int type, unsigned int flags)
{
    SvTypeList *typeList;
    SvTypeListEntry *entry;
    int index;

    typeList = svGetTypeList(type);
    entry = typeList->entries;
    index = 0;
    while (index < SV_ENTRY_COUNT) {
        if (entry->used != 0 && entry->type == (unsigned char)flags) {
            return entry;
        }
        entry++;
        index++;
    }
    return 0;
}

/* Looks the list up with the type 0 for flag 0x100, else with the given type; a negative type returns 0. */
SvTypeListEntry *svGetImageListItem(int type, unsigned int flags) {
    if ((flags & 0x100) != 0) {
        type = 0;
    }
    if (type < 0) {
        return 0;
    }
    return svGetImageListItemSub(type, flags);
}

/* Scans the type list `type` for an active entry whose name equals `name`; returns the entry or 0. */
SvTypeListEntry *svGetResFromName(const char *name, int type) {
    SvTypeListEntry *entry;
    int index;

    entry = svGetTypeList(type)->entries;
    for (index = 0; index < SV_ENTRY_COUNT; index++) {
        if (entry->used != 0 && strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry++;
    }
    return 0;
}

extern int strcmp(const char *left, const char *right);
extern int strlen(const char *string);
extern char *strcpy(char *destination, const char *source);

typedef struct SvSuffix {
    char text[4];
} SvSuffix;

/*
 * Builds the texture parameters of a bitmap and its CLUT from a base name,
 * looking the resource names up with svGetResFromName (name pieces "bmp",
 * "pxy", "clt", "txy"): texture register bits, width and height scaled by
 * 0.01. Clears the parameter and returns 0 when either resource is missing.
 */
SvTexParam *svGetPrmFromName(const char *resourceName, int type, SvTexParam *parameter) {
    char pairedName[128];
    SvTypeListEntry *primary;
    SvTypeListEntry *companion;
    const char *suffix;
    int stemLength;
    int isPxy;
    int pixelFormat;

    memset(parameter, 0, sizeof(*parameter));
    isPxy = 0;
    stemLength = strlen(resourceName);
    if (stemLength < 4) {
        return 0;
    }
    stemLength -= 3;
    suffix = resourceName + stemLength;
    if (strcmp(suffix, "bmp") == 0) {
        strcpy(pairedName, resourceName);
        *(SvSuffix *)(pairedName + stemLength) = *(const SvSuffix *)"clt";
    } else if (strcmp(suffix, "pxy") == 0) {
        strcpy(pairedName, resourceName);
        *(SvSuffix *)(pairedName + stemLength) = *(const SvSuffix *)"txy";
        isPxy = 1;
    } else {
        return 0;
    }
    primary = svGetResFromName(resourceName, type);
    companion = svGetResFromName(pairedName, type);
    if (primary != 0 && companion != 0) {
        if (isPxy != 0) {
            /* The two pointer locals stay: direct field stores changed the code. */
            PxyHeader *primaryPointer = primary->data;
            TxyBlock *companionPointer = companion->data;

            parameter->primaryData = primaryPointer;
            parameter->companionData = companionPointer;
        } else {
            if (primary->bitsPerPixel == 8) {
                pixelFormat = GS_PSMT8;
            } else {
                pixelFormat = GS_PSMT4;
            }
            parameter->tex0 = primary->basePointer
                | ((u64)(primary->bufferWidthPixels >> 6) << 14)
                | ((u64)pixelFormat << 20)
                | ((u64)primary->textureWidthExponent << 26)
                | ((u64)primary->textureHeightExponent << 30)
                | (1ULL << 34) /* TCC: RGBA */
                | ((u64)companion->basePointer << 37)
                | (1ULL << 61); /* CLD: load the CLUT */
            parameter->scaledWidth = primary->width * 0.01f;
            parameter->scaledHeight = primary->height * 0.01f;
        }
        return parameter;
    }
    memset(parameter, 0, sizeof(*parameter));
    return 0;
}

/* Small data after the bitmap name strings of svGetPrmFromName: the draw path flag and the visible entry count. */
short svPolygonClipFlag = 0;
int svVisibleEntryCount = 0;

/* Number of 128-pixel pages of a texture: 128x64 pages for PSMT8, 128x128 for PSMT4, width * height / 128 otherwise. */
int svGetTexturePageCount(int pixelFormat, int width, int height) {
    int pages = 0;

    switch (pixelFormat) {
    case GS_PSMT8: { /* 128x64 pages */
        int roundedWidth;

        roundedWidth = width + 127;
        pages = roundedWidth / 128;
        pages = pages * ((height + 63) / 64);
        return pages;
    }
    case GS_PSMT4: /* 128x128 pages */
        return ((width + 127) / 128) * ((height + 127) / 128);
    default:
        return (width * height) / 128;
    }
    return pages;
}

/* Buffer width of a texture in 64-pixel units, rounded up to an even number, for PSMT8 and PSMT4; 0 for other formats. */
int svGetTextureBufferWidth(int pixelFormat, int width) {
    if (pixelFormat <= GS_PSMT4) {
        if (pixelFormat >= GS_PSMT8) {
            width = (width + 63) / 64;
            return width + (width & 1);
        }
    }
    return 0;
}

int sceVif1PkRefLoadImage(void *packet, u16 basePointer, int psm, int bufferWidth, const void *data, int quadwordCount, int, int, int, int);

void svLoadTexture(SvTypeListEntry *entry) {
    int psm;
    int quadwordCount;
    u16 basePointer;
    u16 width;
    u16 height;
    void *packet;

    height = entry->height;
    basePointer = entry->basePointer;
    width = entry->width;
    psm = GS_PSMT8;
    if (entry->bitsPerPixel != 8) {
        psm = GS_PSMT4;
    }
    quadwordCount = (entry->bufferWidthPixels * height) / 16;
    packet = xglPacketGetCurrent();
    sceVif1PkRefLoadImage(packet, basePointer, psm, (entry->bufferWidthPixels >> 6) + 1, entry->data, quadwordCount, 0, 0, width, height);
}

/* Uploads a palette: 16x16 for 8-bit entries, 8x2 for 4-bit, to the current packet. */
void svLoadClut(SvTypeListEntry *entry) {
    u16 basePointer;
    void *packet;

    basePointer = entry->basePointer;
    packet = xglPacketGetCurrent();
    if (entry->bitsPerPixel == 8) {
        sceVif1PkRefLoadImage(packet, basePointer, 0, 1, entry->data, 0x40, 0, 0, 0x10, 0x10);
        return;
    }
    sceVif1PkRefLoadImage(packet, basePointer, 0, 1, entry->data, 4, 0, 0, 8, 2);
}

int svLoadImageList(SvTypeList *typeList) {
    int index;
    unsigned int imageCount;

    imageCount = 0;
    for (index = 0; index < SV_ENTRY_COUNT; index++) {
        if (typeList->entries[index].used) {
            if (typeList->entries[index].resourceKind == SV_RES_IMAGE) {
                if (imageCount == 0) {
                    void *packet;

                    packet = xglPacketGetCurrent();
                    sceVif1PkCnt(packet, 0);
                    sceVif1PkAddCode(packet, 0x11000000);
                }
                imageCount++;
                svLoadTexture(&typeList->entries[index]);
            } else if (typeList->entries[index].resourceKind == SV_RES_CLUT) {
                svLoadClut(&typeList->entries[index]);
            }
        }
    }
    return imageCount;
}

/*
 * One svAddScript2 sub-record, at +0x1C of a 0x24-byte slot of the type
 * list's item array. Only the fields this function itself writes are
 * modeled, at their byte offset from this sub-record's own base: `codePtr`
 * (+0x00, the script's own +0xC0 address), `active` (+0x08, always 1),
 * `kind` (+0x0C, always 5, matching this function's own "script" name),
 * `id` (+0x0E, the script's own +0xA halfword), two cleared halfwords
 * (+0x18/+0x1A) and `dataPtr` (+0x20, the script's own +0x14 address).
 * +0x20 (slot +0x3C) lands past the slot's own 0x24-byte stride, into the
 * next slot's leading bytes -- evidenced by the original bytes, not
 * treated as a bug.
 */
typedef struct SvScriptSlot {
    void *codePtr;                  /* 0x00 */
    unsigned char unmodeled_04[4];
    int active;                      /* 0x08 */
    short kind;                       /* 0x0C */
    short id;                          /* 0x0E */
    unsigned char unmodeled_10[8];
    short clearedA;                     /* 0x18 */
    short clearedB;                      /* 0x1A */
    unsigned char unmodeled_1C[4];
    void *dataPtr;                        /* 0x20 */
} SvScriptSlot;

int svAddModel(SvTypeList *typeList, SvResourceChunk *chunk);
int svAddScript(SvTypeList *typeList, SvResourceChunk *chunk);

static int svGetSizeBit(unsigned int value)
{
    unsigned int mask;
    int bitIndex;

    mask = 2;
    bitIndex = 1;
    while (bitIndex < 32) {
        if ((value & mask) != 0) {
            return bitIndex;
        }
        mask <<= 1;
        bitIndex++;
    }
    return 0;
}

/* Registers an image chunk in the type list: its pixels follow the 0x40-byte header. The VRAM size it reserves is
 * the chunk's byte size times half the ratio of its longer side to its shorter one (a square 4-bit image of at most
 * 16 pixels takes 0x100), and its base pointer is the list's running size in 64-byte units. */
int svAddImage(SvTypeList *list, SvImageChunk *image)
{
    SvTypeListEntry *entry;
    unsigned int width;
    unsigned int height;
    unsigned int storageSize;
    int index;

    index = svImageListAlloc(list);
    if (index < 0) {
        return -1;
    }
    entry = &list->entries[index];
    entry->resourceKind = SV_RES_IMAGE;
    entry->data = image + 1;
    entry->typeFlags = image->typeFlags;
    entry->used = 1;
    entry->width = image->width;
    width = entry->width;
    entry->height = image->height;
    entry->bitsPerPixel = image->bitsPerPixel;
    entry->textureWidthExponent = svGetSizeBit(width);
    height = entry->height;
    entry->textureHeightExponent = svGetSizeBit(height);
    entry->name = image->name;
    storageSize = image->dataSize;
    if (image->bitsPerPixel == 4) {
        unsigned long comparedHeight; /* height widened for the side comparisons */

        entry->bufferWidthPixels = (short)image->width >> 1;
        comparedHeight = height;
        if (comparedHeight != width) {
            if (comparedHeight < width) {
                storageSize *= (width / height) >> 1;
            } else {
                storageSize *= (height / width) >> 1;
            }
        } else if (comparedHeight <= 16) {
            storageSize = SV_IMAGE_SQUARE_4BIT_VRAM;
        }
    } else {
        unsigned long comparedHeight; /* height widened for the side comparisons */

        comparedHeight = height;
        if (comparedHeight != width) {
            if (comparedHeight < width) {
                storageSize *= (width / height) >> 1;
            } else {
                storageSize *= (height / width) >> 1;
            }
        }
        if (entry->height > entry->width) {
            entry->bufferWidthPixels = (short)image->width * (int)height / (int)width;
        } else {
            entry->bufferWidthPixels = image->width;
        }
    }
    entry->basePointer = list->savedSize >> 6;
    list->savedSize += storageSize >> 2;
    return index;
}

/* Adds a CLUT chunk as a list entry and advances savedSize by 256 (8 bits per pixel) or 128. Returns the entry
 * index, or -1 when the list is full. */
int svAddClut(SvTypeList *typeList, SvClutChunk *chunk)
{
    SvTypeListEntry *entry;
    int index;

    index = svImageListAlloc(typeList);
    if (index < 0) {
        return -1;
    }
    entry = &typeList->entries[index];
    entry->data = chunk->data;
    entry->used = 1;
    entry->resourceKind = SV_RES_CLUT;
    entry->typeFlags = chunk->typeFlags;
    entry->width = chunk->width;
    entry->height = chunk->height;
    entry->bitsPerPixel = chunk->bitsPerPixel;
    entry->textureWidthExponent = svGetSizeBit(entry->width);
    entry->textureHeightExponent = svGetSizeBit(entry->height);
    entry->bufferWidthPixels = (short)chunk->width * 4;
    entry->name = chunk->name;
    entry->basePointer = typeList->savedSize >> 6;
    typeList->savedSize += entry->bitsPerPixel == 8 ? SV_CLUT_8BIT_BYTES : SV_CLUT_4BIT_BYTES;
    return index;
}

int svAddModel(SvTypeList *typeList, SvResourceChunk *chunk)
{
    SvTypeListEntry *entry;
    unsigned short typeFlags;
    int index;

    index = svImageListAlloc(typeList);
    if (index < 0) {
        return -1;
    }
    entry = &typeList->entries[index];
    entry->resourceKind = SV_RES_MODEL;
    entry->name = &chunk->payload[8];
    typeFlags = chunk->typeFlags | 0x2000;
    entry->data = &chunk->payload[sizeof(chunk->payload)];
    entry->typeFlags = typeFlags;
    entry->used = 1;
    entry->height = 0;
    entry->width = 0;
    return index;
}

int svAddScript(SvTypeList *typeList, SvResourceChunk *chunk)
{
    SvTypeListEntry *entry;
    unsigned short typeFlags;
    int index;

    index = svImageListAlloc(typeList);
    if (index < 0) {
        return -1;
    }
    entry = &typeList->entries[index];
    entry->resourceKind = SV_RES_SCRIPT;
    entry->name = &chunk->payload[8];
    typeFlags = chunk->typeFlags;
    entry->data = &chunk->payload[sizeof(chunk->payload)];
    entry->typeFlags = typeFlags;
    entry->used = 1;
    entry->height = 0;
    entry->width = 0;
    return index;
}

/* Registers a script resource of the list: its code sits at +0xC0 of the chunk, its data at +0x14, its id (typeFlags) at +0xA. */
int svAddScript2(SvTypeList *typeList, SvResourceChunk *script)
{
  int index;
  void *codePtr;
  void *dataPtr;
  SvScriptSlot *scriptSlot;

  index = svImageListAlloc(typeList);
  codePtr = (unsigned char *) script + 0xC0;
  dataPtr = (unsigned char *) script + 0x14;
  if (index < 0)
  {
    return -1;
  }
  scriptSlot = (SvScriptSlot *) &typeList->entries[index];
  scriptSlot->kind = 5;
  scriptSlot->dataPtr = dataPtr;
  scriptSlot->codePtr = codePtr;
  scriptSlot->id = (short) script->typeFlags;
  scriptSlot->active = 1;
  scriptSlot->clearedB = 0;
  scriptSlot->clearedA = 0;
  return index;
}

void svAnalyzeChunk(SvTypeList *list, void *chunkFile)
{
    SvResourceChunk *chunk;

    chunk = *(SvResourceChunk **) chunkFile;
    while (chunk->kind != 0) {
        switch (chunk->kind) {
        case 1:
        case 2:
            chunk = chunk + 1;
            continue;
        case SV_RES_IMAGE:
            svAddImage(list, (SvImageChunk *) chunk);
            chunk = (SvResourceChunk *) ((unsigned char *) chunk + chunk->byteSize);
            continue;
        case SV_RES_CLUT:
            svAddClut(list, (SvClutChunk *) chunk);
            chunk = (SvResourceChunk *) ((unsigned char *) chunk + chunk->byteSize);
            continue;
        case 7:
            svAddImage(list, (SvImageChunk *) chunk);
            chunk = (SvResourceChunk *) ((unsigned char *) chunk + chunk->byteSize);
            continue;
        case SV_RES_MODEL:
        case 9:
            svAddModel(list, chunk);
            chunk = (SvResourceChunk *) ((unsigned char *) chunk + chunk->byteSize);
            continue;
        case SV_RES_SCRIPT:
            svAddScript(list, chunk);
            chunk = (SvResourceChunk *) ((unsigned char *) chunk + chunk->byteSize);
            continue;
        case 10:
            svAddScript2(list, chunk);
            chunk = (SvResourceChunk *) ((unsigned char *) chunk + chunk->byteSize);
            continue;
        default:
            return;
        }
    }
}

/* Clears the 57 type lists of _imageMapper and gives each its sizes: list 0 starts with 0xE0000, the others with
 * 0xF0000 (lists 1..8 and the rest take separate, identical arms), and every capacity is 0x10000, set in each arm. */
void svInitImageMapper(void)
{
    unsigned int size;
    int capacity;
    int listIndex;

    memset(&_imageMapper, 0, 57 * sizeof(SvTypeList));
    for (listIndex = 0; listIndex < 57; listIndex++) {
        if (listIndex <= 0) {
            size = SV_LIST0_SIZE;
            capacity = SV_LIST_CAPACITY;
        } else if (listIndex < 9) {
            size = SV_LIST_SIZE;
            capacity = SV_LIST_CAPACITY;
        } else {
            size = SV_LIST_SIZE;
            capacity = SV_LIST_CAPACITY;
        }
        _imageMapper.typeLists[listIndex].size = size;
        _imageMapper.typeLists[listIndex].savedSize = size;
        _imageMapper.typeLists[listIndex].capacity = capacity;

        _imageMapper.typeLists[listIndex].pendingCount = 0;
        _imageMapper.typeLists[listIndex].entryCount = 0;
        _imageMapper.typeLists[listIndex].tag = 0;
    }
}

void svAddImageMapper(int typeListId, int entryCount, void *chunk, int tag) {
    SvTypeList *list;

    list = svGetTypeList(typeListId);
    if (list->pendingCount != 0) {
        svImageListDestroy(list);
    }
    list->tag = tag;
    list->entryCount = entryCount;
    list->pendingCount = list->pendingCount + 1;
    svAnalyzeChunk(list, chunk);
}

void svDeleteImageMapper(int typeListId) {
    SvTypeList *list;

    list = svGetTypeList(typeListId);
    if (list->pendingCount != 0) {
        svImageListDestroy(list);
    }
}

void svDeleteImageMapperID(int entryCount) {
    int listIndex;

    for (listIndex = 1; listIndex < 9; listIndex++) {
        if (_imageMapper.typeLists[listIndex].entryCount == entryCount) {
            svImageListDestroy(&_imageMapper.typeLists[listIndex]);
            return;
        }
    }
}

void svDeleteImageMapperByDimensions(int dimensions) {
    int listIndex;

    for (listIndex = 9; listIndex < 58; listIndex++) {
        if (_imageMapper.typeLists[listIndex].dimensions == dimensions) {
            svImageListDestroy(&_imageMapper.typeLists[listIndex]);
            return;
        }
    }
}

void svDeleteImageMapperData(int tag) {
    int listIndex;

    for (listIndex = 1; listIndex < 9; listIndex++) {
        if (_imageMapper.typeLists[listIndex].tag == tag) {
            svImageListDestroy(&_imageMapper.typeLists[listIndex]);
            return;
        }
    }
}

#define SRS_KEPT_KEY_COUNT 6
/* The type list whose entry 0 is a direct effect (svGetScript returns it without a name search). */
#define SV_DIRECT_ENTRY_LIST 9
/* svSetZTestEnv's TEST_1 words: Z test on (GEQUAL) and always pass. */
#define SV_ZTEST_ON 0x5000D
#define SV_ZTEST_OFF 0x2000D

/* Returns the first free effect slot, or -1 when all are in use. */
int srsFindFreeEffectSlot(void)
{
    int index;

    for (index = 0; index < SRS_SLOT_COUNT; index++) {
        if (srsRecordTable.slots[index].handle == 0) {
            return index;
        }
    }
    return -1;
}


/* Clears effect slot index: frees the buffer its record owns (by the record's kind), then
 * zeroes the record. */
void srsFreeEffectSlot(unsigned int index)
{
    if (index < SRS_SLOT_COUNT) {
        if (srsRecordTable.slots[index].handle != 0) {
            if (srsRecordTable.slots[index].kind == SRS_KIND_SM_BLOCK) {
                smFree(srsRecordTable.slots[index].handle);
            } else if (srsRecordTable.slots[index].kind == SRS_KIND_GAME_RES_BLOCK) {
                GameResBlockFreeByBuffer(srsRecordTable.slots[index].handle);
            }
        }
        memset(&srsRecordTable.slots[index], 0, sizeof(SrsRecord));
    }
}

/* Unloads effect slot `index`: deletes its image list (index + 9), then frees the slot. */
void srsUnloadEffectSlot(unsigned int index)
{
    if (index < SRS_SLOT_COUNT) {
        svDeleteImageMapper(index + SRS_KEY_RECORD_COUNT);
        srsFreeEffectSlot(index);
    }
}


/* Unloads the least recently used slot whose owner-only effect is not busy and whose kind is
 * 0. Returns its index, or -1 when no slot qualifies. */
int srsEvictEffectSlot(void)
{
    int index;
    int victim;
    int key;

    victim = SRS_SLOT_COUNT;
    srsRecordTable.slots[SRS_SLOT_COUNT].stamp = -1;
    for (index = 0; index < SRS_SLOT_COUNT; index++) {
        if (srsRecordTable.slots[index].handle == 0 ||
            srsRecordTable.slots[index].busy != 0 ||
            srsRecordTable.slots[index].kind != 0) {
            continue;
        }
        key = srsRecordTable.slots[index].key;
        if (!sefIsFinishEffect2(key)) {
            continue;
        }
        if (srsRecordTable.slots[index].stamp <=
            srsRecordTable.slots[victim].stamp) {
            victim = index;
        }
    }
    if (victim != SRS_SLOT_COUNT) {
        srsUnloadEffectSlot(victim);
        return victim;
    }
    return -1;
}


/* Evicts least recently used slots until a block of `size` can be allocated. Returns the
 * allocation, or the negative eviction result when no slot is left to evict. */
int srsEvictEffectSlotsForSize(int size)
{
    int evicted;
    int allocation;

    evicted = 0;
    while (evicted >= 0) {
        evicted = srsEvictEffectSlot();
        if (evicted < 0) {
            return evicted;
        }
        allocation = (int)smFindFree(size); /* the free cell address; the function returns it or a negative result */
        if (allocation != 0) {
            break;
        }
    }
    return allocation;
}

extern int srsKeptEffectKeys[];

/* Returns 1 when key is one of the six keys in srsKeptEffectKeys, else 0. */
int srsIsEffectKeyListed(int key)
{
    unsigned int index;

    for (index = 0; index < SRS_KEPT_KEY_COUNT; index++) {
        if (srsKeptEffectKeys[index] == key) {
            return 1;
        }
    }
    return 0;
}

/* Unloads every in-use effect slot that is not owned by the current owner, not busy, not of
 * kind 2 and not listed in srsKeptEffectKeys. */
void srsReleaseOtherOwnerEffectSlots(void)
{
    int index;
    int currentOwner;

    currentOwner = sefGetCurrentOwner();
    for (index = 0; index < SRS_SLOT_COUNT; index++) {
        if (srsRecordTable.slots[index].handle == 0) {
            continue;
        }
        if (srsRecordTable.slots[index].busy != 0) {
            continue;
        }
        if (srsRecordTable.slots[index].kind == SRS_KIND_CALLER_BUFFER) {
            continue;
        }
        if (srsRecordTable.slots[index].owner == currentOwner) {
            continue;
        }
        if (srsIsEffectKeyListed(srsRecordTable.slots[index].key)) {
            continue;
        }
        if (sefIsFinishEffect2(srsRecordTable.slots[index].key)) {
            srsUnloadEffectSlot(index);
        }
    }
}


/* Makes effect effectId (a type 9 effect) resident in an effect slot. An effect already in a slot only takes
 * the current owner and a new stamp (0). Otherwise a free slot is taken, after releasing other owners'
 * slots (owner bits 28..29 set) or evicting slots for the file size; the slot uses buffer when given (kind
 * 2), else a buffer from smAlloc (kind 0) or, for owner mode 1, GameResBufferAlloc (kind 1). Returns 1 when
 * the data load starts, -1 on a bad id or a failed load, -2 when no slot or buffer is free (a mode 0 effect
 * is then queued with sefSetLoadQue). */
int srsLoadEffectSlot(void *buffer, int effectId, int arg)
{
    int charNo;
    int effectType;
    int owner;
    int ownerMode;
    int size;
    int index;
    int slot;
    char *effectName;

    if (effectId == 0) {
        return -1;
    }
    effectId = sefCanonicalEffectId(effectId);
    srsAnalyzeEftNo(effectId, &charNo, &effectType);
    if (effectType == 9) {
        owner = sefGetCurrentOwner();
        ownerMode = (effectId >> 28) & 3;
        size = 0;
        for (index = 0; index < SRS_SLOT_COUNT; index++) {
            if (srsRecordTable.slots[index].handle != 0 && srsRecordTable.slots[index].key == effectId) {
                srsRecordTable.slots[index].owner = owner;
                srsRecordTable.slots[index].stamp = ++srsEffectStamp;
                return 0;
            }
        }
        effectName = srsGetEffectName(effectId);
        if (buffer == 0) {
            size = srsGetFileLen(effectName);
            if (size == 0) {
                return -1;
            }
        }
        slot = srsFindFreeEffectSlot();
        if (slot < 0 || (buffer == 0 && smFindFree(size) == 0)) {
            if (ownerMode != 0) {
                srsReleaseOtherOwnerEffectSlots();
            } else {
                srsEvictEffectSlotsForSize(size);
            }
            slot = srsFindFreeEffectSlot();
        }
        if (slot < 0) {
            if (ownerMode != 0) {
                return -2;
            }
            sefSetLoadQue(effectId, arg);
            return -2;
        }
        memset(&srsRecordTable.slots[slot], 0, sizeof(SrsRecord));
        if (buffer != 0) {
            srsRecordTable.slots[slot].handle = buffer;
            srsRecordTable.slots[slot].kind = SRS_KIND_CALLER_BUFFER;
            srsRecordTable.slots[slot].owner = 0;
            srsRecordTable.slots[slot].arg = 0;
            srsRecordTable.slots[slot].stamp = ++srsEffectStamp;
        } else {
            srsRecordTable.slots[slot].handle = smAlloc(size);
            if (srsRecordTable.slots[slot].handle != 0) {
                srsRecordTable.slots[slot].kind = SRS_KIND_SM_BLOCK;
                srsRecordTable.slots[slot].owner = owner;
                srsRecordTable.slots[slot].arg = arg;
                srsRecordTable.slots[slot].size = size;
                srsRecordTable.slots[slot].stamp = ++srsEffectStamp;
            } else {
                if (((effectId >> 28) & 3) == 1) {
                    srsRecordTable.slots[slot].handle = GameResBufferAlloc(size);
                    if (srsRecordTable.slots[slot].handle != 0) {
                        srsRecordTable.slots[slot].kind = (effectId >> 28) & 3;
                        srsRecordTable.slots[slot].owner = owner;
                        srsRecordTable.slots[slot].arg = arg;
                        srsRecordTable.slots[slot].size = size;
                        srsRecordTable.slots[slot].stamp = ++srsEffectStamp;
                    }
                }
                if (srsRecordTable.slots[slot].handle == 0) {
                    if (ownerMode != 0) {
                        return -2;
                    }
                    sefSetLoadQue(effectId, arg);
                    return -2;
                }
            }
        }
        srsRecordTable.slots[slot].busy = 1;
        srsRecordTable.slots[slot].key = effectId;
        if (srsLoadEffectData(srsRecordTable.slots[slot].handle, effectId) < 0) {
            srsFreeEffectSlot(slot);
            return -1;
        }
        return 1;
    }
    return -1;
}

void sefInitClipViewVolume(void *view);
int strncmp(const char *left, const char *right, unsigned int length);

/* Finds the entry for an effect id: entry 0 of a mapper from index 9 on when it is used and has a kind, else the first script entry whose name matches the effect's stem. */
SvTypeListEntry *svGetScript(int effectId)
{
    int canonicalId;
    int typeListIndex;
    SvTypeList *typeList;
    char stem[0x100];
    int entryIndex;
    SvTypeListEntry *entry;

    if (effectId == 0) {
        return 0;
    }
    canonicalId = sefCanonicalEffectId(effectId);
    typeListIndex = sefSearchMapperIndex(canonicalId);
    if (typeListIndex < 0) {
        return 0;
    }
    typeList = &_imageMapper.typeLists[typeListIndex];
    if (typeListIndex >= SV_DIRECT_ENTRY_LIST && typeList->entries[0].used != 0 && typeList->entries[0].resourceKind != 0) {
        return &typeList->entries[0];
    }
    srsFormatEffectStem(canonicalId, stem, 1);
    for (entryIndex = 0; entryIndex < SV_ENTRY_COUNT; entryIndex++) {
        if (typeList->entries[entryIndex].used != 0 && typeList->entries[entryIndex].resourceKind == SV_RES_SCRIPT
            && typeList->entries[entryIndex].name != (const char *) -1) {
            if (strncmp(stem, typeList->entries[entryIndex].name, strlen(stem)) == 0) {
                return &typeList->entries[entryIndex];
            }
        }
    }
    return 0;
}

/* One key of the item's key table: kind and parameter (4 bytes). */
typedef struct SvKeyHeader {
    short kind;
    unsigned short parameter;
} SvKeyHeader;

typedef struct SvDrawItem {
    unsigned char unmodeled_00[0x180];
    short *entries;
    unsigned char unmodeled_184[0x48];
    unsigned short imageFlags; /* +0x1CC: flags of the first image packet (sef.c imageFlags) */
    unsigned char unmodeled_1CE[0x674 - 0x1CE];
    short *s19Keys;            /* +0x674: s19Keys[-1] holds the mode bits */
    unsigned char unmodeled_678[0x67C - 0x678];
    SvKeyHeader *keyHeaders;   /* +0x67C: the key header before entries 0, kind read by svDrawImageItem */
    unsigned char unmodeled_680[0x684 - 0x680];
    signed char *fogTable;     /* +0x684: fogTable[15] feeds svFogValue */
    unsigned char unmodeled_688[0x6A0 - 0x688];
    short pointHandles[0x100]; /* +0x6A0: one slot handle per row, -1 when unused (sef.c pointHandles) */
    unsigned char unmodeled_8A0[0x8A4 - 0x8A0];
    unsigned short modeFlags;  /* +0x8A4 */
    unsigned short count;      /* +0x8A6: the item's count (line data view) */
    unsigned char unmodeled_8A8[0x8AA - 0x8A8];
    short fallbackKind;
} SvDrawItem;

void svDrawScene(void);
extern unsigned char svMapperPacketEnv[];

/* Refers the current packet to the three-quadword environment block svMapperPacketEnv; runs after every mapper switch. */
void svRefMapperPacket(void) {
    sceVif1PkRef(xglPacketGetCurrent(), svMapperPacketEnv, 3, 0, 0, 0);
}

typedef struct SvDrawTagArea {
    unsigned char unmodeled_00[0x20];
    u64 tag;
    unsigned char unmodeled_28[0x18];
} SvDrawTagArea;

extern SvDrawTagArea svZTestPacketEnv;

void svSetZTestEnv(int enable) {
    if (enable != 0) {
        svZTestPacketEnv.tag = SV_ZTEST_ON;
    } else {
        svZTestPacketEnv.tag = SV_ZTEST_OFF;
    }
    svZTestEnable = enable;
    svDrawTestWord = svZTestPacketEnv.tag;
}

void sefInitClipViewVolume(void *view) {
    Vu0SetClipMatrices(view);
}

extern s16 _sefBattleMode;

/*
 * sefClipViewVolumeA: the whole VU0 clip test as one hand-written block (the flag
 * gate, the six lane compares against limit and the set-clear join).
 */
int sefClipViewVolumeA(const Vector4 *model, float limit)
{
    __asm__ __volatile__(
        ".set noreorder\n\t"
        "lh $3, %0\n\t"
        "daddu $5, $4, $0\n\t"
        "bne $3, $0, 4f\n\t"
        "daddu $2, $0, $0\n\t"
        "lqc2 vf20, 0($5)\n\t"
        "mfc1 $4, $f12\n\t"
        "qmtc2 $4, vf2\n\t"
        "mtc1 $4, $f1\n\t"
        "vaddx.x vf2x, vf2x, vf20x\n\t"
        "vmulax.xyzw ACC, vf16, vf20x\n\t"
        "vmadday.xyzw ACC, vf17, vf20y\n\t"
        "vmaddaz.xyzw ACC, vf18, vf20z\n\t"
        "vmaddw.xyzw vf9, vf19, vf0w\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "sll $0, $0, 0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "sll $0, $0, 0\n\t"
        "bc1f 1f\n\t"
        "sll $0, $0, 0\n\t"
        "sll $0, $0, 0\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "sll $0, $0, 0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "sll $0, $0, 0\n\t"
        "bc1f 1f\n\t"
        "sll $0, $0, 0\n\t"
        "sll $0, $0, 0\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "sll $0, $0, 0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "sll $0, $0, 0\n\t"
        "bc1f 1f\n\t"
        "sll $0, $0, 0\n\t"
        "sll $0, $0, 0\n\t"
        "vmulax.xyzw ACC, vf12, vf20x\n\t"
        "vmadday.xyzw ACC, vf13, vf20y\n\t"
        "vmaddaz.xyzw ACC, vf14, vf20z\n\t"
        "vmaddw.xyzw vf9, vf15, vf0w\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "sll $0, $0, 0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "sll $0, $0, 0\n\t"
        "bc1f 1f\n\t"
        "sll $0, $0, 0\n\t"
        "sll $0, $0, 0\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "sll $0, $0, 0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "sll $0, $0, 0\n\t"
        "bc1f 1f\n\t"
        "sll $0, $0, 0\n\t"
        "sll $0, $0, 0\n\t"
        "vmr32.xyzw vf9, vf9\n\t"
        "qmfc2 $4, vf9\n\t"
        "mtc1 $4, $f0\n\t"
        "sll $0, $0, 0\n\t"
        "c.lt.s $f0, $f1\n\t"
        "sll $0, $0, 0\n\t"
        "bc1f 1f\n\t"
        "sll $0, $0, 0\n\t"
        "sll $0, $0, 0\n\t"
        "sub $4, $4, $4\n\t"
        "j 2f\n\t"
        "sll $0, $0, 0\n\t"
        "sll $0, $0, 0\n\t"
        "1:\n\t"
        "sub $4, $4, $4\n\t"
        "addi $4, $4, 1\n\t"
        "2:\n\t"
        "daddu $2, $4, $0\n\t"
        "4:\n\t"
        ".set reorder"
        :
        : "m"(_sefBattleMode)
        : "$2", "$3", "$4", "$5", "$f0", "$f1", "memory");
}

extern Vector4 svColorMaxVector;

/* The texture object at SvDrawState+0x1EC: its tex0 word is at +0x10. */
typedef struct SvDrawTexture {
    unsigned char unmodeled_00[0x10];
    u64 tex0;
} SvDrawTexture;

typedef struct SvDrawUv {
    float u;
    float v;
    unsigned char unmodeled_08[0x8];
    u64 tex0;
    unsigned char unmodeled_18[0x4];
    unsigned short flags;
    short kind;                     /* +0x1E: 4 for the image kinds drawn by svDrawSortedImages */
} SvDrawUv;

/* The per-draw state the functions below read: the base matrix at +0, the two point quadwords at +0x140 and +0x160, the int vertex at +0x1D0, the UV and texture blocks, the primary and secondary prims, the Z rotation pair, the table index and the extra state quadword. */
typedef struct SvDrawState {
    Matrix4 matrix;
    unsigned char unmodeled_040[0x100];
    Vector4 pointA;                 /* +0x140 */
    unsigned char unmodeled_150[0x10];
    Vector4 pointB;                 /* +0x160 */
    unsigned char unmodeled_170[0x60];
    int vertex[4];
    unsigned char unmodeled_1E0[0x8];
    SvDrawUv *uv;
    SvDrawTexture *texture;
    u64 primaryPrim;
    u64 secondaryPrim;
    unsigned char unmodeled_200[0x28];
    float angleZ;
    unsigned char unmodeled_22C[0xC];
    float angleZOffset;
    unsigned char unmodeled_23C[0x50];
    int tableIndex;
    Vector4 extraState;
} SvDrawState;

typedef struct SvDrawRecordC {
    DoublewordVector4 gif;          /* +0x00: GIF tag and register list */
    DoublewordVector4 texture;      /* +0x10: the UV block's tex0, register 6 */
    DoublewordVector4 test;         /* +0x20: svDrawTestWord, register 0x47 */
    DoublewordVector4 alpha;        /* +0x30: the alpha argument, register 0x42 */
    Vector4 pointA;
    union {
        Vector4 vector;             /* +0x50: the vertex the sqc2 writes */
        int words[4];               /* the same as ints */
        Quadword quad;              /* the same, moved as one quadword */
    } vertex;
    DrawStateRegister prim;
    Vector4 extraState;
    DoublewordVector4 coords;       /* +0x80: u and v of the UV block, z 0 and w 1.0f */
    Vector4 pointB;
    Matrix4 matrix;
} SvDrawRecordC;

/*
 * Appends a kind C draw record (14 quadwords): the GIF tag, the UV block's tex0, svDrawTestWord, the alpha
 * word, the first point, the vertex (scaled by svVertexScale when set), the primary prim, the extra state,
 * the coordinates (u, v, 0, 1), the second point and the state's matrix.
 */
void svDrawListAppendSpriteRecord(SvDrawState *state, u64 alpha)
{
    SvDrawRecordC *record;
    Vector4 *vertexDst;
    const Vector4 *vertexScale;
    Vector4 *extraDst;
    Vector4 *extraSrc;
    DoublewordVector4 *coordsDst;

    record = (SvDrawRecordC *) (svDrawList.entries + svDrawList.end_index);
    record->gif.words[0] = 0xD02A400000008001ULL;
    record->gif.words[1] = 0x000525252521EEEEULL;
    record->texture.words[0] = state->uv->tex0;
    record->texture.words[1] = GS_TEX0_1;
    record->test.words[0] = svDrawTestWord;
    record->test.words[1] = GS_TEST_1;
    record->alpha.words[0] = alpha;
    record->alpha.words[1] = GS_ALPHA_1;
    vertexDst = &record->vertex.vector;
    vertexScale = svVertexScale;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(state->vertex) : "memory");
    if (vertexScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(vertexScale), "r"(&svColorMaxVector) : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(vertexDst));
    QuadwordCopyT0(&record->pointA, &state->pointA);
    QuadwordCopyT0(&record->prim, &state->primaryPrim);
    record->prim.address = 0;
    extraDst = &record->extraState;
    extraSrc = &state->extraState;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*extraDst), "+m"(*extraSrc) : "r"(extraDst), "r"(extraSrc) : "$8", "memory");
    coordsDst = &record->coords;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*coordsDst), "+m"(*state->uv) : "r"(coordsDst), "r"(state->uv) : "$8", "memory");
    record->coords.words[1] = 0x3F80000000000000ULL;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(record->pointB), "+m"(state->pointB) : "r"(&record->pointB), "r"(&state->pointB) : "$8", "memory");
    QuadwordCopyMatrixT0(record->matrix, state->matrix);
    svDrawList.end_index += 14;
}

/*
 * Appends a pair of kind C draw records (28 quadwords): the first carries the GIF tag, the texture block's
 * tex0, the fixed test word 0x31003 and alpha 0x4A; the second the UV block's tex0, svDrawTestWord and the alpha
 * argument. The vertex (scaled by svVertexScale when set) goes to both, the first point to the first, the
 * secondary and primary prims to the first and second, then the extra state, coordinates (u, v, 0, 1),
 * second point and matrix like svDrawListAppendSpriteRecord.
 */
void svDrawListAppendSpriteRecordPair(SvDrawState *state, u64 alpha)
{
    Vector4 *extraSrc;
    Vector4 *extraDst;
    DoublewordVector4 *coordsDst;
    SvDrawRecordC *record;
    const Vector4 *vertexScale;
    int *vertexDst;
    const u64 alphaRegister = GS_ALPHA_1;
    const u64 testRegister = GS_TEST_1;
    u64 testWord;

    record = (SvDrawRecordC *) (svDrawList.entries + svDrawList.end_index);
    testWord = svDrawTestWord;
    record[0].gif.words[0] = 0xD02A400000008001ULL;
    record[0].gif.words[1] = 0x000525252521EEEEULL;
    record[0].texture.words[0] = state->texture->tex0;
    record[1].texture.words[0] = state->uv->tex0;
    record[1].texture.words[1] = GS_TEX0_1;
    record[1].test.words[1] = testRegister;
    record[0].texture.words[1] = GS_TEX0_1;
    record[0].test.words[0] = 0x31003;
    record[1].test.words[0] = testWord;
    record[0].test.words[1] = testRegister;
    record[0].alpha.words[0] = 0x4A;
    vertexDst = record[0].vertex.words;
    vertexScale = svVertexScale;
    record[1].alpha.words[0] = alpha;
    record[1].alpha.words[1] = alphaRegister;
    record[0].alpha.words[1] = alphaRegister;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(state->vertex) : "memory");
    if (vertexScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(vertexScale), "r"(&svColorMaxVector) : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(vertexDst));
    record[1].vertex.quad = record[0].vertex.quad;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(record[0].pointA), "+m"(state->pointA) : "r"(&record[0].pointA), "r"(&state->pointA) : "$8", "memory");
    record[0].prim.data = state->secondaryPrim;
    record[0].prim.address = 0;
    record[1].prim.data = state->primaryPrim;
    record[1].prim.address = 0;
    extraDst = &record[0].extraState;
    extraSrc = &state->extraState;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*extraDst), "+m"(*extraSrc) : "r"(extraDst), "r"(extraSrc) : "$8", "memory");
    coordsDst = &record[0].coords;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*coordsDst), "+m"(*state->uv) : "r"(coordsDst), "r"(state->uv) : "$8", "memory");
    record[0].coords.words[1] = 0x3F80000000000000ULL;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(record[0].pointB), "+m"(state->pointB) : "r"(&record[0].pointB), "r"(&state->pointB) : "$8", "memory");
    QuadwordCopyMatrixT0(record[0].matrix, state->matrix);
    svDrawList.end_index += 28;
}

extern Matrix4 svEffectMatrix;
extern Vector4 svEffectScaleVector;
extern int svFogValue;
extern Vector4 sefCameraMatrix[3];

/* The part block of a transform block: its flags at +0x1C and its kind at +0x1E. */
typedef struct SvDrawPart {
    unsigned char unmodeled_00[0x1C];
    unsigned short flags;
    short kind;
} SvDrawPart;

typedef struct SvTransformBlock {
    Matrix4 matrix;
    unsigned char unmodeled_40[0x40];
    Vector4 rotated_rows[3];
    unsigned char unmodeled_B0[0x140 - 0xB0];
    Vector4 rotation_aux;      /* +0x140 */
    unsigned char unmodeled_150[0x160 - 0x150];
    Vector4 scale;
    unsigned char unmodeled_170[0x1C0 - 0x170];
    Vector4 pivot;             /* +0x1C0: the pivot svPivot points at */
    unsigned char unmodeled_1D0[0x1DC - 0x1D0];
    void *drawData;            /* +0x1DC: NULL when the slot is unused */
    unsigned char unmodeled_1E0[0x1E8 - 0x1E0];
    SvDrawPart *part;          /* +0x1E8 */
    unsigned char unmodeled_1EC[0x28C - 0x1EC];
    int index;                 /* +0x28C */
    Vector4 point;             /* +0x290 */
    unsigned char unmodeled_2A0[0x2C0 - 0x2A0]; /* the slot stride is 0x2C0 */
} SvTransformBlock;

/* Builds the current matrix from the fixed matrix, the identity with the point as translation, or the matrix at svRecordMatrix, multiplies in the block and scales the rows; useItemRows also replaces rows 0..2 from the block and applies the block's rows to the right. */
void svBuildItemMatrix(Vector4 *point, SvTransformBlock *transform, int useFixedMatrix, int useItemRows) {
    if (useFixedMatrix != 0) {
        __asm__ __volatile__(
            "lqc2 vf26, 0(%0)\n\t"
            "lqc2 vf27, 16(%0)\n\t"
            "lqc2 vf28, 32(%0)\n\t"
            "lqc2 vf29, 48(%0)"
            : : "r"(svEffectMatrix));
        vu0TransformPoint(point);
        vu0CurMatrixMul(transform->matrix);
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmuly.xyzw vf27, vf27, vf1y\n\t"
            "vmulz.xyzw vf28, vf28, vf1z\n\t"
            "vmulw.xyzw vf29, vf29, vf1w"
            : : "r"(&svEffectScaleVector));
    } else {
        if (svRecordMatrix != 0) {
            __asm__ __volatile__(
                "lqc2 vf26, 0(%0)\n\t"
                "lqc2 vf27, 16(%0)\n\t"
                "lqc2 vf28, 32(%0)\n\t"
                "lqc2 vf29, 48(%0)"
                : : "r"(svRecordMatrix));
            vu0TransformPoint(point);
        } else {
            __asm__ __volatile__(
                "lqc2 vf26, 0(%0)\n\t"
                "lqc2 vf27, 16(%0)\n\t"
                "lqc2 vf28, 32(%0)"
                : : "r"(vu0IdentityMatrix));
            __asm__ __volatile__("lqc2 vf29, 0(%0)" : : "r"(point));
        }
        vu0CurMatrixMul(transform->matrix);
    }
    if (svFogValue != 0) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmuly.xyzw vf27, vf27, vf1y\n\t"
            "vmulz.xyzw vf28, vf28, vf1z\n\t"
            "vmulw.xyzw vf29, vf29, vf1w"
            : : "r"(&transform->scale));
        if (svFogValue >= 0) {
            vu0TransformPoint(&transform->point);
        }
    } else {
        vu0TransformPoint(&transform->point);
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmuly.xyzw vf27, vf27, vf1y\n\t"
            "vmulz.xyzw vf28, vf28, vf1z\n\t"
            "vmulw.xyzw vf29, vf29, vf1w"
            : : "r"(&transform->scale));
    }
    if (useItemRows != 0) {
        __asm__ __volatile__(
            "lqc2 vf26, 0(%0)\n\t"
            "lqc2 vf27, 16(%0)\n\t"
            "lqc2 vf28, 32(%0)"
            : : "r"(transform->rotated_rows));
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmuly.xyzw vf27, vf27, vf1y\n\t"
            "vmulz.xyzw vf28, vf28, vf1z\n\t"
            "vmulw.xyzw vf29, vf29, vf1w"
            : : "r"(&svEffectScaleVector));
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmuly.xyzw vf27, vf27, vf1y\n\t"
            "vmulz.xyzw vf28, vf28, vf1z\n\t"
            "vmulw.xyzw vf29, vf29, vf1w"
            : : "r"(&transform->scale));
        vu0CurMatrixMulRows3(sefCameraMatrix);
    }
}


/* Partial view of the render item svSetModelRenderState draws: the scale vector at +0x160, the fixed-point position at +0x1D0 whose w word at +0x1DC is the transparency (0x80 is opaque), the part at +0x1E8, the bias at +0x248 and the stencil index at +0x28C. */
typedef struct SvRenderItem {
    unsigned char unmodeled_00[0x160];
    Vector4 scale;
    unsigned char unmodeled_170[0x1D0 - 0x170];
    int position[3];
    unsigned int transparency;
    unsigned char unmodeled_1E0[0x1E8 - 0x1E0];
    struct SvRenderPart *part;
    unsigned char unmodeled_1EC[0x248 - 0x1EC];
    float bias;
    unsigned char unmodeled_24C[0x28C - 0x24C];
    int stencilIndex;
} SvRenderItem;

/* The part of the render item: the block pxySubmitModel takes at +0x8 and the block nmlModelSetTexture takes at +0xC. */
typedef struct SvRenderPart {
    unsigned char unmodeled_00[0x8];
    PxyHeader *submitBlock;
    TxyBlock *txyBlock;
} SvRenderPart;

/* The view block svSetModelRenderState writes: the scaled vertex position at +0xD0 and its copy at +0xE0. */
typedef struct SvViewBlock {
    unsigned char unmodeled_00[0xD0];
    AlignedVector4 scaled;
    AlignedVector4 scaledCopy;
} SvViewBlock;

extern const u64 svStencilTable[];
extern void *svActiveLightSet;
extern const f32 svSortOffsetScale;

/* Sets the render state of one model item and submits it: the stencil word from the table at svStencilTable, the scaled position into the view block, the matrix, the transparency, the flag bits and the fog override, then the scale and the submit. */
void svSetModelRenderState(SvRenderItem *item, Matrix4 *matrix, int scale)
{
    u64 stencil;
    const Vector4 *vertexScale;
    AlignedVector4 *vertexOut;
    SvViewBlock *view;
    SefFogOverride *fog;
    int itemFlags;
    AlignedVector4 vertex;

    if (item->stencilIndex == 0 && item->transparency == 0x80) {
        stencil = 0;
    } else {
        stencil = svStencilTable[item->stencilIndex];
    }
    __asm__ __volatile__(
        "li.s $f0, 0.0078125\n\t"
        "mfc1 $8, $f0\n\t"
        "qmtc2 $8, vf1\n\t"
        "lqc2 vf2, 0(%0)\n\t"
        "vitof0.xyzw vf2, vf2\n\t"
        "vmulx.xyz vf2, vf2, vf1x\n\t"
        "sqc2 vf2, 0(%1)"
        : : "r"(item->position), "r"((AlignedVector4 *) &svEffectLightSet[208]) : "$8", "$f0");
    do {
        vertexScale = svVertexScale;
    } while (0);
    vertexOut = &vertex;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(item->position));
    if (vertexScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%1)\n\t"
            "lqc2 vf3, 0(%0)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(&svColorMaxVector), "r"(vertexScale));
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(vertexOut));
    view = svActiveLightSet;
    __asm__ __volatile__("li.s $f0, 0.0078125" : : : "$f0");
    __asm__ __volatile__(
        "mfc1 $8, $f0\n\t"
        "qmtc2 $8, vf1\n\t"
        "lqc2 vf2, 0(%0)\n\t"
        "vitof0.xyzw vf2, vf2\n\t"
        "vmulx.xyz vf2, vf2, vf1x\n\t"
        "sqc2 vf2, 0(%1)"
        : : "r"(&vertex), "r"(&view->scaled) : "$8", "$f0");
    QuadwordCopyT0(&view->scaledCopy, &view->scaled);

    nmlModelSetStencil(stencil);
    if (stencil != 0) {
        nmlModelSetStencilAlpha(stencil);
    }

    if (item->part->txyBlock != 0 && item->part->submitBlock != 0) {
        itemFlags = svItemDrawFlags;
        pxySetLightCopy(svActiveLightSet);
        pxySetAnimTime(item->bias);
        nmlModelSetTexture(item->part->txyBlock);
        nmlModelSetDrawMatrix(*matrix);
        nmlModelSetTransparency((float) item->transparency * 0.0078125f);
        if ((itemFlags & 1) != 0) {
            nmlModelSetRenderLevel(4);
        }
        if ((itemFlags & 8) != 0) {
            nmlModelSetZwrite(1);
        } else {
            nmlModelSetZwrite(0);
        }
        if ((itemFlags & 2) != 0) {
            nmlModelSetRenderLevel(8);
        }
        if ((itemFlags & 0x80) != 0) {
            nmlModelSetRenderLevel(0x200);
        }
        if ((itemFlags & 0x40) != 0) {
            nmlModelSetRenderLevel(0x100);
        } else {
            fog = sefGetFogOverrideBlock();
            if (fog->enabled != 0) {
                nmlModelSetFogCol(&fog->fogColor);
                nmlModelSetFogDist(fog->dist[0], fog->dist[1], fog->dist[2], fog->dist[3]);
            }
        }
        if (scale > 0) {
            nmlModelSetSortOffset((float) scale * svSortOffsetScale);
        }
        nmlModelSetScale(&item->scale);
        pxySubmitModel(item->part->submitBlock);
    }
}


/* Screen vertex written by svProjectVertex: x and y in 12.4 fixed point, then z and the clip flags. */
typedef struct SvScreenVertex {
    int x;
    int y;
    int z;
    int clipFlags;
} SvScreenVertex;

/* One unit of screen space is 16 subpixel steps (the GS 12.4 coordinate format). */
#define SV_SUBPIXEL_STEPS 16.0f


/* Projects a homogeneous vertex to the screen: divides by w, flags 0x8000 when the result lies outside the unit cube, then applies the active camera's screen scale and offset. Returns the flags. */
int svProjectVertex(SvScreenVertex *out, Matrix4 *matrix, const Vector4 *vector) {
    Vector4 clip;
    float invW;
    int clipFlags;
    XglCamera *camera;

    MMathApplyMatrix(&clip, matrix, vector);
    invW = 1.0f / clip.w;
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n\t"
        "mfc1 $8, %1\n\t"
        "qmtc2 $8, vf2\n\t"
        "vmulx.xyz vf1, vf1, vf2x\n\t"
        "sqc2 vf1, 0(%0)"
        : : "r"(&clip), "f"(invW) : "$8", "memory");
    if (clip.x < -1.0f || 1.0f < clip.x || clip.y < -1.0f || 1.0f < clip.y || clip.z < -1.0f || 1.0f < clip.z) {
        clipFlags = 0x8000;
    } else {
        clipFlags = 0;
    }
    out->clipFlags = clipFlags;
    camera = xglStudioGetActiveCamera();
    __asm__ __volatile__("lqc2 vf1, 0(%0)\n\tlqc2 vf2, 0(%1)" : : "r"(&clip), "r"(&camera->screen.screenScale), "r"(&camera->screen.screenOffset) : "memory");
    __asm__ __volatile__("lqc2 vf3, 0(%0)" : : "r"(&camera->screen.screenOffset) : "memory");
    __asm__ __volatile__("vmula.xyz ACC, vf1, vf2" : : : "memory");
    __asm__ __volatile__("vmaddw.xyz vf1, vf3, vf0w" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(&clip) : "memory");
    out->z = (int) clip.z;
    out->x = (int) (clip.x * SV_SUBPIXEL_STEPS);
    out->y = (int) (clip.y * SV_SUBPIXEL_STEPS);
    return out->clipFlags;
}

/* Signed distance of a point (its w) from a clip plane whose offset comes from MMathVectorDotProduct. */
f32 sefClipPlaneDistance(Vector4 *plane, Vector4 *point) {
    return -(MMathVectorDotProduct(plane, point) + plane->w);
}

/* Intersects the segment p1-p2 with a plane (VU0 dot products, one divide); writes the crossing point. */
void sefClipEdgeToPlane(Vector4 *out, const Vector4 *plane, const Vector4 *p1, const Vector4 *p2) {
    __asm__ __volatile__("vmove.xyzw vf4, vf0" : : : "memory");
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(p1) : "memory");
    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(p2) : "memory");
    __asm__ __volatile__("lqc2 vf3, 0(%0)" : : "r"(plane) : "memory");
    __asm__ __volatile__("vsub.xyz vf4, vf1, vf2" : : : "memory");
    __asm__ __volatile__("vmul.xyz vf5, vf3, vf1" : : : "memory");
    __asm__ __volatile__("vaddy.x vf5, vf5, vf5y" : : : "memory");
    __asm__ __volatile__("vaddz.x vf5, vf5, vf5z" : : : "memory");
    __asm__ __volatile__("vmul.xyz vf6, vf3, vf4" : : : "memory");
    __asm__ __volatile__("vaddy.x vf6, vf6, vf6y" : : : "memory");
    __asm__ __volatile__("vaddz.x vf6, vf6, vf6z" : : : "memory");
    __asm__ __volatile__("vaddw.x vf5, vf5, vf3w" : : : "memory");
    __asm__ __volatile__("vsub.x vf5, vf0, vf5" : : : "memory");
    __asm__ __volatile__("vdiv Q, vf5x, vf6x" : : : "memory");
    __asm__ __volatile__("vwaitq" : : : "memory");
    __asm__ __volatile__("vmulq.xyz vf4, vf4, Q" : : : "memory");
    __asm__ __volatile__("vadd.xyz vf4, vf4, vf1" : : : "memory");
    __asm__ __volatile__("sqc2 vf4, 0(%0)" : : "r"(out) : "memory");
}

/* vf20 = the matrix loaded in vf26..vf29 applied to source. */
static inline void sefTransformToVf20(Vector4 *source)
{
    __asm__ __volatile__("lqc2 vf1, 0(%0)\n"
                         "vmulax.xyzw ACC, vf26, vf1x\n"
                         "vmadday.xyzw ACC, vf27, vf1y\n"
                         "vmaddaz.xyzw ACC, vf28, vf1z\n"
                         "vmaddw.xyzw vf20, vf29, vf1w\n"
                         : : "r"(source) : "memory");
}

/* Stores vf20 to destination. */
static inline void sefStoreVf20(Vector4 *destination)
{
    __asm__ __volatile__("sqc2 vf20, 0(%1)\n" : "=m"(destination->w) : "r"(destination) : "memory");
}

/* destination.xyz = minuend.xyz - subtrahend.xyz; the outputs are the four components of destination. */
#define SefSubVectorXYZ(destination, minuend, subtrahend) \
    __asm__ __volatile__("lqc2 vf1, 0(%5)\n" \
                         "lqc2 vf2, 0(%6)\n" \
                         "vsub.xyz vf1, vf1, vf2\n" \
                         "sqc2 vf1, 0(%4)\n" \
                         : "=m"((destination)->x), "=m"((destination)->y), "=m"((destination)->z), "=m"((destination)->w) \
                         : "r"(destination), "r"(minuend), "r"(subtrahend) : "memory")
/* *destination = *source through $8; the outputs are the four components of destination. */
#define SefCopyVector(destination, source) \
    __asm__ __volatile__("lq $8, 0(%5)\n" \
                         "sq $8, 0(%4)\n" \
                         : "=m"((destination)->x), "=m"((destination)->y), "=m"((destination)->z), "=m"((destination)->w) \
                         : "r"(destination), "r"(source) : "$8", "memory")

/* A vertex of the two clip buffers: the vector, its components (element[3] holds the signed plane distance
 * while a plane is being clipped) and its raw words. */
typedef union SefClipVertex {
    Vector4 vector;
    float element[4];
    int word[4];
} SefClipVertex;

/* One vertex sefClipPolygon emits: the clipped position, then its texture coordinate. */
typedef struct SvClipVertex {
    Vector4 position;
    Vector4 texCoord;
} SvClipVertex;

/* Clips the draw state quad, transformed by matrix, against the first five planes of the active camera's view
 * volume (Sutherland-Hodgman, two vertex buffers). Each surviving vertex goes to out as a pair: the position
 * with w = 1, then svUnitQuadCornerMatrix with x and y replaced by the position's projections on the quad's two edges.
 * Returns the vertex count, 0 when the quad is degenerate or clipped away. */
int sefClipPolygon(void *slot, Matrix4 *matrix, SvClipVertex *out)
{
    int vertexCounts[2];
    float edgeLengths[2];
    Vector4 corners[4];
    Vector4 edges[2];
    SefClipVertex clipBuffers[2][9];
    Vector4 offset;
    int planeIndex;
    XglCamera *camera;
    DrawStatePacket *drawPacket;
    Vector4 *planeVector;
    Vector4 *origin;
    Vector4 *dst;
    int currentBuffer;
    int nextBuffer;
    int packetCorner;
    int clipCount;
    int i;
    int nextVertex;

    camera = xglStudioGetActiveCamera();
    currentBuffer = 0;
    vertexCounts[1] = 0;
    vertexCounts[0] = 0;
    __asm__ __volatile__("lqc2 vf26, 0(%0)\n"
                         "lqc2 vf27, 16(%0)\n"
                         "lqc2 vf28, 32(%0)\n"
                         "lqc2 vf29, 48(%0)\n"
                         : : "r"(matrix) : "memory");
    drawPacket = svDrawStatePacket;
    for (i = 0; i < 4; i++) {
        if (i == 2) {
            packetCorner = 3;
        } else if (i == 3) {
            packetCorner = 2;
        } else {
            packetCorner = i;
        }
        sefTransformToVf20(&drawPacket->corners[packetCorner]);
        sefStoreVf20(&corners[i]);
        sefStoreVf20(&clipBuffers[currentBuffer][i].vector);
    }
    vertexCounts[currentBuffer] = 4;
    origin = &corners[0];
    SefSubVectorXYZ(&edges[0], &corners[1], origin);
    SefSubVectorXYZ(&edges[1], &corners[3], origin);
    edgeLengths[0] = MMathVectorDotProduct(&edges[0], &edges[0]);
    edgeLengths[1] = MMathVectorDotProduct(&edges[1], &edges[1]);
    if (edgeLengths[0] == 0.0f || edgeLengths[1] == 0.0f) {
        return 0;
    }
    for (planeIndex = 0; planeIndex < 5; planeIndex++) {
        planeVector = &camera->volume[0].plane[planeIndex].vector;
        nextBuffer = currentBuffer ^ 1;
        for (i = 0; i < vertexCounts[currentBuffer]; i++) {
            clipBuffers[currentBuffer][i].element[3] = sefClipPlaneDistance(planeVector, &clipBuffers[currentBuffer][i].vector);
        }
        clipCount = 0;
        for (i = 0; i < vertexCounts[currentBuffer]; i++) {
            nextVertex = (i + 1) % vertexCounts[currentBuffer];
            if (clipBuffers[currentBuffer][i].element[3] >= 0.0f) {
                clipBuffers[nextBuffer][clipCount++] = clipBuffers[currentBuffer][i];
                if (clipBuffers[currentBuffer][nextVertex].element[3] < 0.0f) {
                    sefClipEdgeToPlane(&clipBuffers[nextBuffer][clipCount++].vector, planeVector, &clipBuffers[currentBuffer][i].vector, &clipBuffers[currentBuffer][nextVertex].vector);
                }
            } else if (clipBuffers[currentBuffer][nextVertex].element[3] >= 0.0f) {
                sefClipEdgeToPlane(&clipBuffers[nextBuffer][clipCount++].vector, planeVector, &clipBuffers[currentBuffer][i].vector, &clipBuffers[currentBuffer][nextVertex].vector);
            }
        }
        vertexCounts[nextBuffer] = clipCount;
        currentBuffer = nextBuffer;
        if (clipCount < 3) {
            return 0;
        }
    }
    for (i = 0; i < vertexCounts[currentBuffer]; i++) {
        dst = &out[i].position;
        __asm__ __volatile__("lq $8, %5\n"
                             "sq $8, 0(%4)\n"
                             : "=m"(dst[0].x), "=m"(dst[0].y), "=m"(dst[0].z), "=m"(dst[0].w)
                             : "r"(&dst[0]), "m"(*(clipBuffers[currentBuffer] + i)) : "$8", "memory");
        dst[0].w = 1.0f;
        SefCopyVector(&dst[1], &svUnitQuadCornerMatrix);
        SefSubVectorXYZ(&offset, &dst[0], origin);
        dst[1].x = MMathVectorDotProduct(&offset, &edges[0]) / edgeLengths[0];
        dst[1].y = MMathVectorDotProduct(&offset, &edges[1]) / edgeLengths[1];
    }
    return vertexCounts[currentBuffer];
}

/* A vector moved as one quadword. */
typedef union SvQuadVector {
    Quadword quad;
    Vector4 vector;
} SvQuadVector;

/* A texture part: the two scale factors the polygon axes take and the TEX0 register word at +0x10. */
typedef struct SvPolygonPart {
    float scaleX;
    float scaleY;
    unsigned char unmodeled_08[0x10 - 0x08];
    u64 tex0;
} SvPolygonPart;

/* The argument block the item draw (svDrawItemSlots) and the sorted-image draw (svDrawSortedImages) hand to the slot
 * callees, which read only quad: the vector at +0x4 (the entry's scale vector, or the slot state's second point),
 * then the two reference blocks at +0x8 and +0xC and the draw-mode bits (-1 before the first line). */
typedef struct SvDrawCall {
    unsigned char unmodeled_00[4];
    Vector4 *quad;
    const void *texture;            /* +0x08 */
    const void *uv;                 /* +0x0C */
    int mode;                       /* +0x10 */
} SvDrawCall;

/* The polygon item svDrawPolygonItem draws: its matrix, the scale vector at +0x160, the fixed-point colour at +0x1D0,
 * the base and overlay parts with their texture offsets, the alpha table index at +0x28C and the point at +0x290. */
typedef struct SvPolygonItem {
    Matrix4 matrix;
    unsigned char unmodeled_40[0x160 - 0x40];
    Vector4 scale;
    unsigned char unmodeled_170[0x1D0 - 0x170];
    int color[4];
    unsigned char unmodeled_1E0[0x1E8 - 0x1E0];
    SvPolygonPart *part;
    SvPolygonPart *overlayPart;
    float texOffsetX;
    float texOffsetY;
    float overlayOffsetX;
    float overlayOffsetY;
    unsigned char unmodeled_200[0x28C - 0x200];
    int drawIndex;
    Vector4 point;
} SvPolygonItem;

/* The packet head: the GIF A+D tag, TEX0, TEST, ALPHA and the CLAMP entry of the draw state, then the vertex tag. */
typedef struct SvPolygonPacket {
    DoublewordVector4 gif;
    DoublewordVector4 tex;
    DoublewordVector4 test;
    DoublewordVector4 alpha;
    Quadword clamp;
    DoublewordVector4 vertexTag;
} SvPolygonPacket;

/* One packed vertex: ST, RGBAQ and XYZ2. */
typedef struct SvPolygonVertex {
    SvQuadVector texCoord;
    Quadword color;
    Quadword position;
} SvPolygonVertex;



extern short svPolygonClipFlag;
extern const u64 svPairAlphaTable[];

/* Draws the item's clipped polygon: the item matrix scaled by the part's axes (svPolygonAxesMode picks the axes) goes to
 * sefClipPolygon, then the vertices are packed with the item colour, once with the overlay part over the base part
 * when svPolygonClipFlag is set, otherwise once with the base part. */
void svDrawPolygonItem(SvPolygonItem *item, Vector4 *position, SvDrawCall *quadRef, int flag)
{
    Matrix4 matrix;
    Vector4 axes;
    SvClipVertex vertices[9];
    Quadword color;
    Quadword *colorOut;
    int *itemColor;
    int size;
    SvPolygonPacket *packet;
    SvPolygonVertex *vertex;
    SvPolygonPart *part;
    const Vector4 *vertexScale;
    int clipCount;
    int i;
    int mode;
    float offsetX;

    vertex = (SvPolygonVertex *) (svDrawList.entries + svDrawList.end_index);
    part = item->part;
    __asm__ __volatile__(
        "lqc2 vf26, 0(%0)\n\t"
        "lqc2 vf27, 16(%0)\n\t"
        "lqc2 vf28, 32(%0)"
        : : "r"(vu0IdentityMatrix));
    __asm__ __volatile__("lqc2 vf29, 0(%0)" : : "r"(position));
    vu0CurMatrixMul(item->matrix);
    if (sefGlobalTransform.scaled != 0) {
        __asm__ __volatile__(
            "mfc1 $8, %0\n\t"
            "qmtc2.ni $8, vf1\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmulx.xyzw vf27, vf27, vf1x\n\t"
            "vmulx.xyzw vf28, vf28, vf1x"
            : : "f"(sefGlobalTransform.scale) : "$8", "memory");
    }
    if (svFogValue != 0) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmuly.xyzw vf27, vf27, vf1y\n\t"
            "vmulz.xyzw vf28, vf28, vf1z\n\t"
            "vmulw.xyzw vf29, vf29, vf1w"
            : : "r"(&item->scale));
        if (svFogValue >= 0) {
            vu0TransformPoint(&item->point);
        }
    } else {
        vu0TransformPoint(&item->point);
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmuly.xyzw vf27, vf27, vf1y\n\t"
            "vmulz.xyzw vf28, vf28, vf1z\n\t"
            "vmulw.xyzw vf29, vf29, vf1w"
            : : "r"(&item->scale));
    }
    __asm__ __volatile__("vmaxw.xyzw vf1, vf0, vf0w\n\tsqc2 vf1, 0(%0)" : : "r"(&axes) : "memory");
    mode = svPolygonAxesMode;
    if (mode == 5) {
        axes.x = part->scaleX;
        axes.y = part->scaleY;
    } else if (mode == 7) {
        axes.x = part->scaleX;
        axes.z = part->scaleY;
    } else {
        axes.y = part->scaleX;
        axes.z = part->scaleY;
    }
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n\t"
        "vmulx.xyzw vf26, vf26, vf1x\n\t"
        "vmuly.xyzw vf27, vf27, vf1y\n\t"
        "vmulz.xyzw vf28, vf28, vf1z\n\t"
        "vmulw.xyzw vf29, vf29, vf1w"
        : : "r"(&axes) : "memory");
    /* The record matrix goes through the int mode: a pointer-typed call changes the register allocation. */
    mode = (int) svRecordMatrix;
    if (mode != 0) {
        vu0CurMatrixMulRight((void *) mode);
    }
    __asm__ __volatile__(
        "sqc2 vf26, 0(%0)\n\t"
        "sqc2 vf27, 16(%0)\n\t"
        "sqc2 vf28, 32(%0)\n\t"
        "sqc2 vf29, 48(%0)"
        : : "r"(matrix) : "memory");
    clipCount = sefClipPolygon(item, &matrix, vertices);
    if (clipCount == 0) {
        return;
    }
    colorOut = &color;
    itemColor = item->color;
    vertexScale = svVertexScale;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(itemColor) : "memory");
    if (vertexScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(vertexScale), "r"(&svColorMaxVector) : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(colorOut) : "memory");
    if (svPolygonClipFlag != 0) {
        packet = (SvPolygonPacket *) vertex;
        packet->gif.words[0] = 0x1000000000008004ULL;
        packet->gif.words[1] = GIF_AD;
        packet->tex.words[0] = item->overlayPart->tex0;
        packet->tex.words[1] = GS_TEX0_1;
        packet->test.words[0] = 0x31003;
        packet->test.words[1] = GS_TEST_1;
        packet->alpha.words[0] = 0x4A;
        packet->alpha.words[1] = GS_ALPHA_1;
        packet->clamp = *(Quadword *) &svDrawStatePacket->clamp;
        packet->vertexTag.words[0] = (clipCount | 0x8000) | ((u64) 0xC0AB << 46);
        packet->vertexTag.words[1] = 0x512;
        vertex = (SvPolygonVertex *) (packet + 1);
        for (i = 0; i < clipCount; i++) {
            vertex->texCoord.quad = *(Quadword *) &vertices[i].texCoord;
            offsetX = item->overlayOffsetX;
            vertex->texCoord.vector.x += offsetX;
            vertex->texCoord.vector.y += item->overlayOffsetY;
            vertex->color = color;
            vertex->position = *(Quadword *) &vertices[i].position;
            vertex++;
        }
        packet = (SvPolygonPacket *) vertex;
        packet->gif.words[0] = 0x1000000000008004ULL;
        packet->gif.words[1] = GIF_AD;
        packet->tex.words[0] = item->part->tex0;
        packet->tex.words[1] = GS_TEX0_1;
        packet->test.words[0] = svDrawTestWord;
        packet->test.words[1] = GS_TEST_1;
        packet->alpha.words[0] = svPairAlphaTable[item->drawIndex];
        packet->alpha.words[1] = GS_ALPHA_1;
        packet->clamp = *(Quadword *) &svDrawStatePacket->clamp;
        packet->vertexTag.words[0] = (clipCount | 0x8000) | ((u64) 0xC0AB << 46);
        packet->vertexTag.words[1] = 0x512;
        vertex = (SvPolygonVertex *) (packet + 1);
        for (i = 0; i < clipCount; i++) {
            vertex->texCoord.quad = *(Quadword *) &vertices[i].texCoord;
            offsetX = item->texOffsetX;
            vertex->texCoord.vector.x += offsetX;
            vertex->texCoord.vector.y += item->texOffsetY;
            vertex->color = color;
            vertex->position = *(Quadword *) &vertices[i].position;
            vertex++;
        }
        svDrawList.end_index += (unsigned int) ((clipCount * 3 + 6) * 2);
    } else {
        packet = (SvPolygonPacket *) vertex;
        packet->gif.words[0] = 0x1000000000008004ULL;
        packet->gif.words[1] = GIF_AD;
        packet->tex.words[0] = item->part->tex0;
        packet->tex.words[1] = GS_TEX0_1;
        packet->test.words[0] = svDrawTestWord;
        packet->test.words[1] = GS_TEST_1;
        packet->alpha.words[0] = svAlphaTestTable[item->drawIndex];
        packet->alpha.words[1] = GS_ALPHA_1;
        packet->clamp = *(Quadword *) &svDrawStatePacket->clamp;
        packet->vertexTag.words[0] = (clipCount | 0x8000) | ((u64) 0xC0AB << 46);
        packet->vertexTag.words[1] = 0x512;
        vertex = (SvPolygonVertex *) (packet + 1);
        for (i = 0; i < clipCount; i++) {
            vertex->texCoord.quad = *(Quadword *) &vertices[i].texCoord;
            offsetX = item->texOffsetX;
            vertex->texCoord.vector.x += offsetX;
            vertex->texCoord.vector.y += item->texOffsetY;
            vertex->color = color;
            vertex->position = *(Quadword *) &vertices[i].position;
            vertex++;
        }
        svDrawList.end_index += (unsigned int) (clipCount * 3 + 6);
    }
}

/* Part record of a slot: the two floats the draw packet reads, the 64-bit word at +0x10, the flags halfword (bit 0x10 hides the slot unless the flag allows it) and the kind halfword (4 is drawn here). */
typedef struct SefSlotPart {
    float coordinateX;
    float coordinateY;
    unsigned char unmodeled_08[0x10 - 0x08];
    unsigned long long word10;
    unsigned char unmodeled_18[0x1C - 0x18];
    unsigned short flags;
    short kind;
} SefSlotPart;

/* One 704-byte slot: the matrix rows at +0x00, the position vector at +0x140, a draw block at +0x1DC (null skips the slot) and the part at +0x1E8. */
typedef struct SefSlotRecord {
    Matrix4 matrix;
    unsigned char unmodeled_40[0x140 - 0x40];
    Vector4 position;
    unsigned char unmodeled_150[0x1D0 - 0x150];
    unsigned char scaleBlock[0x1DC - 0x1D0];
    void *drawData;
    unsigned char unmodeled_1E0[0x1E8 - 0x1E0];
    SefSlotPart *part;
    unsigned char unmodeled_1EC[0x1F0 - 0x1EC];
    Vector4 partBlock;
    unsigned char unmodeled_200[0x28C - 0x200];
    int drawIndex;
    Vector4 slotBlock;
    unsigned char unmodeled_2A0[0x2C0 - 0x2A0];
} SefSlotRecord;

/* A mode word holds either a float coordinate or a zero written as an integer word. */
typedef union SvModeWord {
    float f;
    unsigned int u;
} SvModeWord;

/* One slot draw record appended to svDrawList: 14 quadwords of VIF tags, the slot blocks, the mode words and the matrix rows. */
typedef struct SvSlotDrawPacket {
    DoublewordVector4 gif;          /* +0x00: GIF tag */
    DoublewordVector4 tex;          /* +0x10: the part word and register 6 */
    DoublewordVector4 test;         /* +0x20: svDrawTestWord and register 0x47 */
    DoublewordVector4 alpha;        /* +0x30: the svAlphaTestTable entry and register 0x42 */
    Vector4 position;
    /* DoublewordVector4 here makes the quadword copy ICE (emit_move_insn): kept as two doublewords. */
    unsigned long long partBlock[2];
    Vector4 scaled;
    Vector4 slotBlock;
    SvModeWord modeWord0;
    SvModeWord modeWord1;
    SvModeWord modeWord2;
    SvModeWord modeWord3;
    Vector4 state;
    Matrix4 matrix;
} SvSlotDrawPacket;


/* Appends one slot draw record to svDrawList: tags, the part and state words, the scaled matrix rows and the position block. */
void svDrawListAppendSlot(SefSlotRecord *slot, const Vector4 *position, const SvDrawCall *args, int useScale)
{
    SvSlotDrawPacket *packet;
    long long gifTagLow;
    long long gifTagHigh;
    const Vector4 *vertexScale;
    Vector4 *scaledDst;
    Vector4 *stateDst;
    Vector4 *positionOut;

    gifTagLow = ((long long)0x2A4000 << 0x20) | 0x8001;
    gifTagHigh = (((long long)0x51251251 << 0x10) | 0x2512) << 0x10 | 0xEEEE;
    packet = (SvSlotDrawPacket *) (svDrawList.entries + svDrawList.end_index);
    packet->gif.words[0] = gifTagLow;
    packet->gif.words[1] = gifTagHigh;
    packet->tex.words[0] = slot->part->word10;
    packet->tex.words[1] = GS_TEX0_1;
    packet->test.words[0] = svDrawTestWord;
    packet->test.words[1] = GS_TEST_1;
    positionOut = &packet->position;
    packet->alpha.words[0] = svAlphaTestTable[slot->drawIndex];
    packet->alpha.words[1] = GS_ALPHA_1;
    QuadwordCopyT0(positionOut, position);
    QuadwordCopyT0(packet->partBlock, &slot->partBlock);
    vertexScale = svVertexScale;
    scaledDst = &packet->scaled;
    packet->partBlock[1] = 0;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(slot->scaleBlock) : "memory");
    if (vertexScale != 0) {
        Vector4 *clampMax;
        clampMax = &svColorMaxVector;
        __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(vertexScale) : "memory");
        __asm__ __volatile__("lqc2 vf3, 0(%0)" : : "r"(clampMax) : "memory");
        __asm__ __volatile__("vitof0.xyzw vf1, vf1" : : : "memory");
        __asm__ __volatile__("vmul.xyzw vf1, vf1, vf2" : : : "memory");
        __asm__ __volatile__("vmini.xyz vf1, vf1, vf3" : : : "memory");
        __asm__ __volatile__("vftoi0.xyzw vf1, vf1" : : : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(scaledDst) : "memory");
    QuadwordCopyT0(&packet->slotBlock, &slot->slotBlock);
    if (svDrawMode == 5) {
        float coordinateX;
        float coordinateY;
        coordinateX = slot->part->coordinateX;
        packet->modeWord0.f = coordinateX;
        coordinateY = slot->part->coordinateY;
        packet->modeWord3.f = 1.0f;
        packet->modeWord1.f = coordinateY;
        packet->modeWord2.u = 0;
    } else if (svDrawMode == 7) {
        float coordinateX;
        float coordinateY;
        coordinateX = slot->part->coordinateX;
        packet->modeWord0.f = coordinateX;
        coordinateY = slot->part->coordinateY;
        packet->modeWord3.f = 1.0f;
        packet->modeWord2.f = coordinateY;
        packet->modeWord1.u = 0;
    } else {
        float coordinateX;
        float coordinateY;
        coordinateX = slot->part->coordinateX;
        packet->modeWord1.f = coordinateX;
        coordinateY = slot->part->coordinateY;
        packet->modeWord3.f = 1.0f;
        packet->modeWord2.f = coordinateY;
        packet->modeWord0.u = 0;
    }
    if ((slot->part->flags & 0x10) != 0) {
        stateDst = &packet->state;
        __asm__ __volatile__("vmaxw.xyzw vf1, vf0, vf0w" : : : "memory");
        __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(stateDst) : "memory");
    } else {
        QuadwordCopyT0(&packet->state, args->quad);
    }
    __asm__ __volatile__("lqc2 vf26, 0(%0)" : : "r"(slot->matrix) : "memory");
    __asm__ __volatile__("lqc2 vf27, 16(%0)" : : "r"(slot->matrix) : "memory");
    __asm__ __volatile__("lqc2 vf28, 32(%0)" : : "r"(slot->matrix) : "memory");
    __asm__ __volatile__("lqc2 vf29, 48(%0)" : : "r"(slot->matrix) : "memory");
    if (svPivotAddress != 0) {
        __asm__ __volatile__("mfc1 $8, %0\n\tqmtc2 $8, vf1" : : "f"(sefGlobalTransform.scale) : "$8", "memory");
        __asm__ __volatile__("vmulx.xyz vf26, vf26, vf1x" : : : "memory");
        __asm__ __volatile__("vmulx.xyz vf27, vf27, vf1x" : : : "memory");
        __asm__ __volatile__("vmulx.xyz vf28, vf28, vf1x" : : : "memory");
    }
    if (useScale != 0) {
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&svEffectScaleVector) : "memory");
        __asm__ __volatile__("vmulx.xyz vf26, vf26, vf1x" : : : "memory");
        __asm__ __volatile__("vmuly.xyz vf27, vf27, vf1y" : : : "memory");
        __asm__ __volatile__("vmulz.xyz vf28, vf28, vf1z" : : : "memory");
    }
    __asm__ __volatile__("sqc2 vf26, 0(%0)" : : "r"(packet->matrix) : "memory");
    __asm__ __volatile__("sqc2 vf27, 16(%0)" : : "r"(packet->matrix) : "memory");
    __asm__ __volatile__("sqc2 vf28, 32(%0)" : : "r"(packet->matrix) : "memory");
    __asm__ __volatile__("sqc2 vf29, 48(%0)" : : "r"(packet->matrix) : "memory");
    svDrawList.end_index += 0x11;
}

/* A draw record of 17 quadwords (0x110 bytes): the A+D registers, the position, the prim, the vertex, the extra state, the float coordinates, the quadword of the reference and the matrix. */
typedef struct SvDrawRecordB {
    DoublewordVector4 gif;          /* +0x00: GIF tag and register list */
    DoublewordVector4 texture;      /* +0x10: tex0 word, register 6 */
    DoublewordVector4 test;         /* +0x20: register 0x47 */
    DoublewordVector4 alpha;        /* +0x30: register 0x42 */
    Vector4 position;
    DrawStateRegister prim;
    Vector4 vertex;
    Vector4 extraState;
    DoublewordVector4 coords;       /* +0x80: u and v placed by svDrawMode, the remaining lane 0 and w 1.0f */
    Vector4 quad;
    Matrix4 matrix;
    unsigned char unmodeled_E0[0x110 - 0xE0];
} SvDrawRecordB;

/*
 * Appends a pair of kind B draw records (34 quadwords): the first carries the GIF tag, the texture block's tex0, the fixed test word 0x31003 and alpha 0x4A; the second the UV block's tex0, svDrawTestWord and the svPairAlphaTable entry of the table index. The point, the prims, the vertex (scaled by svVertexScale when set), the extra state, the coordinates (placed by the svDrawMode mode), the quadword and the state's matrix (scaled by sefGlobalTransformScale and svEffectScaleVector) follow.
 */
void svDrawListAppendRecordBPair(SvDrawState *state, Vector4 *point, SvDrawCall *quadRef, int useScale)
{
    Vector4 *positionDst;
    Vector4 *vertexDst;
    SvDrawRecordB *record;
    const Vector4 *vertexScale;
    u64 testWord;

    record = (SvDrawRecordB *) (svDrawList.entries + svDrawList.end_index);
    testWord = svDrawTestWord;
    record[0].gif.words[0] = 0x002A400000008001ULL;
    record[0].gif.words[1] = 0x512512512512EEEEULL;
    record[0].texture.words[0] = state->texture->tex0;
    record[1].texture.words[0] = state->uv->tex0;
    record[1].texture.words[1] = GS_TEX0_1;
    record[1].test.words[1] = GS_TEST_1;
    record[0].texture.words[1] = GS_TEX0_1;
    record[0].test.words[0] = 0x31003;
    record[1].test.words[0] = testWord;
    record[0].test.words[1] = GS_TEST_1;
    record[0].alpha.words[0] = 0x4A;
    record[1].alpha.words[0] = svPairAlphaTable[state->tableIndex];
    record[1].alpha.words[1] = GS_ALPHA_1;
    record[0].alpha.words[1] = GS_ALPHA_1;
    positionDst = &record[0].position;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "+m"(*positionDst), "+m"(*point) : "r"(positionDst), "r"(point), "m"(*positionDst) : "$8", "memory");
    record[0].prim.data = state->secondaryPrim;
    vertexDst = &record[0].vertex;
    record[0].prim.address = 0;
    vertexScale = svVertexScale;
    record[1].prim.data = state->primaryPrim;
    record[1].prim.address = 0;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(state->vertex) : "memory");
    if (vertexScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(vertexScale), "r"(&svColorMaxVector) : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(vertexDst));
    QuadwordCopyT0(&record[0].extraState, &state->extraState);
    if (svDrawMode == 6) {
        record[0].coords.vector.x = state->uv->u;
        record[0].coords.vector.y = state->uv->v;
        record[0].coords.vector.w = 1.0f;
        record[0].coords.vector.z = 0.0f;
    } else if (svDrawMode == 8) {
        record[0].coords.vector.x = state->uv->u;
        record[0].coords.vector.z = state->uv->v;
        record[0].coords.vector.w = 1.0f;
        record[0].coords.vector.y = 0.0f;
    } else {
        record[0].coords.vector.y = state->uv->u;
        record[0].coords.vector.z = state->uv->v;
        record[0].coords.vector.w = 1.0f;
        record[0].coords.vector.x = 0.0f;
    }
    QuadwordCopyT0(&record[0].quad, quadRef->quad);
    __asm__ __volatile__(
        "lqc2 vf26, 0(%0)\n"
        "lqc2 vf27, 16(%0)\n"
        "lqc2 vf28, 32(%0)\n"
        "lqc2 vf29, 48(%0)\n"
        : : "r"(state->matrix));
    if (svPivotAddress != 0) {
        __asm__ __volatile__(
            "mfc1 $8, %0\n"
            "qmtc2 $8, vf1\n"
            "vmulx.xyz vf26, vf26, vf1x\n"
            "vmulx.xyz vf27, vf27, vf1x\n"
            "vmulx.xyz vf28, vf28, vf1x\n"
            : : "f"(sefGlobalTransform.scale) : "$8", "memory");
    }
    if (useScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n"
            "vmulx.xyz vf26, vf26, vf1x\n"
            "vmuly.xyz vf27, vf27, vf1y\n"
            "vmulz.xyz vf28, vf28, vf1z\n"
            : : "r"(&svEffectScaleVector));
    }
    __asm__ __volatile__(
        "sqc2 vf26, 0(%0)\n"
        "sqc2 vf27, 16(%0)\n"
        "sqc2 vf28, 32(%0)\n"
        "sqc2 vf29, 48(%0)\n"
        : : "r"(record[0].matrix) : "memory");
    svDrawList.end_index += 34;
}

typedef struct SvDrawRecordA {
    DoublewordVector4 gif;          /* +0x00: GIF tag and register list */
    DoublewordVector4 texture;      /* +0x10: tex0 word, register 6 */
    DoublewordVector4 test;         /* +0x20: svDrawTestWord, register 0x47 */
    DoublewordVector4 alpha;        /* +0x30: the svAlphaTestTable entry, register 0x42 */
    Vector4 position;
    union {
        int words[4];               /* +0x50: the int vertex the sqc2 writes */
        Quadword quad;              /* the same, moved as one quadword */
    } vertex;
    DrawStateRegister prim;
    Vector4 extraState;
    DoublewordVector4 coords;       /* +0x80: u and v from the UV block, then (0.0f, 1.0f) written as one doubleword */
    Vector4 quad;
    Matrix4 matrix;
} SvDrawRecordA;

/* The fixed TEST_1 data (0x31003) and alpha data (0x4A) of the first record of a pair. */
#define SV_TEST_FIXED_DATA 0x31003
#define SV_ALPHA_FIXED_DATA 0x4A


/*
 * Appends a kind A draw record at the end cursor: the GIF tag and registers, the texture, test and alpha registers, the vertex scaled by svVertexScale when set, the prim, the extra state, the coordinates (u, v, 0, 1), the quadword of the reference (or all ones when the UV flags have 0x10) and the matrix rows. The rows are transformed by vu0TransformPoint and the current matrix; a Z rotation is applied when the two angles sum to a non-zero value. end_index moves on by 14.
 */
void svDrawListAppendRecordA(SvDrawState *state, Vector4 *point, SvDrawCall *quadRef, int useFixedMatrix)
{
    Vector4 *extraSrc;
    Vector4 *extraDst;
    DoublewordVector4 *coordsDst;
    SvDrawRecordA *record;
    const Vector4 *vertexScale;
    int *vertexDst;
    float zAngle;
    DrawStatePacket *packet;

    record = (SvDrawRecordA *) (svDrawList.entries + svDrawList.end_index);
    record->gif.words[0] = 0xD02A400000008001ULL;
    record->gif.words[1] = 0x000525252521EEEEULL;
    record->texture.words[0] = state->uv->tex0;
    record->texture.words[1] = GS_TEX0_1;
    record->test.words[0] = svDrawTestWord;
    record->test.words[1] = GS_TEST_1;
    vertexDst = record->vertex.words;
    vertexScale = svVertexScale;
    record->alpha.words[0] = svAlphaTestTable[state->tableIndex];
    record->alpha.words[1] = GS_ALPHA_1;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(state->vertex) : "memory");
    if (vertexScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(vertexScale), "r"(&svColorMaxVector) : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(vertexDst) : "memory");
    QuadwordCopyT0(&record->prim, &state->primaryPrim);
    record->prim.address = 0;
    extraDst = &record->extraState;
    extraSrc = &state->extraState;
    __asm__ __volatile__("lq $8, 0(%2)\nsq $8, 0(%1)\n" : "+m"(*extraSrc) : "r"(extraDst), "r"(extraSrc) : "$8", "memory");
    coordsDst = &record->coords;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*coordsDst), "+m"(*state->uv) : "r"(coordsDst), "r"(state->uv) : "$8", "memory");
    record->coords.words[1] = 0x3F80000000000000ULL;
    if ((state->uv->flags & 0x10) != 0) {
        __asm__ __volatile__(
            "vmaxw.xyzw vf1, vf0, vf0w\n"
            "sqc2 vf1, 0(%0)\n"
            : : "r"(&record->quad) : "memory");
    } else {
        QuadwordCopyT0(&record->quad, quadRef->quad);
    }
    if (useFixedMatrix != 0) {
        __asm__ __volatile__(
            "lqc2 vf26, 0(%0)\n"
            "lqc2 vf27, 16(%0)\n"
            "lqc2 vf28, 32(%0)\n"
            "lqc2 vf29, 48(%0)\n"
            : : "r"(&svEffectMatrix));
        vu0TransformPoint(point);
        vu0CurMatrixMul(state->matrix);
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&svEffectScaleVector));
        __asm__ __volatile__(
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            : : );
    } else {
        if (svRecordMatrix != 0) {
            __asm__ __volatile__(
                "lqc2 vf26, 0(%0)\n"
                "lqc2 vf27, 16(%0)\n"
                "lqc2 vf28, 32(%0)\n"
                "lqc2 vf29, 48(%0)\n"
                : : "r"(svRecordMatrix));
            vu0TransformPoint(point);
        } else {
            __asm__ __volatile__(
                "lqc2 vf26, 0(%0)\n"
                "lqc2 vf27, 16(%0)\n"
                "lqc2 vf28, 32(%0)\n"
                "lqc2 vf29, 0(%1)\n"
                : : "r"(vu0IdentityMatrix), "r"(point));
        }
        vu0CurMatrixMul(state->matrix);
    }
    if (svPivotAddress != 0) {
        __asm__ __volatile__(
            "mfc1 $8, %0\n"
            "qmtc2 $8, vf1\n"
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmulx.xyzw vf27, vf27, vf1x\n"
            "vmulx.xyzw vf28, vf28, vf1x\n"
            : : "f"(sefGlobalTransform.scale) : "$8", "memory");
    }
    if (svFogValue != 0) {
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(quadRef->quad));
        __asm__ __volatile__(
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            : : );
        if (svFogValue >= 0) {
            vu0TransformPoint(extraSrc);
        }
    } else {
        vu0TransformPoint(extraSrc);
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(quadRef->quad));
        __asm__ __volatile__(
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            : : );
    }
    __asm__ __volatile__("sqc2 vf29, 0(%0)" : : "r"(&record->position) : "memory");
    zAngle = state->angleZ + state->angleZOffset;
    packet = svDrawStatePacket;
    __asm__ __volatile__(
        "lqc2 vf26, 0(%0)\n"
        "lqc2 vf27, 16(%0)\n"
        "lqc2 vf28, 32(%0)\n"
        "lqc2 vf29, 48(%0)\n"
        : : "r"(packet->baseMatrix));
    if (zAngle != 0.0f) {
        vu0RotateCurrentZ(zAngle);
    }
    __asm__ __volatile__(
        "sqc2 vf26, 0(%0)\n"
        "sqc2 vf27, 16(%0)\n"
        "sqc2 vf28, 32(%0)\n"
        "sqc2 vf29, 48(%0)\n"
        : : "r"(record->matrix) : "memory");
    svDrawList.end_index += 0x0E;
}

/*
 * Appends a pair of kind A draw records (28 quadwords): the first carries the GIF tag, the texture block's tex0, the fixed test word 0x31003 and alpha 0x4A; the second the UV block's tex0, svDrawTestWord and the svPairAlphaTable entry of the table index. The vertex (scaled by svVertexScale when set) goes to both, the secondary and primary prims to the first and second, then the extra state, coordinates, quadword and matrix like svDrawListAppendRecordA.
 */
void svDrawListAppendRecordAPair(SvDrawState *state, Vector4 *point, SvDrawCall *quadRef, int useFixedMatrix)
{
    Vector4 *extraSrc;
    Vector4 *extraDst;
    DoublewordVector4 *coordsDst;
    SvDrawRecordA *record;
    const Vector4 *vertexScale;
    int *vertexDst;
    u64 testWord;
    float zAngle;
    DrawStatePacket *packet;

    record = (SvDrawRecordA *) (svDrawList.entries + svDrawList.end_index);
    testWord = svDrawTestWord;
    record[0].gif.words[0] = 0xD02A400000008001ULL;
    record[0].gif.words[1] = 0x000525252521EEEEULL;
    record[0].texture.words[0] = state->texture->tex0;
    record[1].texture.words[0] = state->uv->tex0;
    record[1].texture.words[1] = GS_TEX0_1;
    record[1].test.words[1] = GS_TEST_1;
    record[0].texture.words[1] = GS_TEX0_1;
    record[0].test.words[0] = SV_TEST_FIXED_DATA;
    record[1].test.words[0] = testWord;
    record[0].test.words[1] = GS_TEST_1;
    record[0].alpha.words[0] = SV_ALPHA_FIXED_DATA;
    vertexDst = record[0].vertex.words;
    vertexScale = svVertexScale;
    record[1].alpha.words[0] = svPairAlphaTable[state->tableIndex];
    record[1].alpha.words[1] = GS_ALPHA_1;
    record[0].alpha.words[1] = GS_ALPHA_1;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(state->vertex) : "memory");
    if (vertexScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(vertexScale), "r"(&svColorMaxVector) : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(vertexDst));
    record[1].vertex.quad = record[0].vertex.quad;
    record[0].prim.data = state->secondaryPrim;
    record[0].prim.address = 0;
    record[1].prim.data = state->primaryPrim;
    record[1].prim.address = 0;
    extraDst = &record[0].extraState;
    extraSrc = &state->extraState;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "+m"(*extraDst), "+m"(*extraSrc) : "r"(extraDst), "r"(extraSrc) : "$8", "memory");
    coordsDst = &record[0].coords;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*coordsDst), "+m"(*state->uv) : "r"(coordsDst), "r"(state->uv) : "$8", "memory");
    record[0].coords.words[1] = 0x3F80000000000000ULL;
    if ((state->uv->flags & 0x10) != 0) {
        __asm__ __volatile__(
            "vmaxw.xyzw vf1, vf0, vf0w\n"
            "sqc2 vf1, 0(%0)\n"
            : : "r"(&record[0].quad) : "memory");
    } else {
        QuadwordCopyT0(&record[0].quad, quadRef->quad);
    }
    if (useFixedMatrix != 0) {
        __asm__ __volatile__(
            "lqc2 vf26, 0(%0)\n"
            "lqc2 vf27, 16(%0)\n"
            "lqc2 vf28, 32(%0)\n"
            "lqc2 vf29, 48(%0)\n"
            : : "r"(&svEffectMatrix));
        vu0TransformPoint(point);
        vu0CurMatrixMul(state->matrix);
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&svEffectScaleVector));
        __asm__ __volatile__(
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            : : );
    } else {
        if (svRecordMatrix != 0) {
            __asm__ __volatile__(
                "lqc2 vf26, 0(%0)\n"
                "lqc2 vf27, 16(%0)\n"
                "lqc2 vf28, 32(%0)\n"
                "lqc2 vf29, 48(%0)\n"
                : : "r"(svRecordMatrix));
            vu0TransformPoint(point);
        } else {
            __asm__ __volatile__(
                "lqc2 vf26, 0(%0)\n"
                "lqc2 vf27, 16(%0)\n"
                "lqc2 vf28, 32(%0)\n"
                "lqc2 vf29, 0(%1)\n"
                : : "r"(vu0IdentityMatrix), "r"(point));
        }
        vu0CurMatrixMul(state->matrix);
    }
    if (svPivotAddress != 0) {
        __asm__ __volatile__(
            "mfc1 $8, %0\n"
            "qmtc2 $8, vf1\n"
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmulx.xyzw vf27, vf27, vf1x\n"
            "vmulx.xyzw vf28, vf28, vf1x\n"
            : : "f"(sefGlobalTransform.scale) : "$8", "memory");
    }
    if (svFogValue != 0) {
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(quadRef->quad));
        __asm__ __volatile__(
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            : : );
        if (svFogValue >= 0) {
            vu0TransformPoint(extraSrc);
        }
    } else {
        vu0TransformPoint(extraSrc);
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(quadRef->quad));
        __asm__ __volatile__(
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            : : );
    }
    __asm__ __volatile__("sqc2 vf29, 0(%0)" : : "r"(&record[0].position) : "memory");
    zAngle = state->angleZ + state->angleZOffset;
    packet = svDrawStatePacket;
    __asm__ __volatile__(
        "lqc2 vf26, 0(%0)\n"
        "lqc2 vf27, 16(%0)\n"
        "lqc2 vf28, 32(%0)\n"
        "lqc2 vf29, 48(%0)\n"
        : : "r"(packet->baseMatrix));
    if (zAngle != 0.0f) {
        vu0RotateCurrentZ(zAngle);
    }
    __asm__ __volatile__(
        "sqc2 vf26, 0(%0)\n"
        "sqc2 vf27, 16(%0)\n"
        "sqc2 vf28, 32(%0)\n"
        "sqc2 vf29, 48(%0)\n"
        : : "r"(record[0].matrix) : "memory");
    svDrawList.end_index += 28;
}

/* The part svDrawListAppendPart packs: the transform, then the position, the register source and the extents. */
typedef struct SvPartSource {
    Vector4 header;                 /* +0x00: copied into the packet */
    u64 registerData;               /* +0x10 */
    unsigned char unmodeled_18[0x1C - 0x18];
    unsigned short flags;           /* +0x1C: bit 0x10 selects the copy path */
} SvPartSource;

typedef struct SvPart {
    SefPartTransform transform;     /* +0x000 */
    unsigned char unmodeled_058[0x1D0 - 0x58];
    int position[4];                /* +0x1D0: converted and clamped to the scene bounds */
    unsigned char unmodeled_1E0[0x1E8 - 0x1E0];
    SvPartSource *source;           /* +0x1E8 */
    unsigned char unmodeled_1EC[0x1F0 - 0x1EC];
    Vector4 extentA;                /* +0x1F0 */
    unsigned char unmodeled_200[0x28C - 0x200];
    int index;                      /* +0x28C: selects the svAlphaTestTable entry */
    Vector4 extentB;                /* +0x290 */
} SvPart;

/* One quadword as two doublewords. */
typedef struct DrawQuad {
    u64 low;
    u64 high;
} DrawQuad;

/* The 14-quadword draw packet appended to svDrawList: a GIF tag, the register list, then the A+D entries and the vectors. */
typedef struct SvPartPacket {
    DoublewordVector4 gif;
    DoublewordVector4 tex;          /* +0x10 */
    DoublewordVector4 test;         /* +0x20 */
    DoublewordVector4 alpha;        /* +0x30 */
    Vector4 vertex;                 /* +0x40 */
    int position[4];                /* +0x50 */
    DrawQuad extentA;               /* +0x60: high doubleword zero */
    Vector4 extentB;                /* +0x70 */
    DoublewordVector4 source;       /* +0x80: the source header, high doubleword 0x3F80000000000000 */
    Vector4 ref;                    /* +0x90 */
    Matrix4 rows;                   /* +0xA0 */
} SvPartPacket;

/* Copies one quadword through t0. */
static inline void svCopyQuadword(Vector4 *destination, const Vector4 *source)
{
    QuadwordCopyT0(destination, source);
}

/* Appends one part packet to svDrawList: the transformed matrix rows (scaled by svEffectScaleVector when `scaled`), then the position, the extents and the source. */
void svDrawListAppendPart(SvPart *part, const Vector4 *vertex, SvDrawCall *ref, int scaleRows) {
    SvPartPacket *packet;
    int *positionDst;
    const Vector4 *scaleVector;

    packet = (SvPartPacket *) (svDrawList.entries + svDrawList.end_index);
    packet->gif.words[0] = 0xD02A400000008001ULL;
    packet->gif.words[1] = 0x000525252521EEEEULL;
    packet->tex.words[0] = part->source->registerData;
    packet->tex.words[1] = GS_TEX0_1;
    packet->test.words[0] = svDrawTestWord;
    packet->test.words[1] = GS_TEST_1;
    positionDst = packet->position;
    scaleVector = svVertexScale;
    packet->alpha.words[0] = svAlphaTestTable[part->index];
    packet->alpha.words[1] = GS_ALPHA_1;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(part->position) : "memory");
    if (scaleVector != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(scaleVector), "r"(&svColorMaxVector) : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(positionDst) : "memory");
    QuadwordCopyT0(&packet->vertex, vertex);
    QuadwordCopyT0(&packet->extentA, &part->extentA);
    packet->extentA.high = 0;
    QuadwordCopyT0(&packet->extentB, &part->extentB);
    QuadwordCopyT0(&packet->source, part->source);
    packet->source.words[1] = 0x3F80000000000000ULL;
    if ((part->source->flags & 0x10) != 0) {
        __asm__ __volatile__("vmaxw.xyzw vf1, vf0, vf0w\n\tsqc2 vf1, 0(%0)" : : "r"(&packet->ref) : "memory");
    } else {
        svCopyQuadword(&packet->ref, ref->quad);
    }
    __asm__ __volatile__(
        "lqc2 vf26, 0(%0)\n\t"
        "lqc2 vf27, 16(%0)\n\t"
        "lqc2 vf28, 32(%0)\n\t"
        "lqc2 vf29, 48(%0)"
        : : "r"(&part->transform.matrix) : "memory");
    if (scaleRows != 0) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n\t"
            "vmulx.xyzw vf26, vf26, vf1x\n\t"
            "vmuly.xyzw vf27, vf27, vf1y\n\t"
            "vmulz.xyzw vf28, vf28, vf1z\n\t"
            "vmulw.xyzw vf29, vf29, vf1w"
            : : "r"(&svEffectScaleVector) : "memory");
    }
    __asm__ __volatile__(
        "sqc2 vf26, 0(%0)\n\t"
        "sqc2 vf27, 16(%0)\n\t"
        "sqc2 vf28, 32(%0)\n\t"
        "sqc2 vf29, 48(%0)"
        : : "r"(packet->rows) : "memory");
    svDrawList.end_index += 14;
}

/*
 * Appends a pair of kind A draw records (28 quadwords) like svDrawListAppendRecordAPair, but places the point directly: the point goes to the first record's position, and the matrix is the state's own, scaled per row by svEffectScaleVector when flag is set.
 */
void svDrawListAppendStatePair(SvDrawState *state, Vector4 *point, SvDrawCall *quadRef, int scaleRows)
{
    Vector4 *pointDst;
    Vector4 *extraStateSrc;
    Vector4 *extraStateDst;
    DoublewordVector4 *coordsDst;
    SvDrawRecordA *pairRecords;
    const Vector4 *vertexScale;
    int *vertexDst;
    u64 testWord;

    pairRecords = (SvDrawRecordA *) (svDrawList.entries + svDrawList.end_index);
    testWord = svDrawTestWord;
    pairRecords[0].gif.words[0] = 0xD02A400000008001ULL;
    pairRecords[0].gif.words[1] = 0x000525252521EEEEULL;
    pairRecords[0].texture.words[0] = state->texture->tex0;
    pairRecords[1].texture.words[0] = state->uv->tex0;
    pairRecords[1].texture.words[1] = GS_TEX0_1;
    pairRecords[1].test.words[1] = GS_TEST_1;
    pairRecords[0].texture.words[1] = GS_TEX0_1;
    pairRecords[0].test.words[0] = SV_TEST_FIXED_DATA;
    pairRecords[1].test.words[0] = testWord;
    pairRecords[0].test.words[1] = GS_TEST_1;
    pairRecords[0].alpha.words[0] = SV_ALPHA_FIXED_DATA;
    vertexDst = pairRecords[0].vertex.words;
    vertexScale = svVertexScale;
    pairRecords[1].alpha.words[0] = svPairAlphaTable[state->tableIndex];
    pairRecords[1].alpha.words[1] = GS_ALPHA_1;
    pairRecords[0].alpha.words[1] = GS_ALPHA_1;
    __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(state->vertex) : "memory");
    if (vertexScale != 0) {
        __asm__ __volatile__(
            "lqc2 vf2, 0(%0)\n\t"
            "lqc2 vf3, 0(%1)\n\t"
            "vitof0.xyzw vf1, vf1\n\t"
            "vmul.xyzw vf1, vf1, vf2\n\t"
            "vmini.xyz vf1, vf1, vf3\n\t"
            "vftoi0.xyzw vf1, vf1"
            : : "r"(vertexScale), "r"(&svColorMaxVector) : "memory");
    }
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(vertexDst));
    pairRecords[1].vertex.quad = pairRecords[0].vertex.quad;
    pointDst = &pairRecords[0].position;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*pointDst), "+m"(*point) : "r"(pointDst), "r"(point) : "$8", "memory");
    pairRecords[0].prim.data = state->secondaryPrim;
    pairRecords[0].prim.address = 0;
    pairRecords[1].prim.data = state->primaryPrim;
    pairRecords[1].prim.address = 0;
    extraStateDst = &pairRecords[0].extraState;
    extraStateSrc = &state->extraState;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "+m"(*extraStateDst), "+m"(*extraStateSrc) : "r"(extraStateDst), "r"(extraStateSrc) : "$8", "memory");
    coordsDst = &pairRecords[0].coords;
    __asm__ __volatile__("lq $8, 0(%3)\nsq $8, 0(%2)\n" : "=m"(*coordsDst), "+m"(*state->uv) : "r"(coordsDst), "r"(state->uv) : "$8", "memory");
    pairRecords[0].coords.words[1] = 0x3F80000000000000ULL;
    QuadwordCopyT0(&pairRecords[0].quad, quadRef->quad);
    __asm__ __volatile__(
        "lqc2 vf26, 0(%0)\n"
        "lqc2 vf27, 16(%0)\n"
        "lqc2 vf28, 32(%0)\n"
        "lqc2 vf29, 48(%0)\n"
        : : "r"(state->matrix));
    if (scaleRows != 0) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n"
            "vmulx.xyzw vf26, vf26, vf1x\n"
            "vmuly.xyzw vf27, vf27, vf1y\n"
            "vmulz.xyzw vf28, vf28, vf1z\n"
            "vmulw.xyzw vf29, vf29, vf1w\n"
            : : "r"(&svEffectScaleVector));
    }
    __asm__ __volatile__(
        "sqc2 vf26, 0(%0)\n"
        "sqc2 vf27, 16(%0)\n"
        "sqc2 vf28, 32(%0)\n"
        "sqc2 vf29, 48(%0)\n"
        : : "r"(pairRecords[0].matrix) : "memory");
    svDrawList.end_index += 28;
}

/* Sets the part's scale from one of three keys (0x40, 0x80, 0x100 select scaleKey40, scaleKey80 or scaleKey100); a unit scale leaves scaled at 0. Otherwise the matrix becomes the scaled identity with the position in row 3. Returns kind. */
int sefInitPartTransform(SefPartTransform *part, int kind, int scaleKey40, int scaleKey80, int scaleKey100) {
    float partScale = 1.0f;
    const Matrix4 *unitMatrix;

    if ((kind & 0x40) != 0) {
        partScale = sefGetEntryScale(scaleKey40);
    } else if ((kind & 0x80) != 0) {
        partScale = sefGetEntryScale(scaleKey80);
    } else if ((kind & 0x100) != 0) {
        partScale = sefGetEntryScale(scaleKey100);
    }
    part->scaled = 0;
    if (partScale != 1.0f) {
        part->scaled = 1;
        part->scale = partScale;
        part->kind = kind;
        sefGetPosition(&part->position, 0, kind);
        unitMatrix = &MMathUnitMatrix;
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&part->position) : "memory");
        __asm__ __volatile__("vmove.w vf1, vf0" : : : "memory");
        __asm__ __volatile__("lqc2 vf2, 32(%0)" : : "r"(unitMatrix) : "memory");
        __asm__ __volatile__("lqc2 vf3, 16(%0)" : : "r"(unitMatrix) : "memory");
        __asm__ __volatile__("lqc2 vf4, 0(%0)" : : "r"(unitMatrix) : "memory");
        __asm__ __volatile__("sqc2 vf1, 48(%0)" : : "r"(part) : "memory");
        __asm__ __volatile__("sqc2 vf2, 32(%0)" : : "r"(part) : "memory");
        __asm__ __volatile__("sqc2 vf3, 16(%0)" : : "r"(part) : "memory");
        __asm__ __volatile__("sqc2 vf4, 0(%0)" : : "r"(part) : "memory");
        part->matrix[0][0] = partScale;
        part->matrix[1][1] = partScale;
        part->matrix[2][2] = partScale;
    }
    return kind;
}

/* Reads the kind of the item's entry two places before entries[0]; kinds 0x10 and 0x11 use fallbackKind instead. */
void svInitItemDrawKind(SefPartTransform *part, SvDrawItem *item, int scaleKey40, int scaleKey80, int scaleKey100) {
    short kind;

    kind = item->entries[-2];
    if ((unsigned int) (kind - 0x10) < 2U) {
        kind = item->fallbackKind;
    }
    sefInitPartTransform(part, kind, scaleKey40, scaleKey80, scaleKey100);
}


/* The current effect record (sefCurrentRecord): the scale vector at +0x20, three ints at +0x50, +0x54 and +0x58 and the selector at +0x74. */
typedef struct SefCurrentRecord {
    unsigned char unmodeled_00[0x20];
    Vector4 scale;
    unsigned char unmodeled_30[0x50 - 0x30];
    int scaleKey40;            /* +0x50: the scale key for kind 0x40 */
    int scaleKey100;           /* +0x54: the scale key for kind 0x100 */
    int scaleKey80;            /* +0x58: the scale key for kind 0x80 */
    unsigned char unmodeled_5C[0x74 - 0x5C];
    int drawSelector;          /* +0x74 */
} SefCurrentRecord;

extern SefCurrentRecord *sefCurrentRecord;
extern unsigned short svSceneFlags;
/* Stays asm-labelled: svPivotAddress is also read as an int; a single pointer declaration changed the code. */
extern Vector4 *svPivot[1] __asm__("svPivotAddress");
/* Stays asm-labelled: svEffectCameraMatrices is also declared as Matrix4[2]; the one-matrix size is in the object. */
extern Vector4 svDefaultScaleVectors[3];
extern const DrawStateWords svDefaultDrawStateTable;
extern const DrawStateWords svMode7DrawStateTable;
extern const DrawStateWords svMode9DrawStateTable;
extern const DrawStateWords svSpriteDrawStateTable;
extern const u64 svDrawRefUvWords[2];
extern const Vector4 svDefaultVertexScale;
extern unsigned char sefAppliedLightSet[];

/* Unused in this file; the original compiled it here (it takes one label number ahead of svDrawImageItem). */
static inline void svClearFlags(int *flags, int mask)
{
    *flags &= ~mask;
}

static inline void svSubVectorXYZ(Vector4 *destination, Vector4 *minuend, Vector4 *subtrahend)
{
    __asm__ __volatile__(
        "lqc2 vf1, 0(%4)\n"
        "lqc2 vf2, 0(%5)\n"
        "vsub.xyz vf1, vf1, vf2\n"
        "sqc2 vf1, 0(%3)\n"
        : "=m" (destination->x), "=m" (destination->y), "=m" (destination->z)
        : "r" (destination), "r" (minuend), "r" (subtrahend));
}

static inline void svCopyVector(Vector4 *destination, Vector4 *source)
{
    __asm__ __volatile__(
        "lq $8, 0(%5)\n"
        "sq $8, 0(%4)\n"
        : "=m" (destination->x), "=m" (destination->y), "=m" (destination->z), "=m" (destination->w)
        : "r" (destination), "r" (source)
        : "$8");
}

extern Matrix4 svEffectCameraProjectionMatrix;

/* Draws one item: the 256 drawIndex rows select slots whose kind and _draw3D mode pick the draw call. */
void svDrawImageItem(SvDrawItem *item, int renderState)
{
    AlignedVector4 matrixRows[4];
    AlignedVector4 scaledPosition;
    SvDrawCall ctx;
    AlignedVector4 localPosition;
    int i;
    int s19Bit3Flag;
    int hasPartScale;
    int usesItemMatrix;
    int clipFlag;
    int s19Mask8;
    int reflectFlag;
    SvTransformBlock *slots;
    Matrix4 *itemMatrix;
    int s19;
    SvTransformBlock *slot;
    SefPartTransform *scalePart;
    const DrawStateWords *stateTable;
    Vector4 *slotPosition;
    short *entries;
    SefCurrentRecord *currentRecord;
    XglPacket *xglPacket;
    short *indexPtr;
    short idx;
    int drawMode;
    int kind;
    int hasRecordMatrix;
    int scaledRecord;
    int isKind8;
    int drawState;
    int flagsMode;
    int showFlagged;
    int modeFlags;
    int flags;

    entries = item->entries;
    s19Bit3Flag = 0;
    hasPartScale = 0;
    drawMode = _draw3D;
    modeFlags = item->modeFlags;
    flagsMode = modeFlags & 0x6;
    kind = entries[-2];
    isKind8 = kind == 8;
    hasRecordMatrix = 0;
    if (((unsigned short) entries[-2] & 0x40) != 0) {
        hasRecordMatrix = svRecordMatrix != 0;
    }
    scaledRecord = hasRecordMatrix;
    usesItemMatrix = 0;
    if (kind == 2 || kind == 7) {
        usesItemMatrix = 1;
    }
    s19 = 0;
    if (item->s19Keys != 0) {
        s19 = item->s19Keys[-1];
    }
    clipFlag = (unsigned short) (item->imageFlags & 0x4000);
    reflectFlag = modeFlags & 0x1;
    showFlagged = svSceneFlags & 0x2;
    s19Mask8 = 0;
    slots = (SvTransformBlock *) sevGetPtAllocator2(0);
    stateTable = &svDefaultDrawStateTable;
    if (svRecordSourcePointer == 0) {
        flagsMode = 0;
    }
    svFogValue = -1;
    itemMatrix = 0;
    flags = item->imageFlags;
    svItemDrawFlags = flags & 0xCB;
    svItemModeFlags = flagsMode;
    if (_sefBattleMode != 0) {
        if (item->keyHeaders != 0 && item->keyHeaders[-1].kind != 0) {
            svItemDrawFlags |= 0x40;
        }
        if ((unsigned int) (sefCurrentRecord->drawSelector - 55) < 3U) {
            svItemDrawFlags |= 0x40;
        }
        if ((unsigned int) (sefCurrentRecord->drawSelector - 0x32001) < 16U) {
            svItemDrawFlags |= 0x40;
        }
    }
    if ((flags & 0x10) != 0 && showFlagged == 0) {
        return;
    }
    if (drawMode == SV_RENDER_2D_SPRITE && !isKind8) {
        return;
    }
    if (drawMode != SV_RENDER_2D_SPRITE && isKind8) {
        return;
    }
    ctx.texture = svUnitQuadCornerMatrix;
    ctx.uv = svDrawRefUvWords;
    ctx.mode = s19;
    svPivot[0] = 0;
    scalePart = &sefGlobalTransform;
    scalePart->scaled = 0;
    if (hasRecordMatrix != 0) {
        scaledRecord = MMathIsVectorEqual(&svEffectScaleVector, svDefaultScaleVectors) == 0 ? hasRecordMatrix : 0;
    }
    if (drawMode != SV_RENDER_2D_SPRITE && (item->modeFlags & 0x2000) != 0) {
        currentRecord = sefCurrentRecord;
        svInitItemDrawKind(scalePart, item, currentRecord->scaleKey40, currentRecord->scaleKey80, currentRecord->scaleKey100);
        hasPartScale = scalePart->scaled;
    }
    if (drawMode == SV_RENDER_2D_SPRITE) {
        svDrawMode = clipFlag != 0 ? 4 : 3;
        stateTable = &svSpriteDrawStateTable;
    } else if (drawMode == SV_RENDER_2D) {
        if ((item->modeFlags & 0x80) != 0 || svZTestEnable == 0) {
            svDrawTestWord = SV_ZTEST_OFF;
        } else {
            svDrawTestWord = SV_ZTEST_ON;
        }
        if (ctx.mode == 0) {
            drawState = clipFlag != 0 ? 2 : 1;
            svItemDrawFlags &= ~2;
            svDrawMode = drawState;
            svPolygonAxesMode = drawState;
        } else {
            if ((s19 & 1) != 0) {
                svDrawMode = 5;
            } else if ((s19 & 2) != 0) {
                svDrawMode = 7;
                stateTable = &svMode7DrawStateTable;
            } else if ((s19 & 8) != 0) {
                svItemDrawFlags &= ~2;
                s19Bit3Flag = 1;
                svDrawMode = 11;
            } else {
                svDrawMode = 9;
                stateTable = &svMode9DrawStateTable;
            }
            svPolygonAxesMode = svDrawMode;
            if ((svItemDrawFlags & 2) != 0) {
                svDrawMode = 13;
            }
            if (clipFlag != 0) {
                svPolygonClipFlag = 1;
                svDrawMode = svDrawMode + 1;
            } else {
                svPolygonClipFlag = 0;
            }
        }
    } else {
        s19 &= 0x8;
        s19Mask8 = s19;
        svDrawMode = 0;
        svPolygonAxesMode = 0;
        svActiveLightSet = ((item->imageFlags & 0x4) != 0) ? sefAppliedLightSet : svEffectLightSet;
    }

    svDrawStatePacket->stateWords = *stateTable;
    if (usesItemMatrix != 0) {
        if (item->entries[-2] == 2) {
            itemMatrix = &svEffectCameraMatrices[0];
        } else {
            itemMatrix = &svEffectCameraProjectionMatrix;
        }
        __asm__ __volatile__(
            "lqc2 vf1, 32(%0)\n\t"
            "lqc2 vf2, 16(%0)\n\t"
            "lqc2 vf3, 0(%0)\n\t"
            "sqc2 vf0, 48(%1)\n\t"
            "sqc2 vf1, 32(%1)\n\t"
            "sqc2 vf2, 16(%1)\n\t"
            "sqc2 vf3, 0(%1)"
            : : "r"(&MMathUnitMatrix), "r"(&svDrawStatePacket->baseMatrix) : "memory");
        MMathMulMatrix(&svDrawStatePacket->matrixA, svViewMatrix, itemMatrix);
    } else {
        MMathMulMatrix(&svDrawStatePacket->baseMatrix, svViewMatrix, (Matrix4 *) &sefCameraMatrix);
        if (scaledRecord == 0) {
            MMathScaleMatrix(&svDrawStatePacket->baseMatrix, &svDrawStatePacket->baseMatrix,
                             &sefCurrentRecord->scale);
        }
        QuadwordCopyMatrixT0(&svDrawStatePacket->matrixA, svViewMatrix);
    }
    if (s19Bit3Flag != 0) {
        QuadwordCopyMatrixT0(&svDrawStatePacket->matrixA, svViewMatrix);
    } else if (svRecordMatrix != 0 && (svItemDrawFlags & 0x2) == 0) {
        if (scaledRecord != 0) {
            MMathMulMatrix(&svDrawStatePacket->matrixA, &svDrawStatePacket->matrixA,
                           &svEffectMatrix);
        } else {
            MMathMulMatrix(&svDrawStatePacket->matrixA, &svDrawStatePacket->matrixA,
                           svRecordMatrix);
        }
    }
    if (hasPartScale != 0 && (svItemDrawFlags & 0x2) == 0) {
        MMathScaleMatrixRotS(&svDrawStatePacket->baseMatrix, &svDrawStatePacket->baseMatrix,
                             sefGlobalTransform.scale);
        svDrawStatePacket->baseMatrix[3][3] = 1.0f;
    }
    if (item->fogTable != 0) {
        svFogValue = item->fogTable[15];
    }
    if (svDrawMode != 0) {
        svDrawSetState((short) (item->modeFlags & 0x200), item->keyHeaders != 0 ? item->keyHeaders[-1].kind : 0);
        svDrawSubmitMode();
        svDrawListAppend(SV_DRAW_LIST_FLUSH_CODE, 0, svFogValue);
    }
    if ((flagsMode & 0x2) != 0) {
        svRecordSourceRowsPointer = (Vector4 *) ((u8 *) svRecordSourcePointer + 256);
    } else {
        svRecordSourceRowsPointer = 0;
    }

    indexPtr = item->pointHandles;
    for (i = 0; i < 256; i++, indexPtr++) {
        idx = *indexPtr;
        if (idx < 0) {
            continue;
        }
        slot = slots + idx;
        if (slot->drawData == 0) {
            continue;
        }
        svVertexScale = svRecordScale;
        if (drawMode != SV_RENDER_3D_MODEL && sefFlagTest(2) != 0 && slot->index == 0) {
            svVertexScale = &svDefaultVertexScale;
        }

        switch (drawMode) {
        case SV_RENDER_3D_MODEL:
            if (slot->part->kind != 6) {
                continue;
            }
            if (hasPartScale != 0) {
                if (reflectFlag != 0) {
                    svSubVectorXYZ(&localPosition, &slot->rotation_aux, &sefGlobalTransform.position);
                    svBuildItemMatrix(&localPosition, slot, scaledRecord, s19Mask8);
                    svCopyVector((Vector4 *) &sefGlobalTransform.matrix[3], &sefGlobalTransform.position);
                } else {
                    svSubVectorXYZ(&localPosition, &slot->rotation_aux, &slot->pivot);
                    svBuildItemMatrix(&localPosition, slot, scaledRecord, s19Mask8);
                    svCopyVector((Vector4 *) &sefGlobalTransform.matrix[3], &slot->pivot);
                }
                vu0CurMatrixMulRight((Vector4 *) &sefGlobalTransform);
            } else {
                svBuildItemMatrix(&slot->rotation_aux, slot, scaledRecord, s19Mask8);
            }
            if (svRecordSourceRowsPointer != 0) {
                vu0CurMatrixMulRight(svRecordSourceRowsPointer);
            }
            if (usesItemMatrix != 0) {
                vu0CurMatrixMulRight((Vector4 *) itemMatrix);
            }
            __asm__ __volatile__(
                "sqc2 vf26, 0(%0)\n\t"
                "sqc2 vf27, 16(%0)\n\t"
                "sqc2 vf28, 32(%0)\n\t"
                "sqc2 vf29, 48(%0)"
                : : "r"(matrixRows) : "memory");
            svSetModelRenderState((SvRenderItem *) slot, (Matrix4 *) matrixRows, renderState);
            break;
        case SV_RENDER_2D_SPRITE:
            if (slot->part->kind != 4) {
                continue;
            }
            if (clipFlag != 0) {
                svDrawListAppendSpriteRecordPair((SvDrawState *) slot, svPairAlphaTable[slot->index]);
            } else {
                svDrawListAppendSpriteRecord((SvDrawState *) slot, svAlphaTestTable[slot->index]);
            }
            svDrawListTryFlush();
            break;
        case SV_RENDER_2D:
            if (slot->part->kind != 4) {
                continue;
            }
            ctx.quad = &slot->scale;
            slotPosition = &slot->rotation_aux;
            if (hasPartScale != 0) {
                __asm__ __volatile__("lqc2 vf20, 0(%0)" : : "r"(slotPosition));
                if (reflectFlag != 0) {
                    svPivot[0] = &sefGlobalTransform.position;
                    __asm__ __volatile__(
                        "lqc2 vf1, 0(%0)\n\t"
                        "vsub.xyz vf20, vf20, vf1"
                        : : "r"(&sefGlobalTransform.position));
                    __asm__ __volatile__(
                        "mfc1 $8, %0\n\t"
                        "qmtc2.ni $8, vf1\n\t"
                        "vmulx.xyz vf20, vf20, vf1x"
                        : : "f"(sefGlobalTransform.scale) : "$8");
                    __asm__ __volatile__(
                        "lqc2 vf1, 0(%0)\n\t"
                        "vadd.xyz vf20, vf20, vf1"
                        : : "r"(&sefGlobalTransform.position));
                } else {
                    svPivot[0] = &slot->pivot;
                    __asm__ __volatile__(
                        "lqc2 vf1, 0(%0)\n\t"
                        "vsub.xyz vf20, vf20, vf1"
                        : : "r"(&slot->pivot));
                    __asm__ __volatile__(
                        "mfc1 $8, %0\n\t"
                        "qmtc2.ni $8, vf1\n\t"
                        "vmulx.xyz vf20, vf20, vf1x"
                        : : "f"(sefGlobalTransform.scale) : "$8");
                    __asm__ __volatile__(
                        "lqc2 vf1, 0(%0)\n\t"
                        "vadd.xyz vf20, vf20, vf1"
                        : : "r"(&slot->pivot));
                }
                __asm__ __volatile__("sqc2 vf20, 0(%0)" : : "r"(&scaledPosition) : "memory");
                slotPosition = &scaledPosition;
            }
            if (ctx.mode == 0) {
                if (clipFlag != 0) {
                    svDrawListAppendStatePair((SvDrawState *) slot, slotPosition, (SvDrawCall *) &ctx, scaledRecord);
                } else {
                    svDrawListAppendPart((SvPart *) slot, slotPosition, &ctx, scaledRecord);
                }
            } else if (clipFlag != 0) {
                if (svDrawMode == 12) {
                    svDrawListAppendRecordAPair((SvDrawState *) slot, slotPosition, (SvDrawCall *) &ctx, scaledRecord);
                } else if (svDrawMode == 14) {
                    svDrawListFlush(svDrawList.end_index - svDrawList.begin_index);
                    svDrawPolygonItem((SvPolygonItem *) slot, slotPosition, &ctx, scaledRecord);
                    svDrawListFlush(svDrawList.end_index - svDrawList.begin_index);
                } else {
                    svDrawListAppendRecordBPair((SvDrawState *) slot, slotPosition, (SvDrawCall *) &ctx, scaledRecord);
                }
            } else {
                if (svDrawMode == 11) {
                    svDrawListAppendRecordA((SvDrawState *) slot, slotPosition, (SvDrawCall *) &ctx, scaledRecord);
                } else if (svDrawMode == 13) {
                    svDrawListFlush(svDrawList.end_index - svDrawList.begin_index);
                    svDrawPolygonItem((SvPolygonItem *) slot, slotPosition, &ctx, scaledRecord);
                    svDrawListFlush(svDrawList.end_index - svDrawList.begin_index);
                } else {
                    svDrawListAppendSlot((SefSlotRecord *) slot, slotPosition, &ctx, scaledRecord);
                }
            }
            svDrawListTryFlush();
            break;
        }
    }

    svDrawListFlush(svDrawList.end_index - svDrawList.begin_index);
    xglPacket = xglPacketGetCurrent();
    sceVif1PkCnt(xglPacket, 0);
    sceVif1PkAddCode(xglPacket, 0x11000000);
}

/* Makes mapper `index` the active one (svActiveMapperIndex caches it) and refers its packet; does nothing when it is already active. */
void svSelectImageMapper(int index) {
    if (svActiveMapperIndex != index) {
        svActiveMapperIndex = index;
        svLoadImageList(&_imageMapper.typeLists[index]);
        svRefMapperPacket();
    }
}

/* Chooses the mapper for an item: mapper 0 when its 0x2000 flag is set, else the requested index svRequestedMapperIndex. Does nothing while _draw3D is 2. */
void svSelectImageMapperForItem(SvDrawItem *item) {
    int index;

    if (_draw3D != SV_RENDER_3D_MODEL) {
        if (item->imageFlags & 0x2000) {
            if (svActiveMapperIndex != 0) {
                svActiveMapperIndex = 0;
                svLoadImageList(&_imageMapper.typeLists[0]);
                svRefMapperPacket();
            }
        } else {
            index = svRequestedMapperIndex;
            if (svActiveMapperIndex != index) {
                svActiveMapperIndex = index;
                svLoadImageList(&_imageMapper.typeLists[index]);
                svRefMapperPacket();
            }
        }
    }
}

/* sefCurrentRecord seen as a one-member record slot. */
typedef struct SefCurrentRecordSlot {
    SefEffectRecord *record;
} SefCurrentRecordSlot;

/* The definition in sef.c returns SefLocalData *; this file views the same record as SvDrawItem. */
struct SefLocalData;

/* Draws the depth-sorted image slots of the current effect record: walks the 512 sort buckets from the back, switches the
 * draw state (mode, texture-state block, clamp, fog) whenever the line changes, then appends each image slot through the
 * record builder of its mode. */
void svDrawSortedImages(SefEffectRecord *effectRecord)
{
    SvDrawCall drawRef;
    int bucketIndex;
    SvDrawItem *lineData;
    SortItem *sortItem;
    SvDrawState *slotState;
    unsigned char *slotBase;
    const u64 *stateTable;
    const u64 *lastStateTable;
    unsigned short additiveBit;
    short lineMode;
    int lastLineIndex;
    int stateChanged;
    int pendingCount;
    short itemIndex;

    lastStateTable = 0;
    pendingCount = 0;
    additiveBit = 0;
    stateTable = svDefaultDrawStateTable.words;
    lastLineIndex = -1;
    slotBase = sevGetPtAllocator2(0);
    drawRef.texture = svUnitQuadCornerMatrix;
    drawRef.uv = svDrawRefUvWords;
    svDrawMode = 5;
    sefGlobalTransform.scaled = 0;
    drawRef.mode = -1;
    svPivotAddress = 0;
    ((DoublewordVector4 *) &svDrawStatePacket->clamp)->words[1] = 8;
    ((DoublewordVector4 *) &svDrawStatePacket->clamp)->words[0] = 5;
    MMathMulMatrix(&svDrawStatePacket->baseMatrix, svViewMatrix, (Matrix4 *) sefCameraMatrix);
    MMathScaleMatrix(&svDrawStatePacket->baseMatrix, &svDrawStatePacket->baseMatrix, &((SefCurrentRecordSlot *) &sefCurrentRecord)->record->scale);
    svDrawSubmitMode();
    bucketIndex = 511;
    do {
        sortItem = (SortItem *) &svSortBuckets[bucketIndex];
        while (sortItem->next >= 0) {
            itemIndex = sortItem->next;
            sortItem = &svSortItems[itemIndex];
            lineData = (SvDrawItem *) sefGetLineAdr(sortItem->flags);
            if (lineData == 0) {
                continue;
            }
            if (sortItem->flags != lastLineIndex) {
                lastLineIndex = sortItem->flags;
                stateChanged = 0;
                additiveBit = lineData->imageFlags & 0x4000;
                lineMode = 0;
                if (lineData->s19Keys != 0) {
                    lineMode = lineData->s19Keys[-1];
                }
                drawRef.mode = lineMode;
                if ((lineData->modeFlags & 0x80) || svZTestEnable == 0) {
                    svDrawTestWord = 0x2000D;
                } else {
                    svDrawTestWord = 0x5000D;
                }
                if (pendingCount > 0) {
                    pendingCount = 0;
                    svDrawListFlush(svDrawList.end_index - svDrawList.begin_index);
                }
                if (lineData->fogTable != 0) {
                    svFogValue = lineData->fogTable[0xF];
                } else {
                    svFogValue = -1;
                }
                if (lineMode == 0) {
                    svDrawMode = additiveBit ? 2 : 1;
                    stateTable = svDefaultDrawStateTable.words;
                } else {
                    if (lineMode & 1) {
                        stateTable = svDefaultDrawStateTable.words;
                        svDrawMode = 5;
                    } else if (lineMode & 2) {
                        svDrawMode = 7;
                        stateTable = svMode7DrawStateTable.words;
                    } else if (lineMode & 8) {
                        stateTable = svDefaultDrawStateTable.words;
                        svDrawMode = 11;
                    } else {
                        svDrawMode = 9;
                        stateTable = svMode9DrawStateTable.words;
                    }
                    if (additiveBit) {
                        svDrawMode++;
                    }
                }
                if (lastStateTable != stateTable) {
                    lastStateTable = stateTable;
                    stateChanged = 1;
                    svDrawStatePacket->textureState[0].words[0] = lastStateTable[0];
                    svDrawStatePacket->textureState[0].words[1] = lastStateTable[1];
                    svDrawStatePacket->textureState[1].words[0] = lastStateTable[2];
                    svDrawStatePacket->textureState[1].words[1] = lastStateTable[3];
                    svDrawStatePacket->textureState[2].words[0] = lastStateTable[4];
                    svDrawStatePacket->textureState[2].words[1] = lastStateTable[5];
                    svDrawStatePacket->textureState[3].words[0] = lastStateTable[6];
                    svDrawStatePacket->textureState[3].words[1] = lastStateTable[7];
                }
                svDrawSetState(lineData->modeFlags & 0x200, lineData->keyHeaders != 0 ? lineData->keyHeaders[-1].kind : 0);
                if (stateChanged) {
                    svDrawSubmitMode();
                }
                svDrawListAppend(0x28, 0, svFogValue);
            }
            /* Kept: sevGetPtAllocator2 returns unsigned char *, so the 0x2C0-byte slot stride stays a byte offset. */
            slotState = (SvDrawState *) (slotBase + sortItem->id * 0x2C0);
            if (slotState->uv->kind != 4) {
                continue;
            }
            svVertexScale = svRecordScale;
            if (sefFlagTest(2)) {
                if (slotState->tableIndex == 0) {
                    svVertexScale = &svDefaultVertexScale;
                }
            }
            drawRef.quad = &slotState->pointB;
            if (drawRef.mode == 0) {
                if (additiveBit) {
                    svDrawListAppendStatePair(slotState, &slotState->pointA, &drawRef, 0);
                } else {
                    svDrawListAppendPart((SvPart *) slotState, &slotState->pointA, &drawRef, 0);
                }
            } else if (additiveBit) {
                if (svDrawMode == 12) {
                    svDrawListAppendRecordAPair(slotState, &slotState->pointA, &drawRef, 0);
                } else {
                    svDrawListAppendRecordBPair(slotState, &slotState->pointA, &drawRef, 0);
                }
            } else if (svDrawMode == 11) {
                svDrawListAppendRecordA(slotState, &slotState->pointA, &drawRef, 0);
            } else {
                svDrawListAppendSlot((SefSlotRecord *) slotState, &slotState->pointA, &drawRef, 0);
            }
            pendingCount++;
            svDrawListTryFlush();
        }
    } while (--bucketIndex >= 0);
    svDrawListFlush(svDrawList.end_index - svDrawList.begin_index);
}

/* Scratchpad base address: svCollectSortedLines fills the SvEntryPair array here, and the cull code reuses it. */
#define SV_SCRATCHPAD 0x70000000

/* One output pair in the scratchpad: the line index and its sort key, sorted ascending by sortKey. */
typedef struct SvEntryPair {
    short lineIndex;
    short sortKey;
} SvEntryPair;

/* Context block passed by svDrawEntry: the current matrix sits at +0x470. */
typedef struct SvDrawContext {
    unsigned char unmodeled_00[0x470];
    Matrix4 matrix;
} SvDrawContext;

/* Adds each drawn slot of the pairs' lines to the depth sort, with the view-space z of its position under the current matrix as depth; returns the summed line count. */
int svSortLineSlots(SefEffectRecord *record, SvEntryPair *pairs, int pairCount, SvDrawContext *context) {
    unsigned char *slotBase;
    SvDrawItem *lineData;
    SefSlotRecord *slotRecord;
    SefSlotPart *slotPart;
    short *slotIndexPtr;
    int showFlagged;
    int lineTotal;
    int slotIndex;
    int lineIndex;
    int pairsLeft;
    SvEntryPair *pair;
    int slotsLeft;
    int wantedPartKind;
    Vector4 depthVector;

    slotBase = sevGetPtAllocator2(0);
    lineTotal = 0;
    showFlagged = svSceneFlags & 2;
    svSortReset();
    __asm__ __volatile__(
        "lqc2 vf26, 0(%0)\n\t"
        "lqc2 vf27, 16(%0)\n\t"
        "lqc2 vf28, 32(%0)\n\t"
        "lqc2 vf29, 48(%0)"
        : : "r"(context->matrix));
    if (pairCount > 0) {
        pair = pairs;
        for (pairsLeft = pairCount; pairsLeft != 0; pairsLeft--, pair++) {
            lineIndex = pair->lineIndex;
            lineData = (SvDrawItem *) sefGetLineAdr(lineIndex);
            if (lineData->count == 0 || lineData->entries[-2] == 8) {
                continue;
            }
            lineTotal += lineData->count;
            wantedPartKind = 4;
            slotIndexPtr = lineData->pointHandles;
            for (slotsLeft = 255; slotsLeft >= 0; slotsLeft--, slotIndexPtr++) {
                slotIndex = *slotIndexPtr;
                if (slotIndex < 0) {
                    continue;
                }
                slotRecord = (SefSlotRecord *)(slotBase + slotIndex * sizeof(SefSlotRecord));
                slotPart = slotRecord->part;
                if ((slotPart->flags & 0x10) != 0 && showFlagged == 0) {
                    continue;
                }
                if (slotPart->kind != wantedPartKind) {
                    continue;
                }
                if (slotRecord->drawData == 0) {
                    continue;
                }
                __asm__ __volatile__(
                    "lqc2 vf1, 0(%1)\n\t"
                    "vmulax.xyzw ACC, vf26, vf1x\n\t"
                    "vmadday.xyzw ACC, vf27, vf1y\n\t"
                    "vmaddaz.xyzw ACC, vf28, vf1z\n\t"
                    "vmaddw.xyzw vf20, vf29, vf1w\n\t"
                    "vaddz.xyzw vf1, vf0, vf20z\n\t"
                    "qmfc2 $8, vf1\n\t"
                    "mtc1 $8, %0\n\t"
                    "nop"
                    : "=f"(depthVector.x) : "r"(&slotRecord->position) : "$8");
                svSortAdd(slotIndex, lineIndex, depthVector.x);
            }
        }
    }
    return lineTotal;
}

/* Gathers the line index and sort key of every line of the active entries into out, sorts the pairs ascending by sort key and returns their count. */
int svCollectSortedLines(SefEffectRecord *record, SvEntryPair *out) {
    SefEffectEntry *effectEntry;
    SvDrawItem *lineData;
    SvEntryPair *pair;
    SvEntryPair *earlier;
    SvEntryPair *later;
    SvEntryPair swapPair;
    int showFlagged;
    int pairCount;
    int i;
    int j;

    showFlagged = svSceneFlags & 2;
    pairCount = 0;
    for (j = 0; j < 32; j++) {
        effectEntry = &record->entries[j];
        if (effectEntry->flags & 0x80000000) {
            /* Byte-offset addressing of out: the indexed form changed the code. */
            pair = (SvEntryPair *)((pairCount << 2) + (int)out);
            for (i = 0; i < 32; i++) {
                if (effectEntry->lineIndex[i] >= 0) {
                    lineData = (SvDrawItem *) sefGetLineAdr(effectEntry->lineIndex[i]);
                    if (!(lineData->imageFlags & 0x10) || showFlagged != 0) {
                        pairCount++;
                        pair->lineIndex = effectEntry->lineIndex[i];
                        pair->sortKey = effectEntry->sortKey[i];
                        pair++;
                    }
                }
            }
        }
    }
    for (i = 0; i < pairCount - 1; i++) {
        for (j = i + 1; j < pairCount; j++) {
            /* Byte-offset addressing of out: the indexed form changed the code. */
            earlier = (SvEntryPair *)((i << 2) + (int)out);
            later = &out[j];
            if (later->sortKey < earlier->sortKey) {
                swapPair = *earlier;
                *earlier = *later;
                *later = swapPair;
            }
        }
    }
    return pairCount;
}

void svLoadDrawViewMatrix(void) {
    XglCamera *camera;

    camera = xglStudioGetActiveCamera();
    QuadwordCopyMatrixT0(svDrawStatePacket, camera->volume[0].viewMatrix);
}


/* Builds the draw-state packet: a count, a 0x11000000 tag, two sized blocks (0x14 and 0x6C), then 0x10 bytes from svDrawStatePacket. */
void svBuildDrawStatePacket(void) {
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceVif1PkOpenUpkCode(packet, 0x14, 0x6C, 4, 4);
    sceVif1PkAddUpkData128N(packet, svDrawStatePacket, 0x10);
    sceVif1PkCloseUpkCode(packet);
}


extern Matrix4 svEffectScaledMatrix;
void svDrawSendViewSetup(void);
void svDrawSortedImages(SefEffectRecord *record);

/* Draws effect record `index`: publishes it as the current record, loads its
 * request parameters, builds its matrices (the view, and the effect's own
 * rotation/translation/scale when flag 0x1000000 is set), then draws its
 * lines either through the depth sort (2D with flag 0x800000) or one by one. */
void svDrawEntry(int recordIndex)
{
    SefEffectRecord *effectRecord;
    SefLinkedContext *request;
    XglCamera *camera;
    Matrix4 *viewMatrix;
    Matrix4 *effectMatrix;
    SvEntryPair *sortedPairs;
    int pairCount;
    int pairIndex;
    Vector4 *scale;

    effectRecord = sefGetScheduler() + recordIndex;
    if (effectRecord->inUse == 0 || effectRecord->gate == 0) {
        return;
    }
    if (_draw3D != SV_RENDER_3D_MODEL) {
        svDrawSendViewSetup();
    }
    /* Record-slot view of sefCurrentRecord: the typed pointer store reordered the global accesses. */
    ((SefCurrentRecordSlot *) &sefCurrentRecord)->record = effectRecord;
    svRecordSourcePointer = (void *) effectRecord->owner;
    svRecordTargetPointer = effectRecord->object;
    request = sefGetBattleData();
    request->kind = effectRecord->ownerKey;
    request->mode = (signed char) effectRecord->requestMode;
    request->kind1VectorY = effectRecord->fieldA5C;
    /* The owner word is read as a pointer, as sef.c's sefExecScheduler does: an int load reorders the argument loads. */
    sefLoadRequestBuffer((int) *(void **) &effectRecord->owner, (int)effectRecord->ownerHigh, (int)effectRecord->object, effectRecord->values[0], effectRecord->values[1], effectRecord->values[2], effectRecord->requestTail, effectRecord->flags & 0x400);
    svRecordMatrix = 0;
    effectMatrix = &svEffectMatrix;
    __asm__ __volatile__(
        "lqc2 vf1, 0x20(%1)\n"
        "lqc2 vf2, 0x10(%1)\n"
        "lqc2 vf3, 0x0(%1)\n"
        "sqc2 vf0, 0x30(%0)\n"
        "sqc2 vf1, 0x20(%0)\n"
        "sqc2 vf2, 0x10(%0)\n"
        "sqc2 vf3, 0x0(%0)\n"
        :
        : "r"(effectMatrix), "r"(&MMathUnitMatrix)
        : "memory");
    camera = xglStudioGetActiveCamera();
    viewMatrix = &camera->volume[0].viewMatrix;
    svViewMatrix = viewMatrix;
    QuadwordCopyMatrixT0(svDrawStatePacket, viewMatrix);
    if (effectRecord->flags & 0x1000000) {
        sefCalcRotTransSMatrix(&effectRecord->rotation, &effectRecord->position, 0, effectMatrix);
        scale = &effectRecord->scale;
        MMathScaleMatrix(&svEffectScaledMatrix, effectMatrix, scale);
        svRecordMatrix = &svEffectScaledMatrix;
        MMathMulMatrix((Matrix4 *)svDrawStatePacket, svViewMatrix, &svEffectScaledMatrix);
    } else {
        scale = &effectRecord->scale;
    }
    /* QuadwordCopyT0(&svEffectScaleVector, scale), with the scale read as memory operands. */
    __asm__ __volatile__(
        "lq $8, 0(%1)\n"
        "sq $8, 0(%0)\n"
        :
        : "r"(&svEffectScaleVector), "r"(scale), "m"(*scale), "m"(scale->w)
        : "$8", "memory");
    MMathMulMatrix((Matrix4 *)svDrawStatePacket + 1, viewMatrix, (Matrix4 *)sefCameraMatrix);
    sortedPairs = (SvEntryPair *)SV_SCRATCHPAD;
    pairCount = svCollectSortedLines(effectRecord, sortedPairs);
    if (effectRecord->vertexScale.x == 1.0f && effectRecord->vertexScale.y == 1.0f && effectRecord->vertexScale.z == 1.0f) {
        svVertexScale = 0;
    } else {
        svVertexScale = &effectRecord->vertexScale;
    }
    svRecordScale = svVertexScale;
    if (_draw3D == SV_RENDER_2D && (effectRecord->flags & 0x800000)) {
        svSortLineSlots(effectRecord, sortedPairs, pairCount, (SvDrawContext *)camera);
        svDrawSortedImages(effectRecord);
    } else {
        /* The sorted (lineIndex, sortKey) pairs svCollectSortedLines left in the scratchpad. */
        for (pairIndex = 0; pairIndex < pairCount; pairIndex++) {
            svDrawImageItem((SvDrawItem *) sefGetLineAdr(((short *)SV_SCRATCHPAD)[pairIndex * 2]), ((short *)SV_SCRATCHPAD)[pairIndex * 2 + 1]);
        }
    }
}

/* Transforms a point by the clip matrix (vf21-vf24, set by sefInitClipViewVolume); a null input means the origin. */
void sefTransformClipPoint(Vector4 *out, const Vector4 *in) {
    if (in != 0) {
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(in) : "memory");
    } else {
        __asm__ __volatile__("vmove.xyzw vf1, vf0" : : : "memory");
    }
    __asm__ __volatile__("vmulax.xyzw ACC, vf21, vf1x" : : : "memory");
    __asm__ __volatile__("vmadday.xyzw ACC, vf22, vf1y" : : : "memory");
    __asm__ __volatile__("vmaddaz.xyzw ACC, vf23, vf1z" : : : "memory");
    __asm__ __volatile__("vmaddw.xyzw vf1, vf24, vf1w" : : : "memory");
    __asm__ __volatile__("sqc2 vf1, 0(%0)" : : "r"(out) : "memory");
}

extern SvDrawSlot svVisibleSlots[];
extern int svVisibleEntryCount;
extern int sefEffectRecordCount;

/* Culls the effect records into svVisibleSlots (visible ones only, with their depth), then sorts them by depth, largest first. */
void sefCullSchedulerEntries(void) {
    int i;
    int insertPos;
    int visibleCount;
    SefEffectRecord *record;
    Vector4 *clipPoint;
    XglCamera *camera;
    SvDrawSlot *visibleSlots;
    SvDrawSlot *insertSlot;
    float originZ;
    float radius;

    visibleCount = 0;
    clipPoint = (Vector4 *) SV_SCRATCHPAD;
    record = sefGetScheduler();
    camera = xglStudioGetActiveCamera();
    visibleSlots = svVisibleSlots;
    memset(visibleSlots, 0, 0x800);
    __asm__ __volatile__(
        "lqc2 vf21, 0(%0)\n\t"
        "lqc2 vf22, 16(%0)\n\t"
        "lqc2 vf23, 32(%0)\n\t"
        "lqc2 vf24, 48(%0)"
        : : "r"(&camera->volume[0].viewMatrix));
    sefInitClipViewVolume(camera->volume[0].planeSet[0][0]);
    sefTransformClipPoint(clipPoint, 0);
    originZ = clipPoint->z;
    for (i = 0; i < sefEffectRecordCount; i++, record++) {
        if (record->inUse == 0 || record->ownerKey == 0 || record->gate == 0) {
            continue;
        }
        visibleSlots[visibleCount].recordIndex = i;
        visibleSlots[visibleCount].disabled = 0;
        if (record->flags & 0x01000000) {
            radius = record->fieldA60;
            sefTransformClipPoint((Vector4 *) SV_SCRATCHPAD, &record->position);
            if (1.0f < record->scale.x) {
                radius *= record->scale.x;
            }
            if (!(record->flags & 0x10) && sefClipViewVolumeA(&record->position, radius) != 0) {
                continue;
            }
            visibleSlots[visibleCount].depth = ((Vector4 *) SV_SCRATCHPAD)->z;
            visibleCount++;
        } else {
            visibleSlots[visibleCount].depth = originZ;
            visibleCount++;
        }
    }
    svVisibleEntryCount = visibleCount;
    if (_sefBattleMode == 0) {
        sefCullResourceSlots();
        visibleCount = svVisibleEntryCount;
    }
    i = 1;
    if (i < visibleCount) {
        do {
            insertSlot = (SvDrawSlot *) 0x70000010;
            *insertSlot = visibleSlots[i];
            insertPos = i;
            while (insertPos > 0 && visibleSlots[insertPos - 1].depth < insertSlot->depth) {
                visibleSlots[insertPos] = visibleSlots[insertPos - 1];
                insertPos--;
            }
            visibleSlots[insertPos] = *insertSlot;
            i++;
        } while (i < visibleCount);
    }
}

void svDrawEntriesByKind(int kind, int flags) {
    SefEffectRecord *records;
    SefEffectRecord *record;
    int visibleIndex;
    int recordIndex;

    records = sefGetScheduler();
    for (visibleIndex = 0; visibleIndex < svVisibleEntryCount; visibleIndex++) {
        recordIndex = svVisibleSlots[visibleIndex].recordIndex;
        if (svVisibleSlots[visibleIndex].disabled != 0) {
            continue;
        }
        record = &records[recordIndex];
        if (record->entryId != kind) {
            continue;
        }
        if (_draw3D != SV_RENDER_3D_MODEL) {
            svSetZTestEnv(!((record->flags >> 2) & 1));
        }
        svDrawEntry(recordIndex);
    }
}

/* Makes mapper `index` the active one without touching the cache; indices of 0x100 and above are ignored. */
void svForceImageMapper(unsigned int index) {
    if (index < 0x100U) {
        svLoadImageList(&_imageMapper.typeLists[index]);
        svRefMapperPacket();
    }
}

/* Draws the alter layer only in 2D mode (_draw3D == 0). */
void svDrawAlters(int layer)
{
    if (_draw3D == SV_RENDER_2D) {
        sdvDrawAlters(layer);
    }
}

/* One 20-byte entry of the table sefEntryCountsEntryGet indexes: the first two words are the buffer pair, +8 is the count of draw entries of that kind. */
typedef struct DrawKindEntry {
    DrawBufferPair buffers;
    int count;
    unsigned char unmodeled_0c[8];
} DrawKindEntry;


/* Draws the scene. Mapper 0 is loaded first (also when effects are active), with its kind-0 entries pending when the
 * draw buffer of the current mode holds any. In 2D with flag 1 of svSceneFlags every visible effect record is then drawn
 * in depth order, switching mappers as the records need them; otherwise each mapper 1..56 that has images is loaded
 * and its entries and alters are drawn. */
void svDrawScene(void)
{
    DrawKindEntry *entries;
    DrawKindEntry *entry;
    SefEffectRecord *records;
    SefEffectRecord *record;
    SvTypeList *mapper;
    int drawMode;
    int hasStaticEntries;
    int hasDrawEntries;
    int activeMapper;
    int visibleIndex;
    int mapperIndex;
    int recordIndex;
    int recordKind;

    hasStaticEntries = 0;
    activeMapper = -1;
    drawMode = _draw3D;
    entries = sefEntryCountsEntryGet(0);
    svDrawSendStaticRef();
    svDrawSendViewSetup();
    svSetZTestEnv(1);
    if ((drawMode == SV_RENDER_2D && entries[0].buffers.first != 0) || (drawMode == SV_RENDER_2D_SPRITE && entries[0].buffers.second != 0)) {
        hasStaticEntries = 1;
    }
    mapper = &_imageMapper.typeLists[0];
    if (mapper->pendingCount > 0 || sefGetActiveCount() > 0) {
        svActiveMapperIndex = 0;
        svRequestedMapperIndex = 0;
        svLoadImageList(mapper);
        svRefMapperPacket();
    }
    /* The 32-bit read of svSceneFlags: a typed unsigned short read changed the code. */
    if ((*(u32 *) &svSceneFlags & 1) && drawMode == SV_RENDER_2D) {
        records = sefGetScheduler();
        for (visibleIndex = 0; visibleIndex < svVisibleEntryCount; visibleIndex++) {
            recordIndex = svVisibleSlots[visibleIndex].recordIndex;
            if (svVisibleSlots[visibleIndex].disabled == 0) {
                record = &records[recordIndex];
                recordKind = record->entryId;
                if (recordKind != 0 && recordKind != activeMapper) {
                    if (_imageMapper.typeLists[recordKind].pendingCount > 0) {
                        activeMapper = recordKind;
                        svActiveMapperIndex = activeMapper;
                        svRequestedMapperIndex = activeMapper;
                        svLoadImageList(&_imageMapper.typeLists[recordKind]);
                        svRefMapperPacket();
                    }
                }
                if (_draw3D != SV_RENDER_3D_MODEL) {
                    svSetZTestEnv(!((record->flags >> 2) & 1));
                }
                if (record->gate != 0) {
                    svDrawEntry(recordIndex);
                    sdvDrawOwnerAlters(recordIndex);
                }
            } else {
                sefDrawResources(recordIndex);
            }
        }
        return;
    }
    if (hasStaticEntries) {
        svActiveMapperIndex = 0;
        svRequestedMapperIndex = 0;
        svDrawEntriesByKind(0, 0);
    }
    svDrawAlters(0);
    for (mapperIndex = 1; mapperIndex < 57; mapperIndex++) {
        entry = &entries[mapperIndex];
        hasDrawEntries = 0;
        if ((drawMode == SV_RENDER_2D && entry->buffers.first != 0) || (drawMode == SV_RENDER_2D_SPRITE && entry->buffers.second != 0)) {
            hasDrawEntries = 1;
        }
        if (_imageMapper.typeLists[mapperIndex].pendingCount > 0) {
            svActiveMapperIndex = mapperIndex;
            svRequestedMapperIndex = mapperIndex;
            svLoadImageList(&_imageMapper.typeLists[mapperIndex]);
            svRefMapperPacket();
            if (hasDrawEntries) {
                svDrawEntriesByKind(mapperIndex, 0);
            }
            svDrawAlters(mapperIndex);
        }
    }
}

/* Sets the 3D flag and draws the scene when svDrawListBegin(0) reports it. */
void svDrawScene3D(void) {
    if (svDrawListBegin(0) != 0) {
        _draw3D = 1;
        svDrawScene();
    }
}

/* Clears the 3D flag and draws the scene when svDrawListBegin(1) reports it. */
void svDrawScene2D(void) {
    _draw3D = SV_RENDER_2D;
    if (svDrawListBegin(1) != 0) {
        svDrawScene();
    }
}

extern const Matrix4 svEffectProjectionMatrix;

void svDrawEffectRecords3D(void) {
    DrawKindEntry *kindEntries;
    Matrix inverse;
    XglCamera *camera;
    int i;

    kindEntries = sefEntryCountsEntryGet(0);
    _draw3D = SV_RENDER_3D_MODEL;
    camera = xglStudioGetActiveCamera();
    QuadwordCopyMatrixT0(svEffectCameraMatrices[0], camera->travel.transform);
    xglMatrixInverse(&inverse, (Matrix *)&camera->volume[0].viewMatrix);
    MMathMulMatrix(&svEffectCameraMatrices[1], &inverse.rows, &svEffectProjectionMatrix);
    memset(svEffectLightSet, 0, 384);
    QuadwordCopyT0(&svEffectLightSet[96], vu0IdentityMatrix);
    sefCullSchedulerEntries();
    for (i = 0; i < 57; i++) {
        if (kindEntries[i].count > 0) {
            svDrawEntriesByKind(i, 0);
        }
    }
}

/* Read-only scaffold items after the C-owned literals: the stencil table svStencilTable and the matrix svEffectProjectionMatrix. */
const u64 svStencilTable[3] = { 0x44ULL, 0x48ULL, 0x42ULL };
const Matrix4 svEffectProjectionMatrix = {
    { 0.264445007f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.343434989f, 0.0f, 0.0f },
    { 0.0f, 0.0f, -1.00000203f, -1.0f },
    { 0.0f, 0.0f, -0.0199999996f, 0.0f },
};

/* .data of this TU: definitions in address order (data recovery pass 2). */
const float svSortOffsetScale = -0.1f;
float svDefaultScreenParamsTail[4] = {
    1.0f,
    -1.1428571939468384f,
    0.0625f,
    1.0f
};

u32 svDrawModeTagTable[] = { 0 };

u32 svDrawRecordDivisors[31] = {
    0x00000000u, 0x000006D0u, 0x0000000Eu, 0x000009D8u, 0x0000001Cu, 0x000001D8u, 0x0000000Eu, 0x00000430u,
    0x0000001Cu, 0x00000D28u, 0x00000011u, 0x00000D58u, 0x00000022u, 0x00000D38u, 0x00000011u, 0x00000D68u,
    0x00000022u, 0x00000D48u, 0x00000011u, 0x00000D78u, 0x00000022u, 0x00001688u, 0x0000000Eu, 0x000018D8u,
    0x0000001Cu, 0x00000D88u, 0x00000003u, 0x00000DD8u, 0x00000003u, 0x00000000u, 0x0000000Eu,
};

unsigned int svViewSetupPacket[36] = {
    0x00000000u, 0x00000000u, 0x01000404u, 0x6C080000u, 0x00008007u, 0x10000000u, 0x0000000Eu, 0x00000000u,
    0x0005000Du, 0x00000000u, 0x00000047u, 0x00000000u, 0x00000060u, 0x00000000u, 0x00000014u, 0x00000000u,
    0x00000005u, 0x00000000u, 0x00000008u, 0x00000000u, 0x31000000u, 0x00000001u, 0x0000004Eu, 0x00000000u,
    0x00000000u, 0x00000000u, 0x0000004Au, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000049u, 0x00000000u,
    0x00000001u, 0x00000000u, 0x00000046u, 0x00000000u,
};

int srsKeptEffectKeys[8] = {
    268435507, 268435509, 268435511, 268435489, 268435519, 268435520, 0, 0,
};

unsigned char svMapperPacketEnv[48] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x51, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

SvDrawTagArea svZTestPacketEnv = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x51, 0x02, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    0x5000DULL,
    { 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x01, 0x00, 0x00, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

Vector4 svColorMaxVector = { 255.0f, 255.0f, 255.0f, 255.0f };

Vector4 svDefaultScaleVectors[3] = {
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f }
};

SvImageMapperStorage _imageMapper = { 0 };

/* sefGlobalTransform (0x58 bytes): its position is at 0x46E100, its scale at 0x46E110 and its scaled flag at 0x46E114. */
SefPartTransform sefGlobalTransform = { 0 };

SvDrawSlot svVisibleSlots[260] = { 0 };



/* svDrawStatePacket: the pointer the draw-state code reads, initialized to the scratch block svDrawStatePacketBuffer (data recovery pass 4). */
DrawStatePacket *svDrawStatePacket = (DrawStatePacket *) svDrawStatePacketBuffer;

/* _imageList (0x46E940..0x471D90): header, SV_ENTRY_COUNT items, a 0x28-byte gap and the 1024-entry float table at 0x470D84, one object. */
static SvImageListStorage _imageList = { { 0 }, { { { 0 } } }, { 0 }, {
0.000976999989f, 0.00195299997f, 0.00292999996f, 0.00390599994f, 0.00488300016f, 0.00585900014f, 0.0068359999f, 0.00781199988f,
    0.0087890001f, 0.00976500008f, 0.0107420003f, 0.0117180003f, 0.0126949996f, 0.0136709996f, 0.0146469995f, 0.0156239998f,
    0.0165999997f, 0.0175759997f, 0.0185529999f, 0.0195289999f, 0.0205049999f, 0.0214809999f, 0.0224569999f, 0.0234329998f,
    0.0244089998f, 0.0253849998f, 0.0263609998f, 0.0273369998f, 0.0283129998f, 0.0292879995f, 0.0302639995f, 0.0312399995f,
    0.0322149992f, 0.0331909992f, 0.0341660008f, 0.0351420008f, 0.0361169986f, 0.0370920002f, 0.0380680002f, 0.0390430018f,
    0.0400179997f, 0.0409930013f, 0.0419679992f, 0.0429419987f, 0.0439170003f, 0.0448919982f, 0.0458660014f, 0.0468409993f,
    0.0478149988f, 0.0487889983f, 0.0497639999f, 0.0507379994f, 0.0517119989f, 0.0526859984f, 0.0536589995f, 0.054632999f,
    0.0556069985f, 0.0565799996f, 0.0575539991f, 0.0585270002f, 0.0595000014f, 0.0604729988f, 0.0614459999f, 0.062419001f,
    0.0633919984f, 0.0643640012f, 0.0653370023f, 0.0663089976f, 0.0672810003f, 0.0682530031f, 0.0692249984f, 0.0701970011f,
    0.0711689964f, 0.0721400008f, 0.0731120035f, 0.0740830004f, 0.0750539973f, 0.0760250017f, 0.0769959986f, 0.077967003f,
    0.0789370015f, 0.0799079984f, 0.080877997f, 0.081848003f, 0.0828180015f, 0.083788f, 0.0847570002f, 0.0857269987f,
    0.0866959989f, 0.0876649991f, 0.0886339992f, 0.0896029994f, 0.0905719995f, 0.0915400013f, 0.0925090015f, 0.0934770033f,
    0.0944449976f, 0.0954129994f, 0.0963800028f, 0.0973479971f, 0.0983150005f, 0.0992819965f, 0.100249f, 0.101214997f,
    0.102182001f, 0.103147998f, 0.104114003f, 0.105080001f, 0.106045999f, 0.107012004f, 0.107977003f, 0.108942002f,
    0.109907001f, 0.110872f, 0.111836001f, 0.112800002f, 0.113764003f, 0.114727996f, 0.115691997f, 0.116655f,
    0.117619f, 0.118582003f, 0.119543999f, 0.120507002f, 0.121468998f, 0.122431003f, 0.123392999f, 0.124355003f,
    0.125315994f, 0.126277998f, 0.127239004f, 0.128198996f, 0.129160002f, 0.130119994f, 0.131080002f, 0.132039994f,
    0.132999003f, 0.133958995f, 0.134918004f, 0.135876f, 0.136834994f, 0.137793005f, 0.138751f, 0.139708996f,
    0.140665993f, 0.141624004f, 0.142581001f, 0.143537f, 0.144493997f, 0.145449996f, 0.146405995f, 0.147360995f,
    0.148316994f, 0.149271995f, 0.150226995f, 0.151180997f, 0.152135998f, 0.15309f, 0.154043004f, 0.154997006f,
    0.155949995f, 0.156902999f, 0.157855004f, 0.158807993f, 0.159759998f, 0.160711005f, 0.161662996f, 0.162614003f,
    0.163564995f, 0.164515004f, 0.165464997f, 0.166415006f, 0.167365f, 0.168313995f, 0.169263005f, 0.170212001f,
    0.171159998f, 0.172107995f, 0.173056006f, 0.174004003f, 0.174951002f, 0.175898001f, 0.176844001f, 0.177790001f,
    0.178736001f, 0.179682001f, 0.180627003f, 0.181572005f, 0.182515994f, 0.183459997f, 0.184404001f, 0.185348004f,
    0.186290994f, 0.187233999f, 0.188177004f, 0.189118996f, 0.190061003f, 0.191001996f, 0.191943005f, 0.192883998f,
    0.193825006f, 0.194765002f, 0.195704997f, 0.196643993f, 0.197583005f, 0.198522002f, 0.199460998f, 0.200398996f,
    0.201335996f, 0.202273995f, 0.203210995f, 0.204146996f, 0.205082998f, 0.206018999f, 0.206955001f, 0.207890004f,
    0.208825007f, 0.209758997f, 0.210693002f, 0.211627007f, 0.212559998f, 0.213493004f, 0.214425996f, 0.215358004f,
    0.216288999f, 0.217221007f, 0.218152002f, 0.219082996f, 0.220012993f, 0.220943004f, 0.221872002f, 0.222801f,
    0.223729998f, 0.224657997f, 0.225585997f, 0.226513997f, 0.227440998f, 0.228367001f, 0.229294002f, 0.230220005f,
    0.231144994f, 0.232069999f, 0.232995003f, 0.233918995f, 0.234843001f, 0.235767007f, 0.23669f, 0.237611994f,
    0.238535002f, 0.239455998f, 0.240378007f, 0.241299003f, 0.242219001f, 0.243139997f, 0.244058996f, 0.244978994f,
    0.245897993f, 0.246815994f, 0.247733995f, 0.248651996f, 0.249568999f, 0.250485986f, 0.251401991f, 0.252317995f,
    0.253232986f, 0.25414899f, 0.255062997f, 0.255977005f, 0.256891012f, 0.257804006f, 0.258717f, 0.259629995f,
    0.260542005f, 0.261453003f, 0.262364f, 0.263274997f, 0.264185011f, 0.265094995f, 0.266003996f, 0.266912997f,
    0.267821014f, 0.268729001f, 0.269636989f, 0.270543993f, 0.271450013f, 0.272356987f, 0.273261994f, 0.274167001f,
    0.275072008f, 0.275976002f, 0.276879996f, 0.27778399f, 0.278687f, 0.279588997f, 0.280490994f, 0.281392008f,
    0.282292992f, 0.283194005f, 0.284094006f, 0.284994006f, 0.285892993f, 0.286790997f, 0.287690014f, 0.288587004f,
    0.289485008f, 0.290381014f, 0.291278005f, 0.292172998f, 0.293069005f, 0.293962985f, 0.294858009f, 0.295751989f,
    0.296644986f, 0.297538012f, 0.298429996f, 0.299322009f, 0.300213993f, 0.301104993f, 0.301995009f, 0.302884996f,
    0.303773999f, 0.304663002f, 0.305552006f, 0.306439996f, 0.307327002f, 0.308214009f, 0.309100002f, 0.309985995f,
    0.310871989f, 0.311756998f, 0.312640995f, 0.313524991f, 0.314408988f, 0.315290987f, 0.316174001f, 0.317056f,
    0.317936987f, 0.318818003f, 0.319698006f, 0.320578009f, 0.321458012f, 0.322335988f, 0.323215008f, 0.324092001f,
    0.324970007f, 0.325845987f, 0.326723009f, 0.327598006f, 0.328473985f, 0.329347998f, 0.330222011f, 0.331095994f,
    0.331968993f, 0.332841992f, 0.333714008f, 0.334585011f, 0.335456014f, 0.336326987f, 0.337197006f, 0.338066012f,
    0.338934988f, 0.33980301f, 0.340671003f, 0.341538012f, 0.342404991f, 0.343270987f, 0.344137013f, 0.345001996f,
    0.345867008f, 0.346731007f, 0.347593993f, 0.348457009f, 0.349319994f, 0.350181997f, 0.351042986f, 0.351904005f,
    0.35276401f, 0.353623986f, 0.354483008f, 0.355342001f, 0.35620001f, 0.357057005f, 0.357914001f, 0.358770996f,
    0.359627008f, 0.360482007f, 0.361337006f, 0.362190992f, 0.363045007f, 0.363898009f, 0.364749998f, 0.365601987f,
    0.366454005f, 0.367305011f, 0.368155003f, 0.369004995f, 0.369854003f, 0.370703012f, 0.371551007f, 0.372397989f,
    0.373245001f, 0.374092013f, 0.374938011f, 0.375782996f, 0.376628011f, 0.377472013f, 0.378315985f, 0.379159003f,
    0.380001009f, 0.380843014f, 0.381684005f, 0.382524997f, 0.383365005f, 0.384205014f, 0.385044008f, 0.385883003f,
    0.386720985f, 0.387558013f, 0.388395011f, 0.389230996f, 0.390067011f, 0.390902013f, 0.391736001f, 0.392569989f,
    0.393402994f, 0.394235998f, 0.39506799f, 0.395900011f, 0.396730989f, 0.397561997f, 0.398391008f, 0.399221003f,
    0.400050014f, 0.400878012f, 0.401704997f, 0.402532011f, 0.403358996f, 0.404184014f, 0.405010015f, 0.405833989f,
    0.406657994f, 0.407481998f, 0.408304989f, 0.409126997f, 0.409949005f, 0.410769999f, 0.41159001f, 0.412409991f,
    0.413230002f, 0.414049f, 0.414867014f, 0.415684015f, 0.416500986f, 0.417317986f, 0.418134004f, 0.418949008f,
    0.419764012f, 0.420578003f, 0.42139101f, 0.422203988f, 0.423016012f, 0.423828006f, 0.424638987f, 0.425449997f,
    0.426259995f, 0.427069008f, 0.427877992f, 0.428685993f, 0.42949301f, 0.430299997f, 0.431107014f, 0.431912005f,
    0.432716995f, 0.433521986f, 0.434325993f, 0.435128987f, 0.43593201f, 0.436733991f, 0.437536001f, 0.438336998f,
    0.439137012f, 0.439936996f, 0.440735996f, 0.441534013f, 0.442332f, 0.443129987f, 0.443926007f, 0.444721997f,
    0.445517987f, 0.446312994f, 0.447106987f, 0.447901011f, 0.448693991f, 0.449485987f, 0.450278014f, 0.451070011f,
    0.451860011f, 0.452650011f, 0.453440011f, 0.454228997f, 0.455017f, 0.455805004f, 0.456591994f, 0.457378f,
    0.458164006f, 0.458949f, 0.459733993f, 0.460518003f, 0.461300999f, 0.462083995f, 0.462866008f, 0.463647991f,
    0.464428991f, 0.465209007f, 0.465988994f, 0.466767997f, 0.467545986f, 0.468324006f, 0.469101012f, 0.469877988f,
    0.470654011f, 0.471430004f, 0.472204f, 0.472979009f, 0.473751992f, 0.474525005f, 0.475297987f, 0.476069003f,
    0.476839989f, 0.477611005f, 0.478381008f, 0.479149997f, 0.479918987f, 0.480686992f, 0.481454015f, 0.482221007f,
    0.482988f, 0.483752996f, 0.484517992f, 0.485282987f, 0.486045986f, 0.486808985f, 0.487572014f, 0.488334f,
    0.489095002f, 0.489856005f, 0.490615994f, 0.491374999f, 0.492134005f, 0.49289301f, 0.493649989f, 0.494406998f,
    0.495164007f, 0.495918989f, 0.496674001f, 0.497429013f, 0.498183012f, 0.498935997f, 0.499689013f, 0.500441015f,
    0.501191974f, 0.501942992f, 0.502692997f, 0.503443003f, 0.504191995f, 0.504939973f, 0.505688012f, 0.506434977f,
    0.507180989f, 0.507927001f, 0.508673012f, 0.509416997f, 0.510160983f, 0.510905027f, 0.511646986f, 0.512389004f,
    0.513131022f, 0.513872027f, 0.514612019f, 0.515352011f, 0.516090989f, 0.516829014f, 0.517566979f, 0.51830399f,
    0.519041002f, 0.519777f, 0.520511985f, 0.521247029f, 0.521981001f, 0.522714972f, 0.523446977f, 0.524179995f,
    0.524910986f, 0.525641978f, 0.526373029f, 0.527101994f, 0.527831972f, 0.528559983f, 0.529287994f, 0.530014992f,
    0.53074199f, 0.531467974f, 0.532193005f, 0.532917976f, 0.533641994f, 0.534366012f, 0.535089016f, 0.535811007f,
    0.536532998f, 0.537253976f, 0.537975013f, 0.538694024f, 0.539413989f, 0.540131986f, 0.540849984f, 0.541567981f,
    0.542284012f, 0.543000996f, 0.543716013f, 0.544430971f, 0.545144975f, 0.545858979f, 0.54657203f, 0.547284007f,
    0.547995985f, 0.548707008f, 0.549417973f, 0.550127983f, 0.55083698f, 0.551545978f, 0.552254021f, 0.552962005f,
    0.553668022f, 0.554374993f, 0.555079997f, 0.555785f, 0.556490004f, 0.557193995f, 0.557896972f, 0.558598995f,
    0.559301019f, 0.560002983f, 0.56070298f, 0.561402977f, 0.562102973f, 0.562802017f, 0.563499987f, 0.564198017f,
    0.564894974f, 0.565590978f, 0.566286981f, 0.566981971f, 0.567676008f, 0.568369985f, 0.569064021f, 0.569755971f,
    0.570448995f, 0.571139991f, 0.571830988f, 0.572520971f, 0.573211014f, 0.573899984f, 0.574588001f, 0.575276017f,
    0.57596302f, 0.576650023f, 0.577336013f, 0.57802099f, 0.578706026f, 0.579389989f, 0.580072999f, 0.580756009f,
    0.581439018f, 0.582120001f, 0.582800984f, 0.583482027f, 0.584161997f, 0.584841013f, 0.585520029f, 0.586197972f,
    0.586875021f, 0.587552011f, 0.588227987f, 0.58890301f, 0.589577973f, 0.590252995f, 0.590927005f, 0.591600001f,
    0.592271984f, 0.592944026f, 0.593614995f, 0.594286025f, 0.594955981f, 0.595625997f, 0.596294999f, 0.596962988f,
    0.597630978f, 0.598298013f, 0.598963976f, 0.599629998f, 0.600295007f, 0.600960016f, 0.601624012f, 0.602286994f,
    0.602949977f, 0.603612006f, 0.604273975f, 0.60493499f, 0.605594993f, 0.606254995f, 0.606913984f, 0.607572973f,
    0.608231008f, 0.60888797f, 0.609544992f, 0.610201001f, 0.61085701f, 0.611512005f, 0.612165987f, 0.612820029f,
    0.613472998f, 0.614126027f, 0.614777982f, 0.615428984f, 0.616079986f, 0.616729975f, 0.617380023f, 0.618028998f,
    0.61867702f, 0.619324982f, 0.619971991f, 0.620618999f, 0.621264994f, 0.621909976f, 0.622555017f, 0.623198986f,
    0.623843014f, 0.624486029f, 0.625127971f, 0.625769973f, 0.626411021f, 0.627052009f, 0.627691984f, 0.628332019f,
    0.628970027f, 0.629608989f, 0.630245984f, 0.630882978f, 0.631519973f, 0.632156014f, 0.632790983f, 0.633426011f,
    0.634060025f, 0.63469398f, 0.635326982f, 0.635959029f, 0.636591017f, 0.637221992f, 0.637852013f, 0.638481975f,
    0.639111996f, 0.639741004f, 0.640368998f, 0.640995979f, 0.64162302f, 0.642250001f, 0.642876029f, 0.643500984f,
    0.644125998f, 0.644749999f, 0.645372987f, 0.645995975f, 0.646619022f, 0.647240996f, 0.647862017f, 0.648482025f,
    0.649101973f, 0.64972198f, 0.650340974f, 0.650959015f, 0.651576996f, 0.652194023f, 0.652809978f, 0.653425992f,
    0.654042006f, 0.654657006f, 0.655270994f, 0.655884027f, 0.656497002f, 0.657109976f, 0.657721996f, 0.658333004f,
    0.658944011f, 0.659554005f, 0.660163999f, 0.660772979f, 0.661381006f, 0.661988974f, 0.662595987f, 0.663203001f,
    0.663809001f, 0.664415002f, 0.665019989f, 0.665624022f, 0.666227996f, 0.666831017f, 0.667433977f, 0.668035984f,
    0.668637991f, 0.669238985f, 0.669839025f, 0.670439005f, 0.671037972f, 0.671636999f, 0.672235012f, 0.672833025f,
    0.673430026f, 0.674026012f, 0.674621999f, 0.675216973f, 0.675812006f, 0.676406026f, 0.676999986f, 0.677592993f,
    0.678184986f, 0.678776979f, 0.679368019f, 0.679958999f, 0.680549026f, 0.681138992f, 0.681728005f, 0.682317019f,
    0.682904005f, 0.683492005f, 0.684078991f, 0.684665024f, 0.685250998f, 0.685836017f, 0.686420023f, 0.687004983f,
    0.687587976f, 0.688171029f, 0.688753009f, 0.689334989f, 0.689916015f, 0.690496981f, 0.691076994f, 0.691657007f,
    0.692236006f, 0.692813993f, 0.693391979f, 0.693969011f, 0.694545984f, 0.695122004f, 0.695698023f, 0.696273029f,
    0.696847975f, 0.697422028f, 0.697995007f, 0.698567986f, 0.699141026f, 0.699711978f, 0.700284004f, 0.700854003f,
    0.701425016f, 0.701994002f, 0.702562988f, 0.703131974f, 0.703700006f, 0.704267025f, 0.704833984f, 0.70539999f,
    0.705965996f, 0.706532001f, 0.707095981f, 0.707660019f, 0.708223999f, 0.708787024f, 0.70934999f, 0.709912002f,
    0.710473001f, 0.711034f, 0.711593986f, 0.712153971f, 0.712713003f, 0.713271976f, 0.713829994f, 0.714388013f,
    0.714945018f, 0.715502024f, 0.716058016f, 0.716612995f, 0.717167974f, 0.717723012f, 0.718276978f, 0.718829989f,
    0.719383001f, 0.719935f, 0.720486999f, 0.721037984f, 0.721589029f, 0.722139001f, 0.722688973f, 0.723237991f,
    0.723785996f, 0.724334002f, 0.724882007f, 0.725428998f, 0.725974977f, 0.726521015f, 0.72706598f, 0.727611005f,
    0.728155971f, 0.728699028f, 0.72924298f, 0.729785025f, 0.730328023f, 0.730868995f, 0.73141098f, 0.731950998f,
    0.732491016f, 0.733030975f, 0.73356998f, 0.734108984f, 0.734646022f, 0.735184014f, 0.735720992f, 0.736257017f,
    0.736792982f, 0.737329006f, 0.737864017f, 0.738398015f, 0.738932014f, 0.739464998f, 0.739997983f, 0.740530014f,
    0.741061985f, 0.741593003f, 0.742124021f, 0.742654026f, 0.74318397f, 0.743713021f, 0.744242013f, 0.74476999f,
    0.745298028f, 0.745824993f, 0.746352017f, 0.746878028f, 0.747403026f, 0.747928023f, 0.748453021f, 0.748977005f,
    0.74950099f, 0.750024021f, 0.750545979f, 0.751067996f, 0.751590014f, 0.752111018f, 0.752631009f, 0.753151f,
    0.75367099f, 0.754190028f, 0.754707992f, 0.755226016f, 0.75574398f, 0.756260991f, 0.756776989f, 0.757292986f,
    0.757808983f, 0.758324027f, 0.758837998f, 0.759352028f, 0.759864986f, 0.760378003f, 0.76089102f, 0.761403024f,
    0.761914015f, 0.762425005f, 0.762935996f, 0.763445973f, 0.763954997f, 0.764464021f, 0.764972985f, 0.765479982f,
    0.765987992f, 0.766494989f, 0.767000973f, 0.767507017f, 0.768013f, 0.768517971f, 0.769021988f, 0.769526005f,
    0.770030022f, 0.770533025f, 0.771036029f, 0.771538019f, 0.772038996f, 0.772539973f, 0.77304101f, 0.773540974f,
    0.774040997f, 0.774540007f, 0.775038004f, 0.775537014f, 0.776033998f, 0.776530981f, 0.777028024f, 0.777523994f,
    0.778020024f, 0.778514981f, 0.779009998f, 0.779504001f, 0.779998004f, 0.780490994f, 0.780983984f, 0.781476974f,
    0.781967998f, 0.782459974f, 0.782950997f, 0.783441007f, 0.783931017f, 0.784421027f, 0.784910023f, 0.785398006f
}, { 0 } };
