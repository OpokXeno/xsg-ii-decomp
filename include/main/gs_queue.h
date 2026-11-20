#ifndef INCLUDE_MAIN_GS_QUEUE_H
#define INCLUDE_MAIN_GS_QUEUE_H

#include "main/xgl_camera.h"
#include "shared.h"

/* Functions of src/main/gs_queue.c that other files call. */

/* One rectangle of a draw record: top, left, right, bottom. */
typedef struct GsQueueRect {
    int top;
    int left;
    int right;
    int bottom;
} GsQueueRect;

/*
 * One 0x7C-byte draw record (recordBanks in record_render.c holds three banks of ten), the context the gsQueue batch
 * handlers take (gs_queue.c's batch handlers and quad_draw.c's effect handlers read the same record). The type byte
 * picks the draw handler recordDrawHandlers[type - 1], and that handler decides what the words after it hold: the
 * handlers of types 4 and 8-20 read the gsQueue words of `queue` (+0x04..+0x1F) and pass the record to a
 * gsQueueBegin* handler, then read their own arm from +0x20 on.
 * The type 1 handler reads +0x04 as a stage, the type 3 handler as a ring step, the type 5 handler as a spread, the
 * type 6 handler as an alpha. The type 2 handler reads only tex0 (+0x08) and draws it through a type 20 record of its
 * own (frame 2). The per-type arms are partial: only the words the handlers read.
 */
