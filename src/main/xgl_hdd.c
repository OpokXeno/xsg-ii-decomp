#include "common.h"
#include "shared.h"
#include "main/pad.h"
#include "main/tu012.h"
#include "main/tu021.h"
#include "main/tu093.h"
#include "main/xgl_cd.h"
#include "main/xgl_hdd.h"
#include "main/control_entry.h"
#include "main/xgl_dma.h"
#include "main/xgl_font.h"

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

struct stat;
int stat(const char *path, struct stat *status);
extern u8 *buffer;
extern unsigned int strlen(const char *string);
extern int strcmp(const char *left, const char *right);
extern int strncmp(const char *left, const char *right, unsigned int count);
extern int xglHddMcUmount(void);
extern int xglHddMcLoadMount(void);
extern int xglHddMcLoadCore(void *save);

#define HDD_CHECK_TOO_MANY_FOLDERS (-2)
#define HDD_CHECK_TOO_MANY_SAVES (-3)
#define HDD_CHECK_NO_SPACE (-4)
#define HDD_DEVCTL_FLUSH 0x4804
#define HDD_DEVCTL_STATUS 0x4807
#define HDD_DEVCTL_FREE_SECTORS_TOTAL 0x480a
#define HDD_DEVCTL_ZONE_SIZE 0x5001
#define HDD_DEVCTL_FREE_ZONES 0x5002

/* The header of the card data image: the begin and end of the transferred
 * payload (carddata.new). */
struct HddTransferInfo {
    int unmodeled_00;
    int begin;
    int end;
};

struct HddSaveImage;

/* The card save request: xglHddMcCheck/xglHddMcCreate read it through
 * `transfer`, which file.c fills with its card data buffer. */
struct HddCheckState {
    int status;
    union {
        unsigned char *bytes; /* file.c: the card data buffer it hands in */
        struct HddTransferInfo *info; /* xgl_hdd: the begin/end transfer header */
        struct HddSaveImage *image; /* xgl_hdd: the whole saved image, header first */
    } transfer;
    int dataSize;
};

struct HddExistRequest {
    int card;
    unsigned char *found;
};

struct HddLoadRequest {
    int card;
    void *data;
    int size;
};

struct HddSaveRequest {
    int card;
    const void *data;
    int size;
};

/*
 * The xglHdd layer keeps its own copy of the "hdd0:" device name at
 * main:0x004DC368; the HddTest debug tool has an identical string of its own
 * at 0x004DA2D0, which accepted source calls hdd_device.  Neither address
 * carries an original symbol, so this one is named for the layer that owns
 * it.
 */
extern char xgl_hdd_device[];
extern char hdd_mc_path[];
extern char mount_device[];
extern int xglHddMcGetFree(void);
int xglHddMcExist(struct HddExistRequest *request);
int xglHddMcSave(const struct HddSaveRequest *save);

/* Size of the icon.sys image that opens every resCmdSave (the icon header). */
#define MC_ICON_SYS_SIZE 964

unsigned int xglClockDayTime2UInt(XglClock *clock_time);

struct HddIoStat {
    unsigned int mode;
    unsigned int attributes;
    unsigned int size;
    unsigned char created[8];
    unsigned char accessed[8];
    unsigned char modified[8];
    unsigned int sizeHigh;
    unsigned int privateData[6];
};

struct HddFileStat {
    unsigned int mode;
    unsigned int flags;
    unsigned char unmodeled_08[21];
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned short year;
    unsigned char unmodeled_24[4];
    unsigned int privateData[6];
};

struct HddDirectoryEntry {
    struct HddFileStat stat;
    char name[256];
    unsigned char unmodeled_140[16];
};

extern u8 system_cnf[];
int sceRead(int descriptor, void *buffer, int bytes);
int xglHddCheck2(void);
static int xglHddCheckCore(void);
extern int sceUmount(const char *path);
int xglHddMcLoad(void *save);
int xglHddUninstall(void);
extern int sceRemove(const char *path);
extern int sceMount(char *mount_point, char *device, int flags,
                    void *payload, unsigned int payload_length);
extern int sceDevctl(const char *device, int command, const void *input,
                     unsigned int input_size, void *output, unsigned int output_size);
extern int sceOpen(const char *path, int flags, ...);
extern int sceDopen(const char *path);
extern int sceDread(int descriptor, struct HddDirectoryEntry *entry);
extern int sceDclose(int descriptor);
extern int sceGetstat(const char *path, struct HddFileStat *stat);
extern int sceClose(int descriptor);
extern int strncmp(const char *left, const char *right, unsigned int length);
extern unsigned int xglClockDayTime2UInt(XglClock *clock_time);
extern int sceWrite(int descriptor, const void *buffer, int size);
extern int sceChstat(const char *path, const struct HddIoStat *stat, int flags);
int Judge_MakeNewFolder(void);
extern int Judge_MakeNewSavedata(int descriptor, int card);
static int xglHddMcCheckYourSaves(int);
int xglHddMcCheckCore(struct HddCheckState *state);
extern int xglHddMcCheck(struct HddCheckState *state);
int create_file(int card, int slot, const void *data, int size);

