#include "common.h"
#include "main/gs_make_tex0.h"
#include "main/gs_queue.h"
#include "main/xgl_2.h"
#include "shared.h"
#include "main/record_render.h"
#include "vu0_macro.h"
#include "main/oLib.h"

/* The queue context read through a float-aliased view (the original scheduling of ringWavePoint depends on that). */
typedef union EffectParams {
    GsQueueContext context;
    f32 words[24];
} EffectParams;


/* Transforms the four corners around corner 0 plus the screen offset and queues the quad count times. */
void quadDrawTransformed(XglPacket *packet, Vector4 *corners, GsQueueContext *transform, const Vector4 *rotation)
{
    Vector4 quad[4];
    int x;
    int y;
    int value;
    int count;
    int remaining;

    x = (int) (corners[0].x + (float) transform->u.grid.pivotX);
    y = (int) (corners[0].y + (float) transform->u.grid.pivotY);
    value = transform->u.queue.z;
    count = transform->u.queue.count;
    quadRotateCorner(&quad[0], &corners[0], rotation, value, x, y);
    quadRotateCorner(&quad[1], &corners[1], rotation, value, x, y);
    quadRotateCorner(&quad[2], &corners[2], rotation, value, x, y);
    quadRotateCorner(&quad[3], &corners[3], rotation, value, x, y);
    if (count > 0)
    {
        remaining = count;
        do
        {
            remaining = remaining - 1;
            gsQueueAddVertex(packet, &quad[0], transform);
            gsQueueAddVertex(packet, &quad[1], transform);
            gsQueueAddKickVertex(packet, &quad[2], transform);
            gsQueueAddKickVertex(packet, &quad[3], transform);
        }
        while (remaining != 0);
    }
}

/* Copies the four corners, sets their depth from the batch and queues the quad count times. */

void quadDrawRepeated(XglPacket *packet, Vector4 *corners, GsQueueContext *batch)
{
    Vector4 quad[4];
    int z;
    int count;
    int remaining;

    z = batch->u.queue.z;
    count = batch->u.queue.count;
    quad[0] = corners[0];
    quad[1] = corners[1];
    quad[2] = corners[2];
    quad[3] = corners[3];
    quad[3].z = (float) z;
    quad[2].z = (float) z;
    quad[1].z = (float) z;
    quad[0].z = (float) z;
    if (count > 0)
    {
        remaining = count;
        do
        {
            remaining = remaining - 1;
            gsQueueAddVertex(packet, &quad[0], batch);
            gsQueueAddVertex(packet, &quad[1], batch);
            gsQueueAddKickVertex(packet, &quad[2], batch);
            gsQueueAddKickVertex(packet, &quad[3], batch);
        }
        while (remaining != 0);
    }
}

void *memset(void *dest, int value, unsigned int count);

/* Scales a point's squared distance from the centre into a fraction, lerps three channels by it and
 * returns 0, 1 or 2 when the channels pass their limits (cleared on 0 and 2). */
int pointRadialAngles(Vector4 *out, f32 x, f32 y, const GsQueueContext *params) {
    int centerY = params->u.grid.centerY;
    int radius = params->u.grid.radius;
    int centerX = params->u.grid.centerX;
    int invert = params->u.grid.invert;
    int fromB = params->u.grid.startAngle[1];
    int fromA = params->u.grid.startAngle[0];
    int fromC = params->u.grid.startAngle[2];
    int toA = params->u.grid.endAngle[0];
    int toB = params->u.grid.endAngle[1];
    int toC = params->u.grid.endAngle[2];
    int limitA = params->u.grid.minAngle;
    int limitB = params->u.grid.maxAngle;
    f32 dx = x - (f32) centerX;
    f32 dy = y - (f32) centerY;
    f32 fraction = (dx * dx + dy * dy) / (f32) (radius * radius);

    if (invert != 0) {
        fraction = 1.0f - fraction;
    }
    out->x = angleLerp((f32) fromA, (f32) toA, fraction);
    out->y = angleLerp((f32) fromB, (f32) toB, fraction);
    out->z = angleLerp((f32) fromC, (f32) toC, fraction);
    if (out->x < (f32) limitA) {
        memset(out, 0, sizeof(*out));
        return 0;
    }
    if ((f32) limitB < out->x) {
        memset(out, 0, sizeof(*out));
        out->x = 90.0f;
        return 2;
    }
    return 1;
}

