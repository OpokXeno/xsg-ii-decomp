#include "common.h"
#include "main/xgl_render.h"
#include "main/font_obj.h"
#include "main/cursor_buffer.h"
#include "main/menu_scene.h"
#include "main/txd_table_entry.h"
#include "main/save_screen.h"
#include "main/pt_member.h"
#include "main/game_snap_shot.h"
#include "main/game_data.h"
#include "main/menu_draw.h"
#include "main/xgl_font.h"
#include "main/se_bank_load.h"
#include "main/xgl_thread.h"
#include "main/xgl_cd.h"
#include "main/xenoActor.h"
#include "main/bg_layer.h"
#include "main/sound_driver.h"
#include "main/nml_model_set.h"
#include "main/xgl_pad.h"
#include "main/xgl_jpeg.h"
#include "main/fade_ctrl.h"
#include "main/xgl_packet.h"
#include "main/pxy_packet_state.h"
#include "main/xgl_cd_arena.h"
#include "main/save_party_init.h"

/* Text strings of the save and config screens, in address order (read by the draw helpers below). */
const char saveProgressStyleText[56] = "$init;$bg0;$zoom13;$han1;$prop1;$xx0,21;$colc0c0c0;";
const char saveProgressSavingText[16] = "Saving...";
const char saveSelectTitleText[16] = "  Select";
const char saveScreenStyleText[64] = "$init;$bg2;$shadow+1,+3,000000;$zoom12;$han1;$prop1;$colc0c0c0;";
const char saveOptionZoomText[16] = "$zoom12,13;";
const char saveSlotDetailDimColorText[40] = "$col407080;$bg3;$shadow+1,+1,000000;";
const char saveSlotDetailColorText[16] = "$colc0c0c0;";


/* xglMcRequest commands (its second argument), numbered as in src/main/xgl_mc_request.c. */
enum {
    MC_REQ_FREE_SPACE = 1,
    MC_REQ_LIST = 3,
    MC_REQ_LOAD = 4,
    MC_REQ_SAVE = 5,
    MC_REQ_CREATE = 6,
    MC_REQ_FORMAT = 7,
    MC_REQ_CHECK_OLD = 8,
};

/* seVoiceStartDefault ids of the save and config menus: cursor step, accept, deny and cancel. */
enum {
    SAVE_SE_CURSOR = 0x01000001,
    SAVE_SE_ACCEPT = 0x01000002,
    SAVE_SE_DENY = 0x01000003,
    SAVE_SE_CANCEL = 0x01000008,
};

/* Values of the config menu step configState (configScreenUpdate; configScreenDraw draws by it). */
enum {
    CONFIG_STATE_BROWSE = 0,
    CONFIG_STATE_CHECK_CARDS = 1,
    CONFIG_STATE_CARD_NOTICE = 2,
    CONFIG_STATE_CARD_A_WAIT = 3,
    CONFIG_STATE_CARD_B_WAIT = 4,
    CONFIG_STATE_CARD_B_START = 5,
    CONFIG_STATE_NOTICE_DONE = 8,
    CONFIG_STATE_RESOLVE = 10,
    CONFIG_STATE_CARD_ERROR = 13,
    CONFIG_STATE_OPEN_LIST = 20,
    CONFIG_STATE_LIST = 21,
    CONFIG_STATE_ITEM = 22,
    CONFIG_STATE_CLOSE_LIST = 25,
};

/* Bits of the notice byte configNoticePending (set by configOldDataCallback, moved to configNoticeShown by configScreenUpdate).
 * EP1 and EP1RE: an Episode I save (callback code 0) or an Episode I Reloaded save (code 1) was found; EP_SAVES is both. */
enum {
    CONFIG_NOTICE_EP1 = 0x01,
    CONFIG_NOTICE_EP1RE = 0x02,
    CONFIG_NOTICE_EP_SAVES = 0x03,
    CONFIG_NOTICE_FLAG = 0x04,
    CONFIG_NOTICE_KIND_1 = 0x08,
    CONFIG_NOTICE_KIND_2 = 0x10,
    CONFIG_NOTICE_KINDS = 0x18,
};

extern int saveProgressFrame;
extern int saveProgressTotal;
void *memset(void *, int, unsigned int);

void saveProgressDraw(void)
{
    int barValue;
    int progressLimit;

    if (saveProgressTotal != 0)
    {
        menuDrawCell(353, 165, 300, 1);
        menuDrawCell(355, 331, 300, 1);
        menuDrawCellSized(354, 181, 300, 150, 17, 1);
        barValue = saveProgressFrame * 5;
        barValue += barValue * 16;
        nmlPacketGsInit();
        packet_gs_entry64(71, 0x30000ULL);
        packet_gs_entry64(66, 0x8000000044ULL);
        barValue <<= 1;
        packet_gs_entry64(1, 0x3f80000030000000ULL);
        barValue >>= 9;
        packet_gs_entry64(0, 70);
        barValue <<= 4;
        packet_gs_entry64(5, 0x85107aa0ULL);
        barValue += 31408;
        packet_gs_entry64(5, 0x85808560ULL);
        packet_gs_entry64(1, 0x3f800000c0ffffaaULL);
        packet_gs_entry64(0, 6);
        packet_gs_entry64(5, 0x85207ab0ULL);
        packet_gs_entry64(5, (unsigned long long) barValue | 0x85700000ULL);
        xglDirectQueueSubmitRef();
        xglFontPrint(0, 0, 0xfffff0, saveProgressStyleText);
        xglFontPrint(234, 278, 0xfffff0, saveProgressSavingText);
        progressLimit = saveProgressTotal - 64;
        if (saveProgressFrame < saveProgressTotal)
        {
            saveProgressFrame++;
        }
        if (saveProgressFrame < progressLimit)
        {
            saveProgressFrame++;
        }
    }
}

#include "shared.h"

typedef struct SaveSlot {
    int active;                 /* offset 0: the decoded thumbnail. saveSlotsDeactivateAll and saveSlotsActivateAll write the
                                 * state field of the records through this offset (see there). */
    unsigned char unmodeled_04[0x7900 - 0x04];
    int state;
    unsigned char member_id;
    unsigned char level;
    unsigned char row_value[7];
    unsigned char unmodeled_790D[0x7910 - 0x790D];
    int play_time;
    unsigned char flag;
    unsigned char unmodeled_7915[0x7918 - 0x7915];
    void *title_text;
    void *subtitle_text;
} SaveSlot;

/* Values of SaveSlot.state: 0 is an unscanned slot, PENDING (1) a slot saveSlotsActivateAll marked and
 * saveSlotsPendingToEmpty turns EMPTY, LOADED a slot read from the card, EMPTY a free slot a save can
 * create, BAD an unreadable image (states 4 and 5 are drawn alike). */
enum {
    SAVE_SLOT_NONE = 0,
    SAVE_SLOT_PENDING = 1,
    SAVE_SLOT_LOADED = 2,
    SAVE_SLOT_EMPTY = 3,
    SAVE_SLOT_BAD = 4,
};

/* The save slots on the card: the table saveSlotTable holds this many records. */
#define SAVE_SLOT_COUNT 20

/* Small statics of the save screen in .sbss, in layout order (the object's .sbss order is this file order). */
static unsigned char *saveSlotRows;
static short mcPortResult[4];
static unsigned char *saveScanBuffer;
static unsigned char *cardDataBuffer;
static SaveThumbnail *saveThumbnailBuffer;
static int *saveTextTable;
static SaveSlot *saveSlotTable;
static signed char configNoticeShown;
static unsigned char configNoticePending;
static signed char configState;
static signed char configCursor;
static short configCardNoticeTimer;
static signed char saveTextMaxima[6];
static unsigned char configOldDataKind;
static unsigned char configFadeStep;
static signed short configCardNoticeResults[2];


/* The loop starts at table + 0x97660 and steps back one record per pass, so record->active is the field at
 * offset 0x7900 (state) of records 19 down to 0. */
void saveSlotsDeactivateAll(void) {
    SaveSlot *record;
    int index;

    record = (SaveSlot *) ((unsigned char *) saveSlotTable + 0x97660);
    for (index = 0; index < 20; index++) {
        record->active = 0;
        record--;
    }
}

/* The same walk as saveSlotsDeactivateAll: it writes SAVE_SLOT_PENDING to the state field of each record. */
void saveSlotsActivateAll(void) {
    SaveSlot *record;
    int index;

    record = (SaveSlot *) ((unsigned char *) saveSlotTable + 0x97660);
    for (index = 0; index < 20; index++) {
        record->active = 1;
        record--;
    }
}

void saveSlotsPendingToEmpty(void) {
    SaveSlot *record;
    int index;

    record = saveSlotTable;
    for (index = 0; index < 20; index++) {
        if (record->state == SAVE_SLOT_PENDING) {
            record->state = SAVE_SLOT_EMPTY;
        }
        record++;
    }
}


typedef struct SaveTextEntry {
    unsigned short id;
    unsigned char firstIndex;
    unsigned char secondIndex;
} SaveTextEntry;

extern SaveTextEntry saveTextIndexTable[];

void saveSlotSetTexts(int slot, int value)
{
    SaveTextEntry *entry;

    if (saveTextIndexTable[0].id != 0) {
        entry = saveTextIndexTable;
        do {
            if (value == entry->id) {
                SaveSlot *table;
                if (entry->firstIndex != 0) {
                    void *first = txdTableEntry(saveTextTable, entry->firstIndex - 1);
                    table = saveSlotTable;
                    (table + slot)->title_text = first;
                } else {
                    table = saveSlotTable;
                    (table + slot)->title_text = 0;
                }
                if (entry->secondIndex != 0) {
                    (saveSlotTable + slot)->subtitle_text = txdTableEntry(saveTextTable, entry->secondIndex - 1);
                } else {
                    (table + slot)->subtitle_text = 0;
                }
                return;
            }
            entry++;
        } while (entry->id != 0);
    }
    (saveSlotTable + slot)->subtitle_text = 0;
    (saveSlotTable + slot)->title_text = 0;
}

extern unsigned char *saveLabelSeparator;
static unsigned char saveLabelBuffer[0x40];

/* Label text buffer size: the bytes written to saveLabelBuffer are capped at this count (an EUC-JP lead byte copies its
 * trail byte too). */
#define SAVE_LABEL_MAX_BYTES 64

/* Builds the label text for `labelId` in saveLabelBuffer: its first table text, the separator and its second text,
 * at most SAVE_LABEL_MAX_BYTES bytes. */
unsigned char *saveLabelBuild(int labelId)
{
    unsigned char *firstText;
    unsigned char *secondText;
    unsigned char *out;
    int written;
    int i;

    firstText = 0;
    secondText = 0;
    for (i = 0; saveTextIndexTable[i].id != 0; i++)
    {
        if (labelId == saveTextIndexTable[i].id)
        {
            if (saveTextIndexTable[i].firstIndex != 0)
            {
                firstText = txdTableEntry(saveTextTable, saveTextIndexTable[i].firstIndex - 1);
            }
            else
            {
                firstText = 0;
            }
            if (saveTextIndexTable[i].secondIndex != 0)
            {
                secondText = txdTableEntry(saveTextTable, saveTextIndexTable[i].secondIndex - 1);
            }
            else
            {
                secondText = 0;
            }
        }
    }

    out = saveLabelBuffer;
    written = 0;
    if (firstText != 0)
    {
        i = 0;
        while (firstText[i] != 0 && written < SAVE_LABEL_MAX_BYTES)
        {
            *out = firstText[i];
            if ((signed char) *out < 0)
            {
                i++;
                out++;
                written++;
                *out = firstText[i];
            }
            i++;
            written++;
            out++;
        }
        if (secondText != 0)
        {
            i = 0;
            while (saveLabelSeparator[i] != 0 && written < SAVE_LABEL_MAX_BYTES)
            {
                *out = saveLabelSeparator[i];
                if ((signed char) *out < 0)
                {
                    i++;
                    out++;
                    written++;
                    *out = saveLabelSeparator[i];
                }
                i++;
                written++;
                out++;
            }
        }
    }
    if (secondText != 0)
    {
        i = 0;
        while (secondText[i] != 0 && written < SAVE_LABEL_MAX_BYTES)
        {
            *out = secondText[i];
            if ((signed char) *out < 0)
            {
                i++;
                out++;
                written++;
                *out = secondText[i];
            }
            i++;
            written++;
            out++;
        }
    }
    *out = 0;
    return saveLabelBuffer;
}

/* Empties the EUC-JP string at text (its first byte becomes the terminator). Callers clear saveHeaderText before
   eucStrcat appends to it. */
void eucStrClear(signed char *text) {
    *text = 0;
}

/* Appends the EUC-JP string src to dst and terminates the result. A lead byte (0x80 and up) takes the next byte
 * with it, so both the scan for the end of dst and the copy step over whole characters. */
