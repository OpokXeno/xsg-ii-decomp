#ifndef INCLUDE_MAIN_ACT_3_H
#define INCLUDE_MAIN_ACT_3_H

#include "shared.h"

/* Functions of src/main/act_3.c that other files call. ACT_DrawShadow is not declared here: it takes the actor as
 * act_3.c's own ActShadowActor layout view of XenoActor (see there), so its one caller (xenoActor.c) declares it. */
void ACT_DrawShadowBegin(void);
void ACT_DrawShadowEnd(void);

#endif /* INCLUDE_MAIN_ACT_3_H */
