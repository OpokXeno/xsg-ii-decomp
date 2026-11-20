#ifndef INCLUDE_OV01_OV01_TASK_H
#define INCLUDE_OV01_OV01_TASK_H

/* Functions of src/ov01/ov01_task.c that other files call. */

/* The task record of ov01TaskTable as screen tasks (screen_flash.c, screen_fill.c) read it: +0x04 update, +0x08 draw,
 * +0x0C framesLeft (the flash only). The update slot is typed with the record, so the flash tick takes a ScreenTask. */
typedef struct ScreenTask ScreenTask;
struct ScreenTask {
    unsigned char unmodeled_00[4];
    void (*update)(ScreenTask *task);           /* +0x04 */
    void (*draw)(void);                         /* +0x08 */
    int framesLeft;                             /* +0x0C */
};

struct BattleUnit;
struct XenoActor;
struct FadeOwner;

/*
 * Task record of a fade. The fields from +0xC on are the payload of the task record, a union because each kind of
 * task reads it its own way: a unit fade keeps the unit at +0xC (unitFadeTo also a target), a unit slot fade the
 * actor, a monster slot fade (monsSlotFadeInStart/OutStart) its slot owner (screen_flash.c's screen flash keeps a frame
 * counter there, see ScreenTask).
 */
typedef struct FadeTask FadeTask;
struct FadeTask {
    unsigned char unmodeled_00[4];
    void (*update)(FadeTask *task); /* +0x4: the step callback the task runs */
    void (*draw)(void);             /* +0x8: no draw callback for a fade */
    union {
        struct {
            struct BattleUnit *unit;  /* +0xC */
            float progress;           /* +0x10 */
            float step;               /* +0x14 */
            float target;             /* +0x18: unitFadeTo only */
        } unitFade;
        struct {
            struct XenoActor *actor;  /* +0xC */
            float progress;           /* +0x10 */
            float step;               /* +0x14 */
        } slotFade;
        struct {
            struct FadeOwner *owner;  /* +0xC */
            float progress;           /* +0x10: advanced by step each frame */
            float step;               /* +0x14 */
        } monsFade;
    } payload;
};

/* Claims a task record of ov01TaskTable (claim bit of ov01_slot.h). Returned as void *: the fade, screen and message
 * tasks each read the claimed record as their own struct (FadeTask, ScreenTask, ...). */
void *ov01TaskClaim(void);
void ov01TaskClearAll(void);
void ov01TaskRelease(void *task);
void ov01TaskRunDraw(void);
void ov01TaskRunUpdate(void);

#endif /* INCLUDE_OV01_OV01_TASK_H */
