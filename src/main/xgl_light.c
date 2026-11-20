#include "common.h"
#include "main/nml_model_set.h"
#include "shared.h"
#include "main/xgl_2.h"
#include "main/xgl_light.h"

#include "quadword.h"

#include "vu0_macro.h"


float sqrtf(float x);

void *memcpy(void *destination, const void *source, unsigned int size);

/* A loaded light pack file: it starts with the "lxy" tag and is registered in xglLightPackTable. */
typedef struct XglLightPack {
    char tag[4];            /* 0x00: "lxy" */
    u8 unk04[0xC];
    char name[0x20];        /* 0x10 */
    int entryCount;         /* 0x30 */
    u8 unk34[8];
    int keyOffset;          /* 0x3C */
    u8 unk40[0xB0];
    int entryOffsets[1];    /* 0xF0 */
} XglLightPack;

/* One slot of the 16-entry light pack table at 0x0095A410 in this TU's .bss (no symbol). */
typedef struct XglLightPackSlot {
    XglLightPack *pack;      /* 0x00 */
    int refs;               /* 0x04 */
    u32 unk08;
    u32 unk0C;
} XglLightPackSlot;

typedef struct XglLightCurve {
    int count;
    int minimum;
    int maximum;
    int unknown12;
    int fixed;
    u8 unmodeled_14[0x20 - 0x14];
    Vector4Elements keys[1]; /* +0x20: count key rows (time, value) */
} XglLightCurve;


void xglVectorZero(Vector4 *destination);
void xglVectorCopy(void *dst, void *src);
void xglVectorSubXYZ_001AF3F0(AlignedVector4 *dst, AlignedVector4 *minuend, AlignedVector4 *subtrahend);
void xglVectorSub(AlignedVector4 *dst, AlignedVector4 *minuend, AlignedVector4 *subtrahend);
float acosf(float);

/* One light of a light pack (0x50 bytes per element). */
typedef struct XglLightKey {
    DoublewordAlignedVector4 position; /* 0x00: 8-aligned, so a copy of it is ld/sd */
    Vector4 rotation;       /* 0x10: x, y, z angles */
    Vector4 extent;         /* 0x20: radii / slopes / box half sizes by type */
    Vector4 color;          /* 0x30 */
    float coneAngle;        /* 0x40 */
    float penumbra;         /* 0x44 */
    float intensity;        /* 0x48 */
    float reserved4C;
} XglLightKey;

typedef struct XglLightEntry {
    int unk00;
    int type;               /* 0x04 */
    u8 unk08[8];
    u8 channelTypes[20];    /* 0x10 */
    u8 unk24[12];
    int stream[1];          /* 0x30 */
} XglLightEntry;

typedef struct XglLightSlotDir {
    float x;
    float y;
    float z;
    int weakest;
} XglLightSlotDir;

typedef struct XglLightSlots {
    Vector4 colors[6];
    XglLightSlotDir directions[6];
} XglLightSlots;

void xglVectorAddXYZ(AlignedVector4 *dst, AlignedVector4 *first, AlignedVector4 *second);
void xglLightSlotsInsert(XglLightSlots *work, Vector4 *vector, Vector4 *direction);
int xglLightPointColor(Vector4 *result, XglLightKey *key, Vector4 *origin);
int xglLightSpotColor(Vector4 *result, Vector4 *direction, XglLightKey *key, Vector4 *origin);
int xglLightBoxColor(Vector4 *result, Vector4 *direction, XglLightKey *key, Vector4 *origin);
int xglLightConeColor(Vector4 *result, Vector4 *direction, XglLightKey *key, Vector4 *origin);

/* Loads the key's Z, Y, X rotation into the matrix stack (identity first, zero angles skipped).
 * A plain block, not do/while (0): the loop's labels would renumber the output. */
#define LOAD_KEY_ROTATION(key) \
    { \
        xglMatrixStackUnit(); \
        if ((key)->rotation.z != 0.0f) { \
            xglMatrixStackRotZ((key)->rotation.z); \
        } \
        if ((key)->rotation.y != 0.0f) { \
            xglMatrixStackRotY((key)->rotation.y); \
        } \
        if ((key)->rotation.x != 0.0f) { \
            xglMatrixStackRotX((key)->rotation.x); \
        } \
    }
#define XGL_MOTION_TYPE_COUNT 19

extern const u64 lightConeEdgeVectorLow[2];

int strcmp(const char *s1, const char *s2);

void xglLightPackEvaluate(XglLightPack *pack, float time);

extern const u64 lightDebugBoxPointTable[10];

void *memset(void *destination, int value, unsigned int count);
float sinf(float angle);
float cosf(float angle);
float tanf(float value);


/* The 0x0068E660 table: four Vector4 (x, y, z, w) copied as eight doublewords. Each word keeps its own
 * scaffold label, so the copies reference the same symbols as the original. */
extern const u64 lightDebugDirectionalPointTable[8];

extern const u64 lightDebugConePointTable[4];

/* The 0x0068E6C0 table: three Vector4 copied as six doublewords, one scaffold label per word. */
extern const u64 lightDebugAxisPointTable[6];

void xglLightPackDebugAmbient(void *state);
void xglLightPackDebugMarker(XglLightKey *key);
void xglLightPackDebugPoint(XglLightKey *key);

/* This unit's .sdata (0x69E070-0x69E078) and .sbss (0x69E398), defined before
 * their first use so that every access is $gp-relative. */
int xglLightSetDefaultReady = 0;
int xglMotionCacheValid = 0;

void xglStudioLightReset(void *light)
{
    xglLightSetDefault(light);
}

static XglLightPackSlot xglLightPackTable[16];

static XglLightSet xglLightSetDefaultCache;

