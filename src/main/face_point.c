#include "common.h"
#include "main/xgl_camera.h"
#include "shared.h"
#include "main/face_point.h"
#include "main/xgl_2.h"

extern int strcmp(const char *left, const char *right);

typedef int s32;

/*
 * One registered culling volume: the three vectors xglCullingMapSet and
 * xglCullingMapCreate fill in from a 9-float box record, followed by the
 * derived state culling_matrix builds from them and setup_occlusion and
 * check_occlusion read back. position.w and scale.w are set to 1.0f by both
 * writers; rotation.w is never written.
 */
typedef struct CullingVolume {
    Vector4 position;
    Vector4 rotation;
    Vector4 scale;
    Vector4 normalizedDirection;
    Matrix4 transform;
    Matrix4 inverseTransform;
    Vector4 cullingCorners[4];
    Vector4 occlusionPlanes[5];
    float occlusionDepth;
    u8 unmodeled_154[12];
} CullingVolume;

/* The map table stores pointers to separately scaffolded volume and index data. */
typedef struct CullingMapEntry {
    const char *name;
    /* The unchanged loop tests these pointer fields only for nullness. */
    const float *entryCount;
    const s32 *source;
    u8 unmodeled_00C[4];
} CullingMapEntry;

/*
 * Declared as an incomplete array, not a scalar: cc1 -G8 would otherwise
 * classify a plain scalar extern as small data and emit a single
 * gp-relative load, but the original site is two absolute hi/lo
 * instructions; an incomplete-extent declaration keeps the target's
 * unknown size ineligible for that optimization.
 */
static s32 cullingExistFlag[4];
int strcmp(const char *, const char *);
static int s_nIgnoreCulling = 0;
extern const CullingMapEntry s_aCullingMap[14];

/* _ApplyMatrix33: _ApplyMatrix33, verbatim X1 twin (high). */
void _ApplyMatrix33(register Vector4 *destination, register Matrix4 *matrix, register const Vector4 *vector)
{
    __asm__ __volatile__(
        "lqc2 $vf31,0(%2)\n\t"
        "lqc2 $vf27,0(%1)\n\t"
        "lqc2 $vf28,16(%1)\n\t"
        "lqc2 $vf29,32(%1)\n\t"
        "lqc2 $vf30,48(%1)\n\t"
        "vmulax.xyz ACCxyz,$vf27xyz,$vf31x\n\t"
        "vmadday.xyz ACCxyz,$vf28xyz,$vf31y\n\t"
        "vmaddz.xyz $vf31xyz,$vf29xyz,$vf31z\n\t"
        "sqc2 $vf31,0(%0)"
        : : "r"(destination), "r"(matrix), "r"(vector) : "memory");
}

/* _ApplyMatrix: _ApplyMatrix, verbatim X1 twin (high). */
void _ApplyMatrix(register Vector4 *destination, register Matrix4 *matrix, register const Vector4 *vector)
{
    __asm__ __volatile__(
        "lqc2 $vf31,0(%2)\n\t"
        "lqc2 $vf27,0(%1)\n\t"
        "lqc2 $vf28,16(%1)\n\t"
        "lqc2 $vf29,32(%1)\n\t"
        "lqc2 $vf30,48(%1)\n\t"
        "vmulax.xyz ACCxyz,$vf27xyz,$vf31x\n\t"
        "vmadday.xyz ACCxyz,$vf28xyz,$vf31y\n\t"
        "vmaddaz.xyz ACCxyz,$vf29xyz,$vf31z\n\t"
        "vmaddw.xyz $vf31xyz,$vf30xyz,$vf0w\n\t"
        "sqc2 $vf31,0(%0)"
        : : "r"(destination), "r"(matrix), "r"(vector) : "memory");
}

