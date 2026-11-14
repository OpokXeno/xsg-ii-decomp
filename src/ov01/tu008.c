/*
 * OV01 original TU 8: 0x00a818e8..0x00a83d28 (53 functions)
 */
#include "common.h"
#include "main/tu021.h"
#include "ov01/tu008.h"
#include "ov01/tu009.h"
#include "ov01/tu070.h"

void dataFileBufAlloc(void);
int dataEntryBlockReset(void);
int ov01HeapInit(void);
void *memset(void *, int, unsigned int);
extern unsigned char dataSlotPool[0xE18];
extern unsigned char dataStreamPool[0x48];
extern unsigned char dataEntryBlock[0x68];

void dataFileInit(void) {
    memset(dataSlotPool, 0, sizeof(dataSlotPool));
    memset(dataStreamPool, 0, sizeof(dataStreamPool));
    memset(dataEntryBlock, 0, sizeof(dataEntryBlock));
    ov01CdPendingReset();
    ov01HeapInit();
    dataFileBufAlloc();
    dataEntryBlockReset();
}

extern int D_00AC0DF4;

int dataStatBufGet(void)
{
    return D_00AC0DF4;
}

extern int dataFaceHandle;

int dataFaceHandleGet(void)
{
    return dataFaceHandle;
}

struct DataFileRecord {
    unsigned char unmodeled_00[4];
    int buffer;
    int size;
};

/* Two adjacent 0x0c-byte file records: the .prj and .vsf of the battle sound bank bts_se (dataBattleSeLoad). */
struct DataFileRecordPair {
    struct DataFileRecord prj;
    struct DataFileRecord vsf;
};

int ov01CdReadFileAsync(int, int);
int ov01HeapAlloc(int);

void dataFileBufLoad(struct DataFileRecord *file_record, int path, int size) {
    int buffer;

    buffer = ov01HeapAlloc(size);
    ov01CdReadFileAsync(path, buffer);
    file_record->buffer = buffer;
    file_record->size = size;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataFileSlotLoad);

extern int dataBatDatPath;
extern int D_61D040;

void dataBatDatLoad(void) {
    ov01CdReadFileSync(dataBatDatPath, &D_61D040);
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataThinkScriptLoad);

extern int dataStatPath;
extern struct DataFileRecord dataStatRecord;

void dataStatFileLoad(void) {
    dataFileBufLoad(&dataStatRecord, dataStatPath, 0x41000);
}

extern int dataFacePath;
extern struct DataFileRecord dataFaceRecord;

void dataFaceFileLoad(void) {
    int result;

    result = ov01LookupNonHeap(0x30041);
    if (result != 0) {
        dataFaceHandle = result;
        return;
    }
    dataFileBufLoad(&dataFaceRecord, dataFacePath, 0x8000);
}

char *strcpy(char *, const char *);
char *func_002AA7F0(char *, char *);
void dataFileBufLoad(struct DataFileRecord *file_record, int path, int size);
extern char *dataBattleSeName;
extern char *dataVoiceDirPath;
extern char *dataVsfExt;
extern char *dataPrjExt;
extern struct DataFileRecordPair dataBattleSeRecords;
extern char dataPathBuf[];

void dataBattleSeLoad(void)
{
    strcpy(dataPathBuf, dataVoiceDirPath);
    func_002AA7F0(dataPathBuf, dataBattleSeName);
    func_002AA7F0(dataPathBuf, dataPrjExt);
    dataFileBufLoad(&dataBattleSeRecords.prj, dataPathBuf, 0x800);
    strcpy(dataPathBuf, dataVoiceDirPath);
    func_002AA7F0(dataPathBuf, dataBattleSeName);
    func_002AA7F0(dataPathBuf, dataVsfExt);
    dataFileBufLoad(&dataBattleSeRecords.vsf, dataPathBuf, 0x1E800);
}

typedef struct DataVoiceBlock {
    unsigned char unmodeled_000[0x130];
    unsigned char prj_slot[8];
    int prj_size;
    unsigned char vsf_slot[8];
    int vsf_size;
} DataVoiceBlock;

int ov01HeapFree(int);