/* sceOpen flags and permission modes (0666, 0555, 0777). */
#define HDD_OPEN_READ 1
#define HDD_OPEN_WRITE_CREATE_TRUNC 0x602
#define HDD_MODE_RW 438
/* sceMount flags, and the errors the layer tests for. */
#define HDD_MOUNT_READ_WRITE 4
#define HDD_MOUNT_READ_ONLY 5
#define HDD_ERR_NOENT (-2)
#define HDD_ERR_BUSY (-16)
#define HDD_ERR_NODEV (-19)
/* Results of xglHddCheckCore() beyond 0 (ready). */
#define HDD_NOT_READY (-5)
#define HDD_RESET_REQUIRED (-6)
#define HDD_PATH_MAX 256
#define HDD_DIR_SCAN_LIMIT 1024
#define HDD_MAX_FOLDERS 256
#define HDD_FOLDER_ENTRY_LIMIT 0x3FE
#define HDD_SLACK_ZONES 7
/* Total free sectors above which the save area can be grown, and the free
 * sectors (total, and the largest free block) the game installation needs. */
#define HDD_LARGE_DISK_SECTORS 0x1fffff
#define HDD_LARGE_DISK_FREE_KB 0x100000
#define HDD_SAVE_ERR_WRITE (-24)
/* xglHddMcLoadCore() */
#define HDD_LOAD_ERR_OPEN (-6)
#define HDD_LOAD_ERR_READ (-7)
#define HDD_LOAD_ERR_CLOSE (-8)
/* The other sceOpen flag, and the 0555 and 0777 modes named above. */
#define HDD_OPEN_READ_WRITE 3
#define HDD_MODE_READ_ONLY 0x16d
#define HDD_MODE_DIRECTORY 0x1ff
#define HDD_SECTOR_SIZE 512
/* sceDevctl: the largest free block; sceIoctl2: add a sub-partition. */
#define HDD_DEVCTL_MAX_SECTORS 0x4801
#define HDD_IOCTL2_ADD_SUBPARTITION 0x6801
#define HDD_INSTALL_MIN_SECTORS 0x37ffff
#define HDD_INSTALL_MIN_FREE_SECTORS 0x7ffff
/* The leading directory record (lsn, size, name, date) xglHddMount compares. */
#define HDD_CD_RECORD_SIZE 36
/* xglHddErrorScreen: frames before the circle button is accepted, and the
 * decide sound it plays (seVoiceStartDefault tag 1, id 2). */
#define HDD_ERROR_WAIT_FRAMES 30
#define HDD_ERROR_SE_DECIDE 0x01000002
/* xglHddMcCreate() */
#define HDD_CREATE_ERR_MKDIR_ROOT (-10)
#define HDD_CREATE_ERR_CLOSE_ROOT (-0x0c)
#define HDD_CREATE_ERR_OPEN_DEVICE (-0x10)
#define HDD_CREATE_ERR_ADD_PARTITION (-0x11)
#define HDD_CREATE_ERR_CLOSE_DEVICE (-0x12)
#define HDD_CREATE_ERR_OPEN_SAVE_DIR (-0x13)
#define HDD_CREATE_ERR_MKDIR_SAVE (-0x14)
#define HDD_CREATE_ERR_SAVE_DIR_CLOSE (-0x15)
#define HDD_CREATE_ERR_WRITE_ICON (-0x16)
#define HDD_CREATE_ERR_WRITE_PAYLOAD (-0x17)
#define HDD_CREATE_ERR_CHSTAT_SAVE (-0x18)
#define HDD_CREATE_ERR_UNMOUNT (-0x19)

/* The saved image begins with the transfer header. icon.sys occupies +0x40
 * through +0x403; the game payload starts at +0x440. The gaps remain unknown. */
struct HddSaveImage {
    struct HddTransferInfo header;
    unsigned char unmodeled_0c[0x40 - 0x0c];
    char iconSystem[MC_ICON_SYS_SIZE];
    unsigned char unmodeled_404[0x440 - 0x404];
    unsigned char payloadStart; /* First byte; the transfer header gives size. */
};

/* xglHddMount removes the high bit from this stored volume identifier. */
extern char partitionname[];
extern char commonname[];
extern char yoursaves[];
extern char pfs1RootPath[];
extern char currentDirEntryName[];
extern char parentDirEntryName[];
extern char commonname[14];
extern char yoursaves[17];
extern char xgl_hdd_device[6];
extern char hdd_mc_path[6];
extern char pfs1RootPath[8];
extern char currentDirEntryName[8];
extern char parentDirEntryName[8];
extern u8 HddActive;
extern char mount_device[6];

/* The pad records of xgl_pad.c (0x80 bytes per port). */
extern PadRecord PadData[2];

/* The GIF packet xglHddErrorScreen sends before each frame (6 quadwords on
 * DMA channel 2), and the error message it prints. */
extern u8 hddErrorScreenPacket[];
extern char hddLoadErrorText[];
/* "pfs0:/xenosaga.hdd", the installed copy of the disc file record, and
 * "xenosaga.00", the disc file whose record it must equal. */
extern char hddcheck[];
extern char cd_filename[];
/* The error codes xglHddMcCreate returns (negated) for the xglHddMcCheckCore
 * results -1 to -4 (indexed by ~checkResult), and the "1G" size argument of
 * the save partition. */
extern unsigned char hddMcCheckErrorCodes[];
extern unsigned char mcPartitionSize1G[];
extern u8 *WorkEnd;

extern int sceIoctl2(int descriptor, int command, const void *input,
                     unsigned int input_size, void *output, unsigned int output_size);
extern int sceMkdir(const char *path, int mode);
int xglHddCheck(void);
void xglHddErrorScreen(void);
int xglHddMountPartition(int mountFlags);
void decrypt(void);

/* Core HDD status probe. In this build it always reports -1; xglHddCheck2 and xglHddCheck
 * compare the result with HDD_RESET_REQUIRED (-6). */
static int xglHddCheckCore(void) {
    return -1;
}

