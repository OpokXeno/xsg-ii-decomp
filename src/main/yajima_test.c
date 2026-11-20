#include "common.h"
#include "main/game_camera.h"
#include "main/xgl_mc.h"
#include "shared.h"
#include "main/pad_buttons.h"
#include "main/xgl_mc_request.h"
#include "main/xgl_hdd.h"
#include "main/xgl_font.h"
#include "main/game_snap_shot.h"
#include "main/xgl_thread.h"
#include "main/se_bank_load.h"
#include "main/sound_driver.h"
#include "main/xgl_cd.h"
#include "main/xgl_render.h"
#include "main/xgl_2.h"
#include "main/xgl_1.h"
#include "main/res_get_path.h"
#include "main/game_data.h"
#include "main/yajima_test.h"
#include "main/game_movie.h"
#include "main/xgl_packet.h"
#include "main/elevator.h"
#include "main/tya_draw_gauge.h"
#include "main/xgl_menu.h"
#include "main/tya_display_setting.h"

/* One port of the pad record PadData (pad.h's PadRecord), the full 0x80 bytes: the raw pad buffer, the
 * decoded buttons and sticks and the actuator settings. This file keeps a union-free view of the record
 * and includes only the button bits of pad.h (pad_buttons.h), because PadRecord's union at +0x28
 * (held/pressed/repeated/released with heldAndPressed) gives every access to those halfwords alias set 0
 * in gcc 2.96: the load of repeated in DebugTextViewUpdate and of pressed in EdgeTest then stays behind the
 * stores that the original schedules it above. The members both records name carry pad.h's names at the
 * same offsets. */
typedef struct McTestPadPort {
    unsigned char raw[0x20];
    unsigned char status;
    unsigned char id;
    unsigned short buttons;
    unsigned char unmodeled_24[0x26 - 0x24];
    unsigned char rawButtons[2];
    unsigned short held;     /* +0x28 */
    unsigned short pressed;  /* +0x2A */
    unsigned short repeated; /* +0x2C */
    unsigned short released; /* +0x2E */
    unsigned short directionHeld;
    unsigned short directionPressed;  /* +0x32, as pad.h */
    unsigned short directionRepeated; /* +0x34 */
    unsigned short directionReleased;
    signed char sticks[4];
    short horizontalPressure;
    short verticalPressure; /* +0x3C/+0x3E as pad.h */
    unsigned char pressures[8];
    unsigned char unmodeled_48[0x4E - 0x48];
    unsigned char horizontalDeadZone; /* +0x4E, as pad.h */
    unsigned char verticalDeadZone;   /* +0x4F */
    unsigned char actuators[2];
    unsigned char unmodeled_52[0x56 - 0x52];
    unsigned char actuatorMode;
    unsigned char unmodeled_57[0x64 - 0x57];
    signed char sticksCentered[4];
    unsigned char unmodeled_68[0x80 - 0x68];
} McTestPadPort;

/* PadData: the two port records. The exit test of every page reads port
 * 1's held and pressed halfwords together as one doubleword
 * (PadRecord.heldAndPressed in pad.h). The doubleword overlays the port
 * records here instead of sharing a union inside the port as in pad.h. */
typedef union McTestPad {
    McTestPadPort ports[2];
    struct {
        unsigned char unmodeled_00[0x28];
        unsigned long long heldAndPressed; /* +0x28 */
    } port1;
} McTestPad;

extern McTestPad PadData;

/* PadRecord.heldAndPressed: SELECT held with START pressed (pressed is the
 * upper halfword), the exit combination of every test page. */
#define PAD_EXIT_COMBO (PAD_SELECT | (PAD_START << 16))

/* A scrolling view of a CR/LF separated text: the text, the first line
 * shown and the selected line, with the screen position and height. */
typedef struct DebugTextView {
    char *text;
    char *top;
    char *cursor;
    unsigned char scrolls;
    unsigned char y;
    short height;
} DebugTextView;


/* Moves line back to the start of the previous line of text, never before
 * start. */
char *DebugTextPrevLine(char *line, char *start) {
    char c;

    if (start < line) {
        line--;
        while (*line == '\r' || *line == '\n') {
            line--;
        }
        while (start < line) {
            c = *line;
            if (c == '\r' || c == '\n') {
                return line + 1;
            }
            line--;
        }
    }
    return start;
}

/* Returns the start of the line after line, or line itself when no text
 * follows the line breaks. */
char *DebugTextNextLine(char *line) {
    char *next = line;
    char c;

    while (*next != '\r' && *next != '\n') {
        next++;
    }
    while (*next == '\r' || *next == '\n') {
        next++;
    }
    c = *next;
    if (c == 0) {
        return line;
    }
    return next;
}

void DebugTextViewInit(DebugTextView *view, char *text) {
    view->cursor = text;
    view->top = text;
    view->text = text;
    view->scrolls = 1;
    view->y = 16;
    view->height = 200;
}

/* "^" marks text above the first line shown. */
extern const char menuUpArrowText[];
/* ">" marks the selected line. */
extern char menuCursorText[];
/* "v" marks text below the last line shown. */

const char menuUpArrowText[8] = "^";

char menuCursorText[8] = ">";

/* Scrolls the view by the pad (UP/DOWN move the selection, RIGHT/LEFT move
 * twenty lines), then draws the lines from view->top. Returns whether CIRCLE is
 * pressed: MovieListTest plays the selected movie then. */
int DebugTextViewUpdate(DebugTextView *view) {
    char line[0x40];
    char *top;
    char *p;
    int y;
    int i;
    int circle;
    int circleMask = PAD_CIRCLE;
    McTestPadPort *pad = &PadData.ports[0];

    if (pad->repeated & PAD_UP) {
        view->cursor = DebugTextPrevLine(view->cursor, view->text);
    }
    if (pad->repeated & PAD_DOWN) {
        view->cursor = DebugTextNextLine(view->cursor);
    }
    if (view->scrolls & 1) {
        if (PadData.ports[0].pressed & PAD_RIGHT) {
            for (i = 19; i >= 0; i--) {
                view->cursor = DebugTextNextLine(view->cursor);
            }
        }
        if (PadData.ports[0].pressed & PAD_LEFT) {
            for (i = 19; i >= 0; i--) {
                view->cursor = DebugTextPrevLine(view->cursor, view->text);
            }
        }
    }
    y = view->y;
    top = view->top;
    circle = (PadData.ports[0].pressed & circleMask) != 0;
    if (view->text < top) {
        if (top >= view->cursor) {
            view->top = DebugTextPrevLine(top, view->text);
        }
        if (view->text < view->top) {
            xglFontDebugPrintf(16, y, menuUpArrowText);
        }
    }
    while (y < view->height && *top != 0) {
        if (view->cursor == top) {
            xglFontDebugPrintf(24, y, menuCursorText);
        }
        p = line;
        for (;;) {
            switch (*top) {
            case '\r':
                break;
            default:
                if (*top == '\n') {
                    break;
                }
                *p++ = *top++;
                continue;
            }
            break;
        }
        do {
            top++;
        } while (*top == '\r' || *top == '\n');
        *p = 0;
        xglFontDebugPrintf(32, y, line);
        y += 8;
    }
    if (*top != 0) {
        if (view->cursor >= DebugTextPrevLine(top, view->text)) {
            view->top = DebugTextNextLine(view->top);
        }
        xglFontDebugPrintf(16, y - 8, "v");
    }
    return circle;
}


/* Pad test page: the right/left and down/up repeats set the small and big
 * actuator levels (applied while circle is held), cross toggles the actuator
 * mode; both port records are dumped in hex until SELECT+START. */
void PadTest(int selection) {
    int row;
    int actuatorMode = 0;
    int port;
    int big = 0;
    int small = 0;
    int x;
    McTestPadPort *pad;

    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        if (PadData.ports[0].repeated & PAD_RIGHT) {
            small++;
        }
        if (PadData.ports[0].repeated & PAD_DOWN) {
            big++;
        }
        if (PadData.ports[0].repeated & PAD_LEFT) {
            small--;
        }
        if (PadData.ports[0].repeated & PAD_UP) {
            big--;
        }
        if (PadData.ports[0].held & PAD_CIRCLE) {
            PadData.ports[0].actuators[0] = small;
            PadData.ports[0].actuators[1] = big;
        }
        if (PadData.ports[0].pressed & PAD_CROSS) {
            actuatorMode ^= 1;
            PadData.ports[0].actuatorMode = actuatorMode;
        }
        xglFontDebugPrintf(0, 0, "%2x %2x %1d", small, big, actuatorMode);
        x = 16;
        pad = PadData.ports;
        for (port = 0; port < 2; port++) {
            xglFontDebugHex(x, 0x18, pad->status, 2);
            xglFontDebugHex(x + 16, 0x18, pad->id, 2);
            xglFontDebugHex(x, 0x20, pad->buttons, 4);
            xglFontDebugHex(x, 0x28, pad->rawButtons[0], 2);
            xglFontDebugHex(x + 16, 0x28, pad->rawButtons[1], 2);
            xglFontDebugHex(x, 0x38, pad->held, 4);
            xglFontDebugHex(x, 0x40, pad->pressed, 4);
            xglFontDebugHex(x, 0x48, pad->repeated, 4);
            xglFontDebugHex(x, 0x50, pad->released, 4);
            xglFontDebugHex(x, 0x60, pad->directionHeld, 4);
            xglFontDebugHex(x, 0x68, pad->directionPressed, 4);
            xglFontDebugHex(x, 0x70, pad->directionRepeated, 4);
            xglFontDebugHex(x, 0x78, pad->directionReleased, 4);
            xglFontDebugHex(x, 0x88, pad->sticks[0], 2);
            xglFontDebugHex(x + 16, 0x88, pad->sticksCentered[0], 2);
            xglFontDebugHex(x, 0x90, pad->sticks[1], 2);
            xglFontDebugHex(x + 16, 0x90, pad->sticksCentered[1], 2);
            xglFontDebugHex(x, 0x98, pad->sticks[2], 2);
            xglFontDebugHex(x + 16, 0x98, pad->sticksCentered[2], 2);
            xglFontDebugHex(x, 0xA0, pad->sticks[3], 2);
            xglFontDebugHex(x + 16, 0xA0, pad->sticksCentered[3], 2);
            xglFontDebugHex(x, 0xA8, pad->horizontalPressure, 4);
            xglFontDebugHex(x, 0xB0, pad->verticalPressure, 4);
            xglFontDebugHex(x, 0xB8, pad->pressures[0], 2);
            xglFontDebugHex(x + 16, 0xB8, pad->pressures[4], 2);
            xglFontDebugHex(x, 0xC0, pad->pressures[1], 2);
            xglFontDebugHex(x + 16, 0xC0, pad->pressures[5], 2);
            xglFontDebugHex(x, 0xC8, pad->pressures[2], 2);
            xglFontDebugHex(x + 16, 0xC8, pad->pressures[6], 2);
            xglFontDebugHex(x, 0xD0, pad->pressures[3], 2);
            xglFontDebugHex(x + 16, 0xD0, pad->pressures[7], 2);
            for (row = 0; row < 16; row++) {
                xglFontDebugHex(x + 0x28, 0x18 + row * 8, pad->raw[row], 2);
                xglFontDebugHex(x + 0x38, 0x18 + row * 8, pad->raw[row + 0x10], 2);
            }
            x += 0x80;
            pad++;
        }
        xglSleep();
    }
}

/* Font test page: the proportional fonts and a few kanji rows; circle and
 * cross reload font 0 and 1. */
/* The half-width katakana table "$prop1;%s" prints. */
extern char fontTestGlyphText[];

void FontTestP1(void) {
    if (PadData.ports[0].pressed & PAD_CIRCLE) {
        xglFontLoad(0, CD_NOTIFY_CURRENT);
    }
    if (PadData.ports[0].pressed & PAD_CROSS) {
        xglFontLoad(1, CD_NOTIFY_CURRENT);
    }
    xglFontDebugPrintf(16, 16, "$prop0;$han1;0123456789abcdef");
    xglFontDebugPrintf(16, 32, "$prop1;%s", fontTestGlyphText);
    /* JP: $prop0;$han1;aぁあぃいぅa:;´商検―×☆★□■△♪【】'
     * EN: Glyph test, proportional off: small and large kana, punctuation, two kanji (trade, inspect), dash and symbol glyphs */
    xglFontDebugPrintf(16, 144, "$prop0;$han1;a\xA4\xA1\xA4\xA2\xA4\xA3\xA4\xA4\xA4\xA5" "a:;\xAD\xA1\xAD\xBE\xA6\xB8\xA1\xFB\xA1\xBD\xA1\xDF\xA1\xF9\xA1\xFA\xA2\xA2\xA2\xA3\xA2\xA4\xA2\xF6\xA1\xDA\xA1\xDB'");
    /* JP: $prop1;$han1;aぁあぃいぅa:;´商検―×☆★□■△♪【】'
     * EN: The same glyph test with proportional spacing on */
    xglFontDebugPrintf(16, 160, "$prop1;$han1;a\xA4\xA1\xA4\xA2\xA4\xA3\xA4\xA4\xA4\xA5" "a:;\xAD\xA1\xAD\xBE\xA6\xB8\xA1\xFB\xA1\xBD\xA1\xDF\xA1\xF9\xA1\xFA\xA2\xA2\xA2\xA3\xA2\xA4\xA2\xF6\xA1\xDA\xA1\xDB'");
    xglFontDebugPrintf(16, 176, "$prop0;$han1;%c", 0xAE);
}

