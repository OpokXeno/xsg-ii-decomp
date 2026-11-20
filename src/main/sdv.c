#include "common.h"
#include "shared.h"
#include "main/sdv.h"
#include "main/srs.h"
#include "gs.h"
#include "main/sef.h"
#include "main/sv_get_image_item.h"
#include "main/game_camera.h"
#include "vu0_macro.h"
#include "main/TiWindBase.h"
#include "main/gifState.h"
#include "main/m_math.h"
#include "main/vu0_matrix.h"
#include "main/xgl_camera.h"
#include "main/sm_initilize.h"
#include "main/xgl_2.h"
#include "quadword.h"
#include "main/game_defocus.h"
#include "ov01/m_gs.h"
#include "main/xgl_packet.h"
#include "main/pxy_packet_state.h"
#include "main/xgl_studio.h"
#include "main/sc_get.h"

#include "main/vu0_rotate.h"
extern int _sdvAmbFrame;
extern int _sdvAmbState;
void sdvSetAmbStateSub(int state, int effect_no, int force);
extern float _sdvMapRgb[4];
extern float _sdvAmbient[4];
void sdvSetAmbient(void *map_rgb, void *ambient);

void sdvInitAmbient(void)
{
    _sdvAmbFrame = 0;
    _sdvAmbState = 0;
}

void sdvSaveAmbient(void)
{
}
static int sdvAmbEffectCharNo;
static int sdvAmbEffectType;

void sdvSetAmbStateSub(int state, int effect_no, int force)
{
    if (force != 0) {
        _sdvAmbState = state;
        return;
    }
    srsAnalyzeEftNo(effect_no, &sdvAmbEffectCharNo, &sdvAmbEffectType);
    if (sdvAmbEffectType == 9) {
        _sdvAmbState = state;
    }
}

void sdvSetAmbState(int state, int effect_no)
{
    sdvSetAmbStateSub(state, effect_no, 0);
}

void sdvSetAmbState2(int state, int effect_no)
{
    sdvSetAmbStateSub(state, effect_no, 1);
}

void sdvSetAmbient(void *map_rgb, void *ambient)
{
}

void sdvRestoreAmbient(void)
{
    sdvSetAmbient(_sdvMapRgb, _sdvAmbient);
}

