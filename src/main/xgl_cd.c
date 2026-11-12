#include "common.h"
#include "shared.h"
#include "main/xgl_cd.h"
#include "main/xgl_hdd.h"
#include "main/pad.h"
#include "main/xgl_timer0.h"

struct request {
    u8 mode;
    u8 data;
};

typedef struct {
    unsigned char status;
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned short year;
} XglClock;

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

typedef struct StreamXssBuffer StreamXssBuffer;

typedef struct StreamRing StreamRing;

extern u8 *buffer;
extern u8 *WorkEnd;
void xglClockRead(XglClock *clock);
extern XglTaskPrefix *xglTaskEntryNext(XglTaskScheduler *scheduler,
                                int (*callback)(XglTaskPrefix *task),
                                XglTaskPrefix *entry);

/* Where a file is read from (CdFilePosition.source, CdStreamParam.mode). */
#define CD_SOURCE_DISC 0
#define CD_SOURCE_HOST 1
#define CD_SOURCE_HDD 2

/* A callback gets a third argument: the current read tag from xglCdGetFilePos (lw $6 at
 * 0x19BA04), the request's argument from the read service. */
typedef void CdCompletionCallback(int event, int value, int tag);

int sceCdStSeek(unsigned int lbn);
int sceLseek(int descriptor, int offset, int whence);

typedef struct CdStreamParam CdStreamParam;

struct StreamRing {
    unsigned char *buffer;
    int capacity;
    int write_position;
    int read_position;
};

struct StreamXssBuffer {
    unsigned char *source;
    unsigned char *position;
    unsigned char value;
    unsigned char mode;
    short sectors;
};

struct CdStreamParam {
    /* The XSS interleave state; a stream that has one reads through
     * StreamReadRingCoreXss (xglCdStreamReadRingCore). */
    StreamXssBuffer *xss;       /* +0x00 */
    /* Set once the stream has no bytes left (xglCdStreamRead and the ring
     * readers); cleared by xglCdStreamSeek and xglCdStreamParamInit. */
    unsigned char exhausted;    /* +0x04 */
    /* CD_SOURCE_*: xglCdStreamSeek, xglCdStreamClose, xglCdStreamStat and
     * StreamReadRingCoreSub switch on this byte. */
    unsigned char source;       /* +0x05 */
    short block_count;
    int sector_position;        /* +0x08: advanced by StreamReadRingCoreSub */
    int block_size;             /* +0x0c: xglCdStreamRead pads reads to it */
    int descriptor;             /* +0x10 */
    int file_byte_count;        /* +0x14 */
    int remaining; /* +0x18: bytes not yet transferred into the stream rings. */
    int ring_size;
    StreamRing ring;            /* +0x20 */
    StreamRing alternate_ring;  /* +0x30 */
};

typedef struct CdReadMode {
    unsigned char try_count;
    unsigned char spindle_control;
    unsigned char data_pattern;
} CdReadMode;

/* Only the fields xglCdArcInitSub1 writes are evidenced here. */
typedef struct CdArchiveEntry {
    unsigned char state;
    unsigned char unmodeled_01;
    unsigned char packed;
    unsigned char unmodeled_03;
    unsigned char *destination;
    int lbn;
    int file_lbn_offset;
    int stored_bytes;
    int size_bytes;
} CdArchiveEntry;

unsigned int xglClockDayTime2UInt(XglClock *clock_time);
extern int xglCdReadFilePart(const char *name, void *buffer, int mode,
                             CdCompletionCallback *callback, int offset, int length);

/* Events passed to a completion callback (int event, int value). */
#define CD_EVENT_LOOKUP 0 /* value: the path being looked up */
#define CD_EVENT_SIZE 1
#define CD_EVENT_PROGRESS 2
#define CD_EVENT_DONE 4
#define CD_EVENT_READ_ERROR (-2)

void xglCdPowerOffCB(void);
void xglClockUInt2DayTime(XglClock *clock_time, unsigned int elapsed);
void xglCdStreamReadRingCore(CdStreamParam *stream);
int xglCdStreamReadRing(CdStreamParam *stream, int bytes);
extern int xglHddCheck(void);
extern void xglHddErrorScreen(void);

/* Keep the direct state expression used by both queue loops. */
#define CD_READ_CONTROL (&LW)
/* A CD sector is fixed at 2048 bytes on this drive/read path (see also
 * xglCdStreamReadRing's own `& -2048` sector rounding later in this TU). */
#define CD_SECTOR_BYTES 2048
#define ARC_RECORD_EXTENDED        0x80
#define ARC_RECORD_TYPE_EXTENDED   0x40
#define ARC_RECORD_LENGTH_MASK     0x3f
#define ARC_RECORD_HEADER_BYTES    7
#define ARC_RECORD_TYPE_EXTRA_BYTES 3
#define ARC_TABLE_ALIGN_BYTES      64
#define CD_PACKED_MAGIC 0x585241
#define ARC_ENTRY_UNUSED 0xff
/* CdReadControl.dispatch_state: what the control thread is doing. */
#define CD_STATE_IDLE 0
#define CD_STATE_SYNC_READ 1
#define CD_STATE_ASYNC_READ 2
#define CD_STATE_STREAM 3
/* The request queue is declared as a 32-entry ring, but the asm of the queue
 * helpers (xglCdQueueNext, xglCdServiceRead) masks its indices with 0x7F, so the
 * wrap is not confirmed by them. Left as declared; see the naming notes. */
#define CD_QUEUE_SIZE 32
#define CD_QUEUE_MASK (CD_QUEUE_SIZE - 1)
/* The mask the queue helpers' asm applies to the queue indices. */
#define CD_QUEUE_INDEX_MASK 0x7f
/* Host and HDD files are read in pieces of at most 64 KiB. */
#define CD_FILE_CHUNK_BYTES 0x10000
#define CD_SECTOR_MASK (CD_SECTOR_BYTES - 1)
#define CD_STREAM_FILE_SECTORS 32
#define CD_STREAM_XSS_SECTORS 0x4000
#define CD_CLOCK_READ_INTERVAL 10
#define CD_PATH_MAX 256
/* Single-layer discs: the work space xglCdArcInit skips before the second
 * archive header, and the space it keeps after it (XS1: 0x2D000 and 0x2000). */
#define CD_ARC_LOOKAHEAD_BYTES 0x1A800
#define CD_ARC_HEADER_RESERVE 0x1800
/* Calendar arithmetic of xglClockDayTime2UInt()/xglClockUInt2DayTime(). */
#define CLOCK_BASE_YEAR 2000
#define CLOCK_SECONDS_PER_MINUTE 60
#define CLOCK_SECONDS_PER_HOUR 3600
#define CLOCK_SECONDS_PER_DAY 86400
#define CLOCK_SECONDS_PER_COMMON_YEAR 31536000

/*
 * The queue entries occupy 0x80 bytes; only their used prefix is named.
 * xglCdReadFilePart (0x0019BC98..0x0019C1E0, still INCLUDE_ASM) enqueues by
 * storing its own second argument (buffer) at entry+0x00 and its own first
 * argument (name) at entry+0x14, with the offset and length at +0x04 and +0x08
 * and the completion callback at +0x0C. The queue-next helper xglCdQueueNext
 * passes entry+0x10 to xglCdSetReadTag (xglCdSetReadTag) and entry+0x14 back as
 * the name, in xglCdReadFile's (name, buffer, ...) order from shared.h.
 */
typedef struct CdRequestPartial {
    void *buffer;
    int offset;
    int length;
    CdCompletionCallback *completion_callback;
    int tag;        /* +0x10: handed to xglCdSetReadTag before the read starts */
    char name[0x6c]; /* +0x14 */
} CdRequestPartial;

/*
 * Partial view of LW, limited to fields evidenced by this TU.
 * unmodeled_XX spans are opaque bytes between evidenced fields; naming them
 * `unmodeled_<offset>[<size>]` follows the convention used across the
 * project (e.g. src/main/game.c, src/main/ssd_init.h) instead of inventing
 * members to complete the struct's size.
 */
typedef struct CdReadControl {
    int active_file_descriptor;                 /* +0x00 */
    u8 *write_cursor;                            /* +0x04 */
    int remaining_bytes;                         /* +0x08 */
    int read_poll_mode;                          /* +0x0c */
    /* xglCdQueueNext and xglCdServiceRead call it with callback_argument as
     * a third argument. */
    CdCompletionCallback *completion_callback;  /* +0x10 */
    int callback_argument;                       /* +0x14: the read tag (xglCdReadTag) */
    u8 unmodeled_018[4];                         /* +0x18..0x1b */
    u8 disk_state;                               /* +0x1c */
    u8 disk_flags;                               /* +0x1d */
    u8 unmodeled_01e[2];                         /* +0x1e..0x1f */
    void *extract_source;                         /* +0x20 */
    void *extract_destination;                    /* +0x24 */
    int extract_byte_count;                      /* +0x28 */
    u8 unmodeled_02c[0x4];                       /* +0x2c..0x2f */
    signed char dispatch_state;                  /* +0x30 */
    u8 spindle_control;                          /* +0x31 */
    u8 search_layer;                             /* +0x32 */
    u8 dual_layer_disc;                          /* +0x33 */
    u8 power_off_pending;                        /* +0x34 */
    u8 unmodeled_035[0x9];                       /* +0x35..0x3d */
    u8 queue_cursor;                             /* +0x3e */
    u8 queue_tail;                               /* +0x3f */
    CdRequestPartial requests[32];               /* +0x40..0x103f */
} CdReadControl;

/* Four 0x18-byte archive slots; the initialization helper models each prefix. */
typedef union CdArchiveStorage {
    CdArchiveEntry entry;
    unsigned char bytes[0x18];
} CdArchiveStorage;

