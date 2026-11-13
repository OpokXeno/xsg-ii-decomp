#include "common.h"
#include "shared.h"
#include "main/tu092.h"
#include "main/tu094.h"
#include "main/tu096.h"
#include "main/tu097.h"
#include "main/tu098.h"
#include "main/tu100.h"
#include "main/tu101.h"
#include "main/tu021.h"
#include "main/tu109.h"
#include "main/tu110.h"
#include "main/tu121.h"
#include "main/xgl_cd.h"
#include "main/sdv_033.h"
#include "gs.h"
#include "main/pad.h"
#include "main/tu093.h"

/* Control buffer tables: entries below CONTROL_ENTRY_SECTOR_COUNT take CD sector buffers, the rest buffer
   reserves (menuSkillModelBuffersAlloc). */
#define CONTROL_ENTRY_COUNT 11
#define CONTROL_ENTRY_SECTOR_COUNT 4
/* ControlSlot flags bit 1: the slot is active (set by func_00210560). */
#define CONTROL_SLOT_FLAG_ACTIVE 0x02
/* ControlSlot flags bit 0: the mark of the slot that is current (set and cleared by the menuFocus and menuScene functions). */
#define CONTROL_SLOT_FLAG_MARK 0x01
/* Menu item list (menuMapListDraw): row pitch, the text id of row 0 for menuMainTxdEntry, and the right edge of
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

/* One 0x14-byte entry of the table at D_006A51E0: cursor at 0x00, previous_cursor at 0x04, capacity at 0x08. */
typedef struct CursorEntry {
    int cursor;
    int previous_cursor;
    int capacity;
    unsigned char unmodeled_0C[8];
} CursorEntry;

/* The limits block of a ControlContext; both are set to 1000 by menuTutorialPageReset. */
struct ControlLimits {
    unsigned char unmodeled_00[8];
    int skillPoints;
    int classPoints;
};

struct ControlBuffer {
    unsigned char header[4];
    unsigned char cells[4][8];
    unsigned char flags[2];
    unsigned char items[4];     /* 0x26: entries of the selection list */
    unsigned char count;        /* 0x2A: the item count, also the capacity passed to cursorBufferSetOrInit */
};

struct ControlContext {
    int kind;                   /* 0x00: compared with 0xC; indexes D_003EC0D8 at kind - 4 */
    unsigned char unmodeled_04[4];
    struct ControlLimits *limits;
    struct ControlBuffer *buffer;
};

void fontObjInitGlobals();
void ptReset();
extern int D_0069D4DC;
extern u8 D_0069D4E0;
extern u8 *D_00645868[];
extern unsigned char D_006A4900[];
extern CursorBuffer D_006A4928[];
extern unsigned char xglCallbackQueueRun[];

void menuSceneReset(void) {
    xglCdPendingReadsClear();
    ptReset();
    selectionBuildKeyTable();
    func_00229898();
    xglTickReset();
    cursorBufferInit((CursorBuffer *) D_006A4900, 6, 6, 0);
    cursorBufferInit((CursorBuffer *) (D_006A4900 + 0x14), 4, 4, 0);
    cursorBufferInit(D_006A4928, 0, 0, 0);
    D_0069D4DC = 0;
    D_0069D4E0 = 0;
    func_00233D70();
    fontObjInitGlobals();
    menuSkillModelReset();
    bgLayerInit();
    D_00645868[0] = xglCallbackQueueRun;
    cursorBufferTablesInit();
}

extern signed char D_0069D502;
extern u8 D_0069D504;
extern u8 selectionWindowCount;
extern signed char D_0069E228;
extern CursorBuffer D_006A5150[];
extern CursorBuffer D_006A5168[];
extern CursorBuffer D_006A5180[];
extern CursorBuffer D_006A5198[];
extern CursorBuffer D_006A51B0[];
extern CursorBuffer D_006A51C8[];
extern int D_006A51E0[];
extern CursorBuffer D_006A5220[];
extern CursorBuffer D_006A5238[];

void cursorBufferTablesInit(void) {
    cursorBufferInit((CursorBuffer *) D_006A4900, 6, 6, 0);
    cursorBufferInit((CursorBuffer *) (D_006A4900 + 0x14), 4, 4, 0);
    cursorBufferInit(D_006A5150, selectionWindowCount, 0xA, 0);
    cursorBufferInit(D_006A5168, 0x6B, 0x12, 0);
    cursorBufferInit(D_006A5180, 3, 4, 0);
    cursorBufferInit(D_006A5198, 7, 7, 0);
    cursorBufferInit(D_006A51B0, D_0069D504, 4, 0);
    cursorBufferInit(D_006A51C8, 8, 8, 0);
    cursorBufferInit((CursorBuffer *) D_006A51E0, 4, 4, 0);
    cursorBufferInit((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 1), 8, 8, 0);
    cursorBufferInit((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 2), 4, 4, 0);
    cursorBufferInit(D_006A5238, 8, 8, 0);
    cursorBufferInit(D_006A5220, 0xA, 0xA, 0);
    D_0069E228 = 0;
    D_0069D502 = 0;
}

extern signed char D_0069D4EC;

/* Refreshes a cursor buffer: while D_0069D4EC is set only the unit size and capacity change, otherwise a full init. */
void cursorBufferSetOrInit(CursorBuffer *buffer, int capacity, int unit_size, int mode_flags) {
    if (D_0069D4EC != 0) {
        buffer->unit_size = unit_size;
        cursorBufferSetCapacity(buffer, capacity);
    } else {
        cursorBufferInit(buffer, capacity, unit_size, mode_flags);
    }
}

/* Rewinds the cursors of the menu's cursor buffers (their cursor and previous_cursor words) and clears D_0069E228. */
void cursorBuffersRewind(void) {
    CursorEntry *entries;
    signed char *flag;
    u8 *hook;
    int hookWord;

    hook = D_00645868[0];
    hookWord = *(int *) hook;
    if (D_0069D504 != 0) {
        *(int *) hook = hookWord;
        flag = &D_0069E228;
    }
    D_006A5150[0].previous_cursor = 0;
    D_006A5150[0].cursor = 0;
    *flag = 0;
    D_006A5168[0].previous_cursor = 0;
    D_006A5168[0].cursor = 0;
    D_006A5180[0].previous_cursor = 0;
    D_006A5180[0].cursor = 0;
    D_006A5198[0].previous_cursor = 0;
    D_006A5198[0].cursor = 0;
    D_006A51B0[0].previous_cursor = 0;
    D_006A51B0[0].cursor = 0;
    D_006A51C8[0].previous_cursor = 0;
    D_006A51C8[0].cursor = 0;
    entries = (CursorEntry *) D_006A51E0;
    entries[0].previous_cursor = 0;
    entries[0].cursor = 0;
    entries[1].previous_cursor = 0;
    entries[1].cursor = 0;
    entries[2].previous_cursor = 0;
    entries[2].cursor = 0;
    D_006A5238[0].previous_cursor = 0;
    D_006A5238[0].cursor = 0;
    D_006A5220[0].previous_cursor = 0;
    D_006A5220[0].cursor = 0;
}

struct ControlSlot;

typedef void (*ControlCallback)(struct ControlSlot *slot);
typedef int (*ControlStep)(struct ControlSlot *slot);

typedef struct ControlSlot {
    ControlCallback callback;   /* 0x00: called by menuSlotDraw */
    ControlCallback step;       /* 0x04: the slot's state-machine step, run every frame by menuSceneRunLoop */
    void *link;                 /* 0x08 */
    unsigned short value;       /* 0x0C: written when flag is set */
    u8 state;                   /* 0x0E */
    u8 flag;                    /* 0x0F */
    u8 flags;                   /* 0x10: bit 1 set here */
    u8 unmodeled_11[3];
} ControlSlot;

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
void func_00210560(ControlSlot *slot, int flag, int value) {
    flag &= 0xFF;
    if (flag != 0) {
        slot->value = (short) value;
    }
    slot->flag = flag;
    slot->flags |= CONTROL_SLOT_FLAG_ACTIVE;
}

extern signed char menuUmnImageCount;
extern signed char menuUmnImageIndex;
extern int menuUmnImageSourceBuffer;
extern int menuUmnImageBuffers[];
extern FontTrack D_006A54A0[];

void menuUmnImageReset(void) {
    int i;
    int *sequenceSlot;

    sequenceSlot = menuUmnImageBuffers;
    sequenceSlot += 7;
    for (i = 0; i < 8; i++) {
        *sequenceSlot = 0;
        sequenceSlot -= 1;
    }
    menuUmnImageCount = 0;
    menuUmnImageIndex = 0;
    menuUmnImageSourceBuffer = 0;
    fontTrackSetBoth(&D_006A54A0[0], 0, 0);
    fontTrackSetBoth(&D_006A54A0[1], 0, 0);
    fontTrackSetBoth(&D_006A54A0[2], 0, 0);
    fontTrackSetBoth(&D_006A54A0[3], 0, 0);
}

int xglCdBufferReserve(long size);
void xglCdReadFileAsync();
extern int menuUmnImageFileNames[];

