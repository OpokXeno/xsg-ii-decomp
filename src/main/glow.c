#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "main/TiWater.h"
#include "main/game_camera.h"


/* Moves the glow actor by the pad direction of port 0 (x and z), raises and lowers it with the up and
 * down buttons, and turns it towards a held direction. */
void glowUpdateFromPad(struct XenoActor *actor)
{
    actor->position.x += PadData[0].leftX * 0.001f;
    actor->position.z += PadData[0].leftY * 0.001f;
    if (PadData[0].held & PAD_UP) {
        actor->position.y += 0.1f;
    }
    if (PadData[0].held & PAD_DOWN) {
        actor->position.y -= 0.1f;
    }
    if (PadData[0].leftX != 0 || PadData[0].leftY != 0) {
        actor->rotation.y = tiWaterVectorAngle(PadData[0].leftY, PadData[0].leftX);
    }
}

#include "main/xenoActor.h"
#include "main/test_129.h"
#include "main/glow.h"
#include "main/assert_fail.h"
#include "main/gifState.h"
#include "main/xgl_cd.h"
#include "main/xgl_render.h"
#include "main/xgl_studio.h"
#include "main/xgl_thread.h"
#include "main/nml_model_set.h"

extern const char glowLandscapeAssertText[];
/* "pLandscape" */
extern const char glowSourceName[];
/* "../glow.euc.c" */
extern const char glowLandscapeModelPath[];
/* "host0:/home/xeno/work/iwase/glow3.pxy" */
extern const char glowLandscapeTexturePath[];
/* "host0:/home/xeno/work/iwase/glow3.txy" */
extern const char glowLandscapeModelAssertText[];
/* "pLandscape->rsrc[XAct_Rsrc_MDL]" */
extern const char glowLandscapeTextureAssertText[];
/* "pLandscape->rsrc[XAct_Rsrc_TEX]" */
extern const char glowChaosAssertText[];
/* "pChaos" */
extern const char glowChaosModelPath[];
/* "host0:/home/xeno/work/iwase/ziggy00.pxy" */
extern const char glowChaosTexturePath[];
/* "host0:/home/xeno/work/iwase/ziggy00.txy" */
extern const char glowChaosHierarchyPath[];
/* "host0:/home/xeno/work/iwase/ziggy00.xhr" */
extern const char glowChaosMotionPath[];
/* "host0:/home/xeno/work/iwase/ziggy00.xap" */
extern const char glowChaosModelAssertText[];
/* "pChaos->rsrc[XAct_Rsrc_MDL]" */
extern const char glowChaosTextureAssertText[];
/* "pChaos->rsrc[XAct_Rsrc_TEX]" */
extern const char glowCitizenTexturePath[];

/* Loads the landscape and the chaos models, then runs the scene until SELECT and START are pressed. */
void glowRunScene(void)
{
    struct XenoActor *pLandscape;
    int cdState;
    int bufAddr;
    int loadAddr;
    struct XglCamera *camera;
    struct XenoActor *pChaos;

    loadAddr = 0x1000000;
    tiWaterInitStudio();
    XenoActorPoolInitDefault();
    pLandscape = XenoActorAlloc();
    if (pLandscape == 0) {
        assertFailed(glowLandscapeAssertText, glowSourceName, 72);
    }
    pLandscape->rsrc[XAct_Rsrc_MDL] = testLoadFile(&loadAddr, glowLandscapeModelPath);
    pLandscape->rsrc[XAct_Rsrc_TEX] = testLoadFile(&loadAddr, glowLandscapeTexturePath);
    if (pLandscape->rsrc[XAct_Rsrc_MDL] == 0) {
        assertFailed(glowLandscapeModelAssertText, glowSourceName, 75);
    }
    if (pLandscape->rsrc[XAct_Rsrc_TEX] == 0) {
        assertFailed(glowLandscapeTextureAssertText, glowSourceName, 76);
    }

    pChaos = XenoActorAlloc();
    if (pChaos == 0) {
        assertFailed(glowChaosAssertText, glowSourceName, 79);
    }
    pChaos->rsrc[XAct_Rsrc_MDL] = testLoadFile(&loadAddr, glowChaosModelPath);
    pChaos->rsrc[XAct_Rsrc_TEX] = testLoadFile(&loadAddr, glowChaosTexturePath);
    pChaos->rsrc[XAct_Rsrc_XHR] = testLoadFile(&loadAddr, glowChaosHierarchyPath);
    pChaos->rsrc[XAct_Rsrc_XAP] = testLoadFile(&loadAddr, glowChaosMotionPath);
    if (pChaos->rsrc[XAct_Rsrc_MDL] == 0) {
        assertFailed(glowChaosModelAssertText, glowSourceName, 84);
    }
    if (pChaos->rsrc[XAct_Rsrc_TEX] == 0) {
        assertFailed(glowChaosTextureAssertText, glowSourceName, 85);
    }
    cdState = -1;
    pChaos->position.x = 2.0f;
    XenoActorAllocMatrices(pChaos);
    XenoActorSetChannelValue(pChaos, 8.0f);
    XenoActorSetMode(pChaos, 0);
    bufAddr = loadAddr;
    xglCdReadFile(glowCitizenTexturePath, (void *) bufAddr, 1, CD_NOTIFY_DUMMY);

    while ((PadData[0].heldAndPressed & (PAD_SELECT | (PAD_START << 16))) != (PAD_SELECT | (PAD_START << 16))) {
        xglRenderClearFrame();
        if (PadData[0].pressed & PAD_L2) {
            xglCdReadFile(glowCitizenTexturePath, (void *) bufAddr, 1, CD_NOTIFY_DUMMY);
            cdState = 0;
        }
        if (cdState >= 0) {
            cdState = xglCdServiceIfPending() ? cdState + 1 : -1;
        }
        glowUpdateFromPad(pChaos);
        xglStudioGetCamera(&camera, 0);
        pxyRequestOverlayPass();
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
    }
    XenoActorReleaseAll();
    xglRenderClearFrame();
    gifStateModeHook = 0;
}

const char glowLandscapeAssertText[16] = "pLandscape";
const char glowSourceName[16] = "../glow.euc.c";
const char glowLandscapeModelPath[40] = "host0:/home/xeno/work/iwase/glow3.pxy";
const char glowLandscapeTexturePath[40] = "host0:/home/xeno/work/iwase/glow3.txy";
const char glowLandscapeModelAssertText[32] = "pLandscape->rsrc[XAct_Rsrc_MDL]";
const char glowLandscapeTextureAssertText[32] = "pLandscape->rsrc[XAct_Rsrc_TEX]";
const char glowChaosAssertText[8] = "pChaos";
const char glowChaosModelPath[40] = "host0:/home/xeno/work/iwase/ziggy00.pxy";
const char glowChaosTexturePath[40] = "host0:/home/xeno/work/iwase/ziggy00.txy";
const char glowChaosHierarchyPath[40] = "host0:/home/xeno/work/iwase/ziggy00.xhr";
const char glowChaosMotionPath[40] = "host0:/home/xeno/work/iwase/ziggy00.xap";
const char glowChaosModelAssertText[32] = "pChaos->rsrc[XAct_Rsrc_MDL]";
const char glowChaosTextureAssertText[32] = "pChaos->rsrc[XAct_Rsrc_TEX]";
const char glowCitizenTexturePath[32] = "iwase\\citizen.xtx";