/* Runs xglHddCheckCore. A required reset starts activation 0x103 and reports
 * HDD_NOT_READY (unreachable in this build, where the core always reports -1). */
int xglHddCheck2(void)
{
    int result = xglHddCheckCore();

    if (result == HDD_RESET_REQUIRED) {
        xglHddActivate(0x103);
        return HDD_NOT_READY;
    }
    return result;
}

/*
 * Checks the HDD. A state of 3 shows the error screen first, and a required reset
 * shows it and reactivates the HDD (the latter unreachable in this build, see
 * xglHddCheckCore).
 */
int xglHddCheck(void)
{
    int result;

    if (HddActive == 3) {
        xglHddErrorScreen();
        xglHddActivate(0);
        xglHddActivate(0x102);
    }

    result = xglHddCheckCore();
    if (result == HDD_RESET_REQUIRED) {
        xglHddErrorScreen();
        xglHddActivate(0);
        xglHddActivate(0x102);
        return HDD_NOT_READY;
    }
    return result;
}

/* Shows the HDD load error text, and returns once Circle is pressed after the wait frames. */
void xglHddErrorScreen(void)
{
    int waitFrames = HDD_ERROR_WAIT_FRAMES;

    sRender.errorScreenActive = 1;
    for (;;) {
        xglDmaDirectNormal(2, (u32)hddErrorScreenPacket, 6);
        xglFontPrint(48, 64, 0xffffff, hddLoadErrorText);
        if (waitFrames > 0) {
            waitFrames--;
        } else if (PadData[0].pressed & PAD_CIRCLE) {
            break;
        }
        xglSleep();
    }
    seVoiceStartDefault(HDD_ERROR_SE_DECIDE);
    sRender.errorScreenActive = 0;
}

int xglHddMcLoadMount(void)
{
    int result;

    if (xglHddCheck2() != 0)
        return -1;

    result = sceMount(hdd_mc_path, commonname, HDD_MOUNT_READ_ONLY, 0, 0);
    if (result < 0 && result != HDD_ERR_BUSY)
        return -1;
    return 0;
}

int xglHddMcUmount(void)
{
    int result = sceUmount(hdd_mc_path);
    return result < 0 ? -2 : 0;
}

char *make_fullpath(char *destination, int card, int slot)
{
    const char *source = yoursaves;

    while ((*destination = *source) != 0) {
        source++;
        destination++;
    }

    source = xglMcSetFullPath(card, slot);
    while ((*destination = *source) != 0) {
        source++;
        destination++;
    }
    return destination;
}

int xglHddMcExist(struct HddExistRequest *request)
{
    char filePath[HDD_PATH_MAX];
    char name[HDD_PATH_MAX];
    struct HddDirectoryEntry entry;
    char savePath[HDD_PATH_MAX];
    XglClock timestamp;
    int result;
    int descriptor;
    int save;
    int count;
    int newestSave;
    unsigned int newestTime;
    unsigned int time;
    unsigned char *found;
    char *source;
    char *destination;
    unsigned char *clearPosition;

    result = xglHddMcLoadMount();
    if (result < 0)
        return result;

    if (request->card >= 0) {
        make_fullpath(filePath, request->card, 0);
        descriptor = sceOpen(filePath, HDD_OPEN_READ, HDD_MODE_RW);
        if (descriptor >= 0) {
            result = sceClose(descriptor) < 0 ? -3 : 1;
        } else {
            result = 0;
        }
    } else {
        found = request->found;
        source = xglMcSetFullPath(-1, -1);
        source++;
        newestTime = 0;
        newestSave = 0;
        destination = name;
        *destination = *source;
        if (((unsigned int)*destination << 24) != 0) {
            do {
                source++;
                destination++;
                *destination = *source;
            } while (((unsigned int)*destination << 24) != 0);
        }
        /* The -1 card path ends in a wildcard; compare its directory stem. */
        destination[-1] = 0;

        count = 100;
        clearPosition = &found[100];
        do {
            count--;
            *clearPosition = 0;
            clearPosition--;
        } while (count >= 0);

        count = 0;
        descriptor = sceDopen(yoursaves);
        while (descriptor >= 0 && sceDread(descriptor, &entry) > 0) {
            if (strncmp(name, entry.name, 16) != 0)
                continue;
            save = entry.name[16] * 10 + entry.name[17] - (10 * '0' + '0');
            make_fullpath(savePath, save, 0);
            if (sceGetstat(savePath, &entry.stat) < 0)
                continue;
            found[save] = 1;
            timestamp.status = 0;
            timestamp.year = entry.stat.year;
            timestamp.month = entry.stat.month;
            timestamp.day = entry.stat.day;
            timestamp.hour = entry.stat.hour;
            timestamp.minute = entry.stat.minute;
            timestamp.second = entry.stat.second;
            time = xglClockDayTime2UInt(&timestamp);
            if (newestTime < time) {
                newestTime = time;
                newestSave = save;
            }
            count++;
        }
        found[100] = newestSave;
        result = count;
        if (descriptor >= 0 && sceDclose(descriptor) < 0)
            result = -5;
    }
    xglHddMcUmount();
    return result;
}

int xglHddMcLoadCore(void *save)
{
    struct HddLoadRequest *request = save;
    char path[HDD_PATH_MAX];
    int descriptor;
    int result;

    make_fullpath(path, request->card, 0);
    descriptor = sceOpen(path, HDD_OPEN_READ, HDD_MODE_RW);
    if (descriptor < 0) {
        result = descriptor == HDD_ERR_NOENT ? 1 : HDD_LOAD_ERR_OPEN;
    } else {
        result = 0;
        if (sceRead(descriptor, request->data, request->size) < 0)
            result = HDD_LOAD_ERR_READ;
        if (sceClose(descriptor) < 0)
            result = HDD_LOAD_ERR_CLOSE;
    }
    return result;
}

