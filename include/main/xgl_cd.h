#ifndef INCLUDE_MAIN_XGL_CD_H
#define INCLUDE_MAIN_XGL_CD_H

/* Functions of src/main/xgl_cd.c that other files call. */

/* xglCdGetFilePos and xglCdCheckFile fill a 0x30-byte file-position record;
 * only its leading lbn word is evidenced by the callers in xgl_cd.c. */
typedef struct CdFilePosition {
    unsigned int lbn;
    int stored_bytes;
    unsigned char unmodeled_08[0x1c];
    unsigned char source;
    unsigned char unmodeled_25;
    unsigned char packed;
    unsigned char unmodeled_27;
    int descriptor;
    int size_bytes;
} CdFilePosition;

/* Where a file is read from (CdFilePosition.source, CdStreamParam.source). */
#define CD_SOURCE_DISC 0
#define CD_SOURCE_HOST 1
#define CD_SOURCE_HDD 2

typedef struct StreamXssBuffer StreamXssBuffer;

typedef struct StreamRing StreamRing;

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
    /* The current byte of the interleave map and how many of its bits are left. */
    unsigned char map_byte;
    unsigned char bits_left;
    short sectors;
};

/* A stream record; xgl_1_016.c reads movies through one. */
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

/* Probes path and fills a CdFilePosition at record (lsn at +0, size at +4,
 * see tu022.c); -1 when the file is missing, 0 otherwise. The callers pass
 * work buffers they reuse for the record. */
int xglCdCheckFile(const char *path, void *record);
void xglCdArcCheck(void);
/* A callback gets a third argument: the current read tag from xglCdGetFilePos (lw $6 at
 * 0x19BA04), the request's argument from the read service. */
typedef void CdCompletionCallback(int event, int value, int tag);

/* xglCdReadFilePart's callback argument may instead select one: the current global callback,
 * the dummy or the default one (xglCdReadFile passes its flags argument through as it). */
#define CD_NOTIFY_CURRENT 0
#define CD_NOTIFY_DUMMY 1
#define CD_NOTIFY_DEFAULT 2
/* xglCdReadFilePart mode bits: queue or start the read and return at once; leave the data packed
 * where it was read; read even while another read is active (only as the whole mode). */
#define CD_READ_SYNC 0
#define CD_READ_ASYNC 1
#define CD_READ_NO_EXTRACT 2
#define CD_READ_WHILE_BUSY 4

/* Overlay numbers for xglCdLoadOverlay. */
#define OVERLAY_TITLE 2

int xglCdReadFilePart(const char *name, void *buffer, int mode,
                      CdCompletionCallback *notify, int offset, int length);
int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
void xglCdSifLoadModule(const char *path, const char *args);
void xglCdInitial(void);
void xglCdOpenDevice(void);
void xglCdControlThread(void);
void xglCdLoadOverlay(int overlay);
int xglCdServiceIfPending(void);
void xglCdSetReadTag(int value);
int xglCdGetFileSize(const char *path);
void xglCdReadCancel(int immediate);
int xglCdStreamOpen(CdStreamParam *stream, const char *path);
int xglCdStreamClose(CdStreamParam *stream);
int xglCdStreamRead(CdStreamParam *stream, unsigned char *buffer, int byte_count);
int xglCdStreamReadRing(CdStreamParam *stream, int bytes);
int xglCdStreamSeek(CdStreamParam *stream, int sector);
void xglCdStreamRewind(CdStreamParam *stream);
int xglCdStreamStat(CdStreamParam *stream);
void xglCdStreamParamInit(CdStreamParam *stream);

#endif /* INCLUDE_MAIN_XGL_CD_H */
