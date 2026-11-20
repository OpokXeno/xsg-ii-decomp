#include "common.h"
#include "main/xgl_camera.h"
#include "main/pxy_packet_state.h"
#include "main/pxy_shadow.h"
#include "main/xgl_render.h"
#include "gs.h"
#include "shared.h"
#include "main/xgl_packet.h"
#include "main/nml_packet_add.h"
#include "main/pxy_profile.h"
#include "main/nml_model_set.h"
#include "main/xgl_studio.h"
#include "main/cur_matrix_asm.h"
#include "main/xgl_2.h"

/* A studio camera. */


/* The .sdata block of this unit, 0x69E0D8-0x69E12C, defined in address order so
 * that every access below is $gp-relative. */
int xglStudioSelectedIndex = 0;
int pxyCurrentDrawPass = 0;
XglCamera *xglStudioSelectedCamera = 0;
int xglPacketMode = -1;
PxyPacket *pxyBoneCachePacket = 0;
TxyBlock *pxyTextureTableCache = 0;
int pxyTextureChannelCache = -1;
PxyPacket *pxyUploadedPacket = 0;
int pxyUploadMode = 0;
int pxyPartStyle = 0;
int pxySubmitCount = 0;
int xglDirectQueueRefOffset = 0;
int pxyReflectionType = 0;
int pxyBlackFogFlag = 0;
int pxyFlushDone = 0;
int xglPacketMode3Flag = 0;
int xglPacketMode4Flag = 0;
int pxyAlphaRef = 117;
int pxyRotateFreeze = 0;
int xglDirectQueueCount = 0;
XglDirectQueueEntry *xglDirectQueueBuffer = 0;


typedef int s32;

/* Quadword helpers of the end of this file; the quadword is passed by address whatever its type (vector, matrix row,
 * reflection block words). */
void xglVectorZero_001D4508(void *destination);
void xglMatrixCopy_001D44C4(void *destination, void *source);
void xglVectorSubXYZ_001D4530(void *dst, void *minuend, void *subtrahend);
/* The reflection matrix and the words packed into its last column and row. */
typedef union PxyBreakMatrix {
    Matrix4 rows;
    s32 words[4][4];
} PxyBreakMatrix;

/* Static block of the break-up state (.bss 0x9899F0-0x989A50): the colour vector, the break-up matrix,
 * then the cleared vector and the rotation angle. */
typedef struct PxyBreakBlock {
    Vector4 colour;     /* +0x00 pxyBreakBlock.colour (colorScale copy) */
    /* +0x10 break-up matrix (xglMatrixStackSave target); pxyUploadViewMatrices fills its last column with the
     * blend, shade and mode words and its last row with the position vector and the flag. */
    PxyBreakMatrix matrix;
    s32 zeroVec[3];     /* +0x50 cleared by pxyResetReflectionState */
    float angle;        /* +0x5C reflection rotation angle */
} PxyBreakBlock;

static PxyBreakBlock pxyBreakBlock;

void pxyResetReflectionState(void)
{
    xglMatrixUnit(pxyBreakBlock.matrix.rows);
    xglVectorZero_001D4508(pxyBreakBlock.zeroVec);
}

extern u8 xglPlainMicrocodeSize[];
extern u8 xglPlainMicrocodeProgram[];
extern u8 xglVertexColorMicrocodeSize[];
extern u8 xglVertexColorMicrocodeProgram[];
extern u8 xglBreakUpMicrocodeSize[];
extern u8 xglBreakUpMicrocodeProgram[];
extern u8 xglMode3MicrocodeSize[];
extern u8 xglMode3MicrocodeProgram[];
extern u8 xglMode4MicrocodeSize[];
extern u8 xglMode4MicrocodeProgram[];
extern u8 D_2C90[];
extern u8 D_3290[];
extern u8 D_3478[];
extern u8 D_39D0[];
extern u8 D_3B28[];
extern u8 D_D48[];
extern u8 D_D60[];
extern u8 D_D70[];
extern u8 D_D88[];
extern u8 D_DB0[];
extern u8 D_DD8[];

/* Microcode table lookup for the current xglPacketMode (0..4): kind 0 = program data, 1 = program size, 2 = model kick address,
 * 3 = GS kick address, 4 = wide GS kick address (the last three >> 3, for MSCAL). Packet mode 0 plain, 1 vertex colour,
 * 2 break-up, 3 = xglPacketMode3Flag, 4 = xglPacketMode4Flag (the battle variant of the lit program, no drop shadow). */
u8 *xglPacketMicrocodeEntry(int kind)
{
    switch (kind) {
    case 0:
        if (xglPacketMode == 0) {
            return xglPlainMicrocodeProgram;
        }
        if (xglPacketMode == 1) {
            return xglVertexColorMicrocodeProgram;
        }
        if (xglPacketMode == 2) {
            return xglBreakUpMicrocodeProgram;
        }
        if (xglPacketMode == 3) {
            return xglMode3MicrocodeProgram;
        }
        if (xglPacketMode == 4) {
            return xglMode4MicrocodeProgram;
        }
        break;
    case 1:
        if (xglPacketMode == 0) {
            return *(u8 **) xglPlainMicrocodeSize;
        }
        if (xglPacketMode == 1) {
            return *(u8 **) xglVertexColorMicrocodeSize;
        }
        if (xglPacketMode == 2) {
            return *(u8 **) xglBreakUpMicrocodeSize;
        }
        if (xglPacketMode == 3) {
            return *(u8 **) xglMode3MicrocodeSize;
        }
        if (xglPacketMode == 4) {
            return *(u8 **) xglMode4MicrocodeSize;
        }
        break;
    case 2:
        if (xglPacketMode == 0) {
            return (u8 *) ((u32) D_39D0 >> 3);
        }
        if (xglPacketMode == 1) {
            return (u8 *) ((u32) D_3478 >> 3);
        }
        if (xglPacketMode == 2) {
            return (u8 *) ((u32) D_2C90 >> 3);
        }
        if (xglPacketMode == 3) {
            return (u8 *) ((u32) D_3B28 >> 3);
        }
        if (xglPacketMode == 4) {
            return (u8 *) ((u32) D_3290 >> 3);
        }
        break;
    case 3:
        if (xglPacketMode == 0) {
            return (u8 *) ((u32) D_D60 >> 3);
        }
        if (xglPacketMode == 1) {
            return (u8 *) ((u32) D_DB0 >> 3);
        }
        if (xglPacketMode == 2) {
            return (u8 *) ((u32) D_D60 >> 3);
        }
        if (xglPacketMode == 3) {
            return (u8 *) ((u32) D_D60 >> 3);
        }
        if (xglPacketMode == 4) {
            return (u8 *) ((u32) D_D48 >> 3);
        }
        break;
    case 4:
        if (xglPacketMode == 0) {
            return (u8 *) ((u32) D_D88 >> 3);
        }
        if (xglPacketMode == 1) {
            return (u8 *) ((u32) D_DD8 >> 3);
        }
        if (xglPacketMode == 2) {
            return (u8 *) ((u32) D_D88 >> 3);
        }
        if (xglPacketMode == 3) {
            return (u8 *) ((u32) D_D88 >> 3);
        }
        if (xglPacketMode == 4) {
            return (u8 *) ((u32) D_D70 >> 3);
        }
        break;
    }
    return 0;
}


/*
 * Sets the rotation freeze flag: pxyUpdateBreakRotation adds no angle step while the flag is
 * nonzero. pxyFlush passes pxyFreezeAnimation.
 */
void pxySetRotateFreeze(s32 freeze)
{
    pxyRotateFreeze = freeze;
}


void pxySetAlphaRef(s32 value)
{
    pxyAlphaRef = value;
}


void pxySetFlushDone(void)
{
    pxyFlushDone = 1;
}

void pxyClearFlushDone(void)
{
    pxyFlushDone = 0;
}


void xglPacketSetMode3Flag(s32 value)
{
    xglPacketMode3Flag = value;
}


void xglPacketSetMode4Flag(s32 value)
{
    xglPacketMode4Flag = value;
}

void pxySetGlobalColor(const Vector4 *colorScale)
{
    pxyBreakBlock.colour = *colorScale;
}

f32 sinf(f32 x);

/* Break-up tumble rotation (X1 s_inReflRotX/Y): advances pxyBreakBlock.angle by 0.1 unless frozen, wraps at 2*pi and saves
 * RotY(angle) * RotX(sin(angle)) in pxyReflectionMatrix. */
void pxyUpdateBreakRotation(s32 reflect)
{
    if (reflect != 0) {
        pxyReflectionType = 1;
    } else {
        pxyReflectionType = 0;
    }
    if (pxyRotateFreeze == 0) {
        pxyBreakBlock.angle += 0.1f;
    }
    if (pxyBreakBlock.angle >= 6.283184f) {
        pxyBreakBlock.angle -= 6.283184f;
    }
    xglMatrixStackUnit();
    xglMatrixStackRotX(sinf(pxyBreakBlock.angle));
    xglMatrixStackRotY(pxyBreakBlock.angle);
    xglMatrixStackSave(pxyBreakBlock.matrix.rows);
}


/* Stores the studio index and caches that studio's active camera (xglStudioSelectedCamera). */
void xglStudioSelect(s32 index)
{
    xglStudioSelectedIndex = index;
    xglStudioSelectedCamera = xglStudioSelectGetActiveCamera(index);
}


void pxySetDrawPass(s32 value)
{
    pxyCurrentDrawPass = value;
}


void nmlPacketClrTextureCache(void)
{
    pxyTextureTableCache = 0;
    pxyTextureChannelCache = -1;
}


void nmlPacketClrMatrixCache(void)
{
    pxyBoneCachePacket = 0;
}

void xglPacketModeReset(void)
{
    xglPacketMode = -1;
}

/* The model packet whose state pxyUploadPacketState last uploaded (0 after a reset). */

void nmlPacketClrModelCache(void)
{
    pxyUploadedPacket = 0;
}

void xglPacketInitVuStateArg(int value)
{
    xglPacketInitVuState();
}


void xglPacketStateReset(void)
{
    xglPacketModeReset();
    nmlPacketClrTextureCache();
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
    xglPacketInitVuState();
}

/* Scratchpad words that pxyUploadPacketState hands to the packet callees: the current model packet, and a cleared flag. */
#define SPR_CURRENT_PXY (*(PxyPacket **) 0x70000010)
#define SPR_PXY_FLAG (*(s32 *) 0x7000001C)
#define SPR_PXY_CHANNEL (*(PxyPart **) 0x7000001C)

typedef unsigned int Quadword __attribute__((mode(TI)));

typedef union PxyQword {
    u64 reg[2];
    Vector4 vec;
} PxyQword;

extern s16 cameraScreenWidth;
extern s16 cameraScreenHeight;
extern u16 sRenderDepthBase;


void sceVif1PkAddUpkData128(XglPacket *packet, Quadword data);
void pxyUploadPartStateDefault(void);

/* GS TEST_1 with only the depth test enabled (ZTE = 1) at depth-test method `ztst`. */
#define TEST_ZONLY(ztst) GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, ztst)
/* Same, plus the destination alpha test (DATE, DATM = 1). */
#define TEST_ZDEST(date, ztst) GS_SET_TEST(0, 0, 0, 0, date, 1, 1, ztst)

