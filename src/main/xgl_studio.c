#include "common.h"
#include "main/xgl_camera.h"
#include "shared.h"
#include "main/xgl_studio.h"



/*
 * XglCamera (include/shared.h; config/header-canon.json,
 * decisions.XglCamera) is the active-flag/state/camera-slot layout this
 * TU evidences. xglStudioSelectGetActiveCamera (saveWriteImage +0x970,
 * `lw $3,0($4)` feeding the `bne $3,$0` "camera in use" test) reads +0 as a
 * boolean active flag, and xglStudioMainCameraInit (saveWriteImage +0xA48/
 * saveWriteImage +0xA50, `sw $17,4($2)` / `sw $17,0($2)` with $17=1) stores the same
 * word 1 to both +4 and +0 on init. The same function's
 * `addiu $4,$4,1520` stride (saveWriteImage +0x978) sizes one camera slot at
 * 0x5F0 bytes, not just the 8-byte leading pair. +4 state keeps its name,
 * type and offset, so src/main/game.c's `camera->state == 4` reads the
 * same byte either way.
 */
/*
 * Layout the asm readers use: a studio record is 0x3180 bytes, its cameras
 * start at +0x180 and are 0x600 apart (xglStudioGetCamera2, xglStudioGetActiveCamera,
 * xglStudioSelectGetActiveCamera and xglStudioGetCamera all compute
 * 3*i << 9). StudioLight (0xF0) plus the 0x90 bytes after it put the cameras at +0x180,
 * and XglCamera is 0x600 bytes (a 0x5F8 camera object plus padding).
 */
typedef struct Studio {
    StudioLight light;
    u8 unmodeled_f0[0x90];
    XglCamera cameras[8];
} Studio;

static Studio *pCurrentStudio;

int studioSceneFlag = 0;

void studioSceneFlagSet(void) {
    studioSceneFlag = 1;
}

void studioSceneFlagClear(void) {
    studioSceneFlag = 0;
}

/* The studio records, 0x3180 bytes each: four of them, as one byte array because the code adds
 * studio_index * 0x3180 to the byte pointer. */
static unsigned char asStudioSource[4 * 0x3180];

/* Makes studio_index the current studio: pCurrentStudio points at its record. */
void xglStudioChange(int studio_index)
{
    pCurrentStudio = (Studio *) (asStudioSource + studio_index * 0x3180);
}

/* The studio index that xglStudioMainCameraMove selects and xglStudioMove compares with. */
static int activeStudioIndex;

void xglStudioSetActiveIndex(int studio_index)
{
    activeStudioIndex = studio_index;
}

void xglStudioGetCamera(XglCamera **camera, int camera_index)
{
    *camera = &pCurrentStudio->cameras[camera_index];
}

XglCamera *xglStudioGetActiveCamera(void)
{
    XglCamera *camera;
    int camera_index;

    for (camera_index = 0; camera_index < 8; camera_index++) {
        camera = xglStudioGetCamera2(camera_index);
        if (camera->active != 0)
            return camera;
    }
    return 0;
}

XglCamera *xglStudioSelectGetActiveCamera(int studio_index)
{
    XglCamera *camera;
    int camera_index;

    for (camera_index = 0; camera_index < 8; camera_index++) {
        camera = &((Studio *)(asStudioSource + studio_index * 0x3180))->cameras[camera_index];
        if (camera->active != 0)
            return camera;
    }
    return 0;
}

void xglStudioFlushActiveCamera(void)
{
    int studio_index;
    XglCamera *camera;

    for (studio_index = 0; studio_index < 4; studio_index++) {
        camera = xglStudioSelectGetActiveCamera(studio_index);
        if (camera != 0)
            xglCameraMove(camera);
    }
}

int studioCameraUpdateEnabled = 1;

void xglStudioDisableCameraUpdate(void) {
    studioCameraUpdateEnabled = 0;
}

void xglStudioEnableCameraUpdate(void) {
    studioCameraUpdateEnabled = 1;
}

StudioLight *xglStudioGetLight2(void)
{
    return &pCurrentStudio->light;
}

XglCamera *xglStudioGetCamera2(int camera_index)
{
    return &pCurrentStudio->cameras[camera_index];
}

void xglStudioMainCameraInit(void)
{
    XglCamera *camera;
    int studio_index;

    for (studio_index = 0; studio_index < 4; studio_index++) {
        xglStudioChange(studio_index);
        camera = xglStudioGetCamera2(0);
        camera->active = 1;
        camera->state = 1;
    }
    xglStudioChange(0);
}

void xglStudioMainCameraMove(void) {
    XglCamera *camera;

    xglStudioChange(activeStudioIndex);
    /* xglStudioGetCamera2(0) is the active studio's first camera; the value is not used here. */
    camera = xglStudioGetCamera2(0);
}

int StudioEntrySkip = 0;


/* Resets every studio: its light block and its eight cameras, then selects studio 0. */
void xglStudioInit(void)
{
    int studio_index;
    int camera_index;

    StudioEntrySkip = 0;
    studioSceneFlagClear();
    for (studio_index = 0; studio_index < 4; studio_index++) {
        xglStudioChange(studio_index);
        xglStudioLightReset(xglStudioGetLight2());
        for (camera_index = 0; camera_index < 8; camera_index++) {
            xglCameraInit(xglStudioGetCamera2(camera_index));
        }
    }
    xglStudioChange(0);
    studioCameraUpdateEnabled = 1;
    activeStudioIndex = 0;
}


void xglStudioMove(void)
{
    XglCamera *camera;
    int studio_index;
    int camera_index;

    for (studio_index = 0; studio_index < 4; studio_index++) {
        xglStudioChange(studio_index);
        for (camera_index = 0; camera_index < 8; camera_index++) {
            camera = xglStudioGetCamera2(camera_index);
            if (camera->active == 1) {
                if (studio_index == activeStudioIndex && studioCameraUpdateEnabled)
                    xglCameraTravelProc(camera);
                xglCameraMove(camera);
            }
        }
    }
    xglStudioChange(0);
}

void xglStudioSetup(void) {
    xglStudioInit();
    xglStudioMainCameraInit();
}

void xglStudioUpdate(void) {
    if (StudioEntrySkip == 0) {
        xglStudioMainCameraMove();
        xglStudioMove();
    }
}