void dataBattleSeRelease(void) {
    seBankLoadMemory(2, dataBattleSeRecords.vsf.buffer, dataBattleSeRecords.prj.buffer, 0);
    ov01HeapFree(dataBattleSeRecords.prj.buffer);
    ov01HeapFree(dataBattleSeRecords.vsf.buffer);
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", func_00A81D30);

int dataFileSlotLoad(void *, char *, void *, int);
extern char *dataPxyExt;
extern char *dataTxyExt;
extern char *dataXhrExt;
extern char *dataEpfExt;

int dataUnitFilesLoad(char *table, char *name, int index, int value)
{
    char *table_entry;
    int total;

    strcpy(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataEpfExt);
    table_entry = table + index * 0x30;
    total = dataFileSlotLoad(table_entry + 0x34, dataPathBuf, table, value);
    strcpy(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataXhrExt);
    total += dataFileSlotLoad(table_entry + 0x28, dataPathBuf, table, value);
    strcpy(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataTxyExt);
    total += dataFileSlotLoad(table_entry + 0x1C, dataPathBuf, table, value);
    strcpy(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataPxyExt);
    return total + dataFileSlotLoad(table_entry + 0x10, dataPathBuf, table, value);
}

extern char *dataMotDirPath;
extern char *dataXapExt;

int dataMotFileLoad(char *table, char *name, int index, int value)
{
    char **table_ref = &table;
    strcpy(dataPathBuf, dataMotDirPath);
    func_002AA7F0(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataXapExt);
    return dataFileSlotLoad(*table_ref + index * 0x18 + 0xD0, dataPathBuf, *table_ref, value);
}

int dataVoiceFilesLoad(DataVoiceBlock *block, char *name, int flag)
{
    void *voice_buffer;
    int prj_loaded;

    voice_buffer = 0;
    if (flag == 1) {
        voice_buffer = ov01HeapAlloc(block->prj_size + block->vsf_size);
    }
    strcpy(dataPathBuf, dataVoiceDirPath);
    func_002AA7F0(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataPrjExt);
    prj_loaded = dataFileSlotLoad(block->prj_slot, dataPathBuf, &voice_buffer, flag);
    strcpy(dataPathBuf, dataVoiceDirPath);
    func_002AA7F0(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataVsfExt);
    return prj_loaded + dataFileSlotLoad(block->vsf_slot, dataPathBuf, &voice_buffer, flag);
}

struct DataSlotResetArea {
    unsigned char unmodeled_00[4];
    int first_value;
    unsigned char unmodeled_08[8];
    int second_value;
    unsigned char unmodeled_14[4];
};

struct DataSlot {
    unsigned char unmodeled_00[0xC];
    int handle;
    unsigned char unmodeled_10[0x120];
    struct DataSlotResetArea reset_area;
};

int handleAddressGet(int);

void dataSlotUnload(struct DataSlot *slot) {
    int result;

    result = handleAddressGet(slot->handle);
    seBankLoadMemory(slot->handle, slot->reset_area.second_value, slot->reset_area.first_value, result);
    handleStateSet(slot->handle);
    ov01HeapFree(slot->reset_area.first_value);
}

extern char *dataMapDirPath;

int dataMapFileLoad(char *table, char *name, int index, int value)
{
    char *table_entry;
    int first_loaded;

    strcpy(dataPathBuf, dataMapDirPath);
    func_002AA7F0(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataPxyExt);
    table_entry = table + index * 0x18;
    first_loaded = dataFileSlotLoad(table_entry + 0xC, dataPathBuf, table, value);
    strcpy(dataPathBuf, dataMapDirPath);
    func_002AA7F0(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataTxyExt);
    return first_loaded + dataFileSlotLoad(table_entry + 0x18, dataPathBuf, table, value);
}

int dataEntryFilesLoad(char *table, char *name, int index, int value)
{
    char *table_entry;
    int total;

    strcpy(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataEpfExt);
    table_entry = table + index * 0x30;
    total = dataFileSlotLoad(table_entry + 0x2C, dataPathBuf, table, value);
    strcpy(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataXhrExt);
    total += dataFileSlotLoad(table_entry + 0x20, dataPathBuf, table, value);
    strcpy(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataTxyExt);
    total += dataFileSlotLoad(table_entry + 0x14, dataPathBuf, table, value);
    strcpy(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataPxyExt);
    return total + dataFileSlotLoad(table_entry + 0x08, dataPathBuf, table, value);
}

void dataMotSlotLoad(char *table, char *name, int index, int value)
{
    char **table_ref = &table;
    strcpy(dataPathBuf, dataMotDirPath);
    func_002AA7F0(dataPathBuf, name);
    func_002AA7F0(dataPathBuf, dataXapExt);
    dataFileSlotLoad(*table_ref + index * 0xC + 0x38, dataPathBuf, *table_ref, value);
}

struct DataEntry {
    unsigned char unmodeled_00[8];
    int index;
    unsigned char unmodeled_0c[0x44];
    int alt_index;
};

int func_00A82368(struct DataEntry *entry, int table_value, int which)
{
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataSlotFind);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataSlotAcquire);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataSlotRelease);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataSlotFlagSet);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataSlotFlagClear);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", func_00A82560);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataUnitLoad);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataUnitFileIdLoad);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataMotSetLoad);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataVoiceSetLoad);

