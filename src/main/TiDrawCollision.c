#include "common.h"
#include "main/TiDrawCollision.h"
#include "main/assert_fail.h"
#include "main/xgl_2.h"
#include "shared.h"
#include "main/xgl_camera.h"
#include "main/xgl_cur_mul.h"
#include "main/xgl_cur_matrix.h"
#include "main/TiDrawJoint.h"
#include "main/xgl_studio.h"

float cosf(float);
float sinf(float);
/* Points of the collision ring, and the angle between two neighbouring vertices (the 24-sided ring: 2*pi/24, as the original constant). */
#define TI_COLLISION_RING_POINTS 48
#define TI_COLLISION_RING_STEP 0.26179933547973633f
/* 48 Vector4 points: the ring that tiDrawCollisionBuildRing builds at the given scale. */
extern Vector4 tiDrawCollisionRing[TI_COLLISION_RING_POINTS];
/* First transform pass of the ring (matrix applied). */
extern Vector4 tiDrawCollisionTransformed[];
/* Second pass of the ring, the input of the collision fill. */
extern ScreenVector4 tiDrawCollisionScreenPoints[];

/* Builds the 48-point ring at scale: cos/sin pairs in the camera's rotated frame, two points per pass. */
void tiDrawCollisionBuildRing(float scale)
{
    XglCamera *pCamera;
    float angleStep;
    float unitW;
    float angle;
    unsigned int angleIndex;
    unsigned int nextAngleIndex;
    unsigned int firstPoint;
    Vector4 *pPoint;
    Vector4 *pNext;
    float *pSecondX, *pFirstX;
    float *pFirstY;
    float *pSecondY;

    angleIndex = 0;
    firstPoint = 0;
    pCamera = xglStudioGetActiveCamera();
    angleStep = TI_COLLISION_RING_STEP;
    unitW = 1.0f;
    xglCurSetRotateY(pCamera->travel.rotation.y);
    xglCurMulRotateX(pCamera->travel.rotation.x);
    xglCurMulRotateZ(pCamera->travel.rotation.z);
    for (; firstPoint < TI_COLLISION_RING_POINTS; firstPoint += 2) {
        angle = (float)angleIndex * angleStep;
        pFirstX = &tiDrawCollisionRing[firstPoint].x;
        pFirstY = &tiDrawCollisionRing[firstPoint + 1].x;
        pFirstY -= 3;
        *pFirstX = cosf(angle) * scale;
        angle = (float)angleIndex * angleStep;
        nextAngleIndex = angleIndex + 1;
        pSecondX = &tiDrawCollisionRing[firstPoint].x;
        pSecondX += 4;
        *pFirstY = sinf(angle) * scale;
        tiDrawCollisionRing[firstPoint].z = 0.0f;
        tiDrawCollisionRing[firstPoint].w = unitW;
        angle = (float)nextAngleIndex * angleStep;
        pSecondY = &tiDrawCollisionRing[firstPoint + 1].x;
        pSecondY++;
        *pSecondX = cosf(angle) * scale;
        angle = (float)nextAngleIndex * angleStep;
        *pSecondY = sinf(angle) * scale;
        tiDrawCollisionRing[firstPoint + 1].z = 0.0f;
        tiDrawCollisionRing[firstPoint + 1].w = unitW;
        xglCurApplyVector(&tiDrawCollisionRing[firstPoint], &tiDrawCollisionRing[firstPoint]);
        xglCurApplyVector(&tiDrawCollisionRing[firstPoint + 1], &tiDrawCollisionRing[firstPoint + 1]);
        angleIndex = nextAngleIndex;
    }
}

/* Transforms the 48-point ring by pTransMat in two passes and fills the collision object pCol from the result. */
void tiDrawCollisionDrawRing(Matrix4 pTransMat, const unsigned int *pCol)
{
    if (pCol == 0)
    {
        assertFailed("pCol", "../TiDrawCollision.euc.c", 51);
    }
    if (pTransMat == 0)
    {
        assertFailed("pTransMat", "../TiDrawCollision.euc.c", 52);
    }
    tiDrawJointTransformPoints(TI_COLLISION_RING_POINTS, pTransMat, tiDrawCollisionRing, tiDrawCollisionTransformed);
    tiDrawJointScreenPoints(TI_COLLISION_RING_POINTS, tiDrawCollisionTransformed, tiDrawCollisionScreenPoints);
    tiDrawJointPackPoints(TI_COLLISION_RING_POINTS, tiDrawCollisionScreenPoints, pCol, 0);
}


/* Per-frame collision draw of one object: builds the ring at scale, takes the object transform and fills pCol. */
void tiDrawCollisionDrawObjectRing(const Vector4 *position, const unsigned int *pCol, float scale)
{
    Matrix4 trans_mat;

    xglMatrixStackUnit();
    tiDrawCollisionBuildRing(scale);
    xglCurSetTranslate(position);
    xglCurMatrixGet(trans_mat);
    tiDrawCollisionDrawRing(trans_mat, pCol);
}

void tiDrawCollisionDummy(void)
{
}

/* Collision ring data: zero until tiDrawCollisionBuildRing writes it. */
Vector4 tiDrawCollisionRing[TI_COLLISION_RING_POINTS] = { 0 };
Vector4 tiDrawCollisionTransformed[48] = { 0 };
ScreenVector4 tiDrawCollisionScreenPoints[48] = { 0 };
