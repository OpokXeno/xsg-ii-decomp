#ifndef INCLUDE_MAIN_MENU_SCENE_H
#define INCLUDE_MAIN_MENU_SCENE_H

#include "main/cursor_buffer.h"

/* Functions of src/main/menu_scene.c that other files call. */

/* The config screen's button index of each caption setting (8 bytes, declared without a size). */
extern signed char menuConfigButtonIndexTable[];

void menuSceneReset(void);
int menuSceneRunLoop(void);
void menuSceneRunKeyCheck(void);
int menuSceneRunSplit(void);
void menuSkillModelReset(void);
void menuSkillModelTailReset(void);
void menuSkillModelReadDone(int key);
void menuUmnImageDecode(u8 sequenceIndex);
void cursorBufferTablesInit(void);
void cursorBuffersRewind(void);
void cursorBufferSetOrInit(ListCursor *buffer, int capacity, int unit_size, int mode_flags);

/* ov01 battle functions (src/ov01/calc.c) that this file calls. */
int calcUseItemMenu(int key, int value);
int calcSkillEquipOrg(int first, int index, int dispatch_value);
int calcAccEquipOrg(int battle, int slot, int value);
int slotPairSet(int value);
int slotPairTest(int value);

#endif /* INCLUDE_MAIN_MENU_SCENE_H */
