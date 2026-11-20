#include "common.h"
#include "shared.h"
#include "main/msg_balance.h"
#include "main/game_camera.h"
#include "main/game_data.h"

extern int msgBalance;

/* Stores value in msgBalance, read back by msgBalanceGet. */
void msgBalanceSet(int value)
{
    msgBalance = value;
}

/* Returns msgBalance. */
int msgBalanceGet(void)
{
    return msgBalance;
}

/* Clears CONTROL_SCRIPTED in gameWork.control. */
void msgGameFlagClear(void)
{
    gameWork.control &= ~CONTROL_SCRIPTED;
}

/* Sets CONTROL_SCRIPTED in gameWork.control. */
void msgGameFlagSet(void)
{
    gameWork.control |= CONTROL_SCRIPTED;
}

/* Stores 10000000 (0x989680) in msgBalance. */
void msgBalanceReset(void)
{
    msgBalanceSet(0x989680);
}
