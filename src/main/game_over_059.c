#include "common.h"
#include "main/sdv_033.h"
#include "shared.h"
#include "main/control_entry.h"
#include "main/xenoActor.h"
#include "main/xgl_render.h"
#include "main/xgl_cd.h"
#include "main/xgl_font.h"
#include "main/tu021.h"
#include "main/pad.h"
#include "main/tu051.h"
#include "gs.h"


extern void sceVif1PkRef(XglPacket *packet, const void *environment,
                         int count, int mode, int offset, int flags);
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern XglPacket *xglPacketGetCurrent(void);

typedef unsigned int XglQuadword __attribute__((mode(TI)));

/* DrawBack reads the framebuffer-page halfword at sRender +0x14. */
typedef struct {
    u8 unmodeled_00[0x10];
    u16 width;
    u16 height;
    u16 framebuffer_page;
    u8 unmodeled_16[0x0a];
    u16 buffer_select;
} DrawImageRenderState;

typedef union GameOverDmaPacket4 {
    u64 words[4];
    XglQuadword quadwords[2];
} GameOverDmaPacket4;

typedef union GameOverDmaPacket6 {
    u64 words[6];
    XglQuadword quadwords[3];
} GameOverDmaPacket6;

typedef union GameOverDmaPacket12 {
    u64 words[12];
    XglQuadword quadwords[6];
} GameOverDmaPacket12;

typedef union GameOverDmaPacket20 {
    u64 words[20];
    XglQuadword quadwords[10];
} GameOverDmaPacket20;

void DrawImage(void *framebuffer);
void DrawBack(int alpha);
void redraw_frame(void *framebuffer);
extern void FlushCache(int mode);


extern DrawImageRenderState sRender;
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);
extern GameOverDmaPacket12 TestEnv_0_002BCD00;
extern GameOverDmaPacket4 TransEnv_1_002BCD60;
extern GameOverDmaPacket6 FlushEnv_2_002BCD80;
extern GameOverDmaPacket20 TestEnv_3_002BCED0;
XglCamera *xglStudioGetCamera2(int camera_index);
void uwamonoResetRecords(int index, signed char mode);
void saveScreenRun(int);
void fadeReset(void);


/* The fields of gameWork this file reads and writes. */
typedef struct GameOverWork {
    u8 unmodeled_00[0x06];
    u16 loadedFromSave;         /* +0x06: Title stores saveScreenRun(1)'s result here (1: the game was
                                 * loaded from a save); Intermission then does nothing */
    int control;                /* +0x08: CONTROL_* bits (main/tu051.h) */
    u8 unmodeled_0C[0x08];
    int state;                  /* +0x14: the requested next map (-1 none); Title writes
                                 * MAP_ID_NEW_GAME or GAME_STATE_LOAD_SAVE */
    int mapId;                  /* +0x18: the current map; GameOver runs GameEnd on MAP_ID_GAME_END,
                                 * Intermission copies state here */
    u8 unmodeled_1C[0x04];
    int current_id;             /* +0x20: the current resource id (tu050.c mainSwitchId);
                                 * GameEnd sets 10, Intermission chapter + 1 */
} GameOverWork;

/* The JPEG decoder's 32-byte request: only the source and destination
 * words are set here, memset clears the rest. */
typedef struct JpegDecodeRequest {
    u32 source;
    u32 destination;
    u8 unmodeled_08[0x18];
} JpegDecodeRequest;

/* EE RAM staging buffer for the compressed image and the decoded image
 * that DrawImage presents. */
#define IMAGE_LOAD_BUFFER 0x010e0000
#define IMAGE_FRAME_BUFFER 0x01000000

/* EE scratchpad RAM, where GameCopyFrame builds its packet. */
#define SCRATCHPAD_BASE 0x70000000

/* EE RAM buffers of the synopsis screen: the panel archive, the compressed
 * background image and the decoded background. */
#define SYNOPSIS_ARCHIVE ((u32 *)0x01E00000) /* lui 0x1E0 */
#define SYNOPSIS_IMAGE_BUFFER 0x01D00000
#define SYNOPSIS_FRAME_BUFFER 0x01C00000

/* The 8-bit story progress flag (GameSynopsis asks for disc 2 from 0x4C on). */
#define STORY_PROGRESS_FLAG 0x191

/* Intermission and GameDiskChange load their image file this far past
 * the framebuffer. */
#define IMAGE_LOAD_OFFSET 0x100000

/* The GameEnd map: GameOver shows the ending screen when gameWork.mapId is this. */
#define MAP_ID_GAME_END 0x1B91

