#ifndef INCLUDE_MAIN_TXD_TABLE_ENTRY_H
#define INCLUDE_MAIN_TXD_TABLE_ENTRY_H

/* Functions of src/main/txd_table_entry.c that other files call. */

void menuSceneResetNoop(void);
void *txdTableEntry(void *table, int index);
void ysegfDetailLeaveNoop(void);

#endif /* INCLUDE_MAIN_TXD_TABLE_ENTRY_H */
