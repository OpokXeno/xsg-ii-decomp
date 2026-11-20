#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/sound_driver.h"
#include "main/game_movie.h"
#include "main/runtime_adx.h"


int mpeg2(SbThread *call) {
    int handle;
    char *result;

    handle = vmArgGet(call, 1);
    result = vmKeyedEntryGet(call, handle);
    GameMpeg2Play(result);
    return 1;
}

int adx(SbThread *call) {
    int handle;
    char *result;

    soundSlotSetVolume(0, -1, 0);
    handle = vmArgGet(call, 1);
    result = vmKeyedEntryGet(call, handle);
    adxSetSlotFile(0, result);
    return 1;
}

int adxLoad(SbThread *call) {
    int handle;
    char *fileName;

    soundSlotSetPause(0, 1);
    soundSlotSetVolume(0, -1, 0);
    handle = vmArgGet(call, 1);
    fileName = vmKeyedEntryGet(call, handle);
    adxSetSlotFile(0, fileName);
    return 1;
}

int adxPlay(void) {
    soundSlotSetPause(0, 0);
    return 1;
}
