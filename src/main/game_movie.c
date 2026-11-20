#include "common.h"
#include "gs.h"
#include "shared.h"
#include "main/pad.h"
#include "main/xgl_mc_request.h"
#include "main/movie_player.h"
#include "main/se_bank_load.h"
#include "main/game_camera.h"
#include "main/xgl_font.h"
#include "main/xgl_timer0.h"
#include "main/game_data.h"
#include "main/xgl_cd.h"
#include "main/xgl_render.h"
#include "main/xgl_packet.h"
#include "main/xgl_thread.h"
#include "main/game_movie.h"
#include "main/fade_ctrl.h"
#include "main/game_pause_disp.h"
#include "main/xgl_studio.h"
#include "main/xgl_cd_arena.h"
#include "main/res_get_path.h"

char *caption_nextline(char *cursor);
char *caption_getparam(char *cursor, int *time, char *out);

/* A one-frame fade-out (fadeStartOut(1)), used when the movie is skipped (TRIANGLE). */
void GameMovieCaptionFadeOut(void) {
    fadeStartOut(1);
}

char *caption_nextline(char *cursor)
{
    char character;

    do {
        if (*cursor != '\r' && *cursor != '\n') {
            if (*cursor != ';') {
                break;
            }
            cursor++;
        }
        for (;;) {
            character = *cursor++;
            if (character == '\r' || character == '\n') {
                if (*cursor != '\r' && *cursor != '\n' && *cursor != ';') {
                    break;
                }
            }
        }
    } while (0);
    return cursor;
}

/* Reads one caption entry: a time stamp (minutes:seconds:frames, or minutes and seconds only; a leading
 * '+' makes it relative to the previous *time) followed by blanks and the text up to a CR, LF or
 * backslash. The text is written to out with a two byte header (a tag byte of 1 and the complement of
 * half the text width); a backslash starts a further line in the same entry, '@' starts a line with a
 * space and "[base/ruby]" expands to the ruby control strings. *time receives the time stamp in frames,
 * or -1 when the line has none. Returns the start of the next entry. */
char *caption_getparam(char *cursor, int *time, char *out)
{
    int frames;
    int colons;
    int relative;
    int value;
    int scaled;
    int width;
    char ch;
    char *text;

    if ((*cursor >= '0' && *cursor <= '9') || *cursor == '+') {
        colons = 0;
        relative = 0;
        frames = 0;
        if (*cursor == '+') {
            cursor++;
            relative = 1;
        }
        for (;;) {
            value = 0;
            while (*cursor >= '0' && *cursor <= '9') {
                value = value * 10 + *cursor++ - '0';
            }
            frames += value;
            if (*cursor == ':') {
                colons++;
                scaled = frames * 15;
                frames = scaled * 2;
                if (colons == 1) {
                    frames = scaled * 4;
                }
                cursor++;
            }
            if (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
                break;
            }
        }
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (relative) {
            frames += *time;
        }
        for (;;) {
            out += 2;
            text = out;
            if (*cursor == '@') {
                *out++ = ' ';
                cursor++;
            }
            ch = *cursor++;
            while (ch != '\\' && ch != '\r' && ch != '\n') {
                if (ch == '[') {
                    /* The TU builds with -fno-builtin: the original copies the three
                     * ruby tags inline (ldl/ldr, sdl/sdr), which only the explicit
                     * builtin produces. */
                    __builtin_strcpy(out, "$rubyb;");
                    while (*out != 0) {
                        out++;
                    }
                    ch = *cursor++;
                    while (ch != '/') {
                        *out++ = ch;
                        ch = *cursor++;
                    }
                    __builtin_strcpy(out, "$rubys;");
                    while (*out != 0) {
                        out++;
                    }
                    ch = *cursor++;
                    while (ch != ']') {
                        *out++ = ch;
                        ch = *cursor++;
                    }
                    __builtin_strcpy(out, "$rubye;");
                    while (*out != 0) {
                        out++;
                    }
                } else {
                    *out++ = ch;
                }
                ch = *cursor++;
            }
            *out = 0;
            width = xglFontGetStringWidth(text);
            text[-2] = 1;
            text[-1] = ~(width / 2);
            if (ch != '\\') {
                break;
            }
            *out++ = '\n';
            cursor++;
        }
    } else {
        frames = -1;
    }

    *time = frames;
    *out = 0;
    return caption_nextline(cursor);
}

