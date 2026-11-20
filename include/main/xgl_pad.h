#ifndef INCLUDE_MAIN_XGL_PAD_H
#define INCLUDE_MAIN_XGL_PAD_H

/* Functions of src/main/xgl_pad.c that other files call: the controller records are PadData (main/pad.h). */

void xglPadInitial(void);
/* Sets the auto-repeat of the buttons in mask (PAD_*) on a port (port -1 sets both): the
 * first repeat comes after delay frames, then one every interval frames. */
void xglPadSetRepeat(int port, unsigned short mask, int delay, int interval);
void xglPadRead(void);

#endif /* INCLUDE_MAIN_XGL_PAD_H */
