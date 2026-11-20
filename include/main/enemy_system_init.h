#ifndef INCLUDE_MAIN_ENEMY_SYSTEM_INIT_H
#define INCLUDE_MAIN_ENEMY_SYSTEM_INIT_H

/* Functions of src/main/enemy_system_init.c that other files call. */

void TM_Script_Spline_Add(int index, float x, float y, float z);
void uwamonoClampToStopRange(int index);
void uwamonoReleaseChildren(int index);

#endif /* INCLUDE_MAIN_ENEMY_SYSTEM_INIT_H */
