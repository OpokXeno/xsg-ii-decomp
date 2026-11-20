#include "common.h"
#include "main/sef.h"

/* Both return 0; their callers are not read in this pass. */
int sefDummy(void) {
    return 0;
}

int sefDummy_00259630(void) {
    return 0;
}

/* Address of entry entry_index in the table sefEntryCounts (SefEntryCounts, 20 bytes each). */
void *sefEntryCountsEntryGet(int entry_index) {
    return &sefEntryCounts[entry_index];
}