void xglLightSetDefault(XglLightSet *lightSet)
{
    int i;

    if (xglLightSetDefaultReady == 0) {
        xglLightSetClear(lightSet);
        lightSet->colors[0][0] = lightSet->colors[0][1] = lightSet->colors[0][2] = 0.8f;
        lightSet->colors[1][0] = lightSet->colors[1][1] = lightSet->colors[1][2] = 0.5f;
        lightSet->colors[2][0] = lightSet->colors[2][1] = lightSet->colors[2][2] = 0.5f;
        lightSet->ambientColor.x = lightSet->ambientColor.y = lightSet->ambientColor.z = 0.25f;
        lightSet->ambientColorLower.x = lightSet->ambientColorLower.y = lightSet->ambientColorLower.z = 0.25f;
        lightSet->directions[3][0] = lightSet->directions[4][0] = lightSet->directions[5][0] = 1.5707964f;
        lightSet->viewDirection.x = 1.5707964f;
        lightSet->directions[0][0] = -0.26195645f;
        lightSet->directions[0][1] = 0.53120846f;
        lightSet->directions[0][3] = 1.0f;
        lightSet->directions[1][0] = 2.3561945f;
        lightSet->directions[1][1] = 1.2733574f;
        lightSet->directions[1][3] = 1.0f;
        lightSet->directions[2][0] = 0.34906587f;
        lightSet->directions[2][1] = 3.7866313f;
        lightSet->directions[2][3] = 1.0f;
        for (i = 0; i < 7; i++) {
            xglDirectionFromAngles((Vector4 *)lightSet + i, (Vector4 *)lightSet + i);
        }
        lightSet->pointPositions[0][0] = lightSet->pointPositions[0][1] = lightSet->pointPositions[0][2]
            = lightSet->pointPositions[0][3] = 0.0f;
        lightSet->pointPositions[1][0] = lightSet->pointPositions[1][1] = lightSet->pointPositions[1][2]
            = lightSet->pointPositions[1][3] = 0.0f;
        lightSet->pointPositions[2][0] = lightSet->pointPositions[2][1] = lightSet->pointPositions[2][2]
            = lightSet->pointPositions[2][3] = 0.0f;
        lightSet->pointColors[0][0] = lightSet->pointColors[0][1] = lightSet->pointColors[0][2]
            = lightSet->pointColors[0][3] = 0.0f;
        lightSet->pointColors[1][0] = lightSet->pointColors[1][1] = lightSet->pointColors[1][2]
            = lightSet->pointColors[1][3] = 0.0f;
        lightSet->pointColors[2][0] = lightSet->pointColors[2][1] = lightSet->pointColors[2][2]
            = lightSet->pointColors[2][3] = 0.0f;
        {
            register Vector4 *src = (Vector4 *)lightSet;
            register void *saved = &xglLightSetDefaultCache;
            register Vector4 *copy = src;
            memcpy(saved, copy, sizeof(XglLightSet));
        }
        xglLightSetDefaultReady = 1;
    } else {
        {
            register XglLightSet *dst = lightSet;
            register XglLightSet *cache = &xglLightSetDefaultCache;
            memcpy(dst, cache, sizeof(XglLightSet));
        }
    }
    xglMotionCacheValid = 0;
}

void xglLightPackTableInit(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        xglLightPackTable[i].pack = 0;
        xglLightPackTable[i].refs = 0;
    }
}

int xglLightPackRegister(XglLightPack *pack)
{
    int i;
    Vector4 unused;   /* frame-sizing local, playbook frame */

    if (pack != 0 && pack->tag[0] == 'l' && pack->tag[1] == 'x' && pack->tag[2] == 'y') {
        for (i = 0; i < 16; i++) {
            if (xglLightPackTable[i].pack == pack) {
                xglLightPackTable[i].refs = xglLightPackTable[i].refs + 1;
                return 2;
            }
        }
        {
            for (i = 0; i < 16; i++) {
                if (xglLightPackTable[i].pack == 0) {
                    xglLightPackTable[i].pack = pack;
                    xglLightPackTable[i].refs = 1;
                    return 1;
                }
            }
        }
    }
    return 0;
}

int xglLightPackRelease(XglLightPack *pack)
{
    int i;
    Vector4 unused;

    if (pack != 0 && pack->tag[0] == 'l' && pack->tag[1] == 'x' && pack->tag[2] == 'y') {
        for (i = 0; i < 16; i++) {
            if (xglLightPackTable[i].pack == pack) {
                xglLightPackTable[i].refs = xglLightPackTable[i].refs - 1;
                if (xglLightPackTable[i].refs <= 0) {
                    xglLightPackTable[i].pack = 0;
                    xglLightPackTable[i].refs = 0;
                    return 1;
                }
                return 0;
            }
        }
    }
    return 0;
}

float xglLightPackSampleCurve(int **cursor, float time)
{
    float result = 0.0f;
    XglLightCurve *record;

    record = (XglLightCurve *)*cursor;
    record->fixed = ((int)time << 8) + (int)((time - (float)(int)time) * 256.0f);
    if (record->fixed > (record->maximum << 8)) {
        record->fixed = record->maximum << 8;
    }
    if (record->fixed < (record->minimum << 8)) {
        record->fixed = record->minimum << 8;
    }
    result = pxyInterpolateKeys(record->count, record->keys,
                           (float)(record->fixed / 256) +
                               (float)(record->fixed % 256) / 256.0f,
                           0.2f);
    *cursor = (int *)record + record->count * 4 + 8;
    return result;
}

void xglLightPackEvaluate(XglLightPack *pack, float time)
{
    int i;
    int *entryOffsets;
    XglLightKey *keys;
    XglLightEntry *entry;
    int *cursor;
    float *values;
    int channel;
    int valueIndex;

    entryOffsets = pack->entryOffsets;
    keys = (XglLightKey *)((u8 *)pack + pack->keyOffset);
    for (i = 0; i < pack->entryCount; i++) {
        entry = (XglLightEntry *)((u8 *)pack + entryOffsets[i]);
        cursor = entry->stream;
        values = (float *)&keys[i];
        valueIndex = 0;
        for (channel = 0; channel < 20; channel++) {
            switch (entry->channelTypes[channel]) {
            case 2:
                values[valueIndex] = *(float *)cursor++;
                break;
            case 3:
                values[valueIndex] = 0.0f;
                break;
            case 4:
                values[valueIndex] = 1.0f;
                break;
            case 1:
                {
                    while (((int)cursor & 0xf) != 0) {
                        cursor++;
                    }
                }
                values[valueIndex] = xglLightPackSampleCurve(&cursor, time);
                break;
            }
            valueIndex++;
        }
    }
}