int xglHddMcLoad(void *save)
{
    int result;

    result = xglHddMcLoadMount();
    if (result < 0)
        return result;
    result = xglHddMcLoadCore(save);
    if (result < 0)
        return result;
    return xglHddMcUmount();
}

int Judge_MakeNewFolder(void)
{
    struct HddDirectoryEntry entry;
    int directoryCount;
    int entriesRead;
    int result;
    int descriptor;
    int closeResult;
    char *entryName;

    directoryCount = 0;
    result = sceDopen(pfs1RootPath);
    if (result < 0) {
        return result;
    }
    descriptor = result;

    for (entriesRead = 0; entriesRead < HDD_DIR_SCAN_LIMIT; entriesRead++) {
        result = sceDread(descriptor, &entry);
        if (result <= 0) {
            break;
        }

        entryName = entry.name;
        if (strcmp(entryName, currentDirEntryName) != 0 &&
            strcmp(entryName, parentDirEntryName) != 0 &&
            (entry.stat.mode & 0xF000) == 0x1000 &&
            entry.stat.privateData[0] == 0xFFFF &&
            entry.stat.privateData[1] == 0xFFFF) {
            directoryCount++;
            if (directoryCount >= HDD_MAX_FOLDERS) {
                break;
            }
        }
    }

    closeResult = sceDclose(descriptor);
    if (result >= 0) {
        result = closeResult;
    }

    if (result < 0) {
        return result;
    }
    return directoryCount < HDD_MAX_FOLDERS;
}

int Judge_MakeNewSavedata(int descriptor, int card)
{
    extern unsigned int strlen(const char *string);
    extern char *strchr(const char *string, int character);
    struct HddDirectoryEntry entry;
    int directoryCount;
    int entriesRead;
    int result;
    int closeResult;
    char *path;
    char *entryName;
    unsigned int pathLength;

    directoryCount = 0;
    result = 0;
    path = xglMcSetFullPath(card, -1) + 1;
    if (card < 0) {
        char *end = path;

        if (*path != '\0') {
            do {
                end++;
            } while (*end != '\0');
        }
        end[-1] = '\0';
    }
    pathLength = strlen(path);

    for (entriesRead = 0; entriesRead < HDD_DIR_SCAN_LIMIT; entriesRead++) {
        result = sceDread(descriptor, &entry);
        if (result <= 0)
            break;

        entryName = entry.name;
        if (strcmp(entryName, currentDirEntryName) == 0 ||
            strcmp(entryName, parentDirEntryName) == 0 ||
            strncmp(entryName, path, pathLength) == 0)
            continue;

        if ((entry.stat.mode & 0xF000) == 0x1000 &&
            (entry.stat.flags & 0x4000) == 0)
            continue;

        if (strchr(entryName, ':') != 0 &&
            (entry.stat.mode & 0xF000) == 0x2000)
            continue;

        if ((entry.stat.mode & 0xF000) == 0x4000)
            continue;
        if (entry.stat.privateData[0] != 0xFFFF)
            continue;
        if (entry.stat.privateData[1] == entry.stat.privateData[0])
            directoryCount++;
    }

    closeResult = sceDclose(descriptor);
    if (result >= 0)
        result = closeResult;

    if (result < 0)
        return result;
    return directoryCount < HDD_FOLDER_ENTRY_LIMIT;
}

static int xglHddMcCheckYourSaves(int card)
{
    int descriptor = sceDopen(yoursaves);

    if (descriptor < 0)
        return Judge_MakeNewFolder() == 1 ? 1 : HDD_CHECK_TOO_MANY_FOLDERS;
    return Judge_MakeNewSavedata(descriptor, card) == 1 ? 0 : HDD_CHECK_TOO_MANY_SAVES;
}

int xglHddMcCheckCore(struct HddCheckState *state)
{
    int result = xglHddMcCheckYourSaves(state->status);

    if (result < 0)
        return result;
    {
        int clusterSize = sceDevctl(hdd_mc_path, HDD_DEVCTL_ZONE_SIZE, 0, 0, 0, 0);
        int freeClusters = sceDevctl(hdd_mc_path, HDD_DEVCTL_FREE_ZONES, 0, 0, 0, 0);
        int dataClusters = (state->dataSize + clusterSize - 1) / clusterSize;
        int reserveClusters = ((unsigned int)clusterSize + MC_ICON_SYS_SIZE - 1) / (unsigned int)clusterSize;
        int transferClusters =
            ((unsigned int)(state->transfer.info->end - state->transfer.info->begin) + clusterSize - 1)
            / (unsigned int)clusterSize;

        dataClusters += reserveClusters;
        if (freeClusters < dataClusters + transferClusters + HDD_SLACK_ZONES) {
            int capacity;

            if (sceDevctl(xgl_hdd_device, HDD_DEVCTL_FREE_SECTORS_TOTAL, 0, 0, &capacity, 4) < 0)
                return -1;
            return capacity <= HDD_LARGE_DISK_SECTORS ? HDD_CHECK_NO_SPACE : 1;
        }
    }
    return 0;
}

