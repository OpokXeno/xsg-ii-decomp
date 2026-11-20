#ifndef INCLUDE_OV01_SCENE_H
#define INCLUDE_OV01_SCENE_H

/* Functions of src/ov01/scene.c that other files call. */

/* The battle scene driver: Enter sets the scene up and starts its step chain, Loop runs it until the scene is done or
 * the soft reset combination is pressed, End tears it down. */
void ov01SceneEnter(void);
void ov01SceneLoop(void);
void ov01SceneEnd(void);
int ov01SoftResetPressed(void);
int ov01SceneFrameCount(void);
void ov01SceneRelease(void);
void battleStartBonusApply(void);
struct BattleUnit *selectPL(struct BattleUnit *keep);

#endif /* INCLUDE_OV01_SCENE_H */