void eucStrcat(unsigned char *dst, unsigned char *src)
{
    unsigned char ch;
    unsigned char lead;

    if (*dst != 0)
    {
        while (*dst != 0)
        {
            if ((signed char) *dst < 0)
            {
                dst++;
            }
            dst++;
        }
        ch = *src;
    }
    else
    {
        ch = *src;
    }
    lead = ch;
    while (lead != 0)
    {
        if ((signed char) lead < 0)
        {
            *dst = lead;
            dst++;
            src++;
            ch = *src;
        }
        *dst = ch;
        dst++;
        src++;
        lead = *src;
        ch = lead;
    }
    *dst = 0;
}

/* Counts the bytes of the EUC-JP string text before its terminator. A lead byte (sign bit set) takes its trail
 * byte with it, so each two-byte character counts two. */
int eucStrLength(unsigned char *text)
{
    int charCount;
    unsigned char ch;

    charCount = 0;
    ch = *text;
    while (ch != 0) {
        if ((signed char) ch < 0) {
            text++;
            charCount++;
        }
        text++;
        charCount++;
        ch = *text;
    }
    return charCount;
}

/* One item of the save and config menus. saveUiOptionItem is one of these too: the option item draws its text, the
 * toggle (sub.cursor) and its animation step, which saveOptionItemFadeOut sets in flag. */
typedef struct SaveItem {
    unsigned char *text;
    ListCursor sub;           /* +0x04: initialised by cursorBufferInit and set by cursorBufferSetOffset; saveItemGetValue reads its cursor */
    signed char flag;
} SaveItem;

void saveItemInit(SaveItem *item, int value, unsigned char *text) {
    item->text = text;
    cursorBufferInit(&item->sub, 2, 2, 0);
    cursorBufferSetOffset(&item->sub, value);
    item->flag = 1;
}

int saveItemGetValue(SaveItem *item) {
    return item->sub.cursor;
}


/* Draws one option item at (origin_x, origin_y): its label, then the image for the toggle (0x208 when the
 * toggle is clear, 0x209 when it is set), and advances the animation step while it is non-zero. */
void saveOptionItemDraw(SaveItem *item, int origin_x, int origin_y)
{
    int label_x;
    int value_x;
    int magnitude;
    signed char anim_step;
    unsigned char step;
    unsigned char *first_image;
    unsigned char *second_image;

    label_x = origin_x + 4;
    anim_step = item->flag;
    magnitude = anim_step;
    if (anim_step <= -1)
    {
        magnitude = -magnitude;
    }
    value_x = origin_x + 0x3C;
    if (anim_step != 0)
    {
        fontObjSetOpacity(magnitude * 0x10);
        panelDrawNineSlice(menuPanelFrame, origin_x, origin_y, 0xB6, 0x68);
        menuDrawTitledFrame(label_x, origin_y + 5, 0xAE, 0x26, saveSelectTitleText);
        menuDrawShortOutlineFrame(label_x, origin_y + 0x2D, 0xAE, 0x36);
        textDrawSegment(0, 0, saveScreenStyleText, 0);
        textDrawSegment(0, 0, saveOptionZoomText, 0);
        textDrawSegment(origin_x + 0x10, origin_y + 0x13, item->text, 4);
        first_image = menuMainTxdEntry(0x208);
        textDrawSegment(value_x, origin_y + 0x32, first_image, (1 - item->sub.cursor) | 4);
        second_image = menuMainTxdEntry(0x209);
        textDrawSegment(value_x, origin_y + 0x48, second_image, item->sub.cursor | 4);
        fontObjResetOpacity();
        step = item->flag;
        if (step & 7)
        {
            item->flag = (signed char) (step + 1);
        }
    }
}

void saveOptionItemFadeOut(SaveItem *item) {
    item->flag = -7;
}

/* Four 2-byte marks, each in an 8-byte slot: a full-width zero in Shift-JIS (82 4F) and in EUC-JP (A3 B0), two ASCII
 * '#' and the ASCII digits '1' and '2'. */
extern unsigned char saveChoiceMarkSjisZero[8];
extern unsigned char saveChoiceMarkEucZero[8];
extern unsigned char saveChoiceMarkHashText[8];
extern unsigned char saveChoiceDigitText[8];

/* Finds the first of the four marks in `text` (a 0x07-prefixed byte counts for the last one) and replaces it
 * with 0x07 and the byte of the last mark chosen by `side & 1`; double-byte characters are stepped over. */
void saveMessageSetChoice(unsigned char *text, int choice)
{
    int ch;
    int leadByte;
    unsigned char *mark;

    leadByte = text[0];
    mark = text;
    ch = leadByte;
    while (ch != 0)
    {
        ch = (unsigned char) ch;
        if ((ch == saveChoiceMarkSjisZero[0] && mark[1] == saveChoiceMarkSjisZero[1]) ||
            (ch == saveChoiceMarkEucZero[0] && mark[1] == saveChoiceMarkEucZero[1]) ||
            (ch == saveChoiceMarkHashText[0] && mark[1] == saveChoiceMarkHashText[1]) ||
            (ch == 7 && (mark[1] == saveChoiceDigitText[0] || mark[1] == saveChoiceDigitText[1])))
        {
            mark[0] = 7;
            mark[1] = saveChoiceDigitText[choice & 1];
            return;
        }
        if ((signed char) leadByte < 0)
        {
            mark += 1;
        }
        mark += 1;
        ch = mark[0];
        leadByte = ch;
    }
}

/* One row of the slot list: seven words, the first is its draw value. Rows are at saveSlotRows, stride 0x1C. */
typedef struct SaveSlotRow {
    int value;
    unsigned char unmodeled_04[0x1C - 0x04];
} SaveSlotRow;

extern unsigned char saveListScroll;
/* The save screen's list cursor in its 0x18-byte slot (the word after the ListCursor is not accessed). */
static struct {
    ListCursor list;
    int unmodeled_14;
} saveCursor;

/* Lays out the 20 slot rows: the cursor row (saveCursor.list.cursor) gets 0xC, the three rows from `band` get 0x1E,
 * every other row 0x200. */
void saveSlotRowsLayout(void)
{
    SaveSlotRow *row;
    unsigned char band;
    int index;

    row = (SaveSlotRow *) (saveSlotRows + 8);
    band = saveListScroll;
    index = 0;
    do
    {
        if (index == saveCursor.list.cursor)
        {
            row->value = 0xC;
        }
        else if ((index >= (signed char) band) && (index < (signed char) band + 3))
        {
            row->value = 0x1E;
        }
        else
        {
            row->value = 0x200;
        }
        index += 1;
        row += 1;
    }
    while (index < 0x14);
}

#include "main/gr_screen.h"

typedef struct SaveUiScrollView {
    short scroll : 8;
} SaveUiScrollView;

/* Statics of the save screen in .bss, in layout order (inner labels such as saveSlotListTrackTargetX are fields of these). */
static struct FontTrack saveSlotListTrack;
static MsgRange saveMessageRange[1];
static struct FontTrack saveMessagePanelTrack;
static int saveCardRequestResult[4];
static signed char saveHeaderText[32];
static SaveItem saveUiOptionItem[1];
extern unsigned char saveIsLoad;
extern unsigned char savePort;
extern signed char saveSlotIndex;
extern unsigned char saveUiMode;
extern unsigned char saveUiState;

typedef struct SaveUiStateByte {
    short state : 8;
} SaveUiStateByte;

extern signed char saveMessageState;
extern signed char saveModePendingFlag;
extern unsigned char saveHeaderSeparatorText[];

/* Sets the save-screen mode (0, 2, 3 or 4; other values only reach the tail). Mode 2 keeps the three-row band
 * (saveListScroll) around the cursor; mode 3 resets the cursor row and the option item. The tail records the mode
 * in saveUiMode and clears the pending flag saveModePendingFlag, or saveMessageState when no flag was pending. */
void saveUiSetMode(unsigned char mode)
{
    switch (mode) {
    case 0:
        cursorBufferInit(&saveCursor.list, 3, 3, 0);
        cursorBufferSetOffset(&saveCursor.list, savePort);
        if (saveModePendingFlag != 0) {
            saveMessageState = 2;
            saveMessageSetChoice(menuMainTxdEntry(0x21F), saveCursor.list.cursor);
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(0x21F));
        } else {
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(saveIsLoad + 0x213));
        }
        fontTrackSetTarget(&saveMessagePanelTrack, 0, 0x14);
        eucStrClear(saveHeaderText);
        eucStrcat(saveHeaderText, menuMainTxdEntry(saveIsLoad + 0x200));
        saveUiState = 0;
        break;
    case 2:
        if (saveUiMode < 2) {
            cursorBufferInit(&saveCursor.list, 20, 20, 0);
            saveCursor.list.mode_flags &= ~0x100;
            cursorBufferSetOffset(&saveCursor.list, saveSlotIndex);
            while ((signed char) saveSlotIndex < (signed char) saveListScroll) {
                saveListScroll = (signed char) saveListScroll - 1;
            }
            while ((signed char) saveListScroll + 2 < (signed char) saveSlotIndex) {
                saveListScroll++;
            }
            fontTrackSetBoth(&saveSlotListTrack, 0x200, -(signed char) saveListScroll * 0x73);
            saveSlotListTrack.target_pair.x = 0;
            saveSlotRowsLayout();
            eucStrcat(saveHeaderText, saveHeaderSeparatorText);
            eucStrcat(saveHeaderText, menuMainTxdEntry(savePort + 0x202));
            saveUiState = 0x10;
            saveSlotsDeactivateAll();
        } else {
            int cursorValue = 0xC;
            SaveUiStateByte nextState;
            nextState.state = 0x12;
            saveSlotListTrack.target_pair.x = 0;
            saveSlotListTrack.target_pair.y = -((SaveUiScrollView *) &saveListScroll)->scroll * 0x73;
            *(SaveUiStateByte *) &saveUiState = nextState;
            ((SaveSlotRow *) (saveSlotRows + 8))[saveCursor.list.cursor].value = cursorValue;
        }
        saveMessageSetChoice(menuMainTxdEntry(0x220), savePort);
        if (saveMessageRange[0].text != menuMainTxdEntry(0x21C)) {
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(0x220));
        }
        fontTrackSetTarget(&saveMessagePanelTrack, 0, 0x28);
        break;
    case 3:
        saveSlotListTrack.target_pair.x = 0x200;
        saveSlotListTrack.target_pair.y = -saveCursor.list.cursor * 0x73;
        saveSlotListTrack.step_count = 1;
        ((SaveSlotRow *) (saveSlotRows + 8))[saveCursor.list.cursor].value = -0x1F4;
        saveItemInit(saveUiOptionItem, 1 - saveIsLoad, menuMainTxdEntry(0x20A));
        if (saveSlotTable[saveCursor.list.cursor].state == mode) {
            saveMessageSetChoice(menuMainTxdEntry(0x224), savePort);
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(0x224));
        } else {
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(saveIsLoad + 0x217));
        }
        fontTrackSetTarget(&saveMessagePanelTrack, 0, 0x14);
        saveUiState = 0;
        break;
    case 4:
        saveUiState = 0;
        if (saveIsLoad != 0) {
            saveMessageSetChoice(menuMainTxdEntry(0x22F), savePort);
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(0x22F));
        } else {
            saveMessageSetChoice(menuMainTxdEntry(0x227), savePort);
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(0x227));
        }
        break;
    }
    saveUiMode = mode;
    if (saveModePendingFlag != 0) {
        saveModePendingFlag = 0;
    } else {
        saveMessageState = 0;
    }
}

extern unsigned char saveCardBusy;

/* Starts the save screen for option (saveIsLoad): resets the message range and the option item, lays out the
 * 20 slot rows (each a draw value stepped by 0x73 from 0x35), and clears the pads and the pending flags. */
void saveUiInit(int isLoad)
{
    int i;
    int trackTarget;

    saveIsLoad = isLoad;
    menuResetState(saveMessageRange);
    fontTrackSetBoth(&saveMessagePanelTrack, 0, 0x14);
    saveOptionItemFadeOut(saveUiOptionItem);
    saveUiOptionItem[0].flag = 0;
    saveListScroll = saveSlotIndex;
    if ((signed char) saveSlotIndex >= 0x12) {
        saveListScroll = 0x11;
    }
    trackTarget = 0x35;
    saveSlotRows = xglCdSectorAlloc(0x230);
    saveSlotRowsLayout();
    for (i = 0; i < 20; i++) {
        unsigned char *track = saveSlotRows + i * 0x1C;

        fontTrackSetBoth((struct FontTrack *) track, ((SaveSlotRow *) (track + 8))->value, trackTarget);
        trackTarget += 0x73;
    }
    fontTrackSetBoth(&saveSlotListTrack, 0x200, 0);
    eucStrClear(saveHeaderText);
    saveUiSetMode(0);
    mcPortResult[0] = mcPortResult[1] = 0;
    mcPortResult[2] = 0;
    saveCardBusy = 0;
    saveProgressFrame = 0;
    saveProgressTotal = 0;
    saveModePendingFlag = 0;
    xglPadSetRepeat(0, 0xF00F, 8, 4);
}


