#ifndef INCLUDE_OV01_DATA_UNIT_ORG_GET_H
#define INCLUDE_OV01_DATA_UNIT_ORG_GET_H

/* Functions of src/ov01/data_unit_org_get.c that other files call. */

/* Header of one heap block (0x40 bytes, the payload follows): word 0 is the flags word (bit 0 set = in use),
   word 1 the payload size. */
typedef struct Ov01HeapBlock {
    unsigned int flags;
    int size;
} Ov01HeapBlock;

/* Overlay heap: ov01HeapAlloc returns a payload, ov01HeapFree takes a payload, ov01HeapBlockOf maps a payload to
   the address of its block header. */
void *ov01HeapAlloc(int size);
void ov01HeapFree(void *payload);
Ov01HeapBlock *ov01HeapBlockOf(void *payload);
void ov01HeapInit(void);

/* CD reads: dataFileLoad waits for the read; dataFileLoadNB starts one with dataNBreadCB and counts it as pending. */
int dataCdSync(void);
void dataCdSyncClear(void);
unsigned long long dataFileLoad(const char *name, void *buffer);
unsigned long long dataFileLoadNB(const char *name, void *buffer);
unsigned int ov01LookupNonHeap(int key);
int ov01HeapProbeLargeAlloc(void);

#endif /* INCLUDE_OV01_DATA_UNIT_ORG_GET_H */
