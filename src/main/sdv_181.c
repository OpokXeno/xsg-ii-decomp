#include "common.h"
#include "shared.h"
#include "main/sef.h"
#include "main/tu051.h"
#include "vu0_macro.h"
#include "main/TiWindBase.h"
#include "main/gifState.h"
#include "main/sdv_181.h"
#include "main/tu152.h"
#include "main/sdv_033.h"
#include "quadword.h"
#include "main/tu190.h"

/* Quad-corner and unit-square tables of the alter strips (read by sdvCreateAlter and sdvExecAlter). */
const float D_0068C530[4] = { 0.0f, 0.0f, -1.0f, 1.0f };
const float D_0068C540[8] = { 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f };
const float D_0068C560[8] = { -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };

/* A key sequence: keys holds rows of stride halfwords, the first of each row being the frame
 * the row starts at (see the SDV_KEY_ values below). */
typedef struct SdvSequence {
    short *keys;            /* +0x00 */
    short elapsed;          /* +0x04 frames since the sequence started */
    signed char position;   /* +0x06 current row */
    signed char stride;     /* +0x07 halfwords per row */
} SdvSequence;

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
#define SDV_KEY_OBJECT1 0x100       /* the request's second object (camera kinds only) */

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
    SefRequest *request = (SefRequest *)sefGetRequestBuffer();
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
    SefRequest *request = (SefRequest *)sefGetRequestBuffer();
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

/* The 0x40-byte request MSeFire (ov01) copies into a new MSe effect object; kind picks the
 * creator and effectId goes into the object's +0x4. Each actor comes with the part it is attached at. */
typedef struct MSeRequest {
    int kind;                   /* +0x00 */
    struct XenoActor *owner;    /* +0x04 */
    int ownerPart;              /* +0x08 */
    struct XenoActor *link;     /* +0x0C */
    int linkPart;               /* +0x10 */
    unsigned char unmodeled_14[0x20 - 0x14];
    Vector4 offset;             /* +0x20 */
    struct XenoActor *target;   /* +0x30 */
    int targetPart;             /* +0x34 */
    int effectId;               /* +0x38 the effect record's id (sef_164 record +0x6C) */
    unsigned char unmodeled_3c[0x40 - 0x3C];
} MSeRequest;

/* Clears a 0x40-byte request block: memset(buffer, 0, 0x40) (ov01 tu075.c). Every MSeRequest
 * field the select path below leaves unassigned is therefore 0. */
void MSeFireEntryClear(void *buffer);
void MSeFire(MSeRequest *request);

/* Steps an effect key sequence and fires the MSe effect of the row it reaches, attached to
 * the actors of the current effect request. */
