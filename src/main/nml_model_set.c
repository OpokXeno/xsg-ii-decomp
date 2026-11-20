#include "common.h"
#include "gs.h"
#include "shared.h"
#include "main/nml_model_set.h"
#include "main/pxy_frame_alpha.h"
#include "main/pxy_packet_state.h"
#include "main/TiWater.h"
#include "main/pxy_shadow.h"
#include "main/xgl_2.h"
#include "main/xgl_camera.h"
#include "main/xgl_light.h"
#include "main/xgl_studio.h"
#include "main/xgl_packet.h"
#include "main/xgl_render.h"
#include "main/nml_packet_add.h"
#include "main/pxy_profile.h"
#include "main/face_point.h"
#include "main/cur_matrix_asm.h"
#include "main/model_calc_clip.h"

void *memcpy(void *destination, const void *source, unsigned int size);
void *memset(void *destination, int value, unsigned int size);

/* Quadword helpers of the end of this file. */
void xglVectorCopy_001CC998(void *destination, const void *source);
void xglMatrixCopy_001CC9C4(Matrix4 destination, Matrix4 source);
void xglVectorAddXYZ_001CCA30(void *dst, void *first, void *second);
void xglVectorSubXYZ_001CCA68(void *dst, void *minuend, void *subtrahend);

typedef unsigned int XglQuadword __attribute__((mode(TI)));

typedef union GsTag {
    u64 value;
    u32 words[2];
} GsTag;

#define NML_MAP_SHADOW_PARTS_MAX 8

/* pxySetClipPlaneWithAngle fills it (at most two entries, five quadwords allocated). It is the PxyClipPlanes of a packet
 * with float-array rows: indexing a row as v[i] folds the field offset into the address the way the original does, a
 * vector member does not (pxyClipPlanes, the table under construction, is the only user). */
typedef struct PxyPlaneTable {
    Vector4Elements plane[2]; /* +0x00: plane normal and distance */
    Vector4Elements angle[2]; /* +0x20: rotX, rotY, then the next rotX, rotY */
} PxyPlaneTable;

/* A 0x20-byte range of parts: [0] opaque, [1] translucent. */
typedef struct PxyPartRange {
    u8 unk00[0x14];
    int first; /* +0x14 */
    int count; /* +0x18 */
    u8 unk1C[4];
} PxyPartRange;

/* Address of a table inside the pxy file image: header + byte offset. */
#define PXY_HEADER_AT(header, offset) ((char *)(header) + (offset))

#define PXY_SLOT_COUNT 4 /* ring of recently drawn packets */

typedef struct PxySlot {
    PxyPacket *packet;
    int batch;
    int translucentBatch;
    int unk0C;
} PxySlot;

/* One best-value record of a flags-0x10 pxy header (pxyState +0x560, stride 16).
   pxyEvaluateTracks stores the index and count words through the word view, which
   the original addresses as index * 16 + word offset from pxyState. */
enum { PXY_BEST_WORD_INDEX = 1, PXY_BEST_WORD_COUNT = 2 };
typedef union PxyBestRecord {
    struct {
        float best;  /* +0x00 */
        int index;   /* +0x04 */
        int count;   /* +0x08 */
        int unk0C;   /* +0x0c: not read here */
    };
    int words[4];
    u64 doublewords[2]; /* the records are doubleword aligned */
} PxyBestRecord;

typedef struct PxyState {
    XglLightSet light;
    u8 unk180[0x470 - 0x180];
    u32 cameraVisible[4]; /* +0x470: per camera, nonzero when the model is visible */
    u32 cameraFog[4]; /* +0x480: per camera fog factor << 4 */
    u8 unk490[0x4E0 - 0x490];
    float trackValues[8][4]; /* +0x4e0: result stored at index k + 1 */
    PxyBestRecord bestRecords[13]; /* +0x560: written by pxyEvaluateTracks, read by pxyCalcFogFactors */
    void (*drawCallbacks[16])(int); /* +0x630: registered by pxyRegisterDrawCallback, called with the camera index */
    u8 unk670[0x6A0 - 0x670];
    struct PxyBatch *overlayBatches[32]; /* +0x6a0: batches of header flags 0x81, pxyOverlayBatchCount of them */
    struct PxyBatch *shadowBatches[64]; /* +0x720: batches of the shadow casters */
    struct PxyBatch *unk820[0xA0]; /* +0x820 */
    u8 unkAA0[0x49C0 - 0xAA0];
    PxySlot slots[PXY_SLOT_COUNT]; /* +0x49c0 */
    u8 unk4A00[0x10880 - 0x4A00];
    int headerList[256]; /* +0x10880: headers of the flags-0x10 search, count at +0x5c0; bound from the object extent up to pxyDrawEntryTable at 0x97A980 */
} PxyState;

typedef struct PxyBatch {
    s16 start; /* +0x00: first entry in pxyDrawEntryTable */
    s16 count;
    float depth;
    u32 flags;
    PxyPacket *packet; /* +0x0c */
    s16 unk10;
    s16 next; /* +0x12: index of the next batch, < 0 at the end */
    struct PxyBatch *translucent; /* +0x14 */
    u8 unk18[8];
} PxyBatch;

typedef struct PxyBatchTable {
    PxyBatch batches[256];
    int unk2000;
    int unk2004;
    int count; /* +0x2008 */
    int unk200C;
} PxyBatchTable;

typedef struct PxyDrawEntry {
    PxyPart *part;
    PxyPacket *packet;
} PxyDrawEntry;

/* One 16-byte record of the pxy entry table pxyEntryQueue. */
typedef struct PxyEntry {
    void *data; /* +0x00: the packet data, copied to the attribute buffer when the key is even */
    unsigned short value; /* +0x04: low half of the second argument */
    unsigned char group; /* +0x06: bit index into pxyEntryGroupMask */
    unsigned char flag; /* +0x07 */
    int state; /* +0x08 */
    unsigned char row; /* +0x0C: key / 12 */
    unsigned char unmodeled_0d[3];
} PxyEntry;

/* Draw state kept in the EE scratchpad while the batches are drawn. */
typedef struct PxyDrawWork {
    PxyDrawEntry *entry; /* +0x00 */
    int i; /* +0x04 */
    u32 batchFlags; /* +0x08 */
    int count; /* +0x0c */
    PxyPacket *packet; /* +0x10 */
    PxyPacket *lastPacket; /* +0x14 */
    int mode; /* +0x18 */
    PxyPart *part; /* +0x1c */
    u32 partFlags; /* +0x20 */
    u32 packetFlags; /* +0x24 */
    int unk28; /* +0x28 */
    u8 cameraBit; /* +0x2c */
    u8 unk2D[3];
    u32 frameMask; /* +0x30 */
    u32 frameBase; /* +0x34 */
    PxyBatch *batch; /* +0x38 */
} PxyDrawWork;

#define pxyDrawWork ((PxyDrawWork *)0x70000000)
#define PXY_BATCH_MAX 0x100
#define PXY_OPAQUE_ENTRY_END 0x1000      /* end of the opaque draw-entry range */
#define PXY_TRANSLUCENT_ENTRY_END 0x1A00 /* end of the translucent draw-entry range */
#define PXY_SHADOW_QUADWORDS 0x36        /* size of a PxyShadow in quadwords */
/* Bits of the per-part flag bytes (PxyPacket.partFlags). */
#define PXY_PART_VISIBLE 0x80  /* set for every part when the model is entered */
#define PXY_PART_MERGED 0x40   /* translucent part already attached to the previous entry */
#define PXY_PART_LOD_MASK 0xF  /* level-of-detail bits tested for header flag 1 */

typedef struct PxySortEntry {
    u8 unk00[0xC];
    s16 matrix; /* +0x0c */
    s16 key; /* +0x0e */
} PxySortEntry;

typedef struct PxySortItem {
    Vector4 position;
    s16 start; /* +0x10 */
    s16 count;
    s16 key;
    s16 matrix;
    float depth; /* +0x18 */
    u8 unk1C[4];
} PxySortItem;

typedef struct PxySortGroup {
    int count;
    s16 start; /* +0x04 */
    s16 entryCount; /* +0x06 */
    u8 unk08[8];
    PxySortItem items[32]; /* +0x10 */
    PxySortItem *sorted[32]; /* +0x410 */
} PxySortGroup;

typedef struct PxySortWork {
    PxySortGroup groups[8];
    int groupCount; /* +0x2480 */
    int entryCount;
    PxyDrawEntry *entries; /* +0x2488 */
    PxySortItem *item; /* +0x248c */
    PxySortGroup *group; /* +0x2490 */
    int i; /* +0x2494 */
    PxySortEntry *sortEntries; /* +0x2498 */
    PxySortEntry *sortEntry; /* +0x249c */
    int j; /* +0x24a0 */
    PxyDrawEntry *out; /* +0x24a4 */
    PxySortItem **cursor; /* +0x24a8 */
    PxySortItem *swapped; /* +0x24ac */
    Matrix *matrixSet; /* +0x24b0 */
    Matrix *matrices; /* +0x24b4 */
    u8 unk24B8[8];
    float matrix[4][4]; /* +0x24c0 */
    Vector4 eye; /* +0x2500 */
    PxyDrawEntry buffer[0x35E]; /* +0x2510 */
} PxySortWork;

typedef struct PxyHideMask { u32 bits[1]; } PxyHideMask;

/* Slot of a render layout store: one 32-bit word read as float or int. */
typedef union {
    float f;
    int i;
} LayoutSlot;

/*
 * Request bits of the layout store's render-status word (LayoutStore +0x250).
 * Each nmlModelSet* setter ORs its own bit; the render entries set the
 * microcode-family bit around nmlPacketAddTransMicrocode.
 */
enum NmlRenderStatus {
    NML_RENDER_FOG = 0x1,             /* fog colour/parameters set (nmlModelSetFogCol, nmlModelSetFogDist, global fog) */
    NML_RENDER_FORCE_VISIBLE = 0x2,   /* shadow models and pxyRequestForcedVisibility: camera visibility forced on */
    NML_RENDER_STENCIL = 0x4,         /* nmlModelSetStencil; also set while a fade factor is below 1 */
    NML_RENDER_ZWRITE = 0x8,          /* nmlModelSetZwrite */
    NML_RENDER_HIDDEN = 0x10,         /* not visible in any camera: the model is skipped */
    NML_RENDER_LIGHT_VERTICAL = 0x20, /* light direction has no x/z component */
    NML_RENDER_HUMAN_MODEL = 0x40,
    NML_RENDER_STENCIL_ALPHA = 0x80,  /* nmlModelSetStencilAlpha */
    NML_RENDER_ANIM_TIME = 0x100,     /* pxySetAnimTime: animation time overridden */
    NML_RENDER_FOG_WHITE = 0x400,     /* pxySetFogWhite: fog forced to white, full strength */
    NML_RENDER_PROREAL = 0x800,       /* nmlModelSetTexProreal / RenderProreal */
    NML_RENDER_POINT_LIGHT = 0x1000,  /* nmlModelSetPointLight */
    NML_RENDER_CLIP_PLANE_ANGLE = 0x2000,  /* pxySetClipPlaneWithAngle */
    NML_RENDER_CLIP_PLANE_PAIR = 0x4000,   /* pxyAddClipPlanePair */
    NML_RENDER_CLIP_PLANE_MASKED = 0x8000, /* pxySetClipPlaneMasked */
    NML_RENDER_MAP_ENTRY = 0x10000,   /* nmlModelSetMapEntry / SetShadowMapEntry */
    NML_RENDER_CULLED = 0x20000,      /* nmlModelCalcSample: clipped out of every active window */
    NML_RENDER_TEXMAP_DROP = 0x40000, /* nmlModelSetTexMap, drop kind */
    NML_RENDER_DROP_PASS = 0x80000,   /* set while nmlModelRenderDrop runs */
    NML_RENDER_FACE_MODEL = 0x100000, /* nmlModelSetFaceModel */
    NML_RENDER_TEXMAP_CIRCLE = 0x200000,
    NML_RENDER_TEXMAP_DROP_CIRCLE = 0x400000,
    NML_RENDER_BACK_CLEAR = 0x800000, /* group becomes NML_GROUP_BACK_CLEAR */
    NML_RENDER_GLOBAL_FOG = 0x1000000, /* nmlModelSetGlobalFogCol / Dist */
    NML_RENDER_GLOBAL_POINT_LIGHT = 0x2000000
};

/*
 * A loaded model resource.  Its block count is +0x44 and the table at +0xb0
 * holds byte offsets from this record to individual NmlModelBlock records.
 */
typedef struct NmlModel {
    unsigned char unmodeled_000[0x3f];
    signed char unclassified_3f;
    int partCount; /* +0x40 */
    int blockCount;
    int sampleCount; /* +0x48 */
    unsigned char unmodeled_04c[4];
    int weightedMatrixOffset; /* +0x50: byte offset from the model to its weighted matrices */
    unsigned char unmodeled_054[4];
    int weightOffset;    /* +0x58: byte offset from the model to the sample weights */
    int transformOffset; /* +0x5c: byte offset from the model to the sample transforms */
    u32 geometryTableOffset; /* +0x60: byte offset to the geometry block offset table */
    unsigned char unmodeled_064[8];
    u32 textureSetOffset; /* +0x6c: byte offset to the model's texture set; 0 = none */
    Vector4 boxMinOffset; /* +0x70: bounding box corners relative to the placement */
    Vector4 boxMaxOffset; /* +0x80 */
    Vector4 entryTranslation; /* +0x90; w at +0x9c */
    int unclassified_a0;
    unsigned char unmodeled_0a4[0x0c];
    int blockOffsets[1];
} NmlModel;

/* The same 16 bytes as two doublewords, for the plain ld/sd copies. */
typedef struct NmlQword {
    unsigned long long lo;
    unsigned long long hi;
} NmlQword;

/* A vector on a doubleword boundary: the original copies it with ld/sd. */
typedef union NmlVector4Overlay {
    Vector4 v;
    NmlQword q;
} NmlVector4Overlay;

typedef struct NmlPartPixelAlpha {
    short alpha;
    short partId;
} NmlPartPixelAlpha;

/* One 0x40-byte weighted record: a transform matrix, or the part flags word
 * at +0x1c (matrix[1][3]).  The packet builders send it as four vectors. */
typedef struct NmlPartFlagData {
    unsigned char unmodeled_00[0x1c];
    unsigned int flags; /* +0x1c */
    unsigned char unmodeled_20[0x40 - 0x20];
} NmlPartFlagData;

typedef union NmlWeightedMatrix {
    Matrix4 matrix;
    Vector4 rows[4];
    NmlPartFlagData part;
} NmlWeightedMatrix;

/* +0x1c0 of a layout: the fog colour parcel; the fourth word is the integer
 * fog intensity nmlModelEntry and nmlPacketAddFog store. */
typedef struct NmlFogColor {
    float color[3];
    int intensity;
} NmlFogColor;

typedef union NmlPartsReferenceScratch {
    u64 halves[2];
    Vector4 vector;
    float components[4];
} NmlPartsReferenceScratch;

/*
 * The "proreal" parameter block: the 0x1e0 bytes nmlModelSetProreal builds in
 * s_inProReal (0x95A890 + 0x10B0) and nmlModelEntry copies into the packet with
 * nmlPacketSetAttributeData16N(s_inProReal, 0x1e), storing the packet address
 * it returns in the layout's proreal (ysegfDrawItemNameFrame +0xE8 to +0xF0).
 */
typedef struct NmlProRealParam {
    unsigned char unmodeled_000[0x40];
    Matrix projection_transform;
    Matrix4 normal_transform;
    u32 block_material_data[16];
    unsigned char unmodeled_100[0x1c];
    /*
     * +0x11c: nmlModelSetProreal clears it (yg2PathDrawMainMenu +0x1F4) and the two
     * projection entries set it to 1 immediately after calling that function,
     * nmlModelCalcEntryProjectCircle (ysegfAllocFileBuffer +0x3C) and
     * nmlModelCalcEntryProjectMap (ysegfDrawWindow +0xC0), so it marks a block built
     * for a projected entry.  add_exec_prog reads it to take the projected
     * slot of the "Pro" microcode family instead of the plain one.
     */
    u32 projection_entry;
    unsigned char unmodeled_120[0x10];
    GsTag texture_gs_parameters[2];
    unsigned char unmodeled_140[0x20];
    /* +0x160: the last normal row; the map projection's volume test reads
     * its x and z as the clipping plane normal. */
    union {
        XglQuadword final_normal_row;
        struct {
            float plane_normal_x;
            unsigned char unmodeled_164[4];
            float plane_normal_z;
        };
    };
    unsigned char unmodeled_170[0x20];
    NmlPartsReferenceScratch reference_point; /* +0x190 */
    Vector4 bounds_min; /* +0x1a0 */
    Vector4 bounds_max; /* +0x1b0 */
    void *texture; /* +0x1c0 */
    unsigned char unmodeled_1c4[4];
    u64 alpha; /* +0x1c8 */
} NmlProRealParam;

struct NmlStealthGeometry;

/*
 * The model layout the engine renders from: the single nmlLayoutStore (0x320 bytes) that the nmlModelSet* API fills, plus the
 * per-entry copies nmlModelEntry stores in s_apModelLayout.  The packet
 * builders and the filter renderers read the same record.
 */
union LayoutStore {
    LayoutSlot slots[200];
    unsigned long long qwords[100];
    struct {
        /* +0x0; row 3 w (+0x3c) carries the block material alpha the drop
         * shadow path computes. */
        Matrix4 lightDirection;
        Matrix4 lightColor; /* +0x40; rows 0-2 are the normal light vectors; row 3 (+0x70) holds the scaffold labels D_00969E20 and
                         * s_inLayout (+0x78), which no code references */
        Matrix4 place; /* +0x80 */
        Matrix4 inversePlace; /* +0xc0 */
        Vector4 pointPosition[4]; /* +0x100 */
        Vector4 pointColor[4]; /* +0x140 */
        Matrix4 projectionMatrix; /* +0x180; saved by the circle and map projection paths */
        NmlFogColor fogColor; /* +0x1c0 */
        Vector4 fogPara; /* +0x1d0 */
        float filterAlpha; /* +0x1e0 */
        float filterAmplitude; /* +0x1e4 */
        unsigned char unmodeled_1e8[8];
        /* +0x1f0: the model centre (w = radius): copied as a projection input,
         * passed to AlphaGroupSortEntry, the circle shadow centre and the
         * stealth filter's fallback sphere. */
        NmlVector4Overlay center;
        Vector4 multiplyColor; /* +0x200 */
        Vector4 currentPosition;            /* +0x210; copied from the weighted position by nmlModelCalcSample */
        float transparency; /* +0x220; nmlModelSetTransparency (also +0x2c0) */
        float reflTransparency; /* +0x224; nmlModelSetReflTransparency */
        float defaultBlockAlpha; /* +0x228 */
        float alphaLimit; /* +0x22c */
        union {
            u32 alphaOptions;
            float alphaOptionsValue;
        }; /* +0x230 */
        unsigned int packetFilterFlags;
        int pixelAlpha;                     /* +0x238, full-width pixel-alpha word */
        void *currentMatrix;                /* +0x23c; the model matrix nmlModelSetMatrix stores */
        const void *texture;                /* +0x240; the texture set nmlModelSetTexture stores */
        int texfunc;                        /* +0x244 */
        u64 alpha;                          /* +0x248 */
        /*
         * +0x250: the render-status word.  nmlModelSetRenderStatus is the
         * setter that names it, and it ORs the value it is given (yg2PathDrawMainMenu +0x2C8 sets 0x100, yg2PathDrawMainMenu +0x2E8 sets 0x200); the rest of the
         * nmlModelSet* API ORs one request bit each into the same word -
         * nmlModelSetPlace 0x1 (yg2PathDrawNewMarker +0x8C), nmlModelSetFogCol 0x2
         * (yg2PathDrawMessagePage +0x88), nmlModelSetStencil 0x4 (yg2PathDrawPathGrid +0x160), nmlModelSetZwrite
         * 0x8 (yg2PathDrawPathGrid +0x180), nmlModelSetLight 0x10 (yg2PathDrawPathGrid +0x288),
         * nmlModelSetToumei 0x20 (yg2PathDrawPathGrid +0x228), nmlModelSetHumanModel 0x40
         * (renderSpriteClutAlpha +0x50), nmlModelSetTexProreal 0x800 (yg2PathDrawMainMenu +0x274),
         * nmlModelSetPointLight 0x1000 (yg2PathDrawFramedImage +0x44), nmlModelSetFogCancel
         * 0x2000 (yg2PathDrawMessagePage +0x3A4), nmlModelSetTexMap 0x4000 (yg2PathDrawCellFrame +0xA8) and
         * nmlModelSetFaceModel 0x100000 (renderSpriteClutAlpha +0x2C).  The three render
         * entries set the bit that picks their microcode family just before
         * calling nmlPacketAddTransMicrocode: nmlModelRenderProreal 0x800
         * (battleResultDrawEntries +0x1C4), nmlModelRenderTexture 0x8000 (battleResultDrawScreen +0x70) and
         * nmlModelRenderDrop 0x80000 (battleResultBuildEntries +0x130).
         */
        u32 render_status;
        NmlModel *model; /* +0x254; also how a child entry finds its parent layout */
        NmlWeightedMatrix *weightedMatrices; /* +0x258 */
        unsigned char unmodeled_25c[4];
        /*
         * +0x260: this layout's own proreal block, the packet address
         * nmlModelEntry stores after copying s_inProReal (ysegfDrawItemNameFrame +0xF0).
         */
        NmlProRealParam *proreal;
        /*
         * +0x264: another layout's proreal block, borrowed for the length of
         * one render.  nmlModelRenderProreal stores the entry's own +0x260
         * here (battleResultDrawEntries +0x1DC) and clears it again afterwards (battleResultDrawEntries +0x23C); nmlModelEntry clears it as well (ysegfDrawItemNameFrame +0xF4).
         * add_exec_prog reads it in preference to proreal whenever it is set.
         */
        NmlProRealParam *prorealOverride;
        int texMapMode; /* +0x268; supplied by map and actor shadow setup */
        float textureTransparency;
        int texMapSize; /* +0x270 */
        float texMapWeight; /* +0x274 */
        int mapShadowPartCount; /* +0x278 */
        int window; /* +0x27c; nmlModelSetWindow */
        struct NmlStealthGeometry *stealthParts[10]; /* +0x280 */
        int shadowHeightEnabled; /* +0x2a8 */
        float shadowHeight; /* +0x2ac */
        /*
         * +0x2b0: the render-level word nmlModelSetRenderLevel ORs its whole
         * argument into (citizenFlagSet +0x4).  nmlModelSetSpecularOff sets bit 1
         * (yg2PathDrawFramedImage +0x70), and nmlModelCalcSpecularClip sets and clears that
         * same bit (yg2PathEaseCursor +0xC0, yg2PathRun +0x9C) and sets bit 4 (yg2PathRun +0x7C).
         */
        unsigned int renderLevel;
        int partPixelAlphaCount; /* +0x2b4 */
        unsigned char *clipMasks; /* +0x2b8 */
        float specialBlockAlpha; /* +0x2bc */
        float circleShadowScale; /* +0x2c0 */
        float circleScale; /* +0x2c4 */
        int stealthEntryCount; /* +0x2c8 */
        unsigned char unmodeled_2cc[4];
        NmlPartPixelAlpha partPixelAlpha[4]; /* +0x2d0 */
        unsigned short mapShadowParts[NML_MAP_SHADOW_PARTS_MAX]; /* +0x2e0 */
        float fogIntensity[4]; /* +0x2f0; one fog intensity multiplier per viewport */
        float viewportAlpha[4];
        int viewportFlags[4];
    } fields;
};

typedef union LayoutStore LayoutStore;

/* canon: config/header-canon.json chose src/math/main/spark-cont01-00231d50/nmlModelCalcDropShadow.c over 0 other accepted spellings */
extern LayoutStore nmlLayoutStore;

/* nmlFilterStealthMake allocates 31 sixteen-byte records. Its draw consumer
 * witnesses fourteen XYZ records at +0x30 and fourteen UV records at +0x110.
 * Each record is one vector slot: the projection helper writes its integer
 * screen coordinates into the same storage it would hold a float vector in. */
union NmlStealthXYZ {
    Vector4 vector;
    struct {
        int x;
        int y;
        int z;
        int w;
    };
};

union NmlStealthUV {
    Vector4 vector;
    struct {
        int u;
        int v;
        int clipFlags;
        unsigned char unmodeled_0c[4];
    };
};

/*
 * The allocator's 31 sixteen-byte records, including actual endpoint
 * vectors.  The first endpoint's w is the squared camera distance of the
 * part centre; nmlModelEntry depth sorts a layout's stealth parts by it
 * within a sort class.
 */
struct NmlStealthGeometry {
    Vector4 endpoints[2];
    float sortClass;
    unsigned char unmodeled_24[0x0c];
    union NmlStealthXYZ xyz[14];
    union NmlStealthUV uv[14];
};

