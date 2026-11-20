#ifndef INCLUDE_MAIN_YAJIMA_TEST_H
#define INCLUDE_MAIN_YAJIMA_TEST_H

/* The debug test screens of src/main/yajima_test.c. YajimaTest runs the one the menu
 * selected through yajimaTestActions and hands it the 1-based selection. */

void PadTest(int selection);
void FontTest(int selection);
void SoundTest(int selection);
void HddTest(int selection);
void MemcardTest(int selection);
void CdSlotTest(int selection);
void MovieListTest(int selection);
void IpuMovieTest(int selection);
void KanjiCheckTest(int selection);
void EdgeTest(int selection);


void actionSoundSlotLevelSet(int index);
void bgmSlotLevelSet(int index);
void streamKeySlotLevelSet(int index);
int streamSetLevel(int index);

#endif /* INCLUDE_MAIN_YAJIMA_TEST_H */
