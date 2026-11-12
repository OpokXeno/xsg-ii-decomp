#include "common.h"
#include "shared.h"

/* D_ENABLEW bit 16 (CPND): holds all DMA transfers while a channel register is written. */
#define D_ENABLE_HOLD 0x10000
#define D_ENABLEW ((volatile u32 *) 0x1000f590)
/* DMAC channel 4 feeds the IPU (memory to IPU input); its control register takes the transfer mode and start bit. */
#define D4_CHCR ((volatile u32 *) 0x1000b400)

extern int DIntr(void);
extern int EIntr(void);

/* Starts or stops the IPU input transfer by writing D4_CHCR with the DMA interrupts held off.
 * Static in this file, as in XS1's xgl_1.c: checkdma's delay-slot filling depends on the
 * definition being in the same translation unit. */
static void setD4_CHCR(u32 value)
{
    DIntr();
    *D_ENABLEW = *D_ENABLEW | D_ENABLE_HOLD;
    *D4_CHCR = value;
    *D_ENABLEW = *D_ENABLEW & ~D_ENABLE_HOLD;
    EIntr();
}
/* DMA tag ID (bits 28..30 of the first tag word). */
#define DMA_TAG_ID_CNT 0x10000000U
#define DMA_TAG_ID_REF 0x30000000U
/* Clears the tag ID and IRQ bits, turning a REF tag into REFE. */
#define DMA_TAG_ID_CLEAR_MASK 0x0fffffff
/* Dn_CHCR of the D4 (IPU_to) channel: chain mode, from memory, started. */
#define DMA_CHCR_DIR 0x1
#define DMA_CHCR_MOD_CHAIN 0x4
#define DMA_CHCR_STR 0x100
#define MOVIE_D4_CHCR_CHAIN_START (DMA_CHCR_STR | DMA_CHCR_MOD_CHAIN | DMA_CHCR_DIR)
/* GIF tag fields. */
#define GIF_TAG_EOP 0x8000
#define GIF_TAG_NREG_1 0x1000000000000000
#define GIF_TAG_FLG_IMAGE 0x0800000000000000
#define GIF_REG_AD 14
#define SCE_GS_BITBLTBUF 0x50
#define SCE_GS_TRXPOS 0x51
#define SCE_GS_TRXREG 0x52
#define SCE_GS_TRXDIR 0x53
/* transMode values: which GS buffer a decoded strip is uploaded to, or memory only. */
#define MOVIE_TRANS_BUFFER_0 0
#define MOVIE_TRANS_BUFFER_1 1
#define MOVIE_TRANS_BITBLTBUF 2
/* A strip is one 16-pixel-high row of macroblocks, each macroblock 16x16 RGBA32 pixels. */
#define MOVIE_MACROBLOCK 16
#define MOVIE_MACROBLOCK_BYTES 0x400
#define MOVIE_MACROBLOCK_QWORDS 0x40
/* GIF packets of the strip upload: A+D settings, then one IMAGE tag per macroblock. */
#define MOVIE_GIF_TAG_AD_2 (GIF_TAG_NREG_1 | GIF_TAG_EOP | 2)
#define MOVIE_GIF_TAG_IMAGE_BLOCK (GIF_TAG_FLG_IMAGE | GIF_TAG_EOP | MOVIE_MACROBLOCK_QWORDS)
#define MOVIE_BITBLTBUF_DBW_512 0x8000000000000
#define MOVIE_TRXREG_MACROBLOCK 0x1000000010

typedef struct XglDmaRefTag {
    unsigned int tag;
    unsigned int address;
    unsigned long upperQword;
} XglDmaRefTag;

typedef struct XglMovieSourceHeader XglMovieSourceHeader;

