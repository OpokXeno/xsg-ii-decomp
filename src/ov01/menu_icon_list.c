#include "common.h"
#include "ov01/obj_slot.h"
#include "ov01/unit_cmd.h"
#include "ov01/gr_gp_init.h"
#include "ov01/menu_icon_list.h"
#include "ov01/menu.h"
#include "ov01/menu_panel.h"
#include "ov01/menu_tex_state.h"

/* An entry byte of a row: bit 7 marks the entry, the low seven bits are its kind. */
#define ICON_ENTRY_MARK 0x80
#define ICON_ENTRY_KIND_MASK 0x7f
/* The slot counter of an entry grows up to this value and stops there. */
#define ICON_ENTRY_COUNTER_MAX 4
/* A list holds at most this many entries (ICON_LIST_COUNT). */
#define ICON_LIST_MAX_ENTRIES 8
/* The close countdown (close.motionIndex) runs up to this value, then the list is freed. */
#define ICON_LIST_CLOSE_TIME 16
/* close.request 1 asks the step to count the timer down and close the list. */
#define ICON_LIST_CLOSE_ASKED 1
/* Source texture columns of the entry icons by kind (kind 4 is at 490), and the icon width in texels. */
#define ICON_SRC_LEFT_KIND1 442
#define ICON_SRC_LEFT_KIND2 466
#define ICON_SRC_LEFT_KIND4 490
#define ICON_SRC_WIDTH 21

/* The value of ICON_LIST_STATE: the list is idle, or it slides in. */
enum {
    ICON_LIST_IDLE = 0,
    ICON_LIST_SLIDING_IN = 1
};

/*
 * The icon list panel record of menuIconListPanel (IconListPanel). The step and draw
 * pointers sit at +0x04 and +0x08. The 0x80-byte block at +0x10 that objStdInit
 * clears is the motion block of the panel: its first two floats are the origin that
 * objHatoVec and objStdMove move: slide.position is the origin and slide.goal the target.
 */
typedef struct IconListPanel IconListPanel;

/*
 * The work area of the panel after its motion block: eight 4-byte cells that the
 * functions address as one array (the original indexes the entry bytes from
 * +0x90), each read as a word, the owner, or four entry bytes.
 */
typedef union IconListCell {
    int value;                  /* state, count, or the four bytes cleared at once */
    BattleUnit *owner;           /* the unit the list shows */
    signed char kind[4];        /* entry kinds of one row (bit 7 = mark) */
    signed char counter[4];     /* per slot counters of one row (grow to ICON_ENTRY_COUNTER_MAX) */
    struct {
        short request;          /* 1 asks the step to count down and close the list */
        short timer;
    } close;
} IconListCell;

enum IconListCellIndex {
    ICON_LIST_STATE,            /* +0x90 */
    ICON_LIST_OWNER,            /* +0x94 */
    ICON_LIST_COUNT,            /* +0x98: entries shown, at most 8 */
    ICON_LIST_KIND0,            /* +0x9C: kind of entries 0..3 */
    ICON_LIST_KIND1,            /* +0xA0: kind of entries 4..7 */
    ICON_LIST_COUNTER0,         /* +0xA4: counters of entries 0..3 */
    ICON_LIST_COUNTER1,         /* +0xA8: counters of entries 4..7 */
    ICON_LIST_CLOSE             /* +0xAC: close request and timer */
};

struct IconListPanel {
    unsigned char unmodeled_00[4];
    void (*update)(IconListPanel *panel);   /* +0x04: the step menuIconListStep */
    void (*draw)(IconListPanel *panel);     /* +0x08: the draw menuIconListDraw */
    unsigned char unmodeled_0C[4];
    SlideBlock slide;                       /* +0x10: the motion block; position is -152 when the list opens and 16 when it is shown (y the row from menuPanelRowAlloc), goal (16, row) is where the open list slides to */
    IconListCell cell[8];                   /* +0x90: the list's work cells (IconListCellIndex) */
};

void menuIconListStep(IconListPanel *panel);
void menuIconListDraw(IconListPanel *panel);

extern IconListPanel *menuIconListPanel;

