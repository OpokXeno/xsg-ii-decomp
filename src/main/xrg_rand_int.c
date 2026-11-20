#include "common.h"
#include "shared.h"
#include "main/xrg_rand_int.h"

#include "quadword.h"

#include "main/xgl_2.h"
/* Four-component scale: dst = src * scale (same body as XrgScaleVector in X1 ov12). */
void XrgScaleVector(Vector4 *dst, Vector4 *src, float scale) {
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
    dst->w = src->w * scale;
}

/* Componentwise product of two vectors, all four components. */
void XrgMultiplyVector(Vector4 *dst, Vector4 *lhs, Vector4 *rhs) {
    dst->x = lhs->x * rhs->x;
    dst->y = lhs->y * rhs->y;
    dst->z = lhs->z * rhs->z;
    dst->w = lhs->w * rhs->w;
}

/* In-place accumulate: dst += src, all four components. */
void XrgAccumulateVector(Vector4 *dst, Vector4 *src) {
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
    dst->w += src->w;
}


/* Scales a quaternion to unit length into destination; a zero length is replaced by 1e-8f before the division. */
void XrgQuaternionNormalize(Vector4 *destination, Vector4 *source)
{
    f32 length;

    length = __builtin_sqrtf(source->x * source->x + source->y * source->y + source->z * source->z + source->w * source->w);
    if (length == 0.0f)
    {
        length = 0.00000001f;
    }
    length = 1.0f / length;
    destination->x = source->x * length;
    destination->y = source->y * length;
    destination->z = source->z * length;
    destination->w = source->w * length;
}

f32 cosf(f32);
f32 sinf(f32);

/* Quaternion from Euler angles (x, y, z in radians), halved; the product of the three axis rotations, normalised in place. */
void XrgEulerToQuaternion(Vector4 *quaternion, Vector4 *euler)
{
    Vector4 half_angle;
    Vector4 cosine;
    Vector4 sine;
    f32 cos_x_cos_z;
    f32 cos_x_sin_z;
    f32 sin_x_cos_z;
    f32 sin_x_sin_z;

    half_angle.y = euler->y / 2.0f;
    half_angle.x = euler->x / 2.0f;
    half_angle.z = euler->z / 2.0f;
    cosine.x = cosf(half_angle.x);
    cosine.y = cosf(half_angle.y);
    cosine.z = cosf(half_angle.z);
    sine.x = sinf(half_angle.x);
    sine.y = sinf(half_angle.y);
    sine.z = sinf(half_angle.z);
    cos_x_cos_z = cosine.x * cosine.z;
    cos_x_sin_z = cosine.x * sine.z;
    sin_x_cos_z = sine.x * cosine.z;
    sin_x_sin_z = sine.x * sine.z;
    quaternion->x = cosine.y * sin_x_cos_z - sine.y * cos_x_sin_z;
    quaternion->y = cosine.y * sin_x_sin_z - sine.y * cos_x_cos_z;
    quaternion->z = cosine.y * cos_x_sin_z - sine.y * sin_x_cos_z;
    quaternion->w = cosine.y * cos_x_cos_z - sine.y * sin_x_sin_z;
    XrgQuaternionNormalize(quaternion, quaternion);
}

double fptodp(f32);
double sqrt(double);
double atan2(double, double);
f32 dptofp(double);

static inline f32 XrgArcTangent2(f32 y, f32 x)
{
    return dptofp(atan2(fptodp(y), fptodp(x)));
}

/* Euler angles (x, y, z in radians) from a quaternion, through its rotation matrix. */
void XrgQuaternionToEuler(Vector4 *euler, Vector4 *quaternion)
{
    f32 matrix[3][3];
    Vector4 cosine;
    Vector4 sine;

    matrix[0][0] = 1.0f - 2.0f * quaternion->y * quaternion->y - 2.0f * quaternion->z * quaternion->z;
    matrix[1][0] = 2.0f * quaternion->x * quaternion->y + 2.0f * quaternion->w * quaternion->z;
    matrix[2][0] = 2.0f * quaternion->x * quaternion->z - 2.0f * quaternion->w * quaternion->y;
    sine.y = -matrix[2][0];
    matrix[2][1] = 2.0f * quaternion->y * quaternion->z + 2.0f * quaternion->w * quaternion->x;
    matrix[2][2] = 1.0f - 2.0f * quaternion->x * quaternion->x + 2.0f * quaternion->y * quaternion->y;
    cosine.y = dptofp(sqrt(fptodp(1.0f - sine.y * sine.y)));
    euler->y = XrgArcTangent2(sine.y, cosine.y);
    if (sine.y != 1.0f && sine.y != -1.0f)
    {
        sine.x = matrix[2][1] / cosine.y;
        cosine.x = matrix[2][2] / cosine.y;
        euler->x = XrgArcTangent2(sine.x, cosine.x);
        sine.z = matrix[1][0] / cosine.y;
        cosine.z = matrix[0][0] / cosine.y;
        euler->z = XrgArcTangent2(sine.z, cosine.z);
    }
    else
    {
        matrix[1][1] = 1.0f - 2.0f * quaternion->x * quaternion->x - 2.0f * quaternion->z * quaternion->z;
        matrix[1][2] = 2.0f * quaternion->y * quaternion->z - 2.0f * quaternion->w * quaternion->x;
        euler->x = XrgArcTangent2(-matrix[1][2], matrix[1][1]);
        euler->z = dptofp(atan2(0.0, 1.0));
    }
}

