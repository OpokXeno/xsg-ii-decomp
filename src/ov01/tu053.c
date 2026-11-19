/*
 * OV01 original TU 53: 0x00aa4670..0x00aa4bd8 (3 functions)
 */
#include "common.h"
#include "ov01/tu005.h"
#include "shared.h"
#include "ov01/tu023.h"
#include "ov01/tu062.h"

/* The boost gauge bar is MENU_VALUE_BAR_WIDTH pixels wide; a displayed gauge of MENU_VALUE_GAUGE_FULL fills it. */
#define MENU_VALUE_BAR_WIDTH 0x74
#define MENU_VALUE_GAUGE_FULL 50
/* GS_ALPHA_1, the GS register number of the ALPHA_1 state (include/gs.h names it the same way). */
#define GS_ALPHA_1 0x42

struct MenuValuePanel;

typedef void (*MenuValuePanelCallback)(struct MenuValuePanel *panel);

/* The menu value panel object slotAlloc hands out. The two callbacks run each frame; x and y are the screen position; gauge holds the displayed value of each boost gauge (boostGaugeSelect index 0 and 1), slid toward its target while counter is zero. */
struct MenuValuePanel {
    unsigned char unmodeled_00[0x4];
    MenuValuePanelCallback update; /* +0x04 */
    MenuValuePanelCallback draw; /* +0x08 */
    unsigned char unmodeled_0c[0x4];
    float x; /* +0x10 */
    float y; /* +0x14 */
    int state; /* +0x18 */
    unsigned char unmodeled_1c[0x74];
    int counter; /* +0x90 */
    float gauge[2]; /* +0x94 */
};

void *objSlotAlloc(void);
/* Slides each displayed gauge toward its boost gauge while counter is zero: within half a unit it snaps to the target, otherwise it closes an eighth of the distance. */
void menuValueSlideUpdate(struct MenuValuePanel *panel);
/* Draws both boost gauges at the panel position: each as a sprite bar filled to its gauge value, with its stock count beside it; a full gauge flashes with the pulse. */
void menuValuePanelDraw(struct MenuValuePanel *panel);
extern void *menuValuePanelObj;

/* Creates the panel in the slot objSlotAlloc returns, with both callbacks, its position and zeroed slide state. */
void menuValuePanelCreate(void)
{
    struct MenuValuePanel *panel;

    panel = objSlotAlloc();
    if (panel != 0) {
        panel->update = menuValueSlideUpdate;
        panel->draw = menuValuePanelDraw;
        menuValuePanelObj = panel;
        ov01MatrixBlockInit(&panel->x);
        panel->x = 376.0f;
        panel->y = 404.0f;
        panel->gauge[1] = 0.0f;
        do {
            panel->state = 0;
        } while (0);
        do {
            panel->counter = 0;
        } while (0);
        panel->gauge[0] = 0.0f;
    }
}

/* Slides each displayed gauge toward its boost gauge while counter is zero: within half a unit it snaps to the target, otherwise it closes an eighth of the distance. */
void menuValueSlideUpdate(struct MenuValuePanel *panel)
{
    float *displayed;
    float distance;
    unsigned char targetRaw;
    float absDistance;

    if (panel->counter != 0) {
        return;
    }
    {
        struct BoostGauge *boost = boostGaugeSelect(0);

        displayed = &panel->gauge[0];
        distance = boost->progress - *displayed;
        targetRaw = boost->progress;
        if ((distance < 0.0f) ? ((absDistance = -distance) < 0.5f) : (distance < 0.5f)) {
            *displayed = (signed char)targetRaw;
        } else {
            *displayed = *displayed + ((signed char)targetRaw - *displayed) * 0.125f;
        }
    }
    {
        struct BoostGauge *boost = boostGaugeSelect(1);

        displayed = &panel->gauge[1];
        distance = boost->progress - *displayed;
        targetRaw = boost->progress;
        if ((distance < 0.0f) ? ((absDistance = -distance) < 0.5f) : (distance < 0.5f)) {
            *displayed = (signed char)targetRaw;
        } else {
            *displayed = *displayed + ((signed char)targetRaw - *displayed) * 0.125f;
        }
    }
}