void menuUmnImageLoad(int count) {
    int remaining;
    int buffer;
    int *sequenceSlot;

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

/* The 32-byte request that xglJpegDecode reads: its source and destination words are set here, the rest is zeroed. */
typedef struct JpegDecodeRequest {
    u32 source;
    u32 destination;
    u8 unmodeled_08[0x18];
} JpegDecodeRequest;

void *memset(void *destination, int value, unsigned int size);
void xglJpegDecode(JpegDecodeRequest *request);

/* Sets up JPEG decode of sequence slot index, then advances the sequence counter menuUmnImageIndex and reads the next
   file while it is below menuUmnImageCount. */
void menuUmnImageDecode(u8 sequenceIndex) {
    JpegDecodeRequest request;

    memset(&request, 0, sizeof(request));
    request.source = menuUmnImageSourceBuffer;
    request.destination = menuUmnImageBuffers[sequenceIndex];
    xglJpegDecode(&request);
    menuUmnImageIndex += 1;
    if ((u8) menuUmnImageIndex < (u8) menuUmnImageCount) {
        xglCdReadFileAsync(menuUmnImageFileNames[(u8) menuUmnImageIndex], menuUmnImageSourceBuffer, 4, (u8) menuUmnImageIndex);
    }
}

XglPacket *xglPacketGetCurrent(void);
void sceVif1PkCnt(XglPacket *packet, int value);
void sceVif1PkAddDataN(XglPacket *packet, const void *data, int size);
void sceVif1PkRef(XglPacket *packet, const void *data, int size, int a3, int a4, int a5);
void xglDirectQueueBegin(void);
void xglDirectQueuePush(u64 address, u64 data);
void xglDirectQueueSubmitRef(void);

/* The VIF1 packet template at D_003EB170: its +0x40 tag and +0x60 size are written before each use. */
typedef struct VifPacketTemplate {
    u8 unmodeled_00[0x40];
    u64 tag;                    /* 0x40 */
    u8 unmodeled_48[0x18];
    u32 size;                   /* 0x60 */
} VifPacketTemplate;

extern VifPacketTemplate D_003EB170;

/* Queues one textured sprite (GS A+D packets) for the VIF1 packet at the current position. */
void xglPacketAddTexturedDraw(int x, int y, int sequenceIndex, int scale) {
    XglPacket *packet;
    int texture;
    int shade;
    int alpha;

    texture = menuUmnImageBuffers[sequenceIndex];
    packet = xglPacketGetCurrent();
    shade = ((scale * 96) >> 7) + 32;
    alpha = ((scale << 5) >> 7) + 96;
    if (texture != 0) {
        if ((unsigned char) menuUmnImageIndex >= sequenceIndex + 1) {
            D_003EB170.tag = 0x20000001C0ULL;
            D_003EB170.size = 0xE00;
            sceVif1PkCnt(packet, 0);
            sceVif1PkAddDataN(packet, &D_003EB170, 28);
            sceVif1PkRef(packet, (const void *) texture, 0xE00, 0, 0x51000E00, 0);
            xglDirectQueueBegin();
            xglDirectQueuePush(GS_TEXFLUSH, 0);
            xglDirectQueuePush(GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
            xglDirectQueuePush(GS_TEX0_1, GS_SET_TEX0(0x3800, 8, GS_PSMCT32, 9, 9, 1, 0, 0, 0, 0, 0, 0));
            xglDirectQueuePush(GS_CLAMP_1, GS_SET_CLAMP(1, 1, 0, 0, 0, 0));
            xglDirectQueuePush(GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
            xglDirectQueuePush(GS_ALPHA_1, GS_SET_ALPHA(0, 1, 2, 1, alpha));
            xglDirectQueuePush(GS_RGBAQ, GS_SET_RGBAQ(shade, shade, shade, 0x80, 0x3F800000));
            xglDirectQueuePush(GS_PRIM, GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0));
            xglDirectQueuePush(GS_UV, 0);
            xglDirectQueuePush(GS_XYZ2, GS_SET_XYZ(x * 16 + 0x7000, y * 16 + 0x7200, 0));
            xglDirectQueuePush(GS_UV, GS_SET_UV(0x1C00, 0x200));
            y += 0x20;
            xglDirectQueuePush(GS_XYZ2, GS_SET_XYZ((x + 0x8C0) << 4, y * 16 + 0x7200, 0));
            xglDirectQueueSubmitRef();
        }
    }
}

/* The item list of a menu: the highlighted index and the number of items. */
struct ItemList {
    int selected;
    unsigned char unmodeled_04[4];
    unsigned int count;
};

void textDrawSegment(int x, int y, void *text, int style);
void panelDrawNineSlice(void *record, int x, int y, int width, int height);
void menuDrawTitledFrame(int x, int y, int width, int height, void *text);
int xglFontGetStringWidth();
extern char D_0068A3F0[];
extern unsigned char menuPanelFrame[];
extern unsigned char D_0069D530[];
extern int D_006A54A4[];
extern int D_006A54A8[];
extern int D_006A54AC[];

/* Draws the item list panel with one row per item. Row i's track lane (i & 1) takes the highlight target, 128 on the selected item and 0 otherwise, and the packet scale is that lane's current value. */
void menuMapListDraw(struct ItemList *list) {
    int *target;
    int panelTop;
    int panelHeight;
    int rowY;
    int textX;
    int i;
    int scale;
    int textWidth;
    int selected;

    panelTop = 0x94 - list->count * 7;
    panelHeight = list->count * MENU_ITEM_ROW_HEIGHT + 0x1B;
    textDrawSegment(0, 0, D_0068A3F0, 0);
    panelDrawNineSlice(menuPanelFrame, 0x1A, panelTop, 0x1CC, panelHeight);
    fontTrackEase(&D_006A54A0[0]);
    fontTrackEase(&D_006A54A0[1]);
    fontTrackEase(&D_006A54A0[2]);
    fontTrackEase(&D_006A54A0[3]);
    i = 0;
    if (list->count != 0) {
        rowY = panelTop;
        do {
            if (i & 1) {
                target = &D_006A54AC[7 * (i >> 1)];
            } else {
                target = &D_006A54A8[7 * (i >> 1)];
            }
            selected = (i == list->selected);
            *target = selected << 7;
            if (i & 1) {
                target = &D_006A54A4[7 * (i >> 1)];
            } else {
                target = &D_006A54A0[i >> 1].current_pair.x;
            }
            scale = *target;
            xglPacketAddTexturedDraw(0x20, rowY + 0x13, i, scale);
            textX = MENU_ITEM_TEXT_RIGHT_X;
            textWidth = xglFontGetStringWidth(menuMainTxdEntry(i + MENU_ITEM_TXD_BASE));
            textX -= textWidth * 15 >> 5;
            textDrawSegment(textX, rowY + 0x19, menuMainTxdEntry(i + MENU_ITEM_TXD_BASE), selected == 0 ? selected | 2 : selected);
            rowY += MENU_ITEM_ROW_HEIGHT;
            i++;
        } while (i < list->count);
    }
    fontObjResetAndBuild();
    menuDrawTitledFrame(0x1E, panelTop + 5, 0x1C4, panelHeight - 10, D_0069D530);
}

extern unsigned int menuSkillModelSizes[];
extern int menuSkillModelPxyBuffers[];
extern int menuSkillModelTxyBuffers[];
extern unsigned char menuSkillModelLoadTally[];
extern signed char menuSkillModelsReady;
int xglCdSectorAlloc(unsigned int size);

/* Sets up the two 11-entry tables (menuSkillModelPxyBuffers and menuSkillModelTxyBuffers) from the size pairs at menuSkillModelSizes: the first
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

extern int D_003EB098[];
extern int D_003EB09C[];

/* Runs once per index: the byte at menuSkillModelLoadTally[index] is set on the first call, then the two files of that
   index are read into the buffers held in menuSkillModelPxyBuffers and menuSkillModelTxyBuffers (buffers and file ids not yet proved). */
void menuSkillModelLoadOnce(int index) {
    u8 *tally;
    int firstBuffer;
    int secondBuffer;

    tally = &menuSkillModelLoadTally[index];
    if (*tally == 0) {
        firstBuffer = menuSkillModelPxyBuffers[index];
        *tally += 1;
        if (firstBuffer != 0) {
            xglCdReadFileAsync(D_003EB098[index * 2], firstBuffer, 3, (index * 2) & 0xFE);
        }
        secondBuffer = menuSkillModelTxyBuffers[index];
        if (secondBuffer != 0) {
            xglCdReadFileAsync(D_003EB09C[index * 2], secondBuffer, 3, (index * 2 + 1) & 0xFF);
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

int msgRangeSetText();
int selectionBlockTestBit();
int selectionEntryBitOutsideMask(int group, int slot, int sub, int mask);
char *func_00A9B8D8();
char *func_00A9B9C8();
char *func_00A9B750();
char *func_00A9B7D8();
extern u8 D_0069D518;
extern char *menuTutorialLine;
extern int menuCurrentSlot;
extern int D_0069D508;
extern int D_0069E23C;
extern signed char D_0069E240[8];
extern unsigned char D_0069E238[4];
extern unsigned char D_006A52E0[];
extern unsigned char selectionWindowList[];
extern unsigned char selectionPlainList[];
extern unsigned char D_003EB040[];
extern u8 D_0063F869[];
extern u8 selectionPlainCount;
extern unsigned char D_006A5250[];
extern ControlSlot menuSlots[];
struct ControlContext *ptGetMenuMember();

/* Chooses the menu text id for the current state and passes it to msgRangeSetText when it changed. */
void menuMessageTextUpdate(void) {
    char *previousText;
    char *nextText;
    int currentSlot;
    int entry;

    previousText = ((char **) D_006A5250)[0];
    nextText = previousText;
    if (D_0069D518 != 0) {
        nextText = menuTutorialLine;
    } else if (menuCurrentSlot == 0) {
        if ((u8) menuSlots[0].state == 0) {
            if (D_0069D4E0 != 0 && ((int *) D_006A4900)[0] == 4) {
                nextText = menuMainTxdEntry(433);
            } else {
                nextText = menuMainTxdEntry(((int *) D_006A4900)[0] + 410);
            }
        } else {
            nextText = menuMainTxdEntry(D_0069E240[((int *) D_006A4900)[5]] + 416);
        }
    } else if ((D_0069D4DC & 2) != 0) {
        if (D_0069D4E0 != 0 && ((int *) D_006A4900)[0] == 4) {
            nextText = menuMainTxdEntry(433);
        } else {
            nextText = menuMainTxdEntry(((int *) D_006A4900)[0] + 410);
        }
    } else if (menuCurrentSlot == 6) {
        switch (D_006A4928[0].cursor) {
        case 0:
            if (D_0069D4E0 != 0) {
                nextText = menuMainTxdEntry(431);
            } else {
                nextText = menuMainTxdEntry(428);
            }
            break;
        case 1:
            if (D_0069D4E0 != 0) {
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
                switch (D_006A4928[0].cursor) {
                case 0:
                    nextText = menuMainTxdEntry(402);
                    break;
                case 1:
                    nextText = menuMainTxdEntry(420);
                    break;
                }
            } else {
                if (D_006A5150[0].capacity != 0) {
                    nextText = func_00A9B8D8(selectionWindowList[D_006A5150[0].cursor], 1);
                } else {
                    nextText = menuMainTxdEntry(402);
                }
            }
            break;
        case 8:
            if ((u8) menuSlots[currentSlot].state == 1) {
                switch (D_006A4928[0].cursor) {
                case 0:
                    nextText = menuMainTxdEntry(402);
                    break;
                case 1:
                    nextText = menuMainTxdEntry(420);
                    break;
                }
            } else {
                if (selectionBlockTestBit(D_006A5168[0].cursor + 1) != 0) {
                    nextText = func_00A9B9C8(D_006A5168[0].cursor + 1, 1);
                } else {
                    nextText = menuMainTxdEntry(400);
                }
            }
            break;
        case 2: {
            if (D_006A5180[0].capacity != 0) {
                nextText = func_00A9B750(D_0069E238[D_006A5180[0].cursor], 1);
            } else {
                nextText = menuMainTxdEntry(411);
            }
            break;
        }
        case 3: {
            if ((u8) menuSlots[currentSlot].state == 6) {
                int *cursorWords = D_006A51E0;

                if (selectionEntryBitOutsideMask(cursorWords[0], cursorWords[5], cursorWords[10], D_0069D508) != 0) {
                    nextText = menuMainTxdEntry(400);
                } else {
                    nextText = selectionSetValue(cursorWords[0], cursorWords[5], cursorWords[10], 1);
                }
            } else if ((u8) menuSlots[currentSlot].state == 1) {
                if (D_006A4928[0].cursor != 0) {
                    nextText = menuMainTxdEntry(437);
                } else {
                    nextText = menuMainTxdEntry(435);
                }
            } else {
                int *cursorWords = D_006A51E0;

                if (ptGetMenuMember()->buffer->flags[0] < cursorWords[0]) {
                    nextText = menuMainTxdEntry(436);
                } else {
                    nextText = menuMainTxdEntry(435);
                }
            }
            break;
        }
        case 5:
            if ((u8) menuSlots[currentSlot].state == 1 || (D_0069D4E0 == 0 && D_0069E23C <= 0)) {
                switch (D_006A4928[0].cursor) {
                case 0:
                    if (D_0069D4E0 != 0) {
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
                int *cursorWords = (int *) D_006A51B0;

                if ((cursorWords[4] & 1) != 0) {
                    if (D_0069D4E0 != 0) {
                        nextText = func_00A9B7D8(ptGetMenuMember()->buffer->items[cursorWords[0]], 1);
                    } else {
                        entry = ptGetMenuMember()->buffer->items[cursorWords[0]];
                        if (selectionEntryBitOutsideMask(entry >> 5 & 3, entry >> 2 & 7, entry & 3, D_0069D508) != 0) {
                            nextText = menuMainTxdEntry(400);
                        } else {
                            nextText = selectionRecordSetValue(entry, 1);
                        }
                    }
                } else if (D_0069D4E0 != 0) {
                    if (D_006A4928[0].cursor == 0) {
                        if (selectionPlainCount != 0) {
                            nextText = func_00A9B7D8(selectionPlainList[D_006A51C8[0].cursor], 1);
                        } else {
                            nextText = menuMainTxdEntry(434);
                        }
                    }
                } else {
                    entry = D_006A52E0[D_006A51C8[0].cursor];
                    if (selectionEntryBitOutsideMask(entry >> 5 & 3, entry >> 2 & 7, entry & 3, D_0069D508) != 0) {
                        nextText = menuMainTxdEntry(400);
                    } else {
                        nextText = selectionRecordSetValue(entry, 1);
                    }
                }
            }
            break;
        case 9:
            if (D_003EB040[D_006A5220[0].cursor] == 7 && D_0063F869[0] != 0) {
                nextText = menuMainTxdEntry(391);
            } else {
                nextText = menuMainTxdEntry(D_003EB040[D_006A5220[0].cursor] + 378);
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
        msgRangeSetText(&D_006A5250, nextText);
    }
}


int selectionMemberCapacity();
int selectionPairRecordCount();
int selectionCopilotRecordCount();
int selectionSubIsTaken();
int selectionCollectEntries();
int func_A8B810();
extern u16 D_0069D50C;
extern unsigned char D_003EC0D8[];
extern unsigned char D_003EC662[];
extern unsigned char D_006A4D0C[];
extern unsigned char D_006A53FE[];

/* Rebuilds the cursor buffers for the layout selected by menuCurrentSlot and sets their capacities from the context. */
void menuRebuildCursorBuffers(void) {
    struct ControlContext *context;
    int i;
    int count;
    int mode;
    int capacity;

    D_003EC662[0] = D_006A4D0C[0];
    switch (menuCurrentSlot) {
    case 4:
        if (D_0069D4E0 != 0) {
            D_0069D50C = 1;
            for (i = 0; i < ptGetMenuMember()->buffer->count; i++) {
                if (ptGetMenuMember()->buffer->items[i] == 0x24) {
                    D_0069D50C |= 2;
                }
            }
            if (D_006A53FE[0] == 5) {
                context = func_0022A560();
                capacity = selectionCopilotRecordCount(context->kind, D_0069D50C);
                cursorBufferSetCapacity(D_006A5198, capacity);
                return;
            }
            cursorBufferSetCapacity(D_006A5198, D_003EC0D8[ptGetMenuMember()->kind - 4]);
            return;
        }
        if (D_006A53FE[0] == 5) {
            context = func_0022A560();
            capacity = selectionPairRecordCount(context->kind, D_0069D50C);
            cursorBufferSetCapacity(D_006A5198, capacity);
            return;
        }
        context = func_0022A560();
        capacity = selectionMemberCapacity(context->kind);
        cursorBufferSetCapacity(D_006A5198, capacity);
        return;
    case 2:
        D_0069E238[0] = D_0069E238[1] = D_0069E238[2] = D_0069E238[3] = 0;
        count = 0;
        for (i = 1; i < 4; i++) {
            if (D_0069D4E0 != 0) {
                if (ptGetMenuMember()->kind == 12 && func_A8B810(6, i) != 0) {
                    D_0069E238[count] = i;
                    count++;
                }
            } else if (func_A8B810(ptGetMenuMember()->kind, i) != 0) {
                D_0069E238[count] = i;
                count++;
            }
        }
        cursorBufferSetCapacity(D_006A5180, count);
        return;
    case 5:
        cursorBufferSetOrInit(D_006A51B0, ptGetMenuMember()->buffer->count, 4, 0);
        if (D_0069D4E0 == 0) {
            mode = 0;
            switch (D_006A4928[0].cursor) {
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
            context = func_0022A560();
            D_0069E23C = selectionCollectEntries(D_006A52E0, context, mode, D_0069D508);
            D_0069D504 = selectionSubIsTaken(func_0022A560(), 1, 7, 1) + 3;
            cursorBufferSetCapacity(D_006A51C8, D_0069E23C);
            D_006A51C8[0].unit_size = 4;
        }
        return;
    case 3:
        {
            float *angles = (float *)&menuSkillModelLoadTally[16];
            float x = -0.52359879f;
            float y = -0.34906587f;
            float z = -0.17453294f;
            angles[0] = x;
            angles[1] = y;
            angles[2] = z;
            ((int *)angles)[3] = 0;
        }
        break;
    }
}

typedef struct ControlLink {
    int kind;                   /* 0x00: also the row of the byte tables at D_0069E238 and 0x0069D4F8 */
    u8 unmodeled_04[4];
    int width;                  /* 0x08 */
    u8 unmodeled_0C[4];
    int flags;                  /* 0x10: bit 0 set while this link is current */
} ControlLink;

extern ControlLink *D_0069D4CC;

/* Moves the current mark (flags bit 0) from the old current link to link, and makes link current. */
void func_00211918(ControlLink *link) {
    if (D_0069D4CC != 0) {
        D_0069D4CC->flags &= ~1;
    }
    if (link != 0) {
        link->flags |= 1;
    }
    D_0069D4CC = link;
}

/* Calls func_00210560 on slot from with flag 3 when from is below to, else 4, but only while flags bit 0x10
   is clear and flags 0x2 is set. */
void menuSlotSetLeaveDirection(int from, int to) {
    if (menuSlots[from].flags & 0x10) {
        return;
    }
    if ((menuSlots[from].flags & CONTROL_SLOT_FLAG_ACTIVE) == 0) {
        return;
    }
    if (from < to) {
        func_00210560(&menuSlots[from], 3, 0x80);
    } else {
        func_00210560(&menuSlots[from], 4, 0x80);
    }
}

/* Re-issues func_00210560 on slot to: when its flags bit 0x2 is set, only a flag of 3 or 4 passes. The mode is
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
        func_00210560(&menuSlots[to], 1, 0x80);
    } else if (from < to) {
        func_00210560(&menuSlots[to], 2, 0x80);
    } else {
        func_00210560(&menuSlots[to], 0, 0);
    }
}

typedef struct StateBlock {
    u8 unmodeled_00[4];
    int baseY;                  /* 0x04 */
    u8 unmodeled_08[4];
    int offset;                 /* 0x0C */
    u8 unmodeled_10[0xC];
    FontTrack tracks[3];        /* 0x1C, 0x38 and 0x54: the font tracks of the menu */
    u8 unmodeled_70[0x118];
    int menuFlags;              /* 0x188: bit 0x100000 is set when the scene flag test passes */
    u8 unmodeled_18C[0x240];
    union {
        u8 flag3CC;             /* 0x3CC: read as a byte by menuSceneDrawOverlays */
        int cursor;             /* 0x3CC: read as a word */
    };
    u8 unmodeled_3D0[0xC];
    int mode;                   /* 0x3DC */
    u8 unmodeled_3E0[4];
    int flags;                  /* 0x3E4: bits 0 and 1 cleared by menuFocusRootSlot */
} StateBlock;

extern StateBlock D_006A4940;
extern u8 D_006A4D28[];
extern int menuPendingSelection;
void menuSetMode(StateBlock *menu, int mode);
void menuSetSelected(StateBlock *menu, int index);

void menuFocusRootSlot(void) {
    menuSlots[menuCurrentSlot].flags &= ~CONTROL_SLOT_FLAG_MARK;
    menuSlotSetLeaveDirection(menuCurrentSlot, 0);
    menuSlotSetEnterDirection(7, menuCurrentSlot);
    menuSlots[0].flags |= CONTROL_SLOT_FLAG_MARK;
    D_0069D4DC &= ~1;
    menuPendingSelection = -1;
    D_006A4940.flags &= ~1;
    menuCurrentSlot = 0;
    menuSetMode(&D_006A4940, 0);
    menuSetSelected(&D_006A4940, -1);
    D_006A4940.flags &= ~2;
    ((StateBlock *) D_006A4D28)->flags &= ~1;
    func_00211918(0);
    cursorBufferInit(D_006A4928, 0, 0, 0);
}

void func_00211B88(int index, int flag) {
    ControlSlot *slot = &menuSlots[index];

    slot->state = 0;
    slot->flags |= CONTROL_SLOT_FLAG_MARK;
    func_00211918(slot->link);
    menuSlotSetLeaveDirection(menuCurrentSlot, index);
    if (flag != 0) {
        menuSlotSetEnterDirection(7, menuCurrentSlot);
    } else {
        menuSlotSetLeaveDirection(7, index);
    }
    menuSlotSetEnterDirection(index, menuCurrentSlot);
    menuCurrentSlot = index;
}

extern int menuSavedCursor;
void menuSetCursor(StateBlock *menu, int index);
void menuPlaceItemCursor(StateBlock *menu, int index);
void menuEnableItems(StateBlock *menu, int value);

void menuFocusMainSlot(void) {
    ControlSlot *slot = &menuSlots[7];

    D_0069D4DC |= 1;
    if (D_0069D4E0 != 0 && ((int *) D_006A4900)[0] == 2) {
        StateBlock *menu = &D_006A4940;

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
        menuSetMode(&D_006A4940, 1);
        if (D_0069D4E0 != 0) {
            menuEnableItems(&D_006A4940, 0x401);
        }
    }
    slot->state = 0;
    slot->flags |= CONTROL_SLOT_FLAG_MARK;
    D_006A4940.flags |= 1;
    func_00211918(slot->link);
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

typedef struct KeyedInfo {
    u8 unmodeled_00[8];
    u8 code;                    /* 0x08 */
} KeyedInfo;

typedef struct KeyedObject {
    u8 unmodeled_00[4];
    KeyedInfo *info;            /* 0x04 */
    int key;                    /* 0x08: compared with -1 */
    u8 unmodeled_0C[0x15];
    u8 mapFlag;                 /* 0x21 */
} KeyedObject;

typedef struct PartyState {
    KeyedObject *progress;      /* 0x00 */
    KeyedObject *slots[10];     /* 0x04 */
    KeyedObject *roster[10];    /* 0x2C */
    unsigned char splitMode;    /* 0x54 */
    unsigned char frontIndex;   /* 0x55 */
    unsigned char backIndex;    /* 0x56 */
    unsigned char counts[3];    /* 0x57 */
    unsigned char cursor;       /* 0x5A */
    u8 unmodeled_5B;
    int word5C;                 /* 0x5C */
} PartyState;

extern PartyState ptState;
void selectionBlockFill();
int ptFindRecord();
int ptFindSlot();
void menuFocusRootSlot(void);
/* Rewinds the cursors of the menu's cursor buffers (their cursor and previous_cursor words) and clears D_0069E228. */
void menuPlaceCursor();
void menuSetValuesMode();
/* Rebuilds the cursor buffers for the layout selected by menuCurrentSlot and sets their capacities from the context. */
void menuRebuildCursorBuffers(void);
void fontObjLoadTexture(int mode);
extern u8 D_006A5240[];
extern u8 D_006A542C[];

typedef union MenuLatch {
    int word;
    u8 bytes[4];
} MenuLatch;

extern MenuLatch D_0069D514;
extern signed char D_0069D4ED;

/* The TU's view of xglCdBufferRewind: its result is never used. */
typedef void VoidFunction(void);

/* Scene-change handler (menuState is the menu state id): clears the current slot, refreshes the cursors, then switches
   on menuState to set the menu mode and the cursor buffers. */
void menuSceneSetState(int menuState) {
    int savedCursor;
    StateBlock *stateBlock = (StateBlock *) D_006A4D28;
    ControlSlot *controlSlot = &menuSlots[menuState];

    D_006A4940.flags &= ~2;
    menuSlots[menuCurrentSlot].flags &= ~CONTROL_SLOT_FLAG_MARK;
    menuSetValuesMode(stateBlock, 0);
    stateBlock->flags &= ~(1 | 4);
    menuSetSelected(&D_006A4940, -1);
    menuPlaceCursor(0, 0, 0, 0);
    if (menuSavedCursor != -1 && menuState != 3) {
        menuEnableItems(&D_006A4940, 0xF0F);
        menuSetCursor(&D_006A4940, menuSavedCursor);
        menuSavedCursor = -1;
    }
    if (menuPendingSelection == -1) {
        savedCursor = D_0069D514.word = menuPendingSelection;
    }
    if (!D_0069D4EC && menuState != menuCurrentSlot) {
        cursorBuffersRewind();
    }
    switch (menuState) {
    case 1:
        seVoiceStartDefault(0x01000006);
        D_0069D4DC &= ~1;
        menuSetMode(&D_006A4940, 9);
        D_006A4940.flags &= ~1;
        cursorBufferInit(D_006A4928, 2, 2, 0);
        if (!D_0069D4ED && menuCurrentSlot == 8) {
            func_00211B88(menuState, 0);
            menuSlots[menuState].state = 7;
        } else {
            func_00211B88(menuState, 0);
        }
        break;
    case 8:
        seVoiceStartDefault(0x01000006);
        D_0069D4DC &= ~1;
        selectionBlockFill();
        func_00211B88(menuState, 0);
        break;
    case 2:
        if (D_0069D4ED != 0 && D_0069D4E0 != 0 && ptFindRecord(12) && ptFindRecord(6)) {
            menuSetCursor(&D_006A4940, ptFindSlot(12) - 7);
            seVoiceStartDefault(0x01000006);
            D_0069D4DC |= 1;
            menuSetMode(&D_006A4940, 3);
            D_006A4940.flags &= ~1;
            func_00211B88(menuState, 1);
        } else if (menuPendingSelection == -1) {
            if (menuCurrentSlot == 0) {
                seVoiceStartDefault(0x01000002);
            }
            menuFocusMainSlot();
        } else {
            seVoiceStartDefault(0x01000006);
            D_0069D4DC |= 1;
            menuSetMode(&D_006A4940, 3);
            D_006A4940.flags &= ~1;
            func_00211B88(menuState, 1);
        }
        cursorBufferInit(D_006A4928, 0, 0, 0);
        break;
    case 3:
        if (menuPendingSelection == -1) {
            if (menuCurrentSlot == 0) {
                seVoiceStartDefault(0x01000002);
            }
            D_0069D4DC |= 1;
            if (D_0069D4E0 != 0) {
                menuSetMode(&D_006A4940, 1);
                menuEnableItems(&D_006A4940, 0xF0F);
                if (D_006A4940.cursor < 3) {
                    menuSavedCursor = D_006A4940.cursor;
                    menuSetCursor(&D_006A4940, D_006A4940.cursor + 3);
                }
                menuEnableItems(&D_006A4940, 0x40E);
                menuPlaceItemCursor(&D_006A4940, D_006A4940.cursor);
                fontTrackAdvance(&D_006A4940.tracks[0]);
                fontTrackAdvance(&D_006A4940.tracks[1]);
                fontTrackAdvance(&D_006A4940.tracks[2]);
            } else {
                menuSetMode(&D_006A4940, 1);
            }
            D_006A4940.flags |= 1;
            controlSlot = &menuSlots[7];
            controlSlot->state = 0;
            controlSlot->flags |= CONTROL_SLOT_FLAG_MARK;
            func_00211918(controlSlot->link);
            menuSlotSetLeaveDirection(menuCurrentSlot, 7);
            menuSlotSetEnterDirection(7, menuCurrentSlot);
            menuCurrentSlot = 7;
        } else {
            savedCursor = D_006A4940.cursor;
            seVoiceStartDefault(0x01000006);
            D_0069D4DC |= 1;
            menuSetMode(&D_006A4940, 3);
            if (D_0069D4E0 != 0) {
                menuEnableItems(&D_006A4940, 0xF0F);
                if (savedCursor < 3) {
                    menuSavedCursor = savedCursor;
                    menuSetCursor(&D_006A4940, savedCursor + 3);
                } else {
                    menuSetCursor(&D_006A4940, savedCursor);
                }
                menuEnableItems(&D_006A4940, 0x40E);
                menuPlaceItemCursor(&D_006A4940, D_006A4940.cursor);
                fontTrackAdvance(&D_006A4940.tracks[0]);
                fontTrackAdvance(&D_006A4940.tracks[1]);
                fontTrackAdvance(&D_006A4940.tracks[2]);
                D_0069D514.word = D_006A4940.cursor;
            }
            D_006A4940.flags &= ~1;
            func_00211B88(menuState, 1);
        }
        cursorBufferInit(D_006A4928, 2, 2, 0);
        break;
    case 4:
        if (menuPendingSelection == -1) {
            if (!menuCurrentSlot) {
                seVoiceStartDefault(0x01000002);
            }
            menuFocusMainSlot();
            cursorBufferInit(D_006A4928, 0, 0, 0);
        } else {
            seVoiceStartDefault(0x01000006);
            D_0069D4DC |= 1;
            menuSetMode(&D_006A4940, 3);
            D_006A4940.flags &= ~1;
            func_00211B88(menuState, 1);
            cursorBufferInit(D_006A4928, 2, 2, 0);
        }
        break;
    case 5:
        ptState.cursor = D_006A4940.cursor;
        if (menuPendingSelection == -1) {
            if (menuCurrentSlot == 0) {
                seVoiceStartDefault(0x01000002);
            }
            menuFocusMainSlot();
            cursorBufferInit(D_006A4928, 0, 4, 0);
            break;
        }
        seVoiceStartDefault(0x01000006);
        D_0069D4DC |= 1;
        menuSetMode(&D_006A4940, 3);
        D_006A4940.flags &= ~1;
        cursorBufferInit(D_006A4928, 4, 4, 0);
        if (D_0069D4E0) {
            cursorBufferSetOrInit(D_006A51C8, selectionPlainCount, 8, 0);
            cursorBufferSetOrInit(D_006A51B0, ptGetMenuMember()->buffer->count, 4, 0);
            cursorBufferSetCapacity(D_006A4928, 1);
        } else {
            D_0069D504 = (u8) (selectionSubIsTaken(func_0022A560(), 1, 7, 1) + 3);
            cursorBufferSetOrInit(D_006A51B0, func_0022A560()->buffer->count, 4, 0);
            cursorBufferSetCapacity(D_006A4928, 4);
        }
        func_00211B88(menuState, 1);
        break;
    case 6:
        if (menuCurrentSlot == 0) {
            seVoiceStartDefault(0x01000002);
        }
        D_0069D4DC &= ~1;
        menuSetMode(&D_006A4940, 1);
        D_006A4940.flags &= ~1;
        controlSlot->state = 0;
        controlSlot->flags |= CONTROL_SLOT_FLAG_MARK;
        func_00211918(controlSlot->link);
        menuSlotSetLeaveDirection(menuCurrentSlot, 7);
        menuSlotSetEnterDirection(7, menuCurrentSlot);
        menuCurrentSlot = menuState;
        cursorBufferInit(D_006A4928, 2, 2, 0);
        break;
    case 9:
        D_0069D4DC &= ~1;
        func_00211B88(menuState, 0);
        cursorBufferInit(D_006A4928, 0, 0, 0);
        ((VoidFunction *) xglCdBufferRewind)();
        fontObjLoadTexture(8);
        fontObjLoadTexture(9);
        break;
    case 10:
        D_0069D4DC &= ~1;
        func_00211B88(menuState, 0);
        cursorBufferInit(D_006A4928, 0, 0, 0);
        ((VoidFunction *) xglCdBufferRewind)();
        menuUmnImageLoad(*(int *) D_006A5240);
        break;
    case 7:
    case 0:
    default:
        menuFocusRootSlot();
        return;
    }
    ptState.cursor = D_006A4940.cursor;
    menuRebuildCursorBuffers();
}

XglCamera *xglStudioSelectGetActiveCamera(int studio_index);
void xglCameraSetWindow(XglCamera *camera, int x0, int y0, int x1, int y1);

/* Resets the active studio camera: travel position and rotation to zero (position z 14), fov to 40 degrees,
   then sets its window to (0, 0)-(512, 448). */
void xglActiveCameraSetup(void) {
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

void xglCdArenaOpen(int first, int second);
void xglCdBufferAlloc(int value);
void menuUmnImageReset(void);
/* Sets up the two 11-entry tables (menuSkillModelPxyBuffers and menuSkillModelTxyBuffers) from the size pairs at menuSkillModelSizes: the first four entries use the sector allocator, the rest the buffer reserve; then clears menuSkillModelLoadTally and menuSkillModelsReady. */
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

/* The menu control block at 0x0063F840: counters and toggles that menuConfigSlotStep steps. */
typedef struct MenuControl {
    u8 unmodeled_00[0x28];
    u8 counter_28;              /* 0x28: stepped by 1 or 2 on each press, then reduced modulo 3 */
    u8 flag_29;                 /* 0x29: toggled by the case 7 arm */
    u8 unmodeled_2a[0x32 - 0x2A];
    short field_32;             /* 0x32 */
    short field_34;             /* 0x34 */
    u8 unmodeled_36[0x3A - 0x36];
    u8 index_3a;                /* 0x3A: matched against D_0069D4F0 */
    u8 unmodeled_3b[0x3E - 0x3B];
    u8 flag_3e;                 /* 0x3E: toggled, copied to PadData field 0x56 */
    u8 unmodeled_3f[0x46 - 0x3F];
    u8 flag_46;                 /* 0x46: toggled, mirrored to D_0069D4EC */
    u8 flag_47;                 /* 0x47: toggled, mirrored to D_0069D4ED */
} MenuControl;

extern MenuControl SaveData;

/* menuSceneInit's step and draw callbacks (defined below in this file). */
struct MenuActor;
struct ControlRecord;
struct MenuInputBlock;
int menuMainCommandStep(ControlSlot *slot);
void menuDrawMainCards(ControlSlot *slot);
int menuItemListStep(ControlSlot *slot);
void menuDrawSlotItems(ControlSlot *slot);
int menuLayoutPlaceStep(ControlSlot *slot);
void menuLayoutSubmit(ControlSlot *slot);
int menuLicenseChargeStep(ControlSlot *slot);
void menuDrawLicensePage(ControlSlot *slot);
int menuCommandPageStep(ControlSlot *slot);
void menuDrawCommandPage(struct MenuActor *actor);
int menuEquipAssignStep(ControlSlot *slot);
void menuDrawEquipPage(ControlSlot *slot);
int menuPartyFormationStep(ControlSlot *slot);
void func_00214C98(void);
int menuButtonHandle(struct MenuInputBlock *input);
void menuDrawExtraPage(void);
int menuRecordItemStep(ControlSlot *slot);
int menuRecordItemDetail(struct ControlRecord *record);
int menuConfigSlotStep(ControlSlot *slot);
void menuDrawConfigPage(ControlSlot *slot);
int menuMapChoiceStep(ControlSlot *slot);
void menuMapDraw(struct ControlRecord *record);
void menuResetState();
extern u8 D_0069D503;
extern int D_0069D4E4;
extern int countdownActive;
extern signed char D_0069D501;
extern int D_002B5DA8[];
extern FontTrack D_006A5110;
extern FontTrack D_006A5130[];
extern u8 D_0069D50E;
extern u8 D_0069D538[7];
extern signed char D_0069D4F8[8];
extern u8 D_0069D529;
extern u8 menuTutorialStage;
extern u8 D_0069D51C;
extern float menuRingTilt;
extern float menuRingTiltPrev;
extern float menuRingColumnAngle[4];

void ptRefresh(void);
void selectionRebuildList(void);
void xglPadSetRepeat(int port, int mask, int delay, int rate);

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
    void **slotLink;

    ptRefresh();
    ptState.word5C = func_001FFB70();
    menuSceneLoadAssets();

    D_0069D4EC = SaveData.flag_46;
    D_0069D4ED = SaveData.flag_47;
    D_0069D503 = 24;
    menuCurrentSlot = 0;
    menuPendingSelection = -1;
    D_0069D4E4 = 0;
    countdownActive = 0;
    D_0069E23C = 0;
    menuSavedCursor = -1;
    D_0069D514.word = -1;
    D_0069D501 = 0;
    if (xglFlagsGet1(231)) {
        D_0069D503 = (D_0069D503 | 4) + 16;
    }
    if (xglFlagsGet1(232)) {
        D_0069D503 = (D_0069D503 | 2) + 16;
    }
    if (ptState.word5C == 9 || (ptState.progress->key != -1 && (unsigned int) (D_002B5DA8[0] - 5510) >= 601)) {
        D_0069D503 = (D_0069D503 | 1) + 16;
    }

    D_0069D504 = 3;
    D_0069D508 = 1;
    for (i = 1; i < 32; i++) {
        if (dataBoxChkCategory4(i)) {
            D_0069D508 |= 1 << i;
            if (dataBoxChkCategory3(i + 76) == 0) {
                dataBoxIncCategory3(i + 76);
            }
        }
    }

    if (D_0069D4E0 != 0) {
        D_0069D50C = 1;
    } else {
        D_0069D50C = 1;
        for (i = 0; i < 16; i++) {
            if (func_00A8F478(i + 1)) {
                D_0069D50C |= 1 << i;
            }
        }
    }
    if (D_0069D4E0 != 0) {
        D_0069D504 = 3;
    }

    selectionRebuildWindowList();
    selectionRebuildList();
    menuFocusRootSlot();
    cursorBufferSetOrInit((CursorBuffer *) D_006A4900, 6, 6, 0);
    cursorBufferSetOrInit((CursorBuffer *) ((CursorEntry *) D_006A4900 + 1), D_0069D503 >> 4, 4, 0);

    D_0069E240[0] = -1;
    D_0069E240[1] = -1;
    D_0069E240[2] = -1;
    D_0069E240[3] = -1;
    for (i = 0; i < (D_0069D503 >> 4); i++) {
        D_0069E240[i] = -1;
        seen = 0;
        for (k = 0; k < 4; k++) {
            if (D_0069D503 & (1 << k)) {
                if (seen == i) {
                    D_0069E240[i] = k;
                    break;
                }
                seen++;
            }
        }
    }

    menuResetItems(&D_006A4940, D_0069D4E0);
    menuResetItems(D_006A4D28, D_0069D4E0);
    menuHide(&D_006A4940, 0);
    menuHide(D_006A4D28, 4);
    menuSetValuesMode(D_006A4D28, 0);
    fontTrackSetBoth(&D_006A5110, 0x200, 0);
    fontTrackSetBoth(D_006A5130, -0x110, D_0069D504 * 16 + D_0069D4E0 * 12 + 0x104);

    r = xglFlagsGet8(401);
    flag = r < 12 || (unsigned int) (r - 102) < 4;
    if ((unsigned int) (r - 50) < 8 || (unsigned int) (r - 79) < 9) {
        D_0069D50E = 2;
    } else {
        D_0069D50E = flag;
    }
    if (r < 7) {
        D_006A4940.menuFlags |= 0x100000;
    }

    cursorBufferSetOrInit(D_006A5150, selectionWindowCount, 10, 0);
    cursorBufferSetOrInit(D_006A5168, 107, 18, 0);
    cursorBufferSetOrInit(D_006A5180, 3, 4, 0);
    cursorBufferSetOrInit(D_006A5198, 7, 7, 0);
    cursorBufferSetOrInit(D_006A51B0, D_0069D504, 4, 0);
    cursorBufferSetOrInit(D_006A51C8, 8, 8, 0);
    cursorBufferSetOrInit((CursorBuffer *) D_006A51E0, 4, 4, 0);
    cursorBufferSetOrInit((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 1), 8, 8, 0);
    cursorBufferSetOrInit((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 2), 4, 4, 0);
    cursorBufferSetOrInit(D_006A5238, 8, 8, 0);

    D_0069D4CC = 0;
    flagsB = xglFlagsGet8(401);
    D_006A5238[0].capacity = 0;
    for (i = 0; i < 7; i++) {
        if (D_0069D538[i] <= flagsB) {
            D_006A5238[0].capacity += 1;
        }
    }
    if (ptState.progress->mapFlag != 0) {
        D_006A5238[0].capacity = 8;
    }
    if (flagsB < 91) {
        D_0069D4F8[0] = 0;
    } else {
        D_0069D4F8[0] = 2;
    }

    cursorBufferSetOrInit(D_006A5220, 9, 10, 0);
    menuPlaceCursor(0, 0, 0, 0);
    D_0069D4DC = 0;

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
    *slotLink = D_006A4900;
    func_00210560(slot, 1, 128);
    if (D_0069D4EC != 0) {
        slot->state = D_0069E228;
    }

    slot[1].step = menuItemListStep;
    slot[1].callback = menuDrawSlotItems;
    slot[1].link = D_006A5150;
    slot[2].step = menuLayoutPlaceStep;
    slot[2].callback = menuLayoutSubmit;
    slot[3].step = menuLicenseChargeStep;
    slot[3].callback = menuDrawLicensePage;
    slot[4].step = menuCommandPageStep;
    slot[4].callback = (ControlCallback) menuDrawCommandPage;
    slot[5].step = menuEquipAssignStep;
    slot[5].callback = menuDrawEquipPage;
    slot[6].step = menuPartyFormationStep;
    slot[6].callback = (ControlCallback) func_00214C98;
    slot[7].step = (ControlStep) menuButtonHandle;
    slot7 = &slot[7];
    slot[2].link = D_006A5180;
    slot7->callback = (ControlCallback) menuDrawExtraPage;
    slot[3].link = D_006A51E0;
    slot[4].link = D_006A5198;
    slot[5].link = D_006A51C8;
    func_00210560(slot7, 1, 128);

    slot[8].step = menuRecordItemStep;
    slot[8].callback = (ControlCallback) menuRecordItemDetail;
    slot[8].link = D_006A5168;
    slot[9].step = menuConfigSlotStep;
    slot[9].callback = menuDrawConfigPage;
    slot[9].link = D_006A5220;
    slot[10].step = menuMapChoiceStep;
    slot[10].callback = (ControlCallback) menuMapDraw;
    slot[10].link = D_006A5238;
    menuResetState(D_006A5250);

    D_0069D518 = 0;
    menuTutorialLine = 0;
    D_0069D529 = 0;
    menuTutorialStage = 0;
    D_0069D51C = 0;
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

    if (D_0069D502 != 0) {
        if (D_0069E240[0] == 0) {
            menuSlots[0].state = 1;
            cursorBufferSetOffset((CursorBuffer *) ((CursorEntry *) *slotLink + 1), 0);
        } else {
            D_0069D502 = 0;
        }
    }
}

extern float D_0069B20C;
extern float D_0069B210;
extern float D_0069B214;
extern int D_006A4944[];
extern Matrix4 D_006A4860;
extern Vector4 D_006A48A0;
void xglMatrixStackUnit(void);
void xglMatrixStackRotX(float angle);
void xglMatrixStackRotY(float angle);
void xglMatrixStackTrans(const Vector4 *vector);
void xglMatrixStackScale(const Vector4 *vector);
void xglMatrixStackSave(Matrix4 matrix);
void nmlModelSetDrawMatrix(Matrix4 matrix);
void pxySetTxyBlock(int block);
void pxySubmitModel(int model);

void menuModelDraw(int index, float tx, float ty, float tz, float rotX, float rotY, float scale, int hasTransform) {
    int txyBlock;
    int modelId;

    if (hasTransform != 0) {
        xglMatrixStackUnit();
        xglMatrixStackRotY(0.2f);
        D_006A48A0.x = -2.75f;
        D_006A48A0.z = 9.5f;
        D_006A48A0.w = 1.0f;
        D_006A48A0.y = D_0069B214 - (float) D_006A4944[0] * D_0069B20C * D_0069B210;
        xglMatrixStackTrans(&D_006A48A0);
        xglMatrixStackRotX(rotX);
        xglMatrixStackRotY(rotY);
        D_006A48A0.x = tx;
        D_006A48A0.y = ty;
        D_006A48A0.z = tz;
        D_006A48A0.w = 1.0f;
        xglMatrixStackTrans(&D_006A48A0);
        D_006A48A0.x = D_006A48A0.y = D_006A48A0.z = scale;
        D_006A48A0.w = 1.0f;
        xglMatrixStackScale(&D_006A48A0);
        xglMatrixStackSave(D_006A4860);
    }
    nmlModelSetDrawMatrix(D_006A4860);
    txyBlock = menuSkillModelTxyBuffers[index];
    if (txyBlock != 0) {
        pxySetTxyBlock(txyBlock);
    }
    modelId = menuSkillModelPxyBuffers[index];
    if (modelId != 0) {
        pxySubmitModel(modelId);
    }
}

extern Matrix4 D_006A48B0[];
extern Vector4 D_006A48F0[];
void xglMatrixStackRotZ(float angle);

void menuModelDrawRotated(int index, float tx, float ty, float tz, float rotX, float rotY, float rotZ, float yaw, int hasTransform) {
    int txyBlock;
    int modelId;
    float modelScale;

    if (hasTransform != 0) {
        xglMatrixStackUnit();
        xglMatrixStackRotY(yaw);
        D_006A48F0[0].x = tx;
        D_006A48F0[0].y = ty;
        D_006A48F0[0].z = tz;
        D_006A48F0[0].w = 1.0f;
        xglMatrixStackTrans(D_006A48F0);
        xglMatrixStackRotX(rotX);
        xglMatrixStackRotY(rotY);
        xglMatrixStackRotZ(rotZ);
        modelScale = 5.0f;
        D_006A48F0[0].w = 1.0f;
        D_006A48F0[0].z = modelScale;
        D_006A48F0[0].y = modelScale;
        D_006A48F0[0].x = modelScale;
        xglMatrixStackScale(D_006A48F0);
        xglMatrixStackSave(D_006A48B0[0]);
    }
    nmlModelSetDrawMatrix(D_006A48B0[0]);
    txyBlock = menuSkillModelTxyBuffers[index];
    if (txyBlock != 0) {
        pxySetTxyBlock(txyBlock);
    }
    modelId = menuSkillModelPxyBuffers[index];
    if (modelId != 0) {
        pxySubmitModel(modelId);
    }
}

float xglSin(float);
float xglCos(float);
int xglTickGet(void);
void nmlModelSetTransparency(float);
int selectionEntryBitCount(struct ControlContext *, int, int);
int selectionGroupIsComplete(struct ControlContext *, int);

typedef union FloatBits {
    float f;
    int i;
} FloatBits;

extern float D_0069D540;

/* PadData port record from +0x30: the four direction halfwords, also read as one doubleword. */
typedef struct PadDirections {
    u8 unmodeled_00[0x28];
    unsigned short buttonsHeld;   /* 0x28: the held buttons (PadRecord.held) */
    u8 unmodeled_2A[6];
    union {
        struct {
            unsigned short held;      /* 0x30 */
            unsigned short pressed;   /* 0x32 */
            unsigned short repeated;  /* 0x34 */
            unsigned short released;  /* 0x36 */
        };
        unsigned long long doubleword; /* 0x30: the four halfwords as one doubleword */
    };
    u8 unmodeled_38[0x56 - 0x38];
    u8 field_56;                /* 0x56: written by menuConfigSlotStep in both pads */
    u8 unmodeled_57[0x65 - 0x57];
    signed char axis65;         /* 0x65: read by menuSelectionRingDraw */
    u8 unmodeled_66[0x80 - 0x66];
} PadDirections;

extern PadDirections PadData[2];
extern const float D_0069B218;
extern const float D_0069B21C;
extern const float D_0069B220;
extern const float D_0069B224;
extern const float D_0069B228;
extern const float D_0069B230;
extern const float D_0069B234;
extern const float D_0069B238;
extern const float D_0069B23C;
extern const float D_0069B240;
extern const float D_0069B244;
extern const float D_0069B22C;
extern const float D_0069B248;
extern const float D_0069B24C;
extern const float D_0069B250;
extern const float D_0069B254;
extern const float D_0069B258;
extern const float D_0069B25C;
extern const float D_0069B260;
extern const float D_0069B264;
extern const float D_0069B268;
extern const float D_0069B26C;
extern const float D_0069B270;
extern const float D_0069B274;
extern const float D_0069B278;
extern const float D_0069B27C;
extern const float D_0069B280;
extern const float D_0069B284;
extern const float D_0069B288;

void menuSelectionRingDraw(int mode) {
    float columnSpeed[4];
    int passCount;
    struct ControlContext *ctx;
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
                menuModelDraw(10 - D_006A51E0[0], 0.0f, 0.0f, 0.0f, menuRingTilt, f21, 0.2f, 0);
            }
        }
        columnSpeed[0] = columnSpeed[1] = columnSpeed[2] = columnSpeed[3] = 0.0210000016f;
        for (i = 0; i < 28; i++) {
            col = i >> 3;
            row = i & 7;
            entryCount = selectionEntryBitCount(ctx, col, row);
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
                            (D_006A51E0[0] * 8 + D_006A51E0[5]) == i) {
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
        tilt += (float) PadData[0].axis65 * 0.000199999995f;
        menuRingTiltPrev = f21;
        if (tilt < 0.0f) {
            tilt = 0.0f;
        }
        menuRingTilt = tilt;
        if (1.57079637f < tilt) {
            ((FloatBits *) &menuRingTilt)->f = 1.57079637f;
        }
        if (PadData[0].held & PAD_R3) {
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
        D_0069D540 = 0.0f;
    }
    if (!(D_006A4940.flags & 2) && (menuSlots[7].flags & CONTROL_SLOT_FLAG_ACTIVE) && menuSlots[7].flag == 0 &&
        ((D_006A4940.mode < 2U) || D_006A4940.mode == 5 ||
         D_006A4940.mode == 6)) {
        D_0069D540 = D_0069D540 + (1.0f - D_0069D540) * 0.08f;
    } else {
        D_0069D540 = D_0069D540 * 0.9f;
    }
    if (menuSkillModelLoadTally[0] == 3) {
        if (menuSkillModelLoadTally[1] == menuSkillModelLoadTally[0]) {
            if (0.0f < D_0069D540) {
                nmlModelSetTransparency(D_0069D540);
                menuModelDrawRotated(0, -2.8f, -2.4f, 3.6f, -0.2f, -0.4f, -0.1f, 0.0f, 1);
                nmlModelSetTransparency(D_0069D540);
                menuModelDrawRotated(1, 2.6f, -2.4f, 2.6f, -0.2f, -0.4f, -0.1f, -0.24f, 1);
            }
        }
    }
}

/* Submits the four object requests of the layout at base: ids 0xE4 and 0xE6, then 0xEA twice. */
void func_00213A78(int base) {
    packetSubmitRequest(0xE4, 0, base, 0x11);
    packetSubmitRequest(0xE6, 0, base - 0x1D, 0x11);
    packetSubmitRequestEx(0xEA, 0, base, 0x200, 0x10, 0x801);
    packetSubmitRequestEx(0xEA, 0, base + 0x19, 0x200, 0x10, 0x801);
}

void gameOverDrawOverlay(int value);
void choiceDraw(void);
void xglFontFlush(void);
extern u8 D_003EC662[];

/* Draws the menu overlays, each one gated by its flag, then flushes the font queue. */
void menuSceneDrawOverlays(void) {
    D_003EC662[0] = D_006A4940.flag3CC;
    if (D_0069D529 != 0) {
        D_006A4940.offset = -0x30;
    } else {
        D_006A4940.offset = 0;
    }
    if (D_0069D518 != 0) {
        gameOverDrawOverlay(0x40);
    }
    fontObjResetAndBuild();
    if (D_0069D4E4 != 0) {
        choiceDraw();
    }
    if (countdownActive != 0) {
        countdownDraw();
    }
    xglFontFlush();
}

extern int D_0069D4D8;
extern int *D_006A53A8[];
extern u8 D_003EB1E0[];
extern char D_003EB1F8[];
extern char D_003EB1FA[];
extern signed char D_003EB078[];
extern short D_003EB050[];
extern u8 D_003EB240[];
extern u8 D_003EB268[];
void menuDrawScrollFrame(int value);
void msgRangeDrawRevealed(void *text, int first, int second);

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

    if (D_0069D518 != 0 && (slot->flags & 4) == 0) {
        return;
    }
    originY = D_006A4944[0];
    fontObjResetAndBuild();
    menuDrawScrollFrame(D_0069D529 * 3 + 4);
    if (menuCurrentSlot != 0 && menuCurrentSlot != 9) {
        if (menuCurrentSlot != 10) {
            func_00213A78(originY + 57);
            if ((D_0069D4DC & 2) == 0) {
                labelCount = 12;
                if (menuCurrentSlot != 7) {
                    labelCount = *D_006A53A8[0] * 2 + (D_0069D4E0 != 0);
                }
                labelIds = D_003EB050 + D_003EB078[labelCount * 2];
                labelCount = D_003EB078[labelCount * 2 + 1];
                textDrawSegment(0, 0, D_0068A3F0, 0);
                i = 0;
                if (labelCount > 0) {
                    labelBuffer = D_003EB1FA;
                    labelText = D_003EB1F8;
                    labelMarkup = D_003EB1E0;
                    labelSpan = 612;
                    do {
                        slots = labelCount + 1;
                        centerX = labelSpan / slots - 50;
                        labelSpan += 612;
                        strcpy(labelBuffer, menuMainTxdEntry(*labelIds++));
                        width = xglFontGetStringWidth(labelText);
                        if (i == D_006A4928[0].cursor || menuCurrentSlot == 7) {
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
        panelDrawNineSlice(D_003EB240, cardX, originY + 16, 79, 32);
        if ((unsigned int) (menuCurrentSlot - 1) < 8 || (unsigned char) slot->state == 0) {
            if (i == ((CursorEntry *) slot->link)[0].cursor) {
                panelDrawNineSlice(D_003EB268, w + 1, originY + 8, 95, 48);
            }
            if (i == menuCurrentSlot - 1 ||
                ((menuCurrentSlot == 0 || menuCurrentSlot == 7) && i == ((CursorEntry *) slot->link)[0].cursor && (D_0069D4DC & 2) == 0) ||
                ((D_0069D4DC & 2) != 0 && i == D_0069D4D8)) {
                packetSubmitRequest(203 + i * 2, cardX, originY + 16, 1);
                if (menuCurrentSlot != 0 && (D_0069D4DC & 2) == 0) {
                    packetSubmitRequest(224, cardX + 31, originY + 39, 1);
                }
            } else {
                packetSubmitRequest(202 + i * 2, cardX, originY + 16, 1);
            }
        } else {
            packetSubmitRequest(202 + i * 2, cardX, originY + 16, 1);
        }
    }

    if (menuCurrentSlot == 0 || menuCurrentSlot == 9 || menuCurrentSlot == 10) {
        centerX = D_0069D503 >> 4;
        if (centerX == 0) {
            centerX = 1;
        }
        for (i = 0; i < (int) centerX; i++) {
            if (D_0069E240[i] != -1) {
                cellPitch = 0x1F0 / (int) centerX;
                cellX = cellPitch * i;
                cellW = cellPitch - 4;
                cellDrawX = cellX + 10;
                panelDrawNineSlice(D_003EB240, cellDrawX, originY + 58, cellW, 24);
                if ((unsigned char) slot->state == 1 && i == ((CursorEntry *) slot->link)[1].cursor) {
                    panelDrawNineSlice(D_003EB268, cellX + 2, originY + 50, cellPitch + 12, 40);
                    packetSubmitRequest(D_0069E240[i] * 2 + 215, cellDrawX + (cellW >> 1) - 48, originY + 54, 1);
                } else {
                    packetSubmitRequest(D_0069E240[i] * 2 + 214, cellDrawX + (cellW >> 1) - 48, originY + 54, 1);
                }
            }
        }
    }
    msgRangeDrawRevealed(D_006A5250, 24, (D_0069D529 != 0) ? 348 : 392);
    if ((D_0069D51C & 2) != 0) {
        menuDrawKindIcon(440, 368, 3);
        menuDrawKindIcon(440, 388, 0);
        menuDrawKindIcon(440, 408, 1);
        textDrawSegment(458, 370, menuMainTxdEntry(450), 0);
        textDrawSegment(458, 390, menuMainTxdEntry(451), 0);
        textDrawSegment(458, 410, menuMainTxdEntry(452), 0);
    } else if ((D_0069D51C & 4) != 0) {
        menuDrawKindIcon(440, 388, 3);
        menuDrawKindIcon(440, 408, 1);
        textDrawSegment(458, 390, menuMainTxdEntry(450), 0);
        textDrawSegment(458, 410, menuMainTxdEntry(452), 0);
    }
}

void menuDrawItemList(int id, int unit, void *target);
void menuDrawItems(u8 *table);

/* Draws the item list of the slot's link, then the labels in the table D_006A4D28. */
void menuDrawSlotItems(ControlSlot *slot) {
    menuDrawItemList(0xD, 0x5F, slot->link);
    menuDrawItems(D_006A4D28);
}

void menuDrawMemberItemList();

/* Submits the four layout packets; D_0069D4E0 picks one of two coordinate sets. */
void menuLayoutSubmit(ControlSlot *slot) {
    menuDrawMemberItemList(0xD, 0xD4, D_006A5180, &D_0069E238);
    if (D_0069D4E0 != 0) {
        packetSubmitRequest(0x161, 0x11, 0xB7, 1);
        packetSubmitRequest(0x163, 0xAF, 0xB7, 1);
        packetSubmitRequestEx(0x162, 0x21, 0xB7, 0x8E, 0x11, 1);
        packetSubmitRequest(0x139, 0x48, 0xB6, 1);
    } else {
        packetSubmitRequest(0x161, 0x11, 0xB7, 1);
        packetSubmitRequest(0x163, 0x9E, 0xB7, 1);
        packetSubmitRequestEx(0x162, 0x21, 0xB7, 0x7D, 0x11, 1);
        packetSubmitRequest(0x139, 0x40, 0xB6, 1);
    }
    menuDrawItems(D_006A4D28);
}

extern int menuLicenseState;
extern signed char D_0069D519;
void menuDrawClassInfo(int, int);
void menuDrawLicenseBoard(int x, int y, void *groupCursor, void *slotCursor, void *subCursor, int mode, int mask);

void menuDrawLicensePage(ControlSlot *slot) {
    int baseY = D_006A4940.baseY;
    int show;

    if (D_0069D514.word != -1 && (slot->flags & CONTROL_SLOT_FLAG_MARK) == 0) {
        D_003EC662[0] = D_0069D514.bytes[0];
    } else if (D_0069D4E0 != 0) {
        D_0069D514.word = D_003EC662[0];
    }

    show = 0;
    if (D_0069D518 != 0) {
        if ((slot->flags & 4) != 0) {
            if (((u8) D_0069D519 & 4) != 0) {
                show = 1;
            }
        } else if (((u8) D_0069D519 & 4) == 0) {
            show = 1;
        }
    } else {
        show = 1;
    }
    if (show) {
        menuDrawClassInfo(17, baseY + 332);
    }

    show = 0;
    if (D_0069D518 != 0) {
        if ((slot->flags & 4) != 0) {
            if (((u8) D_0069D519 & 2) != 0) {
                show = 1;
            }
        } else if (((u8) D_0069D519 & 2) == 0) {
            show = 1;
        }
    } else {
        show = 1;
    }
    if (show) {
        menuDrawLicenseBoard(254, baseY + 95, D_006A51E0, &D_006A51E0[5], &D_006A51E0[10], menuLicenseState - 4, D_0069D508);
    }

    if (D_0069D514.word != -1 && (slot->flags & CONTROL_SLOT_FLAG_MARK) == 0) {
        D_003EC662[0] = D_006A4940.flag3CC;
    }
}

struct MenuActor {
    unsigned char unmodeled_00[14];
    unsigned char mode;
};

void menuDrawSpecialAttack(int x, int y, void *menu, int word, unsigned short hp);
void menuDrawCopilotInfo(int x, int y, void *menu, int word, unsigned short hp);
void menuDrawPartnerInfo(int x, int y, void *menu, int word, unsigned short hp);
void menuDrawAttackCommand(int x, int y, void *menu, int word);
void menuDrawCommandInfo(int x, int y, void *menu, int word);
void menuDrawCommandDetail(int x, int y, void *menu, int word);

/* Draws the attack or command panel of the actor and the matching info panel. */
void menuDrawCommandPage(struct MenuActor *actor) {
    int noSavedCursor;

    if (actor->mode == 5) {
        if (D_0069D4E0 != 0) {
            noSavedCursor = -1;
            if (menuSavedCursor != noSavedCursor) {
                D_003EC662[0] = (u8) menuSavedCursor;
            }
            menuDrawSpecialAttack(13, 183, D_006A5198, ptGetMenuMember()->kind, D_0069D50C);
            menuDrawCopilotInfo(275, 95, D_006A5198, ptGetMenuMember()->kind, D_0069D50C);
            if (menuSavedCursor != noSavedCursor) {
                D_003EC662[0] = D_006A4D0C[0];
            }
        } else {
            menuDrawSpecialAttack(13, 183, D_006A5198, ptGetMenuMember()->kind, D_0069D50C);
            menuDrawPartnerInfo(275, 95, D_006A5198, ptGetMenuMember()->kind, D_0069D50C);
            return;
        }
    } else {
        if (D_0069D4E0 != 0) {
            noSavedCursor = -1;
            if (menuSavedCursor != noSavedCursor) {
                D_003EC662[0] = (u8) menuSavedCursor;
            }
            menuDrawAttackCommand(13, 183, D_006A5198, ptGetMenuMember()->kind);
            menuDrawCommandDetail(275, 95, D_006A5198, ptGetMenuMember()->kind);
            if (menuSavedCursor != noSavedCursor) {
                D_003EC662[0] = D_006A4D0C[0];
            }
        } else {
            menuDrawAttackCommand(13, 183, D_006A5198, ptGetMenuMember()->kind);
            menuDrawCommandInfo(275, 95, D_006A5198, ptGetMenuMember()->kind);
            return;
        }
    }
}

/* A selection cursor: a list offset at 0x00, a visibility word at 0x08 and a status word at 0x10. */
typedef struct SelectionCursor {
    int value;                  /* 0x00: index into the selection list */
    int unmodeled_04;
    int visible;                /* 0x08 */
    int unmodeled_0C;
    int status;                 /* 0x10: bit 0 gates the selection */
} SelectionCursor;

extern u8 D_003EC159[];
int selectionFindIndex(void *list, int key);
void menuDrawCharStats(int x, int y, void *rec, int icon, int mark);
void menuDrawEquipPanel(int x, int y, void *rec, void *extra, int flag);
void menuDrawItemCategory(int x, int y, void *a, void *b, int c);
int menuDrawPlainItemList(int a, int b, void *c);
int menuDrawItemHighlighted(void *p, int n);

void menuDrawEquipPage(ControlSlot *slot) {
    int listEntry = 0;
    int highlight;
    struct ControlContext *ctx;
    SelectionCursor *cursor;
    void *text;
    int x;
    int y;
    int style;
    int lineY;

    if (D_0069D4E0 != 0) {
        if (menuSavedCursor != -1) {
            ptState.cursor = (u8) menuSavedCursor;
        }
        if (D_006A4928[0].cursor == 0 && (u8) ((u8) slot->state - 4) < 2) {
            ctx = ptGetMenuMember();
            if (ctx->buffer->count < D_0069D504) {
                ctx = ptGetMenuMember();
                highlight = ctx->buffer->count;
            } else {
                highlight = D_006A51B0[0].cursor;
            }
            cursor = (SelectionCursor *) D_006A51C8;
            if (cursor->visible == 0) {
                highlight = -1;
            } else if (((cursor->status | ((SelectionCursor *) D_006A51B0)->status) & 1) == 0) {
                highlight = -1;
            } else {
                ctx = ptGetMenuMember();
                if (selectionFindIndex(ctx, selectionPlainList[cursor->value]) == -1) {
                    listEntry = selectionPlainList[cursor->value];
                } else {
                    highlight = -1;
                }
            }
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xC5, 0x63, ctx, listEntry, highlight);
        } else if (D_006A4928[0].cursor == 0 && (u8) slot->state == 6) {
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xC5, 0x63, ctx, 0, D_006A51B0[0].cursor);
        } else {
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xC5, 0x63, ctx, 0, -1);
        }
        menuDrawPlainItemList(0x124, 0x5F, D_006A51C8);
        ctx = ptGetMenuMember();
        menuDrawEquipPanel(0x7C, 0xF7, D_006A51B0, ctx->buffer->items, D_0069D504);
        menuDrawItemHighlighted(&D_006A4940, ptState.cursor + 3);
        menuDrawItemHighlighted(&D_006A4940, ptState.cursor + 6);
        if (menuSavedCursor != -1) {
            ptState.cursor = D_006A4940.flag3CC;
        }
    } else {
        if (D_006A4928[0].cursor == 0 && (u8) ((u8) slot->state - 4) < 2) {
            ctx = ptGetMenuMember();
            if (ctx->buffer->count < D_0069D504) {
                ctx = ptGetMenuMember();
                highlight = ctx->buffer->count;
            } else {
                highlight = D_006A51B0[0].cursor;
            }
            cursor = (SelectionCursor *) D_006A51C8;
            if (cursor->visible != 0 && ((cursor->status | ((SelectionCursor *) D_006A51B0)->status) & 1) != 0 &&
                selectionFindIndex(ptGetMenuMember(), D_006A52E0[cursor->value]) == -1) {
                listEntry = D_003EC159[D_006A52E0[cursor->value] * 6];
            } else {
                highlight = -1;
            }
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xB4, 0x63, ctx, listEntry, highlight);
        } else if (D_006A4928[0].cursor == 0 && (u8) slot->state == 6) {
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xB4, 0x63, ctx, 0, D_006A51B0[0].cursor);
        } else {
            ctx = ptGetMenuMember();
            menuDrawCharStats(0xB4, 0x63, ctx, 0, -1);
        }
        if (D_006A4928[0].cursor != 0) {
            fontObjSetOpacity(0x40);
        }
        ctx = ptGetMenuMember();
        menuDrawEquipPanel(0x6B, 0xF7, D_006A51B0, ctx->buffer->items, D_0069D504);
        fontObjResetOpacity();
        menuDrawItemCategory(0x113, 0x5F, D_006A51C8, D_006A52E0, D_0069D508);
    }

    if (D_006A4928[0].cursor == 0 && (u8) slot->state != 1) {
        fontTrackSetTarget(D_006A5130, -0x10, (D_0069D504 << 4) + D_0069D4E0 * 12 + 0x104);
    } else {
        fontTrackSetTarget(D_006A5130, -0x110, (D_0069D504 << 4) + D_0069D4E0 * 12 + 0x104);
    }
    x = D_006A5130[0].current_pair.x;
    y = D_006A5130[0].current_pair.y;
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
    if (ctx->buffer->count == 0 || (u8) slot->state == 5) {
        style = 2;
    } else {
        style = 0;
    }
    textDrawSegment(x + 0x2A, lineY, text, style);
}

void func_00214C98(void) {

}

struct ControlRecord {
    unsigned char unmodeled_00[8];
    struct ItemList *value;     /* 0x08: the item list of the record */
};

int menuDrawItemListDetail(int, int, void *);

/* Draws the detail panel of the record's value through menuDrawItemListDetail. */
int menuRecordItemDetail(struct ControlRecord *record) {
    return menuDrawItemListDetail(0x1F, 0x5F, record->value);
}

void menuDrawControllerConfig(void);
void menuDrawConfig(int, int, void *, unsigned char *);

/* Draws the controller page for a slot whose state is 7, otherwise the general config page. */
void menuDrawConfigPage(ControlSlot *slot) {
    if ((unsigned char) slot->state != 7) {
        menuDrawConfig(121, 100, slot->link, D_003EB040);
    } else {
        do {
            menuDrawControllerConfig();
        } while (0);
    }
}

/* Draws the item list panel with one row per item. Row i's track lane (i & 1) takes the highlight target, 128 on the selected item and 0 otherwise, and the packet scale is that lane's current value. */
void menuMapListDraw(struct ItemList *list);

void menuMapDraw(struct ControlRecord *record) {
    do {
        menuMapListDraw(record->value);
    } while (0);
}

extern u8 D_0069D548[];
void menuDrawPlayTime(int x, int y);
void textDrawGlyphString(int x, int y, void *text, u64 color);
void menuDrawBorder(int x, int y, int w, int h, u64 color);
void menuDrawLitIcon(int x, int y, u8 code);

/* Draws the scene's font track, panels and text, then the item list when the state is 5. */
void menuDrawExtraPage(void) {
    int rosterCount;
    int i;
    int x;
    int y;
    int step;
    KeyedObject *member;

    menuDrawItems(D_006A4940.unmodeled_00);
    if (D_006A4940.mode == 0 || D_006A4940.mode == 5) {
        fontTrackSetTarget(&D_006A5110, 0x118, 0);
    } else {
        fontTrackSetTarget(&D_006A5110, 0x200, 0xFF);
    }
    fontTrackEase(&D_006A5110);
    panelDrawNineSlice(menuPanelFrame, D_006A5110.current_pair.x, 0x149, 0xF0, 0x2B);
    menuDrawTitledFrame(D_006A5110.current_pair.x + 6, 0x14E, 0x6B, 0x21, D_0069D548);
    menuDrawPlayTime(D_006A5110.current_pair.x + 0x75, 0x14E);
    if (D_0069D50E != 0) {
        textDrawGlyphString(D_006A5110.current_pair.x + 0x16, 0x15D, menuMainTxdEntry(D_0069D50E + 0xB),
                            (((u64) 0x3F800000 << 0x10) | 0x8054) << 0x10 | 0x200B);
    }
    if ((unsigned int) (D_006A4940.mode - 5) < 2U) {
        /* the word at 0x14 is scaled_pair.y of the track; the menu reads it as the border step */
        step = D_006A5110.scaled_pair.y;
        menuDrawBorder((-(step * 0x29) >> 0xC) + 0x143, ((step * 4) >> 0xC) + 0x114,
                       ((step * 0x29) >> 0xC) + 0xAE, ((step * 0x31) >> 0xC) + 0x27,
                       (((u64) 0x3F800000 << 0x10) | 0x8038) << 0x10 | 0x1A1A);
        packetSubmitRequest(0x13F, 0x190, ((step * 4) >> 0xC) + 0x104, 1);
        packetSubmitRequest(0x140, 0x1C4, ((step * 4) >> 0xC) + 0x104, 1);
    }
    if (D_006A4940.mode == 5) {
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
                menuDrawLitIcon(y, 0x11B, member->info->code);
                y += 0x18;
            }
        }
    }
}

void grSetFrameBase(void);
void grResetRenderState(void);
void grSetRenderFrame(void);
void grDrawScreenFade(int alpha);

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
extern FontTrack D_006A5360[];
void menuDrawAnimatedBand(int x, int y);
void *menuTutorialTxdEntry(int index);

/* Runs the eleven slot callbacks and the overlays, then the tutorial block when D_0069D518 is set. */
void menuSceneDraw(void) {
    int i;

    for (i = 0; i < 11; i++) {
        menuSlotDraw(i);
    }
    menuSceneDrawOverlays();
    if (D_0069D518 != 0) {
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
            fontTrackEase(D_006A5360);
            menuDrawAnimatedBand(D_006A5360[0].current_pair.x, D_006A5360[0].current_pair.y + D_006A4944[0]);
            if (menuTutorialStage >= 2) {
                textDrawSegment(0, 0, D_0068A3F0, 0);
                textDrawSegment(48, 382, menuTutorialTxdEntry(menuTutorialIndex + 1), 0);
                textDrawSegment(48, 402, menuTutorialTxdEntry(menuTutorialIndex + 2), 0);
            }
        }
        xglFontFlush();
    }
}

struct SelectionRecord {
    unsigned char kind;
    unsigned char dispatch_value;
    unsigned char unmodeled_02[4];
};

extern u8 selectionRecordTable[];

struct SelectionKeys {
    unsigned char unmodeled_00[38];
    unsigned char keys[4];
    unsigned char count;
};

struct SelectionContext {
    int first;
    unsigned char unmodeled_04[8];
    struct SelectionKeys *keys;
};

void battleSlotSetByte(int first, int index, int dispatch_value);

void selectionPlaceKey(struct SelectionContext *context, int key, int count) {
    struct SelectionRecord *record;
    int index;

    index = selectionFindIndex(context, key);
    if (index != -1) {
        context->keys->keys[index] = context->keys->keys[count];
        record = (struct SelectionRecord *) &selectionRecordTable[context->keys->keys[count] * 6];
        battleSlotSetByte(context->first, index, record->dispatch_value);
    }
    context->keys->keys[count] = key;
    record = (struct SelectionRecord *) &selectionRecordTable[key * 6];
    battleSlotSetByte(context->first, count, record->dispatch_value);
}

void func_A8B930(int, int, unsigned char);
void selectionListDropAt(int index);

void selectionTakePlainEntry(struct ControlContext *context, int plainIndex, int row) {
    context->buffer->items[row] = selectionPlainList[plainIndex];
    func_A8B930(context->kind, row, selectionPlainList[plainIndex]);
    selectionListDropAt(plainIndex);
    cursorBufferSetCapacity(D_006A51C8, selectionPlainCount);
}

/* Advances the back or front slot cursor of ptState until the chosen slot is filled, and starts voice 0x01000001 when the cursor moved. */
void ptAdvanceSlotCursor(void) {
    unsigned char previous;

    if (D_0069D4E0 != 0) {
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

void msgBufferAppend(void *buffer);
void countdownInit(void *buffer, int size);
extern u8 D_0069D550[];

/* Loads the two message buffers, starts the voice cue 0x01000003 and sets countdownActive (countdownActive). */
void countdownStart(void) {
    void *firstBuffer;
    void *secondBuffer;

    firstBuffer = menuMainTxdEntry(0x49);
    countdownInit(firstBuffer, 0x12C);
    msgBufferAppend(D_0069D550);
    secondBuffer = menuMainTxdEntry(0x4A);
    msgBufferAppend(secondBuffer);
    seVoiceStartDefault(0x01000003);
    countdownActive = 1;
}

void sprintf(int *target, u8 *source, int value);
extern int D_0069E220;
extern u8 D_0069D558[];

/* Formats value as "%5d" into a shared buffer and returns that buffer. */
int *func_00215598(int value) {
    sprintf(&D_0069E220, D_0069D558, value);
    return &D_0069E220;
}

void func_00211918(ControlLink *link);
void menuSetLayout(u32 mode);
extern unsigned char D_006A53EA[];
void choiceInit(void *buffer, int value);
extern u16 D_003EC0E8[];
extern u16 D_003EC120[];

/* Selects one of the five menu layouts by mode and appends the matching message strings. */
void menuSetLayout(u32 mode) {
    switch (mode) {
    case 0:
        choiceInit((void *) 0, 0);
        countdownInit((void *) 0, 0);
        break;
    case 1: {
        ControlLink *links = (ControlLink *) D_006A51E0;

        choiceInit(menuMainTxdEntry(links[0].kind + 0x36), -1);
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(links[1].kind + 0x3A));
        msgBufferAppend(D_0069D550);
        msgBufferAppend(menuMainTxdEntry(0x42));
        msgBufferAppend(func_00215598(D_003EC0E8[links[1].kind]));
        msgBufferAppend(D_0069D550);
        msgBufferAppend(menuMainTxdEntry(0x44));
        break;
    }
    case 2: {
        ControlLink *links = (ControlLink *) D_006A51E0;

        choiceInit(selectionSetValue(links[0].kind, links[1].kind, links[2].kind, 0), -1);
        msgBufferAppend(D_0069D550);
        msgBufferAppend(menuMainTxdEntry(0x43));
        msgBufferAppend(func_00215598(selectionGetValue(links[0].kind, links[1].kind, links[2].kind)));
        msgBufferAppend(D_0069D550);
        msgBufferAppend(menuMainTxdEntry(0x44));
        break;
    }
    case 3: {
        ControlLink *links = (ControlLink *) D_006A51E0;

        countdownInit(menuMainTxdEntry(links[0].kind + 0x36), -2);
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(links[1].kind + 0x3A));
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(0x47));
        msgBufferAppend(D_0069D550);
        msgBufferAppend(menuMainTxdEntry(0x4B));
        msgBufferAppend(func_00215598(D_003EC120[links[0].kind * 8 + links[1].kind]));
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(0x4C));
        break;
    }
    case 4: {
        ControlLink *links = (ControlLink *) D_006A51E0;

        countdownInit(menuMainTxdEntry(links[0].kind + 0x36), -2);
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(links[1].kind + 0x3A));
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(0x47));
        msgBufferAppend(D_0069D550);
        msgBufferAppend(menuMainTxdEntry(links[0].kind + 0x37));
        msgBufferAppend(" ");
        msgBufferAppend(menuMainTxdEntry(0x48));
        break;
    }
    }
    menuLayoutMode = (signed char) mode;
}

extern u8 D_0069D51B;
/* Scene-change handler (menuState is the menu state id): clears the current slot, refreshes the cursors, then switches on menuState to set the menu mode and the cursor buffers. */
void menuSceneSetState(int menuState);
extern u8 D_0069E248[4];
extern u8 D_0069E258[4];
extern u8 D_0069E25B;
extern u8 D_0069E24C;
extern u8 D_0069E24D;
extern int D_0069E250;
extern int D_0069E254;
extern unsigned char D_006A5380[];

/* Resets the cursor state and link table, saves the kind bytes of links 0 to 2 when menuCurrentSlot is 3, and copies the cursor buffer's header, cells, flags and limits into the saved globals (clearing them in the buffer). */
void menuTutorialSaveState(void) {
    u8 *savedHeader;
    u8 *savedCells;
    int index;

    cursorBufferSetOffset((CursorBuffer *) D_006A4900, 2);
    menuPendingSelection = 0;
    if (menuCurrentSlot == 3) {
        D_0069E258[0] = ((u8 *) D_006A51E0)[0x00];
        D_0069E258[1] = ((u8 *) D_006A51E0)[0x14];
        D_0069E258[2] = ((u8 *) D_006A51E0)[0x28];
        D_0069E25B = menuSlots[3].state;
    } else {
        D_0069E25B = 0;
    }
    index = 0;
    menuSceneSetState(3);
    savedHeader = D_0069E248;
    menuPlaceCursor(0, 0, 0, 0);
    menuSlots[3].state = 4;
    func_00211918((ControlLink *) D_006A51E0);
    cursorBufferSetOffset(D_006A4928, 1);
    menuTutorialIndex = 0;
    D_0069D519 = 0;
    menuSetLayout(0);
    D_0069D51B = 0;
    menuTutorialStage = 0;
    D_0069D518 = 1;
    menuTutorialLine = 0;
    D_0069D51C = 0;
    do {
        *savedHeader = ptGetMenuMember()->buffer->header[index];
        ptGetMenuMember()->buffer->header[index] = 0;
        savedHeader += 1;
        index += 1;
    } while (index < 4);
    savedCells = D_006A5380;
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
    D_0069E24C = ptGetMenuMember()->buffer->flags[0];
    ptGetMenuMember()->buffer->flags[0] = 0;
    D_0069E24D = ptGetMenuMember()->buffer->flags[1];
    ptGetMenuMember()->buffer->flags[1] = 0;
    D_0069E250 = ptGetMenuMember()->limits->skillPoints;
    ptGetMenuMember()->limits->skillPoints = 1000;
    D_0069E254 = ptGetMenuMember()->limits->classPoints;
    ptGetMenuMember()->limits->classPoints = 1000;
    D_0069D529 = 1;
    cursorBufferSetOffset((CursorBuffer *) D_006A51E0, 0);
    cursorBufferSetOffset((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 1), 0);
    cursorBufferSetOffset((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 2), 0);
}

extern unsigned char D_0069E25A;


void menuTutorialRestoreState(void) {
    int i;
    seVoiceStartDefault(MENU_SE_CANCEL);
    D_0069D518 = 0;
    menuTutorialStage = 0;
    D_0069D529 = 0;
    D_0069D51C = 0;
    menuPlaceCursor(0, 0, 0, 0);
    for (i = 0; i < 4; i++) {
        ptGetMenuMember()->buffer->header[i] = D_0069E248[i];
    }
    for (i = 0; i < 8; i++) {
        ptGetMenuMember()->buffer->cells[0][i] = D_006A5380[i];
        ptGetMenuMember()->buffer->cells[1][i] = D_006A5380[i + 8];
        ptGetMenuMember()->buffer->cells[2][i] = D_006A5380[i + 16];
        ptGetMenuMember()->buffer->cells[3][i] = D_006A5380[i + 24];
    }
    ptGetMenuMember()->buffer->flags[0] = D_0069E24C;
    ptGetMenuMember()->buffer->flags[1] = D_0069E24D;
    ptGetMenuMember()->limits->skillPoints = D_0069E250;
    ptGetMenuMember()->limits->classPoints = D_0069E254;
    if (D_0069E25B != 0) {
        CursorEntry *records = (CursorEntry *) D_006A51E0;

        records[0].cursor = D_0069E258[0];
        records[1].cursor = D_0069E258[1];
        if (D_0069E258[0] == 3) {
            records[1].capacity = 4;
        } else {
            records[1].capacity = 8;
        }
        {
            CursorEntry *tail = (CursorEntry *) D_006A51E0;

            tail[2].cursor = D_0069E25A;
        }
        menuSlots[3].state = 7;
        if ((unsigned char) (D_0069E25B - 4) < 3) {
            menuLicenseState = D_0069E25B;
        }
        cursorBufferSetOffset(D_006A4928, 1);
        func_00211918((ControlLink *) menuSlots[3].link + (menuSlots[3].state - 4));
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
            if ((D_0069D51C & 1) == 0) {
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
    D_0069D519 = 0;
    menuSetLayout(0);
    menuTutorialStage = 0;
    D_0069D51C = 0;
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
    ptGetMenuMember()->buffer->flags[0] = 0;
    ptGetMenuMember()->buffer->flags[1] = 0;
    ptGetMenuMember()->limits->skillPoints = 1000;
    ptGetMenuMember()->limits->classPoints = 1000;
    cursorBufferSetOffset((CursorBuffer *) D_006A51E0, 0);
    cursorBufferSetOffset((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 1), 0);
    cursorBufferSetOffset((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 2), 0);
    D_006A53EA[0] = 4;
    func_00211918((ControlLink *) D_006A51E0);
}

extern u8 menuTutorialTimer;
extern u16 D_0063EDB4[];
extern u8 D_006A51F4[];
extern u8 D_006A5208[];
int msgRangeIsRevealed(void *range);
void selectionHolderCharge(struct ControlContext *ctx, int a, int b);
int selectionTakeSub(struct ControlContext *ctx, int a, int b, int c);
long padCirclePressedAlone(void);
int padCrossPressedAlone(void);

/* One step of the menu tutorial script at menuTutorialIndex: a "%" code runs a command (%e ends the tutorial, %r restarts it, %k/%p/%P/%w/%s wait for a press or a revealed line, %c, %f, %l, %a, %g, %m, %t, %j act on the cursor, font track, toggles, timers and jumps), "*" and 0 skip, other bytes are shown text. */
void menuTutorialStep(void) {
    u8 *text;
    int arg;
    int count;
    int trackX;
    int trackY;
    int index;
    ControlLink *links;
    struct ControlContext *ctx;

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
            D_0069D51B += 1;
            break;
        case 'k':
            if (padCirclePressedAlone()) {
                seVoiceStartDefault(MENU_SE_CONFIRM);
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
                D_0069D51C &= 0xFC;
            } else if (padCrossPressedAlone()) {
                menuTutorialRewind();
                if (D_0069D518 != 0) {
                    seVoiceStartDefault(MENU_SE_CANCEL);
                }
                D_0069D51C &= 0xFD;
            } else {
                D_0069D51C |= 2;
            }
            break;
        case 'w':
            if (msgRangeIsRevealed(D_006A5250)) {
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
            }
            break;
        case 'p':
            if (msgRangeIsRevealed(D_006A5250)) {
                if (padCirclePressedAlone()) {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    index = menuTutorialIndex + 1;
                    menuTutorialIndex = index;
                    D_0069D51C &= 0xFC;
                } else if (padCrossPressedAlone()) {
                    menuTutorialRewind();
                    if (D_0069D518 != 0) {
                        seVoiceStartDefault(MENU_SE_CANCEL);
                    }
                    D_0069D51C &= 0xFD;
                } else {
                    D_0069D51C |= 2;
                }
            }
            break;
        case 'P':
            if (msgRangeIsRevealed(D_006A5250)) {
                if (padCirclePressedAlone()) {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    index = menuTutorialIndex + 1;
                    menuTutorialIndex = index;
                    D_0069D51C &= 0xFA;
                } else {
                    D_0069D51C |= 4;
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
                if (D_0069D4CC != 0) {
                    cursorBufferMoveBy((CursorBuffer *) D_0069D4CC, 1);
                }
                break;
            case '-':
                if (D_0069D4CC != 0) {
                    cursorBufferMoveBy((CursorBuffer *) D_0069D4CC, -1);
                }
                break;
            default:
                if (D_0069D4CC != 0) {
                    cursorBufferSetOffset((CursorBuffer *) D_0069D4CC, arg - '0');
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
                func_00211918((ControlLink *) D_006A51E0);
                D_006A53EA[0] = 4;
                break;
            case '1':
                if (((ControlLink *) D_006A51E0)[0].kind == 3) {
                    cursorBufferSetCapacity((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 1), 4);
                } else {
                    cursorBufferSetCapacity((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 1), 8);
                }
                func_00211918((ControlLink *) D_006A51F4);
                D_006A53EA[0] = 5;
                seVoiceStartDefault(MENU_SE_CURSOR);
                break;
            case '2':
                seVoiceStartDefault(MENU_SE_CURSOR);
                func_00211918((ControlLink *) D_006A5208);
                D_006A53EA[0] = 6;
                break;
            }
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            break;
        case 'l':
            arg = text[2];
            switch (arg) {
            case '0':
                D_0069D519 ^= 1;
                break;
            case '1':
                D_0069D519 ^= 2;
                break;
            case '2':
                D_0069D519 ^= 4;
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
                fontTrackSetTarget(D_006A5360, trackX, trackY);
            } else {
                fontTrackSetBoth(D_006A5360, trackX, trackY);
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
            if (D_0069D51B >= 2) {
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
            }
            index = menuTutorialIndex + 1;
            menuTutorialIndex = index;
            menuTutorialIndex = index;
            break;
        case 's':
            if (msgRangeIsRevealed(D_006A5250)) {
                if (menuTutorialStage < 2) {
                    fontTrackSetBoth(D_006A5360, 0x1C, 0x1B0);
                    menuTutorialStage = 2;
                }
                if (D_0063EDB4[0] & (PAD_UP | PAD_DOWN)) {
                    seVoiceStartDefault(MENU_SE_CURSOR);
                    menuTutorialStage = ((menuTutorialStage - 2) ^ 1) + 2;
                    fontTrackSetTarget(D_006A5360, 0x1C, menuTutorialStage * 20 + 0x188);
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
                switch (D_006A53EA[0]) {
                case 5:
                    ctx = ptGetMenuMember();
                    selectionHolderCharge(ctx, ((ControlLink *) D_006A51E0)[0].kind, ((ControlLink *) D_006A51E0)[1].kind);
                    break;
                case 6:
                    ctx = ptGetMenuMember();
                    links = (ControlLink *) D_006A51E0;
                    if (selectionTakeSub(ctx, links[0].kind, links[1].kind, links[2].kind)) {
                        ctx = func_0022A560();
                        ctx->limits->limit_b += D_003EC120[links[0].kind * 8 + links[1].kind];
                    }
                    break;
                }
                index = menuTutorialIndex + 1;
                menuTutorialIndex = index;
            } else {
                menuTutorialTimer += 1;
                if (menuTutorialTimer == 1) {
                    switch (D_006A53EA[0]) {
                    case 5:
                        menuSetLayout(1);
                        break;
                    case 6:
                        menuSetLayout(2);
                        break;
                    }
                    fontTrackSetBoth(D_006A5360, 280, 296);
                    menuTutorialStage = 1;
                } else if (menuTutorialTimer == 21) {
                    fontTrackSetTarget(D_006A5360, 160, 296);
                } else if (menuTutorialTimer == 41) {
                    menuSetLayout(0);
                    menuTutorialStage = 0;
                    switch (D_006A53EA[0]) {
                    case 5:
                        ctx = ptGetMenuMember();
                        selectionHolderCharge(ctx, ((ControlLink *) D_006A51E0)[0].kind, ((ControlLink *) D_006A51E0)[1].kind);
                        break;
                    case 6:
                        ctx = ptGetMenuMember();
                        links = (ControlLink *) D_006A51E0;
                        if (selectionTakeSub(ctx, links[0].kind, links[1].kind, links[2].kind)) {
                            menuSetLayout(3);
                            ctx = ptGetMenuMember();
                            ctx->limits->classPoints += D_003EC120[links[0].kind * 8 + links[1].kind];
                        }
                        break;
                    }
                    if (menuLayoutMode == 0) {
                        menuTutorialTimer = 0;
                        index = menuTutorialIndex + 1;
                        menuTutorialIndex = index;
                    }
                } else if (menuTutorialTimer == 101) {
                    menuSetLayout(4);
                } else if (menuTutorialTimer == 161) {
                    menuSetLayout(0);
                    menuTutorialTimer = 0;
                    index = menuTutorialIndex + 1;
                    menuTutorialIndex = index;
                }
            }
            break;
        case 'm':
            menuSetLayout(text[2] - '0');
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
        D_0069D51C |= 1;
        break;
    default:
        menuTutorialLine = text;
        index = menuTutorialIndex + 1;
        menuTutorialIndex = index;
        break;
    }
    if (D_0069D518 != 0) {
        if ((u8) ((u8) menuSlots[3].state - 4) < 3) {
            menuLicenseState = (u8) menuSlots[3].state;
        }
    }
}

extern int D_006A4914[];
void choiceSetRow(int value);
int choiceGetResult(void);

int menuMainCommandStep(ControlSlot *slot) {
    int activeState;
    int result;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    if (D_0069D502) {
        if (xglCdServiceIfPending() == 0) {
            if (*(int *) D_006A5240 != 0) {
                menuSceneSetState(10);
            }
            D_0069D502 = 0;
        }
        return 0;
    }
    if ((PadData[0].repeated & (PAD_LEFT | PAD_L2)) != 0) {
        cursorBufferMoveBy((CursorBuffer *) ((CursorEntry *) slot->link + slot->state), -1);
    }
    if ((PadData[0].repeated & (PAD_RIGHT | PAD_R2)) != 0) {
        cursorBufferMoveBy((CursorBuffer *) ((CursorEntry *) slot->link + slot->state), 1);
    }
    if ((PadData[0].pressed & PAD_START) != 0) {
        ptAdvanceSlotCursor();
    }
    result = padCrossPressedAlone() ? 1 : 0;
    activeState = 1;
    if (slot->state == 0) {
        if (padCirclePressedAlone()) {
            menuSceneSetState(((CursorEntry *) slot->link)->cursor + 1);
        } else if (PadData[0].pressed & PAD_DOWN) {
            CursorEntry *links = (CursorEntry *) slot->link;

            cursorBufferSetOffset((CursorBuffer *) (links + 1), ((u32) (links[0].cursor * links[1].capacity)) / 6);
            slot->state = activeState;
            seVoiceStartDefault(MENU_SE_CURSOR);
        }
    } else if (D_0069D4E4) {
        if (!choiceGetResult()) {
            result = activeState;
            D_0069D501 = 2;
        } else {
            D_0069D4E4 = 0;
        }
    } else if (padCirclePressedAlone() != 0 && xglCdServiceIfPending() == 0) {
        switch (D_0069E240[D_006A4914[0]]) {
        case 0:
            if (*(int *) D_006A5240) {
                if (ptState.progress->key != -1) {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    choiceInit(menuMainTxdEntry(0x1BE), 1);
                    choiceSetRow(0xF0);
                    D_0069D4E4 = activeState;
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
    } else if ((PadData[0].pressed & PAD_UP) != 0) {
        CursorEntry *links = (CursorEntry *) slot->link;

        if (links[1].capacity) {
            u32 scaled = (u32) (6 * links[1].cursor);
            u32 offset = scaled / links[1].capacity;
            u32 last = (6 + scaled) / links[1].capacity - 1;
            if (links[0].cursor < (int) offset || links[0].cursor > (int) last) {
                cursorBufferSetOffset((CursorBuffer *) links, offset);
            }
        }
        slot->state = 0;
        seVoiceStartDefault(MENU_SE_CURSOR);
    }
    return result;
}
void menuMoveCursor(void *object, int delta);

/* One 0x40-byte entry of the selection table in D_006A4D28; key is its first word. */
typedef struct SelectionEntry {
    int key;                    /* 0x00 */
    u8 unmodeled_04[0x3C];
} SelectionEntry;

/* The selection block at D_006A4D28: entries from 0x8C, the current index at 0x3CC, flags at 0x3E4. */
typedef struct SelectionBlock {
    u8 unmodeled_00[0x8C];
    SelectionEntry entries[13];
    int current;                /* 0x3CC */
    u8 unmodeled_3D0[0x14];
    int flags3E4;               /* 0x3E4 */
} SelectionBlock;

int selectionEntryIsEligible(int key);
u16 selectionWindowEntryMarkedFlags(int index);
int selectionWindowEntryHasFlag4(int index);
int selectionWindowListDropAt(int key);
SelectionEntry *func_002245B8(SelectionEntry *entry);
int selectionCountedMemberNeedsHpEp(void);
int selectionEntryIsUseful(int key, SelectionEntry *entry);
int func_00A8B658(int key, int value);
int func_00A8F3C0(int value);
int func_00A8F478(int value);

int menuItemListStep(ControlSlot *slot) {
    SelectionEntry *entry;
    SelectionBlock *sel;
    int i;

    if (!(slot->flags & 1)) {
        return 0;
    }
    switch (slot->state) {
    case 0:
        if (!D_0069D4ED) {
            goto resetState1;
        }
    case 7:
        slot->state = 4;
        break;
    case 1:
        if (PadData[0].repeated & PAD_LEFT) {
            cursorBufferMoveBy(D_006A4928, -1);
        }
        if (PadData[0].repeated & PAD_RIGHT) {
            cursorBufferMoveBy(D_006A4928, 1);
        }
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_BACK);
            menuFocusRootSlot();
            break;
        }
        if (!padCirclePressedAlone()) {
            break;
        }
        switch (D_006A4928[0].cursor) {
        case 0:
            seVoiceStartDefault(MENU_SE_CONFIRM);
            slot->state = 4;
            func_00211918(slot->link);
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
            func_00211918(0);
            slot->state = 1;
            return 0;
        }
        if (padCirclePressedAlone() != 0 && selectionWindowCount != 0) {
            if (!selectionEntryIsEligible(((CursorBuffer *) slot->link)->cursor)) {
                goto soundFail;
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            if (selectionWindowList[((CursorBuffer *) slot->link)->cursor] >= 0x25) {
                signed char map[4] = {0x08, 0x07, 0x10, 0x0C};

                countdownInit(menuMainTxdEntry(selectionWindowList[((CursorBuffer *) slot->link)->cursor] + 0x193), 0x12C);
                countdownActive = 1;
                entry = menuItemMemberEntry(&((SelectionBlock *) D_006A4D28)->entries[((SelectionBlock *) D_006A4D28)->current]);
                func_00A8B658(entry->key, selectionWindowList[((CursorBuffer *) slot->link)->cursor]);
                func_00A8F3C0(map[selectionWindowList[((CursorBuffer *) slot->link)->cursor] - 0x25]);
                if (!D_0069D4E0) {
                    D_0069D50C = 1;
                    for (i = 0; i < 16; i++) {
                        if (func_00A8F478(i + 1) != 0) {
                            D_0069D50C |= 1 << i;
                        }
                    }
                }
                if (selectionWindowListDropAt(((CursorBuffer *) slot->link)->cursor) >= 1) {
                    return 0;
                }
                cursorBufferSetCapacity((CursorBuffer *) slot->link, selectionWindowCount);
                return 0;
            }
            if (selectionWindowEntryMarkedFlags(((CursorBuffer *) slot->link)->cursor) != 0) {
                menuSetValuesMode(D_006A4D28, 1);
            }
            if (selectionWindowEntryHasFlag4(((CursorBuffer *) slot->link)->cursor) != 0) {
                ((SelectionBlock *) D_006A4D28)->flags3E4 |= 4;
            }
            slot->state = 2;
            ((SelectionBlock *) D_006A4D28)->flags3E4 |= 1;
            func_00211918(0);
            return 0;
        }
        if ((PadData[0].doubleword & (((u64) PAD_LEFT << 32) | PAD_UP | PAD_DOWN)) == ((u64) PAD_LEFT << 32)) {
            cursorBufferMoveUnits(slot->link, -1);
        }
        if ((PadData[0].doubleword & (((u64) PAD_RIGHT << 32) | PAD_UP | PAD_DOWN)) == ((u64) PAD_RIGHT << 32)) {
            cursorBufferMoveUnits(slot->link, 1);
        }
        return 0;
    case 2:
        if (padCrossPressedAlone() != 0) {
            SelectionBlock *menu = (SelectionBlock *) D_006A4D28;

            menuSetValuesMode(menu, 0);
            seVoiceStartDefault(MENU_SE_CANCEL);
            menu->flags3E4 &= ~1;
            slot->state = 4;
            func_00211918(slot->link);
            menu->flags3E4 &= ~4;
            break;
        }
        if (padCirclePressedAlone()) {
            if (selectionWindowEntryHasFlag4(((CursorBuffer *) slot->link)->cursor)) {
                if (!selectionCountedMemberNeedsHpEp()) {
                    goto soundFail;
                }
                seVoiceStartDefault(MENU_SE_CONFIRM);
                entry = menuItemMemberEntry(&((SelectionBlock *) D_006A4D28)->entries[((SelectionBlock *) D_006A4D28)->current]);
                func_00A8B658(entry->key, selectionWindowList[((CursorBuffer *) slot->link)->cursor]);
                if (selectionWindowListDropAt(((CursorBuffer *) slot->link)->cursor) > 0) {
                    return 0;
                }
                cursorBufferSetCapacity((CursorBuffer *) slot->link, selectionWindowCount);
                sel = (SelectionBlock *) D_006A4D28;
                sel->flags3E4 &= ~1;
                slot->state = 4;
                func_00211918(slot->link);
                sel->flags3E4 &= ~4;
                break;
            }
            entry = func_002245B8(&((SelectionBlock *) D_006A4D28)->entries[((SelectionBlock *) D_006A4D28)->current]);
            if (!selectionEntryIsUseful(((CursorBuffer *) slot->link)->cursor, entry)) {
                goto soundFail;
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            entry = menuItemMemberEntry(&((SelectionBlock *) D_006A4D28)->entries[((SelectionBlock *) D_006A4D28)->current]);
            func_00A8B658(entry->key, selectionWindowList[((CursorBuffer *) slot->link)->cursor]);
            if (selectionWindowListDropAt(((CursorBuffer *) slot->link)->cursor) > 0) {
                return 0;
            }
            cursorBufferSetCapacity((CursorBuffer *) slot->link, selectionWindowCount);
            ((SelectionBlock *) D_006A4D28)->flags3E4 &= ~1;
            slot->state = 4;
            func_00211918(slot->link);
            break;
        soundFail:
            seVoiceStartDefault(MENU_SE_REFUSE);
            break;
        }
        if (PadData[0].repeated & PAD_DOWN) {
            menuMoveCursor(D_006A4D28, 1);
        } else if (PadData[0].repeated & PAD_UP) {
            menuMoveCursor(D_006A4D28, -1);
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

/* PROBE ONLY: partial view of the MenuObject at D_006A4D28 (cursor 0x3CC, flags 0x3E4). */
typedef struct ProbeMenuObject {
    unsigned char unmodeled_00[0x3CC];
    int cursor;
    unsigned char unmodeled_3D0[0x14];
    int flags;
} ProbeMenuObject;

typedef struct ProbePartyRecord {
    unsigned char unmodeled_00[2];
    long gauge : 32;
    unsigned char unmodeled_06[0x56];
    int value5C;
    short value60;
} ProbePartyRecord;

/* Clears the mark bit of the current slot, then sets menuPendingSelection to -1 before the tail call. */
void menuCancelToMainSlot(void);
int selectionActiveMemberNeedsHp(void);
void func_A8B4F8(int id, int value, unsigned char flags);
int func_A8B4B0(int id, unsigned char flags);
ProbePartyRecord *func_A8B490(int id);
ProbePartyRecord *func_0022A5A0(void);

int menuLayoutPlaceStep(ControlSlot *slot) {
    unsigned char state;
    struct ControlContext *ctx;
    struct ControlContext *ctx2;
    struct ControlContext *ctx3;
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
            if (D_0069D4ED) {
                menuFocusRootSlot();
            } else {
                menuCancelToMainSlot();
            }
        } else if (padCirclePressedAlone()) {
            if (D_0069E238[((ControlLink *) slot->link)->kind] != 0) {
                struct ControlContext *ctxS4;
                int limit;
                int index = ((ControlLink *) slot->link)->kind;
                limit = (int) func_0022A5A0()->value60;
                ctxS4 = ptGetMenuMember();
                if (func_A8B4B0(ctxS4->kind, D_0069E238[((ControlLink *) slot->link)->kind]) > limit) {
                    seVoiceStartDefault(MENU_SE_REFUSE);
                } else {
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    D_0069D4DC &= ~1;
                    slot->state = 2;
                    ((ProbeMenuObject *) D_006A4D28)->flags = ((ProbeMenuObject *) D_006A4D28)->flags | 1;
                    func_00211918(0);
                    if (D_0069E238[((ControlLink *) slot->link)->kind] == 3) {
                        ((ProbeMenuObject *) D_006A4D28)->flags = ((ProbeMenuObject *) D_006A4D28)->flags | 4;
                    }
                }
            }
        } else if (D_0069D4ED != 0 && (D_0063EDB4[0] & (PAD_LEFT | PAD_RIGHT))) {
            if (D_0063EDB4[0] & PAD_LEFT) {
                menuMoveCursor(&D_006A4940, -1);
            } else {
                menuMoveCursor(&D_006A4940, 1);
            }
            menuRebuildCursorBuffers();
        }
        break;
    case 2:
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            D_0069D4DC = D_0069D4DC | 1;
            ((ProbeMenuObject *) D_006A4D28)->flags = ((ProbeMenuObject *) D_006A4D28)->flags & ~1;
            slot->state = 4;
            func_00211918(slot->link);
            ((ProbeMenuObject *) D_006A4D28)->flags = ((ProbeMenuObject *) D_006A4D28)->flags & ~4;
        } else if (padCirclePressedAlone()) {
            ProbeMenuObject *menu;
            int id20;
            ProbePartyRecord *rec16;
            ProbePartyRecord *rec2;
            ptState.cursor = ((ProbeMenuObject *) D_006A4D28)->cursor;
            ctx = ptGetMenuMember();
            id20 = ctx->kind;
            ptState.cursor = D_006A4D0C[0];
            rec16 = func_A8B490(ctx->kind);
            rec2 = func_A8B490(ctx->kind);
            menu = (ProbeMenuObject *) D_006A4D28;
            if (rec16->value5C != rec2->gauge
                || (D_0069E238[((ControlLink *) slot->link)->kind] == 3 && selectionActiveMemberNeedsHp() != 0)) {
                seVoiceStartDefault(MENU_SE_CONFIRM);
                ctx = ptGetMenuMember();
                func_A8B4F8(ctx->kind, id20, D_0069E238[((ControlLink *) slot->link)->kind]);
                t16 = (short) func_0022A5A0()->value60;
                ctx = ptGetMenuMember();
                if (!(t16 >= func_A8B4B0(ctx->kind, D_0069E238[((ControlLink *) slot->link)->kind]))) {
                    D_0069D4DC = D_0069D4DC | 1;
                    menu->flags = menu->flags & ~1;
                    slot->state = 4;
                    func_00211918(slot->link);
                    menu->flags = menu->flags & ~4;
                }
            } else {
                seVoiceStartDefault(MENU_SE_REFUSE);
            }
        } else {
            unsigned short pad = D_0063EDB4[0];
            if ((pad & PAD_DOWN) != 0) {
                menuMoveCursor(D_006A4D28, 1);
            } else if ((pad & PAD_UP) != 0) {
                menuMoveCursor(D_006A4D28, -1);
            }
        }
        break;
    }
    return 0;
}

int selectionCanOpenSlot(struct ControlContext *ctx, int a, int b);
int selectionCanTakeSub(struct ControlContext *ctx, int a, int b, int c);
int selectionApplyEntry(struct ControlContext *ctx, int a, int b, int c);
extern u64 D_0063EDB0[];

/* PadPort: folded into PadDirections. */
/* The menu-slot state machine: idle, cursor, confirm and selection states, one step per call. */
int menuLicenseChargeStep(ControlSlot *slot) {
    unsigned char state;
    struct ControlContext *ctx;
    struct ControlLimits *limits;
    unsigned char flagsBefore;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    if (slot->state == 0) {
        if (D_0069D4ED != 0) {
            slot->state = 4;
        } else {
            func_00211918(0);
            slot->state = 1;
        }
        menuLicenseState = 4;
        return 0;
    }
    if (slot->state == 7) {
        slot->state = 1;
        return 0;
    }
    if (D_0069D4E4 == 0 && padCrossPressedAlone() != 0) {
        if (slot->state == 1) {
            seVoiceStartDefault(MENU_SE_BACK);
            if (D_0069D4ED != 0) {
                menuFocusRootSlot();
            } else {
                menuCancelToMainSlot();
            }
        } else {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuLicenseState = slot->state;
            slot->state = 1;
            func_00211918(0);
        }
        return 0;
    }
    switch (slot->state) {
    case 1:
        if (PadData[0].repeated & PAD_LEFT) {
            cursorBufferMoveBy(D_006A4928, -1);
        }
        if (PadData[0].repeated & PAD_RIGHT) {
            cursorBufferMoveBy(D_006A4928, 1);
        }
        if (padCirclePressedAlone() != 0) {
            switch (D_006A4928[0].cursor) {
            case 0:
                seVoiceStartDefault(MENU_SE_CONFIRM);
                slot->state = menuLicenseState;
                func_00211918((ControlLink *) slot->link + (menuLicenseState - 4));
                break;
            case 1:
                slot->state = menuLicenseState;
                func_00211918((ControlLink *) slot->link + (menuLicenseState - 4));
                menuTutorialSaveState();
                break;
            }
        }
        break;
    case 4:
        if ((D_0063EDB0[0] & 0x200000005000ULL) == 0x200000000000ULL) {
            if (D_006A51E0[0] == 3) {
                cursorBufferSetCapacity((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 1), 4);
            } else {
                cursorBufferSetCapacity((CursorBuffer *) ((CursorEntry *) D_006A51E0 + 1), 8);
            }
            func_00211918((ControlLink *) D_006A51F4);
            slot->state = 5;
            seVoiceStartDefault(MENU_SE_CURSOR);
        }
        break;
    case 5:
        if (D_0069D4E4 != 0) {
            if (choiceGetResult() == 0) {
                selectionHolderCharge(ptGetMenuMember(), D_006A51E0[0], D_006A51E0[5]);
            }
            D_0069D4E4 = 0;
        } else if ((D_0063EDB0[0] & 0x200000005000ULL) == 0x200000000000ULL) {
            seVoiceStartDefault(MENU_SE_CURSOR);
            func_00211918((ControlLink *) D_006A5208);
            slot->state = 6;
        } else if ((D_0063EDB0[0] & 0x800000005000ULL) == 0x800000000000ULL) {
            seVoiceStartDefault(MENU_SE_CURSOR);
            func_00211918((ControlLink *) D_006A51E0);
            slot->state = 4;
        } else if (padCirclePressedAlone() != 0) {
            int found = selectionCanOpenSlot(func_0022A560(), D_006A51E0[0], D_006A51E0[5]);
            if (found != 0) {
                seVoiceStartDefault(MENU_SE_CONFIRM);
                choiceInit(menuMainTxdEntry(D_006A51E0[0] + 0x36), 1);
                msgBufferAppend(" ");
                msgBufferAppend(menuMainTxdEntry(D_006A51E0[5] + 0x3A));
                msgBufferAppend(D_0069D550);
                msgBufferAppend(menuMainTxdEntry(0x42));
                msgBufferAppend(func_00215598(D_003EC0E8[D_006A51E0[0] * 8 + D_006A51E0[5]]));
                msgBufferAppend(D_0069D550);
                msgBufferAppend(menuMainTxdEntry(0x44));
                D_0069D4E4 = 1;
            } else {
                seVoiceStartDefault(MENU_SE_REFUSE);
            }
        }
        break;
    case 6:
        if (D_0069D4E4 != 0) {
            if (choiceGetResult() == 0) {
                flagsBefore = ptGetMenuMember()->buffer->flags[0];
                ctx = ptGetMenuMember();
                if (selectionApplyEntry(ctx, D_006A51E0[0], D_006A51E0[5], D_006A51E0[10]) != 0) {
                    countdownInit(menuMainTxdEntry(D_006A51E0[0] + 0x36), 300);
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(D_006A51E0[5] + 0x3A));
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(0x47));
                    msgBufferAppend(D_0069D550);
                    msgBufferAppend(menuMainTxdEntry(0x4B));
                    msgBufferAppend(func_00215598(D_003EC120[D_006A51E0[0] * 8 + D_006A51E0[5]]));
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(0x4C));
                    if (flagsBefore != ptGetMenuMember()->buffer->flags[0]) {
                        countdownActive = 3;
                    } else {
                        countdownActive = 2;
                    }
                    limits = ptGetMenuMember()->limits;
                    limits->classPoints += D_003EC120[D_006A51E0[0] * 8 + D_006A51E0[5]];
                    if (ptGetMenuMember()->limits->classPoints > 99999) {
                        ptGetMenuMember()->limits->classPoints = 99999;
                    }
                }
            }
            D_0069D4E4 = 0;
        } else if ((D_0063EDB0[0] & 0x800000005000ULL) == 0x800000000000ULL) {
            seVoiceStartDefault(MENU_SE_CURSOR);
            func_00211918((ControlLink *) D_006A51F4);
            slot->state = 5;
        } else if (padCirclePressedAlone() != 0) {
            if (selectionEntryBitOutsideMask(D_006A51E0[0], D_006A51E0[5], D_006A51E0[10], D_0069D508) == 0
                && selectionCanTakeSub(func_0022A560(), D_006A51E0[0], D_006A51E0[5], D_006A51E0[10]) != 0) {
                int value;

                seVoiceStartDefault(MENU_SE_CONFIRM);
                choiceInit(selectionSetValue(D_006A51E0[0], D_006A51E0[5], D_006A51E0[10], 0), 1);
                msgBufferAppend(D_0069D550);
                msgBufferAppend(menuMainTxdEntry(0x43));
                value = selectionGetValue(D_006A51E0[0], D_006A51E0[5], D_006A51E0[10]);
                msgBufferAppend(func_00215598(value));
                msgBufferAppend(D_0069D550);
                msgBufferAppend(menuMainTxdEntry(0x44));
                D_0069D4E4 = 1;
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

/* The port-0 record of PadData as this function reads it: the held directions at 0x30 (pad.h leaves it unmodeled)
   and the repeated directions at 0x34. */
typedef struct PadRecordFields {
    u8 unmodeled_00[0x30];
    unsigned short directionHeld;     /* 0x30 */
    u8 unmodeled_32[2];
    unsigned short directionRepeated; /* 0x34 */
} PadRecordFields;

/* Per-slot step of a ControlSlot: once its flag bit 0 is set, the state moves 0 -> 4 -> 5 and the cursor offset
   is set for a left or right press; always returns 0. */
int menuCommandPageStep(ControlSlot *slot) {
    PadRecordFields *pad;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    if ((u8) slot->state == 0) {
        slot->state = 4;
        return 0;
    }
    if (padCrossPressedAlone() != 0) {
        seVoiceStartDefault(MENU_SE_BACK);
        if (D_0069D4ED != 0) {
            menuFocusRootSlot();
        } else {
            menuCancelToMainSlot();
        }
        return 0;
    }
    pad = (PadRecordFields *) PadData;
    if ((pad->directionRepeated & (PAD_LEFT | PAD_RIGHT)) != 0 && (pad->directionHeld & (PAD_UP | PAD_DOWN)) == 0) {
        if ((u8) slot->state == 4) {
            slot->state = 5;
            cursorBufferSetOffset(D_006A4928, 1);
            menuRebuildCursorBuffers();
        } else {
            slot->state = 4;
            cursorBufferSetOffset(D_006A4928, 0);
            menuRebuildCursorBuffers();
        }
        seVoiceStartDefault(MENU_SE_CURSOR);
    }
    return 0;
}

void selectionRebuildList(void);
void selectionTakePlainEntry(struct ControlContext *context, int index, int n);
void selectionPlaceKey(struct SelectionContext *context, int value, int count);
void dataBoxIncCategory2(int item);
void battleSlotSetShort(int battle, int slot, int value);

int menuEquipAssignStep(ControlSlot *slot) {
    PadRecord *pad;
    CursorBuffer *cursor;
    CursorBuffer *selection;
    int i;

    if ((slot->flags & 1) == 0) {
        return 0;
    }
    switch ((u8) slot->state) {
    case 0:
        if (D_0069D4ED == 0 && D_0069D4E0 == 0) {
            func_00211918(0);
            slot->state = 1;
        } else {
            slot->state = 4;
        }
        break;
    case 1:
        pad = (PadRecord *) PadData;
        if (pad->directionRepeated & PAD_LEFT) {
            cursorBufferMoveBy(D_006A4928, -1);
            menuRebuildCursorBuffers();
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            cursorBufferMoveBy(D_006A4928, 1);
            menuRebuildCursorBuffers();
        }
        if (padCrossPressedAlone() == 0) {
            if (padCirclePressedAlone() == 0) {
                break;
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            func_00211918((ControlLink *) D_006A51C8);
            slot->state = 4;
            break;
        }
        seVoiceStartDefault(MENU_SE_BACK);
        if (D_0069D4ED != 0) {
            menuFocusRootSlot();
        } else {
            menuCancelToMainSlot();
        }
        break;
    case 4:
        if (padCrossPressedAlone() != 0) {
            if (D_0069D4E0 != 0) {
                seVoiceStartDefault(MENU_SE_BACK);
                if (D_0069D4ED != 0) {
                    menuFocusRootSlot();
                } else {
                    menuCancelToMainSlot();
                }
                break;
            }
            seVoiceStartDefault(MENU_SE_CANCEL);
            func_00211918(0);
            slot->state = 1;
            break;
        }
        if (padCirclePressedAlone() != 0) {
                                    if (D_006A51C8[0].capacity == 0) {
                break;
            }
            if (D_006A4928[0].cursor != 0) {
                break;
            }
            if (ptGetMenuMember()->buffer->count < D_0069D504) {
                if (D_0069D4E0 != 0) {
                    if (selectionFindIndex(ptGetMenuMember(), selectionPlainList[D_006A51C8[0].cursor]) == -1) {
                        selectionTakePlainEntry(ptGetMenuMember(), D_006A51C8[0].cursor,
                                                ptGetMenuMember()->buffer->count);
                    ptGetMenuMember()->buffer->count++;
                    D_006A51B0[0].capacity++;
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    break;
                    } else {
                        seVoiceStartDefault(MENU_SE_REFUSE);
                        break;
                    }
                } else {
                    if (selectionFindIndex(ptGetMenuMember(), D_006A52E0[D_006A51C8[0].cursor]) == -1) {
                        selectionPlaceKey((struct SelectionContext *) ptGetMenuMember(),
                                          D_006A52E0[D_006A51C8[0].cursor], ptGetMenuMember()->buffer->count);
                    ptGetMenuMember()->buffer->count++;
                    D_006A51B0[0].capacity++;
                    seVoiceStartDefault(MENU_SE_CONFIRM);
                    break;
                    } else {
                        seVoiceStartDefault(MENU_SE_REFUSE);
                        break;
                    }
                }
            }
            if (D_0069D4E0 != 0 && selectionFindIndex(ptGetMenuMember(),
                                                      selectionPlainList[D_006A51C8[0].cursor]) != -1) {
                seVoiceStartDefault(MENU_SE_REFUSE);
                break;
            }
            D_0069D4DC &= ~1;
            slot->state = 5;
            func_00211918((ControlLink *) D_006A51B0);
            seVoiceStartDefault(MENU_SE_CONFIRM);
            break;
        }
        {
            PadRecord *held = (PadRecord *) PadData;
            if ((*(unsigned long long *) &held->unmodeled_30 & 0x80F00FLL) == 0x800000LL) {
                if (D_006A4928[0].cursor == 0) {
                    if (ptGetMenuMember()->buffer->count != 0) {
                        D_0069D4DC &= ~1;
                        slot->state = 6;
                        func_00211918((ControlLink *) D_006A51B0);
                        seVoiceStartDefault(MENU_SE_CONFIRM);
                        break;
                    }
                    seVoiceStartDefault(MENU_SE_REFUSE);
                    break;
                }
            }
        }
        {
            PadRecord *pad4 = (PadRecord *) PadData;
            if ((*(unsigned long long *) &pad4->unmodeled_30 & 0x800000005000ULL) == 0x800000000000ULL) {
                cursorBufferMoveUnits(D_006A51C8, -1);
            }
            if ((*(unsigned long long *) &pad4->unmodeled_30 & 0x200000005000ULL) == 0x200000000000ULL) {
                cursorBufferMoveUnits(D_006A51C8, 1);
            }
        }
        break;
    case 5:
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            D_0069D4DC |= 1;
            slot->state = 4;
            func_00211918((ControlLink *) D_006A51C8);
            break;
        }
        if (padCirclePressedAlone() == 0) {
            break;
        }
                        if (D_0069D4E0 != 0) {
            int item;

            item = ptGetMenuMember()->buffer->items[D_006A51B0[0].cursor];
            selectionTakePlainEntry(ptGetMenuMember(), D_006A51C8[0].cursor, D_006A51B0[0].cursor);
            dataBoxIncCategory2(item);
            selectionRebuildList();
            cursorBufferSetCapacity(D_006A51C8, selectionPlainCount);
        } else {
            selectionPlaceKey((struct SelectionContext *) ptGetMenuMember(), D_006A52E0[D_006A51C8[0].cursor],
                              D_006A51B0[0].cursor);
        }
        D_0069D4DC |= 1;
        slot->state = 4;
        func_00211918((ControlLink *) D_006A51C8);
        seVoiceStartDefault(MENU_SE_CONFIRM);
        break;
    case 6:
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            D_0069D4DC |= 1;
            slot->state = 4;
            func_00211918((ControlLink *) D_006A51C8);
            break;
        }
        if ((*(unsigned long long *) &((PadRecord *) PadData)->unmodeled_30 & 0x80F00FLL) == 0x800000LL) {
            if (D_0069D4E0 != 0) {
                int i;
                for (i = 0; i < ptGetMenuMember()->buffer->count; i++) {
                    dataBoxIncCategory2(ptGetMenuMember()->buffer->items[i]);
                }
                selectionRebuildList();
                cursorBufferSetCapacity(D_006A51C8, selectionPlainCount);
                battleSlotSetShort(ptGetMenuMember()->kind, 0, 0);
                battleSlotSetShort(ptGetMenuMember()->kind, 1, 0);
                battleSlotSetShort(ptGetMenuMember()->kind, 2, 0);
            } else {
                battleSlotSetByte(ptGetMenuMember()->kind, 0, 0);
                battleSlotSetByte(ptGetMenuMember()->kind, 1, 0);
                battleSlotSetByte(ptGetMenuMember()->kind, 2, 0);
                battleSlotSetByte(ptGetMenuMember()->kind, 3, 0);
            }
                        ptGetMenuMember()->buffer->count = 0;
            cursorBufferSetCapacity(D_006A51B0, 0);
            seVoiceStartDefault(MENU_SE_CONFIRM);
            D_0069D4DC |= 1;
            slot->state = 4;
            func_00211918((ControlLink *) D_006A51C8);
            break;
        }
        if (padCirclePressedAlone() == 0) {
            break;
        }
        seVoiceStartDefault(MENU_SE_CONFIRM);
        if (ptGetMenuMember()->buffer->count == 0) {
            break;
        }
                        if (D_0069D4E0 != 0) {
            dataBoxIncCategory2(ptGetMenuMember()->buffer->items[D_006A51B0[0].cursor]);
            selectionRebuildList();
            cursorBufferSetCapacity(D_006A51C8, selectionPlainCount);
            battleSlotSetShort(ptGetMenuMember()->kind, D_006A51B0[0].cursor, 0);
            ptGetMenuMember()->buffer->items[D_006A51B0[0].cursor] = 0;
            if (D_006A51B0[0].cursor < ptGetMenuMember()->buffer->count - 1) {
                for (i = D_006A51B0[0].cursor + 1; i < ptGetMenuMember()->buffer->count; i++) {
                    battleSlotSetShort(ptGetMenuMember()->kind, i - 1, ptGetMenuMember()->buffer->items[i]);
                    ptGetMenuMember()->buffer->items[i - 1] = ptGetMenuMember()->buffer->items[i];
                }
                battleSlotSetShort(ptGetMenuMember()->kind, i - 1, 0);
                ptGetMenuMember()->buffer->items[i - 1] = 0;
            }
        } else {
            battleSlotSetByte(ptGetMenuMember()->kind, D_006A51B0[0].cursor, 0);
            ptGetMenuMember()->buffer->items[D_006A51B0[0].cursor] = 0;
            if (D_006A51B0[0].cursor < ptGetMenuMember()->buffer->count - 1) {
                for (i = D_006A51B0[0].cursor + 1; i < ptGetMenuMember()->buffer->count; i++) {
                    battleSlotSetByte(ptGetMenuMember()->kind, i - 1,
                                      selectionRecordTable[ptGetMenuMember()->buffer->items[i] * 6 + 1]);
                    ptGetMenuMember()->buffer->items[i - 1] = ptGetMenuMember()->buffer->items[i];
                }
                battleSlotSetByte(ptGetMenuMember()->kind, i - 1, 0);
                ptGetMenuMember()->buffer->items[i - 1] = 0;
            }
        }
        ptGetMenuMember()->buffer->count--;
        cursorBufferSetCapacity(D_006A51B0, ptGetMenuMember()->buffer->count);
        if (ptGetMenuMember()->buffer->count != 0) {
            break;
        }
        D_0069D4DC |= 1;
        slot->state = 4;
        func_00211918((ControlLink *) D_006A51C8);
        break;
    default:
        break;
    }
    return 0;
}

/* PtState: folded into PartyState. */
void menuNavigate(StateBlock *object, int direction);
void ptSetCurrentSlot(int slot);
void ptSwapSlots(int a, int b);
void ptSwapRosterEntries(int a, int b);
void func_0022B458(void);
void ptLoadPartyModels(void);
/* Advances the back or front slot cursor of ptState until the chosen slot is filled, and starts voice 0x01000001 when the cursor moved. */
void ptAdvanceSlotCursor(void);
int ptAllCopilotsAssigned(void);
extern u8 D_003EC65D[];
extern u8 D_003EC65E[];
extern int D_0069D56C;

int menuPartyFormationStep(ControlSlot *slot) {
    if ((slot->flags & CONTROL_SLOT_FLAG_MARK) == 0) {
        return 0;
    }
    switch ((u8) slot->state) {
    case CONTROL_STATE_IDLE:
        if (D_0069D4ED != 0) {
            D_006A4940.flags |= 1;
            slot->state = CONTROL_STATE_RUNNING;
            D_0069D4DC |= 1;
        } else {
            slot->state = CONTROL_STATE_BROWSE;
        }
        break;
    case CONTROL_STATE_BROWSE:
    {
        PadRecord *pad = (PadRecord *) PadData;
        if (pad->directionRepeated & PAD_LEFT) {
            cursorBufferMoveBy(D_006A4928, -1);
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            cursorBufferMoveBy(D_006A4928, 1);
        }
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuFocusRootSlot();
            break;
        }
        if (padCirclePressedAlone() != 0) {
        switch (D_006A4928[0].cursor) {
        case 0:
            seVoiceStartDefault(MENU_SE_CONFIRM);
            D_006A4940.flags |= 1;
            D_0069D4DC |= 1;
            slot->state = CONTROL_STATE_RUNNING;
            break;
        case 1:
            slot->state = CONTROL_STATE_CHOICE;
            D_006A4940.flags |= 1;
            D_0069D4DC |= 1;
            if (D_0069D4E0 != 0) {
                menuSetCursor(&D_006A4940, D_003EC65E[0]);
            } else {
                menuSetCursor(&D_006A4940, D_003EC65D[0]);
            }
            menuSetMode(&D_006A4940, D_006A4940.mode);
            menuEnableItems(&D_006A4940, 0x500);
            seVoiceStartDefault(MENU_SE_CONFIRM);
            break;
        default:
            break;
        }
            break;
        }
        if (pad->directionPressed & PAD_START) {
            ptAdvanceSlotCursor();
        }
        break;
    }
    case CONTROL_STATE_RUNNING:
    {
        PadRecord *pad = (PadRecord *) PadData;
        if (pad->directionRepeated & PAD_UP) {
            menuNavigate(&D_006A4940, 0);
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            menuNavigate(&D_006A4940, 1);
        }
        if (pad->directionRepeated & PAD_DOWN) {
            menuNavigate(&D_006A4940, 2);
        }
        if (pad->directionRepeated & PAD_LEFT) {
            menuNavigate(&D_006A4940, 3);
        }
        D_003EC662[0] = D_006A4940.cursor;
        if (padCrossPressedAlone() != 0) {
            if (D_0069D4E0 != 0 && ptAllCopilotsAssigned() == 0) {
                countdownStart();
            } else {
                slot->state = CONTROL_STATE_BROWSE;
                D_006A4940.flags &= ~1;
                D_0069D4DC &= ~1;
                seVoiceStartDefault(MENU_SE_CANCEL);
            }
            break;
        }
        if (padCirclePressedAlone() != 0) {
            menuSetSelected(&D_006A4940, D_006A4940.cursor);
            slot->state = 2;
            D_0069D56C = D_006A4940.cursor;
            if (D_0069D4E0 != 0) {
                if (D_006A4940.cursor < 3) {
                    menuEnableItems(&D_006A4940, 0x401);
                } else if (D_006A4940.cursor < 6) {
                    menuEnableItems(&D_006A4940, 0xC02);
                } else {
                    menuEnableItems(&D_006A4940, 0x80C);
                }
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            break;
        }
        if ((*(unsigned long long *) &pad->unmodeled_30 & 0x80F00FLL) == 0x800000LL) {
            if (D_0069D4E0 != 0) {
                seVoiceStartDefault(MENU_SE_CANCEL);
                func_0022B458();
                menuEnableItems(&D_006A4940, 0xF0F);
                menuSetCursor(&D_006A4940, D_006A4940.cursor);
            }
            break;
        }
        if (pad->directionPressed & PAD_START) {
            ptAdvanceSlotCursor();
        }
        break;
    }
    case 2:
    {
        PadRecord *pad = (PadRecord *) PadData;
        if (pad->directionRepeated & PAD_UP) {
            menuNavigate(&D_006A4940, 0);
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            menuNavigate(&D_006A4940, 1);
        }
        if (pad->directionRepeated & PAD_DOWN) {
            menuNavigate(&D_006A4940, 2);
        }
        if (pad->directionRepeated & PAD_LEFT) {
            menuNavigate(&D_006A4940, 3);
        }
        D_003EC662[0] = D_006A4940.cursor;
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuSetSelected(&D_006A4940, -1);
            slot->state = CONTROL_STATE_RUNNING;
            if (D_0069D4E0 != 0) {
                menuEnableItems(&D_006A4940, 0xF0F);
            }
            break;
        }
        if (padCirclePressedAlone() != 0) {
        {
            int sourceIndex = D_0069D56C;
            u8 *entryFlags = (u8 *) &D_006A4940 + sourceIndex * 0x40;
            unsigned int sourceSlot;
            unsigned int targetSlot;
            if (*(int *) (entryFlags + 0xC8) & 0x100) {
                u8 *items = (u8 *) &D_006A4940 + 0x0C;
                sourceSlot = *(int *) (items + sourceIndex * 0x40 + 0x80);
                if (sourceSlot < 3) {
                    targetSlot = *(int *) (items + D_006A4940.cursor * 0x40 + 0x80);
                    ptSwapSlots(sourceSlot + 7, targetSlot + 7);
                } else {
                    targetSlot = *(int *) (items + D_006A4940.cursor * 0x40 + 0x80);
                    ptSwapRosterEntries(sourceSlot, targetSlot);
                }
            } else {
                u8 *items = (u8 *) &D_006A4940 + 0x0C;
                sourceSlot = *(int *) (items + sourceIndex * 0x40 + 0x80);
                targetSlot = *(int *) (items + D_006A4940.cursor * 0x40 + 0x80);
                ptSwapSlots(sourceSlot, targetSlot);
            }
        }
        menuSetSelected(&D_006A4940, -1);
        slot->state = CONTROL_STATE_RUNNING;
        if (D_0069D4E0 != 0) {
            menuEnableItems(&D_006A4940, 0xF0F);
        }
        seVoiceStartDefault(MENU_SE_CONFIRM);
        ptLoadPartyModels();
        break;
        }
        if (pad->directionPressed & PAD_START) {
            ptAdvanceSlotCursor();
        }
        break;
    }
    case 3:
        break;
    case CONTROL_STATE_CHOICE:
    {
        PadRecord *pad = (PadRecord *) PadData;
        if (pad->directionRepeated & PAD_UP) {
            menuNavigate(&D_006A4940, 0);
        }
        if (pad->directionRepeated & PAD_RIGHT) {
            menuNavigate(&D_006A4940, 1);
        }
        if (pad->directionRepeated & PAD_DOWN) {
            menuNavigate(&D_006A4940, 2);
        }
        if (pad->directionRepeated & PAD_LEFT) {
            menuNavigate(&D_006A4940, 3);
        }
        D_003EC662[0] = D_006A4940.cursor;
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            menuSetSelected(&D_006A4940, -1);
            slot->state = CONTROL_STATE_BROWSE;
            D_006A4940.flags &= ~1;
            D_0069D4DC &= ~1;
            menuEnableItems(&D_006A4940, 0xF0F);
            break;
        }
        if (padCirclePressedAlone() != 0) {
            if (D_0069D4E0 != 0) {
                ptSetCurrentSlot(D_006A4940.cursor + 7);
            } else {
                ptSetCurrentSlot(D_006A4940.cursor);
            }
            seVoiceStartDefault(MENU_SE_CONFIRM);
            break;
        }
        if (pad->directionPressed & PAD_START) {
            ptAdvanceSlotCursor();
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

/* Steps one ControlSlot in state 0, 1 or 4; returns 0 on every path. */
int menuRecordItemStep(ControlSlot *slot) {
    unsigned long long padBits;

    if (!(slot->flags & CONTROL_SLOT_FLAG_MARK)) {
        return 0;
    }
    switch ((u8) slot->state) {
    case CONTROL_STATE_IDLE:
        slot->state = CONTROL_STATE_RUNNING;
        break;
    case CONTROL_STATE_BROWSE:
        if ((PadData[0].repeated & PAD_LEFT) != 0) {
            cursorBufferMoveBy(D_006A4928, -1);
        }
        if ((PadData[0].repeated & PAD_RIGHT) != 0) {
            cursorBufferMoveBy(D_006A4928, 1);
        }
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(MENU_SE_BACK);
            menuFocusRootSlot();
            return 0;
        }
        if (padCirclePressedAlone() == 0) {
            return 0;
        }
        switch (D_006A4928[0].cursor) {
        case 0:
            menuSceneSetState(1);
            break;
        case 1:
            seVoiceStartDefault(MENU_SE_CONFIRM);
            slot->state = CONTROL_STATE_RUNNING;
            func_00211918(slot->link);
            break;
        }
        break;
    case CONTROL_STATE_RUNNING:
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(MENU_SE_CANCEL);
            func_00211918(0);
            slot->state = CONTROL_STATE_BROWSE;
            return 0;
        }
        padBits = PadData[0].doubleword;
        if ((padBits & 0x800000005000ULL) == 0x800000000000ULL) {
            cursorBufferMoveBy(D_006A5168, -9);
            padBits = PadData[0].doubleword;
        }
        if ((padBits & 0x200000005000ULL) == 0x200000000000ULL) {
            cursorBufferMoveBy(D_006A5168, 9);
            padBits = PadData[0].doubleword;
        }
        if ((padBits & 0x400005000ULL) == 0x400000000ULL) {
            cursorBufferMoveUnits(D_006A5168, -1);
            padBits = PadData[0].doubleword;
        }
        if ((padBits & 0x800005000ULL) == 0x800000000ULL) {
            cursorBufferMoveUnits(D_006A5168, 1);
        }
        break;
    }
    return 0;
}

extern signed char D_0069D4F0[];
void menuSkillModelTailLoad(void);
void xglSleep(void);
void tyaDisplaySetting(int value);
void saveDataSetDefaults(void);
void tyaSetDisplayOffset(int a, int b);
void func_001A4400(int value);
void vibrationResetFade(void);
void func_00208B50(int value);
void vibrationStartFade(int a, int b, int c);

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
            xglCdBufferRewind();
            menuSkillModelTailLoad();
            menuFocusRootSlot();
            return 0;
        }
        if ((PadData[0].pressed & (PAD_LEFT | PAD_RIGHT)) != 0) {
            int optionIndex = D_006A5220[0].cursor;
            switch (D_003EB040[optionIndex]) {
            case 0:
                seVoiceStartDefault(MENU_SE_CURSOR);
                SaveData.flag_46 ^= 1;
                D_0069D4EC = SaveData.flag_46;
                break;
            case 1:
                seVoiceStartDefault(MENU_SE_CURSOR);
                SaveData.flag_47 ^= 1;
                D_0069D4ED = SaveData.flag_47;
                break;
            case 2:
                seVoiceStartDefault(MENU_SE_CURSOR);
                if ((PadData[0].pressed & PAD_LEFT) != 0) {
                    SaveData.counter_28 = SaveData.counter_28 + 2;
                } else {
                    SaveData.counter_28 = SaveData.counter_28 + 1;
                }
                SaveData.counter_28 %= 3;
                func_001A4400(SaveData.counter_28 > 0);
                break;
            case 3:
                seVoiceStartDefault(MENU_SE_CURSOR);
                SaveData.flag_3e ^= 1;
                PadData[1].field_56 = SaveData.flag_3e;
                PadData[0].field_56 = SaveData.flag_3e;
                if (!SaveData.flag_3e) {
                    func_00208B50(15);
                    vibrationStartFade(200, 60, 30);
                } else {
                    vibrationResetFade();
                }
                break;
            case 4: {
                int tableIndex;
                for (tableIndex = 0; tableIndex < 2; tableIndex++) {
                    if (D_0069D4F0[tableIndex] != SaveData.index_3a) {
                        continue;
                    }
                    tableIndex = (PadData[0].pressed & PAD_LEFT) != 0 ? tableIndex - 1 : tableIndex + 1;
                    tableIndex += 2;
                    tableIndex %= 2;
                    SaveData.index_3a = D_0069D4F0[tableIndex];
                    tableIndex = -1;
                    break;
                }
                if (tableIndex == 2) {
                    SaveData.index_3a = 0;
                }
                seVoiceStartDefault(MENU_SE_CURSOR);
                break;
            }
            case 7:
                seVoiceStartDefault(MENU_SE_CURSOR);
                SaveData.flag_29 ^= 1;
                break;
            }
            return 0;
        }
        if (padCirclePressedAlone() == 0) {
            return 0;
        }
        switch (D_003EB040[D_006A5220[0].cursor]) {
        case 5: {
            int displaySetting;
            seVoiceStartDefault(MENU_SE_CONFIRM);
            xglCdBufferRewind();
            displaySetting = xglCdBufferReserve(0x00100000);
            xglSleep();
            tyaDisplaySetting(displaySetting);
            xglCdBufferRewind();
            fontObjLoadTexture(8);
            fontObjLoadTexture(9);
            break;
        }
        case 6:
            seVoiceStartDefault(MENU_SE_CONFIRM);
            saveDataSetDefaults();
            PadData[1].field_56 = SaveData.flag_3e;
            PadData[0].field_56 = SaveData.flag_3e;
            func_001A4400(SaveData.counter_28 != 0);
            tyaSetDisplayOffset(SaveData.field_32, SaveData.field_34);
            D_0069D4EC = SaveData.flag_46;
            D_0069D4ED = SaveData.flag_47;
            break;
        case 8:
            slot->state = CONTROL_STATE_WAIT;
            func_00211918(0);
            seVoiceStartDefault(0x01000006);
            break;
        }
        return 0;
    case CONTROL_STATE_WAIT:
        if (padCirclePressedAlone() != 0 || padCrossPressedAlone() != 0) {
            slot->state = CONTROL_STATE_RUNNING;
            func_00211918((ControlLink *) D_006A5220); /* a CursorBuffer passed where a ControlLink is expected */
            seVoiceStartDefault(MENU_SE_BACK);
            menuPlaceCursor(0, 0, 0, 0);
        }
        return 0;
    }
    return 0;
}

extern signed char D_0069D4F8[];
extern signed char D_0069D500;
extern unsigned char D_0069D570[];
extern unsigned char D_0069D578[];
void choiceSetFirstOptionText(void *entry);
void choiceSetSecondOptionText(void *entry);

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
            if (D_0069D4E4 != 0) {
                if (choiceGetResult() == 2) {
                    D_0069D4E4 = 0;
                    slot->state = CONTROL_STATE_RUNNING;
                } else {
                    int choice;
                    choice = choiceGetResult();
                    D_0069D4E4 = 0;
                    D_0069D500 = choice;
                    slot->state = CONTROL_STATE_CHOICE_APPLY;
                }
            } else {
                choiceInit(menuMainTxdEntry(((ControlLink *) slot->link)->kind + 0x1D6), D_0069D4E0);
                msgBufferAppend(D_0069D570);
                msgBufferAppend(menuMainTxdEntry(0x1EA));
                choiceSetFirstOptionText(menuMainTxdEntry(0x1EB));
                choiceSetSecondOptionText(menuMainTxdEntry(0x1EC));
                D_0069D4E4 = 1;
            }
        break;
    case CONTROL_STATE_CHOICE_APPLY:
            if (D_0069D4E4 != 0) {
                if (choiceGetResult() == 0) {
                    D_0069D501 = 1;
                    changed = 1;
                } else {
                    D_0069D4E4 = 0;
                    slot->state = CONTROL_STATE_RUNNING;
                }
            } else {
                choiceInit(menuMainTxdEntry(((ControlLink *) slot->link)->kind + 0x1D6), 0);
                msgBufferAppend(D_0069D550);
                msgBufferAppend(menuMainTxdEntry(D_0069D500 + 0x1EF));
                msgBufferAppend(D_0069D578);
                choiceSetFirstOptionText(menuMainTxdEntry(0x1EE - D_0069D500));
                choiceSetSecondOptionText(menuMainTxdEntry(0x1C3));
                choiceSetRow(0xF0);
                D_0069D4E4 = 1;
            }
        break;
    case CONTROL_STATE_RUNNING:
        if (padCrossPressedAlone() != 0) {
            seVoiceStartDefault(MENU_SE_BACK);
            xglCdBufferRewind();
            menuSkillModelTailLoad();
            menuFocusRootSlot();
        } else if (padCirclePressedAlone() != 0 && slot->flag == 0) {
            int kind;
            seVoiceStartDefault(MENU_SE_CONFIRM);
            kind = ((ControlLink *) slot->link)->kind;
            if (D_0069D4F8[kind] == 2) {
                slot->state = CONTROL_STATE_CHOICE;
            } else {
                D_0069D500 = D_0069D4F8[kind];
                slot->state = CONTROL_STATE_CHOICE_APPLY;
            }
        }
        break;
    }
    return changed;
}

/* The block passed to menuButtonHandle: a countdown byte at 0x0E and an enable byte at 0x10. */
typedef struct MenuInputBlock {
    u8 unmodeled_00[14];
    u8 countdown;               /* 0x0E: reset to 3 when it reads zero */
    u8 unmodeled_0F;
    u8 enabled;                 /* 0x10: bit 0 gates the whole routine */
} MenuInputBlock;

int menuButtonHandle(MenuInputBlock *input) {
    if ((input->enabled & 1) == 0) {
        return 0;
    }
    if (input->countdown == 0) {
        input->countdown = 3;
        return 0;
    }
    if (PadData[0].repeated & PAD_UP) {
        menuNavigate(&D_006A4940, 0);
    }
    if (PadData[0].repeated & PAD_RIGHT) {
        menuNavigate(&D_006A4940, 1);
    }
    if (PadData[0].repeated & PAD_DOWN) {
        menuNavigate(&D_006A4940, 2);
    }
    if (PadData[0].repeated & PAD_LEFT) {
        menuNavigate(&D_006A4940, 3);
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

        cursor = *(unsigned int *) D_006A4900;
        value = *(const unsigned int *) D_006A4D0C;
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
    if (PadData[0].pressed & PAD_START) {
        ptAdvanceSlotCursor();
    }
    return 0;
}

void grFillFrame(void);
void grCopyFrame(int dstBase, int srcBase);
void func_0012A510(int);
void xglTickAdvance(void);
void xglCdWaitIdle(void);
int bgLayerReady(int index);
void xglActiveCameraSetup(void);
void xglRenderClearFrame(void);
void func_001BA68C(void);
void menuSelectionRingDraw(int mode);
void XenoActorSetFlagAll(int count);
void XenoActorDoit(void);
void XenoActorDisp(void);

/* ClearEnv (0x006457C0): the GS clear colour words, written as zero before the clear. */
typedef struct ClearEnvColor {
    u8 unmodeled_00[0x20];
    u32 color_r;                    /* 0x20 */
    u32 color_g;                    /* 0x24 */
    u32 color_b;                    /* 0x28 */
    u32 color_a;                    /* 0x2C */
} ClearEnvColor;

extern ClearEnvColor ClearEnv;

/* Scene start: waits until the three background layers are ready and CD reads are idle, sets the camera and the GS clear colour, then plays every control entry once (menuSkillModelLoadOnce 0..10). */
void menuSceneEnter(void) {
    do {
        func_0012A510(0);
        grFillFrame();
        grCopyFrame(sRender.display_base, sRender.frame_base);
        xglTickAdvance();
        xglSleep();
    } while (!(bgLayerReady(0) && bgLayerReady(1) && bgLayerReady(2)));

    xglActiveCameraSetup();
    bgLayerCameraSetup();
    ClearEnv.color_r = 0;
    ClearEnv.color_g = 0;
    ClearEnv.color_b = 0;
    ClearEnv.color_a = 0;
    xglRenderClearFrame();
    while (xglCdServiceIfPending() != 0) {
        func_001BA68C();
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

/* Resets the pad repeat, releases the scene assets, then copies the first control slot's state into D_0069E228 and clears D_0069D502. */
void menuSceneExit(void) {
    unsigned char firstSlotState;

    xglPadSetRepeat(0, PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT | PAD_L1 | PAD_R1, 8, 1);
    ptRefresh();
    menuSceneUnloadAssets();
    bgLayerRelease();
    vibrationResetFade();
    firstSlotState = menuSlots[0].state;
    D_0069D502 = 0;
    D_0069E228 = firstSlotState;
}

typedef struct MenuSceneWork {
    u8 unmodeled_00[8];
    int control;                /* 0x08: 0x04000000 while the menu scene runs; 0x10000000 soft reset */
} MenuSceneWork;

extern MenuSceneWork gameWork;
void func_001BA6F4(void);
void fadeStartIn(int frames);
void uwamonoAttachCurrent(int record, int index);
void uwamonoDetachCurrent(void);
void yg2PathRun(void);
void ysegfRunMenu(void);
void menuDraw(void *menu);

/* Scene loop: steps the control slots and the menu each frame until a result is set (1 leaves, 2 and 3 reload a sub-scene), then runs menuSceneExit. */
int menuSceneRunLoop(void) {
    int opening;
    int sceneResult = 0;
    int i;
    int decrement;
    int remaining;
    int stepResult;
    ControlSlot *slot;
    PadDirections *padPort0;
    PadDirections *padNow;
    RenderFrameState *renderBases;

    opening = 1;
    if (ptState.counts[0] == 0) {
        return 0;
    }
    if (ptState.splitMode == 0) {
        if (ptState.counts[1] == 0) {
            return 0;
        }
        D_0069D4E0 = 0;
    } else {
        if (ptState.counts[2] == 0) {
            return 0;
        }
        D_0069D4E0 = 1;
    }
    XenoActorDoit();
    XenoActorDisp();
    xglSleep();
    {
        RenderFrameState *frame = &sRender;

        grCopyFrame(frame->frame_base, frame->copy_base);
    }
    func_001BA6F4();
    gameWork.control |= 0x04000000;
    padPort0 = PadData;
    seVoiceStartDefault(0x02000001);
    menuSceneInit();
    menuSceneEnter();

    for (;;) {
        MenuSceneWork *work = &gameWork;

        if (D_0069D518 == 0 && (padPort0->doubleword & 0x10F00F) == 0x100000) {
            if (D_0069D4E0 != 0 && menuCurrentSlot == 6) {
                if (ptAllCopilotsAssigned() != 0) {
                    sceneResult = 1;
                } else {
                    countdownStart();
                }
            } else {
                sceneResult = 1;
            }
        }
        padNow = PadData;
        if (((padNow->pressed & PAD_START) != 0 && (padNow->buttonsHeld & PAD_SOFT_RESET_HELD) == 0x10F)
            || (gameWork.control & 0x10000000) != 0) {
            sceneResult = 1;
            work->control |= 0x10000000;
        }

        if (sceneResult != 0) {
            if (sceneResult == 1) {
                goto finish;
            }
            switch (sceneResult) {
            case 2:
                seVoiceStartDefault(MENU_SE_CONFIRM);
                xglCdBufferRewind();
                fontObjLoadTexture(4);
                fontObjLoadTexture(5);
                yg2PathRun();
                menuSkillModelTailLoad();
                D_0069D540 = 0;
                func_00210560(menuSlots, 1, 128);
                func_00210560(&menuSlots[7], 1, 128);
                break;
            case 3:
                seVoiceStartDefault(MENU_SE_CONFIRM);
                xglCdBufferRewind();
                fontObjLoadTexture(6);
                ysegfRunMenu();
                menuSkillModelTailLoad();
                D_0069D540 = 0;
                func_00210560(menuSlots, 1, 128);
                func_00210560(&menuSlots[7], 1, 128);
                break;
            }
            xglActiveCameraSetup();
            sceneResult = 0;
        }

        if ((D_0069D4DC & 2) != 0) {
            if (D_0069D4D8 != *(int *) D_006A4900) {
                if (D_0069D4ED == 0) {
                    menuPendingSelection = -1;
                }
                menuSceneSetState(*(int *) D_006A4900 + 1);
            } else if (D_0069D4CC != 0) {
                D_0069D4CC->flags |= 1;
            }
            D_0069D4DC &= ~2;
            D_0069D4DC |= 4;
        } else if (D_0069D518 == 0) {
            if (menuCurrentSlot > 0) {
                if (menuCurrentSlot < 8 && (padPort0->repeated & 3) != 0
                    && ptGetMenuMember() != 0 && countdownActive == 0 && D_0069D4E4 == 0) {
                    if (D_0069D4E0 != 0 && menuCurrentSlot == 6 && ptAllCopilotsAssigned() == 0) {
                        countdownStart();
                    } else {
                        D_0069D4D8 = *(int *) D_006A4900;
                        if ((padPort0->repeated & 1) != 0) {
                            cursorBufferMoveBy((CursorBuffer *) D_006A4900, -1);
                        } else {
                            cursorBufferMoveBy((CursorBuffer *) D_006A4900, 1);
                        }
                        if (D_0069D4CC != 0) {
                            D_0069D4CC->flags &= ~1;
                        }
                        D_0069D4DC |= 2;
                    }
                }
            }
        }

        for (i = 0; i < 11; i++) {
            if (menuSlots[i].value != 0) {
                if (opening != 0) {
                    decrement = 32;
                } else if (D_0069D4ED != 0) {
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
                    countdownInit(menuMainTxdEntry(D_006A51E0[0] + 54), 300);
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(D_006A51E0[5] + 58));
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(71));
                    msgBufferAppend(D_0069D550);
                    msgBufferAppend(menuMainTxdEntry(D_006A51E0[0] + 55));
                    msgBufferAppend(" ");
                    msgBufferAppend(menuMainTxdEntry(72));
                    countdownActive = 4;
                } else {
                    countdownActive = 0;
                }
            }
        } else if (D_0069D4E4 == 1) {
            if (choiceUpdate() != 0) {
                D_0069D4E4 = 2;
            }
        } else {
            if (D_0069D518 != 0) {
                menuTutorialStep();
                if ((padPort0->pressed & 0x10) != 0) {
                    menuTutorialRestoreState();
                }
            } else {
                if ((D_0069D4DC & 1) != 0) {
                    ControlSlot *activeSlot = &menuSlots[menuCurrentSlot];
                    if (activeSlot->flag == 0 && (padPort0->repeated & 0xc) != 0) {
                        if ((padPort0->repeated & 4) != 0) {
                            menuMoveCursor(&D_006A4940, -1);
                        } else {
                            menuMoveCursor(&D_006A4940, 1);
                        }
                        menuRebuildCursorBuffers();
                    }
                }
                if (D_0069D4CC != 0) {
                    PadDirections *cursorPad = PadData;

                    if ((cursorPad->repeated & PAD_UP) != 0) {
                        cursorBufferMoveBy((CursorBuffer *) D_0069D4CC, -1);
                    }
                    if ((cursorPad->repeated & PAD_DOWN) != 0) {
                        cursorBufferMoveBy((CursorBuffer *) D_0069D4CC, 1);
                    }
                }
            }

            for (i = 0; i < 11; i++) {
                ControlCallback step = menuSlots[i].step;
                if (step != 0 && (D_0069D4DC & 6) == 0 && D_0069D518 == 0) {
                    slot = &menuSlots[i];
                    /* the slot field is declared as the void callback; the steps return int */
                    stepResult = ((ControlStep) step)(slot);
                    if (stepResult != 0) {
                        sceneResult = stepResult;
                    }
                }
            }
        }

        if ((D_0069D4DC & 4) != 0) {
            D_0069D4DC &= ~4;
        }
        fontTrackEase(D_006A5130);
        menuMessageTextUpdate();
        menuDraw(&D_006A4940);
        menuDraw(D_006A4D28);
        func_001BA68C();
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        menuSelectionRingDraw(D_0069D519 & 1);
        if ((work->control & 0x10000000) == 0) {
            xglCallbackQueueAdd((int) menuSceneDraw);
            XenoActorDoit();
            XenoActorDisp();
        }
        xglTickAdvance();
        xglSleep();
    }

finish:
    menuSceneExit();
    if ((gameWork.control & 0x10000000) == 0) {
        if (D_0069D501 == 0) {
            seVoiceStartDefault(MENU_SE_CANCEL);
        }
        renderBases = &sRender;
        grCopyFrame(renderBases->frame_base, renderBases->copy_base);
        for (i = 15; i >= 0; i--) {
            /* Re-taken every step: the original copies the loop-invariant
               address into its own register. */
            RenderFrameState *frame = &sRender;

            func_0012A510(0);
            grFillFrame();
            grCopyFrame(frame->display_base, frame->frame_base);
            xglTickAdvance();
            xglSleep();
        }
        xglTickAdvance();
        xglSleep();
        xglTickAdvance();
        xglSleep();
        ptRefreshCurrent();
        xglSleep();
        if (D_0069D501 == 1) {
            uwamonoAttachCurrent(D_006A5238[0].cursor, D_0069D500);
        } else if (D_0069D501 == 2) {
            uwamonoDetachCurrent();
        } else {
            fadeReset();
            fadeStartIn(30);
        }
    }
    gameWork.control &= ~0x04000000;
    return 0;
}


/* Sets D_0069D502 when the first state object's key is -1, then runs the scene loop menuSceneRunLoop. */
void menuSceneRunKeyCheck(void) {
    if (ptState.progress->key == -1) {
        D_0069D502 = 1;
    }
    menuSceneRunLoop();
}

void ptEnterSplit(void);
void ptLeaveSplit(void);
extern u8 ptSplitMode[];

/* Runs the scene loop menuSceneRunLoop inside the split that ptEnterSplit opens (roster compaction), unless ptSplitMode is 1. */
int menuSceneRunSplit(void) {
    int result;

    if (ptSplitMode[0] == 1) {
        result = menuSceneRunLoop();
    } else {
        ptEnterSplit();
        result = menuSceneRunLoop();
        ptLeaveSplit();
    }
    return result;
}
