#include "common.h"
#include "shared.h"
#include "main/srs.h"
#include "main/sv_get_image_item_159.h"
#include "main/xgl_cd.h"

extern char *strcat(char *destination, const char *source);
extern u8 *buffer;

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

int fileLoad(void *buffer, const char *name, int mode);

extern int srsLoadMode;
extern int _nRead;

#define SRS_PATH_LENGTH 256          /* resolved file path and view path */

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

extern void sresDataMapping(void);
extern int sceOpen(const char *path, int flags, ...);
extern int sceLseek(int descriptor, int offset, int whence);
extern int sceRead(int descriptor, void *buffer, int bytes);
extern int sceClose(int descriptor);
extern int func_00A103C8(const char *path, void *buffer);
extern void srsCdReadCallback(int code);
extern char srsViewPath[SRS_PATH_LENGTH];

int srsMakeFileName(char *buffer, const char *fileName);

void srsCdReadCallback(int code);
void sresDataMapping(void);

/* Returns 1 when the effect key is of kind 1 (effect image) with an index in
 * 300..449, else 0. */
INCLUDE_ASM("asm/main/nonmatchings/tu179", srsIsEffectImageKey);

int srsFileLoad(void *buffer, const char *name, int mode)
{
    return fileLoad(buffer, name, mode);
}

/* Parses a nine-character effect name into an effect key: a two-letter prefix
 * gives the kind (ce is 1, ee is 2, anything else 0), three decimal digits the
 * index and three the sub-number. Returns 0 when the index field comes out
 * negative. */
INCLUDE_ASM("asm/main/nonmatchings/tu179", srsEffectKeyFromName);
