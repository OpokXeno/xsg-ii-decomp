#ifndef INCLUDE_MAIN_MENU_DRAW_H
#define INCLUDE_MAIN_MENU_DRAW_H

#include "shared.h"
#include "main/xgl_tick.h"
#include "main/cursor_buffer.h"
#include "main/pt_member.h"

/* The menu and text drawing of src/main/menu_draw.c. */

/* One item of a MenuBlock (MenuBlock.items[], 0x40 bytes); menuResetItem empties it. member is
   the index into ptState.slots, or ptState.roster with MENU_ITEM_ROSTER, -1 when empty.
   menuDrawPartyPanel draws the item as a panel: position.current_pair is its x and y,
   highlight.current_pair.x its opacity and highlight.current_pair.y the fade of its selection
   frame (0 none, else a fraction of 128). menuRefreshItems sets the highlight targets: x 0x80 when
   the item is lit, y 0x80 when it is selected. */
typedef struct MenuItem {
    int member;
    struct FontTrack position;          /* 0x04 */
    struct FontTrack highlight;         /* 0x20 */
    unsigned int flags;                 /* 0x3C: MenuItemFlag */
} MenuItem;

/* MenuItem.flags. */
enum MenuItemFlag {
    MENU_ITEM_SELECTED = 0x1,           /* highlight y target 0x80 */
    MENU_ITEM_ALT_FRAME = 0x4,          /* draws the 0x121/0x115 frame pair instead of 0x120/0x114 */
    MENU_ITEM_DISABLED = 0x8,
    MENU_ITEM_MARK_B = 0x10,            /* mark 0x13F instead of 0x13E */
    MENU_ITEM_ROSTER = 0x100,           /* member indexes ptState.roster */
    MENU_ITEM_MARK_A = 0x200,           /* mark 0x13D */
    MENU_ITEM_VALUES = 0x400,           /* layout 4 shows the class and skill points instead of HP and EP */
    MENU_ITEM_TALL = 0x10000,           /* tall variant of layouts 5 to 7 */
    MENU_ITEM_LABEL = 0x100000          /* with MENU_ITEM_MARK_A: the empty slot adds the 0x158/0x159
                                           marks and the "Canaan" label */
};

/* A cursor menu (mainMenuBlock and itemMenuBlock of menu_scene.c, 0x3E8 bytes). Partial layout: only the
 * fields the menu code touches.
 * The object starts with its own position track (the origin its items are drawn from), then the
 * cursor frame tracks (0 position, 1 and 2 size, 3 glow) and up to 13 items (MenuItem). cursor is the
 * selected index, wrapped modulo item_count; prev_cursor is the
 * one before it. layout selects the party-panel grid shape menuPlaceItemCursor jumps on
 * (menuDrawPartyPanel's layout argument); prev_layout is the one before it.
 * flags: 0x1 cursor glow, 0x2 hidden, 0x4 locks the cursor, 0x10000 the full 13-item grid (menuResetItems). */
typedef struct MenuBlock {
    struct FontTrack position;
    struct FontTrack tracks[4];
    MenuItem items[13];
    union {
        int cursor;
        unsigned char cursorByte;   /* the low byte as menuSceneDrawOverlays reads it (a union access, alias set 0) */
    };
    int prev_cursor;
    int reset_word; /* only written, to 0, by menuResetItems */
    int item_count;
    int layout;
    int prev_layout;
    int flags;
} MenuBlock;

/* A message being typed out in a window. msgRangeSetText stores the escape-coded text and counts its
 * visible glyphs into glyph_count; msgRangeDrawRevealed advances reveal_pos (starts at -8, a short lead-in)
 * and drawn (the token count its draw loop reaches) toward glyph_count as well. */
typedef struct MsgRange {
    unsigned char *text;
    int glyph_count;
    int reveal_pos;
    int drawn;
} MsgRange;

/* Text: segments, glyph strings, titles and numbers. */
void textColorDefaultSet(void);
void textDrawSegment(int x, int y, const char *text, int flags);
int textDrawLines(int x, int y, unsigned char *text, int flags);
void textDrawGlyphString(int x, int y, const unsigned char *text, unsigned long long colour);
void textDrawShadow(int x, int y, const unsigned char *text, unsigned long long colour);
void textDrawStringAlt(int x, int y, const unsigned char *text, u64 colour);
void textDrawShadowAlt(int x, int y, const unsigned char *text, unsigned long long colour);
void textDrawTitle(int windowLayer, int x, int y, const unsigned char *text);
char *textFormatDecimal(int value, int digits, int zeroPad);
void textDrawDecimal(int x, int y, int value, int digits, u64 colour);
void textDrawDecimalAlt(int x, int y, int value, int digits, u64 colour);
void textDrawDecimalZero(int x, int y, int value, int digits, u64 colour);
void textDrawDecimalShadow(int x, int y, int value, int digits, unsigned long long color);
void textDrawDecimalAltShadow(int x, int y, int value, int digits, u64 colour);
void textDrawDecimalShadowZero(int x, int y, int value, int digits, u64 colour);
void textDrawSignedDecimalShadow(int x, int y, int value, int digits, u64 colour);
void textDrawDigits(int x, int y, int value, int digits);
void textDrawDigitsZero(int x, int y, int value, int digits);
void textDrawDigitsPair(int x, int y, int leftValue, int rightValue, int digits);
void textDrawDigitsWide(int x, int y, int value, int digits);
void textDrawDigitsWideAlt(int x, int y, int value, int count);