void xglLightSlotsInsert(XglLightSlots *lightSlots, Vector4 *color, Vector4 *lightDir)
{
    float strength;
    int i;
    float weakestStrength;
    Vector4 *slot;

    strength = color->x;
    if (strength < color->y) {
        strength = color->y;
    }
    if (strength < color->z) {
        strength = color->z;
    }
    if ((&lightSlots->colors[0])[(&lightSlots->directions[0])->weakest].w < strength) {
        weakestStrength = 0;
        slot = &(&lightSlots->colors[0])[(&lightSlots->directions[0])->weakest];
        xglVectorCopy(slot, color);
        slot->w = strength;
        slot[6].x = lightDir->x;
        slot[6].y = lightDir->y;
        slot[6].z = lightDir->z;
        if (slot[6].x == 0.0f && slot[6].y == 0.0f && slot[6].z == 0.0f) {
            slot[6].z = 1.0f;
        }
        xglVectorNormal(&slot[6], &slot[6]);
        weakestStrength = (&lightSlots->colors[0])[0].w;
        (&lightSlots->directions[0])->weakest = 0;
        for (i = 1; i < 6; i++) {
            if ((&lightSlots->colors[0])[i].w < weakestStrength) {
                weakestStrength = (&lightSlots->colors[0])[i].w;
                (&lightSlots->directions[0])->weakest = i;
            }
        }
    }
}

int xglLightPointColor(Vector4 *color, XglLightKey *key, Vector4 *receiver)
{
    Vector4 toLight;
    float distance;

    xglVectorZero(color);
    xglVectorSubXYZ_001AF3F0(&toLight, &key->position, receiver);
    distance = toLight.x * toLight.x + toLight.y * toLight.y + toLight.z * toLight.z;
    if (key->extent.y * key->extent.y <= distance) {
        return 0;
    }
    distance = sqrtf(distance);
    xglVectorCopy(color, &key->color);
    if (distance > key->extent.x) {
        distance = (key->extent.y - distance) / (key->extent.y - key->extent.x);
        xglVectorScale(color, color, distance);
    }
    return 1;
}

int xglLightSpotColor(Vector4 *color, Vector4 *lightDirection, XglLightKey *key, Vector4 *receiver)
{
    DoublewordVector4 coneAxis = { { 0.0f, 0.0f, -1.0f, 1.0f } };
    float angle;
    float halfCone;

    halfCone = key->coneAngle * 0.5f;
    xglVectorSub(lightDirection, receiver, &key->position);
    xglVectorNormal(lightDirection, lightDirection);
    xglMatrixStackUnit();
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackMulVector(&coneAxis.vector, &coneAxis.vector);
    xglVectorInner(&angle, &coneAxis.vector, lightDirection);
    angle = acosf(angle);
    if (angle > halfCone) {
        return 0;
    }
    xglVectorCopy(color, &key->color);
    if (halfCone - __builtin_fabsf(key->penumbra) < angle) {
        halfCone = (halfCone - angle) / __builtin_fabsf(key->penumbra);
        xglVectorScale(color, color, halfCone);
    }
    xglVectorScale(lightDirection, lightDirection, -1.0f);
    return 1;
}

int xglLightBoxColor(Vector4 *color, Vector4 *lightDirection, XglLightKey *key, Vector4 *receiver)
{
    DoublewordAlignedVector4 boxAxis = {0};
    float edgeDistance;

    boxAxis.z = 1.0f;
    edgeDistance = 1000000.0f;
    *lightDirection = *receiver;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&key->position);
    if (key->rotation.z != 0.0f) {
        xglMatrixStackRotZ(key->rotation.z);
    }
    if (key->rotation.y != 0.0f) {
        xglMatrixStackRotY(key->rotation.y);
    }
    if (key->rotation.x != 0.0f) {
        xglMatrixStackRotX(key->rotation.x);
    }
    xglMatrixStackMulVector(&boxAxis, &boxAxis);
    xglMatrixStackScale(&key->extent);
    xglMatrixStackInverse();
    lightDirection->w = 1.0f;
    xglMatrixStackMulVector(lightDirection, lightDirection);
    if (lightDirection->x < -1.0f || lightDirection->x > 1.0f || lightDirection->y < -1.0f || lightDirection->y > 1.0f || lightDirection->z > 0.0f) {
        return 0;
    }
    xglVectorCopy(color, &key->color);
    if (__builtin_fabsf((-1.0f - lightDirection->x) * key->extent.x) < edgeDistance) {
        edgeDistance = __builtin_fabsf((-1.0f - lightDirection->x) * key->extent.x);
    }
    if (__builtin_fabsf((1.0f - lightDirection->x) * key->extent.x) < edgeDistance) {
        edgeDistance = __builtin_fabsf((1.0f - lightDirection->x) * key->extent.x);
    }
    if (__builtin_fabsf((-1.0f - lightDirection->y) * key->extent.y) < edgeDistance) {
        edgeDistance = __builtin_fabsf((-1.0f - lightDirection->y) * key->extent.y);
    }
    if (__builtin_fabsf((1.0f - lightDirection->y) * key->extent.y) < edgeDistance) {
        edgeDistance = __builtin_fabsf((1.0f - lightDirection->y) * key->extent.y);
    }
    if (edgeDistance < 1.0f) {
        xglVectorScale(color, color, edgeDistance);
    }
    *lightDirection = boxAxis;
    return 1;
}

