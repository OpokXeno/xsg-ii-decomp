/*
 * OV01 original TU 76: 0x00aab010..0x00aab888 (6 functions)
 */
#include "common.h"
#include "shared.h"
#include "main/m_math_195.h"
#include "ov01/tu076.h"
#include "main/tu194.h"
#include "quadword.h"
#include "ov01/tu075.h"
#include "main/m_math_193.h"
#include "main/tu196.h"

/* Extent of the axis lines drawn by MEfDrawFrameAxes, along each axis. */
#define MEF_AXIS_LINE_START -0.4f
#define MEF_AXIS_LINE_END 1.2f
/* Half length of the cross arms drawn by MEfDrawPointCross. */
#define MEF_CROSS_HALF_SIZE 0.25f
/* ov01TickCountGet ticks in one turn of the path-end marker (MEfDrawPathEndMarker, one second). */
#define MEF_MARKER_TICKS_PER_TURN 60

void *memset(void *, int, unsigned int);

/* A point whose single axes are offset by index. */
typedef union MEfAxisPoint {
    AlignedVector4 vector;
    float coords[4];
} MEfAxisPoint;

/* A screen point: the Vector4 that MMathRotTransPersClip writes, read back as one quadword. */
typedef union MEfScreenPoint {
    AlignedVector4 vector;
    Quadword whole;
} MEfScreenPoint;

extern Vector4 MMathUnitVector;
extern const Quadword mefAxisColors[5];

/* One 2D corner of a flat marker shape. */
typedef struct MEfCorner {
    float x;
    float y;
} MEfCorner;

extern const MEfCorner mefMarkerCorners[3];
int ov01TickCountGet(void);
Matrix4 *MMathMulMatrix(Matrix4 *out, Matrix4 *rhs, Matrix4 *lhs);

/* Clears the 0xB0-byte record, then stores value in its first word. */
void ov01RecordInit(int *record, int value)
{
    memset(record, 0, 0xB0);
    *record = value;
}

/* 0xB0-byte record: count, eight four-float points, then the two four-float tangents. */
typedef struct HermitePath {
    int count;
    unsigned char unmodeled_04[0x0C];
    float points[8][4];
    float tangent_start[4];
    float tangent_end[4];
} HermitePath;

void MMathCalcHermitePrm(void *, void *, void *, void *, void *, void *);

/* Key choice of MMathCalcHermiteKeyPrm: picks the previous, start, end and next points of key index, then tail-calls MMathCalcHermitePrm. */
void MMathCalcHermiteKeyPrm(HermitePath *path, int index)
{
    float *previous;
    float *point_start;
    float *point_end;
    float *point_next;

    if (index == 0) {
        previous = path->points[0];
    } else {
        previous = path->points[index - 1];
    }
    point_start = path->points[index];
    point_end = path->points[index + 1];
    if (index < path->count - 2) {
        point_next = path->points[index + 2];
    } else {
        point_next = point_end;
    }
    MMathCalcHermitePrm(path->tangent_start, path->tangent_end, previous, point_start, point_end, point_next);
}

/*
 * Argument order as this TU passes it: the parameter comes last.  The
 * definition in main/tu188 takes the parameter second, so the two
 * declarations do not agree.
 */
void MMathCalcHermite(float *, void *, void *, void *, void *, f32);

/* Hermite sample at key record `index` of the table at base. */
void MMathCalcHermiteKey(float *destination, char *base, int index, f32 parameter)
{
    char *record;

    record = base + (index * 0x10);
    MMathCalcHermite(destination, base + 0x90, base + 0xA0, record + 0x10, record + 0x20, parameter);
}

/* GS packet helpers on an MGsPacket. */
/* Writes one A+D register setting. */
static inline void SetRegister(MEfGsQword *qword, unsigned char address, u64 data)
{
    qword->ad.data = data;
    qword->ad.address = address;
}

/* Stores NLOOP into the open GIF tag and moves past its data. */
static inline void CloseTag(MGsPacket *packet, int nloop)
{
    int size;

    if (nloop > 0) {
        size = packet->regCount * nloop + 1;
        packet->current->nloop |= nloop;
        packet->current += size;
        packet->count += size;
    }
}

