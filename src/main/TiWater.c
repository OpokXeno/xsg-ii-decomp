/*
 * TiWater: the water scene (file ../TiWater.euc.c in the assert strings): the scene loop, the
 * debug parameter panel, the object registry (tiWaterSubCount count, tiWaterSubList list of TiWaterSub
 * tables) and the drawing. Names are proposals (see the proposal file).
 */
#include "common.h"
#include "main/TiWater.h"
#include "gs.h"
#include "main/xgl_thread.h"
#include "main/gifState.h"
#include "main/nml_model_set.h"
#include "main/pad.h"
#include "main/xgl_camera.h"
#include "main/test_129.h"
#include "main/assert_fail.h"
#include "main/pxy_packet_state.h"
#include "main/game_camera.h"
#include "main/gr_screen.h"
#include "main/xenoActor.h"
#include "main/xgl_font.h"
#include "main/xgl_studio.h"
#include "main/xgl_render.h"
#include "main/xgl_cur_mul.h"
#include "main/xgl_cur_matrix.h"
#include "main/TiWatSim.h"
#include "main/xgl_packet.h"
#include "main/xgl_packet_unpack.h"

/* debug panel mode, toggled by select in the scene loop */
static int tiWaterDebugIndex;
/* selected debug parameter, 0..11 */

/* Resets the debug panel mode and its selected parameter to zero. */
void tiWaterDebugReset(void) {
    tiWaterDebugMode = 0;
    tiWaterDebugIndex = 0;
}

extern const char tiWaterMenuFormat[];
extern const char tiWaterMenuMarker[];

/* Number of debug parameters the panel cycles through. */
#define TI_WATER_DEBUG_PARAMS 12
/* Parameters shown at once, and the line height. */
#define TI_WATER_DEBUG_LINES 3
#define TI_WATER_DEBUG_LINE_HEIGHT 9

/* Draws the debug panel (the selected parameter and the next two, the selected one marked) and
 * changes the selected value by its step: triangle adds, cross subtracts, held while select is
 * held and repeated otherwise. */
void tiWaterDebugDrawParams(void)
{
    unsigned int i;
    unsigned int line;
    unsigned short buttons;

    line = 0;
    xglFontDebugPrintf(0, 0, tiWaterMenuFormat, tiWaterDebugIndex + 1, TI_WATER_DEBUG_PARAMS);
    i = tiWaterDebugIndex;
    for (; line < TI_WATER_DEBUG_LINES && i < TI_WATER_DEBUG_PARAMS; i++, line++) {
        if (tiWaterDebugIndex == i) {
            xglFontDebugPrintf(0, (line + 1) * TI_WATER_DEBUG_LINE_HEIGHT, tiWaterMenuMarker);
        }
        xglFontDebugPrintf(10, (line + 1) * TI_WATER_DEBUG_LINE_HEIGHT, tiWaterDebugLabels[i], *tiWaterDebugValues[i]);
    }
    buttons = PadData[0].held;
    if (!(buttons & PAD_SELECT)) {
        buttons = PadData[0].repeated;
    }
    if (buttons & PAD_TRIANGLE) {
        *tiWaterDebugValues[tiWaterDebugIndex] += tiWaterDebugSteps[tiWaterDebugIndex];
    }
    if (buttons & PAD_CROSS) {
        *tiWaterDebugValues[tiWaterDebugIndex] -= tiWaterDebugSteps[tiWaterDebugIndex];
    }
}


/* Debug panel step: up and down on pad 0 select the parameter (wrapping), the panel is drawn and
 * every parameter is applied to the GIF state and the four model sub windows. */