/*
 * The 16-byte tail of a batch table (PxyBatchTable.unk2000 .. unk200C). The
 * record is doubleword aligned: with a word-aligned record GCC folds the field
 * offset into the pxyState address, the original keeps it as the store offset.
 */
typedef union PxyBatchTableTail {
    struct {
        int unk2000;
        int unk2004;
        int count;
        int unk200C;
    };
    u64 doublewords[2];
} PxyBatchTableTail;

/*
 * pxyState as 16-byte records: batch table i (pxyState + 0x9A0, 0x2010 bytes each)
 * ends at record i * 513 + 0x29A, the index this file's reset addresses its tail by.
 */
typedef struct PxyStateRecords {
    PxyBatchTableTail records[1];
} PxyStateRecords;

static PxyState pxyState;

#define PXY_BATCH_TABLE_TAIL(i) \
    (((PxyStateRecords *) &pxyState)->records[(i) * 513 + 0x29A])

extern PxyBatchTable pxyBatchTables[2];
extern PxyEntry pxyEntryQueue[400];
static PxyDrawEntry pxyDrawEntryTable[6656];
static unsigned char pxyResetWork[48];
static unsigned char pxyCircleShadowTexture[4128];
static unsigned int s_aCircleShadowClut[8];
static int pxyFadeRamp[1024];
extern int pxyOverlayBatchCount;
extern int pxyShadowBatchCount;
extern int pxySeparateBatchCount;
extern int pxyExtraBatchCount;
extern struct PxyBatch *pxyFirstBatch;
extern struct PxyBatch *pxyLastBatch;

/* Empties both batch tables and the batch lists. */
void pxyResetBatchTables(void)
{
    int i;

    for (i = 0; i < 2; i++) {
        PXY_BATCH_TABLE_TAIL(i).count = 0;
        PXY_BATCH_TABLE_TAIL(i).unk2000 = 0;
        PXY_BATCH_TABLE_TAIL(i).unk2004 = 0;
        memset(&pxyBatchTables[i], 0, sizeof(pxyBatchTables[i].batches));
    }
    pxySeparateBatchCount = 0;
    pxyOverlayBatchCount = 0;
    pxyShadowBatchCount = 0;
    pxyFirstBatch = 0;
    pxyLastBatch = 0;
    pxyExtraBatchCount = 0;
}

void pxyResetBatchTables(void);

void pxyInitBatchTables(void) {
    pxyResetBatchTables();
}

extern int pxySlotWriteIndex;
void pxyResetSlots(void);

void pxyResetSlots(void) {
    int i;

    pxySlotWriteIndex = 0;
    for (i = 0; i < PXY_SLOT_COUNT; i++) {
        pxyState.slots[i].packet = 0;
    }
}

void pxyInitSlots(void) {
    pxyResetSlots();
}

void xglVectorClear(Vector4 *destination);
extern int nmlRenderStatus;
extern int pxyActiveCameraIndex;
extern int pxyShadowMode;
extern PxyHideMask *pxyHideMask;
extern PxyHeader *pxyCurrentHeader;
extern PxyPartRange *pxyPartRanges;
extern void *pxyHeaderTable58;
extern void *pxyHeaderTable6C;
extern PxyPart *pxyParts;
extern void *pxyDefaultPartMatrices;
extern void *pxyDefaultJointMatrices;
extern Matrix *pxyJointMatrices;
extern void *pxyHeaderTable64;
extern u8 *pxyPartVisibilityFlags;
extern TxyBlock *pxyTxyBlock;
extern void *pxyHeaderTable3C;
extern PxyHeader *pxyParentHeader;
extern float pxyReflTransparency;
extern float pxyTransparency;
extern float pxyCircleShadowScale;
extern void *pxyHeaderTable74;
extern void *pxyHeaderTable78;
extern Matrix *pxyPartMatrices;
extern f32 pxyFocusX;
extern f32 pxyFocusY;
extern void *pxyGeometryFogFactors;
extern XglLightSet *pxyCurrentLight;
extern PxyShadow *pxyCurrentShadow;
extern int pxyRenderLevel;
extern float pxyModelScaleMax;
extern int pxyClipEnabled;
extern Matrix *pxyShadowMatrix;
extern int pxyVisibleOverride;
extern Matrix *pxyMotionGlobalMatrices;
extern s16 pxyClipPlaneCount;
extern PxyPlaneTable *pxyClipPlanes;
extern Matrix *pxyGlobalMatrices;
extern u32 *pxyClipPlaneMask;
extern f32 pxyShadowHeightLower;
extern void *pxySecondPartMask;
extern f32 pxyShadowHeightUpper;
extern u8 *pxyColorIndexSource;
extern int pxyStateFlags;
extern float pxyFadeAlpha;
extern Matrix nmlModelDrawMatrix;
extern Matrix nmlModelDrawInverseMatrix;
extern AlignedVector4 pxyModelScale;
extern int pxyColorLimitR;
extern int pxyColorLimitG;
extern int pxyColorLimitB;
extern int pxyColorLimitA;
extern Vector4 pxyModelPosition;
extern int pxyLatchedActorState;
extern int pxyLatchedSecondState;
extern float nmlModelSortOffset;
extern int pxyForcedVisibilityRequested;
extern f32 pxyShadowScale;

void pxyResetGlobals(void) {
    xglMatrixUnit(nmlModelDrawMatrix.rows);
    xglMatrixUnit(nmlModelDrawInverseMatrix.rows);
    nmlRenderStatus = 0;
    pxyStateFlags = 0;
    pxyShadowMode = 0;
    pxyActiveCameraIndex = 0;
    pxyReflTransparency = 1.0f;
    pxyTransparency = 1.0f;
    pxyCircleShadowScale = 1.0f;
    pxyModelScaleMax = 1.0f;
    pxyModelScale.x = 1.0f;
    pxyModelScale.y = 1.0f;
    pxyModelScale.z = 1.0f;
    pxyModelScale.w = 1.0f;
    pxyColorLimitR = 0xFF0;
    pxyColorLimitG = 0xFF0;
    pxyColorLimitB = 0xFF0;
    pxyColorLimitA = 0xFF0;
    pxyFocusX = 0;
    pxyFocusY = 0;
    pxyRenderLevel = 0;
    pxyVisibleOverride = 0;
    pxyClipPlaneCount = 0;
    pxyShadowHeightUpper = 0.5f;
    pxyShadowHeightLower = -1.0f;
    xglVectorClear(&pxyModelPosition);
    pxyFadeAlpha = 1.0f;
    pxyHideMask = 0;
    pxyCurrentHeader = 0;
    pxyPartRanges = 0;
    pxyHeaderTable58 = 0;
    pxyHeaderTable6C = 0;
    pxyParts = 0;
    pxyDefaultPartMatrices = 0;
    pxyDefaultJointMatrices = 0;
    pxyJointMatrices = 0;
    pxyHeaderTable64 = 0;
    pxyPartVisibilityFlags = 0;
    pxyTxyBlock = 0;
    pxyHeaderTable3C = 0;
    pxyParentHeader = 0;
    pxyHeaderTable74 = 0;
    pxyHeaderTable78 = 0;
    pxyPartMatrices = 0;
    pxyGeometryFogFactors = 0;
    pxyCurrentLight = &pxyState.light;
    pxyCurrentShadow = 0;
    pxyClipEnabled = 0;
    pxyShadowMatrix = 0;
    pxyMotionGlobalMatrices = 0;
    pxyClipPlanes = 0;
    pxyGlobalMatrices = 0;
    pxySecondPartMask = 0;
    pxyClipPlaneMask = 0;
    pxyColorIndexSource = 0;
    pxyLatchedActorState = 0;
    pxyLatchedSecondState = 0;
    nmlModelSortOffset = 0;
    pxyForcedVisibilityRequested = 0;
    pxyShadowScale = 1.0f;
    memset(&pxyResetWork, 0, 0x30);
}

void pxyResetGlobals(void);
extern int pxyBatchedModelCount;
extern int pxyTrackedHeaderCount;
extern int pxyShadowCasterMode;
extern int pxyOpaqueEntryCursor;
extern int pxyTranslucentEntryCursor;
extern int pxyMom2B04LightFlag;

void pxyResetCounters(void)
{
    pxyResetGlobals();
    pxyBatchedModelCount = 0;
    pxyTrackedHeaderCount = 0;
    pxyShadowCasterMode = 1;
    pxyOpaqueEntryCursor = 0;
    pxyTranslucentEntryCursor = 0x1000;
    pxyMom2B04LightFlag = 0;
}

void pxyResetCounters(void);

void pxyInitCounters(void) {
    pxyResetCounters();
}

extern int pxyFreezeAnimation;
extern int pxyDrawCallbackCount;
extern int pxyEntryGroupMask;
extern int pxyEntryCount;
extern int pxyFlags;
extern void (*pxyDrawCallback)(int);
extern Vector4 pxyGlobalColor;

void pxyClearEntryTable(void) {
    int i;

    pxyGlobalColor.x = 1.0f;
    pxyGlobalColor.y = 1.0f;
    pxyGlobalColor.z = 1.0f;
    pxyGlobalColor.w = 1.0f;
    pxyEntryCount = 0;
    pxyEntryGroupMask = 0;
    pxyFreezeAnimation = 0;
    for (i = 0; i < 16; i++) {
        pxyState.drawCallbacks[i] = 0;
    }
    pxyDrawCallbackCount = 0;
    pxyFlags = 0;
    pxyDrawCallback = 0;
}

void pxyClearEntryTable(void);
extern int pxyCameraEnable[4];
extern float pxyLightSlotTable;
extern float pxyLightSlot1Unit;
extern float pxyLightSlot2Unit;
extern float pxyLightSlot3Unit;
extern float pxyLightSlot4Unit;
extern float pxyLightSlot5Unit;
extern float pxyLightSlot6Unit;
extern f32 pxyOpaqueEntryUsagePercent;
extern f32 pxyTranslucentEntryUsagePercent;

void pxyInitTables(void)
{
    pxyCameraEnable[0] = 1;
    pxyCameraEnable[1] = 0;
    pxyCameraEnable[2] = 0;
    pxyCameraEnable[3] = 0;
    xglLightSetDefault(&pxyState.light);
    memset(&pxyLightSlotTable, 0, 0x180);
    pxyLightSlotTable = 1.0f;
    pxyLightSlot1Unit = 1.0f;
    pxyLightSlot2Unit = 1.0f;
    pxyLightSlot3Unit = 1.0f;
    pxyLightSlot4Unit = 1.0f;
    pxyLightSlot5Unit = 1.0f;
    pxyLightSlot6Unit = 1.0f;
    pxyOpaqueEntryUsagePercent = 0;
    pxyTranslucentEntryUsagePercent = 0;
    pxyClearEntryTable();
}

extern int pxyFadeRampOrderTable[];

void pxyBuildFadeRamp(float spread) {
    int *ramp;
    int i;
    float faded;
    float step;

    ramp = pxyFadeRamp;
    faded = 0.0f;
    step = spread;
    ramp[pxyFadeRampOrderTable[0]] = 0x80000000;
    for (i = 1; i < 32; i++) {
        if ((int)faded < 129) {
            ramp[pxyFadeRampOrderTable[i]] = (128 - (int)faded) << 24;
        } else {
            ramp[pxyFadeRampOrderTable[i]] = 0;
        }
        faded = faded + step;
        step = step + (spread * 0.2f + 0.0f);
    }
}

float sqrtf(float);
/* The first two words of this unit's .sdata (0x69E0C0), defined in address order so that
 * every access is $gp-relative. */
int pxyProfileDrawFlag = 0;
int pxyShadowTexturesReady = 0;

void CONSTRUCT_CIRCLR_SHADOW(void)
{
    int row;
    float maxLevel;
    int column;

    maxLevel = 31.0f;
    if (pxyShadowTexturesReady != 0) {
        return;
    }
    {
    for (row = 0; row < 64; row++) {
        for (column = 0; column < 64; column++) {
            int cellIndex;
            float x;
            float y;
            float radius;
            register unsigned char *cell;
            register unsigned int level;
            register int offset;
            register unsigned char *base;

            cellIndex = row * 64 + column;
            x = (float)column - 32.0f;
            y = (float)row - 32.0f;
            radius = sqrtf(x * x + y * y);
            if (radius <= 1.0f) {
                radius = 0.0f;
            } else if (32.0f < radius) {
                radius = 1.0f;
            } else {
                radius = (radius - 1.0f) / 31.0f;
            }
            radius = (1.0f - radius) * maxLevel;
            offset = cellIndex + 32;
            base = pxyCircleShadowTexture;
            cell = base + offset;
            level = (unsigned int)radius;
            *cell = level;
        }
    }
    }
    {
        struct PxyRow { unsigned int w[4]; } *packet;

        packet = (struct PxyRow *)s_aCircleShadowClut;
        packet[0].w[0] = 0;
        packet[0].w[1] = 0;
        packet[0].w[2] = 0;
        packet[0].w[3] = 0x50000041;
        packet[1].w[0] = 0x8040;
        packet[1].w[1] = 0x08000000;
        packet[1].w[2] = 0;
        packet[1].w[3] = 0;
    }
    {
        struct PxyRow { unsigned int w[4]; } *packet;

        packet = (struct PxyRow *)pxyCircleShadowTexture;
        packet[0].w[0] = 0;
        packet[0].w[1] = 0;
        packet[0].w[2] = 0;
        packet[0].w[3] = 0x50000101;
        packet[1].w[0] = 0x8100;
        packet[1].w[1] = 0x08000000;
        packet[1].w[2] = 0;
        packet[1].w[3] = 0;
    }
    pxyBuildFadeRamp(0.3f);
    pxyShadowTexturesReady = 1;
}

typedef int s32;

void pxySetStateFlag(void) {
    pxyStateFlags |= 1;
}

void pxySetColorIndexSource(u8 *colorIndex)
{
  pxyColorIndexSource = colorIndex;
}

void pxySetFogWhite(void) {
    nmlLayoutStore.fields.render_status |= NML_RENDER_FOG_WHITE;
}

void pxySetDefaultLight(XglLightSet *light)
{
    register XglLightSet *src = light;
    register PxyState *dst = &pxyState;
    /* (src = src) is a dead same-value set: it adds a reference to src, so the light
     * pointer, not the address, takes the first volatile-free register as in the original */
    memcpy(dst, (src = src), 0x180);
}

/* Address of the pxy state block: the 16-slot entry table at +0x630 and the slot records. */
void *pxyGetState(void) {
    return &pxyState;
}

void pxyResetLightToDefault(void) {
    xglLightSetDefault(&pxyState.light);
}

void pxySetShadowCasterMode(int value)
{
  pxyShadowCasterMode = value;
}


const u64 xglForwardVector[2] = { 0, 0x3f8000003f800000ULL };

void xglDirectionFromAngles(Vector4 *direction, const Vector4 *rotation) {
    Matrix4 matrix;
    DoublewordVector4 forward;
    AlignedVector4 spare;

    forward.words[0] = xglForwardVector[0];
    forward.words[1] = xglForwardVector[1];
    xglMatrixStackUnit();
    xglMatrixStackRotX(rotation->x);
    xglMatrixStackRotY(rotation->y);
    xglMatrixStackSave(matrix);
    xglVectorMulMat(&forward.vector, matrix, &forward.vector);
    xglVectorCopy_001CC998(direction, &forward.vector);
}

int strcmp(const char *, const char *);

int pxyFindEntryIndex(PxyHeader *header, const char *name) {
    char *base;
    int i;

    if (name != 0 && header != 0 && header != 0 && header->magic[0] == 'p' && header->magic[1] == 'x' && header->magic[2] == 'y') {
        base = PXY_HEADER_AT(header, header->geometryOffset);
        for (i = 0; i < header->geometryCount; i++) {
            if (strcmp(base, name) == 0) {
                return i;
            }
            base += 112;
        }
    }
    return -1;
}

void *pxyGetEntry(PxyHeader *header, int index) {
    char *base;

    if (header != 0 && header != 0 && header->magic[0] == 'p' && header->magic[1] == 'x' && header->magic[2] == 'y') {
        base = PXY_HEADER_AT(header, header->geometryOffset);
        if (header->geometryCount > index) {
            return base + index * 112;
        }
    }
    return 0;
}

void pxySetFocus(f32 first, f32 second) {
    pxyFocusX = first;
    pxyFocusY = second;
}

void pxyRequestTextureCopyPass(void) {
    pxyFlags |= 2;
}

void pxyRequestOverlayPass(void) {
    pxyFlags |= 8;
}

void pxySetOverlayAlphaMaskMode(void) {
    pxyFlags |= 0x2000;
}

void pxySkipOverlayDraw(void) {
    pxyFlags |= 0x200;
}

void pxyRequestFrameClearOnly(void) {
    pxyFlags |= 0x4000;
}

void pxySetShadowUploadFlag(void) {
    pxyFlags |= 0x20000;
}

void pxyRequestForcedVisibility(void) {
    nmlLayoutStore.fields.render_status |= NML_RENDER_FORCE_VISIBLE;
    pxyForcedVisibilityRequested = 1;
}

/* Requests packet mode 3 (bit 0x10 of pxyFlags); pxyFlush passes that bit to xglPacketSetMode3Flag. */
void pxySetMode3Flag(int enabled)
{
    if (enabled) {
        pxyFlags |= 0x10;
    }
}

/* Requests packet mode 4 (bit 0x80000 of pxyFlags); pxyFlush passes that bit to xglPacketSetMode4Flag. */
void pxySetMode4Flag(void) {
    pxyFlags |= 0x80000;
}

void pxySetShadowProjectOff(void) {
    pxyFlags |= 0x40000;
}

void pxySetSkipTextureUpload(void) {
    pxyFlags |= 0x40;
}

void pxySetBlackFog(void) {
    pxyFlags |= 0x80;
}

void nmlModelSetTexProreal(void) {
    nmlLayoutStore.fields.render_status |= NML_RENDER_PROREAL;
}

void pxySetSourceDrawPass(void)
{
    nmlRenderStatus |= NML_RENDER_CULLED;
    pxyFlags |= 0x8000;
}

void nmlModelSetClip(int value)
{
  pxyClipEnabled = value;
}

extern float pxyAnimTime;

void pxySetAnimTime(float value)
{
    pxyAnimTime = value;
    nmlRenderStatus |= NML_RENDER_ANIM_TIME;
}

void pxySetShadowFirstPass(void)
{
    nmlRenderStatus |= NML_RENDER_TEXMAP_DROP;
    pxyFlags |= 0x100000;
}

void nmlModelSetShadowMapEntry(void) {
    nmlLayoutStore.fields.render_status |= NML_RENDER_MAP_ENTRY;
}

void pxySetFreezeAnimation(void) {
    pxyFreezeAnimation = 1;
}

void pxyLatchActorState(s32 value) {
    if (pxyLatchedActorState == 0) {
        pxyLatchedActorState = value;
    }
}

void pxyLatchSecondState(s32 value) {
    if (pxyLatchedSecondState == 0) {
        pxyLatchedSecondState = value;
    }
}

void pxyRegisterDrawCallback(void (*entry)(int)) {
    int i;

    for (i = 0; i < 16; i++) {
        if (pxyState.drawCallbacks[i] == 0) {
            pxyState.drawCallbacks[i] = entry;
            pxyDrawCallbackCount++;
            return;
        }
        if (pxyState.drawCallbacks[i] == entry) {
            return;
        }
    }
}

/* Installs the callback that nmlModelFlushSub calls through pxyDrawCallback (TiWater passes its draw function). */
void pxySetDrawCallback(void (*callback)(int))
{
  pxyDrawCallback = callback;
}

void pxySetCameraEnable(int index, int value)
{
    pxyCameraEnable[index] = value;
}

void nmlModelSetRenderLevel(int flags)
{
    pxyRenderLevel |= flags;
    if (pxyRenderLevel & 0x40) {
        pxyFlags |= 0x100;
    }
}

extern f32 pxyShadowAlpha;
extern f32 pxyShadowPlaneHeight;

void pxySetShadowMode(int mode, f32 a, f32 b, f32 c) {
    if (a <= 0.0f) {
        return;
    }
    pxyShadowMode = mode;
    pxyShadowAlpha = a;
    pxyShadowPlaneHeight = b;
    pxyShadowScale = c;
    if (pxyShadowMode == 4 || pxyShadowMode == 1 || pxyShadowMode == 5) {
        nmlRenderStatus |= NML_RENDER_FORCE_VISIBLE;
    }
}

void pxySetShadowHeightRange(f32 first, f32 second) {
    pxyShadowHeightUpper = first;
    pxyShadowHeightLower = second;
}

void pxySetJointMatrices(Matrix *joints, Matrix *global)
{
    pxyJointMatrices = joints;
    pxyGlobalMatrices = global;
}

void pxySetMotionGlobalMatrices(Matrix *global)
{
  pxyMotionGlobalMatrices = global;
}

void pxySetPartMatrices(Matrix *matrices)
{
  pxyPartMatrices = matrices;
}

extern AlignedVector4 pxyFogColor;

void nmlModelSetFogCol(Vector4 *position) {
    pxyFogColor = *position;
    nmlRenderStatus |= NML_RENDER_FOG;
}


extern u16 pxyClipPlaneMaskedCount;
extern float pxyClipPlaneMaskedX;
extern float pxyClipPlaneMaskedY;
extern float pxyClipPlaneMaskedZ;

void nmlModelFogPara(AlignedVector4 *out, float fogNear, float fogFar, float fogMin, float fogMax)
{
    float shadeMin;
    float shadeMax;

    shadeMin = (1.0f - fogMin) * 255.0f;
    shadeMax = (1.0f - fogMax) * 255.0f;
    if (shadeMax < 0.0f) {
        shadeMax = 0.0f;
    }
    if (255.0f < shadeMax) {
        shadeMax = 255.0f;
    }
    if (shadeMin < 0.0f) {
        shadeMin = 0.0f;
    }
    if (255.0f < shadeMin) {
        shadeMin = 255.0f;
    }
    out->x = shadeMax;
    out->y = shadeMin;
    out->z = (((shadeMax - shadeMin) * (fogFar + fogNear)) / (fogFar - fogNear) + (shadeMax + shadeMin)) * 0.5f;
    out->w = (fogFar * fogNear) * (shadeMin - shadeMax) / (fogFar - fogNear);
}

extern AlignedVector4 pxyFogParams;

void nmlModelSetFogDist(float fogNear, float fogFar, float fogMin, float fogMax) {
    nmlModelFogPara(&pxyFogParams, fogNear, fogFar, fogMin, fogMax);
    nmlRenderStatus |= NML_RENDER_FOG;
}


int nmlModelGetFogParaStudio(const Vector4 *position, int studio)
{
    int result = 255;
    AlignedVector4 *fogPara = &pxyFogParams;
    XglCamera *camera;
    AlignedVector4 projected;

    if (nmlRenderStatus & NML_RENDER_FOG) {
        fogPara = &pxyFogParams;
    }
    camera = xglStudioSelectGetActiveCamera(studio);
    if (camera != 0) {
        _CurMatrixSet(&camera->volume[0].viewMatrix);
        _CurRotTransPersFog(&projected, position, fogPara);
        result = (u32)projected.w;
        if (projected.z < 0.0f) {
            result = (u32)fogPara->y;
        }
    }
    return result;
}

extern AlignedVector4 pxyGlobalFogPara;

void nmlModelSetGlobalFogDist(float fogNear, float fogFar, float fogMin, float fogMax) {
    nmlModelFogPara(&pxyGlobalFogPara, fogNear, fogFar, fogMin, fogMax);
    pxyFlags |= 0x20;
}

extern AlignedVector4 pxyGlobalFogColor;

void nmlModelSetGlobalFogCol(Vector4 *value) {
    pxyGlobalFogColor = *value;
    pxyFlags |= 0x20;
}

void nmlModelSetSortOffset(float value)
{
  nmlModelSortOffset = value;
}

void pxySetColorScale(Vector4 *value)
{
    pxyGlobalColor = *value;
}

void pxySetHideMask(void *value)
{
  pxyHideMask = value;
}

void pxySetSecondPartMask(void *value)
{
  pxySecondPartMask = value;
}

void nmlModelSetTexture(TxyBlock *block)
{
    u8 *bytes;
    u32 i;

    if ((u32)block & 0xF) {
        return;
    }
    if (block != 0 && block->magic[0] == 't' && block->magic[1] == 'x' && block->magic[2] == 'y') {
        if (block->flags & 0x80000000) {
            bytes = (u8 *)block;
            i = 16;
            while (i < block->size) {
                if (bytes[i] == 1) {
                    bytes[i] = -1;
                } else if (bytes[i] != 0) {
                    bytes[i] = bytes[i] + 0xFF;
                }
                i++;
            }
            block->flags &= 0x7FFFFFFF;
        }
        pxyTxyBlock = block;
    }
}

