#ifndef INCLUDE_OV01_TGT_SELECT_H
#define INCLUDE_OV01_TGT_SELECT_H

/* Functions of src/ov01/tgt_select.c that other files call. */

/* One 0x14-byte record of ov01PartyTable (one per party slot; tgtSelectInit sets up three). The record doubles as the
 * party spawn record: scene_load spawns the unit charaId at position and turns it to direction, and the debug config and
 * the plPosInit/plDirInit natives edit the same words. */
typedef struct TgtSlot TgtSlot;

struct TgtSlot {
    int charaId;                            /* +0x00: party member id of the slot (the PLAYERn row of the debug config) */
    int position;                           /* +0x04: spawn position index; tgtSelectInit numbers the slots 0 to 2 */
    int direction;                          /* +0x08: facing in degrees 0..360; starts at 180 */
    int spawnArgC;                          /* +0x0C */
    int spawnArgD;                          /* +0x10 */
};

extern TgtSlot ov01PartyTable[];

int battleAfter(void);
int tgtSelectStep(void);
void tgtSelectModeEnd(void);
void tgtSelectInit(void);
/* The status word of the battle state (codes 1 to 5), which battleAfter switches on. */
int battleRetCodeGet(void);

#endif /* INCLUDE_OV01_TGT_SELECT_H */
