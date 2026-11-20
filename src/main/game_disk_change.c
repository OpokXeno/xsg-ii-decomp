#include "common.h"
#include "main/xgl_camera.h"
#include "shared.h"
#include "main/xgl_thread.h"
#include "main/xgl_render.h"
#include "main/xgl_cd.h"
#include "main/xgl_font.h"
#include "main/se_bank_load.h"
#include "main/pad.h"
#include "gs.h"
#include "main/game_data.h"
#include "main/game_over_059.h"
#include "main/xgl_jpeg.h"
#include "main/game_disk_change.h"
#include "main/fade_ctrl.h"
#include "main/xgl_flags.h"
#include "main/xgl_packet.h"
#include "main/res_get_path.h"

/* Fixed EE RAM framebuffer that GameDiskChange decodes into for a request of 0x10 and up. */
#define IMAGE_FRAME_BUFFER ((u8 *)0x01000000)
/* EE RAM buffers of the synopsis screen: the panel archive, the compressed
 * background image and the decoded background. */
#define SYNOPSIS_ARCHIVE ((u32 *)0x01E00000) /* lui 0x1E0 */
#define SYNOPSIS_IMAGE_BUFFER ((u8 *)0x01D00000)
#define SYNOPSIS_FRAME_BUFFER ((u8 *)0x01C00000)
/* The 8-bit story progress flag (GameSynopsis asks for disc 2 from 0x4C on). */
#define STORY_PROGRESS_FLAG 0x191
/* Intermission and GameDiskChange load their image file this far past
 * the framebuffer. */
#define IMAGE_LOAD_OFFSET 0x100000
/* The first map of a new game: Title's New Game writes it to gameWork.nextMap. */
#define MAP_ID_NEW_GAME 0x1B59
/* The state Title's Load Game writes after menuRequestLoad. */
#define GAME_STATE_LOAD_SAVE 10
/* Sound effects of the prompts. */
#define SE_DECIDE 0x1000002
#define SE_SYNOPSIS_OPEN 0x1000006


typedef struct GameSynopsisPanelSet {
    void *framebuffer;
    GameSynopsisPanel outgoing;     /* +0x04: archive image 0, shown first, fades out */
    GameSynopsisPanel incoming;     /* +0x14: archive image 1, fades in */
} GameSynopsisPanelSet;
void *memset(void *destination, int value, unsigned int count);
extern const char gameDiskChangeLogFormat[];
extern const char gameDiskChangeImagePath[];
/* The disc messages; GameDiskSetDiscDigit rewrites their disc digit. */
extern u8 gameDiskInsertText[];
/* "Please insert DISC 2 of\nXenosaga Episode II." */
extern u8 gameDiskIncorrectText[];
/* "Incorrect disc.\n\nPlease insert DISC 2 ..." */
extern u8 gameDiskMissingText[];
/* "There is no disc inserted.\n\nPlease insert DISC 2 ..." */
extern u8 gameDiskCheckingText[];
/* "Checking disc..." */
/* Message by identification status -3..0: checking, insert, wrong disc, no disc. */
extern char *gameDiskStatusTexts[4];
extern const char gameFontStyleControl[];
extern int printf(const char *format, ...);
void GameDiskSetDiscDigit(u8 *text, int digit);
extern const char gameSynopsisArchivePath[];
/* "yajima\synopsis\00.jpg": GameSynopsis writes the image number digits. */
extern char gameSynopsisImagePath[];

static GameSynopsisPanelSet GameSynopsisPanels;
const char gameDiskChangeLogFormat[24] = "GameDiskChange:%d\n";
const char gameDiskChangeImagePath[24] = "yajima\\diskchg.jpg";
const char gameSynopsisArchivePath[24] = "yajima\\synopsis.bin";

/* Rewrites each '1' or '2' in the disc message text to the disc number digit. */
void GameDiskSetDiscDigit(u8 *text, int digit) {
    u8 *cursor;
    u8 ch;

    cursor = text;
    ch = *cursor;
    if (ch != 0) {
        do {
            if (ch == '1' || ch == '2') {
                *cursor = digit + '0';
            }
            cursor += 1;
            ch = *cursor;
        } while (ch != 0);
    }
}

/* Asks for the disc number given in the low nibble of request (1 or 2)
 * over the disc-change image until the drive identifies it. A request
 * below 0x10 decodes the image into the free resource memory, 0x10 and up
 * (GameSynopsis passes 0x11 / 0x12) into the fixed framebuffer. Returns -1
 * for any other disc number, else 0. */
