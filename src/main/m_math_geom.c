#include "common.h"
#include "shared.h"
#include "vu0_macro.h"
#include "main/m_math.h"
#include "main/m_math_geom.h"
#include "main/game_camera.h"
#include "quadword.h"
#include "main/vu0_matrix.h"

#include "main/xgl_2.h"
/* The point of the Hermite track `keys` at `time`: the origin without keys, the first or last key's
 * point outside the keys (w = 1), else the Catmull-Rom curve between the two keys around `time`. */
void MMathEvalHermiteKeys(Vector4 *result, MMathHermiteKey *keys, float time)
{
    HermiteVector tangent[2];
    int frame;
    int first;
    int count;
    int index;
    int next;
    int after;
    int previous;
    float parameter;
    MMathHermiteKey *start = keys;

    frame = time;
    first = -1;
    index = 0;
    count = 0;
    for (; keys[index].key.time != MMATH_HERMITE_END; index++) {
        if (frame < keys[index].key.time) {
            if (first < 0) {
                first = index - 1;
            }
        }
        count++;
    }
    if (count <= 0) {
        Vu0SetOrigin(result);
        return;
    }
    if (count == 1) {
        Vu0MMathCopyPoint(result, start);
        return;
    }
    if (frame < keys[0].key.time) {
        count = 1;
    }
    if (first < 0 || first == count - 1) {
        Vu0MMathCopyPoint(result, &keys[count - 1]);
        return;
    }
    next = first + 1;
    previous = first - (first != 0);
    after = first < count - 2 ? first + 2 : next;
    parameter = (time - keys[first].key.time) / (float) (keys[next].key.time - keys[first].key.time);
    MMathCalcHermitePrm(&tangent[0], &tangent[1], &keys[previous].point, &keys[first].point,
                        &keys[next].point, &keys[after].point);
    MMathCalcHermite(&result->x, &tangent[0], &tangent[1], &keys[first].point, &keys[next].point, parameter);
    result->w = 1.0f;
}

void mmathNoOpUnused(void)
{
}

/* 1 when origin lies inside the triangle first, second, third (seen along the triangle's normal): the normals
 * of the three edge triangles around origin all point the same way. */
int MMathPointInTriangle(const Vector4 *first, const Vector4 *second, const Vector4 *third, const Vector4 *origin)
{
    AlignedVector4 toFirst;
    AlignedVector4 toSecond;
    AlignedVector4 toThird;
    AlignedVector4 normal0;
    AlignedVector4 normal1;
    AlignedVector4 normal2;
    int inside;

    Vu0SubVectorXYZ12(&toFirst, first, origin);
    Vu0SubVectorXYZ12(&toSecond, second, origin);
    Vu0SubVectorXYZ12(&toThird, third, origin);
    MMathVectorCrossProduct(&normal0, &toFirst, &toThird);
    MMathVectorCrossProduct(&normal1, &toSecond, &toFirst);
    MMathVectorCrossProduct(&normal2, &toThird, &toSecond);
    inside = 0;
    if (MMathVectorDotProduct(&normal0, &normal1) >= 0.0f && MMathVectorDotProduct(&normal0, &normal2) >= 0.0f) {
        inside = 1;
    }
    return inside;
}


/* Returns 1 when the XZ distance between the positions of the two actors is within limit, 0 otherwise. */
int MMathIsWithinDistXZ(struct XenoActor *firstActor, struct XenoActor *secondActor, float limit)
{
    if (MMathCalcDistXZ(&firstActor->position, &secondActor->position) <= limit) {
        return 1;
    }
    return 0;
}

/* Returns 1 when the actor at secondActor stands inside the rectangle of XZ corners the actor firstActor
 * carries, turned by its facing angle. points[0] is firstActor's position (the VU0 translation row), points[1]
 * secondActor's, points[2..5] the four corners (limit wide, 50 deep) transformed in place by VU0; the store
 * asm names the stored vector and its w lane (destW) as outputs. */
int MMathIsInFrontRectXZ(struct XenoActor *firstActor, struct XenoActor *secondActor, float limit)
{
    AlignedVector4 points[6];
    AlignedVector4 *originPtr;
    int inside = 0;
    int i;
    AlignedVector4 *source;
    AlignedVector4 *dest;
    float *destW;

    points[2].x = -limit;
    points[2].y = 0.0f;
    points[2].z = 0.0f;
    points[2].w = 1.0f;
    points[3].x = limit;
    points[3].y = 0.0f;
    points[3].z = 0.0f;
    points[3].w = 1.0f;
    points[4].x = -limit;
    points[4].y = 0.0f;
    points[4].z = 50.0f;
    points[4].w = 1.0f;
    points[5].x = limit;
    points[5].y = 0.0f;
    points[5].z = 50.0f;
    points[5].w = 1.0f;

    originPtr = &points[1];
    QuadwordCopyT0(originPtr, &secondActor->position);
    originPtr->y = 0.0f;
    vu0SetCurrentRotationY(firstActor->rotation.y);
    QuadwordCopyT0(&points[0], &firstActor->position);
    points[0].y = 0.0f;
    __asm__ __volatile__("lqc2 vf29, 0(%0)" : : "r"(&points[0]) : "memory");

    source = &points[2];
    destW = &points[2].w;
    dest = &points[2];
    for (i = 3; i >= 0; i--, destW += 4, dest++, source++) {
        __asm__ __volatile__(
            "lqc2 vf1, 0(%0)\n"
            "vmulax.xyzw ACC, vf26, vf1x\n"
            "vmadday.xyzw ACC, vf27, vf1y\n"
            "vmaddaz.xyzw ACC, vf28, vf1z\n"
            "vmaddw.xyzw vf20, vf29, vf1w\n"
            : : "r"(source), "r"(destW) : "memory");
        __asm__ __volatile__("sqc2 vf20, 0(%2)" : "=m"(*dest), "=m"(*destW) : "r"(dest) : "memory");
    }

    for (i = 2; i < 4; i++) {
        originPtr = &points[1];
        inside |= MMathPointInTriangle(&points[i], &points[i + 1], &points[i + 2], originPtr);
    }
    return inside;
}


