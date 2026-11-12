#include "common.h"
#include "main/pad.h"
#include "main/tu012.h"

/* One port of the pad record PadData, the full 0x80 bytes (xglPadRead
 * scales the port index by 128): the raw pad buffer, the decoded buttons
 * and sticks and the actuator settings. pad.h's PadRecord cannot serve here:
 * it models only the first 0x68 bytes, so neither the port stride the pad
 * test steps by nor the status and actuator members it shows. The members both
 * records name carry pad.h's names at the same offsets. */
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
    unsigned char unmodeled_48[0x50 - 0x48];
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
 * records here instead of sharing a union inside the port as in pad.h:
 * with the inner union the compiler schedules func_001ED568's read of
 * pads[0].pressed differently from the original. */
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

void xglFontDebugPrintf(int x, int y, const char *format, ...);
unsigned char xglFontLoad(int font, int mode);
void GameSnapShotCheck(void);
void xglSleep(void);

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

INCLUDE_ASM("asm/main/nonmatchings/tu060", DebugTextViewUpdate);

extern const char D_006886B0[];
void xglFontDebugHex(int x, int y, int value, int digits);

/* Pad test page: the right/left and down/up repeats set the small and big
 * actuator levels (applied while circle is held), cross toggles the actuator
 * mode; both port records are dumped in hex until SELECT+START. */
