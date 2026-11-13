#include "common.h"
#include "shared.h"
#include "main/tu026.h"
#include "main/tu092.h"
#include "main/tu097.h"
#include "main/tu101.h"
#include "main/xgl_cd.h"
#include "main/yg2path.h"
#include "main/ysegf.h"

/* Scene states: the rows of ysegfStateTable (0 = list page, 1 = detail page). */
enum YsegfScene {
    YSEGF_SCENE_LIST = 0,
    YSEGF_SCENE_DETAIL = 1
};

/* Sound effects started by the page scripts. */
enum YsegfSound {
    YSEGF_SE_CURSOR_MOVE = 0x1000001,   /* up/down or left/right moves the selection */
    YSEGF_SE_DECIDE = 0x1000002,        /* circle opens an entry that has its flag set */
    YSEGF_SE_DENIED = 0x1000003,        /* circle on an entry whose flag is not set */
    YSEGF_SE_CANCEL = 0x1000008         /* cross or triangle leaves the page */
};

#define YSEGF_ENTRY_COUNT 18              /* entries: treasures, entry positions and segment addresses */
#define YSEGF_ENTRY_LAST 17               /* YSEGF_ENTRY_COUNT - 1 */
#define YSEGF_ENTRY_START_POS 0xC8        /* entry position that ysegfInit and ysegfResetEntryPos set */
#define YSEGF_FLAG_BASE 0x1C1             /* xglFlagsGet bit of entry i is YSEGF_FLAG_BASE + i */
#define YSEGF_ROW_HEIGHT 101              /* vertical distance between two list rows */
#define YSEGF_TREASURE_TABLE_SIZE (YSEGF_ENTRY_COUNT * sizeof(YsegfTreasure))
#define YSEGF_CONTROL_SOFT_RESET 0x10000000 /* same bit as CONTROL_SOFT_RESET in include/main/tu051.h */

typedef struct YsegfTarget {
    u8 unmodeled_00[0x48];
    int flag;
} YsegfTarget;

void ysegfSetTargetFlagOnMode(int mode, int unused_arg1, YsegfTarget *target)
{
    switch (mode) {
    case -2:
    case -1:
    case 0:
        break;
    case 1:
        target->flag = 1;
        break;
    case 2:
    case 3:
    case 4:
        break;
    }
}

/* One 0x14-byte row of ysegfStateTable (the current scene state is ysegfState): a one-time start hook,
   a handle queued with xglCallbackQueueAdd on every run, a finish hook called last, and hook_end
   run when the state is left. */
typedef struct YsegfSceneState {
    int initialized;            /* +0x00: 0 until the start hook has run */
    void (*start)(void);        /* +0x04 */
    int handle;                 /* +0x08 */
    void (*finish)(void);       /* +0x0C */
    void (*hook_end)();         /* +0x10: called with the state slot (ysegfState) */
} YsegfSceneState;

/* Runs one frame of a scene state: start hook once, then the queued handle, then the finish hook. */
void ysegfRunState(YsegfSceneState *record)
{
    int handle;

    if (record->initialized == 0) {
        record->start();
        record->initialized = 1;
    }
    handle = record->handle;
    if (handle != 0) {
        xglCallbackQueueAdd(handle);
    }
    record->finish();
}

/* Leaves the current scene state (its hook_end runs with the slot), then makes nextState the current one with initialized cleared. */
void ysegfSwitchState(YsegfSceneState **stateSlot, YsegfSceneState *nextState)
{
    (*stateSlot)->hook_end(stateSlot);
    *stateSlot = nextState;
    nextState->initialized = 0;
}

void segAddrOnClearAll(void);

void yg2PathResetAll(void)
{
    yg2PathClearProgress();
    segAddrOnClearAll();
}

/* Eighteen signed-char flags, one per segment address (index = address number - 1): written by the
   seg.addrOn native (segSetAddrOn), read by seg.getAddrOn (segGetAddrOn), and cleared by yg2PathResetAll. */
extern signed char segAddrOnFlags[YSEGF_ENTRY_COUNT];

void segSetAddrOn(u32 index, int value)
{
    if (index < YSEGF_ENTRY_COUNT) {
        segAddrOnFlags[index] = value;
    }
}

signed char segGetAddrOn(u32 index)
{
    if (index >= YSEGF_ENTRY_COUNT) {
        return 0;
    }
    return segAddrOnFlags[index];
}

void segAddrOnClearAll(void)
{
    signed char *cursor;
    int i;

    cursor = segAddrOnFlags;
    cursor += YSEGF_ENTRY_LAST;
    for (i = 0; i < YSEGF_ENTRY_COUNT; i++) {
        *cursor = 0;
        cursor--;
    }
}

void *xglCdBufferReserve(int size);
extern unsigned char D_0068B200[];
extern unsigned char D_0069D8A0[];

/* Allocates a buffer sized for the named CD file. The size comes from the
 * directory lookup xglCdGetFileSize; a zero size asserts "size" in ../ysegf.euc.c. */
