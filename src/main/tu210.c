#include "common.h"

#include "shared.h"
#include "main/tu204.h"
#include "main/tu207.h"

/*
 * The type 10 record of tu207.h (GsQueueContext, arm u.ring) as orbitDrawRing reads it. Through GsQueueContext.u
 * every read of the record gets alias set 0 (an access through a union), which conflicts with the stack spill slots:
 * sched2 then moves the mflo of 365 / segments and its spill ahead of the record loads (fd.sh orbitDrawRing 161/172,
 * 27 bytes; the same reads through a pointer to the arm, &record->u.ring, cost a second base register, 112/172).
 * With this non-union view of the same words the reads keep the int alias set (172/172).
 */
typedef struct GsQueueRingView {
    unsigned char type;
    unsigned char unmodeled_01[3];
    int frame;       /* +0x04 */
    int tex0;        /* +0x08 */
    int color;       /* +0x0C */
    int z;           /* +0x10 */
    int count;       /* +0x14 */
    int textured;    /* +0x18 */
    int blend_kind;  /* +0x1C */
    int endColor;    /* +0x20 */
    int centerX;     /* +0x24 */
    int centerY;     /* +0x28 */
    int radius;      /* +0x2C */
    int innerRadius; /* +0x30 */
    int phaseSpeed;  /* +0x34 */
    int segments;    /* +0x38 */
    int amplitude;   /* +0x3C */
} GsQueueRingView;

extern int orbitRingPhase; /* wave phase in degrees, advanced by phaseSpeed on every orbitDrawRing call */
extern float orbitDegToRad; /* degrees to radians */

float sinf(float angle);
float cosf(float angle);
void gsQueueBeginTriStrip(GsQueueContext *context);
int setPointFromRecord(Vector4 *destination, int x, int y, const GsQueueContext *record);
void gsQueueAddColor(XglPacket *packet, int color, GsQueueContext *context);
void gsQueueAddKickVertex(XglPacket *packet, Vector4 *vector, GsQueueContext *context);
void gsQueueFlush(XglPacket *packet);

/* Draws a ring as a triangle strip, count times: the outer edge waves with sin(phase) by amplitude around radius,
   the inner edge stays at innerRadius. When endColor is set, the outer vertices get color and the inner ones
   endColor. setPointFromRecord takes z from the record's depth. */
void orbitDrawRing(GsQueueContext *record, XglPacket *packet) {
    GsQueueRingView *ring = (GsQueueRingView *) record;
    Vector4 points[2];
    int pass;
    int color;
    int count;
    int angleStep;
    int endColor;
    int centerX;
    int centerY;
    int radius;
    int innerRadius;
    int amplitude;
    int phaseSpeed;
    int phase;
    int angle;
    float ringRadius;
    int wrappedAngle;
    float radians;

    phaseSpeed = ring->phaseSpeed;
    count = ring->count;
    color = ring->color;
    endColor = ring->endColor;
    centerX = ring->centerX;
    centerY = ring->centerY;
    radius = ring->radius;
    innerRadius = ring->innerRadius;
    amplitude = ring->amplitude;
    angleStep = 365 / ring->segments;
    orbitRingPhase += phaseSpeed;
    gsQueueBeginTriStrip(record);
    for (pass = 0; pass < count; pass++) {
        phase = orbitRingPhase;
        for (angle = 0; angle < 369; angle += angleStep) {
            wrappedAngle = (angle < 360) ? angle : 0;
            radians = wrappedAngle * orbitDegToRad;
            ringRadius = (int) (radius + amplitude * sinf(phase * orbitDegToRad));
            phase += 40;
            setPointFromRecord(&points[0], ringRadius * cosf(radians) + centerX, ringRadius * sinf(radians) + centerY,
                               record);
            setPointFromRecord(&points[1], innerRadius * cosf(radians) + centerX, innerRadius * sinf(radians) + centerY,
                               record);
            if (endColor != 0) {
                gsQueueAddColor(packet, color, record);
            }
            gsQueueAddKickVertex(packet, &points[0], record);
            if (endColor != 0) {
                gsQueueAddColor(packet, endColor, record);
            }
            gsQueueAddKickVertex(packet, &points[1], record);
        }
    }
    gsQueueFlush(packet);
}

