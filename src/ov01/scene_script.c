#include "common.h"
#include "main/game_data.h"
#include "ov01/scene_script.h"

/* Returns at once with no effect. Caller: ov01SceneInit. */
void ov01SceneInitDummy(void) {
}

/*
 * The two images of the scene in gameWork (main): resBuffer holds the SB script image (the key of its threads),
 * imageEnd the SBL library image dataThinkScriptLoad reads behind it. ov01StepSceneScript links the library under
 * the key (sbLibraryLink calls sbLinkImage and keeps the image when sbIsSblImage accepts it); ov01SceneRelease
 * releases the key's threads (sbThreadsRelease).
 */

/* The scene's SB script image (gameWork.resBuffer). */
struct SbImageHeader *ov01ScriptKeyGet(void) {
    return (struct SbImageHeader *)gameWork.resBuffer;
}

/* The scene's SBL library image (gameWork.imageEnd). */
struct SbImageHeader *ov01ScriptImageGet(void) {
    return gameWork.imageEnd;
}
