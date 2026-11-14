#include "common.h"
#include "main/xgl_cd.h"

/* Host-link constants: chunk size of one transfer, timeouts in ms (-1 waits forever), and the byte
 * lengths of the command verbs sent to the host ("SIZE ", "GET ", "PUT ", "SEND ", "RECV ", "CLOSE"). */
enum {
    LAN_CHUNK_SIZE = 0x4000,
    LAN_CONNECT_TIMEOUT = 5000,
    LAN_CLOSE_TIMEOUT = 1000,
    LAN_NO_TIMEOUT = -1,
    LAN_STATUS_X = 16,
    LAN_STATUS_Y = 16,
    LAN_VERB_SIZE_LEN = 5,
    LAN_VERB_GET_LEN = 4,
    LAN_VERB_PUT_LEN = 4,
    LAN_VERB_SEND_LEN = 5,
    LAN_VERB_RECV_LEN = 5,
    LAN_VERB_CLOSE_LEN = 6,
    LAN_SIZE_FIELD_BYTES = 4,
    NET_HOST_NAME_SIZE = 16
};

/* Network adapter that UsbInit brings up; netInitState holds the device plus one. */
enum {
    NET_DEVICE_USB = 0,   /* usbd + an986 */
    NET_DEVICE_SMAP = 1   /* smap */
};

/* Connect request handed to func_0029FCD8: the resolved host address and the port. */
typedef struct LanConnectRequest {
    int mode;                   /* +0x00 */
    int reserved;               /* +0x04: -1 */
    int address[4];             /* +0x08: filled by func_0029FAA8 from the host name */
    int port;                   /* +0x18 */
    int unused[9];
} LanConnectRequest;

int func_0029FD90(void *, int, int, int);
int func_0029FFB0(void *, int, int, const void *, int, int *, int);
unsigned int strlen(const char *);
int printf(const char *, ...);
char *strcpy(char *, const char *);
void xglFontDebugPrintf(int, int, const char *, ...);
extern void (*netStatusHandler)(void);
extern int netWork;
extern const char D_0068E448[];
extern const char D_0068E458[];
extern const char D_0068E480[];
extern const char D_0068E490[];
extern const char netCloseCommand[];
extern const char netGetCommand[];
extern const char netRecvCommand[];
extern unsigned char netContext[];
extern const char netSizeCommand[];
extern char netHostName[];
extern int netInitState;
extern int netHostConfigured;
extern int netHostPort;

/* Shows a step label through the debug font, then runs the status handler (netStatusHandler) when one is set. */
void LanReportStatus(const char *message) {
    if (netStatusHandler != 0) {
        xglFontDebugPrintf(LAN_STATUS_X, LAN_STATUS_Y, message);
        netStatusHandler();
    }
}

int func_0029FDD8(void *, int, int, void *, int, int *, int);
int func_0029FAA8(void *, int, int, int *, char *, int, int);
int func_0029FCD8(void *, int, LanConnectRequest *);
int func_0029FD48(void *, int, int, int);
extern const char D_0068E3B8[];
extern const char D_0068E3C8[];
extern const char D_0068E3E8[];
extern const char D_0068E3F8[];
extern const char D_0068E418[];
extern const char D_0068E428[];
extern const char D_0068E468[];
extern const char netLocalAddressText[];
extern unsigned char netLocalAddress[];

int LanOpen(void) {
    LanConnectRequest block;
    int handle;
    int result;

    LanReportStatus(D_0068E3B8);
    memset(&block, 0, sizeof(block));
    block.mode = 1;
    block.reserved = -1;
    if (func_0029FAA8(netContext, netWork, 0, block.address, netHostName, -1, -1) != 0) {
        printf(D_0068E3C8);
    }
    block.port = netHostPort;
    LanReportStatus(D_0068E3E8);
    handle = func_0029FCD8(netContext, netWork, &block);
    if (handle <= 0) {
        printf(D_0068E3F8, handle);
    }
    LanReportStatus(D_0068E418);
    result = func_0029FD48(netContext, netWork, handle, LAN_CONNECT_TIMEOUT);
    if (result != 0) {
        printf(D_0068E428, result);
        func_0029FD90(netContext, netWork, handle, LAN_CLOSE_TIMEOUT);
        return -1;
    }
    return handle;
}

/* Closes the handle with a one second timeout; prints the error when the close fails. */
void LanCloseHandle(int handle) {
    int status;

    status = func_0029FD90(netContext, netWork, handle, LAN_CLOSE_TIMEOUT);
    if (status < 0) {
        printf(D_0068E448, status);
    }
}

/* Sends the CLOSE verb, then closes the handle. */
void LanClose(int handle) {
    int closeReply;

    LanReportStatus(D_0068E458);
    closeReply = 0;
    func_0029FFB0(netContext, netWork, handle, netCloseCommand, LAN_VERB_CLOSE_LEN, &closeReply, LAN_NO_TIMEOUT);
    LanCloseHandle(handle);
}

