#include "common.h"
#include "main/xgl_cd.h"


/* Writes the low 16 bits of number as a 6-byte name: "cf" then four decimal digits, no NUL. */
void xglCdFormatFileName(char *buffer, int number)
{
    unsigned int remaining;
    int digit;

    remaining = number & 0xFFFF;
    buffer[0] = 'c';
    buffer[1] = 'f';
    for (digit = 0; digit < 4; digit++) {
        buffer[5 - digit] = remaining % 10U + '0';
        remaining = remaining / 10U;
    }
}

/* One 0x14-byte entry of the block table xglCdSlotTable, indexed by slot. The table
 * describes memory blocks in address order; CD reads and other allocations (e.g. the pool
 * cfeNodePoolInit takes through func_001DF928) share it:
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
    int tag;
    int value;
} CdSlot;

/* Entries of xglCdSlotTable; every search walks indexes 1 .. GAME_RES_BLOCK_COUNT - 1. */
#define GAME_RES_BLOCK_COUNT 0x80
/* Block sizes are rounded up to this (GameResBlockAlloc). */
#define GAME_RES_BLOCK_ALIGN 0x40
/* A block loaded by name starts with the name in a header of this size (GameResLoadName). */
#define GAME_RES_NAME_HEADER_SIZE 0x40
/* File sizes are rounded up to whole CD sectors. */
#define CD_SECTOR_SIZE 0x800
/* Owner tag of a block allocated without an owner, and the mask selecting that tag. */
#define GAME_RES_OWNER 0xF00000
#define GAME_RES_OWNER_MASK 0xFF0000
/* Resource ids carry the file id in their low 24 bits (GameResLoadID). */
#define GAME_RES_ID_MASK 0xFFFFFF

extern CdSlot xglCdSlotTable[];
/* Address range D_0069E1A4 .. D_0069E1A8 whose blocks GameResBlockFindByOwner and GameResBlockFindByName skip. */
extern unsigned int D_0069E1A4;
extern unsigned int D_0069E1A8;

/* The name key stored in a block header: name itself when it fits the header; for longer
 * names the original returns name - (GAME_RES_NAME_HEADER_SIZE - 1), not an offset into name. */
char *func_001DF170(char *name)
{
    int length;

    for (length = 0; name[length] != 0; length++) {
    }
    if (length < GAME_RES_NAME_HEADER_SIZE) {
        return name;
    }
    return name - (GAME_RES_NAME_HEADER_SIZE - 1);
}

/* Best fit: the free block whose size equals size, else the smallest larger one; 0 when none. */
int GameResBlockFindBestFit(unsigned int size)
{
    unsigned int best_waste;
    int best;
    int index;

    best_waste = -1;
    best = 0;
    for (index = xglCdSlotTable[0].owner; index < GAME_RES_BLOCK_COUNT && xglCdSlotTable[index].buffer != 0; index++) {
        if (xglCdSlotTable[index].owner == 0) {
            if (xglCdSlotTable[index].size == size) {
                return index;
            }
            if (size < xglCdSlotTable[index].size && xglCdSlotTable[index].size - size < best_waste) {
                best_waste = xglCdSlotTable[index].size - size;
                best = index;
            }
        }
    }
    return best;
}

/* The first block owned by owner whose buffer lies outside D_0069E1A4 .. D_0069E1A8. */
int GameResBlockFindByOwner(int owner)
{
    int index;

    if (owner == 0) {
        return 0;
    }
    for (index = 1; index < GAME_RES_BLOCK_COUNT && xglCdSlotTable[index].buffer != 0; index++) {
        if (((unsigned int) xglCdSlotTable[index].buffer < D_0069E1A4
             || D_0069E1A8 < (unsigned int) xglCdSlotTable[index].buffer)
            && xglCdSlotTable[index].owner == owner) {
            return index;
        }
    }
    return 0;
}

/* The index of the block starting at buffer, or 0. */
int GameResBlockFindByBuffer(void *buffer)
{
    int index;

    if (buffer == 0) {
        return 0;
    }
    for (index = 1; index < GAME_RES_BLOCK_COUNT && xglCdSlotTable[index].buffer != 0; index++) {
        if (xglCdSlotTable[index].buffer == buffer) {
            return index;
        }
    }
    return 0;
}