/* Stores a point: x and y from the arguments, z from the record's depth; returns that depth. */
int setPointFromRecord(Vector4 *destination, int x, int y, const GsQueueContext *record) {
    int depth;

    depth = record->u.queue.z;
    destination->x = (float) x;
    destination->y = (float) y;
    destination->z = (float) depth;
    return depth;
}

void gsQuadHalfScreenFill(int preset, Vector4 *quad);
void gsQueueAddTexturedVertex(XglPacket *unused, Vector4 *position, Vector4 *uv, GsQueueContext *context);
void gsQueueAddTexturedKickVertex(XglPacket *packet, Vector4 *position, Vector4 *uv, GsQueueContext *context);

/* Draws one textured quad count times as a triangle strip: the positions are the preset quad of gsQuadHalfScreenFill,
   the UVs come from the record's texture rectangle. */
void quadDrawPreset(GsQueueContext *record, XglPacket *packet) {
    Vector4 uvs[4];
    Vector4 positions[4];
    int z;
    int top;
    int left;
    int right;
    int bottom;
    int count;
    int i;

    z = record->u.queue.z;
    top = record->u.presetQuad.texture.top;
    left = record->u.presetQuad.texture.left;
    right = record->u.presetQuad.texture.right;
    bottom = record->u.presetQuad.texture.bottom;
    count = record->u.queue.count;
    gsQuadHalfScreenFill(record->u.presetQuad.positionPreset, positions);
    uvs[0].y = (float) top;
    uvs[0].x = (float) left;
    uvs[0].z = (float) z;
    uvs[1].x = (float) right;
    uvs[1].y = (float) top;
    uvs[1].z = (float) z;
    uvs[2].x = (float) left;
    uvs[2].y = (float) bottom;
    uvs[2].z = (float) z;
    uvs[3].x = (float) right;
    uvs[3].y = (float) bottom;
    uvs[3].z = (float) z;
    gsQueueBeginTriStrip(record);
    for (i = 0; i < count; i++) {
        gsQueueAddTexturedVertex(packet, &positions[0], &uvs[0], record);
        gsQueueAddTexturedVertex(packet, &positions[1], &uvs[1], record);
        gsQueueAddTexturedKickVertex(packet, &positions[2], &uvs[2], record);
        gsQueueAddTexturedKickVertex(packet, &positions[3], &uvs[3], record);
    }
    gsQueueFlush(packet);
}

/* As quadDrawPreset with the roles swapped: the positions come from the record's rectangle, the UVs are the preset
   quad of gsQuadHalfScreenFill. */
void quadDrawPresetUv(GsQueueContext *record, XglPacket *packet) {
    Vector4 uvs[4];
    Vector4 positions[4];
    int z;
    int top;
    int left;
    int right;
    int bottom;
    int count;
    int i;

    z = record->u.queue.z;
    top = record->u.presetUvQuad.screen.top;
    left = record->u.presetUvQuad.screen.left;
    right = record->u.presetUvQuad.screen.right;
    bottom = record->u.presetUvQuad.screen.bottom;
    count = record->u.queue.count;
    gsQuadHalfScreenFill(record->u.presetUvQuad.texturePreset, uvs);
    positions[0].y = (float) top;
    positions[0].x = (float) left;
    positions[0].z = (float) z;
    positions[1].x = (float) right;
    positions[1].y = (float) top;
    positions[1].z = (float) z;
    positions[2].x = (float) left;
    positions[2].y = (float) bottom;
    positions[2].z = (float) z;
    positions[3].x = (float) right;
    positions[3].y = (float) bottom;
    positions[3].z = (float) z;
    gsQueueBeginTriStrip(record);
    for (i = 0; i < count; i++) {
        gsQueueAddTexturedVertex(packet, &positions[0], &uvs[0], record);
        gsQueueAddTexturedVertex(packet, &positions[1], &uvs[1], record);
        gsQueueAddTexturedKickVertex(packet, &positions[2], &uvs[2], record);
        gsQueueAddTexturedKickVertex(packet, &positions[3], &uvs[3], record);
    }
    gsQueueFlush(packet);
}

