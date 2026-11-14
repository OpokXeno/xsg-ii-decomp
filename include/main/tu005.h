#ifndef INCLUDE_MAIN_TU005_H
#define INCLUDE_MAIN_TU005_H

/* Functions of src/main/tu005.c that other files call: the host-file debug stubs. */

int xglDebFileSize(const char *name);
int xglDebFileRead(const char *name, void *buffer);
int xglDebFileWrite(const char *name, const void *buffer);

#endif /* INCLUDE_MAIN_TU005_H */