int xglHddMcCheck(struct HddCheckState *state)
{
    int previous;
    int result;

    if (xglHddCheck2() != 0)
        return -1;

    xglHddMcUmount();
    if (sceMount(hdd_mc_path, commonname, HDD_MOUNT_READ_WRITE, 0, 0) < 0)
        return -1;

    previous = state->status;
    state->status = -1;
    result = xglHddMcCheckCore(state);
    state->status = previous;
    xglHddMcUmount();
    return result;
}

int xglHddMcGetFree(void)
{
    int result;
    int freeSpace;
    int clusterSize;
    int freeClusters;
    int capacity;

    result = xglHddMcLoadMount();
    if (result >= 0) {
        freeSpace = xglHddMcCheckYourSaves(-1);
        if (freeSpace >= 0) {
            if (sceDevctl(xgl_hdd_device, HDD_DEVCTL_FREE_SECTORS_TOTAL, 0, 0, &capacity, 4) == 0
                && capacity > HDD_LARGE_DISK_SECTORS) {
                freeSpace = HDD_LARGE_DISK_FREE_KB;
            } else {
                clusterSize = sceDevctl(hdd_mc_path, HDD_DEVCTL_ZONE_SIZE, 0, 0, 0, 0);
                freeClusters = sceDevctl(hdd_mc_path, HDD_DEVCTL_FREE_ZONES, 0, 0, 0, 0);
                freeSpace = (freeClusters < 0 ? 0 : freeClusters) * (clusterSize / 1024);
            }
        }
        xglHddMcUmount();
        result = freeSpace;
    }
    return result;
}

int create_file(int card, int slot, const void *data, int size)
{
    char path[HDD_PATH_MAX];
    struct HddIoStat stat;
    int descriptor;
    int result;

    make_fullpath(path, card, slot);
    descriptor = sceOpen(path, HDD_OPEN_WRITE_CREATE_TRUNC, HDD_MODE_RW);
    if (descriptor < 0)
        return -1;

    result = 0;
    if (sceWrite(descriptor, data, size) < 0)
        result = -1;
    if (sceClose(descriptor) < 0)
        result = -1;
    stat.attributes = 0x8497;
    if (sceChstat(path, &stat, 2) < 0)
        result = -1;
    return result;
}

/*
 * Creates the save folder for state->status and writes its icon and payload files.
 * Returns 0 or a negative failure code.
 */
int xglHddMcCreate(struct HddCheckState *state)
{
    char *deviceName;
    char path[HDD_PATH_MAX];
    struct HddIoStat stat;
    int result;
    int descriptor;
    int remountFailure;
    struct HddSaveImage *transfer;
    int checkResult;

    result = -1;
    xglHddMcUmount();
    if (sceMount(hdd_mc_path, commonname, HDD_MOUNT_READ_WRITE, 0, 0) < 0) {
        return -1;
    }

    make_fullpath(path, state->status, -1);
    if (state->status >= 0 && (descriptor = sceDopen(path)) >= 0) {
        if (sceDclose(descriptor) < 0) {
            result = HDD_CREATE_ERR_SAVE_DIR_CLOSE;
            goto unmount_card;
        }
    } else {
        checkResult = xglHddMcCheckCore(state);
        if (checkResult < 0) {
            result = -(int)hddMcCheckErrorCodes[~checkResult];
            xglHddMcUmount();
            return result;
        }
        if (checkResult == 1) {
            if (xglHddMcUmount() < 0) {
                return HDD_CREATE_ERR_UNMOUNT;
            }
            remountFailure = -1;
            deviceName = commonname;
            descriptor = sceOpen(deviceName, HDD_OPEN_READ_WRITE);
            if (descriptor < 0) {
                return HDD_CREATE_ERR_OPEN_DEVICE;
            }

            result = 0;
            if (sceIoctl2(descriptor, HDD_IOCTL2_ADD_SUBPARTITION, mcPartitionSize1G, 3, 0, 0) < 0) {
                result = HDD_CREATE_ERR_ADD_PARTITION;
            }
            if (sceClose(descriptor) < 0) {
                result = HDD_CREATE_ERR_CLOSE_DEVICE;
            }
            if (result != 0) {
                return result;
            }
            if (sceMount(hdd_mc_path, commonname, HDD_MOUNT_READ_WRITE, 0, 0) < 0) {
                return remountFailure;
            }
            result = 0;
        }

        descriptor = sceDopen(yoursaves);
        if (descriptor < 0) {
            if (sceMkdir(yoursaves, HDD_MODE_DIRECTORY) < 0) {
                result = HDD_CREATE_ERR_MKDIR_ROOT;
                goto unmount_card;
            }
        } else if (sceDclose(descriptor) < 0) {
            result = HDD_CREATE_ERR_CLOSE_ROOT;
            goto unmount_card;
        }

        if (state->status < 0) {
            xglHddMcUmount();
            return result;
        }

        descriptor = sceDopen(path);
        if (descriptor < 0 && descriptor != HDD_ERR_NOENT) {
            result = HDD_CREATE_ERR_OPEN_SAVE_DIR;
            goto unmount_card;
        }

        if (descriptor == HDD_ERR_NOENT) {
            if (sceMkdir(path, HDD_MODE_DIRECTORY) < 0) {
                result = HDD_CREATE_ERR_MKDIR_SAVE;
                goto unmount_card;
            }
            stat.attributes = 0xc4a7;
            if (sceChstat(path, &stat, 2) < 0) {
                result = HDD_CREATE_ERR_CHSTAT_SAVE;
                goto unmount_card;
            }
        } else {
            result = HDD_CREATE_ERR_SAVE_DIR_CLOSE;
            if (sceDclose(descriptor) < 0) {
                goto unmount_card;
            }
        }
    }

    result = HDD_CREATE_ERR_WRITE_ICON;
    transfer = state->transfer.image;
    if (create_file(state->status, 1, transfer->iconSystem, MC_ICON_SYS_SIZE) >= 0) {
        result = HDD_CREATE_ERR_WRITE_PAYLOAD;
        if (create_file(state->status, 2, &transfer->payloadStart,
                        transfer->header.end - transfer->header.begin) >= 0) {
            result = 0;
        }
    }

unmount_card:
    xglHddMcUmount();
    return result;
}