/* Font test: left and right would turn pages, but only page 0 is left. */
void FontTest(int selection) {
    int page = 0;

    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        if (PadData.ports[0].pressed & PAD_RIGHT) {
            ++page;
        }
        if (PadData.ports[0].pressed & PAD_LEFT) {
            --page;
        }
        switch (page) {
        default:
            page = 0;
            break;
        case -1:
            page = 0;
            /* fall through */
        case 0:
            FontTestP1();
            break;
        }
        GameSnapShotCheck();
        xglSleep();
    }
}

/* A sound project file (.prj) read into the debug buffer at 0x01010000. */
typedef struct SoundProjectEntry {
    unsigned char unk00[0xD];
    char mode;           /* 0x0D: 2 in effect banks, 1 in song banks */
    unsigned char unk0E[0x20 - 0xE];
    char name[0x10];     /* 0x20 */
} SoundProjectEntry;

typedef struct SoundProject {
    unsigned char unk00[8];
    int count;           /* 0x08 */
    unsigned char unk0C[4];
    char name[0x10];     /* 0x10 */
    char keyp_header[0x10]; /* 0x20: the KEYp chunk header of the file; the test writes its display lines here */
    SoundProjectEntry entries[1]; /* 0x30 */
} SoundProject;

enum SoundTestState {
    SOUNDTEST_INIT = 0,
    SOUNDTEST_MENU = 1,
    SOUNDTEST_BGM = 0x100,
    SOUNDTEST_SE = 0x200,
    SOUNDTEST_SE_VOICE = 0x201,
    SOUNDTEST_ADX = 0x300,
    SOUNDTEST_VOLUME_INIT = 0x400,
    SOUNDTEST_VOLUME = 0x401
};

int printf(const char *, ...);
void *memset(void *dst, int value, unsigned int size);
int sprintf(char *, const char *, ...);
int sceOpen(const char *path, int flags, ...);
int sceRead(int descriptor, void *buffer, int bytes);
int sceClose(int descriptor);
void korgEvagPauseSeq(int first, int flag);
extern char soundTestModeMenuText[];
extern char soundTestVolumeMenuText[];
extern const char soundTestStereoFormat[];
extern const char stereoText[];
extern const char soundTestLevelFormat[];

/* JP: □:%s
 * EN: (Square): %s */
const char soundTestStereoFormat[8] = "\xA2\xA2:%s";

const char stereoText[8] = "STEREO";

const char soundTestLevelFormat[8] = "%3d";

void SoundTest(int selection)
{
    DebugTextView view;
    DebugTextView voices;
    char bgmName[0x100];
    char seName[0x100];
    char path[0x100];
    int stereo;
    int state;
    int volume;
    int bank;
    int fd;
    int count;
    int i;
    int length;
    int k;
    int voice;
    int pan;
    int level;
    int delta;
    char *file;
    char *from;
    char *to;
    char *dst;
    unsigned char *line;
    char *name;
    char *text;

    seBankLoadDefault(0, 0);
    for (bank = 2; bank < 0x18; bank++) {
        seBankLoadDefault(bank, 0);
    }
    state = SOUNDTEST_INIT;
    volume = 0x7F;
    stereo = 1;
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        /* JP: ○:SELECT △:STOP ×:BACK
         * EN: (O): SELECT (Triangle): STOP (X): BACK */
        xglFontDebugPrintf(0x10, 0, "\xA1\xFB:SELECT \xA2\xA4:STOP \xA1\xDF:BACK");
        switch (state) {
        case SOUNDTEST_INIT:
            DebugTextViewInit(&view, soundTestModeMenuText);
            state = SOUNDTEST_MENU;
        case SOUNDTEST_MENU:
            if (PadData.ports[0].pressed & PAD_SQUARE) {
                stereo ^= 1;
                soundStereoSet(stereo);
            }
            xglFontDebugPrintf(0x10, 0x40, soundTestStereoFormat, stereo ? stereoText : "MONAURAL");
            if (DebugTextViewUpdate(&view)) {
                memset((void *) 0x01000000, 0, 0x10000);
                fd = 0;
                if (*view.cursor == 'S') {
                    fd = sceOpen("host0:/home/xeno/vcd-f/x2/sound/vsf/_list.txt", 1);
                    state = SOUNDTEST_SE;
                } else if (*view.cursor == 'B') {
                    fd = sceOpen("host0:/home/xeno/vcd-f/x2/sound/smf/_list.txt", 1);
                    state = SOUNDTEST_BGM;
                } else if (*view.cursor == 'A') {
                    fd = sceOpen("host0:/home/xeno/vcd-f/x2/sound/adx/_list.txt", 1);
                    state = SOUNDTEST_ADX;
                } else if (*view.cursor == 'V') {
                    state = SOUNDTEST_VOLUME_INIT;
                    break;
                }
                sceRead(fd, (void *) 0x01000000, 0x10000);
                sceClose(fd);
                DebugTextViewInit(&view, (char *) 0x01000000);
            }
            break;
        case SOUNDTEST_BGM:
            if (PadData.ports[0].pressed & PAD_TRIANGLE) {
                seBgmStop();
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = SOUNDTEST_INIT;
                break;
            }
            if (PadData.ports[0].pressed & PAD_SQUARE) {
                seSetMuted(1, 1);
            }
            if (PadData.ports[0].released & PAD_SQUARE) {
                seSetMuted(1, 0);
            }
            if (PadData.ports[0].repeated & (PAD_LEFT | PAD_RIGHT)) {
                if (PadData.ports[0].repeated & PAD_LEFT) {
                    volume--;
                }
                if (PadData.ports[0].repeated & PAD_RIGHT) {
                    volume++;
                }
                if (volume < 0) {
                    volume = 0;
                }
                if (volume > 0x7F) {
                    volume = 0x7F;
                }
                seVolumeSet(volume, 0);
                xglFontDebugPrintf(0, 0x10, soundTestLevelFormat, volume);
            }
            if (PadData.ports[0].pressed & PAD_R1) {
                korgEvagPauseSeq(1, 1);
            }
            if (PadData.ports[0].pressed & PAD_R2) {
                korgEvagPauseSeq(1, 0);
            }
            if (DebugTextViewUpdate(&view)) {
                int fd;
                SoundProject *project;
                unsigned int bgmSize;
                int bgmChan;

                file = bgmName;
                for (to = file, from = view.cursor; *from != '.'; from++, to++) {
                    *to = *from;
                }
                *to = 0;
                bgmSize = 0x19000;
                bgmChan = 0x100;
                seBgmStop();
                printf("smf result:%d\n", seBgmLoad(bgmChan, bgmName, bgmSize));
                printf("seq result:%d\n", seBgmStart(1, 0x100));
                sprintf(seName, "host0:/home/xeno/vcd-f/x2/sound/smf/%s.prj", bgmName);
                printf("==== %s ====\n", bgmName);
                fd = sceOpen(seName, 1);
                sceRead(fd, (void *) 0x01010000, 0x10000);
                sceClose(fd);
                project = (SoundProject *) 0x01010000;
                name = project->name;
                count = project->count;
                dst = (char *) project;
                for (k = 0; k < 16 && *name != 0; k++) {
                    *dst++ = *name++;
                }
                *dst = 0;
                dst = project->keyp_header;
                for (i = 0, name = project->entries[0].name; i < count; i++) {
                    char *text = name;

                    line = dst;
                    for (length = 0; length < 16 && *text != 0; length++) {
                        *dst++ = *text++;
                    }
                    *dst++ = '\n';
                    name += 0x30;
                    if (name[-0x43] != 1) {
                        *dst = 0;
                        printf(line);
                    }
                }
                *dst = 0;
            }
            break;
        case SOUNDTEST_SE:
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = SOUNDTEST_INIT;
                break;
            }
            if (DebugTextViewUpdate(&view)) {
                int fd;
                const char *from;
                char *to;
                int size;
                char *file = seName;

                for (to = file, from = view.cursor; *from != '.';) {
                    *to++ = *from++;
                }
                *to = 0;
                size = 0x19000;
                printf("result:%d\n", seBankLoad(0, seName, size));
                sprintf(path, "host0:/home/xeno/vcd-f/x2/sound/vsf/%s.prj", seName);
                printf("==== %s ====\n", seName);
                fd = sceOpen(path, 1);
                sceRead(fd, (void *) 0x01010000, 0x10000);
                sceClose(fd);
                name = ((SoundProject *) 0x01010000)->name;
                count = ((SoundProject *) 0x01010000)->count;
                dst = (char *) 0x01010000;
                for (k = 0; k < 16 && *name != 0; k++) {
                    *dst++ = *name++;
                }
                *dst = 0;
                dst = ((SoundProject *) 0x01010000)->keyp_header;
                name = ((SoundProject *) 0x01010000)->entries[0].name;
                for (i = 0; i < count; i++) {
                    char *text = name;

                    line = dst;
                    for (length = 0; length < 16 && *text != 0; length++) {
                        *dst++ = *text++;
                    }
                    name += 0x30;
                    *dst++ = '\n';
                    if (1 == name[-0x43]) {
                        *dst = 0;
                        printf(line);
                    }
                }
                *dst = 0;
                DebugTextViewInit(&voices, ((SoundProject *) 0x01010000)->keyp_header);
                state = SOUNDTEST_SE_VOICE;
            }
            break;
        case SOUNDTEST_SE_VOICE:
            voice = 0;
            for (text = voices.text; text < voices.cursor; text++) {
                if (*text == '\n') {
                    voice++;
                }
            }
            if (PadData.ports[0].pressed & PAD_TRIANGLE) {
                seVoiceStop(voice);
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                soundDriverReset();
                state = SOUNDTEST_SE;
                break;
            }
            if (PadData.ports[0].held & PAD_R3) {
                pan = PadData.ports[0].sticksCentered[0] / 2 + 0x40;
                level = 0x7F - PadData.ports[0].sticksCentered[1];
                if (level < 0) {
                    level = 0;
                }
                if (level > 0x7F) {
                    level = 0x7F;
                }
                xglFontDebugPrintf(0, 0x20, soundTestLevelFormat, pan);
                xglFontDebugPrintf(0, 0x28, soundTestLevelFormat, level);
                seVoiceSetPanVolume(voice, level, pan);
            }
            if (DebugTextViewUpdate(&voices)) {
                seVoiceStartDefault(voice);
            }
            xglFontDebugPrintf(0, 0x10, "%2d", voice);
            break;
        case SOUNDTEST_ADX:
            if (PadData.ports[0].pressed & PAD_TRIANGLE) {
                soundSlotStop(0);
            }
            if (PadData.ports[0].pressed & PAD_SQUARE) {
                soundSlotSetVolume(0, 0, 8);
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                soundSlotStop(0);
                state = SOUNDTEST_INIT;
                break;
            }
            if (DebugTextViewUpdate(&view)) {
                char *file = path;
                const char *from;
                char *to;

                for (from = view.cursor, to = file; *from != '.'; from++, to++) {
                    *to = *from;
                }
                *to = 0;
                soundSlotStop(0);
                adxSetSlotFile(0, path);
            }
            break;
        case SOUNDTEST_VOLUME_INIT:
            DebugTextViewInit(&view, soundTestVolumeMenuText);
            state = SOUNDTEST_VOLUME;
            view.scrolls = 0;
        case SOUNDTEST_VOLUME:
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = SOUNDTEST_INIT;
                break;
            }
            xglFontDebugPrintf(0x58, 0x10, soundTestLevelFormat, soundCtrl.master_volume);
            xglFontDebugPrintf(0x58, 0x18, soundTestLevelFormat, soundCtrl.master_level);
            xglFontDebugPrintf(0x58, 0x20, soundTestLevelFormat, soundCtrl.voice_level);
            xglFontDebugPrintf(0x58, 0x28, soundTestLevelFormat, soundCtrl.slot_level);
            DebugTextViewUpdate(&view);
            delta = 0;
            if (PadData.ports[0].repeated & PAD_LEFT) {
                delta = -1;
            }
            if (PadData.ports[0].repeated & PAD_RIGHT) {
                delta = 1;
            }
            if (*view.cursor == 'M') {
                soundCtrl.master_volume += delta;
            } else if (*view.cursor == 'B') {
                soundCtrl.master_level += delta;
            } else if (*view.cursor == 'S') {
                soundCtrl.voice_level += delta;
            } else if (*view.cursor == 'A') {
                soundCtrl.slot_level += delta;
                soundSlotSetVolume(1, -1, 0);
            }
            break;
        }
        xglSleep();
    }
}

/* Empty callback: nothing to do. */
void HddTest(int selection) {

}

/* Port names: memory card slots 1 and 2, then the HDD. */
extern const char *memcardPortNames[];

/* JP: ポート２
 * EN: Port 2 */
const char port2Label[16] = "\xA5\xDD\xA1\xBC\xA5\xC8\xA3\xB2";

/* JP: ポート１
 * EN: Port 1 */
const char port1Label[16] = "\xA5\xDD\xA1\xBC\xA5\xC8\xA3\xB1";

const char *mt_GetPortName(int port) {
    return memcardPortNames[port];
}


/* Enumerate callback: logs the result it is called with ("callback:%d");
 * always returns -1. */
int DebugCallbackLog(int result) {
    printf("callback:%d\n", result);
    return -1;
}

/* Reports of the old-data check: "ep1  :" and "ep1re:" lines carry the seven
 * bytes of an Episode I save entry, "freak:" its first byte. */

/* Callback of the old-data check (MC_REQ_CHECK_OLD): logs each result; the
 * entry bytes are read only for the entry results 0 to 2. Always returns 0. */