/* Small data of the caption player: the file name suffix, the movie directory and the two string pointers
 * GameMpeg2Play builds the paths from. */
char movieFileSuffix[8] = ".sfd";
char *movieFileSuffixPointer = movieFileSuffix;
char movieDirectoryPath[8] = "movie/";
char *movieDirectoryPathPointer = movieDirectoryPath;
const char gameFontStyleControl[8] = "\013\r\003\031\003";

extern const char movieCaptionControl[];

/* Lays the caption script out in place: the first line is kept as the header text when its time is 0
 * (otherwise the header is empty), then one (time, text) record per line until a negative time.
 * Returns the size of the converted script. */
int caption_convert(char *text)
{
    char *dst;
    char *src;
    char *next;
    int time;

    dst = text;
    /* The width is not used; the original makes the call all the same. */
    xglFontGetStringWidth(movieCaptionControl);
    src = caption_nextline(text);
    time = 0;
    next = caption_getparam(src, &time, text);
    if (time == 0) {
        while (*dst++ != 0) {
        }
        src = next;
    } else {
        *text = 0;
        dst = text + 1;
    }
    time = 0;
    do {
        src = caption_getparam(src, &time, dst + 2);
        dst[0] = time;
        dst[1] = time >> 8;
        dst += 4;
        while (*dst++ != 0) {
        }
    } while (time >= 0);
    return dst - text;
}

typedef struct XglPacket XglPacket;

/* The two frame-buffer base pages read by the copy packets below and the movie flag of the caption player. */
typedef struct MovieRenderState {
    unsigned char unmodeled_00[0x10];
    /* +0x10, +0x12: DrawImageRenderState (ov02/title.c) calls these width/height; both are frame-buffer
     * pages in BITBLTBUF. */
    unsigned short buffer_base[2];
    unsigned char unmodeled_14[0x84 - 0x14];
    unsigned char movie_active;    /* +0x84: set while GameMpeg2Play runs the player */
} MovieRenderState;

/* GS registers of a local-to-local transfer (BITBLTBUF, TRXPOS, TRXREG, TRXDIR from gs.h), written through the A+D GIF tag. */

/* VIF DIRECTHL code word with its quadword count (the low 16 bits). */
#define VIF_DIRECTHL(qwc) (0x51000000 | (qwc))

/* VIF DIRECT header, A+D GIF tag and the five registers of a local-to-local GS copy. */
typedef struct MovieCopyPacket {
    u64 header_padding;
    u64 vif_direct;
    u64 gif_tag;
    u64 gif_registers;
    GsAdPair bitbltbuf;
    GsAdPair trxpos;
    GsAdPair trxreg;
    GsAdPair trxdir;
    GsAdPair texflush;
} MovieCopyPacket;

/* EE scratchpad RAM. */
#define SCRATCHPAD_BASE 0x70000000
#define MOVIE_COPY_PACKET ((MovieCopyPacket *)SCRATCHPAD_BASE)
#define MOVIE_COPY_QWORDS 7

extern MovieRenderState sRender;

/* GameMovieFrameCopy1To0 and GameMovieFrameCopy0To1 queue the same 512x448 GS local-to-local copy between the two frame-buffer
 * pages at sRender +0x10 / +0x12 (BITBLTBUF: SBP in bits 0-13, DBP in bits 32-45). GameMovieFrameCopy1To0 copies
 * page buffer_base[1] to buffer_base[0], GameMovieFrameCopy0To1 copies buffer_base[0] to buffer_base[1]. */
