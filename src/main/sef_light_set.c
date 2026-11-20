#include "common.h"
#include "shared.h"
#include "main/sef_light_set.h"
#include "main/xgl_light.h"

extern AlignedXglLightSet sefAppliedLightSet;
static AlignedXglLightSet sefActiveLightSet;
static AlignedXglLightSet sefDefaultLightSet;

/* Defined in xgl_2.c; not in main/xgl_2.h because nml_model_set.c passes its own state record. */

/*
 * Three 0x180-byte blocks (sefActiveLightSet, sefDefaultLightSet and sefAppliedLightSet) are moved
 * by sefLightSetsResetDefault and sefLightSetsApply. The getters below return two of them;
 * sefSetGateBlocks (sef.c) calls both, sefRefreshEntryRows (sef.c) the first.
 */
void *sefLightSetActiveGet(void)
{
    return &sefActiveLightSet;
}

void *sefLightSetDefaultGet(void)
{
    return &sefDefaultLightSet;
}

/* Resets sefActiveLightSet to the default light set and copies it to sefDefaultLightSet and sefAppliedLightSet. */
void sefLightSetsResetDefault(void)
{
    xglLightSetDefault(&sefActiveLightSet);
    sefAppliedLightSet = sefDefaultLightSet = sefActiveLightSet;
}

/* Copies the light set to sefAppliedLightSet and sefActiveLightSet and resets sefDefaultLightSet to the default one. */
void sefLightSetsApply(XglLightSet *lights)
{
    sefAppliedLightSet = *lights;
    sefActiveLightSet = *lights;
    xglLightSetDefault(&sefDefaultLightSet);
}
