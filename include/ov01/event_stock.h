#ifndef INCLUDE_OV01_EVENT_STOCK_H
#define INCLUDE_OV01_EVENT_STOCK_H

/* Event drain handlers of src/ov01/event_stock.c (kinds 18 to 20 of eventQueueHandlers). A handler takes the event record
 * the drain passes it; the drain calls the table through an unprototyped pointer. */

#include "ov01/cmd_exec.h"

/* Kind 18: nothing to do, returns 1. */
int eventCombinedEtherBegin(void);
/* Kind 19: clears the second action-stock boost slot and ends the turn of the record's unit when it changed. Returns 1. */
int eventActStockBoostClear(EventQueueEntry *entry);
/* Kind 20: a countdown wait; returns 1 once the count has run out, 0 while it steps down. */
int eventWaitCountdown(EventQueueEntry *entry);

#endif /* INCLUDE_OV01_EVENT_STOCK_H */