extern const char xenosagaPfs0Path[];
extern const char xenosagaArc00Name[];
extern int sceCdStStat(void);
extern int sceOpen(const char *path, int flags, ...);
extern int sceSifLoadModule(const char *path, int args_length, const char *args);
extern int sceCdReadClock(unsigned char *clock);
int sceLseek(int descriptor, int offset, int whence);
extern int sceCdSync(int mode);
extern int sceCdDiskReady(int mode);
extern int sceCdStatus(void);
extern int sceCdGetDiskType(void);
extern int sceCdTrayReq(int mode, int *tray_changed);
extern int sceCdRead(int lbn, int sectors, void *destination, CdReadMode *read_mode);
void xglCdArcInit(void);
static int BCD2INT(unsigned char bcd);
void extract(u32 *destination, u32 *source, int byte_count);
extern int sceCdStRead(int sectors, void *buffer, int mode, int *result);
extern int sceRead(int descriptor, void *buffer, int bytes);
extern u32 sceCdGetReadPos(void);
extern int sceCdGetError(void);
extern int sceCdLayerSearchFile(CdFilePosition *file, const char *name, int layer);
extern const char xenosagaArc10Name[16];
extern const char xenosagaArc20Name[16];
extern const char xenosagaArc30Name[16];
extern const char xglCdSystemCnfName[];
extern const char xenosagaPfs0Path[24];
extern const char xenosagaArc00Name[16];
/* Twelve month lengths followed by twelve cumulative month offsets. */
extern unsigned short monthday[24];
extern char head_8[13];
extern char root_0[6];
extern char hdd_head_6[7];
extern char arcname_1[12];
extern char arcname_2[12];
extern char arcname_3[12];
extern char vcd_head_5[24];
extern char tail_9[7];
extern CdCompletionCallback *callback;
extern CdReadControl LW;
extern CdArchiveStorage ArcHeader[4];
extern signed char ReadClockInterval;
extern unsigned char PresentTime[8];
void xglCdDefaultCallback(int event, int value, int tag);
void xglCdSetReadTag(int value);
int xglCdGetFileSize(const char *path);
int xglCdStreamStat(CdStreamParam *stream);
void StreamReadRingCoreNormal(CdStreamParam *stream);
void StreamReadRingCoreXss(CdStreamParam *stream);
int sceCdInit(int mode);
int sceCdMmode(int media);
void *sceSifAllocIopHeap(unsigned int size);
int sceCdStInit(unsigned int buffer_max, unsigned int bank_max, unsigned int iop_buffer);
int sceCdReadDvdDualInfo(int *on_dual, unsigned int *layer1_start);
int sceSifLoadElf(const char *name, void *exec_data);
void FlushCache(int operation);
void *memset(void *destination, int value, unsigned int size);
void xglCdReset(void);
int xglHddMount(void);
/* The overlay loaded last, and the path pieces around "OVnn": "cdrom0:\\" and ".OVL;1". */
extern unsigned char loaded_overlay;
extern const char head_10[];
extern const char tail_11[8];
extern int xglCdDeviceHandle;
/* The alternate CD device the handle selects: stop at once, or after the
 * current transfer (xglCdReadCancel). */
int func_0017AAF0(int handle);
int func_0017AA00(int handle);
int sceCdBreak(void);
int sceCdPause(void);
void xglCdDummyCallback(int event, int value, int tag);
int xglCdGetFilePosSub(CdArchiveEntry *entry, const char *path);
CdFilePosition *xglCdGetFilePos(CdFilePosition *position, const char *name,
                                       CdCompletionCallback *callback);
int xglCdReadFilePart(const char *name, void *buffer, int mode,
                      CdCompletionCallback *notify, int offset, int length);

void hdd_error(void)
{
    int *descriptor = &LW.active_file_descriptor;

    sceClose(*descriptor);
    *descriptor = -1;
    xglHddErrorScreen();
}

void xglCdPowerOffCB(void)
{
    LW.power_off_pending = 1;
}

void extract(u32 *destination, u32 *source, int byte_count)
{
    if (source[0] == CD_PACKED_MAGIC && source[3] == 0) {
        do {
            xglArxExtract(destination, source);
        } while (0);
    } else if (destination != source) {
        while (byte_count > 0) {
            *destination++ = *source++;
            byte_count -= 4;
        }
    }
}

/* Queue-next helper (proposed xglCdQueueNext): reports CD_EVENT_DONE with the
 * pending count, starts the request at queue_cursor and advances the cursor. */
int xglCdQueueNext(void)
{
    CdRequestPartial *request;

    CD_READ_CONTROL->completion_callback(CD_EVENT_DONE,
                                         (CD_READ_CONTROL->queue_tail
                                          - CD_READ_CONTROL->queue_cursor) & CD_QUEUE_INDEX_MASK,
                                         CD_READ_CONTROL->callback_argument);
    CD_READ_CONTROL->dispatch_state = CD_STATE_IDLE;
    if (CD_READ_CONTROL->queue_tail != CD_READ_CONTROL->queue_cursor) {
        request = &CD_READ_CONTROL->requests[CD_READ_CONTROL->queue_cursor];
        xglCdSetReadTag(request->tag);
        xglCdReadFilePart(request->name, request->buffer, 1,
                          request->completion_callback, request->offset,
                          request->length);
        CD_READ_CONTROL->queue_cursor = (CD_READ_CONTROL->queue_cursor + 1) & CD_QUEUE_INDEX_MASK;
        return 0;
    }
    return 1;
}

/* completion_callback event of xglCdServiceRead: value is the read position (bytes read for a
 * device-handle read, the drive's read position, or the file offset). */
#define CD_EVENT_PROGRESS_ASYNC 3
/* func_0017AF40 result for a finished device-handle read. */
#define CD_DEVICE_READ_DONE 3

extern unsigned char xglCdServiceActive;
int func_0017AF10(int handle);
int func_0017AF40(int handle);
void func_0017D150(void);

/* One step of the active asynchronous read. A disc read reports its position and, once the drive
 * (or the device handle, when one is open) is done, unpacks the data and starts the next queued
 * request. A host or HDD read transfers up to CD_FILE_CHUNK_BYTES, and after the last chunk pads
 * the sector, closes the file, unpacks and starts the next request. The progress callback is
 * skipped while xglCdServiceActive is set; the function sets it on return. */
void xglCdServiceRead(void)
{
    int requested_bytes;
    int transferred;

    switch (LW.read_poll_mode) {
    case CD_SOURCE_DISC:
        if (xglCdDeviceHandle != 0) {
            if (xglCdServiceActive == 0) {
                LW.completion_callback(CD_EVENT_PROGRESS_ASYNC, func_0017AF10(xglCdDeviceHandle) << 11,
                                       LW.callback_argument);
            }
            if (func_0017AF40(xglCdDeviceHandle) == CD_DEVICE_READ_DONE) {
                extract(LW.extract_destination, LW.extract_source, LW.extract_byte_count);
                xglCdQueueNext();
            }
            func_0017D150();
        } else {
            if (xglCdServiceActive == 0) {
                LW.completion_callback(CD_EVENT_PROGRESS_ASYNC, sceCdGetReadPos(), LW.callback_argument);
            }
            if (sceCdSync(1) != 0) {
                break;
            }
            extract(LW.extract_destination, LW.extract_source, LW.extract_byte_count);
            if (xglCdQueueNext() != 0) {
                sceCdPause();
            }
        }
        break;
    case CD_SOURCE_HOST:
    case CD_SOURCE_HDD:
        requested_bytes = LW.remaining_bytes;
        if (requested_bytes > CD_FILE_CHUNK_BYTES) {
            requested_bytes = CD_FILE_CHUNK_BYTES;
        }
        transferred = sceRead(LW.active_file_descriptor, LW.write_cursor, requested_bytes);
        if (transferred < 0) {
            hdd_error();
            LW.queue_cursor = (LW.queue_cursor + CD_QUEUE_INDEX_MASK) & CD_QUEUE_INDEX_MASK;
            break;
        }
        LW.write_cursor += transferred;
        LW.remaining_bytes -= transferred;
        if (xglCdServiceActive == 0) {
            LW.completion_callback(CD_EVENT_PROGRESS_ASYNC, sceLseek(LW.active_file_descriptor, 0, 1),
                                   LW.callback_argument);
        }
        if (LW.remaining_bytes > 0) {
            break;
        }
        {
            int partial_sector = transferred & CD_SECTOR_MASK;

            if (partial_sector != 0) {
                int remaining_padding = (-partial_sector) & CD_SECTOR_MASK;

                while (remaining_padding > 0) {
                    *LW.write_cursor++ = (unsigned char) remaining_padding;
                    remaining_padding--;
                }
            }
        }
        sceClose(LW.active_file_descriptor);
        LW.active_file_descriptor = -1;
        extract(LW.extract_destination, LW.extract_source, LW.extract_byte_count);
        xglCdQueueNext();
        break;
    }
    xglCdServiceActive = 1;
}

int func_0017A2E8(unsigned char *table, int mode);
extern unsigned char D_0068DEC8[];

/* Clears the busy byte xglCdServiceActive and stores the device handle from the table
 * at D_0068DEC8 in xglCdDeviceHandle (proposed xglCdOpenDevice). */
void xglCdOpenDevice(void)
{
    xglCdServiceActive = 0;
    xglCdDeviceHandle = func_0017A2E8(D_0068DEC8, 0);
}

/* The stream service state at 0x959CC0. xglCdControlThread gives each open stream of the list a
 * turn: turn_countdown runs down from turn_period, then `current` moves on to the next open
 * stream. The list at +0x10 is the symbol xglCdStreamList. */
#define CD_STREAM_LIST_SIZE 2

typedef struct CdStreamService {
    unsigned short turn_countdown;  /* +0x00 */
    unsigned short turn_period;     /* +0x02 */
    unsigned char current;          /* +0x04 */
    unsigned char unmodeled_05[0xb];
    CdStreamParam *list[CD_STREAM_LIST_SIZE]; /* +0x10 */
} CdStreamService;

#define CD_STREAM_TURN_PERIOD 0x40
/* sceDevctl commands of the power-off sequence: close all files of a pfs mount, switch dev9 off. */
#define HDD_DEVCTL_CLOSE_ALL 0x5003
#define HDD_DEVCTL_DEV9_OFF 0x4402

extern CdStreamService xglCdStreamService;
extern const char xglCdPfs0Device[];
extern const char xglCdPfs1Device[];
extern const char xglCdDev9Device[];
extern int sceDevctl(const char *device, int command, const void *input,
                     unsigned int input_size, void *output, unsigned int output_size);
int sceCdPowerOff(int *result);
int xglCdStreamSeek(CdStreamParam *stream, int sector);

/* One tick of the CD service: counts the clock interval down, runs the power-off sequence when it
 * is pending, then serves the current state: an asynchronous read, or the streams, each for
 * turn_period ticks before the next open one is sought to its position. */
