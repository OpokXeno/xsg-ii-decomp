#include "common.h"
#include "main/xgl_2.h"
#include "main/xgl_matrix_axes.h"

/* Fills the four rows of matrix from row0..row3 (matrix[r][i] = row r [i]). */
void xglMatrixFromRows(float matrix[4][4], const float *row0, const float *row1, const float *row2,
                       const float *row3) {
    unsigned int i;

    for (i = 0; i < 4; i++) {
        float *dst = &matrix[0][i];
        const float *src = &row0[i];
        *dst = *src;
    }
    for (i = 0; i < 4; i++) {
        float *dst = &matrix[1][i];
        const float *src = &row1[i];
        *dst = *src;
    }
    for (i = 0; i < 4; i++) {
        float *dst = &matrix[2][i];
        const float *src = &row2[i];
        *dst = *src;
    }
    for (i = 0; i < 4; i++) {
        float *dst = &matrix[3][i];
        const float *src = &row3[i];
        *dst = *src;
    }
}

#include "shared.h"
#include "main/xgl_packet.h"
#include "main/xgl_packet_unpack.h"
#include "main/TiWatSim.h"

/* VIF command words: UNPACK (V4-32), MSCNT (0x17) that starts the VU1 micro-program, and FLUSH. */
#define VIF_CODE_UNPACK_V4_32 0x6C
#define VIF_CODE_MSCNT 0x17000000
#define VIF_CODE_FLUSH 0x11000000
/* Scratchpad addresses: the GIF tag, and the quadword rows that follow it. */
#define SCRATCH_GIF_TAG SCRATCHPAD_BASE
#define SCRATCH_QUAD_ROWS (SCRATCHPAD_BASE + 0x10)
/* GIF tag EOP bit (bit 15 of the tag's low halfword), kept when the tag's loop count is set. */
#define GIF_TAG_EOP 0x8000
/* VU1 addresses of the water sends (the vuAddress argument of xglPacketBeginUnpack). */
#define TIWATER_VU_ROW80 0x3C0
#define TIWATER_VU_ROW70 0x3C1
#define TIWATER_VU_TRANSFORMED 0x3C2
#define TIWATER_VU_MATRIX 0x3C6
#define TIWATER_VU_ORIENTED 0x3CA
#define TIWATER_VU_FIXED 0x3CE
#define TIWATER_VU_POSITION 0x3D2
#define TIWATER_VU_DIRECTION 0x3D6

/* Caches the packet every send in this TU writes to (xglPacketCaptureCurrent stores it). */
extern XglPacket *xglCurrentPacket;

/* Caches the current packet for the sends below. */
void xglPacketCaptureCurrent(void) {
    xglCurrentPacket = xglPacketGetCurrent();
}


/* Opens a VIF UNPACK (V4-32, code 0x6C) of the next data to VU1 address vuAddress. */
void xglPacketBeginUnpack(int vuAddress) {
    sceVif1PkCnt(xglCurrentPacket, 0);
    sceVif1PkOpenUpkCode(xglCurrentPacket, vuAddress & 0xFFFF, VIF_CODE_UNPACK_V4_32, 1, 1);
}


/* Ends the unpack opened by xglPacketBeginUnpack. */
void xglPacketEndUnpack(void) {
    sceVif1PkCloseUpkCode(xglCurrentPacket);
}

/* Count and entries of the reference table added by xglPacketRefTable. */
extern int xglPacketRefCount;
extern u8 xglPacketRefEntries[];

/* Adds a reference to the table xglPacketRefEntries (xglPacketRefCount entries). */
void xglPacketRefTable(void) {
    sceVif1PkRef(xglCurrentPacket, &xglPacketRefEntries, xglPacketRefCount, 0, 0, 0);
}


/* Resets the count and emits one VIF FLUSH (0x11000000). */
void xglPacketFlush(void) {
    sceVif1PkCnt(xglCurrentPacket, 0);
    sceVif1PkAddCode(xglCurrentPacket, VIF_CODE_FLUSH);
}


/* Sends the view matrix of the camera (the camera tiWaterDrawItem passes) multiplied by the
 * object matrix at VU address 0x3C2; tiWaterSendMatrix sends the object matrix alone at 0x3C6. */
void tiWaterSendTransformed(XglCamera *camera, Matrix4 matrix) {
    Matrix4 savedMatrix;

    xglMatrixStackLoad(camera->volume[0].viewMatrix);
    xglMatrixStackMul(matrix);
    xglMatrixStackSave(savedMatrix);
    xglPacketBeginUnpack(TIWATER_VU_TRANSFORMED);
    sceVif1PkAddUpkData128N(xglCurrentPacket, savedMatrix, 4);
    xglPacketEndUnpack();
}

/* Sends the four quads of data at VU address 0x3C6. */
void tiWaterSendMatrix(Matrix4 data) {
    xglPacketBeginUnpack(TIWATER_VU_MATRIX);
    sceVif1PkAddUpkData128N(xglCurrentPacket, data, 4);
    xglPacketEndUnpack();
}

