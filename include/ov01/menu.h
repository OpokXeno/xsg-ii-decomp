#ifndef INCLUDE_OV01_MENU_H
#define INCLUDE_OV01_MENU_H

#include "ov01/obj_slot.h"
#include "ov01/obj.h"

/* Functions of src/ov01/menu.c that other files call. */

int menuPanelRowAlloc(void);
int menuPanelRowFree(int y);
void menuStatPreObjCreate(void);
void menuPanelRowsReset(void);

void menuStockObjCreate(void);
void menuStockDead(int slot);

/* Types and functions of src/ov01/menu.c that other files use. */

/*
 * The slide block of the menu cursor (+0x10..+0x8F of MenuCursor):
 * menuBoostObjCreate clears it with objStdInit, menuBoostObj steps
 * it with objHatoVec/objStdMove from the position towards the target.
 */
typedef union MenuCursorSlide {
    SlideBlock block;
    struct {
        float x;                          /* +0x00 */
        float y;                          /* +0x04 */
        float z;                          /* +0x08 */
        unsigned char unmodeled_0c[0x60 - 0x0C];
        float target_x;                   /* +0x60 */
        float target_y;                   /* +0x64 */
        float target_z;                   /* +0x68 */
    } pos;
} MenuCursorSlide;

/* A sliding menu object, an objSlotAlloc slot: the menu cursor (menuCursor),
 * the icon list (menuIconListObj) and the five path objects of pStockObj
 * (menu.c) and the stat records of menuStatRecords (menu.c) are built this way. From +0x94 on the fields depend
 * on the kind. */
typedef struct MenuObject MenuObject;

struct MenuObject {
    u32 slotFlags;                    /* +0x00: the slot flags (OV01_SLOT_*); decoFlagsSetAll hides the object with SKIP_DRAW */
    void (*update)(MenuObject *object);   /* +0x04 */
    void (*draw)(MenuObject *object);     /* +0x08 */
    unsigned char unmodeled_0c[0x04];
    MenuCursorSlide slide;                /* +0x10 */
    /* A one-member union: its stores are alias set 0, as in the original, where menuStatRecordsCreate stores
     * moving only after the unit pointer load; as a plain int, sched moves the store ahead of that load. */
    union {
        int moving;                       /* +0x90: 1 while the slide runs to the target */
    };
    union {
        struct {
            int visible;                  /* +0x94: menuBoostObjDraw draws the unit list when 1 */
        } cursor;
        struct {
            struct BattleUnit *unit;      /* +0x94: the unit the record shows */
            float shown[3];               /* +0x98: the gauge values shown, eased towards the unit's by
                                           * menuStatObj */
        } stat;
        struct {
            int slot;                     /* +0x94: its index in pStockObj (and actStockGet's table) */
            int index;                    /* +0x98: current point of stockPosTbl */
            int fade;                     /* +0x9C: stepped by 4 up to 0x18 by menuStockObj */
            int shown;                    /* +0xA0 */
        } path;
    } kind;
};

typedef MenuObject MenuCursor;

extern MenuCursor *menuCursor;
/* The icon list object (menuKeyGuideObjCreate). */
extern MenuObject *menuIconListObj;
/* The stat records menuStatRecordsCreate made, one per slot of the unit table (0 for an empty slot). */
extern MenuObject *menuStatRecords[4];

/* A path object of pStockObj. */
typedef MenuObject PathObject;
#define OBJ_PATH_COUNT 5
extern PathObject *pStockObj[OBJ_PATH_COUNT];

/* The menu value panel object slotAlloc hands out. The two callbacks run each frame; slide.position is the screen
 * position; gauge holds the displayed value of each boost gauge (boostGaugeSelect index 0 and 1), slid toward its
 * target while counter is zero. */
struct MenuValuePanel;
typedef void (*MenuValuePanelCallback)(struct MenuValuePanel *panel);
struct MenuValuePanel {
    u32 slotFlags;                    /* +0x00 */
    MenuValuePanelCallback update; /* +0x04 */
    MenuValuePanelCallback draw; /* +0x08 */
    unsigned char unmodeled_0c[0x4];
    SlideBlock slide; /* +0x10 */
    int counter; /* +0x90 */
    float gauge[2]; /* +0x94 */
};
/* The value panel object menuBoostGaugeCreate makes. */
extern struct MenuValuePanel *menuValuePanelObj;

/* Home position of the cursor, and the parked position menuBoostObjCreate and
 * menuBoostDispOff set. */
#define MENU_CURSOR_HOME_X 288.0f
#define MENU_CURSOR_HOME_Y 390.0f
#define MENU_CURSOR_PARKED_Y 454.0f

void menuBoostObjCreate(void);

void menuKeyGuideObjCreate(void);
void menuBoostDispOff(void);

void arrowRemove(void);

struct ArrowObject;
/* The arrow sprite object (arrowCreate makes it, arrowRemove releases it); 0 when none is shown. */
extern struct ArrowObject *arrowObj;

struct BattleUnit;

/* Makes the object that shows amount at the unit (unit_cmd.c passes HP and EP magnitudes with their kind). */
void objDmgNumCreate(struct BattleUnit *object, int amount, int kind, int sign);
/* Makes the arrow sprite at (x, y); the direction flag picks its texel order (arrowDraw). */
void arrowCreate(int x, int y, int direction);
void menuBoostDispOn(void);
void menuStockStart(void);
void menuStockNext(void);
void menuStockBoost(void);
void menuBoostGaugeCreate(void);
void menuStatRecordsCreate(void);

#endif /* INCLUDE_OV01_MENU_H */
