#include "common.h"
#include "shared.h"
#include "vu0_macro.h"
#include "main/xgl_2.h"
#include "main/gs_make_tex0.h"
#include "main/record_render.h"


/* Draws the record's rectangle as one two-colour strip, count times. */
void recordDrawStrip(GsQueueContext *record, XglPacket *packet)
{
    Vector4 corners[4];
    int depth;
    int count;
    int color0;
    int color1;
    int top;
    int right;
    int bottom;
    int i;

    depth = record->u.queue.z;
    count = record->u.queue.count;
    color0 = record->u.gradient.color0;
    color1 = record->u.gradient.color1;
    top = record->u.gradient.top;
    right = record->u.gradient.right;
    bottom = record->u.gradient.bottom;
    corners[0].x = (float) record->u.gradient.left;
    corners[0].y = (float) top;
    if (record->u.gradient.vertical == 0)
    {
        corners[1].x = (float) record->u.gradient.left;
        corners[1].y = (float) bottom;
        corners[2].x = (float) right;
        corners[2].y = (float) top;
    }
    else
    {
        corners[1].x = (float) right;
        corners[1].y = (float) top;
        corners[2].x = (float) record->u.gradient.left;
        corners[2].y = (float) bottom;
    }
    corners[3].x = (float) right;
    corners[3].y = (float) bottom;
    corners[0].z = corners[1].z = corners[2].z = corners[3].z = (float) depth;
    gsQueueBeginTriStrip(record);
    for (i = 0; i < count; i++)
    {
        gsQueueAddColor(packet, color0, record);
        gsQueueAddVertex(packet, &corners[0], record);
        gsQueueAddVertex(packet, &corners[1], record);
        gsQueueAddColor(packet, color1, record);
        gsQueueAddKickVertex(packet, &corners[2], record);
        gsQueueAddKickVertex(packet, &corners[3], record);
    }
    gsQueueFlush(packet);
}

void *memset(void *destination, int value, unsigned int count);


/*
 * Draw handler of record type 9: the area between startX and endX, bottom and top as columns of rows quads. The
 * angle recordTurnAngleAt gives at a column's right x decides: past maxAngle (state 2) the column is not drawn,
 * below 0 (state 0) it is drawn flat as one quad from bottom to top (recordDrawFlatColumn), otherwise its rows
 * quads are stacked from the bottom, turned by the angle about the pivot (recordProjectQuad) and coloured from white
 * towards color by angle / maxAngle.
 */
void recordDrawTurnColumns(GsQueueContext *record, XglPacket *packet)
{
    Vector4 corners[4];
    Vector4 angles;
    int color;
    int top;
    int maxAngle;
    int rowHeight;
    int startX;
    int endX;
    int bottom;
    int columns;
    int rows;
    int columnWidth;
    int column;
    int next;
    int state;
    int row;
    f32 columnLeft;
    f32 columnRight;
    f32 rowTop;

    color = record->u.turn.color;
    bottom = record->u.turn.bottom;
    startX = record->u.turn.startX;
    endX = record->u.turn.endX;
    top = record->u.turn.top;
    columns = record->u.turn.columns;
    rows = record->u.turn.rows;
    maxAngle = record->u.turn.maxAngle;
    memset(corners, 0, sizeof(corners));
    if (columns <= 0 || rows <= 0)
    {
        return;
    }
    columnWidth = (endX - startX) / columns;
    rowHeight = (top - bottom) / rows;
    gsQueueBeginTriStrip(record);
    for (column = 0; column < columns; column = next)
    {
        next = column + 1;
        columnRight = (f32) (next * columnWidth + startX);
        state = recordTurnAngleAt(&angles, record, columnRight);
        if (state == 2)
        {
            continue;
        }
        columnLeft = (f32) (column * columnWidth + startX);
        corners[0].x = columnLeft;
        corners[0].y = (f32) bottom;
        corners[0].z = 0.0f;
        corners[1].x = columnRight;
        corners[1].y = (f32) bottom;
        corners[1].z = 0.0f;
        if (state == 0)
        {
            corners[2].x = columnLeft;
            corners[2].y = (f32) top;
            corners[2].z = 0.0f;
            corners[3].x = columnRight;
            corners[3].y = (f32) top;
            corners[3].z = 0.0f;
            recordDrawFlatColumn(packet, corners, record);
        }
        else
        {
            gsQueueAddColor(packet, xglColorLerp(0x80FFFFFF, color, angles.x / (f32) maxAngle), record);
            rowTop = (f32) (bottom + rowHeight);
            corners[2].x = columnLeft;
            corners[2].y = rowTop;
            corners[2].z = 0.0f;
            corners[3].x = columnRight;
            corners[3].y = rowTop;
            corners[3].z = 0.0f;
            recordProjectQuad(corners, record, &angles);
            for (row = 0; row < rows; row++)
            {
                recordDrawBatchQuad(packet, corners, record);
                recordOffsetCorners(corners, 0.0f, (f32) rowHeight);
            }
        }
    }
    gsQueueFlush(packet);
}