void xglCdControlThread(void)
{
    int power_status;
    CdStreamParam *stream;
    unsigned char previous;

    if (ReadClockInterval > 0) {
        ReadClockInterval--;
    }
    if (LW.power_off_pending != 0) {
        sceDevctl(xglCdPfs0Device, HDD_DEVCTL_CLOSE_ALL, 0, 0, 0, 0);
        sceDevctl(xglCdPfs1Device, HDD_DEVCTL_CLOSE_ALL, 0, 0, 0, 0);
        sceDevctl(xglCdDev9Device, HDD_DEVCTL_DEV9_OFF, 0, 0, 0, 0);
        sceCdPowerOff(&power_status);
    }
    switch (LW.dispatch_state) {
    case CD_STATE_ASYNC_READ:
        xglCdServiceRead();
        break;
    case CD_STATE_STREAM:
        stream = xglCdStreamService.list[xglCdStreamService.current];
        if (stream != 0) {
            xglCdStreamReadRingCore(stream);
        } else {
            xglCdStreamService.turn_countdown = 0;
        }
        if (xglCdStreamService.turn_countdown != 0) {
            xglCdStreamService.turn_countdown--;
            break;
        }
        previous = xglCdStreamService.current;
        xglCdStreamService.turn_countdown = xglCdStreamService.turn_period;
        do {
            xglCdStreamService.current++;
            if (xglCdStreamService.current >= CD_STREAM_LIST_SIZE) {
                xglCdStreamService.current = 0;
                break;
            }
        } while (xglCdStreamService.list[xglCdStreamService.current] == 0);
        if (xglCdStreamService.current != previous) {
            stream = xglCdStreamService.list[xglCdStreamService.current];
            if (stream != 0) {
                xglCdStreamSeek(stream, stream->sector_position);
            }
        }
        break;
    case CD_STATE_IDLE:
    case CD_STATE_SYNC_READ:
        break;
    default:
        LW.dispatch_state = CD_STATE_IDLE;
        break;
    }
    xglCdServiceActive = 0;
}

/* One BCD byte: high nibble tens, low nibble units. */
static int BCD2INT(unsigned char bcd)
{
    return (bcd >> 4) * 10 + (bcd & 15);
}

void xglClockRead(XglClock *clock)
{
    if (ReadClockInterval == 0) {
        ReadClockInterval = CD_CLOCK_READ_INTERVAL;
        sceCdReadClock(PresentTime);
    }

    clock->year = BCD2INT(PresentTime[7]) + CLOCK_BASE_YEAR;
    clock->month = BCD2INT(PresentTime[6]);
    clock->day = BCD2INT(PresentTime[5]);
    clock->hour = BCD2INT(PresentTime[3]);
    clock->minute = BCD2INT(PresentTime[2]);
    clock->second = BCD2INT(PresentTime[1]);
    clock->status = 0;
}

/*
 * Converts a calendar record to the number of seconds elapsed since
 * 2000-01-01 00:00:00.  monthday[12..23] holds the cumulative day offset of
 * each month; the leap day of the current year counts from March onwards,
 * and ((year - 1997) >> 2) counts the leap days of the whole years before it.
 */
unsigned int xglClockDayTime2UInt(XglClock *clock_time)
{
    unsigned int seconds;
    unsigned int year;
    unsigned char month;

    month = clock_time->month;
    seconds = (monthday[month + 11] + clock_time->day - 1) * CLOCK_SECONDS_PER_DAY
        + clock_time->hour * CLOCK_SECONDS_PER_HOUR + clock_time->minute * CLOCK_SECONDS_PER_MINUTE
        + clock_time->second;
    year = clock_time->year;
    if (((year - CLOCK_BASE_YEAR) & 3) == 0 && month >= 3) {
        seconds += CLOCK_SECONDS_PER_DAY;
    }
    return seconds
        + ((year - CLOCK_BASE_YEAR) * CLOCK_SECONDS_PER_COMMON_YEAR
           + ((year - 1997) >> 2) * CLOCK_SECONDS_PER_DAY);
}

void xglClockUInt2DayTime(XglClock *clock_time, unsigned int elapsed)
{
    XglClock *clock;
    unsigned int year_length = 366;
    unsigned int year = 0;
    unsigned int common_year_length = 365;
    unsigned int leap_year_length = 366;
    unsigned int days;
    unsigned int remainder;
    unsigned int month;
    unsigned int month_length;

    clock = clock_time;
    days = elapsed / CLOCK_SECONDS_PER_DAY;
    remainder = elapsed % CLOCK_SECONDS_PER_DAY;
    clock->hour = remainder / CLOCK_SECONDS_PER_HOUR;
    remainder -= clock->hour * CLOCK_SECONDS_PER_HOUR;
    clock->minute = remainder / CLOCK_SECONDS_PER_MINUTE;
    remainder -= clock->minute * CLOCK_SECONDS_PER_MINUTE;
    clock->second = remainder;

    while (days >= year_length) {
        year += 1;
        days -= year_length;
        year_length = (year & 3) == 0 ? leap_year_length : common_year_length;
    }
    clock->year = year + CLOCK_BASE_YEAR;

    for (month = 0; ; month += 1) {
        month_length = monthday[month];
        if (month == 1 && (clock->year & 3) == 0)
            month_length += 1;
        if (days < month_length)
            break;
        days -= month_length;
    }
    clock->month = month + 1;
    clock->day = days + 1;
}

/* sceCdStatus, sceCdDiskReady, sceCdGetDiskType and sceCdTrayReq values tested below. */
#define CD_STATUS_SHELL_OPEN 1
#define CD_DISK_READY_COMPLETE 2
#define CD_DISK_READY_NOT_READY 6
#define CD_DISC_NONE 0
/* Types below this value mean the drive is still detecting the disc. */
#define CD_DISC_DETECT_END 5
#define CD_DISC_PS2_DVD 20
#define CD_TRAY_CHECK 2
/* LW.disk_state. */
#define CD_DISK_STATE_UNKNOWN 0
#define CD_DISK_STATE_IDENTIFIED 1
#define CD_DISK_STATE_TRAY_CHANGED 2
/* LW.disk_flags: which archives the disc holds. */
#define CD_DISC_FLAG_ARC10 1
#define CD_DISC_FLAG_ARC20 2
#define CD_DISC_FLAGS_DUAL_LAYER 0x7f
/* xglCdIdentifyDisc results below zero. */
#define CD_CHECK_UNSUPPORTED (-1)
#define CD_CHECK_SHELL_OPEN (-2)
#define CD_CHECK_NOT_READY (-3)

/* Disc identification (proposed xglCdIdentifyDisc): returns the disc's archive flags, 0 with no
 * disc, or a CD_CHECK_* value. A disc not identified yet must be ready and a PS2 DVD; its flags
 * come from the sizes of the xenosaga.10 and xenosaga.20 archives, and system.cnf must name an
 * executable whose third letter is 'U'. An identified disc only has its tray and shell checked.
 * `result` holds the sceCdDiskReady() and then the sceCdGetDiskType() value. */
int xglCdIdentifyDisc(void)
{
    int tray_changed;
    unsigned char *text;
    unsigned char state;
    int result;
    int size;

    if (LW.dual_layer_disc != 0) {
        LW.disk_state = CD_DISK_STATE_IDENTIFIED;
        LW.disk_flags = CD_DISC_FLAGS_DUAL_LAYER;
    }
    state = LW.disk_state;
    if (state != CD_DISK_STATE_IDENTIFIED) {
        int status;

        status = sceCdStatus();
        if (status == CD_STATUS_SHELL_OPEN) {
            LW.disk_state = CD_DISK_STATE_UNKNOWN;
            return CD_CHECK_SHELL_OPEN;
        }
        result = sceCdDiskReady(1);
        if (result != CD_DISK_READY_COMPLETE) {
            if (LW.disk_state == CD_DISK_STATE_TRAY_CHANGED
                || (status == 0 && result == CD_DISK_READY_NOT_READY)) {
                return CD_CHECK_SHELL_OPEN;
            }
            LW.disk_state = CD_DISK_STATE_UNKNOWN;
            return CD_CHECK_NOT_READY;
        }
        LW.disk_state = CD_DISK_STATE_UNKNOWN;
        result = sceCdGetDiskType();
        if (result == CD_DISC_NONE) {
            return 0;
        }
        if (result != CD_DISC_PS2_DVD) {
            if (result >= CD_DISC_DETECT_END) {
                return CD_CHECK_UNSUPPORTED;
            }
            return CD_CHECK_NOT_READY;
        }
        LW.disk_flags = 0;
        size = xglCdGetFileSize(xenosagaArc10Name);
        if (size > 0 && size <= CD_ARC_HEADER_RESERVE) {
            LW.disk_flags |= CD_DISC_FLAG_ARC10;
        }
        size = xglCdGetFileSize(xenosagaArc20Name);
        if (size > 0 && size <= CD_ARC_HEADER_RESERVE) {
            LW.disk_flags |= CD_DISC_FLAG_ARC20;
        }
        if (LW.disk_flags == 0) {
            return CD_CHECK_UNSUPPORTED;
        }
        text = WorkEnd;
        while (xglCdServiceIfPending() != 0) {
        }
        xglCdReadFile(xglCdSystemCnfName, text, 0, 1);
        while (*text++ != '\\') {
        }
        if (text[2] != 'U') {
            return CD_CHECK_UNSUPPORTED;
        }
        LW.disk_state = CD_DISK_STATE_IDENTIFIED;
    } else {
        if (sceCdTrayReq(CD_TRAY_CHECK, &tray_changed) == 0 || tray_changed != 0) {
            LW.disk_state = CD_DISK_STATE_TRAY_CHANGED;
            return CD_CHECK_SHELL_OPEN;
        }
        if (sceCdStatus() == state) {
            LW.disk_state = CD_DISK_STATE_UNKNOWN;
            return CD_CHECK_SHELL_OPEN;
        }
    }
    return LW.disk_flags;
}

void xglCdDefaultCallback(int event, int value, int tag)
{
}

void xglCdDummyCallback(int event, int value, int tag)
{
}

/* Resets the request queue, the stream service and the callback, then pauses the drive and waits
 * for it. */