typedef struct XglMovieWork {
    unsigned char unmodeled_00[4];
    /* checkdma reads this byte; xglMovieOpen clears it with the word at +0x04. */
    unsigned char streamState;      /* +0x04 */
    unsigned char unmodeled_05[0x0f];
    int sourceLength;               /* +0x14 */
    int streamingMode;              /* +0x18 */
    unsigned char unmodeled_1c[4];
    unsigned char *ringBuffer;      /* +0x20 */
    int ringCapacity;               /* +0x24 */
    int ringWrite;                  /* +0x28 */
    int ringRead;                   /* +0x2c */
    unsigned char unmodeled_30[0x10];
    short width;                    /* +0x40 */
    short height;                   /* +0x42 */
    short frameCount;               /* +0x44 */
    short currentFrame;             /* +0x46 */
    int stripQwords;                /* +0x48 */
    unsigned int imageQwords;       /* +0x4c */
    XglDmaRefTag chain[4];          /* +0x50 */
    XglMovieSourceHeader *source;   /* +0x90 */
    int sourceSize;                 /* +0x94 */
    unsigned char decodeState;      /* +0x98 */
    unsigned char streamOpen;       /* +0x99 */
    unsigned char loop;             /* +0x9a */
    unsigned char unmodeled_9b[4];
    unsigned char bitPosition;      /* +0x9f */
    unsigned int dmaControl;        /* +0xa0 */
    unsigned int dmaAddress;        /* +0xa4 */
    unsigned int dmaQwords;         /* +0xa8 */
    XglDmaRefTag *dmaTag;           /* +0xac */
    unsigned int imageAddress[2];   /* +0xb0 */
    unsigned int packetAddress[2];  /* +0xb8 */
    unsigned long long bitbltbuf;   /* +0xc0 */
    unsigned char bufferIndex;      /* +0xc8 */
    unsigned char transMode;        /* +0xc9 */
    unsigned char threshold1;       /* +0xca */
    unsigned char threshold0;       /* +0xcb */
    /* Memory-mode decode buffer; 0 uses the scratchpad. */
    unsigned int workBuffer;        /* +0xcc */
} XglMovieWork;

struct XglMovieSourceHeader {
    unsigned int signature;             /* "ipum" */
    unsigned char unmodeled_04[4];
    unsigned short width;               /* +0x08 */
    unsigned short height;              /* +0x0a */
    unsigned short frameCount;          /* +0x0c */
    unsigned char unmodeled_0e[2];
};

typedef struct XglMovieRenderState {
    unsigned char unmodeled_00[0x14];
    unsigned short bufferBase0;         /* +0x14 */
    unsigned char unmodeled_16[4];
    unsigned short bufferBase1;         /* +0x1a */
    unsigned char unmodeled_1c[0x40];
} XglMovieRenderState;

extern XglMovieRenderState sRender;

/*
 * Builds the GS upload packet for one 16-line strip at row y: an A+D setting block, then one
 * image block per macroblock with its TRXPOS and TRXDIR, closed by a REF tag. Returns the next free packet.
 */
unsigned long *make_gstrans(XglMovieWork *info, int y)
{
    int width = info->width;
    unsigned long *packet = (unsigned long *) info->packetAddress[info->bufferIndex];
    unsigned int image = info->imageAddress[info->bufferIndex];
    int x;

    packet[0] = DMA_TAG_ID_CNT | 3;
    packet[2] = MOVIE_GIF_TAG_AD_2;
    packet[3] = GIF_REG_AD;
    packet[1] = 0;
    switch (info->transMode) {
    case MOVIE_TRANS_BUFFER_0:
    default:
        packet[4] = ((unsigned long) sRender.bufferBase0 << 37) | MOVIE_BITBLTBUF_DBW_512;
        break;
    case MOVIE_TRANS_BUFFER_1:
        packet[4] = ((unsigned long) sRender.bufferBase1 << 37) | MOVIE_BITBLTBUF_DBW_512;
        break;
    case MOVIE_TRANS_BITBLTBUF:
        packet[4] = info->bitbltbuf;
        break;
    }
    packet[5] = SCE_GS_BITBLTBUF;
    packet[6] = MOVIE_TRXREG_MACROBLOCK;
    packet[7] = SCE_GS_TRXREG;
    packet += 8;
    for (x = 0; x < width; x += MOVIE_MACROBLOCK) {
        packet[1] = 0;
        packet[4] = ((unsigned long) x << 32) | ((unsigned long) y << 48);
        packet[0] = DMA_TAG_ID_CNT | 4;
        packet[2] = MOVIE_GIF_TAG_AD_2;
        packet[3] = GIF_REG_AD;
        packet[5] = SCE_GS_TRXPOS;
        packet[6] = 0;
        packet[7] = SCE_GS_TRXDIR;
        packet[8] = MOVIE_GIF_TAG_IMAGE_BLOCK;
        packet[9] = 0;
        packet += 10;
        ((XglDmaRefTag *) packet)->address = image;
        image += MOVIE_MACROBLOCK_BYTES;
        ((XglDmaRefTag *) packet)->tag = DMA_TAG_ID_REF | MOVIE_MACROBLOCK_QWORDS;
        ((XglDmaRefTag *) packet)->upperQword = 0;
        packet += 2;
    }
    ((XglDmaRefTag *) packet)[-1].tag &= DMA_TAG_ID_CLEAR_MASK;
    return packet;
}

