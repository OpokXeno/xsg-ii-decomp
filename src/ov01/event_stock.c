#include "common.h"
#include "ov01/calc.h"
#include "ov01/cmd_exec.h"
#include "ov01/event_stock.h"

int eventStockBegin(void) {
    return 1;
}

/* Kind-19 handler: clears the second action-stock boost slot (actStockClearBoost1) and ends the turn of the unit at
 * data.args.target when it differs from data.args.unit. Always returns 1, so the drain goes on. */
int eventActStockBoostClear(EventQueueEntry *entry) {
    BattleUnit *unit;

    unit = entry->data.args.target;
    actStockClearBoost1();
    if (unit != entry->data.args.unit) {
        calcTurnEnd(unit);
    }
    return 1;
}

/* Kind-20 handler, a countdown wait: returns 1 once data.args.countdown has run out, otherwise steps the count down by
 * one and returns 0, which stops the drain at this record until a later call. */
int eventWaitCountdown(EventQueueEntry *entry) {
    if (entry->data.args.countdown == 0) {
        return 1;
    }
    entry->data.args.countdown = entry->data.args.countdown - 1;
    return 0;
}
