#include "common.h"
#include "main/save_screen.h"
#include "main/xgl_camera.h"
#include "shared.h"
#include "main/game_over_059.h"
#include "main/gr_screen.h"
#include "main/xgl_jpeg.h"
#include "main/fade_ctrl.h"
#include "main/xgl_thread.h"
#include "main/xenoActor.h"
#include "main/xgl_render.h"
#include "main/xgl_cd.h"
#include "main/xgl_font.h"
#include "main/se_bank_load.h"
#include "main/pad.h"
#include "main/game_camera.h"
#include "gs.h"
#include "main/game_data.h"
#include "main/ytachi_test.h"
#include "main/xgl_packet.h"
#include "main/xgl_studio.h"
#include "main/res_get_path.h"


typedef union GameOverDmaPacket12 {
    u64 words[12];
    XglQuadword quadwords[6];
} GameOverDmaPacket12;

typedef union GameOverDmaPacket20 {
    u64 words[20];
    XglQuadword quadwords[10];
} GameOverDmaPacket20;

void DrawBack(int alpha);
void redraw_frame(void *framebuffer);
extern void FlushCache(int mode);
extern GameOverDmaPacket12 TestEnv_0_002BCD00;
extern GameOverDmaPacket4 TransEnv_1_002BCD60;
extern GameOverDmaPacket6 FlushEnv_2_002BCD80;
extern GameOverDmaPacket20 TestEnv_3_002BCED0;

/* EE RAM staging buffer for the compressed image and the decoded image
 * that DrawImage presents. */
#define IMAGE_LOAD_BUFFER ((u8 *)0x010e0000)
#define IMAGE_FRAME_BUFFER ((u8 *)0x01000000)
/* EE scratchpad RAM, where GameCopyFrame builds its packet. */
#define SCRATCHPAD_BASE 0x70000000
/* Intermission and GameDiskChange load their image file this far past
 * the framebuffer. */
#define IMAGE_LOAD_OFFSET 0x100000
/* The GameEnd map: GameOver shows the ending screen when gameWork.mapId is this. */
#define MAP_ID_GAME_END 0x1B91
/* GameEnd's prompt outcome. */
#define END_CHOICE_SAVE 1
#define END_CHOICE_QUIT 2
/* Sound effects of the prompts. */
#define SE_DECIDE 0x1000002
#define SE_CANCEL 0x1000008

extern GameOverDmaPacket12 TestEnv_4_002BCF70;
extern GameOverDmaPacket12 TestEnv_5_002BCFD0;
extern const char GameEndImagePath[];
extern const char GameEndSaveQuestion[];
extern const char GameEndQuestion[];
extern const char gameOverBgmName[];
extern const char GameOverImagePath[];
extern const char intermissionArchivePath[];
extern const char intermissionSaveQuestion[];
extern const char intermissionNoSaveQuestion[];
void *memset(void *destination, int value, unsigned int count);

void GameSceneReset(void *framebuffer) {
    xglStudioGetCamera2(0)->state = 1;
    xglRenderClearDepth();
    fadeReset();
    xglRenderGlobalFadeInit();
    XenoActorPoolInitDefault();
    uwamonoResetRecords(-1, 1);
    redraw_frame(framebuffer);
    MenuFileMain(0);
}

void DrawImage(void *framebuffer)
{
    XglPacket *packet;
    u16 msb;

    if (framebuffer == 0) {
        return;
    }

    packet = xglPacketGetCurrent();
    /* BITBLTBUF: destination page buffer_select << 5, destination width 8
     * (512 pixels) in bits 48-53. */
    msb = 0x8000;
    TestEnv_0_002BCD00.words[4] = ((u64)sRender.display_buffer_base << 37) | ((u64)msb << 36);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv_0_002BCD00.words, 0x18);
    sceVif1PkRef(packet, TransEnv_1_002BCD60.words, 2, 0, 0, 0);
    sceVif1PkRef(packet, framebuffer, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, TransEnv_1_002BCD60.words, 2, 0, 0, 0);
    sceVif1PkRef(packet, (u8 *)framebuffer + 0x70000, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, FlushEnv_2_002BCD80.words, 3, 0, 0, 0);
}

/* The 28-word VIF payload at 0x002BCDB0 that GameSynopsisDrawPanel sends first. */
typedef struct GameSynopsisPanelHeader {
    u8 unmodeled_00[0x40];
    u64 extent;                 /* +0x40: width in the low word, height in the high word */
    u8 unmodeled_48[0x18];
    int qwc;                    /* +0x60: the image's quadword count */
    u8 unmodeled_64[0x0C];
} GameSynopsisPanelHeader;

