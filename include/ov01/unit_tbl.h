#ifndef INCLUDE_OV01_UNIT_TBL_H
#define INCLUDE_OV01_UNIT_TBL_H

#include "ov01/unit_cmd.h"

/* Functions of src/ov01/unit_tbl.c that other files call: the table of the unit objects (players, then enemies). */

/* Side argument of unitTblGet and the count helpers: players, enemies, or anything else for both tables. */
enum {
    UNIT_SIDE_PLAYER = 0,
    UNIT_SIDE_ENEMY = 1,
    UNIT_SIDE_ALL = 2
};

int unitTblGet(int side, BattleUnit ***table);
int unitTblSet(int side, BattleUnit *object);
int unitTblRemove(BattleUnit *object);
int unitNumGet(int side);
int unitLiveNumGet(int side);
int unitLiveNumGet2(int side);
int unitDownNumGet(int side);
int busyUnitChk(int side);
int unitNumBusyGet(int side);
int unitNoGet(BattleUnit *target);
BattleUnit *unitPtrGet(int index);

#endif /* INCLUDE_OV01_UNIT_TBL_H */
