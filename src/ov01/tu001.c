/*
 * OV01 original TU 1: 0x00a801e0..0x00a80368 (5 functions)
 */
#include "common.h"
#include "ov01/tu000.h"
#include "ov01/tu001.h"
#include "ov01/tu091.h"

/* Entries in the 32-record task table ov01TaskTable. */
#define OV01_TASK_COUNT 32

/*
 * One 0x1C-byte task record of the 32-entry table ov01TaskTable. The table holds the tasks that
 * ov01TaskClaim claims for the fades (fadeInStart, unitFadeIn), the screen flash and fill and the message
 * object (msgPutObject). Bit 0x1 of flags is the claim bit (ov01SlotClaim); update runs unless flags & 2
 * (update sweep ov01TaskRunUpdate), draw runs unless flags & 4 (draw sweep ov01TaskRunDraw).
 * A finished task is ended by ov01TaskRelease.
 */
typedef struct TaskRecord {
    int flags;
    void (*update)(struct TaskRecord *);
    void (*draw)(struct TaskRecord *);
    unsigned char unmodeled_0C[0x1C - 0x0C];
} TaskRecord;

static TaskRecord ov01TaskTable[OV01_TASK_COUNT];

/* Clears all 32 task records (ov01TaskClearAll), called by ov01ClearTables. */
void ov01TaskClearAll(void)
{
    int i;

    for (i = 0; i < OV01_TASK_COUNT; i++) {
        ov01SlotClear(&ov01TaskTable[i]);
    }
}

/* Update sweep over the 32 task records (ov01TaskRunUpdate): runs each record's update unless flags & 2. */
void ov01TaskRunUpdate(void)
{
    void (*update)(TaskRecord *);
    TaskRecord *record;
    int i;

    record = ov01TaskTable;
    for (i = 0; i < OV01_TASK_COUNT; i++) {
        update = record->update;
        if (update != 0 && !(record->flags & OV01_SLOT_SKIP_UPDATE)) {
            update(record);
        }
        record++;
    }
}

/* Draw sweep over the 32 task records (ov01TaskRunDraw): runs each record's draw unless flags & 4. */
void ov01TaskRunDraw(void)
{
    void (*draw)(TaskRecord *);
    TaskRecord *record;
    int i;

    record = ov01TaskTable;
    for (i = 0; i < OV01_TASK_COUNT; i++) {
        draw = record->draw;
        if (draw != 0 && !(record->flags & OV01_SLOT_SKIP_DRAW)) {
            draw(record);
        }
        record++;
    }
}

/* Claims the first free task record: returns it, or 0 when all 32 records are claimed. */
void *ov01TaskClaim(void)
{
    int i;

    for (i = 0; i < OV01_TASK_COUNT; i++) {
        if (ov01SlotClaim(&ov01TaskTable[i].flags)) {
            return &ov01TaskTable[i];
        }
    }
    return 0;
}

/* Ends a task (ov01TaskRelease): the fade steps, screenFlashTick, screenFillEnd and msgObjRelease call it. */
void ov01TaskRelease(void *task)
{
    ov01SlotRelease(task);
}
