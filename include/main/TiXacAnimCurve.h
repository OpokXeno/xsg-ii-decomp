#ifndef INCLUDE_MAIN_TIXACANIMCURVE_H
#define INCLUDE_MAIN_TIXACANIMCURVE_H

#include "shared.h"
#include "main/TiMRPackage.h"

/* Curve keys, transform headers and the functions of src/main/TiXacAnimCurve.c that other files call. */

/* The key type is the low 12 bits of typeFlags. */
#define XAC_KEY_TYPE_MASK 0xfff

/* One curve key header: the curve type is the low 12 bits of typeFlags, origin is the start time, and the payload at +4 holds
 * three floats (value[0] at +4, value[1] at +8, value[2] at +C) or, for some types, a u16 pair (halfwords[0] at +4, halfwords[1] at +6). */
typedef struct TiXacKey {
    u16 typeFlags;
    s16 origin;
    union {
        f32 value[3];
        u16 halfwords[6];
    } payload;
} TiXacKey;

/* Transform channel header: its first halfword holds the nine channel enable bits (0x1 to 0x100) and headerFlags the mode bit (0x1), the weight is at +4. */
typedef struct TiXacTransformHeader {
    u16 channelFlags;
    u16 headerFlags;
    f32 weight;
} TiXacTransformHeader;

/* A curve key table: the key count, then (time, value) float pairs. */
typedef struct XacCurveKeyTable {
    int count;
    f32 keys[1];
} XacCurveKeyTable;


extern const char xacAssertFile[];

f32 xacGetKeyValue(TiXacKey *pKeyHeader);
void xacGetStartKeyValueAngle(TiXacKey *keyHeader, f32 *first, f32 *second);
void xacGetEndKeyValueAngle(TiXacKey *keyHeader, f32 *first, f32 *second);
f32 xacGetKeyOutTangentAngle(TiXacKey *key);
f32 xacGetKeyInTangentAngle(TiXacKey *key);
f32 xacEvalCurve(f32 curveParam, u8 **keyCursor);
void xacEvalTransform(f32 *values, u8 **keyCursor, f32 curveParam);
u8 *xacEvalJointPose(f32 *values, u8 **ppJointHeader, f32 curveParam);
u8 *xacEvalJointValue(f32 curveParam, f32 *values, u8 **ppJointHeader);
u8 *xacReadJointCurves9(f32 *values, u8 **ppJointHeader, f32 curveParam);
void xacEvalTransJoint(f32 *values, TiHrcJoint *pTransJoint, u8 **keyCursor, f32 frame);
void xacEvalTransLocator(f32 *values, TiHrcLocator *pTransLocator, u8 **keyCursor, f32 frame);
void xacEvalTransFace(f32 *values, TiHrcSkip *pTransFace, u8 **keyCursor, f32 frame);
void xacEvalTransDynamics(f32 *values, TiHrcDynamics *pTransDynamics, u8 **ppCursor, f32 curveParam);
void xacEvalJointCurves9(f32 *values, u8 *pJointHeader, f32 curveParam);
void xacEvalTransDynamicsNoWeight(f32 *values, TiHrcDynamics *pTransDynamics, u8 **ppCursor, f32 curveParam);
f32 *xacFindVisibilityKey(int *pFlag, XacCurveKeyTable *pTable, f32 curveParam);
void xacReadWindDirCurves(f32 *values, u8 **ppJointHeader, f32 curveParam);
void xacReadWindPointCurves(f32 *values, u8 **ppJointHeader, f32 curveParam);
void xacReadJointCurves12Padded(f32 *values, u8 **ppJointHeader, f32 curveParam);
u8 *xacReadJointCurves15(f32 *values, u8 **ppJointHeader, f32 curveParam);

#endif /* INCLUDE_MAIN_TIXACANIMCURVE_H */
