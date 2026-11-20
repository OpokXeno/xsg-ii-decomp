#include "common.h"
#include "main/xgl_mc_request.h"
#include "main/xgl_cd.h"

/* xglMcRequest commands (the callers in yajima_test.c use the same values). */
#define MC_REQ_FREE_SPACE 1
#define MC_REQ_EXIST 2
#define MC_REQ_LIST 3
#define MC_REQ_LOAD 4
#define MC_REQ_SAVE 5
#define MC_REQ_CREATE 6
#define MC_REQ_FORMAT 7
#define MC_REQ_CHECK_OLD 8
#define MC_REQ_UNFORMAT 9
/* The request queue holds 16 entries. */
#define MC_QUEUE_MASK 0xF

/* One queued request (0x20 bytes, xglMcQueue). */
typedef struct McQueueEntry {
    short port;               /* +0x00 */
    short command;            /* +0x02 */
    int *result;              /* +0x04 */
    int file;                 /* +0x08 */
    int data;                 /* +0x0C: the buffer address */
    int size;                 /* +0x10 */
    int (*callback)(int code, void *work);     /* +0x14 */
    unsigned char unmodeled_18[8];
} McQueueEntry;

static unsigned char xglCdServiceActive;
static unsigned char loaded_overlay;
static unsigned char xglMcQueueTop;
static unsigned char xglMcQueueEnd;
static McQueueEntry xglMcQueue[16];

/* Queues a memory-card request (the parameters are copied for the commands that use them),
 * clears *result and runs one step of xglMcMain. */
int xglMcRequest(int port, int command, McRequestParam *param, int *result)
{
    int index;
    McQueueEntry *entry;

    index = xglMcQueueEnd;
    *result = 0;
    entry = &xglMcQueue[index];
    entry->port = port;
    entry->result = result;
    entry->command = command;
    switch (command) {
    case MC_REQ_FREE_SPACE:
    case MC_REQ_FORMAT:
    case MC_REQ_UNFORMAT:
        break;
    case MC_REQ_EXIST:
    case MC_REQ_LIST:
    case MC_REQ_LOAD:
    case MC_REQ_SAVE:
    case MC_REQ_CREATE:
    case MC_REQ_CHECK_OLD:
        entry->file = param->file;
        entry->data = (int)param->data;
        entry->callback = param->callback;
        entry->size = param->size;
        break;
    }
    xglMcQueueEnd = (index + 1) & MC_QUEUE_MASK;
    xglMcMain();
    return 0;
}

#include "shared.h"

/* The icon.sys contents the create request keeps (sizeof(sceMcIconSys) bytes, byte aligned). */
typedef struct McIconSys {
    unsigned char bytes[0x3C4];
} McIconSys;

/* Memory-card state block at xglMcState. */
typedef struct XglMcState {
    unsigned char state;            /* +0x00: phase of the request state machine */
    unsigned char substate;         /* +0x01: step inside the current state, cleared per request */
    unsigned char unmodeled_02[1];
    unsigned char next_card;        /* +0x03: bit 0 is the card the next status poll asks about */
    union {
        int combined;               /* +0x04: the status word xglMcGetState returns */
        short slots[2];             /* +0x04: one MC_STATUS_* halfword per card */
    } card_status;
    int descriptor;                 /* +0x08: the open file, or the card of a status poll */
    int card_type;                  /* +0x0C: sceMcGetInfo type */
    unsigned char unmodeled_10[4];
    int formatted;                  /* +0x14: sceMcGetInfo format flag */
    int create_index;               /* +0x18: progress of the create request */
    unsigned char unmodeled_1c[1];
    unsigned char selector;         /* +0x1D: selector for the card search */
    unsigned char card;             /* +0x1E: card index chosen by the search */
    unsigned char status;           /* +0x1F */
    char full_path[0x60];           /* +0x20: full path of the current request (xglMcBuildPath) */
    McQueueEntry request;           /* +0x80: the request being worked on; the card search reuses its
                                     * file number as the search mode (0..2) and data as the work address */
    McIconSys icon;                 /* +0xA0: icon.sys contents kept by the create request */
} XglMcState;

static XglMcState xglMcState;
/* xglMcState.card_status.combined (+0x04) under its own data label. xglMcGetState and the final
 * return of xglMcMain read it through this label; the early return of xglMcMain (no card operation
 * finished, the queue empty) reads the field. Both were checked: the label in the early return
 * changes four bytes, the field in the final return shortens the function. */
extern int xglMcStatusWord[];
extern char xglMcWildcardName[];
extern const char xglMcBasePath[];

/* Returns the status word of the card state (+0x04 of xglMcState). */
int xglMcGetState(void)
{
    return xglMcStatusWord[0];
}

/* Clears both queue indices and the request phase of the memory-card state. */
void xglMcReset(void)
{
    xglMcQueueTop = 0;
    xglMcState.state = 0;
    xglMcQueueEnd = 0;
}

