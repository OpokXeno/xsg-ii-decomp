#ifndef INCLUDE_MAIN_ELEVATOR_H
#define INCLUDE_MAIN_ELEVATOR_H

#include "shared.h"

struct UwamonoRecord;

/* Functions of src/main/elevator.c that other files call. */

void elvReset(void);
void elvGetPosition(int elevator_index, Vector4 *position_out, Vector4 *delta_out);
void elvUpdateMotion(struct UwamonoRecord *holder);
int unitPlayVoiceCueId(void);
int unitActionVoiceCueId(void);

#endif /* INCLUDE_MAIN_ELEVATOR_H */
