#ifndef INCLUDE_MAIN_XENO_ACTOR_SET_H
#define INCLUDE_MAIN_XENO_ACTOR_SET_H

#include "shared.h"
#include "main/xenoActor.h"

void cfeDampDirOffset(struct XenoActor *actor, int dir);
int XenoActorGetHrcPoint(struct XenoActor *actor, Vector4 *out);
void XenoActorSetChannelFlag80(struct XenoActor *actor);
void XenoActorSetState(struct XenoActor *actor, int state);

#endif /* INCLUDE_MAIN_XENO_ACTOR_SET_H */
