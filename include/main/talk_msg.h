#ifndef INCLUDE_MAIN_TALK_MSG_H
#define INCLUDE_MAIN_TALK_MSG_H

struct SbThread;

/* Script natives of src/main/talk_msg.c (talk module, native table talkNativeTable). */

int talkSmsg(struct SbThread *thread);
int talkSetSlotFlags(struct SbThread *thread);
int talkMsgDtl(struct SbThread *thread);

#endif /* INCLUDE_MAIN_TALK_MSG_H */
