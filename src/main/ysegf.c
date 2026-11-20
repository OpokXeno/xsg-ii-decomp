#include "common.h"
#include "main/xenoActor.h"
#include "shared.h"
#include "main/assert_fail.h"
#include "main/xgl_cd_arena.h"
#include "main/font_obj.h"
#include "main/text_window.h"
#include "main/se_bank_load.h"
#include "main/menu_draw.h"
#include "main/xgl_cd.h"
#include "main/yg2path.h"
#include "main/ysegf.h"
#include "main/cfeFileControl.h"
#include "main/game_camera.h"
#include "main/game_data.h"
#include "ov01/data_box.h"
#include "main/xgl_font.h"
#include "main/fade_ctrl.h"
#include "main/xgl_packet.h"
#include "main/render_sprite.h"
#include "main/bg_layer.h"
#include "main/nml_model_set.h"
#include "main/res_get_path.h"

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
   a draw callback queued with xglCallbackQueueAdd on every run, a finish hook called last, and hook_end
   run when the state is left. */
typedef struct YsegfSceneState {
    int initialized;            /* +0x00: 0 until the start hook has run */
    void (*start)(void);        /* +0x04 */
    void (*draw)(void);         /* +0x08: queued with xglCallbackQueueAdd on every run */
    void (*finish)(void);       /* +0x0C */
    void (*hook_end)();         /* +0x10: called with the state slot (ysegfState) */
} YsegfSceneState;