#define MOVIE_D4_CHCR_CHAIN_STOP (DMA_CHCR_MOD_CHAIN | DMA_CHCR_DIR)
#define MOVIE_D4_CHCR_CHAIN_START_REF (DMA_TAG_ID_REF | MOVIE_D4_CHCR_CHAIN_START)
#define D4_MADR ((volatile u32 *) 0x1000b410)
#define D4_TADR ((volatile u32 *) 0x1000b430)
/* XglMovieWork.decodeState once the whole source has been sent. */
#define MOVIE_DECODE_DONE 99

int checkdma(XglMovieWork *info)
{
    int wait = 0;
    unsigned int consumed;

    if (info->decodeState == MOVIE_DECODE_DONE) {
        return 0;
    }
    setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
    consumed = *D4_MADR - (unsigned int)info->source;
    switch (info->decodeState) {
    case 0:
        if ((unsigned int)info->sourceSize < consumed) {
            if (info->ringRead != info->ringWrite &&
                (info->ringWrite - consumed) <
                (unsigned int)(info->sourceSize / 2) && !info->streamState) {
                wait = 1;
                break;
            }
            info->ringRead = info->sourceSize;
            if (!info->streamingMode) {
                *D4_TADR = (unsigned int)&info->chain[3];
                info->decodeState = 3;
            } else {
                *D4_TADR = (unsigned int)&info->chain[1];
                info->decodeState = 1;
            }
        } else {
            *D4_TADR = (unsigned int)&info->chain[2];
        }
        break;
    case 1:
        if (consumed < (unsigned int)info->sourceSize) {
            if (info->ringRead != info->ringWrite &&
                (info->ringWrite - consumed) <
                (unsigned int)(info->sourceSize / 2) && !info->streamState) {
                wait = 1;
                break;
            }
            info->ringRead = 0;
            if (!info->streamingMode) {
                *D4_TADR = (unsigned int)&info->chain[3];
                info->decodeState = 2;
            } else {
                *D4_TADR = (unsigned int)&info->chain[2];
                info->decodeState = 0;
            }
        } else {
            *D4_TADR = (unsigned int)&info->chain[1];
        }
        break;
    case 2:
        if ((unsigned int)info->sourceSize < consumed) {
            info->decodeState = MOVIE_DECODE_DONE;
        }
        break;
    case 3:
        if (consumed < (unsigned int)info->sourceSize) {
            info->decodeState = MOVIE_DECODE_DONE;
        }
        break;
    }
    if (info->decodeState != MOVIE_DECODE_DONE) {
        setD4_CHCR(MOVIE_D4_CHCR_CHAIN_START_REF);
    } else {
        setD4_CHCR(MOVIE_D4_CHCR_CHAIN_START);
    }
    return wait;
}

/* The movie work area starts with its CD stream record (0x40 bytes, the
 * width follows at +0x40); xglMovieInfoInit and xglMovieOpen pass it to the
 * xgl_cd stream functions. */
struct CdStreamParam;
void xglCdStreamParamInit(struct CdStreamParam *stream);

/* xglMovieOpen: the largest source the ring takes, and the size it is cut to. */
#define MOVIE_SOURCE_SIZE_LIMIT 0xfffff
#define MOVIE_RING_HALF_BYTES 0xf0000
#define MOVIE_DMA_QWORD 16
/* "ipum" */
#define MOVIE_SIGNATURE_IPUM 0x6d757069

int printf(const char *format, ...);
int xglCdStreamOpen(struct CdStreamParam *stream, int file);
int xglCdStreamReadRing(struct CdStreamParam *stream, int bytes);
void SyncDCache(void *start, void *end);

/*
 * Opens a movie: from a stream (file) through a ring of two source-sized
 * halves fed by the four-tag DMA chain, or from memory as one transfer.
 * Then checks the "ipum" header and takes the size and frame count.
 */