/*
 * Writes decimal numbers into a save entry as full-width Shift-JIS digits (0x82 0x4F + digit).
 * value goes to object + 0x11B and 0x11D; when source is set, source->hour and source->minute go to object + 0x120..0x129.
 * A first value below 10 gets a full-width space (0x81 0x40) in front.
 */
void xglMcSetEntryNumbers(unsigned char *object, int value, const XglClock *source)
{
    unsigned char *entry_text;
    unsigned char *source_text;
    int first_value;
    int second_value;
    signed char first_tens;

    entry_text = object + 0x40;
    if (value > 0)
    {
        entry_text[0xDD] = (signed char) ((value % 10) + 0x4F);
        entry_text[0xDB] = (signed char) (((value / 10) % 10) + 0x4F);
        if (source != 0)
        {
            first_value = source->hour;
            source_text = object + 0x120;
            second_value = source->minute;
            source_text[3] = (signed char) ((first_value % 10) + 0x4F);
            if (first_value < 10)
            {
                source_text[0] = -0x7F;
                source_text[1] = 0x40;
            }
            else
            {
                first_tens = (signed char) ((first_value / 10) + 0x4F);
                source_text[0] = -0x7E;
                source_text[1] = first_tens;
            }
            source_text[9] = (signed char) ((second_value % 10) + 0x4F);
            source_text[7] = (signed char) ((second_value / 10) + 0x4F);
        }
    }
}

/* Card numbers xglMcBuildPath does not print as digits. */
#define MC_CARD_NONE (-2)
#define MC_CARD_ANY (-1)
/* Selectors of xglMcBuildPath; other values point to a file name. */
#define MC_PATH_REPEAT 0
#define MC_PATH_END (-1)
#define MC_PATH_NAME_COUNT 6

/* File names of selectors 1..5: /icon.sys, /system.ico, /copy.ico, /delete.ico, /dummy.bin. */
extern const char *xglMcFileNames[9];

/* Builds the full path in xglMcState.full_path: base, then the card number as two digits ("*" for
 * MC_CARD_ANY, nothing for MC_CARD_NONE), then by selector the path so far once more, one of the
 * file names 1..5, nothing, or "/" and the name selector points to. Returns the path. */
char *xglMcBuildPath(const char *base, int card, int selector)
{
    char *dest;
    const char *source;
    const char *prefix_end;

    dest = xglMcState.full_path;
    source = base;
    while ((*dest = *source) != 0) {
        dest++;
        source++;
    }
    switch (card) {
    case MC_CARD_NONE:
        break;
    case MC_CARD_ANY:
        *dest = '*';
        dest++;
        break;
    default:
        dest[1] = '0' + card % 10;
        dest[0] = '0' + (card / 10) % 10;
        dest += 2;
        break;
    }
    switch (selector) {
    case MC_PATH_REPEAT:
        prefix_end = dest;
        for (source = xglMcState.full_path; source != prefix_end; source++) {
            *dest = *source;
            dest++;
        }
        *dest = 0;
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        source = xglMcFileNames[selector];
        while ((*dest = *source) != 0) {
            dest++;
            source++;
        }
        break;
    case MC_PATH_END:
        *dest = 0;
        break;
    default:
        *dest = '/';
        dest++;
        source = (const char *)selector;
        while ((*dest = *source) != 0) {
            dest++;
            source++;
        }
        break;
    }
    return xglMcState.full_path;
}


/* Full path for card `card` (or -1 for the wildcard) with the suffix path, built on the base path xglMcBasePath. */
char *xglMcSetFullPath(int card, int selector)
{
    return xglMcBuildPath(xglMcBasePath, card, selector);
}

/* xglMcState.state while a request runs (set by xglMcExecute). */
#define MC_STATE_OPEN_CLOSE 2
#define MC_STATE_WRITE 3
#define MC_STATE_READ 4
#define MC_STATE_CREATE 5
#define MC_STATE_EXIST_SCAN 6
#define MC_STATE_LIST 7
#define MC_STATE_CHECK_OLD 8
/* sceMcOpen modes and the directory read size. */
#define MC_OPEN_READ 1
#define MC_OPEN_WRITE_CREATE 0x202
#define MC_DIRECTORY_MAX_ENTRIES 100
/* Entries of a list / check-old work buffer's slot table (McListWork.slot). */
#define MC_SLOT_COUNT 100
/* sceMcGetDir writes its entry table into the request's work buffer, behind the
 * MC_SLOT_COUNT slot bytes and rounded up to the 64-byte alignment the SDK needs. */
#define MC_DIRECTORY_TABLE(work) (((work) + MC_SLOT_COUNT + 0x3F) & ~0x3F)