int xglLightConeColor(Vector4 *color, Vector4 *lightDirection, XglLightKey *key, Vector4 *receiver)
{
    float keyMatrix[4][4];
    Vector4 coneScale;
    float innerSlope;
    float outerSlope;
    float fade;
    float coneRatio;
    DoublewordVector4 edgePoint;
    DoublewordVector4 spareVector0;

    innerSlope = key->extent.x / key->extent.z;
    outerSlope = key->extent.y / key->extent.z;
    fade = 1.0f;
    coneRatio = 1.0f;
    coneScale.x = key->extent.y;
    coneScale.y = key->extent.z;
    coneScale.z = key->extent.y;
    coneScale.w = 1.0f;

    *lightDirection = *receiver;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&key->position);
    if (key->rotation.z != 0.0f) {
        xglMatrixStackRotZ(key->rotation.z);
    }
    if (key->rotation.y != 0.0f) {
        xglMatrixStackRotY(key->rotation.y);
    }
    if (key->rotation.x != 0.0f) {
        xglMatrixStackRotX(key->rotation.x);
    }
    xglMatrixStackSave(keyMatrix);
    xglMatrixStackScale(&coneScale);
    xglMatrixStackInverse();
    lightDirection->w = 1.0f;
    xglMatrixStackMulVector(lightDirection, lightDirection);

    if (lightDirection->y > 1.0f || lightDirection->y < -1.0f) {
        edgePoint.words[0] = lightConeEdgeVectorLow[0];
        edgePoint.words[1] = lightConeEdgeVectorLow[1];
        if (lightDirection->y < -1.0f) {
            edgePoint.vector.y = -1.0f;
        }
        coneRatio = xglPointLength(lightDirection, &edgePoint.vector) * outerSlope;
        if (coneRatio >= outerSlope) {
            return 0;
        }
        if (coneRatio > innerSlope) {
            fade = 1.0f - (coneRatio - innerSlope) / (outerSlope - innerSlope);
        }
        xglVectorSubXYZ_001AF3F0(lightDirection, lightDirection, &edgePoint.vector);
        xglMatrixStackLoad(keyMatrix);
        lightDirection->w = 0.0f;
        xglMatrixStackMulVector(lightDirection, lightDirection);
        xglVectorNormal(lightDirection, lightDirection);
        xglVectorScale(lightDirection, lightDirection, -1.0f);
        xglVectorCopy(color, &key->color);
        xglVectorScale(color, color, fade);
        return 1;
    } else {
        if (lightDirection->x > -1.0f && lightDirection->x < 1.0f && lightDirection->z > -1.0f && lightDirection->z < 1.0f) {
            coneRatio = sqrtf(lightDirection->x * lightDirection->x + lightDirection->z * lightDirection->z) * outerSlope;
            if (coneRatio >= outerSlope) {
                return 0;
            }
            if (coneRatio > innerSlope) {
                fade = 1.0f - (coneRatio - innerSlope) / (outerSlope - innerSlope);
            }
            lightDirection->y = 0.0f;
            xglMatrixStackLoad(keyMatrix);
            lightDirection->w = 0.0f;
            xglMatrixStackMulVector(lightDirection, lightDirection);
            xglVectorNormal(lightDirection, lightDirection);
            xglVectorScale(lightDirection, lightDirection, -1.0f);
            xglVectorCopy(color, &key->color);
            xglVectorScale(color, color, fade);
            return 1;
        }
    }
    return 0;
}

static Vector4 xglMotionCache[1];

void *xglLightPackCacheGet(void)
{
    if (xglMotionCacheValid != 0) {
        return xglMotionCache;
    }
    return 0;
}

void xglLightPackCacheInvalidate(void)
{
    xglMotionCacheValid = 0;
}

