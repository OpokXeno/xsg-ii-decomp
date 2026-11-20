#ifndef INCLUDE_MAIN_RECORD_RENDER_H
#define INCLUDE_MAIN_RECORD_RENDER_H

#include "main/gs_queue.h"

/* The draw record of src/main/record_render.c and the functions other files call. */



void recordPauseUpdates(void);
void recordClearBanks(void);
int recordGridCellCount(void);
void recordSetPreset(int bankIndex, int preset, int depth, int *params);

/* The draw handlers recordDrawHandlers (record_render.c) points at, one per record type (1..21; the type 15 and 21
   slots take the record and packet and ignore them). */
void screenBlurDraw(GsQueueContext *record, XglPacket *unused);
void screenTextureDraw(GsQueueContext *record, XglPacket *packet);
void tunnelFanDraw(GsQueueContext *record, XglPacket *unused);
void randomCellsDraw(GsQueueContext *record, XglPacket *packet);
void screenSpreadFanDraw(GsQueueContext *record, XglPacket *unused);
void screenWarpDraw(GsQueueContext *record, XglPacket *unused);
void ringStripDraw(GsQueueContext *unusedRecord, XglPacket *unused);
void recordDrawStrip(GsQueueContext *record, XglPacket *packet);
void recordDrawTurnColumns(GsQueueContext *record, XglPacket *packet);
void orbitDrawRing(GsQueueContext *record, XglPacket *packet);
void quadDrawPreset(GsQueueContext *record, XglPacket *packet);
void quadDrawPresetUv(GsQueueContext *record, XglPacket *packet);
void circleDrawSegments(GsQueueContext *record, XglPacket *packet);
void gridDrawCells(GsQueueContext *record, XglPacket *packet);
void recordSetAlphaFlags(GsQueueContext *record, XglPacket *unused);
void quadDrawRotating(GsQueueContext *record, XglPacket *packet);
void pointGridDraw(GsQueueContext *record, XglPacket *packet);
void quadOrbitGridDraw(GsQueueContext *record, XglPacket *packet);
void ringWaveGridDraw(GsQueueContext *record, XglPacket *packet);
void spriteBatchDraw(GsQueueContext *record, XglPacket *packet);
void recordDrawNone(GsQueueContext *unusedRecord, XglPacket *unused);

/* Helpers the handlers share across files. */
int recordTurnAngleAt(Vector4 *angles, GsQueueContext *record, f32 x);
f32 recordLerp(f32 from, f32 to, f32 fraction);
void recordOffsetCorners(Vector4 *points, f32 dx, f32 dy);
void recordProjectQuad(Vector4 *corners, GsQueueContext *record, const Vector4 *angles);
void recordRotateCorner(Vector4 *corner, const Vector4 *angles, int depth, int x, int y);
void recordDrawBatchQuad(XglPacket *packet, Vector4 *corners, GsQueueContext *record);
void recordDrawFlatColumn(XglPacket *packet, Vector4 *corners, GsQueueContext *record);
int setPointFromRecord(Vector4 *destination, int x, int y, const GsQueueContext *record);
void circleSpriteCorners(Vector4 *points, int x, int y, const GsQueueContext *record);
void quadDrawTransformed(XglPacket *packet, Vector4 *corners, GsQueueContext *transform, const Vector4 *rotation);
void quadDrawRepeated(XglPacket *packet, Vector4 *corners, GsQueueContext *batch);
int pointRadialAngles(Vector4 *out, f32 x, f32 y, const GsQueueContext *record);
f32 angleLerp(f32 from, f32 to, f32 fraction);
void quadRotateCorner(Vector4 *destination, Vector4 *source, const Vector4 *rotation, int value, int x, int y);
void quadRotateAroundCenter(Vector4 *vectors, f32 angle, f32 centerX, f32 centerY);
void pointRandomDisplace(Vector4 *position, Vector4 *uv, int x, int y, GsQueueContext *rect);
int randomSeededIndexX(f32 *values, int limit);
int randomSeededIndexY(f32 *values, int limit);
void randomRefreshSeed(void);
f32 quadOrbitPoint(Vector4 *position, Vector4 *center, int centerX, int centerY, GsQueueContext *orbit);
f32 angleWrap360(f32 angle);
f32 angleWrap360Copy(int sum, f32 angle);
int angleAdvance(int angle, int delta);
f32 ringWavePoint(Vector4 *wave, Vector4 *flat, int x, int y, GsQueueContext *ring);
void gsQueueFadeToColorPhases(XglPacket *packet, int colorB, GsQueueContext *context, f32 t);
void gsQueueFadeToColor(XglPacket *packet, int colorB, GsQueueContext *context, f32 t);

#endif /* INCLUDE_MAIN_RECORD_RENDER_H */
