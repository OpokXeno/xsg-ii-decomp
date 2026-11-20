#include "common.h"
#include "shared.h"
#include "main/xgl_cur_mul.h"
#include "main/game_camera.h"
#include "main/xenoActor.h"
#include "main/xeno_actor_set.h"

/* Same as xgl_cur_matrix.h; declared here because that header cannot be included next to the int xglCurApplyVector below. */
void xglCurSetTranslate(const Vector4 *vector);
/* Not xgl_cur_matrix.h (void): the original returns the callee result here, but an int declaration in the shared
   header changes XenoActorUpdateTree in xenoActor.c (4 rows, v0 live across the call) and conflicts with the
   void definition in xgl_cur_matrix.c, so the int form stays local. */
int xglCurApplyVector(Vector4 *, const Vector4 *);

#define XENO_CHANNEL_BIT80 0x80  /* channel flag bit set here; its role is not recovered */
#define CFE_DIR_COUNT 9          /* directions 0..8 index cfeDirOffsetTable */

extern float cfeDirOffsetTable[10][2];

/* Damps the actor's focus pair toward the table offset of direction dir (0..8); the focus-locked draw flag is set unless both results are zero. */
void cfeDampDirOffset(struct XenoActor *actor, int dir) {
    if (dir < CFE_DIR_COUNT) {
        actor->focusX = actor->focusX * 0.9f + cfeDirOffsetTable[dir][0] * 0.1f;
        actor->focusY = actor->focusY * 0.9f + cfeDirOffsetTable[dir][1] * 0.1f;
        if (actor->focusX != 0.0f || actor->focusY != 0.0f) {
            actor->drawFlags |= XENO_DRAW_FOCUS_LOCKED;
        } else {
            actor->drawFlags &= ~XENO_DRAW_FOCUS_LOCKED;
        }
    }
}

/* Builds the current matrix from the table's translation, rotations Z/Y/X and scale, then applies it to the point stored in out. */
int XenoActorGetHrcPoint(struct XenoActor *actor, Vector4 *out) {
    XenoHrc *table = actor->pHrcData;
    XenoTransform *transform;

    if (table == 0) {
        return 0;
    }
    transform = &table->transform;
    xglCurSetTranslate((const Vector4 *) transform->translation);
    xglCurMulRotateZ(transform->rotationZ);
    xglCurMulRotateY(transform->rotationY);
    xglCurMulRotateX(transform->rotationX);
    xglCurMulScaleXYZ(transform->scale);
    out->x = transform->point[0];
    out->y = transform->point[1];
    out->z = transform->point[2];
    out->w = 1.0f;
    return xglCurApplyVector(out, out);
}

/* Sets XENO_CHANNEL_BIT80 of the channel block flag word. */
void XenoActorSetChannelFlag80(struct XenoActor *actor) {
    actor->motion[0].flags |= XENO_CHANNEL_BIT80;
}

/* Stores the state word that XenoActorDrawMotion reads and clears. */
void XenoActorSetState(struct XenoActor *actor, int state) {
    actor->state = state;
}

/* Data of this TU: the focus offset per direction, ten (x, y) pairs as the original stores them. */
float cfeDirOffsetTable[10][2] = {
    { 0.0f, 0.0f },
    { 0.0f, -0.100000001f },
    { 0.0f, 0.150000006f },
    { 0.100000001f, 0.0f },
    { -0.100000001f, 0.0f },
    { 0.100000001f, -0.100000001f },
    { -0.100000001f, -0.100000001f },
    { 0.100000001f, 0.150000006f },
    { -0.100000001f, 0.150000006f },
    { 0.0f, 0.0f }
};
