#include "common.h"
#include "main/yg2path.h"
#include "main/xgl_render.h"
#include "shared.h"
#include "main/xgl_cd_arena.h"
#include "main/bg_layer.h"
#include "main/xgl_tick.h"
#include "main/font_obj.h"
#include "main/cursor_buffer.h"
#include "main/menu_scene.h"
#include "main/menu_draw.h"
#include "main/ysegf.h"
#include "main/se_bank_load.h"
#include "main/menu_selection.h"
#include "main/pt_member.h"
#include "main/txd_table_entry.h"
#include "main/xgl_cd.h"
#include "main/xgl_camera.h"
#include "gs.h"
#include "main/pad.h"
#include "main/save_party_init.h"
#include "main/xenoActor.h"
#include "main/sound_driver.h"
#include "main/nml_model_set.h"
#include "main/xgl_2.h"
#include "main/xgl_studio.h"
#include "main/xgl_font.h"
#include "main/pxy_packet_state.h"
#include "main/xgl_thread.h"
#include "main/gr_screen.h"
#include "main/game_data.h"
#include "ov01/data_record_get.h"
#include "ov01/data_box.h"
#include "main/vibration.h"
#include "main/xgl_pad.h"
#include "main/xgl_jpeg.h"
#include "main/fade_ctrl.h"
#include "main/tya_display_setting.h"
#include "main/game_over_095.h"
#include "ov01/calc.h"
#include "main/xgl_packet.h"

/* Script strings of the text draw commands (0x0068A3F0, .rodata, 80 bytes with zero fill). */
const char menuTextInitMarkup[80] = "$init;$bg2;$shadow+1,+3,000000;$zoom13;$han1;$prop1;$xx0,21;$colc0c0c0;";

/* Uninitialized static data of this TU (.bss 0x0069A4860..0x0069A4900 area): matrix and vector scratch objects used by menuModelDraw and the menu scene; mainCommandCursors holds the two cursors of the main command cards. */
static Matrix4 menuModelDrawMatrix;
static Vector4 menuModelDrawVector;
static Matrix4 menuModelRotatedMatrix[1];
static Vector4 menuModelRotatedVector[1];
static ListCursor mainCommandCursors[2];

struct ControlSlot;

typedef void (*ControlCallback)(struct ControlSlot *slot);
typedef int (*ControlStep)(struct ControlSlot *slot);

typedef struct ControlSlot {
    ControlCallback callback;   /* 0x00: called by menuSlotDraw */
    /* The step is typed as the draw callback on purpose: a distinct function pointer type changes the alias sets and
       reorders the stores of menuSceneInit. The step functions return int, so their table assignments warn. */
    ControlCallback step;       /* 0x04: the slot's state-machine step, run every frame by menuSceneRunLoop */
    ListCursor *link;           /* 0x08: the cursor (or table of cursors) the slot reads */
    unsigned short value;       /* 0x0C: written when flag is set */
    u8 state;                   /* 0x0E */
    u8 flag;                    /* 0x0F */
    u8 flags;                   /* 0x10: bit 1 set here */
    u8 unmodeled_11[3];
} ControlSlot;

/* Uninitialized static data of this TU in address order (.bss 0x006A4928..0x006A5510). */
static ListCursor menuTabCursor[1];
static MenuBlock mainMenuBlock;
static MenuBlock itemMenuBlock;
static FontTrack extraPanelTrack;
static FontTrack equipPanelTrack[1];
static ListCursor windowListCursor[1];
static ListCursor recordItemCursor[1];
static ListCursor learnedEtherCursor[1];
static ListCursor commandPageCursor[1];
static ListCursor equipSlotCursor[1];
static ListCursor equipItemCursor[1];
static ListCursor skillBoardCursorTable[3];
static ListCursor menuConfigCursor[1];
static ListCursor mapChoiceCursor[1];
static MsgRange menuMessageRangeState[1];
static void *menuSkillModelPxyBuffers[12];
static void *menuSkillModelTxyBuffers[12];
static u8 menuSkillModelLoadTally[16];
static float menuRingColumnAngle[4];
static u8 equipItemList[128];
static FontTrack menuBandTrack[1];
static u8 menuTutorialSavedCells[32];
static ControlSlot menuSlots[11];
static void *menuUmnImageBuffers[8];
static FontTrack itemListRowTracks[4];

/* Initialized small data of this TU's functions (0x0069D4C8..0x0069D514, .sdata). Values from the original bytes, types from the existing extern declarations. */
int menuLicenseState = 0;
ListCursor *menuCurrentLink = 0;
int menuCurrentSlot = 0;
int menuPreviousCardCursor = 0;
int menuSceneFlags = 0;
unsigned char menuCopilotModeFlag = 0;
int menuChoiceOpenFlag = 0;
int countdownActive = 0;
signed char menuCursorMemory = 0;
signed char menuCursorMemoryAlt = 0;
signed char menuConfigButtonIndexTable[] = { 0, 2, 0, 0, 0, 0, 0, 0 };
signed char menuMapChoiceDefaultTable[8] = { 2, 0, 0, 0, 1, 1, 0, 2 };
signed char menuMapChoice = 0;
signed char menuChoiceAction = 0;
signed char menuKeyCheckPending = 0;
unsigned char menuCardAvailabilityBits = 0;
unsigned char menuEquipRowCount = 4;
int menuItemCategoryMask = 0;
unsigned short menuPartnerMask = 0;
unsigned char menuExtraPageIndex = 0;
int menuSavedCursor = -1;

/* Uninitialized static data of this TU's own functions (0x0069E220..0x0069E268, .sbss). Types from the uses in this file. */
static char menuNumberBuffer[8];
static signed char menuSavedSlotState;
static signed char menuSkillModelsReady;
static float menuRingTilt;
static float menuRingTiltPrev;
static unsigned char learnedEtherList[4];
static int equipItemCount;
static signed char mainCommandKindTable[8];
static unsigned char menuTutorialSavedHeader[4];
static unsigned char menuTutorialSavedFlags0;
static unsigned char menuTutorialSavedFlags1;
static int menuTutorialSavedSkillPoints;
static int menuTutorialSavedClassPoints;
static unsigned char menuTutorialSavedCursors[3];
extern unsigned char menuTutorialSavedThirdCursor;
static unsigned char menuTutorialSavedLicenseState;
static signed char menuUmnImageCount;
static signed char menuUmnImageIndex;
static void *menuUmnImageSourceBuffer;

/* Control buffer tables: entries below CONTROL_ENTRY_SECTOR_COUNT take CD sector buffers, the rest buffer
   reserves (menuSkillModelBuffersAlloc). */
#define CONTROL_ENTRY_COUNT 11
#define CONTROL_ENTRY_SECTOR_COUNT 4
/* ControlSlot flags bit 1: the slot is active (set by menuSlotSetDirection). */
#define CONTROL_SLOT_FLAG_ACTIVE 0x02
/* ControlSlot flags bit 0: the mark of the slot that is current (set and cleared by the menuFocus and menuScene functions). */
#define CONTROL_SLOT_FLAG_MARK 0x01
/* U.M.N. destination list (menuMapListDraw): row pitch, the text id of row 0 for menuMainTxdEntry, and the right edge of
   the row text. */
#define MENU_ITEM_ROW_HEIGHT 0x20
#define MENU_ITEM_TXD_BASE 0x1E0
#define MENU_ITEM_TEXT_RIGHT_X 0x100
/* Menu sound effect ids for seVoiceStartDefault: cursor move or open, accept, refuse, back out of a list, and
   return to the previous state. */
#define MENU_SE_CURSOR 0x01000001
#define MENU_SE_CONFIRM 0x01000002
#define MENU_SE_REFUSE 0x01000003
#define MENU_SE_BACK 0x01000007
#define MENU_SE_CANCEL 0x01000008

extern u8 *menuCallbackHook[];

void menuSceneReset(void) {
    xglCdPendingReadsClear();
    ptReset();
    commandRecordTextsBind();
    menuSceneResetStub();
    xglTickReset();
    cursorBufferInit(mainCommandCursors, 6, 6, 0);
    cursorBufferInit(&mainCommandCursors[1], 4, 4, 0);
    cursorBufferInit(menuTabCursor, 0, 0, 0);
    menuSceneFlags = 0;
    menuCopilotModeFlag = 0;
    menuSceneResetNoop();
    fontObjInitGlobals();
    menuSkillModelReset();
    bgLayerInit();
    menuCallbackHook[0] = (u8 *) xglCallbackQueueRun; /* the slot holds the address of the callback runner */
    cursorBufferTablesInit();
}


void cursorBufferTablesInit(void) {
    cursorBufferInit(mainCommandCursors, 6, 6, 0);
    cursorBufferInit(&mainCommandCursors[1], 4, 4, 0);
    cursorBufferInit(windowListCursor, selectionWindowCount, 0xA, 0);
    cursorBufferInit(recordItemCursor, 0x6B, 0x12, 0);
    cursorBufferInit(learnedEtherCursor, 3, 4, 0);
    cursorBufferInit(commandPageCursor, 7, 7, 0);
    cursorBufferInit(equipSlotCursor, menuEquipRowCount, 4, 0);
    cursorBufferInit(equipItemCursor, 8, 8, 0);
    cursorBufferInit(skillBoardCursorTable, 4, 4, 0);
    cursorBufferInit(&skillBoardCursorTable[1], 8, 8, 0);
    cursorBufferInit(&skillBoardCursorTable[2], 4, 4, 0);
    cursorBufferInit(mapChoiceCursor, 8, 8, 0);
    cursorBufferInit(menuConfigCursor, 0xA, 0xA, 0);
    menuSavedSlotState = 0;
    menuKeyCheckPending = 0;
}

/* Refreshes a cursor buffer: while menuCursorMemory is set only the unit size and capacity change, otherwise a full init. */
void cursorBufferSetOrInit(ListCursor *buffer, int capacity, int unit_size, int mode_flags) {
    if (menuCursorMemory != 0) {
        buffer->unit_size = unit_size;
        cursorBufferSetCapacity(buffer, capacity);
    } else {
        cursorBufferInit(buffer, capacity, unit_size, mode_flags);
    }
}

/* Rewinds the cursors of the menu's cursor buffers (their cursor and previous_cursor words) and clears menuSavedSlotState. */
void cursorBuffersRewind(void) {
    ListCursor *entries;
    signed char *flag;
    u8 *hook;
    int hookWord;

    hook = menuCallbackHook[0];
    hookWord = *(int *) hook;
    if (menuEquipRowCount != 0) {
        *(int *) hook = hookWord;
        flag = &menuSavedSlotState;
    }
    windowListCursor[0].previous_cursor = 0;
    windowListCursor[0].cursor = 0;
    *flag = 0;
    recordItemCursor[0].previous_cursor = 0;
    recordItemCursor[0].cursor = 0;
    learnedEtherCursor[0].previous_cursor = 0;
    learnedEtherCursor[0].cursor = 0;
    commandPageCursor[0].previous_cursor = 0;
    commandPageCursor[0].cursor = 0;
    equipSlotCursor[0].previous_cursor = 0;
    equipSlotCursor[0].cursor = 0;
    equipItemCursor[0].previous_cursor = 0;
    equipItemCursor[0].cursor = 0;
    entries = skillBoardCursorTable;
    entries[0].previous_cursor = 0;
    entries[0].cursor = 0;
    entries[1].previous_cursor = 0;
    entries[1].cursor = 0;
    entries[2].previous_cursor = 0;
    entries[2].cursor = 0;
    mapChoiceCursor[0].previous_cursor = 0;
    mapChoiceCursor[0].cursor = 0;
    menuConfigCursor[0].previous_cursor = 0;
    menuConfigCursor[0].cursor = 0;
}

/* ControlSlot.state values; 2 and 3 are not named yet. */
enum ControlState {
    CONTROL_STATE_IDLE = 0,
    CONTROL_STATE_BROWSE = 1,
    CONTROL_STATE_RUNNING = 4,
    CONTROL_STATE_CHOICE = 5,
    CONTROL_STATE_CHOICE_APPLY = 6,
    CONTROL_STATE_WAIT = 7
};

/* Stores flag in the slot and marks the slot active (flags bit 1). */
void menuSlotSetDirection(ControlSlot *slot, int flag, int value) {
    flag &= 0xFF;
    if (flag != 0) {
        slot->value = (short) value;
    }
    slot->flag = flag;
    slot->flags |= CONTROL_SLOT_FLAG_ACTIVE;
}

extern void *menuUmnImageBuffers[];
extern FontTrack itemListRowTracks[];

void menuUmnImageReset(void) {
    int i;
    void **sequenceSlot;

    sequenceSlot = menuUmnImageBuffers;
    sequenceSlot += 7;
    for (i = 0; i < 8; i++) {
        *sequenceSlot = 0;
        sequenceSlot -= 1;
    }
    menuUmnImageCount = 0;
    menuUmnImageIndex = 0;
    menuUmnImageSourceBuffer = 0;
    fontTrackSetBoth(&itemListRowTracks[0], 0, 0);
    fontTrackSetBoth(&itemListRowTracks[1], 0, 0);
    fontTrackSetBoth(&itemListRowTracks[2], 0, 0);
    fontTrackSetBoth(&itemListRowTracks[3], 0, 0);
}

extern const char *menuUmnImageFileNames[];

void menuUmnImageLoad(int count) {
    int remaining;
    void *buffer;
    void **sequenceSlot;

    if (count > 0) {
        if (count < 9) {
            menuUmnImageSourceBuffer = xglCdBufferReserve(0xE000);
            if (count > 0) {
                remaining = count;
                sequenceSlot = menuUmnImageBuffers;
                do {
                    remaining -= 1;
                    buffer = xglCdBufferReserve(0xE000);
                    *sequenceSlot = buffer;
                    sequenceSlot += 1;
                } while (remaining != 0);
            }
            menuUmnImageCount = (signed char) count;
            menuUmnImageIndex = 0;
            xglCdReadFileAsync(menuUmnImageFileNames[0], menuUmnImageSourceBuffer, 4, 0);
        }
    }
}

void *memset(void *destination, int value, unsigned int size);

/* Sets up JPEG decode of sequence slot index, then advances the sequence counter menuUmnImageIndex and reads the next
   file while it is below menuUmnImageCount. */
void menuUmnImageDecode(u8 sequenceIndex) {
    JpegDecodeControl request;

    memset(&request, 0, sizeof(request));
    request.source = (u8 *) menuUmnImageSourceBuffer; /* the buffers still hold addresses as ints */
    request.destination = (u8 *) menuUmnImageBuffers[sequenceIndex];
    xglJpegDecode(&request);
    menuUmnImageIndex += 1;
    if ((u8) menuUmnImageIndex < (u8) menuUmnImageCount) {
        xglCdReadFileAsync(menuUmnImageFileNames[(u8) menuUmnImageIndex], menuUmnImageSourceBuffer, 4, (u8) menuUmnImageIndex);
    }
}


/* The VIF1 packet template at menuUmnImagePacketTemplate: its +0x40 tag and +0x60 size are written before each use. */
typedef struct VifPacketTemplate {
    u8 unmodeled_00[0x40];
    u64 tag;                    /* 0x40 */
    u8 unmodeled_48[0x18];
    u32 size;                   /* 0x60 */
    u8 unmodeled_64[0xC];
} VifPacketTemplate;

extern VifPacketTemplate menuUmnImagePacketTemplate;

/* Queues one textured sprite (GS A+D packets) for the VIF1 packet at the current position. */
void menuUmnImageDraw(int x, int y, int sequenceIndex, int scale) {
    XglPacket *packet;
    void *texture;
    int shade;
    int alpha;

    texture = menuUmnImageBuffers[sequenceIndex];
    packet = xglPacketGetCurrent();
    shade = ((scale * 96) >> 7) + 32;
    alpha = ((scale << 5) >> 7) + 96;
    if (texture != 0) {
        if ((unsigned char) menuUmnImageIndex >= sequenceIndex + 1) {
            menuUmnImagePacketTemplate.tag = 0x20000001C0ULL;
            menuUmnImagePacketTemplate.size = 0xE00;
            sceVif1PkCnt(packet, 0);
            sceVif1PkAddDataN(packet, &menuUmnImagePacketTemplate, 28);
            sceVif1PkRef(packet, texture, 0xE00, 0, 0x51000E00, 0);
            nmlPacketGsInit();
            packet_gs_entry64(GS_TEXFLUSH, 0);
            packet_gs_entry64(GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
            packet_gs_entry64(GS_TEX0_1, GS_SET_TEX0(0x3800, 8, GS_PSMCT32, 9, 9, 1, 0, 0, 0, 0, 0, 0));
            packet_gs_entry64(GS_CLAMP_1, GS_SET_CLAMP(1, 1, 0, 0, 0, 0));
            packet_gs_entry64(GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
            packet_gs_entry64(GS_ALPHA_1, GS_SET_ALPHA(0, 1, 2, 1, alpha));
            packet_gs_entry64(GS_RGBAQ, GS_SET_RGBAQ(shade, shade, shade, 0x80, 0x3F800000));
            packet_gs_entry64(GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0));
            packet_gs_entry64(GS_UV, 0);
            packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(x * 16 + 0x7000, y * 16 + 0x7200, 0));
            packet_gs_entry64(GS_UV, GS_SET_UV(0x1C00, 0x200));
            y += 0x20;
            packet_gs_entry64(GS_XYZ2, GS_SET_XYZ((x + 0x8C0) << 4, y * 16 + 0x7200, 0));
            xglDirectQueueSubmitRef();
        }
    }
}

extern unsigned char mapListTitleText[];
extern int itemListOddRowScale[];
extern int itemListEvenRowTarget[];
extern int itemListOddRowTarget[];

/* Draws the U.M.N. destination list (title "  Map") with one row per destination. Row i's track lane (i & 1) takes the highlight target, 128 on the selected item and 0 otherwise, and the packet scale is that lane's current value. */
void menuMapListDraw(ListCursor *list) {
    int *target;
    int panelTop;
    int panelHeight;
    int rowY;
    int textX;
    int i;
    int scale;
    int textWidth;
    int selected;

    panelTop = 0x94 - list->capacity * 7;
    panelHeight = list->capacity * MENU_ITEM_ROW_HEIGHT + 0x1B;
    textDrawSegment(0, 0, menuTextInitMarkup, 0);
    panelDrawNineSlice(menuPanelFrame, 0x1A, panelTop, 0x1CC, panelHeight);
    fontTrackEase(&itemListRowTracks[0]);
    fontTrackEase(&itemListRowTracks[1]);
    fontTrackEase(&itemListRowTracks[2]);
    fontTrackEase(&itemListRowTracks[3]);
    i = 0;
    if (list->capacity != 0) {
        rowY = panelTop;
        do {
            if (i & 1) {
                target = &itemListOddRowTarget[7 * (i >> 1)];
            } else {
                target = &itemListEvenRowTarget[7 * (i >> 1)];
            }
            selected = (i == list->cursor);
            *target = selected << 7;
            if (i & 1) {
                target = &itemListOddRowScale[7 * (i >> 1)];
            } else {
                target = &itemListRowTracks[i >> 1].current_pair.x;
            }
            scale = *target;
            menuUmnImageDraw(0x20, rowY + 0x13, i, scale);
            textX = MENU_ITEM_TEXT_RIGHT_X;
            textWidth = xglFontGetStringWidth(menuMainTxdEntry(i + MENU_ITEM_TXD_BASE));
            textX -= textWidth * 15 >> 5;
            textDrawSegment(textX, rowY + 0x19, menuMainTxdEntry(i + MENU_ITEM_TXD_BASE), selected == 0 ? selected | 2 : selected);
            rowY += MENU_ITEM_ROW_HEIGHT;
            i++;
        } while (i < list->capacity);
    }
    fontObjResetAndBuild();
    menuDrawTitledFrame(0x1E, panelTop + 5, 0x1C4, panelHeight - 10, mapListTitleText);
}

extern unsigned int menuSkillModelSizes[];
extern void *menuSkillModelPxyBuffers[];
extern void *menuSkillModelTxyBuffers[];
extern unsigned char menuSkillModelLoadTally[];

/* Sets up the two 11-entry tables (menuSkillModelPxyBuffers and menuSkillModelTxyBuffers) from the .pxy and .txy size pairs at menuSkillModelSizes: the first
   four entries use the sector allocator, the rest the buffer reserve; then clears menuSkillModelLoadTally and menuSkillModelsReady. */
void menuSkillModelBuffersAlloc(void) {
    int index;

    for (index = 0; index < CONTROL_ENTRY_COUNT; index++) {
        if (index < CONTROL_ENTRY_SECTOR_COUNT) {
            menuSkillModelPxyBuffers[index] = xglCdSectorAlloc(menuSkillModelSizes[2 * index]);
            menuSkillModelTxyBuffers[index] = xglCdSectorAlloc(menuSkillModelSizes[2 * index + 1]);
        } else {
            menuSkillModelPxyBuffers[index] = xglCdBufferReserve(menuSkillModelSizes[2 * index]);
            menuSkillModelTxyBuffers[index] = xglCdBufferReserve(menuSkillModelSizes[2 * index + 1]);
        }
        menuSkillModelLoadTally[index] = 0;
    }
    menuSkillModelsReady = 0;
}

/* Clears the eleven per-index tables (two int tables and one byte table), then the flag at menuSkillModelsReady, and
   hands off to menuUmnImageReset. */
void menuSkillModelReset(void) {
    int index;

    for (index = 0; index < CONTROL_ENTRY_COUNT; index++) {
        menuSkillModelPxyBuffers[index] = 0;
        menuSkillModelTxyBuffers[index] = 0;
        menuSkillModelLoadTally[index] = 0;
    }
    menuSkillModelsReady = 0;
    menuUmnImageReset();
}

/* Clears the tail indices 4..10 of the same tables that menuSkillModelReset clears, then the flag at menuSkillModelsReady,
   and hands off to menuUmnImageReset. */
void menuSkillModelTailReset(void) {
    int index;

    for (index = CONTROL_ENTRY_SECTOR_COUNT; index < CONTROL_ENTRY_COUNT; index++) {
        menuSkillModelPxyBuffers[index] = 0;
        menuSkillModelTxyBuffers[index] = 0;
        menuSkillModelLoadTally[index] = 0;
    }
    menuSkillModelsReady = 0;
    menuUmnImageReset();
}

void menuSkillModelReadDone(int key) {
    menuSkillModelLoadTally[((u8) key) >> 1] += 1;
    if (menuSkillModelLoadTally[2] == 3 && menuSkillModelLoadTally[3] == menuSkillModelLoadTally[2] && menuSkillModelLoadTally[4] == menuSkillModelLoadTally[3] && menuSkillModelLoadTally[5] == menuSkillModelLoadTally[4] && menuSkillModelLoadTally[6] == menuSkillModelLoadTally[5] && menuSkillModelLoadTally[7] == menuSkillModelLoadTally[6] && menuSkillModelLoadTally[8] == menuSkillModelLoadTally[7] && menuSkillModelLoadTally[9] == menuSkillModelLoadTally[8] && menuSkillModelLoadTally[10] == menuSkillModelLoadTally[9]) {
        menuSkillModelsReady = 1;
    }
}

extern const char *menuSkillModelFileNames[];

/* Runs once per index: the byte at menuSkillModelLoadTally[index] is set on the first call, then the two files of that
   index are read into the buffers held in menuSkillModelPxyBuffers and menuSkillModelTxyBuffers (buffers and file ids not yet proved). */
