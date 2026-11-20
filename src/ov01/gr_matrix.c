#include "common.h"
#include "shared.h"
#include "ov01/gr_matrix.h"
#include "main/xgl_camera.h"
#include "main/xgl_studio.h"
#include "main/xgl_2.h"

#define GR_GS_WINDOW_OFFSET_X 0x7000    /* GS X offset of the projected point, 1/16 pixel (X1 gr_gp_init.c) */
#define GR_GS_WINDOW_OFFSET_Y 0x7200    /* GS Y offset of the projected point, 1/16 pixel (X1 gr_gp_init.c) */
#define GR_VERTEX_OUT_OF_FRUSTUM 0x8000 /* clip flag grRotTransPers sets for a point outside [-1, 1] (X1 gr_gp_init.c) */



Matrix4 *grWorldMatGet(void)
{
    XglCamera *camera;

    /* Camera zero's view matrix (volume[0].viewMatrix) is the current world matrix. */
    xglStudioGetCamera(&camera, 0);
    return &camera->volume[0].viewMatrix;
}


/* grCalcMatrix: destination = world * (translate(position + offset) * rotate(angles) * scale).
 * The X1 twin (grCalcMatrix in ov01 gr_gp_init.c) runs the same steps with the same world matrix. */
void grCalcMatrix(TransformState *state, Matrix4 *destination)
{
    Matrix localMatrix;
    Vector4 translation;
    Matrix4 *world;

    world = grWorldMatGet();
    xglMatrixUnit(localMatrix.rows);
    translation.x = state->position.x + state->offset.x;
    translation.y = state->position.y + state->offset.y;
    translation.z = state->position.z + state->offset.z;
    translation.w = 1.0f;
    xglMatrixTrans(&localMatrix, &localMatrix, &translation);
    grRotMatrix(&localMatrix.rows, &localMatrix.rows, &state->angles);
    xglMatrixScale(&localMatrix, &localMatrix, &state->scale);
    xglMatrixMul(*destination, *world, localMatrix.rows);
}

void grRotMatrix(Matrix4 *destination, Matrix4 *source, const Vector4 *angles)
{
    /* The resident matrix stack applies Y, then X, then Z in this wrapper. */
    xglMatrixStackLoad(*source);
    xglMatrixStackRotY(angles->y);
    xglMatrixStackRotX(angles->x);
    xglMatrixStackRotZ(angles->z);
    xglMatrixStackSave(*destination);
}


/* grRotTransPers: transforms the point by the matrix, divides by w, clips to [-1, 1] (flag
 * GR_VERTEX_OUT_OF_FRUSTUM) and maps an in-bounds point to screen X/Y/Z by the camera's screenScale and
 * screenOffset. */
float grRotTransPers(GrScreenVertex *out, Matrix4 matrix, const Vector4 *point, int *clipFlags)
{
    Vector4 clip;
    XglCamera *camera;
    float invW;
    int outside;

    xglVectorMulMat(&clip, matrix, point);
    invW = 1.0f / clip.w;
    clip.w = 1.0f;
    clip.x = clip.x * invW;
    clip.y = clip.y * invW;
    clip.z = clip.z * invW;
    outside = 0;
    if (clip.x > 1.0f || clip.x < -1.0f) {
        outside = 1;
    }
    if (clip.y > 1.0f || clip.y < -1.0f) {
        outside++;
    }
    if (clip.z > 1.0f || clip.z < -1.0f) {
        outside++;
    }
    if (outside == 0) {
        xglStudioGetCamera(&camera, 0);
        xglVectorMulAdd(&clip, &clip, &camera->screen.screenScale, &camera->screen.screenOffset);
    } else {
        *clipFlags |= GR_VERTEX_OUT_OF_FRUSTUM;
    }
    out->z = (int)clip.z;
    out->y = (int)(clip.y * 16.0f);
    out->x = (int)(clip.x * 16.0f);
    out->flags = *clipFlags & GR_VERTEX_OUT_OF_FRUSTUM;
    return invW;
}

extern void *memset(void *destination, int value, unsigned int count);

/* grBBCalc: projects the object origin (0, 0, 0, 1) through grCalcMatrix's matrix. Returns 0 when the point is
 * clipped; otherwise stores its screen X/Y with the GS window offsets GR_GS_WINDOW_OFFSET_X and
 * GR_GS_WINDOW_OFFSET_Y removed and divided by 16. */
int grBBCalc(TransformState *state, int *screenX, int *screenY)
{
    Vector4 origin;
    Matrix4 matrix;
    GrScreenVertex point;
    int clipFlags;

    memset(&origin, 0, sizeof(origin));
    origin.w = 1.0f;
    clipFlags = 0;
    grCalcMatrix(state, &matrix);
    grRotTransPers(&point, matrix, &origin, &clipFlags);
    if ((clipFlags & GR_VERTEX_OUT_OF_FRUSTUM) != 0) {
        return 0;
    }
    *screenX = (point.x - GR_GS_WINDOW_OFFSET_X) / 16;
    *screenY = (point.y - GR_GS_WINDOW_OFFSET_Y) / 16;
    return 1;
}