/* _ApplyMatrix2Mat: _ApplyMatrix2Mat, verbatim X1 twin (high). */
void _ApplyMatrix2Mat(register Vector4 *destination, register Matrix4 *matrix1, register Matrix4 *matrix2, register const Vector4 *vector)
{
    __asm__ __volatile__(
        "lqc2 $vf31,0(%3)\n\t"
        "lqc2 $vf27,0(%1)\n\t"
        "lqc2 $vf28,16(%1)\n\t"
        "lqc2 $vf29,32(%1)\n\t"
        "lqc2 $vf30,48(%1)\n\t"
        "vmulax.xyz ACCxyz,$vf27xyz,$vf31x\n\t"
        "vmadday.xyz ACCxyz,$vf28xyz,$vf31y\n\t"
        "vmaddaz.xyz ACCxyz,$vf29xyz,$vf31z\n\t"
        "vmaddw.xyz $vf31xyz,$vf30xyz,$vf0w\n\t"
        "lqc2 $vf27,0(%2)\n\t"
        "lqc2 $vf28,16(%2)\n\t"
        "lqc2 $vf29,32(%2)\n\t"
        "lqc2 $vf30,48(%2)\n\t"
        "vmulax.xyz ACCxyz,$vf27xyz,$vf31x\n\t"
        "vmadday.xyz ACCxyz,$vf28xyz,$vf31y\n\t"
        "vmaddaz.xyz ACCxyz,$vf29xyz,$vf31z\n\t"
        "vmaddw.xyz $vf31xyz,$vf30xyz,$vf0w\n\t"
        "sqc2 $vf31,0(%0)"
        : : "r"(destination), "r"(matrix1), "r"(matrix2), "r"(vector) : "memory");
}

extern const AlignedVector4 cullingForwardAxisVector;
extern const AlignedVector4 cullingCornerLowerLeftVector;
extern const AlignedVector4 cullingCornerLowerRightVector;
extern const AlignedVector4 cullingCornerUpperRightVector;
extern const AlignedVector4 cullingCornerUpperLeftVector;

/* culling_matrix: culling_matrix (medium); builds the volume transform from the corner constants. */
void culling_matrix(CullingVolume *volume)
{
    AlignedVector4 forwardAxis;
    AlignedVector4 lowerLeft;
    AlignedVector4 lowerRight;
    AlignedVector4 upperRight;
    AlignedVector4 upperLeft;

    xglMatrixStackUnit();
    xglMatrixStackTrans(&volume->position);
    xglMatrixStackRotY(volume->rotation.y);
    xglMatrixStackRotX(volume->rotation.x);
    xglMatrixStackRotZ(volume->rotation.z);
    xglMatrixStackScale(&volume->scale);
    xglMatrixStackSave(volume->transform);
    xglMatrixStackInverse();
    xglMatrixStackSave(volume->inverseTransform);
    forwardAxis = cullingForwardAxisVector;
    _ApplyMatrix33(&volume->normalizedDirection, &volume->transform, &forwardAxis);
    xglVectorNormal(&volume->normalizedDirection, &volume->normalizedDirection);
    lowerLeft = cullingCornerLowerLeftVector;
    lowerRight = cullingCornerLowerRightVector;
    upperRight = cullingCornerUpperRightVector;
    upperLeft = cullingCornerUpperLeftVector;
    _ApplyMatrix(&volume->cullingCorners[0], &volume->transform, &lowerLeft);
    _ApplyMatrix(&volume->cullingCorners[1], &volume->transform, &lowerRight);
    _ApplyMatrix(&volume->cullingCorners[2], &volume->transform, &upperRight);
    _ApplyMatrix(&volume->cullingCorners[3], &volume->transform, &upperLeft);
}

/* An empty per-volume debug display hook; the caller still passes the volume. */
void culling_cell_disp(CullingVolume *volume)
{
}

