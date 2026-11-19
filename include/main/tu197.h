#ifndef INCLUDE_MAIN_TU197_H
#define INCLUDE_MAIN_TU197_H

#include "shared.h"

struct XglCamera;

/* The camera snapshot MSeObjProc mode 0 takes before the object callbacks run. The effect files of ov01 read it
 * to project their points (MMathRotTransPersClip) and to face the camera (rotationMatrix). */
typedef struct MEfCameraParams {
    struct XglCamera *camera;       /* +0x0: xglStudioGetCamera2(0) */
    unsigned char unmodeled_04[0xC];
    Matrix4 viewMatrix;             /* +0x10: camera->volume[0].viewMatrix */
    Vector4 rotation;               /* +0x50: camera->travel.rotation */
    Matrix4 rotationMatrix;         /* +0x60: MMathRotateMatrixYXZ of rotation */
    unsigned char unmodeled_A0[0x20]; /* +0xA0: the .data object is 0xC0 bytes */
} MEfCameraParams;

extern MEfCameraParams MEfCamParams;

/* Functions of src/main/tu197.c that other files call. */

void MSeObjInit(void);
void MSeObjRequestFree(void *object);
void MSeObjShutdown(void);

#endif /* INCLUDE_MAIN_TU197_H */
