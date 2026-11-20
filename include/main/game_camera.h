#ifndef INCLUDE_MAIN_GAME_CAMERA_H
#define INCLUDE_MAIN_GAME_CAMERA_H

#include "main/game_data.h"
#include "main/xenoActor.h"
#include "main/ytachi_test.h"
#include "shared.h"

/* Types used by the map object code (game_camera.c, runtime_*); the actor itself is struct XenoActor of main/xenoActor.h. */

/* One 0x870-byte record of uwamonoScaleRecords: the scale of the map object record in the same slot
 * (UwamonoRecord.extent.x). Kept as its own label because the setCollisionScale natives of
 * runtime_uwamono_door.c address it with a base register of its own; folding it into uwamonoRecords
 * changes their register allocation. */
typedef struct UwamonoScaleRecord {
    float scale;                        /* +0x000 */
    unsigned char unmodeled_004[0x870 - 0x004];
} UwamonoScaleRecord;

extern UwamonoScaleRecord uwamonoScaleRecords[];

/* Bit of gameWork.camera.flags that cameraApplySlots sets when it starts a transition. */
#define CAMERA_FLAG_SLOTS_APPLIED 0x10

/* One 20-byte entry of cameraSlotTable: the signed key and sub-key cameraSlotFind matches. */
struct CameraKeyEntry {
    signed char key;                /* +0x00 */
    signed char sub;                /* +0x01 */
    unsigned char anchorReady;      /* +0x02 */
    unsigned char unmodeled_03;
    struct CameraSlot slot;         /* +0x04 */
};

/* Functions of src/main/game_camera.c that other files call. */

void cameraApplySlots(int slot_mask, int index_a, int index_b, int enable_flag);
void cameraInit(void);
void cameraApplyView(void);
void mapObjectResourcesLoad(void);
void GameCameraStateSave(void);
void GameCameraStateRestore(void);
void GameCameraReset(void);
void GameCameraChangeID(int mode, int arg1, int arg2);
void cameraSetIndex(int index);
struct XenoActor *cameraAnchorGet(Vector4 *out, int index, int subIndex);
struct XenoActor *cameraApplyAnchorOffset(Vector4 *position, int anchorIndex, int anchorSubIndex, Vector4 *offset);
void cameraApplyAllSlots(int unusedArg);
struct CameraKeyEntry *cameraSlotFind(int key, int subKey);
void GetCameraPos(Vector4 *position, Vector4 *offset, Vector4 *angles);
void cameraAnglesBetween(Vector4 *angles, const Vector4 *from, const Vector4 *to);
void cameraUpdateFrame(void);
void resPlayerSlotsLoadModels(int modelId0, int modelId1, int modelId2);
void uwamonoAttachPlayerParts(void);
void uwamonoChangePlayerModel(int request);
void uwamonoPlayerUpdate(void);
int uwamonoFindOverlapRecord(int owner, AlignedVector4 *point, float reachScale);
/* The record of uwamonoCurrentIndex (a UwamonoRecord), or 0 when there is none. */
void *uwamonoCurrentRecord(void);

#endif /* INCLUDE_MAIN_GAME_CAMERA_H */
