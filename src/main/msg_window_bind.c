#include "common.h"
#include "main/pad.h"
#include "main/msg_balance.h"
#include "main/msg_state.h"
#include "main/msg_window_bind.h"
#include "main/msg_slot_core.h"
#include "main/msg_window_close.h"
#include "main/msg_window_open.h"
#include "main/msg_window_scroll.h"
#include "ov01/data_box.h"


extern MsgWindow msgWindow;
static char *msgItemNames[40];
static int msgItemCost[40];
static int msgShownBalance;
static int msgTargetBalance;
static int msgItemCount[40];
static int msgModeFlag = 0;
extern int msgBalance;

/* Bind the window to its item tables and to the shown balance (msgShownBalance). */
void msgWindowBindTables(void) {
    msgWindowOpenNextSlot(&msgWindow);
    msgWindowBindCurrentItems(&msgWindow, msgItemNames, msgItemCost, msgItemCount, &msgShownBalance);
    msgGameFlagSet();
}

/* Set the mode word that msgSaleUpdate tests before its sound call. */
void msgSetMode(int mode) {
    msgModeFlag = mode;
}

/* Set the name of item `index`. */
void msgSetItemName(int index, char *name) {
    msgItemNames[index] = name;
}

/* Set the cost of item `index`; msgSaleUpdate subtracts it from the total. */
void msgSetItemCost(int index, int value) {
    msgItemCost[index] = value;
}

/* Set the remaining count of item `index`. */
void msgSetItemCount(int index, int value) {
    msgItemCount[index] = value;
}

/* Start the shown value and the total at the value in msgBalance. The department native passes its third script argument, which is not used. */
void msgResetBalances(int unused_script_arg) {
    msgShownBalance = msgBalance;
    msgTargetBalance = msgBalance;
}


/* Per frame: the shown balance rolls down by 100 toward the total (at once while CIRCLE, 0x20, is pressed).
 * UP and DOWN move the selection, CROSS (0x40) resets the window, CIRCLE sells the selected item when its count
 * is above zero: the cost comes off the total (floored at 0) and the count drops by one. Returns the shown value once the purchase window
 * closes (it also becomes msgBalance), else -1. */
int msgSaleUpdate(void)
{
    int item;
    int total;

    if (msgTargetBalance < msgShownBalance) {
        msgShownBalance -= 100;
        if ((PadData[0].pressed & 0x20) || msgShownBalance < msgTargetBalance) {
            msgShownBalance = msgTargetBalance;
        }
        return -1;
    }
    if (msgWindowTestCurrentFlags(&msgWindow, MSG_WINDOW_OPENING | MSG_WINDOW_SCANNING)) {
        return -1;
    }
    if (msgWindowTestCurrentFlags(&msgWindow, 8) == 0 && msgWindowTestCurrentFlags(&msgWindow, 2) != 0) {
        msgGameFlagClearUnlessKept();
        msgBalance = msgShownBalance;
        return msgShownBalance;
    }
    if (msgShownBalance == 0) {
        msgWindowCloseAll(&msgWindow);
        return -1;
    }
    if (PadData[0].pressed & PAD_CROSS) {
        msgWindowCloseAll(&msgWindow);
    } else if (PadData[0].pressed & PAD_CIRCLE) {
        item = msgWindowGetCurrentItemIndex(&msgWindow);
        if (msgItemCount[item] > 0) {
            msgItemCount[item]--;
            total = msgTargetBalance - msgItemCost[item];
            if (total < 0) {
                total = 0;
            }
            msgTargetBalance = total;
            if (msgModeFlag == 0) {
                dataBoxDecCategory1(item + 1);
            } else {
                dataBoxDecCategory2(item + 1);
            }
        }
    }
    if (PadData[0].directionRepeated & PAD_UP) {
        msgWindowMoveSelection(&msgWindow, -1);
    }
    if (PadData[0].directionRepeated & PAD_DOWN) {
        msgWindowMoveSelection(&msgWindow, 1);
    }
    return -1;
}

/* Balance word read and written by other main TUs (msgBalanceGet, msgBalanceSet). */
int msgBalance = 0;
