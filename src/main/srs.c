#include "common.h"
#include "shared.h"
#include "main/srs.h"
#include "main/sv_get_image_item_159.h"
#include "main/xgl_cd.h"

extern char *strcat(char *destination, const char *source);
extern int sprintf(char *destination, const char *format, ...);
extern u8 *buffer;
extern char *strcpy(char *destination, const char *source);
int srsLoadEffectData(void *buffer, int effectNo);

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
extern int srsGetEffect2Idx(int effectNo);
void srsSetLoadMode(int load_mode);
char *srsGetEsdData(int index);
char *srsGetEffectName(int effectNo);
int fileLoad(void *buffer, const char *name, int mode);
extern int srsLoadMode;
extern int _nRead;

#define SRS_EFFECT2_INDEX_ROWS 117   /* _srsEffect2IndexTbl */
#define SRS_EFFECT2_INDEX_FIRST 601  /* effect number of its first row */
#define SRS_PATH_LENGTH 256          /* resolved file path and view path */
#define SRS_NAME_LENGTH 128          /* "esd/NAME.esd" style file name buffers */
/* The effect-name (.esd) table: one 16-byte name per effect number. */
#define SRS_EFFECT_NO_COUNT 0xC00    /* effect numbers are below this */
#define SRS_ESD_ENTRY_SIZE 0x10
#define SRS_ESD_TABLE_SIZE 0xC800
/* srsLoadMode values. */
#define SRS_LOAD_MODE_CD 0
#define SRS_LOAD_MODE_HOST 1
#define SRS_LOAD_MODE_USB 2
#define SRS_USB_PREFIX_LENGTH 5      /* "usb0:" */
/* sceOpen flags and sceLseek origins used by the host loader. */
#define SRS_OPEN_RDONLY 1
#define SRS_SEEK_SET 0
#define SRS_SEEK_END 2
/* xglCdReadFile completion callback: the read finished. */
#define SRS_CD_READ_COMPLETE 4
/* svAddImageMapper "height" (the fourth argument) registered for each resource. */
#define SRS_MAPPER_HEIGHT_IMAGE 3000
#define SRS_MAPPER_HEIGHT_EFFECT_IMAGE 3100
#define SRS_MAPPER_HEIGHT_WEAPON 2000
#define SRS_MAPPER_HEIGHT_COMBO 2100
#define SRS_MAPPER_HEIGHT_BATTLE_IMAGE 3400
#define SRS_MAPPER_HEIGHT_CF_IMAGE 3500
#define SRS_MAPPER_HEIGHT_EFFECT 3600
/*
 * Effect number ranges (first number and count). Each range resolves to one
 * of the SrsResourceType categories in srsAnalyzeEftNo/srsGetEffectType.
 */
#define SRS_EFFECT_COMMON_FIRST 2600       /* default.esp */
/* Weapon effects 2900..2998 share the data of effects 100 lower. */
#define SRS_EFFECT_WEAPON_ALT_FIRST 2900
#define SRS_EFFECT_WEAPON_ALT_COUNT 99
#define SRS_EFFECT_WEAPON_ALT_OFFSET 100

extern char _loadEsdData[SRS_ESD_TABLE_SIZE];
extern void sresDataMapping(void);
extern int sceOpen(const char *path, int flags, ...);
extern int sceLseek(int descriptor, int offset, int whence);
extern int sceRead(int descriptor, void *buffer, int bytes);
extern int sceClose(int descriptor);
extern int func_00A103C8(const char *path, void *buffer);
extern void srsCdReadCallback(int code);
extern char srsViewPath[SRS_PATH_LENGTH];
extern signed char _srsEffect2IndexTbl[SRS_EFFECT2_INDEX_ROWS];
int srsGetEffect2Idx(int effectNo);
int srsMakeFileName(char *buffer, const char *fileName);
char *srsGetEffectData(int index);
void srsCdReadCallback(int code);
void sresDataMapping(void);

/* Classifies an effect key from its index (bits 12-21) and kind (bits 28-29):
 * 2 for an .esd effect, 1 for an .esp effect, 0 for none. */
INCLUDE_ASM("asm/main/nonmatchings/srs", srsGetEffectFileKind);

/* Formats the effect stem into buffer: the kind prefix from
 * srsEffectKindPrefix, the index as three digits, and, when mode is nonzero,
 * an underscore and the low ten bits of the key as three digits. A negative
 * key prints "bad effect no:%d" instead. */
INCLUDE_ASM("asm/main/nonmatchings/srs", srsFormatEffectStem);

void srsFormatEffectStem(int effectNo, char *buffer, int mode);
extern void *memset(void *destination, int value, unsigned int size);
extern char srsEffectStemBuffer[];