#include "quadword.h"

/* The three axis rows that tiWaterSendOrientedMatrix passes to xglMatrixFromAxes. */
extern Vector4 tiWaterAxes[3];

/* Builds the matrix from the axes, copies data into a local matrix with its row 3 xyz cleared, and sends the stack product at VU address 0x3CA. */
void tiWaterSendOrientedMatrix(Matrix4 data) {
    Matrix4 local;
    Matrix4 stack;

    xglMatrixFromAxes(stack, &tiWaterAxes[0], &tiWaterAxes[1], &tiWaterAxes[2]);
    QuadwordCopyMatrix(local, data);
    local[3][0] = local[3][1] = local[3][2] = 0.0f;
    xglMatrixStackLoad(stack);
    xglMatrixStackMul(local);
    xglMatrixStackSave(stack);
    xglPacketBeginUnpack(TIWATER_VU_ORIENTED);
    sceVif1PkAddUpkData128N(xglCurrentPacket, stack, 4);
    xglPacketEndUnpack();
}

/* The four rows of the fixed matrix sent by tiWaterSendFixedMatrix. */
extern Matrix4 tiWaterFixedMatrix;

/* Sends the matrix built from tiWaterFixedMatrix at VU address 0x3CE. */
void tiWaterSendFixedMatrix(void) {
    Matrix4 matrix;

    xglMatrixFromRows(matrix, tiWaterFixedMatrix[0], tiWaterFixedMatrix[1], tiWaterFixedMatrix[2], tiWaterFixedMatrix[3]);
    xglPacketBeginUnpack(TIWATER_VU_FIXED);
    sceVif1PkAddUpkData128N(xglCurrentPacket, matrix, 4);
    xglPacketEndUnpack();
}

/* Sets the constants 1/512 and 7/16 in the block, then sends row80 at 0x3C0 and row70 at 0x3C1. */
void tiWaterSendParamRows(XglCamera *camera) {
    camera->screen.screenScale.w = 0.001953125f;
    camera->screen.screenOffset.w = 0.4375f;
    xglPacketBeginUnpack(TIWATER_VU_ROW80);
    sceVif1PkAddUpkData128N(xglCurrentPacket, &camera->screen.screenScale, 1);
    xglPacketEndUnpack();
    xglPacketBeginUnpack(TIWATER_VU_ROW70);
    sceVif1PkAddUpkData128N(xglCurrentPacket, &camera->screen.screenOffset, 1);
    xglPacketEndUnpack();
}


void tiWaterSendDirection(XglCamera *camera, Matrix4 transform) {
    Vector4 dir;

    xglPacketBeginUnpack(TIWATER_VU_POSITION);
    sceVif1PkAddUpkData128N(xglCurrentPacket, &camera->travel.position, 1);
    xglPacketEndUnpack();
    dir.x = transform[3][0] - camera->travel.position.x;
    dir.y = transform[3][1] - camera->travel.position.y;
    dir.z = transform[3][2] - camera->travel.position.z;
    dir.w = 1.0f;
    xglVectorNormal(&dir, &dir);
    xglPacketBeginUnpack(TIWATER_VU_DIRECTION);
    sceVif1PkAddUpkData128N(xglCurrentPacket, &dir, 1);
    xglPacketEndUnpack();
}

/* Sends the pair (x, y) at VU address 0x3DA. */
void tiWaterSendItemPair(f32 x, f32 y) {
    f32 xy[2];

    xy[0] = x;
    xy[1] = y;
    xglPacketBeginUnpack(0x3DA);
    sceVif1PkAddUpkData128N(xglCurrentPacket, xy, 1);
    xglPacketEndUnpack();
}

/* Sends the pair (x, y) at VU address 0x3DE. */
void tiWaterSendFramePair(f32 x, f32 y) {
    f32 xy[2];

    xy[0] = x;
    xy[1] = y;
    xglPacketBeginUnpack(0x3DE);
    sceVif1PkAddUpkData128N(xglCurrentPacket, xy, 1);
    xglPacketEndUnpack();
}

/* Sends one water frame: packet cache, reference table, transformed matrix, matrices, parameter rows, then the pair (0.7, 0.05). */
void tiWaterSendFrame(XglCamera *camera, Matrix4 data) {
    xglPacketCaptureCurrent();
    xglPacketRefTable();
    tiWaterSendTransformed(camera, data);
    tiWaterSendMatrix(data);
    tiWaterSendOrientedMatrix(data);
    tiWaterSendFixedMatrix();
    tiWaterSendParamRows(camera);
    tiWaterSendFramePair(0.7f, 0.05f);
}

typedef struct WaterStripGrid {
    u8 unmodeled_00[0x08];
    int rowStride;
    int rowCount;
    u8 unmodeled_10[0x30];
    Vector4 origin;
    float cell;
    u8 unmodeled_54[0x28];
    u32 vertexOffset;
    u8 unmodeled_80[0x04];
    u32 vertexBase;
    u8 unmodeled_88[0x48];
    Matrix4 transform;
} WaterStripGrid;

