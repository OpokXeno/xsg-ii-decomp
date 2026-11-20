#include "common.h"
#include "main/xgl_debug_file.h"

int scriptReadFile(void **slot, char *fileName);

/* Builds "scNNNN.sb" from number (N = thousands, hundreds, tens, units digit) and loads it. */
int scriptLoadByNumber(void **slot, int number) {
    char fileName[] = "sc0000.sb";

    fileName[2] = ((number / 1000) % 10) + '0';
    fileName[3] = ((number / 100) % 10) + '0';
    fileName[4] = ((number / 10) % 10) + '0';
    fileName[5] = (number % 10) + '0';
    return scriptReadFile(slot, fileName);
}

void *malloc(int size);
char *strcat(char *destination, const char *source);
/* Local on purpose: the original callers pass one argument, while sbLinkImage (sb_vm.h) takes a second, unread
 * library word that sbLibraryLink passes; the full prototype would make these callers load $5 as well. */
int sbLinkImage(void *buffer);
char *strcpy(char *, const char *);

/* Allocates a buffer in *slot, waits for it, then loads fileName from host0:.../cf/. Original behaviour, kept as
 * coded: the buffer is 100 bytes, far smaller than a whole script. */
int scriptReadFile(void **slot, char *fileName) {
    char path[0x100];
    const char *hostPath;
    int status;

    hostPath = "host0:/home/xeno/vcd/x2/cf/";
    *slot = malloc(100);
    while (*slot == 0) {
    }
    strcpy(path, hostPath);
    strcat(path, fileName);
    xglDebFileRead(path, *slot);
    status = sbLinkImage(*slot);
    if (status != 0) {
        return status;
    }
    for (;;) {
    }
}

void free(void *buffer);

/* Frees the buffer in *slot (allocated by scriptReadFile) and clears the slot. */
void scriptFreeBuffer(void **slot) {
    free(*slot);
    *slot = 0;
}
