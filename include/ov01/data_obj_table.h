#ifndef INCLUDE_OV01_DATA_OBJ_TABLE_H
#define INCLUDE_OV01_DATA_OBJ_TABLE_H

#include "ov01/obj_slot.h"
#include "ov01/obj.h"
#include "ov01/gr_matrix.h"

/* Functions of src/ov01/data_obj_table.c that other files call, and the data object record they share with src/ov01/battle_init.c. */

/* The pose block of a data object: a slide block for the movers (objStdMove, objHatoVec; its goal at +0x60 is the cursor
 * anchor curMove2Pos writes) and the TransformState grBBCalc projects (offset.y at +0x14 is cleared with the anchor). */
typedef union DataObjectPose {
    SlideBlock block;
    TransformState transform;
} DataObjectPose;

/* One of the two 0x1E0-byte data object slots (dataObjTable) and the object made in it by objEnCurCreate (src/ov01/battle_init.c, global
 * dataObject), also the target block of the key and cursor code. 0x00 slot flags (OV01_SLOT_* of ov01_slot.h: claim and
 * the update and draw skip bits of the sweeps in data_obj_table.c), 0x04 update callback (objCur), 0x08 draw callback
 * (objCurDraw), 0x0C object flags (0x2 hidden, 0x1 set by curCmdSet), 0x10 selected index (the entry
 * curSel compares), 0x14 lookup block, 0x150 pose, 0x1D0 id, 0x1D4 kind. */
struct DataObject {
    /* 0x00 */ int slotFlags;
    /* 0x04 */ void (*update)(struct DataObject *obj);
    /* 0x08 */ void (*draw)(struct DataObject *obj);
    /* 0x0C */ int flags;
    /* 0x10 */ int selectedIndex;
    /* 0x14 */ struct DataLookup lookup; /* the lookup block (obj_slot.h); the pad below keeps the pose at 0x150 */
    /* 0x140 */ unsigned char unmodeled_140[0x150 - 0x140];
    /* 0x150 */ DataObjectPose pose;
    /* 0x1D0 */ void *id;
    /* 0x1D4 */ int kind;
    /* 0x1D8 */ unsigned char unmodeled_1D8[0x1E0 - 0x1D8];
};

struct BattleUnit;

/* Releases the slot of a unit (unitRemove calls it). */
void unitSlotRelease(struct BattleUnit *unit);
void dataObjResetAll(void);
struct DataObject *dataObjAlloc(void);
void dataObjDrawAll(void);
void dataObjUpdateAll(void);

#endif /* INCLUDE_OV01_DATA_OBJ_TABLE_H */
