#include "common.h"
#include "main/cfeFileControl.h"
#include "main/xgl_debug_file.h"
#include "main/assert_fail.h"

typedef char *va_list;

#define va_start(ap, last) ((ap) = (va_list)__builtin_next_arg(last) - (8 - __builtin_args_info(2)) * 8)
#define va_end(ap) ((void)0)

int sceClose(int fd);
char *strcat(char *destination, const char *source);
void *memset(void *destination, int value, unsigned int size);
int strlen(const char *text);
int vsprintf(char *buffer, const char *format, va_list args);
/* pFileTemp: the text buffer the log lines are appended to. */
extern char *cfeFileTemp;

/* The assert texts "pFileTemp" and "size < MAX_OF_FILESIZE", and the source file name. */
#define MAX_OF_FILESIZE 0x60000

/* Closes a descriptor: a thin wrapper over sceClose. */
void cfeFileClose(int fd) {
    sceClose(fd);
}

/* Returns the pFileTemp buffer (cfeFileTemp). */
char *cfeFileTempGet(void) {
    return cfeFileTemp;
}

extern int cfeFileLogIndent;
void cfeFileLogIndentSet(int indent);

/* Sets pFileTemp to buffer, clears size bytes of it and resets the log indent to 0. */
void cfeFileTempInit(char *buffer, int size)
{
    cfeFileTemp = buffer;
    memset(buffer, 0, size);
    cfeFileLogIndentSet(0);
}

/* Sets the log indent level. */
void cfeFileLogIndentSet(int indent)
{
    cfeFileLogIndent = indent;
}


/* Adds `delta` to the log indent level (cfeFileLogIndent), never below 0. */
void cfeFileLogIndentAdd(int delta) {
    int total;

    total = cfeFileLogIndent + delta;
    cfeFileLogIndent = total;
    if (total < 0) {
        cfeFileLogIndent = 0;
    }
}

/* The indent string the log repeats per level. */
extern const char *cfeFileLogIndentStr;

/* Appends `count` copies of the indent string cfeFileLogIndentStr to pFileTemp. */
void cfeFileLogWriteIndent(int count) {
    int remaining;

    if (count > 0) {
        remaining = count;
        do {
            remaining -= 1;
            strcat(cfeFileTemp, cfeFileLogIndentStr);
        } while (remaining != 0);
    }
}

/* Log line: formats into the scratchpad at 0x70000000, appends the indent, then the text to pFileTemp.
   Does nothing when pFileTemp is null. */
void cfeFileLogf(const char *format, ...) {
    va_list args;

    va_start(args, format);
    if (cfeFileTemp != 0) {
        vsprintf((char *) 0x70000000, format, args);
        cfeFileLogWriteIndent(cfeFileLogIndent);
        strcat(cfeFileTemp, (const char *) 0x70000000);
    }
    va_end(args);
}

/* Writes the NUL-terminated pFileTemp text to the file `name`. */
int cfeFileTempWrite(const char *name)
{
    if (cfeFileTemp == 0) {
        assertFailed("pFileTemp", "../cfeFileControl.euc.c", 140);
    }
    return xglDebFileWrite(name, cfeFileTemp, strlen(cfeFileTemp) + 1);
}

/* Reads the file `name` into pFileTemp, stores the buffer in *buffer and returns the size. */
int cfeFileTempRead(const char *name, char **buffer)
{
    int size;

    if (cfeFileTemp == 0) {
        assertFailed("pFileTemp", "../cfeFileControl.euc.c", 151);
    }
    size = xglDebFileRead(name, cfeFileTemp);
    if (!(size < MAX_OF_FILESIZE)) {
        assertFailed("size < MAX_OF_FILESIZE", "../cfeFileControl.euc.c", 154);
    }
    *buffer = cfeFileTemp;
    return size;
}

/* Opens a block in the log: writes "{" at the current indent, then indents one level. */
void cfeFileLogBlockBegin(void) {
    cfeFileLogf("{\n");
    cfeFileLogIndentAdd(1);
}

/* Closes a block in the log: unindents one level, then writes "}" at that indent. */
void cfeFileLogBlockEnd(void) {
    cfeFileLogIndentAdd(-1);
    cfeFileLogf("}\n");
}

/* Data items of this file: the log indent counter, the indent string pointer (to the "\t" string of main/cfe_init.c's
 * rodata) and the pFileTemp buffer pointer. */
int cfeFileLogIndent = 0;
extern const char cfeFileLogIndentTab[];
const char *cfeFileLogIndentStr = cfeFileLogIndentTab;
char *cfeFileTemp = 0;