void menuSkillModelLoadOnce(int index) {
    u8 *tally;
    void *firstBuffer;
    void *secondBuffer;

    tally = &menuSkillModelLoadTally[index];
    if (*tally == 0) {
        firstBuffer = menuSkillModelPxyBuffers[index];
        *tally += 1;
        if (firstBuffer != 0) {
            xglCdReadFileAsync(menuSkillModelFileNames[index * 2], firstBuffer, 3, (index * 2) & 0xFE);
        }
        secondBuffer = menuSkillModelTxyBuffers[index];
        if (secondBuffer != 0) {
            xglCdReadFileAsync(menuSkillModelFileNames[index * 2 + 1], secondBuffer, 3, (index * 2 + 1) & 0xFF);
        }
    }
}

void menuSkillModelTailLoad(void) {
    int index;

    menuSkillModelsReady = 0;
    for (index = CONTROL_ENTRY_SECTOR_COUNT; index < CONTROL_ENTRY_COUNT; index++) {
        menuSkillModelPxyBuffers[index] = xglCdBufferReserve(menuSkillModelSizes[2 * index]);
        menuSkillModelTxyBuffers[index] = xglCdBufferReserve(menuSkillModelSizes[2 * index + 1]);
        menuSkillModelLoadTally[index] = 0;
    }
    for (index = CONTROL_ENTRY_SECTOR_COUNT; index < CONTROL_ENTRY_COUNT; index++) {
        menuSkillModelLoadOnce(index);
    }
}

extern u8 menuTutorialActive;
extern char *menuTutorialLine;
extern unsigned char equipItemList[];
extern unsigned char menuConfigRowKindTable[];
extern ControlSlot menuSlots[];

/* Chooses the menu text id for the current state and passes it to msgRangeSetText when it changed. */
void menuMessageTextUpdate(void) {
    unsigned char *previousText;
    unsigned char *nextText;
    int currentSlot;
    int entry;

    previousText = menuMessageRangeState[0].text;
    nextText = previousText;
    if (menuTutorialActive != 0) {
        nextText = menuTutorialLine;
    } else if (menuCurrentSlot == 0) {
        if ((u8) menuSlots[0].state == 0) {
            if (menuCopilotModeFlag != 0 && mainCommandCursors[0].cursor == 4) {
                nextText = menuMainTxdEntry(433);
            } else {
                nextText = menuMainTxdEntry(mainCommandCursors[0].cursor + 410);
            }
        } else {
            nextText = menuMainTxdEntry(mainCommandKindTable[mainCommandCursors[1].cursor] + 416);
        }
    } else if ((menuSceneFlags & 2) != 0) {
        if (menuCopilotModeFlag != 0 && mainCommandCursors[0].cursor == 4) {
            nextText = menuMainTxdEntry(433);
        } else {
            nextText = menuMainTxdEntry(mainCommandCursors[0].cursor + 410);
        }
    } else if (menuCurrentSlot == 6) {
        switch (menuTabCursor[0].cursor) {
        case 0:
            if (menuCopilotModeFlag != 0) {
                nextText = menuMainTxdEntry(431);
            } else {
                nextText = menuMainTxdEntry(428);
            }
            break;
        case 1:
            if (menuCopilotModeFlag != 0) {
                nextText = menuMainTxdEntry(432);
            } else {
                nextText = menuMainTxdEntry(429);
            }
            break;
        }
    } else {
        currentSlot = menuCurrentSlot;
        switch (currentSlot) {
        case 1:
            if ((u8) menuSlots[currentSlot].state == 1) {
                switch (menuTabCursor[0].cursor) {
                case 0:
                    nextText = menuMainTxdEntry(402);
                    break;
                case 1:
                    nextText = menuMainTxdEntry(420);
                    break;
                }
            } else {
                if (windowListCursor[0].capacity != 0) {
                    nextText = hitActionTextGet(selectionUsableItemList[windowListCursor[0].cursor], 1);
                } else {
                    nextText = menuMainTxdEntry(402);
                }
            }
            break;
        case 8:
            if ((u8) menuSlots[currentSlot].state == 1) {
                switch (menuTabCursor[0].cursor) {
                case 0:
                    nextText = menuMainTxdEntry(402);
                    break;
                case 1:
                    nextText = menuMainTxdEntry(420);
                    break;
                }
            } else {
                if (selectionKeyItemBitTest(recordItemCursor[0].cursor + 1) != 0) {
                    nextText = dataBoxCategory3TextGet(recordItemCursor[0].cursor + 1, 1);
                } else {
                    nextText = menuMainTxdEntry(400);
                }
            }
            break;
        case 2: {
            if (learnedEtherCursor[0].capacity != 0) {
                nextText = battleLookupTextGet(learnedEtherList[learnedEtherCursor[0].cursor], 1);
            } else {
                nextText = menuMainTxdEntry(411);
            }
            break;
        }
        case 3: {
            if ((u8) menuSlots[currentSlot].state == 6) {
                ListCursor *cursors = skillBoardCursorTable;

                if (selectionEntryBitOutsideMask(cursors[0].cursor, cursors[1].cursor, cursors[2].cursor, menuItemCategoryMask) != 0) {
                    nextText = menuMainTxdEntry(400);
                } else {
                    nextText = selectionSubTextGet(cursors[0].cursor, cursors[1].cursor, cursors[2].cursor, 1);
                }
            } else if ((u8) menuSlots[currentSlot].state == 1) {
                if (menuTabCursor[0].cursor != 0) {
                    nextText = menuMainTxdEntry(437);
                } else {
                    nextText = menuMainTxdEntry(435);
                }
            } else {
                ListCursor *cursors = skillBoardCursorTable;

                if (ptGetMenuMember()->buffer->openGroup < cursors[0].cursor) {
                    nextText = menuMainTxdEntry(436);
                } else {
                    nextText = menuMainTxdEntry(435);
                }
            }
            break;
        }
        case 5:
            if ((u8) menuSlots[currentSlot].state == 1 || (menuCopilotModeFlag == 0 && equipItemCount <= 0)) {
                switch (menuTabCursor[0].cursor) {
                case 0:
                    if (menuCopilotModeFlag != 0) {
                        nextText = menuMainTxdEntry(434);
                    } else {
                        nextText = menuMainTxdEntry(424);
                    }
                    break;
                case 1:
                    nextText = menuMainTxdEntry(425);
                    break;
                case 2:
                    nextText = menuMainTxdEntry(426);
                    break;
                case 3:
                    nextText = menuMainTxdEntry(427);
                    break;
                }
            } else {
                ListCursor *cursorWords = equipSlotCursor;

                if ((cursorWords->mode_flags & 1) != 0) {
                    if (menuCopilotModeFlag != 0) {
                        nextText = itemTextGet(ptGetMenuMember()->buffer->entries[cursorWords->cursor], 1);
                    } else {
                        entry = ptGetMenuMember()->buffer->entries[cursorWords->cursor];
                        if (selectionEntryBitOutsideMask(entry >> 5 & 3, entry >> 2 & 7, entry & 3, menuItemCategoryMask) != 0) {
                            nextText = menuMainTxdEntry(400);
                        } else {
                            nextText = selectionRecordTextGet(entry, 1);
                        }
                    }
                } else if (menuCopilotModeFlag != 0) {
                    if (menuTabCursor[0].cursor == 0) {
                        if (selectionPlainCount != 0) {
                            nextText = itemTextGet(selectionAccessoryList[equipItemCursor[0].cursor], 1);
                        } else {
                            nextText = menuMainTxdEntry(434);
                        }
                    }
                } else {
                    entry = equipItemList[equipItemCursor[0].cursor];
                    if (selectionEntryBitOutsideMask(entry >> 5 & 3, entry >> 2 & 7, entry & 3, menuItemCategoryMask) != 0) {
                        nextText = menuMainTxdEntry(400);
                    } else {
                        nextText = selectionRecordTextGet(entry, 1);
                    }
                }
            }
            break;
        case 9:
            if (menuConfigRowKindTable[menuConfigCursor[0].cursor] == 7 && SaveData.dolby_digital != 0) {
                nextText = menuMainTxdEntry(391);
            } else {
                nextText = menuMainTxdEntry(menuConfigRowKindTable[menuConfigCursor[0].cursor] + 378);
            }
            break;
        case 7:
            nextText = menuMainTxdEntry(403);
            break;
        case 4:
            nextText = menuMainTxdEntry(413);
            break;
        }
    }
    if (previousText != nextText) {
        msgRangeSetText(menuMessageRangeState, nextText);
    }
}

/* The cursor byte and base y of mainMenuBlock read through their own symbols: the original addresses them
 * separately, and as mainMenuBlock.cursorByte / .position.current_pair.y the loads share the block's base
 * register and change .text. */
extern unsigned char mainMenuBlockCursor[];
extern unsigned char commandSlotState[];

/* Rebuilds the cursor buffers for the layout selected by menuCurrentSlot and sets their capacities from the context. */
void menuRebuildCursorBuffers(void) {
    PartyMember *context;
    int i;
    int count;
    int mode;
    int capacity;

    ptState.menuSlot = mainMenuBlockCursor[0];
    switch (menuCurrentSlot) {
    case 4:
        if (menuCopilotModeFlag != 0) {
            menuPartnerMask = 1;
            for (i = 0; i < ptGetMenuMember()->buffer->entry_count; i++) {
                if (ptGetMenuMember()->buffer->entries[i] == 0x24) {
                    menuPartnerMask |= 2;
                }
            }
            if (commandSlotState[0] == 5) {
                context = ptGetMenuMember();
                capacity = selectionCopilotRecordCount(context->id, menuPartnerMask);
                cursorBufferSetCapacity(commandPageCursor, capacity);
                return;
            }
            cursorBufferSetCapacity(commandPageCursor, selectionMemberCapacityTableByKind[ptGetMenuMember()->id - 4]);
            return;
        }
        if (commandSlotState[0] == 5) {
            context = ptGetMenuMember();
            capacity = selectionPairRecordCount(context->id, menuPartnerMask);
            cursorBufferSetCapacity(commandPageCursor, capacity);
            return;
        }
        context = ptGetMenuMember();
        capacity = selectionMemberCapacity(context->id);
        cursorBufferSetCapacity(commandPageCursor, capacity);
        return;
    case 2:
        learnedEtherList[0] = learnedEtherList[1] = learnedEtherList[2] = learnedEtherList[3] = 0;
        count = 0;
        for (i = 1; i < 4; i++) {
            if (menuCopilotModeFlag != 0) {
                if (ptGetMenuMember()->id == 12 && dataSkillLearnGet(6, i) != 0) {
                    learnedEtherList[count] = i;
                    count++;
                }
            } else if (dataSkillLearnGet(ptGetMenuMember()->id, i) != 0) {
                learnedEtherList[count] = i;
                count++;
            }
        }
        cursorBufferSetCapacity(learnedEtherCursor, count);
        return;
    case 5:
        cursorBufferSetOrInit(equipSlotCursor, ptGetMenuMember()->buffer->entry_count, 4, 0);
        if (menuCopilotModeFlag == 0) {
            mode = 0;
            switch (menuTabCursor[0].cursor) {
            case 2:
                mode = 0;
                break;
            case 0:
                mode = 1;
                break;
            case 1:
                mode = 2;
                break;
            case 3:
                mode = 3;
                break;
            }
            context = ptGetMenuMember();
            equipItemCount = selectionCollectEntries(equipItemList, context, mode, menuItemCategoryMask);
            menuEquipRowCount = selectionSubIsTaken(ptGetMenuMember(), 1, 7, 1) + 3;
            cursorBufferSetCapacity(equipItemCursor, equipItemCount);
            equipItemCursor[0].unit_size = 4;
        }
        return;
    case 3:
        {
            float *angles = menuRingColumnAngle;
            float x = -0.52359879f;
            float y = -0.34906587f;
            float z = -0.17453294f;
            angles[0] = x;
            angles[1] = y;
            angles[2] = z;
            angles[3] = 0;
        }
        break;
    }
}

/* Moves the current xglFontCmdMark (flags bit 0) from the old current link to link, and makes link current. */
void menuLinkSetCurrent(ListCursor *link) {
    if (menuCurrentLink != 0) {
        menuCurrentLink->mode_flags &= ~1;
    }
    if (link != 0) {
        link->mode_flags |= 1;
    }
    menuCurrentLink = link;
}

/* Calls menuSlotSetDirection on slot from with flag 3 when from is below to, else 4, but only while flags bit 0x10
   is clear and flags 0x2 is set. */
void menuSlotSetLeaveDirection(int from, int to) {
    if (menuSlots[from].flags & 0x10) {
        return;
    }
    if ((menuSlots[from].flags & CONTROL_SLOT_FLAG_ACTIVE) == 0) {
        return;
    }
    if (from < to) {
        menuSlotSetDirection(&menuSlots[from], 3, 0x80);
    } else {
        menuSlotSetDirection(&menuSlots[from], 4, 0x80);
    }
}

/* Re-issues menuSlotSetDirection on slot to: when its flags bit 0x2 is set, only a flag of 3 or 4 passes. The mode is
   1 when to is below from, 2 when from is below to, and 0 with a zero value when they are equal. */
void menuSlotSetEnterDirection(int to, int from) {
    if (menuSlots[to].flags & CONTROL_SLOT_FLAG_ACTIVE) {
        u8 flag = menuSlots[to].flag;

        if (flag != 3) {
            if (flag != 4) {
                return;
            }
        }
    }
    if (to < from) {
        menuSlotSetDirection(&menuSlots[to], 1, 0x80);
    } else if (from < to) {
        menuSlotSetDirection(&menuSlots[to], 2, 0x80);
    } else {
        menuSlotSetDirection(&menuSlots[to], 0, 0);
    }
}

extern MenuBlock mainMenuBlock;
extern int menuPendingSelection;

void menuFocusRootSlot(void) {
    menuSlots[menuCurrentSlot].flags &= ~CONTROL_SLOT_FLAG_MARK;
    menuSlotSetLeaveDirection(menuCurrentSlot, 0);
    menuSlotSetEnterDirection(7, menuCurrentSlot);
    menuSlots[0].flags |= CONTROL_SLOT_FLAG_MARK;
    menuSceneFlags &= ~1;
    menuPendingSelection = -1;
    mainMenuBlock.flags &= ~1;
    menuCurrentSlot = 0;
    menuSetMode(&mainMenuBlock, 0);
    menuSetSelected(&mainMenuBlock, -1);
    mainMenuBlock.flags &= ~2;
    itemMenuBlock.flags &= ~1;
    menuLinkSetCurrent(0);
    cursorBufferInit(menuTabCursor, 0, 0, 0);
}

void menuFocusSlot(int index, int flag) {
    ControlSlot *slot = &menuSlots[index];

    slot->state = 0;
    slot->flags |= CONTROL_SLOT_FLAG_MARK;
    menuLinkSetCurrent(slot->link);
    menuSlotSetLeaveDirection(menuCurrentSlot, index);
    if (flag != 0) {
        menuSlotSetEnterDirection(7, menuCurrentSlot);
    } else {
        menuSlotSetLeaveDirection(7, index);
    }
    menuSlotSetEnterDirection(index, menuCurrentSlot);
    menuCurrentSlot = index;
}


void menuFocusMainSlot(void) {
    ControlSlot *slot = &menuSlots[7];

    menuSceneFlags |= 1;
    if (menuCopilotModeFlag != 0 && mainCommandCursors[0].cursor == 2) {
        MenuBlock *menu = &mainMenuBlock;

        menuSetMode(menu, 1);
        menuEnableItems(menu, 0xF0F);
        if (menu->cursor < 3) {
            menuSavedCursor = menu->cursor;
            menuSetCursor(menu, menu->cursor + 3);
        }
        menuEnableItems(menu, 0x40E);
        menuPlaceItemCursor(menu, menu->cursor);
        fontTrackAdvance(&menu->tracks[0]);
        fontTrackAdvance(&menu->tracks[1]);
        fontTrackAdvance(&menu->tracks[2]);
    } else {
        menuSetMode(&mainMenuBlock, 1);
        if (menuCopilotModeFlag != 0) {
            menuEnableItems(&mainMenuBlock, 0x401);
        }
    }
    slot->state = 0;
    slot->flags |= CONTROL_SLOT_FLAG_MARK;
    mainMenuBlock.flags |= 1;
    menuLinkSetCurrent(slot->link);
    menuSlotSetLeaveDirection(menuCurrentSlot, 7);
    menuSlotSetEnterDirection(7, menuCurrentSlot);
    menuCurrentSlot = 7;
}

/* Clears the mark bit of the current slot, then sets menuPendingSelection to -1 before the tail call. */
void menuCancelToMainSlot(void) {
    menuPendingSelection = -1;
    menuSlots[menuCurrentSlot].flags &= ~CONTROL_SLOT_FLAG_MARK;
    menuFocusMainSlot();
}

void menuFocusRootSlot(void);
/* Rewinds the cursors of the menu's cursor buffers (their cursor and previous_cursor words) and clears menuSavedSlotState. */
/* Rebuilds the cursor buffers for the layout selected by menuCurrentSlot and sets their capacities from the context. */
void menuRebuildCursorBuffers(void);
extern int mapChoiceCount[]; /* an incomplete array keeps it out of the small data area, as the original loads it */

typedef union MenuLatch {
    int word;
    u8 bytes[4];
} MenuLatch;

/* Initialized small data of this TU's menu functions (0x0069D514..0x0069D53F, .sdata); the definitions follow the MenuLatch type. */
MenuLatch menuCursorLatch = { -1 };
u8 menuTutorialActive = 0;
signed char menuTutorialToggleFlags = 0;
u8 menuLayoutMode = 0;
u8 menuTutorialRestartCount = 0;
u8 menuTutorialHintFlags = 0;
int menuTutorialIndex = 0;
char *menuTutorialLine = 0;
u8 menuTutorialStage = 0;
u8 menuTutorialPanelFlag = 0;
unsigned char mapListTitleText[] = "  Map";
u8 mapUnlockProgressTable[7] = { 0x0c, 0x3b, 0x49, 0x53, 0x5c, 0x6d, 0x6d };
float selectionRingOpacity = 0.0f;

extern MenuLatch menuCursorLatch;
/* xglCdBufferRewind returns int but is called as a void function: the int result changes the register allocation of menuSceneSetState. */
typedef void VoidFunction(void);

/* The TU's view of xglCdBufferRewind: its result is never used. */

/* Scene-change handler (menuState is the menu state id): clears the current slot, refreshes the cursors, then switches
   on menuState to set the menu mode and the cursor buffers. */
void menuSceneSetState(int menuState) {
    int savedCursor;
    MenuBlock *stateBlock = &itemMenuBlock;
    ControlSlot *controlSlot = &menuSlots[menuState];

    mainMenuBlock.flags &= ~2;
    menuSlots[menuCurrentSlot].flags &= ~CONTROL_SLOT_FLAG_MARK;
    menuSetValuesMode(stateBlock, 0);
    stateBlock->flags &= ~(1 | 4);
    menuSetSelected(&mainMenuBlock, -1);
    menuPlaceCursor(0, 0, 0, 0);
    if (menuSavedCursor != -1 && menuState != 3) {
        menuEnableItems(&mainMenuBlock, 0xF0F);
        menuSetCursor(&mainMenuBlock, menuSavedCursor);
        menuSavedCursor = -1;
    }
    if (menuPendingSelection == -1) {
        savedCursor = menuCursorLatch.word = menuPendingSelection;
    }
    if (!menuCursorMemory && menuState != menuCurrentSlot) {
        cursorBuffersRewind();
    }
    switch (menuState) {
    case 1:
        seVoiceStartDefault(0x01000006);
        menuSceneFlags &= ~1;
        menuSetMode(&mainMenuBlock, 9);
        mainMenuBlock.flags &= ~1;
        cursorBufferInit(menuTabCursor, 2, 2, 0);
        if (!menuCursorMemoryAlt && menuCurrentSlot == 8) {
            menuFocusSlot(menuState, 0);
            menuSlots[menuState].state = 7;
        } else {
            menuFocusSlot(menuState, 0);
        }
        break;
    case 8:
        seVoiceStartDefault(0x01000006);
        menuSceneFlags &= ~1;
        selectionKeyItemBitsFill();
        menuFocusSlot(menuState, 0);
        break;
    case 2:
        if (menuCursorMemoryAlt != 0 && menuCopilotModeFlag != 0 && ptFindRecord(12) && ptFindRecord(6)) {
            menuSetCursor(&mainMenuBlock, ptFindSlot(12) - 7);
            seVoiceStartDefault(0x01000006);
            menuSceneFlags |= 1;
            menuSetMode(&mainMenuBlock, 3);
            mainMenuBlock.flags &= ~1;
            menuFocusSlot(menuState, 1);
        } else if (menuPendingSelection == -1) {
            if (menuCurrentSlot == 0) {
                seVoiceStartDefault(0x01000002);
            }
            menuFocusMainSlot();
        } else {
            seVoiceStartDefault(0x01000006);
            menuSceneFlags |= 1;
            menuSetMode(&mainMenuBlock, 3);
            mainMenuBlock.flags &= ~1;
            menuFocusSlot(menuState, 1);
        }
        cursorBufferInit(menuTabCursor, 0, 0, 0);
        break;
    case 3:
        if (menuPendingSelection == -1) {
            if (menuCurrentSlot == 0) {
                seVoiceStartDefault(0x01000002);
            }
            menuSceneFlags |= 1;
            if (menuCopilotModeFlag != 0) {
                menuSetMode(&mainMenuBlock, 1);
                menuEnableItems(&mainMenuBlock, 0xF0F);
                if (mainMenuBlock.cursor < 3) {
                    menuSavedCursor = mainMenuBlock.cursor;
                    menuSetCursor(&mainMenuBlock, mainMenuBlock.cursor + 3);
                }
                menuEnableItems(&mainMenuBlock, 0x40E);
                menuPlaceItemCursor(&mainMenuBlock, mainMenuBlock.cursor);
                fontTrackAdvance(&mainMenuBlock.tracks[0]);
                fontTrackAdvance(&mainMenuBlock.tracks[1]);
                fontTrackAdvance(&mainMenuBlock.tracks[2]);
            } else {
                menuSetMode(&mainMenuBlock, 1);
            }
            mainMenuBlock.flags |= 1;
            controlSlot = &menuSlots[7];
            controlSlot->state = 0;
            controlSlot->flags |= CONTROL_SLOT_FLAG_MARK;
            menuLinkSetCurrent(controlSlot->link);
            menuSlotSetLeaveDirection(menuCurrentSlot, 7);
            menuSlotSetEnterDirection(7, menuCurrentSlot);
            menuCurrentSlot = 7;
        } else {
            savedCursor = mainMenuBlock.cursor;
            seVoiceStartDefault(0x01000006);
            menuSceneFlags |= 1;
            menuSetMode(&mainMenuBlock, 3);
            if (menuCopilotModeFlag != 0) {
                menuEnableItems(&mainMenuBlock, 0xF0F);
                if (savedCursor < 3) {
                    menuSavedCursor = savedCursor;
                    menuSetCursor(&mainMenuBlock, savedCursor + 3);
                } else {
                    menuSetCursor(&mainMenuBlock, savedCursor);
                }
                menuEnableItems(&mainMenuBlock, 0x40E);
                menuPlaceItemCursor(&mainMenuBlock, mainMenuBlock.cursor);
                fontTrackAdvance(&mainMenuBlock.tracks[0]);
                fontTrackAdvance(&mainMenuBlock.tracks[1]);
                fontTrackAdvance(&mainMenuBlock.tracks[2]);
                menuCursorLatch.word = mainMenuBlock.cursor;
            }
            mainMenuBlock.flags &= ~1;
            menuFocusSlot(menuState, 1);
        }
        cursorBufferInit(menuTabCursor, 2, 2, 0);
        break;
    case 4:
        if (menuPendingSelection == -1) {
            if (!menuCurrentSlot) {
                seVoiceStartDefault(0x01000002);
            }
            menuFocusMainSlot();
            cursorBufferInit(menuTabCursor, 0, 0, 0);
        } else {
            seVoiceStartDefault(0x01000006);
            menuSceneFlags |= 1;
            menuSetMode(&mainMenuBlock, 3);
            mainMenuBlock.flags &= ~1;
            menuFocusSlot(menuState, 1);
            cursorBufferInit(menuTabCursor, 2, 2, 0);
        }
        break;
    case 5:
        ptState.menuSlot = mainMenuBlock.cursor;
        if (menuPendingSelection == -1) {
            if (menuCurrentSlot == 0) {
                seVoiceStartDefault(0x01000002);
            }
            menuFocusMainSlot();
            cursorBufferInit(menuTabCursor, 0, 4, 0);
            break;
        }
        seVoiceStartDefault(0x01000006);
        menuSceneFlags |= 1;
        menuSetMode(&mainMenuBlock, 3);
        mainMenuBlock.flags &= ~1;
        cursorBufferInit(menuTabCursor, 4, 4, 0);
        if (menuCopilotModeFlag) {
            cursorBufferSetOrInit(equipItemCursor, selectionPlainCount, 8, 0);
            cursorBufferSetOrInit(equipSlotCursor, ptGetMenuMember()->buffer->entry_count, 4, 0);
            cursorBufferSetCapacity(menuTabCursor, 1);
        } else {
            menuEquipRowCount = (u8) (selectionSubIsTaken(ptGetMenuMember(), 1, 7, 1) + 3);
            cursorBufferSetOrInit(equipSlotCursor, ptGetMenuMember()->buffer->entry_count, 4, 0);
            cursorBufferSetCapacity(menuTabCursor, 4);
        }
        menuFocusSlot(menuState, 1);
        break;
    case 6:
        if (menuCurrentSlot == 0) {
            seVoiceStartDefault(0x01000002);
        }
        menuSceneFlags &= ~1;
        menuSetMode(&mainMenuBlock, 1);
        mainMenuBlock.flags &= ~1;
        controlSlot->state = 0;
        controlSlot->flags |= CONTROL_SLOT_FLAG_MARK;
        menuLinkSetCurrent(controlSlot->link);
        menuSlotSetLeaveDirection(menuCurrentSlot, 7);
        menuSlotSetEnterDirection(7, menuCurrentSlot);
        menuCurrentSlot = menuState;
        cursorBufferInit(menuTabCursor, 2, 2, 0);
        break;
    case 9:
        menuSceneFlags &= ~1;
        menuFocusSlot(menuState, 0);
        cursorBufferInit(menuTabCursor, 0, 0, 0);
        ((VoidFunction *) xglCdBufferRewind)();
        fontObjLoadTexture(8);
        fontObjLoadTexture(9);
        break;
    case 10:
        menuSceneFlags &= ~1;
        menuFocusSlot(menuState, 0);
        cursorBufferInit(menuTabCursor, 0, 0, 0);
        ((VoidFunction *) xglCdBufferRewind)();
        menuUmnImageLoad(mapChoiceCount[0]);
        break;
    case 7:
    case 0:
    default:
        menuFocusRootSlot();
        return;
    }
    ptState.menuSlot = mainMenuBlock.cursor;
    menuRebuildCursorBuffers();
}