/* _FacePoint: _FacePoint, verbatim X1 twin (high). */
void _FacePoint(Vector4 *destination, const Vector4 *lineStart, const Vector4 *lineEnd, const Vector4 *normal, const Vector4 *planePoint)
{
    __asm__ __volatile__(
        "lqc2 $vf22,0(%3)\n\t"
        "lqc2 $vf20,0(%1)\n\t"
        "lqc2 $vf21,0(%2)\n\t"
        "lqc2 $vf23,0(%4)\n\t"
        "vmulw.xyz vf14xyz,vf22xyz,vf22w\n\t"
        "vmulw.xyz vf14xyz,vf14xyz,vf20w\n\t"
        "vadd.xyz vf23xyz,vf23xyz,vf14xyz\n\t"
        "vaddx.x vf10x,vf0x,vf22x\n\t"
        "vaddy.x vf11x,vf0x,vf22y\n\t"
        "vaddz.x vf12x,vf0x,vf22z\n\t"
        "vsub.xyz vf19xyz,vf21xyz,vf20xyz\n\t"
        "vmulax.x ACCx,vf10x,vf21x\n\t"
        "vmadday.x ACCx,vf11x,vf21y\n\t"
        "vmaddz.x vf17x,vf12x,vf21z\n\t"
        "vmulax.x ACCx,vf10x,vf19x\n\t"
        "vmadday.x ACCx,vf11x,vf19y\n\t"
        "vmaddz.x vf18x,vf12x,vf19z\n\t"
        "vmulax.x ACCx,vf10x,vf23x\n\t"
        "vmadday.x ACCx,vf11x,vf23y\n\t"
        "vmaddz.x vf16x,vf12x,vf23z\n\t"
        "vdiv Q,vf0w,vf18x\n\t"
        "vsub.x vf16x,vf17x,vf16x\n\t"
        "vwaitq\n\t"
        "vmulq.x vf16x,vf16x,Q\n\t"
        "vmulx.xyz vf15xyz,vf19xyz,vf16x\n\t"
        "vsub.xyz vf15xyz,vf21xyz,vf15xyz\n\t"
        "sqc2 vf15,0(%0)"
        : : "r"(destination), "r"(lineStart), "r"(lineEnd), "r"(normal), "r"(planePoint) : "memory");
}

void xglVectorSubXYZ_001B7468(Vector4 *destination, const Vector4 *left, const Vector4 *right);
void xglVectorAddXYZ_001B7430(Vector4 *destination, const Vector4 *left, const Vector4 *right);

float _CheckLine(const Vector4 *offsetStart, const Vector4 *offsetEnd,
                 const Vector4 *endpointStart, const Vector4 *endpointEnd,
                 const Vector4 *planePoint, const Vector4 *planeNormal)
{
    Vector4 adjustedStart;
    Vector4 adjustedEnd;
    Vector4 endpointDelta;
    Vector4 pointFromStart;
    Vector4 normalOffset;

    xglVectorScaleXYZ(&normalOffset, planeNormal,
                      planeNormal->w * planePoint->w * 0.5f);
    xglVectorScaleXYZ(&adjustedStart, offsetStart, planePoint->w * 0.5f);
    xglVectorScaleXYZ(&adjustedEnd, offsetEnd, planePoint->w * 0.5f);
    xglVectorSubXYZ_001B7468(&adjustedStart, endpointStart, &adjustedStart);
    xglVectorSubXYZ_001B7468(&adjustedEnd, endpointEnd, &adjustedEnd);
    xglVectorAddXYZ_001B7430(&adjustedStart, &adjustedStart, &normalOffset);
    xglVectorAddXYZ_001B7430(&adjustedEnd, &adjustedEnd, &normalOffset);
    xglVectorSubXYZ_001B7468(&endpointDelta, &adjustedEnd, &adjustedStart);
    xglVectorSubXYZ_001B7468(&pointFromStart, planePoint, &adjustedStart);
    return endpointDelta.x * pointFromStart.x +
           endpointDelta.y * pointFromStart.y +
           endpointDelta.z * pointFromStart.z;
}