f32 acosf(f32);

/* Axis-angle from a quaternion: angle is twice acosf(w); the axis is xyz divided by sin(angle / 2), with a zero
 * sine clamped to 1e-6. */
void XrgQuaternionToAxisAngle(Vector4 *axis_angle, Vector4 *quaternion)
{
    f32 angle;
    f32 divisor;

    angle = 2.0f * acosf(quaternion->w);
    divisor = sinf(angle * 0.5f);
    if (divisor == 0.0f)
    {
        divisor = 0.000001f;
    }
    axis_angle->x = quaternion->x / divisor;
    axis_angle->y = quaternion->y / divisor;
    axis_angle->z = quaternion->z / divisor;
    axis_angle->w = angle;
}

/* Quaternion from axis-angle: w = cos(angle / 2), xyz = axis * sin(angle / 2). */
void XrgAxisAngleToQuaternion(Vector4 *quaternion, Vector4 *axis_angle)
{
    f32 half_angle;
    f32 sin_half_angle;

    half_angle = axis_angle->w * 0.5f;
    quaternion->w = cosf(half_angle);
    sin_half_angle = sinf(half_angle);
    quaternion->x = axis_angle->x * sin_half_angle;
    quaternion->y = axis_angle->y * sin_half_angle;
    quaternion->z = axis_angle->z * sin_half_angle;
}

float XrgInnerVector(Vector4 *first, Vector4 *second)
{
    return first->x * second->x + first->y * second->y + first->z * second->z;
}

void XrgOuterVector(Vector4 *destination, Vector4 *first, Vector4 *second)
{
    destination->x = first->y * second->z - first->z * second->y;
    destination->y = first->z * second->x - first->x * second->z;
    destination->z = first->x * second->y - first->y * second->x;
}

void XrgQuaternionCombine(Vector4 *result, Vector4 *first, Vector4 *second)
{
    Vector4 scaled_first;
    Vector4 scaled_second;
    Vector4 cross;
    Vector4 sum;
    f32 dot;

    scaled_first = *first;
    XrgScaleVector(&scaled_first, &scaled_first, first->w);
    scaled_second = *second;
    XrgScaleVector(&scaled_second, &scaled_second, second->w);
    XrgOuterVector(&cross, second, first);
    sum.x = scaled_first.x + scaled_second.x + cross.x;
    sum.y = scaled_first.y + scaled_second.y + cross.y;
    sum.z = scaled_first.z + scaled_second.z + cross.z;
    sum.w = scaled_first.w + scaled_second.w + cross.w;
    dot = XrgInnerVector(first, second);
    {
        f32 w = first->w * second->w - dot;
        result->x = sum.x;
        result->y = sum.y;
        result->z = sum.z;
        result->w = w;
    }
}

/* Hamilton product of two quaternions into result; both operands are copied before the first store, since result may alias one of them. */
void XrgQuaternionMultiply(Vector4 *result, Vector4 *first, Vector4 *second)
{
    Vector4 a;
    Vector4 b;

    a = *first;
    b = *second;
    result->x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    result->y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    result->z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    result->w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
}


void XrgQuaternionSlerp(Vector4 *result, Vector4 *from, Vector4 *to, f32 t)
{
    f32 dot;
    f32 theta;
    f32 to_weight;
    f32 from_weight;
    f32 inv_sin;

    xglVectorInner(&dot, from, to);
    if (dot > 0.99f)
    {
        QuadwordCopy(result, from);
        return;
    }
    if (dot < -0.2f)
    {
        dot = -dot;
        to->x = -to->x;
        to->y = -to->y;
        to->z = -to->z;
        to->w = -to->w;
    }
    theta = acosf(dot);
    inv_sin = sinf(theta);
    to_weight = sinf(theta * t);
    from_weight = sinf(theta * (1.0f - t));
    inv_sin = 1.0f / inv_sin;
    result->x = (from->x * from_weight + to->x * to_weight) * inv_sin;
    result->y = (from->y * from_weight + to->y * to_weight) * inv_sin;
    result->z = (from->z * from_weight + to->z * to_weight) * inv_sin;
    result->w = (from->w * from_weight + to->w * to_weight) * inv_sin;
    XrgQuaternionNormalize(result, result);
}

/* Normalised linear interpolation: result = from + blend * (to - from) on all four components,
 * then renormalised by XrgQuaternionNormalize. */
void XrgQuaternionLerpNormalized(Vector4 *result, Vector4 *from, Vector4 *to, f32 blend)
{
    result->x = from->x + blend * (to->x - from->x);
    result->y = from->y + blend * (to->y - from->y);
    result->z = from->z + blend * (to->z - from->z);
    result->w = from->w + blend * (to->w - from->w);
    XrgQuaternionNormalize(result, result);
}