void tiWaterDebugUpdate(void)
{
    if (PadData[0].pressed & PAD_UP) {
        if (--tiWaterDebugIndex < 0) {
            tiWaterDebugIndex = TI_WATER_DEBUG_PARAMS - 1;
        }
    }
    if (PadData[0].pressed & PAD_DOWN) {
        if (++tiWaterDebugIndex >= TI_WATER_DEBUG_PARAMS) {
            tiWaterDebugIndex = 0;
        }
    }
    tiWaterDebugDrawParams();
    gifStateSetCenter(tiWaterCx, tiWaterCy);
    gifStateSetEffectAlpha(tiWaterAlphaType, tiWaterAlpha);
    gifStateSetDecrease(tiWaterDecrease);
    gifStateSetFbp(tiWaterFbp);
    gifStateSetBlur(tiWaterBlur);
    nmlModelUseSubWindow(0, tiWaterIntVals[0]);
    nmlModelUseSubWindow(1, tiWaterIntVals[1]);
    nmlModelUseSubWindow(2, tiWaterIntVals[2]);
    nmlModelUseSubWindow(3, tiWaterIntVals[3]);
}


/* The "WAT " tag at the start of a water table. */
#define TI_WATER_SUB_MAGIC 0x20544157
/* Most water tables tiWaterSubList holds. */
#define TI_WATER_MAX_SUBS 64
/* Item flag: the item is set up (its block, height buffers, transforms and random heights). */
#define TI_WATER_ITEM_SET_UP 0x100
/* Range of the random heights given to a new item. */
#define TI_WATER_RANDOM_RANGE 30

/* Registers a water table in tiWaterSubList and sets each of its items up once: the block and the height
 * buffers (offsets from the table), the translate, rotate and scale transform, its inverse and random heights.
 * Returns 0 when the list is full or the table is not a water table, 1 otherwise. */
int tiWaterRegisterSub(TiWaterSub *sub)
{
    unsigned int i;
    int index;
    unsigned int flags;
    int range;
    TiWatSimItem *item;

    if (tiWaterSubCount >= TI_WATER_MAX_SUBS) {
        return 0;
    }
    if (sub->magic != TI_WATER_SUB_MAGIC) {
        return 0;
    }
    if (!sub->active) {
        return 1;
    }
    range = TI_WATER_RANDOM_RANGE;
    tiWaterSubList[tiWaterSubCount++] = sub;
    item = sub->items;
    for (i = 0; i < sub->active; i += 1, item++) {
        if ((item->flags & TI_WATER_ITEM_SET_UP) == 0) {
            item->height_buffer_a = (float *) ((char *) sub + item->height_offset_a);
            item->height_buffer_b = (float *) ((char *) sub + item->height_offset_b);
            item->block = (u8 *) sub;
            xglCurSetTranslate(&item->translate);
            xglCurMulRotateY(item->rotateY);
            xglCurMulScaleXYZ(&item->scale.x);
            xglCurMatrixGet(item->transform.rows);
            xglMatrixInverse(&item->matrix, &item->transform);
            tiWatSimRandomizeItem(item, TI_WATER_RANDOM_RANGE);
            item->flags |= TI_WATER_ITEM_SET_UP;
        }
    }
    return 1;
}

/* The water state template sent by tiWaterSendStateTemplate, and its length for sceVif1PkAddDataN. */
extern u64 tiWaterStateTemplate[];

#define TI_WATER_STATE_TEMPLATE_COUNT 0x3C
/* The two words tiWaterSendStateTemplate patches: the data of the FRAME_1 and TEX0_1 A+D pairs. */
#define TI_WATER_TEMPLATE_FRAME_1 (0x60 / 8)
#define TI_WATER_TEMPLATE_TEX0_1 (0x70 / 8)

/* Sends the water state template: FRAME_1 at the frame buffer and TEX0_1 on the display buffer
 * (512 x 512, with alpha) are patched in, then the template goes out as direct data. */
void tiWaterSendStateTemplate(void)
{
    XglPacket *packet = xglPacketGetCurrent();

    tiWaterStateTemplate[TI_WATER_TEMPLATE_FRAME_1] = GS_SET_FRAME(sRender.vram.flip_base, 8, 0, 0);
    tiWaterStateTemplate[TI_WATER_TEMPLATE_TEX0_1]
        = GS_SET_TEX0(sRender.display_buffer_base << 5, 8, 0, 9, 9, 1, 0, 0, 0, 0, 0, 0);
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDataN(packet, tiWaterStateTemplate, TI_WATER_STATE_TEMPLATE_COUNT);
    sceVif1PkCloseDirectHLCode(packet);
}