void sdvExecAmbient(void)
{
    Vector4 map_rgb;
    Vector4 ambient;
    float parameter;
    int state = _sdvAmbState;

    if (state == 0) {
        return;
    }

    if (state == 1) {
        if (_sdvAmbFrame >= 20) {
            _sdvAmbState = 0;
            _sdvAmbFrame = 20;
            state = 0;
        } else {
            _sdvAmbFrame++;
        }
    } else if (state == 2) {
        if (_sdvAmbFrame <= 0) {
            _sdvAmbFrame = 0;
            state = 0;
            _sdvAmbState = 0;
        } else {
            _sdvAmbFrame--;
        }
    }

    if (state != 0) {
        float half = 0.5f;
        float divisor = 20.0f;

        parameter = (float)_sdvAmbFrame / divisor;
        /* These constrained operations implement the eight evidenced COP2 instructions. */
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n\t"
            "mfc1 $8, %0\n\t"
            "qmtc2 $8, vf2\n\t"
            "vmulx.xyz vf1, vf1, vf2x\n\t"
            "sqc2 vf1, 0(%2)"
            :
            : "f"(half), "r"(&_sdvMapRgb[0]), "r"(&map_rgb)
            : "$8", "memory");
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n\t"
            "mfc1 $8, %0\n\t"
            "qmtc2 $8, vf2\n\t"
            "vmulx.xyz vf1, vf1, vf2x\n\t"
            "sqc2 vf1, 0(%2)"
            :
            : "f"(half), "r"(&_sdvAmbient[0]), "r"(&ambient)
            : "$8", "memory");
        MMathVectorInterpolation(&map_rgb, (const Vector4 *)_sdvMapRgb,
                                 &map_rgb, parameter);
        MMathVectorInterpolation(&ambient, (const Vector4 *)_sdvAmbient,
                                 &ambient, parameter);
        sdvSetAmbient(&map_rgb, &ambient);
    }
}
float _sdvMapRgb[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float _sdvAmbient[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
int _sdvAmbFrame = 0;
int _sdvAmbState = 0;

/* Quad-corner and unit-square tables of the alter strips (read by sdvCreateAlter and sdvExecAlter). */
const AlignedVector4 sdvAlterForward = { 0.0f, 0.0f, -1.0f, 1.0f };
const AlignedVector4 sdvAlterStartDelta = { 0.0f, 0.0f, -1.0f, 1.0f };
const AlignedVector4 sdvAlterUpVector = { 0.0f, 1.0f, 0.0f, 1.0f };
const AlignedVector4 sdvAlterMirrorXY = { -1.0f, -1.0f, 1.0f, 1.0f };
const AlignedVector4 sdvAlterOriginVector = { 0.0f, 0.0f, 0.0f, 1.0f };


#define SDV_KEY_END 0x400           /* frame value that ends the table */
#define SDV_KEY_WAIT_HIT 0x1000     /* the row waits for the hit signal */
#define SDV_KEY_FRAME_MASK 0xFFF    /* frame of a hit-wait row */

/* Actor select bits of a key row: which actor of the effect request the row applies to. They live in the select
   column, not the frame column, so LINK_1 and LINK_3 share their values with SDV_KEY_END and SDV_KEY_WAIT_HIT. */
#define SDV_KEY_SOURCE 0x40         /* the request's source actor */
#define SDV_KEY_TARGET 0x80         /* the request's target actor */
#define SDV_KEY_LINK_1 0x400        /* the request's linked actors (links[0..2]) */
#define SDV_KEY_LINK_2 0x800
#define SDV_KEY_LINK_3 0x1000
#define SDV_KEY_OBJECT1 0x100       /* the request's partner, the second actor of a pair technique (camera kinds only) */

/* Hit signal of the current effect step; sefExecStep clears it. */
extern short _hitSignal;

/* A vector stored component by component through a one-member union, so that the stores have alias set 0:
 * the functions using it say which plain Vector4 form changed the output. */
typedef union SdvVectorView {
    Vector4 vector;
} SdvVectorView;

/* Returns the current row when the sequence reaches it (and advances to the next row), else -1
 * (sdvExecSeqTbl's key test in X1). */
int sdvCheckSeqKey(SdvSequence *sequence)
{
    int result = -1;
    short key = sequence->keys[sequence->position * sequence->stride];
    SefRequest *request = (SefRequest *)sefGetBattleData();
    int hitFrame = key & SDV_KEY_FRAME_MASK;

    if ((key & SDV_KEY_WAIT_HIT) && _hitSignal != 0 && sequence->elapsed >= hitFrame) {
        if (request->waitHit == 0) {
            sequence->position++;
        } else {
            result = sequence->position++;
        }
    } else if (sequence->elapsed >= key) {
        result = sequence->position++;
    }
    return result;
}

/* Counts one frame (up to 0x3FF) and runs the key test; SDV_KEY_END when the sequence has ended
 * (sdvExecSeqTbl in X1, which counts after the test). */
int sdvExecSeqTbl(SdvSequence *sequence)
{
    if (sequence->keys[sequence->position * sequence->stride] == SDV_KEY_END) {
        return SDV_KEY_END;
    }
    sequence->elapsed++;
    if (sequence->elapsed >= 0x400) {
        sequence->elapsed = 0x3FF;
    }
    return sdvCheckSeqKey(sequence);
}

/* The same key test on a row position kept by the caller and an elapsed frame count passed in;
 * SDV_KEY_END when the sequence has ended (sdvProgressKey in X1). */
int sdvProgressKey(short *keys, short *position, int stride, int elapsed)
{
    int result = -1;
    int key = keys[*position * stride];
    SefRequest *request = (SefRequest *)sefGetBattleData();
    int waitsForHit = key & SDV_KEY_WAIT_HIT;
    int hitFrame = key & SDV_KEY_FRAME_MASK;

    if (key == SDV_KEY_END) {
        return SDV_KEY_END;
    }
    if (waitsForHit && _hitSignal != 0 && elapsed >= hitFrame) {
        if (request->waitHit == 0) {
            (*position)++;
        } else {
            result = (*position)++;
        }
    } else if (elapsed >= key) {
        result = (*position)++;
    }
    return result;
}

/* Columns of an effect key row: the halfwords after the row's frame. */
enum SdvEffectKeyColumn {
    SDV_EFFECT_KIND = 1,        /* the MSe effect kind */
    SDV_EFFECT_TARGET = 2,      /* SDV_KEY_TARGET plus the part the effect targets */
    SDV_EFFECT_OWNER = 3,       /* SDV_KEY_SOURCE or SDV_KEY_LINK_n plus the part it is attached to */
    SDV_EFFECT_OFFSET_X = 4,    /* offset from that actor, in thousandths */
    SDV_EFFECT_OFFSET_Y = 5,
    SDV_EFFECT_OFFSET_Z = 6
};

/* Steps an effect key sequence and fires the MSe effect of the row it reaches, attached to
 * the actors of the current effect request. */
void sdvExecEffectSeq(SdvSequence *sequence, int effectId)
{
    MSeFireEntry fire;
    SdvVectorView offset;
    short *keys;
    int row;
    int index;
    SefRequest *request;
    int kind;
    int targetSelect;
    int ownerSelect;

    if (sequence == 0 || sequence->keys == 0) {
        return;
    }
    index = sdvExecSeqTbl(sequence);
    if ((unsigned int)index >= SDV_KEY_END) {
        return;
    }
    request = (SefRequest *)sefGetBattleData();
    /* The row is read halfword by halfword: through a row pointer gcc keeps the row in keys' register.
       offset is an SdvVectorView: as a plain Vector4 gcc loads and converts the y and z halfwords before the
       x store. */
    keys = sequence->keys;
    row = index * sequence->stride;
    kind = keys[row + SDV_EFFECT_KIND];
    targetSelect = keys[row + SDV_EFFECT_TARGET];
    ownerSelect = keys[row + SDV_EFFECT_OWNER];
    offset.vector.x = keys[row + SDV_EFFECT_OFFSET_X];
    offset.vector.y = keys[row + SDV_EFFECT_OFFSET_Y];
    offset.vector.z = keys[row + SDV_EFFECT_OFFSET_Z];
    offset.vector.w = 1.0f;
    MSeFireEntryClear(&fire);
    Vu0SefScaleVectorXYZNI(&fire.offset, &offset.vector, 0.001f);
    fire.type = kind;
    fire.param = effectId;
    fire.owner = request->source;
    /* A select is the actor bit plus the part index plus one; 0xA1 (part 0x1F), 0x80 and 0xA2 (part 1)
     * and the bare SDV_KEY_SOURCE (part 1) are remapped. */
    if (targetSelect == 0xA1) {
        fire.target = request->target;
        fire.targetPart = 0x1F;
    } else if (targetSelect == SDV_KEY_TARGET || targetSelect == 0xA2) {
        fire.target = request->target;
        fire.targetPart = 1;
    } else if (targetSelect & SDV_KEY_TARGET) {
        fire.target = request->target;
        fire.targetPart = targetSelect - (SDV_KEY_TARGET + 1);
    }
    if (ownerSelect & SDV_KEY_LINK_1) {
        fire.link = request->links[0];
        fire.linkPart = ownerSelect - (SDV_KEY_LINK_1 + 1);
    } else if (ownerSelect & SDV_KEY_LINK_2) {
        fire.link = request->links[1];
        fire.linkPart = ownerSelect - (SDV_KEY_LINK_2 + 1);
    } else if (ownerSelect & SDV_KEY_LINK_3) {
        fire.link = request->links[2];
        fire.linkPart = ownerSelect - (SDV_KEY_LINK_3 + 1);
    } else if (ownerSelect == SDV_KEY_SOURCE) {
        fire.ownerPart = 1;
    } else if (ownerSelect & SDV_KEY_SOURCE) {
        fire.ownerPart = ownerSelect - (SDV_KEY_SOURCE + 1);
    }
    MSeFire(&fire);
}

/* One row of a shadow key sequence. */
typedef struct SdvShadowKey {
    short frame;            /* +0x00 */
    short flags;            /* +0x02 SDV_KEY_SOURCE / SDV_KEY_TARGET pick the actor */
    signed char shadowCount; /* +0x04 */
    signed char state;      /* +0x05 */
    short velocity;         /* +0x06 tenths */
    short shadowValue;      /* +0x08 hundredths */
    short angle;            /* +0x0A hundredths */
    short shadowValue3;     /* +0x0C hundredths */
} SdvShadowKey;

/* Steps a shadow key sequence and copies the row it reaches into the shadow fields of the
 * request's source or target actor. */
void sdvExecShadowSeq(SdvSequence *sequence)
{
    int index;
    SefRequest *request;
    SdvShadowKey *key;
    struct XenoActor *actor;
    int flags;
    int state;
    int shadowCount;
    int shadowValue3Hundredths;
    int velocityTenths;
    int shadowValueHundredths;
    int angleHundredths;
    float velocity;
    float shadowValue;
    float angle;
    float shadowValue3;

    if (sequence == 0 || sequence->keys == 0) {
        return;
    }
    index = sdvExecSeqTbl(sequence);
    if ((unsigned int)index >= SDV_KEY_END) {
        return;
    }
    request = (SefRequest *)sefGetBattleData();
    actor = 0;
    key = (SdvShadowKey *)(sequence->keys + index * sequence->stride);
    /* The row is read first and scaled afterwards, shadowValue3 last: scaling each halfword as it
     * is read makes gcc issue the angle product before the shadowValue one. */
    shadowValue3Hundredths = key->shadowValue3;
    velocityTenths = key->velocity;
    shadowValueHundredths = key->shadowValue;
    angleHundredths = key->angle;
    flags = key->flags;
    state = key->state;
    shadowCount = key->shadowCount;
    velocity = velocityTenths * 0.1f;
    shadowValue = shadowValueHundredths * 0.01f;
    angle = angleHundredths * 0.01f;
    shadowValue3 = shadowValue3Hundredths * 0.01f;
    if (flags & SDV_KEY_TARGET) {
        actor = request->target;
    } else if (flags & SDV_KEY_SOURCE) {
        actor = request->source;
    }
    if (actor != 0) {
        actor->sweepPhase = angle;
        actor->clipPlaneValue = shadowValue;
        actor->sweepSpeed = velocity;
        actor->clipPlaneCount = shadowCount;
        actor->clipPlaneValue3 = shadowValue3;
        actor->moveState = state;
    }
}

/* One wind slot: the effect key and sub-key that own it and the kind of its wind (the wind key
 * table's kind: 0 directional, else point). Four slots, the same count as the directional and
 * point wind tables of TiWindBase. */
typedef struct TiWindSlot {
    int key;
    unsigned short subkey;
    short windKind;
} TiWindSlot;

static TiWindSlot tiWindSlots[4];

/* Stores key, subkey and windKind in the first empty wind slot (key 0) and returns
 * its index, or -1 when all four slots are in use. */
int tiWindAcquireSlot(int key, unsigned short subkey, int windKind)
{
    TiWindSlot *slot;
    int i;

    slot = tiWindSlots;
    for (i = 0; i < 4; i++) {
        if (slot->key == 0) {
            slot->key = key;
            slot->subkey = subkey;
            slot->windKind = windKind;
            return i;
        }
        slot++;
    }
    return -1;
}

/* Releases every wind slot owned by key and subkey (key 0 matches all slots):
 * each one is disabled and cleared. */
void tiWindReleaseSlots(int key, int subkey)
{
    TiWindSlot *slot;
    int i;

    slot = tiWindSlots;
    for (i = 0; i < 4; i++) {
        if (key == 0 || (slot->key == key && slot->subkey == (subkey & 0xFFFF))) {
            tiWindDirSetEnabled(i, 0);
            tiWindPointSetEnabled(i, 0);
            slot->key = 0;
            slot->subkey = 0;
            slot->windKind = 0;
        }
        slot++;
    }
}

/* Index of the wind slot whose key and subkey match, or -1. */
int tiWindFindSlot(int key, unsigned short subkey)
{
    TiWindSlot *slot;
    int i;

    slot = tiWindSlots;
    for (i = 0; i < 4; i++) {
        if (slot->key == key && slot->subkey == subkey) {
            return i;
        }
        slot++;
    }
    return -1;
}

void *memset(void *destination, int value, unsigned int size);

/* Clears the first wind slot only (sizeof of one entry). */
void tiWindClearFirstSlot(void)
{
    memset(tiWindSlots, 0, sizeof(tiWindSlots[0]));
}

/* Releases all wind slots and clears the first one. */
void tiWindResetSlots(void)
{
    tiWindReleaseSlots(0, 0);
    tiWindClearFirstSlot();
}

/* One row of a wind key table (11 halfwords). The two halfwords before the first row are the
 * table's header: keys[-2] the speed and keys[-1] the wind kind (0 directional, else point).
 * Lengths are in hundredths. */
typedef struct TiWindKey {
    short frame;            /* +0x00 */
    short positionKind;     /* +0x02 sefGetPosition kind of the offset */
    short offset[3];        /* +0x04 */
    short rangeUpper;       /* +0x0A phase bounds */
    short rangeLower;       /* +0x0C */
    short radius;           /* +0x0E negative: global wind; 0 or TIWIND_RADIUS_OFF and beyond: wind off */
    short direction[3];     /* +0x10 directional wind only */
} TiWindKey;

#define TIWIND_RADIUS_OFF 20000


/* One length of the key reached: the row's value, moved towards the next key by t (in 256ths of
 * the interval) when the two keys differ (SDV_BLEND of sdvExecSpecial in X1). */
#define TIWIND_BLEND(member) \
    frame_key->member = (next->member == row->member ? row->member \
        : (row->member + (((next->member - row->member) * t + 0x80) >> 8)) & 0xFFFF)

/* Steps a wind key sequence and applies the key it reaches (interpolated towards the next key)
 * to the wind slot of key and subkey, acquiring one when needed: the point or directional wind
 * of the table's kind is placed, given its speed, range and radius and enabled; a radius of 0
 * or TIWIND_RADIUS_OFF and beyond disables it and releases the slot.
 * Two plain forms change the output: offset as a bare AlignedVector4 lets gcc load and convert
 * the y and z halfwords before the x store (hence SdvVectorView), and a bare
 * `if (radius != 0 && radius < TIWIND_RADIUS_OFF)` gives slti/movz instead of the enabled flag's
 * movn (declared per branch: one function-scope flag gave other registers). */
void tiWindUpdateKeyframes(SdvSequence *sequence, int key, unsigned short subkey)
{
    TiWindKey current;
    SdvVectorView offset;
    AlignedVector4 position;
    TiWindKey *frame_key;
    TiWindKey *row;
    TiWindKey *next;
    int index;
    int found;
    int kind;
    int speed;
    int elapsed;
    int frame;
    int nextFrame;
    int t;
    int slot;
    int radius;

    if (sequence == 0 || sequence->keys == 0) {
        return;
    }
    index = sdvExecSeqTbl(sequence);
    if (index < 0) {
        index = sequence->position - 1;
    }
    if (index < 0) {
        index = 0;
    }
    if (index == SDV_KEY_END) {
        return;
    }
    found = 0;
    frame_key = &current;
    row = (TiWindKey *)(sequence->keys + index * sequence->stride);
    next = row + 1;
    elapsed = sequence->elapsed;
    speed = sequence->keys[-2];
    kind = sequence->keys[-1];
    frame = row->frame;
    if (frame != SDV_KEY_END && elapsed >= frame) {
        nextFrame = next->frame;
        if (nextFrame >= SDV_KEY_END) {
            frame_key = row;
            found = 1;
        } else if (frame != nextFrame) {
            t = ((elapsed - frame) << 8) / (nextFrame - frame);
            TIWIND_BLEND(offset[0]);
            TIWIND_BLEND(offset[1]);
            TIWIND_BLEND(offset[2]);
            TIWIND_BLEND(radius);
            TIWIND_BLEND(rangeUpper);
            TIWIND_BLEND(rangeLower);
            TIWIND_BLEND(direction[0]);
            TIWIND_BLEND(direction[1]);
            TIWIND_BLEND(direction[2]);
            found = 1;
            frame_key->positionKind = row->positionKind;
        }
    }
    if (found == 0) {
        return;
    }
    slot = tiWindFindSlot(key, subkey);
    if (slot < 0) {
        slot = tiWindAcquireSlot(key, subkey, kind);
        if (slot < 0) {
            return;
        }
    }
    offset.vector.x = frame_key->offset[0] * 0.01f;
    offset.vector.y = frame_key->offset[1] * 0.01f;
    offset.vector.z = frame_key->offset[2] * 0.01f;
    offset.vector.w = 1.0f;
    sefGetPosition(&position, &offset.vector, frame_key->positionKind);
    if (kind != 0) {
        int enabled;

        tiWindDirSetEnabled(slot, 0);
        tiWindPointSetPosition(slot, &position.x);
        tiWindPointSetSpeed(slot, speed * 0.01f);
        tiWindPointSetRange(slot, frame_key->rangeUpper * 0.01f, frame_key->rangeLower * 0.01f);
        enabled = 0;
        radius = frame_key->radius;
        if (radius != 0 && radius < TIWIND_RADIUS_OFF) {
            enabled = 1;
        }
        if (enabled) {
            if (radius < 0) {
                tiWindPointSetRadius(slot, 0.0f, 1);
            } else {
                tiWindPointSetRadius(slot, radius * 0.01f, 0);
            }
            tiWindPointSetEnabled(slot, 1);
        } else {
            tiWindPointSetEnabled(slot, 0);
            tiWindReleaseSlots(key, subkey);
        }
    } else {
        int enabled;

        tiWindPointSetEnabled(slot, 0);
        tiWindDirSetPosition(slot, &position.x);
        offset.vector.x = frame_key->direction[0] * 0.01f;
        offset.vector.y = frame_key->direction[1] * 0.01f;
        offset.vector.z = frame_key->direction[2] * 0.01f;
        offset.vector.w = 1.0f;
        MMathNormalizeVector2(&offset.vector, &offset.vector);
        tiWindDirSetDirection(slot, offset.vector.x, offset.vector.y, offset.vector.z);
        tiWindDirSetSpeed(slot, speed * 0.01f);
        tiWindDirSetRange(slot, frame_key->rangeUpper * 0.01f, frame_key->rangeLower * 0.01f);
        enabled = 0;
        radius = frame_key->radius;
        if (radius != 0 && radius < TIWIND_RADIUS_OFF) {
            enabled = 1;
        }
        if (enabled) {
            if (radius < 0) {
                tiWindDirSetRadius(slot, 0.0f, 1);
            } else {
                tiWindDirSetRadius(slot, radius * 0.01f, 0);
            }
            tiWindDirSetEnabled(slot, 1);
        } else {
            tiWindDirSetEnabled(slot, 0);
            tiWindReleaseSlots(key, subkey);
        }
    }
}

static short _sdvSpecialBuf[8];

/* A packed RGBA colour; the channels are written one by one as bit-fields. */
typedef union SdvColor {
    struct {
        unsigned int red : 8;
        unsigned int green : 8;
        unsigned int blue : 8;
        unsigned int alpha : 8;
    } channel;
    int word;
} SdvColor;

/* One key of a special table (26 bytes). */
typedef struct SdvSpecialKey {
    unsigned short frame;     /* +0x00 */
    unsigned short depth;     /* +0x02 */
    unsigned char color[4];   /* +0x04 */
    short x;                  /* +0x08 */
    short y;                  /* +0x0A */
    short z;                  /* +0x0C */
    short width;              /* +0x0E */
    short height;             /* +0x10 */
    short subWindow2;         /* +0x12 sub-window 2 value (kind 11: x0.005 into sub-window 3) */
    short subWindow3;         /* +0x14 sub-window 3 value (kind 11: x0.005 into sub-window 1) */
    short intensity;          /* +0x16 */
    unsigned short decrease;  /* +0x18 */
} SdvSpecialKey;

/* A key viewed as raw bytes: the char member makes the key copies alias every type. */
typedef union SdvKeyBytes {
    SdvSpecialKey key;
    char byte[sizeof(SdvSpecialKey)];
} SdvKeyBytes;

/* A special table: the kind in the low byte of the first halfword, a signed argument in its high byte. */
typedef struct SdvSpecialTable {
    unsigned short kind;      /* +0x00 */
    short part;               /* +0x02 */
    unsigned short modes;     /* +0x04 sdvSpecialFlags turns it into the work flags */
    SdvKeyBytes keys[1];      /* +0x06 */
} SdvSpecialTable;

/* One special record of sdvSpecialTable (32 of them, indexed by special slot). */
typedef struct SdvSpecialWork {
    SdvSpecialTable *table;   /* +0x00 */
    SdvSpecialKey key;        /* +0x04 the interpolated key */
    short frame;              /* +0x1E */
    short keyIndex;           /* +0x20 */
    unsigned char flags;      /* +0x22 bits 0-1 alpha type, bit 4 fbp, bit 6 mode set */
    signed char decreaseWait; /* +0x23 */
    unsigned short clip;      /* +0x24 */
    unsigned char unmodeled_26[10];
    Matrix4 matrix;           /* +0x30 */
} SdvSpecialWork;

static SdvSpecialWork sdvSpecialTable[32];


int rand(void);

/* Picks a camera of table by weighted chance (weights in percent) and loads its four tracks; table holds five
   entries, all five are read, but only the first four can be picked. */
int sdvSelectCamera(SdvCameraSchedule *schedule, int base, unsigned short *table)
{
    int weights[5];
    int cameras[5];
    int roll;
    int camera;
    int total;
    int i;

    camera = 0;
    roll = rand() % 100;
    if (table == 0 || base == 0) {
        return 0;
    }
    for (i = 0; i < 5; i++) {
        cameras[i] = scGetImmAdrImmIdx2(base, table, i * 2);
        weights[i] = scGetImmNumIdx((short *)table, i * 2 + 1);
    }
    total = 0;
    for (i = 0; i < 4; i++) {
        total += weights[i];
        if (roll < total && cameras[i] != 0) {
            camera = cameras[i];
            break;
        }
    }
    if (camera != 0) {
        schedule->channels[0].keys = (short *)scGetImmAdrImmIdx2(base, (unsigned short *)camera, 0);
        schedule->channels[1].keys = (short *)scGetImmAdrImmIdx2(base, (unsigned short *)camera, 1);
        schedule->channels[2].keys = (short *)scGetImmAdrImmIdx2(base, (unsigned short *)camera, 2);
        schedule->channels[3].keys = (short *)scGetImmAdrImmIdx2(base, (unsigned short *)camera, 3);
    }
    schedule->camera = camera;
    schedule->animation = 0;
    return camera;
}

/* A camera point request: sefGetPosition resolves offset (relative to object) into point. */
typedef struct SdvCameraParam {
    Vector4 point;      /* +0x00 */
    /* SdvVectorView: with a plain Vector4 member gcc hoists the three placement loads of sdvSetCameraParam above
       the offset stores; the original keeps load, store, load, store. */
    SdvVectorView offset; /* +0x10 */
    int object;         /* +0x20 */
    int part;           /* +0x24 */
    unsigned char unmodeled_28[0x30 - 0x28];
} SdvCameraParam;

/* Returns its second argument. kind is int: declared short, a sll/sra pair extends it before the call. */

/* Builds a camera point from a key, relative to the object that kind selects: the request's kind1Key
   (SDV_KEY_SOURCE), kind0Key (SDV_KEY_TARGET) or kind5Key (SDV_KEY_OBJECT1). With mode 1 placement is an orbit
   (pitch and yaw in degrees, distance in hundredths), otherwise an x, y, z offset in hundredths. */
void sdvSetCameraParam(SdvCameraParam *camera, int kind, short *placement, int scaled, int mode)
{
    Vector4 orbit;
    Vector4 vector;
    SefLinkedContext *request;
    Vector4 *offset;
    float scale;
    float radians;
    int vectorKind;

    vectorKind = -1;
    request = sefGetBattleData();
    scale = 1.0f;
    memset(camera, 0, sizeof(*camera));
    if (scaled != 0) {
        sefInitPartTransform(&sefGlobalTransform, kind, request->kind1Key, request->kind0Key,
                             request->kind5Key);
        if (sefGlobalTransform.scaled != 0) {
            scale = sefGlobalTransform.scale;
        }
    }
    mode &= 0xF;
    camera->offset.vector.x = placement[0];
    camera->offset.vector.y = placement[1];
    camera->offset.vector.z = placement[2];
    camera->offset.vector.w = 1.0f;
    if (mode == 1) {
        /* orbit = (0, 0, distance, 1) turned by yaw and pitch through the current matrix (vf26-vf29) */
        __asm__ __volatile__("sqc2 vf0, 0(%1)" : "=m"(orbit) : "r"(&orbit));
        orbit.z = camera->offset.vector.z * 0.01f;
        radians = 0.017453292f;
        vu0SetCurrentRotationY(camera->offset.vector.y * radians);
        vu0RotateCurrentX(camera->offset.vector.x * radians);
        __asm__ __volatile__("lqc2 vf1, 0(%0)" : : "r"(&orbit) : "memory");
        __asm__ __volatile__(
            "vmulax.xyzw ACC, vf26, vf1x\n"
            "vmadday.xyzw ACC, vf27, vf1y\n"
            "vmaddaz.xyzw ACC, vf28, vf1z\n"
            "vmaddw.xyzw vf20, vf29, vf1w\n"
            : : : "memory");
        offset = &camera->offset.vector;
        __asm__ __volatile__("sqc2 vf20, 0(%0)" : : "r"(offset) : "memory");
    } else {
        offset = &camera->offset.vector;
        Vu0SefScaleVectorXYZNI(offset, offset, 0.01f);
    }
    if ((kind & SDV_KEY_SOURCE) && kind != SDV_KEY_SOURCE) {
        camera->object = request->kind1Key;
        camera->part = kind - (SDV_KEY_SOURCE + 1);
    } else if ((kind & SDV_KEY_TARGET) && kind != SDV_KEY_TARGET) {
        camera->object = request->kind0Key;
        camera->part = kind - (SDV_KEY_TARGET + 1);
    } else if ((kind & SDV_KEY_OBJECT1) && kind != SDV_KEY_OBJECT1) {
        camera->object = request->kind5Key;
        camera->part = kind - (SDV_KEY_OBJECT1 + 1);
    } else if (kind == SDV_KEY_SOURCE) {
        camera->object = request->kind1Key;
        camera->part = 1;
        kind = SDV_KEY_SOURCE + 2;
        vectorKind = 1;
    } else if (kind == SDV_KEY_TARGET) {
        camera->object = request->kind0Key;
        camera->part = 1;
        kind = SDV_KEY_TARGET + 2;
        vectorKind = 0;
    } else if (kind == SDV_KEY_OBJECT1) {
        camera->object = request->kind5Key;
        camera->part = 1;
        kind = SDV_KEY_OBJECT1 + 2;
        vectorKind = 5;
    }
    if (vectorKind >= 0) {
        sefGetKindVector(&vector, camera->object, 1, vectorKind);
        if (vector.y != 0.0f) {
            camera->offset.vector.y -= 1.3f;
        } else {
            camera->offset.vector.y -= 7.8f;
        }
    }
    Vu0SefScaleVectorXYZNI(offset, offset, scale);
    sefGetPosition(&camera->point, offset, kind);
}

/* Applies the slots, then clears CAMERA_FLAG_SLOTS_APPLIED so the next camera update re-blends.
   The camera address is taken before the call (kept in $s0); a bare `gameWork.camera.flags &= ...` rebuilds it
   after the call. */
void sdvApplyCameraSlots(int slot_mask, int blend_mode, int ease_mode, int frames)
{
    GameWorkCamera *camera = &gameWork.camera;

    cameraApplySlots(slot_mask, blend_mode, ease_mode, frames);
    camera->flags &= ~CAMERA_FLAG_SLOTS_APPLIED;
}

/* Wraps angle (radians) into [0, 2 pi) by subtracting the whole turns its degrees hold. */
float sdvWrapAngle(float angle)
{
    float scale = 57.29578f;    /* degrees per radian */
    float range = 6.2831855f;   /* one turn */
    int quotient;
    float wrapped;

    quotient = (int)(angle * scale) / 360;
    wrapped = angle - (float)quotient * range;
    if (wrapped < 0.0f) {
        wrapped += range;
    }
    return wrapped;
}

/* Returns angle moved by whole turns towards current (radians): the other way round is taken when the whole
   degrees of the difference exceed 180. */
float sdvNearestAngle(float angle, float current)
{
    float wrappedCurrent = sdvWrapAngle(current);
    float delta = sdvWrapAngle(angle) - wrappedCurrent;
    float degrees = __builtin_fabs(delta) * 57.29578f;

    if (degrees == 0.0f) {
        return current;
    }
    if ((int)degrees % 360 > 180) {
        /* the other way round is shorter */
        if (delta > 0.0f) {
            return current - (6.2831855f - degrees * 0.017453292f);
        }
        return current + (6.2831855f - degrees * 0.017453292f);
    }
    return current + delta;
}


/* The track kinds sdvScheduleCamera passes to sdvProgressPrm. */
#define SDV_TRACK_POSITION 0
#define SDV_TRACK_LOOK_AT 1
#define SDV_TRACK_LENS 3
#define SDV_TRACK_SHAKE 0x41

/* The second halfword of a camera key: the ease and blend modes of the slot transition from this key to the
   next; the request log copies it whole (the union: a struct copy comes out as two byte moves,
   the original copies the halfword). */
typedef union SdvKeyModes {
    short word;
    struct {
        unsigned char ease;             /* +0x02: the ease mode cameraSlotSetup stores */
        signed char blend;              /* +0x03: the blend mode; nonzero also drives the angles from the positions */
    } mode;
} SdvKeyModes;

/* Bit of a camera key's mode: the look-at follows the object (gameWork.camera.range.anchorIndex). */
#define SDV_CAMERA_MODE_ANCHOR 0x10

/* A key of the look-at (kind 1) and position (kind 0) tracks: 8 halfwords. Every track's key starts with its frame. */
typedef struct SdvCameraKey {
    short frame;                        /* +0x00: the key's frame, or a hit wait (SDV_KEY_WAIT_HIT) */
    SdvKeyModes modes;                  /* +0x02 */
    short mode;                         /* +0x04: sdvSetCameraParam's mode, plus SDV_CAMERA_MODE_ANCHOR */
    short scaled;                       /* +0x06 */
    short placement[3];                 /* +0x08: sdvSetCameraParam's placement */
    short kind;                         /* +0x0E: the object and part the camera orbits */
} SdvCameraKey;

/* A key of the perspective and roll track (kind 3): 4 halfwords, both angles in degrees. */
typedef struct SdvLensKey {
    short frame;
    SdvKeyModes modes;
    short perspective;                  /* +0x04 */
    short roll;                         /* +0x06 */
} SdvLensKey;

/* A key of the shake track (kind 0x41): 6 halfwords; a negative mask leaves the waves as they are. */
typedef struct SdvShakeKey {
    short frame;
    short waveMask;                     /* +0x02: bits 0-2 select the waves; its low byte becomes shakeFlags */
    short amplitude;                    /* +0x04: degrees */
    short period;                       /* +0x06 */
    short hold;                         /* +0x08 */
    short cycles;                       /* +0x0A */
} SdvShakeKey;

/* The last look-at and position keys applied: kind, scale flag and modes of each. */
typedef struct SdvCameraKeyLog {
    short lookAtKind;                   /* +0x00 */
    short lookAtScaled;                 /* +0x02 */
    short positionKind;                 /* +0x04 */
    short positionScaled;               /* +0x06 */
    SdvKeyModes lookAtModes;            /* +0x08 */
    SdvKeyModes positionModes;          /* +0x0A */
    unsigned char unmodeled_0C[0x28 - 0x0C];
} SdvCameraKeyLog;

extern SdvCameraKeyLog sdvCameraKeyLog;
/* Minus the distance from the camera position to its look-at point. */
extern float sdvCameraDistance;

/* Distance between two points (m_math.c). */

/* Advances one track to frame: applies the key frame reaches (look-at, position, perspective and roll, or the
   shake waves), then starts the slot transition to the next key; when the key after next is the end marker,
   keyIndex is stepped onto it early (sdvProgressKey leaves it there).
   Returns the key index, SDV_KEY_END when the track is over.
   The key after next is an integer sum with the key size first: with `(short *)next + keySize` (or
   `(unsigned char *)next + keyBytes`) gcc emits `addu $2,$20,$3` for the original's `addu $2,$3,$20`. */
int sdvProgressPrm(int kind, SdvScheduleChannel *channel, int keySize, int frame)
{
    SdvCameraParam param;
    Vector4 angles;
    GameWorkCamera *camera;
    short *keys = channel->keys;
    SdvCameraKey *key;
    int keyBytes;                       /* a key's size in bytes */
    SdvCameraKey *next;
    SdvCameraKey *after;
    SdvLensKey *lensKey;
    SdvLensKey *lensNext;
    SdvShakeKey *shakeKey;
    unsigned int index;
    int remaining;                      /* frames from key to next */
    signed char blend;                  /* key's blend mode while applying it */
    signed char blendMode;              /* key's blend and ease modes, read again for the slot transition */
    unsigned char easeMode;
    CameraWave *wave;
    float amplitude;                    /* the shake key's amplitude in radians */
    int bit;
    int i;

    index = sdvProgressKey(keys, &channel->keyIndex, keySize, frame);
    if (index < SDV_KEY_END) {
        camera = &gameWork.camera;
        key = (SdvCameraKey *)&keys[index * keySize];
        keyBytes = keySize * sizeof(short);
        next = (SdvCameraKey *)((unsigned char *)key + keyBytes);
        remaining = next->frame - key->frame;
        if (key->frame & SDV_KEY_WAIT_HIT) {
            if ((next->frame & SDV_KEY_WAIT_HIT) == 0) {
                remaining = next->frame - frame + 1;
            } else {
                remaining = 0;
            }
        }
        if (kind == SDV_TRACK_SHAKE) {
            shakeKey = (SdvShakeKey *)key;
            if (shakeKey->waveMask >= 0) {
                camera->shakeFlags = shakeKey->waveMask;
                for (i = 0; i < 3; i++) {
                    bit = 1 << i;
                    if (shakeKey->waveMask & bit) {
                        if (bit & 1) {
                            wave = &camera->waves[0];
                        } else if (bit & 2) {
                            wave = &camera->waves[1];
                        } else if (bit & 4) {
                            wave = &camera->waves[2];
                        } else {
                            wave = 0;
                        }
                        if (wave != 0) {
                            amplitude = (float)shakeKey->amplitude * 0.017453292f * 0.05f;
                            wave->period = shakeKey->period;
                            wave->hold = shakeKey->hold;
                            wave->remaining = shakeKey->cycles;
                            wave->value = amplitude;
                            if (shakeKey->cycles != 0) {
                                wave->step = amplitude / (float)shakeKey->cycles;
                            } else {
                                wave->step = 0.0f;
                            }
                        }
                    }
                }
            }
        } else {
            blend = key->modes.mode.blend;
            if (kind == SDV_TRACK_LOOK_AT) {
                if (index == 0) {
                    /* the first look-at key resets the camera when the studio camera's state is 4, then restores
                       that state */
                    XglCamera *studio = xglStudioGetActiveCamera();
                    unsigned int state = studio->state;

                    if (state == 4) {
                        cameraInit();
                    }
                    studio->state = state;
                }
                sdvCameraKeyLog.lookAtKind = key->kind;
                sdvCameraKeyLog.lookAtScaled = key->scaled;
                sdvCameraKeyLog.lookAtModes.word = key->modes.word;
                sdvSetCameraParam(&param, key->kind, key->placement, key->scaled, key->mode);
                camera->slot[0].duration = 0.0f;
                camera->slot[0].progress = 0.0f;
                camera->slot[0].fraction = 0.0f;
                if ((key->mode & SDV_CAMERA_MODE_ANCHOR) && param.object != 0) {
                    camera->range.anchorIndex = param.object;
                    camera->range.anchorSubIndex = param.part;
                    camera->range.lookAt.x = param.point.x;
                    camera->range.lookAt.y = param.point.y;
                    camera->range.lookAt.z = param.point.z;
                    camera->range.offset.x = param.offset.vector.x;
                    camera->range.offset.y = param.offset.vector.y;
                    camera->range.offset.z = param.offset.vector.z;
                    camera->range.offset.w = 1.0f;
                    camera->to.lookAt.x = param.point.x;
                    camera->to.lookAt.y = param.point.y;
                    camera->to.lookAt.z = param.point.z;
                    camera->fromOffset.x = param.offset.vector.x;
                    camera->fromOffset.y = param.offset.vector.y;
                    camera->fromOffset.z = param.offset.vector.z;
                    camera->fromOffset.w = 1.0f;
                } else {
                    camera->range.anchorIndex = -1;
                    camera->range.lookAt.x = param.point.x;
                    camera->range.lookAt.y = param.point.y;
                    camera->range.lookAt.z = param.point.z;
                    camera->to.lookAt.x = param.point.x;
                    camera->to.lookAt.y = param.point.y;
                    camera->to.lookAt.z = param.point.z;
                }
            } else if (kind == SDV_TRACK_POSITION) {
                sdvCameraKeyLog.positionKind = key->kind;
                sdvCameraKeyLog.positionScaled = key->scaled;
                sdvCameraKeyLog.positionModes.word = key->modes.word;
                sdvSetCameraParam(&param, key->kind, key->placement, key->scaled, key->mode);
                camera->slot[1].duration = 0.0f;
                camera->slot[1].progress = 0.0f;
                camera->range.cameraPosition.x = param.point.x;
                camera->range.cameraPosition.y = param.point.y;
                camera->range.cameraPosition.z = param.point.z;
                camera->slot[1].fraction = 0.0f;
                sdvCameraDistance = -MMathCalcDist(&camera->range.cameraPosition, &camera->range.lookAt);
                if (blend != 0) {
                    cameraAnglesBetween(&angles, &camera->range.lookAt, &camera->range.cameraPosition);
                    camera->range.angles.x = angles.x;
                    camera->range.angles.y = angles.y;
                    camera->range.angles.w = angles.w;
                } else {
                    camera->range.angles.x = 0.0f;
                    camera->range.angles.y = 0.0f;
                    camera->range.angles.w = 0.0f;
                }
            } else if (kind == SDV_TRACK_LENS) {
                lensKey = (SdvLensKey *)key;
                camera->slot[2].duration = 0.0f;
                camera->slot[2].progress = 0.0f;
                camera->slot[2].fraction = 0.0f;
                camera->slot[3].duration = 0.0f;
                camera->slot[3].progress = 0.0f;
                camera->slot[3].fraction = 0.0f;
                camera->range.cameraPosition.w = (float)lensKey->perspective * 0.017453292f;
                camera->range.lookAt.w = (float)lensKey->roll * 0.017453292f;
            }
            if (remaining >= 2 && next->frame < SDV_KEY_END) {
                blendMode = key->modes.mode.blend;
                easeMode = key->modes.mode.ease;
                if (kind == SDV_TRACK_LOOK_AT) {
                    sdvSetCameraParam(&param, next->kind, next->placement, key->scaled, next->mode);
                    if ((next->mode & SDV_CAMERA_MODE_ANCHOR) && param.object != 0) {
                        camera->to.lookAt.x = param.point.x;
                        camera->to.lookAt.y = param.point.y;
                        camera->to.lookAt.z = param.point.z;
                        camera->fromOffset.x = param.offset.vector.x;
                        camera->fromOffset.y = param.offset.vector.y;
                        camera->fromOffset.z = param.offset.vector.z;
                        camera->fromOffset.w = 1.0f;
                    } else {
                        camera->to.lookAt.x = param.point.x;
                        camera->to.lookAt.y = param.point.y;
                        camera->to.lookAt.z = param.point.z;
                        camera->range.anchorIndex = -1;
                        sdvApplyCameraSlots(1, blendMode, easeMode, remaining);
                    }
                } else if (kind == SDV_TRACK_POSITION) {
                    sdvSetCameraParam(&param, next->kind, next->placement, key->scaled, next->mode);
                    camera->to.position.x = param.point.x;
                    camera->to.position.y = param.point.y;
                    camera->to.position.z = param.point.z;
                    if (blendMode != 0) {
                        cameraAnglesBetween(&angles, &camera->to.lookAt, &camera->to.position);
                        camera->range.angles.x = sdvWrapAngle(camera->range.angles.x);
                        camera->range.angles.y = sdvWrapAngle(camera->range.angles.y);
                        camera->to.angles.x = sdvNearestAngle(angles.x, camera->range.angles.x);
                        camera->to.angles.y = sdvNearestAngle(angles.y, camera->range.angles.y);
                        camera->to.angles.w = angles.w;
                    } else {
                        camera->to.angles.x = 0.0f;
                        camera->to.angles.y = 0.0f;
                        camera->to.angles.w = 0.0f;
                    }
                    sdvApplyCameraSlots(2, blendMode, easeMode, remaining);
                } else if (kind == SDV_TRACK_LENS) {
                    lensKey = (SdvLensKey *)key;
                    lensNext = (SdvLensKey *)next;
                    if (lensNext->perspective != lensKey->perspective) {
                        camera->to.position.w = (float)lensNext->perspective * 0.017453292f;
                        sdvApplyCameraSlots(4, blendMode, easeMode, remaining);
                    }
                    if (lensNext->roll != lensKey->roll) {
                        camera->to.lookAt.w = (float)lensNext->roll * 0.017453292f;
                        sdvApplyCameraSlots(8, blendMode, easeMode, remaining);
                    }
                }
                after = (SdvCameraKey *)(keyBytes + (int)next);
                if (after->frame == SDV_KEY_END) {
                    /* the key after next is the end marker: step onto it now */
                    channel->keyIndex++;
                }
            }
        }
    }
    return index;
}

extern s16 _sefBattleMode;

/* Advances the four tracks of a schedule to frame; returns how many of the look-at and position tracks applied
   their first key (key index 0) on this frame. */
int sdvScheduleCamera(SdvCameraSchedule *schedule, int frame)
{
    int started = 0;

    if (_sefBattleMode == 0) {
        return 0;
    }
    if (schedule == 0 || schedule->camera == 0) {
        return 0;
    }
    if (schedule->channels[1].keys != 0) {
        if (sdvProgressPrm(SDV_TRACK_LOOK_AT, &schedule->channels[1], 8, frame) == 0) {
            started++;
        }
    }
    if (schedule->channels[0].keys != 0) {
        if (sdvProgressPrm(SDV_TRACK_POSITION, &schedule->channels[0], 8, frame) == 0) {
            started++;
        }
    }
    if (schedule->channels[2].keys != 0) {
        sdvProgressPrm(SDV_TRACK_LENS, &schedule->channels[2], 4, frame);
    }
    if (schedule->channels[3].keys != 0) {
        sdvProgressPrm(SDV_TRACK_SHAKE, &schedule->channels[3], 6, frame);
    }
    return started;
}

/* (0, 0, 0, 1): the point sdvTransOffset projects and the offset sdvExecSpecial asks sefGetPoint for. */
const AlignedVector4 sdvOriginPoint = { 0.0f, 0.0f, 0.0f, 1.0f };

/* Vector4Elements for the scale vectors, which the VU0 code also stores as a whole quadword. sdvDrawSpecial indexes it as
   component[] and passes vector to the helpers; as a plain Vector4 gcc schedules the scale stores after the part loads before the two
   sdvTransOffset calls (fndiff 674/684 in place), and sdvTransOffset reloads the screen position after
   its extent stores. */

/*
 * Forms of the quadword.h / vu0_macro.h macros that also name the memory they read as an "m" operand, as
 * sef.c's sefAddVectorXYZ does ("m"(*second)). The instructions are the plain macros'; the operand gives the
 * source pointer the weight the original register allocation shows (allocator weight device; each function says
 * what it changes). Users: sdvTransOffset, sdvCreateAlter, sdvSlerpVector and sdvExecAlter.
 */
/* *destination = *source through the 128-bit $8 (QuadwordCopyT0). */
#define SdvCopyQuadword(destination, source) \
    __asm__ __volatile__("lq $8, 0(%1)\nsq $8, 0(%0)\n" \
                         : : "r" (destination), "r" (source), "m" (*(source)) : "$8", "memory")
/* destination = (float) source, all four lanes (Vu0SefIntToFloatVector). */
#define SdvIntToFloatVector(destination, source) \
    __asm__ __volatile__("lqc2 vf1, 0(%1)\nvitof0.xyzw vf1, vf1\nsqc2 vf1, 0(%0)\n" \
                         : : "r" (destination), "r" (source), "m" (*(source)) : "memory")
/* destination.xyz = first.xyz * second.xyz, w keeps first's (Vu0SefMulVectorXYZ). */
#define SdvMulVectorXYZ(destination, first, second) \
    __asm__ __volatile__("lqc2 vf1, 0(%1)\nlqc2 vf2, 0(%2)\nvmul.xyz vf1, vf1, vf2\nsqc2 vf1, 0(%0)\n" \
                         : : "r" (destination), "r" (first), "r" (second), "m" (*(second)) : "memory")
/* The translation row (row 3, +48) of a matrix as a point with w = 1 from vf0 (Vu0MMathSetTranslation of vu0_macro.h
   stores one back). */
#define SdvGetTranslation(destination, matrix) \
    __asm__ __volatile__("lqc2 vf1, 48(%1)\nvmove.w vf1, vf0\nsqc2 vf1, 0(%0)\n" \
                         : : "r" (destination), "r" (matrix), "m" ((*(matrix))[3][0]) : "memory")

/* Part kinds with this bit take their position from the matrix the caller passes (when it passes one) instead of from
   sefGetPosition. */
#define SDV_PART_USES_MATRIX 0x40

/*
 * Projects offset (hundredths, relative to the part, or to matrix when the part has bit 0x40) to the screen:
 * screen gets the fixed-point position minus the screen origin and the depth in z, scale (a point on input)
 * its projected extent from the position. Part 8 is already a screen position. Returns 0 when clipped.
 * Memory operands: SdvCopyQuadword and SdvIntToFloatVector put offset in $16 ahead of &local, SdvGetTranslation
 * puts matrix in $19 ahead of request (the plain macros: fndiff 36/187 in place).
 */
int sdvTransOffset(int *screen, int *offset, int part, int depth, Vector4Elements *scale, int argument, Matrix4 *matrix)
{
    Vector4 point;
    Vector4 local;
    Matrix4 cameraMatrix;
    Matrix4 projection;
    int edge[4];
    XglCamera *camera;
    SefLinkedContext *request;
    Matrix4 *view;
    int useMatrix;
    int deltaX;
    int deltaY;

    if (part == 8) {
        SdvCopyQuadword(screen, offset);
        screen[2] = depth << 8;
        screen[0] += 0x100;
        screen[1] += 0xE0;
        return 1;
    }
    camera = xglStudioGetActiveCamera();
    request = sefGetBattleData();
    view = &camera->volume[0].viewMatrix;
    useMatrix = 0;
    if (matrix != 0 && (part & SDV_PART_USES_MATRIX)) {
        useMatrix = 1;
    }
    SdvIntToFloatVector(&local, offset);
    Vu0SefScaleVectorXYZNI(&local, &local, 0.01f);
    local.w = 1.0f;
    sefCalcInvView(&cameraMatrix);
    MMathMulMatrix(&projection, view, &cameraMatrix);
    if (useMatrix) {
        MMathApplyMatrix(&point, matrix, &local);
    } else {
        sefGetPosition(&point, &local, part);
    }
    if (argument != 0) {
        sefInitPartTransform(&sefGlobalTransform, part, request->kind1Key, request->kind0Key,
                             request->kind5Key);
        if (useMatrix) {
            SdvGetTranslation(&sefGlobalTransform.position, matrix);
            Vu0MMathSetTranslation(&sefGlobalTransform.matrix, &sefGlobalTransform.position);
        }
        if (sefGlobalTransform.scaled != 0) {
            MMathMulMatrix(&cameraMatrix, view, &sefGlobalTransform.matrix);
            MMathScaleMatrixRotS(&projection, &projection, sefGlobalTransform.scale);
            Vu0SefSubVectorXYZ(&point, &point, &sefGlobalTransform.position);
            view = &cameraMatrix;
        }
    }
    MMathApplyMatrix((Vector4 *)projection[3], view, &point);
    if (MMathRotTransPersClip((Vector4 *)screen, camera, &projection, &sdvOriginPoint) != 0) {
        return 0;
    }
    MMathRotTransPersClip((Vector4 *)edge, camera, &projection, &scale->vector);
    deltaX = screen[0] - edge[0];
    scale->component[0] = (deltaX < 0 ? -deltaX : deltaX) >> 4;
    deltaY = screen[1] - edge[1];
    scale->component[1] = (deltaY < 0 ? -deltaY : deltaY) >> 4;
    screen[0] = (screen[0] >> 4) - 0x700;
    screen[1] = (screen[1] >> 4) - 0x720;
    if (depth != 0) {
        screen[2] += depth << 8;
        if (screen[2] > 0xF00000) {
            screen[2] = 0xF00000;
        }
    }
    return 1;
}

/* Reset: zeroes the slot flags and every special record, then gifStateClearModeHook. */
void sdvResetSpecialWork(void)
{
    memset(_sdvSpecialBuf, 0, sizeof(_sdvSpecialBuf));
    memset(sdvSpecialTable, 0, sizeof(sdvSpecialTable));
    gifStateClearModeHook();
}

void sdvClearSpecialWork(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (_sdvSpecialBuf[i] != 0) {
            GameDefocusSet(i, 0, 0);
            _sdvSpecialBuf[i] = 0;
        }
    }
}

int sdvAllocSpecialWork(int kind)
{
    int slot;

    if (kind == 0) {
        if (_sdvSpecialBuf[0] == 0) {
            _sdvSpecialBuf[0] = 1;
            return 0;
        }
        return -1;
    }

    for (slot = 1; slot < 8; slot++) {
        if (_sdvSpecialBuf[slot] == 0) {
            _sdvSpecialBuf[slot] = 1;
            return slot;
        }
    }
    return -1;
}

/* Finds a free special record (index 1..31); 0 when none is free. */
int sdvFindFreeSpecialWork(void)
{
    int slot;

    for (slot = 1; slot < 0x20; slot++) {
        if (sdvSpecialTable[slot].table == 0) {
            return slot;
        }
    }
    return 0;
}

/* Clears the record of one special slot; slot 0 is kept. */
void sdvClearSpecialRecord(int slot)
{
    if (slot > 0) {
        memset(&sdvSpecialTable[slot], 0, sizeof(SdvSpecialWork));
    }
}

/* Bits of SdvSpecialWork.flags. */
#define SDV_FLAG_ALPHA_TYPE 0x03
#define SDV_FLAG_FBP_SHIFT 4
#define SDV_FLAG_MODE_SET_SHIFT 6
#define SDV_FLAG_MODE_SET (1 << SDV_FLAG_MODE_SET_SHIFT)


/* The number of slots a special lists. */
#define SDV_SPECIAL_SLOT_COUNT 6

/* Kinds from this one on are screen effects (kinds 10-15); they interpolate all four size halfwords of a key. */
#define SDV_SPECIAL_KIND_SCREEN 10
#define SDV_SPECIAL_SCREEN_KINDS 6

/* Frees the special slots an owner lists; a screen-effect record (kinds 10-15) with no key decrease, or of
   kind 13, also clears the gifState mode value. */
void sdvFreeSpecialSlots(SdvSpecial *special)
{
    int i;
    int slot;
    SdvSpecialWork *record;
    unsigned int kind;
    signed char *position;

    if (special == 0 || special->sequence.keys == 0) {
        return;
    }
    /* The first row test reads the position through its address: read as special->sequence.position, the compiler
       shares this load with the one that ends each pass of the loop, which the original reloads every time. */
    position = &special->sequence.position;
    i = 0;
    if (*position <= 0) {
        return;
    }
    do {
        slot = special->slotIds[i];
        if (slot > 0) {
            record = &sdvSpecialTable[slot];
            special->slotIds[i] = 0;
            if (record->table != 0) {
                kind = record->table->kind & 0xFF;
                if (kind - SDV_SPECIAL_KIND_SCREEN < SDV_SPECIAL_SCREEN_KINDS) {
                    if (record->key.decrease == 0 || kind == 13) {
                        gifStateClearModeHook();
                    }
                }
                record->table = 0;
                record->flags &= ~SDV_FLAG_MODE_SET;
                sdvClearSpecialRecord(slot);
            }
        }
        i++;
        if (i >= special->sequence.position) {
            break;
        }
    } while (i < SDV_SPECIAL_SLOT_COUNT);
}


/* One step of a key interpolation: from + (to - from) * blend / 256, rounded; from itself when both are equal. */
#define SDV_LERP(from, to, blend) ((to) == (from) ? (from) : ((((to) - (from)) * (blend) + 0x80) >> 8) + (from))

/* The SdvSpecialWork.flags of a table: bits 0-3 are modes bits 0-3 ORed with bits 4-7, bits 4-7 are modes bits 8-11. */
static inline int sdvSpecialFlags(short modes)
{
    return (modes >> 4) | (modes & 0xF);
}

/* Runs one special: every row its sequence reaches takes a free special record (the row names the record's
   table), then the key of each slot's record is interpolated and its frame counted. A key followed by a frame
   above SDV_KEY_END frees the record once its frame is passed; a following SDV_KEY_END holds the key. */
void sdvExecSpecial(SdvSpecial *special, int base)
{
    AlignedVector4 point;
    SdvSpecialWork *record;
    SdvSpecialTable *table;
    SdvKeyBytes *keys;
    SdvSpecialKey *key;
    SdvSpecialKey *next;
    Matrix4 *matrix;
    unsigned int step;
    int kind;
    int blend;
    int i;
    int j;
    short *fromSize;
    short *toSize;
    short *size;

    if (special == 0 || special->sequence.keys == 0) {
        return;
    }
    step = sdvExecSeqTbl(&special->sequence);
    while (step < SDV_KEY_END) {
        for (i = 0; i < SDV_SPECIAL_SLOT_COUNT; i++) {
            if (special->slotIds[i] <= 0 && (special->slotIds[i] = sdvFindFreeSpecialWork()) > 0) {
                record = &sdvSpecialTable[special->slotIds[i]];
                record->table = (SdvSpecialTable *) scGetImmAdrImmIdx2(
                    base, (unsigned short *) (special->sequence.keys + step * special->sequence.stride), 1);
                record->frame = 0;
                record->keyIndex = 0;
                record->flags = sdvSpecialFlags(record->table->modes);
                record->decreaseWait = 1;
                record->clip = 0;
                if (record->table != 0) {
                    matrix = sefGetPoint(&point, &sdvOriginPoint, record->table->part);
                    if (matrix != 0) {
                        record->clip = 1;
                        QuadwordCopyMatrixT0(record->matrix, matrix);
                    } else {
                        record->clip = 0;
                        Vu0CopyUnitMatrix(&record->matrix, &MMathUnitMatrix);
                    }
                }
                break;
            }
        }
        if (i == SDV_SPECIAL_SLOT_COUNT) {
            break;
        }
        step = sdvCheckSeqKey(&special->sequence);
    }

    for (i = 0; i < special->sequence.position && i < SDV_SPECIAL_SLOT_COUNT; i++) {
        record = &sdvSpecialTable[special->slotIds[i]];
        table = record->table;
        if (table == 0) {
            continue;
        }
        kind = table->kind & 0xFF;
        keys = table->keys;
        key = &keys[record->keyIndex].key;
        next = key + 1;
        if (key->frame != SDV_KEY_END && record->frame >= key->frame) {
            if (record->frame > key->frame && next->frame > SDV_KEY_END) {
                if (kind >= SDV_SPECIAL_KIND_SCREEN && kind < SDV_SPECIAL_KIND_SCREEN + 6
                    && record->key.decrease == 0) {
                    gifStateClearModeHook();
                }
                record->table = 0;
                record->flags &= ~SDV_FLAG_MODE_SET;
                sdvClearSpecialRecord(special->slotIds[i]);
                special->slotIds[i] = 0;
                continue;
            }
            if (next->frame >= SDV_KEY_END) {
                record->key = *key;
                record->decreaseWait = 1;
            } else if (key->frame != next->frame) {
                blend = ((record->frame - key->frame) << 8) / (next->frame - key->frame);
                record->key.depth = SDV_LERP(key->depth, next->depth, blend);
                record->key.color[0] = SDV_LERP(key->color[0], next->color[0], blend);
                record->key.color[1] = SDV_LERP(key->color[1], next->color[1], blend);
                record->key.color[2] = SDV_LERP(key->color[2], next->color[2], blend);
                record->key.color[3] = SDV_LERP(key->color[3], next->color[3], blend);
                record->key.x = SDV_LERP(key->x, next->x, blend);
                record->key.y = SDV_LERP(key->y, next->y, blend);
                /* z moves toward the next key's intensity (as the original does) once the two z differ. */
                record->key.z = next->z == key->z
                    ? key->z
                    : (((next->intensity - key->z) * blend + 0x80) >> 8) + key->z;
                record->key.intensity = SDV_LERP(key->intensity, next->intensity, blend);
                record->key.decrease = key->decrease;
                if (record->decreaseWait < 0) {
                    record->decreaseWait = 1;
                }
                if (kind >= SDV_SPECIAL_KIND_SCREEN) {
                    fromSize = &key->width;
                    toSize = &next->width;
                    size = &record->key.width;
                    for (j = 0; j < 4; j++) {
                        size[j] = SDV_LERP(fromSize[j], toSize[j], blend);
                    }
                } else {
                    record->key.width = SDV_LERP(key->width, next->width, blend);
                    record->key.height = SDV_LERP(key->height, next->height, blend);
                }
            }
        }
        if (record->frame < SDV_KEY_END) {
            record->frame++;
            if (record->frame >= next->frame) {
                record->decreaseWait = -1;
                record->keyIndex++;
            }
        }
    }
}

/* sefActiveOwnerKey value while a battle runs: the clip matrix is used and boxes stay square. */
#define SDV_BATTLE_MODE 0x1012F003

/* The 512x448 screen in pixels. */
#define SDV_SCREEN_WIDTH 0x200
#define SDV_SCREEN_HEIGHT 0x1C0

/* Depth of a kind-6 key without a depth: the far plane. */
#define SDV_DEPTH_FAR 0xFFFFF8

extern int sefActiveOwnerKey;
extern short sdvDefocusSizeSteps[9];


#define SDV_SET_COLOR(rgba, source) \
    ((rgba).channel.red = (source)->color[0], \
     (rgba).channel.green = (source)->color[1], \
     (rgba).channel.blue = (source)->color[2], \
     (rgba).channel.alpha = (source)->color[3], (rgba).word)


/* Normalizes a vector in place. */
static inline void *sdvNormalize(Vector4 *vector)
{
    return MMathNormalizeVector2(vector, vector);
}

/* Draws the slots of one special: sdvTransOffset per slot, GameDefocusSet when allocated. */
void sdvDrawSpecial(SdvSpecial *special)
{
    int defocusParams[16];
    Vector4Elements scale;
    int offset[4];
    int screen[4];
    int slotIndex;

    for (slotIndex = 0; slotIndex < special->sequence.position && slotIndex < SDV_SPECIAL_SLOT_COUNT; slotIndex++) {
        SdvSpecialWork *record = &sdvSpecialTable[special->slotIds[slotIndex]];
        SdvSpecialTable *table = record->table;
        SdvSpecialKey *key;
        SdvKeyBytes *nextKey;
        SdvKeyBytes *keys;
        int inBattle;
        Matrix4 *matrix;
        int kind;
        int argument;
        SdvColor color;
        int defocusSlot;
        int overshoot;
        int sizeStep;
        unsigned int i;
        Vector4Elements *sizeVector;
        int depth;

        if (table == 0) {
            continue;
        }
        kind = table->kind & 0xFF;
        argument = (short)table->kind >> 8;
        keys = table->keys;
        nextKey = &keys[record->keyIndex];
        inBattle = sefActiveOwnerKey == SDV_BATTLE_MODE;
        if (record->clip != 0 && inBattle) {
            matrix = &record->matrix;
        } else {
            matrix = 0;
        }
        defocusParams[0] = 0;
        key = &record->key;
        if (record->frame < nextKey->key.frame) {
            continue;
        }
        switch (kind) {
        case 7: /* box drawn at the projected point, sized by width/height */
            if (key->color[3] == 0) {
                break;
            }
            sizeVector = &scale;
            offset[0] = key->x;
            offset[1] = key->y;
            offset[2] = key->z;
            /* sizeVector = (0, 0, 0, 1): VU0 vf0 stored through the pointer */
            __asm__ __volatile__("sqc2 vf0, 0(%1)" : "=m"(*sizeVector) : "r"(sizeVector));
            scale.component[0] = key->width * 0.01;
            scale.component[1] = key->height * 0.01;
            if (sdvTransOffset(screen, offset, table->part, key->depth, sizeVector, argument, matrix) == 0) {
                break;
            }
            if (screen[0] < 0 || screen[1] < 0 || screen[0] >= SDV_SCREEN_WIDTH || screen[1] >= SDV_SCREEN_HEIGHT) {
                break;
            }
            defocusParams[1] = 1;
            defocusParams[0] = 0;
            if (sdvAllocSpecialWork(0) >= 0) {
                GameDefocusSet(0, 2, defocusParams);
            }
            SDV_SET_COLOR(color, key);
            defocusParams[0] = screen[2];
            defocusParams[1] = color.word;
            defocusParams[2] = screen[0];
            defocusParams[3] = screen[1];
            if (record->table->part == 8) {
                defocusParams[4] = key->width;
                defocusParams[5] = key->height;
            } else {
                defocusParams[4] = scale.component[0];
                defocusParams[5] = scale.component[1];
                if (record->clip != 0) {
                    /* keep the box inside the screen */
                    overshoot = defocusParams[2] - defocusParams[4];
                    if (overshoot < 0) {
                        defocusParams[4] += overshoot;
                    }
                    overshoot = defocusParams[2] + defocusParams[4] - SDV_SCREEN_WIDTH;
                    if (overshoot > 0) {
                        defocusParams[4] -= overshoot;
                    }
                    overshoot = defocusParams[3] - defocusParams[5];
                    if (overshoot < 0) {
                        defocusParams[5] += overshoot;
                    }
                    overshoot = defocusParams[3] + defocusParams[5] - SDV_SCREEN_HEIGHT;
                    if (overshoot > 0) {
                        defocusParams[5] -= overshoot;
                    }
                    if (sefActiveOwnerKey == SDV_BATTLE_MODE) {
                        if ((unsigned int)defocusParams[4] < (unsigned int)defocusParams[5]) {
                            defocusParams[5] = defocusParams[4];
                        }
                        if ((unsigned int)defocusParams[5] < (unsigned int)defocusParams[4]) {
                            defocusParams[4] = defocusParams[5];
                        }
                    }
                }
            }
            defocusParams[6] = key->intensity;
            defocusSlot = sdvAllocSpecialWork(1);
            if (defocusSlot > 0) {
                GameDefocusSet(defocusSlot, 7, defocusParams);
            }
            break;
        case 6:
            if (key->color[3] == 0) {
                break;
            }
            defocusParams[1] = 1;
            defocusParams[0] = 0;
            if (sdvAllocSpecialWork(0) >= 0) {
                GameDefocusSet(0, 2, defocusParams);
            }
            depth = key->depth != 0 ? key->depth << 8 : SDV_DEPTH_FAR;
            SDV_SET_COLOR(color, key);
            defocusParams[0] = depth;
            defocusParams[2] = key->x;
            defocusParams[3] = key->y;
            defocusParams[4] = key->width;
            defocusParams[5] = key->height;
            defocusParams[1] = color.word;
            defocusSlot = sdvAllocSpecialWork(1);
            if (defocusSlot > 0) {
                GameDefocusSet(defocusSlot, 6, defocusParams);
            }
            break;
        case 5:
            if (key->color[3] == 0) {
                break;
            }
            defocusParams[0] = key->depth << 8;
            SDV_SET_COLOR(color, key);
            sizeStep = 1;
            defocusParams[1] = color.word;
            defocusParams[2] = key->x;
            defocusParams[3] = key->y;
            defocusParams[4] = key->width;
            defocusParams[5] = key->height;
            for (i = 0; i < 9; i++) {
                if (sdvDefocusSizeSteps[i] >= key->intensity) {
                    sizeStep = sdvDefocusSizeSteps[i];
                    break;
                }
            }
            defocusParams[6] = sizeStep;
            defocusParams[7] = sizeStep;
            defocusSlot = sdvAllocSpecialWork(1);
            if (defocusSlot > 0) {
                GameDefocusSet(defocusSlot, 5, defocusParams);
            }
            break;
        case 3:
            if (key->color[3] == 0) {
                break;
            }
            GameDefocusSet(0, 2, defocusParams);
            defocusParams[0] = key->intensity;
            SDV_SET_COLOR(color, key);
            defocusParams[2] = key->x;
            defocusParams[1] = color.word;
            defocusParams[3] = key->y;
            defocusParams[4] = key->width;
            defocusParams[5] = key->height;
            defocusSlot = sdvAllocSpecialWork(1);
            if (defocusSlot > 0) {
                GameDefocusSet(defocusSlot, 3, defocusParams);
            }
            break;
        case 1:
            if (key->color[3] == 0) {
                break;
            }
            defocusParams[0] = 0;
            defocusParams[3] = SDV_SET_COLOR(color, key);
            defocusParams[1] = key->intensity;
            defocusParams[2] = key->depth << 8;
            defocusParams[4] = key->x;
            defocusParams[5] = key->y;
            defocusParams[6] = key->width;
            defocusParams[7] = key->height;
            defocusSlot = sdvAllocSpecialWork(1);
            if (defocusSlot > 0) {
                GameDefocusSet(defocusSlot, 1, defocusParams);
            }
            break;
        case 4:
            if (key->color[3] == 0) {
                break;
            }
            defocusParams[0] = 1;
            defocusParams[3] = SDV_SET_COLOR(color, key);
            defocusParams[4] = 0;
            defocusParams[5] = 0;
            defocusParams[6] = 0;
            defocusParams[7] = 0;
            defocusParams[1] = 1;
            defocusParams[2] = key->depth << 8;
            defocusSlot = sdvAllocSpecialWork(1);
            if (defocusSlot > 0) {
                GameDefocusSet(defocusSlot, 1, defocusParams);
            }
            break;
        case 9:
            defocusSlot = sdvAllocSpecialWork(1);
            if (defocusSlot > 0) {
                GameDefocusSet(defocusSlot, 9, defocusParams);
            }
            break;
        /* kinds 10-14: screen effects, handed to gifState (kind 10 + mode) */
        case SDV_SPECIAL_KIND_SCREEN:
        case SDV_SPECIAL_KIND_SCREEN + 1:
        case SDV_SPECIAL_KIND_SCREEN + 2:
        case SDV_SPECIAL_KIND_SCREEN + 3:
        case SDV_SPECIAL_KIND_SCREEN + 4: {
            unsigned char modeSet;
            Vector4Elements *vector = &scale;

            offset[0] = key->x;
            offset[1] = key->y;
            offset[2] = key->z;
            Vu0SetOrigin(vector);
            scale.component[0] = key->height * 0.005f;
            scale.component[1] = key->subWindow2 * 0.005f;
            modeSet = (record->flags >> SDV_FLAG_MODE_SET_SHIFT) & 1;
            if (record->decreaseWait > 0 && (modeSet == 0 || key->decrease != 0)) {
                gifStateSetDecrease(key->decrease * (1.0f / 256.0f));
                record->decreaseWait = 0;
            }
            if (modeSet == 0) {
                gifStateSetMode(kind - SDV_SPECIAL_KIND_SCREEN);
                record->flags |= SDV_FLAG_MODE_SET;
            }
            if (sdvTransOffset(screen, offset, record->table->part, key->depth, vector, argument, 0) == 0) {
                break;
            }
            gifStateSetCenter(screen[0], screen[1]);
            gifStateSetEffectAlpha(record->flags & SDV_FLAG_ALPHA_TYPE, key->color[3]);
            gifStateSetFbp((record->flags >> SDV_FLAG_FBP_SHIFT) & 1);
            gifStateSetBlur(key->intensity);
            gifStateSetDepth(screen[2]);
            nmlModelUseSubWindow(0, key->width);
            if (kind == 11 && record->table->part != 8) {
                nmlModelUseSubWindow(2, scale.component[0]);
                nmlModelUseSubWindow(3, scale.component[1]);
                scale.component[0] = key->subWindow3 * 0.005f;
                sdvTransOffset(screen, offset, record->table->part, key->depth, vector, argument, 0);
                nmlModelUseSubWindow(1, scale.component[0]);
            } else {
                nmlModelUseSubWindow(1, key->height);
                nmlModelUseSubWindow(2, key->subWindow2);
                nmlModelUseSubWindow(3, key->subWindow3);
            }
            if (kind == 12) {
                signed char *direction = (signed char *)key->color;

                scale.component[0] = direction[0];
                scale.component[1] = direction[1];
                scale.component[2] = direction[2];
                scale.component[3] = 1.0f;
                sdvNormalize(&scale.vector);
                sefApplyPartTransform(&vector->vector, &vector->vector, record->table->part);
                nmlModelSetMulColor(&scale.vector);
            }
            break;
        }
        }
    }
}


/*
 * The 0x58-byte request an alter is created from: sdvScheduleAlter fills it from an event, sdvCreateAlter
 * copies it into the record at +0xAD0 and sdvExecAlter/sdvDrawAlter read it there.
 */
typedef struct SdvAlterRequest {
    /* +0x00 GS TEX0 of the two images, low word first. Kept as words: the record is 4-aligned (sdvCreateAlter's
       copy tests both pointers' alignment at run time); sdvScheduleAlter stores them through SdvAlterTex0View */
    unsigned int tex0[2][2];
    SdvSefRequest *sefRequest;    /* +0x10 the seven words sdvExecAlter copies into the sef request buffer */
    unsigned char rgba[4];        /* +0x14 */
    unsigned char unmodeled_18[4];
    float length;                 /* +0x1C progress added over maxSamples points (each frame length / maxSamples) */
    float imageTime;              /* +0x20 frames the current image has been shown */
    float imageFrames;            /* +0x24 frames each image is shown */
    float life;                   /* +0x28 frames left; 2048 never runs out */
    float ringSpacing;            /* +0x2C: distance between the rings of a follow trail */
    float height;                 /* +0x30 */
    float minDelta;               /* +0x34 a shorter move keeps the previous delta */
    short parts[2];               /* +0x38 sefGetPosition kinds; with follow set, parts[1] is the heading turn
                                     rate in 1/100 */
    short maxSamples;             /* +0x3C points the record grows to: at most 64, 32 with follow set */
    unsigned short shrink;        /* +0x3E frames taken per frame once the life is spent */
    short blend;                  /* +0x40 */
    short imageCount;             /* +0x42 */
    short mode;                   /* +0x44 */
    short image;                  /* +0x46 the image drawn, 0..imageCount-1 */
    short layer;                  /* +0x48 sdvDrawAlters draws the records of one layer */
    short owner;                  /* +0x4A index of the owning effect record; negative for none */
    short follow;                 /* +0x4C nonzero: the record trails the part of parts[0] */
    short spread;                 /* +0x4E */
    short spin;                   /* +0x50 degrees the point angles turn per frame */
    short timeBase;               /* +0x52 */
    short timeStep;               /* +0x54 */
    unsigned char unmodeled_56[2];
} SdvAlterRequest;

/* The two TEX0 words of a request as the 64-bit values the GS takes (the record itself is 4-aligned). */
typedef struct SdvAlterTex0View {
    unsigned long long tex0[2];
} SdvAlterTex0View;

/* One alter event row of a sequence (the keys of an SdvSequence read as a record). */
typedef struct SdvAlterEvent {
    unsigned short frame;         /* +0x00 the frame the row starts at (SDV_KEY_ rules) */
    short life;                   /* +0x02 */
    unsigned short mode;          /* +0x04 low nibble the mode, the high 12 bits (signed) the follow flag */
    unsigned short blend;         /* +0x06 */
    unsigned short rgba[4];       /* +0x08 the low byte of each */
    short parts[2];               /* +0x10 */
    short maxSamples;             /* +0x14 */
    unsigned short shrink;        /* +0x16 */
    short imageFrames;            /* +0x18 */
    short length;                 /* +0x1A */
    short images[2][2];           /* +0x1C the image and the palette index of each pair */
    float ringSpacing;            /* +0x24 */
    float height;                 /* +0x28 */
    unsigned short spread;        /* +0x2C */
    unsigned short spin;          /* +0x2E */
    unsigned short timeBase;      /* +0x30 */
    unsigned short timeStep;      /* +0x32 */
    short minDelta;               /* +0x34 in 1/1000 */
} SdvAlterEvent;

/* An alter record keeps its points in rings of 64 (32 while it trails a part); the mode of an event is its low four
   bits. */
#define SDV_ALTER_POINT_COUNT 64
#define SDV_ALTER_RING_COUNT 32
#define SDV_ALTER_MODE_MASK 0xF

/* The parts kinds whose alters fan out over a table of linked members. */
#define SDV_PART_LINKED_A 0x61
#define SDV_PART_LINKED_B 0x62

/* sef.c declares it without a prototype; sdvScheduleAlter passes a member index (part kind - 0x41). */
int sdvCreateAlter(SdvAlterRequest *request);
/* Zero-terminated lists of the part kinds that parts kinds SDV_PART_LINKED_A and SDV_PART_LINKED_B fan out to. */
extern unsigned short sdvLinkedPartsA[];
extern unsigned short sdvLinkedPartsB[];

/* Builds an alter request from each event the sequence steps onto, then creates it through sdvCreateAlter. */
void sdvScheduleAlter(SdvSequence *sequence, SdvSefRequest *sefRequest, int layer, int recordIndex)
{
    SdvAlterRequest request;
    SdvAlterEvent *event;
    SvTypeListEntry *image;
    SvTypeListEntry *palette;
    unsigned short *parts;
    int step;
    int i;
    int partIndex;
    short imageIndex;
    short paletteIndex;
    int psm;
    unsigned short part;

    if (sequence == 0) {
        return;
    }
    if (sequence->keys == 0) {
        return;
    }
    for (step = sdvExecSeqTbl(sequence); (unsigned int)step < SDV_KEY_END; step = sdvCheckSeqKey(sequence)) {
        event = (SdvAlterEvent *)&sequence->keys[step * sequence->stride];
        memset(&request, 0, sizeof(request));
        request.life = event->life;
        request.parts[0] = event->parts[0];
        request.sefRequest = sefRequest;
        request.parts[1] = event->parts[1];
        request.maxSamples = event->maxSamples > SDV_ALTER_POINT_COUNT ? SDV_ALTER_POINT_COUNT : event->maxSamples;
        request.shrink = event->shrink;
        request.mode = event->mode & SDV_ALTER_MODE_MASK;
        request.follow = (short)event->mode >> 4;
        request.blend = event->blend;
        request.rgba[0] = event->rgba[0];
        request.rgba[1] = event->rgba[1];
        request.rgba[2] = event->rgba[2];
        request.rgba[3] = event->rgba[3];
        request.imageFrames = event->imageFrames;
        request.length = event->length;
        request.layer = layer;
        request.owner = recordIndex;
        request.spread = event->spread;
        request.ringSpacing = event->ringSpacing;
        request.spin = event->spin;
        request.height = event->height;
        request.timeBase = event->timeBase;
        request.timeStep = event->timeStep;
        request.minDelta = event->minDelta * 0.001f;
        if (request.follow != 0 && request.maxSamples > SDV_ALTER_RING_COUNT) {
            request.maxSamples = SDV_ALTER_RING_COUNT;
        }
        for (i = 0; i < 2; i++) {
            imageIndex = event->images[i][0];
            paletteIndex = event->images[i][1];
            if (imageIndex >= 0 && paletteIndex >= 0) {
                image = svGetImageListItem(layer, imageIndex);
                palette = svGetImageListItem(layer, paletteIndex);
                if (image != 0 && palette != 0) {
                    psm = image->bitsPerPixel == 8 ? GS_PSMT8 : GS_PSMT4;
                    ((SdvAlterTex0View *)&request)->tex0[request.imageCount] = GS_SET_TEX0(image->basePointer,
                        image->bufferWidthPixels >> 6, psm, image->textureWidthExponent,
                        image->textureHeightExponent, 1, 0, palette->basePointer, 0, 0, 0, 1);
                    request.imageCount++;
                    /* Stored again here although the layer is set above: without this store the image loop is scheduled
                       differently (fd.sh, sdvScheduleAlter). */
                    request.layer = layer;
                }
            }
        }
        part = event->parts[0];
        if ((unsigned int)(part - SDV_PART_LINKED_A) <= SDV_PART_LINKED_B - SDV_PART_LINKED_A) {
            parts = (short)part == SDV_PART_LINKED_A ? sdvLinkedPartsA : sdvLinkedPartsB;
            for (partIndex = 0; parts[partIndex] != 0; partIndex++) {
                if (sefGetLinkedFlag(parts[partIndex] - 0x41) != 0) {
                    request.parts[0] = parts[partIndex];
                    sdvCreateAlter(&request);
                }
            }
        } else {
            sdvCreateAlter(&request);
        }
    }
}


/*
 * One alter record: 0xD40 bytes. The quadword members make the record 16-byte aligned, which is why
 * gcc addresses +0xD38 as +0xD30 + 8 and +0xB18 as +0xB10 + 8.
 */
typedef struct SdvAlter {
    Matrix4 worldMatrix;                /* +0x000 */
    Matrix4 matrix;                     /* +0x040 */
    /* +0x080 two 64-point rings, one per sef part; when the record trails a part (follow), [0] holds the
       sampled anchors and [1] the ring swung around them */
    SdvVectorView points[2][64];
    AlignedVector4 quats[33];           /* +0x880 heading quaternions, newest first */
    AlignedVector4 heading;             /* +0xA90 */
    AlignedVector4 lastDelta;           /* +0xAA0 */
    AlignedVector4 position;            /* +0xAB0 sdvCreateAlter stores the sefGetPosition result here (lq/sq) */
    AlignedVector4 previousPosition;    /* +0xAC0 */
    SdvAlterRequest request;            /* +0xAD0 */
    float texturePhases[64];            /* +0xB28: texture phase of each sample */
    float progress;                     /* +0xC28 */
    float angles[32];                   /* +0xC2C */
    unsigned char unmodeled_CAC[0xCB0 - 0xCAC];
    float radii[32];                    /* +0xCB0 */
    unsigned char unmodeled_D30[0xD34 - 0xD30];
    short frame;                        /* +0xD34 */
    unsigned short index;               /* +0xD36 the point written this frame, 0..63 */
    unsigned short active;              /* +0xD38 */
    unsigned short wrapped;             /* +0xD3A set once index has wrapped */
    short serial;                       /* +0xD3C */
    unsigned char unmodeled_D3E[0xD40 - 0xD3E];
} SdvAlter;

static SdvAlter sdvAlterTable[49];

/* Zeroes one alter record (sdvInitAlter). */
void sdvInitAlter(SdvAlter *alter)
{
    sefMemZero(alter, sizeof(*alter));
}

#define SDV_ALTER_COUNT 49
#define SDV_DEG_TO_RAD 0.017453292f
#define SDV_PI 3.1415927f
/* The noise vector sefNoiseNextVector returns is in [-0.5, 0.5): scaled to +-5 degrees around each axis. */
#define SDV_ALTER_NOISE_ANGLE 0.17453292f
/* The random radius offset of the trailing ring, in units; its timeBase/timeStep radii are in 1/1000. */
#define SDV_ALTER_RADIUS_RANDOM 0.1f
#define SDV_ALTER_RADIUS_UNIT 0.001f

extern int sdvAlterSerial;

/*
 * Takes the first free alter record, stamps a serial and copies the request in; returns index | serial << 16,
 * -1 when every record is taken. A trailing record (follow) also gets its start position, a slightly
 * noisy world matrix, its heading quaternions and the angle and radius of each ring point.
 * Memory operands: SdvMulVectorXYZ puts &sdvAlterMirrorXY in $17 ahead of alter, SdvCopyQuadword puts
 * &sdvAlterStartDelta ahead of &noise (the plain macros: fndiff 181/220 in place). The ring loop stores z through
 * SdvVectorView.
 */
int sdvCreateAlter(SdvAlterRequest *request)
{
    AlignedVector4 position;
    AlignedVector4 noise;
    SdvAlter *alter;
    int i;
    int k;
    int maxSamples;
    float radius;
    float angle;

    for (i = 0; i < SDV_ALTER_COUNT; i++) {
        if (sdvAlterTable[i].active == 0) {
            alter = &sdvAlterTable[i];
            sdvAlterSerial++;
            sdvAlterSerial &= 0xFF;
            alter->request = *request;
            alter->active = 1;
            alter->serial = sdvAlterSerial;
            alter->frame = 0;
            alter->index = 0;
            alter->progress = 0.0f;
            if (request->follow != 0) {
                maxSamples = request->maxSamples;
                radius = sefRandf() * SDV_ALTER_RADIUS_RANDOM;
                sefGetPosition(&position, 0, request->parts[0]);
                QuadwordCopyT0(&alter->position, &position);
                QuadwordCopyT0(&alter->previousPosition, &position);
                sefNoiseNextVector(&noise);
                noise.x *= SDV_ALTER_NOISE_ANGLE;
                noise.y *= SDV_ALTER_NOISE_ANGLE;
                noise.z *= SDV_ALTER_NOISE_ANGLE;
                MMathRotateMatrixXYZ(&alter->worldMatrix, 0, &noise);
                QuadwordCopyT0(&alter->heading, &sdvAlterForward);
                SdvCopyQuadword(&alter->lastDelta, &sdvAlterStartDelta);
                for (k = 0; k < SDV_ALTER_RING_COUNT; k++) {
                    SdvMulVectorXYZ(&alter->quats[k], &alter->quats[k], &sdvAlterMirrorXY);
                    MMathQuatBetweenVectors(&alter->quats[k], &alter->quats[k], &sdvAlterForward);
                }
                angle = sefRandf() * SDV_PI;
                /* maxSamples + 1 passes, as in the original: with maxSamples 64 the last one is one element past the
                   ring. */
                for (k = 0; k <= maxSamples; k++) {
                    SdvVectorView *anchor = &alter->points[0][k];

                    Vu0SetOrigin(anchor);
                    Vu0SetOrigin(&alter->points[1][k]);
                    anchor->vector.z = (float) -k * alter->request.ringSpacing;
                    alter->angles[k] = (float) (alter->request.spread / maxSamples * k) * SDV_DEG_TO_RAD + angle;
                    alter->radii[k] = radius + (float) (alter->request.timeBase + k * alter->request.timeStep)
                                               * SDV_ALTER_RADIUS_UNIT;
                }
            }
            return (sdvAlterSerial << 16) | i;
        }
    }
    return -1;
}

/* Zeroes one alter record (same code as sdvInitAlter; the callers tell them apart). */
void sdvDestroyAlter(SdvAlter *alter)
{
    sefMemZero(alter, sizeof(*alter));
}

/* Kills alter id (index | serial << 16) when it is active and the serial matches. */
void sdvKillAlter(int id)
{
    if (id >= 0) {
        int index = id & 0xFFFF;
        int serial = id >> 16;

        if (sdvAlterTable[index].active != 0 && sdvAlterTable[index].serial == serial) {
            sdvDestroyAlter(&sdvAlterTable[index]);
        }
    }
}

/*
 * Destroys every active alter record that belongs to owner; a negative owner destroys nothing.
 * With "active != 0 && owner == owner" gcc reads owner through the active pointer (bnel slots;
 * fndiff 23/34 in place); the owner test on a separate record pointer keeps the original's beq/bne (34/34).
 */
void sdvDestroyOwnerAlters(int owner)
{
    int i;

    if (owner >= 0) {
        for (i = 0; i < 49; i++) {
            if (sdvAlterTable[i].active != 0) {
                SdvAlter *alter = &sdvAlterTable[i];

                if (alter->request.owner == owner) {
                    sdvDestroyAlter(alter);
                }
            }
        }
    }
}

/* A vector read as a quadword or as a float array indexed by axis. */

float acosf(float value);

extern AlignedVector4 sdvAxisPositiveX; /* (1, 0, 0, 1) */
extern AlignedVector4 sdvAxisNegativeX; /* (-1, 0, 0, 1) */
extern int sdvSlerpNudgeCount;         /* counts the opposite-vector nudges; bit 0 picks the direction */

/* destination.xyz = first.xyz + second.xyz, w keeps first's (Vu0SefAddVectorXYZ); the outputs are the four
   components of the destination, as sef.c's SefSubVectorXYZ4 (allocator weight device of sdvSlerpVector, through
   sdvBlendVectors). */
#define SdvAddVectorXYZ4(destination, first, second) \
    __asm__ __volatile__("lqc2 vf1, 0(%4)\nlqc2 vf2, 0(%5)\nvadd.xyz vf1, vf1, vf2\nsqc2 vf1, 0(%6)\n" \
        : "=m"((destination)->x), "=m"((destination)->y), "=m"((destination)->z), "=m"((destination)->w) \
        : "r"(first), "r"(second), "r"(destination) : "memory")

/* out = normalize(first * firstWeight + second * secondWeight) on xyz. */
static inline void sdvBlendVectors(AlignedVector4 *out, AlignedVector4 *first, float firstWeight,
                                   AlignedVector4 *second, float secondWeight)
{
    AlignedVector4 firstPart;
    AlignedVector4 secondPart;

    Vu0SefScaleVectorXYZNI(&firstPart, first, firstWeight);
    Vu0SefScaleVectorXYZNI(&secondPart, second, secondWeight);
    SdvAddVectorXYZ4(out, &firstPart, &secondPart);
    MMathNormalizeVector2(out, out);
}

/* Spherical interpolation from current (t = 0) towards target (t = 1), normalized into out. Nearly parallel
 * vectors give current; nearly opposite ones first nudge a component of target by 0.002 (z for the x axes,
 * else x).
 * Memory operands: SdvAddVectorXYZ4 and SdvCopyQuadword put out, target and current in $17..$19 ahead of the
 * axis (the plain macros: fndiff 126/153 in place). */
void sdvSlerpVector(AlignedVector4 *out, AlignedVector4 *target, AlignedVector4 *current, float t)
{
    AlignedVector4Elements nudged;
    AlignedVector4 *source = target;
    float cosine;
    float angle;
    float sine;
    float targetWeight;
    float currentWeight;
    double absCosine;
    int axis;

    cosine = MMathVectorDotProduct(source, current);
    absCosine = cosine;
    if (absCosine < 0.0) {
        absCosine = -absCosine;
    }
    if (absCosine >= 0.9999f) {
        if (cosine < 0.0f) {
            short onXAxis;

            axis = 0;
            onXAxis = MMathIsVectorEqual(source, &sdvAxisPositiveX) | MMathIsVectorEqual(source, &sdvAxisNegativeX);
            QuadwordCopyT0(&nudged.vector, source);
            if (onXAxis) {
                axis = 2;
            }
            source = &nudged.vector;
            if (sdvSlerpNudgeCount & 1) {
                nudged.component[axis] += 0.002f;
            } else {
                nudged.component[axis] -= 0.002f;
            }
            cosine = MMathVectorDotProduct(source, current);
            sdvSlerpNudgeCount++;
        } else {
            SdvCopyQuadword(out, current);
            return;
        }
    }
    angle = acosf(cosine);
    sine = xglSin(angle);
    if (sine != 0.0f) {
        targetWeight = xglSin(t * angle) / sine;
        currentWeight = xglSin((1.0f - t) * angle) / sine;
    } else {
        targetWeight = t;
        currentWeight = 1.0f - targetWeight;
    }
    sdvBlendVectors(out, source, targetWeight, current, currentWeight);
}

#define SEF_RECORD_HIDDEN 0x80          /* SefEffectRecord flags (+0x44, beside sef.h's SEF_RECORD_* bits): not shown */
#define SDV_ALTER_LIFETIME_FOREVER 2048.0f


/*
 * Advances one alter record by a frame: the lifetime and frame counters, the sef request, then either the
 * trailing ring (follow) or the next point of each part; a spent record is destroyed.
 * Memory operand: SdvCopyQuadword puts &alter->position in $17 ahead of the sef request (QuadwordCopyT0:
 * fndiff 311/344 in place).
 */
void sdvExecAlter(SdvAlter *alter)
{
    AlignedVector4 delta;
    Matrix4 localMatrix;
    Matrix4 inverse;
    Vector4Elements sinFirst;
    Vector4Elements sinValue;
    SdvAlterRequest *request = &alter->request;
    AlignedVector4 *position;
    AlignedVector4 *previousPosition;
    SdvSefRequest *sefRequest;
    SdvSefRequest *buffer;
    float *height;
    float *radius;
    float *angle;
    SefEffectRecord *record;
    int hidden = 0;
    int follow = request->follow;
    int partCount;
    float length;
    int i;

    if (request->owner >= 0) {
        record = sefGetScheduler() + request->owner;
        if (record->flags & SEF_RECORD_HIDDEN) {
            hidden = 1;
        }
    }
    if (request->life != SDV_ALTER_LIFETIME_FOREVER) {
        request->life -= 1.0f;
    }
    if (hidden) {
        if (alter->frame > 0) {
            alter->frame--;
        }
    } else if (request->life > 0.0f) {
        if (alter->frame < SDV_ALTER_POINT_COUNT && alter->frame < alter->request.maxSamples) {
            alter->frame++;
        }
    } else {
        alter->frame -= alter->request.shrink;
    }
    if (alter->frame > 0) {
        sefRequest = request->sefRequest;
        buffer = (SdvSefRequest *) sefGetBattleData();
        buffer->target.actor = sefRequest->target.actor;
        buffer->params[0] = sefRequest->params[0];
        buffer->params[1] = sefRequest->params[1];
        buffer->params[2] = sefRequest->params[2];
        buffer->params[3] = sefRequest->params[3];
        buffer->params[4] = sefRequest->params[4];
        buffer->params[5] = sefRequest->params[5];
        sefLoadRequestBuffer((int) sefRequest->target.actor, sefRequest->params[0], sefRequest->params[1],
                             sefRequest->params[2], sefRequest->params[3], sefRequest->params[4],
                             sefRequest->params[5], 0);
        if (follow) {
            previousPosition = &alter->previousPosition;
            position = &alter->position;
            SdvCopyQuadword(previousPosition, position);
            sefGetPosition(position, 0, request->parts[0]);
            Vu0SefSubVectorXYZ(&delta, previousPosition, position);
            if (sefRequest->target.actor != 0) {
                sefMergeMatrixPos(&alter->matrix, &sefRequest->target.actor->localMatrix.m, position);
            }
            QuadwordCopyMatrixT0(localMatrix, alter->matrix);
            Vu0SetOrigin(localMatrix[3]);
            xglMatrixInverse((Matrix *)&inverse, (Matrix *)&localMatrix);
            MMathMulMatrix(&alter->matrix, &alter->matrix, &alter->worldMatrix);
            MMathApplyMatrix(&delta, &inverse, &delta);
            length = MMathCalcLength(&delta);
            MMathNormalizeVector2(&delta, &delta);
            if (length < alter->request.minDelta) {
                QuadwordCopyT0(&delta, &alter->lastDelta);
            } else {
                QuadwordCopyT0(&alter->lastDelta, &delta);
            }
            sdvSlerpVector(&alter->heading, &delta, &alter->heading, (float) request->parts[1] * 0.01f);
            for (i = SDV_ALTER_RING_COUNT; i > 0; i--) {
                QuadwordCopyT0(&alter->quats[i], &alter->quats[i - 1]);
            }
            Vu0SefMulVectorXYZ(&delta, &alter->heading, &sdvAlterMirrorXY);
            MMathQuatBetweenVectors(&alter->quats[0], &delta, &sdvAlterForward);
            Vu0SinNI(sinFirst.component[0], alter->angles[0]);
            length = sinFirst.component[0] * alter->radii[0];
            radius = alter->radii;
            height = &alter->points[1][0].vector.y;
            angle = alter->angles;
            for (i = 0; i < SDV_ALTER_RING_COUNT; i++, height += 4) {
                *angle += (float) -alter->request.spin * SDV_DEG_TO_RAD;
                height[-1] = 0.0f;
                height[1] = 0.0f;
                Vu0SinNI(sinValue.component[0], *angle);
                height[0] = sinValue.component[0] * *radius;
                Vu0SefAddVectorXYZ(&alter->points[1][i], &alter->points[1][i], &alter->points[0][i]);
                height[0] -= length;
                radius++;
                angle++;
            }
        } else {
            partCount = 2;
            if (request->mode == 8) {
                partCount = 1;
            }
            for (i = 0; i < partCount; i++) {
                sefGetPosition(&alter->points[i][alter->index].vector, 0, request->parts[i]);
            }
        }
        if (request->mode >= 3 && request->mode <= 7) {
            float progress = alter->progress;

            alter->progress = progress + alter->request.length / (float) alter->request.maxSamples;
            if (request->mode == 3) {
                if (alter->wrapped == 0) {
                    alter->texturePhases[alter->index] = progress;
                } else {
                    alter->texturePhases[alter->index] = 4.0f;
                }
            }
            if (request->mode == 4) {
                alter->texturePhases[alter->index] = progress;
                if (alter->progress > 10.0f) {
                    alter->progress -= 10.0f;
                }
            }
        }
        alter->index++;
        if (alter->index >= SDV_ALTER_POINT_COUNT) {
            alter->wrapped = 1;
            alter->index &= SDV_ALTER_POINT_COUNT - 1;
        }
    } else if (request->life < 0.0f) {
        sdvDestroyAlter(alter);
    }
}


void sdvPacketFlushMscnt(void)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceVif1PkAddCode(packet, 0x17000000);
}