/* JP: ＨＤＤ
 * EN: HDD */
const char hddText[8] = "\xA3\xC8\xA3\xC4\xA3\xC4";

int McOldDataCheckCallback(int result, unsigned char *entry) {
    switch (result) {
    case 0:
        printf("ep1  :%d,%2d,%2d,%2d,%2d,%2d,%2d\n", entry[0], entry[1], entry[2], entry[3], entry[4], entry[5], entry[6]);
        break;
    case 1:
        printf("ep1re:%d,%2d,%2d,%2d,%2d,%2d,%2d\n", entry[0], entry[1], entry[2], entry[3], entry[4], entry[5], entry[6]);
        break;
    case 2:
        printf("freak:%d\n", entry[0]);
        break;
    case -2:
        printf("end\n");
        break;
    case -3:
        printf("error\n");
        break;
    case -1:
        break;
    }
    return 0;
}

/* A memory card icon model: the header counts, then per vertex one
 * position for each animation shape followed by the normal and the
 * texture coordinate/colour words (two more 8-byte entries). */
typedef struct McIconPosition {
    short x;
    short y;
    short z;
    short w;
} McIconPosition;

typedef struct McIconModel {
    int magic;
    int shapeCount;
    int textureType;
    unsigned char unmodeled_0C[0x10 - 0x0C];
    int vertexCount;
    McIconPosition positions[1];
} McIconModel;

/* "  %08x:%d,%d\n" and "  (%f,%f)-(%f,%f)-(%f,%f)\n". */

/* Reports the bounding box of an icon model (positions are 1/1024 fixed
 * point) and the zoom that keeps its height within 20 units. */
void McIconBoundsReport(void *icon, float *zoom) {
    McIconModel *model = icon;
    McIconPosition *position = model->positions;
    int shapeCount = model->shapeCount;
    int vertexCount = model->vertexCount;
    int vertex;
    int shape;
    float value;
    float minX, maxX, minY, maxY, minZ, maxZ;

    printf("  %08x:%d,%d\n", model, shapeCount, vertexCount);
    minX = minY = minZ = 100.0f;
    maxX = maxY = maxZ = -100.0f;
    for (vertex = 0; vertex < vertexCount; vertex++) {
        for (shape = 0; shape < shapeCount; shape++) {
            value = I2F(position->x) * (1.0f / 1024.0f);
            if (value < minX) {
                minX = value;
            }
            if (maxX < value) {
                maxX = value;
            }
            value = I2F(position->y) * (1.0f / 1024.0f);
            if (value < minY) {
                minY = value;
            }
            if (maxY < value) {
                maxY = value;
            }
            value = I2F(position->z) * (1.0f / 1024.0f);
            if (value < minZ) {
                minZ = value;
            }
            if (maxZ < value) {
                maxZ = value;
            }
            position++;
        }
        position += 2;
    }
    printf("  (%f,%f)-(%f,%f)-(%f,%f)\n", minX, maxX, minY, maxY, minZ, maxZ);
    *zoom = 1.0f;
    if (0.0f < maxY) {
        minY -= maxY;
    }
    if (minY < -20.0f) {
        *zoom = -20.0f / minY;
    }
}

/* The header of a card icon image: the size of the header and the offsets
 * of the icon.sys, copy and delete icons, each rounded up to 64 bytes. */
typedef struct McIconImageHeader {
    int headerSize;
    int sysOffset;
    int copyOffset;
    int deleteOffset;
} McIconImageHeader;

#define MC_ICON_ALIGN(size) (((size) + 0x3F) & -0x40)


/* Loads a card icon image and reports its three icons. The smallest zoom
 * is tracked but never shown: the delete icon's zoom is what gets printed. */
void McFileSectionsReport(const char *name, void *image) {
    McIconImageHeader *header = image;
    char *icons;
    int offset;
    float iconZoom;
    float minZoom = 1.0f;

    printf("%s\n", name);
    xglCdReadFile(name, image, 0, CD_NOTIFY_CURRENT);
    offset = MC_ICON_ALIGN(header->headerSize);
    icons = (char *)image + offset;
    printf(" sys\n");
    offset = MC_ICON_ALIGN(header->sysOffset);
    McIconBoundsReport(icons + offset, &iconZoom);
    if (iconZoom < minZoom) {
        minZoom = iconZoom;
    }
    printf(" copy\n");
    offset = MC_ICON_ALIGN(header->copyOffset);
    McIconBoundsReport(icons + offset, &iconZoom);
    if (iconZoom < minZoom) {
        minZoom = iconZoom;
    }
    printf(" delete\n");
    offset = MC_ICON_ALIGN(header->deleteOffset);
    McIconBoundsReport(icons + offset, &iconZoom);
    if (iconZoom < minZoom) {
        minZoom = iconZoom;
    }
    printf(" zoom:%f\n", iconZoom);
}

/* Free EE RAM the tests use as scratch. */
#define DEBUG_BUFFER_BASE 0x01000000
#define MC_TEST_FILE_BASE 0x01002000
#define MC_TEST_FILE_STRIDE 0x50000
#define MC_TEST_FILE_COUNT 11
#define MC_TEST_PATTERN_BASE 0x01100000
#define IPU_STREAM_BUFFER_BASE 0x02000000
/* Requests of the xglMc queue as MemcardTest issues them. */
#define MC_REQ_FREE_SPACE 1
#define MC_REQ_EXIST 2
#define MC_REQ_LIST 3
#define MC_REQ_LOAD 4
#define MC_REQ_SAVE 5
#define MC_REQ_CREATE 6
#define MC_REQ_FORMAT 7
#define MC_REQ_CHECK_OLD 8
#define MC_REQ_UNFORMAT 9
/* Status halfwords returned by xglMcMain for each port. */
#define MC_PORT_READY 1
#define MC_PORT_BUSY 0xFF
#define MC_PORT_HDD 2
#define MC_PORT_COUNT 3
#define MC_SAVE_FILE_COUNT 100
#define MC_MENU_ITEM_COUNT 7
#define MC_HDD_MENU_ITEM_COUNT 2
#define MC_CREATE_PROGRESS_STEPS 20
#define MC_DUMMY_LIMIT_KB 1000
#define MC_EASY_SAVE_DUMMY_SIZE 0x77D800
#define MC_ICON_SYS_SIZE 964
#define SAVE_FLAGS_COUNT 128

/* MemcardTest state machine: the high nibble selects the screen family. */
enum MemcardTestState {
    MCTEST_STATE_INIT = 0x00,
    MCTEST_STATE_SELECT_PORT = 0x01,
    MCTEST_STATE_MENU_INIT = 0x10,
    MCTEST_STATE_MENU = 0x11,
    MCTEST_STATE_LOAD_LIST = 0x20,
    MCTEST_STATE_LOAD_CHECKING = 0x21,
    MCTEST_STATE_LOAD_NO_FILE = 0x22,
    MCTEST_STATE_LOAD_SELECT = 0x23,
    MCTEST_STATE_LOAD_READING = 0x24,
    MCTEST_STATE_SAVE_QUERY_FREE = 0x30,
    MCTEST_STATE_SAVE_CHECKING_FREE = 0x31,
    MCTEST_STATE_SAVE_SELECT_FILE = 0x32,
    MCTEST_STATE_SAVE_CREATE_CONFIRM = 0x33,
    MCTEST_STATE_SAVE_CREATING = 0x34,
    MCTEST_STATE_SAVE_CHECKING_FILE = 0x35,
    MCTEST_STATE_SAVE_OVERWRITE_CONFIRM = 0x36,
    MCTEST_STATE_SAVE_START = 0x37,
    MCTEST_STATE_SAVE_WRITING = 0x38,
    MCTEST_STATE_FORMAT_INIT = 0x40,
    MCTEST_STATE_FORMAT_CONFIRM = 0x41,
    MCTEST_STATE_FORMATTING = 0x42,
    MCTEST_STATE_FORMAT_DONE = 0x43,
    MCTEST_STATE_UNFORMAT_INIT = 0x50,
    MCTEST_STATE_UNFORMAT_CONFIRM = 0x51,
    MCTEST_STATE_UNFORMATTING = 0x52,
    MCTEST_STATE_UNFORMAT_DONE = 0x53,
    MCTEST_STATE_HDD_MENU_INIT = 0x60,
    MCTEST_STATE_HDD_MENU = 0x61,
    MCTEST_STATE_HDD_LOAD_LIST = 0x70,
    MCTEST_STATE_HDD_LOAD_NO_FILE = 0x71,
    MCTEST_STATE_HDD_LOAD_SELECT = 0x72,
    MCTEST_STATE_HDD_SAVE_INIT = 0x80,
    MCTEST_STATE_HDD_SAVE_SELECT_FILE = 0x81,
    MCTEST_STATE_HDD_SAVE_CREATE_CONFIRM = 0x82,
    MCTEST_STATE_HDD_SAVE_OVERWRITE_CONFIRM = 0x83,
    MCTEST_STATE_HDD_SAVE_WRITE = 0x84,
    MCTEST_STATE_LIST_START = 0x90,
    MCTEST_STATE_LISTING = 0x91,
    MCTEST_STATE_LIST_DONE = 0x92,
    MCTEST_STATE_CHECK_OLD_START = 0xA0,
    MCTEST_STATE_CHECKING_OLD = 0xA1,
    MCTEST_STATE_CHECK_OLD_DONE = 0xA2,
    MCTEST_STATE_DUMMY_QUERY_FREE = 0xB0,
    MCTEST_STATE_DUMMY_CHECKING_FREE = 0xB1,
    MCTEST_STATE_DUMMY_SELECT_SIZE = 0xB2
};

/* JP: メモカテスト (EN: memory card test) */
#define MC_TEXT_TITLE "\x0B\xA5\xE1\xA5\xE2\xA5\xAB\xA5\xC6\xA5\xB9\xA5\xC8"
/* JP: ポート選択 (EN: select a port) */
#define MC_TEXT_SELECT_PORT "\xA5\xDD\xA1\xBC\xA5\xC8\xC1\xAA\xC2\xF2"
/* JP: ロード (EN: load) */
#define MC_TEXT_LOAD "\xA5\xED\xA1\xBC\xA5\xC9"
/* JP: セーブ (EN: save) */
#define MC_TEXT_SAVE "\xA5\xBB\xA1\xBC\xA5\xD6"
/* JP: フォーマット (EN: format) */
#define MC_TEXT_FORMAT "\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8"
/* JP: アンフォーマット (EN: unformat) */
#define MC_TEXT_UNFORMAT "\xA5\xA2\xA5\xF3\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8"
/* JP: 列挙 (EN: list) */
#define MC_TEXT_LIST "\xCE\xF3\xB5\xF3"
/* JP: 旧データチェック (EN: old data check) */
#define MC_TEXT_CHECK_OLD "\xB5\xEC\xA5\xC7\xA1\xBC\xA5\xBF\xA5\xC1\xA5\xA7\xA5\xC3\xA5\xAF"
/* JP: ダミーデータ作成 (EN: create dummy data) */
#define MC_TEXT_DUMMY "\xA5\xC0\xA5\xDF\xA1\xBC\xA5\xC7\xA1\xBC\xA5\xBF\xBA\xEE\xC0\xAE"
/* JP: ロード　ファイル確認中 (EN: load: checking files) */
#define MC_TEXT_LOAD_CHECKING \
    "\xA5\xED\xA1\xBC\xA5\xC9\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xB3\xCE\xC7\xA7\xC3\xE6"
/* JP: ロード　ファイルがありません / Ｏ：戻る (EN: load: no file / O: back) */
#define MC_TEXT_LOAD_NO_FILE \
    "\xA5\xED\xA1\xBC\xA5\xC9\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xA4\xAC\xA4\xA2\xA4\xEA" \
    "\xA4\xDE\xA4\xBB\xA4\xF3\n\n\xA3\xCF\xA1\xA7\xCC\xE1\xA4\xEB"
/* JP: ファイル番号 %2d/99 (EN: file number %2d/99) */
#define MC_TEXT_FILE_OF_99 "\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xC8\xD6\xB9\xE6 %2d/99"
/* JP: ファイルなし (EN: no file) */
#define MC_TEXT_FILE_ABSENT "\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xA4\xCA\xA4\xB7"
/* JP: ファイルあり (EN: file present) */
#define MC_TEXT_FILE_PRESENT "\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xA4\xA2\xA4\xEA"
/* JP: ロード　ファイル%d読み込み中 (EN: load: reading file %d) */
#define MC_TEXT_LOAD_READING \
    "\xA5\xED\xA1\xBC\xA5\xC9\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB%d\xC6\xC9\xA4\xDF\xB9\xFE" \
    "\xA4\xDF\xC3\xE6"
/* JP: セーブ　空き容量：確認中 (EN: save: checking free space) */
#define MC_TEXT_SAVE_CHECKING_FREE \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xB6\xF5\xA4\xAD\xCD\xC6\xCE\xCC\xA1\xA7\xB3\xCE\xC7\xA7\xC3\xE6"
/* JP: セーブ　空き容量：%5dKbyte (EN: save: free space %5d KB) */
#define MC_TEXT_SAVE_FREE \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xB6\xF5\xA4\xAD\xCD\xC6\xCE\xCC\xA1\xA7%5dKbyte"
/* JP: ファイル番号 %2d (EN: file number %2d) */
#define MC_TEXT_FILE_NUMBER "\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xC8\xD6\xB9\xE6 %2d"
/* JP: 新規作成します / Ｏ：はい　　Ｘ：いいえ (EN: create new / O: yes  X: no) */
#define MC_TEXT_CREATE_CONFIRM \
    "\xBF\xB7\xB5\xAC\xBA\xEE\xC0\xAE\xA4\xB7\xA4\xDE\xA4\xB9\n\n\xA3\xCF\xA1\xA7\xA4\xCF\xA4\xA4" \
    "\xA1\xA1\xA1\xA1\xA3\xD8\xA1\xA7\xA4\xA4\xA4\xA4\xA4\xA8"
