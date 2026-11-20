#include "common.h"
#include "shared.h"
#include "main/xgl_thread.h"
#include "main/xgl_render.h"
#include "main/xgl_studio.h"
#include "main/xgl_cd.h"


void xglSleep(void) {
    xglRenderFrame();
    xglCdControlThread();
    xglStudioUpdate();
}