void grPutSprOpen(GrPacket *packet);
void grPutSprClose(void);
void func_00AA8490(GrPacket *packet);
int ov01SceneFrameCount(void);

/* Draws both boost gauges at the panel position: each as a sprite bar filled to its gauge value, with its stock count beside it; a full gauge flashes with the pulse. */
void menuValuePanelDraw(struct MenuValuePanel *panel)
{
    GrPacket packet;
    struct BoostGauge *boost;
    int x;
    int y;
    int flashAlpha;
    int fillWidth0;
    int fillWidth1;
    int shownValue; /* the gauge value, then the bar width the full check compares with, then the stock count */
    int numberX;

    grGpInit(&packet);
    func_00AA8490(&packet);
    x = panel->x;
    y = panel->y;
    /* the flash fades the ALPHA_1 FIX byte (bits 32..39) in a triangle: 8 steps per frame */
    flashAlpha = (ov01SceneFrameCount() * 8) & 0xFF;
    if (flashAlpha >= 0x80) {
        flashAlpha = 0xFF - flashAlpha;
    }
    flashAlpha += 0xA;

    boost = boostGaugeSelect(0);
    shownValue = panel->gauge[0];
    fillWidth0 = shownValue * MENU_VALUE_BAR_WIDTH / MENU_VALUE_GAUGE_FULL;
    grGsRegSet(&packet, GS_ALPHA_1, 0x44);
    grPutSprOpen(&packet);
    menuStatSprite(&packet, x, y - 0x18, MENU_VALUE_BAR_WIDTH, 9, 0x48, 0xB0);
    menuStatSprite(&packet, x, y - 0x21, 0x2E, 8, 0x48, 0x9D);
    grPutSprClose();
    {
        shownValue = fillWidth0;
        if (boost->stock >= boost->maxStock && shownValue == MENU_VALUE_BAR_WIDTH) {
            grGsRegSet(&packet, GS_ALPHA_1, ((u64)flashAlpha << 32) | 0x68);
        }
    }
    grPutSprOpen(&packet);
    menuStatSprite(&packet, x, y - 0x1A, fillWidth0, 0xA, 0x48, 0xA7);
    grPutSprClose();

    boost = boostGaugeSelect(1);
    shownValue = panel->gauge[1];
    fillWidth1 = shownValue * MENU_VALUE_BAR_WIDTH / MENU_VALUE_GAUGE_FULL;
    grGsRegSet(&packet, GS_ALPHA_1, 0x44);
    grPutSprOpen(&packet);
    menuStatSprite2(&packet, x, y + 0x10, MENU_VALUE_BAR_WIDTH, 0xA, 0x48, 0xB9, 0xBB, 0xB0);
    menuStatSprite(&packet, x, y + 0x19, 0x2E, 8, 0x78, 0x9D);
    grPutSprClose();
    {
        shownValue = fillWidth1;
        if (boost->stock >= boost->maxStock) {
            if (shownValue == MENU_VALUE_BAR_WIDTH) {
                grGsRegSet(&packet, GS_ALPHA_1, ((u64)flashAlpha << 32) | 0x68);
            }
        }
    }
    numberX = x + MENU_VALUE_BAR_WIDTH;
    grPutSprOpen(&packet);
    menuStatSprite2(&packet, x, y + 0x13, fillWidth1, 8, 0x48, 0xB0, fillWidth1 + 0x48, 0xA7);
    grPutSprClose();
    grGsRegSet(&packet, GS_ALPHA_1, 0);
    boost = boostGaugeSelect(0);
    menuNumDraw(&packet, numberX, y - 0x24, boost->stock, 1, 0);
    boost = boostGaugeSelect(1);
    shownValue = boost->stock;
    menuNumDraw(&packet, numberX, y + 0xC, shownValue, 1, 0);
    grPacketSend(&packet);
}

/* The value panel object, a common global: tu066 names it too (as unsigned int *). */
void *menuValuePanelObj;