int xglHddMcSave(const struct HddSaveRequest *save)
{
    int result;

    if (xglHddCheck2() != 0)
        return -1;
    if (sceMount(hdd_mc_path, commonname, HDD_MOUNT_READ_WRITE, 0, 0) < 0)
        return -1;

    result = create_file(save->card, 0, save->data, save->size) < 0 ? HDD_SAVE_ERR_WRITE : 0;
    xglHddMcUmount();
    return result;
}

/* Mounts the game partition (mount_device, "pfs0:") with the given sceMount flags. */
int xglHddMountPartition(int mountFlags) {
    return sceMount(mount_device, partitionname, mountFlags, 0, 0U);
}

int xglHddUninstall(void)
{
    int result;

    xglHddActivate(0);
    result = sceUmount(mount_device);
    if (result < 0 && result != HDD_ERR_NODEV)
        return -1;
    result = sceRemove(partitionname);
    return result < 0 ? -5 : 0;
}

/* The progress block at hddInstallContext that xglHddInstallReadCB updates: the
 * installed callback receives a stage, an offset and `param`. */
struct HddInstallCBParam {
    int (*callback)(int stage, int offset, void *param);
    int base;
    void *param;
    int status;
    int total;
};

extern struct HddInstallCBParam *hddInstallContext;

void xglHddInstallReadCB(int event, int value, int tag) {
    struct HddInstallCBParam *cb;
    int result;

    cb = hddInstallContext;
    if (event == 1) {
        cb->total = value + 1;
        return;
    }
    if (event == 2) {
        result = cb->callback(6, cb->base * 0x10 + (value * 0x10) / cb->total, cb->param);
        cb->status = result;
        if (result != 0) {
            xglCdReadCancel(0);
            return;
        }
        return;
    }
}

#define HDD_OPEN_READ_WRITE_CREATE 0x203
#define HDD_MODE_RW_FILE 0x1b6
#define HDD_SYSTEM_CNF_SIZE 80
#define HDD_INSTALL_IMAGE_ADDRESS 0x1000000
#define HDD_INSTALL_ARCHIVE_SIZE 0x1000000
#define HDD_INSTALL_PARTITION_STEP 0x80000
/* Progress stages and failures xglHddInstall reports to its callback. */
#define HDD_INSTALL_STAGE_BEGIN 0
#define HDD_INSTALL_STAGE_RESOURCE_READ 1
#define HDD_INSTALL_STAGE_PARTITION_OPEN 2
#define HDD_INSTALL_STAGE_EXTEND 3
#define HDD_INSTALL_STAGE_FORMAT 4
#define HDD_INSTALL_STAGE_MOUNT 5
#define HDD_INSTALL_STAGE_COPY 6
#define HDD_INSTALL_STAGE_FINISH 7
#define HDD_INSTALL_STAGE_DONE 8
#define HDD_INSTALL_FAIL_RESOURCE (-1)
#define HDD_INSTALL_FAIL_PARTITION_OPEN (-2)
#define HDD_INSTALL_FAIL_WRITE_HEADER (-3)
#define HDD_INSTALL_FAIL_EXTEND (-4)
#define HDD_INSTALL_FAIL_PARTITION_CLOSE (-5)
#define HDD_INSTALL_FAIL_FORMAT (-6)
#define HDD_INSTALL_FAIL_MOUNT (-7)
#define HDD_INSTALL_FAIL_DATA_OPEN (-8)
#define HDD_INSTALL_FAIL_DATA_WRITE (-9)
#define HDD_INSTALL_FAIL_DATA_CLOSE (-10)
#define HDD_INSTALL_FAIL_CHECK_OPEN (-11)
#define HDD_INSTALL_FAIL_CHECK_WRITE (-12)
#define HDD_INSTALL_FAIL_CHECK_CLOSE (-13)
#define HDD_INSTALL_FAIL_UNMOUNT (-14)
#define HDD_INSTALL_FAIL_REMOUNT (-15)

/* The install image staged at the fixed work address: the offset and size of
 * its last entry at +0x28 / +0x2c (their sum is the image size), and the
 * 80-byte SYSTEM.CNF copy at +0x200. */
struct HddInstallImage {
    unsigned char unmodeled_00[0x28];
    int lastEntryOffset;
    int lastEntrySize;
    unsigned char unmodeled_30[0x1d0];
    unsigned char systemCnf[HDD_SYSTEM_CNF_SIZE];
};

extern int sprintf(char *buffer, const char *format, ...);
extern int sceFormat(const char *mount_point, const char *device, void *payload, int payload_length);
int xglHddDummyCallback(int stage, int offset, void *param);
/* The resource file, the partition spec format, the 256 MB sub-partition
 * size, the pfs device, the installed data file and the disc archive name
 * (xenosaga.0N, digit at [10]). */
extern char hddResourcePath[];
extern char partitionSpecFormat[];
extern char hddPartitionSize256M[];
extern char pfsDevicePath[];
extern char hddname[];
extern char dvdname_2[];