void *ysegfAllocFileBuffer(const char *name) {
    int fileSize;

    fileSize = xglCdGetFileSize(name);
    if (fileSize == 0) {
        assertFailed(D_0069D8A0, D_0068B200, 0x123);
    }
    return xglCdBufferReserve(fileSize);
}

/* Reads a CD file into *buffer, allocating the buffer first when allocate is set. */
void ysegfReadFile(void **buffer, const char *name, int allocate, int mode) {
    if (allocate != 0) {
        *buffer = ysegfAllocFileBuffer(name);
    }
    xglCdReadFile(name, *buffer, mode, 0);
}

#include "main/pad.h"

int xglFlagsGet(int bit_offset, int bit_count);
/* One 169-byte treasure record: the three tagged texts that ysegfLoadTreasure reads (offsets from the record start). */
typedef struct YsegfTreasure {
    unsigned char item[21];          /* +0x00: item name (tagItem) */
    unsigned char location[27];      /* +0x15: location text (tagLocate) */
    unsigned char explanation[121];  /* +0x30: explanation text (tagExplanation) */
} YsegfTreasure;

extern YsegfTreasure *ysegfTreasureTable;
extern unsigned char D_0068B210[];
extern unsigned char D_0068B220[];
extern unsigned char D_0068B230[];
extern unsigned char D_0068B240[];
extern unsigned char D_0069D8A8[];
void *memset(void *dest, int value, unsigned int count);
void *yg2PathReadTaggedText(void *source, void *tag, void *dest);

/* Loads the treasure table: clears the 3042-byte buffer, reads the tag file, then reads
 * three tagged texts into each of the eighteen 169-byte records. */
void ysegfLoadTreasure(void) {
    void *textCursor;
    int index;
    unsigned char *tagItem = D_0068B210;
    unsigned char *tagLocate = D_0068B220;
    unsigned char *tagExplanation = D_0068B230;

    textCursor = 0;
    ysegfTreasureTable = xglCdBufferReserve(YSEGF_TREASURE_TABLE_SIZE);
    memset(ysegfTreasureTable, 0, YSEGF_TREASURE_TABLE_SIZE);
    while (xglCdServiceIfPending() != 0) {
    }
    ysegfReadFile(&textCursor, D_0068B240, 1, 0);
    if (textCursor == 0) {
        assertFailed(D_0069D8A8, D_0068B200, 328);
    }
    for (index = 0; index < YSEGF_ENTRY_COUNT; index++) {
        textCursor = yg2PathReadTaggedText(textCursor, tagItem, ysegfTreasureTable[index].item);
        textCursor = yg2PathReadTaggedText(textCursor, tagLocate, ysegfTreasureTable[index].location);
        textCursor = yg2PathReadTaggedText(textCursor, tagExplanation, ysegfTreasureTable[index].explanation);
    }
}

extern signed char ysegfSelectedIndex;
extern signed char ysegfScrollTop;

/* Clears the two signed cursor bytes of the item page. */
void ysegfResetSelection(void)
{
    ysegfSelectedIndex = 0;
    ysegfScrollTop = 0;
}

void menuResetState(void *menu);
void msgRangeSetText(void *menu, const char *text);
void cfeFileTempInit(void *buffer, int size);
void ysegfResetEntryPos(void);
void ysegfDrawList(void);
void ysegfUpdatePage(void);
void ysegfStartFadeIn(void);
void ysegfDrawDetail(void);
void ysegfUpdateDetail(void);
void ysegfListLeaveNoop(void);
void func_00233D50(void);
extern struct FontTrack ysegfListTrack;
extern unsigned char ysegfMenu[];
extern unsigned char D_0068B258[];
extern unsigned char D_0068B2C8[];
extern unsigned char D_0068B2E8[];
extern unsigned char D_0068B308[];
extern unsigned char D_0068B328[];
extern unsigned char D_0068B348[];
extern unsigned char D_0068B358[];
extern unsigned char ysegfItemFadeAlpha;
extern unsigned char D_0069D8B0[];
extern unsigned char D_0069D8B8[];
extern unsigned char ysegfEntryPos[];
extern void *ysegfDoorImage;
extern void *ysegfItemImage;
extern void *ysegfDoorAlpha;
extern void *ysegfItemAlpha;

extern YsegfSceneState ysegfStateTable[2];
extern YsegfSceneState *ysegfState;