void func_001E9B28(void) {
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
        xglFontDebugPrintf(0, 0, D_006886B0, small, big, actuatorMode);
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
extern const char D_006886C0[];
extern const char D_006886E0[];
extern const char D_006886F0[];
extern const char D_00688730[];
extern const char D_00688770[];
/* The half-width katakana table "$prop1;%s" prints. */
extern char D_002BD130[];

void FontTestP1(void) {
    if (PadData.ports[0].pressed & PAD_CIRCLE) {
        xglFontLoad(0, 0);
    }
    if (PadData.ports[0].pressed & PAD_CROSS) {
        xglFontLoad(1, 0);
    }
    xglFontDebugPrintf(16, 16, D_006886C0);
    xglFontDebugPrintf(16, 32, D_006886E0, D_002BD130);
    xglFontDebugPrintf(16, 144, D_006886F0);
    xglFontDebugPrintf(16, 160, D_00688730);
    xglFontDebugPrintf(16, 176, D_00688770, 0xAE);
}

/* Font test: left and right would turn pages, but only page 0 is left. */
void FontTest(void) {
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

INCLUDE_ASM("asm/main/nonmatchings/tu060", SoundTest);

/* Empty callback: nothing to do. */
void func_001EAB08(void) {

}

/* Port names: memory card slots 1 and 2, then the HDD. */
extern const char *D_002BD1E8[];

const char *func_001EAB10(int port) {
    return D_002BD1E8[port];
}

int printf(const char *, ...);
extern const char D_00688900[];

/* Enumerate callback: logs the result it is called with ("callback:%d");
 * always returns -1. */
int DebugCallbackLog(int result) {
    printf(D_00688900, result);
    return -1;
}

/* Reports of the old-data check: "ep1  :" and "ep1re:" lines carry the seven
 * bytes of an Episode I save entry, "freak:" its first byte. */
extern const char D_00688910[];
extern const char D_00688938[];
extern const char D_00688960[];
extern const char D_0069D060[];
extern const char D_0069D068[];

/* Callback of the old-data check (MC_REQ_CHECK_OLD): logs each result; the
 * entry bytes are read only for the entry results 0 to 2. Always returns 0. */
int func_001EAB50(int result, unsigned char *entry) {
    switch (result) {
    case 0:
        printf(D_00688910, entry[0], entry[1], entry[2], entry[3], entry[4], entry[5], entry[6]);
        break;
    case 1:
        printf(D_00688938, entry[0], entry[1], entry[2], entry[3], entry[4], entry[5], entry[6]);
        break;
    case 2:
        printf(D_00688960, entry[0]);
        break;
    case -2:
        printf(D_0069D060);
        break;
    case -3:
        printf(D_0069D068);
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
extern const char D_00688988[];
extern const char D_00688998[];

float I2F(int value);

/* Reports the bounding box of an icon model (positions are 1/1024 fixed
 * point) and the zoom that keeps its height within 20 units. */
void func_001EAC18(void *icon, float *zoom) {
    McIconModel *model = icon;
    McIconPosition *position = model->positions;
    int shapeCount = model->shapeCount;
    int vertexCount = model->vertexCount;
    int vertex;
    int shape;
    float value;
    float minX, maxX, minY, maxY, minZ, maxZ;

    printf(D_00688988, model, shapeCount, vertexCount);
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
    printf(D_00688998, minX, maxX, minY, maxY, minZ, maxZ);
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

extern const char D_0069D070[];
extern const char D_0069D078[];
extern const char D_0069D080[];
extern const char D_006889B8[];
extern const char D_006889C8[];

int xglCdReadFile(const char *name, void *buffer, int mode, int flags);

/* Loads a card icon image and reports its three icons. The smallest zoom
 * is tracked but never shown: the delete icon's zoom is what gets printed. */
void McFileSectionsReport(const char *name, void *image) {
    McIconImageHeader *header = image;
    char *icons;
    int offset;
    float iconZoom;
    float minZoom = 1.0f;

    printf(D_0069D070, name);
    xglCdReadFile(name, image, 0, 0);
    offset = MC_ICON_ALIGN(header->headerSize);
    icons = (char *)image + offset;
    printf(D_0069D078);
    offset = MC_ICON_ALIGN(header->sysOffset);
    func_001EAC18(icons + offset, &iconZoom);
    if (iconZoom < minZoom) {
        minZoom = iconZoom;
    }
    printf(D_0069D080);
    offset = MC_ICON_ALIGN(header->copyOffset);
    func_001EAC18(icons + offset, &iconZoom);
    if (iconZoom < minZoom) {
        minZoom = iconZoom;
    }
    printf(D_006889B8);
    offset = MC_ICON_ALIGN(header->deleteOffset);
    func_001EAC18(icons + offset, &iconZoom);
    if (iconZoom < minZoom) {
        minZoom = iconZoom;
    }
    printf(D_006889C8, iconZoom);
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

/* The header of a card data image: the begin and end of the payload. */
struct HddTransferInfo {
    int unmodeled_00;
    int begin;
    int end;
};

struct HddCheckState {
    int status;
    void *transfer;
    int dataSize;
};

struct HddExistRequest {
    int card;
    unsigned char *found;
};

struct HddLoadRequest {
    int card;
    void *data;
    int size;
};

struct HddSaveRequest {
    int card;
    const void *data;
    int size;
};

/* The thumbnail image MemcardTest copies into the save. */
typedef struct McThumbnail {
    unsigned char bytes[0x1000];
} McThumbnail;

/* The save image is the memory from SaveData on: card file 0 holds its
 * 0x50-byte header, the other files the rest from D_0063F890. */
#define SAVE_DATA_SIZE 0x5160
#define SAVE_HEADER_SIZE 0x50
/* The two save-image settings this TU reads: the Dolby Digital flag that
 * MovieListTest toggles and the caption mode it forces. */
typedef struct McTestSaveData {
    unsigned char unmodeled_00[0x29];
    unsigned char dolby_digital; /* +0x29: DOLBY DIGITAL ON/OFF */
    unsigned char unmodeled_2a[0x3A - 0x2A];
    /* +0x3A: nonzero shows every caption line, not only the ones starting
     * with a space (game_movie.c); 2 is the save default. */
    unsigned char caption_all;
    unsigned char unmodeled_3b[SAVE_DATA_SIZE - 0x3B];
} McTestSaveData;
extern McTestSaveData SaveData;
extern unsigned char D_0063F890[];
/* The event flags the triangle button randomizes. */
extern unsigned char D_0063F8B4[];
/* The thumbnail saved with the game. */
extern McThumbnail D_0063F9B4;

/* Strings and a table MemcardTest shares with functions that are still
 * assembly: ">" is the menu cursor (X1's menu_cursor, first emitted by
 * SoundTest), "%2x" is shared with func_001EC8E8, and "%1x", "%2x %2x" and
 * the HDD menu table sit in the next scaffold piece of .sdata. */
extern char D_0069D028[];
extern const char D_0069D090[];
extern const char D_0069D0D0[];
extern const char D_0069D0D8[];
/* HDD menu row -> state: MCTEST_STATE_HDD_LOAD_LIST, MCTEST_STATE_HDD_SAVE_INIT
 * (X1 names this static tbl_49). */
extern unsigned char tbl_49[4];

int xglMcEasySave(const char *path, const void *buffer, int size);
void xglMcSetEntryNumbers(unsigned char *object, int value, const unsigned char *source);
int xglHddCheck(void);
int xglHddMcExist(struct HddExistRequest *request);
int xglHddMcLoad(void *save);
int xglHddMcSave(const struct HddSaveRequest *save);
int xglHddMcCreate(struct HddCheckState *state);
int sceMcSync(int mode, int *command, int *result);
unsigned short xglSRand(void);
void FlushCache(int mode);

void MemcardTest(void) {
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
    xglCdReadFile("yajima\\thumb.jpg", (void *)DEBUG_BUFFER_BASE, 0, 0);
    D_0063F9B4 = *(McThumbnail *)DEBUG_BUFFER_BASE;
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
        xglFontDebugPrintf(0, 16, D_0069D090, state);

        switch (state) {
        case MCTEST_STATE_INIT:
            state = MCTEST_STATE_SELECT_PORT;
            hdd = xglHddCheck();
            /* fall through */
        case MCTEST_STATE_SELECT_PORT:
            xglFontDebugPrintf(0, 32, MC_TEXT_SELECT_PORT);
            name = func_001EAB10(0);
            if (port_status[0] == MC_PORT_READY) {
                xglFontDebugPrintf(16, 48, "%s", name);
            } else {
                /* Text: {color #202020}%s */
                xglFontDebugPrintf(16, 48, "\x0C   %s", name);
            }
            name = func_001EAB10(1);
            if (port_status[1] == MC_PORT_READY) {
                xglFontDebugPrintf(16, 64, "%s", name);
            } else {
                xglFontDebugPrintf(16, 64, "\x0C   %s", name);
            }
            name = func_001EAB10(2);
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
            xglFontDebugPrintf(8, work.result * 16 + 48, D_0069D028);
            break;

        case MCTEST_STATE_MENU_INIT:
            work.result = 0;
            state = MCTEST_STATE_MENU;
            /* fall through */
        case MCTEST_STATE_MENU:
            xglFontDebugPrintf(0, 32, "%s", func_001EAB10(port));
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
            xglFontDebugPrintf(8, work.result * 16 + 48, D_0069D028);
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
                                     (const unsigned char *)MC_ENTRY_PLACE);
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
                save_request.data = D_0063F890;
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
                xglFontDebugPrintf((i % 10) * 8 + 128, (i / 10) * 8 + 48, D_0069D0D0, found[i]);
            }
            xglFontDebugPrintf(128, 128, D_0069D0D8, found[100], found[101]);
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
            check_old_request.callback = func_001EAB50;
            state = MCTEST_STATE_CHECKING_OLD;
            xglMcRequest(port, MC_REQ_CHECK_OLD, &check_old_request, &work.result);
            break;

        case MCTEST_STATE_CHECKING_OLD:
        case MCTEST_STATE_CHECK_OLD_DONE:
            for (i = 1; i < MC_SAVE_FILE_COUNT; i++) {
                xglFontDebugPrintf((i % 10) * 8 + 128, (i / 10) * 8 + 48, D_0069D0D0, found[i]);
            }
            xglFontDebugPrintf(128, 128, D_0069D0D8, found[100], found[101]);
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
            xglFontDebugPrintf(0, 32, "%s", func_001EAB10(port));
            xglFontDebugPrintf(16, 48, MC_TEXT_LOAD);
            xglFontDebugPrintf(16, 64, MC_TEXT_SAVE);
            hdd_menu_step = (PadData.ports[0].pressed & PAD_UP) ? -1 : 0;
            if (PadData.ports[0].pressed & PAD_DOWN) {
                hdd_menu_step = 1;
            }
            if (hdd_menu_step != 0) {
                work.result = (work.result + hdd_menu_step + MC_HDD_MENU_ITEM_COUNT) % MC_HDD_MENU_ITEM_COUNT;
            }
            xglFontDebugPrintf(8, work.result * 16 + 48, D_0069D028);
            if (PadData.ports[0].pressed & PAD_CIRCLE) {
                state = tbl_49[work.result];
            }
            if (PadData.ports[0].pressed & PAD_CROSS) {
                state = MCTEST_STATE_INIT;
            }
            break;

        case MCTEST_STATE_HDD_LOAD_LIST:
            hdd_list_request.card = -1;
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
                    hdd_load_request.card = work.file;
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

                hdd_check_request.card = work.file;
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
                hdd_create_request.status = work.file;
                hdd_create_request.transfer = files;
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
            hdd_save_request.card = work.file;
            hdd_save_request.data = &SaveData;
            hdd_save_request.size = SAVE_DATA_SIZE;
            printf("xglHddMcSave():%d\n", xglHddMcSave(&hdd_save_request));
            state = MCTEST_STATE_HDD_MENU_INIT;
            break;
        }

        if (PadData.ports[0].pressed & PAD_TRIANGLE) {
            flags = D_0063F8B4;
            for (i = 0; i < SAVE_FLAGS_COUNT; i++) {
                *flags++ = xglSRand();
            }
            FlushCache(0);
        }
        shown_flags = D_0063F8B4;
        xglFontDebugPrintf(16, 180, "%2x %2x %2x %2x %2x %2x %2x %2x", shown_flags[0], shown_flags[1],
                           shown_flags[2], shown_flags[3], shown_flags[4], shown_flags[5], shown_flags[6],
                           shown_flags[7]);
        xglSleep();
    }
}

INCLUDE_ASM("asm/main/nonmatchings/tu060", CdSlotTableView);

INCLUDE_ASM("asm/main/nonmatchings/tu060", func_001EC8E8);

/* JP: リソーステスト (EN: resource test), with "%d" for the view. */
extern const char D_00689378[];
/* The spinner character "*". */
extern const char D_0069D120[];
/* Frame counter of the spinner. */
extern unsigned char D_0069D118;

void resInitPathHeader(int key, int value);
void resPathSlotsInit(void);
void resSelectSlot(int search);
void func_001E1B10(void);
void CdSlotTableView(void);
void func_001EC8E8(void);

/* CD slot test: shows the slot table or the second view (SELECT toggles),
 * START reloads the path table. */
void CdSlotTest(void) {
    int view = 0;

    resInitPathHeader(DEBUG_BUFFER_BASE, DEBUG_BUFFER_BASE);
    resPathSlotsInit();
    resSelectSlot(1);
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        xglFontDebugPrintf(0, 0, D_00689378, view);
        D_0069D118++;
        xglFontDebugPrintf(((D_0069D118 * 4) & 0x38) + 0x50, 0, D_0069D120);
        if (PadData.ports[0].pressed & PAD_START) {
            func_001E1B10();
        }
        if (PadData.ports[0].pressed & PAD_SELECT) {
            view ^= 1;
        }
        switch (view) {
        case 0:
            CdSlotTableView();
            break;
        case 1:
            func_001EC8E8();
            break;
        }
        xglSleep();
    }
}

/* The movie list "host0:/home/xeno/vcd-f/x2u/movie/sfd.lst". */
extern const char D_00689390[];
/* JP: ムービーリスト (EN: movie list) title. */
extern const char D_006893C0[];
/* JP: ∥：ＤＯＬＢＹ ＤＩＧＩＴＡＬ (EN: dolby digital) "%s". */
extern const char D_006893D0[];
/* "VOL%3d". */
extern const char D_006893F0[];
/* "ON". */
extern const char D_0069D128[];
/* "OFF". */
extern const char D_0069D130[];
extern int D_002B5D98[];

/* Scratch memory of the tests. */
void *GameResBufferAlloc(int size);
extern int sceOpen(const char *path, int flags, ...);
extern int sceRead(int descriptor, void *buffer, int bytes);
extern int sceClose(int descriptor);
void *memset(void *destination, int value, unsigned int count);
int DebugTextViewUpdate(DebugTextView *view);
void func_001A4400(int flag);
void GameMovieCaptionPlay(const char *name);
void xglRenderClearFrame(void);
void xglRenderClearColor(unsigned int color);

/* Movie list test: shows sfd.lst as a scrolling text; square toggles the
 * Dolby Digital flag of the save data, L1 and R1 set the volume and circle
 * plays the selected movie (its name up to the '.'). */
void MovieListTest(void) {
    DebugTextView view;
    char name[0x40];
    int volume = 0x7F;
    char *list;
    int descriptor;
    char *cursor;
    char *out;

    resInitPathHeader(DEBUG_BUFFER_BASE, DEBUG_BUFFER_BASE);
    list = (char *)GameResBufferAlloc(0x4000);
    memset(list, 0, 0x4000);
    descriptor = sceOpen(D_00689390, 1);
    sceRead(descriptor, list, 0x4000);
    sceClose(descriptor);
    DebugTextViewInit(&view, list);
    SaveData.caption_all = 2; /* the save default: every caption line shown */
    D_002B5D98[0] = 0;
    func_001A4400(0);
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        xglFontDebugPrintf(0, 0, D_006893C0);
        if (PadData.ports[0].pressed & PAD_SQUARE) {
            SaveData.dolby_digital ^= 1;
        }
        xglFontDebugPrintf(0, 0xD0, D_006893D0, SaveData.dolby_digital ? D_0069D128 : D_0069D130);
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
        xglFontDebugPrintf(0xB0, 0xD0, D_006893F0, volume);
        if (DebugTextViewUpdate(&view)) {
            cursor = view.cursor;
            out = name;
            while (*cursor != '.') {
                *out++ = *cursor++;
            }
            *out = 0;
            GameMovieCaptionPlay(name);
            xglRenderClearFrame();
            xglRenderClearColor(0x80004000);
        }
        xglSleep();
    }
}

/* "yajima\\ipu_101.ipu". */
extern const char D_00689400[];
/* "%8x %1x %8x". */
extern const char D_00689418[];
/* "%dx%d\n". */
extern const char D_0069D138[];
/* JP: IPU title. */
extern const char D_0069D140[];

/* The IPU movie player state: the frame size at 0x40, the frame counter
 * at 0x46, the stream buffer at 0x90, the two decoded frame buffers at
 * 0xB0 with the index of the one being decoded at 0xC8, and the work
 * buffer at 0xCC. */
typedef struct XglMovieWork {
    unsigned char unmodeled_00[0x40];
    short width;
    short height;
    unsigned char unmodeled_44[0x46 - 0x44];
    short currentFrame;
    unsigned char unmodeled_48[0x90 - 0x48];
    unsigned char *source;
    int sourceSize;
    unsigned char unmodeled_98[0xB0 - 0x98];
    unsigned char *imageAddress[2];
    unsigned char unmodeled_B8[0xC8 - 0xB8];
    unsigned char bufferIndex;
    unsigned char unmodeled_C9[0xCC - 0xC9];
    unsigned char *workBuffer;
} XglMovieWork;

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

extern unsigned long long D_002BD200[12];
extern unsigned long long D_002BD260[4];
extern unsigned long long D_002BD280[6];
extern unsigned long long D_002BD2B0[8];
extern IpuSpritePacket D_002BD2F0;

void xglMovieInfoInit(XglMovieWork *info);
int xglMovieOpen(XglMovieWork *info, const char *name);
int xglMoviePlay(XglMovieWork *info);
int xglMovieRewind(XglMovieWork *info);

typedef struct XglPacket XglPacket;

XglPacket *xglPacketGetCurrent(void);
void sceVif1PkCnt(XglPacket *packet, int count);
void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);
void sceVif1PkRef(XglPacket *packet, const void *data, int count, int mode, int offset, int flags);
void sceVif1PkOpenDirectHLCode(XglPacket *packet, int count);
void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);
void sceVif1PkCloseDirectHLCode(XglPacket *packet);

