#ifndef INCLUDE_MAIN_MSG_WINDOW_BIND_H
#define INCLUDE_MAIN_MSG_WINDOW_BIND_H

/* Functions of src/main/msg_window_bind.c that other files call. */

int msgPurchaseUpdate(void);
void msgWindowBindTables(void);
void msgSetMode(int mode);
void msgSetItemName(int index, char *name);
void msgSetItemCost(int index, int value);
void msgSetItemCount(int index, int value);
void msgResetBalances(int unused_script_arg);

#endif /* INCLUDE_MAIN_MSG_WINDOW_BIND_H */