/* Scene init: resets the menu and font state, loads the treasure data and the item and door images. */
void ysegfInit(void)
{
    int index;
    int lastEntryOffset;
    int *slot;
    YsegfSceneState *scene;

    menuResetState(ysegfMenu);
    msgRangeSetText(ysegfMenu, D_0068B258);
    lastEntryOffset = 0x44;
    slot = (int *) (ysegfEntryPos + lastEntryOffset);
    for (index = 0; index < YSEGF_ENTRY_COUNT; index++) {
        *slot = YSEGF_ENTRY_START_POS;
        slot--;
    }
    fontTrackSetBoth(&ysegfListTrack, 0x200, -ysegfScrollTop * YSEGF_ROW_HEIGHT);
    ysegfListTrack.target_pair.x = 0;
    do {
    } while (xglCdServiceIfPending() != 0);
    cfeFileTempInit(xglCdBufferReserve(0x60000), 0x60000);
    ysegfLoadTreasure();
    scene = ysegfStateTable;
    scene[YSEGF_SCENE_LIST].start = ysegfResetEntryPos;
    scene[YSEGF_SCENE_LIST].handle = (int) ysegfDrawList;
    scene[YSEGF_SCENE_LIST].finish = ysegfUpdatePage;
    scene[YSEGF_SCENE_LIST].hook_end = ysegfListLeaveNoop;
    scene[YSEGF_SCENE_DETAIL].start = ysegfStartFadeIn;
    scene[YSEGF_SCENE_DETAIL].handle = (int) ysegfDrawDetail;
    scene[YSEGF_SCENE_DETAIL].finish = ysegfUpdateDetail;
    scene[YSEGF_SCENE_DETAIL].hook_end = func_00233D50;
    scene[YSEGF_SCENE_DETAIL].initialized = 0;
    ysegfState = scene;
    scene[YSEGF_SCENE_LIST].initialized = 0;
    ysegfItemFadeAlpha = 0;
    ysegfItemImage = 0;
    ysegfItemAlpha = 0;
    ysegfDoorImage = 0;
    ysegfDoorAlpha = 0;
    ysegfItemImage = ysegfAllocFileBuffer(D_0068B2C8);
    ysegfReadFile(&ysegfItemAlpha, D_0068B2E8, 1, 0);
    ysegfReadFile(&ysegfDoorImage, D_0068B308, 1, 0);
    ysegfReadFile(&ysegfDoorAlpha, D_0068B328, 1, 0);
    if (ysegfItemImage == 0) {
        assertFailed(D_0069D8B0, D_0068B200, 403);
    }
    if (ysegfItemAlpha == 0) {
        assertFailed(D_0068B348, D_0068B200, 404);
    }
    if (ysegfDoorImage == 0) {
        assertFailed(D_0069D8B8, D_0068B200, 405);
    }
    if (ysegfDoorAlpha == 0) {
        assertFailed(D_0068B358, D_0068B200, 406);
    }
    do {
    } while (xglCdServiceIfPending() != 0);
}

/* Releases the scene buffers and clears the four image pointers. */
void ysegfReleaseBuffers(void) {
    xglCdBufferRewind();
    ysegfItemImage = 0;
    ysegfItemAlpha = 0;
    ysegfDoorImage = 0;
    ysegfDoorAlpha = 0;
}

/* Draws a pair of sprites at (x, y); the second pair is drawn only when flag is set. */
void ysegfDrawFlagIcon(int x, int y, int flag) {
    packetSubmitRequest(0x253, x, y, 0);
    packetSubmitRequest(0x256, x, y, 1);
    if (flag != 0) {
        packetSubmitRequest(0x266, x + 0x17, y + 0x18, 0);
        packetSubmitRequest(0x267, x + 0x10, y + 0x10, 1);
    }
}

/* Draws a window frame: four corner sprites and two edge strips (sprite 0x236). */
void ysegfDrawWindow(int x, int y, int width, int height)
{
    int right;
    int bottom;

    packetSubmitRequest(0x237, x, y, 0);
    right = (x + width) - 0x10;
    bottom = (y + height) - 0x10;
    packetSubmitRequest(0x238, right, y, 0);
    packetSubmitRequest(0x234, x, bottom, 0);
    packetSubmitRequest(0x235, right, bottom, 0);
    packetSubmitRequestEx(0x236, x, y + 0x10, width, height - 0x20, 0x100);
    packetSubmitRequestEx(0x236, x + 0x10, y, width - 0x20, height, 0x100);
}

/* Draws a panel frame of eight pieces (sprites 0x239-0x240) around x, y, width, height. */
void ysegfDrawPanel(int x, int y, int width, int height)
{
    int right;
    int bottom;

    packetSubmitRequest(0x23E, x, y, 1);
    right = (x + width) - 0x10;
    packetSubmitRequest(0x240, right, y, 1);
    bottom = (y + height) - 0x11;
    packetSubmitRequest(0x239, x, bottom, 1);
    packetSubmitRequest(0x23B, right, bottom, 1);
    packetSubmitRequestEx(0x23F, x + 0x10, y, width - 0x20, 0x10, 1);
    packetSubmitRequestEx(0x23A, x + 0x10, bottom, width - 0x20, 0x11, 1);
    packetSubmitRequestEx(0x23C, x, y + 0x10, 0x10, height - 0x21, 1);
    packetSubmitRequestEx(0x23D, right, y + 0x10, 0x10, height - 0x21, 1);
}

/* Same frame as ysegfDrawPanel with the sprite set 0x272-0x279. */
void ysegfDrawImageFrame(int x, int y, int width, int height)
{
    int right;
    int bottom;

    packetSubmitRequest(0x277, x, y, 1);
    right = (x + width) - 0x10;
    bottom = (y + height) - 0x11;
    packetSubmitRequest(0x279, right, y, 1);
    packetSubmitRequest(0x272, x, bottom, 1);
    packetSubmitRequest(0x274, right, bottom, 1);
    packetSubmitRequestEx(0x278, x + 0x10, y, width - 0x20, 0x10, 1);
    packetSubmitRequestEx(0x273, x + 0x10, bottom, width - 0x20, 0x10, 1);
    packetSubmitRequestEx(0x275, x, y + 0x10, 0x10, height - 0x21, 1);
    packetSubmitRequestEx(0x276, right, y + 0x10, 0x10, height - 0x21, 1);
}