void pxyUploadPacketState(s32 mode, PxyPacket *model)
{
    XglPacket *vif;

    vif = xglPacketGetCurrent();
    pxyUploadMode = mode;
    pxyPartStyle = 0;
    if (model->renderStatus & 0x20000000) {
        pxyPartStyle = 4;
    } else if (model->renderStatus & 0x10000000) {
        pxyPartStyle = 3;
    } else if (model->renderStatus & 0x1000000) {
        pxyPartStyle = 5;
    } else if (mode & 8) {
        pxyPartStyle = 2;
    } else if (mode & 0x1000) {
        pxyPartStyle = 6;
    }
    sceVif1PkCnt(vif, 0);
    if (pxyUploadedPacket != model) {
        if (model->renderStatus & 1) {
            sceVif1PkOpenUpkCode(vif, 0x39C, 0x6C, 1, 1);
            sceVif1PkAddUpkData128(vif, *(Quadword *) model->unmodeled_1A0);
            sceVif1PkCloseUpkCode(vif);
        }
        if (model->header->flags & 4) {
            sceVif1PkOpenUpkCode(vif, 0x370, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(vif, model->unmodeled_1E0, 8);
            sceVif1PkCloseUpkCode(vif);
        }
        if (model->header->flags & 0x10) {
            sceVif1PkOpenUpkCode(vif, 0x37C, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(vif, model->unmodeled_260, 4);
            sceVif1PkCloseUpkCode(vif);
        }
        if (model->renderStatus & 0x80) {
            sceVif1PkOpenUpkCode(vif, 0x3DB, 0x6C, 1, 1);
            sceVif1PkAddUpkData128(vif, *(Quadword *) model->unmodeled_70);
            sceVif1PkCloseUpkCode(vif);
        }
        {
        AlignedVector4 viewConstants = { 1.0f, 1.0f, 1.0f, 0.0f };
        PxyQword gsWords[6];

        sceVif1PkOpenUpkCode(vif, 0x39D, 0x6C, 1, 1);
        sceVif1PkAddUpkData128(vif, *(Quadword *) &viewConstants);
        sceVif1PkCloseUpkCode(vif);
        viewConstants.x = 4096.0f / (cameraScreenWidth + 4.0f);
        viewConstants.y = (4096.0f / (cameraScreenHeight + 4.0f))
                    / (12.272727f / (54.0f / (DispEnv.display1.field.magh + 1)));
        viewConstants.z = 1.001f;
        sceVif1PkOpenUpkCode(vif, 0x3E7, 0x6C, 1, 1);
        sceVif1PkAddUpkData128(vif, *(Quadword *) &viewConstants);
        sceVif1PkCloseUpkCode(vif);
        if (model->planes != 0
            && ((model->renderStatus & 0x2000 && model->renderStatus & 4) || model->renderStatus & 0x4000
                || model->renderStatus & 0x8000)) {
            Vector4 *row;
            Vector4 *plane;

            row = &gsWords[0].vec;
            plane = model->planes->plane;
            row[0].w = model->planes->plane[0].x;
            row[1].w = model->planes->plane[0].y;
            row[2].w = model->planes->plane[0].z;
            row[3].w = model->planes->plane[0].w;
            row[4].w = model->planes->angle[0].x;
            row[5].w = model->planes->angle[0].y;
            row[5].z = 1.0f / model->planes->angle[0].y;
            if (model->planeCount == 2) {
                plane = &model->planes->plane[1];
            }
            row[0].z = plane->x;
            row[1].z = plane->y;
            row[2].z = plane->z;
            row[3].z = plane->w;
            sceVif1PkOpenUpkCode(vif, 0x352, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(vif, gsWords, 6);
            sceVif1PkCloseUpkCode(vif);
        }
        SPR_CURRENT_PXY = model;
        SPR_PXY_FLAG = 0;
        pxyUploadViewMatrices();
        pxyUploadRenderFlags(mode);
        pxyUploadPartStateDefault();
        {
            s32 date;
            s32 ztst;

            date = 1;
            ztst = 2;
            if (mode & 8) {
                date = 0;
            }
            if (model->drawFlags & 0x200) {
                ztst = 1;
            }
            if (model->renderStatus & 0x10000000) {
                gsWords[0].reg[0] = TEST_ZONLY(1);
                gsWords[1].reg[0] = TEST_ZONLY(1);
                gsWords[2].reg[0] = TEST_ZONLY(1);
                gsWords[3].reg[0] = TEST_ZONLY(1);
                gsWords[4].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 1);
                gsWords[5].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 1);
            } else if (model->renderStatus & 0x1000000) {
                gsWords[0].reg[0] = TEST_ZONLY(ztst);
                gsWords[1].reg[0] = TEST_ZONLY(ztst);
                gsWords[2].reg[0] = TEST_ZONLY(ztst);
                gsWords[3].reg[0] = TEST_ZONLY(ztst);
                gsWords[4].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 1);
                gsWords[5].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 1);
            } else {
                if (mode & 8) {
                    gsWords[1].reg[0] = GS_SET_TEST(1, 5, pxyAlphaRef, 0, 0, 0, 1, ztst);
                    gsWords[3].reg[0] = GS_SET_TEST(1, 5, pxyAlphaRef, 0, date, 1, 1, 1);
                    gsWords[5].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 0);
                } else if (mode & 4) {
                    gsWords[1].reg[0] = TEST_ZONLY(ztst);
                    gsWords[3].reg[0] = TEST_ZDEST(date, 1);
                    gsWords[5].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 1);
                } else if (pxyCurrentDrawPass == 1) {
                    gsWords[1].reg[0] = GS_SET_TEST(1, 2, pxyAlphaRef, 0, 0, 0, 1, ztst);
                    gsWords[3].reg[0] = GS_SET_TEST(1, 2, pxyAlphaRef, 0, date, 1, 1, 1);
                    gsWords[5].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 1);
                } else {
                    gsWords[1].reg[0] = GS_SET_TEST(1, 5, pxyAlphaRef, 0, 0, 0, 1, ztst);
                    gsWords[3].reg[0] = GS_SET_TEST(1, 5, pxyAlphaRef, 0, date, 1, 1, 1);
                    gsWords[5].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 0);
                }
                gsWords[0].reg[0] = TEST_ZONLY(ztst);
                gsWords[2].reg[0] = TEST_ZDEST(date, ztst);
                if (pxyCurrentDrawPass == 1 && !(pxyUploadMode & 8)) {
                    gsWords[4].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 1);
                } else {
                    gsWords[4].reg[0] = GS_SET_ZBUF(sRenderDepthBase, 1, 0);
                }
            }
            gsWords[0].reg[1] = GS_TEST_1;
            gsWords[1].reg[1] = GS_TEST_1;
            gsWords[2].reg[1] = GS_TEST_1;
            gsWords[3].reg[1] = GS_TEST_1;
            gsWords[4].reg[1] = GS_ZBUF_1;
            gsWords[5].reg[1] = GS_ZBUF_1;
            sceVif1PkOpenUpkCode(vif, 0x3DC, 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(vif, gsWords, 6);
            sceVif1PkCloseUpkCode(vif);
        }
        }
        pxyUploadedPacket = model;
    }
    pxyAlphaRef = 0x75;
}

/* One word of a 16-byte record, viewed as float, signed or unsigned. */
typedef union XglScratchWord {
    float f;
    s32 i;
    u32 u;
} XglScratchWord;

/* A 16-byte record: the four words, the whole quadword for a by-value call, or a vector. */
typedef union XglScratchRecord {
    Quadword quad;
    XglScratchWord words[4];
    Vector4 vector;
} XglScratchRecord;

#define SCRATCH_WORD (*(u32 *) 0x70000028)

/* One 4x4 float matrix as a struct, so a whole matrix copies with one assignment. */
typedef struct XglMatrix {
    Matrix4 m;
} XglMatrix;


