#include "common.h"
#include "main/xgl_packet.h"
#include "main/xgl_camera.h"
#include "main/xgl_render.h"
#include "main/pad.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "main/xgl_dma.h"

#include "quadword.h"
#include "vu0_macro.h"


/* The camera draws into the GS 4096x4096 primitive space centred on 2048. */
#define CAMERA_GS_COORD_SIZE 4096.0f
#define CAMERA_GS_COORD_CENTER 2048.0f

/* The camera view functions; xglCameraMove is their caller. */
void xglCameraViewScreen(XglCamera *camera, const Vector4 *offset);
/* Builds the frustum corners and the view/inverse transforms from the screen * and travel state, then saves them into the camera. */
void xglCameraViewTravel(XglCamera *camera, Vector4 *offset);
void xglCameraViewVolume(XglCamera *camera, Vector4 *offset);
extern float tanf(float angle);
extern signed char aaiVertexIndex[6][3];

void *memset(void *, int, unsigned int);

/* Pad state forced by xglCameraSetPadOverride for one frame. */
typedef struct CameraPadOverride {
    unsigned short held;
    unsigned short pressed;
    unsigned short unknown_04;
    unsigned char unmodeled_06[2];
    signed char rightX;
    signed char rightY;
    signed char leftX;
    signed char leftY;
    int active;                  /* +0xC (cameraPadOverrideActive) */
} CameraPadOverride;

static CameraPadOverride cameraPadOverride;

/* Clears the per-frame pad override record (cameraPadOverride, flag at +0xC). */
void xglCameraClearPadOverride(void) {
    memset(&cameraPadOverride, 0, sizeof(cameraPadOverride));
}

/* Stores seven arguments into the pad override record and raises its flag. */
void xglCameraSetPadOverride(unsigned short held, unsigned short pressed, unsigned short unknown04, unsigned char rightX, unsigned char rightY, unsigned char leftX, unsigned char leftY) {
    cameraPadOverride.held = held;
    cameraPadOverride.pressed = pressed;
    cameraPadOverride.unknown_04 = unknown04;
    cameraPadOverride.rightX = rightX;
    cameraPadOverride.rightY = rightY;
    cameraPadOverride.leftX = leftX;
    cameraPadOverride.leftY = leftY;
    cameraPadOverride.active = 1;
}

/* Resets the camera's active flag, state and pad port (+0, +4, +8). */
void xglCameraControlInit(XglCamera *camera) {
    XglCamera *cam = camera;
    cam->active = 0;
    cam->state = 0;
    cam->padPort = 1;
}

/* Default-data externs of this TU: Vector4 constants copied by xglVectorCopy_001B2008 (a quadword copy), and the display-size shorts. */
extern Vector4 sClipRange_0;
extern Vector4 sPortEffect_1;
extern Vector4 sShootAngleA_10;
extern Vector4 sShootPlaceA_11;
extern short cameraScreenWidth;
extern short cameraScreenHeight;
/* Copies a Vector4 (quadword) through $2 with a 128-bit load and store. Non-static inline: the * out-of-line definition keeps the parameters in registers, as the original does. Byte-identical * to xglVectorCopy in xgl_light.c (proposed xglVectorCopy); named once there. */ inline void xglVectorCopy_001B2008(Vector4 *destination, const Vector4 *source);

/* Fills the XglCameraScreen block at camera+0x10 with the default screen. */
void xglCameraScreenInit(XglCamera *camera) {
    XglCameraScreen *screen = &camera->screen;

    screen->depthMax = 0x00FFFFFF;
    screen->depthMin = 0;
    xglVectorCopy_001B2008(&screen->clipRange.vector, &sClipRange_0);
    xglVectorCopy_001B2008(&screen->portEffect, &sPortEffect_1);
    screen->resolution.x = cameraScreenWidth;
    screen->resolution.y = cameraScreenHeight;
    screen->resolution.z = cameraScreenWidth;
    screen->resolution.w = cameraScreenHeight;
    screen->window.x = 0.0f;
    screen->window.y = 0.0f;
    screen->window.z = cameraScreenWidth;
    screen->window.w = cameraScreenHeight;
}

extern Vector4 sShootAngleR_2;
extern Vector4 sShootAngleV_3;
extern Vector4 sShootAngleA_4;
extern Vector4 sShootPlaceR_5;
extern Vector4 sShootPlaceV_6;
extern Vector4 sShootPlaceA_7;
extern Vector4 sChaseRefPos_8;
extern Vector4 sChaseIntPos_9;

/* Fills the CameraTravel block at camera+0x90 with the defaults (focus 5.0). */
void xglCameraTravelInit(XglCamera *camera) {
    CameraTravel *travel = &camera->travel;

    travel->focus = 5.0f;
    travel->unknown_78 = 0;
    travel->fov = 0.69813174f;
    travel->fov_velocity = 0.0f;
    travel->scale = 0.0f;
    xglVectorCopy_001B2008(&travel->rotation, &sShootAngleR_2);
    xglVectorCopy_001B2008(&travel->angle_velocity, &sShootAngleV_3);
    xglVectorCopy_001B2008(&travel->angle_delta, &sShootAngleA_4);
    xglVectorCopy_001B2008(&travel->position, &sShootPlaceR_5);
    xglVectorCopy_001B2008(&travel->place_velocity, &sShootPlaceV_6);
    xglVectorCopy_001B2008(&travel->place_delta, &sShootPlaceA_7);
    travel->chase_yaw = travel->rotation.y;
    travel->chase_distance = travel->position.z;
    xglVectorCopy_001B2008(&travel->chase_reference, &sChaseRefPos_8);
    xglVectorCopy_001B2008(&travel->chase_position, &sChaseIntPos_9);
}

