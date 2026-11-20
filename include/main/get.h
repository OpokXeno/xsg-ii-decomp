#ifndef INCLUDE_MAIN_GET_H
#define INCLUDE_MAIN_GET_H

#include "shared.h"

struct MapCollisionObject;

/* Functions of src/main/get.c that other files call. */

int Get_Rnd(int min, int max);
int Get_RndFloatRange(float minimum, float maximum);
float Get_Decimal_Surplus_for_Radius(float angle);
float Get_Distance(const Vector4 *first, const Vector4 *second);
float Get_Distance3D(const Vector4 *first, const Vector4 *second);
float Get_Angle(const Vector4 *first, const Vector4 *second);
float Get_Angle_Relative(const Vector4 *first, const Vector4 *second, float reference);
float Get_Cursol_by_Reduce_Speed_Angle_Loop(float current, float target, float speed);
float Get_Multi_Max_Under(float value, float step, float maximum);
int Check_Angle(float angle, float from, float to);
void Get_Point_By_AngleLength(const Vector4 *source, Vector4 *destination, float angle, float length);
void Get_One_Step(float length, const Vector4 *source, const Vector4 *destination, Vector4 *output);
void Get_EnemyIDPos(Vector4 *origin, Vector4 *destination);
void Get_MiddlePoint(const Vector4 *start, const Vector4 *following,
                     short point_index, short point_count, Vector4 *result);
void Get_MiddlePoint_Curve(const Vector4 *start, const Vector4 *following, short point_index, short point_count,
                           const float *curve_weights, const float *curve_values, short sample_count, Vector4 *result);
void Get_MiddlePoint_Parabora(const Vector4 *first, const Vector4 *second, int current, int total, float height,
                              Vector4 *destination);
int Check_CrossingOver(const Vector4 *first_start, const Vector4 *first_end,
                       const Vector4 *second_start, const Vector4 *second_end);
int Check_InsideFan(int slotIndex, const Vector4 *origin, float centerAngle, float range, const Vector4 *target,
                    float fanWidth, float radius, int mask);
int Check_MoveSquare(const Vector4 *first, const Vector4 *second, Vector4 *destination, float radius, int map_index);
int Check_Straight_ID(float radius, const Vector4 *start, const Vector4 *end, short steps, Vector4 *result, int block);
int Check_Undu(int slotIndex, const Vector4 *from, const Vector4 *to, int map_block, int bits, Vector4 *hit,
               int attr_mask);
float Get_HeightAttr(Vector4 *position, int unused, int attr_mask, struct MapCollisionObject *request);
float unduGetSlotHeight(int map_index, const Vector4 *position, int unused, int map_key, int attr_mask);
int Get_Attr(Vector4 *position, int unused, int attr_mask);
int Get_Attr_NU(Vector4 *position, int map_key, int attr_mask);
float uwamonoRecordsOverlap(int a, int b);
int uwamonoQuadEdgesVisible(const Vector4 *origin, const Vector4 *corner0, const Vector4 *corner1,
                            const Vector4 *corner2, const Vector4 *corner3);
void vectorCopyPoint(Vector4 *destination, const Vector4 *source, float facing);
void GetBSplineLoop(short frame, short count, int period_value, Vector4 *points, Vector4 *destination);
void GetBSplineLoopDummy(short frame, short count, short period, Vector4 *points, Vector4 *destination, int slot);
int padDebugPrintDirection(int sector_count);

#endif /* INCLUDE_MAIN_GET_H */
