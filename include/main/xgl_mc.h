#ifndef INCLUDE_MAIN_XGL_MC_H
#define INCLUDE_MAIN_XGL_MC_H

/* Functions of src/main/xgl_mc.c that other files call: whole-file memory card access. */

int xglMcEasyLoad(const char *path, void *buffer, int size);
int xglMcEasySave(const char *path, const void *buffer, int size);
void xglMcInitial(void);

#endif /* INCLUDE_MAIN_XGL_MC_H */
