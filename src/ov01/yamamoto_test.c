#include "common.h"
#include "main/battle_result.h"
#include "main/xgl_render.h"
#include "main/xgl_thread.h"
#include "main/res_get_path.h"
#include "main/xgl_cd_arena.h"
#include "main/menu_scene.h"
#include "main/msg_state.h"
#include "main/xenoActor.h"
#include "shared.h"
#include "main/xgl_camera.h"
#include "main/xgl_studio.h"
#include "ov01/scene.h"
#include "ov01/tgt_select.h"
#include "ov01/choice_menu.h"
#include "ov01/debug_entry.h"

#define OV01_STUDIO_COUNT 4
#define OV01_STUDIO_CAMERA_COUNT 8

/* Scene driver: runs the setup chain, then the frame loop until the config menu returns 0. */
void ov01StudioCameraSetup(void);

void YamamotoTest(void)
{
    int selectStatus;

    xglRenderClearFrame();
    ClearEnv.value.color_g = 64;
    ClearEnv.value.color_a = 128;
    ClearEnv.value.color_b = 0;
    ClearEnv.value.color_r = 0;
    GameResourceInit((void *) 0x1000000, 0x1000000);
    ov01InitAll();
    ov01StudioCameraSetup();
    menuSceneReset();
    msgResetAll();
    for (;;) {
        ov01SceneEnter();
        if (configMenuRun(0) == 0) {
            break;
        }
        ov01SceneLoop();
        ov01SceneEnd();
        GameResArenaRelease();
        selectStatus = battleRetCodeGet();
        if (selectStatus == 1 || selectStatus == 3) {
            xglCdArenaOpen(0, 0);
            battleResultScene(0);
        }
        xglSleep();
    }
    ov01SceneEnd();
    GameResArenaRelease();
    xglRenderClearFrame();
}


/* Studio camera setup: four studios of eight cameras each; camera 0 of each studio is the main camera. */
void ov01StudioCameraSetup(void)
{
    int studio_index;
    int camera_index;
    XglCamera *camera;

    XenoActorPoolInitDefault();
    xglRenderDispOn();
    xglStudioInit();
    for (studio_index = 0; studio_index < OV01_STUDIO_COUNT; studio_index++) {
        xglStudioChange(studio_index);
        xglStudioMainCameraInit();
        camera = xglStudioGetCamera2(0);
        camera->state = 2;
        xglCameraSetPadPort(camera, 1);
        for (camera_index = 1; camera_index < OV01_STUDIO_CAMERA_COUNT; camera_index++) {
            camera = xglStudioGetCamera2(camera_index);
            xglCameraInit(camera);
            xglCameraMove(camera);
            xglCameraSetPadPort(camera, 0);
            camera->state = 2;
        }
    }
    studioSceneFlagClear();
    xglRenderCpUDispOff();
}

/* Empty stub: does nothing and has no recorded caller. */
void ov01Dummy(void)
{
}
