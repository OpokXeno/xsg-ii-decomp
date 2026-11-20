#include "common.h"
#include "shared.h"
#include "main/record_render.h"

void *memset(void *destination, int value, unsigned int count);


/*
 * The angle of a type 9 record's turn at x: startAngle at startX to endAngle at endX (the first of the three angles,
 * set alike by recordSetPreset). Returns 0 with angles cleared when the angle is below 0 (the column is drawn flat),
 * 2 with angles cleared but x = maxAngle when it is past maxAngle (the column is not drawn), otherwise 1 with the
 * angle in x, y and z.
 */
int recordTurnAngleAt(Vector4 *angles, GsQueueContext *record, f32 x)
{
    int startX = record->u.turn.startX;
    f32 endAngle = (f32)record->u.turn.endAngle[0];
    int endX = record->u.turn.endX;
    f32 fraction = (x - (f32)startX) / (f32)(endX - startX);
    f32 startAngle = (f32)record->u.turn.startAngle[0];
    int maxAngle = record->u.turn.maxAngle;
    f32 angle = recordLerp(startAngle, endAngle, fraction);
    f32 maxAngleF;

    if (angle < 0.0f) {
        memset(angles, 0, sizeof(*angles));
        return 0;
    }
    maxAngleF = (f32)maxAngle;
    if (maxAngleF < angle) {
        memset(angles, 0, sizeof(*angles));
        angles->x = maxAngleF;
        return 2;
    }
    angles->x = angles->y = angles->z = angle;
    return 1;
}

/* Linear interpolation from `from` to `to` by fraction. */
f32 recordLerp(f32 from, f32 to, f32 fraction)
{
    return ((to - from) * fraction) + from;
}

/* Moves x and y of the four corners of a type 9 quad (recordDrawTurnColumns in record_draw_strip.c) by (dx, dy);
   z and w are untouched. */
void recordOffsetCorners(Vector4 *points, f32 dx, f32 dy)
{
    points[0].y += dy;
    points[0].x += dx;
    points[1].y += dy;
    points[1].x += dx;
    points[2].y += dy;
    points[2].x += dx;
    points[3].y += dy;
    points[3].x += dx;
}