void xglCameraTravelReset(XglCamera *camera) {
    CameraTravel *travel = &camera->travel;

    travel->scale = 0.0f;
    xglVectorCopy_001B2008(&travel->angle_delta, &sShootAngleA_10);
    xglVectorCopy_001B2008(&travel->place_delta, &sShootPlaceA_11);
}

/* Zoom step from the pad: focus +-1 within 1..9, scale from two constants. */
extern int cameraPadOverrideActive;
extern unsigned short cameraPadOverridePressed;

void xglCameraTravelFocus(XglCamera *camera) {
    CameraTravel *travel = &camera->travel;
    unsigned short held;
    unsigned short pressed;

    held = PadData[1].held | PadData[camera->padPort].held;
    pressed = PadData[1].pressed | PadData[camera->padPort].pressed;
    if (cameraPadOverrideActive) {
        held = cameraPadOverride.held;
        pressed = cameraPadOverridePressed;
    }
    if ((held & (PAD_R2 | PAD_L1 | PAD_R1)) == 0) {
        if (pressed & PAD_CIRCLE) {
            if (travel->focus < 9.0f) {
                travel->focus = travel->focus + 1.0f;
            }
        }
        if (pressed & PAD_SQUARE) {
            if (travel->focus > 1.0f) {
                travel->focus = travel->focus - 1.0f;
            }
        }
    }
    if (held & PAD_R1) {
        travel->scale = 0.0011170107f;
    }
    if (held & PAD_R2) {
        travel->scale = -0.0011170107f;
    }
}

/* Component-wise product of two vectors (vmul.xyzw on vf20/vf21). */ void xglVectorMul(register AlignedVector4 *destination, register const AlignedVector4 *first, register const AlignedVector4 *second);
/* Byte-identical to xglVectorAddXYZ in xgl_light.c (xglVectorAddXYZ); named once there. */ void xglVectorAddXYZ_001B20B0(register AlignedVector4 *destination, register AlignedVector4 *first, register AlignedVector4 *second);

/* Scales travel scale and deltas by focus-dependent factors, x4 while button 0x40 is held. */
void xglCameraTravelScale(XglCamera *camera) {
    CameraTravel *travel = &camera->travel;
    unsigned short held;

    held = PadData[1].held | PadData[camera->padPort].held;
    if (cameraPadOverrideActive) {
        held = cameraPadOverride.held;
    }
    if (held & PAD_CROSS) {
        travel->scale = travel->scale * 4.0f;
        xglVectorScaleXYZ(&travel->angle_delta, &travel->angle_delta, 4.0f);
        xglVectorScaleXYZ(&travel->place_delta, &travel->place_delta, 4.0f);
        travel->unknown_78 = travel->unknown_78 + 1;
    } else {
        travel->unknown_78 = 0;
    }
    travel->scale = travel->scale * ((travel->focus - 5.0f) * 0.08f + 1.0f);
    xglVectorScaleXYZ(&travel->angle_delta, &travel->angle_delta,
                      (travel->focus - 5.0f) * 0.08f + 1.0f);
    xglVectorScaleXYZ(&travel->place_delta, &travel->place_delta,
                      (travel->focus - 5.0f) * 0.24f + 1.0f +
                          travel->unknown_78 * 0.01f);
}

