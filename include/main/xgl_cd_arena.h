#ifndef INCLUDE_MAIN_XGL_CD_ARENA_H
#define INCLUDE_MAIN_XGL_CD_ARENA_H

#include "main/xgl_cd.h"

/* Functions of src/main/xgl_cd_arena.c that other files call. */

/* The CD arena: opened over a caller's buffer, or over memory GameResourceWorkAlloc hands out when the buffer is
 * null (size 0 then takes the default size). Buffers are carved from it in whole 2048-byte CD sectors. */
void xglCdArenaOpen(void *buffer, long long size);
void xglCdArenaClose(void);
/* The arena start address; 0 while no arena is open. */
unsigned char *xglCdArenaGetSize(void);
/* Bump allocation of size bytes at the arena cursor. */
void *xglCdSectorAlloc(int size);
/* Reserves size bytes in the arena as the buffer area xglCdBufferReserve hands out from and rewinds to. */
void xglCdBufferAlloc(long long size);
void *xglCdBufferReserve(long long size);
void *xglCdBufferRewind(void);
void xglCdBufferClear(void);
void xglCdPendingReadsClear(void);
void xglCdWaitIdle(void);
/* Reads a file with xglCdReadDoneCallback as completion callback; the kind/sub pair selects what that does. */
int xglCdReadFileAsync(const char *name, void *buffer, unsigned char kind, unsigned char sub);

/* Play-time clock: the CD clock at the last sample, and the time elapsed since then added to the party play time. */
void xglClockSampleDayTime(void);
void xglClockAccumulate(void);
/* Turns a duration clock (xglClockUInt2DayTime: day, hour, minute, second) into hours (clamped at 99:59:59), minutes
 * and seconds for display: the days count as 24 hours each. */
void PartyTimeDispChange(XglClock *playTime);

/* The 16-bit tick counter xglTickAdvance advances (xgl_tick.c reads and clears it). */
extern int xglTickCounter;
/* Per-frame upkeep: advances the tick counter and runs the vibration engine. */
void xglTickAdvance(void);
/* 1 when CIRCLE (CROSS) is newly pressed and no other button is held. */
long padCirclePressedAlone(void);
int padCrossPressedAlone(void);
/* Mode 1 doubles the tick scale (saving the old one), mode 0 restores it. */
void pxyTickScaleDoubleOverride(int mode);
/* The width of text as xglFontGetStringWidth measures it. */
int xglFontGetTextWidth(const char *text);

/* Callbacks run in order by xglCallbackQueueRun; a null entry empties the queue. */
void xglCallbackQueueAdd(void (*entry)(void));
void xglCallbackQueueRun(void);

#endif /* INCLUDE_MAIN_XGL_CD_ARENA_H */