void plane_from_points(const Vector4 *point0, const Vector4 *point1,
                              const Vector4 *point2, Vector4 *plane)
{
    Vector4 edge0;
    Vector4 edge1;
    Vector4 normal;

    edge0.x = point1->x - point0->x;
    edge0.y = point1->y - point0->y;
    edge0.z = point1->z - point0->z;
    edge1.x = point2->x - point0->x;
    edge1.y = point2->y - point0->y;
    edge1.z = point2->z - point0->z;
    xglVectorOuter(&normal, &edge0, &edge1);
    xglVectorNormal(&normal, &normal);
    plane->x = normal.x;
    plane->y = normal.y;
    plane->z = normal.z;
    plane->w = -(normal.x * point0->x + normal.y * point0->y + normal.z * point0->z);
}


void xglVectorZero_001B7408(Vector4 *destination);

void setup_occlusion(CullingVolume *volume, XglCamera *camera)
{
    AlignedVector4 corner0;
    AlignedVector4 corner1;
    AlignedVector4 corner2;
    AlignedVector4 corner3;
    AlignedVector4 zero;

    _ApplyMatrix(&corner0, &camera->travel.inverseTransform, &volume->cullingCorners[0]);
    _ApplyMatrix(&corner1, &camera->travel.inverseTransform, &volume->cullingCorners[1]);
    _ApplyMatrix(&corner2, &camera->travel.inverseTransform, &volume->cullingCorners[2]);
    _ApplyMatrix(&corner3, &camera->travel.inverseTransform, &volume->cullingCorners[3]);

    corner0.z = corner0.z * -1.0f;
    corner1.z = corner1.z * -1.0f;
    corner2.z = corner2.z * -1.0f;
    corner3.z = corner3.z * -1.0f;

    volume->occlusionDepth = corner0.z;
    if (corner1.z < volume->occlusionDepth) {
        volume->occlusionDepth = corner1.z;
    }
    if (corner2.z < volume->occlusionDepth) {
        volume->occlusionDepth = corner2.z;
    }
    if (corner3.z < volume->occlusionDepth) {
        volume->occlusionDepth = corner3.z;
    }

    xglVectorZero_001B7408(&zero);
    plane_from_points(&corner0, &corner1, &corner2, &volume->occlusionPlanes[0]);
    if (0.0f < volume->occlusionPlanes[0].w) {
        plane_from_points(&zero, &corner0, &corner1, &volume->occlusionPlanes[1]);
        plane_from_points(&zero, &corner1, &corner2, &volume->occlusionPlanes[2]);
        plane_from_points(&zero, &corner2, &corner3, &volume->occlusionPlanes[3]);
        plane_from_points(&zero, &corner3, &corner0, &volume->occlusionPlanes[4]);
    } else {
        plane_from_points(&corner2, &corner1, &corner0, &volume->occlusionPlanes[0]);
        plane_from_points(&zero, &corner1, &corner0, &volume->occlusionPlanes[1]);
        plane_from_points(&zero, &corner2, &corner1, &volume->occlusionPlanes[2]);
        plane_from_points(&zero, &corner3, &corner2, &volume->occlusionPlanes[3]);
        plane_from_points(&zero, &corner0, &corner3, &volume->occlusionPlanes[4]);
    }
}

