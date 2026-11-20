#ifndef INCLUDE_MAIN_UWAMONO_RECORD_H
#define INCLUDE_MAIN_UWAMONO_RECORD_H

#include "shared.h"

struct XenoActor;

/* Functions of src/main/uwamono_record.c: the selection, pairing and pickup logic of the uwamono records. */

int uwamonoCountLeadingInUse(void);
void uwamonoReleaseRecord(int index);
int uwamonoFindRecord(int kind, int key);
int uwamonoStateCountLeading(void);
int uwamonoCountInReach(void);
int uwamonoFindNearest(void);
short uwamonoFindSelected(void);
void UnlockMapUnit(void);
void uwamonoUpdateInReach(struct XenoActor *actor, int index);
void uwamonoMoveSelection(int step);
int uwamonoIsActorInCells(int index);
int uwamonoFindEntry(int key);
float uwamonoGetEntryPose(int key, float *out);
void uwamonoSetPairMode(int index, int mode);
void uwamonoStartPairMove(int index, int mode, int duration);
int uwamonoFindTalkTarget(struct XenoActor *subject, float range);
void uwamonoStartPickupMove(int index);
void uwamonoUpdateItemPickups(void);
void uwamonoTriggerRecord(int index);
void uwamonoResetPoints(void);
void uwamonoAddPoint(Vector4 *position, int key, float value);
void uwamonoUpdatePoints(void);
int uwamonoGetNearbySearchType(void);

#endif /* INCLUDE_MAIN_UWAMONO_RECORD_H */