/* Linear interpolation from `from` to `to` by fraction; the same body as recordLerp in record_turn_angle.c. */
f32 angleLerp(f32 from, f32 to, f32 fraction) {
    return ((to - from) * fraction) + from;
}

extern f32 degreesToRadians;

/* Rotates source by the three Euler angles (degrees) around the point (x, y) and sets the depth from value. */
void quadRotateCorner(Vector4 *destination, Vector4 *source, const Vector4 *rotation, int value, int x, int y)
{
    AlignedVector4 offset;
    AlignedVector4 position;
    Matrix4 rotX;
    Matrix4 rotY;
    Matrix4 rotZ;
    Matrix4 rotXYZ;
    Matrix4 rotXY;
    AlignedVector4 rotated;
    AlignedVector4 result;
    offset.x = (f32) -x;
    offset.y = (f32) -y;
    offset.z = 0.0f;
    Vu0AddVectorXYZ(&position, source, &offset);
    xglMatrixUnit(rotX);
    xglMatrixUnit(rotY);
    xglMatrixUnit(rotZ);
    /* The original also initialises rotXY and rotXYZ to identity before xglMatrixMul overwrites them (five xglMatrixUnit calls). */
    xglMatrixUnit(rotXY);
    xglMatrixUnit(rotXYZ);
    xglMatrixRotX(rotX, rotX, rotation->x * degreesToRadians);
    xglMatrixRotY(rotY, rotY, rotation->y * degreesToRadians);
    xglMatrixRotZ(rotZ, rotZ, rotation->z * degreesToRadians);
    xglMatrixMul(rotXY, rotX, rotY);
    xglMatrixMul(rotXYZ, rotZ, rotXY);
    xglVectorMulMat(&rotated, rotXYZ, &position);
    Vu0SubVectorXYZ(&result, &rotated, &offset);
    *(AlignedVector4 *) destination = result;
    destination->z = (f32) value;
}

void recordSetAlphaFlags(GsQueueContext *record, XglPacket *unused)
{
    int value;
    int extra;

    value = record->u.alphaFlags.texAlphaExpand;
    extra = record->u.alphaFlags.alphaTest;
    if (record->u.alphaFlags.enable != 0) {
        gsQueueSetAlphaFlags(value, extra, value);
        return;
    }
    gsQueueSetAlphaFlags(0, 0, value);
}

extern f32 quadWobblePhase;          /* wobble phase (degrees), advanced by every quadDrawRotating call */
f32 cosf(f32 x);
f32 sinf(f32 x);

