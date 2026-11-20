#ifndef INCLUDE_MAIN_TYA_DISPLAY_SETTING_H
#define INCLUDE_MAIN_TYA_DISPLAY_SETTING_H

/* Functions of src/main/tya_display_setting.c that other files call. */

/* Native names of the elevator module (src/main/elevator.c registers them); .rodata strings, declared without a size. */
extern const char elevatorNativeNamePreload[];
extern const char elevatorNativeNamePreren[];
extern const char elevatorNativeNameKeepOff[];
extern const char elevatorNativeNameElvMove[];

void actionSoundSlotLevelSet(int index);
void bgmSlotLevelSet(int index);
void streamKeySlotLevelSet(int index);
int streamSetLevel(int index);

#endif /* INCLUDE_MAIN_TYA_DISPLAY_SETTING_H */