/* Pad update, free (manual) travel mode (state 2); X1: xglCameraTravelManual. */
void xglCameraTravelManual(XglCamera *camera) {
    CameraTravel *travel = &camera->travel;
    Matrix axes;
    short rightX = PadData[camera->padPort].rightX;
    short leftX = PadData[camera->padPort].leftX;
    short rightY = PadData[camera->padPort].rightY;
    short leftY = PadData[camera->padPort].leftY;
    unsigned char deadZone[4];
    unsigned short held;

    held = PadData[camera->padPort].held | PadData[1].held;
    deadZone[0] = PadData[camera->padPort].axisDeadZone[0];
    deadZone[1] = PadData[camera->padPort].axisDeadZone[1];
    deadZone[2] = PadData[camera->padPort].axisDeadZone[2];
    deadZone[3] = PadData[camera->padPort].axisDeadZone[3];
    if (cameraPadOverrideActive) {
        held = cameraPadOverride.held;
        rightX = cameraPadOverride.rightX;
        leftX = cameraPadOverride.leftX;
        rightY = cameraPadOverride.rightY;
        leftY = cameraPadOverride.leftY;
        deadZone[0] = PadData[0].axisDeadZone[0];
        deadZone[1] = PadData[0].axisDeadZone[1];
        deadZone[2] = PadData[0].axisDeadZone[2];
        deadZone[3] = PadData[0].axisDeadZone[3];
    }
    /* The second pad takes over when its sticks are deflected more than the first pad's (or the override's). */
    if (((rightX < 0) ? -rightX : rightX) + ((leftX < 0) ? -leftX : leftX)
            + ((rightY < 0) ? -rightY : rightY) + ((leftY < 0) ? -leftY : leftY)
        < (((PadData[1].rightX < 0) ? -PadData[1].rightX : PadData[1].rightX)
           + ((PadData[1].leftX < 0) ? -PadData[1].leftX : PadData[1].leftX)
           + ((PadData[1].rightY < 0) ? -PadData[1].rightY : PadData[1].rightY)
           + ((PadData[1].leftY < 0) ? -PadData[1].leftY : PadData[1].leftY))) {
        rightX = PadData[1].rightX;
        leftX = PadData[1].leftX;
        rightY = PadData[1].rightY;
        leftY = PadData[1].leftY;
        deadZone[0] = PadData[1].axisDeadZone[0];
        deadZone[1] = PadData[1].axisDeadZone[1];
        deadZone[2] = PadData[1].axisDeadZone[2];
        deadZone[3] = PadData[1].axisDeadZone[3];
    }

    xglCameraTravelReset(camera);
    xglCameraTravelFocus(camera);
    if (rightY < 0) {
        travel->angle_delta.x = (rightY + deadZone[1]) * -6.981317e-05f;
    }
    if (rightY > 0) {
        travel->angle_delta.x = (rightY - deadZone[1]) * -6.981317e-05f;
    }
    if (rightX < 0) {
        travel->angle_delta.y = (rightX + deadZone[0]) * -6.981317e-05f;
    }
    if (rightX > 0) {
        travel->angle_delta.y = (rightX - deadZone[0]) * -6.981317e-05f;
    }
    if (held & 0x8000) {
        travel->angle_delta.z = 0.0022340214f;
    }
    if (held & 0x2000) {
        travel->angle_delta.z = -0.0022340214f;
    }
    if (leftX < 0) {
        travel->place_delta.x = (leftX + deadZone[2]) * 0.0002f;
    }
    if (leftX > 0) {
        travel->place_delta.x = (leftX - deadZone[2]) * 0.0002f;
    }
    if (held & 0x1) {
        travel->place_delta.y = -0.0064f;
    }
    if (held & 0x4) {
        travel->place_delta.y = 0.0064f;
    }
    if (leftY < 0) {
        travel->place_delta.z = (leftY + deadZone[3]) * 0.0002f;
    }
    if (leftY > 0) {
        travel->place_delta.z = (leftY - deadZone[3]) * 0.0002f;
    }

    xglCameraTravelScale(camera);
    travel->fov_velocity = travel->fov_velocity * 0.8f + travel->scale;
    travel->fov = travel->fov + travel->fov_velocity;
    xglVectorScaleXYZ(&travel->angle_velocity, &travel->angle_velocity, 0.8f);
    xglVectorScaleXYZ(&travel->place_velocity, &travel->place_velocity, 0.8f);
    xglVectorAddXYZ_001B20B0(&travel->angle_velocity, &travel->angle_velocity, &travel->angle_delta);
    xglMatrixStackUnit();
    xglMatrixStackRotY(travel->rotation.y);
    xglMatrixStackSave(axes.rows);
    xglVectorScaleAddXYZ(&travel->place_velocity, &axes.vectors[0], travel->place_delta.x, &travel->place_velocity);
    travel->place_velocity.y = travel->place_velocity.y + travel->place_delta.y;
    xglVectorScaleAddXYZ(&travel->place_velocity, &axes.vectors[2], travel->place_delta.z, &travel->place_velocity);
    xglVectorAddXYZ_001B20B0(&travel->rotation, &travel->rotation, &travel->angle_velocity);
    xglVectorAddXYZ_001B20B0(&travel->position, &travel->position, &travel->place_velocity);
}

void xglCameraTravelReset(XglCamera *camera);
void xglCameraTravelFocus(XglCamera *camera);
/* Byte-identical to xglVectorSubXYZ_001AF3F0 in xgl_2.c (xglVectorSubXYZ); named once there. */ void xglVectorSubXYZ_001B20E8(register AlignedVector4 *destination, register AlignedVector4 *minuend, register AlignedVector4 *subtrahend);
extern float atan2f(float y, float x);

/* Pad button bits (PadBlock.held / pressed) that the free camera reads. */
#define CAMERA_BTN_DOWN PAD_L2
#define CAMERA_BTN_UP PAD_L1
#define CAMERA_BTN_ORBIT PAD_CROSS
#define CAMERA_BTN_MOVE_FOCUS PAD_SQUARE
#define CAMERA_BTN_LOCK PAD_START
#define HALF_PI 1.570796f
#define STICK_SCALE 0.0002f /* stick units past the dead zone to camera units */
#define PAN_STEP 0.032f     /* height step per frame, as a fraction of the distance to the focus */
#define ABS(x) ((x) < 0 ? -(x) : (x))

