#include "common.h"
#include "main/TiXacAnimCurve.h"
#include "main/assert_fail.h"
#include "shared.h"

#include "main/xac_eval.h"
/* Time first: the calls below keep the original argument order (key first changes the code);
 * xac_eval.c defines them key first. */

/* Channel bits of a transform header (channelFlags), in the order of the nine values: translation X, Y, Z (0 to 2),
 * rotation X, Y, Z (3 to 5), scale X, Y, Z (6 to 8); the same order as TiHrcDynamics. */
enum XacChannelBit {
    XAC_CHANNEL_TX = 0x1,
    XAC_CHANNEL_TY = 0x2,
    XAC_CHANNEL_TZ = 0x4,
    XAC_CHANNEL_RX = 0x8,
    XAC_CHANNEL_RY = 0x10,
    XAC_CHANNEL_RZ = 0x20,
    XAC_CHANNEL_SX = 0x40,
    XAC_CHANNEL_SY = 0x80,
    XAC_CHANNEL_SZ = 0x100
};

/* Mode bit of a transform header (headerFlags) and the same bit of the object flags word it is copied to. */
#define XAC_TRANS_MODE_BIT 0x1
#define XAC_TRANS_OBJ_MODE_BIT 0x2

/* Key type word: XAC_KEY_END marks the last key of a curve, XAC_KEY_LINEAR a linear segment (the type itself is XAC_KEY_TYPE_MASK). */
#define XAC_KEY_END 0x4000
#define XAC_KEY_LINEAR 0x8000


const char xacAssertFile[24] = "../TiXacAnimCurve.euc.c";
const char xacKeyHeaderAssertText[16] = "pKeyHeader";

/* Returns one float of a key header by its key type: types 0 and 1 read +4, 2 and 3 read +8, 5 reads +C, 4 and types above 5 give 0.0; asserts on a null pointer. */
f32 xacGetKeyValue(TiXacKey *pKeyHeader) {
    if (pKeyHeader == 0) {
        assertFailed(xacKeyHeaderAssertText, xacAssertFile, 78);
    }
    switch (pKeyHeader->typeFlags & XAC_KEY_TYPE_MASK) {
    case 5:
        return pKeyHeader->payload.value[2];
    case 2:
    case 3:
        return pKeyHeader->payload.value[1];
    case 0:
    case 1:
        return pKeyHeader->payload.value[0];
    default:
        return 0.0f;
    }
}

const char xacHermiteKeyAssertText[24] = "XacKeyType_Hermite";
const char xacHermiteKeyAssertExpr[24] = "!\"XacKeyType_Hermite\"";
const char xacIllegalCurveTypeAssertText[24] = "IllegalCurveType";
const char xacIllegalCurveTypeAssertExpr[32] = "!\"IllegalCurveType\"";