/* Draws the two footer sprites 0x270 and 0x271 at (0, 0x170). */
void ysegfDrawFooter(void) {
    packetSubmitRequest(0x271, 0, 0x170, 1);
    packetSubmitRequest(0x270, 0, 0x170, 1);
}

extern unsigned char D_003ECAD8[];
extern unsigned char D_003ECAB0[];
extern unsigned char D_0068B368[];
extern unsigned char D_0068B3B0[];
extern unsigned char D_0068B3C0[];
extern unsigned char D_0069D8C8[];
extern unsigned char D_0069D8D0[];
extern unsigned char D_0069D8D8[];
extern void *D_0069D8C0;
void panelDrawNineSlice(unsigned char *image, int x, int y, int width, int height);
int dataBoxChkCategory3(int id);
void menuDrawFixedFrame(int x, int y);
void renderSpriteClutAlpha(int x, int y, int a, int b, int c, int d);
void renderSpriteClut(int x, int y, int a, int b, int c);
int fontObjBuildBaseVifPacket(void);
void textDrawDigitsWide(int x, int y, int value, int digits);
void textDrawSegment(int x, int y, void *text, int style);
void textDrawShadow(int x, int y, void *text, unsigned long long colour);
void textDrawTitle(int a, int x, int y, void *data);
void xglPacketTextureTrans(void *texture);

/* Draws the treasure detail page for the record at index of ysegfTreasureTable. */
void ysegfDrawTreasureDetail(int x, int y, int index, int selected, int alpha)
{
    int frameX;
    int displayNumber;
    int labelY;
    int addressOn;
    int decoderFlag;
    int treasureFound;
    unsigned char *doorTexture;
    int *doorEntry;

    if (index == selected) {
        panelDrawNineSlice(D_003ECAD8, x, y, 0x1D0, 0x64);
    } else {
        panelDrawNineSlice(D_003ECAB0, x, y, 0x1D0, 0x64);
    }
    frameX = x + 0xC;
    displayNumber = index + 1;
    packetSubmitRequest(0x26A, frameX, y - 1, 1);
    labelY = y + 0x15;
    textDrawDigitsWide(x + 0x36, y, displayNumber, 2);
    menuDrawFixedFrame(x + 0x5D, labelY);
    packetSubmitRequest(0x252, x + 0x69, y + 0x1F, 0);
    packetSubmitRequest(0x255, x + 0x69, y + 0x1F, 1);
    packetSubmitRequest(0x252, x + 0x69, y + 0x3E, 0);
    packetSubmitRequest(0x255, x + 0x69, y + 0x3E, 1);
    textDrawSegment(0, 0, D_0068B368, 0);
    /* addressOn: segAddrOnFlags of this entry; decoderFlag: dataBoxChkCategory3 (second icon);
       treasureFound: entry flag (third icon). */
    addressOn = segAddrOnFlags[index];
    decoderFlag = dataBoxChkCategory3(displayNumber);
    treasureFound = xglFlagsGet(index + YSEGF_FLAG_BASE, 1);
    ysegfDrawFlagIcon(x + 0x101, labelY, addressOn);
    ysegfDrawFlagIcon(x + 0x142, labelY, decoderFlag);
    ysegfDrawFlagIcon(x + 0x183, labelY, treasureFound);

    if (addressOn != 0) {
        textDrawSegment(x + 0x71, y + 0x20, ysegfTreasureTable[index].location, 4);
    } else {
        textDrawSegment(x + 0x71, y + 0x22, D_0069D8C0, 4);
    }

    if (treasureFound != 0) {
        textDrawSegment(x + 0x71, y + 0x3F, ysegfTreasureTable[index].item, 4);
        packetSubmitRequest(0x24F, x + 0x120, y + 0x4A, 1);
    } else if (addressOn != 0) {
        textDrawSegment(x + 0x71, y + 0x40, D_0069D8C0, 4);
        packetSubmitRequest(0x24E, x + 0x120, y + 0x4A, 1);
    } else if (decoderFlag != 0) {
        textDrawSegment(x + 0x71, y + 0x40, D_0069D8C0, 4);
        packetSubmitRequest(0x24E, x + 0x120, y + 0x4A, 1);
    } else {
        textDrawSegment(x + 0x71, y + 0x40, D_0069D8C0, 4);
        packetSubmitRequest(0x24D, x + 0x12E, y + 0x4A, 1);
    }

    if (addressOn != 0) {
        doorEntry = (int *) ysegfDoorImage + index;
        doorTexture = (unsigned char *) ysegfDoorImage + doorEntry[1];
        xglPacketTextureTrans(ysegfDoorAlpha);
        renderSpriteClutAlpha(frameX, y + 0x17, 0x4C, 0x41, 0x20, alpha);
        xglPacketTextureTrans(doorTexture);
        renderSpriteClut(frameX, y + 0x17, 0x4C, 0x41, 0x20);
        fontObjBuildBaseVifPacket();
        ysegfDrawPanel(frameX, y + 0x17, 0x4C, 0x41);
    } else {
        ysegfDrawWindow(frameX, y + 0x17, 0x4C, 0x41);
        packetSubmitRequest(0x250, frameX, y + 0x17, 1);
        ysegfDrawPanel(frameX, y + 0x17, 0x4C, 0x41);
        fontObjSetColor(0x3F80000060000000ULL);
        fontObjSetColor(0x3F80000080808080ULL);
        textDrawShadow(x + 0x13, y + 0x30, D_0069D8C8, 0x3F80000000000000ULL);
        fontObjResetColor();
    }

    textDrawTitle(0, x + 0x6C, labelY, D_0068B3B0);
    textDrawTitle(0, x + 0x6C, y + 0x34, D_0068B3C0);
    textDrawTitle(0, x + 0x109, y + 0x16, D_0069D8D0);
    textDrawTitle(0, x + 0x14A, y + 0x16, D_0069D8D8);
    textDrawTitle(0, x + 0x187, y + 0x16, D_0068B3C0);
}

