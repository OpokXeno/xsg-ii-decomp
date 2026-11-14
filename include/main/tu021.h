#ifndef INCLUDE_MAIN_TU021_H
#define INCLUDE_MAIN_TU021_H

/* Functions of src/main/tu021.c that other files call. */

void func_001A3A18(void);
void seSetMuted(int first, int flag);
void seVoiceStartDefault(int voice);
int seBankLoad(int slot, const char *name, int bank_size);
int seBankLoadDefault(int slot, const char *name);
int seBankLoadMemory(int slot, int vsf, int prj, int bank_size);
void seBgmPlay(int packed, const char *name, int bank_size);
void seVolumeSet(int level, int step);
void seVoiceStart(int voice, int volume, int pan);
void seVoiceStop(int voice);
void seVoiceSetPanVolume(int voice, int volume, int pan);

#endif /* INCLUDE_MAIN_TU021_H */
