#include "common.h"
#include "main/txd_table_entry.h"

void ysegfDetailLeaveNoop(void) {

}

/* Resolves a table entry as an offset relative to the table base: table + table[index]. */
void *txdTableEntry(void *table, int index) {
    char *base = table;
    int *offsets = table;

    return base + offsets[index];
}

void menuSceneResetNoop(void) {

}
