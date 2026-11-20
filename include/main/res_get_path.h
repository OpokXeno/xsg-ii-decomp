#ifndef INCLUDE_MAIN_RES_GET_PATH_H
#define INCLUDE_MAIN_RES_GET_PATH_H

/* Functions of src/main/res_get_path.c that other files call. */

/* A completion callback stored in CdSlot.tag (as a void * word): block index, buffer, size (xglCdSlotEvent
 * passes the state word it stored on event 1) and the CdSlot.value word. */
typedef void (*GameResCallback)(int index, void *buffer, unsigned int size, void *value);

/* One 0x14-byte entry of the block table xglCdSlotTable, indexed by slot. The table
 * describes memory blocks in address order; CD reads and other allocations (e.g. the pool
 * cfeNodePoolInit takes through GameResBufferAlloc) share it:
 *   buffer: block start; 0 ends the table (every search stops there).
 *   size:   block bytes (GameResBlockAssign splits, GameResBlockFree merges by adding sizes).
 *   owner:  0 when free; a resource id, or GAME_RES_OWNER plus bits 8-23 of the buffer
 *           address for a block allocated without an owner (GameResBlockAlloc with owner 0,
 *           named loads and anonymous allocations alike). Entry 0's owner is the first
 *           index the best-fit search GameResBlockFindBestFit starts at.
 *   tag:    completion callback of a read into the block, 0 when idle.
 *   value:  word handed back to that callback (copied from xglCdSlotValue). */
typedef struct CdSlot {
    void *buffer;
    unsigned int size;
    int owner;
    void *tag;
    void *value;
} CdSlot;

/* Entries of xglCdSlotTable; every search walks indexes 1 .. GAME_RES_BLOCK_COUNT - 1. */
#define GAME_RES_BLOCK_COUNT 0x80

extern CdSlot xglCdSlotTable[];

/* Starts the resource manager over the memory range [base, base + size): entry 0 of xglCdSlotTable records the
 * range, then the slot selection, the descriptor tables and the sound record table are reset. */
void GameResourceInit(void *base, unsigned int size);
void GameResourceReset(void);
void GameResourceWorkReload(void);
void *GameResBufferAlloc(int size);
/* Frees the block that starts at buffer / the block owned by owner. */
int GameResBlockFreeByBuffer(void *buffer);
int GameResBlockFreeByOwner(int owner);
/* The start of the largest free block, and its size in *size when that is not null. */
void *GameResourceGetFreeAddr(unsigned int *size);
/* The block of resource id: its start (0 when not loaded) and its size in *outSize when that is not null. */
void *GameResFindID(int resourceId, int *outSize);
int GameResLoadID(int id, void **buffer, unsigned int *size, GameResCallback callback);
int GameResLoadName(const char *name, void **data, unsigned int *size, GameResCallback callback);
/* Reserves the largest free run of blocks for at least bytes_wanted bytes and returns its start; the first call
 * records it as the arena (gameResArenaStart .. gameResArenaEnd), later ones as the work area. */
void *GameResourceWorkAlloc(unsigned int bytes_wanted);
/* Ends the arena (or work area) GameResourceWorkAlloc recorded and clears the recorded ranges. */
void GameResArenaRelease(void);

/* The jump point resJumpRecordSet stores: x, y, z and the radius within which resJumpPointsUpdate loads its CF file. */
typedef struct ResJumpPoint {
    float value[4];
} ResJumpPoint;

/* The CF file (script) state: resLoadCfFile reads the file of resource id, resApplyCfFile applies it,
 * resResetScriptState clears what it set up. */
void resLoadCfFile(int id);
void resApplyCfFile(int id);
void resResetScriptState(void);
void resSelectSlot(int search);
void resResetDescriptorTables(void);
void resJumpPointsUpdate(void);
/* Stores point as the jump point of record index (a new record at the end when index is negative) for CF file cfId. */
void resJumpRecordSet(int index, int cfId, ResJumpPoint *point);
void resGetEntryTables(unsigned int **first, unsigned int **second);
void cdSlotTestOnStart(void);

/* The player's resource slots. */
void resPlayerSlotsInit(void);
void resPlayerSlotsReset(void);
void resPlayerSlotUpdate(int slot);
int resPlayerSlotFindByOwner(int owner);
void resPlayerSlotLoadID(int slot, int value, void **out);

/* Sound state: the field/default restore pair, the footstep banks and the BGM name. */
void seStateClear(void);
void seStateRestoreDefault(void);
void seStateRestoreField(void);
void seFootBanksLoad(void);
int seFootBankFromKind(int kind, int code);
void seBgmNameSet(char *name);
/* Index into the BGM name list the CF script keeps; resetting the resource state clears it. */
extern unsigned char bgmNameListIndex;
void soundReset(void);
void soundRestart(void);

/* xglCdBuildFileName writes the group prefix and the name of file_id to buffer and returns the end of the written
 * name; xglCdFindFileName returns the name of file_id within its group (0 when no group holds it). */
char *xglCdBuildFileName(char *buffer, unsigned int file_id);
char *xglCdFindFileName(unsigned int file_id);

/* The sound records of resRecordTable: seVoiceRecordSet stores a voice with its volume and pan,
 * seVoiceRecordAddAt one that follows the emitter position (three floats) and seVoicePanVolumeFromPosition turns a
 * position into the packed (volume << 8) + pan the record update applies. */
void seVoiceRecordSet(unsigned int voice, unsigned int volume, unsigned int pan);
void seVoiceRecordAddAt(int voice, float *position);
unsigned int seVoicePanVolumeFromPosition(float *position);

#endif /* INCLUDE_MAIN_RES_GET_PATH_H */
