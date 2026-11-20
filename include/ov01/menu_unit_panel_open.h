#ifndef INCLUDE_OV01_MENU_UNIT_PANEL_OPEN_H
#define INCLUDE_OV01_MENU_UNIT_PANEL_OPEN_H

/* Functions of src/ov01/menu_unit_panel_open.c that other files call. */

struct BattleUnit;

void menuUnitPanelArmTimer(void);
void menuUnitPanelOpen(struct BattleUnit *unit, int mode);
/* Binds the unit status panel to the unit and places its origin at (16, 80). */
void menuUnitPanelBind(struct BattleUnit *unit);

#endif /* INCLUDE_OV01_MENU_UNIT_PANEL_OPEN_H */
