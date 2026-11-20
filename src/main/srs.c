#include "common.h"
#include "shared.h"
#include "main/sef.h"
#include "main/sm_initilize.h"
#include "main/srs.h"
#include "main/sv_get_image_item.h"
#include "main/xgl_cd.h"
#include "main/xgl_debug_file.h"
#include "main/res_get_path.h"

extern int sprintf(char *destination, const char *format, ...);
extern char *strcpy(char *destination, const char *source);

void srsSetLoadMode(int load_mode);
static int _nRead;

#define SRS_EFFECT2_INDEX_ROWS 117   /* _srsEffect2IndexTbl */
#define SRS_PATH_LENGTH 256          /* resolved file path and view path */
#define SRS_SECTOR_SIZE 2048         /* CD file lengths are rounded up to this */
/* File kinds returned by srsGetEffectFileKind: none, a .esp file or a .esd file. */
enum SrsEffectFileKind {
    SRS_EFFECT_FILE_NONE = 0,
    SRS_EFFECT_FILE_ESP = 1,
    SRS_EFFECT_FILE_ESD = 2
};
/* sceOpen flags and sceLseek origins used by the host loader. */
#define SRS_OPEN_RDONLY 1
#define SRS_OPEN_MODE_READ_ALL 0444  /* file mode bits passed to sceOpen (292) */
#define SRS_SEEK_SET 0
#define SRS_SEEK_END 2
/* xglCdReadFile completion callback: the read finished. */
#define SRS_CD_READ_COMPLETE 4
/* xglCdReadFile result that asks for the read to be retried in mode 1. */
#define SRS_CD_READ_RETRY -3
/* svAddImageMapper "height" (the fourth argument) registered for each resource. */
#define SRS_MAPPER_HEIGHT_IMAGE 3000
#define SRS_MAPPER_HEIGHT_EFFECT_IMAGE 3100
#define SRS_MAPPER_HEIGHT_WEAPON 2000
#define SRS_MAPPER_HEIGHT_COMBO 2100
#define SRS_MAPPER_HEIGHT_BATTLE_IMAGE 3400
#define SRS_MAPPER_HEIGHT_CF_IMAGE 3500
#define SRS_MAPPER_HEIGHT_EFFECT 3600
#define SRS_MAPPER_HEIGHT_WAIT_SLOT 2000    /* srsRecordTable wait-slot records, the weapon effects' height */
/*
 * Effect number ranges (first number and count). Each range resolves to one
 * of the SrsResourceType categories in srsAnalyzeEftNo/srsGetEffectType.
 */
#define SRS_EFFECT_COMMON_FIRST 2600       /* default.esp */
/* Weapon effects 2900..2998 share the data of effects 100 lower. */
#define SRS_EFFECT_WEAPON_ALT_FIRST 2900
#define SRS_EFFECT_WEAPON_ALT_COUNT 99
#define SRS_EFFECT_WEAPON_ALT_OFFSET 100
/* effectType written by srsAnalyzeEftNo for .esd effects (also its initial value). */
#define SRS_EFFECT_TYPE_ESD 9

extern int sceOpen(const char *path, int flags, ...);
extern int sceLseek(int descriptor, int offset, int whence);
extern int sceRead(int descriptor, void *buffer, int bytes);
extern int sceClose(int descriptor);
char srsViewPath[SRS_PATH_LENGTH] = "";
extern signed char _srsEffect2IndexTbl[SRS_EFFECT2_INDEX_ROWS];


/* Classifies an effect key from its index (bits 12-21) and kind (bits 28-29):
 * 2 for an .esd effect, 1 for an .esp effect, 0 for none. */
int srsGetEffectFileKind(int index, int kind)
{
    int esdCategory = SRS_EFFECT_FILE_ESD;

    if (kind == 2) {
        return SRS_EFFECT_FILE_ESD;
    }
    if (kind == 1) {
        if (index == 200) {
            return SRS_EFFECT_FILE_NONE;
        }
        return ((u32)(index - 300) < 150U) ? kind : esdCategory;
    }
    if ((u32)(index - 1) < 10U) {
        return SRS_EFFECT_FILE_ESP;
    }
    if ((u32)(index - 101) < 3U) {
        return SRS_EFFECT_FILE_ESP;
    }
    if ((u32)(index - 500) < 300U) {
        return SRS_EFFECT_FILE_ESP;
    }
    return ((u32)(index - 300) < 100U) ? SRS_EFFECT_FILE_NONE : esdCategory;
}