void xglCdReset(void)
{
    int i;

    LW.queue_cursor = 0;
    LW.queue_tail = 0;
    xglCdStreamService.turn_period = CD_STREAM_TURN_PERIOD;
    xglCdStreamService.turn_countdown = CD_STREAM_TURN_PERIOD;
    xglCdStreamService.current = 0;
    for (i = 0; i < CD_STREAM_LIST_SIZE; i++) {
        xglCdStreamService.list[i] = 0;
    }
    LW.dispatch_state = 0;
    callback = xglCdDefaultCallback;
    sceCdPause();
    sceCdSync(0);
}

CdCompletionCallback *xglCdSetCallback(int callback_address)
{
    CdCompletionCallback *old_callback = callback;

    switch (callback_address) {
    case 0:
        callback = xglCdDefaultCallback;
        break;
    case 1:
        callback = xglCdDummyCallback;
        break;
    case -1:
        break;
    default:
        callback = (CdCompletionCallback *)callback_address;
        break;
    }
    return old_callback;
}

extern int xglCdReadTag;

/* Sets the current read tag xglCdReadTag (proposed xglCdSetReadTag). */
void xglCdSetReadTag(int value)
{
    xglCdReadTag = value;
}

/* Expands one name record of an archive table into `name` and returns the name's end. A record starts with
 * a count byte. Zero: the bytes after it, up to a terminator, replace the name. Otherwise the low
 * nibble is the number of bytes copied over the name, starting (high nibble + 1) characters
 * before its end; the rest of the name stays. */
char *xglCdArcNameExpand(char *name, const unsigned char *record)
{
    unsigned char count;
    int back;

    count = *record++;
    if (count == 0) {
        while ((*name++ = *record++) != 0) {
        }
    } else {
        back = (count >> 4) + 1;
        count &= 0xF;
        while (*name != 0) {
            name++;
        }
        name -= back;
        while (count != 0) {
            *name++ = *record++;
            count--;
        }
        name--;
        while (*(unsigned char *) name++ != 0) {
        }
    }
    return name - 1;
}

/* Where a name stands in an archive's directory tree: its depth and the length of its directory
 * part. Both halves are compared as one word. */
typedef union ArcNameKey {
    struct {
        short depth;
        short length;
    } part;
    int both;
} ArcNameKey;

#define ARC_DIRECTORY_DEPTH_MAX 16
#define ARC_RECORD_POSITION_BYTES 7
#define ARC_RECORD_SIZE_BYTES 3

/* Walks the archive record table of one entry for a backslash-separated path. Directory records
 * rebuild the current directory name; a file record is only expanded and compared when it stands
 * at the path's depth with a directory part of the path's length. On a match the entry gets the
 * file's position and sizes and the result is 1; 0 when the table ends. */
int xglCdGetFilePosSub(CdArchiveEntry *entry, const char *path)
{
    char name[CD_PATH_MAX];
    char *directory[ARC_DIRECTORY_DEPTH_MAX];
    ArcNameKey current;
    ArcNameKey target;
    const unsigned char *record;
    const char *cursor;
    const char *left;
    const char *right;
    char *end;
    int flags;
    int packed;
    int length;
    int path_length;

    current.both = 0;
    target.both = 0;
    record = entry->destination + 1;
    directory[0] = name;
    name[0] = 0;
    for (cursor = path; *cursor != 0; cursor++) {
        if (*cursor == '\\') {
            target.part.depth++;
            target.part.length = cursor - path;
        }
    }
    path_length = cursor - path;
    while ((flags = *record) != 0) {
        packed = flags & ARC_RECORD_TYPE_EXTENDED;
        length = flags & ARC_RECORD_LENGTH_MASK;
        if ((flags & ARC_RECORD_EXTENDED) != 0) {
            if (record[1] != 0) {
                current.part.depth -= record[1];
                directory[current.part.depth + 1][-1] = 0;
            }
            end = xglCdArcNameExpand(directory[current.part.depth], record + 2);
            record += length;
            end[0] = '\\';
            end[1] = 0;
            current.part.depth++;
            current.part.length = end - name;
            directory[current.part.depth] = end + 1;
        } else {
            if (current.both == target.both) {
                end = xglCdArcNameExpand(directory[current.part.depth], record + 1);
                record += length;
                if (end - name == path_length) {
                    left = path;
                    right = name;
                    while (*left == *right) {
                        if (*left == 0) {
                            entry->file_lbn_offset = (record[2] << 16) + (record[1] << 8) + record[0];
                            entry->stored_bytes = (record[6] << 24) + (record[5] << 16) + (record[4] << 8) + record[3];
                            if ((entry->packed = packed) != 0) {
                                entry->size_bytes = (record[9] << 16) + (record[8] << 8) + record[7];
                            } else {
                                entry->size_bytes = entry->stored_bytes;
                            }
                            return 1;
                        }
                        left++;
                        right++;
                    }
                }
            } else {
                record += length;
            }
            record += ARC_RECORD_POSITION_BYTES;
            if (packed != 0) {
                record += ARC_RECORD_SIZE_BYTES;
            }
        }
    }
    return 0;
}

/* Debug output for a path that starts with "data\": the prefix is dropped. */
extern const char xglCdReplacePathFormat[];
extern const char xglCdReplacedPathFormat[];
int printf(const char *format, ...);

#define CD_DATA_PREFIX_LENGTH 5
#define CD_OPEN_READ 1
/* sceLseek moves at most this far in one call. */
#define CD_SEEK_STEP 0x40000000

/* Locates a file: in the archives first (the path then becomes the archive's own file name), else
 * by its name on the disc, the host or the HDD. Fills the position record (and opens and seeks
 * the descriptor of a host or HDD file) and returns it, or 0 when the file is missing. The
 * callback gets CD_EVENT_LOOKUP with the path and the current read tag. */
CdFilePosition *xglCdGetFilePos(CdFilePosition *position, const char *name, CdCompletionCallback *callback)
{
    char buffer[CD_PATH_MAX];
    const char *path = name;
    CdArchiveEntry *entry;
    const char *cursor;
    char *destination;
    const char *absolute_source;
    char *absolute_destination;
    char *scan;
    int descriptor;
    long offset;
    int size;
    int source;

    if (path[0] == 'd' && path[1] == 'a' && path[2] == 't' && path[3] == 'a' && path[4] == '\\') {
        printf(xglCdReplacePathFormat, name);
        name = path + CD_DATA_PREFIX_LENGTH;
        printf(xglCdReplacedPathFormat, name);
    }
    position->source = 0;
    if (name[0] != '\\') {
        /* the separators are rewritten in the caller's string */
        for (scan = (char *) name; *scan != 0; scan++) {
            if (*scan == '/') {
                *scan = '\\';
            }
        }
    }
    do {
        entry = &ArcHeader[0].entry;
        if (entry->state != ARC_ENTRY_UNUSED && xglCdGetFilePosSub(entry, name) != 0) {
            name = arcname_1;
            position->source = entry->state;
            break;
        }
        entry = &ArcHeader[1].entry;
        if (entry->state != ARC_ENTRY_UNUSED && xglCdGetFilePosSub(entry, name) != 0) {
            name = arcname_2;
            position->source = entry->state;
        } else {
            entry++;
            if (entry->state != ARC_ENTRY_UNUSED && xglCdGetFilePosSub(entry, name) != 0) {
                name = arcname_3;
                position->source = entry->state;
            } else {
                entry = 0;
            }
        }
    } while (0);
    if (name[0] == '\\') {
        absolute_source = name + 1;
        absolute_destination = buffer;
        while ((*absolute_destination = *absolute_source) != 0) {
            absolute_source++;
            absolute_destination++;
        }
        position->source = CD_SOURCE_HOST;
    } else {
        destination = buffer;
        source = position->source;
        switch (source) {
        case CD_SOURCE_DISC:
            buffer[0] = '\\';
            destination = buffer + 1;
            for (cursor = name; *cursor != 0; cursor++, destination++) {
                if (*cursor < 'a' || *cursor > 'z') {
                    *destination = *cursor;
                } else {
                    *destination = *cursor - ('a' - 'A');
                }
            }
            destination[0] = ';';
            destination[1] = '1';
            destination[2] = 0;
            break;
        case CD_SOURCE_HOST:
        case CD_SOURCE_HDD:
            if (position->source == CD_SOURCE_HOST) {
                cursor = vcd_head_5;
            } else {
                cursor = hdd_head_6;
            }
            while ((*destination = *cursor) != 0) {
                cursor++;
                destination++;
            }
            cursor = name;
            for (;;) {
                if (*cursor == '\\') {
                    *destination = '/';
                } else {
                    *destination = *cursor;
                }
                if (*cursor == 0) {
                    break;
                }
                cursor++;
                destination++;
            }
            break;
        }
    }
    callback(CD_EVENT_LOOKUP, (int) path, xglCdReadTag);
    switch (position->source) {
    case CD_SOURCE_DISC:
        if (entry != 0) {
            position->lbn = entry->lbn + entry->file_lbn_offset;
            position->stored_bytes = entry->stored_bytes;
            position->packed = entry->packed;
            position->size_bytes = entry->size_bytes;
        } else {
            sceCdSync(0);
            sceCdDiskReady(0);
            if (sceCdLayerSearchFile(position, buffer, LW.search_layer) == 0) {
                return 0;
            }
            position->packed = 1;
            position->size_bytes = position->stored_bytes;
        }
        break;
    case CD_SOURCE_HDD:
        if (xglHddCheck() < 0) {
            return xglCdGetFilePos(position, path, callback);
        }
        /* fall through */
    case CD_SOURCE_HOST:
        descriptor = sceOpen(buffer, CD_OPEN_READ);
        if (descriptor < 0) {
            return 0;
        }
        if (entry != 0) {
            position->lbn = entry->lbn + entry->file_lbn_offset;
            position->stored_bytes = entry->stored_bytes;
            position->packed = entry->packed;
            position->size_bytes = entry->size_bytes;
        } else {
            position->lbn = 0;
            position->packed = 0;
            size = sceLseek(descriptor, 0, 2);
            position->stored_bytes = size;
            if (position->packed == 0) {
                position->size_bytes = size;
            }
        }
        offset = (long) position->lbn << 11;
        sceLseek(descriptor, 0, 0);
        while (offset > CD_SEEK_STEP) {
            sceLseek(descriptor, CD_SEEK_STEP, 1);
            offset -= CD_SEEK_STEP;
        }
        sceLseek(descriptor, offset, 1);
        position->descriptor = descriptor;
        break;
    }
    return position;
}