/* Uploads the view, light and part matrices of the current part to VU1 (addresses 0x384..0x3D4). */
void pxyUploadViewMatrices(void)
{
    XglPacket *vif;
    XglMatrix lightMatrix;
    XglMatrix viewMatrix;
    XglScratchRecord transformed;
    Matrix worldMatrix;
    XglMatrix modelMatrix;
    XglMatrix shadowMatrix;
    void *cameraMatrix;
    u64 unused360;
    float scale;
    float invScale;
    float extra;

    vif = xglPacketGetCurrent();
    sceVif1PkCnt(vif, 0);
    if (xglStudioSelectedCamera != 0) {
        worldMatrix = SPR_CURRENT_PXY->worldMatrix;
        if (SPR_PXY_CHANNEL != 0 && (SPR_PXY_CHANNEL->flags & 8) != 0) {
            xglMatrixInverse(&worldMatrix, &SPR_CURRENT_PXY->boneMatrices[SPR_PXY_CHANNEL->matrixIndex]);
            xglMatrixMul(worldMatrix.rows, SPR_CURRENT_PXY->worldMatrix.rows, worldMatrix.rows);
        }
        _CurMatrixSet(&worldMatrix.rows);
        _CurMatrixMul33normXYZW(viewMatrix.m[0], SPR_CURRENT_PXY->light->directions[0]);
        _CurApplyMatrix(&transformed, &xglStudioSelectedCamera->travel.position);
        viewMatrix.m[0][3] = transformed.words[0].f;
        viewMatrix.m[1][3] = transformed.words[1].f;
        viewMatrix.m[2][3] = transformed.words[2].f;
        if ((SPR_CURRENT_PXY->renderStatus & 0x40) != 0) {
            _CurApplyMatrix(lightMatrix.m[3], &SPR_CURRENT_PXY->light->pointPositions[0]);
            lightMatrix.m[0][0] = lightMatrix.m[3][0];
            lightMatrix.m[1][0] = lightMatrix.m[3][1];
            lightMatrix.m[2][0] = lightMatrix.m[3][2];
            _CurApplyMatrix(lightMatrix.m[3], &SPR_CURRENT_PXY->light->pointPositions[1]);
            lightMatrix.m[0][1] = lightMatrix.m[3][0];
            lightMatrix.m[1][1] = lightMatrix.m[3][1];
            lightMatrix.m[2][1] = lightMatrix.m[3][2];
            _CurApplyMatrix(lightMatrix.m[3], &SPR_CURRENT_PXY->light->pointPositions[2]);
            lightMatrix.m[0][2] = lightMatrix.m[3][0];
            lightMatrix.m[1][2] = lightMatrix.m[3][1];
            lightMatrix.m[2][2] = lightMatrix.m[3][2];
            sceVif1PkOpenUpkCode(vif, 980, 108, 1, 1);
            sceVif1PkAddUpkData128N(vif, &lightMatrix, 4);
            sceVif1PkCloseUpkCode(vif);
            sceVif1PkOpenUpkCode(vif, 984, 108, 1, 1);
            sceVif1PkAddUpkData128N(vif, SPR_CURRENT_PXY->light->pointColors, 3);
            sceVif1PkCloseUpkCode(vif);
        }
        sceVif1PkOpenUpkCode(vif, 900, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, &viewMatrix, 3);
        sceVif1PkCloseUpkCode(vif);
        sceVif1PkOpenUpkCode(vif, 830, 108, 1, 1);
        sceVif1PkAddUpkData128(vif, transformed.quad);
        sceVif1PkCloseUpkCode(vif);
        xglVectorSubXYZ_001D4530(&transformed, &xglStudioSelectedCamera->travel.position, &SPR_CURRENT_PXY->center);
        xglVectorNormal(&transformed.vector, &transformed.vector);
        _CurMatrixSet(&worldMatrix.rows);
        _CurMatrixMul33normXYZW(viewMatrix.m[0], SPR_CURRENT_PXY->light->directions[3]);
        _CurApplyMatrix(&transformed, &transformed);
        sceVif1PkOpenUpkCode(vif, 903, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, &viewMatrix, 3);
        sceVif1PkCloseUpkCode(vif);
        SPR_CURRENT_PXY->light->colors[0][3] = transformed.words[0].f;
        SPR_CURRENT_PXY->light->colors[1][3] = transformed.words[1].f;
        SPR_CURRENT_PXY->light->colors[2][3] = transformed.words[2].f;
        sceVif1PkOpenUpkCode(vif, 906, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, SPR_CURRENT_PXY->light->colors, 8);
        sceVif1PkCloseUpkCode(vif);
        cameraMatrix = xglStudioSelectedCamera->volume[0].viewMatrix;
        if ((SPR_CURRENT_PXY->renderStatus & 0x10000000) != 0) {
            cameraMatrix = &SPR_CURRENT_PXY->shadow->project;
        }
        xglMatrixCopy_001D44C4(&modelMatrix, SPR_CURRENT_PXY->localMatrix);
        if (SPR_PXY_CHANNEL != 0) {
            xglMatrixMul(modelMatrix.m, modelMatrix.m, SPR_CURRENT_PXY->boneMatrices[SPR_PXY_CHANNEL->matrixIndex].rows);
        }
        sceVif1PkOpenUpkCode(vif, 935, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, &modelMatrix, 4);
        sceVif1PkCloseUpkCode(vif);
        if ((SPR_CURRENT_PXY->renderStatus & 0x1000000) != 0) {
            xglMatrixMul(shadowMatrix.m, cameraMatrix, SPR_CURRENT_PXY->shadowMatrix->rows);
            xglMatrixMul(modelMatrix.m, shadowMatrix.m, modelMatrix.m);
        } else {
            xglMatrixMul(modelMatrix.m, cameraMatrix, modelMatrix.m);
        }
        sceVif1PkOpenUpkCode(vif, 918, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, &modelMatrix, 4);
        sceVif1PkCloseUpkCode(vif);
        if ((SPR_CURRENT_PXY->renderStatus & 0x8000) != 0 && SPR_CURRENT_PXY->clipPlaneMask != 0
            && SPR_PXY_CHANNEL != 0) {
            register u8 *partBitmask;

            xglVectorSubXYZ_001D4530(pxyBreakBlock.matrix.rows[3], &SPR_CURRENT_PXY->center,
                                     (AlignedVector4 *) SPR_CURRENT_PXY->localMatrix[3]);
            pxyBreakBlock.matrix.rows[0][3] = SPR_CURRENT_PXY->clipPlaneMaskedTime;
            pxyBreakBlock.matrix.rows[1][3] = 1.0f;
            if (SPR_CURRENT_PXY->clipPlaneMaskedCount == 2) {
                pxyBreakBlock.matrix.rows[1][3] = -1.0f;
            }
            pxyBreakBlock.matrix.words[2][3] = SPR_CURRENT_PXY->clipPlaneMaskedCount;
            pxyBreakBlock.matrix.words[3][3] = 0;
            partBitmask = SPR_CURRENT_PXY->clipPlaneMask;
            /* One bit per matrix index, packed into 32-bit words. */
            if ((*(u32 *) (partBitmask + ((s16) (SPR_PXY_CHANNEL->matrixIndex >> 5) << 2))
                 & (1 << (SPR_PXY_CHANNEL->matrixIndex & 0x1F))) != 0) {
                pxyBreakBlock.matrix.words[3][3] = 1;
            }
            sceVif1PkOpenUpkCode(vif, 969, 108, 1, 1);
            sceVif1PkAddUpkData128N(vif, pxyBreakBlock.matrix.rows, 4);
            sceVif1PkCloseUpkCode(vif);
            scale = SPR_CURRENT_PXY->clipPlaneMaskedScale;
            if (scale < 1.0f) {
                scale = 1.0f;
            }
            invScale = 1.0f / scale;
            extra = SPR_CURRENT_PXY->clipPlaneMaskedPush;
            sceVif1PkOpenUpkCode(vif, 973, 108, 1, 1);
            sceVif1PkAddUpkData128N(vif, &scale, 1);
            sceVif1PkCloseUpkCode(vif);
        }
    }
}


void pxyUploadPartState(void)
{
    XglPacket *packet;
    XglScratchRecord params[2];
    XglScratchRecord *style;

    packet = xglPacketGetCurrent();
    style = &params[0];
    style->words[2].f = 4.0f;
    style->words[3].i = pxyPartStyle;
    style->words[0].f = -1.0f;
    style->words[1].i = 0;
    if (style->words[3].i == 0) {
        if (SPR_CURRENT_PXY->reflectTransparency == 0.0f || (SPR_CURRENT_PXY->drawFlags & 0x800) != 0) {
            style->words[3].i = 1;
        }
    }
    if (SPR_PXY_CHANNEL->styleLevel < 4) {
        style->words[1].i = SPR_PXY_CHANNEL->styleLevel;
    }
    if (SPR_PXY_CHANNEL->styleLevel >= 2) {
        style->words[0].f = 1.0f;
    }
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenUpkCode(packet, 0x3C6, 0x6C, 1, 1);
    sceVif1PkAddUpkData128(packet, style->quad);
    sceVif1PkCloseUpkCode(packet);
    style->words[0].u = SCRATCH_WORD;
    style->words[1].i = 0;
    style->words[2].i = 0;
    style->words[3].i = 0;
    sceVif1PkOpenUpkCode(packet, 0x3FC, 0x6C, 1, 1);
    sceVif1PkAddUpkData128(packet, style->quad);
    sceVif1PkCloseUpkCode(packet);
}

typedef union PxyStateQuad {
    Quadword quad;
    u32 word[4];
    struct {
        f32 x;
        s32 y;
        f32 z;
        s32 w;
    } field;
} PxyStateQuad;

void pxyUploadPartStateDefault(void)
{
    XglPacket *packet;
    PxyStateQuad buffer;
    PxyStateQuad reserved;
    PxyStateQuad *state;

    packet = xglPacketGetCurrent();
    state = &buffer;
    state->field.z = 4.0f;
    state->field.w = pxyPartStyle;
    state->field.x = -1.0f;
    state->field.y = 0;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenUpkCode(packet, 966, 108, 1, 1);
    sceVif1PkAddUpkData128(packet, *(Quadword *) state);
    sceVif1PkCloseUpkCode(packet);
    state->word[0] = 0;
    state->word[1] = 0;
    state->word[2] = 0;
    state->word[3] = 0;
    sceVif1PkOpenUpkCode(packet, 1020, 108, 1, 1);
    sceVif1PkAddUpkData128(packet, *(Quadword *) state);
    sceVif1PkCloseUpkCode(packet);
}

typedef struct XglDrawParams {
    u32 flagBits;
    s32 cameraLevel;
    float alpha;
    float scale;
} XglDrawParams;

/* Uploads the VU1 flag word (params.flagBits) and the alpha/scale of the current model. */
void pxyUploadRenderFlags(s32 mode)
{
    XglPacket *packet;
    s32 cameraLevel;
    XglDrawParams params;
    AlignedVector4 uniformCopy;
    AlignedVector4 uniform;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    cameraLevel = SPR_CURRENT_PXY->cameraFog[xglStudioSelectedIndex];
    params.scale = 1.0f;
    if ((mode & 8) == 0) {
        params.scale = params.scale * SPR_CURRENT_PXY->circleShadowScale;
    }
    if ((mode & 0x1000000) != 0) {
        params.scale = 1.0f - SPR_CURRENT_PXY->shadowAlpha * SPR_CURRENT_PXY->circleShadowScale;
    }
    params.alpha = SPR_CURRENT_PXY->circleShadowScale * SPR_CURRENT_PXY->reflectTransparency;
    if (SPR_CURRENT_PXY->levelTable != 0 && SPR_PXY_CHANNEL != 0) {
        register s32 *levelRow;

        levelRow = SPR_CURRENT_PXY->levelTable[SPR_PXY_CHANNEL->matrixIndex];
        cameraLevel = *(s32 *) ((u8 *) levelRow + (xglStudioSelectedIndex << 2));
    }
    if ((SPR_CURRENT_PXY->renderStatus & 1) != 0 && (SPR_CURRENT_PXY->header->flags & 1) == 0) {
        params.alpha = params.alpha * ((float) cameraLevel / 4080.0f);
    }
    params.cameraLevel = cameraLevel;
    params.flagBits = 0;
    if ((mode & 8) != 0) {
        params.flagBits |= 4;
    }
    if ((mode & 4) != 0) {
        params.flagBits |= 8;
    }
    if (pxyCurrentDrawPass == 1) {
        params.flagBits |= 0x10;
    }
    if ((SPR_CURRENT_PXY->renderStatus & 1) != 0) {
        params.flagBits |= 2;
    }
    if ((SPR_CURRENT_PXY->renderStatus & 0x20000000) != 0) {
        params.flagBits |= 0x20;
    }
    if ((SPR_CURRENT_PXY->renderStatus & 0x40) != 0) {
        params.flagBits |= 0x40;
    }
    if ((SPR_CURRENT_PXY->renderStatus & 0x80) != 0) {
        params.flagBits |= 0x80;
    }
    if ((SPR_CURRENT_PXY->drawFlags & 1) != 0) {
        params.flagBits |= 0x100;
    }
    if ((SPR_CURRENT_PXY->renderStatus & 0x210000) != 0) {
        params.flagBits |= 0x400;
    }
    if (pxyBlackFogFlag != 0) {
        params.flagBits |= 0x800;
    }
    if (((SPR_CURRENT_PXY->renderStatus & 0x2000) != 0 && (SPR_CURRENT_PXY->renderStatus & 4) != 0)
        || (SPR_CURRENT_PXY->renderStatus & 0x4000) != 0) {
        params.flagBits |= 0x200;
    }
    if ((SPR_CURRENT_PXY->renderStatus & 0x8000) != 0) {
        params.flagBits |= 0x1000;
    }
    sceVif1PkOpenUpkCode(packet, 965, 108, 1, 1);
    sceVif1PkAddUpkData128(packet, *(Quadword *) &params);
    sceVif1PkCloseUpkCode(packet);
    uniform.x = SPR_CURRENT_PXY->fadeAlpha;
    uniform.y = SPR_CURRENT_PXY->fadeAlpha;
    uniform.z = SPR_CURRENT_PXY->fadeAlpha;
    uniform.w = 0.0f;
    uniformCopy = uniform;
    sceVif1PkOpenUpkCode(packet, 974, 108, 1, 1);
    sceVif1PkAddUpkData128(packet, *(Quadword *) &uniformCopy);
    sceVif1PkCloseUpkCode(packet);
}

/* Uploads `count` quadwords from `data` to VU1 data memory at `address`. */
void xglPacketUploadVu1Data(void *data, int count, int address)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenUpkCode(packet, (unsigned short)address, 108, 1, 1);
    sceVif1PkAddUpkData128N(packet, data, count);
    sceVif1PkCloseUpkCode(packet);
}