int srsLoadMode = 0;
int srsEffectStamp = 0;
char srsPrefixEe[8] = "ee";
char srsPrefixCe[8] = "ce";
char srsPrefixPe[8] = "pe";
char *srsEffectKindPrefix[4] = { srsPrefixPe, srsPrefixCe, srsPrefixEe, 0 };

/* Formats the effect stem into buffer: the kind prefix from
 * srsEffectKindPrefix, the index as three digits, and, when withSubNo is
 * nonzero, an underscore and the low ten bits of the key as three digits. A
 * negative key prints "bad effect no:%d" instead. */
void srsFormatEffectStem(int effectNo, char *buffer, int withSubNo)
{
    int effectKind = (effectNo >> 28) & 3;
    int effectIndex = (effectNo >> 12) & 0x3ff;
    int effectSub = effectNo & 0x3ff;

    if (effectNo < 0) {
        sprintf(buffer, "bad effect no:%d", effectNo);
        return;
    }
    if (withSubNo != 0) {
        sprintf(buffer, "%s%03d_%03d", srsEffectKindPrefix[effectKind], effectIndex, effectSub);
        return;
    }
    sprintf(buffer, "%s%03d", srsEffectKindPrefix[effectKind], effectIndex);
}

extern void *memset(void *destination, int value, unsigned int size);
static char srsEffectStemBuffer[16];

/* Returns the effect stem with its sub number, in srsEffectStemBuffer. */
char *srsGetEffectStem(int effectNo)
{
    srsFormatEffectStem(effectNo, srsEffectStemBuffer, 1);
    return srsEffectStemBuffer;
}

static char srsEftNameBuffer[256];
extern char *strcat(char *dest, const char *src);

/* Writes *charNo (the kind, for an .esp effect) and *effectType, and returns
 * the file name of the effect's .esp or .esd file, or 0 when it has none. */
char *srsAnalyzeEftNo(int effectNo, int *charNo, int *effectType)
{
    char *name = 0;
    int effectKind = (effectNo >> 28) & 3;
    int effectIndex = (effectNo >> 12) & 0x3ff;
    int fileKind;

    *charNo = 0;
    *effectType = SRS_EFFECT_TYPE_ESD;
    if (effectNo == 0) {
        return 0;
    }
    fileKind = srsGetEffectFileKind(effectIndex, effectKind);
    switch (fileKind) {
        case SRS_EFFECT_FILE_NONE:
            *charNo = 0;
            *effectType = 0;
            break;
        case SRS_EFFECT_FILE_ESP:
            *charNo = effectKind;
            *effectType = fileKind;
            srsFormatEffectStem(effectNo, srsEftNameBuffer, 0);
            strcat(srsEftNameBuffer, ".esp");
            name = srsEftNameBuffer;
            break;
        case SRS_EFFECT_FILE_ESD:
            *charNo = 0;
            *effectType = SRS_EFFECT_TYPE_ESD;
            srsFormatEffectStem(effectNo, srsEftNameBuffer, 1);
            strcat(srsEftNameBuffer, ".esd");
            name = srsEftNameBuffer;
            break;
    }
    return name;
}

/* Stores the load mode as given: SRS_LOAD_MODE_CD, _HOST, _USB or _DEB. */
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

/* Root text of each load mode, copied as one struct: "simajiri/" (CD, 9 chars
 * and the terminator), "host0:/home/xeno/work/" (host) and "usb0:/" (USB). */
typedef struct { char text[10]; } SrsCdRoot;

typedef struct { char text[23]; } SrsHostRoot;

typedef struct { char text[7]; } SrsUsbRoot;

/* Builds the resource path for the current load mode: CD "simajiri/" + fileName;
 * host "host0:/home/xeno/work/" or USB "usb0:/" + srsViewPath + fileName; DEB
 * srsViewPath + fileName. */
