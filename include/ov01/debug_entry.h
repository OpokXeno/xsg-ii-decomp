#ifndef INCLUDE_OV01_DEBUG_ENTRY_H
#define INCLUDE_OV01_DEBUG_ENTRY_H

/* Functions of src/ov01/debug_entry.c (the BATTLE CONFIG debug menu) that other files call. */

/* Nonzero while the PAD_CIRCLE bit of PadData[0].pressed is set. */
int configStart(void);

/* Nonzero while bit 0x40 (PAD_CROSS) of PadData[0].pressed is set: the back button of the debug menus. */
int configCancelPressed(void);
/* Clears the page state and sets the debug defaults of the main page. */
void configInit(void);
/* Runs the debug menu frame until it reports done; mode 1 copies the three party status bytes first. */
int configMenuRun(int mode);

#endif /* INCLUDE_OV01_DEBUG_ENTRY_H */