/* Resets the active studio camera: travel position and rotation to zero (position z 14), fov to 40 degrees,
   then sets its window to (0, 0)-(512, 448). */
void menuSceneCameraSetup(void) {
    XglCamera *camera = xglStudioSelectGetActiveCamera(0);

    camera->travel.position.z = 14.0f;
    camera->travel.fov = 0.69813174f;
    camera->travel.position.x = 0.0f;
    camera->travel.position.y = 0.0f;
    camera->travel.rotation.x = 0.0f;
    camera->travel.rotation.y = 0.0f;
    camera->travel.rotation.z = 0.0f;
    xglCameraSetWindow(camera, 0, 0, 512, 448);
}

void menuUmnImageReset(void);
/* Sets up the two 11-entry tables (menuSkillModelPxyBuffers and menuSkillModelTxyBuffers) from the .pxy and .txy size pairs at menuSkillModelSizes: the first four entries use the sector allocator, the rest the buffer reserve; then clears menuSkillModelLoadTally and menuSkillModelsReady. */
void menuSkillModelBuffersAlloc(void);

void menuSceneLoadAssets(void) {
    while (xglCdServiceIfPending() != 0) {
    }
    xglCdArenaOpen(0, 0);
    xglCdBufferAlloc(0x140000);
    menuUmnImageReset();
    menuSkillModelBuffersAlloc();
    bgLayerLoadAll();
    fontObjLoadTexture(2);
    fontObjLoadTexture(3);
    fontObjLoadTexture(0);
}

void menuSceneUnloadAssets(void) {
    xglCdBufferClear();
    bgLayerResetState();
    fontObjClearSlots();
    xglCdArenaClose();
}

/* menuSceneInit's step and draw callbacks (defined below in this file). */
struct MenuActor;
struct ControlRecord;
struct MenuInputBlock;
int menuMainCommandStep(ControlSlot *slot);
void menuDrawMainCards(ControlSlot *slot);
int menuItemListStep(ControlSlot *slot);
void menuDrawSlotItems(ControlSlot *slot);
int menuEtherPlaceStep(ControlSlot *slot);
void menuDrawEtherPage(ControlSlot *slot);
int menuSkillChargeStep(ControlSlot *slot);
void menuDrawSkillPage(ControlSlot *slot);
int menuTechPageStep(ControlSlot *slot);
void menuDrawCommandPage(ControlSlot *slot);
int menuEquipAssignStep(ControlSlot *slot);
void menuDrawEquipPage(ControlSlot *slot);
int menuPartyFormationStep(ControlSlot *slot);
void menuPartyFormationDrawNone(ControlSlot *slot);
int menuPartyChoiceStep(ControlSlot *slot);
void menuDrawPartyPage(ControlSlot *slot);
int menuKeyItemStep(ControlSlot *slot);
void menuDrawKeyItemDetail(ControlSlot *slot);
int menuConfigSlotStep(ControlSlot *slot);
void menuDrawConfigPage(ControlSlot *slot);
int menuMapChoiceStep(ControlSlot *slot);
void menuMapDraw(ControlSlot *slot);
extern FontTrack extraPanelTrack;
extern FontTrack equipPanelTrack[];
extern u8 mapUnlockProgressTable[7];
extern u8 menuTutorialPanelFlag;
extern u8 menuTutorialStage;
extern u8 menuTutorialHintFlags;
extern float menuRingColumnAngle[4];


void menuSceneInit(void) {
    int i;
    int r;
    int flag;
    int flagsB;
    int seen;
    int k;
    ControlSlot *slot;
    ControlSlot *slot7;
    u8 *slotFlags;
    ListCursor **slotLink;

    ptRefresh();
    ptState.nearbySearchType = uwamonoGetNearbyUnitType();
    menuSceneLoadAssets();

    menuCursorMemory = SaveData.cursor_memory;
    menuCursorMemoryAlt = SaveData.cursor_memory_alt;
    menuCardAvailabilityBits = 24;
    menuCurrentSlot = 0;
    menuPendingSelection = -1;
    menuChoiceOpenFlag = 0;
    countdownActive = 0;
    equipItemCount = 0;
    menuSavedCursor = -1;
    menuCursorLatch.word = -1;
    menuChoiceAction = 0;
    if (xglFlagsGet1(231)) {
        menuCardAvailabilityBits = (menuCardAvailabilityBits | 4) + 16;
    }
    if (xglFlagsGet1(232)) {
        menuCardAvailabilityBits = (menuCardAvailabilityBits | 2) + 16;
    }
    if (ptState.nearbySearchType == 9 || (ptState.progress->mapId != -1 && (unsigned int) (gameWork.mapId - 5510) >= 601)) {
        menuCardAvailabilityBits = (menuCardAvailabilityBits | 1) + 16;
    }

    menuEquipRowCount = 3;
    menuItemCategoryMask = 1;
    for (i = 1; i < 32; i++) {
        if (dataBoxChkCategory4(i)) {
            menuItemCategoryMask |= 1 << i;
            if (dataBoxChkCategory3(i + 76) == 0) {
                dataBoxIncCategory3(i + 76);
            }
        }
    }

    if (menuCopilotModeFlag != 0) {
        menuPartnerMask = 1;
    } else {
        menuPartnerMask = 1;
        for (i = 0; i < 16; i++) {
            if (slotPairTest(i + 1)) {
                menuPartnerMask |= 1 << i;
            }
        }
    }
    if (menuCopilotModeFlag != 0) {
        menuEquipRowCount = 3;
    }

    selectionRebuildWindowList();
    selectionRebuildList();
    menuFocusRootSlot();
    cursorBufferSetOrInit(mainCommandCursors, 6, 6, 0);
    cursorBufferSetOrInit(&mainCommandCursors[1], menuCardAvailabilityBits >> 4, 4, 0);

    mainCommandKindTable[0] = -1;
    mainCommandKindTable[1] = -1;
    mainCommandKindTable[2] = -1;
    mainCommandKindTable[3] = -1;
    for (i = 0; i < (menuCardAvailabilityBits >> 4); i++) {
        mainCommandKindTable[i] = -1;
        seen = 0;
        for (k = 0; k < 4; k++) {
            if (menuCardAvailabilityBits & (1 << k)) {
                if (seen == i) {
                    mainCommandKindTable[i] = k;
                    break;
                }
                seen++;
            }
        }
    }

    menuResetItems(&mainMenuBlock, menuCopilotModeFlag);
    menuResetItems(&itemMenuBlock, menuCopilotModeFlag);
    menuHide(&mainMenuBlock, 0);
    menuHide(&itemMenuBlock, 4);
    menuSetValuesMode(&itemMenuBlock, 0);
    fontTrackSetBoth(&extraPanelTrack, 0x200, 0);
    fontTrackSetBoth(equipPanelTrack, -0x110, menuEquipRowCount * 16 + menuCopilotModeFlag * 12 + 0x104);

    r = xglFlagsGet8(401);
    flag = r < 12 || (unsigned int) (r - 102) < 4;
    if ((unsigned int) (r - 50) < 8 || (unsigned int) (r - 79) < 9) {
        menuExtraPageIndex = 2;
    } else {
        menuExtraPageIndex = flag;
    }
    if (r < 7) {
        mainMenuBlock.items[3].flags |= 0x100000;
    }

    cursorBufferSetOrInit(windowListCursor, selectionWindowCount, 10, 0);
    cursorBufferSetOrInit(recordItemCursor, 107, 18, 0);
    cursorBufferSetOrInit(learnedEtherCursor, 3, 4, 0);
    cursorBufferSetOrInit(commandPageCursor, 7, 7, 0);
    cursorBufferSetOrInit(equipSlotCursor, menuEquipRowCount, 4, 0);
    cursorBufferSetOrInit(equipItemCursor, 8, 8, 0);
    cursorBufferSetOrInit(skillBoardCursorTable, 4, 4, 0);
    cursorBufferSetOrInit(&skillBoardCursorTable[1], 8, 8, 0);
    cursorBufferSetOrInit(&skillBoardCursorTable[2], 4, 4, 0);
    cursorBufferSetOrInit(mapChoiceCursor, 8, 8, 0);

    menuCurrentLink = 0;
    flagsB = xglFlagsGet8(401);
    mapChoiceCursor[0].capacity = 0;
    for (i = 0; i < 7; i++) {
        if (mapUnlockProgressTable[i] <= flagsB) {
            mapChoiceCursor[0].capacity += 1;
        }
    }
    if (ptState.progress->mapFlag != 0) {
        mapChoiceCursor[0].capacity = 8;
    }
    if (flagsB < 91) {
        menuMapChoiceDefaultTable[0] = 0;
    } else {
        menuMapChoiceDefaultTable[0] = 2;
    }

    cursorBufferSetOrInit(menuConfigCursor, 9, 10, 0);
    menuPlaceCursor(0, 0, 0, 0);
    menuSceneFlags = 0;

    for (i = 0; i < 11; i++) {
        menuSlots[i].value = 0;
        menuSlots[i].flag = 0;
        menuSlots[i].flags = 0;
        menuSlots[i].link = 0;
        menuSlots[i].state = 0;
        menuSlots[i].step = 0;
        menuSlots[i].callback = 0;
    }
    slot = menuSlots;
    slotFlags = &slot->flags;
    *slotFlags |= CONTROL_SLOT_FLAG_MARK | 0x10;
    slotLink = &slot->link;
    slot->step = menuMainCommandStep;
    slot->callback = menuDrawMainCards;
    *slotLink = mainCommandCursors;
    menuSlotSetDirection(slot, 1, 128);
    if (menuCursorMemory != 0) {
        slot->state = menuSavedSlotState;
    }

    slot[1].step = menuItemListStep;
    slot[1].callback = menuDrawSlotItems;
    slot[1].link = windowListCursor;
    slot[2].step = menuEtherPlaceStep;
    slot[2].callback = menuDrawEtherPage;
    slot[3].step = menuSkillChargeStep;
    slot[3].callback = menuDrawSkillPage;
    slot[4].step = menuTechPageStep;
    slot[4].callback = menuDrawCommandPage;
    slot[5].step = menuEquipAssignStep;
    slot[5].callback = menuDrawEquipPage;
    slot[6].step = menuPartyFormationStep;
    slot[6].callback = menuPartyFormationDrawNone;
    slot[7].step = menuPartyChoiceStep;
    slot7 = &slot[7];
    slot[2].link = learnedEtherCursor;
    slot7->callback = menuDrawPartyPage;
    slot[3].link = skillBoardCursorTable;
    slot[4].link = commandPageCursor;
    slot[5].link = equipItemCursor;
    menuSlotSetDirection(slot7, 1, 128);

    slot[8].step = menuKeyItemStep;
    slot[8].callback = menuDrawKeyItemDetail;
    slot[8].link = recordItemCursor;
    slot[9].step = menuConfigSlotStep;
    slot[9].callback = menuDrawConfigPage;
    slot[9].link = menuConfigCursor;
    slot[10].step = menuMapChoiceStep;
    slot[10].callback = menuMapDraw;
    slot[10].link = mapChoiceCursor;
    menuResetState(menuMessageRangeState);

    menuTutorialActive = 0;
    menuTutorialLine = 0;
    menuTutorialPanelFlag = 0;
    menuTutorialStage = 0;
    menuTutorialHintFlags = 0;
    xglPadSetRepeat(0, 0xF00C, 6, 2);

    menuRingTilt = 0.47123894f;
    menuRingTiltPrev = 0.47123894f;
    menuRingColumnAngle[0] = -0.5235988f;
    menuRingColumnAngle[1] = -0.34906587f;
    menuRingColumnAngle[2] = -0.17453294f;
    menuRingColumnAngle[3] = 0;
    yg2PathInit();
    ysegfResetSelection();
    menuRebuildCursorBuffers();
    menuMessageTextUpdate();

    if (menuKeyCheckPending != 0) {
        if (mainCommandKindTable[0] == 0) {
            menuSlots[0].state = 1;
            cursorBufferSetOffset(*slotLink + 1, 0);
        } else {
            menuKeyCheckPending = 0;
        }
    }
}

extern const float menuModelRowHeight;
extern const float menuModelRowStep;
extern const float menuModelBaseY;
extern int mainMenuBlockBaseY[];

void menuModelDraw(int index, float tx, float ty, float tz, float rotX, float rotY, float scale, int hasTransform) {
    void *txyBlock;
    void *modelId;

    if (hasTransform != 0) {
        xglMatrixStackUnit();
        xglMatrixStackRotY(0.2f);
        menuModelDrawVector.x = -2.75f;
        menuModelDrawVector.z = 9.5f;
        menuModelDrawVector.w = 1.0f;
        menuModelDrawVector.y = menuModelBaseY - (float) mainMenuBlockBaseY[0] * menuModelRowHeight * menuModelRowStep;
        xglMatrixStackTrans(&menuModelDrawVector);
        xglMatrixStackRotX(rotX);
        xglMatrixStackRotY(rotY);
        menuModelDrawVector.x = tx;
        menuModelDrawVector.y = ty;
        menuModelDrawVector.z = tz;
        menuModelDrawVector.w = 1.0f;
        xglMatrixStackTrans(&menuModelDrawVector);
        menuModelDrawVector.x = menuModelDrawVector.y = menuModelDrawVector.z = scale;
        menuModelDrawVector.w = 1.0f;
        xglMatrixStackScale(&menuModelDrawVector);
        xglMatrixStackSave(menuModelDrawMatrix);
    }
    nmlModelSetDrawMatrix(menuModelDrawMatrix);
    txyBlock = menuSkillModelTxyBuffers[index];
    if (txyBlock != 0) {
        nmlModelSetTexture(txyBlock);
    }
    modelId = menuSkillModelPxyBuffers[index];
    if (modelId != 0) {
        pxySubmitModel(modelId);
    }
}


void menuModelDrawRotated(int index, float tx, float ty, float tz, float rotX, float rotY, float rotZ, float yaw, int hasTransform) {
    void *txyBlock;
    void *modelId;
    float modelScale;

    if (hasTransform != 0) {
        xglMatrixStackUnit();
        xglMatrixStackRotY(yaw);
        menuModelRotatedVector[0].x = tx;
        menuModelRotatedVector[0].y = ty;
        menuModelRotatedVector[0].z = tz;
        menuModelRotatedVector[0].w = 1.0f;
        xglMatrixStackTrans(menuModelRotatedVector);
        xglMatrixStackRotX(rotX);
        xglMatrixStackRotY(rotY);
        xglMatrixStackRotZ(rotZ);
        modelScale = 5.0f;
        menuModelRotatedVector[0].w = 1.0f;
        menuModelRotatedVector[0].z = modelScale;
        menuModelRotatedVector[0].y = modelScale;
        menuModelRotatedVector[0].x = modelScale;
        xglMatrixStackScale(menuModelRotatedVector);
        xglMatrixStackSave(menuModelRotatedMatrix[0]);
    }
    nmlModelSetDrawMatrix(menuModelRotatedMatrix[0]);
    txyBlock = menuSkillModelTxyBuffers[index];
    if (txyBlock != 0) {
        nmlModelSetTexture(txyBlock);
    }
    modelId = menuSkillModelPxyBuffers[index];
    if (modelId != 0) {
        pxySubmitModel(modelId);
    }
}


typedef union FloatBits {
    float f;
    int i;
} FloatBits;

extern float selectionRingOpacity;


void menuSelectionRingDraw(int mode) {
    float columnSpeed[4];
    int passCount;
    PartyMember *ctx;
    int tick;
    int i;
    int row;
    int col;
    int entryCount;
    int k;
    unsigned char highlight;
    int busy;
    float zoom;
    float pulse;
    float f22;
    float f21;
    float tilt;
    float f20;
    float cellRadius;
    float cellAngle;
    float prev;
    int flag3;
    int tailIdx;
    float cellY;
    int flag;
    float spacing;

    passCount = (mode != 0) ? 2 : 1;
    if ((menuSlots[3].flags & CONTROL_SLOT_FLAG_ACTIVE) && *(unsigned char *) &menuSkillModelsReady != 0) {
        ctx = ptGetMenuMember();
        flag = menuSlots[3].flag;
        switch (flag) {
        case 1:
        case 2:
            i = 128 - menuSlots[3].value;
            break;
        case 3:
        case 4:
            i = menuSlots[3].value;
            break;
        default:
            i = 128;
            break;
        }
        i = (i * i) >> 7;
        zoom = (float) i * 0.0078125f;
        f20 = 3.14159274f;
        tick = xglTickGet();
        pulse = (xglSin((float) (tick & 0xF) * f20 * 0.125f) + 1.0f) * 1.5f;
        tick = xglTickGet();
        f21 = (float) ((~tick) & 0xFFF) * f20 * 0.001953125f;
        f22 = zoom * 0.4f;
        for (i = 0; i < passCount; i++) {
            {
                nmlModelSetTransparency(zoom * 0.7f);
                menuModelDraw(2, 0.0f, 0.0f, 0.0f, menuRingTilt, f21, 0.2f, 1);
                nmlModelSetTransparency(f22);
                menuModelDraw(7, 0.0f, 0.0f, 0.0f, menuRingTilt, f21, 0.2f, 0);
                nmlModelSetTransparency(f22);
                menuModelDraw(8, 0.0f, 0.0f, 0.0f, menuRingTilt, f21, 0.2f, 0);
                nmlModelSetTransparency(f22);
                menuModelDraw(9, 0.0f, 0.0f, 0.0f, menuRingTilt, f21, 0.2f, 0);
                nmlModelSetTransparency(f22);
                menuModelDraw(10, 0.0f, 0.0f, 0.0f, menuRingTilt, f21, 0.2f, 0);
                nmlModelSetTransparency(pulse * zoom);
                menuModelDraw(10 - skillBoardCursorTable[0].cursor, 0.0f, 0.0f, 0.0f, menuRingTilt, f21, 0.2f, 0);
            }
        }
        columnSpeed[0] = columnSpeed[1] = columnSpeed[2] = columnSpeed[3] = 0.0210000016f;
        for (i = 0; i < 28; i++) {
            col = i >> 3;
            row = i & 7;
            entryCount = selectionSlotTakenCount(ctx, col, row);
            if (entryCount != 0) {
                cellRadius = (float) (3 - col) * 0.184f + 0.355f;
                cellY = 0.0f;
                if (col < 3) {
                    cellAngle = (float) row;
                    cellAngle *= 3.14159274f;
                    cellAngle *= 0.25f;
                    f22 = xglCos(cellAngle) * cellRadius;
                    f20 = xglSin(cellAngle) * cellRadius;
                    columnSpeed[col] = columnSpeed[col] - 0.0005f * (float) entryCount;
                } else {
                    cellAngle = (float) row;
                    cellAngle *= 3.14159274f;
                    cellAngle *= 0.5f;
                    f22 = xglCos(cellAngle) * cellRadius;
                    f20 = xglSin(cellAngle) * cellRadius;
                    columnSpeed[col] = columnSpeed[col] - 0.001f * (float) entryCount;
                }
                busy = selectionGroupIsComplete(ctx, col);
                if (passCount != 0) {
                    k = passCount;
                    do {
                        if ((unsigned char) menuSlots[3].state != 4 && (unsigned char) menuSlots[3].state != 1 &&
                            (skillBoardCursorTable[0].cursor * 8 + skillBoardCursorTable[1].cursor) == i) {
                            highlight = 1;
                            nmlModelSetTransparency(pulse * zoom);
                        } else {
                            highlight = 0;
                            nmlModelSetTransparency(zoom);
                        }
                        menuModelDraw(entryCount + 2, f22, cellY, f20, menuRingTilt, -menuRingColumnAngle[i >> 3], 0.2f, 1);
                        if (!((busy != 0 || entryCount < 3) && highlight == 0)) {
                            nmlModelSetTransparency(zoom);
                            menuModelDraw(entryCount + 2, f22, cellY, f20, menuRingTilt, -menuRingColumnAngle[i >> 3], 0.2f, 0);
                        }
                        k--;
                    } while (k != 0);
                }
            }
        }
        f21 = menuRingTilt;
        tilt = f21;
        tilt += (tilt - menuRingTiltPrev) * 0.7f;
        tilt += (float) PadData[0].rightY * 0.000199999995f;
        menuRingTiltPrev = f21;
        if (tilt < 0.0f) {
            tilt = 0.0f;
        }
        menuRingTilt = tilt;
        if (1.57079637f < tilt) {
            ((FloatBits *) &menuRingTilt)->f = 1.57079637f;
        }
        if (PadData[0].directionHeld & PAD_R3) {
            tilt = menuRingTilt + (0.471238941f - menuRingTilt) * 0.1f;
            menuRingTilt = tilt;
        }
        for (i = 0; i < 4; i++) {
            menuRingColumnAngle[i] = menuRingColumnAngle[i] + columnSpeed[i];
            while (6.2831855f < menuRingColumnAngle[i]) {
                menuRingColumnAngle[i] -= 6.2831855f;
            }
        }
    }
    if (menuSkillModelLoadTally[0] != 3 || menuSkillModelLoadTally[1] != menuSkillModelLoadTally[0]) {
        selectionRingOpacity = 0.0f;
    }
    if (!(mainMenuBlock.flags & 2) && (menuSlots[7].flags & CONTROL_SLOT_FLAG_ACTIVE) && menuSlots[7].flag == 0 &&
        ((mainMenuBlock.layout < 2U) || mainMenuBlock.layout == 5 ||
         mainMenuBlock.layout == 6)) {
        selectionRingOpacity = selectionRingOpacity + (1.0f - selectionRingOpacity) * 0.08f;
    } else {
        selectionRingOpacity = selectionRingOpacity * 0.9f;
    }
    if (menuSkillModelLoadTally[0] == 3) {
        if (menuSkillModelLoadTally[1] == menuSkillModelLoadTally[0]) {
            if (0.0f < selectionRingOpacity) {
                nmlModelSetTransparency(selectionRingOpacity);
                menuModelDrawRotated(0, -2.8f, -2.4f, 3.6f, -0.2f, -0.4f, -0.1f, 0.0f, 1);
                nmlModelSetTransparency(selectionRingOpacity);
                menuModelDrawRotated(1, 2.6f, -2.4f, 2.6f, -0.2f, -0.4f, -0.1f, -0.24f, 1);
            }
        }
    }
}

