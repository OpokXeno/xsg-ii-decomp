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
/* Address range gameResArenaStart .. gameResArenaEnd whose blocks GameResBlockFindByOwner and GameResBlockFindByName skip. */
extern unsigned int gameResArenaStart;
extern unsigned int gameResArenaEnd;

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

/* The first block owned by owner whose buffer lies outside gameResArenaStart .. gameResArenaEnd. */
int GameResBlockFindByOwner(int owner)
{
    int index;

    if (owner == 0) {
        return 0;
    }
    for (index = 1; index < GAME_RES_BLOCK_COUNT && xglCdSlotTable[index].buffer != 0; index++) {
        if (((unsigned int) xglCdSlotTable[index].buffer < gameResArenaStart
             || gameResArenaEnd < (unsigned int) xglCdSlotTable[index].buffer)
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

/* The block outside gameResArenaStart .. gameResArenaEnd whose owner is a GAME_RES_OWNER tag and whose
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
        if (((unsigned int) xglCdSlotTable[index].buffer < gameResArenaStart
             || gameResArenaEnd < (unsigned int) xglCdSlotTable[index].buffer)
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

/* Frees block index: it is marked free and absorbs the free block after it, then the free block
 * before it (index 2 and up). The entries past the merged block move down to close the gap. */
void GameResBlockFree(int index)
{
    CdSlot *block;
    int dest;
    int srcIndex;

    block = &xglCdSlotTable[index];
    block->owner = 0;
    dest = -1;
    srcIndex = -1;
    if (block[1].owner == 0) {
        block->size += block[1].size;
        dest = index + 1;
        srcIndex = index + 2;
    }
    if (index >= 2 && block[-1].owner == 0) {
        block[-1].size += block->size;
        dest = index;
        if (srcIndex == -1) {
            srcIndex = index + 1;
        }
    }
    if (srcIndex > 0) {
        if (srcIndex < GAME_RES_BLOCK_COUNT) {
            do {
                xglCdSlotTable[dest].buffer = xglCdSlotTable[srcIndex].buffer;
                xglCdSlotTable[dest].size = xglCdSlotTable[srcIndex].size;
                xglCdSlotTable[dest].tag = xglCdSlotTable[srcIndex].tag;
                xglCdSlotTable[dest].owner = xglCdSlotTable[srcIndex].owner;
                xglCdSlotTable[dest].value = xglCdSlotTable[srcIndex].value;
                srcIndex++;
                dest++;
            } while (srcIndex < GAME_RES_BLOCK_COUNT);
        }
    }
}

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
/* Frees block index: it is marked free and absorbs the free block after it, then the free block * before it (index 2 and up). The entries past the merged block move down to close the gap. */ void GameResBlockFree(int index);

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

/* Allocates a block of size bytes for no owner, clears its tag and value words and returns
 * its buffer, 0 when none fits. */
void *GameResBufferAlloc(int size)
{
    int index;

    index = GameResBlockAlloc(0, size);
    if (index != 0) {
        xglCdSlotTable[index].tag = 0;
        xglCdSlotTable[index].value = 0;
        return xglCdSlotTable[index].buffer;
    }
    return 0;
}

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

/* Allocates a block of size bytes for owner, clears its tag and value words and returns
 * its buffer, 0 when none fits. */
void *GameResBufferAllocOwned(int owner, int size)
{
    int index;

    index = GameResBlockAlloc(owner, size);
    if (index != 0) {
        xglCdSlotTable[index].tag = 0;
        xglCdSlotTable[index].value = 0;
        return xglCdSlotTable[index].buffer;
    }
    return 0;
}

/* Called by GameResLoadID and GameResLoadName for a block that was already loaded:
 * block index, buffer, size and xglCdSlotValue. Stored as a block's read tag otherwise. */
typedef void (*GameResCallback)(int index, void *buffer, unsigned int size, int value);

extern int xglCdSlotValue;

/* Sets the value that xglCdReadFileTagged and xglCdReadFileSlot copy into the slot (xglCdSlotValue). */
void xglCdSetSlotValue(int value)
{
    xglCdSlotValue = value;
}


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

extern u8 xglCdLoadSync;

/* Starts a read into a slot (proposed xglCdReadFileSlot). tag 0 reads without a slot;
 * otherwise the slot (index 0 when slot >= 0x80) takes tag and xglCdSlotValue before xglCdReadFile. */
void xglCdReadFileSlot(const char *name, void *buffer, int tag, u32 slot) {
    CdSlot *entry;
    u32 index;
    int in_table;
    int value;

    in_table = slot < 0x80U;
    if (tag == 0) {
        if (xglCdLoadSync != 0) {
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
    if (xglCdLoadSync != 0) {
        xglCdReadFile(name, buffer, 1, xglCdSlotEvent);
        return;
    }
    xglCdReadFile(name, buffer, 0, xglCdSlotEvent);
}

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

/* The words of xglCdXapGroups are groups: a name, its file ids, then -1; a lone -1 closes the table. */
extern int xglCdXapGroups[];

/* Returns the name of the group that lists file_id, or 0 when no group does. */
char *xglCdFindXapName(int file_id)
{
    int *word;
    int groupName;
    int listedId;

    word = xglCdXapGroups;
    while (*word != -1) {
        groupName = *word;
        word++;
        listedId = *word++;
        while (listedId != -1) {
            if (listedId == file_id) {
                return (char *) groupName;
            }
            listedId = *word++;
        }
    }
    return 0;
}

/* File type of a file id (bits 16-23), as the loader dispatcher xglCdLoadByKind selects on it. */
#define CD_FILE_TYPE_MASK 0xFF0000
#define CD_FILE_TYPE(type) ((type) << 16)
/* File kinds: bits 16-23 of a file id, and the kinds of the entries resScriptRun collects in
 * resEntryTable (RES_KIND_SOUND .. RES_KIND_EVENT are entry kinds only, no file id has them). */
enum ResKind {
    RES_KIND_ACT = 0x10000,
    RES_KIND_MAP = 0x20000,
    RES_KIND_MOT = 0x30000,
    RES_KIND_EFFECT = 0x40000,
    RES_KIND_MOBJ = 0x70000,
    RES_KIND_ENE = 0x80000,
    RES_KIND_SOUND = 0x90000,
    RES_KIND_EVENT = 0xF10000
};
/* Event numbers of a RES_KIND_EVENT entry (bits 8-15): ";save" and ";disk". */
enum ResEvent {
    RES_EVENT_SAVE = 0x100,
    RES_EVENT_DISK = 0x200
};
/* Sound-bank numbers the script records name (seBankLoad kinds). */
#define SE_BANK_FOOT 7
#define SE_BANK_FOOT_MATERIAL 8
#define SE_BANK_ENEMY_FOOT 12
/* Footstep materials a scene lists (resFootMaterials holds at most this many). */
#define RES_FOOT_MATERIAL_MAX 9
/* Voice records of resRecordTable. */
#define SE_RECORD_COUNT 24
/* In a record's flags word: bit 31 = started with volume and pan, bits 0-30 = voice id. */
#define SE_RECORD_HAS_PARAMS 0x80000000
#define SE_VOICE_ID_MASK 0x7FFFFFFF
/* High bit of a resource id: load now, without waiting for the CD service (xglCdLoadSync). */
#define RES_ID_SYNC_LOAD 0x80000000
/* Owner flag of a block that resPreloadStep loaded for the next scene; resApplyCfFile compacts them. */
#define RES_OWNER_PRELOADED 0x40000000
/* Each path slot is one block of this size followed by a RES_PATH_PAD_SIZE pad block. */
#define RES_PATH_BUFFER_SIZE 0xC5FC0
#define RES_PATH_PAD_SIZE 0x40
/* Read-completion event of xglCdReadFile; "#scn" is the script header magic. */
#define CD_EVENT_READ_DONE 4
#define RES_SCRIPT_MAGIC 0x6E637323
/* Jump records (resJumpRecords): kind of the end marker and of a record named by resJumpRecordSet. */
#define RES_JUMP_END 0xFF
#define RES_JUMP_NAMED 0x7F
/* Map block key class of a jump-point polygon; its low byte is the jump kind. */
#define RES_MAP_KEY_JUMP 0x100
/* cf file state kind: set while the file is being read. */
#define RES_CF_LOADING 0xFF
/* SB bytecode (OV01 VM) opcodes the jump scan reads, and the VM stack base in the scratchpad. */
#define SB_OP_END 0x1C
#define SB_OP_JUMP 0x0E
#define SB_OP_LOOKUPSWITCH 0x25
#define SB_OP_CALL_INDIRECT 0x15
#define SB_STACK_BASE 0x70000000

/* Appends the string src at dst and leaves dst on the copied NUL, so the next append continues
 * the string. Its do/while(0) block boundary keeps the dst-- ahead of the next call's argument
 * setup, as in the original. */
#define STRING_APPEND(dst, src) do { while ((*(dst)++ = *(src)++) != 0) {} (dst)--; } while (0)

/* Header of a file-set block (xglCdLoadFileSet): the data address of each file of the set,
 * 0 for a file that is missing. It takes the first 0x14 of GAME_RES_NAME_HEADER_SIZE bytes. */
typedef struct CdFileSetHeader {
    char *pxy;
    char *txy;
    char *xhr;
    char *epf;
    char *xap;
    int xapBytes;
} CdFileSetHeader;

/* Returns the name of the group that lists file_id, or 0 when no group does. */ char *xglCdFindXapName(int file_id);
/* Default .xap name "man.xap" for ids whose own and fallback names are all missing
 * (tu054's own .sdata, still asm-owned data). */
extern char D_0069CEE0[8];

/* Loads the file set of file_id into slot's block: a CdFileSetHeader, then the .xap file and
 * the name's .epf, .xhr, .txy and .pxy files, each rounded up to GAME_RES_BLOCK_ALIGN and the
 * last to CD_SECTOR_SIZE. The .xap name is the group's override (xglCdFindXapName) or the file's
 * own name; when that file is missing, its trailing digits, then an 'H' before them, are
 * replaced by "00", and as the last resort (not for type 7) the default name "man.xap" is
 * appended to the group directory.
 * Returns the bytes the set takes. */
int xglCdLoadFileSet(unsigned int file_id, unsigned int slot, int tag)
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
    STRING_APPEND(end, source);
    source = xglCdFindXapName(file_id);
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

void *memset(void *dest, int value, unsigned int count);
unsigned short xglGetLanguageCode(char *buffer);
extern u32 gameWork[];
extern char *D_002B5DBC[];

/* Loads the .bin, language-specific .sbl, .id, .txy and .pxy files of file_id into slot's
 * block (with slot 0 it only measures them) and returns the bytes the set takes. */
int xglCdLoadBinFileSet(unsigned int file_id, unsigned int slot, int tag)
{
    char name[256];
    char *start;
    char *buffer;
    char *extension;
    char *dirEnd;
    char *sblPath;
    char *source;
    char *dest;
    char *binData;
    char *sblBuffer;
    CdFileSetHeader *header;
    CdFileSetHeader *info;
    int size;
    int sblSize;
    unsigned int binBytes;

    start = buffer = (char *) xglCdSlotGetBuffer(slot);
    extension = xglCdBuildFileName(name, file_id);
    header = (CdFileSetHeader *) buffer;
    info = (CdFileSetHeader *) buffer;
    buffer += GAME_RES_NAME_HEADER_SIZE;
    extension[0] = '.';
    if (slot != 0) {
        memset(header, 0, GAME_RES_NAME_HEADER_SIZE);
    }

    extension[4] = 0;
    extension[1] = 'b';
    extension[2] = 'i';
    extension[3] = 'n';
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0 && D_002B5DBC[0] != 0) {
            if (xglCdLoadSync != 0) {
                binData = buffer;
            } else {
                binData = D_002B5DBC[0];
            }
            header->xap = binData;
            xglCdReadFileSlot(name, binData, 0, 0);
        }
        binBytes = (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
        buffer += binBytes;
        sblBuffer = 0;
        if (slot != 0) {
            if (info->xap != 0) {
                sblBuffer = info->xap + binBytes;
            }
            info->xapBytes = binBytes;
        }

        sblPath = extension + 5;
        source = name;
        dest = sblPath;
        dirEnd = extension + 3;
        while (*dirEnd != '\\') {
            dirEnd--;
        }
        while (dirEnd >= source) {
            *dest++ = *source++;
        }
        xglGetLanguageCode(dest);
        dest[2] = '\\';
        dest = dest + 3;
        while (*source != '.') {
            *dest++ = *source++;
        }
        dest[0] = '.';
        dest[1] = 's';
        dest[2] = 'b';
        dest[3] = 'l';
        dest[4] = 0;
        sblSize = xglCdGetFileSize(sblPath);
        if (sblSize > 0) {
            if (slot != 0 && sblBuffer != 0) {
                sblBuffer = buffer;
                if (xglCdLoadSync == 0) {
                    char *sblTop;

                    sblTop = (char *) (gameWork[11] + info->xapBytes);
                    gameWork[46] = (u32) sblTop;
                    sblBuffer = sblTop;
                }
                xglCdReadFileSlot(sblPath, sblBuffer, 0, 0);
            }
            buffer += (sblSize + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
        }
    }

    extension[1] = 'i';
    extension[2] = 'd';
    extension[3] = 0;
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            info->epf = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }

    extension[1] = 't';
    extension[2] = 'x';
    extension[3] = 'y';
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            info->txy = buffer;
            xglCdReadFileSlot(name, buffer, 0, 0);
        }
        buffer += (size + (GAME_RES_BLOCK_ALIGN - 1)) & ~(GAME_RES_BLOCK_ALIGN - 1);
    }

    extension[1] = 'p';
    size = xglCdGetFileSize(name);
    if (size > 0) {
        if (slot != 0) {
            info->pxy = buffer;
            xglCdReadFileSlot(name, buffer, tag, slot);
        }
        buffer += (size + (CD_SECTOR_SIZE - 1)) & ~(CD_SECTOR_SIZE - 1);
    }
    return buffer - start;
}

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

int xglCdLoadByKind(int id, unsigned int slot, int *size, int tag);
/* Loads the .bin, language-specific .sbl, .id, .txy and .pxy files of file_id into slot's * block (with slot 0 it only measures them) and returns the bytes the set takes. */ int xglCdLoadBinFileSet(unsigned int file_id, unsigned int slot, int tag);

int xglCdLoadByKind(int resourceId, unsigned int slot, int *outSize, int tag)
{
    unsigned int fileId;
    unsigned int type;
    int bytes;

    fileId = resourceId & GAME_RES_ID_MASK;
    if (resourceId < 0) {
        xglCdLoadSync = 1;
    } else {
        xglCdLoadSync = 0;
    }
    bytes = 0;
    if (xglCdLoadSync == 1 || xglCdServiceIfPending() == 0) {
        if (xglCdFindFileGroup(fileId) != 0) {
            type = fileId & CD_FILE_TYPE_MASK;
            switch (type) {
            case RES_KIND_ACT:
            case RES_KIND_MOBJ:
                bytes = xglCdLoadFileSet(fileId, slot, tag);
                break;
            case RES_KIND_MAP:
                bytes = xglCdLoadBinFileSet(fileId, slot, tag);
                break;
            case RES_KIND_MOT:
                bytes = xglCdLoadXapFile(fileId, slot, tag);
                break;
            case RES_KIND_EFFECT:
                break;
            case RES_KIND_ENE:
                bytes = xglCdLoadEneFile(fileId, slot, tag);
                break;
            }
        }
    }
    if (outSize != 0) {
        *outSize = bytes;
    }
    return bytes;
}

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
    total = xglCdLoadByKind(id, 0, 0, 0);
    if (total == 0) {
        return -1;
    }
    index = GameResBlockAlloc(file_id, total);
    if (index == 0) {
        return -2;
    }
    xglCdLoadByKind(id, index, size, (int) callback);
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

/* ---- res_get_path part (old tu055 from 0x1E0B10; merged 2026-11-09) ---- */

#include "common.h"
#include "shared.h"
#include "main/res_get_path.h"
#include "main/tu021.h"
#include "main/tu022.h"

int GameResBlockFindByOwner(int owner);

/* Entry 0 of xglCdSlotTable (CdSlot above) is the header: resInitPathHeader stores the
   managed range start in its buffer and length in its size, and its owner holds the matched index. */


int GameResFindID(int resourceId, int *outSize)
{
    int block;

    block = GameResBlockFindByOwner(resourceId & GAME_RES_ID_MASK);
    if (block == 0) {
        return 0;
    }
    if (outSize != 0) {
        *outSize = xglCdSlotTable[block].size;
    }
    return xglCdSlotTable[block].buffer;
}

int GameResBlockFindLargestFree(void);
extern int resSelectedSlot[];

/* Stores the path slot index in resSelectedSlot[0]: the slot found by the search (GameResBlockFindLargestFree) when search is
   nonzero, otherwise slot 1. Callers pass 1 to search and 0 for the default (proposed resSelectSlot). */
void resSelectSlot(int search) {
    if (search != 0) {
        resSelectedSlot[0] = GameResBlockFindLargestFree();
    } else {
        resSelectedSlot[0] = 1;
    }
}

extern int seBankNames[32];
extern int D_0069CEF0;
extern char *seBankNameCursor;
extern int seBankNamePool[];
extern signed char resFootMaterials[9];
extern int resEnemyFootGroups[16];

void resResetDescriptorTables(void) {
    int i;

    for (i = 0; i < 32; i++) {
        seBankNames[i] = 0;
    }
    seBankNames[2] = D_0069CEF0;
    seBankNameCursor = (char *)seBankNamePool;
    for (i = 0; i < RES_FOOT_MATERIAL_MAX; i++) {
        resFootMaterials[i] = -1;
    }
    for (i = 0; i < 16; i++) {
        resEnemyFootGroups[i] = -1;
    }
}

/* One 16-byte record of resRecordTable; tag is word 0, whose top byte is the record type that resReleaseRecordsOfType compares. */
typedef struct {
    u32 kindWord;
    u8 unmodeled_04[0xC];
} ResKindRecord;

/* One 16-byte entry of resRecordTable (0x6A12E0): a voice's start words and its key/flags word. */
/* In a record whose flags have SE_RECORD_HAS_PARAMS, volume and pan are what seVoiceStart and
 * seVoiceSetPanVolume take (seVoiceRecordsUpdatePan passes best >> 8 and best & 0x7F); without it the first
 * three words are the emitter position (seVoiceRecordAddAt). */
typedef struct ResRecord {
    union {
        struct {
            u32 volume;
            u32 pan;
            u32 unmodeled_08;
        } params;
        f32 position[3];
    } u;
    u32 flags;
} ResRecord;

extern ResRecord resRecordTable[];
extern unsigned char D_006A10E0[];
extern unsigned char D_006A10F0[];
extern signed char seBgmName[];

/* Clears the type word of all 24 records and the three flags at seBgmName, D_006A10E0 and D_006A10F0 (proposed resClearRecordTable body). */
void resClearRecordTable(void)
{
    int i;

    seBgmName[0] = 0;
    D_006A10E0[0] = 0;
    D_006A10F0[0] = 0;
    for (i = SE_RECORD_COUNT - 1; i >= 0; i--) {
        resRecordTable[i].flags = 0;
    }
}

/* 16-byte voice entries of resRecordTable (0x6A12E0); the flags word is tested and its top bit selects the start path. */
typedef struct SeVoiceEntry {
    int startA;
    int startB;
    u8 unmodeled_08[4];
    unsigned int flags;
} SeVoiceEntry;

extern int D_002BB5B8[];
void seFootBanksLoad(void);

void resResetKindState(int mode) {
    int i;
    int j;
    u32 flags;
    int voice;

    seBankLoadDefault(0, 0);
    for (i = 2; i < SE_RECORD_COUNT; i++) {
        seBankLoadDefault(i, 0);
    }
    for (i = 0; i < 32; i++) {
        if (seBankNames[i] != 0) {
            seBankLoad(i, seBankNames[i], D_002BB5B8[i]);
        }
    }
    seFootBanksLoad();
    if (mode < 0) {
        seBgmName[0] = 0;
        return;
    }
    if (seBgmName[0] != 0) {
        func_001A37E0(mode + 1, seBgmName, 0x19000);
    }
    for (i = 0; i < SE_RECORD_COUNT; i++) {
        flags = resRecordTable[i].flags;
        if (flags != 0) {
            voice = flags & SE_VOICE_ID_MASK;
            if (flags & SE_RECORD_HAS_PARAMS) {
                seVoiceStart(voice, resRecordTable[i].u.params.volume, resRecordTable[i].u.params.pan);
            } else {
                for (j = 0; j < i; j++) {
                    if (resRecordTable[i].flags == resRecordTable[j].flags) {
                        j = -1;
                        break;
                    }
                }
                if (j != -1) {
                    seVoiceStart(voice, 0, 64);
                }
            }
        }
    }
}

void resResetKindState(int mode);

void resResetActState(void) {
    resResetKindState(0x10000);
}

/* Clears the type word of all 24 records and the three flags at seBgmName, D_006A10E0 and D_006A10F0 (proposed resClearRecordTable body). */ void resClearRecordTable(void);

void resResetAllKinds(void) {
    resResetKindState(-1);
    resClearRecordTable();
}

void func_001E0E98(void) {
    resResetKindState(0);
}

typedef struct {
    unsigned char b;
} ResByte;

int seNameCopyIfChanged(char *dst, char *src)
{
    signed char srcChar;
    ResByte srcByte;
    int changed;

    srcChar = *src;
    srcByte.b = *src;
    changed = 1;
    if (srcChar != *dst) {
        *dst = srcByte.b;
        if (srcChar != 0) {
            do {
                src++;
                dst++;
                *dst = *src;
            } while (((unsigned int)*dst << 24) != 0);
        }
    } else {
        if (((unsigned int)srcByte.b << 24) == 0) {
            return 0;
        }
        src++;
        dst++;
        return seNameCopyIfChanged(dst, src);
    }
    return changed;
}

#include "main/tu051.h"


void resReleaseRecordsOfType(int kind, int forward) {
    int i;
    u8 *base;
    ResKindRecord *record;

    if (forward == 0) {
        base = (u8 *)resRecordTable;
        record = (ResKindRecord *)(base + 12);
        for (i = 0; i < SE_RECORD_COUNT; i++, record++) {
            if (((record->kindWord >> 24) & 0x7f) == kind) {
                seVoiceStop(record->kindWord);
                record->kindWord = 0;
            }
        }
    } else {
        base = (u8 *)resRecordTable;
        record = (ResKindRecord *)(base + 12);
        for (i = SE_RECORD_COUNT - 1; i >= 0; i--, record++) {
            if ((record->kindWord >> 24) == kind) {
                seVoiceStop(record->kindWord);
                record->kindWord = 0;
            }
        }
    }
    soundInitIfNeeded();
}

extern char *D_006A116C[];
extern char *D_006A1170[];
void resReleaseRecordsOfType(int kind, int forward);
int seNameCopyIfChanged(char *dst, char *src);

int seBankSetName(int kind, char *text)
{
    char *cursor;
    char *name;
    char *source;
    u32 textAddress;
    int bankSize;

    textAddress = (u32)text + (((u32)text & 0xFF) >> 8);
    name = text;
    if (name != 0) {
        switch (kind) {
        case 3:
            cursor = D_006A10E0;
            D_006A116C[0] = cursor;
            resReleaseRecordsOfType(kind, 1);
            if (seNameCopyIfChanged(cursor, name) == 0) {
                return 0;
            }
            resReleaseRecordsOfType(kind, 0);
            break;
        case 4:
            cursor = D_006A10F0;
            D_006A1170[0] = cursor;
            resReleaseRecordsOfType(kind, 1);
            if (seNameCopyIfChanged(cursor, name) == 0) {
                return 0;
            }
            resReleaseRecordsOfType(kind, 0);
            break;
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            break;
        default:
            cursor = seBankNameCursor;
            seBankNames[kind] = (u32)cursor;
            source = (char *)textAddress;
            while ((*cursor++ = *source++) != 0) {
            }
            seBankNameCursor = cursor;
            break;
        }
    } else {
        switch (kind) {
        case 3:
            D_006A10E0[0] = 0;
            resReleaseRecordsOfType(kind, (int)textAddress);
            /* fall through */
        case 4:
            D_006A10F0[0] = 0;
            resReleaseRecordsOfType(kind, 0);
            break;
        }
    }
    if (kind == 3) {
        seBankLoad(4, 0, 0);
        bankSize = seBankLoad(3, name, D_002BB5B8[3]);
        D_002BB5B8[4] = 0xBE000 + ((bankSize + 255) & -256);
        return bankSize;
    }
    return seBankLoad(kind, name, D_002BB5B8[kind]);
}

void func_0013A618(int, int);

void soundRestart(void) {
    soundSlotsRestart();
    func_0013A618(1, 1);
    func_001A4010();
}

void soundReset(void) {
    soundRestart();
    seSetMuted(1, 0);
    func_001A3A18();
}

extern unsigned char D_0069CFD0;
extern signed char seBgmVolume;

void seBgmNameSet(signed char *name)
{
    if (*name == 0) {
        func_001A37E0(1, 0, 0x19000);
        func_001A3A18();
        seBgmName[0] = 0;
        return;
    }
    if (seNameCopyIfChanged(seBgmName, name) != 0) {
        seVolumeSet(seBgmVolume, 0);
        func_001A37E0(0x20001, seBgmName, 0x19000);
    } else {
        seVolumeSet(seBgmVolume, 16);
    }
}

void seVoiceRecordSet(u32 voice, u32 volume, u32 pan) {
    int freeSlot = -1;
    int i;
    for (i = 0; i < SE_RECORD_COUNT; i++) {
        if (resRecordTable[i].flags == 0 && freeSlot == -1) {
            freeSlot = i;
        }
        if ((resRecordTable[i].flags & SE_VOICE_ID_MASK) == voice) {
            if (volume != 0) {
                resRecordTable[i].flags = voice | SE_RECORD_HAS_PARAMS;
                resRecordTable[i].u.params.volume = volume;
                resRecordTable[i].u.params.pan = pan;
                seVoiceSetPanVolume(voice, volume, pan);
                return;
            }
            /* With volume 0 (stop), pan is read as a record index. */
            if (!(resRecordTable[i].flags & SE_RECORD_HAS_PARAMS) && (resRecordTable[pan].flags & SE_VOICE_ID_MASK) == voice) {
                resRecordTable[pan].flags = 0;
                for (i = 0; i < SE_RECORD_COUNT; i++) {
                    if ((resRecordTable[i].flags & SE_VOICE_ID_MASK) == voice) {
                        return;
                    }
                }
                seVoiceStop(voice);
                return;
            }
            resRecordTable[i].flags = 0;
            seVoiceStop(voice);
            return;
        }
    }
    if (volume != 0) {
        resRecordTable[freeSlot].flags = voice | SE_RECORD_HAS_PARAMS;
        resRecordTable[freeSlot].u.params.volume = volume;
        resRecordTable[freeSlot].u.params.pan = pan;
        seVoiceStart(voice, volume, pan);
    }
}

extern u32 gameResArenaStart;
extern u32 gameResArenaEnd;
extern u32 D_0069E1AC;
extern u32 D_0069E1B0;

typedef union {
    u32 w;
} ResTagWord;

void seVoiceRecordAddAt(int voice, f32 *src)
{
    int i;
    int freeSlot = -1;
    int running = 0;

    for (i = 0; i < SE_RECORD_COUNT; i++) {
        u32 flagsWord = resRecordTable[i].flags;

        if (flagsWord == 0 && freeSlot == -1) {
            freeSlot = i;
        }
        if ((flagsWord & SE_VOICE_ID_MASK) == voice) {
            running = 1;
        }
    }
    {
        f32 *slotPos = resRecordTable[freeSlot].u.position;

        ((ResTagWord *)&resRecordTable[freeSlot].flags)->w = voice;
        slotPos[0] = src[0];
        ((f32 *)&resRecordTable[freeSlot].u.params.pan)[0] = src[1];
        ((f32 *)&resRecordTable[freeSlot].u.params.pan)[1] = src[2];
    }
    if (!running) {
        seVoiceStart((void *)voice, 0, 64);
    }
}

extern u32 D_002B5D98[];
u32 seVoicePanVolumeFromPosition(ResRecord *rec);

void seVoiceRecordsUpdatePan(void)
{
    int i;
    int j;
    u32 loudest;

    if (D_002B5D98[0] & 0x20) {
        return;
    }
    for (i = 0; i < SE_RECORD_COUNT; i++) {
        if (resRecordTable[i].flags == 0 || (resRecordTable[i].flags & SE_RECORD_HAS_PARAMS)) {
            continue;
        }
        for (j = 0; j < i; j++) {
            if (resRecordTable[i].flags == resRecordTable[j].flags) {
                j = -1;
                break;
            }
        }
        if (j == -1) {
            continue;
        }
        loudest = seVoicePanVolumeFromPosition(&resRecordTable[i]);
        for (j = i + 1; j < SE_RECORD_COUNT; j++) {
            if (resRecordTable[i].flags == resRecordTable[j].flags) {
                u32 candidate = seVoicePanVolumeFromPosition(&resRecordTable[j]);
                if (candidate > loudest) {
                    loudest = candidate;
                }
            }
        }
        seVoiceSetPanVolume(resRecordTable[i].flags, loudest >> 8, loudest & 0x7F);
    }
}

extern f32 seMaxDistance;

/* Sets the sound's maximum audible distance when maxDistance is not negative; returns the current value
   (proposed seSetMaxDistance). The distance check in seVoicePanVolumeFromPosition reads seMaxDistance. */
f32 seSetMaxDistance(f32 maxDistance) {
    if (maxDistance >= 0.0f) {
        seMaxDistance = maxDistance;
    }
    return seMaxDistance;
}

typedef struct {
    u8 unmodeled_00[16];
    f32 position[3];
} SeEmitter;

/* 0x870-byte entries of uwamonoSlotTable (the symbol is at 0x3400F0); the emitter pointer is the first word. */
typedef struct {
    SeEmitter *emitter;
    u8 unmodeled_04[0x870 - 0x04];
} SeListener;

extern int uwamonoCurrentIndex;
extern SeListener uwamonoSlotTable[];
extern f32 D_002B5F20[];
void xglVectorLength(f32 *destination, f32 *vector);
f32 xglAtan2(f32 x, f32 z);
int F2I(f32 value);

u32 seVoicePanVolumeFromPosition(ResRecord *rec)
{
    f32 *position = rec->u.position;
    SeListener *listener;
    SeEmitter *emitter;
    f32 *target;
    f32 offset[3];
    f32 distance;
    f32 angle;
    int volume;
    int pan;

    listener = &uwamonoSlotTable[uwamonoCurrentIndex];
    emitter = listener->emitter;
    if (emitter == 0) {
        return 0;
    }
    target = emitter->position;
    offset[0] = position[0] - target[0];
    offset[1] = position[1] - target[1];
    offset[2] = position[2] - target[2];
    xglVectorLength(&distance, offset);
    if (seMaxDistance < distance) {
        return 0;
    }
    volume = F2I((1.0f - distance / seMaxDistance) * 127.0f);
    angle = xglAtan2(offset[0], offset[2]) + D_002B5F20[0] + 1.5707963705062866f;
    while (6.2831854820251465f < angle) {
        angle -= 6.2831854820251465f;
    }
    while (angle < 0.0f) {
        angle += 6.2831854820251465f;
    }
    if (3.1415927410125732f < angle) {
        angle = 6.2831854820251465f - angle;
    }
    pan = F2I(angle / 3.1415927410125732f * 127.0f);
    return (volume << 8) + pan;
}

void *GameResArenaReserve(unsigned int bytesWanted)
{
    unsigned int first;
    unsigned int last;
    unsigned int total;

    first = GameResBlockFindLargestFree();
    last = first;
    total = xglCdSlotTable[first].size;
    if (total < bytesWanted) {
        while (total < bytesWanted) {
            if (xglCdSlotTable[last + 1].buffer == 0 && xglCdSlotTable[0].owner < first) {
                first--;
                total += xglCdSlotTable[first].size;
            } else {
                last++;
                total += xglCdSlotTable[last].size;
            }
        }
    }
    if (gameResArenaEnd == 0) {
        gameResArenaStart = xglCdSlotTable[first].buffer;
        gameResArenaEnd = xglCdSlotTable[last].buffer;
    } else {
        D_0069E1AC = xglCdSlotTable[first].buffer;
        D_0069E1B0 = xglCdSlotTable[last].buffer;
    }
    return xglCdSlotTable[first].buffer;
}

void GameResArenaReload(void)
{
    int i;
    CdSlot *entry;
    CdSlot *first;
    CdSlot *table;
    u32 blockStart;

    table = xglCdSlotTable;
    first = &table[1];
    i = 1;
    if (first->buffer != 0) {
        entry = first;
        do {
            if (entry->owner != 0) {
                blockStart = entry->buffer;
                if ((blockStart < D_0069E1AC || blockStart > D_0069E1B0) && blockStart >= gameResArenaStart && blockStart <= gameResArenaEnd) {
                    xglCdLoadByKind(entry->owner | RES_ID_SYNC_LOAD, blockStart, 0, 0);
                }
            }
            i++;
            entry++;
        } while (i < 0x80 && entry->buffer != 0);
    }
}

void GameResArenaRelease(void)
{
    int i;
    CdSlot *entry;
    CdSlot *first;
    CdSlot *table;
    u32 blockStart;
    u32 lower;
    u32 upper;

    if (D_0069E1B0 == 0) {
        lower = gameResArenaStart;
        upper = gameResArenaEnd;
    } else {
        lower = D_0069E1AC;
        upper = D_0069E1B0;
    }
    table = xglCdSlotTable;
    first = &table[1];
    i = 1;
    if (first->buffer != 0) {
        entry = first;
        do {
            if (entry->owner != 0) {
                blockStart = entry->buffer;
                if (blockStart >= lower && blockStart <= upper) {
                    xglCdLoadByKind(entry->owner, blockStart, 0, 0);
                }
            }
            i++;
            entry++;
        } while (i < 0x80 && entry->buffer != 0);
    }
    gameResArenaStart = 0;
    gameResArenaEnd = 0;
    D_0069E1AC = 0;
    D_0069E1B0 = 0;
}

void func_001E1B10(void) {
}

typedef struct PathSlot {
    int base;
    int unmodeled_04;
} PathSlot;

extern PathSlot resPathSlots[];

void resResetSlotTable(void) {
    int i = xglCdSlotTable[0].owner;

    if (i == 1) {
        xglCdSlotTable[1].buffer = xglCdSlotTable[0].buffer;
    }
    xglCdSlotTable[i].size = xglCdSlotTable[0].size - (xglCdSlotTable[i].buffer - xglCdSlotTable[0].buffer);
    xglCdSlotTable[i].owner = 0;
    for (i++; i < 128; i++) {
        xglCdSlotTable[i].buffer = 0;
        xglCdSlotTable[i].size = 0;
        xglCdSlotTable[i].owner = -1;
    }
    gameResArenaStart = 0;
    gameResArenaEnd = 0;
    D_0069E1AC = 0;
    D_0069E1B0 = 0;
}

void resSelectSlot(int search);
void resResetDescriptorTables(void);
void resResetSlotTable(void);

/* Stores key and value in the header entry, then resets the path state (proposed resInitPathHeader). */
void resInitPathHeader(int key, int value) {
    xglCdSlotTable[0].buffer = key;
    xglCdSlotTable[0].size = value;
    resSelectSlot(0);
    resResetSlotTable();
    resResetDescriptorTables();
    resClearRecordTable();
    D_0069CFD0 = 0;
}

u8 *next_line(u8 *cursor)
{
    for (;;) {
        while (*cursor != '\n' && *cursor != '\r')
            cursor++;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
            cursor++;
        if (*cursor != ';')
            return cursor;
    }
}

u8 *skip_space(u8 *cursor)
{
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    return cursor;
}

u8 *resParseFileGroup(u8 *text, int kind, int *out)
{
    CdFileGroup *group;
    char **names;
    unsigned int nameIndex;
    u8 *cursor;
    u8 *groupName;
    int textChar;
    int nameChar;

    for (group = xglCdFileGroups; group->base != -1; group++) {
        if (kind != 0 && (group->base & CD_FILE_TYPE_MASK) != kind) {
            continue;
        }
        names = group->names;
        for (nameIndex = 1, names++; nameIndex < group->count; nameIndex++, names++) {
            groupName = (u8 *)*names;
            cursor = text;
            for (;;) {
                for (;;) {
                    textChar = *cursor++;
                    if (textChar == ',' || textChar == ' ' || textChar == '\t' || textChar == '\n' || textChar == '\r' || textChar == 0) {
                        textChar = 0;
                        break;
                    }
                    if (textChar != '_') {
                        textChar |= 0x20;
                        break;
                    }
                }
                for (;;) {
                    nameChar = *groupName++;
                    if (nameChar == ' ' || nameChar == '\t' || nameChar == '\n' || nameChar == '\r' || nameChar == 0) {
                        nameChar = 0;
                        break;
                    }
                    if (nameChar != '_') {
                        nameChar |= 0x20;
                        break;
                    }
                }
                if ((textChar | nameChar) == 0) {
                    if (out != 0) {
                        *out = group->base + nameIndex;
                    }
                    return cursor;
                }
                if (textChar != nameChar) {
                    break;
                }
            }
        }
    }
    if (out != 0) {
        *out = -1;
    }
    return text;
}

extern u32 resEntryTable[];
extern char resNamePool[];
extern char *resNameCursor;

/*
 * Appends one word to resEntryTable; word 0 holds the next free index. Keys 0x40000, 0x90000 and 0xF00000
 * store key plus the offset of resNameCursor in resNamePool, then copy the NUL-terminated name at name
 * (when name is not null) to the cursor. Key 0xF10000 stores the integer name plus key, and other keys store key.
 */
void resEntryAdd(u32 key, u8 *name)
{
    switch (key) {
    case RES_KIND_EFFECT:
    case RES_KIND_SOUND:
    case GAME_RES_OWNER:
        {
            int index;

            index = resEntryTable[0];
            resEntryTable[index] = key + (resNameCursor - resNamePool);
            resEntryTable[0] = index + 1;
            if (name != 0) {
                while ((*resNameCursor++ = *name++) != 0) {
                }
            }
        }
        break;
    case RES_KIND_EVENT:
        {
            int index;

            index = resEntryTable[0];
            resEntryTable[index] = (int) name + key;
            resEntryTable[0] = index + 1;
        }
        break;
    default:
        {
            int index;

            index = resEntryTable[0];
            resEntryTable[index] = key;
            resEntryTable[0] = index + 1;
        }
        break;
    }
}

u8 *resParseFileGroup(u8 *text, int kind, int *out);
/* * Appends one word to resEntryTable; word 0 holds the next free index. Keys 0x40000, 0x90000 and 0xF00000 * store key plus the offset of resNameCursor in resNamePool, then copy the NUL-terminated name at name * (when name is not null) to the cursor. Key 0xF10000 stores the integer name plus key, and other keys store key. */ void resEntryAdd(u32 key, u8 *name);

/* Parses a name at cursor for kind and records the match; returns the cursor (proposed resCmdResolve). */
int resCmdResolve(int cursor, int kind) {
    int entryId;

    resParseFileGroup(cursor, kind, &entryId);
    resEntryAdd(entryId, 0);
    return cursor;
}

/* Script command handlers; each passes its kind to resCmdResolve (act: 0x10000, map: 0x20000, mot: 0x30000, mobj: 0x70000). */
void resCmdAct(int cursor) {
    resCmdResolve(cursor, 0x10000);
}

void resCmdMap(int cursor) {
    resCmdResolve(cursor, 0x20000);
}

void resCmdMot(int cursor) {
    resCmdResolve(cursor, 0x30000);
}

void resCmdMobj(int cursor) {
    resCmdResolve(cursor, 0x70000);
}

/* Copies the name at cursor into the name buffer up to whitespace and returns the cursor after it. */
u8 *resCmdEffect(u8 *cursor) {
    int c;

    resEntryAdd(RES_KIND_EFFECT, 0);
    c = *cursor;
    if ((u8)c == 0x20 || (u8)c == 0x9 || (u8)c == 0xA || (u8)c == 0xD) {
    } else {
        do {
            cursor++;
            *resNameCursor++ = c;
            c = *cursor;
            if ((u8)c == 0x20 || (u8)c == 0x9 || (u8)c == 0xA) {
                break;
            }
        } while ((u8)c != 0xD);
    }
    *resNameCursor++ = 0;
    return cursor;
}

u8 *resCmdMapse(u8 *script)
{
    resEntryAdd(RES_KIND_SOUND, 0);
    *resNameCursor++ = 3;
    while (*script != ',' && *script != ' ' && *script != '\t' && *script != '\n' && *script != '\r') {
        *resNameCursor++ = *script++;
    }
    *resNameCursor++ = 0;
    resEntryAdd(RES_KIND_SOUND, 0);
    *resNameCursor++ = 4;
    if (*script++ == ',') {
        while (*script != ',' && *script != ' ' && *script != '\t' && *script != '\n' && *script != '\r') {
            *resNameCursor++ = *script++;
        }
    }
    *resNameCursor++ = 0;
    return script;
}

/*
 * Script command handler for the runtime BGM name list: adds the 0x90000 entry, writes the prefix 's' to the
 * name pool, then copies the names at script, splitting at ',' (each name NUL-terminated), until a space,
 * tab, line feed or carriage return. Writes a final NUL and returns the pointer where the copy stopped.
 */
u8 *resCmdRuntimeBgm(u8 *script)
{
    int c;
    u8 ch;

    resEntryAdd(RES_KIND_SOUND, 0);
    *resNameCursor++ = 's';
    ch = *script;
    /* ch + ((ch & 0xFF00) >> 8) is ch; only combine folds the zero offset, so the byte copy survives cprop. */
    c = (int) ch + (((u32) ch & 0xFF00) >> 8);
    while ((u8) c != ' ' && (u8) c != '\t' && (u8) c != '\n' && (u8) c != '\r') {
        if (c == ',') {
            *resNameCursor++ = '\0';
            script++;
            ch = *script;
        }
        script++;
        *resNameCursor++ = ch;
        c = *script;
        /* the same zero-offset copy keeps the byte that the next pass stores alive */
        ch = (u8) (c + (((u32) c & 0xFF) >> 8));
    }
    *resNameCursor++ = '\0';
    return script;
}

u8 *resCmdBgmVol(u8 *cursor) {
    int volume = 0;
    u8 **cursorSlot;
    u8 *writePos;
    u8 *next;

    if ((u8)(*cursor - '0') < 10) {
        do {
            int times5 = volume * 5;
            volume = times5 << 1;
            volume = (volume + *cursor) - '0';
            cursor++;
        } while ((u8)(*cursor - '0') < 10);
    }
    resEntryAdd(RES_KIND_SOUND, 0);
    cursorSlot = &resNameCursor;
    writePos = *cursorSlot;
    *writePos = 'v';
    writePos++;
    *cursorSlot = writePos;
    next = writePos + 1;
    *writePos = (u8) volume;
    *cursorSlot = next;
    return cursor;
}

extern signed char D_006A1110[];
int footMaterialFind(u8 *name);

u8 *resCmdFoot(u8 *script) {
    u8 *materialList = D_006A1110;
    u8 *materialListStart = materialList;
    int material;

    for (;;) {
        material = footMaterialFind(script);
        script += 3;
        resEntryAdd(RES_KIND_SOUND, 0);
        resNameCursor[0] = 'f';
        resNameCursor[1] = materialList - materialListStart;
        resNameCursor[2] = material;
        *materialList++ = material;
        resNameCursor += 3;
        if (*script == '\n' || *script == '\r') {
            *materialList = 0xFF;
            return script;
        }
        for (;;) {
            if (*script == ',' || *script == ' ' || *script == '\t') {
                script++;
            } else {
                break;
            }
        }
    }
}

void *footMaterialKey(int index);

/* Parses the script's word list: one 'e' record per word, then one record per footstep material (proposed resCmdEnefoot). */
u8 *resCmdEnefoot(u8 *script)
{
    u8 footName[20] = "fs_0123456789abcdef";
    u8 *nameEnd;
    u8 *copyPos;
    u8 *materialKey;
    int groupId;
    u8 groupWord[4];
    int groupIndex;
    int recordCode = 12;
    int materialIndex;

    groupIndex = 0;
    for (;;) {
        nameEnd = footName + 3;
        for (;;) {
            if (*script == ',' || *script == ' ' || *script == '\t' || *script == '\n' || *script == '\r') {
                break;
            }
            *nameEnd++ = *script;
            script++;
        }
        *nameEnd = 0;
        resParseFileGroup(footName + 3, 0, &groupId);
        resEntryAdd(RES_KIND_SOUND, 0);
        resNameCursor[0] = 'e';
        resNameCursor[1] = groupIndex;
        groupIndex++;
        *(int *)groupWord = groupId;
        resNameCursor[2] = groupWord[0];
        resNameCursor[3] = groupWord[1];
        resNameCursor[4] = groupWord[2];
        resNameCursor[5] = groupWord[3];
        resNameCursor += 6;
        if (groupId == -1) {
            break;
        }
        *nameEnd++ = '_';
        for (materialIndex = 0; materialIndex < RES_FOOT_MATERIAL_MAX; materialIndex++) {
            if (D_006A1110[materialIndex] == -1) {
                break;
            }
            {
                resEntryAdd(RES_KIND_SOUND, 0);
                *resNameCursor++ = recordCode;
                recordCode++;
                for (copyPos = footName; copyPos < nameEnd; copyPos++) {
                    *resNameCursor++ = *copyPos;
                }
                materialKey = footMaterialKey(D_006A1110[materialIndex]);
                resNameCursor[0] = materialKey[0];
                resNameCursor[1] = materialKey[1];
                resNameCursor[2] = materialKey[2];
                resNameCursor[3] = 0;
                resNameCursor += 4;
            }
        }
        if (*script == '\n' || *script == '\r') {
            break;
        }
        for (;;) {
            if (*script == ',' || *script == ' ' || *script == '\t') {
                script++;
            } else {
                break;
            }
        }
    }
    return script;
}

/* Integer event name of the disk command: first byte of the command plus this base (key 0xF10000 stores the integer name). */
#define RES_DISK_EVENT_BASE 0x1D0

u8 *resCmdDisk(u8 *entry) {
    resEntryAdd(RES_KIND_EVENT, *entry + RES_DISK_EVENT_BASE);
    return entry;
}

u8 *resCmdSave(u8 *cursor) {
    int saveId = 0;

    if ((u8)(*cursor - '0') < 10) {
        do {
            int times5 = saveId * 5;
            saveId = times5 << 1;
            saveId = (saveId + *cursor) - '0';
            cursor++;
        } while ((u8)(*cursor - '0') < 10);
    }
    resEntryAdd(RES_KIND_EVENT, saveId + RES_EVENT_SAVE);
    return cursor;
}

void GameMovieCaptionPlay(void);

/*
 * Script command ";mpeg2" (name at 0x002BB670, handler at 0x002BB674 in the command table; proposed resCmdMpeg2).
 * It plays the caption and returns the script cursor unchanged, as the other resCmd handlers do.
 */
int resCmdMpeg2(int cursor) {
    GameMovieCaptionPlay();
    return cursor;
}

/* One entry of the script command table at 0x002BB610; a NULL name ends it. */
typedef struct {
    signed char *name;
    unsigned char *(*handler)(unsigned char *cursor);
} ResCommand;

extern void *resScriptBuffer[];           /* script buffer, "#scn" header */
extern ResCommand resCommandTable[];      /* script command table */
extern unsigned char D_006A0AC0[];   /* 0x006A0AC0: u32 list, [0] = count incl. header */
extern u8 resPreloadIndex;                /* gp-relative done flag */

void resScriptRun(int mode)
{
    ResCommand *command;
    u8 *cursor;
    u8 *commandText;
    signed char *keyword;
    signed char *textChar;
    unsigned char keywordChar;
    int found;
    u32 n, next;
    int i, j, m;


    if (mode != CD_EVENT_READ_DONE) {
        return;
    }
    if (*(u32 *)resScriptBuffer != RES_SCRIPT_MAGIC) {
        return;
    }
    cursor = next_line((u8 *)resScriptBuffer);
    while (1) {
        if (cursor[0] == '#') {
            if (cursor[1] == 'e' && cursor[2] == 'n' && cursor[3] == 'd') {
                break;
            }
            continue;
        }
        commandText = skip_space(cursor);
        found = 0;
        for (command = resCommandTable; command->name != 0; command++) {
            keyword = command->name;
            textChar = (signed char *)commandText;
            for (;;) {
                keywordChar = *keyword;
                if (*keyword == 0) {
                    if (*textChar == ' ' || *textChar == '\t' || *textChar == '\n' || *textChar == '\r') {
                        found = 1;
                        commandText = (u8 *)textChar;
                        break;
                    }
                }
                keyword++;
                if ((u8)*textChar != keywordChar) {
                    break;
                }
                textChar++;
            }
            if (found) {
                break;
            }
        }
        if (found) {
            commandText = command->handler(skip_space(commandText));
        }
        cursor = next_line(commandText);
    }

    /* Remove the duplicates of resEntryTable, keeping the first of each value. */
    for (n = 1; n < resEntryTable[0]; n = next) {
        next = n + 1;
        for (j = next; j < resEntryTable[0]; j++) {
            if (resEntryTable[n] == resEntryTable[j]) {
                for (j++; j < resEntryTable[0]; j++) {
                    resEntryTable[j - 1] = resEntryTable[j];
                }
                resEntryTable[0]--;
                next = n;
                break;
            }
        }
    }

    /* Collect the xglCdSlotTable indexes that resEntryTable does not hold. */
    n = 1;
    i = 1;
    next = xglCdSlotTable[i].owner;
    if (xglCdSlotTable[i].buffer != 0) {
        /* The original enters past a dead `m = 0` store; the jump into the
         * loop keeps loop.c from optimizing it (flow later drops the store). */
        goto scan;
        do {
            m = 0;
        scan:
            if (next != 0) {
                m = 1;
                if (m < resEntryTable[0]) {
                    do {
                        if (next == resEntryTable[m]) {
                            for (m++; m < resEntryTable[0]; m++) {
                                resEntryTable[m - 1] = resEntryTable[m];
                            }
                            m = 0;
                            resEntryTable[0]--;
                            break;
                        }
                    } while (++m < resEntryTable[0]);
                }
                if (i >= (u32)xglCdSlotTable[0].owner && m >= resEntryTable[0]) {
                    ((u32 *)D_006A0AC0)[n++] = xglCdSlotTable[i].owner;
                }
            }
        } while (++i < GAME_RES_BLOCK_COUNT && (next = xglCdSlotTable[i].owner, xglCdSlotTable[i].buffer != 0));
    }
    ((u32 *)D_006A0AC0)[0] = n;

    /* Sort resEntryTable ascending; each swap restarts the scan. */
    {
        int i, j, swapped;
        u32 tmp;

        do {
            swapped = 0;
            for (i = 1; i < resEntryTable[0]; i++) {
                for (j = i; ++j < resEntryTable[0];) {
                    if (resEntryTable[j] < resEntryTable[i]) {
                        tmp = resEntryTable[j];
                        resEntryTable[j] = resEntryTable[i];
                        resEntryTable[i] = tmp;
                        swapped = 1;
                        break;
                    }
                }
                if (swapped) {
                    break;
                }
            }
        } while (swapped);
    }
    resPreloadIndex = 1;
}

extern u8 resPreloadIndex;
int xglCdGetServicePending(void);

void resPreloadStep(int flag)
{
    void *block;
    int loadResult;

    if (flag == 0) {
        if (xglCdGetServicePending() != 0) {
            return;
        }
    }
    {
        u32 address = (u32)&resPreloadIndex;
        u8 *preloadIndex = (u8 *)(address + ((address & 0xFF) >> 8));
        if (GameResLoadID(((u32 *)resEntryTable)[*preloadIndex] + RES_ID_SYNC_LOAD, &block, 0, 0) > 0) {
            xglCdSlotTable[GameResBlockFindByBuffer(block)].owner |= RES_OWNER_PRELOADED;
        }
        *preloadIndex = *preloadIndex + 1;
        if (*preloadIndex >= ((u32 *)resEntryTable)[0]) {
            *preloadIndex = 0;
        }
    }
}

typedef struct ResCfFile {
    s16 id;
    u8 kind;
} ResCfFile;

extern ResCfFile resCfCurrent;
extern char *D_0069CF5C;
extern void *resScriptBuffer[];
void resPreloadStep(int flag);
void xglCdFormatFileName(char *destination, int id);
void resResetScriptState(void);
void resScriptRun(int mode);

void resLoadCfFile(int id) {
    char name[256];
    char *dst;
    char *src;

    if (resCfCurrent.id == (u16)id) {
        if (resCfCurrent.kind != 0 && resCfCurrent.kind != RES_CF_LOADING) {
            resPreloadStep(id & 0xFFFF0000);
        }
        return;
    }
    resResetScriptState();
    resCfCurrent.id = id;
    resEntryAdd(RES_KIND_SOUND, 0);
    *resNameCursor++ = 12;
    *resNameCursor++ = 0;
    resScriptBuffer[0] = 0;
    dst = name;
    src = D_0069CF5C;
    while (*src != 0) {
        *dst++ = *src++;
    }
    xglCdFormatFileName(dst, id);
    dst[6] = '.';
    dst[7] = 't';
    dst[8] = 'x';
    dst[9] = 't';
    dst[10] = 0;
    if (xglCdGetFileSize(name) > 0) {
        resCfCurrent.kind = RES_CF_LOADING;
        xglCdReadFile(name, resScriptBuffer, 1, resScriptRun);
    }
}

/* Two 16-bit halves of one word; lo is the half the reader replaces. */
typedef struct {
    unsigned lo : 16;
    unsigned hi : 16;
} ResHalfWordBits;

/* Reads a 32-bit value stored as two halfwords, low at p[0] and high at p[1] (proposed resLoadHalfPair); the second argument's low half is replaced by p[0] before the result is built. */
u32 resReadHalfPairWord(u16 *p, ResHalfWordBits prev)
{
    u32 hi;

    prev.lo = p[0];
    hi = (u32)p[1] << 16;
    return prev.lo | hi;
}

typedef union { f32 f; int w; } JumpFloat;

typedef struct ResSlotName {
    u8 text[16];
} ResSlotName;

typedef struct JumpRecord {
    u8 kind;
    signed char flag;
    s16 stackWord;
    u8 unmodeled_04[0x0C];
    union {
        JumpFloat unsetValue[4];
        ResSlotName name;
    };
} JumpRecord;

extern JumpRecord resJumpRecords[];

typedef struct JumpTableHeader {
    u8 unmodeled_00[0x0C];
    int nameCount;
    u8 unmodeled_10[0x04];
    int cursorOffset;
    u8 unmodeled_18[0x08];
    int nameTableOffset;
} JumpTableHeader;

typedef struct JumpNameEntry {
    int nameOffset;
    int length;
} JumpNameEntry;

extern JumpTableHeader D_003C6DC0;
extern char D_0069CF60[];
extern char D_0069CF68[];
extern char D_0069CF70[];
int strcmp(const char *, const char *);
int func_A9D148(void *, void *);
int func_A9DAD0(u16 op, int value);

void resParseJumpTarget(void)
{
    extern u32 resReadHalfPairWord();
    JumpTableHeader *header;
    JumpNameEntry *entry;
    u8 *names;
    u16 *start;
    u16 *cursor;
    u16 *table;
    int *stack;
    int nameIndex;
    int index;
    int matchKey;
    int recordCount;
    int value;
    u16 op;

    header = &D_003C6DC0;
    start = (u16 *) ((u8 *) header + header->cursorOffset);
    cursor = start;
    names = (u8 *) header + header->nameTableOffset;
    entry = (JumpNameEntry *) names;
    for (nameIndex = 0; nameIndex < header->nameCount; nameIndex++) {
        if (strcmp(names + entry->nameOffset, D_0069CF60) == 0) {
            cursor += entry->length;
            break;
        }
        entry++;
    }
    if (cursor == start) {
        return;
    }
    matchKey = func_A9D148(D_0069CF68, D_0069CF70);
    do {
    } while (0);
    recordCount = 0;
    op = cursor[0];
    while (op != SB_OP_END) {
        value = resReadHalfPairWord(cursor + 1);
        if (op == SB_OP_JUMP) {
            cursor = start + value;
            for (;;) {
                op = cursor[0];
                if (op == SB_OP_END) {
                    return;
                }
                value = resReadHalfPairWord(cursor + 1);
                if (op == SB_OP_LOOKUPSWITCH) {
                    break;
                }
                cursor += func_A9DAD0(op, value);
            }
            recordCount = value;
            cursor += 5;
            break;
        }
        cursor += func_A9DAD0(op, value);
        op = cursor[0];
    }

    if (op == SB_OP_END) {
        return;
    }
    table = cursor;
    for (index = 0; index < recordCount; index++) {
        stack = (int *) SB_STACK_BASE;
        resJumpRecords[index].kind = resReadHalfPairWord(table) - 1;
        resJumpRecords[index].flag = 0;
        resJumpRecords[index].stackWord = 0;
        resJumpRecords[index].unsetValue[0].f = resJumpRecords[index].unsetValue[1].f = resJumpRecords[index].unsetValue[2].f = -1000.0f;
        resJumpRecords[index].unsetValue[3].w = 0;
        cursor = start + resReadHalfPairWord(table + 2);
        table += 4;
        for (;;) {
            op = cursor[0];
            if (op == SB_OP_END) {
                break;
            }
            value = resReadHalfPairWord(cursor + 1);
            if (op == SB_OP_CALL_INDIRECT && value == matchKey) {
                resJumpRecords[index].stackWord = (u16) stack[-*stack];
                break;
            }
            if (op == SB_OP_JUMP) {
                break;
            }
            switch (op) {
            case SB_OP_CALL_INDIRECT:
                stack = (int *) SB_STACK_BASE;
                break;
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0x08:
            case 0x09:
            case 0x26:
                stack += 1;
                *stack = value;
                break;
            case 0x01:
            case 0x02:
            case 0x0A:
            case 0x0B:
                stack -= 1;
                break;
            }
            cursor += func_A9DAD0(op, value);
        }
        if (op == SB_OP_END) {
            break;
        }
    }
    resJumpRecords[index].kind = RES_JUMP_END;
}

f32 I2F();
void *mapFindBlockEntry();
f32 xglPointLength();

typedef struct MapKeyEntry {
    int key;
    int unmodeled_04;
} MapKeyEntry;

typedef struct MapBlockHeader {
    u16 entryCount;
    u16 unmodeled_02;
    int vertexBase;
    int unmodeled_08;
    MapKeyEntry *keyTable;
} MapBlockHeader;

typedef struct MapEntry {
    u16 keyIndex;
    u16 unmodeled_02;
    int unmodeled_04;
    u16 vertexA;
    u16 vertexB;
    u16 vertexC;
    u16 unmodeled_0E;
} MapEntry;

void resJumpPointsBuild(void) {
    int entryIndex;
    int key;
    int vertexBase;
    MapKeyEntry *keyTable;
    int i;
    int j;
    int k;
    MapBlockHeader *block;
    MapEntry *entry;
    JumpRecord *record;
    JumpFloat *vertexA;
    JumpFloat *vertexB;
    JumpFloat *vertexC;
    f32 center[3];
    f32 count;
    f32 distance;
    f32 denom;
    f32 minDistance;

    i = 0;
    do {
        block = mapFindBlockEntry(RES_MAP_KEY_JUMP, i);
        if (block != 0) {
            entry = (MapEntry *) (block + 1);
            vertexBase = block->vertexBase;
            keyTable = block->keyTable;
            for (entryIndex = 0; entryIndex < block->entryCount; entryIndex++) {
                key = keyTable[entry->keyIndex].key;
                if ((key & ~0x1F) == RES_MAP_KEY_JUMP) {
                    for (k = 0; resJumpRecords[k].kind != RES_JUMP_END; k++) {
                        record = &resJumpRecords[k];
                        if (record->kind == (key & 0xFF)) {
                            vertexA = (JumpFloat *) (vertexBase + entry->vertexA * 0x10);
                            vertexB = (JumpFloat *) (vertexBase + entry->vertexB * 0x10);
                            vertexC = (JumpFloat *) (vertexBase + entry->vertexC * 0x10);
                            center[0] = (vertexA[0].f + vertexB[0].f + vertexC[0].f) / 3.0f;
                            center[1] = (vertexA[1].f + vertexB[1].f + vertexC[1].f) / 3.0f;
                            center[2] = (vertexA[2].f + vertexB[2].f + vertexC[2].f) / 3.0f;
                            count = I2F((u8) record->flag);
                            denom = count + 1.0f;
                            center[0] = center[0] + record->unsetValue[0].f * count;
                            center[1] = center[1] + record->unsetValue[1].f * count;
                            center[2] = center[2] + record->unsetValue[2].f * count;
                            record->flag = (signed char) ((u8) record->flag + 1);
                            record->unsetValue[0].f = center[0] / denom;
                            record->unsetValue[1].f = center[1] / denom;
                            record->unsetValue[2].f = center[2] / denom;
                            break;
                        }
                    }
                }
                entry++;
            }
        }
        i++;
    } while (i < 0x10);

    for (i = 0; resJumpRecords[i].kind != RES_JUMP_END; i++) {
        minDistance = 100000.0f;
        for (j = 0; resJumpRecords[j].kind != RES_JUMP_END; j++) {
            if (i != j && resJumpRecords[i].stackWord != resJumpRecords[j].stackWord) {
                distance = xglPointLength(&resJumpRecords[i].unsetValue[0].f, &resJumpRecords[j].unsetValue[0].f) * 0.5f;
                if (distance < minDistance) {
                    minDistance = distance;
                }
            }
        }
        resJumpRecords[i].unsetValue[3].f = minDistance;
    }
}

void resJumpRecordSet(int index, int cfId, ResSlotName *name) {
    if (name == 0) {
        return;
    }
    if (index < 0) {
        for (index = 0; resJumpRecords[index].kind != RES_JUMP_END; index++) {
        }
        resJumpRecords[index + 1].kind = RES_JUMP_END;
    }
    resJumpRecords[index].kind = RES_JUMP_NAMED;
    resJumpRecords[index].flag = 0;
    resJumpRecords[index].stackWord = cfId;
    resJumpRecords[index].name = *name;
}

int xglCdSyncDone(void);

/* Loads the cf file of the first jump point whose radius reaches the current map object. A jump
   record holds its point in unsetValue[0..2], the radius in unsetValue[3] and the cf file in stackWord;
   gameWork[6] is the map id. */
void resJumpPointsUpdate(void)
{
    AlignedVector4 *position;
    int i;

    seVoiceRecordsUpdatePan();
    if (resJumpRecords[0].kind == RES_JUMP_END) {
        resParseJumpTarget();
        resJumpPointsBuild();
        resJumpRecordSet(0, 0, 0);
    }
    if (gameWork[6] == 1520 || gameWork[6] == 1560) {
        return;
    }
    if (xglCdSyncDone() == 0) {
        return;
    }
    position = &uwamonoRecords[uwamonoCurrentIndex].actor->position;
    for (i = 0; resJumpRecords[i].kind != RES_JUMP_END; i++) {
        if (xglPointLength(position, &resJumpRecords[i].unsetValue[0].f) < resJumpRecords[i].unsetValue[3].f) {
            if (resJumpRecords[i].stackWord > 0) {
                resLoadCfFile(resJumpRecords[i].stackWord);
                return;
            }
        }
    }
}

extern u32 D_006A0BC0[];
int func_0019C338(void);
int func_001E0928(u32 addr, int *out, int a2, int a3);
int func_001DF2D8(int value);

typedef struct {
    u8 bytes[0x10];
} ResBlock;

extern unsigned char D_006A0AC0[];

typedef struct {
    u16 value;
    u8 mode;
} ResNameLatch;

int GameResBlockFreeByOwner(int owner);

void resResetNameTables(void)
{
    CdSlot *head;
    CdSlot *walk;
    CdSlot *entry;
    CdSlot *end;
    int found;
    int index;
    u32 flags;
    u32 keepFlag;

    ((u32 *)resEntryTable)[0] = 1;
    ((u32 *)D_006A0AC0)[0] = 1;
    resNameCursor = resNamePool;
    head = xglCdSlotTable;
    do {
        found = 0;
        index = head[0].owner;
        if (index < 128 && head[index].buffer != 0) {
            walk = xglCdSlotTable;
            entry = &walk[index];
            keepFlag = 0x40000000;
            end = &walk[128];
            do {
                flags = entry->owner;
                entry++;
                if ((flags & keepFlag) != 0) {
                    GameResBlockFreeByOwner(flags);
                    found = 1;
                    break;
                }
            } while ((int)entry < (int)end && entry->buffer != 0);
        }
    } while (found != 0);
    resCfCurrent.id = -1;
    resCfCurrent.kind = 0;
}

void resResetNameTables(void);

void resResetScriptState(void) {
    xglCdReadCancel(0);
    resResetNameTables();
}

/* Adds the record's address to its pointer words (toAbsolute != 0) or subtracts it, for kinds 0x10000, 0x20000 and 0x70000 (proposed resRelocateRecord). */
void resRelocateRecord(int toAbsolute, int kindWord, int *record) {
    int delta;
    u32 kind;

    kind = kindWord & CD_FILE_TYPE_MASK;
    switch (kind) {
    case RES_KIND_ACT:
    case RES_KIND_MAP:
    case RES_KIND_MOBJ:
        break;
    default:
        return;
    }
    delta = (int) record;
    if (toAbsolute == 0) {
        delta = -delta;
    }
    if (record[0] != 0) {
        record[0] += delta;
    }
    if (record[1] != 0) {
        record[1] += delta;
    }
    if (record[2] != 0) {
        record[2] += delta;
    }
    if (record[3] != 0) {
        record[3] += delta;
    }
    if (record[4] != 0) {
        record[4] += delta;
    }
}

extern int mainObject;
extern int D_002B5DB8[];
extern unsigned char resEnemyFootCount;
void mapLoadBlock(int block);
void sefStartEffectByCode(int code, char *name);
void Intermission(int mode);
void GameDiskChange(int mode);
void xglMemCopy16(void *dst, void *src, int count);
int xglCdServiceIfPending(void);

void resApplyCfFile(int id)
{
    int i;
    int k;
    int cur;
    int changed;
    int flags;
    int size;
    int found;
    int idx;
    unsigned int *base;
    unsigned int *owners;
    unsigned int *owner;
    unsigned int *entries;
    unsigned int entry;
    unsigned int kind;
    unsigned char *name;
    int code;
    int *oldRecord;
    CdSlot *slot;

    while (resCfCurrent.id != id || resCfCurrent.kind != 0) {
        resLoadCfFile(0x10000 | id);
        xglCdServiceIfPending();
    }
    resResetDescriptorTables();
    mainObject = 0;
    D_002B5DB8[0] = 0;
    mapLoadBlock(0);
    resJumpRecords[0].kind = RES_JUMP_END;
    seBgmVolume = -1;
    while (xglCdServiceIfPending() != 0)
        ;
    if (resScriptBuffer[0] == 0) {
        resResetSlotTable();
        resClearRecordTable();
    }

    base = (unsigned int *) D_006A0AC0;
    i = 1;
    if (i < base[0]) {
        owners = base;
        base = base + 1;
        owner = base;
        do {
            GameResBlockFreeByOwner(*owner);
            owner++;
            i++;
        } while (i < owners[0]);
    }

    do {
        changed = 0;
        slot = xglCdSlotTable;
        i = slot[0].owner;
        for (; i < GAME_RES_BLOCK_COUNT && xglCdSlotTable[i].buffer != 0; i++) {
            flags = xglCdSlotTable[i].owner;
            if (flags & RES_OWNER_PRELOADED) {
                size = xglCdSlotTable[i].size;
                found = 0;
                idx = xglCdSlotTable[0].owner;
                if (idx < i && xglCdSlotTable[idx].buffer != 0) {
                    while (idx < i && xglCdSlotTable[idx].buffer != 0) {
                        if (xglCdSlotTable[idx].owner == 0
                            && ((int) xglCdSlotTable[idx].size >= size || idx + 1 == i)) {
                            found = idx;
                            break;
                        }
                        idx++;
                    }
                }
                if (found == 0) {
                    xglCdSlotTable[i].owner &= 0xBFFFFFFF;
                } else {
                    oldRecord = (int *) xglCdSlotTable[i].buffer;
                    GameResBlockFreeByOwner(flags);
                    GameResBlockAssign(found, flags & 0xBFFFFFFF, size);
                    resRelocateRecord(0, flags, oldRecord);
                    xglMemCopy16(xglCdSlotTable[found].buffer, oldRecord, size / 16);
                    resRelocateRecord(1, flags, (int *) xglCdSlotTable[found].buffer);
                    changed = 1;
                    break;
                }
            }
        }
    } while (changed);

    entries = (unsigned int *) resEntryTable;
    for (i = 1; i < ((unsigned int *) resEntryTable)[0]; i++) {
        entry = entries[i];
        name = (unsigned char *) resNamePool + (entry & 0xFFFF);
        switch (entry & CD_FILE_TYPE_MASK) {
        case RES_KIND_SOUND:
            switch ((signed char) name[0]) {
            case 0 ... 63:
                if (((char *) name)[1] == 0) {
                    if ((signed char) name[0] == SE_BANK_FOOT_MATERIAL) {
                        for (k = SE_BANK_FOOT_MATERIAL; k < SE_BANK_FOOT_MATERIAL + 5; k++) {
                            seBankSetName(k, 0);
                        }
                    } else if ((signed char) name[0] == SE_BANK_ENEMY_FOOT) {
                        for (k = SE_BANK_ENEMY_FOOT; k < SE_BANK_ENEMY_FOOT + 5; k++) {
                            seBankSetName(k, 0);
                        }
                    } else {
                        seBankSetName((signed char) name[0], 0);
                    }
                } else {
                    seBankSetName((signed char) name[0], (char *) (name + 1));
                }
                break;
            /* footstep material k = name[1] has material name[2]; the list ends in 0xFF */
            case 'f':
                k = (signed char) name[1];
                resFootMaterials[k] = name[2];
                resFootMaterials[k + 1] = 0xFF;
                resEnemyFootCount = k + 1;
                break;
            /* enemy footstep group name[1] = file group id in name[2..5] */
            case 'e': {
                unsigned char word[4];

                word[0] = name[2];
                word[1] = name[3];
                word[2] = name[4];
                word[3] = name[5];
                resEnemyFootGroups[(signed char) name[1]] = *(int *) word;
                break;
            }
            /* BGM name list: skip names already played, then start the next (seBgmNameSet) */
            case 's':
                name++;
                while (D_0069CFD0 >= 2) {
                    while (*name++ != 0) {
                    }
                    D_0069CFD0--;
                }
                seBgmNameSet((signed char *) name);
                break;
            /* BGM volume */
            case 'v':
                seBgmVolume = name[1];
                break;
            }
            break;
        case RES_KIND_EFFECT:
            sefStartEffectByCode(0, (char *) name);
            break;
        case GAME_RES_OWNER:
            GameResLoadName((char *) name, 0, 0, 0);
            break;
        case RES_KIND_EVENT:
            code = ((unsigned char *) &entries[i])[0];
            switch (entry & 0xFF00) {
            case RES_EVENT_SAVE:
                Intermission(code);
                break;
            case RES_EVENT_DISK:
                GameDiskChange(code);
                break;
            }
            break;
        default:
            GameResLoadID(entries[i], 0, 0, 0);
            break;
        }
    }
    resResetScriptState();
}

void resGetEntryTables(u8 **first, u8 **second) {
    if (first != 0) {
        *first = (u8 *)resEntryTable;
    }
    if (second != 0) {
        *second = D_006A0AC0;
    }
}

/* Ten-byte pad text copied after each path buffer. */
typedef struct {
    char text[10];
} ResPadText;

int sprintf(char *destination, const char *format, ...);
extern char D_006882E8[];
extern ResPadText D_006882F8;

/* Loads resource 0x30041 and sets up the three path buffers (each followed by its pad text) and,
   in slot 3, the largest free block. */
void resPathSlotsInit(void)
{
    unsigned char *buffer;
    int i;

    GameResLoadID(0x30041, 0, 0, 0);
    for (i = 0; i < 3; i++) {
        buffer = GameResBufferAlloc(RES_PATH_BUFFER_SIZE);
        resPathSlots[i].unmodeled_04 = 0;
        resPathSlots[i].base = (int) buffer;
        sprintf((char *) buffer, D_006882E8, i);
        buffer = GameResBufferAlloc(RES_PATH_PAD_SIZE);
        *(ResPadText *) buffer = D_006882F8;
    }
    resPathSlots[3].base = (int) func_001DF9F0(0);
}

int resPathSlotFindByOwner(int owner) {
    int i;

    for (i = 0; i < 3; i++) {
        int index = GameResBlockFindByBuffer((void *) resPathSlots[i].base);
        if (index > 0 && xglCdSlotTable[index].owner == owner) {
            return i;
        }
    }
    return -1;
}

void XenoActorAllocMatrices(struct XenoActor *actor);
void tiMRPackageInit(void *package);
void tiMRPackageSetEntryA(void *package, int value);

void func_001E4178(int owner) {
    int i;

    for (i = 0; i < 256; i++) {
        if (uwamonoRecords[i].key == owner) {
            struct XenoActor *actor = uwamonoRecords[i].actor;

            if (actor != 0) {
                actor->kind = 0;
                actor->lookLink = 0;
                actor->slotTable = 0;
                actor->hierarchy = 0;
                actor->effect = 0;
                actor->pack = 0;
                XenoActorAllocMatrices(actor);
                tiMRPackageInit(actor->channel);
                tiMRPackageSetEntryA(actor->channel, 0);
            }
        }
    }
}

#include "debug_break.h"

void *GameResBlockAssign(int index, int owner, unsigned int size);
void GameResBlockFree(int index);
void func_001E4178(int handle);

typedef struct {
    char text[9];
} PlayerName;

extern char D_00688318[];

void resSetPathSlot(int slot, unsigned int base, unsigned int limit, int value)
{
    int i;

    do {
        for (i = 1; i < GAME_RES_BLOCK_COUNT; i++) {
            int blockStart;
            int owner;

            if (xglCdSlotTable[i].buffer == 0) {
                i = GAME_RES_BLOCK_COUNT;
                break;
            }
            blockStart = xglCdSlotTable[i].buffer;
            if (blockStart < base || blockStart >= limit) {
                continue;
            }
            owner = xglCdSlotTable[i].owner;
            if (owner == 0) {
                continue;
            }
            if (base != value) {
                func_001E4178(owner);
            }
            GameResBlockFree(i);
            break;
        }
    } while (i < GAME_RES_BLOCK_COUNT);
    if (base != value) {
        i = GameResBlockFindByBuffer((void *)base);
        if (i == 0) {
            printf("block lost\n");
            DEBUG_BREAK();
        }
        GameResBlockAssign(i, GAME_RES_OWNER, RES_PATH_BUFFER_SIZE);
        *(PlayerName *)base = *(PlayerName *)D_00688318;
    }
    for (i = 1; i < GAME_RES_BLOCK_COUNT; i++) {
        if (xglCdSlotTable[i].buffer == value) {
            xglCdSlotTable[0].owner = i;
            break;
        }
    }
    resPathSlots[slot].unmodeled_04 = 0;
}

void resSetPathSlot(int slot, unsigned int base, unsigned int limit, int value);

void resUpdatePathSlot(int slot)
{
    int base;

    if (slot < 0) {
        base = resPathSlots[2].base;
        resSetPathSlot(2, base, base + 0xC6000, base);
        if (slot < -1) {
            base = resPathSlots[1].base;
            resSetPathSlot(1, base, base + 0xC6000, base);
        }
        if (slot < -2) {
            base = resPathSlots[0].base;
            resSetPathSlot(0, base, base + 0xC6000, base);
        }
        return;
    }
    base = resPathSlots[slot].base;
    resSetPathSlot(slot, base, base + 0xC5FC0, xglCdSlotTable[xglCdSlotTable[0].owner].buffer);
}

int GameResFindID(int key, int *out);

static inline void resRecordSlotBlock(int slot, int mask, int *out, int block) {
    if (out != 0) {
        *out = block;
    }
    if (block == resPathSlots[slot].base) {
        resPathSlots[slot].unmodeled_04 = mask;
    }
}

void resPathSlotLoadID(int slot, int value, int *out) {
    int resourceId;
    int savedSelected;
    int slotBase;
    int limit;
    int index;
    int blockStart;
    int owner;
    int size;
    int loaded;

    resourceId = value & GAME_RES_ID_MASK;
    savedSelected = xglCdSlotTable[xglCdSlotTable[0].owner].buffer;
    slotBase = GameResFindID(resourceId, 0);
    if (slotBase != 0) {
        if (out != 0) {
            *out = slotBase;
        }
        if (slotBase == resPathSlots[slot].base) {
            resPathSlots[slot].unmodeled_04 = resourceId;
        }
        return;
    }
    slotBase = resPathSlots[slot].base;
    limit = slotBase + RES_PATH_BUFFER_SIZE;
    for (index = 1; index < GAME_RES_BLOCK_COUNT && xglCdSlotTable[index].buffer != 0; index++) {
        blockStart = xglCdSlotTable[index].buffer;
        if ((u32) blockStart >= (u32) slotBase && (u32) blockStart < (u32) limit) {
            owner = xglCdSlotTable[index].owner;
            if ((owner & GAME_RES_OWNER_MASK) == GAME_RES_OWNER || owner == 0) {
                size = xglCdLoadByKind(value, 0, 0, 0);
                if (size != 0) {
                    loaded = (int) GameResBlockAssign(index, resourceId, size);
                    xglCdLoadByKind(value, loaded, 0, 0);
                    if (out != 0) {
                        *out = loaded;
                    }
                    if (loaded == resPathSlots[slot].base) {
                        resPathSlots[slot].unmodeled_04 = resourceId;
                    }
                }
                break;
            }
        }
    }
    for (index = 1; index < GAME_RES_BLOCK_COUNT; index++) {
        if (xglCdSlotTable[index].buffer == savedSelected) {
            xglCdSlotTable[0].owner = index;
            break;
        }
    }
}

typedef struct {
    char text[9];
} ResPlayerName;

/* A path slot: the buffer block, directly followed by the pad block holding the pad text. */
typedef struct {
    unsigned char data[RES_PATH_BUFFER_SIZE];
    ResPadText pad;
} ResPathSlotBuffer;

void resPathSlotBlockReset(void *buffer) {
    int block;

    block = GameResBlockFindByBuffer(buffer);
    if (block != 0 && resSelectedSlot[block * 5] == 0) {
        GameResBlockAssign(block, GAME_RES_OWNER, RES_PATH_BUFFER_SIZE);
        *(ResPlayerName *) buffer = *(ResPlayerName *) D_00688318;
        GameResBlockAssign(block + 1, GAME_RES_OWNER, RES_PATH_PAD_SIZE);
        ((ResPathSlotBuffer *) buffer)->pad = D_006882F8;
    }
}

void resPathSlotsLoadModels(int, int, int);
void resPathSlotBlockReset(void *buffer);

void resResetPathSlots(void) {
    int i;

    resPathSlotBlockReset(resPathSlots[0].base);
    resPathSlotBlockReset(resPathSlots[1].base);
    resPathSlotBlockReset(resPathSlots[2].base);
    for (i = 1; i < 0x80; i++) {
        if (xglCdSlotTable[i].buffer == resPathSlots[3].base) {
            xglCdSlotTable[0].owner = i;
            break;
        }
    }
    resPathSlotsLoadModels(resPathSlots[0].unmodeled_04, resPathSlots[1].unmodeled_04, resPathSlots[2].unmodeled_04);
}

typedef struct FootRecord {
    int key;
    u8 unk04[0x86C];
} FootRecord;

extern int D_002BB6A8[];
extern u8 *D_002BB680[];
extern FootRecord D_0033FDC4[];
extern u8 D_002BB748[];
extern u8 D_002BB758[];
int xglFlagsGet8(int flag);
void func_001A3E10(int mode);
int seBankSetName(int kind, char *text);
float seSetMaxDistance(float maxDistance);

void seFootBanksLoad(void) {
    int footKey;
    int materialGroup;
    int listIndex;
    int i;
    const u8 *groupMaterial;

    footKey = D_0033FDC4[uwamonoCurrentIndex].key;
    groupMaterial = 0;
    materialGroup = 0;
    listIndex = 0;
    if (-1 != D_002BB6A8[0]) {
        if (D_002BB6A8[0] == footKey) {
            groupMaterial = D_002BB680[0];
        } else {
            int listKey;

        next:
            if (D_002BB6A8[listIndex + 1] == -1) {
                listIndex++;
                materialGroup++;
            }
            listIndex++;
            listKey = D_002BB6A8[listIndex];
            if (listKey == -1) {
                goto done;
            }
            if (listKey != footKey) {
                goto next;
            }
            groupMaterial = D_002BB680[materialGroup];
        }
    }
done:
    D_002BB748[3] = groupMaterial[0];
    D_002BB758[6] = D_002BB748[3];
    D_002BB748[4] = D_002BB758[7] = groupMaterial[1];
    D_002BB748[5] = D_002BB758[8] = groupMaterial[2];
    if (materialGroup == 6) {
        if (xglFlagsGet8(401) > 13) {
            D_002BB748[5] = 0x32;
        }
    }
    if (materialGroup < 7) {
        seSetMaxDistance(10);
        func_001A3E10(-1);
    } else {
        seSetMaxDistance(60);
        func_001A3E10(-2);
    }
    seBankSetName(SE_BANK_FOOT, 0);
    for (i = SE_BANK_FOOT_MATERIAL; i < SE_BANK_ENEMY_FOOT; i++) {
        seBankSetName(i, 0);
    }
    seBankSetName(SE_BANK_FOOT, D_002BB758);
    for (i = SE_BANK_FOOT_MATERIAL; i < SE_BANK_ENEMY_FOOT; i++) {
        seBankSetName(i, 0);
    }
    for (i = 0; i < RES_FOOT_MATERIAL_MAX && resFootMaterials[i] != -1; i++) {
        {
            u8 *footMaterial = footMaterialKey(resFootMaterials[i]);

            D_002BB748[7] = footMaterial[0];
            D_002BB748[8] = footMaterial[1];
            D_002BB748[9] = footMaterial[2];
        }
        seBankSetName(i + SE_BANK_FOOT_MATERIAL, D_002BB748);
    }
}

extern unsigned char D_002BB768[];

int resFootMaterialIndex(int code) {
    int i;

    if ((code & 0xFFFFFF00) == 0x400) {
        code = D_002BB768[code & 0xFF];
        for (i = 0; i < RES_FOOT_MATERIAL_MAX && resFootMaterials[i] != -1; i++) {
            if (resFootMaterials[i] == code) {
                return i;
            }
        }
    }
    return -1;
}

typedef struct UwamonoWordRecord UwamonoWordRecord;

/* One 0x870-byte record of D_00340330, indexed like uwamonoRecords; the first word holds the flags. */
struct UwamonoWordRecord {
    int flags;                  /* +0x00: bit 0x2 tested by seFootBankFromKind */
    unsigned char unmodeled_04[0x870 - 4];
};

extern UwamonoWordRecord D_00340330[];
extern unsigned char resEnemyFootCount;
extern char D_0069CFC8[];
void xglFontDebugPrintf(int x, int y, const char *format, ...);
int resFootMaterialIndex(int key);

int seFootBankFromKind(int kind, int code) {
    int i;
    int kindClass;
    int slot;

    kindClass = kind & 0xFFFFF000;
    if (kindClass == 0x10000 || kindClass == 0x14000) {
        if (!(D_00340330[uwamonoCurrentIndex].flags & 2)) {
            return -1;
        }
        slot = resFootMaterialIndex(code);
        xglFontDebugPrintf(64, 0, D_0069CFC8, code, slot);
        return slot < 0 ? slot : slot + SE_BANK_FOOT_MATERIAL;
    }
    if (kind == 0x18037) {
        kind = 0x12047;
    }
    if (kind == 0x18038) {
        kind = 0x12048;
    }
    if (kind == 0x1901E) {
        kind = 0x12049;
    }
    if (kind == 0x18020) {
        kind = 0x1204A;
    }
    if (kind == 0x18021) {
        kind = 0x1204B;
    }
    i = 0;
    while (resEnemyFootGroups[i] != -1) {
        if (resEnemyFootGroups[i] == kind) {
            slot = resFootMaterialIndex(code);
            if (slot < 0) {
                return slot;
            }
            return resEnemyFootCount * i + slot + SE_BANK_ENEMY_FOOT;
        }
        i++;
    }
    return -1;
}