void GameMovieFrameCopy1To0(void)
{
    XglPacket *packet;
    MovieCopyPacket *copy;

    packet = xglPacketGetCurrent();
    copy = MOVIE_COPY_PACKET;
    copy->header_padding = 0;
    copy->vif_direct = VIF_DIRECTHL(6);
    copy->gif_tag = 0x1000000000008005ULL;
    copy->gif_registers = 0xE;
    copy->bitbltbuf.data = ((u64)(sRender.buffer_base[1] << 5) | 0x80000) | ((u64)sRender.buffer_base[0] << 37) |
                           ((u64)0x8000 << 36);
    copy->bitbltbuf.reg = GS_BITBLTBUF;
    copy->trxpos.data = 0;
    copy->trxpos.reg = GS_TRXPOS;
    copy->trxreg.data = ((u64)448 << 32) | 512;
    copy->trxreg.reg = GS_TRXREG;
    copy->trxdir.data = 2;
    copy->trxdir.reg = GS_TRXDIR;
    copy->texflush.data = 0;
    copy->texflush.reg = GS_TEXFLUSH;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, MOVIE_COPY_PACKET, MOVIE_COPY_QWORDS);
    sceVif1PkCloseDirectHLCode(packet);
}

void GameMovieFrameCopy0To1(void)
{
    XglPacket *packet;
    MovieCopyPacket *copy;

    packet = xglPacketGetCurrent();
    copy = MOVIE_COPY_PACKET;
    copy->header_padding = 0;
    copy->vif_direct = VIF_DIRECTHL(6);
    copy->gif_tag = 0x1000000000008005ULL;
    copy->gif_registers = 0xE;
    copy->bitbltbuf.data = ((u64)(sRender.buffer_base[0] << 5) | 0x80000) | ((u64)sRender.buffer_base[1] << 37) |
                           ((u64)0x8000 << 36);
    copy->bitbltbuf.reg = GS_BITBLTBUF;
    copy->trxpos.data = 0;
    copy->trxpos.reg = GS_TRXPOS;
    copy->trxreg.data = ((u64)448 << 32) | 512;
    copy->trxreg.reg = GS_TRXREG;
    copy->trxdir.data = 2;
    copy->trxdir.reg = GS_TRXDIR;
    copy->texflush.data = 0;
    copy->texflush.reg = GS_TEXFLUSH;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, MOVIE_COPY_PACKET, MOVIE_COPY_QWORDS);
    sceVif1PkCloseDirectHLCode(packet);
}

#define MOVIE_SE_PAUSE 0x2000011

extern char *movieFileSuffixPointer; /* ".sfd" */
extern char *movieDirectoryPathPointer; /* "movie/" */
extern const char movieCaptionPositionFormat[]; /* "$ay%d;$xx0,22;" */
extern const char movieSeekErrorText[]; /* the "seek error" message with its replay/skip hints */

int sprintf(char *destination, const char *format, ...);

/* Plays the movie name (file name.sfd) with its caption script movie/<language>/name.txt: the script is read
 * with xglCdReadFile into the render buffer, laid out by caption_convert and printed with xglFontPrintDirect
 * while the player runs; START pauses, the soft-reset combination (PAD_SOFT_RESET_HELD) sets CONTROL_SOFT_RESET
 * and ends it, a seek error offers replay or skip. Called by the mpeg2 playback. */