/* IPU movie test: plays yajima\\ipu_101.ipu from a 64 KB stream buffer
 * and draws each decoded frame as a textured sprite; circle rewinds. */
void IpuMovieTest(void) {
    XglMovieWork info;
    int frames = 0;
    XglPacket *packet;
    unsigned char *frame;
    int width;
    int height;
    int result;

    xglMovieInfoInit(&info);
    info.source = (unsigned char *)IPU_STREAM_BUFFER_BASE;
    info.sourceSize = 0x10000;
    xglMovieOpen(&info, D_00689400);
    info.imageAddress[0] = (unsigned char *)DEBUG_BUFFER_BASE;
    info.imageAddress[1] = (unsigned char *)(DEBUG_BUFFER_BASE + 0x200000);
    info.workBuffer = (unsigned char *)(DEBUG_BUFFER_BASE + 0x400000);
    printf(D_0069D138, info.width, info.height);
    while ((PadData.port1.heldAndPressed & PAD_EXIT_COMBO) != PAD_EXIT_COMBO) {
        if (PadData.ports[0].pressed & PAD_CIRCLE) {
            xglMovieRewind(&info);
        }
        xglFontDebugPrintf(0, 0, D_0069D140);
        result = xglMoviePlay(&info);
        xglFontDebugPrintf(0, 0x20, D_00689418, info.currentFrame, result, frames);
        frames++;
        packet = xglPacketGetCurrent();
        frame = info.imageAddress[info.bufferIndex ^ 1];
        width = info.width << 4;
        height = info.height << 4;
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, D_002BD200, 0x18);
        sceVif1PkRef(packet, D_002BD260, 2, 0, 0, 0);
        sceVif1PkRef(packet, frame, 0x7000, 0, 0x51007000, 0);
        sceVif1PkRef(packet, D_002BD260, 2, 0, 0, 0);
        sceVif1PkRef(packet, frame + 0x70000, 0x7000, 0, 0x51007000, 0);
        sceVif1PkRef(packet, D_002BD280, 3, 0, 0, 0);
        sceVif1PkCnt(packet, 0);
        sceVif1PkOpenDirectHLCode(packet, 0);
        sceVif1PkAddDirectDataN(packet, D_002BD2B0, 4);
        D_002BD2F0.tex0 = 0x664123800ULL;
        D_002BD2F0.x0 = 0x73F8;
        D_002BD2F0.y0 = 0x80F7;
        D_002BD2F0.x1 = D_002BD2F0.x0 + width;
        D_002BD2F0.y1 = D_002BD2F0.y0 + height;
        D_002BD2F0.u1 = width;
        D_002BD2F0.v1 = height;
        sceVif1PkAddDirectDataN(packet, &D_002BD2F0, 7);
        sceVif1PkCloseDirectHLCode(packet);
        xglSleep();
    }
}