/* check_occlusion: tests a point against the volume's depth bound and its five occlusion planes. */
s32 check_occlusion(CullingVolume *volume, XglCamera *camera, const Vector4 *point)
{
    float distance;
    AlignedVector4 cameraPoint;

    _ApplyMatrix(&cameraPoint, &camera->travel.inverseTransform, point);
    cameraPoint.z = cameraPoint.z * -1.0f;
    distance = cameraPoint.z - cameraPoint.w;
    if (volume->occlusionDepth > distance) {
        return 0;
    }
    if (cameraPoint.x * volume->occlusionPlanes[0].x + cameraPoint.y * volume->occlusionPlanes[0].y
                   + cameraPoint.z * volume->occlusionPlanes[0].z + volume->occlusionPlanes[0].w > -cameraPoint.w) {
        return 0;
    }
    if (cameraPoint.x * volume->occlusionPlanes[1].x + cameraPoint.y * volume->occlusionPlanes[1].y
                   + cameraPoint.z * volume->occlusionPlanes[1].z + volume->occlusionPlanes[1].w > -cameraPoint.w) {
        return 0;
    }
    if (cameraPoint.x * volume->occlusionPlanes[2].x + cameraPoint.y * volume->occlusionPlanes[2].y
                   + cameraPoint.z * volume->occlusionPlanes[2].z + volume->occlusionPlanes[2].w > -cameraPoint.w) {
        return 0;
    }
    if (cameraPoint.x * volume->occlusionPlanes[3].x + cameraPoint.y * volume->occlusionPlanes[3].y
                   + cameraPoint.z * volume->occlusionPlanes[3].z + volume->occlusionPlanes[3].w > -cameraPoint.w) {
        return 0;
    }
    if (cameraPoint.x * volume->occlusionPlanes[4].x + cameraPoint.y * volume->occlusionPlanes[4].y
                   + cameraPoint.z * volume->occlusionPlanes[4].z + volume->occlusionPlanes[4].w > -cameraPoint.w) {
        return 0;
    }
    return 1;
}

/* xglCullingCheck: xglCullingCheck, X1 twin (high). */
static CullingVolume cullingVolumes[10];

s32 xglCullingCheck(XglCamera *camera, const Vector4 *point)
{
    s32 index = 0;
    s32 result = 0;

    if (s_nIgnoreCulling == 0) {
        index = 0;
        while (index < cullingExistFlag[0]) {
            setup_occlusion(&cullingVolumes[index], camera);
            result = check_occlusion(&cullingVolumes[index], camera, point);
            if (result != 0) {
                break;
            }
            index++;
        }
    }
    return result;
}

s32 xglCullingExist(void) {
    return cullingExistFlag[0];
}

/* xglCullingMapLastCheck: no name proposed; no X1 twin with the same map-last-entry call. */
/* Source table of the culling map selected by xglCullingMapSet (0 when none). */
extern const s32 *cullingMapSource;

void xglCullingMapLastCheck(void)
{
    int i;

    if (cullingMapSource != 0) {
        for (i = 0; i < 12; i++) {
            if (cullingMapSource[i] >= 0) {
                continue;
            }
            break;
        }
    }
}

/*
 * The 32-byte map-name buffer: the code stores its first byte as a scalar and
 * passes its address to strcpy and check_culling_map.
 */
typedef union CullingMapNameBuffer {
    signed char first;
    char name[32];
} CullingMapNameBuffer;

static CullingMapNameBuffer cullingMapName;

/* xglCullingMapInit: xglCullingMapInit (high); clears the culling state. */
void xglCullingMapInit(void) {
    cullingExistFlag[0] = 0;
    cullingMapName.first = 0;
    cullingMapSource = 0;
    s_nIgnoreCulling = 0;
}

/*
 * Finds a named culling-map entry in the current map table and returns its
 * index or minus one. The table ends at the first row with entryCount and
 * source both zero.
 */
s32 check_culling_map(const char *name) {
    s32 result = -1;
    s32 i;

    for (i = 0; s_aCullingMap[i].entryCount != 0 || s_aCullingMap[i].source != 0; i++) {
        if (strcmp(s_aCullingMap[i].name, name) == 0) {
            result = i;
            break;
        }
    }
    return result;
}

extern const PxyHeader *cullingMapSetSource;
unsigned int strlen(const char *string);
char *strcpy(char *destination, const char *source);