int sceMcGetInfo(int port, int slot, int *type, int *free, int *format);
int sceMcMkdir(int port, int slot, const char *name);
int sceMcGetDir(int port, int slot, const char *name, int mode, int max_entries, int table);
int sceMcOpen(int port, int slot, const char *name, int mode);
/* Save folders of the check-old search, one per search mode (xglMcState.request.file):
 * /BASLUS-20469Xeno, /BISLPS-29005Xeno, /BISLPS-25353. This game's own folder is
 * xglMcBasePath, /BASLUS-20892Xeno2. */
extern char *xglMcBasePaths[];
int sceMcFormat(int port, int slot);
int sceMcUnformat(int port, int slot);

const char xglMcBasePath[24] = "/BASLUS-20892Xeno2";

/* Starts the queued request: the SDK call for the command and the state xglMcMain then follows. */
void xglMcExecute(McQueueEntry *request)
{
    xglMcState.substate = 0;
    switch (request->command) {
    case MC_REQ_FREE_SPACE:
        sceMcGetInfo(request->port, 0, 0, request->result, 0);
        return;
    case MC_REQ_EXIST:
        if (request->file == -1) {
            xglMcSetFullPath(-1, -1);
            sceMcGetDir(request->port, 0, xglMcState.full_path, 0, MC_DIRECTORY_MAX_ENTRIES,
                        MC_DIRECTORY_TABLE(request->data));
            xglMcState.state = MC_STATE_EXIST_SCAN;
            return;
        }
        xglMcSetFullPath(request->file, 0);
        sceMcOpen(request->port, 0, xglMcState.full_path, MC_OPEN_READ);
        xglMcState.state = MC_STATE_OPEN_CLOSE;
        return;
    case MC_REQ_LIST:
        xglMcSetFullPath(-1, -1);
        xglMcState.state = MC_STATE_LIST;
        sceMcGetDir(request->port, 0, xglMcState.full_path, 0, MC_DIRECTORY_MAX_ENTRIES,
                    MC_DIRECTORY_TABLE(request->data));
        return;
    case MC_REQ_CHECK_OLD:
        xglMcBuildPath(xglMcBasePaths[0], -1, -1);
        request->file = 0;
        xglMcState.state = MC_STATE_CHECK_OLD;
        sceMcGetDir(request->port, 0, xglMcState.full_path, 0, MC_DIRECTORY_MAX_ENTRIES,
                    MC_DIRECTORY_TABLE(request->data));
        return;
    case MC_REQ_CREATE:
        xglMcSetFullPath(request->file, -1);
        sceMcMkdir(request->port, 0, xglMcState.full_path);
        xglMcState.create_index = 0;
        xglMcState.state = MC_STATE_CREATE;
        return;
    case MC_REQ_SAVE:
        xglMcSetFullPath(request->file, 0);
        sceMcOpen(request->port, 0, xglMcState.full_path, MC_OPEN_WRITE_CREATE);
        xglMcState.state = MC_STATE_WRITE;
        return;
    case MC_REQ_LOAD:
        xglMcSetFullPath(request->file, 0);
        sceMcOpen(request->port, 0, xglMcState.full_path, MC_OPEN_READ);
        xglMcState.state = MC_STATE_READ;
        return;
    case MC_REQ_FORMAT:
        sceMcFormat(request->port, 0);
        return;
    case MC_REQ_UNFORMAT:
        sceMcUnformat(request->port, 0);
        break;
    }
}

/* The create request's buffer: a table of file offsets (file k spans offset[k]..offset[k + 1],
 * each start rounded up to 64 bytes, relative to the end of the rounded header size offset[0]),
 * then at +0x40 the icon.sys contents. */
typedef struct McCreateData {
    int offset[16];
    McIconSys icon;
} McCreateData;

/* Steps of the create request (xglMcState.substate). From MC_CREATE_FILES_START on, each file
 * takes three steps: open, write, close; step / 3 + 1 is its xglMcBuildPath selector
 * (2 system.ico, 3 copy.ico, 4 delete.ico, 5 dummy.bin). */
#define MC_CREATE_DELETE_SAVE 0
#define MC_CREATE_DELETE_DUMMY 1
#define MC_CREATE_COPY_ICON 2
#define MC_CREATE_FILES_START 3 /* no card operation; the delete steps before it accept MC_RESULT_NO_ENTRY */
#define MC_CREATE_OPEN_SYSTEM_ICO 4
#define MC_CREATE_WRITE_SYSTEM_ICO 5
#define MC_CREATE_CLOSE_SYSTEM_ICO 6
#define MC_CREATE_OPEN_COPY_ICO 7
#define MC_CREATE_WRITE_COPY_ICO 8
#define MC_CREATE_CLOSE_COPY_ICO 9
#define MC_CREATE_OPEN_DELETE_ICO 10
#define MC_CREATE_WRITE_DELETE_ICO 11
#define MC_CREATE_CLOSE_DELETE_ICO 12
#define MC_CREATE_OPEN_DUMMY 13
#define MC_CREATE_DUMMY_WRITE 14
#define MC_CREATE_LAST_CLOSE 15
#define MC_CREATE_STEPS_PER_FILE 3
/* The save folder is filled up to this many bytes by the dummy file. */
#define MC_CREATE_TOTAL_BYTES 0x42400
/* sceMc result the delete steps accept: the file did not exist. */
#define MC_RESULT_NO_ENTRY (-4)