/* Runs one frame of a scene state: start hook once, then the queued handle, then the finish hook. */
void ysegfRunState(YsegfSceneState *record)
{
    void (*draw)(void);

    if (record->initialized == 0) {
        record->start();
        record->initialized = 1;
    }
    draw = record->draw;
    if (draw != 0) {
        xglCallbackQueueAdd(draw);
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

int segGetAddrOn(u32 index)
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

extern const unsigned char ysegfSourceName[];
extern unsigned char ysegfSizeAssertText[];

/* Allocates a buffer sized for the named CD file. The size comes from the
 * directory lookup xglCdGetFileSize; a zero size asserts "size" in ../ysegf.euc.c. */
void *ysegfAllocFileBuffer(const char *name) {
    int fileSize;

    fileSize = xglCdGetFileSize(name);
    if (fileSize == 0) {
        assertFailed(ysegfSizeAssertText, ysegfSourceName, 0x123);
    }
    return xglCdBufferReserve(fileSize);
}

/* Reads a CD file into *buffer, allocating the buffer first when allocate is set. */
void ysegfReadFile(void **buffer, const char *name, int allocate, int mode) {
    if (allocate != 0) {
        *buffer = ysegfAllocFileBuffer(name);
    }
    xglCdReadFile(name, *buffer, mode, CD_NOTIFY_CURRENT);
}

#include "main/pad.h"
#include "main/xgl_flags.h"
#include "main/xgl_thread.h"
#include "main/txd_table_entry.h"

/* One 169-byte treasure record: the three tagged texts that ysegfLoadTreasure reads (offsets from the record start). */
typedef struct YsegfTreasure {
    unsigned char item[21];          /* +0x00: item name (tagItem) */
    unsigned char location[27];      /* +0x15: location text (tagLocate) */
    unsigned char explanation[121];  /* +0x30: explanation text (tagExplanation) */
} YsegfTreasure;

static YsegfSceneState *ysegfState;
static YsegfTreasure *ysegfTreasureTable;
static signed char ysegfSelectedIndex;
static signed char ysegfScrollTop;
static int ysegfExitFlag;
static YsegfSceneState ysegfStateTable[2];
static unsigned char ysegfEntryPos[72];
static struct FontTrack ysegfListTrack;
static MsgRange ysegfMenu;

static TextWindow textWindow;
extern const unsigned char ysegfItemTagFormat[];
extern const unsigned char ysegfLocateTagFormat[];
extern const unsigned char ysegfExplanationTagFormat[];
extern const unsigned char ysegfTreasureFilePath[];
extern unsigned char ysegfTextBufferAssertText[];
void *memset(void *dest, int value, unsigned int count);

/* Loads the treasure table: clears the 3042-byte buffer, reads the tag file, then reads
 * three tagged texts into each of the eighteen 169-byte records. */
void ysegfLoadTreasure(void) {
    void *textCursor;
    int index;
    const unsigned char *tagItem = ysegfItemTagFormat;
    const unsigned char *tagLocate = ysegfLocateTagFormat;
    const unsigned char *tagExplanation = ysegfExplanationTagFormat;

    textCursor = 0;
    ysegfTreasureTable = xglCdBufferReserve(YSEGF_TREASURE_TABLE_SIZE);
    memset(ysegfTreasureTable, 0, YSEGF_TREASURE_TABLE_SIZE);
    while (xglCdServiceIfPending() != 0) {
    }
    ysegfReadFile(&textCursor, ysegfTreasureFilePath, 1, 0);
    if (textCursor == 0) {
        assertFailed(ysegfTextBufferAssertText, ysegfSourceName, 328);
    }
    for (index = 0; index < YSEGF_ENTRY_COUNT; index++) {
        textCursor = yg2PathReadTaggedText(textCursor, tagItem, ysegfTreasureTable[index].item);
        textCursor = yg2PathReadTaggedText(textCursor, tagLocate, ysegfTreasureTable[index].location);
        textCursor = yg2PathReadTaggedText(textCursor, tagExplanation, ysegfTreasureTable[index].explanation);
    }
}

/* Clears the two signed cursor bytes of the item page. */
void ysegfResetSelection(void)
{
    ysegfSelectedIndex = 0;
    ysegfScrollTop = 0;
}

void ysegfResetEntryPos(void);
void ysegfDrawList(void);
void ysegfUpdatePage(void);
void ysegfStartFadeIn(void);
void ysegfDrawDetail(void);
void ysegfUpdateDetail(void);
void ysegfListLeaveNoop(void);
extern const unsigned char ysegfScrollHelpText[];
extern const unsigned char ysegfItemImageName[];
extern const unsigned char ysegfItemAlphaImageName[];
extern const unsigned char ysegfDoorImageName[];
extern const unsigned char ysegfDoorAlphaImageName[];
extern const unsigned char ysegfItemAlphaAssertText[];
extern const unsigned char ysegfDoorAlphaAssertText[];
extern unsigned char ysegfItemFadeAlpha;
extern unsigned char ysegfItemImageAssertText[];
extern unsigned char ysegfDoorImageAssertText[];
extern void *ysegfDoorImage;
extern void *ysegfItemImage;
extern void *ysegfDoorAlpha;
extern void *ysegfItemAlpha;


/* Scene init: resets the menu and font state, loads the treasure data and the item and door images. */
void ysegfInit(void)
{
    int index;
    int lastEntryOffset;
    int *slot;
    YsegfSceneState *scene;

    menuResetState(&ysegfMenu);
    msgRangeSetText(&ysegfMenu, ysegfScrollHelpText);
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
    scene[YSEGF_SCENE_LIST].draw = ysegfDrawList;
    scene[YSEGF_SCENE_LIST].finish = ysegfUpdatePage;
    scene[YSEGF_SCENE_LIST].hook_end = ysegfListLeaveNoop;
    scene[YSEGF_SCENE_DETAIL].start = ysegfStartFadeIn;
    scene[YSEGF_SCENE_DETAIL].draw = ysegfDrawDetail;
    scene[YSEGF_SCENE_DETAIL].finish = ysegfUpdateDetail;
    scene[YSEGF_SCENE_DETAIL].hook_end = ysegfDetailLeaveNoop;
    scene[YSEGF_SCENE_DETAIL].initialized = 0;
    ysegfState = scene;
    scene[YSEGF_SCENE_LIST].initialized = 0;
    ysegfItemFadeAlpha = 0;
    ysegfItemImage = 0;
    ysegfItemAlpha = 0;
    ysegfDoorImage = 0;
    ysegfDoorAlpha = 0;
    ysegfItemImage = ysegfAllocFileBuffer(ysegfItemImageName);
    ysegfReadFile(&ysegfItemAlpha, ysegfItemAlphaImageName, 1, 0);
    ysegfReadFile(&ysegfDoorImage, ysegfDoorImageName, 1, 0);
    ysegfReadFile(&ysegfDoorAlpha, ysegfDoorAlphaImageName, 1, 0);
    if (ysegfItemImage == 0) {
        assertFailed(ysegfItemImageAssertText, ysegfSourceName, 403);
    }
    if (ysegfItemAlpha == 0) {
        assertFailed(ysegfItemAlphaAssertText, ysegfSourceName, 404);
    }
    if (ysegfDoorImage == 0) {
        assertFailed(ysegfDoorImageAssertText, ysegfSourceName, 405);
    }
    if (ysegfDoorAlpha == 0) {
        assertFailed(ysegfDoorAlphaAssertText, ysegfSourceName, 406);
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
    menuDrawCell(0x253, x, y, 0);
    menuDrawCell(0x256, x, y, 1);
    if (flag != 0) {
        menuDrawCell(0x266, x + 0x17, y + 0x18, 0);
        menuDrawCell(0x267, x + 0x10, y + 0x10, 1);
    }
}

/* Draws a window frame: four corner sprites and two edge strips (sprite 0x236). */
void ysegfDrawWindow(int x, int y, int width, int height)
{
    int right;
    int bottom;

    menuDrawCell(0x237, x, y, 0);
    right = (x + width) - 0x10;
    bottom = (y + height) - 0x10;
    menuDrawCell(0x238, right, y, 0);
    menuDrawCell(0x234, x, bottom, 0);
    menuDrawCell(0x235, right, bottom, 0);
    menuDrawCellSized(0x236, x, y + 0x10, width, height - 0x20, 0x100);
    menuDrawCellSized(0x236, x + 0x10, y, width - 0x20, height, 0x100);
}

/* Draws a panel frame of eight pieces (sprites 0x239-0x240) around x, y, width, height. */
void ysegfDrawPanel(int x, int y, int width, int height)
{
    int right;
    int bottom;

    menuDrawCell(0x23E, x, y, 1);
    right = (x + width) - 0x10;
    menuDrawCell(0x240, right, y, 1);
    bottom = (y + height) - 0x11;
    menuDrawCell(0x239, x, bottom, 1);
    menuDrawCell(0x23B, right, bottom, 1);
    menuDrawCellSized(0x23F, x + 0x10, y, width - 0x20, 0x10, 1);
    menuDrawCellSized(0x23A, x + 0x10, bottom, width - 0x20, 0x11, 1);
    menuDrawCellSized(0x23C, x, y + 0x10, 0x10, height - 0x21, 1);
    menuDrawCellSized(0x23D, right, y + 0x10, 0x10, height - 0x21, 1);
}

/* Same frame as ysegfDrawPanel with the sprite set 0x272-0x279. */
void ysegfDrawImageFrame(int x, int y, int width, int height)
{
    int right;
    int bottom;

    menuDrawCell(0x277, x, y, 1);
    right = (x + width) - 0x10;
    bottom = (y + height) - 0x11;
    menuDrawCell(0x279, right, y, 1);
    menuDrawCell(0x272, x, bottom, 1);
    menuDrawCell(0x274, right, bottom, 1);
    menuDrawCellSized(0x278, x + 0x10, y, width - 0x20, 0x10, 1);
    menuDrawCellSized(0x273, x + 0x10, bottom, width - 0x20, 0x10, 1);
    menuDrawCellSized(0x275, x, y + 0x10, 0x10, height - 0x21, 1);
    menuDrawCellSized(0x276, right, y + 0x10, 0x10, height - 0x21, 1);
}

/* Draws the two footer sprites 0x270 and 0x271 at (0, 0x170). */
void ysegfDrawFooter(void) {
    menuDrawCell(0x271, 0, 0x170, 1);
    menuDrawCell(0x270, 0, 0x170, 1);
}

extern int ysegfTreasureSelectedPanelImage[];
extern int ysegfTreasureRowPanelImage[];
extern const unsigned char ysegfDetailTextHeader[];
extern const unsigned char ysegfLocationTitleText[];
extern const unsigned char ysegfTreasureTitleText[];
extern unsigned char ysegfNoDataShadowText[];
extern unsigned char ysegfAddressTitleText[];
extern unsigned char ysegfDecoderTitleText[];
extern void *ysegfNoDataText;

/* Draws one segment card (a row of the list, or the header of the detail page) for the record at index of ysegfTreasureTable. */
void ysegfDrawTreasureCard(int x, int y, int index, int selected, int alpha)
{
    int frameX;
    int displayNumber;
    int labelY;
    int addressOn;
    int decoderFlag;
    int treasureFound;
    XglTextureTransSet *doorTexture;
    int *doorEntry;

    if (index == selected) {
        panelDrawNineSlice(ysegfTreasureSelectedPanelImage, x, y, 0x1D0, 0x64);
    } else {
        panelDrawNineSlice(ysegfTreasureRowPanelImage, x, y, 0x1D0, 0x64);
    }
    frameX = x + 0xC;
    displayNumber = index + 1;
    menuDrawCell(0x26A, frameX, y - 1, 1);
    labelY = y + 0x15;
    textDrawDigitsWide(x + 0x36, y, displayNumber, 2);
    menuDrawFixedFrame(x + 0x5D, labelY);
    menuDrawCell(0x252, x + 0x69, y + 0x1F, 0);
    menuDrawCell(0x255, x + 0x69, y + 0x1F, 1);
    menuDrawCell(0x252, x + 0x69, y + 0x3E, 0);
    menuDrawCell(0x255, x + 0x69, y + 0x3E, 1);
    textDrawSegment(0, 0, ysegfDetailTextHeader, 0);
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
        textDrawSegment(x + 0x71, y + 0x22, ysegfNoDataText, 4);
    }

    if (treasureFound != 0) {
        textDrawSegment(x + 0x71, y + 0x3F, ysegfTreasureTable[index].item, 4);
        menuDrawCell(0x24F, x + 0x120, y + 0x4A, 1);
    } else if (addressOn != 0) {
        textDrawSegment(x + 0x71, y + 0x40, ysegfNoDataText, 4);
        menuDrawCell(0x24E, x + 0x120, y + 0x4A, 1);
    } else if (decoderFlag != 0) {
        textDrawSegment(x + 0x71, y + 0x40, ysegfNoDataText, 4);
        menuDrawCell(0x24E, x + 0x120, y + 0x4A, 1);
    } else {
        textDrawSegment(x + 0x71, y + 0x40, ysegfNoDataText, 4);
        menuDrawCell(0x24D, x + 0x12E, y + 0x4A, 1);
    }

    if (addressOn != 0) {
        doorEntry = (int *) ysegfDoorImage + index;
        doorTexture = (XglTextureTransSet *) ((unsigned char *) ysegfDoorImage + doorEntry[1]);
        xglPacketTextureTrans(ysegfDoorAlpha);
        renderSpriteClutAlpha(frameX, y + 0x17, 0x4C, 0x41, 0x20, alpha);
        xglPacketTextureTrans(doorTexture);
        renderSpriteClut(frameX, y + 0x17, 0x4C, 0x41, 0x20);
        fontObjBuildBaseVifPacket();
        ysegfDrawPanel(frameX, y + 0x17, 0x4C, 0x41);
    } else {
        ysegfDrawWindow(frameX, y + 0x17, 0x4C, 0x41);
        menuDrawCell(0x250, frameX, y + 0x17, 1);
        ysegfDrawPanel(frameX, y + 0x17, 0x4C, 0x41);
        fontObjSetColor(0x3F80000060000000ULL);
        fontObjSetColor(0x3F80000080808080ULL);
        textDrawShadow(x + 0x13, y + 0x30, ysegfNoDataShadowText, 0x3F80000000000000ULL);
        fontObjResetColor();
    }

    textDrawTitle(0, x + 0x6C, labelY, ysegfLocationTitleText);
    textDrawTitle(0, x + 0x6C, y + 0x34, ysegfTreasureTitleText);
    textDrawTitle(0, x + 0x109, y + 0x16, ysegfAddressTitleText);
    textDrawTitle(0, x + 0x14A, y + 0x16, ysegfDecoderTitleText);
    textDrawTitle(0, x + 0x187, y + 0x16, ysegfTreasureTitleText);
}


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

    menuDrawCell(0x24C, 0x40, 0x22, 1);
    menuDrawCellSized(0x24C, 0x1D3, -0x80, 1, 0x240, 0x81);
    menuDrawCellSized(0x24C, 0x1D2, -0x80, 1, 0x240, 0x81);
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
            ysegfDrawTreasureCard(x, y, row, ysegfSelectedIndex, alpha);
            fontObjResetOpacity();
        }
    }
    if (ysegfScrollTop > 0) {
        menuDrawCell(0x26C, 0x1DD, 0x23, 1);
    }
    if (ysegfScrollTop < 15) {
        menuDrawCell(0x26B, 0x1DD, 0x160, 1);
    }
    xglFontFlush();
    fontObjResetAndBuild();
}