/* The 44-word VIF payload at 0x002BCE20 that GameSynopsisDrawPanel sends second. */
typedef struct GameSynopsisPanelRect {
    u8 unmodeled_00[0x6C];
    int alpha;                  /* +0x6C */
    u8 unmodeled_70[0x10];
    int top_left_x;             /* +0x80 */
    int top_left_y;             /* +0x84 */
    u8 unmodeled_88[0x08];
    int width;                  /* +0x90 */
    int height;                 /* +0x94 */
    u8 unmodeled_98[0x08];
    int bottom_right_x;         /* +0xA0 */
    int bottom_right_y;         /* +0xA4 */
    u8 unmodeled_A8[0x08];
} GameSynopsisPanelRect;

extern GameSynopsisPanelHeader gameSynopsisPanelHeaderPacket;
extern GameSynopsisPanelRect gameSynopsisPanelRectPacket;

void GameSynopsisDrawPanel(GameSynopsisPanel *panel, int x, int y, int alpha)
{
    XglPacket *packet;
    u64 extent;
    int origin_x;
    int origin_y;

    if (panel->image == 0) {
        return;
    }
    if (alpha == 0) {
        return;
    }
    packet = xglPacketGetCurrent();
    origin_x = 0x6FF8;
    origin_y = 0x71F7;
    extent = (u64)panel->width | ((u64)panel->height << 32);
    gameSynopsisPanelHeaderPacket.qwc = panel->qwc;
    gameSynopsisPanelRectPacket.alpha = alpha;
    gameSynopsisPanelHeaderPacket.extent = extent;
    gameSynopsisPanelRectPacket.width = panel->width * 16;
    gameSynopsisPanelRectPacket.height = panel->height * 16;
    gameSynopsisPanelRectPacket.top_left_x = x * 16 + origin_x;
    gameSynopsisPanelRectPacket.top_left_y = y * 16 + origin_y;
    gameSynopsisPanelRectPacket.bottom_right_x = (x + panel->width) * 16 + origin_x;
    gameSynopsisPanelRectPacket.bottom_right_y = (y + panel->height) * 16 + origin_y;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &gameSynopsisPanelHeaderPacket, 28);
    sceVif1PkRef(packet, panel->image, panel->qwc, 0, 0x51000000 | panel->qwc, 0);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &gameSynopsisPanelRectPacket, 44);
}

void DrawBack(int alpha)
{
    u64 nonnegative_alpha;
    u64 gs_frame_address_bits;

    nonnegative_alpha = alpha < 0 ? 0 : alpha;
    gs_frame_address_bits = 0xc800;
    TestEnv_3_002BCED0.words[6] =
        (0x24120000 | (sRender.vram.flip_base << 5)) |
        (gs_frame_address_bits << 0x13);
    TestEnv_3_002BCED0.words[8] = (nonnegative_alpha << 32) | 100;
    FlushCache(0);
    sceVif1PkRef(xglPacketGetCurrent(), TestEnv_3_002BCED0.words, 10, 0, 0, 0);
}

/* GS registers of a local-to-local transfer, written through the A+D GIF tag
 * (GS_TEXFLUSH and GIF_AD come from gs.h). */
#define GS_BITBLTBUF 0x50
#define GS_TRXPOS 0x51
#define GS_TRXREG 0x52
#define GS_TRXDIR 0x53
/* TRXDIR 2: local-to-local transfer. */
#define GS_TRXDIR_LOCAL_TO_LOCAL 2

/* One A+D entry: register data, then register address. */
typedef struct GameCopyFrameRegister {
    u64 data;
    u64 address;
} GameCopyFrameRegister;

/* VIF DIRECT header, A+D GIF tag (8 entries) and two GS copies: the second
 * copy reuses the first one's TRXPOS and TRXREG. */
typedef struct GameCopyFramePacket {
    u64 header_padding;
    u64 vif_direct;
    u64 gif_tag;
    u64 gif_registers;
    GameCopyFrameRegister bitbltbuf;
    GameCopyFrameRegister trxpos;
    GameCopyFrameRegister trxreg;
    GameCopyFrameRegister trxdir;
    GameCopyFrameRegister texflush;
    GameCopyFrameRegister second_bitbltbuf;
    GameCopyFrameRegister second_trxdir;
    GameCopyFrameRegister second_texflush;
} GameCopyFramePacket;