/* JP: エンセフェロン・シミュレーター内 (EN: inside the Encephalon simulator) */
#define MC_ENTRY_PLACE \
    "\xA5\xA8\xA5\xF3\xA5\xBB\xA5\xD5\xA5\xA7\xA5\xED\xA5\xF3\xA1\xA6\xA5\xB7\xA5\xDF\xA5\xE5" \
    "\xA5\xEC\xA1\xBC\xA5\xBF\xA1\xBC\xC6\xE2"
/* JP: セーブ　新規作成中 (EN: save: creating) */
#define MC_TEXT_SAVE_CREATING \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xBF\xB7\xB5\xAC\xBA\xEE\xC0\xAE\xC3\xE6"
/* JP: セーブ　ファイル%2d確認中 (EN: save: checking file %2d) */
#define MC_TEXT_SAVE_CHECKING_FILE \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB%2d\xB3\xCE\xC7\xA7\xC3\xE6"
/* JP: セーブ　ファイル%2d (EN: save: file %2d) */
#define MC_TEXT_SAVE_FILE "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB%2d"
/* JP: 上書きしてよろしいですか？ / Ｏ：はい　　Ｘ：いいえ (EN: overwrite? / O: yes  X: no) */
#define MC_TEXT_OVERWRITE_CONFIRM \
    "\xBE\xE5\xBD\xF1\xA4\xAD\xA4\xB7\xA4\xC6\xA4\xE8\xA4\xED\xA4\xB7\xA4\xA4\xA4\xC7\xA4\xB9\xA4\xAB" \
    "\xA1\xA9\n\n\xA3\xCF\xA1\xA7\xA4\xCF\xA4\xA4\xA1\xA1\xA1\xA1\xA3\xD8\xA1\xA7\xA4\xA4\xA4\xA4\xA4\xA8"
/* JP: セーブ　ファイル%2d書き込み中 (EN: save: writing file %2d) */
#define MC_TEXT_SAVE_WRITING \
    "\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB%2d\xBD\xF1\xA4\xAD\xB9\xFE" \
    "\xA4\xDF\xC3\xE6"
/* JP: フォーマットしてよろしいですか？ / Ｏ：はい　　×：いいえ (EN: format? / O: yes  X: no) */
#define MC_TEXT_FORMAT_CONFIRM \
    "\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xA4\xB7\xA4\xC6\xA4\xE8\xA4\xED\xA4\xB7\xA4\xA4" \
    "\xA4\xC7\xA4\xB9\xA4\xAB\xA1\xA9\n\n\xA3\xCF\xA1\xA7\xA4\xCF\xA4\xA4\xA1\xA1\xA1\xA1\xA1\xDF\xA1\xA7" \
    "\xA4\xA4\xA4\xA4\xA4\xA8"
/* JP: フォーマット中・・・ (EN: formatting...) */
#define MC_TEXT_FORMATTING \
    "\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xC3\xE6\xA1\xA6\xA1\xA6\xA1\xA6"
/* JP: フォーマットが終了しました / Ｏ：戻る (EN: format finished / O: back) */
#define MC_TEXT_FORMAT_DONE \
    "\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xA4\xAC\xBD\xAA\xCE\xBB\xA4\xB7\xA4\xDE\xA4\xB7" \
    "\xA4\xBF\n\n\xA3\xCF\xA1\xA7\xCC\xE1\xA4\xEB"
/* JP: アンフォーマットしてよろしいですか？ / Ｏ：はい　　×：いいえ (EN: unformat? / O: yes  X: no) */
#define MC_TEXT_UNFORMAT_CONFIRM \
    "\xA5\xA2\xA5\xF3\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xA4\xB7\xA4\xC6\xA4\xE8\xA4\xED" \
    "\xA4\xB7\xA4\xA4\xA4\xC7\xA4\xB9\xA4\xAB\xA1\xA9\n\n\xA3\xCF\xA1\xA7\xA4\xCF\xA4\xA4\xA1\xA1\xA1\xA1" \
    "\xA1\xDF\xA1\xA7\xA4\xA4\xA4\xA4\xA4\xA8"
/* JP: アンフォーマット中・・・ (EN: unformatting...) */
#define MC_TEXT_UNFORMATTING \
    "\xA5\xA2\xA5\xF3\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xC3\xE6\xA1\xA6\xA1\xA6\xA1\xA6"
/* JP: アンフォーマットが終了しました / Ｏ：戻る (EN: unformat finished / O: back) */
#define MC_TEXT_UNFORMAT_DONE \
    "\xA5\xA2\xA5\xF3\xA5\xD5\xA5\xA9\xA1\xBC\xA5\xDE\xA5\xC3\xA5\xC8\xA4\xAC\xBD\xAA\xCE\xBB\xA4\xB7" \
    "\xA4\xDE\xA4\xB7\xA4\xBF\n\n\xA3\xCF\xA1\xA7\xCC\xE1\xA4\xEB"
/* JP: 列挙中・・・ (EN: listing...) */
#define MC_TEXT_LISTING "\xCE\xF3\xB5\xF3\xC3\xE6\xA1\xA6\xA1\xA6\xA1\xA6"
/* JP: 列挙終了 / ○：戻る (EN: listing finished / O: back) */
#define MC_TEXT_LIST_DONE "\xCE\xF3\xB5\xF3\xBD\xAA\xCE\xBB\n\n\xA1\xFB\xA1\xA7\xCC\xE1\xA4\xEB"
/* JP: ダミーデータ作成　空き容量：確認中 (EN: dummy data: checking free space) */
#define MC_TEXT_DUMMY_CHECKING_FREE \
    "\xA5\xC0\xA5\xDF\xA1\xBC\xA5\xC7\xA1\xBC\xA5\xBF\xBA\xEE\xC0\xAE\xA1\xA1\xB6\xF5\xA4\xAD\xCD\xC6" \
    "\xCE\xCC\xA1\xA7\xB3\xCE\xC7\xA7\xC3\xE6"
/* JP: ダミーデータ作成　空き容量：%5dKB (EN: dummy data: free space %5d KB) */
#define MC_TEXT_DUMMY_FREE \
    "\xA5\xC0\xA5\xDF\xA1\xBC\xA5\xC7\xA1\xBC\xA5\xBF\xBA\xEE\xC0\xAE\xA1\xA1\xB6\xF5\xA4\xAD\xCD\xC6" \
    "\xCE\xCC\xA1\xA7%5dKB"
/* JP: 残す容量 %3dKB (EN: space to keep %3d KB) */
#define MC_TEXT_DUMMY_KEEP "\xBB\xC4\xA4\xB9\xCD\xC6\xCE\xCC %3dKB"
/* JP: 左右：増減 / □：１００ＫＢ単位 / ○：作成（時間掛かります）
 * EN: left/right: change / square: 100 KB steps / O: create (takes a while) */
#define MC_TEXT_DUMMY_HELP \
    "\xBA\xB8\xB1\xA6\xA1\xA7\xC1\xFD\xB8\xBA\n\n\xA2\xA2\xA1\xA7\xA3\xB1\xA3\xB0\xA3\xB0\xA3\xCB\xA3\xC2" \
    "\xC3\xB1\xB0\xCC\n\n\xA1\xFB\xA1\xA7\xBA\xEE\xC0\xAE\xA1\xCA\xBB\xFE\xB4\xD6\xB3\xDD\xA4\xAB\xA4\xEA" \
    "\xA4\xDE\xA4\xB9\xA1\xCB"
/* JP: ＨＤＤロード　ファイルがありません / Ｏ：戻る (EN: HDD load: no file / O: back) */
#define MC_TEXT_HDD_LOAD_NO_FILE \
    "\xA3\xC8\xA3\xC4\xA3\xC4\xA5\xED\xA1\xBC\xA5\xC9\xA1\xA1\xA5\xD5\xA5\xA1\xA5\xA4\xA5\xEB\xA4\xAC" \
    "\xA4\xA2\xA4\xEA\xA4\xDE\xA4\xBB\xA4\xF3\n\n\xA3\xCF\xA1\xA7\xCC\xE1\xA4\xEB"
/* JP: ＨＤＤロード (EN: HDD load) */
#define MC_TEXT_HDD_LOAD "\xA3\xC8\xA3\xC4\xA3\xC4\xA5\xED\xA1\xBC\xA5\xC9"
/* JP: ＨＤＤセーブ　空き容量：%5dKbyte (EN: HDD save: free space %5d KB) */
#define MC_TEXT_HDD_SAVE_FREE \
    "\xA3\xC8\xA3\xC4\xA3\xC4\xA5\xBB\xA1\xBC\xA5\xD6\xA1\xA1\xB6\xF5\xA4\xAD\xCD\xC6\xCE\xCC\xA1\xA7" \
    "%5dKbyte"

typedef struct MemcardTestWork {
    int result;
    int check;
    int file;
} MemcardTestWork;

/* The save image is the memory from SaveData on: card file 0 holds its
 * 0x50-byte header, the other files the rest from saveDataBody. */
#define SAVE_HEADER_SIZE 0x50

extern unsigned char saveDataBody[];
/* The event flags the triangle button randomizes. */
extern unsigned char saveEventFlags[];
/* The thumbnail saved with the game: the label of SaveData.thumbnail (SaveData + 0x174). Written as
 * SaveData.thumbnail the copy at the start of MemcardTest makes cse keep the SaveData base in a callee-saved
 * register and add 0x174 with addu, where the original has the absolute address (lui/addiu). */
extern SaveThumbnail saveGameThumbnail;
extern const char yajimaHexDigitFormat[];
extern const char memcardFoundBytesFormat[];
/* HDD menu row -> state: MCTEST_STATE_HDD_LOAD_LIST, MCTEST_STATE_HDD_SAVE_INIT. */
extern unsigned char tbl_49[4];
int sceMcSync(int mode, int *command, int *result);
void FlushCache(int mode);