/* Submits the four object requests of the layout at base: ids 0xE4 and 0xE6, then 0xEA twice. */
void menuDrawLabelBarFrame(int base) {
    menuDrawCell(0xE4, 0, base, 0x11);
    menuDrawCell(0xE6, 0, base - 0x1D, 0x11);
    menuDrawCellSized(0xEA, 0, base, 0x200, 0x10, 0x801);
    menuDrawCellSized(0xEA, 0, base + 0x19, 0x200, 0x10, 0x801);
}


/* Draws the menu overlays, each one gated by its flag, then flushes the font queue. */
void menuSceneDrawOverlays(void) {
    ptState.menuSlot = mainMenuBlock.cursorByte;
    if (menuTutorialPanelFlag != 0) {
        mainMenuBlock.position.target_pair.y = -0x30;
    } else {
        mainMenuBlock.position.target_pair.y = 0;
    }
    if (menuTutorialActive != 0) {
        screenFadeQuadDraw(0x40);
    }
    fontObjResetAndBuild();
    if (menuChoiceOpenFlag != 0) {
        choiceDraw();
    }
    if (countdownActive != 0) {
        countdownDraw();
    }
    xglFontFlush();
}

extern int *mainSlotLink[];
extern u8 menuMainCardLabelMarkup[];
extern char menuMainCardLabelBuffer[];
extern signed char menuMainCardLabelRangeTable[];
extern short menuMainCardLabelIdTable[];

void menuDrawMainCards(ControlSlot *slot) {
    int originY;
    int labelCount;
    int i;
    int labelSpan;
    int lo;
    int centerX;
    int width;
    int w;
    int scale;
    int k13;
    short *labelIds;
    char *labelBuffer;
    char *labelText;
    int cellDrawX;
    int cellPitch;
    u8 *labelMarkup;
    int cellW;
    int cellX;
    int cardX;
    int w2;
    int k;
    int slots;

    if (menuTutorialActive != 0 && (slot->flags & 4) == 0) {
        return;
    }
    originY = mainMenuBlockBaseY[0];
    fontObjResetAndBuild();
    menuDrawScrollFrame(menuTutorialPanelFlag * 3 + 4);
    if (menuCurrentSlot != 0 && menuCurrentSlot != 9) {
        if (menuCurrentSlot != 10) {
            menuDrawLabelBarFrame(originY + 57);
            if ((menuSceneFlags & 2) == 0) {
                labelCount = 12;
                if (menuCurrentSlot != 7) {
                    labelCount = *mainSlotLink[0] * 2 + (menuCopilotModeFlag != 0);
                }
                labelIds = menuMainCardLabelIdTable + menuMainCardLabelRangeTable[labelCount * 2];
                labelCount = menuMainCardLabelRangeTable[labelCount * 2 + 1];
                textDrawSegment(0, 0, menuTextInitMarkup, 0);
                i = 0;
                if (labelCount > 0) {
                    labelBuffer = menuMainCardLabelBuffer + 2;
                    labelText = menuMainCardLabelBuffer;
                    labelMarkup = menuMainCardLabelMarkup;
                    labelSpan = 612;
                    do {
                        slots = labelCount + 1;
                        centerX = labelSpan / slots - 50;
                        labelSpan += 612;
                        strcpy(labelBuffer, menuMainTxdEntry(*labelIds++));
                        width = xglFontGetStringWidth(labelText);
                        if (i == menuTabCursor[0].cursor || menuCurrentSlot == 7) {
                            labelMarkup[2] = 0x0D;
                            labelMarkup[1] = 0x0D;
                            k13 = 13;
                            w = width * k13;
                            width = w >> 4;
                            labelMarkup[4] = 0x6D;
                            labelMarkup[5] = 0x72;
                            labelMarkup[6] = 0x7F;
                            labelMarkup[10] = 0x0B;
                            labelMarkup[11] = 0x20;
                            labelMarkup[12] = 0x54;
                            textDrawSegment(0, 0, labelMarkup, 0);
                            textDrawSegment(centerX - (w >> 5), originY + 61, labelText, 0);
                            if (menuCurrentSlot == 4 || (unsigned char) menuSlots[menuCurrentSlot].state == 1) {
                                menuDrawSelectArrows(centerX - ((width + 16) >> 1), width + 16, originY);
                            }
                        } else {
                            w2 = 12;
                            w2 = width * w2;
                            labelMarkup[2] = 12;
                            labelMarkup[1] = 12;
                            labelMarkup[10] = 0;
                            labelMarkup[11] = 1;
                            labelMarkup[4] = 0x38;
                            labelMarkup[5] = 0x37;
                            labelMarkup[6] = 0x6A;
                            labelMarkup[12] = 0x4E;
                            textDrawSegment(0, 0, labelMarkup, 0);
                            textDrawSegment(centerX - (w2 >> 5), originY + 62, labelText, 0);
                        }
                        i++;
                    } while (i < labelCount);
                }
            }
        }
    }

    for (i = 0; i < 6; i++) {
        w = i * 83;
        cardX = w + 9;
        panelDrawNineSlice(menuCardPanelFrame, cardX, originY + 16, 79, 32);
        if ((unsigned int) (menuCurrentSlot - 1) < 8 || (unsigned char) slot->state == 0) {
            if (i == slot->link[0].cursor) {
                panelDrawNineSlice(menuCardSelectedPanelFrame, w + 1, originY + 8, 95, 48);
            }
            if (i == menuCurrentSlot - 1 ||
                ((menuCurrentSlot == 0 || menuCurrentSlot == 7) && i == slot->link[0].cursor && (menuSceneFlags & 2) == 0) ||
                ((menuSceneFlags & 2) != 0 && i == menuPreviousCardCursor)) {
                menuDrawCell(203 + i * 2, cardX, originY + 16, 1);
                if (menuCurrentSlot != 0 && (menuSceneFlags & 2) == 0) {
                    menuDrawCell(224, cardX + 31, originY + 39, 1);
                }
            } else {
                menuDrawCell(202 + i * 2, cardX, originY + 16, 1);
            }
        } else {
            menuDrawCell(202 + i * 2, cardX, originY + 16, 1);
        }
    }

    if (menuCurrentSlot == 0 || menuCurrentSlot == 9 || menuCurrentSlot == 10) {
        centerX = menuCardAvailabilityBits >> 4;
        if (centerX == 0) {
            centerX = 1;
        }
        for (i = 0; i < (int) centerX; i++) {
            if (mainCommandKindTable[i] != -1) {
                cellPitch = 0x1F0 / (int) centerX;
                cellX = cellPitch * i;
                cellW = cellPitch - 4;
                cellDrawX = cellX + 10;
                panelDrawNineSlice(menuCardPanelFrame, cellDrawX, originY + 58, cellW, 24);
                if ((unsigned char) slot->state == 1 && i == slot->link[1].cursor) {
                    panelDrawNineSlice(menuCardSelectedPanelFrame, cellX + 2, originY + 50, cellPitch + 12, 40);
                    menuDrawCell(mainCommandKindTable[i] * 2 + 215, cellDrawX + (cellW >> 1) - 48, originY + 54, 1);
                } else {
                    menuDrawCell(mainCommandKindTable[i] * 2 + 214, cellDrawX + (cellW >> 1) - 48, originY + 54, 1);
                }
            }
        }
    }
    msgRangeDrawRevealed(menuMessageRangeState, 24, (menuTutorialPanelFlag != 0) ? 348 : 392);
    if ((menuTutorialHintFlags & 2) != 0) {
        menuDrawKindIcon(440, 368, 3);
        menuDrawKindIcon(440, 388, 0);
        menuDrawKindIcon(440, 408, 1);
        textDrawSegment(458, 370, menuMainTxdEntry(450), 0);
        textDrawSegment(458, 390, menuMainTxdEntry(451), 0);
        textDrawSegment(458, 410, menuMainTxdEntry(452), 0);
    } else if ((menuTutorialHintFlags & 4) != 0) {
        menuDrawKindIcon(440, 388, 3);
        menuDrawKindIcon(440, 408, 1);
        textDrawSegment(458, 390, menuMainTxdEntry(450), 0);
        textDrawSegment(458, 410, menuMainTxdEntry(452), 0);
    }
}


/* Draws the item list of the slot's link, then the labels in the table itemMenuBlock. */
void menuDrawSlotItems(ControlSlot *slot) {
    menuDrawItemList(0xD, 0x5F, slot->link);
    menuDrawItems(&itemMenuBlock);
}


/* Submits the four layout packets; menuCopilotModeFlag picks one of two coordinate sets. */
void menuDrawEtherPage(ControlSlot *slot) {
    menuDrawMemberItemList(0xD, 0xD4, learnedEtherCursor, learnedEtherList);
    if (menuCopilotModeFlag != 0) {
        menuDrawCell(0x161, 0x11, 0xB7, 1);
        menuDrawCell(0x163, 0xAF, 0xB7, 1);
        menuDrawCellSized(0x162, 0x21, 0xB7, 0x8E, 0x11, 1);
        menuDrawCell(0x139, 0x48, 0xB6, 1);
    } else {
        menuDrawCell(0x161, 0x11, 0xB7, 1);
        menuDrawCell(0x163, 0x9E, 0xB7, 1);
        menuDrawCellSized(0x162, 0x21, 0xB7, 0x7D, 0x11, 1);
        menuDrawCell(0x139, 0x40, 0xB6, 1);
    }
    menuDrawItems(&itemMenuBlock);
}

extern int menuLicenseState;
extern signed char menuTutorialToggleFlags;

void menuDrawSkillPage(ControlSlot *slot) {
    int baseY = mainMenuBlock.position.current_pair.y;
    int show;

    if (menuCursorLatch.word != -1 && (slot->flags & CONTROL_SLOT_FLAG_MARK) == 0) {
        ptState.menuSlot = menuCursorLatch.bytes[0];
    } else if (menuCopilotModeFlag != 0) {
        menuCursorLatch.word = ptState.menuSlot;
    }

    show = 0;
    if (menuTutorialActive != 0) {
        if ((slot->flags & 4) != 0) {
            if (((u8) menuTutorialToggleFlags & 4) != 0) {
                show = 1;
            }
        } else if (((u8) menuTutorialToggleFlags & 4) == 0) {
            show = 1;
        }
    } else {
        show = 1;
    }
    if (show) {
        menuDrawClassInfo(17, baseY + 332);
    }

    show = 0;
    if (menuTutorialActive != 0) {
        if ((slot->flags & 4) != 0) {
            if (((u8) menuTutorialToggleFlags & 2) != 0) {
                show = 1;
            }
        } else if (((u8) menuTutorialToggleFlags & 2) == 0) {
            show = 1;
        }
    } else {
        show = 1;
    }
    if (show) {
        menuDrawSkillBoard(254, baseY + 95, skillBoardCursorTable, &skillBoardCursorTable[1], &skillBoardCursorTable[2], menuLicenseState - 4, menuItemCategoryMask);
    }

    if (menuCursorLatch.word != -1 && (slot->flags & CONTROL_SLOT_FLAG_MARK) == 0) {
        ptState.menuSlot = mainMenuBlock.cursorByte;
    }
}


/* Draws the attack or command panel of the actor and the matching info panel. */
void menuDrawCommandPage(ControlSlot *slot) {
    int noSavedCursor;

    if (slot->state == 5) {
        if (menuCopilotModeFlag != 0) {
            noSavedCursor = -1;
            if (menuSavedCursor != noSavedCursor) {
                ptState.menuSlot = (u8) menuSavedCursor;
            }
            menuDrawSpecialAttack(13, 183, commandPageCursor, ptGetMenuMember()->id, menuPartnerMask);
            menuDrawCopilotInfo(275, 95, commandPageCursor, ptGetMenuMember()->id, menuPartnerMask);
            if (menuSavedCursor != noSavedCursor) {
                ptState.menuSlot = mainMenuBlockCursor[0];
            }
        } else {
            menuDrawSpecialAttack(13, 183, commandPageCursor, ptGetMenuMember()->id, menuPartnerMask);
            menuDrawPartnerInfo(275, 95, commandPageCursor, ptGetMenuMember()->id, menuPartnerMask);
            return;
        }
    } else {
        if (menuCopilotModeFlag != 0) {
            noSavedCursor = -1;
            if (menuSavedCursor != noSavedCursor) {
                ptState.menuSlot = (u8) menuSavedCursor;
            }
            menuDrawAttackCommand(13, 183, commandPageCursor, ptGetMenuMember()->id);
            menuDrawCommandDetail(275, 95, commandPageCursor, ptGetMenuMember()->id);
            if (menuSavedCursor != noSavedCursor) {
                ptState.menuSlot = mainMenuBlockCursor[0];
            }
        } else {
            menuDrawAttackCommand(13, 183, commandPageCursor, ptGetMenuMember()->id);
            menuDrawCommandInfo(275, 95, commandPageCursor, ptGetMenuMember()->id);
            return;
        }
    }
}

extern u8 selectionRecordDispatchValues[];

void menuDrawEquipPage(ControlSlot *slot) {
    int listEntry = 0;
    int highlight;
    PartyMember *ctx;
    ListCursor *cursor;
    void *text;
    int x;
    int y;
    int style;
    int lineY;

    if (menuCopilotModeFlag != 0) {
        if (menuSavedCursor != -1) {
            ptState.menuSlot = (u8) menuSavedCursor;
        }
        if (menuTabCursor[0].cursor == 0 && (u8) ((u8) slot->state - 4) < 2) {
            ctx = ptGetMenuMember();
            if (ctx->buffer->entry_count < menuEquipRowCount) {
                ctx = ptGetMenuMember();
                highlight = ctx->buffer->entry_count;
            } else {
                highlight = equipSlotCursor[0].cursor;
            }
            cursor = equipItemCursor;
            if (cursor->capacity == 0) {
                highlight = -1;
            } else if (((cursor->mode_flags | (equipSlotCursor)->mode_flags) & 1) == 0) {
                highlight = -1;
            } else {
                ctx = ptGetMenuMember();
                if (selectionFindIndex(ctx, selectionAccessoryList[cursor->cursor]) == -1) {
                    listEntry = selectionAccessoryList[cursor->cursor];
                } else {
                    highlight = -1;
                }
            }
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xC5, 0x63, ctx, listEntry, highlight);
        } else if (menuTabCursor[0].cursor == 0 && (u8) slot->state == 6) {
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xC5, 0x63, ctx, 0, equipSlotCursor[0].cursor);
        } else {
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xC5, 0x63, ctx, 0, -1);
        }
        menuDrawPlainItemList(0x124, 0x5F, equipItemCursor);
        ctx = ptGetMenuMember();
        menuDrawEquipPanel(0x7C, 0xF7, equipSlotCursor, ctx->buffer->entries, menuEquipRowCount);
        menuDrawItemHighlighted(&mainMenuBlock, ptState.menuSlot + 3);
        menuDrawItemHighlighted(&mainMenuBlock, ptState.menuSlot + 6);
        if (menuSavedCursor != -1) {
            ptState.menuSlot = mainMenuBlock.cursorByte;
        }
    } else {
        if (menuTabCursor[0].cursor == 0 && (u8) ((u8) slot->state - 4) < 2) {
            ctx = ptGetMenuMember();
            if (ctx->buffer->entry_count < menuEquipRowCount) {
                ctx = ptGetMenuMember();
                highlight = ctx->buffer->entry_count;
            } else {
                highlight = equipSlotCursor[0].cursor;
            }
            cursor = equipItemCursor;
            if (cursor->capacity != 0 && ((cursor->mode_flags | (equipSlotCursor)->mode_flags) & 1) != 0 &&
                selectionFindIndex(ptGetMenuMember(), equipItemList[cursor->cursor]) == -1) {
                listEntry = selectionRecordDispatchValues[equipItemList[cursor->cursor] * 6];
            } else {
                highlight = -1;
            }
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xB4, 0x63, ctx, listEntry, highlight);
        } else if (menuTabCursor[0].cursor == 0 && (u8) slot->state == 6) {
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xB4, 0x63, ctx, 0, equipSlotCursor[0].cursor);
        } else {
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xB4, 0x63, ctx, 0, -1);
        }
        if (menuTabCursor[0].cursor != 0) {
            fontObjSetOpacity(0x40);
        }
        ctx = ptGetMenuMember();
        menuDrawEquipPanel(0x6B, 0xF7, equipSlotCursor, ctx->buffer->entries, menuEquipRowCount);
        fontObjResetOpacity();
        menuDrawItemCategory(0x113, 0x5F, equipItemCursor, equipItemList, menuItemCategoryMask);
    }

    if (menuTabCursor[0].cursor == 0 && (u8) slot->state != 1) {
        fontTrackSetTarget(equipPanelTrack, -0x10, (menuEquipRowCount << 4) + menuCopilotModeFlag * 12 + 0x104);
    } else {
        fontTrackSetTarget(equipPanelTrack, -0x110, (menuEquipRowCount << 4) + menuCopilotModeFlag * 12 + 0x104);
    }
    x = equipPanelTrack[0].current_pair.x;
    y = equipPanelTrack[0].current_pair.y;
    panelDrawNineSlice(menuPanelFrame, x, y, 0x77, 0x30);
    menuDrawKindIcon(x + 0x18, y + 4, 3);
    menuDrawKindIcon(x + 0x18, y + 0x18, 2);
    if ((u8) slot->state == 6) {
        textDrawSegment(x + 0x2A, y + 5, menuMainTxdEntry(0x108), 0);
        textDrawSegment(x + 0x2A, y + 0x19, menuMainTxdEntry(0x109), 0);
        return;
    }
    textDrawSegment(x + 0x2A, y + 5, menuMainTxdEntry(0x106), 0);
    lineY = y + 0x19;
    text = menuMainTxdEntry(0x107);
    ctx = ptGetMenuMember();
    if (ctx->buffer->entry_count == 0 || (u8) slot->state == 5) {
        style = 2;
    } else {
        style = 0;
    }
    textDrawSegment(x + 0x2A, lineY, text, style);
}

void menuPartyFormationDrawNone(ControlSlot *slot) {

}


/* Draws the detail panel of the record's value through menuDrawItemListDetail. */
void menuDrawKeyItemDetail(ControlSlot *slot) {
    menuDrawItemListDetail(0x1F, 0x5F, slot->link);
}


/* Draws the controller page for a slot whose state is 7, otherwise the general config page. */
void menuDrawConfigPage(ControlSlot *slot) {
    if ((unsigned char) slot->state != 7) {
        menuDrawConfig(121, 100, slot->link, menuConfigRowKindTable);
    } else {
        do {
            menuDrawControllerConfig();
        } while (0);
    }
}

/* Draws the U.M.N. destination list (title "  Map") with one row per destination. Row i's track lane (i & 1) takes the highlight target, 128 on the selected item and 0 otherwise, and the packet scale is that lane's current value. */
void menuMapListDraw(ListCursor *list);

void menuMapDraw(ControlSlot *slot) {
    do {
        menuMapListDraw(slot->link);
    } while (0);
}

extern u8 extraTitleText[];

/* Draws the scene's font track, panels and text, then the item list when the state is 5. */
void menuDrawPartyPage(ControlSlot *slot) {
    int rosterCount;
    int i;
    int x;
    int y;
    int step;
    PartyMember *member;

    menuDrawItems(&mainMenuBlock);
    if (mainMenuBlock.layout == 0 || mainMenuBlock.layout == 5) {
        fontTrackSetTarget(&extraPanelTrack, 0x118, 0);
    } else {
        fontTrackSetTarget(&extraPanelTrack, 0x200, 0xFF);
    }
    fontTrackEase(&extraPanelTrack);
    panelDrawNineSlice(menuPanelFrame, extraPanelTrack.current_pair.x, 0x149, 0xF0, 0x2B);
    menuDrawTitledFrame(extraPanelTrack.current_pair.x + 6, 0x14E, 0x6B, 0x21, extraTitleText);
    menuDrawPlayTime(extraPanelTrack.current_pair.x + 0x75, 0x14E);
    if (menuExtraPageIndex != 0) {
        textDrawGlyphString(extraPanelTrack.current_pair.x + 0x16, 0x15D, menuMainTxdEntry(menuExtraPageIndex + 0xB),
                            (((u64) 0x3F800000 << 0x10) | 0x8054) << 0x10 | 0x200B);
    }
    if ((unsigned int) (mainMenuBlock.layout - 5) < 2U) {
        /* the word at 0x14 is scaled_pair.y of the track; the menu reads it as the border step */
        step = extraPanelTrack.scaled_pair.y;
        menuDrawBorder((-(step * 0x29) >> 0xC) + 0x143, ((step * 4) >> 0xC) + 0x114,
                       ((step * 0x29) >> 0xC) + 0xAE, ((step * 0x31) >> 0xC) + 0x27,
                       (((u64) 0x3F800000 << 0x10) | 0x8038) << 0x10 | 0x1A1A);
        menuDrawCell(0x13F, 0x190, ((step * 4) >> 0xC) + 0x104, 1);
        menuDrawCell(0x140, 0x1C4, ((step * 4) >> 0xC) + 0x104, 1);
    }
    if (mainMenuBlock.layout == 5) {
        rosterCount = 0;
        for (i = 0; i < 4; i++) {
            if (ptState.roster[6 + i] != 0) {
                rosterCount += 1;
            }
        }
        step = -(rosterCount * 12);
        y = step + 0x19A;
        for (i = 0; i < 4; i++) {
            member = ptState.roster[6 + i];
            if (member != 0) {
                menuDrawLitIcon(y, 0x11B, member->status->memberIndex);
                y += 0x18;
            }
        }
    }
}