/* Builds the wobbling, rotated quad from quad (the phase advances every call) and queues it count times. */
void quadDrawRotating(GsQueueContext *quad, XglPacket *packet)
{
    Vector4 corners[4];
    Vector4 uvs[4];
    int z;
    int count;
    int top;
    int left;
    int right;
    int bottom;
    int v0;
    int u0;
    int u1;
    int v1;
    int margin;
    int wobbleX;
    int wobbleY;
    int angle;
    int pivotX;
    int pivotY;
    f32 toRad = 0.0174532793f;
    f32 amplitudeX;
    f32 amplitudeY;
    f32 offsetX;
    f32 offsetY;
    f32 leftX;
    f32 topY;
    f32 rightX;
    f32 bottomY;
    int remaining;
    wobbleX = quad->u.rotating.wobbleX;
    z = quad->u.queue.z;
    count = quad->u.queue.count;
    top = quad->u.rotating.screen.top;
    left = quad->u.rotating.screen.left;
    quadWobblePhase = quadWobblePhase + (f32) quad->u.rotating.phaseStep;
    right = quad->u.rotating.screen.right;
    bottom = quad->u.rotating.screen.bottom;
    v0 = quad->u.rotating.texture.top;
    u0 = quad->u.rotating.texture.left;
    u1 = quad->u.rotating.texture.right;
    v1 = quad->u.rotating.texture.bottom;
    margin = quad->u.rotating.margin;
    wobbleY = quad->u.rotating.wobbleY;
    angle = quad->u.rotating.angle;
    amplitudeX = (f32) wobbleX;
    pivotX = quad->u.rotating.pivotX;
    pivotY = quad->u.rotating.pivotY;
    offsetX = amplitudeX * cosf(quadWobblePhase * toRad);
    amplitudeY = (f32) wobbleY;
    offsetY = amplitudeY * sinf(quadWobblePhase * toRad);
    leftX = ((f32) left - (f32) margin) + offsetX;
    topY = ((f32) top - (f32) margin) + offsetY;
    rightX = ((f32) right + (f32) margin) + offsetX;
    bottomY = ((f32) bottom + (f32) margin) + offsetY;
    uvs[2].x = uvs[0].x = (f32) u0;
    uvs[1].y = uvs[0].y = (f32) v0;
    uvs[3].x = uvs[1].x = (f32) u1;
    uvs[3].y = uvs[2].y = (f32) v1;
    corners[2].x = corners[0].x = leftX;
    corners[1].y = corners[0].y = topY;
    corners[3].x = corners[1].x = rightX;
    corners[3].y = corners[2].y = bottomY;
    corners[3].z = corners[2].z = corners[1].z = corners[0].z = (f32) z;
    quadRotateAroundCenter(corners, (f32) angle, (f32) pivotX, (f32) pivotY);
    gsQueueBeginTriStrip(quad);
    if (count > 0)
    {
        remaining = count;
        do
        {
            remaining = remaining - 1;
            gsQueueAddTexturedVertex(packet, &corners[0], &uvs[0], quad);
            gsQueueAddTexturedVertex(packet, &corners[1], &uvs[1], quad);
            gsQueueAddTexturedKickVertex(packet, &corners[2], &uvs[2], quad);
            gsQueueAddTexturedKickVertex(packet, &corners[3], &uvs[3], quad);
        }
        while (remaining != 0);
    }
    gsQueueFlush(packet);
}


/* Rotates four vectors by angle (degrees) around the point (centerX, centerY). */
void quadRotateAroundCenter(Vector4 *vectors, f32 angle, f32 centerX, f32 centerY)
{
    AlignedVector4 center;
    AlignedVector4 offset;
    AlignedVector4 rotated;
    Matrix4 rotation;
    int i;

    center.x = centerX;
    center.y = centerY;
    center.z = 0.0f;
    xglMatrixUnit(rotation);
    xglMatrixRotZ(rotation, rotation, angle * 0.0174532793f);
    for (i = 0; i < 4; i++) {
        Vu0SubVector(&offset, vectors, &center);
        xglVectorMulMat(&rotated, rotation, &offset);
        __asm__ __volatile__("lqc2 vf20, 0(%0)" : : "r"(&rotated) : "memory");
        __asm__ __volatile__("lqc2 vf21, 0(%0)" : : "r"(&center) : "memory");
        __asm__ __volatile__("vadd.xyzw vf20, vf20, vf21" : : : "memory");
        __asm__ __volatile__("sqc2 vf20, 0(%0)" : : "r"(vectors) : "memory");
        vectors++;
    }
}

/* Queues the grid of randomly displaced points repeat times as triangle strips: row by row, each column adds the points of two grid rows (the first column without kick).
   points[0] holds the current grid row and points[1] the next one; the next row is reused as the current row of the following pass. */
