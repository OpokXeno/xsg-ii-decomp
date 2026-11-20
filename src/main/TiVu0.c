#include "common.h"
#include "main/assert_fail.h"

extern const char tiVu0UnreachableAssertText[];
extern const char tiVu0AssertFile[];

/* Unconditional assert(0) at TiVu0.euc.c line 739: a fail-only path with no role shown. */
void tiVu0AssertUnreachable739(void)
{
    assertFailed(tiVu0UnreachableAssertText, tiVu0AssertFile, 739);
}

/* Unconditional assert(0) at TiVu0.euc.c line 758: a fail-only path with no role shown. */
void tiVu0AssertUnreachable758(void)
{
    assertFailed(tiVu0UnreachableAssertText, tiVu0AssertFile, 758);
}

#include "shared.h"
#include "main/TiXacAnimCurve.h"

extern const char xacUnreachableAssertText[];

extern f32 xacKeyU16ScaleA;

/* Reads one curve key's value by key type: 0 gives 0.0, 2 a float at +4, 3 a u16 at +6 times 1e-4, 5 a float at +8; other types assert.
 * The assert file names TiXacAnimCurve.euc.c, so this reader belongs to that family. */
f32 xacGetKeyOutTangentAngle(TiXacKey *key) {
    int keyType;

    keyType = key->typeFlags & XAC_KEY_TYPE_MASK;
    switch (keyType) {
    case 0:
        return 0.0f;
    case 3:
        return (f32) key->payload.halfwords[1] * xacKeyU16ScaleA;
    case 5:
        return key->payload.value[1];
    case 2:
        return key->payload.value[0];
    default:
        assertFailed(xacUnreachableAssertText, xacAssertFile, 32);
        return 0.0f;
    }
}

extern f32 xacKeyU16ScaleB;

/* Same type switch as xacGetKeyOutTangentAngle with another key layout: 3 reads a u16 at +4 times 1e-4, 2 and 5 read a float at +4. */
f32 xacGetKeyInTangentAngle(TiXacKey *key) {
    int keyType;

    keyType = key->typeFlags & XAC_KEY_TYPE_MASK;
    switch (keyType) {
    case 0:
        return 0.0f;
    case 3:
        return (f32) key->payload.halfwords[0] * xacKeyU16ScaleB;
    case 5:
        return key->payload.value[0];
    case 2:
        return key->payload.value[0];
    default:
        assertFailed(xacUnreachableAssertText, xacAssertFile, 67);
        return 0.0f;
    }
}

const char tiVu0UnreachableAssertText[8] = "0";
const char tiVu0AssertFile[24] = "../TiVu0.euc.c";
const char xacUnreachableAssertText[8] = "0";

f32 xacKeyU16ScaleA = 0.0001f;
f32 xacKeyU16ScaleB = 0.0001f;