/* Runs the slot's callback (its first word) when the slot's category is nonzero, flushes the font, then fades the
   screen by the slot's flag and value when the category is 2. */
void menuSlotDraw(int index) {
    int slotFlag;
    unsigned int category;
    int value;
    int fade;

    if (menuSlots[index].callback == 0) {
        return;
    }
    if ((menuSlots[index].flags & CONTROL_SLOT_FLAG_ACTIVE) == 0) {
        return;
    }
    slotFlag = menuSlots[index].flag;
    switch (slotFlag) {
    case 1:
    case 2:
        category = menuSlots[index].value < 129 ? 2 : 0;
        break;
    case 3:
    case 4:
        category = 1;
        if (menuSlots[index].value < 129) {
            category = 2;
        } else {
            category = 1;
        }
        break;
    default:
        category = 1;
        break;
    }
    if (category == 2) {
        grSetFrameBase();
        grResetRenderState();
    }
    fontObjResetAndBuild();
    if (category != 0) {
        menuSlots[index].callback(&menuSlots[index]);
    }
    xglFontFlush();
    if (category != 2) {
        return;
    }
    fade = 0;
    grSetRenderFrame();
    switch (menuSlots[index].flag) {
    case 1:
        fade = menuSlots[index].value;
        break;
    case 2:
        fade = 0 - menuSlots[index].value;
        break;
    case 3:
        fade = menuSlots[index].value;
        fade = 128 - (fade * fade) / 128;
        break;
    case 4:
        fade = menuSlots[index].value;
        fade = (fade * fade) / 128 - 128;
        break;
    }
    grDrawScreenFade(fade);
}

extern u8 menuLayoutMode;
extern int menuTutorialIndex;
extern FontTrack menuBandTrack[];

/* Runs the eleven slot callbacks and the overlays, then the tutorial block when menuTutorialActive is set. */
void menuSceneDraw(void) {
    int i;

    for (i = 0; i < 11; i++) {
        menuSlotDraw(i);
    }
    menuSceneDrawOverlays();
    if (menuTutorialActive != 0) {
        ControlSlot *slot = &menuSlots[0];
        u8 *slot0Flags = &slot->flags;
        u8 *slot3Flags = &slot[3].flags;

        *slot0Flags |= 4;
        menuSlotDraw(0);
        *slot0Flags &= 0xfb;
        *slot3Flags |= 4;
        menuSlotDraw(3);
        *slot3Flags &= 0xfb;
        fontObjResetAndBuild();
        {
            switch (menuLayoutMode) {
            case 1:
            case 2:
                choiceDraw();
                break;
            case 3:
            case 4:
                countdownDraw();
                break;
            }
        }
        if (menuTutorialStage != 0) {
            fontTrackEase(menuBandTrack);
            menuDrawAnimatedBand(menuBandTrack[0].current_pair.x, menuBandTrack[0].current_pair.y + mainMenuBlockBaseY[0]);
            if (menuTutorialStage >= 2) {
                textDrawSegment(0, 0, menuTextInitMarkup, 0);
                textDrawSegment(48, 382, menuTutorialTxdEntry(menuTutorialIndex + 1), 0);
                textDrawSegment(48, 402, menuTutorialTxdEntry(menuTutorialIndex + 2), 0);
            }
        }
        xglFontFlush();
    }
}

extern SelectionRecord selectionRecordTable[];

void menuPlaceKey(PartyMember *context, int key, int count) {
    SelectionRecord *record;
    int index;

    index = selectionFindIndex(context, key);
    if (index != -1) {
        context->buffer->entries[index] = context->buffer->entries[count];
        record = &selectionRecordTable[context->buffer->entries[count]];
        calcSkillEquipOrg(context->id, index, record->dispatch_value);
    }
    context->buffer->entries[count] = key;
    record = &selectionRecordTable[key];
    calcSkillEquipOrg(context->id, count, record->dispatch_value);
}


void menuTakePlainEntry(PartyMember *context, int plainIndex, int row) {
    context->buffer->entries[row] = selectionAccessoryList[plainIndex];
    calcAccEquipOrg(context->id, row, selectionAccessoryList[plainIndex]);
    selectionListDropAt(plainIndex);
    cursorBufferSetCapacity(equipItemCursor, selectionPlainCount);
}

/* Advances the back or front slot cursor of ptState until the chosen slot is filled, and starts voice 0x01000001 when the cursor moved. */
void menuAdvanceSlotCursor(void) {
    unsigned char previous;

    if (menuCopilotModeFlag != 0) {
        previous = ptState.backIndex;
        do {
            ptState.backIndex++;
            ptState.backIndex %= 2;
        } while (ptState.slots[7 + ptState.backIndex] == 0);
        if (previous != ptState.backIndex) {
            seVoiceStartDefault(0x01000001);
        }
    } else {
        previous = ptState.frontIndex;
        do {
            ptState.frontIndex++;
            ptState.frontIndex %= 3;
        } while (ptState.slots[ptState.frontIndex] == 0);
        if (previous != ptState.frontIndex) {
            seVoiceStartDefault(0x01000001);
        }
    }
}

extern u8 menuNewlineText[];

/* Loads the two message buffers, starts the voice cue 0x01000003 and sets countdownActive (countdownActive). */
void menuCountdownStart(void) {
    void *firstBuffer;
    void *secondBuffer;

    firstBuffer = menuMainTxdEntry(0x49);
    countdownInit(firstBuffer, 0x12C);
    msgBufferAppend(menuNewlineText);
    secondBuffer = menuMainTxdEntry(0x4A);
    msgBufferAppend(secondBuffer);
    seVoiceStartDefault(0x01000003);
    countdownActive = 1;
}

int sprintf(char *target, const char *format, ...);
extern u8 fiveDigitNumberFormat[];

/* Formats value as "%5d" into a shared buffer and returns that buffer. */
char *menuFormatNumber5(int value) {
    sprintf(menuNumberBuffer, fiveDigitNumberFormat, value);
    return menuNumberBuffer;
}

void menuLinkSetCurrent(ListCursor *link);
void menuPromptSet(u32 mode);
extern unsigned char licenseSlotState[];

/* Selects one of the five menu layouts by mode and appends the matching message strings. */
void menuPromptSet(u32 mode) {
    switch (mode) {
    case 0:
        choiceInit((void *) 0, 0);
        countdownInit((void *) 0, 0);
        break;
    case 1: {
        ListCursor *links = skillBoardCursorTable;

        choiceInit(menuMainTxdEntry(links[0].cursor + 0x36), -1);
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(links[1].cursor + 0x3A));
        msgBufferAppend(menuNewlineText);
        msgBufferAppend(menuMainTxdEntry(0x42));
        msgBufferAppend(menuFormatNumber5(selectionSlotOpenCostTable[links[1].cursor]));
        msgBufferAppend(menuNewlineText);
        msgBufferAppend(menuMainTxdEntry(0x44));
        break;
    }
    case 2: {
        ListCursor *links = skillBoardCursorTable;

        choiceInit(selectionSubTextGet(links[0].cursor, links[1].cursor, links[2].cursor, 0), -1);
        msgBufferAppend(menuNewlineText);
        msgBufferAppend(menuMainTxdEntry(0x43));
        msgBufferAppend(menuFormatNumber5(selectionSubCostGet(links[0].cursor, links[1].cursor, links[2].cursor)));
        msgBufferAppend(menuNewlineText);
        msgBufferAppend(menuMainTxdEntry(0x44));
        break;
    }
    case 3: {
        ListCursor *links = skillBoardCursorTable;

        countdownInit(menuMainTxdEntry(links[0].cursor + 0x36), -2);
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(links[1].cursor + 0x3A));
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(0x47));
        msgBufferAppend(menuNewlineText);
        msgBufferAppend(menuMainTxdEntry(0x4B));
        msgBufferAppend(menuFormatNumber5(selectionSlotRewardTable[links[0].cursor * 8 + links[1].cursor]));
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(0x4C));
        break;
    }
    case 4: {
        ListCursor *links = skillBoardCursorTable;

        countdownInit(menuMainTxdEntry(links[0].cursor + 0x36), -2);
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(links[1].cursor + 0x3A));
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(0x47));
        msgBufferAppend(menuNewlineText);
        msgBufferAppend(menuMainTxdEntry(links[0].cursor + 0x37));
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(0x48));
        break;
    }
    }
    menuLayoutMode = (signed char) mode;
}

extern u8 menuTutorialRestartCount;
/* Scene-change handler (menuState is the menu state id): clears the current slot, refreshes the cursors, then switches on menuState to set the menu mode and the cursor buffers. */
void menuSceneSetState(int menuState);
extern unsigned char menuTutorialSavedCells[];

/* Resets the cursor state and link table, saves the kind bytes of links 0 to 2 when menuCurrentSlot is 3, and copies the cursor buffer's header, cells, flags and limits into the saved globals (clearing them in the buffer). */
void menuTutorialSaveState(void) {
    u8 *savedHeader;
    u8 *savedCells;
    int index;

    cursorBufferSetOffset(mainCommandCursors, 2);
    menuPendingSelection = 0;
    if (menuCurrentSlot == 3) {
        menuTutorialSavedCursors[0] = ((u8 *) skillBoardCursorTable)[0x00];
        menuTutorialSavedCursors[1] = ((u8 *) skillBoardCursorTable)[0x14];
        menuTutorialSavedCursors[2] = ((u8 *) skillBoardCursorTable)[0x28];
        menuTutorialSavedLicenseState = menuSlots[3].state;
    } else {
        menuTutorialSavedLicenseState = 0;
    }
    index = 0;
    menuSceneSetState(3);
    savedHeader = menuTutorialSavedHeader;
    menuPlaceCursor(0, 0, 0, 0);
    menuSlots[3].state = 4;
    menuLinkSetCurrent(skillBoardCursorTable);
    cursorBufferSetOffset(menuTabCursor, 1);
    menuTutorialIndex = 0;
    menuTutorialToggleFlags = 0;
    menuPromptSet(0);
    menuTutorialRestartCount = 0;
    menuTutorialStage = 0;
    menuTutorialActive = 1;
    menuTutorialLine = 0;
    menuTutorialHintFlags = 0;
    do {
        *savedHeader = ptGetMenuMember()->buffer->header[index];
        ptGetMenuMember()->buffer->header[index] = 0;
        savedHeader += 1;
        index += 1;
    } while (index < 4);
    savedCells = menuTutorialSavedCells;
    index = 0;
    do {
        savedCells[0] = ptGetMenuMember()->buffer->cells[0][index];
        savedCells[8] = ptGetMenuMember()->buffer->cells[1][index];
        savedCells[16] = ptGetMenuMember()->buffer->cells[2][index];
        savedCells[24] = ptGetMenuMember()->buffer->cells[3][index];
        savedCells += 1;
        ptGetMenuMember()->buffer->cells[0][index] = 0;
        ptGetMenuMember()->buffer->cells[1][index] = 0;
        ptGetMenuMember()->buffer->cells[2][index] = 0;
        ptGetMenuMember()->buffer->cells[3][index] = 0;
        index += 1;
    } while (index < 8);
    menuTutorialSavedFlags0 = ptGetMenuMember()->buffer->openGroup;
    ptGetMenuMember()->buffer->openGroup = 0;
    menuTutorialSavedFlags1 = ptGetMenuMember()->buffer->takenCount;
    ptGetMenuMember()->buffer->takenCount = 0;
    menuTutorialSavedSkillPoints = ptGetMenuMember()->limits->skillPoints;
    ptGetMenuMember()->limits->skillPoints = 1000;
    menuTutorialSavedClassPoints = ptGetMenuMember()->limits->classPoints;
    ptGetMenuMember()->limits->classPoints = 1000;
    menuTutorialPanelFlag = 1;
    cursorBufferSetOffset(skillBoardCursorTable, 0);
    cursorBufferSetOffset(&skillBoardCursorTable[1], 0);
    cursorBufferSetOffset(&skillBoardCursorTable[2], 0);
}

void menuTutorialRestoreState(void) {
    int i;
    seVoiceStartDefault(MENU_SE_CANCEL);
    menuTutorialActive = 0;
    menuTutorialStage = 0;
    menuTutorialPanelFlag = 0;
    menuTutorialHintFlags = 0;
    menuPlaceCursor(0, 0, 0, 0);
    for (i = 0; i < 4; i++) {
        ptGetMenuMember()->buffer->header[i] = menuTutorialSavedHeader[i];
    }
    for (i = 0; i < 8; i++) {
        ptGetMenuMember()->buffer->cells[0][i] = menuTutorialSavedCells[i];
        ptGetMenuMember()->buffer->cells[1][i] = menuTutorialSavedCells[i + 8];
        ptGetMenuMember()->buffer->cells[2][i] = menuTutorialSavedCells[i + 16];
        ptGetMenuMember()->buffer->cells[3][i] = menuTutorialSavedCells[i + 24];
    }
    ptGetMenuMember()->buffer->openGroup = menuTutorialSavedFlags0;
    ptGetMenuMember()->buffer->takenCount = menuTutorialSavedFlags1;
    ptGetMenuMember()->limits->skillPoints = menuTutorialSavedSkillPoints;
    ptGetMenuMember()->limits->classPoints = menuTutorialSavedClassPoints;
    if (menuTutorialSavedLicenseState != 0) {
        ListCursor *records = skillBoardCursorTable;

        records[0].cursor = menuTutorialSavedCursors[0];
        records[1].cursor = menuTutorialSavedCursors[1];
        if (menuTutorialSavedCursors[0] == 3) {
            records[1].capacity = 4;
        } else {
            records[1].capacity = 8;
        }
        {
            ListCursor *tail = skillBoardCursorTable;

            tail[2].cursor = menuTutorialSavedThirdCursor;
        }
        menuSlots[3].state = 7;
        if ((unsigned char) (menuTutorialSavedLicenseState - 4) < 3) {
            menuLicenseState = menuTutorialSavedLicenseState;
        }
        cursorBufferSetOffset(menuTabCursor, 1);
        menuLinkSetCurrent(menuSlots[3].link + (menuSlots[3].state - 4));
    }
}

void menuTutorialRewind(void) {
    int found;
    u8 *entry;

    found = 0;
    while (menuTutorialIndex > 0) {
        entry = menuTutorialTxdEntry(menuTutorialIndex);
        if (*entry == '*') {
            if (found != 0) {
                break;
            }
            found = 1;
            if ((menuTutorialHintFlags & 1) == 0) {
                break;
            }
        }
        menuTutorialIndex -= 1;
    }
    if (menuTutorialIndex == 0) {
        menuTutorialRestoreState();
    }
}

void menuTutorialPageReset(void) {
    int i;

    i = 0;
    menuTutorialToggleFlags = 0;
    menuPromptSet(0);
    menuTutorialStage = 0;
    menuTutorialHintFlags = 0;
    do {
        ptGetMenuMember()->buffer->header[i] = 0;
        i += 1;
    } while (i < 4);
    i = 0;
    do {
        ptGetMenuMember()->buffer->cells[0][i] = 0;
        ptGetMenuMember()->buffer->cells[1][i] = 0;
        ptGetMenuMember()->buffer->cells[2][i] = 0;
        ptGetMenuMember()->buffer->cells[3][i] = 0;
        i += 1;
    } while (i < 8);
    ptGetMenuMember()->buffer->openGroup = 0;
    ptGetMenuMember()->buffer->takenCount = 0;
    ptGetMenuMember()->limits->skillPoints = 1000;
    ptGetMenuMember()->limits->classPoints = 1000;
    cursorBufferSetOffset(skillBoardCursorTable, 0);
    cursorBufferSetOffset(&skillBoardCursorTable[1], 0);
    cursorBufferSetOffset(&skillBoardCursorTable[2], 0);
    licenseSlotState[0] = 4;
    menuLinkSetCurrent(skillBoardCursorTable);
}

extern u8 menuTutorialTimer;
extern ListCursor licenseSecondCursor[];
extern ListCursor licenseThirdCursor[];

/* One step of the menu tutorial script at menuTutorialIndex: a "%" code runs a command (%e ends the tutorial, %r restarts it, %k/%p/%P/%w/%s wait for a press or a revealed line, %c, %f, %l, %a, %g, %m, %t, %j act on the cursor, font track, toggles, timers and jumps), "*" and 0 skip, other bytes are shown text. */
void menuTutorialStep(void) {
    u8 *text;
    int arg;
    int count;
    int trackX;
    int trackY;
    int index;
    ListCursor *links;
    PartyMember *ctx;

    index = menuTutorialIndex;
    if (index < 0) {
        index = 0;
    }
    menuTutorialIndex = index;
    text = menuTutorialTxdEntry(index);
    switch (text[0]) {
    case '%':
        switch (text[1]) {
        case 'e':
            menuTutorialRestoreState();
            break;
        case 'r':
            menuTutorialIndex = 0;
            menuTutorialLine = 0;
            menuTutorialPageReset();
            menuTutorialRestartCount += 1;
            break;
        case 'k':
            if (padCirclePressedAlone()) {
                seVoiceStartDefault(MENU_SE_CONFIRM);
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
                menuTutorialHintFlags &= 0xFC;
            } else if (padCrossPressedAlone()) {
                menuTutorialRewind();
                if (menuTutorialActive != 0) {
                    seVoiceStartDefault(MENU_SE_CANCEL);
                }
                menuTutorialHintFlags &= 0xFD;
            } else {
                menuTutorialHintFlags |= 2;
            }
            break;
        case 'w':
            if (msgRangeIsRevealed(menuMessageRangeState)) {
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
            }
            break;
        case 'p':
            if (msgRangeIsRevealed(menuMessageRangeState)) {
                if (padCirclePressedAlone()) {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    index = menuTutorialIndex + 1;
                    menuTutorialIndex = index;
                    menuTutorialHintFlags &= 0xFC;
                } else if (padCrossPressedAlone()) {
                    menuTutorialRewind();
                    if (menuTutorialActive != 0) {
                        seVoiceStartDefault(MENU_SE_CANCEL);
                    }
                    menuTutorialHintFlags &= 0xFD;
                } else {
                    menuTutorialHintFlags |= 2;
                }
            }
            break;
        case 'P':
            if (msgRangeIsRevealed(menuMessageRangeState)) {
                if (padCirclePressedAlone()) {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    index = menuTutorialIndex + 1;
                    menuTutorialIndex = index;
                    menuTutorialHintFlags &= 0xFA;
                } else {
                    menuTutorialHintFlags |= 4;
                }
            }
            break;
        case 't':
            count = text[2] * 10 + text[3] - 0x210;
            menuTutorialTimer += 1;
            if (menuTutorialTimer >= count) {
                menuTutorialTimer = 0;
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
            }
            break;
        case 'c':
            arg = text[2];
            switch (arg) {
            case '+':
                if (menuCurrentLink != 0) {
                    cursorBufferMoveBy(menuCurrentLink, 1);
                }
                break;
            case '-':
                if (menuCurrentLink != 0) {
                    cursorBufferMoveBy(menuCurrentLink, -1);
                }
                break;
            default:
                if (menuCurrentLink != 0) {
                    cursorBufferSetOffset(menuCurrentLink, arg - '0');
                }
                break;
            }
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            break;
        case 'f':
            arg = text[2];
            switch (arg) {
            case '0':
                seVoiceStartDefault(MENU_SE_CURSOR);
                menuLinkSetCurrent(skillBoardCursorTable);
                licenseSlotState[0] = 4;
                break;
            case '1':
                if (skillBoardCursorTable[0].cursor == 3) {
                    cursorBufferSetCapacity(&skillBoardCursorTable[1], 4);
                } else {
                    cursorBufferSetCapacity(&skillBoardCursorTable[1], 8);
                }
                menuLinkSetCurrent(licenseSecondCursor);
                licenseSlotState[0] = 5;
                seVoiceStartDefault(MENU_SE_CURSOR);
                break;
            case '2':
                seVoiceStartDefault(MENU_SE_CURSOR);
                menuLinkSetCurrent(licenseThirdCursor);
                licenseSlotState[0] = 6;
                break;
            }
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            break;
        case 'l':
            arg = text[2];
            switch (arg) {
            case '0':
                menuTutorialToggleFlags ^= 1;
                break;
            case '1':
                menuTutorialToggleFlags ^= 2;
                break;
            case '2':
                menuTutorialToggleFlags ^= 4;
                break;
            }
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            break;
        case 'a':
            trackX = 0;
            trackY = 0;
            switch (text[2] - '0') {
            case 0:
                trackX = 99999;
                trackY = 99999;
                break;
            case 1:
                trackX = 11;
                trackY = 341;
                break;
            case 2:
                trackX = 11;
                trackY = 360;
                break;
            case 3:
                trackX = 135;
                trackY = 341;
                break;
            case 4:
                trackX = 135;
                trackY = 360;
                break;
            case 5:
                trackX = 160;
                trackY = 296;
                break;
            case 6:
                trackX = 280;
                trackY = 296;
                break;
            }
            if (trackX == 99999 && trackY == 99999) {
                menuTutorialStage = 0;
            } else if (menuTutorialStage != 0) {
                fontTrackSetTarget(menuBandTrack, trackX, trackY);
            } else {
                fontTrackSetBoth(menuBandTrack, trackX, trackY);
                menuTutorialStage = 1;
            }
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            break;
        case 'j':
            count = text[2] * 10 + text[3] - 0x210;
            if (count < 1) {
                count = 1;
            }
            index = menuTutorialIndex + count;
            menuTutorialIndex = index;
            break;
        case 'b':
            if (menuTutorialRestartCount >= 2) {
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
            }
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            break;
        case 's':
            if (msgRangeIsRevealed(menuMessageRangeState)) {
                if (menuTutorialStage < 2) {
                    fontTrackSetBoth(menuBandTrack, 0x1C, 0x1B0);
                    menuTutorialStage = 2;
                }
                if (PadData[0].directionRepeated & (PAD_UP | PAD_DOWN)) {
                    seVoiceStartDefault(MENU_SE_CURSOR);
                    menuTutorialStage = ((menuTutorialStage - 2) ^ 1) + 2;
                    fontTrackSetTarget(menuBandTrack, 0x1C, menuTutorialStage * 20 + 0x188);
                }
                if (padCirclePressedAlone()) {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    if (menuTutorialStage == 2) {
                        index = menuTutorialIndex + 3;
                        menuTutorialIndex = index;
                    } else {
                        index = menuTutorialIndex + 4;
                        menuTutorialIndex = index;
                    }
                    menuTutorialStage = 0;
                }
            }
            break;
        case 'g':
            if (text[2] == '0') {
                switch (licenseSlotState[0]) {
                case 5:
                    ctx = ptGetMenuMember();
                    selectionSlotOpen(ctx, skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor);
                    break;
                case 6:
                    ctx = ptGetMenuMember();
                    links = skillBoardCursorTable;
                    if (selectionTakeSub(ctx, links[0].cursor, links[1].cursor, links[2].cursor)) {
                        ctx = ptGetMenuMember();
                        ctx->limits->classPoints += selectionSlotRewardTable[links[0].cursor * 8 + links[1].cursor];
                    }
                    break;
                }
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
            } else {
                menuTutorialTimer += 1;
                if (menuTutorialTimer == 1) {
                    switch (licenseSlotState[0]) {
                    case 5:
                        menuPromptSet(1);
                        break;
                    case 6:
                        menuPromptSet(2);
                        break;
                    }
                    fontTrackSetBoth(menuBandTrack, 280, 296);
                    menuTutorialStage = 1;
                } else if (menuTutorialTimer == 21) {
                    fontTrackSetTarget(menuBandTrack, 160, 296);
                } else if (menuTutorialTimer == 41) {
                    menuPromptSet(0);
                    menuTutorialStage = 0;
                    switch (licenseSlotState[0]) {
                    case 5:
                        ctx = ptGetMenuMember();
                        selectionSlotOpen(ctx, skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor);
                        break;
                    case 6:
                        ctx = ptGetMenuMember();
                        links = skillBoardCursorTable;
                        if (selectionTakeSub(ctx, links[0].cursor, links[1].cursor, links[2].cursor)) {
                            menuPromptSet(3);
                            ctx = ptGetMenuMember();
                            ctx->limits->classPoints += selectionSlotRewardTable[links[0].cursor * 8 + links[1].cursor];
                        }
                        break;
                    }
                    if (menuLayoutMode == 0) {
                        menuTutorialTimer = 0;
                        index = menuTutorialIndex + 1;
                        menuTutorialIndex = index;
                    }
                } else if (menuTutorialTimer == 101) {
                    menuPromptSet(4);
                } else if (menuTutorialTimer == 161) {
                    menuPromptSet(0);
                    menuTutorialTimer = 0;
                    index = menuTutorialIndex + 1;
                    menuTutorialIndex = index;
                }
            }
            break;
        case 'm':
            menuPromptSet(text[2] - '0');
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            break;
        default:
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            break;
        }
        break;
    case 0:
    case '#':
        index = menuTutorialIndex + 1;
        menuTutorialIndex = index;
        break;
    case '*':
        menuTutorialPageReset();
        index = menuTutorialIndex + 1;
        menuTutorialIndex = index;
        menuTutorialHintFlags |= 1;
        break;
    default:
        menuTutorialLine = text;
        index = menuTutorialIndex + 1;
        menuTutorialIndex = index;
        break;
    }
    if (menuTutorialActive != 0) {
        if ((u8) ((u8) menuSlots[3].state - 4) < 3) {
            menuLicenseState = (u8) menuSlots[3].state;
        }
    }
}