void menuIconEntryDraw(void *packet, int x, int y, int kind, int colourIndex, int slot);

extern MenuColor menuIconColors[];

/*
 * menuIconListNew: constructor of the unit icon list panel. It allocates the panel
 * with objSlotAlloc, sets its step menuIconListStep and its draw menuIconListDraw,
 * keeps it in menuIconListPanel and zeroes its state, owner, counters and close
 * request. Called from the battle setup ov01StepNextEnemy.
 */
void menuIconListNew(void)
{
    IconListPanel *panel;

    panel = objSlotAlloc();
    if (panel != 0) {
        panel->update = menuIconListStep;
        panel->draw = menuIconListDraw;
        menuIconListPanel = panel;
        objStdInit(&panel->slide);
        panel->cell[ICON_LIST_STATE].value = ICON_LIST_IDLE;
        panel->cell[ICON_LIST_OWNER].owner = 0;
        panel->cell[ICON_LIST_COUNT].value = 0;
        panel->cell[ICON_LIST_KIND0].value = 0;
        panel->cell[ICON_LIST_KIND1].value = 0;
        panel->cell[ICON_LIST_COUNTER0].value = 0;
        panel->cell[ICON_LIST_COUNTER1].value = 0;
        panel->cell[ICON_LIST_CLOSE].value = 0;
    }
}

/*
 * menuIconListStep: with an owner, state ICON_LIST_SLIDING_IN slides the list in
 * with objHatoVec and objStdMove. In the idle state a close request
 * (close.request ICON_LIST_CLOSE_ASKED) counts its timer at +0xAE up to
 * ICON_LIST_CLOSE_TIME, then clears the owner and frees the row with
 * menuPanelRowFree. Each frame the slot counters of the first entries (bytes at
 * +0xA4..+0xAB) grow and saturate at ICON_ENTRY_COUNTER_MAX.
 */
void menuIconListStep(IconListPanel *panel)
{
    int i;
    int counterCap;
    int count;
    int row;

    if (panel->cell[ICON_LIST_OWNER].owner != 0) {
        switch (panel->cell[ICON_LIST_STATE].value) {
        case ICON_LIST_SLIDING_IN:
            objHatoVec(&panel->slide, 2);
            if (objStdMove(&panel->slide) != 0) {
                panel->cell[ICON_LIST_STATE].value = ICON_LIST_IDLE;
            }
            break;
        case ICON_LIST_IDLE:
        default:
            if (panel->cell[ICON_LIST_CLOSE].close.request == ICON_LIST_CLOSE_ASKED) {
                panel->cell[ICON_LIST_CLOSE].close.timer = panel->cell[ICON_LIST_CLOSE].close.timer + 1;
                if (panel->cell[ICON_LIST_CLOSE].close.timer >= ICON_LIST_CLOSE_TIME) {
                    row = panel->slide.position.y;
                    panel->cell[ICON_LIST_OWNER].owner = 0;
                    panel->cell[ICON_LIST_STATE].value = ICON_LIST_IDLE;
                    panel->cell[ICON_LIST_COUNT].value = 0;
                    panel->cell[ICON_LIST_COUNTER0].value = 0;
                    panel->cell[ICON_LIST_COUNTER1].value = 0;
                    panel->cell[ICON_LIST_KIND0].value = 0;
                    panel->cell[ICON_LIST_KIND1].value = 0;
                    menuPanelRowFree(row);
                }
            }
            break;
        }
        count = panel->cell[ICON_LIST_COUNT].value;
        counterCap = ICON_ENTRY_COUNTER_MAX;
        for (i = 0; i < count; i++) {
            if (i >= 4) {
                panel->cell[ICON_LIST_COUNTER1].counter[i % 4]++;
                if (panel->cell[ICON_LIST_COUNTER1].counter[i % 4] > counterCap) {
                    panel->cell[ICON_LIST_COUNTER1].counter[i % 4] = counterCap;
                }
            } else {
                panel->cell[ICON_LIST_COUNTER0].counter[i % 4]++;
                if (panel->cell[ICON_LIST_COUNTER0].counter[i % 4] > counterCap) {
                    panel->cell[ICON_LIST_COUNTER0].counter[i % 4] = counterCap;
                }
            }
        }
    }
}