/* Receives the requested bytes in chunks of at most 0x4000, calling the optional hook after each chunk. */
int LanReceive(int handle, void *buffer, int size, void (*progressHook)(void)) {
    unsigned int bytesLeft;
    int chunkSize;
    int recvStatus;
    int received;

    bytesLeft = size;
    while (bytesLeft != 0) {
        chunkSize = (bytesLeft <= LAN_CHUNK_SIZE) ? bytesLeft : LAN_CHUNK_SIZE;
        recvStatus = 0;
        received = func_0029FDD8(netContext, netWork, handle, buffer, chunkSize, &recvStatus, LAN_NO_TIMEOUT);
        bytesLeft -= received;
        if (buffer != 0) {
            buffer += received;
        }
        if (progressHook != 0) {
            xglFontDebugPrintf(LAN_STATUS_X, LAN_STATUS_Y, D_0068E468, buffer, bytesLeft, size, received);
            progressHook();
        }
    }
    return 0;
}

/* SIZE: sends the path and returns the size the host reports, or -1 when no connection opens. */
int UsbGetFileSize(const char *path) {
    int hostFileSize;
    int handle;
    int sendStatus;

    hostFileSize = -1;
    handle = LanOpen();
    if (handle > 0) {
        LanReportStatus(D_0068E480);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, netSizeCommand, LAN_VERB_SIZE_LEN, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        LanReportStatus(D_0068E490);
        LanReceive(handle, &hostFileSize, LAN_SIZE_FIELD_BYTES, 0);
        LanClose(handle);
    }
    return hostFileSize;
}

/* GET: sends the path, reads the file size, then receives the file into buffer. */
int UsbReadFile(const char *path, void *buffer) {
    int sendStatus;
    int fileSize;
    int handle;

    fileSize = -1;
    handle = LanOpen();
    if (handle > 0) {
        LanReportStatus(D_0068E480);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, netGetCommand, LAN_VERB_GET_LEN, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        LanReportStatus(D_0068E490);
        LanReceive(handle, &fileSize, LAN_SIZE_FIELD_BYTES, 0);
        LanReceive(handle, buffer, fileSize, netStatusHandler);
        LanClose(handle);
    }
    return fileSize;
}

int sprintf(char *, const char *, int, int);
extern const char D_0068E4D8[];
extern const char netSendCommand[];
extern const char netPutCommand[];
extern const char D_0068E4A8[];
extern const char D_0068E4C0[];

/* PUT: sends the path, the size and the body in chunks of at most 0x4000. */
int UsbWrite(const char *path, void *buffer, int size) {
    char progressText[256];
    int sendStatus;
    int handle;
    int chunkSize;
    int sentBytes;
    unsigned int bytesLeft;
    void *cursor;

    handle = LanOpen();
    if (handle > 0) {
        LanReportStatus(D_0068E4A8);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, netPutCommand, LAN_VERB_PUT_LEN, &sendStatus, LAN_NO_TIMEOUT);
        cursor = buffer;
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        LanReportStatus(D_0068E4C0);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, &size, LAN_SIZE_FIELD_BYTES, &sendStatus, LAN_NO_TIMEOUT);
        bytesLeft = size;
        while (bytesLeft != 0) {
            chunkSize = (bytesLeft <= LAN_CHUNK_SIZE) ? bytesLeft : LAN_CHUNK_SIZE;
            sendStatus = 0;
            sentBytes = func_0029FFB0(netContext, netWork, handle, cursor, chunkSize, &sendStatus, LAN_NO_TIMEOUT);
            bytesLeft -= sentBytes;
            cursor += sentBytes;
            sprintf(progressText, D_0068E4D8, bytesLeft, size);
            LanReportStatus(progressText);
        }
        LanClose(handle);
    }
    return size;
}

/* SEND: the PUT protocol with the SEND verb. */
int UsbSend(const char *path, void *buffer, int size) {
    char progressText[256];
    int sendStatus;
    int handle;
    int chunkSize;
    int sentBytes;
    unsigned int bytesLeft;
    void *cursor;

    handle = LanOpen();
    if (handle > 0) {
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, netSendCommand, LAN_VERB_SEND_LEN, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, &size, LAN_SIZE_FIELD_BYTES, &sendStatus, LAN_NO_TIMEOUT);
        bytesLeft = size;
        cursor = buffer;
        if (bytesLeft != 0) {
            do {
                chunkSize = (bytesLeft <= LAN_CHUNK_SIZE) ? bytesLeft : LAN_CHUNK_SIZE;
                sendStatus = 0;
                sentBytes = func_0029FFB0(netContext, netWork, handle, cursor, chunkSize, &sendStatus, LAN_NO_TIMEOUT);
                bytesLeft -= sentBytes;
                cursor += sentBytes;
                sprintf(progressText, D_0068E4D8, bytesLeft, size);
                LanReportStatus(progressText);
            } while (bytesLeft != 0);
        }
        LanClose(handle);
    }
    return size;
}

