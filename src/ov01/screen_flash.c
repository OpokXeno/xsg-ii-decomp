#include "common.h"
#include "ov01/ov01_task.h"
#include "ov01/screen_flash.h"
#include "ov01/screen_fill.h"

/* Frames the flash stays on screen, and the grey level (R = G = B) of the screen fill. */
#define SCREEN_FLASH_FRAMES 0xA
#define SCREEN_GREY 0xC0

void screenFlashTick(ScreenTask *task);

/* Starts a 10-frame grey screen flash. */
void screenFlashStart(void)
{
    ScreenTask *task;

    task = ov01TaskClaim();
    if (task != 0) {
        task->update = screenFlashTick;
        task->draw = screenFillGray;
        task->framesLeft = SCREEN_FLASH_FRAMES;
    }
}

/* Per-frame tick: counts framesLeft down to zero, then removes the task. */
void screenFlashTick(ScreenTask *task)
{
    if (task->framesLeft > 0) {
        task->framesLeft = task->framesLeft - 1;
        return;
    }
    ov01TaskRelease(task);
}

/* Draw callback of the flash: a grey (SCREEN_GREY) screen fill. */
void screenFillGray(void)
{
    grPacketScreenFill(SCREEN_GREY, SCREEN_GREY, SCREEN_GREY);
}