float xglCosDeg(int degrees);
float xglSinDeg(int degrees);
void gsQueueBeginSprites(GsQueueContext *context);
void circleSpriteCorners(Vector4 *points, int x, int y, const GsQueueContext *record);

/* Draws rings of sprites around the record rectangle, count times: segments sprites per ring, ring (ring + 1) *
   radiusStep away from the centre along directions spaced 360 / segments degrees from startAngle, each ring coloured
   from color towards endColor when endColor is set; a centre sprite comes first unless centerDisabled. Nothing is drawn
   with 15 segments or more (the direction table holds 15). */
void circleDrawSegments(GsQueueContext *record, XglPacket *packet) {
    float directions[15][2];
    Vector4 positions[2];
    Vector4 uvs[2];
    int color;
    int segments;
    int count;
    int uvTop;
    int uvLeft;
    int uvRight;
    int uvBottom;
    int radiusStep;
    int ringCount;
    int endColor;
    int startAngle;
    int centerDisabled;
    int angleStep;
    int i;
    int ring;
    int segment;
    float radius;

    angleStep = 0;
    color = record->u.queue.color;
    segments = record->u.circle.segments;
    count = record->u.queue.count;
    uvTop = record->u.circle.texture.top;
    uvLeft = record->u.circle.texture.left;
    uvRight = record->u.circle.texture.right;
    uvBottom = record->u.circle.texture.bottom;
    radiusStep = record->u.circle.radiusStep;
    ringCount = record->u.circle.ringCount;
    endColor = record->u.circle.endColor;
    startAngle = record->u.circle.startAngle;
    centerDisabled = record->u.circle.centerDisabled;
    if (segments < 15) {
        uvs[0].x = (float) uvLeft;
        uvs[0].y = (float) uvTop;
        uvs[1].x = (float) uvRight;
        uvs[1].y = (float) uvBottom;
        if (segments != 0) {
            angleStep = 360 / segments;
        }
        for (i = 0; i < segments; i++) {
            directions[i][0] = xglCosDeg(startAngle + i * angleStep);
            directions[i][1] = xglSinDeg(startAngle + i * angleStep);
        }
        gsQueueBeginSprites(record);
        for (i = 0; i < count; i++) {
            if (centerDisabled == 0) {
                circleSpriteCorners(positions, 0, 0, record);
                gsQueueAddTexturedVertex(packet, &positions[0], &uvs[0], record);
                gsQueueAddTexturedKickVertex(packet, &positions[1], &uvs[1], record);
            }
            for (ring = 0; ring < ringCount; ring++) {
                if (endColor != 0) {
                    gsQueueAddColor(packet, xglColorLerp(color, endColor, (float) ring / (float) ringCount),
                                    record);
                }
                for (segment = 0; segment < segments; segment++) {
                    radius = (float) (radiusStep * (ring + 1));
                    circleSpriteCorners(positions, (int) (radius * directions[segment][0]),
                                  (int) (radius * directions[segment][1]), record);
                    gsQueueAddTexturedVertex(packet, &positions[0], &uvs[0], record);
                    gsQueueAddTexturedKickVertex(packet, &positions[1], &uvs[1], record);
                }
            }
        }
        gsQueueFlush(packet);
    }
}

/* Sets the two corner points of a sprite: the record rectangle shifted by (x, y), z from the record. */
void circleSpriteCorners(Vector4 *points, int x, int y, const GsQueueContext *record) {
    int top;
    int left;
    int right;
    int bottom;
    int z;

    bottom = record->u.circle.screen.bottom;
    top = record->u.circle.screen.top;
    left = record->u.circle.screen.left;
    right = record->u.circle.screen.right;
    z = record->u.queue.z;
    points[0].x = (float) (left + x);
    points[0].y = (float) (top + y);
    points[1].x = (float) (right + x);
    points[1].y = (float) (bottom + y);
    points[0].z = (float) z;
    points[1].z = (float) z;
}