/* A 128-bit quadword (one lq/sq), as the VIF1 packet writers take it. */
typedef unsigned int XglQuadword __attribute__((mode(TI)));

/* The header sdvDrawAlter puts in front of each strip it sends by reference: sdvSetStripHeader clears its first
   two quadwords, fills the UNPACK code, primitive count and strip flag, and ORs the primitive count into the
   loop count at +0x20 (the third quadword is not cleared). */
typedef union SdvStripHeader {
    XglQuadword quad[3];
    struct {
        unsigned int unmodeled_00[3];
        unsigned int unpack;           /* +0x0C VIF UNPACK code for the whole strip */
        unsigned int primitiveCount;   /* +0x10 */
        unsigned int strip;            /* +0x14 sdvDrawAlter passes 1 with its count / 2, else 0 with count / 3 */
        unsigned int unmodeled_18[2];
        unsigned short loopCount;      /* +0x20 ORed with the primitive count */
    } layout;
} SdvStripHeader;


/* Keeps the physical address of an EE pointer: DMA references take physical addresses. */
#define SDV_PHYSICAL_ADDRESS_MASK 0x0FFFFFFF

/*
 * Clears and fills a strip header, then references its qwc quadwords (physical address) in the current packet.
 * Masking the address inside the sceVif1PkRef call loads the mask after the calls instead of before them
 * (fndiff 44 insns, 5/48 in place; this form 48/48).
 */