void xglFontFlush(void);

/* Draws the treasure list: eighteen detail rows placed from the entry positions,
 * the two count marks, then flushes the font. */
void ysegfDrawTreasureList(void)
{
    int row;
    int *entrySlide;
    int rawSlide;
    int x;
    int y;
    int alpha;
    int slideX;
    struct FontPair *listOrigin;

    packetSubmitRequest(0x24C, 0x40, 0x22, 1);
    packetSubmitRequestEx(0x24C, 0x1D3, -0x80, 1, 0x240, 0x81);
    packetSubmitRequestEx(0x24C, 0x1D2, -0x80, 1, 0x240, 0x81);
    listOrigin = &ysegfListTrack.current_pair;
    entrySlide = (int *) ysegfEntryPos;
    for (row = 0; row < YSEGF_ENTRY_COUNT; row++) {
        y = listOrigin->y + row * YSEGF_ROW_HEIGHT + 0x34;
        slideX = entrySlide[row];
        rawSlide = slideX;
        if (slideX >= 31) {
            slideX = 30;
        }
        x = listOrigin->x + slideX;
        if (x < 0x200 && rawSlide < 0x1F0) {
            alpha = 0x7F - ((rawSlide >> 2) & 0x7F);
            if (rawSlide >= 31) {
                fontObjSetOpacity(alpha);
            }
            ysegfDrawTreasureDetail(x, y, row, ysegfSelectedIndex, alpha);
            fontObjResetOpacity();
        }
    }
    if (ysegfScrollTop > 0) {
        packetSubmitRequest(0x26C, 0x1DD, 0x23, 1);
    }
    if (ysegfScrollTop < 15) {
        packetSubmitRequest(0x26B, 0x1DD, 0x160, 1);
    }
    xglFontFlush();
    fontObjResetAndBuild();
}

extern struct FontDrawItem ysegfItemNameDraw;
void menuDrawShortOutlineFrame(void);
void fontObjDraw(struct FontDrawItem *item);

/* Draws the item-name frame: four font cells across the top row, then clears the colour. */
void ysegfDrawItemNameFrame(int x, int y, int width, int unused_arg3, void *labelText)
{
    int inner_width;

    menuDrawShortOutlineFrame();
    fontObjSetColor(0x3F80000080000080ULL);
    inner_width = width - 16;
    ysegfItemNameDraw.cell_index = 82;
    ysegfItemNameDraw.x = x;
    ysegfItemNameDraw.y = y;
    ysegfItemNameDraw.flags = 5;
    fontObjDraw(&ysegfItemNameDraw);
    ysegfItemNameDraw.cell_index = 83;
    ysegfItemNameDraw.flags = 13;
    ysegfItemNameDraw.x = ysegfItemNameDraw.x + 8;
    ysegfItemNameDraw.size_w = inner_width;
    ysegfItemNameDraw.size_h = 16;
    fontObjDraw(&ysegfItemNameDraw);
    ysegfItemNameDraw.flags = 5;
    ysegfItemNameDraw.cell_index = 84;
    ysegfItemNameDraw.x = ysegfItemNameDraw.x + inner_width;
    fontObjDraw(&ysegfItemNameDraw);
    ysegfItemNameDraw.cell_index = 90;
    ysegfItemNameDraw.flags = 280;
    ysegfItemNameDraw.size_w = width;
    ysegfItemNameDraw.size_h = 16;
    ysegfItemNameDraw.x = x;
    ysegfItemNameDraw.y = y;
    fontObjDraw(&ysegfItemNameDraw);
    fontObjResetColor();
    if (labelText != 0) {
        textDrawTitle(0, x + 12, y - 1, labelText);
    }
}