void xglLightPackBuildLightSet(XglLightPack *pack, XglLightSet *lightSet, Vector4 *receiver)
{
    int i;
    int *entryOffsets;
    XglLightSlots lightSlots;

    entryOffsets = pack->entryOffsets;
    memset(&lightSlots, 0, sizeof(lightSlots));
    xglLightSetClear(lightSet);
    for (i = 0; i < pack->entryCount; i++) {
        XglLightEntry *entry = (XglLightEntry *)((u8 *)pack + entryOffsets[i]);

        if (entry->type < XGL_MOTION_TYPE_COUNT) {
            XglLightKey *key = &((XglLightKey *)((u8 *)pack + pack->keyOffset))[i];
            DoublewordAlignedVector4 axisDir = { 0.0f, 0.0f, -1.0f, 1.0f };
            Vector4 pointColor;
            Vector4 coneColor;
            Vector4 coneDirection;
            Vector4 spotColor;
            Vector4 spotDirection;
            Vector4 boxColor;
            Vector4 boxDirection;
            Vector4 scaledColor;

            switch (entry->type) {
            case 9:
                xglVectorScale(xglMotionCache, &key->color, key->intensity);
                pxySetColorScale(xglMotionCache);
                xglMotionCacheValid = 1;
                break;
            case 1:
                xglVectorScale(&lightSet->ambientColor, &key->color, key->intensity);
                xglVectorScale(&lightSet->ambientColorLower, &key->color, key->intensity);
                break;
            case 2:
                xglVectorScale(&lightSet->ambientColor, &key->color, key->intensity);
                xglVectorScale(&lightSet->ambientColorLower, &key->extent, key->intensity);
                LOAD_KEY_ROTATION(key)
                                xglMatrixStackMulVector(&axisDir, &axisDir);
                xglVectorCopy(&lightSet->viewDirection, &axisDir);
                break;
            case 3:
                if (!xglLightPointColor(&pointColor, key, receiver)) {
                    break;
                }
                xglVectorScale(&pointColor, &pointColor, key->intensity);
                xglVectorAddXYZ(&lightSet->ambientColor, &lightSet->ambientColor, &pointColor);
                xglVectorAddXYZ(&lightSet->ambientColorLower, &lightSet->ambientColorLower, &pointColor);
                break;
            case 7:
                if (!xglLightConeColor(&coneColor, &coneDirection, key, receiver)) {
                    break;
                }
                xglVectorScale(&coneColor, &coneColor, key->intensity);
                xglVectorAddXYZ(&lightSet->ambientColor, &lightSet->ambientColor, &coneColor);
                xglVectorAddXYZ(&lightSet->ambientColorLower, &lightSet->ambientColorLower, &coneColor);
                break;
            case 6:
                if (!xglLightSpotColor(&spotColor, &spotDirection, key, receiver)) {
                    break;
                }
                xglVectorScale(&spotColor, &spotColor, key->intensity);
                xglVectorAddXYZ(&lightSet->ambientColor, &lightSet->ambientColor, &spotColor);
                xglVectorAddXYZ(&lightSet->ambientColorLower, &lightSet->ambientColorLower, &spotColor);
                break;
            case 4:
                if (!xglLightBoxColor(&boxColor, &boxDirection, key, receiver)) {
                    break;
                }
                xglVectorScale(&boxColor, &boxColor, key->intensity);
                xglVectorAddXYZ(&lightSet->ambientColor, &lightSet->ambientColor, &boxColor);
                xglVectorAddXYZ(&lightSet->ambientColorLower, &lightSet->ambientColorLower, &boxColor);
                break;
            case 8:
                axisDir.z = 1.0f;
                LOAD_KEY_ROTATION(key)
                                xglMatrixStackMulVector(&axisDir, &axisDir);
                xglVectorScale(&scaledColor, &key->color, key->intensity);
                xglLightSlotsInsert(&lightSlots, &scaledColor, &axisDir);
                break;
            case 10:
                axisDir.z = 1.0f;
                LOAD_KEY_ROTATION(key)
                                xglMatrixStackMulVector(&lightSet->shadows[0], &axisDir);
                lightSet->shadows[0].w = 1.0f;
                break;
            case 11:
                xglVectorSubXYZ_001AF3F0(&lightSet->shadows[0], &key->position, receiver);
                xglVectorNormal(&lightSet->shadows[0], &lightSet->shadows[0]);
                lightSet->shadows[0].w = 1.0f;
                break;
            }
        } else {
            break;
        }
    }
    {
        int pointCount;

        pointCount = 0;
        for (i = 0; i < pack->entryCount; i++) {
            XglLightEntry *entry = (XglLightEntry *)((u8 *)pack + entryOffsets[i]);

            if (entry->type >= XGL_MOTION_TYPE_COUNT) {
                XglLightKey *key = &((XglLightKey *)((u8 *)pack + pack->keyOffset))[i];
                Vector4 pointColor;
                Vector4 pointDirection;
                Vector4 coneColor;
                Vector4 coneDirection;
                Vector4 boxColor;
                Vector4 boxDirection;
                Vector4 spotColor;
                Vector4 spotDirection;

                switch (entry->type) {
                case 19:
                    if (pointCount < 3) {
                        xglVectorCopy((Vector4 *)lightSet->pointPositions[pointCount], &key->position);
                        xglVectorScale((Vector4 *)lightSet->pointColors[pointCount], &key->color,
                            key->intensity * 128.0f);
                        lightSet->pointPositions[pointCount][3] = 1.0f;
                        pointCount++;
                    }
                    break;
                case 20:
                case 35:
                    if (!xglLightPointColor(&pointColor, key, receiver)) {
                        break;
                    }
                    xglVectorSubXYZ_001AF3F0(&pointDirection, &key->position, receiver);
                    xglVectorScale(&pointColor, &pointColor, key->intensity);
                    xglLightSlotsInsert(&lightSlots, &pointColor, &pointDirection);
                    break;
                case 24:
                    if (!xglLightConeColor(&coneColor, &coneDirection, key, receiver)) {
                        break;
                    }
                    xglVectorScale(&coneColor, &coneColor, key->intensity);
                    xglLightSlotsInsert(&lightSlots, &coneColor, &coneDirection);
                    break;
                case 21:
                    if (!xglLightBoxColor(&boxColor, &boxDirection, key, receiver)) {
                        break;
                    }
                    xglVectorScale(&boxColor, &boxColor, key->intensity);
                    xglLightSlotsInsert(&lightSlots, &boxColor, &boxDirection);
                    break;
                case 23:
                    if (!xglLightSpotColor(&spotColor, &spotDirection, key, receiver)) {
                        break;
                    }
                    xglVectorScale(&spotColor, &spotColor, key->intensity);
                    xglLightSlotsInsert(&lightSlots, &spotColor, &spotDirection);
                    break;
                }
            }
        }
    }
    {
        int slot;

        for (slot = 0; slot < 6; slot++) {
            xglVectorCopy(lightSet->colors[slot], &lightSlots.colors[slot]);
            xglVectorCopy(lightSet->directions[slot], &lightSlots.directions[slot]);
            lightSet->colors[slot][3] = 0;
        }
    }
}

void xglLightPackBuildByName(XglLightSet *light, const char *packName, Vector4 *receiver)
{
    int i;

    xglMotionCacheValid = 0;
    for (i = 0; i < 16; i++) {
        if (xglLightPackTable[i].pack != 0) {
            if (strcmp(xglLightPackTable[i].pack->name, packName) == 0) {
                xglLightPackBuildLightSet(xglLightPackTable[i].pack, light, receiver);
                return;
            }
        }
    }
}

void xglLightPackTableClear(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        xglLightPackTable[i].pack = 0;
        xglLightPackTable[i].refs = 0;
    }
}

void xglLightPackEvaluateAll(float time)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (xglLightPackTable[i].pack != 0) {
            xglLightPackEvaluate(xglLightPackTable[i].pack, time);
        }
    }
}

