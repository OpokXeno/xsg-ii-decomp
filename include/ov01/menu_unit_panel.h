#ifndef INCLUDE_OV01_MENU_UNIT_PANEL_H
#define INCLUDE_OV01_MENU_UNIT_PANEL_H

/* Types of src/ov01/menu_unit_panel.c that src/ov01/menu_unit_panel_open.c shares: the unit status panel record
 * and its state values. */

#include "ov01/unit_cmd.h"
#include "ov01/obj.h"

/* Values of UnitPanel.state (menuUnitPanelBind stores idle, menuUnitPanelOpen sliding in), and the timer count at which the panel closes. */
enum UnitPanelState {
    UNIT_PANEL_IDLE = 0,
    UNIT_PANEL_SLIDING_IN = 1
};

#define UNIT_PANEL_CLOSE_TIME 16

/* The unit status panel record. Only the evidenced fields are modeled: the slide block at +0x10 (the slide functions
 * move it; position is the origin and goal the slide end target at +0x70), the state word at +0x90,
 * the owner at +0x94, the unit kind at +0x98 that the timer expiry clears, the halfword timerArmed at +0x9C and the halfword
 * timer at +0x9E. Bytes between the fields are untouched.
 *
 * The owner word holds the unit pointer (menuUnitPanelOpen and menuUnitPanelBind store it there); menuUnitPanelDraw reads the
 * unit body at its +0x144. The owner store of menu_unit_panel_open.c is alias set 0 (the ownerWord view). */
typedef struct UnitPanel UnitPanel;

struct UnitPanel {
    unsigned char unmodeled_00[4];
    void (*update)(UnitPanel *panel);   /* +0x04: the step menuUnitPanelStep */
    void (*draw)(UnitPanel *panel);     /* +0x08: the draw menuUnitPanelDraw */
    unsigned char unmodeled_0C[4];
    SlideBlock slide;                   /* +0x10: the block objHatoVec and objStdMove move; position is the panel
                                         * origin (menu_unit_panel_open.c sets it),
                                         * goal (16, 80) where the slide ends */
    enum UnitPanelState state;          /* +0x90 */
    union {
        BattleUnit *owner;              /* +0x94: the unit the panel shows */
        unsigned int ownerWord;         /* the same word; this store is alias set 0 */
    };
    int kind;                           /* +0x98: the unit body state kind; 1 for the stat panel */
    union {
        struct {
            s16 timerArmed;             /* +0x9C: 1 while the auto-close timer runs */
            s16 timer;                  /* +0x9E: counts up while armed, closes at UNIT_PANEL_CLOSE_TIME */
        };
        int timers;                     /* the two halfwords as one word: New and Open clear both with one store */
    };
};

/* Functions of src/ov01/menu_unit_panel.c that other files call. */
void menuUnitPanelNew(void);

#endif /* INCLUDE_OV01_MENU_UNIT_PANEL_H */