void sdvSetStripHeader(SdvStripHeader *header, int unused, int qwc, int primitiveCount, int strip)
{
    XglPacket *packet;
    void *physical;

    physical = (void *)((unsigned int)header & SDV_PHYSICAL_ADDRESS_MASK);
    packet = xglPacketGetCurrent();
    sdvPacketFlushMscnt();
    header->quad[0] = 0;
    header->quad[1] = 0;
    header->layout.unpack = ((qwc - 1) << 16) | 0x6C008000;
    header->layout.primitiveCount = primitiveCount;
    header->layout.strip = strip;
    header->layout.loopCount |= primitiveCount;
    sceVif1PkRef(packet, physical, qwc, 0, 0, 0);
}

/* The TEX0 pair of the same packet. Its value is a different 64-bit type than the others (long is 64 bits in
   ee-gcc): with one type for all six registers, gcc keeps the TEX0 store ahead of the blend table load in
   sdvDrawAlter. */
typedef struct SdvGsTextureRegister {
    unsigned long data;
    unsigned long address;
} SdvGsTextureRegister;

/* The environment itself: a six-register A+D packet (TEX0, TEST, TEX1, CLAMP, ZBUF, ALPHA). */
typedef struct SdvAlterEnvironment {
    u64 giftag[2];
    SdvGsTextureRegister tex0;
    DrawStateRegister test;
    DrawStateRegister tex1;
    DrawStateRegister clamp;
    DrawStateRegister zbuf;
    DrawStateRegister alpha;
} SdvAlterEnvironment;

/* One quadword of an alter strip: a texture coordinate, a colour or a position. */
typedef union SdvAlterQuad {
    XglQuadword quad;
    float f[4];
    Vector4 vector;
    unsigned int rgba[4];
    u64 words[2];
} SdvAlterQuad;

/* A GIF tag with PRE set, EOP set and a loop count that sdvSetStripHeader fills in. */
#define SDV_GIF_TAG(prim, registerCount) \
    (((u64)(registerCount) << 60) | ((u64)(prim) << 47) | ((u64)1 << 46) | GIF_TAG_EOP)
#define SDV_PRIM_LINE_STRIP 0x4A          /* line strip, gouraud, alpha blend */
#define SDV_PRIM_TRIANGLE_STRIP 0x4C      /* triangle strip, gouraud, alpha blend */
#define SDV_PRIM_TEXTURED 0x10            /* TME */

#define SDV_ALTER_MODE_SCROLL 2
#define SDV_ALTER_MODE_REPEAT 3
#define SDV_ALTER_MODE_SINGLE 8
#define SDV_ALTER_FOLLOW_LINE 2
#define SDV_ALTER_STEPS 4                 /* each segment is drawn as four steps (five points) */
#define SDV_ALTER_SEGMENT_QUADWORDS 45     /* packet quadwords reserved and counted per segment */
#define SDV_UNCACHED 0x20000000           /* added to a packet address to write it through the uncached mirror */

extern SdvAlterEnvironment sdvAlterEnvironment;
extern unsigned char D_1B70[];
/* GS ALPHA_1 values per alter blend mode (sdvDrawAlter indexes it; sefBuildModelPacket reads [0] and [1]):
 * normal (Cs - Cd) * As + Cd, additive Cs * As + Cd, subtractive (0 - Cs) * As + Cd with FIX 0x80, and Cd alone. */
const u64 sefAlphaBlendValues[4] = {
    GS_SET_ALPHA(0, 1, 0, 1, 0),
    GS_SET_ALPHA(0, 2, 0, 1, 0),
    GS_SET_ALPHA(2, 0, 0, 1, 0x80),
    GS_SET_ALPHA(2, 2, 2, 1, 0),
};

/* Copies one float vector as a quadword through t0: the inline form of svCopyVector (sv_get_image_item.c),
   whose four outputs let gcc walk both edges with one pointer in sdvDrawAlter. */
static inline void sdvCopyVector(float *destination, float *source)
{
    __asm__ __volatile__(
        "lq $8, 0(%5)\n"
        "sq $8, 0(%4)\n"
        : "=m" (destination[0]), "=m" (destination[1]), "=m" (destination[2]), "=m" (destination[3])
        : "r" (destination), "r" (source)
        : "$8");
}

/* Whether the request draws a single line along the followed part instead of a ribbon. */
static inline int sdvAlterIsFlat(const SdvAlterRequest *request)
{
    return request->follow == SDV_ALTER_FOLLOW_LINE;
}

/* The same test on a follow value that is already loaded. */
static inline int sdvFollowIsFlat(int follow)
{
    return follow == SDV_ALTER_FOLLOW_LINE;
}

/*
 * Draws one alter into the packet from xglPacketGetCurrent: each pair of neighbouring points becomes one strip
 * of four Hermite steps (five samples), a ribbon between the two point rings (or, when the record trails a part,
 * between the turned anchors moved up and down by the height) or a single line; texture coordinate and alpha run
 * along it.
 */
void sdvDrawAlter(SdvAlter *alter)
{
    Vector4 tangents[2];
    float edges[2][10][4];
    AlignedVector4 positions[32];
    SdvAlterQuad color;
    SdvAlterQuad giftag;
    SdvAlterQuad texcoords[2];
    Matrix4 heading;
    int previous;
    int last;
    int vertexCount;
    int newest;
    SdvAlterRequest *request;
    SdvStripHeader *header;
    int total;
    int follow;
    int line;
    int current;
    XglPacket *packet;
    SdvAlterQuad *vertex;
    Vector4 *turned;
    Vector4 *points;
    void *buffer;
    float progress;
    float progressStep;
    float alpha;
    float alphaStep;
    float parameter;
    int next;
    int ring;
    int flat;
    int kind;
    int i;
    int sides;
    int side;
    int step;
    int count;

    alphaStep = 0.0f;
    request = &alter->request;
    progressStep = 0.0f;
    count = alter->frame;
    newest = alter->index - 1;
    progress = alter->progress;
    packet = xglPacketGetCurrent();
    /* one scratch holds the follow value and then the mode, as the original reuses it */
    kind = request->follow;
    flat = sdvFollowIsFlat(kind);
    total = 0;
    line = 0;
    follow = kind;
    if (flat || (kind = request->mode) == SDV_ALTER_MODE_SINGLE) {
        line = 1;
    }
    texcoords[0].f[0] = 0.0f;
    texcoords[0].f[1] = 0.0f;
    texcoords[0].f[2] = 1.0f;
    texcoords[0].f[3] = 0.0f;
    texcoords[1].quad = texcoords[0].quad;
    texcoords[1].f[1] = 1.0f;
    if (count < 2) {
        return;
    }
    buffer = nmlPacketSetAttributeAlloc16N(count * SDV_ALTER_SEGMENT_QUADWORDS);
    header = (SdvStripHeader *)((unsigned int)buffer + SDV_UNCACHED);
    if (request->mode == SDV_ALTER_MODE_SCROLL) {
        alter->progress -= alter->request.length / ((float)alter->request.maxSamples * 2.0f);
        if (alter->progress < -20.0f) {
            alter->progress += 20.0f;
        }
    }
    if (buffer == 0) {
        return;
    }
    color.rgba[0] = alter->request.rgba[0];
    color.rgba[1] = alter->request.rgba[1];
    color.rgba[2] = alter->request.rgba[2];
    alpha = alter->request.rgba[3];
    /* True after the return above, but the original tests it again (without the test the branch layout differs). */
    if (count >= 2) {
        progressStep = alpha / (float)(count - 1);
        alphaStep = progressStep * 0.25f;
        progressStep = alter->request.length / (float)alter->request.maxSamples;
        progressStep *= 0.25f;
    }
    if (request->imageCount != 0) {
        request->imageTime += 1.0f;
        if (request->imageTime >= request->imageFrames) {
            request->imageTime = 0.0f;
            request->image++;
            if (request->image >= request->imageCount) {
                request->image = 0;
            }
        }
        giftag.words[1] = GS_ST | (GS_RGBAQ << 4) | (GS_XYZ2 << 8);
        giftag.words[0] = SDV_GIF_TAG(SDV_PRIM_TRIANGLE_STRIP | SDV_PRIM_TEXTURED, 3);
    } else {
        giftag.words[1] = GS_ST | (GS_RGBAQ << 4) | (GS_XYZ2 << 8);
        giftag.words[0] = SDV_GIF_TAG(SDV_PRIM_TRIANGLE_STRIP, 3);
    }
    if (line) {
        giftag.words[1] = GS_RGBAQ | (GS_XYZ2 << 4);
        giftag.words[0] = SDV_GIF_TAG(SDV_PRIM_LINE_STRIP, 2);
    }
    /* A plain cast of the 4-aligned tex0 array: through SdvAlterTex0View the entry is addressed from the request, not
       from the alter as in the original (fd.sh: 592 instead of 594 instructions). */
    sdvAlterEnvironment.tex0.data =
        request->imageCount != 0 ? ((unsigned long long *) alter->request.tex0)[request->image] : 0;
    sdvAlterEnvironment.clamp.data = request->mode == SDV_ALTER_MODE_REPEAT ? 5 : 4;
    sdvAlterEnvironment.alpha.data = sefAlphaBlendValues[request->blend];
    vertexCount = 0;
    vertex = (SdvAlterQuad *)(header + 1);
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenUpkCode(packet, 0, 0x6C, 4, 4);
    sceVif1PkAddUpkData128N(packet, &sdvAlterEnvironment, 7);
    sceVif1PkCloseUpkCode(packet);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, 0x11000000);
    sceVif1PkAddCode(packet, ((unsigned int)D_1B70 >> 3) | 0x14000000);
    if (follow != 0) {
        for (i = 0; i < count; i++) {
            MMathQuatToMatrix((Vector4 *)heading, &alter->quats[i]);
            MMathApplyMatrix(&positions[i], &heading, (Vector4 *) &alter->points[1][i]);
        }
    }
    for (i = 0; i < count - 1; i++) {
        if (follow != 0) {
            if (i == 0) {
                previous = 0;
                current = 0;
            } else {
                previous = i - 1;
                current = i;
            }
            if (i >= count - 2) {
                next = i + 1;
                last = next;
            } else {
                last = i + 2;
                next = i + 1;
            }
        } else {
            ring = (newest - i) & (SDV_ALTER_POINT_COUNT - 1);
            if (i == 0) {
                previous = ring;
                current = ring;
            } else {
                previous = (ring + 1) & (SDV_ALTER_POINT_COUNT - 1);
                current = ring;
            }
            if (i >= count - 2) {
                next = (ring - 1) & (SDV_ALTER_POINT_COUNT - 1);
                last = next;
            } else {
                next = (ring - 1) & (SDV_ALTER_POINT_COUNT - 1);
                last = (ring - 2) & (SDV_ALTER_POINT_COUNT - 1);
            }
        }
        if (follow != 0) {
            flat = sdvAlterIsFlat(request);
            turned = positions;
            MMathCalcHermitePrm(&tangents[0], &tangents[1], &turned[previous], &turned[current], &turned[next],
                                &turned[last]);
            parameter = 0.0f;
            for (step = 0; step < SDV_ALTER_STEPS + 1; step++) {
                MMathCalcHermite(edges[0][step], &tangents[0], &tangents[1], &turned[current], &turned[next],
                                 parameter);
                sdvCopyVector(edges[1][step], edges[0][step]);
                if (!flat) {
                    edges[0][step][1] += alter->request.height;
                    edges[1][step][1] -= alter->request.height;
                }
                parameter += 0.25f;
            }
        } else {
            sides = request->mode != SDV_ALTER_MODE_SINGLE ? 2 : 1;
            for (side = 0; side < sides; side++) {
                points = (Vector4 *) alter->points[side];
                MMathCalcHermitePrm(&tangents[0], &tangents[1], &points[previous], &points[current], &points[next],
                                    &points[last]);
                parameter = 0.0f;
                for (step = 0; step < SDV_ALTER_STEPS + 1; step++) {
                    MMathCalcHermite(edges[side][step], &tangents[0], &tangents[1], &points[current],
                                     &points[next], parameter);
                    parameter += 0.25f;
                }
            }
        }
        if (request->mode >= 3 && request->mode <= 7) {
            progress = alter->texturePhases[current];
            progressStep = (alter->texturePhases[next] - progress) * 0.25f;
        }
        for (step = 0; step < SDV_ALTER_STEPS + 1; step++) {
            color.rgba[3] = (unsigned int)alpha;
            if (line) {
                vertex[0].quad = color.quad;
                if (sdvAlterIsFlat(request)) {
                    MMathApplyMatrix(&vertex[1].vector, &alter->matrix, (Vector4 *)edges[0][step]);
                } else {
                    vertex[1].quad = *(XglQuadword *)edges[0][step];
                }
                vertex += 2;
                vertexCount += 2;
            } else {
                for (side = 0; side < 2; side++) {
                    vertex[0].quad = side != 0 ? texcoords[1].quad : texcoords[0].quad;
                    vertex[0].f[0] = progress;
                    vertex[1].quad = color.quad;
                    if (follow != 0) {
                        MMathApplyMatrix(&vertex[2].vector, &alter->matrix, (Vector4 *)edges[side][step]);
                    } else {
                        vertex[2].quad = *(XglQuadword *)edges[side][step];
                    }
                    vertex += 3;
                    vertexCount += 3;
                }
            }
            if (step < SDV_ALTER_STEPS) {
                progress += progressStep;
                alpha -= alphaStep;
            }
        }
        total += SDV_ALTER_SEGMENT_QUADWORDS;
        header->quad[2] = giftag.quad;
        if (line) {
            sdvSetStripHeader(header, 0, vertexCount + 3, vertexCount / 2, 1);
        } else {
            sdvSetStripHeader(header, 0, vertexCount + 3, vertexCount / 3, 0);
        }
        header = (SdvStripHeader *)vertex;
        vertex += 3;
        vertexCount = 0;
    }
    if (total > 0) {
        sdvPacketFlushMscnt();
        sefPacketFlush();
    }
}

/* Zeroes all 49 alter records (sdvInitAlters). */
void sdvInitAlters(void)
{
    SdvAlter *alter;

    alter = sdvAlterTable;
    do {
        sdvInitAlter(alter);
        alter++;
    } while ((int) alter < (int) (sdvAlterTable + 49)); /* the original compares the addresses signed (slt) */
}

/* Zeroes all 49 alter records through sdvDestroyAlter (sdvDestroyAlters). */
void sdvDestroyAlters(void)
{
    SdvAlter *alter;

    alter = sdvAlterTable;
    do {
        sdvDestroyAlter(alter);
        alter++;
    } while ((int) alter < (int) (sdvAlterTable + 49)); /* the original compares the addresses signed (slt) */
}

/* Runs every active alter record (sdvExecAlters). */
void sdvExecAlters(void)
{
    int i;

    for (i = 0; i < 49; i++) {
        if (sdvAlterTable[i].active != 0) {
            sdvExecAlter(&sdvAlterTable[i]);
        }
    }
}


/* Draws every active alter record of one layer; the draw state is set up before the first one. */
void sdvDrawAlters(int layer)
{
    int i;
    int stateBuilt = 0;

    for (i = 0; i < 49; i++) {
        if (sdvAlterTable[i].active != 0 && sdvAlterTable[i].request.layer == layer) {
            if (!stateBuilt) {
                stateBuilt = 1;
                svLoadDrawViewMatrix();
                svBuildDrawStatePacket();
            }
            sdvDrawAlter(&sdvAlterTable[i]);
        }
    }
}

/* Draws every active alter record that belongs to owner, like sdvDrawAlters. */
void sdvDrawOwnerAlters(int owner)
{
    int i;
    int stateBuilt = 0;

    for (i = 0; i < 49; i++) {
        if (sdvAlterTable[i].active != 0 && sdvAlterTable[i].request.owner == owner) {
            if (!stateBuilt) {
                stateBuilt = 1;
                svLoadDrawViewMatrix();
                svBuildDrawStatePacket();
            }
            sdvDrawAlter(&sdvAlterTable[i]);
        }
    }
}

short sdvDefocusSizeSteps[9] = { 1, 3, 7, 15, 31, 63, 127, 255, 511 };

/* Zero-terminated lists of effect part ids walked by sdvScheduleAlter. */
unsigned short sdvLinkedPartsA[32] = {
    68, 69, 70, 71, 72, 73, 74, 75,
    76, 77, 78, 79, 80, 81, 82, 83,
    84, 85, 86, 87, 88, 89, 90, 91,
    92, 93, 94, 95, 0, 0, 0, 0
};
unsigned short sdvLinkedPartsB[20] = {
    70, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86,
    0, 0, 0, 0
};
AlignedVector4 sdvAxisPositiveX = { 1.0f, 0.0f, 0.0f, 1.0f };
AlignedVector4 sdvAxisNegativeX = { -1.0f, 0.0f, 0.0f, 1.0f };
SdvAlterEnvironment sdvAlterEnvironment = {
    { 0x1000000000008006UL, 14 },
    { 0, 6 },        /* TEX0 */
    { 0x5000D, 71 }, /* TEST */
    { 96, 20 },      /* TEX1 */
    { 5, 8 },        /* CLAMP */
    { 0x131000000UL, 78 }, /* ZBUF */
    { 72, 66 }       /* ALPHA */
};

float sdvCameraDistance = 0.0f;
int sdvAlterSerial = 0;
int sdvSlerpNudgeCount = 0;

/* One matrix row as the code writes it: single floats, a doubleword half (sd) or the whole quadword (sq). */
typedef union SefRotationRow {
    f32 element[4];
    u64 halves[2];
    XglQuadword quad;
} SefRotationRow;

/*
 * The 0xA0-byte particle of sefEntryPool has one layout per effect kind; the words every kind uses keep one
 * name in all three. sefPoolAlloc returns a particle's pool index, 0 when its search finds no free entry.
 */

/* Kinds 0 to 7: spawned by sefSpawnKinds0To2 (0 to 2), sefSpawnKinds3To4 (3 and 4) and sefSpawnKinds5To7 (5 to 7);
 * kinds 0 to 2 drift in sefUpdateBurstParticles, kinds 5 to 7 grow and fade in sefUpdateFadeOutParticles. */
typedef struct SefFadeParticle {
    SefRotationRow rotation[3]; /* +0x00: built by sefRotationZRows */
    AlignedVector4 position;    /* +0x30 */
    int red;                    /* +0x40 */
    int green;                  /* +0x44 */
    int blue;                   /* +0x48 */
    int alpha;                  /* +0x4C */
    AlignedVector4 scale;       /* +0x50: xyz the axis scales, w the particle size */
    AlignedVector4 velocity;    /* +0x60: kinds 0 to 2: the step per frame for the first 20 frames */
    f32 life;                   /* +0x70: the particle dies once age reaches it; nonzero while the entry is in use */
    f32 age;                    /* +0x74: frames lived */
    f32 verticalSpeed;          /* +0x78: added to position.y each frame by the kind 3 and 4 update; zeroed on spawn */
    f32 axisOffset;             /* +0x7C: distance along rotation[0] that sefDrawEmitter moves the drawn particle */
    AlignedVector4 target;      /* +0x80: kinds 0 to 2: the step velocity turns towards after frame 20 */
    AlignedVector4 growth;      /* +0x90: per-axis factors around 1.0, w = 1.0 */
} SefFadeParticle;

/* Kinds 8 to 19 and 22 up: they fly along velocity at speed. */
typedef struct SefDriftParticle {
    SefRotationRow rotation[3]; /* +0x00: built by sefRotationZRows or sefRandomRotateMatrix */
    AlignedVector4 position;    /* +0x30 */
    int red;                    /* +0x40 */
    int green;                  /* +0x44 */
    int blue;                   /* +0x48 */
    int alpha;                  /* +0x4C */
    AlignedVector4 scale;       /* +0x50: xyz the axis scales, w the particle size */
    AlignedVector4 velocity;    /* +0x60: unit direction of flight (or already scaled by speed, sefSpawnKinds14To16) */
    f32 life;                   /* +0x70 */
    f32 age;                    /* +0x74 */
    f32 verticalSpeed;          /* +0x78: added to position.y each frame, lowered by a constant (gravity) per frame */
    f32 axisOffset;             /* +0x7C: zeroed on spawn */
    f32 speed;                  /* +0x80: sefSpawnKinds14To16 scales the unit velocity by it */
    int landed;                 /* +0x84: set once the particle reaches the floor (sefFloorHeight), when it bounces */
    unsigned char unmodeled_88[0x90 - 0x88];
    AlignedVector4 growth;      /* +0x90 */
} SefDriftParticle;

/* Kinds 20 and 21: they turn about Z in place (sefUpdateKind20Particles, sefUpdateSpinParticles). */
typedef struct SefSpinParticle {
    SefRotationRow rotation[3]; /* +0x00: rebuilt from angle by sefRotationZRows */
    AlignedVector4 position;    /* +0x30 */
    int red;                    /* +0x40 */
    int green;                  /* +0x44 */
    int blue;                   /* +0x48 */
    int alpha;                  /* +0x4C */
    AlignedVector4 scale;       /* +0x50 */
    unsigned char unmodeled_60[0x70 - 0x60];
    f32 life;                   /* +0x70 */
    f32 age;                    /* +0x74 */
    f32 verticalSpeed;          /* +0x78: zeroed on spawn */
    /* +0x7C: the f32 axisOffset of the other kinds (sefDrawEmitter reads it for every kind), cleared on spawn. Typed
     * int: as f32, sefSpawnKind21 stores the 0.0f from $f20 (swc1) where the original stores $zero (sw). */
    int axisOffset;
    f32 angle;                  /* +0x80: degrees, 0 on spawn */
    unsigned char unmodeled_84[0x90 - 0x84];
    AlignedVector4 growth;      /* +0x90 */
} SefSpinParticle;

/* Any kind as sefPoolAlloc sees it: it tests the life word's bits (lw, beqz), zero (0.0f) for a free entry. */
typedef struct SefPoolRecord {
    u8 unmodeled_00[0x70];
    int life;                   /* +0x70 */
    u8 unmodeled_74[0xA0 - 0x74];
} SefPoolRecord;