/* Probes a path: -1 when missing, closes the descriptor of a host or HDD file
 * and returns 0 otherwise. */
int xglCdCheckFile(const char *path, CdFilePosition *position)
{
    if (xglCdGetFilePos(position, path, xglCdDummyCallback) == 0) {
        return -1;
    }
    switch (position->source) {
    case CD_SOURCE_DISC:
        break;
    case CD_SOURCE_HOST:
    case CD_SOURCE_HDD:
        sceClose(position->descriptor);
        break;
    }
    return 0;
}

/* Size of a path in bytes, or -1 when it is missing. */
int xglCdGetFileSize(const char *path)
{
    CdFilePosition position;

    if (xglCdCheckFile(path, &position) != 0) {
        return -1;
    }
    return position.size_bytes;
}

int func_0017ACE0(int handle, unsigned int lbn, int mode);
int func_0017A9C0(int handle, int sectors, void *buffer);

int xglCdReadFilePart(const char *name, void *buffer, int mode,
                      CdCompletionCallback *notify, int offset, int length)
{
    CdFilePosition position;
    CdReadMode read_mode;
    CdRequestPartial *request;
    const char *source;
    char *destination;
    int asynchronous;
    int result; /* sceRead byte count, or sceCdGetError status */
    int padding;
    int partial;
    /* The lookup fills the record through this pointer; the packed test reads it back the same way. */
    CdFilePosition *file = &position;

    switch ((int)notify) {
    case 0:
        notify = callback;
        break;
    case 1:
        notify = xglCdDummyCallback;
        break;
    case 2:
        notify = xglCdDefaultCallback;
        break;
    }
    asynchronous = mode & 1;
    if (asynchronous != 0) {
        if (CD_READ_CONTROL->dispatch_state != CD_STATE_IDLE) {
            if (CD_READ_CONTROL->dispatch_state != CD_STATE_STREAM) {
                request = &CD_READ_CONTROL->requests[CD_READ_CONTROL->queue_tail];
                source = name;
                destination = request->name;
                while ((*destination++ = *source++) != 0)
                    ;
                request->buffer = buffer;
                request->offset = offset;
                request->length = length;
                request->completion_callback = notify;
                request->tag = xglCdReadTag;
                CD_READ_CONTROL->queue_tail = (CD_READ_CONTROL->queue_tail + 1) & CD_QUEUE_INDEX_MASK;
                return 0;
            }
        }
    }
    if (CD_READ_CONTROL->dispatch_state != CD_STATE_IDLE && mode != 4)
        return -3;
    if (xglCdGetFilePos(file, name, notify) == 0)
        return -1;
    position.lbn += offset >> 11;
    if (length > 0)
        position.stored_bytes = length;
    notify(CD_EVENT_SIZE, (position.stored_bytes + CD_SECTOR_BYTES - 1) & -CD_SECTOR_BYTES, xglCdReadTag);
    CD_READ_CONTROL->completion_callback = notify;
    CD_READ_CONTROL->callback_argument = xglCdReadTag;
    CD_READ_CONTROL->extract_byte_count = (position.stored_bytes + CD_SECTOR_BYTES - 1) & -CD_SECTOR_BYTES;
    CD_READ_CONTROL->extract_destination = buffer;
    if (file->packed == 0 || (mode & 2) != 0)
        CD_READ_CONTROL->extract_source = buffer;
    else
        CD_READ_CONTROL->extract_source = (u8 *)buffer
            + ((position.size_bytes + CD_SECTOR_BYTES - 1) & -CD_SECTOR_BYTES)
            - CD_READ_CONTROL->extract_byte_count;
    CD_READ_CONTROL->read_poll_mode = position.source;
    switch (position.source) {
    case CD_SOURCE_DISC:
    retry_cd:
        if (xglCdDeviceHandle != 0) {
            func_0017ACE0(xglCdDeviceHandle, position.lbn, 0);
            func_0017A9C0(xglCdDeviceHandle, CD_READ_CONTROL->extract_byte_count / CD_SECTOR_BYTES,
                          CD_READ_CONTROL->extract_source);
        } else {
            sceCdSync(0);
            sceCdDiskReady(0);
            read_mode.try_count = 0;
            read_mode.spindle_control = CD_READ_CONTROL->spindle_control;
            read_mode.data_pattern = 0;
            sceCdRead(position.lbn, CD_READ_CONTROL->extract_byte_count / CD_SECTOR_BYTES,
                      CD_READ_CONTROL->extract_source, &read_mode);
        }
        if (asynchronous != 0) {
            CD_READ_CONTROL->dispatch_state = CD_STATE_ASYNC_READ;
            return position.size_bytes;
        }
        CD_READ_CONTROL->dispatch_state = CD_STATE_SYNC_READ;
        if (xglCdDeviceHandle != 0) {
            while (func_0017AF40(xglCdDeviceHandle) != CD_DEVICE_READ_DONE) {
                notify(CD_EVENT_PROGRESS, func_0017AF10(xglCdDeviceHandle) << 11, xglCdReadTag);
                func_0017D150();
            }
        } else {
            while (sceCdSync(1) > 0)
                notify(CD_EVENT_PROGRESS, sceCdGetReadPos(), xglCdReadTag);
        }
        if (xglCdDeviceHandle != 0) {
            if (func_0017AF40(xglCdDeviceHandle) == 4) {
                notify(CD_EVENT_READ_ERROR, 4, xglCdReadTag);
                goto retry_cd;
            }
        } else {
            result = sceCdGetError();
            if ((unsigned int)result >= 2) {
                notify(CD_EVENT_READ_ERROR, result, xglCdReadTag);
                goto retry_cd;
            }
        }
        break;
    case CD_SOURCE_HOST:
    case CD_SOURCE_HDD:
        CD_READ_CONTROL->active_file_descriptor = position.descriptor;
        CD_READ_CONTROL->write_cursor = CD_READ_CONTROL->extract_source;
        CD_READ_CONTROL->remaining_bytes = position.stored_bytes;
        if (asynchronous != 0) {
            CD_READ_CONTROL->dispatch_state = CD_STATE_ASYNC_READ;
            return position.size_bytes;
        }
        CD_READ_CONTROL->dispatch_state = CD_STATE_SYNC_READ;
        /* Host/HDD files are read synchronously in chunks of at most 64 KiB;
         * a failed read reports the HDD error and restarts the request. */
        do {
            if (CD_READ_CONTROL->remaining_bytes > CD_FILE_CHUNK_BYTES - 1)
                result = sceRead(CD_READ_CONTROL->active_file_descriptor,
                                 CD_READ_CONTROL->write_cursor, CD_FILE_CHUNK_BYTES);
            else
                result = sceRead(CD_READ_CONTROL->active_file_descriptor,
                                 CD_READ_CONTROL->write_cursor,
                                 CD_READ_CONTROL->remaining_bytes);
            if (result < 0) {
                hdd_error();
                CD_READ_CONTROL->dispatch_state = CD_STATE_IDLE;
                return xglCdReadFilePart(name, buffer, mode, notify, offset, length);
            }
            CD_READ_CONTROL->write_cursor += result;
            CD_READ_CONTROL->remaining_bytes -= result;
            notify(CD_EVENT_PROGRESS, position.stored_bytes - CD_READ_CONTROL->remaining_bytes, xglCdReadTag);
        } while (CD_READ_CONTROL->remaining_bytes > 0);
        partial = result & CD_SECTOR_MASK;
        if (partial != 0) {
            padding = -partial & CD_SECTOR_MASK;
            while (padding > 0) {
                *CD_READ_CONTROL->write_cursor++ = padding;
                padding--;
            }
        }
        sceClose(CD_READ_CONTROL->active_file_descriptor);
        CD_READ_CONTROL->active_file_descriptor = -1;
        break;
    }
    if ((mode & 2) == 0)
        extract(CD_READ_CONTROL->extract_destination, CD_READ_CONTROL->extract_source,
                CD_READ_CONTROL->extract_byte_count);
    notify(CD_EVENT_DONE, 0, xglCdReadTag);
    CD_READ_CONTROL->dispatch_state = CD_STATE_IDLE;
    return (position.size_bytes + CD_SECTOR_BYTES - 1) & -CD_SECTOR_BYTES;
}

int xglCdReadFile(const char *name, void *buffer, int mode, int flags)
{
    return xglCdReadFilePart(name, buffer, mode, (CdCompletionCallback *)flags, 0, -1);
}

/* Cancels the active read: clears pending requests, breaks or pauses a sync
 * read, closes an open descriptor and goes idle (proposed xglCdReadCancel). */
void xglCdReadCancel(int immediate)
{
    LW.queue_tail = LW.queue_cursor;
    if (LW.dispatch_state == CD_STATE_SYNC_READ) {
        switch (LW.read_poll_mode) {
        case CD_SOURCE_DISC:
            if (xglCdDeviceHandle != 0) {
                if (immediate) {
                    func_0017AAF0(xglCdDeviceHandle);
                } else {
                    func_0017AA00(xglCdDeviceHandle);
                }
            } else {
                sceCdBreak();
                sceCdPause();
            }
            break;
        case CD_SOURCE_HOST:
        case CD_SOURCE_HDD:
            LW.remaining_bytes = 0;
            break;
        }
    } else if (LW.dispatch_state == CD_STATE_ASYNC_READ) {
        switch (LW.read_poll_mode) {
        case CD_SOURCE_DISC:
            if (xglCdDeviceHandle != 0) {
                if (immediate) {
                    func_0017AAF0(xglCdDeviceHandle);
                } else {
                    func_0017AA00(xglCdDeviceHandle);
                }
            } else {
                sceCdBreak();
                sceCdPause();
            }
            break;
        case CD_SOURCE_HOST:
        case CD_SOURCE_HDD:
            if (LW.active_file_descriptor >= 0) {
                sceClose(LW.active_file_descriptor);
                LW.active_file_descriptor = -1;
            }
            break;
        }
    }
    LW.dispatch_state = CD_STATE_IDLE;
}

extern unsigned char xglCdStreamsActive[];

/* Services the stream list when xglCdStreamsActive is set (proposed xglCdServiceIfPending). */
int xglCdServiceIfPending(void)
{
    if ((signed char) xglCdStreamsActive[0] == 0) {
        return 0;
    }
    xglCdServiceRead();
    return 1;
}