void pxySetModelPosition(Vector4 *position)
{
    pxyModelPosition = *position;
}

void nmlModelSetParent(PxyHeader *value)
{
  pxyParentHeader = value;
}


void nmlModelSetDrawMatrix(Matrix4 matrix) {
    xglMatrixCopy_001CC9C4(nmlModelDrawMatrix.rows, matrix);
}

void nmlModelSetScale(const Vector4 *src) {
    pxyModelScale = *src;
    pxyModelScaleMax = src->x;
    if (pxyModelScaleMax < src->y) {
        pxyModelScaleMax = src->y;
    }
    if (pxyModelScaleMax < src->z) {
        pxyModelScaleMax = src->z;
    }
}

void nmlModelSetZwrite(int enabled)
{
    if (enabled) {
        nmlLayoutStore.fields.render_status |= NML_RENDER_ZWRITE;
    }
}

void nmlModelSetTransparency(float value) {
    if (value < 0.0f) {
        value = 0.0f;
    }
    if (value > 1.0f) {
        value = 1.0f;
    }
    pxyTransparency = value;
    pxyCircleShadowScale = value;
}

void nmlModelSetReflTransparency(float value) {
    if (value < 0.0f) {
        value = 0.0f;
    }
    if (value > 1.0f) {
        value = 1.0f;
    }
    pxyReflTransparency = value;
}

void nmlModelSetStencil(int enabled) {
    if (enabled) {
        nmlLayoutStore.fields.render_status |= NML_RENDER_STENCIL;
    }
}

void pxySetLightPtr(XglLightSet *light)
{
  pxyCurrentLight = light;
}


void pxySetLightCopy(XglLightSet *light)
{
    pxyCurrentLight = nmlPacketSetAttributeAlloc16N(0x19);
    memcpy(pxyCurrentLight, light, 0x180);
}

void nmlModelSetHumanModel(void) {
    nmlLayoutStore.fields.render_status |= NML_RENDER_HUMAN_MODEL;
}

extern long long nmlStencilAlphaData;
extern long long nmlStencilAlphaRegister;

/* Stencil passes: stores the GS ALPHA_1 (register 0x42) data word gsAlphaData in nmlStencilAlphaData and the register address in nmlStencilAlphaRegister. */
void nmlModelSetStencilAlpha(long long gsAlphaData)
{
    nmlStencilAlphaData = gsAlphaData;
    nmlStencilAlphaRegister = 0x42;
    nmlRenderStatus |= NML_RENDER_STENCIL_ALPHA;
}

void nmlModelSetDropPass(void) {
    nmlLayoutStore.fields.render_status |= NML_RENDER_DROP_PASS;
}

void nmlModelSetTexMapCircle(void) {
    nmlLayoutStore.fields.render_status |= NML_RENDER_TEXMAP_CIRCLE;
}

void nmlModelSetModel(NmlModel *model)
{
  nmlLayoutStore.fields.model = model;
}

void pxyAbortModel(void) {
    pxyResetGlobals();
}

/* Bounding sphere of the model being drawn: centre and radius (w), computed by pxyCalcModelBounds. */
extern DoublewordVector4 pxyBoundSphere;

void *pxyGetBoundSphere(void) {
    return &pxyBoundSphere.vector;
}

extern int pxyModelHiddenFlag;

int pxyIsModelHidden(void) {
    return pxyModelHiddenFlag;
}

extern s32 pxyFlushDoneFlag;

/* Frame start for the pxy module: reset the state, clear pxyFlushDoneFlag (pxyFlush sets it at its end). */
void pxyBeginFrame(void) {
    nmlModelFlushClear();
    pxyFlushDoneFlag = 0;
    pxyClearFlushDone();
}

void pxySetTexMapDropAll(void) {
    pxyFlags |= 0x400;
}

void pxyEnableFrameMaskPass(void) {
    pxyFlags |= 0x800;
}

void pxyForceCameraVisibleSet(void) {
    pxyFlags |= 0x1000;
}

int pxyGlowPendingGet(void) {
    return pxyFlags & 4;
}

int pxyWaterReflectionPendingGet(void) {
    return pxyFlags & 1;
}

void pxySetClipPlaneWithAngle(Vector4Elements *plane, Vector4Elements *angle, PxyAngle *rotation)
{
    if (pxyClipPlanes == 0) {
        pxyClipPlanes = nmlPacketSetAttributeAlloc16N(5);
    }
    if (pxyClipPlaneCount < 2) {
        pxyClipPlanes->plane[pxyClipPlaneCount] = *plane;
        pxyClipPlanes->angle[pxyClipPlaneCount] = *angle;
        pxyClipPlanes->angle[pxyClipPlaneCount].component[2] = rotation->rotX;
        pxyClipPlanes->angle[pxyClipPlaneCount].component[3] = rotation->rotY;
        pxyClipPlaneCount++;
        nmlRenderStatus |= NML_RENDER_CLIP_PLANE_ANGLE;
    }
}


void pxyAddClipPlanePair(PxyRowBits *plane, PxyRowBits *angle)
{
    if (plane->words[0] != 0 || plane->words[1] != 0 || plane->words[2] != 0 ||
        angle->words[0] != 0 || angle->words[1] != 0 || angle->words[2] != 0) {
        if (pxyClipPlanes == 0) {
            pxyClipPlanes = nmlPacketSetAttributeAlloc16N(5);
        }
        if (pxyClipPlaneCount < 2) {
            pxyClipPlanes->plane[pxyClipPlaneCount] = plane->row;
            pxyClipPlanes->angle[pxyClipPlaneCount] = angle->row;
            if (pxyClipPlaneCount == 0) {
                pxyClipPlanes->plane[1] = plane->row;
                pxyClipPlanes->angle[1] = angle->row;
            }
            nmlRenderStatus |= NML_RENDER_CLIP_PLANE_PAIR;
            pxyClipPlaneCount++;
        }
    }
}

void pxySetClipPlaneMasked(PxyRowBits *plane, PxyRowBits *angle, int count, float x, float y, float z, u32 *mask)
{
    if (plane->words[0] != 0 || plane->words[1] != 0 || plane->words[2] != 0 || angle->words[0] != 0 || angle->words[1] != 0 || angle->words[2] != 0) {
        if (pxyClipPlanes == 0) {
            pxyClipPlanes = nmlPacketSetAttributeAlloc16N(5);
        }
        pxyClipPlanes->plane[0] = plane->row;
        pxyClipPlanes->angle[0] = angle->row;
        nmlRenderStatus |= NML_RENDER_CLIP_PLANE_MASKED;
        pxyClipPlaneCount = 1;
        pxyClipPlaneMaskedCount = (u16)count;
        pxyClipPlaneMaskedX = x;
        pxyRenderLevel |= 8;
        pxyClipPlaneMask = mask;
        pxyClipPlaneMaskedY = y;
        pxyClipPlaneMaskedZ = z;
    }
}


float pxyInterpolateKeys(int count, Vector4Elements *keyframes, float time, float weight)
{
    int i;

    if (time <= keyframes[0].component[0]) {
        return keyframes[0].component[1];
    }
    if (time >= keyframes[count - 1].component[0]) {
        return keyframes[count - 1].component[1];
    }
    if (count == 1) {
        return keyframes[0].component[1];
    }
    i = 0;
    while (i < count) {
        if (time < keyframes[i].component[0]) {
            break;
        }
        i++;
    }
    i--;
    if (i < 0) {
        return keyframes[0].component[1];
    }
    if (weight >= 0.2f) {
        float value = keyframes[i].component[1];

        if (*(u32 *)&value & 1) {
            return keyframes[i].component[1];
        }
    }
    return pxyKeyInterpolate(&keyframes[i], &keyframes[i + 1], time, 1.0f);
}

/* Append one A+D pair (value, register) to a direct-queue packet. */
extern float pxyShadowAlpha;
extern float pxyFadeMetric;
extern float pxyShadowOffsetX;
extern float pxyShadowOffsetY;
extern float pxyShadowOffsetZ;
extern u16 sRenderTextureBase;
extern s16 cameraScreenWidth;
extern s16 cameraScreenHeight;
float atan2f(float, float);

/* Aims the shadow projector at the model from the light and builds its matrices and texture registers. */
void pxySetupShadowProjector(PxyHeader *header)
{
    int texWidth = 0x400;
    int texHeight = 0x200;
    PxyShadow *shadow = pxyCurrentShadow;
    float rotX;
    float rotY;
    float horizontal;
    Matrix inverse;
    AlignedVector4 scale = { pxyShadowScale * 128.0f, pxyShadowScale * -128.0f, 8000.0f, 1.0f };
    AlignedVector4 size = { 256.0f, 256.0f, 1.0f, 1.0f };
    AlignedVector4 offset = { 2045.0f, 2045.0f, 0.0f, 1.0f };
    AlignedVector4 direction;
    float zoom = 1.0f;
    float extent = header->sphere.w;

    if (pxyFlags & 0x10) {
        if (pxyShadowOffsetX != 0.0f || pxyShadowOffsetY != 0.0f || pxyShadowOffsetZ != 0.0f) {
        }
        if (pxyFadeMetric < 139000.0f) {
            pxyShadowMode = 0;
            return;
        } else if (pxyFadeMetric < 173000.0f) {
            pxyShadowAlpha *= 1.0f - (173000.0f - pxyFadeMetric) / 34000.0f;
        }
    }
    if (pxyCurrentShadow == 0) {
        pxyCurrentShadow = nmlPacketSetAttributeAlloc16N(PXY_SHADOW_QUADWORDS);
    }
    shadow = pxyCurrentShadow;
    xglVectorCopy_001CC998(&shadow->center, &pxyModelPosition);
    extent = header->sphere.w * 2.0f;
    if (header->sphere.w >= 5.0f) {
        extent = header->sphere.w * 3.0f;
    }
    if (extent < 0.968f) {
        extent = 0.968f;
    }
    zoom = 0.968f / extent;
    scale.x *= zoom;
    scale.y *= zoom;
    if (pxyCurrentLight->shadows[0].w == 0.0f) {
        direction.x = pxyCurrentLight->directions[0][0];
        direction.y = 2.0f;
        direction.z = pxyCurrentLight->directions[0][2];
        direction.w = 1.0f;
        if (pxyFlags & 0x10) {
            direction.y = pxyCurrentLight->directions[0][1];
        }
    } else {
        direction.x = pxyCurrentLight->shadows[0].x;
        direction.y = pxyCurrentLight->shadows[0].y;
        direction.z = pxyCurrentLight->shadows[0].z;
        direction.w = 1.0f;
    }
    xglVectorNormal(&direction, &direction);
    xglVectorCopy_001CC998(&shadow->min, &shadow->center);
    xglVectorCopy_001CC998(&shadow->max, &shadow->center);
    if (pxyFlags & 0x10) {
        float tilt;

        shadow->min.x -= header->sphere.w;
        shadow->min.y -= header->sphere.w;
        shadow->min.z -= header->sphere.w;
        shadow->max.x += header->sphere.w;
        shadow->max.y += header->sphere.w;
        shadow->max.z += header->sphere.w;
        tilt = 1.0f;
        if (direction.y != 0.0f) {
            tilt = __builtin_fabsf((direction.x * direction.x + direction.z * direction.z) / direction.y);
        }
        if (tilt < 1.0f) {
            tilt = 1.0f;
        }
        if (direction.x > 0.0f) {
            shadow->min.x -= direction.x * tilt * header->sphere.w * 1.3f;
            shadow->max.x += direction.x * tilt * header->sphere.w * 0.3f;
        } else {
            shadow->max.x -= direction.x * tilt * header->sphere.w * 1.3f;
            shadow->min.x += direction.x * tilt * header->sphere.w * 0.3f;
        }
        if (direction.z > 0.0f) {
            shadow->min.z -= direction.z * tilt * header->sphere.w * 1.3f;
            shadow->max.z += direction.z * tilt * header->sphere.w * 0.3f;
        } else {
            shadow->max.z -= direction.z * tilt * header->sphere.w * 1.3f;
            shadow->min.z += direction.z * tilt * header->sphere.w * 0.3f;
        }
        if (!(direction.y > 0.0f)) {
            shadow->max.y += header->sphere.w;
            shadow->min.y -= header->sphere.w * 0.5f;
        } else {
            shadow->min.y -= header->sphere.w * 1.5f;
            shadow->max.y -= header->sphere.w * 0.5f;
        }
    } else {
        float spread;
        float side;
        float tilt;
        float lift;

        shadow->min.x -= header->sphere.w;
        shadow->min.y -= header->sphere.w;
        shadow->min.z -= header->sphere.w;
        shadow->max.x += header->sphere.w;
        shadow->max.y += header->sphere.w;
        shadow->max.z += header->sphere.w;
        spread = 1.5f;
        side = 0.3f;
        tilt = 1.0f;
        lift = 0.0f;
        if (header->sphere.w >= 5.0f) {
            lift = header->sphere.w * 1.9f;
        } else if (header->sphere.w >= 2.0f) {
            lift = header->sphere.w;
        }
        if (direction.y != 0.0f) {
            tilt = __builtin_fabsf((direction.x * direction.x + direction.z * direction.z) / direction.y);
        }
        if (tilt < 1.0f) {
            tilt = 1.0f;
        }
        if (direction.x != 0.0f || direction.z != 0.0f) {
            if (direction.x > 0.0f) {
                shadow->min.x -= __builtin_fabsf(direction.x * tilt * header->sphere.w * spread + lift);
                shadow->max.x += __builtin_fabsf(direction.x * tilt * header->sphere.w * side);
            } else {
                shadow->max.x += __builtin_fabsf(direction.x * tilt * header->sphere.w * spread + lift);
                shadow->min.x -= __builtin_fabsf(direction.x * tilt * header->sphere.w * side);
            }
            if (direction.z > 0.0f) {
                shadow->min.z -= __builtin_fabsf(direction.z * tilt * header->sphere.w * spread + lift);
                shadow->max.z += __builtin_fabsf(direction.z * tilt * header->sphere.w * side);
            } else {
                shadow->max.z += __builtin_fabsf(direction.z * tilt * header->sphere.w * spread + lift);
                shadow->min.z -= __builtin_fabsf(direction.z * tilt * header->sphere.w * side);
            }
        }
        if (direction.y > 0.0f) {
            shadow->min.y -= header->sphere.w * 1.5f;
            shadow->max.y -= header->sphere.w * 0.5f;
        } else {
            shadow->max.y += header->sphere.w;
            shadow->min.y -= header->sphere.w * 0.5f;
        }
    }
    shadow->bounds.x = (shadow->max.x + shadow->min.x) * 0.5f;
    shadow->bounds.y = (shadow->max.y + shadow->min.y) * 0.5f;
    shadow->bounds.z = (shadow->max.z + shadow->min.z) * 0.5f;
    shadow->bounds.w = xglPointLength(&shadow->max, &shadow->min) * 0.5f;
    {
        XglCamera *camera = xglStudioSelectGetActiveCamera(0);

        if (camera != 0) {
            Matrix clip;

            xglMatrixUnit(clip.rows);
            _ModelCalcClipInitMat1(camera->volume[0].planeSet[0], clip.rows);
            if (_ModelCalcClipMat1(&shadow->bounds) == 0) {
                nmlRenderStatus &= ~NML_RENDER_FORCE_VISIBLE;
            } else if ((pxyFlags & 0x10) && nmlModelCalcClipCulling(camera, &shadow->bounds)) {
                nmlRenderStatus &= ~NML_RENDER_FORCE_VISIBLE;
            }
            if ((nmlRenderStatus & NML_RENDER_LIGHT_VERTICAL) && (nmlRenderStatus & NML_RENDER_HIDDEN)) {
                nmlRenderStatus &= ~NML_RENDER_FORCE_VISIBLE;
            }
        }
    }
    horizontal = sqrtf(direction.x * direction.x + direction.z * direction.z);
    rotY = -atan2f(direction.x, -direction.z);
    rotX = atan2f(direction.y, horizontal);
    {
        AlignedVector4 eye = direction;

        xglVectorScale(&eye, &eye, 0.0f);
        xglVectorAddXYZ_001CCA30(&eye, &eye, &shadow->center);
        xglMatrixStackUnit();
        xglMatrixStackTrans(&eye);
        xglMatrixStackRotY(rotY);
        xglMatrixStackRotX(rotX);
        xglMatrixStackInverse();
        xglMatrixStackSave(shadow->view.rows);
        xglVectorNormal(&direction, &direction);
        xglVectorScale(&direction, &direction, 3.0f);
        xglVectorAddXYZ_001CCA30(&direction, &direction, &shadow->center);
        xglMatrixStackUnit();
        xglMatrixStackTrans(&direction);
        xglMatrixStackRotY(rotY);
        xglMatrixStackRotX(rotX);
        xglMatrixStackRotZ(1.5707964f);
        xglMatrixStackSave(shadow->light.rows);
        xglMatrixStackInverse();
        xglMatrixStackSave(inverse.rows);
        xglMatrixStackUnit();
        offset.x -= (cameraScreenWidth - size.x) * 0.5f;
        offset.y -= (cameraScreenHeight - size.y) * 0.5f;
        xglMatrixStackTrans(&offset);
        xglMatrixStackScale(&scale);
        xglMatrixStackMul(inverse.rows);
        xglMatrixStackSave(shadow->project.rows);
        memset(&shadow->lighting, 0, sizeof(shadow->lighting));
        if (pxyShadowMode == 4) {
            shadow->alphaData = 0x42;
        } else {
            shadow->alphaData = 0x89;
        }
        shadow->x = 0;
        shadow->y = 0;
        shadow->width = 0x100;
        shadow->height = 0x100;
        shadow->repeatU = 0;
        shadow->repeatV = 0;
        shadow->frameBase = sRenderTextureBase;
        shadow->scaleU = 64.0f / (zoom * 128.0f);
        shadow->scaleV = 64.0f / (zoom * 128.0f);
        shadow->unk330 = 0;
        shadow->clutOffset = 0;
        shadow->psm = GS_PSMCT32;
        {
            AlignedVector4 depth = { 0.0f, 0.0f, 1000.0f, 1.0f };
            Matrix texture;
            int wrapU;
            int wrapV;
            int minU;
            int maxU;
            int minV;
            int maxV;

            scale.x = (float)shadow->width * 0.015625f * shadow->scaleU;
            scale.y = (float)shadow->height * 0.015625f * shadow->scaleV;
            scale.z = scale.w = 1.0f;
            xglMatrixScale(&texture, &shadow->light, &scale);
            xglVectorMulMat(&shadow->unk2D0, texture.rows, &depth);
            xglVectorSubXYZ_001CCA68(&shadow->direction, &shadow->unk2D0, texture.rows[3]);
            xglMatrixInverse(&shadow->textureInverse, &texture);
            xglVectorNormal(&shadow->direction, &shadow->direction);
            switch (shadow->psm) {
            case GS_PSMT4:
                shadow->tex0 = GS_SET_TEX0(shadow->frameBase << 5, 16, GS_PSMT4, 10, 9, 1, 0,
                                           (shadow->frameBase << 5) + shadow->clutOffset, 0, 0, 0, 1);
                break;
            case GS_PSMT8:
                shadow->tex0 = GS_SET_TEX0(shadow->frameBase << 5, 16, GS_PSMT8, 10, 9, 1, 0,
                                           (shadow->frameBase << 5) + shadow->clutOffset, 0, 0, 0, 1);
                break;
            case GS_PSMCT32:
                shadow->tex0 = GS_SET_TEX0(shadow->frameBase << 5, 8, GS_PSMCT32, 9, 9, 1, 0, 0, 0, 0, 0, 1);
                texWidth = 0x200;
                texHeight = 0x200;
                break;
            }
            wrapU = shadow->repeatU ? GS_REGION_REPEAT : GS_REGION_CLAMP;
            wrapV = shadow->repeatV ? GS_REGION_REPEAT : GS_REGION_CLAMP;
            minU = shadow->repeatU ? shadow->width - 1 : shadow->x;
            maxU = shadow->repeatU ? shadow->x : shadow->x + shadow->width - 1;
            minV = shadow->repeatV ? shadow->height - 1 : shadow->y;
            maxV = shadow->repeatV ? shadow->y : shadow->y + shadow->height - 1;
            shadow->clamp = GS_SET_CLAMP(wrapU, wrapV, minU, maxU, minV, maxV);
            shadow->uv.y = (float)shadow->width / (float)texWidth;
            shadow->uv.x = (float)shadow->x / (float)texWidth + shadow->uv.y * 0.5f;
            shadow->uv.w = -(float)shadow->height / (float)texHeight;
            shadow->uv.z = (float)shadow->y / (float)texHeight - shadow->uv.w * 0.5f;
            xglVectorScaleXYZ(&shadow->direction, &shadow->direction, -1.0f);
            shadow->lighting.direction.x = shadow->direction.x;
            shadow->lighting.direction.y = shadow->direction.y;
            shadow->lighting.direction.z = shadow->direction.z;
            shadow->lighting.color[0] = shadow->lighting.color[1] = shadow->lighting.color[2] = 1.0f;
        }
    }
}

/*
 * A keyed track part of a pxy file: key count, 24.8 fixed-point play time and
 * the keyframe rows. Part entries of the flags-0x10 tracks with a zero key
 * count are leaves that carry a single value instead.
 */
typedef struct PxyPartView {
    int count;          /* +0x00: key count, zero for a leaf */
    int minValue;       /* +0x04 */
    int maxValue;       /* +0x08 */
    u8 unk0C[4];
    int time;           /* +0x10: 24.8 fixed point, advances 256 per frame */
    float leafValue;    /* +0x14: value of a leaf */
    signed char mode;   /* +0x18: 0 or 1 selects pxyFocusX or pxyFocusY when focus driven */
    signed char focusDriven; /* +0x19: nonzero takes pxyFocusX/Y instead of keyframes */
    u8 unk1A[6];
    Vector4Elements keys;        /* +0x20: first keyframe row */
} PxyPartView;

extern f32 pxyTrackFocusTable[][4];

void pxyTrackStoreFocus(PxyHeader *header, PxyPartView *record, int trackIndex) {
    float *trackSlot;
    if (record->focusDriven != 1) {
        return;
    }
    trackSlot = &pxyTrackFocusTable[(trackIndex + 1) / 4][(trackIndex + 1) % 4];
    if (record->mode == 0) {
        *trackSlot = pxyFocusX;
    } else {
        if (record->mode == 1) {
            *trackSlot = pxyFocusY;
        }
    }
}

typedef struct PxyTrack {
    float weight; /* +0x00 */
    int count;    /* +0x04 */
} PxyTrack;

typedef struct PxyTrackEntry {
    int count; /* +0x00 */
    int loopStart;    /* +0x04 */
    int loopEnd;    /* +0x08 */
    int time;   /* +0x0c */
    int partOffsets[31]; /* +0x10 */
    signed char frozen;  /* +0x8c */
} PxyTrackEntry;


float fmodf(float, float);