/* The definition (xgl_packet_unpack.c) takes the item as its own view of the grid fields (WaterStripGrid, block as a u32):
 * typing it TiWatSimItem (block as a pointer) reschedules tiWaterSendStrip, so the prototype stays here. */
void tiWaterSendGrid(TiWatSimItem *item, void *program);
/* VU1 microprogram entry points of the water drawing (VU1 micro memory addresses, linked as absolute symbols
 * like sv_get_image_item.c's D_50 / D_80) and the grid tables sent with them. */
extern unsigned char D_688[];
/* the surface pass */
extern unsigned char D_7F0[];
/* the depth pass while tiWaterDepthProgramSelect is set */
extern unsigned char D_9F0[];
/* the depth pass otherwise */
extern int tiWaterDepthProgramSelect;
extern unsigned char tiWaterSurfaceGridHeader[];
extern unsigned char tiWaterDepthGridHeader[];

/* VIF MSCAL: start the VU1 microprogram at the given address (in 8-byte units). */
#define TI_WATER_VIF_MSCAL(address) (0x14000000 | ((unsigned int) (address) >> 3))
/* Item flags: the two draw passes. */
#define TI_WATER_ITEM_DRAW_SURFACE 0x4
#define TI_WATER_ITEM_DRAW_DEPTH 0x2

/* Draws one water item: sends its frame, values and direction, then for each enabled pass the GS state
 * (clamp, frame on the display page, blend, the frame page as texture, Z test) and the VU1 program with
 * the item's grid. */
