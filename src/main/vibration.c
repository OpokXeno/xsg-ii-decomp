#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "main/game_camera.h"
#include "main/game_data.h"
#include "main/vibration.h"

/* Indices into PadData[0].actuator: the small-motor on/off byte and the large-motor strength byte. */
enum {
    VIBRATION_RUNNING_FLAG = 0,
    VIBRATION_VALUE_BYTE = 1
};
/* Clears the fade state: the hold countdown, the fade value and the running flag. */

void Vibration_Stop(void) {
    do {
        gameWork.vibrationHold = 0;
        PadData[0].actuator[VIBRATION_VALUE_BYTE] = 0;
    } while (0);
    do {
        gameWork.vibrationFramesLeft = 0;
        PadData[0].actuator[VIBRATION_RUNNING_FLAG] = 0;
    } while (0);
}

/* Sets the hold countdown of the vibration fade. */
void Vibration_Set_Weak(int value) {
    gameWork.vibrationHold = value;
}

void Vibration_Set_Strong(unsigned char targetValue, unsigned char currentValue, int framesTotal) {
    gameWork.vibrationTarget = targetValue;
    gameWork.vibrationValue = currentValue;
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
        PadData[0].actuator[VIBRATION_RUNNING_FLAG] = 1;
        gameWork.vibrationHold = remaining;
    }
    if (gameWork.vibrationFramesLeft != 0) {
        int remaining;
        int fadeStep;

        fadeStep = (gameWork.vibrationTarget - gameWork.vibrationValue) * gameWork.vibrationFramesLeft / gameWork.vibrationFramesTotal;
        remaining = 0;
        if (gameWork.vibrationFramesLeft - 1 >= 0) {
            remaining = gameWork.vibrationFramesLeft - 1;
        }
        gameWork.vibrationFramesLeft = remaining;
        PadData[0].actuator[VIBRATION_VALUE_BYTE] = gameWork.vibrationValue + fadeStep;
    }
}
