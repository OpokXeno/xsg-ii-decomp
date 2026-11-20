#include "common.h"
#include "main/xenoActor.h"
#include "main/debug_test.h"
#include "main/save_screen.h"
#include "main/text_marker.h"
#include "main/xgl_thread.h"
#include "main/text_window.h"
#include "main/xgl_render.h"
#include "main/xgl_camera.h"
#include "main/xgl_cd.h"
#include "main/pad.h"
#include "main/game_snap_shot.h"
#include "main/choice_menu.h"
#include "main/cursor_buffer.h"
#include "main/menu_scene.h"
#include "main/menu_draw.h"
#include "main/xgl_2.h"
#include "main/xgl_packet.h"
#include "shared.h"
#include "main/game_data.h"
#include "main/control_entry.h"
#include "main/xgl_font.h"
#include "ov01/choice_menu.h"
#include "main/xgl_studio.h"
#include "main/xgl_cd_arena.h"
#include "main/res_get_path.h"

/* Debug test menu (YosikawaTest): item count, arena size, clear colour and list layout. */
#define TEST_MENU_ITEM_COUNT 7
#define TEST_MENU_ARENA_BYTES 0x40000
#define TEST_MENU_ROW_X 40
#define TEST_MENU_ROW_Y 40
#define TEST_MENU_ROW_HEIGHT 20
#define TEST_MENU_CLEAR_R 16
#define TEST_MENU_CLEAR_G 16
#define TEST_MENU_CLEAR_B 48
#define TEST_MENU_CLEAR_A 128

/* Debug counter (debugCounterStepAndDraw): bit 0x2000 steps it up, 0x8000 steps it down, and the value is kept in 15 bits. */
#define DEBUG_COUNTER_UP 0x2000
#define DEBUG_COUNTER_DOWN 0x8000
#define DEBUG_COUNTER_MASK 0x7FFF

/* Choice menu test (choiceMenuTest): 19 random items in the main menu, 4 in the submenu, state machine values. */
#define CHOICE_TEST_ITEM_COUNT 19
#define CHOICE_TEST_SUB_ITEM_COUNT 4
#define CHOICE_TEST_VALUE_MASK 0x7F
enum ChoiceTestState {
    CHOICE_TEST_STATE_MAIN = 0,
    CHOICE_TEST_STATE_SUB = 1,
    CHOICE_TEST_STATE_DONE = -1
};

/*
 * The 0xD0-byte packet template at debugCounterPacket, sent whole by
 * sceVif1PkAddDataN (52 words). The debug counter writer fills the members
 * below; the rest of the template is never written by this TU.
 */
typedef struct DebugCounterPacket {
    unsigned char unmodeled_00[0x60];
    u64 tag;
    unsigned char unmodeled_68[0x28];
    u32 ctl_90;
    u32 ctl_94;
    unsigned char unmodeled_98[0x8];
    u32 reg_a0;
    u32 reg_a4;
    unsigned char unmodeled_a8[0x8];
    u32 reg_b0;
    u32 reg_b4;
    unsigned char unmodeled_b8[0x8];
    u32 reg_c0;
    u32 reg_c4;
    unsigned char unmodeled_c8[0x8];
} DebugCounterPacket;

/* The two 64-bit tags written per entry into the shared upload buffer. */
typedef struct TexUploadBuffer {
    unsigned char unmodeled_00[0x20];
    u64 tag_20;
    unsigned char unmodeled_28[0x18];
    u64 tag_40;
    unsigned char unmodeled_48[0x18];
} TexUploadBuffer;

extern TexUploadBuffer texUploadTagBuffer;
extern DebugCounterPacket debugCounterPacket;
extern int debugCounterValue;

static unsigned char *choiceTestItems[CHOICE_TEST_ITEM_COUNT + 1];
static unsigned char *choiceTestSubItems[6];
static u8 choiceTestFlags[24];
static short choiceTestValues[20];
/* The test menu's cursor: a ListCursor in a 0x18-byte .bss slot (the word after it is not accessed). */
typedef struct TestMenuCursor {
    ListCursor list;
    int unmodeled_14;
} TestMenuCursor;
static TestMenuCursor testMenuCursorBuffer[1];

typedef struct TestUiItemNames {
    const char *name[TEST_MENU_ITEM_COUNT];
} TestUiItemNames;

