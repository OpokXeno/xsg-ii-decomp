#ifndef INCLUDE_MAIN_INIT_UWAMONO_SYS_H
#define INCLUDE_MAIN_INIT_UWAMONO_SYS_H

#include "shared.h"

struct UwamonoRecord;
struct XenoActor;

/* Functions of src/main/init_uwamono_sys.c that other files call. */

int uwamonoAddMoveStep(int index, float x, float y);
int uwamonoAddRotateStep(int index, float x, float y);
void uwamonoClearMoveResetWord(int index);
void uwamonoClearRotateResetWord(int index);
void uwamonoUpdateTransform(struct UwamonoRecord *record);
int AimMapUnitLookCheck(struct UwamonoRecord *record);
int CrossPointUwamono(const Vector4 *origin, const Vector4 *target, float radius, AlignedVector4 *out);
int HitCheckMapUnit(struct XenoActor *actor);
int HitCheckMapUnitPos(const Vector4 *position);
int HitCheckMapUnitPosSize(const Vector4 *position, float radius);
int HitCheckMapUnitWithNyuru(struct XenoActor *actor);

#endif /* INCLUDE_MAIN_INIT_UWAMONO_SYS_H */
