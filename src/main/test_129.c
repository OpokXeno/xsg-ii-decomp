#include "common.h"
#include "main/xgl_debug_file.h"
#include "main/assert_fail.h"
#include "main/pad.h"
#include "main/xgl_font.h"
#include "main/xgl_cd.h"
#include "main/TiWater.h"
#include "main/xgl_thread.h"
#include "main/test_129.h"
#include "main/glow.h"

/* Opens the .data of this file (0x631140); read by TiWater.c. */
float tiWaterTwoPi = 6.28318548f;

/* Debug test menu layout: entry count, the origin and line height of the list, and the slide offset at rest. */
#define TEST_MENU_ENTRY_COUNT 4
#define TEST_MENU_ORIGIN_X 20
#define TEST_MENU_ORIGIN_Y 20
#define TEST_MENU_LINE_HEIGHT 12
#define TEST_MENU_SLIDE_REST 10
/* Overlay loaded when the menu opens (its name is not known from this file). */
#define TEST_MENU_OVERLAY 98

extern const char testLoadFileDataAssertText[];
extern const char testLoadFileSourceName[];
extern int testMenuFontPrefix;
void testCfEventToolRun(void);

int testMenuFontPrefix = 0x0068C728;
int testMenuUnusedWord0 = 0;
int testMenuUnusedWord1 = 0;

int tiWaterSubCount = 0;

int tiWaterDebugMode = 0;

float tiWaterEffectNum = 0.0f;

float tiWaterCx = 256.0f;

float tiWaterCy = 224.0f;

float tiWaterAlphaType = 0.0f;

float tiWaterAlpha = 63.0f;

float tiWaterFbp = 0.0f;

float tiWaterBlur = 10.0f;

float tiWaterDecrease = 0.0133400001f;

float tiWaterIntVals[4] = {0.0f, 0.0f, 50.0f, 100.0f};

/* Debug test menu: UP and DOWN move the cursor, CIRCLE runs the highlighted entry, SELECT with START leaves it. */
void testMenuRun(void)
{
    void (*callbacks[TEST_MENU_ENTRY_COUNT])(void) = {testCfEventToolRun, glowRunScene, tiWaterSceneLoop, tiWaterSceneLoop};
    /* JP: てすと (the fourth label)
     * EN: Test */
    const char *labels[TEST_MENU_ENTRY_COUNT] = {"CFEventTool", "GlowTest", "WaterTest", "\xa4\xc6\xa4\xb9\xa4\xc8"};
    int selected;
    int slideOffset;
    unsigned short pressed;
    int i;

    selected = 0;
    slideOffset = TEST_MENU_SLIDE_REST;
    xglCdLoadOverlay(TEST_MENU_OVERLAY);
    for (;;) {
        xglFontDebugPrefixSet(testMenuFontPrefix);
        if ((PadData[0].heldAndPressed & (PAD_SELECT | ((unsigned long long)PAD_START << 16))) ==
            (PAD_SELECT | ((unsigned long long)PAD_START << 16))) {
            break;
        }
        pressed = PadData[0].pressed;
        if (pressed & PAD_UP) {
            selected -= 1;
            slideOffset = 0;
        }
        if (pressed & PAD_DOWN) {
            selected += 1;
            slideOffset = 0;
        }
        if (selected < 0) {
            selected = TEST_MENU_ENTRY_COUNT - 1;
        }
        if (selected >= TEST_MENU_ENTRY_COUNT) {
            selected = 0;
        }
        if (pressed & PAD_CIRCLE) {
            callbacks[selected]();
        }
        for (i = 0; i < TEST_MENU_ENTRY_COUNT; i++) {
            if (selected == i) {
                xglFontDebugPrintf(TEST_MENU_ORIGIN_X - slideOffset, TEST_MENU_ORIGIN_Y + i * TEST_MENU_LINE_HEIGHT, labels[i]);
            } else {
                xglFontDebugPrintf(TEST_MENU_ORIGIN_X, TEST_MENU_ORIGIN_Y + i * TEST_MENU_LINE_HEIGHT, labels[i]);
            }
        }
        if (slideOffset < TEST_MENU_SLIDE_REST) {
            slideOffset += 2;
        }
        xglSleep();
    }
}

/*
 * Reads the debug file name into the next 16-byte aligned block of the test heap
 * cursor (*bufferCursor) and returns its address, or 0 for an empty file.
 * The NULL check is the assert on pData at ../test.euc.c line 95.
 */
int testLoadFile(int *bufferCursor, const char *name) {
    int buffer;
    int size;

    buffer = 0;
    size = xglDebFileSize(name);
    if (size > 0) {
        buffer = *bufferCursor;
        *bufferCursor = (buffer + size + 0xF) & ~0xF;
        if (buffer == 0) {
            assertFailed(testLoadFileDataAssertText, testLoadFileSourceName, 95);
        }
        /* The test heap keeps its cursor as an address word. */
        xglDebFileRead(name, (void *) buffer);
    }
    return buffer;
}