void xglLightPackDebugBox(XglLightKey *key)
{
    int scaled[3];
    DoublewordVector4 point0;
    DoublewordVector4 point1;
    DoublewordVector4 point2;
    DoublewordVector4 point3;
    DoublewordVector4 point4;
    DoublewordVector4 spareVector0;
    DoublewordVector4 spareVector1;
    DoublewordVector4 spareVector2;

    point0.words[0] = lightDebugBoxPointTable[0];
    point0.words[1] = lightDebugBoxPointTable[1];
    point1.words[0] = lightDebugBoxPointTable[2];
    point1.words[1] = lightDebugBoxPointTable[3];
    point2.words[0] = lightDebugBoxPointTable[4];
    point2.words[1] = lightDebugBoxPointTable[5];
    point3.words[0] = lightDebugBoxPointTable[6];
    point3.words[1] = lightDebugBoxPointTable[7];
    point4.words[0] = lightDebugBoxPointTable[8];
    point4.words[1] = lightDebugBoxPointTable[9];
    xglMatrixStackUnit();
    xglMatrixStackTrans(&key->position);
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackScale(&key->extent);
    xglMatrixStackMulVector(&point0.vector, &point0.vector);
    xglMatrixStackMulVector(&point1.vector, &point1.vector);
    xglMatrixStackMulVector(&point2.vector, &point2.vector);
    xglMatrixStackMulVector(&point3.vector, &point3.vector);
    xglMatrixStackMulVector(&point4.vector, &point4.vector);
    /* scaled[] is stored and never read in the original. */
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
}

void xglLightPackDebugAmbient(void *state)
{
}
/* gp-relative float constants of this TU (.sdata). */

void xglLightPackDebugSpot(XglLightKey *key)
{
    int scaled[3];
    Vector4 baseRing[8];
    Vector4 apex;
    float height;
    int i;
    float radius;
    register unsigned int size;
    register Vector4 *entry;
    register Vector4 *entry2;
    register int offset;
    register int zoff8;
    register int zoff12;
    register unsigned int tb;
    register int index;
    register unsigned int tby;

    memset(&apex, 0, (size = 16, size));
    apex.w = 1.0f;
    height = key->extent.z * 0.44f + 0.867f;
    radius = tanf(key->coneAngle * 0.5f) * height;
    apex.z = -height * 1.2f;
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
    for (i = 0; i < 8; i++) {
        index = i << 4;
        tb = (unsigned int)baseRing;
        entry = (Vector4 *)(tb + index);
        entry->x = radius * cosf(2.0f * ((float)i / 8.0f * 3.141592f));
        offset = i * 16 + 4;
        tby = (unsigned int)baseRing;
        entry2 = (Vector4 *)(tby + offset);
        entry2->x = radius * sinf(2.0f * ((float)i / 8.0f * 3.141592f));
        zoff8 = i * 16 + 8;
        *(float *)(zoff8 + (unsigned int)baseRing) = -height;
        zoff12 = i * 16 + 12;
        *(float *)(zoff12 + (unsigned int)baseRing) = 1.0f;
    }
    xglMatrixStackUnit();
    xglMatrixStackTrans(&key->position);
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackMulVector(&apex, &apex);
    for (i = 0; i < 8; i++) {
        xglMatrixStackMulVector(&baseRing[i], &baseRing[i]);
    }
    {
        for (i = 0; i < 8; i++) {
        }
    }
    {
        for (i = 0; i < 8; i++) {
        }
    }
}

void xglLightPackDebugMarker(XglLightKey *key)
{
    int spare0;
    int spare1;
    int spare2;
    int x;
    int y;
    int z;
    DoublewordAlignedVector4 copy;

    copy = key->position;
    copy.w = 0.5f;
    x = key->color.x * 128.0f * key->intensity;
    y = key->color.y * 128.0f * key->intensity;
    z = key->color.z * 128.0f * key->intensity;
}

void xglLightPackDebugDirectional(XglLightKey *key)
{
    int scaled[3];
    DoublewordVector4 point0;
    DoublewordVector4 point1;
    DoublewordVector4 point2;
    DoublewordVector4 point3;
    DoublewordVector4 spareVector0;
    DoublewordVector4 spareVector1;

    point0.words[0] = lightDebugDirectionalPointTable[0];
    point0.words[1] = lightDebugDirectionalPointTable[1];
    point1.words[0] = lightDebugDirectionalPointTable[2];
    point1.words[1] = lightDebugDirectionalPointTable[3];
    point2.words[0] = lightDebugDirectionalPointTable[4];
    point2.words[1] = lightDebugDirectionalPointTable[5];
    point3.words[0] = lightDebugDirectionalPointTable[6];
    point3.words[1] = lightDebugDirectionalPointTable[7];
    /* scaled[] is stored and never read in the original. */
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
    xglMatrixStackUnit();
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackMulVector(&point0.vector, &point0.vector);
    xglMatrixStackMulVector(&point1.vector, &point1.vector);
    xglMatrixStackMulVector(&point2.vector, &point2.vector);
    xglMatrixStackMulVector(&point3.vector, &point3.vector);
}

void xglLightPackDebugPoint(XglLightKey *key)
{
    int spare0;
    int spare1;
    int spare2;
    int x;
    int y;
    int z;
    DoublewordAlignedVector4 copy;

    copy = key->position;
    copy.w = key->extent.x;
    x = key->color.x * 128.0f * key->intensity;
    y = key->color.y * 128.0f * key->intensity;
    z = key->color.z * 128.0f * key->intensity;
    copy.w = key->extent.y;
}

