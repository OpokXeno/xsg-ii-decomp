#ifndef INCLUDE_MAIN_OUTER_PRODUCT_H
#define INCLUDE_MAIN_OUTER_PRODUCT_H

#include "shared.h"

/* Functions of src/main/outer_product.c that other files call. */

int CheckPointLine(const Vector4 *first, const Vector4 *second, const Vector4 *point);
int CheckCrossLine(const Vector4 *first_start, const Vector4 *first_end, const Vector4 *second_start, const Vector4 *second_end);
void CalcCrossPoint(const Vector4 *first, const Vector4 *second, const Vector4 *third, const Vector4 *fourth, Vector4 *cross_point);
void CalcVerticalVector(const Vector4 *first, const Vector4 *second, const Vector4 *third, Vector4 *vertical);
float CheckDist2D(const Vector4 *first, const Vector4 *second);

#endif /* INCLUDE_MAIN_OUTER_PRODUCT_H */