float tanf(float x);
void *memset(void *s, int c, unsigned int n);
const u8 vuStripGifTags[256] = { 0, 128, 0, 0, 0, 64, 14, 48, 18, 4, 0, 0, 0, 192, 14, 48, 0, 128, 0, 0, 0, 64, 6, 48, 18, 4, 0, 0, 0, 192, 6, 48, 0, 128, 0, 0, 0, 64, 46, 48, 18, 4, 0, 0, 0, 192, 46, 48, 0, 128, 0, 0, 0, 64, 38, 48, 18, 4, 0, 0, 0, 192, 38, 48, 0, 128, 0, 0, 0, 64, 14, 80, 242, 241, 4, 0, 0, 192, 14, 80, 0, 128, 0, 0, 0, 64, 6, 80, 242, 241, 4, 0, 0, 192, 6, 80, 0, 128, 0, 0, 0, 64, 46, 80, 242, 241, 4, 0, 0, 192, 46, 80, 0, 128, 0, 0, 0, 64, 38, 80, 242, 241, 4, 0, 0, 192, 38, 80, 0, 128, 0, 0, 0, 64, 30, 48, 18, 4, 0, 0, 0, 192, 30, 48, 0, 128, 0, 0, 0, 64, 22, 48, 18, 4, 0, 0, 0, 192, 22, 48, 0, 128, 0, 0, 0, 64, 62, 48, 18, 4, 0, 0, 0, 192, 62, 48, 0, 128, 0, 0, 0, 64, 54, 48, 18, 4, 0, 0, 0, 192, 54, 48, 0, 128, 0, 0, 0, 64, 30, 80, 242, 241, 4, 0, 0, 192, 30, 80, 0, 128, 0, 0, 0, 64, 22, 80, 242, 241, 4, 0, 0, 192, 22, 80, 0, 128, 0, 0, 0, 64, 62, 80, 242, 241, 4, 0, 0, 192, 62, 80, 0, 128, 0, 0, 0, 64, 54, 80, 242, 241, 4, 0, 0, 192, 54, 80 };
const u8 vuStripGifTagsUntextured[256] = { 0, 128, 0, 0, 0, 64, 6, 48, 18, 4, 0, 0, 0, 192, 6, 48, 0, 128, 0, 0, 0, 64, 6, 48, 18, 4, 0, 0, 0, 192, 6, 48, 0, 128, 0, 0, 0, 64, 38, 48, 18, 4, 0, 0, 0, 192, 38, 48, 0, 128, 0, 0, 0, 64, 38, 48, 18, 4, 0, 0, 0, 192, 38, 48, 0, 128, 0, 0, 0, 64, 6, 80, 242, 241, 4, 0, 0, 192, 6, 80, 0, 128, 0, 0, 0, 64, 6, 80, 242, 241, 4, 0, 0, 192, 6, 80, 0, 128, 0, 0, 0, 64, 38, 80, 242, 241, 4, 0, 0, 192, 38, 80, 0, 128, 0, 0, 0, 64, 38, 80, 242, 241, 4, 0, 0, 192, 38, 80, 0, 128, 0, 0, 0, 64, 22, 48, 18, 4, 0, 0, 0, 192, 22, 48, 0, 128, 0, 0, 0, 64, 22, 48, 18, 4, 0, 0, 0, 192, 22, 48, 0, 128, 0, 0, 0, 64, 54, 48, 18, 4, 0, 0, 0, 192, 54, 48, 0, 128, 0, 0, 0, 64, 54, 48, 18, 4, 0, 0, 0, 192, 54, 48, 0, 128, 0, 0, 0, 64, 22, 80, 242, 241, 4, 0, 0, 192, 22, 80, 0, 128, 0, 0, 0, 64, 22, 80, 242, 241, 4, 0, 0, 192, 22, 80, 0, 128, 0, 0, 0, 64, 54, 80, 242, 241, 4, 0, 0, 192, 54, 80, 0, 128, 0, 0, 0, 64, 54, 80, 242, 241, 4, 0, 0, 192, 54, 80 };
const u8 vuLayerGifTags[32] = { 0, 128, 0, 0, 0, 64, 46, 81, 47, 31, 4, 0, 0, 192, 46, 81, 0, 128, 0, 0, 0, 64, 38, 81, 47, 31, 4, 0, 0, 192, 38, 81 };
const u8 vuSmallStateTableReflection[32] = { 0, 128, 0, 0, 0, 64, 38, 81, 47, 31, 4, 0, 0, 192, 38, 81, 0, 128, 0, 0, 0, 64, 38, 81, 47, 31, 4, 0, 0, 192, 38, 81 };
const u8 vuThreeKickGifTags[32] = { 0, 128, 0, 0, 0, 64, 46, 81, 47, 241, 4, 0, 0, 192, 46, 81, 0, 128, 0, 0, 0, 64, 46, 81, 242, 241, 4, 0, 0, 192, 46, 81 };
const u8 vuEmptyEndTag[16] = { 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#define GS_ZBUF_2 0x4F

/* One 16-byte A+D entry of the register list: two data words, then the register and its high word. */
typedef struct GsRegEntry {
    u32 data[2];
    u32 reg;
    u32 regHigh;
} GsRegEntry;

/* One 16-byte record of the five-record VU1 parameter block uploaded to 994: a count, a CLIPW flag mask, a float and a spare word. */
typedef struct PxyVu1ParamRecord {
    u32 count;
    u32 clipFlags;
    float value;
    u32 spare;
} PxyVu1ParamRecord;

/* One quadword of a GIF packet, as four words (the tag) or as two register-data pairs. */
typedef union GifQword {
    u32 word[4];
    u64 reg[2];
    float value[4];
} GifQword;

/* Initializes the VU1 constants and GS state for the selected camera, then sets the default packet mode. */
void xglPacketInitVuState(void)
{
    XglPacket *vif;

    vif = xglPacketGetCurrent();
    sceVif1PkCnt(vif, 0);
    sceVif1PkOpenUpkCode(vif, 832, 108, 1, 1);
    sceVif1PkAddUpkData128(vif, *(Quadword *) &xglStudioSelectedCamera->screen.screenOffset);
    sceVif1PkCloseUpkCode(vif);
    sceVif1PkCnt(vif, 0);
    sceVif1PkOpenUpkCode(vif, 831, 108, 1, 1);
    sceVif1PkAddUpkData128(vif, *(Quadword *) &xglStudioSelectedCamera->screen.screenScale);
    sceVif1PkCloseUpkCode(vif);
    {
        GifQword gif[2];
        GifQword *tag;

        tag = gif;
        tag->word[0] = 0x8001;
        tag->word[1] = 0x10000000;
        tag->word[2] = 14;
        tag->word[3] = 0;
        tag[1].reg[0] = 0;
        tag[1].reg[1] = 63;
        sceVif1PkOpenUpkCode(vif, 813, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, tag, 2);
        sceVif1PkCloseUpkCode(vif);
        sceVif1PkOpenUpkCode(vif, 826, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, tag, 2);
        sceVif1PkCloseUpkCode(vif);
    }
    {
        GsRegEntry regs[6];
        GsRegEntry *regList;
        s32 regCount;

        regList = regs;
        regCount = 0;
        memset(regList, 0, 96);
        regList[regCount++].reg = 6;
        regList[regCount++].reg = 8;
        regList[regCount].regHigh = 7;
        regList[regCount++].reg = 66;
        regList[regCount++].reg = 67;
        regList[regCount++].reg = 20;
        regList[regCount++].reg = 21;
        sceVif1PkOpenUpkCode(vif, 834, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, regList, regCount);
        sceVif1PkCloseUpkCode(vif);
    }
    {
        PxyQword regPair;

        regPair.reg[0] = ((u64) 128 << 32) | 0x68;
        regPair.reg[1] = 67;
        sceVif1PkOpenUpkCode(vif, 959, 108, 1, 1);
        sceVif1PkAddUpkData128(vif, *(Quadword *) &regPair);
        sceVif1PkCloseUpkCode(vif);
        regPair.reg[0] = ((u64) 128 << 32) | 0x62;
        sceVif1PkOpenUpkCode(vif, 960, 108, 1, 1);
        sceVif1PkAddUpkData128(vif, *(Quadword *) &regPair);
        sceVif1PkCloseUpkCode(vif);
    }
    {
        const u8 *vuTable;
        const u8 *smallTable;

        vuTable = vuStripGifTags;
        smallTable = vuLayerGifTags;
        if (pxyReflectionType != 0) {
            vuTable = vuStripGifTagsUntextured;
            smallTable = vuSmallStateTableReflection;
        }
        sceVif1PkOpenUpkCode(vif, 860, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, vuTable, 16);
        sceVif1PkCloseUpkCode(vif);
        sceVif1PkOpenUpkCode(vif, 954, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, smallTable, 2);
        sceVif1PkCloseUpkCode(vif);
        sceVif1PkOpenUpkCode(vif, 956, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, vuThreeKickGifTags, 2);
        sceVif1PkCloseUpkCode(vif);
        sceVif1PkOpenUpkCode(vif, 828, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, vuEmptyEndTag, 1);
        sceVif1PkCloseUpkCode(vif);
    }
    {
        u32 buf[16];

        buf[3] = 0;
        buf[7] = 0x6000;
        buf[11] = 0;
        buf[10] = 4095;
        buf[15] = 0x7000;
        buf[14] = 0x7FFF;
        sceVif1PkOpenUpkCode(vif, 808, 108, 1, 1);
        sceVif1PkAddUpkData128N(vif, buf, 4);
        sceVif1PkCloseUpkCode(vif);
    }
    {
        Vector4 scaleVec;

        if (xglStudioSelectedCamera->travel.fov < 0.69813174f) {
            scaleVec.w = tanf(0.34906587f) /
                tanf(xglStudioSelectedCamera->travel.fov * 0.5f);
        } else {
            scaleVec.w = 1.0f;
        }
        scaleVec.x = scaleVec.w;
        scaleVec.y = scaleVec.w;
        scaleVec.z = scaleVec.w;
        xglPacketUploadVu1Data(&scaleVec, 1, 825);
        xglPacketUploadVu1Data(&pxyBreakBlock.colour, 1, 833);
    }
    {
        PxyVu1ParamRecord records[5];
        PxyVu1ParamRecord *record;

        record = records;
        record->count = 2;
        record->clipFlags = 2080;
        (record++)->value = -1.0f;
        record->count = 1;
        record->clipFlags = 520;
        (record++)->value = -1.0f;
        record->count = 1;
        record->clipFlags = 260;
        (record++)->value = 1.0f;
        record->count = 0;
        record->clipFlags = 130;
        (record++)->value = -1.0f;
        record->count = 0;
        record->clipFlags = 65;
        (record++)->value = 1.0f;
        xglPacketUploadVu1Data(records, 5, 994);
    }
    xglPacketSelectDefaultMode();
    nmlPacketGsInit();
    {
        u64 *p;
        u64 *start;
        u32 unk[12];

        p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
        start = p;
        GS_PUT(p, GS_ZBUF_2, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
        xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    }
    nmlPacketAddGsFlush();
}

u8 *xglPacketMicrocodeEntry(int mode);

void xglPacketSelectDefaultMode(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    xglPacketMode = 0;
    sceVif1PkRef(packet, xglPacketMicrocodeEntry(0), xglPacketMicrocodeEntry(1), 0, 0, 0);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, (u32) xglPacketMicrocodeEntry(3) | 0x14000000);
    xglDirectQueueCount = 0;
}

void nmlPacketAddTransMicrocode(PxyPacket *model)
{
    XglPacket *packet;
    s32 mode;

    packet = xglPacketGetCurrent();
    mode = 0;
    if (model->renderStatus & 0x8000) {
        mode = 2;
    } else if (model->header->flags & 1) {
        mode = 1;
    } else if (xglPacketMode4Flag) {
        mode = 4;
    } else if (xglPacketMode3Flag) {
        mode = 3;
    }
    if (xglPacketMode != mode) {
        xglPacketMode = mode;
        sceVif1PkRef(packet, xglPacketMicrocodeEntry(0), xglPacketMicrocodeEntry(1), 0, 0, 0);
        sceVif1PkCnt(packet, 0);
    }
}

/* Uploads the part's bone matrices (4 quadwords each) to VU1 address 0x228; skipped when this packet's matrices are already there. */
void pxyUploadBoneMatrices(void)
{
    XglPacket *packet;
    s32 i;
    u8 *base;
    u8 *matrixIds;

    packet = xglPacketGetCurrent();
    if (SPR_CURRENT_PXY->header->nJointMatrix == 0) {
        return;
    }
    if (SPR_CURRENT_PXY->header->nJointMatrix < 0x41) {
        if (pxyBoneCachePacket != SPR_CURRENT_PXY && SPR_CURRENT_PXY->matrixTable != 0) {
            i = SPR_CURRENT_PXY->header->nJointMatrix;
            sceVif1PkCnt(packet, 0);
            if (i == 0x40) {
                sceVif1PkOpenUpkCode(packet, 0x228, 0x6C, 1, 1);
                sceVif1PkAddUpkData128N(packet, SPR_CURRENT_PXY->matrixTable, 0xFC);
                sceVif1PkCloseUpkCode(packet);
                sceVif1PkOpenUpkCode(packet, 0x324, 0x6C, 1, 1);
                sceVif1PkAddUpkData128N(packet, SPR_CURRENT_PXY->matrixTable + 0xFC0, 4);
                sceVif1PkCloseUpkCode(packet);
            } else {
                sceVif1PkOpenUpkCode(packet, 0x228, 0x6C, 1, 1);
                sceVif1PkAddUpkData128N(packet, SPR_CURRENT_PXY->matrixTable, i * 4);
                sceVif1PkCloseUpkCode(packet);
            }
        }
        pxyBoneCachePacket = SPR_CURRENT_PXY;
    } else {
        i = 0;
        base = SPR_CURRENT_PXY->channelTable + ((SPR_PXY_CHANNEL->matrixIndex * 7) << 4);
        matrixIds = base + 0x30;
        sceVif1PkCnt(packet, 0);
        while (*matrixIds != 0) {
            sceVif1PkOpenUpkCode(packet, (u16) (i * 4 + 0x228), 0x6C, 1, 1);
            sceVif1PkAddUpkData128N(packet, SPR_CURRENT_PXY->matrixTable + (*matrixIds << 6) - 0x40, 4);
            sceVif1PkCloseUpkCode(packet);
            matrixIds++;
            i++;
        }
    }
}

void sceGsSetDefLoadImage(void *image, short fbp, short fbw, short psm, short x, short y, short w, short h);
int xglPsmBufferWidth(int value, int mode);

/* One 32-byte record of a texture set; its data sits dataOffset bytes past the table. */
typedef struct XglPacketRecord {
    s32 dataOffset;     /* +0x00 */
    u16 destX;          /* +0x04: destination x of the block */
    u8 unk06[2];
    s16 width;         /* +0x08: block width */
    u8 unk0A[2];
    s16 height;        /* +0x0C: block height */
    u8 unk0E[6];
    s32 count;          /* +0x14 */
    u8 unk18[8];
} XglPacketRecord;

/* A texture set of a txy block: its record count at +0x14, records from +0x20. */
typedef struct XglPacketSet {
    u8 unk00[0x14];
    s32 count;                  /* +0x14 */
    u8 unk18[0x20 - 0x18];
    XglPacketRecord records[8]; /* +0x20 */
} XglPacketSet;

typedef union XglPacketBlock5 {
    u64 words[12];
    Quadword quads[6];
} XglPacketBlock5;

/* One direct-mode message: a GIF tag quadword, then the block of six quadwords. */
typedef struct XglPacketMessage {
    u32 tag[4];
    XglPacketBlock5 block;
} XglPacketMessage;


void pxyQueueTextureSet(int slot, TxyBlock *table, int index, int originX)
{
    XglPacketSet *set;
    int i;
    XglPacketMessage msg;
    XglPacketRecord *rec;

    if (table->setCount <= index) {
        return;
    }
    set = (XglPacketSet *) (table->setOffset[index] + (int) table);
    if (set != 0) {
        i = set->count - 1;
        while (i >= 0) {
            rec = &set->records[i];
            sceGsSetDefLoadImage(&msg.block, (s16) ((u16) originX + rec->destX), (s16) xglPsmBufferWidth(512, 0), 0,
                                 0, 0, rec->width, rec->height);
            msg.block.words[0] &= ~0x8000;
            msg.tag[3] = 0x50000005;
            msg.tag[0] = 0;
            msg.tag[1] = 0;
            msg.tag[2] = 0;
            nmlModelDirectSend(slot, &msg, 6);
            nmlModelDirectSend(slot + 1, (u8 *) table + rec->dataOffset, rec->count);
            i--;
        }
    }
}

void nmlPacketTextureTrans(TxyBlock *table, int index, int originX)
{
    XglPacketBlock5 block;
    XglPacketSet *set;
    XglPacket *packet;
    int i;
    XglPacketRecord *rec;

    set = (XglPacketSet *) (table->setOffset[index] + (int) table);
    if (set != 0) {
        packet = xglPacketGetCurrent();
        i = set->count - 1;
        while (i >= 0) {
            rec = &set->records[i];
            sceVif1PkCnt(packet, 0);
            sceVif1PkAddCode(packet, 0x11000000);
            sceGsSetDefLoadImage(&block, (s16) ((u16) originX + rec->destX), (s16) xglPsmBufferWidth(512, 0), 0, 0, 0,
                          rec->width, rec->height);
            block.words[0] &= ~0x8000;
            sceVif1PkOpenDirectCode(packet, 0);
            sceVif1PkAddDirectDataN(packet, &block, 5);
            sceVif1PkCloseDirectCode(packet);
            sceVif1PkRef(packet, (u8 *) table + rec->dataOffset, rec->count, 0, 0, 0);
            i--;
        }
        sceVif1PkCnt(packet, 0);
    }
}

extern u16 sRenderTextureBase;


/* GS FOGCOL: the fog colour register that the first A+D pair of the packet writes. */
#define GS_FOGCOL 0x3D

/* Uploads the packet's texture table for the current channel if it changed, then sets GS FOGCOL from the packet's fog colour. */
void nmlPacketAddTexture(void)
{
    u64 *p;
    u64 *start;

    if (SPR_CURRENT_PXY->textureTable != 0 &&
        (pxyTextureTableCache != SPR_CURRENT_PXY->textureTable || pxyTextureChannelCache != SPR_PXY_CHANNEL->layer)) {
        nmlPacketTextureTrans(SPR_CURRENT_PXY->textureTable, SPR_PXY_CHANNEL->layer, sRenderTextureBase << 5);
        pxySubmitCount++;
        nmlPacketGsInit();
        p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
        start = p;
        p[0] = (u64) (u32) SPR_CURRENT_PXY->fogColorR | ((u64) (u32) SPR_CURRENT_PXY->fogColorG << 8)
            | ((u64) (u32) SPR_CURRENT_PXY->fogColorB << 16);
        p[1] = GS_FOGCOL;
        p += 2;
        GS_PUT(p, GS_TEXFLUSH, 0);
        xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
        nmlPacketAddGsFlush();
        pxyTextureTableCache = SPR_CURRENT_PXY->textureTable;
        pxyTextureChannelCache = SPR_PXY_CHANNEL->layer;
    }
}

void nmlPacketAddWaitMicrocode(void);

void nmlPacketAddGsFlush(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    if (xglDirectQueueCount >= 3) {
        packet->cursor += xglDirectQueueCount << 4;
        xglDirectQueueBuffer[0].word[0] = 0;
        xglDirectQueueBuffer[0].word[1] = 0;
        xglDirectQueueBuffer[0].word[2] = 0;
        xglDirectQueueBuffer[0].word[3] =
            ((xglDirectQueueCount - 1) << 16) + 0x6C000330;
        xglDirectQueueBuffer[1].word[0] = xglDirectQueueCount + 0x7FFE;
        xglDirectQueueBuffer[1].word[1] = 0x10000000;
        xglDirectQueueBuffer[1].word[2] = 0xE;
        xglDirectQueueBuffer[1].word[3] = 0;
        sceVif1PkRef(packet, xglDirectQueueBuffer, xglDirectQueueCount, 0, 0, 0);
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddCode(packet, (u32) xglPacketMicrocodeEntry(3) | 0x14000000);
        sceVif1PkAddCode(packet, 0x17000000);
        nmlPacketAddWaitMicrocode();
        sceVif1PkCnt(packet, 0);
    }
    xglDirectQueueCount = 0;
}

/* Same as nmlPacketAddGsFlush but unpacks at VU1 0x228 and kicks the wide GS program (kind 4); resets the bone cache. */
void nmlPacketAddGsFlushWide(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    if (xglDirectQueueCount >= 3) {
        packet->cursor += xglDirectQueueCount << 4;
        xglDirectQueueBuffer[0].word[0] = 0;
        xglDirectQueueBuffer[0].word[1] = 0;
        xglDirectQueueBuffer[0].word[2] = 0;
        xglDirectQueueBuffer[0].word[3] =
            ((xglDirectQueueCount - 1) << 16) + 0x6C000228;
        xglDirectQueueBuffer[1].word[0] = xglDirectQueueCount + 0x7FFE;
        xglDirectQueueBuffer[1].word[1] = 0x10000000;
        xglDirectQueueBuffer[1].word[2] = 0xE;
        xglDirectQueueBuffer[1].word[3] = 0;
        sceVif1PkRef(packet, xglDirectQueueBuffer, xglDirectQueueCount, 0, 0, 0);
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddCode(packet, (u32) xglPacketMicrocodeEntry(4) | 0x14000000);
        sceVif1PkAddCode(packet, 0x17000000);
        nmlPacketAddWaitMicrocode();
        sceVif1PkCnt(packet, 0);
    }
    xglDirectQueueCount = 0;
    nmlPacketClrMatrixCache();
}

extern u16 sRenderDisplayBase;
extern s16 sRenderPsm;

#define GS_FBA_2 0x4B
#define GS_TEST_2 0x48
#define GS_PSMCT24 0x01

void pxyResetGsState(void)
{
    u64 *p;
    u64 *start;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, GS_PSMCT24, 0));
    GS_PUT(p, GS_ZBUF_2, GS_SET_ZBUF(sRenderDepthBase, GS_PSMCT24, 0));
    GS_PUT(p, GS_FBA_1, 0);
    GS_PUT(p, GS_FBA_2, 0);
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
    GS_PUT(p, GS_TEST_2, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(sRenderDisplayBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0));
    GS_PUT(p, GS_FRAME_2, GS_SET_FRAME(sRenderDisplayBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0));
    GS_PUT(p, GS_ALPHA_1, GS_SET_ALPHA(0, 2, 2, 1, 0x80));
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    nmlPacketAddGsFlushWide();
}

