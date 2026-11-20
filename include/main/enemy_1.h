#ifndef INCLUDE_MAIN_ENEMY_1_H
#define INCLUDE_MAIN_ENEMY_1_H

#include "shared.h"

/* Functions of src/main/enemy_1.c that other files call. */

void Enemy_Init(int slotIndex);
void ACT_updateEnemy(int index);
int uwamonoMoveAlongHeading(int index, Vector4 *target, float speed);
int uwamonoCopyPointIfNear(int index, const Vector4 *point, signed char copyFlag);
int TM_Check_Before_Eyes(int slotIndex, float length);
void uwamonoSetMotion(int slotIndex, int motion, float speed);
void Enemy_ActionReady(int slot_index, signed char action);

#endif /* INCLUDE_MAIN_ENEMY_1_H */