extern unsigned char D_003ECB00[];
extern unsigned char D_0068B3D0[];
extern unsigned char D_0068B3E0[];
extern unsigned char D_0068B420[];
extern unsigned char D_0069D8E0[];
extern unsigned char D_0069D8E8[];
extern unsigned char D_0069D8F0[];
void sprintf(void *dst, void *format, ...);
void func_002AA7F0(void *dst, void *format);
void textDrawDigitsPair(int x, int y, int value, int width, int digits);

/* Draws the item explanation page: the panel and labels, the selected item's
 * text, then the item image, which fades in when it is loaded or loads its file. */
void ysegfDrawItemPage(void)
{
    char fileName[64];

    panelDrawNineSlice(D_003ECB00, 47, 120, 417, 243);
    packetSubmitRequest(0x27F, 72, 0x77, 1);
    packetSubmitRequest(0x228, 55, 0x8F, 1);
    packetSubmitRequest(0x232, 0x1B8, 0x8F, 0);
    packetSubmitRequest(0x22A, 0x1B8, 0x8F, 1);
    packetSubmitRequestEx(0x229, 71, 0x8F, 0x171, 16, 1);
    packetSubmitRequestEx(0x227, 55, 0x9F, 16, 0xAE, 1);
    packetSubmitRequestEx(0x231, 0x1B8, 0x9F, 16, 0xAE, 0);
    packetSubmitRequest(0x22E, 55, 0x14D, 0);
    packetSubmitRequest(0x226, 55, 0x14D, 1);
    packetSubmitRequest(0x230, 0x1B8, 0x14D, 0);
    packetSubmitRequestEx(0x22F, 71, 0x14D, 0x171, 16, 0);
    ysegfDrawItemNameFrame(75, 163, 206, 165, D_0068B3D0);
    menuDrawOutlineFrame(294, 153, 147, 165);
    menuDrawOutlineFrame(69, 153, 216, 184);
    packetSubmitRequestEx(0x27A, 73, 0xD1, 0xD2, 5, 1);
    packetSubmitRequestEx(0x27A, 73, 0xE5, 0xD2, 5, 1);
    packetSubmitRequestEx(0x27A, 73, 0xF9, 0xD2, 5, 1);
    packetSubmitRequestEx(0x27A, 73, 0x10D, 0xD2, 5, 1);
    packetSubmitRequestEx(0x27A, 73, 0x121, 0xD2, 5, 1);
    packetSubmitRequestEx(0x27A, 73, 0x135, 0xD2, 5, 1);
    packetSubmitRequest(0x26F, 14, 0xC3, 1);
    packetSubmitRequest(0x26F, 0x1DA, 0xC3, 1);
    packetSubmitRequest(0x26D, 14, 0xC3, 1);
    packetSubmitRequest(0x26E, 0x1DA, 0xC3, 1);
    packetSubmitRequest(0x1C, 0x15E, 0x14A, 0x40);
    textDrawDigitsPair(390, 330, ysegfSelectedIndex + 1, 18, 2);
    textDrawSegment(0, 0, D_0068B3E0, 0);
    textDrawSegment(80, 190, ysegfTreasureTable[ysegfSelectedIndex].explanation, 4);

    if (ysegfItemFadeAlpha != 0 && xglCdServiceIfPending() == 0) {
        xglPacketTextureTrans(ysegfItemAlpha);
        renderSpriteClutAlpha(0x130, 0xA4, 0x80, 0x8F, 0x20, ysegfItemFadeAlpha);
        xglPacketTextureTrans(ysegfItemImage);
        renderSpriteClut(0x130, 0xA4, 0x80, 0x8F, 0x20);
        ysegfItemFadeAlpha = ysegfItemFadeAlpha + 16;
        if (ysegfItemFadeAlpha >= 129) {
            ysegfItemFadeAlpha = 0x80;
        }
        fontObjSetOpacity(ysegfItemFadeAlpha);
        ysegfDrawImageFrame(0x130, 0xA4, 0x80, 0x8F);
        fontObjResetOpacity();
    } else if (xglCdServiceIfPending() == 0) {
        int selected = ysegfSelectedIndex;
        if (selected < 10) {
            sprintf(fileName, D_0069D8E0, D_0068B420, selected);
        } else {
            sprintf(fileName, D_0069D8E8, D_0068B420, selected);
        }
        func_002AA7F0(fileName, D_0069D8F0);
        ysegfReadFile(&ysegfItemImage, fileName, 0, 1);
        ysegfItemFadeAlpha = 16;
    }
    fontObjBuildBaseVifPacket();
}

extern int ysegfExitFlag;
extern unsigned char D_0068B438[];
void soundRestart(void);
void seBgmPlay(int mode, unsigned char *name, int size);
void seVoiceStartDefault(int voice);
void func_001BA68C(void);
void XenoActorSetFlagAll(int flag);
void bgLayerClearAllFlags(void);
void XenoActorDoit(void);
void XenoActorDisp(void);
void xglSleep(void);
void func_001E0E98(void);

typedef struct YsegfGameWork {
    unsigned char unmodeled_00[8];
    unsigned int control; /* +0x8: CONTROL_* bits; 0x10000000 is the soft-reset combo */
} YsegfGameWork;

