#include "common.h"
#include "shared.h"
#include "main/assert_fail.h"
#include "main/TiCameraSolver.h"
#include "main/xgl_camera.h"

#include "main/xgl_2.h"
void tiCameraSolverDummy(void)
{
}

f32 atanf(f32);

/* Horizontal or vertical field of view in degrees from a focal length in millimetres and a frame size in inches
 * (0.03937 converts mm to inch). */
f32 tiCameraFovDegrees(f32 focal_length_mm, f32 frame_size)
{
    f32 half_frame;

    half_frame = frame_size * 0.5f;
    return 2.0f * atanf(half_frame / (focal_length_mm * 0.03937f)) * 57.29578f;
}

/* The scalar and angle readers zero two words of the camera's travel block (fov_velocity and scale of
 * XglCamera.travel) with integer stores; as float members they are stored as 0.0f, which changes the code (.text DIFF).
 * This view of travel.focus..scale keeps those two words int. */
typedef struct TiCameraChannel {
    unsigned char unmodeled_00[4];
    float value;                /* travel.fov */
    int unknown_08;             /* travel.fov_velocity */
    int unknown_0c;             /* travel.scale */
} TiCameraChannel;

extern const char tiCameraAssertPData[];
extern const char tiCameraSourceFile[];
extern const char tiCameraAssertPCam[];
/* Declared void here although TiXacAnimCurve.h gives them a pointer return: the return value is unused, and a
 * pointer return changes the register allocation of the callers (check.py: the dead v0 result forces the camera
 * address into v1). The definitions really return the advanced cursor (a void definition changes TiXacAnimCurve.c), so
 * the header form cannot be used here. */
void xacEvalJointPose(f32 *values, u8 **ppJointHeader, f32 curveParam);
void xacEvalJointValue(f32 curveParam, f32 *values, u8 **ppJointHeader);

void tiCameraReadPose(XglCamera *camera, u8 **ppData, f32 curveParam)
{
    /* Three position values, then three rotation angles, as xacEvalJointPose writes them. */
    f32 pose[6];
    Vector4 rotation;
    /* Unused local: removing it changes the stack frame and the .text bytes. */
    Vector4 unused_frame_slot;

    if (ppData == 0) {
        assertFailed(tiCameraAssertPData, tiCameraSourceFile, 28);
    }
    if (camera == 0) {
        assertFailed(tiCameraAssertPCam, tiCameraSourceFile, 29);
    }
    xacEvalJointPose(pose, ppData, curveParam);
    camera->travel.position.x = pose[0];
    camera->travel.position.y = pose[1];
    camera->travel.position.z = pose[2];
    rotation.x = pose[3];
    rotation.y = pose[4];
    rotation.z = pose[5];
    xglEulerFromRotation(&camera->travel.rotation, &rotation);
}

void tiCameraReadScalar(XglCamera *camera, u8 **ppData, f32 curveParam)
{
    f32 sample;
    TiCameraChannel *channel;

    if (ppData == 0) {
        assertFailed(tiCameraAssertPData, tiCameraSourceFile, 56);
    }
    if (camera == 0) {
        assertFailed(tiCameraAssertPCam, tiCameraSourceFile, 57);
    }
    xacEvalJointValue(curveParam, &sample, ppData);
    channel = (TiCameraChannel *)&camera->travel;
    channel->unknown_08 = channel->unknown_0c = 0;
    channel->value = sample;
}

void tiCameraReadAngle(XglCamera *camera, u8 **ppData, f32 curveParam)
{
    f32 sample;
    TiCameraChannel *channel;

    if (ppData == 0) {
        assertFailed(tiCameraAssertPData, tiCameraSourceFile, 76);
    }
    if (camera == 0) {
        assertFailed(tiCameraAssertPCam, tiCameraSourceFile, 77);
    }
    xacEvalJointValue(curveParam, &sample, ppData);
    channel = (TiCameraChannel *)&camera->travel;
    channel->unknown_0c = 0;
    channel->unknown_08 = 0;
    channel->value = sample * 3.1415927f / 180.0f;
}

/* Camera animation record: three header words, then `version` picks the layout (< 1.1 uses the
 * scalar reader, otherwise the angle reader). The data cursor starts after the header. */
void tiCameraReadRecord(XglCamera *camera, TiCameraRecord *record, f32 value)
{
    u8 *stream;

    stream = record->data;
    if (record->version < 1.1f)
    {
        tiCameraReadPose(camera, &stream, value);
        tiCameraReadScalar(camera, &stream, value);
        return;
    }
    tiCameraReadPose(camera, &stream, value);
    tiCameraReadAngle(camera, &stream, value);
}

/* Data of this TU: the assert strings, in address order. */
const char tiCameraAssertPData[8] = "pData";
const char tiCameraSourceFile[24] = "../TiCameraSolver.euc.c";
const char tiCameraAssertPCam[8] = "pCam";