int xglMovieOpen(XglMovieWork *info, int stream)
{
    XglMovieSourceHeader *source;
    XglDmaRefTag *chain;

    if (stream) {
        info->streamOpen = 1;
        if (info->sourceSize > MOVIE_SOURCE_SIZE_LIMIT) {
            printf("ring buffer size too large.\n");
            info->sourceSize = MOVIE_RING_HALF_BYTES;
        }
        info->ringCapacity = info->sourceSize * 2;
        info->ringBuffer = (unsigned char *) info->source;
        xglCdStreamOpen((struct CdStreamParam *) info, stream);
        chain = info->chain;
        chain[0].tag = info->sourceSize / MOVIE_DMA_QWORD - 1;
        chain[0].address = (unsigned int) (info->source + 1);
        chain[0].upperQword = 0;
        chain[1].tag = info->sourceSize / MOVIE_DMA_QWORD;
        chain[1].address = (unsigned int) info->source;
        chain[1].upperQword = 0;
        chain[2].tag = info->sourceSize / MOVIE_DMA_QWORD;
        chain[2].address = (unsigned int) info->source + info->sourceSize;
        chain[2].upperQword = 0;
        chain[3].tag = (info->sourceLength % info->sourceSize + 15) / MOVIE_DMA_QWORD;
        chain[3].address = (unsigned int) info->source
                                 + info->sourceSize * (info->sourceLength / info->sourceSize & 1);
        chain[3].upperQword = 0;
        info->ringRead = 0;
        info->ringWrite = 0;
        xglCdStreamReadRing((struct CdStreamParam *) info, info->sourceSize);
        info->ringRead = 0;
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaTag = chain;
        info->decodeState = 0;
        info->dmaAddress = 0;
        info->dmaQwords = 0;
        info->bitPosition = 0;
    } else {
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaAddress = (unsigned int) (info->source + 1);
        info->dmaQwords = (info->sourceSize - 1) / MOVIE_DMA_QWORD;
        info->streamOpen = 0;
        info->dmaTag = 0;
        info->bitPosition = 0;
    }
    source = info->source;
    if (source->signature != MOVIE_SIGNATURE_IPUM) {
        xglMovieClose(info);
        return 1;
    }
    info->width = source->width;
    info->stripQwords = (info->width >> 4) << 6;
    info->height = source->height;
    info->imageQwords = (info->height >> 4) * info->stripQwords;
    info->frameCount = source->frameCount;
    info->currentFrame = 0;
    info->bufferIndex = 0;
    SyncDCache(info, info + 1);
    return 0;
}

#define D3_CHCR ((volatile u32 *) 0x1000b000)
/* D4_CHCR value that leaves the chain set up but stopped. */
#define MOVIE_D4_CHCR_CHAIN_STOP (DMA_CHCR_MOD_CHAIN | DMA_CHCR_DIR)

void xglCdStreamRewind(struct CdStreamParam *stream);

/* EE IPU and IPU DMA registers. */
#define IPU_CMD ((volatile u32 *) 0x10002000)
#define IPU_CTRL ((volatile u32 *) 0x10002010)
#define IPU_BP ((volatile u32 *) 0x10002020)
#define D4_MADR ((volatile u32 *) 0x1000b410)
#define D4_QWC ((volatile u32 *) 0x1000b420)
#define D4_TADR ((volatile u32 *) 0x1000b430)
#define IPU_CTRL_RESET 0x40000000
#define IPU_CMD_BCLR 0
#define IPU_CMD_IDEC 0x10000000
#define IPU_CMD_FDEC 0x40000000
#define IPU_CMD_SETTH 0x90000000
#define IPU_IDEC_QSC_1 0x10000 /* quantiser step code 1 */
/* sceIpuSync timeout, and the busy-poll bound of the D3 wait. */
#define IPU_SYNC_TIMEOUT 0x200
#define MOVIE_POLL_LIMIT 0xFFFFFF
/* transMode: decode into memory only (the strips are copied out of the scratchpad). */
#define MOVIE_TRANS_MEMORY 0xff
#define MOVIE_DMA_MAX_QWORDS 0xffff
#define SCRATCHPAD_BASE 0x70000000
#define XGL_DMA_CHANNEL_GIF 2

typedef unsigned int Quadword __attribute__((mode(TI)));

int sceIpuSync(int mode, int timeout);
int sceGsSyncPath(int mode, int timeout);
int myDmaDirectFromIPU(u32 address, u32 count);
void xglDmaDirectSrcChain(u32 channel, u32 address);
int checkdma(XglMovieWork *info);
int xglMovieRewind(XglMovieWork *info);
/* "frame drop\n" and "ipu time out:%d\n" */
extern const char frameDropMessage[];
extern const char ipuTimeoutMessage[];

/*
 * Decodes one movie frame: restarts the IPU feed when its DMA has stopped, checks the stream ring,
 * decodes the strips into the GS (through make_gstrans packets) or into memory, and advances the
 * frame. At the end it stops (loop off) or rewinds; otherwise it saves the IPU bitstream position
 * for the next frame. Returns -1 on an IPU or ring error, 1 at the end, 0 otherwise.
 */