/* The block outside D_0069E1A4 .. D_0069E1A8 whose owner is a GAME_RES_OWNER tag and whose
 * first bytes hold the key func_001DF170 makes of name; 0 when none. GameResLoadName writes
 * that key as the header of the blocks it loads, but the test applies to every block
 * allocated without an owner, anonymous ones included. */
int GameResBlockFindByName(char *name)
{
    char *key;
    char *wanted;
    char *stored;
    int index;

    key = func_001DF170(name);
    for (index = 1; index < GAME_RES_BLOCK_COUNT && xglCdSlotTable[index].buffer != 0; index++) {
        if (((unsigned int) xglCdSlotTable[index].buffer < D_0069E1A4
             || D_0069E1A8 < (unsigned int) xglCdSlotTable[index].buffer)
            && (xglCdSlotTable[index].owner & GAME_RES_OWNER_MASK) == GAME_RES_OWNER) {
            wanted = key;
            stored = xglCdSlotTable[index].buffer;
            while (*wanted == *stored) {
                if (*wanted == 0) {
                    return index;
                }
                wanted++;
                stored++;
            }
        }
    }
    return 0;
}

/* Gives block index to owner, first splitting off the bytes beyond size as a new free block. */
void *GameResBlockAssign(int index, int owner, unsigned int size)
{
    CdSlot *entry;
    int i;

    entry = &xglCdSlotTable[index];
    if (entry->size != size) {
        for (i = GAME_RES_BLOCK_COUNT - 2; index < i; i--) {
            xglCdSlotTable[i + 1].buffer = xglCdSlotTable[i].buffer;
            xglCdSlotTable[i + 1].size = xglCdSlotTable[i].size;
            xglCdSlotTable[i + 1].owner = xglCdSlotTable[i].owner;
            xglCdSlotTable[i + 1].tag = xglCdSlotTable[i].tag;
            xglCdSlotTable[i + 1].value = xglCdSlotTable[i].value;
        }
        entry[1].size = entry->size - size;
        entry->size = size;
        entry[1].buffer = (char *) entry->buffer + size;
        entry[1].owner = 0;
    }
    entry->owner = owner;
    return entry->buffer;
}

INCLUDE_ASM("asm/main/nonmatchings/tu054", GameResBlockFree);

#include "shared.h"

/* Completion callback of xglCdReadFile: slot index, buffer, state word, value. */
typedef void (*CdCallback)(int slot, void *buffer, int state, int value);

extern int xglCdEventState;

/* Completion handler passed to xglCdReadFile (proposed xglCdSlotEvent): event 1 stores value as the
 * state word xglCdEventState (xglCdEventState); event 4 calls the slot's callback with that state word. */
void xglCdSlotEvent(int event, int value, int slot) {
    CdSlot *entry;
    CdCallback callback;

    entry = &xglCdSlotTable[slot];
    callback = (CdCallback) entry->tag;
    if (callback != 0) {
        if (event == 1) {
            xglCdEventState = value;
        } else if (event == 4) {
            callback(slot, entry->buffer, xglCdEventState, entry->value);
        }
    }
}

/* One 0xC-byte entry of xglCdFileGroups: file ids base .. base + count - 1 map to
 * names[0 .. count - 1] (xglCdFindFileId, xglCdFindFileName); xglCdBuildFileName also writes
 * names[0] in front of the file's own name. An entry with no names ends the table. */
typedef struct CdFileGroup {
    unsigned int base;
    unsigned int count;
    char **names;
} CdFileGroup;

extern CdFileGroup xglCdFileGroups[];
extern int strcmp(const char *a, const char *b);

void GameResBlockFree(int index);

/* Returns the group whose id range holds file_id, or 0 when none does. */
CdFileGroup *xglCdFindFileGroup(unsigned int file_id)
{
    CdFileGroup *group;

    for (group = xglCdFileGroups; group->names != 0; group++) {
        if (file_id >= group->base && file_id < group->base + group->count) {
            return group;
        }
    }
    return 0;
}

/* The name of file_id within its group, or 0 when no group holds it. */
char *xglCdFindFileName(unsigned int file_id)
{
    CdFileGroup *group;

    group = xglCdFindFileGroup(file_id);
    if (group == 0) {
        return 0;
    }
    return group->names[file_id - group->base];
}