/* One entry of sefEntryPool. The kind 8-21 spawners and the group updates reach the particle through the
 * union (a union access may alias anything, so gcc keeps the particle stores and the age reloads in the
 * original's order); sefSpawnKinds5To7 and sefUpdateSpinParticles take a pointer to their kind's member. */
typedef union SefPoolEntry {
    SefFadeParticle fade;
    SefDriftParticle drift;
    SefSpinParticle spin;
    SefPoolRecord record;
    Matrix4 matrix;             /* every kind: rotation rows 0 to 2 and the position as row 3 (sefDrawEmitter) */
} SefPoolEntry;

/* The particle pool (0x500B0 bytes, cleared by sefResetPool). Entry 0 is never handed out: sefPoolAlloc returns
 * 0 for no free entry. The SEF_POOL_RING_SIZE entries after it form a ring that the search for a free one walks. */
#define SEF_POOL_RING_SIZE 2048

typedef struct SefPool {
    SefPoolEntry entries[SEF_POOL_RING_SIZE + 1];
    int cursor;                 /* +0x500A0: where the next search starts */
} SefPool;

static Matrix4 sefViewCameraMatrix;
static SefPool sefEntryPool;

void sefResetPool(void) {
    memset(&sefEntryPool, 0, sizeof(SefPool));
}

extern unsigned char sefSmallHeap[];

void sefResetBuffers(void) {
    smInitilizeSmall(sefSmallHeap, 0x20000);
    sefResetPool();
}

/* Set when sefPoolAlloc finds no free entry; sefRunEmitters clears it every frame. */
s16 sefPoolFull = 0;

/* Returns the sefEntryPool index of a free entry (ring entry r is entries[r + 1]), searching the ring from the
 * cursor; the cursor moves to the ring entry after the one handed out. Returns 0 and sets sefPoolFull when no
 * free entry is found: the search stops at the ring entry just before the start, so it looks at 2047 of the 2048.
 * The free test reads life through SefPoolRecord: the plain `entry->fade.life == 0.0f` compiles to a float
 * compare (mtc1, lwc1, c.eq.s), the original tests the word (lw, beqz). The cursor is stored through poolCursor:
 * with `pool->cursor = ...` gcc folds the 0x500A0 offset into the store instead of keeping its address in a
 * register as the original does. */
int sefPoolAlloc(void) {
    SefPool *pool;
    SefPoolEntry *ring;
    SefPoolEntry *entry;
    int *poolCursor;
    int start;
    int last;
    int index;

    pool = &sefEntryPool;
    ring = &pool->entries[1];
    start = pool->cursor;
    poolCursor = &pool->cursor;
    last = (start - 1) & (SEF_POOL_RING_SIZE - 1);
    if (sefPoolFull != 0) {
        return 0;
    }
    for (index = start; index != last; index &= SEF_POOL_RING_SIZE - 1) {
        entry = &ring[index++];
        if (entry->record.life == 0) {
            *poolCursor = index & (SEF_POOL_RING_SIZE - 1);
            return index;
        }
    }
    sefPoolFull = 1;
    return 0;
}

/* VIF UNPACK code: CMD 0x6C (V4-32, USN set), NUM = count in bits 16-23, the low word 0x8000 fixed. */
#define VIF_UNPACK_V4_32_USN 0x6C008000U

/* The first data quadword of a block. The same 16 bytes are written at two widths: sefUnpackBlockInit clears the
 * whole quadword (sq) and then the count in its first word (sw), sefPacketUnpackRef writes the count (sw).
 * Flat fields (entryCount plus three zeroed words) compile to four sw instead of the original's por/sq. */
typedef union SefUnpackData {
    XglQuadword quad;
    unsigned int entryCount;  /* the particles in the block */
} SefUnpackData;

/* The head of a quadword block that sefDrawEmitter builds and sends by reference: sefUnpackBlockInit zeroes the
 * first VIF codes and writes a one-quadword UNPACK, which sefPacketUnpackRef widens to the whole block. */
typedef struct SefUnpackBlock {
    unsigned int vifNop[3];   /* +0x00: zero VIF codes (NOP) */
    unsigned int unpackCode;  /* +0x0C: VIF UNPACK V4-32 of the block's data */
    SefUnpackData data;       /* +0x10 */
} SefUnpackBlock;

/* Starts a block: zeroes its VIF NOPs, writes a one-quadword UNPACK and clears the first data quadword and its
 * count. */
void sefUnpackBlockInit(SefUnpackBlock *block) {
    block->vifNop[0] = 0;
    block->vifNop[1] = 0;
    block->vifNop[2] = 0;
    block->unpackCode = VIF_UNPACK_V4_32_USN | (1 << 16);
    block->data.quad = 0;
    block->data.entryCount = 0;
}


/* VIF codes (command in bits 24..30): FLUSH, MSCNT, and MSCAL whose immediate is the micro program address / 8. */
#define VIF_CODE_FLUSH 0x11000000
#define VIF_CODE_MSCAL 0x14000000
#define VIF_CODE_MSCNT 0x17000000

void sefPacketFlush(void) {
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, VIF_CODE_FLUSH);
}

void sefPacketMscnt(void) {
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddCode(packet, VIF_CODE_MSCNT);
}

/* blockSize (always 0x28 at every caller) is read nowhere here. */
void sefPacketUnpackRef(SefUnpackBlock *block, int blockSize, int count, unsigned int entryCount) {
    const void *environment;
    XglPacket *currentPacket;

    environment = (const void *) ((unsigned int) block & SDV_PHYSICAL_ADDRESS_MASK);
    currentPacket = xglPacketGetCurrent();
    sefPacketMscnt();
    block->data.entryCount = entryCount;
    block->unpackCode = VIF_UNPACK_V4_32_USN | ((unsigned int) (count - 1) << 16);
    sceVif1PkRef(currentPacket, environment, count, 0, 0, 0);
}

/* Opens a VIF UNPACK of the following data: VU address, UNPACK command, cycle and write lengths
   (pxy_packet_state.c calls it too). */
/* The corners of a unit quad, (x, y, 0, 1) for x, y = -1, 1, as vectors and as the eight doublewords
 * sefBuildCameraPacket copies. */
typedef union SefQuadCorners {
    DoublewordAlignedVector4 corner[4];
    u64 words[8];
} SefQuadCorners;

/* The VU packet at sefVuPacket: sefBuildVuPacket sends 0x11 quadwords from +0x00, 0xD from +0x40
 * or 3 from +0xE0. */
typedef struct SefVuPacket {
    SefQuadCorners corners;   /* +0x00: copy of sefQuadCorners (sefBuildCameraPacket) */
    Matrix4 modelViewMatrix;  /* +0x40: *sefViewMatrixPtr x the model matrix (sefBuildModelPacket) */
    Matrix4 cameraMatrix;     /* +0x80: copy of sefViewCameraMatrix (sefBuildModelPacket) */
    u64 stripTag;             /* +0xC0: GIF tag: NLOOP 1, EOP, PRIM 0x54 (textured, blended triangle strip), NREG 13 */
    u64 stripRegisters;       /* +0xC8: its registers: A+D, then ST, RGBAQ and XYZ2 for four vertices */
    u64 settingsTag;          /* +0xD0: GIF tag of the three A+D pairs from tex0 on: NLOOP 3, EOP, NREG 1 */
    u64 settingsRegisters;    /* +0xD8: GIF_AD */
    u64 tex0;                 /* +0xE0: A+D TEX0_1 value */
    u64 tex0Register;         /* +0xE8: GS_TEX0_1 */
    u64 alpha;                /* +0xF0: A+D ALPHA_1 value */
    u64 alphaRegister;        /* +0xF8: GS_ALPHA_1 */
    u64 test;                 /* +0x100: A+D TEST_1 value (sefSetupEmitterPacket) */
    u64 testRegister;         /* +0x108: GS_TEST_1 */
} SefVuPacket;

static SefVuPacket sefVuPacket;
extern unsigned char D_1F70[];
extern unsigned char D_1FD0[];

void sefBuildVuPacket(int kind, int alternate) {
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    sceVif1PkCnt(packet, 0);
    if (kind == 0) {
        sceVif1PkOpenUpkCode(packet, 0x14, 0x6C, 4, 4);
        sceVif1PkAddUpkData128N(packet, &sefVuPacket, 0x11);
    } else {
        if (alternate != 0) {
            sceVif1PkAddCode(packet, ((u32) D_1FD0 >> 3) | VIF_CODE_MSCAL);
        } else {
            sceVif1PkAddCode(packet, ((u32) D_1F70 >> 3) | VIF_CODE_MSCAL);
        }
        if (kind == 1) {
            sceVif1PkOpenUpkCode(packet, 0x18, 0x6C, 4, 4);
            sceVif1PkAddUpkData128N(packet, &sefVuPacket.modelViewMatrix, 0xD);
        } else {
            sceVif1PkOpenUpkCode(packet, 0x22, 0x6C, 4, 4);
            sceVif1PkAddUpkData128N(packet, &sefVuPacket.tex0, 3);
        }
    }
    sceVif1PkCloseUpkCode(packet);
    sceVif1PkAddCode(packet, VIF_CODE_FLUSH);
}

/* The height of the floor under the effects, set by sefRunEmitters. A one-member record (alias set device): the
 * member read has the float alias set, so the scheduler does not tie the load to the liveCount store that follows
 * it; sefUpdateKinds8To10Particles, sefUpdateKinds11To13Particles, sefUpdateKinds14To16Particles and
 * sefUpdateKinds17To19Particles depend on it. */
typedef struct SefFloor {
    f32 height;
} SefFloor;

/* The active camera's view matrix (sefBuildCameraPacket), the kind sefDrawResources drew last, and the floor height.
 * The floor record sits at 0x69E334: the original object was 4-aligned, while the record is 8-aligned, so the data
 * carve places this .sbss in two spans at their original addresses. */
static Matrix4 *sefViewMatrixPtr;
static int sefLastDrawnKind;
static SefFloor sefFloorHeight;
extern Matrix4 sefCameraMatrix[1];
/* The unit quad (rodata). Declared without const: as a const object its loads cannot conflict with the packet stores,
 * and gcc then schedules the epilogue's ld $31 after the fourteen stores instead of before them (fd 53/70). */
extern SefQuadCorners sefQuadCorners;

/* Takes the active camera's view matrix (sefViewMatrixPtr) into the VU packet and sefViewCameraMatrix (view x
   sefCameraMatrix),
 * and fills the packet's quad corners, GIF tags and depth test. The corners are copied as doublewords through the
 * union: as one struct assignment the tag stores go out before the epilogue's ld $31 (fd 55/70). */
void sefBuildCameraPacket(void) {
    XglCamera *camera;

    camera = xglStudioGetActiveCamera();
    sefViewMatrixPtr = &camera->volume[0].viewMatrix;
    QuadwordCopyMatrixT0(&sefVuPacket.modelViewMatrix, sefViewMatrixPtr);
    MMathMulMatrix(&sefViewCameraMatrix, sefViewMatrixPtr, sefCameraMatrix);
    sefVuPacket.stripTag = 0xD02A400000008001ULL;
    sefVuPacket.stripRegisters = 0x000512512512512EULL;
    sefVuPacket.settingsTag = 0x1000000000008003ULL;
    sefVuPacket.settingsRegisters = GIF_AD;
    sefVuPacket.test = GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 2);
    sefVuPacket.testRegister = GS_TEST_1;
    sefVuPacket.corners.words[0] = sefQuadCorners.words[0];
    sefVuPacket.corners.words[1] = sefQuadCorners.words[1];
    sefVuPacket.corners.words[2] = sefQuadCorners.words[2];
    sefVuPacket.corners.words[3] = sefQuadCorners.words[3];
    sefVuPacket.corners.words[4] = sefQuadCorners.words[4];
    sefVuPacket.corners.words[5] = sefQuadCorners.words[5];
    sefVuPacket.corners.words[6] = sefQuadCorners.words[6];
    sefVuPacket.corners.words[7] = sefQuadCorners.words[7];
}

/*
 * Fills the packet's model-view and camera matrices and its TEX0_1/ALPHA_1 A+D pairs for emitter emitterIndex.
 * Additive blending for blend kinds 3-4; 5-7 for even emitter indices; 11-13 for emitters 0 and 3; 14-16 for
 * emitters 0 and 1; 17-19 for emitter 2; 20 for emitter 1. Normal blending otherwise. Each test keeps its own
 * store, as the original's one block per test shows.
 */
void sefBuildModelPacket(u64 tex0, Matrix4 *matrix, int blendKind, int emitterIndex) {
    MMathMulMatrix(&sefVuPacket.modelViewMatrix, sefViewMatrixPtr, matrix);
    QuadwordCopyMatrixT0(&sefVuPacket.cameraMatrix, &sefViewCameraMatrix);
    sefVuPacket.tex0 = tex0;
    sefVuPacket.tex0Register = GS_TEX0_1;
    if (blendKind >= 3 && blendKind <= 4) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else if (blendKind >= 5 && blendKind <= 7) {
        if (emitterIndex & 1) {
            sefVuPacket.alpha = sefAlphaBlendValues[0];
        } else {
            sefVuPacket.alpha = sefAlphaBlendValues[1];
        }
    } else if (blendKind >= 11 && blendKind <= 13 && (emitterIndex == 0 || emitterIndex == 3)) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else if (blendKind >= 14 && blendKind <= 16 && emitterIndex < 2) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else if (blendKind >= 17 && blendKind <= 19 && emitterIndex == 2) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else if (blendKind == 20 && emitterIndex == 1) {
        sefVuPacket.alpha = sefAlphaBlendValues[1];
    } else {
        sefVuPacket.alpha = sefAlphaBlendValues[0];
    }
    sefVuPacket.alphaRegister = GS_ALPHA_1;
}

int sefActiveCount = 0;
f32 sefKind3StartRadii[2] = { 0.2f, 0.1f };

/* The unit quad, read by sefBuildCameraPacket above through a non-const declaration. */
const SefQuadCorners sefQuadCorners = {
    { { -1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, 0.0f, 1.0f } }
};

int sefGetActiveCount(void) {
    return sefActiveCount;
}

/* One 0x28-byte emitter of a resource slot: it spawns particles into its ring of pool indices
 * (sefSpawnHandlers per kind) and its group update ages them (sefUpdateHandlers per kind). */
typedef struct SefEmitter {
    u64 tex0;                  /* +0x00: GS TEX0_1 value of the emitter's image (sefSetupEmitterPacket) */
    unsigned short *particles; /* +0x08: ring of sefEntryPool indices, particleCount long; 0 for a free entry */
    f32 elapsed;               /* +0x0C: frames run; truncated to the frame number */
    f32 delay;                 /* +0x10: frames left before the emitter runs, counted down by sefRunEmitters */
    int frame;                 /* +0x14: the last whole frame handled; -1 until the first */
    short cursor;              /* +0x18: where the search for a free ring entry starts */
    short liveCount;           /* +0x1A: activity count of the last update: its live particles (kind 20 always 1) */
    short spawned;             /* +0x1C: set by the emitters that spawn only once */
    short createdFlag;         /* +0x1E: set to 1 when the slot is created; no reader in main, ov01 or ov02 */
    short clearedOnCreate[2];  /* +0x20: zeroed when the slot is created; no reader in main, ov01 or ov02 */
    u8 unmodeled_24[0x28 - 0x24];
} SefEmitter;

/* A texture and its palette (image numbers in the effect's type list) for one emitter. */
typedef struct SefImagePair {
    short image;
    short clut;
} SefImagePair;

/* The part of a resource slot from +0x40 on (offsets below are slot offsets). sefSetSlotFlags walks the
 * table through this part: its loop pointer starts at sefResourceTable + 0x40. */
typedef struct ResourceBody {
    int resetFlag;   /* +0x40: set to 1 by sefReleaseCellSlot after the slot is cleared */
    int flags;       /* +0x44: bit flags (RESOURCE_FLAG_*), set by sefSetSlotFlags and sefObjectSetFlags */
    /* +0x48: nonzero while the slot is in use. sefCreateResourceSlot stores its pairs argument here (the caller's
     * stack array, so a slot created with no pairs stays free); it is only tested against 0, never dereferenced. */
    const SefImagePair *activePairs;
    u8 unmodeled_4c[0x50 - 0x4c];
    /* +0x50: the owning actor or record (uwamono_objx.c passes pointers to sefSetRecordOwner); saved with the slot.
     * Typed as sef.c's SefSavedEffect holds it: a void * here changes sefCollectPersistentSlots' copies. */
    struct SefGroupObject *owner;
    u8 unmodeled_54[0x58 - 0x54];
    /* +0x58: saved with the slot and written back by sefPopEffect; no reader here. A pointer typed like owner:
     * as an int or a void *, sefCollectPersistentSlots's field copies come out in another order. */
    struct SefGroupObject *savedWord;
    u8 unmodeled_5c[0x6c - 0x5c];
    s16 typeListIndex; /* +0x6C: image type list of the effect (sefSearchMapperIndex), passed to svGetImageListItem */
    s16 visible;     /* +0x6E: set to 1 when the slot is set up; zero skips drawing */
    u8 unmodeled_70[0x74 - 0x70];
    int key;         /* +0x74: the owner key that releases or flags the slot */
    u8 unmodeled_78[0x80 - 0x78];
    Matrix4 matrix;  /* +0x80: the slot's model matrix, handed to sefBuildModelPacket */
    SefEmitter emitters[5]; /* +0xC0 */
    /* +0x188: this slot's particle-index rings inside the small-cell heap (particleCount entries per
     * emitter), nonzero while the slot owns cells; sefReleaseCellSlot frees it with smFreeHandle(void *). */
    unsigned short *handle;
    int particleCount; /* +0x18C: entries of each emitter's ring (a power of two) */
    int emitterCount;  /* +0x190: emitters in use: the loop bound of sefRunEmitters and sefDrawResources */
    s16 kind;          /* +0x194: effect kind: selects the handlers in sefSpawnHandlers and sefUpdateHandlers */
    s16 liveCount;     /* +0x196: sum of the emitters' liveCount, recounted by sefRunEmitters */
    s16 clearedOnCreate; /* +0x198: zeroed when the slot is created; no reader in main, ov01 or ov02 */
    u8 unmodeled_19a[0x1a0 - 0x19a];
} ResourceBody;

/* ResourceBody.flags bits read by sefRunEmitters (SUSPENDED is also set and cleared by sefResetSlotRecords). */
#define RESOURCE_FLAG_NO_DEPTH_TEST 0x4    /* sefSetupEmitterPacket draws the slot with the depth test off */
#define RESOURCE_FLAG_PAUSED 0x80          /* the spawn handler is skipped and an empty slot is kept */
#define RESOURCE_FLAG_SUSPENDED 0x400000   /* the slot is not updated at all */
#define RESOURCE_FLAG_PERSISTENT 0x4000000 /* an empty slot is kept */

/* One 0x1A0-byte resource slot of sefResourceTable; only the fields the code touches are named. */
typedef struct ResourceSlot {
    Vector4 position;  /* +0x00: w set to 1.0f per draw by sefDrawResources */
    Vector4 rotation;  /* +0x10 */
    Vector4 scale;     /* +0x20 */
    Vector4 tint;      /* +0x30: colour multiplier of the drawn particles (lqc2 vf15 in sefDrawEmitter);
                        * 1, 1, 1 draws them untinted; the slot is created with it set to ones */
    ResourceBody body;
} ResourceSlot;

/* The slot table: SEF_RESOURCE_SLOT_COUNT slots of 0x1A0 bytes. */
#define SEF_RESOURCE_SLOT_COUNT 192

extern ResourceSlot sefResourceTable[SEF_RESOURCE_SLOT_COUNT];

/* Clears the active count (sefActiveCount) and the whole slot table. */
void sefResetResourceTable(void) {
    sefActiveCount = 0;
    memset(sefResourceTable, 0, sizeof(sefResourceTable));
}


void sefResetAll(void) {
    sefReleaseByKey(-1);
    sefResetBuffers();
    sefResetResourceTable();
}


/* Releases every active slot whose key matches (a negative key matches all), then clears the active count. */
void sefReleaseByKey(int key) {
    int index;

    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        if (sefResourceTable[index].body.activePairs != 0 && sefResourceTable[index].body.handle != 0
            && (key < 0 || key == sefResourceTable[index].body.key)) {
            sefReleaseCellSlot(&sefResourceTable[index]);
        }
    }
    sefActiveCount = 0;
}

/* Sets flagBits in every active slot whose key matches; a negative key matches all. */
void sefSetSlotFlags(int key, int flagBits) {
    ResourceSlot *table;
    int index;
    ResourceBody *body;

    table = sefResourceTable;
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        body = &table[index].body;
        if (body->activePairs != 0 && body->handle != 0 && (key < 0 || key == body->key)) {
            body->flags |= flagBits;
        }
    }
}

/* Clears flagBits in every active slot whose key matches; a negative key matches all. */
void sefClearSlotFlags(int key, int flagBits) {
    ResourceSlot *table;
    int index;
    ResourceBody *body;

    table = sefResourceTable;
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        body = &table[index].body;
        if (body->activePairs != 0 && body->handle != 0 && (key < 0 || key == body->key)) {
            body->flags &= ~flagBits;
        }
    }
}

/* Frees every particle the slot's rings still hold: the pool entry's life goes to 0 and the ring entry with it. */
void sefFreeSlotCells(ResourceSlot *slot) {
    int i;

    for (i = 0; i < slot->body.particleCount * slot->body.emitterCount; i++) {
        if (slot->body.handle[i] != 0) {
            sefEntryPool.entries[slot->body.handle[i]].fade.life = 0.0f;
            slot->body.handle[i] = 0;
        }
    }
}

/* Stores (1, 1, 1, 1) through vector: vf0.w broadcast into all four lanes of vf1. */
static inline void sefSetOnes(AlignedVector4 *vector) {
    __asm__ __volatile__(
        "vmaxw.xyzw vf1, vf0, vf0w\n"
        "sqc2 vf1, 0(%0)\n"
        :
        : "r" (vector)
        : "memory");
}

/* Loads MMathUnitMatrix into destination: rows 0..2 through vf3..vf1, row 3 stored from vf0 (0, 0, 0, 1). */
static inline void sefLoadUnitMatrix(Matrix4 *destination) {
    __asm__ __volatile__(
        "lqc2 vf1, 32(%1)\n"
        "lqc2 vf2, 16(%1)\n"
        "lqc2 vf3, 0(%1)\n"
        "sqc2 vf0, 48(%0)\n"
        "sqc2 vf1, 32(%0)\n"
        "sqc2 vf2, 16(%0)\n"
        "sqc2 vf3, 0(%0)\n"
        :
        : "r" (destination), "r" (&MMathUnitMatrix)
        : "memory");
}

/* Particles in each emitter ring, per effect kind. */
extern short sefRingSizes[];

/* Creates a resource slot for effectId: the first free slot from slotIndex on (0 when negative; a busy
 * slotIndex fails), with emitterCount emitters drawing pairs. Ids 0x100C83A7 and 0x100C83A8 become
 * 0x100C8399 and 0x100C839A at five times the scale. Returns the slot, 0 on failure. rotation, the block
 * sefCreateEffectCf2 passes on, is not read. */
ResourceSlot *sefCreateResourceSlot(int effectId, SefImagePair *pairs, int kind, const Vector4 *position,
                                    Vector4 *rotation, Vector4 *scale, int emitterCount, int slotIndex,
                                    f32 delayStep) {
    int enlarged;
    int size;
    unsigned short *rings;
    ResourceSlot *slot;
    SefEmitter *emitter;
    SvTypeListEntry *image;
    SvTypeListEntry *clut;
    int typeListIndex;
    int index;
    int emitterIndex;
    int psm;
    const SefImagePair *busy;

    size = sefRingSizes[kind] * emitterCount * 2;
    enlarged = 0;
    rings = smAllocSmall(size);
    if (rings == 0) {
        return 0;
    }
    if (effectId == 0x100C83A7) {
        effectId = 0x100C8399;
        enlarged = 1;
    } else if (effectId == 0x100C83A8) {
        effectId = 0x100C839A;
        enlarged = 1;
    }
    index = 0;
    if (slotIndex >= 0) {
        busy = sefResourceTable[slotIndex].body.activePairs;
        index = slotIndex;
        if (busy != 0) {
            return 0;
        }
    }
    for (; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        slot = &sefResourceTable[index];
        if (slot->body.activePairs == 0) {
            typeListIndex = sefSearchMapperIndex(0x100C8384);
            memset(rings, 0, size);
            memset(slot, 0, sizeof(ResourceSlot));
            slot->body.activePairs = pairs;
            slot->body.key = effectId;
            slot->body.particleCount = sefRingSizes[kind];
            slot->body.handle = rings;
            slot->body.kind = kind;
            slot->body.typeListIndex = typeListIndex;
            slot->body.resetFlag = 1;
            slot->body.emitterCount = emitterCount;
            slot->body.clearedOnCreate = 0;
            slot->body.liveCount = 0;
            slot->body.visible = 1;
            sefSetOnes((AlignedVector4 *) &slot->tint);
            Vu0SetOrigin(&slot->rotation);
            if (position != 0) {
                QuadwordCopyT0(&slot->position, position);
            } else {
                Vu0SetOrigin(&slot->position);
            }
            if (scale != 0) {
                QuadwordCopyT0(&slot->scale, scale);
            } else {
                sefSetOnes((AlignedVector4 *) &slot->scale);
            }
            if (enlarged) {
                Vu0SefScaleVectorXYZNI(&slot->scale, &slot->scale, 5.0f);
            }
            emitterIndex = 0;
            if (emitterCount > 0) {
                /* The id is stored in the key above; from here on effectId holds the address of the current
                 * emitter. With a separate `SefEmitter *` cursor (emitter = slot->body.emitters, emitter++) gcc
                 * swaps the slot and cursor registers ($17/$18, fd 193/252 in place). */
                effectId = (int) slot->body.emitters;
                do {
                    emitter = (SefEmitter *) effectId;
                    emitter->frame = -1;
                    emitter->cursor = 0;
                    emitter->spawned = 0;
                    emitter->createdFlag = 1;
                    emitter->clearedOnCreate[0] = 0;
                    emitter->delay = delayStep * emitterIndex;
                    emitter->clearedOnCreate[1] = 0;
                    emitter->elapsed = 0;
                    emitter->particles = rings + slot->body.particleCount * emitterIndex;
                    if (pairs != 0) {
                        image = svGetImageListItem(typeListIndex, pairs[emitterIndex].image);
                        clut = svGetImageListItem(typeListIndex, pairs[emitterIndex].clut);
                        if (image != 0 && clut != 0) {
                            psm = image->bitsPerPixel == 8 ? GS_PSMT8 : GS_PSMT4;
                            emitter->tex0 = GS_SET_TEX0(image->basePointer, image->bufferWidthPixels >> 6, psm,
                                                        image->textureWidthExponent, image->textureHeightExponent, 1, 0,
                                                        clut->basePointer, 0, 0, 0, 1);
                        }
                    }
                    effectId += sizeof(SefEmitter);
                    emitterIndex++;
                } while (emitterIndex < emitterCount);
            }
            sefLoadUnitMatrix(&slot->body.matrix);
            if (slot->body.kind < 5 || (slot->body.kind >= 20 && slot->body.kind <= 21)) {
                slot->body.flags |= RESOURCE_FLAG_PERSISTENT;
            }
            slot->body.flags |= 2;
            return slot;
        }
    }
    if (rings != 0) {
        smFreeHandle(rings);
    }
    return 0;
}

/* Frees the particles of a slot that owns cells and resets its emitters' frame, elapsed time and delay (the one-shot
 * spawned latch is kept), then sets or clears its suspended flag. */
void sefResetSlotRecords(ResourceSlot *slot, int suspend) {
    int index;

    if (slot != 0 && slot->body.handle != 0) {
        sefFreeSlotCells(slot);
        for (index = 0; index < slot->body.emitterCount; index++) {
            slot->body.emitters[index].frame = -1;
            slot->body.emitters[index].elapsed = 0.0f;
            slot->body.emitters[index].delay = 0.0f;
        }
        if (suspend) {
            slot->body.flags |= RESOURCE_FLAG_SUSPENDED;
        } else {
            slot->body.flags &= ~RESOURCE_FLAG_SUSPENDED;
        }
    }
}

void sefObjectSetFlags(ResourceSlot *slot, int mask) {
    if (slot != 0 && slot->body.handle != 0) {
        slot->body.flags |= mask;
    }
}

void sefObjectClearFlags(ResourceSlot *slot, int mask) {
    if (slot != 0 && slot->body.handle != 0) {
        slot->body.flags &= ~mask;
    }
}

void sefReleaseCellSlot(ResourceSlot *slot) {
    if (slot != 0 && slot->body.handle != 0) {
        sefFreeSlotCells(slot);
        smFreeHandle(slot->body.handle);
        memset(slot, 0, sizeof(ResourceSlot));
        slot->body.resetFlag = 1;
    }
}

/* Picks the image pairs and emitter count for an effect kind (ids 0x100C83A7 and 0x100C83A8 force kinds 20
 * and 21) and creates its resource slot through sefCreateResourceSlot (rotation is only passed on). */
