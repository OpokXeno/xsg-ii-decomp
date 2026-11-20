#include "common.h"
#include "shared.h"

#include "vu0_macro.h"
#include "main/ytachi_test.h"
#include "main/xgl_camera.h"
#include "main/xgl_studio.h"
#include "main/control_entry.h"
#include "main/pause_menu_page_uwamono.h"
#include "main/get_ladder.h"
#include "main/nml_packet_add.h"

#include "main/cur_matrix_asm.h"
#include "main/xgl_2.h"
/* Record mode: which collision test a record gets (HitCheckMapUnitAt and CrossPointUwamonoAt dispatch on it). */
enum {
    UWAMONO_MODE_BOX = 1,
    UWAMONO_MODE_CIRCLE = 2
};

/* Records of uwamonoRecords the scans visit. */
#define UWAMONO_RECORD_COUNT 0x100


extern float uwamonoDebugCornerW;

extern float uwamonoPathDebugLift;

extern float uwamonoLinkDebugLift;

extern float ladderRungExitAngle;
extern float ladderRungExitRadius;
extern float sinf(float angle);
extern float cosf(float angle);

/* A circle is walked in 16 steps of an eighth of pi (22.5 degrees). */
#define CIRCLE_STEP 0.39269909f

/* Two screen coordinates, scaled by 16 before uwamonoProjectToScreen halves them. */
typedef ScreenVector4 UwamonoScreenPos;


/*
 * Builds the debug outline of every active record and draws nothing: a box record (mode 1) gets the four bottom
 * corners of its footprint and their copies raised by the extent height, a circle record (mode 2) goes to
 * DispPillar. The Xenosaga II form of Xenosaga I's DispCollUwamono. Does nothing while uwamonoDrawDisabled is set.
 */
void DispCollUwamono(void)
{
    DoublewordVector4 bottom[4];
    DoublewordVector4 top[4];
    AlignedVector4 position;
    DoublewordAlignedVector4 size;
    int colors[8];
    float cornerW;
    int i;

    if (uwamonoDrawDisabled == 0) {
        colors[0] = colors[1] = 0;
        colors[2] = 255;
        colors[3] = 128;
        colors[4] = 255;
        colors[5] = colors[6] = 0;
        colors[7] = 128;
        cornerW = uwamonoDebugCornerW;
        for (i = 0; i < UWAMONO_RECORD_COUNT; i++) {
            UwamonoRecord *record = &uwamonoRecords[i];
            UwamonoRecord *unit;
            float (*rotation)[4];

            if (record->kind < 2 || record->kind == 4 || record->kind == 5 || record->kind == 6
                || record->kind == 3 || record->kind == 7) {
                continue;
            }
            if (record->extent.x == 0.0f || record->extent.y == 0.0f || record->extent.z == 0.0f) {
                continue;
            }
            if (record->searchMode == 0) {
                continue;
            }
            unit = &uwamonoRecords[i];
            rotation = unit->matrix;
            position.x = rotation[3][0];
            position.y = rotation[3][1];
            position.z = rotation[3][2];
            position.w = 1.0f;
            size = unit->extent;
            switch (record->searchMode) {
            case UWAMONO_MODE_BOX:
                bottom[0].vector.x = position.x + (size.x * rotation[0][0] + size.z * rotation[2][0]);
                bottom[0].vector.y = position.y;
                bottom[0].vector.z = position.z + (size.x * rotation[0][2] + size.z * rotation[2][2]);
                bottom[1].vector.x = position.x - (size.z * rotation[0][2] + size.x * rotation[2][2]);
                bottom[1].vector.y = position.y;
                bottom[1].vector.z = position.z + (size.z * rotation[0][0] + size.x * rotation[2][0]);
                bottom[2].vector.x = position.x - (size.x * rotation[0][0] + size.z * rotation[2][0]);
                bottom[2].vector.y = position.y;
                bottom[2].vector.z = position.z - (size.x * rotation[0][2] + size.z * rotation[2][2]);
                bottom[3].vector.x = position.x + (size.z * rotation[0][2] + size.x * rotation[2][2]);
                bottom[3].vector.y = position.y;
                bottom[3].vector.z = position.z - (size.z * rotation[0][0] + size.x * rotation[2][0]);
                bottom[0].vector.w = cornerW;
                bottom[1].vector.w = cornerW;
                bottom[2].vector.w = cornerW;
                bottom[3].vector.w = cornerW;
                top[0] = bottom[0];
                top[1] = bottom[1];
                top[2] = bottom[2];
                top[3] = bottom[3];
                top[0].vector.y += size.y;
                top[1].vector.y += size.y;
                top[2].vector.y += size.y;
                top[3].vector.y += size.y;
                break;
            case UWAMONO_MODE_CIRCLE:
                DispPillar(&record->position, record->extent.x, record->extent.y, 4);
                break;
            }
            /*
             * The unit pointer is set again with no use left, as Xenosaga I's DispCollUwamono sets it once per
             * branch. A pointer set more than once has no known base while the corners are stored, so the
             * rotation rows are loaded again after each corner store.
             */
            unit = &uwamonoRecords[i];
        }
    }
}

