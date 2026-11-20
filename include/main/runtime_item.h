#ifndef INCLUDE_MAIN_RUNTIME_ITEM_H
#define INCLUDE_MAIN_RUNTIME_ITEM_H

struct SbThread;

/* Script natives of src/main/runtime_item.c that other files register. */

int runtimeItem(struct SbThread *thread);
int runtimeTimeAttack(struct SbThread *thread);
int runtimeGetAnalogR(struct SbThread *thread);

#endif /* INCLUDE_MAIN_RUNTIME_ITEM_H */
