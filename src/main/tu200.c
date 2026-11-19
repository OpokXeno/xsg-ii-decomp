#include "common.h"
#include "main/tu005.h"

int scriptReadFile(int *slot, char *fileName);

/* Builds "scNNNN.sb" from number (N = thousands, hundreds, tens, units digit) and loads it. */
int scriptLoadByNumber(int *slot, int number) {
    char fileName[] = "sc0000.sb";

    fileName[2] = ((number / 1000) % 10) + '0';
    fileName[3] = ((number / 100) % 10) + '0';
    fileName[4] = ((number / 10) % 10) + '0';
    fileName[5] = (number % 10) + '0';
    return scriptReadFile(slot, fileName);
}

int malloc(int size);
char *strcat(char *destination, const char *source);
int sbLinkImage(int buffer);
char *strcpy(char *, const char *);

/* Allocates a buffer in *slot, waits for it, then loads fileName from host0:.../cf/. */
int scriptReadFile(int *slot, char *fileName) {
    char path[0x100];
    const char *hostPath;
    int status;

    hostPath = "host0:/home/xeno/vcd/x2/cf/";
    *slot = malloc(100);
    while (*slot == 0) {
    }
    strcpy(path, hostPath);
    strcat(path, fileName);
    xglDebFileRead(path, (void *) *slot);
    status = sbLinkImage(*slot);
    if (status != 0) {
        return status;
    }
    for (;;) {
    }
}

void free(int value);

/* Frees the buffer in *slot (allocated by scriptReadFile) and clears the slot. */
void scriptFreeBuffer(int *slot) {
    free(*slot);
    *slot = 0;
}
