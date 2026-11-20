#ifndef INCLUDE_OV01_SCENE_LOAD_H
#define INCLUDE_OV01_SCENE_LOAD_H

/* Functions of src/ov01/scene_load.c that other files call. */

struct DataSlot;

void ov01StepChainStart(void);
int ov01LoadIsDone(void);
void ov01LoadRequest(struct DataSlot *owner);

#endif /* INCLUDE_OV01_SCENE_LOAD_H */