#define GS_SCISSOR_2 0x41
/* The selected studio camera's screen rectangle (floats at +0x60..+0x6C), read as the GS scissor. */
#define STUDIO_CAMERA_WINDOW(edge) ((s32) xglStudioSelectedCamera->screen.window.edge)

void xglStudioSetScissor(void)
{
    u64 *p;
    u64 *start;

    if (xglStudioSelectedCamera == 0) {
        return;
    }
    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_SCISSOR_1,
           GS_SET_SCISSOR(STUDIO_CAMERA_WINDOW(x), STUDIO_CAMERA_WINDOW(z) - 1,
                          STUDIO_CAMERA_WINDOW(y), STUDIO_CAMERA_WINDOW(w) - 1));
    GS_PUT(p, GS_SCISSOR_2,
           GS_SET_SCISSOR(STUDIO_CAMERA_WINDOW(x), STUDIO_CAMERA_WINDOW(z) - 1,
                          STUDIO_CAMERA_WINDOW(y), STUDIO_CAMERA_WINDOW(w) - 1));
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    nmlPacketAddGsFlush();
}

void xglSetFullScreenScissor(void)
{
    u32 entry;
    u32 first;

    nmlPacketGsInit();
    entry = (u32) &xglDirectQueueBuffer[xglDirectQueueCount];
    first = entry;
    ((u64 *) entry)[0] = ((u64) (cameraScreenWidth - 1) << 16) | ((u64) (cameraScreenHeight - 1) << 48);
    ((u64 *) entry)[1] = 64;
    entry += 16;
    ((u64 *) entry)[0] = ((u64) (cameraScreenWidth - 1) << 16) | ((u64) (cameraScreenHeight - 1) << 48);
    ((u64 *) entry)[1] = 65;
    entry += 16;
    xglDirectQueueCount += (entry - first) >> 4;
    nmlPacketAddGsFlush();
}