/* Turns the four corners by angles about the pivot: corner 0 plus the record's pivot offset. */
void recordProjectQuad(Vector4 *corners, GsQueueContext *record, const Vector4 *angles)
{
    int depth;
    int pivotX;
    int pivotY;

    pivotX = (int) (corners[0].x + (float) record->u.turn.pivotX);
    depth = record->u.queue.z;
    pivotY = (int) (corners[0].y + (float) record->u.turn.pivotY);
    recordRotateCorner(&corners[0], angles, depth, pivotX, pivotY);
    recordRotateCorner(&corners[1], angles, depth, pivotX, pivotY);
    recordRotateCorner(&corners[2], angles, depth, pivotX, pivotY);
    recordRotateCorner(&corners[3], angles, depth, pivotX, pivotY);
}

/* Degrees to radians. */
extern f32 recordTurnDegToRad;

/* Rotates one corner by the x, y and z angles in degrees around the point (x, y); depth is not read. */
void recordRotateCorner(Vector4 *corner, const Vector4 *angles, int depth, int x, int y)
{
    AlignedVector4 offset;
    AlignedVector4 centered;
    Matrix4 rotationX;
    Matrix4 rotationY;
    Matrix4 rotationZ;
    Matrix4 rotation;
    Matrix4 rotationXY;
    AlignedVector4 rotated;
    AlignedVector4 result;
    AlignedVector4 position;

    position = *corner;
    offset.x = (f32) -x;
    offset.y = (f32) -y;
    offset.z = 0.0f;
    Vu0AddVectorXYZ(&centered, &position, &offset);
    xglMatrixUnit(rotationX);
    xglMatrixUnit(rotationY);
    xglMatrixUnit(rotationZ);
    xglMatrixUnit(rotationXY);
    xglMatrixUnit(rotation);
    xglMatrixRotX(rotationX, rotationX, angles->x * recordTurnDegToRad);
    xglMatrixRotY(rotationY, rotationY, angles->y * recordTurnDegToRad);
    xglMatrixRotZ(rotationZ, rotationZ, angles->z * recordTurnDegToRad);
    xglMatrixMul(rotationXY, rotationX, rotationY);
    xglMatrixMul(rotation, rotationZ, rotationXY);
    xglVectorMulMat(&rotated, rotation, &centered);
    Vu0SubVectorXYZ(&result, &rotated, &offset);
    *corner = result;
}

/* Draws one turned quad of a type 9 column count times, at the record's depth. */
void recordDrawBatchQuad(XglPacket *packet, Vector4 *corners, GsQueueContext *record)
{
    float z;
    int count;
    int remaining;
    Vector4 *corner1;
    Vector4 *corner2;
    Vector4 *corner3;

    corner1 = &corners[1];
    corner2 = &corners[2];
    corner3 = &corners[3];
    z = (float) record->u.queue.z;
    count = record->u.queue.count;
    corner3->z = z;
    corner2->z = z;
    corner1->z = z;
    corners->z = z;
    if (count > 0)
    {
        remaining = count;
        do
        {
            remaining = remaining - 1;
            gsQueueAddVertex(packet, corners, record);
            gsQueueAddVertex(packet, corner1, record);
            gsQueueAddKickVertex(packet, corner2, record);
            gsQueueAddKickVertex(packet, corner3, record);
        }
        while (remaining != 0);
    }
}

/* The same body as recordDrawBatchQuad: draws the quad of a type 9 column that is not turned. */
void recordDrawFlatColumn(XglPacket *packet, Vector4 *corners, GsQueueContext *record)
{
    float z;
    int count;
    int remaining;
    Vector4 *corner1;
    Vector4 *corner2;
    Vector4 *corner3;

    corner1 = &corners[1];
    corner2 = &corners[2];
    corner3 = &corners[3];
    z = (float) record->u.queue.z;
    count = record->u.queue.count;
    corner3->z = z;
    corner2->z = z;
    corner1->z = z;
    corners->z = z;
    if (count > 0)
    {
        remaining = count;
        do
        {
            remaining = remaining - 1;
            gsQueueAddVertex(packet, corners, record);
            gsQueueAddVertex(packet, corner1, record);
            gsQueueAddKickVertex(packet, corner2, record);
            gsQueueAddKickVertex(packet, corner3, record);
        }
        while (remaining != 0);
    }
}

f32 recordTurnDegToRad = 0.017453289f;