/* Draws the save screen frame and its three rows; row 0 is highlighted while saveCursor selects it. */
void savePortSelectDraw(void)
{
    int index;
    int flags;
    unsigned short cardResult;

    if (saveUiState >= 0x11) {
        fontObjSetOpacity(0x20);
    }
    if (saveMessageState >= 2) {
        fontObjSetOpacity(0);
    }
    panelDrawNineSlice(menuPanelFrame, 0xA8, 0xA2, 0xAF, 0x5B);
    menuDrawTitledFrame(0xAC, 0xA7, 0xA7, 0x51, saveSelectTitleText);
    index = 0;
    for (; index < 3; index++) {
        flags = (index == saveCursor.list.cursor);
        if (index < 2) {
            cardResult = mcPortResult[index];
            if (((unsigned int) (cardResult - 1) >= 2) && ((short) cardResult != 0x80)) {
                flags |= 2;
            }
        }
        textDrawSegment(0xB6, 0xB6 + index * 0x14, menuMainTxdEntry(index + 0x202), flags | 4);
    }
    fontObjResetOpacity();
    xglFontFlush();
    fontObjResetAndBuild();
    saveOptionItemDraw(saveUiOptionItem, 0xA5, 0x10A);
}

#include "gs.h"

typedef struct XglPacket XglPacket;


/* The direct-data block at saveThumbnailDirect: sceVif1PkAddDataN adds its first 28 bytes to the packet. The words at 0x40 and
 * 0x60 are set on each call before that add. */
typedef struct DirectDataBlock {
    u32 data_words[7];
    unsigned char unmodeled_1C[0x40 - 0x1C];
    u64 frame_reg_40;
    unsigned char unmodeled_48[0x60 - 0x48];
    u32 ref_count;
    unsigned char unmodeled_64[0x70 - 0x64];
} DirectDataBlock;

extern DirectDataBlock saveThumbnailDirect;

/* Builds one textured sprite into the current packet with its corners at (x, y) and the 12.4 offsets the GS expects. */
void saveThumbnailSprite(int spriteX, int spriteY, const void *environment)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    saveThumbnailDirect.frame_reg_40 = ((u64) 0x4D << 32) | 0x58;
    saveThumbnailDirect.ref_count = 0x790;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, &saveThumbnailDirect, 28);
    sceVif1PkRef(packet, environment, 0x790, 0, 0x51000790, 0);
    nmlPacketGsInit();
    packet_gs_entry64(GS_TEXFLUSH, 0);
    packet_gs_entry64(GS_TEST_1, GS_TEST_ZTE_ALWAYS);
    packet_gs_entry64(GS_TEX0_1, GS_SET_TEX0(0x3800, 8, GS_PSMCT32, 9, 9, 1, 0, 0, 0, 0, 0, 0));
    packet_gs_entry64(GS_CLAMP_1, GS_SET_CLAMP(1, 1, 0, 0, 0, 0));
    packet_gs_entry64(GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
    packet_gs_entry64(GS_ALPHA_1, GS_SET_ALPHA(0, 1, 1, 1, 0x80));
    packet_gs_entry64(GS_RGBAQ, GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x3F800000));
    packet_gs_entry64(GS_PRIM, GS_PRIM_TEXTURED_SPRITE);
    packet_gs_entry64(GS_UV, 0);
    {
        int vertexX = spriteX << 4;
        int vertexY = spriteY << 4;

        vertexX += 0x7000;
        vertexY += 0x7200;

        packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(vertexX, vertexY, 0));
    }
    packet_gs_entry64(GS_UV, GS_SET_UV(0x580, 0x4D0));
    {
        int vertexX = (spriteX << 4) + 0x7580;
        int vertexY = (spriteY << 4) + 0x76D0;

        packet_gs_entry64(GS_XYZ2, GS_SET_XYZ(vertexX, vertexY, 0));
    }
    xglDirectQueueSubmitRef();
}



/* Draws the frame of the slot detail window at (windowX, y): the shadow pieces first, then, when the slot at
 * index holds data (state 2), its detail, otherwise the empty-slot panel, and finally the lit pieces. */
void saveSlotWindowDraw(int windowX, int y, int slot)
{
    int frameX = windowX;
    int innerX = frameX + 16;
    int innerY = y + 16;
    int lowerY = y + 61;
    int footerY;

    menuDrawCell(567, frameX, y, 0);
    menuDrawCell(568, frameX + 72, y, 0);
    menuDrawCell(564, frameX, lowerY, 0);
    menuDrawCell(565, frameX + 72, lowerY, 0);
    menuDrawCellSized(566, frameX, innerY, 88, 45, 256);
    menuDrawCellSized(566, innerX, y, 56, 77, 256);
    /* Byte offsets: the constant slot stride here; pointer indexing (saveSlotTable[slot]) changes the code. */
    if (((SaveSlot *) ((unsigned char *) saveSlotTable + slot * sizeof(SaveSlot)))->state == SAVE_SLOT_LOADED)
    {
        xglFontFlush();
        saveThumbnailSprite(frameX, y, (unsigned char *) saveSlotTable + slot * sizeof(SaveSlot));
        fontObjResetAndBuild();
        textDrawSegment(0, 0, saveScreenStyleText, 0);
    }
    else
    {
        menuDrawCellSized(592, frameX, y, 88, 77, 1);
    }
    footerY = y + 60;
    menuDrawCell(574, frameX, y, 1);
    menuDrawCell(576, frameX + 72, y, 1);
    menuDrawCell(569, frameX, footerY, 1);
    menuDrawCell(571, frameX + 72, footerY, 1);
    menuDrawCellSized(575, innerX, y, 56, 16, 1);
    menuDrawCellSized(570, innerX, footerY, 56, 17, 1);
    menuDrawCellSized(572, frameX, innerY, 16, 44, 1);
    menuDrawCellSized(573, frameX + 72, innerY, 16, 44, 1);
}

extern int saveSlotPanelIdleTable[];
extern int saveSlotPanelSelectedTable[];
extern unsigned char saveSlotLevelLabelText[];
extern unsigned char saveSlotTimeLabelText[];
extern unsigned char saveSlotTimeColonText[];
extern unsigned char saveSlotNoDataText[];

/* Draws the detail panel of save slot `slot` at (x, y): a slot with no data (status below 2) shows a note only;
 * otherwise its member name, its level and play time, and its item rows are drawn. */
void saveSlotDrawDetail(int x, int y, int slot) {
    PartyMember *member;
    XglClock clock;
    int *panel;
    void *image;
    int title_x;
    int bar_x;
    int bar_x2;
    int bar_y;
    int text_y;
    int item_x;
    int row;

    if (slot == saveCursor.list.cursor) {
        panel = saveSlotPanelSelectedTable;
    } else {
        panel = saveSlotPanelIdleTable;
    }
    title_x = x + 0xC;
    panelDrawNineSlice(panel, x, y, 0x1D0, 0x72);
    menuDrawCell(0x26A, title_x, y - 1, 1);
    textDrawDigitsWide(x + 0x36, y, slot + 1, 2);
    textDrawSegment(0, 0, saveScreenStyleText, 0);
    if (saveSlotTable[slot].state < SAVE_SLOT_LOADED) {
        image = menuMainTxdEntry(0x20F);
        textDrawSegment(x + 0xA0, y + 0x34, image, 4);
        return;
    }
    bar_y = y + 0x15;
    bar_x = x + 0x6A;
    bar_x2 = x + 0x72;
    text_y = y + 0x18;
    menuDrawShortOutlineFrame(bar_x, bar_y, 0x15E, 0x53);
    menuDrawCellSized(0x2A0, bar_x, bar_y, 0x15E, 0x16, 0x800);
    menuDrawCell(0x29A, bar_x, bar_y, 1);
    menuDrawCellSized(0x29B, bar_x2, bar_y, 0x14E, 0x16, 0x801);
    menuDrawCell(0x29C, x + 0x1C0, bar_y, 1);
    menuDrawCell(0x29D, bar_x, bar_y, 0);
    menuDrawCellSized(0x29E, bar_x2, bar_y, 0x14E, 0x16, 0x800);
    menuDrawCell(0x29F, x + 0x1C0, bar_y, 0);
    saveSlotWindowDraw(title_x, text_y, slot);
    switch (saveSlotTable[slot].state) {
    case SAVE_SLOT_LOADED:
        menuDrawCell(0x252, x + 0x76, y + 0x2E, 0);
        menuDrawCell(0x255, x + 0x76, y + 0x2E, 1);
        menuDrawCell(0x252, x + 0x76, y + 0x49, 0);
        menuDrawCell(0x255, x + 0x76, y + 0x49, 1);
        if (saveSlotTable[slot].title_text != 0) {
            textDrawSegment(x + 0x78, y + 0x31, saveSlotTable[slot].title_text, 4);
        }
        if (saveSlotTable[slot].subtitle_text != 0) {
            textDrawSegment(x + 0x78, y + 0x4C, saveSlotTable[slot].subtitle_text, 4);
        }
        fontObjSetColor(0x3F80000080808080ULL);
        if (saveSlotTable[slot].member_id != 0) {
            member = ptMemberRecordFindById(saveSlotTable[slot].member_id);
            if (member != 0) {
                textDrawShadow(x + 0x78, text_y, member->status->text, 0xFE00ULL << 46);
            }
            textDrawShadow(x + 0xD0, text_y, saveSlotLevelLabelText, 0xFE00ULL << 46);
            textDrawDecimalShadow(x + 0xF0, text_y, saveSlotTable[slot].level, 2, 0xFE00ULL << 46);
        }
        textDrawShadow(x + 0x138, text_y, saveSlotTimeLabelText, 0xFE00ULL << 46);
        xglClockUInt2DayTime(&clock, saveSlotTable[slot].play_time);
        PartyTimeDispChange(&clock);
        textDrawDecimalShadow(x + 0x16E, text_y, clock.hour, 2, 0xFE00ULL << 46);
        textDrawShadow(x + 0x180, text_y, saveSlotTimeColonText, 0xFE00ULL << 46);
        textDrawDecimalShadowZero(x + 0x18A, text_y, clock.minute, 2, 0xFE00ULL << 46);
        textDrawShadow(x + 0x19C, text_y, saveSlotTimeColonText, 0xFE00ULL << 46);
        textDrawDecimalShadowZero(x + 0x1A6, text_y, clock.second, 2, 0xFE00ULL << 46);
        fontObjResetColor();
        row = 0;
        item_x = x + 0x114;
        do {
            if (saveSlotTable[slot].row_value[row] != 0) {
                menuDrawLitIcon(item_x, y + 0x47, saveSlotTable[slot].row_value[row] - 1);
            }
            item_x += 0x18;
            row++;
        } while (row < 7);
        if (saveSlotTable[slot].flag != 0) {
            textDrawSegment(x + 0x12C, y + 0x2E, saveSlotDetailDimColorText, 0);
            image = menuMainTxdEntry(0x236);
            textDrawSegment(x + 0x13E, y + 0x2E, image, 0);
            textDrawSegment(x + 0x12C, y + 0x2E, saveSlotDetailColorText, 0);
        }
        break;
    case SAVE_SLOT_EMPTY:
        if (saveIsLoad != 0) {
            image = menuMainTxdEntry(0x211);
            textDrawSegment(x + 0x78, y + 0x16, image, 4);
        } else {
            image = menuMainTxdEntry(0x210);
            textDrawSegment(x + 0x78, y + 0x16, image, 4);
        }
        fontObjSetColor(0x3F80000080808080ULL);
        textDrawShadow(x + 0x1A, y + 0x38, saveSlotNoDataText, 0xFE00ULL << 46);
        fontObjResetColor();
        break;
    case SAVE_SLOT_BAD:
    case 5:
        image = menuMainTxdEntry(0x238);
        textDrawSegment(x + 0x78, y + 0x16, image, 4);
        break;
    }
}


/* The eased position track of slot row `index`: rows are FontTracks at saveSlotRows, stride 0x1C. */
#define SAVE_SLOT_TRACK(index) (((struct FontTrack *) saveSlotRows)[index])

/* Draws the save slot list: two scroll arrows while the list track is near the left edge, then every row that is on
 * screen, each at the eased position of its track plus the list track. The cursor row keeps at least 0xC of x. A row
 * whose table entry is neither 1 nor 0xFF clears its slot state. */