void xglCameraTravelChase(XglCamera *camera) {
    CameraTravel *travel = &camera->travel;
    Vector4 lookDirection;
    Vector4 forward;
    float forwardLength; /* unused by the original after xglVectorLength */
    float distance;
    float pitchScale = 1.0f;
    short rightX = PadData[camera->padPort].rightX;
    short leftX = PadData[camera->padPort].leftX;
    short rightY = PadData[camera->padPort].rightY;
    short leftY = PadData[camera->padPort].leftY;
    unsigned char deadZone[4];
    unsigned short held;
    unsigned short pressed;
    float yaw;
    float angle;

    held = PadData[camera->padPort].held | PadData[1].held;
    pressed = PadData[camera->padPort].pressed | PadData[1].pressed;
    deadZone[0] = PadData[camera->padPort].axisDeadZone[0];
    deadZone[1] = PadData[camera->padPort].axisDeadZone[1];
    deadZone[2] = PadData[camera->padPort].axisDeadZone[2];
    deadZone[3] = PadData[camera->padPort].axisDeadZone[3];
    if (cameraPadOverride.active) {
        held = cameraPadOverride.held;
        pressed = cameraPadOverride.pressed;
        rightX = cameraPadOverride.rightX;
        leftX = cameraPadOverride.leftX;
        rightY = cameraPadOverride.rightY;
        leftY = cameraPadOverride.leftY;
        deadZone[0] = PadData[0].axisDeadZone[0];
        deadZone[1] = PadData[0].axisDeadZone[1];
        deadZone[2] = PadData[0].axisDeadZone[2];
        deadZone[3] = PadData[0].axisDeadZone[3];
    }
    /* The second pad takes over when its sticks are deflected more than the first pad's (or the override's). */
    if (ABS(rightX) + ABS(leftX) + ABS(rightY) + ABS(leftY)
        < ABS(PadData[1].rightX) + ABS(PadData[1].leftX) + ABS(PadData[1].rightY) + ABS(PadData[1].leftY)) {
        rightX = PadData[1].rightX;
        leftX = PadData[1].leftX;
        rightY = PadData[1].rightY;
        leftY = PadData[1].leftY;
        deadZone[0] = PadData[1].axisDeadZone[0];
        deadZone[1] = PadData[1].axisDeadZone[1];
        deadZone[2] = PadData[1].axisDeadZone[2];
        deadZone[3] = PadData[1].axisDeadZone[3];
    }

    xglCameraTravelReset(camera);
    xglCameraTravelFocus(camera);
    xglVectorSubXYZ_001B20E8(&forward, &travel->chase_position, &travel->position);
    xglVectorLength(&forwardLength, &forward);
    xglVectorNormal(&forward, &forward);
    distance = xglPointLengthXZ(&travel->chase_position, &travel->position);

    if (pressed & CAMERA_BTN_LOCK) {
        if (camera->lookAtLocked) {
            camera->lookAtLocked = 0;
        } else {
            camera->lookAtLocked = 1;
        }
    }
    if ((held & CAMERA_BTN_MOVE_FOCUS) || (held & CAMERA_BTN_ORBIT)) {
        if (held & CAMERA_BTN_DOWN) {
            travel->chase_position.y += distance * -PAN_STEP;
        }
        if (held & CAMERA_BTN_UP) {
            travel->chase_position.y += distance * PAN_STEP;
        }
    }
    if (!(held & CAMERA_BTN_ORBIT)) {
        if (held & CAMERA_BTN_DOWN) {
            travel->position.y += distance * -PAN_STEP;
        }
        if (held & CAMERA_BTN_UP) {
            travel->position.y += distance * PAN_STEP;
        }
    }
    if (leftX < 0) {
        travel->place_delta.x = (leftX + deadZone[2]) * STICK_SCALE;
    }
    if (leftX > 0) {
        travel->place_delta.x = (leftX - deadZone[2]) * STICK_SCALE;
    }
    if (travel->place_delta.x != 0.0f) {
        if ((held & CAMERA_BTN_MOVE_FOCUS) && !(held & CAMERA_BTN_ORBIT)) {
            yaw = atan2f(forward.x, forward.z);
            angle = HALF_PI;
            travel->position.x += xglSin(yaw - angle) * travel->place_delta.x * distance;
            travel->position.z += xglCos(yaw - angle) * travel->place_delta.x * distance;
            travel->chase_position.x += xglSin(yaw - angle) * travel->place_delta.x * distance;
            travel->chase_position.z += xglCos(yaw - angle) * travel->place_delta.x * distance;
        }
        if (!(held & CAMERA_BTN_ORBIT) && !(held & CAMERA_BTN_MOVE_FOCUS)) {
            angle = atan2f(forward.x, forward.z);
            angle = angle + travel->place_delta.x;
            travel->position.x = travel->chase_position.x - xglSin(angle) * distance;
            travel->position.z = travel->chase_position.z - xglCos(angle) * distance;
        }
        if ((held & CAMERA_BTN_ORBIT) && !(held & CAMERA_BTN_MOVE_FOCUS)) {
            angle = atan2f(forward.x, forward.z);
            angle = angle - travel->place_delta.x;
            travel->chase_position.x = travel->position.x + xglSin(angle) * distance;
            travel->chase_position.z = travel->position.z + xglCos(angle) * distance;
        }
    }
    if (leftY < 0) {
        travel->place_delta.z = (leftY + deadZone[3]) * STICK_SCALE;
    }
    if (leftY > 0) {
        travel->place_delta.z = (leftY - deadZone[3]) * STICK_SCALE;
    }
    if ((held & CAMERA_BTN_MOVE_FOCUS) || (held & CAMERA_BTN_ORBIT)) {
        travel->chase_position.x -= forward.x * travel->place_delta.z * distance * pitchScale;
        travel->chase_position.y -= forward.y * travel->place_delta.z * distance * pitchScale;
        travel->chase_position.z -= forward.z * travel->place_delta.z * distance * pitchScale;
    }
    if (!(held & CAMERA_BTN_ORBIT)) {
        travel->position.x -= forward.x * travel->place_delta.z * distance * pitchScale;
        travel->position.y -= forward.y * travel->place_delta.z * distance * pitchScale;
        travel->position.z -= forward.z * travel->place_delta.z * distance * pitchScale;
    }

    xglVectorSubXYZ_001B20E8(&lookDirection, &travel->chase_position, &travel->position);
    xglVectorNormal(&lookDirection, &lookDirection);
    lookDirection.z = -lookDirection.z;
    travel->rotation.y = atan2f(lookDirection.x, lookDirection.z);
    travel->rotation.x = atan2f(lookDirection.y, xglSin(travel->rotation.y) * lookDirection.x + xglCos(travel->rotation.y) * lookDirection.z);
    travel->rotation.y = -travel->rotation.y;
}