extern const char *const testUiItemNames[8];
extern const char testUiKanaSample[];
extern const char testUiFontMarkup[];
extern const char testUiTitle[];
extern char testTextEntryWindowName[];
extern char testTextEntryTestName[];
extern char testTextEntryThanksName[];
void TexturTest(void);
void choiceMenuTest(void);


const char *const testUiItemNames[8] = {
    testUiItemNameText + 0x00, testUiItemNameText + 0x10, testUiItemNameText + 0x20, testUiItemNameText + 0x38,
    testUiItemNameText + 0x48, testUiItemNameText + 0x58, testUiItemNameText + 0x68, 0
};

/* JP: あいうえお
 * EN: a i u e o (the first five kana) */
const char testUiKanaSample[16] = "\244\242\244\244\244\246\244\250\244\252";
const char testUiFontMarkup[16] = "$colc0c0c0;";
/* JP: 芳川テスト
 * EN: Yoshikawa test */
const char testUiTitle[24] = "\013\313\247\300\356\245\306\245\271\245\310";

void YosikawaTest(void) {
    TestUiItemNames items;
    unsigned char *entryNames[4];
    TextEntry *textEntry = 0;
    u32 i;
    ListCursor *cursor;
    AnalyzeStats block;

    items = *(TestUiItemNames *) testUiItemNames;
    entryNames[0] = testTextEntryWindowName;
    entryNames[1] = testTextEntryTestName;
    entryNames[2] = testTextEntryThanksName;
    entryNames[3] = 0;
    GameResourceInit((void *) 0x01000000, 0x01000000);
    ov01InitAll();
    xglRenderClearFrame();
    XenoActorPoolInitDefault();
    xglStudioMainCameraInit();
    xglSleep();
    ClearEnv.value.color_r = TEST_MENU_CLEAR_R;
    ClearEnv.value.color_g = TEST_MENU_CLEAR_G;
    ClearEnv.value.color_b = TEST_MENU_CLEAR_B;
    ClearEnv.value.color_a = TEST_MENU_CLEAR_A;
    xglRenderClearFrame();
    cursorBufferInit(&testMenuCursorBuffer[0].list, TEST_MENU_ITEM_COUNT, 10, 0);
    menuSceneReset();
    xglCdArenaOpen(0, TEST_MENU_ARENA_BYTES);
    textEntryPoolReset();
    choiceMenuResetAll();
    textWindowReset();
    analyzePanelClear();

    while ((PadData[0].pressed & PAD_CROSS) == 0) {
        cursor = &testMenuCursorBuffer[0].list;
        cursorBufferMoveBy(cursor, ((PadData[0].directionRepeated >> 14) & 1) - ((PadData[0].directionRepeated >> 12) & 1));
        if (PadData[0].pressed & PAD_CIRCLE) {
            switch (cursor->cursor) {
            case 0:
                xglCdArenaClose();
                menuSceneRunLoop();
                xglCdArenaOpen(0, TEST_MENU_ARENA_BYTES);
                break;
            case 1:
                xglCdArenaClose();
                menuSceneRunSplit();
                xglCdArenaOpen(0, TEST_MENU_ARENA_BYTES);
                break;
            case 2:
                xglCdArenaClose();
                if (PadData[0].held & PAD_R2) {
                    MenuFileMain(1);
                } else {
                    MenuFileMain(0);
                }
                xglCdArenaOpen(0, TEST_MENU_ARENA_BYTES);
                break;
            case 3:
                TexturTest();
                break;
            case 4:
                if (textEntry == 0) {
                    textEntry = textEntryAlloc(200, 300, 200, 88, 7, entryNames);
                }
                break;
            case 5:
                choiceMenuTest();
                break;
            case 6:
                block.hp = 123456;
                block.maxHp = 123456;
                block.ep = 12;
                block.maxEp = 12;
                block.exp = 123456;
                block.skillPoints = 1234;
                block.classPoints = 1234;
                block.attackType = xglSRand() & 3;
                block.attributes = xglSRand();
                block.rareItem = 5;
                block.item = 8;
                block.rareItemKind = 2;
                block.itemKind = 1;
                analyzePanelStart(100, 150, &block);
                break;
            }
            ClearEnv.value.color_r = TEST_MENU_CLEAR_R;
            ClearEnv.value.color_g = TEST_MENU_CLEAR_G;
            ClearEnv.value.color_b = TEST_MENU_CLEAR_B;
            ClearEnv.value.color_a = TEST_MENU_CLEAR_A;
            gameWork.control &= 0xEFFFFFFF;
        }
        if (PadData[0].pressed & PAD_SELECT) {
            textWindowShow(32, 400, 0, testUiKanaSample, 60);
        }
        if (textEntry != 0) {
            if (textEntryConfirmPoll(textEntry)) {
                textEntryRemove(textEntry);
                textEntry = 0;
            }
        }
        textUpdate();
        XenoActorDoit();
        XenoActorDisp();
        xglFontPrint(0, 0, 0xFFFFF0, testUiFontMarkup);
        for (i = 0; i < testMenuCursorBuffer[0].list.capacity; i++) {
            textDrawSegment(TEST_MENU_ROW_X, TEST_MENU_ROW_Y + i * TEST_MENU_ROW_HEIGHT, items.name[i], i == testMenuCursorBuffer[0].list.cursor);
        }
        xglFontDebugPrintf(0, 0, testUiTitle);
        xglSleep();
    }
    xglCdArenaClose();
}

