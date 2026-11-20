#ifndef INCLUDE_OV01_SCENE_SCRIPT_H
#define INCLUDE_OV01_SCENE_SCRIPT_H

/* Functions of src/ov01/scene_script.c that other files call. */

struct SbImageHeader;

struct SbImageHeader *ov01ScriptKeyGet(void);
struct SbImageHeader *ov01ScriptImageGet(void);
void ov01SceneInitDummy(void);

#endif