/* Writes the strip of columns first..last between grid rows row and row + 1 into the scratchpad and sends it. */
void tiWaterSendStrip(int first, unsigned int last, int row, WaterStripGrid *grid) {
    u32 *gifTag = (u32 *)SCRATCH_GIF_TAG;
    DoublewordVector4 *quad = (DoublewordVector4 *)SCRATCH_QUAD_ROWS;
    unsigned int quadCount;
    DoublewordVector4 *vertexCopy;
    Vector4 *gridVertices;
    unsigned int rowStart;
    unsigned int gridColumn;
    float cellX;
    float cellZ;
    XglPacket *packet;

    quadCount = 2 * (last - first) + 2;
    gridVertices = (Vector4 *)(grid->vertexBase + grid->vertexOffset);
    *gifTag = (*gifTag & GIF_TAG_EOP) | (2 * (last - first) + 2);
    cellX = (float)first * grid->cell + grid->origin.x;
    cellZ = (float)row * -grid->cell + grid->origin.z;
    rowStart = row * grid->rowStride;
    packet = xglCurrentPacket;
    vertexCopy = quad + quadCount;
    for (gridColumn = first; gridColumn <= last; gridColumn++) {
        quad->vector.x = cellX;
        quad->vector.y = 0.0f;
        quad->vector.z = cellZ;
        quad->vector.w = 1.0f;
        quad++;
        vertexCopy->vector.x = gridVertices[rowStart + gridColumn].x;
        vertexCopy->vector.y = gridVertices[rowStart + gridColumn].y;
        vertexCopy->vector.z = gridVertices[rowStart + gridColumn].z;
        vertexCopy++;
        quad->vector.x = cellX;
        quad->vector.y = 0.0f;
        quad->vector.z = cellZ - grid->cell;
        quad->vector.w = 1.0f;
        quad++;
        vertexCopy->vector.x = gridVertices[rowStart + grid->rowStride + gridColumn].x;
        vertexCopy->vector.y = gridVertices[rowStart + grid->rowStride + gridColumn].y;
        vertexCopy->vector.z = gridVertices[rowStart + grid->rowStride + gridColumn].z;
        vertexCopy++;
        cellX += grid->cell;
    }
    sceVif1PkOpenUpkCode(packet, 0x8000, VIF_CODE_UNPACK_V4_32, 1, 1);
    sceVif1PkAddUpkData128N(packet, gifTag, 2 * quadCount + 1);
    sceVif1PkCloseUpkCode(packet);
}

/* Strip count and column step of the grid (the strip width in columns). */
extern int tiWaterStripColumns;
/* Same objects as tiWatSimSourcePos and tiWatSimSourcePosTransformed (TiWatSim.h): the original addresses the pair off one
 * symbol (the transformed one at +0x10), and two separate symbols change the relocations (.text DIFF). */
extern Vector4 tiWatSimSourcePos[2];

/* Sends the grid: copies the block into the GIF tag, sends the transformed origin, then one strip per row pair and column group. */
void tiWaterSendGrid(WaterStripGrid *grid, const void *block) {
    unsigned int strips;
    unsigned int row;
    unsigned int strip;
    int first;
    int column;
    XglPacket *packet;

    strips = (grid->rowStride - 1) / tiWaterStripColumns;
    packet = xglCurrentPacket;
    if (grid->rowStride >= 2 && grid->rowCount >= 2) {
        __builtin_memcpy((void *)SCRATCH_GIF_TAG, block, 16);
        xglVectorMulMat(&tiWatSimSourcePos[1], grid->transform, &tiWatSimSourcePos[0]);
        for (row = 0; row < grid->rowCount - 1; row++) {
            sceVif1PkCnt(packet, 0);
            first = 0;
            column = 0;
            for (strip = 0; strip < strips; strip++) {
                column += tiWaterStripColumns;
                tiWaterSendStrip(first, column, row, grid);
                first = column;
                sceVif1PkAddCode(packet, VIF_CODE_MSCNT);
            }
            column = column + (grid->rowStride - tiWaterStripColumns * strips) - 1;
            if (first < column) {
                tiWaterSendStrip(first, column, row, grid);
                sceVif1PkAddCode(packet, VIF_CODE_MSCNT);
                xglPacketFlush();
            }
        }
    }
}

Vector4 tiWaterAxes[3] = {
    { .x = -0.5f, .y = -0.1f, .z = 0.0f, .w = 1.0f },
    { .x = 0.5f, .y = 0.1f, .z = 0.5f, .w = 1.0f },
    { .x = 0.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f },
};

Matrix4 tiWaterFixedMatrix = {
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 128.0f, 128.0f, 128.0f, 1.0f },
};

int tiWaterStripColumns = 29;

XglPacket *xglCurrentPacket = 0;