/* Returns the stream-service flag xglCdStreamsActive (proposed xglCdGetServicePending). */
signed char xglCdGetServicePending(void)
{
    return (signed char) xglCdStreamsActive[0];
}

int func_0017AF40(int);

/* Nonzero when the CD side has finished its last request (proposed xglCdSyncDone). */
int xglCdSyncDone(void)
{
    if (xglCdDeviceHandle != 0) {
        return func_0017AF40(xglCdDeviceHandle) != 1;
    }
    return sceCdSync(1) == 0;
}

int sceCdStStart(unsigned int lbn, CdReadMode *read_mode);

/* Opens a stream on a file: looks the path up, fills the stream's source, sizes and position and
 * puts the stream into the service list when it has a ring buffer. The first disc stream starts
 * the drive's streaming; a further one is sought to its start. Returns -1 when the file is
 * missing. */
int xglCdStreamOpen(CdStreamParam *stream, const char *path)
{
    CdFilePosition position;
    CdReadMode read_mode;
    CdStreamParam **slot;
    int index;
    int source;
    int starting;

    starting = 0;
    if (LW.dispatch_state != CD_STATE_STREAM) {
        starting = 1;
    }
    LW.dispatch_state = CD_STATE_STREAM;
    if (xglCdGetFilePos(&position, path, xglCdDummyCallback) == 0) {
        LW.dispatch_state = CD_STATE_IDLE;
        return -1;
    }
    stream->source = position.source;
    stream->file_byte_count = position.stored_bytes;
    stream->remaining = position.stored_bytes;
    stream->block_size = CD_SECTOR_BYTES;
    stream->exhausted = 0;
    stream->sector_position = 0;
    source = position.source;
    switch (source) {
    case CD_SOURCE_DISC:
        stream->descriptor = position.lbn;
        if (starting) {
            sceCdSync(0);
            sceCdDiskReady(0);
            read_mode.try_count = 0;
            read_mode.spindle_control = 0;
            read_mode.data_pattern = 0;
            sceCdStStart(stream->descriptor, &read_mode);
        } else {
            xglCdStreamSeek(stream, 0);
        }
        break;
    case CD_SOURCE_HOST:
    case CD_SOURCE_HDD:
        stream->descriptor = position.descriptor;
        break;
    }
    if (stream->ring.buffer != 0) {
        stream->ring.read_position = 0;
        stream->ring.write_position = 0;
        stream->alternate_ring.read_position = 0;
        stream->alternate_ring.write_position = 0;
        for (slot = xglCdStreamService.list, index = 0; index < CD_STREAM_LIST_SIZE; index++, slot++) {
            if (*slot == 0 && *slot != stream) {
                *slot = stream;
                break;
            }
        }
    }
    return 0;
}

/* Reads whole sectors from the stream's source; returns the bytes read and
 * advances the sector position by them, rounded up. */
int StreamReadRingCoreSub(CdStreamParam *stream, unsigned char *buffer, int sectors)
{
    int read_error;
    int result = 0;

    switch (stream->source) {
    case CD_SOURCE_DISC:
        result = sceCdStRead(sectors, buffer, 1, &read_error) << 11;
        break;
    case CD_SOURCE_HOST:
    case CD_SOURCE_HDD:
        result = sceRead(stream->descriptor, buffer, sectors << 11);
        break;
    }
    stream->sector_position += (result + CD_SECTOR_MASK) >> 11;
    return result;
}

/* Stream status: sceCdStStat for a disc stream, 32 sectors (CD_STREAM_FILE_SECTORS)
 * for a host or HDD stream. */
int xglCdStreamStat(CdStreamParam *stream)
{
    int source;
    int result;

    result = 0;
    source = stream->source;
    switch (source) {
    case CD_SOURCE_DISC:
        result = sceCdStStat();
        break;
    case CD_SOURCE_HOST:
    case CD_SOURCE_HDD:
        result = CD_STREAM_FILE_SECTORS;
        break;
    }
    return result;
}

/* Fills the stream ring from the source: up to the read position, wrapping
 * once, limited by the sectors xglCdStreamStat says are ready. At the end of
 * the file the last block is zero-padded and the stream marked exhausted. */
void StreamReadRingCoreNormal(CdStreamParam *stream)
{
    /* Sectors to read: [0] up to the first boundary, [1] after the wrap. */
    int sectors[2];
    int available;
    int bytes;
    int tail;
    int remaining;
    u8 *cursor;

    if (stream->remaining > 0) {
        if (stream->ring.write_position <= stream->ring.read_position) {
            sectors[1] = 0;
            sectors[0] = (stream->ring.read_position - stream->ring.write_position) / CD_SECTOR_BYTES;
        } else {
            sectors[0] = (stream->ring.capacity - stream->ring.write_position) / CD_SECTOR_BYTES;
            sectors[1] = stream->ring.read_position / CD_SECTOR_BYTES;
        }
        available = xglCdStreamStat(stream);
        if (available < sectors[0]) {
            sectors[0] = available;
            sectors[1] = 0;
        } else if (available - sectors[0] < sectors[1]) {
            sectors[1] = available - sectors[0];
        }
        if (sectors[0] > 0) {
            cursor = stream->ring.buffer + stream->ring.write_position;
            bytes = StreamReadRingCoreSub(stream, cursor, sectors[0]);
            if (sectors[1] > 0) {
                cursor = stream->ring.buffer;
                bytes = StreamReadRingCoreSub(stream, cursor, sectors[1]);
            }
            remaining = stream->remaining - ((sectors[0] + sectors[1]) << 11);
            if (remaining <= 0) {
                int block_size = stream->block_size;

                stream->exhausted = 1;
                tail = bytes & (block_size - 1);
                remaining = 0;
                if (tail > 0) {
                    cursor += bytes;
                    if (tail < block_size) {
                        do {
                            *cursor = 0;
                            tail++;
                            cursor++;
                        } while (tail < stream->block_size);
                    }
                }
            }
            stream->remaining = remaining;
            stream->ring.write_position =
                (stream->ring.write_position + ((sectors[0] + sectors[1]) << 11))
                % stream->ring.capacity;
        }
    }
}

/* Like StreamReadRingCoreNormal for an XSS stream: one sector at a time, each
 * routed to ring or alternate_ring by the next bit of the interleave map, a
 * map sector being read whenever the current one is used up. */
void StreamReadRingCoreXss(CdStreamParam *stream)
{
    StreamXssBuffer *xss;
    StreamRing *ring;
    int available;

    if (stream->remaining > 0) {
        xss = stream->xss;
        available = xglCdStreamStat(stream);
        while (available > 0 && stream->remaining > 0) {
            if (xss->sectors <= 0) {
                StreamReadRingCoreSub(stream, xss->source, 1);
                xss->position = xss->source;
                xss->value = 0;
                xss->mode = 0;
                xss->sectors = CD_STREAM_XSS_SECTORS;
            } else {
                if (xss->mode == 0) {
                    xss->value = *xss->position;
                    xss->position += 1;
                    xss->mode = 8;
                }
                ring = ((xss->value >> (8 - xss->mode)) & 1) == 0
                    ? &stream->ring : &stream->alternate_ring;
                if (ring->write_position == ring->read_position) {
                    break;
                }
                xss->mode--;
                xss->sectors--;
                StreamReadRingCoreSub(stream, ring->buffer + ring->write_position, 1);
                ring->write_position = (ring->write_position + CD_SECTOR_BYTES) % ring->capacity;
            }
            available--;
            stream->remaining -= CD_SECTOR_BYTES;
        }
        if (stream->remaining <= 0) {
            stream->remaining = 0;
            stream->exhausted = 1;
        }
    }
}

void xglCdStreamReadRingCore(CdStreamParam *stream)
{
    if (stream->xss != 0) {
        StreamReadRingCoreXss(stream);
        return;
    }
    StreamReadRingCoreNormal(stream);
}

int xglCdStreamReadRing(CdStreamParam *stream, int bytes)
{
    int aligned;

    aligned = (bytes + CD_SECTOR_BYTES - 1) & -CD_SECTOR_BYTES;
    stream->ring.read_position =
        (stream->ring.read_position + aligned) % stream->ring.capacity;
    do {
        xglCdStreamReadRingCore(stream);
    } while (stream->remaining != 0 &&
             stream->ring.write_position != stream->ring.read_position);
    return 0;
}


/*
 * Reads byte_count bytes (in whole 2048-byte sectors, rounded down) into
 * buffer, then zero-fills the tail of the last partial block so the read ends
 * on a block boundary. Returns the bytes covered, padding included.
 */
int xglCdStreamRead(CdStreamParam *stream, unsigned char *buffer, int byte_count)
{
    int transferred;
    int remaining;
    int block_size;
    int tail;

    transferred = StreamReadRingCoreSub(stream, buffer, byte_count >> 11);
    remaining = stream->remaining - transferred;
    if (remaining <= 0)
    {
        remaining = 0;
        stream->exhausted = 1;
    }
    block_size = stream->block_size;
    tail = transferred & (block_size - 1);
    stream->remaining = remaining;
    if (tail > 0)
    {
        buffer = &buffer[transferred];
        transferred += block_size - tail;
        if (tail < block_size)
        {
            do
            {
                *buffer = 0;
                tail += 1;
                buffer += 1;
            } while (tail < stream->block_size);
        }
    }
    return transferred;
}

/* Seeks a stream to a sector: clears the exhausted byte, sets remaining from the
 * stored size and seeks the disc or host/HDD descriptor (proposed xglCdStreamSeek). */
int xglCdStreamSeek(CdStreamParam *stream, int sector)
{
    int offset = sector * CD_SECTOR_BYTES;

    stream->exhausted = 0;
    stream->remaining = stream->file_byte_count - offset;
    switch (stream->source) {
    case CD_SOURCE_DISC:
        sceCdStSeek(stream->descriptor + sector);
        break;
    case CD_SOURCE_HOST:
    case CD_SOURCE_HDD:
        sceLseek(stream->descriptor, offset, 0);
        break;
    }
    return 0;
}

/* Seeks a stream to its first sector (proposed xglCdStreamRewind). */
void xglCdStreamRewind(CdStreamParam *stream)
{
    xglCdStreamSeek(stream, 0);
}