int xglMoviePlay(XglMovieWork *info)
{
    int strip;
    int retry;
    unsigned int header;
    int pitch;
    int x;
    int row;
    Quadword *source;
    Quadword *destination;
    unsigned int work;

    if (info->loop == 0 && info->currentFrame >= info->frameCount) {
        return 1;
    }
    if ((*D4_CHCR & DMA_CHCR_STR) == 0) {
        *IPU_CTRL = IPU_CTRL_RESET;
        if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
            return -1;
        }
        *IPU_CMD = IPU_CMD_SETTH | (info->threshold1 << 16) | info->threshold0;
        if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
            return -1;
        }
        *IPU_CMD = info->bitPosition;
        if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
            return -1;
        }
        *D4_MADR = info->dmaAddress;
        if (info->dmaQwords > MOVIE_DMA_MAX_QWORDS) {
            *D4_QWC = MOVIE_DMA_MAX_QWORDS;
        } else {
            *D4_QWC = info->dmaQwords;
        }
        *D4_TADR = (unsigned int) info->dmaTag;
        setD4_CHCR(info->dmaControl);
    }
    if (info->streamOpen) {
        if (checkdma(info) != 0) {
            printf(frameDropMessage);
            return -1;
        }
    }
    if (info->transMode != MOVIE_TRANS_MEMORY) {
        sceGsSyncPath(0, 0);
        for (strip = 0; strip < info->height; strip += MOVIE_MACROBLOCK) {
            myDmaDirectFromIPU(info->imageAddress[info->bufferIndex], info->stripQwords);
            if (strip == 0) {
                *IPU_CMD = IPU_CMD_FDEC;
                if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
                    return -1;
                }
                header = *IPU_CMD >> 24;
                *IPU_CMD = IPU_CMD_FDEC | 8;
                if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
                    return -1;
                }
                *IPU_CTRL = (header & 0xfb) << 16;
                *IPU_CMD = (((header & 4) >> 2) << 24) | (IPU_CMD_IDEC | IPU_IDEC_QSC_1);
            }
            make_gstrans(info, strip);
            for (retry = 0; retry <= MOVIE_POLL_LIMIT; retry++) {
                if ((*D3_CHCR & DMA_CHCR_STR) == 0) {
                    break;
                }
            }
            xglDmaDirectSrcChain(XGL_DMA_CHANNEL_GIF, info->packetAddress[info->bufferIndex]);
            info->bufferIndex ^= 1;
        }
        sceGsSyncPath(0, 0);
    } else {
        work = SCRATCHPAD_BASE;
        if (info->workBuffer != 0) {
            work = info->workBuffer;
        }
        for (strip = 0; strip < info->height; strip += MOVIE_MACROBLOCK) {
            myDmaDirectFromIPU(work, info->stripQwords);
            if (strip == 0) {
                *IPU_CMD = IPU_CMD_FDEC;
                if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
                    return -1;
                }
                header = *IPU_CMD >> 24;
                *IPU_CMD = IPU_CMD_FDEC | 8;
                if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
                    return -1;
                }
                *IPU_CTRL = (header & 0xfb) << 16;
                *IPU_CMD = (((header & 4) >> 2) << 24) | (IPU_CMD_IDEC | IPU_IDEC_QSC_1);
            }
            for (retry = 0; retry <= MOVIE_POLL_LIMIT; retry++) {
                if ((*D3_CHCR & DMA_CHCR_STR) == 0) {
                    break;
                }
            }
            if (retry > MOVIE_POLL_LIMIT) {
                printf(ipuTimeoutMessage, strip);
            }
            pitch = info->width / 4;
            source = (Quadword *) work;
            destination = (Quadword *) (info->imageAddress[info->bufferIndex] + info->width * strip * 4);
            for (x = 0; x < info->width; x += MOVIE_MACROBLOCK) {
                for (row = MOVIE_MACROBLOCK - 1; row >= 0; row--) {
                    destination[0] = source[0];
                    destination[1] = source[1];
                    destination[2] = source[2];
                    destination[3] = source[3];
                    source += 4;
                    destination += pitch;
                }
                destination = destination - pitch * MOVIE_MACROBLOCK + 4;
            }
        }
        info->bufferIndex ^= 1;
    }
    if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
        return -1;
    }
    *IPU_CMD = IPU_CMD_FDEC | 0x20;
    if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
        return -1;
    }
    info->currentFrame++;
    if (info->currentFrame >= info->frameCount) {
        if (info->loop == 0) {
            info->currentFrame = info->frameCount;
            setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
            DIntr();
            *D_ENABLEW = *D_ENABLEW | D_ENABLE_HOLD;
            *D3_CHCR = 0;
            *D_ENABLEW = *D_ENABLEW & ~D_ENABLE_HOLD;
            EIntr();
        } else {
            xglMovieRewind(info);
        }
        return 1;
    }
    if (info->streamOpen) {
        unsigned int bitstream;

        info->dmaControl = *D4_CHCR;
        setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
        bitstream = *IPU_BP;
        info->dmaAddress = *D4_MADR - (((bitstream >> 16) & 3) + ((bitstream >> 8) & 0xf)) * 16;
        info->dmaQwords = *D4_QWC + (((bitstream >> 16) & 3) + ((bitstream >> 8) & 0xf));
        info->dmaTag = (XglDmaRefTag *) *D4_TADR;
        info->bitPosition = bitstream & 0x7f;
    } else {
        unsigned int bitstream;
        unsigned int consumed;

        info->dmaControl = *D4_CHCR;
        setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
        bitstream = *IPU_BP;
        consumed = *D4_MADR - info->dmaAddress - (((bitstream >> 16) & 3) + ((bitstream >> 8) & 0xf)) * 16;
        info->dmaAddress += consumed;
        info->dmaQwords -= consumed / MOVIE_DMA_QWORD;
        info->dmaTag = (XglDmaRefTag *) *D4_TADR;
        info->bitPosition = bitstream & 0x7f;
    }
    return 0;
}



