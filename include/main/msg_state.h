#ifndef INCLUDE_MAIN_MSG_STATE_H
#define INCLUDE_MAIN_MSG_STATE_H

/* Functions of src/main/msg_state.c that other files call. */

/* Flag bits of a message slot (msgSlotFlags, msgSlotTestCurrent). */
#define MSG_SLOT_NO_CIRCLE 0x1       /* CIRCLE does not advance the window */
#define MSG_SLOT_KEEP_GAME_FLAG 0x4  /* closing the slot leaves the game flag set (msgGameFlagClearUnlessKept) */
#define MSG_SLOT_CROSS_CLOSES 0x40   /* CROSS closes the slot */

/* Flag bits of the message window (msgWindowTestCurrentFlags). */
#define MSG_WINDOW_LOCKED 0x4        /* CROSS cannot close it */
#define MSG_WINDOW_TYPING 0x10       /* the text is still being typed; CIRCLE shows the rest at once */
/* msgWindowBusy's mask: bit 0x2 (not recovered), locked or typing. */
#define MSG_WINDOW_BUSY (0x2 | MSG_WINDOW_LOCKED | MSG_WINDOW_TYPING)

/* Flag bit of the choice object (msgChoiceTestFlags): a choice is open. */
#define MSG_CHOICE_OPEN 0x8

unsigned int msgWindowInUse(void);
void msgChoiceOpen(int frame, int second);
int msgChoicePoll(void);
int msgPollSelect(void);
void msgPanelShow(int first, int second);
void msgResetAll(void);
void msgStateReset(void);
int msgWindowBusy(void);
void msgWindowCloseForTalk(void);
int msgSlotTestCurrent(int mask);
int msgSlotCurrent(void);
void msgGameFlagClearUnlessKept(void);
int msgWindowHasMore(void);
void msgUpdateTimed(void);
void msgClose(void);
void msgPanelSetTimecode(int hours, int minutes, int seconds, int frames);
void msgPanelOpen(void);
void msgPanelClear(void);
void msgPanelShowDefault(void);
void msgBegin(void);
void msgChoiceShowPair(char *name, int count);
void msgSetCurrentText(char *text);
void msgSlotSet(int index, int value);
void msgWindowOpenText(char *text);
void msgWindowSetScriptText(const char *text);
void msgWindowSetSlot(int slot, int value);

#endif /* INCLUDE_MAIN_MSG_STATE_H */