void tiWaterDrawItem(TiWatSimItem *item)
{
    XglPacket *packet = xglPacketGetCurrent();
    XglCamera *camera = xglStudioSelectGetActiveCamera(0);

    tiWaterSendFrame(camera, item->transform.rows);
    tiWaterSendItemPair(item->drawPair[0], item->drawPair[1]);
    tiWaterSendDirection(camera, item->transform.rows);
    if (item->flags & TI_WATER_ITEM_DRAW_SURFACE) {
        sceVif1PkCnt(packet, 0);
        sceVif1PkOpenDirectHLCode(packet, 0);
        gifStateAddReg(GS_CLAMP_1, GS_SET_CLAMP(GS_REGION_CLAMP, GS_REGION_CLAMP, 256, 510, 0, 222));
        gifStateAddReg(GS_FBA_1, 1);
        gifStateAddReg(GS_FRAME_1, GS_SET_FRAME(sRender.display_buffer_base, 8, 0, 0));
        gifStateAddReg(GS_ALPHA_1, GS_SET_ALPHA(0, 1, 2, 1, 0x4F));
        gifStateAddReg(GS_TEX0_1, GS_SET_TEX0(sRender.vram.flip_base << 5, 8, 0, 9, 9, 0, 0, 0, 0, 0, 0, 0));
        gifStateAddReg(GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
        gifStateAddReg(GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
        gifStateAddReg(GS_TEXFLUSH, 0);
        gifStateAddReg(GS_ZBUF_1, GS_SET_ZBUF(0, 0x31, 0));
        gifStateFlush();
        sceVif1PkCloseDirectHLCode(packet);
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddCode(packet, TI_WATER_VIF_MSCAL(D_688));
        tiWaterSendGrid(item, tiWaterSurfaceGridHeader);
    }
    if (item->flags & TI_WATER_ITEM_DRAW_DEPTH) {
        sceVif1PkCnt(packet, 0);
        sceVif1PkOpenDirectHLCode(packet, 0);
        gifStateAddReg(GS_CLAMP_1, GS_SET_CLAMP(GS_REGION_CLAMP, GS_REGION_CLAMP, 0, 254, 0, 222));
        gifStateAddReg(GS_FBA_1, 1);
        gifStateAddReg(GS_FRAME_1, GS_SET_FRAME(sRender.display_buffer_base, 8, 0, 0));
        gifStateAddReg(GS_ALPHA_1, GS_SET_ALPHA(0, 2, 2, 1, 0x40));
        gifStateAddReg(GS_TEX0_1, GS_SET_TEX0(sRender.vram.flip_base << 5, 8, 0, 8, 8, 1, 0, 0, 0, 0, 0, 0));
        gifStateAddReg(GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
        gifStateAddReg(GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2));
        gifStateAddReg(GS_TEXFLUSH, 0);
        gifStateAddReg(GS_ZBUF_1, GS_SET_ZBUF(0, 0x31, 0));
        gifStateFlush();
        sceVif1PkCloseDirectHLCode(packet);
        sceVif1PkCnt(packet, 0);
        if (tiWaterDepthProgramSelect != 0) {
            sceVif1PkAddCode(packet, TI_WATER_VIF_MSCAL(D_7F0));
        } else {
            sceVif1PkAddCode(packet, TI_WATER_VIF_MSCAL(D_9F0));
        }
        tiWaterSendGrid(item, tiWaterDepthGridHeader);
    }
}


/* Draw callback: in pass 0, draws every registered table (CLAMP_1 and the state template first when
 * it has items, then each item) and empties the list; the packet state is reset afterwards. */
void tiWaterDrawAll(int pass)
{
    XglPacket *packet = xglPacketGetCurrent();
    unsigned int i;
    unsigned int j;
    TiWatSimItem *item;

    if (pass != 0) {
        return;
    }
    for (i = 0; i < tiWaterSubCount; i++) {
        item = tiWaterSubList[i]->items;
        gifStateReset();
        if (tiWaterSubList[i]->active != 0) {
            sceVif1PkCnt(packet, 0);
            sceVif1PkOpenDirectHLCode(packet, 0);
            gifStateAddReg(GS_CLAMP_1, GS_SET_CLAMP(0, 0, 0xFF, 0xFF, 0, 0));
            gifStateFlush();
            sceVif1PkCloseDirectHLCode(packet);
            tiWaterSendStateTemplate();
        }
        for (j = 0; j < tiWaterSubList[i]->active; j++) {
            tiWaterDrawItem(item);
            item++;
        }
    }
    tiWaterSubCount = 0;
    xglPacketModeReset();
    xglPacketInitVuStateArg(pass);
    nmlPacketClrTextureCache();
    nmlPacketClrModelCache();
    nmlPacketClrMatrixCache();
}

float atanf(float value);

/* The angle of (x, y) in [0, 2 pi): atan(y / x) moved into the right half turn. */
float tiWaterVectorAngle(float x, float y)
{
    float angle;

    if (x == 0.0f) {
        x = 1e-05f;
    }
    angle = atanf(y / x);
    if (x > 0.0f) {
        if (angle < 0.0f) {
            angle += tiWaterTwoPi;
        }
    } else {
        angle += 3.1415927f;
    }
    return angle;
}

/* Source steps per unit of pad direction. */
#define TI_WATER_SOURCE_STEP 0.001f

/* Moves the water source actor by the pad direction of port 0 (cross enables its channel). While a direction
 * is held, it turns the actor towards it and disturbs every registered water table at the actor's position.
 * Then stores the source position for the simulation. */
void tiWaterUpdateSource(struct XenoActor *actor)
{
    int i;

    actor->previousPosition.x = actor->position.x;
    actor->previousPosition.z = actor->position.z;
    actor->position.x += PadData[0].leftX * TI_WATER_SOURCE_STEP;
    actor->position.z += PadData[0].leftY * TI_WATER_SOURCE_STEP;
    if (PadData[0].held & PAD_CROSS) {
        XenoActorEnableChannel(actor);
    }
    if (PadData[0].leftX != 0 || PadData[0].leftY != 0) {
        actor->rotation.y = tiWaterVectorAngle(PadData[0].leftY, PadData[0].leftX);
        for (i = 0; i < tiWaterSubCount; i++) {
            tiWatSimDisturbAll(tiWaterSubList[i], &actor->position);
        }
        /* The original computes the length of the direction here and drops it. */
        (void) __builtin_sqrtf(PadData[0].leftY * PadData[0].leftY
                               + PadData[0].leftX * PadData[0].leftX);
    }
    tiWatSimStoreSourcePos(&actor->position);
}

/* Steps the rain and the simulation of every item of the sub-object. */
void tiWaterStepSub(TiWaterSub *sub)
{
    TiWatSimItem *item = sub->items;
    unsigned int i;

    for (i = 0; i < sub->active; i++) {
        tiWatSimStepRain(item);
        tiWatSimUpdate(1.0f, item);
        item++;
    }
}

void tiWaterSetEffectMode(void) {
    gifStateClearDepthAndFlip();
    gifStateSetDepth(0xFFFFFF);
    gifStateSetMode((int) tiWaterEffectNum);
}

extern const char tiWaterLandscapeName[];
extern const char tiWaterSourceFile[];
extern const char tiWaterLandscapeModelPath[];
extern const char tiWaterLandscapeTexturePath[];
extern const char tiWaterLandscapeModelAssert[];
extern const char tiWaterLandscapeTextureAssert[];
extern const char tiWaterKosmosName[];
extern const char tiWaterKosmosModelPath[];
extern const char tiWaterKosmosTexturePath[];
extern const char tiWaterKosmosHierarchyPath[];
extern const char tiWaterKosmosMotionPath[];
extern const char tiWaterKosmosMotionExt2Path[];
extern const char tiWaterKosmosModelAssert[];
extern const char tiWaterKosmosTextureAssert[];
extern const char tiWaterKosmosHierarchyAssert[];
extern const char tiWaterKosmosMotionAssert[];
extern const char tiWaterKosmosMotionExt2Assert[];
extern const DoublewordAlignedVector4 tiWaterMulColor;

/* The test heap the scene files are loaded into. */
#define TI_WATER_HEAP 0x1000000
/* XenoActor drawFlags bit set on the source actor. */
#define TI_WATER_SOURCE_DRAW_FLAG 0x800000
/* Select held and start pressed end the scene. */
#define TI_WATER_EXIT (PAD_SELECT | (PAD_START << 16))
/* The landscape frame counts up to this and starts again. */
#define TI_WATER_FRAMES 120.0f

/* The water test scene: loads the landscape and the KOS-MOS source actor from the host, sets the GIF effect
 * up from the debug parameters and runs the frame loop (L2 toggles the debug panel, circle applies the effect
 * mode) until select is held with start pressed. */
void tiWaterSceneLoop(void)
{
    DoublewordAlignedVector4 color;
    int heap;
    XglCamera *camera;
    struct XenoActor *landscape;
    struct XenoActor *kosmos;

    heap = TI_WATER_HEAP;
    tiWaterInitStudio();
    XenoActorPoolInitDefault();
    landscape = XenoActorAlloc();
    if (landscape == 0) {
        assertFailed(tiWaterLandscapeName, tiWaterSourceFile, 578);
    }
    landscape->rsrc[XAct_Rsrc_MDL] = testLoadFile(&heap, tiWaterLandscapeModelPath);
    landscape->rsrc[XAct_Rsrc_TEX] = testLoadFile(&heap, tiWaterLandscapeTexturePath);
    if (landscape->rsrc[XAct_Rsrc_MDL] == 0) {
        assertFailed(tiWaterLandscapeModelAssert, tiWaterSourceFile, 581);
    }
    if (landscape->rsrc[XAct_Rsrc_TEX] == 0) {
        assertFailed(tiWaterLandscapeTextureAssert, tiWaterSourceFile, 582);
    }
    kosmos = XenoActorAlloc();
    if (kosmos == 0) {
        assertFailed(tiWaterKosmosName, tiWaterSourceFile, 585);
    }
    kosmos->rsrc[XAct_Rsrc_MDL] = testLoadFile(&heap, tiWaterKosmosModelPath);
    kosmos->rsrc[XAct_Rsrc_TEX] = testLoadFile(&heap, tiWaterKosmosTexturePath);
    kosmos->rsrc[XAct_Rsrc_XHR] = testLoadFile(&heap, tiWaterKosmosHierarchyPath);
    kosmos->rsrc[XAct_Rsrc_XAP] = testLoadFile(&heap, tiWaterKosmosMotionPath);
    kosmos->rsrc[XAct_Rsrc_XAP_Ext2] = testLoadFile(&heap, tiWaterKosmosMotionExt2Path);
    if (kosmos->rsrc[XAct_Rsrc_MDL] == 0) {
        assertFailed(tiWaterKosmosModelAssert, tiWaterSourceFile, 596);
    }
    if (kosmos->rsrc[XAct_Rsrc_TEX] == 0) {
        assertFailed(tiWaterKosmosTextureAssert, tiWaterSourceFile, 597);
    }
    if (kosmos->rsrc[XAct_Rsrc_XHR] == 0) {
        assertFailed(tiWaterKosmosHierarchyAssert, tiWaterSourceFile, 598);
    }
    if (kosmos->rsrc[XAct_Rsrc_XAP] == 0) {
        assertFailed(tiWaterKosmosMotionAssert, tiWaterSourceFile, 599);
    }
    if (kosmos->rsrc[XAct_Rsrc_XAP_Ext2] == 0) {
        assertFailed(tiWaterKosmosMotionExt2Assert, tiWaterSourceFile, 600);
    }
    kosmos->drawFlags |= TI_WATER_SOURCE_DRAW_FLAG;
    XenoActorAllocMatrices(kosmos);
    XenoActorSetChannelValue(kosmos, 8.0f);
    XenoActorSetMode(kosmos, 0);
    tiWaterDebugReset();
    gifStateSetCenter(tiWaterCx, tiWaterCy);
    gifStateSetEffectAlpha(tiWaterAlphaType, tiWaterAlpha);
    gifStateSetDecrease(tiWaterDecrease);
    gifStateSetFbp(tiWaterFbp);
    gifStateSetBlur(tiWaterBlur);
    color = tiWaterMulColor;
    nmlModelSetMulColor(&color);
    while ((PadData[0].heldAndPressed & TI_WATER_EXIT) != TI_WATER_EXIT) {
        xglRenderClearFrame();
        pxyRequestOverlayPass();
        if ((unsigned short) (PadData[0].pressed & PAD_L2)) {
            if (++tiWaterDebugMode >= 2) {
                tiWaterDebugMode = 0;
            }
        }
        if (tiWaterDebugMode == 1) {
            tiWaterDebugUpdate();
        }
        if (PadData[0].pressed & PAD_CIRCLE) {
            tiWaterSetEffectMode();
        }
        landscape->frame += 1.0f;
        if (TI_WATER_FRAMES <= landscape->frame) {
            landscape->frame = 0.0f;
        }
        xglStudioGetCamera(&camera, 0);
        XenoActorDoit();
        XenoActorDisp();
        tiWaterUpdateSource(kosmos);
        xglSleep();
    }
    XenoActorReleaseAll();
    xglRenderClearFrame();
    gifStateModeHook = 0;
}



/* Per-object update: the word at +0xC0 of the pxy header (waterFlag) is the offset of its water table; registers the
 * table when it has items, steps it when stepMode is 0, and installs the draw callback tiWaterDrawAll. */
void tiWaterObjectUpdate(PxyHeader *header, int stepMode) {
    TiWaterSub *sub;

    sub = (TiWaterSub *) ((char *) header + header->waterFlag);
    if (sub->active != 0) {
        tiWaterRegisterSub(sub);
        if (stepMode == 0) {
            tiWaterStepSub(sub);
        }
        pxyRequestTextureCopyPass();
        pxySetDrawCallback(tiWaterDrawAll);
    }
}

/* Starts the studio with its main camera, clears the camera's rotation and position, marks it active in
 * state 2 and moves the studio. */
void tiWaterInitStudio(void)
{
    XglCamera *camera;
    XglCamera *current;

    xglStudioInit();
    xglStudioChange(0);
    xglStudioMainCameraInit();
    xglStudioGetCamera(&camera, 0);
    camera->travel.rotation.x = 0.0f;
    camera->travel.rotation.y = 0.0f;
    camera->travel.rotation.z = 0.0f;
    camera->travel.position.x = 0.0f;
    camera->travel.position.y = 0.0f;
    camera->travel.position.z = 0.0f;
    current = camera;
    current->active = 1;
    current->state = 2;
    xglStudioMove();
}

float cosf(float angle);
float sinf(float angle);

/* The fine camera steps: distance per frame and angle per frame. */
#define TI_WATER_CAMERA_FINE_STEP 0.2f
#define TI_WATER_CAMERA_FINE_TURN 0.02f
/* The coarse camera step (the turn step is the fine one). */
#define TI_WATER_CAMERA_STEP 5.0f

/* Debug camera control on pad 0 (held buttons) like tiWaterCameraPadControlFine, with a step of 5;
 * with start held, square/circle turn the roll instead of the heading. */
void tiWaterCameraPadControl(XglCamera *camera)
{
    float step = TI_WATER_CAMERA_STEP;
    float forward = cosf(camera->travel.rotation.y) * step;
    float side = sinf(camera->travel.rotation.y) * step;

    if (PadData[0].held & PAD_RIGHT) {
        camera->travel.position.x += forward;
        camera->travel.position.z -= side;
    }
    if (PadData[0].held & PAD_LEFT) {
        camera->travel.position.x -= forward;
        camera->travel.position.z += side;
    }
    if (PadData[0].held & PAD_UP) {
        camera->travel.position.x -= side;
        camera->travel.position.z -= forward;
    }
    if (PadData[0].held & PAD_DOWN) {
        camera->travel.position.x += side;
        camera->travel.position.z += forward;
    }
    if (PadData[0].held & PAD_R2) {
        camera->travel.position.y -= step;
    }
    if (PadData[0].held & PAD_R1) {
        camera->travel.position.y += step;
    }
    if (PadData[0].held & PAD_TRIANGLE) {
        camera->travel.rotation.x += TI_WATER_CAMERA_FINE_TURN;
        if (camera->travel.rotation.x > tiWaterTwoPi) {
            camera->travel.rotation.x -= tiWaterTwoPi;
        }
    }
    if (PadData[0].held & PAD_CROSS) {
        camera->travel.rotation.x -= TI_WATER_CAMERA_FINE_TURN;
        if (camera->travel.rotation.x < 0.0f) {
            camera->travel.rotation.x += tiWaterTwoPi;
        }
    }
    if (PadData[0].held & PAD_SQUARE) {
        if (PadData[0].held & PAD_START) {
            camera->travel.rotation.z += TI_WATER_CAMERA_FINE_TURN;
            if (camera->travel.rotation.z > tiWaterTwoPi) {
                camera->travel.rotation.z -= tiWaterTwoPi;
            }
        } else {
            camera->travel.rotation.y += TI_WATER_CAMERA_FINE_TURN;
            if (camera->travel.rotation.y > tiWaterTwoPi) {
                camera->travel.rotation.y -= tiWaterTwoPi;
            }
        }
    }
    if (PadData[0].held & PAD_CIRCLE) {
        if (PadData[0].held & PAD_START) {
            camera->travel.rotation.z -= TI_WATER_CAMERA_FINE_TURN;
            if (camera->travel.rotation.z < 0.0f) {
                camera->travel.rotation.z += tiWaterTwoPi;
            }
        } else {
            camera->travel.rotation.y -= TI_WATER_CAMERA_FINE_TURN;
            if (camera->travel.rotation.y < 0.0f) {
                camera->travel.rotation.y += tiWaterTwoPi;
            }
        }
    }
}

/* Fine debug camera control on pad 0 (held buttons): the d-pad moves in the heading's plane, R2/R1
 * move down/up, triangle/cross change the pitch and square/circle the heading (both kept in
 * [0, 2 pi)). */
void tiWaterCameraPadControlFine(XglCamera *camera)
{
    float step = TI_WATER_CAMERA_FINE_STEP;
    float forward = cosf(camera->travel.rotation.y) * step;
    float side = sinf(camera->travel.rotation.y) * step;

    if (PadData[0].held & PAD_RIGHT) {
        camera->travel.position.x += forward;
        camera->travel.position.z -= side;
    }
    if (PadData[0].held & PAD_LEFT) {
        camera->travel.position.x -= forward;
        camera->travel.position.z += side;
    }
    if (PadData[0].held & PAD_UP) {
        camera->travel.position.x -= side;
        camera->travel.position.z -= forward;
    }
    if (PadData[0].held & PAD_DOWN) {
        camera->travel.position.x += side;
        camera->travel.position.z += forward;
    }
    if (PadData[0].held & PAD_R2) {
        camera->travel.position.y -= step;
    }
    if (PadData[0].held & PAD_R1) {
        camera->travel.position.y += step;
    }
    if (PadData[0].held & PAD_TRIANGLE) {
        camera->travel.rotation.x += TI_WATER_CAMERA_FINE_TURN;
        if (camera->travel.rotation.x > tiWaterTwoPi) {
            camera->travel.rotation.x -= tiWaterTwoPi;
        }
    }
    if (PadData[0].held & PAD_CROSS) {
        camera->travel.rotation.x -= TI_WATER_CAMERA_FINE_TURN;
        if (camera->travel.rotation.x < 0.0f) {
            camera->travel.rotation.x += tiWaterTwoPi;
        }
    }
    if (PadData[0].held & PAD_SQUARE) {
        camera->travel.rotation.y += TI_WATER_CAMERA_FINE_TURN;
        if (camera->travel.rotation.y > tiWaterTwoPi) {
            camera->travel.rotation.y -= tiWaterTwoPi;
        }
    }
    if (PadData[0].held & PAD_CIRCLE) {
        camera->travel.rotation.y -= TI_WATER_CAMERA_FINE_TURN;
        if (camera->travel.rotation.y < 0.0f) {
            camera->travel.rotation.y += tiWaterTwoPi;
        }
    }
}

/* Data of this TU (the scaffold items, in address order). */
const char tiWaterMenuFormat[24] = "\x0d" "\x02" "MENU %2d / %2d";
const char tiWaterMenuMarker[8] = "\x0d" "\x02" "*";
const char tiWaterLandscapeName[16] = "pLandscape";
const char tiWaterSourceFile[24] = "../TiWater.euc.c";
const char tiWaterLandscapeModelPath[48] = "host0:/home/xeno/work/iwase/E2_MOM17.pxy";
const char tiWaterLandscapeTexturePath[48] = "host0:/home/xeno/work/iwase/E2_MOM17.txy";
const char tiWaterLandscapeModelAssert[32] = "pLandscape->rsrc[XAct_Rsrc_MDL]";
const char tiWaterLandscapeTextureAssert[32] = "pLandscape->rsrc[XAct_Rsrc_TEX]";
const char tiWaterKosmosName[8] = "pKosmos";
const char tiWaterKosmosModelPath[40] = "host0:/home/xeno/work/iwase/jr00.pxy";
const char tiWaterKosmosTexturePath[40] = "host0:/home/xeno/work/iwase/jr00.txy";
const char tiWaterKosmosHierarchyPath[40] = "host0:/home/xeno/work/iwase/jr00.xhr";
const char tiWaterKosmosMotionPath[40] = "host0:/home/xeno/work/iwase/jr00.xap";
const char tiWaterKosmosMotionExt2Path[48] = "host0:/home/xeno/work/iwase/x2/ziggy00.xap";
const char tiWaterKosmosModelAssert[32] = "pKosmos->rsrc[XAct_Rsrc_MDL]";
const char tiWaterKosmosTextureAssert[32] = "pKosmos->rsrc[XAct_Rsrc_TEX]";
const char tiWaterKosmosHierarchyAssert[32] = "pKosmos->rsrc[XAct_Rsrc_XHR]";
const char tiWaterKosmosMotionAssert[32] = "pKosmos->rsrc[XAct_Rsrc_XAP]";
const char tiWaterKosmosMotionExt2Assert[48] = "pKosmos->rsrc[XAct_Rsrc_XAP_Ext2]";
const DoublewordAlignedVector4 tiWaterMulColor = { 1.0f, 0.0f, 0.0f, 1.0f };
