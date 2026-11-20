#include "common.h"

#include "main/gs_queue.h"
#include "shared.h"
#include "main/record_render.h"

/* Queues the batch's textured sprite count times: the top-left and bottom-right
   corners with their texture coordinates, then flushes the packet. The corner
   vectors are written through the DoublewordVector4 union (the original's stores
   alias the batch reads, which only a union access reproduces). */
void spriteBatchDraw(GsQueueContext *batch, XglPacket *packet) {
    DoublewordVector4 position[2];
    DoublewordVector4 uv[2];
    int z = batch->u.queue.z;
    int count = batch->u.queue.count;
    int top = batch->u.queue.screen.top;
    int left = batch->u.queue.screen.left;
    int right = batch->u.queue.screen.right;
    int bottom = batch->u.queue.screen.bottom;
    int uvTop = batch->u.queue.texture.top;
    int uvLeft = batch->u.queue.texture.left;
    int uvRight = batch->u.queue.texture.right;
    int uvBottom = batch->u.queue.texture.bottom;
    int i;

    position[0].vector.x = left;
    position[0].vector.y = top;
    uv[0].vector.x = uvLeft;
    uv[0].vector.y = uvTop;
    position[1].vector.x = right;
    uv[1].vector.x = uvRight;
    position[1].vector.y = bottom;
    uv[1].vector.y = uvBottom;
    position[0].vector.z = z;
    position[1].vector.z = z;
    gsQueueBeginSprites(batch);
    for (i = 0; i < count; i++) {
        gsQueueAddTexturedVertex(packet, &position[0].vector, &uv[0].vector, batch);
        gsQueueAddTexturedKickVertex(packet, &position[1].vector, &uv[1].vector, batch);
    }
    gsQueueFlush(packet);
}

/* Returns at once with no effect. No caller inside this unit. */
void recordDrawNone(GsQueueContext *unusedRecord, XglPacket *unused) {
}

#define FRAME_WIDTH 0x200
#define FRAME_HEIGHT 0x1C0

/* Fills a clip rectangle (y, x, width, height) with the given size clamped to
   the frame, and stores the remaining frame margin in a second record. */
void clipRectFill(long long *clip, long long *margin, int width, int height) {
    clip[0] = 0;
    clip[1] = 0;
    clip[2] = width;
    clip[3] = height;
    if (clip[2] > FRAME_WIDTH) {
        clip[2] = FRAME_WIDTH;
    }
    if (clip[3] > FRAME_HEIGHT) {
        clip[3] = FRAME_HEIGHT;
    }
    margin[0] = FRAME_HEIGHT - clip[3];
    margin[1] = FRAME_WIDTH - clip[2];
    margin[2] = FRAME_WIDTH;
    margin[3] = FRAME_HEIGHT;
}

#define ABS(v) ((v) >= 0 ? (v) : -(v))

/* Splits a signed (x, y) offset between two clip records: the record that
   receives the offset gets its magnitude, the other keeps the full span. */
void clipRectOffsetSplit(long long *a, long long *b, int x, int y) {
    int mag;

    if (x < 0) {
        mag = ABS(x);
        a[1] = 0;
        a[2] = FRAME_WIDTH;
        b[1] = mag;
        b[2] = FRAME_WIDTH - mag;
    } else {
        mag = x;
        a[1] = mag;
        a[2] = FRAME_WIDTH - mag;
        b[2] = FRAME_WIDTH;
        b[1] = 0;
    }

    if (y < 0) {
        mag = ABS(y);
        a[0] = 0;
        a[3] = FRAME_HEIGHT;
        b[0] = mag;
        b[3] = FRAME_HEIGHT - mag;
    } else {
        mag = y;
        a[0] = mag;
        a[3] = FRAME_HEIGHT - mag;
        b[3] = FRAME_HEIGHT;
        b[0] = 0;
    }
}

