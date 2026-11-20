#ifndef INCLUDE_MAIN_PXY_PACKET_STATE_H
#define INCLUDE_MAIN_PXY_PACKET_STATE_H

#include "shared.h"
#include "main/xgl_light.h"
#include "main/xgl_2.h"

/* One 16-byte entry of the direct queue (a GIF packet of A+D pairs): data and register address, or four words (the tag,
 * then the header words, are written through the word view). */
typedef union XglDirectQueueEntry {
    struct {
        u64 data;
        u64 address;
    };
    u32 word[4];
} XglDirectQueueEntry;

/* One source entry of xglDirectQueueAppendFlushWide: the data doubleword and the register address. */
typedef struct XglDirectQueueSourceEntry {
    u64 data;
    u32 address;
    u8 unmodeled_0c[4];
} XglDirectQueueSourceEntry;

/* A "txy" texture block: the magic, the set table and the flag word. nmlModelSetTexture walks its bytes from +0x10 up to
 * size; the packet code reads the offsets of the texture sets from +0x10. */
typedef struct TxyBlock {
    char magic[4]; /* "txy" */
    int setCount; /* +0x04 */
    u32 size; /* +0x08 */
    u32 flags; /* +0x0c: bit 31 set while the table is pending */
    int setOffset[8]; /* +0x10: offsets of the packet sets; the file table has 32 slots, only eight are declared */
} TxyBlock;

/* One 0x20-byte record of a model's entry table: the offset and size of its data in the model file. */
typedef struct PxyTableEntry {
    u32 offset; /* +0x00 */
    u32 count; /* +0x04 */
    u8 unmodeled_08[0x18];
} PxyTableEntry;

/* The clip planes of a packet (five quadwords): two planes, then the rotation rows pxySetClipPlaneWithAngle fills. The
 * first angle row's x and y are also read as the clip depth and range. */
typedef struct PxyClipPlanes {
    AlignedVector4 plane[2]; /* +0x00: plane normal and distance */
    AlignedVector4 angle[2]; /* +0x20: rotX, rotY, then the next rotX, rotY */
} PxyClipPlanes;

/* The "pxy" model file header. */
typedef struct PxyHeader {
    char magic[4]; /* "pxy" */
    float version; /* +0x04 */
    u32 flags; /* +0x08: bit 0x2000 = per-geometry shadow mask */
    u32 fileSize[1]; /* +0x0c, a one-word array, read as *h->fileSize */
    char name[0x20]; /* +0x10 */
    int layerCount; /* +0x30 */
    int entryCount; /* +0x34: same count as geometryCount (+0x60) */
    u8 unmodeled_38[4];
    int entryTableOffset; /* +0x3c: offset of the PxyTableEntry table (packet->entries) */
    int partOffset; /* +0x40 */
    u8 unmodeled_44[4];
    int partCount; /* +0x48 */
    u8 unmodeled_4C[8];
    int rangeOffset; /* +0x54 */
    int materialOffset; /* +0x58: offset of the material table */
    int nJointMatrix; /* +0x5c: number of global part matrices (pPxyHead->nJointMatrix in the original asserts) */
    int geometryCount; /* +0x60 */
    int jointMatrixOffset; /* +0x64: offset of the joint matrices */
    int entryMatrixOffset; /* +0x68: offset of the 0x40-byte entry matrices (the default part matrices) */
    int geometryOffset; /* +0x6c: offset of the 0x70-byte geometry array */
    int defaultJointOffset; /* +0x70: offset of the default joint matrices */
    int trackHeaderOffset; /* +0x74: offset of the track header */
    int trackTableOffset; /* +0x78: offset of the table of track part offsets */
    u8 unmodeled_7C[0x80 - 0x7C];
    AlignedVector4 sphere; /* +0x80: bounding sphere centre, radius in w */
    float minX; /* +0x90: box corner min, x */
    u8 unmodeled_94[0x98 - 0x94];
    float minZ; /* +0x98: box corner min, z */
    u8 unmodeled_9C[0xA0 - 0x9C];
    float maxX; /* +0xA0: box corner max, x */
    u8 unmodeled_A4[0xA8 - 0xA4];
    float maxZ; /* +0xA8: box corner max, z */
    u8 unmodeled_AC[0xC0 - 0xAC];
    int waterOffset; /* +0xc0: offset of the water table (WAT chunk), 0 if none */
    u8 unmodeled_C4[0xC8 - 0xC4];
    int trackOffset; /* +0xc8: offset of the PxyTrack of the flags-0x10 entries */
    int entryOffsets; /* +0xcc: offset of the int table of entry offsets */
    int subPackOffset; /* +0xd0: offset of a second pack, 0 if none */
    u8 unmodeled_D4[0xE8 - 0xD4];
    int sortEntryOffset; /* +0xe8 */
} PxyHeader;