ResourceSlot *sefCreateKindResourceSlot(int effectId, SefImageSource *sources, int kind, const Vector4 *position,
                                        Vector4 *rotation, Vector4 *scale, int slotIndex) {
    SefImagePair pairs[5];

    if (effectId == 0x100C83A7) {
        kind = 20;
    }
    if (effectId == 0x100C83A8) {
        kind = 21;
    }
    if (kind < 0) {
        return 0;
    }
    if (kind < 3) {
        pairs[0].image = sources[0].image;
        pairs[0].clut = sources[0].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, rotation, scale, 1, slotIndex, 0.0f);
    } else if (kind == 3) {
        pairs[0].image = sources[1].image;
        pairs[0].clut = sources[1].clut;
        pairs[1].image = sources[2].image;
        pairs[1].clut = sources[2].clut;
        return sefCreateResourceSlot(effectId, pairs, 3, position, rotation, scale, 2, slotIndex, 0.0f);
    } else if (kind == 4) {
        pairs[0].image = sources[1].image;
        pairs[0].clut = sources[1].clut;
        pairs[1].image = sources[2].image;
        pairs[1].clut = sources[2].clut;
        return sefCreateResourceSlot(effectId, pairs, 4, position, rotation, scale, 2, slotIndex, 0.0f);
    } else if (kind >= 5 && kind <= 7) {
        pairs[0].image = sources[3].image;
        pairs[0].clut = sources[3].clut;
        pairs[1].image = sources[0].image;
        pairs[1].clut = sources[0].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, rotation, scale, 2, slotIndex, 1.0f);
    } else if (kind >= 8 && kind <= 10) {
        pairs[0].image = sources[0].image;
        pairs[0].clut = sources[0].clut;
        pairs[1].image = sources[5].image;
        pairs[1].clut = sources[5].clut;
        pairs[2].image = sources[4].image;
        pairs[2].clut = sources[4].clut;
        pairs[3].image = sources[0].image;
        pairs[3].clut = sources[0].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, rotation, scale, 4, slotIndex, 1.0f);
    } else if (kind >= 11 && kind <= 13) {
        pairs[0].image = sources[3].image;
        pairs[0].clut = sources[3].clut;
        pairs[1].image = sources[0].image;
        pairs[1].clut = sources[0].clut;
        pairs[2].image = sources[7].image;
        pairs[2].clut = sources[7].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, rotation, scale, 3, slotIndex, 1.0f);
    } else if (kind >= 22 && kind <= 24) {
        pairs[0].image = sources[0].image;
        pairs[0].clut = sources[0].clut;
        pairs[1].image = sources[7].image;
        pairs[1].clut = sources[7].clut;
        pairs[2].image = sources[7].image;
        pairs[2].clut = sources[7].clut;
        pairs[3].image = sources[0].image;
        pairs[3].clut = sources[0].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, rotation, scale, 4, slotIndex, 1.0f);
    } else if (kind >= 14 && kind <= 16) {
        pairs[0].image = sources[8].image;
        pairs[0].clut = sources[8].clut;
        pairs[1].image = sources[9].image;
        pairs[1].clut = sources[9].clut;
        pairs[2].image = sources[10].image;
        pairs[2].clut = sources[10].clut;
        pairs[3].image = sources[11].image;
        pairs[3].clut = sources[11].clut;
        pairs[4].image = sources[12].image;
        pairs[4].clut = sources[12].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, rotation, scale, 5, slotIndex, 1.0f);
    } else if (kind >= 17 && kind <= 19) {
        pairs[0].image = sources[0].image;
        pairs[0].clut = sources[0].clut;
        pairs[1].image = sources[0].image;
        pairs[1].clut = sources[0].clut;
        pairs[2].image = sources[13].image;
        pairs[2].clut = sources[13].clut;
        pairs[3].image = sources[14].image;
        pairs[3].clut = sources[14].clut;
        pairs[4].image = sources[15].image;
        pairs[4].clut = sources[15].clut;
        return sefCreateResourceSlot(effectId, pairs, kind, position, rotation, scale, 5, slotIndex, 0.0f);
    } else if (kind == 20) {
        pairs[0].image = sources[18].image;
        pairs[0].clut = sources[18].clut;
        pairs[1].image = sources[17].image;
        pairs[1].clut = sources[17].clut;
        pairs[2].image = sources[19].image;
        pairs[2].clut = sources[19].clut;
        return sefCreateResourceSlot(effectId, pairs, 20, position, rotation, scale, 3, slotIndex, 0.0f);
    } else if (kind == 21) {
        pairs[0].image = sources[20].image;
        pairs[0].clut = sources[20].clut;
        pairs[1].image = sources[21].image;
        pairs[1].clut = sources[21].clut;
        return sefCreateResourceSlot(effectId, pairs, 21, position, rotation, scale, 2, slotIndex, 0.0f);
    } else {
        return 0;
    }
}

void sefGetCylinderPos(Vector4 *vector, f32 scale, f32 yScale) {
    sefGetCirclePos(vector, sefRandf() * scale);
    vector->y = sefRandf() * yScale;
}

/* sef.h does not declare it: the definition takes a DoublewordVector4 (a codegen lever of its stores: with an
 * AlignedVector4 parameter sef.c's .text differs, checked 2026-11-20) and these callers hold AlignedVector4 vectors. */
void sefGetUnitDirection(AlignedVector4 *vector);

/* Writes a random unit direction (sefGetUnitDirection) into vector and scales it: by -scale when scale is negative,
 * else by scale times sefRandf(). */
void sefGetRandomScaledDirection(AlignedVector4 *vector, f32 scale) {
    f32 factor;

    sefGetUnitDirection(vector);
    if (scale < 0.0f) {
        factor = -scale;
    } else {
        factor = sefRandf() * scale;
    }
    Vu0SefScaleVectorXYZNI(vector, vector, factor);
}

/* Writes a rotation about Z by angle into the first three rows of rows: (c, s, 0, 0), (-s, c, 0, 0), (0, 0, 1, 0).
 * VU0 macro mode: the cosine (0xE8) and sine (0x20) micro subroutines take the angle in vf4 and leave the result
 * in the x lane of vf1. Each result lands in the x member of a stack vector, as in the original; the two asm blocks
 * have no "memory" clobber, so the matrix stores reuse the FPR copies. An inline function: sefSpawnKinds0To2 and
 * sefSpawnKinds3To4 hold a copy of these instructions each, the other spawners call sefRotationZRows. */
static inline void sefWriteRotationZ(SefRotationRow *rows, f32 angle) {
    Vector4 cosine;
    Vector4 sine;

    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0xE8\n\t"
        "qmfc2.i $8, vf1\n\t"
        "mtc1 $8, %0"
        : "=f"(cosine.x) : "f"(angle) : "$8");
    __asm__ __volatile__(
        "mfc1 $8, %1\n\t"
        "qmtc2.ni $8, vf4\n\t"
        "vcallms 0x20\n\t"
        "qmfc2.i $8, vf1\n\t"
        "mtc1 $8, %0"
        : "=f"(sine.x) : "f"(angle) : "$8");
    rows[0].element[0] = cosine.x;
    rows[0].element[1] = sine.x;
    rows[0].halves[1] = 0;
    rows[1].element[0] = -sine.x;
    rows[1].element[1] = cosine.x;
    rows[1].halves[1] = 0;
    rows[2].quad = 0;
    rows[2].element[2] = 1.0f;
}

/* The out-of-line copy of sefWriteRotationZ (0x26CEA8). */
void sefRotationZRows(SefRotationRow *rows, f32 angle) {
    sefWriteRotationZ(rows, angle);
}


/* Writes a random rotation into the first three rows of rows: angle x 0, y and z random, composed by
 * MMathRotateMatrixYXZ. */
void sefRandomRotateMatrix(SefRotationRow *rows)
{
    Vector4 angles;
    /* The original copies the three rows out as six doublewords (ld/sd), so the matrix is also seen as u64 words:
     * the plain copy rows[row].element[column] = matrix[row][column] compiles to a lwc1/swc1 loop (39 insns for 29). */
    union {
        Matrix4 rows;
        u64 words[8];
    } matrix;

    angles.x = 0.0f;
    angles.y = sefRandAngle();
    angles.z = sefRandAngle();
    MMathRotateMatrixYXZ(&matrix.rows, 0, &angles);
    rows[0].halves[0] = matrix.words[0];
    rows[0].halves[1] = matrix.words[1];
    rows[1].halves[0] = matrix.words[2];
    rows[1].halves[1] = matrix.words[3];
    rows[2].halves[0] = matrix.words[4];
    rows[2].halves[1] = matrix.words[5];
}


/*
 * The vector operations of the particle spawners and updates of effect kinds 0 to 19 and of sefSetSlotTransform and
 * sefCullResourceSlots as inline functions: the instructions of QuadwordCopyT0, Vu0SetOrigin, Vu0SefScaleVectorXYZNI,
 * Vu0SefSubVectorXYZ and Vu0SefAddVectorXYZ with the four floats the store writes declared as "=m" outputs, the form of
 * sef.c's SefCopyVector4, SefSetOrigin4 and SefSubVectorXYZ4 and of sv_get_image_item.c's svCopyVector (TU-local
 * there too).
 * The four outputs make each address or scale operand weigh four times in the register allocator, which the original's
 * choice of callee-saved registers needs; being functions, both Vu0SetOrigin sites of sefSpawnKinds3To4 are one
 * source line.
 */
static inline void sefCopyVector(AlignedVector4 *destination, AlignedVector4 *source) {
    __asm__ __volatile__(
        "lq $8, 0(%5)\n"
        "sq $8, 0(%4)\n"
        : "=m" (destination->x), "=m" (destination->y), "=m" (destination->z), "=m" (destination->w)
        : "r" (destination), "r" (source)
        : "$8", "memory");
}

static inline void sefSetOrigin(AlignedVector4 *vector) {
    Vu0SetOrigin(vector);
}

static inline void sefScaleVector(AlignedVector4 *destination, AlignedVector4 *vector, f32 scale) {
    __asm__ __volatile__(
        "lqc2 vf1, 0(%5)\n"
        "mfc1 $8, %6\n"
        "qmtc2.ni $8, vf2\n"
        "vmulx.xyz vf1, vf1, vf2x\n"
        "sqc2 vf1, 0(%4)\n"
        : "=m" (destination->x), "=m" (destination->y), "=m" (destination->z), "=m" (destination->w)
        : "r" (destination), "r" (vector), "f" (scale)
        : "$8", "memory");
}

static inline void sefSubVector(AlignedVector4 *destination, AlignedVector4 *minuend, AlignedVector4 *subtrahend) {
    __asm__ __volatile__(
        "lqc2 vf1, 0(%5)\n"
        "lqc2 vf2, 0(%6)\n"
        "vsub.xyz vf1, vf1, vf2\n"
        "sqc2 vf1, 0(%4)\n"
        : "=m" (destination->x), "=m" (destination->y), "=m" (destination->z), "=m" (destination->w)
        : "r" (destination), "r" (minuend), "r" (subtrahend)
        : "memory");
}

static inline void sefAddVector(AlignedVector4 *destination, AlignedVector4 *first, AlignedVector4 *second) {
    __asm__ __volatile__(
        "lqc2 vf1, 0(%5)\n"
        "lqc2 vf2, 0(%6)\n"
        "vadd.xyz vf1, vf1, vf2\n"
        "sqc2 vf1, 0(%4)\n"
        : "=m" (destination->x), "=m" (destination->y), "=m" (destination->z), "=m" (destination->w)
        : "r" (destination), "r" (first), "r" (second)
        : "memory");
}

/* The burst emitter parameters of effect kinds 0 to 2 (12 bytes, sefKinds0To2Params indexed by kind). */
typedef struct SefBurstParams {
    int interval;  /* +0x00: frames between spawns; 0 spawns every frame */
    int count;     /* +0x04: particles per spawn */
    int endFrame;  /* +0x08: the first frame without spawning; 0 never stops */
} SefBurstParams;

extern SefBurstParams sefKinds0To2Params[];
extern AlignedVector4 sefKinds0To2Origin;
extern AlignedVector4 sefKinds0To2AimCenter;
extern Matrix4 sefKinds0To2AimMatrix;
extern int sefKinds0To2Shades[];

/* Emitters of effect kinds 0 to 2: fires once per frame until params->endFrame, on the frames params->interval
 * divides: spawns up to count particles that start at sefKinds0To2Origin and fly towards a random point 3.0 around
 * sefKinds0To2AimCenter; target is that direction turned by sefKinds0To2AimMatrix; sefKinds0To2Shades holds the
 * shade per kind. */
void sefSpawnKinds0To2(ResourceSlot *slot, int index) {
    SefEmitter *emitter = &slot->body.emitters[index];
    SefBurstParams *params;
    SefFadeParticle *particle;
    AlignedVector4 aim;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int shade;
    int spawned;

    params = &sefKinds0To2Params[slot->body.kind];
    frame = emitter->elapsed;
    due = 0;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame) {
        return;
    }
    if (params->endFrame != 0 && frame >= params->endFrame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    shade = sefKinds0To2Shades[slot->body.kind];
    spawned = 0;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        particle = &sefEntryPool.entries[id].fade;
        emitter->particles[cursor] = id;
        sefCopyVector(&particle->position, &sefKinds0To2Origin);
        sefSetOnes(&particle->growth);
        particle->position.w = 1.0f;
        sefGetUnitDirection(&aim);
        sefScaleVector(&aim, &aim, 3.0f);
        sefAddVector(&aim, &aim, &sefKinds0To2AimCenter);
        sefSubVector(&particle->velocity, &aim, &particle->position);
        MMathNormalizeVector2(&particle->velocity, &particle->velocity);
        MMathApplyMatrix(&particle->target, &sefKinds0To2AimMatrix, &particle->velocity);
        sefWriteRotationZ(particle->rotation, sefRandAngle());
        spawned++;
        particle->scale.x = particle->scale.y = particle->scale.z = 1.0f;
        particle->scale.w = (sefNoiseNext() * 0.5f + 1.0f) * 0.5f * 0.64f;
        particle->life = 120.0f;
        particle->red = shade;
        particle->green = shade;
        particle->blue = shade;
        particle->alpha = 0;
        particle->age = 0;
        particle->verticalSpeed = 0;
        particle->axisOffset = 0;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern AlignedVector4 sefKinds3To4GrowthOffset;
extern f32 sefKinds3To4Speeds[];
extern f32 sefKinds3To4Counts[];

/* Emitters of effect kinds 3 and 4 (sefKinds3To4Speeds and sefKinds3To4Counts hold a speed and a count per kind
 * and emitter): on the frames that are a multiple of emitter->spawned + 1 (redrawn from 0 to 3 at each spawn),
 * spawns up to count particles that fly from a random start point towards a random target; emitter 1's are
 * smaller and offset along their first axis. */
void sefSpawnKinds3To4(ResourceSlot *slot, int index) {
    SefEmitter *emitter = &slot->body.emitters[index];
    SefFadeParticle *particle;
    AlignedVector4 aim;
    int frame;
    int row;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int count;
    short phase;
    f32 speed;

    frame = emitter->elapsed;
    if (emitter->frame == frame) {
        return;
    }
    phase = frame % (emitter->spawned + 1);
    emitter->frame = frame;
    if (phase != 0) {
        return;
    }
    row = index;
    if (slot->body.kind != 3) {
        row = index + 2;
    }
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    count = sefKinds3To4Counts[row];
    speed = sefKinds3To4Speeds[row];
    spawned = 0;
    emitter->spawned = xglSRand() & 3;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        emitter->particles[cursor] = id;
        particle = &sefEntryPool.entries[id].fade;
        if (slot->body.kind == 3) {
            sefGetUnitDirection(&aim);
            sefScaleVector(&particle->position, &aim, sefKind3StartRadii[index]);
            if (index != 0) {
                sefGetRandomScaledDirection(&aim, 0.2f);
            } else {
                sefSetOrigin(&aim);
            }
        } else if (index != 0) {
            sefGetCylinderPos(&particle->position, 0.3f, 0.1f);
            sefGetCylinderPos(&aim, 0.2f, 0.0f);
            aim.y += 0.4f;
            speed = (sefRandf() + 0.25f) * 0.015f;
        } else {
            sefGetCylinderPos(&particle->position, 0.4f, 0.0f);
            sefSetOrigin(&aim);
        }
        sefSubVector(&particle->velocity, &aim, &particle->position);
        MMathNormalizeVector2(&particle->velocity, &particle->velocity);
        sefScaleVector(&particle->velocity, &particle->velocity, speed);
        particle->velocity.w = 1.0f;
        particle->position.w = 1.0f;
        sefWriteRotationZ(particle->rotation, sefRandAngle());
        particle->growth.x = sefNoiseNext();
        particle->growth.y = sefNoiseNext();
        particle->growth.z = sefNoiseNext();
        particle->growth.w = 1.0f;
        if (index != 0) {
            sefSetOnes(&particle->scale);
            particle->scale.w = 0.02f;
            particle->axisOffset = -4.0f;
            sefScaleVector(&particle->growth, &particle->growth, 0.5f);
        } else {
            particle->scale.x = particle->scale.y = particle->scale.z = 0.2f;
            particle->scale.w = 0.2048f;
            particle->axisOffset = 0;
        }
        sefAddVector(&particle->growth, &particle->growth, &sefKinds3To4GrowthOffset);
        spawned++;
        particle->life = 20.0f;
        particle->red = 0x80;
        particle->green = 0x80;
        particle->blue = 0x80;
        particle->alpha = 0;
        particle->age = 0;
        particle->verticalSpeed = 0;
        if (spawned >= count) {
            break;
        }
    }
}

/* What one emitter of an effect kind spawns (0x20 bytes). */
typedef struct SefEmitterParams {
    int interval;           /* +0x00: frames between spawns; 0 spawns every frame */
    int count;              /* +0x04: particles per spawn */
    f32 life;               /* +0x08 */
    f32 spread;             /* +0x0C: scale handed to sefGetRandomScaledDirection for the start position */
    f32 size;               /* +0x10 */
    f32 scaleW;             /* +0x14: stored in the particle's scale.w */
    short shade;            /* +0x18: red, green and blue */
    short alpha;            /* +0x1A */
    short lastFrame;        /* +0x1C: no spawning after this frame */
    unsigned char unmodeled_1e[0x20 - 0x1E];
} SefEmitterParams;

/* The parameter rows of kinds 5 to 7 (sefEmitterParamsKinds5To7, 0x5E6F80), addressed from the row of kind 0: the
   original
 * indexes [kind][line] from 0x5E6D00, five rows (0x80 bytes each) before the table, which lies inside srsRecordTable
 * (+0x630). The same access as (sefEmitterParamsKinds5To7 - 5)[kind] or [kind - 5] costs two instructions (fd
   137/135). */
extern SefEmitterParams sefEmitterParamsKindBase[][4];

/* Emitters of effect kinds 0 to 7 (parameter rows of four per kind): once per frame, spawns up to count particles into
   free
 * entries of the ring. */
void sefSpawnKinds5To7(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefEmitterParams *params;
    SefFadeParticle *particle;
    int frame;
    int fire;
    int mask;
    int cursor;
    int last;
    int spawned;
    int id;
    int lastFrame;
    f32 growth;

    emitter = &slot->body.emitters[index];
    fire = 0;
    frame = emitter->elapsed;
    params = &sefEmitterParamsKindBase[slot->body.kind][index];
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        fire = 1;
    }
    if (emitter->frame != frame && frame <= lastFrame) {
        emitter->frame = frame;
        if (fire) {
            mask = slot->body.particleCount - 1;
            last = (emitter->cursor - 1) & mask;
            cursor = emitter->cursor;
            spawned = 0;
            while (cursor != last) {
                cursor &= mask;
                if (emitter->particles[cursor] == 0) {
                    id = sefPoolAlloc();
                    if (id == 0) {
                        break;
                    }
                    particle = &sefEntryPool.entries[id].fade;
                    emitter->particles[cursor] = id;
                    spawned++;
                    sefGetRandomScaledDirection(&particle->position, params->spread);
                    sefRotationZRows(particle->rotation, sefRandAngle());
                    particle->scale.x = particle->scale.y = particle->scale.z = params->size;
                    particle->scale.w = params->scaleW;
                    growth = sefNoiseNext() * 0.5f + 1.0f;
                    particle->growth.x = particle->growth.y = particle->growth.z = growth;
                    particle->growth.w = 1.0f;
                    particle->red = particle->green = particle->blue = params->shade;
                    particle->alpha = params->alpha;
                    particle->position.y += 0.25f;
                    particle->life = params->life;
                    particle->position.w = 1.0f;
                    particle->age = 0;
                    particle->verticalSpeed = 0;
                    particle->axisOffset = 0;
                    if (spawned >= params->count) {
                        break;
                    }
                }
                cursor++;
            }
        }
    }
}

/* velocity = target - position (xyz). The "m" operand is an input the instructions never read (they go through
   the register operands); it is there only so that ee-gcc gives the position pointer the registers the original
   uses ($17/$18; without it the parameter row takes them). The three spawners that use it keep the position in a
   local; the plain Vu0SefSubVectorXYZ forms, a const or reordered local, the local also used for the position
   stores, and several other lifetimes compiled to the swapped allocation in sefSpawnKinds8To10, 22Up and
   14To16. */
#define SefVelocityTowards(velocity, target, position) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%1)\n" \
        "lqc2 vf2, 0(%2)\n" \
        "vsub.xyz vf1, vf1, vf2\n" \
        "sqc2 vf1, 0(%0)\n" \
        : \
        : "r" (velocity), "r" (target), "r" (position), "m" (*(position)) \
        : "memory")

extern SefEmitterParams sefEmitterParamsKinds8To10[];

/* Emitters of effect kinds 8 to 10 (parameter rows of four per kind): 0 and 3 spin in place, 1 and 2 fly towards a
   random point
   above the origin; emitter 1 shrinks its particles after the ninth. */
void sefSpawnKinds8To10(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 *position;
    SefEmitterParams *params;
    AlignedVector4 direction;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int row;
    int lastFrame;

    due = 0;
    emitter = &slot->body.emitters[index];
    frame = emitter->elapsed;
    row = slot->body.kind - 8;
    params = &sefEmitterParamsKinds8To10[index + row * 4];
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        position = &entry->drift.position;
        emitter->particles[cursor] = id;
        sefGetRandomScaledDirection(position, params->spread);
        entry->drift.scale.w = params->scaleW;
        entry->drift.growth.w = 1.0f;
        if (index == 0 || index == 3) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z =
                sefNoiseNext() * 0.5f + 1.0f;
        } else {
            sefRandomRotateMatrix(entry->drift.rotation);
            if (index == 1 && spawned >= 9) {
                Vu0SetOrigin(&entry->drift.scale);
                entry->drift.scale.x = 1.0f;
                entry->drift.scale.y = 1.0f;
                entry->drift.scale.z = 1.0f;
                entry->drift.scale.w = 0.032f;
            } else {
                entry->drift.scale.x = 0.75f;
                entry->drift.scale.y = 3.0f;
                entry->drift.scale.z = 0.75f;
            }
            entry->drift.growth.x = sefNoiseNext() * params->size + 1.0f;
            entry->drift.growth.y = sefNoiseNext() * params->size + 1.0f;
            entry->drift.growth.z = sefNoiseNext() * params->size + 1.0f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            direction.y += 1.5f;
            SefVelocityTowards(&entry->drift.velocity, &direction, position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = sefNoiseNext() * 0.025f + 0.15f;
            entry->drift.landed = 0;
        }
        entry->drift.position.y += 0.5f;
        entry->drift.position.w = 1.0f;
        entry->drift.red = entry->drift.green = entry->drift.blue = params->shade;
        entry->drift.alpha = params->alpha;
        entry->drift.life = params->life;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.axisOffset = 0;
        spawned++;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern SefEmitterParams sefEmitterParamsKinds11To13[];

/* Emitters of effect kinds 11 to 13 (parameter rows of five per kind; sefCreateKindResourceSlot creates three
   emitters):
 * each spawn emits up to count particles while free entries last, until lastFrame. */
void sefSpawnKinds11To13(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    SefEmitterParams *params;
    AlignedVector4 direction;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int row;
    int lastFrame;

    due = 0;
    emitter = &slot->body.emitters[index];
    row = slot->body.kind - 11;
    params = &sefEmitterParamsKinds11To13[index + row * 5];
    frame = emitter->elapsed;
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        emitter->particles[cursor] = id;
        sefGetRandomScaledDirection(&entry->drift.position, params->spread);
        entry->drift.growth.w = 1.0f;
        entry->drift.scale.w = params->scaleW;
        if (index != 2) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z =
                sefNoiseNext() * 0.5f + 1.0f;
        } else {
            sefRandomRotateMatrix(entry->drift.rotation);
            entry->drift.scale.x = 2.0f;
            entry->drift.scale.y = 5.0f;
            entry->drift.scale.z = 2.0f;
            entry->drift.growth.x = sefNoiseNext() * 1.5f + 1.0f;
            entry->drift.growth.y = sefNoiseNext() * 1.5f + 1.0f;
            entry->drift.growth.z = sefNoiseNext() * 1.5f + 1.0f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            direction.y += 1.5f;
            Vu0SefSubVectorXYZ(&entry->drift.velocity, &direction, &entry->drift.position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = sefNoiseNext() * 0.05f + 0.2f;
            entry->drift.landed = 0;
        }
        entry->drift.position.y += 0.25f;
        entry->drift.position.w = 1.0f;
        entry->drift.red = entry->drift.green = entry->drift.blue = params->shade;
        entry->drift.alpha = params->alpha;
        entry->drift.life = params->life;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.axisOffset = 0;
        spawned++;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern SefEmitterParams sefEmitterParamsKinds22Up[];

/* Emitters 1 and 2 of these kinds add xglSRand() & SEF_SHADE_TINT_MASK, 0 to 31, to the shade. */
#define SEF_SHADE_TINT_MASK 0x1F

/* Emitters of effect kinds 22 and up (parameter rows of four per kind): like sefSpawnKinds8To10 with its emitters 1
   and 2 facing a random
   angle, adding a random tint to the shade and emitter 2 shrinking its particles. */
void sefSpawnKinds22Up(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 *position;
    SefEmitterParams *params;
    AlignedVector4 direction;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int row;
    int lastFrame;
    int tint;

    due = 0;
    emitter = &slot->body.emitters[index];
    row = slot->body.kind - 22;
    params = &sefEmitterParamsKinds22Up[index + row * 4];
    frame = emitter->elapsed;
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    tint = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        position = &entry->drift.position;
        emitter->particles[cursor] = id;
        sefGetRandomScaledDirection(position, params->spread);
        entry->drift.scale.w = params->scaleW;
        entry->drift.growth.w = 1.0f;
        if (index == 0 || index == 3) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z =
                sefNoiseNext() * 0.5f + 1.0f;
        } else {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            tint = xglSRand() & SEF_SHADE_TINT_MASK;
            if (index == 2) {
                Vu0SetOrigin(&entry->drift.scale);
                entry->drift.scale.x = 1.0f;
                entry->drift.scale.y = 1.0f;
                entry->drift.scale.z = 1.0f;
                entry->drift.scale.w = 0.025f;
            } else {
                entry->drift.scale.x = 1.5f;
                entry->drift.scale.y = 1.5f;
                entry->drift.scale.z = 1.5f;
            }
            entry->drift.growth.x = sefNoiseNext() * params->size + 1.0f;
            entry->drift.growth.y = sefNoiseNext() * params->size + 1.0f;
            entry->drift.growth.z = sefNoiseNext() * params->size + 1.0f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            direction.y += 1.5f;
            SefVelocityTowards(&entry->drift.velocity, &direction, position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = sefNoiseNext() * 0.05f + 0.21f;
            entry->drift.landed = 0;
        }
        entry->drift.position.y += 0.5f;
        entry->drift.position.w = 1.0f;
        entry->drift.red = entry->drift.green = entry->drift.blue = params->shade + tint;
        entry->drift.alpha = params->alpha;
        entry->drift.life = params->life;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.axisOffset = 0;
        spawned++;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern SefEmitterParams sefEmitterParamsKinds14To16[];

/* Emitters of effect kinds 14 to 16 (parameter rows of five per kind): 0 to 2 fly outwards along their start position,
   3 and 4 fly
   towards a random point. */
void sefSpawnKinds14To16(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 *position;
    SefEmitterParams *params;
    AlignedVector4 direction;
    float spread;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int row;
    int lastFrame;

    due = 0;
    emitter = &slot->body.emitters[index];
    row = slot->body.kind - 14;
    params = &sefEmitterParamsKinds14To16[index + row * 5];
    frame = emitter->elapsed;
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        position = &entry->drift.position;
        emitter->particles[cursor] = id;
        sefGetRandomScaledDirection(position, params->spread);
        entry->drift.scale.w = params->scaleW;
        entry->drift.growth.w = 1.0f;
        entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
        if (index < 3) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z =
                sefNoiseNext() * 0.5f + 1.0f;
            entry->drift.red = 0x50;
            entry->drift.green = 0x50;
            entry->drift.blue = 0x5A;
            if (index != 0) {
                entry->drift.speed = sefNoiseNext() * 0.005f + 0.005f;
            } else {
                entry->drift.speed = 0.001f;
            }
            MMathNormalizeVector2(&entry->drift.velocity, position);
            Vu0SefScaleVectorXYZNI(&entry->drift.velocity, &entry->drift.velocity, entry->drift.speed);
        } else {
            if (index == 3) {
                spread = 1.5f;
            } else {
                spread = 1.0f;
            }
            sefRandomRotateMatrix(entry->drift.rotation);
            entry->drift.growth.x = sefNoiseNext() * spread + 1.0f;
            entry->drift.growth.y = sefNoiseNext() * spread + 1.0f;
            entry->drift.growth.z = sefNoiseNext() * spread + 1.0f;
            entry->drift.red = entry->drift.green = entry->drift.blue = params->shade;
            entry->drift.landed = 0;
            entry->drift.speed = sefNoiseNext() * 0.01f + 0.01f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            SefVelocityTowards(&entry->drift.velocity, &direction, position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
        }
        entry->drift.position.y += 0.25f;
        entry->drift.alpha = params->alpha;
        entry->drift.axisOffset = 0;
        entry->drift.position.w = 1.0f;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.life = params->life;
        spawned++;
        if (spawned >= params->count) {
            break;
        }
    }
}

extern SefEmitterParams sefEmitterParamsKinds17To19[];

/* Emitters of effect kinds 17 to 19 (parameter rows of five per kind, sefEmitterParamsKinds17To19 indexed from kind
   0): 0 and 1 start on a cylinder,
   2 at a random offset, 3 and 4 on a cylinder and fly towards a random point. */
void sefSpawnKinds17To19(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    SefEmitterParams *params;
    AlignedVector4 direction;
    int frame;
    int due;
    int mask;
    int cursor;
    int last;
    int id;
    int spawned;
    int lastFrame;

    due = 0;
    emitter = &slot->body.emitters[index];
    params = &sefEmitterParamsKinds17To19[index + (slot->body.kind - 17) * 5];
    frame = emitter->elapsed;
    lastFrame = params->lastFrame;
    if (params->interval == 0 || frame % params->interval == 0) {
        due = 1;
    }
    if (emitter->frame == frame || lastFrame < frame) {
        return;
    }
    emitter->frame = frame;
    if (!due) {
        return;
    }
    spawned = 0;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] != 0) {
            continue;
        }
        id = sefPoolAlloc();
        if (id == 0) {
            break;
        }
        entry = &sefEntryPool.entries[id];
        emitter->particles[cursor] = id;
        entry->drift.scale.w = params->scaleW;
        entry->drift.growth.w = 1.0f;
        if (index == 2) {
            sefGetRandomScaledDirection(&entry->drift.position, params->spread);
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z = sefNoiseNext() + 1.0f;
            entry->drift.position.y = 0.2f;
        } else if (index == 1) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            sefGetCylinderPos(&entry->drift.position, 0.2f, 0.0f);
            entry->drift.position.y += 0.5f;
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z = sefNoiseNext() + 1.0f;
            entry->drift.velocity.x = sefNoiseNext() * 4.0f;
            entry->drift.velocity.y = sefRandf() * 2.0f;
            entry->drift.velocity.z = sefRandf() * 4.0f;
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = 0.06f;
        } else if (index == 0) {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            sefGetCylinderPos(&entry->drift.position, 0.2f, 0.0f);
            entry->drift.position.y += 0.5f;
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            entry->drift.growth.x = entry->drift.growth.y = entry->drift.growth.z = sefNoiseNext() + 1.0f;
        } else {
            sefRotationZRows(entry->drift.rotation, sefRandAngle());
            entry->drift.scale.x = entry->drift.scale.y = entry->drift.scale.z = params->size;
            if (index == 3) {
                entry->drift.growth.x = sefNoiseNext() + 1.0f;
                entry->drift.growth.y = sefNoiseNext() + 1.0f;
                entry->drift.growth.z = sefNoiseNext() + 1.0f;
                entry->drift.red = entry->drift.green = entry->drift.blue = 0x80;
            } else {
                entry->drift.growth.y = entry->drift.growth.z = entry->drift.growth.x = sefNoiseNext() + 1.0f;
                entry->drift.red = entry->drift.green = entry->drift.blue = 0x28;
            }
            sefGetCylinderPos(&entry->drift.position, 0.2f, 0.0f);
            entry->drift.position.y = 0.1f;
            sefGetRandomScaledDirection(&direction, -2.0f);
            direction.y += 1.0f;
            direction.z += 2.0f;
            Vu0SefSubVectorXYZ(&entry->drift.velocity, &direction, &entry->drift.position);
            MMathNormalizeVector2(&entry->drift.velocity, &entry->drift.velocity);
            entry->drift.speed = sefRandf() * 0.05f + 0.2f;
            entry->drift.landed = 0;
        }
        entry->drift.position.w = 1.0f;
        if (index < 3) {
            entry->drift.red = entry->drift.green = entry->drift.blue = params->shade;
        }
        entry->drift.alpha = params->alpha;
        if (slot->body.kind == 19) {
            entry->drift.position.z += emitter->elapsed * 0.2f;
        }
        spawned++;
        entry->drift.life = params->life;
        entry->drift.age = 0;
        entry->drift.verticalSpeed = 0;
        entry->drift.axisOffset = 0;
        if (spawned >= params->count) {
            break;
        }
    }
}