#define GAME_COPY_FRAME_PACKET ((GameCopyFramePacket *)SCRATCHPAD_BASE)
#define GAME_COPY_FRAME_QWORDS 10

/* Builds a GS packet in the scratchpad and sends it through VIF1 DIRECT:
 * two TEXFLUSH-terminated BITBLTBUF/TRXPOS/TRXREG/TRXDIR copies of the whole
 * 512x448 screen, page sRender +0x12 to page +0x10, then page +0x10 to the
 * framebuffer page at +0x14 (width 8: 512 pixels, in BITBLTBUF's SBW and
 * DBW fields). */
void GameCopyFrame(void)
{
    XglPacket *packet;
    GameCopyFramePacket *copy;

    packet = xglPacketGetCurrent();
    copy = GAME_COPY_FRAME_PACKET;
    copy->header_padding = 0;
    copy->vif_direct = 0x51000009;
    copy->gif_tag = ((u64)0x10000000 << 32) | 0x8008; /* NREG 1, EOP, NLOOP 8 */
    copy->gif_registers = GIF_AD;
    copy->bitbltbuf.data = (0x80000 | (sRender.vram.height << 5)) |
                           ((u64)(sRender.vram.width << 5) << 32) | ((u64)0x8000 << 36);
    copy->bitbltbuf.address = GS_BITBLTBUF;
    copy->trxpos.data = 0;
    copy->trxpos.address = GS_TRXPOS;
    copy->trxreg.data = ((u64)448 << 32) | 512;
    copy->trxreg.address = GS_TRXREG;
    copy->trxdir.data = GS_TRXDIR_LOCAL_TO_LOCAL;
    copy->trxdir.address = GS_TRXDIR;
    copy->texflush.data = 0;
    copy->texflush.address = GS_TEXFLUSH;
    copy->second_bitbltbuf.data = (0x80000 | (sRender.vram.width << 5)) |
                                  ((u64)sRender.vram.flip_base << 37) | ((u64)0x8000 << 36);
    copy->second_bitbltbuf.address = GS_BITBLTBUF;
    copy->second_trxdir.data = GS_TRXDIR_LOCAL_TO_LOCAL;
    copy->second_trxdir.address = GS_TRXDIR;
    copy->second_texflush.data = 0;
    copy->second_texflush.address = GS_TEXFLUSH;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, GAME_COPY_FRAME_PACKET, GAME_COPY_FRAME_QWORDS);
    sceVif1PkCloseDirectHLCode(packet);
}

void redraw_frame(void *framebuffer) {
    xglSleep();
    DrawImage(framebuffer);
    xglSleep();
    DrawImage(framebuffer);
    xglSleep();
}

/* One frame of the fade-out: present the image, program the ALPHA_1
 * register of the fade packet with the new level, send the packet and wait
 * for the next frame. It stays a do-while statement macro, as in X1's
 * game_over.c: written inline or as a static inline function, GCC hoists the
 * alpha step of GameEnd's fade loop above the per-frame outcome check.
 * GameOver's fade writes the same steps inline (see there). */
#define DRAW_FADE_FRAME(env, alpha)                                     \
    do {                                                                \
        DrawImage(IMAGE_FRAME_BUFFER);                          \
        (env)[4] = GS_SET_ALPHA(2, 0, 2, 1, (alpha));                   \
        FlushCache(0);                                                  \
        sceVif1PkRef(xglPacketGetCurrent(), (env), 6, 0, 0, 0);         \
        xglSleep();                                                     \
    } while (0)

/* The ending screen: fade the ending image in, ask whether to save, and
 * either reset the scene for the save or (after a second confirmation)
 * fade out. */
const char GameEndImagePath[16] = "yajima\\end.jpg";
/* JP: Save Xenosaga Episode II clear data?\n\n○ button: Yes\n\n× button: No
 * EN: Save Xenosaga Episode II clear data?\n\n(O) button: Yes\n\n(X) button: No */
const char GameEndSaveQuestion[96] = "\x0b" "\x0e" "\x01" "\x01" "\x00" "\x00" "\x00" "\x0d" "\x03" "\x19" "\x03" "Save Xenosaga Episode II clear data?\n\n\x0c" "\x80" "  \xa1" "\xfb" "\x0c" "\x80" "\x80" "\x80" " button: Yes\n\n\x0c" "  \x80" "\xa1" "\xdf" "\x0c" "\x80" "\x80" "\x80" " button: No";
/* JP: Ending without saving.\n\nIs this okay?\n\n○ button: Yes\n\n× button: No
 * EN: Ending without saving.\n\nIs this okay?\n\n(O) button: Yes\n\n(X) button: No */