/* The first map of a new game: Title's New Game writes it to gameWork.state. */
#define MAP_ID_NEW_GAME 0x1B59

/* The state Title's Load Game writes after menuRequestLoad. */
#define GAME_STATE_LOAD_SAVE 10

/* GameEnd's prompt outcome. */
#define END_CHOICE_SAVE 1
#define END_CHOICE_QUIT 2

/* Sound effects of the prompts. */
#define SE_DECIDE 0x1000002
#define SE_CANCEL 0x1000008
#define SE_SYNOPSIS_OPEN 0x1000006

/* An image of the synopsis screen and its size in pixels and in quadwords. */
typedef struct GameSynopsisPanel {
    void *image;
    int width;
    int height;
    int qwc;
} GameSynopsisPanel;

typedef struct GameSynopsisPanelSet {
    void *framebuffer;
    GameSynopsisPanel outgoing;     /* +0x04: archive image 0, shown first, fades out */
    GameSynopsisPanel incoming;     /* +0x14: archive image 1, fades in */
} GameSynopsisPanelSet;

extern GameSynopsisPanelSet GameSynopsisPanels;
void GameSynopsisDrawPanel(GameSynopsisPanel *panel, int x, int y, int alpha);
extern GameOverWork gameWork;
extern PadRecord PadData;
extern GameOverDmaPacket12 TestEnv_4_002BCF70;
extern GameOverDmaPacket12 TestEnv_5_002BCFD0;
extern const char D_00688488[];
extern const char D_00688498[];
extern const char D_006884F8[];
extern const char D_00688558[];
extern const char D_00688568[];
extern const char D_00688580[];
extern const char D_00688590[];
extern const char D_006885E0[];
void *memset(void *destination, int value, unsigned int count);
int xglJpegDecode(JpegDecodeRequest *request);
void xglRenderDrawFlipPk(XglPacket *packet);
void *func_001DF9F0(unsigned int *size);
extern void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
extern void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);
extern void sceVif1PkCloseDirectHLCode(XglPacket *packet);
extern const char D_00688648[];
extern const char D_00688660[];
/* The disc messages; GameDiskSetDiscDigit rewrites their disc digit. */
extern u8 D_002BD030[]; /* "Please insert DISC 2 of\nXenosaga Episode II." */
extern u8 D_002BD060[]; /* "Incorrect disc.\n\nPlease insert DISC 2 ..." */
extern u8 D_002BD0A0[]; /* "There is no disc inserted.\n\nPlease insert DISC 2 ..." */
extern u8 D_002BD0F0[]; /* "Checking disc..." */
/* Message by identification status -3..0: checking, insert, wrong disc, no disc. */
extern char *D_002BD108[4];
extern const char D_0069D018[];
extern int printf(const char *format, ...);
int xglRenderGlobalFadeGet(void);
void xglRenderGlobalFadeSetPacked(int value);
int xglCdIdentifyDisc(void);
void GameDiskSetDiscDigit(u8 *text, int digit);
extern const char D_00688678[];
/* "yajima\synopsis\00.jpg": GameSynopsis writes the image number digits. */
extern char D_002BD118[];
int xglFlagsGet1(int bit_offset);
int xglFlagsGet8(int bit_offset);
void fadeClear(void);

void GameSceneReset(void *framebuffer) {
    xglStudioGetCamera2(0)->state = 1;
    xglRenderClearDepth();
    fadeReset();
    xglRenderGlobalFadeReset();
    XenoActorPoolInitDefault();
    uwamonoResetRecords(-1, 1);
    redraw_frame(framebuffer);
    saveScreenRun(0);
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
    TestEnv_0_002BCD00.words[4] = ((u64)sRender.buffer_select << 37) | ((u64)msb << 36);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv_0_002BCD00.words, 0x18);
    sceVif1PkRef(packet, TransEnv_1_002BCD60.words, 2, 0, 0, 0);
    sceVif1PkRef(packet, framebuffer, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, TransEnv_1_002BCD60.words, 2, 0, 0, 0);
    sceVif1PkRef(packet, (u8 *)framebuffer + 0x70000, 0x7000, 0, 0x51007000, 0);
    sceVif1PkRef(packet, FlushEnv_2_002BCD80.words, 3, 0, 0, 0);
}

INCLUDE_ASM("asm/main/nonmatchings/game_over_059", GameSynopsisDrawPanel);