/* Closes a stream and drops it from the two-entry list xglCdStreamList (proposed
 * xglCdStreamClose). */
/* The open streams, two entries. */

/* Unsized: the original addresses it absolutely, not through $gp. */
int sceCdStStop(void);

int xglCdStreamClose(CdStreamParam *stream)
{
    CdStreamParam **slot = xglCdStreamService.list;
    int last = 1;
    int index;

    for (index = 0; index < CD_STREAM_LIST_SIZE; index++, slot++) {
        if (*slot == stream) {
            *slot = 0;
        } else if (*slot != 0) {
            last = 0;
        }
    }

    if (stream->descriptor == -1) {
        return 0;
    }

    switch (stream->source) {
    case CD_SOURCE_DISC:
        if (last) {
            sceCdStStop();
        }
        stream->descriptor = -1;
        break;
    case CD_SOURCE_HOST:
    case CD_SOURCE_HDD:
        sceClose(stream->descriptor);
        break;
    }
    if (last) {
        xglCdStreamsActive[0] = 0;
    }
    return 0;
}

/* Resets a stream record to idle: descriptor -1, buffers cleared (proposed
 * xglCdStreamParamInit). */
void xglCdStreamParamInit(CdStreamParam *stream)
{
    stream->alternate_ring.buffer = 0;
    stream->descriptor = -1;
    stream->exhausted = 0;
    do {
        stream->xss = 0;
    } while (0);
    stream->ring.buffer = 0;
}

/* The debug file-select state (XS1 layout; xglCdFileSelectListReload reads
 * directory, list and list_size). */
typedef struct CdFileSelect {
    int x;                                         /* +0x000 */
    int y;                                         /* +0x004 */
    int rows;                                      /* +0x008 */
    int state;                                     /* +0x00c */
    char directory[0x100];                         /* +0x010 */
    char *list;                                    /* +0x110 */
    unsigned int list_size;                        /* +0x114 */
    void (*print)(int x, int y, const char *format, ...); /* +0x118 */
    char path[0x100];                              /* +0x11c */
    unsigned short entry_count;                    /* +0x21c */
    short first_row;                               /* +0x21e */
    short cursor;                                  /* +0x220 */
    unsigned char unmodeled_222[2];                /* +0x222 */
    int list_index;                                /* +0x224 */
    char *list_position;                           /* +0x228 */
    unsigned char unmodeled_22c[0x268 - 0x22c];    /* +0x22c */
} CdFileSelect;

/* ".filelist", the name list file read from the selected directory. */
extern const char listname_7[];
int xglDebFileRead(const char *name, void *buffer);

/* Reloads the .filelist name list of the selected directory through
 * xglDebFileRead and turns its lines into NUL-terminated names. */
void xglCdFileSelectListReload(CdFileSelect *select)
{
    char path[CD_PATH_MAX];
    unsigned int index;
    char *list;
    char *line;
    char *destination;
    const char *source;
    unsigned char character;

    list = select->list;
    for (index = 0; index < select->list_size; index++) {
        *list++ = 0;
    }
    destination = path;
    for (source = select->directory; *source != 0; source++, destination++) {
        *destination = *source;
    }
    for (source = listname_7; *source != 0; source++, destination++) {
        *destination = *source;
    }
    *destination = 0;
    xglDebFileRead(path, select->list);
    line = select->list;
    character = *line;
    if (character != 0) {
        do {
            if (character == '\n') {
                *line = 0;
            }
            line++;
            character = *line;
        } while (character != 0);
    }
}

/* The file-select state machine: state 0 (and any unknown state) reloads the .filelist and clears
 * the path; state 1 looks up the path's "<path>:" section in the list (".:" for the root), points
 * list_position at its names and counts them (plus the "..", "." rows inside a directory), then
 * moves to state 2 (idle); state 3 drops the last path component and goes back to state 1. */
void xglCdFileSelectSub(CdFileSelect *select)
{
    char prefix[CD_PATH_MAX];
    unsigned char *src;
    char *dst;
    char *name;
    char *probe;
    char *part;
    char *cursor;
    char *last;
    unsigned char character;
    int mismatch;

    switch (select->state) {
    case 0:
    default:
        xglCdFileSelectListReload(select);
        select->path[0] = 0;
        select->list_index = 0;
        select->list_position = select->list;
        select->entry_count = 1;
        select->first_row = 0;
        select->state = 1;
        /* fall through */
    case 1:
        src = (unsigned char *)select->path;
        dst = prefix;
        if (select->path[0] == 0) {
            prefix[0] = '.';
            prefix[1] = ':';
            prefix[2] = 0;
        } else {
            do {
                *dst++ = *src;
                src++;
            } while (*src != 0);
            *dst = 0;
            dst[-1] = ':';
        }
        name = select->list;
        while (*name != 0) {
            mismatch = 0;
            for (part = prefix; *part != 0; part++) {
                if (*name == 0) {
                    mismatch = 1;
                    break;
                }
                if (*name != *part) {
                    mismatch = 1;
                    break;
                }
                name++;
            }
            if (!mismatch) {
                select->list_position = name + 1;
                break;
            }
            {
                unsigned char character;

                while ((character = *name++) != 0) {
                    do {
                        character = *name++;
                    } while (character != 0);
                }
            }
        }
        select->entry_count = 0;
        probe = select->list_position;
        if (*probe != 0) {
            probe++;
            for (;;) {
                character = *probe++;
                if (character != 0) {
                    continue;
                }
                select->entry_count++;
                character = *probe;
                if (character == 0) {
                    break;
                }
            }
        }
        if (select->path[0] != 0) {
            select->entry_count += 2;
        }
        select->state = 2;
        break;
    case 2:
        return;
    case 3:
        last = select->path;
        for (cursor = last; *cursor != 0; cursor++) {
            if (*cursor == '/') {
                if (cursor[1] == 0) {
                    *last = 0;
                    break;
                }
                last = cursor + 1;
            }
        }
        *last = 0;
        select->state = 1;
        break;
    }
}

extern PadRecord PadData[2];
extern const char fileSelectPathFormat[];
extern const char fileSelectUpArrowText[];
extern const char fileSelectCursorText[];
/* Rows 0 and 1 of a subdirectory listing: "..", "." */
extern const char *fileSelectParentNames[2];
extern const char fileSelectDownArrowText[];

/* Pad-driven debug file select. Up/down move the cursor (wrapping only on a fresh press), left/right
 * move it by a page; triangle resets the state to 0 and cross cancels (returns 2). Circle on a
 * directory appends it to the path (state 1), on a file appends it and returns 1 (state 3); on ".."
 * it leaves (state 3), on "." it returns 3 (state 2). Inside a directory rows 0 and 1 are "..", ".".
 * Then prints the path and the visible rows with the cursor and the scroll arrows. */
int xglCdFileSelect(CdFileSelect *select)
{
    int index;
    int skipped;
    int end_row;
    int count;
    int x;
    int y;
    unsigned char *list;
    char *last;
    char *path;
    char *tail;
    unsigned char *entry;
    unsigned short buttons;

    xglCdFileSelectSub(select);
    buttons = PadData[0].pressed;
    if ((buttons & PAD_TRIANGLE) != 0) {
        select->state = 0;
    }
    if ((buttons & PAD_CROSS) != 0) {
        return 2;
    }
    if (PadData[0].repeated & PAD_LEFT) {
        select->cursor -= select->rows;
    }
    if (PadData[0].repeated & PAD_UP) {
        select->cursor--;
    }
    if (select->cursor < 0) {
        if (PadData[0].pressed & PAD_UP) {
            select->cursor = select->entry_count - 1;
        }
        else {
            select->cursor = 0;
        }
    }
    if (select->cursor - select->first_row <= 0) {
        select->first_row = select->cursor - 1;
    }
    if (select->first_row < 0) {
        select->first_row = 0;
    }
    if (PadData[0].repeated & PAD_RIGHT) {
        select->cursor += select->rows;
    }
    if (PadData[0].repeated & PAD_DOWN) {
        select->cursor++;
    }
    if (select->entry_count - 1 < select->cursor) {
        if (PadData[0].pressed & PAD_DOWN) {
            select->cursor = 0;
        }
        else {
            select->cursor = select->entry_count - 1;
        }
    }
    if (select->cursor - select->first_row > select->rows - 2) {
        select->first_row = select->cursor - select->rows + 2;
    }
    if ((unsigned int)(select->entry_count - select->rows) < (unsigned int)select->first_row) {
        select->first_row = select->entry_count - select->rows;
    }
    if (PadData[0].pressed & PAD_CIRCLE) {
        entry = (unsigned char *)select->list_position;
        if (select->path[0] != 0) {
            if (select->cursor == 1) {
                select->cursor = 0;
                select->state = 2;
                select->first_row = 0;
                return 3;
            }
            select->cursor -= 2;
        }
        if (select->cursor < 0) {
            path = select->path;
            select->cursor = 0;
            select->state = 3;
            select->first_row = 0;
        } else {
            for (skipped = 0; skipped < select->cursor; skipped++) {
                while (*entry++ != 0) {
                    ;
                }
            }
            last = (char *)entry;
            while (last[1] != 0) {
                last++;
            }
            path = select->path;
            tail = path;
            while (*tail != 0) {
                tail++;
            }
            do {
                *tail++ = *entry;
            } while (*entry++ != 0);
            if (*last == '/') {
                select->cursor = 0;
                select->state = 1;
                select->first_row = 0;
            } else {
                select->cursor = 0;
                select->state = 3;
                select->first_row = 0;
                return 1;
            }
        }
    } else {
        path = select->path;
    }
    x = select->x;
    y = select->y;
    list = (unsigned char *)select->list_position;
    select->print(x, y, fileSelectPathFormat, path);
    end_row = select->rows;
    count = select->entry_count;
    if (count < end_row) {
        end_row = count;
    }
    end_row += select->first_row;
    index = 0;
    if (*path != 0) {
        index = 2;
    }
    y += 12;
    for (; index < select->first_row; index++) {
        while (*list++ != 0) {
            ;
        }
    }
    if (select->first_row > 0) {
        select->print(x, y, fileSelectUpArrowText);
    }
    for (index = select->first_row; index < end_row; index++) {
        if (index == select->cursor) {
            select->print(x + 8, y, fileSelectCursorText);
        }
        if (index < 2 && select->path[0] != 0) {
            select->print(x + 16, y, fileSelectParentNames[index]);
        } else {
            select->print(x + 16, y, (char *)list);
            while (*list++ != 0) {
                ;
            }
            if (*list == 0) {
                return 0;
            }
        }
        y += 8;
    }
    if (*list != 0) {
        select->print(x, y - 8, fileSelectDownArrowText);
    }
    return 0;
}