int GameDiskChange(int request)
{
    JpegDecodeControl jpeg;
    int fade;
    int disc;
    int disc_mask;
    u8 *framebuffer;
    u8 *image;
    int status;
    int retry;
    int checked;

    disc = request & 0xF;
    /* The original shifts before it rejects the disc number (sllv ahead of
     * the printf call); the mask of a rejected disc is never used. */
    disc_mask = 1 << (disc - 1);
    printf(gameDiskChangeLogFormat, disc);
    if (disc < 1 || disc > 2) {
        return -1;
    }

    /* A block left with break once the requested disc is in the drive. The
     * original's control flow needs the do-while: written as an if block
     * around the prompt, GCC lays the prompt out in a different order. */
    do {
        fade = xglRenderGlobalFadeGet();
        xglRenderGlobalFadeSetPacked(0);
        status = xglCdDiskCheck();
        if (status == -2) {
            status = xglCdDiskCheck();
        }
        if (status > 0 && (status & disc_mask)) {
            break;
        }
        if (request < 0x10) {
            framebuffer = GameResourceGetFreeAddr(0);
        } else {
            framebuffer = IMAGE_FRAME_BUFFER;
        }
        image = framebuffer + IMAGE_LOAD_OFFSET;
        xglCdReadFile(gameDiskChangeImagePath, image, 4, CD_NOTIFY_DUMMY);
        memset(&jpeg, 0, sizeof(jpeg));
        jpeg.source = image;
        jpeg.destination = framebuffer;
        xglJpegDecode(&jpeg);
        retry = 0;
        GameDiskSetDiscDigit(gameDiskInsertText, disc);
        GameDiskSetDiscDigit(gameDiskIncorrectText, disc);
        GameDiskSetDiscDigit(gameDiskMissingText, disc);
        checked = 0;

        /* The disc must be identified on two frames in a row; the
         * archive check runs between them. */
        for (;;) {
            DrawImage(framebuffer);
            status = xglCdDiskCheck();
            if (status == -2) {
                retry = 1;
            }
            if (status > 0 && (status & disc_mask)) {
                if (checked == 1) {
                    break;
                }
                xglCdArcCheck();
                checked = 1;
            } else {
                if (checked) {
                    status = -3;
                }
                checked = 0;
            }
            xglFontPrintDirectOT(0xFFFFFF, gameFontStyleControl);
            xglFontPrint(0x20, 0x40, 0xFFFFFF,
                         retry == 0 ? (const char *)gameDiskInsertText
                         : status <= 0 ? gameDiskStatusTexts[status + 3]
                         : (status & disc_mask) ? (const char *)gameDiskCheckingText
                         : (const char *)gameDiskIncorrectText);
            xglSleep();
        }
    } while (0);
    xglRenderGlobalFadeSetPacked(fade);
    xglSleep();
    xglSleep();
    return 0;
}

/* The synopsis screen: asks for the disc that matches the story progress,
 * unpacks the synopsis panel archive and decodes the background image of
 * the current chapter, then draws the first frames. */
