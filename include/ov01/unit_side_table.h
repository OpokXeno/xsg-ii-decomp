#ifndef INCLUDE_OV01_UNIT_SIDE_TABLE_H
#define INCLUDE_OV01_UNIT_SIDE_TABLE_H

/* Functions of src/ov01/unit_side_table.c that other files call. */

struct BattleUnit;

/* Claims the first free unit slot and returns its unit object, or 0 when all nine slots are claimed. */
struct BattleUnit *unitSideClaim(void);
void monsSlotReleaseRecord(void);
void unitSideClearAll(void);
void unitSideRunDraw(void);
void unitSideRunUpdate(void);

#endif /* INCLUDE_OV01_UNIT_SIDE_TABLE_H */