void pxyEvaluateTracks(PxyHeader *header)
{
    int i;
    int trackedIndex;

    trackedIndex = -1;
    for (i = 0; i < pxyTrackedHeaderCount; i++) {
        if (pxyState.headerList[i] == (int)header) {
            trackedIndex = i;
            break;
        }
    }
    if (trackedIndex < 0 || (nmlRenderStatus & NML_RENDER_ANIM_TIME) != 0) {
        if ((header->flags & 4) != 0) {
            int k;
            PxyTrack *track;
            int *offsets;
            PxyPartView *trackPart;
            float result;
            float value;

            track = (PxyTrack *)PXY_HEADER_AT(header, header->offset74);
            offsets = (int *)PXY_HEADER_AT(header, header->offset78);
            for (k = 0; k < track->count; k++) {
                trackPart = (PxyPartView *)PXY_HEADER_AT(header, offsets[k]);
                value = pxyAnimTime;
                if (trackPart->focusDriven != 0) {
                    pxyTrackStoreFocus(header, trackPart, k);
                    continue;
                }
                if (pxyFreezeAnimation == 0) {
                    trackPart->time = trackPart->time + 256;
                }
                if ((nmlRenderStatus & NML_RENDER_ANIM_TIME) == 0) {
                    /* time is 24.8 fixed point */
                    value = (float)(trackPart->time / 256)
                        + (float)(trackPart->time % 256) / 256.0f;
                }
                value = fmodf(value, (float)(trackPart->maxValue - trackPart->minValue)) + (float)trackPart->minValue;
                result = pxyInterpolateKeys(trackPart->count, &trackPart->keys, value, track->weight);
                if (trackPart->mode == 2) {
                    float fraction;
                    fraction = result - (float)(int)result;
                    if (fraction >= 0.5f) {
                        result = result + 0.5f;
                    }
                } else if (trackPart->mode == 0 || trackPart->mode == 1) {
                    while (result > 8.0f) {
                        result = result - 8.0f;
                    }
                    {
                        while (result < -8.0f) {
                            result = result + 8.0f;
                        }
                    }
                }
                pxyState.trackValues[(k + 1) / 4][(k + 1) % 4] = result;
            }
        }
        if ((header->flags & 0x10) != 0) {
            float best;
            float value;
            float result; /* unused */
            int k;
            PxyTrack *track;
            int *offsets;
            int i;
            PxyTrackEntry *entry;
            int bestIndex;
            PxyPartView *branch;

            track = (PxyTrack *)PXY_HEADER_AT(header, header->trackOffset);
            offsets = (int *)PXY_HEADER_AT(header, header->entryOffsets);
            for (k = 0; k < track->count; k++) {
                entry = (PxyTrackEntry *)PXY_HEADER_AT(header, offsets[k]);
                bestIndex = 0;
                best = 0.0f;
                if (entry->frozen != 0) {
                    continue;
                }
                if (pxyFreezeAnimation == 0) {
                    entry->time = entry->time + 256;
                }
                if (entry->time > (entry->loopEnd << 8)) {
                    entry->time = entry->time - (entry->loopEnd << 8) + (entry->loopStart << 8);
                }
                if (entry->time < (entry->loopStart << 8)) {
                    entry->time = entry->loopStart << 8;
                }
                {
                    for (i = 0; i < entry->count; i++) {
                    value = 0.0f;
                    branch = (PxyPartView *)PXY_HEADER_AT(header, entry->partOffsets[i]);
                    if (branch->count == 0) {
                        value = branch->leafValue;
                    } else {
                        value = pxyInterpolateKeys(branch->count, &branch->keys,
                                              (float)(entry->time / 256) + (float)(entry->time % 256) / 256.0f,
                                              track->weight);
                    }
                    if (best < value) {
                        best = value;
                        bestIndex = i;
                    }
                }
                }
                pxyState.bestRecords[k].best = best;
                pxyState.bestRecords[k].words[PXY_BEST_WORD_INDEX] = bestIndex;
                pxyState.bestRecords[k].words[PXY_BEST_WORD_COUNT] = entry->count;
            }
        }
    }
    if (header->waterFlag != 0) {
        tiWaterObjectUpdate(header, pxyFreezeAnimation);
    }
}

extern PxyBatch *pxyFirstBatch;
extern PxyBatch *pxyLastBatch;

void pxyQueueBatch(PxySlot *slot, PxyPacket *packet)
{
    PxyBatchTable *table;
    PxyBatch *batch;
    int cursor;

    if (slot == 0) {
        return;
    }
    table = pxyBatchTables;
    batch = &table->batches[table->count];
    if (packet->header->flags & 0x81) {
        pxyState.overlayBatches[pxyOverlayBatchCount++] = batch;
        slot->packet = packet;
        slot->batch = pxyBatchTables[0].count++;
        slot->translucentBatch = pxyBatchTables[1].count++;
        return;
    }
    if (batch->flags & 0x100000) {
        pxyState.unk820[pxySeparateBatchCount++] = batch;
        slot->packet = packet;
        slot->batch = pxyBatchTables[0].count++;
        slot->translucentBatch = pxyBatchTables[1].count++;
        return;
    }
    if (packet->drawMode) {
        pxyState.shadowBatches[pxyShadowBatchCount++] = batch;
    }
    if (pxyFirstBatch == 0) {
        table->unk2000 = table->count;
        table->unk2004 = table->count;
        pxyFirstBatch = batch;
        pxyLastBatch = batch;
    } else {
        cursor = table->unk2000;
        while (1) {
            if (cursor < 0 || cursor >= 256) {
                break;
            }
            if ((table->batches[cursor].depth < batch->depth && !(table->batches[cursor].packet->drawFlags & 0x10)) || (packet->drawFlags & 0x10)) {
                if (table->batches[cursor].unk10 < 0) {
                    table->batches[cursor].unk10 = table->count;
                    batch->next = cursor;
                    table->unk2000 = table->count;
                    pxyFirstBatch = batch;
                    break;
                }
                if (table->batches[cursor].unk10 < 0 || table->batches[cursor].unk10 >= 256) {
                    break;
                }
                table->batches[table->batches[cursor].unk10].next = table->count;
                batch->unk10 = table->batches[cursor].unk10;
                batch->next = cursor;
                table->batches[cursor].unk10 = table->count;
                break;
            }
            if (table->batches[cursor].next < 0) {
                table->batches[cursor].next = table->count;
                batch->unk10 = cursor;
                batch->next = -1;
                pxyLastBatch = batch;
                break;
            }
            cursor = table->batches[cursor].next;
        }
    }
    slot->packet = packet;
    slot->batch = pxyBatchTables[0].count++;
    slot->translucentBatch = pxyBatchTables[1].count++;
}

void pxyQueueBatchBasic(PxySlot *slot, PxyPacket *packet) {
    PxyBatchTable *table;
    PxyBatch *batch;

    if (slot == 0) {
        return;
    }
    table = &pxyBatchTables[0];
    batch = &table->batches[table->count];
    if (pxyFirstBatch == 0) {
        table->unk2000 = table->count;
        table->unk2004 = table->count;
        pxyFirstBatch = batch;
        pxyLastBatch = batch;
    } else if (pxyLastBatch != 0) {
        pxyLastBatch->next = (u16)table->count;
        batch->unk10 = (u16)table->unk2000;
        batch->next = -1;
        pxyLastBatch = batch;
    }
    slot->packet = packet;
    slot->batch = pxyBatchTables[0].count++;
    slot->translucentBatch = pxyBatchTables[1].count++;
}

float tanf(float);
float pxyCalcFadeMetric(int cameraIndex, const Vector4 *point, float scale);

float pxyCalcFadeMetric(int cameraIndex, const Vector4 *point, float scale) {
    union {
        Vector4 vector;
        int words[4];
        ScreenVector4 projected;
    } screen; /* the projection helper stores fixed-point coordinates in vector storage */
    float result;
    XglCamera *camera;

    result = 0.0f;
    camera = xglStudioSelectGetActiveCamera(cameraIndex);
    if (camera == 0) {
        return result;
    }
    _CurMatrixSet(&camera->volume[0].viewMatrix);
    _CurSetViewScaleTrans(&camera->screen.screenScale, &camera->screen.screenOffset);
    _CurRotTransPersClipScreen(&screen.projected, point);
    result = (float)screen.words[2] * scale / tanf(camera->travel.fov * 0.5f);
    if (result < 0.0f) {
        result = 10000000.0f;
    }
    return result;
}

void pxyCalcModelBounds(PxyHeader *header)
{
    DoublewordVector4 sphere;
    Vector4 spare;

    xglVectorScaleXYZ(&sphere.vector, &pxyModelPosition, 1.0f / pxyModelScaleMax);
    if (pxyJointMatrices == 0) {
        xglVectorCopy_001CC998(&sphere.vector, &header->sphere);
    }
    sphere.vector.w = 1.0f;
    xglVectorMulMat(&pxyModelPosition, nmlModelDrawMatrix.rows, &sphere.vector);
    xglVectorCopy_001CC998(&sphere, &pxyModelPosition);
    sphere.vector.w = header->sphere.w * pxyModelScaleMax;
    pxyModelPosition.w = sphere.vector.w;
    pxyBoundSphere.words[0] = sphere.words[0];
    pxyBoundSphere.words[1] = sphere.words[1];
    pxyShadowHeightLower += pxyShadowOffsetY;
    pxyShadowHeightUpper += pxyShadowOffsetY;
    pxyFadeMetric = pxyCalcFadeMetric(0, &pxyModelPosition, pxyModelPosition.w);
}

void pxyCalcCameraVisibility(void)
{
    int i;
    int anyVisible;
    Matrix4 clipMatrix;
    PxyHeader *header;
    int forceMask;
    XglCamera *camera;
    int visible;

    anyVisible = 0;
    header = pxyCurrentHeader;
    forceMask = (nmlRenderStatus & NML_RENDER_TEXMAP_CIRCLE) | (pxyFlags & 0x1000);
    xglMatrixUnit(clipMatrix);
    for (i = 0; i < 4; i++) {
        camera = xglStudioSelectGetActiveCamera(i);
        if (pxyCameraEnable[i] != 0 && camera != 0) {
            if (nmlRenderStatus & NML_RENDER_PROREAL || pxyRenderLevel & 0x8 || header->flags & 0x88 || forceMask != 0) {
                pxyState.cameraVisible[i] = 1;
            } else {
                _ModelCalcClipInitMat1(camera->volume[0].planeSet[0], clipMatrix);
                visible = _ModelCalcClipMat1(&pxyBoundSphere.vector);
                if (visible && (header->flags & 2)) {
                    visible = nmlModelCalcClipCulling(camera, &pxyBoundSphere.vector) == 0;
                }
                if ((pxyState.cameraVisible[i] = visible)) {
                    anyVisible = 1;
                } else if ((pxyRenderLevel & 0x80) && pxyGlobalMatrices != 0 && !(pxyFlags & 0x10)) {
                    int j;

                    _ModelCalcClipInitMat1(camera->volume[0].planeSet[0], clipMatrix);
                    for (j = 0; j < header->nJointMatrix; j++) {
                        if (_ModelCalcClipMat1(&pxyGlobalMatrices[j].vectors[3])) {
                            pxyState.cameraVisible[i] = 1;
                            anyVisible = 1;
                            break;
                        }
                    }
                }
            }
            if (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE) {
                pxyState.cameraVisible[i] = 1;
            }
        }
    }
    if (anyVisible == 0) {
        if (!(nmlRenderStatus & NML_RENDER_PROREAL) && !(pxyRenderLevel & 0x8) && !(header->flags & 0x88) && forceMask == 0) {
            nmlRenderStatus |= NML_RENDER_HIDDEN;
        }
    }
    if (pxyForcedVisibilityRequested != 0 && pxyVisibleOverride == 0) {
        nmlRenderStatus |= NML_RENDER_HIDDEN;
    }
    if (nmlRenderStatus & NML_RENDER_HIDDEN) {
        pxyModelHiddenFlag = 1;
    }
}

typedef struct {
    u32 word[4];
} PxySlotWords;

void pxyCalcFogFactors(PxyHeader *header)
{
    int i;
    int j;

    if (!(nmlRenderStatus & NML_RENDER_FOG)) {
        return;
    }
    {
    for (i = 0; i < 4; i++) {
        if (pxyCameraEnable[i] != 0) {
            if (nmlRenderStatus & NML_RENDER_FOG_WHITE) {
                pxyState.cameraFog[i] = 0xA00;
            } else {
                pxyState.cameraFog[i] = nmlModelGetFogParaStudio(&pxyBoundSphere.vector, i) << 4;
            }
            if (pxyGeometryFogFactors != 0) {
                for (j = 0; j < header->geometryCount; j++) {
                    Vector4 local;
                    xglVectorAddXYZ_001CCA30(&local, pxyPartMatrices[j].rows[3], &pxyBoundSphere.vector);
                    ((PxySlotWords *)pxyGeometryFogFactors)[j].word[i] = nmlModelGetFogParaStudio(&local, i) << 4;
                }
            }
        }
    }
    }
}


void pxyCalcPartVisibility(PxyHeader *header)
{
    int i;
    int j;
    XglCamera *camera;
    PxyPart *part;
    u8 forcedBits;
    u8 flags21;
    Matrix4 partMatrix;
    u8 flags96;

    for (i = 0; i < 4; i++) {
        camera = xglStudioSelectGetActiveCamera(i);
        if (pxyCameraEnable[i] != 0 && camera != 0) {
            part = pxyParts;
            forcedBits = 0;
            if ((header->nJointMatrix != 0 && pxyJointMatrices != 0) || (pxyRenderLevel & 0x8)) {
                forcedBits = 15;
            }
            _ModelCalcClipInitMat1(camera->volume[0].planeSet[0], nmlModelDrawMatrix.rows);
            if (nmlRenderStatus & NML_RENDER_PROREAL || header->flags & 0x88) {
                flags21 = 1 << i;
                for (j = 0; j < header->partCount; j++) {
                    int visible;
                    Vector4 sphereWorld;

                    flags96 = (part->flags & 0x60) ? flags21 : 0;
                    visible = 1;
                    xglMatrixMul(partMatrix, nmlModelDrawMatrix.rows, pxyPartMatrices[part->matrixIndex].rows);
                    if (xglCullingExist()) {
                        sphereWorld = part->sphere;
                        sphereWorld.w = 1.0f;
                        xglVectorMulMat(&sphereWorld, partMatrix, &sphereWorld);
                        sphereWorld.w = part->sphere.w;
                        visible = nmlModelCalcClipCulling(camera, &sphereWorld) == 0;
                    }
                    _ModelCalcClipInitMat1(camera->volume[0].planeSet[0], partMatrix);
                    pxyPartVisibilityFlags[j] |= ((_ModelCalcClipMat1(&part->sphere) & visible) << i) | forcedBits | flags96;
                    part++;
                }
            } else {
                u8 *partFlagPtr;

                flags96 = 1 << i;
                partFlagPtr = pxyPartVisibilityFlags;
                for (j = 0; j < header->partCount; j++, partFlagPtr++) {
                    Vector4 sphereScaled;
                    int inside;

                    flags21 = (part->flags & 0x60) ? flags96 : 0;
                    inside = 1;
                    xglVectorCopy_001CC998(&sphereScaled, &part->sphere);
                    sphereScaled.w *= pxyModelScaleMax;
                    if ((pxyCurrentHeader->flags & 1) && xglCullingExist()) {
                        inside = nmlModelCalcClipCulling(camera, &sphereScaled) == 0;
                    }
                    _ModelCalcClipInitMat1(camera->volume[0].planeSet[0], nmlModelDrawMatrix.rows);
                    *partFlagPtr |= ((_ModelCalcClipMat1(&sphereScaled) & inside) << i) | forcedBits | flags21;
                    part++;
                }
            }
        }
    }
}

typedef struct {
    int e[8][2];
} PxyShuffleTable;

const PxyShuffleTable pxyDescrambleSwapTable = { .e = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 0, 2 }, { 1, 3 }, { 2, 0 }, { 3, 1 } } };

void pxyDescrambleHeader(PxyHeader *hdr)
{
    PxyHeader *h;
    u8 *bytes;
    int i;
    int seed;
    u32 j;
    u8 pairTemp;
    PxyShuffleTable swapPairs;
    u8 shuffleTemp;
    int unused100;

    if (hdr->flags & 0x80000000) {
        h = hdr;
        bytes = (u8 *)hdr;
        seed = 0x16253495;
        j = 13;
        swapPairs = pxyDescrambleSwapTable;
        for (i = *h->fileSize - 1; i >= 16; i--) {
            j = seed;
            j = ((2121 * i - 909) * (i - 1) + j) - 1;
            j = j % i;
            if (bytes[i] && bytes[j] && j >= 16) {
                shuffleTemp = bytes[i];
                bytes[i] = bytes[j];
                bytes[j] = shuffleTemp;
            }
        }
        i = 4;
        for (; i < (*h->fileSize >> 2); i++) {
            if (i * 4 >= *h->fileSize) {
                continue;
            }
            if (bytes[i * 4] || bytes[i * 4 + 1] || bytes[i * 4 + 2] || bytes[i * 4 + 3]) {
                pairTemp = bytes[i * 4 + swapPairs.e[i % 8][0]];
                bytes[i * 4 + swapPairs.e[i % 8][0]] = bytes[i * 4 + swapPairs.e[i % 8][1]];
                bytes[i * 4 + swapPairs.e[i % 8][1]] = pairTemp;
            }
        }
        hdr->flags &= 0x7fffffff;
    }
}

extern float pxyShadowPlaneHeight;
void pxyDescrambleHeader(PxyHeader *hdr);
void pxyEvaluateTracks(PxyHeader *);
void pxyCalcFogFactors(PxyHeader *header);
void pxyCalcModelBounds(PxyHeader *header);
void pxyCalcCameraVisibility(void);
void pxyCalcPartVisibility(PxyHeader *header);
void pxyQueueBatch(PxySlot *slot, PxyPacket *packet);