/* Returns the effect stem with its sub number, in srsEffectStemBuffer. */
char *srsGetEffectStem(int effectNo)
{
    srsFormatEffectStem(effectNo, srsEffectStemBuffer, 1);
    return srsEffectStemBuffer;
}

/* Writes *charNo (the kind, for an .esp effect) and *effectType, and returns
 * the file name of the effect's .esp or .esd file, or 0 when it has none. */
INCLUDE_ASM("asm/main/nonmatchings/srs", srsAnalyzeEftNo);

/* Sets the load mode: 0 CD, 1 host, 2 USB. */
void srsSetLoadMode(int load_mode)
{
    srsLoadMode = load_mode;
}

/* Returns the load mode set by srsSetLoadMode. */
int srsGetLoadMode(void)
{
    return srsLoadMode;
}

void srsSetViewPath(char *path)
{
    strcpy(srsViewPath, path);
}

/* Returns the view path set by srsSetViewPath. */
char *srsGetViewPath(void)
{
    return srsViewPath;
}

/* Builds the resource path for the current load mode from the mode's root
 * ("simajiri/", "host0:/home/xeno/work/" or "usb0:/") and srsViewPath. */
INCLUDE_ASM("asm/main/nonmatchings/srs", srsMakeFileName);

/* Returns the file length for the current load mode, rounded up to a 2 KB
 * sector, or 0 when the file cannot be opened. */
INCLUDE_ASM("asm/main/nonmatchings/srs", srsGetFileLen);

void srsChangeSeparator(char *path)
{
    int index;

    if (path != 0) {
        for (index = 0; path[index] != '\0'; index++) {
            if (path[index] == '/')
                path[index] = '\\';
        }
    }
}

/* Loads a file by name for the current load mode. Mode 1 leaves the CD read
 * counted in _nRead for srsCdReadCallback; mode 0 does not. */
INCLUDE_ASM("asm/main/nonmatchings/srs", fileLoad);

/* Loads a file with mode 0 (the read is not left counted in _nRead). */
int srsLoadFileSync(void *buffer, const char *name)
{
    return fileLoad(buffer, name, 0);
}

extern const char srsEsdPathFormat[];
extern char srsEsdNameBuffer[];

/* Returns esd/<stem>.esd for a positive effect number, in srsEsdNameBuffer. */
char *srsGetEffectName(int effectNo)
{
    char stem[256];
    char *name;

    name = 0;
    if (effectNo > 0) {
        srsFormatEffectStem(effectNo, stem, 1);
        sprintf(srsEsdNameBuffer, srsEsdPathFormat, stem);
        name = srsEsdNameBuffer;
    }
    return name;
}

extern const char srsEspPathFormat[];
extern char srsEspNameBuffer[];

/* Returns esp/<stem>.esp for a positive effect number, in srsEspNameBuffer. */
char *srsGetEspName(int effectNo)
{
    char stem[256];

    if (effectNo <= 0) {
        return 0;
    }
    srsFormatEffectStem(effectNo, stem, 0);
    sprintf(srsEspNameBuffer, srsEspPathFormat, stem);
    return srsEspNameBuffer;
}

/* Maps a kind-0 key whose index is 300..340 to slot (low nibble - 1); any
 * other key gives 100. */
int srsGetEffectSlot(int effectKey)
{
    int effectIndex;
    int slot;

    effectIndex = effectKey >> 12;
    slot = 100;
    if (((effectKey >> 28) & 3) == 0 && (unsigned int) ((effectIndex & 0x3FF) - 300) < 41) {
        slot = (effectKey & 0xF) - 1;
    }
    return slot;
}

void srsInitCdRead(void)
{
    _nRead = 0;
}

/* Returns _nRead, the number of CD reads still counted. */
int srsLeaveCdRead(void)
{
    return _nRead;
}

/* Read-completion callback for xglCdReadFile. Code 4 finishes a read: _nRead
 * drops (never below 0) and sresDataMapping runs when none is left. Codes -1
 * and -2 drop _nRead the same way. */
INCLUDE_ASM("asm/main/nonmatchings/srs", srsCdReadCallback);

int srsLoadEffectData(void *buffer, int effectNo)
{
    char *name;

    name = srsGetEffectName(effectNo);
    if (name == 0) {
        return -1;
    }
    return fileLoad(buffer, name, 1);
}

extern int srsEffectStamp;
extern unsigned char srsRecordTable[];

/* Clears the 58-record key table srsRecordTable (0x1C-byte records) and srsEffectStamp. */
void srsResetKeyTable(void)
{
    srsEffectStamp = 0;
    memset(srsRecordTable, 0, 0x658);
}