void pointGridDraw(GsQueueContext *scatter, XglPacket *packet)
{
    Vector4 points[2][30];
    Vector4 currentUv;
    Vector4 nextUv;
    int column;
    int remaining;
    int divisions = scatter->u.pointGrid.divisions;
    int repeat = scatter->u.queue.count;
    int top = scatter->u.pointGrid.screen.top;
    int left = scatter->u.pointGrid.screen.left;
    int right = scatter->u.pointGrid.screen.right;
    int bottom = scatter->u.pointGrid.screen.bottom;
    int stepX;
    int stepY;
    int row;

    if (divisions > 0)
    {
        randomRefreshSeed();
        stepY = (bottom - top) / divisions;
        stepX = (right - left) / divisions;
        if (repeat > 0)
        {
            remaining = repeat;
            do
            {
                gsQueueBeginTriStrip(scatter);
                for (row = 0; row < divisions; row++)
                {
                    for (column = 0; column <= divisions; column++)
                    {
                        if (row == 0)
                        {
                            pointRandomDisplace(&points[0][column], &currentUv, left + column * stepX, top, scatter);
                        }
                        else
                        {
                            points[0][column] = points[1][column];
                            currentUv.x = (f32) (left + column * stepX);
                            currentUv.y = (f32) (top + row * stepY);
                        }
                        pointRandomDisplace(&points[1][column], &nextUv, left + column * stepX, top + (row + 1) * stepY, scatter);
                        if (column == 0)
                        {
                            gsQueueAddTexturedVertex(packet, &points[0][0], &currentUv, scatter);
                            gsQueueAddTexturedVertex(packet, &points[1][0], &nextUv, scatter);
                        }
                        else
                        {
                            gsQueueAddTexturedKickVertex(packet, &points[0][column], &currentUv, scatter);
                            gsQueueAddTexturedKickVertex(packet, &points[1][column], &nextUv, scatter);
                        }
                    }
                }
                gsQueueFlush(packet);
                remaining = remaining - 1;
            }
            while (remaining != 0);
        }
    }
}


/* Writes the grid point (x, y) displaced by a seeded random amount below countX/countY plus the offsets,
   with the block depth as z, and stores the undisplaced point as its uv. */
void pointRandomDisplace(Vector4 *position, Vector4 *uv, int x, int y, GsQueueContext *rect)
{
    int depth = rect->u.queue.z;
    int countX = rect->u.pointGrid.countX;
    int offsetX = rect->u.pointGrid.offsetX;
    int countY = rect->u.pointGrid.countY;
    int offsetY = rect->u.pointGrid.offsetY;
    int displaceX;
    int displaceY;

    if (countX > 0)
    {
        displaceX = (unsigned int) randomSeededIndexX(&uv->x, countX) % countX + offsetX;
    }
    else
    {
        displaceX = offsetX;
    }
    if (countY > 0)
    {
        displaceY = (unsigned int) randomSeededIndexY(&uv->x, countY) % countY + offsetY;
    }
    else
    {
        displaceY = offsetY;
    }
    position->x = (f32) (x + displaceX);
    position->y = (f32) (y + displaceY);
    position->z = (f32) depth;
    uv->x = (f32) x;
    uv->y = (f32) y;
}

extern int randomSeed;

/* Same body as randomSeededIndexY below; the original binary contains two copies. */
int randomSeededIndexX(f32 *values, int limit) {
    if (recordUpdatePaused == 0) {
        return xglSRand();
    }
    return (int) (((values[1] * 20.0f) / values[0]) + (f32) randomSeed) % limit;
}

/* Seeded random index below limit (or the game RNG when the seed flag is clear). */
int randomSeededIndexY(f32 *values, int limit) {
    if (recordUpdatePaused == 0) {
        return xglSRand();
    }
    return (int) (((values[0] * 20.0f) / values[1]) + (f32) randomSeed) % limit;
}

void randomRefreshSeed(void) {
    if (recordUpdatePaused == 0) {
        randomSeed = xglSRand();
    }
}

extern f32 quadOrbitPhaseA;
extern f32 quadOrbitPhaseB;
extern f32 quadOrbitDegToRad;


