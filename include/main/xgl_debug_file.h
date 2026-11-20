#ifndef INCLUDE_MAIN_XGL_DEBUG_FILE_H
#define INCLUDE_MAIN_XGL_DEBUG_FILE_H

/* Functions of src/main/xgl_debug_file.c that other files call: the host-file debug stubs. */

int xglDebFileSize(const char *name);
int xglDebFileRead(const char *name, void *buffer);
/* cfeFileTempWrite passes the byte count of the text as size. */
int xglDebFileWrite(const char *name, const void *buffer, int size);

#endif /* INCLUDE_MAIN_XGL_DEBUG_FILE_H */
