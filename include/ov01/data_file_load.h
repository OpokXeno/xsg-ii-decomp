#ifndef INCLUDE_OV01_DATA_FILE_LOAD_H
#define INCLUDE_OV01_DATA_FILE_LOAD_H

/* Functions of src/ov01/data_file_load.c that other files call. */

/* The bat.dat buffer (main 0x0061D040) dataBatDatLoad reads the file to; data_record_get.c reads its unit tables. */
extern int batDat;

struct DataFileRecord {
    int loaded_index;
    char *buffer;
    int size;
};

/* Two adjacent 0x0c-byte file records: the .prj and .vsf of a sound bank (the battle bank bts_se,
 * and the voice of a slot). */
struct DataFileRecordPair {
    struct DataFileRecord prj;
    struct DataFileRecord vsf;
};

/* First two words of every file table (slot, stream entry, entry block): the cursor the next file is read to
 * (dataFileSlotLoad advances it) and the heap block the table owns (the same address, the heap block
 * of ov01HeapBlockOf). */
struct DataFileHead {
    char *next_buffer;
    union {
        char *buffer;
    } base;
};

/* The four files of a model: pxy, txy, xhr, epf (0x30 bytes). */
struct DataModelFiles {
    struct DataFileRecord pxy;
    struct DataFileRecord txy;
    struct DataFileRecord xhr;
    struct DataFileRecord epf;
};

/* A motion file record and the 0x0c bytes after it that nothing reads. */
struct DataMotionFile {
    struct DataFileRecord file;
    unsigned char unmodeled_0C[0xC];
};

/* The entry block: the model entry files from +0x08 (pxy, txy, xhr, epf), the motion records at +0x38 and
 * alt_index at +0x50. */
struct DataEntry {
    struct DataFileHead head;
    struct DataModelFiles model;
    struct DataFileRecord motions[2];
    int alt_index;
    unsigned char unmodeled_54[0x14];
};

/* The voice files of a slot at +0x130: the prj record (loaded_index is the voice) and the vsf record. */
struct DataSlot {
    struct DataFileHead head;
    int id;
    int handle;
    /* Up to four models, one DataModelFiles from +0x10 (index 0 is the unit itself) and one motion file from +0xD0. */
    struct DataModelFiles models[4];
    struct DataMotionFile motions[4];
    struct DataFileRecordPair voice_files;
};

/* One map of a stream: the pxy and txy files (0x18 bytes). */
struct DataMapFiles {
    struct DataFileRecord pxy;
    struct DataFileRecord txy;
};

/* A stream: the file table head, its key and its maps from +0x0C. */
struct DataStreamEntry {
    struct DataFileHead head;
    int key;
    struct DataMapFiles maps[1];
};

void dataBatDatLoad(void);
void dataBattleSeLoad(void);
void dataBattleSeRelease(void);
char *dataBattleActionPathGet(int kind, int mode);
void dataFaceFileLoad(void);
void dataSlotUnload(struct DataSlot *slot);
int dataStatBufGet(void);
void dataStatFileLoad(void);
/* Streams and effect slots: dataStreamOpen returns the stream record, unitEffectSlotAcquire takes an effect key. */
struct DataStreamEntry *dataStreamOpen(int count, int small_buffer);
int unitEffectSlotAcquire(int key);
/* The stream record of a key. */
struct DataStreamEntry *dataStreamFind(int key);
/* The data slot of a unit id that dataUnitFileLoad loads, and the lookup dataSlotFind. */
struct DataSlot *dataUnitFileLoad(int id);
struct DataSlot *dataSlotFind(int id);
int dataSlotRelease(int id);
int dataSlotFlagClear(int id);
void dataSlotVoiceReset(int id);
/* The model entry of a table index and the motion entry of a table index, motion and suffix. */
struct DataEntry *dataModelEntryLoad(int table_index);
struct DataEntry *dataMotEntryLoad(int table_index, int index, int suffix_index);
void dataFileInit(void);
void dataCdArenaOpen(void);
int dataEntryBlockReset(void);
int dataFaceHandleGet(void);
void dataThinkScriptLoad(int index);
/* The path of the battle sound file of an action code, in dataPathBuf; null when the code has none. */
struct BattleUnit;
char *dataBattleCodePathGet(struct BattleUnit *object, int code);

#endif /* INCLUDE_OV01_DATA_FILE_LOAD_H */