extern YsegfGameWork gameWork;
extern PadRecord PadData[2];

/* Scene loop of the 201_menu screen: init, poll pad, run frames until the exit flag is set. */
void ysegfRunMenu(void)
{
    int exitFrames;
    int pressed;
    PadRecord *pad;
    YsegfGameWork *work;
    int held;

    ysegfExitFlag = 0;
    work = &gameWork;
    soundRestart();
    seBgmPlay(1, D_0068B438, 0x19000);
    ysegfInit();
    pad = &PadData[0];
    do {
        pressed = pad->pressed;
        if ((pressed & PAD_START) != 0) {
            held = pad->held;
            if ((held & PAD_SOFT_RESET_HELD) == PAD_SOFT_RESET_HELD) {
                ysegfExitFlag = 1;
                work->control |= YSEGF_CONTROL_SOFT_RESET;
            }
        }
        if ((pad->pressed & PAD_TRIANGLE) != 0) {
            seVoiceStartDefault(YSEGF_SE_CANCEL);
            ysegfExitFlag = 1;
        }
        if (ysegfExitFlag != 0) {
            break;
        }
        ysegfRunState(ysegfState);
        func_001BA68C();
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
    } while (1);
    ysegfReleaseBuffers();
    if ((gameWork.control & YSEGF_CONTROL_SOFT_RESET) == 0) {
        for (exitFrames = 9; exitFrames >= 0; exitFrames--) {
            func_001BA68C();
            XenoActorSetFlagAll(2);
            bgLayerClearAllFlags();
            XenoActorDoit();
            XenoActorDisp();
            xglSleep();
        }
        func_001E0E98();
    }
}

/* Resets the eighteen entry positions to 0xC8. */
void ysegfResetEntryPos(void)
{
    int lastEntryOffset;
    int *slot;
    int index;

    lastEntryOffset = 0x44;
    slot = (int *) (ysegfEntryPos + lastEntryOffset);
    for (index = 0; index < YSEGF_ENTRY_COUNT; index++) {
        *slot = YSEGF_ENTRY_START_POS;
        slot--;
    }
}

/* Draws the header row of eleven sprites (0x241-0x24B). */
void ysegfDrawListHeader(void) {
    packetSubmitRequest(0x241, 0x3B, 0xD, 1);
    packetSubmitRequest(0x242, 0x5E, 0xD, 1);
    packetSubmitRequest(0x243, 0x81, 0xD, 1);
    packetSubmitRequest(0x244, 0xA4, 0xD, 1);
    packetSubmitRequest(0x245, 0xC7, 0xD, 1);
    packetSubmitRequest(0x246, 0xEA, 0xD, 1);
    packetSubmitRequest(0x247, 0x10D, 0xD, 1);
    packetSubmitRequest(0x248, 0x14F, 0xD, 1);
    packetSubmitRequest(0x249, 0x172, 0xD, 1);
    packetSubmitRequest(0x24A, 0x195, 0xD, 1);
    packetSubmitRequest(0x24B, 0x1B8, 0xD, 1);
}

void msgRangeDrawRevealed(void *data, int count, int id);
/* List page enter/draw: header, treasure list, footer, then the list object. */
void ysegfDrawList(void) {
    fontObjResetAndBuild();
    ysegfDrawListHeader();
    ysegfDrawTreasureList();
    ysegfDrawFooter();
    msgRangeDrawRevealed(ysegfMenu, 0x18, 0x175);
}

/* Update of the list page (scene 0): eases the entry positions toward their targets, UP/DOWN moves
   the selection, cross leaves the menu, circle opens the detail page when the entry flag is set. */
extern float ysegfEntryEaseRate;
extern unsigned char D_0068B448[];