/*
 * Restarts a movie at its first frame: stops both IPU DMA channels, seeks a stream back to its first
 * sector and refills the first ring half, and sets up the DMA start as xglMovieOpen does.
 */
int xglMovieRewind(XglMovieWork *info)
{
    info->currentFrame = 0;
    setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
    DIntr();
    *D_ENABLEW = *D_ENABLEW | D_ENABLE_HOLD;
    *D3_CHCR = 0;
    *D_ENABLEW = *D_ENABLEW & ~D_ENABLE_HOLD;
    EIntr();
    if (info->streamOpen) {
        xglCdStreamRewind((struct CdStreamParam *) info);
        info->ringWrite = 0;
        info->ringRead = 0;
        xglCdStreamReadRing((struct CdStreamParam *) info, info->sourceSize);
        info->ringRead = 0;
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaTag = info->chain;
        info->decodeState = 0;
        info->dmaAddress = 0;
        info->dmaQwords = 0;
        info->bitPosition = 0;
    } else {
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaAddress = (unsigned int) (info->source + 1);
        info->dmaQwords = (info->sourceSize - 1) / MOVIE_DMA_QWORD;
        info->dmaTag = 0;
        info->bitPosition = 0;
    }
    SyncDCache(info, info + 1);
    return 0;
}

typedef signed char s8;

typedef int s32;

int xglCdStreamClose(struct CdStreamParam *stream);

/*
 * Closes the movie stream of info: closes its CD stream while streamOpen is set,
 * clears the D4 channel, writes IPU_CTRL, and returns -1
 * when sceIpuSync reports a failure before or after the write to IPU_CMD.
 */

int xglMovieClose(XglMovieWork *info) {
    if (info->streamOpen != 0) {
        xglCdStreamClose((struct CdStreamParam *) info);
    }
    setD4_CHCR(0);
    *IPU_CTRL = IPU_CTRL_RESET;
    if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
        return -1;
    }
    *IPU_CMD = IPU_CMD_BCLR;
    return (sceIpuSync(0, IPU_SYNC_TIMEOUT) >= 0) ? 0 : -1;
}


/* The two packet buffers in scratchpad RAM. */
#define MOVIE_PACKET_ADDRESS_0 0x70000040
#define MOVIE_PACKET_ADDRESS_1 0x70001000

void xglMovieInfoInit(XglMovieWork *info)
{
    xglCdStreamParamInit((struct CdStreamParam *) info);
    info->packetAddress[0] = MOVIE_PACKET_ADDRESS_0;
    info->packetAddress[1] = MOVIE_PACKET_ADDRESS_1;
    info->transMode = info->currentFrame = -1;
    info->loop = 0;
    info->threshold1 = 0;
    info->threshold0 = 0;
    info->workBuffer = 0;
}