int srsMakeFileName(char *buffer, const char *fileName)
{
    if (fileName == 0) {
        buffer[0] = 0;
        return 0;
    }
    if (srsLoadMode == SRS_LOAD_MODE_CD) {
        *(SrsCdRoot *)buffer = *(SrsCdRoot *)"simajiri/";
        strcat(buffer, fileName);
    } else if (srsLoadMode == SRS_LOAD_MODE_HOST) {
        *(SrsHostRoot *)buffer = *(SrsHostRoot *)"host0:/home/xeno/work/";
        strcat(buffer, srsViewPath);
        strcat(buffer, fileName);
    } else if (srsLoadMode == SRS_LOAD_MODE_DEB) {
        strcpy(buffer, srsViewPath);
        strcat(buffer, fileName);
    } else if (srsLoadMode == SRS_LOAD_MODE_USB) {
        *(SrsUsbRoot *)buffer = *(SrsUsbRoot *)"usb0:/";
        strcat(buffer, srsViewPath);
        strcat(buffer, fileName);
    }
    return 1;
}

/* Script-record state that scGetKeyRecord (sc_get.c) sets and sef.c reads: the
 * mode of the record found (SEF_START_MODE_* in sef.h), the group count of a
 * mode-0x3E record, the group key sefAdvanceGroupKey steps (a halfword), and
 * the entry of a mode-0x49 record (SefEntryHeader in sef.c). */
int scKeyRecordMode = 0;
int scKeyRecordGroupCount = 0;
/* Read and written as a halfword (sef.c declares it unsigned short). An unsigned short definition
 * leaves the two bytes after it to the alignment pad, and data_carve refuses that .sdata span. */
int sefGroupKey = 0;
struct SefEntryHeader *scKeyRecordEntry = 0;

/* Returns the file length, rounded up to a 2 KB sector, for the CD, host and
 * DEB load modes; 0 when the file cannot be opened, and 0 in USB mode (no
 * size query). */
int srsGetFileLen(const char *fileName)
{
    char path[SRS_PATH_LENGTH];
    int fileLen = 0;
    int descriptor;

    if (fileName == 0) {
        return 0;
    }
    if (srsMakeFileName(path, fileName) != 0) {
        if (srsLoadMode == SRS_LOAD_MODE_CD) {
            srsChangeSeparator(path);
            fileLen = xglCdGetFileSize(path);
            if (fileLen <= 0) {
                fileLen = 0;
            }
        } else if (srsLoadMode == SRS_LOAD_MODE_HOST) {
            descriptor = sceOpen(path, SRS_OPEN_RDONLY, SRS_OPEN_MODE_READ_ALL);
            if (descriptor >= 0) {
                fileLen = sceLseek(descriptor, 0, SRS_SEEK_END);
                sceLseek(descriptor, 0, SRS_SEEK_SET);
                sceClose(descriptor);
            }
        } else if (srsLoadMode == SRS_LOAD_MODE_DEB) {
            fileLen = xglDebFileSize(path);
        }
    }
    if (fileLen <= 0) {
        return 0;
    }
    return (fileLen + (SRS_SECTOR_SIZE - 1)) & -SRS_SECTOR_SIZE;
}

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

int fileLoad(void *buffer, const char *name, int readMode)
{
    char path[SRS_PATH_LENGTH];
    int size = 0;

    if (srsMakeFileName(path, name) == 0) {
        return -1;
    }
    if (srsLoadMode == SRS_LOAD_MODE_CD) {
        CdCompletionCallback *readCallback;
        int readResult;

        srsChangeSeparator(path);
        size = xglCdGetFileSize(path);
        if (size <= 0) {
            return -1;
        }
        readCallback = srsCdReadCallback;
        _nRead++;
        readResult = xglCdReadFile(path, buffer, readMode, readCallback);
        if (readResult == SRS_CD_READ_RETRY) {
            readResult = xglCdReadFile(path, buffer, 1, readCallback);
            readMode = 1;
        }
        if (readResult > 0 && readMode == 0) {
            size = readResult;
            _nRead--;
            if (_nRead < 0) {
                _nRead = 0;
            }
        } else if (readResult < 0 && readMode == 0) {
            _nRead--;
        }
    } else if (srsLoadMode == SRS_LOAD_MODE_HOST) {
        int descriptor = sceOpen(path, SRS_OPEN_RDONLY);

        if (descriptor < 0) {
            return -1;
        }
        size = sceLseek(descriptor, 0, SRS_SEEK_END);
        sceLseek(descriptor, 0, SRS_SEEK_SET);
        size = sceRead(descriptor, buffer, size);
        sceClose(descriptor);
        sresDataMapping();
    } else if (srsLoadMode == SRS_LOAD_MODE_DEB) {
        size = xglDebFileSize(path);
        if (size <= 0) {
            return -1;
        }
        xglDebFileRead(path, buffer);
        sresDataMapping();
    } else if (srsLoadMode == SRS_LOAD_MODE_USB) {
        size = xglDebFileRead(path, buffer);
        if (size > 0) {
            sresDataMapping();
        }
    }
    return size;
}

