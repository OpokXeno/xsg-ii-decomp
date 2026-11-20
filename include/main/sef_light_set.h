#ifndef INCLUDE_MAIN_SEF_LIGHT_SET_H
#define INCLUDE_MAIN_SEF_LIGHT_SET_H

/* Functions of src/main/sef_light_set.c that other files call. */

struct XglLightSet;

void sefLightSetsApply(struct XglLightSet *lights);

/* The active and default light set blocks (0x180 bytes each): sef.c reads them as its own gate source rows. */
void *sefLightSetActiveGet(void);
void *sefLightSetDefaultGet(void);

#endif /* INCLUDE_MAIN_SEF_LIGHT_SET_H */
