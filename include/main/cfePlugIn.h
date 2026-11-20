#ifndef INCLUDE_MAIN_CFEPLUGIN_H
#define INCLUDE_MAIN_CFEPLUGIN_H

/* Functions of src/main/cfePlugIn.c that other files call. */

#include "main/cfe_list.h"

void cfeRegisterPlugIn(void);
void cfeDirectionFromAngles(float *result, const float *angles);

/* One 0x50-byte sound record of cfeSndRecords: the name is copied to offset 0 and the tween
   fields sit at +0x40..+0x4C (cfeEvaluate.c's cfeSndRecordEvaluate tweens them). */
typedef struct CfeSndRecord {
    char name[0x40];
    int value;                   /* +0x40: moves toward the target */
    int duration;                /* +0x44 */
    CfeTrackNode *track[2];      /* +0x48: CFE_SND_TRACK_* (cfe.stKySnd sets them by this index) */
} CfeSndRecord;

/* The tracks of a sound record: value, then duration. */
enum CfeSndTrack {
    CFE_SND_TRACK_VALUE,
    CFE_SND_TRACK_DURATION
};

/* Sound records in use: cfeSndRecordCount of them. */
extern CfeSndRecord cfeSndRecords[];
extern int cfeSndRecordCount;

#endif /* INCLUDE_MAIN_CFEPLUGIN_H */
