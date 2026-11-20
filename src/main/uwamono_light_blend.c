#include "common.h"
#include "shared.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/undulate.h"
#include "main/xgl_font.h"
#include "main/uwamono_light_blend.h"
#include "main/get.h"

/* A midpoint track whose cursor is UWAMONO_TRACK_IDLE is not running; a started track takes UWAMONO_BLEND_STEPS steps. */
#define UWAMONO_TRACK_IDLE (-1)
#define UWAMONO_BLEND_STEPS 5
/* Probe attribute: both bits set means the low byte holds the area index. */
#define UWAMONO_PROBE_AREA_FLAGS 0x500
#define UWAMONO_PROBE_AREA_MASK 0xff
extern const char uwamonoAttributeFormat[];
extern const char uwamonoFrameCountFormat[];

/*
 * Per uwamonoRecords[index] (called from DispMapObject): three Get_MiddlePoint pairs write midpoints to the record's
 * object (the pointer at +0x330) at +0x190 and +0x200; short counters at +0x222..+0x244 pace them, and the float at
 * +0xCC is blended over a frame count.
 */
void uwamonoLightBlendStep(int index)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    struct XenoActor *object = record->body.actor;
    Vector4Elements out;   /* alias set 0: the midpoint loads stay behind the cursor store */
    int i;

    for (i = 0; i < 3; i++) {
        if (record->cursorA[i] != UWAMONO_TRACK_IDLE) {
            unsigned short cursor;
            Get_MiddlePoint(&record->startA[i].vector, &record->followA[i].vector, record->cursorA[i], record->countA[i], &out.vector);
            cursor = record->cursorA[i];
            record->cursorA[i] = (short)(cursor + 1);
            object->light.directions[i][0] = out.component[0];
            object->light.directions[i][1] = out.component[1];
            object->light.directions[i][2] = out.component[2];
            object->light.directions[i][3] = out.component[3];
            if ((short)cursor >= record->countA[i]) {
                record->cursorA[i] = UWAMONO_TRACK_IDLE;
                record->countA[i] = 0;
            }
        }
        if (record->cursorB[i] != UWAMONO_TRACK_IDLE) {
            unsigned short cursor;
            Get_MiddlePoint(&record->startB[i].vector, &record->followB[i].vector, record->cursorB[i], record->countB[i], &out.vector);
            cursor = record->cursorB[i];
            record->cursorB[i] = (short)(cursor + 1);
            object->light.colors[i][0] = out.component[0];
            object->light.colors[i][1] = out.component[1];
            object->light.colors[i][2] = out.component[2];
            object->light.colors[i][3] = out.component[3];
            if ((short)cursor >= record->countB[i]) {
                record->cursorB[i] = UWAMONO_TRACK_IDLE;
                record->countB[i] = 0;
            }
        }
    }
    if (record->cursorC != UWAMONO_TRACK_IDLE) {
        unsigned short cursor;
        Get_MiddlePoint(&record->startC.vector, &record->followC.vector, record->cursorC, record->countC, &out.vector);
        cursor = record->cursorC;
        record->cursorC = (short)(cursor + 1);
        object->light.ambient[0] = out.component[0];
        object->light.ambient[1] = out.component[1];
        object->light.ambient[2] = out.component[2];
        object->light.ambient[3] = out.component[3];
        if ((short)cursor >= record->countC) {
            record->cursorC = UWAMONO_TRACK_IDLE;
            record->countC = 0;
        }
    }
    if (record->cursorD != UWAMONO_TRACK_IDLE) {
        unsigned short cursor;
        Get_MiddlePoint(&record->startD.vector, &record->followD.vector, record->cursorD, record->countD, &out.vector);
        cursor = record->cursorD;
        record->cursorD = (short)(cursor + 1);
        object->light.fourthDirection[0] = out.component[0];
        object->light.fourthDirection[1] = out.component[1];
        object->light.fourthDirection[2] = out.component[2];
        object->light.fourthDirection[3] = record->followD.component[3];
        if ((short)cursor >= record->countD) {
            record->cursorD = UWAMONO_TRACK_IDLE;
            record->countD = 0;
        }
    }
    if (record->cursorE != UWAMONO_TRACK_IDLE) {
        short cursor;
        cursor = record->cursorE;
        object->drawParam = record->blendStart + (record->blendEnd - record->blendStart) * cursor / record->totalE;
        record->cursorE = record->cursorE + 1;
        if (cursor >= record->totalE) {
            record->cursorE = UWAMONO_TRACK_IDLE;
            record->totalE = 0;
        }
    }
}