/*
 * menuIconListDraw: while the list has an owner it draws its header and then one
 * icon per entry (count at +0x98, up to 8) with menuIconEntryDraw, using the
 * entry's kind and mark bytes. The count-based footer is drawn after the rows.
 */
void menuIconListDraw(IconListPanel *panel)
{
    int i;
    int x;

    if (panel->cell[ICON_LIST_OWNER].owner) {
        GrPacket packet;
        int count;
        int y;

        x = panel->slide.position.x;
        y = panel->slide.position.y;
        i = 0;
        menuTexFlush();
        grGpInit(&packet);
        menuPanelTexState(&packet);
        grOpenSpr(&packet);
        menuStatSprite(&packet, 0, y + 10, x + 66, 10, 375 - x, 74);
        menuStatSprite(&packet, x, y, 56, 18, 384, 36);
        {
            float iconOffset = 72.0f;

            x = panel->slide.goal.x + iconOffset;
        }
        count = panel->cell[ICON_LIST_COUNT].value;
        while (i < count) {
            signed char kind;
            signed char counter;

            if (i >= 4) {
                kind = panel->cell[ICON_LIST_KIND1].kind[i % 4];
            } else {
                kind = panel->cell[ICON_LIST_KIND0].kind[i % 4];
            }
            if (!(kind & ICON_ENTRY_KIND_MASK)) {
                break;
            }
            if (i >= 4) {
                counter = panel->cell[ICON_LIST_COUNTER1].counter[i % 4];
            } else {
                counter = panel->cell[ICON_LIST_COUNTER0].counter[i % 4];
            }
            menuIconEntryDraw(&packet, x, y, kind & ICON_ENTRY_KIND_MASK, (unsigned int) kind >> 31, counter);
            x = x + 24;
            i++;
        }
        grCloseSpr();
        grPacketSend(&packet);
    }
}

/*
 * menuIconListAppend (called by attackHitsShow): appends one entry to the list for
 * the unit (up to ICON_LIST_MAX_ENTRIES; the unit body's entry table at +0xDA and
 * +0xE2 gives the kind and mark). When the unit has no entries, or its state70 is
 * 2 or 3, it requests the close instead. An unowned list is opened for the unit at
 * origin (-152, row).
 */
void menuIconListAppend(BattleUnit *unit)
{
    StatBlock *body;
    IconListPanel *panel;
    int count;
    int kind;
    int row;

    body = unit->stats;
    panel = menuIconListPanel;
    if (body->hitZoneCount <= 0 || body->breakState == 2 || body->breakState == 3) {
        menuIconListClose();
        return;
    }
    count = panel->cell[ICON_LIST_COUNT].value;
    kind = body->hitZoneKind[count];
    if (body->hitZoneMark[count] != 0) {
        kind |= ICON_ENTRY_MARK;
    }
    if (count >= 4) {
        panel->cell[ICON_LIST_KIND1].kind[count % 4] = kind;
    } else {
        panel->cell[ICON_LIST_KIND0].kind[count % 4] = kind;
    }
    if (panel->cell[ICON_LIST_COUNT].value < ICON_LIST_MAX_ENTRIES) {
        panel->cell[ICON_LIST_COUNT].value = panel->cell[ICON_LIST_COUNT].value + 1;
    }
    if (panel->cell[ICON_LIST_OWNER].owner == 0) {
        panel->cell[ICON_LIST_OWNER].owner = unit;
        panel->cell[ICON_LIST_STATE].value = ICON_LIST_SLIDING_IN;
        panel->cell[ICON_LIST_CLOSE].value = 0;
        row = menuPanelRowAlloc();
        panel->slide.position.x = -152.0f;
        panel->slide.goal.x = 16.0f;
        panel->slide.position.y = row;
        panel->slide.goal.y = row;
    }
}

