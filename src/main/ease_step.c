#include "common.h"
#include "main/ease_step.h"

/* The change from start to target divided by divisor, as a float then truncated to int. */
int deltaPerStep(int start, int target, int divisor) {
    return (int) ((float) (target - start) / (float) divisor);
}

/* The ease step override: easeStepOverrideEnabled is its enable flag, easeStepOverrideValue its value (set by easeStepOverrideSet, cleared by easeStepOverrideClear). */
extern int easeStepOverrideEnabled;
extern int easeStepOverrideValue;

/* Degrees added per frame to a cosine ease that runs from 0 to 180 degrees (interp_update.c interpStepCosine,
 * msg_window_update.c and timecode_frames.c count their angle up in these steps). */
#define ANGLE_STEP_NEAR 45         /* step when the distance is under ANGLE_STEP_NEAR_LIMIT */
#define ANGLE_STEP_FAR 23          /* step otherwise */
#define ANGLE_STEP_NEAR_LIMIT 31   /* distance (in the caller's units) below which the near step applies */

/*
 * Angle step for an ease from start to target: the override value when easeStepOverrideEnabled is set; otherwise
 * ANGLE_STEP_NEAR when |target - start| is under ANGLE_STEP_NEAR_LIMIT and ANGLE_STEP_FAR when it is not.
 * __builtin_abs is required for the bytes: the plain `d < 0 ? -d : d` is if-converted, while the
 * original branches with bltzl (the builtin keeps that shape).
 */
int easeAngleStep(int start, int target)
{
    int distance = __builtin_abs(target - start);
    int step = ANGLE_STEP_FAR;

    if (distance < ANGLE_STEP_NEAR_LIMIT) {
        step = ANGLE_STEP_NEAR;
    }
    if (easeStepOverrideEnabled != 0) {
        return easeStepOverrideValue;
    }
    return step;
}

/* Sets the override value and turns the override on. */
void easeStepOverrideSet(int value) {
    easeStepOverrideValue = value;
    easeStepOverrideEnabled = 1;
}

/* Turns the override off. */
void easeStepOverrideClear(void) {
    easeStepOverrideEnabled = 0;
}

/* Override state: enable flag and value, both zero until easeStepOverrideSet. */
int easeStepOverrideEnabled = 0;
int easeStepOverrideValue = 0;