/*
 * Per uwamonoRecords[index] (called from DispMapObject before uwamonoLightBlendStep): copies the object's midpoints back into
 * the record (+0xF0, +0x1B0, +0x1E0, +0xCC) and stores the frame count at +0x248. A debug line goes out when the flags
 * word returned through the stack has 0x500 set.
 */
void uwamonoStartAreaBlend(int index)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    struct XenoActor *object = record->body.actor;
    MapCollisionObject request;
    int attribute;
    int count;
    int i;

    UnduParamInit(&request);
    request.block = UnduDataGetHeader(2560, -1);
    UnduCheck(&object->position, 0, &request);
    attribute = request.kind;
    if ((attribute & UWAMONO_PROBE_AREA_FLAGS) == UWAMONO_PROBE_AREA_FLAGS) {
        xglFontDebugPrintf(192, 20, uwamonoAttributeFormat, attribute);
        attribute = request.kind;
        count = (attribute & UWAMONO_PROBE_AREA_MASK) + 1;
    } else {
        count = 0;
    }
    xglFontDebugPrintf(192, 30, uwamonoFrameCountFormat, record->lightArea, count, attribute);
    if (record->lightArea != count) {
        for (i = 0; i < 3; i++) {
            record->startA[i].component[0] = object->light.directions[i][0];
            record->startA[i].component[1] = object->light.directions[i][1];
            record->startA[i].component[2] = object->light.directions[i][2];
            record->startA[i].component[3] = object->light.directions[i][3];
            record->followA[i].component[0] = gameWork.lights.direction[count][i][0];
            record->followA[i].component[1] = gameWork.lights.direction[count][i][1];
            record->followA[i].component[2] = gameWork.lights.direction[count][i][2];
            record->followA[i].component[3] = gameWork.lights.direction[count][i][3];
            record->startB[i].component[0] = object->light.colors[i][0];
            record->startB[i].component[1] = object->light.colors[i][1];
            record->startB[i].component[2] = object->light.colors[i][2];
            record->startB[i].component[3] = object->light.colors[i][3];
            record->followB[i].component[0] = gameWork.lights.color[count][i][0];
            record->followB[i].component[1] = gameWork.lights.color[count][i][1];
            record->followB[i].component[2] = gameWork.lights.color[count][i][2];
            record->followB[i].component[3] = gameWork.lights.color[count][i][3];
            record->cursorA[i] = record->cursorB[i] = 0;
            record->countB[i] = UWAMONO_BLEND_STEPS;
            record->countA[i] = UWAMONO_BLEND_STEPS;
        }
        record->startD.component[0] = object->light.fourthDirection[0];
        record->startD.component[1] = object->light.fourthDirection[1];
        record->startD.component[2] = object->light.fourthDirection[2];
        record->startD.component[3] = object->light.fourthDirection[3];
        record->followD.component[0] = gameWork.lights.type4Direction[count][0];
        record->followD.component[1] = gameWork.lights.type4Direction[count][1];
        record->followD.component[2] = gameWork.lights.type4Direction[count][2];
        record->followD.component[3] = gameWork.lights.type4Direction[count][3];
        record->blendStart = object->drawParam;
        record->blendEnd = gameWork.lights.type4Value[count];
        record->startC.component[0] = object->light.ambient[0];
        record->startC.component[1] = object->light.ambient[1];
        record->startC.component[2] = object->light.ambient[2];
        record->startC.component[3] = object->light.ambient[3];
        record->followC.component[0] = gameWork.lights.ambient[count][0];
        record->followC.component[1] = gameWork.lights.ambient[count][1];
        record->followC.component[2] = gameWork.lights.ambient[count][2];
        record->followC.component[3] = gameWork.lights.ambient[count][3];
        record->cursorC = 0;
        record->cursorE = 0;
        record->cursorD = 0;
        record->countC = UWAMONO_BLEND_STEPS;
        record->totalE = UWAMONO_BLEND_STEPS;
        record->countD = UWAMONO_BLEND_STEPS;
        record->lightArea = count;
    }
}

/* Returns 0. No caller inside this unit. */
int uwamonoUnusedReturnZero(void) {
    return 0;
}

/* .rodata strings of this unit (recovered from the original bytes). */
/* JP: $col00F0F0;ＩＤ踏 %8x
 * EN: $col00F0F0;ID stepped on %8x */
const char uwamonoAttributeFormat[24] = "$col00F0F0;\243\311\243\304\306\247 %8x";

/* JP: $col00F0F0;光ID %2d/%2d:%8x
 * EN: $col00F0F0;Light ID %2d/%2d:%8x */
const char uwamonoFrameCountFormat[40] = "$col00F0F0;\270\367ID %2d/%2d:%8x";
