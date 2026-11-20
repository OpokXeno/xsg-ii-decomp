#ifndef INCLUDE_MAIN_GAME_PAUSE_DISP_H
#define INCLUDE_MAIN_GAME_PAUSE_DISP_H

/* Functions of src/main/game_pause_disp.c that other files call. */

/* Names of the map object resources, loaded in order (defined here with the pause data). */
extern const char *mapObjectResourceNames[];

void pauseStart(void);
void GamePauseDispBG(void);
void pauseDrawScreen(void);
void GamePauseDispEvent(int skip_hidden);

#endif /* INCLUDE_MAIN_GAME_PAUSE_DISP_H */