/*
 * Appends the three axes of a frame: the camera view, multiplied by matrix
 * when one is given. Each axis is a line from -0.4 to 1.2 along it, in the
 * axis colour of mefAxisColors, written when it survives clipping.
 */
void MEfDrawFrameAxes(MGsPacket *packet, Matrix4 *matrix)
{
    Matrix4 view;
    MEfAxisPoint start;
    MEfAxisPoint end;
    MEfScreenPoint fromScreen;
    MEfScreenPoint toScreen;
    MEfGsQword *header;
    MEfGsQword *out;
    Quadword *axisColors;
    struct XglCamera *camera;
    int axis;
    int lineCount;
    int clipFlags;
    int headerRegs;

    camera = MEfCamParams.camera;
    lineCount = 0;
    if (matrix == 0) {
        QuadwordCopyMatrixT0(&view, &MEfCamParams.viewMatrix);
    } else {
        MMathMulMatrix(&view, &MEfCamParams.viewMatrix, matrix);
    }
    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    SetRegister(&header[1], MEF_GS_TEST_1, MEF_GS_TEST_1_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    headerRegs = 3;
    CloseTag(packet, headerRegs);
    header = packet->current;
    out = header + 1;
    header[0].gif.registers = MEF_GS_LINE_REGS;
    header[0].gif.tag = MEF_GS_LINE_TAG;
    packet->regCount = headerRegs;
    axisColors = mefAxisColors;
    for (axis = 0; axis < 3; axis++, axisColors++) {
        __asm__ __volatile__("sqc2 $vf0, 0(%2)\n" : "=m"(start), "=m"(start.vector.w) : "r"(&start), "r"(&start));
        __asm__ __volatile__("sqc2 $vf0, 0(%2)\n" : "=m"(end), "=m"(end.vector.w) : "r"(&end), "r"(&end));
        start.coords[axis] = MEF_AXIS_LINE_START;
        end.coords[axis] = MEF_AXIS_LINE_END;
        clipFlags = MMathRotTransPersClip(&fromScreen.vector, camera, &view, &start.vector);
        clipFlags |= MMathRotTransPersClip(&toScreen.vector, camera, &view, &end.vector);
        if (clipFlags == 0) {
            lineCount++;
            out[0].whole = *axisColors;
            out[1].whole = fromScreen.whole;
            out[2].whole = toScreen.whole;
            out += 3;
        }
    }
    if (lineCount != 0) {
        CloseTag(packet, lineCount);
    }
}

/*
 * Appends a small cross at the point (the unit vector when none is given):
 * an A+D header for test, depth and alpha, then one line of a fixed colour for each axis
 * from -0.25 to +0.25 that survives clipping.
 */
void MEfDrawPointCross(MGsPacket *packet, Vector4 *point)
{
    MEfAxisPoint start;
    MEfAxisPoint end;
    MEfScreenPoint fromScreen;
    MEfScreenPoint toScreen;
    MEfGsQword *header;
    MEfGsQword *out;
    Matrix4 *view;
    struct XglCamera *camera;
    int axis;
    int lineCount;
    int clipFlags;
    int headerRegs;

    camera = MEfCamParams.camera;
    view = &MEfCamParams.viewMatrix;
    lineCount = 0;
    if (point == 0) {
        point = &MMathUnitVector;
    }
    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    SetRegister(&header[1], MEF_GS_TEST_1, MEF_GS_TEST_1_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    headerRegs = 3;
    CloseTag(packet, headerRegs);
    header = packet->current;
    out = header + 1;
    header[0].gif.registers = MEF_GS_LINE_REGS;
    header[0].gif.tag = MEF_GS_LINE_TAG;
    packet->regCount = headerRegs;
    for (axis = 0; axis < 3; axis++) {
        __asm__ __volatile__(
            "lq $8, 0(%2)\n"
            "sq $8, 0(%1)\n"
            : "=m"(start)
            : "r"(&start), "r"(point), "m"(*point)
            : "$8");
        __asm__ __volatile__(
            "lq $8, 0(%4)\n"
            "sq $8, 0(%3)\n"
            : "=m"(end), "=m"(end.vector.z), "=m"(end.vector.w)
            : "r"(&end), "r"(point), "m"(*point)
            : "$8");
        start.coords[axis] -= MEF_CROSS_HALF_SIZE;
        end.coords[axis] += MEF_CROSS_HALF_SIZE;
        clipFlags = MMathRotTransPersClip(&fromScreen.vector, camera, view, &start.vector);
        clipFlags |= MMathRotTransPersClip(&toScreen.vector, camera, view, &end.vector);
        if (clipFlags == 0) {
            lineCount++;
            out[0].whole = 0x000000FF00000000000000FF00000000;
            out[1].whole = fromScreen.whole;
            out[2].whole = toScreen.whole;
            out += 3;
        }
    }
    if (lineCount != 0) {
        CloseTag(packet, lineCount);
    }
}

/*
 * Appends a marker of four triangles around point, turning once a second:
 * the corners of mefMarkerCorners are scaled and rotated in the camera-facing frame,
 * and each quarter turn that survives clipping is written as one triangle.
 */
void MEfDrawPathEndMarker(MGsPacket *packet, Vector4 *point, float scale)
{
    Matrix4 frame;
    AlignedVector4 corner;
    MEfScreenPoint vertices[3];
    MEfGsQword *header;
    MEfGsQword *out;
    int quarterTurn;
    int vertex;
    int clipCount;
    float angle;
    u64 gifTag;
    u64 gifRegisters;
    Quadword triangleColor;
    MEfCameraParams *camera;

    camera = &MEfCamParams;
    angle = (ov01TickCountGet() % MEF_MARKER_TICKS_PER_TURN) * 0.104719758f;
    if (point == 0) {
        point = &MMathUnitVector;
    }
    MMathRotateMatrixZ(&frame, &camera->rotationMatrix, angle);
    MMathScaleMatrixRotS(&frame, &frame, scale);
    __asm__ __volatile__(
        "lqc2 vf1, 0(%0)\n"
        "vmove.w vf1, vf0\n"
        "sqc2 vf1, 48(%1)\n"
        :
        : "r" (point), "r" (frame)
        : "memory");
    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    SetRegister(&header[1], MEF_GS_TEST_1, MEF_GS_TEST_1_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE_FIX);
    CloseTag(packet, 3);
    gifTag = MEF_GS_TRIANGLE_TAG;
    gifRegisters = MEF_GS_TRIANGLE_REGS;
    triangleColor = 0x000000FF000000C00000006000000000;
    for (quarterTurn = 0; quarterTurn < 4; quarterTurn++) {
        clipCount = 0;
        for (vertex = 0; vertex < 3; vertex++) {
            __asm__ __volatile__("sqc2 $vf0, 0(%2)\n" : "=m"(corner), "=m"(corner.w) : "r"(&corner), "r"(&corner));
            corner.x = mefMarkerCorners[vertex].x;
            corner.y = mefMarkerCorners[vertex].y;
            MMathApplyMatrix(&corner, &frame, &corner);
            if (MMathRotTransPersClip(&vertices[vertex].vector, camera->camera, &camera->viewMatrix, &corner) != 0) {
                clipCount++;
            }
        }
        if (clipCount == 0) {
            out = packet->current;
            out[0].gif.tag = gifTag;
            out[0].gif.registers = gifRegisters;
            packet->regCount = 4;
            out[1].whole = triangleColor;
            out[2].whole = vertices[0].whole;
            out[3].whole = vertices[1].whole;
            out[4].whole = vertices[2].whole;
            CloseTag(packet, 1);
        }
        MMathRotateMatrixZ(&frame, &frame, 1.57079637f);
    }
}

/* Colours of the axis lines (the loop in this file reads the first three quadwords) and the marker corners. */
const Quadword mefAxisColors[5] = {
    0x000000ff00000000000000ff000000ff,
    0x000000ff000000ff00000000000000ff,
    0x000000ff000000ff000000ff00000000,
    0x000000ff00000000000000ff00000000,
    0x000000ff000000c00000006000000000,
};

const MEfCorner mefMarkerCorners[3] = {
    { 0.0f, -0.8f },
    { 0.4f, -1.666f },
    { -0.4f, -1.666f },
};
