#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "main/game_camera.h"
#include "main/game_data.h"
#include "main/vibration.h"

/* Indices into PadData[0].actuator: the small-motor on/off byte and the large-motor strength byte. */
enum {
    VIBRATION_SMALL_MOTOR = 0,
    VIBRATION_LARGE_MOTOR = 1
};
/* Clears the fade state: the hold countdown, the fade value and the running flag. */

void Vibration_Stop(void) {
    do {
        gameWork.vibrationHold = 0;
        PadData[0].actuator[VIBRATION_LARGE_MOTOR] = 0;
    } while (0);
    do {
        gameWork.vibrationFramesLeft = 0;
        PadData[0].actuator[VIBRATION_SMALL_MOTOR] = 0;
    } while (0);
}

/* Keeps the small motor running for frames frames (VibrationEngine counts it down). */
void Vibration_Set_Weak(int frames) {
    gameWork.vibrationHold = frames;
}

/* Starts a fade of the large motor: it begins at startValue and ends near endValue over framesTotal steps. */
void Vibration_Set_Strong(unsigned char startValue, unsigned char endValue, int framesTotal) {
    gameWork.vibrationStart = startValue;
    gameWork.vibrationEnd = endValue;
    gameWork.vibrationFramesLeft = framesTotal;
    gameWork.vibrationFramesTotal = framesTotal;
}

void VibrationEngine(void) {
    if (gameWork.vibrationHold != 0) {
        int remaining;

        remaining = 0;
        if (gameWork.vibrationHold - 1 >= 0) {
            remaining = gameWork.vibrationHold - 1;
        }
        PadData[0].actuator[VIBRATION_SMALL_MOTOR] = 1;
        gameWork.vibrationHold = remaining;
    }
    if (gameWork.vibrationFramesLeft != 0) {
        int remaining;
        int fadeStep;

        fadeStep = (gameWork.vibrationStart - gameWork.vibrationEnd) * gameWork.vibrationFramesLeft / gameWork.vibrationFramesTotal;
        remaining = 0;
        if (gameWork.vibrationFramesLeft - 1 >= 0) {
            remaining = gameWork.vibrationFramesLeft - 1;
        }
        gameWork.vibrationFramesLeft = remaining;
        PadData[0].actuator[VIBRATION_LARGE_MOTOR] = gameWork.vibrationEnd + fadeStep;
    }
}
