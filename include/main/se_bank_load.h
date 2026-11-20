#ifndef INCLUDE_MAIN_SE_BANK_LOAD_H
#define INCLUDE_MAIN_SE_BANK_LOAD_H

/* Functions of src/main/se_bank_load.c that other files call. */

void seBgmStop(void);
void seSetMuted(int player_id, int flag);
void seVoiceStartDefault(int voice);
int seBankLoad(int slot, const char *name, int bank_address);
int seBankLoadDefault(int slot, const char *name);
int seBankLoadMemory(int slot, void *vsf, void *prj, int bank_address);
void seBgmPlay(int packed, const char *name, int bank_address);
/* Loads the song `name` (its bank and, for a packed id up to the maximum, its MIDI file); the id is the packed bank/song id. */
int seBgmLoad(int packed, const char *name, int bank_address);
/* Starts song id (loaded as next_id by seBgmLoad); returns the driver's reply. */
int seBgmStart(int id, int next_id);
/* Plays bank `packed & 0x7F` on the next channel in turn; above 0xFFFFFF it also carries volume and pan. */
void seTonePlay(int packed);
void seVolumeSet(int level, int step);
void seVoiceStart(int voice, int volume, int pan);
void seVoiceStop(int voice);
void seVoiceSetPanVolume(int voice, int volume, int pan);

#endif /* INCLUDE_MAIN_SE_BANK_LOAD_H */