void xglCullingMapSet(const PxyHeader *source)
{
    s32 boxIndex = 0;
    s32 index;
    const float *box;
    CullingVolume *volume;

    cullingExistFlag[0] = 0;
    cullingMapSource = 0;
    if (strlen(source->name) < 32) {
        strcpy((char *)&cullingMapName, source->name);
    }
    index = check_culling_map((const char *)&cullingMapName);
    if (index < 0) {
        return;
    }
    cullingMapSource = s_aCullingMap[index].source;
    cullingMapSetSource = source;
    if (s_aCullingMap[index].entryCount == 0) {
        return;
    }
    {
    for (;;) {
        box = s_aCullingMap[index].entryCount + (boxIndex++) * 9;
        volume = &cullingVolumes[cullingExistFlag[0]];
        if (cullingExistFlag[0] >= 10) {
            return;
        }
        if (box[6] < 0.0f) {
            return;
        }
        volume->position.x = box[0];
        volume->position.y = box[1];
        volume->position.z = box[2];
        volume->position.w = 1.0f;
        volume->rotation.x = box[3];
        volume->rotation.y = box[4];
        volume->rotation.z = box[5];
        volume->scale.x = box[6];
        volume->scale.y = box[7];
        volume->scale.z = box[8];
        volume->scale.w = 1.0f;
        culling_matrix(volume);
        cullingExistFlag[0]++;
    }
    }
}

/* xglCullingMapCreate: xglCullingMapCreate, X1 twin (high). */
extern const float s_aCullingBase[12];

/* Appends the base volume (s_aCullingBase) to the map while there is room; the
 * body sits in a do { } while (0), which the -O0 nop after the prologue shows. */
void xglCullingMapCreate(void)
{
    do {
        const float *record = s_aCullingBase;
        CullingVolume *volume = &cullingVolumes[cullingExistFlag[0]];

        if (cullingExistFlag[0] >= 10) {
            break;
        }
        volume->position.x = record[0];
        volume->position.y = record[1];
        volume->position.z = record[2];
        volume->position.w = 1.0f;
        volume->rotation.x = record[3];
        volume->rotation.y = record[4];
        volume->rotation.z = record[5];
        volume->scale.x = record[6];
        volume->scale.y = record[7];
        volume->scale.z = record[8];
        volume->scale.w = 1.0f;
        culling_matrix(volume);
        cullingExistFlag[0]++;
    } while (0);
}

/* xglCullingMapDisp: xglCullingMapDisp (medium); loops culling_cell_disp over the volumes. */
void xglCullingMapDisp(void) {
    int i = 0;

    /* The index is stored as zero twice; both stores are in the original. */
    i = 0;
    while (i < cullingExistFlag[0]) {
        culling_cell_disp(&cullingVolumes[i]);
        i++;
    }
}

void xglCullingIgnore(void)
{
    s_nIgnoreCulling = 1;
}

void xglCullingIgnoreOff(void)
{
    s_nIgnoreCulling = 0;
}

/* Empty in this build: the original body is a bare return. */
void xglCullingMapDebug(void)
{
}

#include "quadword.h"

s32 xglCullingCheck(XglCamera *camera, const Vector4 *point);

int nmlModelCalcClipCulling(XglCamera *camera, const Vector4 *point)
{
    int result;

    result = 0;
    result = result | xglCullingCheck(camera, point);
    return result;
}

void xglVectorZero_001B7408(register Vector4 *destination)
{
    QuadwordZero(destination);
}

#include "vu0_macro.h"

void xglVectorAddXYZ_001B7430(register Vector4 *destination, register const Vector4 *left, register const Vector4 *right)
{
    Vu0AddVectorXYZ(destination, left, right);
}

void xglVectorSubXYZ_001B7468(register Vector4 *destination, register const Vector4 *minuend, register const Vector4 *subtrahend)
{
    Vu0SubVectorXYZ(destination, minuend, subtrahend);
}

/* main/face_point.c .rodata 0x0068EDD0..0x0068F000: culling map table (14 rows, the last
 * one is the terminator), its name strings, the base record and the corner vectors. */