void xglLightPackDebugCone(XglLightKey *key)
{
    int scaled[3];
    Vector4 baseRing[8];
    Vector4 ringA[8];
    Vector4 ringB[8];
    Vector4 ringC[8];
    Vector4 ringD[8];
    Vector4 coneScale;
    DoublewordVector4 point0;
    DoublewordVector4 point1;
    int i;
    float radius;
    Vector4 spareVector0;
    register Vector4 *entry;
    register Vector4 *entry2;
    register Vector4 *entry3;
    register Vector4 *entry4;
    register float *zptr;
    register float *zptr2;
    register int index;
    register int index2;
    register unsigned int tb;
    register unsigned int tb2;
    register int zoff8;
    register int zoff82;
    register int off;
    register int off2;
    register int off3;
    register int off4;
    register int off5;
    register int off6;
    register float *zD;
    register float *zC;
    register float *zD2;
    register float *zC2;
    register int off11;
    register int off12;
    register int index3;
    register int index4;
    register int off7;
    register int off8;
    register int off9;
    register int off10;
    register unsigned int tbd;
    register unsigned int tbc;
    register unsigned int tbc2;
    register unsigned int tbd2;
    register unsigned int tbd3;
    register unsigned int tbc3;
    register unsigned int tbd4;
    register unsigned int tbc4;
    register unsigned int tby;
    register unsigned int tby2;
    register unsigned int tby3;
    register unsigned int tby4;
    register unsigned int tby5;
    register unsigned int tby6;
    register unsigned int tby7;
    register unsigned int tby8;

    point0.words[0] = lightDebugConePointTable[0];
    point0.words[1] = lightDebugConePointTable[1];
    point1.words[0] = lightDebugConePointTable[2];
    point1.words[1] = lightDebugConePointTable[3];
    radius = 1.0f;
    point0.vector.y = (1.0f / key->extent.z) * key->extent.y + 1.0f;
    point1.vector.y = -1.0f - (1.0f / key->extent.z) * key->extent.y;
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
    coneScale.x = key->extent.x;
    coneScale.y = key->extent.z;
    coneScale.z = key->extent.x;
    coneScale.w = 1.0f;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&key->position);
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackScale(&coneScale);
    xglMatrixStackMulVector(&point0.vector, &point0.vector);
    xglMatrixStackMulVector(&point1.vector, &point1.vector);
    for (i = 0; i < 8; i++) {
        index = i << 4;
        tb = (unsigned int)baseRing;
        entry = (Vector4 *)(tb + index);
        entry->x = cosf(2.0f * ((float)i / 8.0f * 3.141592f));
        off = i * 16 + 4;
        tby = (unsigned int)baseRing;
        *(float *)(tby + off) = 1.0f;
        zoff8 = i * 16 + 8;
        tby2 = (unsigned int)baseRing;
        zptr = (float *)(tby2 + zoff8);
        *zptr = sinf(2.0f * ((float)i / 8.0f * 3.141592f));
        off2 = i * 16 + 12;
        tby3 = (unsigned int)baseRing;
        *(float *)(tby3 + off2) = 1.0f;
        xglMatrixStackMulVector(&ringA[i], &baseRing[i]);
        off3 = i * 16 + 4;
        tby4 = (unsigned int)baseRing;
        *(float *)(tby4 + off3) = -1.0f;
        xglMatrixStackMulVector(&ringB[i], &baseRing[i]);
    }
    {
        for (i = 0; i < 8; i++) {
        }
    }
    coneScale.x = key->extent.y;
    coneScale.z = key->extent.y;
    xglMatrixStackUnit();
    xglMatrixStackTrans(&key->position);
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackScale(&coneScale);
    {
        for (i = 0; i < 8; i++) {
        index2 = i << 4;
        tb2 = (unsigned int)baseRing;
        entry3 = (Vector4 *)(tb2 + index2);
        entry3->x = radius * cosf(2.0f * ((float)i / 8.0f * 3.141592f));
        off4 = i * 16 + 4;
        tby5 = (unsigned int)baseRing;
        *(float *)(tby5 + off4) = 1.0f;
        zoff82 = i * 16 + 8;
        tby6 = (unsigned int)baseRing;
        zptr2 = (float *)(tby6 + zoff82);
        *zptr2 = radius * sinf(2.0f * ((float)i / 8.0f * 3.141592f));
        off5 = i * 16 + 12;
        tby7 = (unsigned int)baseRing;
        *(float *)(tby7 + off5) = 1.0f;
        xglMatrixStackMulVector(&ringA[i], &baseRing[i]);
        off6 = i * 16 + 4;
        tby8 = (unsigned int)baseRing;
        *(float *)(tby8 + off6) = -1.0f;
        xglMatrixStackMulVector(&ringB[i], &baseRing[i]);
        index3 = i << 4;
        tbd = (unsigned int)ringD;
        entry2 = (Vector4 *)(tbd + index3);
        index4 = i << 4;
        tbc = (unsigned int)ringC;
        entry4 = (Vector4 *)(tbc + index4);
        entry2->x = entry4->x = cosf(2.0f * ((float)i / 8.0f * 3.141592f)) * 0.707f;
        off7 = i * 16 + 4;
        tbc2 = (unsigned int)ringC;
        *(float *)(tbc2 + off7) = (0.707f / key->extent.z) * key->extent.y + 1.0f;
        off8 = i * 16 + 4;
        tbd2 = (unsigned int)ringD;
        *(float *)(tbd2 + off8) = -1.0f - (0.707f / key->extent.z) * key->extent.y;
        off9 = i * 16 + 8;
        tbd3 = (unsigned int)ringD;
        zD = (float *)(tbd3 + off9);
        off11 = i * 16 + 8;
        tbc3 = (unsigned int)ringC;
        zC = (float *)(tbc3 + off11);
        *zD = *zC = sinf(2.0f * ((float)i / 8.0f * 3.141592f)) * 0.707f;
        off10 = i * 16 + 12;
        tbd4 = (unsigned int)ringD;
        zD2 = (float *)(tbd4 + off10);
        off12 = i * 16 + 12;
        tbc4 = (unsigned int)ringC;
        zC2 = (float *)(tbc4 + off12);
        *zD2 = *zC2 = 1.0f;
        xglMatrixStackMulVector(&ringC[i], &ringC[i]);
        xglMatrixStackMulVector(&ringD[i], &ringD[i]);
        }
    }
    {
        for (i = 0; i < 8; i++) {
        }
    }
}

