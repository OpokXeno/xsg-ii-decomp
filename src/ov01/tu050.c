/*
 * OV01 original TU 50: 0x00aa3928..0x00aa3e28 (7 functions)
 */
#include "common.h"
#include "ov01/tu005.h"
#include "ov01/tu023.h"
#include "ov01/tu036.h"
#include "ov01/tu048.h"
#include "ov01/tu050.h"
#include "ov01/tu051.h"

typedef MenuObject PathObject;

/* Screen points of the five path objects (x, y). */
typedef struct ObjPathPoint {
    int x;
    int y;
} ObjPathPoint;

#define OBJ_PATH_COUNT 5
/* The last path object is the extra one objPathSendLast sends to point 0. */
#define OBJ_PATH_MOVING 4

ObjPathPoint objPathPoints[OBJ_PATH_COUNT] = {
    { 376, 390 }, { 404, 390 }, { 432, 390 }, { 460, 390 }, { 288, 390 },
};
PathObject *objPathTable[OBJ_PATH_COUNT];

/* ActStock.pickSource value that shows the moving path object instead of point 0. */
#define PATH_STOCK_SOURCE_MOVING 2

void menuKeyCellDraw(GrPacket *packet, int x, int y, int key, int mode, int fade);

int objStdMove(void *target);
void slideSetStep(void *target, int value);
void *objSlotAlloc(void);
void objMoveUpdate(PathObject *object);
void objPathDraw(PathObject *object);

/* Allocates the five path objects on their points: slot and point index i, fade
 * staggered by -12, stopped; the last one starts hidden. */
void objPathInit(void)
{
    PathObject *object;
    int i;

    for (i = 0; i < OBJ_PATH_COUNT; i++) {
        object = objSlotAlloc();
        if (object == 0) {
            return;
        }
        object->update = objMoveUpdate;
        object->draw = objPathDraw;
        objPathTable[i] = object;
        ov01MatrixBlockInit(&object->slide.block);
        object->slide.pos.x = objPathPoints[i].x;
        object->slide.pos.y = objPathPoints[i].y;
        object->slide.pos.z = 0;
        object->kind.path.slot = i;
        object->kind.path.index = i;
        object->kind.path.fade = i * -12;
        object->moving = 0;
        if (i >= OBJ_PATH_MOVING) {
            object->kind.path.shown = 0;
        }
    }
}

/* Per-frame update (callback +4): while the mover is active it steps toward its target,
 * clears state when objStdMove reports arrival, and advances the counter to 0x18. */
void objMoveUpdate(PathObject *object) {
    int state;
    void *target;
    int counter;

    state = object->moving;
    if (state != 0) {
        if (state >= 0) {
            if (state < 3) {
                target = &object->slide;
                slideSetStep(target, 2);
                if (objStdMove(target) != 0) {
                    object->moving = 0;
                }
            }
        }
    }
    counter = 0x18;
    if (object->kind.path.fade < 0x18) {
        counter = object->kind.path.fade + 4;
    }
    object->kind.path.fade = counter;
}

/* Draw callback (+8) of a path object: draws the stock count of the unit on the
 * object's point while the object is shown, its point is within the stock and
 * the unit is active; the targeted unit is drawn 4 pixels higher. */
void objPathDraw(PathObject *object)
{
    struct ActStock *stock;
    StatOwner *unit;
    GrPacket packet;
    int slot;
    int index;
    int left;
    int x;
    int y;

    if (object->kind.path.shown == 0) {
        return;
    }
    stock = actStockGet();
    slot = object->kind.path.slot;
    index = object->kind.path.index;
    left = stock->max - stock->pickCursor + 1;
    if (stock->max == 0) {
        return;
    }
    if (index != OBJ_PATH_MOVING && !(index < left)) {
        return;
    }
    if (index == 0 && stock->pickSource == PATH_STOCK_SOURCE_MOVING) {
        return;
    }
    if (index == OBJ_PATH_MOVING && stock->pickSource != PATH_STOCK_SOURCE_MOVING) {
        return;
    }
    if (object->kind.path.fade <= 0) {
        return;
    }
    if (index == OBJ_PATH_MOVING) {
        unit = stock->lastPicked;
    } else {
        unit = stock->stock[slot];
    }
    if (unit == 0) {
        return;
    }
    if (unit->flags & UNIT_FLAG_DOWN) {
        return;
    }
    if (!(unit->slotFlags & OV01_OBJECT_IN_USE)) {
        return;
    }
    y = object->slide.pos.y;
    x = object->slide.pos.x;
    if (unit == tgtUnitGet()) {
        y -= 4;
    }
    grGpInit(&packet);
    menuKeyCellDraw(&packet, x, y, unit->stats->key, 1, object->kind.path.fade);
    grPacketSend(&packet);
}

/* Puts every path object back on its own point, stopped, its fade restarted (staggered by -12 per object); all
 * but the last are shown. */
void objPathReset(void)
{
    PathObject *object;
    int i;

    for (i = 0; i < OBJ_PATH_COUNT; i++) {
        object = objPathTable[i];
        object->kind.path.index = i;
        object->kind.path.fade = i * -12;
        object->moving = 0;
        object->slide.pos.x = objPathPoints[i].x;
        object->slide.pos.y = objPathPoints[i].y;
        object->slide.pos.z = 0;
        if (i < OBJ_PATH_MOVING) {
            object->kind.path.shown = 1;
        } else {
            object->kind.path.shown = 0;
        }
    }
}

/* Moves each of the first four path objects one point back (to index - 1);
 * one already at point 0 is hidden instead. */
void objPathStep(void)
{
    PathObject *object;
    int i;
    int index;

    for (i = 0; i < OBJ_PATH_MOVING; i++) {
        object = objPathTable[i];
        index = object->kind.path.index;
        if (index <= 0) {
            object->kind.path.index = 0;
            object->kind.path.shown = 0;
        } else {
            index--;
            object->moving = 1;
            object->kind.path.index = index;
            object->slide.pos.target_x = objPathPoints[index].x;
            object->slide.pos.target_y = objPathPoints[index].y;
            object->slide.pos.target_z = 0;
        }
    }
}

/* Sends the last path object from point 4 towards point 0 and shows it. */
void objPathSendLast(void)
{
    PathObject *object = objPathTable[OBJ_PATH_MOVING];

    object->moving = 2;
    object->slide.pos.target_x = objPathPoints[0].x;
    object->slide.pos.target_y = objPathPoints[0].y;
    object->slide.pos.target_z = 0;
    object->slide.pos.x = objPathPoints[OBJ_PATH_MOVING].x;
    object->slide.pos.y = objPathPoints[OBJ_PATH_MOVING].y;
    object->slide.pos.z = 0;
    object->kind.path.shown = 1;
}

/* After the action of slot is used: when its stock entry is empty, the path
 * objects after it (up to the moving one) each move one point back. */
void objPathShift(int slot)
{
    int i;
    int index;
    PathObject *object;

    if (actStockGet()->stock[objPathTable[slot]->kind.path.slot] != 0) {
        return;
    }
    for (i = slot + 1; i < OBJ_PATH_MOVING; i++) {
        object = objPathTable[i];
        index = object->kind.path.index;
        if (index > 0) {
            index--;
            object->moving = 1;
            object->kind.path.index = index;
            object->slide.pos.target_x = objPathPoints[index].x;
            object->slide.pos.target_y = objPathPoints[index].y;
            object->slide.pos.target_z = 0;
        }
    }
}