/* Installs the game data on the HDD: writes the hdd.res image (with this
 * SYSTEM.CNF) into a new 256 MB partition, adds 256 MB sub-partitions up to the
 * minimum size, formats and mounts it, copies the xenosaga.0N archives in 16 MB pieces and
 * writes the check file. Progress and failures go to callback (stages and
 * HDD_INSTALL_FAIL_* above). Returns 0 on success, 1 when the copy was
 * cancelled and -1 on failure. */
int xglHddInstall(int (*callback)(int stage, int offset, void *param), void *param)
{
    struct HddInstallImage *image = (struct HddInstallImage *)HDD_INSTALL_IMAGE_ADDRESS;
    struct HddInstallCBParam context;
    char path[HDD_PATH_MAX];
    int formatSize;
    int partition;
    int descriptor;
    int result;
    int size;
    int chunk;
    u8 *source;
    int i;
    int archives;
    int offset;
    int length;

    if (callback == 0) {
        callback = xglHddDummyCallback;
    }
    hddInstallContext = &context;
    context.callback = callback;
    context.param = param;
    context.status = 0;
    callback(HDD_INSTALL_STAGE_BEGIN, 0, param);

    /* Read the install image, then copy SYSTEM.CNF into it. */
    if (xglCdReadFile(hddResourcePath, image, 0, 1) < 0) {
        callback(HDD_INSTALL_FAIL_RESOURCE, 0, param);
        return -1;
    }
    source = system_cnf;
    for (i = 0; i < HDD_SYSTEM_CNF_SIZE; i++) {
        image->systemCnf[i] = source[i];
    }
    size = (image->lastEntryOffset + image->lastEntrySize + HDD_SECTOR_SIZE - 1) & -HDD_SECTOR_SIZE;

    /* Create the partition and extend it in 256M steps. */
    callback(HDD_INSTALL_STAGE_RESOURCE_READ, 0, param);
    sprintf(path, partitionSpecFormat, partitionname);
    partition = sceOpen(path, HDD_OPEN_READ_WRITE_CREATE);
    if (partition < 0) {
        callback(HDD_INSTALL_FAIL_PARTITION_OPEN, partition, param);
        return -1;
    }
    callback(HDD_INSTALL_STAGE_PARTITION_OPEN, 0, param);
    if ((result = sceWrite(partition, image, size)) < 0) {
        callback(HDD_INSTALL_FAIL_WRITE_HEADER, result, param);
        xglHddUninstall();
        return -1;
    }

    for (size = HDD_INSTALL_PARTITION_STEP, chunk = 1; size <= HDD_INSTALL_MIN_SECTORS;
         size += HDD_INSTALL_PARTITION_STEP, chunk++) {
        callback(HDD_INSTALL_STAGE_EXTEND, chunk, param);
        result = sceIoctl2(partition, HDD_IOCTL2_ADD_SUBPARTITION, hddPartitionSize256M, 5, 0, 0);
        if (result < 0) {
            callback(HDD_INSTALL_FAIL_EXTEND, result, param);
            xglHddUninstall();
            return -1;
        }
    }
    if ((result = sceClose(partition)) < 0) {
        callback(HDD_INSTALL_FAIL_PARTITION_CLOSE, result, param);
        return -1;
    }

    /* Format and mount the new partition. */
    callback(HDD_INSTALL_STAGE_FORMAT, 0, param);
    formatSize = 0x2000;
    if ((result = sceFormat(pfsDevicePath, partitionname, &formatSize, 4)) < 0) {
        callback(HDD_INSTALL_FAIL_FORMAT, result, param);
        xglHddUninstall();
        return -1;
    }
    callback(HDD_INSTALL_STAGE_MOUNT, 0, param);
    if ((result = xglHddMountPartition(HDD_MOUNT_READ_WRITE)) < 0) {
        callback(HDD_INSTALL_FAIL_MOUNT, result, param);
        xglHddUninstall();
        return -1;
    }
    callback(HDD_INSTALL_STAGE_COPY, 0, param);
    descriptor = sceOpen(hddname, HDD_OPEN_WRITE_CREATE_TRUNC, HDD_MODE_RW_FILE);
    if (descriptor < 0) {
        result = descriptor;
        callback(HDD_INSTALL_FAIL_DATA_OPEN, result, param);
        xglHddUninstall();
        return -1;
    }

    /* Count the archive parts on the disc, then copy each part to the hard disk
       in pieces of at most 16 MB. */
    archives = 0;
    dvdname_2[10] = '0';
    while ((size = xglCdGetFileSize(dvdname_2)) >= 0) {
        archives += (size + HDD_INSTALL_ARCHIVE_SIZE - 1) / HDD_INSTALL_ARCHIVE_SIZE;
        dvdname_2[10]++;
    }
    /* Move the total piece count into the high half: context.base starts at
       count << 16 and grows by one per piece, so the offset that
       xglHddInstallReadCB reports (base * 0x10 plus sixteenths of the current
       piece) carries the total in bits 20 and up and the progress in
       sixteenths of a piece below them. */
    archives <<= 16;

    dvdname_2[10] = '0';
    while ((size = xglCdGetFileSize(dvdname_2)) >= 0) {
        for (offset = 0; offset < size;) {
            length = size - offset;
            if (length > HDD_INSTALL_ARCHIVE_SIZE) {
                length = HDD_INSTALL_ARCHIVE_SIZE;
            }
            context.base = archives++;
            xglCdReadFilePart(dvdname_2, image, CD_READ_NO_EXTRACT, xglHddInstallReadCB, offset, length);
            offset += length;
            if (context.status != 0) {
                sceClose(descriptor);
                if (xglHddCheck() == 0) {
                    xglHddUninstall();
                    return 1;
                }
                return context.status == 1 ? 1 : -1;
            }
            result = sceWrite(descriptor, image, length);
            if (result != length) {
                callback(HDD_INSTALL_FAIL_DATA_WRITE, result, param);
                sceClose(descriptor);
                if (xglHddCheck() == 0) {
                    xglHddUninstall();
                }
                return -1;
            }
        }
        dvdname_2[10]++;
    }

    if ((result = sceClose(descriptor)) < 0) {
        callback(HDD_INSTALL_FAIL_DATA_CLOSE, result, param);
        xglHddUninstall();
        return -1;
    }
    /* Write the check file, then remount the partition read-only. */
    callback(HDD_INSTALL_STAGE_FINISH, 0, param);
    descriptor = sceOpen(hddcheck, HDD_OPEN_WRITE_CREATE_TRUNC, HDD_MODE_RW_FILE);
    if (descriptor < 0) {
        result = descriptor;
        callback(HDD_INSTALL_FAIL_CHECK_OPEN, result, param);
        xglHddUninstall();
        return -1;
    }
    callback(HDD_INSTALL_STAGE_FINISH, 1, param);
    xglCdCheckFile(cd_filename, image);
    result = sceWrite(descriptor, image, HDD_SECTOR_SIZE);
    if (result != HDD_SECTOR_SIZE) {
        callback(HDD_INSTALL_FAIL_CHECK_WRITE, result, param);
        sceClose(descriptor);
        xglHddUninstall();
        return -1;
    }
    callback(HDD_INSTALL_STAGE_FINISH, 2, param);
    if ((result = sceClose(descriptor)) < 0) {
        callback(HDD_INSTALL_FAIL_CHECK_CLOSE, result, param);
        xglHddUninstall();
        return -1;
    }
    callback(HDD_INSTALL_STAGE_FINISH, 3, param);
    if ((result = sceUmount(mount_device)) < 0) {
        callback(HDD_INSTALL_FAIL_UNMOUNT, result, param);
        xglHddUninstall();
        return -1;
    }
    callback(HDD_INSTALL_STAGE_FINISH, 4, param);
    if ((result = xglHddMountPartition(HDD_MOUNT_READ_ONLY)) < 0) {
        callback(HDD_INSTALL_FAIL_REMOUNT, result, param);
        xglHddUninstall();
        return -1;
    }
    callback(HDD_INSTALL_STAGE_DONE, 0, param);
    return 0;
}

