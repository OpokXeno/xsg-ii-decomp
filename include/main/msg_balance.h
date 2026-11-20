#ifndef INCLUDE_MAIN_MSG_BALANCE_H
#define INCLUDE_MAIN_MSG_BALANCE_H

/* Functions of src/main/msg_balance.c that other files call. */

void msgBalanceReset(void);
void msgGameFlagClear(void);
void msgGameFlagSet(void);

void msgBalanceSet(int value);
int msgBalanceGet(void);

#endif /* INCLUDE_MAIN_MSG_BALANCE_H */
