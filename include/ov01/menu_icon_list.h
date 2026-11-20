#ifndef INCLUDE_OV01_MENU_ICON_LIST_H
#define INCLUDE_OV01_MENU_ICON_LIST_H

/* Functions of src/ov01/menu_icon_list.c that other files call. */

struct BattleUnit;

void menuIconListClose(void);
void menuIconListNew(void);
void menuIconListAppend(struct BattleUnit *unit);
void menuIconListShow(struct BattleUnit *unit);

#endif /* INCLUDE_OV01_MENU_ICON_LIST_H */