int menuMainCommandStep(ControlSlot *slot) {
    int activeState;
    int result;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    if (menuKeyCheckPending) {
        if (xglCdServiceIfPending() == 0) {
            if (mapChoiceCount[0] != 0) {
                menuSceneSetState(10);
            }
            menuKeyCheckPending = 0;
        }
        return 0;
    }
    if ((PadData[0].directionRepeated & (PAD_LEFT | PAD_L2)) != 0) {
        cursorBufferMoveBy(slot->link + slot->state, -1);
    }
    if ((PadData[0].directionRepeated & (PAD_RIGHT | PAD_R2)) != 0) {
        cursorBufferMoveBy(slot->link + slot->state, 1);
    }
    if ((PadData[0].directionPressed & PAD_START) != 0) {
        menuAdvanceSlotCursor();
    }
    result = padCrossPressedAlone() ? 1 : 0;
    activeState = 1;
    if (slot->state == 0) {
        if (padCirclePressedAlone()) {
            menuSceneSetState(slot->link->cursor + 1);
        } else if (PadData[0].directionPressed & PAD_DOWN) {
            ListCursor *links = slot->link;

            cursorBufferSetOffset(links + 1, ((u32) (links[0].cursor * links[1].capacity)) / 6);
            slot->state = activeState;
            seVoiceStartDefault(MENU_SE_CURSOR);
        }
    } else if (menuChoiceOpenFlag) {
        if (!choiceGetResult()) {
            result = activeState;
            menuChoiceAction = 2;
        } else {
            menuChoiceOpenFlag = 0;
        }
    } else if (padCirclePressedAlone() != 0 && xglCdServiceIfPending() == 0) {
        switch (mainCommandKindTable[mainCommandCursors[1].cursor]) {
        case 0:
            if (mapChoiceCount[0]) {
                if (ptState.progress->mapId != -1) {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    choiceInit(menuMainTxdEntry(0x1BE), 1);
                    choiceSetRow(0xF0);
                    menuChoiceOpenFlag = activeState;
                } else {
                    menuSceneSetState(10);
                    seVoiceStartDefault(0x01000006);
                }
            } else {
                seVoiceStartDefault(MENU_SE_REFUSE);
            }
            break;
        case 1:
            result = 2;
            seVoiceStartDefault(0x01000006);
            break;
        case 2:
            result = 3;
            seVoiceStartDefault(0x01000006);
            break;
        case 3:
            menuSceneSetState(9);
            seVoiceStartDefault(0x01000006);
            break;
        }
    } else if ((PadData[0].directionPressed & PAD_UP) != 0) {
        ListCursor *links = slot->link;

        if (links[1].capacity) {
            u32 scaled = (u32) (6 * links[1].cursor);
            u32 offset = scaled / links[1].capacity;
            u32 last = (6 + scaled) / links[1].capacity - 1;
            if (links[0].cursor < (int) offset || links[0].cursor > (int) last) {
                cursorBufferSetOffset(links, offset);
            }
        }
        slot->state = 0;
        seVoiceStartDefault(MENU_SE_CURSOR);
    }
    return result;
}


int menuItemListStep(ControlSlot *slot) {
    PartyMember *entry;
    MenuBlock *sel;
    int i;

    if (!(slot->flags & 1)) {
        return 0;
    }
    switch (slot->state) {
    case 0:
        if (!menuCursorMemoryAlt) {
            goto resetState1; /* goto kept: one shared reset block; a second copy of it grows the function by two instructions */
        }
    case 7:
        slot->state = 4;
        break;
    case 1:
        if (PadData[0].directionRepeated & PAD_LEFT) {
            cursorBufferMoveBy(menuTabCursor, -1);
        }
        if (PadData[0].directionRepeated & PAD_RIGHT) {
            cursorBufferMoveBy(menuTabCursor, 1);
        }
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_BACK);
            menuFocusRootSlot();
            break;
        }
        if (!padCirclePressedAlone()) {
            break;
        }
        switch (menuTabCursor[0].cursor) {
        case 0:
            seVoiceStartDefault(MENU_SE_CONFIRM);
            slot->state = 4;
            menuLinkSetCurrent(slot->link);
            return 0;
        case 1:
            menuSceneSetState(8);
            break;
        default:
            break;
        }
        return 0;
    case 4:
        if (countdownActive) {
            countdownActive = 0;
            return 0;
        }
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(MENU_SE_CANCEL);
        resetState1:
            menuLinkSetCurrent(0);
            slot->state = 1;
            return 0;
        }
        if (padCirclePressedAlone() != 0 && selectionWindowCount != 0) {
            if (!selectionEntryIsEligible(slot->link->cursor)) {
                goto soundFail; /* goto kept: the three refusals share one block; inlined copies are not cross-jumped */
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            if (selectionUsableItemList[slot->link->cursor] >= 0x25) {
                signed char map[4] = {0x08, 0x07, 0x10, 0x0C};

                countdownInit(menuMainTxdEntry(selectionUsableItemList[slot->link->cursor] + 0x193), 0x12C);
                countdownActive = 1;
                entry = menuItemMemberEntry(&itemMenuBlock.items[itemMenuBlock.cursor]);
                calcUseItemMenu(entry->id, selectionUsableItemList[slot->link->cursor]);
                slotPairSet(map[selectionUsableItemList[slot->link->cursor] - 0x25]);
                if (!menuCopilotModeFlag) {
                    menuPartnerMask = 1;
                    for (i = 0; i < 16; i++) {
                        if (slotPairTest(i + 1) != 0) {
                            menuPartnerMask |= 1 << i;
                        }
                    }
                }
                if (selectionWindowListDropAt(slot->link->cursor) >= 1) {
                    return 0;
                }
                cursorBufferSetCapacity(slot->link, selectionWindowCount);
                return 0;
            }
            if (selectionWindowEntryMarkedFlags(slot->link->cursor) != 0) {
                menuSetValuesMode(&itemMenuBlock, 1);
            }
            if (selectionWindowEntryTargetsParty(slot->link->cursor) != 0) {
                itemMenuBlock.flags |= 4;
            }
            slot->state = 2;
            itemMenuBlock.flags |= 1;
            menuLinkSetCurrent(0);
            return 0;
        }
        if ((PadData[0].directionState & (((u64) PAD_LEFT << 32) | PAD_UP | PAD_DOWN)) == ((u64) PAD_LEFT << 32)) {
            cursorBufferMoveUnits(slot->link, -1);
        }
        if ((PadData[0].directionState & (((u64) PAD_RIGHT << 32) | PAD_UP | PAD_DOWN)) == ((u64) PAD_RIGHT << 32)) {
            cursorBufferMoveUnits(slot->link, 1);
        }
        return 0;
    case 2:
        if (padCrossPressedAlone() != 0) {
            MenuBlock *menu = &itemMenuBlock;

            menuSetValuesMode(menu, 0);
            seVoiceStartDefault(MENU_SE_CANCEL);
            menu->flags &= ~1;
            slot->state = 4;
            menuLinkSetCurrent(slot->link);
            menu->flags &= ~4;
            break;
        }
        if (padCirclePressedAlone()) {
            if (selectionWindowEntryTargetsParty(slot->link->cursor)) {
                if (!selectionCountedMemberNeedsHpEp()) {
                    goto soundFail;
                }
                seVoiceStartDefault(MENU_SE_CONFIRM);
                entry = menuItemMemberEntry(&itemMenuBlock.items[itemMenuBlock.cursor]);
                calcUseItemMenu(entry->id, selectionUsableItemList[slot->link->cursor]);
                if (selectionWindowListDropAt(slot->link->cursor) > 0) {
                    return 0;
                }
                cursorBufferSetCapacity(slot->link, selectionWindowCount);
                sel = &itemMenuBlock;
                sel->flags &= ~1;
                slot->state = 4;
                menuLinkSetCurrent(slot->link);
                sel->flags &= ~4;
                break;
            }
            entry = menuItemMemberEntry(&itemMenuBlock.items[itemMenuBlock.cursor]);
            if (!selectionEntryIsUseful(slot->link->cursor, entry)) {
                goto soundFail;
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            entry = menuItemMemberEntry(&itemMenuBlock.items[itemMenuBlock.cursor]);
            calcUseItemMenu(entry->id, selectionUsableItemList[slot->link->cursor]);
            if (selectionWindowListDropAt(slot->link->cursor) > 0) {
                return 0;
            }
            cursorBufferSetCapacity(slot->link, selectionWindowCount);
            itemMenuBlock.flags &= ~1;
            slot->state = 4;
            menuLinkSetCurrent(slot->link);
            break;
        soundFail:
            seVoiceStartDefault(MENU_SE_REFUSE);
            break;
        }
        if (PadData[0].directionRepeated & PAD_DOWN) {
            menuMoveCursor(&itemMenuBlock, 1);
        } else if (PadData[0].directionRepeated & PAD_UP) {
            menuMoveCursor(&itemMenuBlock, -1);
        }
        return 0;
    case 3:
    case 5:
    case 6:
    default:
        break;
    }
    return 0;
}

/* Clears the mark bit of the current slot, then sets menuPendingSelection to -1 before the tail call. */
void menuCancelToMainSlot(void);

int menuEtherPlaceStep(ControlSlot *slot) {
    unsigned char state;
    PartyMember *ctx;
    PartyMember *ctx2;
    PartyMember *ctx3;
    int t16;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    state = slot->state;
    switch (state) {
    case 0:
        slot->state = 4;
        break;
    case 4:
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(MENU_SE_BACK);
            if (menuCursorMemoryAlt) {
                menuFocusRootSlot();
            } else {
                menuCancelToMainSlot();
            }
        } else if (padCirclePressedAlone()) {
            if (learnedEtherList[slot->link->cursor] != 0) {
                PartyMember *ctxS4;
                int limit;
                int index = slot->link->cursor;
                limit = (int) ptGetMenuMemberState()->ep;
                ctxS4 = ptGetMenuMember();
                if (calcTakeEpGetMenu(ctxS4->id, learnedEtherList[slot->link->cursor]) > limit) {
                    seVoiceStartDefault(MENU_SE_REFUSE);
                } else {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    menuSceneFlags &= ~1;
                    slot->state = 2;
                    itemMenuBlock.flags = itemMenuBlock.flags | 1;
                    menuLinkSetCurrent(0);
                    if (learnedEtherList[slot->link->cursor] == 3) {
                        itemMenuBlock.flags = itemMenuBlock.flags | 4;
                    }
                }
            }
        } else if (menuCursorMemoryAlt != 0 && (PadData[0].directionRepeated & (PAD_LEFT | PAD_RIGHT))) {
            if (PadData[0].directionRepeated & PAD_LEFT) {
                menuMoveCursor(&mainMenuBlock, -1);
            } else {
                menuMoveCursor(&mainMenuBlock, 1);
            }
            menuRebuildCursorBuffers();
        }
        break;
    case 2:
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuSceneFlags = menuSceneFlags | 1;
            itemMenuBlock.flags = itemMenuBlock.flags & ~1;
            slot->state = 4;
            menuLinkSetCurrent(slot->link);
            itemMenuBlock.flags = itemMenuBlock.flags & ~4;
        } else if (padCirclePressedAlone()) {
            MenuBlock *menu;
            int id20;
            StatBlock *rec16;
            StatBlock *rec2;
            ptState.menuSlot = itemMenuBlock.cursor;
            ctx = ptGetMenuMember();
            id20 = ctx->id;
            ptState.menuSlot = mainMenuBlockCursor[0];
            rec16 = calcTotalParaMenu(ctx->id);
            rec2 = calcTotalParaMenu(ctx->id);
            menu = &itemMenuBlock;
            if (rec16->hp != rec2->maxHp
                || (learnedEtherList[slot->link->cursor] == 3 && selectionActiveMemberNeedsHp() != 0)) {
                seVoiceStartDefault(MENU_SE_CONFIRM);
                ctx = ptGetMenuMember();
                calcUseEtherMenu(ctx->id, id20, learnedEtherList[slot->link->cursor]);
                t16 = (short) ptGetMenuMemberState()->ep;
                ctx = ptGetMenuMember();
                if (!(t16 >= calcTakeEpGetMenu(ctx->id, learnedEtherList[slot->link->cursor]))) {
                    menuSceneFlags = menuSceneFlags | 1;
                    menu->flags = menu->flags & ~1;
                    slot->state = 4;
                    menuLinkSetCurrent(slot->link);
                    menu->flags = menu->flags & ~4;
                }
            } else {
                seVoiceStartDefault(MENU_SE_REFUSE);
            }
        } else {
            unsigned short pad = PadData[0].directionRepeated;
            if ((pad & PAD_DOWN) != 0) {
                menuMoveCursor(&itemMenuBlock, 1);
            } else if ((pad & PAD_UP) != 0) {
                menuMoveCursor(&itemMenuBlock, -1);
            }
        }
        break;
    }
    return 0;
}


/* The menu-slot state machine: idle, cursor, confirm and selection states, one step per call. */
int menuSkillChargeStep(ControlSlot *slot) {
    unsigned char state;
    PartyMember *ctx;
    PartyMemberExpPoints *limits;
    unsigned char flagsBefore;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    if (slot->state == 0) {
        if (menuCursorMemoryAlt != 0) {
            slot->state = 4;
        } else {
            menuLinkSetCurrent(0);
            slot->state = 1;
        }
        menuLicenseState = 4;
        return 0;
    }
    if (slot->state == 7) {
        slot->state = 1;
        return 0;
    }
    if (menuChoiceOpenFlag == 0 && padCrossPressedAlone() != 0) {
        if (slot->state == 1) {
            seVoiceStartDefault(MENU_SE_BACK);
            if (menuCursorMemoryAlt != 0) {
                menuFocusRootSlot();
            } else {
                menuCancelToMainSlot();
            }
        } else {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuLicenseState = slot->state;
            slot->state = 1;
            menuLinkSetCurrent(0);
        }
        return 0;
    }
    switch (slot->state) {
    case 1:
        if (PadData[0].directionRepeated & PAD_LEFT) {
            cursorBufferMoveBy(menuTabCursor, -1);
        }
        if (PadData[0].directionRepeated & PAD_RIGHT) {
            cursorBufferMoveBy(menuTabCursor, 1);
        }
        if (padCirclePressedAlone() != 0) {
            switch (menuTabCursor[0].cursor) {
            case 0:
                seVoiceStartDefault(MENU_SE_CONFIRM);
                slot->state = menuLicenseState;
                menuLinkSetCurrent(slot->link + (menuLicenseState - 4));
                break;
            case 1:
                slot->state = menuLicenseState;
                menuLinkSetCurrent(slot->link + (menuLicenseState - 4));
                menuTutorialSaveState();
                break;
            }
        }
        break;
    case 4:
        if ((PadData[0].directionState & 0x200000005000ULL) == 0x200000000000ULL) {
            if (skillBoardCursorTable[0].cursor == 3) {
                cursorBufferSetCapacity(&skillBoardCursorTable[1], 4);
            } else {
                cursorBufferSetCapacity(&skillBoardCursorTable[1], 8);
            }
            menuLinkSetCurrent(licenseSecondCursor);
            slot->state = 5;
            seVoiceStartDefault(MENU_SE_CURSOR);
        }
        break;
    case 5:
        if (menuChoiceOpenFlag != 0) {
            if (choiceGetResult() == 0) {
                selectionSlotOpen(ptGetMenuMember(), skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor);
            }
            menuChoiceOpenFlag = 0;
        } else if ((PadData[0].directionState & 0x200000005000ULL) == 0x200000000000ULL) {
            seVoiceStartDefault(MENU_SE_CURSOR);
            menuLinkSetCurrent(licenseThirdCursor);
            slot->state = 6;
        } else if ((PadData[0].directionState & 0x800000005000ULL) == 0x800000000000ULL) {
            seVoiceStartDefault(MENU_SE_CURSOR);
            menuLinkSetCurrent(skillBoardCursorTable);
            slot->state = 4;
        } else if (padCirclePressedAlone() != 0) {
            int found = selectionCanOpenSlot(ptGetMenuMember(), skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor);
            if (found != 0) {
                seVoiceStartDefault(MENU_SE_CONFIRM);
                choiceInit(menuMainTxdEntry(skillBoardCursorTable[0].cursor + 0x36), 1);
                msgBufferAppend(" ");
                msgBufferAppend(menuMainTxdEntry(skillBoardCursorTable[1].cursor + 0x3A));
                msgBufferAppend(menuNewlineText);
                msgBufferAppend(menuMainTxdEntry(0x42));
                msgBufferAppend(menuFormatNumber5(selectionSlotOpenCostTable[skillBoardCursorTable[0].cursor * 8 + skillBoardCursorTable[1].cursor]));
                msgBufferAppend(menuNewlineText);
                msgBufferAppend(menuMainTxdEntry(0x44));
                menuChoiceOpenFlag = 1;
            } else {
                seVoiceStartDefault(MENU_SE_REFUSE);
            }
        }
        break;
    case 6:
        if (menuChoiceOpenFlag != 0) {
            if (choiceGetResult() == 0) {
                flagsBefore = ptGetMenuMember()->buffer->openGroup;
                ctx = ptGetMenuMember();
                if (selectionApplyEntry(ctx, skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor, skillBoardCursorTable[2].cursor) != 0) {
                    countdownInit(menuMainTxdEntry(skillBoardCursorTable[0].cursor + 0x36), 300);
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(skillBoardCursorTable[1].cursor + 0x3A));
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(0x47));
                    msgBufferAppend(menuNewlineText);
                    msgBufferAppend(menuMainTxdEntry(0x4B));
                    msgBufferAppend(menuFormatNumber5(selectionSlotRewardTable[skillBoardCursorTable[0].cursor * 8 + skillBoardCursorTable[1].cursor]));
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(0x4C));
                    if (flagsBefore != ptGetMenuMember()->buffer->openGroup) {
                        countdownActive = 3;
                    } else {
                        countdownActive = 2;
                    }
                    limits = ptGetMenuMember()->limits;
                    limits->classPoints += selectionSlotRewardTable[skillBoardCursorTable[0].cursor * 8 + skillBoardCursorTable[1].cursor];
                    if (ptGetMenuMember()->limits->classPoints > 99999) {
                        ptGetMenuMember()->limits->classPoints = 99999;
                    }
                }
            }
            menuChoiceOpenFlag = 0;
        } else if ((PadData[0].directionState & 0x800000005000ULL) == 0x800000000000ULL) {
            seVoiceStartDefault(MENU_SE_CURSOR);
            menuLinkSetCurrent(licenseSecondCursor);
            slot->state = 5;
        } else if (padCirclePressedAlone() != 0) {
            if (selectionEntryBitOutsideMask(skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor, skillBoardCursorTable[2].cursor, menuItemCategoryMask) == 0
                && selectionCanTakeSub(ptGetMenuMember(), skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor, skillBoardCursorTable[2].cursor) != 0) {
                int value;

                seVoiceStartDefault(MENU_SE_CONFIRM);
                choiceInit(selectionSubTextGet(skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor, skillBoardCursorTable[2].cursor, 0), 1);
                msgBufferAppend(menuNewlineText);
                msgBufferAppend(menuMainTxdEntry(0x43));
                value = selectionSubCostGet(skillBoardCursorTable[0].cursor, skillBoardCursorTable[1].cursor, skillBoardCursorTable[2].cursor);
                msgBufferAppend(menuFormatNumber5(value));
                msgBufferAppend(menuNewlineText);
                msgBufferAppend(menuMainTxdEntry(0x44));
                menuChoiceOpenFlag = 1;
            } else {
                seVoiceStartDefault(MENU_SE_REFUSE);
            }
        }
        break;
    }
    if (slot->state >= 4 && slot->state <= 6) {
        menuLicenseState = slot->state;
    }
    return 0;
}

/* Per-slot step of a ControlSlot: once its flag bit 0 is set, the state moves 0 -> 4 -> 5 and the cursor offset
   is set for a left or right press; always returns 0. */