void saveSlotListDraw(void)
{
    int x;
    int y;
    int index;
    short cardResult;

    if (saveSlotListTrack.current_pair.x < 0x101)
    {
        if ((signed char) saveListScroll > 0)
        {
            menuDrawCell(0x26C, 0x1DD, 0x26, 1);
        }
        if ((signed char) saveListScroll < 0x11)
        {
            menuDrawCell(0x26B, 0x1DD, 0x188, 1);
        }
    }
    fontTrackEase(&saveSlotListTrack);
    for (index = 0; index < SAVE_SLOT_COUNT; index++)
    {
        fontTrackEase(&SAVE_SLOT_TRACK(index));
        x = saveSlotListTrack.current_pair.x + ((SAVE_SLOT_TRACK(index).current_pair.x < 0x1F) ? SAVE_SLOT_TRACK(index).current_pair.x : 0x1E);
        y = saveSlotListTrack.current_pair.y + SAVE_SLOT_TRACK(index).current_pair.y;
        if (index == saveCursor.list.cursor)
        {
            x = (x < 0xC) ? 0xC : x;
        }
        if (x >= 0x200 || SAVE_SLOT_TRACK(index).current_pair.x >= 0x1F0)
        {
            continue;
        }
        if (SAVE_SLOT_TRACK(index).current_pair.x >= 0x1F)
        {
            fontObjSetOpacity(~(SAVE_SLOT_TRACK(index).current_pair.x >> 2) & 0x7F);
        }
        cardResult = mcPortResult[savePort];
        if (cardResult != 1 && cardResult != 0xFF)
        {
            saveSlotTable[index].state = SAVE_SLOT_NONE;
        }
        saveSlotDrawDetail(x, y, index);
        fontObjResetOpacity();
    }
}


void saveUiOptionDraw(void) {
    saveOptionItemDraw(saveUiOptionItem, 0xA5, 0xCE);
}

extern unsigned char saveScreenZoomText[];
extern void (*saveUiDrawTable[])(void);

/* Draws the save screen: the header text and progress panel, the 64 fading background rows, the four option
 * labels chosen by saveIsLoad, the slot list, an optional page hook from saveUiDrawTable, the text panel sized from the
 * character count of saveHeaderText, and the lower status line. */
void saveScreenDraw(void)
{
    int row;
    u64 alpha;
    int pixel_x;
    int text_width;
    int chars;

    fontObjResetAndBuild();
    saveProgressDraw();
    textDrawSegment(0, 0, saveScreenStyleText, 0);
    textDrawSegment(0, 0, saveScreenZoomText, 0);
    fontObjSetColor(0x3F80000080381A1AULL);
    row = 0;
    alpha = 0;
    do
    {
        fontObjSetAlpha(alpha++);
        pixel_x = row << 3;
        row += 1;
        menuDrawCellSized(0x29, pixel_x, 0x22, 8, 0x10, 5);
        menuDrawCellSized(0x26, 0x1D2, 0x200 - pixel_x, 0x10, 8, 5);
    } while (row < 0x40);
    menuDrawCell(saveIsLoad ? 0x28B : 0x292, 0x14F, 0xD, 1);
    menuDrawCell(saveIsLoad ? 0x28E : 0x280, 0x172, 0xD, 1);
    menuDrawCell(saveIsLoad ? 0x280 : 0x295, 0x195, 0xD, 1);
    menuDrawCell(saveIsLoad ? 0x283 : 0x284, 0x1B8, 0xD, 1);
    saveSlotListDraw();
    xglFontFlush();
    fontObjResetAndBuild();
    textDrawSegment(0, 0, saveScreenStyleText, 0);
    textDrawSegment(0, 0, saveScreenZoomText, 0);
    if (saveUiDrawTable[saveUiMode] != 0)
    {
        saveUiDrawTable[saveUiMode]();
        xglFontFlush();
        fontObjResetAndBuild();
        textDrawSegment(0, 0, saveScreenStyleText, 0);
        textDrawSegment(0, 0, saveScreenZoomText, 0);
    }
    chars = eucStrLength(saveHeaderText) >> 1;
    text_width = (chars << 4) - chars;
    panelDrawNineSlice(menuPanelFrame, 4, 8, text_width + 0x22, 0x25);
    menuDrawShortOutlineFrame(8, 0xD, text_width + 0x1A, 0x1B);
    textDrawSegment(0, 0, saveOptionZoomText, 0);
    textDrawSegment(0x14, 0x10, saveHeaderText, 0);
    fontTrackEase(&saveMessagePanelTrack);
    if (saveMessageState != 0)
    {
        menuDrawKindIcon(0x1A0, 0x17E - saveMessagePanelTrack.current_pair.y, 3);
        menuDrawKindIcon(0x1B0, 0x17E - saveMessagePanelTrack.current_pair.y, 0);
        textDrawSegment(0x1C0, 0x17E - saveMessagePanelTrack.current_pair.y, menuMainTxdEntry(0xE3), 0);
    }
    fontObjSetColorRgba(0x10, 0x10, 0x10, 0x40);
    menuDrawCellSized(0x33, 0, 0x198 - saveMessagePanelTrack.current_pair.y, 0x200,
                          saveMessagePanelTrack.current_pair.y + 0x1D, 0x104);
    menuDrawBorder(-0x10, 0x198 - saveMessagePanelTrack.current_pair.y, 0x220,
                   saveMessagePanelTrack.current_pair.y + 0x1D, 0x3F80000040381A1AULL);
    fontObjResetColor();
    xglFontFlush();
    grSetScissor(0, 0x199 - saveMessagePanelTrack.current_pair.y, 0x1FF, 0x1B2);
    msgRangeDraw(saveMessageRange, 0x18, 0x19C - saveMessagePanelTrack.current_pair.y);
    xglFontFlush();
    grResetScissor();
}


void saveScreenDrawCallback(void) {
    saveScreenDraw();
}

#include "main/pad.h"
#include "main/xgl_dma.h"

extern signed char saveOptionPromptFlag;
extern ListCursor saveOptionItemCursor[]; /* = saveUiOptionItem.sub, a label of its own at 0x6ABF9C */

int savePortSelectUpdate(void) {
    signed char optionFlag;

    switch (saveUiState) {
    case 0:
        if (saveMessageState != 0) {
            if (saveMessageState >= 2) {
                if (padCirclePressedAlone() || padCrossPressedAlone()) {
                    saveMessageState = 1;
                    if (saveMessageRange[0].text != menuMainTxdEntry(saveIsLoad + 531)) {
                        msgRangeSetText(saveMessageRange, menuMainTxdEntry(saveIsLoad + 531));
                    }
                    seVoiceStartDefault(SAVE_SE_ACCEPT);
                } else if (saveCursor.list.cursor < 2) {
                    if (mcPortResult[saveCursor.list.cursor] == 1) {
                        saveMessageState = 1;
                        if (saveMessageRange[0].text != menuMainTxdEntry(saveIsLoad + 531)) {
                            msgRangeSetText(saveMessageRange, menuMainTxdEntry(saveIsLoad + 531));
                        }
                    }
                }
                break;
            }
            saveMessageState = 0;
            break;
        }
        if (PadData[0].directionRepeated & PAD_UP) {
            cursorBufferMoveBy(&saveCursor.list, -1);
        }
        if (PadData[0].directionRepeated & PAD_DOWN) {
            cursorBufferMoveBy(&saveCursor.list, 1);
        }
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(SAVE_SE_CANCEL);
            optionFlag = saveOptionPromptFlag;
            if (optionFlag != 0) {
                saveItemInit(saveUiOptionItem, 1, menuMainTxdEntry(525));
                saveUiState = 30;
                break;
            }
            return 1;
        }
        if (!padCirclePressedAlone()) {
            break;
        }
        if (saveMessageState != 0) {
            break;
        }
        if (saveCursor.list.cursor == 2) {
            seVoiceStartDefault(SAVE_SE_ACCEPT);
            optionFlag = saveOptionPromptFlag;
            if (optionFlag != 0) {
                saveItemInit(saveUiOptionItem, 1, menuMainTxdEntry(525));
                saveUiState = 30;
                break;
            }
            return 1;
        }
        if (mcPortResult[saveCursor.list.cursor] == 1 || mcPortResult[saveCursor.list.cursor] == 2) {
            if (mcPortResult[saveCursor.list.cursor] == 2) {
                if (saveIsLoad != 0) {
                    saveMessageSetChoice(menuMainTxdEntry(553), saveCursor.list.cursor);
                    msgRangeSetText(saveMessageRange, menuMainTxdEntry(553));
                    saveMessageState = 2;
                    seVoiceStartDefault(SAVE_SE_DENY);
                    break;
                }
                saveMessageSetChoice(menuMainTxdEntry(546), saveCursor.list.cursor);
                msgRangeSetText(saveMessageRange, menuMainTxdEntry(546));
                saveUiState = 17;
                saveItemInit(saveUiOptionItem, 1, menuMainTxdEntry(523));
            } else {
                savePort = saveCursor.list.cursor;
                saveUiSetMode(2);
            }
            seVoiceStartDefault(SAVE_SE_ACCEPT);
            break;
        }
        if (saveCursor.list.cursor < 2) {
            saveMessageSetChoice(menuMainTxdEntry(543), saveCursor.list.cursor);
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(543));
        } else {
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(565));
        }
        seVoiceStartDefault(SAVE_SE_DENY);
        saveMessageState = 2;
        break;
    case 0x11:
        if (PadData[0].directionRepeated & PAD_UP) {
            cursorBufferMoveBy(saveOptionItemCursor, -1);
        }
        if (PadData[0].directionRepeated & PAD_DOWN) {
            cursorBufferMoveBy(saveOptionItemCursor, 1);
        }
        if (mcPortResult[saveCursor.list.cursor] != 2) {
            saveMessageState = 2;
            saveMessageSetChoice(menuMainTxdEntry(543), saveCursor.list.cursor);
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(543));
            saveUiState = 0;
            saveOptionItemFadeOut(saveUiOptionItem);
            break;
        }
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(SAVE_SE_CANCEL);
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(saveIsLoad + 531));
            saveUiState = 0;
            saveOptionItemFadeOut(saveUiOptionItem);
            break;
        }
        if (!padCirclePressedAlone()) {
            break;
        }
        seVoiceStartDefault(SAVE_SE_ACCEPT);
        if (saveItemGetValue(saveUiOptionItem) == 0) {
            xglMcRequest(saveCursor.list.cursor, MC_REQ_FORMAT, 0, saveCardRequestResult);
            saveMessageSetChoice(menuMainTxdEntry(539), saveCursor.list.cursor);
            fontTrackSetTarget(&saveMessagePanelTrack, 0, 40);
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(539));
            saveUiState = 0x12;
            saveCardBusy = 1;
        } else {
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(saveIsLoad + 531));
            saveUiState = 0;
        }
        saveOptionItemFadeOut(saveUiOptionItem);
        break;
    case 0x12:
        if (mcPortResult[saveCursor.list.cursor] != 0xFF) {
            saveUiState = 0;
            if (mcPortResult[saveCursor.list.cursor] == 2) {
                do {
                    *(int *) mcPortResult = xglMcMain();
                } while (mcPortResult[saveCursor.list.cursor] == 2);
            }
            if (mcPortResult[saveCursor.list.cursor] == 1) {
                savePort = saveCursor.list.cursor;
                saveUiSetMode(2);
                saveCardBusy = 0;
            } else {
                saveMessageSetChoice(menuMainTxdEntry(541), saveCursor.list.cursor);
                msgRangeSetText(saveMessageRange, menuMainTxdEntry(541));
                saveMessageState = 2;
                saveCardBusy = 0;
            }
        }
        break;
    case 0x1E:
        if (PadData[0].directionRepeated & PAD_UP) {
            cursorBufferMoveBy(saveOptionItemCursor, -1);
        }
        if (PadData[0].directionRepeated & PAD_DOWN) {
            cursorBufferMoveBy(saveOptionItemCursor, 1);
        }
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(SAVE_SE_CANCEL);
            saveUiState = 0;
            saveOptionItemFadeOut(saveUiOptionItem);
            break;
        }
        if (!padCirclePressedAlone()) {
            break;
        }
        seVoiceStartDefault(SAVE_SE_ACCEPT);
        if (saveItemGetValue(saveUiOptionItem) == 0) {
            return 1;
        }
        saveUiState = 0;
        saveOptionItemFadeOut(saveUiOptionItem);
        break;
    default:
        break;
    }
    return 0;
}


void saveSlotFromData(int slot, unsigned char *data)
{
    JpegDecodeControl decode;
    unsigned char *src;
    int i;

    memset(&decode, 0, sizeof(decode));
    decode.source = data + 372;
    /* The decoded thumbnail is written at the start of the slot record. */
    decode.destination = (unsigned char *) (saveSlotTable + slot);
    xglJpegDecode(&decode);
    i = 0;
    src = data + 20696;
    saveSlotTable[slot].member_id = data[20694];
    saveSlotTable[slot].level = data[20695];
    for (; i < 7; i++) {
        saveSlotTable[slot].row_value[i] = *src;
        src += 2;
    }
    (saveSlotTable + slot)->play_time = xglClockDayTime2UInt((XglClock *) (data + 104));
    (saveSlotTable + slot)->flag = data[20710];
    saveSlotSetTexts(slot, *(int *) (data + 11928));
}