struct DataSlot *dataSlotFind();

void dataSlotReset(void) {
    struct DataSlot *slot;

    slot = dataSlotFind();
    if (slot != 0) {
        memset(&slot->reset_area, 0, sizeof(slot->reset_area));
        slot->handle = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataModelLoad);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", func_00A82D50);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", func_00A82E18);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataStreamFind);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataStreamAcquire);

int dataStreamFind();

int func_00A82F60(void) {
    if (dataStreamFind() != 0) {
        return 0;
    }
    /* The original passes a zero base address to the clear of 0x24 bytes. */
    memset(0, 0, 0x24);
    return 1;
}

typedef struct StreamBuffer {
    void *base;
    void *cursor;
} StreamBuffer;

StreamBuffer *dataStreamAcquire(void);
int dataMapSetLoad(StreamBuffer *, int, int);

StreamBuffer *dataStreamOpen(int count, int small_buffer)
{
    StreamBuffer *buffer;
    int size;

    buffer = dataStreamAcquire();
    if (buffer == 0) {
        return buffer;
    }
    size = 0x200000;
    if (small_buffer != 0) {
        size = 0x2A800;
    }
    if (buffer->base == 0) {
        buffer->base = ov01HeapAlloc(size);
        buffer->cursor = buffer->base;
    }
    if (size < dataMapSetLoad(buffer, count, 0)) {
        while (1) {
        }
    }
    return buffer;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataMapSetLoad);

int *dataEntryBlockGet(void)
{
    return &dataEntryBlock;
}

extern int dataFileBufBase;

void dataFileBufAlloc(void) {
    dataFileBufBase = ov01HeapAlloc(0x2E800);
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataEntryBlockReset);


int dataEntryFilesLoad(char *table, char *name, int index, int value);
int *dataEntryBlockGet(void);
unsigned char *dataModelNameGet(int table_index);
extern char *dataModelDirPath;
extern char dataEntryPathBuf[];

struct DataEntry *dataModelEntryLoad(int table_index) {
    struct DataEntry *entry;
    unsigned char *table_text;

    entry = dataEntryBlockGet();
    strcpy(dataEntryPathBuf, dataModelDirPath);
    table_text = dataModelNameGet(table_index);
    func_002AA7F0(dataEntryPathBuf, table_text);
    entry->index = table_index;
    dataEntryFilesLoad(entry, dataEntryPathBuf, 0, 0);
    dataEntryFilesLoad(entry, dataEntryPathBuf, 0, 1);
    return entry;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", dataMotEntryLoad);

int func_00A82368(struct DataEntry *, int, int);
int dataVoiceNameGet(int index);

struct DataEntry *dataVoiceEntryLoad(int table_index) {
    struct DataEntry *entry;
    int table_value;

    entry = dataEntryBlockGet();
    table_value = dataVoiceNameGet(table_index);
    entry->alt_index = table_index;
    func_00A82368(entry, table_value, 0);
    func_00A82368(entry, table_value, 1);
    return entry;
}

extern unsigned char *dataModelNameTbl[];
extern unsigned char dataEmptyName[];

unsigned char *dataModelNameGet(int table_index) {
    unsigned char *text;

    text = dataModelNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return dataEmptyName;
}

extern unsigned char *dataMotNameTbl[];

unsigned char *dataMotNameGet(int table_index) {
    unsigned char *text;

    text = dataMotNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return dataEmptyName;
}

extern int dataVoiceNameTbl[];

int dataVoiceNameGet(int index)
{
    return dataVoiceNameTbl[index];
}

extern unsigned char *dataMapNameTbl[];

unsigned char *dataMapNameGet(int table_index) {
    unsigned char *text;

    text = dataMapNameTbl[table_index];
    if (text != 0) {
        return text;
    }
    return dataEmptyName;
}

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", func_00A83428);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", func_00A835D0);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", func_00A83678);

INCLUDE_ASM("asm/nonmatchings/ov01/tu008", func_00A837A0);
