#include "common.h"
#include "main/xgl_cd.h"
#include "main/xgl_font.h"
#include "main/control_entry.h"

/* Host-link constants: chunk size of one transfer, timeouts in ms (-1 waits forever), and the byte
 * lengths of the command verbs sent to the host ("SIZE ", "GET ", "PUT ", "SEND ", "RECV ", "CLOSE"). */
static char netLocalAddressText[16];
static unsigned char netLocalAddress[16];
static char netHostName[16];
static unsigned char netContext[64];
static int netInetWorkArea;
static void (*netStatusHandler)(void);
static struct {
    int port;
    int unmodeled_04;
} netHostPort;

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

/* Connect request handed to sceInetCreate: the resolved host address and the port. */
typedef struct LanConnectRequest {
    int mode;                   /* +0x00 */
    int reserved;               /* +0x04: -1 */
    int address[4];             /* +0x08: filled by sceInetName2Address from the host name */
    int port;                   /* +0x18 */
    int unused[9];
} LanConnectRequest;

int sceInetClose(void *, int, int, int);
int sceInetSend(void *, int, int, const void *, int, int *, int);
unsigned int strlen(const char *);
int printf(const char *, ...);
char *strcpy(char *, const char *);
extern const char lanCloseErrorFormat[];
extern const char lanCloseStatusText[];
extern const char usbReadRequestStatusText[];
extern const char usbReadSizeStatusText[];
extern const char netCloseCommand[];
extern const char netGetCommand[];
extern const char netRecvCommand[];
extern const char netSizeCommand[];
extern int netInitState;
extern int netHostConfigured;

/* Shows a step label through the debug font, then runs the status handler (netStatusHandler) when one is set. */
void LanReportStatus(const char *message) {
    if (netStatusHandler != 0) {
        xglFontDebugPrintf(LAN_STATUS_X, LAN_STATUS_Y, message);
        netStatusHandler();
    }
}

int sceInetRecv(void *, int, int, void *, int, int *, int);
int sceInetName2Address(void *, int, int, int *, char *, int, int);
int sceInetCreate(void *, int, LanConnectRequest *);
int sceInetOpen(void *, int, int, int);
extern const char lanOpenSearchStatusText[];
extern const char lanName2AddressFailedText[];
extern const char lanOpenConnectStatusText[];
extern const char lanInetCreateResultFormat[];
extern const char lanOpenOpenStatusText[];
extern const char lanInetOpenFailedFormat[];
extern const char lanReceiveProgressFormat[];

int LanOpen(void) {
    LanConnectRequest block;
    int handle;
    int result;

    LanReportStatus(lanOpenSearchStatusText);
    memset(&block, 0, sizeof(block));
    block.mode = 1;
    block.reserved = -1;
    if (sceInetName2Address(netContext, netInetWorkArea, 0, block.address, netHostName, -1, -1) != 0) {
        printf(lanName2AddressFailedText);
    }
    block.port = netHostPort.port;
    LanReportStatus(lanOpenConnectStatusText);
    handle = sceInetCreate(netContext, netInetWorkArea, &block);
    if (handle <= 0) {
        printf(lanInetCreateResultFormat, handle);
    }
    LanReportStatus(lanOpenOpenStatusText);
    result = sceInetOpen(netContext, netInetWorkArea, handle, LAN_CONNECT_TIMEOUT);
    if (result != 0) {
        printf(lanInetOpenFailedFormat, result);
        sceInetClose(netContext, netInetWorkArea, handle, LAN_CLOSE_TIMEOUT);
        return -1;
    }
    return handle;
}

/* Closes the handle with a one second timeout; prints the error when the close fails. */
void LanCloseHandle(int handle) {
    int status;

    status = sceInetClose(netContext, netInetWorkArea, handle, LAN_CLOSE_TIMEOUT);
    if (status < 0) {
        printf(lanCloseErrorFormat, status);
    }
}

/* Sends the CLOSE verb, then closes the handle. */
void LanClose(int handle) {
    int closeReply;

    LanReportStatus(lanCloseStatusText);
    closeReply = 0;
    sceInetSend(netContext, netInetWorkArea, handle, netCloseCommand, LAN_VERB_CLOSE_LEN, &closeReply, LAN_NO_TIMEOUT);
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
        received = sceInetRecv(netContext, netInetWorkArea, handle, buffer, chunkSize, &recvStatus, LAN_NO_TIMEOUT);
        bytesLeft -= received;
        if (buffer != 0) {
            buffer += received;
        }
        if (progressHook != 0) {
            xglFontDebugPrintf(LAN_STATUS_X, LAN_STATUS_Y, lanReceiveProgressFormat, buffer, bytesLeft, size, received);
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
        LanReportStatus(usbReadRequestStatusText);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, netSizeCommand, LAN_VERB_SIZE_LEN, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        LanReportStatus(usbReadSizeStatusText);
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
        LanReportStatus(usbReadRequestStatusText);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, netGetCommand, LAN_VERB_GET_LEN, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        LanReportStatus(usbReadSizeStatusText);
        LanReceive(handle, &fileSize, LAN_SIZE_FIELD_BYTES, 0);
        LanReceive(handle, buffer, fileSize, netStatusHandler);
        LanClose(handle);
    }
    return fileSize;
}

