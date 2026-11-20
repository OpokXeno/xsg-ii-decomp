#include "common.h"
#include "ov01/obj_slot.h"
#include "ov01/obj.h"
#include "ov01/act_step_icon.h"
#include "ov01/menu_panel.h"
#include "ov01/menu_tex_state.h"
#include "ov01/calc.h"

/* The step icon sprite: 24 by 42 pixels. */
#define ACT_STEP_ICON_WIDTH 24
#define ACT_STEP_ICON_HEIGHT 42


void actStepIconUpdate(ActStepIcon *object);
/* Draw callback of the object (its +0x08 slot): draws a 24 by 42 stat sprite at the object's position, with the texture entry chosen by actStepCounterValue. */
void actStepIconDraw(ActStepIcon *object);

void actStepIconCreate(void)
{
    ActStepIcon *object;

    object = objSlotAlloc();
    if (object != 0) {
        object->update = actStepIconUpdate;
        object->draw = actStepIconDraw;
        actStepIconObj = object;
        objStdInit(&object->slide);
        object->slide.position.x = 348.0f;
        object->slide.position.y = 386.0f;
        do {
            object->busy = 0;
            object->slide.position.z = (object->counter = 0);
        } while (0);
    }
}

void actStepIconUpdate(ActStepIcon *object) {

}

/* Texture table at 0x00ab6548: two words per entry, the u and v of a sprite. */
extern int actStepIconTexTable[];

/* Draw callback of the object (its +0x08 slot): draws a 24 by 42 stat sprite at
 * the object's position, with the texture entry chosen by actStepCounterValue. */
void actStepIconDraw(ActStepIcon *object)
{
    GrPacket packet;
    unsigned int entry;
    int y;
    int x;
    int texU;
    int texV;

    entry = actStepCounterValue();
    x = object->slide.position.x;
    y = object->slide.position.y;
    texU = actStepIconTexTable[entry * 2];
    texV = *(entry * 2 + 1 + actStepIconTexTable);
    grGpInit(&packet);
    menuStepIconTexState(&packet);
    grOpenSpr(&packet);
    menuStatSprite(&packet, x, y, ACT_STEP_ICON_WIDTH, ACT_STEP_ICON_HEIGHT, texU, texV);
    grCloseSpr();
    grPacketSend(&packet);
}

/* The step icon object, a common global: deco_flags.c names it too (as unsigned int *). */
ActStepIcon *actStepIconObj;

/* Texture table of actStepIconDraw: the u and v of each of the eight sprite entries. */
int actStepIconTexTable[16] = {
    440, 130, 464, 130, 488, 130, 488, 172,
    440, 172, 464, 172, 488, 172, 440, 214
};