/* Loads a file with read mode 0 (fileLoad may retry it in mode 1). */
int srsLoadFileSync(void *buffer, const char *name)
{
    return fileLoad(buffer, name, 0);
}

const char srsEsdPathFormat[16] = "esd/%s.esd";
static char srsEsdNameBuffer[256];

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

const char srsEspPathFormat[16] = "esp/%s.esp";
static char srsEspNameBuffer[256];

/* Returns esp/<stem>.esp for a positive effect number, in srsEspNameBuffer. */
char *srsGetEffectName2(int effectNo)
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
void srsCdReadCallback(int readStatus, int value, int tag)
{
    int remaining;

    switch (readStatus) {
    case SRS_CD_READ_COMPLETE:
        remaining = _nRead - 1;
        if (remaining < 0) {
            remaining = 0;
        }
        _nRead = remaining;
        if (remaining == 0) {
            sresDataMapping();
        }
        break;
    case -2:
    case -1:
        remaining = _nRead - 1;
        if (remaining >= 0) {
            goto store; /* store shared with the retry case (bgezl delay-slot store); if/else is if-converted to movn, and a fall-through into the retry case recomputes _nRead - 1 and renumbers registers (both tried) */
        }
        _nRead = 0;
        break;
    case SRS_CD_READ_RETRY:
        remaining = _nRead - 1;
    store:
        _nRead = remaining;
        break;
    case 0:
    case 1:
    case 2:
    case 3:
        break;
    }
}

int srsLoadEffectData(void *buffer, int effectNo)
{
    char *name;

    name = srsGetEffectName(effectNo);
    if (name == 0) {
        return -1;
    }
    return fileLoad(buffer, name, 1);
}

SrsTable srsRecordTable = { 0 };

/* Clears the 58-record table srsRecordTable and srsEffectStamp. */
void sresInitMemoryRes(void)
{
    srsEffectStamp = 0;
    memset(&srsRecordTable, 0, sizeof(srsRecordTable));
}

#define SRS_ESP_BUFFER_SIZE 0x28800  /* buffer of the effect package (record 0) */
/* An effect key: kind in bits 28..29, index in bits 12..21, sub number in bits 0..9. */
#define SRS_EFFECT_KEY(kind, index, sub) (((kind) << 28) | ((index) << 12) | (sub))
/* Index and sub number of the effect package's key (its kind is 1 for cfdef.esp, 0 for default.esp). */
#define SRS_ESP_PACKAGE_INDEX 999
#define SRS_ESP_PACKAGE_SUB 500
#define SRS_SLOT_BUFFER_SIZE 0x1C000 /* buffer of a wait-slot record */
#define SRS_BLOCK_BUFFER_SIZE 0x10000 /* buffer srsLoadEffectBlock gives wait slot 0 */


/* Loads an effect package file ("esp/cfdef.esp" when cfdef is set, else
 * "esp/default.esp") into the buffer of record 0 of srsRecordTable, allocating
 * it on first use. When the buffer already holds the package with the same key
 * and reload is 0, only its image mapper is registered again; otherwise the
 * file is read again. A failed first read clears the key and falls through
 * into that shared tail, so the file is read a second time. */
void srsLoadEspFile(int reload, int cfdef)
{
    const char *path;
    int key;

    if (cfdef != 0) {
        path = "esp/cfdef.esp";
    } else {
        path = "esp/default.esp";
    }
    key = cfdef ? SRS_EFFECT_KEY(1, SRS_ESP_PACKAGE_INDEX, SRS_ESP_PACKAGE_SUB)
                : SRS_EFFECT_KEY(0, SRS_ESP_PACKAGE_INDEX, SRS_ESP_PACKAGE_SUB);
    if (srsRecordTable.package.handle == 0) {
        srsRecordTable.package.handle = smAlloc(SRS_ESP_BUFFER_SIZE);
        if (srsRecordTable.package.handle == 0) {
            return;
        }
        srsRecordTable.package.busy = 1;
        srsRecordTable.package.key = key;
        if (fileLoad(srsRecordTable.package.handle, path, 0) >= 0) {
            if (cfdef != 0) {
                xglCdWaitUntilIdle();
            }
            return;
        }
        srsRecordTable.package.busy = 0;
        srsRecordTable.package.key = 0;
        if (srsRecordTable.package.handle == 0) {
            return;
        }
    }
    svDeleteImageMapper(0);
    if (reload == 0 && srsRecordTable.package.key == key) {
        srsRecordTable.package.busy = 1;
        sresDataMapping();
        return;
    }
    srsRecordTable.package.busy = 1;
    srsRecordTable.package.key = key;
    if (fileLoad(srsRecordTable.package.handle, path, 0) >= 0) {
        if (cfdef != 0) {
            xglCdWaitUntilIdle();
        }
    } else {
        srsRecordTable.package.busy = 0;
        srsRecordTable.package.key = 0;
    }
}