void sdvExecEffectSeq(SdvSequence *sequence, int effectId)
{
    MSeRequest fire;
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
    request = (SefRequest *)sefGetRequestBuffer();
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
    fire.kind = kind;
    fire.effectId = effectId;
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
    request = (SefRequest *)sefGetRequestBuffer();
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
        actor->moveAngle = angle;
        actor->shadowValue = shadowValue;
        actor->moveVelocity = velocity;
        actor->shadowCount = shadowCount;
        actor->shadowValue3 = shadowValue3;
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

void *memset(void *s, int c, unsigned int n);

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

void sefGetPosition(Vector4 *destination, Vector4 *offset, int kind);
void *MMathNormalizeVector2(void *destination, const Vector4 *source);

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

void sdvClearSpecialWork(void);
static short _sdvSpecialBuf[8];
void GameDefocusSet(int index, int type, const int *settings);

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

/* One key track of a camera schedule; sdvProgressPrm reads its key table at +0x04. */
typedef struct SdvScheduleChannel {
    short keyIndex;           /* +0x00: the key sdvProgressKey is at; sdvProgressPrm steps it onto the end marker */
    unsigned char unmodeled_02[2];
    short *keys;              /* +0x04 */
} SdvScheduleChannel;

/* A camera schedule (sefEffectRecords + 0xA1C): the selected camera and four key tracks. */
typedef struct SdvCameraSchedule {
    int camera;                     /* +0x00 the selected camera's script address, 0 when none */
    unsigned char unmodeled_04[3];
    unsigned char animation;        /* +0x07 cleared on selection (X1's SdvCameraChoice.animation); no reader yet */
    SdvScheduleChannel channels[4]; /* +0x08 */
} SdvCameraSchedule;

int rand(void);
int scGetImmAdrImmIdx2(int base, unsigned short *table, int index);
short scGetImmNumIdx(short *table, int index);

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
typedef struct SefPartTransform SefPartTransform;
int sefInitPartTransform(SefPartTransform *part, int kind, int scaleKey40, int scaleKey80, int scaleKey100);
void vu0RotateCurrentX(float value);
int sefGetKindVector(Vector4 *destination, int object, int count, int kind);

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
    request = sefGetRequestBuffer();
    scale = 1.0f;
    memset(camera, 0, sizeof(*camera));
    if (scaled != 0) {
        sefInitPartTransform((SefPartTransform *)&sefGlobalTransform, kind, request->kind1Key, request->kind0Key,
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

XglCamera *xglStudioGetActiveCamera(void);
void cameraInit(void);
/* Distance between two points (tu190.c). */
void cameraAnglesBetween(Vector4 *angles, const Vector4 *from, const Vector4 *to);

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
                camera->range.lockPosition.x = param.point.x;
                camera->range.lockPosition.y = param.point.y;
                camera->range.lockPosition.z = param.point.z;
                camera->slot[1].fraction = 0.0f;
                sdvCameraDistance = -MMathCalcDist(&camera->range.lockPosition, &camera->range.lookAt);
                if (blend != 0) {
                    cameraAnglesBetween(&angles, &camera->range.lookAt, &camera->range.lockPosition);
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
                camera->range.lockPosition.w = (float)lensKey->perspective * 0.017453292f;
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

extern s16 sefClipTestDisabled;

/* Advances the four tracks of a schedule to frame; returns how many of the look-at and position tracks applied
   their first key (key index 0) on this frame. */
int sdvScheduleCamera(SdvCameraSchedule *schedule, int frame)
{
    int started = 0;

    if (sefClipTestDisabled == 0) {
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

int sdvTransOffset(int *screen, int *offset, int part, int depth, float *scale, int argument, Matrix4 *matrix);

INCLUDE_ASM("asm/main/nonmatchings/sdv_181", sdvTransOffset);

/* Reset: zeroes the slot flags and every special record, then gifStateClearModeValue. */
void sdvResetSpecialWork(void)
{
    memset(_sdvSpecialBuf, 0, sizeof(_sdvSpecialBuf));
    memset(sdvSpecialTable, 0, sizeof(sdvSpecialTable));
    gifStateClearModeValue();
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

/* A special (X1's SdvSpecial, which also starts with its sequence): the sequence it runs, then its slot ids.
   sequence.position counts the rows reached; slotIds[i] (i < position) is the special record row i took, 0 once
   that record is freed or when no record was free. */
typedef struct SdvSpecial {
    SdvSequence sequence;     /* +0x00 */
    signed char slotIds[6];   /* +0x08 */
} SdvSpecial;

/* Frees the special slots an owner lists; a screen-effect record (kinds 10-15) with no key decrease, or of
   kind 13, also clears the gifState mode value. */
INCLUDE_ASM("asm/main/nonmatchings/sdv_181", sdvFreeSpecialSlots);

/* The number of slots a special lists. */
#define SDV_SPECIAL_SLOT_COUNT 6
/* Kinds from this one on are screen effects: they interpolate all four size halfwords of a key. */
#define SDV_SPECIAL_KIND_SCREEN 10

/* (0, 0, 0, 1): the offset sdvExecSpecial asks sefGetPoint for. */
const AlignedVector4 sdvOriginPoint = { 0.0f, 0.0f, 0.0f, 1.0f };
extern const Matrix4 MMathUnitMatrix;
Matrix4 *sefGetPoint(Vector4 *destination, const Vector4 *offset, int kind);

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
                    gifStateClearModeValue();
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

void sefApplyPartTransform(float *destination, float *source, int part);

#define SDV_SET_COLOR(rgba, source) \
    ((rgba).channel.red = (source)->color[0], \
     (rgba).channel.green = (source)->color[1], \
     (rgba).channel.blue = (source)->color[2], \
     (rgba).channel.alpha = (source)->color[3], (rgba).word)

/* A float vector that the VU0 code also stores as a whole quadword. sdvDrawSpecial indexes it as f[] and passes
   vector to the helpers; as a plain Vector4 gcc schedules the scale stores after the part loads before the two
   sdvTransOffset calls (fndiff 674/684 in place). */
typedef union SdvScale {
    float f[4];
    Vector4 vector;
} SdvScale;

/* Normalizes a vector in place. */
static inline void *sdvNormalize(Vector4 *vector)
{
    return MMathNormalizeVector2(vector, vector);
}

/* Draws the slots of one special: sdvTransOffset per slot, GameDefocusSet when allocated. */
void sdvDrawSpecial(SdvSpecial *special)
{
    int defocusParams[16];
    SdvScale scale;
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
        SdvScale *sizeVector;
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
            scale.f[0] = key->width * 0.01;
            scale.f[1] = key->height * 0.01;
            if (sdvTransOffset(screen, offset, table->part, key->depth, sizeVector->f, argument, matrix) == 0) {
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
                defocusParams[4] = scale.f[0];
                defocusParams[5] = scale.f[1];
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
            float *vector = scale.f;

            offset[0] = key->x;
            offset[1] = key->y;
            offset[2] = key->z;
            Vu0SetOrigin(vector);
            scale.f[0] = key->height * 0.005f;
            scale.f[1] = key->subWindow2 * 0.005f;
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
            func_00246DD8(screen[2]);
            nmlModelUseSubWindow(0, key->width);
            if (kind == 11 && record->table->part != 8) {
                nmlModelUseSubWindow(2, scale.f[0]);
                nmlModelUseSubWindow(3, scale.f[1]);
                scale.f[0] = key->subWindow3 * 0.005f;
                sdvTransOffset(screen, offset, record->table->part, key->depth, vector, argument, 0);
                nmlModelUseSubWindow(1, scale.f[0]);
            } else {
                nmlModelUseSubWindow(1, key->height);
                nmlModelUseSubWindow(2, key->subWindow2);
                nmlModelUseSubWindow(3, key->subWindow3);
            }
            if (kind == 12) {
                signed char *direction = (signed char *)key->color;

                scale.f[0] = direction[0];
                scale.f[1] = direction[1];
                scale.f[2] = direction[2];
                scale.f[3] = 1.0f;
                sdvNormalize(&scale.vector);
                sefApplyPartTransform(vector, vector, record->table->part);
                nmlModelSetMulColor(&scale.vector);
            }
            break;
        }
        }
    }
}

/* The seven words of an sef request: sdvExecAlter copies them into the sef request buffer and hands them to
   sefLoadRequestBuffer. */
typedef struct SdvSefRequest {
    struct SefActor *actor;             /* +0x00 the SefActor of sef_164.c */
    int params[6];                      /* +0x04 */
} SdvSefRequest;

/*
 * The 0x58-byte request an alter is created from: sdvScheduleAlter fills it from an event, sdvCreateAlter
 * copies it into the record at +0xAD0 and sdvExecAlter/sdvDrawAlter read it there.
 */
typedef struct SdvAlterRequest {
    unsigned long long tex0[2];   /* +0x00 GS TEX0 of the two images */
    SdvSefRequest *sefRequest;    /* +0x10 the seven words sdvExecAlter copies into the sef request buffer */
    unsigned char rgba[4];        /* +0x14 */
    unsigned char unmodeled_18[4];
    float length;                 /* +0x1C progress added over maxSamples points (each frame length / maxSamples) */
    unsigned char unmodeled_20[4];
    float imageFrames;            /* +0x24 */
    float life;                   /* +0x28 frames left; 2048 never runs out */
    float width;                  /* +0x2C */
    float height;                 /* +0x30 */
    float minDelta;               /* +0x34 a shorter move keeps the previous delta */
    short parts[2];               /* +0x38 sefGetPosition kinds; with follow set, parts[1] is the heading turn
                                     rate in 1/100 */
    short maxSamples;             /* +0x3C points the record grows to: at most 64, 32 with follow set */
    unsigned short shrink;        /* +0x3E frames taken per frame once the life is spent */
    unsigned short blend;         /* +0x40 */
    short imageCount;             /* +0x42 */
    unsigned short mode;          /* +0x44 */
    unsigned char unmodeled_46[2];
    short layer;                  /* +0x48 sdvDrawAlters draws the records of one layer */
    short owner;                  /* +0x4A index of the owning effect record; negative for none */
    short follow;                 /* +0x4C nonzero: the record trails the part of parts[0] */
    short spread;                 /* +0x4E */
    short spin;                   /* +0x50 degrees the point angles turn per frame */
    short timeBase;               /* +0x52 */
    short timeStep;               /* +0x54 */
    unsigned char unmodeled_56[2];
} SdvAlterRequest;

/* Builds an alter request from each event the sequence steps onto, then creates it through sdvCreateAlter. */
INCLUDE_ASM("asm/main/nonmatchings/sdv_181", sdvScheduleAlter);


/*
 * One alter record: 0xD40 bytes. The quadword members make the record 16-byte aligned, which is why
 * gcc addresses +0xD38 as +0xD30 + 8 and +0xB18 as +0xB10 + 8.
 */
typedef struct SdvAlter {
    Matrix4 worldMatrix;                /* +0x000 */
    Matrix4 matrix;                     /* +0x040 */
    /* +0x080 two 64-point rings, one per sef part; when the record trails a part (follow), [0] holds the
       sampled anchors and [1] the ring swung around them */
    AlignedVector4 points[2][64];
    AlignedVector4 quats[33];           /* +0x880 heading quaternions, newest first */
    AlignedVector4 heading;             /* +0xA90 */
    AlignedVector4 lastDelta;           /* +0xAA0 */
    AlignedVector4 position;            /* +0xAB0 sdvCreateAlter stores the sefGetPosition result here (lq/sq) */
    AlignedVector4 previousPosition;    /* +0xAC0 */
    SdvAlterRequest request;            /* +0xAD0 */
    float widths[64];                   /* +0xB28 */
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

/* Takes the first free alter record, stamps a serial and copies the request in. */
INCLUDE_ASM("asm/main/nonmatchings/sdv_181", sdvCreateAlter);

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

INCLUDE_ASM("asm/main/nonmatchings/sdv_181", func_0026A7A0);

/*
 * Advances one alter record by a frame: the lifetime and frame counters, the sef request, then either the
 * trailing ring (follow) or the next point of each part; a spent record is destroyed.
 */
INCLUDE_ASM("asm/main/nonmatchings/sdv_181", sdvExecAlter);

extern void sceVif1PkAddCode(XglPacket *packet, unsigned int tag);
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern XglPacket *xglPacketGetCurrent(void);

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

extern void sceVif1PkRef(XglPacket *packet, const void *environment,
                         int count, int mode, int offset, int flags);

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

/* Draws one alter's key frames into the packet from xglPacketGetCurrent. */
void sdvDrawAlter(SdvAlter *alter);

INCLUDE_ASM("asm/main/nonmatchings/sdv_181", sdvDrawAlter);

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

void svLoadDrawViewMatrix(void);
void svBuildDrawStatePacket(void);

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
unsigned short D_005E6D78[32] = {
    68, 69, 70, 71, 72, 73, 74, 75,
    76, 77, 78, 79, 80, 81, 82, 83,
    84, 85, 86, 87, 88, 89, 90, 91,
    92, 93, 94, 95, 0, 0, 0, 0
};
unsigned short D_005E6DB8[20] = {
    70, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86,
    0, 0, 0, 0
};
float D_005E6DE0[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
float D_005E6DF0[4] = { -1.0f, 0.0f, 0.0f, 1.0f };
u64 D_005E6E00[14] = {
    0x1000000000008006UL, 14, 0, 6, 0x5000DUL, 71, 96, 20,
    5, 8, 0x131000000UL, 78, 72, 66
};

float sdvCameraDistance = 0.0f;
int sdvAlterSerial = 0;
int D_0069DC44 = 0;