const char GameEndQuestion[96] = "\x0b" "\x0e" "\x01" "\x01" "\x00" "\x00" "\x00" "\x0d" "\x03" "\x19" "\x03" "Ending without saving.\n\nIs this okay?\n\n\x0c" "\x80" "  \xa1" "\xfb" "\x0c" "\x80" "\x80" "\x80" " button: Yes\n\n\x0c" "  \x80" "\xa1" "\xdf" "\x0c" "\x80" "\x80" "\x80" " button: No";
const char gameOverBgmName[16] = "203_game_over";
const char GameOverImagePath[24] = "yajima\\gameov1.jpg";
const char intermissionArchivePath[16] = "yajima\\im.bin";
/* JP: Save game data?\n\n○ button: Yes\n\n× button: No
 * EN: Save game data?\n\n(O) button: Yes\n\n(X) button: No */
const char intermissionSaveQuestion[80] = "\x0b" "\x0e" "\x01" "\x01" "\x00" "\x00" "\x00" "\x0d" "\x03" "\x19" "\x03" "Save game data?\n\n\x0c" "\x80" "  \xa1" "\xfb" "\x0c" "\x80" "\x80" "\x80" " button: Yes\n\n\x0c" "  \x80" "\xa1" "\xdf" "\x0c" "\x80" "\x80" "\x80" " button: No";
/* JP: Continuing without saving.\n\nIs this okay?\n\n○ button: Yes\n\n× button: No
 * EN: Continuing without saving.\n\nIs this okay?\n\n(O) button: Yes\n\n(X) button: No */
const char intermissionNoSaveQuestion[104] = "\x0b" "\x0e" "\x01" "\x01" "\x00" "\x00" "\x00" "\x0d" "\x03" "\x19" "\x03" "Continuing without saving.\n\nIs this okay?\n\n\x0c" "\x80" "  \xa1" "\xfb" "\x0c" "\x80" "\x80" "\x80" " button: Yes\n\n\x0c" "  \x80" "\xa1" "\xdf" "\x0c" "\x80" "\x80" "\x80" " button: No";

void GameEnd(void)
{
    JpegDecodeControl jpeg;
    u8 *framebuffer = IMAGE_FRAME_BUFFER;
    u8 *jpeg_source = IMAGE_LOAD_BUFFER;
    int alpha;
    /* The fade-in hold counter, then the prompt outcome: one variable in
     * the original (separate hold and outcome variables get other
     * registers). */
    int count;
    int buttons;

    gameWork.resourceId = 10;
    GameCopyFrame();
    xglCdReadFile(GameEndImagePath, jpeg_source, 0, CD_NOTIFY_DUMMY);
    memset(&jpeg, 0, sizeof(jpeg));
    jpeg.source = jpeg_source;
    jpeg.destination = framebuffer;
    xglJpegDecode(&jpeg);
    xglRenderDrawFlipPk(xglPacketGetCurrent());
    xglSleep();

    /* Fade the image in, then hold it for 121 more frames. */
    alpha = 128;
    count = 0;
    for (;;) {
        if (alpha > 0) {
            alpha -= 2;
        } else {
            count += 1;
            if (count >= 121) {
                break;
            }
        }
        DrawImage(framebuffer);
        DrawBack(alpha);
        xglSleep();
    }

    for (;;) {
        DrawImage(framebuffer);
        xglFontPrint(32, 32, -1, GameEndSaveQuestion);
        buttons = PadData[0].pressed;
        if (buttons & PAD_CIRCLE) {
            seVoiceStartDefault(SE_DECIDE);
            GameSceneReset(framebuffer);
            count = END_CHOICE_SAVE;
            break;
        }
        if (buttons & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            do {
                xglSleep();
                DrawImage(framebuffer);
                xglFontPrint(32, 32, -1, GameEndQuestion);
            } while (!(PadData[0].pressed & (PAD_CIRCLE | PAD_CROSS)));
            if (PadData[0].pressed & PAD_CIRCLE) {
                seVoiceStartDefault(SE_DECIDE);
                redraw_frame(framebuffer);
                count = END_CHOICE_QUIT;
                break;
            }
            seVoiceStartDefault(SE_CANCEL);
        }
        xglSleep();
    }

    if (count == END_CHOICE_QUIT) {
        /* Fade to black. The original re-checks the outcome every frame
         * (bne on it after xglSleep), although nothing in the loop changes it. */
        alpha = 4;
        for (;;) {
            DRAW_FADE_FRAME(TestEnv_4_002BCF70.words, alpha);
            if (count != END_CHOICE_QUIT) {
                break;
            }
            if (alpha < 255) {
                alpha += 4;
                if (alpha > 255) {
                    alpha = 255;
                }
            } else {
                break;
            }
        }
    }
    GameCopyFrame();
    xglSleep();
}