void ysegfUpdatePage(void)
{
    int index;
    int *entry;
    signed char selected;

    entry = (int *) ysegfEntryPos;
    selected = ysegfSelectedIndex;
    for (index = 0; index <= YSEGF_ENTRY_LAST; index++) {
        if (selected == index) {
            *entry += (int) ((16.0f - (float) *entry) * ysegfEntryEaseRate);
        } else if (index >= (signed char) ysegfScrollTop && index < (signed char) ysegfScrollTop + 3) {
            *entry += (int) ((30.0f - (float) *entry) * ysegfEntryEaseRate);
        } else {
            *entry += (int) ((512.0f - (float) *entry) * ysegfEntryEaseRate);
        }
        entry++;
    }
    ysegfListTrack.target_pair.y = -(signed char) ysegfScrollTop * YSEGF_ROW_HEIGHT;
    fontTrackEase(&ysegfListTrack);

    if ((PadData[0].pressed & PAD_CROSS) != 0) {
        ysegfExitFlag = 1;
        seVoiceStartDefault(YSEGF_SE_CANCEL);
    } else if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
        if (xglFlagsGet(ysegfSelectedIndex + YSEGF_FLAG_BASE, 1) != 0) {
            msgRangeSetText(ysegfMenu, D_0068B448);
            ysegfSwitchState(&ysegfState, &ysegfStateTable[YSEGF_SCENE_DETAIL]);
            ysegfItemFadeAlpha = 0;
            seVoiceStartDefault(YSEGF_SE_DECIDE);
            return;
        }
        seVoiceStartDefault(YSEGF_SE_DENIED);
        return;
    }

    if ((PadData[0].directionRepeated & PAD_UP) != 0) {
        int nextIndex = (unsigned char) ysegfSelectedIndex - 1;
        if ((signed char) nextIndex < 0) {
            ysegfSelectedIndex = 0;
        } else {
            ysegfSelectedIndex = nextIndex;
            seVoiceStartDefault(YSEGF_SE_CURSOR_MOVE);
        }
    }
    if ((PadData[0].directionRepeated & PAD_DOWN) != 0) {
        int nextIndex = (unsigned char) ysegfSelectedIndex + 1;
        if ((signed char) nextIndex >= YSEGF_ENTRY_COUNT) {
            ysegfSelectedIndex = YSEGF_ENTRY_LAST;
        } else {
            ysegfSelectedIndex = nextIndex;
            seVoiceStartDefault(YSEGF_SE_CURSOR_MOVE);
        }
    }

    while (ysegfSelectedIndex < (signed char) ysegfScrollTop) {
        ysegfScrollTop = (signed char) ysegfScrollTop - 1;
    }
    while ((signed char) ysegfScrollTop + 2 < ysegfSelectedIndex) {
        ysegfScrollTop = (signed char) ysegfScrollTop + 1;
    }
}

void ysegfListLeaveNoop(void)
{
}

void fadeStartIn(int value);

void ysegfStartFadeIn(void) {
    fadeStartIn(0xA);
}

/* Detail page draw: treasure detail, item explanation, footer, list object. */
void ysegfDrawDetail(void) {
    fontObjResetAndBuild();
    ysegfDrawTreasureDetail(0xC, 0xF, ysegfSelectedIndex, ysegfSelectedIndex, 0x80);
    ysegfDrawItemPage();
    ysegfDrawFooter();
    msgRangeDrawRevealed(ysegfMenu, 0x18, 0x175);
}

/* Returns the nearest entry below the given index whose flag (0x1C1 + i) is set, wrapping. */
int ysegfFindPrevEntry(signed char index) {
    signed char candidate = index;

    candidate--;
    goto test1;
found1:
    return (signed char) candidate;
loop1:
    if (xglFlagsGet(YSEGF_FLAG_BASE + candidate, 1) == 0) {
        candidate--;
    } else {
        goto found1;
    }
test1:
    if (candidate >= 0) {
        goto loop1;
    }
    candidate = YSEGF_ENTRY_LAST;
    while (1) {
        if (index == candidate) {
            return (signed char) candidate;
        }
        if (xglFlagsGet(YSEGF_FLAG_BASE + candidate, 1) != 0) {
            return (signed char) candidate;
        }
        candidate--;
    }
}

/* Returns the nearest entry above the given index whose flag (0x1C1 + i) is set, wrapping. */
int ysegfFindNextEntry(int index)
{
    int candidate;
    int startIndex;

    candidate = (signed char)index;
    startIndex = candidate;
    candidate = (signed char)(startIndex + 1);
    if (candidate >= YSEGF_ENTRY_COUNT) {
        candidate = 0;
    } else {
        do {
            if (xglFlagsGet(candidate + YSEGF_FLAG_BASE, 1) != 0) {
                return candidate;
            }
            candidate = (signed char)(candidate + 1);
        } while (candidate < YSEGF_ENTRY_COUNT);
        candidate = 0;
    }
    for (;;) {
        if (startIndex == candidate) {
            return candidate;
        }
        if (xglFlagsGet(candidate + YSEGF_FLAG_BASE, 1) != 0) {
            return candidate;
        }
        candidate = (signed char)(candidate + 1);
    }
}

/* Update of the detail page (scene 1): cross returns to the list page, L1/LEFT and R1/RIGHT step to the
   previous and next entry. */
void ysegfUpdateDetail(void) {
    signed char previous;

    if ((PadData[0].pressed & PAD_CROSS) != 0) {
        msgRangeSetText(ysegfMenu, D_0068B258);
        ysegfSwitchState(&ysegfState, ysegfStateTable);
        seVoiceStartDefault(YSEGF_SE_CANCEL);
        return;
    }
    if ((PadData[0].directionRepeated & (PAD_LEFT | PAD_L1)) != 0) {
        previous = ysegfSelectedIndex;
        ysegfSelectedIndex = ysegfFindPrevEntry(previous);
        if (previous != ysegfSelectedIndex) {
            ysegfItemFadeAlpha = 0;
        }
        seVoiceStartDefault(YSEGF_SE_CURSOR_MOVE);
    }
    if ((PadData[0].directionRepeated & (PAD_RIGHT | PAD_R1)) != 0) {
        previous = ysegfSelectedIndex;
        ysegfSelectedIndex = ysegfFindNextEntry(previous);
        if (previous != ysegfSelectedIndex) {
            ysegfItemFadeAlpha = 0;
        }
        seVoiceStartDefault(YSEGF_SE_CURSOR_MOVE);
    }
}
