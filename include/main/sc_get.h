#ifndef INCLUDE_MAIN_SC_GET_H
#define INCLUDE_MAIN_SC_GET_H

/* Script effect record: the data of the type-list entry svGetScript returns. */
typedef struct SefScriptRecord {
    unsigned char unmodeled_00[0x10];
    short kind;                 /* 0x10: 0x3C..0x3F or 0x49 */
    short operand;              /* 0x12: immediate offset, slot count or table index */
    unsigned short offsets[1];  /* 0x14: offset table for kinds 0x3E and 0x3F */
} SefScriptRecord;

/* Functions of src/main/sc_get.c that other files call. */

/* scGetKeyRecord(int key, int *offset_out, int index) and scGetKeyRecordFirst(int key, int *offset_out) return the
 * SefScriptRecord * of a key and store its offset; they are not declared here because sef.c calls them in the word form
 * (see there). */
void sefAdvanceGroupKey(void);
int scGetImmAdrImmIdx2(int base, unsigned short *table, int index);
short scGetImmNumIdx(short *table, int index);
int scOfsToAdr(int offset);

#endif /* INCLUDE_MAIN_SC_GET_H */