enum {
    SAVE_IMAGE_MARKER = 0xC8,
    SAVE_IMAGE_SIZE = 0x5160,
};


/* Slot request handler: -2 sets saveUiState to 0x12, -1 returns saveCursor.list.cursor + 1, -3 writes state 0 to record
 * slot - 1 = -4, four records before the table (an original bug: no real record is cleared). For slots 1..20 the image in data is checked: a matching checksum and marker load the
 * record (state 2, then saveSlotFromData), anything else sets state 4. Returns slot + 1, or -1 when nothing ran. */
int saveSlotScanCallback(int slot, unsigned char *data)
{
    unsigned long long computed_checksum;
    int result;

    result = -1;
    switch (slot) {
    case -1:
        result = saveCursor.list.cursor + 1;
        break;
    case -2:
        saveUiState = 0x12;
        break;
    case -3:
        (saveSlotTable + (slot - 1))->state = SAVE_SLOT_NONE;
        break;
    default:
        if ((unsigned int) (slot - 1) < SAVE_SLOT_COUNT) {
            saveChecksum((struct SaveData *) data, &computed_checksum);
            if (((struct SaveData *) data)->checksum == computed_checksum
                && ((struct SaveData *) data)->version == SAVE_IMAGE_MARKER) {
                (saveSlotTable + (slot - 1))->state = SAVE_SLOT_LOADED;
                saveSlotFromData(slot - 1, data);
                memset(data, 0, SAVE_IMAGE_SIZE);
            } else {
                (saveSlotTable + (slot - 1))->state = SAVE_SLOT_BAD;
            }
            result = slot + 1;
        }
        break;
    }
    return result;
}

extern int saveSlotListTrackTargetX[];
extern int saveSlotListTrackTargetY[];
extern int saveCardFreeSpace;
extern int saveMessageTimer;

int saveSlotListUpdate(void)
{
    McRequestParam request;
    short cardResult;
    int slotState;

    cardResult = mcPortResult[savePort];
    if (cardResult != 1 && cardResult != 0xFF)
    {
        saveSlotListTrackTargetX[0] = 0x200;
        saveModePendingFlag = 1;
        saveUiSetMode(0);
        if (saveUiState == 0x11)
        {
            xglMcReset();
        }
        saveCardBusy = 0;
        saveSlotsDeactivateAll();
    }
    if (saveMessageState == 0)
    {
        if (PadData[0].directionRepeated & PAD_UP)
        {
            cursorBufferMoveBy(&saveCursor.list, -1);
        }
        else if (PadData[0].directionRepeated & PAD_DOWN)
        {
            cursorBufferMoveBy(&saveCursor.list, 1);
        }
        else if (PadData[0].directionRepeated & PAD_L1)
        {
            cursorBufferMoveBy(&saveCursor.list, -3);
            if ((signed char) ((signed char) saveListScroll - 3) < 0)
            {
                saveListScroll = 0;
            }
            else
            {
                saveListScroll = (signed char) saveListScroll - 3;
            }
        }
        else if (PadData[0].directionRepeated & PAD_R1)
        {
            cursorBufferMoveBy(&saveCursor.list, 3);
            saveListScroll = (signed char) saveListScroll + 3;
            if ((signed char) saveListScroll >= 0x11)
            {
                saveListScroll = 0x11;
            }
        }
        if (saveCursor.list.cursor != saveCursor.list.previous_cursor)
        {
            saveSlotRowsLayout();
            while (saveCursor.list.cursor < (signed char) saveListScroll)
            {
                saveListScroll = (signed char) saveListScroll - 1;
            }
            while ((signed char) saveListScroll + 2 < saveCursor.list.cursor)
            {
                saveListScroll = (signed char) saveListScroll + 1;
            }
            saveSlotListTrackTargetY[0] = -(signed char) saveListScroll * 0x73;
        }
    }
    if (saveMessageTimer != 0)
    {
        saveMessageTimer = saveMessageTimer - 1;
        if (saveMessageTimer == 0)
        {
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(saveIsLoad + 0x215));
            fontTrackSetTarget(&saveMessagePanelTrack, 0, 0);
            saveMessageTimer = 0;
        }
    }
    switch (saveUiState)
    {
    case 10:
        if (padCirclePressedAlone() != 0 || padCrossPressedAlone() != 0)
        {
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(saveIsLoad + 0x215));
            saveUiState = 0;
            fontTrackSetTarget(&saveMessagePanelTrack, 0, 0);
            saveMessageState = 0;
        }
        break;
    case 0:
        if (padCrossPressedAlone() != 0)
        {
            saveSlotListTrackTargetX[0] = 0x200;
            seVoiceStartDefault(SAVE_SE_CANCEL);
            saveUiSetMode(0);
        }
        else if (padCirclePressedAlone() != 0)
        {
            if (saveIsLoad != 0 && saveSlotTable[saveCursor.list.cursor].state != SAVE_SLOT_LOADED)
            {
                seVoiceStartDefault(SAVE_SE_DENY);
            }
            else
            {
                slotState = saveSlotTable[saveCursor.list.cursor].state;
                if (slotState >= 4)
                {
                    seVoiceStartDefault(SAVE_SE_DENY);
                }
                else if (saveIsLoad != 0 || slotState != SAVE_SLOT_EMPTY || saveCardFreeSpace >= 0x22)
                {
                    seVoiceStartDefault(SAVE_SE_ACCEPT);
                    saveUiSetMode(3);
                }
                else
                {
                    saveMessageSetChoice(menuMainTxdEntry(0x223), savePort);
                    msgRangeSetText(saveMessageRange, menuMainTxdEntry(0x223));
                    saveUiState = 0xA;
                    saveMessageState = 2;
                    seVoiceStartDefault(SAVE_SE_DENY);
                    fontTrackSetTarget(&saveMessagePanelTrack, 0, 0x28);
                }
            }
        }
        break;
    case 16:
        request.file = -1;
        request.callback = saveSlotScanCallback;
        request.data = saveScanBuffer;
        request.size = SAVE_IMAGE_SIZE;
        xglMcRequest(savePort, MC_REQ_LIST, &request, saveCardRequestResult);
        saveUiState = 0x11;
        saveSlotsActivateAll();
        saveMessageTimer = 0;
        saveCardBusy = 1;
        break;
    case 18:
        xglMcRequest(savePort, MC_REQ_FREE_SPACE, 0, &saveCardFreeSpace);
        saveUiState = 0x13;
        saveCardBusy = 1;
        saveMessageSetChoice(menuMainTxdEntry(0x220), savePort);
        if (saveMessageRange[0].text != menuMainTxdEntry(0x220))
        {
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(0x220));
            fontTrackSetTarget(&saveMessagePanelTrack, 0, 0x28);
        }
        saveMessageTimer = 0;
        break;
    case 19:
        if (mcPortResult[savePort] == 0xFF)
        {
            break;
        }
        saveUiState = 0x14;
        break;
    case 20:
        saveUiState = 0;
        saveSlotsPendingToEmpty();
        saveCardBusy = 0;
        saveMessageTimer = 0x1E;
        break;
    }
    return 0;
}

extern unsigned char saveOptionItemFlag[];

int saveSlotTransferUpdate(void)
{
    McRequestParam entry_block;   /* MC_REQ_CREATE: the file number and the save image (McCreateData) */
    XglClock clock;
    McRequestParam request;
    signed char scrollRow;
    signed char slotIndex;
    SaveItem *item;
    int cardResult;
    int cardState;
    int result;
    int slot;
    SaveThumbnail *source;
    SaveThumbnail *dest;

    result = 0;
    cardState = mcPortResult[savePort];
    if (cardState != 1 && cardState != 0xFF)
    {
        scrollRow = ((SaveUiScrollView *) &saveListScroll)->scroll;
        saveSlotListTrack.target_pair.y = -scrollRow * 115;
        saveSlotListTrack.target_pair.x = 0;
        ((SaveSlotRow *) (saveSlotRows + 8))[saveCursor.list.cursor].value = 12;
        saveSlotListTrack.target_pair.x = 512;
        if (saveCardBusy != 0)
        {
            saveCardBusy = 0;
            saveUiSetMode(4);
        }
        else
        {
            saveModePendingFlag = 1;
            saveUiSetMode(0);
        }
        if (saveUiOptionItem[0].flag != 0)
        {
            saveOptionItemFadeOut(saveUiOptionItem);
            saveUiOptionItem[0].flag = 0;
        }
        saveProgressFrame = 0;
        saveProgressTotal = 0;
        saveSlotsDeactivateAll();
    }

    switch (saveUiState)
    {
    case 0:
        if (PadData[0].directionRepeated & PAD_UP)
        {
            cursorBufferMoveBy(&saveUiOptionItem[0].sub, -1);
        }
        if (PadData[0].directionRepeated & PAD_DOWN)
        {
            cursorBufferMoveBy(&saveUiOptionItem[0].sub, 1);
        }
        if (padCrossPressedAlone())
        {
            seVoiceStartDefault(SAVE_SE_CANCEL);
            saveUiSetMode(2);
            saveOptionItemFadeOut(saveUiOptionItem);
            saveUiOptionItem[0].flag = 0;
            break;
        }
        if (padCirclePressedAlone())
        {
            seVoiceStartDefault(SAVE_SE_ACCEPT);
            item = saveUiOptionItem;
            saveOptionItemFadeOut(item);
            if (saveItemGetValue(item) == 0)
            {
                saveSlotIndex = saveCursor.list.cursor;
                if (saveIsLoad == 0)
                {
                    signed char cursorSlot = saveCursor.list.cursor;
                    entry_block.file = cursorSlot + 1;
                    entry_block.data = cardDataBuffer;
                    dest = &SaveData.thumbnail;
                    source = saveThumbnailBuffer;
                    *dest = *source;
                    saveWriteImage();
                    clock = SaveData.playTime;
                    PartyTimeDispChange(&clock);
                    xglMcSetEntryNumbers(entry_block.data, entry_block.file, &clock);
                    xglMcRequest(savePort, MC_REQ_CREATE, &entry_block, saveCardRequestResult);
                    saveUiState = 16;
                    msgRangeSetText(saveMessageRange, menuMainTxdEntry(549));
                    saveCardBusy = 1;
                    saveProgressFrame = 0;
                    saveProgressTotal = 0;
                    break;
                }
                slotIndex = saveCursor.list.cursor;
                request.data = &SaveData;
                request.file = slotIndex + 1;
                request.size = SAVE_IMAGE_SIZE;
                slot = savePort;
                xglMcRequest(slot, MC_REQ_LOAD, &request, saveCardRequestResult);
                saveUiState = 20;
                saveCardBusy = 1;
                msgRangeSetText(saveMessageRange, menuMainTxdEntry(557));
                break;
            }
            saveUiSetMode(2);
            item->flag = 0;
        }
        break;
    case 16:
        if (mcPortResult[savePort] != 0xFF)
        {
            saveUiState = 17;
        }
        else
        {
            saveProgressTotal = 73 * (saveCardRequestResult[0] + 1);
            break;
        }
        /* fall through */
    case 17:
        slot = savePort;
        cardResult = mcPortResult[slot];
        if (cardResult == 1)
        {
            request.file = saveSlotIndex + 1;
            request.data = &SaveData;
            request.size = SAVE_IMAGE_SIZE;
            xglMcRequest(slot, MC_REQ_SAVE, &request, saveCardRequestResult);
            saveUiState = 18;
            break;
        }
        scrollRow = saveListScroll;
        saveSlotListTrack.target_pair.y = -scrollRow * 115;
        saveSlotListTrack.target_pair.x = 0;
        ((SaveSlotRow *) (saveSlotRows + 8))[saveCursor.list.cursor].value = 12;
        saveCardBusy = 0;
        saveSlotListTrack.target_pair.x = 512;
        saveUiSetMode(4);
        if (saveUiOptionItem[0].flag != 0)
        {
            saveOptionItemFadeOut(saveUiOptionItem);
            saveUiOptionItem[0].flag = 0;
        }
        saveProgressFrame = 0;
        saveProgressTotal = 0;
        saveSlotsDeactivateAll();
        break;
    case 18:
        if (mcPortResult[savePort] != 0xFF)
        {
            saveUiState = 100;
        }
        break;
    case 20:
        if (mcPortResult[savePort] != 0xFF)
        {
            saveOptionItemFlag[0] = 0;
            msgRangeSetText(saveMessageRange, menuMainTxdEntry(558));
            saveCardBusy = 0;
            result = 2;
        }
        break;
    default:
        if (saveUiState >= 130)
        {
            seVoiceStartDefault(SAVE_SE_ACCEPT);
            saveSlotFromData(saveSlotIndex, (unsigned char *) &SaveData);
            saveSlotTable[saveSlotIndex].state = SAVE_SLOT_LOADED;
            saveUiSetMode(2);
            saveOptionItemFlag[0] = 0;
            saveCardBusy = 0;
            saveProgressFrame = 0;
            saveProgressTotal = 0;
        }
        else
        {
            saveUiState = saveUiState + 1;
        }
        break;
    }
    return result;
}


