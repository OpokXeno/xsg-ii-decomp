/*
 * OV01 original TU 55: 0x00aa5248..0x00aa5390 (3 functions)
 */
#include "common.h"
#include "ov01/tu005.h"
#include "ov01/tu055.h"
#include "ov01/tu062.h"

/* The step icon sprite: 24 by 42 pixels. */
#define ACT_STEP_ICON_WIDTH 24
#define ACT_STEP_ICON_HEIGHT 42

typedef struct Ov01Object {
    unsigned char unmodeled_00[0x04];
    void (*update)(struct Ov01Object *object);
    void (*draw)(struct Ov01Object *object);
    unsigned char unmodeled_0c[0x04];
    float x;
    float y;
    int state;
    unsigned char unmodeled_1c[0x74];
    int counter;
    int busy;
} Ov01Object;

void *objSlotAlloc(void);
void func_00AA52D0(struct Ov01Object *object);
/* Draw callback of the object (its +0x08 slot): draws a 24 by 42 stat sprite at the object's position, with the texture entry chosen by actStepCounterValue. */
void actStepIconDraw(Ov01Object *object);
extern void *actStepIconObj;

void actStepIconCreate(void)
{
    Ov01Object *object;

    object = objSlotAlloc();
    if (object != 0) {
        object->update = func_00AA52D0;
        object->draw = actStepIconDraw;
        actStepIconObj = object;
        ov01MatrixBlockInit(&object->x);
        object->x = 348.0f;
        object->y = 386.0f;
        do {
            object->busy = 0;
            object->state = (object->counter = 0);
        } while (0);
    }
}

void func_00AA52D0(struct Ov01Object *object) {

}

/* Texture table at 0x00ab6548: two words per entry, the u and v of a sprite. */
extern int actStepIconTexTable[];
int actStepCounterValue(void);
void func_00AA8530(GrPacket *packet);

/* Draw callback of the object (its +0x08 slot): draws a 24 by 42 stat sprite at
 * the object's position, with the texture entry chosen by actStepCounterValue. */
void actStepIconDraw(Ov01Object *object)
{
    GrPacket packet;
    unsigned int entry;
    int y;
    int x;
    int texU;
    int texV;

    entry = actStepCounterValue();
    x = object->x;
    y = object->y;
    texU = actStepIconTexTable[entry * 2];
    texV = *(entry * 2 + 1 + actStepIconTexTable);
    grGpInit(&packet);
    func_00AA8530(&packet);
    grPutSprOpen(&packet);
    menuStatSprite(&packet, x, y, ACT_STEP_ICON_WIDTH, ACT_STEP_ICON_HEIGHT, texU, texV);
    grPutSprClose();
    grPacketSend(&packet);
}

/* The step icon object, a common global: tu066 names it too (as unsigned int *). */
void *actStepIconObj;

/* Texture table of actStepIconDraw: the u and v of each of the eight sprite entries. */
int actStepIconTexTable[16] = {
    440, 130, 464, 130, 488, 130, 488, 172,
    440, 172, 464, 172, 488, 172, 440, 214
};