void xglLightPackDebugAxis(XglLightKey *key)
{
    int scaled[3];
    Vector4 zero;
    DoublewordVector4 point0;
    DoublewordVector4 point1;
    DoublewordVector4 point2;
    DoublewordVector4 spareVector0;
    DoublewordVector4 spareVector1;
    register unsigned int size;

    /* The count is stored inside the call: the original loads it after the buffer address. */
    memset(&zero, 0, (size = 16, size));
    point0.words[0] = lightDebugAxisPointTable[0];
    point0.words[1] = lightDebugAxisPointTable[1];
    point1.words[0] = lightDebugAxisPointTable[2];
    point1.words[1] = lightDebugAxisPointTable[3];
    point2.words[0] = lightDebugAxisPointTable[4];
    point2.words[1] = lightDebugAxisPointTable[5];
    scaled[0] = key->color.x * 128.0f * key->intensity;
    scaled[1] = key->color.y * 128.0f * key->intensity;
    scaled[2] = key->color.z * 128.0f * key->intensity;
    xglMatrixStackUnit();
    xglMatrixStackRotZ(key->rotation.z);
    xglMatrixStackRotY(key->rotation.y);
    xglMatrixStackRotX(key->rotation.x);
    xglMatrixStackMulVector(&point0.vector, &point0.vector);
    xglMatrixStackMulVector(&point1.vector, &point1.vector);
    xglMatrixStackMulVector(&point2.vector, &point2.vector);
    scaled[0] = key->extent.x * 128.0f;
    scaled[1] = key->extent.y * 128.0f;
    scaled[2] = key->extent.z * 128.0f;
    xglVectorSubXYZ_001AF3F0(&point0.vector, &zero, &point0.vector);
}

void xglLightPackDebugDrawAll(void)
{
    int i;
    XglLightKey *key;
    int *entryOffsets;
    int j;
    XglLightEntry *entry;

    for (i = 0; i < 16; i++) {
        if (xglLightPackTable[i].pack != 0) {
            key = (XglLightKey *)((u8 *)xglLightPackTable[i].pack + xglLightPackTable[i].pack->keyOffset);
            entryOffsets = xglLightPackTable[i].pack->entryOffsets;
            for (j = 0; j < xglLightPackTable[i].pack->entryCount; j++) {
                entry = (XglLightEntry *)((u8 *)xglLightPackTable[i].pack + entryOffsets[j]);
                switch (entry->type - 1) {
                case 0:
                    xglLightPackDebugAmbient(key);
                    break;
                case 1:
                    xglLightPackDebugAxis(key);
                    break;
                case 7:
                case 9:
                    xglLightPackDebugDirectional(key);
                    break;
                case 3:
                case 4:
                case 20:
                case 21:
                    xglLightPackDebugBox(key);
                    break;
                case 5:
                case 22:
                    xglLightPackDebugSpot(key);
                    break;
                case 23:
                    xglLightPackDebugCone(key);
                    break;
                case 10:
                case 18:
                    xglLightPackDebugMarker(key);
                    break;
                case 2:
                case 19:
                case 34:
                    xglLightPackDebugPoint(key);
                    break;
                }
                key++;
            }
        }
    }
}

void xglVectorCopy(register void *dst, register void *src)
{
    QuadwordCopy(dst, src);
}

void xglVectorZero(register AlignedVector4 *v)
{
    QuadwordZero(v);
}

void xglVectorSub(register AlignedVector4 *dst, register AlignedVector4 *minuend, register AlignedVector4 *subtrahend)
{
    Vu0SubVector(dst, minuend, subtrahend);
}

void xglVectorAddXYZ(register AlignedVector4 *dst, register AlignedVector4 *first, register AlignedVector4 *second)
{
    Vu0AddVectorXYZ(dst, first, second);
}

void xglVectorSubXYZ_001AF3F0(register AlignedVector4 *dst, register AlignedVector4 *minuend, register AlignedVector4 *subtrahend)
{
    Vu0SubVectorXYZ(dst, minuend, subtrahend);
}

void xglLightSetClear(register XglLightSet *lightSet)
{
    __asm__ __volatile__(
        "sq $0, 0(%0)\n"
        "sq $0, 16(%0)\n"
        "sq $0, 32(%0)\n"
        "sq $0, 48(%0)\n"
        "sq $0, 64(%0)\n"
        "sq $0, 80(%0)\n"
        "sq $0, 96(%0)\n"
        "sq $0, 112(%0)\n"
        "sq $0, 128(%0)\n"
        "sq $0, 144(%0)\n"
        "sq $0, 160(%0)\n"
        "sq $0, 176(%0)\n"
        "sq $0, 192(%0)\n"
        "sq $0, 208(%0)\n"
        "sq $0, 224(%0)\n"
        "sq $0, 240(%0)\n"
        "sq $0, 256(%0)\n"
        "sq $0, 272(%0)\n"
        :
        : "r" (lightSet)
        : "memory");
}

const u64 lightConeEdgeVectorLow[2] = { 0x3F80000000000000ULL, 0x3F80000000000000ULL };

const u64 lightDebugBoxPointTable[10] = {
    0x3F8000003F800000ULL, 0x3F80000000000000ULL, 0xBF8000003F800000ULL, 0x3F80000000000000ULL, 0xBF800000BF800000ULL, 0x3F80000000000000ULL, 0x3F800000BF800000ULL, 0x3F80000000000000ULL,
    0, 0x3F800000BF800000ULL
};

const u64 lightDebugDirectionalPointTable[8] = {
    0, 0x3F8000003F800000ULL, 0, 0x3F800000BF800000ULL, 0x3DCCCCCDULL, 0x3F800000BE99999AULL, 0xBDCCCCCDULL, 0x3F800000BE99999AULL
};

const u64 lightDebugConePointTable[4] = {
    0x3F80000000000000ULL, 0x3F80000000000000ULL, 0xBF80000000000000ULL, 0x3F80000000000000ULL
};

const u64 lightDebugAxisPointTable[6] = {
    0, 0x3F800000BF800000ULL, 0x3E99999AULL, 0x3F80000000000000ULL, 0xBE99999AULL, 0x3F80000000000000ULL
};