void DrawBack(int alpha)
{
    u64 nonnegative_alpha;
    u64 gs_frame_address_bits;

    nonnegative_alpha = alpha < 0 ? 0 : alpha;
    gs_frame_address_bits = 0xc800;
    TestEnv_3_002BCED0.words[6] =
        (0x24120000 | (sRender.framebuffer_page << 5)) |
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
    copy->bitbltbuf.data = (0x80000 | (sRender.height << 5)) |
                           ((u64)(sRender.width << 5) << 32) | ((u64)0x8000 << 36);
    copy->bitbltbuf.address = GS_BITBLTBUF;
    copy->trxpos.data = 0;
    copy->trxpos.address = GS_TRXPOS;
    copy->trxreg.data = ((u64)448 << 32) | 512;
    copy->trxreg.address = GS_TRXREG;
    copy->trxdir.data = GS_TRXDIR_LOCAL_TO_LOCAL;
    copy->trxdir.address = GS_TRXDIR;
    copy->texflush.data = 0;
    copy->texflush.address = GS_TEXFLUSH;
    copy->second_bitbltbuf.data = (0x80000 | (sRender.width << 5)) |
                                  ((u64)sRender.framebuffer_page << 37) | ((u64)0x8000 << 36);
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
        DrawImage((void *)IMAGE_FRAME_BUFFER);                          \
        (env)[4] = GS_SET_ALPHA(2, 0, 2, 1, (alpha));                   \
        FlushCache(0);                                                  \
        sceVif1PkRef(xglPacketGetCurrent(), (env), 6, 0, 0, 0);         \
        xglSleep();                                                     \
    } while (0)

/* The ending screen: fade the ending image in, ask whether to save, and
 * either reset the scene for the save or (after a second confirmation)
 * fade out. */