/* RECV: sends the path, then receives at most requested bytes into buffer and discards the rest. */
int UsbRecv(const char *path, void *buffer, int maxBytes) {
    int sendStatus;
    int size;
    int excess;
    int handle;

    handle = LanOpen();
    if (handle > 0) {
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, netRecvCommand, LAN_VERB_RECV_LEN, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        func_0029FFB0(netContext, netWork, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        LanReceive(handle, &size, LAN_SIZE_FIELD_BYTES, 0);
        excess = size - maxBytes;
        if (maxBytes >= size) {
            excess = 0;
            maxBytes = size;
        }
        if (size > 0) {
            LanReceive(handle, buffer, maxBytes, 0);
            if (excess > 0) {
                LanReceive(handle, 0, excess, 0);
            }
        }
        LanClose(handle);
    }
    return size;
}

/* Returns the initialization state: zero until UsbInit has completed. */
int UsbGetInitState(void) {
    return netInitState;
}

/* Once the network stack is initialized: copies the default host name into name, stores the device (init state minus one) into device, and returns the word at offset 4 of the default block. */
int UsbGetLocalAddress(char *name, int *device) {
    if (netInitState == 0) {
        return 0;
    }
    if (name != 0) {
        strcpy(name, netLocalAddressText);
    }
    if (device != 0) {
        *device = netInitState - 1;
    }
    return ((netLocalAddress[7] << 24) + (netLocalAddress[5] << 8)) + (netLocalAddress[4] + (netLocalAddress[6] << 16));
}

/* Stores the host name and its port and marks the host configured; a name of 16 or more characters is refused. */
int UsbSetHost(const char *name, int port) {
    if (strlen(name) >= NET_HOST_NAME_SIZE) {
        return -1;
    }
    strcpy(netHostName, name);
    netHostPort = port;
    netHostConfigured = 1;
    return 0;
}

/* Returns the configured flag; when it is set, copies the host name and its port out. */
int UsbGetHost(char *name, int *port) {
    if (netHostConfigured != 0) {
        strcpy(name, netHostName);
        *port = netHostPort;
    } else {
        name[0] = '\0';
        *port = 0;
    }
    return netHostConfigured;
}

extern int WorkEnd;
extern const char D_0069E030[];
extern const char D_0069E038[];
extern const char D_0069E040[];
extern const char D_0069E048[];
extern const char D_0069E050[];
extern const char D_0069E058[];
extern const char D_0069E060[];
extern const char D_0069E068[];
extern const char D_00645208[];
extern const char D_00645278[];
extern const char D_006452B0[];
extern const char D_0068E4F0[];
extern const char D_0068E508[];
extern const char D_0068E528[];
extern const char D_0068E538[];
void func_00122D68(int);
void func_0029F008(int);
int func_002A06F0(void *, int, int, int);
int func_002A0848(void *, int);
int func_002A0C80(void *, int, void *, int, void *, int);
int func_0029FB68(void *, int, const char *, int, void *);

/* Stores the status handler, loads the network IRX modules and starts the stack once. */
int UsbInit(void (*statusHandler)(void), int device) {
    int linkReply[4];
    int netWorkBase;

    netStatusHandler = statusHandler;
    if (device < 0 || netInitState != 0) {
        return 0;
    }
    netInitState = 0;
    netWorkBase = (WorkEnd + 64) & -64;
    netWork = netWorkBase;
    WorkEnd = netWorkBase + 0x8010;
    func_00122D68(0);
    xglCdSifLoadModule(D_0069E030, 0);
    xglCdSifLoadModule(D_0069E038, D_00645208);
    if (device == NET_DEVICE_USB) {
        xglCdSifLoadModule(D_0069E040, D_00645278);
        xglCdSifLoadModule(D_0069E048, 0);
        xglCdSifLoadModule(D_0069E050, 0);
    } else {
        xglCdSifLoadModule(D_0069E040, D_006452B0);
        xglCdSifLoadModule(D_0069E058, 0);
    }
    xglCdSifLoadModule(D_0069E060, 0);
    xglCdSifLoadModule(D_0069E068, 0);
    func_0029F008(0);
    func_002A06F0(netContext, 16448, 8192, 32);
    if (func_002A0848(netContext, netWork) < 0) {
        printf(D_0068E4F0);
    }
    memset(netLocalAddress, 0, 16);
    if (func_002A0C80(netContext, netWork, linkReply, 1, netLocalAddress, 2) < 0) {
        printf(D_0068E508);
    }
    if (func_0029FB68(netContext, netWork, netLocalAddressText, 16, netLocalAddress) != 0) {
        printf(D_0068E528);
    }
    printf(D_0068E538, netLocalAddressText);
    netInitState = device + 1;
    return 0;
}