/* xglMcState.icon (+0xA0, 0x959FD8) under its own data label: the copy goes through the label.
 * Through the field it reuses the xglMcState base register and does not match. */
extern McIconSys xglMcIconSys;
int sceMcDelete(int port, int slot, const char *name);
int sceMcClose(int descriptor);
int sceMcWrite(int descriptor, const void *buffer, int size);

/* One step of the create request, called with the result of the previous card operation: deletes
 * the old save and dummy file, keeps the icon.sys contents, then opens, writes and closes the
 * files of the buffer's offset table and the dummy file that fills the folder. Returns 1 when
 * done, 0 while steps remain (the step count goes to the request's result), -1 on an error. */
int xglMcCreateStep(int result)
{
    int file;
    McCreateData *save;
    char *data;
    int *entry;
    int offset;
    int size;

    file = xglMcState.request.file;
    if (result >= 0 || (xglMcState.substate < MC_CREATE_FILES_START && result == MC_RESULT_NO_ENTRY)) {
        save = (McCreateData *)xglMcState.request.data;
        switch (xglMcState.substate) {
        case MC_CREATE_DELETE_SAVE:
            xglMcSetFullPath(file, 0);
            sceMcDelete(xglMcState.request.port, 0, xglMcState.full_path);
            break;
        case MC_CREATE_DELETE_DUMMY:
            xglMcSetFullPath(file, 5);
            sceMcDelete(xglMcState.request.port, 0, xglMcState.full_path);
            break;
        case MC_CREATE_COPY_ICON:
            xglMcIconSys = save->icon;
            break;
        case MC_CREATE_FILES_START:
            break;
        case MC_CREATE_CLOSE_SYSTEM_ICO:
        case MC_CREATE_CLOSE_COPY_ICO:
        case MC_CREATE_CLOSE_DELETE_ICO:
        case MC_CREATE_LAST_CLOSE:
            sceMcClose(xglMcState.descriptor);
            /* An offset table that ends after system.ico (offset[3] == 0) has no copy.ico,
               delete.ico or dummy file. */
            if (save->offset[3] == 0 && xglMcState.substate == MC_CREATE_CLOSE_SYSTEM_ICO) {
                return 1;
            }
            if (xglMcState.substate == MC_CREATE_LAST_CLOSE) {
                return 1;
            }
            break;
        case MC_CREATE_OPEN_SYSTEM_ICO:
        case MC_CREATE_OPEN_COPY_ICO:
        case MC_CREATE_OPEN_DELETE_ICO:
        case MC_CREATE_OPEN_DUMMY:
            xglMcSetFullPath(file, xglMcState.substate / MC_CREATE_STEPS_PER_FILE + 1);
            sceMcOpen(xglMcState.request.port, 0, xglMcState.full_path, MC_OPEN_WRITE_CREATE);
            break;
        case MC_CREATE_WRITE_SYSTEM_ICO:
        case MC_CREATE_WRITE_COPY_ICO:
        case MC_CREATE_WRITE_DELETE_ICO:
            xglMcState.descriptor = result;
            data = (char *)save + ((save->offset[0] + 0x3F) & ~0x3F);
            /* The entry address is computed as an integer, index first (addu index*4, base). */
            entry = (int *)(xglMcState.substate / MC_CREATE_STEPS_PER_FILE * 4 + (int)save);
            offset = (entry[0] + 0x3F) & ~0x3F;
            size = entry[1] - offset;
            xglMcState.create_index += (size + 0x3FF) & ~0x3FF;
            sceMcWrite(result, data + offset, size);
            break;
        case MC_CREATE_DUMMY_WRITE:
            xglMcState.descriptor = result;
            sceMcWrite(result, save, MC_CREATE_TOTAL_BYTES - xglMcState.create_index);
            break;
        }
        xglMcState.substate++;
        *xglMcState.request.result = xglMcState.substate;
        return 0;
    }
    *xglMcState.request.result = result;
    return -1;
}

#define MC_SLOT_WANTED 1 /* the value the search looks for */
/* McListWork.latest: the card number with the newest time stamp, behind the slot table (X1). */
#define MC_LATEST_SLOT_INDEX MC_SLOT_COUNT

/* Returns the index of the entry with value 1 in the 100-entry table nearest to index, searching
 * index, index+1, index-1, index+2, ...; -1 once both directions have run off the table. */
