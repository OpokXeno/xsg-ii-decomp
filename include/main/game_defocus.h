#ifndef INCLUDE_MAIN_GAME_DEFOCUS_H
#define INCLUDE_MAIN_GAME_DEFOCUS_H

/* Functions of src/main/game_defocus.c that other files call. */

void GameDefocusCheck(void);
/* Sets up layer index with the given type and settings (a negative index finalizes every layer). */
void GameDefocusSet(int index, int type, const int *settings);

#endif /* INCLUDE_MAIN_GAME_DEFOCUS_H */