/* Evaluates a keyframe curve at curveParam: reads keys from *keyCursor, advances it, interpolates through xacEvalLinearKeys; asserts on an illegal curve type. */
f32 xacEvalCurve(f32 curveParam, u8 **keyCursor) {
    TiXacKey *key;
    TiXacKey *lastKey;
    TiXacKey *nextKey;
    f32 *keyValues;
    f32 value;
    f32 evalTime;
    int searching;

    /* Key records are 8, 12 or 16 bytes by key type, so the next key is found by a byte stride in each case. */
    key = (TiXacKey *) *keyCursor;
    value = 0.0f;
    evalTime = curveParam;
    searching = 1;
    if (evalTime < (f32) key->origin) {
        evalTime = (f32) key->origin;
    }
    do {
        switch (key->typeFlags & XAC_KEY_TYPE_MASK) {
        case 1:
            keyValues = key->payload.value;
            nextKey = (TiXacKey *) ((u8 *) key + 8);
            if (searching != 0) {
                if (key->typeFlags & XAC_KEY_END) {
                    value = keyValues[0];
                    searching = 0;
                } else if ((f32) key->origin <= evalTime) {
                    if (evalTime < (f32) nextKey->origin) {
                        value = xacEvalLinearKeys(evalTime, key, nextKey);
                        searching = 0;
                    }
                }
            }
            lastKey = key;
            key = nextKey;
            break;
        case 4:
            if (xacHermiteKeyAssertText != 0) {
                assertFailed(xacHermiteKeyAssertExpr, xacAssertFile, 154);
            }
            break;
        case 2:
            keyValues = key->payload.value;
            nextKey = (TiXacKey *) ((u8 *) key + 12);
            if (searching != 0) {
                if (key->typeFlags & XAC_KEY_END) {
                    value = keyValues[1];
                    searching = 0;
                } else if ((f32) key->origin <= evalTime) {
                    if (evalTime < (f32) nextKey->origin) {
                        searching = 0;
                        if ((key->typeFlags & XAC_KEY_LINEAR) && nextKey->typeFlags == 1) {
                            value = xacEvalLinearKeys(evalTime, key, nextKey);
                        } else {
                            value = xacEvalCubicKeys(evalTime, key, nextKey);
                        }
                    }
                }
            }
            lastKey = key;
            key = nextKey;
            break;
        case 3:
            keyValues = key->payload.value;
            nextKey = (TiXacKey *) ((u8 *) key + 12);
            if (searching != 0) {
                if (key->typeFlags & XAC_KEY_END) {
                    value = keyValues[1];
                    searching = 0;
                } else if ((f32) key->origin <= evalTime) {
                    if (evalTime < (f32) nextKey->origin) {
                        searching = 0;
                        if ((key->typeFlags & XAC_KEY_LINEAR) && nextKey->typeFlags == 1) {
                            value = xacEvalLinearKeys(evalTime, key, nextKey);
                        } else {
                            value = xacEvalCubicKeys(evalTime, key, nextKey);
                        }
                    }
                }
            }
            lastKey = key;
            key = nextKey;
            break;
        case 5:
            keyValues = key->payload.value;
            nextKey = (TiXacKey *) ((u8 *) key + 16);
            if (searching != 0) {
                if (key->typeFlags & XAC_KEY_END) {
                    value = keyValues[2];
                    searching = 0;
                } else if ((f32) key->origin <= evalTime) {
                    if (evalTime < (f32) nextKey->origin) {
                        searching = 0;
                        if ((key->typeFlags & XAC_KEY_LINEAR) && nextKey->typeFlags == 1) {
                            value = xacEvalLinearKeys(evalTime, key, nextKey);
                        } else {
                            value = xacEvalCubicKeys(evalTime, key, nextKey);
                        }
                    }
                }
            }
            lastKey = key;
            key = nextKey;
            break;
        case 0:
            keyValues = key->payload.value;
            nextKey = (TiXacKey *) ((u8 *) key + 8);
            if (searching != 0) {
                if (key->typeFlags & XAC_KEY_END) {
                    value = keyValues[0];
                    searching = 0;
                } else if ((f32) key->origin <= evalTime) {
                    if (evalTime < (f32) nextKey->origin) {
                        searching = 0;
                        if ((key->typeFlags & XAC_KEY_LINEAR) && nextKey->typeFlags == 1) {
                            value = xacEvalLinearKeys(evalTime, key, nextKey);
                        } else {
                            value = xacEvalCubicKeys(evalTime, key, nextKey);
                        }
                    }
                }
            }
            lastKey = key;
            key = nextKey;
            break;
        default:
            if (xacIllegalCurveTypeAssertText != 0) {
                assertFailed(xacIllegalCurveTypeAssertExpr, xacAssertFile, 293);
            }
            lastKey = 0;
            break;
        }
    } while (!(lastKey->typeFlags & XAC_KEY_END));
    *keyCursor = (u8 *) key;
    return value;
}

const char xacResAssertText[8] = "res";
const char xacTransJointAssertText[16] = "pTransJoint";

