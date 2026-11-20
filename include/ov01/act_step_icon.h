#ifndef INCLUDE_OV01_ACT_STEP_ICON_H
#define INCLUDE_OV01_ACT_STEP_ICON_H

#include "shared.h"
#include "ov01/obj.h"

/* Functions of src/ov01/act_step_icon.c that other files call. */

/* The step icon object an objSlotAlloc slot holds: update and draw callbacks, the screen position (slide.position) and
 * two counters cleared by actStepIconCreate. */
typedef struct ActStepIcon {
    u32 slotFlags;                  /* +0x00: the slot flags (OV01_SLOT_*); decoFlagsSetAll hides the icon with SKIP_DRAW */
    void (*update)(struct ActStepIcon *object);
    void (*draw)(struct ActStepIcon *object);
    unsigned char unmodeled_0c[0x04];
    SlideBlock slide;
    int counter;
    int busy;
} ActStepIcon;

/* The icon object actStepIconCreate makes. */
extern ActStepIcon *actStepIconObj;

void actStepIconCreate(void);

#endif /* INCLUDE_OV01_ACT_STEP_ICON_H */