/* menuIconListClose: requests the icon list to close (the step counts its timer). */
void menuIconListClose(void) {
    menuIconListPanel->cell[ICON_LIST_CLOSE].close.request = ICON_LIST_CLOSE_ASKED;
}

/*
 * menuIconListShow, argument unit: for a unit of kind 0 with entries, frees the
 * list's row if it is open, copies up to 8 entries from the unit body (count at
 * +0xEC), and opens the list at origin (16, row) with its timer at
 * ICON_LIST_CLOSE_TIME. Otherwise it requests the close.
 */
void menuIconListShow(BattleUnit *unit)
{
    IconListPanel *panel;
    int i;
    int kind;
    int row;

    panel = menuIconListPanel;
    if (unit->stats->hitZoneCount <= 0 || unit->stats->breakState != 0) {
        menuIconListClose();
        panel->cell[ICON_LIST_CLOSE].close.timer = ICON_LIST_CLOSE_TIME;
        return;
    }
    if (panel->cell[ICON_LIST_OWNER].owner != 0) {
        menuPanelRowFree(panel->slide.position.y);
    }
    for (i = 0; i < unit->stats->hitZoneCount; i++) {
        kind = unit->stats->hitZoneKind[i];
        kind = unit->stats->hitZoneMark[i] != 0 ? (kind | ICON_ENTRY_MARK) : kind;
        if (i >= 4) {
            panel->cell[ICON_LIST_KIND1].kind[i % 4] = kind;
            panel->cell[ICON_LIST_COUNTER1].counter[i % 4] = ICON_ENTRY_COUNTER_MAX;
        } else {
            panel->cell[ICON_LIST_KIND0].kind[i % 4] = kind;
            panel->cell[ICON_LIST_COUNTER0].counter[i % 4] = ICON_ENTRY_COUNTER_MAX;
        }
    }
    panel->cell[ICON_LIST_OWNER].owner = unit;
    panel->cell[ICON_LIST_STATE].value = ICON_LIST_IDLE;
    panel->cell[ICON_LIST_COUNT].value = unit->stats->hitZoneCount;
    panel->cell[ICON_LIST_CLOSE].close.timer = ICON_LIST_CLOSE_TIME;
    panel->cell[ICON_LIST_CLOSE].close.request = 0;
    row = menuPanelRowAlloc();
    panel->slide.position.x = 16.0f;
    panel->slide.position.y = row;
}

/*
 * menuIconEntryDraw: draws one icon of an entry. The entry kind selects the source
 * column 0x1BA (kind 1), 0x1D2 (kind 2) or 0x1EA (kind 4); other kinds draw nothing.
 * The source rectangle is 0x15 texels wide. The colour index (the mark bit of the
 * entry) picks the plain or the marked colour in menuIconColors, and the icon grows
 * by 12 pixels for each slot away from slot 4.
 */
void menuIconEntryDraw(void *packet, int x, int y, int kind, int colourIndex, int slot)
{
    int srcLeft;
    int dx;
    int span;
    int srcRight;
    int half;
    int fromCentre;

    dx = slot;
    switch (kind) {
    case 1:
        srcLeft = ICON_SRC_LEFT_KIND1;
        break;
    case 2:
        srcLeft = ICON_SRC_LEFT_KIND2;
        break;
    case 4:
        srcLeft = ICON_SRC_LEFT_KIND4;
        break;
    default:
        return;
    }
    srcRight = srcLeft + ICON_SRC_WIDTH;
    fromCentre = dx - 4;
    if (fromCentre >= 0) {
        span = dx * 12 - 48;
    } else {
        span = dx * -12 + 48;
    }
    dx = span;
    half = dx / 2;
    menuStatSprite2RGB(packet, x - half, y - half, dx + 24, dx + 18, srcLeft, 36, srcRight, 54,
                       &menuIconColors[colourIndex]);
}

/* The icon list panel, a common global. */
IconListPanel *menuIconListPanel;

/* Plain and marked colour of an icon (r, g, b, a). */
MenuColor menuIconColors[2] = {
    { .rgba = { 0x80, 0x80, 0x80, 0x80 } },
    { .rgba = { 0xff, 0x60, 0x60, 0x80 } }
};