/* Validates a "pxy" model header and queues its parts as opaque and translucent draw batches (shadow, light and fade setup included). */
int pxySubmitModel(PxyHeader *header)
{
    XglCamera *camera;
    PxySlot *newSlot;
    PxySlot *slot;
    Vector4Elements vector;
    void *ptr;
    int i;
    int j;
    float fadeFullDistance;
    float fadeZeroDistance;
    float fadeFactor;
    Vector4 direction;
    PxyDrawEntry *entry;
    PxyDrawEntry *translucentEntry;
    PxyBatchTable *table;
    PxyBatch *batch;
    PxyBatchTable *translucentTable;
    PxyBatch *translucentBatch;
    int a;
    int b;
    int c;
    int d;
    PxySortWork work;
    PxySortWork *w;

    camera = xglStudioSelectGetActiveCamera(pxyActiveCameraIndex);
    newSlot = 0;
    slot = 0;
    pxyModelHiddenFlag = 0;
    if (camera == 0) {
        pxyResetGlobals();
        return 1;
    }
    if (pxyBatchedModelCount >= PXY_BATCH_MAX || pxyBatchTables[0].count >= PXY_BATCH_MAX) {
        pxyResetGlobals();
        return 1;
    }
    if ((u32)header & 0xF) {
        pxyResetGlobals();
        return 1;
    }
    if (header == 0 || header->magic[0] != 'p' || header->magic[1] != 'x' || header->magic[2] != 'y'
        || (int)(header->version * 10.0f) <= 0) {
        pxyResetGlobals();
        return 1;
    }
    pxyDescrambleHeader(header);
    pxyEvaluateTracks(header);
    if ((header->flags & 0x40) && (pxyFlags & 2)) {
        pxyFlags |= 1;
    }
    if ((header->flags & 0x100) && (pxyFlags & 8)) {
        pxyFlags |= 4;
    }
    if (header->flags & 0x800) {
        pxyFlags |= 0x8000;
    }
    if ((pxyFlags & 0x20) && !(pxyRenderLevel & 0x100)) {
        pxyFogParams = pxyGlobalFogPara;
        pxyFogColor = pxyGlobalFogColor;
        nmlRenderStatus |= NML_RENDER_FOG;
    }
    if (header->flags & 2) {
        if (pxyShadowMode == 1 || pxyShadowMode == 5 || pxyShadowMode == 4) {
            pxyRenderLevel |= 0x80;
        }
    }
    if (pxyRenderLevel & 0x10) {
        pxyVisibleOverride = -1;
    }
    if (pxyFlags & 0x80) {
        pxyFogColor.x = 0;
        pxyFogColor.y = 0;
        pxyFogColor.z = 0;
    }
    if (pxyFlags & 0x400) {
        nmlRenderStatus |= NML_RENDER_TEXMAP_DROP;
    }
    if (nmlRenderStatus & NML_RENDER_FOG_WHITE) {
        nmlRenderStatus |= NML_RENDER_FOG;
        pxyFogColor.x = 255.0f;
        pxyFogColor.y = 255.0f;
        pxyFogColor.z = 255.0f;
    }
    if (pxyCurrentLight->shadows[0].w == 0.0f) {
        vector.component[0] = pxyCurrentLight->directions[0][0];
        vector.component[2] = pxyCurrentLight->directions[0][2];
    } else {
        vector.component[0] = pxyCurrentLight->shadows[0].x;
        vector.component[2] = pxyCurrentLight->shadows[0].z;
    }
    if (vector.component[0] == 0.0f && vector.component[2] == 0.0f) {
        nmlRenderStatus |= NML_RENDER_LIGHT_VERTICAL;
    }
    if (pxyFlags & 0x80000) {
        if (strcmp(header->name, "MOM2_B04") == 0) {
            pxyMom2B04LightFlag = 1;
        } else if (pxyMom2B04LightFlag != 0) {
            ptr = &pxyState.light;
            ((XglLightSet *)ptr)->ambientColorLower.x = ((XglLightSet *)ptr)->ambientColor.x;
            ((XglLightSet *)ptr)->ambientColorLower.y = ((XglLightSet *)ptr)->ambientColor.y;
            ((XglLightSet *)ptr)->ambientColorLower.z = ((XglLightSet *)ptr)->ambientColor.z;
            ptr = pxyCurrentLight;
            ((XglLightSet *)ptr)->ambientColorLower.x = ((XglLightSet *)ptr)->ambientColor.x;
            ((XglLightSet *)ptr)->ambientColorLower.y = ((XglLightSet *)ptr)->ambientColor.y;
            ((XglLightSet *)ptr)->ambientColorLower.z = ((XglLightSet *)ptr)->ambientColor.z;
        }
    }
    pxyBatchedModelCount++;
    ptr = 0;
    pxyCurrentHeader = header;
    pxyParts = (PxyPart *)PXY_HEADER_AT(header, header->partOffset);
    pxyPartRanges = (PxyPartRange *)PXY_HEADER_AT(header, header->rangeOffset);
    pxyHeaderTable58 = PXY_HEADER_AT(header, header->offset58);
    pxyHeaderTable6C = PXY_HEADER_AT(header, header->geometryOffset);
    pxyHeaderTable3C = PXY_HEADER_AT(header, header->offset3C);
    pxyDefaultPartMatrices = PXY_HEADER_AT(header, header->entryMatrixOffset);
    pxyDefaultJointMatrices = PXY_HEADER_AT(header, header->offset70);
    pxyHeaderTable64 = PXY_HEADER_AT(header, header->jointMatrixOffset);
    pxyHeaderTable74 = PXY_HEADER_AT(header, header->offset74);
    pxyHeaderTable78 = PXY_HEADER_AT(header, header->offset78);
    if ((nmlRenderStatus & NML_RENDER_PROREAL) || (header->flags & 0x88)) {
        if (nmlRenderStatus & NML_RENDER_FOG) {
            pxyGeometryFogFactors = nmlPacketSetAttributeAlloc16N(header->geometryCount);
        }
    }
    pxyPartVisibilityFlags = nmlPacketSetAttributeAlloc16N(header->partCount / 16 + 1);
    xglVectorScaleXYZ(&pxyModelPosition, &pxyModelPosition, pxyModelScaleMax);
    memset(pxyPartVisibilityFlags, PXY_PART_VISIBLE, header->partCount);
    if (pxyPartMatrices == 0) {
        pxyPartMatrices = pxyDefaultPartMatrices;
    }
    xglMatrixInverse(&nmlModelDrawInverseMatrix, &nmlModelDrawMatrix);
    if ((nmlRenderStatus & (NML_RENDER_CLIP_PLANE_ANGLE | NML_RENDER_CLIP_PLANE_PAIR | NML_RENDER_CLIP_PLANE_MASKED)) && pxyClipPlanes != 0) {
        vector = (Vector4Elements){ { 0.0f, 0.0f, 1.0f, 0.0f } };
        for (i = 0; i < pxyClipPlaneCount; i++) {
            pxyClipPlanes->plane[i].component[3] = 1.0f;
            xglMatrixStackLoad(nmlModelDrawInverseMatrix.rows);
            xglMatrixStackMulVector(&pxyClipPlanes->plane[i].vector, &pxyClipPlanes->plane[i].vector);
            xglMatrixStackRotY(pxyClipPlanes->angle[i].component[1]);
            xglMatrixStackRotX(pxyClipPlanes->angle[i].component[0]);
            xglMatrixStackMulVector(&vector.vector, &vector.vector);
            vector.component[3] = -(vector.component[0] * pxyClipPlanes->plane[i].component[0] + vector.component[1] * pxyClipPlanes->plane[i].component[1]
                            + vector.component[2] * pxyClipPlanes->plane[i].component[2]);
            pxyClipPlanes->plane[i] = vector;
            pxyClipPlanes->angle[i].component[0] = pxyClipPlanes->angle[i].component[2];
            pxyClipPlanes->angle[i].component[1] = pxyClipPlanes->angle[i].component[3];
            pxyClipPlanes->angle[i].component[2] = pxyClipPlanes->angle[i].component[0] + pxyClipPlanes->angle[i].component[1];
        }
    }
    if (pxyParentHeader != 0) {
        i = pxySlotWriteIndex;
        for (j = PXY_SLOT_COUNT; j > 0; j--) {
            if (--i < 0) {
                i = PXY_SLOT_COUNT - 1;
            }
            if (pxyState.slots[i].packet != 0 && pxyState.slots[i].packet->header == pxyParentHeader) {
                slot = &pxyState.slots[i];
                ptr = slot->packet;
                pxyCurrentLight = ((PxyPacket *)ptr)->light;
                pxyCurrentShadow = ((PxyPacket *)ptr)->shadow;
                pxyShadowMatrix = ((PxyPacket *)ptr)->shadowMatrix;
                if (pxyTxyBlock == 0) {
                    pxyTxyBlock = ((PxyPacket *)ptr)->textureTable;
                }
                break;
            }
        }
        pxyCalcFogFactors(header);
    } else {
        newSlot = &pxyState.slots[pxySlotWriteIndex++];
        if (pxySlotWriteIndex >= PXY_SLOT_COUNT) {
            pxySlotWriteIndex = 0;
        }
        pxyCalcModelBounds(header);
        pxyCalcCameraVisibility();
        pxyCalcFogFactors(header);
    }
    if ((pxyFlags & 0x80010) && (header->flags & 2)) {
        fadeFullDistance = 173000.0f;
        fadeZeroDistance = 139000.0f;
        if (pxyFlags & 0x80000) {
            fadeFullDistance = 1073000.0f;
            fadeZeroDistance = 839000.0f;
        }
        if (pxyFadeMetric < fadeZeroDistance) {
            pxyReflTransparency = 0;
            pxyFadeAlpha = 0;
        } else if (pxyFadeMetric < fadeFullDistance) {
            pxyReflTransparency *= 1.0f - (fadeFullDistance - pxyFadeMetric) / (fadeFullDistance - fadeZeroDistance);
            pxyFadeAlpha *= 1.0f - (fadeFullDistance - pxyFadeMetric) / (fadeFullDistance - fadeZeroDistance);
        }
        if (pxyRenderLevel & 0x800) {
            pxyReflTransparency = 0;
        }
        if (pxyReflTransparency == 0.0f) {
            pxyRenderLevel |= 0x800;
        }
    }
    if (pxyRenderLevel & 0x1000) {
        fadeZeroDistance = 223000.0f;
        fadeFullDistance = 209000.0f;
        fadeFactor = 1.0f;
        if (pxyFadeMetric < fadeFullDistance) {
            fadeFactor = 0;
        } else if (pxyFadeMetric < fadeZeroDistance) {
            fadeFactor *= 1.0f - (fadeZeroDistance - pxyFadeMetric) / (fadeZeroDistance - fadeFullDistance);
        }
        if (fadeFactor <= 0.0f) {
            pxyResetGlobals();
            return 0;
        }
        if (fadeFactor < 1.0f) {
            nmlRenderStatus |= NML_RENDER_STENCIL;
            pxyReflTransparency *= fadeFactor;
            pxyTransparency *= fadeFactor;
            pxyCircleShadowScale *= fadeFactor;
            pxyFadeAlpha *= fadeFactor;
        }
    }
    if (!(nmlRenderStatus & NML_RENDER_HIDDEN) || (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE)) {
        pxyCalcPartVisibility(header);
        if (header->geometryCount < 0x100 && pxyHideMask != 0) {
            i = (int)pxyParts;
            for (j = 0; j < header->partCount; i += sizeof(PxyPart), j++) {
                if (pxyHideMask->bits[(s16)(((PxyPart *)i)->matrixIndex >> 5)] & (1 << (s16)(((PxyPart *)i)->matrixIndex & 0x1F))) {
                    pxyPartVisibilityFlags[j] = pxyPartVisibilityFlags[j] & 0x7F;
                }
            }
        }
    }
    if (header->nJointMatrix != 0 && pxyJointMatrices == 0) {
        pxyJointMatrices = pxyDefaultJointMatrices;
    }
    if (pxyParentHeader == 0) {
        switch (pxyShadowMode) {
        case 1:
        case 4:
        case 5:
            if (!(header->flags & 1)) {
                pxySetupShadowProjector(header);
            }
            break;
        case 3:
            pxyShadowMatrix = nmlPacketSetAttributeAlloc16N(4);
            xglMatrixUnit(pxyShadowMatrix->rows);
            if (pxyCurrentLight->shadows[0].w == 0.0f) {
                direction.x = pxyCurrentLight->directions[0][0];
                direction.y = 2.0f;
                direction.z = pxyCurrentLight->directions[0][2];
                if (pxyFlags & 0x10) {
                    direction.y = pxyCurrentLight->directions[0][1];
                }
            } else {
                direction.x = pxyCurrentLight->shadows[0].x;
                direction.y = pxyCurrentLight->shadows[0].y;
                direction.z = pxyCurrentLight->shadows[0].z;
            }
            xglVectorNormal(&direction, &direction);
            if (direction.y != 0.0f) {
                pxyShadowMatrix->rows[1][0] = -direction.x / direction.y;
                pxyShadowMatrix->rows[1][2] = -direction.z / direction.y;
            } else {
                pxyShadowMatrix->rows[1][0] = 0;
                pxyShadowMatrix->rows[1][2] = 0;
            }
            pxyShadowMatrix->rows[1][1] = 0;
            pxyShadowMatrix->rows[3][1] = pxyShadowOffsetY + 0.005f;
            pxyShadowMatrix->rows[3][0] = -pxyShadowOffsetY * pxyShadowMatrix->rows[1][0];
            pxyShadowMatrix->rows[3][2] = -pxyShadowOffsetY * pxyShadowMatrix->rows[1][2];
            pxyShadowMatrix->rows[3][1] = pxyShadowPlaneHeight + 0.005f;
            pxyShadowMatrix->rows[3][0] = -pxyShadowPlaneHeight * pxyShadowMatrix->rows[1][0];
            pxyShadowMatrix->rows[3][2] = -pxyShadowPlaneHeight * pxyShadowMatrix->rows[1][2];
            break;
        case 2:
            break;
        }
    }
    j = (int)nmlPacketSetAttributeData16N(&nmlRenderStatus, 0x2A);
    i = (int)((PxyPacket *)j)->ranges;
    ptr = &((PxyPacket *)j)->ranges[1];
    entry = &pxyDrawEntryTable[pxyOpaqueEntryCursor];
    translucentEntry = &pxyDrawEntryTable[pxyTranslucentEntryCursor];
    table = &pxyBatchTables[0];
    batch = &table->batches[table->count];
    translucentTable = &pxyBatchTables[1];
    translucentBatch = &translucentTable->batches[translucentTable->count];
    if (slot == 0) {
        batch->start = pxyOpaqueEntryCursor;
        batch->count = 0;
        batch->depth = xglPointLength(&((PxyPacket *)j)->center, &camera->travel.position) + nmlModelSortOffset;
        batch->flags = ((PxyPacket *)j)->renderStatus;
        batch->unk10 = -1;
        batch->next = -1;
        batch->packet = (PxyPacket *)j;
        batch->translucent = translucentBatch;
        translucentBatch->start = pxyTranslucentEntryCursor;
        translucentBatch->count = 0;
        translucentBatch->depth = 0;
        translucentBatch->flags = ((PxyPacket *)j)->renderStatus;
        translucentBatch->unk10 = -1;
        translucentBatch->next = -1;
        translucentBatch->packet = (PxyPacket *)j;
    } else {
        batch = &table->batches[slot->batch];
        translucentBatch = &translucentTable->batches[slot->translucentBatch];
        if (!(translucentBatch->flags & 4) && (((PxyPacket *)j)->renderStatus & 4)) {
            translucentBatch->flags |= 0x80000000;
        }
        batch->flags |= 0x400000;
        translucentBatch->flags |= 0x400000;
        translucentBatch->flags |= ((PxyPacket *)j)->renderStatus & 8;
        entry = &pxyDrawEntryTable[batch->start + batch->count];
        translucentEntry = &pxyDrawEntryTable[translucentBatch->start + translucentBatch->count];
    }
    if ((nmlRenderStatus & (NML_RENDER_CLIP_PLANE_MASKED | NML_RENDER_PROREAL)) || (header->flags & 0x88)) {
        batch->flags |= 0x40000000;
        translucentBatch->flags |= 0x40000000;
    }
    if (header->flags & 0x40) {
        batch->flags |= 0x4000000;
        translucentBatch->flags |= 0x4000000;
    }
    if (header->flags & 0x100) {
        batch->flags |= 0x2000000;
        translucentBatch->flags |= 0x2000000;
    }
    if ((header->flags & 0x800) || (nmlRenderStatus & NML_RENDER_CULLED)) {
        batch->flags |= 0x20000;
        translucentBatch->flags |= 0x20000;
    }
    if (pxyRenderLevel & 0x40) {
        batch->flags |= 0x100000;
        translucentBatch->flags |= 0x100000;
    }
    if (header->flags & 0x200) {
        batch->flags |= 0x800000;
        translucentBatch->flags |= 0x800000;
    }
    if (((PxyPacket *)j)->levelTable != 0) {
        batch->flags |= 0x80000000;
        translucentBatch->flags |= 0x80000000;
    }
    if ((header->flags & 0x20) || (nmlRenderStatus & NML_RENDER_ZWRITE)) {
        batch->flags |= 0x8000000;
        translucentBatch->flags |= 0x8000000;
    }
    if (!(((PxyPacket *)j)->renderStatus & 4)) {
        for (a = 0; a < header->layerCount; a++) {
            c = (int)&((PxyPacket *)j)->parts[((PxyPartRange *)i)->first];
            d = ((PxyPartRange *)i)->count;
            if (!(nmlRenderStatus & NML_RENDER_HIDDEN) || (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE)) {
                for (b = 0; b < d; b++, c += sizeof(PxyPart)) {
                    if (((PxyPart *)c)->layer == a) {
                        if (!(((PxyPacket *)j)->partFlags[((PxyPart *)c)->index] & PXY_PART_VISIBLE)) {
                            continue;
                        }
                        if ((header->flags & 1) && !(((PxyPacket *)j)->partFlags[((PxyPart *)c)->index] & PXY_PART_LOD_MASK)) {
                            continue;
                        }
                        if (batch->start + batch->count >= PXY_OPAQUE_ENTRY_END) {
                            break;
                        }
                        entry->part = (PxyPart *)c;
                        (entry++)->packet = (PxyPacket *)j;
                        batch->count++;
                    }
                }
            }
        }
    }
    if (((PxyPacket *)j)->renderStatus & 4) {
        for (d = 0; d < header->layerCount; d++) {
            b = (int)&((PxyPacket *)j)->parts[((PxyPartRange *)i)->first];
            a = ((PxyPartRange *)i)->count;
            if (!(nmlRenderStatus & NML_RENDER_HIDDEN) || (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE)) {
                for (c = 0; c < a; c++, b += sizeof(PxyPart)) {
                    if (((PxyPart *)b)->layer == d) {
                        if (!(((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_VISIBLE)) {
                            continue;
                        }
                        if (((PxyPart *)b)->flags & 0x10) {
                            continue;
                        }
                        if ((header->flags & 1) && !(((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_LOD_MASK)) {
                            continue;
                        }
                        if (translucentBatch->start + translucentBatch->count >= PXY_TRANSLUCENT_ENTRY_END) {
                            break;
                        }
                        translucentEntry->part = (PxyPart *)b;
                        (translucentEntry++)->packet = (PxyPacket *)j;
                        translucentBatch->count++;
                    }
                }
            }
        }
    }
    if (!(((PxyPacket *)j)->unmodeled_D4 & 1)) {
        if (!(nmlRenderStatus & NML_RENDER_HIDDEN) || (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE)) {
            for (d = header->layerCount - 1; d >= 0; d--) {
                b = (int)&((PxyPacket *)j)->parts[((PxyPartRange *)ptr)->first];
                if (header->flags & 0x200) {
                    for (c = 0; c < ((PxyPartRange *)ptr)->count; c++, b += sizeof(PxyPart)) {
                        if (((PxyPart *)b)->layer == d) {
                            if (((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_MERGED) {
                                continue;
                            }
                            if (((((PxyPart *)b)->flags & 0x80) || !(((PxyPart *)b)->flags & 0x40))
                                && !(((PxyPart *)b)->flags & 0x200)) {
                                if (!(((PxyPart *)b)->flags & 0x240)) {
                                    if (!(((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_VISIBLE)) {
                                        continue;
                                    }
                                    if ((header->flags & 1)
                                        && !(((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_LOD_MASK)) {
                                        continue;
                                    }
                                }
                                if (translucentBatch->start + translucentBatch->count >= PXY_TRANSLUCENT_ENTRY_END) {
                                    break;
                                }
                                translucentEntry->part = (PxyPart *)b;
                                (translucentEntry++)->packet = (PxyPacket *)j;
                                translucentBatch->count++;
                                if (((PxyPart *)b)->flags & 0x80) {
                                    for (a = c + 1; a < ((PxyPartRange *)ptr)->count; a++) {
                                        if (((PxyPart *)b + 1)->flags & 0x80) {
                                            break;
                                        }
                                        if (((PxyPart *)b + 1)->flags & 0x240) {
                                            if (translucentBatch->start + translucentBatch->count >= PXY_TRANSLUCENT_ENTRY_END) {
                                                break;
                                            }
                                            c++;
                                            b += sizeof(PxyPart);
                                            translucentEntry->part = (PxyPart *)b;
                                            (translucentEntry++)->packet = (PxyPacket *)j;
                                            ((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] =
                                                ((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] | PXY_PART_MERGED;
                                            translucentBatch->count++;
                                        } else {
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (((PxyPacket *)j)->drawFlags & 2) {
                        break;
                    }
                } else {
                    a = ((PxyPartRange *)ptr)->count;
                    if (!(header->flags & 0x1000) || header->sortEntryOffset == 0) {
                        for (c = 0; c < a; c++, b += sizeof(PxyPart)) {
                            if (((PxyPart *)b)->layer == d) {
                                if (!(((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_VISIBLE)) {
                                    continue;
                                }
                                if ((header->flags & 1)
                                    && !(((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_LOD_MASK)) {
                                    continue;
                                }
                                if (translucentBatch->start + translucentBatch->count >= PXY_TRANSLUCENT_ENTRY_END) {
                                    break;
                                }
                                translucentEntry->part = (PxyPart *)b;
                                (translucentEntry++)->packet = (PxyPacket *)j;
                                translucentBatch->count++;
                            }
                        }
                    } else {
                        w = &work;
                        w->sortEntries = (PxySortEntry *)PXY_HEADER_AT(header, header->sortEntryOffset);
                        w->entryCount = 0;
                        w->group = w->groups;
                        w->entries = translucentEntry;
                        w->groupCount = 0;
                        w->group->count = 0;
                        w->item = w->group->items;
                        w->item->count = 0;
                        w->matrixSet = pxyGlobalMatrices;
                        w->matrices = pxyJointMatrices;
                        xglMatrixCopy_001CC9C4(w->matrix, nmlModelDrawMatrix.rows);
                        xglVectorCopy_001CC998(&w->eye, &camera->travel.position);
                        for (c = 0; c < a; c++, b += sizeof(PxyPart)) {
                            if (((PxyPart *)b)->layer == d) {
                                if (!(((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_VISIBLE)) {
                                    continue;
                                }
                                if ((header->flags & 1)
                                    && !(((PxyPacket *)j)->partFlags[((PxyPart *)b)->index] & PXY_PART_LOD_MASK)) {
                                    continue;
                                }
                                if (translucentBatch->start + translucentBatch->count >= PXY_TRANSLUCENT_ENTRY_END) {
                                    break;
                                }
                                translucentEntry->part = (PxyPart *)b;
                                (translucentEntry++)->packet = (PxyPacket *)j;
                                translucentBatch->count++;
                                w->entryCount++;
                            }
                        }
                        {
                            for (w->i = 0; w->i < w->entryCount; w->i++) {
                                w->sortEntry = &w->sortEntries[w->entries[w->i].part->matrixIndex];
                                if (w->sortEntry->key >= 0) {
                                    if (w->item->count == 0 || w->item->key != w->sortEntry->key) {
                                        if (w->item->count != 0) {
                                            w->item++;
                                            w->group->count++;
                                        }
                                        w->item->start = w->i;
                                        w->item->count = 1;
                                        w->item->key = w->sortEntry->key;
                                        w->item->matrix = w->sortEntry->matrix;
                                        xglVectorCopy_001CC998(w->item, w->sortEntry);
                                    } else {
                                        w->item->count++;
                                    }
                                    if (w->i == w->entryCount - 1) {
                                        w->group->count++;
                                        w->group++;
                                        w->groupCount++;
                                        w->group->count = 0;
                                        w->item = w->group->items;
                                        w->item->count = 0;
                                    }
                                } else if (w->item->count != 0) {
                                    w->group->count++;
                                    w->group++;
                                    w->groupCount++;
                                    w->group->count = 0;
                                    w->item = w->group->items;
                                    w->item->count = 0;
                                }
                            }
                        }
                        w->group = w->groups;
                        for (w->i = 0; w->i < w->groupCount; w->i++, w->group++) {
                            w->item = w->group->items;
                            w->group->start = w->group->items[0].start;
                            w->group->entryCount = 0;
                            for (w->j = 0; w->j < w->group->count; w->j++, w->item++) {
                                w->group->entryCount += w->item->count;
                                w->group->sorted[w->j] = w->item;
                                w->item->position.w = 1.0f;
                                if (w->item->matrix >= 0 && w->matrixSet != 0 && w->matrices != 0) {
                                    xglMatrixStackLoad(w->matrix);
                                    xglMatrixStackMul(w->matrices[w->item->matrix].rows);
                                    xglMatrixStackMulVector(&w->item->position, &w->item->position);
                                    w->item->depth = xglPointLength(&w->item->position, &w->eye);
                                } else {
                                    xglMatrixStackLoad(w->matrix);
                                    xglMatrixStackMulVector(&w->item->position, &w->item->position);
                                    w->item->depth = xglPointLength(&w->item->position, &w->eye);
                                }
                            }
                            {
                                do {
                                    w->swapped = 0;
                                    w->cursor = w->group->sorted;
                                    for (w->j = 0; w->j < w->group->count - 1; w->j++, w->cursor++) {
                                        if (w->cursor[0]->depth < w->cursor[1]->depth) {
                                            w->swapped = *w->cursor;
                                            *w->cursor = w->cursor[1];
                                            w->cursor[1] = w->swapped;
                                        }
                                    }
                                } while (w->swapped != 0);
                            }
                            w->out = w->buffer;
                            w->cursor = w->group->sorted;
                            for (w->j = 0; w->j < w->group->count; w->j++, w->cursor++) {
                                memcpy(w->out, &w->entries[(*w->cursor)->start], (*w->cursor)->count * 8);
                                w->out += (*w->cursor)->count;
                            }
                            memcpy(&w->entries[w->group->start], w->buffer, w->group->entryCount * 8);
                        }
                    }
                }
            }
        }
    }
    pxyQueueBatch(newSlot, (PxyPacket *)j);
    pxyOpaqueEntryCursor += batch->count;
    pxyTranslucentEntryCursor += translucentBatch->count;
    pxyResetGlobals();
    return 0;
}

void pxyQueueBatchBasic(PxySlot *slot, PxyPacket *packet);

/* Reduced variant of pxySubmitModel: no header validation, shadow or fade setup; queues the parts as draw batches. */
int pxySubmitModelBasic(PxyHeader *header)
{
    XglCamera *camera;
    PxySlot *newSlot;
    int j;
    int i;
    void *ptr;
    PxyDrawEntry *entry;
    PxyDrawEntry *translucentEntry;
    PxyBatchTable *table;
    PxyBatch *batch;
    PxyBatchTable *translucentTable;
    PxyBatch *translucentBatch;
    int a;
    int b;
    int c;
    int d;

    camera = xglStudioSelectGetActiveCamera(pxyActiveCameraIndex);
    newSlot = 0;
    pxyModelHiddenFlag = 0;
    if (pxyBatchedModelCount >= PXY_BATCH_MAX || pxyBatchTables[0].count >= PXY_BATCH_MAX) {
        pxyResetGlobals();
        return 1;
    }
    pxyEvaluateTracks(header);
    if ((header->flags & 0x100) && (pxyFlags & 8)) {
        pxyFlags |= 4;
    }
    if (pxyRenderLevel & 0x10) {
        pxyVisibleOverride = -1;
    }
    if (pxyFlags & 0x80) {
        pxyFogColor.x = 0;
        pxyFogColor.y = 0;
        pxyFogColor.z = 0;
    }
    if (pxyFlags & 0x400) {
        nmlRenderStatus |= NML_RENDER_TEXMAP_DROP;
    }
    pxyBatchedModelCount++;
    pxyCurrentHeader = header;
    pxyParts = (PxyPart *)PXY_HEADER_AT(header, header->partOffset);
    pxyPartRanges = (PxyPartRange *)PXY_HEADER_AT(header, header->rangeOffset);
    pxyHeaderTable58 = PXY_HEADER_AT(header, header->offset58);
    pxyHeaderTable6C = PXY_HEADER_AT(header, header->geometryOffset);
    pxyHeaderTable3C = PXY_HEADER_AT(header, header->offset3C);
    pxyDefaultPartMatrices = PXY_HEADER_AT(header, header->entryMatrixOffset);
    pxyDefaultJointMatrices = PXY_HEADER_AT(header, header->offset70);
    pxyHeaderTable64 = PXY_HEADER_AT(header, header->jointMatrixOffset);
    pxyHeaderTable74 = PXY_HEADER_AT(header, header->offset74);
    pxyHeaderTable78 = PXY_HEADER_AT(header, header->offset78);
    pxyPartVisibilityFlags = nmlPacketSetAttributeAlloc16N(header->partCount / 16 + 1);
    xglVectorScaleXYZ(&pxyModelPosition, &pxyModelPosition, pxyModelScaleMax);
    memset(pxyPartVisibilityFlags, PXY_PART_VISIBLE, header->partCount);
    pxyPartMatrices = pxyDefaultPartMatrices;
    xglMatrixInverse(&nmlModelDrawInverseMatrix, &nmlModelDrawMatrix);
    newSlot = &pxyState.slots[pxySlotWriteIndex++];
    if (pxySlotWriteIndex >= PXY_SLOT_COUNT) {
        pxySlotWriteIndex = 0;
    }
    pxyCalcModelBounds(header);
    pxyCalcCameraVisibility();
    if (!(nmlRenderStatus & NML_RENDER_HIDDEN) || (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE)) {
        pxyCalcPartVisibility(header);
        if (header->geometryCount < 0x100 && pxyHideMask != 0) {
            i = (int)pxyParts;
            for (j = 0; j < header->partCount; i += sizeof(PxyPart), j++) {
                if (pxyHideMask->bits[(s16)(((PxyPart *)i)->matrixIndex >> 5)] & (1 << (s16)(((PxyPart *)i)->matrixIndex & 0x1F))) {
                    pxyPartVisibilityFlags[j] = pxyPartVisibilityFlags[j] & 0x7F;
                }
            }
        }
    }
    i = (int)nmlPacketSetAttributeData16N(&nmlRenderStatus, 0x2A);
    j = (int)((PxyPacket *)i)->ranges;
    ptr = &((PxyPacket *)i)->ranges[1];
    entry = &pxyDrawEntryTable[pxyOpaqueEntryCursor];
    translucentEntry = &pxyDrawEntryTable[pxyTranslucentEntryCursor];
    table = &pxyBatchTables[0];
    batch = &table->batches[table->count];
    translucentTable = &pxyBatchTables[1];
    translucentBatch = &translucentTable->batches[translucentTable->count];
    batch->start = pxyOpaqueEntryCursor;
    batch->count = 0;
    batch->depth = xglPointLength(&((PxyPacket *)i)->center, &camera->travel.position) + nmlModelSortOffset;
    batch->flags = ((PxyPacket *)i)->renderStatus;
    batch->unk10 = -1;
    batch->next = -1;
    batch->packet = (PxyPacket *)i;
    batch->translucent = translucentBatch;
    translucentBatch->start = pxyTranslucentEntryCursor;
    translucentBatch->count = 0;
    translucentBatch->depth = 0;
    translucentBatch->flags = ((PxyPacket *)i)->renderStatus;
    translucentBatch->unk10 = -1;
    translucentBatch->next = -1;
    translucentBatch->packet = (PxyPacket *)i;
    if (header->flags & 0x100) {
        batch->flags |= 0x2000000;
        translucentBatch->flags |= 0x2000000;
    }
    if (pxyRenderLevel & 0x40) {
        batch->flags |= 0x100000;
        translucentBatch->flags |= 0x100000;
    }
    if ((header->flags & 0x20) || (nmlRenderStatus & NML_RENDER_ZWRITE)) {
        batch->flags |= 0x8000000;
        translucentBatch->flags |= 0x8000000;
    }
    if (!(((PxyPacket *)i)->renderStatus & 4)) {
        for (a = 0; a < header->layerCount; a++) {
            c = (int)&((PxyPacket *)i)->parts[((PxyPartRange *)j)->first];
            d = ((PxyPartRange *)j)->count;
            if (!(nmlRenderStatus & NML_RENDER_HIDDEN) || (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE)) {
                for (b = 0; b < d; b++, c += sizeof(PxyPart)) {
                    if (((PxyPart *)c)->layer == a) {
                        if (!(((PxyPacket *)i)->partFlags[((PxyPart *)c)->index] & PXY_PART_VISIBLE)) {
                            continue;
                        }
                        if (batch->start + batch->count >= PXY_OPAQUE_ENTRY_END) {
                            break;
                        }
                        entry->part = (PxyPart *)c;
                        (entry++)->packet = (PxyPacket *)i;
                        batch->count++;
                    }
                }
            }
        }
    }
    if (((PxyPacket *)i)->renderStatus & 4) {
        for (d = 0; d < header->layerCount; d++) {
            b = (int)&((PxyPacket *)i)->parts[((PxyPartRange *)j)->first];
            a = ((PxyPartRange *)j)->count;
            if (!(nmlRenderStatus & NML_RENDER_HIDDEN) || (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE)) {
                for (c = 0; c < a; c++, b += sizeof(PxyPart)) {
                    if (((PxyPart *)b)->layer == d) {
                        if (!(((PxyPacket *)i)->partFlags[((PxyPart *)b)->index] & PXY_PART_VISIBLE)) {
                            continue;
                        }
                        if (((PxyPart *)b)->flags & 0x10) {
                            continue;
                        }
                        if (translucentBatch->start + translucentBatch->count >= PXY_TRANSLUCENT_ENTRY_END) {
                            break;
                        }
                        translucentEntry->part = (PxyPart *)b;
                        (translucentEntry++)->packet = (PxyPacket *)i;
                        translucentBatch->count++;
                    }
                }
            }
        }
    }
    if (!(((PxyPacket *)i)->unmodeled_D4 & 1)) {
        if (!(nmlRenderStatus & NML_RENDER_HIDDEN) || (nmlRenderStatus & NML_RENDER_FORCE_VISIBLE)) {
            for (d = header->layerCount - 1; d >= 0; d--) {
                b = (int)&((PxyPacket *)i)->parts[((PxyPartRange *)ptr)->first];
                a = ((PxyPartRange *)ptr)->count;
                for (c = 0; c < a; c++, b += sizeof(PxyPart)) {
                    if (((PxyPart *)b)->layer == d) {
                        if (!(((PxyPacket *)i)->partFlags[((PxyPart *)b)->index] & PXY_PART_VISIBLE)) {
                            continue;
                        }
                        if (translucentBatch->start + translucentBatch->count >= PXY_TRANSLUCENT_ENTRY_END) {
                            break;
                        }
                        translucentEntry->part = (PxyPart *)b;
                        (translucentEntry++)->packet = (PxyPacket *)i;
                        translucentBatch->count++;
                    }
                }
            }
        }
    }
    pxyQueueBatchBasic(newSlot, (PxyPacket *)i);
    pxyOpaqueEntryCursor += batch->count;
    pxyTranslucentEntryCursor += translucentBatch->count;
    pxyResetGlobals();
    return 0;
}

void pxyPlayEntryGroup(int group) {
    int i;
    PxyEntry *entry;

    if (((1 << group) & (unsigned int)pxyEntryGroupMask) != 0) {
        for (i = 0; i < pxyEntryCount; i++) {
            entry = &pxyEntryQueue[i];
            if (entry->group == group) {
                xglPacketAddRefOffset(entry->data, entry->value, entry->state);
            }
        }
    }
}

void pxyPlayEntryGroupFiltered(int group, int filter) {
    int i;
    PxyEntry *entry;
    register int mask;

    mask = 1 << group;
    if ((mask & pxyEntryGroupMask) != 0) {
        for (i = 0; i < pxyEntryCount; i++) {
            entry = &pxyEntryQueue[i];
            if (entry->group == group) {
                if (entry->flag == filter) {
                    if ((signed char)entry->row == 1) {
                        xglDirectQueueAppendFlushWide(entry->data, entry->value);
                    } else {
                        xglPacketAddRefOffset(entry->data, entry->value, entry->state);
                    }
                }
            }
        }
    }
}

void nmlModelDirectSend(int key, void *data, int qwc)
{
    nmlModelDirectSendFull(key, data, qwc, 0, 0);
}

void nmlModelDirectSendWithState(int key, void *data, int qwc, int offset) {
    nmlModelDirectSendFull(key, data, qwc, 0, offset);
}

void nmlModelDirectSendFlagged(int key, void *data, int qwc, int flag)
{
    nmlModelDirectSendFull(key, data, qwc, flag, 0);
}

void nmlModelDirectSendFull(int key, void *data, int qwc, int flag, int state) {
    PxyEntry *entry;

    if (pxyFlushDoneFlag != 0) {
        return;
    }
    if (pxyEntryCount < 3048) {
            entry = &pxyEntryQueue[pxyEntryCount];
            entry->value = qwc;
            entry->flag = flag;
            if ((key & 1) == 0) {
                entry->data = nmlPacketSetAttributeData16N(data, qwc);
            } else {
                entry->data = data;
            }
            entry->group = (key % 12) >> 1;
            entry->row = key / 12;
            entry->state = state;
            pxyEntryGroupMask |= 1 << entry->group;
            pxyEntryCount += 1;
    }
}

int pxyPartNotInIndexList(PxyPacket *packet, PxyDrawWork *work) {
    if (packet->partIndexList != 0) {
        int i = 0;

        for (i = 0; i < 16; i++) {
            if (packet->partIndexList[i] == 0xFF) {
                break;
            }
            if (work->part->index == packet->partIndexList[i]) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

typedef struct XglPacket XglPacket;

extern int pxyCurrentCamera;
extern int pxyDrawPass;
extern u16 sRenderDepthBase;
extern s16 sRenderPsm;
int pxyPartNotInIndexList(PxyPacket *packet, PxyDrawWork *work);
void sceVif1PkAddUpkData128(XglPacket *packet, XglQuadword);

/* Draws the batches of shadow caster `caster` into its shadow texture at frameBase. */
void pxyDrawShadowCaster(int caster, u32 frameBase)
{
    int i;
    PxyPacket *packet;
    PxyShadow *shadow;
    int savedPass;
    u64 *p;

    packet = pxyState.shadowBatches[caster]->packet;
    shadow = packet->shadow;
    savedPass = pxyDrawPass;
    pxyDrawWork->frameBase = frameBase;
    pxyDrawWork->unk28 = 0;
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    {
        u64 *start;

        nmlPacketGsInit();
        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
        start = p;
        GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
        GS_PUT(p, GS_FBA_1, 0);
        GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
        GS_PUT(p, GS_CLAMP_1, shadow->clamp);
        GS_PUT(p, GS_TEX0_1, shadow->tex0);
        GS_PUT(p, GS_ALPHA_1, shadow->alphaData);
        xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
    }
    nmlPacketAddGsFlushWide();
    for (i = 0; i < pxyOverlayBatchCount; i++) {
        int pass;
        AlignedVector4 v;
        void *q;
        Matrix m;
        Vector4 box[2];
        Vector4 position;
        AlignedVector4 sphere;
        Matrix local;

        pxyDrawWork->batch = pxyState.overlayBatches[i];
        if ((pxyFlags & 0x10) && !(pxyFlags & 0x40000) && (packet->drawMode == 1 || packet->drawMode == 4)
            && packet->lowerClipHeight < packet->upperClipHeight) {
            v = (AlignedVector4){ 0.0f, -1.0f, 0.0f, 0.0f };
            v.w = -(v.x * packet->center.x + v.y * packet->lowerClipHeight + v.z * packet->center.z);
            if (pxyDrawWork->batch->packet->planes == 0) {
                pxyDrawWork->batch->packet->planes = nmlPacketSetAttributeAlloc16N(5);
            }
            pxyDrawWork->batch->packet->planes->plane[0] = v;
            pxyDrawWork->batch->packet->planes->plane[1] = v;
            v = (AlignedVector4){ 0.0f, 1.0f, 0.0f, 0.0f };
            v.w = -(v.x * packet->center.x + v.y * packet->upperClipHeight + v.z * packet->center.z);
            if (pxyDrawWork->batch->packet->planes == 0) {
                pxyDrawWork->batch->packet->planes = nmlPacketSetAttributeAlloc16N(5);
            }
            pxyDrawWork->batch->packet->planes->plane[1] = v;
            pxyDrawWork->batch->packet->planeCount = 2;
            pxyDrawWork->batch->packet->renderStatus |= 0x4000;
        }
        nmlPacketGsInit();
        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
        q = p;
        GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm,
                                           pxyDrawWork->batch->packet->frameMask | 0xFF000000));
        GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm,
                                           pxyDrawWork->batch->packet->frameMask | 0xFF000000));
        xglDirectQueueCount += ((u32)p - (u32)q) >> 4;
        nmlPacketAddGsFlush();
        for (pass = 0; pass < 2; pass++) {
            pxyDrawWork->lastPacket = pxyDrawWork->batch->packet;
            pxyDrawPass = pass;
            pxySetDrawPass(pxyDrawPass);
            if (pass != 0) {
                pxyDrawWork->batch = pxyDrawWork->batch->translucent;
            }
            if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == pxyDrawWork->batch->packet->camera)
                  && pxyDrawWork->batch->packet->cameraMask[pxyCurrentCamera] != 0)) {
                continue;
            }
            pxyDrawWork->batch->packet->renderStatus |= 0x20000000;
            pxyUploadPacketState(pxyDrawWork->batch->packet->renderStatus & 4, pxyDrawWork->batch->packet);
            nmlPacketAddTransMicrocode(pxyDrawWork->batch->packet);
            q = xglPacketGetCurrent();
            xglMatrixStackLoad(shadow->textureInverse.rows);
            xglMatrixStackMul(pxyDrawWork->batch->packet->localMatrix);
            xglMatrixStackSave(m.rows);
            sceVif1PkCnt(q, 0);
            sceVif1PkOpenUpkCode(q, 0x39E, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(q, &m, 4);
            sceVif1PkCloseUpkCode(q);
            sceVif1PkOpenUpkCode(q, 0x3C3, 0x6C, 1, 1);
            sceVif1PkAddUpkData128(q, *(XglQuadword *)&shadow->uv);
            sceVif1PkCloseUpkCode(q);
            xglMatrixUnit(m.rows);
            shadow->center.w = 1.0f;
            shadow->lighting.direction.w = 0;
            xglVectorCopy_001CC998(m.rows[3], &shadow->lighting.direction);
            m.rows[0][3] = m.rows[3][0];
            m.rows[1][3] = m.rows[3][1];
            m.rows[2][3] = m.rows[3][2];
            m.rows[0][2] = shadow->center.x;
            m.rows[1][2] = shadow->center.y;
            m.rows[2][2] = shadow->center.z;
            m.rows[0][0] = 128.0f / (packet->center.w * 2.0f);
            if (packet->drawMode == 4) {
                m.words[0][1] = 1;
            }
            if (pxyFlags & 0x20000) {
                m.words[1][0] = 1;
            }
            sceVif1PkOpenUpkCode(q, 0x392, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(q, &m, 4);
            sceVif1PkCloseUpkCode(q);
            pxyDrawWork->entry = &pxyDrawEntryTable[pxyDrawWork->batch->start];
            for (pxyDrawWork->i = 0; pxyDrawWork->i < pxyDrawWork->batch->count;
                 pxyDrawWork->i++, pxyDrawWork->entry++) {
                pxyDrawWork->packet = pxyDrawWork->entry->packet;
                pxyDrawWork->part = pxyDrawWork->entry->part;
                if (pxyDrawWork->packet->header->flags & 0x80) {
                    break;
                }
                if (pxyPartNotInIndexList(packet, pxyDrawWork)) {
                    continue;
                }
                if (pxyDrawWork->part->flags & 0x1000) {
                    continue;
                }
                if (pxyFlags & 0x10) {
                    if (pxyShadowCasterMode == 2) {
                        if (pass != 0 && !(pxyDrawWork->part->flags & 0x20000)) {
                            continue;
                        }
                        if (pass == 0 && !(pxyDrawWork->part->flags & 0x20000)
                            && (pxyDrawWork->part->flags & 0x10)) {
                            continue;
                        }
                    } else if (pxyShadowCasterMode == 1 && (pxyDrawWork->part->flags & 0x10)) {
                        continue;
                    }
                } else if (pxyShadowCasterMode == 1 && (pxyDrawWork->part->flags & 0x10)) {
                    continue;
                }
                if (pxyFlags & 0x10) {
                    xglVectorAddXYZ_001CCA30(&box[0], &pxyDrawWork->part->boundsMax, pxyDrawWork->batch->packet->localMatrix[3]);
                    xglVectorAddXYZ_001CCA30(&box[1], &pxyDrawWork->part->boundsMin, pxyDrawWork->batch->packet->localMatrix[3]);
                    if (shadow->min.x > box[0].x || shadow->min.y > box[0].y || shadow->min.z > box[0].z
                        || shadow->max.x < box[1].x || shadow->max.y < box[1].y || shadow->max.z < box[1].z) {
                        continue;
                    }
                } else {
                    v = pxyDrawWork->part->sphere;
                    v.w = 1.0f;
                    xglVectorMulMat(&box[0], pxyDrawWork->batch->packet->localMatrix, &v);
                    if (shadow->min.x > box[0].x + pxyDrawWork->part->sphere.w
                        || shadow->min.y > box[0].y + pxyDrawWork->part->sphere.w
                        || shadow->min.z > box[0].z + pxyDrawWork->part->sphere.w
                        || shadow->max.x < box[0].x - pxyDrawWork->part->sphere.w
                        || shadow->max.y < box[0].y - pxyDrawWork->part->sphere.w
                        || shadow->max.z < box[0].z - pxyDrawWork->part->sphere.w) {
                        continue;
                    }
                }
                pxyDrawWork->packet->renderStatus |= 0x20000000;
                pxyUploadBoneMatrices();
                if ((pxyDrawWork->batch->flags & 0x40000000) || pxyDrawWork->lastPacket != pxyDrawWork->packet) {
                    pxyUploadViewMatrices();
                }
                pxyDrawPartData();
                pxyDrawWork->packet->renderStatus &= ~0x20000000;
                pxyDrawWork->lastPacket = pxyDrawWork->packet;
            }
            if (pxyDrawWork->i < pxyDrawWork->batch->count) {
                for (; pxyDrawWork->i < pxyDrawWork->batch->count; pxyDrawWork->i++, pxyDrawWork->entry++) {
                    q = &pxyDrawWork->entry->packet->boneMatrices[pxyDrawWork->entry->part->matrixIndex];
                    pxyDrawWork->packet = pxyDrawWork->entry->packet;
                    pxyDrawWork->part = pxyDrawWork->entry->part;
                    if (pxyDrawWork->part->flags & 0x1000) {
                        continue;
                    }
                    if (pxyFlags & 0x10) {
                        if (pxyShadowCasterMode == 2) {
                            if (pass != 0 && !(pxyDrawWork->part->flags & 0x20000)) {
                                continue;
                            }
                            if (pass == 0 && !(pxyDrawWork->part->flags & 0x20000)
                                && (pxyDrawWork->part->flags & 0x10)) {
                                continue;
                            }
                        } else if (pxyShadowCasterMode == 1 && (pxyDrawWork->part->flags & 0x10)) {
                            continue;
                        }
                    } else if (pxyShadowCasterMode == 1 && (pxyDrawWork->part->flags & 0x10)) {
                        continue;
                    }
                    if (pxyFlags & 0x10) {
                        v = pxyDrawWork->part->sphere;
                        v.w = 1.0f;
                        xglMatrixStackLoad(pxyDrawWork->entry->packet->localMatrix);
                        xglMatrixStackMul(q);
                        xglMatrixStackMulVector(&v, &v);
                        v.w = pxyDrawWork->part->sphere.w;
                        if (shadow->min.x > v.x + pxyDrawWork->part->sphere.w
                            || shadow->min.y > v.y + pxyDrawWork->part->sphere.w
                            || shadow->min.z > v.z + pxyDrawWork->part->sphere.w
                            || shadow->max.x < v.x - pxyDrawWork->part->sphere.w
                            || shadow->max.y < v.y - pxyDrawWork->part->sphere.w
                            || shadow->max.z < v.z - pxyDrawWork->part->sphere.w) {
                            continue;
                        }
                    } else {
                        sphere = pxyDrawWork->part->sphere;
                        sphere.w = 1.0f;
                        xglVectorMulMat(&position, q, &sphere);
                        if (shadow->min.x > position.x + pxyDrawWork->part->sphere.w
                            || shadow->min.y > position.y + pxyDrawWork->part->sphere.w
                            || shadow->min.z > position.z + pxyDrawWork->part->sphere.w
                            || shadow->max.x < position.x - pxyDrawWork->part->sphere.w
                            || shadow->max.y < position.y - pxyDrawWork->part->sphere.w
                            || shadow->max.z < position.z - pxyDrawWork->part->sphere.w) {
                            continue;
                        }
                    }
                    p = (u64 *)xglPacketGetCurrent();
                    xglMatrixMul(local.rows, shadow->textureInverse.rows, pxyDrawWork->packet->localMatrix);
                    xglMatrixMul(local.rows, local.rows, pxyDrawWork->packet->boneMatrices[pxyDrawWork->part->matrixIndex].rows);
                    sceVif1PkCnt((XglPacket *)p, 0);
                    sceVif1PkOpenUpkCode((XglPacket *)p, 0x39E, 0x6C, 1, 1);
                    sceVif1PkAddUpkData128N((XglPacket *)p, &local, 4);
                    sceVif1PkCloseUpkCode((XglPacket *)p);
                    sceVif1PkOpenUpkCode((XglPacket *)p, 0x3C3, 0x6C, 1, 1);
                    sceVif1PkAddUpkData128((XglPacket *)p, *(XglQuadword *)&shadow->uv);
                    sceVif1PkCloseUpkCode((XglPacket *)p);
                    xglMatrixMul(local.rows, pxyDrawWork->packet->localMatrix, pxyDrawWork->packet->boneMatrices[pxyDrawWork->part->matrixIndex].rows);
                    xglMatrixInverse(&local, &local);
                    shadow->center.w = 1.0f;
                    xglVectorMulMat(&sphere, local.rows, &shadow->center);
                    shadow->lighting.direction.w = 0;
                    xglVectorMulMat((Vector4 *)local.rows[3], local.rows, &shadow->lighting.direction);
                    local.rows[0][3] = local.rows[3][0];
                    local.rows[1][3] = local.rows[3][1];
                    local.rows[2][3] = local.rows[3][2];
                    local.rows[0][2] = sphere.x;
                    local.rows[1][2] = sphere.y;
                    local.rows[2][2] = sphere.z;
                    local.rows[0][0] = 128.0f / (packet->center.w * 2.0f);
                    if (packet->drawMode == 4) {
                        local.words[0][1] = 1;
                    }
                    if (pxyFlags & 0x20000) {
                        local.words[1][0] = 1;
                    }
                    sceVif1PkOpenUpkCode((XglPacket *)p, 0x392, 0x6C, 1, 1);
                    sceVif1PkAddUpkData128N((XglPacket *)p, &local, 4);
                    sceVif1PkCloseUpkCode((XglPacket *)p);
                    pxyDrawWork->packet->renderStatus |= 0x20000000;
                    pxyUploadBoneMatrices();
                    if ((pxyDrawWork->batch->flags & 0x40000000) || pxyDrawWork->lastPacket != pxyDrawWork->packet) {
                        pxyUploadViewMatrices();
                    }
                    pxyDrawPartData();
                    pxyDrawWork->packet->renderStatus &= ~0x20000000;
                    pxyDrawWork->lastPacket = pxyDrawWork->packet;
                }
            }
            pxyDrawWork->batch->packet->renderStatus &= ~0x20000000;
            if (pxyShadowCasterMode == 1) {
                break;
            }
        }
        if ((pxyFlags & 0x10) && (packet->drawMode == 1 || packet->drawMode == 4)) {
            pxyDrawWork->batch->packet->renderStatus &= ~0x4000;
        }
    }
    nmlPacketClrTextureCache();
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    pxyDrawPass = savedPass;
    pxySetDrawPass(pxyDrawPass);
}

void pxyBuildFadeRamp(float f);

void pxyDrawCircleShadows(int frameBase)
{
    int i;
    u64 *p;

    nmlPacketGsInit();
    {
        u64 *start;

        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
        start = p;
        GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
        GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
        xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
    }
    nmlPacketAddGsFlush();
    for (i = 0; i < pxyShadowBatchCount; i++) {
        PxyBatch *batch = pxyState.shadowBatches[i];

        if (batch->packet->renderStatus & 4) {
            batch = batch->translucent;
        }
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == batch->packet->camera)
              && batch->packet->cameraMask[pxyCurrentCamera] != 0 && batch->packet->drawMode == 2)) {
            continue;
        }
        {
            pxyBuildFadeRamp(batch->packet->glowIntensity * batch->packet->glowScale);
            nmlPacketSendCircleTexture(pxyCircleShadowTexture, s_aCircleShadowClut);
            nmlPacketMakeCircleTexture(batch->packet);
        }
    }
}


/* Draws the shadow caster batches of the current camera, then their glow, at frameBase. */
void pxyDrawShadowPass(u32 frameBase)
{
    int i;
    int savedPass;
    int translucent;
    int frameBuffer;

    savedPass = pxyDrawPass;
    pxyDrawWork->frameBase = frameBase;
    pxyDrawWork->unk28 = 0;
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    for (i = 0; i < pxyShadowBatchCount; i++) {
        translucent = 0;
        frameBuffer = sRenderTextureBase;
        pxyDrawWork->batch = pxyState.shadowBatches[i];
        pxyDrawWork->lastPacket = pxyDrawWork->batch->packet;
        if (pxyDrawWork->batch->packet->renderStatus & 4) {
            translucent = 1;
            pxyDrawWork->batch = pxyDrawWork->batch->translucent;
        }
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == pxyDrawWork->batch->packet->camera)
              && (pxyDrawWork->batch->packet->renderStatus & 2)
              && pxyDrawWork->batch->packet->cameraMask[pxyCurrentCamera] != 0
              && (pxyDrawWork->batch->packet->drawMode == 1 || pxyDrawWork->batch->packet->drawMode == 5
                  || pxyDrawWork->batch->packet->drawMode == 4))) {
            continue;
        }
        if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        pxyDrawPass = translucent;
        pxySetDrawPass(pxyDrawPass);
        {
            u64 *p;
            u64 *q;

            nmlPacketGsInit();
            p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
            q = p;
            GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
            GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBuffer, cameraScreenWidth / 64, sRenderPsm, 0));
            GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(frameBuffer, cameraScreenWidth / 64, sRenderPsm, 0xFFFFFFFF));
            xglDirectQueueCount += ((u32)p - (u32)q) >> 4;
            nmlPacketAddGsFlush();
        }
        {
            u64 *p;
            u64 *q;

            nmlPacketGsInit();
            p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
            q = p;
            GS_PUT(p, GS_SCISSOR_1, GS_SET_SCISSOR(0, cameraScreenWidth - 1, 0, 0xFF));
            xglDirectQueueCount += ((u32)p - (u32)q) >> 4;
            nmlPacketAddGsFlush();
        }
        pxyFillFrameAlphaMsbStrip(frameBuffer);
        pxyDrawWork->batch->packet->renderStatus |= 0x10000000;
        nmlPacketClrModelCache();
        pxyUploadPacketState(pxyDrawWork->batch->packet->renderStatus & 4, pxyDrawWork->batch->packet);
        nmlPacketAddTransMicrocode(pxyDrawWork->batch->packet);
        pxyDrawWork->entry = &pxyDrawEntryTable[pxyDrawWork->batch->start];
        for (pxyDrawWork->i = 0; pxyDrawWork->i < pxyDrawWork->batch->count; pxyDrawWork->i++, pxyDrawWork->entry++) {
            pxyDrawWork->packet = pxyDrawWork->entry->packet;
            pxyDrawWork->part = pxyDrawWork->entry->part;
            pxyDrawWork->packet->renderStatus |= 0x10000000;
            pxyUploadBoneMatrices();
            if ((pxyDrawWork->batch->flags & 0x40000000) || pxyDrawWork->lastPacket != pxyDrawWork->packet) {
                pxyUploadViewMatrices();
            }
            pxyDrawPartData();
            pxyDrawWork->packet->renderStatus &= ~0x10000000;
            pxyDrawWork->lastPacket = pxyDrawWork->packet;
        }
        pxyDrawWork->batch->packet->renderStatus &= ~0x10000000;
        pxyDrawGlow(frameBuffer, pxyDrawWork->batch->packet);
        xglStudioSetScissor();
        pxyDrawShadowCaster(i, pxyDrawWork->frameBase);
    }
    nmlPacketClrTextureCache();
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    pxyDrawPass = savedPass;
    pxySetDrawPass(pxyDrawPass);
}


void pxyDrawMode3Pass(u32 frameBase)
{
    int batchIndex;
    int started;
    int savedPass;
    u64 *cursor;

    started = 0;
    savedPass = pxyDrawPass;
    pxyDrawWork->frameBase = frameBase;
    pxyDrawWork->unk28 = 0;
    {
        u64 *start;

        nmlPacketGsInit();
        cursor = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
        start = cursor;
        GS_PUT(cursor, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
        GS_PUT(cursor, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
        xglDirectQueueCount += ((u32)cursor - (u32)start) >> 4;
        nmlPacketAddGsFlush();
    }
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    for (batchIndex = 0; batchIndex < pxyShadowBatchCount; batchIndex++) {
        int translucent;

        translucent = 0;
        pxyDrawWork->batch = pxyState.shadowBatches[batchIndex];
        pxyDrawWork->lastPacket = pxyDrawWork->batch->packet;
        if (pxyDrawWork->lastPacket->renderStatus & 4) {
            translucent = 1;
            pxyDrawWork->batch = pxyDrawWork->batch->translucent;
        }
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == pxyDrawWork->batch->packet->camera)
              && pxyDrawWork->batch->packet->cameraMask[pxyCurrentCamera] != 0
              && pxyDrawWork->batch->packet->drawMode == 3
              && pxyDrawWork->batch->packet->shadowMatrix != 0)) {
            continue;
        }
        if (!started) {
            u64 *start;

            pxyFillFrameAlpha80(pxyDrawWork->batch->packet->frameMask, pxyDrawWork->frameBase);
            nmlPacketGsInit();
            cursor = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
            start = cursor;
            GS_PUT(cursor, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0x00FFFFFF));
            GS_PUT(cursor, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFFFFFFFF));
            xglDirectQueueCount += ((u32)cursor - (u32)start) >> 4;
            nmlPacketAddGsFlush();
            started = 1;
        }
        {
            u64 *writePtr;

            nmlPacketGsInit();
            writePtr = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
            cursor = writePtr;
            GS_PUT(writePtr, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
            xglDirectQueueCount += ((u32)writePtr - (u32)cursor) >> 4;
            nmlPacketAddGsFlush();
        }
        pxyDrawWork->batch->packet->renderStatus |= 0x01000000;
        pxySetDrawPass(0);
        nmlPacketClrModelCache();
        pxyUploadPacketState(pxyDrawWork->batch->packet->renderStatus & 0x01000000, pxyDrawWork->batch->packet);
        nmlPacketAddTransMicrocode(pxyDrawWork->batch->packet);
        pxyDrawPass = translucent;
        pxySetDrawPass(pxyDrawPass);
        pxyDrawWork->entry = &pxyDrawEntryTable[pxyDrawWork->batch->start];
        for (pxyDrawWork->i = 0; pxyDrawWork->i < pxyDrawWork->batch->count; pxyDrawWork->i++, pxyDrawWork->entry++) {
            pxyDrawWork->packet = pxyDrawWork->entry->packet;
            pxyDrawWork->part = pxyDrawWork->entry->part;
            if (pxyDrawWork->packet->shadowMatrix == 0) {
                continue;
            }
            pxyDrawWork->packet->renderStatus |= 0x01000000;
            pxyUploadBoneMatrices();
            if ((pxyDrawWork->batch->flags & 0x40000000) || pxyDrawWork->lastPacket != pxyDrawWork->packet) {
                pxyUploadViewMatrices();
            }
            pxyDrawPartData();
            pxyDrawWork->packet->renderStatus &= 0xFEFFFFFF;
            pxyDrawWork->lastPacket = pxyDrawWork->packet;
        }
        pxyDrawWork->batch->packet->renderStatus &= 0xFEFFFFFF;
    }
    if (started) {
        u64 *writePtr;
        u64 *start;

        nmlPacketGsInit();
        writePtr = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
        start = writePtr;
        GS_PUT(writePtr, GS_FBA_1, 0);
        GS_PUT(writePtr, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
        GS_PUT(writePtr, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
        xglDirectQueueCount += ((u32)writePtr - (u32)start) >> 4;
        nmlPacketAddGsFlush();
        pxyModulateFrameByAlpha(pxyDrawWork->frameBase, 1.0f);
    }
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    pxyDrawPass = savedPass;
    pxySetDrawPass(pxyDrawPass);
}

extern u16 sRenderDisplayBase;

/* Writes the GS prelude and frame state of batch into the direct queue, then the entries of the batch. */
void pxyDrawScreenSampledParts(PxyBatch *batch, u32 frameBase)
{
    u64 *p;
    u64 *q;

    pxyDrawWork->cameraBit = 1 << pxyCurrentCamera;
    pxyDrawWork->batch = batch;
    pxyDrawWork->frameBase = frameBase;
    pxyDrawWork->unk28 = 0;
    if (!(pxyDrawWork->batch->packet->header->flags & 0x400)) {
        return;
    }
    nmlPacketGsInit();
    p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    q = p;
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(sRenderTextureBase, cameraScreenWidth / 64, sRenderPsm, 0));
    GS_PUT(p, GS_TEXFLUSH, 0);
    GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    GS_PUT(p, GS_TEX0_1, GS_SET_TEX0(sRenderDisplayBase << 5, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 1));
    GS_PUT(p, GS_CLAMP_1, 0);
    GS_PUT(p, GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x3F800000));
    GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 0, 0, 1, 0, 0));
    GS_PUT(p, GS_UV, 0);
    GS_PUT(p, GS_XYZF2, GS_SET_XYZ(0x800 << 4, (0x800 - (s16)(cameraScreenHeight / 2)) << 4, 0));
    GS_PUT(p, GS_UV, GS_SET_UV(cameraScreenWidth << 4, cameraScreenHeight << 4));
    GS_PUT(p, GS_XYZF2, GS_SET_XYZ(0x900 << 4, (0x900 - (s16)(cameraScreenHeight / 2)) << 4, 0));
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(sRenderDisplayBase, cameraScreenWidth / 64, sRenderPsm, 0));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
    xglDirectQueueCount += ((u32)p - (u32)q) >> 4;
    nmlPacketAddGsFlushWide();
    nmlPacketClrMatrixCache();
    nmlPacketClrModelCache();

    nmlPacketGsInit();
    q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm,
                                       (u32)pxyDrawWork->batch->packet->frameMask));
    GS_PUT(q, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm,
                                       pxyDrawWork->batch->packet->frameMask | 0xFF000000));
    xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
    nmlPacketAddGsFlush();
    pxyUploadPacketState(0x1000, pxyDrawWork->batch->packet);
    nmlPacketAddTransMicrocode(pxyDrawWork->batch->packet);
    pxyDrawWork->entry = &pxyDrawEntryTable[pxyDrawWork->batch->start];
    pxyUploadScreenSampleParams();
    for (pxyDrawWork->i = 0; pxyDrawWork->i < pxyDrawWork->batch->count;
         pxyDrawWork->i++, pxyDrawWork->entry++) {
        pxyDrawWork->packet = pxyDrawWork->entry->packet;
        pxyDrawWork->part = pxyDrawWork->entry->part;
        if (!(pxyDrawWork->part->flags & 0x8000)) {
            continue;
        }
        if (!(pxyDrawWork->packet->partFlags[pxyDrawWork->part->index] & pxyDrawWork->cameraBit)) {
            continue;
        }
        pxyUploadBoneMatrices();
        pxyDrawPartData();
    }

    nmlPacketGsInit();
    q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
    GS_PUT(q, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
    xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
    nmlPacketAddGsFlush();
    nmlPacketClrModelCache();
    nmlPacketClrTextureCache();
}


void pxyDrawZwriteParts(PxyBatch *batch, u32 frameBase)
{
    u64 *p;
    u64 *q;

    pxyDrawWork->cameraBit = 1 << pxyCurrentCamera;
    pxyDrawWork->lastPacket = 0;
    pxyDrawWork->batch = batch;
    pxyDrawWork->frameBase = frameBase;
    pxyDrawWork->batchFlags = pxyDrawWork->batch->flags;
    pxyDrawWork->unk28 = 0;
    if (pxyDrawWork->batchFlags & 0x08000008) {
        if (pxyDrawPass == 1) {
            nmlPacketClrModelCache();
            nmlPacketGsInit();
            p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
            q = p;
            GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, -1));
            GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, -1));
            xglDirectQueueCount += ((u32)p - (u32)q) >> 4;
            nmlPacketAddGsFlush();
            pxyUploadPacketState(8, pxyDrawWork->batch->packet);
            nmlPacketAddTransMicrocode(pxyDrawWork->batch->packet);
            pxyDrawWork->entry = &pxyDrawEntryTable[pxyDrawWork->batch->start];
            pxyDrawWork->i = 0;
            for (; pxyDrawWork->i < pxyDrawWork->batch->count; pxyDrawWork->i++, pxyDrawWork->entry++) {
                pxyDrawWork->packet = pxyDrawWork->entry->packet;
                pxyDrawWork->part = pxyDrawWork->entry->part;
                if (!(pxyDrawWork->packet->partFlags[pxyDrawWork->part->index] & pxyDrawWork->cameraBit)) {
                    continue;
                }
                if (!(pxyDrawWork->packet->renderStatus & 0x8) && !(pxyDrawWork->part->flags & 0x2)) {
                    continue;
                }
                if (!(pxyFlags & 0x40)) {
                    nmlPacketAddTexture();
                }
                pxyUploadBoneMatrices();
                if ((pxyDrawWork->batchFlags & 0x40000000) || pxyDrawWork->lastPacket != pxyDrawWork->packet) {
                    pxyUploadViewMatrices();
                }
                if (pxyDrawWork->batchFlags & 0x80000000) {
                    pxyUploadRenderFlags(pxyDrawWork->packet->renderStatus & 4);
                }
                pxyDrawPartData();
                pxyDrawWork->lastPacket = pxyDrawWork->packet;
            }

            nmlPacketGsInit();
            q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
            p = q;
            GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
            GS_PUT(q, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
            xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
            nmlPacketAddGsFlush();
            nmlPacketClrModelCache();
        }
    }
}

void pxyDrawFrameMaskParts(PxyBatch *batch, u32 frameBase)
{
    u64 *cursor;
    u64 *start;

    pxyDrawWork->cameraBit = 1 << pxyCurrentCamera;
    pxyDrawWork->lastPacket = 0;
    pxyDrawWork->batch = batch;
    pxyDrawWork->frameBase = frameBase;
    pxyDrawWork->unk28 = 0;
    if (!(pxyFlags & 0x800)) {
        return;
    }
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    nmlPacketGsInit();
    cursor = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    start = cursor;
    GS_PUT(cursor, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFFFFFFFF));
    GS_PUT(cursor, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFFFFFFFF));
    xglDirectQueueCount += ((u32)cursor - (u32)start) >> 4;
    nmlPacketAddGsFlush();
    pxySetAlphaRef(64);
    pxyUploadPacketState(pxyDrawWork->batch->packet->renderStatus & 4, pxyDrawWork->batch->packet);
    nmlPacketAddTransMicrocode(pxyDrawWork->batch->packet);
    pxyDrawWork->entry = &pxyDrawEntryTable[pxyDrawWork->batch->start];
    for (pxyDrawWork->i = 0; pxyDrawWork->i < pxyDrawWork->batch->count; pxyDrawWork->i++, pxyDrawWork->entry++) {
        pxyDrawWork->packet = pxyDrawWork->entry->packet;
        pxyDrawWork->part = pxyDrawWork->entry->part;
        if (!(pxyDrawWork->packet->partFlags[pxyDrawWork->part->index] & pxyDrawWork->cameraBit)) {
            continue;
        }
        if (!(pxyDrawWork->part->flags & 0x10)) {
            continue;
        }
        if (pxyDrawWork->part->flags & 0x800) {
            continue;
        }
        if (!(pxyFlags & 0x40)) {
            nmlPacketAddTexture();
        }
        pxyUploadBoneMatrices();
        if ((pxyDrawWork->batch->flags & 0x40000000) || pxyDrawWork->lastPacket != pxyDrawWork->packet) {
            pxyUploadViewMatrices();
        }
        if (pxyDrawWork->batch->flags & 0x80000000) {
            pxyUploadRenderFlags(pxyDrawWork->packet->renderStatus & 4);
        }
        pxyUploadPartState();
        pxyDrawPartData();
        pxyDrawWork->lastPacket = pxyDrawWork->packet;
    }
    nmlPacketGsInit();
    start = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    cursor = start;
    GS_PUT(start, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
    GS_PUT(start, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
    xglDirectQueueCount += ((u32)start - (u32)cursor) >> 4;
    nmlPacketAddGsFlush();
    pxySetAlphaRef(117);
    nmlPacketClrMatrixCache();
}

extern int pxyAlphaCorrectionFlag;

/* Draws the entries of `batch` in draw mode `mode` into the frame at frameBase; `frameMask`
 * is or-ed into the FRAME write mask of the batch. A part is drawn only in the modes its flags
 * select: flag 4 parts in mode 1 and only there, flag 0x2000 parts in mode 2, and parts that
 * are hidden-capable (packet 0x20000 or part 0x10000) in mode 4; mode 3 stops at the first
 * entry unless the batch has flag 0x100000. Parts with flag 0x4000 write their own FRAME masks. */
void pxyDrawBatch(int mode, PxyBatch *batch, u32 frameBase, int frameMask)
{
    u64 *p;
    u64 *q;

    pxyDrawWork->cameraBit = 1 << pxyCurrentCamera;
    pxyDrawWork->lastPacket = 0;
    pxyDrawWork->batch = batch;
    pxyDrawWork->mode = mode;
    pxyDrawWork->frameBase = frameBase;
    pxyDrawWork->frameMask = frameMask;
    pxyDrawWork->unk28 = 0;
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    nmlPacketGsInit();
    p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    q = p;
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm,
                                       pxyDrawWork->frameMask | pxyDrawWork->batch->packet->frameMask));
    GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm,
                                       pxyDrawWork->frameMask | pxyDrawWork->batch->packet->frameMask | 0xFF000000));
    xglDirectQueueCount += ((u32)p - (u32)q) >> 4;
    nmlPacketAddGsFlush();
    pxyUploadPacketState(pxyDrawWork->batch->packet->renderStatus & 4, pxyDrawWork->batch->packet);
    nmlPacketAddTransMicrocode(pxyDrawWork->batch->packet);
    if (pxyDrawWork->batch->flags & 0x6920000) {
        pxyDrawWork->entry = &pxyDrawEntryTable[pxyDrawWork->batch->start];
        pxyDrawWork->count = pxyDrawWork->batch->count;
        pxyDrawWork->batchFlags = pxyDrawWork->batch->flags;
        pxyDrawWork->mode = mode;
        for (pxyDrawWork->i = 0; pxyDrawWork->i < pxyDrawWork->count; pxyDrawWork->i++, pxyDrawWork->entry++) {
            pxyDrawWork->unk28 = 0;
            pxyDrawWork->packet = pxyDrawWork->entry->packet;
            pxyDrawWork->part = pxyDrawWork->entry->part;
            pxyDrawWork->partFlags = pxyDrawWork->part->flags;
            pxyDrawWork->packetFlags = pxyDrawWork->packet->renderStatus;
            if (!(pxyDrawWork->packet->partFlags[pxyDrawWork->part->index] & pxyDrawWork->cameraBit)) {
                continue;
            }
            if ((pxyDrawWork->mode == 1) ^ ((pxyDrawWork->partFlags & 4) != 0)) {
                continue;
            }
            if ((pxyDrawWork->mode == 2) ^ ((pxyDrawWork->partFlags & 0x2000) != 0)) {
                continue;
            }
            if (pxyDrawWork->mode == 3 && !(pxyDrawWork->batchFlags & 0x100000)) {
                break;
            }
            if (pxyDrawWork->packet->hideMask != 0) {
                if (pxyDrawWork->mode != 4
                    && !(pxyDrawWork->packet->hideMask->bits[(s16)(pxyDrawWork->part->matrixIndex >> 5)]
                         & (1 << (s16)(pxyDrawWork->part->matrixIndex & 0x1F)))
                    && ((pxyDrawWork->packetFlags & 0x20000) || (pxyDrawWork->partFlags & 0x10000))) {
                    continue;
                }
                if (pxyDrawWork->mode == 4
                    && ((pxyDrawWork->packet->hideMask->bits[(s16)(pxyDrawWork->part->matrixIndex >> 5)]
                         & (1 << (s16)(pxyDrawWork->part->matrixIndex & 0x1F)))
                        || !((pxyDrawWork->packetFlags & 0x20000) || (pxyDrawWork->partFlags & 0x10000)))) {
                    continue;
                }
            } else if ((pxyDrawWork->mode == 4)
                       ^ (((pxyDrawWork->packetFlags & 0x20000) || (pxyDrawWork->partFlags & 0x10000)) != 0)) {
                continue;
            }
            if (pxyDrawPass != 0 && (pxyDrawWork->partFlags & 0x80)) {
                pxyClearFrameAlphaMsb(pxyDrawWork->batch->packet->frameMask, pxyDrawWork->frameBase);
            }
            if (pxyDrawWork->partFlags & 0x4000) {
                if (!(pxyDrawWork->partFlags & 0x1000)) {
                    nmlPacketGsInit();
                    q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                    p = q;
                    GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFFFFFFFF));
                    GS_PUT(q, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFFFFFFFF));
                    xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
                    nmlPacketAddGsFlush();
                    pxyDrawWork->unk28 = 1;
                } else {
                    nmlPacketGsInit();
                    q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                    p = q;
                    GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
                    GS_PUT(q, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFFFFFFFF));
                    xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
                    nmlPacketAddGsFlush();
                }
            } else if ((pxyDrawWork->partFlags & 0x200) && (pxyDrawWork->partFlags & 0x1000)) {
                continue;
            }
            if (pxyDrawWork->partFlags & 0x40) {
                nmlPacketGsInit();
                q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                p = q;
                GS_PUT(q, GS_FBA_1, 1);
                xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
                nmlPacketAddGsFlush();
            } else {
                nmlPacketGsInit();
                q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                p = q;
                GS_PUT(q, GS_FBA_1, pxyAlphaCorrectionFlag);
                xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
                nmlPacketAddGsFlush();
            }
            if (!(pxyFlags & 0x40)) {
                nmlPacketAddTexture();
            }
            pxyUploadBoneMatrices();
            if ((pxyDrawWork->batchFlags & 0x40000000) || pxyDrawWork->lastPacket != pxyDrawWork->packet) {
                pxyUploadViewMatrices();
            }
            if (pxyDrawWork->batchFlags & 0x80000000) {
                pxyUploadRenderFlags(pxyDrawWork->packetFlags & 4);
            }
            pxyUploadPartState();
            pxyDrawPartData();
            if (pxyDrawWork->partFlags & 0x4000) {
                nmlPacketGsInit();
                q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                p = q;
                GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm,
                                                   (u32)pxyDrawWork->batch->packet->frameMask));
                GS_PUT(q, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm,
                                                   pxyDrawWork->batch->packet->frameMask | 0xFF000000));
                xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
                nmlPacketAddGsFlush();
            }
            if (pxyDrawPass != 0 && (pxyDrawWork->partFlags & 0x100)) {
                pxyClearDepthByFrameAlpha(pxyDrawWork->batch->packet->frameMask, pxyDrawWork->frameBase);
            }
            pxyDrawWork->lastPacket = pxyDrawWork->packet;
        }
    } else {
        pxyDrawWork->entry = &pxyDrawEntryTable[pxyDrawWork->batch->start];
        pxyDrawWork->count = pxyDrawWork->batch->count;
        pxyDrawWork->batchFlags = pxyDrawWork->batch->flags;
        if (pxyDrawWork->batchFlags & 0xC0400000) {
            for (pxyDrawWork->i = 0; pxyDrawWork->i < pxyDrawWork->count; pxyDrawWork->i++, pxyDrawWork->entry++) {
                pxyDrawWork->packet = pxyDrawWork->entry->packet;
                pxyDrawWork->part = pxyDrawWork->entry->part;
                if (!(pxyDrawWork->packet->partFlags[pxyDrawWork->part->index] & pxyDrawWork->cameraBit)) {
                    continue;
                }
                if (!(pxyFlags & 0x40)) {
                    nmlPacketAddTexture();
                }
                pxyUploadBoneMatrices();
                if ((pxyDrawWork->batchFlags & 0x40000000) || pxyDrawWork->lastPacket != pxyDrawWork->packet) {
                    pxyUploadViewMatrices();
                }
                if (pxyDrawWork->batchFlags & 0x80000000) {
                    pxyUploadRenderFlags(pxyDrawWork->packet->renderStatus & 4);
                }
                pxyUploadPartState();
                pxyDrawPartData();
                pxyDrawWork->lastPacket = pxyDrawWork->packet;
            }
        } else {
            for (pxyDrawWork->i = 0; pxyDrawWork->i < pxyDrawWork->count; pxyDrawWork->i++, pxyDrawWork->entry++) {
                pxyDrawWork->packet = pxyDrawWork->entry->packet;
                pxyDrawWork->part = pxyDrawWork->entry->part;
                if (!(pxyDrawWork->packet->partFlags[pxyDrawWork->part->index] & pxyDrawWork->cameraBit)) {
                    continue;
                }
                if (!(pxyFlags & 0x40)) {
                    nmlPacketAddTexture();
                }
                pxyUploadBoneMatrices();
                pxyUploadPartState();
                pxyDrawPartData();
            }
        }
    }
    nmlPacketGsInit();
    q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
    GS_PUT(q, GS_FRAME_2, GS_SET_FRAME(pxyDrawWork->frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
    xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
    nmlPacketAddGsFlush();
    nmlPacketClrMatrixCache();
}

/* Half the screen, in pixels, as the GS offsets use it. */
#define SCREEN_HALF_W ((s16)(cameraScreenWidth / 2))
#define SCREEN_HALF_H ((s16)(cameraScreenHeight / 2))

void pxyPlayEntryGroupFiltered(int kind, int sub);
void pxyDrawBatch(int, PxyBatch *, u32, int);

void pxyDrawZwriteParts(PxyBatch *batch, u32 frameBase);
/* Writes the GS prelude and frame state of batch into the direct queue, then the entries of the batch. */ void pxyDrawScreenSampledParts(PxyBatch *batch, u32 frameBase);
void pxyDrawFrameMaskParts(PxyBatch *batch, u32 frameBase);
/* Draws the shadow caster batches of the current camera, then their glow, at frameBase. */ void pxyDrawShadowPass(u32 frameBase);
void pxyDrawCircleShadows(int frameBase);
void pxyDrawMode3Pass(u32 frameBase);

/* Draws the overlay and then the sorted batches of the current camera in draw mode `mode`, then shadow casters and the translucent pass. */
void pxyDrawBatches(int mode, u32 frameBase)
{
    PxyBatchTable *table;
    int i;
    PxyBatch *batch;

    table = 0;
    batch = 0;
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    pxyDrawPass = 0;
    pxySetDrawPass(pxyDrawPass);
    for (i = 0; i < pxyOverlayBatchCount; i++) {
        batch = pxyState.overlayBatches[i];
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == batch->packet->camera)
              && batch->packet->cameraMask[pxyCurrentCamera] != 0)
            || (mode == 4 && !(batch->packet->renderStatus & 0x20000) && !(batch->flags & 0x20000))
            || (mode == 1 && !(batch->flags & 0x4000000)) || (mode == 2 && !(batch->flags & 0x2000000))) {
            continue;
        }
        if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        if (mode == 0 && batch->packet->preDraw != 0) {
            batch->packet->preDraw(pxyCurrentCamera, pxyDrawPass);
        }
        pxyDrawBatch(mode, batch, frameBase, 0);
        if (mode == 0 && batch->packet->postDraw != 0) {
            batch->packet->postDraw(pxyCurrentCamera, pxyDrawPass);
        }
    }
    if (mode == 0) {
        pxyPlayEntryGroupFiltered(4, pxyCurrentCamera);
    }
    table = &pxyBatchTables[pxyDrawPass];
    batch = pxyFirstBatch;
    while (pxyFirstBatch != 0) {
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == batch->packet->camera)
              && batch->packet->cameraMask[pxyCurrentCamera] != 0 && !(batch->packet->renderStatus & 0x10))
            || (mode == 4 && !(batch->packet->renderStatus & 0x20000) && !(batch->flags & 0x20000))
            || (mode == 1 && !(batch->flags & 0x4000000)) || (mode == 2 && !(batch->flags & 0x2000000))) {
            if (batch->next < 0) {
                break;
            }
            batch = &table->batches[batch->next];
            continue;
        }
        if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        if (mode == 0 && batch->packet->preDraw != 0) {
            batch->packet->preDraw(pxyCurrentCamera, pxyDrawPass);
        }
        if (batch->packet->renderStatus & 0x80000) {
            u64 *p;
            u64 *start;

            pxyClearFrameAlphaMsb(batch->packet->frameMask, frameBase);
            nmlPacketGsInit();
            p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
            start = p;
            GS_PUT(p, GS_FBA_1, 1);
            xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
            nmlPacketAddGsFlush();
        }
        pxyDrawBatch(mode, batch, frameBase, 0);
        if (batch->packet->renderStatus & 0x80000) {
            int y;
            int color[4];

            color[0] = 0x80;
            color[1] = 0x80;
            color[2] = 0x80;
            color[3] = 0x80;
            {
                u64 *p;
                u64 *start;

                nmlPacketGsInit();
                p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                start = p;
                GS_PUT(p, GS_FBA_1, 0);
                xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
                nmlPacketAddGsFlush();
            }
            for (y = 0; y < cameraScreenHeight; y += SCREEN_HALF_H) {
                int pass;

                {
                    u64 *p;
                    u64 *start;

                    nmlPacketGsInit();
                    p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                    start = p;
                    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(sRenderTextureBase, cameraScreenWidth / 64, sRenderPsm, 0));
                    GS_PUT(p, GS_TEXFLUSH, 0);
                    GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
                    GS_PUT(p, GS_CLAMP_1, 0);
                    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0, 0x3F800000));
                    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
                    GS_PUT(p, GS_TEX0_1, GS_SET_TEX0(frameBase << 5, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 1));
                    GS_PUT(p, GS_TEX1_1, 0);
                    GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 0, 0, 1, 0, 0));
                    GS_PUT(p, GS_UV, GS_SET_UV(0, (y << 4)));
                    GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((0x800 - SCREEN_HALF_W) << 4), ((0x800 - SCREEN_HALF_H) << 4), 0));
                    GS_PUT(p, GS_UV, GS_SET_UV((cameraScreenWidth << 4), ((SCREEN_HALF_H + y) << 4)));
                    GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((cameraScreenWidth - SCREEN_HALF_W + 0x7FF) << 4), (0x7FF << 4), 0));
                    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(color[0], color[1], color[2], color[3], 0x3F800000));
                    GS_PUT(p, GS_TEST_1, GS_SET_TEST(1, 5, 0x80, 0, 0, 0, 1, 1));
                    GS_PUT(p, GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
                    GS_PUT(p, GS_FBA_1, 1);
                    xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
                    nmlPacketAddGsFlushWide();
                }
                for (pass = 0; pass < 1; pass++) {
                    int alphas[2] = { 40, 48 };
                    int shifts[2] = { 2, 1 };
                    int alpha;
                    int shift;

                    alpha = alphas[pass];
                    shift = shifts[pass];
                    {
                        u64 *p;
                        u64 *start;

                        nmlPacketGsInit();
                        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                        start = p;
                        GS_PUT(p, GS_ALPHA_1, GS_SET_ALPHA(0, 1, 2, 1, alpha));
                        GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0));
                        GS_PUT(p, GS_UV, GS_SET_UV(9, (y << 4) + 9));
                        GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((shift - SCREEN_HALF_W + 0x800) << 4), ((0x800 - SCREEN_HALF_H) << 4), 0));
                        GS_PUT(p, GS_UV, GS_SET_UV(((cameraScreenWidth - shift) << 4) + 9, ((SCREEN_HALF_H + y) << 4) + 9));
                        GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((cameraScreenWidth - SCREEN_HALF_W + 0x800) << 4), 0x8000, 0));
                        xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
                        nmlPacketAddGsFlushWide();
                    }
                    {
                        u64 *p;
                        u64 *start;

                        nmlPacketGsInit();
                        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                        start = p;
                        GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0));
                        GS_PUT(p, GS_UV, GS_SET_UV((shift << 4) + 9, (y << 4) + 9));
                        GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((0x800 - SCREEN_HALF_W) << 4), ((0x800 - SCREEN_HALF_H) << 4), 0));
                        GS_PUT(p, GS_UV, GS_SET_UV((cameraScreenWidth << 4) + 9, ((SCREEN_HALF_H + y) << 4) + 9));
                        GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((cameraScreenWidth - SCREEN_HALF_W - shift + 0x800) << 4), 0x8000, 0));
                        xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
                        nmlPacketAddGsFlushWide();
                    }
                    {
                        u64 *p;
                        u64 *start;

                        nmlPacketGsInit();
                        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                        start = p;
                        GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0));
                        GS_PUT(p, GS_UV, GS_SET_UV(8, ((shift + y) << 4) + 9));
                        GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((0x800 - SCREEN_HALF_W) << 4), ((0x800 - SCREEN_HALF_H) << 4), 0));
                        GS_PUT(p, GS_UV, GS_SET_UV((cameraScreenWidth << 4) + 8, ((SCREEN_HALF_H + y) << 4) + 9));
                        GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((cameraScreenWidth - SCREEN_HALF_W + 0x800) << 4), ((0x800 - shift) << 4), 0));
                        xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
                        nmlPacketAddGsFlushWide();
                    }
                    {
                        u64 *p;
                        u64 *start;

                        nmlPacketGsInit();
                        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                        start = p;
                        GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0));
                        GS_PUT(p, GS_UV, GS_SET_UV(8, (y << 4) + 9));
                        GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((0x800 - SCREEN_HALF_W) << 4), ((shift - SCREEN_HALF_H + 0x800) << 4), 0));
                        GS_PUT(p, GS_UV, GS_SET_UV((cameraScreenWidth << 4) + 8, ((SCREEN_HALF_H + y - shift) << 4) + 9));
                        GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((cameraScreenWidth - SCREEN_HALF_W + 0x800) << 4), 0x8000, 0));
                        xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
                        nmlPacketAddGsFlushWide();
                    }
                }
                {
                    u64 *p;
                    u64 *start;

                    nmlPacketGsInit();
                    p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                    start = p;
                    GS_PUT(p, GS_FBA_1, 0);
                    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x3F800000));
                    GS_PUT(p, GS_TEST_1, GS_SET_TEST(1, 5, 0x80, 0, 0, 0, 1, 1));
                    GS_PUT(p, GS_TEX1_1, 0);
                    GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 0, 0, 1, 0, 0));
                    GS_PUT(p, GS_UV, GS_SET_UV(0, (y << 4)));
                    GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((0x800 - SCREEN_HALF_W) << 4), ((0x800 - SCREEN_HALF_H) << 4), 0));
                    GS_PUT(p, GS_UV, GS_SET_UV((cameraScreenWidth << 4), ((SCREEN_HALF_H + y) << 4)));
                    GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((cameraScreenWidth - SCREEN_HALF_W + 0x7FF) << 4), (0x7FF << 4), 0));
                    xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
                    nmlPacketAddGsFlushWide();
                }
                {
                    u64 *p;
                    u64 *start;

                    nmlPacketGsInit();
                    p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
                    start = p;
                    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
                    GS_PUT(p, GS_TEXFLUSH, 0);
                    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x3F800000));
                    GS_PUT(p, GS_TEX0_1, GS_SET_TEX0(sRenderTextureBase << 5, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 1));
                    GS_PUT(p, GS_TEST_1, GS_SET_TEST(1, 5, 0x80, 0, 1, 0, 1, 1));
                    GS_PUT(p, GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
                    GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 0, 0, 1, 0, 0));
                    GS_PUT(p, GS_UV, GS_SET_UV(0, 0));
                    GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((0x800 - SCREEN_HALF_W) << 4), ((y - SCREEN_HALF_H + 0x800) << 4), 0));
                    GS_PUT(p, GS_UV, GS_SET_UV((cameraScreenWidth << 4), (SCREEN_HALF_H << 4)));
                    GS_PUT(p, GS_XYZF2, GS_SET_XYZ(((cameraScreenWidth - SCREEN_HALF_W + 0x7FF) << 4), ((y + 0x7FF) << 4), 0));
                    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
                    xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
                    nmlPacketAddGsFlushWide();
                }
            }
            nmlPacketClrTextureCache();
        }
        if (mode == 0 && batch->packet->postDraw != 0) {
            batch->packet->postDraw(pxyCurrentCamera, pxyDrawPass);
        }
        if (batch->next < 0) {
            break;
        }
        batch = &table->batches[batch->next];
    }
    {
        u64 *p;
        u64 *start;

        nmlPacketGsInit();
        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
        start = p;
        GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
        GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
        xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
        nmlPacketAddGsFlush();
    }
    if (mode == 0) {
        pxyPlayEntryGroupFiltered(0, pxyCurrentCamera);
    }
    pxyDrawPass = 1;
    pxySetDrawPass(pxyDrawPass);
    table = &pxyBatchTables[0];
    batch = pxyFirstBatch;
    while (pxyFirstBatch != 0) {
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == batch->packet->camera)
              && batch->packet->cameraMask[pxyCurrentCamera] != 0 && (batch->packet->renderStatus & 0x40000)
              && !(batch->packet->renderStatus & 0x10) && !(batch->packet->unmodeled_D4 & 1))
            || (mode == 4 && !(batch->packet->renderStatus & 0x20000) && !(batch->flags & 0x20000))
            || (mode == 1 && !(batch->flags & 0x4000000)) || (mode == 2 && !(batch->flags & 0x2000000))) {
            if (batch->next < 0) {
                break;
            }
            batch = &table->batches[batch->next];
            continue;
        }
        if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        if (mode == 0 || mode == 2 || mode == 4) {
            pxyDrawZwriteParts(batch->translucent, frameBase);
        }
        if (mode == 0 || mode == 2 || mode == 4) {
            pxyDrawScreenSampledParts(batch->translucent, frameBase);
        }
        pxyDrawBatch(mode, batch->translucent, frameBase, 0);
        if (mode == 0 || mode == 2 || mode == 4) {
            pxyDrawPass = 0;
            pxySetDrawPass(pxyDrawPass);
            pxyDrawFrameMaskParts(batch, frameBase);
            pxyDrawPass = 1;
            pxySetDrawPass(pxyDrawPass);
        }
        if (batch->next < 0) {
            break;
        }
        batch = &table->batches[batch->next];
    }
    if ((pxyFlags & 0x100000) && mode == 0) {
        if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        pxyDrawShadowPass(frameBase);
        pxyDrawCircleShadows(frameBase);
        pxyDrawMode3Pass(frameBase);
    }
    {
        for (i = 0; i < pxyOverlayBatchCount; i++) {
            batch = pxyState.overlayBatches[i];
            if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == batch->packet->camera)
                  && batch->packet->cameraMask[pxyCurrentCamera] != 0)
                || (mode == 4 && !(batch->packet->renderStatus & 0x20000) && !(batch->flags & 0x20000))
                || (mode == 1 && !(batch->flags & 0x4000000)) || (mode == 2 && !(batch->flags & 0x2000000))) {
                continue;
            }
            if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
                return;
            }
            if (mode == 0 || mode == 2 || mode == 4) {
                pxyDrawZwriteParts(batch->translucent, frameBase);
            }
            pxyDrawBatch(mode, batch->translucent, frameBase, 0);
        }
    }
    if (!(pxyFlags & 0x100000) && mode == 0) {
        if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        pxyDrawShadowPass(frameBase);
        pxyDrawCircleShadows(frameBase);
        pxyDrawMode3Pass(frameBase);
    }
    table = &pxyBatchTables[0];
    batch = pxyFirstBatch;
    while (pxyFirstBatch != 0) {
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == batch->packet->camera)
              && batch->packet->cameraMask[pxyCurrentCamera] != 0 && !(batch->packet->renderStatus & 0x40000)
              && !(batch->packet->renderStatus & 0x10) && !(batch->packet->unmodeled_D4 & 1))
            || (mode == 4 && !(batch->packet->renderStatus & 0x20000) && !(batch->flags & 0x20000))
            || (mode == 1 && !(batch->flags & 0x4000000)) || (mode == 2 && !(batch->flags & 0x2000000))) {
            if (batch->next < 0) {
                break;
            }
            batch = &table->batches[batch->next];
            continue;
        }
        if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        if (mode == 0 || mode == 2 || mode == 4) {
            pxyDrawZwriteParts(batch->translucent, frameBase);
        }
        if (mode == 0 || mode == 2 || mode == 4) {
            pxyDrawScreenSampledParts(batch->translucent, frameBase);
        }
        pxyDrawBatch(mode, batch->translucent, frameBase, 0);
        if (mode == 0 || mode == 2 || mode == 4) {
            pxyDrawPass = 0;
            pxySetDrawPass(pxyDrawPass);
            pxyDrawFrameMaskParts(batch, frameBase);
            pxyDrawPass = 1;
            pxySetDrawPass(pxyDrawPass);
        }
        if (batch->next < 0) {
            break;
        }
        batch = &table->batches[batch->next];
    }
    {
        u64 *p;
        u64 *start;

        nmlPacketGsInit();
        p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
        start = p;
        GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
        GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
        xglDirectQueueCount += ((u32)p - (u32)start) >> 4;
        nmlPacketAddGsFlush();
    }
    if (mode == 0) {
        if (xglPacketQueuedPercent() > 85.0f || xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        pxyPlayEntryGroupFiltered(1, pxyCurrentCamera);
    }
}

