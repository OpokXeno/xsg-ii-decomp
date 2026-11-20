#include "common.h"
#include "shared.h"
#include "main/game_snap_shot.h"
#include "main/xgl_font.h"
#include "main/xgl_packet.h"
#include "main/game_pause_disp.h"
#include "main/xgl_render.h"

#define PAUSE_TEXT_COLOR 0xFFFFFF /* white */
#define PAUSE_SCREEN_CENTER_X 256

/* 8 quadwords of direct GIF/VIF data: the pause backdrop state added to the packet. */
extern u8 pauseBackdropPacket[128];
/* "Pause" title text, in the game's string encoding (control bytes precede the glyphs). */
extern char pauseTitleText[16];
/* "button: Skip and proceed" hint. */
extern char pauseSkipHintText[48];
/* "START button: Cancel Pause" hint. */
extern char pauseCancelHintText[48];
/* One-shot draw hook: set when the pause starts, cleared after the title is drawn. */
/* One-shot draw hook: adds the pause backdrop to the packet, draws the title, then clears itself. */ void pauseDrawTitleHook(XglPacket *packet);

void DrawShadow(FontDrawCursor *cursor, void *argument)
{
    sceVif1PkAddDirectDataN(cursor->packet, pauseBackdropPacket, 8);
}


/* One-shot draw hook: adds the pause backdrop to the packet, draws the title, then clears itself. */
void pauseDrawTitleHook(XglPacket *packet)
{
    int width;

    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pauseBackdropPacket, 8);
    sceVif1PkCloseDirectHLCode(packet);
    width = xglFontGetStringWidth(pauseTitleText);
    xglFontPrint(PAUSE_SCREEN_CENTER_X - width / 2, 200, PAUSE_TEXT_COLOR, pauseTitleText);
    xglFontFlush();
    pauseDrawHook[0] = 0;
}

/* Draws the "Pause" title centred at the top of the screen when visible is nonzero. */
void pauseDrawTitle(int visible)
{
    int width;

    if (visible != 0) {
        xglFontPrintExtFunc(PAUSE_TEXT_COLOR, DrawShadow, 0);
        width = xglFontGetStringWidth(pauseTitleText);
        xglFontPrint(PAUSE_SCREEN_CENTER_X - width / 2, 200, PAUSE_TEXT_COLOR, pauseTitleText);
    }
}

void pauseStart(void)
{
    pauseDrawHook[0] = pauseDrawTitleHook;
    GameSnapShotCheck();
    pauseDrawTitle(0);
}

void GamePauseDispBG(void)
{
    sceVif1PkRef(xglPacketGetCurrent(), pauseBackdropPacket, 8, 0, 0, 0);
}

/* Draws the two button hints; a zero skip_hidden also draws the skip-and-proceed hint above the cancel hint. */
void pauseDrawHints(int skip_hidden)
{
    int width;

    if (skip_hidden == 0) {
        width = xglFontGetStringWidth(pauseSkipHintText);
        xglFontPrint(PAUSE_SCREEN_CENTER_X - width / 2, 256, PAUSE_TEXT_COLOR, pauseSkipHintText);
    }
    width = xglFontGetStringWidth(pauseCancelHintText);
    xglFontPrint(PAUSE_SCREEN_CENTER_X - width / 2, 288, PAUSE_TEXT_COLOR, pauseCancelHintText);
}

void pauseDrawScreen(void)
{
    pauseDrawTitle(1);
    GamePauseDispBG();
    pauseDrawHints(0);
}

void GamePauseDispEvent(int skip_hidden)
{
    pauseDrawTitle(1);
    pauseDrawHints(skip_hidden);
}

char pauseTitleText[16] = "\013\r\003\031\003Pause\031";

u8 pauseBackdropPacket[128] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 81, 1, 128, 0, 0, 0, 64, 35, 96, 238, 30, 85, 0, 0, 0, 0, 0, 0, 0, 255, 1, 0, 0, 255, 1, 64, 0, 0, 0, 0, 0, 0, 0, 1, 16, 3, 0, 0, 0, 0, 0, 71, 0, 0, 0, 0, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 66, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 248, 111, 0, 0, 247, 113, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 248, 143, 0, 0, 247, 141, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0 };

/* JP: △ button: Skip and proceed
 * EN: (Triangle) button: Skip and proceed */
char pauseSkipHintText[48] = "\013\r\003\014 \200 \242\244\013\r\003\014\200\200\200\031\003 button: Skip and proceed";