int xglMcFindNearestSlot(unsigned char *table, int index)
{
    int step;
    int misses;

    step = 1;
    misses = 0;
    while (table[index] != MC_SLOT_WANTED) {
        for (;;) {
            index += step;
            if (step > 0) {
                step = -step - 1;
            } else {
                step = 1 - step;
            }
            if ((unsigned int)index < MC_SLOT_COUNT) {
                break;
            }
            misses++;
            if (misses >= 2) {
                return -1;
            }
        }
        misses = 0;
    }
    return index;
}


/* Picks a card for the request, then starts a directory scan of 16 entries on it. Returns 1 when no card fits. */
int xglMcStartDirectoryScan(void)
{
    int card;

    card = xglMcFindNearestSlot((unsigned char *)xglMcState.request.data, xglMcState.selector);
    if (card < 0)
    {
        return 1;
    }
    xglMcState.card = (signed char) card;
    xglMcState.status = 0;
    /* selector values outside the small codes are name pointers (xglMcBuildPath); this one is "*", every save folder */
    xglMcSetFullPath(card, (int)xglMcWildcardName);
    sceMcGetDir(xglMcState.request.port, 0, xglMcState.full_path, 0, 0x10, MC_DIRECTORY_TABLE(xglMcState.request.data));
    return 0;
}

/* Opens the save file on the card nearest to the selector; when none fits, moves on to the next
 * search mode: mode 0 starts a directory read of up to 100 entries on the second base path
 * (returns -1), mode 1 opens the third base path, mode 2 gives up (returns 1). Returns 0 when a
 * file open was started. */
int xglMcOpenNearestSave(void)
{
    int card;

    card = xglMcFindNearestSlot((unsigned char *)xglMcState.request.data, xglMcState.selector);
    if (card < 0) {
        /* The search mode is tested unsigned (beqz for mode 0 after the test for 1). */
        switch ((unsigned int)xglMcState.request.file) {
        case 0:
            xglMcState.request.file = 1;
            xglMcBuildPath(xglMcBasePaths[xglMcState.request.file], -1, -1);
            sceMcGetDir(xglMcState.request.port, 0, xglMcState.full_path, 0, 100,
                        MC_DIRECTORY_TABLE(xglMcState.request.data));
            return -1;
        case 1:
            xglMcState.request.file = 2;
            xglMcBuildPath(xglMcBasePaths[xglMcState.request.file], -2, 0);
            sceMcOpen(xglMcState.request.port, 0, xglMcState.full_path, 1);
            return 0;
        case 2:
            return 1;
        }
    }
    xglMcState.card = card;
    xglMcState.status = 0;
    xglMcBuildPath(xglMcBasePaths[xglMcState.request.file], card, 0);
    sceMcOpen(xglMcState.request.port, 0, xglMcState.full_path, 1);
    return 0;
}

/* Result of xglMcMain / the card status word while a request is running. */
#define MC_STATUS_BUSY_BOTH 0xFF00FF
/* Per-card status halfwords (xglMcState.card_status.slots). */
#define MC_STATUS_NONE 0
#define MC_STATUS_READY 1
#define MC_STATUS_UNFORMATTED 2
#define MC_STATUS_ERROR 0x80
#define MC_STATUS_PS1_CARD 0x81
#define MC_STATUS_POCKETSTATION 0x82
/* sceMcGetInfo card types. */
#define MC_TYPE_PS1 1
#define MC_TYPE_PS2 2
#define MC_TYPE_POCKETSTATION 3
#define MC_STATE_IDLE 0
#define MC_STATE_POLL_INFO 1
/* Steps of a list / check-old scan (xglMcState.substate). */
#define MC_SCAN_DIRECTORY 0
#define MC_SCAN_NEXT_CARD 1
#define MC_SCAN_READ 2
#define MC_SCAN_REPORT 3
#define MC_SCAN_ICON_CHECK 4
/* McListWork.status values. */
#define MC_LIST_FOUND 1
#define MC_LIST_FAILED 2
/* Codes the scan callback gets instead of a card number. */
#define MC_CALLBACK_DIRECTORY (-1)
#define MC_CALLBACK_NO_DATA (-2)
#define MC_CALLBACK_ERROR (-3)
/* sceMc results below this one are hard errors for a scan. */
#define MC_RESULT_SOFT_LIMIT (-9)
/* Bytes read from a save of the other save folders (check-old). */
#define MC_OLD_SAVE_BYTES 0x163B0
/* Bytes of an old save the check-old scan reports, relative to the read data. */
#define MC_OLD_CLEAR_FLAG 0x10250
#define MC_OLD_CLEAR_VALUE 10
#define MC_OLD_INFO_1 0x10418
#define MC_OLD_INFO_2 0x10598
#define MC_OLD_INFO_3 0x10718
#define MC_OLD_INFO_5 0x10A18
#define MC_OLD_INFO_6 0x10B98
#define MC_OLD_INFO_7 0x10D18
#define MC_OLD2_FLAG 0xA4
#define MC_OLD2_INFO_1 0x50