/* The scratchpad words the model drawing code shares: the packet being drawn and its channel. */
typedef struct PxyScratch {
    u8 unk00[0x10];
    PxyPacket *model;     /* +0x10 */
    u8 unk14[0x8];
    PxyPart *channel;  /* +0x1C */
} PxyScratch;

void pxyDrawPartData(void)
{
    XglPacket *packet;
    register PxyScratch *scratch;

    packet = xglPacketGetCurrent();
    scratch = (PxyScratch *) 0x70000000;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, (u32) xglPacketMicrocodeEntry(2) | 0x14000000);
    sceVif1PkRef(packet, (u8 *) scratch->model->header + scratch->model->entries[scratch->channel->entryIndex].offset,
                 scratch->model->entries[scratch->channel->entryIndex].count, 0, 0, 0);
    nmlPacketAddWaitMicrocode();
}

/* Sets the current packet's count to 0 (sceVif1PkCnt), then adds VIF FLUSH with the interrupt bit (0x91000000). */
void nmlPacketAddWaitMicrocodeIrq(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x91000000);
}

/* Sets the current packet's count to 0 (sceVif1PkCnt), then adds VIF FLUSH (0x11000000). */
void nmlPacketAddWaitMicrocode(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
}

void xglPacketAddRefOffset(const void *data, int count, int offset)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkRef(packet, data, count, 0, offset, 0);
}

void xglPacketAddRef(const void *environment, int count)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkRef(packet, environment, count, 0, 0, 0);
}

void xglDirectQueueAppendFlushWide(XglDirectQueueSourceEntry *source, s32 count)
{
    XglDirectQueueSourceEntry *src;
    XglDirectQueueEntry *entry;
    XglDirectQueueEntry *first;
    s32 i;

    src = source;
    nmlPacketGsInit();
    entry = &xglDirectQueueBuffer[xglDirectQueueCount];
    first = entry;
    for (i = 0; i < count - 1; i++) {
        entry->data = src[i + 1].data;
    }
    ((u64 *) entry)[1] = src[i + 1].address;
    entry += 1;
    xglDirectQueueCount += ((u32) entry - (u32) first) >> 4;
    nmlPacketAddGsFlushWide();
}

/* GS primitive coordinates are 12.4 fixed point around the 2048 origin; Q = 1.0f as raw bits. */
#define GS_ORIGIN 0x800
#define GS_Q_ONE 0x3F800000
/* The shadow passes copy the 0xFF-pixel square that starts one pixel after the origin. */
#define GS_ORIGIN_1 (GS_ORIGIN + 1)
#define GS_LAST (GS_ORIGIN + 0xFF)
/* XYZ2 vertex at (x, y) in pixels. */
#define XY(x, y) GS_SET_XYZ((x) << 4, (y) << 4, 0)
/* Half the screen size. */
#define SCREEN_HALF_WIDTH ((s16) (cameraScreenWidth / 2))
#define SCREEN_HALF_HEIGHT ((s16) (cameraScreenHeight / 2))

void pxyFillFrameAlphaMsbStrip(u16 unused0)
{
    u64 *p;
    u64 *start;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_FBA_1, 1);
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    GS_PUT(p, GS_RGBAQ, (u64) 0xFE00 << 46);
    GS_PUT(p, GS_PRIM, 6);
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN + SCREEN_HALF_WIDTH, GS_LAST - SCREEN_HALF_HEIGHT));
    GS_PUT(p, GS_FBA_1, 0);
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    nmlPacketAddGsFlushWide();
}

/*
 * Queues the render state of the frame buffer at frameBase: the frame and Z-buffer registers, a sprite in
 * colour that covers the camera rectangle, and the frame register again for the second pass.
 * flags: 1 clears the colour alpha and the upper frame mask bits, 2 keeps only the alpha frame mask bits,
 * 4 clears the colour, 8 enables Z writes.
 */
void nmlPacketAddScreenClear(u32 frameBase, u32 flags)
{
    s32 colour[4];
    u32 frameMask;
    u32 zMask;
    u64 *p;
    u64 *start;

    frameMask = 0xFFFFFFFF;
    zMask = 1;
    colour[0] = ClearEnv.value.color_r;
    colour[1] = ClearEnv.value.color_g;
    colour[2] = ClearEnv.value.color_b;
    colour[3] = ClearEnv.value.color_a;
    if (flags & 4) {
        memset(colour, 0, 0x10);
    }
    if (flags & 2) {
        frameMask &= 0xFF000000;
    }
    if (flags & 1) {
        frameMask &= 0xFFFFFF;
        colour[3] = 0;
    }
    if (flags & 8) {
        zMask = 0;
    }

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_FBA_1, 0);
    GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, zMask));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, frameMask));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(colour[0], colour[1], colour[2], colour[3], GS_Q_ONE));
    GS_PUT(p, GS_PRIM, 6);
    GS_PUT(p, GS_XYZF2,
           GS_SET_XYZ((GS_ORIGIN - cameraScreenWidth) << 4, (GS_ORIGIN - cameraScreenHeight) << 4, 1));
    GS_PUT(p, GS_XYZF2,
           GS_SET_XYZ((GS_ORIGIN + cameraScreenWidth) << 4, (GS_ORIGIN + cameraScreenHeight) << 4, 1));
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0));
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    nmlPacketAddGsFlushWide();
}

#define SHADOW_LEVEL(packet) \
    ((int) ((packet)->shadowAlpha * 128.0f * (packet)->circleShadowScale * \
            (float) (packet)->cameraFog[xglStudioSelectedIndex] / 4080.0f))


/*
 * Writes the render-state packet for the texture at texBase and the frame at frameBase, then two
 * screen quads: the first from the UV and XYZ2 of posA, the second from the XYZ2 corner of posB.
 */
void pxyDrawTextureRect(u32 texBase, Vector4 *srcRect, u32 frameBase, Vector4 *dstRect)
{
    u64 *p;
    u64 *start;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0));
    GS_PUT(p, GS_TEXFLUSH, 0);
    GS_PUT(p, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 1));
    GS_PUT(p, GS_CLAMP_1, 0);
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, GS_Q_ONE));
    GS_PUT(p, GS_TEX0_1, GS_SET_TEX0(texBase << 5, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 1));
    GS_PUT(p, GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
    GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 0, 0, 1, 0, 0));
    GS_PUT(p, GS_UV, GS_SET_UV(((s32) srcRect->x + 1) << 4, ((s32) srcRect->y + 1) << 4));
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(p, GS_UV, GS_SET_UV((s32) srcRect->z << 4, (s32) srcRect->w << 4));
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN + ((s32) dstRect->z - SCREEN_HALF_WIDTH), GS_ORIGIN + ((s32) dstRect->w - SCREEN_HALF_HEIGHT)));
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    xglDirectQueueSubmitRef();
}

/* GS_ALPHA_2 register (0x43) of the glow setup packet. */
#define GS_ALPHA_2 0x43

/*
 * Writes the render-state packet for the texture at texBase and the frame at frameBase, then the
 * screen quad (UV and XYZ2 corners at the origin plus or minus half the screen size).
 */
void pxyDrawTextureFullscreen(u32 texBase, u32 frameBase)
{
    u64 *p;
    u64 *start;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0));
    GS_PUT(p, GS_TEXFLUSH, 0);
    GS_PUT(p, GS_CLAMP_1, 0);
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
    GS_PUT(p, GS_ALPHA_2, GS_SET_ALPHA(0, 2, 2, 1, 128));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, GS_Q_ONE));
    GS_PUT(p, GS_TEX0_1, GS_SET_TEX0(texBase << 5, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 1));
    GS_PUT(p, GS_TEX1_1, 0);
    GS_PUT(p, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0));
    GS_PUT(p, GS_UV, 0);
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(p, GS_UV, GS_SET_UV(cameraScreenWidth << 4, cameraScreenHeight << 4));
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN + SCREEN_HALF_WIDTH, GS_ORIGIN + SCREEN_HALF_HEIGHT));
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    xglDirectQueueSubmitRef();
}

/*
 * Softens the projected shadow of `packet` in the frame buffer at frameBase: a sprite whose level comes from the
 * shadow alpha and scale, three passes that copy the buffer onto itself shifted by one texel, and a line-strip border.
 */