/* Queues the orbiting grid repeat times as triangle strips: row by row, each column adds the points of two grid rows (the first column without kick). */
void quadOrbitGridDraw(GsQueueContext *grid, XglPacket *packet)
{
    Vector4 point[2];
    Vector4 center[2];
    int remaining;
    int divisions = grid->u.orbitGrid.divisions;
    int repeat = grid->u.queue.count;
    int colorB = grid->u.orbitGrid.endColor;
    int top = grid->u.orbitGrid.screen.top;
    int left = grid->u.orbitGrid.screen.left;
    int right = grid->u.orbitGrid.screen.right;
    int bottom = grid->u.orbitGrid.screen.bottom;
    int phaseStepA = grid->u.orbitGrid.phaseStepA;
    int phaseStepB = grid->u.orbitGrid.phaseStepB;
    int stepX;
    int stepY;
    int row;
    int column;
    f32 height0;
    f32 height1;

    if (divisions > 0)
    {
        stepY = (bottom - top) / divisions;
        stepX = (right - left) / divisions;
        if (recordUpdatePaused == 0)
        {
            quadOrbitPhaseA = quadOrbitPhaseA + (f32) phaseStepA;
            quadOrbitPhaseA = angleWrap360(quadOrbitPhaseA);
            quadOrbitPhaseB = quadOrbitPhaseB + (f32) phaseStepB;
            quadOrbitPhaseB = angleWrap360(quadOrbitPhaseB);
        }
        if (repeat > 0)
        {
            remaining = repeat;
            do
            {
                gsQueueBeginTriStrip(grid);
                for (row = 0; row < divisions; row++)
                {
                    for (column = 0; column <= divisions; column++)
                    {
                        height0 = quadOrbitPoint(&point[0], &center[0], left + column * stepX, top + row * stepY, grid);
                        height1 = quadOrbitPoint(&point[1], &center[1], left + column * stepX, top + (row + 1) * stepY, grid);
                        if (column == 0)
                        {
                            if (colorB != 0)
                            {
                                gsQueueFadeToColorPhases(packet, colorB, grid, height0);
                            }
                            gsQueueAddTexturedVertex(packet, &point[0], &center[0], grid);
                            if (colorB != 0)
                            {
                                gsQueueFadeToColorPhases(packet, colorB, grid, height1);
                            }
                            gsQueueAddTexturedVertex(packet, &point[1], &center[1], grid);
                        }
                        else
                        {
                            if (colorB != 0)
                            {
                                gsQueueFadeToColorPhases(packet, colorB, grid, height0);
                            }
                            gsQueueAddTexturedKickVertex(packet, &point[0], &center[0], grid);
                            if (colorB != 0)
                            {
                                gsQueueFadeToColorPhases(packet, colorB, grid, height1);
                            }
                            gsQueueAddTexturedKickVertex(packet, &point[1], &center[1], grid);
                        }
                    }
                }
                gsQueueFlush(packet);
                remaining = remaining - 1;
            }
            while (remaining != 0);
        }
    }
}

/* Evaluates the orbit around (centerX, centerY): writes the position and height to position and the center to center. */
f32 quadOrbitPoint(Vector4 *position, Vector4 *center, int centerX, int centerY, GsQueueContext *orbit)
{
    int height;
    int top;
    int left;
    int radiusX;
    int radiusY;
    f32 speedY;
    f32 speedX;
    f32 radiusXf;
    f32 radiusYf;
    f32 centerXf;
    f32 centerYf;
    int stepX;
    int stepY;
    f32 angleX;
    f32 angleY;
    f32 px;
    f32 py;
    f32 sumX;
    f32 sumY;

    height = orbit->u.queue.z;
    top = orbit->u.orbitGrid.screen.top;
    left = orbit->u.orbitGrid.screen.left;
    radiusX = orbit->u.orbitGrid.radiusX;
    speedY = (f32) (orbit->u.orbitGrid.speedY * 0.1);
    radiusY = orbit->u.orbitGrid.radiusY;
    speedX = (f32) (orbit->u.orbitGrid.speedX * 0.1);
    radiusYf = (f32) radiusY;
    radiusXf = (f32) radiusX;
    centerXf = (f32) centerX;
    centerYf = (f32) centerY;
    stepY = (int) (quadOrbitPhaseA + speedY * (f32) (centerX - left));
    stepX = (int) (quadOrbitPhaseB + speedX * (f32) (centerY - top));
    angleX = (f32) stepX;
    angleY = (f32) stepY;
    position->x = centerXf + radiusXf * cosf(angleX * quadOrbitDegToRad);
    position->y = centerYf + radiusYf * cosf(angleY * quadOrbitDegToRad);
    position->z = (f32) height;
    center->x = centerXf;
    center->y = centerYf;
    sumY = radiusYf * cosf(angleY * quadOrbitDegToRad);
    sumX = radiusXf * cosf(angleX * quadOrbitDegToRad);
    return sumY + sumX;
}