/* "yajima\\check.txt". */
extern const char D_00689428[];
/* JP: 漢字チェック (EN: kanji check) title. */
extern const char D_00689440[];

/* Kanji check: lists every distinct two-byte character of check.txt, 16
 * per line, and shows the list; circle prints it. */
void func_001ED1C0(void) {
    unsigned char *text;
    unsigned char *list = (unsigned char *)DEBUG_BUFFER_BASE;
    int count = 0;
    unsigned char *out = list;
    unsigned char *scan;
    unsigned char high;
    unsigned char low;

    xglCdReadFile(D_00689428, (void *)(DEBUG_BUFFER_BASE + 0x800), 0, 0);
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
        xglFontDebugPrintf(0, 0, D_00689440);
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

extern DebugLinePacket D_002BD360;

/* Draws a line between two points of the top-down view in the colour
 * 0xBBGGRR; the GS origin offsets place the view on screen. */
void func_001ED358(PointXZ *from, PointXZ *to, unsigned int colour) {
    int originX = 0x7000;
    int originY = 0x7200;
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    D_002BD360.red = colour & 0xFF;
    D_002BD360.green = (colour >> 8) & 0xFF;
    D_002BD360.blue = (colour >> 16) & 0xFF;
    D_002BD360.x0 = from->x + originX;
    D_002BD360.y0 = from->z + originY;
    D_002BD360.x1 = to->x + originX;
    D_002BD360.y1 = to->z + originY;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &D_002BD360, 0x18);
}

