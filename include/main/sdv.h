#ifndef INCLUDE_MAIN_SDV_H
#define INCLUDE_MAIN_SDV_H

#include "shared.h"

/* Functions and record parts of src/main/sdv.c that other files use. */

/* A key sequence: keys holds rows of stride halfwords, the first of each row being the frame
 * the row starts at (see the SDV_KEY_ values in sdv.c). A SefEffectRecord holds the sound, sound state, alter and wind
 * sequences, and the first part of its special. */
typedef struct SdvSequence {
    short *keys;            /* +0x00 */
    short elapsed;          /* +0x04 frames since the sequence started */
    signed char position;   /* +0x06 current row */
    signed char stride;     /* +0x07 halfwords per row */
} SdvSequence;

/* A special (X1's SdvSpecial, which also starts with its sequence): the sequence it runs, then its slot ids.
   sequence.position counts the rows reached; slotIds[i] (i < position) is the special record row i took, 0 once
   that record is freed or when no record was free. */
typedef struct SdvSpecial {
    SdvSequence sequence;     /* +0x00 */
    signed char slotIds[6];   /* +0x08 */
} SdvSpecial;

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

/* The seven words of an sef request (the block of a SefEffectRecord from its owner word on): sdvExecAlter copies
   them into the sef request buffer and hands them to sefLoadRequestBuffer. */
typedef struct SdvSefRequest {
    /* +0x00 the actor of the request (struct XenoActor); the one-member union (alias set 0)
       keeps sdvExecAlter's seven word copies
       in load/store order */
    union {
        struct XenoActor *actor;
    } target;
    int params[6];                      /* +0x04 */
} SdvSefRequest;

/* One 12-byte entry of an effect's image list. */
typedef struct SefImageSource {
    unsigned short unmodeled_00[2];
    unsigned short image;   /* +0x04 */
    unsigned short clut;    /* +0x06 */
    unsigned short unmodeled_08[2];
} SefImageSource;

/* One entry of sefEffectRecordList (sef.c, 64 entries of 0x50 bytes): a record or persistent resource slot saved by
 * sefPushEffect and sefCollectPersistentSlots, and respawned by sefPopEffect, which passes position, rotation, scale,
 * key and slotIndex to sefCreateEffectCf2 and writes owner, object, flags and gate back into the new record. For a
 * resource slot, object is its savedWord, gate its visible flag and cellSlot the low half of its reset flag. */
typedef struct SefSavedEffect {
    AlignedVector4 position;           /* +0x00 */
    Vector4 rotation;                  /* +0x10 */
    Vector4 scale;                     /* +0x20 */
    struct SefGroupObject *owner;      /* +0x30 */
    struct SefGroupObject *object;     /* +0x34 */
    int key;                           /* +0x38: the record's owner key */
    int flags;                         /* +0x3c */
    unsigned short gate;               /* +0x40 */
    unsigned short cellSlot;           /* +0x42: not read back */
    short slotIndex;                   /* +0x44: record index, or index in sefResourceTable for a slot */
    unsigned char unmodeled_46[0x50 - 0x46];
} SefSavedEffect;

void sdvInitAmbient(void);
void sdvSetAmbState2(int state, int effect_no);
void sdvExecAmbient(void);

void sdvInitAlters(void);
void sdvDestroyAlters(void);
void sdvDestroyOwnerAlters(int owner);
void sdvExecAlters(void);
void sdvDrawAlters(int layer);
void sdvDrawOwnerAlters(int owner);
void sdvScheduleAlter(SdvSequence *sequence, SdvSefRequest *sefRequest, int layer, int recordIndex);

void tiWindClearFirstSlot(void);
void tiWindResetSlots(void);
void tiWindReleaseSlots(int key, int subkey);
void tiWindUpdateKeyframes(SdvSequence *sequence, int key, unsigned short subkey);

int sdvSelectCamera(SdvCameraSchedule *schedule, int base, unsigned short *table);
int sdvScheduleCamera(SdvCameraSchedule *schedule, int frame);
void sdvExecEffectSeq(SdvSequence *sequence, int effectId);
void sdvExecShadowSeq(SdvSequence *sequence);

void sdvResetSpecialWork(void);
void sdvClearSpecialWork(void);
void sdvFreeSpecialSlots(SdvSpecial *special);
void sdvExecSpecial(SdvSpecial *special, int base);
void sdvDrawSpecial(SdvSpecial *special);

void sefResetResourceTable(void);
void sefResetAll(void);
void sefPacketFlush(void);
void sefCullResourceSlots(void);
void sefDrawResources(int slotIndex);
int sefGetActiveCount(void);
void sefReleaseByKey(int key);
void sefSetSlotFlags(int key, int flagBits);
void sefClearSlotFlags(int key, int flagBits);
void sefCollectPersistentSlots(SefSavedEffect *list, int count);

/* Cell records: a SefEffectRecord whose cellSlot is set is the head of a ResourceSlot of sefResourceTable (the
 * leading position, rotation, scale and tint blocks and the word at +0x40 are shared). */
struct ResourceSlot;
void sefReleaseCellSlot(struct ResourceSlot *slot);
void sefResetSlotRecords(struct ResourceSlot *slot, int suspend);
void sefRunEmitters(struct ResourceSlot *target);
void sefObjectClearFlags(struct ResourceSlot *slot, int mask);
struct ResourceSlot *sefCreateKindResourceSlot(int effectId, SefImageSource *sources, int kind, const Vector4 *position,
                                               Vector4 *rotation, Vector4 *scale, int slotIndex);

#endif /* INCLUDE_MAIN_SDV_H */