typedef struct GsQueueContext {
    unsigned char type; /* +0x00: 0 unused, 1..21 picks the draw handler recordDrawHandlers[type - 1] */
    unsigned char unmodeled_01[3];
    union {
        /* Any type: recordSetSlot copies the first ten words without looking at the type. */
        int params[30]; /* +0x04..+0x7B */
        /* The gsQueue words every gsQueueBegin* handler reads (the names the gsQueue handlers use); types 2 and 20 (spriteBatchDraw) add
           a screen and a texture rectangle. */
        struct {
            int frame;           /* +0x04: frame buffer value for gsQueueSetFrame */
            int tex0;            /* +0x08: TEX0 value for gsQueueSetTex0 */
            int color;           /* +0x0C: RGBA color */
            int z;               /* +0x10: depth shared by all vertices */
            int count;           /* +0x14: how many times the effect is queued */
            int textured;        /* +0x18: nonzero queues UV and TEX0 and halves R, G and B */
            int blend_kind;      /* +0x1C: kind for gsQueueSetBlendColor */
            GsQueueRect screen;  /* +0x20 */
            GsQueueRect texture; /* +0x30 */
        } queue;
        /* Type 4 (randomCellsDraw): the number of grid cells to set. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            int cellCount;                         /* +0x20 */
        } gridFill;
        /* Type 8 (recordDrawStrip): two colours over a rectangle, along y when vertical. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            int color0;                            /* +0x20 */
            int color1;                            /* +0x24 */
            int top;                               /* +0x28 */
            int left;                              /* +0x2C */
            int right;                             /* +0x30 */
            int bottom;                            /* +0x34 */
            int vertical;                          /* +0x38 */
        } gradient;
        /* Type 9 (recordDrawTurnColumns): columns of rows quads between bottom and top, turned about the pivot by an
           angle swept across x, from startAngle at startX to endAngle at endX (the x, y and z angles in degrees). A
           column whose angle is below 0 is drawn flat, one past maxAngle is not drawn; the turned rows fade from
           white towards color by angle / maxAngle. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            int color;                             /* +0x20 */
            int bottom;                            /* +0x24 */
            int startX;                            /* +0x28 */
            int endX;                              /* +0x2C */
            int top;                               /* +0x30 */
            int columns;                           /* +0x34 */
            int rows;                              /* +0x38 */
            int endAngle[3];                       /* +0x3C */
            int pivotX;                            /* +0x48: from a column's bottom left corner */
            int pivotY;                            /* +0x4C */
            int startAngle[3];                     /* +0x50 */
            int maxAngle;                          /* +0x5C */
        } turn;
        /* Type 10 (orbitDrawRing): a ring whose outer edge waves by amplitude around radius. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            int endColor;                          /* +0x20 */
            int centerX;                           /* +0x24 */
            int centerY;                           /* +0x28 */
            int radius;                            /* +0x2C */
            int innerRadius;                       /* +0x30 */
            int phaseSpeed;                        /* +0x34 */
            int segments;                          /* +0x38 */
            int amplitude;                         /* +0x3C */
        } ring;
        /* Type 11 (quadDrawPreset): a position preset and a texture rectangle. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            int positionPreset;                    /* +0x20 */
            unsigned char unmodeled_24[0x30 - 0x24];
            GsQueueRect texture;                   /* +0x30 */
        } presetQuad;
        /* Type 12 (quadDrawPresetUv): a screen rectangle and a texture preset. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            GsQueueRect screen;                    /* +0x20 */
            int texturePreset;                     /* +0x30 */
        } presetUvQuad;
        /* Type 13 (circleDrawSegments, circleSpriteCorners): rings of sprites around the screen rectangle. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            GsQueueRect screen;                    /* +0x20 */
            GsQueueRect texture;                   /* +0x30 */
            int radiusStep;                        /* +0x40 */
            int ringCount;                         /* +0x44 */
            int endColor;                          /* +0x48 */
            int startAngle;                        /* +0x4C */
            int segments;                          /* +0x50 */
            int centerDisabled;                    /* +0x54 */
        } circle;
        /* Type 14 (gridDrawCells): the screen rectangle as rows x columns cells, each turned about the pivot by an
           angle that pointRadialAngles (quad_draw.c) lerps from startAngle at the centre to endAngle at radius by the
           squared distance (reversed when invert is set). A cell whose angle is below minAngle is drawn flat, one
           past maxAngle is not drawn; quadDrawTransformed turns a cell about its first corner plus the pivot. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            int endColor;                          /* +0x20 */
            GsQueueRect screen;                    /* +0x24 */
            int columns;                           /* +0x34 */
            int rows;                              /* +0x38 */
            int centerX;                           /* +0x3C */
            int centerY;                           /* +0x40 */
            int radius;                            /* +0x44 */
            int startAngle[3];                     /* +0x48 */
            int endAngle[3];                       /* +0x54 */
            int pivotX;                            /* +0x60: from a cell's top left corner */
            int pivotY;                            /* +0x64 */
            int minAngle;                          /* +0x68 */
            int maxAngle;                          /* +0x6C */
            int invert;                            /* +0x70 */
        } grid;
        /* Type 1 (screenBlurDraw): the stage (0..2) and alpha passed to recordStageColor/recordStageSize, and the Z of
           the full-screen sprite. */
        struct {
            int stage; /* +0x04 */
            int alpha; /* +0x08 */
            int depth; /* +0x0C */
        } fade;
        /* Type 3 (tunnelFanDraw): the ring radius step passed to recordAdvanceRingRadius, and the Z of the fan. */
        struct {
            int step;  /* +0x04 */
            int depth; /* +0x08 */
        } tunnel;
        /* Type 5 (screenSpreadFanDraw): how far the fan's edge points move out, and the Z and fog word. */
        struct {
            int spread; /* +0x04 */
            int z;      /* +0x08 */
        } fan;
        /* Type 6 (screenWarpDraw): the alpha of the RGBAQ word and the Z and fog word. */
        struct {
            int alpha; /* +0x04 */
            int z;     /* +0x08 */
        } warp;
        /* Type 15 (recordSetAlphaFlags): sets the texture alpha expand and alpha test flags when enable is set. */
        struct {
            int enable;                            /* +0x04 */
            int alphaTest;                         /* +0x08 */
            int texAlphaExpand;                    /* +0x0C */
        } alphaFlags;
        /* Type 16 (quadDrawRotating): a wobbling, rotating textured quad; the phase advances every call. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            GsQueueRect screen;                    /* +0x20 */
            GsQueueRect texture;                   /* +0x30 */
            int margin;                            /* +0x40: the quad is grown by this on every side */
            int wobbleX;                           /* +0x44: amplitude of the cosine offset */
            int wobbleY;                           /* +0x48: amplitude of the sine offset */
            int angle;                             /* +0x4C: rotation around the pivot (degrees) */
            int phaseStep;                         /* +0x50: added to the global phase every call */
            int pivotX;                            /* +0x54 */
            int pivotY;                            /* +0x58 */
        } rotating;
        /* Type 17 (pointGridDraw): a screen rectangle as a grid of randomly displaced points. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            GsQueueRect screen;                    /* +0x20 */
            int countX;                            /* +0x30: horizontal displacement range, none when <= 0 */
            int offsetX;                           /* +0x34: added to the horizontal displacement */
            int countY;                            /* +0x38: vertical displacement range, none when <= 0 */
            int offsetY;                           /* +0x3C: added to the vertical displacement */
            int divisions;                         /* +0x40: grid cells per side */
        } pointGrid;
        /* Type 18 (quadOrbitGridDraw): an orbiting grid. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            int endColor;                          /* +0x20: second fade color, 0 for none */
            GsQueueRect screen;                    /* +0x24 */
            int radiusX;                           /* +0x34 */
            unsigned int speedY;                   /* +0x38 */
            int phaseStepA;                        /* +0x3C: added to the global phase per call */
            int radiusY;                           /* +0x40 */
            unsigned int speedX;                   /* +0x44 */
            int phaseStepB;                        /* +0x48: added to the global phase per call */
            int divisions;                         /* +0x4C: grid cells per side */
        } orbitGrid;
        /* Type 19 (ringWaveGridDraw): a ring-wave grid. */
        struct {
            unsigned char queueWords[0x20 - 0x04]; /* +0x04: frame..blend_kind, see queue */
            int endColor;                          /* +0x20: second fade color, 0 for none */
            GsQueueRect screen;                    /* +0x24 */
            int centerX;                           /* +0x34 */
            int centerY;                           /* +0x38 */
            int innerRadius;                       /* +0x3C */
            int outerRadius;                       /* +0x40 */
            int phaseScale;                        /* +0x44 */
            int amplitude;                         /* +0x48 */
            int angleStep;                         /* +0x4C: added to the global wave angle per call */
            int divisions;                         /* +0x50: grid cells per side */
        } ringWave;
        /* Fade lengths read by gsQueueFadeToColor / gsQueueFadeToColorPhases (overlaps the arms above). */
        struct {
            unsigned char unmodeled_04[0x34 - 0x04];
            int phase1Frames;                      /* +0x34: first phase length; with phase2Frames the divisor of the fade */
            unsigned char unmodeled_38[8];
            int phase2Frames;                      /* +0x40 */
            unsigned char unmodeled_44[4];
            int fadeFrames;                        /* +0x48: fade length, the divisor of the fade */
        } colorFade;
    } u;
} GsQueueContext;