/* Evaluates the nine channels of a joint transform (pTransJoint) into values[0..8]: each channel is a curve or a stored default, chosen by its flag bit. */
void xacEvalTransJoint(f32 *values, TiHrcJoint *pTransJoint, u8 **keyCursor, f32 frame) {
    TiXacTransformHeader *header;
    u8 *cursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 314);
    }
    if (pTransJoint == 0) {
        assertFailed(xacTransJointAssertText, xacAssertFile, 315);
    }
    if (*keyCursor != 0) {
        header = (TiXacTransformHeader *) *keyCursor;
        if (((TiXacTransformHeader *) *keyCursor)->headerFlags & XAC_TRANS_MODE_BIT) {
            pTransJoint->flags |= XAC_TRANS_OBJ_MODE_BIT;
        } else {
            pTransJoint->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
        }
        cursor = *keyCursor + 4;
        if (header->channelFlags & XAC_CHANNEL_TX) {
            values[0] = xacEvalCurve(frame, &cursor);
        } else {
            values[0] = pTransJoint->translationDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TY) {
            values[1] = xacEvalCurve(frame, &cursor);
        } else {
            values[1] = pTransJoint->translationDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TZ) {
            values[2] = xacEvalCurve(frame, &cursor);
        } else {
            values[2] = pTransJoint->translationDefault[2];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RX) {
            values[3] = xacEvalCurve(frame, &cursor);
        } else {
            values[3] = pTransJoint->rotationDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RY) {
            values[4] = xacEvalCurve(frame, &cursor);
        } else {
            values[4] = pTransJoint->rotationDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RZ) {
            values[5] = xacEvalCurve(frame, &cursor);
        } else {
            values[5] = pTransJoint->rotationDefault[2];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SX) {
            values[6] = xacEvalCurve(frame, &cursor);
        } else {
            values[6] = pTransJoint->scaleDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SY) {
            values[7] = xacEvalCurve(frame, &cursor);
        } else {
            values[7] = pTransJoint->scaleDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SZ) {
            values[8] = xacEvalCurve(frame, &cursor);
        } else {
            values[8] = pTransJoint->scaleDefault[2];
        }
        *keyCursor = cursor;
    } else {
        values[0] = pTransJoint->translationDefault[0];
        values[1] = pTransJoint->translationDefault[1];
        values[2] = pTransJoint->translationDefault[2];
        values[3] = pTransJoint->rotationDefault[0];
        values[4] = pTransJoint->rotationDefault[1];
        values[5] = pTransJoint->rotationDefault[2];
        values[6] = pTransJoint->scaleDefault[0];
        values[7] = pTransJoint->scaleDefault[1];
        values[8] = pTransJoint->scaleDefault[2];
        pTransJoint->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
    }
}

/* Evaluates nine keyed channels into values[0..8] from the transform header at *keyCursor (flag bits 0x1 to 0x100); channels 6 to 8 default to 1.0, the rest to 0.0; writes the advanced cursor back. */
void xacEvalTransform(f32 *values, u8 **keyCursor, f32 curveParam) {
    u8 *cursor;

    if (*keyCursor != 0) {
        cursor = *keyCursor + 4;
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TX) {
            values[0] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[0] = 0.0f;
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TY) {
            values[1] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[1] = 0.0f;
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TZ) {
            values[2] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[2] = 0.0f;
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RX) {
            values[3] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[3] = 0.0f;
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RY) {
            values[4] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[4] = 0.0f;
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RZ) {
            values[5] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[5] = 0.0f;
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SX) {
            values[6] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[6] = 1.0f;
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SY) {
            values[7] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[7] = 1.0f;
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SZ) {
            values[8] = xacEvalCurve(curveParam, &cursor);
        } else {
            values[8] = 1.0f;
        }
        *keyCursor = cursor;
    }
}

const char xacTransLocatorAssertText[16] = "pTransLocator";
const char xacTransFaceAssertText[16] = "pTransFace";

