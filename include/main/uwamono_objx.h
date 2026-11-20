#ifndef INCLUDE_MAIN_UWAMONO_OBJX_H
#define INCLUDE_MAIN_UWAMONO_OBJX_H

#include "shared.h"

/* Functions of src/main/uwamono_objx.c that other files call. */

struct SbThread;

/* Functions of src/main/uwamono_objx.c: the objx script natives and the record creation and movement they use. */

int uwamonoFindIndexByShortKey(short key);
int uwamonoFindIndexByKey(short key);
void uwamonoObjectEntryPositionGet(Vector4 *destination, int index);
void uwamonoObjectEntryAnglesGet(Vector4 *angles, int index);
void scriptRegisterModules(void);
void objxRegisterNatives(void);
void uwamonoStartMove(int index, Vector4Elements *destination, int frames, signed char hasMotion, int motion, signed char mode);
void uwamonoStartScale(int index, Vector4Elements *scale, int frames);
void uwamonoStartTransparency(int index, int frames, float value);
int uwamonoSetTarget(int index, int target, short flag);

#endif /* INCLUDE_MAIN_UWAMONO_OBJX_H */