void MemcardTest(int selection) {
    unsigned char *files[MC_TEST_FILE_COUNT];
    unsigned short port_status[2];
    MemcardTestWork work;
    McRequestParam list_request;
    McRequestParam load_request;
    McRequestParam check_request;
    McRequestParam create_request;
    char progress[32];
    McRequestParam save_request;
    McRequestParam enumerate_request;
    McRequestParam check_old_request;
    struct HddExistRequest hdd_list_request;
    struct HddLoadRequest hdd_load_request;
    struct HddExistRequest hdd_check_request;
    struct HddCheckState hdd_create_request;
    struct HddSaveRequest hdd_save_request;
    int sync_command;
    int sync_result;
    int dummy_command;
    int dummy_result;
    unsigned char *found = (unsigned char *)DEBUG_BUFFER_BASE;
    const char *name;
    char *bar;
    unsigned char *flags;
    unsigned char *shown_flags;
    int port;
    int hdd;
    int state;
    int dir;
    int port_step;
    int menu_step;
    int save_file_step;
    int dummy_step;
    int hdd_menu_step;
    int hdd_file_step;
    int done;
    int file;
    int i;

    state = MCTEST_STATE_INIT;
    port = -1;
    hdd = -99;
    xglCdReadFile("yajima\\thumb.jpg", (void *)DEBUG_BUFFER_BASE, 0, CD_NOTIFY_CURRENT);
    saveGameThumbnail = *(SaveThumbnail *)DEBUG_BUFFER_BASE;
    files[0] = (unsigned char *)MC_TEST_FILE_BASE;
    for (file = 1; file < MC_TEST_FILE_COUNT; file++) {
        files[file] = files[file - 1] + MC_TEST_FILE_STRIDE;
    }
    printf("carddata:%08x\n", files[0]);
    McFileSectionsReport("yajima\\mc\\clear.bin", files[0]);
    McFileSectionsReport("yajima\\mc\\chaos.bin", files[1]);
    McFileSectionsReport("yajima\\mc\\jin.bin", files[2]);
    McFileSectionsReport("yajima\\mc\\jr.bin", files[3]);
    McFileSectionsReport("yajima\\mc\\kosmos.bin", files[4]);
    McFileSectionsReport("yajima\\mc\\momo.bin", files[5]);
    McFileSectionsReport("yajima\\mc\\r02.bin", files[6]);
    McFileSectionsReport("yajima\\mc\\r03.bin", files[7]);
    McFileSectionsReport("yajima\\mc\\r05.bin", files[8]);
    McFileSectionsReport("yajima\\mc\\shion.bin", files[9]);
    McFileSectionsReport("yajima\\mc\\ziggy.bin", files[10]);
    printf("sizeof(sceMcIconSys):%d\n", MC_ICON_SYS_SIZE);
    printf("sizeof(sSaveData):%d\n", SAVE_DATA_SIZE);
    printf("sizeof(carddata):%d\n",
           ((struct HddTransferInfo *)files[0])->end - ((struct HddTransferInfo *)files[0])->begin);

    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        xglFontDebugPrintf(0, 0, MC_TEXT_TITLE);
        /* xglMcMain returns both ports' status halfwords as one word. */
        *(unsigned int *)port_status = xglMcMain();
        xglFontDebugPrintf(64, 0, "%4x %4x", port_status[0], port_status[1]);
        if (port >= 0 && port < MC_PORT_HDD) {
            if (port_status[port] != MC_PORT_READY && port_status[port] != MC_PORT_BUSY) {
                state = MCTEST_STATE_INIT;
            }
        }
        xglFontDebugPrintf(0, 16, "%2x", state);

        switch (state) {
        case MCTEST_STATE_INIT:
            state = MCTEST_STATE_SELECT_PORT;
            hdd = xglHddCheck();
            /* fall through */
        case MCTEST_STATE_SELECT_PORT:
            xglFontDebugPrintf(0, 32, MC_TEXT_SELECT_PORT);
            name = mt_GetPortName(0);
            if (port_status[0] == MC_PORT_READY) {
                xglFontDebugPrintf(16, 48, "%s", name);
            } else {
                /* Text: {color #202020}%s */
                xglFontDebugPrintf(16, 48, "\x0C   %s", name);
            }
            name = mt_GetPortName(1);
            if (port_status[1] == MC_PORT_READY) {
                xglFontDebugPrintf(16, 64, "%s", name);
            } else {
                xglFontDebugPrintf(16, 64, "\x0C   %s", name);
            }
            name = mt_GetPortName(2);
            if (hdd == 0) {
                xglFontDebugPrintf(16, 80, "%s", name);
            } else {
                xglFontDebugPrintf(16, 80, "\x0C   %s", name);
            }
            if (port_status[0] == MC_PORT_READY || port_status[1] == MC_PORT_READY || hdd == 0) {
                port_step = (PadData.ports[0].pressed & PAD_UP) ? -1 : 0;
                if (PadData.ports[0].pressed & PAD_DOWN) {
                    port_step = 1;
                }
                if (port_step != 0) {
                    work.result += port_step;
                } else {
                    port_step = 1;
                }
                for (;;) {
                    work.result = (work.result + MC_PORT_COUNT) % MC_PORT_COUNT;
                    if ((port_status[0] != MC_PORT_READY && work.result == 0) ||
                        (port_status[1] != MC_PORT_READY && work.result == 1) ||
                        (hdd < 0 && work.result == MC_PORT_HDD)) {
                        work.result += port_step;
                    } else {
                        break;
                    }
                }
            } else {
                work.result = -1;
            }
            if (work.result != -1 && (PadData.ports[0].pressed & PAD_CIRCLE)) {
                if (work.result == MC_PORT_HDD) {
                    port = MC_PORT_HDD;
                    state = MCTEST_STATE_HDD_MENU_INIT;
                } else {
                    port = work.result;
                    state = MCTEST_STATE_MENU_INIT;
                }
            }
            xglFontDebugPrintf(8, work.result * 16 + 48, menuCursorText);
            break;

        case MCTEST_STATE_MENU_INIT:
            work.result = 0;
            state = MCTEST_STATE_MENU;
            /* fall through */
        case MCTEST_STATE_MENU:
            xglFontDebugPrintf(0, 32, "%s", mt_GetPortName(port));
            xglFontDebugPrintf(16, 48, MC_TEXT_LOAD);
            xglFontDebugPrintf(16, 64, MC_TEXT_SAVE);
            xglFontDebugPrintf(16, 80, MC_TEXT_FORMAT);
            xglFontDebugPrintf(16, 96, MC_TEXT_UNFORMAT);
            xglFontDebugPrintf(16, 112, MC_TEXT_LIST);
            xglFontDebugPrintf(16, 128, MC_TEXT_CHECK_OLD);
            xglFontDebugPrintf(16, 144, MC_TEXT_DUMMY);
            menu_step = (PadData.ports[0].pressed & PAD_UP) ? -1 : 0;
            if (PadData.ports[0].pressed & PAD_DOWN) {
                menu_step = 1;
            }
            if (menu_step != 0) {
                work.result = (work.result + menu_step + MC_MENU_ITEM_COUNT) % MC_MENU_ITEM_COUNT;
            }
            xglFontDebugPrintf(8, work.result * 16 + 48, menuCursorText);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                static unsigned char menu_states[8] = {
                    MCTEST_STATE_LOAD_LIST, MCTEST_STATE_SAVE_QUERY_FREE,
                    MCTEST_STATE_FORMAT_INIT, MCTEST_STATE_UNFORMAT_INIT,
                    MCTEST_STATE_LIST_START, MCTEST_STATE_CHECK_OLD_START,
                    MCTEST_STATE_DUMMY_QUERY_FREE
                };

                if (PadData.ports[0].held & PAD_R1) {
                    state = MCTEST_STATE_INIT;
                    sceMcSync(0, &sync_command, &sync_result);
                    printf("%d %d\n", sync_command, sync_result);
                    xglMcEasySave("/BISLPS-99999Dummy", (void *)DEBUG_BUFFER_BASE, MC_EASY_SAVE_DUMMY_SIZE);
                } else {
                    state = menu_states[work.result];
                }
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_INIT;
            }
            break;

        case MCTEST_STATE_LOAD_LIST:
            list_request.file = -1;
            list_request.data = found;
            xglMcRequest(port, MC_REQ_EXIST, &list_request, &work.result);
            state = MCTEST_STATE_LOAD_CHECKING;
            break;

        case MCTEST_STATE_LOAD_CHECKING:
            xglFontDebugPrintf(0, 32, MC_TEXT_LOAD_CHECKING);
            if (port_status[port] != MC_PORT_BUSY) {
                state = MCTEST_STATE_LOAD_NO_FILE;
                if (work.result >= 0) {
                    work.file = 0;
                    state = MCTEST_STATE_LOAD_SELECT;
                }
            }
            break;

        case MCTEST_STATE_LOAD_NO_FILE:
            xglFontDebugPrintf(0, 32, MC_TEXT_LOAD_NO_FILE, work.file);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_LOAD_SELECT:
            xglFontDebugPrintf(0, 32, MC_TEXT_LOAD);
            dir = 0;
            if (PadData.ports[0].repeated & PAD_LEFT) {
                dir = -1;
            }
            if (PadData.ports[0].repeated & PAD_RIGHT) {
                dir = 1;
            }
            if (dir != 0) {
                work.file = (work.file + dir + MC_SAVE_FILE_COUNT) % MC_SAVE_FILE_COUNT;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_FILE_OF_99, work.file);
            if (found[work.file] == 0) {
                xglFontDebugPrintf(16, 64, MC_TEXT_FILE_ABSENT);
            } else {
                xglFontDebugPrintf(16, 64, MC_TEXT_FILE_PRESENT);
            }
            if ((PadData.ports[0].pressed & PAD_CIRCLE) && found[work.file] != 0) {
                load_request.file = work.file;
                load_request.data = &SaveData;
                load_request.size = SAVE_DATA_SIZE;
                state = MCTEST_STATE_LOAD_READING;
                xglMcRequest(port, MC_REQ_LOAD, &load_request, &work.result);
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_LOAD_READING:
            xglFontDebugPrintf(0, 32, MC_TEXT_LOAD_READING, work.file);
            if (port_status[port] != MC_PORT_BUSY) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_SAVE_QUERY_FREE:
            xglMcRequest(port, MC_REQ_FREE_SPACE, 0, &work.result);
            state = MCTEST_STATE_SAVE_CHECKING_FREE;
            work.check = 0;
            break;

        case MCTEST_STATE_SAVE_CHECKING_FREE:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_CHECKING_FREE);
            if (port_status[port] != MC_PORT_BUSY) {
                work.file = 0;
                state = MCTEST_STATE_SAVE_SELECT_FILE;
            }
            break;

        case MCTEST_STATE_SAVE_SELECT_FILE:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_FREE, work.result);
            save_file_step = (PadData.ports[0].repeated & PAD_LEFT) ? -1 : 0;
            if (PadData.ports[0].repeated & PAD_RIGHT) {
                save_file_step = 1;
            }
            if (save_file_step != 0) {
                work.file = (work.file + save_file_step + MC_SAVE_FILE_COUNT) % MC_SAVE_FILE_COUNT;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_FILE_NUMBER, work.file);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                check_request.file = work.file;
                xglMcRequest(port, MC_REQ_EXIST, &check_request, &work.check);
                state = MCTEST_STATE_SAVE_CHECKING_FILE;
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_SAVE_CREATE_CONFIRM:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_FREE, work.result);
            xglFontDebugPrintf(16, 48, MC_TEXT_CREATE_CONFIRM);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                create_request.data = files;
                create_request.file = work.file;
                if (work.file < MC_TEST_FILE_COUNT) {
                    create_request.data = files[work.file];
                }
                state = MCTEST_STATE_SAVE_CREATING;
                xglMcSetEntryNumbers(create_request.data, create_request.file,
                                     (const XglClock *)MC_ENTRY_PLACE /* the debug test passes its place text; the callee reads its bytes 2 and 3 */);
                xglMcRequest(port, MC_REQ_CREATE, &create_request, &work.result);
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_SAVE_CREATING:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_CREATING);
            bar = progress;
            for (done = 0; done < work.result; done++) {
                *bar++ = '*';
            }
            for (; done < MC_CREATE_PROGRESS_STEPS; done++) {
                *bar++ = '.';
            }
            *bar = '\0';
            xglFontDebugPrintf(16, 48, progress);
            if (port_status[port] != MC_PORT_BUSY) {
                state = MCTEST_STATE_SAVE_START;
            }
            break;

        case MCTEST_STATE_SAVE_CHECKING_FILE:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_CHECKING_FILE, work.file);
            if (port_status[port] != MC_PORT_BUSY) {
                state = MCTEST_STATE_SAVE_CREATE_CONFIRM;
            }
            break;

        case MCTEST_STATE_SAVE_OVERWRITE_CONFIRM:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_FILE, work.file);
            xglFontDebugPrintf(16, 48, MC_TEXT_OVERWRITE_CONFIRM);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                state = MCTEST_STATE_SAVE_START;
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_SAVE_START:
            save_request.file = work.file;
            if (work.file == 0) {
                save_request.data = &SaveData;
                save_request.size = SAVE_HEADER_SIZE;
            } else {
                save_request.data = saveDataBody;
                save_request.size = SAVE_DATA_SIZE - SAVE_HEADER_SIZE;
            }
            xglMcRequest(port, MC_REQ_SAVE, &save_request, &work.result);
            state = MCTEST_STATE_SAVE_WRITING;
            break;

        case MCTEST_STATE_SAVE_WRITING:
            xglFontDebugPrintf(0, 32, MC_TEXT_SAVE_WRITING, work.file);
            if (port_status[port] != MC_PORT_BUSY) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_FORMAT_INIT:
            state = MCTEST_STATE_FORMAT_CONFIRM;
            /* fall through */
        case MCTEST_STATE_FORMAT_CONFIRM:
            xglFontDebugPrintf(0, 32, MC_TEXT_FORMAT_CONFIRM);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                xglMcRequest(port, MC_REQ_FORMAT, 0, &work.result);
                state = MCTEST_STATE_FORMATTING;
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_FORMATTING:
            xglFontDebugPrintf(0, 32, MC_TEXT_FORMATTING);
            if (port_status[port] != MC_PORT_BUSY) {
                state = MCTEST_STATE_FORMAT_DONE;
            }
            break;

        case MCTEST_STATE_FORMAT_DONE:
            xglFontDebugPrintf(0, 32, MC_TEXT_FORMAT_DONE);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_UNFORMAT_INIT:
            state = MCTEST_STATE_UNFORMAT_CONFIRM;
            /* fall through */
        case MCTEST_STATE_UNFORMAT_CONFIRM:
            xglFontDebugPrintf(0, 32, MC_TEXT_UNFORMAT_CONFIRM);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                xglMcRequest(port, MC_REQ_UNFORMAT, 0, &work.result);
                state = MCTEST_STATE_UNFORMATTING;
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_UNFORMATTING:
            xglFontDebugPrintf(0, 32, MC_TEXT_UNFORMATTING);
            if (port_status[port] != MC_PORT_BUSY) {
                state = MCTEST_STATE_UNFORMAT_DONE;
            }
            break;

        case MCTEST_STATE_UNFORMAT_DONE:
            xglFontDebugPrintf(0, 32, MC_TEXT_UNFORMAT_DONE);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_LIST_START:
            enumerate_request.file = -1;
            enumerate_request.data = found;
            enumerate_request.size = SAVE_DATA_SIZE;
            enumerate_request.callback = DebugCallbackLog;
            state = MCTEST_STATE_LISTING;
            xglMcRequest(port, MC_REQ_LIST, &enumerate_request, &work.result);
            break;

        case MCTEST_STATE_LISTING:
        case MCTEST_STATE_LIST_DONE:
            for (i = 1; i < MC_SAVE_FILE_COUNT; i++) {
                xglFontDebugPrintf((i % 10) * 8 + 128, (i / 10) * 8 + 48, yajimaHexDigitFormat, found[i]);
            }
            xglFontDebugPrintf(128, 128, memcardFoundBytesFormat, found[100], found[101]);
            {
                unsigned char *pattern = (unsigned char *)MC_TEST_PATTERN_BASE;

                xglFontDebugPrintf(16, 160, "%2x %2x %2x %2x %2x %2x %2x %2x", pattern[0], pattern[1],
                                   pattern[2], pattern[3], pattern[4], pattern[5], pattern[6], pattern[7]);
            }
            if (state == MCTEST_STATE_LISTING) {
                if (found[101] == 2) {
                    state = MCTEST_STATE_LIST_DONE;
                } else {
                    xglFontDebugPrintf(0, 32, MC_TEXT_LISTING);
                }
            } else {
                xglFontDebugPrintf(0, 32, MC_TEXT_LIST_DONE);
                if (PadData.ports[0].pressed & PAD_CIRCLE) {
                    state = MCTEST_STATE_MENU_INIT;
                }
            }
            break;

        case MCTEST_STATE_CHECK_OLD_START:
            check_old_request.file = 0;
            check_old_request.data = found;
            check_old_request.callback = McOldDataCheckCallback;
            state = MCTEST_STATE_CHECKING_OLD;
            xglMcRequest(port, MC_REQ_CHECK_OLD, &check_old_request, &work.result);
            break;

        case MCTEST_STATE_CHECKING_OLD:
        case MCTEST_STATE_CHECK_OLD_DONE:
            for (i = 1; i < MC_SAVE_FILE_COUNT; i++) {
                xglFontDebugPrintf((i % 10) * 8 + 128, (i / 10) * 8 + 48, yajimaHexDigitFormat, found[i]);
            }
            xglFontDebugPrintf(128, 128, memcardFoundBytesFormat, found[100], found[101]);
            {
                unsigned char *pattern = (unsigned char *)MC_TEST_PATTERN_BASE;

                xglFontDebugPrintf(16, 160, "%2x %2x %2x %2x %2x %2x %2x %2x", pattern[0], pattern[1],
                                   pattern[2], pattern[3], pattern[4], pattern[5], pattern[6], pattern[7]);
            }
            if (state == MCTEST_STATE_CHECKING_OLD) {
                if (found[101] != 2) {
                    xglFontDebugPrintf(0, 32, MC_TEXT_LISTING);
                } else {
                    state = MCTEST_STATE_CHECK_OLD_DONE;
                }
            } else {
                xglFontDebugPrintf(0, 32, MC_TEXT_LIST_DONE);
                if (PadData.ports[0].pressed & PAD_CIRCLE) {
                    state = MCTEST_STATE_MENU_INIT;
                }
            }
            break;

        case MCTEST_STATE_DUMMY_QUERY_FREE:
            xglMcRequest(port, MC_REQ_FREE_SPACE, 0, &work.result);
            state = MCTEST_STATE_DUMMY_CHECKING_FREE;
            break;

        case MCTEST_STATE_DUMMY_CHECKING_FREE:
            xglFontDebugPrintf(0, 32, MC_TEXT_DUMMY_CHECKING_FREE);
            if (port_status[port] != MC_PORT_BUSY) {
                work.check = 0;
                state = MCTEST_STATE_DUMMY_SELECT_SIZE;
            }
            break;

        case MCTEST_STATE_DUMMY_SELECT_SIZE:
            xglFontDebugPrintf(0, 32, MC_TEXT_DUMMY_FREE, work.result);
            dummy_step = (PadData.ports[0].repeated & PAD_LEFT) ? -1 : 0;
            if (PadData.ports[0].repeated & PAD_RIGHT) {
                dummy_step = 1;
            }
            if (PadData.ports[0].held & PAD_SQUARE) {
                dummy_step *= 100;
            }
            if (dummy_step != 0) {
                work.check = (work.check + dummy_step + MC_DUMMY_LIMIT_KB) % MC_DUMMY_LIMIT_KB;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_DUMMY_KEEP, work.check);
            xglFontDebugPrintf(16, 64, MC_TEXT_DUMMY_HELP);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                sceMcSync(0, &dummy_command, &dummy_result);
                xglMcEasySave("/BISLPS-99999Dummy", (void *)DEBUG_BUFFER_BASE,
                              (work.result - work.check) << 10);
                state = MCTEST_STATE_DUMMY_QUERY_FREE;
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_MENU_INIT;
            }
            break;

        case MCTEST_STATE_HDD_MENU_INIT:
            work.result = 0;
            state = MCTEST_STATE_HDD_MENU;
            /* fall through */
        case MCTEST_STATE_HDD_MENU:
            xglFontDebugPrintf(0, 32, "%s", mt_GetPortName(port));
            xglFontDebugPrintf(16, 48, MC_TEXT_LOAD);
            xglFontDebugPrintf(16, 64, MC_TEXT_SAVE);
            hdd_menu_step = (PadData.ports[0].pressed & PAD_UP) ? -1 : 0;
            if (PadData.ports[0].pressed & PAD_DOWN) {
                hdd_menu_step = 1;
            }
            if (hdd_menu_step != 0) {
                work.result = (work.result + hdd_menu_step + MC_HDD_MENU_ITEM_COUNT) % MC_HDD_MENU_ITEM_COUNT;
            }
            xglFontDebugPrintf(8, work.result * 16 + 48, menuCursorText);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                state = tbl_49[work.result];
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_INIT;
            }
            break;

        case MCTEST_STATE_HDD_LOAD_LIST:
            hdd_list_request.saveNumber = -1;
            hdd_list_request.found = found;
            if (xglHddMcExist(&hdd_list_request) < 0) {
                state = MCTEST_STATE_HDD_LOAD_NO_FILE;
            } else {
                state = MCTEST_STATE_HDD_LOAD_SELECT;
            }
            break;

        case MCTEST_STATE_HDD_LOAD_NO_FILE:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_LOAD_NO_FILE, work.file);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                state = MCTEST_STATE_HDD_MENU_INIT;
            }
            break;

        case MCTEST_STATE_HDD_LOAD_SELECT:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_LOAD);
            dir = 0;
            if (PadData.ports[0].repeated & PAD_LEFT) {
                dir = -1;
            }
            if (PadData.ports[0].repeated & PAD_RIGHT) {
                dir = 1;
            }
            if (dir != 0) {
                work.file = (work.file + dir + MC_SAVE_FILE_COUNT) % MC_SAVE_FILE_COUNT;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_FILE_OF_99, work.file);
            if (found[work.file] == 0) {
                xglFontDebugPrintf(16, 64, MC_TEXT_FILE_ABSENT);
            } else {
                xglFontDebugPrintf(16, 64, MC_TEXT_FILE_PRESENT);
            }
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                if (found[work.file] != 0) {
                    hdd_load_request.saveNumber = work.file;
                    hdd_load_request.data = &SaveData;
                    hdd_load_request.size = SAVE_DATA_SIZE;
                    printf("xglHddMcLoad():%d\n", xglHddMcLoad(&hdd_load_request));
                    state = MCTEST_STATE_HDD_MENU_INIT;
                }
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_HDD_MENU_INIT;
            }
            break;

        case MCTEST_STATE_HDD_SAVE_INIT:
            work.result = 0;
            state = MCTEST_STATE_HDD_SAVE_SELECT_FILE;
            /* fall through */
        case MCTEST_STATE_HDD_SAVE_SELECT_FILE:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_SAVE_FREE, work.result);
            hdd_file_step = (PadData.ports[0].pressed & PAD_LEFT) ? -1 : 0;
            if (PadData.ports[0].pressed & PAD_RIGHT) {
                hdd_file_step = 1;
            }
            if (hdd_file_step != 0) {
                work.file = (work.file + hdd_file_step + MC_SAVE_FILE_COUNT) % MC_SAVE_FILE_COUNT;
            }
            xglFontDebugPrintf(16, 48, MC_TEXT_FILE_NUMBER, work.file);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                int exist;

                hdd_check_request.saveNumber = work.file;
                state = MCTEST_STATE_HDD_SAVE_CREATE_CONFIRM;
                exist = xglHddMcExist(&hdd_check_request);
                printf("xglHddMcExist():%d\n", exist);
                if (exist < 0) {
                    state = MCTEST_STATE_HDD_MENU_INIT;
                }
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_HDD_MENU_INIT;
            }
            break;

        case MCTEST_STATE_HDD_SAVE_CREATE_CONFIRM:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_SAVE_FREE, work.result);
            xglFontDebugPrintf(16, 48, MC_TEXT_CREATE_CONFIRM);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                hdd_create_request.saveNumber = work.file;
                hdd_create_request.transfer.files = files;
                hdd_create_request.dataSize = SAVE_DATA_SIZE;
                printf("xglHddMcCreate():%d\n", xglHddMcCreate(&hdd_create_request));
                state = MCTEST_STATE_HDD_SAVE_WRITE;
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_HDD_MENU_INIT;
            }
            break;

        case MCTEST_STATE_HDD_SAVE_OVERWRITE_CONFIRM:
            xglFontDebugPrintf(0, 32, MC_TEXT_HDD_SAVE_FREE, work.result);
            xglFontDebugPrintf(16, 48, MC_TEXT_OVERWRITE_CONFIRM);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                state = MCTEST_STATE_HDD_SAVE_WRITE;
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_HDD_MENU_INIT;
            }
            break;

        case MCTEST_STATE_HDD_SAVE_WRITE:
            hdd_save_request.saveNumber = work.file;
            hdd_save_request.data = &SaveData;
            hdd_save_request.size = SAVE_DATA_SIZE;
            printf("xglHddMcSave():%d\n", xglHddMcSave(&hdd_save_request));
            state = MCTEST_STATE_HDD_MENU_INIT;
            break;
        }

        if (PadData.ports[0].pressed & PAD_TRIANGLE) {
            flags = saveEventFlags;
            for (i = 0; i < SAVE_FLAGS_COUNT; i++) {
                *flags++ = xglSRand();
            }
            FlushCache(0);
        }
        shown_flags = saveEventFlags;
        xglFontDebugPrintf(16, 180, "%2x %2x %2x %2x %2x %2x %2x %2x", shown_flags[0], shown_flags[1],
                           shown_flags[2], shown_flags[3], shown_flags[4], shown_flags[5], shown_flags[6],
                           shown_flags[7]);
        xglSleep();
    }
}

