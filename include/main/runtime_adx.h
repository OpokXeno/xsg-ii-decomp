#ifndef INCLUDE_MAIN_RUNTIME_ADX_H
#define INCLUDE_MAIN_RUNTIME_ADX_H

struct SbThread;

/* Script natives of src/main/runtime_adx.c, listed in runtimeNativeTable: mpeg2 plays a movie, adx gives slot 0 an
 * ADX file, adxLoad does the same paused and adxPlay unpauses slot 0. */
int mpeg2(struct SbThread *call);
int adx(struct SbThread *call);
int adxLoad(struct SbThread *call);
int adxPlay(void);

#endif /* INCLUDE_MAIN_RUNTIME_ADX_H */