/* Sprite cells and cell-built marks. */
void menuDrawCell(int cell, int x, int y, int flags);
void menuDrawCellSized(int cell, int x, int y, int width, int height, int flags);
void menuDrawCompareMark(int x, int y, int index, u64 colour);

/* Panels and frames. */
void panelDrawNineSlice(const int *kinds, int x, int y, int width, int height);
void panelDrawNineSliceDepth(const int *kinds, int x, int y, int width, int height);
void panelDrawNineSliceFlipCorner(const int *kinds, int x, int y, int width, int height);
void panelDrawNineSliceNormalBlend(const int *kinds, int x, int y, int width, int height);
void menuDrawTabFrame(int x, int y, int width, int height);
void menuDrawBadge(int x, int y);
void menuDrawListFrame(int x, int y, int width, int height);
void menuDrawOutlineWindow(int x, int y, int width, int height);
void menuDrawTitledFrame(int x, int y, int width, int height, const char *text);
void menuDrawTitledFrameBlue(int x, int y, int width, int height, const char *text);
void menuDrawTitledFrameRed(int x, int y, int width, int height, const char *text);
void menuDrawFixedFrame(int x, int y);
void menuDrawThreePieceBar(int x, int y, int width);
void menuDrawBorder(int x, int y, int width, int height, unsigned long long color);
void menuDrawPanelFrame(int x, int y, int width, int height, unsigned long long color);
void menuDrawPanelFrameWide(int x, int y, int width, int height, unsigned long long color);
void menuDrawPanelFrameAlt(int x, int y, int width, int height, unsigned long long color);
void menuDrawPanelFrameWideAlt(int x, int y, int width, int height, unsigned long long color);
void menuDrawPanel(int x, int y, int width, int height, unsigned long long color);
void menuDrawPanelWide(int x, int y, int width, int height, unsigned long long color);
void menuDrawWindowFrame(int x, int y, int width, int height);
void menuDrawShortOutlineFrame(int x, int y, int width, int height);
void menuDrawOutlineFrame(int x, int y, int width, int height);
void menuDrawSplitFrame(int x, int y, int width, int height, int split);
void menuDrawTitledWindow(int x, int y, int width, int height, char *title);
void menuDrawScrollFrame(int count);

/* Icons, cursor and page controls. */
void menuDrawIndexedIcon(int x, int y, int index);
void menuDrawIndexedIconMarked(int x, int y, int icon);
void menuDrawLitIcon(int x, int y, int icon);
void menuDrawLitIconWide(int x, int y, int icon);
void menuDrawDimIcon(int x, int y, int icon);
void menuDrawLitIconMarked(int x, int y, int icon);
void menuPlaceCursor(int x, int y, int width, int blink);
void menuDrawCursor(void);
void menuDrawSelectArrows(int x, int width, int y);
void menuDrawPageTabs(int x, int y, int selected);
void menuDrawPageCells(int x, int y, int selected, int count, int wideCellLimit);
void menuDrawListWindow(int x, int y, int layer, int count);
void menuDrawFrontMemberIcons(int x, int y, int record);
void menuDrawCharInfo(int x, int y, int group, int slot, int mode);