struct PxyPartRange;
union Matrix;
struct PxyShadow;
struct PxyHideMask;

/* A 0x60-byte part of a pxy model (the record the scratchpad word at 0x7000001C points at while it is drawn). */
typedef struct PxyPart {
    u8 unmodeled_00[4];
    s16 entryIndex; /* +0x04: index into the model's entry table */
    u8 unmodeled_06[2];
    signed char layer; /* +0x08: layer, also the texture channel */
    u8 unmodeled_09[7];
    s16 matrixIndex; /* +0x10: index of the part matrix, also the bit of the pxyHideMask hide mask */
    u8 unmodeled_12[2];
    s16 index; /* +0x14: index into the per-part flag bytes */
    u8 unmodeled_16[2];
    u32 flags; /* +0x18 */
    signed char styleLevel; /* +0x1C: back-face removal switch and side */
    u8 unmodeled_1D[3];
    Vector4 boundsMin; /* +0x20 */
    Vector4 boundsMax; /* +0x30 */
    AlignedVector4 sphere; /* +0x40: centre and radius */
    u8 unmodeled_50[0x60 - 0x50];
} PxyPart;

/* The model packet: the 0x2a0-byte parameter block nmlModelEntry copies from nmlRenderStatus. The scratchpad word at
 * 0x70000010 points at the one being drawn. */
typedef struct PxyPacket {
    u32 renderStatus; /* +0x00 */
    int camera; /* +0x04: camera that owns the model */
    int drawMode; /* +0x08: 1 and 4 cast shadows */
    u8 unmodeled_0C[4]; /* +0x0C: the hide mask (pxyHideMask) */
    PxyHeader *header; /* +0x10 */
    struct PxyPartRange *ranges; /* +0x14 */
    u8 unmodeled_18[4];
    u8 *channelTable; /* +0x1C: the 0x70-byte geometry array (pxyHeaderTable6C) */
    PxyPart *parts; /* +0x20 */
    u8 unmodeled_24[0x2C - 0x24];
    u8 *matrixTable; /* +0x2C */
    u8 unmodeled_30[4];
    u8 *partFlags; /* +0x34 */
    TxyBlock *textureTable; /* +0x38 */
    PxyTableEntry *entries; /* +0x3C */
    u8 unmodeled_40[4];
    float reflectTransparency; /* +0x44: transparency of the break-up pieces */
    u8 unmodeled_48[4]; /* +0x48: model transparency (pxyTransparency) */
    float circleShadowScale; /* +0x4C: circle shadow scale, a copy of the transparency */
    u8 unmodeled_50[0x58 - 0x50];
    float shadowAlpha; /* +0x58: shadow strength */
    union Matrix *boneMatrices; /* +0x5C: one per part matrix index */
    u8 unmodeled_60[0x68 - 0x60];
    int (*levelTable)[4]; /* +0x68 */
    XglLightSet *light; /* +0x6C */
    float unmodeled_70[4]; /* +0x70 */
    struct PxyShadow *shadow; /* +0x80: the shadow projector; the matrices of its camera, the view at index 3 */
    u32 drawFlags; /* +0x84 */
    u8 unmodeled_88[0x94 - 0x88];
    float shadowY; /* +0x94 */
    union Matrix *shadowMatrix; /* +0x98 */
    int frameWriteMask; /* +0x9c: GS frame write mask */
    u8 unmodeled_A0[4];
    s16 planeCount; /* +0xA4 */
    s16 clipPlaneMaskedCount; /* +0xA6: 2 = the pieces move the other way */
    PxyClipPlanes *planes; /* +0xA8: clip planes */
    u8 unmodeled_AC[0xB0 - 0xAC];
    float clipPlaneMaskedTime; /* +0xB0: time factor */
    u8 *clipPlaneMask; /* +0xB4: mask of the geometries that break up */
    float clipPlaneMaskedScale; /* +0xB8: plane distance scale */
    float clipPlaneMaskedPush; /* +0xBC: push strength */
    float lowerClipHeight; /* +0xC0 */
    struct PxyHideMask *secondPartMask; /* +0xC4 */
    float upperClipHeight; /* +0xC8 */
    u8 unmodeled_CC[4];
    u8 *shadowReceiverParts; /* +0xD0: 16 part indices ended by 0xff, the receivers of the shadow */
    u32 unmodeled_D4; /* +0xD4 */
    float fadeAlpha; /* +0xD8 */
    u8 unmodeled_DC[4];
    Matrix4 localMatrix; /* +0xE0: model to world */
    Matrix worldMatrix; /* +0x120 */
    u8 unmodeled_160[0x170 - 0x160];
    int cameraMask[4]; /* +0x170: nonzero where a camera draws the model */
    int cameraFog[4]; /* +0x180: per studio camera fog factor, out of 4080 */
    float fogColorR; /* +0x190 */
    float fogColorG; /* +0x194 */
    float fogColorB; /* +0x198 */
    u8 unmodeled_19C[4];
    float unmodeled_1A0[4]; /* +0x1A0 */
    Vector4 center; /* +0x1B0: bounding sphere centre (the shadow position) */
    void (*preDraw)(int, int); /* +0x1c0 */
    u8 unmodeled_1C4[0x1D0 - 0x1C4];
    void (*postDraw)(int, int); /* +0x1d0 */
    u8 unmodeled_1D4[0x1E0 - 0x1D4];
    float unmodeled_1E0[8][4]; /* +0x1E0 */
    float unmodeled_260[4][4]; /* +0x260 */
} PxyPacket;