/* Wraps an angle in degrees into the range 0 to 360. */
f32 angleWrap360(f32 angle) {
    if (angle > 360.0f) {
        angle -= 360.0f;
    }
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle;
}

/* Fades from the context color toward colorB by |t| / (phase1 + phase2) frames, then queues it. */
void gsQueueFadeToColorPhases(XglPacket *packet, int colorB, GsQueueContext *context, f32 t) {
    int from = context->u.queue.color;
    f32 divisor = (f32) (context->u.colorFade.phase1Frames + context->u.colorFade.phase2Frames);
    if (t > 0.0f) {
        int color = xglColorLerp(from, colorB, __builtin_fabsf(t / divisor));
        gsQueueAddColor(packet, color, context);
        return;
    }
    gsQueueAddContextColor(packet, context);
}

extern int ringWaveAngle;

/* Draws the ring-wave grid repeat times: one triangle strip per row, each column adding the points of two grid rows (the first column without kick). */
void ringWaveGridDraw(GsQueueContext *grid, XglPacket *packet)
{
    Vector4 position[2];
    Vector4 flat[2];
    int pass;
    int divisions = grid->u.ringWave.divisions;
    int repeat = grid->u.queue.count;
    int colorB = grid->u.ringWave.endColor;
    int top = grid->u.ringWave.screen.top;
    int left = grid->u.ringWave.screen.left;
    int right = grid->u.ringWave.screen.right;
    int bottom = grid->u.ringWave.screen.bottom;
    int angleStep = grid->u.ringWave.angleStep;
    int stepX;
    int stepY;
    int row;
    int column;
    f32 height0;
    f32 height1;

    if (divisions > 0)
    {
        if (recordUpdatePaused == 0)
        {
            ringWaveAngle = angleAdvance(ringWaveAngle, angleStep);
        }
        stepY = (bottom - top) / divisions;
        stepX = (right - left) / divisions;
        if (repeat > 0)
        {
            pass = repeat;
            do
            {
                for (row = 0; row < divisions; row++)
                {
                    gsQueueBeginTriStrip(grid);
                    for (column = 0; column <= divisions; column++)
                    {
                        height0 = ringWavePoint(&position[0], &flat[0], left + column * stepX, top + row * stepY, grid);
                        height1 = ringWavePoint(&position[1], &flat[1], left + column * stepX, top + (row + 1) * stepY, grid);
                        if (column == 0)
                        {
                            if (colorB != 0)
                            {
                                gsQueueFadeToColor(packet, colorB, grid, height0);
                            }
                            gsQueueAddTexturedVertex(packet, &position[0], &flat[0], grid);
                            if (colorB != 0)
                            {
                                gsQueueFadeToColor(packet, colorB, grid, height1);
                            }
                            gsQueueAddTexturedVertex(packet, &position[1], &flat[1], grid);
                        }
                        else
                        {
                            if (colorB != 0)
                            {
                                gsQueueFadeToColor(packet, colorB, grid, height0);
                            }
                            gsQueueAddTexturedKickVertex(packet, &position[0], &flat[0], grid);
                            if (colorB != 0)
                            {
                                gsQueueFadeToColor(packet, colorB, grid, height1);
                            }
                            gsQueueAddTexturedKickVertex(packet, &position[1], &flat[1], grid);
                        }
                    }
                    gsQueueFlush(packet);
                }
                pass = pass - 1;
            }
            while (pass != 0);
        }
    }
}