/* Nine-channel transform reader for pTransLocator; same shape as the pTransJoint reader. */
void xacEvalTransLocator(f32 *values, TiHrcLocator *pTransLocator, u8 **keyCursor, f32 frame) {
    TiXacTransformHeader *header;
    u8 *cursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 419);
    }
    if (pTransLocator == 0) {
        assertFailed(xacTransLocatorAssertText, xacAssertFile, 420);
    }
    if (*keyCursor != 0) {
        header = (TiXacTransformHeader *) *keyCursor;
        if (((TiXacTransformHeader *) *keyCursor)->headerFlags & XAC_TRANS_MODE_BIT) {
            pTransLocator->flags |= XAC_TRANS_OBJ_MODE_BIT;
        } else {
            pTransLocator->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
        }
        cursor = *keyCursor + 4;
        if (header->channelFlags & XAC_CHANNEL_TX) {
            values[0] = xacEvalCurve(frame, &cursor);
        } else {
            values[0] = pTransLocator->translationDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TY) {
            values[1] = xacEvalCurve(frame, &cursor);
        } else {
            values[1] = pTransLocator->translationDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TZ) {
            values[2] = xacEvalCurve(frame, &cursor);
        } else {
            values[2] = pTransLocator->translationDefault[2];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RX) {
            values[3] = xacEvalCurve(frame, &cursor);
        } else {
            values[3] = pTransLocator->rotationDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RY) {
            values[4] = xacEvalCurve(frame, &cursor);
        } else {
            values[4] = pTransLocator->rotationDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RZ) {
            values[5] = xacEvalCurve(frame, &cursor);
        } else {
            values[5] = pTransLocator->rotationDefault[2];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SX) {
            values[6] = xacEvalCurve(frame, &cursor);
        } else {
            values[6] = pTransLocator->scaleDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SY) {
            values[7] = xacEvalCurve(frame, &cursor);
        } else {
            values[7] = pTransLocator->scaleDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SZ) {
            values[8] = xacEvalCurve(frame, &cursor);
        } else {
            values[8] = pTransLocator->scaleDefault[2];
        }
        *keyCursor = cursor;
    } else {
        values[0] = pTransLocator->translationDefault[0];
        values[1] = pTransLocator->translationDefault[1];
        values[2] = pTransLocator->translationDefault[2];
        values[3] = pTransLocator->rotationDefault[0];
        values[4] = pTransLocator->rotationDefault[1];
        values[5] = pTransLocator->rotationDefault[2];
        values[6] = pTransLocator->scaleDefault[0];
        values[7] = pTransLocator->scaleDefault[1];
        values[8] = pTransLocator->scaleDefault[2];
        pTransLocator->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
    }
}

/* Nine-channel transform reader for pTransFace; same shape as the pTransJoint reader. */
void xacEvalTransFace(f32 *values, TiHrcSkip *pTransFace, u8 **keyCursor, f32 frame) {
    TiXacTransformHeader *header;
    u8 *cursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 488);
    }
    if (pTransFace == 0) {
        assertFailed(xacTransFaceAssertText, xacAssertFile, 489);
    }
    if (*keyCursor != 0) {
        header = (TiXacTransformHeader *) *keyCursor;
        if (((TiXacTransformHeader *) *keyCursor)->headerFlags & XAC_TRANS_MODE_BIT) {
            pTransFace->flags |= XAC_TRANS_OBJ_MODE_BIT;
        } else {
            pTransFace->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
        }
        cursor = *keyCursor + 4;
        if (header->channelFlags & XAC_CHANNEL_TX) {
            values[0] = xacEvalCurve(frame, &cursor);
        } else {
            values[0] = pTransFace->translationDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TY) {
            values[1] = xacEvalCurve(frame, &cursor);
        } else {
            values[1] = pTransFace->translationDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_TZ) {
            values[2] = xacEvalCurve(frame, &cursor);
        } else {
            values[2] = pTransFace->translationDefault[2];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RX) {
            values[3] = xacEvalCurve(frame, &cursor);
        } else {
            values[3] = pTransFace->rotationDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RY) {
            values[4] = xacEvalCurve(frame, &cursor);
        } else {
            values[4] = pTransFace->rotationDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_RZ) {
            values[5] = xacEvalCurve(frame, &cursor);
        } else {
            values[5] = pTransFace->rotationDefault[2];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SX) {
            values[6] = xacEvalCurve(frame, &cursor);
        } else {
            values[6] = pTransFace->scaleDefault[0];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SY) {
            values[7] = xacEvalCurve(frame, &cursor);
        } else {
            values[7] = pTransFace->scaleDefault[1];
        }
        if (((TiXacTransformHeader *) *keyCursor)->channelFlags & XAC_CHANNEL_SZ) {
            values[8] = xacEvalCurve(frame, &cursor);
        } else {
            values[8] = pTransFace->scaleDefault[2];
        }
        *keyCursor = cursor;
    } else {
        values[0] = pTransFace->translationDefault[0];
        values[1] = pTransFace->translationDefault[1];
        values[2] = pTransFace->translationDefault[2];
        values[3] = pTransFace->rotationDefault[0];
        values[4] = pTransFace->rotationDefault[1];
        values[5] = pTransFace->rotationDefault[2];
        values[6] = pTransFace->scaleDefault[0];
        values[7] = pTransFace->scaleDefault[1];
        values[8] = pTransFace->scaleDefault[2];
        pTransFace->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
    }
}