/* The game-over screen: play the game-over music and fade the game-over
 * image in, hold it until a decide/START press after 121 frames, then fade
 * to black. */
void GameOver(void)
{
    JpegDecodeControl jpeg;
    u8 *framebuffer;
    u8 *jpeg_source;
    int alpha;
    int frame_count;

    if (gameWork.control & CONTROL_SOFT_RESET) {
        return;
    }
    if (gameWork.mapId == MAP_ID_GAME_END) {
        GameEnd();
    } else {
        xglRenderClearDepth();
        framebuffer = IMAGE_FRAME_BUFFER;
        jpeg_source = IMAGE_LOAD_BUFFER;
        GameCopyFrame();
        seBgmPlay(1, gameOverBgmName, 0x19000);
        xglCdReadFile(GameOverImagePath, jpeg_source, 0, CD_NOTIFY_DUMMY);
        memset(&jpeg, 0, sizeof(jpeg));
        jpeg.source = jpeg_source;
        jpeg.destination = framebuffer;
        xglJpegDecode(&jpeg);

        alpha = 128;
        xglRenderDrawFlipPk(xglPacketGetCurrent());
        frame_count = 0;
        xglSleep();

        for (;;) {
            if (alpha > 0) {
                alpha -= 2;
            } else {
                frame_count += 1;
                if (frame_count >= 121 && (PadData[0].pressed & (PAD_CIRCLE | PAD_START))) {
                    break;
                }
            }
            DrawImage(framebuffer);
            DrawBack(alpha);
            xglSleep();
        }
        xglSleep();

        /* Fade the music out (volume 0, fade length 2) and the image to
         * black. The frame is written out here, not as DRAW_FADE_FRAME:
         * through the macro, the alpha step below compiles with one branch
         * fewer than the original. */
        alpha = 4;
        seVolumeSet(0, 2);
        for (;;) {
            DrawImage(framebuffer);
            TestEnv_5_002BCFD0.words[4] = GS_SET_ALPHA(2, 0, 2, 1, alpha);
            FlushCache(0);
            sceVif1PkRef(xglPacketGetCurrent(), TestEnv_5_002BCFD0.words, 6, 0, 0, 0);
            xglSleep();
            if (alpha < 255) {
                alpha += 4;
                if (alpha > 255) {
                    alpha = 255;
                }
            } else {
                break;
            }
        }
        seBgmPlay(1, 0, 0x19000);
        GameCopyFrame();
        xglSleep();
        xglSleep();
    }
}

/* The chapter intermission: unpack the intermission image archive, fade
 * the chapter's image in, then ask whether to save (decide resets the
 * scene for the save menu, cancel asks again). */