/* Converts the two-byte character at first/second in place: the lead byte is doubled and both bytes are
 * moved into the Shift-JIS ranges. Only yg2PathReadTaggedText calls it, for each high-bit character. */
void sjisToEuc(unsigned char *first, unsigned char *second)
{
    unsigned int leadDoubled;
    unsigned char trailCode;

    leadDoubled = *first << 1;
    *first = leadDoubled;
    trailCode = *second;
    if (trailCode < 0x9F) {
        if ((leadDoubled & 0xFE) < 0x3F) {
            *first = leadDoubled + 0x9F;
        } else {
            *first = leadDoubled + 0x1F;
        }
        if (*second >= 0x7F) {
            *second = *second + 0x60;
        } else {
            *second = *second + 0x61;
        }
    } else {
        if ((leadDoubled & 0xFE) < 0x3F) {
            *first = leadDoubled + 0xA0;
        } else {
            *first = leadDoubled + 0x20;
        }
        *second = *second + 2;
    }
}

/* Bump pointer of the test heap. */
static int testHeapCursor;

/* Sets the test heap cursor. */
void testHeapSetCursor(int newCursor) {
    testHeapCursor = newCursor;
}

/* Bump allocation: returns the next 16-byte aligned address and moves the cursor past size bytes. */
int testHeapAlloc(int size) {
    int buffer;
    int cursorAfter;

    buffer = (testHeapCursor + 0xF) & ~0xF;
    cursorAfter = buffer + size;
    testHeapCursor = cursorAfter;
    return buffer;
}

const char testLoadFileDataAssertText[8] = "pData";

const char testLoadFileSourceName[16] = "../test.euc.c";

const char tiWaterDebugIntVal4Format[16] = "\015\002IntVal4   %f";

const char tiWaterDebugIntVal3Format[16] = "\015\002IntVal3   %f";

const char tiWaterDebugIntVal2Format[16] = "\015\002IntVal2   %f";

const char tiWaterDebugIntVal1Format[16] = "\015\002IntVal1   %f";

const char tiWaterDebugDecreaseFormat[16] = "\015\002Decrease  %f";

const char tiWaterDebugBlurFormat[16] = "\015\002Blur      %f";

const char tiWaterDebugFbpFormat[16] = "\015\002FBP       %f";

const char tiWaterDebugAlphaFormat[16] = "\015\002Alpha     %f";

const char tiWaterDebugAlphaTypeFormat[16] = "\015\002AlphaType %f";

const char tiWaterDebugCyFormat[16] = "\015\002cy        %f";

const char tiWaterDebugCxFormat[16] = "\015\002cx        %f";

const char tiWaterDebugEffectNumFormat[16] = "\015\002EffectNum %f";

const char *tiWaterDebugLabels[12] = {
    tiWaterDebugEffectNumFormat, tiWaterDebugCxFormat, tiWaterDebugCyFormat, tiWaterDebugAlphaTypeFormat, tiWaterDebugAlphaFormat, tiWaterDebugFbpFormat,
    tiWaterDebugBlurFormat, tiWaterDebugDecreaseFormat, tiWaterDebugIntVal1Format, tiWaterDebugIntVal2Format, tiWaterDebugIntVal3Format, tiWaterDebugIntVal4Format,
};

float tiWaterDebugSteps[12] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.01f, 1.0f, 1.0f, 1.0f, 1.0f };

float *tiWaterDebugValues[14] = {
    &tiWaterEffectNum, &tiWaterCx, &tiWaterCy, &tiWaterAlphaType, &tiWaterAlpha, &tiWaterFbp, &tiWaterBlur,
    &tiWaterDecrease, &tiWaterIntVals[0], &tiWaterIntVals[1], &tiWaterIntVals[2], &tiWaterIntVals[3], 0, 0,
};

unsigned int tiWaterStateTemplate[60] = {
    0, 0, 0, 0x5100000Eu, 32769, 0xD08B4000u, 0x31EEEEEEu, 0xEE535u,
    0x31000000u, 1, 78, 0, 0x71001u, 0, 71, 0,
    0, 0, 20, 0, 1, 0, 74, 0,
    0, 0, 76, 0, 0, 0, 6, 0,
    128, 128, 128, 0, 0, 0, 0, 0,
    32760, 29175, 0xFFFFFFFFu, 0, 8192, 7168, 0, 0,
    36856, 32759, 0xFFFFFFFFu, 0, 0, 0, 63, 0,
    0, 0, 74, 0
};
unsigned int tiWaterDepthProgramSelect[2] = { 0 };
unsigned int tiWaterSurfaceGridHeader[4] = {
    32768, 0x300E4000u, 1298, 0
};
unsigned int tiWaterDepthGridHeader[4] = {
    32768, 0x302E4000u, 1298, 0
};
TiWaterSub *tiWaterSubList[66] = { 0 };