const char xacTransDynamicsAssertText[16] = "pTransDynamics";

/* The channel tests read the header through a cast of *ppCursor on each test: a local header pointer changes the code. */
void xacEvalTransDynamics(f32 *values, TiHrcDynamics *pTransDynamics, u8 **ppCursor, f32 curveParam) {
    u8 *keyCursor;
    TiXacTransformHeader *header;
    f32 *pWeight;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 555);
    }
    if (pTransDynamics == 0) {
        assertFailed(xacTransDynamicsAssertText, xacAssertFile, 556);
    }
    if (*ppCursor != 0) {
        header = (TiXacTransformHeader *) *ppCursor;
        pWeight = &header->weight;
        keyCursor = *ppCursor + 8;
        if (header->headerFlags & XAC_TRANS_MODE_BIT) {
            pTransDynamics->flags |= XAC_TRANS_OBJ_MODE_BIT;
        } else {
            pTransDynamics->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
        }
        pTransDynamics->weight = *pWeight;
        if (header->channelFlags & XAC_CHANNEL_TX) {
            values[0] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[0] = pTransDynamics->translationDefault.x;
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_TY) {
            values[1] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[1] = pTransDynamics->translationDefault.y;
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_TZ) {
            values[2] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[2] = pTransDynamics->translationDefault.z;
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_RX) {
            values[3] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[3] = pTransDynamics->rotationDefault[0];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_RY) {
            values[4] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[4] = pTransDynamics->rotationDefault[1];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_RZ) {
            values[5] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[5] = pTransDynamics->rotationDefault[2];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_SX) {
            values[6] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[6] = pTransDynamics->scaleDefault[0];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_SY) {
            values[7] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[7] = pTransDynamics->scaleDefault[1];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_SZ) {
            values[8] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[8] = pTransDynamics->scaleDefault[2];
        }
        *ppCursor = keyCursor;
    } else {
        pTransDynamics->weight = 0.0f;
        pTransDynamics->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
        values[0] = pTransDynamics->translationDefault.x;
        values[1] = pTransDynamics->translationDefault.y;
        values[2] = pTransDynamics->translationDefault.z;
        values[3] = pTransDynamics->rotationDefault[0];
        values[4] = pTransDynamics->rotationDefault[1];
        values[5] = pTransDynamics->rotationDefault[2];
        values[6] = pTransDynamics->scaleDefault[0];
        values[7] = pTransDynamics->scaleDefault[1];
        values[8] = pTransDynamics->scaleDefault[2];
    }
}