/* Stores the pad port index at camera+8 (values of 2 or more become 1). */
void xglCameraSetPadPort(XglCamera *camera, int port) {
    camera->padPort = port;
    if (camera->padPort >= 2) {
        camera->padPort = 1;
    }
}

/* Per-frame camera update: pad read, override, state dispatch, then clears the override. */
void xglCameraTravelProc(XglCamera *camera) {
    unsigned short held;
    unsigned short pressed;

    held = PadData[1].held | PadData[camera->padPort].held;
    pressed = PadData[1].pressed | PadData[camera->padPort].pressed;
    if (cameraPadOverrideActive) {
        held = cameraPadOverride.held;
        pressed = cameraPadOverride.pressed;
    }
    switch (camera->state) {
    case 0:
        return;
    case 1:
        break;
    case 2:
        if (held & 0x100) {
            if (pressed & 0x10) {
                xglCameraTravelInit(camera);
            }
        } else {
            xglCameraTravelManual(camera);
        }
        break;
    case 3:
        if (held & 0x100) {
            if (pressed & 0x10) {
                xglCameraTravelInit(camera);
            }
        } else {
            xglCameraTravelChase(camera);
        }
        break;
    case 4:
        break;
    default:
        break;
    }
    xglCameraClearPadOverride();
}

/* The camera view functions; xglCameraMove is their caller. */
void xglCameraViewScreen(XglCamera *camera, const Vector4 *offset) {
    XglCameraScreen *screen = &camera->screen;
    float scale;
    float depthA;
    float depthB;
    Vector4 scaled;
    float offsetX;
    float offsetY;

    offsetX = (screen->window.z + screen->window.x) * 0.5f - screen->resolution.x * 0.5f;
    offsetY = (screen->window.w + screen->window.y) * 0.5f - screen->resolution.y * 0.5f;
    scale = (135.0f / 11.0f) / (54.0f / (float)(((int)(DispEnv.display1.value >> 23) & 0xF) + 1));
    depthA = (float)screen->depthMin;
    depthB = (float)(unsigned int)(screen->depthMax - screen->depthMin);
    if (screen->resolution.z > screen->resolution.w) {
        xglVectorScale(&scaled, &screen->resolution,
                       (CAMERA_GS_COORD_SIZE - __builtin_fabsf(2.0f * offsetX)) / screen->resolution.z);
    } else {
        xglVectorScale(&scaled, &screen->resolution,
                       (CAMERA_GS_COORD_SIZE - __builtin_fabsf(2.0f * offsetY)) / screen->resolution.w);
    }
    screen->clipRange.vector.z = screen->projection.x * (scaled.z / screen->resolution.z);
    screen->clipRange.vector.w = screen->projection.y * (scaled.w / screen->resolution.w);
    xglVectorCopy_001B2008(&screen->projection, &screen->resolution);
    screen->projection.x = screen->projection.x * scale;
    screen->projection.z = screen->projection.z * scale;
    screen->screenOffset.x = offsetX + CAMERA_GS_COORD_CENTER;
    screen->screenOffset.y = offsetY + CAMERA_GS_COORD_CENTER;
    screen->screenOffset.z = depthA + depthB * 0.5f;
    screen->screenOffset.w = (1.0f - screen->portEffect.z) * 255.0f;
    screen->screenScale.x = (scaled.x * screen->portEffect.x) * 0.5f;
    screen->screenScale.y = -((scaled.y * screen->portEffect.y) * 0.5f);
    screen->screenScale.z = -(depthB * 0.5f);
    screen->screenScale.w = (screen->portEffect.z * screen->portEffect.w) * 255.0f;
}

/* Builds the frustum corners and the view/inverse transforms from the screen
 * and travel state, then saves them into the camera. */
