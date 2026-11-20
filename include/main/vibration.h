#ifndef INCLUDE_MAIN_VIBRATION_H
#define INCLUDE_MAIN_VIBRATION_H

/* Functions of src/main/vibration.c that other files call. */

void Vibration_Stop(void);
void Vibration_Set_Weak(int value);
void Vibration_Set_Strong(unsigned char startValue, unsigned char endValue, int framesTotal);
void VibrationEngine(void);

#endif /* INCLUDE_MAIN_VIBRATION_H */
