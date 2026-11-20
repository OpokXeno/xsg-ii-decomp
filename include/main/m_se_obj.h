#ifndef INCLUDE_MAIN_M_SE_OBJ_H
#define INCLUDE_MAIN_M_SE_OBJ_H

#include "shared.h"

struct XglCamera;

/* The camera snapshot MSeObjProc mode 0 takes before the object callbacks run. The effect files of ov01 read it
 * to project their points (MMathRotTransPersClip) and to face the camera (rotationMatrix). */
typedef struct MEfCameraParams {
    struct XglCamera *camera;       /* +0x0: xglStudioGetCamera2(0) */
    unsigned char unmodeled_04[0xC];
    Matrix4 viewMatrix;             /* +0x10: camera->volume[0].viewMatrix */
    Vector4 rotation;               /* +0x50: camera->travel.rotation */
    Matrix4 rotationMatrix;         /* +0x60: MMathRotateMatrixYXZ of rotation */
    unsigned char unmodeled_A0[0x20]; /* +0xA0: the .data object is 0xC0 bytes */
} MEfCameraParams;

extern MEfCameraParams MEfCamParams;

struct XenoActor;

/* The 0x40-byte request MSeFire copies into a new MSe effect object (the start of the object's block, which each
 * effect constructor reads before it overwrites the block). type picks the constructor in MSeFireCallbacks and
 * param goes into the object's +0x4. Each actor comes with the part it is attached at. Copied as doublewords. */
typedef struct MSeFireEntry {
    int type;                           /* +0x00 */
    struct XenoActor *owner;            /* +0x04 */
    int ownerPart;                      /* +0x08 */
    struct XenoActor *link;             /* +0x0C */
    int linkPart;                       /* +0x10 */
    unsigned char unmodeled_14[0x20 - 0x14];
    DoublewordAlignedVector4 offset;    /* +0x20 */
    struct XenoActor *target;           /* +0x30 */
    int targetPart;                     /* +0x34 */
    int param;                          /* +0x38: the effect record's id (sef_164 record +0x6C) */
    int unmodeled_3C;                   /* +0x3C */
} MSeFireEntry;

/* One MSe object: a 0x800-byte slot of the pool MSeObjInit clears (MSeObjAlloc sets flags to 1 and the
 * mapper and the four callbacks to 0). */
typedef struct MSeObj MSeObj;
typedef void (*MSeObjCallback)(MSeObj *object);

struct MSeObj {
    unsigned short flags;           /* +0x0: MSE_OBJ_BUSY, MSE_OBJ_FREE_REQUESTED (MSeObjRequestFree) */
    unsigned char unmodeled_02[2];
    unsigned int imageMapper;       /* +0x4: mapper forced before the mode 2 callback; 0 keeps the current one (MSeFire stores the entry's param here) */
    MSeObjCallback modeProc[4];     /* +0x8: the callback MSeObjProc runs for each mode */
    unsigned char unmodeled_18[8];
    MSeFireEntry entry;             /* +0x20: the entry MSeFire copied in */
    unsigned char unmodeled_60[0x800 - 0x60];
};

/* A callback MSeObjProc calls with no arguments after its mode 1 or mode 2 pass. */
typedef void (*MSeHook)(void);

/* Functions of src/main/m_se_obj.c that other files call. */

void MSeObjInit(void);
void MSeObjRequestFree(void *object);
void MSeObjShutdown(void);
/* Runs one pass over the busy objects of the pool (mode 0..3). */
void MSeObjProc(unsigned int mode);
/* Takes a free slot of the pool; 0 when none is left. */
MSeObj *MSeObjAlloc(void);
void MSeSetMode1Hook(MSeHook hook);
void MSeSetMode2Hook(MSeHook hook);
/* Builds in destination the rotation that turns the unit vector from onto the unit vector to; returns 0 for
 * opposite vectors (a half turn about y), otherwise 1. */
int MMathMatrixBetweenVectors(Matrix4 *destination, const Vector4 *from, const Vector4 *to);

#endif /* INCLUDE_MAIN_M_SE_OBJ_H */