int menuTechPageStep(ControlSlot *slot) {
    PadRecord *pad;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    if ((u8) slot->state == 0) {
        slot->state = 4;
        return 0;
    }
    if (padCrossPressedAlone() != 0) {
        seVoiceStartDefault(MENU_SE_BACK);
        if (menuCursorMemoryAlt != 0) {
            menuFocusRootSlot();
        } else {
            menuCancelToMainSlot();
        }
        return 0;
    }
    pad = PadData;
    if ((pad->directionRepeated & (PAD_LEFT | PAD_RIGHT)) != 0 && (pad->directionHeld & (PAD_UP | PAD_DOWN)) == 0) {
        if ((u8) slot->state == 4) {
            slot->state = 5;
            cursorBufferSetOffset(menuTabCursor, 1);
            menuRebuildCursorBuffers();
        } else {
            slot->state = 4;
            cursorBufferSetOffset(menuTabCursor, 0);
            menuRebuildCursorBuffers();
        }
        seVoiceStartDefault(MENU_SE_CURSOR);
    }
    return 0;
}

void menuTakePlainEntry(PartyMember *context, int index, int n);
void menuPlaceKey(PartyMember *context, int value, int count);

int menuEquipAssignStep(ControlSlot *slot) {
    PadRecord *pad;
    ListCursor *cursor;
    ListCursor *selection;
    int i;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    switch ((u8) slot->state) {
    case 0:
        if (menuCursorMemoryAlt == 0 && menuCopilotModeFlag == 0) {
            menuLinkSetCurrent(0);
            slot->state = 1;
        } else {
            slot->state = 4;
        }
        break;
    case 1:
        pad = PadData;
        if (pad->directionRepeated & PAD_LEFT) {
            cursorBufferMoveBy(menuTabCursor, -1);
            menuRebuildCursorBuffers();
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            cursorBufferMoveBy(menuTabCursor, 1);
            menuRebuildCursorBuffers();
        }
        if (padCrossPressedAlone() == 0) {
            if (padCirclePressedAlone() == 0) {
                break;
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            menuLinkSetCurrent(equipItemCursor);
            slot->state = 4;
            break;
        }
        seVoiceStartDefault(MENU_SE_BACK);
        if (menuCursorMemoryAlt != 0) {
            menuFocusRootSlot();
        } else {
            menuCancelToMainSlot();
        }
        break;
    case 4:
        if (padCrossPressedAlone() != 0) {
            if (menuCopilotModeFlag != 0) {
                seVoiceStartDefault(MENU_SE_BACK);
                if (menuCursorMemoryAlt != 0) {
                    menuFocusRootSlot();
                } else {
                    menuCancelToMainSlot();
                }
                break;
            }
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuLinkSetCurrent(0);
            slot->state = 1;
            break;
        }
        if (padCirclePressedAlone() != 0) {
                                    if (equipItemCursor[0].capacity == 0) {
                break;
            }
            if (menuTabCursor[0].cursor != 0) {
                break;
            }
            if (ptGetMenuMember()->buffer->entry_count < menuEquipRowCount) {
                if (menuCopilotModeFlag != 0) {
                    if (selectionFindIndex(ptGetMenuMember(), selectionAccessoryList[equipItemCursor[0].cursor]) == -1) {
                        menuTakePlainEntry(ptGetMenuMember(), equipItemCursor[0].cursor,
                                                ptGetMenuMember()->buffer->entry_count);
                    ptGetMenuMember()->buffer->entry_count++;
                    equipSlotCursor[0].capacity++;
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    break;
                    } else {
                        seVoiceStartDefault(MENU_SE_REFUSE);
                        break;
                    }
                } else {
                    if (selectionFindIndex(ptGetMenuMember(), equipItemList[equipItemCursor[0].cursor]) == -1) {
                        menuPlaceKey(ptGetMenuMember(),
                                          equipItemList[equipItemCursor[0].cursor], ptGetMenuMember()->buffer->entry_count);
                    ptGetMenuMember()->buffer->entry_count++;
                    equipSlotCursor[0].capacity++;
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    break;
                    } else {
                        seVoiceStartDefault(MENU_SE_REFUSE);
                        break;
                    }
                }
            }
            if (menuCopilotModeFlag != 0 && selectionFindIndex(ptGetMenuMember(),
                                                      selectionAccessoryList[equipItemCursor[0].cursor]) != -1) {
                seVoiceStartDefault(MENU_SE_REFUSE);
                break;
            }
            menuSceneFlags &= ~1;
            slot->state = 5;
            menuLinkSetCurrent(equipSlotCursor);
            seVoiceStartDefault(MENU_SE_CONFIRM);
            break;
        }
        {
            PadRecord *held = PadData;
            if ((*(unsigned long long *) &held->directionHeld & 0x80F00FLL) == 0x800000LL) {
                if (menuTabCursor[0].cursor == 0) {
                    if (ptGetMenuMember()->buffer->entry_count != 0) {
                        menuSceneFlags &= ~1;
                        slot->state = 6;
                        menuLinkSetCurrent(equipSlotCursor);
                        seVoiceStartDefault(MENU_SE_CONFIRM);
                        break;
                    }
                    seVoiceStartDefault(MENU_SE_REFUSE);
                    break;
                }
            }
        }
        {
            PadRecord *pad4 = PadData;
            if ((*(unsigned long long *) &pad4->directionHeld & 0x800000005000ULL) == 0x800000000000ULL) {
                cursorBufferMoveUnits(equipItemCursor, -1);
            }
            if ((*(unsigned long long *) &pad4->directionHeld & 0x200000005000ULL) == 0x200000000000ULL) {
                cursorBufferMoveUnits(equipItemCursor, 1);
            }
        }
        break;
    case 5:
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuSceneFlags |= 1;
            slot->state = 4;
            menuLinkSetCurrent(equipItemCursor);
            break;
        }
        if (padCirclePressedAlone() == 0) {
            break;
        }
                        if (menuCopilotModeFlag != 0) {
            int item;

            item = ptGetMenuMember()->buffer->entries[equipSlotCursor[0].cursor];
            menuTakePlainEntry(ptGetMenuMember(), equipItemCursor[0].cursor, equipSlotCursor[0].cursor);
            dataBoxIncCategory2(item);
            selectionRebuildList();
            cursorBufferSetCapacity(equipItemCursor, selectionPlainCount);
        } else {
            menuPlaceKey(ptGetMenuMember(), equipItemList[equipItemCursor[0].cursor],
                              equipSlotCursor[0].cursor);
        }
        menuSceneFlags |= 1;
        slot->state = 4;
        menuLinkSetCurrent(equipItemCursor);
        seVoiceStartDefault(MENU_SE_CONFIRM);
        break;
    case 6:
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuSceneFlags |= 1;
            slot->state = 4;
            menuLinkSetCurrent(equipItemCursor);
            break;
        }
        if ((PadData[0].directionState & 0x80F00FLL) == 0x800000LL) {
            if (menuCopilotModeFlag != 0) {
                int i;
                for (i = 0; i < ptGetMenuMember()->buffer->entry_count; i++) {
                    dataBoxIncCategory2(ptGetMenuMember()->buffer->entries[i]);
                }
                selectionRebuildList();
                cursorBufferSetCapacity(equipItemCursor, selectionPlainCount);
                calcAccEquipOrg(ptGetMenuMember()->id, 0, 0);
                calcAccEquipOrg(ptGetMenuMember()->id, 1, 0);
                calcAccEquipOrg(ptGetMenuMember()->id, 2, 0);
            } else {
                calcSkillEquipOrg(ptGetMenuMember()->id, 0, 0);
                calcSkillEquipOrg(ptGetMenuMember()->id, 1, 0);
                calcSkillEquipOrg(ptGetMenuMember()->id, 2, 0);
                calcSkillEquipOrg(ptGetMenuMember()->id, 3, 0);
            }
                        ptGetMenuMember()->buffer->entry_count = 0;
            cursorBufferSetCapacity(equipSlotCursor, 0);
            seVoiceStartDefault(MENU_SE_CONFIRM);
            menuSceneFlags |= 1;
            slot->state = 4;
            menuLinkSetCurrent(equipItemCursor);
            break;
        }
        if (padCirclePressedAlone() == 0) {
            break;
        }
        seVoiceStartDefault(MENU_SE_CONFIRM);
        if (ptGetMenuMember()->buffer->entry_count == 0) {
            break;
        }
                        if (menuCopilotModeFlag != 0) {
            dataBoxIncCategory2(ptGetMenuMember()->buffer->entries[equipSlotCursor[0].cursor]);
            selectionRebuildList();
            cursorBufferSetCapacity(equipItemCursor, selectionPlainCount);
            calcAccEquipOrg(ptGetMenuMember()->id, equipSlotCursor[0].cursor, 0);
            ptGetMenuMember()->buffer->entries[equipSlotCursor[0].cursor] = 0;
            if (equipSlotCursor[0].cursor < ptGetMenuMember()->buffer->entry_count - 1) {
                for (i = equipSlotCursor[0].cursor + 1; i < ptGetMenuMember()->buffer->entry_count; i++) {
                    calcAccEquipOrg(ptGetMenuMember()->id, i - 1, ptGetMenuMember()->buffer->entries[i]);
                    ptGetMenuMember()->buffer->entries[i - 1] = ptGetMenuMember()->buffer->entries[i];
                }
                calcAccEquipOrg(ptGetMenuMember()->id, i - 1, 0);
                ptGetMenuMember()->buffer->entries[i - 1] = 0;
            }
        } else {
            calcSkillEquipOrg(ptGetMenuMember()->id, equipSlotCursor[0].cursor, 0);
            ptGetMenuMember()->buffer->entries[equipSlotCursor[0].cursor] = 0;
            if (equipSlotCursor[0].cursor < ptGetMenuMember()->buffer->entry_count - 1) {
                for (i = equipSlotCursor[0].cursor + 1; i < ptGetMenuMember()->buffer->entry_count; i++) {
                    calcSkillEquipOrg(ptGetMenuMember()->id, i - 1,
                                      selectionRecordTable[ptGetMenuMember()->buffer->entries[i]].dispatch_value);
                    ptGetMenuMember()->buffer->entries[i - 1] = ptGetMenuMember()->buffer->entries[i];
                }
                calcSkillEquipOrg(ptGetMenuMember()->id, i - 1, 0);
                ptGetMenuMember()->buffer->entries[i - 1] = 0;
            }
        }
        ptGetMenuMember()->buffer->entry_count--;
        cursorBufferSetCapacity(equipSlotCursor, ptGetMenuMember()->buffer->entry_count);
        if (ptGetMenuMember()->buffer->entry_count != 0) {
            break;
        }
        menuSceneFlags |= 1;
        slot->state = 4;
        menuLinkSetCurrent(equipItemCursor);
        break;
    default:
        break;
    }
    return 0;
}


/* Advances the back or front slot cursor of ptState until the chosen slot is filled, and starts voice 0x01000001 when the cursor moved. */
void menuAdvanceSlotCursor(void);
extern int partyFormationSourceIndex;

int menuPartyFormationStep(ControlSlot *slot) {
    if ((slot->flags & CONTROL_SLOT_FLAG_MARK) == 0) {
        return 0;
    }
    switch ((u8) slot->state) {
    case CONTROL_STATE_IDLE:
        if (menuCursorMemoryAlt != 0) {
            mainMenuBlock.flags |= 1;
            slot->state = CONTROL_STATE_RUNNING;
            menuSceneFlags |= 1;
        } else {
            slot->state = CONTROL_STATE_BROWSE;
        }
        break;
    case CONTROL_STATE_BROWSE:
    {
        PadRecord *pad = PadData;
        if (pad->directionRepeated & PAD_LEFT) {
            cursorBufferMoveBy(menuTabCursor, -1);
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            cursorBufferMoveBy(menuTabCursor, 1);
        }
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuFocusRootSlot();
            break;
        }
        if (padCirclePressedAlone() != 0) {
        switch (menuTabCursor[0].cursor) {
        case 0:
            seVoiceStartDefault(MENU_SE_CONFIRM);
            mainMenuBlock.flags |= 1;
            menuSceneFlags |= 1;
            slot->state = CONTROL_STATE_RUNNING;
            break;
        case 1:
            slot->state = CONTROL_STATE_CHOICE;
            mainMenuBlock.flags |= 1;
            menuSceneFlags |= 1;
            if (menuCopilotModeFlag != 0) {
                menuSetCursor(&mainMenuBlock, ptState.backIndex);
            } else {
                menuSetCursor(&mainMenuBlock, ptState.frontIndex);
            }
            menuSetMode(&mainMenuBlock, mainMenuBlock.layout);
            menuEnableItems(&mainMenuBlock, 0x500);
            seVoiceStartDefault(MENU_SE_CONFIRM);
            break;
        default:
            break;
        }
            break;
        }
        if (pad->directionPressed & PAD_START) {
            menuAdvanceSlotCursor();
        }
        break;
    }
    case CONTROL_STATE_RUNNING:
    {
        PadRecord *pad = PadData;
        if (pad->directionRepeated & PAD_UP) {
            menuNavigate(&mainMenuBlock, 0);
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            menuNavigate(&mainMenuBlock, 1);
        }
        if (pad->directionRepeated & PAD_DOWN) {
            menuNavigate(&mainMenuBlock, 2);
        }
        if (pad->directionRepeated & PAD_LEFT) {
            menuNavigate(&mainMenuBlock, 3);
        }
        ptState.menuSlot = mainMenuBlock.cursor;
        if (padCrossPressedAlone() != 0) {
            if (menuCopilotModeFlag != 0 && ptAllCopilotsAssigned() == 0) {
                menuCountdownStart();
            } else {
                slot->state = CONTROL_STATE_BROWSE;
                mainMenuBlock.flags &= ~1;
                menuSceneFlags &= ~1;
                seVoiceStartDefault(MENU_SE_CANCEL);
            }
            break;
        }
        if (padCirclePressedAlone() != 0) {
            menuSetSelected(&mainMenuBlock, mainMenuBlock.cursor);
            slot->state = 2;
            partyFormationSourceIndex = mainMenuBlock.cursor;
            if (menuCopilotModeFlag != 0) {
                if (mainMenuBlock.cursor < 3) {
                    menuEnableItems(&mainMenuBlock, 0x401);
                } else if (mainMenuBlock.cursor < 6) {
                    menuEnableItems(&mainMenuBlock, 0xC02);
                } else {
                    menuEnableItems(&mainMenuBlock, 0x80C);
                }
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            break;
        }
        if ((*(unsigned long long *) &pad->directionHeld & 0x80F00FLL) == 0x800000LL) {
            if (menuCopilotModeFlag != 0) {
                seVoiceStartDefault(MENU_SE_CANCEL);
                ptResetSplitReserve();
                menuEnableItems(&mainMenuBlock, 0xF0F);
                menuSetCursor(&mainMenuBlock, mainMenuBlock.cursor);
            }
            break;
        }
        if (pad->directionPressed & PAD_START) {
            menuAdvanceSlotCursor();
        }
        break;
    }
    case 2:
    {
        PadRecord *pad = PadData;
        if (pad->directionRepeated & PAD_UP) {
            menuNavigate(&mainMenuBlock, 0);
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            menuNavigate(&mainMenuBlock, 1);
        }
        if (pad->directionRepeated & PAD_DOWN) {
            menuNavigate(&mainMenuBlock, 2);
        }
        if (pad->directionRepeated & PAD_LEFT) {
            menuNavigate(&mainMenuBlock, 3);
        }
        ptState.menuSlot = mainMenuBlock.cursor;
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuSetSelected(&mainMenuBlock, -1);
            slot->state = CONTROL_STATE_RUNNING;
            if (menuCopilotModeFlag != 0) {
                menuEnableItems(&mainMenuBlock, 0xF0F);
            }
            break;
        }
        if (padCirclePressedAlone() != 0) {
        {
            int sourceIndex = partyFormationSourceIndex;
            unsigned int sourceSlot;
            unsigned int targetSlot;
            if (mainMenuBlock.items[sourceIndex].flags & 0x100) {
                sourceSlot = mainMenuBlock.items[sourceIndex].member;
                if (sourceSlot < 3) {
                    targetSlot = mainMenuBlock.items[mainMenuBlock.cursor].member;
                    ptSwapSlots(sourceSlot + 7, targetSlot + 7);
                } else {
                    targetSlot = mainMenuBlock.items[mainMenuBlock.cursor].member;
                    ptSwapRosterEntries(sourceSlot, targetSlot);
                }
            } else {
                sourceSlot = mainMenuBlock.items[sourceIndex].member;
                targetSlot = mainMenuBlock.items[mainMenuBlock.cursor].member;
                ptSwapSlots(sourceSlot, targetSlot);
            }
        }
        menuSetSelected(&mainMenuBlock, -1);
        slot->state = CONTROL_STATE_RUNNING;
        if (menuCopilotModeFlag != 0) {
            menuEnableItems(&mainMenuBlock, 0xF0F);
        }
        seVoiceStartDefault(MENU_SE_CONFIRM);
        ptLoadPartyModels();
        break;
        }
        if (pad->directionPressed & PAD_START) {
            menuAdvanceSlotCursor();
        }
        break;
    }
    case 3:
        break;
    case CONTROL_STATE_CHOICE:
    {
        PadRecord *pad = PadData;
        if (pad->directionRepeated & PAD_UP) {
            menuNavigate(&mainMenuBlock, 0);
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            menuNavigate(&mainMenuBlock, 1);
        }
        if (pad->directionRepeated & PAD_DOWN) {
            menuNavigate(&mainMenuBlock, 2);
        }
        if (pad->directionRepeated & PAD_LEFT) {
            menuNavigate(&mainMenuBlock, 3);
        }
        ptState.menuSlot = mainMenuBlock.cursor;
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuSetSelected(&mainMenuBlock, -1);
            slot->state = CONTROL_STATE_BROWSE;
            mainMenuBlock.flags &= ~1;
            menuSceneFlags &= ~1;
            menuEnableItems(&mainMenuBlock, 0xF0F);
            break;
        }
        if (padCirclePressedAlone() != 0) {
            if (menuCopilotModeFlag != 0) {
                ptSetCurrentSlot(mainMenuBlock.cursor + 7);
            } else {
                ptSetCurrentSlot(mainMenuBlock.cursor);
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            break;
        }
        if (pad->directionPressed & PAD_START) {
            menuAdvanceSlotCursor();
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

/* Steps one ControlSlot in state 0, 1 or 4; returns 0 on every path. */
int menuKeyItemStep(ControlSlot *slot) {
    unsigned long long padBits;

    if (!(slot->flags & CONTROL_SLOT_FLAG_MARK)) {
        return 0;
    }
    switch ((u8) slot->state) {
    case CONTROL_STATE_IDLE:
        slot->state = CONTROL_STATE_RUNNING;
        break;
    case CONTROL_STATE_BROWSE:
        if ((PadData[0].directionRepeated & PAD_LEFT) != 0) {
            cursorBufferMoveBy(menuTabCursor, -1);
        }
        if ((PadData[0].directionRepeated & PAD_RIGHT) != 0) {
            cursorBufferMoveBy(menuTabCursor, 1);
        }
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(MENU_SE_BACK);
            menuFocusRootSlot();
            return 0;
        }
        if (padCirclePressedAlone() == 0) {
            return 0;
        }
        switch (menuTabCursor[0].cursor) {
        case 0:
            menuSceneSetState(1);
            break;
        case 1:
            seVoiceStartDefault(MENU_SE_CONFIRM);
            slot->state = CONTROL_STATE_RUNNING;
            menuLinkSetCurrent(slot->link);
            break;
        }
        break;
    case CONTROL_STATE_RUNNING:
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuLinkSetCurrent(0);
            slot->state = CONTROL_STATE_BROWSE;
            return 0;
        }
        padBits = PadData[0].directionState;
        if ((padBits & 0x800000005000ULL) == 0x800000000000ULL) {
            cursorBufferMoveBy(recordItemCursor, -9);
            padBits = PadData[0].directionState;
        }
        if ((padBits & 0x200000005000ULL) == 0x200000000000ULL) {
            cursorBufferMoveBy(recordItemCursor, 9);
            padBits = PadData[0].directionState;
        }
        if ((padBits & 0x400005000ULL) == 0x400000000ULL) {
            cursorBufferMoveUnits(recordItemCursor, -1);
            padBits = PadData[0].directionState;
        }
        if ((padBits & 0x800005000ULL) == 0x800000000ULL) {
            cursorBufferMoveUnits(recordItemCursor, 1);
        }
        break;
    }
    return 0;
}

void menuSkillModelTailLoad(void);

/* One step of a ControlSlot in state 0, 4 or 7; returns 0 on every path. */
int menuConfigSlotStep(ControlSlot *slot) {
    if (!(slot->flags & CONTROL_SLOT_FLAG_MARK)) {
        return 0;
    }
    switch (slot->state) {
    case CONTROL_STATE_IDLE:
        slot->state = CONTROL_STATE_RUNNING;
        break;
    case CONTROL_STATE_RUNNING:
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(MENU_SE_BACK);
            ((VoidFunction *) xglCdBufferRewind)();
            menuSkillModelTailLoad();
            menuFocusRootSlot();
            return 0;
        }
        if ((PadData[0].directionPressed & (PAD_LEFT | PAD_RIGHT)) != 0) {
            int optionIndex = menuConfigCursor[0].cursor;
            switch (menuConfigRowKindTable[optionIndex]) {
            case 0:
                seVoiceStartDefault(MENU_SE_CURSOR);
                SaveData.cursor_memory ^= 1;
                menuCursorMemory = SaveData.cursor_memory;
                break;
            case 1:
                seVoiceStartDefault(MENU_SE_CURSOR);
                SaveData.cursor_memory_alt ^= 1;
                menuCursorMemoryAlt = SaveData.cursor_memory_alt;
                break;
            case 2:
                seVoiceStartDefault(MENU_SE_CURSOR);
                if ((PadData[0].directionPressed & PAD_LEFT) != 0) {
                    SaveData.soundMode = SaveData.soundMode + 2;
                } else {
                    SaveData.soundMode = SaveData.soundMode + 1;
                }
                SaveData.soundMode %= 3;
                soundStereoSet(SaveData.soundMode > 0);
                break;
            case 3:
                seVoiceStartDefault(MENU_SE_CURSOR);
                SaveData.actuatorMode ^= 1;
                PadData[1].actuatorMode = SaveData.actuatorMode;
                PadData[0].actuatorMode = SaveData.actuatorMode;
                if (!SaveData.actuatorMode) {
                    Vibration_Set_Weak(15);
                    Vibration_Set_Strong(200, 60, 30);
                } else {
                    Vibration_Stop();
                }
                break;
            case 4: {
                int tableIndex;
                for (tableIndex = 0; tableIndex < 2; tableIndex++) {
                    if (menuConfigButtonIndexTable[tableIndex] != SaveData.caption_all) {
                        continue;
                    }
                    tableIndex = (PadData[0].directionPressed & PAD_LEFT) != 0 ? tableIndex - 1 : tableIndex + 1;
                    tableIndex += 2;
                    tableIndex %= 2;
                    SaveData.caption_all = menuConfigButtonIndexTable[tableIndex];
                    tableIndex = -1;
                    break;
                }
                if (tableIndex == 2) {
                    SaveData.caption_all = 0;
                }
                seVoiceStartDefault(MENU_SE_CURSOR);
                break;
            }
            case 7:
                seVoiceStartDefault(MENU_SE_CURSOR);
                SaveData.dolby_digital ^= 1;
                break;
            }
            return 0;
        }
        if (padCirclePressedAlone() == 0) {
            return 0;
        }
        switch (menuConfigRowKindTable[menuConfigCursor[0].cursor]) {
        case 5: {
            void *displaySetting;
            seVoiceStartDefault(MENU_SE_CONFIRM);
            ((VoidFunction *) xglCdBufferRewind)();
            displaySetting = xglCdBufferReserve(0x00100000);
            xglSleep();
            tyaDisplaySetting((u8 *)displaySetting);
            ((VoidFunction *) xglCdBufferRewind)();
            fontObjLoadTexture(8);
            fontObjLoadTexture(9);
            break;
        }
        case 6:
            seVoiceStartDefault(MENU_SE_CONFIRM);
            saveDataSetDefaults();
            PadData[1].actuatorMode = SaveData.actuatorMode;
            PadData[0].actuatorMode = SaveData.actuatorMode;
            soundStereoSet(SaveData.soundMode != 0);
            tyaSetDisplayOffset(SaveData.displayOffsetX, SaveData.displayOffsetY);
            menuCursorMemory = SaveData.cursor_memory;
            menuCursorMemoryAlt = SaveData.cursor_memory_alt;
            break;
        case 8:
            slot->state = CONTROL_STATE_WAIT;
            menuLinkSetCurrent(0);
            seVoiceStartDefault(0x01000006);
            break;
        }
        return 0;
    case CONTROL_STATE_WAIT:
        if (padCirclePressedAlone() != 0 || padCrossPressedAlone() != 0) {
            slot->state = CONTROL_STATE_RUNNING;
            menuLinkSetCurrent(menuConfigCursor);
            seVoiceStartDefault(MENU_SE_BACK);
            menuPlaceCursor(0, 0, 0, 0);
        }
        return 0;
    }
    return 0;
}