/* Second pTransDynamics reader (asserts at lines 625/626); see xacEvalTransDynamics for the difference. */
void xacEvalTransDynamicsNoWeight(f32 *values, TiHrcDynamics *pTransDynamics, u8 **ppCursor, f32 curveParam) {
    u8 *keyCursor;
    TiXacTransformHeader *header;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 625);
    }
    if (pTransDynamics == 0) {
        assertFailed(xacTransDynamicsAssertText, xacAssertFile, 626);
    }
    pTransDynamics->weight = 0.0f;
    if (*ppCursor != 0) {
        header = (TiXacTransformHeader *) *ppCursor;
        if (header->headerFlags & XAC_TRANS_MODE_BIT) {
            pTransDynamics->flags |= XAC_TRANS_OBJ_MODE_BIT;
        } else {
            pTransDynamics->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
        }
        keyCursor = (u8 *) header + 4;
        if (header->channelFlags & XAC_CHANNEL_TX) {
            values[0] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[0] = pTransDynamics->translationDefault.x;
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_TY) {
            values[1] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[1] = pTransDynamics->translationDefault.y;
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_TZ) {
            values[2] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[2] = pTransDynamics->translationDefault.z;
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_RX) {
            values[3] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[3] = pTransDynamics->rotationDefault[0];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_RY) {
            values[4] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[4] = pTransDynamics->rotationDefault[1];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_RZ) {
            values[5] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[5] = pTransDynamics->rotationDefault[2];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_SX) {
            values[6] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[6] = pTransDynamics->scaleDefault[0];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_SY) {
            values[7] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[7] = pTransDynamics->scaleDefault[1];
        }
        if (((TiXacTransformHeader *) *ppCursor)->channelFlags & XAC_CHANNEL_SZ) {
            values[8] = xacEvalCurve(curveParam, &keyCursor);
        } else {
            values[8] = pTransDynamics->scaleDefault[2];
        }
        *ppCursor = keyCursor;
    } else {
        pTransDynamics->flags &= ~XAC_TRANS_OBJ_MODE_BIT;
        values[0] = pTransDynamics->translationDefault.x;
        values[1] = pTransDynamics->translationDefault.y;
        values[2] = pTransDynamics->translationDefault.z;
        values[3] = pTransDynamics->rotationDefault[0];
        values[4] = pTransDynamics->rotationDefault[1];
        values[5] = pTransDynamics->rotationDefault[2];
        values[6] = pTransDynamics->scaleDefault[0];
        values[7] = pTransDynamics->scaleDefault[1];
        values[8] = pTransDynamics->scaleDefault[2];
    }
}

const char xacPpJointHeaderAssertText[16] = "ppJointHeader";

/* Reads six curve values from a joint header, starting 4 bytes in, and advances *ppJointHeader. */
u8 *xacEvalJointPose(f32 *values, u8 **ppJointHeader, f32 curveParam) {
    u8 *keyCursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 696);
    }
    if (ppJointHeader == 0) {
        assertFailed(xacPpJointHeaderAssertText, xacAssertFile, 697);
    }
    keyCursor = *ppJointHeader + 4;
    values[0] = xacEvalCurve(curveParam, &keyCursor);
    values[1] = xacEvalCurve(curveParam, &keyCursor);
    values[2] = xacEvalCurve(curveParam, &keyCursor);
    values[3] = xacEvalCurve(curveParam, &keyCursor);
    values[4] = xacEvalCurve(curveParam, &keyCursor);
    values[5] = xacEvalCurve(curveParam, &keyCursor);
    *ppJointHeader = keyCursor;
    return keyCursor;
}

/* Reads one curve value from a joint header, starting 4 bytes in, and advances *ppJointHeader. */
u8 *xacEvalJointValue(f32 curveParam, f32 *values, u8 **ppJointHeader) {
    u8 *keyCursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 722);
    }
    if (ppJointHeader == 0) {
        assertFailed(xacPpJointHeaderAssertText, xacAssertFile, 723);
    }
    keyCursor = *ppJointHeader + 4;
    values[0] = xacEvalCurve(curveParam, &keyCursor);
    *ppJointHeader = keyCursor;
    return keyCursor;
}

/* Reads nine curve values from a joint header, starting 4 bytes in, and advances *ppJointHeader. */
u8 *xacReadJointCurves9(f32 *values, u8 **ppJointHeader, f32 curveParam) {
    u8 *keyCursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 741);
    }
    if (ppJointHeader == 0) {
        assertFailed(xacPpJointHeaderAssertText, xacAssertFile, 742);
    }
    keyCursor = *ppJointHeader + 4;
    values[0] = xacEvalCurve(curveParam, &keyCursor);
    values[1] = xacEvalCurve(curveParam, &keyCursor);
    values[2] = xacEvalCurve(curveParam, &keyCursor);
    values[3] = xacEvalCurve(curveParam, &keyCursor);
    values[4] = xacEvalCurve(curveParam, &keyCursor);
    values[5] = xacEvalCurve(curveParam, &keyCursor);
    values[6] = xacEvalCurve(curveParam, &keyCursor);
    values[7] = xacEvalCurve(curveParam, &keyCursor);
    values[8] = xacEvalCurve(curveParam, &keyCursor);
    *ppJointHeader = keyCursor;
    return keyCursor;
}