/* Item, command and license windows. */
void menuDrawItemList(int x, int y, ListCursor *cursor);
void menuDrawItemListDetail(int x, int y, ListCursor *cursor);
int menuMemberHasEntry(PartyMember *member, unsigned char item);
void menuDrawPlainItemList(int x, int y, ListCursor *cursor);
void menuDrawMemberItemList(int x, int y, ListCursor *cursor, unsigned char *items);
void menuDrawStatCompare(int x, int y, int value, int compare);
void menuDrawCharStats(int x, int y, PartyMember *member, int item, int slot);
void menuDrawEquipPanel(int x, int y, ListCursor *cursor, unsigned char *items, unsigned char rowCount);
void menuDrawItemCategory(int x, int y, ListCursor *cursor, unsigned char *items, int mask);
void menuDrawKindIcon(int x, int y, int index);
void menuDrawAttackCommand(int x, int y, ListCursor *cursor, int member);
void menuDrawSpecialAttack(int x, int y, ListCursor *cursor, int member, unsigned short mode);
void menuDrawCommandInfo(int x, int y, ListCursor *cursor, int member);
void menuDrawPartnerInfo(int x, int y, ListCursor *cursor, int member, unsigned short mode);
void menuDrawCommandDetail(int x, int y, ListCursor *cursor, int member);
void menuDrawCopilotInfo(int x, int y, ListCursor *cursor, int member, unsigned short mode);
void menuDrawClassInfo(int x, int y);
void menuDrawLicenseBoard(int x, int y, ListCursor *groupCursor, ListCursor *slotCursor, ListCursor *subCursor, int mode, int mask);
void menuDrawConfig(int x, int y, ListCursor *cursor, unsigned char *items);
void menuDrawControllerConfig(void);
void menuDrawPlayTime(int x, int y);

/* Party panels and the cursor menu (MenuBlock). */
void menuResetItem(MenuItem *item);
PartyMember *menuItemMemberEntry(MenuItem *item);
void menuDrawStatIcon(int kind, int x, int y, u64 colour);
void menuDrawLevel(int x, int y, int level);
void menuHpColours(int hp, int maxHp, u64 *shadowColour, u64 *faceColour);
void menuDrawHp(int x, int y, int hp, int maxHp);
void menuDrawEp(int x, int y, int ep, int maxEp);
void menuDrawHpWide(int x, int y, int hp, int maxHp);
void menuDrawEpWide(int x, int y, int ep, int maxEp);
void menuDrawLevelShadow(int x, int y, int level);
void menuDrawHpShadow(int x, int y, int hp, int maxHp);
void menuDrawEpShadow(int x, int y, int ep, int maxEp);
void menuDrawHpWideShadow(int x, int y, int hp, int maxHp);
void menuDrawEpWideShadow(int x, int y, int ep, int maxEp);
void menuDrawAnimatedBand(int x, int y);
void menuDrawTrackPanel(int command, struct FontTrack *pos, struct FontTrack *size, struct FontTrack *tabs);
void menuDrawPanelHighlight(int x, int y, int kind);
void menuDrawPartyPanel(MenuItem *item, PartyMember *member, int x, int y, int layout);
void menuDrawEmptyPartySlot(MenuItem *item, int x, int y);
void menuResetItems(MenuBlock *object, int full);
void menuPlaceItemCursor(MenuBlock *object, int index);
void menuRefreshItems(MenuBlock *object);
void menuDrawObject(MenuBlock *object);
void menuDrawItems(MenuBlock *object);
void menuSetMode(MenuBlock *object, int layout);
void menuHide(MenuBlock *object, int mode);
void menuSkipDisabled(MenuBlock *object);
void menuMoveCursor(MenuBlock *object, int delta);
void menuSetCursor(MenuBlock *object, int value);
void menuNavigate(MenuBlock *object, int direction);
void menuSetSelected(MenuBlock *object, int selected);
void menuSetValuesMode(MenuBlock *object, int enable);
void menuDrawItemHighlighted(MenuBlock *object, int index);
void menuEnableItems(MenuBlock *object, int enableMask);
void menuResetState(MsgRange *range);

/* Typed-out messages, the yes/no prompt and the countdown. */
void msgRangeSetText(MsgRange *range, unsigned char *text);
void msgRangeReset(MsgRange *range);
int msgRangeDrawIcon(int x, int y, int code, int alpha);
void msgRangeDrawRevealed(MsgRange *range, int x, int y);
int msgRangeIsRevealed(const MsgRange *range);
void msgRangeDraw(MsgRange *range, int x, int y);
void msgBufferAppend(unsigned char *text);
void choiceInit(unsigned char *text, int initialOption);
void choiceSetFirstOptionText(void *text);
void choiceSetSecondOptionText(void *text);
void choiceSetRow(int row);
int choiceUpdate(void);
void choiceDraw(void);
int choiceGetResult(void);
void countdownInit(unsigned char *text, int count);
int countdownTick(void);
void countdownDraw(void);
void textSetCharSpacing(signed char spacing);
void textResetCharSpacing(void);

/* Data of menu_draw.c that other files use. */
extern int menuCardPanelFrame[10];
extern int menuCardSelectedPanelFrame[10];
extern int menuPanelFrame[10];
extern int yg2PathFrameImage[20];
extern int yg2PathFrameBorderImage[10];

#endif /* INCLUDE_MAIN_MENU_DRAW_H */
