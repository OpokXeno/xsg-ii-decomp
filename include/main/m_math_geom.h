#ifndef INCLUDE_MAIN_M_MATH_GEOM_H
#define INCLUDE_MAIN_M_MATH_GEOM_H

#include "shared.h"
#include "main/m_math.h"

/* Functions of src/main/m_math_geom.c that other files call. */

/* A key of a Hermite track: the point, and the key time in the low half of w. A time of
 * MMATH_HERMITE_END ends the track. */
typedef union MMathHermiteKey {
    HermiteVector point;
    struct {
        unsigned char unmodeled_00[0xC];
        unsigned short time;
    } key;
} MMathHermiteKey;

#define MMATH_HERMITE_END 0x400

/* The default screen parameters; sv_get_image_item.c reads them as vectors, together with its svDefaultScreenParamsTail. */
extern float svDefaultScreenParams[12];

struct XenoActor;

void MMathEvalHermiteKeys(Vector4 *result, MMathHermiteKey *keys, float time);
int MMathActorsInRangeByKind(int kind, struct XenoActor *firstActor, struct XenoActor *secondActor, float limit);

#endif /* INCLUDE_MAIN_M_MATH_GEOM_H */
