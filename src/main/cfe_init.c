#include "common.h"
#include "shared.h"
#include "main/TiGlobalColl.h"
#include "main/cfePlugIn.h"
#include "main/sound_driver.h"
#include "main/res_get_path.h"
#include "main/xgl_camera.h"
#include "main/cfe_node_pool.h"
#include "main/xgl_light.h"
#include "main/xgl_studio.h"
#include "quadword.h"
#include "main/cfeEvaluate.h"
#include "main/cfe_init.h"


void cfeNodePoolInit(void) {
    cfeNodePoolSet(GameResBufferAlloc(0x48000));
    cfeNodePoolClear();
}

void *memset(void *, int, unsigned int);


/* Number of nodes in cfeActors. */
#define CFE_NODE_COUNT 64

extern const AlignedVector4 cfeActorInitialVector;

/* Clears the 64 actor entries of cfeActors and gives each the vector cfeActorInitialVector at +0xD0 and as
 * its position, no parent, both motion speeds 1 and the first word 1. */
void cfeActorTableInit(void)
{
    AlignedVector4 initial = cfeActorInitialVector;
    const AlignedVector4 *source;
    int i;

    memset(cfeActors, 0, CFE_NODE_COUNT * sizeof(CfeActor));
    for (i = 0; i < CFE_NODE_COUNT; i++) {
        source = &initial;
        QuadwordCopy(&cfeActors[i].vectorD0, source);
        QuadwordCopy(&cfeActors[i].lookTarget, source);
        cfeActors[i].parentActId = -1;
        cfeActors[i].speedA = 1.0f;
        cfeActors[i].speedB = 1.0f;
        cfeActors[i].firstWord = 1;
    }
}


/* Studio setup: clears the 0x70-byte block at cfeCamera, starts xglStudio and the main camera,
   then keeps the camera xglStudioGetCamera2 creates there (its state set to 2). */
void cfeStudioInit(void) {
    XglCamera *camera;

    memset(&cfeCamera, 0, 0x70);
    xglStudioInit();
    xglStudioChange(0);
    xglStudioMainCameraInit();
    camera = xglStudioGetCamera2(0);
    cfeCamera.eval.camera = camera;
    camera->state = 2;
    xglCameraSetPadPort(camera, 1);
}


void cfeCommandTableInit(void) {
    int *entry;
    int i;

    cfeCommandCount = 0;
    memset(cfeEffects, 0, 0x2000);
    entry = &cfeEffects[63].owner;
    for (i = 0; i < 64; i++) {
        *entry = -1;
        entry -= 0x20;
    }
}

/* pi / 2: the angle the unused lights 3 to 5 are given. */
#define CFE_LIGHT_HALF_PI 1.5707964f

#include "main/nml_model_set.h"

/* Number of angle vectors turned into directions: directions[0..5] and viewDirection. */
#define CFE_LIGHT_ANGLE_VECTORS 7

/* Sets the light set the CFE preview uses: three directional lights given as angles (turned into
 * directions here), ambient 0.2 and no point lights, then makes it the pxy default light. */