extern XglDirectQueueEntry *xglDirectQueueBuffer;
extern int xglDirectQueueCount;

/* Functions of src/main/pxy_packet_state.c that other files call. */

void pxySetRotateFreeze(int value);
void nmlPacketClrTextureCache(void);
void nmlPacketClrMatrixCache(void);
void nmlPacketClrModelCache(void);
void pxyResetSubmitCount(void);
int pxyGetSubmitCount(void);
void pxyClearFlushDone(void);
void pxySetFlushDone(void);
void nmlPacketAddWaitMicrocodeIrq(void);
void xglPacketModeReset(void);
void xglPacketSetMode3Flag(int value);
void xglPacketSetMode4Flag(int value);
int xglRoundUpPow2(int size);
int xglRoundUpLog2(int size);
int xglPsmBufferWidthEven(int size, int psm);
void nmlPacketGsInit(void);
void packet_gs_entry64(u64 address, u64 data);
void xglDirectQueueDefer(int flag, int key);
void pxyUploadPacketState(int mode, PxyPacket *model);
void pxyUploadViewMatrices(void);
void pxyUploadPartState(void);
void pxyUploadRenderFlags(int mode);
void xglPacketSelectDefaultMode(void);
void nmlPacketAddTransMicrocode(PxyPacket *model);
void pxyUploadBoneMatrices(void);
void nmlPacketAddTexture(void);
void nmlPacketAddGsFlush(void);
void nmlPacketAddGsFlushWide(void);
void pxyResetGsState(void);
void xglStudioSetScissor(void);
void xglSetFullScreenScissor(void);
void pxyDrawPartData(void);
void nmlPacketAddScreenClear(u32 frameBase, u32 flags);
void pxyDrawTextureRect(u32 texBase, Vector4 *srcRect, u32 frameBase, Vector4 *dstRect);
void pxyFillFrameAlphaMsbStrip(u16 unused0);
void pxySoftenShadow(u32 frameBase, PxyPacket *packet);
void nmlPacketMakeCircleTexture(PxyPacket *model);
void nmlPacketSendCircleTexture(const void *indices, void *palette);
void pxyUploadScreenSampleParams(void);
void pxySetGlobalColor(const Vector4 *colorScale);
void pxyUpdateBreakRotation(int reflect);
void pxySetBlackFogFlag(int flag);
void pxySetDrawPass(int value);
void xglStudioSelect(int index);
float xglPacketQueuedPercent(void);
float xglPacketBufferUsedPercent(void);
void xglPacketInitVuState(void);
void *nmlPacketSetAttributeData16N(void *matrices, int count);
void *nmlPacketSetAttributeAlloc16N(int count);
void pxySetAlphaRef(int value);
void xglPacketInitVuStateArg(int value);
void xglPacketAddRefOffset(const void *data, int count, int offset);
void xglDirectQueueAppendFlushWide(XglDirectQueueSourceEntry *source, int count);
void xglDirectQueueSubmitRef(void);

#endif /* INCLUDE_MAIN_PXY_PACKET_STATE_H */