void xglCameraViewTravel(XglCamera *camera, Vector4 *offset) {
    XglCameraScreen *screen = &camera->screen;
    CameraTravel *travel = &camera->travel;
    AlignedVector4 scale;
    AlignedVector4 aspect;
    AlignedVector4 position;

    scale.x = tanf(travel->fov * 0.5f);
    scale.y = scale.x;
    scale.z = scale.x * (screen->clipRange.vector.w / screen->projection.y);
    scale.w = scale.z;
    aspect.x = screen->projection.x / screen->projection.y;
    aspect.y = aspect.x;
    aspect.z = screen->clipRange.vector.z / screen->clipRange.vector.w;
    aspect.w = aspect.z;
    travel->frustum.corner[3].x = screen->clipRange.vector.x;
    travel->frustum.corner[3].y = screen->clipRange.vector.y;
    travel->frustum.corner[3].z = travel->frustum.corner[3].x;
    travel->frustum.corner[3].w = travel->frustum.corner[3].y;
    xglVectorMul(&travel->frustum.corner[3], &travel->frustum.corner[3], &scale);
    xglVectorScale(&travel->frustum.corner[2], &travel->frustum.corner[3], -1.0f);
    xglVectorMul(&travel->frustum.corner[1], &travel->frustum.corner[3], &aspect);
    xglVectorScale(&travel->frustum.corner[0], &travel->frustum.corner[1], -1.0f);
    xglMatrixStackUnit();
    xglVectorSubXYZ_001B20E8(&position, &travel->position, offset);
    xglMatrixStackTrans(&position);
    xglMatrixStackRotY(travel->rotation.y);
    xglMatrixStackRotX(travel->rotation.x);
    xglMatrixStackRotZ(travel->rotation.z);
    xglMatrixStackSave(travel->transform);
    xglMatrixStackInverse();
    xglMatrixStackSave(travel->inverseTransform);
}

void xglMatrixCopy_001B2034(Matrix4 *destination, Matrix4 *source);

void xglCameraViewVolume(XglCamera *camera, Vector4 *offset) {
    XglCameraScreen *screen;
    CameraTravel *travel;
    CameraVolume *volume;
    AlignedVector4 corner;
    int x;
    int b;
    int y;
    int i;
    int j;
    int k;

    screen = &camera->screen;
    travel = &camera->travel;
    for (i = 0; i <= 0; i++) {
        volume = &camera->volume[i];
        xglMatrixCopy_001B2034(&volume->frustum, &travel->frustum.matrix);
        xglMatrixStackUnit();
        xglMatrixStackFrustum(volume->frustum[0][2], volume->frustum[1][2], volume->frustum[2][2],
                      volume->frustum[3][2], screen->clipRange.vector.x, screen->clipRange.vector.y);
        xglMatrixStackMul(travel->inverseTransform);
        xglMatrixStackSave(volume->viewMatrix);
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 8; k++) {
                x = k & 1;
                b = ((k & 2) >> 1) | 2;
                y = ((k & 4) >> 2) | ((j & 1) << 1);
                corner.x = volume->frustum[x][y];
                corner.y = volume->frustum[b][y];
                corner.z = -screen->clipRange.component[y & 1];
                corner.w = 1.0f;
                xglVectorMulMat(&volume->vertex[j * 8 + k].vector, travel->transform, &corner);
            }
            {
            for (k = 0; k < 6; k++) {
                xglPlaneParameter(&(volume->plane + j * 6)[k].vector,
                                  &volume->vertex[j * 8 + aaiVertexIndex[k][0]].vector,
                                  &volume->vertex[j * 8 + aaiVertexIndex[k][1]].vector,
                                  &volume->vertex[j * 8 + aaiVertexIndex[k][2]].vector);
            }
            }
        }
        {
        for (j = 0; j < 4; j++) {
            for (k = 0; k < 3; k++) {
                volume->planeSet[0][j][k] = volume->plane[k].component[j];
                volume->planeSet[1][j][k] = volume->plane[k + 3].component[j];
                volume->planeSet[2][j][k] = volume->plane[k + 6].component[j];
                volume->planeSet[3][j][k] = volume->plane[k + 9].component[j];
            }
        }
        }
    }
}

/* Per-camera init, called by xglStudioInit: reset, screen, travel, then camera+0x5F4 = 1.0f. */
void xglCameraInit(XglCamera *camera) {
    xglCameraControlInit(camera);
    xglCameraScreenInit(camera);
    xglCameraTravelInit(camera);
    camera->unknown_5f4 = 1.0f;
}

void xglCameraMove(XglCamera *camera) {
    Vector4 offset;

    offset = (Vector4){0, 0, 0, 0};
    xglCameraViewScreen(camera, &offset);
    xglCameraViewTravel(camera, &offset);
    xglCameraViewVolume(camera, &offset);
}

void xglCameraMoveOffset(XglCamera *camera, Vector4 *offset)
{
    xglCameraViewScreen(camera, offset);
    xglCameraViewTravel(camera, offset);
    xglCameraViewVolume(camera, offset);
}

/* Sets camera->screen.window (x0, y0, x1, y1), sorting each pair and clamping to the screen size. */
void xglCameraSetWindow(XglCamera *camera, int x0, int y0, int x1, int y1) {
    if (camera) {
        int temp;
        XglCameraScreen *screen = &camera->screen;

        if (x0 > x1) {
            temp = x1;
            x1 = x0;
            x0 = temp;
        }
        if (y0 > y1) {
            temp = y1;
            y1 = y0;
            y0 = temp;
        }
        if (x0 < 0) {
            x0 = 0;
        }
        if (x0 >= cameraScreenWidth) {
            x0 = cameraScreenWidth;
        }
        if (x1 < 0) {
            x1 = 0;
        }
        if (x1 >= cameraScreenWidth) {
            x1 = cameraScreenWidth;
        }
        if (y0 < 0) {
            y0 = 0;
        }
        if (y0 >= cameraScreenHeight) {
            y0 = cameraScreenHeight;
        }
        if (y1 < 0) {
            y1 = 0;
        }
        if (y1 >= cameraScreenHeight) {
            y1 = cameraScreenHeight;
        }
        screen->window.x = x0;
        screen->window.y = y0;
        screen->window.z = x1;
        screen->window.w = y1;
    }
}

