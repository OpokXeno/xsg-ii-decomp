#ifndef INCLUDE_MAIN_CFE_TRACK_H
#define INCLUDE_MAIN_CFE_TRACK_H

/* Functions of src/main/cfe_track.c that other files call. */

#include "main/cfe_list.h"

void cfeTrackUpdateDirections(CfeTrackNode *track);
void cfeChainUnwrapAngles(CfeTrackNode *node);
float cfeTrackSample(CfeTrackNode *track, int time);
float cfeTrackSampleStep(CfeTrackNode *track, int time);

#endif /* INCLUDE_MAIN_CFE_TRACK_H */
