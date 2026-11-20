#ifndef INCLUDE_OV01_MENU_COMBO_PANEL_H
#define INCLUDE_OV01_MENU_COMBO_PANEL_H

/* Functions of src/ov01/menu_combo_panel.c that other files call. */

struct BattleUnit;
struct MenuState;

/* Binds the unit to the timed sequence (or arms its tick, below MENU_TIMED_SEQ_MIN_HITS hits); called by
 * dataTargetShow (battle_init.c). */
void menuComboPanelShow(struct BattleUnit *unit);
void menuComboPanelArmTick(void);
void menuComboPanelNew(void);
void menuComboPanelStart(struct BattleUnit *unit);
/* Draws a timed panel while it has an owner: the title from menuTimedTitleTable, then the combo count. Also the draw
 * of the extra panels menuComboPanelExtraNew makes. */
void menuComboPanelDraw(struct MenuState *sequence);
/* The timed sequence panel menuComboPanelNew makes. */
extern struct MenuState *menuTimedSeqPanel;

#endif /* INCLUDE_OV01_MENU_COMBO_PANEL_H */