extern unsigned char CdSlotCursor;
extern const char cdSlotFreeText[];
extern const char cdSlotBoundaryText[];
extern const char cdSlotBufferSizeFormat[];
extern const char cdSlotNameFormat[];

/* CD slot view: UP and DOWN move the cursor over the slots of gameResBlockTable, the
 * buttons below load or free paths, and the table is listed from row 0x40. */
void CdSlotTableView(void) {
    GameResBlock *entry;
    int index;
    int owner;
    int y;
    McTestPadPort *pad;

    if (PadData.ports[0].repeated & PAD_UP) {
        CdSlotCursor--;
        if (CdSlotCursor == 0) {
            CdSlotCursor = 1;
        }
    }
    if (PadData.ports[0].repeated & PAD_DOWN) {
        CdSlotCursor++;
        if (gameResBlockTable[CdSlotCursor].owner == -1) {
            do {
                CdSlotCursor--;
            } while (gameResBlockTable[CdSlotCursor].owner == -1);
        }
    }
    pad = &PadData.ports[0];
    if (pad->pressed & PAD_CIRCLE) {
        resPlayerSlotsLoadModels(0x1000A, 0x1001C, 0x10016);
    }
    if (pad->pressed & PAD_TRIANGLE) {
        resPlayerSlotsLoadModels(0x10016, 0x10026, 0x10002);
    }
    if (pad->pressed & PAD_SQUARE) {
        resPlayerSlotUpdate(0);
    }
    if (pad->pressed & PAD_R1) {
        resPlayerSlotUpdate(-1);
    }
    if (pad->pressed & PAD_R2) {
        resPlayerSlotsReset();
    }
    if (pad->pressed & PAD_CROSS) {
        GameResBlockFreeByOwner(gameResBlockTable[CdSlotCursor].owner);
    }

    y = 0x40;
    index = 1;
    entry = gameResBlockTable + index;
    owner = entry->owner;
    if (owner != -1) {
        do {
            const char *name;

            if (owner == 0) {
                name = cdSlotFreeText;
            } else if ((owner & 0xFF0000) == 0xF00000) {
                name = entry->buffer;
            } else {
                name = xglCdFindFileName(owner & 0xFFFFFF);
            }
            if (index == gameResBlockTable[0].owner) {
                xglFontDebugPrintf(0xC, y, cdSlotBoundaryText);
                y += 8;
            }
            if (index == CdSlotCursor) {
                xglFontDebugPrintf(4, y, menuCursorText);
            }
            xglFontDebugPrintf(0xC, y, cdSlotBufferSizeFormat, entry->buffer, entry->size);
            xglFontDebugPrintf(0x78, y, cdSlotNameFormat, name);
            y += 8;
            index++;
            if (index < GAME_RES_BLOCK_COUNT) {
                entry = gameResBlockTable + index;
                if (entry->owner != -1) {
                    owner = entry->owner;
                    continue;
                }
            }
            break;
        } while (1);
    }
}

/* [0]: the cf file number stepped by the pad, passed to resLoadCfFile/resApplyCfFile; [1]: name or owner
 * column, toggled by LEFT/RIGHT. */
extern unsigned char cdSlotCfFileIndex;
extern unsigned char cdSlotOwnerColumnFlag;
extern const char yajimaHexWordFormat[];
extern const char cdSlotTableCountFormat[];

/* Pad 0 steps the byte flag cdSlotCfFileIndex (the cf file number shown by the load and
 * apply buttons) and toggles cdSlotOwnerColumnFlag (name or owner column), then the
 * CD slot table is listed from row 0x40 and the two count-prefixed entry
 * tables from resGetEntryTables follow in two columns. */