extern unsigned char menuParagraphBreakText[];
extern unsigned char menuNewlineIndentText[];

int menuMapChoiceStep(ControlSlot *slot) {
    int slotState;
    int changed;

    changed = 0;
    if ((slot->flags & CONTROL_SLOT_FLAG_MARK) == 0) {
        return 0;
    }
    slotState = (u8) slot->state;
    switch (slotState) {
    case CONTROL_STATE_IDLE:
        slot->state = CONTROL_STATE_RUNNING;
        break;
    case CONTROL_STATE_CHOICE:
            if (menuChoiceOpenFlag != 0) {
                if (choiceGetResult() == 2) {
                    menuChoiceOpenFlag = 0;
                    slot->state = CONTROL_STATE_RUNNING;
                } else {
                    int choice;
                    choice = choiceGetResult();
                    menuChoiceOpenFlag = 0;
                    menuMapChoice = choice;
                    slot->state = CONTROL_STATE_CHOICE_APPLY;
                }
            } else {
                choiceInit(menuMainTxdEntry(slot->link->cursor + 0x1D6), menuCopilotModeFlag);
                msgBufferAppend(menuParagraphBreakText);
                msgBufferAppend(menuMainTxdEntry(0x1EA));
                choiceSetFirstOptionText(menuMainTxdEntry(0x1EB));
                choiceSetSecondOptionText(menuMainTxdEntry(0x1EC));
                menuChoiceOpenFlag = 1;
            }
        break;
    case CONTROL_STATE_CHOICE_APPLY:
            if (menuChoiceOpenFlag != 0) {
                if (choiceGetResult() == 0) {
                    menuChoiceAction = 1;
                    changed = 1;
                } else {
                    menuChoiceOpenFlag = 0;
                    slot->state = CONTROL_STATE_RUNNING;
                }
            } else {
                choiceInit(menuMainTxdEntry(slot->link->cursor + 0x1D6), 0);
                msgBufferAppend(menuNewlineText);
                msgBufferAppend(menuMainTxdEntry(menuMapChoice + 0x1EF));
                msgBufferAppend(menuNewlineIndentText);
                choiceSetFirstOptionText(menuMainTxdEntry(0x1EE - menuMapChoice));
                choiceSetSecondOptionText(menuMainTxdEntry(0x1C3));
                choiceSetRow(0xF0);
                menuChoiceOpenFlag = 1;
            }
        break;
    case CONTROL_STATE_RUNNING:
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_BACK);
            ((VoidFunction *) xglCdBufferRewind)();
            menuSkillModelTailLoad();
            menuFocusRootSlot();
        } else if (padCirclePressedAlone() != 0 && slot->flag == 0) {
            int kind;
            seVoiceStartDefault(MENU_SE_CONFIRM);
            kind = slot->link->cursor;
            if (menuMapChoiceDefaultTable[kind] == 2) {
                slot->state = CONTROL_STATE_CHOICE;
            } else {
                menuMapChoice = menuMapChoiceDefaultTable[kind];
                slot->state = CONTROL_STATE_CHOICE_APPLY;
            }
        }
        break;
    }
    return changed;
}

/* Runs while flags bit 0 is set; state is a countdown that is reset to 3 when it reads zero. */
int menuPartyChoiceStep(ControlSlot *slot) {
    if ((slot->flags & 1) == 0) {
        return 0;
    }
    if (slot->state == 0) {
        slot->state = 3;
        return 0;
    }
    if (PadData[0].directionRepeated & PAD_UP) {
        menuNavigate(&mainMenuBlock, 0);
    }
    if (PadData[0].directionRepeated & PAD_RIGHT) {
        menuNavigate(&mainMenuBlock, 1);
    }
    if (PadData[0].directionRepeated & PAD_DOWN) {
        menuNavigate(&mainMenuBlock, 2);
    }
    if (PadData[0].directionRepeated & PAD_LEFT) {
        menuNavigate(&mainMenuBlock, 3);
    }
    if (padCrossPressedAlone() != 0) {
        menuFocusRootSlot();
        seVoiceStartDefault(MENU_SE_CANCEL);
        return 0;
    }
    if (padCirclePressedAlone() != 0) {
        unsigned int cursor;
        unsigned int value;
        float stored;

        cursor = mainCommandCursors[0].cursor;
        value = *(const unsigned int *) mainMenuBlockCursor;
        if (cursor != 0) {
            *(unsigned int *) &menuPendingSelection = menuPendingSelection;
        }
        menuPendingSelection = value;
        stored = *(float *) &menuPendingSelection;
        *(float *) &menuPendingSelection = stored;
        /* menuSceneSetState takes one argument: the call keeps the original two-word form. */
        ((void (*)(int, int)) menuSceneSetState)(cursor + 1, value);
        return 0;
    }
    if (PadData[0].directionPressed & PAD_START) {
        menuAdvanceSlotCursor();
    }
    return 0;
}

void sceGsSyncV(int);
void menuSceneCameraSetup(void);
void menuSelectionRingDraw(int mode);


/* Scene start: waits until the three background layers are ready and CD reads are idle, sets the camera and the GS clear colour, then plays every control entry once (menuSkillModelLoadOnce 0..10). */
void menuSceneEnter(void) {
    do {
        sceGsSyncV(0);
        grFillFrame();
        grCopyFrame(sRender.display_buffer_base, sRender.vram.work_buffer_base);
        xglTickAdvance();
        xglSleep();
    } while (!(bgLayerReady(0) && bgLayerReady(1) && bgLayerReady(2)));

    menuSceneCameraSetup();
    bgLayerCameraSetup();
    ClearEnv.value.color_r = 0;
    ClearEnv.value.color_g = 0;
    ClearEnv.value.color_b = 0;
    ClearEnv.value.color_a = 0;
    xglRenderClearFrame();
    while (xglCdServiceIfPending() != 0) {
        pxyRequestOverlayPass();
        XenoActorSetFlagAll(2);
        bgLayerClearFlags(0);
        bgLayerClearFlags(1);
        bgLayerClearFlags(2);
        menuSelectionRingDraw(0);
        XenoActorDoit();
        XenoActorDisp();
        xglTickAdvance();
        xglSleep();
    }
    xglCdWaitIdle();
    menuSkillModelLoadOnce(0);
    menuSkillModelLoadOnce(1);
    menuSkillModelLoadOnce(2);
    menuSkillModelLoadOnce(3);
    menuSkillModelLoadOnce(4);
    menuSkillModelLoadOnce(5);
    menuSkillModelLoadOnce(6);
    menuSkillModelLoadOnce(7);
    menuSkillModelLoadOnce(8);
    menuSkillModelLoadOnce(9);
    menuSkillModelLoadOnce(10);
}

/* Resets the pad repeat, releases the scene assets, then copies the first control slot's state into menuSavedSlotState and clears menuKeyCheckPending. */
void menuSceneExit(void) {
    unsigned char firstSlotState;

    xglPadSetRepeat(0, PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT | PAD_L1 | PAD_R1, 8, 1);
    ptRefresh();
    menuSceneUnloadAssets();
    bgLayerRelease();
    Vibration_Stop();
    firstSlotState = menuSlots[0].state;
    menuKeyCheckPending = 0;
    menuSavedSlotState = firstSlotState;
}


/* Scene loop: steps the control slots and the menu each frame until a result is set (1 leaves, 2 and 3 reload a sub-scene), then runs menuSceneExit. */
int menuSceneRunLoop(void) {
    int opening;
    int sceneResult = 0;
    int i;
    int decrement;
    int remaining;
    int stepResult;
    ControlSlot *slot;
    PadRecord *padPort0;
    PadRecord *padNow;
    XglRenderState *renderBases;

    opening = 1;
    if (ptState.counts[0] == 0) {
        return 0;
    }
    if (ptState.splitMode == 0) {
        if (ptState.counts[1] == 0) {
            return 0;
        }
        menuCopilotModeFlag = 0;
    } else {
        if (ptState.counts[2] == 0) {
            return 0;
        }
        menuCopilotModeFlag = 1;
    }
    XenoActorDoit();
    XenoActorDisp();
    xglSleep();
    {
        XglRenderState *frame = &sRender;

        grCopyFrame(frame->vram.work_buffer_base, frame->draw_buffer_base);
    }
    pxySkipOverlayDraw();
    gameWork.control |= 0x04000000;
    padPort0 = PadData;
    seVoiceStartDefault(0x02000001);
    menuSceneInit();
    menuSceneEnter();

    for (;;) {
        if (menuTutorialActive == 0 && (padPort0->directionState & 0x10F00F) == 0x100000) {
            if (menuCopilotModeFlag != 0 && menuCurrentSlot == 6) {
                if (ptAllCopilotsAssigned() != 0) {
                    sceneResult = 1;
                } else {
                    menuCountdownStart();
                }
            } else {
                sceneResult = 1;
            }
        }
        padNow = PadData;
        if (((padNow->directionPressed & PAD_START) != 0 && (padNow->held & PAD_SOFT_RESET_HELD) == 0x10F)
            || (gameWork.control & 0x10000000) != 0) {
            sceneResult = 1;
            gameWork.control |= 0x10000000;
        }

        if (sceneResult != 0) {
            if (sceneResult == 1) {
                goto finish; /* goto kept: a break here changes the loop rotation and the register allocation of the loop */
            }
            switch (sceneResult) {
            case 2:
                seVoiceStartDefault(MENU_SE_CONFIRM);
                ((VoidFunction *) xglCdBufferRewind)();
                fontObjLoadTexture(4);
                fontObjLoadTexture(5);
                yg2PathRun();
                menuSkillModelTailLoad();
                selectionRingOpacity = 0;
                menuSlotSetDirection(menuSlots, 1, 128);
                menuSlotSetDirection(&menuSlots[7], 1, 128);
                break;
            case 3:
                seVoiceStartDefault(MENU_SE_CONFIRM);
                ((VoidFunction *) xglCdBufferRewind)();
                fontObjLoadTexture(6);
                ysegfRunMenu();
                menuSkillModelTailLoad();
                selectionRingOpacity = 0;
                menuSlotSetDirection(menuSlots, 1, 128);
                menuSlotSetDirection(&menuSlots[7], 1, 128);
                break;
            }
            menuSceneCameraSetup();
            sceneResult = 0;
        }

        if ((menuSceneFlags & 2) != 0) {
            if (menuPreviousCardCursor != mainCommandCursors[0].cursor) {
                if (menuCursorMemoryAlt == 0) {
                    menuPendingSelection = -1;
                }
                menuSceneSetState(mainCommandCursors[0].cursor + 1);
            } else if (menuCurrentLink != 0) {
                menuCurrentLink->mode_flags |= 1;
            }
            menuSceneFlags &= ~2;
            menuSceneFlags |= 4;
        } else if (menuTutorialActive == 0) {
            if (menuCurrentSlot > 0) {
                if (menuCurrentSlot < 8 && (padPort0->directionRepeated & 3) != 0
                    && ptGetMenuMember() != 0 && countdownActive == 0 && menuChoiceOpenFlag == 0) {
                    if (menuCopilotModeFlag != 0 && menuCurrentSlot == 6 && ptAllCopilotsAssigned() == 0) {
                        menuCountdownStart();
                    } else {
                        menuPreviousCardCursor = mainCommandCursors[0].cursor;
                        if ((padPort0->directionRepeated & 1) != 0) {
                            cursorBufferMoveBy(mainCommandCursors, -1);
                        } else {
                            cursorBufferMoveBy(mainCommandCursors, 1);
                        }
                        if (menuCurrentLink != 0) {
                            menuCurrentLink->mode_flags &= ~1;
                        }
                        menuSceneFlags |= 2;
                    }
                }
            }
        }

        for (i = 0; i < 11; i++) {
            if (menuSlots[i].value != 0) {
                if (opening != 0) {
                    decrement = 32;
                } else if (menuCursorMemoryAlt != 0) {
                    decrement = 128;
                } else if (menuSlots[i].flag == 1 || menuSlots[i].flag == 2) {
                    decrement = 12;
                } else {
                    decrement = 8;
                }
                remaining = menuSlots[i].value - decrement;
                if (remaining > 0) {
                    menuSlots[i].value = remaining;
                } else {
                    menuSlots[i].value = 0;
                }
                if (menuSlots[i].value == 0) {
                    /* Kinds 1 and 2 keep their own empty case: the original
                       case tree tests "< 1" first, which combine later turns
                       into the beqz on zero. */
                    switch (menuSlots[i].flag) {
                    case 1:
                    case 2:
                        break;
                    case 3:
                    case 4:
                        menuSlots[i].flags &= ~CONTROL_SLOT_FLAG_ACTIVE;
                        break;
                    }
                    menuSlots[i].flag = 0;
                    opening = 0;
                }
            }
        }

        if (countdownActive != 0) {
            if (countdownTick() == 0) {
                if (countdownActive == 3) {
                    countdownInit(menuMainTxdEntry(skillBoardCursorTable[0].cursor + 54), 300);
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(skillBoardCursorTable[1].cursor + 58));
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(71));
                    msgBufferAppend(menuNewlineText);
                    msgBufferAppend(menuMainTxdEntry(skillBoardCursorTable[0].cursor + 55));
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(72));
                    countdownActive = 4;
                } else {
                    countdownActive = 0;
                }
            }
        } else if (menuChoiceOpenFlag == 1) {
            if (choiceUpdate() != 0) {
                menuChoiceOpenFlag = 2;
            }
        } else {
            if (menuTutorialActive != 0) {
                menuTutorialStep();
                if ((padPort0->directionPressed & 0x10) != 0) {
                    menuTutorialRestoreState();
                }
            } else {
                if ((menuSceneFlags & 1) != 0) {
                    ControlSlot *activeSlot = &menuSlots[menuCurrentSlot];
                    if (activeSlot->flag == 0 && (padPort0->directionRepeated & 0xc) != 0) {
                        if ((padPort0->directionRepeated & 4) != 0) {
                            menuMoveCursor(&mainMenuBlock, -1);
                        } else {
                            menuMoveCursor(&mainMenuBlock, 1);
                        }
                        menuRebuildCursorBuffers();
                    }
                }
                if (menuCurrentLink != 0) {
                    PadRecord *cursorPad = PadData;

                    if ((cursorPad->directionRepeated & PAD_UP) != 0) {
                        cursorBufferMoveBy(menuCurrentLink, -1);
                    }
                    if ((cursorPad->directionRepeated & PAD_DOWN) != 0) {
                        cursorBufferMoveBy(menuCurrentLink, 1);
                    }
                }
            }

            for (i = 0; i < 11; i++) {
                ControlCallback step = menuSlots[i].step;
                if (step != 0 && (menuSceneFlags & 6) == 0 && menuTutorialActive == 0) {
                    slot = &menuSlots[i];
                    /* the slot field is declared as the void callback; the steps return int */
                    stepResult = ((ControlStep) step)(slot);
                    if (stepResult != 0) {
                        sceneResult = stepResult;
                    }
                }
            }
        }

        if ((menuSceneFlags & 4) != 0) {
            menuSceneFlags &= ~4;
        }
        fontTrackEase(equipPanelTrack);
        menuMessageTextUpdate();
        menuDrawObject(&mainMenuBlock);
        menuDrawObject(&itemMenuBlock);
        pxyRequestOverlayPass();
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        menuSelectionRingDraw(menuTutorialToggleFlags & 1);
        if ((gameWork.control & 0x10000000) == 0) {
            xglCallbackQueueAdd(menuSceneDraw);
            XenoActorDoit();
            XenoActorDisp();
        }
        xglTickAdvance();
        xglSleep();
    }

finish:
    menuSceneExit();
    if ((gameWork.control & 0x10000000) == 0) {
        if (menuChoiceAction == 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
        }
        renderBases = &sRender;
        grCopyFrame(renderBases->vram.work_buffer_base, renderBases->draw_buffer_base);
        for (i = 15; i >= 0; i--) {
            /* Re-taken every step: the original copies the loop-invariant
               address into its own register. */
            XglRenderState *frame = &sRender;

            sceGsSyncV(0);
            grFillFrame();
            grCopyFrame(frame->display_buffer_base, frame->vram.work_buffer_base);
            xglTickAdvance();
            xglSleep();
        }
        xglTickAdvance();
        xglSleep();
        xglTickAdvance();
        xglSleep();
        ptRefreshCurrent();
        xglSleep();
        if (menuChoiceAction == 1) {
            uwamonoAttachCurrent(mapChoiceCursor[0].cursor, menuMapChoice);
        } else if (menuChoiceAction == 2) {
            uwamonoDetachCurrent();
        } else {
            fadeReset();
            fadeStartIn(30);
        }
    }
    gameWork.control &= ~0x04000000;
    return 0;
}

/* Sets menuKeyCheckPending when the first state object's key is -1, then runs the scene loop menuSceneRunLoop. */
void menuSceneRunKeyCheck(void) {
    if (ptState.progress->mapId == -1) {
        menuKeyCheckPending = 1;
    }
    menuSceneRunLoop();
}


/* Runs the scene loop menuSceneRunLoop inside the split that ptEnterSplit opens (roster compaction), unless ptState.splitMode is 1. */
int menuSceneRunSplit(void) {
    int result;

    if (ptState.splitMode == 1) {
        result = menuSceneRunLoop();
    } else {
        ptEnterSplit();
        result = menuSceneRunLoop();
        ptLeaveSplit();
    }
    return result;
}

/* Strings read by the menu draw and message code (.sdata 0x0069D548..0x0069D580); defined after the functions so their code keeps the extern form. */
u8 extraTitleText[8] = "  Extra";
u8 menuNewlineText[8] = "\n";
u8 fiveDigitNumberFormat[8] = "%5d";
int partyFormationSourceIndex = 0;
u8 menuParagraphBreakText[8] = "\n\n ";
u8 menuNewlineIndentText[8] = "\n ";

/* Float constants of menuModelDraw (.lit4 0x0069B20C..0x0069B218): the three values the function reads by name. */
const float menuModelRowHeight = 0.32f;
const float menuModelRowStep = 0.020833f;
const float menuModelBaseY = -0.1f;
int menuPendingSelection = -1;

/* CD file names are string literals of the tables below (menuSkillModelFileNames, menuUmnImageFileNames). */

/* .data of this file (0x3EB040-0x3EB210), address order. */
unsigned char menuConfigRowKindTable[16] = { 2, 7, 4, 3, 1, 0, 5, 6, 8, 9, 0, 0, 0, 0, 0, 0 };

short menuMainCardLabelIdTable[20] = { 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 0, 0, 0 };

signed char menuMainCardLabelRangeTable[32] = { 0, 2, 0, 2, 16, 1, 16, 1, 13, 2, 13, 2, 8, 2, 10, 2, 2, 4, 12, 1, 6, 2, 6, 2, 15, 1, 0, 0, 0, 0, 0, 0 };

const char *menuSkillModelFileNames[22] = {
    "yosikawa\134model\134atk.pxy",
    "yosikawa\134model\134atk.txy",
    "yosikawa\134model\134rsv.pxy",
    "yosikawa\134model\134rsv.txy",
    "yosikawa\134skillmdl\134skilplate01.pxy",
    "yosikawa\134skillmdl\134skilplate01.txy",
    "yosikawa\134skillmdl\134skilball01.pxy",
    "yosikawa\134skillmdl\134skilball01.txy",
    "yosikawa\134skillmdl\134skilball02.pxy",
    "yosikawa\134skillmdl\134skilball02.txy",
    "yosikawa\134skillmdl\134skilball03.pxy",
    "yosikawa\134skillmdl\134skilball03.txy",
    "yosikawa\134skillmdl\134skilball04.pxy",
    "yosikawa\134skillmdl\134skilball04.txy",
    "yosikawa\134skillmdl\134skilplate02.pxy",
    "yosikawa\134skillmdl\134skilplate02.txy",
    "yosikawa\134skillmdl\134skilplate03.pxy",
    "yosikawa\134skillmdl\134skilplate03.txy",
    "yosikawa\134skillmdl\134skilplate04.pxy",
    "yosikawa\134skillmdl\134skilplate04.txy",
    "yosikawa\134skillmdl\134skilplate05.pxy",
    "yosikawa\134skillmdl\134skilplate05.txy"
};

/* Size pairs of the model sets: each pair is the .pxy and the .txy buffer size of one model. Models 0 and 1 are the
 * party models atk and rsv, not skill models. */
unsigned int menuSkillModelSizes[22] = { 0x4800, 0x8800, 0x4800, 0x8800, 0xd800, 0x9000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x11000, 0x1000, 0x11000, 0x1000, 0x11000, 0x1000, 0x11000 };

const char *menuUmnImageFileNames[10] = { "yosikawa\134umn1.jpg", "yosikawa\134umn2.jpg", "yosikawa\134umn3.jpg", "yosikawa\134umn4.jpg", "yosikawa\134umn5.jpg", "yosikawa\134umn6.jpg", "yosikawa\134umn7.jpg", "yosikawa\134umn8.jpg", 0, 0 };

/* VIF1 packet template (menu_scene.c writes its +0x40 tag and +0x60 size); bytes of the unmodeled parts. */
VifPacketTemplate menuUmnImagePacketTemplate = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 81, 4, 0, 0, 0, 0, 0, 0, 16, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 56, 8, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0, 0 },
    0x0ULL,
    { 82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 83, 0, 0, 0, 0, 0, 0, 0 },
    0x7000,
    { 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0 }
};

unsigned char menuMainCardLabelMarkup[24] = { 18, 16, 16, 12, 128, 128, 128, 14, 1, 1, 0, 0, 0, 13, 3, 15, 72, 0, 128, 0, 0, 0, 0, 0 };

/* The first two bytes are a markup header; the label text is copied in from +2. */
char menuMainCardLabelBuffer[24] = { 0x19, 0x03 };