/* Loads the default effect package. */
void srsLoadDefaultEsp(void)
{
    srsLoadEspFile(0, 0);
}

/* Decodes an effect key: index = bits 12..21, kind = bits 28..29. Returns the
 * index when srsGetEffectFileKind accepts the pair, else 0. */
int srsCheckEffectKey(int effectKey)
{
    int index;
    int kind;

    index = (effectKey >> 0xC) & 0x3FF;
    kind = (effectKey >> 0x1C) & 3;
    if (srsGetEffectFileKind(index, kind) != 1) {
        return 0;
    }
    return index;
}


/* Clears every wait-slot record (records 1..8) whose key index equals
 * index, releasing its image mapper and buffer first when it has one. */
void srsReleaseSlotsByIndex(int index)
{
    int keyIndex;
    int i;

    for (i = 0; i < SRS_WAIT_SLOT_COUNT; i++) {
        keyIndex = (srsRecordTable.waitSlots[i].key >> 12) & 0x3FF;
        if (keyIndex != 0 && keyIndex == index) {
            if (srsRecordTable.waitSlots[i].handle != 0) {
                svDeleteImageMapper(i + 1);
                smFree(srsRecordTable.waitSlots[i].handle);
            }
            memset(&srsRecordTable.waitSlots[i], 0, sizeof(SrsRecord));
        }
    }
}


/* Releases records 9..56 (image mapper i + 9): the buffer goes back to smFree
 * or GameResBlockFreeByBuffer by kind, then the record is cleared. */
void srsReleaseEffectRecords(void)
{
    int i;

    for (i = 0; i < SRS_SLOT_COUNT; i++) {
        if (srsRecordTable.slots[i].handle != 0) {
            svDeleteImageMapper(i + SRS_KEY_RECORD_COUNT);
            if (srsRecordTable.slots[i].kind == SRS_KIND_SM_BLOCK) {
                smFree(srsRecordTable.slots[i].handle);
            } else if (srsRecordTable.slots[i].kind == SRS_KIND_GAME_RES_BLOCK) {
                GameResBlockFreeByBuffer(srsRecordTable.slots[i].handle);
            }
            memset(&srsRecordTable.slots[i], 0, sizeof(SrsRecord));
        }
    }
}

/* Releases every wait-slot record (records 1..8) that holds a buffer. */
void srsReleaseAllSlots(void)
{
    int i;

    for (i = 0; i < SRS_WAIT_SLOT_COUNT; i++) {
        if (srsRecordTable.waitSlots[i].handle != 0) {
            svDeleteImageMapper(i + 1);
            smFree(srsRecordTable.waitSlots[i].handle);
            memset(&srsRecordTable.waitSlots[i], 0, sizeof(SrsRecord));
        }
    }
}

void sresFreeReloaderMemory(void)
{
    srsReleaseAllSlots();
    srsReleaseEffectRecords();
}

void sresFreeMemoryRes(void)
{
    sresFreeReloaderMemory();
    if (srsRecordTable.package.handle != 0) {
        svDeleteImageMapper(0);
        smFree(srsRecordTable.package.handle);
        memset(&srsRecordTable.package, 0, sizeof(SrsRecord));
    }
}

/* Registers the image mapper of every loaded record still flagged
 * busy (image mapper still owed): the effect package (mapper 0), the wait-slot
 * records (mappers 1..8, kind from key bits 28..29) and the effect records
 * (mappers 9..56). */
