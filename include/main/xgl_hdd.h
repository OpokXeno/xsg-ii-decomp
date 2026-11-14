#ifndef INCLUDE_MAIN_XGL_HDD_H
#define INCLUDE_MAIN_XGL_HDD_H

/* Functions of src/main/xgl_hdd.c that other files call. */

/* xglHddActivate: with this bit the new state is set without xglCdArcCheck. */
#define HDD_ACTIVATE_NO_ARC_CHECK 0x100

int xglHddActivate(int state);
int xglHddMount(void);

#endif /* INCLUDE_MAIN_XGL_HDD_H */
