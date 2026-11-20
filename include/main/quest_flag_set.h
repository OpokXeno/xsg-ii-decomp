#ifndef INCLUDE_MAIN_QUEST_FLAG_SET_H
#define INCLUDE_MAIN_QUEST_FLAG_SET_H

/* Functions of src/main/quest_flag_set.c that other files call. */

void questFlagSet(unsigned int index, int value);
void citizenFlagSet(unsigned int index, int value);
int questFlagGet(unsigned int index);

#endif /* INCLUDE_MAIN_QUEST_FLAG_SET_H */
