#ifndef INCLUDE_MAIN_XGL_MC_REQUEST_H
#define INCLUDE_MAIN_XGL_MC_REQUEST_H

#include "shared.h"
#include "main/game_data.h"
#include "main/xgl_cd.h"

/* Functions of src/main/xgl_mc_request.c that other files call. */

/* One request of the xglMc queue: the file number, its buffer and size, and
 * a callback the list and old-data scans call per entry (with the result,
 * and the entry bytes for the old-data check).  The callback is unprototyped
 * because the callers in yajima_test.c differ: DebugCallbackLog takes the result
 * only, McOldDataCheckCallback the result and the entry. */
typedef struct McRequestParam {
    int file;
    void *data;
    int size;
    int (*callback)();
} McRequestParam;

/* The save image SaveData (src/main/xgl_mc_request.c) is the memory card file contents: card file 0 holds its
 * 0x50-byte header, the other files the rest. */
#define SAVE_DATA_SIZE 0x5160

/* The 64-bit party values saveWriteImage copies; the pair of them in PartyProgress is the actor position
 * (as floats). Their loads stay after the int store before them, so each value is a union (its alias set
 * conflicts with the int store; copied as a Vector4 the loads move above it). */
typedef union PartyValue64 {
    long long value;
    int words[2];
    float coords[2];
} PartyValue64;

/* The 256-bit bitmap of PartyProgress (one bit per data box entry of category 3), copied whole into the save. */
typedef struct PartyEligibility {
    unsigned char bits[0x20];
} PartyEligibility;

/* One 0x2B-byte record of ptMemberTakenTable, copied whole into the save image and read through
 * PartyMember.buffer: the taken bits of one member (header, cells), the highest open group, the
 * taken count and the entries selectionFindIndex searches. ptReset clears all of it. */
typedef struct PartyTakenBits {
    unsigned char header[4];            /* +0x00 */
    unsigned char cells[4][8];          /* +0x04 */
    unsigned char openGroup;            /* +0x24 */
    unsigned char takenCount;           /* +0x25 */
    unsigned char entries[4];           /* +0x26 */
    unsigned char entry_count;          /* +0x2A */
} PartyTakenBits;

/* The party as the save image keeps it: first id bytes of every slot and
 * roster entry, the progress record's fields and the leader. */
typedef struct SaveParty {
    unsigned char slotIds[10];
    unsigned char rosterIds[10];
    unsigned short memberMask;
    unsigned char unmodeled_16[2];
    int mapId;
    unsigned char unmodeled_1C[4];
    PartyValue64 position[2];
    unsigned char savedSplitMode;
    unsigned char splitMode;
    unsigned char frontIndex;
    unsigned char backIndex;
    unsigned char counts[3];
    PartyTakenBits names[10];
    unsigned char unmodeled_1E5;
    unsigned char leaderId;
    unsigned char leaderValue;
    unsigned short slotValues[7];
    unsigned char mapFlag;
    PartyEligibility eligibility;
} SaveParty;

/* The blocks saveWriteImage copies whole into the save image. Their types set
 * the copy alignment the original shows: the int blocks and the short tables
 * take the runtime alignment test, the byte blocks unaligned moves. */
typedef struct SaveFlagBlock1080 {
    int words[0x420];
} SaveFlagBlock1080;

typedef struct SaveFlagBlock400 {
    int words[0x100];
} SaveFlagBlock400;

typedef struct SaveShortTable {
    short values[0xFF];
} SaveShortTable;

typedef struct SaveWords4 {
    int words[4];
} SaveWords4;

typedef struct QuestFlagBlock {
    unsigned char bytes[0x24];
} QuestFlagBlock;

typedef struct CitizenFlagBlock {
    unsigned char bytes[0x32];
} CitizenFlagBlock;

typedef struct SegFlagBlock {
    unsigned char bytes[0x12];
} SegFlagBlock;

/* The thumbnail block of a save image (0x1000 bytes at SaveData + 0x174). */
typedef struct SaveThumbnail {
    unsigned char pixels[0x1000];
} SaveThumbnail;

/* The save image SaveData (0x5160 bytes, the size saveChecksum sums). Its settings are read by the option
 * pages, the movie player and the display adjustment; the script flag window at +0x74 is packed by xgl_flags.c. */
struct SaveData {
    unsigned char unmodeled_00[8];
    long long checksum;
    int saveCount;
    unsigned char unmodeled_14[0x28 - 0x14];
    unsigned char soundMode;                /* +0x28: 0..2; the sound function soundStereoSet gets soundMode != 0 */
    unsigned char dolby_digital;            /* +0x29: Dolby Digital on; the movie render buffer gets the extra block */
    unsigned char unmodeled_2A[0x32 - 0x2A];
    short displayOffsetX;                   /* +0x32: the horizontal argument of tyaSetDisplayOffset */
    short displayOffsetY;                   /* +0x34: the vertical argument */
    unsigned char unmodeled_36[0x3A - 0x36];
    unsigned char caption_all;              /* +0x3A: nonzero shows every caption line; 2 is the save default */
    unsigned char unmodeled_3B[0x3E - 0x3B];
    unsigned char actuatorMode;             /* +0x3E: copied to both pads' actuatorMode */
    unsigned char unmodeled_3F[0x46 - 0x3F];
    unsigned char cursor_memory;            /* +0x46 */
    unsigned char cursor_memory_alt;        /* +0x47 */
    unsigned char unmodeled_48[0x50 - 0x48];
    long long version;
    unsigned char unmodeled_58[8];
    XglClock saveTime;
    XglClock playTime;
    unsigned char unmodeled_70[4];
    unsigned char scriptFlags[0x100];       /* +0x74: the packed script flags of xgl_flags.c */
    SaveThumbnail thumbnail;                /* +0x174 */
    SaveFlagBlock1080 flagBlock1080;
    unsigned char unmodeled_21F4[0x2274 - 0x21F4];
    SaveFlagBlock400 flagBlock400;
    SaveShortTable shortTables[4];
    int word2E6C;
    SaveWords4 words2E70;
    GameWork game;
    unsigned char unmodeled_49B0[0x4E80 - 0x49B0];
    QuestFlagBlock questFlags;
    CitizenFlagBlock citizenFlags;
    SegFlagBlock segFlags;
    unsigned char unmodeled_4EE8[8];
    SaveParty party;
    unsigned char unmodeled_5108[0x5150 - 0x5108];
    int balance;
    unsigned char unmodeled_5154[0x5160 - 0x5154];
};

extern struct SaveData SaveData;

void xglMcReset(void);
char * xglMcSetFullPath(int card, int selector);
int xglMcMain(void);
/* Writes the numbers of a save entry's slot and time (time->hour and time->minute) into its text. */
void xglMcSetEntryNumbers(unsigned char *object, int value, const XglClock *time);
int xglMcRequest(int port, int command, McRequestParam *param, int *result);

#endif /* INCLUDE_MAIN_XGL_MC_REQUEST_H */
