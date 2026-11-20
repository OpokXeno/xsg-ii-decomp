#ifndef INCLUDE_MAIN_QUEST_FLAG_H
#define INCLUDE_MAIN_QUEST_FLAG_H

#include "ov01/sb_vm.h"

/* Script natives of src/main/quest_flag.c (tables in script_register_natives.c): ctx is the script call,
 * error_flag / arg_count_out the status word of the natives that can fail. */

int segAddrOnMessage(SbThread *ctx);
int segAddrOnGetFlag(SbThread *ctx);
int setQuestFlag(SbThread *ctx);
int getQuestFlag(SbThread *ctx);
int setCitizenFlag(SbThread *ctx);
int getCitizenFlag(SbThread *ctx);
int menuAddItem(SbThread *ctx, int *error_flag);
int menuDeptQuest(SbThread *ctx, int *error_flag);
int menuDeptGetResult(SbThread *unused_ctx, int *error_flag);
int menuGetResult(SbThread *ctx, int *arg_count_out);
int slotWinOpen(SbThread *ctx, int *error_flag);

#endif /* INCLUDE_MAIN_QUEST_FLAG_H */
