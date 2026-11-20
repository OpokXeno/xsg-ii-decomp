#ifndef INCLUDE_OV01_EFFECT_POS_H
#define INCLUDE_OV01_EFFECT_POS_H

/* Functions of src/ov01/effect_pos.c that other files call. */

struct EffectPosRecord;

void effectPosTableSet(struct EffectPosRecord *table);
int * effectPosGet(int index);
int unitFileIdGet(int key);
int effectPosGetCount(void);

/* The name batUnitNameGet gives unit 0 ("NULL"). */
extern char *batUnitNullName;

#endif /* INCLUDE_OV01_EFFECT_POS_H */