extern void FlushCache(int mode);

void xglTexUploadList(XglTextureTransSet *list) {
    XglPacket *packet = xglPacketGetCurrent();
    XglTextureTransEntry *entry;
    int i = 0;

    entry = (XglTextureTransEntry *)((u8 *)list + list->entryOffset);
    if (list->count > 0) {
        do {
            u32 header = entry->size;
            u32 low = header & 0xFFFF;
            int blocks;

            if (header == low) {
                blocks = ((int)low + 63) >> 6;
            } else {
                blocks = header >> 16;
            }
            i += 1;
            texUploadTagBuffer.tag_40 = ((u64)entry->height << 32) | low;
            texUploadTagBuffer.tag_20 = ((u64)((u32)(entry->vramOffset + 0xF0000) >> 6) << 32) | ((u64)blocks << 48);
            FlushCache(0);
            sceVif1PkRef(packet, &texUploadTagBuffer, 6, 0, 0, 0);
            sceVif1PkRef(packet, (u8 *)list + entry->dataOffset, entry->dataSize, 0, 0, 0);
            entry++;
        } while (i < list->count);
    }
}

/* Format of the debug counter text ("%8d"). */
extern char debugCounterFormat[];

void debugCounterStepAndDraw(void) {
    XglPacket *packet = xglPacketGetCurrent();
    int counter;
    unsigned int stepFlags;

    stepFlags = PadData[0].repeated;
    counter = debugCounterValue;
    debugCounterPacket.tag = ((u64)(counter + 0x3C00) << 37) | 0x2000000621343C00ULL;
    debugCounterPacket.ctl_90 = 0;
    debugCounterPacket.ctl_94 = 0;
    debugCounterPacket.reg_b0 = 0x2000;
    debugCounterPacket.reg_b4 = 0x2000;
    debugCounterPacket.reg_a0 = 0x6FF8;
    debugCounterPacket.reg_a4 = 0x71F7;
    debugCounterPacket.reg_c0 = 0x8FF8;
    debugCounterPacket.reg_c4 = 0x8DF7;
    if (stepFlags & DEBUG_COUNTER_UP) {
        debugCounterValue += 1;
    }
    if (stepFlags & DEBUG_COUNTER_DOWN) {
        debugCounterValue -= 1;
    }
    debugCounterValue &= DEBUG_COUNTER_MASK;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &debugCounterPacket, 52);
    xglFontDebugPrintf(16, 16, debugCounterFormat, debugCounterValue);
}

extern char testTexturePath[];

void TexturTest(void) {
    xglCdReadFile(testTexturePath, WorkEnd, 0, CD_NOTIFY_DUMMY);
    if ((PadData[0].pressed & PAD_CROSS) == 0) {
        do {
            xglTexUploadList((XglTextureTransSet *) WorkEnd);
            debugCounterStepAndDraw();
            xglSleep();
        } while ((PadData[0].pressed & PAD_CROSS) == 0);
    }
}

extern unsigned char choiceItemText[8];