void uwamonoDrawPointPath(int slotIndex) {
    UwamonoState *state;
    int colour[4];
    DoublewordVector4 segment[2];
    int i;

    state = uwamonoRecords[slotIndex].body.motions;
    if (uwamonoDrawDisabled == 0) {
        colour[0] = 255;
        colour[1] = 0;
        colour[2] = 0;
        colour[3] = 128;
        i = 0;
        if (state->pointCount - 1 > 0) {
            do {
                segment[0].vector = state->points[i].point;
                segment[1].vector = state->points[i + 1].point;
                segment[0].vector.y += uwamonoPathDebugLift;
                segment[1].vector.y += uwamonoPathDebugLift;
                i++;
            } while (i < state->pointCount - 1);
        }
    }
}

/*
 * For each vector pair of the state's link table, copies both vectors into locals and
 * adds the gain to the y of each copy. Does nothing while uwamonoDrawDisabled is set.
 */
void uwamonoDrawStateLinks(int slotIndex)
{
    UwamonoState *state;
    int color[4];
    int i;
    int j;
    int target;

    state = uwamonoRecords[slotIndex].body.motions;
    if (uwamonoDrawDisabled == 0) {
        color[0] = 255;
        color[1] = 0;
        color[2] = 255;
        color[3] = 128;
        for (i = 0; i < state->routeNodeCount - 1; i++) {
            j = 0;
            if (state->routeLinkCounts[i] > 0) {
                do {
                    DoublewordVector4 current;
                    DoublewordVector4 linked;
                    target = state->routeLinks[i][j];
                    current.vector = state->homeNodes[i].point;
                    linked.vector = state->homeNodes[target].point;
                    current.vector.y += uwamonoLinkDebugLift;
                    linked.vector.y += uwamonoLinkDebugLift;
                } while (++j < state->routeLinkCounts[i]);
            }
        }
    }
}

void uwamonoDrawLadder(int slotIndex)
{
    LadderSlot *slot;
    int color[4];
    DoublewordAlignedVector4 rung;
    DoublewordAlignedVector4 point;
    float phase;
    float radius;
    int i;

    slot = &ladderSlots[slotIndex];
    if (uwamonoDrawDisabled == 0 && slot->id != -1) {
        color[0] = 255;
        color[1] = 255;
        color[2] = 0;
        color[3] = 128;
        point = slot->position;
        i = 0;
        point.y = point.y + slot->height;
        point = slot->position;
        point.x = point.x + sinf(slot->angle);
        point.z = point.z + cosf(slot->angle);
        if (slot->rungCount > 0) {
            phase = ladderRungExitAngle;
            radius = ladderRungExitRadius;
            do {
                rung = slot->position;
                rung.y = rung.y + slot->rungs[i];
                if (slot->rungExits[i] & 1) {
                    point.x = rung.x + sinf(slot->angle + phase) * radius;
                    point.y = rung.y;
                    point.z = rung.z + cosf(slot->angle + phase) * radius;
                }
                if (slot->rungExits[i] & 2) {
                    point.x = rung.x + sinf(slot->angle - phase) * radius;
                    point.y = rung.y;
                    point.z = rung.z + cosf(slot->angle - phase) * radius;
                }
            } while (++i < slot->rungCount);
        }
    }
}

void uwamonoDrawPursuitDebug(int slotIndex)
{
    int table[4];
    int n;

    if (uwamonoDrawDisabled != 0) {
        return;
    }
    table[0] = 255;
    table[1] = 255;
    table[2] = 0;
    table[3] = 128;
    n = 123;
    do {
        n -= 4;
    } while (n > -1);
}

/*
 * Walks a ring of the given radius around source in the XZ plane, one segment per step: each step builds the segment's
 * two end points; when lift is positive the end points are copied with y raised by lift. Does nothing while
 * uwamonoDrawDisabled is set. color is not read: the callers pass a small id there (1, 2, 4 or 7), where Xenosaga I's
 * DispPillar took its unused colors argument.
 */
