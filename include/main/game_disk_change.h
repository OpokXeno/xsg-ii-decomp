#ifndef INCLUDE_MAIN_GAME_DISK_CHANGE_H
#define INCLUDE_MAIN_GAME_DISK_CHANGE_H

/* Functions of src/main/game_disk_change.c that other files call. */

/* Asks for the disc given in the low nibble of request (see the definition); -1 for an invalid disc, else 0. */
int GameDiskChange(int request);
/* The synopsis screen shown before a new game or a loaded game. */
void GameSynopsis(void);
/* Cross-fades the synopsis panels until decide; 1 when the soft-reset combination was pressed, else 0. */
int GameSynopsisCrossfadeWait(void);

#endif /* INCLUDE_MAIN_GAME_DISK_CHANGE_H */
