#ifndef INCLUDE_MAIN_TALK_NATIVES_H
#define INCLUDE_MAIN_TALK_NATIVES_H

#include "shared.h"
#include "main/xgl_mc_request.h"
#include "ov01/data_record_get.h"
#include "ov01/unit_cmd.h"
#include "ov01/sb_vm.h"

/* Functions of src/main/talk_natives.c that other files call. */

struct UwamonoRecord;
int talkCheck(void);
int talkReset(void);

/* The active map object and the talk result (.sdata of talk_natives.c). */
extern struct UwamonoRecord *mapActiveObject;
extern int talkResult;

struct SbThread;

/* Script natives of the talk module (native table talkNativeTable). */

int talkSmsg(struct SbThread *thread);
int talkSetSlotFlags(struct SbThread *thread);
int talkMsgDtl(struct SbThread *thread);

int talkWinClose(void);
int talkResume(void);
int talkEnableFlags(void);
int talkDisableFlags(void);
int talkIsDispMsg(void);
int talkWait(struct SbThread *unused_thread, int *out);

/* Script natives of the seg and g2path modules and the menu natives of the talk module: ctx is the script call,
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

/* The registration of the three modules. The native owner tables it registers
 * are SbNativeMethod arrays (ov01/sb_vm.h). */

void talkStateReset(void);
void scriptRegisterNatives(void);

/* Script native slotWinGetResult (the "talk" table); the second parameter is the status word. */
int slotWinGetResult(SbThread *thread, int *failure_flag);

/* The talk state of the map object a message belongs to (mapActiveObject, talkResult). */
void mapObjectBeginMessage(struct UwamonoRecord *object);
void mapObjectFaceActive(struct UwamonoRecord *object);
void mapObjectSetActive(struct UwamonoRecord *owner, struct UwamonoRecord *value);
int mapObjectClearTalkFlags(struct UwamonoRecord *object);
void talkSetResult(int value);

#endif /* INCLUDE_MAIN_TALK_NATIVES_H */
