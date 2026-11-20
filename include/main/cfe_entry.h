#ifndef INCLUDE_MAIN_CFE_ENTRY_H
#define INCLUDE_MAIN_CFE_ENTRY_H

/* Functions of src/main/cfe_entry.c that other files call. */

void *cfeGetEntry(int index);
void cfeRangeSetMax(int upper);
void cfeRangeSetMin(int lower);
void cfeRangeSetValue(int value);
int cfeRangeGetValue(void);
int cfeRangeGetMin(void);

#endif /* INCLUDE_MAIN_CFE_ENTRY_H */
