#include "common.h"
#include "shared.h"
#include "main/xac_eval.h"
#include "main/TiXacAnimCurve.h"

/* The time scale tiXacAnimCurveTimeScale converts (time - origin). */

/* Cubic segment from two keys: times[0..3] and values[0..3] hold each key's time and value and its tangent sample; the power-basis coefficients (coeffs[0] leading, coeffs[3] constant) of the Hermite curve are written to coeffs. */
void xacBuildCubicSegment(f32 *coeffs, const f32 *times, const f32 *values)
{
    f32 toStart;
    f32 fromStart;
    f32 fromStep;
    f32 fromEnd;
    f32 span;
    f32 toEnd;
    f32 toDelta;
    f32 startRatio;
    f32 endRatio;
    f32 fromTail;
    f32 startTerm;
    f32 endTerm;
    f32 inverseSquare;

    fromStart = times[0];
    toStart = values[0];
    fromEnd = times[3];
    fromStep = times[1] - fromStart;
    toEnd = values[3];
    span = fromEnd - fromStart;
    toDelta = toEnd - toStart;
    startRatio = 5729578.0f;
    endRatio = startRatio;
    if (fromStep != 0.0) {
        startRatio = (values[1] - toStart) / fromStep;
    }
    fromStep = fromEnd - times[2];
    /* The (double) compare keeps the double-precision dpcmp call of the original; the plain float compare changes the code. */
    if ((double) fromStep != 0.0) {
        endRatio = (toEnd - values[2]) / fromStep;
    }
    startTerm = span * startRatio;
    endTerm = span * endRatio;
    inverseSquare = 1.0f / (span * span);
    coeffs[2] = startRatio;
    coeffs[1] = (((2.0f * toDelta) + toDelta) - startTerm - startTerm - endTerm) * inverseSquare;
    coeffs[0] = (((startTerm + endTerm) - toDelta - toDelta) * inverseSquare) / span;
    coeffs[3] = values[0];
}

/* Reads two key values of one key header: the first through xacGetKeyValue, the second through xacGetKeyOutTangentAngle. */
void xacGetStartKeyValueAngle(TiXacKey *keyHeader, f32 *first, f32 *second)
{
    *first = xacGetKeyValue(keyHeader);
    *second = xacGetKeyOutTangentAngle(keyHeader);
}

void xacGetEndKeyValueAngle(TiXacKey *keyHeader, f32 *first, f32 *second)
{
    *first = xacGetKeyValue(keyHeader);
    *second = xacGetKeyInTangentAngle(keyHeader);
}

extern f32 sinf(f32 angle);
extern f32 cosf(f32 angle);

extern f32 tiXacAnimCurveTimeScale;
extern f32 xacKeyOriginScale;

/* Evaluates the curve of one key pair at time: the two key segments (position and tangent) are built from the key values and the trig of each second value, then the cubic is evaluated. */
f32 xacEvalCubicKeys(f32 time, TiXacKey *key, TiXacKey *nextKey)
{
    f32 keyFirst;
    f32 keySecond;
    f32 nextFirst;
    f32 nextSecond;
    f32 times[4];
    f32 values[4];
    f32 coeffs[4];
    f32 keyScale;
    f32 nextScale;
    f32 keyCos;
    TiXacKey *record;
    f32 keySin;
    f32 nextCos;
    f32 nextSin;
    f32 keyAngle;
    f32 nextAngle;

    record = key;
    xacGetStartKeyValueAngle(key, &keyFirst, &keySecond);
    xacGetEndKeyValueAngle(nextKey, &nextFirst, &nextSecond);
    keyAngle = keySecond;
    nextAngle = nextSecond;
    keyCos = cosf(keyAngle);
    keySin = sinf(keyAngle);
    nextCos = cosf(nextAngle);
    nextSin = sinf(nextAngle);
    keyScale = (f32) key->origin * xacKeyOriginScale;
    nextScale = (f32) nextKey->origin * xacKeyOriginScale;
    values[0] = keyFirst;
    values[3] = nextFirst;
    times[0] = keyScale;
    times[3] = nextScale;
    values[1] = keyFirst + keySin;
    values[2] = nextFirst - nextSin;
    times[1] = keyScale + keyCos;
    times[2] = nextScale - nextCos;
    xacBuildCubicSegment(coeffs, times, values);
    return xacEvalCubicSegment(time, record, coeffs);
}

/* Cubic segment of one key: coeffs[0] is the leading term, coeffs[3] the constant (Horner form). */
f32 xacEvalCubicSegment(f32 value, const TiXacKey *record, const f32 *coeffs)
{
    f32 scaled;

    scaled = (value - (f32) record->origin) * tiXacAnimCurveTimeScale;
    return scaled * (scaled * (scaled * coeffs[0] + coeffs[1]) + coeffs[2]) + coeffs[3];
}

/* Blends the value of one key with the next key over the segment between them; a zero span counts as one frame. */
f32 xacEvalLinearKeys(f32 time, TiXacKey *key, TiXacKey *nextKey)
{
    f32 offset;
    f32 span;
    f32 offsetScaled;
    f32 fraction;
    f32 valueA;
    f32 valueB;

    offset = time - (f32) key->origin;
    offsetScaled = offset / 30.0f;
    span = (f32) (nextKey->origin - key->origin);
    if (span == 0.0f) {
        span = 1.0f;
    }
    span = span / 30.0f;
    valueA = xacGetKeyValue(key);
    valueB = xacGetKeyValue(nextKey);
    fraction = offsetScaled / span;
    return valueA + (valueB - valueA) * fraction;
}

f32 xacKeyOriginScale = 0.0333333351f;
f32 tiXacAnimCurveTimeScale = 0.0333333351f;