void pxySoftenShadow(u32 frameBase, PxyPacket *packet)
{
    int i;
    u64 *p;
    u64 *q;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    q = p;
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(1, 0, 0, 1, 1, 0, 1, 1));
    if (packet->drawMode == 4) {
        GS_PUT(p, GS_RGBAQ,
               GS_SET_RGBAQ(SHADOW_LEVEL(packet), SHADOW_LEVEL(packet), SHADOW_LEVEL(packet), 0x80, GS_Q_ONE));
    } else {
        GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80 - SHADOW_LEVEL(packet), GS_Q_ONE));
    }
    GS_PUT(p, GS_PRIM, 6);
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(p, GS_XYZF2, XY(GS_ORIGIN, GS_LAST - SCREEN_HALF_HEIGHT));
    GS_PUT(p, GS_FRAME_1, GS_SET_FRAME(frameBase, (s16) (cameraScreenWidth / 64), sRenderPsm, 0));
    GS_PUT(p, GS_TEX0_1, GS_SET_TEX0(frameBase << 5, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 1));
    GS_PUT(p, GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(1, 0, 0, 1, 0, 0, 1, 1));
    GS_PUT(p, GS_CLAMP_1, 0);
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, GS_Q_ONE));
    xglDirectQueueCount += ((u32) p - (u32) q) >> 4;
    nmlPacketAddGsFlushWide();

    for (i = 0; i < 3; i++) {
        nmlPacketGsInit();
        q = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
        p = q;
        GS_PUT(q, GS_TEXFLUSH, 0);
        GS_PUT(q, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 0, 0, 1, 0, 0));
        GS_PUT(q, GS_UV, GS_SET_UV(0x10, 0x10));
        GS_PUT(q, GS_XYZF2, XY(GS_ORIGIN_1, GS_ORIGIN_1 - SCREEN_HALF_HEIGHT));
        GS_PUT(q, GS_UV, GS_SET_UV(0xFF0, 0xFF0));
        GS_PUT(q, GS_XYZF2, XY(GS_LAST, GS_LAST - SCREEN_HALF_HEIGHT));
        GS_PUT(q, GS_TEXFLUSH, 0);
        GS_PUT(q, GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 0, 0, 1, 0, 0));
        GS_PUT(q, GS_UV, GS_SET_UV((SCREEN_HALF_WIDTH + 1) << 4, 0x10));
        GS_PUT(q, GS_XYZF2, XY(GS_ORIGIN_1 - SCREEN_HALF_WIDTH, GS_ORIGIN_1 - SCREEN_HALF_HEIGHT));
        GS_PUT(q, GS_UV, GS_SET_UV((SCREEN_HALF_WIDTH + 0xFF) << 4, 0xFF0));
        GS_PUT(q, GS_XYZF2, XY(GS_LAST - SCREEN_HALF_WIDTH, GS_LAST - SCREEN_HALF_HEIGHT));
        xglDirectQueueCount += ((u32) q - (u32) p) >> 4;
        nmlPacketAddGsFlushWide();
    }

    nmlPacketGsInit();
    q = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    p = q;
    GS_PUT(q, GS_TEXFLUSH, 0);
    GS_PUT(q, GS_TEST_1, GS_SET_TEST(1, 0, 0, 1, 0, 0, 1, 1));
    GS_PUT(q, GS_RGBAQ, GS_SET_RGBAQ(0, 0, 0, 0x80, GS_Q_ONE));
    GS_PUT(q, GS_PRIM, 2);
    GS_PUT(q, GS_XYZF2, XY(GS_LAST - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(q, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    GS_PUT(q, GS_XYZF2, XY(GS_ORIGIN - SCREEN_HALF_WIDTH, GS_LAST - SCREEN_HALF_HEIGHT));
    GS_PUT(q, GS_XYZF2, XY(GS_LAST - SCREEN_HALF_WIDTH, GS_LAST - SCREEN_HALF_HEIGHT));
    GS_PUT(q, GS_XYZF2, XY(GS_LAST - SCREEN_HALF_WIDTH, GS_ORIGIN - SCREEN_HALF_HEIGHT));
    xglDirectQueueCount += ((u32) q - (u32) p) >> 4;
    nmlPacketAddGsFlushWide();
}

extern u16 sRenderFrameBase;

#define GS_BITBLTBUF 0x50
#define GS_TRXPOS 0x51
#define GS_TRXREG 0x52
#define GS_TRXDIR 0x53
#define GS_PSMZ24 0x31
#define GS_SET_BITBLTBUF(sbp, sbw, spsm, dbp, dbw, dpsm) \
    ((u64)(sbp) | ((u64)(sbw) << 16) | ((u64)(spsm) << 24) | ((u64)(dbp) << 32) | ((u64)(dbw) << 48) \
     | ((u64)(dpsm) << 56))
#define GS_SET_TRXREG(width, height) ((u64)(width) | ((u64)(height) << 32))

void xglCopyZBufferToRenderFrame(s32 unused0, s32 unused1)
{
    u64 *p;
    u64 *start;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_BITBLTBUF,
           GS_SET_BITBLTBUF(sRenderDepthBase << 5, 8, GS_PSMZ24, sRenderFrameBase << 5, 8, GS_PSMZ24));
    GS_PUT(p, GS_TRXPOS, 0);
    GS_PUT(p, GS_TRXREG, GS_SET_TRXREG(cameraScreenWidth, cameraScreenHeight));
    GS_PUT(p, GS_TRXDIR, 2);
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    nmlPacketAddGsFlush();
}

void xglCopyRenderFrameToZBuffer(s32 unused0, s32 unused1)
{
    u64 *p;
    u64 *start;

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_BITBLTBUF,
           GS_SET_BITBLTBUF(sRenderFrameBase << 5, 8, GS_PSMZ24, sRenderDepthBase << 5, 8, GS_PSMZ24));
    GS_PUT(p, GS_TRXPOS, 0);
    GS_PUT(p, GS_TRXREG, GS_SET_TRXREG(cameraScreenWidth, cameraScreenHeight));
    GS_PUT(p, GS_TRXDIR, 2);
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    nmlPacketAddGsFlush();
}


void *memcpy(void *destination, const void *source, unsigned int size);

void *nmlPacketSetAttributeData(void *data, int size)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    memcpy(packet->cursor, data, size);
    packet->cursor += size;
    return packet->cursor - size;
}


void *nmlPacketSetAttributeData64(void *matrix)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    xglMatrixCopy_001D44C4(packet->cursor, matrix);
    packet->cursor += 64;
    return packet->cursor - 64;
}


void *nmlPacketSetAttributeData64N(void *matrices, int count)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    xglMemCopy64(packet->cursor, matrices, count);
    packet->cursor += count << 6;
    return packet->cursor - (count << 6);
}


void *nmlPacketSetAttributeData16N(void *matrices, int count)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    xglMemCopy16(packet->cursor, matrices, count);
    packet->cursor += count << 4;
    return packet->cursor - (count << 4);
}

void *nmlPacketSetAttributeAlloc16N(int count)
{
    XglPacket *packet;
    u8 *cursor;

    packet = xglPacketGetCurrent();
    cursor = packet->cursor;
    if ((u32) packet->cursor - packet->limit >= packet->buffer + packet->bufferSize - packet->limit - 0x8000) {
        return 0;
    }
    packet->cursor = packet->cursor + (count << 4) + 16;
    return cursor;
}

/*
 * Rounds a requested size up to the next power of two: the floor is 2 and
 * any size above 0x800 returns 1.
 */
int xglRoundUpPow2(int size)
{
    size = size;
    if (size < 3)
    {
        return 2;
    }
    if (size < 5)
    {
        return 4;
    }
    if (size < 9)
    {
        return 8;
    }
    if (size < 0x11)
    {
        return 0x10;
    }
    if (size < 0x21)
    {
        return 0x20;
    }
    if (size < 0x41)
    {
        return 0x40;
    }
    if (size < 0x81)
    {
        return 0x80;
    }
    if (size < 0x101)
    {
        return 0x100;
    }
    if (size < 0x201)
    {
        return 0x200;
    }
    if (size < 0x401)
    {
        return 0x400;
    }
    if (size < 0x801)
    {
        return 0x800;
    }
    return 1;
}

int xglRoundUpLog2(int size)
{
    if (size < 3) {
        return 1;
    }
    if (size < 5) {
        return 2;
    }
    if (size < 9) {
        return 3;
    }
    if (size < 0x11) {
        return 4;
    }
    if (size < 0x21) {
        return 5;
    }
    if (size < 0x41) {
        return 6;
    }
    if (size < 0x81) {
        return 7;
    }
    if (size < 0x101) {
        return 8;
    }
    if (size < 0x201) {
        return 9;
    }
    if (size < 0x401) {
        return 10;
    }
    if (size < 0x801) {
        return 11;
    }
    return 1;
}

int xglPsmBufferWidthEven(int size, int psm)
{
    int result;

    result = 1;
    switch (psm) {
    case 20:
        result = size / 128 * 2;
        if ((size & 0x7F) != 0) {
            result += 2;
        }
        break;
    case 19:
        result = size / 128 * 2;
        if ((size & 0x7F) != 0) {
            result += 2;
        }
        break;
    case 0:
        result = size / 128 * 2;
        if ((size & 0x7F) != 0) {
            result += 2;
        }
        break;
    }
    return result;
}

int xglPsmBufferWidth(int width, int psm)
{
    int result;

    result = 1;
    switch (psm) {
    case 20:
        result = width / 256 + 1;
        break;
    case 19:
        result = width / 128 + 1;
        break;
    case 0:
        result = width / 64;
        if ((width & 0x3F) != 0) {
            result += 1;
        }
        break;
    }
    return result;
}

void nmlPacketGsInit(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    xglDirectQueueBuffer = (XglDirectQueueEntry *) packet->cursor;
    xglDirectQueueCount = 0;
    xglDirectQueueBuffer[xglDirectQueueCount].word[0] = 0;
    xglDirectQueueBuffer[xglDirectQueueCount].word[1] = 0;
    xglDirectQueueBuffer[xglDirectQueueCount].word[2] = 0;
    xglDirectQueueBuffer[xglDirectQueueCount++].word[3] = 0x50000000;
    xglDirectQueueBuffer[xglDirectQueueCount].data = 0x1000000000008000ULL;
    xglDirectQueueBuffer[xglDirectQueueCount++].address = 14;
}

void packet_gs_entry64(u64 address, u64 data)
{
    xglDirectQueueBuffer[xglDirectQueueCount].data = data;
    xglDirectQueueBuffer[xglDirectQueueCount++].address = address;
}

void xglDirectQueuePushCopy(const XglDirectQueueEntry *entry)
{
    xglDirectQueueBuffer[xglDirectQueueCount++] = *entry;
}


void xglDirectQueueDefer(int flag, int key)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    xglDirectQueueBuffer[0].word[3] = xglDirectQueueCount + 0x4fffffff;
    xglDirectQueueBuffer[1].word[0] = xglDirectQueueCount + 0x7ffe;
    nmlModelDirectSendFull(key | 1, xglDirectQueueBuffer, xglDirectQueueCount, flag, xglDirectQueueRefOffset);
    packet->cursor += xglDirectQueueCount << 4;
    xglDirectQueueCount = 0;
    xglDirectQueueRefOffset = 0;
}


void xglDirectQueueSetRefOffset(s32 value)
{
    xglDirectQueueRefOffset = value;
}

void xglDirectQueueSubmitRef(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    xglDirectQueueBuffer[0].word[3] = xglDirectQueueCount + 0x4fffffff;
    xglDirectQueueBuffer[1].word[0] = xglDirectQueueCount + 0x7ffe;
    sceVif1PkRef(packet, xglDirectQueueBuffer, xglDirectQueueCount, 0, xglDirectQueueRefOffset, 0);
    packet->cursor += xglDirectQueueCount << 4;
    xglDirectQueueCount = 0;
    xglDirectQueueRefOffset = 0;
}


void pxyResetSubmitCount(void)
{
    pxySubmitCount = 0;
}

s32 pxyGetSubmitCount(void)
{
    return pxySubmitCount;
}


