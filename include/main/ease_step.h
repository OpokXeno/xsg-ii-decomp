#ifndef INCLUDE_MAIN_EASE_STEP_H
#define INCLUDE_MAIN_EASE_STEP_H

/* Functions of src/main/ease_step.c that other files call. */

int deltaPerStep(int start, int target, int divisor);
int easeAngleStep(int start, int target);
void easeStepOverrideSet(int value);
void easeStepOverrideClear(void);

#endif /* INCLUDE_MAIN_EASE_STEP_H */
