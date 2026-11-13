#include "common.h"
#include "main/tu005.h"
#include "main/tu026.h"
#include "main/pad.h"
#include "main/xgl_font.h"
#include "main/control_entry.h"

/* Debug test menu layout: entry count, the origin and line height of the list, and the slide offset at rest. */
#define TEST_MENU_ENTRY_COUNT 4
#define TEST_MENU_ORIGIN_X 20
#define TEST_MENU_ORIGIN_Y 20
#define TEST_MENU_LINE_HEIGHT 12
#define TEST_MENU_SLIDE_REST 10
/* Overlay loaded when the menu opens (its name is not known from this file). */
#define TEST_MENU_OVERLAY 98

int xglDebFileRead(const char *name, void *buffer);
int xglDebFileSize(const char *name);

extern char D_0068C788[4];
extern char D_0068C790[4];
extern PadRecord PadData;
extern int D_00631144;
void xglCdLoadOverlay(int overlay);
void xglFontSetStoredValue(int value);
void glowRunScene(void);
void tiWaterSceneLoop(void);
void func_00A89428(void);

/* Debug test menu: UP and DOWN move the cursor, CIRCLE runs the highlighted entry, SELECT with START leaves it. */
void testMenuRun(void)
{
    void (*callbacks[TEST_MENU_ENTRY_COUNT])(void) = {func_00A89428, glowRunScene, tiWaterSceneLoop, tiWaterSceneLoop};
    const char *labels[TEST_MENU_ENTRY_COUNT] = {"CFEventTool", "GlowTest", "WaterTest", "\xa4\xc6\xa4\xb9\xa4\xc8"};
    int selected;
    int slideOffset;
    unsigned short pressed;
    int i;

    selected = 0;
    slideOffset = TEST_MENU_SLIDE_REST;
    xglCdLoadOverlay(TEST_MENU_OVERLAY);
    for (;;) {
        xglFontSetStoredValue(D_00631144);
        if ((PadData.heldAndPressed & (PAD_SELECT | ((unsigned long long)PAD_START << 16))) ==
            (PAD_SELECT | ((unsigned long long)PAD_START << 16))) {
            break;
        }
        pressed = PadData.pressed;
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
 * Reads a debug file into the next 16-byte aligned block of the test heap
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
            assertFailed(D_0068C788, D_0068C790, 95);
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
extern int testHeapCursor;

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
