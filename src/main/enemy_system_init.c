#include "common.h"
#include "shared.h"
#include "main/game_camera.h"
#include "main/game_data.h"
#include "main/uwamono_record.h"
#include "main/get.h"
#include "main/enemy_system_init.h"

extern float sinf(float angle);
extern float cosf(float angle);

void uwamonoClampToStopRange(int index)
{
    UwamonoRecord *record;
    struct XenoActor *actor;
    UwamonoState *state;
    float limit;
    float angle;
    float x;
    float z;

    record = &uwamonoRecords[index];
    actor = record->body.actor;
    state = record->body.motions;
    limit = state->areaSize - actor->radius - 0.1f;
    if (Get_Distance(&state->area, &actor->position) < limit) {
        return;
    }
    angle = Get_Angle(&state->area, &actor->position);
    x = state->area.x + sinf(angle) * limit;
    actor->position.x = x;
    record->position.x = x;
    z = state->area.z + cosf(angle) * limit;
    actor->position.z = z;
    record->position.z = z;
    actor->direction.y = Get_Decimal_Surplus_for_Radius(angle + 3.1415927f);
}

void TM_Script_Spline_Add(int index, float x, float y, float z)
{
    UwamonoState *state;

    state = uwamonoRecords[index].body.motions;
    state->trailPoints[state->trailCount].x = x;
    state->trailPoints[state->trailCount].y = y;
    state->trailPoints[state->trailCount].z = z;
    state->trailPoints[state->trailCount++].w = 1.0f;
}


void uwamonoReleaseChildren(int index)
{
    UwamonoState *state;
    unsigned int workBase;
    int children[4];
    int i;

    if (index == -1) {
        return;
    }
    state = uwamonoRecords[index].body.motions;
    for (i = 0; i < 4; i++) {
        children[i] = state->together[i];
    }
    /* The flag byte is gameWork.rowFlags[control bit 11][flagIndex], reached by a byte offset from &gameWork:
     * the indexed form changes the code. */
    if (children[0] == -1 && children[1] == -1 && children[2] == -1 && children[3] == -1) {
        UwamonoState *childState;
        GameWork *selfWork;
        int offset;

        childState = uwamonoRecords[index].body.motions;
        selfWork = &gameWork;
        offset = childState->row + 0x1920;
        if (!(selfWork->control & 0x800)) {
            offset = childState->row + 0x1720;
        }
        ((unsigned char *)selfWork)[offset] = 1;
        uwamonoReleaseRecord(index);
        return;
    }
    workBase = (unsigned int)&gameWork;
    for (i = 0; i < 4; i++) {
        int child = children[i];

        if (child != -1) {
            UwamonoState *childState;
            int offset;

            childState = uwamonoRecords[child].body.motions;
            offset = childState->row + 0x1920;
            if (!(((GameWork *)workBase)->control & 0x800)) {
                offset = childState->row + 0x1720;
            }
            *(unsigned char *)(offset + workBase) = 1;
            uwamonoReleaseRecord(child);
        }
    }
}
