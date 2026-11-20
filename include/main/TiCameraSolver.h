#ifndef INCLUDE_MAIN_TICAMERASOLVER_H
#define INCLUDE_MAIN_TICAMERASOLVER_H

/* Functions of src/main/TiCameraSolver.c that other files call. */

#include "shared.h"
#include "main/xgl_camera.h"

/* Camera animation record: the version float at +4 picks the layout of the curve data that starts at +0xC; the frame
 * range of its playback is at +8 and +0xA (the title scene steps through it). */
typedef struct TiCameraRecord {
    unsigned char unmodeled_00[4];
    f32 version;
    unsigned short frameStart;  /* +8 */
    unsigned short frameEnd;    /* +0xA */
    u8 data[0];
} TiCameraRecord;

void tiCameraReadRecord(XglCamera *camera, TiCameraRecord *record, f32 value);

#endif /* INCLUDE_MAIN_TICAMERASOLVER_H */