int saveUiPollButtons(void) {
    saveMessageState = 1;
    if ((padCirclePressedAlone() != 0) || (padCrossPressedAlone() != 0)) {
        saveMessageState = 0;
        saveUiSetMode(0);
    }
    return 0;
}

/* Partial views of the JPEG encoder parameter block (32 bytes, cleared by memset) and of the render state. */

extern unsigned long long saveThumbnailDma[24];
void SyncDCache(void *start, void *end);
int sceGsSyncPath(int mode, int timeout);

enum {
    SAVE_JPEG_FALLBACK_QUALITY = 10,
};

/* Encodes an 88x77 thumbnail from (0x3800, 8) into output_buffer with luma and Cb quality from quality (the
 * fallback when it is not positive) and Cr quality 0. The DMA packet saveThumbnailDma takes the BITBLTBUF
 * word from sRender.draw_buffer_base before the sync and transfer. Returns the encoded size. */
unsigned int saveThumbnailEncode(int quality, void *output_buffer)
{
    JpegEncodeRequest params;
    unsigned long long bitbltWord;

    bitbltWord = 0x24020000 | (sRender.draw_buffer_base << 5);
    saveThumbnailDma[10] = bitbltWord | (0xC800ULL << 19);
    saveThumbnailDma[8] = 0x801C0;
    SyncDCache(saveThumbnailDma, saveThumbnailDma + 24);
    sceGsSyncPath(0, 0);
    xglDmaDirectNormal(2, (u32) saveThumbnailDma, 12);
    sceGsSyncPath(0, 0);

    memset(&params, 0, sizeof(params));
    params.outputBuffer = output_buffer;
    params.frameBufferBase = 0x3800;
    params.frameBufferWidth = 8;
    params.width = 88;
    params.height = 77;
    params.originX = 0;
    params.originY = 0;
    if (quality <= 0) {
        params.luminanceQuality = SAVE_JPEG_FALLBACK_QUALITY;
        params.chrominanceQuality = SAVE_JPEG_FALLBACK_QUALITY;
    } else {
        params.luminanceQuality = quality;
        params.chrominanceQuality = quality;
    }
    params.samplingMode = JPEG_SAMPLING_444;
    xglJpegEncode(&params);
    return params.outputSize;
}



extern int (*saveUiUpdateTable[])(void);
const char cardDataFileName[16] = "carddata.bin";
const char saveTextTxdPath[24] = "yosikawa\\Location.txd";
void sceGsSyncV(int arg);

int MenuFileMain(int entryMode)
{
    unsigned char mode;
    int handlerResult;
    int (*handler)(void);
    int jpegQuality;
    int encodedSize;
    int i;

    mode = entryMode & 0xFF;
    handlerResult = 0;
    if (mode == 0) {
        XglRenderState *frame = &sRender;

        if (*(int *) ((char *) frame + 0x50) != 0) {
            return 1;
        }
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
        grCopyFrame(frame->vram.work_buffer_base, frame->draw_buffer_base);
        pxySkipOverlayDraw();
    }

    gameWork.control |= 0x04000000;
    ClearEnv.value.color_r = 0;
    ClearEnv.value.color_g = 0;
    ClearEnv.value.color_b = 0;
    ClearEnv.value.color_a = 0;
    xglRenderClearFrame();
    if (mode != 0) {
        xglCdArenaOpen((void *) 0x01000000, 0x200000);
    } else {
        xglCdArenaOpen(0, 0x200000);
    }
    xglCdBufferAlloc(0x40800);
    saveScanBuffer = xglCdSectorAlloc(0x7160);
    saveSlotTable = xglCdSectorAlloc(0x97680);
    memset(saveSlotTable, 0, 0x97680);
    cardDataBuffer = xglCdSectorAlloc(0x42000);
    saveThumbnailBuffer = xglCdSectorAlloc(0x7900);
    saveTextTable = xglCdSectorAlloc(0x800);
    if (mode >= 2) {
        mode = 1;
    }
    if (mode == 0 && gameWork.mapId == 0x1B91) {
        saveOptionPromptFlag = 1;
    } else {
        saveOptionPromptFlag = 0;
    }
    saveUiInit(mode);
    if (mode == 0) {
        seVoiceStartDefault(0x2000001);
    }
    fontObjLoadTexture(0);
    fontObjLoadTexture(3);
    bgLayerLoadAll();
    if (mode == 0) {
        memset(saveThumbnailBuffer, 0, 0x7900);
        jpegQuality = 100;
        do {
            encodedSize = saveThumbnailEncode(jpegQuality, saveThumbnailBuffer);
            jpegQuality -= 25;
        } while (encodedSize >= 0x1001);
    }
    if (mode == 0) {
        do {
            sceGsSyncV(0);
            grFillFrame();
            grCopyFrame(sRender.display_buffer_base, sRender.vram.work_buffer_base);
            xglTickAdvance();
            xglSleep();
        } while (xglCdGetServicePending() != 0);
    }

    xglTickAdvance();
    xglSleep();
    xglTickAdvance();
    xglSleep();
    xglCdReadFile(cardDataFileName, cardDataBuffer, 1, CD_NOTIFY_CURRENT);
    xglCdReadFile(saveTextTxdPath, saveTextTable, 1, CD_NOTIFY_CURRENT);
    xglCdWaitIdle();
    fontObjLoadTexture(6);
    bgLayerCameraSetup();
    pxyTickScaleDoubleOverride(1);
    while (xglCdServiceIfPending() != 0) {
        *(int *) mcPortResult = xglMcMain();
        pxyRequestOverlayPass();
        XenoActorSetFlagAll(2);
        bgLayerClearFlags(0);
        bgLayerClearFlags(1);
        bgLayerClearFlags(2);
        XenoActorDoit();
        XenoActorDisp();
        xglTickAdvance();
        xglSleep();
    }
    for (;;) {
        *(int *) mcPortResult = xglMcMain();
        if (handlerResult != 0) {
            break;
        }
        handler = saveUiUpdateTable[saveUiMode];
        if (handler != 0) {
            handlerResult = handler();
        }
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        xglCallbackQueueAdd(saveScreenDrawCallback);
        XenoActorDoit();
        XenoActorDisp();
        xglTickAdvance();
        xglSleep();
    }

    bgLayerResetState();
    bgLayerRelease();
    xglCdBufferClear();
    xglCdArenaClose();
    if ((gameWork.control & 0x10000000) == 0) {
        grCopyFrame(sRender.vram.work_buffer_base, sRender.draw_buffer_base);
        for (i = 0; i < 16; i++) {
            sceGsSyncV(0);
            grFillFrame();
            grCopyFrame(sRender.display_buffer_base, sRender.vram.work_buffer_base);
            xglTickAdvance();
            xglSleep();
        }
        xglTickAdvance();
        xglSleep();
        xglTickAdvance();
        xglSleep();
        xglSleep();
        fadeReset();
        fadeStartIn(0x1E);
    }
    gameWork.control &= 0xFBFFFFFF;
    xglPadSetRepeat(0, PAD_L1 | PAD_R1 | PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT, 8, 1);
    pxyTickScaleDoubleOverride(0);
    if (mode != 0) {
        if (handlerResult == 1) {
            return 0;
        }
    }
    return 1;
}


void configScreenStateReset(void)
{
    saveTextMaxima[0] = (mcPortResult[0] = 0, configNoticePending = 0, configCardNoticeResults[0] = 0, configNoticeShown = 0);
    configCardNoticeTimer = 0;
    do {
        mcPortResult[1] = 0;
        configState = CONFIG_STATE_BROWSE;
    } while (0);
    do {
        configCardNoticeResults[1] = 0;
        configCursor = 0;
    } while (0);
    do {
        saveTextMaxima[5] = 0;
        saveTextMaxima[4] = 0;
        saveTextMaxima[3] = 0;
        saveTextMaxima[2] = 0;
    } while (0);
    saveTextMaxima[1] = 0;
    configOldDataKind = 0;
    configFadeStep = 0;
}

const char configScreenTitleText[16] = "  Configuration";
const char configStyleText[72] = "$init;$bg2;$shadow+1,+3,000000;$zoom13;$han1;$prop1;$xx0,21;$colc0c0c0;";
const char configZoomText[24] = "$zoom13;";

/* JP: メインメニュー / ロボメニュー / セーブ（Ｒ２でロード / てくすちゃ / バトルウィン / バトル選択 / アナライズ
 * EN: Main menu / Robot menu / Save (R2 to load / Texture / Battle win(dow) / Battle select / Analyze */
const char testUiItemNameText[120] = "\xa5" "\xe1" "\xa5" "\xa4" "\xa5" "\xf3" "\xa5" "\xe1" "\xa5" "\xcb" "\xa5" "\xe5" "\xa1" "\xbc" "\x00" "\x00" "\xa5" "\xed" "\xa5" "\xdc" "\xa5" "\xe1" "\xa5" "\xcb" "\xa5" "\xe5" "\xa1" "\xbc" "\x00" "\x00" "\x00" "\x00" "\xa5" "\xbb" "\xa1" "\xbc" "\xa5" "\xd6" "\xa1" "\xca" "\xa3" "\xd2" "\xa3" "\xb2" "\xa4" "\xc7" "\xa5" "\xed" "\xa1" "\xbc" "\xa5" "\xc9" "\x00" "\x00" "\x00" "\x00" "\xa4" "\xc6" "\xa4" "\xaf" "\xa4" "\xb9" "\xa4" "\xc1" "\xa4" "\xe3" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\xa5" "\xd0" "\xa5" "\xc8" "\xa5" "\xeb" "\xa5" "\xa6" "\xa5" "\xa3" "\xa5" "\xf3" "\x00" "\x00" "\x00" "\x00" "\xa5" "\xd0" "\xa5" "\xc8" "\xa5" "\xeb" "\xc1" "\xaa" "\xc2" "\xf2" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\xa5" "\xa2" "\xa5" "\xca" "\xa5" "\xe9" "\xa5" "\xa4" "\xa5" "\xba" "";

void configScreenDraw(void) {
    int y;

    fontObjResetAndBuild();
    fontObjSetOpacity(0x80 - configFadeStep);
    panelDrawNineSlice(menuPanelFrame, 0x20, 0x62, 0x1C0, 0xC4);
    menuDrawShortOutlineFrame(0x24, 0x66, 0x1B8, 0xBC);
    fontObjSetOpacity(configFadeStep);
    panelDrawNineSlice(menuPanelFrame, 0x51, 0x96, 0x15E, 0x60);
    menuDrawTitledFrame(0x55, 0x9B, 0x156, 0x56, configScreenTitleText);
    menuDrawScrollFrame(4);
    fontObjResetOpacity();
    textDrawSegment(0, 0, configStyleText, 0);
    textDrawSegment(0, 0, configZoomText, 0);
    switch (configState) {
    case CONFIG_STATE_BROWSE:
        textDrawLines(0x40, 0x76, menuMainTxdEntry(0xCE), 0);
        y = textDrawLines(0x40, -1, menuMainTxdEntry(0xCF), 0) + 0x14;
        textDrawSegment(0xA8, y, menuMainTxdEntry(0x208), configCursor == 0);
        textDrawSegment(0x128, y, menuMainTxdEntry(0x209), configCursor == 1);
        return;
    case CONFIG_STATE_CARD_NOTICE:
        textDrawLines(0x40, 0x76, menuMainTxdEntry(configCardNoticeResults[0] == 2 ? 0xDD : 0xDC), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(configCardNoticeResults[1] == 2 ? 0xDF : 0xDE), 0);
        return;
    case CONFIG_STATE_CARD_A_WAIT:
    case CONFIG_STATE_CARD_B_WAIT:
        textDrawLines(0x40, 0x76, menuMainTxdEntry(0xDA), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(0xDB), 0);
        return;
    case CONFIG_STATE_NOTICE_DONE:
        textDrawLines(0x40, 0x76, menuMainTxdEntry(0x22F), 0);
        return;
    case 11:
        textDrawLines(0x40, 0x76, menuMainTxdEntry(0xD1), 0);
        return;
    case 18:
        textDrawLines(0x40, 0x75, menuMainTxdEntry(0xC9), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(0xE0), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(0xE1), 0);
        return;
    case 17:
        textDrawLines(0x40, 0x76, menuMainTxdEntry(0xCB), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(0xE0), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(0xE1), 0);
        return;
    case 15:
        textDrawLines(0x40, 0x76, menuMainTxdEntry(0xD7), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(0xD3), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(0xD4), 0);
        return;
    case 16:
        textDrawLines(0x40, 0x76, menuMainTxdEntry(0xD3), 0);
        textDrawLines(0x40, -1, menuMainTxdEntry(0xD5), 0);
        return;
    case CONFIG_STATE_CARD_ERROR:
        saveMessageSetChoice(menuMainTxdEntry(0x22F), savePort);
        textDrawLines(0x40, 0x76, menuMainTxdEntry(0x22F), 0);
        return;
    case CONFIG_STATE_ITEM:
        saveOptionItemDraw(saveUiOptionItem, 0xA5, 0x100);
        /* fall through */
    case CONFIG_STATE_LIST:
        textDrawSegment(0, 0, configZoomText, 0);
        textDrawSegment(0x69, 0xAC, menuMainTxdEntry(0x15A), configCursor == 0);
        textDrawSegment(0x69, 0xC0, menuMainTxdEntry(0x158), configCursor == 1);
        textDrawSegment(0x69, 0xD4, menuMainTxdEntry(0x15D), configCursor == 2);
        textDrawSegment(0x10D, 0xAC, menuMainTxdEntry(SaveData.caption_all != 0 ? 0x16E : 0x16D), configCursor == 0);
        textDrawSegment(0x10D, 0xC0, menuMainTxdEntry(SaveData.soundMode + 0x165), configCursor == 1);
        textDrawSegment(0x10D, 0xD4, menuMainTxdEntry(SaveData.dolby_digital + 0x16B), configCursor == 2);
        msgRangeDrawRevealed(saveMessageRange, 0x18, 0x188);
        if (configState == CONFIG_STATE_LIST) {
            menuDrawKindIcon(0x188, 0x156, 3);
            textDrawSegment(0x198, 0x156, menuMainTxdEntry(0xE2), 0);
            menuDrawKindIcon(0x188, 0x16A, 0);
            textDrawSegment(0x198, 0x16A, menuMainTxdEntry(0xE3), 0);
        }
        break;
    default:
        break;
    }
}