void sresDataMapping(void)
{
    /* svAddImageMapper reads only chunk[0]. Declared as void *chunk[1] (compiled 2026-11-18) the frame is
     * 64 bytes and the saves sit 16 bytes lower; the original reserves 0x20 bytes for it (an 80-byte frame). */
    void *chunk[8];
    int i;

    if (srsRecordTable.package.handle != 0 && srsRecordTable.package.busy != 0) {
        chunk[0] = srsRecordTable.package.handle;
        svAddImageMapper(0, 0, chunk, SRS_MAPPER_HEIGHT_IMAGE);
        srsRecordTable.package.busy = 0;
    }
    for (i = 0; i < SRS_WAIT_SLOT_COUNT; i++) {
        if (srsRecordTable.waitSlots[i].handle != 0 && srsRecordTable.waitSlots[i].busy != 0) {
            chunk[0] = srsRecordTable.waitSlots[i].handle;
            svAddImageMapper(i + 1, (srsRecordTable.waitSlots[i].key >> 28) & 3, chunk, SRS_MAPPER_HEIGHT_WAIT_SLOT);
            srsRecordTable.waitSlots[i].busy = 0;
        }
    }
    for (i = 0; i < SRS_SLOT_COUNT; i++) {
        if (srsRecordTable.slots[i].handle != 0 && srsRecordTable.slots[i].busy != 0) {
            chunk[0] = srsRecordTable.slots[i].handle;
            svAddImageMapper(i + SRS_KEY_RECORD_COUNT, 0, chunk, SRS_MAPPER_HEIGHT_EFFECT);
            srsRecordTable.slots[i].busy = 0;
        }
    }
}

/* Loads the .esp package of every wait slot flagged loadPending into the
 * buffer of wait-slot record i, flags it busy, and sets
 * its key when the file fits. */
void srsLoadPendingEffects(SefSlotTable *slotTable)
{
    char *name;
    int size;
    int i;

    for (i = 0; i < SRS_WAIT_SLOT_COUNT; i++) {
        if (slotTable->effectNo[i] > 0 && slotTable->loadPending[i] != 0) {
            slotTable->loadPending[i] = 0;
            name = srsGetEffectName2(slotTable->effectNo[i] << 12);
            if (srsRecordTable.waitSlots[i].handle == 0) {
                srsRecordTable.waitSlots[i].handle = smAlloc(SRS_SLOT_BUFFER_SIZE);
            }
            srsRecordTable.waitSlots[i].busy = 1;
            srsRecordTable.waitSlots[i].effectNo = slotTable->effectNo[i];
            size = fileLoad(srsRecordTable.waitSlots[i].handle, name, 1);
            if (size <= 0) {
                srsRecordTable.waitSlots[i].busy = 0;
            } else if (size <= SRS_SLOT_BUFFER_SIZE) {
                srsRecordTable.waitSlots[i].key = slotTable->effectNo[i] << 12;
            }
        }
    }
}


/* Loads the .esp package of effectKey (its low 10 bits dropped) into wait slot
 * 0 (record 1, image mapper 1), allocating its 0x10000-byte buffer on first
 * use. When the slot already holds the same key, only its image mapper is
 * registered again. Returns -1 without a buffer, else 1. */
int srsLoadEffectBlock(int effectKey)
{
    int key = effectKey & ~0x3FF;
    void *buffer = srsRecordTable.waitSlots[0].handle;
    int result = -1;
    char *name;
    int size;

    if (buffer != 0 && srsRecordTable.waitSlots[0].key == key) {
        svDeleteImageMapper(1);
        srsRecordTable.waitSlots[0].busy = 1;
        sresDataMapping();
        return 1;
    }
    if (buffer == 0) {
        srsRecordTable.waitSlots[0].handle = smAlloc(SRS_BLOCK_BUFFER_SIZE);
    } else {
        svDeleteImageMapper(1);
        memset(&srsRecordTable.waitSlots[0], 0, sizeof(SrsRecord));
        srsRecordTable.waitSlots[0].handle = buffer;
        sefReleaseImageRecords();
    }
    if (srsRecordTable.waitSlots[0].handle != 0) {
        name = srsGetEffectName2(key);
        srsRecordTable.waitSlots[0].busy = 1;
        size = fileLoad(srsRecordTable.waitSlots[0].handle, name, 0);
        if (size <= 0) {
            srsRecordTable.waitSlots[0].busy = 0;
        } else if (size <= SRS_BLOCK_BUFFER_SIZE) {
            srsRecordTable.waitSlots[0].key = key;
            xglCdWaitUntilIdle();
        }
        result = 1;
    }
    return result;
}
