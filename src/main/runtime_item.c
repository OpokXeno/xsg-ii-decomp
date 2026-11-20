#include "common.h"
#include "shared.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "ov01/data_record_get.h"
#include "main/game_data.h"
#include "ov01/data_box.h"
#include "ov01/calc.h"
#include "main/msg_state.h"
#include "main/text_box.h"
#include "main/runtime_item.h"
#include "main/menu_selection.h"


/* Debug pad state defined in src/main/uwamono_event_check.c. */
extern int padDebugSector;
extern int padDebugCount;

/* Natives used by runtimeItem (defined elsewhere) and the library compare helpers it calls. */
extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *dest, const char *src);
/* Text default and digit buffer: runtime_item.c's own data at 0x0069D220 and the rodata run at 0x00689700. */
extern char runtimeItemEmptyText[];
extern char itemPickupMessageBuffer[];

/* runtimeItem writes its digits into this buffer, so the original type is a writable string literal; as a named const array it stays in .rodata and `char *digits` keeps one qualifier warning (a plain char array moves to .data). */
/* EUC-JP text "00" (two digits written in place) then the rest of the 32-byte buffer, zero-filled. */
/* JP: を00個手に入れた！
 * EN: (after the item name) x00 obtained! */
const char itemObtainedCountText[32] = "\xa4\xf2\x30\x30\xb8\xc4\xbc\xea\xa4\xcb\xc6\xfe\xa4\xec\xa4\xbf\xa1\xaa";

int runtimeItem(SbThread *thread)
{
    int kind;
    int target;
    int count;
    int flag;
    int steps;
    char *text;
    char *digits;
    char *dest;
    char buf[256];

    steps = 0;
    digits = itemObtainedCountText;
    text = runtimeItemEmptyText;
    if (VM_PLUGIN_CNT_GET(thread) < 3) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    kind = vmArgGet(thread, 1);
    target = vmArgGet(thread, 2);
    count = vmArgTagIsZero(thread, 3) == 0 ? vmArgGet(thread, 3) : 0;
    flag = vmArgTagIsZero(thread, 4) == 0 ? (signed char) vmArgGet(thread, 4) : 0;
    switch (kind) {
    case 1:
        text = hitActionTextGet(target, 0);
        if (count > 0) {
            for (steps = 0; steps < count; steps++) {
                dataBoxIncCategory1(target);
            }
        } else {
            for (steps = 0; steps < __builtin_fabs(count); steps++) {
                dataBoxDecCategory1(target);
            }
        }
        steps = dataBoxChkCategory1(target);
        break;
    case 2:
        text = itemTextGet(target, 0);
        if (count > 0) {
            for (steps = 0; steps < count; steps++) {
                dataBoxIncCategory2(target);
            }
        } else {
            for (steps = 0; steps < __builtin_fabs(count); steps++) {
                dataBoxDecCategory2(target);
            }
        }
        steps = dataBoxChkCategory2(target);
        break;
    case 3:
        text = dataBoxCategory4TextGet(target, 0);
        if (count > 0) {
            for (steps = 0; steps < count; steps++) {
                dataBoxIncCategory4(target);
            }
        } else {
            for (steps = 0; steps < __builtin_fabs(count); steps++) {
                dataBoxDecCategory4(target);
            }
        }
        steps = dataBoxChkCategory4(target);
        break;
    case 4:
        text = dataBoxCategory3TextGet(target, 0);
        if (count > 0) {
            for (steps = 0; steps < count; steps++) {
                dataBoxIncCategory3(target);
            }
        } else {
            for (steps = 0; steps < __builtin_fabs(count); steps++) {
                dataBoxDecCategory3(target);
            }
        }
        selectionBlockFill();
        steps = dataBoxChkCategory3(target);
        break;
    case 5:
        slotPairSet(target);
        break;
    case 6:
        dataSkillLearnSet(3, target);
        break;
    case 7:
        battleSlotMarkSet(2, target);
        break;
    case 8:
        dataEtherLearnSet(target, 63);
        break;
    }
    if (flag == 1) {
        dest = itemPickupMessageBuffer;
        digits[2] = count / 10 + '0';
        digits[3] = count % 10 + '0';
        strcpy(buf, text);
        strcat(buf, digits);
        strcpy(dest, buf);
        cursorBufferDeactivateCurrent();
        cursorBufferPush(dest);
    }
    return steps;
}