void pxyDrawSeparateBatches(int mode, u32 frameBase)
{
    int i;
    PxyBatch *batch = 0;
    u64 *p;
    u64 *q;

    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    pxyDrawPass = 0;
    pxySetDrawPass(pxyDrawPass);
    for (i = 0; i < pxySeparateBatchCount; i++) {
        batch = pxyState.unk820[i];
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == batch->packet->camera)
              && batch->packet->cameraMask[pxyCurrentCamera] != 0)) {
            continue;
        }
        if (xglPacketQueuedPercent() > 85.0f) {
            return;
        }
        if (xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        pxyDrawBatch(mode, batch, frameBase, 0);
    }

    nmlPacketGsInit();
    p = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    q = p;
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
    GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
    xglDirectQueueCount += ((u32)p - (u32)q) >> 4;
    nmlPacketAddGsFlush();
    pxyDrawPass = 1;
    pxySetDrawPass(pxyDrawPass);
    for (i = 0; i < pxySeparateBatchCount; i++) {
        batch = pxyState.unk820[i];
        if (!((pxyCameraEnable[pxyCurrentCamera] == 1 || pxyCurrentCamera == batch->packet->camera)
              && batch->packet->cameraMask[pxyCurrentCamera] != 0)) {
            continue;
        }
        if (xglPacketQueuedPercent() > 85.0f) {
            return;
        }
        if (xglPacketBufferUsedPercent() > 85.0f) {
            return;
        }
        pxyDrawZwriteParts(batch->translucent, frameBase);
        pxyDrawScreenSampledParts(batch->translucent, frameBase);
        pxyDrawBatch(mode, batch->translucent, frameBase, 0);
    }

    nmlPacketGsInit();
    q = (u64 *)&xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_FRAME_1, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0));
    GS_PUT(q, GS_FRAME_2, GS_SET_FRAME(frameBase, cameraScreenWidth / 64, sRenderPsm, 0xFF000000));
    xglDirectQueueCount += ((u32)q - (u32)p) >> 4;
    nmlPacketAddGsFlush();
}

