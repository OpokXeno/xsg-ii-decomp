#include "common.h"
#include "main/xgl_thread.h"
#include "main/elevator.h"
#include "main/sound_driver.h"
#include "main/pad.h"
#include "main/tya_display_setting.h"
#include "main/tya_draw_gauge.h"
#include "main/xgl_menu.h"
#include "main/xgl_cd.h"
#include "main/xgl_render.h"
#include "main/yajima_test.h"
#include "main/xgl_font.h"
#include "main/game_over_065.h"

#include "main/xgl_2.h"
#define NULL ((void *) 0)


/* Calls tyaDrawGauge, registers the elevator "id" module and resets the
 * elevator records (elvReset), then runs the display adjustment screen into
 * the framebuffer at 0x01000000. */
void tyaRunDisplaySetting(void) {
    /* passes a null gauge: tyaDrawGauge reads its arguments from address 0
     * (no test in tyaDrawGauge.s) */
    tyaDrawGauge(NULL);
    elvReset();
    tyaDisplaySetting((u8 *)0x01000000);
}


extern void (*yajimaTestActions[10])(int selection);
extern XglMenuRow yajimaTestRows[];
extern XglMenuNode yajimaTestMenuNode;
extern XglMenuParams yajimaTestMenu;
extern const char selectText[];
extern const char clockDateTimeFormat[];


/* Debug menu loop ("SELECT" title, clock line); the selected row runs its
 * entry of yajimaTestActions with the 1-based selection still in $a0. */
void YajimaTest(void) {
    int selection;
    XglClock clock;

    PadData[0].horizontalDeadZone = 0x40;
    PadData[0].verticalDeadZone = 0x40;
    xglRenderClearFrame();
    xglRenderClearColor(0x80004000);

    for (;;) {
        xglMenuInitial();
        selection = 0;
        while (yajimaTestRows[selection].text != 0) {
            selection++;
        }
        yajimaTestMenuNode.item_count = selection;
        selection = 0;
        yajimaTestMenu.result_out = &selection;
        xglMenuOpen(XGL_MENU_SLOT_ANY, &yajimaTestMenu);
        while (selection == 0) {
            xglFontDebugPrintf(0, 0, selectText);
            xglMenuDraw();
            xglClockRead(&clock);
            xglFontDebugPrintf(0x80, 0x10, clockDateTimeFormat, clock.year, clock.month,
                               clock.day, clock.hour, clock.minute, clock.second);
            /* Draws a random number each frame and drops it: only the
             * generator's state advances. */
            xglFRand();
            xglSleep();
        }
        if (selection == -1) {
            break;
        }
        yajimaTestActions[selection - 1](selection);
        xglSleep();
    }
    xglRenderClearDepth();
}


/* Channel level 0x5A (bgm fade). Byte-identical to actionSoundSlotLevelSet. */
void bgmSlotLevelSet(int index) {
    soundSlotSetVolume(index, 0x5A, 0);
}

/* Channel level 0x5A; same body as bgmSlotLevelSet. */
void actionSoundSlotLevelSet(int index) {
    soundSlotSetVolume(index, 0x5A, 0);
}

/* Channel level 0x7A; same body as streamSetLevel. */
void streamKeySlotLevelSet(int index) {
    soundSlotSetVolume(index, 0x7A, 0);
}

/* Channel level 0x7A (stream play). Byte-identical to streamKeySlotLevelSet. */
int streamSetLevel(int index) {
    return soundSlotSetVolume(index, 0x7A, 0);
}

void (*yajimaTestActions[10])(int selection) = {
    FontTest,
    PadTest,
    HddTest,
    MemcardTest,
    CdSlotTest,
    IpuMovieTest,
    SoundTest,
    MovieListTest,
    KanjiCheckTest,
    EdgeTest
};

extern const char fontLabel[];
extern const char padLabel[];
extern const char hddLabel[];
extern const char memcardLabel[];
extern const char resourceLabel[];
extern const char ipuLabel[];
extern const char soundLabel[];
extern const char captionLabel[];
extern const char nisuiLabel[];
extern const char edgeLabel[];

XglMenuRow yajimaTestRows[11] = {
    { fontLabel, { .result = 1 } },
    { padLabel, { .result = 2 } },
    { hddLabel, { .result = 3 } },
    { memcardLabel, { .result = 4 } },
    { resourceLabel, { .result = 5 } },
    { ipuLabel, { .result = 6 } },
    { soundLabel, { .result = 7 } },
    { captionLabel, { .result = 8 } },
    { nisuiLabel, { .result = 9 } },
    { edgeLabel, { .result = 10 } },
    { 0, { .result = -1 } }
};

const char clockDateTimeFormat[24] = "%4d.%2d.%2d\n%2d.%2d.%2d";

XglMenuNode yajimaTestMenuNode = { 0, { -1, 1, 8 }, 0, { 0, 0, 0 }, 0, 0, { 0 }, yajimaTestRows };

XglMenuParams yajimaTestMenu = { 1, 0, { 0, 0 }, 8, 16, 0, { 0, 0 }, -1, &yajimaTestMenuNode, 0 };

const char selectText[8] = "SELECT";
const char elevatorNativeNamePreload[8] = "preload";
const char elevatorNativeNamePreren[8] = "preren";
const char elevatorNativeNameKeepOff[8] = "keepoff";
const char elevatorNativeNameElvMove[8] = "elvMove";
