#include "common.h"
#include "shared.h"
#include "main/xgl_cd_arena.h"
#include "main/font_obj.h"
#include "main/menu_scene.h"
#include "main/pt_member.h"
#include "main/bg_layer.h"
#include "main/xgl_cd.h"
#include "main/res_get_path.h"
#include "main/xgl_font.h"
#include "main/pad.h"
#include "main/vibration.h"
#include "main/xgl_render.h"

char *strcpy(char *destination, const char *source);
extern unsigned char xglFontMeasureBuffer[];

/* The CD arena and buffers are handed out in whole 2048-byte CD sectors. */
#define CD_SECTOR_SIZE 0x800
#define CD_SECTOR_MASK (CD_SECTOR_SIZE - 1)

/* xglCdReadDoneCallback event and the packed tag kinds of xglCdReadFileAsync. */
enum CdEvent {
    CD_EVENT_DONE = 4
};

enum CdReadKind {
    CD_READ_FONT_SLOT = 1,
    CD_READ_BG_LAYER = 2,
    CD_READ_MENU_SKILL = 3,
    CD_READ_UMN_IMAGE = 4
};

/* xglFontGetTextWidth(text): copies the text into xglFontMeasureBuffer and returns the width xglFontGetStringWidth
   measures from the buffer start, two bytes before the text. */
int xglFontGetTextWidth(const char *text) {
    strcpy(&xglFontMeasureBuffer[2], text);
    return xglFontGetStringWidth(xglFontMeasureBuffer);
}

extern u8 *xglCdArenaUsed;

/* xglCdSectorAlloc(size): bump allocator over xglCdArenaUsed; returns the old address and advances it by `size`
   rounded up to 2048-byte CD sectors. */
void *xglCdSectorAlloc(int size) {
    u8 *allocAddress;

    allocAddress = xglCdArenaUsed;
    xglCdArenaUsed = (u8 *) (((u32) allocAddress + size + CD_SECTOR_MASK) & ~CD_SECTOR_MASK);
    return allocAddress;
}

/* xglCdArenaOpen(buffer, size): opens the arena over the caller's buffer of `size` bytes; a null buffer takes the
   memory from GameResourceWorkAlloc (the default size when `size` is 0) and sets the owned flag. Despite their
   names, xglCdArenaStart holds the arena start address and xglCdArenaBytes its size in bytes. */
extern u8 *xglCdArenaStart;
extern unsigned char xglCdArenaOwned;
extern long long xglCdArenaBytes;

void xglCdArenaOpen(void *buffer, long long size) {
    u8 *reserved;

    if (buffer != 0) {
        xglCdArenaBytes = size;
        xglCdArenaStart = buffer;
        xglCdArenaOwned = 0;
        xglCdArenaUsed = (u8 *) (((u32) buffer + CD_SECTOR_MASK) & ~CD_SECTOR_MASK);
    } else {
        if (size == 0) {
            size = 0x288000; /* default arena size, meaning not shown; kept as a literal */
        }
        reserved = GameResourceWorkAlloc(size);
        xglCdArenaUsed = (u8 *) (((u32) reserved + CD_SECTOR_MASK) & ~CD_SECTOR_MASK);
        xglCdArenaOwned = 1;
        xglCdArenaBytes = size;
        xglCdArenaStart = reserved;
    }
}

/* xglCdArenaClose: when the arena is open, waits for the CD (xglCdWaitIdle), runs four cleanup calls, and clears
   the arena globals. */
void xglCdArenaClose(void) {
    if (xglCdArenaStart != 0) {
        xglCdWaitIdle();
        menuSkillModelReset();
        fontObjClearRunLists();
        bgLayerResetState();
        if (xglCdArenaOwned != 0) {
            GameResArenaRelease();
        }
    }
    xglCdArenaUsed = 0;
    xglCdArenaStart = 0;
    xglCdArenaOwned = 0;
    xglCdArenaBytes = 0;
}

/* xglCdArenaGetStart: returns xglCdArenaStart, the arena start address (0 while no arena is open). */
/* xglCdArenaGetStart: returns the arena start address (the global xglCdArenaStart), not its size. */
u8 *xglCdArenaGetStart(void) {
    return xglCdArenaStart;
}

/* xglCdBufferAlloc(size): reserves `size` bytes in the arena and points xglCdBufferBase and xglCdBufferCursor at
   them. */