extern XglCamera *pxyActiveCamera;
extern u16 sRenderFrameBase;

void nmlModelFlushSub(void)
{
    DoublewordAlignedVector4 rect;
    DoublewordAlignedVector4 vec;
    int i;

    pxyActiveCamera = xglStudioSelectGetActiveCamera(pxyCurrentCamera);
    pxyAlphaCorrectionFlag = 0;
    xglStudioSelect(pxyCurrentCamera);
    nmlPacketClrTextureCache();
    nmlPacketClrModelCache();
    xglPacketModeReset();
    nmlPacketClrMatrixCache();
    if (pxyActiveCamera == 0) {
        return;
    }
    if (pxyCameraEnable[pxyCurrentCamera] == 0) {
        return;
    }
    xglPacketSelectDefaultMode();
    pxyResetGsState();
    if (pxyFlags & 1) {
        vec.x = 0.0f;
        vec.y = 0.0f;
        vec.z = (float)(cameraScreenWidth / 2);
        vec.w = (float)(cameraScreenHeight / 2);
        rect = vec;
        xglStudioSetScissor();
        xglPacketInitVuState();
        nmlPacketAddScreenClear(sRenderFrameBase, 3);
        if (!(pxyFlags & 0x4000)) {
            pxyDrawBatches(1, sRenderFrameBase);
            pxyDrawTextureRect(sRenderFrameBase, &pxyActiveCamera->screen.window, sRenderDisplayBase, &rect);
            pxyDrawTextureRect(sRenderDisplayBase, &rect, sRenderFrameBase, &rect);
        }
        xglSetFullScreenScissor();
    }
    if (pxyFlags & 0x8000) {
        xglStudioSetScissor();
        xglPacketInitVuState();
        if (pxyCurrentCamera != 0 || (pxyFlags & 1)) {
            nmlPacketAddScreenClear(sRenderDisplayBase, 3);
        }
        if (!(pxyFlags & 0x4000)) {
            pxyDrawBatches(4, sRenderDisplayBase);
        }
        if (pxyDrawCallback != 0) {
            pxyDrawCallback(pxyCurrentCamera);
        }
        xglSetFullScreenScissor();
    }
    xglStudioSetScissor();
    xglPacketInitVuState();
    if (!(pxyFlags & 0x8000)) {
        nmlPacketAddScreenClear(sRenderDisplayBase, 11);
    }
    pxyDrawBatches(0, sRenderDisplayBase);
    for (i = 0; i < 16; i++) {
        if (pxyState.drawCallbacks[i] != 0) {
            pxyState.drawCallbacks[i](pxyCurrentCamera);
        }
    }
    if (!(pxyFlags & 0x8000)) {
        if (pxyDrawCallback != 0) {
            pxyDrawCallback(pxyCurrentCamera);
        }
    }
    xglSetFullScreenScissor();
    if (pxyFlags & 0x100) {
        nmlPacketAddScreenClear(sRenderDisplayBase, 8);
        xglStudioSetScissor();
        xglPacketInitVuState();
        pxyDrawSeparateBatches(3, sRenderDisplayBase);
        xglSetFullScreenScissor();
    }
    if (pxyFlags & 0x4000) {
        nmlPacketAddScreenClear(sRenderDisplayBase, 2);
    }
    if (pxyFlags & 4) {
        if (pxyFlags & 0x2000) {
            xglStudioSetScissor();
            xglPacketInitVuState();
            pxyClearFrameAlphaMsb(0, sRenderDisplayBase);
            pxyAlphaCorrectionFlag = 1;
            pxyDrawBatches(2, sRenderDisplayBase);
            xglSetFullScreenScissor();
            pxyAlphaCorrectionFlag = 0;
        } else {
            xglStudioSetScissor();
            xglPacketInitVuState();
            nmlPacketAddScreenClear(sRenderFrameBase, 7);
            if (!(pxyFlags & 0x200)) {
                pxyDrawBatches(2, sRenderFrameBase);
            }
            xglSetFullScreenScissor();
        }
        pxyResetGsState();
    }
}

