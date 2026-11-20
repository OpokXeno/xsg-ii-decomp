#ifndef INCLUDE_OV01_MENU_WEP_SP_PANEL_H
#define INCLUDE_OV01_MENU_WEP_SP_PANEL_H

#include "ov01/obj.h"

/* Functions of src/ov01/menu_wep_sp_panel.c that other files call, and the menu panel record that
 * menu_wep_sp_panel.c and src/ov01/menu_panel.c share. */

/* Request state of a menu panel (state at +0x90): menuWepSpPanelShow and Hide set a request, the step consumes it. */
enum MenuPanelState {
    MENU_PANEL_STATE_IDLE = 0,
    MENU_PANEL_STATE_SHOW_REQUEST = 1,
    MENU_PANEL_STATE_HIDE_REQUEST = 2
};

/* The menu panel record behind menuWepSpPanel, menuTargetListPanel, menuTgtStatusPanel (menu_wep_sp_panel.c)
 * and menuPanelCurrent (menu_panel.c).
 * origin is the slide position, aux the (16, row) pair of a shown panel, state the request word (0 idle, 1 show,
 * 2 hide for menu_wep_sp_panel.c; 0 idle or counting down, 1 sliding in for menu_panel.c), flag nonzero while the
 * panel is shown, wait, value and countdown are menu_panel.c's stat and timed panel words. The 0x18 word has two
 * views: the alias-0 store follows the origin stores. */
typedef struct MenuState {
    u32 slotFlags;                  /* +0x00: the slot flags (OV01_SLOT_*); decoFlagsSetAll hides the target status panel with SKIP_DRAW */
    union {
        void (*update)(struct MenuState *); /* +0x04: the state step, menuPanelStep, menuWepSpPanelStep or menuTargetListStep */
        int (*updateStatus)(struct MenuState *); /* the same slot for menuComboPanelExtraStep, which returns a status the slot sweep ignores */
    };
    void (*draw)(struct MenuState *);   /* +0x08: the draw, menuPanelDraw, menuWepSpPanelDraw or menuTargetListDraw */
    unsigned char unmodeled_0C[4];
    SlideBlock slide;                   /* +0x10: the slide block; position is the panel origin, x and y, that slides toward goal (16, row) on a panel shown in a slot */
    union {
        int state;                      /* +0x90 */
        unsigned int stateWord;         /* the same word, read in its own alias set: the store follows the palette stores */
    };
    union {
        int flag;                       /* +0x94: nonzero while the panel is shown and drawn */
        unsigned int flagWord;          /* the same word, read in its own alias set: the palette copy stores follow it */
        struct BattleUnit *owner;       /* the timed sequence (menuComboPanel*): the unit it shows, zero when it is free */
    };
    int wait;                           /* +0x98: ticks down while state is 0 (menuStatPanelStep); the timed sequence's title slot */
    int value;                          /* +0x9C: the number a stat panel prints at kind 7 (menuStatPanelDraw); the combo count of the timed sequence */
    union {
        int countdown;                  /* +0xA0: ticks down while state is 0 (menuComboPanelExtraStep) */
        struct {
            s16 tickEnabled;            /* +0xA0: the timed sequence: set by menuComboPanelArmTick, lets the ticks count */
            u16 tickCount;              /* +0xA2: counts ticks; MENU_TIMED_SEQ_TICKS ends the sequence */
        };
    };
} MenuState;

/* The panels the constructors below make. */
extern MenuState *menuWepSpPanel;
extern MenuState *menuTargetListPanel;
extern MenuState *menuTgtStatusPanel;

void menuWepSpPanelNew(void);
void menuTargetListNew(void);
void menuTgtStatusNew(void);
void menuWepSpPanelShow(void);
void menuWepSpPanelHide(void);
void menuTargetListShow(void);
void menuTargetListHide(void);
void menuTgtStatusHide(void);
/* Sets the visible flag of the target status panel. */
void menuTgtStatusShow(void);

#endif /* INCLUDE_OV01_MENU_WEP_SP_PANEL_H */