char pauseCancelHintText[48] = "\013\r\003\031\003START button: Cancel Pause\031";

extern char mapKowaremonoDataName[];
extern char mapTreasureBoxDataName[];
extern char mapEnemyOrderDataName[];
extern char mapSplineDataName[];
extern char mapTra001PxyName[];
extern char mapTra001TxyName[];
extern char mapTra002PxyName[];
extern char mapTra002TxyName[];
extern char mapTra003PxyName[];
extern char mapTra003TxyName[];
extern char mapTra004PxyName[];
extern char mapTra004TxyName[];
extern char mapTre001PxyName[];
extern char mapTre001TxyName[];
extern char mapTre002PxyName[];
extern char mapTre002TxyName[];
extern char mapTre101PxyName[];
extern char mapTre101TxyName[];
extern char mapSavePPxyName[];
extern char mapSavePTxyName[];
extern char mapSaveP2PxyName[];
extern char mapSaveP2TxyName[];
extern char mapEvsPPxyName[];
extern char mapEvsPTxyName[];
extern char mapRetPPxyName[];
extern char mapRetPTxyName[];
extern char mapRetP2PxyName[];
extern char mapRetP2TxyName[];
extern char mapItem001PxyName[];
extern char mapItem001TxyName[];
extern char mapItem002PxyName[];
extern char mapItem002TxyName[];
extern char mapItem101PxyName[];
extern char mapItem101TxyName[];
extern char mapItem102PxyName[];
extern char mapItem102TxyName[];
extern char mapItem103PxyName[];
extern char mapItem103TxyName[];
extern char mapItem003PxyName[];
extern char mapItem003TxyName[];
extern char mapItem004PxyName[];
extern char mapItem004TxyName[];
extern char mapItem005PxyName[];
extern char mapItem005TxyName[];
extern char mapItem006PxyName[];
extern char mapItem006TxyName[];
extern char mapItem007PxyName[];
extern char mapItem007TxyName[];
extern char mapItem008PxyName[];
extern char mapItem008TxyName[];
extern char mapItem009PxyName[];
extern char mapItem009TxyName[];
extern char mapItem010PxyName[];
extern char mapItem010TxyName[];
const char *mapObjectResourceNames[56] = {
    (char *) mapKowaremonoDataName,
    (char *) mapTreasureBoxDataName,
    (char *) mapEnemyOrderDataName,
    (char *) mapSplineDataName,
    (char *) mapTra001PxyName,
    (char *) mapTra001TxyName,
    (char *) mapTra002PxyName,
    (char *) mapTra002TxyName,
    (char *) mapTra003PxyName,
    (char *) mapTra003TxyName,
    (char *) mapTra004PxyName,
    (char *) mapTra004TxyName,
    (char *) mapTre001PxyName,
    (char *) mapTre001TxyName,
    (char *) mapTre002PxyName,
    (char *) mapTre002TxyName,
    (char *) mapTre101PxyName,
    (char *) mapTre101TxyName,
    (char *) mapSavePPxyName,
    (char *) mapSavePTxyName,
    (char *) mapSaveP2PxyName,
    (char *) mapSaveP2TxyName,
    (char *) mapEvsPPxyName,
    (char *) mapEvsPTxyName,
    (char *) mapRetPPxyName,
    (char *) mapRetPTxyName,
    (char *) mapRetP2PxyName,
    (char *) mapRetP2TxyName,
    (char *) mapItem001PxyName,
    (char *) mapItem001TxyName,
    (char *) mapItem002PxyName,
    (char *) mapItem002TxyName,
    (char *) mapItem101PxyName,
    (char *) mapItem101TxyName,
    (char *) mapItem102PxyName,
    (char *) mapItem102TxyName,
    (char *) mapItem103PxyName,
    (char *) mapItem103TxyName,
    (char *) mapItem003PxyName,
    (char *) mapItem003TxyName,
    (char *) mapItem004PxyName,
    (char *) mapItem004TxyName,
    (char *) mapItem005PxyName,
    (char *) mapItem005TxyName,
    (char *) mapItem006PxyName,
    (char *) mapItem006TxyName,
    (char *) mapItem007PxyName,
    (char *) mapItem007TxyName,
    (char *) mapItem008PxyName,
    (char *) mapItem008TxyName,
    (char *) mapItem009PxyName,
    (char *) mapItem009TxyName,
    (char *) mapItem010PxyName,
    (char *) mapItem010TxyName,
    0,
    0
};