void xglCameraClipRangeDefault(XglCamera *camera) {
    camera->screen.clipRange.vector.x = 0.08f;
    camera->screen.clipRange.vector.y = 12000.0f;
}

/* Copies a Vector4 (quadword) through $2 with a 128-bit load and store. Non-static inline: the
 * out-of-line definition keeps the parameters in registers, as the original does. Byte-identical
 * to xglVectorCopy in xgl_light.c (proposed xglVectorCopy); named once there. */
inline void xglVectorCopy_001B2008(Vector4 *destination, const Vector4 *source)
{
    QuadwordCopy(destination, source);
}

/* Copies a 4x4 float matrix (four quadwords) through the $2 quadword register. */
void xglMatrixCopy_001B2034(register Matrix4 *destination, register Matrix4 *source) {
    QuadwordCopyMatrix(destination, source);
}

/* Component-wise product of two vectors (vmul.xyzw on vf20/vf21). */
void xglVectorMul(register AlignedVector4 *destination, register const AlignedVector4 *first,
                  register const AlignedVector4 *second) {
    __asm__ __volatile__(
        "lqc2 vf20, 0(%1)\n"
        "lqc2 vf21, 0(%2)\n"
        "vmul.xyzw vf20, vf20, vf21\n"
        "sqc2 vf20, 0(%0)\n"
        : : "r"(destination), "r"(first), "r"(second) : "memory");
}

/* Byte-identical to xglVectorAddXYZ in xgl_light.c (xglVectorAddXYZ); named once there. */
void xglVectorAddXYZ_001B20B0(register AlignedVector4 *destination, register AlignedVector4 *first, register AlignedVector4 *second)
{
    Vu0AddVectorXYZ(destination, first, second);
}

/* Byte-identical to xglVectorSubXYZ_001AF3F0 in xgl_2.c (xglVectorSubXYZ); named once there. */
void xglVectorSubXYZ_001B20E8(register AlignedVector4 *destination, register AlignedVector4 *minuend, register AlignedVector4 *subtrahend)
{
    Vu0SubVectorXYZ(destination, minuend, subtrahend);
}

