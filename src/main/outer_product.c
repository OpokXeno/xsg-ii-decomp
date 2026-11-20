#include "common.h"
#include "shared.h"
#include "main/outer_product.h"
#include "main/xgl_2.h"

#include "vu0_macro.h"

void CalcVerticalVector(const Vector4 *first, const Vector4 *second,
                        const Vector4 *third, Vector4 *vertical)
{
    Vector4 first_edge;
    Vector4 second_edge;
    Vector4 normal;

    Vu0SubVectorXYZ(&first_edge, second, first);
    Vu0SubVectorXYZ(&second_edge, third, first);
    xglVectorOuter(&normal, &first_edge, &second_edge);
    xglVectorOuter(vertical, &first_edge, &normal);
}

float CalcLength(const Vector4 *first, const Vector4 *second, const Vector4 *third)
{
    Vector4 vertical;
    Vector4 delta;
    float length;

    CalcVerticalVector(first, second, third, &vertical);
    xglVectorNormal(&vertical, &vertical);
    Vu0SubVectorXYZ(&delta, first, third);
    xglVectorInner(&length, &vertical, &delta);
    return length;
}

void CalcCrossPoint(const Vector4 *first, const Vector4 *second,
                    const Vector4 *third, const Vector4 *fourth,
                    Vector4 *cross_point)
{
    Vector4 normal;
    /* Reused pair: [1] holds the plane length first, then the length of cross_point;
       [0] holds the dot product of the edge with the normal, then the scale ratio. */
    float intersection_scale[2];

    intersection_scale[1] = CalcLength(first, second, third);
    CalcVerticalVector(first, second, third, &normal);
    xglVectorNormal(&normal, &normal);
    Vu0SubVectorXYZ(cross_point, fourth, third);
    xglVectorInner(&intersection_scale[0], cross_point, &normal);
    intersection_scale[0] = intersection_scale[1] / intersection_scale[0];
    xglVectorLength(&intersection_scale[1], cross_point);
    xglVectorNormal(cross_point, cross_point);
    cross_point->x *= intersection_scale[0] * intersection_scale[1];
    cross_point->y *= intersection_scale[0] * intersection_scale[1];
    cross_point->z *= intersection_scale[0] * intersection_scale[1];
    /* VU0 add: the third point's position is added to the scaled offset in place. */
    __asm__ __volatile__(
        "lqc2 vf20, 0(%0)\n\t"
        "lqc2 vf21, 0(%1)\n\t"
        "vadd.xyzw vf20, vf20, vf21\n\t"
        "sqc2 vf20, 0(%0)"
        :
        : "r"(cross_point), "r"(third)
        : "memory");
}

/* Side of the XZ directed line first->second that point lies on: 1 left (cross > 0), 2 right (cross < 0), 0 on the line. */
int CheckPointLine(const Vector4 *first, const Vector4 *second,
                   const Vector4 *point)
{
    Vector4 first_delta;
    Vector4 second_delta;
    float cross;
    float first_x = first->x;
    int side = 1;

    first_delta.x = second->x - first_x;
    second_delta.x = point->x - first_x;
    first_delta.z = second->z - first->z;
    second_delta.z = point->z - first->z;
    cross = first_delta.x * second_delta.z
          - first_delta.z * second_delta.x;

    if (!(0.0f < cross)) {
        if (cross < 0.0f) {
            side = 2;
        } else {
            side = 0;
        }
    }
    return side;
}

/* Same body as Check_CrossingOver in get.c: two segments cross when each has its ends on opposite sides of the other. */
int CheckCrossLine(const Vector4 *first_start, const Vector4 *first_end,
                   const Vector4 *second_start, const Vector4 *second_end)
{
    int first_start_side = CheckPointLine(first_start, first_end, second_start);
    int first_end_side = CheckPointLine(first_start, first_end, second_end);

    if (first_start_side != first_end_side) {
        int second_start_side = CheckPointLine(second_start, second_end, first_start);
        int second_end_side = CheckPointLine(second_start, second_end, first_end);

        if (second_start_side != second_end_side) {
            return 1;
        }
    }
    return 0;
}

float CheckDist2D(const Vector4 *first, const Vector4 *second)
{
    return __builtin_sqrtf((first->x - second->x) * (first->x - second->x)
                           + (first->z - second->z) * (first->z - second->z));
}
