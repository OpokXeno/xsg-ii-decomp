#ifndef INCLUDE_MAIN_XGL_TASK_H
#define INCLUDE_MAIN_XGL_TASK_H

/* The task scheduler of src/main/xgl_task.c: a doubly linked list of active tasks and a free list
 * of task slots. */

typedef struct XglTaskPrefix XglTaskPrefix;
typedef struct XglTaskScheduler XglTaskScheduler;

/*
 * Only the link/callback prefix is evidenced by this allocation.  The
 * complete task records are allocated with a 0x80-byte stride, and callers
 * write task-specific payload beyond this 0x10-byte prefix.  This declaration
 * must not be read as the complete task layout or allocation size.
 */
struct XglTaskPrefix {
    XglTaskScheduler *scheduler;
    XglTaskPrefix *next;
    XglTaskPrefix *previous;
    int (*callback)(XglTaskPrefix *task);
};

struct XglTaskScheduler {
    XglTaskPrefix *free_tasks;
    XglTaskPrefix *active_head;
    XglTaskPrefix *active_tail;
    XglTaskPrefix *next_to_visit;
};

/* Every task slot of the pool reserves this many bytes. */
#define XGL_TASK_SLOT_SIZE 0x80

typedef union XglTaskSlot {
    XglTaskPrefix prefix;
    char reserved[XGL_TASK_SLOT_SIZE];
} XglTaskSlot;

typedef struct XglTaskManager {
    XglTaskScheduler scheduler;
    XglTaskSlot pool[1];
} XglTaskManager;

void *xglTaskInitial(void *manager, int capacity, int reset_gs);
XglTaskPrefix *xglTaskEntryPrev(XglTaskScheduler *scheduler,
                                int (*callback)(XglTaskPrefix *task),
                                XglTaskPrefix *entry);
XglTaskPrefix *xglTaskEntryNext(XglTaskScheduler *scheduler,
                                int (*callback)(XglTaskPrefix *task),
                                XglTaskPrefix *entry);
int xglTaskRemove(XglTaskPrefix *task);
int xglTaskWaitRemove(XglTaskPrefix *task);
void xglTaskExecute(XglTaskScheduler *scheduler);

#endif /* INCLUDE_MAIN_XGL_TASK_H */