void CdSlotEntryTablesView(void) {
    McTestPadPort *pad = &PadData.ports[0];
    GameResBlock *entry;
    const char *name;
    int owner;
    int index;
    int y;
    unsigned int *leftTable;
    unsigned int *rightTable;

    if (pad->repeated & PAD_UP) {
        cdSlotCfFileIndex += 0xFF;
    }
    if (pad->repeated & PAD_DOWN) {
        cdSlotCfFileIndex += 1;
    }
    if (pad->pressed & (PAD_LEFT | PAD_RIGHT)) {
        cdSlotOwnerColumnFlag ^= 1;
    }
    if (pad->pressed & PAD_SQUARE) {
        GameResourceReset();
    }
    if (pad->pressed & PAD_TRIANGLE) {
        resLoadCfFile(cdSlotCfFileIndex);
    }
    if (pad->pressed & PAD_CIRCLE) {
        resApplyCfFile(cdSlotCfFileIndex);
    }
    if (pad->pressed & PAD_CROSS) {
        resResetScriptState();
    }

    y = 0x40;
    index = 1;
    entry = gameResBlockTable + index;
    owner = entry->owner;
    if (owner != -1) {
        do {
            if (owner == 0) {
                name = cdSlotFreeText;
            } else if ((owner & 0xFF0000) == 0xF00000) {
                name = entry->buffer;
            } else {
                name = xglCdFindFileName(owner);
            }
            xglFontDebugPrintf(0xC, y, cdSlotBufferSizeFormat, entry->buffer, entry->size);
            if (cdSlotOwnerColumnFlag == 0) {
                xglFontDebugPrintf(0x78, y, cdSlotNameFormat, name);
            } else {
                xglFontDebugPrintf(0x78, y, yajimaHexWordFormat, entry->owner);
            }
            index++;
            y += 8;
            if (index < GAME_RES_BLOCK_COUNT) {
                entry = gameResBlockTable + index;
                if (entry->owner != -1) {
                    owner = entry->owner;
                    continue;
                }
            }
            break;
        } while (1);
    }

    resGetEntryTables(&rightTable, &leftTable);
    xglFontDebugPrintf(0x10, 0x18, "%2x", cdSlotCfFileIndex);
    y += 8;
    xglFontDebugPrintf(0x10, 0x20, cdSlotTableCountFormat, leftTable[0], rightTable[0]);
    index = 1;
    do {
        if (index < leftTable[0]) {
            xglFontDebugPrintf(0x20, y, yajimaHexWordFormat, leftTable[index]);
        }
        if (index < rightTable[0]) {
            xglFontDebugPrintf(0x60, y, yajimaHexWordFormat, rightTable[index]);
        }
        index++;
        y += 8;
    } while (index < 0x40);
}

/* JP: リソーステスト (EN: resource test), with "%d" for the view. */
/* The spinner character "*". */
/* Frame counter of the spinner. */
extern unsigned char cdSlotTestBlinkCount;
/* CD slot view: UP and DOWN move the cursor over the slots of gameResBlockTable, the * buttons below load or free paths, and the table is listed from row 0x40. */
void CdSlotTableView(void);
/* Pad 0 steps the byte flag cdSlotCfFileIndex (the slot shown by the load and * apply buttons) and toggles cdSlotOwnerColumnFlag (name or owner column), then the * CD slot table is listed from row 0x40 and the two count-prefixed entry * tables from resGetEntryTables follow in two columns. */
void CdSlotEntryTablesView(void);

/* CD slot test: shows the slot table or the second view (SELECT toggles),
 * START reloads the path table. */
const char cdSlotNameFormat[24] = "$zoom12;%s$zoom16;";

const char yajimaHexDigitFormat[8] = "%1x";

const char memcardFoundBytesFormat[8] = "%2x %2x";

unsigned char tbl_49[4] = {
    0x70, 0x80, 0x00, 0x00
};

unsigned char CdSlotCursor = 1;

const char cdSlotFreeText[8] = "(free)";

const char cdSlotBoundaryText[8] = "----";

const char cdSlotBufferSizeFormat[8] = "%8x %8x";

unsigned char cdSlotCfFileIndex = 0;

unsigned char cdSlotOwnerColumnFlag = 0;

const char yajimaHexWordFormat[8] = "%8x";

const char cdSlotTableCountFormat[8] = "%3d %3d";

unsigned char cdSlotTestBlinkCount = 0;

void CdSlotTest(int selection) {
    int view = 0;

    GameResourceInit((void *) DEBUG_BUFFER_BASE, DEBUG_BUFFER_BASE);
    resPlayerSlotsInit();
    resSelectSlot(1);
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        /* JP: リソーステスト%1x
         * EN: Resource test %1x */
        xglFontDebugPrintf(0, 0, "\x0B\xA5\xEA\xA5\xBD\xA1\xBC\xA5\xB9\xA5\xC6\xA5\xB9\xA5\xC8%1x", view);
        cdSlotTestBlinkCount++;
        xglFontDebugPrintf(((cdSlotTestBlinkCount * 4) & 0x38) + 0x50, 0, "*");
        if (PadData.ports[0].pressed & PAD_START) {
            cdSlotTestOnStart();
        }
        if (PadData.ports[0].pressed & PAD_SELECT) {
            view ^= 1;
        }
        switch (view) {
        case 0:
            CdSlotTableView();
            break;
        case 1:
            CdSlotEntryTablesView();
            break;
        }
        xglSleep();
    }
}

/* The movie list "host0:/home/xeno/vcd-f/x2u/movie/sfd.lst". */
/* JP: ムービーリスト (EN: movie list) title. */
/* JP: ∥：ＤＯＬＢＹ ＤＩＧＩＴＡＬ (EN: dolby digital) "%s". */
/* "VOL%3d". */
/* "ON". */
/* "OFF". */
/* Scratch memory of the tests. */
extern int sceOpen(const char *path, int flags, ...);
extern int sceRead(int descriptor, void *buffer, int bytes);
extern int sceClose(int descriptor);
void *memset(void *destination, int value, unsigned int count);
/* Scrolls the view by the pad (UP/DOWN move the selection, RIGHT/LEFT move * twenty lines), then draws the lines from view->top. Returns whether CIRCLE is * pressed: MovieListTest plays the selected movie then. */
int DebugTextViewUpdate(DebugTextView *view);

/* Movie list test: shows sfd.lst as a scrolling text; square toggles the
 * Dolby Digital flag of the save data, L1 and R1 set the volume and circle
 * plays the selected movie (its name up to the '.'). */
void MovieListTest(int selection) {
    DebugTextView view;
    char name[0x40];
    int volume = 0x7F;
    char *list;
    int descriptor;
    char *cursor;
    char *out;

    GameResourceInit((void *) DEBUG_BUFFER_BASE, DEBUG_BUFFER_BASE);
    list = (char *)GameResBufferAlloc(0x4000);
    memset(list, 0, 0x4000);
    descriptor = sceOpen("host0:/home/xeno/vcd-f/x2u/movie/sfd.lst", 1);
    sceRead(descriptor, list, 0x4000);
    sceClose(descriptor);
    DebugTextViewInit(&view, list);
    SaveData.caption_all = 2; /* the save default: every caption line shown */
    gameWork.control = 0;
    soundStereoSet(0);
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        /* JP: 字幕確認
         * EN: Subtitle check */
        xglFontDebugPrintf(0, 0, "\x0B\xBB\xFA\xCB\xEB\xB3\xCE\xC7\xA7");
        if (PadData.ports[0].pressed & PAD_SQUARE) {
            SaveData.dolby_digital ^= 1;
        }
        /* JP: □：ＤｏｌｂｙＤｉｇｉｔａｌ:%s
         * EN: (Square): Dolby Digital: %s */
        xglFontDebugPrintf(0, 0xD0, "\xA2\xA2\xA1\xA7\xA3\xC4\xA3\xEF\xA3\xEC\xA3\xE2\xA3\xF9\xA3\xC4\xA3\xE9\xA3\xE7\xA3\xE9\xA3\xF4\xA3\xE1\xA3\xEC:%s", SaveData.dolby_digital ? "ON" : "OFF");
        if (PadData.ports[0].repeated & (PAD_L1 | PAD_R1)) {
            if (PadData.ports[0].repeated & PAD_L1) {
                volume--;
            }
            if (PadData.ports[0].repeated & PAD_R1) {
                volume++;
            }
            if (volume < 0) {
                volume = 0;
            }
            if (volume > 0x7F) {
                volume = 0x7F;
            }
        }
        /* JP: ＬＲ：VOL%3d
         * EN: LR: VOL%3d */
        xglFontDebugPrintf(0xB0, 0xD0, "\xA3\xCC\xA3\xD2\xA1\xA7VOL%3d", volume);
        if (DebugTextViewUpdate(&view)) {
            cursor = view.cursor;
            out = name;
            while (*cursor != '.') {
                *out++ = *cursor++;
            }
            *out = 0;
            GameMpeg2Play(name);
            xglRenderClearFrame();
            xglRenderClearColor(0x80004000);
        }
        xglSleep();
    }
}

/* "yajima\\ipu_101.ipu". */
/* "%8x %1x %8x". */
/* "%dx%d\n". */
/* JP: IPU title. */

/* The textured sprite that shows a decoded frame: TEX0 at 0x10, the two
 * XYZ2 vertices at 0x40 and 0x60 and the second vertex's UV at 0x50. */
typedef struct IpuSpritePacket {
    unsigned char unmodeled_00[0x10];
    unsigned long long tex0;
    unsigned char unmodeled_18[0x40 - 0x18];
    int x0;
    int y0;
    unsigned char unmodeled_48[0x50 - 0x48];
    int u1;
    int v1;
    unsigned char unmodeled_58[0x60 - 0x58];
    int x1;
    int y1;
    unsigned char unmodeled_68[0x70 - 0x68];
} IpuSpritePacket;

extern unsigned long long ipuMovieSetupPacket[12];
extern unsigned long long ipuMovieImageHeaderPacket[4];
extern unsigned long long ipuMovieTransferEndPacket[6];
extern unsigned long long ipuMovieDrawStatePacket[8];
extern IpuSpritePacket ipuMovieSpritePacket;

typedef struct XglPacket XglPacket;


/* IPU movie test: plays yajima\\ipu_101.ipu from a 64 KB stream buffer
 * and draws each decoded frame as a textured sprite; circle rewinds. */
void IpuMovieTest(int selection) {
    XglMovieWork info;
    int frames = 0;
    XglPacket *packet;
    unsigned char *frame;
    int width;
    int height;
    int result;

    xglMovieInfoInit(&info);
    info.source = (XglMovieSourceHeader *)IPU_STREAM_BUFFER_BASE;
    info.sourceSize = 0x10000;
    xglMovieOpen(&info, "yajima\\ipu_101.ipu");
    info.imageAddress[0] = (unsigned char *)DEBUG_BUFFER_BASE;
    info.imageAddress[1] = (unsigned char *)(DEBUG_BUFFER_BASE + 0x200000);
    info.workBuffer = (unsigned char *)(DEBUG_BUFFER_BASE + 0x400000);
    printf("%dx%d\n", info.width, info.height);
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        if (PadData.ports[0].pressed & PAD_CIRCLE) {
            xglMovieRewind(&info);
        }
        /* JP: ＩＰＵ
         * EN: IPU */
        xglFontDebugPrintf(0, 0, "\x0B\xA3\xC9\xA3\xD0\xA3\xD5");
        result = xglMoviePlay(&info);
        xglFontDebugPrintf(0, 0x20, "%8x %1x %8x", info.currentFrame, result, frames);
        frames++;
        packet = xglPacketGetCurrent();
        frame = info.imageAddress[info.bufferIndex ^ 1];
        width = info.width << 4;
        height = info.height << 4;
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, ipuMovieSetupPacket, 0x18);
        sceVif1PkRef(packet, ipuMovieImageHeaderPacket, 2, 0, 0, 0);
        sceVif1PkRef(packet, frame, 0x7000, 0, 0x51007000, 0);
        sceVif1PkRef(packet, ipuMovieImageHeaderPacket, 2, 0, 0, 0);
        sceVif1PkRef(packet, frame + 0x70000, 0x7000, 0, 0x51007000, 0);
        sceVif1PkRef(packet, ipuMovieTransferEndPacket, 3, 0, 0, 0);
        sceVif1PkCnt(packet, 0);
        sceVif1PkOpenDirectHLCode(packet, 0);
        sceVif1PkAddDirectDataN(packet, ipuMovieDrawStatePacket, 4);
        ipuMovieSpritePacket.tex0 = 0x664123800ULL;
        ipuMovieSpritePacket.x0 = 0x73F8;
        ipuMovieSpritePacket.y0 = 0x80F7;
        ipuMovieSpritePacket.x1 = ipuMovieSpritePacket.x0 + width;
        ipuMovieSpritePacket.y1 = ipuMovieSpritePacket.y0 + height;
        ipuMovieSpritePacket.u1 = width;
        ipuMovieSpritePacket.v1 = height;
        sceVif1PkAddDirectDataN(packet, &ipuMovieSpritePacket, 7);
        sceVif1PkCloseDirectHLCode(packet);
        xglSleep();
    }
}

/* "yajima\\check.txt". */
/* JP: 漢字チェック (EN: kanji check) title. */

/* Kanji check: lists every distinct two-byte character of check.txt, 16
 * per line, and shows the list; circle prints it. */