void Intermission(int chapter)
{
    JpegDecodeControl jpeg;
    u8 *framebuffer;
    u32 *archive;
    /* The archive's header size, then the address of the image data that
     * follows the header (one local in the original: header size and data
     * address share a register). */
    u32 data;
    u32 *end;
    u32 *entry;
    int alpha;
    int count;
    int buttons;

    if (gameWork.loadStage == 1) {
        return;
    }
    gameWork.resourceId = chapter + 1;
    GameCopyFrame();
    framebuffer = GameResourceGetFreeAddr(0);
    archive = (u32 *)(framebuffer + IMAGE_LOAD_OFFSET);
    xglCdReadFile(intermissionArchivePath, archive, 0, CD_NOTIFY_DUMMY);

    /* The archive starts with its header size and a table of image
     * offsets relative to the 64-byte aligned end of the header: turn
     * them into addresses. */
    data = archive[0];
    end = (u32 *)((u8 *)archive + data);
    data = (u32)archive + ((data + 63) & ~63);
    archive[0] = data;
    for (entry = &archive[1]; entry < end; entry++) {
        *entry += data;
    }

    memset(&jpeg, 0, sizeof(jpeg));
    alpha = 128;
    count = 0;
    jpeg.source = (u8 *) archive[chapter];
    jpeg.source = (u8 *)archive[chapter];
    jpeg.destination = framebuffer;
    xglJpegDecode(&jpeg);
    xglRenderDrawFlipPk(xglPacketGetCurrent());
    xglSleep();

    for (;;) {
        if (alpha > 0) {
            alpha -= 2;
        } else {
            count += 1;
            if (count >= 6) {
                break;
            }
        }
        DrawImage(framebuffer);
        DrawBack(alpha);
        xglSleep();
    }

    for (;;) {
        DrawImage(framebuffer);
        xglFontPrint(32, 32, -1, intermissionSaveQuestion);
        buttons = PadData[0].pressed;
        if (buttons & PAD_CIRCLE) {
            seVoiceStartDefault(SE_DECIDE);
            gameWork.mapId = gameWork.nextMap;
            GameSceneReset(framebuffer);
            break;
        }
        if (buttons & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            do {
                xglSleep();
                DrawImage(framebuffer);
                xglFontPrint(32, 32, -1, intermissionNoSaveQuestion);
            } while (!(PadData[0].pressed & (PAD_CIRCLE | PAD_CROSS)));
            if (PadData[0].pressed & PAD_CIRCLE) {
                seVoiceStartDefault(SE_DECIDE);
                redraw_frame(framebuffer);
                break;
            }
            seVoiceStartDefault(SE_CANCEL);
        }
        xglSleep();
    }
    GameCopyFrame();
    xglSleep();
}

GameOverDmaPacket12 TestEnv_0_002BCD00 = {
    { 0, 0x5100000500000000UL, 0x1000000000000004UL, 14, 0, 80, 0, 81,
      0x1C000000200UL, 82, 0, 83 }
};

GameOverDmaPacket4 TransEnv_1_002BCD60 = {
    { 0, 0x5100000100000000UL, 0x800000000007000UL, 0 }
};

GameOverDmaPacket6 FlushEnv_2_002BCD80 = {
    { 0, 0x5100000200000000UL, 0x1000000000008001UL, 14, 0, 63 }
};

GameSynopsisPanelHeader gameSynopsisPanelHeaderPacket = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 81,
      4, 0, 0, 0, 0, 0, 0, 16, 14, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 56, 8, 0, 80, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0, 0 },
    0x0UL,
    { 82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 83, 0, 0, 0, 0, 0, 0, 0 },
    28672,
    { 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0 }
};

GameSynopsisPanelRect gameSynopsisPanelRectPacket = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 81,
      1, 128, 0, 0, 0, 64, 171, 144, 238, 238, 49, 53, 5, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 63, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 3, 0, 0, 0, 0, 0, 71, 0, 0, 0, 0, 0, 0, 0,
      0, 56, 2, 100, 6, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0,
      68, 0, 0, 0, 128, 0, 0, 0, 66, 0, 0, 0, 0, 0, 0, 0,
      128, 0, 0, 0, 128, 0, 0, 0, 128, 0, 0, 0 },
    128,
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    28664,
    29175,
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    8192,
    3584,
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    36856,
    36343,
    { 0, 0, 0, 0, 0, 0, 0, 0 }
};

GameOverDmaPacket20 TestEnv_3_002BCED0 = {
    { 0, 0x5000000900000000UL, 0x80AB400000008001UL, 0x53531E6EUL, 0x30000UL, 71, 0, 6,
      0, 66, 0x8000000080UL, 0x8000000080UL, 0, 0, 0x71F800006FF8UL, 0,
      0x1C0000002000UL, 0, 0x8DF800008FF8UL, 0 }
};

GameOverDmaPacket12 TestEnv_4_002BCF70 = {
    { 0, 0x5000000500000000UL, 0x4023400000008001UL, 21790, 0, 66, 0x8000000080UL, 0x8000000080UL,
      0x71F800006FF8UL, 0, 0x8DF800008FF8UL, 0 }
};

GameOverDmaPacket12 TestEnv_5_002BCFD0 = {
    { 0, 0x5000000500000000UL, 0x4023400000008001UL, 21790, 0, 66, 0x8000000080UL, 0x8000000080UL,
      0x71F800006FF8UL, 0, 0x8DF800008FF8UL, 0 }
};