void pxyPlayEntryGroup(int key);
void nmlModelFlushSub(void);
const char pxyFlushProfileName[24] = "pxyFlush";
extern int pxyFlushSubmitCount;
extern f32 pxyPacketFillPercent;
extern f32 pxyPacketBufferPercent;

void pxyFlush(void) {
    pxyProfileBegin(pxyFlushProfileName);
    pxyResetSubmitCount();
    xglPacketSelectDefaultMode();
    pxySetGlobalColor(&pxyGlobalColor);
    pxySetBlackFogFlag(pxyFlags & 0x80);
    pxyUpdateReflectionRotation(pxyFlags & 0x40);
    xglPacketSetMode3Flag(pxyFlags & 0x10);
    xglPacketSetMode4Flag(pxyFlags & 0x80000);
    pxySetRotateFreeze(pxyFreezeAnimation);
    pxyResetGsState();
    pxyPlayEntryGroup(3);
    if ((pxyBatchedModelCount != 0) || (pxyEntryCount != 0) || (pxyDrawCallbackCount != 0)) {
        for (pxyCurrentCamera = 0; pxyCurrentCamera < 4; pxyCurrentCamera += 1) {
            nmlModelFlushSub();
        }
    }
    pxyPlayEntryGroup(2);
    xglLightPackCacheInvalidate();
    pxyProfileEnd(pxyFlushProfileName);
    if (pxyProfileDrawFlag != 0) {
        pxyProfileDrawEntries();
    }
    pxyProfileClearEntries();
    pxyFlushSubmitCount = pxyGetSubmitCount();
    pxyPacketFillPercent = xglPacketQueuedPercent();
    pxyPacketBufferPercent = xglPacketBufferUsedPercent();
    pxyFlushDoneFlag = 1;
    pxyOpaqueEntryUsagePercent = ((f32) pxyOpaqueEntryCursor / 4096.0f) * 100.0f;
    pxyTranslucentEntryUsagePercent = ((f32) (pxyTranslucentEntryCursor - 0x1000) / 2560.0f) * 100.0f;
    pxySetFlushDone();
}

void pxyInitBatchTables(void);
void pxyInitSlots(void);

/* Reset the pxy state: records, counters and globals, entry table and slots. */
void nmlModelFlushClear(void)
{
    pxyInitSlots();
    pxyResetCounters();
    pxyClearEntryTable();
    pxyInitBatchTables();
}

void xglRenderFrameBeginFlushEIrq(void) {
    nmlPacketAddWaitMicrocodeIrq();
}

void xglRenderFrameEndFlushEIrq(void) {
    nmlPacketAddWaitMicrocodeIrq();
}

int pxyFlushSubmitCountGet(void) {
    return pxyFlushSubmitCount;
}

f32 pxyFlushPacketFillPercentGet(void) {
    return pxyPacketFillPercent;
}

f32 pxyFlushPacketBufferPercentGet(void) {
    return pxyPacketBufferPercent;
}

f32 pxyOpaqueEntryUsagePercentGet(void) {
    return pxyOpaqueEntryUsagePercent;
}

f32 pxyTranslucentEntryUsagePercentGet(void) {
    return pxyTranslucentEntryUsagePercent;
}

extern int pxyFrameCopyRequested;

void pxyFrameCopyRequest(void) {
    pxyFrameCopyRequested = 1;
}

extern u16 sRenderFlipBase;

void pxyAddFrameCopyPacket(XglPacket *packet)
{
    u64 pairs[7][2];
    int count;

    if (pxyFrameCopyRequested != 0) {
        count = 0;
        pairs[count][0] = 0x1000000000008000ULL;
        pairs[count++][1] = 14;
        pairs[count][0] = 0;
        pairs[count++][1] = 63;
        pairs[count][0] = ((sRenderFlipBase << 5) | 0x80000) | ((u64)sRenderPsm << 24) | ((u64)(sRenderDisplayBase << 5) << 32) | ((u64)sRenderPsm << 56) | 0x0008000000000000ULL;
        pairs[count++][1] = 80;
        pairs[count][0] = 0;
        pairs[count++][1] = 81;
        pairs[count][0] = (u64)cameraScreenWidth | ((u64)cameraScreenHeight << 32);
        pairs[count++][1] = 82;
        pairs[count][0] = 2;
        pairs[count++][1] = 83;
        pairs[0][0] += count - 1;
        sceVif1PkCnt(packet, 0);
        sceVif1PkOpenDirectHLCode(packet, 0);
        sceVif1PkAddDirectDataN(packet, pairs, count);
        sceVif1PkCloseDirectHLCode(packet);
    }
    pxyFrameCopyRequested = 0;
}

void pxyPlayEntryGroup5(int unused)
{
    pxyPlayEntryGroup(5);
}

void pxyInitCounters(void);
void pxyInitTables(void);
void CONSTRUCT_CIRCLR_SHADOW(void);
void pxyAddFrameCopyPacket(XglPacket *packet);
void pxyPlayEntryGroup5(int unused);

/* Init the pxy module: the reset helpers, then the two callbacks it installs in pxyFrameCopyHooks and pxyEntryGroupHooks. */
void nmlModelConstruct(void)
{
    pxyProfileDrawFlag = 0;
    pxyResetSlots();
    pxyInitCounters();
    pxyInitTables();
    pxyResetBatchTables();
    CONSTRUCT_CIRCLR_SHADOW();
    pxyFrameCopyRequested = 0;
    pxyFrameCopyHooks[0] = &pxyAddFrameCopyPacket;
    pxyEntryGroupHooks[0] = &pxyPlayEntryGroup5;
}

#include "quadword.h"

/* Copies one 16-byte vector through the $2 quadword register. Any quadword (a Vector4, a matrix row, a sort entry) is
 * passed by address, so the parameters are untyped. */
void xglVectorCopy_001CC998(register void *destination, register const void *source) {
    QuadwordCopy(destination, source);
}

/* Copies a 4x4 float matrix (four quadwords) through the $2 quadword register. */
void xglMatrixCopy_001CC9C4(register Matrix4 destination, register Matrix4 source) {
    QuadwordCopyMatrix(destination, source);
}

/* Clears one 16-byte vector to zero with a quadword store of $0. */
void xglVectorClear(register Vector4 *destination) {
    QuadwordZero(destination);
}

#include "vu0_macro.h"

void xglVectorAddXYZ_001CCA30(register void *dst, register void *first, register void *second)
{
    Vu0AddVectorXYZ(dst, first, second);
}

void xglVectorSubXYZ_001CCA68(register void *dst, register void *minuend, register void *subtrahend)
{
    Vu0SubVectorXYZ(dst, minuend, subtrahend);
}

/* .data of this file (0x645F10-0x645FB0). */
int pxyFadeRampOrderTable[32] = { 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31 };

int pxyCameraEnable[4] = { 0 };

Vector4 pxyGlobalColor = { 0 };