const unsigned char cullingMapDataTable[1584] = {
    102, 102, 150, 65, 154, 153, 57, 65,
    51, 51, 175, 193, 0, 0, 0, 0,
    78, 81, 37, 191, 0, 0, 0, 0,
    51, 51, 243, 64, 154, 153, 41, 65,
    0, 0, 128, 63, 102, 102, 36, 66,
    51, 51, 115, 192, 0, 0, 64, 193,
    0, 0, 0, 0, 81, 119, 86, 191,
    0, 0, 0, 0, 205, 204, 188, 64,
    102, 102, 150, 64, 0, 0, 128, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 0, 0,
    205, 204, 76, 190, 0, 0, 64, 64,
    51, 51, 83, 193, 0, 0, 0, 0,
    222, 53, 122, 190, 0, 0, 0, 0,
    51, 51, 3, 65, 51, 51, 211, 64,
    0, 0, 128, 63, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 0, 0, 154, 153, 25, 63,
    205, 204, 204, 191, 11, 190, 117, 191,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 64, 102, 102, 38, 64,
    0, 0, 128, 63, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 32, 193, 154, 153, 25, 63,
    51, 51, 67, 65, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 8, 65, 154, 153, 41, 65,
    0, 0, 128, 63, 0, 0, 32, 65,
    154, 153, 25, 63, 51, 51, 67, 65,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 8, 65,
    154, 153, 41, 65, 0, 0, 128, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    51, 51, 35, 193, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 144, 64, 0, 0, 224, 64,
    0, 0, 128, 63, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 128, 191,
    51, 51, 3, 65, 0, 0, 0, 0,
    51, 51, 195, 64, 0, 0, 0, 0,
    219, 15, 73, 191, 0, 0, 0, 0,
    205, 204, 124, 65, 154, 153, 201, 64,
    0, 0, 128, 63, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 128, 191,
    51, 51, 159, 193, 0, 0, 0, 0,
    51, 51, 143, 65, 0, 0, 0, 0,
    219, 15, 201, 191, 0, 0, 0, 0,
    154, 153, 65, 65, 154, 153, 217, 64,
    0, 0, 128, 63, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 219, 15, 201, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    154, 153, 1, 65, 0, 0, 28, 66,
    0, 0, 128, 63, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 128, 191,
    61, 10, 167, 64, 0, 0, 0, 0,
    133, 235, 43, 194, 0, 0, 0, 0,
    81, 119, 86, 63, 0, 0, 0, 0,
    154, 153, 49, 65, 0, 0, 252, 65,
    0, 0, 128, 63, 164, 112, 13, 193,
    0, 0, 0, 0, 61, 10, 203, 193,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 154, 153, 149, 65,
    0, 0, 84, 66, 0, 0, 128, 63,
    123, 20, 14, 192, 0, 0, 0, 0,
    31, 133, 41, 194, 0, 0, 0, 0,
    219, 15, 201, 63, 0, 0, 0, 0,
    0, 0, 0, 65, 154, 153, 7, 66,
    0, 0, 128, 63, 82, 184, 86, 193,
    0, 0, 0, 0, 184, 30, 37, 194,
    0, 0, 0, 0, 219, 15, 201, 63,
    0, 0, 0, 0, 0, 0, 8, 65,
    154, 153, 7, 66, 0, 0, 128, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 0, 0,
    61, 10, 83, 193, 0, 0, 0, 0,
    215, 163, 136, 192, 0, 0, 0, 0,
    189, 195, 16, 63, 0, 0, 0, 0,
    205, 204, 0, 65, 61, 10, 187, 65,
    0, 0, 128, 63, 246, 40, 220, 192,
    0, 0, 0, 0, 20, 174, 167, 192,
    0, 0, 0, 0, 219, 15, 201, 63,
    0, 0, 0, 0, 174, 71, 209, 64,
    61, 10, 187, 65, 0, 0, 128, 63,
    41, 92, 95, 64, 0, 0, 0, 0,
    10, 215, 167, 193, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    205, 204, 0, 65, 61, 10, 187, 65,
    0, 0, 128, 63, 72, 225, 62, 193,
    0, 0, 80, 64, 123, 20, 46, 191,
    219, 15, 201, 63, 0, 0, 0, 0,
    0, 0, 0, 0, 205, 204, 0, 65,
    225, 122, 244, 64, 0, 0, 128, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 0, 0,
    0, 0, 64, 192, 0, 0, 0, 0,
    174, 71, 173, 65, 0, 0, 0, 0,
    219, 15, 201, 63, 0, 0, 0, 0,
    205, 204, 160, 65, 0, 0, 12, 66,
    0, 0, 128, 63, 154, 153, 185, 191,
    0, 0, 0, 0, 82, 184, 134, 192,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 102, 102, 246, 64,
    0, 0, 12, 66, 0, 0, 128, 63,
    72, 225, 182, 65, 0, 0, 0, 0,
    205, 204, 148, 193, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 216, 65, 0, 0, 12, 66,
    0, 0, 128, 63, 72, 225, 238, 65,
    0, 0, 0, 0, 0, 0, 80, 65,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 216, 65,
    0, 0, 12, 66, 0, 0, 128, 63,
    215, 163, 142, 193, 72, 225, 186, 63,
    225, 122, 164, 192, 219, 15, 201, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    51, 51, 27, 65, 51, 51, 67, 65,
    0, 0, 128, 63, 225, 122, 12, 193,
    0, 0, 0, 0, 113, 61, 94, 65,
    0, 0, 0, 0, 219, 15, 201, 63,
    0, 0, 0, 0, 205, 204, 160, 65,
    0, 0, 12, 66, 0, 0, 128, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 219, 15, 201, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 96, 19, 69, 0, 96, 19, 69,
    0, 0, 128, 63, 154, 153, 3, 66,
    0, 0, 0, 0, 0, 0, 176, 192,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 240, 65,
    154, 153, 89, 66, 0, 0, 128, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 0, 0,
    51, 51, 53, 66, 102, 102, 94, 193,
    51, 51, 243, 65, 77, 57, 181, 189,
    76, 178, 20, 61, 195, 184, 50, 190,
    102, 102, 202, 65, 154, 153, 31, 66,
    0, 0, 128, 63, 205, 204, 52, 66,
    205, 204, 204, 63, 102, 102, 62, 193,
    167, 146, 43, 189, 227, 163, 251, 58,
    179, 134, 30, 62, 205, 204, 124, 65,
    113, 61, 254, 65, 0, 0, 128, 63,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 128, 191, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 0, 0,
    102, 102, 214, 192, 0, 0, 0, 0,
    51, 51, 131, 64, 0, 0, 0, 0,
    219, 15, 201, 191, 0, 0, 0, 0,
    0, 0, 176, 64, 205, 204, 252, 64,
    0, 0, 128, 63, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 128, 191,
    0, 0, 128, 191, 0, 0, 128, 191,
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255
};

/* .data of this file (0x645370-0x645580): camera and packet tables (data map review 2026-11-17). */
Vector4 sClipRange_0 = { .x = 0.08f, .y = 14000.0f, .z = 0.0f, .w = 0.0f };

Vector4 sPortEffect_1 = { 1.0f, 1.0f, 1.0f, 100.0f };

Vector4 sShootAngleR_2 = { 0.0f, 0.0f, 0.0f, 1.0f };

Vector4 sShootAngleV_3 = { 0.0f, 0.0f, 0.0f, 1.0f };

Vector4 sShootAngleA_4 = { 0.0f, 0.0f, 0.0f, 1.0f };

Vector4 sShootPlaceR_5 = { 0.0f, 1.0f, 4.0f, 1.0f };

Vector4 sShootPlaceV_6 = { 0.0f, 0.0f, 0.0f, 1.0f };

Vector4 sShootPlaceA_7 = { 0.0f, 0.0f, 0.0f, 1.0f };

Vector4 sChaseRefPos_8 = { 0.0f, 1.0f, 0.0f, 1.0f };

Vector4 sChaseIntPos_9 = { 0.0f, 1.0f, 0.0f, 1.0f };

Vector4 sShootAngleA_10 = { 0.0f, 0.0f, 0.0f, 1.0f };

Vector4 sShootPlaceA_11 = { 0.0f, 0.0f, 0.0f, 1.0f };

signed char aaiVertexIndex[6][3] = { { 0, 2, 4 }, { 1, 0, 5 }, { 0, 1, 2 }, { 3, 1, 7 }, { 2, 3, 6 }, { 4, 6, 5 } };
