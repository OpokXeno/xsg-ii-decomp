#ifndef INCLUDE_MAIN_SET_MOTION_H
#define INCLUDE_MAIN_SET_MOTION_H

#include "shared.h"

/* Functions of src/main/set_motion.c that other files call. */

void Enemy_After_Battle(int unusedKey);
int Check_Actor(const Vector4 *from, int index);
int Check_EnemyFound(void);
void Enemy_FindByEar(int viewerIndex, const Vector4 *viewpoint);
void Check_Discovery(int index);
void Check_Encount(int index, signed char takeValue, int reactionValue);
void Set_Spline_By_Route(int index, short step);
void Homing_Search(int index);
void Homing_Add(int index, short parent, float x, float y, float z);
int Get_MostNear_Actor(short index);
void Actor_LookAt(int index);
int Actor_LookAt_Set(int index, signed char priority, int target, Vector4 *point);
int Actor_LookAt_Release(int index, signed char threshold);

#endif /* INCLUDE_MAIN_SET_MOTION_H */