void oldDataMaximaRaise(unsigned char *entry)
{
    unsigned char value;

    value = entry[1];
    if (saveTextMaxima[0] < value) {
        saveTextMaxima[0] = value;
    }
    value = entry[2];
    if (saveTextMaxima[1] < value) {
        saveTextMaxima[1] = value;
    }
    value = entry[3];
    if (saveTextMaxima[2] < value) {
        saveTextMaxima[2] = value;
    }
    value = entry[5];
    if (saveTextMaxima[3] < value) {
        saveTextMaxima[3] = value;
    }
    value = entry[6];
    if (saveTextMaxima[4] < value) {
        saveTextMaxima[4] = value;
    }
    if (saveTextMaxima[5] < entry[7]) {
        saveTextMaxima[5] = entry[7];
    }
}

int configOldDataCallback(int slot, unsigned char *data)
{
    unsigned char entryResult;

    switch (slot) {
    case -2:
        if (configState == 3) {
            if (mcPortResult[0] == 1 || mcPortResult[0] == 0xFF) {
                configState = CONFIG_STATE_CARD_B_START;
                break;
            }
        } else {
            if (mcPortResult[1] == 1 || mcPortResult[1] == 0xFF) {
                configState = CONFIG_STATE_RESOLVE;
                break;
            }
        }
        configNoticePending = 0;
        configState = CONFIG_STATE_CARD_ERROR;
        break;
    case -3:
        configNoticePending = 0;
        configState = CONFIG_STATE_NOTICE_DONE;
        break;
    case 0:
        if (data[0] == 1) {
            configNoticePending |= CONFIG_NOTICE_EP1 | CONFIG_NOTICE_FLAG | CONFIG_NOTICE_KINDS;
            oldDataMaximaRaise(data);
            configOldDataKind = 2;
        }
        break;
    case 1:
        entryResult = data[0];
        if (entryResult == 1) {
            configNoticePending |= CONFIG_NOTICE_EP1RE | CONFIG_NOTICE_FLAG | CONFIG_NOTICE_KINDS;
            oldDataMaximaRaise(data);
            configOldDataKind = entryResult;
        }
        break;
    case 2:
        if (data[0] == 1) {
            configNoticePending |= CONFIG_NOTICE_KIND_1;
        }
        if (data[0] == 2) {
            configNoticePending |= CONFIG_NOTICE_KIND_2;
        }
        if (data[1] == 0x7F) {
            configNoticePending |= CONFIG_NOTICE_FLAG;
        }
        break;
    }
    return 0;
}

extern short mcPortResultB;

/* One step of the new game configuration screen (configScreenRun): the state byte configState selects the step and returns 1 only after the
 * cross/circle-with-zero path of the option item. */
int configScreenUpdate(void)
{
    int result;
    int cardResult;
    int nextOption;
    McRequestParam request;

    result = 0;
    switch ((signed char) configState) {
    case CONFIG_STATE_BROWSE:
        if (padCirclePressedAlone() != 0) {
            if (configCursor != 0) {
                seVoiceStartDefault(SAVE_SE_ACCEPT);
                configState = CONFIG_STATE_OPEN_LIST;
                configCursor = 0;
                configNoticePending = 0;
            } else {
                configState = CONFIG_STATE_CHECK_CARDS;
                configNoticePending = 0;
                break;
            }
        } else if (PadData[0].directionPressed & (PAD_LEFT | PAD_RIGHT)) {
            seVoiceStartDefault(SAVE_SE_CURSOR);
            configCursor ^= 1;
        }
        break;
    case CONFIG_STATE_CHECK_CARDS:
        if (mcPortResult[0] == 1) {
            goto startCardCheck;
        }
        if (mcPortResult[1] != 1) {
            configCardNoticeResults[0] = mcPortResult[0];
            configCardNoticeResults[1] = mcPortResult[1];
            configState = CONFIG_STATE_CARD_NOTICE;
            configCardNoticeTimer = 0;
            seVoiceStartDefault(SAVE_SE_DENY);
            break;
        }
        if (mcPortResult[0] == mcPortResult[1]) {
        startCardCheck: /* entered from the first test above and from the equal-results test; the original has the card B start out of line after this block, and every goto-free form (nested if, merged condition, early break) puts it inline before the block or drops the dead results-equal test */
            {
            saveMessageSetChoice(menuMainTxdEntry(559), 0);
            request.file = 0;
            request.data = saveScanBuffer;
            request.callback = configOldDataCallback;
            xglMcRequest(0, MC_REQ_CHECK_OLD, &request, saveCardRequestResult);
            seVoiceStartDefault(SAVE_SE_ACCEPT);
            saveCardBusy = 1;
            configState = CONFIG_STATE_CARD_A_WAIT;
            saveMessageSetChoice(menuMainTxdEntry(218), 0);
            saveMessageSetChoice(menuMainTxdEntry(219), 0);
            }
        } else {
            configState = CONFIG_STATE_CARD_B_START;
            break;
        }
        break;
    case CONFIG_STATE_CARD_A_WAIT:
        cardResult = mcPortResult[0];
        if (cardResult != 1 && cardResult != 255) {
            configState = CONFIG_STATE_CARD_ERROR;
            configNoticePending = 0;
        }
        break;
    case CONFIG_STATE_CARD_B_WAIT:
        cardResult = mcPortResultB;
        if (cardResult != 1 && cardResult != 255) {
            configState = CONFIG_STATE_CARD_ERROR;
            configNoticePending = 0;
        }
        break;
    case CONFIG_STATE_CARD_B_START:
        if (mcPortResult[1] == 255) {
            do {
                *(int *) mcPortResult = xglMcMain();
            } while (mcPortResult[1] == 255);
        }
        if ((signed char) configState != CONFIG_STATE_CARD_B_START) {
            break;
        }
        {
            int value;

            value = mcPortResult[1];
            if (value == 1) {
                McRequestParam request;

                saveMessageSetChoice(menuMainTxdEntry(559), 1);
                request.data = saveScanBuffer;
                request.callback = configOldDataCallback;
                request.file = 0;
                xglMcRequest(1, MC_REQ_CHECK_OLD, &request, saveCardRequestResult);
                seVoiceStartDefault(SAVE_SE_ACCEPT);
                saveCardBusy = 1;
                configState = CONFIG_STATE_CARD_B_WAIT;
                saveMessageSetChoice(menuMainTxdEntry(218), 1);
                saveMessageSetChoice(menuMainTxdEntry(219), 1);
            } else {
                configState = CONFIG_STATE_RESOLVE;
            }
        }
        break;
    case CONFIG_STATE_NOTICE_DONE:
        saveCardBusy = 0;
        if (padCirclePressedAlone()) {
            seVoiceStartDefault(SAVE_SE_ACCEPT);
            configState = CONFIG_STATE_BROWSE;
        }
        break;
    case CONFIG_STATE_RESOLVE:
        saveCardBusy = 0;
        configNoticeShown |= (signed char) configNoticePending & CONFIG_NOTICE_KINDS;
        configNoticePending = (signed char) configNoticePending & ~CONFIG_NOTICE_KINDS;
        if ((configNoticeShown & CONFIG_NOTICE_EP_SAVES) != 0 && (configNoticeShown & CONFIG_NOTICE_FLAG) != 0) {
            configState = CONFIG_STATE_OPEN_LIST;
            configCursor = 0;
            break;
        }
        if (((signed char) configNoticePending & CONFIG_NOTICE_EP_SAVES) != 0 && (configNoticeShown & CONFIG_NOTICE_EP_SAVES) != 0 && (((signed char) configNoticePending & CONFIG_NOTICE_FLAG) == 0 || (configNoticeShown & CONFIG_NOTICE_FLAG) != 0)) {
            configState = 17;
            break;
        }
        if (((signed char) configNoticePending & CONFIG_NOTICE_FLAG) != 0 && (configNoticeShown & CONFIG_NOTICE_FLAG) != 0 && (((signed char) configNoticePending & CONFIG_NOTICE_EP_SAVES) == 0 || (configNoticeShown & CONFIG_NOTICE_EP_SAVES) != 0)) {
            configState = 18;
            break;
        }
        /* fall through */
    case 19:
        configNoticePending = (signed char) configNoticePending & ~configNoticeShown;
        if (mcPortResult[1] == 255) {
            do {
                *(int *) mcPortResult = xglMcMain();
            } while (mcPortResult[1] == 255);
        }
        if ((signed char) configState == CONFIG_STATE_RESOLVE || (signed char) configState == 19) {
            if (((signed char) configNoticePending & CONFIG_NOTICE_EP_SAVES) != 0) {
                configNoticeShown |= (signed char) configNoticePending & CONFIG_NOTICE_EP_SAVES;
                configNoticePending = (signed char) configNoticePending & ~CONFIG_NOTICE_EP_SAVES;
                configState = 15;
            } else if (((signed char) configNoticePending & CONFIG_NOTICE_FLAG) != 0) {
                configNoticeShown |= (signed char) configNoticePending & CONFIG_NOTICE_FLAG;
                configNoticePending = (signed char) configNoticePending & ~CONFIG_NOTICE_FLAG;
                configState = 16;
            } else {
                if (mcPortResult[0] != 1 && mcPortResult[1] != 1) {
                    configState = CONFIG_STATE_NOTICE_DONE;
                } else {
                    configState = 11;
                }
            }
            saveCardBusy = 0;
        }
        break;
    case 15:
    case 16:
        if (padCirclePressedAlone() == 0) {
            break;
        }
        seVoiceStartDefault(SAVE_SE_ACCEPT);
        if ((signed char) (configNoticeShown & 7) >= 5) {
            configCursor = 0;
            configState = CONFIG_STATE_OPEN_LIST;
        } else {
            if (*(signed char *) &configNoticePending == 0) {
                configState = CONFIG_STATE_BROWSE;
            } else {
                configState = 19;
            }
        }
        break;
    case CONFIG_STATE_CARD_NOTICE:
    case 11:
    case CONFIG_STATE_CARD_ERROR:
    case 17:
    case 18:
        if (padCirclePressedAlone()) {
            seVoiceStartDefault(SAVE_SE_ACCEPT);
            configState = CONFIG_STATE_BROWSE;
        }
        break;
    case CONFIG_STATE_OPEN_LIST:
        {
            int nextFade;

            nextFade = configFadeStep + 8;
            if ((signed char) nextFade < 0) {
                configFadeStep = -128;
                msgRangeSetText(saveMessageRange, menuMainTxdEntry(382));
                configState = CONFIG_STATE_LIST;
                break;
            }
            configFadeStep = nextFade;
        }
        break;
    case CONFIG_STATE_CLOSE_LIST:
        configFadeStep = configFadeStep + 0xF8;
        if (configFadeStep == 0) {
            configCursor = 0;
            configState = CONFIG_STATE_BROWSE;
        }
        break;
    case CONFIG_STATE_LIST:
        if (padCrossPressedAlone()) {
            seVoiceStartDefault(SAVE_SE_DENY);
            configState = CONFIG_STATE_CLOSE_LIST;
            break;
        }
        if (padCirclePressedAlone() != 0) {
            seVoiceStartDefault(SAVE_SE_ACCEPT);
            saveItemInit(saveUiOptionItem, 0, menuMainTxdEntry(214));
            configState = CONFIG_STATE_ITEM;
            break;
        }
        if (PadData[0].directionPressed & (PAD_UP | PAD_DOWN)) {
            seVoiceStartDefault(SAVE_SE_CURSOR);
            if (PadData[0].directionPressed & PAD_UP) {
                nextOption = (unsigned char) configCursor + 2;
            } else {
                nextOption = (unsigned char) configCursor + 1;
            }
            configCursor = (signed char) nextOption % 3;
            switch (configCursor) {
            case 0:
                msgRangeSetText(saveMessageRange, menuMainTxdEntry(382));
                break;
            case 1:
                msgRangeSetText(saveMessageRange, menuMainTxdEntry(380));
                break;
            case 2:
                if (SaveData.dolby_digital != 0) {
                    msgRangeSetText(saveMessageRange, menuMainTxdEntry(391));
                } else {
                    msgRangeSetText(saveMessageRange, menuMainTxdEntry(385));
                }
                break;
            }
        } else if (PadData[0].directionPressed & (PAD_LEFT | PAD_RIGHT)) {
            if (configCursor == 2) {
                seVoiceStartDefault(SAVE_SE_CURSOR);
                SaveData.dolby_digital ^= 1;
                if (SaveData.dolby_digital != 0) {
                    msgRangeSetText(saveMessageRange, menuMainTxdEntry(391));
                } else {
                    msgRangeSetText(saveMessageRange, menuMainTxdEntry(385));
                }
            } else if (configCursor == 1) {
                seVoiceStartDefault(SAVE_SE_CURSOR);
                if (PadData[0].directionPressed & PAD_LEFT) {
                    SaveData.soundMode += 2;
                } else {
                    SaveData.soundMode += 1;
                }
                SaveData.soundMode = SaveData.soundMode % 3;
                soundStereoSet(SaveData.soundMode != 0);
            } else {
                {
                    int i;

                    i = 0;
                    do {
                        if (menuConfigButtonIndexTable[i] == SaveData.caption_all) {
                            if (PadData[0].directionPressed & PAD_LEFT) {
                                i--;
                            } else {
                                i++;
                            }
                            i += 2;
                            i %= 2;
                            SaveData.caption_all = menuConfigButtonIndexTable[i];
                            i = -1;
                            break;
                        }
                        i++;
                    } while (i < 2);
                    if (i == 2) {
                        SaveData.caption_all = 0;
                    }
                }
                seVoiceStartDefault(SAVE_SE_CURSOR);
            }
        }
        break;
    case CONFIG_STATE_ITEM:
        if (PadData[0].directionRepeated & PAD_UP) {
            cursorBufferMoveBy(saveOptionItemCursor, -1);
        }
        if (PadData[0].directionRepeated & PAD_DOWN) {
            cursorBufferMoveBy(saveOptionItemCursor, 1);
        }
        if (padCrossPressedAlone() != 0) {
            SaveItem *item;

            seVoiceStartDefault(SAVE_SE_CANCEL);
            item = saveUiOptionItem;
            saveOptionItemFadeOut(item);
            item->flag = 0;
            configState = CONFIG_STATE_LIST;
        } else if (padCirclePressedAlone() != 0) {
            SaveItem *item;

            seVoiceStartDefault(SAVE_SE_ACCEPT);
            item = saveUiOptionItem;
            if (saveItemGetValue(item) == 0) {
                saveOptionItemFadeOut(item);
                result = 1;
            } else {
                saveOptionItemFadeOut(item);
                item->flag = 0;
                configState = CONFIG_STATE_LIST;
                break;
            }
            item->flag = 0;
        }
        break;
    default:
        break;
    }
funcEnd:
    return result;
}