int angleAdvance(int angle, int delta) {
    int sum;

    sum = angle + delta;
    return (int) angleWrap360Copy(sum, (f32) sum);
}

extern f32 ringWaveDegToRad;          /* degrees to radians */

/* Computes the ring-wave point for grid position (x, y): outside the ring between innerRadius and outerRadius
   (or for an empty ring) the point stays flat; inside it, y is lifted by amplitude * sin(global angle + phase),
   the phase growing from 0 at the inner edge to 180 * phaseScale at the outer edge. Writes the waved point to
   wave and the flat point to flat, and returns the lift. */
f32 ringWavePoint(Vector4 *wave, Vector4 *flat, int x, int y, GsQueueContext *ring)
{
    EffectParams *params = (EffectParams *) ring;
    f32 offset;
    int centerX;
    int centerY;
    int innerRadius;
    int outerRadius;
    int phaseScale;
    int z;
    int amplitude;
    int innerSq;
    int outerSq;
    int deltaX;
    int deltaY;
    int squareX;
    int squareY;
    int distanceSq;

    /* The outer radius is read as a plain int word; the other words go through the float-aliased view. */
    innerRadius = params->context.u.ringWave.innerRadius;
    outerRadius = ring->u.ringWave.outerRadius;
    centerX = params->context.u.ringWave.centerX;
    centerY = params->context.u.ringWave.centerY;
    z = params->context.u.queue.z;
    phaseScale = params->context.u.ringWave.phaseScale;
    amplitude = params->context.u.ringWave.amplitude;
    innerSq = innerRadius * innerRadius;
    outerSq = outerRadius * outerRadius;
    if (innerRadius == outerRadius)
    {
        return 0.0f;
    }
    deltaX = x - centerX;
    squareX = deltaX * deltaX;
    deltaY = y - centerY;
    squareY = deltaY * deltaY;
    distanceSq = squareX + squareY;
    if (distanceSq < innerSq || outerSq < distanceSq)
    {
        wave->x = (f32) x;
        wave->y = (f32) y;
        wave->z = (f32) z;
        flat->x = (f32) x;
        flat->y = (f32) y;
        return 0.0f;
    }
    {
        f32 flatX = (f32) x;
        f32 height = (f32) amplitude;
        f32 phase = ((f32) (distanceSq - innerSq) / (f32) (outerSq - innerSq)) * 180.0f * (f32) phaseScale;

        wave->x = flatX;
        offset = height * sinf(((f32) ringWaveAngle + phase) * ringWaveDegToRad);
        wave->y = (f32) y - offset;
        wave->z = (f32) z;
        flat->x = flatX;
        flat->y = (f32) y;
    }
    return offset;
}

/* Same body as angleWrap360 above; the original binary holds a second copy that takes (and ignores) a leading int. */
f32 angleWrap360Copy(int sum, f32 angle) {
    if (angle > 360.0f) {
        angle -= 360.0f;
    }
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle;
}

/* Fades from the context color toward colorB by |t| / fade_frames, then queues it. */
void gsQueueFadeToColor(XglPacket *packet, int colorB, GsQueueContext *context, f32 t) {
    int from = context->u.queue.color;
    int divisor = context->u.colorFade.fadeFrames;
    if (t > 0.0f) {
        int color = xglColorLerp(from, colorB, __builtin_fabsf(t / (f32) divisor));
        gsQueueAddColor(packet, color, context);
        return;
    }
    gsQueueAddContextColor(packet, context);
}

/* Scalar data of the original .sdata section, in address order. */
f32 degreesToRadians = 0.0174532887f;
f32 quadWobblePhase = 0;
int randomSeed = 0;
f32 quadOrbitPhaseA = 0;
f32 quadOrbitPhaseB = 0;
f32 quadOrbitDegToRad = 0.0174532887f;
int ringWaveAngle = 0;
f32 ringWaveDegToRad = 0.0174532887f;
