#ifndef INCLUDE_MAIN_SRS_H
#define INCLUDE_MAIN_SRS_H

/* Functions of src/main/srs.c that other files call. */

void srsChangeSeparator(char *path);
void srsInitCdRead(void);
int srsLeaveCdRead(void);
void sresInitMemoryRes(void);
void sresFreeReloaderMemory(void);
int srsIsEffectImageKey(int queryKey);
int fileLoad(void *buffer, const char *name, int mode);

/* Load modes of srsSetLoadMode and srsLoadMode: CD, host0:, usb0: and the xglDeb file access. */
#define SRS_LOAD_MODE_CD 0
#define SRS_LOAD_MODE_HOST 1
#define SRS_LOAD_MODE_USB 2
#define SRS_LOAD_MODE_DEB 3          /* xglDeb file access, relative to srsViewPath */
extern int srsLoadMode;

/* Battle slots of the resource state. */
#define SRS_CHARACTER_SLOTS 3        /* party members that own weapon data */
#define SRS_WEAPONS_PER_CHARACTER 3  /* weapon effects loaded per party member */
#define SRS_COMBO_SLOTS 3            /* enemy combo data slots */
#define SRS_EFFECT_SLOTS 24          /* loaded cf effect slots */

/* Image-mapper resource types: every slot of the resource state is indexed by
 * the type it registers with svAddImageMapper. srsAnalyzeEftNo reports the
 * same numbers as the effect category. */
typedef enum {
    SRS_RES_IMAGE = 0,
    SRS_RES_EFFECT_IMAGE = 1,
    SRS_RES_WEAPON = 2,         /* 3 characters x 3 weapon effects */
    SRS_RES_COMBO = 11,         /* 3 enemy combo slots */
    SRS_RES_BATTLE_IMAGE = 14,
    SRS_RES_CF_IMAGE = 15,
    SRS_RES_EFFECT = 16,        /* 24 loaded effect slots */
    SRS_RES_COUNT = 40
} SrsResourceType;

/* The complete 0x1A0-byte resource state shared with sef.c. The pending
 * mapper arguments and resource numbers are tables indexed by the resource
 * type each data pointer registers with svAddImageMapper. */
typedef struct SrsMemRes {
    void *image;
    void *effectImage;
    void *weaponData[SRS_CHARACTER_SLOTS][SRS_WEAPONS_PER_CHARACTER];
    void *comboData[SRS_COMBO_SLOTS];
    void *battleImage;
    void *cfImage;
    void *effectData[SRS_EFFECT_SLOTS];
    short pending[SRS_RES_COUNT];
    short no[SRS_RES_COUNT];
    void *effectExtra[SRS_EFFECT_SLOTS];
} SrsMemRes;

extern SrsMemRes _srsMemRes;

/* One 28-byte record of srsRecordTable. Effect slots are records 9..56. */
typedef struct SrsRecord {
    void *handle;           /* 0x00: buffer owned by the record, 0 when free */
    int key;                /* 0x04: canonical effect id */
    int size;               /* 0x08 */
    int owner;              /* 0x0C */
    int arg;                /* 0x10 */
    unsigned int stamp;     /* 0x14: last-use stamp, the eviction order */
    short effectNo;         /* 0x18: effect number of a wait-slot record */
    signed char kind;       /* 0x1A */
    signed char busy;       /* 0x1B */
} SrsRecord;


#define SRS_SLOT_COUNT 48
#define SRS_WAIT_SLOT_COUNT 8
/* Records before the effect slots (the package and the wait slots): effect slot i is image mapper i + 9. */
#define SRS_KEY_RECORD_COUNT (1 + SRS_WAIT_SLOT_COUNT)

/* SrsRecord.kind: how the record's buffer is freed (smFree, GameResBlockFreeByBuffer, or not at all). */
enum SrsRecordKind {
    SRS_KIND_SM_BLOCK = 0,
    SRS_KIND_GAME_RES_BLOCK = 1,
    SRS_KIND_CALLER_BUFFER = 2
};

/* srsRecordTable (0x5E66D0..0x5E6D28): 58 records (sresInitMemoryRes clears 0x658 bytes), each
 * registered as the image mapper of its record number. Record 0 holds the effect package (default.esp
 * or cfdef.esp), records 1..8 the .esp packages of the eight wait slots of sefSlotTable, records 9..56
 * the effect slots; record 57 is a sentinel slot whose stamp (+0x650) srsEvictEffectSlot sets to the
 * maximum. */
/* The package and the wait slots are their own members: as a nine-record array indexed [i + 1], the
 * wait-slot loops of srs.c compile to other address arithmetic (an offset of 28 from the table instead
 * of 16 from the table + 12). */
typedef struct SrsTable {
    SrsRecord package;                         /* +0x000: record 0 */
    SrsRecord waitSlots[SRS_WAIT_SLOT_COUNT];  /* +0x01C: records 1..8 */
    SrsRecord slots[SRS_SLOT_COUNT + 1];       /* +0x0FC: records 9..56 and the sentinel */
} SrsTable;

extern SrsTable srsRecordTable;

/* Resource loader functions of srs.c, read by sef.c and the sv file. */
char *srsGetEffectName(int effectNo);
int srsGetFileLen(const char *fileName);
int srsLoadEffectData(void *buffer, int effectNo);
char *srsAnalyzeEftNo(int effectNo, int *charNo, int *effectType);
void srsFormatEffectStem(int effectNo, char *buffer, int withSubNo);
extern int srsEffectStamp;

struct SefSlotTable;
void srsLoadPendingEffects(struct SefSlotTable *slotTable);
int srsLoadEffectBlock(int effectKey);
void srsReleaseSlotsByIndex(int index);
void sresDataMapping(void);
void srsCdReadCallback(int readStatus, int value, int tag);
int srsGetEffectSlot(int effectKey);
void srsLoadEspFile(int reload, int cfdef);
int srsMakeFileName(char *buffer, const char *fileName);

#endif /* INCLUDE_MAIN_SRS_H */
