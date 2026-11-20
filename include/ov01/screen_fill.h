#ifndef INCLUDE_OV01_SCREEN_FILL_H
#define INCLUDE_OV01_SCREEN_FILL_H

#include "ov01/ov01_task.h"

/* Menu panel rows: MENU_PANEL_ROW_COUNT slots (screen_fill.c holds the storage), cleared by menuPanelRowsReset. */
#define MENU_PANEL_ROW_COUNT 10
extern int menuPanelRows[MENU_PANEL_ROW_COUNT];

/* The running grey screen fill, 0 when none. */
extern ScreenTask *screenFillTask;

/* Functions of src/ov01/screen_fill.c that other files call. */

/* Draws a fill of the given RGB colour over the screen (the grey fill of screen_flash.c's flash uses it). */
void grPacketScreenFill(int r, int g, int b);
/* Starts and ends the grey screen fill (a ScreenTask claimed from ov01TaskTable). */
void screenFillStart(void);
void screenFillEnd(void);

#endif /* INCLUDE_OV01_SCREEN_FILL_H */