/* The label text, then four NUL bytes and the two floats that the original .rodata keeps after it (no reader in this TU reaches them). */
const char xacJointHeaderAssertText[24] = "pJointHeader\0\0\0\0\xab\xaa\xaa\x3e\x54\xda\xae\x4a";

/* Reads nine curve values from a joint header (pJointHeader, assert lines 772/773). */
void xacEvalJointCurves9(f32 *values, u8 *pJointHeader, f32 curveParam) {
    u8 *keyCursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 772);
    }
    if (pJointHeader == 0) {
        assertFailed(xacJointHeaderAssertText, xacAssertFile, 773);
    }
    keyCursor = pJointHeader + 4;
    values[0] = xacEvalCurve(curveParam, &keyCursor);
    values[1] = xacEvalCurve(curveParam, &keyCursor);
    values[2] = xacEvalCurve(curveParam, &keyCursor);
    values[3] = xacEvalCurve(curveParam, &keyCursor);
    values[4] = xacEvalCurve(curveParam, &keyCursor);
    values[5] = xacEvalCurve(curveParam, &keyCursor);
    values[6] = xacEvalCurve(curveParam, &keyCursor);
    values[7] = xacEvalCurve(curveParam, &keyCursor);
    values[8] = xacEvalCurve(curveParam, &keyCursor);
}

/* Reads twelve curve values from a joint header (ppJointHeader, assert lines 801/802). */
void xacReadWindDirCurves(f32 *values, u8 **ppJointHeader, f32 curveParam) {
    u8 *keyCursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 801);
    }
    if (ppJointHeader == 0) {
        assertFailed(xacPpJointHeaderAssertText, xacAssertFile, 802);
    }
    keyCursor = *ppJointHeader + 4;
    values[0] = xacEvalCurve(curveParam, &keyCursor);
    if (values[0] == 0.0f) {
        return;
    }
    values[1] = xacEvalCurve(curveParam, &keyCursor);
    values[2] = xacEvalCurve(curveParam, &keyCursor);
    values[3] = xacEvalCurve(curveParam, &keyCursor);
    values[4] = xacEvalCurve(curveParam, &keyCursor);
    values[5] = xacEvalCurve(curveParam, &keyCursor);
    values[6] = xacEvalCurve(curveParam, &keyCursor);
    values[7] = xacEvalCurve(curveParam, &keyCursor);
    values[8] = xacEvalCurve(curveParam, &keyCursor);
    values[9] = xacEvalCurve(curveParam, &keyCursor);
    values[10] = xacEvalCurve(curveParam, &keyCursor);
    values[11] = xacEvalCurve(curveParam, &keyCursor);
    *ppJointHeader = keyCursor;
}

/* Reads twelve curve values from a joint header (ppJointHeader, assert lines 840/841). */
void xacReadWindPointCurves(f32 *values, u8 **ppJointHeader, f32 curveParam) {
    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 840);
    }
    if (ppJointHeader == 0) {
        assertFailed(xacPpJointHeaderAssertText, xacAssertFile, 841);
    }
    {
        u8 *keyCursor;

        keyCursor = *ppJointHeader + 4;
        values[0] = xacEvalCurve(curveParam, &keyCursor);
        if (values[0] == 0.0f) {
            return;
        }
        values[1] = xacEvalCurve(curveParam, &keyCursor);
        values[2] = xacEvalCurve(curveParam, &keyCursor);
        values[3] = xacEvalCurve(curveParam, &keyCursor);
        values[4] = xacEvalCurve(curveParam, &keyCursor);
        values[5] = xacEvalCurve(curveParam, &keyCursor);
        values[6] = xacEvalCurve(curveParam, &keyCursor);
        values[7] = xacEvalCurve(curveParam, &keyCursor);
        values[8] = xacEvalCurve(curveParam, &keyCursor);
        values[9] = xacEvalCurve(curveParam, &keyCursor);
        values[10] = xacEvalCurve(curveParam, &keyCursor);
        values[11] = xacEvalCurve(curveParam, &keyCursor);
        *ppJointHeader = keyCursor;
    }
}