/* Returns 1 when the actor at secondActor stands inside one of the fan triangles around the actor
 * firstActor, turned by its facing angle: kind - 3 sectors of pi/12 starting at pi/2, each tested at the
 * two radius scales of the table (50 units long). origin is copied with its four lanes named as asm
 * outputs; the matrix rows vf26..vf29 are stored without a memory clobber. */
int MMathIsInFanSectors(struct XenoActor *firstActor, struct XenoActor *secondActor, int kind)
{
    AlignedVector4 translation;
    AlignedVector4 origin;
    AlignedVector4 points[3];
    Matrix4 matrix;
    int sectors = kind - 3;
    float radius;
    int inside = 0;
    int sector;
    int pass;
    int i;
    float start;
    float end;
    float first;
    float second;
    AlignedVector4 *originPtr;

    originPtr = &origin;
    radius = 50.0f;
    __asm__ __volatile__("lq $8, 0(%5)\n"
                         "sq $8, 0(%4)\n"
                         : "=m"(originPtr->x), "=m"(originPtr->y), "=m"(originPtr->z), "=m"(originPtr->w)
                         : "r"(originPtr), "r"(&secondActor->position) : "$8", "memory");
    originPtr->y = 0.0f;
    vu0SetCurrentRotationY(firstActor->rotation.y);
    QuadwordCopyT0(&translation, &firstActor->position);
    translation.y = 0.0f;
    __asm__ __volatile__("lqc2 vf29, 0(%0)" : : "r"(&translation) : "memory");
    __asm__ __volatile__(
        "sqc2 vf26, 0(%0)\n"
        "sqc2 vf27, 16(%0)\n"
        "sqc2 vf28, 32(%0)\n"
        "sqc2 vf29, 48(%0)\n"
        : : "r"(matrix));

    for (sector = 0; sector < sectors; sector++) {
        start = sector * 0.2617994f; /* pi/12 */
        end = start + 0.2617994f;
        for (pass = 0; pass < 2; pass++) {
            first = start * svDefaultScreenParams[pass] + 1.5707964f; /* pi/2 */
            second = end * svDefaultScreenParams[pass] + 1.5707964f;
            points[0].x = 0.0f;
            points[0].y = 0.0f;
            points[0].z = 0.0f;
            points[0].w = 1.0f;
            points[1].x = xglCos(first) * radius;
            points[1].y = 0.0f;
            points[1].z = xglSin(first) * radius;
            points[1].w = 1.0f;
            points[2].x = xglCos(second) * radius;
            points[2].y = 0.0f;
            points[2].z = xglSin(second) * radius;
            points[2].w = 1.0f;
            for (i = 0; i < 3; i++) {
                MMathApplyMatrix(&points[i], &matrix, &points[i]);
            }
            inside |= MMathPointInTriangle(&points[0], &points[1], &points[2], originPtr);
        }
    }
    return inside;
}

/* Returns 1 when the actor at secondActor stands inside the rectangle of XZ corners the actor firstActor * carries, turned by its facing angle. points[0] is firstActor's position (the VU0 translation row), points[1] * secondActor's, points[2..5] the four corners (limit wide, 50 deep) transformed in place by VU0; the store * asm names the stored vector and its w lane (destW) as outputs. */ int MMathIsInFrontRectXZ(struct XenoActor *firstActor, struct XenoActor *secondActor, float limit);
/* Returns 1 when the actor at secondActor stands inside one of the fan triangles around the actor * firstActor, turned by its facing angle: kind - 3 sectors of pi/12 starting at pi/2, each tested at the * two radius scales of the table (50 units long). origin is copied with its four lanes named as asm * outputs; the matrix rows vf26..vf29 are stored without a memory clobber. */ int MMathIsInFanSectors(struct XenoActor *firstActor, struct XenoActor *secondActor, int kind);

/* Tests two actors by kind (ov01 passes the XenoActor roots that two battle units keep at +0x1D0): 1 with
 * MMathIsInFrontRectXZ at half the limit, 2 and 3 by their XZ distance, 4 to 7 with MMathIsInFanSectors. 1 when the test
 * passes; 0 for a missing actor or another kind. */
int MMathActorsInRangeByKind(int kind, struct XenoActor *firstActor, struct XenoActor *secondActor, float limit)
{
    int result = 0;

    if (firstActor == 0 || secondActor == 0) {
        return 0;
    }
    switch (kind) {
    case 1:
        result = MMathIsInFrontRectXZ(firstActor, secondActor, limit * 0.5f);
        break;
    case 2:
    case 3:
        result = MMathIsWithinDistXZ(firstActor, secondActor, limit);
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        result = MMathIsInFanSectors(firstActor, secondActor, kind);
        break;
    }
    return result != 0;
}

/* Screen parameters read by MMathIsInFanSectors: pass 0 and 1 are the fan scale factors (1 and -1). The first three
 * Vector4 slots are read by sv_get_image_item.c as scale and offset values. */
float svDefaultScreenParams[12] = { 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2048.0f, 2048.0f, 268435360.0f, 0.0f };
