#include "common.h"
#include "main/game_camera.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/msg_state.h"
#include "main/script_register_natives.h"
#include "main/talk_msg.h"

int strcmp(const char *left, const char *right);

/* vmArgGetHigh's type of a string argument. */
#define SCRIPT_ARG_STRING 3

/* "NULL" is the entry that ends a script string list. */

/* Script native talk.smsg (table 0x3ec878): begins a message. Argument 1 is one
 * string, or a list of strings ended by "NULL", for the window text; arguments 2
 * and up fill the message slots 1 and up. Clears the open slot's flag word and
 * updates the current map object. */
int talkSmsg(SbThread *thread)
{
    int index;
    int *list;
    int arg;
    void *object;

    /* The argument type is compared as a byte: without the cast gcc drops the andi 0xFF. */
    if ((unsigned char)vmArgGetHigh(thread, 1) != SCRIPT_ARG_STRING) {
        list = vmEntryGet(thread, vmArgGet(thread, 1));
        msgBegin();
        for (index = 0; strcmp("NULL", vmKeyedEntryGet(thread, list[index])) != 0; index++) {
            msgWindowSetScriptText(vmKeyedEntryGet(thread, list[index]));
        }
    } else {
        msgBegin();
        msgWindowSetScriptText(vmKeyedEntryGet(thread, vmArgGet(thread, 1)));
    }
    for (arg = 2; arg <= VM_PLUGIN_CNT_GET(thread); arg++) {
        msgWindowSetSlot(arg - 1, vmArgGet(thread, arg));
    }
    msgSlotSet(msgSlotCurrent(), 0);
    object = uwamonoCurrentRecord();
    mapObjectBeginMessage(object);
    mapObjectFaceActive(object);
    return 1;
}

/* Store script argument 1 as the flag word of the open message slot (none open:
 * nothing stored). No call or native table entry references it. */
int talkSetSlotFlags(SbThread *thread)
{
    int flags = vmArgGet(thread, 1);

    if (msgSlotCurrent() < 0) {
        return 1;
    }
    msgSlotSet(msgSlotCurrent(), flags);
    return 1;
}

/* Script native talk.talkMsgDtl (table 0x3ec880): talk.smsg with a leading argument.
 * Argument 1 is the flag word stored for the open slot, argument 2 the string or
 * "NULL"-ended string list for the window text, arguments 3 and up fill the
 * message slots 1 and up. */
int talkMsgDtl(SbThread *thread)
{
    int index;
    int *list;
    int arg;
    int flags;
    void *object;

    /* The argument type is compared as a byte, as in smsg. */
    if ((unsigned char)vmArgGetHigh(thread, 2) != SCRIPT_ARG_STRING) {
        list = vmEntryGet(thread, vmArgGet(thread, 2));
        msgBegin();
        for (index = 0; strcmp("NULL", vmKeyedEntryGet(thread, list[index])) != 0; index++) {
            msgWindowSetScriptText(vmKeyedEntryGet(thread, list[index]));
        }
    } else {
        msgBegin();
        msgWindowSetScriptText(vmKeyedEntryGet(thread, vmArgGet(thread, 2)));
    }
    for (arg = 3; arg <= VM_PLUGIN_CNT_GET(thread); arg++) {
        msgWindowSetSlot(arg - 2, vmArgGet(thread, arg));
    }
    flags = vmArgGet(thread, 1);
    msgSlotSet(msgSlotCurrent(), flags);
    object = uwamonoCurrentRecord();
    mapObjectBeginMessage(object);
    mapObjectFaceActive(object);
    return 1;
}

/* Names of script natives (.sdata, address order). The native tables of script_register_natives.c point at them;
 * no code of this file does. */
char talkNativeNameWait[8] = "wait";
char talkNativeNameResume[8] = "resume";
char talkNativeNameMsgDtl[8] = "msgDtl";
char talkNativeNameSmsg[8] = "smsg";
char talkNativeNameReset[8] = "reset";
char talkNativeNameCheck[8] = "check";
char segNativeNameAddrOn[8] = "addrOn";