void cfeLightSetInit(XglLightSet *light)
{
    Vector4 *direction;
    int i;

    light->colors[0][0] = light->colors[0][1] = light->colors[0][2] = 0.9f;
    light->colors[1][0] = light->colors[1][1] = light->colors[1][2] = 0.0f;
    light->colors[2][0] = light->colors[2][1] = light->colors[2][2] = 0.0f;
    light->ambientColor.x = light->ambientColor.y = light->ambientColor.z = 0.2f;
    light->ambientColorLower.x = light->ambientColorLower.y = light->ambientColorLower.z = 0.2f;
    light->directions[3][0] = light->directions[4][0] = light->directions[5][0] = CFE_LIGHT_HALF_PI;
    light->viewDirection.x = 0.0f;
    light->directions[0][0] = 0.0f;
    light->directions[0][1] = 0.0f;
    light->directions[0][3] = 1.0f;
    light->directions[1][0] = 0.0f;
    light->directions[1][1] = 0.0f;
    light->directions[1][3] = 1.0f;
    light->directions[2][0] = 0.0f;
    light->directions[2][1] = 0.0f;
    light->directions[2][3] = 1.0f;
    /* directions[0..5] and viewDirection are seven consecutive angle vectors */
    direction = (Vector4 *) light->directions;
    for (i = 0; i < CFE_LIGHT_ANGLE_VECTORS; i++, direction++) {
        xglDirectionFromAngles(direction, direction);
    }
    light->pointPositions[0][0] = light->pointPositions[0][1] = light->pointPositions[0][2]
        = light->pointPositions[0][3] = 0.0f;
    light->pointPositions[1][0] = light->pointPositions[1][1] = light->pointPositions[1][2]
        = light->pointPositions[1][3] = 0.0f;
    light->pointPositions[2][0] = light->pointPositions[2][1] = light->pointPositions[2][2]
        = light->pointPositions[2][3] = 0.0f;
    light->pointColors[0][0] = light->pointColors[0][1] = light->pointColors[0][2]
        = light->pointColors[0][3] = 0.0f;
    light->pointColors[1][0] = light->pointColors[1][1] = light->pointColors[1][2]
        = light->pointColors[1][3] = 0.0f;
    light->pointColors[2][0] = light->pointColors[2][1] = light->pointColors[2][2]
        = light->pointColors[2][3] = 0.0f;
    pxySetDefaultLight(light);
}


/* Sets up the light set cfeLightSet, clears cfeLights and points its rows at the ambient colour
 * and at the direction and colour of lights 0 to 2. */
void cfeLightRowsInit(void)
{
    cfeLightSetInit(&cfeLightSet);
    memset(cfeLights, 0, sizeof(cfeLights));
    cfeLights[0].direction = 0;
    cfeLights[0].color = &cfeLightSet.ambientColor.x;
    cfeLights[1].direction = cfeLightSet.directions[0];
    cfeLights[1].color = cfeLightSet.colors[0];
    cfeLights[2].direction = cfeLightSet.directions[1];
    cfeLights[2].color = cfeLightSet.colors[1];
    cfeLights[3].direction = cfeLightSet.directions[2];
    cfeLights[3].color = cfeLightSet.colors[2];
}


void cfeGroupTableInit(void) {
    memset(cfeMaps, 0, 0x300);
    cfeGroupCount = 0;
}

extern int cfeMessageCount;

void cfeMessageCountReset(void) {
    cfeMessageCount = 0;
}


/* Clears the global block and sets its two default vectors (255, 255, 255, 1) and (5, 20, 0, 1). */
void cfeEvalGlobalInit(void)
{
    memset(&cfeFog, 0, sizeof(cfeFog));
    cfeFog.fogEnabled = 0;
    cfeFog.evaluated[0].x = 255.0f;
    cfeFog.evaluated[0].y = 255.0f;
    cfeFog.evaluated[0].z = 255.0f;
    cfeFog.evaluated[0].w = 1.0f;
    cfeFog.evaluated[1].x = 5.0f;
    cfeFog.evaluated[1].y = 20.0f;
    cfeFog.evaluated[1].z = 0.0f;
    cfeFog.evaluated[1].w = 1.0f;
}


/* The four corners every entry starts with (a unit quad around the origin). */
typedef struct CfeEntryCorners {
    AlignedVector4 corner[4];
} CfeEntryCorners;

extern const CfeEntryCorners cfeUnitQuad;

/* Number of entries in cfeMonitors. */
#define CFE_EVAL_ENTRY_COUNT 16
/* Clears the entries and starts each one with the unit quad of cfeUnitQuad, the position and angles
 * at the origin (w = 1) and unit scale. */