/* Reads twelve curve values from a joint header (ppJointHeader, assert lines 879/880). */
void xacReadJointCurves12Padded(f32 *values, u8 **ppJointHeader, f32 curveParam) {
    u8 *keyCursor;
    f32 lastValue;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 879);
    }
    if (ppJointHeader == 0) {
        assertFailed(xacPpJointHeaderAssertText, xacAssertFile, 880);
    }
    keyCursor = *ppJointHeader + 4;
    values[0] = xacEvalCurve(curveParam, &keyCursor);
    values[1] = xacEvalCurve(curveParam, &keyCursor);
    values[2] = xacEvalCurve(curveParam, &keyCursor);
    values[3] = xacEvalCurve(curveParam, &keyCursor);
    values[4] = xacEvalCurve(curveParam, &keyCursor);
    values[5] = xacEvalCurve(curveParam, &keyCursor);
    values[6] = xacEvalCurve(curveParam, &keyCursor);
    values[7] = xacEvalCurve(curveParam, &keyCursor);
    values[8] = xacEvalCurve(curveParam, &keyCursor);
    values[9] = xacEvalCurve(curveParam, &keyCursor);
    values[10] = xacEvalCurve(curveParam, &keyCursor);
    lastValue = xacEvalCurve(curveParam, &keyCursor);
    do {
        values[14] = 1.0f;
        *ppJointHeader = keyCursor;
    } while (0);
    values[11] = lastValue;
    values[13] = 1.0f;
    values[12] = 1.0f;
}

/* Reads fifteen curve values from a joint header, starting 4 bytes in, and advances *ppJointHeader. */
u8 *xacReadJointCurves15(f32 *values, u8 **ppJointHeader, f32 curveParam) {
    u8 *keyCursor;

    if (values == 0) {
        assertFailed(xacResAssertText, xacAssertFile, 924);
    }
    if (ppJointHeader == 0) {
        assertFailed(xacPpJointHeaderAssertText, xacAssertFile, 925);
    }
    keyCursor = *ppJointHeader + 4;
    values[0] = xacEvalCurve(curveParam, &keyCursor);
    values[1] = xacEvalCurve(curveParam, &keyCursor);
    values[2] = xacEvalCurve(curveParam, &keyCursor);
    values[3] = xacEvalCurve(curveParam, &keyCursor);
    values[4] = xacEvalCurve(curveParam, &keyCursor);
    values[5] = xacEvalCurve(curveParam, &keyCursor);
    values[6] = xacEvalCurve(curveParam, &keyCursor);
    values[7] = xacEvalCurve(curveParam, &keyCursor);
    values[8] = xacEvalCurve(curveParam, &keyCursor);
    values[9] = xacEvalCurve(curveParam, &keyCursor);
    values[10] = xacEvalCurve(curveParam, &keyCursor);
    values[11] = xacEvalCurve(curveParam, &keyCursor);
    values[12] = xacEvalCurve(curveParam, &keyCursor);
    values[13] = xacEvalCurve(curveParam, &keyCursor);
    values[14] = xacEvalCurve(curveParam, &keyCursor);
    *ppJointHeader = keyCursor;
    return keyCursor;
}

/* Scans a key table (count word, then key times) for the key segment around curveParam; writes a 0/1 flag to its first argument and returns the key pointer. */
f32 *xacFindVisibilityKey(int *pFlag, XacCurveKeyTable *pTable, f32 curveParam) {
    f32 *pKey;
    f32 *pValue;
    f32 prevTime;
    int index;

    prevTime = 0.0f;
    if (pTable->count <= 0) {
        return pTable->keys;
    }
    pKey = pTable->keys;
    for (index = 0; index < pTable->count; index++) {
        pValue = pKey + 1;
        if ((prevTime < curveParam && curveParam <= *pKey) || index == 0) {
            if (*pValue != 0.0f) {
                *pFlag = 1;
            } else {
                *pFlag = 0;
            }
        }
        prevTime = *pKey;
        pKey = pValue + 1;
    }
    if (prevTime < curveParam) {
        if (*pValue != 0.0f) {
            *pFlag = 1;
        } else {
            *pFlag = 0;
        }
    }
    return pKey;
}