/* Six bytes of saveTextMaxima copied to dst; returns the signed byte at configNoticeShown. */
signed char oldDataMaximaCopy(SaveTextMaximaBytes *dst)
{
    *dst = *(SaveTextMaximaBytes *) saveTextMaxima;
    return configNoticeShown;
}

/* mcPortResult: the memory-card result word (configScreenRun stores xglMcMain() there). */
typedef union SaveMcWord {
    int result;
    short half[2];
} SaveMcWord;


void configScreenRun(void)
{
    int done;
    int countdown;
    done = 0;
    gameWork.control |= 0x04000000;
    ClearEnv.value.color_r = 0;
    ClearEnv.value.color_g = 0;
    ClearEnv.value.color_b = 0;
    ClearEnv.value.color_a = 0;
    xglRenderClearFrame();
    xglCdArenaOpen((void *) 0x1000000, 0x200000);
    saveScanBuffer = xglCdSectorAlloc(0x19000);
    configScreenStateReset();
    fontObjLoadTexture(0);
    fontObjLoadTexture(3);
    bgLayerLoadAll();
    xglCdWaitIdle();
    fontObjLoadTexture(2);
    bgLayerCameraSetup();
    pxyTickScaleDoubleOverride(1);
    while (xglCdServiceIfPending() != 0) {
        ((SaveMcWord *) mcPortResult)->result = xglMcMain();
        pxyRequestOverlayPass();
        XenoActorSetFlagAll(2);
        bgLayerClearFlags(0);
        bgLayerClearFlags(1);
        bgLayerClearFlags(2);
        XenoActorDoit();
        XenoActorDisp();
        xglTickAdvance();
        xglSleep();
    }
    menuResetState(saveMessageRange);
    while (1) {
        ((SaveMcWord *) mcPortResult)->result = xglMcMain();
        if ((PadData[0].directionPressed & PAD_START) != 0 && (((unsigned short *) PadData)[20] & PAD_SOFT_RESET_HELD) == PAD_SOFT_RESET_HELD && saveCardBusy == 0) {
            gameWork.control |= 0x10000000;
            done = 1;
        }
        if (done != 0) {
            break;
        }
        done = configScreenUpdate();
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        xglCallbackQueueAdd(configScreenDraw);
        XenoActorDoit();
        XenoActorDisp();
        xglTickAdvance();
        xglSleep();
    }
    bgLayerResetState();
    bgLayerRelease();
    xglCdArenaClose();
    {
        if ((gameWork.control & 0x10000000) == 0) {
            grCopyFrame(sRender.vram.work_buffer_base, sRender.draw_buffer_base);
            countdown = 15;
            do {
                sceGsSyncV(0);
                countdown = countdown - 1;
                grFillFrame();
                grCopyFrame(sRender.display_buffer_base, sRender.vram.work_buffer_base);
                xglTickAdvance();
                xglSleep();
            } while (countdown >= 0);
            xglTickAdvance();
            xglSleep();
            xglTickAdvance();
            xglSleep();
            xglSleep();
            fadeReset();
            fadeStartIn(30);
        }
    }
    gameWork.control &= 0xFBFFFFFF;
    xglPadSetRepeat(0, PAD_L1 | PAD_R1 | PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT, 8, 1);
    pxyTickScaleDoubleOverride(0);
}

/* Save-screen flags and indexes (small data, read through $gp), in address order. */

/* Slot panel layouts of the save slot detail: the two panel texture-index tables. */
/* Save text index table: id, first and second text index of each entry (saveTextIndexTable, 0x3ECC30). */
SaveTextEntry saveTextIndexTable[54] = {
    { 1010, 1, 18 }, { 1070, 1, 19 }, { 1210, 2, 20 }, { 1250, 2, 21 },
    { 1280, 2, 22 }, { 1520, 3, 23 }, { 2010, 4, 24 }, { 2070, 5, 25 },
    { 2110, 6, 26 }, { 2150, 6, 27 }, { 2170, 6, 28 }, { 2210, 79, 29 },
    { 2250, 79, 30 }, { 2270, 79, 31 }, { 2540, 7, 32 }, { 3030, 8, 33 },
    { 3080, 8, 34 }, { 3130, 71, 35 }, { 3190, 71, 36 }, { 3410, 9, 37 },
    { 3440, 9, 38 }, { 3470, 9, 39 }, { 3480, 9, 40 }, { 3530, 9, 41 },
    { 3540, 9, 42 }, { 4020, 10, 43 }, { 4310, 11, 44 }, { 4350, 11, 45 },
    { 4370, 11, 46 }, { 4610, 12, 47 }, { 4640, 12, 48 }, { 4650, 12, 49 },
    { 4720, 12, 50 }, { 5040, 13, 51 }, { 5070, 13, 52 }, { 5110, 13, 53 },
    { 5170, 13, 54 }, { 5510, 14, 55 }, { 5540, 14, 56 }, { 5710, 15, 57 },
    { 5730, 15, 58 }, { 5760, 15, 59 }, { 5920, 16, 60 }, { 5950, 17, 61 },
    { 7011, 62, 67 }, { 8070, 63, 68 }, { 7039, 64, 69 }, { 8060, 65, 70 },
    { 7057, 66, 0 }, { 1040, 1, 73 }, { 1910, 72, 74 }, { 1610, 78, 75 },
    { 5310, 76, 77 }, { 0, 0, 0 } };

int saveSlotPanelIdleTable[10] = { 606, 607, 608, 609, 538, 610, 611, 612, 613, 0 };
int saveSlotPanelSelectedTable[12] = { 600, 601, 602, 537, 538, 539, 603, 604, 605, 0, 0, 0 };

DirectDataBlock saveThumbnailDirect = {
    { 0x0, 0x0, 0x0, 0x51000006, 0x4, 0x10000000, 0xE },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x08, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    0x0ULL,
    { 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    0x7000,
    { 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};
void (*saveUiDrawTable[6])(void) = {
    savePortSelectDraw,
    0,
    0,
    saveUiOptionDraw,
    0,
    0
};
unsigned char saveScreenZoomText[16] = "$zoom12,13;";
int (*saveUiUpdateTable[6])(void) = {
    savePortSelectUpdate,
    0,
    saveSlotListUpdate,
    saveSlotTransferUpdate,
    saveUiPollButtons,
    0
};
unsigned long long saveThumbnailDma[24] = {
    0, 0x5100000B00000000UL, 0xA08B400000008001UL, 0xE53531EEEEUL, 0x30000UL, 71, 96, 20,
    0, 76, 0, 6, 0x8000000080UL, 0x8000000080UL, 0, 0,
    0x71F700006FF8UL, 0xFFFFFFFFUL, 0x1C0000002000UL, 0, 0x76C700007578UL, 0xFFFFFFFFUL, 0, 63
};

/* Small data (.sdata) of the save screen, in address order. */
unsigned char saveIsLoad = 0;
unsigned char savePort = 0;
signed char saveSlotIndex = 0;
unsigned char saveUiMode = 0;
unsigned char saveUiState = 0;
unsigned char saveListScroll = 0;
unsigned char saveCardBusy = 0;
int saveCardFreeSpace = 0;
signed char saveMessageState = 0;
signed char saveOptionPromptFlag = 0;
signed char saveModePendingFlag = 0;
int saveProgressFrame = 0;
int saveProgressTotal = 0;
unsigned char saveLabelSeparatorText[4] = { 0xa1, 0xa6, 0, 0 };
unsigned char *saveLabelSeparator = saveLabelSeparatorText;
unsigned char saveChoiceMarkSjisZero[8] = { 0x82, 0x4f, 0, 0, 0, 0, 0, 0 };
unsigned char saveChoiceMarkEucZero[8] = { 0xa3, 0xb0, 0, 0, 0, 0, 0, 0 };
unsigned char saveChoiceMarkHashText[8] = { 0x23, 0x23, 0, 0, 0, 0, 0, 0 };
unsigned char saveChoiceDigitText[8] = { 0x31, 0x32, 0, 0, 0, 0, 0, 0 };
unsigned char saveHeaderSeparatorText[8] = { 0x20, 0x2f, 0x20, 0, 0, 0, 0, 0 };
unsigned char saveSlotLevelLabelText[8] = { 0x2f, 0x20, 0x4c, 0x76, 0x20, 0, 0, 0 };
unsigned char saveSlotTimeLabelText[8] = { 0x54, 0x69, 0x6d, 0x65, 0x20, 0x20, 0x2f, 0 };
unsigned char saveSlotTimeColonText[8] = { 0x3a, 0, 0, 0, 0, 0, 0, 0 };
unsigned char saveSlotNoDataText[8] = { 0x4e, 0x4f, 0x20, 0x44, 0x41, 0x54, 0x41, 0 };
int saveMessageTimer = 0;