int xglHddActivate(int state)
{
    int previous = HddActive;

    if (state != -1) {
        if (previous != 2 && (state & 0xff) < 4 && (state & 0xff) >= 0) {
            HddActive = (u8)state;
            if (state < 0x100)
                xglCdArcCheck();
        }
    }
    return previous < 2 ? previous : 0;
}

void decrypt(void)
{
    u8 *source = partitionname;
    int value = *source;
    register int masked = value & 0xff;
    register u8 *destination = source;

    if (masked != 0x68) {
        if (masked != 0) {
            do {
                *destination = (unsigned int)value ^ 0x80;
                source++;
                value = *source;
                destination++;
            } while (value != 0);
        }
    }
    partitionname[17] = system_cnf[0x1a];
}

int xglHddMount(void)
{
    int mount_result;
    int readiness;
    int device_result;
    int close_result;
    int capacity;
    int descriptor;
    int mismatch;
    int index;
    u8 *buffer;
    u8 *reference;

    decrypt();
    if (HddActive >= 2) {
        return -1;
    }

    HddActive = 0;
    readiness = xglHddCheck();
    if (readiness != 0) {
        return readiness == HDD_NOT_READY ? -1 : readiness;
    }

    mount_result = xglHddMountPartition(HDD_MOUNT_READ_ONLY);
    if (mount_result == -5) {
        return mount_result;
    }
    if (mount_result < 0 && mount_result != HDD_ERR_BUSY) {
        if (mount_result == HDD_ERR_NOENT) {
            device_result = sceDevctl(xgl_hdd_device, HDD_DEVCTL_FREE_SECTORS_TOTAL, 0, 0,
                                      &capacity, 4);
            if (device_result < 0) {
                return -2;
            }
            if (capacity <= HDD_INSTALL_MIN_SECTORS) {
                return -4;
            }
            device_result = sceDevctl(xgl_hdd_device, HDD_DEVCTL_MAX_SECTORS, 0, 0, 0, 0);
            if (device_result <= HDD_INSTALL_MIN_FREE_SECTORS) {
                return -4;
            }
            return 0;
        }
        return -2;
    }

    descriptor = sceOpen(hddcheck, HDD_OPEN_READ, HDD_MODE_READ_ONLY);
    if (descriptor < 0) {
        return -5;
    }

    buffer = (u8 *)(((u32)WorkEnd + 63) & (u32)-64);
    reference = buffer + HDD_SECTOR_SIZE;
    buffer[0] = 1;
    reference[0] = 2;
    mismatch = 0;
    if (sceRead(descriptor, buffer, HDD_SECTOR_SIZE) < 0) {
        mismatch = 1;
    } else {
        xglCdCheckFile(cd_filename, reference);
        for (index = 0; index < HDD_CD_RECORD_SIZE; index++) {
            if (buffer[index] != reference[index]) {
                mismatch = 1;
                break;
            }
        }
    }
    close_result = sceClose(descriptor);
    if (close_result < 0 || mismatch == 1) {
        return -5;
    }
    HddActive = 1;
    return 1;
}