/* Returns the file id whose name is name, or -1 when no group lists it. */
int xglCdFindFileId(char *name)
{
    CdFileGroup *group;
    unsigned int index;

    for (group = xglCdFileGroups; group->names != 0; group++) {
        for (index = 0; index < group->count; index++) {
            if (strcmp(group->names[index], name) == 0) {
                return index + group->base;
            }
        }
    }
    return -1;
}

/* Writes the group's names[0] and then file_id's name to buffer; returns the end of the
 * written name (its NUL), or buffer unchanged when no group holds file_id. */
char *xglCdBuildFileName(char *buffer, unsigned int file_id)
{
    CdFileGroup *group;
    char **names;
    char *out;
    char *source;

    group = xglCdFindFileGroup(file_id);
    out = buffer;
    if (group != 0) {
        names = group->names;
        for (source = names[0]; *source != 0; source++) {
            *out++ = *source;
        }
        source = names[file_id - group->base];
        while ((*out++ = *source++) != 0) {
        }
        return out - 1;
    }
    return buffer;
}

/* Frees the block starting at buffer; returns its former index, 0 when no block starts there. */
int GameResBlockFreeByBuffer(void *buffer)
{
    int index;

    index = GameResBlockFindByBuffer(buffer);
    if (index == 0) {
        return 0;
    }
    GameResBlockFree(index);
    return index;
}

/* Frees the block of resource owner; returns its former index, 0 when none holds it. */
int GameResBlockFreeByOwner(int owner)
{
    int index;

    index = GameResBlockFindByOwner(owner);
    if (index == 0) {
        return 0;
    }
    GameResBlockFree(index);
    return index;
}

/* Allocates a block of size bytes (rounded up to GAME_RES_BLOCK_ALIGN) for owner; owner 0
 * makes a GAME_RES_OWNER tag from the block's address. Returns the block index, 0 when none fits. */
