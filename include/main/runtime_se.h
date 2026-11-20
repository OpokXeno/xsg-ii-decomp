#ifndef INCLUDE_MAIN_RUNTIME_SE_H
#define INCLUDE_MAIN_RUNTIME_SE_H

struct SbThread;

/* Script natives of src/main/runtime_se.c, listed in runtimeNativeTable: sound effects ("se", "sePos", "seVol"),
 * background music sound effects ("bgmse", "bgmsePos") and the BGM control natives. */
int seSetVolumePan(struct SbThread *call);
int seVol(struct SbThread *call);
int sePos(struct SbThread *call);
int bgmSe(struct SbThread *call);
int bgmSePos(struct SbThread *call);
int bgmFade(struct SbThread *call);
int nextBgm(struct SbThread *call);
int bgmSetTrack(struct SbThread *call);

#endif /* INCLUDE_MAIN_RUNTIME_SE_H */