extern u8 *xglCdBufferBase;
extern u8 *xglCdBufferCursor;

void xglCdBufferAlloc(long long size) {
    u8 *base;

    base = xglCdSectorAlloc((int) size);
    xglCdBufferCursor = base;
    xglCdBufferBase = base;
}

/* xglCdBufferReserve(size): reserves `size` bytes at the CD buffer cursor and returns the old cursor; the cursor
   advances to the next 0x800-byte sector boundary. */
void *xglCdBufferReserve(long long size) {
    u8 *cursor = xglCdBufferCursor;

    xglCdBufferCursor = (u8 *) (((u32) cursor + (int) size + CD_SECTOR_MASK) & ~CD_SECTOR_MASK);
    return cursor;
}

/* xglCdBufferRewind: waits for the CD (xglCdWaitIdle), runs two calls, moves the cursor back to the base, and
   returns the base. */
void *xglCdBufferRewind(void) {
    u8 *base;

    xglCdWaitIdle();
    fontObjResetBufferSlots();
    menuSkillModelTailReset();
    base = xglCdBufferBase;
    xglCdBufferCursor = base;
    return base;
}

/* xglCdBufferClear: zeroes xglCdBufferCursor and xglCdBufferBase. */
void xglCdBufferClear(void) {
    xglCdBufferCursor = 0;
    xglCdBufferBase = 0;
}

extern unsigned char pxyTickScaleDoubleSaved;

/* mode 1 saves pxyTickScaleDouble in pxyTickScaleDoubleSaved and sets it to 1 (doubled tick scale); mode 0 restores it from
   pxyTickScaleDoubleSaved. Other modes do nothing. */
void pxyTickScaleDoubleOverride(int mode) {
    unsigned char previousScale;

    if (mode != 0) {
        if (mode == 1) {
            previousScale = pxyTickScaleDouble;
            pxyTickScaleDouble = mode;
            pxyTickScaleDoubleSaved = previousScale;
        }
    } else {
        pxyTickScaleDouble = pxyTickScaleDoubleSaved;
    }
}

extern int xglCdPendingReads;

/* xglCdPendingReadsClear: zeroes xglCdPendingReads. */
void xglCdPendingReadsClear(void) {
    xglCdPendingReads = 0;
}

/* xglCdWaitIdle: loops while the CD reports busy (xglCdServiceIfPending) and while xglCdPendingReads is nonzero. */
void xglCdWaitIdle(void) {
    do {
        while (xglCdServiceIfPending() != 0) {
        }
    } while (xglCdPendingReads != 0);
}

/* xglCdReadDoneCallback(event, value...): completion callback of xglCdReadFileAsync. On CD_EVENT_DONE (4) it
   dispatches the kind/sub byte pair of the packed tag (kind 1: fontObjMarkSlotLoaded, 2: bgLayerAdvance, 3: menuSkillModelReadDone,
   4: menuUmnImageDecode) and then decrements xglCdPendingReads. */

void xglCdReadDoneCallback(int event, int value, int tag) {
    int kind;
    unsigned int sub;
    unsigned int packed_tag = tag;

    if (event < -2) {
        return;
    }
    if (event >= CD_EVENT_DONE) {
        if (event != CD_EVENT_DONE) {
            return;
        }
        kind = (packed_tag >> 8) & 0xFF;
        sub = packed_tag & 0xFF;
        switch (kind) {
        case CD_READ_FONT_SLOT:
            fontObjMarkSlotLoaded(sub);
            break;
        case CD_READ_BG_LAYER:
            bgLayerAdvance(sub);
            break;
        case CD_READ_MENU_SKILL:
            menuSkillModelReadDone(sub);
            break;
        case CD_READ_UMN_IMAGE:
            menuUmnImageDecode(sub);
            break;
        }
        xglCdPendingReads--;
    }
}

int xglCdReadFileAsync(const char *name, void *buffer, unsigned char kind, unsigned char sub) {
    int result;

    result = 0;
    if (name == 0) {
        return result;
    }
    if (buffer == 0) {
        return result;
    }
    xglCdSetReadTag((kind << 8) + sub);
    result = xglCdReadFile(name, buffer, 1, xglCdReadDoneCallback);
    if (result >= 0) {
        xglCdPendingReads++;
    }
    return result;
}

