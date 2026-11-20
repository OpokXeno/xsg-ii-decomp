#include "common.h"
#include "ov01/menu_unit_panel_open.h"
#include "ov01/unit_cmd.h"
#include "ov01/menu_unit_panel.h"
#include "ov01/menu_panel.h"
#include "ov01/calc.h"

/* Kinds and statPanelState values of the unit status panel; its states are in ov01/menu_unit_panel.h. */
#define UNIT_PANEL_KIND_STAT 1
#define STAT_PANEL_PENDING 1
#define STAT_PANEL_SHOWN 2

extern UnitPanel *menuUnitPanel;

/*
 * menuUnitPanelOpen: opens the unit status panel menuUnitPanel (ov01/menu_unit_panel.c) for the
 * unit, arguments unit and kind. For kind 1 the first call creates a stat panel
 * with menuStatPanelNew(0) and marks the word at +0xC as 2. When the panel's kind
 * at +0x98 differs, the panel takes the unit as owner, state 1 (slide in), origin
 * (-64, 80) and target (16, 80), and disarms its timer.
 */
void menuUnitPanelOpen(BattleUnit *unit, int kind)
{
    UnitPanel *panel;

    panel = menuUnitPanel;
    if (kind == UNIT_PANEL_KIND_STAT) {
        if (actStepCounterGet()->statPanelState == STAT_PANEL_PENDING) {
            menuStatPanelNew(0);
            actStepCounterGet()->statPanelState = STAT_PANEL_SHOWN;
        }
    }
    if (panel->kind != kind) {
        panel->owner = unit;
        panel->state = UNIT_PANEL_SLIDING_IN;
        panel->kind = kind;
        panel->slide.position.x = -64.0f;
        panel->slide.goal.x = 16.0f;
        panel->timers = 0;
        panel->slide.position.y = 80.0f;
        panel->slide.goal.y = 80.0f;
    }
}

/* menuUnitPanelArmTimer: starts the auto-close count of the unit status panel. */
void menuUnitPanelArmTimer(void) {
    menuUnitPanel->timerArmed = 1;
}

/*
 * menuUnitPanelBind: binds the panel to the unit, argument unit. It sets the owner
 * to the unit, the state to 0 and the kind to the unit body's kind byte at +0x70,
 * sets the timer at +0x9E to 16 with timerArmed 0 (the count is not running), and
 * places the origin at (16, 80).
 */
void menuUnitPanelBind(BattleUnit *unit)
{
    UnitPanel *panel;
    float originX = 16.0f;
    float originY = 80.0f;

    panel = menuUnitPanel;
    panel->owner = unit;
    panel->state = UNIT_PANEL_IDLE;
    panel->kind = unit->stats->breakState;
    panel->slide.position.y = originY;
    panel->timer = UNIT_PANEL_CLOSE_TIME;
    panel->slide.position.x = originX;
    panel->timerArmed = 0;
}