void KanjiCheckTest(int selection) {
    unsigned char *text;
    unsigned char *list = (unsigned char *)DEBUG_BUFFER_BASE;
    int count = 0;
    unsigned char *out = list;
    unsigned char *scan;
    unsigned char high;
    unsigned char low;

    xglCdReadFile("yajima\\check.txt", (void *)(DEBUG_BUFFER_BASE + 0x800), 0, CD_NOTIFY_CURRENT);
    text = (unsigned char *)(DEBUG_BUFFER_BASE + 0x800);
    while (*text != 0) {
        if ((char)*text >= 0) {
            text++;
        } else if (*text < 0xD0) {
            text += 2;
        } else {
            high = *text++;
            low = *text++;
            scan = list;
            while (scan < out) {
                if (*scan == '\n') {
                    scan++;
                } else if (scan[0] == high && scan[1] == low) {
                    scan = 0;
                    break;
                } else {
                    scan += 2;
                }
            }
            if (scan != 0) {
                count++;
                out[0] = high;
                out[1] = low;
                out += 2;
                if ((count & 0xF) == 0) {
                    *out++ = '\n';
                }
            }
        }
    }
    out[0] = '\n';
    out[1] = 0;
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        /* JP: 第二水準チェック
         * EN: JIS level-2 kanji check */
        xglFontDebugPrintf(0, 0, "\x0B\xC2\xE8\xC6\xF3\xBF\xE5\xBD\xE0\xA5\xC1\xA5\xA7\xA5\xC3\xA5\xAF");
        xglFontDebugPrintf(16, 16, (const char *)list);
        if (PadData.ports[0].pressed & PAD_CIRCLE) {
            printf((const char *)list);
        }
        xglSleep();
    }
}

/* Partial view of a point: the VU0 block reads only its X and Z components. */
typedef struct PointXZ {
    int x;
    int z;
} PointXZ;

/* The debug line packet: three tag quadwords, RGBAQ as four words, then
 * the two XYZ2 vertices as packed words; 0x18 words in all. */
typedef struct DebugLinePacket {
    unsigned char tags[0x30];
    int red;
    int green;
    int blue;
    int alpha;
    int x0;
    int y0;
    unsigned char unmodeled_48[0x50 - 0x48];
    int x1;
    int y1;
    unsigned char unmodeled_58[0x60 - 0x58];
} DebugLinePacket;

extern DebugLinePacket edgeTestLinePacket;

/* Draws a line between two points of the top-down view in the colour
 * 0xBBGGRR; the GS origin offsets place the view on screen. */
void EdgeTestLineDraw(PointXZ *from, PointXZ *to, unsigned int colour) {
    int originX = 0x7000;
    int originY = 0x7200;
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    edgeTestLinePacket.red = colour & 0xFF;
    edgeTestLinePacket.green = (colour >> 8) & 0xFF;
    edgeTestLinePacket.blue = (colour >> 16) & 0xFF;
    edgeTestLinePacket.x0 = from->x + originX;
    edgeTestLinePacket.y0 = from->z + originY;
    edgeTestLinePacket.x1 = to->x + originX;
    edgeTestLinePacket.y1 = to->z + originY;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &edgeTestLinePacket, 0x18);
}

/* Returns 1 when the VU0 orientation value is negative, 0 otherwise. */
int EdgeTestOrientationSign(PointXZ *origin, PointXZ *point1, PointXZ *point2, PointXZ *point3)
{
    float origin_vec[4];
    float point1_vec[4];
    float point2_vec[4];
    float point3_vec[4];
    float orientation;

    origin_vec[0] = I2F(origin->x);
    origin_vec[2] = I2F(origin->z);
    point1_vec[0] = I2F(point1->x);
    point1_vec[2] = I2F(point1->z);
    point2_vec[0] = I2F(point2->x);
    point2_vec[2] = I2F(point2->z);
    point3_vec[0] = I2F(point3->x);
    point3_vec[2] = I2F(point3->z);
    __asm__ __volatile__(
        "lqc2 vf10, 0(%1)\n\t"
        "lqc2 vf4, 0(%2)\n\t"
        "lqc2 vf5, 0(%3)\n\t"
        "lqc2 vf6, 0(%4)\n\t"
        "vsub.xz vf1, vf4, vf10\n\t"
        "vsub.xz vf2, vf5, vf10\n\t"
        "vsub.xz vf3, vf6, vf10\n\t"
        "vmul.xz vf7, vf1, vf1\n\t"
        "vmul.xz vf8, vf2, vf2\n\t"
        "vmul.xz vf9, vf3, vf3\n\t"
        "vaddz.x vf4, vf7, vf7z\n\t"
        "vaddz.x vf5, vf8, vf8z\n\t"
        "vaddz.x vf6, vf9, vf9z\n\t"
        "vrsqrt Q, vf0w, vf4x\n\t"
        "vwaitq\n\t"
        "vaddq.x vf4, vf0, Q\n\t"
        "vmulz.x vf7, vf1, vf2z\n\t"
        "vmulz.x vf8, vf2, vf1z\n\t"
        "vmulz.x vf9, vf1, vf3z\n\t"
        "vmulz.x vf10, vf3, vf1z\n\t"
        "vsub.x vf2, vf7, vf8\n\t"
        "vsub.x vf3, vf9, vf10\n\t"
        "vrsqrt Q, vf2x, vf5x\n\t"
        "vwaitq\n\t"
        "vmulq.x vf1, vf4, Q\n\t"
        "vrsqrt Q, vf3x, vf6x\n\t"
        "vwaitq\n\t"
        "vmulq.x vf2, vf4, Q\n\t"
        "vmul.x vf3, vf1, vf2\n\t"
        "qmfc2.ni %0, vf3"
        : "=r"(orientation)
        : "r"(origin_vec), "r"(&point1_vec[0]), "r"(&point2_vec[0]), "r"(&point3_vec[0])
        : "memory");
    /* Written as !(x >= 0) in the original: the compare is c.le.s, not c.lt.s. */
    if (!(!(orientation >= 0.0f))) {
        return 1;
    }
    return 0;
}

/* "(%3d,%3d)-(%3d,%3d)", " %3d,%3d", "%8x" and "%f". */
extern const char edgeSegmentFormat[];

const char edgeSegmentFormat[24] = "(%3d,%3d)-(%3d,%3d)";

/* Segment intersection test: the left and right sticks of pad 2 move the
 * ends of the first segment, those of pad 1 the second; circle squares them
 * up. A crossing is marked with a red cross (state 2 when it lies outside
 * the first segment). */
void EdgeTest(int selection) {
    PointXZ start1;
    PointXZ end1;
    PointXZ start2;
    PointXZ end2;
    PointXZ crossing;
    PointXZ to;
    PointXZ from;
    int state;
    McTestPadPort *pads;
    float t;

    start1.x = 0x800;
    start1.z = 0x800;
    end1.x = 0x1800;
    end1.z = 0x800;
    start2.x = 0x1000;
    start2.z = 0x400;
    end2.x = 0x1000;
    end2.z = 0x1000;
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        pads = PadData.ports;
        start1.x += pads[1].sticksCentered[2];
        start1.z += pads[1].sticksCentered[3];
        end1.x += pads[1].sticksCentered[0];
        end1.z += pads[1].sticksCentered[1];
        start2.x += pads[0].sticksCentered[2];
        start2.z += pads[0].sticksCentered[3];
        end2.x += pads[0].sticksCentered[0];
        end2.z += pads[0].sticksCentered[1];
        if (pads[0].pressed & PAD_CIRCLE) {
            start1.z = end1.z;
            start2.x = end2.x;
        }
        state = 0;
        /* At most one pass: the body always ends in break. Written as an
         * if, the compiler allocates the registers differently. */
        while (!EdgeTestOrientationSign(&start1, &end1, &start2, &end2)
               && !EdgeTestOrientationSign(&start2, &end2, &start1, &end1)) {
            state = 1;
            to.x = end1.x - start1.x;
            to.z = end1.z - start1.z;
            from.x = start2.x - start1.x;
            from.z = start2.z - start1.z;
            t = I2F(from.x * to.x + from.z * to.z) / I2F(to.x * to.x + to.z * to.z);
            from.x = (float)to.x * t - (float)from.x;
            from.z = (float)to.z * t - (float)from.z;
            if (t < 0.0f || t > 1.0f) {
                state = 2;
            }
            to.x = end2.x - start2.x;
            to.z = end2.z - start2.z;
            t = I2F(from.x * from.x + from.z * from.z) / I2F(to.x * from.x + to.z * from.z);
            xglFontDebugPrintf(0, 0x20, "%f", t);
            crossing.x = (float)start2.x + (float)to.x * t;
            crossing.z = (float)start2.z + (float)to.z * t;
            break;
        }
        if (state) {
            from.x = crossing.x;
            from.z = crossing.z - 0x80;
            to.x = crossing.x;
            to.z = crossing.z + 0x80;
            EdgeTestLineDraw(&from, &to, 0xC00000);
            from.x = crossing.x - 0x80;
            from.z = crossing.z;
            to.x = crossing.x + 0x80;
            to.z = crossing.z;
            EdgeTestLineDraw(&from, &to, 0xC00000);
        }
        EdgeTestLineDraw(&start1, &end1, 0xC0C0C0);
        EdgeTestLineDraw(&start2, &end2, 0xC0);
        xglFontDebugPrintf(0, 0, edgeSegmentFormat, start1.x >> 4, start1.z >> 4, end1.x >> 4, end1.z >> 4);
        xglFontDebugPrintf(0, 8, edgeSegmentFormat, start2.x >> 4, start2.z >> 4, end2.x >> 4, end2.z >> 4);
        /* crossing is shown every frame: it keeps the last crossing found
         * and is not set before the first one (the original reads the
         * stack slot the same way). */
        xglFontDebugPrintf(0, 0x10, " %3d,%3d", crossing.x >> 4, crossing.z >> 4);
        xglFontDebugPrintf(0, 0x18, yajimaHexDigitFormat, state);
        xglFontDebugPrintf(0, 0x40, yajimaHexWordFormat,
                           (end1.x - start1.x) * (end2.x - start2.x) + (end1.z - start1.z) * (end2.z - start2.z));
        xglSleep();
    }
}

const char resourceLabel[16] = "RESOURCE";
const char edgeLabel[8] = "EDGE";
const char nisuiLabel[8] = "NISUI";
const char captionLabel[8] = "CAPTION";
const char soundLabel[8] = "SOUND";
const char ipuLabel[8] = "IPU";
const char memcardLabel[8] = "MEMCARD";
const char hddLabel[8] = "HDD";
const char padLabel[8] = "PAD";
const char fontLabel[8] = "FONT";
/* JP: (codes 0x80-0xAF) 葦桶患況弦沙悉梢 / 請唾津毒班碧麺力 / 佰厶壞嶐慵无槿渤 / 珀矣粤肄蓍裨跋鈿 / 韵鴦 (codes 0xF4-0xFF)
 * EN: Font test: the one-byte codes 0x80-0xAF, sample kanji of JIS levels 1 and 2, then the codes 0xF4-0xFF */
char fontTestGlyphText[136] = "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F\n\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F\n\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF\n\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF\n\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF\n\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF\n\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF\n\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF";
char soundTestModeMenuText[24] = "SE\nBGM\nADX\nVOL\n";
char soundTestVolumeMenuText[24] = "MASTER\nBGM\nSE\nADX\n";
const char *memcardPortNames[6] = { port1Label, port2Label, hddText, 0, 0, 0 };
unsigned long long ipuMovieSetupPacket[12] = {
    0, 0x5100000500000000ULL, 0x1000000000000004ULL, 0xEULL,
    0x8380000000000ULL, 0x50ULL, 0, 0x51ULL,
    0x1C000000200ULL, 0x52ULL, 0, 0x53ULL
};
unsigned long long ipuMovieImageHeaderPacket[4] = {
    0, 0x5100000100000000ULL, 0x800000000007000ULL, 0
};
unsigned long long ipuMovieTransferEndPacket[6] = {
    0, 0x5100000200000000ULL, 0x1000000000008001ULL, 0xEULL,
    0, 0x3FULL
};
unsigned long long ipuMovieDrawStatePacket[8] = {
    0x1000000000008003ULL, 0xEULL, 0x30000ULL, 0x47ULL,
    0, 0x14ULL, 0, 0x3FULL
};
IpuSpritePacket ipuMovieSpritePacket = {
    { 0x01, 0x80, 0x00, 0x00, 0x00, 0x40, 0x8B, 0x60, 0x1E, 0x53, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00 },
    0,
    { 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    0, 0,
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    0, 0,
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    0, 0,
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};
DebugLinePacket edgeTestLinePacket = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x51, 0x01, 0x80, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x40, 0x1E, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    192, 192, 192, 128,
    28672, 29184,
    { 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 },
    36864, 36352,
    { 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 }
};
#define NULL ((void *) 0)


/* Calls tyaDrawGauge with a null gauge, resets the elevator records (elvReset),
 * then runs tyaDisplaySetting, the display adjustment screen, into the
 * framebuffer at 0x01000000. */
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

    PadData.ports[0].horizontalDeadZone = 0x40;
    PadData.ports[0].verticalDeadZone = 0x40;
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


/* Sets the level of ADX slot index to 0x5A at once. Byte-identical to actionSoundSlotLevelSet. */
void bgmSlotLevelSet(int index) {
    soundSlotSetVolume(index, 0x5A, 0);
}

/* Sets the level of ADX slot index to 0x5A at once; same body as bgmSlotLevelSet. */
void actionSoundSlotLevelSet(int index) {
    soundSlotSetVolume(index, 0x5A, 0);
}

/* Sets the level of ADX slot index to 0x7A at once; same body as streamSetLevel. */
void streamKeySlotLevelSet(int index) {
    soundSlotSetVolume(index, 0x7A, 0);
}

/* Sets the level of ADX slot index to 0x7A at once (stream play). Byte-identical to streamKeySlotLevelSet. */
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