/* The work buffer of a list / check-old request (request.data). */
typedef struct McListWork {
    unsigned char slot[100];   /* +0x00: per card number: 1 present, 2 read */
    unsigned char latest;      /* +0x64: card number with the newest time stamp */
    unsigned char status;      /* +0x65 */
    unsigned char has_icon;    /* +0x66: the card's folder holds a full icon.sys */
    unsigned char unmodeled_67[0x2000 - 0x67];
    /* +0x2000: the file contents (up to MC_OLD_SAVE_BYTES for check-old), then the bytes reported
     * from them at the start. */
    unsigned char data[MC_OLD_SAVE_BYTES];
} McListWork;

/* One sceMcGetDir entry (0x40 bytes). */
typedef struct McDirectoryEntry {
    unsigned char unmodeled_00[9];
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned short year;
    int size;                  /* +0x10 */
    unsigned char unmodeled_14[0xC];
    char name[32];             /* +0x20 */
} McDirectoryEntry;

/* xglMcState.request (+0x80, 0x959FB8) under its own data label: xglMcMain copies the queued
 * request through it. Through the field the copy reuses the xglMcState base register and is two
 * instructions shorter. */
extern McQueueEntry savedMcRequest;
int sceMcSync(int mode, int *command, int *result);
int sceMcRead(int descriptor, void *buffer, int size);
int strcmp(const char *left, const char *right);
void *memset(void *destination, int value, unsigned int size);
int xglMcStartDirectoryScan(void);
int xglMcOpenNearestSave(void);
int xglMcCreateStep(int result);

/* One step of the memory-card service: polls the pending card operation, then either starts the
 * next queued request, refreshes a card's status, or advances the running request (open/close,
 * read/write followed by icon.sys, create, or the directory/list/check-old scans). Returns
 * MC_STATUS_BUSY_BOTH while a request runs, otherwise the card status word. */