int GameResBlockAlloc(int owner, int size)
{
    int index;

    size = (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    index = GameResBlockFindBestFit(size);
    if (index == 0) {
        return 0;
    }
    if (owner == 0) {
        owner = GAME_RES_OWNER + (((unsigned int) xglCdSlotTable[index].buffer >> 8) & 0xFFFF);
    }
    GameResBlockAssign(index, owner, size);
    return index;
}

INCLUDE_ASM("asm/main/nonmatchings/tu054", func_001DF928);

/* The largest free block, 0 when none is free. */
int GameResBlockFindLargestFree(void)
{
    unsigned int best_size;
    int best;
    int index;

    best_size = 0;
    best = 0;
    for (index = 1; index < GAME_RES_BLOCK_COUNT && xglCdSlotTable[index].buffer != 0; index++) {
        if (xglCdSlotTable[index].owner == 0 && best_size < xglCdSlotTable[index].size) {
            best_size = xglCdSlotTable[index].size;
            best = index;
        }
    }
    return best;
}

/* Buffer and size of the largest free block (entry 0's when none is free). */
void *func_001DF9F0(unsigned int *size)
{
    int index;

    index = GameResBlockFindLargestFree();
    if (size != 0) {
        *size = xglCdSlotTable[index].size;
    }
    return xglCdSlotTable[index].buffer;
}

INCLUDE_ASM("asm/main/nonmatchings/tu054", func_001DFA58);

/* Called by GameResLoadID and GameResLoadName for a block that was already loaded:
 * block index, buffer, size and xglCdSlotValue. Stored as a block's read tag otherwise. */
typedef void (*GameResCallback)(int index, void *buffer, unsigned int size, int value);

extern int xglCdSlotValue;

/* Sets the value that xglCdReadFileTagged and xglCdReadFileSlot copy into the slot (xglCdSlotValue). */
void xglCdSetSlotValue(int value)
{
    xglCdSlotValue = value;
}

int xglCdGetFileSize(const char *name);

int xglCdReadFileTagged(const char *name, void *buffer, int unused_arg2, int tag) {
    int status;

    status = xglCdGetFileSize(name);
    if (status > 0) {
        if (buffer != 0) {
            xglCdSlotTable[0].tag = tag;
            xglCdSlotTable[0].value = xglCdSlotValue;
            xglCdSetReadTag(0);
            xglCdReadFile(name, buffer, 1, xglCdSlotEvent);
        }
    }
    return status;
}

/* Slot index to buffer (xglCdSlotGetBuffer): an index below 0x80 returns that entry's buffer; others pass through. */
unsigned int xglCdSlotGetBuffer(unsigned int index)
{
    unsigned int result;

    result = index;
    if (result != 0) {
        if (result < 0x80U) {
            result = (unsigned int) xglCdSlotTable[result].buffer;
        }
    }
    return result;
}

extern u8 D_0069E1BC;

/* Starts a read into a slot (proposed xglCdReadFileSlot). tag 0 reads without a slot;
 * otherwise the slot (index 0 when slot >= 0x80) takes tag and xglCdSlotValue before xglCdReadFile. */
void xglCdReadFileSlot(const char *name, void *buffer, int tag, u32 slot) {
    CdSlot *entry;
    u32 index;
    int in_table;
    int value;

    in_table = slot < 0x80U;
    if (tag == 0) {
        if (D_0069E1BC != 0) {
            xglCdReadFile(name, buffer, 1, 0);
            return;
        }
        xglCdReadFile(name, buffer, tag, 0);
        return;
    }
    index = 0;
    entry = &xglCdSlotTable[0];
    if (in_table != 0) {
        index = slot;
        entry = &xglCdSlotTable[index];
    }
    value = xglCdSlotValue;
    entry->tag = tag;
    entry->value = value;
    xglCdSetReadTag(index);
    if (D_0069E1BC != 0) {
        xglCdReadFile(name, buffer, 1, xglCdSlotEvent);
        return;
    }
    xglCdReadFile(name, buffer, 0, xglCdSlotEvent);
}

/* Probes path and fills a file record (lsn at +0, size at +4, see tu022.c); -1 when missing, 0 otherwise. */
int xglCdCheckFile(const char *path, unsigned int *record);

/* Writes the extension ".xap" with its NUL at extension, the end of path, then probes path
 * (xglCdCheckFile): -1 when the .xap file is missing, 0 otherwise. */
int xglCdCheckXapFile(const char *path, char *extension)
{
    unsigned int record[0xC];

    extension[0] = '.';
    extension[1] = 'x';
    extension[2] = 'a';
    extension[3] = 'p';
    extension[4] = 0;
    return xglCdCheckFile(path, record);
}

INCLUDE_ASM("asm/main/nonmatchings/tu054", func_001DFCA8);

/* File type of a file id (bits 16-23), as the loader dispatcher func_001E0610 selects on it. */
#define CD_FILE_TYPE_MASK 0xFF0000
#define CD_FILE_TYPE(type) ((type) << 16)

/* Header of a file-set block (func_001DFD20): the data address of each file of the set,
 * 0 for a file that is missing. It takes the first 0x14 of GAME_RES_NAME_HEADER_SIZE bytes. */
typedef struct CdFileSetHeader {
    char *pxy;
    char *txy;
    char *xhr;
    char *epf;
    char *xap;
} CdFileSetHeader;

/* Still asm: D_002BB350 holds groups { name, file id, file id, ..., -1 } closed by a lone -1;
 * returns the name of the group that lists file_id, or 0. */
char *func_001DFCA8(int file_id);

/* Default .xap name "man.xap" for ids whose own and fallback names are all missing
 * (tu054's own .sdata, still asm-owned data). */
extern char D_0069CEE0[8];

/* Loads the file set of file_id into slot's block: a CdFileSetHeader, then the .xap file and
 * the name's .epf, .xhr, .txy and .pxy files, each rounded up to GAME_RES_BLOCK_ALIGN and the
 * last to CD_SECTOR_SIZE. The .xap name is the group's override (func_001DFCA8) or the file's
 * own name; when that file is missing, its trailing digits, then an 'H' before them, are
 * replaced by "00", and as the last resort (not for type 7) the default name "man.xap" is
 * appended to the group directory.
 * Returns the bytes the set takes. */
int func_001DFD20(unsigned int file_id, unsigned int slot, int tag)
{
    char name[256];
    char path[256];
    char *extension;
    char *end;
    char *base;
    char *source;
    CdFileGroup *group;
    char **names;
    CdFileSetHeader *header;
    char *buffer;
    char *start;
    int size;

    start = buffer = (char *) xglCdSlotGetBuffer(slot);
    extension = xglCdBuildFileName(name, file_id);
    extension[0] = '.';
    /* end is reused: it first clears the header, then walks the .xap path (the original's
     * register use: one pseudo for both). */
    end = buffer;
    header = (CdFileSetHeader *) buffer;
    buffer += GAME_RES_NAME_HEADER_SIZE;
    if (slot != 0) {
        ((CdFileSetHeader *) end)->txy = 0;
        ((CdFileSetHeader *) end)->xhr = 0;
        ((CdFileSetHeader *) end)->epf = 0;
        ((CdFileSetHeader *) end)->xap = 0;
    }
    end = path;
    names = xglCdFindFileGroup(CD_FILE_TYPE(3))->names;
    for (source = names[0]; *source != 0; source++) {
        *end++ = *source;
    }
    base = end;
    group = xglCdFindFileGroup(file_id);
    names = group->names;
    source = names[file_id - group->base];
    /* A string append in the original's do/while(0) macro form: its block boundary keeps end--
     * ahead of the argument setup of the next call. end is left on the NUL. */
    do {
        while ((*end++ = *source++) != 0) {
        }
        end--;
    } while (0);
    source = func_001DFCA8(file_id);
    if (source != 0) {
        while ((*base++ = *source++) != 0) {
        }
        base[-1] = '.';
        base[0] = 'x';
        base[1] = 'a';
        base[2] = 'p';
        base[3] = 0;
    } else if (xglCdCheckXapFile(path, end) != 0) {
        /* strip the trailing decimal digits */
        while ((unsigned int) ((unsigned char) end[-1] - '0') < 10) {
            end--;
        }
        if (xglCdCheckXapFile(path, end) != 0) {
            if (end[-1] == 'H') {
                end--;
            }
            end[0] = '0';
            end[1] = '0';
            if (xglCdCheckXapFile(path, end + 2) != 0
                && (file_id & CD_FILE_TYPE_MASK) != CD_FILE_TYPE(7)) {
                for (source = D_0069CEE0; (*base++ = *source++) != 0;) {
                }
            }
        }
    }
    size = xglCdGetFileSize(path);
    if (size > 0) {
        if (slot != 0) {
            header->xap = buffer;
            xglCdReadFileSlot(path, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }
    extension[1] = 'e';
    extension[2] = 'p';
    extension[3] = 'f';
    extension[4] = 0;
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            header->epf = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }
    extension[1] = 'x';
    extension[2] = 'h';
    extension[3] = 'r';
    extension[4] = 0;
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            header->xhr = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }
    /* ".txy": the NUL written for ".xhr" stays */
    extension[1] = 't';
    extension[2] = 'x';
    extension[3] = 'y';
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            header->txy = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }
    /* ".pxy": read with the caller's tag and slot, no size check */
    extension[1] = 'p';
    size = xglCdGetFileSize(name);
    if (slot != 0) {
        header->pxy = buffer;
        xglCdReadFileSlot(name, buffer, tag, slot);
    }
    buffer += (size + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    return buffer - start;
}

INCLUDE_ASM("asm/main/nonmatchings/tu054", func_001E00F0);

/* Reads file_id as ".xap" (or ".fap" when no ".xap" exists) into slot's buffer; returns the
 * bytes it takes rounded up to CD_SECTOR_SIZE, 0 when the file is missing. */
int xglCdLoadXapFile(unsigned int file_id, unsigned int slot, int tag)
{
    char name[256];
    char *extension;
    char *buffer;
    char *start;
    int size;

    buffer = (char *) xglCdSlotGetBuffer(slot);
    start = buffer;
    extension = xglCdBuildFileName(name, file_id);
    extension[0] = '.';
    extension[1] = 'x';
    extension[2] = 'a';
    extension[3] = 'p';
    extension[4] = 0;
    size = xglCdGetFileSize(name);
    if (size < 0) {
        extension[1] = 'f';
        size = xglCdGetFileSize(name);
    }
    if (size > 0) {
        if (slot != 0) {
            xglCdReadFileSlot(name, buffer, tag, slot);
        }
        buffer += (size + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    }
    return buffer - start;
}

/* Same for ".ene" files, with no fallback extension. */
int xglCdLoadEneFile(unsigned int file_id, unsigned int slot, int tag)
{
    char name[256];
    char *extension;
    char *buffer;
    char *start;
    int size;

    buffer = (char *) xglCdSlotGetBuffer(slot);
    start = buffer;
    extension = xglCdBuildFileName(name, file_id);
    extension[0] = '.';
    extension[1] = 'e';
    extension[2] = 'n';
    extension[3] = 'e';
    extension[4] = 0;
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            xglCdReadFileSlot(name, buffer, tag, slot);
        }
        buffer += (size + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    }
    return buffer - start;
}

int func_001E0610(int id, unsigned int slot, int *size, int tag);

INCLUDE_ASM("asm/main/nonmatchings/tu054", func_001E0610);

/* Loads the file name into a block whose GAME_RES_NAME_HEADER_SIZE header holds the name, unless a block
 * already holds it. Reports the data (past the header) and its size; returns the owner tag,
 * -1 when the file is missing or no block is free. */
int GameResLoadName(char *name, void **data, unsigned int *size, GameResCallback callback)
{
    int index;
    int bytes;
    char *buffer;
    char *source;
    char *header;

    index = GameResBlockFindByName(name);
    if (index > 0) {
        buffer = xglCdSlotTable[index].buffer;
        buffer += GAME_RES_NAME_HEADER_SIZE;
        if (data != 0) {
            *data = buffer;
        }
        if (size != 0) {
            *size = xglCdSlotTable[index].size;
        }
        if (callback != 0) {
            callback(index, xglCdSlotTable[index].buffer, xglCdSlotTable[index].size, xglCdSlotValue);
        }
        return xglCdSlotTable[index].owner;
    }
    bytes = xglCdGetFileSize(name);
    if (bytes < 0) {
        return -1;
    }
    bytes = (bytes + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    bytes += GAME_RES_NAME_HEADER_SIZE;
    index = GameResBlockAlloc(0, bytes);
    if (index == 0) {
        return -1;
    }
    buffer = xglCdSlotTable[index].buffer;
    source = func_001DF170(name);
    header = buffer;
    while ((*header++ = *source++) != 0) {
    }
    xglCdSlotTable[index].tag = (int) callback;
    xglCdSlotTable[index].value = xglCdSlotValue;
    buffer += GAME_RES_NAME_HEADER_SIZE;
    if (data != 0) {
        *data = buffer;
    }
    if (size != 0) {
        *size = bytes;
    }
    xglCdSetReadTag(index);
    xglCdReadFile(name, buffer, 1, (int) xglCdSlotEvent);
    return xglCdSlotTable[index].owner;
}

/* Returns the owner tag of the block holding resource id, loading it first when no block
 * holds it (-1: no such file, -2: no free block). Reports buffer and size, and calls
 * callback with the block when it was already loaded. */
int GameResLoadID(int id, void **buffer, unsigned int *size, GameResCallback callback)
{
    unsigned int file_id;
    int index;
    int total;

    file_id = id & GAME_RES_ID_MASK;
    index = GameResBlockFindByOwner(file_id);
    if (index != 0) {
        if (buffer != 0) {
            *buffer = xglCdSlotTable[index].buffer;
        }
        if (size != 0) {
            *size = xglCdSlotTable[index].size;
        }
        if (callback != 0) {
            callback(index, xglCdSlotTable[index].buffer, xglCdSlotTable[index].size, xglCdSlotValue);
        }
        return xglCdSlotTable[index].owner;
    }
    total = func_001E0610(id, 0, 0, 0);
    if (total == 0) {
        return -1;
    }
    index = GameResBlockAlloc(file_id, total);
    if (index == 0) {
        return -2;
    }
    func_001E0610(id, index, size, (int) callback);
    if (buffer != 0) {
        *buffer = xglCdSlotTable[index].buffer;
    }
    return xglCdSlotTable[index].owner;
}

/* The data (past the GAME_RES_NAME_HEADER_SIZE header) of the block loaded by name, and its size; 0 when absent. */
void *GameResFindName(char *name, unsigned int *size)
{
    int index;
    char *data;

    index = GameResBlockFindByName(name);
    if (index > 0) {
        data = xglCdSlotTable[index].buffer;
        data += GAME_RES_NAME_HEADER_SIZE;
        if (size != 0) {
            *size = xglCdSlotTable[index].size;
        }
        return data;
    }
    return 0;
}