int sprintf(char *, const char *, int, int);
extern const char usbWriteProgressFormat[];
extern const char netSendCommand[];
extern const char netPutCommand[];
extern const char usbWriteRequestStatusText[];
extern const char usbWriteSizeStatusText[];

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
        LanReportStatus(usbWriteRequestStatusText);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, netPutCommand, LAN_VERB_PUT_LEN, &sendStatus, LAN_NO_TIMEOUT);
        cursor = buffer;
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        LanReportStatus(usbWriteSizeStatusText);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, &size, LAN_SIZE_FIELD_BYTES, &sendStatus, LAN_NO_TIMEOUT);
        bytesLeft = size;
        while (bytesLeft != 0) {
            chunkSize = (bytesLeft <= LAN_CHUNK_SIZE) ? bytesLeft : LAN_CHUNK_SIZE;
            sendStatus = 0;
            sentBytes = sceInetSend(netContext, netInetWorkArea, handle, cursor, chunkSize, &sendStatus, LAN_NO_TIMEOUT);
            bytesLeft -= sentBytes;
            cursor += sentBytes;
            sprintf(progressText, usbWriteProgressFormat, bytesLeft, size);
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
        sceInetSend(netContext, netInetWorkArea, handle, netSendCommand, LAN_VERB_SEND_LEN, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, &size, LAN_SIZE_FIELD_BYTES, &sendStatus, LAN_NO_TIMEOUT);
        bytesLeft = size;
        cursor = buffer;
        if (bytesLeft != 0) {
            do {
                chunkSize = (bytesLeft <= LAN_CHUNK_SIZE) ? bytesLeft : LAN_CHUNK_SIZE;
                sendStatus = 0;
                sentBytes = sceInetSend(netContext, netInetWorkArea, handle, cursor, chunkSize, &sendStatus, LAN_NO_TIMEOUT);
                bytesLeft -= sentBytes;
                cursor += sentBytes;
                sprintf(progressText, usbWriteProgressFormat, bytesLeft, size);
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
        sceInetSend(netContext, netInetWorkArea, handle, netRecvCommand, LAN_VERB_RECV_LEN, &sendStatus, LAN_NO_TIMEOUT);
        sendStatus = 0;
        sceInetSend(netContext, netInetWorkArea, handle, path, strlen(path) + 1, &sendStatus, LAN_NO_TIMEOUT);
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

/* Once the network stack is initialized: copies the local address text (netLocalAddressText) into name, stores the device (init state minus one) into device, and returns the word at offset 4 of the default block. */
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
    netHostPort.port = port;
    netHostConfigured = 1;
    return 0;
}

/* Returns the configured flag; when it is set, copies the host name and its port out. */
int UsbGetHost(char *name, int *port) {
    if (netHostConfigured != 0) {
        strcpy(name, netHostName);
        *port = netHostPort.port;
    } else {
        name[0] = '\0';
        *port = 0;
    }
    return netHostConfigured;
}

extern const char inetModuleName[];
extern const char netcnfModuleName[];
extern const char inetctlModuleName[];
extern const char usbdModuleName[];
extern const char an986ModuleName[];
extern const char smapModuleName[];
extern const char msifrpcModuleName[];
extern const char libnetModuleName[];
extern char netcnfModuleArgs[];
extern char inetctlUsbModuleArgs[];
extern char inetctlSmapModuleArgs[];
extern const char netRegHandlerFailedText[];
extern const char netWaitGetAddrFailedText[];
extern const char netInvalidAddrText[];
extern const char netLocalAddressFormat[];
void sceSifInitRpc(unsigned int mode);
void sceEENetInit(int);
int sceInetInit(void *, int, int, int);
int sceInetRegisterEventHandler(void *, int);
int sceInetWaitGetAddress(void *, int, void *, int, void *, int);
int sceInetAddress2String(void *, int, const char *, int, void *);

/* Stores the status handler, loads the network IRX modules and starts the stack once. */
int UsbInit(void (*statusHandler)(void), int device) {
    int linkReply[4];
    int netWorkBase;

    netStatusHandler = statusHandler;
    if (device < 0 || netInitState != 0) {
        return 0;
    }
    netInitState = 0;
    netWorkBase = ((int) WorkEnd + 64) & -64;
    netInetWorkArea = netWorkBase;
    WorkEnd = (u8 *) (netWorkBase + 0x8010);
    sceSifInitRpc(0);
    xglCdSifLoadModule(inetModuleName, 0);
    xglCdSifLoadModule(netcnfModuleName, netcnfModuleArgs);
    if (device == NET_DEVICE_USB) {
        xglCdSifLoadModule(inetctlModuleName, inetctlUsbModuleArgs);
        xglCdSifLoadModule(usbdModuleName, 0);
        xglCdSifLoadModule(an986ModuleName, 0);
    } else {
        xglCdSifLoadModule(inetctlModuleName, inetctlSmapModuleArgs);
        xglCdSifLoadModule(smapModuleName, 0);
    }
    xglCdSifLoadModule(msifrpcModuleName, 0);
    xglCdSifLoadModule(libnetModuleName, 0);
    sceEENetInit(0);
    sceInetInit(netContext, 16448, 8192, 32);
    if (sceInetRegisterEventHandler(netContext, netInetWorkArea) < 0) {
        printf(netRegHandlerFailedText);
    }
    memset(netLocalAddress, 0, 16);
    if (sceInetWaitGetAddress(netContext, netInetWorkArea, linkReply, 1, netLocalAddress, 2) < 0) {
        printf(netWaitGetAddrFailedText);
    }
    if (sceInetAddress2String(netContext, netInetWorkArea, netLocalAddressText, 16, netLocalAddress) != 0) {
        printf(netInvalidAddrText);
    }
    printf(netLocalAddressFormat, netLocalAddressText);
    netInitState = device + 1;
    return 0;
}

char netcnfModuleArgs[112] = "icon=host1:/home/xeno/vcd-f/conf/iconjp/SYS_NET.ICO\000iconsys=host1:/home/xeno/vcd-f/conf/iconjp/icon.sys";

char inetctlUsbModuleArgs[56] = "-no_decode\000mc0:/BISLPS-00000DebugSta/net000.cnf";

char inetctlSmapModuleArgs[64] = "-no_decode\000mc0:/BISLPS-00000DebugSta/net001.cnf";

const char lanOpenSearchStatusText[16] = "LanOpen:search";
const char lanName2AddressFailedText[32] = "sceInetName2Address() failed.\n";
const char lanOpenConnectStatusText[16] = "LanOpen:connect";
const char lanInetCreateResultFormat[32] = "sceInetCreate() returns %d\n";
const char lanOpenOpenStatusText[16] = "LanOpen:open";
const char lanInetOpenFailedFormat[32] = "sceInetOpen() failed. ret = %d\n";
const char lanCloseErrorFormat[16] = "close error:%d\n";
const char lanCloseStatusText[16] = "LanClose:close";
const char lanReceiveProgressFormat[24] = "UsbRead:%8x %8d/%8d %8d";
const char usbReadRequestStatusText[16] = "UsbRead:request";
const char usbReadSizeStatusText[24] = "UsbRead:read size";
const char usbWriteRequestStatusText[24] = "UsbWrite:request";
const char usbWriteSizeStatusText[24] = "UsbWrite:read size";
const char usbWriteProgressFormat[24] = "UsbWrite:%8d/%8d";
const char netRegHandlerFailedText[24] = "reg_handler() failed.\n";
const char netWaitGetAddrFailedText[32] = "wait_get_addr() failed.\n";
const char netInvalidAddrText[16] = "invalid addr\n";
const char netLocalAddressFormat[16] = "My address: %s\n";

int netInitState = 0;
int netHostConfigured = 0;
const char netCloseCommand[8] = "CLOSE";
const char netSizeCommand[8] = "SIZE ";
const char netGetCommand[8] = "GET ";
const char netPutCommand[8] = "PUT ";
const char netSendCommand[8] = "SEND ";
const char netRecvCommand[8] = "RECV ";
const char inetModuleName[8] = "inet";
const char netcnfModuleName[8] = "netcnf";
const char inetctlModuleName[8] = "inetctl";
const char usbdModuleName[8] = "usbd";
const char an986ModuleName[8] = "an986";
const char smapModuleName[8] = "smap";
const char msifrpcModuleName[8] = "msifrpc";
const char libnetModuleName[8] = "libnet";