/* Returns 1 when the VU0 orientation value is negative, 0 otherwise. */
int func_001ED420(PointXZ *origin, PointXZ *point1, PointXZ *point2, PointXZ *point3)
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
extern const char D_00689458[];
extern const char D_00689470[];
extern const char D_0069D108[];
extern const char D_0069D148[];

/* Segment intersection test: the left and right sticks of pad 2 move the
 * ends of the first segment, those of pad 1 the second; circle squares them
 * up. A crossing is marked with a red cross (state 2 when it lies outside
 * the first segment). */
void func_001ED568(void) {
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
        while (!func_001ED420(&start1, &end1, &start2, &end2)
               && !func_001ED420(&start2, &end2, &start1, &end1)) {
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
            xglFontDebugPrintf(0, 0x20, D_0069D148, t);
            crossing.x = (float)start2.x + (float)to.x * t;
            crossing.z = (float)start2.z + (float)to.z * t;
            break;
        }
        if (state) {
            from.x = crossing.x;
            from.z = crossing.z - 0x80;
            to.x = crossing.x;
            to.z = crossing.z + 0x80;
            func_001ED358(&from, &to, 0xC00000);
            from.x = crossing.x - 0x80;
            from.z = crossing.z;
            to.x = crossing.x + 0x80;
            to.z = crossing.z;
            func_001ED358(&from, &to, 0xC00000);
        }
        func_001ED358(&start1, &end1, 0xC0C0C0);
        func_001ED358(&start2, &end2, 0xC0);
        xglFontDebugPrintf(0, 0, D_00689458, start1.x >> 4, start1.z >> 4, end1.x >> 4, end1.z >> 4);
        xglFontDebugPrintf(0, 8, D_00689458, start2.x >> 4, start2.z >> 4, end2.x >> 4, end2.z >> 4);
        /* crossing is shown every frame: it keeps the last crossing found
         * and is not set before the first one (the original reads the
         * stack slot the same way). */
        xglFontDebugPrintf(0, 0x10, D_00689470, crossing.x >> 4, crossing.z >> 4);
        xglFontDebugPrintf(0, 0x18, D_0069D0D0, state);
        xglFontDebugPrintf(0, 0x40, D_0069D108,
                           (end1.x - start1.x) * (end2.x - start2.x) + (end1.z - start1.z) * (end2.z - start2.z));
        xglSleep();
    }
}
