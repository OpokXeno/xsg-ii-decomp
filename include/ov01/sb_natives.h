#ifndef INCLUDE_OV01_SB_NATIVES_H
#define INCLUDE_OV01_SB_NATIVES_H

/* Functions of src/ov01/sb_natives.c that other files call: the argument readers of the script natives
 * (each takes the running thread, see include/ov01/sb_vm.h). */

struct SbThread;

int VM_PLUGIN_CNT_GET(struct SbThread *thread);
int vmArgGet(struct SbThread *thread, int argIndex);
int vmArgGetHigh(struct SbThread *thread, int argIndex);
int vmArgTagIsZero(struct SbThread *thread, int argIndex);
char *vmKeyedEntryGet(struct SbThread *thread, int key);
void *vmEntryGet(struct SbThread *thread, int index);
int vmSubEntryGet(struct SbThread *thread, int index, int subIndex);
void sbFatalReport(struct SbThread *thread, int pc);

/* The natives of the "bat" owner table (batMethods in sb_natives_unit.c). */
int posDataSet(struct SbThread *thread);
int monsTblSet(struct SbThread *thread);
int monsSetNoGet(void);
int cameraSet(struct SbThread *thread);
int thinkSet(struct SbThread *thread);
int thinkSet2(struct SbThread *thread);
int thinkSetEnd(struct SbThread *thread);
int atkTbl(struct SbThread *thread);
int atkTblGet(struct SbThread *thread);
int actorGet(struct SbThread *thread);
int unitParaGet(struct SbThread *thread);
int unitParaSet(struct SbThread *thread);
int loadEffect(struct SbThread *thread);
int loadWaitEffect(int unused, int *flag);
int createEffect(struct SbThread *thread);
int createEffectPos(struct SbThread *thread);
int tecParaGet(struct SbThread *thread);
int unitStatGet(struct SbThread *thread);
int unitStatSet(struct SbThread *thread);
int counterBoost(struct SbThread *thread);
int boost(struct SbThread *thread);
int VPadEnable(void);
int VPadDisable(void);

void *monsTblGet(void);
struct EffectPosRecord *posDataGet(void);
int cameraSettingGet(void);

#endif /* INCLUDE_OV01_SB_NATIVES_H */