void choiceMenuTest(void) {
    int state;
    int i;
    short *pValue;
    unsigned char **pItem;
    PadRecord *pad;

    pValue = choiceTestValues;
    pItem = choiceTestItems;
    for (i = 0; i < CHOICE_TEST_ITEM_COUNT; i++) {
        *pItem++ = choiceItemText;
        *pValue++ = xglSRand() & CHOICE_TEST_VALUE_MASK;
        choiceTestFlags[i] = (xglSRand() & 1) * 3;
    }
    choiceTestItems[CHOICE_TEST_ITEM_COUNT] = 0;
    for (i = CHOICE_TEST_SUB_ITEM_COUNT - 1; i >= 0; i--) {
        choiceTestSubItems[i] = choiceItemText;
    }
    choiceTestSubItems[CHOICE_TEST_SUB_ITEM_COUNT] = 0;
    state = CHOICE_TEST_STATE_MAIN;
    choiceMenuOpen(0, 30, 200, 420, 111, 1, 2, 0, choiceTestItems, choiceTestValues, choiceTestFlags);
    pad = PadData;
    do {
        switch (state) {
        case CHOICE_TEST_STATE_MAIN:
            if (PadData[0].directionPressed & 0x40) {
                choiceMenuClose(0);
            }
            if (choiceMenuIsOpen(0) == 0) {
                choiceMenuGetResult(0);
                state = CHOICE_TEST_STATE_DONE;
            } else if (PadData[0].pressed & PAD_TRIANGLE) {
                choiceMenuSetActive(0, 0);
                choiceMenuOpen(1, 30, 330, 420, 39, 1, 2, 0, choiceTestSubItems, 0, 0);
                state = CHOICE_TEST_STATE_SUB;
            }
            break;
        case CHOICE_TEST_STATE_SUB:
            if (pad->directionPressed & 0x40) {
                choiceMenuClose(1);
            }
            if (choiceMenuIsOpen(1) == 0) {
                if (choiceMenuGetResult(1) == 0) {
                    choiceMenuSetActive(0, 1);
                    state = CHOICE_TEST_STATE_MAIN;
                } else {
                    choiceMenuClose(0);
                    state = CHOICE_TEST_STATE_DONE;
                }
            }
            break;
        }
        if (state == CHOICE_TEST_STATE_DONE) {
            break;
        }
        textUpdate();
        XenoActorDoit();
        XenoActorDisp();
        GameSnapShotCheck();
        xglSleep();
    } while (state != CHOICE_TEST_STATE_DONE);
}

/* Clears the frame, then sleeps forever: a test loop that never returns. */
void ItaiTest(void) {
    xglRenderClearFrame();
    for (;;) {
        xglSleep();
    }
}

/* JP: バトル●ウィン\nドウ●
 * EN: Battle win\ndow (● marks between the pieces) */
char testTextEntryWindowName[24] = "\245\320\245\310\245\353\241\374\245\246\245\243\245\363\\n\245\311\245\246\241\374";
/* JP: テ●ス●ト●中で●
 * EN: being te●st●ed● (● marks between the pieces) */
char testTextEntryTestName[24] = "\245\306\241\374\245\271\241\374\245\310\241\374\303\346\244\307\241\374";
/* JP: ございます\n●
 * EN: (polite ending; the three names read "The battle window is being tested.")\n● */
char testTextEntryThanksName[16] = "\244\264\244\266\244\244\244\336\244\271\012\241\374";
char testTexturePath[32] = "yosikawa\\battle01.xtx";
TexUploadBuffer texUploadTagBuffer = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x05, 0x00, 0x00, 0x51, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x00083C0000000000ULL, { 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x0000000000000000ULL, { 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
DebugCounterPacket debugCounterPacket = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x50, 0x01, 0x80, 0x00, 0x00, 0x00, 0x40, 0xAB, 0xB0, 0xEE, 0xEE, 0xEE, 0x31, 0x35, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x0000000000000000ULL, { 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00 }, 0x0U, 0x0U, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x6FF8U, 0x81F7U, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x780U, 0x480U, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x7778U, 0x7677U, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
unsigned char fontInitTagText[16] = "$init;$bg0;";
/* JP: 選択肢
 * EN: Choices */
unsigned char choiceItemText[8] = "\xc1" "\xaa" "\xc2" "\xf2" "\xbb" "\xe8";
int debugCounterValue = 0x80;
char debugCounterFormat[16] = "%8d\000\000\000\000\000PxyHeap";

static unsigned char fontLayoutRestoreTable[520];