void GameSynopsis(void)
{
    u32 data;
    u32 *end;
    u32 *entry;

    if (gameWork.nextMap == GAME_STATE_LOAD_SAVE) {
        return;
    }
    if (xglFlagsGet8(STORY_PROGRESS_FLAG) < 0x4C) {
        GameDiskChange(0x11);
    } else {
        GameDiskChange(0x12);
    }
    xglRenderClearDepth();
    GameCopyFrame();
    xglCdReadFile(gameSynopsisArchivePath, SYNOPSIS_ARCHIVE, 0, CD_NOTIFY_DUMMY);

    /* The same archive layout as Intermission's: a header size, then
     * offsets relative to the 64-byte aligned end of the header (data
     * holds the header size first, then the data address). */
    entry = SYNOPSIS_ARCHIVE;
    data = *entry;
    end = (u32 *)((u8 *)entry + data);
    data = (u32)entry + ((data + 63) & ~63);
    *entry = data;
    while (++entry < end) {
        *entry += data;
    }

    /* Each panel image follows a 0x50-byte header. */
    GameSynopsisPanels.outgoing.image = (u8 *)SYNOPSIS_ARCHIVE[0] + 0x50;
    GameSynopsisPanels.incoming.image = (u8 *)SYNOPSIS_ARCHIVE[1] + 0x50;
    GameSynopsisPanels.outgoing.width = 0x200;
    GameSynopsisPanels.outgoing.height = 0x40;
    GameSynopsisPanels.outgoing.qwc = 0x2000;
    GameSynopsisPanels.incoming.width = 0x200;
    GameSynopsisPanels.incoming.height = 0x40;
    GameSynopsisPanels.incoming.qwc = 0x2000;

    if (gameWork.nextMap == MAP_ID_NEW_GAME) {
        xglRenderClearFrame();
        GameSynopsisPanels.framebuffer = 0;
    } else {
        /* The background image number: one past the first progress
         * threshold the story progress is below, with two special cases. */
        int chapter = 0;
        u8 progress = xglFlagsGet8(STORY_PROGRESS_FLAG);
        u8 thresholds[21] = {
            9, 13, 21, 23, 30, 37, 43, 51, 59, 66, 68,
            76, 78, 81, 88, 91, 96, 102, 112, 122, 255
        };
        JpegDecodeControl jpeg;

        /* 255 ends the table for every progress value but 255 itself: the
         * original has no bound, so a progress of 255 would read past it. */
        while (progress >= thresholds[chapter]) {
            chapter++;
        }
        chapter++;
        /* The two special cases (their story meaning is not known yet):
         * progress exactly 0x87 shows image 20, and the 1-bit flag 0x279
         * shows image 22 over anything else. */
        if (progress == 0x87) {
            chapter = 20;
        }
        if (xglFlagsGet1(0x279)) {
            chapter = 22;
        }
        gameSynopsisImagePath[16] = chapter / 10 + '0';
        gameSynopsisImagePath[17] = chapter % 10 + '0';
        memset(&jpeg, 0, sizeof(jpeg));
        GameSynopsisPanels.framebuffer = SYNOPSIS_FRAME_BUFFER;
        jpeg.destination = SYNOPSIS_FRAME_BUFFER;
        jpeg.source = SYNOPSIS_IMAGE_BUFFER;
        xglCdReadFile(gameSynopsisImagePath, SYNOPSIS_IMAGE_BUFFER, 0, CD_NOTIFY_DUMMY);
        xglJpegDecode(&jpeg);
    }
    xglRenderDrawFlipPk(xglPacketGetCurrent());
    fadeReset();
    fadeClear();
    xglSleep();
    DrawImage(GameSynopsisPanels.framebuffer);
    GameSynopsisDrawPanel(&GameSynopsisPanels.outgoing,
                          0x200 - GameSynopsisPanels.outgoing.width,
                          0x1C0 - GameSynopsisPanels.outgoing.height, 128);
    xglSleep();
    DrawImage(GameSynopsisPanels.framebuffer);
    GameSynopsisDrawPanel(&GameSynopsisPanels.outgoing,
                          0x200 - GameSynopsisPanels.outgoing.width,
                          0x1C0 - GameSynopsisPanels.outgoing.height, 128);
    xglSleep();
    xglRenderClearDepth();
}

/* Cross-fades the synopsis panels (the outgoing panel fades out while the
 * incoming one fades in) and waits for decide. Returns 1 when the soft-reset
 * button combination was pressed (and sets CONTROL_SOFT_RESET for it), else
 * 0; does nothing for GAME_STATE_LOAD_SAVE and MAP_ID_NEW_GAME. */
int GameSynopsisCrossfadeWait(void)
{
    int alpha;

    alpha = 0;
    if (gameWork.nextMap == GAME_STATE_LOAD_SAVE) {
        return 0;
    }
    if (gameWork.nextMap == MAP_ID_NEW_GAME) {
        return 0;
    }
    seVoiceStartDefault(SE_SYNOPSIS_OPEN);
    for (;;) {
        alpha += 4;
        DrawImage(GameSynopsisPanels.framebuffer);
        if (alpha > 128) {
            alpha = 128;
        }
        GameSynopsisDrawPanel(&GameSynopsisPanels.outgoing,
                              0x200 - GameSynopsisPanels.outgoing.width,
                              0x1C0 - GameSynopsisPanels.outgoing.height, 128 - alpha);
        GameSynopsisDrawPanel(&GameSynopsisPanels.incoming,
                              0x200 - GameSynopsisPanels.incoming.width,
                              0x1C0 - GameSynopsisPanels.incoming.height, alpha);
        if ((PadData[0].heldAndPressed & PAD_SOFT_RESET) == PAD_SOFT_RESET) {
            gameWork.control |= CONTROL_SOFT_RESET;
            return 1;
        }
        if (!(PadData[0].pressed & PAD_CIRCLE)) {
            xglSleep();
            continue;
        }
        seVoiceStartDefault(SE_DECIDE);
        xglSleep();
        DrawImage(GameSynopsisPanels.framebuffer);
        xglSleep();
        DrawImage(GameSynopsisPanels.framebuffer);
        xglSleep();
        return 0;
    }
}

u8 gameDiskInsertText[48] = "Please insert DISC 2 of\nXenosaga Episode II.";

u8 gameDiskIncorrectText[64] = "Incorrect disc.\n\nPlease insert DISC 2 of\nXenosaga Episode II.";

u8 gameDiskMissingText[80] = "There is no disc inserted.\n\nPlease insert DISC 2 of\nXenosaga Episode II.";

u8 gameDiskCheckingText[24] = "Checking disc...";

char *gameDiskStatusTexts[4] = { gameDiskCheckingText, gameDiskInsertText, gameDiskIncorrectText, gameDiskMissingText };

char gameSynopsisImagePath[24] = "yajima\\synopsis\\00.jpg";