void gsQueueSetAlphaFlags(int texAlphaExpand, int alphaTest, int unusedArg);
void gsQueueAddColor(XglPacket *packet, int color, GsQueueContext *context);
void gsQueueAddContextColor(XglPacket *packet, GsQueueContext *context);

/* Batch begin handlers: start a GIF batch of the given primitive type. */
/* Fills a four-vertex screen quad (x, y, z per vertex) for kind 0 or 1: the left or right half of the screen. */
void gsQuadHalfScreenFill(int kind, Vector4 *quad);
void gsQueueBeginTriStrip(GsQueueContext *context);
void gsQueueBeginSprites(GsQueueContext *context);
/* Restart re-runs the stored begin handler; flush sends the queued qwords. */
void gsQueueRestart(GsQueueContext *context);
void gsQueueFlush(XglPacket *packet);
/* Vertex queueing; the Kick forms also flush and restart a full queue. The
   non-kick forms ignore their first argument and the trailing
   batch/context argument, which the original callers pass but never read. */
void gsQueueAddKickVertex(XglPacket *packet, Vector4 *pos, GsQueueContext *context);
void gsQueueAddVertex(XglPacket *unused, Vector4 *pos, GsQueueContext *batch);
void gsQueueAddTexturedKickVertex(XglPacket *packet, Vector4 *pos, Vector4 *uv, GsQueueContext *context);
void gsQueueAddTexturedVertex(XglPacket *unused, Vector4 *pos, Vector4 *uv, GsQueueContext *context);
void gsQueueAddTriangle(XglPacket *packet, DoublewordVector4 *pos, Vector4 *uv, GsQueueContext *context);

#endif /* INCLUDE_MAIN_GS_QUEUE_H */