void GameEnd(void)
{
    JpegDecodeRequest jpeg;
    u32 framebuffer = IMAGE_FRAME_BUFFER;
    u32 jpeg_source = IMAGE_LOAD_BUFFER;
    int alpha;
    /* The fade-in hold counter, then the prompt outcome: one variable in
     * the original (separate hold and outcome variables get other
     * registers). */
    int count;
    int buttons;

    gameWork.current_id = 10;
    GameCopyFrame();
    xglCdReadFile(D_00688488, (void *)jpeg_source, 0, 1);
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
        DrawImage((void *)framebuffer);
        DrawBack(alpha);
        xglSleep();
    }

    for (;;) {
        DrawImage((void *)framebuffer);
        xglFontPrint(32, 32, -1, D_00688498);
        buttons = PadData.pressed;
        if (buttons & PAD_CIRCLE) {
            seVoiceStartDefault(SE_DECIDE);
            GameSceneReset((void *)framebuffer);
            count = END_CHOICE_SAVE;
            break;
        }
        if (buttons & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            do {
                xglSleep();
                DrawImage((void *)framebuffer);
                xglFontPrint(32, 32, -1, D_006884F8);
            } while (!(PadData.pressed & (PAD_CIRCLE | PAD_CROSS)));
            if (PadData.pressed & PAD_CIRCLE) {
                seVoiceStartDefault(SE_DECIDE);
                redraw_frame((void *)framebuffer);
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
    JpegDecodeRequest jpeg;
    u32 framebuffer;
    u32 jpeg_source;
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
        func_001A37E0(1, D_00688558, 0x19000);
        xglCdReadFile(D_00688568, (void *)jpeg_source, 0, 1);
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
                if (frame_count >= 121 && (PadData.pressed & (PAD_CIRCLE | PAD_START))) {
                    break;
                }
            }
            DrawImage((void *)framebuffer);
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
            DrawImage((void *)framebuffer);
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
        func_001A37E0(1, 0, 0x19000);
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
    JpegDecodeRequest jpeg;
    u32 framebuffer;
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

    if (gameWork.loadedFromSave == 1) {
        return;
    }
    gameWork.current_id = chapter + 1;
    GameCopyFrame();
    framebuffer = (u32)func_001DF9F0(0);
    archive = (u32 *)(framebuffer + IMAGE_LOAD_OFFSET);
    xglCdReadFile(D_00688580, archive, 0, 1);

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
    jpeg.source = archive[chapter];
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
        DrawImage((void *)framebuffer);
        DrawBack(alpha);
        xglSleep();
    }

    for (;;) {
        DrawImage((void *)framebuffer);
        xglFontPrint(32, 32, -1, D_00688590);
        buttons = PadData.pressed;
        if (buttons & PAD_CIRCLE) {
            seVoiceStartDefault(SE_DECIDE);
            gameWork.mapId = gameWork.state;
            GameSceneReset((void *)framebuffer);
            break;
        }
        if (buttons & PAD_CROSS) {
            seVoiceStartDefault(SE_CANCEL);
            do {
                xglSleep();
                DrawImage((void *)framebuffer);
                xglFontPrint(32, 32, -1, D_006885E0);
            } while (!(PadData.pressed & (PAD_CIRCLE | PAD_CROSS)));
            if (PadData.pressed & PAD_CIRCLE) {
                seVoiceStartDefault(SE_DECIDE);
                redraw_frame((void *)framebuffer);
                break;
            }
            seVoiceStartDefault(SE_CANCEL);
        }
        xglSleep();
    }
    GameCopyFrame();
    xglSleep();
}

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
    JpegDecodeRequest jpeg;
    int fade;
    int disc;
    int disc_mask;
    u32 framebuffer;
    u32 image;
    int status;
    int retry;
    int checked;

    disc = request & 0xF;
    /* The original shifts before it rejects the disc number (sllv ahead of
     * the printf call); the mask of a rejected disc is never used. */
    disc_mask = 1 << (disc - 1);
    printf(D_00688648, disc);
    if (disc < 1 || disc > 2) {
        return -1;
    }

    /* A block left with break once the requested disc is in the drive. The
     * original's control flow needs the do-while: written as an if block
     * around the prompt, GCC lays the prompt out in a different order. */
    do {
        fade = xglRenderGlobalFadeGet();
        xglRenderGlobalFadeSetPacked(0);
        status = xglCdIdentifyDisc();
        if (status == -2) {
            status = xglCdIdentifyDisc();
        }
        if (status > 0 && (status & disc_mask)) {
            break;
        }
        if (request < 0x10) {
            framebuffer = (u32)func_001DF9F0(0);
        } else {
            framebuffer = IMAGE_FRAME_BUFFER;
        }
        image = framebuffer + IMAGE_LOAD_OFFSET;
        xglCdReadFile(D_00688660, (void *)image, 4, 1);
        memset(&jpeg, 0, sizeof(jpeg));
        jpeg.source = image;
        jpeg.destination = framebuffer;
        xglJpegDecode(&jpeg);
        retry = 0;
        GameDiskSetDiscDigit(D_002BD030, disc);
        GameDiskSetDiscDigit(D_002BD060, disc);
        GameDiskSetDiscDigit(D_002BD0A0, disc);
        checked = 0;

        /* The disc must be identified on two frames in a row; the
         * archive check runs between them. */
        for (;;) {
            DrawImage((void *)framebuffer);
            status = xglCdIdentifyDisc();
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
            xglFontPrintDirectOT(0xFFFFFF, D_0069D018);
            xglFontPrint(0x20, 0x40, 0xFFFFFF,
                         retry == 0 ? (const char *)D_002BD030
                         : status <= 0 ? D_002BD108[status + 3]
                         : (status & disc_mask) ? (const char *)D_002BD0F0
                         : (const char *)D_002BD060);
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

    if (gameWork.state == GAME_STATE_LOAD_SAVE) {
        return;
    }
    if (xglFlagsGet8(STORY_PROGRESS_FLAG) < 0x4C) {
        GameDiskChange(0x11);
    } else {
        GameDiskChange(0x12);
    }
    xglRenderClearDepth();
    GameCopyFrame();
    xglCdReadFile(D_00688678, SYNOPSIS_ARCHIVE, 0, 1);

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

    if (gameWork.state == MAP_ID_NEW_GAME) {
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
        JpegDecodeRequest jpeg;

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
        D_002BD118[16] = chapter / 10 + '0';
        D_002BD118[17] = chapter % 10 + '0';
        memset(&jpeg, 0, sizeof(jpeg));
        GameSynopsisPanels.framebuffer = (void *)SYNOPSIS_FRAME_BUFFER;
        jpeg.destination = SYNOPSIS_FRAME_BUFFER;
        jpeg.source = SYNOPSIS_IMAGE_BUFFER;
        xglCdReadFile(D_002BD118, (void *)SYNOPSIS_IMAGE_BUFFER, 0, 1);
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
int func_001E95C8(void)
{
    int alpha;

    alpha = 0;
    if (gameWork.state == GAME_STATE_LOAD_SAVE) {
        return 0;
    }
    if (gameWork.state == MAP_ID_NEW_GAME) {
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
        if ((PadData.heldAndPressed & PAD_SOFT_RESET) == PAD_SOFT_RESET) {
            gameWork.control |= CONTROL_SOFT_RESET;
            return 1;
        }
        if (!(PadData.pressed & PAD_CIRCLE)) {
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