/* Native "TimeAttack": argument 1 is the kind, argument 2 the value (-1 when omitted).  Kind 0 opens
   the message panel and stores the kind and the value; kind 1 clears the panel and stores the kind
   with value 0; kind 2 stores the kind; kind 3 stores 0 as the kind; kind 4 returns -1 when the stored
   kind is 1, else the stored value.  Kinds 0 to 3 return the value, -1 or the stored value as the
   case reads; any other kind returns -1. */
/* Native "TimeAttack": argument 1 is the kind, argument 2 the value (-1 when omitted).  Kind 0 opens
   the message panel and stores the kind and the value; kind 1 clears the panel and stores the kind
   with value 0; kind 2 stores the kind; kind 3 stores 0 as the kind; kind 4 returns -1 when the stored
   kind is 1, else the stored value.  Kinds 0 to 3 return the value, -1 or the stored value as the
   case reads; any other kind returns -1. */
int runtimeTimeAttack(SbThread *thread)
{
    int kind;
    int value;
    int result;

    result = -1;
    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    kind = vmArgGet(thread, 1);
    if (vmArgTagIsZero(thread, 2) == 0) {
        value = vmArgGet(thread, 2);
    } else {
        value = -1;
    }
    switch (kind) {
    case 0:
        msgPanelOpen();
        gameWork.panelMode = kind;
        gameWork.panelValue = value;
        result = value;
        break;
    case 1:
        result = -1;
        msgPanelClear();
        gameWork.panelMode = kind;
        gameWork.panelValue = 0;
        break;
    case 2:
        gameWork.panelMode = kind;
        result = gameWork.panelValue;
        break;
    case 3:
        gameWork.panelMode = 0;
        result = gameWork.panelValue;
        break;
    case 4:
        if (gameWork.panelMode == 1) {
            result = -1;
        } else {
            result = gameWork.panelValue;
        }
        break;
    }
    return result;
}

/* Native "GetAnalogR": argument 1 selects the mode.  Mode 0 clears padDebugSector and
   padDebugCount and returns 0; mode 4 returns padDebugCount; any other mode returns -1. */
int runtimeGetAnalogR(SbThread *thread)
{
    int result;

    result = -1;
    if (VM_PLUGIN_CNT_GET(thread) <= 0) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    switch (vmArgGet(thread, 1)) {
    case 0:
        padDebugSector = 0;
        padDebugCount = 0;
        result = 0;
        break;
    case 4:
        result = padDebugCount;
        break;
    }
    return result;
}

char runtimeItemEmptyText[8] = { 0 };
char runtimeNativeNameBgm[8] = "bgm";
char runtimeNativeNameNextBgm[8] = "nextBgm";
char runtimeNativeNameBgmFade[8] = "bgmFade";
char runtimeNativeNameBgmse[8] = "bgmse";
char runtimeNativeNameSeVol[8] = "seVol";
char runtimeNativeNameSePos[8] = "sePos";
char runtimeNativeNameSe[8] = "se";
char runtimeNativeNameAdxplay[8] = "adxplay";
char runtimeNativeNameAdxload[8] = "adxload";
char runtimeNativeNameAdx[8] = "adx";
char runtimeNativeNameMpeg2[8] = "mpeg2";
char runtimeNativeNameSystem[8] = "System";
char runtimeNativeNameSetItai[8] = "setItai";
char runtimeNativeNameSetFog[8] = "setFog";
char runtimeNativeNameGetFlag[8] = "getFlag";
char runtimeNativeNameSetFlag[8] = "setFlag";
char runtimeNativeNameFade[8] = "Fade";
char runtimeNativeNameJumpCF[8] = "jumpCF";
char runtimeNativeNameMessage[8] = "message";
char runtimeNativeNameItem[8] = "Item";