void xglCdSifLoadModule(const char *path, const char *args)
{
    char name[CD_PATH_MAX];
    const char *src;
    char *dst;
    int args_length;

    src = head_8;
    dst = name;
    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
    for (src = path; *src != 0; src++, dst++) {
        if (*src < 'a')
            *dst = *src;
        else
            *dst = *src - ('a' - 'A');
    }
    src = tail_9;
    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
    args_length = 0;
    if (args != 0) {
        const char *scan = args;

        args_length = 1;
        if (scan[0] != 0 || scan[1] != 0) {
            for (;;) {
                scan++;
                args_length++;
                if (*scan == 0 && scan[1] == 0)
                    break;
            }
        }
    }
    sceSifLoadModule(name, args_length, args);
}

/* Loads an overlay ELF from cdrom0 with its .OVL;1 name, after clearing and
 * flushing caches (proposed xglCdLoadOverlay). */
/* EE memory the overlays load into, cleared before each load. */
#define CD_OVERLAY_ADDRESS 0xa80000
#define CD_OVERLAY_SIZE 0x80000

void xglCdLoadOverlay(int overlay)
{
    char name[CD_PATH_MAX];
    char exec_data[16];
    const char *src;
    char *dst;

    if (overlay != loaded_overlay) {
        loaded_overlay = overlay;
        src = head_10;
        dst = name;
        while ((*dst = *src) != 0) {
            src++;
            dst++;
        }
        dst[0] = 'O';
        dst[1] = 'V';
        dst[2] = overlay / 10 + '0';
        dst[3] = overlay % 10 + '0';
        dst += 4;
        src = tail_11;
        while ((*dst = *src) != 0) {
            src++;
            dst++;
        }
        memset((void *)CD_OVERLAY_ADDRESS, 0, CD_OVERLAY_SIZE);
        FlushCache(0);
        FlushCache(2);
        while (sceSifLoadElf(name, exec_data) != 0) {
        }
        FlushCache(0);
        FlushCache(2);
    }
}

static void xglCdArcInitSub0(const int *archive_entry, void *destination, int sectors)
{
    int lbn;
    CdReadMode read_mode;

    sceCdSync(0);
    sceCdDiskReady(0);
    if (sectors == 0) {
        lbn = archive_entry[0];
        sectors = 1;
    } else {
        lbn = archive_entry[0] + 1;
    }
    read_mode.spindle_control = 1;
    read_mode.try_count = 0;
    read_mode.data_pattern = 0;
    sceCdRead(lbn, sectors, destination, &read_mode);
    while (sceCdSync(1) > 0)
        ;
}

int xglCdArcInitSub1(CdArchiveEntry *archive_entry,
                            const char *path, unsigned char *destination)
{
    CdFilePosition file_position;
    int header_count;

    if (xglCdGetFilePos(&file_position, path, xglCdDummyCallback) == 0)
        return 0;

    archive_entry->destination = destination;
    archive_entry->lbn = file_position.lbn;
    archive_entry->state = 0;

    /* xglCdArcInitSub0 only reads its `archive_entry` argument's leading
     * word as an LBN, so the file position's own lbn field (the one
     * evidenced member of CdFilePosition) is exactly what it needs. */
    xglCdArcInitSub0(&file_position.lbn, destination, 0);

    /* Sector 0 of the file is the archive header; its first byte is the
     * header's own sector count. */
    header_count = destination[0];
    if (header_count >= 2) {
        /* The header spans more than one sector: read the remaining
         * header_count - 1 sectors right after the first. */
        xglCdArcInitSub0(&file_position.lbn, destination + CD_SECTOR_BYTES,
                         header_count - 1);
    }
    return header_count + 1;
}

/*
 * Archive table-of-contents walk: archive_data is a header buffer filled by
 * xglCdArcInitSub1/xglCdArcInit, and this finds the byte right after the
 * table's last record so the caller can place fresh data there.
 *
 * Each record begins with a one-byte length/flags field:
 *   - ARC_RECORD_EXTENDED set: no fixed header past this byte.
 *   - ARC_RECORD_EXTENDED clear: a fixed ARC_RECORD_HEADER_BYTES-byte header
 *     follows, extended by ARC_RECORD_TYPE_EXTRA_BYTES more bytes when
 *     ARC_RECORD_TYPE_EXTENDED is also set.
 *   - ARC_RECORD_LENGTH_MASK (the low bits) is the record's payload length,
 *     walked past after the fixed header, if any.
 * A zero length/flags byte ends the table.
 */
unsigned char *xglCdArcInitSub2(unsigned char *archive_data)
{
    unsigned char *cursor = archive_data + 1;
    unsigned char value = *cursor;

    while (value != 0) {
        int high_bit = value & ARC_RECORD_EXTENDED;
        int type_flag = value & ARC_RECORD_TYPE_EXTENDED;
        if (high_bit == 0) {
            cursor += ARC_RECORD_HEADER_BYTES;
            if (type_flag != 0)
                cursor += ARC_RECORD_TYPE_EXTRA_BYTES;
        }
        value &= ARC_RECORD_LENGTH_MASK;
        cursor += value;
        value = *cursor;
    }

    /* Round up to the next ARC_TABLE_ALIGN_BYTES-byte boundary and write a
     * two-byte zero terminator marking the end of the table. */
    cursor = (unsigned char *)(((u32)cursor + ARC_TABLE_ALIGN_BYTES)
        & (u32)-ARC_TABLE_ALIGN_BYTES);
    cursor[1] = 0;
    cursor[0] = 0;
    return cursor;
}

void xglCdArcInit(void)
{
    u8 *work = WorkEnd;
    unsigned char header_sectors = 0;
    int descriptor;

    while (xglHddActivate(-1) != 0 && (descriptor = sceOpen(xenosagaPfs0Path, 1)) >= 0) {
        ArcHeader[0].entry.state = 2;
        ArcHeader[0].entry.destination = work;
        ArcHeader[0].entry.lbn = 0;
        sceRead(descriptor, work, CD_SECTOR_BYTES);
        header_sectors = work[0];
        sceRead(descriptor, work + CD_SECTOR_BYTES,
                (header_sectors << 11) - CD_SECTOR_BYTES);
        sceClose(descriptor);
        break;
    }
    if (header_sectors == 0) {
        if (xglCdArcInitSub1(&ArcHeader[0].entry, xenosagaArc00Name, work) == 0) {
            return;
        }
        xglHddActivate(0x100);
    }
    do {
        if (LW.dual_layer_disc != 0) {
            work = xglCdArcInitSub2(work);
            do {
            } while (xglCdArcInitSub1(&ArcHeader[1].entry, xenosagaArc10Name, work) <= 0);
            work = xglCdArcInitSub2(work);
            LW.search_layer = 1;
            do {
            } while (xglCdArcInitSub1(&ArcHeader[2].entry, xenosagaArc20Name, work) <= 0);
            LW.search_layer = 0;
            work += ((xglCdArcInitSub2(work) - work) + CD_SECTOR_BYTES - 1) & -CD_SECTOR_BYTES;
        } else {
            work += CD_ARC_LOOKAHEAD_BYTES;
            while (xglCdArcInitSub1(&ArcHeader[1].entry, xenosagaArc10Name, work) <= 0
                   && xglCdArcInitSub1(&ArcHeader[1].entry, xenosagaArc20Name, work) <= 0) {
                ArcHeader[1].entry.state = ARC_ENTRY_UNUSED;
                work[0] = 0;
                work[1] = 0;
                break;
            }
            work += CD_ARC_HEADER_RESERVE;
        }
    } while (0);
    WorkEnd = work;
}

void xglCdArcCheck(void)
{
    u8 *saved_work_end = WorkEnd;

    WorkEnd = ArcHeader[0].entry.destination;
    xglCdArcInit();
    WorkEnd = saved_work_end;
}

/* Subsystem start after the reset: calls xglCdReset, HDD activation, the HDD
 * mount and xglCdArcInit (proposed xglCdInitial); called by InitializeSystem. */
/* CD stream setup: 0x40 sectors of IOP buffer in 0x10 banks, plus the
 * alignment slack the 0x80-byte rounding needs. */
#define CD_STREAM_BUFFER_SECTORS 0x40
#define CD_STREAM_BANKS 0x10
#define CD_STREAM_IOP_ALIGN 0x80
#define CD_STREAM_IOP_BYTES (CD_STREAM_BUFFER_SECTORS * CD_SECTOR_BYTES + CD_STREAM_IOP_ALIGN)
#define SCECdMmodeDvd 2

void xglCdInitial(void)
{
    int on_dual;
    unsigned int layer1_start;
    unsigned int iop_buffer;

    xglCdDeviceHandle = 0;
    LW.spindle_control = 0;
    LW.search_layer = 0;
    LW.dual_layer_disc = 0;
    LW.power_off_pending = 0;
    LW.disk_state = 0;
    LW.disk_flags = 0;
    sceCdInit(0);
    sceCdMmode(SCECdMmodeDvd);
    iop_buffer = (unsigned int)sceSifAllocIopHeap(CD_STREAM_IOP_BYTES);
    sceCdStInit(CD_STREAM_BUFFER_SECTORS, CD_STREAM_BANKS,
                (iop_buffer + CD_STREAM_IOP_ALIGN - 1) & -CD_STREAM_IOP_ALIGN);
    xglCdReset();
    on_dual = 0;
    layer1_start = 0;
    sceCdReadDvdDualInfo(&on_dual, &layer1_start);
    loaded_overlay = 2;
    LW.dual_layer_disc = on_dual;
    ArcHeader[0].entry.state = ARC_ENTRY_UNUSED;
    ArcHeader[1].entry.state = ARC_ENTRY_UNUSED;
    ArcHeader[2].entry.state = ARC_ENTRY_UNUSED;
    ArcHeader[3].entry.state = ARC_ENTRY_UNUSED;
    xglHddActivate(0x100);
    xglHddMount();
    xglCdArcInit();
}