/* Pad predicates on PadData: 1 when the bit 0x20 (CIRCLE) of directionPressed is set, or 0x40 (CROSS) for the
   second, and no bit of mask 0xF00F is set in directionHeld. The two tests are separate ifs: as one && the
   compiler merges the adjacent halfword loads of the 8-aligned PadRecord into one doubleword test. */

long padCirclePressedAlone(void) {
    PadRecord *pad;

    pad = PadData;
    if ((pad->directionHeld & 0xF00F) == 0) {
        if ((pad->directionPressed & PAD_CIRCLE) != 0) {
            return 1;
        }
    }
    return 0;
}

int padCrossPressedAlone(void) {
    PadRecord *pad;

    pad = PadData;
    if ((pad->directionHeld & 0xF00F) == 0) {
        if ((pad->directionPressed & PAD_CROSS) != 0) {
            return 1;
        }
    }
    return 0;
}

extern int xglTickCounter;

/* xglTickAdvance: advances the 16-bit xglTickCounter, then calls VibrationEngine. */
void xglTickAdvance(void) {
    xglTickCounter = (xglTickCounter + 1) & 0xFFFF;
    VibrationEngine();
    do { } while (0);
}

/* PartyTimeDispChange: adds 24 hours per day after the first to playTime->hour and clamps the total at 99:59:59 (hour 0x63). */
void PartyTimeDispChange(XglClock *playTime) {
    unsigned int newHours;
    unsigned int stepHours;

    stepHours = playTime->day * 24 - 24;
    newHours = playTime->hour + stepHours;
    if (newHours >= 0x100U) {
        newHours = 0xFF;
    }
    playTime->hour = (unsigned char) newHours;
    if ((newHours & 0xFF) >= 100U) {
        playTime->hour = 99;
        playTime->minute = 59;
        playTime->second = 59;
    }
}

extern int xglClockLastDayTime;

/* xglClockSampleDayTime: reads the clock and stores its day time in xglClockLastDayTime. */
void xglClockSampleDayTime(void) {
    XglClock clock_time;

    xglClockRead(&clock_time);
    xglClockLastDayTime = xglClockDayTime2UInt(&clock_time);
}

/* xglClockAccumulate: adds the day time elapsed since xglClockLastDayTime to the party play time. */
void xglClockAccumulate(void) {
    XglClock clock_time;
    int newDayTime;
    int elapsed;

    xglClockRead(&clock_time);
    elapsed = xglClockDayTime2UInt(&clock_time) - xglClockLastDayTime;
    newDayTime = xglClockLastDayTime + elapsed;
    ptState.progress->playTime += elapsed;
    xglClockLastDayTime = newDayTime;
}

extern int xglCallbackQueueCount;
extern void (*xglCallbackQueue[4])(void);

/* xglCallbackQueueAdd(entry): appends a nonzero entry to xglCallbackQueue; zero empties the queue. */
void xglCallbackQueueAdd(void (*entry)(void)) {
    if (entry != 0) {
        xglCallbackQueue[xglCallbackQueueCount] = entry;
        xglCallbackQueueCount = xglCallbackQueueCount + 1;
        return;
    }
    xglCallbackQueueCount = 0;
    xglCallbackQueue[0] = 0;
}

/* xglCallbackQueueRun: calls each entry of xglCallbackQueue in order, up to the zero entry, then empties it. */
void xglCallbackQueueRun(void) {
    int i;

    if (xglCallbackQueue[0] != 0) {
        i = 0;
        do {
            xglCallbackQueue[i]();
            i = i + 1;
        } while (xglCallbackQueue[i] != 0);
    }
    xglCallbackQueueAdd(0);
}

/* Two-byte prefix xglFontMeasureBuffer[0..1] followed by the 254-byte text buffer at +2. */
unsigned char xglFontMeasureBuffer[256] = { 0x19, 0x03 };
void (*xglCallbackQueue[4])(void) = { 0, 0, 0, 0 };

u8 *xglCdArenaStart = 0;
u8 *xglCdArenaUsed = 0;
unsigned char xglCdArenaOwned = 0;
long long xglCdArenaBytes = 0LL;
u8 *xglCdBufferBase = 0;
u8 *xglCdBufferCursor = 0;
int xglTickCounter = 0;
unsigned char pxyTickScaleDoubleSaved = 0;
int xglCdPendingReads = 0;
int xglClockLastDayTime = 0;
int xglCallbackQueueCount = 0;
