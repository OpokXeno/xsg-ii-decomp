#ifndef INCLUDE_MAIN_SAVE_SCREEN_H
#define INCLUDE_MAIN_SAVE_SCREEN_H

/* Functions of src/main/save_screen.c that other files call. */

/* Names of the debug test menu items (EUC-JP text, 0x10 or 0x18 bytes apart). */
extern const char testUiItemNameText[];

/* Six bytes of saved maxima, one per party member that has them. */
typedef struct SaveTextMaximaBytes {
    signed char bytes[6];
} SaveTextMaximaBytes;

/* Runs the save/load screen; entryMode is reduced to a byte that selects the mode (callers pass 0 or 1). */
int MenuFileMain(int entryMode);
void configScreenRun(void);
void configScreenStateReset(void);
/* Copies the six saved maxima to dst; returns the byte at configNoticeShown (the caller tests its low two bits). */
signed char oldDataMaximaCopy(SaveTextMaximaBytes *dst);

#endif /* INCLUDE_MAIN_SAVE_SCREEN_H */
