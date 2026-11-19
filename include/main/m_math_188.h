#ifndef INCLUDE_MAIN_M_MATH_188_H
#define INCLUDE_MAIN_M_MATH_188_H

/* The Hermite helpers of src/main/m_math_188.c that other files call. */

typedef struct {
    float x;
    float y;
    float z;
    float w;
} HermiteVector;

float *MMathCalcHermite(float *destination, HermiteVector *tangent_first, HermiteVector *tangent_second,
                         HermiteVector *endpoint_first, HermiteVector *endpoint_second, float parameter);

void MMathCalcHermitePrm(HermiteVector *tangent_start, HermiteVector *tangent_end,
                          const HermiteVector *point_prev, const HermiteVector *point_start,
                          const HermiteVector *point_end, const HermiteVector *point_next);

#endif /* INCLUDE_MAIN_M_MATH_188_H */