int xglMcMain(void)
{
    XglClock clock;
    int command;
    int result;
    int sync;
    short *status;
    int stored;
    McListWork *list;
    int (*callback)(int code, void *work);
    McDirectoryEntry *directory;
    unsigned int latest;
    unsigned int timestamp;
    int latest_slot;
    int index;
    int entry;
    int slot;
    int created;
    int card_type;
    int scan;
    unsigned char card;
    unsigned char phase;
    void *transfer;
    int size;
    unsigned char *data;
    int chosen;
    int selected;
    char *end;
    McQueueEntry *request;
    unsigned char *clear_slot;

    sync = sceMcSync(1, &command, &result);
    if (sync == 0) {
        if (xglMcState.state == MC_STATE_POLL_INFO && xglMcQueueEnd == xglMcQueueTop) {
            return xglMcState.card_status.combined;
        }
        return MC_STATUS_BUSY_BOTH;
    }
    if (sync == -1) {
        result = 0;
        command = 0;
    }
    switch (xglMcState.state) {
    case MC_STATE_POLL_INFO:
        if (xglMcQueueEnd == xglMcQueueTop) {
            status = &xglMcState.card_status.slots[xglMcState.descriptor];
            switch (xglMcState.card_type) {
            case MC_TYPE_PS1:
                *status = MC_STATUS_PS1_CARD;
                break;
            case MC_TYPE_PS2:
                *status = MC_STATUS_READY;
                break;
            case MC_TYPE_POCKETSTATION:
                *status = MC_STATUS_POCKETSTATION;
                break;
            default:
                *status = MC_STATUS_NONE;
                break;
            }
            if (result == -1 || result == -2) {
                *status = MC_STATUS_ERROR;
            } else {
                card_type = xglMcState.card_type;
                if (card_type == MC_TYPE_PS2 && result == 0 && xglMcState.formatted == 0) {
                    *status = MC_STATUS_UNFORMATTED;
                }
            }
        }
        xglMcState.state = MC_STATE_IDLE;
    case MC_STATE_IDLE:
        if (xglMcQueueEnd != xglMcQueueTop) {
            request = &xglMcQueue[xglMcQueueTop];
            savedMcRequest = *request;
            xglMcExecute(request);
            xglMcQueueTop = (xglMcQueueTop + 1) & MC_QUEUE_MASK;
            return MC_STATUS_BUSY_BOTH;
        }
        card = xglMcState.next_card & 1;
        xglMcState.descriptor = card;
        sceMcGetInfo(card, 0, &xglMcState.card_type, 0, &xglMcState.formatted);
        xglMcState.state = MC_STATE_POLL_INFO;
        xglMcState.next_card ^= 1;
        break;
    case MC_STATE_OPEN_CLOSE:
        *xglMcState.request.result = result;
        xglMcState.state = MC_STATE_IDLE;
        if (result < 0) {
            break;
        }
        sceMcClose(result);
        return MC_STATUS_BUSY_BOTH;
    case MC_STATE_EXIST_SCAN:
    case MC_STATE_LIST:
    case MC_STATE_CHECK_OLD:
        list = (McListWork *)xglMcState.request.data;
        callback = xglMcState.request.callback;
        while ((stored = result) < 0) {
            if (xglMcState.state == MC_STATE_CHECK_OLD && xglMcState.substate == MC_SCAN_READ
                && xglMcState.request.file == 2) {
                list->status = MC_LIST_FAILED;
                if (result >= MC_RESULT_SOFT_LIMIT) {
                    callback(MC_CALLBACK_NO_DATA, list);
                } else {
                    callback(MC_CALLBACK_ERROR, list);
                }
            } else {
                if (stored >= MC_RESULT_SOFT_LIMIT
                    && (xglMcState.substate == MC_SCAN_REPORT || xglMcState.substate == MC_SCAN_ICON_CHECK)) {
                    break;
                }
                if (xglMcState.state == MC_STATE_EXIST_SCAN) {
                    break;
                }
                if (xglMcState.substate == MC_SCAN_READ) {
                    break;
                }
                list->status = MC_LIST_FAILED;
                callback(MC_CALLBACK_ERROR, list);
            }
            xglMcState.state = MC_STATE_IDLE;
            return MC_STATUS_BUSY_BOTH;
        }
        switch (xglMcState.substate) {
        case MC_SCAN_DIRECTORY:
            *xglMcState.request.result = stored;
            /* Clears the slot table, latest and status (XS1 stops at latest). */
            index = MC_LATEST_SLOT_INDEX + 1;
            clear_slot = &list->status;
            do {
                index--;
                *clear_slot = 0;
                clear_slot--;
            } while (index >= 0);
            if (result > 0) {
                directory = (McDirectoryEntry *)MC_DIRECTORY_TABLE((unsigned int)list);
                latest = 0;
                index = 0;
                latest_slot = 0;
                while (index < result) {
                    slot = 0;
                    for (end = directory->name; *end != 0; end++) {
                    }
                    if ((end[-2] & 0xF0) == '0' && (end[-1] & 0xF0) == '0') {
                        slot = end[-2] * 10 + end[-1] - ('0' * 10 + '0');
                    }
                    list->slot[slot] = 1;
                    clock.year = directory->year;
                    clock.month = directory->month;
                    clock.day = directory->day;
                    clock.hour = directory->hour;
                    clock.minute = directory->minute;
                    clock.second = directory->second;
                    clock.status = 0;
                    timestamp = xglClockDayTime2UInt(&clock);
                    if (latest < timestamp) {
                        latest = timestamp;
                        latest_slot = slot;
                    }
                    index++;
                    directory++;
                }
                list->latest = latest_slot;
                list->status = MC_LIST_FOUND;
            }
            if (xglMcState.state == MC_STATE_EXIST_SCAN) {
                xglMcState.state = MC_STATE_IDLE;
            } else if (xglMcState.state == MC_STATE_LIST) {
                chosen = callback(MC_CALLBACK_DIRECTORY, list);
                if (chosen < 0) {
                    chosen = list->latest;
                }
                xglMcState.selector = chosen;
            } else {
                xglMcState.selector = list->latest;
            }
            xglMcState.substate = MC_SCAN_NEXT_CARD;
            break;
        case MC_SCAN_REPORT:
            selected = -1;
            if (xglMcState.descriptor >= 0) {
                sceMcClose(xglMcState.descriptor);
            }
            list->slot[xglMcState.card] = 2;
            if (xglMcState.state == MC_STATE_LIST) {
                if (list->has_icon != 1) {
                    memset(list->data, 0, xglMcState.request.size);
                }
                selected = callback(xglMcState.card, list->data);
            } else {
                data = list->data;
                switch ((unsigned int)xglMcState.request.file) {
                case 0:
                case 1:
                    data[0] = 0;
                    if (data[MC_OLD_CLEAR_FLAG] == MC_OLD_CLEAR_VALUE) {
                        data[0] = 1;
                    }
                    list->data[1] = data[MC_OLD_INFO_1];
                    list->data[2] = data[MC_OLD_INFO_2];
                    list->data[3] = data[MC_OLD_INFO_3];
                    list->data[4] = 0;
                    list->data[5] = data[MC_OLD_INFO_5];
                    list->data[6] = data[MC_OLD_INFO_6];
                    list->data[7] = data[MC_OLD_INFO_7];
                    break;
                case 2:
                    data[0] = data[MC_OLD2_FLAG];
                    list->data[1] = data[MC_OLD2_INFO_1];
                    break;
                }
                callback(xglMcState.request.file, data);
            }
            if (selected >= 0) {
                xglMcState.selector = selected;
            }
            xglMcState.substate = MC_SCAN_NEXT_CARD;
            return MC_STATUS_BUSY_BOTH;
        case MC_SCAN_NEXT_CARD:
            if (xglMcState.state == MC_STATE_LIST) {
                scan = xglMcStartDirectoryScan();
                xglMcState.substate = MC_SCAN_ICON_CHECK;
            } else {
                scan = xglMcOpenNearestSave();
                xglMcState.substate = MC_SCAN_READ;
            }
            switch (scan) {
            case 0:
                return MC_STATUS_BUSY_BOTH;
            case -1:
                xglMcState.substate = MC_SCAN_DIRECTORY;
                return MC_STATUS_BUSY_BOTH;
            case 1:
                list->status = MC_LIST_FAILED;
                callback(MC_CALLBACK_NO_DATA, list);
                xglMcState.state = MC_STATE_IDLE;
                break;
            }
            break;
        case MC_SCAN_READ:
            xglMcState.descriptor = result;
            if (result >= 0) {
                if (xglMcState.state == MC_STATE_LIST) {
                    sceMcRead(stored, list->data, xglMcState.request.size);
                } else {
                    sceMcRead(stored, list->data, MC_OLD_SAVE_BYTES);
                }
            } else {
                list->has_icon = 0;
            }
            xglMcState.substate = MC_SCAN_REPORT;
            return MC_STATUS_BUSY_BOTH;
        case MC_SCAN_ICON_CHECK:
            list->has_icon = 0;
            if (result >= 0) {
                directory = (McDirectoryEntry *)MC_DIRECTORY_TABLE((unsigned int)list);
                for (entry = 0; entry < result; entry++, directory++) {
                    if (strcmp(directory->name, "icon.sys") == 0 && directory->size == sizeof(McIconSys)) {
                        list->has_icon = 1;
                    }
                }
            }
            xglMcSetFullPath(xglMcState.card, 0);
            sceMcOpen(xglMcState.request.port, 0, xglMcState.full_path, MC_OPEN_READ);
            xglMcState.substate = MC_SCAN_READ;
            return MC_STATUS_BUSY_BOTH;
        }
        break;
    case MC_STATE_CREATE:
        created = xglMcCreateStep(result);
        if (created == 0) {
            return MC_STATUS_BUSY_BOTH;
        }
        xglMcState.state = MC_STATE_IDLE;
        if (created < 0) {
            xglMcState.card_status.combined = MC_STATUS_BUSY_BOTH;
        }
        break;
    case MC_STATE_WRITE:
    case MC_STATE_READ:
        if (result >= 0) {
            switch (xglMcState.substate) {
            case 0:
                xglMcState.descriptor = result;
                phase = xglMcState.state;
                size = xglMcState.request.size;
                transfer = (void *)xglMcState.request.data;
                switch (phase) {
                case MC_STATE_WRITE:
                    sceMcWrite(result, transfer, size);
                    break;
                case MC_STATE_READ:
                    sceMcRead(result, transfer, size);
                    break;
                }
                xglMcState.substate = 1;
                break;
            case 1:
            case 4:
                sceMcClose(xglMcState.descriptor);
                if (xglMcState.state == MC_STATE_WRITE && xglMcState.substate == 1) {
                    xglMcState.substate = 2;
                } else {
                    xglMcState.state = MC_STATE_IDLE;
                }
                break;
            case 2:
                xglMcSetFullPath(xglMcState.request.file, 1);
                sceMcOpen(xglMcState.request.port, 0, xglMcState.full_path, MC_OPEN_WRITE_CREATE);
                xglMcState.substate = 3;
                break;
            case 3:
                xglMcState.descriptor = result;
                sceMcWrite(xglMcState.descriptor, &xglMcState.icon, sizeof(McIconSys));
                xglMcState.substate = 4;
                break;
            }
            return MC_STATUS_BUSY_BOTH;
        }
        *xglMcState.request.result = result;
        xglMcState.card_status.combined = MC_STATUS_BUSY_BOTH;
        xglMcState.state = MC_STATE_IDLE;
        break;
    }
    return xglMcStatusWord[0];
}

char *xglMcBasePaths[4] = { xglMcSaveDirBaslus20469, xglMcSaveDirBislps29005, xglMcSaveDirBislps25353, 0 };
char xglMcIconSysName[16] = "/icon.sys";
char xglMcSystemIconName[16] = "/system.ico";
char xglMcCopyIconName[16] = "/copy.ico";
char xglMcDeleteIconName[16] = "/delete.ico";
char xglMcDummyFileName[12] = "/dummy.bin";
const char *xglMcFileNames[9] = { 0, xglMcIconSysName, xglMcSystemIconName, xglMcCopyIconName, xglMcDeleteIconName, xglMcDummyFileName, 0, 0, 0 };
/* The save image (0x5160 bytes, the size saveChecksum sums), declared in xgl_mc_request.h. */
struct SaveData SaveData = { 0 };

char xglMcWildcardName[8] = "*";
