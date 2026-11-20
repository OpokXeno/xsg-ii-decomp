#include "common.h"
#include "shared.h"
#include "main/menu_scene.h"
#include "main/ysegf.h"
#include "main/msg_balance.h"
#include "main/xgl_flags.h"
#include "gs.h"
#include "main/game_data.h"
#include "ov02/title_game_init.h"

void *memset(void *, int, unsigned int);

void TitleGameInit(void) {
    memset(&gameWork, 0, sizeof(gameWork));
    xglFlagsInitial();
    menuSceneReset();
    yg2PathResetAll();
    msgBalanceReset();
}