extern struct FontDrawItem ysegfItemNameDraw;

/* Draws the item-name frame: four font cells across the top row, then clears the colour. */
void ysegfDrawItemNameFrame(int x, int y, int width, int height, const unsigned char *labelText)
{
    int inner_width;

    menuDrawShortOutlineFrame(x, y, width, height);
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

extern int ysegfItemFramePanelImage[];
extern const unsigned char ysegfExplanationTitleText[];
extern const unsigned char ysegfItemPageTextHeader[];
extern const unsigned char ysegfItemImagePathPrefix[];
extern unsigned char ysegfItemImageZeroPadFormat[];
extern unsigned char ysegfItemImageNumberFormat[];
extern unsigned char ysegfImageSuffixText[];
void sprintf(void *dst, void *format, ...);
void strcat(void *dst, void *format);

/* Draws the item explanation page: the panel and labels, the selected item's
 * text, then the item image, which fades in when it is loaded or loads its file. */
void ysegfDrawItemPage(void)
{
    char fileName[64];

    panelDrawNineSlice(ysegfItemFramePanelImage, 47, 120, 417, 243);
    menuDrawCell(0x27F, 72, 0x77, 1);
    menuDrawCell(0x228, 55, 0x8F, 1);
    menuDrawCell(0x232, 0x1B8, 0x8F, 0);
    menuDrawCell(0x22A, 0x1B8, 0x8F, 1);
    menuDrawCellSized(0x229, 71, 0x8F, 0x171, 16, 1);
    menuDrawCellSized(0x227, 55, 0x9F, 16, 0xAE, 1);
    menuDrawCellSized(0x231, 0x1B8, 0x9F, 16, 0xAE, 0);
    menuDrawCell(0x22E, 55, 0x14D, 0);
    menuDrawCell(0x226, 55, 0x14D, 1);
    menuDrawCell(0x230, 0x1B8, 0x14D, 0);
    menuDrawCellSized(0x22F, 71, 0x14D, 0x171, 16, 0);
    ysegfDrawItemNameFrame(75, 163, 206, 165, ysegfExplanationTitleText);
    menuDrawOutlineFrame(294, 153, 147, 165);
    menuDrawOutlineFrame(69, 153, 216, 184);
    menuDrawCellSized(0x27A, 73, 0xD1, 0xD2, 5, 1);
    menuDrawCellSized(0x27A, 73, 0xE5, 0xD2, 5, 1);
    menuDrawCellSized(0x27A, 73, 0xF9, 0xD2, 5, 1);
    menuDrawCellSized(0x27A, 73, 0x10D, 0xD2, 5, 1);
    menuDrawCellSized(0x27A, 73, 0x121, 0xD2, 5, 1);
    menuDrawCellSized(0x27A, 73, 0x135, 0xD2, 5, 1);
    menuDrawCell(0x26F, 14, 0xC3, 1);
    menuDrawCell(0x26F, 0x1DA, 0xC3, 1);
    menuDrawCell(0x26D, 14, 0xC3, 1);
    menuDrawCell(0x26E, 0x1DA, 0xC3, 1);
    menuDrawCell(0x1C, 0x15E, 0x14A, 0x40);
    textDrawDigitsPair(390, 330, ysegfSelectedIndex + 1, 18, 2);
    textDrawSegment(0, 0, ysegfItemPageTextHeader, 0);
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
            sprintf(fileName, ysegfItemImageZeroPadFormat, ysegfItemImagePathPrefix, selected);
        } else {
            sprintf(fileName, ysegfItemImageNumberFormat, ysegfItemImagePathPrefix, selected);
        }
        strcat(fileName, ysegfImageSuffixText);
        ysegfReadFile(&ysegfItemImage, fileName, 0, 1);
        ysegfItemFadeAlpha = 16;
    }
    fontObjBuildBaseVifPacket();
}