f32 xglPacketQueuedPercent(void)
{
    XglPacket *packet;
    register f32 ratio;
    register u32 usedBytes;
    register u32 capacity;
    register f32 capacityF;

    packet = xglPacketGetCurrent();
    usedBytes = sceVif1PkSize(packet) << 4;
    if ((s32) usedBytes >= 0) {
        ratio = (f32) (s32) usedBytes;
    } else {
        ratio = (f32) (s32) ((usedBytes & 1) | (usedBytes >> 1));
        ratio += ratio;
    }
    capacity = packet->limit - packet->buffer;
    if ((s32) capacity >= 0) {
        capacityF = (f32) (s32) capacity;
    } else {
        capacityF = (f32) (s32) ((capacity & 1) | (capacity >> 1));
        capacityF += capacityF;
    }
    ratio /= capacityF;
    ratio *= 100.0f;
    return ratio;
}

/* Percentage of the current packet's buffer in use. */
f32 xglPacketBufferUsedPercent(void)
{
    XglPacket *packet;
    register f32 ratio;
    register u32 usedBytes;

    packet = xglPacketGetCurrent();
    usedBytes = (u32) packet->cursor - packet->limit;
    ratio = usedBytes;
    ratio /= packet->buffer + packet->bufferSize - packet->limit;
    ratio *= 100.0f;
    return ratio;
}


/* One screen position of the circle shadow quad: GS X, Y and depth as integers, and the scale of the corner. */
typedef ScreenVector4 PxyScreenVertex;


#define GS_SET_ST(s, t) ((u64)(s) | ((u64)(t) << 32))
/* The bit pattern of a float, as the packet words carry it. */
#define FLOAT_BITS(f) (*(s32 *) &(f))

/* Four corner signs of the circle shadow quad, viewed as floats or copied as two doublewords. */
typedef Vector4Doublewords GlowSigns;

/*
 * Writes the circle shadow quad of the active camera: four corners at the unit square, projected through the
 * camera's view, then the packet of the four vertices. Does nothing without a selected camera.
 */
/* Draws the circle shadow quad (textured with the circle texture sent by nmlPacketSendCircleTexture) under the model. */
void nmlPacketMakeCircleTexture(PxyPacket *model)
{
    union { Matrix4 f; Vector4 v[4]; } worldCorner;
    union { Matrix4 f; s32 words[4][4]; Vector4 v[4]; } texCoord;
    union { PxyScreenVertex v[4]; float f[4][4]; } screen;
    GlowSigns xSign = { { -1.0f, 1.0f, -1.0f, 1.0f } };
    GlowSigns zSign = { { -1.0f, -1.0f, 1.0f, 1.0f } };
    int i;
    float extent;
    u64 *p;
    u64 *start;

    extent = 1.0f;
    if (xglStudioSelectedCamera == 0) {
        return;
    }
    extent = (model->header->maxX - model->header->minX) + (model->header->maxZ - model->header->minZ);
    texCoord.f[0][0] = 0.0f;
    texCoord.f[0][1] = 0.0f;
    texCoord.f[0][2] = 1.0f;
    texCoord.f[1][0] = 1.0f;
    texCoord.f[1][1] = 0.0f;
    texCoord.f[1][2] = 1.0f;
    texCoord.f[2][0] = 0.0f;
    texCoord.f[2][1] = 1.0f;
    texCoord.f[2][2] = 1.0f;
    texCoord.f[3][0] = 1.0f;
    texCoord.f[3][1] = 1.0f;
    texCoord.f[3][2] = 1.0f;
    _CurMatrixSet(&xglStudioSelectedCamera->volume[0].viewMatrix);
    _CurSetViewScaleTrans(&xglStudioSelectedCamera->screen.screenScale,
                          &xglStudioSelectedCamera->screen.screenOffset);
    for (i = 0; i < 4; i++) {
        worldCorner.f[i][0] = model->center.x + xSign.component[i] * extent * 0.35f;
        worldCorner.f[i][1] = model->shadowY + 0.005f;
        worldCorner.f[i][2] = model->center.z + zSign.component[i] * extent * 0.35f;
        if (_CurRotTransPersClipScreen(&screen.v[i], &worldCorner.v[i]) != 0) {
            xglVectorScaleXYZ(&texCoord.v[i], &texCoord.v[i], screen.f[i][3]);
            texCoord.words[i][3] = GS_XYZ3;
        } else {
            xglVectorScaleXYZ(&texCoord.v[i], &texCoord.v[i], screen.f[i][3]);
            texCoord.words[i][3] = GS_XYZF2;
        }
    }
    if (texCoord.words[2][3] == GS_XYZ3) {
        texCoord.words[3][3] = GS_XYZ3;
    }
    if (texCoord.words[1][3] == GS_XYZ3) {
        texCoord.words[2][3] = GS_XYZ3;
        texCoord.words[3][3] = GS_XYZ3;
    }
    if (texCoord.words[0][3] == GS_XYZ3) {
        texCoord.words[1][3] = GS_XYZ3;
        texCoord.words[2][3] = GS_XYZ3;
    }

    nmlPacketGsInit();
    p = (u64 *) &xglDirectQueueBuffer[xglDirectQueueCount];
    start = p;
    GS_PUT(p, GS_TEXFLUSH, 0);
    GS_PUT(p, GS_TEX0_1,
           GS_SET_TEX0(sRenderTextureBase << 5, 1, GS_PSMT8, 6, 6, 1, 0, (sRenderTextureBase << 5) + 128, 0, 0, 0, 1));
    GS_PUT(p, GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
    GS_PUT(p, GS_TEST_1, GS_SET_TEST(1, 0, 0, 1, 0, 0, 1, 3));
    GS_PUT(p, GS_CLAMP_1, 0);
    GS_PUT(p, GS_ALPHA_1, GS_SET_ALPHA(1, 2, 0, 2, 0));
    GS_PUT(p, GS_PRIM, GS_SET_PRIM(4, 1, 1, 0, 1, 0, 0, 0, 0));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, FLOAT_BITS(screen.f[0][3])));
    GS_PUT(p, GS_ST, GS_SET_ST(FLOAT_BITS(texCoord.f[0][0]), FLOAT_BITS(texCoord.f[0][1])));
    GS_PUT(p, texCoord.words[0][3], GS_SET_XYZ(screen.v[0].x, screen.v[0].y, (s32) ((float) screen.v[0].z * 1.01f)));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, FLOAT_BITS(screen.f[1][3])));
    GS_PUT(p, GS_ST, GS_SET_ST(FLOAT_BITS(texCoord.f[1][0]), FLOAT_BITS(texCoord.f[1][1])));
    GS_PUT(p, texCoord.words[1][3], GS_SET_XYZ(screen.v[1].x, screen.v[1].y, (s32) ((float) screen.v[1].z * 1.01f)));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, FLOAT_BITS(screen.f[2][3])));
    GS_PUT(p, GS_ST, GS_SET_ST(FLOAT_BITS(texCoord.f[2][0]), FLOAT_BITS(texCoord.f[2][1])));
    GS_PUT(p, texCoord.words[2][3], GS_SET_XYZ(screen.v[2].x, screen.v[2].y, (s32) ((float) screen.v[2].z * 1.01f)));
    GS_PUT(p, GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, FLOAT_BITS(screen.f[3][3])));
    GS_PUT(p, GS_ST, GS_SET_ST(FLOAT_BITS(texCoord.f[3][0]), FLOAT_BITS(texCoord.f[3][0])));
    GS_PUT(p, texCoord.words[3][3], GS_SET_XYZ(screen.v[3].x, screen.v[3].y, (s32) ((float) screen.v[3].z * 1.01f)));
    xglDirectQueueCount += ((u32) p - (u32) start) >> 4;
    xglDirectQueueSubmitRef();
}

#include "quadword.h"


/*
 * Builds the two GIF packets for the environment and the vectors. gifPacket
 * holds 12 quadwords of which the first five are sent.
 */
/* X1 nmlPacketSendCircleTexture: loads the 64x64 PSMT8 circle texture and its 16x16 PSMCT32 palette. */
void nmlPacketSendCircleTexture(const void *indices, void *palette)
{
    XglPacket *packet;
    u64 image[12];

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceGsSetDefLoadImage(image, (short)(sRenderTextureBase << 5), 1, 19, 0, 0, 64, 64);
    image[0] |= 0x8000;
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, image, 5);
    sceVif1PkCloseDirectCode(packet);
    sceVif1PkRef(packet, indices, 258, 0, 0, 0);
    sceVif1PkCnt(packet, 0);
    sceGsSetDefLoadImage(image, (short)((sRenderTextureBase << 5) + 128), 1, 0, 0, 0, 16, 16);
    image[0] |= 0x8000;
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, image, 5);
    sceVif1PkCloseDirectCode(packet);
    sceVif1PkRef(packet, nmlPacketSetAttributeData16N(palette, 66), 66, 0, 0, 0);
}

void xglPacketUploadVu1Data(void *data, int count, int address);

/*
 * Builds the ten-quadword GIF packet (a tag, the A+D registers and two
 * float-scaled rectangles) in a 12-quadword buffer and hands it to xglPacketUploadVu1Data.
 */
void pxyUploadScreenSampleParams(void)
{
    GifQword block[12];
    GifQword *cursor;

    cursor = block;
    cursor->word[0] = 0x8006;
    cursor->word[1] = 0x10000000;
    cursor->word[2] = 14;
    cursor->word[3] = 0;
    cursor += 1;
    cursor->reg[0] = 0;
    cursor->reg[1] = 63;
    cursor += 1;
    cursor->reg[0] = 0x2000000640000000ULL | (0x24020000 | ((sRenderTextureBase << 5) + 128));
    cursor->reg[1] = 6;
    cursor += 1;
    cursor->reg[0] = 0xFF000FFFULL;
    cursor->reg[1] = 8;
    cursor += 1;
    cursor->reg[0] = 96;
    cursor->reg[1] = 20;
    cursor += 1;
    cursor->reg[0] = 0x50000;
    cursor->reg[1] = 71;
    cursor += 1;
    cursor->reg[0] = 0x101000000ULL | sRenderDepthBase;
    cursor->reg[1] = 78;
    cursor += 1;
    cursor->value[0] = (float)cameraScreenWidth / 2.0f - 2048.0f;
    cursor->value[1] = (float)cameraScreenHeight / 2.0f - 2048.0f;
    cursor->word[2] = 0;
    cursor->word[3] = 0;
    cursor += 1;
    cursor->value[0] = (256.0f / (float)cameraScreenWidth) / 512.0f;
    cursor->value[1] = (256.0f / (float)cameraScreenHeight) / 512.0f;
    cursor->word[2] = 0;
    cursor->word[3] = 0;
    cursor += 1;
    cursor->value[0] = 1.0f;
    cursor->value[1] = 1.0f;
    cursor->word[2] = 0;
    cursor->word[3] = 0;
    cursor += 1;
    xglPacketUploadVu1Data(block, 10, 840);
}

/* Small-data word pxyBlackFogFlag, one of the unnamed .sdata cells between xglDirectQueueRefOffset and pxyFlushDone. */

void pxySetBlackFogFlag(int flag)
{
    pxyBlackFogFlag = flag;
}

void xglMatrixCopy_001D44C4(register void *destination, register void *source)
{
    QuadwordCopyMatrix(destination, source);
}

void xglVectorZero_001D4508(register void *destination)
{
    QuadwordZero(destination);
}

#include "vu0_macro.h"

void xglVectorSubXYZ_001D4530(register void *dst, register void *minuend, register void *subtrahend)
{
    Vu0SubVectorXYZ(dst, minuend, subtrahend);
}