void cfeEvalEntriesInit(void)
{
    CfeEntryCorners corners = cfeUnitQuad;
    AlignedVector4 *source;
    int i;

    cfeEvalEntryCount = 0;
    memset(cfeMonitors, 0, CFE_EVAL_ENTRY_COUNT * sizeof(CfeMonitor));
    for (i = 0; i < CFE_EVAL_ENTRY_COUNT; i++) {
        source = corners.corner;
        QuadwordCopy(&cfeMonitors[i].corners[0], &source[0]);
        QuadwordCopy(&cfeMonitors[i].corners[1], &source[1]);
        QuadwordCopy(&cfeMonitors[i].corners[2], &source[2]);
        QuadwordCopy(&cfeMonitors[i].corners[3], &source[3]);
        cfeMonitors[i].evaluated[0].component[0] = cfeMonitors[i].evaluated[0].component[1]
            = cfeMonitors[i].evaluated[0].component[2] = 0.0f;
        cfeMonitors[i].evaluated[0].component[3] = 1.0f;
        cfeMonitors[i].evaluated[1].component[0] = cfeMonitors[i].evaluated[1].component[1]
            = cfeMonitors[i].evaluated[1].component[2] = 0.0f;
        cfeMonitors[i].evaluated[1].component[3] = 1.0f;
        cfeMonitors[i].evaluated[2].component[0] = cfeMonitors[i].evaluated[2].component[1]
            = cfeMonitors[i].evaluated[2].component[2] = 1.0f;
        cfeMonitors[i].evaluated[2].component[3] = 1.0f;
    }
}

/* Resets the three CfeScreenEffect entries of cfeScreenEffects (cfeEvaluate.h): result 0, sample 0x80, sample_float 1.0. */

void cfeEvalSlotsInit(void) {
    CfeScreenEffect *slot;
    int i;

    do { memset(cfeScreenEffects, 0, 0x48); slot = cfeScreenEffects; } while (0);
    for (i = 0; i < 3; i++) {
        slot->effectKind = 0;
        slot->sample_int = 0x80;
        slot->sample_float = 1.0f;
        slot++;
    }
}


/* Empties the sound record table: count cfeSndRecordCount = 0 and the first 0x50-byte sound record of cfeSndRecords
   (cfePlugIn appends records of that size and bumps the count). */
void cfeSndRecordsReset(void) {
    int *field;

    cfeSndRecordCount = 0;
    memset(cfeSndRecords, 0, sizeof(CfeSndRecord));
    field = &cfeSndRecords[0].duration;
    field[-1] = 0x7F;
    field[0] = 0;
}

/* Module init: calls two helpers, then each table initializer of this file in order. */
void cfeInit(void) {
    soundDriverReset();
    tiGlobalCollSetHandle(0);
    cfeNodePoolInit();
    cfeGroupTableInit();
    cfeActorTableInit();
    cfeStudioInit();
    cfeCommandTableInit();
    cfeLightRowsInit();
    cfeMessageCountReset();
    cfeEvalGlobalInit();
    cfeEvalEntriesInit();
    cfeEvalSlotsInit();
    cfeSndRecordsReset();
}

int sceLseek(int, int, int);
int sceOpen(const char *, int, ...);

/* Opens `path` for writing, creating it (O_WRONLY | O_CREAT = 0x202, mode 0666 = 0x1B6). */
int cfeFileOpenWrite(const char *path) {
    int fd;

    fd = sceOpen(path, 0x202, 0x1B6);
    if (fd >= 0) {
        sceLseek(fd, 0, 0);
    }
    return fd;
}

void cfeFileOpenRead(const char *path) {
    sceOpen(path, 1);
}

int sceWrite(int, const void *, int);
int sceRead(int, void *, int);

/* Thin wrappers over the SDK file calls. */
int cfeFileWrite(int fd, const void *buf, int size) {
    return sceWrite(fd, buf, size);
}

int cfeFileRead(int fd, void *buf, int size) {
    return sceRead(fd, buf, size);
}

/* The initial actor vector (0, 0, 1, 1) that cfeActorTableInit copies into every entry. */
const AlignedVector4 cfeActorInitialVector = { 0.0f, 0.0f, 1.0f, 1.0f };

/* The unit quad: four corners (-0.5, 0.5), (0.5, 0.5), (-0.5, -0.5), (0.5, -0.5), each with z 0 and w 1. */
const CfeEntryCorners cfeUnitQuad = {
    {
        { -0.5f, 0.5f, 0.0f, 1.0f },
        { 0.5f, 0.5f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.0f, 1.0f },
        { 0.5f, -0.5f, 0.0f, 1.0f },
    },
};

/* The tab string "\t" (9, 0) that cfeFileControl.c writes as the log indent (cfeFileLogIndentStr). */
const int cfeFileLogIndentTab[2] = { 9, 0 };