extern const unsigned char ysegfBgmName[];


/* Scene loop of the segment address screen (its music is 201_menu): init, poll pad, run frames until the exit flag is set. */
void ysegfRunMenu(void)
{
    int exitFrames;
    int pressed;
    PadRecord *pad;
    int held;
    GameWork *work; /* a local pointer to gameWork: the soft-reset store goes through it */

    ysegfExitFlag = 0;
    work = &gameWork;
    soundRestart();
    seBgmPlay(1, ysegfBgmName, 0x19000);
    ysegfInit();
    pad = &PadData[0];
    do {
        pressed = pad->pressed;
        if ((pressed & PAD_START) != 0) {
            held = pad->held;
            if ((held & PAD_SOFT_RESET_HELD) == PAD_SOFT_RESET_HELD) {
                ysegfExitFlag = 1;
                work->control |= CONTROL_SOFT_RESET;
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
        pxyRequestOverlayPass();
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
    } while (1);
    ysegfReleaseBuffers();
    if ((gameWork.control & CONTROL_SOFT_RESET) == 0) {
        for (exitFrames = 9; exitFrames >= 0; exitFrames--) {
            pxyRequestOverlayPass();
            XenoActorSetFlagAll(2);
            bgLayerClearAllFlags();
            XenoActorDoit();
            XenoActorDisp();
            xglSleep();
        }
        seStateRestoreDefault();
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
    menuDrawCell(0x241, 0x3B, 0xD, 1);
    menuDrawCell(0x242, 0x5E, 0xD, 1);
    menuDrawCell(0x243, 0x81, 0xD, 1);
    menuDrawCell(0x244, 0xA4, 0xD, 1);
    menuDrawCell(0x245, 0xC7, 0xD, 1);
    menuDrawCell(0x246, 0xEA, 0xD, 1);
    menuDrawCell(0x247, 0x10D, 0xD, 1);
    menuDrawCell(0x248, 0x14F, 0xD, 1);
    menuDrawCell(0x249, 0x172, 0xD, 1);
    menuDrawCell(0x24A, 0x195, 0xD, 1);
    menuDrawCell(0x24B, 0x1B8, 0xD, 1);
}

/* List page enter/draw: header, treasure list, footer, then the list object. */
void ysegfDrawList(void) {
    fontObjResetAndBuild();
    ysegfDrawListHeader();
    ysegfDrawTreasureList();
    ysegfDrawFooter();
    msgRangeDrawRevealed(&ysegfMenu, 0x18, 0x175);
}

/* Update of the list page (scene 0): eases the entry positions toward their targets, UP/DOWN moves
   the selection, cross leaves the menu, circle opens the detail page when the entry flag is set. */
extern const float ysegfEntryEaseRate;
extern const unsigned char ysegfPageSwitchHelpText[];

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
            msgRangeSetText(&ysegfMenu, ysegfPageSwitchHelpText);
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


void ysegfStartFadeIn(void) {
    fadeStartIn(0xA);
}

/* Detail page draw: treasure detail, item explanation, footer, list object. */
void ysegfDrawDetail(void) {
    fontObjResetAndBuild();
    ysegfDrawTreasureCard(0xC, 0xF, ysegfSelectedIndex, ysegfSelectedIndex, 0x80);
    ysegfDrawItemPage();
    ysegfDrawFooter();
    msgRangeDrawRevealed(&ysegfMenu, 0x18, 0x175);
}

/* Returns the previous entry (the next lower index) whose flag (0x1C1 + i) is set, wrapping. */
int ysegfFindPrevEntry(signed char index) {
    signed char candidate = index;

    /* The gotos are kept: the original loop is not rotated (entry branch to the bottom test, found-return block above
       the body); while/for forms compile rotated (bltz entry test, bgez back edge). */
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

/* Returns the next entry (the next higher index) whose flag (0x1C1 + i) is set, wrapping. */
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
        msgRangeSetText(&ysegfMenu, ysegfScrollHelpText);
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

const unsigned char ysegfSourceName[16] = "../ysegf.euc.c";
const unsigned char ysegfItemTagFormat[16] = "Item:%s\n";
const unsigned char ysegfLocateTagFormat[16] = "Locate:%s\n";
const unsigned char ysegfExplanationTagFormat[16] = "Expl:%s\n";
const unsigned char ysegfTreasureFilePath[24] = "iwase\\Treasure.txt";
const unsigned char ysegfScrollHelpText[112] = "Scroll with the directional buttons or the left analog stick.\\nPress &03 for details. Press &04 to go back.";
const unsigned char ysegfItemImageName[32] = "iwase\\img\\item\\item00.xtx";
const unsigned char ysegfItemAlphaImageName[32] = "iwase\\img\\item\\item_alpha.xtx";
const unsigned char ysegfDoorImageName[32] = "iwase\\img\\door\\doorimg.dat";
const unsigned char ysegfDoorAlphaImageName[32] = "iwase\\img\\door\\door_alpha.xtx";
const unsigned char ysegfItemAlphaAssertText[16] = "imgItemAlpha";
const unsigned char ysegfDoorAlphaAssertText[16] = "imgDoorAlpha";
const unsigned char ysegfDetailTextHeader[72] = "$init;$bg2;$shadow+1,+3,000000;$colc0c0c0;$zoom12;\000\000\000\000\000\000NO  DATA";
const unsigned char ysegfLocationTitleText[16] = "Location";
const unsigned char ysegfTreasureTitleText[16] = "Treasure";
const unsigned char ysegfExplanationTitleText[16] = "Explanation";
const unsigned char ysegfItemPageTextHeader[64] = "$init;$bg2;$shadow+1,+3,000000;$colc0c0c0;$zoom12;$xx0,20;";
const unsigned char ysegfItemImagePathPrefix[24] = "iwase\\img\\item\\item";
const unsigned char ysegfBgmName[16] = "201_menu";
const unsigned char ysegfPageSwitchHelpText[72] = "Use the L1 and R1 buttons to switch pages.\\nPress &04 to go back.";

void * ysegfDoorImage = 0;
void * ysegfItemImage = 0;
void * ysegfDoorAlpha = 0;
void * ysegfItemAlpha = 0;
unsigned char ysegfItemFadeAlpha = 0;
unsigned char ysegfSizeAssertText[8] = "size";
unsigned char ysegfTextBufferAssertText[8] = "pBuf";
unsigned char ysegfItemImageAssertText[8] = "imgItem";
unsigned char ysegfDoorImageAssertText[8] = "imgDoor";
void *ysegfNoDataText = (void *) ((char *) ysegfDetailTextHeader + 0x38);
unsigned char ysegfNoDataShadowText[8] = "NO DATA";
unsigned char ysegfAddressTitleText[8] = "Address";
unsigned char ysegfDecoderTitleText[8] = "Decoder";
unsigned char ysegfItemImageZeroPadFormat[8] = "%s0%d";
unsigned char ysegfItemImageNumberFormat[8] = "%s%d";
unsigned char ysegfImageSuffixText[8] = ".xtx";

int ysegfTreasureRowPanelImage[10] = { 0x25e, 0x25f, 0x260, 0x261, 0x21a, 0x262, 0x263, 0x264, 0x265, 0x0 };
int ysegfTreasureSelectedPanelImage[10] = { 0x258, 0x259, 0x25a, 0x219, 0x21a, 0x21b, 0x25b, 0x25c, 0x25d, 0x0 };
int ysegfItemFramePanelImage[10] = { 0x21f, 0x220, 0x221, 0x219, 0x21a, 0x21b, 0x27c, 0x27d, 0x27e, 0x0 };
struct FontDrawItem ysegfItemNameDraw = { .cell_index = 0, .x = 0, .y = 0, .offset_x = 0, .offset_y = 0, .size_w = 0, .size_h = 0, .flags = 0 };
signed char segAddrOnFlags[YSEGF_ENTRY_COUNT] = { 0 };

const float ysegfEntryEaseRate = 0.300000012f;