extern const char cullingMapNameE2Mil06[16];
extern const char cullingMapNameE2Kuk06[16];
extern const char cullingMapNameE2Dyu01[16];
extern const char cullingMapNameE2Umn01[16];
extern const char cullingMapNameE2Vec02[16];
extern const char cullingMapNameE2Mil02[16];
extern const char cullingMapNameE2Mid02[16];
extern const char cullingMapNameE2Rav03[16];
extern const char cullingMapNameE2Exb02[16];
extern const char cullingMapNameE2Exb05[16];
extern const char cullingMapNameE2Exb06[16];
extern const char cullingMapNameE2Sui02[16];
extern const char cullingMapNameE2Els04[16];
const CullingMapEntry s_aCullingMap[14] = {
    { cullingMapNameE2Mil06, (const float *)cullingMapDataTable, (const s32 *)((const char *)cullingMapDataTable + 0x600) },
    { cullingMapNameE2Kuk06, (const float *)((const char *)cullingMapDataTable + 0x70), 0 },
    { cullingMapNameE2Dyu01, (const float *)((const char *)cullingMapDataTable + 0xB8), 0 },
    { cullingMapNameE2Umn01, (const float *)((const char *)cullingMapDataTable + 0x100), 0 },
    { cullingMapNameE2Vec02, (const float *)((const char *)cullingMapDataTable + 0x190), 0 },
    { cullingMapNameE2Mil02, (const float *)((const char *)cullingMapDataTable + 0x1D8), 0 },
    { cullingMapNameE2Mid02, (const float *)((const char *)cullingMapDataTable + 0x220), 0 },
    { cullingMapNameE2Rav03, (const float *)((const char *)cullingMapDataTable + 0x268), 0 },
    { cullingMapNameE2Exb02, (const float *)((const char *)cullingMapDataTable + 0x320), 0 },
    { cullingMapNameE2Exb05, (const float *)((const char *)cullingMapDataTable + 0x3D8), 0 },
    { cullingMapNameE2Exb06, (const float *)((const char *)cullingMapDataTable + 0x4D8), 0 },
    { cullingMapNameE2Sui02, (const float *)((const char *)cullingMapDataTable + 0x548), 0 },
    { cullingMapNameE2Els04, (const float *)((const char *)cullingMapDataTable + 0x5B8), 0 },
    { (const char *)&xglCullingMapEmptyName, 0, 0 },
};
const char cullingMapNameE2Els04[16] = "E2_ELS04";
const char cullingMapNameE2Sui02[16] = "E2_SUI02";
const char cullingMapNameE2Exb06[16] = "E2_EXB06";
const char cullingMapNameE2Exb05[16] = "E2_EXB05";
const char cullingMapNameE2Exb02[16] = "E2_EXB02";
const char cullingMapNameE2Rav03[16] = "E2_RAV03";
const char cullingMapNameE2Mid02[16] = "E2_MID02";
const char cullingMapNameE2Mil02[16] = "E2_MIL02";
const char cullingMapNameE2Vec02[16] = "E2_VEC02";
const char cullingMapNameE2Umn01[16] = "E2_UMN01";
const char cullingMapNameE2Dyu01[16] = "E2_DYU01";
const char cullingMapNameE2Kuk06[16] = "E2_KUK06";
const char cullingMapNameE2Mil06[16] = "E2_MIL06";
const float s_aCullingBase[12] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
const AlignedVector4 cullingForwardAxisVector = { 0.0f, 0.0f, 1.0f, 1.0f };
const AlignedVector4 cullingCornerLowerLeftVector = { -1.0f, -1.0f, 0.0f, 1.0f };
const AlignedVector4 cullingCornerLowerRightVector = { 1.0f, -1.0f, 0.0f, 1.0f };
const AlignedVector4 cullingCornerUpperRightVector = { 1.0f, 1.0f, 0.0f, 1.0f };
const AlignedVector4 cullingCornerUpperLeftVector = { -1.0f, 1.0f, 0.0f, 1.0f };