void GameMpeg2Play(const char *name)
{
    char path[0x40];
    MoviePlayerRecord player;
    char position_control[0x20];
    int replay;
    char *buffer;
    char *text;
    const char *src;
    char *cursor;
    char *dst;
    unsigned char *entry;
    const char *line;
    int status;
    int size;
    int time;
    int lines;
    int y;
    int pass;
    int skip_hidden;

    fadeClear();
    skip_hidden = 0;
    GameMovieFrameCopy0To1();
    soundRestart();
    buffer = GameResourceWorkAlloc(movieRenderBufferSize(SaveData.dolby_digital));
    if (gameWork.control & CONTROL_SOFT_RESET) {
        return;
    }
    while (xglCdServiceIfPending() != 0) {
    }
    xglFontLoad(0, CD_NOTIFY_CURRENT);
    src = name;
    cursor = path;
    if (*src == 'E') {
        skip_hidden = src[1] == 'P';
    }
    while ((*cursor = *src) != 0) {
        src++;
        cursor++;
    }
    src = movieFileSuffixPointer;
    while ((*cursor = *src) != 0) {
        src++;
        cursor++;
    }
    xglRenderClearDepth();
    do {
        dst = buffer;
        replay = 0;
        cursor = movieDirectoryPathPointer;
        while (*cursor != 0) {
            *dst++ = *cursor++;
        }
        xglGetLanguageCode(dst);
        dst[2] = '/';
        dst += 3;
        for (cursor = path; *cursor != 0; *dst++ = *cursor++) {
            if (*cursor == '.') {
                dst[0] = '.';
                dst[1] = 't';
                dst[2] = 'x';
                dst[3] = 't';
                dst[4] = 0;
                break;
            }
        }
        size = xglCdReadFile(buffer, buffer, 0, CD_NOTIFY_CURRENT);
        if (size < 0) {
            size = 0;
        } else {
            size = caption_convert(buffer);
        }
        text = 0;
        if (size != 0) {
            text = buffer;
        }
        movieRenderBufferAlloc(&player, (int)(buffer + ((size + 63) & ~63)), SaveData.dolby_digital);
        if (moviePlayerStartFile(&player, path) >= 0) {
            sRender.movie_active = 1;
            xglRenderVSyncInterval = StudioEntrySkip = 1;
            gameWork.inputDelay = 32;
            for (;;) {
                status = moviePlayerUpdate(&player);
                if (text != 0) {
                    entry = (unsigned char *)text;
                    while (*entry++ != 0) {
                    }
                    time = entry[0] + (entry[1] << 8);
                    if (time >= 0 && player.frame_stamp >= time) {
                        text = (char *)(entry + 2);
                    }
                    xglFontPrint(0, 317, 0xFFFFFF, movieCaptionControl);
                    y = 370;
                    lines = 1;
                    line = text;
                    while (*line != 0) {
                        if (*line == '\n') {
                            lines++;
                        }
                        line = xglFontCopyTextUnit(0, line, 0);
                    }
                    if (lines >= 3) {
                        y -= (lines - 2) * 22;
                    }
                    sprintf(position_control, movieCaptionPositionFormat, y);
                    xglFontPrintDirect(position_control);
                    if (SaveData.caption_all != 0 || text[2] == ' ') {
                        xglFontPrintDirect(text);
                    }
                }
                if (status < 0) {
                    GameMovieFrameCopy1To0();
                    for (pass = 0; pass < 2; pass++) {
                        xglFontPrint(96, 200, 0xFFFFFF, movieSeekErrorText);
                        GamePauseDispBG();
                        xglSleep();
                    }
                    xglClockSampleDayTime();
                    for (;;) {
                        if (PadData[0].pressed & PAD_TRIANGLE) {
                            GameMovieCaptionFadeOut();
                            break;
                        }
                        if (PadData[0].pressed & PAD_CIRCLE) {
                            replay = 1;
                            break;
                        }
                        xglSleep();
                    }
                    seVoiceStartDefault(MOVIE_SE_PAUSE);
                    xglClockAccumulate();
                    break;
                }
                if (status > 0) {
                    break;
                }
                if (player.frame_stamp >= 0) {
                    if (gameWork.inputDelay != 0) {
                        gameWork.inputDelay--;
                    } else if (PadData[0].pressed & PAD_START) {
                        moviePlayerPause(&player, 1);
                        xglClockSampleDayTime();
                        gameWork.inputDelay = 32;
                        if ((PadData[0].held & PAD_SOFT_RESET_HELD) == PAD_SOFT_RESET_HELD) {
                            gameWork.control |= CONTROL_SOFT_RESET;
                            break;
                        }
                        seVoiceStartDefault(MOVIE_SE_PAUSE);
                        xglSleep();
                        GameMovieFrameCopy1To0();
                        GamePauseDispEvent(skip_hidden);
                        xglSleep();
                        GamePauseDispEvent(skip_hidden);
                        xglSleep();
                        for (;;) {
                            if (gameWork.inputDelay != 0) {
                                gameWork.inputDelay--;
                            } else if (skip_hidden == 0 && (PadData[0].pressed & PAD_TRIANGLE)) {
                                GameMovieCaptionFadeOut();
                                break;
                            } else if (PadData[0].pressed & PAD_START) {
                                break;
                            }
                            xglSleep();
                        }
                        /* After the pause the original tests the combination with START pressed
                         * (heldAndPressed, 0x0800010F), not the held mask tested above. */
                        if ((PadData[0].heldAndPressed & PAD_SOFT_RESET) == PAD_SOFT_RESET) {
                            gameWork.control |= CONTROL_SOFT_RESET;
                            break;
                        }
                        seVoiceStartDefault(MOVIE_SE_PAUSE);
                        gameWork.inputDelay = 32;
                        if (skip_hidden == 0 && (PadData[0].pressed & PAD_TRIANGLE)) {
                            gameWork.inputDelay = 64;
                            break;
                        }
                        xglClockAccumulate();
                        moviePlayerPause(&player, 0);
                    }
                }
                xglSleep();
            }
            StudioEntrySkip = 0;
            xglRenderVSyncInterval = 2;
            sRender.movie_active = 0;
        }
        moviePlayerDestroy(&player);
    } while (replay);
    /* The maps on which the movie skips its closing fadeStartOut and keeps the screen as it is; every
     * other map fades out. */
    if (!(gameWork.mapId == 0x1B6C || gameWork.mapId == 0x1B6D || gameWork.mapId == 0x1B6E ||
          gameWork.mapId == 0x1B6F || gameWork.mapId == 0x1B71 || gameWork.mapId == 0x1B8D ||
          gameWork.mapId == 0x1B8E || gameWork.mapId == 0x1B8F || gameWork.mapId == 0x1B60 ||
          gameWork.mapId == 0x1B70 || gameWork.mapId == 0x1B72 || gameWork.mapId == 0x1B81 ||
          gameWork.mapId == 0x1B89 || gameWork.mapId == 0x1B90 ||
          /* the GameEnd map (game_over_059 MAP_ID_GAME_END, pt_member.c MAP_ID_SPLIT_FLAG_MAP) */
          gameWork.mapId == 0x1B91)) {
        fadeStartOut(1);
    }
    GameMovieFrameCopy1To0();
    xglSleep();
    xglSleep();
    xglFontLoad(1, CD_NOTIFY_CURRENT);
    GameResArenaRelease();
    fadeClear();
}

const char movieCaptionControl[48] = "$bg3;$prop1;$han1;$xx0,24;$xx1,8;$xx2,0;";
const char movieCaptionPositionFormat[16] = "$ay%d;$xx0,22;";
/* JP: A seek error has occured.\n\n\n○button: Replay\n\n△button: Skip
 * EN: A seek error has occured.\n\n\n(O) button: Replay\n\n(Triangle) button: Skip */
const char movieSeekErrorText[88] = "\013\025\002\001\020\r\003\031\001A seek error has occured.\n\n\n\014\200  \241\373\014\200\200\200button: Replay\n\n\014 \200 \242\244\014\200\200\200button: Skip";