/* Results of pointFadeColorStep for one grid cell. */
enum {
    GRID_CELL_FLAT,
    GRID_CELL_TRANSFORMED,
    GRID_CELL_HIDDEN
};

/* tu211.c defines it as (out, record, x, y); under the EABI the floats travel in $f12/$f13 either way, but declared and
   called in that order the argument moves of gridDrawCells come out swapped (gate: 6 bytes at 0x278B54). */
int pointFadeColorStep(Vector4 *transform, float x, float y, const GsQueueContext *record);
void quadDrawRepeated(XglPacket *packet, Vector4 *quad, GsQueueContext *record);
void quadDrawTransformed(XglPacket *packet, Vector4 *corners, GsQueueContext *transform, const float *rotation);

/* Draws the record rectangle as rows x columns cells. Once per cell, from the cell's first corner, pointFadeColorStep
   (tu211.c) gives the cell's angles; the cell is then drawn flat, drawn turned (quadDrawTransformed) or skipped.
   endColor, when set, colours the cells (lerped by angle / (maxAngle - minAngle) for turned ones). */
void gridDrawCells(GsQueueContext *record, XglPacket *packet) {
    Vector4 quad[4];
    Vector4 transform;
    int color;
    int columns;
    int endColor;
    int top;
    int left;
    int right;
    int bottom;
    int rows;
    int minAngle;
    int maxAngle;
    int cellWidth;
    int cellHeight;
    int row;
    int column;
    int clip;

    color = record->u.queue.color;
    columns = record->u.grid.columns;
    endColor = record->u.grid.endColor;
    top = record->u.grid.screen.top;
    left = record->u.grid.screen.left;
    right = record->u.grid.screen.right;
    bottom = record->u.grid.screen.bottom;
    rows = record->u.grid.rows;
    minAngle = record->u.grid.minAngle;
    maxAngle = record->u.grid.maxAngle;
    if (columns > 0 && rows > 0) {
        cellWidth = (right - left) / columns;
        cellHeight = (bottom - top) / rows;
        gsQueueBeginTriStrip(record);
        for (row = 0; row < rows; row++) {
            for (column = 0; column < columns; column++) {
                quad[0].x = (float) (column * cellWidth + left);
                quad[0].y = (float) (row * cellHeight + top);
                quad[0].z = 0.0f;
                quad[1].x = (float) ((column + 1) * cellWidth + left);
                quad[1].y = (float) (row * cellHeight + top);
                quad[1].z = 0.0f;
                quad[2].x = (float) (column * cellWidth + left);
                quad[2].y = (float) ((row + 1) * cellHeight + top);
                quad[2].z = 0.0f;
                quad[3].x = (float) ((column + 1) * cellWidth + left);
                quad[3].y = (float) ((row + 1) * cellHeight + top);
                quad[3].z = 0.0f;
                clip = pointFadeColorStep(&transform, quad[0].x, quad[0].y, record);
                if (clip == GRID_CELL_HIDDEN) {
                    continue;
                }
                switch (clip) {
                case GRID_CELL_FLAT:
                    if (endColor != 0) {
                        gsQueueAddColor(packet, color, record);
                    }
                    quadDrawRepeated(packet, quad, record);
                    break;
                case GRID_CELL_TRANSFORMED:
                    if (endColor != 0) {
                        gsQueueAddColor(packet,
                                        xglColorLerp(color, endColor,
                                                     transform.x / (float) (maxAngle - minAngle)),
                                        record);
                    }
                    quadDrawTransformed(packet, quad, record, &transform.x);
                    break;
                }
            }
        }
        gsQueueFlush(packet);
    }
}

int orbitRingPhase = 0;
float orbitDegToRad = 0.017453289f;