void DispPillar(Vector4 *source, float radius, float lift, int color)
{
    DoublewordAlignedVector4 first;
    DoublewordAlignedVector4 second;
    DoublewordAlignedVector4 firstLifted;
    DoublewordAlignedVector4 secondLifted;
    float angle;
    int i;

    if (uwamonoDrawDisabled == 0) {
        for (i = 0; i < 17; i++) {
            angle = CIRCLE_STEP;
            angle = i * angle;
            first = *source;
            second = *source;
            first.x = first.x + xglSin(angle) * radius;
            first.z = first.z + xglCos(angle) * radius;
            angle = CIRCLE_STEP;
            angle = (i + 1) * angle;
            second.x = second.x + xglSin(angle) * radius;
            second.z = second.z + xglCos(angle) * radius;
            if (lift > 0.0f) {
                firstLifted = first;
                secondLifted = second;
                firstLifted.y = firstLifted.y + lift;
                secondLifted.y = secondLifted.y + lift;
            }
        }
    }
}

/* Empty stub: six callers, nothing to name from the body. */
void uwamonoDrawLine(const Vector4 *first, const Vector4 *second, int mode) {

}

void uwamonoDrawFanEdge(const Vector4 *from, float angle, float radius, int mode)
{
    Vector4 point;

    if (uwamonoDrawDisabled == 0) {
        point = *from;
        point.x += sinf(angle) * radius;
        point.z += cosf(angle) * radius;
    }
}

/* Stores a value in the record's pendingValue, but only while the global gate uwamonoDrawDisabled is clear. */
void uwamonoSetPendingValue(UwamonoRecord *record, float pendingValue) {
    if (uwamonoDrawDisabled == 0) {
        record->pendingValue = pendingValue;
    }
}

/* color is the RGBA block the callers pass (255, 127, 0, 127); the body does not read it. */
void uwamonoDrawSegmentBox(const Vector4 *from, const Vector4 *to, const int color[4])
{
    Vector4 outline[8];
    int i;

    if (uwamonoDrawDisabled != 0) {
        return;
    }
    for (i = 0; i < 4; i++) {
        outline[i] = *from;
        outline[i + 4] = *to;
    }
    outline[1].x = to->x;
    outline[2].x = to->x;
    outline[2].z = to->z;
    outline[3].z = to->z;
    outline[5].x = from->x;
    outline[6].x = from->x;
    outline[6].z = from->z;
    outline[7].z = from->z;
}

/*
 * Builds three copies of *position, each nudged by 0.5 on one axis (x, y, z),
 * and three RGBA colour rows (red, green, blue); no value leaves the function.
 */
void uwamonoDrawPointAxes(const Vector4 *position) {
    if (uwamonoDrawDisabled == 0) {
        Vector4 pointX = *position;
        Vector4 pointY = *position;
        Vector4 pointZ = *position;
        int colorRed[4];
        int colorGreen[4];
        int colorBlue[4];

        colorRed[0] = 255;
        colorRed[1] = 0;
        colorRed[2] = 0;
        colorRed[3] = 63;
        colorGreen[0] = 0;
        colorGreen[1] = 255;
        colorGreen[2] = 0;
        colorGreen[3] = 63;
        colorBlue[0] = 0;
        colorBlue[1] = 0;
        colorBlue[2] = 255;
        colorBlue[3] = 63;

        pointX.x += 0.5f;
        pointY.y += 0.5f;
        pointZ.z += 0.5f;
    }
}

/*
 * Sets up the active camera's vectors, then rescales the two screen coordinates in
 * position (x16) to centred values; _CurRotTransPersClipScreen can replace both with 0x1000.
 */
void uwamonoProjectToScreen(const Vector4 *query, UwamonoScreenPos *position) {
    XglCamera *camera;

    camera = xglStudioSelectGetActiveCamera(0);
    _CurMatrixSet(&camera->volume[0].viewMatrix);
    _CurSetViewScaleTrans(&camera->screen.screenScale, &camera->screen.screenOffset);
    if (_CurRotTransPersClipScreen(position, query) != 0) {
        position->x = 0x1000;
        position->y = 0x1000;
        return;
    }
    position->x = ((position->x >> 4) - 0x700) >> 1;
    position->y = ((position->y >> 4) - 0x720) >> 1;
}
float uwamonoDebugCornerW = 0.10000000149011612f;
float uwamonoPathDebugLift = 0.20000000298023224f;
float uwamonoLinkDebugLift = 0.20000000298023224f;
float ladderRungExitAngle = 2.1816616058349609f;
float ladderRungExitRadius = 0.87000000476837158f;
