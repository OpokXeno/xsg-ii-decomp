#ifndef INCLUDE_OV01_BATTLE_INIT_H
#define INCLUDE_OV01_BATTLE_INIT_H

/* Functions of src/ov01/battle_init.c that other files call. */

struct DataObject;
struct BattleUnit;

/*
 * Sound ids played by the slot and cursor code and the battle menu (SE_CURSOR_MOVE is the same id as
 * cursor_buffer.c).
 */
#define SE_CURSOR_MOVE 0x01000001
#define SE_CONFIRM 0x01000002
#define SE_REFUSE 0x01000003
#define SE_MENU_OPEN 0x01000004                 /* manInput: the command panel opens */
#define SE_MENU_CLOSE 0x01000005                /* manInput: the command panel is closed with PAD_CROSS */
#define SE_CANCEL 0x01000008

/* Key codes of the key functions: DATA_KEY_CANCEL is the cancel press (PAD_CROSS), DATA_KEY_NONE a key that
 * is not registered, not found or refused. */
enum DataKeyCode {
    DATA_KEY_CANCEL = 20,
    DATA_KEY_NONE = 21
};

/*
 * Data object and state reset: objCurRemove releases the data object, dataStateReset is the state step of the
 * reset chain (scene.c).
 */
void objCurDraw(struct DataObject *obj);
void objCur(struct DataObject *obj);
void objCurRemove(void);
void dataStateReset(void);

/* Counters and virtual pad. */
void keyBuffClear(void);
void dataVPadModeSet(int mode);
void dataVPadReset(void);
void dataVPadSet(int virtualPad);
void objEnCurCreateAtOrigin(void);

/* State of a command key for the unit (0 usable, 1 shown, 2 hidden; negative skips the icons). */
int keyCmdChk(struct BattleUnit *unit, int key, void *tgt);

/* Pad, key input and the battle menu panels (manInput). */
unsigned short dataPadRead(int port);
int keyCmdGet(struct BattleUnit *unit);
void keyBuffClearUnit(struct BattleUnit *unit);
void menuWepSpPanelReset(void);
int menuWepSpPanelUpdate(struct BattleUnit *unit);
void menuTargetListCursorReset(void);
int menuWepSpPanelCursorGet(void);
int menuWepSpPanelRowCountGet(void);
int menuTargetListCursorGet(void);

/* One 24-byte row of a slot table (the table of a unit whose key is 1 to 10 or of the others, dataSlotRowGet):
 * word 0 is a count, words 1 to 5 are slots. */
struct DataSlotRow {
    int count;
    int slot[5];
};

struct DataSlotRow *dataSlotRowGet(struct BattleUnit *unit, int index);
int dataSlotKeyTest(struct BattleUnit *unit, int key);
int menuTargetListUpdate(struct BattleUnit *unit);

/* Target and entry cursors of the battle menu. */
void dataTargetShow(struct BattleUnit *unit, unsigned char *targetFlags);
void dataTargetHide(void);
void dataObjShow(void);
void dataObjHide(void);
void curSelGrpSet(unsigned int value, int flags);
int curPosProcInit(struct BattleUnit *obj);
int curEnProcInit(struct BattleUnit **selected, int side);
int curEnProc(struct BattleUnit **out);
int curPosProc(struct BattleUnit *unit, int *out);
int curTgtProcInit(struct BattleUnit **selected, struct BattleUnit *target, int flags);
int curTgtProc(struct BattleUnit **out, int mode);

#endif /* INCLUDE_OV01_BATTLE_INIT_H */