/* Per emitter of effect kind 20: the spawn interval in frames (also the particle life) and the particle size. */
extern short sefKind20Intervals[];
extern float sefKind20Sizes[];

/* Emitters of effect kind 20: one particle every interval frames; emitter 1 starts it orange-red and transparent. */
void sefSpawnKind20(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    int frame;
    int mask;
    int cursor;
    int last;
    int id;
    int due;

    emitter = &slot->body.emitters[index];
    frame = emitter->elapsed;
    due = frame % sefKind20Intervals[index] == 0;
    if (emitter->frame != frame) {
        emitter->frame = frame;
        if (!due) {
            return;
        }
        mask = slot->body.particleCount - 1;
        last = (emitter->cursor - 1) & mask;
        for (cursor = emitter->cursor; cursor != last; cursor++) {
            cursor &= mask;
            if (emitter->particles[cursor] == 0) {
                id = sefPoolAlloc();
                if (id == 0) {
                    break;
                }
                entry = &sefEntryPool.entries[id];
                emitter->particles[cursor] = id;
                sefSetOnes(&entry->spin.scale);
                sefSetOnes(&entry->spin.growth);
                entry->spin.scale.w = sefKind20Sizes[index];
                Vu0SetOrigin(&entry->spin.position);
                sefRotationZRows(entry->spin.rotation, 0.0f);
                if (index == 1) {
                    entry->spin.red = 200;
                    entry->spin.green = 60;
                    entry->spin.blue = 40;
                    entry->spin.alpha = 0;
                } else {
                    entry->spin.alpha = entry->spin.red = entry->spin.green = entry->spin.blue = 0x80;
                }
                entry->spin.life = sefKind20Intervals[index];
                entry->spin.angle = 0.0f;
                entry->spin.age = 0.0f;
                entry->spin.verticalSpeed = 0.0f;
                entry->spin.axisOffset = 0;
                break;
            }
        }
    }
}

/* Emitters of effect kind 21: tries to spawn one particle on the first whole frame handled. The one-shot latch is set
 * before the search, so a failed search or allocation is not retried. */
void sefSpawnKind21(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    int frame;
    int mask;
    int cursor;
    int last;
    int id;

    emitter = &slot->body.emitters[index];
    frame = emitter->elapsed;
    if (emitter->frame == frame) {
        return;
    }
    emitter->frame = frame;
    if (emitter->spawned != 0) {
        return;
    }
    emitter->spawned = 1;
    mask = slot->body.particleCount - 1;
    last = (emitter->cursor - 1) & mask;
    for (cursor = emitter->cursor; cursor != last; cursor++) {
        cursor &= mask;
        if (emitter->particles[cursor] == 0) {
            id = sefPoolAlloc();
            if (id == 0) {
                break;
            }
            entry = &sefEntryPool.entries[id];
            emitter->particles[cursor] = id;
            sefSetOnes(&entry->spin.scale);
            sefSetOnes(&entry->spin.growth);
            if (index != 0) {
                entry->spin.scale.w = 0.32f;
            } else {
                entry->spin.scale.w = 0.64f;
            }
            Vu0SetOrigin(&entry->spin.position);
            sefRotationZRows(entry->spin.rotation, 0.0f);
            entry->spin.red = 200;
            entry->spin.life = 10.0f;
            entry->spin.axisOffset = 0;
            entry->spin.blue = 0x80;
            entry->spin.green = 0x80;
            entry->spin.angle = 0.0f;
            entry->spin.age = 0.0f;
            entry->spin.verticalSpeed = 0.0f;
            entry->spin.alpha = 0x80;
            break;
        }
    }
}

/* destination.xyz = from + (to - from) * t through vf1..vf3 (the w of to is replaced by 1 first). */
#define SefLerpVectorXYZ(destination, from, to, t) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%1)\n" \
        "lqc2 vf2, 0(%2)\n" \
        "mfc1 $8, %3\n" \
        "vmove.w vf2, vf0\n" \
        "qmtc2.ni $8, vf3\n" \
        "vsub.xyz vf2, vf2, vf1\n" \
        "vmulx.xyz vf2, vf2, vf3x\n" \
        "vadd.xyz vf2, vf2, vf1\n" \
        "sqc2 vf2, 0(%0)\n" \
        : \
        : "r" (destination), "r" (from), "r" (to), "f" (t) \
        : "$8", "memory")

/* Group update of effect kinds 0 to 2 (sefUpdateHandlers): ages the live particles and moves each by a step that turns
 * from velocity towards target after frame 20 and lengthens with age; the particles grow (fastest up to frame 10),
 * fade in until frame 10 and out from frame 60. */
void sefUpdateBurstParticles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 step;
    unsigned short id;
    f32 age;
    int i;

    emitter = &slot->body.emitters[index];
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->fade.age < entry->fade.life) {
                entry->fade.age = entry->fade.age + 1.0f;
                emitter->liveCount++;
                if (entry->fade.age > 20.0f) {
                    SefLerpVectorXYZ(&step, &entry->fade.velocity, &entry->fade.target,
                                     (entry->fade.age - 20.0f) * 0.01f);
                } else {
                    QuadwordCopyT0NoClobber(&step, &entry->fade.velocity);
                }
                Vu0SefScaleVectorXYZNI(&step, &step, entry->fade.age * 0.0008f + 0.025f);
                Vu0SefAddVectorXYZ(&entry->fade.position, &entry->fade.position, &step);
                age = entry->fade.age;
                if (age <= 10.0f) {
                    entry->fade.scale.x += 0.05f;
                } else if (age < 60.0f) {
                    entry->fade.scale.x += 0.03f;
                } else {
                    entry->fade.scale.x += 0.017f;
                }
                entry->fade.scale.z = entry->fade.scale.y = entry->fade.scale.x;
                if (age < 10.0f) {
                    entry->fade.alpha += 25;
                } else if (!(age < 60.0f)) {
                    entry->fade.alpha -= 4;
                    if (entry->fade.alpha < 0) {
                        entry->fade.alpha = 0;
                    }
                }
            } else {
                entry->fade.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

/* The lift of effect kinds 3 and 4 per frame, added to verticalSpeed: kind 3's two emitters, then kind 4's. */
extern f32 sefKinds3To4Lift[];

/* Group update of effect kinds 3 and 4 (sefUpdateHandlers): ages the live particles, moves each along velocity and
 * up by its growing verticalSpeed; emitter 0's grow, the others slide out along their first axis; green and blue
 * drop (the particle reddens) while alpha fades in until frame 5 and out after it. The motion goes through the pool
 * entry's union; the look is changed through the kind's member (one more pointer copy inside the loop, which the
 * loop pass needs to leave the axis step's load behind the 15.0f one). */
void sefUpdateKinds3To4Particles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    SefFadeParticle *particle;
    unsigned short id;
    f32 gravity;
    int row;
    int i;

    emitter = &slot->body.emitters[index];
    row = index;
    if (slot->body.kind != 3) {
        row = index + 2;
    }
    gravity = sefKinds3To4Lift[row];
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->fade.age < entry->fade.life) {
                entry->fade.age = entry->fade.age + 1.0f;
                emitter->liveCount++;
                entry->fade.verticalSpeed += gravity;
                sefAddVector(&entry->fade.position, &entry->fade.position, &entry->fade.velocity);
                entry->fade.position.y += entry->fade.verticalSpeed;
                particle = &entry->fade;
                if (index == 0) {
                    particle->scale.x += 0.04f;
                    particle->scale.z = particle->scale.y = particle->scale.x;
                } else {
                    particle->axisOffset += 0.39999998f; /* 0x3ECCCCCC, one step under 0.4f */
                }
                if (index != 0) {
                    if (particle->age <= 10.0f) {
                        particle->green -= 3;
                        particle->blue -= 5;
                    } else {
                        particle->green -= 8;
                        particle->blue -= 4;
                    }
                } else if (particle->age < 15.0f) {
                    particle->green -= 2;
                    particle->blue -= 3;
                } else {
                    particle->green -= 12;
                    particle->blue -= 7;
                }
                if (particle->age <= 5.0f) {
                    particle->alpha += 26;
                } else if (particle->age <= 15.0f) {
                    particle->alpha -= 5;
                } else {
                    particle->alpha -= 16;
                    if (particle->alpha < 0) {
                        particle->alpha = 0;
                    }
                }
            } else {
                entry->fade.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

/* Group update of effect kinds 5-7 (sefUpdateHandlers): ages the live particles,
 * grows them and fades them out; odd emitters also brighten. */
void sefUpdateFadeOutParticles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    unsigned short id;
    int i;

    emitter = &slot->body.emitters[index];
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->fade.age < entry->fade.life) {
                entry->fade.age = entry->fade.age + 1.0f;
                emitter->liveCount++;
                if (index & 1) {
                    if (slot->body.kind == 5) {
                        if (entry->fade.age <= 10.0f) {
                            entry->fade.scale.x += 0.025f;
                        } else {
                            entry->fade.scale.x += 0.02f;
                        }
                    } else if (slot->body.kind == 6) {
                        if (entry->fade.age <= 10.0f) {
                            entry->fade.scale.x += 0.03f;
                        } else {
                            entry->fade.scale.x += 0.02f;
                        }
                    } else if (entry->fade.age <= 10.0f) {
                        entry->fade.scale.x += 0.07f;
                    } else {
                        entry->fade.scale.x += 0.033f;
                    }
                    entry->fade.scale.z = entry->fade.scale.y = entry->fade.scale.x;
                    entry->fade.green = entry->fade.blue = entry->fade.red = entry->fade.red + 1;
                    if (entry->fade.age <= 10.0f) {
                        entry->fade.alpha += 25;
                    } else if (entry->fade.age > 20.0f) {
                        entry->fade.alpha -= 12;
                    }
                    if (entry->fade.alpha < 0) {
                        entry->fade.alpha = 0;
                    }
                } else {
                    if (entry->fade.age <= 5.0f) {
                        entry->fade.scale.x += 0.16f;
                    } else {
                        entry->fade.scale.x += 0.06f;
                    }
                    entry->fade.scale.z = entry->fade.scale.y = entry->fade.scale.x;
                    if (entry->fade.age >= 10.0f) {
                        entry->fade.green -= 2;
                        entry->fade.blue -= 4;
                        entry->fade.alpha -= 6;
                        if (entry->fade.alpha < 0) {
                            entry->fade.alpha = 0;
                        }
                    }
                }
            } else {
                entry->fade.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

/* Group update of effect kinds 8 to 10 and 22 to 24 (sefUpdateHandlers): ages the live particles. Emitters 0 and 3
 * grow, brighten on odd frames and fade from frame 15. The others fly along velocity at a falling speed and drop
 * under gravity (twice as strong for kinds 22 to 24) until they reach sefFloorHeight, where they keep half of
 * their step as velocity and bounce; they fade from frame 20. */
void sefUpdateKinds8To10Particles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 step;
    unsigned short id;
    f32 gravity;
    f32 floorY;
    int i;

    emitter = &slot->body.emitters[index];
    floorY = sefFloorHeight.height;
    emitter->liveCount = 0;
    gravity = (slot->body.kind >= 22 && slot->body.kind <= 24) ? 0.004f : 0.002f;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->drift.age < entry->drift.life) {
                entry->drift.age = entry->drift.age + 1.0f;
                emitter->liveCount++;
                if (index == 0 || index == 3) {
                    if (entry->drift.age <= 3.0f) {
                        entry->drift.scale.x += 0.125f;
                    } else {
                        entry->drift.scale.x += 0.037f;
                    }
                    entry->drift.scale.z = entry->drift.scale.y = entry->drift.scale.x;
                    if ((int) entry->drift.age & 1) {
                        entry->drift.green = entry->drift.blue = entry->drift.red = entry->drift.red + 1;
                    }
                    if (entry->drift.age > 15.0f) {
                        entry->drift.alpha -= 10;
                        if (entry->drift.alpha < 0) {
                            entry->drift.alpha = 0;
                        }
                    }
                } else {
                    if (entry->drift.landed != 0) {
                        entry->drift.velocity.y -= gravity;
                        sefAddVector(&entry->drift.position, &entry->drift.position, &entry->drift.velocity);
                    } else {
                        if (entry->drift.age < 10.0f) {
                            entry->drift.speed -= 0.012f;
                        } else {
                            entry->drift.speed -= 0.002f;
                        }
                        sefScaleVector(&step, &entry->drift.velocity, entry->drift.speed);
                        entry->drift.verticalSpeed -= gravity;
                        sefAddVector(&entry->drift.position, &entry->drift.position, &step);
                        entry->drift.position.y += entry->drift.verticalSpeed;
                    }
                    if (entry->drift.position.y <= floorY) {
                        if (entry->drift.landed == 0) {
                            sefCopyVector(&entry->drift.velocity, &step);
                            entry->drift.velocity.y += entry->drift.verticalSpeed;
                            sefScaleVector(&entry->drift.velocity, &entry->drift.velocity, 0.5f);
                        }
                        entry->drift.landed = 1;
                        if (entry->drift.velocity.y < 0.0f) {
                            entry->drift.velocity.y = -entry->drift.velocity.y;
                        }
                    }
                    if (entry->drift.age > 20.0f) {
                        entry->drift.alpha -= 13;
                        if (entry->drift.alpha < 0) {
                            entry->drift.alpha = 0;
                        }
                    }
                }
            } else {
                entry->drift.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

/* Group update of effect kinds 11 to 13 (sefUpdateHandlers): ages the live particles. Emitters 0 and 3 grow and
 * redden as they fade, emitters 1 and 4 grow, brighten and fade from frame 15. The others shrink while they fly
 * along velocity at a falling speed and drop under gravity until they reach sefFloorHeight, where they keep half
 * of their step as velocity and bounce; they fade from frame 20. */
void sefUpdateKinds11To13Particles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 step;
    unsigned short id;
    f32 floorY;
    f32 speed;
    int i;

    emitter = &slot->body.emitters[index];
    floorY = sefFloorHeight.height;
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->drift.age < entry->drift.life) {
                entry->drift.age = entry->drift.age + 1.0f;
                emitter->liveCount++;
                if (index == 0 || index == 3) {
                    if (entry->drift.age <= 5.0f) {
                        entry->drift.scale.x += 0.32f;
                    } else {
                        entry->drift.scale.x += 0.1f;
                    }
                    entry->drift.scale.z = entry->drift.scale.y = entry->drift.scale.x;
                    if (entry->drift.age > 2.0f) {
                        entry->drift.green -= 6;
                        entry->drift.blue -= 11;
                    }
                    if (entry->drift.age < 2.0f) {
                        entry->drift.alpha -= 128;
                    } else {
                        entry->drift.alpha -= 12;
                        if (entry->drift.alpha < 0) {
                            entry->drift.alpha = 0;
                        }
                    }
                } else if (index == 1 || index == 4) {
                    if (entry->drift.age <= 3.0f) {
                        entry->drift.scale.x += 0.25f;
                    } else {
                        entry->drift.scale.x += 0.037f;
                    }
                    entry->drift.scale.z = entry->drift.scale.y = entry->drift.scale.x;
                    entry->drift.red++;
                    entry->drift.green++;
                    entry->drift.blue++;
                    if (entry->drift.age >= 15.0f) {
                        entry->drift.alpha -= 17;
                        if (entry->drift.alpha < 0) {
                            entry->drift.alpha = 0;
                        }
                    }
                } else {
                    if (entry->drift.age < 4.0f) {
                        entry->drift.scale.y -= 1.0f;
                    } else {
                        entry->drift.scale.z = entry->drift.scale.y = entry->drift.scale.x =
                            entry->drift.scale.x - 0.058f;
                    }
                    if (entry->drift.landed != 0) {
                        entry->drift.velocity.y -= 0.002f;
                        sefAddVector(&entry->drift.position, &entry->drift.position, &entry->drift.velocity);
                    } else {
                        speed = entry->drift.speed - 0.006f;
                        entry->drift.speed = speed;
                        sefScaleVector(&step, &entry->drift.velocity, speed);
                        entry->drift.verticalSpeed -= 0.002f;
                        sefAddVector(&entry->drift.position, &entry->drift.position, &step);
                        entry->drift.position.y += entry->drift.verticalSpeed;
                    }
                    if (entry->drift.position.y <= floorY) {
                        if (entry->drift.landed == 0) {
                            sefCopyVector(&entry->drift.velocity, &step);
                            entry->drift.velocity.y += entry->drift.verticalSpeed;
                            sefScaleVector(&entry->drift.velocity, &entry->drift.velocity, 0.5f);
                        }
                        entry->drift.landed = 1;
                        if (entry->drift.velocity.y < 0.0f) {
                            entry->drift.velocity.y = -entry->drift.velocity.y;
                        }
                    }
                    if (entry->drift.age > 20.0f) {
                        entry->drift.alpha -= 13;
                        if (entry->drift.alpha < 0) {
                            entry->drift.alpha = 0;
                        }
                    }
                }
            } else {
                entry->drift.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

/* Group update of effect kinds 14 to 16 (sefUpdateHandlers): ages the live particles. Emitters 0 and 1 drift along
 * velocity and sink while they brighten and fade (emitter 1 also pulses its x and y scales in a 10-frame cycle),
 * emitter 2 grows, brightens and fades in place. The others (emitter 4 shrinking) fly along velocity at a falling
 * speed and drop under gravity until they reach sefFloorHeight, where they keep a tenth of their step as velocity
 * and bounce; they fade from frame 20. */
void sefUpdateKinds14To16Particles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    AlignedVector4 step;
    unsigned short id;
    f32 floorY;
    int phase;
    int i;

    emitter = &slot->body.emitters[index];
    floorY = sefFloorHeight.height;
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->drift.age < entry->drift.life) {
                entry->drift.age = entry->drift.age + 1.0f;
                emitter->liveCount++;
                if (index < 2) {
                    entry->drift.verticalSpeed -= 0.001f;
                    sefAddVector(&entry->drift.position, &entry->drift.position, &entry->drift.velocity);
                    entry->drift.position.y += entry->drift.verticalSpeed;
                    if (index != 0) {
                        phase = (int) entry->drift.age % 10;
                        if (phase < 2) {
                            entry->drift.scale.x -= 0.5f;
                        } else if (phase < 4) {
                            entry->drift.scale.x += 0.5f;
                            entry->drift.scale.y -= 0.5f;
                        } else if (phase < 6) {
                            entry->drift.scale.y += 0.5f;
                        } else if (phase < 8) {
                            entry->drift.scale.x -= 0.5f;
                        } else {
                            entry->drift.scale.x += 0.5f;
                        }
                        entry->drift.red = 100;
                        entry->drift.green = 100;
                        entry->drift.blue = 110;
                        entry->drift.alpha -= 7;
                    } else {
                        if (entry->drift.age <= 3.0f) {
                            entry->drift.scale.y = entry->drift.scale.z = entry->drift.scale.x =
                                entry->drift.scale.x + 0.125f;
                        } else {
                            entry->drift.scale.y = entry->drift.scale.z = entry->drift.scale.x =
                                entry->drift.scale.x + 0.055f;
                        }
                        entry->drift.alpha -= 4;
                    }
                    entry->drift.red++;
                    entry->drift.green++;
                    entry->drift.blue++;
                    if (entry->drift.alpha < 0) {
                        entry->drift.alpha = 0;
                    }
                } else if (index == 2) {
                    if (entry->drift.age <= 3.0f) {
                        entry->drift.scale.x += 0.125f;
                    } else {
                        entry->drift.scale.x += 0.037f;
                    }
                    entry->drift.scale.z = entry->drift.scale.y = entry->drift.scale.x;
                    entry->drift.red++;
                    entry->drift.green++;
                    entry->drift.blue++;
                    entry->drift.alpha -= 4;
                    if (entry->drift.alpha < 0) {
                        entry->drift.alpha = 0;
                    }
                } else {
                    if (index == 4) {
                        entry->drift.scale.z = entry->drift.scale.y = entry->drift.scale.x =
                            entry->drift.scale.x - 0.0033f;
                    }
                    if (entry->drift.landed != 0) {
                        entry->drift.velocity.y -= 0.002f;
                        sefAddVector(&entry->drift.position, &entry->drift.position, &entry->drift.velocity);
                    } else {
                        entry->drift.speed -= 0.00016f;
                        sefScaleVector(&step, &entry->drift.velocity, entry->drift.speed * 3.0f);
                        entry->drift.verticalSpeed -= 0.002f;
                        sefAddVector(&entry->drift.position, &entry->drift.position, &step);
                        entry->drift.position.y += entry->drift.verticalSpeed;
                    }
                    if (entry->drift.position.y <= floorY) {
                        if (entry->drift.landed == 0) {
                            sefCopyVector(&entry->drift.velocity, &step);
                            entry->drift.velocity.y += entry->drift.verticalSpeed;
                            sefScaleVector(&entry->drift.velocity, &entry->drift.velocity, 0.1f);
                        }
                        entry->drift.landed = 1;
                        if (entry->drift.velocity.y < 0.0f) {
                            entry->drift.velocity.y = -entry->drift.velocity.y;
                        }
                    }
                    if (entry->drift.age > 20.0f) {
                        entry->drift.alpha -= 13;
                        if (entry->drift.alpha < 0) {
                            entry->drift.alpha = 0;
                        }
                    }
                }
            } else {
                entry->drift.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

/* Group update of effect kinds 17 to 19 (sefUpdateHandlers): ages the live particles. Emitter 1's slow down along
 * velocity, grow, brighten and fade out after frame 15; emitter 0's brighten, fade in until frame 10 and out after
 * frame 20; emitter 2's stay as spawned. The other emitters' particles squash and stretch over the first 30 frames,
 * fall with a slowing step until they reach sefFloorHeight: the first landing halves their velocity, a later one only
 * reverses a downward y velocity; they fade out after frame 20. */
void sefUpdateKinds17To19Particles(ResourceSlot *slot, int index) {
    AlignedVector4 move;
    AlignedVector4 step;
    SefEmitter *emitter;
    SefPoolEntry *entry;
    unsigned short id;
    f32 floorY;
    f32 speed;
    int phase;
    int i;

    emitter = &slot->body.emitters[index];
    floorY = sefFloorHeight.height;
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->drift.age < entry->drift.life) {
                entry->drift.age = entry->drift.age + 1.0f;
                emitter->liveCount++;
                if (index != 2) {
                    if (index == 1) {
                        entry->drift.speed -= 0.00125;
                        Vu0SefScaleVectorXYZNI(&move, &entry->drift.velocity, entry->drift.speed);
                        Vu0SefAddVectorXYZ(&entry->drift.position, &entry->drift.position, &move);
                        if (entry->drift.age <= 5.0f) {
                            entry->drift.scale.x += 0.1f;
                        } else {
                            entry->drift.scale.x += 0.0428f;
                        }
                        entry->drift.scale.z = entry->drift.scale.y = entry->drift.scale.x;
                        entry->drift.green = entry->drift.blue = entry->drift.red = entry->drift.red + 1;
                        if (entry->drift.age > 15.0f) {
                            entry->drift.alpha -= 10;
                            if (entry->drift.alpha < 0) {
                                entry->drift.alpha = 0;
                            }
                        }
                    } else if (index == 0) {
                        entry->drift.red++;
                        entry->drift.green++;
                        entry->drift.blue++;
                        if (entry->drift.age < 10.0f) {
                            entry->drift.alpha += 25;
                        } else if (entry->drift.age > 20.0f) {
                            entry->drift.alpha -= 25;
                            if (entry->drift.alpha < 0) {
                                entry->drift.alpha = 0;
                            }
                        }
                    } else {
                        phase = entry->drift.age;
                        if (phase < 10) {
                            entry->drift.scale.x -= 0.1f;
                        } else if (phase < 20) {
                            entry->drift.scale.x += 0.1f;
                            entry->drift.scale.y -= 0.1f;
                        } else if (phase < 30) {
                            entry->drift.scale.y += 0.1f;
                        }
                        if (entry->drift.landed != 0) {
                            entry->drift.velocity.y -= 0.001f;
                            Vu0SefAddVectorXYZ(&entry->drift.position, &entry->drift.position, &entry->drift.velocity);
                        } else {
                            speed = entry->drift.speed - 0.006f;
                            entry->drift.speed = speed;
                            sefScaleVector(&step, &entry->drift.velocity, speed);
                            entry->drift.verticalSpeed -= 0.001f;
                            sefAddVector(&entry->drift.position, &entry->drift.position, &step);
                            entry->drift.position.y += entry->drift.verticalSpeed;
                        }
                        if (entry->drift.position.y <= floorY) {
                            if (entry->drift.landed == 0) {
                                sefCopyVector(&entry->drift.velocity, &step);
                                entry->drift.velocity.y += entry->drift.verticalSpeed;
                                Vu0SefScaleVectorXYZNI(&entry->drift.velocity, &entry->drift.velocity, 0.5f);
                            }
                            entry->drift.landed = 1;
                            if (entry->drift.velocity.y < 0.0f) {
                                entry->drift.velocity.y = -entry->drift.velocity.y;
                            }
                        }
                        if (entry->drift.age > 20.0f) {
                            entry->drift.alpha -= 13;
                            if (entry->drift.alpha < 0) {
                                entry->drift.alpha = 0;
                            }
                        }
                    }
                }
            } else {
                entry->drift.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
}

/* Group update of effect kind 20 (sefUpdateHandlers): emitter 1 shifts the colour and
 * grows at ages 0 to 30 while alive, emitter 2 turns 6 degrees a frame and pulses its size. */
void sefUpdateKind20Particles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefPoolEntry *entry;
    unsigned short id;
    int i;

    emitter = &slot->body.emitters[index];
    emitter->liveCount = 1;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            entry = &sefEntryPool.entries[id];
            if (entry->spin.age < entry->spin.life) {
                if (index == 1) {
                    if (entry->spin.age <= 30.0f) {
                        entry->spin.green = 60 - (int) (entry->spin.age + entry->spin.age);
                        entry->spin.red = (int) (entry->spin.age * 1.667f) + 200;
                        entry->spin.blue = (int) (entry->spin.age * 1.333f) + 40;
                    }
                    if (entry->spin.age <= 16.0f) {
                        entry->spin.alpha += 8;
                    } else if (entry->spin.age <= 31.0f) {
                        entry->spin.alpha -= 8;
                    }
                    if (entry->spin.age <= 30.0f) {
                        entry->spin.scale.y = entry->spin.scale.z = entry->spin.scale.x =
                            entry->spin.scale.x + 0.0333f;
                    }
                }
                if (index == 2) {
                    entry->spin.angle -= 6.0f;
                    sefRotationZRows(entry->spin.rotation, entry->spin.angle * 0.017453292f);
                    if (entry->spin.age < 18.0f) {
                        entry->spin.scale.x += 0.0222f;
                    } else if (entry->spin.age < 22.0f) {
                        entry->spin.scale.x += 0.025f;
                    } else if (entry->spin.age < 25.0f) {
                        entry->spin.scale.x -= 0.2f;
                    } else if (entry->spin.age < 30.0f) {
                        entry->spin.scale.x += 0.02f;
                    } else if (entry->spin.age < 48.0f) {
                        entry->spin.scale.x += 0.0222f;
                    } else if (entry->spin.age < 52.0f) {
                        entry->spin.scale.x += 0.025f;
                    } else if (entry->spin.age < 55.0f) {
                        entry->spin.scale.x -= 0.2f;
                    } else if (entry->spin.age < 60.0f) {
                        entry->spin.scale.x += 0.02f;
                    }
                    entry->spin.scale.y = entry->spin.scale.x;
                }
                entry->spin.age = entry->spin.age + 1.0f;
            } else {
                entry->spin.life = 0.0f;
                emitter->particles[i] = 0;
            }
        }
    }
    emitter->elapsed += 1.0f;
    if (emitter->elapsed > 59.9f) {
        emitter->elapsed = 0.0f;
    }
}

/* Counts the live particles of the emitter; except for emitter 0 each one also turns 6 degrees (back to 0 once
 * at or past -359) and gets its matrix rebuilt. Then the emitter ages by one frame. */
void sefUpdateSpinParticles(ResourceSlot *slot, int index) {
    SefEmitter *emitter;
    SefSpinParticle *particle;
    int i;
    unsigned short id;

    emitter = &slot->body.emitters[index];
    emitter->liveCount = 0;
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id != 0) {
            particle = &sefEntryPool.entries[id].spin;
            if (index != 0) {
                particle->angle -= 6.0f;
                if (particle->angle <= -359.0f) {
                    particle->angle = 0.0f;
                }
                sefRotationZRows(particle->rotation, particle->angle * 0.017453292f);
            }
            emitter->liveCount++;
        }
    }
    emitter->elapsed += 1.0f;
}

/* A per-kind emitter handler, called with the slot and the emitter index. */
typedef void (*SefEmitterFunc)(ResourceSlot *slot, int index);

extern SefEmitterFunc sefSpawnHandlers[];
extern SefEmitterFunc sefUpdateHandlers[];

/* Runs the emitters of every active slot (or only of target when it is not null) and recounts sefActiveCount:
 * an emitter waits out its delay first, then runs its kind's spawn and group-update handlers. A slot left
 * without live particles is released unless it is paused or persistent. */
void sefRunEmitters(ResourceSlot *target) {
    ResourceSlot *slot;
    SefEmitter *emitter;
    int index;
    int emitterIndex;

    sefActiveCount = 0;
    sefPoolFull = 0;
    if (_sefBattleMode != 0) {
        return;
    }
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++) {
        slot = &sefResourceTable[index];
        if (slot->body.activePairs == 0) {
            continue;
        }
        if (target != 0 && target != slot) {
            continue;
        }
        if (slot->body.flags & RESOURCE_FLAG_SUSPENDED) {
            continue;
        }
        sefFloorHeight.height = (0.0f < slot->position.y) ? -slot->position.y : 0.0f;
        slot->body.liveCount = 0;
        for (emitterIndex = 0; emitterIndex < slot->body.emitterCount; emitterIndex++) {
            emitter = &slot->body.emitters[emitterIndex];
            if (0.0f < emitter->delay) {
                emitter->delay -= 1.0f;
            } else {
                if (!(slot->body.flags & RESOURCE_FLAG_PAUSED)) {
                    sefSpawnHandlers[slot->body.kind](slot, emitterIndex);
                }
                sefUpdateHandlers[slot->body.kind](slot, emitterIndex);
                slot->body.liveCount += emitter->liveCount;
            }
        }
        sefActiveCount += slot->body.liveCount;
        if (slot->body.liveCount == 0
            && !(slot->body.flags & (RESOURCE_FLAG_PERSISTENT | RESOURCE_FLAG_PAUSED))) {
            if (((slot->body.key >> 28) & 3) == 1) {
                uwamonoReleaseOwnedRecords((SefOwner *)slot);
            }
            sefReleaseCellSlot(slot);
        }
    }
}

/* Nonzero when x, y or z has any bit set: the original tests the raw words (x and y as one doubleword, z as a
 * word), so -0.0f counts as set. */
static inline short sefVectorXYZSet(const Vector4 *vector) {
    return (*(const int *)&vector->z | *(const u64 *)&vector->x) != 0;
}

/* Sets the VU packet up for one emitter of slot: with the slot matrix, or, when the slot has a position, rotation
 * or scale of its own, with that transform applied to it first (and the camera matrix scaled too); then TEST_1,
 * whose depth test is off (ZTST ALWAYS) under RESOURCE_FLAG_NO_DEPTH_TEST or for kinds 20 and 21, else GEQUAL.
 * tex0 and kind, the caller's copies of emitter->tex0 and slot->body.kind, are not read. */
void sefSetupEmitterPacket(u64 tex0, ResourceSlot *slot, short kind, int emitterIndex) {
    Matrix4 matrix;
    SefEmitter *emitter;
    short hasPosition;
    short hasRotation;
    short hasScale;

    emitter = &slot->body.emitters[emitterIndex];
    hasPosition = sefVectorXYZSet(&slot->position);
    hasRotation = sefVectorXYZSet(&slot->rotation);
    hasScale = sefVectorXYZSet(&slot->scale);
    if (hasPosition || hasRotation || hasScale) {
        sefLoadUnitMatrix(&matrix);
        if (hasPosition) {
            QuadwordCopyT0(matrix[3], &slot->position);
        }
        if (hasRotation) {
            MMathRotateMatrixXYZ(&matrix, &matrix, &slot->rotation);
        }
        if (hasScale) {
            MMathScaleMatrix(&matrix, &matrix, &slot->scale);
        }
        MMathMulMatrix(&matrix, &matrix, &slot->body.matrix);
        sefBuildModelPacket(emitter->tex0, &matrix, slot->body.kind, emitterIndex);
        MMathScaleMatrix(&sefVuPacket.cameraMatrix, &sefVuPacket.cameraMatrix, &slot->scale);
    } else {
        sefBuildModelPacket(emitter->tex0, &slot->body.matrix, slot->body.kind, emitterIndex);
    }
    if (slot->body.flags & RESOURCE_FLAG_NO_DEPTH_TEST) {
        sefVuPacket.test = GS_SET_TEST(1, 6, 0, 0, 0, 0, 0, 1);
    } else if (slot->body.kind == 20 || slot->body.kind == 21) {
        sefVuPacket.test = GS_SET_TEST(1, 6, 0, 0, 0, 0, 0, 1);
    } else {
        sefVuPacket.test = GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 2);
    }
}

/* sefDrawEmitter builds each particle in vf1..vf5: matrix rows 0 to 3, then the colour. */

/* vf15 = the slot's tint, kept for SefTintColour. */
#define SefLoadTint(tint) \
    __asm__ __volatile__("lqc2 vf15, 0(%0)\n" : : "r" (tint) : "memory")

/* vf1..vf4 = the four rows of matrix. */
#define SefLoadMatrixRows(matrix) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%0)\n" \
        "lqc2 vf2, 16(%0)\n" \
        "lqc2 vf3, 32(%0)\n" \
        "lqc2 vf4, 48(%0)\n" \
        : \
        : "r" (matrix) \
        : "memory")

/* vf5 = the particle's colour words; rows 0 to 2 scaled by scale.xyz * scale.w * growth.xyz (vf7). */
#define SefScaleParticleRows(particle) \
    __asm__ __volatile__( \
        "lqc2 vf6, 80(%0)\n" \
        "lqc2 vf5, 64(%0)\n" \
        "lqc2 vf8, 144(%0)\n" \
        "vmulw.xyz vf7, vf6, vf6w\n" \
        "vmul.xyz vf7, vf7, vf8\n" \
        "vmulx.xyzw vf1, vf1, vf7x\n" \
        "vmuly.xyzw vf2, vf2, vf7y\n" \
        "vmulz.xyzw vf3, vf3, vf7z\n" \
        : \
        : "r" (particle) \
        : "memory")

/* vf5 = vf5 * vf15 (the integer colour through float and back). */
#define SefTintColour() \
    __asm__ __volatile__( \
        "vitof0.xyzw vf5, vf5\n" \
        "vmul.xyzw vf5, vf5, vf15\n" \
        "vftoi0.xyzw vf5, vf5\n" \
        : : : "memory")

/* Row 3 (the position) += row 0 * offset. */
#define SefOffsetAlongRow0(offset) \
    __asm__ __volatile__( \
        "mfc1 $8, %0\n" \
        "qmtc2.ni $8, vf8\n" \
        "vmulax.xyzw ACC, vf1, vf8x\n" \
        "vmadday.xyzw ACC, vf2, vf0y\n" \
        "vmaddaz.xyzw ACC, vf3, vf0z\n" \
        "vmaddw.xyzw vf4, vf4, vf0w\n" \
        : \
        : "f" (offset) \
        : "$8", "memory")

/* Stores vf1..vf5 to the five quadwords at destination. */
#define SefStoreParticle(destination) \
    __asm__ __volatile__( \
        "sqc2 vf1, 0(%0)\n" \
        "sqc2 vf2, 16(%0)\n" \
        "sqc2 vf3, 32(%0)\n" \
        "sqc2 vf4, 48(%0)\n" \
        "sqc2 vf5, 64(%0)\n" \
        : \
        : "r" (destination) \
        : "memory")

/* Draws the live particles of emitter emitterIndex of slot: per particle five quadwords (its matrix with rows 0 to 2
 * scaled by its size, then its colour, tinted by slot->tint), 24 particles per full block (two header quadwords
 * plus 120 particle quadwords, 0x7A in all). The spinning emitters turn the matrix by the particle age first;
 * axisOffset moves it along row 0. */
void sefDrawEmitter(ResourceSlot *slot, int emitterIndex) {
    Matrix4 matrix;
    Vector4 angles;
    int drawn;
    SefEmitter *emitter;
    SefPoolEntry *entry;
    u8 *reserved;
    XglQuadword *packet;
    int qwc;
    int batch;
    int i;
    int alternate;
    int spinning;
    int tinted;
    int overhead;
    unsigned short id;

    qwc = 2;
    batch = 0;
    emitter = &slot->body.emitters[emitterIndex];
    drawn = 0;
    alternate = 0;
    if (slot->body.kind >= 8 && slot->body.kind <= 10) {
        alternate = emitterIndex == 1 || emitterIndex == 2;
    }
    spinning = alternate;
    if (slot->body.kind >= 14 && slot->body.kind <= 16 && emitterIndex >= 3) {
        alternate = 1;
        spinning = 1;
    } else if (slot->body.kind >= 11 && slot->body.kind <= 13 && emitterIndex == 2) {
        alternate = 1;
    }
    if (slot->body.visible == 0) {
        return;
    }
    if (emitter->liveCount <= 0) {
        if (emitterIndex == 0) {
            sefSetupEmitterPacket(emitter->tex0, slot, slot->body.kind, 0);
            sefBuildVuPacket(1, alternate);
        }
        return;
    }
    sefSetupEmitterPacket(emitter->tex0, slot, slot->body.kind, emitterIndex);
    if (emitterIndex != 0) {
        sefBuildVuPacket(2, alternate);
    } else {
        sefBuildVuPacket(1, alternate);
    }
    /* Two header quadwords per block of 24 particles, plus 8. */
    overhead = emitter->liveCount / 24 * 2 + 8;
    reserved = nmlPacketSetAttributeAlloc16N(emitter->liveCount * 5 + overhead);
    packet = (XglQuadword *) (reserved + SDV_UNCACHED);
    if (reserved == 0) {
        return;
    }
    sefUnpackBlockInit((SefUnpackBlock *) packet);
    if (slot->tint.x == 1.0f && slot->tint.y == 1.0f && slot->tint.z == 1.0f) {
        tinted = 0;
    } else {
        tinted = 1;
        SefLoadTint(&slot->tint);
    }
    for (i = 0; i < slot->body.particleCount; i++) {
        id = emitter->particles[i];
        if (id == 0) {
            continue;
        }
        entry = &sefEntryPool.entries[id];
        if (!(0.0f < entry->drift.life)) {
            continue;
        }
        if (spinning) {
            angles.x = angles.z = (entry->drift.age + entry->drift.age) * 0.017453292f;
            angles.y = entry->drift.age * 1.5f * 0.017453292f;
            /* Also leaves the turned rows in vf1..vf4 (its last sqc2 store them, m_math.c). */
            MMathRotateMatrixYXZ(&matrix, &entry->matrix, &angles);
        } else {
            SefLoadMatrixRows(&entry->matrix);
        }
        SefScaleParticleRows(entry);
        if (tinted) {
            SefTintColour();
        }
        if (entry->drift.axisOffset != 0.0f) {
            SefOffsetAlongRow0(entry->drift.axisOffset);
        }
        SefStoreParticle(&packet[qwc]);
        qwc += 5;
        batch++;
        drawn++;
        if ((unsigned int) qwc >= 0x79) {
            sefPacketUnpackRef((SefUnpackBlock *) packet, 0x28, qwc, batch);
            batch = 0;
            packet += qwc;
            qwc = 2;
            sefUnpackBlockInit((SefUnpackBlock *) packet);
        }
    }
    if (qwc >= 3) {
        sefPacketUnpackRef((SefUnpackBlock *) packet, 0x28, qwc, batch);
    }
    if (drawn != 0) {
        sefPacketMscnt();
    }
}

/* Sets the slot's position, rotation and scale (each only when given) and rebuilds its model matrix from them:
 * the unit matrix, rotated, scaled, then translated to the position. Nothing for a null slot. */
void sefSetSlotTransform(ResourceSlot *slot, Vector4 *position, Vector4 *rotation, Vector4 *scale) {
    if (slot != 0) {
        if (position != 0) {
            sefCopyVector(&slot->position, position);
        }
        if (rotation != 0) {
            sefCopyVector(&slot->rotation, rotation);
        }
        if (scale != 0) {
            sefCopyVector(&slot->scale, scale);
        }
        sefLoadUnitMatrix(&slot->body.matrix);
        if (rotation != 0) {
            MMathRotateMatrixXYZ(&slot->body.matrix, &slot->body.matrix, rotation);
        }
        if (scale != 0) {
            MMathScaleMatrix(&slot->body.matrix, &slot->body.matrix, scale);
        }
        if (position != 0) {
            sefCopyVector((AlignedVector4 *) slot->body.matrix[3], position);
        }
    }
}

extern SvDrawSlot svVisibleSlots[];
extern int svVisibleEntryCount;

/* The slot culling's scratchpad points: sefTransformClipPoint's output and input. */
typedef struct SefClipScratch {
    Vector4 clipPoint;  /* +0x00: the point in clip space */
    Vector4 worldPoint; /* +0x10: the slot origin in world space */
} SefClipScratch;

/* The scratchpad as sefCullResourceSlots sees it: a union (alias set device), so its accesses may alias the
 * visible list stores. */
typedef union SefScratchpad {
    SefClipScratch clip;
} SefScratchpad;

#define SEF_SCRATCHPAD ((SefScratchpad *) SCRATCHPAD_BASE)

/* Appends every active slot with live particles whose bounding sphere (radius 6, times scale.x when that is
 * above 1) is not clipped by the view volume to the visible list, with its clip-space depth. */
void sefCullResourceSlots(void) {
    ResourceSlot *slot;
    SvDrawSlot *visibleSlot;
    SefScratchpad *scratch;
    int count;
    int index;
    f32 radius;

    count = svVisibleEntryCount;
    visibleSlot = &svVisibleSlots[count];
    slot = sefResourceTable;
    scratch = SEF_SCRATCHPAD;
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++, slot++) {
        if (slot->body.activePairs != 0 && slot->body.liveCount > 0) {
            radius = 6.0f;
            sefAddVector(&scratch->clip.worldPoint, (AlignedVector4 *) slot->body.matrix[3], &slot->position);
            scratch->clip.worldPoint.w = 1.0f;
            sefTransformClipPoint(&scratch->clip.clipPoint, &scratch->clip.worldPoint);
            if (1.0f < slot->scale.x) {
                radius = slot->scale.x * radius;
            }
            if (sefClipViewVolumeA(&scratch->clip.worldPoint, radius) == 0) {
                visibleSlot->recordIndex = index;
                visibleSlot->disabled = 1;
                visibleSlot->depth = scratch->clip.clipPoint.z;
                count++;
                visibleSlot++;
            }
        }
    }
    svVisibleEntryCount = count;
}

ResourceSlot *sefGetResourceSlot(int index) {
    return &sefResourceTable[index];
}

/* Rebuilds the draw-state and VU packets, then draws every emitter of slot
 * slotIndex (one call per emitter, up to emitterCount); nothing when no particle
 * is active or slotIndex is outside the table. The packet is flushed when the
 * slot had an emitter. */
void sefDrawResources(int slotIndex) {
    ResourceSlot *slot;
    int emitterIndex;

    if (sefActiveCount <= 0) {
        return;
    }
    if ((unsigned int)slotIndex >= SEF_RESOURCE_SLOT_COUNT) {
        return;
    }
    sefLastDrawnKind = -1;
    svLoadDrawViewMatrix();
    svBuildDrawStatePacket();
    sefBuildCameraPacket();
    sefBuildVuPacket(0, 0);

    slot = &sefResourceTable[slotIndex];
    emitterIndex = 0;
    if (slot->body.emitterCount > 0) {
        do {
            slot->position.w = 1.0f;
            sefDrawEmitter(slot, emitterIndex);
            emitterIndex += 1;
            sefLastDrawnKind = slot->body.kind;
        } while (emitterIndex < slot->body.emitterCount);
    }
    if (sefLastDrawnKind >= 0) {
        sefPacketFlush();
    }
}

/* The original body is a bare return: no code between entry and return. */
void sefNoOp(void) {
}

/* Appends every active persistent slot to the saved list from entry count on, up to 64 entries (sefPushEffect
 * tail-calls it after collecting the effect records). */
void sefCollectPersistentSlots(SefSavedEffect *list, int count) {
    ResourceSlot *slot;
    int index;

    slot = sefResourceTable;
    for (index = 0; index < SEF_RESOURCE_SLOT_COUNT; index++, slot++) {
        if (slot->body.activePairs != 0 && count < 64 && (slot->body.flags & RESOURCE_FLAG_PERSISTENT)) {
            QuadwordCopyT0(&list[count].position, &slot->position);
            QuadwordCopyT0(&list[count].rotation, &slot->rotation);
            QuadwordCopyT0(&list[count].scale, &slot->scale);
            list[count].flags = slot->body.flags;
            list[count].owner = slot->body.owner;
            list[count].key = slot->body.key;
            list[count].object = slot->body.savedWord;
            list[count].gate = slot->body.visible;
            list[count].cellSlot = slot->body.resetFlag;
            list[count].slotIndex = index;
            count++;
        }
    }
}


/* Particle ring size (entries per emitter) of each effect kind. */
short sefRingSizes[28] = {
    32, 32, 32, 128, 128, 32, 32, 64,
    32, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 128, 16, 16, 64, 64,
    64, 0, 0, 0
};

SefBurstParams sefKinds0To2Params[4] = { { 4, 1, 0 }, { 4, 1, 0 }, { 4, 1, 0 }, { 4, 1, 0 } };
AlignedVector4 sefKinds0To2Origin = { 0.0f, 0.5f, 0.0f, 1.0f };
AlignedVector4 sefKinds0To2AimCenter = { 0.0f, 10.0f, 0.0f, 1.0f };
Matrix4 sefKinds0To2AimMatrix = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
int sefKinds0To2Shades[4] = { 80, 50, 25, 25 };
AlignedVector4 sefKinds3To4GrowthOffset = { 1.0f, 1.0f, 1.0f, 1.0f };
f32 sefKinds3To4Speeds[4] = { 0.01f, 0.01f, 0.005f, 0.005f };
f32 sefKinds3To4Counts[4] = { 3.0f, 3.0f, 3.0f, 2.0f };

SefEmitterParams sefEmitterParamsKinds5To7[12] = {
    { 2, 2, 30.0f, -0.3f, 0.3f, 0.22399999f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.4f, 0.75f, 0.512f, 25, 0, 10 },
    { 2, 2, 30.0f, -0.3f, 0.3f, 0.22399999f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.4f, 0.75f, 0.512f, 25, 0, 10 },
    { 2, 2, 30.0f, -0.3f, 0.3f, 0.24f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.4f, 0.85f, 0.544f, 25, 0, 10 },
    { 2, 2, 30.0f, -0.3f, 0.3f, 0.24f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.4f, 0.85f, 0.544f, 25, 0, 10 },
    { 2, 2, 30.0f, -0.4f, 0.3f, 0.24f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.7f, 0.4f, 0.41599998f, 25, 255, 10 },
    { 2, 2, 30.0f, -0.4f, 0.3f, 0.24f, 128, 128, 10 },
    { 2, 3, 40.0f, -0.7f, 0.4f, 0.41599998f, 25, 255, 16 }
};
SefEmitterParams sefEmitterParamsKinds8To10[12] = {
    { 2, 5, 30.0f, 1.2f, 0.5f, 0.768f, 64, 150, 2 },
    { 100, 8, 30.0f, 0.6f, 1.5f, 0.128f, 64, 150, 2 },
    { 100, 5, 30.0f, 0.6f, 1.0f, 0.128f, 64, 150, 2 },
    { 2, 4, 30.0f, 1.2f, 0.5f, 0.64f, 64, 150, 2 },
    { 2, 5, 30.0f, 1.4f, 0.5f, 0.89599997f, 64, 150, 4 },
    { 2, 10, 30.0f, 0.8f, 1.5f, 0.128f, 64, 150, 2 },
    { 2, 5, 30.0f, 0.8f, 1.0f, 0.128f, 64, 150, 4 },
    { 2, 4, 30.0f, 1.4f, 0.5f, 0.768f, 64, 150, 4 },
    { 2, 5, 30.0f, 1.6f, 0.5f, 0.96f, 64, 150, 4 },
    { 2, 10, 30.0f, 0.8f, 1.5f, 0.128f, 64, 150, 2 },
    { 2, 5, 30.0f, 0.8f, 1.0f, 0.128f, 64, 150, 4 },
    { 2, 4, 30.0f, 1.6f, 0.5f, 0.89599997f, 64, 150, 4 }
};

SefEmitterParams sefEmitterParamsKinds11To13[15] = {
    { 2, 3, 10.0f, 0.6f, 0.5f, 0.22399999f, 128, 192, 2 },
    { 2, 2, 30.0f, 0.8f, 0.6f, 0.576f, 25, 255, 2 },
    { 2, 5, 30.0f, 0.2f, 1.0f, 0.0448f, 40, 128, 2 },
    { 2, 3, 10.0f, 0.6f, 0.5f, 0.22399999f, 128, 192, 2 },
    { 2, 2, 30.0f, 0.8f, 0.6f, 0.576f, 25, 255, 2 },
    { 2, 4, 10.0f, 0.7f, 0.5f, 0.272f, 128, 192, 2 },
    { 2, 2, 30.0f, 1.0f, 0.6f, 0.64f, 25, 255, 2 },
    { 2, 5, 30.0f, 0.3f, 1.0f, 0.048f, 40, 128, 2 },
    { 2, 3, 10.0f, 0.7f, 0.5f, 0.272f, 128, 192, 2 },
    { 2, 3, 30.0f, 1.0f, 0.6f, 0.64f, 25, 255, 2 },
    { 2, 4, 10.0f, 0.85f, 0.5f, 0.32f, 128, 192, 2 },
    { 2, 3, 30.0f, 1.1f, 0.6f, 0.768f, 25, 255, 2 },
    { 2, 6, 30.0f, 0.4f, 1.0f, 0.0512f, 40, 128, 2 },
    { 2, 3, 10.0f, 0.85f, 0.5f, 0.32f, 128, 192, 2 },
    { 2, 3, 30.0f, 1.1f, 0.6f, 0.768f, 25, 255, 2 }
};

SefEmitterParams sefEmitterParamsKinds22Up[12] = {
    { 2, 5, 30.0f, 1.2f, 0.5f, 0.768f, 64, 150, 2 },
    { 2, 5, 30.0f, 0.2f, 1.0f, 0.0448f, 110, 128, 2 },
    { 2, 7, 30.0f, 0.9f, 1.0f, 0.016f, 120, 255, 3 },
    { 2, 4, 30.0f, 1.2f, 0.5f, 0.64f, 64, 150, 2 },
    { 2, 5, 30.0f, 1.4f, 0.5f, 0.89599997f, 64, 150, 4 },
    { 2, 5, 30.0f, 0.3f, 1.0f, 0.048f, 110, 128, 2 },
    { 2, 6, 30.0f, 0.8f, 1.0f, 0.0139999995f, 120, 255, 3 },
    { 2, 4, 30.0f, 1.4f, 0.5f, 0.768f, 64, 150, 4 },
    { 2, 5, 30.0f, 1.6f, 0.5f, 0.96f, 64, 150, 4 },
    { 2, 6, 30.0f, 0.4f, 1.0f, 0.0512f, 110, 128, 2 },
    { 2, 4, 30.0f, 0.6f, 1.0f, 0.012f, 120, 255, 3 },
    { 2, 4, 30.0f, 1.6f, 0.5f, 0.89599997f, 64, 150, 4 }
};

SefEmitterParams sefEmitterParamsKinds14To16[15] = {
    { 2, 4, 30.0f, 0.7f, 0.7f, 0.32f, 80, 128, 4 },
    { 2, 4, 30.0f, 0.6f, 1.0f, 0.012f, 90, 255, 3 },
    { 2, 8, 30.0f, 0.8f, 0.7f, 0.64f, 80, 128, 2 },
    { 2, 4, 30.0f, 0.3f, 2.4f, 0.048f, 128, 200, 2 },
    { 2, 6, 30.0f, 0.3f, 1.4f, 0.032f, 128, 200, 2 },
    { 2, 4, 30.0f, 0.8f, 0.7f, 0.352f, 80, 128, 4 },
    { 2, 6, 30.0f, 0.8f, 1.0f, 0.0139999995f, 90, 255, 3 },
    { 2, 10, 30.0f, 0.9f, 0.7f, 0.768f, 80, 128, 2 },
    { 2, 4, 30.0f, 0.7f, 2.4f, 0.055999998f, 128, 200, 2 },
    { 2, 5, 30.0f, 0.7f, 1.4f, 0.04f, 128, 200, 2 },
    { 2, 4, 30.0f, 0.9f, 0.7f, 0.41599998f, 80, 128, 4 },
    { 2, 7, 30.0f, 0.9f, 1.0f, 0.016f, 90, 255, 3 },
    { 2, 10, 30.0f, 1.0f, 0.7f, 0.89599997f, 80, 128, 2 },
    { 2, 4, 30.0f, 0.9f, 2.4f, 0.064f, 128, 200, 2 },
    { 2, 5, 30.0f, 0.9f, 1.4f, 0.048f, 128, 200, 2 }
};

SefEmitterParams sefEmitterParamsKinds17To19[15] = {
    { 100, 1, 40.0f, 0.2f, 0.5f, 0.32f, 25, 0, 2 },
    { 100, 4, 40.0f, 0.2f, 0.5f, 0.384f, 25, 255, 2 },
    { 100, 1, 2.0f, 0.0f, 1.0f, 0.384f, 128, 128, 2 },
    { 2, 1, 30.0f, 0.2f, 1.0f, 0.064f, 128, 128, 4 },
    { 2, 1, 30.0f, 0.2f, 1.0f, 0.096f, 128, 128, 4 },
    { 100, 1, 40.0f, 0.3f, 0.5f, 0.384f, 25, 0, 2 },
    { 2, 4, 40.0f, 0.2f, 0.5f, 0.384f, 25, 255, 4 },
    { 2, 1, 2.0f, 0.0f, 1.0f, 0.384f, 128, 128, 2 },
    { 2, 1, 30.0f, 0.2f, 1.0f, 0.064f, 128, 128, 4 },
    { 2, 1, 30.0f, 0.2f, 1.0f, 0.096f, 128, 128, 4 },
    { 100, 1, 40.0f, 0.5f, 0.8f, 0.41599998f, 25, 0, 2 },
    { 2, 4, 40.0f, 0.5f, 0.8f, 0.41599998f, 25, 255, 8 },
    { 2, 1, 2.0f, 0.0f, 1.0f, 0.48f, 128, 128, 8 },
    { 2, 2, 30.0f, 0.2f, 1.0f, 0.064f, 128, 128, 8 },
    { 2, 2, 30.0f, 0.2f, 1.0f, 0.096f, 128, 128, 8 }
};

short sefKind20Intervals[] = { 60, 30, 60, 0 };
float sefKind20Sizes[4] = { 0.64f, 0.288f, 0.32f, 0.0f };
f32 sefKinds3To4Lift[4] = { 0.002f, 0.0032f, 0.0005f, 0.0008f };

SefEmitterFunc sefSpawnHandlers[26] = {
    sefSpawnKinds0To2, sefSpawnKinds0To2, sefSpawnKinds0To2, sefSpawnKinds3To4, sefSpawnKinds3To4,
    sefSpawnKinds5To7, sefSpawnKinds5To7, sefSpawnKinds5To7,
    sefSpawnKinds8To10, sefSpawnKinds8To10, sefSpawnKinds8To10,
    sefSpawnKinds11To13, sefSpawnKinds11To13, sefSpawnKinds11To13,
    sefSpawnKinds14To16, sefSpawnKinds14To16, sefSpawnKinds14To16,
    sefSpawnKinds17To19, sefSpawnKinds17To19, sefSpawnKinds17To19,
    sefSpawnKind20, sefSpawnKind21,
    sefSpawnKinds22Up, sefSpawnKinds22Up, sefSpawnKinds22Up,
    0
};

SefEmitterFunc sefUpdateHandlers[26] = {
    sefUpdateBurstParticles, sefUpdateBurstParticles, sefUpdateBurstParticles,
    sefUpdateKinds3To4Particles, sefUpdateKinds3To4Particles,
    sefUpdateFadeOutParticles, sefUpdateFadeOutParticles, sefUpdateFadeOutParticles,
    sefUpdateKinds8To10Particles, sefUpdateKinds8To10Particles, sefUpdateKinds8To10Particles,
    sefUpdateKinds11To13Particles, sefUpdateKinds11To13Particles, sefUpdateKinds11To13Particles,
    sefUpdateKinds14To16Particles, sefUpdateKinds14To16Particles, sefUpdateKinds14To16Particles,
    sefUpdateKinds17To19Particles, sefUpdateKinds17To19Particles, sefUpdateKinds17To19Particles,
    sefUpdateKind20Particles, sefUpdateSpinParticles,
    sefUpdateKinds8To10Particles, sefUpdateKinds8To10Particles, sefUpdateKinds8To10Particles,
    0
};

/* Last look-at and position keys applied (SdvCameraKeyLog, 0x28 bytes). */
SdvCameraKeyLog sdvCameraKeyLog = { 0 };

/* The small-cell heap handed to smInitilizeSmall (0x20000 bytes), and the resource slot table behind it. */
unsigned char sefSmallHeap[0x20000] = { 0 };
ResourceSlot sefResourceTable[SEF_RESOURCE_SLOT_COUNT] = { 0 };
